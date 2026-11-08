'use strict';
// Execute freshly exported event programs in the resident native VM. Scene
// playback is treated as a completed external operation between VM steps.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const zlib = require('node:zlib');
const assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');

const directory = process.argv[2];
if (!directory) throw Error('Supply the exported mission test directory');
const expected = JSON.parse(fs.readFileSync(path.join(directory, 'mission-fixture.json'), 'utf8'));
const files = fs.readdirSync(directory).filter(name => /\.(z64|v64)$/i.test(name));
const selected = process.argv[3] || expected.rom;
if (!selected) assert.equal(files.length, 1, 'select a ROM filename when the fixture has variants');
const rom = fs.readFileSync(path.resolve(directory, selected || files[0]));
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
assert.equal(rom.readUInt32BE(0), 0x80371240);
assert([2, 3].includes(expected.event) || (expected.event >= 115 && expected.event < 179));
assert(expected.location >= 1 && expected.location <= 40);
assert(expected.battlefield >= 1 && expected.battlefield < 133);
assert(expected.completionFlag >= 256 && expected.completionFlag < 768);

const context = {window: {}, Uint8Array, Uint32Array, ArrayBuffer, DataView, Buffer};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, '../../cutscene-codec.js'), 'utf8'), context);
const directoryBase = expected.event < 115 ? 0x9398e8 : 0x27c5800;
const key = rom.readUInt32BE(directoryBase + 4 + expected.event * 4);
assert(key, 'the custom event directory entry must be populated');
const start = 0x594280 + key;
const length = rom.readUInt32BE(start);
const program = Buffer.from(context.window.OB64.cutsceneCodec.decodeCustomLz(
    rom.subarray(start + 4, start + 4 + length)).bytes);
const descriptor = 0x5dc80;
assert.equal(rom.toString('ascii', descriptor, descriptor + 8), 'LC05CLAS');
assert.equal(rom.readUInt32BE(descriptor + 8), 6, 'the exported flag runtime must be installed');
const dataOffset = rom.readUInt32BE(descriptor + 12);
const dataSize = rom.readUInt32BE(descriptor + 16);
const dataAddress = rom.readUInt32BE(descriptor + 24);
const metadata = rom.subarray(dataOffset + dataSize - 32, dataOffset + dataSize);
assert.equal(metadata.toString('ascii', 0, 8), 'LCFLAGS1');
const bank = metadata.readUInt32BE(8);
const residentAddress = rom.readUInt32BE(descriptor + 104);
const residentSize = rom.readUInt32BE(descriptor + 108);
const PROGRAM = 0x80700000, SP = 0x807ff000;
function machine(row = expected.event) {
    const m = new FloatMips();
    m.load(0x80163910, rom.subarray(0x39810, 0x66e10));
    m.load(dataAddress, rom.subarray(dataOffset, dataOffset + dataSize));
    m.load(PROGRAM, program);
    m.w8(0x801936a8, row);
    m.w8(0x801936aa, expected.location);
    m.w8(0x80196a9a, expected.location);
    m.w32(0x80197b1c, PROGRAM);
    m.w32(0x80197b18, PROGRAM);
    m.set('sp', SP);
    return m;
}
const read = (m, address, length) => Buffer.from(m.ram.subarray(m.phys(address), m.phys(address) + length));
const flag = (m, id) => (m.r8(bank + ((id - 256) >> 3)) >> ((id - 256) & 7)) & 1;
const entries = group => {
    const output = [];
    for (let at = program.readUInt16BE(group * 2);; at += 2) {
        assert(at + 2 <= program.length, 'handler list exceeds event resource');
        const entry = program.readUInt16BE(at);
        if (entry === 65535) return output;
        assert(entry >= 12 && entry + 2 <= program.length && !(entry & 1));
        output.push(entry);
    }
};
function execute(m, entry) {
    let pc = entry;
    const scenes = [];
    for (let count = 0; count < 1024; ++count) {
        assert(pc >= 12 && pc + 2 <= program.length && !(pc & 1));
        const op = program[pc];
        m.set('sp', SP);
        m.set('a0', PROGRAM);
        m.w16(0x80197b04, pc / 2);
        m.call(0x80170954, 0x80171b58, 50000);
        if (op === 0x10) {
            assert.equal(m.r16(0x8018f1a2), 0x8002);
            scenes.push(m.r16(0x8018f1a0) & 0x3fff);
        }
        if (op === 0x11 || op === 0x12 || op === 0x2f) return {scenes, op};
        pc = (m.r16(0x80197b04) + 1) * 2;
    }
    throw Error('The authored handler did not terminate');
}
assert.equal(entries(0).length, 1, 'one opening template handler');
assert.equal(entries(5).length, 1, 'one closing template handler');
let nativeCases = 0;
for (const row of [expected.event]) {
    const m = machine(row);
    m.w32(0x80197b0c, 1);
    m.w32(0x80197b10, 1);
    m.w8(0x80197b00, 0);
    m.w8(0x80197b01, 0);
    m.w8(0x80197b03, 0);
    m.w16(0x80197b04, 0);
    m.set('a0', 1);
    m.call(0x80171b94, 0x80000100, 100000);
    assert.equal(m.r16(0x8018f1a0) & 0x3fff, 1078, 'native scheduler reaches the opening effect');
    assert.equal(m.r8(0x801936a7), expected.battlefield);
    assert.equal(m.r8(0x8018f481), expected.battlefield, 'staged battlefield key');
    assert.equal(m.r8(0x801936a8), row, 'custom row identity is retained');
    nativeCases++;
}
for (const pattern of [0, 0xa5, 255]) {
    const m = machine();
    m.load(bank, Buffer.alloc(64, pattern));
    const before = read(m, bank, 64);
    const opened = execute(m, entries(0)[0]);
    assert.equal(opened.op, 0x11);
    assert(opened.scenes.includes(1078));
    assert.equal(m.r8(0x801936a7), expected.battlefield);
    assert.equal(m.r16(0x8018f1a2), 0x8003, 'native mode request starts the chosen battlefield');
    assert.deepEqual(read(m, bank, 64), before, 'opening preserves all custom flags');
    m.load(0x80196a86, Buffer.alloc(10, 0xaa));
    m.load(0x80196a93, Buffer.alloc(5, 255));
    const beforeStates=read(m,0x80196a86,10),beforeMarkers=read(m,0x80196a93,5);
    const closed = execute(m, entries(5)[0]);
    assert.equal(closed.op, 0x11);
    assert.equal(flag(m, expected.completionFlag), 1);
    const after = Buffer.from(before), id = expected.completionFlag - 256;
    after[id >> 3] |= 1 << (id & 7);
    assert.deepEqual(read(m, bank, 64), after, 'closing changes only its completion flag');
    assert.equal(m.r8(0x80196a9a), 0x80 | expected.location);
    assert.equal(m.r8(0x801936aa), expected.location, 'saved location snapshot uses the new map location');
    assert.equal(m.r16(0x8018f1a2), 0x8005, 'completion requests the world map, not the save menu');
    assert.equal(m.r8(0x801936a8),0,'completion leaves the mission program');
    const stateBit=(expected.location-1)*2,markerBit=expected.location-1;
    const states=Buffer.from(beforeStates),markers=Buffer.from(beforeMarkers);
    states[stateBit>>3]=(states[stateBit>>3]&~(3<<(stateBit&7)))|(1<<(stateBit&7));
    markers[markerBit>>3]&=~(1<<(markerBit&7));
    assert.deepEqual(read(m,0x80196a86,10),states,'only the completed location changes to cleared');
    assert.deepEqual(read(m,0x80196a93,5),markers,'only its active mission marker is cleared');
    assert.deepEqual(read(m, PROGRAM, program.length), program, 'VM preserves the exported program');
    nativeCases += 2;
}
if (expected.townFlag !== undefined) {
    assert.equal(entries(2).length, 1, 'one authored town handler');
    for (const [site, applied] of [[0, true], [1, false]]) {
        const m = machine();
        m.load(bank, Buffer.alloc(64, 0));
        m.w8(0x801976e8, site); // F8 is this zero-based site plus one.
        const result = execute(m, entries(2)[0]);
        assert.equal(result.op, 0x2f);
        const after = Buffer.alloc(64, 0), id = expected.townFlag - 256;
        if (applied) after[id >> 3] |= 1 << (id & 7);
        assert.deepEqual(read(m, bank, 64), after,
                         'town target guard changes only its assigned flag');
        nativeCases++;
    }
}
// The same native transition command supports both report-first and direct
// world-map templates. Change only its immediate in private test memory.
for (const request of [3, 27]) {
    const m = machine();
    const probe = Buffer.from([0x68, request, 0x11, 0]);
    m.load(PROGRAM + program.length + 16, probe);
    m.set('sp', SP);
    m.set('a0', PROGRAM + program.length + 16);
    m.w16(0x80197b04, 0);
    m.call(0x80170954, 0x80171b58, 50000);
    m.w16(0x80197b04, 1);
    m.set('sp', SP);
    m.set('a0', PROGRAM + program.length + 16);
    m.call(0x80170954, 0x80171b58, 50000);
    assert.equal(m.r16(0x8018f1a2), request === 27 ? 0x8016 : 0x8005);
    nativeCases++;
}
const authoredAt = rom.indexOf('LC08MSSN', 0x2800000);
const resident = rom.subarray(residentAddress - 0x8012a100,
                             residentAddress - 0x8012a100 + residentSize);
const marker = resident.indexOf('LCMISN02');
assert(marker >= 0, 'the authored mission mode helper must be installed');
const exit = resident.readUInt32BE(marker + 28);
assert(exit >= residentAddress && exit < residentAddress + marker && !(exit & 3));
assert.equal(rom.readUInt32BE(0xec6fc), 0x0c000000 | ((exit >>> 2) & 0x3ffffff));
assert.equal(rom.subarray(0xec700, 0xec70c).toString('hex'), '000000001440002500000000');

// Execute the outer native field release, including its original result switch
// and boot request store. The previous VM-only check did not reach this owner.
const release = machine();
release.load(0x8019ee50, rom.subarray(0xebbb0, 0xf9ff0));
for (const target of [0x80089a10, 0x80221cb8, 0x8007485c, 0x80079618])
    release.trap(target, () => {});
const suspendCall = rom.readUInt32BE(0xec740);
assert.equal(suspendCall >>> 26, 3);
const suspendTarget = (0x80000000 | ((suspendCall & 0x3ffffff) << 2)) >>> 0;
let suspendResult = 0xfffd;
if (suspendTarget !== 0x8007485c)
    release.trap(suspendTarget, () => release.set('v0', suspendResult));
function checkRelease(key, result, mode, victory, event, scene, vmMode, parentMode, expectedMode) {
    const frame = 0x80710000, parent = frame + 8;
    release.w32(0x800c4bbc, frame);
    release.w32(frame, parentMode === null ? 0 : parent);
    release.w16(frame + 4, 3);
    release.w16(parent + 4, parentMode ?? 0);
    release.w16(0x800e810e, mode);
    release.w32(0x801ab84c, result >>> 0);
    release.w8(0x8018f481, key);
    release.w32(0x801f0de0, 0); // Native field cleanup already reset the phase.
    release.w16(0x801e7d62, victory);
    release.w8(0x801936a8, event);
    release.w16(0x8018f1a0, scene);
    release.w16(0x8018f1a2, vmMode);
    release.w16(0x800c4c26, 0xffff);
    release.set('sp', SP);
    release.call(0x8019f974, 0x80000100, 10000);
    assert.equal(release.r16(0x800c4c26), expectedMode);
    assert.equal(release.get('sp'), SP);
    nativeCases++;
}
const originalModes = [0xfffe, 0xfffd, 0xfffe, 0xfffd, 0xfffe, 1, 6, 2, 0x8002, 0x15, 0xfffe];
for (const key of [1, 65, 66, expected.battlefield]) {
    for (const mode of [0, 3]) {
        for (let result = -4; result <= 6; ++result) {
            const native = !mode || ([65, 66].includes(key) && result === -3)
                ? 0xfffe : originalModes[result + 4];
            checkRelease(key, result, mode, 0, expected.event, 0, 0xfffe, null, native);
        }
    }
}
for (const key of [1, expected.battlefield]) {
    checkRelease(key, -2, 3, 0x301, 0, 0, 0x8005, null, 0x8005);
    checkRelease(key, -2, 3, 0x302, 0, 0, 0x8005, null, 0x8005);
    checkRelease(key, -2, 3, 0x302, 0, 0, 0x8005, 5, 0xfffe);
    checkRelease(key, -2, 3, 0x302, 0, 0, 0x8005, 2, 0x8005);
    checkRelease(key, -2, 3, 0x401, 0, 0, 0x8005, null, 0xfffe);
    checkRelease(key, -2, 3, 0x302, expected.event, 0, 0x8005, null, 0xfffe);
    checkRelease(key, -2, 3, 0x302, 0, 1078, 0x8005, null, 0xfffe);
    checkRelease(key, -2, 3, 0x302, 0, 0, 0xfffe, null, 0xfffe);
    checkRelease(key, 4, 3, 0x302, expected.event, 1078, 0x8005, null, 0x8002);
}
if (suspendTarget !== 0x8007485c) {
    suspendResult = 0xfffe;
    checkRelease(expected.battlefield, -1, 3, 0, expected.event, 0, 0xfffe, null, 0xfffe);
}
if (authoredAt >= 0) {
    const packedSize = rom.readUInt32BE(authoredAt + 20);
    const authored = JSON.parse(zlib.inflateSync(rom.subarray(authoredAt + 68,
                                        authoredAt + 64 + packedSize)).toString());
    const entry = resident.readUInt32BE(marker + 8);
    const briefing = resident.readUInt32BE(marker + 24);
    const briefingCall = 0x0c000000 | ((briefing >>> 2) & 0x3ffffff);
    assert(briefing >= residentAddress && briefing < residentAddress + marker && !(briefing & 3));
    for (const at of [0x175c20, 0x175e5c]) {
        assert.equal(rom.readUInt32BE(at), briefingCall, 'camera and site panel use the same authored fallback');
        assert.equal(rom.readUInt32BE(at + 4), 0);
    }
    let calls = 0;
    for (let at = 0x1390f0; at < 0x139c10; at += 4)
        calls += rom.readUInt32BE(at) === (0x0c000000 | ((entry >>> 2) & 0x3ffffff));
    assert.equal(calls, 1, 'native settlement must call the compiled capture predicate');
    const m = machine();
    // Run the original opposing-squad site lookup and its coordinate reader.
    // Terrain height contributes only Y, which the site match ignores.
    m.load(0x801dc4ec, rom.subarray(0x130c2c, 0x130d78));
    m.load(0x801da340, rom.subarray(0x12ea80, 0x12ebb8));
    m.load(0x801ee8c0, rom.subarray(0x143000, 0x143008));
    m.trap(0x801a63fc, c => c.setSingle(0, 0));
    const points = 0x80710000, opposing = 0x801f2700;
    m.w32(0x800e7a90, points);
    m.w8(0x800e7ac5, 0);
    m.w32(0x801f0d28, opposing);
    function checkBriefing(key, siteCount, occupied, x, z, expectedSite) {
        m.w8(0x8018f481, key);
        m.w32(0x801f1070, siteCount);
        for (let site = 0; site < siteCount; ++site) {
            m.writeFloat(points + 28 + site * 8, site * 5);
            m.writeFloat(points + 32 + site * 8, site * 7);
        }
        m.w32(opposing, occupied ? 0x10 : 0);
        m.writeFloat(opposing + 8, x);
        m.writeFloat(opposing + 16, z);
        m.set('sp', SP);
        m.call(briefing, 0x80000100, 10000);
        assert.equal(m.r[2] | 0, expectedSite, `briefing site for key ${key}, occupied ${occupied}, (${x}, ${z})`);
        assert.equal(m.r8(0x8018f481), key);
        nativeCases++;
    }
    m.load(0x801e54d0, rom.subarray(0x139c10, 0x139c88));
    const pointerAt = ((rom.readUInt32BE(0x139c28) & 65535) * 65536 +
                       (rom.readInt16BE(0x139c2e))) >>> 0;
    m.w32(pointerAt, 0x801f2700);
    m.w32(pointerAt + 4, 0x801f27c0);
    for (const row of authored) {
    const fallback = Math.max(0, row.snapshot.units.objective - 1);
    const count = Math.max(2, fallback + 1);
    const nativeSite = fallback === 0 ? 1 : 0;
    checkBriefing(row.key, count, false, 0, 0, fallback);
    checkBriefing(row.key, count, true, 100, 100, fallback);
    checkBriefing(row.key, count, true, nativeSite * 5, nativeSite * 7, nativeSite);
    m.w8(0x8018f481, row.key);
    if (row.mode === 'exact') {
        for (const [first, second] of [[0, 0], [0x10, 0], [0, 0x10], [0x10, 0x10]]) {
            m.w32(0x801f2700, first);
            m.w32(0x801f27c0, second);
            m.w16(0x801952a4, 0);
            const win = [45, 46].includes(row.source) ? !(first || second) : !first;
            m.call(entry, 0x80000100, 10000);
            assert.equal(m.r[2] >>> 0, +win, `exact-copy donor ${row.source} victory preserved`);
            assert.equal(m.r8(0x8018f481), row.key, 'exact-copy predicate restores current battlefield key');
            nativeCases++;
        }
        if (row.source === 8) {
            m.w32(0x801f2700, 0x10);
            m.w16(0x801952a4, 2);
            m.call(entry, 0x80000100, 10000);
            assert.equal(m.r[2] >>> 0, 1, 'source8 retains its special captured-site victory');
            assert.equal(m.r8(0x8018f481), row.key);
            nativeCases++;
        }
        continue;
    }
    const objective = row.snapshot.units.objective;
    assert(objective > 0 && objective <= 20);
    m.w32(0x801f1070, objective);
    const site = 0x801951cc + (objective - 1) * 36;
    for (const [flags, result] of [[4, 0], [0, 0], [6, 0], [2, 1], [0x2012, 1]]) {
        m.w16(site, flags);
        m.set('sp', SP);
        m.call(entry, 0x80000100, 10000);
        assert.equal(m.r[2] >>> 0, result, `objective flags ${flags} produce capture victory ${result}`);
        nativeCases++;
    }
    m.w32(0x801f1070, objective - 1);
    m.call(entry, 0x80000100, 10000);
    assert.equal(m.r[2] >>> 0, 0, 'invalid/unloaded objective cannot complete a mission');
    }
    const ordinary = authored.some(row => row.key === 1) ? 2 : 1;
    checkBriefing(ordinary, 2, true, 5, 7, 1);
    checkBriefing(ordinary, 2, true, 100, 100, -1);
    m.w8(0x8018f481, ordinary);
    m.w32(pointerAt, 0x801f2700);
    for (const [flags, result] of [[0x10, 0], [0, 1]]) {
        m.w32(0x801f2700, flags);
        m.call(entry, 0x80000100, 10000);
        assert.equal(m.r[2] >>> 0, result, 'ordinary missions preserve native boss defeat victory');
        nativeCases++;
    }
}
console.log(`PASS ${nativeCases} exported native mission cases: row${expected.event} scheduler dispatch, opening scene, battlefield selection, start request, completion flag, cleared map state and return destination with unrelated custom flags preserved.`);
console.log('Scene completion is simulated between native VM instructions. Authored victory and briefing site fallbacks are checked with custom battlefield fixtures. Full result timing has a separate mission-return-mips probe; rendering and complete console gameplay remain outside this test.');
