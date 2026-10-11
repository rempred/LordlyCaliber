'use strict';
// Boot DMA, cache service arguments and heap ownership from exported ROMs.
// Hardware cache effects and full-game memory pressure are outside this check.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const fixtures = JSON.parse(fs.readFileSync(process.argv[2]));
function clobber(c) {
  for (const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])
    c.set(r, 0xdeadbeef);
}
for (const f of fixtures) {
  const rom = fs.readFileSync(f.rom);
  if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
  assert.equal(rom.readUInt32BE(0), 0x80371240);
  const h = rom.subarray(0x5dc80, 0x5dd00), field = at => h.readUInt32BE(at);
  assert.equal(h.toString('ascii', 0, 8), 'LC05CLAS'); assert.equal(field(8), 4);
  const begin = field(104), end = begin + field(108), state = f.state;
  assert.equal(begin, f.expandedSaves ? ((0x80187f00 + field(84) + 15) & 0xfffffff0) >>> 0 : 0x80187f00);
  assert(end >= state + 144); assert(end <= 0x8018aa70);
  const m = new Mips(); m.ram = new Uint8Array(0x400000); m.ram.fill(0xa5);
  m.load(0x80071eb0, rom.subarray(0x22b0, 0x2798)); m.set('sp', 0x803ffe00);
  const cache = [], dma = [], heaps = [], pools = []; let secondary = 0;
  m.trap(0x800900c0, c => {cache.push(['instruction',c.get('a0'),c.get('a1')]);clobber(c);});
  m.trap(0x80090010, c => {cache.push(['data',c.get('a0'),c.get('a1')]);clobber(c);});
  m.trap(0x8009da50, c => {
    const a=c.get('a0'), b=c.get('a1'), n=c.get('a2');
    assert(a+n <= rom.length); assert(b >= 0x80000000 && b+n <= 0x80400000);
    dma.push([a,b,n]); c.load(b,rom.subarray(a,a+n)); clobber(c);
  });
  // Original resident loader setup and calls, excluding unrelated decompression.
  m.call(0x80071ff4, 0x80072024);
  m.call(0x80072034, 0x80072078);
  assert.deepEqual(cache,[['instruction',0x8016af80,end-0x8016af80],
                         ['data',0x80186310,0x80190f10-0x80186310]]);
  assert.deepEqual(dma,[[0x40e80,0x8016af80,0x66e10-0x40e80]]);
  const read = (a,n) => Buffer.from(m.ram.slice(m.phys(a),m.phys(a)+n));
  assert.deepEqual(read(begin,field(108)),rom.subarray(begin-0x8012a100,end-0x8012a100));
  assert.deepEqual(read(state,144),Buffer.alloc(144));
  const stateBytes=Buffer.alloc(144,0x57); stateBytes.writeUInt32BE(1,4); m.load(state,stateBytes);
  m.trap(0x80070c60,c=>{heaps.push([c.get('a0'),c.get('a1')]);clobber(c);});
  m.trap(0x80070d20,c=>{pools.push([c.get('a0'),c.get('a1')]);clobber(c);});
  m.trap(0x80070f14,c=>{secondary++;clobber(c);});
  const call = (entry, start = 0x80243db0) => {
    m.set('a0',start);m.set('a1',0x80400000-start);m.set('sp',0x803ffe00);
    ['s0','s1','s2','s3','s4','s5','s6','s7','fp'].forEach((r,i)=>m.set(r,0xaaaa0000+i));
    m.call(entry);
    assert.equal(m.get('sp'),0x803ffe00);
    ['s0','s1','s2','s3','s4','s5','s6','s7','fp'].forEach((r,i)=>assert.equal(m.get(r),0xaaaa0000+i));
  };
  for (const entry of [field(60), field(64), field(68)]) {
    pools.length=0;call(entry);
    const expected=[[end,0x8018aa70-end],[0x8018ece0,0x3d0],[0x8018fec0,0x4b0]].filter(p=>p[1]);
    assert.deepEqual(pools,expected);
    for(const [a,n] of pools) assert(a+n<=begin || a>=end);
    assert.deepEqual(read(state,144),stateBytes);
  }
  assert.equal(secondary,1);
  assert.deepEqual(heaps,Array(2).fill([field(48),0x80400000-field(48)]));
  assert.deepEqual(dma.slice(1),[[field(12),0x80243db0,field(16)]]);
  assert.deepEqual(read(0x80243db0,field(16)),rom.subarray(field(12),field(12)+field(16)));
  // The title/new-game reset uses a lower start, unlike the ordinary reset.
  // Both live heap spans must exclude the persistent data, with no lost lower RAM.
  pools.length=0; call(field(64),0x801ba710);
  assert.deepEqual(heaps.at(-1),[field(48),0x80400000-field(48)]);
  assert.deepEqual(pools.at(-1),[0x801ba710,0x80243db0-0x801ba710]);
  for(const [a,n] of [...pools,heaps.at(-1)])
    assert(a+n<=0x80243db0 || a>=field(48), 'heap exposes persistent class data');
  call(f.entries.lc_custom_message);assert.equal(m.get('v0'),state+16+68);
  m.w32(state+4,0);call(f.entries.lc_custom_message);assert.equal(m.get('v0'),0);
}
console.log(`PASS ${fixtures.length} resident allocations: original loader DMA/cache arguments, compiled C entry, boot and both heap resets, retained runtime state, guarded 4 MiB RAM. Hardware cache behavior and full-game pressure are not simulated.`);
