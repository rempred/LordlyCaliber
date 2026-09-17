'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),root=path.resolve(__dirname,'../..');
new Function('require','__dirname',fs.readFileSync(path.join(__dirname,'cutscene-shared-actor-integration.test.js'),'utf8').split('\n(async()=>')[0])(require,__dirname);
vm.runInThisContext(fs.readFileSync(path.join(root,'editor/cutscene-njpg.js'),'utf8'));
// Exercise the real private painter with a canvas sink, without changing product exports.
vm.runInThisContext(fs.readFileSync(path.join(root,'editor/cutscene-ui.js'),'utf8').replace('OB64.cutsceneUI = {','OB64.cutsceneUI = { paintStageForTest:paintStage,'));
(async()=>{
 const v=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(v.length);for(let i=0;i<v.length;i+=2){z64[i]=v[i+1];z64[i+1]=v[i];}
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}},ui=OB64.cutsceneUI,runtime=OB64.cutsceneRuntime,state=ui.initialize(rom),input=structuredClone(require('./fixtures/cutscene-room-input.json')),before=JSON.stringify(input),scene=state.catalog.getScene(input.assetId);
 state.selectedSceneId=scene.sceneId;ui.setNativeLaunchInputs(state,scene,input);await ui.loadScene(rom,state,scene);
 const run=state.runtimeByAssetId[scene.assetId],source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,state.catalog);
 assert.equal(run.outcome,'modeled-termination',JSON.stringify(run.unresolvedQuery));assert.equal(run.states.length,3392);assert.equal(run.unresolvedQuery,null);assert.equal(JSON.stringify(input),before);assert(run.retainedStateBytes<128*1024*1024);
 assert(run.framebuffers.length>0);assert(run.trace.find(t=>t.kind==='map-menu-memory').bytes<=131072);
 console.log('Default route reached modeled termination.');
 function same(a,b,label){assert(a&&b&&Buffer.from(a).equals(Buffer.from(b)),label);}
 function element(){return {className:'',style:{},children:[],classList:{toggle(){}},set innerHTML(v){this.children=[];},appendChild(c){this.children.push(c);},setAttribute(){}};}
 global.document={createElement:element};let painted;
 state.ui={stageOverlay:element(),canvas:{style:{},getContext(){return {createImageData(w,h){return {data:new Uint8ClampedArray(w*h*4)};},putImageData(i){painted=i.data.slice();}};}}};
 const images={},rows=[];let unsupportedTextStates=0;
 const textPass=run.states.findIndex(s=>{
  if(s.background.layers.length!==6||!s.dialogue.some(d=>d.payload.text&&d.payload.text.length>=30&&d.payload.nativeDialogue&&d.payload.nativeDialogue.drawingState))return false;
  try{return ui.composeDialogue(rom,state,s,true).rows.length>0;}catch(e){unsupportedTextStates++;return false;}
 });
 assert(textPass>=0,'the route must include supported ordinary dialogue over the room');
 for(const pass of [textPass,600,2306,2948,textPass,600,2306]){
  const preview=runtime.evaluate(run,pass);state.views[scene.sceneId]={frame:pass,pathId:'default',selectedActorId:null};
  const image=await ui.capturePreviewFrame(rom,state,p.document,{preview,pass,nodeId:'room-seek',backgroundPolicy:'require'});
  ui.paintStageForTest(rom,state);same(painted,image.rgba,'preview/capture pixels at '+pass);
  if(preview.audio.some(row=>!row.payload))assert(state.ui.stageOverlay.children.some(row=>String(row.textContent).includes('Sound request')),'native request cue must paint without a timeline payload');
  if(images[pass])same(image.rgba,images[pass],'seek pixels at '+pass);else images[pass]=image.rgba.slice();
  assert.deepStrictEqual(preview.background.layers.map(l=>l.nativeOrdinal),[0,1,3,4,5,6]);
  const omitted=await ui.capturePreviewFrame(rom,state,p.document,{preview,pass,nodeId:'room-omission-control',backgroundPolicy:'omit'});
  assert(!Buffer.from(image.rgba).equals(Buffer.from(omitted.rgba)),'background omission must change pixels');
  if(process.env.OB64_ROOM_OUTPUT){fs.writeFileSync(path.join(process.env.OB64_ROOM_OUTPUT,'room-'+pass+'.rgba'),image.rgba);fs.writeFileSync(path.join(process.env.OB64_ROOM_OUTPUT,'omitted-'+pass+'.rgba'),omitted.rgba);}
  rows.push({pass,previewCaptureEqual:true,backgrounds:preview.background.layers.length,actors:preview.actors.length,dialogue:preview.dialogue.length});
 }
 // Current transform mutation changes pixels, without changing retained state or cache artwork.
 const changed=runtime.evaluate(run,600),original=runtime.evaluate(run,600);changed.background.layers[1].sceneTransform.uniformScale=.75;
 const changedImage=await ui.capturePreviewFrame(rom,state,p.document,{preview:changed,pass:600,nodeId:'room-transform-control',backgroundPolicy:'require'});
 assert(!Buffer.from(changedImage.rgba).equals(Buffer.from(images[600])));assert.deepStrictEqual(runtime.evaluate(run,600).background,original.background);
 if(process.env.OB64_ROOM_OUTPUT)fs.writeFileSync(path.join(process.env.OB64_ROOM_OUTPUT,'changed-600.rgba'),changedImage.rgba);
 const options={z64,nativeLaunchInputs:input,diagnosticAssumptions:false,maxTicks:3500,captureFrame:request=>ui.capturePreviewFrame(rom,state,p.document,request)};
 const again=await runtime.compileAsync(p.document,p.program,scene,state.catalog,options);
 assert.equal(again.states.length,run.states.length);assert.equal(again.outcome,run.outcome);assert.equal(again.retainedStateBytes,run.retainedStateBytes);
 assert.equal(again.framebuffers.length,run.framebuffers.length);
 again.framebuffers.forEach((f,i)=>{same(f.rgba,run.framebuffers[i].rgba,'recompile framebuffer');assert.notStrictEqual(f.rgba,run.framebuffers[i].rgba);});
 const controller=new AbortController();await assert.rejects(runtime.compileAsync(p.document,p.program,scene,state.catalog,{...options,signal:controller.signal,captureFrame:async request=>{const image=await ui.capturePreviewFrame(rom,state,p.document,request);controller.abort();return image;}}),e=>e.name==='AbortError');
 const result={status:'pass',states:run.states.length,completedPasses:run.trace.filter(t=>t.kind==='resource-pass').length,outcome:run.outcome,terminalReason:run.states.at(-1).runtime.terminalReason,retainedStateBytes:run.retainedStateBytes,nativeMemoryBytes:run.trace.find(t=>t.kind==='map-menu-memory').bytes,imageCacheBytes:state.imageCacheBytes,imageCacheLimit:ui.imageCacheLimit,framebuffers:run.framebuffers.map(f=>({pass:f.pass,id:f.id,bytes:f.rgba.byteLength})),textPass,unsupportedTextStates,rows,recompileCaptureEquality:true,captureCancellation:true};
 assert(state.imageCacheBytes<=ui.imageCacheLimit);ui.resetAll(state);assert.equal(state.imageCacheBytes,0);assert.deepStrictEqual(Object.keys(state.imageCache),[]);
 if(process.env.OB64_ROOM_OUTPUT)fs.writeFileSync(path.join(process.env.OB64_ROOM_OUTPUT,'integration-result.json'),JSON.stringify(result,null,2)+'\n');
 console.log(JSON.stringify(result));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
