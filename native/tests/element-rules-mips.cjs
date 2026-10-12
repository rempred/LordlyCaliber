'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const b=fs.readFileSync(process.argv[2]);if(b.readUInt32BE(0)===0x37804012)b.swap16();
const strength=Number(process.argv[3]),f=Math.fround;
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x801f4294,b.subarray(0x237564,0x237590));
let cases=0;
for(const power of [1,57,300,9999])for(const defense of [0,3,60])for(const level of [-20,0,17])for(const resistance of [0,4,40,50,75,99,100])for(const random of [1,1.07]) {
 m.setSingle(24,power);m.setSingle(26,defense);m.setSingle(22,level);m.setSingle(20,resistance);m.setSingle(6,random);
 m.call(0x801f4294,0x801f42c0);
 const divisor=f(strength?10000/strength:100);
 const expected=Math.trunc(f(f(f(f(f(power-defense)+level)*f(divisor-(strength?resistance:0)))/divisor)*f(random)));
 assert.equal(m.get('s0')|0,expected|0);cases++;
}
for(const [base,width,value] of [[0x239ce4,5,9],[0x239d14,7,3]]) {
 const opposed=new Set(['1,3','3,1','2,4','4,2','5,6','6,5']);
 for(let a=0;a<width;++a)for(let c=0;c<width;++c) {
  const expected=a&&a===c?value:opposed.has(`${a},${c}`)?-value:0;
  assert.equal(b.readInt8(base+a*width+c),expected);
 }
}
console.log(`PASS element tables and ${cases} exported damage calculations at ${strength}% resistance; existing post-kernel minimum and fixed-damage branches remain byte-identical.`);
