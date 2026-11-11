'use strict';
// The original resident audio callbacks may run before the game heap exists.
// Execute the exported class boot and audio installer; model DMA/heap/interrupt
// services, and execute the original cache routines. Full-console boot is separate.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const read=p=>{const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;};
const rom=read(process.argv[2]),retail=read(process.argv[3]),cp=0x5dc80,ap=0x27c5300;
const sites=[[0x126ac,0x800822ac],[0x12ee8,0x80082ae8],[0x1299c,0x8008259c]];
for(const [at]of sites)assert(rom.subarray(at,at+8).equals(retail.subarray(at,at+8)),`Resident audio hook ROM${at.toString(16)} must stay native before the allocation loads`);
const m=new Mips(),data=rom.readUInt32BE(cp+12),size=rom.readUInt32BE(cp+16),base=rom.readUInt32BE(cp+24),end=rom.readUInt32BE(cp+48),boot=rom.readUInt32BE(cp+60);
m.load(0x80070000,rom.subarray(0x400,0x40000));
m.load(0x80187c14,rom.subarray(0x5db14,0x6097c));
const hookAt=rom.readUInt32BE(ap+12)+rom.readUInt32BE(ap+60)-rom.readUInt32BE(ap+16),n=rom.readUInt32BE(ap+64),expected=new Map();
for(let i=0;i<n;i++){const p=hookAt+i*24;if(rom.readUInt32BE(p+20))expected.set(rom.readUInt32BE(p)+0x8006fc00,(0x08000000|((rom.readUInt32BE(p+12)>>>2)&0x3ffffff))>>>0);}
assert.equal(expected.size,3);
let dma=0,masked=false,writes=0;const caches=[];
// The boot entry's first call is its data-load service, including expanded saves.
let dmaService=0;for(let at=boot;at<boot+80;at+=4){const w=m.r32(at);if(w>>>26===3){dmaService=(0x80000000|((w&0x3ffffff)<<2))>>>0;break;}}
assert(dmaService);
m.trap(dmaService,c=>{
 assert.equal(c.get('a0'),data);assert.equal(c.get('a1'),base);assert.equal(c.get('a2'),size);
 for(const [at,ram]of sites)assert.equal(c.r32(ram),retail.readUInt32BE(at));
 c.load(base,rom.subarray(data,data+size));dma++;
});
m.trap(0x800996d0,c=>{assert.equal(dma,1);assert(!masked);masked=true;c.set('v0',0x1234);});
m.trap(0x80099740,c=>{assert(masked);assert.equal(c.get('a0'),0x1234);masked=false;});
m.trap(0x80070c60,c=>{assert(!masked);assert.equal(c.get('a0'),end);assert.equal(c.get('a1'),0x80400000-end);});
m.trap(0x80070d20,()=>{});
const write=m.w32.bind(m);m.w32=(a,v)=>{if(expected.has(a)||expected.has(a-4)){assert(masked);assert.equal(dma,1);writes++;}write(a,v);};
const exec=m.exec.bind(m);m.exec=(pc,next)=>{if(pc===0x80090140||pc===0x800900c0){assert(masked);assert(expected.has(m.get('a0')));assert.equal(m.get('a1'),8);caches.push([pc,m.get('a0')]);}return exec(pc,next);};
m.set('sp',0x807ff000);m.set('a0',0x80243db0);m.set('a1',0x80400000-0x80243db0);m.call(boot);
assert.equal(m.get('sp'),0x807ff000);assert.equal(dma,1);assert.equal(writes,6);assert(!masked);
for(const [at,value]of expected){assert.equal(m.r32(at),value);assert.equal(m.r32(at+4),0);assert(caches.some(x=>x[0]===0x80090140&&x[1]===at));assert(caches.some(x=>x[0]===0x800900c0&&x[1]===at));}
console.log('PASS: three native audio callbacks before DMA; actual class boot installs all three only after code loads, with interrupt protection, cache synchronization and balanced stack.');
