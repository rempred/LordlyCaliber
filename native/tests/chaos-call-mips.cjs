'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const catalog=require('../game/tools/chaos-call.json'),owner=catalog.active;
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const clobber=c=>{for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(r,0xcccccccc);};
let cases=0;
for(const flags of [0,1])for(const selectedArmy of [false,true])for(const taskSize of [0,0x100,0x1200]){
 const run=legacy=>{
  const m=new Mips(),events=[],task=0x80300000,s=owner.symbols;
  const code=Buffer.from(rom.subarray(owner.start,owner.start+owner.size));
  if(legacy)code.writeUInt32BE((0x08000000|((0x800a4a78>>>2)&0x3ffffff))>>>0,0x23f7c-owner.start);
  m.load(owner.address,code);m.load(0x800a4a78,rom.subarray(0x34e78,0x34e78+136));
  m.load(0x80420000,rom.subarray(0x27c1000,0x27c1a00));
  m.w32(0x80196af8,selectedArmy?0x801f1550:0);m.w32(task+4,flags);
  for(const off of [8,16,24,32,40,44,48,56])m.w32(task+off,0x80220000+off*16);
  m.w32(task+12,0x100);m.w32(task+28,0x80);m.w32(task+52,taskSize);m.w32(task+60,0x40);
  m.trap(s.bcopy,c=>{assert.equal(c.get('a2'),64);const src=c.get('a0'),dst=c.get('a1');c.load(dst,c.ram.slice(c.phys(src),c.phys(src)+64));clobber(c);});
  m.trap(s.osVirtualToPhysical,c=>{const a=c.get('a0')&0x1fffffff;clobber(c);c.set('v0',a);});
  m.trap(0x80090140,c=>{events.push(['writeback',c.get('a0'),c.get('a1')]);clobber(c);});
  for(const name of ['__osSpSetStatus','__osSpSetPc','__osSpRawStartDma','__osSpDeviceBusy'])m.trap(s[name],c=>{
   events.push([name,c.get('a0'),c.get('a1'),c.get('a2'),c.get('a3')]);clobber(c);c.set('v0',0);
  });
  m.set('sp',0x803ffe00);for(let i=0;i<8;i++)m.set('s'+i,0x12340000+i);
  m.set('a0',task);m.call(owner.address);assert.equal(m.get('sp'),0x803ffe00);
  for(let i=0;i<8;i++)assert.equal(m.get('s'+i),0x12340000+i);
  assert(events.some(e=>e[0]==='writeback'&&e[1]===s.D_800C3560&&e[2]===64));
  // Only each service's declared argument registers matter; spare caller-saved
  // values are deliberately poisoned and are not part of its ABI.
  const arity={writeback:2,__osSpSetStatus:1,__osSpSetPc:1,__osSpRawStartDma:4,__osSpDeviceBusy:0};
  return {events:events.map(e=>e.slice(0,arity[e[0]]+1)),task:Buffer.from(m.ram.slice(m.phys(task),m.phys(task)+64)),
   submitted:Buffer.from(m.ram.slice(m.phys(s.D_800C3560),m.phys(s.D_800C3560)+64))};
 };
 assert.deepEqual(run(false),run(true));cases++;
}
console.log(`PASS ${cases} complete task submissions: C call versus legacy hook, service arguments, flags, stack and callee-saved registers.`);
