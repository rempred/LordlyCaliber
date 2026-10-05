'use strict';
// Exercise the unmodified body constructor, layer lookup and image selection on
// a source-exported private descriptor. Heap/resource I/O are bounded traps;
// descriptor/config/child selection and pixel conversion run original MIPS.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const token=Number(process.argv[3]),art=Number(process.argv[4]);
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decode=k=>Buffer.from(OB64.art.readCompressedResource(rom,k).decoded);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x801bdb70,rom.subarray(0x201000,0x213a00));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
// The live compound pointer uses the exported owned resource, not the retail key.
const routeKey=rom.readUInt32BE(0x27c4c00+12)-0x594280;
const handles=raw(routeKey),tables=0x80280000,pool=0x80300010,request=0x80302010,sp=0x803ffe00;
m.load(tables,handles);m.w32(0x801d0690,tables+handles.readUInt16BE(0));m.w32(0x801d0680,tables+handles.readUInt16BE(4));m.w32(0x801d0728,pool);
m.load(0x801ceee0,Buffer.from([1,3,3,2,2,2,1,1,1,1,1,1,4,3,3,2,3,3,3,3,3,4]));
let heap=0x80310000,resources=new Map(),conversion,allocations=[];
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const alloc=n=>{const p=heap+16;heap+=((n+15)&~15)+32;assert(heap<0x803f0000);allocations.push({p,n});m.load(p-16,Buffer.alloc(n+32,0xa7));return p;};
for(const p of [0x80071c04,0x80070f30])m.trap(p,c=>c.set('v0',alloc(c.get('a0'))));
m.trap(0x8009daf4,c=>c.set('v0',raw(c.get('a0')).length));
m.trap(0x8009dbb8,c=>m.load(c.get('a0'),raw(c.get('a1'))));
for(const p of [0x8009dd38,0x8009df48])m.trap(p,c=>{const key=c.get('a0'),bytes=decode(key),ptr=alloc(bytes.length);m.load(ptr,bytes);resources.set(ptr,key);c.set('v0',ptr);});
m.trap(0x800712c4,()=>{});m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80093060,c=>m.load(c.get('a1'),read(c.get('a0'),c.get('a2'))));
const exec=m.exec.bind(m);m.exec=(pc,next)=>{
 if(pc===0x801c15e0||pc===0x801c0f50)conversion={key:resources.get(m.get('a0')),child:m.get('a1'),paletteOffset:m.get('a2')-m.r32(pool+0x6c)};
 return exec(pc,next);
};
const call=(p,args)=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p,0x80000100,500000);assert.equal(m.get('sp'),sp);return m.get('v0');};
m.load(pool-16,Buffer.alloc(20*176+32));m.w8(0x80193c12,3);
assert.equal(call(0x801c27f8,[pool,2,2,0,0,1]),pool);
const descriptor=m.r32(pool+8),config=decode(m.r32(descriptor+8)),count=config.readUInt32BE(0);
assert.equal(count,art+1);assert.equal(m.r32(pool+0x1c),count);assert.equal(m.r32(pool+0x18),count+10);
assert.deepEqual(read(m.r32(pool+0x54),count),config.subarray(32,32+count));
const map=config.readUInt32BE(28);
assert.deepEqual(read(m.r32(pool+0x5c),count),config.subarray(map,map+count));
assert.equal(config[32+art],0);
m.load(request-16,Buffer.alloc(68,0xa7));
assert.equal(call(0x801c1ff4,[2,2,0,0,token,0,request]),1);
assert.equal(m.r32(request),art);
m.w8(0x801cef00,2);
const output=call(0x801c2eb0,[2,2,request,0,0,1,0]);
assert(output>=0x80310000&&output<heap);
assert.deepEqual(read(output,48),Buffer.from('3634000200030002f80107c1003f00000001ae7b110d00003634040100030002ff8000000000000044c0010000000000','hex'));
assert.deepEqual(conversion,{key:m.r32(descriptor+16+art*4),child:0,paletteOffset:config[map+art]*512});
const sprite=decode(conversion.key);assert.equal(sprite.readUInt16BE(0),0x5554);assert.equal(sprite.readUInt16BE(4),3);assert.equal(sprite.readUInt16BE(6),2);
assert.deepEqual(read(request-16,16),Buffer.alloc(16,0xa7));assert.deepEqual(read(request+36,16),Buffer.alloc(16,0xa7));
for(const {p,n} of allocations){assert.deepEqual(read(p-16,16),Buffer.alloc(16,0xa7));assert.deepEqual(read(p+n,16),Buffer.alloc(16,0xa7));}
console.log(JSON.stringify({artCount:count,frame:token,art,conversion,allocationBytes:allocations.reduce((n,a)=>n+a.n,0),allocations:allocations.length,nativeConvertedBytes:48}));
