'use strict';
// Original curve, constructor and lifecycle instructions. Resource decoding is
// independently supplied by the browser codec; allocation/libm/vertex emission
// are controlled. No curve or reveal helper is replaced.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
class RibbonMips extends FloatMips {
 exec(pc,next){const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31;
  if([34,38,42,46].includes(op)){const a=(this.r[rs]+(w<<16>>16))>>>0;assert.equal(a&3,op===38||op===46?3:0);const p=(a&~3)>>>0;if(op===34||op===38){if(rt)this.r[rt]=this.r32(p);}else this.w32(p,this.r[rt]);return[next,(next+4)>>>0];}
  return super.exec(pc,next);
 }
}
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new RibbonMips();m.ram=new Uint8Array(0x400000);
m.load(0x80232030,rom.subarray(0x2a1860,0x2a21e0));m.load(0x80239340,rom.subarray(0x2a8b70,0x2a8b98));
m.load(0x8024049c,rom.subarray(0x2b529c,0x2b5d58));m.load(0x80242624,rom.subarray(0x2b7424,0x2b7950));
m.load(0x80242f28,rom.subarray(0x2b7d28,0x2b7d50));m.load(0x80243c98,rom.subarray(0x2b8a98,0x2b8b58));
const ctx=0x80300000,root=0x80301000,sourceX=0x80304000,sourceY=0x80304100,outX=0x80305000,outY=0x80306000,sp=0x803ffe00;
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const call=(p,args=[])=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p,0x80000100,5000000);assert.equal(m.get('sp'),sp);return m.get('v0');};
let heap,points;
for(const pc of [0x80070f30,0x80071c04])m.trap(pc,c=>{const size=c.get('a0');c.set('v0',heap);m.load(heap,Buffer.alloc(size));heap+=(size+15)&~15;assert(heap<0x803f0000);});
m.trap(0x800712c4,c=>c.set('v0',0));m.trap(0x80089a10,c=>c.set('v0',0));
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80080998,c=>m.load(c.get('a0'),read(c.get('a1'),c.get('a2'))));
m.trap(0x8009cfe0,c=>c.setSingle(0,Math.atan2(c.single(12),c.single(14))));
m.trap(0x800907f0,c=>c.setSingle(0,Math.cos(c.single(12))));m.trap(0x80092db0,c=>c.setSingle(0,Math.sin(c.single(12))));
m.trap(0x80242298,c=>{}); // Vertex packing only; actual centerline/edges execute.
m.trap(0x80240ae0,c=>{const x=c.r32(c.get('a0')),y=c.r32(c.get('a1'));points.forEach((p,i)=>{c.writeFloat(x+i*4,p[0]);c.writeFloat(y+i*4,p[1]);});c.set('v0',points.length);});
function initialize(){heap=0x80310000;m.load(ctx,Buffer.alloc(0x848));m.load(root,Buffer.alloc(0x2400));m.w32(0x8022a970,ctx);m.w32(0x8022a974,root);m.w16(0x800e8100,0);}
const stored=key=>Buffer.from(OB64.art.readResource(rom,key).stored);
const directory=stored(0x1a8d7a6),rows=[];let nulls=0;
for(let group=0;group<directory.length/4;group++){
 const table=stored(directory.readUInt32BE(group*4));
 for(let entry=0;entry<table.length/4;entry++){
  const key=table.readUInt32BE(entry*4);if(!key){nulls++;continue;}
  const source=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded),markers={};points=[];
  for(let at=0;at<source.length;at+=12){const marker=source.readInt32BE(at);
   if(marker===-1)points.push([source.readInt32BE(at+4),source.readInt32BE(at+8)]);
   else {const p=[40+marker*13,22-marker*9];markers[marker]=p;points.push(p);}
  }
  initialize();points.forEach((p,i)=>{m.writeFloat(sourceX+i*4,p[0]);m.writeFloat(sourceY+i*4,p[1]);});
  call(0x80232694,[outX,outY,400,points.length,sourceX,sourceY]);
  const sampled=[read(outX,1600).toString('hex'),read(outY,1600).toString('hex')];
  const width=1+rows.length%5,palette=rows.length%4,instant=rows.length%2;
  const args=[0,group,entry,90,-1,width,palette,instant,1];
  call(0x80240ca8,args);const record=m.r32(root+0x19f4);assert(record);
  const initial=read(record,0x88).toString('hex'),steps=[];
  for(let i=0;i<45;++i){let set=null,release=null;
   if(i===8)set=[10,-1];if(i===12)set=[100,-1];if(i===17)set=[0,points.length-1];if(i===20)set=[1,-1];
   if(set)call(0x8024098c,[0,...set]);if(i===25){release=1;call(0x802408a8,[0,1]);}
   call(0x8024049c);steps.push({set,release,record:read(record,0x88).toString('hex')});
  }
  for(let i=0;i<9;++i)call(0x8024049c);assert.equal(m.r32(root+0x19f4),0);
  rows.push({group,entry,key,points,markers,args,sampled,initial,steps});
 }
}
assert.equal(rows.length,121);assert.equal(nulls,13);
fs.writeFileSync(process.argv[3],JSON.stringify({rows,nulls}));
console.log(`${rows.length} retail paths: 400-sample curves, full constructor state and45 update/retarget/fade snapshots each. Native sampler/builder/reveal code executes; browser decompression, marker coordinates, allocation, libm and vertex packing controlled.`);
