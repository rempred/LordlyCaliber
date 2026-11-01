'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');const {FloatMips:Mips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();const amount=Number(process.argv[3]);
const {setup}=require('./settlement-mips.cjs');
let cases=0;
for(const key of [1,5,54])for(const assignedTail of [false,true])for(const startingXp of [0,74,75,90,99]) {
 const m=new Mips(),growth=[];const base=0x80193bc0;
 const settlement=setup(m,rom,{key});
 // Force reload to replace stale cached payload; garbage is not executed.
 m.load(0x804c0000,new Uint8Array(4096).fill(0xa5));
 for(let id=1;id<100;id++){
  const p=base+id*56;m.load(p,new Uint8Array(56).fill(0x31));m.w8(p+17,5);m.w8(p+18,5);m.w8(p+19,20);
  m.w16(p+24,50);m.w8(p+51,0);m.w8(p+53,startingXp);}
 m.w8(base+4*56+51,2);m.w8(base+5*56+51,4);m.w16(base+6*56+24,0);m.w8(base+7*56+17,1);
 m.w8(base+8*56+19,99);m.w8(base+9*56+17,0);m.w8(base+10*56+19,98);
 m.w8(0x801971f1,1);m.w8(0x801971f2,3);m.w8(0x801971f3,150);
 if(assignedTail){m.w8(0x801971f0+29*25+1,1);m.w8(0x801971f0+29*25+6,99);}
 const before=m.ram.slice(m.phys(base),m.phys(base)+100*56);
 m.trap(0x8016eba4,c=>{const p=c.get('a0');growth.push((p-base)/56);c.w8(p+19,c.r8(p+19)+1);});
 m.set('sp',0x803ffe00);for(const [i,r] of ['s0','s1','s2','s3'].entries())m.set(r,1000+i);
 settlement.run();
 assert.equal(m.get('sp'),0x803ffe00);for(const [i,r] of ['s0','s1','s2','s3'].entries())assert.equal(m.get(r),1000+i);
 const excluded=new Set([0,3,4,5,6,7,8,9,...(assignedTail?[99]:[])]),expected=before.slice(),expectedGrowth=[];
 for(let id=1;id<100;id++)if(key!==54&&!excluded.has(id)){
  if(startingXp+amount>=100){expected[id*56+19]++;expected[id*56+53]=0;expectedGrowth.push(id);}
  else expected[id*56+53]=startingXp+amount;
 }
 assert.deepEqual(m.ram.slice(m.phys(base),m.phys(base)+5600),expected);
 assert.deepEqual(growth,expectedGrowth);assert.equal(settlement.events.loads,key===54?0:1);assert.equal(settlement.events.cache.length,key===54?0:2);
 assert.deepEqual(settlement.events.items,key===54?[]:[[48,0]]);assert.equal(m.r32(0x80196a6c),key===54?100:600);cases++;
}
console.log(`PASS ${cases} reserve XP settlements at ${amount} XP: squad membership, ineligible characters, level-up calls, cache/DMA bounds, preserved records/registers, item/Goth award and key54 exclusion.`);
