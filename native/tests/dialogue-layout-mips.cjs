'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80198be8,rom.subarray(0xe5968,0xe659c));m.load(0x8019dbb0,rom.subarray(0xea930,0xeacc0));m.load(0x8019ecd0,rom.subarray(0xeba50,0xebba8));
const shared=0x800e7a30,payload=0x800e91d0,archive=0x80310000,text=archive+4,sp=0x803ffe00;
let measuredWidth=0,measuredLines=0,warnings=0;
m.trap(0x80093380,c=>{assert.equal(c.get('a0'),payload);assert.equal(c.get('a1'),1144);m.load(payload,Buffer.alloc(1144));});
m.trap(0x8007938c,c=>{assert.equal(c.get('a0'),0x1a3b7b2);assert.equal(c.get('a2'),0xfffffffc);assert.equal(c.get('a3'),0);c.set('v0',archive);});
m.trap(0x8019cb3c,c=>{assert.equal(c.get('a0'),text);assert.equal(c.get('a1'),0);c.set('v0',measuredWidth);});
m.trap(0x8019c7a8,c=>{assert.equal(c.get('a0'),text);c.set('v0',measuredLines);});
m.trap(0x80093540,c=>{const format=c.get('a0');assert([0x8019e1f8,0x8019e20c].includes(format));warnings|=format===0x8019e1f8?1:2;});
const sources=['Body\0','@*Body\0','@$Body\0','@-Body\0','@f084Body\0','@F084Body\0','@v036@y003Body\0','@$@v000@y000Body\0','@x@u@gBody\0','@<001Body\0','@$@y999Body\0','@<255@r@l3@S7Body\0'];
const results=[];
for(const source of sources)for(const flags of [0,4,8,12,0x40,0x44,0x80,0xff])for(let sample=0;sample<12;sample++){
 const initial=Buffer.alloc(168);for(let i=0;i<168;i++)initial[i]=(i*31+sample*17)&255;
 initial.writeUInt16BE(0xc800,0);initial.writeUInt32BE(0x80198be8,16);initial.writeUInt16BE(0,0x88);initial[0x8a]=flags;
 initial[0x8b]=[0,3,4,7,8,0xa0,0xff][sample%7];initial[0x91]=[0,4,8,12][sample%4];initial.writeUInt16BE([0,1,84,0xffac,998,999,1000,0x8000,0xffff][sample%9],0x92);
 initial.writeUInt16BE([0,1,100,0xffff,0xfff0][sample%5],0x8c);initial.writeUInt16BE([0,1,200,0x8000,0xfff0][sample%5],0x94);
 measuredWidth=[0,1,6,7,251,252,65535][sample%7];measuredLines=[0,1,3,65535][sample%4];warnings=0;
 m.load(shared,initial);m.load(payload,Buffer.alloc(1144,0xa7));m.w32(0x8019e194,payload);m.w32(archive,4);m.load(text,Buffer.from(source));m.set('sp',sp);m.set('a0',sample%6);m.call(0x80198be8);
 const state=Buffer.from(m.ram.subarray(m.phys(payload),m.phys(payload)+1144));state.writeUInt32BE((state.readUInt32BE(4)-text)>>>0,4);state.writeUInt32BE((state.readUInt32BE(8)-text)>>>0,8);
 results.push({source:Buffer.from(source).toString('hex'),initial:initial.toString('hex'),slot:sample%6,width:measuredWidth,lines:measuredLines,state:state.toString('hex'),window:Buffer.from(m.ram.subarray(m.phys(shared),m.phys(shared)+168)).toString('hex'),warnings});
 assert.equal(m.get('sp'),sp);
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} original dialogue initializations, full window and 1144-byte payload. Archive pointer, zero-fill, measurements and diagnostic output controlled; prefix parser executes.`);
