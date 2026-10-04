'use strict';
// Execute original callback through its first primitive-color emission. Metadata
// and decoded texture services are controlled; no rasterization or body scaling.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022f2cc,rom.subarray(0x29eafc,0x29f5e8));
const actorAddress=0x80300000,display=0x80310000,sp=0x803ffe00;
m.trap(0x80233800,c=>{assert.equal(c.get('a2'),0);c.set('v0',1);});
m.trap(0x80232ee8,c=>c.set('v0',0x80320000));
const cases=[];
for(const alpha of [0,128,255])for(const shadow of [0,1])for(const layerAlpha of [0,1,127,255])
for(const opacity of [0,32,255])for(const global of [[255,255,255],[255,0,0],[-80,50,350]]){
 const actor=Buffer.alloc(336);actor[0]=layerAlpha;actor[0x13c]=opacity;actor[0x142]=100;actor[0x143]=200;actor[0x144]=255;
 m.load(actorAddress,actor);m.load(display,Buffer.alloc(64));m.w32(0x800e9ba0,display);
 for(let i=0;i<3;i++)m.w16(0x80220e70+i*2,global[i]);
 m.set('a0',actorAddress);m.set('a1',alpha);m.set('a2',shadow);m.set('sp',sp);m.call(0x8022f2cc,0x8022f5e8);
 assert.equal(m.r32(display),0xfa000000);assert.equal(m.r32(0x800e9ba0),display+8);
 const color=m.r32(display+4);cases.push({actor:actor.toString('hex'),alpha,shadow,global,color});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Actor primitive colors: ${cases.length}; original callback prefix, metadata/texture controlled.`);
