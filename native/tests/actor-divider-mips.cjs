'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
// Mode-zero scene overlay; do not apply the mode-two placement delta.
m.load(0x8023a5ec,rom.subarray(0x2af3ec,0x2af6c0));
const root=0x80300000,job=0x80320010,sp=0x803ffe00;
let allocated=false;
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),12);assert(!allocated);allocated=true;m.load(job-16,Buffer.alloc(44,0xa7));m.load(job,Buffer.alloc(12));c.set('v0',job);});
m.trap(0x800712c4,c=>{assert([0,job].includes(c.get('a0')));allocated=false;});
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const actor=slot=>0x80310010+slot*368;
const geometry=[[110,93,-52,134],[-54,140,123,92],[0,0,10,10],[0,4,10,4],[7,0,7,10],
 [0,0,0,0],[-9,-9,9,9],[10,-9,-3,5],[-32768,32767,32767,-32768],[65537,65538,65539,65540],
 [1,1,2,2],[1,2,2,1]];
const positions=[[0,0],[1,1],[-1,-1],[110,93],[-52,134],[-54,140],[123,92],[-1000,1000],
 [1000,-1000],[32767,-32768],[-32768,32767],[0.01,-0.01]];
const results=[];
for(const points of geometry)for(const reuse of [false,true])for(const channels of [[3,1],[257,256],[255,-1]]){
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.set('sp',sp);allocated=reuse;
 const initialDivider=reuse?'3f800000400000000304a7a7':'';
 m.load(job-16,Buffer.alloc(44,0xa7));if(reuse)m.load(job,Buffer.from(initialDivider,'hex'));m.w32(root+0x19b4,reuse?job:0);
 const initial=[];
 for(const slot of [0,5,27]){const b=Buffer.alloc(336,0xa7);b.writeInt32BE(slot,0xe4);b.writeInt32BE(30,0xe8);b.writeInt16BE(0,0x134);b[0x13d]=b[0x145]=b[0x146]=0;b.writeFloatBE(0,0x11c);b.writeFloatBE(0,0x120);b.writeFloatBE(0,0x124);m.load(actor(slot)-16,Buffer.alloc(368,0xa7));m.load(actor(slot),b);m.w32(root+24+slot*4,actor(slot));initial.push({slot,record:b.toString('hex')});}
 const operations=[{op:0x5f,args:[...points,...channels]},...positions.map((_,i)=>({positions:[0,5,27].map((slot,j)=>({slot,x:positions[(i+j)%positions.length][0],z:positions[(i+j)%positions.length][1]}))})),
  {op:0x1c,args:[-1,0x103]},{op:0x1c,args:[65536,9]},
  {op:0x5f,args:[-54,140,123,92,7,8]},{positions:[{slot:0,x:1,z:1}]},
  {op:0x75,args:[]},{positions:[{slot:0,x:-9,z:900}]},{op:0x1c,args:[27,1]}];
 const states=[];
 for(const op of operations){
  if(op.positions){for(const p of op.positions){m.writeFloat(actor(p.slot)+0x11c,p.x);m.writeFloat(actor(p.slot)+0x124,p.z);}m.call(0x8023a7f4);}
  else if(op.op===0x5f){op.args.forEach((v,i)=>{if(i<4)m.set('a'+i,v);else m.w32(sp+i*4,v);});m.call(0x8023a670);}
  else if(op.op===0x75)m.call(0x8023a7c4);
  else{m.set('a0',op.args[0]);m.set('a1',op.args[1]);m.call(0x8023a5ec);}
  states.push({divider:m.r32(root+0x19b4)?memory(job,12):'',actors:initial.map(a=>({slot:a.slot,record:memory(actor(a.slot),336)}))});
  assert.equal(m.get('sp'),sp);
  for(let k=0;k<16;k++){assert.equal(m.r8(job-16+k),0xa7);assert.equal(m.r8(job+12+k),0xa7);}
 }
 results.push({initial,initialDivider,operations,states});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original Actor divider: ${results.length} sequences, ${results.length*results[0].states.length} snapshots; allocation/free controlled.`);
