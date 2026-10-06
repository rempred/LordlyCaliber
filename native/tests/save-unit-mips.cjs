'use strict';
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const rom = fs.readFileSync(process.env.LORDLY_TEST_ROM);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const m = new Mips(), dir = process.argv[2];
m.load(0x80070c00, rom.subarray(0x1000, 0x2f000));
m.load(0x8016af80, rom.subarray(0x40e80, 0x66e10));
m.set('sp', 0x803f0000);
const descriptor = 0x80187444, payload = 0x80350000;
if (process.argv[3] !== 'verify') {
    for (let id = 1; id <= 3; ++id) {
        const at = 0x80193bc0 + id * 56, name = Buffer.alloc(13);
        Buffer.from('Member' + id).forEach((c, i) => name[i] = c + 16);
        name[7] = 255;
        m.load(at, name);
        m.w8(at + 17, 1); m.w8(at + 18, 1); m.w8(at + 19, 10);
        m.w16(at + 22, 100); m.w16(at + 24, 90);
        m.w8(at + 51, id === 1 ? 2 : 0); m.w8(at + 52, id);
    }
    for (const [index, flags, ids, cells] of [
        [0, 1, [1, 2, 3, 0, 0], [0, 4, 8, 0, 0]],
        [1, 0x81, [1, 0, 0, 0, 0], [0, 0, 0, 0, 0]],
        [2, 1, [2, 100, 0, 0, 0], [1, 7, 0, 0, 0]]
    ]) {
        const at = 0x801971f0 + index * 25;
        m.w8(at, index + 1); m.w8(at + 1, flags);
        ids.forEach((id, i) => m.w8(at + 2 + i, id));
        cells.forEach((cell, i) => m.w8(at + 7 + i, cell));
        m.w8(at + 12, 23); m.w8(at + 13, 9); m.w8(at + 23, 2); m.w8(at + 24, 2);
    }
    m.set('a0', payload); m.set('a1', descriptor); m.call(0x80074494, 0x80000100, 5000000);
    fs.writeFileSync(path.join(dir, 'unit-payload.bin'), Buffer.from(m.ram.slice(m.phys(payload), m.phys(payload) + 6180)));
    fs.writeFileSync(path.join(dir, 'unit-ram.bin'), Buffer.from(m.ram));
} else {
    const fixture = JSON.parse(fs.readFileSync(path.join(dir, 'unit-verify.json')));
    for (const row of fixture.sizes) {
        m.set('a0', row.primary); m.set('a1', row.secondary);
        m.call(0x8016de1c);
        assert.equal(m.get('v0'), row.expected, `size for ${row.primary}/${row.secondary}`);
    }
    m.load(payload, Buffer.from(fixture.payload, 'hex'));
    m.set('a0', payload); m.set('a1', descriptor); m.call(0x800742f4, 0x80000100, 5000000);
    assert.deepEqual(Buffer.from(m.ram.slice(0x1971f0, 0x1971f0 + 75)), Buffer.from(fixture.units, 'hex'));
    // Whole-function identity was qualified against the Army snapshot. Execute
    // only field conversion blocks here; OS, menu allocation and natural load
    // are outside this reference check.
    const delta = 0x801135a0, live = at => (at + delta) >>> 0;
    m.load(live(0xc188c), rom.subarray(0xc188c, 0xc192c));
    m.load(live(0xc1ed0), rom.subarray(0xc1ed0, 0xc1f58));
    const saved = 0x80310000, active = 0x80311000, root = 0x80260000;
    m.w32(0x80196af8, root);
    for (const row of fixture.cases) {
        const record = Buffer.from(row.record, 'hex');
        m.load(saved, record); m.load(active, Buffer.alloc(54, 0xff));
        m.w8(active + 1, 0);
        m.set('s4', saved); m.set('s2', active); m.set('s8', 8);
        m.call(live(0xc188c), live(0xc192c));
        const grid = Array.from({length: 9}, (_, i) => m.r8(active + 4 + i));
        assert.deepEqual(grid, row.grid);
        assert.equal(m.r8(active + 3), row.leaderCell);
        m.set('s0', saved); m.set('s1', active); m.set('s6', 0x80193bc0); m.set('s3', 255);
        m.call(live(0xc1ed0), live(0xc1f58));
        assert.equal(m.r8(saved + 2), record[2], 'leader identity remains first');
        assert.equal(m.r8(saved + 7), row.leaderCell);
        const pairs = [];
        for (let i = 0; i < 5; ++i) if (m.r8(saved + 2 + i)) pairs.push([m.r8(saved + 2 + i), m.r8(saved + 7 + i)]);
        const expected = row.grid.flatMap((id, cell) => id === 255 ? [] : [[id, cell]]);
        assert.deepEqual(pairs.sort((a,b) => a[0]-b[0]), expected.sort((a,b) => a[0]-b[0]));
    }
    console.log(`PASS ${fixture.sizes.length} original class-size pairs, original save decoder and ${fixture.cases.length} Army formation materialize/store comparisons; ordinary/special IDs and leader pairing. No natural game launch or menu-allocation claim.`);
}
