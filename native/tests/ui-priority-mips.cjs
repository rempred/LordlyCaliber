'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8007819c,rom.subarray(0x859c,0x86ec));
const pool=0x800e82c8,queue=0x800c4c10,results=[];
for(let mask=0;mask<64;mask++)for(const priorities of [[0,0,0,0,0,0],[1,1,1,1,1,1],[1,2,3,4,5,6],[6,5,4,3,2,1],[0,65535,1,32768,32767,0],[10,0,10,1,0,10]]){
 const initial=Buffer.alloc(6*168),entries=[65535,4,0,123,5,2];for(let i=0;i<initial.length;i++)initial[i]=(i*31+mask*13)&255;
 for(let slot=0;slot<6;slot++){initial.writeUInt16BE(((mask>>slot)&1?0x8000:0)|0x7000|(slot<<4),slot*168);initial.writeUInt16BE(priorities[slot],slot*168+14);m.w16(queue+slot*2,entries[slot]);}
 m.load(pool,initial);m.w16(0x800c49d0,65535);m.set('sp',0x803ffe00);m.call(0x8007819c);
 results.push({initial:initial.toString('hex'),entries,count:m.r16(0x800c49d0),queue:Array.from({length:6},(_,i)=>m.r16(queue+i*2)),after:Buffer.from(m.ram.subarray(m.phys(pool),m.phys(pool)+1008)).toString('hex')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} complete priority queue/pool comparisons; no helpers controlled.`);
