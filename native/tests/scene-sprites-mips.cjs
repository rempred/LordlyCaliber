'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const table=Buffer.from(OB64.art.readCompressedResource(rom,0x109eeb0).decoded);
const m=new Mips();m.ram=new Uint8Array(0x400000);
const delta=0x7ff907d0,scene=0x80310010,sp=0x803ffe00,program=0x80330000;
m.load(0x29a000+delta,rom.subarray(0x29a000,0x2a8d00));
m.load(0x80241ed0,rom.subarray(0x2b6cd0,0x2b6f40));
m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));m.load(0x801ceee0,rom.subarray(0x212370,0x212f00));
m.load(0x80320000,table);m.w32(0x8022a97c,0x80320000);m.w32(0x8022a974,scene);
let allocation=0x80380010,allocated=new Set(),freed=[],sounds=[];
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),80);const at=allocation;allocation+=112;allocated.add(at);m.load(at-16,Buffer.alloc(112,0xa7));m.load(at,Buffer.alloc(80));c.set('v0',at);});
m.trap(0x800712c4,c=>{const at=c.get('a0');assert(allocated.has(at));allocated.delete(at);freed.push(at);});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
const programs=new Map();
m.trap(0x80232e44,c=>{
 const bank=c.get('a0');if(!programs.has(bank)){
  const descriptor=rom.readUInt32BE(0x109f95e+0x594284+(bank-1)*4);
  programs.set(bank,OB64.art.readCompressedResource(rom,rom.readUInt32BE(descriptor+0x594288)).decoded);
 }
 m.load(program,programs.get(bank));c.set('v0',program);
});
m.trap(0x801c5938,c=>sounds.push([c.get('a0')===0x800eb240?0:1,c.get('a1')]));
const routines={70:0x8022af94,99:0x80241ed0,100:0x80242030,101:0x802420dc,102:0x8022b414};
function snapshot(){const result=[];for(let slot=0;slot<30;slot++){const p=m.r32(scene+0x1ac0+slot*4);if(p)result.push({slot,record:Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+80)).toString('hex')});}return result;}
const results=[];
function run(operations,initial=[]){
 allocation=0x80380010;allocated=new Set();m.load(scene,Buffer.alloc(0x1d00));
 for(const item of initial){const p=allocation;allocation+=112;allocated.add(p);m.load(p-16,Buffer.alloc(112,0xa7));m.load(p,Buffer.from(item.record,'hex'));m.w32(scene+0x1ac0+item.slot*4,p);}
 const states=[];
 for(const op of operations){
  freed=[];sounds=[];m.set('sp',sp);
  if(op.advance){m.set('a0',0);m.call(0x80230fd4);}
  else {
   for(let i=0;i<Math.min(4,op.args.length);i++)m.set('a'+i,op.args[i]);
   for(let i=4;i<op.args.length;i++)m.w32(sp+16+(i-4)*4,op.args[i]);
   m.call(routines[op.opcode]);
  }
  assert.equal(m.get('sp'),sp);
  for(const p of allocated)for(let i=0;i<16;i++){assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+80+i),0xa7);}
  states.push({records:snapshot(),value:op.opcode===101?m.get('v0'):0,freed:freed.length,sounds});
 }
 results.push({initial,operations,states});
}
for(let bank=1;bank<=68;bank++){
 const first=table.readInt16BE((bank-1)*4),key=table.readInt16BE(first*2);
 if(key<0)continue;
 const slot=(bank*7)%30,animation=Math.trunc(key/10),facing=key%10;
 run([{opcode:70,args:[slot,bank,animation,facing,-162,55,3,108,0]},
  {opcode:101,args:[slot]},...Array.from({length:8},()=>({advance:true})),{opcode:102,args:[slot]}]);
}
for(const value of [-2147483648,-32769,-1,0,1,32768,65537,2147483647]){
 const c={opcode:70,args:[0x11d,57,0x10000,0,value,91,257,value,258]};
 run([c,{opcode:70,args:[29,57,1,0,15,-1,2,100,0]},
  {opcode:100,args:[0x11d,-1,-1,-1]}, {opcode:101,args:[29]}, {advance:true},
  {opcode:99,args:[29,63,0,7,9,10,value,-1,-1,0]}, {opcode:102,args:[-1]}]);
 run([{opcode:99,args:[0,57,0,0,value,1,value,-1,-1,-1]}, {advance:true},
  {opcode:100,args:[0,63,0,255]}, {opcode:101,args:[0]}, {opcode:102,args:[0]}]);
}
// Preserve all fields outside the restart's explicit write set.
const initial=Buffer.alloc(80,0xa7);initial.writeInt32BE(57,0x30);initial.writeInt16BE(0,0x42);initial.writeInt16BE(0,0x44);
run([{opcode:100,args:[12,-1,-1,0]},{opcode:101,args:[12]},{opcode:102,args:[-1]}],[{slot:12,record:initial.toString('hex')}]);
run([{opcode:70,args:[0,57,0,0,0,0,0,100,0]}, {opcode:70,args:[29,63,0,0,1,1,3,100,0]},
 {advance:true},{opcode:102,args:[0]},{opcode:101,args:[29]},{opcode:102,args:[-1]},{opcode:102,args:[-1]}]);
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original sprite lifecycle: ${results.length} sequences. Allocation/free/resource registry/audio sink controlled; resolver and immediate poses execute.`);
