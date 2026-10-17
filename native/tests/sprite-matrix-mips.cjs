'use strict';
// Original sprite driver, sine/cosine and packed matrix constructors. Only the
// final raster callback is controlled; emitted matrix bytes are captured.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022b100,rom.subarray(0x29a930,0x29ac44));
m.load(0x80098660,rom.subarray(0x28a60,0x29200));
m.load(0x800abae0,rom.subarray(0x3bee0,0x3bee4));
require('./graphics-math-reference.cjs').loadGraphicsMath(m,rom);
const scene=0x80310000,sp=0x803ffe00,dl=0x80350000,recordAt=0x80320000;
m.w32(0x8022a974,scene);m.w32(scene+0x1ac0,recordAt);
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
let captured=[];
m.trap(0x8022fdb8,c=>{
 for(let p=dl;p<c.r32(0x800e9ba0);p+=8)
  if((c.r32(p)>>>16)===0xda38)captured.push(memory(c.r32(p+4),64).toString('hex'));
});
const cases=[];
for(const xy of [[-123,45],[0,0],[239,-99]])for(const angle of [-180,-1,0,90,179,180,270,32767])
for(const scale of [-32768,-100,-1,0,1,99,100,32767]){
 const record=Buffer.alloc(80);record.fill(255,0,16);record.writeUInt32BE(0x8022fdb8,0x20);
 record.writeInt32BE(xy[0],0x24);record.writeInt32BE(xy[1],0x28);
 record.writeInt16BE(angle,0x40);record.writeInt16BE(scale,0x48);
 m.load(recordAt,record);m.w32(0x800e9ba0,dl);m.w32(0x801969b0,0x80360000);
 m.set('sp',sp);m.set('a0',0);m.set('a1',0);captured=[];m.call(0x8022b100);
 assert.equal(m.get('sp'),sp);assert.equal(captured.length,1+(angle!==180)+(scale!==100));
 cases.push({xy,angle,scale,matrices:captured});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original sprite matrices: ${cases.length} draws; sin/cos and raster callback controlled.`);
