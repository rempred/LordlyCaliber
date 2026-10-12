'use strict';
// Execute the original next-order resolver with exported ESET records.
// Condition truth and dormant capacity services are controlled test inputs.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');
const f = JSON.parse(fs.readFileSync(process.argv[2]));
const rom = fs.readFileSync(f.rom);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const descriptor = 0x387c0 + 7 * 40;
const base = rom.readUInt32BE(descriptor), start = rom.readUInt32BE(descriptor + 8);
const end = rom.readUInt32BE(descriptor + 12), live = offset => (offset + base - start) >>> 0;
assert.equal(base, 0x801ad5a0); assert.equal(start, 0x101ce0);
assert(start <= 0x1237f0 && end > 0x124000);
let count = 0;
function run(node, a, b, expected, active = 0) {
    const m = new FloatMips(); m.ram = new Uint8Array(0x400000);
    m.load(base, rom.subarray(start, end)); m.set('sp', 0x803ffe00);
    f.nodes.forEach((hex, i) => m.load(0x801f0ebe + 18*i, Buffer.from(hex, 'hex')));
    f.conditions.forEach((hex, i) => m.load(0x801f0e19 + 10*i, Buffer.from(hex, 'hex')));
    m.load(0x801f0ebe + 18*(node[0]-4), node);
    const unit = 0x80300000;
    m.w32(unit, 0x11); m.w8(unit + 0xba, node[0]); m.w8(0x801f0fde, active);
    m.trap(live(0x123458), c => {
        const id = c.get('a0');
        assert([f.conditionA, f.conditionB].includes(id), `Unexpected condition ${id}`);
        c.set('v0', id === f.conditionA ? a : b);
    });
    m.trap(live(0x12e8ec), c => c.set('v0', 1));
    m.set('a0', unit); m.call(live(0x1237f0));
    assert.equal(m.get('v0'), expected, `mode ${node[11]}, A=${a}, B=${b}, active=${active}`);
    assert.equal(m.get('sp'), 0x803ffe00); ++count;
}
const source = Buffer.from(f.nodes[f.first-4], 'hex');
assert.equal(source[11], 1); assert.equal(source[16], f.second); assert.equal(source[17], f.third);
for (const a of [0,1]) for (const b of [0,1]) {
    run(source,a,b,a?f.second:b?f.third:0);
    for (const mode of [2,3]) {
        const n=Buffer.from(source);n[11]=mode;n[16]=0;
        run(n,a,b,(mode===2?(a&&b):(a||b))?f.third:0);
    }
}
const single=Buffer.from(source);single[11]=0;single[12]=0;single[16]=0;
run(single,0,0,0);run(single,1,0,f.third);
const unconditioned=Buffer.from(single);unconditioned[10]=0;
run(unconditioned,0,0,f.third);run(unconditioned,0,0,0,15);
const disabled=Buffer.from(source);disabled[17]=0;run(disabled,1,1,0);
for (const id of [f.second,f.third]) {
    const n=Buffer.from(f.nodes[id-4],'hex');run(n,0,0,n[17]);
}
console.log(`PASS ${count} original resolver cases: exported branch targets, follow-ups, loop, AND/OR, false-condition waits, disabled follow-ups and capacity. Trigger truth is simulated; no full mission is replayed.`);
