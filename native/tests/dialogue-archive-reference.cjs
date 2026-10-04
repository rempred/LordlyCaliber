'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const c={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};
c.window=c;c.OB64={};vm.createContext(c);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),c);
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const table=0x1a3b7b2+0x594280,results=[];
for(let selector=0;selector<rom.readUInt32BE(table)/4;selector++) {
 const key=rom.readUInt32BE(table+4+selector*4);if(!key)continue;
 const at=key+0x594280+4,header=rom[at+20]===2?rom.readUInt16LE(at):rom[at]+2;
 const size=rom.readUInt32LE(at+7),decoded=rom.readUInt32LE(at+11);
 assert.equal(rom.toString('latin1',at+2,at+7),'-lh5-');
 const bytes=Buffer.from(c.OB64.lh5Decompress(rom.subarray(at+header,at+header+size),decoded));
 const entries=[];
 for(let i=0;i<bytes.readUInt32BE(0)/4;i++) {
  const start=bytes.readUInt32BE(i*4),end=bytes.indexOf(0,start);assert(end>=start);
  entries.push(bytes.subarray(start,end).toString('hex'));
 }
 results.push({selector,bytes:bytes.toString('hex'),entries});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} archives, ${results.reduce((n,r)=>n+r.entries.length,0)} exact byte entries; independent browser LH5 decoder.`);
