'use strict';
// Original matrix composition and packing, using current-ROM constants.
// Original sine/cosine execute; tangent remains an explicit host service.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023ac68,rom.subarray(0x2afa68,0x2afc68));
m.load(0x80092a90,rom.subarray(0x22e90,0x23018));
m.load(0x800987e0,rom.subarray(0x28be0,0x29160));
m.load(0x80098340,rom.subarray(0x28740,0x287c0));
m.load(0x80243c60,rom.subarray(0x2b8a60,0x2b8a78));
m.load(0x800aa750,rom.subarray(0x3ab50,0x3ab54));
m.trap(0x8009cc10,c=>c.setDouble(0,Math.tan(c.double(12))));
require('./graphics-math-reference.cjs').loadGraphicsMath(m,rom);
const root=0x80300000,base=0x80302000,sp=0x803ffe00;
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const cases=[];
for(const eye of [[0,0,120],[0,0,-120],[3,4,12],[0,60,120]])
for(const fov of [30,45,90])for(const count of [1,4,20]){
 const camera=Buffer.from(rom.subarray(0x2866f0,0x2866f0+56));camera.writeFloatBE(fov,0);
 for(let i=0;i<3;i++){camera.writeFloatBE(eye[i],20+i*4);camera.writeFloatBE(0,32+i*4);}
 m.load(0x8022a720,camera);m.w32(0x8022a974,root);m.w8(0x8022a997,count-1);
 const initial=[];
 for(let i=0;i<20;i++){
  const record=Buffer.alloc(88,0xa7);
  const values=[(i-10)*1.25,(i%7)*2.3,(i%3)*15-30,(i%5)*35.25-60,(i%4)-2,(i+1)*0.5];
  values.forEach((v,f)=>record.writeFloatBE(v,64+f*4));initial.push(record);
  m.w32(root+0x1c54+i*4,base+i*120+16);m.load(base+i*120,Buffer.alloc(120,0xa7));m.load(base+i*120+16,record);
 }
 m.set('sp',sp);m.call(0x8023ac68);assert.equal(m.get('sp'),sp);
 const result=[],layers=[];for(let i=0;i<20;i++){
  result.push(memory(base+i*120+16,88));
  if(i<count){
   m.set('a0',0x80305000);const factor=m.r32(base+i*120+16+0x54);m.set('a1',factor);m.set('a2',factor);m.set('a3',factor);m.call(0x80098aa0);
   m.set('a0',base+i*120+16);m.set('a1',0x80305000);m.set('a2',0x80305100);m.call(0x80098340);layers.push(memory(0x80305100,64).toString('hex'));
  }
  for(let k=0;k<16;k++){assert.equal(m.r8(base+i*120+k),0xa7);assert.equal(m.r8(base+i*120+104+k),0xa7);}
 }
 cases.push({camera:camera.toString('hex'),count,layers,initial:Buffer.concat(initial).toString('hex'),result:Buffer.concat(result).toString('hex')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original channel matrices: ${cases.length} camera/count cases, ${cases.reduce((n,c)=>n+c.count,0)} matrices; only tangent supplied.`);
