'use strict';
// Execute exported clock and cycle instructions, with native service calls trapped.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
function rom(p){const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const edited=rom(process.argv[2]),original=rom(process.argv[3]);
let clocks=0,cycles=0;
for(const [image,day,night,period,rest] of [[original,60,60,180,180],[edited,90,30,90,360]]) {
 const m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(0x8016c81c,image.subarray(0x4271c,0x42800));
 const clock=0x80196a2c,sp=0x803fff00;let dates=0;
 m.trap(0x8016c5bc,c=>{assert.equal(c.get('a0'),clock+4);dates++;});
 for(const hour of [0,5,6,17,18,23]) for(const minute of [0,59]) for(const speed of [0,1,4]) {
  const length=hour>=6&&hour<18?day:night;
  for(const second of [0,length-1])for(const sub of [0,3]) {
   m.r.fill(0);m.set('sp',sp);m.set('s0',0xabcdef01);m.w16(clock,hour);m.w8(clock+2,minute);
   m.w8(clock+3,second);m.w8(clock+8,speed);m.w8(clock+9,sub);m.w8(clock+10,0xa5);dates=0;
   let h=hour,min=minute,sec=second,tick=sub+1,date=0;
   if(tick>=speed){tick=0;if(++sec>=length){sec=0;if(++min>=60){min=0;if(++h>=24){h=0;date=1;}}}}
   m.call(0x8016c81c);
   assert.equal(m.r16(clock),h);assert.equal(m.r8(clock+2),min);assert.equal(m.r8(clock+3),sec);
   assert.equal(m.r8(clock+9),tick);assert.equal(dates,date);assert.equal(m.r8(clock+10),0xa5);
   assert.equal(m.get('sp'),sp);assert.equal(m.get('s0'),0xabcdef01);clocks++;
  }
 }
 const ram=n=>(n+0x800ab8c0)>>>0,unit=0x803e0000;
 m.load(ram(0x133700),image.subarray(0x133700,0x133a8c));
 for(const hour of [5,6,17,18])for(const hasItem of [0,1])for(const phase of [0,1]) {
  const nightTime=hour<6||hour>=18;
  for(const before of [0,1,2,3,6,period-3,period-1,period,period+1]) {
   m.r.fill(0);m.set('sp',sp);m.set('a0',unit);m.w32(unit,0x40001);m.w8(unit+4,0);
   m.w32(unit+0x70,phase);m.w16(unit+0xb6,before);m.w8(0x801f0e0a,hour);m.w32(0x801f0de0,0);
   let gains=0;m.trap(0x801dcd40,c=>{assert.equal(c.get('a1'),26);c.set('v0',hasItem);});
   m.trap(0x801df34c,c=>{assert.equal(c.get('a0'),unit);gains++;c.set('v0',40);});
   let step=phase===1?3:2;if(hasItem)step>>=1;
   let counter=before+step,expected=0;if(counter>period){counter-=period;expected++;}
   if(nightTime&&++counter>period){counter-=period;expected++;}
   m.call(ram(0x133944));assert.equal(m.r16(unit+0xb6),counter);assert.equal(gains,expected);
   assert.equal(m.get('sp'),sp);cycles++;
  }
 }
 // Enter the unchanged eligibility gate's accepted rest-cycle body.
 for(const hour of [5,6,17,18])for(const hasItem of [0,1])for(const before of [0,1,2,3,6,rest-1,rest]) {
  m.r.fill(0);m.set('sp',sp);m.set('s0',unit);m.set('s2',0xffffffff);
  m.w16(unit+0xb6,before);m.w8(0x801f0e0a,hour);let healed=0;
  m.trap(0x801dcd40,c=>{assert.equal(c.get('a1'),27);c.set('v0',hasItem);});
  m.trap(0x801df4a0,c=>{assert.equal(c.get('a0'),unit);healed++;c.set('v0',20);});
  let counter=before,expected=0,step=hasItem?6:3;
  if(counter<step){counter=rest;expected++;}else counter-=step;
  if(hour<6||hour>=18){if(counter<2){counter=rest;expected++;}else counter-=2;}
  m.call(ram(0x133884),ram(0x13392c));assert.equal(m.r16(unit+0xb6),counter);assert.equal(healed,expected);cycles++;
 }
}
assert.deepEqual([...edited.subarray(0x239cd0,0x239cd4)].map(x=>(x<<24)>>24),[-5,2,5,-2]);
console.log(`PASS ${clocks} clock boundary cases and ${cycles} movement/rest cycle cases, retail and exported code; tactic signed values verified. Whole-console timing and calendar helper internals are outside this check.`);
