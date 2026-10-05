"use strict";
const fs=require("node:fs"), assert=require("node:assert/strict"), {FloatMips}=require("./float-mips.cjs");
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80089540,rom.subarray(0x19940,0x19a60));
m.load(0x80089a10,rom.subarray(0x19e10,0x19e30));
m.load(0x80089bc0,rom.subarray(0x19fc0,0x1a060));
m.load(0x80095b30,rom.subarray(0x25f30,0x25f90));
const request=0x800c4bd8,count=0x800e79a4,vi=0x80302000,sp=0x803ffe00,msg=0x80303000,type=0x80303100;
m.w32(0x80000300,1); // US NTSC route, no PAL scale change.
m.w32(0x800abbd4,vi);
m.trap(0x800996d0,c=>c.set("v0",1));
m.trap(0x80099740,c=>assert.equal(c.get("a0"),1));
m.trap(0x8008b820,c=>c.set("v0",0));
const call=(pc,stop=0x80000100,limit=20000)=>{m.set("sp",sp);m.call(pc,stop,limit);assert.equal(m.get("sp"),sp);};
const snapshot=()=>{assert.equal(m.r16(vi)&~0x20,0x5510);return {request:m.r32(request),outstanding:m.r32(count),black:!!(m.r16(vi)&0x20)};};
const rows=[];
for(const pending of [0,1,128,129,255,0xffffffff])for(const black of [false,true])for(const tasks of [0,1,3]){
 m.w32(request,pending);m.w32(count,tasks);m.w16(vi,0x5510|(black?0x20:0));
 const initial=snapshot(),steps=[];
 const action=(kind,event=0)=>{
  let accepted=true;
  if(kind==='black'){
   if(m.r32(count)){assert.throws(()=>call(0x80089a10,0x80000100,50),/step limit/);accepted=false;}
   else {call(0x80089a10);call(0x80089bc0);}
  }else if(kind==='request')call(0x80089c50);
  else if(kind==='submit')m.w32(count,m.r32(count)+1);
  else if(kind==='complete'){
   m.w32(sp+16,msg);m.w32(msg+84,type);m.w16(type,event);
   call(0x80089574,0x80089560);
  }
  steps.push({kind,event,accepted,...snapshot()});
 };
 action('black');
 while(m.r32(count))action('complete',8);
 action('black');action('request');action('idle');action('complete',2);
 action('submit');action('complete',16);action('submit');action('complete',4);
 action('request');action('black');action('submit');action('complete',4);
 rows.push({initial,steps});
}
fs.writeFileSync(process.argv[3],JSON.stringify({rows}));
console.log(`${rows.length} original display-control sequences; queue receipt, interrupt masks and completion callback controlled; drain, request, completion routing and VI black-bit mutation execute.`);
