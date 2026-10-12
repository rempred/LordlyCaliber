'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();const limit=Number(process.argv[3]);
const ram=o=>(o+0x800ab8c0)>>>0;let cases=0;
function cpu(count,legion=false){
 const m=new Mips();m.load(ram(0x10d1e8),rom.subarray(0x10d1e8,0x10d450));m.load(ram(0x12013c),rom.subarray(0x12013c,0x120280));
 m.set('sp',0x803fff00);m.w32(0x801f367c,50);
 for(let i=0;i<50;i++){const p=0x80280000+i*192;m.w32(0x801f0cb0+i*4,p);m.w8(p+4,i);
  m.w32(p,i<count?0x39|(legion?0x40:0):i<30?0x38:0x31);}
 m.trap(0x801f69fc,()=>{});m.trap(0x801dd244,c=>c.set('v0',1));return m;
}
for(let count=0;count<=30;count++)for(const legion of [false,true]) {
 const m=cpu(count,legion);m.call(ram(0x10d1e8));assert.equal(m.get('v0'),Math.max(0,limit-count));assert.equal(m.get('sp'),0x803fff00);
 const opened=[];m.trap(0x801b8780,c=>c.set('v0',0x80300000));m.trap(0x801dc910,()=>{});
 m.trap(0x80205dc8,()=>opened.push(true));m.trap(0x8020cda0,()=>{});m.trap(0x800712c4,()=>{});
 m.call(ram(0x10d2b0));assert.equal(opened.length,count<limit?1:0);assert.equal(m.get('sp'),0x803fff00);cases++;
 // The Legion confirmation also checks the total number of constituent squads.
 for(let group=1;group<=5;group++) {
  const c=cpu(count,true),strings=[];let allowed=false;
  c.set('s0',0x80280000);c.w32(0x80280000,c.r32(0x80280000)|0x40);
  c.trap(0x801da1ac,x=>x.set('v0',group));c.trap(0x8020cda0,()=>{});
  c.trap(0x801d9474,x=>strings.push(x.get('a0')));
  c.trap(ram(0x1201e8),x=>{allowed=true;x.set('ra',0x80000100);});
  c.trap(ram(0x120264),x=>x.set('ra',0x80000100));
  c.call(ram(0x12013c));const remaining=Math.max(0,limit-count);
  assert.equal(allowed,remaining>=group);
  if(!allowed)assert.deepEqual(strings,[group,limit-remaining,remaining]);cases++;
 }
}
console.log(`PASS ${cases} deployment cases at limit ${limit}: capacity, full/overfull maps, enemy exclusion, individual Legion squads, menu entry and rejection-message counts.`);
