'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const file of ['art.js','cutscene-data.js'])vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
const delta=0x8022e9e8-0x29e218,actor=0x80300010,program=0x80310010,sp=0x803ffe00;
m.load(0x29d000+delta,rom.subarray(0x29d000,0x2a8d00));
m.load(0x801ceee0,rom.subarray(0x212370,0x212f00));
m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
m.trap(0x80232e44,c=>c.set('v0',program)); // Resource registry supplies the ROM pose allocation.
let audio=[];
m.trap(0x801c5938,c=>audio.push([c.get('a0')===0x800eb240?0:1,c.get('a1')]));
m.trap(0x8022d534,c=>{ // Output-only projector; input is the original ROM template.
 assert.equal(c.get('a2'),actor+0xa0);c.writeFloat(c.get('a1'),987);c.writeFloat(c.get('a1')+4,654);
});
// Optional source-built owners; the baseline invocation omits all three paths.
if(process.argv[5])m.load(0x8022e9e8,fs.readFileSync(process.argv[5]));
if(process.argv[6])m.load(0x801c1aa4,fs.readFileSync(process.argv[6]));
if(process.argv[7])m.load(0x801ceee0,fs.readFileSync(process.argv[7]));
const rows=[];let steps=0,unbound=0;
for(const source of OB64.cutsceneData.actorArtSources){
 const descriptor=rom.subarray(source.descriptorKey+0x594284),key=descriptor.readUInt32BE(4);
 const data=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded);
 m.load(program-16,Buffer.alloc(data.length+32,0xa7));m.load(program,data);
 const count=data.readUInt32BE(0)/4;
 for(let state=0;state<count;state++){
  if(data.readUInt32BE(state*4)>=data.length){unbound++;continue;}
  m.load(actor-16,Buffer.alloc(0x170,0xa7));m.load(actor,Buffer.alloc(0x150));
  for(let i=0;i<16;i++)m.w8(actor+i,255);
  m.w32(actor+0xe8,source.bank);m.w32(actor+0xf0,0xffffffff);m.w16(actor+0x134,state);
  m.writeFloat(actor+0x11c,1.25);m.writeFloat(actor+0x120,-2.5);m.writeFloat(actor+0x124,3.75);
  m.set('sp',sp);const calls=[];
  for(let tick=0;tick<24;tick++){
   audio=[];m.set('a0',actor);m.call(0x8022e9e8,0x80000100,200000);assert.equal(m.get('sp'),sp);steps+=m.steps;
   calls.push([m.r32(actor+0xf0)|0,m.r32(actor+0xf4)|0,m.r32(actor+0xf8)|0,m.r16(actor+0x134)<<16>>16,m.r16(actor+0x136)<<16>>16,
    ...[0,4,8].map(o=>m.r32(actor+0x11c+o)),Buffer.from(m.ram.subarray(m.phys(actor),m.phys(actor)+32)).toString('hex'),m.get('v0'),audio]);
  }
  for(let i=0;i<16;i++){assert.equal(m.r8(actor-16+i),0xa7);assert.equal(m.r8(actor+0x150+i),0xa7);assert.equal(m.r8(program-16+i),0xa7);assert.equal(m.r8(program+data.length+i),0xa7);}
  rows.push({bank:source.bank,state,calls});
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({rows,unbound,steps}));
console.log(`Original actor pose: ${rows.length} programs, ${rows.length*24} invocations, ${unbound} end-of-resource pointers; ${steps} instructions. Registry/projector/audio services controlled.`);
