'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const c={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};c.window=c;c.OB64={};vm.createContext(c);vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),c);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8019d130,rom.subarray(0xe9eb0,0xea184));m.load(0x8017b550,rom.subarray(0x51450,0x5148c));m.load(0x8019e880,rom.subarray(0xeb600,0xeb77c));
const payload=0x800e91d0,text=0x80301000,strings=0x80310000,table=0x1a3b7b2+0x594280,fixtures=[],substitutions=[];
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

m.load(0x8019ba78,rom.subarray(0xe87f8,0xe9528));m.load(0x8019e400,rom.subarray(0xeb180,0xeb300));
m.load(0x8019d904,rom.subarray(0xea684,0xea930));m.load(0x8019ea00,rom.subarray(0xeb780,0xeba50));m.load(0x8019df7c,rom.subarray(0xeacfc,0xead04));
const shared=0x800e7a30,suffix=Buffer.from('tail\0'),suffixAddress=0x80330000; m.load(suffixAddress,suffix);m.w32(0x8019e190,suffixAddress);
let sounds=0,children=[];
m.trap(0x800ea9bc,c=>{assert.deepEqual(['a0','a1','a2','a3'].map(r=>c.get(r)),[1,4,26,10]);assert.equal(m.r32(c.get('sp')+16),0);++sounds;});
m.trap(0x80093380,c=>{assert([96,192,768].includes(c.get('a1')));m.load(c.get('a0'),Buffer.alloc(c.get('a1')));});
m.trap(0x8007f648,c=>{assert.equal(c.get('a0'),0x80198be8);assert.equal(c.get('a3'),0);assert.equal(m.r32(c.get('sp')+16),0);const slot=children.length;assert(slot<6);children.push({x:c.get('a1')<<16>>16,y:c.get('a2')<<16>>16,slot});c.set('v0',slot);});
m.trap(0x800934b0,c=>{assert.equal(c.get('a1'),0x8019e3f8);m.load(c.get('a0'),Buffer.from('{X'+String(c.get('a2')).padStart(3,'0')+'}\0'));});

const a=0x80340000,b=0x80341000,source=Buffer.from('@%0@%0Done\0'),state=Buffer.alloc(1144),window=Buffer.alloc(168);
state.writeUInt16BE(36,0x1a);state.writeUInt16BE(3,0x1e);state[0x3c]=2;state.writeUInt32BE(text,4);state.writeUInt32BE(text,8);
m.load(payload,state);m.load(shared,window);m.load(text,source);m.load(a,Buffer.from('ABC\0'));m.load(b,Buffer.from('XYZ\0'));m.w32(0x8019e194,payload);m.w32(0x8018f500,a);
const results=[];
for(let update=0;update<6;update++){
 if(update===1)m.w32(0x8018f500,b);if(update===2)m.load(a,Buffer.from('PQR\0'));
 const speed=[512,256,256,512,256,512][update];m.w16(0x800e9c0c,speed);sounds=0;children=[];m.set('sp',0x803ffe00);m.call(0x8019ba78);
 const output=Buffer.from(m.ram.subarray(m.phys(payload),m.phys(payload)+1144)),ptr=output.readUInt32BE(12);let insertion=null;if(ptr){const identity=ptr>=b?2:1;insertion={identity,cursor:ptr-(identity===1?a:b)};}
 output.writeUInt32BE(output.readUInt32BE(4)-text,4);output.writeUInt32BE(output.readUInt32BE(8)-text,8);output.writeUInt32BE(ptr?1:0,12);
 results.push({speed,state:output.toString('hex'),window:Buffer.from(m.ram.subarray(m.phys(shared),m.phys(shared)+168)).toString('hex'),insertion,sounds,result:m.get('v0')});
}
state.writeUInt32BE(0,4);state.writeUInt32BE(0,8);
fs.writeFileSync(process.argv[3],JSON.stringify({source:source.toString('hex'),state:state.toString('hex'),window:window.toString('hex'),results}));
console.log('Six original invocations distinguish pointer rebinding from writes to retained source storage. Sound controlled; no other helper reached.');
