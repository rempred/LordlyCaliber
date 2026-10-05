'use strict';
// Original materializer, constructor, initializer, pose and leader normalizer.
// Host services control resource allocations, terrain and the draw/audio sinks.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decode=k=>Buffer.from(OB64.art.readCompressedResource(rom,k).decoded);
const handles=raw(0x315736),directory=raw(0x3b6cd0);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022d7d0,rom.subarray(0x29d000,0x2a8d00));m.load(0x802395c0,rom.subarray(0x2a8d20,0x2ae338));
m.load(0x801bdb70,rom.subarray(0x201000,0x212f00));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(0x801d0458,rom.subarray(0x2138e8,0x213990));m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
const scene=0x80300000,cast=0x80301010,objects=0x80310000,table=0x80320000,program=0x80340010,pool=0x80350000,arena=0x80360010,sp=0x803ffe00;
m.load(table,handles.subarray(handles.readUInt16BE(0),handles.readUInt16BE(2)));
m.w32(0x801d0690,table);m.w32(0x801d0680,table+0x1000);m.w32(0x801d0728,pool);
m.w32(0x8022a974,scene);m.w32(0x801ce8bc,cast);m.w32(0x801ce8fc,60);
let cursor,allocations,draws;
m.trap(0x80070f30,c=>{const size=c.get('a0'),p=cursor;cursor+=((size+15)&~15)+32;assert(cursor<0x803f0000);allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x801c27f8,c=>c.set('v0',c.get('a0')));
m.trap(0x801c2ce4,c=>{const desc=raw(directory.readUInt32BE((c.get('a0')-1)*4));m.load(program,decode(desc.readUInt32BE(4)));c.set('v0',program);});
m.trap(0x801c5938,()=>{});
m.trap(0x801bc35c,c=>c.setSingle(0,123.75));
m.trap(0x8022eedc,c=>draws.push(m.r32(c.get('a0')+0xe4)));
// Availability is a declared input here. Separate entrance tests cover its fields.
m.trap(0x8023de14,c=>c.set('v0',m.r8(c.get('a0')+0xf6)===9?1:0));
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const results=[];
function run(first,second,context,scenario,variant,kind) {
 cursor=arena;allocations=[];draws=[];m.load(scene,Buffer.alloc(0x1cc0));m.load(pool,Buffer.alloc(20*0xb0));
 const rows=Array.from({length:20},()=>Buffer.alloc(248)),objs=[];
 const sources=kind?[10,0x87,0x87,0xa1]:[0x51,0x53,30,31];
 for(let i=0;i<4;++i) {
  const r=rows[i],source=sources[i],flags=(i&1?0x100:0)|(i<2?0x200:0)|(kind===2?2:0);
  // Repeated87 constructs its secondary pose50. Depth<4 selects its complete
  // special body; the opposite route's 46-entry resource cannot hold pose50.
  r.writeUInt32BE(source,0x48);r.writeUInt32BE(source,0x4c);r.writeUInt32BE(flags,0x40);r.writeInt32BE(source===0x87?3:i*3,0x58);r[0xf6]=i===1?9:i+1;
  for(let j=0;j<(source===10?2:1);++j) {
   const address=objects+i*128+j*64,b=Buffer.alloc(48);b.writeInt16BE(10+i*25+j*7,0x28);b.writeInt16BE(17+i,0x2a);b.writeInt16BE(-20+i*10+j*6,0x2c);
   r.writeUInt32BE(address,j*4);m.load(address,b);objs.push({address,record:b.toString('hex')});
  }
 }
 rows.forEach((r,i)=>m.load(cast+0x1c4+i*248,r));
 m.w8(0x8022ac80,context);m.w16(0x8022a994,scenario);m.set('sp',sp);m.set('a0',first);m.set('a1',second);m.set('a2',variant);
 try{m.call(0x8023c3dc,0x80000100,1000000);}catch(e){throw Error(JSON.stringify({first,second,context,scenario,variant,kind})+': '+e.message);}
 assert.equal(m.get('sp'),sp);const actors=[];
 for(let slot=0;slot<28;++slot){const p=m.r32(scene+24+slot*4);if(p)actors.push({slot,record:read(p,336)});}
 for(const {p,size} of allocations)for(let i=0;i<16;++i){assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+size+i),0xa7);}
 results.push({first,second,context,scenario,variant,kind,rows:rows.map(r=>r.toString('hex')),objects:objs,actors,draws});
}
for(const context of [-9,-6,-3,-10,0])for(const variant of [0,1])for(const kind of [0,1,2])run(-1,-1,context,0,variant,kind);
for(const scenario of [0x3d9,0x21f,0xc6,0xf5,0x1e4,0x1e5])for(const variant of [0,1])run(-1,-1,-9,scenario,variant,0);
for(const [first,second] of [[0,0],[-2,0],[0,-2],[0,0x51],[0x51,0],[0x151,-1],[30,31],[-2,-2]])run(first,second,-9,0,0,0);
fs.writeFileSync(process.argv[3],JSON.stringify({results}));console.log(`${results.length} original roster constructions/poses/leader normalizations`);
