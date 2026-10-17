'use strict';
// Compare portable library adaptations against original instructions and data.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');
const {loadGraphicsMath} = require('./graphics-math-reference.cjs');
const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const cpu = new FloatMips(), sp = 0x803ffe00, out = 0x80300000;
loadGraphicsMath(cpu, rom);
for (const [ram, start, end] of [[0x80090940, 0x20d40, 0x210c0],
  [0x80092320, 0x22720, 0x228d0], [0x80092a90, 0x22e90, 0x23018],
  [0x800987e0, 0x28be0, 0x28ca0], [0x800aa750, 0x3ab50, 0x3ab54]])
  cpu.load(ram, rom.subarray(start, end));
const bits = v => { const b = Buffer.alloc(4); b.writeFloatBE(v); return b.readUInt32BE(); };
const matrix = () => Buffer.from(cpu.ram.subarray(cpu.phys(out), cpu.phys(out) + 64)).toString('hex');
const trig = [], cameras = [], rotations = [];
const inputs = [0, -0, 1e-8, -1e-8, 0.5, -0.5, 1, -1, 1.5707964, -1.5707964,
  3.1415927, 6.2831855, 1000, -1000, 1e8, -1e8, 1e10, -1e10];
let seed = 17, hostDifferences = 0;
for (let i = 0; i < 2048; ++i) {
  seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
  inputs.push(Math.fround(((seed / 0x100000000) * 2 - 1) * 8 * Math.PI));
}
for (const input of inputs) {
  const x = Math.fround(input), row = {input: bits(x)};
  for (const [name, address, host] of [['sin', 0x80092db0, Math.sin], ['cos', 0x800907f0, Math.cos]]) {
    cpu.set('sp', sp); cpu.setSingle(12, x); cpu.call(address);
    assert.equal(cpu.get('sp'), sp);
    row[name] = cpu.fp[0];
    if (row[name] !== bits(host(x))) hostDifferences++;
  }
  trig.push(row);
}
for (const eye of [[0, 0, 120], [31, 83, 301], [-300.25, 42.7, -10.3]])
for (const up of [[0, 1, 0], [0.2, 3, -0.4]])
for (const fov of [30, 55, 90, 135])
for (const aspect of [4 / 3, 1.1, 2]) {
  const bank = [fov, aspect, 0.125, 1000, 0.75, ...eye, 3, -2, 7, ...up].map(Math.fround);
  cpu.set('sp', sp); cpu.set('a0', out);
  bank.slice(5).forEach((v, i) => i < 3 ? cpu.set(['a1', 'a2', 'a3'][i], bits(v)) : cpu.w32(sp + 16 + (i - 3) * 4, bits(v)));
  cpu.call(0x80090940); assert.equal(cpu.get('sp'), sp);
  const view = matrix();
  cpu.set('a0', out); cpu.set('a1', 0); cpu.set('a2', bits(fov)); cpu.set('a3', bits(bank[1]));
  cpu.w32(sp + 16, bits(bank[2])); cpu.w32(sp + 20, bits(bank[3])); cpu.w32(sp + 24, bits(bank[4]));
  cpu.call(0x80092320); assert.equal(cpu.get('sp'), sp);
  cameras.push({bank, view, projection: matrix()});
}
for (const x of [-179.5, -30, 0, 20, 90, 359.9])
for (const y of [-150.3, 0, 75.25]) for (const z of [-40.7, 0, 120]) {
  cpu.set('sp', sp); cpu.set('a0', out);
  [x, y, z].forEach((v, i) => cpu.set(['a1', 'a2', 'a3'][i], bits(v)));
  cpu.call(0x80092a90); assert.equal(cpu.get('sp'), sp);
  rotations.push({angles: [x, y, z], matrix: matrix()});
}
fs.writeFileSync(process.argv[3], JSON.stringify({trig, cameras, rotations, hostDifferences,
  radians: rom.readFloatBE(0x3ab50)}));
console.log(`${trig.length} original trig inputs; ${cameras.length} camera pairs; ${rotations.length} rotations; ${hostDifferences} host-libm differences.`);
