'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');const {FloatMips}=require('./float-mips.cjs');
function read(p){const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const edited=read(process.argv[2]),retail=read(process.argv[3]),scale=Number(process.argv[4]);
const ram=n=>(n+0x800ab8c0)>>>0,f=Math.fround;let goths=0,soldiers=0;
for(const [image,low,high,mult,min] of [[retail,25,50,100,1],[edited,10,100,scale,scale?3:0]]) {
 const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(ram(0x1399c0),image.subarray(0x1399c0,0x139b78));
 const wallet=0x80196a6c,award=0x8021c744,reserves=0x80190f80;
 m.trap(0x801dc720,c=>{assert.equal(c.get('a0'),48);assert.equal(c.get('a1'),0);});
 for(const key of [1,5,54])for(const cf of [0,49,50,51,100])for(const pop of [0,1,999,100000])for(const base of [0,500,65535])for(const money of [0,9999998,9999999]) {
  m.r.fill(0);m.set('a0',cf);m.set('a1',pop);m.set('a2',base);m.w8(0x8018f481,key);
  m.w32(wallet,money);m.w16(0x8021c740,48);m.w8(0x8021c748,9);m.w8(reserves,195);m.w8(reserves+1,220);
  m.call(ram(0x1399c0),ram(0x139b78));
  const expected=base+Math.floor(Math.abs(cf-50)*pop/(cf<50?low:high));
  assert.equal(m.r32(award),expected);assert.equal(m.r32(wallet),key===54?money:Math.min(9999999,money+expected));
  assert.equal(m.r8(reserves),key===54?195:200);assert.equal(m.r8(reserves+1),key===54?220:225);goths++;
 }
 m.load(ram(0x127c50),image.subarray(0x127c50,0x127d30));m.load(0x801ee7f0,image.subarray(0x142f30,0x142f38));
 for(const population of [0,1,99,704,999,65535])for(const morale of [0,1,50,100,127,128,173,255])for(const flags of [0,2,4,6])for(const count of [0,1,3,16]) {
  m.r.fill(0);m.set('sp',0x803fff00);m.w32(0x801f1070,count);let expected=0;
  for(let i=0;i<count;++i){const site=0x801951b0+i*36;m.w16(site+0x1c,flags);m.w8(site+0x1e,morale);m.w16(site+0x20,population);
   if(flags&2)expected+=Math.max(min,Math.trunc(f(f(population)*f(f(morale===255?50:morale&127)/f(100)))*(mult/10000)));}
  m.call(ram(0x127c50));assert.equal(m.get('v0'),expected&255);assert.equal(m.get('sp'),0x803fff00);soldiers++;
 }
}
console.log(`PASS ${goths} completion Goth/cap/key54 cases and ${soldiers} captured-site Soldier calculations, including zero scale, morale metadata, minimums and byte wrapping.`);
