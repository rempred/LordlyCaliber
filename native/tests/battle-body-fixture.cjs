'use strict';
// Native battle body resource registration. The fixture handles ROM transfer,
// decompression and allocation. Game code chooses and advances each pose.
const fs = require('node:fs'), path = require('node:path'), vm = require('node:vm');
const context = vm.createContext({ Uint8Array, Uint16Array, Uint32Array, Int8Array, DataView, ArrayBuffer });
context.window = context;
context.OB64 = {};
vm.runInContext(fs.readFileSync(path.resolve(__dirname, '../../art.js'), 'utf8'), context);

class BattleBodyFixture {
 constructor(m, rom) {
  this.m = m;
  this.pool = 0x80500000;
  this.heap = 0x80510000;
  this.allocations = new Map();
  this.freed = [];
  const art = context.OB64.art;
  const raw = key => Buffer.from(art.readResource(rom, key).stored);
  const cache = new Map();
  const decode = key => {
   if (!cache.has(key)) cache.set(key, Buffer.from(art.readCompressedResource(rom, key).decoded));
   return cache.get(key);
  };
  m.w32(0x801d0728, this.pool);
  for (const pc of [0x80071c04, 0x80070f30]) m.trap(pc, c => c.set('v0', this.allocate(c.get('a0'))));
  m.trap(0x8009daf4, c => c.set('v0', raw(c.get('a0')).length));
  m.trap(0x8009dbb8, c => c.load(c.get('a0'), raw(c.get('a1'))));
  for (const pc of [0x8009dd38, 0x8009df48]) m.trap(pc, c => {
   const bytes = decode(c.get('a0')), p = this.allocate(bytes.length);
   c.load(p, bytes);
   c.set('v0', p);
  });
  m.trap(0x800712c4, c => {
   const p = c.get('a0'), size = this.allocations.get(p);
   if (size) {
    this.allocations.delete(p);
    this.freed.push({ p, size });
   }
   c.set('v0', 0);
  });
  m.trap(0x80093380, c => c.load(c.get('a0'), Buffer.alloc(c.get('a1'))));
  m.trap(0x8009c970, c => {
   c.load(c.get('a0'), Buffer.alloc(c.get('a2'), c.get('a1') & 255));
   c.set('v0', c.get('a0'));
  });
  m.trap(0x80093060, c => {
   const from = c.phys(c.get('a0'));
   c.load(c.get('a1'), c.ram.slice(from, from + c.get('a2')));
  });
  // Sound output and 3D-to-audio positioning do not control pose advancement.
  m.trap(0x801c5938, () => {});
  m.trap(0x801c9028, c => { c.w32(c.get('a3'), 0); c.set('v0', 0); });
 }
 allocate(bytes) {
  const size = Math.max(16, (bytes + 15) & ~15);
  if (bytes > 0x100000) throw Error('Invalid battle allocation size');
  const reuse = this.freed.findIndex(block => block.size >= size);
  let p;
  if (reuse >= 0) {
   const block = this.freed[reuse];
   p = block.p;
   if (block.size === size) this.freed.splice(reuse, 1);
   else { block.p += size; block.size -= size; }
  } else {
   p = this.heap;
   this.heap += size;
   if (this.heap >= 0x80600000) throw Error('Battle allocation exceeded its fixture area');
  }
  this.allocations.set(p, size);
  return p;
 }
 reset(call) {
  this.heap = 0x80510000;
  this.allocations.clear();
  this.freed.length = 0;
  this.m.load(this.pool - 16, Buffer.alloc(20 * 176 + 32));
  this.m.w8(0x80193c12, 3);
  call(0x801be71c, []);
 }
 register(call, index, actor, identity, current, flags) {
  const m = this.m, entry = this.pool + index * 176;
  call(0x801c27f8, [entry, identity, current, (flags >> 10) & 1, (flags >> 8) & 1, 0]);
  m.w32(actor + 0x50, m.r32(entry));
  const child = m.r32(actor);
  m.w32(child + 0x90, child);
  call(0x801ad9d4, [child + 0x44, 0]);
 }
}
module.exports = { BattleBodyFixture };
