'use strict';
// Original body registration + idle-program traversal + frame bounds. Only
// allocation and ROM resource transfer are host services; no geometry/timing
// answer is supplied by the editor under test.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const fixture=process.argv[4]?JSON.parse(fs.readFileSync(process.argv[4],'utf8')):{};
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decoded=new Map();
const decode=k=>{if(!decoded.has(k))decoded.set(k,Buffer.from(OB64.art.readCompressedResource(rom,k).decoded));return decoded.get(k);};
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x801bdb70,rom.subarray(0x201000,0x213a00));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
if(fixture.classSize)m.load(fixture.classAddress,rom.subarray(fixture.classOffset,fixture.classOffset+fixture.classSize));
const handles=raw(fixture.key||0x315736),tables=0x80280000,pool=0x80300010,actor=0x80271000,sp=0x803ffe00;
m.load(tables,handles);m.w32(0x801d0690,tables+handles.readUInt16BE(0));m.w32(0x801d0680,tables+handles.readUInt16BE(4));m.w32(0x801d0728,pool);
let heap=0;
const alloc=n=>{const p=heap;heap+=(n+15)&~15;assert(heap<0x803f0000);return p;};
for(const p of [0x80071c04,0x80070f30])m.trap(p,c=>c.set('v0',alloc(c.get('a0'))));
m.trap(0x8009daf4,c=>c.set('v0',raw(c.get('a0')).length));
m.trap(0x8009dbb8,c=>m.load(c.get('a0'),raw(c.get('a1'))));
for(const p of [0x8009dd38,0x8009df48])m.trap(p,c=>{const bytes=decode(c.get('a0')),ptr=alloc(bytes.length);m.load(ptr,bytes);c.set('v0',ptr);});
m.trap(0x800712c4,()=>{});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80093060,c=>m.load(c.get('a1'),Buffer.from(m.ram.subarray(m.phys(c.get('a0')),m.phys(c.get('a0'))+c.get('a2')))));
const call=(p,args)=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p,0x80000100,500000);assert.equal(m.get('sp'),sp);return m.get('v0');};
const rows=[];
const contexts=[];
for(let source=1;source<165;source++)for(const current of new Set([rom[0x5db14+source*72+69]||source,5]))for(let flags=0;flags<4;flags++)contexts.push([source,current,flags]);
for(const [source,current,flags] of fixture.contexts||contexts){
 heap=0x80310000;m.load(pool-16,Buffer.alloc(20*176+32));m.w8(0x80193c12,3);
 const a=flags>>1,b=flags&1;
 if(source!==0x101)assert.equal(call(0x801c27f8,[pool,source,current,a,b,0]),pool);
 m.load(actor,Buffer.alloc(0xf8));m.w32(actor+0x40,(a<<10)|(b<<8));m.w32(actor+0x48,source);m.w32(actor+0x4c,current);m.w32(actor+0x50,m.r32(pool));
 const height=call(0x801ca100,[actor])|0;
 rows.push([source,current,flags,height]);
}
fs.writeFileSync(process.argv[3],JSON.stringify(rows));
console.log(`${rows.length} native idle-height lookups; registration, idle traversal and bounds executed. Allocation/resource transfer controlled.`);
