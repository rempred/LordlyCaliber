'use strict';
// Complete original object callback with a controlled empty art lookup. Camera
// basis, RPY, sine/cosine and packing execute; atan2 and variant lookup supplied.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x801b0770,rom.subarray(0x1f3c00,0x1f5654));
m.load(0x8023aa8c,rom.subarray(0x2aa1ec,0x2aa3b0));
for(const [a,b] of [[0x22e90,0x231b0],[0x28be0,0x28da0]])m.load(a+0x8006fc00,rom.subarray(a,b));
m.load(0x800aa750,rom.subarray(0x3ab50,0x3ab54));
m.load(0x8023ec10,rom.subarray(0x2ae370,0x2ae3b0));
require('./graphics-math-reference.cjs').loadGraphicsMath(m,rom);
m.trap(0x8009cfe0,c=>c.setSingle(0,Math.atan2(c.single(12),c.single(14))));
m.trap(0x801c8fb8,c=>c.set('v0',0));m.trap(0x801c2224,c=>c.set('v0',2));
m.trap(0x801c2178,c=>c.set('v0',0));
const object=0x80310000,owner=0x80310100,matrix=0x80320000,dl=0x80330000,cameraAt=0x80340000,sp=0x803ffe00;
const memory=(at,n)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+n));
m.w32(owner+0x48,0x100);m.w8(owner+0xab,255);
for(let i=0;i<3;i++)m.w16(0x80220e70+i*2,255);
const cases=[];
for(const eye of [[0,80,300],[40,100,-200],[240,-60,0],[0,0,100]])
for(const xyz of [[0,0,0],[-150,200,-100],[32760,-32760,17],[19,-44,0],[-100,0,90]])
for(const type of [2,3]){
 const camera=Buffer.from(rom.subarray(0x2866f0,0x286728));
 for(let i=0;i<3;i++){camera.writeFloatBE(eye[i],20+i*4);camera.writeFloatBE([10,0,-5][i],32+i*4);}
 m.load(cameraAt,camera);m.set('sp',sp);m.set('a0',cameraAt);m.call(0x8023aa8c);
 const record=Buffer.alloc(192);record[0x14]=type;record.writeUInt32BE(owner,0x40);record.writeUInt16BE(255,0x34);
 record[0x56]=255;
 for(let i=0;i<3;i++){record.writeInt16BE(xyz[i],0x1c+i*2);record.writeInt16BE([7,-5,3][i],0x22+i*2);record.writeInt16BE([-9,11,-3][i],0x50+i*2);}
 m.load(object,record);m.w32(0x801969b0,matrix);m.w32(0x800e9ba0,dl);
 m.set('a0',object);m.set('a1',type!==3?1:0);m.set('a2',0);m.set('a3',0);m.w32(sp+16,0);m.w32(sp+20,0);
 m.call(0x801b0770);assert.equal(m.get('sp'),sp);assert.equal(m.r32(0x801969b0),matrix+64);
 cases.push({camera:camera.toString('hex'),drawing:record.toString('hex'),matrix:memory(matrix,64).toString('hex')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
