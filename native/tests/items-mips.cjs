'use strict';
// Run the actual exported loader/getters and original save codec in 4 MiB RAM.
// Expected fields come from the authored project, not the relocated ROM table.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const fixture = JSON.parse(fs.readFileSync(process.argv[2]));
const rom = fs.readFileSync(fixture.rom);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const header = rom.subarray(0x5dc80, 0x5dd00);
assert.equal(header.toString('ascii', 0, 8), 'LC05CLAS');
assert.equal(header.readUInt32BE(120), fixture.records.length);
const m = new Mips(); m.ram = new Uint8Array(0x400000); m.set('sp', 0x803f0000);
m.load(0x80070c00, rom.subarray(0x1000, 0x2f000));
m.load(0x8016af80, rom.subarray(0x40e80, 0x66e10));
const base = header.readUInt32BE(24), end = header.readUInt32BE(48);
let transfers = 0, heaps = 0;
m.trap(0x8009da50, c => {
  assert.equal(c.get('a0'), header.readUInt32BE(12));
  assert.equal(c.get('a1'), base); assert.equal(c.get('a2'), end - base);
  c.load(base, rom.subarray(c.get('a0'), c.get('a0') + c.get('a2'))); transfers++;
});
m.trap(0x80070c60, c => { assert.equal(c.get('a0'), end); assert.equal(c.get('a1'), 0x80400000-end); heaps++; });
m.trap(0x80070d20, () => {});
m.set('a0', base); m.set('a1', 0x80400000-base); m.call(header.readUInt32BE(60));
assert.equal(transfers, 1); assert.equal(heaps, 1);
let getters = 0;
for (const id of [1, 277, ...Array.from({length:fixture.records.length-278}, (_,n)=>n+278)]) {
  const expected = Buffer.from(fixture.records[id], 'hex');
  for (const [entry, offset, width] of [[0x45480,27,1],[0x45498,28,1],[0x454c8,4,1],[0x45514,6,1],[0x4552c,8,2]]) {
    m.set('a0', id); m.call((entry + 0x8012a100) >>> 0);
    assert.equal(m.get('v0'), width===2?expected.readUInt16BE(offset):expected[offset], `${entry.toString(16)} item ${id}`); getters++;
  }
  m.set('a0', id); m.call(0x8016f5b0);
  const name = m.get('v0'); assert(name >= header.readUInt32BE(116) && name < end);
  let text = []; for (let at=name; m.r8(at); at++) { assert(at<end); text.push(m.r8(at)); }
  if (fixture.names) assert.equal(Buffer.from(text).toString('hex'), fixture.names[id]);
  // Description routing must pass the complete ID and the relocated resource key.
  m.w32(0x801939dc, id); m.call(0x801804b4, 0x80180544);
  assert.equal(m.get('a2'), id);
  assert.equal(m.get('a1'), rom.readUInt32BE(0x237431c)-0x594280);
}
const last = fixture.records.length-1, payload = 0x80350000, descriptor = 0x80187444;
m.w16(0x80196b00,last); m.w8(0x80196b03,12); m.w16(0x80193bf8+42,last);
m.set('a0',payload); m.set('a1',descriptor); m.call(0x80074494,0x80000100,5000000);
m.w16(0x80196b00,0); m.w16(0x80193bf8+42,0);
m.set('a0',payload); m.set('a1',descriptor); m.call(0x800742f4,0x80000100,5000000);
assert.equal(m.r16(0x80196b00),last); assert.equal(m.r8(0x80196b03),12); assert.equal(m.r16(0x80193bf8+42),last);
console.log(JSON.stringify({getters, loader:'PASS', descriptionRouting:'PASS', originalSaveRoundTrip:'PASS', ramBytes:0x400000}));
