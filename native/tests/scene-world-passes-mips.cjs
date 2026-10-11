'use strict';
// Original mode-two draw driver. Queue, prepasses, materials and raster callbacks
// are controlled; compare pass order, shadow selection and native alpha narrowing.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023aecc,rom.subarray(0x2aa62c,0x2aab88));
const root=0x80300000,actorBase=0x80305000,objectBase=0x80310000,manager=0x80380000,sp=0x803ffe00;
let calls=[],copied=-1,queueCount=0;
for(const address of [0x802397c4,0x8023ac50,0x80098aa0,0x80098c20,0x80098340,0x801b8f2c,
 0x8022c3a8,0x8022c04c,0x8023e83c])m.trap(address,()=>{});
m.trap(0x8022de04,c=>c.set('v0',queueCount));
m.trap(0x80080998,c=>{assert.equal(c.get('a2'),64);const src=c.get('a1'),p=c.phys(src);c.load(c.get('a0'),Buffer.from(c.ram.subarray(p,p+64)));
 if(src>=actorBase&&src<actorBase+28*336){const off=(src-actorBase)%336;copied=off===0x20?0:off===0x60?1:-1;}});
m.trap(0x803f0000,c=>calls.push([1,(c.get('a0')-actorBase)/336,c.get('a2')?copied:-1,c.get('a1')]));
m.trap(0x803f0040,c=>{const p=c.get('a0');calls.push([m.r8(p+20),(p-objectBase)/32,-1,255]);});
const cases=[];
for(const alpha of [[0,0],[96,128],[255,0],[0,255],[-1,256],[-256,511]])
for(let flags=0;flags<16;flags++)for(const reverse of [false,true]){
 m.load(root,Buffer.alloc(0x2000));m.load(manager,Buffer.alloc(0x56c4));m.w8(root+0x1cb1,1);
 m.w32(0x8022a974,root);m.w32(0x801ce8bc,manager);m.w32(0x800e9ba0,0x80340000);m.w32(0x801969b0,0x80360000);
 m.w32(root+0x19f0,0x80320000);m.w16(0x801d071c,alpha[0]);m.w16(0x801d0820,alpha[1]);
 const actors=[],objects=Array(256).fill(''),entries=[];
 for(let i=0;i<28;i++){
  const actor=Buffer.alloc(336);actor.writeUInt32BE(0x803f0000,0xe0);actor[0x141]=(flags*16+i)&255;actor[0x13e]=i%20;
  m.load(actorBase+i*336,actor);m.w32(root+0x18+i*4,actorBase+i*336);actors.push(actor.toString('hex'));entries.push([1,i]);
 }
 for(let i=0;i<6;i++){
  const kind=[0,2,3][i%3],object=Buffer.alloc(32);object.writeUInt32BE(0x803f0040);object[20]=kind;
  m.load(objectBase+i*32,object);m.w32(manager+0x52c0+i*4,objectBase+i*32);objects[i]=object.toString('hex');entries.splice(i*4,0,[kind,i]);
 }
 entries.push([9,-1]); // Original driver ignores unrecognized queue kinds.
 if(reverse)entries.reverse();queueCount=entries.length;
 const queue=Buffer.alloc(entries.length*20);entries.forEach(([kind,index],i)=>{queue.writeInt16BE(kind,i*20+16);queue.writeInt16BE(index,i*20+18);});m.load(root+0x328,queue);
 calls=[];copied=-1;m.set('sp',sp);m.call(0x8023aecc);assert.equal(m.get('sp'),sp);
 assert.ok(m.r32(0x800e9ba0)<0x80360000);assert.ok(m.r32(0x801969b0)<0x80380000);
 cases.push({alpha,actors,objects,queue:queue.toString('hex'),calls});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original world draw passes: ${cases.length}; callback order/matrix/alpha, no raster proof.`);
