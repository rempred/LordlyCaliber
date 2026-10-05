'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
// Actor overlay placements from the captured Total Resolver observations.
const m=new Mips();m.ram=new Uint8Array(0x400000);
for(const [at,end,ram] of [[0x2a3fa0,0x2a46d0,0x80234770],[0x29b388,0x29b5a4,0x8022bb58],[0x29d468,0x29d4cc,0x8022dc38]])m.load(ram,rom.subarray(at,end));
const root=0x80300000,pool=0x80310010,display=0x80330000,sp=0x803ffe00;
let sizes=[],freed=[],calls=[];
m.trap(0x80070f30,c=>{const size=c.get('a0');assert([16,0x3a0].includes(size));sizes.push(size);m.load(pool-16,Buffer.alloc(size+32,0xa7));c.set('v0',pool);});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x800712c4,c=>freed.push(c.get('a0')));
for(const p of [0x80089a10,0x801c22e8,0x801c45e0,0x801b7e9c,0x8023aa84,0x8023bf30,0x80233580,0x8022bddc,0x8022b6f4,0x80233398,0x800987e0])m.trap(p,c=>calls.push({p,a0:c.get('a0')}));
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
function reset(){m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w32(0x80187af0,0);m.set('sp',sp);sizes=[];freed=[];calls=[];}
function guards(size){assert.equal(m.get('sp'),sp);assert(memory(pool-16,16).equals(Buffer.alloc(16,0xa7)));assert(memory(pool+size,16).equals(Buffer.alloc(16,0xa7)));}
function state(){const p=m.r32(root+0x1ca4);if(!p)return {record:'',alpha:null};m.w32(0x800e9ba0,display);m.call(0x80234814);let alpha=null;for(let at=display;at<m.r32(0x800e9ba0);at+=8)if(m.r32(at)===0xfa000000)alpha=m.r32(at+4)&255;return {record:memory(p,16).toString('hex'),alpha};}
const sequences=[];
for(const mode of [0,2])for(const selector of [0,1,2,3,4,5,255,258])for(const duration of [1,2,3,255,32768,65535,65537]){
 reset();m.w8(0x8018fc19,mode);m.set('a0',selector);m.set('a1',duration);m.call(0x80234cbc);assert.deepEqual(sizes,[16]);guards(16);
 const initial=state(),states=[];
 for(let i=0;i<5;i++){m.call(0x80234770);states.push(state());guards(16);}
 sequences.push({selector,duration,mode,initial,states});
}
// Progress boundaries exercise unsigned wrapping and equality completion without65535 updates.
const boundaries=[];
for(const selector of [2,3,4,5])for(const duration of [1,2,32768,65535])for(const progress of [0,1,duration-1,duration,65534,65535]){
 reset();m.load(pool,Buffer.alloc(16));m.w32(root+0x1ca4,pool);m.w8(pool,selector);m.w16(pool+2,progress);m.w16(pool+4,duration);
 const input=memory(pool,16).toString('hex');m.call(0x80234770);boundaries.push({input,...state()});
}
reset();m.w32(root+0x1a48,root+0x1d00);m.set('a0',160);m.set('a1',120);m.call(0x8022bb58);guards(0x3a0);
const vertices=Array.from({length:10},(_,i)=>({x:(m.r16(pool+i*16)<<16)>>16,y:(m.r16(pool+i*16+2)<<16)>>16,alpha:m.r8(pool+i*16+15)}));
fs.writeFileSync(process.argv[3],JSON.stringify({sequences,boundaries,vertices}));
console.log(`Original captured transitions: ${sequences.length} constructor/update/render sequences, ${boundaries.length} boundary updates, ${vertices.length} vignette vertices; allocation and presentation services controlled.`);
