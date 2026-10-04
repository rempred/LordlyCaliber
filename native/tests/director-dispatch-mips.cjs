'use strict';
// Original dispatcher and helpers. Only the external query producer is a double.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
// The control-only paths save/restore callee-saved FPR pairs but do not perform
// floating-point arithmetic. Preserve each opaque pair across SDC1/LDC1.
class ControlMips extends Mips {
 constructor(){super();this.fp=new Uint32Array(32);}
 exec(pc,next){
  const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31,address=(this.r[rs]+(w<<16>>16))>>>0;
  if(op===61){this.w32(address,this.fp[rt+1]);this.w32(address+4,this.fp[rt]);return[next,(next+4)>>>0];}
  if(op===53){this.fp[rt+1]=this.r32(address);this.fp[rt]=this.r32(address+4);return[next,(next+4)>>>0];}
  if(op===17&&rs===4){this.fp[w>>>11&31]=this.r[rt];return[next,(next+4)>>>0];}
  return super.exec(pc,next);
 }
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
// Derive placement from the actual overlay descriptor, not the early-boot delta.
const descriptor=0x387c0+14*40,vram=rom.readUInt32BE(descriptor),start=rom.readUInt32BE(descriptor+8),end=rom.readUInt32BE(descriptor+12);
assert.equal(start,0x281860);assert.equal(end,0x286bd0);
const address=offset=>(vram+offset-start)>>>0,stop=0x80000001,barrier=0x80000000,bridge=0x80000002;
const rows=[];
function check(name,words,initial={}) {
 const input={resume:0,mode:2,counter:0,controller:0,barrierState:0,producer:0,...initial};
 const m=new ControlMips();m.ram=new Uint8Array(0x400000);m.load(vram,rom.subarray(start,end));
 m.set('sp',0x803ffe00);const stream=0x80300010,object=0x80310000;
 m.load(stream-16,Buffer.alloc(words.length*4+32,0xa7));
 words.forEach((w,i)=>m.w32(stream+i*4,w));
 m.w32(0x8022a958,stream);m.w32(0x8022a950,input.resume);m.w32(0x8022a990,input.counter);
 m.w8(0x8018fc19,input.mode);m.w16(0x800e8100,input.controller);
 m.w32(0x8022a970,object);m.w8(object+0x840,input.barrierState);
 let producerCalls=0;m.trap(address(0x2864f8),c=>{assert.equal(c.get('a0'),77);c.set('v0',input.producer);producerCalls++;});
 m.call(address(0x284288));
 for(let i=0;i<16;i++){assert.equal(m.r8(stream-16+i),0xa7);assert.equal(m.r8(stream+words.length*4+i),0xa7);}
 assert.equal(m.get('sp'),0x803ffe00);
 rows.push({name,words:words.map(w=>w>>>0),initial:input,expected:{result:m.get('v0')|0,
  resume:m.r32(0x8022a950),counter:m.r32(0x8022a990),barrierState:m.r8(object+0x840),producerCalls}});
}
check('terminal',[stop]);
check('uncommitted barrier',[1,barrier,stop]);
check('handoff commits after barrier',[0,1,barrier,stop]);
check('reset',[2,0,barrier,stop],{counter:99});
check('resume saved node',[1,0,barrier,2,0,barrier,stop],{resume:3,counter:77});
for(let compare=0;compare<6;compare++)for(const actual of [-8,-7,-6])
 check(`signed comparison ${compare}/${actual}`,[0xe,compare,-7,1,bridge,0,barrier,stop],{counter:actual+1});
for(const producer of [-1,0,1])
 check(`Q4 producer ${producer}`,[0x57,0,0,77,1,bridge,0,barrier,stop],{producer});
check('nested false scan',[0xe,0,0,0xe,0,2,1,bridge,2,bridge,0,barrier,stop],{counter:1});
check('forward scan mode',[0x56,0xe,0,1,1,bridge,2,bridge,0,barrier,stop],{counter:1});
check('inactive query operands execute',[0xd,0x12,0x12,0,barrier,stop],{mode:2});
check('A button counter skip',[0xb5,2,50,0,bridge,barrier,stop],{controller:0x8000});
check('label yield',[0x59,7,0x80000005,7,stop]);
check('tracked object label redirect',[0,barrier,0x80000005,12345,stop],{barrierState:2});
fs.writeFileSync(process.argv[3],JSON.stringify({rows}));
console.log(`Original MIPS dispatcher: ${rows.length} bounded control cases, descriptor 14, 4 MiB RAM.`);
