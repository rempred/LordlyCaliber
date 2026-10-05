'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
// Complete functions and constant block match retained opening RAM. Only
// allocation/free are controlled; all pan, zoom, query and lifecycle code runs.
m.load(0x8023f1e4,rom.subarray(0x2b3fe4,0x2b4104));
m.load(0x8023ff58,rom.subarray(0x2b4d58,0x2b5044));
m.load(0x80243cc8,rom.subarray(0x2b8ac8,0x2b8ae8));
const context=0x80300010,root=0x80310000,channel=0x80320010,job=0x80321010,sp=0x803ffe00;
m.w32(0x8022a970,context);m.w32(0x8022a974,root);m.w32(root+0x1c58,channel);
let allocations=0,frees=0;
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),16);allocations++;c.set('v0',job);});
m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),job);frees++;});
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const call=(p,args=[])=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p);assert.equal(m.get('sp'),sp);return m.get('v0');};
const snap=()=>({context:read(context,0x848),channel:read(channel,88),job:m.r32(root+0x1a4c)?read(job,16):'',query:call(0x802401a0)});
const rows=[];
for(const state of [0,1,2,3,4,9])for(const reverse of [0,1])for(const duration of [1,4,30,-1])for(const origin of [[-1,-1],[0,0],[-1,0],[17,-8],[19,-6],[2147483000,-2147483000]]) {
 m.load(context-16,Buffer.alloc(0x848+32,0xa7));m.load(channel-16,Buffer.alloc(88+32,0xa7));m.load(job-16,Buffer.alloc(48,0xa7));
 m.w32(root+0x1a4c,0);m.w8(context+0x83c,state);m.w32(context+0x828,2);m.w32(context+0x830,7);
 m.writeFloat(channel+0x40,12.25);m.writeFloat(channel+0x44,-4.5);m.writeFloat(channel+0x54,.875);
 allocations=frees=0;
 const initial=snap(),args=[...origin,0x12345678,18,-7,reverse,duration],steps=[];
 call(0x8023ff58,args);steps.push({start:args,after:snap()});
 const updates=duration>0?duration+3:5;
 for(let i=0;i<updates;i++){call(0x802401b4);steps.push({after:snap()});}
 // A fresh start, then in-place reversal while the job remains active.
 const retarget=[-1,-1,0,3,5,reverse,8];call(0x8023ff58,retarget);steps.push({start:retarget,after:snap()});
 call(0x802401b4);steps.push({after:snap()});
 const beforeAllocations=allocations,back=[-1,-1,99,-4,9,1-reverse,3];call(0x8023ff58,back);steps.push({start:back,after:snap()});assert.equal(allocations,beforeAllocations);
 for(let i=0;i<5;i++){call(0x802401b4);steps.push({after:snap()});}
 for(const [p,n]of [[context,0x848],[channel,88],[job,16]])for(let i=0;i<16;i++){assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+n+i),0xa7);}
 rows.push({initial,steps});
}
// Counter wraparound and equality release, including a zero-duration idle zoom.
const boundaries=[];
for(const [duration,elapsed]of [[0xffffffff,0xfffffffe],[0xffffffff,0xffffffff],[0,0],[1,0xffffffff],[0x80000000,0x7fffffff]]) {
 m.load(context,Buffer.alloc(0x848));m.load(channel,Buffer.alloc(88));m.w8(context+0x83c,4);m.writeFloat(channel+0x54,1);
 m.w32(root+0x1a4c,job);m.writeFloat(job,1.5);m.writeFloat(job+4,-2.25);m.w32(job+8,duration);m.w32(job+12,elapsed);
 const initial=snap(),steps=[];for(let i=0;i<3;i++){call(0x802401b4);steps.push(snap());}boundaries.push({initial,steps});
}
fs.writeFileSync(process.argv[3],JSON.stringify({rows,boundaries}));
console.log(`${rows.length} original pan/zoom sequences with retargeting, plus ${boundaries.length} counter boundary sequences; allocation/free controlled.`);
