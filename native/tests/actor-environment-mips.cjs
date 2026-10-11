'use strict';
// Complete original world environment routine; no controlled helper calls.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023e83c,rom.subarray(0x2adf9c,0x2ae1a4));
const actor=0x80300000,table=0x80301000,display=0x80302000,cases=[];
const rows=[[0,0,0,255,255,255],[255,255,255,0,0,0],[13,41,233,191,89,2],
 [255,0,128,0,255,127],[10,20,30,10,20,30],[0,1,2,3,4,5]];
m.load(table,Buffer.from(rows.flat()));m.w32(0x801d06f4,table);
for(let row=0;row<rows.length;row++)for(const x of [-1e6,-228,-190.01,-190,-189.99,-.25,0,189.99,190,190.01,1e6])
for(const z of [-50,0,70]){
 m.load(actor,Buffer.alloc(336));m.writeFloat(actor+0x11c,x);m.writeFloat(actor+0x124,z);
 m.w32(0x801ceab0,row);m.w32(0x800e9ba0,display);m.set('a0',actor);m.call(0x8023e83c);
 assert.equal(m.r32(display),0xfb000000);assert.equal(m.r32(0x800e9ba0),display+8);
 cases.push({x,z,gradient:rows[row],color:m.r32(display+4)});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original world environment: ${cases.length} complete calls; no helpers controlled.`);
