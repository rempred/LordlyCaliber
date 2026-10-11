'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
class TurnMips extends FloatMips {
 exec(pc,next){
  const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31;
  // These original table copies use paired unaligned instructions at aligned
  // word boundaries. Each access covers the complete word; reject other lanes.
  if([34,38,42,46].includes(op)){
   const a=(this.r[rs]+(w<<16>>16))>>>0,last=op===38||op===46;
   assert.equal(a&3,last?3:0);const aligned=(a&~3)>>>0;
   if(op===34||op===38){if(rt)this.r[rt]=this.r32(aligned);}else this.w32(aligned,this.r[rt]);
   return[next,(next+4)>>>0];
  }
  return super.exec(pc,next);
 }
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const delta=0x8022e9e8-0x29e218,m=new TurnMips();m.ram=new Uint8Array(0x400000);
m.load(0x29c000+delta,rom.subarray(0x29c000,0x2a8d00));m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
m.load(0x801ceee0,rom.subarray(0x212370,0x212f00));
const root=0x80300000,actor=0x80310010,job=0x80320010,program=0x80330000,vocabulary=0x80350000,sp=0x803ffe00;
m.load(vocabulary,OB64.art.readCompressedResource(rom,0x109eeb0).decoded);m.w32(0x8022a97c,vocabulary);
const descriptor=rom.readUInt32BE(0x109f95e+0x594284+29*4),key=rom.readUInt32BE(descriptor+0x594288);
m.load(program,OB64.art.readCompressedResource(rom,key).decoded);
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),10);c.set('v0',job);});
m.trap(0x800712c4,c=>assert.equal(c.get('a0'),job));
m.trap(0x80232e44,c=>c.set('v0',program));m.trap(0x801c5938,()=>{});
m.trap(0x8022d534,c=>{m.writeFloat(c.get('a1'),987);m.writeFloat(c.get('a1')+4,654);});
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const results=[];
function run(from,to,direction,duration,animation=1,retarget=false){
 const initial=Buffer.alloc(336,0xa7);initial.fill(0,16,32);initial.writeInt32BE(3,0xe4);initial.writeInt32BE(30,0xe8);
 initial.writeInt16BE(0,0x134);initial.writeInt16BE(animation,0x138);initial[0x13d]=initial[0x146]=0;initial[0x13f]=from;
 [10.25,20.5,30.75].forEach((v,i)=>initial.writeFloatBE(v,0x11c+i*4));
 const initialJob=results.length%2?'a7a7a7a7a7a7a7a7a7a7':'';
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w32(root+24+3*4,actor);m.w32(root+0x168+3*4,initialJob?job:0);
 m.load(actor-16,Buffer.alloc(368,0xa7));m.load(actor,initial);m.load(job-16,Buffer.alloc(42,0xa7));m.set('sp',sp);
 const operations=[{args:[3,from,to,direction,duration]}];
 for(let i=0;i<20;i++){if(retarget&&i===2)operations.push({args:[3,to,from,-direction,12]});operations.push({});}
 const states=[];
 for(const op of operations){
  if(op.args){op.args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16,v));m.call(0x29c624+delta);}
  else m.call(0x29c790+delta);
  m.set('a0',3);m.call(0x29c92c+delta);
  states.push({actor:memory(actor,336),job:m.r32(root+0x168+3*4)?memory(job,10):'',query:m.get('v0')});
  assert.equal(m.get('sp'),sp);for(let i=0;i<16;i++){assert.equal(m.r8(actor-16+i),0xa7);assert.equal(m.r8(actor+336+i),0xa7);assert.equal(m.r8(job-16+i),0xa7);assert.equal(m.r8(job+10+i),0xa7);}
 }
 results.push({initial:initial.toString('hex'),initialJob,operations,states});
}
for(let from=0;from<4;from++)for(let to=0;to<4;to++)if(from!==to)for(const direction of [-1,1])for(const duration of [0,1,2,3,8,12,18,-3,65537,2147483647,-2147483648])run(from,to,direction,duration);
run(0,2,-1,12,9999);run(1,3,1,18,1,true);
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original actor turns: ${results.length} sequences; original State/resolver/immediate pose execute, allocation/resource registry/audio controlled.`);
