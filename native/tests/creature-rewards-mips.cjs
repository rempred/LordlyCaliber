'use strict';
// Execute compiled selectors and the original synchronous DMA wrapper in 4 MiB.
// Only the lower cartridge transfer and RNG services are controlled test doubles.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
function read(file){const b=fs.readFileSync(file);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const rom=read(fixture.rom),original=read(fixture.sourceRom),ram=at=>(at+0x800ab8c0)>>>0;
let nativeCases=0,retailCases=0,dmaCalls=0;
function callerClobber(m){for(const reg of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])m.set(reg,0xdeadbeef);}
for(const native of [false,true]){
 const image=native?rom:original,m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(ram(0x101ddc),image.subarray(0x101ddc,0x102f00));
 m.load(0x8009da50,image.subarray(0x2de50,0x2def4));
 m.load(0x801edb18,original.subarray(0x142258,0x142380));
 for(let id=0;id<256;id++){
  let wanted;
  if(native)wanted=fixture.rewards.rows[id];
  else{
   let row=0;while(original.readUInt16BE(0x142258+row*8)!==0&&original.readUInt16BE(0x142258+row*8)!==id)row++;
   wanted={slots:[0,1,2].map(s=>original.readUInt16BE(0x14225a+row*8+s*2)),weights:[1,1,1]};
  }
  const [a,b,c]=wanted.weights,total=a+b+c;
  const randoms=[...new Set([0,1,2,Math.max(0,a-1),a,Math.max(0,a+b-1),a+b,total-1,total,0x3fffffff])];
  for(const random of randoms)for(const alignment of [0,8]){
   const sp=0x803ffe00+alignment;
   m.r.fill(0);m.ram.fill(0xa5,m.phys(sp-0x80),m.phys(sp+0x80));
   m.set('sp',sp);m.set('s0',0x12340000);m.set('s1',0x12341111);m.set('s2',0x12342222);m.set('s4',id);
   m.w8(0x80193670,0x7f);let reads=0,rolls=0;
   m.trap(0x80089f80,cpu=>{
    assert.ok(native);assert.equal(cpu.get('a0'),fixture.dataOffset+id*16);
    assert.equal(cpu.get('a1'),sp+0x20);assert.equal(cpu.get('a2'),16);
    assert.equal(cpu.get('a0')%16,0);assert.equal(cpu.get('a1')%8,0);
    cpu.load(cpu.get('a1'),rom.subarray(cpu.get('a0'),cpu.get('a0')+16));
    callerClobber(cpu);reads++;dmaCalls++;
   });
   m.trap(0x80092a60,cpu=>{callerClobber(cpu);cpu.set('v0',random);rolls++;});
   m.call(ram(0x10245c),ram(0x1024d4));
   const roll=random%total,slot=roll<a?0:roll<a+b?1:2;
   assert.equal(m.get('s3'),wanted.slots[slot],`class ${id} roll ${random} native ${native}`);
   assert.equal(m.get('v0'),0x7f);assert.equal(rolls,1);assert.equal(reads,native?1:0);
   for(const [reg,value] of [['s0',0x12340000],['s2',0x12342222],['s4',id],['sp',sp]])assert.equal(m.get(reg),value);
   if(native)assert.equal(m.get('s1'),0x12341111);
   for(let at=0;at<0x80;at++)if(!native||at<0x20||at>=0x30)assert.equal(m.r8(sp+at),0xa5,`guard ${at}`);
   for(let at=-0x80;at< -0x40;at++)assert.equal(m.r8(sp+at),0xa5);
   if(native)nativeCases++;else retailCases++;
  }
 }
}
console.log(`PASS creature reward instructions: ${retailCases} retail and ${nativeCases} native cases, all 256 class IDs, zero/max weights, ${dmaCalls} original DMA-wrapper calls, both stack alignments, register/stack guards in 4 MiB RAM. Lower PI/cache hardware and full battles are outside this check.`);
