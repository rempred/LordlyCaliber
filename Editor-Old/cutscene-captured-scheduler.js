// Native auxiliary state for bounded captured continuation and shared resource services.
window.OB64=window.OB64||{};
(function(O){
 'use strict';
 function fail(message,code){const e=new Error(message);e.code=code||'resume-native-input';throw e;}
 function bytes(h){if(typeof h!=='string'||!h.length||h.length%2||!/^[a-f0-9]+$/i.test(h))fail('Captured native memory requires hexadecimal bytes.');return Uint8Array.from(h.match(/../g),x=>parseInt(x,16));}
 function Scheduler(rom,resume,snapshot){
  const continuous=resume.entry==='normal-mode-two-continuous',input=resume.nativeScheduler,data=continuous?O.cutsceneCapturedContinuousCode:O.cutsceneCapturedSchedulerCode;
  this.continuous=continuous;
  if(!input||input.kind!==(continuous?'native-captured-continuous-v1':'native-held-movement-v1')||!Array.isArray(input.memory)||!data||!O.cutsceneDialogue||!O.cutsceneImageEcho)fail('Captured scheduling requires qualified native code and explicit initial memory.');
  const view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),code={};
  (data.references||[]).forEach(r=>{if(r[1]+4>rom.length||view.getUint32(r[1])!==r[2])fail('Captured scheduler replacement differs from qualified ROM.','resume-native-image');});
  data.words.forEach(r=>{if(r[1]+4>rom.length||view.getUint32(r[1])!==r[2])fail('Captured scheduler code differs from the qualified ROM.','resume-native-image');code[r[0]]=r[2];});
  if(continuous)delete code[0x80230b24];
  const regions=input.memory.map(r=>{if(!Number.isInteger(r.address)||r.address<0x80000000||r.address+r.hex.length/2>0x80400000)fail('Captured native memory lies outside RDRAM.');return {address:r.address,bytes:bytes(r.hex),writable:true};});
  regions.push({address:0x807fe000,bytes:new Uint8Array(8192),writable:true});
  this.machine=new O.cutsceneDialogue.Machine(code,regions);O.cutsceneImageEcho.installFpu(this.machine);
  const scheduler=this;
  const nativeStep=this.machine.step.bind(this.machine);this.machine.step=function(pc){try{
   if(continuous && pc===data.hooks.movement){scheduler.validateMovement();scheduler.callbacks.movement();}
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
   if(continuous && pc===0x80230b24){this.machine.r[2]=this.callbacks.dialogueQuery(this.machine.r[4]&255)&255;return true;}
   if(pc===data.hooks.movement){
    this.validateMovement();
    this.callbacks.movement();this.sync();return true;
   }
   if(pc===data.hooks.actors){if(continuous)this.assertActors();this.callbacks.actors();this.sync();return true;}
   if(continuous)fail('Captured scheduler reached an unsupported helper at RAM 0x'+pc.toString(16),'resume-native-helper');
   return false;
  };
 }
 Scheduler.prototype.assertActors=function(final){
  const m=this.machine,owner=this.resume.primaryOwnerAddress;
  this.callbacks.records().forEach(row=>{const a=m.get(owner+24+row.slot*4);if(!a||row.bytes.some((v,i)=>m.get(a+i,1)!==v))fail('Shared Actor differs from current native computation for slot '+row.slot+'.','resume-actor-comparison');if(final)this.actorComparisons=(this.actorComparisons||0)+1;});
 };
 Scheduler.prototype.validateMovement=function(){
  if(this.machine.get(0x80197b14)&8)fail('Mode-two proximity movement requires its separate native helper.','resume-movement-helper');
  this.callbacks.records().forEach(row=>{if(row.movement && row.bytes[0x145]&1)fail('Mode-two terrain movement requires its separate native helper.','resume-movement-helper');});
 };
 Scheduler.prototype.sync=function(){
  const m=this.machine,owner=this.resume.primaryOwnerAddress;
  this.callbacks.records().forEach(row=>{const a=m.get(owner+24+row.slot*4);if(!a)fail('A shared Actor lost its native owner.');row.bytes.forEach((b,i)=>m.put(a+i,b,1));const p=m.get(owner+0xf8+row.slot*4);if(row.movement){if(!p)fail('Shared movement lost its native owner.');const v=new DataView(new ArrayBuffer(16));v.setFloat32(0,row.movement.vx);v.setFloat32(8,row.movement.vz);v.setUint16(12,row.movement.remaining);v.setUint8(14,row.movement.pauseByte);for(const i of [0,1,2,3,8,9,10,11,12,13,14])m.put(p+i,v.getUint8(i),1);}else if(p)fail('Movement cleanup exceeds this held-query window.');});
 };
 Scheduler.prototype.advance=function*(callbacks){
  this.callbacks=callbacks;this.sync();
  yield* this.machine.run(this.data.entry,[],[],262144);
  if(!this.continuous && this.machine.get(0x8022a950)!==this.snapshot.observedParserCursor)fail('Native parsing left the qualified held movement query.','resume-transition-input');
  this.updates++;
  const p=this.machine.get(this.resume.primaryOwnerAddress+0x19f0),m=this.machine;
  if(!p)fail('Mode-two presentation state is unavailable.','resume-presentation-input');
  const f=o=>{const b=new DataView(new ArrayBuffer(4));b.setUint32(0,m.get(p+o));return b.getFloat32(0);};
  return {translateX:f(0x30),translateY:f(0x34),scaleX:f(0x24),scaleY:f(0x28)};
 };
 Scheduler.prototype.compareDialogueRegistration=function(slot){
  const a=0x800e82c8+slot*168;for(let i=0;i<168;i++)if(this.machine.get(a+i,1)!==this.engine.machine.get(a+i,1))fail('Shared dialogue construction differs from current native computation.','resume-dialogue-comparison');
  this.dialogueComparisons=(this.dialogueComparisons||0)+1;
 };
 Scheduler.prototype.dialoguePoint=function(slot){
  const m=this.machine,actor=m.get(this.resume.primaryOwnerAddress+24+slot*4);
  if(!actor)fail('Dialogue placement requires a current captured Actor.','dialogue-constructor-placement');
  const input=0x807fe100,output=0x807fe120;for(let i=0;i<12;i++)m.put(input+i,0,1);
  const run=m.run(0x8022d534,[input,output,actor+0xa0],[],16384);while(!run.next().done){}
  const f=a=>{const b=new DataView(new ArrayBuffer(4));b.setUint32(0,m.get(a));return b.getFloat32(0);};return {x:f(output),y:f(output+4)};
 };
 Scheduler.prototype.attachColorResources=function(engine,arena,rom){
  if(!arena||arena.byteLength!==16||!Number.isInteger(arena.address)||arena.address<0x80300000||arena.address+16>0x80700000||arena.address%16)fail('Captured color service requires a sixteen-byte preview arena.','resume-color-arena');
  for(let i=0;i<1008;i++)if(this.machine.get(0x800e82c8+i,1)!==engine.machine.get(0x800e82c8+i,1))fail('Captured resource memories disagree on initial ownership.','resume-resource-owner');
  if(this.machine.get(0x8018fc10)!==0)fail('Captured color services require an explicitly empty initial color owner.','resume-color-owner');
  var m=engine.machine,self=this,data=O.cutsceneDirectorLaunchCode,view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength);
  if(m.regions.concat(this.machine.regions).some(r=>r.address<arena.address+16&&arena.address<r.address+r.bytes.length))fail('Color preview arena overlaps resource memory.','resume-color-arena');
  m.regions.push({address:arena.address,bytes:new Uint8Array(16),writable:true});
  data.words.forEach(r=>{if(view.getUint32(r[1])!==r[2])fail('Color service differs from qualified ROM.','resume-color-code');m.code[r[0]]=r[2];});
  const table=data.tables.find(r=>r.address===0x8022ab80),b=bytes(table.hex);
  if(b.some((v,i)=>v!==rom[table.rom+i]))fail('Color registration table differs from ROM.','resume-color-code');
  for(const [a,n] of [[table.address,b.length],[0x8018fc10,4],[0x8022a720,144]])if(m.regions.some(r=>r.address<a+n&&a<r.address+r.bytes.length))fail('Computed color context overlaps supplied resource memory.','resume-color-arena');
  m.regions.push({address:table.address,bytes:b,writable:false});
  m.regions.push({address:0x8018fc10,bytes:new Uint8Array(4),writable:true});
  m.regions.push({address:0x8022a720,bytes:Uint8Array.from({length:144},(_,i)=>self.machine.get(0x8022a720+i,1)),writable:true});
  this.engine=engine;this.colorArena=arena;this.colorSerial=0;this.colorOwned=false;
  const old=m.serviceHelper;m.serviceHelper=function(pc){
   if(pc===0x80070f30&&self.colorCall){if(m.r[4]!==12||self.colorOwned)fail('Color allocation exceeds its owned arena.','resume-color-owner');self.colorOwned=true;m.r[2]=arena.address;return true;}
   if(pc===0x800712c4&&m.r[4]===arena.address){if(!self.colorOwned)fail('Color release has no current owner.','resume-color-owner');self.colorOwned=false;return true;}
   if(pc===0x80093380){m.region(m.r[4],m.r[5],true);for(let i=0;i<m.r[5];i++)m.put(m.r[4]+i,0,1);m.r[2]=m.r[4];return true;}
   return old&&old(pc);
  };
 };
 Scheduler.prototype.syncResourceContext=function(engine){
  for(const [a,n] of [[0x800e82c8,1008],[0x800e7a30,168]])for(let i=0;i<n;i++)this.machine.put(a+i,engine.machine.get(a+i,1),1);
  const native=this.machine.get(0x8018fc10),current=engine.machine.get(0x8018fc10);
  if(native&&current)for(let i=0;i<12;i++)this.machine.put(native+i,engine.machine.get(current+i,1),1);
 };
 Scheduler.prototype.createColor=function(words){const result=O.cutsceneDirectorLaunch.prototype.createColor.call(this,words),a=this.machine.get(0x8018fc10),b=this.engine.machine.get(0x8018fc10);if(!a||!b||Array.from({length:12},(_,i)=>i).some(i=>this.machine.get(a+i,1)!==this.engine.machine.get(b+i,1)))fail('Shared color construction differs from current native computation.','resume-color-comparison');this.colorComparisons=(this.colorComparisons||0)+1;return result;};
 Scheduler.prototype.color=function(){return O.cutsceneDirectorLaunch.prototype.color.call(this);};
 Scheduler.prototype.readEngine=function(a,n){return O.cutsceneDirectorLaunch.prototype.readEngine.call(this,a,n);};
 Scheduler.prototype.releaseColor=function(){return O.cutsceneDirectorLaunch.prototype.releaseColor.call(this);};
 O.cutsceneCapturedScheduler=Scheduler;
})(window.OB64);
