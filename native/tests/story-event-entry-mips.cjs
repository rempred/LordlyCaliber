'use strict';

// Run the resident event loader, scheduler, interpreter and scene-completion
// wrapper. Resource services return the directory and decoded rows from the
// supplied ROM. No scene renderer, input or saved game is involved.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {FloatMips} = require('./float-mips.cjs');

const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
assert.equal(rom.readUInt32BE(0), 0x80371240);
const codec = {window: {}, Uint8Array, Uint32Array, ArrayBuffer, DataView, Buffer};
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../../cutscene-codec.js'), 'utf8'), codec);
const resources = 0x594280, directory = 0x9398e8, directoryRam = 0x80500000;
const rows = new Map();
for (const id of [65, 67, 68]) {
    const key = rom.readUInt32BE(directory + 4 * (1 + id));
    const start = resources + key;
    const data = rom.subarray(start + 4, start + 4 + rom.readUInt32BE(start));
    rows.set(key, {id, address: 0x80600000 + (id - 65) * 0x10000,
                   bytes: codec.window.OB64.cutsceneCodec.decodeCustomLz(data).bytes});
}
const context = 0x80197b00;
const reportKey = rom.readUInt32BE(directory + 4 + 67 * 4);
const trainingClasses = new Set([5, 10, 12, 15, 18, 19, 20, 22, 25, 27, 30, 32, 33, 34, 35, 36, 37]);
class EventProbe extends FloatMips {
    constructor() {
        super();
        this.loaded = [];
        this.lastInstructionOffset = -1;
        this.set('sp', 0x807ff000);
        this.load(0x8006fc00, rom.subarray(0, 0x2f000));
        this.load(0x80163910, rom.subarray(0x39810, 0x66e10));
        this.trap(0x800712c4, () => this.set('v0', 0));
        this.trap(0x8009dbb8, () => {
            assert.equal(this.get('a1'), directory - resources);
            this.load(directoryRam, rom.subarray(directory + 4, directory + 4 + rom.readUInt32BE(directory)));
            this.set('v0', directoryRam);
        });
        this.trap(0x8009dd38, () => {
            const row = rows.get(this.get('a0'));
            assert.ok(row, 'requested a known row');
            this.loaded.push(row.id);
            this.load(row.address, row.bytes);
            this.set('v0', row.address);
        });
    }
    exec(pc, next) {
        if (pc === 0x801709b0) this.lastInstructionOffset = this.get('v0') - this.r32(context + 0x18);
        return super.exec(pc, next);
    }
}

function training(repeat, unit, unitClass) {
    const m = new EventProbe();
    m.w8(0x801936a8, 68);
    m.w16(context + 6, 0xffff);
    m.call(0x80172394);
    assert.equal(m.r16(context + 6), 68);
    assert.deepEqual(m.loaded, [68, 65]);
    if (repeat) m.w8(0x80193686 + (27 >> 3), 1 << (27 & 7));
    m.w8(0x801976e8, unit);
    if (unit) {
        m.w8(0x801971f2 + 25 * unit, 1);
        m.w8(0x80195560 + 52 + 0x12, unitClass);
    }
    m.set('a0', 8);
    m.call(0x8017223c); // Request the encounter through its ordinary caller.
    assert.equal(m.r16(0x8018f1a0) & 0x3fff, repeat ? 1080 : 1079);
    m.call(0x80172124); // External scene has finished.
    assert.equal(m.lastInstructionOffset, repeat ? 142 : 40);
    assert.equal(m.r8(context + 1), 0);
    assert.equal(m.r8(context + 2), 0);
    assert.ok(m.r8(0x80193686 + (27 >> 3)) & (1 << (27 & 7)));
    if (repeat) {
        const discipline = m.r8(0x80193686 + (170 >> 3)) & (1 << (170 & 7));
        assert.equal(Boolean(discipline), unit === 0 || trainingClasses.has(unitClass));
    }
}
training(false, 0, 0);
training(true, 0, 0);
for (let i = 0; i < 165; i++) training(true, 30, i);

// func_00047ee0 is a separate call path. Stop at the interpreter boundary to
// check exactly which routine it receives and restore the surrounding event.
const report = new EventProbe();
const reportEntries = [14, 4370, 5128, 7074, 8988, 9330];
for (const [index, entry] of reportEntries.entries()) {
    for (let i = 0; i < 100; ++i) report.w8(context + i, i + 11);
    report.trap(0x80170954, () => {
        const row = rows.get(reportKey);
        assert.equal(report.get('a0'), row.address + entry);
        assert.equal(report.r8(context), 67);
        assert.equal(report.r8(context + 1), index);
        report.w32(context + 0x14, 0x12345678);
    });
    report.set('a0', 67);
    report.set('a1', index);
    report.call(0x80171fe0);
    assert.equal(report.get('v0'), 0x12345678);
    for (let i = 0; i < 100; ++i) assert.equal(report.r8(context + i), i + 11);
}
assert.deepEqual(report.loaded, Array(6).fill(67));
console.log('Passed 167 loader-to-training continuations and all six direct report entries.');
