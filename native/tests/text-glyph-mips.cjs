'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8017b550,rom.subarray(0x51450,0x5148c));m.load(0x801793e4,rom.subarray(0x4f2e4,0x4f3c8));
const normal=Buffer.alloc(256),alternate=Buffer.alloc(256);
for(let i=0;i<256;i++){normal[i]=(i*7+3)&255;alternate[i]=(i*11+31)&255;}
m.load(0x80300000,normal);m.load(0x80301000,alternate);m.w32(0x8018fc70,0x80300000);m.w32(0x8018fc74,0x80301000);
const results=[];
for(const fixed of [-1,0,16,48,165,255])for(const other of [0,1,-1])for(let c=0;c<256;c++){
 m.w16(0x80190f74,fixed);m.set('sp',0x803ffe00);m.set('a0',0x12340000|c);m.set('a1',other);m.call(0x801793e4);const width=m.get('v0');
 m.set('a0',0xffff0000|c);m.call(0x8017b550);
 results.push({fixed,other,character:c,width,lead:m.get('v0')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({normal:normal.toString('hex'),alternate:alternate.toString('hex'),results}));
console.log(`${results.length} original glyph widths and lead classifications; no helpers controlled.`);
