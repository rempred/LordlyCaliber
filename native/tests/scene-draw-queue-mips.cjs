'use strict';
// Original queue construction, flattened-camera depth, comparator and complete
// quick/insertion sorter. Only allocation/free and byte copies are controlled.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80098340,rom.subarray(0x28740,0x29160));
m.load(0x80090940,rom.subarray(0x20d40,0x210c0));
m.load(0x800ae890,rom.subarray(0x3ec90,0x3ed00));
m.load(0x801cca5c,rom.subarray(0x20feec,0x2101dc));
m.load(0x8022dcc0,rom.subarray(0x29d4f0,0x29d790));
m.load(0x80231fb0,rom.subarray(0x2a17e0,0x2a1860));
const root=0x80300000,actorBase=0x80305000,objectBase=0x80310000,manager=0x80380000,sp=0x803ffe00;
let temporary=0,allocations=0;
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),20);c.set('v0',temporary);allocations++;});
m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),temporary);allocations--;});
m.trap(0x80093484,c=>{assert.equal(c.get('a2'),20);const p=c.phys(c.get('a1'));c.load(c.get('a0'),Buffer.from(c.ram.subarray(p,p+20)));});
const cases=[];let seed=0x204c;
const random=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return (seed%6001)-3000;};
for(const objectsCount of [0,7,256])for(const actorsCount of [0,1,12,28])
for(const pattern of [0,1,2])for(const eye of [[0,0,400],[20,100,600],[240,80,20]])
for(const bankChoice of [0,1])for(const scratch of [0x802f0000,0x80330000,0x80330008]){
 m.load(root,Buffer.alloc(0x2000,0xa7));m.load(manager,Buffer.alloc(0x56c4));
 m.w32(0x8022a974,root);m.w32(0x801ce8bc,manager);m.w32(manager+0x56c0,objectsCount);
 const camera=Buffer.from(rom.subarray(0x286748,0x286748+56));
 for(let i=0;i<3;i++){camera.writeFloatBE(eye[i],20+i*4);camera.writeFloatBE(0,32+i*4);}
 m.load(0x8022a778,rom.subarray(0x286748,0x286748+56));m.load(0x8022a720,rom.subarray(0x2866f0,0x2866f0+56));
 m.load(bankChoice?0x8022a720:0x8022a778,camera);m.w8(0x8018fc19,bankChoice);
 const actors=[],objects=[];
 for(let i=0;i<objectsCount;i++){
  if(pattern===2&&i%11===0){objects.push('');continue;}
  const record=Buffer.alloc(24,0xa7);record[20]=pattern===0?1:i%6;
  record.writeInt32BE(pattern===0?0:random(),4);record.writeInt32BE(pattern<2?20:random(),8);record.writeInt32BE(pattern===0?0:random(),12);
  m.load(objectBase+i*32,record);m.w32(manager+0x52c0+i*4,objectBase+i*32);objects.push(record.toString('hex'));
 }
 for(let i=0;i<28;i++){
  if(i>=actorsCount){m.w32(root+0x18+i*4,0);actors.push('');continue;}
  const record=Buffer.alloc(336,0xa7);
  record.writeFloatBE(pattern===0?0:random()/30,0x11c);record.writeFloatBE(pattern<2?2/3:random()/30,0x120);record.writeFloatBE(pattern===0?0:random()/30,0x124);
  m.load(actorBase+i*336,record);m.w32(root+0x18+i*4,actorBase+i*336);actors.push(record.toString('hex'));
 }
 temporary=scratch;allocations=0;m.set('sp',sp);m.call(0x8022de04,0x80000100,4000000);
 assert.equal(m.get('sp'),sp);assert.equal(allocations,0);const count=m.get('v0');assert.ok(count<=284);
 const at=m.phys(root+0x328),result=Buffer.from(m.ram.subarray(at,at+count*20));
 assert.equal(m.r8(root+0x327),0xa7);assert.equal(m.r8(root+0x328+count*20),0xa7);
 for(let i=0;i<28;i++)if(actors[i])assert.equal(Buffer.from(m.ram.subarray(m.phys(actorBase+i*336),m.phys(actorBase+i*336)+336)).toString('hex'),actors[i]);
 cases.push({camera:camera.toString('hex'),queue:root+0x328,temporary,actors,objects,result:result.toString('hex')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original scene draw queues: ${cases.length}; complete depth and sort owners; heap and copy controlled.`);
