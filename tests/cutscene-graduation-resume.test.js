'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),R=path.resolve(__dirname,'../..');
new Function('require','__dirname',fs.readFileSync(path.join(__dirname,'cutscene-shared-actor-integration.test.js'),'utf8').split('\n(async()=>')[0])(require,__dirname);
for(const file of ['cutscene-njpg.js','cutscene-captured-scheduler-data.js','cutscene-captured-scheduler.js'])vm.runInThisContext(fs.readFileSync(path.join(R,'editor',file),'utf8'),{filename:file});
const fixture=require('./fixtures/cutscene-graduation-resume.json'),observed=require('./fixtures/cutscene-graduation-native.json'),clone=x=>JSON.parse(JSON.stringify(x));
(async()=>{
 const raw=fs.readFileSync(path.join(R,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}},ui=OB64.cutsceneUI.initialize(rom),scene=ui.catalog.getScene(fixture.input.assetId),source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,ui.catalog);
 const originalScheduler=OB64.cutsceneCapturedScheduler,instances=[];
 OB64.cutsceneCapturedScheduler=function(...args){const value=new originalScheduler(...args);instances.push(value);return value;};
 const run=(input=fixture.input,options={})=>OB64.cutsceneRuntime.compile(p.document,p.program,scene,ui.catalog,{z64,nativeLaunchInputs:input,diagnosticAssumptions:false,...options});
 const inputBefore=JSON.stringify(fixture.input),result=run(),service=instances.at(-1);
 assert(Buffer.byteLength(inputBefore)<131072);assert.equal(JSON.stringify(fixture.input),inputBefore);
 assert.equal(result.outcome,'prospective-update-limit');assert.equal(result.states.length,69);assert.equal(result.capturedSnapshot.executedUpdates,69);assert.equal(result.unresolvedQuery,null);assert.deepStrictEqual(result.missingInputs,[]);assert.deepStrictEqual(result.assumptions,[]);
 let actorChecks=0,memoryChecks=0;
 for(const expected of fixture.expected){const state=result.states[expected.update-1];assert.equal(state.actors.length,8);for(const row of expected.actors){const actual=state.actors.find(a=>a.slot===row.slot);assert.equal(actual.source.recordHex,row.recordHex,`complete Actor record ${expected.update}/${row.slot}`);actorChecks++;}}
 for(const row of fixture.expectedMemory){const actual=Buffer.from(Array.from({length:row.hex.length/2},(_,i)=>service.machine.get(row.address+i,1)));assert.equal(actual.toString('hex'),row.hex,`auxiliary memory ${row.address.toString(16)}`);memoryChecks+=actual.length;}
 const bits=n=>{const b=Buffer.alloc(4);b.writeFloatBE(n);return b.toString('hex').toUpperCase();};
 for(const sample of observed.samples){const actor=result.states[sample.eligibleUpdate-1].actors.find(a=>a.slot===1),native=OB64.cutsceneRuntime.decodeNativeActorState(actor.nativeActorState);assert.equal(bits(actor.baseX),sample.xBits);assert.deepStrictEqual([native.poseCursor,native.poseDelay,actor.displayedFrameToken],sample.pose);}
 const one=clone(fixture.input);one.capturedResume.value.updates=1;assert.deepStrictEqual(run(one).states[0].actors,result.states[0].actors);
 const again=run();assert.deepStrictEqual(again.states,result.states);assert.notStrictEqual(again.states[0].actors,result.states[0].actors);
 for(const tick of [0,34,68,34,0])assert.deepStrictEqual(OB64.cutsceneRuntime.evaluate(result,tick).actors,result.states[tick].actors);
 const detached=OB64.cutsceneRuntime.evaluate(result,34);detached.actors[0].x=9999;assert.notEqual(OB64.cutsceneRuntime.evaluate(result,34).actors[0].x,9999);
 const asynchronous=await OB64.cutsceneRuntime.compileAsync(p.document,p.program,scene,ui.catalog,{z64,nativeLaunchInputs:fixture.input,diagnosticAssumptions:false});assert.deepStrictEqual(asynchronous.states,result.states);
 const cancel=new AbortController(),advance=originalScheduler.prototype.advance;let entered=false;
 originalScheduler.prototype.advance=function*(callbacks){entered=true;cancel.abort();return yield* advance.call(this,callbacks);};
 try{await assert.rejects(OB64.cutsceneRuntime.compileAsync(p.document,p.program,scene,ui.catalog,{z64,nativeLaunchInputs:fixture.input,signal:cancel.signal}),e=>e.name==='AbortError');}finally{originalScheduler.prototype.advance=advance;}assert(entered);
 const badMode=clone(fixture.input);badMode.capturedSnapshot.value.sceneMode=0;assert.throws(()=>run(badMode));
 const badLimit=clone(fixture.input);badLimit.capturedResume.value.updates=124;assert.throws(()=>run(badLimit),e=>e.code==='resume-transition-input');
 const badBranch=clone(fixture.input);badBranch.schedulerBranch.value='alternate';assert.throws(()=>run(badBranch),e=>e.code==='resume-input');
 const missing=clone(fixture.input);delete missing.poseRegistry;assert.notEqual(run(missing).outcome,'prospective-update-limit');
 const mismatch=clone(fixture.input);const b=Buffer.from(mismatch.capturedSnapshot.value.slots[1].recordHex,'hex');b.writeFloatBE(12,0x11c);mismatch.capturedSnapshot.value.slots[1].recordHex=b.toString('hex');assert.throws(()=>run(mismatch),e=>e.code==='resume-native-record');
 const absent=clone(fixture.input);absent.capturedResume.value.nativeScheduler.memory=absent.capturedResume.value.nativeScheduler.memory.filter(r=>!(r.address<=0x801a1150&&r.address+r.hex.length/2>0x801a1150));assert.notEqual(run(absent).outcome,'prospective-update-limit');
 const badCode=new Uint8Array(z64);badCode[0x2a0680]^=1;assert.throws(()=>run(fixture.input,{z64:badCode}),e=>e.code==='resume-native-image');
 const nativeByte=(input,address,value)=>{const row=input.capturedResume.value.nativeScheduler.memory.find(r=>r.address<=address&&address<r.address+r.hex.length/2);assert(row);const bytes=Buffer.from(row.hex,'hex');bytes[address-row.address]=value;row.hex=bytes.toString('hex');};
 const proximity=clone(fixture.input);nativeByte(proximity,0x80197b17,9);assert.equal(run(proximity).unresolvedQuery.code,'resume-movement-helper');
 const terrain=clone(fixture.input),terrainRecord=Buffer.from(terrain.capturedSnapshot.value.slots[1].recordHex,'hex');terrainRecord[0x145]|=1;terrain.capturedSnapshot.value.slots[1].recordHex=terrainRecord.toString('hex');
 const ownerMemory=terrain.capturedResume.value.nativeScheduler.memory.find(r=>r.address<=terrain.capturedResume.value.primaryOwnerAddress+28&&r.address+r.hex.length/2>=terrain.capturedResume.value.primaryOwnerAddress+32),actorAddress=Buffer.from(ownerMemory.hex,'hex').readUInt32BE(terrain.capturedResume.value.primaryOwnerAddress+28-ownerMemory.address);nativeByte(terrain,actorAddress+0x145,terrainRecord[0x145]);assert.equal(run(terrain).unresolvedQuery.code,'resume-movement-helper');
 const unsupportedPose=clone(fixture.input);unsupportedPose.poseRegistry.value.alternate[0].programs[0].programHex='02011a080500';assert.equal(run(unsupportedPose).unresolvedQuery.code,'resume-pose-input');
 const changedProgram=clone(fixture.input);changedProgram.poseRegistry.value.alternate[0].programs[0].programHex='02011a080400';const changed=run(changedProgram);assert.equal(changed.states[3].actors.find(a=>a.slot===6).displayedFrameToken,26,'shared decoder consumes supplied program, never future state answers');
 OB64.cutsceneUI.setNativeLaunchInputs(ui,scene,fixture.input);await OB64.cutsceneUI.loadScene(rom,ui,scene);const loaded=ui.runtimeByAssetId[scene.assetId];assert.deepStrictEqual(loaded.states,result.states);
 const images={};let props=0;
 for(const tick of [0,34,68,34,0]){
  const preview=OB64.cutsceneRuntime.evaluate(loaded,tick),backgrounds=[];
  for(const layer of preview.background.layers){const asset=ui.catalog.getImageAsset(layer.assetId);if(asset){const image=await OB64.cutsceneUI.decodeImageAsset(rom,ui,asset);assert(image.renderable);backgrounds.push({layer,image});}}
  const actorFrames=OB64.cutsceneSprites.framesForPreview(ui.spriteState,preview),scenePropFrames=OB64.cutsceneSprites.framesForStageProps(ui.spriteState,preview.background.projection,preview.frame);assert.equal(Object.keys(actorFrames).length,8);props=scenePropFrames.length;
  // Same shared composition arguments as ordinary paintStage; strict framebuffer
  // capture retains its independent scene-prop ordering guard.
  const image=OB64.cutsceneRenderer.renderFrame(p.document,preview,{showMovementPaths:false,backgrounds,backgroundProjection:preview.background.projection,projection:preview.actorProjection,actorFrames,scenePropFrames,effectFrames:[],camera:preview.cameraState,overlays:preview.overlays,colorModulation:preview.sceneColor,screenTransition:preview.screenTransition,sceneVignette:preview.sceneVignette,oversizedImageView:preview.oversizedImageView});
  if(images[tick])assert.deepStrictEqual(image.rgba,images[tick]);else images[tick]=image.rgba;
 }
 assert.notDeepStrictEqual(images[0],images[68]);assert.equal(props,12);assert.deepStrictEqual(ui.spriteState.errors,{});assert(service.memoryBytes<=131072);assert(result.retainedStateBytes<=128*1024*1024);assert(ui.imageCacheBytes<=OB64.cutsceneUI.imageCacheLimit);
 console.log(JSON.stringify({status:'pass',eligibleUpdates:69,fullActorRecordComparisons:actorChecks,auxiliaryMemoryByteComparisons:memoryChecks,acceptedHistoricalSamples:observed.samples.length,inputBytes:Buffer.byteLength(inputBefore),serviceBytes:service.memoryBytes,retainedStateBytes:result.retainedStateBytes,images:3,sceneProps:props,publicImportLoad:true,seekRepeatAsyncParity:true,cancellation:true,scope:'Prospective normal scheduler calls; ordinary composed preview; no strict framebuffer or historical frame cadence claim'}));
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
