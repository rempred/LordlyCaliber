'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8019d404,rom.subarray(0xea184,0xea628));
const payload=0x80300000,shared=0x800e7a30,coordinates=[0,1,-1,32767,-32768,40,160,319,60000],results=[];
let sample=0;
for(const start of coordinates)for(const step of [0,1,5,32767,32768,65535])for(const margin of [0,1,127,128,255]){
 const state=Buffer.alloc(1144),window=Buffer.alloc(168);for(let i=0;i<state.length;i++)state[i]=(i*19+sample*17)&255;for(let i=0;i<window.length;i++)window[i]=(i*31+sample*13)&255;
 const half=(b,at,value)=>b.writeUInt16BE(value&65535,at);
 half(state,0x24,start);half(state,0x26,coordinates[(sample+1)%coordinates.length]);half(state,0x28,coordinates[(sample+3)%coordinates.length]);half(state,0x2a,coordinates[(sample+5)%coordinates.length]);half(state,0x38,step);state[0x3e]=sample%3;state[0x40]=margin;
 half(state,0x20,[0,1,200,32767,32768,65535][sample%6]);half(state,0x1a,[0,1,36,100,65535][sample%5]);half(state,0x14,sample%2?-1:84);state[0x47]=[0,1,2,255][sample%4];state[0x3a]=(sample>>1)%2;state[0x3d]=(sample>>2)%2;window[0x91]=(sample>>3)%2?4:0;half(window,0x8c,coordinates[(sample+7)%coordinates.length]);
 m.load(payload,state);m.load(shared,window);m.w32(0x8019e194,payload);const snapshots=[];
 for(let update=0;update<8;update++){
  m.set('sp',0x803ffe00);m.call(0x8019d404);m.call(0x8019d5d0);m.call(0x8019d50c);
  snapshots.push({state:Buffer.from(m.ram.subarray(m.phys(payload),m.phys(payload)+1144)).toString('hex'),window:Buffer.from(m.ram.subarray(m.phys(shared),m.phys(shared)+168)).toString('hex')});
 }
 results.push({state:state.toString('hex'),window:window.toString('hex'),snapshots});++sample;
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} motion sequences / ${results.length*8} full window+payload comparisons; no helpers controlled.`);
