'use strict';
// Original body registration, metadata selection and image loader. Allocation,
// resource transfer and final pixel conversion are controlled; selection is not.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decoded=new Map();
const decode=k=>{if(!decoded.has(k))decoded.set(k,Buffer.from(OB64.art.readCompressedResource(rom,k).decoded));return decoded.get(k);};
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x801bdb70,rom.subarray(0x201000,0x213a00));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const handles=raw(0x315736),tables=0x80280000,pool=0x80300010,request=0x80302010,sp=0x803ffe00;
m.load(tables,handles);m.w32(0x801d0690,tables+handles.readUInt16BE(0));m.w32(0x801d0680,tables+handles.readUInt16BE(4));m.w32(0x801d0728,pool);
m.load(0x801ceee0,Buffer.from([1,3,3,2,2,2,1,1,1,1,1,1,4,3,3,2,3,3,3,3,3,4]));
let heap=0,resourceKeys=new Map(),conversion;
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const alloc=n=>{const p=heap;heap+=(n+15)&~15;assert(heap<0x803f0000);return p;};
for(const p of [0x80071c04,0x80070f30])m.trap(p,c=>c.set('v0',alloc(c.get('a0'))));
m.trap(0x8009daf4,c=>c.set('v0',raw(c.get('a0')).length));
m.trap(0x8009dbb8,c=>m.load(c.get('a0'),raw(c.get('a1'))));
for(const p of [0x8009dd38,0x8009df48])m.trap(p,c=>{const key=c.get('a0'),bytes=decode(key),ptr=alloc(bytes.length);m.load(ptr,bytes);resourceKeys.set(ptr,key);c.set('v0',ptr);});
m.trap(0x800712c4,()=>{});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80093060,c=>m.load(c.get('a1'),read(c.get('a0'),c.get('a2'))));
for(const p of [0x801c15e0,0x801c0f50])m.trap(p,c=>{
 conversion={key:resourceKeys.get(c.get('a0')),child:c.get('a1'),paletteOffset:c.get('a2')-m.r32(pool+0x6c),flags:m.r32(c.get('sp')+16),sixteen:p===0x801c15e0};
 assert(conversion.key);c.set('v0',0x80270000);
});
const call=(p,args)=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p,0x80000100,500000);assert.equal(m.get('sp'),sp);return m.get('v0');};
const rows=[];let replacements=0;
for(let source=1;source<165;source++)for(let flags=0;flags<4;flags++) {
 const a=flags>>1,b=flags&1;
 const items=[2,32,57,63,81,123,164].includes(source)?Array.from({length:278},(_,i)=>i):[0,1,20];
 for(const item of items) {
  heap=0x80310000;resourceKeys=new Map();m.load(pool-16,Buffer.alloc(20*176+32,0));m.w8(0x80193c12,3);
  assert.equal(call(0x801c27f8,[pool,source,source,a,b,item]),pool);
  const metadata=m.r32(pool+0x48),frameCount=m.r32(pool+0x50),baseHeap=heap;
  const tokens=new Set([0]);for(const off of [0xa8,0xac]){const token=m.r16(pool+off);if(token!==65535){tokens.add(token);replacements++;}}
  // Keep the all-item cross-product bounded; all routes still cover substitutions.
  if(items.length===278 && item!==0)for(const token of [...tokens])if(token!==0)tokens.delete(token);
  for(const token of tokens)for(const sepia of [0,1]) {
   assert(token<frameCount);const layers=[];
   for(let layer=0;layer<16;layer++) {
    m.load(request-16,Buffer.alloc(36+32,0xa7));
    if(!call(0x801c1ff4,[source,source,a,b,token,layer,request]))break;
    const record=read(request,36),art=record.readUInt32BE(0);
    // Force cold-load paths so expected child/palette selection is observable.
    const count=m.r32(pool+0x18);for(const offset of [0x38,0x3c,0x40])m.load(m.r32(pool+offset),Buffer.alloc(count*4));
    m.w8(0x801cef00,2|sepia);heap=baseHeap;conversion=undefined;
    assert.equal(call(0x801c2eb0,[source,source,request,b,a,item,0]),0x80270000);assert(conversion);
    const desc=m.r32(pool+8);assert.equal(conversion.key,m.r32(desc+16+art*4));
    assert(read(request-16,16).equals(Buffer.alloc(16,0xa7)));assert(read(request+36,16).equals(Buffer.alloc(16,0xa7)));
    layers.push({record:record.toString('hex'),...conversion});
   }
   rows.push({source,flags,item,token,sepia,layers});
  }
 }
}
// Conversion itself has no resource dependencies. Compare every 16-bit input.
const warm=Buffer.alloc(65536*2);for(let color=0;color<65536;color++)warm.writeUInt16BE(call(0x801bedc8,[color]),color*2);
fs.writeFileSync(process.argv[3],JSON.stringify({rows,warm16:warm.toString('hex')}));
console.log(`${rows.length} original body image selections; ${replacements} substitution routes; all 65536 warm-tone RGBA16 values. Allocation/resource transfer/pixel expansion controlled.`);
