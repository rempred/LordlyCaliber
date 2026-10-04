'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decode=k=>Buffer.from(OB64.art.readCompressedResource(rom,k).decoded);
const handles=raw(0x315736),directory=raw(0x3b6cd0);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022d7d0,rom.subarray(0x29d000,0x2a8d00));m.load(0x802395c0,rom.subarray(0x2a8d20,0x2ae338));
m.load(0x801bdb70,rom.subarray(0x201000,0x212f00));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(0x801d0458,rom.subarray(0x2138e8,0x213990));
m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
const scene=0x80300000,cast=0x80303000,actors=0x80310010,table=0x80320000,
 objects=0x80330000,program=0x80340010,pool=0x80350000,alloc=0x80360010,sp=0x803ffe00;
m.load(table,handles);m.w32(0x801d0690,table+handles.readUInt16BE(0));m.w32(0x801d0698,table+handles.readUInt16BE(2));
m.w32(0x801d0680,table+handles.readUInt16BE(4));m.w32(0x801d0728,pool);
m.w32(0x8022a974,scene);m.w32(0x801ce8bc,cast);m.w32(0x801ce8fc,0x3c);
let allocations=[];
m.trap(0x80070f30,c=>{const size=c.get('a0');assert([4,16,29,336,514].includes(size));const p=alloc+allocations.length*1024;assert(p+size<0x803f0000);
 allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x800712c4,c=>assert(allocations.some(a=>a.p===c.get('a0'))));
m.trap(0x801c27f8,c=>c.set('v0',c.get('a0'))); // Actual registration owner runs; native allocation handles are controlled.
m.trap(0x801c2ce4,c=>{const desc=raw(directory.readUInt32BE((c.get('a0')-1)*4));
 m.load(program,decode(desc.readUInt32BE(4)));c.set('v0',program);});
m.trap(0x801c5938,()=>{}); // Deferred linked audio sink.
m.trap(0x801bc35c,c=>c.setSingle(0,0)); // Explicit flat terrain.
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
function record(slot,source,member,identity,x,z){
 const b=Buffer.alloc(336,0xa7);b.writeInt32BE(slot,0xe4);b.writeInt32BE(source,0xe8);b.writeInt32BE(source,0xec);
 b.writeInt32BE(0,0xf0);b.writeInt32BE(0,0xf4);b.writeInt16BE(0,0x134);b.writeInt16BE(0,0x138);b.writeInt16BE(0,0x13a);
 b[0x13d]=1;b[0x145]=0;b[0x146]=0;b[0x147]=identity;b[0x149]=member;
 b.writeFloatBE(x,0x11c);b.writeFloatBE(7.5,0x120);b.writeFloatBE(z,0x124);return b;
}
const results=[];
function run(group,kind){
 const initial=[],rows=Array.from({length:28},()=>Buffer.alloc(248)),objs=[];
 // Four independent army rows exercise both side/subgroup flags. Paired bodies
 // add the secondary in slot 27, ensuring shared-row propagation is observed.
 for(let i=0;i<4;i++){
  const source=kind===1?10:kind===2?32:30;
  rows[i].writeUInt32BE((i&1?0x100:0)|(i&2?0x200:0),0x40);rows[i].writeUInt32BE(source,0x48);rows[i].writeUInt32BE(source,0x4c);
  for(let member=0;member<(kind===1&&i===0?2:1);member++){
   const slot=member?27:i,object=objects+i*128+member*64,b=Buffer.alloc(48,0xa7),x=20+i*50,z=10-i*30;
   b.writeInt16BE(x,0x28);b.writeInt16BE(z,0x2c);rows[i].writeUInt32BE(object,member*4);objs.push({address:object,record:b.toString('hex')});
   const r=record(slot,source,member,i,x+(kind===3?(i===0?.05:0):i&1?-40:40),z+(kind===3?0:4));
   initial.push({slot,record:r.toString('hex')});
  }
 }
 // Unlinked ordinary actor must be ignored by filtered selection.
 const extra=record(12,30,0,255,99,99);extra[0x13d]=0;extra.writeInt32BE(1000,0xf4);initial.push({slot:12,record:extra.toString('hex')});
 m.load(scene,Buffer.alloc(0x1cc0));m.load(pool,Buffer.alloc(20*0xb0));allocations=[];
 rows.forEach((r,i)=>m.load(cast+0x1c4+i*248,r));objs.forEach(o=>m.load(o.address,Buffer.from(o.record,'hex')));
 initial.forEach(a=>{const p=actors+a.slot*512;m.load(p-16,Buffer.alloc(368,0xa7));m.load(p,Buffer.from(a.record,'hex'));m.w32(scene+24+a.slot*4,p);});
 m.w8(0x8018fc19,2);m.w32(0x80197b14,8);m.set('sp',sp);
 m.set('a0',group);try{m.call(0x8023d878);}catch(e){throw Error(`group=${group} kind=${kind} args=${['a0','a1','a2','ra','sp'].map(r=>m.get(r).toString(16))}: ${e.message}`);}
 const states=[];
 for(let step=0;step<80;step++){
  if(step){m.call(0x802316c0);for(const a of initial){m.set('a0',actors+a.slot*512);m.call(0x8022e9e8);}m.call(0x8023dac4);}
  const p=m.r32(scene+0x1ca8);m.call(0x8023dcc0);
  const actorsAfter=initial.map(a=>({slot:a.slot,record:read(actors+a.slot*512,336)}));
  const motions=initial.map(a=>{const p=m.r32(scene+0xf8+a.slot*4);return p?{slot:a.slot,record:read(p,16)}:null;}).filter(Boolean);
  states.push({presentation:p?read(p,29):'',active:m.get('v0'),actors:actorsAfter,motions});
  assert.equal(m.get('sp'),sp);
  for(const a of allocations)for(let i=0;i<16;i++){assert.equal(m.r8(a.p-16+i),0xa7);assert.equal(m.r8(a.p+a.size+i),0xa7);}
 }
 results.push({group,kind,initial,rows:rows.map(r=>r.toString('hex')),objects:objs,states});
}
for(const group of [-2147483648,-4,-1,0,1,2,3,4,5,6,2147483647])for(let kind=0;kind<4;kind++)run(group,kind);
// Direct updater boundaries: all state bytes, completion variant, paired owner
// and motion countdowns. Busy helper and body initialization execute original code.
const boundaries=[];
for(const phase of [0,1,2,3,4,5,255])for(const variant of [0,1,255])for(const owner of [30,10])for(const delay of [-1,0,1,2]){
 const r=record(27,owner,owner===10?1:0,0,20,10),row=Buffer.alloc(248),obj=Buffer.alloc(48);
 row.writeUInt32BE(owner,0x48);row.writeUInt32BE(owner,0x4c);row.writeUInt32BE(objects,(owner===10?1:0)*4);obj.writeInt16BE(20,0x28);obj.writeInt16BE(10,0x2c);
 // State zero/cursor -1 asks the native counted decoder for its end sentinel.
 r.writeInt32BE(-1,0xf0);r.writeInt32BE(delay,0xf4);
 const vector=Buffer.alloc(29,4);vector[27]=phase;vector[28]=variant;
 m.load(scene,Buffer.alloc(0x1cc0));m.load(pool,Buffer.alloc(20*0xb0));m.load(actors,r);m.load(cast+0x1c4,row);m.load(objects,obj);
 m.w32(scene+24+27*4,actors);allocations=[{p:alloc,size:29}];m.load(alloc,vector);m.w32(scene+0x1ca8,alloc);m.set('sp',sp);
 const states=[];
 for(let step=0;step<2;step++){m.call(0x8023dac4);const p=m.r32(scene+0x1ca8);states.push({presentation:p?read(p,29):'',actor:read(actors,336)});}
 boundaries.push({initial:r.toString('hex'),row:row.toString('hex'),object:obj.toString('hex'),address:objects,vector:vector.toString('hex'),states});
}
const speeds=[];
for(let source=0;source<165;source++)for(const owner of new Set([source,30,rom[0x5db14+source*72+69]]))for(let direction=0;direction<2;direction++){
 if(owner>=172)continue;
 m.set('a0',source);m.set('a1',owner);m.call(direction?0x801be264:0x801be1e0);speeds.push({source,owner,direction,value:m.single(0)});
}
// Entrance resource allocation is supplied at the ordinary decompression boundary.
m.trap(0x8009dd38,c=>{assert.equal(c.get('a0'),0x1a9045a);const b=decode(c.get('a0')),
 p=alloc+allocations.length*1024;allocations.push({p,size:b.length});m.load(p-16,Buffer.alloc(b.length+32,0xa7));m.load(p,b);c.set('v0',p);});
function memberAddress(id,bank){
 return bank===0?(id<100?[0x80193bd8+id*56,0x80193bf3+id*56]:[0x80190ebc+id*2,0x80191010+id]):
 (id<100?[0x80195578+id*52,0x80195593+id*52]:[0x8019532c+id*2,0x80195480+id]);
}
const entrances=[];
for(const preset of [0,1,7,63])for(let kind=0;kind<5;kind++){
 const rows=Array.from({length:28},()=>Buffer.alloc(248)),objs=[],initial=[],sideGroups=[29,30],members=[[],[]],overrideFlag=kind===4?1:0;
 const sources=[30,10,135,135,161,136],ids=[0,99,100,164,255,1];
 for(let i=0;i<sources.length;i++){
  rows[i].writeUInt32BE(sources[i],0x48);rows[i].writeUInt32BE(sources[i],0x4c);rows[i].writeUInt32BE(i&1?0x100:0,0x40);rows[i][0xf6]=ids[i];
  const count=i===0?3:i===1?2:1;
  for(let member=0;member<count;member++){
   if(kind===2&&i===0&&member===0)continue;
   const address=objects+i*192+member*64,b=Buffer.alloc(48,0xa7);b.writeInt16BE(240+i*50,0x28);b.writeInt16BE(19+member,0x2a);b.writeInt16BE(20+member*25,0x2c);
   rows[i].writeUInt32BE(address,member*4);objs.push({address,record:b.toString('hex')});
  }
  for(let bank=0;bank<2;bank++){
   const value=(i+bank)%3===0?0:123,flags=(i+bank)%3===1?4:0;
   members[bank].push({id:ids[i],value,flags});const [va,fa]=memberAddress(ids[i],bank);m.w16(va,value);m.w8(fa,flags);
  }
 }
 if(kind===1){initial.push({slot:12,record:record(12,30,0,0,190,20).toString('hex')});initial.push({slot:26,record:record(26,135,0,2,100,100).toString('hex')});}
 m.load(scene,Buffer.alloc(0x1cc0));m.load(pool,Buffer.alloc(20*0xb0));allocations=[];
 rows.forEach((r,i)=>m.load(cast+0x1c4+i*248,r));objs.forEach(o=>m.load(o.address,Buffer.from(o.record,'hex')));
 initial.forEach(a=>{const p=actors+a.slot*512;m.load(p,Buffer.from(a.record,'hex'));m.w32(scene+24+a.slot*4,p);});
 const variant=kind===3?-3:kind===4?-10:0;
 m.w32(0x801ceab0,preset);m.w8(0x8022ac80,variant);m.w8(0x8022a7f0,overrideFlag);m.w8(0x801976dc,sideGroups[0]);m.w8(0x801976e8,sideGroups[1]);
 m.w8(0x8018fc19,2);m.w32(0x80197b14,8);m.w8(0x8022a980,0x67);m.set('sp',sp);
 // Bootstrap construction narrows the terrain result; the normal movement updater does not.
 m.trap(0x801bc35c,c=>c.setSingle(0,7.75));
 try{m.call(0x8023b500,0x80000100,1000000);}catch(e){throw Error(`entrance preset=${preset} kind=${kind}: ${e.message}`);}
 const states=[];
 for(let step=0;step<100;step++){
  if(step){m.call(0x802316c0);for(let slot=0;slot<28;slot++){const p=m.r32(scene+24+slot*4);if(p){m.set('a0',p);m.call(0x8022e9e8);}}m.call(0x8023dac4);}
  const p=m.r32(scene+0x1ca8);
  const actorStates=Array.from({length:28},(_,slot)=>{const p=m.r32(scene+24+slot*4);return p?{slot,record:read(p,336)}:null;}).filter(Boolean);
  const motions=Array.from({length:28},(_,slot)=>{const p=m.r32(scene+0xf8+slot*4);return p?{slot,record:read(p,16)}:null;}).filter(Boolean);
  states.push({actors:actorStates,motions,presentation:p?read(p,29):'',opacity:m.r8(0x8022a980)});
  assert.equal(m.get('sp'),sp);
  for(const a of allocations)for(let i=0;i<16;i++){assert.equal(m.r8(a.p-16+i),0xa7);assert.equal(m.r8(a.p+a.size+i),0xa7);}
 }
 entrances.push({preset,kind,variant,overrideFlag,sideGroups,members,rows:rows.map(r=>r.toString('hex')),objects:objs,initial,states});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results,boundaries,speeds,entrances}));
console.log(`Original presentation: ${results.length} sequences / ${results.length*80} snapshots, ${boundaries.length} boundary sequences, ${speeds.length} speed samples. Allocation/resource handles/terrain/audio controlled.`);

console.log(`Original entrances: ${entrances.length} sequences / ${entrances.length*100} complete snapshots.`);
