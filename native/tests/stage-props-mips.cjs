'use strict';
// Original placement resolvers, direct pose initializer, sequencer and counted
// decoder. Resource registry, PRNG, memset/bzero and audio sink are controlled.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.join(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);
const delta=0x801add88-0x1f1218,sp=0x803ffe00,record=0x80300010,program=0x80310010,payload=0x80340000,out=0x80350010;
for(const [a,b] of [[0x1f0e04,0x1f0f5c],[0x1f1218,0x1f197c],[0x1ffe80,0x20019c],[0x201908,0x201b18],[0x2050ac,0x205220],[0x212370,0x213280]])m.load(a+delta,rom.subarray(a,b));
m.trap(0x801c2ce4,c=>c.set('v0',program));
const fill=c=>{for(let i=0;i<c.get('a2');i++)c.w8(c.get('a0')+i,c.get('a1'));c.set('v0',c.get('a0'));};
m.trap(0x8009c970,fill);m.trap(0x80093380,c=>{for(let i=0;i<c.get('a1');i++)c.w8(c.get('a0')+i,0);});
let rng=0,soundCount=0,steps=0;
const random=()=>[13,32767,0,71,111,99][rng++%6];
m.trap(0x8009c7cc,c=>c.set('v0',random()));
m.trap(0x801c5938,c=>soundCount++);
const bytes=(at,n)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+n));
function call(at,args){m.set('sp',sp);for(let i=0;i<args.length;i++)m.set('a'+i,args[i]);m.call(at,0x80000100,1000000);assert.equal(m.get('sp'),sp);steps+=m.steps;}
const raw=rom.subarray(0x8a99ba,0x8a99ba+rom.readUInt32BE(0x8a99b6));m.load(payload,raw);
const placements=[],pairs=new Map();
for(let k=0;k<2;k++){
 const base=raw.readUInt16BE(18+k*2),stride=k?26:22,size=k?36:24,entry=k?0x801be564:0x801be478;
 m.w32(k?0x801d06a0:0x801d069c,payload+base);
 for(let selector=0;selector<80;selector++)for(let index=0;;index++){
  const at=base+raw.readInt16BE(base+selector*2)+index*stride,status=raw.readInt16BE(at);
  m.load(out-16,Buffer.alloc(size+32,0xa7));call(entry,[selector,index,out]);assert.equal(m.get('v0')|0,status);
  const source=Buffer.alloc(stride);raw.copy(source,0,at,Math.min(raw.length,at+stride));
  placements.push({kind:k,selector,index,source:source.toString('hex'),resolved:bytes(out,size).toString('hex')});
  assert.equal(bytes(out-16,16).toString('hex'),'a7'.repeat(16));assert.equal(bytes(out+size,16).toString('hex'),'a7'.repeat(16));
  if(status===-1)break;
  const handle=source.readUInt16BE(status===-2?2:0),start=source.readUInt16BE(status===-2?6:4),end=status===-2?source.readUInt16BE(8):start;
  for(let state=start;state<=end;state++)pairs.set(handle+':'+state,[handle,state]);
 }
}
// Sentinel conversion and signed copy cases absent from some retail lists.
for(let k=0;k<2;k++)for(const status of [-2,7,-1]){
 const base=raw.readUInt16BE(18+k*2),stride=k?26:22,size=k?36:24,source=Buffer.alloc(stride);
 for(let i=0;i<stride;i+=2)source.writeUInt16BE((i*1027+0x8000)&65535,i);source.writeInt16BE(status,0);source.writeUInt16BE(0x8000,8);
 m.w16(payload+base,160);m.load(payload+base+160,source);m.load(out,Buffer.alloc(size,0xa7));call(k?0x801be564:0x801be478,[0,0,out]);
 placements.push({kind:k,selector:-1,index:0,source:source.toString('hex'),resolved:bytes(out,size).toString('hex')});
}
const initializations=[];
for(const flags of [0,1,2,3])for(const selector of [0,1,50,255,256,511,65535,0xffffffff]){
 const initial=Buffer.alloc(80,0xa7);initial[0x48]=flags;m.load(record,initial);rng=0;
 call(0x801ad9d4,[record,selector]);initializations.push({initial:initial.toString('hex'),selector,record:bytes(record,80).toString('hex'),random:rng});
}
for(const selector of [0,12,255,0x1000c]){
 const initial=Buffer.alloc(80,0xa7);initial[0x48]=3;m.load(record,initial);rng=0;
 call(0x801ad974,[record,selector]);initializations.push({reset:true,initial:initial.toString('hex'),selector,record:bytes(record,80).toString('hex'),random:rng});
}
function run(handle,state,data,ticks=64,flags=0,offsets=[]){
 m.load(program,data);const initial=Buffer.alloc(80,0);initial.writeUInt32BE(handle,0);initial[0x48]=flags;
 m.load(record-16,Buffer.alloc(112,0xa7));m.load(record,initial);rng=0;call(0x801ad9d4,[record,state]);
 offsets.forEach((value,i)=>m.w16(record+12+i*2,value));
 const start=bytes(record,80).toString('hex'),calls=[];
 for(let i=0;i<ticks;i++){
  soundCount=0;call(0x801add88,[record,0x100,0,0]);calls.push({record:bytes(record,80).toString('hex'),soundCount,random:rng});
 }
 assert.equal(bytes(record-16,16).toString('hex'),'a7'.repeat(16));assert.equal(bytes(record+80,16).toString('hex'),'a7'.repeat(16));
 return {handle,state,initial:start,calls};
}
const directory=0x3b6cd0+0x594284,rows=[];
for(const [handle,state] of pairs.values()){
 const descriptor=rom.readUInt32BE(directory+((handle&4095)-1)*4)+0x594284;
 const data=Buffer.from(OB64.art.readCompressedResource(rom,rom.readUInt32BE(descriptor+4)).decoded);
 rows.push(run(handle,state,data));
}
function table(programs){let offset=programs.length*4;const directory=Buffer.alloc(offset),parts=[];programs.forEach((p,i)=>{directory.writeUInt32BE(offset,i*4);const bytes=Buffer.from([p.length,...p.flat()]);parts.push(bytes);offset+=bytes.length;});return Buffer.concat([directory,...parts]);}
const custom=[];
function scenario(programs,flags=0,offsets=[]){const data=table(programs);custom.push({...run(190,0,data,32,flags,offsets),programs});}
scenario([[[2,128,127],[12,127,128,127],[13,255,100],[16,255,128],[16,0,127],[13,15,200],[21,0x34,0x12,3],[0]]],0,[32767,-32768,32767]);
scenario([[[15,1],[3,3],[0]],[[15,2],[1,7,4],[0]],[[1,9,3],[0]]]);
scenario([[[3,1],[2,1,1],[14,0,3],[1,5,4],[0]]]);
scenario([[[4,2],[2,1,1],[3,1],[5,0]]]);
scenario([[[6],[7],[8],[9],[10],[11],[17,0,7],[18,0,8],[19,0,9],[20,0,10],[1,9,2],[0]]]);
scenario(Array.from({length:55},(_,i)=>[[1,i,3],[0]]),3);
const wrapping=[];
m.trap(0x801bc450,()=>{});m.trap(0x801bdf40,()=>{});
m.w32(0x801ce8c0,0x80370000);m.w32(0x801d07e0,0);m.w32(0x801d07d4,1);m.w32(0x801d07d0,record);
const desc=rom.readUInt32BE(directory+(190-1)*4)+0x594284;
m.load(program,Buffer.from(OB64.art.readCompressedResource(rom,rom.readUInt32BE(desc+4)).decoded));
for(const [x,y,dx,dy] of [[0,0,-32768,32767],[0,0,-101,51],[100,50,1,1],[-100,-50,-1,-1],[0,0,100,-50],[0,0,0,0]]){
 const initial=Buffer.alloc(100);initial.writeInt16BE(x,0);initial.writeInt16BE(y,2);initial.writeInt16BE(-6,4);
 for(const [at,value] of [[8,-100],[10,-50],[12,100],[14,50],[16,1]])initial.writeInt16BE(value,at);
 initial.writeUInt32BE(190,20);m.load(record,initial);call(0x801ad9d4,[record+20,0]);m.w16(record+0x20,dx);m.w16(record+0x22,dy);
 const start=bytes(record,100).toString('hex'),calls=[];
 for(let i=0;i<8;i++){call(0x801bc9f0,[]);calls.push(bytes(record,100).toString('hex'));}
 wrapping.push({initial:start,calls});
}
// Perspective construction section, ROM1FBF00..1FC374. Background setup before
// this section and the shared final color reset are separate owners. Complete
// section+epilogue1212 bytes uniquely match saved RAM801B8A70; initializer172
// bytes match801CBD6C. Allocation, registration, cache warming and RGB controlled.
m.load(0x801b8a70,rom.subarray(0x1fbf00,0x1fc3bc));
m.load(0x801cbd6c,rom.subarray(0x20f1fc,0x20f2a8));
m.load(payload,raw);m.w32(0x801d06a0,payload+raw.readUInt16BE(20));
let heap=0x80380000;
m.trap(0x80070f30,c=>{c.set('v0',heap);heap=(heap+c.get('a0')+15)&~15;assert.ok(heap<0x803f0000);});
m.trap(0x801adadc,()=>{});m.trap(0x801c3f3c,()=>{});
[0x801bc76c,0x801bc838,0x801bc914].forEach((at,i)=>m.trap(at,c=>c.set('v0',[31,97,203][i])));
const programs=new Map();
m.trap(0x801c2ce4,c=>{
 const handle=c.get('a0');
 if(!programs.has(handle)){
  const d=rom.readUInt32BE(directory+((handle&4095)-1)*4)+0x594284;
  programs.set(handle,Buffer.from(OB64.art.readCompressedResource(rom,rom.readUInt32BE(d+4)).decoded));
 }
 m.load(program,programs.get(handle));c.set('v0',program);
});
function worldRecords(){
 const count=m.r32(0x801d07e0),owners=[],drawings=[];
 for(let i=0;i<count;i++){
  const owner=bytes(m.r32(0x801d07d8)+i*248,248),draw=bytes(m.r32(0x801d07dc)+i*192,192);
  owner.writeUInt32BE(i+1,0);draw.writeUInt32BE(0,0);draw.writeUInt32BE(i+1,0x40);draw.writeUInt32BE(i+1,0x90);
  owners.push(owner.toString('hex'));drawings.push(draw.toString('hex'));
 }
 return {owners,drawings};
}
const perspective=[];
for(let selector=0;selector<80;selector++){
 heap=0x80380000;rng=0;m.w32(0x801ceab0,selector);m.set('v0',0x801d07e0);m.set('sp',sp);
 m.call(0x801b8a70,0x801b8ee4,1000000);assert.equal(m.get('sp'),sp);
 const initial=worldRecords(),randomCount=rng,calls=[];
 m.w32(0x801d07d4,0);
 for(let tick=0;tick<(selector===35?128:2);tick++){call(0x801bc9f0,[]);calls.push(worldRecords().drawings);}
 perspective.push({selector,initial,randomCount,calls});
}
fs.writeFileSync(process.argv[3],JSON.stringify({placements,initializations,rows,custom,wrapping,perspective,steps}));
console.log(`${placements.length} resolver outputs; ${initializations.length} initializations; ${rows.length*64+custom.length*32} pose updates; ${steps} original instructions.`);
