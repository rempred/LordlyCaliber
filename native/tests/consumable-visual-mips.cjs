'use strict';
// Executes newly exported C, including both actual menu activation owners.
const fs = require('fs'), assert = require('assert/strict');
const { Mips } = require('../../tests/lib/mips-interpreter.cjs');
// The map owner also contains camera math in other states. This test executes
// only integer item confirmation, but its prologue saves/restores FP registers.
// Model those memory instructions; leave FP arithmetic unsupported deliberately.
class MenuMips extends Mips {
  constructor() { super(); this.fp = Uint32Array.from({ length:32 }, (_, n) => 0x3f000000 + n); }
  exec(pc, next) {
    const word = this.r32(pc), op = word >>> 26;
    if (![49, 53, 57, 61].includes(op)) return super.exec(pc, next);
    const base = this.r[(word >>> 21) & 31], ft = (word >>> 16) & 31, at = (base + ((word << 16) >> 16)) >>> 0;
    if (op === 49) this.fp[ft] = this.r32(at);
    if (op === 57) this.w32(at, this.fp[ft]);
    if (op === 53 || op === 61) {
      assert.equal(ft & 1, 0, 'FR=0 double register pairs are even');
      if (op === 53) { this.fp[ft + 1] = this.r32(at); this.fp[ft] = this.r32(at + 4); }
      else { this.w32(at, this.fp[ft + 1]); this.w32(at + 4, this.fp[ft]); }
    }
    return [next >>> 0, (next + 4) >>> 0];
  }
}
const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const fixture = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
const nativePixels = Buffer.from(fixture.pixels, 'hex'), nativePalettes = Buffer.from(fixture.palettes, 'hex');
const descriptor = 0x27c5400, offset = rom.readUInt32BE(descriptor + 12), address = rom.readUInt32BE(descriptor + 16), size = rom.readUInt32BE(descriptor + 20);
const data = rom.subarray(offset, offset + size), entry = data.readUInt32BE(24);
assert.ok(entry >= address && entry < address + size);
const model = JSON.parse(data.subarray(data.readUInt32BE(8), data.readUInt32BE(8) + data.readUInt32BE(12)));
const pixels = 0x803a0000, palettes = 0x803a2000, resourcePointers = 0x803a3000, ui = 0x803b0000;
assert.ok(address + size < pixels, 'test buffers do not overlap the runtime');
function cpu() {
  const m = new MenuMips();
  m.load(0x8016af80, rom.subarray(0x40e80, 0x66e10));
  m.load(address, data);
  for (const [at, ram] of [[0x41098, 0x8016b198], [0xc9850, 0x801dcdf0], [0x1a9290, 0x80214f80]]) m.load(ram, rom.subarray(at, at + 8));
  m.set('sp', 0x803ff000);
  m.load(pixels, Buffer.alloc(2872, 0x55)); m.load(palettes, nativePalettes);
  m.flushes = []; m.trap(0x80090140, c => c.flushes.push([c.get('a0'), c.get('a1')]));
  return m;
}
function bytes(m, at, length) { return Buffer.from(Array.from({ length }, (_, n) => m.r8(at + n))); }
function expected(m, id) {
  const visual = model.visuals[id];
  assert.deepEqual(bytes(m, pixels, 2048), visual ? Buffer.from(visual.pixels, 'base64') : nativePixels);
  const palette = Buffer.from(nativePalettes);
  if (visual) { Buffer.from(visual.palette, 'base64').copy(palette, 0); Buffer.from(visual.palette, 'base64').copy(palette, 96); }
  assert.deepEqual(bytes(m, palettes, 128), palette);
  assert.deepEqual(bytes(m, pixels + 2048, 824), Buffer.alloc(824, 0x55), 'result artwork is untouched');
  assert.deepEqual(m.flushes.slice(-2), [[pixels, 2048], [palettes, 128]], 'updated textures are visible to the graphics hardware');
}
{
  const m = cpu();
  for (const id of [2, 3, 1, 2, 8]) {
    m.set('a0', id); m.set('a1', pixels); m.set('a2', palettes); m.call(entry);
    expected(m, id);
  }
  assert.deepEqual(bytes(m, address, size), data, 'assigned visual data remains independent and immutable');
}
// Army Management: confirm a single-target or squad item. Calls the full generated
// C9850 owner, while unrelated sound/UI routines are isolated as traps.
for (const group of [false, true]) {
  const m = cpu(), character = 0x80193bc0 + 56, inventory = 0x80193ac0 + 4;
  const id = group ? 3 : 2;
  m.w32(0x80196af8, ui); m.w16(ui + 0x122, group ? 4 : 3); m.w16(ui + 0x1bda, 0); m.w16(ui + 0x1bee, 1);
  m.w16(ui + 0x18a, 1); m.w16(0x800e8100, 0x8000); m.w32(ui + 0x1e28, resourcePointers);
  m.w32(resourcePointers, pixels); m.w32(resourcePointers + 4, palettes);
  m.load(ui + 0x1180, Buffer.from([1,255,255,255,255,255,255,255,255]));
  m.w16(inventory, id); m.w8(inventory + 3, 5); m.w16(character + 0x16, 500); m.w16(character + 0x18, 100);
  const owners = JSON.parse(fs.readFileSync(require('path').join(__dirname, '../game/consumables/tree-owners.json'))).owners;
  const owner = owners.find(o => /000c9850/i.test(o.name));
  for (const name of ['func_000bc7e4', 'func_000bc828', 'func_000c54c0']) m.trap(owner.bindings[name], () => {});
  m.call(0x801dcdf0); expected(m, id);
  assert.equal(m.r16(ui + 0x122), 5); if (!group) assert.equal(m.r32(ui + 0x24fc), 300);
  assert.equal(m.r16(character + 0x18), 100, 'visual selection does not prematurely apply healing');
  assert.equal(m.r8(inventory + 3), 4, 'one item is consumed');
  assert.equal(m.r32(resourcePointers), pixels); assert.equal(m.r32(resourcePointers + 4), palettes, 'heap ownership is retained');
}
// Unit menu on the mission map: native confirmation and effect preparation use
// the same item artwork, queue the native amount and preserve character state.
{
  const m = cpu(), item = 0x803a3100, target = item + 2, character = 0x80193bc0 + 56;
  m.w32(0x8021e194, 0x100); m.w16(item, 2); m.w8(target, 1);
  m.w8(0x8019ee40, 0); m.w8(0x8021e3c3, 0); m.w16(0x8021e3b4, 0);
  m.w32(0x80214a6c, pixels); m.w32(0x80214eac, palettes);
  m.w16(character + 0x16, 500); m.w16(character + 0x18, 100);
  m.trap(0x801e027c, c => c.set('v0', 1)); m.trap(0x8020cda0, () => {});
  const queued = []; m.trap(0x8021dc08, c => queued.push([c.get('a0'), c.get('a2')]));
  m.set('a0', 0); m.set('a1', 0); m.set('a2', item); m.set('a3', target); m.call(0x80214f80);
  assert.deepEqual(m.fp, Uint32Array.from({ length:32 }, (_, n) => 0x3f000000 + n), 'callee restores FP registers');
  expected(m, 2); assert.deepEqual(queued, [[2, 300]]);
  assert.equal(m.r32(0x8021e194), 0x8000); assert.equal(m.r16(character + 0x18), 100);
}
console.log('Item visuals: independent copies, native restoration, icon preservation, cache writeback, Army confirmation and map preparation passed.');
