'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);
const descriptor=0x387c0+14*40,vram=rom.readUInt32BE(descriptor),start=rom.readUInt32BE(descriptor+8),end=rom.readUInt32BE(descriptor+12);
assert.equal(start,0x281860);assert.equal(end,0x286bd0);m.load(vram,rom.subarray(start,end));
const address=p=>(vram+p-start)>>>0,object=0x80300010,sp=0x803ffe00;
let active=false,registrations=0,frees=0;
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),12);c.set('v0',object);});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x8022681c,c=>{assert.equal(c.get('a0')|0,-2);assert.equal(c.get('a1')|0,-1);active=true;registrations++;});
m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),object);frees++;});
const bytes=()=>m.r32(0x8018fc10)?Buffer.from(m.ram.subarray(m.phys(object),m.phys(object)+12)).toString('hex'):'';
const results=[];
function run(initial,initialActive,args){
 m.set('sp',sp);m.load(object-16,Buffer.alloc(44,0xa7));if(initial)m.load(object,initial);
 m.w32(0x8018fc10,initial?object:0);active=initialActive;registrations=frees=0;
 const operations=[{kind:'start',args},{kind:'update'},{kind:'start',args:[3,257,-2,128,-1,0]},
  ...Array.from({length:4},()=>({kind:'update'})),{kind:'mark'},{kind:'start',args:[2,0,0,0,-1,255]},
  {kind:'update'},{kind:'cleanup'},{kind:'start',args:[2,5,6,7,17,219]},{kind:'update'},
  {kind:'mark'},{kind:'update'},{kind:'cleanup'},{kind:'mark'},{kind:'update'}];
 const states=[];
 for(const op of operations){let release=false;
  if(op.kind==='start'){
   op.args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(address(0x2838ac));
  } else if(op.kind==='update'&&active){m.w8(0x800e7a32,0x89);m.call(address(0x282508));release=!!(m.r8(0x800e7a32)&2);}
  else if(op.kind==='mark')m.call(address(0x2839a8));
  else if(op.kind==='cleanup'){m.call(address(0x2827ac));active=false;}
  m.call(address(0x2839c4));states.push({record:bytes(),query:m.get('v0')|0,active,release,registrations,frees});
  assert.equal(m.get('sp'),sp);
  for(let i=0;i<16;i++){assert.equal(m.r8(object-16+i),0xa7);assert.equal(m.r8(object+12+i),0xa7);}
 }
 results.push({initial:initial?initial.toString('hex'):'',initialActive,operations,states});
}
for(const duration of [0,1,2,3,30,32767,32768,65535,65536,65537,-1])for(const from of [0,255,-1,257])for(const state of [-1,0,1,2,3]){
 const initial=state<0?null:Buffer.from('000500056496c8007f21fea7','hex');if(initial)initial[7]=state;
 run(initial,state>=0,[duration,32,64,128,from,19]);
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original fade lifecycle: ${results.length} sequences, ${results.length*18} snapshots; allocation/registration/free controlled, arithmetic and lifecycle execute original instructions.`);
