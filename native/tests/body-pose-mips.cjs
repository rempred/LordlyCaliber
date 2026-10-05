'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decoded=k=>Buffer.from(OB64.art.readCompressedResource(rom,k).decoded);
const handles=raw(0x315736),directory=raw(0x3b6cd0),begin=handles.readUInt16BE(0),end=handles.readUInt16BE(2);
const linked=process.argv[4]==='linked',object=0x80350000;
const m=new FloatMips();m.ram=new Uint8Array(0x400000);const actor=0x80300010,program=0x80310010,sp=0x803ffe00;
m.load(0x8022d7d0,rom.subarray(0x29d000,0x2a8d00));m.load(0x801bdb70,rom.subarray(0x201000,0x212f00));
m.load(0x80187c14,rom.subarray(0x5db14,0x5db14+165*72));m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
m.load(0x80320010,handles.subarray(begin,end));m.w32(0x801d0690,0x80320010);
m.w32(0x801ce8bc,0x80340000); // Explicit empty linked-object row, used by alternate controls 18/20.
if(linked)m.w32(0x803401c4,object);
m.trap(0x801c2ce4,c=>c.set('v0',program));
let audio=[];m.trap(0x801c5938,c=>audio.push([c.get('a0')===0x800eb240?0:1,c.get('a1')]));
let linkedAudio=[];m.trap(0x8022e72c,c=>linkedAudio.push([c.get('a0')-0x44,c.get('a1'),c.get('a2'),
 ((c.get('a3')&255)<<8)+(c.r32(c.get('sp')+16)&255),c.r32(c.get('sp')+20)&255]));
// Optional source-built owners; the baseline invocation omits all three paths.
if(process.argv[5])m.load(0x8022e9e8,fs.readFileSync(process.argv[5]));
if(process.argv[6])m.load(0x801c1aa4,fs.readFileSync(process.argv[6]));
if(process.argv[7])m.load(0x801ceee0,fs.readFileSync(process.argv[7]));
const rows=[],seen=new Set();let steps=0;
for(let source=0;source<165;source++)for(let flags=0;flags<4;flags++){
 const handle=handles.readUInt16BE(begin+(source*4+flags)*2),slot=(handle&0xfff)-1;if(slot<0||slot>=directory.length/4)continue;
 const key=directory.readUInt32BE(slot*4),identity=key+':'+(handle>>>12)+':'+([135,136,161].includes(source)?'clamped':'ordinary');
 if(seen.has(identity))continue;seen.add(identity);
 const desc=raw(key),data=decoded(desc.readUInt32BE(4)),count=data.readUInt32BE(0)/4;
 m.load(program-16,Buffer.alloc(data.length+32,0xa7));m.load(program,data);
 for(let state=0;state<count;state++){
  m.load(actor-16,Buffer.alloc(368,0xa7));m.load(actor,Buffer.alloc(336));for(let i=0;i<16;i++)m.w8(actor+i,255);
  m.w32(actor+0xe8,source);m.w32(actor+0xec,source);m.w32(actor+0xf0,0xffffffff);m.w16(actor+0x134,state);
  m.w8(actor+0x13d,1);m.w16(actor+0x13a,flags&1);m.w8(actor+0x146,flags>>1);
  m.w32(0x8034020c,source);m.w32(0x80340210,source);
  m.writeFloat(actor+0x11c,1.25);m.writeFloat(actor+0x120,-2.5);m.writeFloat(actor+0x124,3.75);m.set('sp',sp);
  const calls=[];
  for(let tick=0;tick<8;tick++){
   audio=[];linkedAudio=[];m.set('a0',actor);m.call(0x8022e9e8,0x80000100,200000);assert.equal(m.get('sp'),sp);steps+=m.steps;
   calls.push([m.r32(actor+0xf0)|0,m.r32(actor+0xf4)|0,m.r32(actor+0xf8)|0,m.r16(actor+0x134)<<16>>16,m.r16(actor+0x136)<<16>>16,
    ...[0,4,8].map(o=>m.r32(actor+0x11c+o)),Buffer.from(m.ram.subarray(m.phys(actor),m.phys(actor)+32)).toString('hex'),m.get('v0'),audio,linkedAudio]);
  }
  for(let i=0;i<16;i++){assert.equal(m.r8(actor-16+i),0xa7);assert.equal(m.r8(actor+336+i),0xa7);assert.equal(m.r8(program-16+i),0xa7);assert.equal(m.r8(program+data.length+i),0xa7);}
  rows.push({source,flags,state,calls});
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({rows,steps}));console.log(`Original alternate pose: ${rows.length} states, ${rows.length*8} invocations, ${steps} instructions. Resource registry/audio controlled; linked object ${linked?'present, sound service requests recorded':'explicitly absent'}.`);
