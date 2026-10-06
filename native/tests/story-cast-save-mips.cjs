// Test fixture: original save packer and event getters, from the user's ROM.
'use strict';
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const rom = fs.readFileSync(process.env.LORDLY_TEST_ROM);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const m = new Mips(), dir = process.argv[2];
m.load(0x80070c00, rom.subarray(0x1000, 0x2f000));
m.load(0x8016af80, rom.subarray(0x40e80, 0x66e10));
m.set('sp', 0x803f0000);
// SaveDocument validates a populated first roster character independently of launch banks.
m.w8(0x80193bf8,255);m.w8(0x80193c09,0x58);m.w8(0x80193c0a,0x58);m.w8(0x80193c0b,10);
const casts = [
    [[0x54,0x55],[0x56,0x57],[0x61,0x61],[4,4],[2,2]],
    [[0x58,0x58],[0x59,0x59],[0x5a,0x5a],[0x5b,0x5b],[0x5c,0x5c]]
];
for (let slot=0;slot<casts.length;++slot) {
    casts[slot].forEach(([primary,secondary],i) => {
        m.w8(0x8019367c+i,primary);m.w8(0x80193681+i,secondary);
        m.set('a0',i);m.call(0x80170414);assert.equal(m.get('v0'),primary);
        m.set('a0',i);m.call(0x80170424);assert.equal(m.get('v0'),secondary);
    });
    m.set('a0',0x80350000);m.set('a1',0x80187444);m.call(0x80074494,0x80000100,5000000);
    fs.writeFileSync(path.join(dir,`cast-${slot}.bin`),Buffer.from(m.ram.slice(0x350000,0x350000+6180)));
    fs.writeFileSync(path.join(dir,`cast-${slot}-ram.bin`),Buffer.from(m.ram));
}
console.log('PASS original event getters for20 source bytes; two class-pair banks packed by original save code.');
