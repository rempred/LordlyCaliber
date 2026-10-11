'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x801794c8,rom.subarray(0x4f3c8,0x4f86c));m.load(0x8017b500,rom.subarray(0x51400,0x5148c));
const normal=Buffer.alloc(256),alternate=Buffer.alloc(256),text=0x80302000;
for(let i=0;i<256;i++){normal[i]=(i*7+3)&255;alternate[i]=(i*11+31)&255;}
m.load(0x80300000,normal);m.load(0x80301000,alternate);m.w32(0x8018fc70,0x80300000);m.w32(0x8018fc74,0x80301000);
const sources=[Buffer.from([0]),Buffer.from('A'.repeat(2000)+'\0')];
for(let c=1;c<256;c++)if(c!==123)sources.push(Buffer.from([65,c,66,67,15,68,0]));
const tokens=['ABC','\t','\b','\r\n\f','{T1}','{t166}','{T256}','{H0}','{h3}','{H65535}','{H4294967295}','{Qignored}','\x81\x40','\x0eABC\x0f','\x0eAB\x10aCD\x0f','\x0eAB\x10/CD\x0f','\x0eA\x10ZBCD\x0f','\x0eA\x81\x40BC\x0f','\x0eABC'];
for(let i=0;i<tokens.length;i++)for(let j=0;j<tokens.length;j++)sources.push(Buffer.from(tokens[i]+'X'+tokens[j]+'Y\0','latin1'));
const results=[];
for(const fixed of [-1,16,165])for(const source of sources){
 m.load(text,Buffer.alloc(65536));m.load(text,source);m.w16(0x80190f74,fixed);m.set('sp',0x803ffe00);m.set('a0',text);m.call(0x801794c8);
 results.push({source:source.toString('hex'),fixed,width:m.get('v0')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({normal:normal.toString('hex'),alternate:alternate.toString('hex'),results}));
console.log(`${results.length} original general text measurements, explicit metric tables; no helpers controlled.`);
