'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert'),vm=require('vm');
const root=path.resolve(__dirname,'../..');
const bootstrap=fs.readFileSync(path.join(__dirname,'cutscene-framebuffer-integration.test.js'),'utf8').split('(async()=>')[0];
new Function('require','__dirname',bootstrap)(require,__dirname);
// Expose only the existing private edit-refresh entry inside this test process.
const uiSource=fs.readFileSync(path.join(root,'editor/cutscene-ui.js'),'utf8');
vm.runInThisContext(uiSource.replace('loadScene: loadScene,','loadScene: loadScene, testRefreshRuntime: refreshRuntime,'));
(async()=>{
 const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
 const z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}};
 const state=OB64.cutsceneUI.initialize(rom);
 const text=fs.readFileSync(path.join(root,'docs/reviews/cutscene-framebuffer-effects-20260916/playback-input.json'),'utf8');
 assert(Buffer.byteLength(text)<=131072);const input=JSON.parse(text),scene=state.catalog.getScene(input.assetId);state.selectedSceneId=scene.sceneId;
 const selected=OB64.cutsceneUI.launchContextChoice(state,scene,null);
 assert.equal(selected.id,'event-director:7:b00A4:invocation:52:context:0');
 assert.equal(selected.contextScene.assetId,'rom-director:01F4524C');
 const compile=OB64.cutsceneRuntime.compileAsync,calls=[],captures=[];let hold=false,entered,release;
 // Keep the real compile and capture services. Observe UI composition and suspend
 // one computed target to test replacement during asynchronous preparation.
 OB64.cutsceneRuntime={...OB64.cutsceneRuntime,compileAsync:async(document,program,selectedScene,catalog,options)=>{
   calls.push({assetId:selectedScene.assetId,contextRuntime:options.contextRuntime,launchContext:options.launchContext});
   const capture=options.captureFrame;
   return compile(document,program,selectedScene,catalog,{...options,captureFrame:async request=>{
     const target=await capture(request);captures.push({pass:request.pass,targetId:target.targetId,width:target.width,height:target.height});
     if(hold){entered();return new Promise(resolve=>release=()=>resolve(target));}return target;
   }});
 }};
 OB64.cutsceneUI.setNativeLaunchInputs(state,scene,input);
 const document=await OB64.cutsceneUI.loadScene(rom,state,scene),runtime=state.runtimeByAssetId[scene.assetId];
 assert(document);assert(runtime,JSON.stringify(state.sourceErrors));assert.equal(runtime.states.length,693);assert.equal(runtime.unresolvedQuery.code,'framebuffer-echo');assert.equal(runtime.framebuffers.length,1);
 assert.equal(calls.length,1);assert.equal(calls[0].contextRuntime,undefined);assert.equal(calls[0].launchContext,null);assert.equal(captures.length,1);assert.equal(captures[0].pass,641);
 assert(state.imageCache['archive:128'].result.renderable);assert.deepStrictEqual(state.sourceErrors,{});assert.equal(state.views[scene.sceneId].launchContextId,selected.id);
 // Edit refresh must not recover a stale, automatically derived Actor namespace.
 state.concurrentRuntimeByLaunchContext[scene.assetId+'|'+selected.id]={states:[{actors:[]}]};
 const refreshed=await OB64.cutsceneUI.testRefreshRuntime(state,scene,document,rom);
 assert.equal(refreshed.states.length,693);assert.equal(calls.at(-1).contextRuntime,undefined);assert.equal(calls.at(-1).launchContext,null);
 // A new import cancels pending preparation without publishing its later target.
 OB64.cutsceneUI.setNativeLaunchInputs(state,scene,input);hold=true;const waiting=new Promise(resolve=>entered=resolve);
 const pending=OB64.cutsceneUI.loadScene(rom,state,scene);await waiting;OB64.cutsceneUI.setNativeLaunchInputs(state,scene,input);
 assert.equal(await pending,null);assert.equal(state.runtimeByAssetId[scene.assetId],undefined);release();hold=false;await Promise.resolve();assert.equal(state.runtimeByAssetId[scene.assetId],undefined);
 await OB64.cutsceneUI.loadScene(rom,state,scene);assert.equal(state.runtimeByAssetId[scene.assetId].states.length,693);
 // Clearing the imported profile restores the saved event choice and owner chain.
 OB64.cutsceneUI.setNativeLaunchInputs(state,scene,null);await OB64.cutsceneUI.loadScene(rom,state,scene);
 const contextual=state.runtimeByAssetId[scene.assetId];assert.equal(contextual.concurrentContext.assetId,selected.contextScene.assetId);assert(calls.at(-1).contextRuntime);assert.equal(state.views[scene.sceneId].launchContextId,selected.id);
 const owner=calls.at(-1).contextRuntime;
 const incompatible=await compile(document,state.programByAssetId[scene.assetId],scene,state.catalog,{z64,nativeLaunchInputs:input,contextRuntime:owner,diagnosticAssumptions:false});
 assert.equal(incompatible.unresolvedQuery.code,'director-launch-input');assert.equal(incompatible.framebuffers.length,0);
 const explicit=structuredClone(input);explicit.existingActors={status:'known',value:{otherJobsEmpty:true,slots:Array(28).fill(null)}};OB64.cutsceneUI.setNativeLaunchInputs(state,scene,explicit);await OB64.cutsceneUI.loadScene(rom,state,scene);
 assert.equal(state.runtimeByAssetId[scene.assetId].unresolvedQuery.code,'director-launch-input');
 console.log(JSON.stringify({status:'pass',defaultInvocation:selected.id,retailStates:runtime.states.length,completedPasses:runtime.states.length-1,remaining:runtime.unresolvedQuery.code,capture:captures[0],image:'archive:128',inputBytes:Buffer.byteLength(text),editRefresh:true,pendingCaptureCancellation:true,contextRestored:contextual.concurrentContext.assetId,incompatibleInputsRejected:true,browserInteraction:false}));
})().catch(error=>{console.error(error.stack||error);process.exitCode=1;});
