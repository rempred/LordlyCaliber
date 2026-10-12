'use strict';
// Actual exported condition bytes through the original descriptor-7 predicates.
// No condition-result stubs; simulated units/counts are the controlled inputs.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const f=JSON.parse(fs.readFileSync(process.argv[2]));
const rom=fs.readFileSync(f.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const d=0x387c0+7*40,base=rom.readUInt32BE(d),start=rom.readUInt32BE(d+8),end=rom.readUInt32BE(d+12);
assert.equal(base,0x801ad5a0);assert.equal(start,0x101ce0);
const live=at=>(at+base-start)>>>0,unit=0x80300000;
function setup(){
 const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(base,rom.subarray(start,end));m.set('sp',0x803ffe00);
 f.conditions.forEach((c,i)=>m.load(0x801f0e19+10*i,Buffer.from(c,'hex')));
 m.w8(0x801f0e18,f.conditions.length);return m;
}
function evaluate(m,id){m.set('a0',id);m.set('a1',unit);m.call(live(0x123458));assert.equal(m.get('sp'),0x803ffe00);return m.get('v0');}
const area=Buffer.from(f.conditions[f.area-1],'hex');assert.equal(area[1],16);
const [left,top,right,bottom]=[...area.subarray(2,6)];assert(left>0 && top>0 && right<255 && bottom<255);
let checks=0;
for(const [x,z,expected] of [[left,top,1],[right,bottom,1],[(left+right)/2,(top+bottom)/2,1],
 [left-1,top,0],[right+1,bottom,0],[left,top-1,0],[right,bottom+1,0]]) {
 for(const bounds of [[0,0,256,256],[-300,90,724,602]]) {
  const m=setup();bounds.forEach((v,i)=>m.writeFloat(0x801f0d98+i*4,v));
  m.writeFloat(unit+8,bounds[0]+x*(bounds[2]-bounds[0])/256);
  m.writeFloat(unit+16,bounds[1]+z*(bounds[3]-bounds[1])/256);
  assert.equal(evaluate(m,f.area),expected,`area ${x},${z}`);assert.equal(evaluate(m,f.area),expected,'area is not consumed');checks+=2;
 }
}
const count=Buffer.from(f.conditions[f.count-1],'hex');assert.equal(count[1],9);const limit=count[6];
for(const active of [0,1,limit,limit+1,30]) {
 const m=setup();
 // The scheduler amortizes its unit scan. Start at its transfer phase, bypassing
 // unrelated unit update/setter services without replacing the threshold logic.
 m.w8(0x801e8680,50);m.w16(0x801f0eba,0xffff);
 m.set('a0',active);m.call(live(0x10adb8));
 const expected=active>0 && active<=limit?1:0;
 assert.equal(evaluate(m,f.count),expected,`active count ${active}`);checks++;
 assert.equal(evaluate(m,f.count),0,'successful count trigger is consumed');checks++;
 // Next scheduler cycle rearms it, or clears it when the count no longer fits.
 m.w8(0x801e8680,50);m.set('a0',active);m.call(live(0x10adb8));
 assert.equal(evaluate(m,f.count),expected);checks++;
}
assert(f.nodes && f.first && f.next,'Exported route fixture required');
for(const gate of [1,2,3]) for(const inArea of [false,true]) for(const belowLimit of [false,true]) {
 const m=setup();f.nodes.forEach((n,i)=>m.load(0x801f0ebe+18*i,Buffer.from(n,'hex')));
 const node=Buffer.from(f.nodes[f.first-4],'hex');
 assert.equal(node[10],f.area);assert.equal(node[12],f.count);assert.equal(node[17],f.next);
 node[11]=gate;node[16]=1;m.load(0x801f0ebe+18*(f.first-4),node);
 [0,0,256,256].forEach((v,i)=>m.writeFloat(0x801f0d98+i*4,v));
 m.writeFloat(unit+8,inArea?left:left-1);m.writeFloat(unit+16,top);m.w32(unit,0x11);m.w8(unit+0xba,f.first);
 m.w8(0x801e8680,50);m.set('a0',belowLimit?limit:limit+1);m.call(live(0x10adb8));
 m.set('a0',unit);m.call(live(0x1237f0));
 const expected=gate===1?(inArea?1:belowLimit?f.next:0):(gate===2?(inArea&&belowLimit):(inArea||belowLimit))?f.next:0;
 assert.equal(m.get('v0'),expected,`resolver gate ${gate}, area=${inArea}, count=${belowLimit}`);checks++;
}
console.log(`PASS ${checks} original trigger cases: inclusive areas, normalized bounds, repeated area checks, count limits, zero exclusion, consumption/rearming and real area/count AND/OR/branch resolution. Controlled runtime inputs; no full mission/emulator replay.`);
