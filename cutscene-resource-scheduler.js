// Candidate resident resource-pass scheduling. Clock: one declared pass, not a video frame.
window.OB64=window.OB64||{};
(function(OB64){
 'use strict';
 var POOL=0x800e82c8,STRIDE=168;
 function stop(message,code){var e=new Error(message);e.code=code||'dialogue-scheduler-input';throw e;}
 // Native initialization restarts its stopping point after every initialized slot.
 function* initializePlan(read,initialize){var slot=0,last=0,visits=0;do{if(++visits>256)stop('Resource initialization exceeded its bounded circular scan.');var r=read(slot);if((r.flags&0xa000)===0x8000&&r.initialize){yield* initialize(slot);last=slot;}slot=(slot+1)%6;}while(slot!==last);}
 function Scheduler(engine,input,rom){
  if(!input||input.kind!=='resident-resource-pass-v1'||!engine.lifecycle||!Number.isInteger(input.directorSlot)||input.directorSlot<0||input.directorSlot>5||!Number.isInteger(input.directorCallback)||input.directorCallback<=0)stop('Resource scheduling requires shared lifecycle and an explicit Director resource binding.');
  if(input.directorCallback===0x80226190&&!['captured-mode-two-v1','rom-mode-two-v1'].includes(input.directorBinding))stop('Mode-two resource scheduling requires a qualified callback binding.');
  if(![0x80225abc,0x80226190].includes(input.directorCallback)||!(rom instanceof Uint8Array)||!OB64.cutsceneResourceSchedulerWords)stop('The Director callback binding lacks a qualified scheduling contract.');
  var view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength);OB64.cutsceneResourceSchedulerWords.forEach(function(r){if(r[0]+4>rom.length||view.getUint32(r[0])!==r[1])stop('Resource scheduling code differs from its qualified ROM.','dialogue-scheduler-image');});
  var c=input.controller;
  if(!c||!Number.isInteger(c.throughPass)||c.throughPass<0||c.throughPass>29999||!Array.isArray(c.changes)||!c.changes.length||c.changes.length>60000)stop('Resource scheduling requires bounded controller changes.');
  var previous=-1;c.changes.forEach(function(r){if(!r||!Number.isInteger(r.pass)||r.pass<=previous||r.pass>c.throughPass||!['actionMask','directionMask','historyMask','dummyMask'].every(function(k){return Number.isInteger(r[k])&&r[k]>=0&&r[k]<=65535;}))stop('Controller changes must be ordered unsigned masks.');previous=r.pass;});
  if(c.changes[0].pass!==0)stop('Controller state must start at pass zero.');
  if(!Array.isArray(input.helperOutcomes)||input.helperOutcomes.length>10000)stop('Resource scheduling requires an explicit external helper outcome stream.');
  input.helperOutcomes.forEach(function(r){OB64.cutsceneDialogue.validateHelper(r);if(![0x800ea9bc,0x800934b0,0x80093540,0x8019d5d0,0x80070f30].includes(r.address))stop('Computed archive/free services must not receive external outcomes.');});
  if(input.pageAdvancePolicy!==undefined&&!['neutral','automatic'].includes(input.pageAdvancePolicy))stop('Unsupported dialogue page policy.');
  if(input.dialogueChoicePolicy!==undefined&&!['manual','first'].includes(input.dialogueChoicePolicy))stop('Unsupported dialogue choice policy.');
  this.engine=engine;this.machine=engine.machine;this.input=input;this.helperCursor=0;this.controlCursor=0;this.trace=[];
  // The native priority rebuild resets its count, then writes each live queue
  // entry before reading it. Preserve the initial live prefix; reserve output
  // capacity for all six slots without treating unused cells as caller inputs.
  var m=this.machine,count=m.get(0x800c49d0,2);
  if(count<=6){
  for(var q=0;q<count;q++)m.region(0x800c4c10+q*2,2,false);
  var start=-1;
  for(var qi=0;qi<=12;qi++){
    var present=qi<12&&m.regions.some(r=>0x800c4c10+qi>=r.address&&0x800c4c10+qi<r.address+r.bytes.length);
    if(qi<12&&!present){if(start<0)start=qi;}
    else if(start>=0){m.regions.push({address:0x800c4c10+start,bytes:new Uint8Array(qi-start),writable:true});start=-1;}
  }
  }
  var self=this;this.machine.sharedOutcome=function(pc){if(![0x800ea9bc,0x800934b0,0x80093540,0x8019d5d0,0x80070f30].includes(pc))return null;var row=input.helperOutcomes[self.helperCursor++];if(!row)stop('The explicit external helper outcome stream is exhausted at RAM 0x'+pc.toString(16)+'.','dialogue-helper-outcome');return row;};
 }
 Scheduler.prototype.read=function(slot){var a=POOL+slot*STRIDE,m=this.machine;return {flags:m.get(a,2),initialize:m.get(a+0x10),callback:m.get(a+0x14)};};
 Scheduler.prototype.queue=function*(kind){yield* this.engine.service({service:kind,eligible:true,ownerId:'resource-pool',helpers:[]});this.trace.push({service:kind});};
 Scheduler.prototype.dialogue=function*(slot,kind){
  var r=this.read(slot),owner=this.engine.owners[slot];
  if(r.initialize===0x8022643c&&this.colorService){if(kind==='callback'&&r.callback!==0x80226538)stop('The color resource callback differs from its qualified binding.','dialogue-scheduler-callback');this.machine.put(0x800c4c20,slot);yield* this.colorService(slot,kind);this.trace.push({service:kind,slot:slot,resource:'color'});return;}
  if(r.initialize!==0x80198be8||(kind==='callback'&&r.callback!==0x8019981c)||!owner)stop('A reached resource callback lacks a supported dialogue owner.','dialogue-scheduler-callback');
  this.machine.put(0x800c4c20,slot);
  // Generated input belongs to this callback owner. Declared controller masks
  // retain the native dispatch semantics for every resource, including history.
  var controller={...this.control,queueHead:this.machine.get(0x800c4c10,2)},pulse=this.autoPulse;
  if(kind==='callback'&&pulse&&pulse.slot===slot&&pulse.owner===owner&&controller.queueHead===slot){
   controller.actionMask|=0x8000;controller.historyMask|=0x8000;
   this.autoPulse=null;this.autoRelease=pulse;
   this.trace.push({service:pulse.choice?'automatic-dialogue-choice':'automatic-page-acknowledgement',slot:slot,timing:'simulated-next-eligible-pass'});
  }
  yield* this.engine.service({service:kind,slot:slot,ownerId:owner.ownerId,eligible:true,helpers:[],controller:controller},true);
  if(this.machine.get(POOL+slot*STRIDE+3,1)&2)stop('This callback requires the native render-node attachment service.','dialogue-scheduler-render-context');
  this.trace.push({service:kind,slot:slot});
 };
 Scheduler.prototype.before=function*(pass){
  if(pass>this.input.controller.throughPass)stop('Controller declaration ends before this resource pass.','dialogue-controller-history');
  while(this.controlCursor+1<this.input.controller.changes.length&&this.input.controller.changes[this.controlCursor+1].pass<=pass)this.controlCursor++;
  this.control={...this.input.controller.changes[this.controlCursor]};this.trace=[];
  // The following pass supplies no generated pulse; it never clears manual input.
  const release=this.autoRelease;this.autoRelease=null;this.autoPulse=null;
  if(this.machine.get(0x800c4c26,2)!==0xffff)stop('A pending bulk resource cleanup is outside this pass profile.','dialogue-scheduler-cleanup');
  if(this.machine.get(0x800c49d0,2)>6)stop('The resource queue count exceeds the six-slot pool.','dialogue-scheduler-queue');
  if(this.machine.get(0x800c49d0,2)){yield* this.queue('opening');yield* this.queue('closing');yield* this.queue('priority');}
  yield* initializePlan(this.read.bind(this),function*(slot){if(slot===this.input.directorSlot&&this.initializeDirector)yield* this.initializeDirector(slot);else yield* this.dialogue(slot,'initialize');}.bind(this));
  yield* this.queue('priority');this.budget=this.machine.get(0x800c49d0,2);
  // Only the focused, ordinary dialogue resource can receive an automatic pulse.
  const page=this.machine.get(0x800c4c10,2),owner=this.engine.owners[page],binding=page<6?this.read(page):null;
  if(this.input.pageAdvancePolicy==='automatic'&&!release&&binding&&(binding.flags&0xa000)===0xa000&&binding.initialize===0x80198be8&&binding.callback===0x8019981c&&owner&&owner.payload&&owner.payload.length===1144){
   const textState=owner.payload[0x3c],choice=this.input.dialogueChoicePolicy==='first'&&textState===7&&!this.control.actionMask&&!this.control.directionMask&&!this.control.historyMask;
   if([3,6].includes(textState)||choice)this.autoPulse={slot:page,owner:owner,choice:choice};
  }

  for(var slot=0;slot<this.input.directorSlot;slot++)yield* this.callback(slot);
  var r=this.read(this.input.directorSlot);
  if((r.flags&0xa000)!==0xa000||r.callback!==this.input.directorCallback||this.budget<=0)stop('The declared Director resource is not eligible in this pass.','dialogue-scheduler-director');
  this.machine.put(0x800c4c20,this.input.directorSlot);this.trace.push({service:'director',slot:this.input.directorSlot});if(this.beforeDirector)yield* this.beforeDirector();
 };
 Scheduler.prototype.callback=function*(slot){
  if(this.budget<=0)return;
  var r=this.read(slot);if((r.flags&0xa000)!==0xa000||!r.callback)return;
  yield* this.dialogue(slot,'callback');this.budget--;this.machine.put(0x800c49d0,this.budget,2);
 };
 Scheduler.prototype.after=function*(terminal){
  if(terminal){if(this.helperCursor!==this.input.helperOutcomes.length)stop('The explicit helper stream contains unused outcomes.','dialogue-helper-outcome');return;}
  if(this.afterDirector)yield* this.afterDirector();
  this.budget--;this.machine.put(0x800c49d0,this.budget,2);
  for(var slot=this.input.directorSlot+1;slot<6;slot++)yield* this.callback(slot);yield* this.queue('priority');
  this.machine.put(0x800c4c20,0xffffffff);
  if(this.machine.get(0x800c4c26,2)!==0xffff)stop('A pending bulk resource cleanup is outside this pass profile.','dialogue-scheduler-cleanup');
 };
 OB64.cutsceneResourceScheduler={Scheduler:Scheduler,initializePlan:initializePlan};
})(window.OB64);
