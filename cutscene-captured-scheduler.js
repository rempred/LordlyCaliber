// Native auxiliary state for a prospective, held movement-query window.
window.OB64=window.OB64||{};
(function(O){
 'use strict';
 function fail(message,code){const e=new Error(message);e.code=code||'resume-native-input';throw e;}
 function bytes(h){if(typeof h!=='string'||!h.length||h.length%2||!/^[a-f0-9]+$/i.test(h))fail('Captured native memory requires hexadecimal bytes.');return Uint8Array.from(h.match(/../g),x=>parseInt(x,16));}
 function Scheduler(rom,resume,snapshot){
  const input=resume.nativeScheduler,data=O.cutsceneCapturedSchedulerCode;
  if(!input||input.kind!=='native-held-movement-v1'||!Array.isArray(input.memory)||!data||!O.cutsceneDialogue||!O.cutsceneImageEcho)fail('Captured scheduling requires qualified native code and explicit initial memory.');
  const view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),code={};
  data.words.forEach(r=>{if(r[1]+4>rom.length||view.getUint32(r[1])!==r[2])fail('Captured scheduler code differs from the qualified ROM.','resume-native-image');code[r[0]]=r[2];});
  const regions=input.memory.map(r=>{if(!Number.isInteger(r.address)||r.address<0x80000000||r.address+r.hex.length/2>0x80400000)fail('Captured native memory lies outside RDRAM.');return {address:r.address,bytes:bytes(r.hex),writable:true};});
  regions.push({address:0x807fe000,bytes:new Uint8Array(8192),writable:true});
  this.machine=new O.cutsceneDialogue.Machine(code,regions);O.cutsceneImageEcho.installFpu(this.machine);
  const nativeStep=this.machine.step.bind(this.machine);this.machine.step=function(pc){try{
   const w=this.code[pc],op=w>>>26;
   if([34,38,42,46].includes(op)){
    const rs=w>>>21&31,rt=w>>>16&31,a=(this.r[rs]+(w<<16>>16))>>>0,k=a&3,base=a-k;
    if(op===34){for(let j=k;j<4;j++){const shift=24-8*(j-k),mask=(255<<shift)>>>0;this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;}}
    else if(op===38){for(let j=0;j<=k;j++){const shift=8*(k-j),mask=(255<<shift)>>>0;this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;}}
    else if(op===42){for(let j=k;j<4;j++)this.put(base+j,(this.r[rt]>>>(24-8*(j-k)))&255,1);}
    else for(let j=0;j<=k;j++)this.put(base+j,(this.r[rt]>>>(8*(k-j)))&255,1);
    this.r[0]=0;this.steps++;return {target:null,annul:false};
   }
   return nativeStep(pc);
  }catch(e){e.message+=' (captured scheduler PC 0x'+pc.toString(16)+', opcode 0x'+(this.code[pc]>>>0).toString(16)+')';throw e;}};
  this.resume=resume;this.snapshot=snapshot;this.input=input;this.updates=0;this.data=data;
  const m=this.machine;this.memoryBytes=regions.reduce((n,r)=>n+r.bytes.length,0);
  if(m.get(0x8022a974)!==resume.primaryOwnerAddress||m.get(0x8022a970)!==resume.secondaryOwnerAddress||m.get(0x8018fc19,1)!==2||m.get(0x8022a950)!==snapshot.observedParserCursor||m.get(resume.primaryOwnerAddress+0x1cb1,1)!==0)fail('Captured scheduler memory disagrees with its declared mode, owners, or parser cursor.');
  function match(a,h){const b=bytes(h);b.forEach((v,i)=>{if(m.get(a+i,1)!==v)fail('Captured scheduler memory disagrees with an imported record.','resume-native-record');});}
  match(resume.primaryOwnerAddress,resume.primaryOwnerHex);match(resume.menuRootAddress,resume.menuRootHex);
  if(m.get(0x8022a980)!==resume.registeredCounter || (m.get(0x801cfc70,1)<<24>>24)!==resume.tailTimer)fail('Captured native counters disagree with the resume declaration.');
  for(let i=0;i<30;i++)if(m.get(resume.primaryOwnerAddress+0x1ac0+i*4)!==0)fail('This shared Actor sweep requires empty secondary sprite slots.','resume-sprite-input');
  snapshot.slots.forEach((row,i)=>{const actor=m.get(resume.primaryOwnerAddress+24+i*4),move=m.get(resume.primaryOwnerAddress+0xf8+i*4);if(row){match(actor,row.recordHex);if(row.movementHex)match(move,row.movementHex);}else if(actor||move)fail('Captured scheduler retains an undeclared Actor.');});
  this.machine.serviceHelper=pc=>{
   if(pc===data.hooks.movement){
    // The shared planar integrator omits the two native mode-two helper branches.
    if(m.get(0x80197b14)&8)fail('Mode-two proximity movement requires its separate native helper.','resume-movement-helper');
    this.callbacks.records().forEach(row=>{if(row.movement && row.bytes[0x145]&1)fail('Mode-two terrain movement requires its separate native helper.','resume-movement-helper');});
    this.callbacks.movement();this.sync();return true;
   }
   if(pc===data.hooks.actors){this.callbacks.actors();this.sync();return true;}
   return false;
  };
 }
 Scheduler.prototype.sync=function(){
  const m=this.machine,owner=this.resume.primaryOwnerAddress;
  this.callbacks.records().forEach(row=>{const a=m.get(owner+24+row.slot*4);if(!a)fail('A shared Actor lost its native owner.');row.bytes.forEach((b,i)=>m.put(a+i,b,1));const p=m.get(owner+0xf8+row.slot*4);if(row.movement){if(!p)fail('Shared movement lost its native owner.');const v=new DataView(new ArrayBuffer(16));v.setFloat32(0,row.movement.vx);v.setFloat32(8,row.movement.vz);v.setUint16(12,row.movement.remaining);v.setUint8(14,row.movement.pauseByte);for(const i of [0,1,2,3,8,9,10,11,12,13,14])m.put(p+i,v.getUint8(i),1);}else if(p)fail('Movement cleanup exceeds this held-query window.');});
 };
 Scheduler.prototype.advance=function*(callbacks){
  this.callbacks=callbacks;this.sync();
  yield* this.machine.run(this.data.entry,[],[],262144);
  if(this.machine.get(0x8022a950)!==this.snapshot.observedParserCursor)fail('Native parsing left the qualified held movement query.','resume-transition-input');
  this.updates++;
  const p=this.machine.get(this.resume.primaryOwnerAddress+0x19f0),m=this.machine;
  if(!p)fail('Mode-two presentation state is unavailable.','resume-presentation-input');
  const f=o=>{const b=new DataView(new ArrayBuffer(4));b.setUint32(0,m.get(p+o));return b.getFloat32(0);};
  return {translateX:f(0x30),translateY:f(0x34),scaleX:f(0x24),scaleY:f(0x28)};
 };
 O.cutsceneCapturedScheduler=Scheduler;
})(window.OB64);
