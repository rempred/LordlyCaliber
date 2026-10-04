'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8022d7d0,rom.subarray(0x29d000,0x2a8d00));
const scene=0x80300000,actors=0x80310010;m.w32(0x8022a974,scene);m.set('sp',0x803ffe00);
const initial=[0,3,27].map(slot=>{const b=Buffer.alloc(336,0xa7);b.writeInt32BE(slot,0xe4);b.writeInt32BE(30,0xe8);b.writeInt16BE(0,0x134);b[0x13d]=0;b[0x146]=0;
 return {slot,record:b.toString('hex')};});
const results=[];
for(const opcode of [0x1d,0x48])for(const slot of [-1,0,3,4,27,65536,65563,32767])for(const value of [-300,0,1,97,100,255,256,16777217,-2147483648,2147483647]){
 for(const a of initial){const ptr=actors+a.slot*0x200;m.load(ptr-16,Buffer.alloc(368,0xa7));m.load(ptr,Buffer.from(a.record,'hex'));m.w32(scene+24+a.slot*4,ptr);}
 m.set('a0',slot);m.set('a1',value);m.call(opcode===0x1d?0x80231e60:0x80231f2c);
 const after=initial.map(a=>{const ptr=actors+a.slot*0x200;for(let i=0;i<16;i++){assert.equal(m.r8(ptr-16+i),0xa7);assert.equal(m.r8(ptr+336+i),0xa7);}
  return {slot:a.slot,record:Buffer.from(m.ram.subarray(m.phys(ptr),m.phys(ptr)+336)).toString('hex')};});
 results.push({opcode,slot,value,after});
}
fs.writeFileSync(process.argv[3],JSON.stringify({initial,results}));
console.log(`Original actor style: ${results.length} scale/opacity cases, complete records, no helper doubles.`);
