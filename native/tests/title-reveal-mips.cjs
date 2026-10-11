"use strict";
const fs = require("node:fs"), assert = require("node:assert/strict");
const {FloatMips} = require("./float-mips.cjs");
const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const m = new FloatMips(); m.ram = new Uint8Array(0x400000);
// ROM2829BC..2829D8 supplies this exact source/destination/extent to DMA.
m.load(0x8022ac90, rom.subarray(0x286bd0, 0x29a4c0));
m.load(0x80227dc4, rom.subarray(0x283d94, 0x283de8));
// Verify the original draw prefix: opaque black, primitive-only combiner.
assert.equal(m.r32(0x8023dfe0), 0xfa000000);
assert.equal(m.r32(0x8023dfe4), 0x000000ff);
assert.equal(m.r32(0x8023dfe8), 0xfcffffff);
assert.equal(m.r32(0x8023dfec), 0xfffdf6fb);
const record = 0x80300010, dl = 0x80310000, sp = 0x803ffe00;
let allocations = 0, frees = 0, textureLoads = 0, high = 0;
m.trap(0x80070f30, c => { assert.equal(c.get("a0"), 14); ++allocations; c.set("v0", record); });
m.trap(0x800712c4, c => { assert.equal(c.get("a0"), record); ++frees; });
m.trap(0x8016cd30, c => c.set("v0", high));
// The CI4 loader changes texture state, not primitive color or combine mode.
// Its independent board allocation is controlled; rectangle emission executes.
m.trap(0x80234b38, c => { assert.equal(c.get("a0"), 0); ++textureLoads; });
const call = (pc, args = []) => {
    m.set("sp", sp);
    args.forEach((v, i) => i < 4 ? m.set("a" + i, v) : m.w32(sp + 16 + (i - 4) * 4, v));
    m.call(pc); assert.equal(m.get("sp"), sp); return m.get("v0");
};
const snapshot = () => ({record: m.r32(0x8023de38) ? Buffer.from(m.ram.subarray(m.phys(record), m.phys(record) + 14)).toString("hex") : "", query: call(0x8023519c), allocations, frees});
const draw = () => {
    m.w32(0x800e9ba0, dl); m.load(dl, Buffer.alloc(256));
    const before = snapshot(), loads = textureLoads;
    if (before.record && m.r16(record + 10) === 0) {
        assert.throws(() => call(0x80234e10), /unsupported|break/i);
        return {...before, fault: true};
    }
    call(0x80234e10);
    const words = [];
    for (let at = dl; at < m.r32(0x800e9ba0); at += 8)
        if (m.r32(at) >>> 24 === 0xe4) {
            assert.equal(m.r32(at + 8), 0xe1000000); assert.equal(m.r32(at + 16), 0xf1000000);
            words.push(m.r32(at), m.r32(at + 4), m.r32(at + 12), m.r32(at + 20));
        }
    assert.equal(words.length, before.record ? 8 : 0);
    assert.equal(textureLoads - loads, before.record && high ? 1 : 0);
    assert.deepEqual(snapshot(), before);
    return {...before, words};
};
const rows = [];
for (high of [0, 1])
for (const initial of [[-1,-1], [0,240], [12,29], [-32768,32767]])
for (const final of [[0,240], [80,160], [-1,-1], [32767,-32768]])
for (const duration of [0,1,7,0xffff])
for (const persistence of [0,1]) {
    m.load(record - 16, Buffer.alloc(46, 0xa7)); m.w32(0x8023de38, 0);
    allocations = frees = 0;
    const args = [...initial, ...final, duration, persistence].map(x => x >>> 0), steps = [];
    call(0x80234d10, args); steps.push({start: args, ...draw()});
    for (let i = 0; i < (duration === 0xffff ? 4 : duration + 2); ++i) {
        call(0x80234db8); steps.push({...draw(), update: true});
    }
    const replacement = [0xffffffff, 0xffffffff, 10, 20, 3, 1];
    call(0x80234d10, replacement); steps.push({start: replacement, ...draw()});
    for (let i = 0; i < 5; ++i) { call(0x80234db8); steps.push({...draw(), update: true}); }
    for (let i = 0; i < 16; ++i) { assert.equal(m.r8(record - 16 + i), 0xa7); assert.equal(m.r8(record + 14 + i), 0xa7); }
    rows.push({high, steps});
}
fs.writeFileSync(process.argv[3], JSON.stringify({rows}));
console.log(`${rows.length} original title reveal sequences; allocator, resolution and texture loader controlled; interpolation, lifecycle, query and rectangle emission execute.`);
