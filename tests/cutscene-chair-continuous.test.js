'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..'),fixture=require(process.argv[2]?path.resolve(process.argv[2]):'./fixtures/cutscene-chair-continuous.json');
global.window=global;vm.runInThisContext('var OB64=window.OB64={};');
for(const file of ['data.js','parsers.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js',
 'cutscene-director.js','cutscene-codec.js','cutscene-dialogue-data.js','cutscene-dialogue-lifecycle-data.js','cutscene-dialogue.js','cutscene-dialogue-lifecycle.js','cutscene-runtime.js','cutscene-preview.js',
 'cutscene-assets.js','cutscene-sprites.js','cutscene-renderer.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',file),'utf8'),{filename:file});
const clone=value=>JSON.parse(JSON.stringify(value));
(async()=>{
 const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);
 for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.getScene(fixture.input.assetId);
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
 const run=(input=fixture.input,extra={})=>OB64.cutsceneRuntime.compile(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:input,diagnosticAssumptions:false,...extra});
 const before=JSON.stringify(fixture.input),result=run();
 assert(Buffer.byteLength(before)<=131072,'the direct JSON input fits the existing import ceiling');
 assert.equal(JSON.stringify(fixture.input),before);
 assert.equal(result.outcome,'modeled-termination');assert.equal(result.states.length,fixture.expectedUpdates);assert.equal(result.capturedSnapshot.executedUpdates,fixture.expectedUpdates);
 assert.deepStrictEqual(result.missingInputs,[]);assert.deepStrictEqual(result.assumptions,[]);
 assert.equal(result.resumedMenuSelection,null,'continuous execution does not claim the one-update menu-preservation result');
 assert.equal(result.trace.find(row=>row.kind==='direct-actor-creation').recordHex,fixture.expectedCreation);
 let compared=0;
 for(const expected of fixture.expectedMilestones){const state=result.states[expected.tick],dialogue=state.nativeExternal.dialogue;
  for(const row of expected.dialogues){const address=0x800e82c8+row.slot*0xa8;
   const region=dialogue.memory.find(r=>r.address<=address&&r.address+r.hex.length/2>=address+0xa8);
   assert.equal(region.hex.slice((address-region.address)*2,(address-region.address+0xa8)*2),row.recordHex);
   if(row.payloadHex)assert.equal(dialogue.owners[row.slot].payloadHex,row.payloadHex);
  }
  if(expected.colorHex){const bytes=Buffer.from(expected.colorHex,'hex');
   assert.deepStrictEqual(['remaining','duration','red','green','blue','ownershipFlag','alpha','targetAlpha','startAlpha'].map(k=>state.overlays[0][k]),
    [bytes.readInt16BE(0),bytes.readInt16BE(2),...bytes.subarray(4,11)]);}
 }
 for(const expected of fixture.expectedMilestones)for(const row of expected.actors){
  const actor=result.states[expected.tick].actors.find(a=>a.slot===row.slot),n=OB64.cutsceneRuntime.decodeNativeActorState(actor.nativeActorState);
  assert.deepStrictEqual([actor.baseX,actor.baseY,actor.baseZ,n.poseCursor,n.poseDelay,actor.displayedFrameToken,n.poseStateIndex,n.sourceRowOrdinal,actor.nativeUniformScale,...n.material,...n.materialDelta],row.values,`tick ${expected.tick}, slot ${row.slot}`);compared++;
 }
 const sprites=OB64.cutsceneSprites.create(z64,catalog);let frames=0,rendered=0,effects=0;
 for(const state of result.states){
  const actorFrames=OB64.cutsceneSprites.framesForPreview(sprites,state);
  assert.equal(Object.keys(actorFrames).length,state.actors.length);frames+=state.actors.length;
  const effectFrames=state.effects.filter(row=>row.payload.sourceSystem==='cutscene-sprite-native'&&!row.payload.nativeLifetimeEmpty).map(row=>{
   const payload=row.payload,image=OB64.cutsceneSprites.frameForActor(sprites,{poseId:payload.poseId,bank:payload.bank,animationKey:payload.animationKey,nativeFacing:payload.nativeFacing,
    variantSelector:payload.variantSelector,poseFrame:payload.poseFrame,poseLoop:payload.poseLoop,displayedFrameToken:payload.displayedFrameToken});
   assert(image);effects++;return {image,x:payload.stageX,y:payload.stageY,scale:payload.scale,rotationDegrees:payload.rotationDegrees||0,anchorX:image.anchorX,anchorY:image.anchorY};
  });
  const image=OB64.cutsceneRenderer.renderFrame(p.document,state,{actorFrames,effectFrames,projection:state.actorProjection,backgrounds:[],overlays:state.overlays||[],camera:state.cameraState,colorModulation:state.sceneColor});
  assert.equal(image.rgba.length,320*240*4);rendered++;
 }
 assert.deepStrictEqual(sprites.errors,{});
 const neutral=run(fixture.neutralInput);
 assert.equal(neutral.states.length,90);assert.equal(neutral.outcome,'prospective-update-limit');
 for(const row of fixture.originalDialogueSamples){const state=neutral.states[row.sample-1];
  assert.equal(state.nativeExternal.dialogue.owners[row.slot].payloadHex,row.payloadHex,`original payload sample ${row.sample}`);
  assert.deepStrictEqual(state.dialogue.find(d=>d.payload.nativeDialogue).payload.nativeDialogue.rectangle,row.rectangle,`original rectangle sample ${row.sample}`);
 }
 const shorter=clone(fixture.input);shorter.capturedResume.value.updates=50;
 const table=shorter.externalProducers.value.events;
 table.sequence=table.sequence.filter(([tick])=>tick<50);shorter.externalProducers.value.throughTick=49;
 const compact=run(shorter),expanded=clone(shorter);
 expanded.externalProducers.value.events=table.sequence.map(([tick,index])=>({...table.templates[index],tick}));
 assert.deepStrictEqual(run(expanded).states,compact.states,'compact history preserves explicit event order and outcomes');
 assert.equal(compact.outcome,'prospective-update-limit');assert.equal(compact.states.length,50);
 const missingCreation=clone(fixture.input);delete missingCreation.directActorCreates;
 const creationStop=run(missingCreation);assert.equal(creationStop.outcome,'actor-creation-input');assert(!creationStop.states.at(-1).actors.some(a=>a.slot===14));
 const missingProjection=clone(fixture.input);missingProjection.externalProducers.value.poseCalls=[];
 assert.equal(run(missingProjection).outcome,'shared-pose-control-18');
 const wrongState=clone(fixture.input);wrongState.directActorCreates.value[0].stateIndex++;
 assert.equal(run(wrongState).outcome,'actor-creation-state');
 const badAllocation=clone(fixture.input);badAllocation.directActorCreates.value[0].allocationAddress=badAllocation.capturedResume.value.primaryOwnerAddress;
 assert.throws(()=>run(badAllocation),e=>e.code==='resume-memory-input');
 const badIndex=clone(fixture.input);badIndex.externalProducers.value.events.sequence[0][1]=99999;
 assert.throws(()=>run(badIndex),e=>e.code==='launch-input');
 const badOrder=clone(fixture.input);badOrder.externalProducers.value.events.sequence[1][0]=0;
 assert.throws(()=>run(badOrder),e=>e.code==='launch-input');
 assert.equal(run(fixture.input,{maxTicks:10}).outcome,'tick-limit');
 assert.equal(run(fixture.input,{maxTicks:10}).states.length,10);
 assert.throws(()=>run(fixture.input,{controllerMask:0x8000}),e=>e.code==='launch-input');
 assert(result.retainedStateBytes<=result.limits.maxStateBytes);
 const asynchronous=await OB64.cutsceneRuntime.compileAsync(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:fixture.input,diagnosticAssumptions:false});
 assert.deepStrictEqual(asynchronous.states,result.states);
 const controller=new AbortController();controller.abort();
 await assert.rejects(OB64.cutsceneRuntime.compileAsync(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:fixture.input,signal:controller.signal}),e=>e.name==='AbortError');
 console.log(JSON.stringify({status:'pass',updates:fixture.expectedUpdates,milestoneActorComparisons:compared,actorFrames:frames,renderedFrames:rendered,effectFrames:effects,
  neutralHistoricalPayloads:fixture.originalDialogueSamples.length,neutralHistoricalRectangles:fixture.originalDialogueSamples.length,
  inputBytes:Buffer.byteLength(before),retainedStateBytes:result.retainedStateBytes,asyncParity:true,compactHistoryParity:true,
  renderingScope:'Actual Actor/effect renderer; backgrounds omitted; no original-game pixel claim'}));
})().catch(error=>{console.error(error.stack||error);process.exitCode=1;});
