'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);m.set('sp',0x803ffe00);
// Descriptor-10 combat placement: ROM2015C8 -> RAM801BE138.
m.load(0x801be138,rom.subarray(0x2015c8,0x20162c));
m.load(0x80187c14,rom.subarray(0x5db14,0x5db14+165*72));
const envelope=0x315736+0x594280,begin=rom.readUInt16BE(envelope+4),end=rom.readUInt16BE(envelope+6);
const table=rom.subarray(envelope+4+begin,envelope+4+end);
m.load(0x80300010,table);m.w32(0x801d0690,0x80300010);const rows=[];
for(let source=0;source<165;source++)for(let owner=0;owner<165;owner++)for(let flags=0;flags<4;flags++){
 const flagA=flags>>1,flagB=flags&1;[source,owner,flagA,flagB].forEach((v,i)=>m.set('a'+i,v));m.call(0x801be138);assert.equal(m.get('sp'),0x803ffe00);
 rows.push([source,owner,flagA,flagB,m.get('v0')]);
}
fs.writeFileSync(process.argv[3],JSON.stringify({table:table.toString('hex'),rows}));
console.log(`Original body resolver: ${rows.length} source/owner/flag combinations; no helper doubles.`);
