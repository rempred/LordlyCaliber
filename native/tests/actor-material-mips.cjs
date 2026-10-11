'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022bf5c,rom.subarray(0x29b78c,0x29bef4));
const root=0x80300000,pool=0x80320010,input=0x80340000,display=0x80370000,sp=0x803ffe00;
let allocations=[],frees=0;
m.trap(0x80070f30,c=>{const size=c.get('a0');assert([7,12].includes(size));const p=pool+allocations.length*64;allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x800712c4,c=>{const p=c.get('a0');assert(p===0||allocations.some(a=>a.p===p));if(p)frees++;});
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const actor=slot=>0x80310010+slot*368;
const results=[];
for(const selector of [-1,0,27,65536,3])for(const duration of [0,1,2,3,7,32767,32768,65535,65536,65537])for(const enabled of [0,1,65536,65535]){
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.set('sp',sp);allocations=[];frees=0;
 const initial=[];
 for(const slot of [0,9,27]){
  const b=Buffer.alloc(336,0xa7);b.writeInt32BE(slot,0xe4);b.writeInt32BE(30,0xe8);b.writeInt16BE(0,0x134);b[0x13d]=b[0x146]=0;
  m.load(actor(slot)-16,Buffer.alloc(368,0xa7));m.load(actor(slot),b);m.w32(root+24+slot*4,actor(slot));initial.push({slot,record:b.toString('hex')});
 }
 const slot=selector===-1?0:selector&65535;
 const operations=[{draw:[0,9,27]},{op:0x1e,args:[selector,257,250,-1,200,17,56,duration,enabled]},
  ...Array.from({length:5},()=>({draw:[0,9,27]})),
  {op:0x28,args:[slot+65536,300,-2,70,91,92,93,enabled]}, {draw:[0,27]},
  {op:0x28,args:[slot,10,20,30,33,44,55,1]},{draw:[0,9,27]},
  {op:0x29,args:[slot]},{draw:[0,9,27]},
  {op:0x1e,args:[slot,7,8,9,250,240,230,2,1]},{draw:[0,9,27]},
  {op:0xa7,args:[slot]},{draw:[0,9,27]},{op:0xa7,args:[slot]}];
 const states=[];
 for(const op of operations){
  let commands='';
  if(op.draw){
   m.w32(0x800e9ba0,display);
   for(const slot of op.draw){m.set('a0',actor(slot));m.call(0x8022c3a8);m.set('a0',actor(slot));m.call(0x8022c04c);}
   commands=memory(display,m.r32(0x800e9ba0)-display);
  }else if(op.op===0x1e){op.args.forEach((v,i)=>{if(i<4)m.set('a'+i,v);else m.w32(sp+i*4,v);});m.call(0x8022c250);}
  else if(op.op===0x28){op.args.forEach((v,i)=>m.w32(input+i*4,v));m.set('a0',input);m.call(0x8022bf5c);}
  else{m.set('a0',op.args[0]);m.call(op.op===0x29?0x8022c1b0:0x8022c5c8);}
  const tints=[],materials=[],active=[];
  for(let i=0;i<28;i++){
   let p=m.r32(root+0x248+i*4);if(p)tints.push({slot:i,record:memory(p,12)});
   p=m.r32(root+0x1944+i*4);if(p)materials.push({slot:i,record:memory(p,7)});
   m.set('a0',i);m.call(0x8022c594);if(m.get('v0'))active.push(i);
  }
  states.push({tints,materials,active,commands,actors:initial.map(a=>({slot:a.slot,record:memory(actor(a.slot),336)}))});
  assert.equal(m.get('sp'),sp);
  for(const a of allocations)for(let k=0;k<16;k++){assert.equal(m.r8(a.p-16+k),0xa7);assert.equal(m.r8(a.p+a.size+k),0xa7);}
 }
 results.push({initial,operations,states});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original actor materials: ${results.length} sequences, ${results.length*results[0].states.length} full snapshots; allocation/free controlled.`);
