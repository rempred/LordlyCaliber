'use strict';
// Original resource dispatcher, archive framing and LH5 decoding from the user's
// ROM. OS synchronization, allocation and the lower transfer service are doubles.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
if(process.argv.includes('--require-rom')&&!process.env.LORDLY_TEST_ROM)process.exit(77);
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
function read(path) {const b=fs.readFileSync(path);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const rom=read(fixture.rom),original=read(fixture.sourceRom);
let transfers=0,allocations=0,cases=0;
function cpu(image) {
 const m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(0x80070c00,image.subarray(0x1000,0x3f1b0));
 const execute=m.exec.bind(m);m.exec=(pc,next)=>{try{return execute(pc,next);}catch(e){throw Error(`${e.message}; PC=${pc.toString(16)} ra=${m.get('ra').toString(16)}`);}};
 m.set('sp',0x803ffe00);let heap=0x80100010;const guards=[];
 const alloc=c=>{const size=c.get('a0');assert(size>0&&size<0x80000);const at=heap,capacity=(size+15)&~15;heap=at+capacity+32;
  assert(heap<0x80300000);c.load(at-16,Buffer.alloc(capacity+32,0xa7));guards.push(at-16,at+capacity);c.set('v0',at);allocations++;};
 m.checkGuards=()=>{for(const at of guards)for(let i=0;i<16;i++)assert.equal(m.r8(at+i),0xa7,'allocation boundary');};
 m.trap(0x80070f30,alloc);m.trap(0x80071288,alloc);m.trap(0x800712c4,()=>{});
 m.trap(0x80093380,c=>{const a=c.get('a0'),n=c.get('a1');assert(a+n<=0x80400000);c.load(a,Buffer.alloc(n));});
 m.trap(0x80089f80,c=>{const at=c.get('a0'),to=c.get('a1'),n=c.get('a2');assert(at+n<=image.length);assert(n<=0x400000);
  c.load(to,image.subarray(at,at+n));transfers++;});
 for(const at of [0x8007bb90,0x8007bbc0,0x8007bbf4,0x80093540])
  m.trap(at,c=>{let message='';for(let i=0;i<100&&c.r8(c.get('a0')+i);i++)message+=String.fromCharCode(c.r8(c.get('a0')+i));throw Error(`Original resource loader reported an error at ${at.toString(16)} ra=${c.get('ra').toString(16)}: ${message}`);});
 return m;
}
function load(image,key,index,expected) {
 const m=cpu(image);m.set('a0',key);m.set('a1',0);m.set('a2',-4);m.set('a3',index);
 m.call(0x8007938c,0x80000100,20000000);
 const at=m.get('v0');assert(at>=0x80100000&&at+expected.length<0x80300000);
 assert.deepEqual(Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+expected.length)),expected);
 assert.equal(m.get('sp'),0x803ffe00);m.checkGuards();cases++;
}
for(const row of fixture.rows) {
 // Execute the actual signed selector load and caller setup at its overlay address.
 const m=cpu(rom),delta=0x800b32a0; // Descriptor 3: ROM EBBB0 loads at RAM 8019EE50.
 const start=0xec178, address=(start+delta)>>>0;
 m.load(address,rom.subarray(start,0xec1c0));
 m.load(0x8018f5a0,rom.subarray(0x654a0,0x654a0+65*9));m.w8(0x800e7ab8,row.key);m.set('v0',0);
 let args;
 m.trap(0x8007938c,c=>{args=['a0','a1','a2','a3'].map(r=>c.get(r));c.set('v0',0x80200000);});
 m.call(address,(0xec1c0+delta)>>>0);
 assert.deepEqual(args,[0x21b35b2,0,0xfffffffc,row.index]);
 assert.equal(original[0x654a8+row.key*9],row.originalIndex);
 load(rom,args[0],args[3],Buffer.from(row.data,'hex'));
}
load(rom,0x21ae74c,0,Buffer.from(fixture.enemyData,'hex'));
console.log(`PASS ${cases} original resource loads, ${transfers} transfers, ${allocations} allocations: per-scenario ordinal selection, relocated ESET collection, grown squad library and original LH5 decoding in 4 MiB memory. OS/heap/PI services are simulated; full-game loading and peak heap use remain unverified.`);
