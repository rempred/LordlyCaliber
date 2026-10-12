'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();const amount=Number(process.argv[3]);
class PiMips extends Mips {
 constructor(){super();this.pi=new Map();this.loads=0;}
 exec(pc,next){if(pc<0x80100000)throw Error(`Unexpected PC ${pc.toString(16)} RA=${this.get("ra").toString(16)} SP=${this.get("sp").toString(16)} trace=${this.trace}`);this.trace=(this.trace||[]).concat(pc.toString(16)).slice(-12);return super.exec(pc,next);}
 r32(a){if((a>>>0)===0xa4600010)return 0;return super.r32(a);}
 w32(a,v){if((a>>>0)>=0xa4600000&&(a>>>0)<0xa4600020){
  this.pi.set(a>>>0,v>>>0);if((a>>>0)===0xa460000c){
   assert.equal(this.pi.get(0xa4600000),0x4c0000);assert.equal(this.pi.get(0xa4600004),0x127c6000);assert.equal(v,4095);
   this.load(0x804c0000,rom.subarray(0x27c6000,0x27c7000));this.loads++;}return;
 }super.w32(a,v);}
}
let cases=0;
for(const key of [1,5,54])for(const assignedTail of [false,true])for(const startingXp of [0,74,75,90,99]) {
 const m=new PiMips(),growth=[],cache=[],items=[];const base=0x80193bc0;
 m.load(0x801e5280,rom.subarray(0x1399c0,0x139b78));
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
 for(const at of [0x800900c0,0x80090010])m.trap(at,c=>{
  cache.push([at,c.get('a0'),c.get('a1')]);assert.equal(c.get('a0'),0x804c0000);assert.equal(c.get('a1'),4096);
  for(const r of ['a0','a1','a2','a3','v0','v1','t0','t1'])c.set(r,0xbad);
 });
 m.trap(0x801dc720,c=>{items.push([c.get('a0'),c.get('a1')]);c.set('v0',77);});
 m.trap(0x8016eba4,c=>{const p=c.get('a0');growth.push((p-base)/56);c.w8(p+19,c.r8(p+19)+1);});
 m.set('sp',0x803ffe00);for(const [i,r] of ['s0','s1','s2','s3'].entries())m.set(r,1000+i);
 m.set('a0',50);m.set('a1',0);m.set('a2',500);m.w8(0x8018f481,key);m.w32(0x80196a6c,100);
 m.w16(0x8021c740,48);m.w8(0x8021c748,0);m.call(0x801e5280,0x801e5438);
 assert.equal(m.get('sp'),0x803ffe00);for(const [i,r] of ['s0','s1','s2','s3'].entries())assert.equal(m.get(r),1000+i);
 const excluded=new Set([0,3,4,5,6,7,8,9,...(assignedTail?[99]:[])]),expected=before.slice(),expectedGrowth=[];
 for(let id=1;id<100;id++)if(key!==54&&!excluded.has(id)){
  if(startingXp+amount>=100){expected[id*56+19]++;expected[id*56+53]=0;expectedGrowth.push(id);}
  else expected[id*56+53]=startingXp+amount;
 }
 assert.deepEqual(m.ram.slice(m.phys(base),m.phys(base)+5600),expected);
 assert.deepEqual(growth,expectedGrowth);assert.equal(m.loads,key===54?0:1);assert.equal(cache.length,key===54?0:2);
 assert.deepEqual(items,key===54?[]:[[48,0]]);assert.equal(m.r32(0x80196a6c),key===54?100:600);cases++;
}
console.log(`PASS ${cases} reserve XP settlements at ${amount} XP: squad membership, ineligible characters, level-up calls, cache/DMA bounds, preserved records/registers, item/Goth award and key54 exclusion.`);
