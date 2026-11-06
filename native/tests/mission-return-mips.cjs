'use strict';
// Read-only instruction check against the original US Rev 0 ROM. Execute
// the settlement tail and battle dispatcher case bodies with external
// services trapped. The real event scheduler runs a synthetic group 5
// handler containing the authored return sequence.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const {FloatMips} = require('./float-mips.cjs');

const filename = process.argv[2] || process.env.LORDLY_TEST_ROM;
if (!filename) {
    console.log('SKIP: supply the private Rev 0 ROM');
    process.exit(77);
}
const rom = fs.readFileSync(filename);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
assert.equal(crypto.createHash('sha256').update(rom).digest('hex'),
             '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');

const BATTLE_DELTA = 0x800ab8c0;
const CASE_TABLE = 0x142380;
const battleCase = state => rom.readUInt32BE(CASE_TABLE + state * 4);
const BATTLE_CASE_END = 0x801b0208;
const BATTLE_STATE = 0x801f0de0;
const BATTLE_REQUEST = 0x801e7d62;
const DEATH_ACTIVE = 0x8021c74d;
const PROGRAM = 0x80700000;
const SP = 0x807ff000;
const GROUP_FIVE = Buffer.from([
    '000c000c000c000c000c000e', // Six trigger-table offsets
    'ffff0012ffff',             // Empty groups, then group 5 at +0x12
    '6806690171b0',             // Location 6 changes to cleared state
    '69008150',                 // Clear its mission marker
    '688670f92900',             // Return to physical location 6 and snapshot it
    '680070e8',                 // Clear the completed event row
    '68031100',                 // Request the world map
].join(''), 'hex');

function checkReturn(aliasSource, aliasTarget, snapshot) {
    const m = new FloatMips();
    m.load(0x80070c00, rom.subarray(0x1000, 0x2f000)); // Resident boot services
    m.load(0x80163910, rom.subarray(0x39810, 0x66e10)); // Resident event VM and data
    m.load(0x103da0 + BATTLE_DELTA, rom.subarray(0x103da0, 0x104860));
    m.load(0x139ab4 + BATTLE_DELTA, rom.subarray(0x139ab4, 0x139c10));
    m.set('sp', SP);
    const calls = [];
    const trap = (address, name, result) => m.trap(address, () => {
        calls.push(name);
        if (result !== undefined) m.set('v0', result);
    });
    trap(0x801dc720, 'settlement service', 0);
    trap(0x802156d4, 'death processing');
    m.trap(0x801ab0fc, () => {
        assert.equal(m.get('a0'), 7);
        assert.equal(m.get('a1'), 15);
        calls.push('death result index');
        m.set('v0', 1);
    });
    trap(0x80186190, 'prepare transition');
    trap(0x801c9aa4, 'battle cleanup');
    trap(0x801dd244, 'special cleanup check', 0);
    trap(0x801d5a60, 'finalize battle');
    trap(0x8016c5bc, 'finalize army');
    m.trap(0x800712c4, () => {}); // Ignore freeing the test program buffer.

    const exec = m.exec.bind(m);
    m.exec = (pc, next) => {
        if (pc === 0x8017223c) {
            assert.equal(m.get('a0'), 0x20, 'dispatch mask selects mission completion');
            calls.push('dispatch group 5');
        } else if (pc === 0x80170954) {
            assert.equal(m.get('a0'), PROGRAM + 0x12);
            assert.equal(m.r8(0x80197b00), 5);
            calls.push('execute completion handler');
        }
        return exec(pc, next);
    };

    m.load(PROGRAM, GROUP_FIVE);
    m.w32(0x80197b1c, PROGRAM);
    m.w16(0x80197b06, 2);         // Active mission scheduler
    m.w8(0x801936a8, 2);          // Event row to clear on completion
    m.w8(0x801936aa, 15);         // Deliberately different prior snapshot
    m.w8(0x80196aea, aliasSource);
    m.w8(0x80196aeb, aliasTarget);
    m.w8(0x80196a87, 0xaa);       // Node 6 and all adjacent states start at 2
    m.w8(0x80196a93, 0xe5);       // Node 6 and several unrelated markers

    m.w32(0x800eb0dc, 11);        // Skip unrelated presentation request
    m.w8(0x8018f481, 1);          // An ordinary mission eligible for rewards
    m.w32(0x80196a6c, 1000);
    m.w32(0x8021c744, 123);       // Previously calculated Goth reward
    m.call(0x139ab4 + BATTLE_DELTA, 0x139bf0 + BATTLE_DELTA);
    assert.equal(m.r32(0x80196a6c), 1123);
    assert.equal(m.r32(BATTLE_STATE), 0x1b);
    assert.deepEqual(calls, ['settlement service']);

    m.w32(0x801ab850, 0x80730000);
    m.w16(0x80730000, 7);
    m.w8(DEATH_ACTIVE, 1);
    m.call(battleCase(0x1b), BATTLE_CASE_END);
    assert.equal(m.r32(BATTLE_STATE), 0x1b);
    assert.equal(m.r16(BATTLE_REQUEST), 0);
    assert.deepEqual(calls, ['settlement service', 'death processing']);

    m.w8(DEATH_ACTIVE, 0);
    m.call(battleCase(0x1b), BATTLE_CASE_END);
    assert.equal(m.r32(BATTLE_STATE), 0x63);
    assert.equal(m.r16(BATTLE_REQUEST), 0x301);
    assert.deepEqual(calls, ['settlement service', 'death processing',
                            'death processing', 'death result index']);
    assert.equal(m.r16(0x8018f1a2), 0, 'world request is still unset');
    assert.equal(m.get('sp'), SP);

    const resultReady = 0x800e8350 + 168 + 15; // Readiness byte of result record 1
    m.call(battleCase(0x63), BATTLE_CASE_END);
    assert.equal(m.r32(BATTLE_STATE), 0x63);
    assert.equal(calls.length, 4, 'the scheduler waits for a ready result');
    assert.equal(m.r8(0x801936a8), 2);

    m.w8(resultReady, 1);
    // The original scheduler has executed group 5 and released its script by
    // the time it reaches the event-resource reload boundary.
    m.call(battleCase(0x63), 0x80172394);
    assert.deepEqual(calls.slice(4), ['prepare transition', 'battle cleanup',
                     'special cleanup check', 'finalize battle', 'finalize army',
                     'dispatch group 5', 'execute completion handler']);
    assert.equal(m.r8(0x801936a8), 0);
    assert.equal(m.r8(0x80196a9a), 0x86, 'live return targets physical node 6');
    assert.equal(m.r8(0x801936aa), snapshot, 'opcode 29 applies the retail snapshot alias');
    assert.equal(m.r8(0x80196aea), aliasSource);
    assert.equal(m.r8(0x80196aeb), aliasTarget);
    assert.equal(m.r8(0x80196a87), 0xa6, 'clearing node 6 preserves adjacent states');
    assert.equal(m.r8(0x80196a93), 0xc5, 'clearing node 6 preserves unrelated markers');
    assert.equal(m.r16(0x8018f1a2), 0x8005, 'request 3 selects the world-map mode');
}

checkReturn(0, 0, 6);
checkReturn(6, 37, 37);
checkReturn(7, 38, 6);
console.log('PASS: settlement tail, pending and finished death dispatch, and real group 5 scheduling across three location-alias cases.');
console.log('External services and presentation are stubbed. The fresh export and full world-map transition are separate checks.');
