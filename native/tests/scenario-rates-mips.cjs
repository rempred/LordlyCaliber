'use strict';
// Compiled scenario gates and original/rebuilt candidate draw/removal instructions.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
function read(file){const b=fs.readFileSync(file);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const rom=read(fixture.rom),retail=read(fixture.sourceRom),ram=at=>(at+0x800ab8c0)>>>0;
function cpu(image){const m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(ram(0x13c060),image.subarray(0x13c060,0x13c49c));m.load(0x8009da50,image.subarray(0x2de50,0x2def4));return m;}
function clobber(m){for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])m.set(r,0xdeadbeef);}
let gates=0,draws=0,removals=0;
const m=cpu(rom);
for(let key=0;key<256;key++)for(const flag of [0,1,7]){
 const branch=fixture.rates.rows[key]?.[flag?'normal':'alternate'];
 const divisor=branch?.mode==='override'?branch.divisor:fixture.global.divisor;
 const pass=branch?.mode==='override'?branch.passCount:fixture.global[flag?'normalPassCount':'alternatePassCount'];
 const samples=[...new Set([0,1,2,pass-1,pass,pass+1,divisor-1,divisor,0x3fffffff].filter(n=>n>=0&&n<0x40000000))];
 for(const random of samples)for(const alignment of [0,8]){
  const sp=0x803ffe00+alignment;m.r.fill(0);m.ram.fill(0xa5,m.phys(sp-0x80),m.phys(sp+0x80));
  m.set('sp',sp);m.set('s3',0x12343333);m.set('s7',0x12347777);m.w8(0x8018f481,key);
  let dma=0,flags=0,rolls=0;
  m.trap(0x80089f80,c=>{assert.equal(c.get('a0'),fixture.dataOffset+key*16);assert.equal(c.get('a1'),sp+0x20);assert.equal(c.get('a2'),16);
   c.load(c.get('a1'),rom.subarray(c.get('a0'),c.get('a0')+16));clobber(c);dma++;});
  m.trap(0x80186210,c=>{assert.equal(c.get('a0'),17);clobber(c);c.set('v0',flag);flags++;});
  m.trap(0x80092a60,c=>{clobber(c);c.set('v0',random);rolls++;});
  const passing=random%divisor<pass;
  m.call(ram(0x13c1e8),ram(passing?0x13c230:0x13c46c));
  assert.equal(m.get('s1'),divisor);assert.equal(m.get('s0'),pass);assert.equal(m.get('v1'),29);
  assert.equal(m.get('s3'),0x12343333);assert.equal(m.get('s7'),0x12347777);assert.equal(m.get('sp'),sp);
  assert.equal(dma,1);assert.equal(flags,1);assert.equal(rolls,1);
  for(let at=0;at<0x80;at++)if(at<0x20||at>=0x30)assert.equal(m.r8(sp+at),0xa5);
  for(let at=-0x80;at< -0x40;at++)assert.equal(m.r8(sp+at),0xa5);
  gates++;
 }
}
for(const image of [retail,rom]){
 const c=cpu(image),sp=0x803ffe08;
 for(let remaining=1;remaining<=30;remaining++){
  for(const random of [0,1,remaining-1,remaining,0x3fffffff]){
   c.r.fill(0);c.set('sp',sp);c.set('s3',remaining);let rolls=0;
   c.trap(0x80092a60,m=>{clobber(m);m.set('v0',random);rolls++;});
   c.call(ram(0x13c260),ram(0x13c294));
   assert.equal(c.get('s4'),random%remaining);assert.equal(c.get('s3'),remaining);assert.equal(rolls,1);draws++;
  }
  for(let chosen=0;chosen<remaining;chosen++){
   c.r.fill(0);c.ram.fill(0xa5,c.phys(sp),c.phys(sp+0x80));
   for(let i=0;i<30;i++)c.w8(sp+0x18+i,i);
   c.w32(sp+0x3c,sp+0x19);c.set('sp',sp);c.set('s7',sp+0x18);
   c.set('s3',remaining);c.set('s5',remaining-1);c.set('s4',chosen);c.set('v0',29);
   c.call(ram(0x13c420),ram(remaining>1?0x13c260:0x13c46c));
   assert.equal(c.get('s3'),remaining-1);assert.equal(c.get('s5'),(remaining-2)>>>0);
   const expected=Array.from({length:remaining},(_,i)=>i).filter(i=>i!==chosen);
   assert.deepEqual(expected.map((_,i)=>c.r8(sp+0x18+i)),expected);
   for(let at=0;at<0x80;at++)if(at<0x18||(at>=0x36&&at<0x3c)||at>=0x40)assert.equal(c.r8(sp+at),0xa5);
   removals++;
  }
 }
}
console.log(`PASS scenario-rate instructions: ${gates} key/flag/RNG/stack cases, ${draws} original/rebuilt candidate draws, ${removals} candidate removals covering counts 1–30; guarded 4 MiB RAM. Lower PI/cache hardware and complete game dispatch remain outside this check.`);
