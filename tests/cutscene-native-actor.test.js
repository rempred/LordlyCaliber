'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const crypto = require('crypto');
const hash = value => crypto.createHash('sha256').update(value).digest('hex');
const sourceHashes = {};
const root = path.resolve(__dirname, '../..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js',
  'cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js',
  'cutscene-codec.js','cutscene-runtime.js','cutscene-preview.js','cutscene-assets.js',
  'cutscene-renderer.js','cutscene-project.js','cutscene-export.js','cutscene-ui.js']) {
  const source=fs.readFileSync(path.join(root,'editor',file),'utf8');
  sourceHashes[file]=hash(source);
  vm.runInThisContext(source,{filename:file});
}
const native = OB64.cutsceneRuntime.nativeActor;
const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
let familyMatches=0;
const acceptedFamilies=[[81,82,83],[84,85],[86,87],[95,96],[99,100]];
for(let a=0;a<256;a++) for(let b=0;b<256;b++) {
  const expected=a===b || acceptedFamilies.some(group=>group.includes(a)&&group.includes(b));
  assert.strictEqual(native.classFamilyMatch(a,b),expected);
  if(expected) familyMatches++;
}
assert.strictEqual(familyMatches,270);
assert(native.classFamilyMatch(0x151,0x153));
function pose() { return {decoderMode:0,poseCursor:-1,poseDelay:0,poseFrame:0,
  displayedFrameToken:0,poseStateIndex:0,x:0,y:0,z:0,material:Array(16).fill(255),materialDelta:Array(16).fill(0)}; }
function resolve(records) {return ()=>({records:records.map(([opcode,...operands])=>({opcode,operands}))});}
function bits(x) {const b=Buffer.alloc(4);b.writeFloatBE(x);return b.toString('hex');}
let actor={x:0,z:0};
let job={vx:2,vz:0,remaining:3,pauseByte:0,elapsed:0};
assert.strictEqual(native.createMovement(actor,job,[7,0,-1000,-1000,100000,0,1,0]),job);
assert.strictEqual(actor.x,100);
const positions=[];
for(let i=0;i<3;i++) { const alive=native.advanceMovement(actor,job);positions.push(actor.x);assert.strictEqual(alive,i<2); }
assert.deepStrictEqual(positions,[102,104,106]);
actor={x:0,z:0};job=native.createMovement(actor,null,[7,0,-1000,-1000,1000,0,10,0]);
for(let i=0;i<10;i++)native.advanceMovement(actor,job);
assert.strictEqual(bits(actor.x),'3f800001');
job={vx:1,vz:1,remaining:0,pauseByte:1,elapsed:0};actor={x:0,z:0};
assert.strictEqual(native.advanceMovement(actor,job),false);assert.strictEqual(actor.x,0);
job.pauseByte=0;assert.strictEqual(native.advanceMovement(actor,job),true);assert.strictEqual(job.remaining,65535);
assert.throws(()=>native.createMovement({x:0,z:0},null,[7,0,-1000,-1000,1000,0,0,0]),/nonfinite/);
actor={x:0,z:0};job=native.createMovement(actor,null,[7,0,-1000,-1000,1000,0,65536,0]);
assert.strictEqual(job.remaining,0);assert.strictEqual(job.vx,Math.fround(1/65536));
native.advanceMovement(actor,job);assert.strictEqual(job.remaining,65535);
actor={x:0.05,z:0.05};const previousJob={vx:2,vz:0,remaining:3,pauseByte:1,elapsed:0};
assert.strictEqual(native.createMovement(actor,previousJob,[7,0,-1000,-1000,0,0,3,0]),previousJob);
assert.strictEqual(actor.x,0);assert.strictEqual(previousJob.pauseByte,1);
// Physical Bank 57/state 39 begins with an unresolved shared control. The
// reviewed fixture starts after that control, before its first frame dispatch.
const physical=catalog.getPhysicalPoseProgramByStateIndex(57,39);
actor=pose();actor.poseCursor=0;
const tokens=[];
for(let i=0;i<17;i++){assert.strictEqual(native.advancePose(actor,()=>physical,256),null);tokens.push(actor.displayedFrameToken);}
assert.deepStrictEqual(tokens,[31,32,33,34,35,35,36,36,37,37,37,36,36,36,36,36,36]);
actor=pose();assert.strictEqual(native.advancePose(actor,()=>physical,256),'shared-pose-control-17');
actor=pose();const loop=resolve([[1,10,2],[1,20,2],[4,1]]);
for(let i=0;i<4;i++)native.advancePose(actor,loop,256);
assert.strictEqual(actor.displayedFrameToken,20);assert.strictEqual(actor.poseCursor,1);
actor=pose();native.advancePose(actor,resolve([[1,31,0],[1,32,2]]),256);assert.strictEqual(actor.displayedFrameToken,32);
actor=pose();const delay=resolve([[21,7,1,6]]);native.advancePose(actor,delay,256);native.advancePose(actor,delay,256);
assert.strictEqual(actor.poseSequencerResult,1);assert.strictEqual(delay().records[actor.poseCursor].opcode,21);
actor=pose();assert.strictEqual(native.advancePose(actor,resolve([[4,0]]),256),'pose-dispatch-limit');
actor=pose();const states={0:resolve([[5,1]])(),1:resolve([[1,9,2]])()};
native.advancePose(actor,a=>states[a.poseStateIndex],256);
assert.strictEqual(actor.previousPoseStateIndex,0);assert.strictEqual(actor.poseStateIndex,1);assert.strictEqual(actor.displayedFrameToken,9);
actor=pose();native.advancePose(actor,resolve([[2,255,2],[12,1,2,255],[13,255,3],[16,255,255],[3,2]]),256);
assert.deepStrictEqual([actor.x,actor.y,actor.z],[2,2,-3]);assert(actor.material.every(v=>v===2));

// Update zero is the saved in-walk state, not scene launch or sample zero.
// Eligible movement and pose calls share the fixture's conditional schedule.
const graduation=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-graduation-native.json'),'utf8'));
const seed=graduation.seed;
actor={...pose(),x:seed.x,z:0,poseCursor:seed.cursor,poseDelay:seed.delay,
  displayedFrameToken:seed.frameToken,poseStateIndex:12};
job={vx:seed.velocityX,vz:seed.velocityZ,remaining:seed.countdown,pauseByte:seed.pause,elapsed:0};
const walkProgram=catalog.getPhysicalPoseProgramByStateIndex(30,12);
assert.deepStrictEqual(walkProgram.records.map(r=>[r.opcode,...r.operands]),graduation.entries);
let eligibleUpdates=0;
for(const sample of graduation.samples) {
  while(eligibleUpdates<sample.eligibleUpdate) {
    native.advanceMovement(actor,job);
    assert.strictEqual(native.advancePose(actor,()=>walkProgram,256),null);
    eligibleUpdates++;
  }
  assert.strictEqual(bits(actor.x).toUpperCase(),sample.xBits);
  assert.deepStrictEqual([actor.poseCursor,actor.poseDelay,actor.displayedFrameToken],sample.pose);
  assert.strictEqual(actor.y,0);assert.strictEqual(actor.z,0);
}
assert.strictEqual(eligibleUpdates,69);
assert.strictEqual(graduation.samples.length,120);
assert.strictEqual(job.remaining,55);

(async()=>{
 const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
 const z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const scene=catalog.directorScenes.find(s=>s.friendlyName==='Graduation Ceremony');
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene);
 const projected=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
 const records=Array(20).fill('00'.repeat(0xF8));
 const row=Buffer.alloc(0xF8);row.writeUInt32BE(256,0x40);row.writeUInt32BE(3,0x48);row[0xF6]=7;records[2]=row.toString('hex');
 const record=Buffer.alloc(0x150);record.writeInt32BE(4,0xE4);record.writeInt32BE(3,0xE8);record.writeInt16BE(51,0x134);
 record.writeInt32BE(-1,0xF0);record[0x147]=2;
 const slots=Array(28).fill(null);slots[4]={identity:'fixture-actor',recordHex:record.toString('hex'),movementHex:null};
 const input={schema:'ob64-cutscene-launch-inputs.v1',assetId:scene.assetId,invocationId:'diagnostic-fixture',
  sourceIdentity:'synthetic-regression:binding-fields',evidenceGrade:'Candidate',
  actorInputRows:{status:'known',value:records},existingActors:{status:'known',value:{slots,otherJobsEmpty:true}},
  currentUnitMembers:{status:'known',value:[7,255,255,255,255]},schedulerBranch:{status:'known',value:'normal'}};
 const ui=OB64.cutsceneUI.ensureState({z64,layout:{id:'us-rev0'}});
 let cancelled=0;ui.runtimeController={abort(){cancelled++;}};
 OB64.cutsceneUI.setNativeLaunchInputs(ui,scene,input);
 assert.strictEqual(cancelled,1);input.invocationId='changed-after-import';
 assert.strictEqual(ui.nativeLaunchInputsByAssetId[scene.assetId].invocationId,'diagnostic-fixture');
 assert.throws(()=>OB64.cutsceneUI.setNativeLaunchInputs(ui,scene,{...input,assetId:'other'}),/matching resource/);
 assert.strictEqual(ui.nativeLaunchInputsByAssetId[scene.assetId].invocationId,'diagnostic-fixture');
 const frozenInput=ui.nativeLaunchInputsByAssetId[scene.assetId];
 // Actual Director dispatch through a minimal source-backed opcode program.
 function run(words,extra={}) {
  const bytes=new Uint8Array(words.length*4);const view=new DataView(bytes.buffer);words.forEach((v,i)=>view.setUint32(i*4,v>>>0));
  const fixtureScene={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:bytes.length,decodedWordCount:words.length}};
  if (extra.sceneMode !== undefined) fixtureScene.launchProfile={...scene.launchProfile,
    directorMode:{...scene.launchProfile.directorMode,value:extra.sceneMode}};
  const ir=OB64.cutsceneCodec.createIr(fixtureScene,bytes);
  return OB64.cutsceneRuntime.compile(projected.document,ir.program,fixtureScene,catalog,{z64,maxTicks:4,diagnosticAssumptions:false,nativeLaunchInputs:frozenInput,...extra});
 }
 const bound=run([0xA6,1,0,0x80000001]);
 assert.strictEqual(bound.states[0].actors.find(a=>a.id==='fixture-actor').slot,1);
 const stored=bound.states[0].actors.find(a=>a.id==='fixture-actor');
 const restored=OB64.cutsceneRuntime.decodeNativeActorState(stored.nativeActorState);
 assert.strictEqual(restored.poseCursor,-1);assert.strictEqual(restored.poseDelay,0);
 assert.strictEqual(restored.poseStateIndex,51);assert.strictEqual(restored.sourceRowOrdinal,2);
 assert.deepStrictEqual(restored.material,Array(16).fill(0));assert.deepStrictEqual(restored.materialDelta,Array(16).fill(0));
 assert.strictEqual(OB64.cutsceneRuntime.compactContextRuntime(bound).contextFrames[0].actors[0].nativeActorState,stored.nativeActorState);
 const interruptedParent={assetId:'parent-fixture',states:[bound.states[0]],terminated:false,
   outcome:'shared-pose-control-17',safetyLimited:false};
 const inherited=run([0x80000001],{sceneMode:0,contextRuntime:interruptedParent,contextTickOffset:1});
 assert.strictEqual(inherited.outcome,'context-input',
   'a stopped parent Actor decoder cannot become indefinitely reusable inherited state');
 assert(!bound.states[0].actors.some(a=>a.slot===4));
 const swappedInput=JSON.parse(JSON.stringify(frozenInput));
 const second=Buffer.from(record);second.writeInt32BE(1,0xE4);second[0x147]=5;
 swappedInput.existingActors.value.slots[1]={identity:'second-actor',recordHex:second.toString('hex'),movementHex:null};
 const swapped=run([0xA6,1,0,0x80000001],{nativeLaunchInputs:swappedInput});
 assert.strictEqual(swapped.states[0].actors.find(a=>a.id==='second-actor').slot,4);
 const noMatch=run([0xA6,1,1,0x80000001]);
 assert.strictEqual(noMatch.states[0].actors.find(a=>a.id==='fixture-actor').slot,4);
 assert(!noMatch.missingInputs.some(v=>v.includes('binding')));
 const modeZeroClass=run([0x92,1,3,0x80000001],{sceneMode:0});
 assert(!modeZeroClass.missingInputs.some(v=>v.includes('class-family')));
 const classInput=JSON.parse(JSON.stringify(frozenInput));
 const classRow=Buffer.from(row);classRow.writeUInt32BE(0x151,0x48);
 classInput.actorInputRows.value[2]=classRow.toString('hex');
 const classBound=run([0x92,257,0x153,0x80000001],{sceneMode:2,nativeLaunchInputs:classInput});
 assert.strictEqual(classBound.states[0].actors.find(a=>a.id==='fixture-actor').slot,1);
 const classNoMatch=run([0x92,1,0x54,0x80000001],{sceneMode:2,nativeLaunchInputs:classInput});
 assert.strictEqual(classNoMatch.states[0].actors.find(a=>a.id==='fixture-actor').slot,4);
 classInput.actorInputRows.value[0]=classRow.toString('hex');
 const firstRowMissingActor=run([0x92,1,0x53,0x80000001],{sceneMode:2,nativeLaunchInputs:classInput});
 assert.strictEqual(firstRowMissingActor.states[0].actors.find(a=>a.id==='fixture-actor').slot,4);
 classInput.actorInputRows.value[0]='00'.repeat(0xF8);
 const component=Buffer.from(record);component.writeInt32BE(6,0xE4);
 classInput.existingActors.value.slots[6]={identity:'second-component',recordHex:component.toString('hex'),movementHex:null};
 const occupant=Buffer.from(record);occupant.writeInt32BE(1,0xE4);occupant[0x147]=5;
 classInput.existingActors.value.slots[1]={identity:'destination-actor',recordHex:occupant.toString('hex'),movementHex:null};
 const compositeBound=run([0x92,1,0x52,0x80000001],{sceneMode:2,nativeLaunchInputs:classInput});
 const compositeActors=compositeBound.states[0].actors;
 assert.strictEqual(compositeActors.find(a=>a.id==='fixture-actor').slot,1);
 assert.strictEqual(compositeActors.find(a=>a.id==='destination-actor').slot,4);
 assert.strictEqual(compositeActors.find(a=>a.id==='second-component').slot,6);
 assert.strictEqual(OB64.cutsceneRuntime.decodeNativeActorState(compositeActors.find(a=>a.id==='fixture-actor').nativeActorState).sourceRowOrdinal,2);
 const absent=run([3,2,-1,-1,-1,-1000,-1000,-1000,-1,0x80000001]);
 assert.strictEqual(absent.outcome,'actor-input');
 const sentinel=run([3,-1,-1,-1,-1,-1000,-1000,-1000,-1,0x80000001]);
 assert(!sentinel.missingInputs.some(v=>v.includes('Actor State')));
 const roster=run([0x45,-1,-1,0x80000001]);assert.strictEqual(roster.outcome,'roster-construction-input');
 const empty=JSON.parse(JSON.stringify(frozenInput));empty.actorInputRows.value=Array(20).fill('00'.repeat(0xF8));
 const emptyRoster=run([0x45,-1,-1,0x80000001],{nativeLaunchInputs:empty});
 assert(!emptyRoster.missingInputs.some(v=>v.includes('roster')));
 const resetRecord=run([0x14,0,3,0,0,0,0,0,0,0,0x1C,0,7,
   0x14,0,3,0,0,0,0,0,0,0,0x80000001],{sceneMode:0,diagnosticAssumptions:true});
 assert.strictEqual(resetRecord.states[0].actors.find(a=>a.slot===0).transformChannel,0);
 const unknownPose=run([0x14,0,999,0,0,0,0,0,0,0,0x80000001],{diagnosticAssumptions:true});
 assert.strictEqual(OB64.cutsceneRuntime.decodeNativeActorState(
   unknownPose.states[0].actors.find(a=>a.slot===0).nativeActorState).poseStateIndex,null,
   'compact snapshots must preserve an unresolved selector instead of inventing native physical state -1');
 OB64.cutsceneUI.resetAll(ui);assert.deepStrictEqual(ui.nativeLaunchInputsByAssetId,{});
 const contextCases=[];
 for(const [id,expected] of [['rom-director:01F79F48','modeled-termination'],
  ['rom-director:01F81C06','modeled-termination'],['rom-director:01F8F56E','external-input']]) {
  const selected=catalog.getScene(id);ui.selectedSceneId=selected.sceneId;
  await OB64.cutsceneUI.loadScene({z64,layout:{id:'us-rev0'}},ui,selected);
  const result=ui.runtimeByAssetId[id];
  assert(!result.missingInputs.some(v=>v.includes('missing-pose-program')),id+' must not overwrite child-owned State from a packed parent delta');
  assert(result.outcome===expected || result.outcome==='context-input',id+' must preserve the previous path or stop at its now-explicit parent boundary');
  if(result.outcome==='context-input') assert(result.missingInputs.some(v=>v.includes('concurrent Director context')));
  contextCases.push({assetId:id,outcome:result.outcome});OB64.cutsceneUI.resetAll(ui);
 }
 console.log(JSON.stringify({status:'pass',command:process.argv,node:process.version,sourceHashes,
   generatorSha256:hash(fs.readFileSync(__filename)),romV64Sha256:hash(raw),romZ64Sha256:hash(z64),
   nativeMovementPositions:positions,bank57State39Tokens:tokens,contextCases,
   familyMatches,graduationSamples:graduation.samples.length,graduationEligibleUpdates:eligibleUpdates,
   bindingSlot:1,missingActor:absent.outcome,nonemptyRoster:roster.outcome,scope:'Offline native fixtures; synthetic binding inputs do not prove a natural launch. Graduation starts at saved in-walk state; scheduling is conditional, pose aliases every 24 updates, and Actor lifetime remains Supported.'},null,2));
})().catch(e=>{console.error(e.stack || e);process.exitCode=1;});
