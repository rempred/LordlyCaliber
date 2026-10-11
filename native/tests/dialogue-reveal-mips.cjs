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
for(const token of ['@c','@/','@n','@a','@p','@i','@e003','@f084','@F084','@s','@k','@m','@g','@1one@n@2two@#end','@2skip@#end','@w0255','@t000','@t999','@T000','@T999','@%0','@%7','@&73','@!7','@z7','@:6','@-','@l3','@=Centered','@R','@r','@<255','@#','{T005}']) fixtures.push({id:token,source:Buffer.from(token+'XYZ\0')});
const results=[];let sample=0;
function run(f,speed,updates=4){
 const state=Buffer.alloc(1144),window=Buffer.alloc(168);state.fill(0xa7,0x57,0x178);state.writeUInt16BE(36,0x1a);state.writeUInt16BE(3,0x1e);state.writeUInt16BE(0xffff,0x14);state[0x3c]=2;state[0x48]=sample%2;state.writeUInt32BE(text,4);state.writeUInt32BE(text,8);window.writeUInt16BE(0x8200,0);window.writeUInt16BE(123,6);window.writeUInt16BE(0xffec,8);
 const controls=Buffer.from([1,2,3,4,5,6,7,8]);m.load(shared,window);m.load(payload,state);m.load(text,Buffer.alloc(65536));m.load(text,f.source);m.w32(0x8019e194,payload);m.load(0x8019ee40,controls);m.w16(0x800e9c0c,speed);const snapshots=[];
 for(let update=0;update<updates;update++){
  sounds=0;children=[];m.set('sp',0x803ffe00);m.call(0x8019ba78);
  const output=Buffer.from(m.ram.subarray(m.phys(payload),m.phys(payload)+1144));const ptr=output.readUInt32BE(12);let insertion=null;if(ptr){if(ptr>=suffixAddress&&ptr<suffixAddress+suffix.length)insertion={key:-1,cursor:ptr-suffixAddress};else{assert(ptr>=strings&&ptr<strings+256*128);insertion={key:Math.floor((ptr-strings)/128),cursor:(ptr-strings)%128};}}
  output.writeUInt32BE(output.readUInt32BE(4)-text,4);output.writeUInt32BE(output.readUInt32BE(8)-text,8);output.writeUInt32BE(ptr?1:0,12);
  snapshots.push({state:output.toString('hex'),window:Buffer.from(m.ram.subarray(m.phys(shared),m.phys(shared)+168)).toString('hex'),insertion,controls:Buffer.from(m.ram.subarray(m.phys(0x8019ee40),m.phys(0x8019ee40)+8)).toString('hex'),sounds,children:children.map(c=>({...c,entry:m.r16(0x800e8350+c.slot*168)})),result:m.get('v0')});
  if(m.get('v0')!==2)break;
 }
 state.writeUInt32BE(0,4);state.writeUInt32BE(0,8);results.push({id:f.id,source:f.source.toString('hex'),state:state.toString('hex'),window:window.toString('hex'),controls:controls.toString('hex'),speed,snapshots});++sample;
}
for(const f of fixtures.slice(0,retailCount))run(f,8192);
for(const f of fixtures.slice(retailCount).filter(f=>f.source))for(const speed of [0,128,256,257,512,65535])run(f,speed,8);
fs.writeFileSync(process.argv[3],JSON.stringify({retailCount,substitutions,suffix:suffix.toString('hex'),results}));
console.log(`${results.length} original text-reveal sequences, ${results.reduce((n,r)=>n+r.snapshots.length,0)} complete snapshots. Sound, child registration, zero-fill and formatted append controlled.`);
