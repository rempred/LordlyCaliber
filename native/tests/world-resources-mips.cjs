'use strict';
// Original resource dispatcher/cache, framing, and LH5 instructions. Allocation,
// OS, and PI are doubles. This does not run a full game or measure peak game RAM.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
if(process.argv.includes('--require-rom')&&!process.env.LORDLY_TEST_ROM)process.exit(77);
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
function read(path){const b=fs.readFileSync(path);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const rom=read(fixture.rom);let transfers=0,allocations=0,loads=0,frees=0;
function cpu(){
 const m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(0x80070c00,rom.subarray(0x1000,0x3f1b0));
 const execute=m.exec.bind(m);m.exec=(pc,next)=>{try{return execute(pc,next);}catch(e){throw Error(`${e.message}; PC=${pc.toString(16)} ra=${m.get('ra').toString(16)}`);}};
 m.set('sp',0x803ffe00);let heap=0x80100010;const guards=[];
 const alloc=c=>{const size=c.get('a0');assert(size>0&&size<0x80000);const at=heap,capacity=(size+15)&~15;heap=at+capacity+32;
  assert(heap<0x80280000);c.load(at-16,Buffer.alloc(capacity+32,0xa7));guards.push(at-16,at+capacity);c.set('v0',at);allocations++;};
 m.checkGuards=()=>{for(const at of guards)for(let i=0;i<16;i++)assert.equal(m.r8(at+i),0xa7,'allocation boundary');};
 m.trap(0x80070f30,alloc);m.trap(0x80071288,alloc);m.trap(0x800712c4,()=>{frees++;});
 m.trap(0x80093380,c=>{const a=c.get('a0'),n=c.get('a1');assert(a+n<=0x80400000);c.load(a,Buffer.alloc(n));});
 m.trap(0x80089f80,c=>{const at=c.get('a0'),to=c.get('a1'),n=c.get('a2');assert(at+n<=rom.length);assert(n<=0x400000);
  c.load(to,rom.subarray(at,at+n));transfers++;});
 for(const at of [0x8007bb90,0x8007bbc0,0x8007bbf4,0x80093540])
  m.trap(at,c=>{throw Error(`Original resource loader error at ${at.toString(16)} ra=${c.get('ra').toString(16)}`);});
 return m;
}
function load(m,slot,index,expected){
 m.set('a0',0x21af9f8);m.set('a1',slot);m.set('a2',-3);m.set('a3',index);
 m.call(0x8007938c,0x80000100,20000000);
 const at=m.get('v0');assert(at>=0x80100000&&at+expected.length<0x80280000);
 assert.deepEqual(Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+expected.length)),expected);
 assert.equal(m.get('sp'),0x803ffe00);m.checkGuards();loads++;return at;
}
for(const row of fixture.rows){
 const m=cpu(),expected=Buffer.from(row.data,'hex');
 const first=load(m,row.slot,row.index,expected),before=transfers;
 assert.equal(load(m,row.slot,row.index,expected),first,'cached resource pointer');
 assert.equal(transfers,before,'cached load must not read ROM again');
 m.set('a0',0x21af9f8);m.set('a1',row.slot);m.set('a2',-17);
 m.call(0x80079618,0x80000100,20000000);m.checkGuards();
 load(m,row.slot,row.index,expected);
}
for(const row of fixture.selectors){
 const m=cpu(),args=[];
 m.load(0x801e7e10,rom.subarray(0x13c550,0x13c550+65*12));m.w8(0x8018f481,row.key);
 m.trap(0x8007938c,c=>{args.push(['a0','a1','a2','a3'].map(r=>c.get(r)));c.set('v0',0x80200000);});
 // Descriptor 7: ROM 101CE0 loads at RAM 801AD5A0.
 const delta=0x800ab8c0,start=(0x1278d4+delta)>>>0,end=(0x127934+delta)>>>0;
 m.load(start,rom.subarray(0x1278d4,0x127934));m.call(start,end);
 assert.deepEqual(args,[[0x21af9f8,2,0xfffffffd,row.sites],[0x21af9f8,2,0xfffffffd,0]]);
 // Treasure setup is straight-line code with one absolute call. Run it in test
 // scratch memory: no claim about this caller's unresolved overlay placement.
 args.length=0;m.load(0x80300000,rom.subarray(0x196138,0x196174));m.call(0x80300000,0x8030003c);
 assert.deepEqual(args,[[0x21af9f8,0,0xfffffffd,row.treasure]]);
}
assert(frees>0);
console.log(`PASS ${loads} original cached world loads, ${transfers} transfers, ${allocations} allocations and ${frees} frees; 65 site/treasure selector setups. Original -3 resource mode, relocated collections, preserved shop member and LH5 decode in guarded 4 MiB. OS/heap/PI are doubles; full-game loading and peak RAM remain unverified.`);
