"use strict";
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),crypto=require('crypto'),R=path.resolve(__dirname,'../..');
new Function('require','__dirname',fs.readFileSync(path.join(__dirname,'cutscene-shared-actor-integration.test.js'),'utf8').split('\n(async()=>')[0])(require,__dirname);
for(const file of ['cutscene-rom-start-data.js','cutscene-rom-start.js','cutscene-njpg.js','cutscene-dialogue-draw.js'])vm.runInThisContext(fs.readFileSync(path.join(R,'editor',file),'utf8'),{filename:file});
(async()=>{
 const v=fs.readFileSync(path.join(R,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(v.length);for(let i=0;i<v.length;i+=2){z64[i]=v[i+1];z64[i+1]=v[i];}
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}},ui=OB64.cutsceneUI.initialize(rom),scene=ui.catalog.getScene('rom-custom-lz:01F3EAD2');
 assert.equal(Object.keys(ui.nativeLaunchInputsByAssetId||{}).length,0);assert.deepStrictEqual(ui.runtimeByAssetId,{});
 const start=performance.now();await OB64.cutsceneUI.loadScene(rom,ui,scene);const elapsedMs=performance.now()-start,r=ui.runtimeByAssetId[scene.assetId];assert(r,JSON.stringify(ui.sourceErrors));
 assert.equal(r.outcome,'modeled-termination');assert.equal(r.states.length,1365);assert.deepStrictEqual(r.missingInputs,[]);assert.equal(r.states.at(-1).runtime.terminalReason,'terminal-state-release');assert.equal(r.states[0].background.layers[0].source.sourceKind,'ROM-launch-prescan');assert(r.states.every(s=>s.actors.length===8));assert(r.retainedStateBytes<128*1024*1024);assert.equal(Object.keys(ui.nativeLaunchInputsByAssetId||{}).length,0);assert(OB64.cutsceneUI.playbackTimingLabel(ui,scene).includes('simulated timing'));
 assert(!r.trace.some(t=>t.kind==='captured-resume-entry'||t.label==='Observed background route'));
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,ui.catalog),input=OB64.cutsceneRomStart.input(z64,scene,p.program);
 assert(!('capturedSnapshot'in input));assert(!('capturedResume'in input));assert(!('capturedPresentation'in input));assert.equal(input.externalProducers.value.directorLaunch.selector,2);assert.equal(input.externalProducers.value.directorLaunch.environmentSelector,57);
 assert.equal(OB64.cutsceneRomStart.supports({...scene,assetId:'another-scene'},p.program),true);
 // Exclude all observation-derived launch geometry and predecessor choices.
 const clean=structuredClone(scene);delete clean.actorCameraObservation;delete clean.backgroundRuntimeObservation;
 clean.launchProfile.directorMode={value:null,status:'Excluded test observation',source:'test',evidenceStatus:'external-unresolved'};
 for(const bank of ['actor','registered'])clean.launchProfile.cameras[bank]={kind:'external-unresolved',projection:null,status:'Excluded test observation',evidenceStatus:'external-unresolved'};
 clean.launchProfile.background.requests=[];clean.launchProfile.parentEventLaunches=[];
 const fresh=OB64.cutsceneUI.initialize(rom);await OB64.cutsceneUI.loadScene(rom,fresh,clean);const cleanRun=fresh.runtimeByAssetId[clean.assetId];assert(cleanRun,JSON.stringify(fresh.sourceErrors));assert.equal(cleanRun.outcome,r.outcome,JSON.stringify(cleanRun.missingInputs));assert.equal(cleanRun.states.length,r.states.length);
 const facts=s=>({actors:s.actors.map(a=>[a.slot,a.baseX,a.baseY,a.baseZ,a.bank,a.animationKey,a.displayedFrameToken,a.nativeActorState]),camera:s.cameraState,projection:s.actorProjection,background:s.background.layers.map(l=>l.assetId),dialogue:s.dialogue.map(d=>d.payload.nativeDialogue.drawingState)});
 for(const tick of [0,80,330,550,617,700,789,1050,1364])assert.deepStrictEqual(facts(cleanRun.states[tick]),facts(r.states[tick]),'observation-free pass '+tick);
 // Compare the actual ordinary composed scene after excluding observations.
 const cleanSource=await OB64.cutsceneCodec.loadSceneSource(z64,clean),cleanProjected=OB64.cutsceneCodec.projectSceneDocument(clean,cleanSource,fresh.catalog);
 async function compose(state,document,run,tick){
  const preview=OB64.cutsceneRuntime.evaluate(run,tick),projection=preview.background.projection,backgrounds=[];
  assert(!projection.calibrationCamera);assert.equal(preview.actorProjection.evidenceStatus,'computed-native-initializer');
  for(const layer of preview.background.layers){const asset=state.catalog.getImageAsset(layer.assetId),image=await OB64.cutsceneUI.decodeImageAsset(rom,state,asset);assert(image.renderable);backgrounds.push({layer,image});}
  const actorFrames=OB64.cutsceneSprites.framesForPreview(state.spriteState,preview),scenePropFrames=OB64.cutsceneSprites.framesForStageProps(state.spriteState,projection,preview.frame);assert.equal(scenePropFrames.length,12);
  const target=OB64.cutsceneRenderer.renderFrame(document,preview,{showMovementPaths:false,backgrounds,backgroundProjection:projection,actorFrames,scenePropFrames,effectFrames:[],projection:preview.actorProjection,camera:preview.cameraState,overlays:preview.overlays,colorModulation:preview.sceneColor,screenTransition:preview.screenTransition});
  OB64.cutsceneUI.composeDialogue(rom,state,preview,true).rows.forEach(row=>OB64.cutsceneDialogueDraw.paint(target,row));return target.rgba;
 }
 for(const tick of [80,330,760,1050,1260,1364])assert.deepStrictEqual(await compose(fresh,cleanProjected.document,cleanRun,tick),await compose(ui,p.document,r,tick),'observation-free composed pass '+tick);
 // Re-encode changed ROM bytes and qualify only their changed storage metadata.
 const altered=z64.slice(),decoded=source.decodedBytes.slice(),dv=new DataView(decoded.buffer),place=p.program.primitives.find(n=>n.opcode===0x14&&n.rawWords[1]===0);assert(place);dv.setInt32((place.startWord+5)*4,5000);
 const encoded=OB64.cutsceneCodec.encodeCustomLzOptimal(decoded),changedScene=structuredClone(clean);assert(encoded.length<=scene.source.storedPayloadLength);
 new DataView(altered.buffer).setUint32(scene.source.z64PrefixStart,encoded.length);altered.set(encoded,scene.source.z64PayloadStart);changedScene.source.storedPayloadLength=encoded.length;changedScene.source.z64PayloadEndExclusive=scene.source.z64PayloadStart+encoded.length;changedScene.source.decodedSha256=crypto.createHash('sha256').update(decoded).digest('hex').toUpperCase();
 const changedSource=await OB64.cutsceneCodec.loadSceneSource(altered,changedScene),changed=OB64.cutsceneCodec.projectSceneDocument(changedScene,changedSource,ui.catalog),changedInput=OB64.cutsceneRomStart.input(altered,changedScene,changed.program),mutation=OB64.cutsceneRuntime.compile(changed.document,changed.program,changedScene,ui.catalog,{z64:altered,nativeLaunchInputs:changedInput,maxTicks:2});
 assert.equal(mutation.states[0].actors.find(a=>a.slot===0).baseX,5);assert.equal(r.states[0].actors.find(a=>a.slot===0).baseX,-40);
 const neutral=structuredClone(input);neutral.externalProducers.value.resourceSchedule.pageAdvancePolicy='neutral';const wait=OB64.cutsceneRuntime.compile(p.document,p.program,scene,ui.catalog,{z64,nativeLaunchInputs:neutral,maxTicks:2000});assert.equal(wait.outcome,'awaiting-dialogue-input');assert.equal(wait.unresolvedQuery.code,'dialogue-page-acknowledgement');
 const cancel=new AbortController(),service=OB64.cutsceneDialogue.Engine.prototype.service;let entered=false;
 OB64.cutsceneDialogue.Engine.prototype.service=function*(event,...args){if(event.service==='callback'&&this.owners.some(o=>o&&o.payload&&o.payload.length===1144)){entered=true;cancel.abort();}return yield* service.call(this,event,...args);};
 try{await assert.rejects(OB64.cutsceneRuntime.compileAsync(p.document,p.program,scene,ui.catalog,{z64,nativeLaunchInputs:input,signal:cancel.signal}),e=>e.name==='AbortError');}finally{OB64.cutsceneDialogue.Engine.prototype.service=service;}assert(entered);
 const before=OB64.cutsceneRuntime.evaluate(r,700);before.actors[0].x=9999;assert.notEqual(OB64.cutsceneRuntime.evaluate(r,700).actors[0].x,9999);
 console.log(JSON.stringify({status:'pass',freshPublicPasses:r.states.length,elapsedMs,retainedBytes:r.retainedStateBytes,observationFree:true,composedObservationFreePasses:[80,330,760,1050,1260,1364],romMutation:{slot:0,before:-40,after:5},neutral:{passes:wait.states.length,outcome:wait.outcome},cancelledDuringNativeCallback:true,importedInputs:Object.keys(ui.nativeLaunchInputsByAssetId||{}).length}));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
