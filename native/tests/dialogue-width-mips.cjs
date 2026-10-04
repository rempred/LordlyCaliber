'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const c={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};c.window=c;c.OB64={};vm.createContext(c);vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),c);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8019d130,rom.subarray(0xe9eb0,0xea184));m.load(0x8017b550,rom.subarray(0x51450,0x5148c));m.load(0x8019e880,rom.subarray(0xeb600,0xeb77c));
const payload=0x80300000,text=0x80301000,strings=0x80310000,table=0x1a3b7b2+0x594280,fixtures=[],substitutions=[];
for(let i=0;i<256;i++){const s=i===48?Buffer.from([0]):i>=49&&i<=54?Buffer.from('ABCDEF'.slice(0,i-48)+'\0'):i===55?Buffer.from([0x81,0x40,65,0]):i===56?Buffer.from('@%5\0'):Buffer.from('Speaker\0');substitutions.push(s?s.toString('hex'):null);m.w32(0x8018f440+i*4,s?strings+i*128:0);if(s)m.load(strings+i*128,s);}
for(let selector=0;selector<rom.readUInt32BE(table)/4;selector++){
 const key=rom.readUInt32BE(table+4+selector*4);if(!key)continue;
 const at=key+0x594280+4,head=rom[at+20]===2?rom.readUInt16LE(at):rom[at]+2;
 const data=Buffer.from(c.OB64.lh5Decompress(rom.subarray(at+head,at+head+rom.readUInt32LE(at+7)),rom.readUInt32LE(at+11)));
 for(let i=0;i<data.readUInt32BE(0)/4;i++){const start=data.readUInt32BE(i*4);fixtures.push({id:`${selector}:${i}`,source:data.subarray(start,data.indexOf(0,start)+1)});}
}
const retailCount=fixtures.length;
for(const control of '%!z:')for(const name of '012345678')fixtures.push({id:`${control}${name}`,source:Buffer.from('@'+control+name+'X\0')});
for(const maximum of [0,47,48,49,57,255])fixtures.push({id:'limit'+maximum,source:Buffer.from([64,38,54,maximum,88,0])});
fixtures.push({id:'mixed',source:Buffer.from('@f084@l3@w1234{T05}ABC@nDE\0')},{id:'empty',source:Buffer.from([0])},{id:'null',source:null});

m.load(0x8019cb3c,rom.subarray(0xe98bc,0xe9eb0));m.load(0x8019e700,rom.subarray(0xeb480,0xeb600));
m.load(0x801794c8,rom.subarray(0x4f3c8,0x4f86c));m.load(0x801793e4,rom.subarray(0x4f2e4,0x4f3c8));m.load(0x8017b500,rom.subarray(0x51400,0x51450));
m.load(0x80093420,rom.subarray(0x23820,0x23884));m.load(0x8009c590,rom.subarray(0x2c990,0x2c9e0));
const normal=Buffer.alloc(256),alternate=Buffer.alloc(256),suffix=Buffer.from('tail\0');
for(let i=0;i<256;i++){normal[i]=(i*7+3)&255;alternate[i]=(i*11+31)&255;}
m.load(0x80330000,normal);m.load(0x80331000,alternate);m.w32(0x8018fc70,0x80330000);m.w32(0x8018fc74,0x80331000);m.w32(0x8019e190,0x80332000);m.load(0x80332000,suffix);
const results=[];
function run(f,columns=36,minimum=0,fixed=-1,style=0,side=0,flags=0,portrait=-1){
 m.load(text,Buffer.alloc(65536));if(f.source)m.load(text,f.source);m.w32(0x8019e194,payload);m.w16(payload+0x1a,columns);m.w8(payload+0x47,style);m.w8(payload+0x3d,side);m.w16(payload+0x14,portrait);m.w8(0x800e7ac1,flags);m.w16(0x80190f74,fixed);
 m.set('sp',0x803ffe00);m.set('a0',f.source?text:0);m.set('a1',minimum);m.call(0x8019cb3c);
 results.push({id:f.id,source:f.source?f.source.toString('hex'):null,columns,minimum,fixed,width:m.get('v0')});
}
for(const f of fixtures.slice(0,retailCount))run(f);
fixtures.push({id:'unclosed-brace',source:Buffer.from('{ABC\0')},{id:'pages',source:Buffer.from('@pA@n@pB@n@pC@c@pD\0')},{id:'spaces',source:Buffer.from('@iA@nB@n@pC\0')});
// @!7 copies one byte of a two-byte character and leaves the original routine
// reading beyond the terminator into uninitialized scratch. The host rejects
// that unsafe input separately instead of blessing this shim's zero-filled RAM.
for(const f of fixtures.slice(retailCount).filter(f=>f.id!=='!7'))for(const columns of [0,3,36,100])for(const minimum of [-5,0,80,65537])for(const fixed of [-1,16])run(f,columns,minimum,fixed);
for(const columns of [0,36,65535])for(const style of [0,1,2,255])for(const side of [0,1])for(const flags of [0,4])for(const portrait of [-1,0])run({id:`styles ${style}/${side}/${flags}/${portrait}`,source:Buffer.from('@iA@nB@n@pC\0')},columns,0,-1,style,side,flags,portrait);
fs.writeFileSync(process.argv[3],JSON.stringify({retailCount,substitutions,normal:normal.toString('hex'),alternate:alternate.toString('hex'),suffix:suffix.toString('hex'),results}));
console.log(`${results.length} original dialogue width measurements, ${retailCount} retail sources, explicit substitutions and metric tables; no helpers controlled.`);
