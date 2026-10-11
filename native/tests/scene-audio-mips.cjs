'use strict';
// Original scene audio wrappers and recurring deferred/envelope update.
// Resident voice counts, flag read, allocator and downstream audio services are controlled.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);
const delta=0x802270bc-0x28308c,root=0x80300000,envelope=0x80310000,sp=0x803ffe00;
m.load(0x28308c+delta,rom.subarray(0x28308c,0x28364c));
const context=i=>0x800eb0b0+i*0x50;
let calls,voices,guard;
function request(address,kind,global=false,extra=false) {
 m.trap(address,c=>{
  const a=c.get('a0'),index=global?a:(a-context(0))/0x50;
  calls.push({kind,context:index,value:c.get('a1'),extra:extra?c.get('a2'):0});
 });
}
request(0x800ea6b0,0,false,true);request(0x801c5938,1);request(0x800ea604,2);
m.trap(0x800ea6c8,c=>calls.push({kind:3,context:(c.get('a0')-context(0))/0x50,value:0,extra:0}));
m.trap(0x800ea714,()=>calls.push({kind:4,context:-1,value:0,extra:0}));
request(0x800ea884,5);request(0x800ea838,6);request(0x800ea7e4,7);
request(0x800ea86c,8);request(0x800ea89c,9);request(0x800ea8b4,10);
m.trap(0x800814c4,c=>{const mask=c.get('a0'),value=c.get('a1');
 calls.push({kind:11,context:mask,value,extra:0});
 for(let i=0;i<2;++i)if(mask&(1<<i))m.w32(0x800eb2e0+i*8,value&65535);
});
m.trap(0x80081b08,c=>c.set('v0',voices[c.get('a0')-100]));
m.trap(0x8016fef4,c=>{assert.equal(c.get('a0'),0xb5);c.set('v0',guard);});
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),4);m.load(envelope,Buffer.alloc(8,0xa7));c.set('v0',envelope);});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x800712c4,c=>assert.equal(c.get('a0'),envelope));
const entry={110:0x28308c,111:0x2831e0,112:0x2834c4,113:0x283564,180:0x283224,0:0x283350};
const results=[];
function run(commands,route=0,count=0,suppressed=0) {
 voices=Array(7).fill(count);guard=suppressed;calls=[];
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w32(0x8022a8c4,0);
 m.w16(0x8022a994,route);[6000,14000,9000,21000].forEach((gain,i)=>m.w32(0x800eb2e0+8*i,gain));
 for(let i=0;i<7;++i){m.w32(0x8022a8a8+i*4,context(i));m.w32(context(i)+0x24,100+i);}
 const steps=[];
 for(const command of commands){
  const [op,...args]=command;
  if(op===-1){voices.fill(0);continue;}
  calls=[];m.set('sp',sp);args.forEach((v,i)=>m.set('a'+i,v));
  m.call(entry[op]+delta);assert.equal(m.get('sp'),sp);
  const env=m.r32(0x8022a8c4);
  steps.push({op,args:args.map(v=>v>>>0),voices:[...voices],calls,
   pending:Array.from({length:3},(_,i)=>m.r8(root+0x1cb2+i)),
   envelope:env?Buffer.from(m.ram.subarray(m.phys(env),m.phys(env)+8)).toString('hex'):null,
   gains:[m.r32(0x800eb2e0),m.r32(0x800eb2e8)]});
 }
 results.push({route,count,suppressed,steps});
}
for(let context=0;context<8;++context)for(const action of [-1,0,1,2,5])
 for(const route of [0,9])for(const count of [0,2])run([[110,context,action,0x12345678],[0],[-1],[0]],route,count);
for(let context=0;context<8;++context)for(const value of [-2147483648,-1,0,128,65536,2147483647])
 run([[111,context,value]]);
for(let context=0;context<8;++context)for(const guard of [0,1,2])run([[112,context,0x12345678]],0,0,guard);
for(let context=0;context<8;++context)for(const values of [[-1,-1,-1],[128,-1,-1],[-1,999,-1],[-1,-1,-129],[128,50,0x400000],[-2,-3,-2147483648]])
 run([[113,context,...values]]);
for(let group=0;group<8;++group)for(const step of [-65539,-5,0,5,65539,2147483647])
 run([[180,group,group%2,step,3],[0],[0],[180,group,99,-7,2],[0],[0],[0]]);
for(const pending of [0,256,700])run([[180,0,0,-5,20],[110,0,0,pending],[0],[-1],[0],[0]],9,2);
// Graduation's closing fade reads gain-table entry3 before starting its envelope.
run([[180,0,3,-500,30],[0],[0]]);
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} original audio sequences; ${results.reduce((n,r)=>n+r.steps.length,0)} snapshots`);
