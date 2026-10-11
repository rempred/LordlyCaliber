'use strict';
// Original mode-zero draw driver with projection, material and final drawing
// services controlled. Records callback order; does not claim RSP/RDP output.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023ae68,rom.subarray(0x2afc68,0x2b04fc));
const root=0x80300000,actorBase=0x80305000,objectBase=0x80310000,manager=0x80380000,sp=0x803ffe00;
let calls=[];
for(const address of [0x800924d0,0x80090cc0,0x80098aa0,0x80098c20])m.trap(address,()=>{});
m.trap(0x80080998,c=>{assert.equal(c.get('a2'),64);const p=c.phys(c.get('a1'));c.load(c.get('a0'),Buffer.from(c.ram.subarray(p,p+64)));});
for(const address of [0x8022c3a8,0x8022c04c])m.trap(address,()=>{});
m.trap(0x803f0000,c=>{assert.equal(c.get('a1'),255);calls.push([c.get('a2')?2:1,(c.get('a0')-actorBase)/336]);});
m.trap(0x803f0040,c=>calls.push([0,(c.get('a0')-objectBase)/32]));
const cases=[];
for(const channel of [0,1,19])for(const objectChannel of [0,1,19])for(let flags=0;flags<16;flags++)for(const reverse of [false,true]){
 m.load(root,Buffer.alloc(0x2000));m.load(manager,Buffer.alloc(0x56c4));
 m.w32(0x8022a974,root);m.w32(0x801ce8bc,manager);m.w32(0x800e9ba0,0x80340000);m.w32(0x801969b0,0x80360000);
 m.w32(root+0x1ca0,0x80320000);m.w8(root+0x1cb0,objectChannel);
 const actors=[],entries=[];
 for(let i=0;i<28;i++){
  const actor=Buffer.alloc(336);actor.writeUInt32BE(0x803f0000,0xe0);actor[0x141]=(flags*16+i)&255;actor[0x13e]=[0,1,19][i%3];
  m.load(actorBase+i*336,actor);m.w32(root+0x18+i*4,actorBase+i*336);actors.push(actor.toString('hex'));
  entries.push([i%7===0?2:i%11===0?3:1,i]);
  if(i%10===0){const object=Buffer.alloc(32);object.writeUInt32BE(0x803f0040);m.load(objectBase+i*32,object);m.w32(manager+0x52c0+i*4,objectBase+i*32);entries.push([0,i]);}
 }
 if(reverse)entries.reverse();
 const queue=Buffer.alloc(entries.length*20);entries.forEach(([kind,index],i)=>{queue.writeInt16BE(kind,i*20+16);queue.writeInt16BE(index,i*20+18);});m.load(root+0x328,queue);
 calls=[];m.set('sp',sp);m.set('a0',channel);m.set('a1',entries.length);m.call(0x8023ae68);
 assert.equal(m.get('sp'),sp);assert.ok(m.r32(0x800e9ba0)<0x80360000);assert.ok(m.r32(0x801969b0)<0x80380000);
 cases.push({channel,objectChannel,actors,queue:queue.toString('hex'),calls});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original draw passes: ${cases.length}; callback order and eligibility only.`);
