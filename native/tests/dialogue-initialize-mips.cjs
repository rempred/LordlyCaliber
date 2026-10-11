'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const c={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};c.window=c;c.OB64={};vm.createContext(c);vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),c);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8019d130,rom.subarray(0xe9eb0,0xea184));m.load(0x8017b550,rom.subarray(0x51450,0x5148c));m.load(0x8019e880,rom.subarray(0xeb600,0xeb77c));
const payload=0x800e91d0,text=0x80301000,strings=0x80310000,table=0x1a3b7b2+0x594280,fixtures=[],substitutions=[];
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

m.load(0x80198be8,rom.subarray(0xe5968,0xe659c));m.load(0x8019dbb0,rom.subarray(0xea930,0xeacc0));m.load(0x8019ecd0,rom.subarray(0xeba50,0xebba8));
m.load(0x8019c7a8,rom.subarray(0xe9528,0xe98bc));m.load(0x8019e580,rom.subarray(0xeb300,0xeb47c));
const shared=0x800e7a30;let warnings=0;
m.trap(0x80093380,c=>{assert.equal(c.get('a0'),payload);assert.equal(c.get('a1'),1144);m.load(payload,Buffer.alloc(1144));});
m.trap(0x8007938c,c=>{assert.equal(c.get('a0'),0x1a3b7b2);c.set('v0',text-4);});
m.trap(0x80093540,c=>{const f=c.get('a0');assert([0x8019e1f8,0x8019e20c].includes(f));warnings|=f===0x8019e1f8?1:2;});
const results=[];
for(const prefix of ['@*','@$','@$@v100','@$@v036@y003','@$@v000@y000','@x@u@g','@f084','@F084'])for(const body of ['Body','A@nBC@nDE@cF','@%6 @!5 @z4X', 'A'.repeat(120)])fixtures.push({id:prefix+body.slice(0,8),source:Buffer.from(prefix+body+'\0')});
let sample=0;
for(const f of fixtures.filter(f=>f.source&&f.id!=='!7')){
 const initial=Buffer.alloc(168);for(let i=0;i<168;i++)initial[i]=(i*31+sample*17)&255;
 initial.writeUInt16BE(0xc800,0);initial.writeUInt32BE(0x80198be8,16);initial.writeUInt16BE(0,0x88);initial[0x8a]=[0,4,8,12,0x40,0x44,0x80,0xff][sample%8];
 initial[0x8b]=[0,3,4,7,8,0xa0,0xff][sample%7];initial[0x91]=[0,4,8,12][sample%4];initial.writeUInt16BE([0,1,84,0xffac,998,999,1000,0x8000,0xffff][sample%9],0x92);
 initial.writeUInt16BE([0,1,100,0xffff,0xfff0][sample%5],0x8c);initial.writeUInt16BE([0,1,200,0x8000,0xfff0][sample%5],0x94);
 m.load(shared,initial);m.load(payload,Buffer.alloc(1144,0xa7));m.load(text,Buffer.alloc(65536));m.load(text,f.source);m.w32(text-4,4);m.w32(0x8019e194,payload);m.w16(0x80190f74,-1);warnings=0;
 m.set('sp',0x803ffe00);m.set('a0',sample%6);m.call(0x80198be8);
 const state=Buffer.from(m.ram.subarray(m.phys(payload),m.phys(payload)+1144));state.writeUInt32BE((state.readUInt32BE(4)-text)>>>0,4);state.writeUInt32BE((state.readUInt32BE(8)-text)>>>0,8);
 results.push({id:f.id,source:f.source.toString('hex'),initial:initial.toString('hex'),slot:sample%6,state:state.toString('hex'),window:Buffer.from(m.ram.subarray(m.phys(shared),m.phys(shared)+168)).toString('hex'),warnings});++sample;
}
fs.writeFileSync(process.argv[3],JSON.stringify({retailCount,substitutions,normal:normal.toString('hex'),alternate:alternate.toString('hex'),suffix:suffix.toString('hex'),results}));
console.log(`${results.length} complete original dialogue initializations. Archive-pointer/zero-fill/diagnostics controlled; prefix, measurements and geometry execute.`);
