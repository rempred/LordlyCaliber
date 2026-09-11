'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
global.window=global;vm.runInThisContext('var OB64 = window.OB64 = {};');
const sourceHashes={};
for(const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js',
 'cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-runtime.js','cutscene-preview.js','cutscene-assets.js',
 'cutscene-renderer.js','cutscene-sprites.js','cutscene-project.js','cutscene-export.js','cutscene-ui.js']) {
 const source=fs.readFileSync(path.join(root,'editor',file),'utf8');sourceHashes[file]=hash(source);vm.runInThisContext(source,{filename:file});
}
const native=OB64.cutsceneRuntime.nativeActor,catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
const fixtures=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-actor-services.json'),'utf8'));
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
const z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
function seedRecord(slot=5) {
 const b=Buffer.alloc(0x150);for(let i=0;i<b.length;i++)b[i]=(i*7+3)&255;
 b.writeInt32BE(slot,0xE4);b.writeFloatBE(100.25,0x11C);b.writeFloatBE(25.5,0x120);b.writeFloatBE(-10.25,0x124);
 b.writeInt32BE(5,0xF0);b.writeInt32BE(99,0xF4);b[0x13C]=2;
 b.fill(10,0,16);b.fill(1,16,32);return b;
}
function registry(art,context,flagA,flagB,states) {
 return {sourceArt:art,ownerContext:context,flagA,flagB,handle:1,programs:states.map(state=>({state,programHex:'00'}))};
}
const checked=[];
(async()=>{
 const scene=catalog.directorScenes.find(s=>s.friendlyName==='Graduation Ceremony');
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene);
 const document=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog).document;
 function program(words) {
  const b=new Uint8Array(words.length*4),v=new DataView(b.buffer);words.forEach((w,i)=>v.setUint32(i*4,w>>>0));
  const selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:b.length,decodedWordCount:words.length},
   launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:2}}};
  return {selected,ir:OB64.cutsceneCodec.createIr(selected,b)};
 }
 function input(slots,rows=Array(20).fill('00'.repeat(0xF8))) {
  return {schema:'ob64-cutscene-launch-inputs.v1',assetId:scene.assetId,invocationId:'synthetic-services',sourceIdentity:'accepted-native-synthetic-fixtures',evidenceGrade:'Candidate',
   existingActors:{status:'known',value:{slots,otherJobsEmpty:true}},actorInputRows:{status:'known',value:rows},schedulerBranch:{status:'known',value:'normal'}};
 }
 function run(p,launch,extra={}) {return OB64.cutsceneRuntime.compile(document,p.ir.program,p.selected,catalog,{z64,maxTicks:26,diagnosticAssumptions:false,nativeLaunchInputs:launch,...extra});}
 const command=program([0xC2,5,9,0x80000001]),nodeId=command.ir.program.primitives[0].id;
 for(const fixture of fixtures.subordinates) {
  const info=fixture.input;
  const slots=Array(28).fill(null);
  for(const slot of info.occupiedSlots) slots[slot]={identity:'actor-'+slot,recordHex:seedRecord(slot).toString('hex'),movementHex:null};
  const rows=Array(20).fill('00'.repeat(0xF8)),row=Buffer.alloc(0xF8);
  row.writeUInt32BE(512,0x40);row.writeUInt32BE(info.art>>>0,0x48);row.writeUInt32BE(info.context,0x4C);
  for(let i=0;i<3;i++)row.writeUInt32BE(info.linkedMask&(1<<i)?0x600000+i*256:0,i*4);
  row.writeUInt16BE(1,0x38);rows[7]=row.toString('hex');
  const launch=input(slots,rows),stateWords=[0xC2,5,info.state,0x80000001],p=program(stateWords),id=p.ir.program.primitives[0].id;
  const states=[0,1,2].map(i=>(info.state+50*i)&65535).map(v=>v>=32768?v-65536:v);
  const clamped=[135,136,161,-1].includes((info.art<<16)>>16)?[0,50]:states;
  launch.poseRegistry={status:'known',value:{alternate:[registry(info.art|0,info.context,1,1,clamped)],ordinary:[]}};
  launch.subordinateServices={status:'known',value:[{nodeId:id,occurrence:0,allocations:[!info.allocationFails,!info.allocationFails],
   preparations:[0,1,2].map(()=>({status:'ready',sourceArt:info.art|0,ownerContext:info.context,flagA:1,flagB:1,equipment:1}))}]};
  const result=run(p,launch);
  if(!info.linkedMask) {assert.strictEqual(result.states[0].actors.length,info.occupiedSlots.length);continue;}
  if(!(info.linkedMask&1)) {assert.strictEqual(result.outcome,'subordinate-topology');continue;}
  if([10,25,123].includes(info.context) && info.occupiedSlots.length===28 && (info.linkedMask&6)) {
   assert.strictEqual(result.outcome,'subordinate-capacity');continue;
  }
  if(fixture.failure && fixture.failure.includes('allocator-null')) {
   assert.strictEqual(result.outcome,'subordinate-allocation-failed');
   assert.strictEqual(Buffer.from(result.states[0].actors.find(a=>a.id==='actor-5').source.recordHex,'hex').readInt32BE(0xF0),6);continue;
  }
  if(fixture.failure) continue;
  const actual=result.states[0].actors;
  const expected=fixture.records.filter((r,i)=>i===0 || i===fixture.records.length-1);
  for(const record of expected) {
   const actor=actual.find(a=>a.slot===record.slot);
   assert(actor,'expected slot '+record.slot);
   assert.strictEqual(hash(Buffer.from(actor.source.recordHex,'hex')).toUpperCase(),record.bytesSha256,
     JSON.stringify(info)+' complete record slot '+record.slot);
  }
  checked.push({context:info.context,mask:info.linkedMask,state:info.state,records:expected.length});
 }
 // Native no-match and known absent anchor return without inventing a record.
 assert(!run(command,input(Array(28).fill(null))).missingInputs.some(v=>v.includes('Subordinate')));
 const missing=run(command,null);assert.strictEqual(missing.outcome,'subordinate-row-input');
 // Preparation failure preserves the first component's writes and prevents later clones.
 const rows=Array(20).fill('00'.repeat(0xF8)),row=Buffer.alloc(0xF8);row.writeUInt32BE(512,0x40);row.writeUInt32BE(2,0x48);row.writeUInt32BE(10,0x4C);
 row.writeUInt32BE(1,0);row.writeUInt32BE(2,4);row.writeUInt16BE(1,0x38);rows[7]=row.toString('hex');
 const slots=Array(28).fill(null);slots[5]={identity:'anchor',recordHex:seedRecord().toString('hex'),movementHex:null};
 const absentPrep=run(command,input(slots,rows));assert.strictEqual(absentPrep.outcome,'subordinate-preparation-input');
 const partial=Buffer.from(absentPrep.states[0].actors.find(a=>a.id==='anchor').source.recordHex,'hex');
 assert.strictEqual(partial.readInt16BE(0x138),9);assert.strictEqual(partial.readInt32BE(0xF0),5);assert.strictEqual(absentPrep.states[0].actors.length,1);
 const fullSlots=Array.from({length:28},(_,i)=>({identity:'full-'+i,recordHex:seedRecord(i).toString('hex'),movementHex:null}));
 assert.strictEqual(run(command,input(fullSlots,rows)).outcome,'subordinate-capacity');
 const poseChanges=input(slots,rows);
 poseChanges.poseRegistry={status:'known',value:{alternate:[registry(2,10,1,1,[9,59])],ordinary:[]}};
 poseChanges.poseRegistry.value.alternate[0].programs[0].programHex='080000000000000c010203011104';
 poseChanges.subordinateServices={status:'known',value:[{nodeId,occurrence:0,allocations:[true],preparations:[0,1].map(()=>({status:'ready',sourceArt:2,ownerContext:10,flagA:1,flagB:1,equipment:1}))}]};
 const changedPose=run(command,poseChanges),clone=changedPose.states[0].actors.find(a=>a.slot===0);
 assert.deepStrictEqual([clone.baseX,clone.baseY,clone.baseZ],[93.25,27.5,0.75],
  'clone must inherit coordinates after the preceding synchronous pose');
 const cacheFull=JSON.parse(JSON.stringify(poseChanges));cacheFull.subordinateServices.value[0].preparations[0].status='cache-full';
 assert.strictEqual(run(command,cacheFull).outcome,'subordinate-cache-full');
 cacheFull.subordinateServices.value[0].preparations[0].equipment=2;
 assert.strictEqual(run(command,cacheFull).outcome,'subordinate-preparation-input',
  'a service result for another equipment tuple cannot establish a full cache here');
 const mutated=program([0x1D,5,100,0xC2,5,9,0x80000001]);
 assert.strictEqual(run(mutated,input(slots,rows)).outcome,'subordinate-anchor-record',
  'partially modeled preceding writes cannot leave a falsely complete anchor record');
 const invalidRegistry=JSON.parse(JSON.stringify(poseChanges));invalidRegistry.poseRegistry.value.alternate[0].programs[0].programHex='0101';
 assert.throws(()=>OB64.cutsceneRuntime.validateLaunchInputs(invalidRegistry,scene.assetId),/extent/);
 // Both named successful setup examples invoke their empty program immediately.
 const alternateResults=[];
 for(const named of fixtures.alternate.programs) {
  const words=[0x2A,0,named.sourceArt,7,named.flagB,named.flagA,named.ownerContext,0x80000001],p=program(words);
  const setup=Buffer.alloc(0x150);setup.fill(255,0,16);setup.writeInt32BE(named.sourceArt,0xE8);setup.writeInt32BE(named.ownerContext,0xEC);
  setup.writeInt32BE(-1,0xF0);setup.writeInt16BE(7,0x134);setup.writeInt16BE(7,0x138);setup.writeInt16BE(named.flagB,0x13A);setup[0x146]=named.flagA;setup[0x13D]=1;
  const occupied=Array(28).fill(null);occupied[0]={identity:'body-anchor',recordHex:seedRecord(0).toString('hex'),movementHex:null};
  const launch=input(occupied);
  launch.poseRegistry={status:'known',value:{alternate:[{...registry(named.sourceArt,named.ownerContext,named.flagA,named.flagB,[7]),handle:named.handle}],ordinary:[]}};
  launch.bodyPoseSetups={status:'known',value:[{nodeId:p.ir.program.primitives[0].id,occurrence:0,words:words.slice(0,7),recordHex:setup.toString('hex'),otherActorsUnchanged:true}]};
  const result=run(p,launch),actor=result.states[0].actors[0],decoded=OB64.cutsceneRuntime.decodeNativeActorState(actor.nativeActorState);
  assert.strictEqual(decoded.poseCursor,0);assert.strictEqual(decoded.poseDelay,0);assert.strictEqual(actor.displayedFrameToken,0);assert(!actor.poseBlocked);
  const initial={bank:named.sourceArt,decoderMode:1,poseStateIndex:7,poseCursor:-1,poseDelay:0,displayedFrameToken:0,poseFrame:0,material:Array(16).fill(255),materialDelta:Array(16).fill(0)};
  for(let i=0;i<24;i++) {assert.strictEqual(native.advancePose(initial,()=>({records:[]}),256),null);assert.strictEqual(initial.poseCursor,i);assert.strictEqual(initial.poseDelay,0);assert.strictEqual(initial.displayedFrameToken,0);}
  const unknown=JSON.parse(JSON.stringify(launch));delete unknown.poseRegistry;
  assert.strictEqual(run(p,unknown).outcome,'alternate-pose-registration');
  delete unknown.bodyPoseSetups;assert.strictEqual(run(p,unknown).outcome,'alternate-pose-setup');
  const sweepProgram=program(words.slice(0,7).concat(Array.from({length:23},()=>[0,0x80000000]).flat(),[0x80000001]));
  const swept=run(sweepProgram,launch);
  assert(swept.states.length>=24,JSON.stringify({states:swept.states.length,outcome:swept.outcome,missing:swept.missingInputs}));
  for(let tick=0;tick<24;tick++) {
   const sampled=swept.states[tick].actors[0],nativeState=OB64.cutsceneRuntime.decodeNativeActorState(sampled.nativeActorState);
   assert.strictEqual(nativeState.poseCursor,tick);assert.strictEqual(nativeState.poseDelay,0);assert.strictEqual(sampled.displayedFrameToken,0);
  }
  const paused=JSON.parse(JSON.stringify(launch));paused.schedulerBranch.value='alternate';
  const alternateSchedule=run(sweepProgram,paused);
  assert.strictEqual(OB64.cutsceneRuntime.decodeNativeActorState(alternateSchedule.states[23].actors[0].nativeActorState).poseCursor,0,
   'immediate pose is unconditional, but recurring sweep follows scheduler eligibility');
  const queryProgram=program(words.slice(0,7).concat([0x0F,0,1,0,0x80000001]));
  assert.strictEqual(run(queryProgram,launch).outcome,'ordinary-pose-query-input');
  const queryInput=JSON.parse(JSON.stringify(launch));
  const ordinary=registry(named.sourceArt,named.ownerContext,named.flagA,named.flagB,[7]);
  ordinary.programs[0].programHex='01012308';queryInput.poseRegistry.value.ordinary=[ordinary];
  const queried=run(queryProgram,queryInput);
  assert(queried.trace.some(r=>r.kind==='branch-query' && r.actual===1),
   'ordinary query opcode must differ from alternate empty-program zero');
  const spriteState=OB64.cutsceneSprites.create(z64,catalog);
  const nativeFrame=OB64.cutsceneSprites.frameForActor(spriteState,actor);
  assert(nativeFrame,'qualified empty pose still renders its retained native token');
  assert.strictEqual(nativeFrame.bodyPoseProgram.displayedFrameToken,0);
  assert.strictEqual(nativeFrame.bodyPoseProgram.nativeFrameSelection,true);
  const capturedInput=JSON.parse(JSON.stringify(launch));
  delete capturedInput.existingActors;
  const capturedSlots=Array(28).fill(null);
  capturedSlots[actor.slot]={identity:'qualified-alternate-snapshot',recordHex:actor.source.recordHex,movementHex:null};
  capturedInput.capturedSnapshot={status:'known',value:{slots:capturedSlots,sceneMode:0,observedParserCursor:0,resumeState:'unknown',otherJobOwners:'unknown'}};
  const capturedResult=run(p,capturedInput),capturedActor=capturedResult.states[0].actors[0];
  assert.strictEqual(capturedResult.outcome,'captured-snapshot-resume-input');
  assert.strictEqual(capturedResult.capturedSnapshot.executedUpdates,0);
  assert(capturedActor.bodyPoseProgram);
  const capturedFrame=OB64.cutsceneSprites.frameForActor(spriteState,capturedActor);
  assert(capturedFrame,'qualified alternate captured record retains its sprite consumer');
  assert.strictEqual(capturedFrame.bodyPoseProgram.nativeFrameSelection,true);
  assert.strictEqual(capturedFrame.bodyPoseProgram.artSource,named.sourceArt);
  const retainedOutsideState={...actor,bodyPoseProgram:{...actor.bodyPoseProgram,selector:32767}};
  assert(OB64.cutsceneSprites.frameForActor(spriteState,retainedOutsideState),
   'retained native token does not require resolving a stale requested-State directory');
  alternateResults.push({sourceArt:named.sourceArt,handle:named.handle,immediateCursor:decoded.poseCursor,calls:24,capturedAlternateFrame:true});
 }
 console.log(JSON.stringify({status:'pass',node:process.version,sourceHashes,generatorSha256:hash(fs.readFileSync(__filename)),
  fixtureSha256:hash(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-actor-services.json'))),checked,alternateResults,
  boundary:'Qualified synthetic service inputs; original-image agreement, complete resource loading, and natural launch are not claimed.'},null,2));
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
