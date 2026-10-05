'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
class EchoMips extends FloatMips {
 exec(pc,next){
  const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31;
  // Same bounded aligned-word-pair support as actor-matrix-mips.cjs.
  if([34,38,42,46].includes(op)){
   const a=(this.r[rs]+(w<<16>>16))>>>0;assert.equal(a&3,op===38||op===46?3:0);
   const aligned=(a&~3)>>>0;
   if(op===34||op===38){if(rt)this.r[rt]=this.r32(aligned);}else this.w32(aligned,this.r[rt]);
   return[next,(next+4)>>>0];
  }
  return super.exec(pc,next);
 }
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new EchoMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023e500,rom.subarray(0x2b3300,0x2b3d30));
m.load(0x8024049c,rom.subarray(0x2b529c,0x2b555c));
for(const [a,b]of [[0x22e90,0x231b0],[0x28740,0x29160]])m.load(a+0x8006fc00,rom.subarray(a,b));
m.load(0x800aa750,rom.subarray(0x3ab50,0x3ab54));m.load(0x80243c98,rom.subarray(0x2b8a98,0x2b8ae8));
m.trap(0x80092db0,c=>c.setSingle(0,Math.sin(c.single(12))));m.trap(0x800907f0,c=>c.setSingle(0,Math.cos(c.single(12))));
m.trap(0x8009cfe0,c=>c.setSingle(0,Math.atan2(c.single(12),c.single(14))));
const context=0x80300000,root=0x80302000,channel=0x80305000,heap=0x80306000,sp=0x803ffe00;
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const word=v=>{const b=Buffer.alloc(4);b.writeFloatBE(v);return b.readUInt32BE();};
const call=(p,args=[])=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p);assert.equal(m.get('sp'),sp);return m.get('v0');};
m.w32(0x8022a970,context);m.w32(0x8022a974,root);m.w32(root+0x1c58,channel);
m.trap(0x8023d4a4,c=>{m.w8(context+0x83d,c.get('a1'));m.w32(context+0x50,1);m.w32(context+0x4c,2);c.set('v0',2);});
m.trap(0x80080998,c=>c.load(c.get('a0'),read(c.get('a1'),c.get('a2'))));
let live=new Set;
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),64);let p=heap;while(live.has(p))p+=64;assert(p<heap+192);live.add(p);c.set('v0',p);});
m.trap(0x800712c4,c=>{if(c.get('a0'))assert(live.delete(c.get('a0')));});
function snap(){
 const fields=read(context,0x848),matrices=[];
 for(let i=0;i<3;i++){const p=m.r32(context+0x40+i*4);matrices.push(p?read(p,64).toString('hex'):null);fields.writeUInt32BE(p?i+1:0,0x40+i*4);}
 return {head:fields.subarray(0,0x54).toString('hex'),tail:fields.subarray(0x798).toString('hex'),matrices,channel:read(channel,88).toString('hex'),query:call(0x8023eaa0)};
}
function initialize(){
 live=new Set;m.load(context,Buffer.alloc(0x848));m.load(channel,Buffer.alloc(88));m.w32(context+0x4c,1);m.writeFloat(context+0x818,1);m.writeFloat(context+0x81c,1);
 [45,4/3,1,20000,1,272,80,127,0,0,0,0,1,0].forEach((v,i)=>m.writeFloat(0x8022a778+i*4,v));
 call(0x8023e694,[2,word(-102),word(39),0,word(29),word(29),130,4]);
 m.writeFloat(channel+0x40,23.5);m.writeFloat(channel+0x44,-19.25);m.writeFloat(channel+0x54,.7);
}
const rows=[];
for(const mode of [0,1,2])for(const duration of [0,1,4,5,6,15,30,255,256,257])for(const fade of [0,1,4]) {
 initialize();const initial=snap(),steps=[],args=[mode,duration,fade];call(0x8023e998,args);assert.equal(m.r32(root+0x1c08),1);steps.push({start:args,after:snap()});
 for(let i=0;i<(duration&255)+fade*5+4;i++){call(0x8023ead0);steps.push({after:snap()});}
 assert.equal(live.size,0);assert.equal(call(0x8023eaa0),0);rows.push({initial,steps});
}
// Retargeting does not clear the three existing matrices or sample counter.
for(const first of [0,1])for(const second of [0,1,2]) {
 initialize();const initial=snap(),steps=[];
 for(const args of [[first,30,4],[second,12,2]]) {
  call(0x8023e998,args);steps.push({start:args,after:snap()});
  for(let i=0;i<(args[1]===30?11:28);i++){call(0x8023ead0);steps.push({after:snap()});}
 }
 assert.equal(live.size,0);rows.push({initial,steps});
}
// The separate scheduler helper also visits twenty path-ribbon owners. They are
// explicitly absent here; its copied ribbon-width lookup is never used.
const fades=[];
for(const mode of [0,1,2])for(const sample of [0,1,2,3,4,5,20])for(const mask of [0,1,5,7])
for(const alphas of [[0,20,21],[40,70,255]])for(const pressed of [0,0x1000])for(const prompt of [0,1,2]) {
 m.load(context,Buffer.alloc(0x848));m.w32(context+0x824,sample);m.w8(context+0x83b,mode);m.w8(context+0x840,prompt);
 for(let i=0;i<3;i++){m.w32(context+0x40+i*4,mask&(1<<i)?heap+i*64:0);m.w8(context+0x820+i,alphas[i]);}
 m.w16(0x800e8100,pressed);const initial=snap(),steps=[];
 for(let i=0;i<3;i++){call(0x8024049c);steps.push(snap());}
 fades.push({mask,pressed,initial,steps});
}
fs.writeFileSync(process.argv[3],JSON.stringify({rows,fades}));console.log(`${rows.length} complete original echo sequences, ${rows.reduce((n,r)=>n+r.steps.length,0)} snapshots, ${fades.length} three-step fade/input sequences; allocator, copying, prior pixel construction and libm controlled; ribbon pool absent.`);
