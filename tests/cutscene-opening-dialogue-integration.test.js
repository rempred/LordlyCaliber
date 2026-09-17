'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert'),root=path.resolve(__dirname,'../..');
new Function('require','__dirname',fs.readFileSync(path.join(__dirname,'cutscene-room-integration.test.js'),'utf8').split('\n(async()=>')[0])(require,__dirname);
(async()=>{
 const v=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(v.length);for(let i=0;i<v.length;i+=2){z64[i]=v[i+1];z64[i+1]=v[i];}
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}},ui=OB64.cutsceneUI,runtime=OB64.cutsceneRuntime,state=ui.initialize(rom),input=structuredClone(require('./fixtures/cutscene-room-input.json')),before=JSON.stringify(input),scene=state.catalog.getScene(input.assetId);
 state.selectedSceneId=scene.sceneId;ui.setNativeLaunchInputs(state,scene,input);await ui.loadScene(rom,state,scene);
 const run=state.runtimeByAssetId[scene.assetId],document=ui.selectedDocument(state);
 assert.equal(run.states.length,3392);assert.equal(run.outcome,'modeled-termination');assert.equal(run.unresolvedQuery,null);assert.equal(JSON.stringify(input),before);assert.equal(run.retainedStateBytes,117173782);assert.equal(run.trace.find(t=>t.kind==='map-menu-memory').bytes,130578);
 function element(){return {className:'',style:{},children:[],classList:{toggle(){}},set innerHTML(v){this.children=[];},appendChild(c){this.children.push(c);},setAttribute(){}};}
 global.document={createElement:element};let painted;
 state.ui={stageOverlay:element(),canvas:{style:{},getContext(){return {createImageData(w,h){return {data:new Uint8ClampedArray(w*h*4)};},putImageData(i){painted=i.data.slice();}};}}};
 const images={},rows=[];let peak=0;
 for(const pass of [105,106,120,124,128,132,150,200,120,106]){
  const preview=runtime.evaluate(run,pass);state.views[scene.sceneId]={frame:pass,pathId:'default',selectedActorId:null};
  const capture=await ui.capturePreviewFrame(rom,state,document,{preview,pass,nodeId:'opening-dialogue',backgroundPolicy:'require'});
  ui.paintStageForTest(rom,state);assert(Buffer.from(capture.rgba).equals(Buffer.from(painted)),'preview/capture equality '+pass);assert.equal(state.composedDialogueIds.length,1);
  assert(!state.ui.stageOverlay.children.some(c=>c.className.includes('cutscene-preview-dialogue')),'ordinary opening must not use an HTML dialogue fallback');
  if(images[pass])assert(Buffer.from(capture.rgba).equals(images[pass]),'seek equality');else images[pass]=Buffer.from(capture.rgba);
  const composition=ui.composeDialogue(rom,state,preview,true),mask={rgba:new Uint8Array(307200)};composition.rows.forEach(c=>{OB64.cutsceneDialogueDraw.paint(mask,c);peak=Math.max(peak,c.temporaryTextureBytes);});
  const background=await ui.capturePreviewFrame(rom,state,document,{preview:{...preview,dialogue:[]},pass,nodeId:'opening-background-control',backgroundPolicy:'require'});
  let changed=0;for(let i=0;i<307200;i+=4)for(let c=0;c<4;c++){if(!mask.rgba[i+3])assert.equal(capture.rgba[i+c],background.rgba[i+c]);else if(capture.rgba[i+c]!==background.rgba[i+c])changed++;}assert(changed>0);
  if(process.env.OB64_OPENING_OUTPUT)fs.writeFileSync(path.join(process.env.OB64_OPENING_OUTPUT,'opening-'+pass+'.rgba'),capture.rgba);
  rows.push({pass,text:preview.dialogue[0].payload.text,actors:preview.actors.length,backgrounds:preview.background.layers.length,previewCaptureEqual:true,unchangedOutsideDialogue:true});
 }
 const payload=runtime.evaluate(run,106).dialogue[0].payload.nativeDialogue.drawingState.payloadHex;
 const copy=runtime.evaluate(run,106);copy.dialogue[0].payload.nativeDialogue.drawingState.payloadHex='bad';assert.equal(runtime.evaluate(run,106).dialogue[0].payload.nativeDialogue.drawingState.payloadHex,payload);
 let next=null,checked=0;for(let pass=106;pass<run.states.length;pass++){const s=run.states[pass];if(!s.dialogue.length)continue;try{ui.composeDialogue(rom,state,s,true);checked++;}catch(e){const d=s.dialogue[0],p=Buffer.from(d.payload.nativeDialogue.drawingState.payloadHex,'hex');next={pass,error:e.message,text:d.payload.text,selectors:Object.fromEntries([0x3d,0x42,0x47,0x50,0x3a,0x3b].map(o=>['0x'+o.toString(16),p[o]]))};break;}}
 const oldDrawer=state.dialogueDrawer,cache=oldDrawer.assetBytes;ui.resetAll(state);assert.equal(state.dialogueDrawer,null);assert.equal(state.imageCacheBytes,0);
 ui.composeDialogue(rom,state,runtime.evaluate(run,120),true);assert.notStrictEqual(state.dialogueDrawer,oldDrawer);assert.equal(state.dialogueDrawer.assetBytes,2874);
 const result={status:'pass',states:run.states.length,outcome:run.outcome,retainedStateBytes:run.retainedStateBytes,nativeServiceBytes:130578,artworkCacheBytes:cache,peakTemporaryTextureBytes:peak,rows,checkedDialogueStates:checked,nextBoundary:next,seek:true,independentEvaluation:true,reset:true};
 if(process.env.OB64_OPENING_OUTPUT)fs.writeFileSync(path.join(process.env.OB64_OPENING_OUTPUT,'integration-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
