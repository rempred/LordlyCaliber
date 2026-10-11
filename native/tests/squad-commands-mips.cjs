'use strict';
// Generated high-level orders executed by the original field-order publisher.
// Height, scenario-point lookup and movement cleanup services are test doubles.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');
const fixture = JSON.parse(fs.readFileSync(process.argv[2]));
const rom = fs.readFileSync(fixture.rom);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
// Descriptor 7 owns this publisher; descriptor 3 owns the earlier field loader.
const descriptor = 0x387c0 + 7 * 40;
const base = rom.readUInt32BE(descriptor), start = rom.readUInt32BE(descriptor + 8);
const end = rom.readUInt32BE(descriptor + 12), delta = base - start;
assert.equal(base, 0x801ad5a0); assert.equal(start, 0x101ce0);
assert(start <= 0x121f38 && end >= 0x123458);
const live = offset => (offset + delta) >>> 0;
for (const check of fixture.cases) {
    const m = new FloatMips();
    m.ram = new Uint8Array(0x400000);
    m.load(base, rom.subarray(start, end));
    const unit = 0x80300000, record = 0x80301000;
    m.set('sp', 0x803ffe00);
    m.load(record, Buffer.from(check.node, 'hex'));
    m.w32(unit, 0x11);
    [0, 0, 256, 256].forEach((n, i) => m.writeFloat(0x801f0d98 + 4 * i, n));
    // Actual JAL targets: cleanup/point lookup are in descriptor 7, heights in 3.
    m.trap(0x801b2b78, () => {});
    for (const height of [0x801a63fc, 0x801a66c8]) m.trap(height, c => c.setSingle(0, 7));
    m.trap(0x801da340, c => {
        assert.equal(c.get('a0'), check.x - 1);
        [40, 7, 60].forEach((n, i) => c.writeFloat(c.get('a1') + 4 * i, n));
    });
    m.set('a0', unit); m.set('a1', record);
    m.call(live(0x121f38));
    assert.equal(m.r8(unit + 0x91), check.move, 'movement mode');
    assert.equal(m.r8(unit + 0x92), check.wait === 2 ? 0 : check.wait, 'proximity mode');
    assert.equal(!!(m.r32(unit) & 0x800000), check.wait === 2, 'retreat state');
    if (check.moving) {
        assert.equal(m.r8(unit + 0x20), 1, 'one destination');
        assert.equal(m.readFloat(unit + 0x28), check.z ? check.x : 40);
        assert.equal(m.readFloat(unit + 0x2c), 7);
        assert.equal(m.readFloat(unit + 0x30), check.z || 60);
    }
    assert.equal(m.get('sp'), 0x803ffe00);
}
console.log(`PASS ${fixture.cases.length} generated orders through the original MIPS publisher: coordinates, town-point lookup, movement, pursuit eligibility and retreat flag. Height/cleanup services are simulated; this does not run full missions or pathfinding.`);
