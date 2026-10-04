'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x802395c0,rom.subarray(0x2a8d20,0x2ae338));
const root=0x80300000,projection=0x80310010,sp=0x803ffe00;
m.w32(0x8022a974,root);m.set('sp',sp);
m.load(0x8022a720,rom.subarray(0x2866f0,0x2866f0+144));
m.load(0x801ce8cc,rom.subarray(0x1efa4c,0x1efa4c+32));
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),76);c.set('v0',projection);});
const read=()=>Buffer.from(m.ram.subarray(m.phys(projection),m.phys(projection)+76)).toString('hex');
const bits=x=>{const b=Buffer.alloc(4);b.writeFloatBE(x);return b.readUInt32BE(0);};
m.load(projection-16,Buffer.alloc(108,0xa7));const initial=read();m.call(0x8023998c);const initialized=read();
const actorCamera=Buffer.from(m.ram.subarray(m.phys(0x8022a720),m.phys(0x8022a720)+56)).toString('hex');
const cases=[];
for(const duration of [1,2,3,12,32767,32768,65535,65536,65537,-1,-32768,2147483647,-2147483648]){
 m.load(projection,Buffer.from(initialized,'hex'));
 const stages=[];
 for(const targets of [[-80,30,1,1],[5.125,-0,1.75,0.375],[-1.25,3.5,-0.5,2.25]]){
  m.setSingle(12,targets[0]);m.setSingle(14,targets[1]);m.set('a2',bits(targets[2]));m.set('a3',bits(targets[3]));m.w32(sp+16,duration);
  m.call(0x80239a84);const setup=read(),steps=[];
  for(let i=0;i<15;i++){m.call(0x80239af8);steps.push(read());}
  stages.push({targets:targets.map(bits),duration,setup,steps});
 }
 for(let i=0;i<16;i++){assert.equal(m.r8(projection-16+i),0xa7);assert.equal(m.r8(projection+76+i),0xa7);}
 assert.equal(m.get('sp'),sp);cases.push({duration,stages});
}
m.w32(root+0x19f0,0);m.call(0x80239af8);assert.equal(m.get('sp'),sp);
fs.writeFileSync(process.argv[3],JSON.stringify({initial,initialized,actorCamera,cases}));
console.log(`Original projection: 39 changes, 585 updates, allocation controlled; no interpolation helpers doubled.`);
