'use strict';
// Replay a seeded preview in the independent original-instruction harness.
// Resource allocation, actor height and the impact sink are controlled; effect
// scheduling, cue lookup and drawing-side callbacks run the original code.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const expected=JSON.parse(fs.readFileSync(process.argv[4]));
let tick=0,pending=-1;const cues=[...expected.initial];
class AudioMachine extends FloatMips {
 exec(pc,next){
  if(pc===0x801f9dfc)pending=this.get('a2');
  if(pc===0x801c5938){cues.push([tick,pending,this.get('a1'),this.get('a0')|0]);pending=-1;}
  return super.exec(pc,next);
 }
}
const m=new AudioMachine();m.ram=Uint8Array.from(fs.readFileSync(process.argv[3]));m.set('sp',0x807ff000);
let heap=0x80580000;
m.trap(0x80070f30,c=>{const size=(c.get('a0')+15)&~15;c.set('v0',heap);heap+=size;assert.ok(heap<0x80600000);});
m.trap(0x800712c4,()=>{});
m.trap(0x80089f80,c=>c.load(c.get('a1'),rom.subarray(c.get('a0')&0xfffffff,(c.get('a0')&0xfffffff)+c.get('a2'))));
m.trap(0x801ca100,c=>c.set('v0',38));m.trap(0x801c893c,c=>c.set('v0',0));
const call=(a,arg)=>{if(arg!==undefined)m.set('a0',arg);m.call(a,0x80000100,2000000);};
while(m.r32(0x80220d88)!==0&&tick<1200){
 call(0x8020d9fc);call(0x8020d3d4);
 m.w32(0x800e9ba0,0x80600000);m.w32(0x801969b0,0x80680000);m.w32(0x80197178,0x806c0000);
 call(0x8020e224,0);call(0x801b0300);call(0x8020e3a0,1);tick++;
}
assert.equal(tick,expected.ticks,'Original effect completion');assert.deepEqual(cues,expected.cues,'Original cue IDs, order, contexts and request ticks');
console.log(`PASS ${cues.length} cue requests across ${tick} original effect ticks`);
