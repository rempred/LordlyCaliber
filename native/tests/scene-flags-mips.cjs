'use strict';
// Run the generated target dispatcher, comparing unchanged control paths with
// the original ROM. Only the external story-flag storage service is trapped.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
class ControlMips extends Mips {
 constructor(){super();this.fp=new Uint32Array(32);}
 exec(pc,next){const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31,a=(this.r[rs]+(w<<16>>16))>>>0;
  if(op===61){this.w32(a,this.fp[rt+1]);this.w32(a+4,this.fp[rt]);return[next,next+4];}
  if(op===53){this.fp[rt+1]=this.r32(a);this.fp[rt]=this.r32(a+4);return[next,next+4];}
  if(op===17&&rs===4){this.fp[w>>>11&31]=this.r[rt];return[next,next+4];}
  return super.exec(pc,next);
 }
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const d=0x387c0+14*40,base=rom.readUInt32BE(d),lo=rom.readUInt32BE(d+8),hi=rom.readUInt32BE(d+12);
const entry=Number(process.argv[4]),code=fs.readFileSync(process.argv[3]);
function run(words,patched,mode=2,initial={}){
 const m=new ControlMips();m.ram=new Uint8Array(0x800000);m.load(base,rom.subarray(lo,hi));if(patched)m.load(0x80360000,code);
 m.set('sp',0x803ffe00);const stream=0x80300000,obj=0x80310000,flags={...initial},calls=[];
 words.forEach((w,i)=>m.w32(stream+i*4,w>>>0));m.w32(0x8022a958,stream);m.w32(0x8022a950,0);m.w32(0x8022a970,obj);m.w8(0x8018fc19,mode);
 m.trap(0x80000100,c=>{c.set('v0',flags[c.get('a0')]||0);});
 m.trap(0x80000110,c=>{flags[c.get('a0')]=c.get('a2');calls.push([c.get('a0'),c.get('a1'),c.get('a2')]);});
 m.trap(0x8016fef4,c=>{calls.push(['native',c.get('a0')]);c.set('v0',1);});
 m.call(patched?entry:base+0x284288-lo,0x80000080);
 assert.equal(m.get('sp'),0x803ffe00);
 return {flags,calls,result:m.get('v0'),resume:m.r32(0x8022a950),counter:m.r32(0x8022a990)};
}
const stop=0x80000001;
for(const mode of [0,1,2])for(const w of [[stop],[0x8e,123,456,stop],[1,0,0x80000000,stop],[0x8f,0,1,5,1,0x80000002,stop]])assert.deepEqual(run(w,true,mode),run(w,false,mode));
for(const flag of [0,98,255,256,767])for(const state of [0,1,2]){
 const actual=run([0x8e,0x4c430000|flag,state,stop],true,2,{[flag]:1});
 assert.equal(actual.flags[flag],state===1?1:0);assert.equal(actual.calls.length,1);
}
for(const state of [0,1]){
 const a=run([0x8f,0,state,0x4c430100,1,0x80000002,0,0x80000000,stop],true,2,{256:state});assert.equal(a.counter,1);
 const b=run([0x8f,0,state,0x4c430100,1,0x80000002,0,0x80000000,stop],true,2,{256:1-state});assert.equal(b.counter,0);
}
console.log('31 target-dispatcher checks: original controls, legacy queries, OFF/ON/Toggle and conditional custom flags.');
// Position tween executes beside the native pose sweep. This isolates the new
// movement math/lifetime; it does not claim native animation timing coverage.
{
 const m=new ControlMips();m.ram=new Uint8Array(0x800000);m.load(base,rom.subarray(lo,hi));m.load(0x80360000,code);m.set('sp',0x803ffe00);
 const root=0x80310000,sprite=0x80314000,stream=0x80318000,update=code.readUInt32BE(28);
 m.w32(0x8022a974,root);m.w32(root+0x1ac0,sprite);m.w32(sprite+0x24,10);m.w32(sprite+0x28,-20);m.w8(0x8018fc19,0);
 let poses=0;m.trap(0x80230fd4,()=>{poses++;m.w32(sprite+0x3c,77);});
 function dispatch(w){w.forEach((v,i)=>m.w32(stream+i*4,v>>>0));m.w32(0x8022a958,stream);m.w32(0x8022a950,0);m.call(entry,0x80000080);}
 dispatch([0x8e,0x4c4d0000,50,0x8e,0x4c4e0000,60,0x8e,0x4c540000,4,stop]);
 for(let i=1;i<=4;i++){m.call(update,0x80000080);assert.equal(m.r32(sprite+0x24),10+10*i);assert.equal(m.r32(sprite+0x28)|0,-20-10*i);assert.equal(m.r32(sprite+0x3c),77);}
 assert.equal(poses,4);m.call(update,0x80000080);assert.equal(m.r32(sprite+0x24),50);
 dispatch([0x8e,0x4c500000,-7,0x8e,0x4c510000,9,stop]);assert.equal(m.r32(sprite+0x24)|0,-7);assert.equal(m.r32(sprite+0x28)|0,-9);
 dispatch([0x8e,0x4c4d0000,13,0x8e,0x4c4e0000,9,0x8e,0x4c540000,4,stop]);
 m.w32(root+0x1ac0,0);m.call(update,0x80000080);assert.equal(m.r32(sprite+0x24)|0,-7);
 console.log('Target sprite motion: four-update path, final position, unchanged pose output, placement and released-slot handling passed.');
}
