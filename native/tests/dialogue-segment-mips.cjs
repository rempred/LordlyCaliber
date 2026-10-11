'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const c={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};c.window=c;c.OB64={};vm.createContext(c);vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),c);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8019d130,rom.subarray(0xe9eb0,0xea184));m.load(0x8017b550,rom.subarray(0x51450,0x5148c));m.load(0x8019e880,rom.subarray(0xeb600,0xeb77c));
const payload=0x80300000,text=0x80301000,strings=0x80310000,table=0x1a3b7b2+0x594280,fixtures=[],substitutions=[];
for(let i=0;i<256;i++){const s=i===48?null:i>=49&&i<=54?Buffer.from('ABCDEF'.slice(0,i-48)+'\0'):i===55?Buffer.from([0x81,0x40,65,0]):i===56?Buffer.from('@%5\0'):Buffer.from('Speaker\0');substitutions.push(s?s.toString('hex'):null);m.w32(0x8018f440+i*4,s?strings+i*128:0);if(s)m.load(strings+i*128,s);}
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
const results=[];
for(const f of fixtures)for(const columns of [0,1,3,36,100]){
 m.load(text,Buffer.alloc(65536));if(f.source)m.load(text,f.source);
 m.w32(0x8019e194,payload);m.w16(payload+0x1a,columns);m.w32(0x8019ee30,0x12345678);m.w8(0x8019ee34,0xa7);m.set('sp',0x803ffe00);m.set('a0',f.source?text:0);m.call(0x8019d130);
 const published=!!f.source,cursor=published?m.r32(0x8019ee30)-text:0,stop=published?m.r8(0x8019ee34):0;
 if(published)assert(cursor<=f.source.length,'cursor overrun '+f.id);else {assert.equal(m.r32(0x8019ee30),0x12345678);assert.equal(m.r8(0x8019ee34),0xa7);}
 results.push({id:f.id,source:f.source?f.source.toString('hex'):null,columns,count:m.get('v0'),cursor,stop,published});
}
fs.writeFileSync(process.argv[3],JSON.stringify({retailCount,substitutions,results}));
console.log(`${results.length} original segment scans, ${retailCount} retail sources, explicit substitution tables; no helpers controlled.`);
