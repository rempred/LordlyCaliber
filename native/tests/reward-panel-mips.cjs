'use strict';
// Original preset34 constructor, shared controller and entity animation.
// Icon resource decoding is a controlled opaque handle: drawing is not executed.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80236920,rom.subarray(0x2a6150,0x2a7010));
m.load(0x8017bfb0,rom.subarray(0x51eb0,0x5aff0));
m.load(0x802276f8,rom.subarray(0x2836c8,0x283864));
m.load(0x802393d0,rom.subarray(0x2a8c00,0x2a8d20));
m.load(0x8018fe10,rom.subarray(0x65d10,0x66e10));
const root=0x80300000,manager=0x80302000,arena=0x80303000,sp=0x803ffe00;
let cursor,allocations,audio;
m.trap(0x80070f30,c=>{const size=c.get('a0'),p=cursor;cursor+=((size+15)&~15)+32;assert(cursor<0x803f0000);allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x80071288,c=>{const size=c.get('a0'),p=cursor;cursor+=((size+15)&~15)+32;allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x800712c4,()=>{});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80080998,c=>m.load(c.get('a0'),Buffer.from(m.ram.subarray(m.phys(c.get('a1')),m.phys(c.get('a1'))+c.get('a2')))));
m.trap(0x80093060,c=>m.load(c.get('a1'),Buffer.from(m.ram.subarray(m.phys(c.get('a0')),m.phys(c.get('a0'))+c.get('a2')))));
m.trap(0x8009dd38,c=>c.set('v0',0x803f0000));
m.trap(0x800ea604,c=>audio.push(c.get('a1')));
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const invoke=(pc,args=[])=>{m.set('sp',sp);args.forEach((n,i)=>m.set('a'+i,n));m.call(pc);assert.equal(m.get('sp'),sp);};
const results=[];
for(const item of [1,277,0x201,0x22c])for(const press of [0,13,14,20])for(const mask of [0x8000,0x4000]){
 cursor=arena;allocations=[];audio=[];
 m.load(root,Buffer.alloc(0x1cc0));m.load(manager,Buffer.alloc(216));
 m.w32(0x8022a974,root);m.w32(0x8018fdc0,manager);m.w16(0x8019715e,item);
 m.w8(0x801936e0,0);m.w8(0x80196aec,0);m.w16(0x800c4b28,0);m.w16(0x800e8100,0);
 invoke(0x80236920,[6,34]);
 const owner=m.r32(root+0x19b8+6*4),data=read(m.r32(owner+4),56).toString('hex');
 const states=[];
 for(let tick=0;tick<44;++tick){
  audio=[];const input=tick===press||tick===30?mask:0;
  if(tick){m.w16(0x800c4b28,input);m.w16(0x800e8100,input);invoke(0x80237620);}
  const e=m.r32(manager+4);let record=null,payload=null;
  if(e){const b=read(e,232);payload=read(m.r32(e+0xd0),0x604).toString('hex');for(const at of [0,4,0xd0])b.writeUInt32BE(0,at);record=b.toString('hex');}
  invoke(0x802276f8,[6]);states.push({input,status:m.get('v0')|0,record,payload,audio:[...audio]});
 }
 for(const {p,size} of allocations)for(let i=0;i<16;++i){assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+size+i),0xa7);}
 results.push({item,press,mask,data,states});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));console.log(`${results.length} original reward panels; ${results.length*44} snapshots`);
