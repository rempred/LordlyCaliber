'use strict';
// Original confirmation constructors, foremost-panel control and entity animation.
// ROM text-resource decoding, heap and audio services are controlled. Drawing is not executed.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80236920,rom.subarray(0x2a6150,0x2a7010));
m.load(0x8017bfb0,rom.subarray(0x51eb0,0x5aff0));
m.load(0x802276f8,rom.subarray(0x2836c8,0x283864));
m.load(0x802393d0,rom.subarray(0x2a8c00,0x2a8d20));
m.load(0x8018fe10,rom.subarray(0x65d10,0x66e10));
const root=0x80300000,manager=0x80302000,arena=0x80303000,sp=0x803ffe00;
let cursor,allocations,audio;
m.trap(0x80070f30,c=>{const size=c.get('a0'),p=cursor;cursor+=((size+15)&~15)+32;assert(cursor<0x803f0000);allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x80071288,c=>{const size=c.get('a0'),p=cursor;cursor+=((size+15)&~15)+32;allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x800712c4,()=>{});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80080998,c=>m.load(c.get('a0'),Buffer.from(m.ram.subarray(m.phys(c.get('a1')),m.phys(c.get('a1'))+c.get('a2')))));
m.trap(0x80093060,c=>m.load(c.get('a1'),Buffer.from(m.ram.subarray(m.phys(c.get('a0')),m.phys(c.get('a0'))+c.get('a2')))));
m.trap(0x8009dd38,c=>c.set('v0',0x803f0000));
m.trap(0x800ea604,c=>audio.push(c.get('a1')));
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const invoke=(pc,args=[])=>{m.set('sp',sp);args.forEach((n,i)=>m.set('a'+i,n));m.call(pc);assert.equal(m.get('sp'),sp);};

m.load(0x8022ac90,rom.subarray(0x29a4c0,0x2a8d20));
const vm=require('node:vm'),path=require('node:path'),ctx={OB64:{}};ctx.window=ctx;vm.createContext(ctx);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),ctx);
const resource=key=>{const p=key+0x594280,n=rom.readUInt32BE(p);assert(p+4+n<=rom.length);return rom.subarray(p+4,p+4+n);};
m.trap(0x80227a18,c=>{const dir=resource(c.get('a0')),packed=resource(dir.readUInt32BE(c.get('a1')*4));const decoded=ctx.OB64.lzssDecode(packed,4,packed.readUInt32BE(0)).output;c.set('a0',decoded.length);m.traps.get(0x80070f30)(c);m.load(c.get('v0'),decoded);});

const results=[];
const layouts=[[[4,1]],[[4,2]],[[4,1],[6,2]],[[6,2],[4,1]],[[4,2],[6,2]]];
for(const layout of layouts)for(const mask of [0x8000,0x4000])for(const direction of [0,0x400,0xc00]) {
 cursor=arena;allocations=[];audio=[];
 m.load(root,Buffer.alloc(0x1cc0));m.load(manager,Buffer.alloc(216));
 m.w32(0x8022a974,root);m.w32(0x8018fdc0,manager);
 m.w8(0x801936e0,0);m.w8(0x80196aec,0);m.w16(0x800c4b28,0);m.w16(0x800e8100,0);m.w16(0x800e8700,0);
 for(const [slot,preset]of layout)invoke(0x80236920,[slot,preset]);
 const steps=[];
 const snapshot=(action,dir,released=null)=>{
  const records=[];for(let e=m.r32(manager+4);e;e=m.r32(e+4)){
   const record=read(e,232),p=m.r32(e+0xd0);let payload=null;
   if(p){const b=read(p,0x604);for(let at=0x530;at<0x5f0;at+=4)if(b.readUInt32BE(at))b.writeUInt32BE(b.readUInt32BE(at)-p,at);payload=b.toString('hex');}
   for(const at of[0,4,0xd0])record.writeUInt32BE(0,at);
   records.push({record:record.toString('hex'),payload});
  }
  const queries=[];for(const [slot]of layout){invoke(0x802276f8,[slot]);queries.push(m.get('v0')|0);}
  steps.push({action,dir,released,queries,records,audio:[...audio]});
 };
 snapshot(0,0);
 for(let tick=0;tick<80;++tick){
  audio=[];const action=[0,29,55].includes(tick)?mask:0,dir=[20,46].includes(tick)?direction:0;
  m.w16(0x800e8100,action);m.w16(0x800e8700,dir);invoke(0x80237620);snapshot(action,dir);
 }
 for(const [slot]of layout){audio=[];invoke(0x80227778,[slot]);snapshot(0,0,slot);}
 for(const {p,size} of allocations)for(let i=0;i<16;++i){assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+size+i),0xa7);}
 results.push({layout,steps});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} original confirmation sequences including overlapping owners and ordered release`);
