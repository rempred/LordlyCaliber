'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
for(const [ram,begin,end] of [[0x80230858,0x2a0088,0x2a0354],[0x80227838,0x283808,0x2838ac],
 [0x80227b9c,0x283b6c,0x283c8c],[0x80227cbc,0x283c8c,0x283d94],[0x80178534,0x4e434,0x4e448]])m.load(ram,rom.subarray(begin,end));
const root=0x80300000,actor=0x80310000,armyContext=0x80320000,tracking=0x80330000,strings=0x80340000,sp=0x803ffe00;
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),6);c.set('v0',tracking);});
m.trap(0x8019ee58,c=>c.set('v0',0));m.trap(0x80230b60,()=>{});
function readString(p){const out=[];for(let i=0;i<1024;i++){const b=m.r8(p+i);if(!b)return Buffer.from(out);out.push(b);}throw Error('Unterminated reference string');}
m.trap(0x8009c550,c=>{m.load(c.get('a0'),Buffer.concat([readString(c.get('a1')),Buffer.from([0])]));c.set('v0',c.get('a0'));});
m.trap(0x8009c620,c=>{const dest=c.get('a0'),start=readString(dest).length;m.load(dest+start,Buffer.concat([readString(c.get('a1')),Buffer.from([0])]));c.set('v0',dest);});
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const patterns=[],results=[];
for(let p=0;p<8;p++){
 const army=Buffer.alloc(5600),guests=Buffer.alloc(5200),groups=Buffer.alloc(4*25);
 for(let i=0;i<100;i++){army.write('Army'+i,56*i);guests.write('Guest'+i,52*i);}
 army[5*56+17]=84;groups[3*25+2]=5;
 if([1,5].includes(p))army[2*56+17]=81;
 if([2,5].includes(p))guests[4*52+17]=81;
 if(p===3)army[2*56+17]=82;
 if(p===4)guests[4*52+17]=83;
 if(p===6){army[0*56+17]=83;guests[3*52+17]=82;}
 if(p===7)army[5*56+17]=0;
 const overrides=[{selector:84,text:'-one\0'},{selector:84,text:'-two\0'},{selector:81,text:'-hero\0'}];
 patterns.push({army:army.toString('hex'),guests:guests.toString('hex'),groups:groups.toString('hex'),fixed:'Fixed\0',overrides,currentGroup:3,defaultTextParameter:65530});
 for(const mode of [0,1,2])for(const identity of [-1,0,19,20,255])for(const portrait of [0,84,65536+84,0xffffffff]){
  m.load(0x80193bc0,army);m.load(0x80195560,guests);m.load(0x801971f0,groups);m.load(0x80196a58,Buffer.from('Fixed\0'));
  m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w32(0x801ce8bc,armyContext);m.load(armyContext,Buffer.alloc(0x1c4+28*248));
  m.load(0x8022ac00,Buffer.alloc(128));m.load(0x8018f500,Buffer.alloc(16));m.w16(0x80190f30,65530);m.w8(0x801976dc,3);
  overrides.forEach((v,i)=>{m.w8(0x8022a921+i*2,v.selector);m.w32(0x8022a8c8+i*4,strings+i*64);m.load(strings+i*64,Buffer.from(v.text));});m.w8(0x8022a921+overrides.length*2,255);
  const record=Buffer.alloc(336),row=Buffer.alloc(248);row[0xf6]=5;row.writeUInt32BE(84,0x48);
  m.load(actor,record);m.w32(root+24+7*4,actor);m.load(armyContext+0x1c4,row);
  function invoke(whichMode){m.w8(0x8018fc19,whichMode);m.set('sp',sp);[3,2,0,10,1,7,3,20,0,84].forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+i*4,v));m.call(0x80230858);assert.equal(m.get('sp'),sp);}
  function snapshot(){return {references:[0,1,2,3].map(i=>m.r32(0x8018f500+i*4)),owner:read(0x8022ac00,64),group:read(0x8022ac40,64),textParameter:m.r16(0x800e9c0c)};}
  invoke(2);const first=snapshot();
  record[0x147]=identity<0?0:identity;row.writeUInt32BE(portrait,0x48);
  m.load(actor,record);m.w32(root+24+7*4,identity<0?0:actor);if(identity>=0&&identity<20)m.load(armyContext+0x1c4+identity*248,row);
  m.w16(0x800e9c0c,123);invoke(mode);
  results.push({pattern:p,mode,actor:identity<0?'':record.toString('hex'),row:row.toString('hex'),first,second:snapshot()});
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({patterns,results}));
console.log(`${results.length} original shared-name sequences; full references and 64-byte workspaces. String copy/append, allocation, window registration and projection controlled.`);
