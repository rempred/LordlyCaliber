'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const c={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};c.window=c;c.OB64={};vm.createContext(c);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),c);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8019dbb0,rom.subarray(0xea930,0xeacc0));m.load(0x8019ecd0,rom.subarray(0xeba50,0xebba8));
const payload=0x80300000,text=0x80301000,record=0x800e7a30,table=0x1a3b7b2+0x594280;
const fixtures=[];
for(let selector=0;selector<rom.readUInt32BE(table)/4;selector++){
 const key=rom.readUInt32BE(table+4+selector*4);if(!key)continue;
 const at=key+0x594280+4,head=rom[at+20]===2?rom.readUInt16LE(at):rom[at]+2;
 const data=Buffer.from(c.OB64.lh5Decompress(rom.subarray(at+head,at+head+rom.readUInt32LE(at+7)),rom.readUInt32LE(at+11)));
 for(let i=0;i<data.readUInt32BE(0)/4;i++){const start=data.readUInt32BE(i*4);fixtures.push({id:`${selector}:${i}`,source:data.subarray(start,data.indexOf(0,start)+1)});}
}
const retailCount=fixtures.length;
for(const char of '*A$-bxkgumr^')fixtures.push({id:`control ${char}`,source:Buffer.from('@'+char+'Body\0')});
for(const char of 'Sl')for(const value of [0,47,48,49,55,56,79,80,255])fixtures.push({id:`${char} ${value}`,source:Buffer.from([64,char.charCodeAt(0),value,66,0])});
for(const char of 'fFvy<')for(const bytes of [[48,48,48],[57,57,57],[0,0,0],[255,255,255],[47,58,31]])fixtures.push({id:`${char} ${bytes}`,source:Buffer.from([64,char.charCodeAt(0),...bytes,66,0])});
fixtures.push({id:'combined controls',source:Buffer.from('@*@A@$@-@b@x@k@g@g@u@m@r@^@f123@F998@v036@y003@l9@S7@<255@p\0')});
const results=[];
for(const [index,f] of fixtures.entries()){
 const state=Buffer.alloc(1144),window=Buffer.alloc(168);for(let i=0;i<state.length;i++)state[i]=(i*17+index*3)&255;for(let i=0;i<window.length;i++)window[i]=(i*19+index*7)&255;
 state.writeUInt32BE(text,8);m.load(payload,state);m.load(record,window);m.load(text,Buffer.alloc(65536));m.load(text,f.source);m.w32(0x8019e194,payload);m.set('sp',0x803ffe00);
 m.call(0x8019dbb0);const cursor=m.r32(payload+8)-text;assert(cursor<f.source.length,'cursor leaves source '+f.id);
 const out=Buffer.from(m.ram.subarray(m.phys(payload),m.phys(payload)+1144));out.writeUInt32BE(0,8);state.writeUInt32BE(0,8);
 results.push({id:f.id,source:f.source.toString('hex'),initialState:state.toString('hex'),initialWindow:window.toString('hex'),state:out.toString('hex'),window:Buffer.from(m.ram.subarray(m.phys(record),m.phys(record)+168)).toString('hex'),cursor});
}
fs.writeFileSync(process.argv[3],JSON.stringify({retailCount,results}));
console.log(`${results.length} original prefix parses (${retailCount} retail), full payload/window comparisons; no helpers controlled.`);
