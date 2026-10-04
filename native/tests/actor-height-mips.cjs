'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022ad6c,rom.subarray(0x29a59c,0x29a7c4));
m.load(0x80239180,rom.subarray(0x2a89b0,0x2a89c0));
const root=0x80300000,actor=0x80310010,job=0x80320010,sp=0x803ffe00;
let frees=0;m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),12);m.load(job,Buffer.alloc(12));c.set('v0',job);});
m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),job);frees++;});
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const results=[];
function run(args,flags=0,base=20.25,initial='',slot=3){
 const record=Buffer.alloc(336,0xa7);record.writeInt32BE(slot,0xe4);record.writeInt32BE(30,0xe8);record.writeInt16BE(0,0x134);record[0x13d]=record[0x146]=0;record[0x145]=flags;
 record.writeFloatBE(base,0x120);record.writeFloatBE(-11.75,0x128);
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w32(root+24+slot*4,actor);
 m.load(actor-16,Buffer.alloc(368,0xa7));m.load(actor,record);m.load(job-16,Buffer.alloc(44,0xa7));
 if(initial)m.load(job,Buffer.from(initial,'hex'));m.w32(root+0x1a50+slot*4,initial?job:0);m.set('sp',sp);frees=0;
 const state=()=>({actor:memory(actor,336),job:m.r32(root+0x1a50+slot*4)?memory(job,12):'',frees});
 if(args){[slot,...args].forEach((v,i)=>m.set('a'+i,v));m.call(0x8022ad6c);}
 const states=[state()];
 for(let i=0;i<14;i++){
  m.call(0x8022ae84);states.push(state());assert.equal(m.get('sp'),sp);
  for(let k=0;k<16;k++){assert.equal(m.r8(actor-16+k),0xa7);assert.equal(m.r8(actor+336+k),0xa7);assert.equal(m.r8(job-16+k),0xa7);assert.equal(m.r8(job+12+k),0xa7);}
 }
 results.push({slot,initialActor:record.toString('hex'),initial,args,states});
}
for(const flags of [0,4])for(const interval of [1,2,9,12,-2,32767,65545])for(const duration of [0,1,2,12,108,32768,65535,65536,65537])run([interval,-9,duration],flags);
for(const flags of [0,4])for(const base of [20,19.995,20.005,19.99,20.01,19.98,20.02,-16777217])for(const duration of [1,2,3,32767,65535,65536,-1])run([-1,20,duration],flags,base,'00050002000fa7a73f800000',27);
for(const flags of [0,4])for(const progress of [32766,32767,65534,65535]){
 const bytes=Buffer.from('000200000000a7a73f800000','hex');bytes.writeUInt16BE(progress,2);run(null,flags,20,bytes.toString('hex'),0);
}
// Zero cadence is harmless when the first update releases before division.
run([0,9,1]);
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original actor height: ${results.length} sequences, ${results.length*15} full actor/job snapshots; allocation/free controlled.`);
