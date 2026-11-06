'use strict';

// Execute the four original event-directory lookup paths and their resource
// reader. Only allocation and PI transfer are doubled. Each path runs at its
// actual resident RAM address, including its original branch delay slots.
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');

const [stockPath, exportedPath] = process.argv.slice(2);
if (!stockPath) throw Error('Supply the stock ROM and the 64-mission export');
const base = 0x594280;
const worldProgram = 0x80300000;
const residentAddress = romAddress => 0x8012a100 + romAddress;
const context = {window: {}, Uint8Array, Uint32Array, ArrayBuffer, DataView, Buffer};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, '../../cutscene-codec.js'), 'utf8'), context);

function readRom(file) {
    const rom = fs.readFileSync(file);
    if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
    assert.equal(rom.readUInt32BE(0), 0x80371240);
    return rom;
}

function machine(rom, transfers = []) {
    const m = new FloatMips();
    m.ram = new Uint8Array(0x400000);
    m.load(0x80070c00, rom.subarray(0x1000, 0x3f1b0));
    m.load(0x80163910, rom.subarray(0x39810, 0x66e10));
    m.set('sp', 0x803ff000);
    m.trap(0x80070f30, cpu => {
        assert(cpu.get('a0') <= 0x1000, 'event directory allocation size');
        cpu.set('v0', 0x800f0000);
    });
    m.trap(0x80089f80, cpu => {
        const from = cpu.get('a0'), to = cpu.get('a1'), count = cpu.get('a2');
        assert(from + count <= rom.length, 'directory read exceeds cartridge');
        cpu.load(to, rom.subarray(from, from + count));
        transfers.push([from, count]);
    });
    return m;
}

const paths = [
    {start: 0x47f3c, end: 0x47f90, setRow: (m, row) => m.set('t1', row)},
    {start: 0x480c4, end: 0x480e4, setRow: (m, row) => m.w8(0x80197b00, row)},
    {start: 0x48390, end: 0x48478, aux: 65, setRow: (m, row) => m.w8(0x801936a8, row)},
    {start: 0x4843c, end: 0x48478, aux: 70, setRow: (m, row) => m.w8(0x801936a8, row)}
];
let lookups = 0;
function lookUp(rom, table, rows) {
    const size = rom.readUInt32BE(table);
    const key = row => rom.readUInt32BE(table + 4 + row * 4);
    for (const row of rows) {
        for (const {start, end, aux, setRow} of paths) {
            const transfers = [];
            const m = machine(rom, transfers);
            setRow(m, row);
            m.call(residentAddress(start), residentAddress(end), 10000);
            assert.deepEqual(transfers, [[table, 4], [table + 4, size]],
                `resident path ${start.toString(16)} requests the expected directory`);
            assert.equal(m.get('s0'), key(row), `native directory lookup for row ${row}`);
            if (aux !== undefined) assert.equal(m.get('s1'), key(aux), 'original auxiliary program');
            lookups++;
        }
    }
}

lookUp(readRom(stockPath), 0x9398e8, [0, 65, 70, 114]);
if (exportedPath) {
    const image = readRom(exportedPath);
    const expanded = 0x27c5800;
    lookUp(image, expanded, [0, 2, 3, 65, 70, 114, 115, 127, 128, 178]);

    const offset = base + image.readUInt32BE(expanded + 4);
    const length = image.readUInt32BE(offset);
    const entryProgram = Buffer.from(context.window.OB64.cutsceneCodec.decodeCustomLz(
        image.subarray(offset + 4, offset + 4 + length)).bytes);
    for (const row of [115, 127, 128, 178]) {
        const route = Buffer.from([0x68, 0, 0x88, 0, 0x12, row]);
        const entry = entryProgram.indexOf(route);
        assert(entry >= 0 && !(entry & 1), `world entry selects row ${row}`);
        const m = machine(image);
        m.load(worldProgram, route);
        for (let word = 0; word < 3; ++word) {
            m.set('a0', worldProgram);
            m.set('sp', 0x803ff000);
            m.w16(0x80197b04, word);
            m.call(0x80170954, 0x80171b58, 10000);
        }
        assert.equal(m.r8(0x801936a8), row);
    }
}
console.log(`PASS ${lookups} native directory lookups${exportedPath ? ' and four authored world-entry operands' : ''}`);
