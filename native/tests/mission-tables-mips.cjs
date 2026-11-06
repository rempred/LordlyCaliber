'use strict';
// Usage: node mission-tables-mips.cjs <expanded ROM> [scenario key]
// The native three-instruction addressing windows run against the exported
// permanent bytes. Test offsets and row positions come from the original paths.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const {FloatMips} = require('./float-mips.cjs');
const image = fs.readFileSync(process.argv[2]);
if (image.readUInt32BE() === 0x37804012) image.swap16();
const desc = 0x27c5600, delta = 0x8012a100;
assert.equal(image.toString('ascii', desc, desc + 8), 'LCMSTBL1');
const storage = image.readUInt32BE(desc + 12), resident = image.readUInt32BE(desc + 16);
const row = key => storage + (key - 69) * 37;
const active = Array.from({length:64}, (_, i) => i + 69).filter(key => image[row(key) + 3]);
const key = process.argv[3] ? Number(process.argv[3]) : active.at(-1);
assert(active.includes(key));
const m = new FloatMips();
const classes = 0x5dc80;
const tableRom = resident < 0x80243db0 ? resident - delta :
    image.readUInt32BE(classes + 12) + resident - image.readUInt32BE(classes + 24);
m.load(resident, image.subarray(tableRom, tableRom + 4928));
function load(romOffset, stride, expected, index = key, highOffset = romOffset - 8) {
    const start = 0x80000800;
    // Copy the native address calculation and load. Some C variants schedule
    // the load in a branch delay slot; the branch is outside this data test.
    const code = Buffer.concat([image.subarray(highOffset, highOffset + 8), image.subarray(romOffset, romOffset + 4)]);
    assert.equal(code.readUInt32BE() >>> 26, 15, 'original LUI');
    const add = code.readUInt32BE(4), op = code.readUInt32BE(8);
    assert.equal(add & 63, 33, 'original ADDU');
    m.r[(add >>> 16) & 31] = index * stride;
    m.load(start, code);
    m.call(start, start + 12);
    assert.equal(m.r[(op >>> 16) & 31], expected, `z64 load at ${romOffset.toString(16)}`);
}
for (const [at, offset] of [
    [0x62bc,3], [0xebe10,3], [0xebf3c,4], [0xebfa4,5], [0xec128,7],
    [0xec1a0,8], [0xedcf4,7], [0xedda0,3], [0x2b50e0,3]
]) load(at, 9, image[row(key) + offset]);
load(0x2b50e0, 9, image[0x654a0 + 67 * 9 + 3], 67);
assert.equal(image.readUInt32BE(0xeddac) & 0xffff, 133, 'model reverse lookup includes key132');
for (const [at, offset] of [
    [0x101e54,7], [0x127900,5], [0x130dcc,3], [0x130e30,4], [0x1312b4,3],
    [0x131318,4], [0x13c0a8,8], [0x13c3b4,8], [0x196160,6]
]) load(at, 12, image[row(key) + 9 + offset]);
const modernShop = image.toString('ascii', 0x27c4000, 0x27c4008) === 'LC05SHOP' &&
    image.readUInt32BE(0x27c4008) >= 4;
load(modernShop ? 0x19be94 : 0x19be98, 12, image[row(key) + 14]);
// Expanded armies replace the treasure constructor with compiled C in the
// permanent allocation. Check the load actually reached by that entry jump.
if (image.readUInt32BE(0x1960a8) >>> 26 === 2) {
    const entry = (0x80000000 | (image.readUInt32BE(0x1960a8) & 0x3ffffff) << 2) >>> 0;
    const owner = image.readUInt32BE(classes + 12) + entry - image.readUInt32BE(classes + 24);
    const wanted = resident + 1200 + 6, found = [];
    for (let at = owner; at < owner + 0x400; at += 4) {
        const high = image.readUInt32BE(at), add = image.readUInt32BE(at + 4), low = image.readUInt32BE(at + 8);
        if (high >>> 26 === 15 && (add & 63) === 33 && low >>> 26 === 36 &&
            (((high << 16) + (low << 16 >> 16)) >>> 0) === wanted) found.push(at);
    }
    assert.equal(found.length, 1, 'compiled army constructor uses expanded treasure selector');
    load(found[0] + 8, 12, image[row(key) + 15]);
}
const payout = resident + 1200 + 10;
const payoutSites = [];
for (let at = 0x1390f8; at < 0x139c10; at += 4) {
    let highAt = at - 8;
    if (image.readUInt32BE(highAt) >>> 26 !== 15) highAt -= 4;
    const high = image.readUInt32BE(highAt), low = image.readUInt32BE(at);
    if ((high >>> 26) === 15 && (low >>> 26) === 37 &&
        (((high << 16) + (low << 16 >> 16)) >>> 0) === payout) payoutSites.push({at,highAt});
}
assert.equal(payoutSites.length, 1);
load(payoutSites[0].at, 12, image.readUInt16BE(row(key) + 19), key, payoutSites[0].highAt);
for (const [at, offset] of [
    [0x10324c,0], [0x10328c,1], [0x1032b8,2], [0x1032e4,3],
    [0x115b6c,0], [0x115ba4,3]
]) load(at, 4, image[row(key) + 21 + offset]);
for (const [at, offset] of [
    [0x102e6c,0], [0x102e8c,1], [0x102eac,2], [0x102ecc,3], [0x1154d8,0]
]) load(at, 12, image[row(key) + 25 + offset]);
// A maximum unsigned ordinal must reach the caller's existing >=0 path.
m.w8(resident + key * 9 + 8, 0xfd);
load(0xec1a0, 9, 0xfd);
console.log(`PASS ${active.length} occupied extended rows, native selector, map, camera, and payout windows for key ${key}; widened ESET ordinal and hidden image row67`);
