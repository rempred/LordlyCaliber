'use strict';
// Original ROM codec and boot-load instructions in isolated 4 MiB RAM.
// No emulator, cartridge SRAM, source ROM or existing save is modified.
const fs = require('node:fs');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const filename = process.argv[2] || process.env.LORDLY_TEST_ROM;
if (!filename) { console.log('SKIP: supply the private Rev 0 ROM'); process.exit(0); }
const rom = fs.readFileSync(filename);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
assert.equal(crypto.createHash('sha256').update(rom).digest('hex'),
  '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
const m = new Mips(); m.ram = new Uint8Array(0x400000); m.set('sp', 0x80060000);
m.load(0x80070C00, rom.subarray(0x1000, 0x2F000));
const transfers = [];
for (const address of [0x800EA714, 0x80073FD4, 0x800900C0, 0x80090010])
  m.trap(address, () => {});
m.trap(0x8009DA50, c => {
  const from = c.get('a0'), to = c.get('a1'), length = c.get('a2');
  transfers.push([from,to,length]); c.load(to, rom.subarray(from,from+length));
});
m.call(0x80071FF4, 0x80072078);
assert.deepEqual(transfers, [[0x40E80,0x8016AF80,0x25F90]]);
const descriptor = 0x80187444, groups = [];
let bits = 0;
for (let at = descriptor; m.r32(at); at += 16) {
  assert(groups.length < 32);
  const base = m.r32(at), stride = m.r32(at+4), commands = m.r32(at+8), count = m.r32(at+12);
  assert(base >= 0x80000000 && base + stride * count <= 0x80400000);
  const fields = [];
  for (let cmd = commands; m.r8(cmd+1) & 127; cmd += 3) {
    const field = {offset:m.r8(cmd),bytes:m.r8(cmd+1)&127,bits:m.r8(cmd+2)};
    assert(field.bytes <= 4 && field.bits <= 32 && fields.length < 256);
    fields.push(field); bits += field.bits * count;
  }
  groups.push({base,stride,count,fields});
}
assert.equal(groups.length,21); assert.equal(bits,49433); assert.equal(Math.ceil(bits/8),6180);
const roster = groups.find(g => g.base === 0x80193BC0);
assert.equal(roster.stride,56); assert.equal(roster.count,100);
for (const offset of [17,18])
  assert.deepEqual(roster.fields.find(f => f.offset === offset),{offset,bytes:1,bits:8});
const state = groups.find(g => g.base === 0x80196A58);
for (let offset = 30; offset < 41; ++offset)
  assert.deepEqual(state.fields.find(f => f.offset === offset),{offset,bytes:1,bits:8});
let classValues = 0;
const payload = 0x80350000;
for (let batch = 0; batch < 3; ++batch) {
  const expected = [];
  for (let actor = 0; actor < 100; ++actor) {
    const primary = (batch*100+actor)&255, secondary = 255-primary;
    const at = roster.base + actor*56;
    m.w8(at+17,primary); m.w8(at+18,secondary); m.w8(at+19,99);
    expected.push([primary,secondary,99]);
  }
  m.load(payload-16,Buffer.alloc(6180+32,0xA6));
  m.set('a0',payload); m.set('a1',descriptor); m.call(0x80074494,0x80000100,5000000);
  assert.equal(m.r32(0x800AEFB4),6180);
  assert.equal(m.r8(payload-1),0xA6); assert.equal(m.r8(payload+6180),0xA6);
  for (let actor = 0; actor < 100; ++actor) {
    const at = roster.base + actor*56;
    m.w8(at+17,0xA6); m.w8(at+18,0xA6); m.w8(at+19,0xA6);
  }
  m.set('a0',payload); m.set('a1',descriptor); m.call(0x800742F4,0x80000100,5000000);
  for (let actor = 0; actor < 100; ++actor) {
    const at = roster.base + actor*56;
    assert.deepEqual([m.r8(at+17),m.r8(at+18),m.r8(at+19)],expected[actor]); classValues+=2;
  }
}
console.log(JSON.stringify({groups:groups.length,packedBits:bits,packedBytes:6180,
  classFieldValues:classValues,distinctClassIds:256,discoveryBytes:11,
  scope:'original descriptor/codec execution; save IO and extended discovery remain separate'}));
