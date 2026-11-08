"use strict";
// Fresh-export VM and maintained-C execution. Native scene presentation is the
// controlled boundary; no full-console rendering is claimed by this test.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const fixture=JSON.parse(fs.readFileSync(path.join(path.dirname(process.argv[2]),'campaign-fixture.json')));
const parser=fs.readFileSync(path.join(__dirname,'../../parsers.js'),'utf8'),start=parser.indexOf('OB64.lzssDecode = function'),end=parser.indexOf('\n};',start)+3,codec={OB64:{}};vm.runInNewContext(parser.slice(start,end),codec);
function eventRow(row){const at=0x594280+rom.readUInt32BE(0x9398ec+row*4);return Buffer.from(codec.OB64.lzssDecode(rom,at+8,rom.readUInt32BE(at+4)).output);}
const program=eventRow(1),arrival=eventRow(0),header=0x5dc80;assert.equal(program.toString('hex'),fixture.program);assert.equal(arrival.toString('hex'),fixture.arrival);
assert.equal(rom.toString('ascii',header,header+8),'LC05CLAS');
const data=rom.readUInt32BE(header+12),size=rom.readUInt32BE(header+16),address=rom.readUInt32BE(header+24);
const bridge=rom.readUInt32BE(0x66658);
assert(bridge>=address+8&&bridge<address+size);
assert.equal(rom.toString('ascii',data+bridge-address-8,data+bridge-address),'LCSTART1');
const SP=0x807ff000,PROGRAM=0x80700000;
const spans=[[0x80193bc0,5600],[fixture.capacity?fixture.capacity+0x3800:0x801971f0,750],[fixture.capacity?fixture.capacity+0x3e00:0x801977d8,30],[0x80197710,100],
 [0x801969b8,110],[0x80193ab0,6],[0x80190f80,364],[0x80196b00,1112],[0x80193ac0,160]];
const read=(m,at,n)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+n));
function machine(row=1){const m=new FloatMips();m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(address,rom.subarray(data,data+size));m.load(PROGRAM,program);m.w8(0x801936a8,row);m.set('sp',SP);return m;}
function execute(m,entry){let pc=entry;for(let n=0;n<4096;++n){assert(pc>=12&&pc+2<=program.length);const op=program[pc];m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);m.call(0x80170954,0x80171b58,1000000);if([0x11,0x12,0x2f].includes(op))return op;pc=(m.r16(0x80197b04)+1)*2;}throw Error('Opening failed to finish');}
const entry=program.readUInt16BE(program.readUInt16BE(0));
for(const answer of [0,1,2,255])for(const fill of [0,0xa5,255]){
 const m=machine();for(const [at,n]of spans)m.load(at,Buffer.alloc(n,fill));
 m.load(0x80193686,Buffer.alloc(32,fill));m.load(0x80196a81,Buffer.alloc(24,fill));
 m.w8(0x8019ee43,answer);m.w32(0x80196aa4,0x12345678);
 assert.equal(execute(m,entry),0x11);
 let offset=0;const expected=Buffer.from(answer===1?fixture.army1:fixture.army0,'hex');
 // The preset's battery representation is independent of the live roster.
 // Both paths must publish the exact text to the native name buffer.
 expected.fill(0,56,56+17);
 expected.write(answer===1?'Choice Two':'Magnus',56,'ascii');
 for(const [at,n]of spans){assert.deepEqual(read(m,at,n),expected.subarray(offset,offset+n),'starting army span '+at.toString(16));offset+=n;}
 const flags=Buffer.alloc(32);flags[99>>3]=1<<(99&7);assert.deepEqual(read(m,0x80193686,32),flags);
 assert.equal(m.r32(0x80196a6c),1234);assert.equal(m.r8(0x801936a9),37);
 for(let i=0;i<52;i++)assert.equal((m.r8(0x80196a86+(i>>2))>>((i&3)*2))&3,fixture.settings.locations.includes(i+1)?1:0);
 assert.deepEqual(read(m,0x80196a93,6),Buffer.alloc(6));assert.deepEqual(read(m,0x80196a81,5),Buffer.alloc(5));
 assert.equal(m.r8(0x80196a9a),0x86);assert.equal(m.r8(0x801936aa),6);assert.equal(m.r8(0x801936a8),0);
 assert.equal(m.r16(0x8018f1a2),0x8005,'native request enters world map');
 const x=rom.readInt16BE(0x864a0+5*8),y=rom.readInt16BE(0x864a0+5*8+2),sx=Math.max(0,Math.min(256,x-128)),sy=Math.max(0,Math.min(124,y-96));
 assert.equal(m.r16(0x80196a9c),sx);assert.equal(m.r16(0x80196a9e),sy);assert.equal(m.r16(0x80196aa0),x-sx+32);assert.equal(m.r16(0x80196aa2),y-sy+24);
 assert.equal(m.r32(0x80196aa4),0x12345678,'unrelated state preserved');
}
// A foreign/mission event cannot use the new opcode to replace an existing army.
for(const row of [0,4,115]){const m=machine(row);m.load(PROGRAM,Buffer.from([9,0]));m.w16(SP-144+80,1);m.w16(0x80197b04,0);m.w32(0x80193bc0+56,0x12345678);m.set('a0',PROGRAM);m.call(0x80170954,0x80171b58,1000);assert.equal(m.r32(0x80193bc0+56),0x12345678);}
for(const completed of [false,true]) {
 const m=machine(0);m.load(PROGRAM,arrival);m.w16(0x80197b04,arrival.readUInt16BE(0)/2);
 m.w8(0x80193686+(99>>3),completed?1<<(99&7):0);m.set('a0',PROGRAM);m.call(0x80170954,0x80000100,100000);
 assert.equal(m.get('v0'),completed?0:2);assert.equal(m.r8(0x801936a8),completed?0:1,'New Game entry, not a retail mission');
}
// First authored mission remains reachable after clearing the retail routing.
const flagMetadata=data+size-32;
assert.equal(rom.toString('ascii',flagMetadata,flagMetadata+8),'LCFLAGS1');
const flagBank=rom.readUInt32BE(flagMetadata+8);
for(const completed of [false,true]) {
 const m=machine(0);m.load(PROGRAM,arrival);m.w8(0x80196a9a,6);m.w8(flagBank,completed?1:0);
 m.w16(0x80197b04,arrival.readUInt16BE(6*2)/2);m.set('a0',PROGRAM);m.call(0x80170954,0x80000100,100000);
 assert.equal(m.get('v0'),completed?0:2);assert.equal(m.r8(0x801936a8),completed?0:fixture.mission,'arrival loads the new mission once');
}
const overlay=(((rom.readUInt32BE(0x51524)&65535)<<16)+rom.readInt16BE(0x5152a))>>>0,h=overlay-80;
assert.equal(rom.readUInt32BE(h+8),7);const roadCount=rom.readUInt32BE(h+20),roadAddress=rom.readUInt32BE(h+24);
let roads=0;
for(let id=1;id<=roadCount;id++)for(const left of [0,1])for(const right of [0,1]){
 const m=machine();m.load(0x80197b70,rom.subarray(0x76b00,0x87200));m.load(0x8019a7a0,rom.subarray(overlay,overlay+rom.readUInt32BE(h+16)));
 const a=m.r8(roadAddress+(id-1)*2),b=m.r8(roadAddress+(id-1)*2+1);if(!a||!b)continue;
 const state=(id,v)=>{const n=id-1,at=0x80196a86+(n>>2),shift=(n&3)*2;m.w8(at,(m.r8(at)&~(3<<shift))|(v<<shift));};
 state(a,left);state(b,right);m.load(0x80196a81,Buffer.alloc(5,255));m.set('a0',id);m.call(0x8019e9d0);
 assert.equal(m.get('v0'),left&&right?1:0,'road follows visibility, not retail story flags');roads++;
}
console.log(`PASS: 12 exported New Game/answer cases with live names, 3 existing-army protection cases, 4 New Game/mission routing cases and ${roads} road-access cases. Scene rendering and full gameplay not exercised.`);
