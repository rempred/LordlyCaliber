'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-chair-camera.json');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
global.window=global;vm.runInThisContext('var OB64=window.OB64={}');
const hashes={};
for(const f of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-dialogue-data.js','cutscene-dialogue.js','cutscene-runtime.js','cutscene-preview.js','cutscene-assets.js','cutscene-sprites.js','cutscene-renderer.js']){
 const b=fs.readFileSync(f==='cutscene-runtime.js'&&process.argv[2]?process.argv[2]:path.join(root,'editor',f));hashes[f]=hash(b);vm.runInThisContext(b.toString(),{filename:f});
}
(async()=>{
 const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.getScene(fixture.input.assetId),source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
 const compile=input=>OB64.cutsceneRuntime.compile(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:input,diagnosticAssumptions:false});
 const clone=x=>JSON.parse(JSON.stringify(x)),before=JSON.stringify(fixture.input),rows=[];
 const spriteState=OB64.cutsceneSprites.create(z64,catalog);
 for(let index=0;index<2;index++){
  const input=clone(fixture.input);if(index===0)delete input.capturedResume;
  const rt=compile(input),state=rt.states[0];assert.equal(rt.capturedSnapshot.executedUpdates,index);
  assert.equal(rt.outcome,index?'qualified-resume-update-complete':'captured-snapshot-resume-input');
  const frames=OB64.cutsceneSprites.framesForPreview(spriteState,state);assert.equal(Object.keys(frames).length,9);
  for(const a of state.actors){
   const expected=fixture.expected[index].actors.find(x=>x.slot===a.slot),g=OB64.cutsceneRenderer.modeZeroActorGeometry(a,state,state.actorProjection);
   assert(g);const anchor=[g.registeredScreenPoint.x,g.registeredScreenPoint.y],point=[g.scenePoint.x,g.scenePoint.y,g.scenePoint.z];
   const errors={anchor:anchor.map((v,i)=>v-expected.anchor[i]),translation:point.map((v,i)=>v-expected.mainMatrix[12+i]),scale:[g.actorPlaneScale-expected.mainMatrix[0],g.actorPlaneScale-expected.mainMatrix[5]]};
   assert(errors.anchor.every(x=>Math.abs(x)<=fixture.contract.anchorAbsoluteTolerancePixels),'registered anchor '+a.slot+': '+JSON.stringify(errors));
   assert(errors.translation.every(x=>Math.abs(x)<=fixture.contract.matrixTranslationAbsoluteTolerance),'main translation '+a.slot);
   assert(errors.scale.every(x=>Math.abs(x)<=fixture.contract.matrixScaleAbsoluteTolerance),'main scale '+a.slot+': '+JSON.stringify(errors));
   assert.equal(a.nativeUniformScale,expected.scale[0]);rows.push({sample:index,slot:a.slot,anchor,scenePoint:point,actorPlaneScale:g.actorPlaneScale,screenPoint:g.screenPoint,errors});
  }
  const rendered=OB64.cutsceneRenderer.renderFrame(p.document,state,{actorFrames:frames,projection:state.actorProjection,backgrounds:[],overlays:[]});
  assert.equal(rendered.rgba.length,320*240*4);assert.equal(rendered.hitRegions.length,9);
  const again=OB64.cutsceneRenderer.renderFrame(p.document,state,{actorFrames:frames,projection:state.actorProjection,backgrounds:[],overlays:[]});assert.equal(hash(rendered.rgba),hash(again.rgba));
  const asyncResult=await OB64.cutsceneRuntime.compileAsync(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:input,diagnosticAssumptions:false});assert.deepStrictEqual(asyncResult.states,rt.states);
  assert(rt.retainedStateBytes<=rt.limits.maxStateBytes);assert.equal(rt.states.length,1);
 }
 const invalid=[i=>i.capturedPresentation.value.actorCamera.values.pop(),i=>i.capturedPresentation.value.channels[0]=null,i=>i.capturedPresentation.value.channels[0].rotationX=1,i=>i.capturedPresentation.value.registeredCamera.modelScale=0,i=>i.capturedPresentation.value.actorCamera.values[0]=NaN,i=>delete i.capturedSnapshot,i=>i.capturedPresentation.value.actorCamera.values.splice(8,3,...i.capturedPresentation.value.actorCamera.values.slice(5,8))];
 for(const change of invalid){const input=clone(fixture.input);change(input);assert.throws(()=>compile(input),e=>e.code==='captured-presentation-input');}
 const unknown=clone(fixture.input);unknown.capturedPresentation={status:'unknown'};const u=compile(unknown);assert.equal(u.outcome,'qualified-resume-update-complete');assert(u.missingInputs.some(x=>x.includes('camera and scene transforms are unknown')));
 const absent=clone(fixture.input);delete absent.capturedPresentation;assert.deepStrictEqual(compile(absent).states,u.states);
 const cancelled=new AbortController();cancelled.abort();await assert.rejects(OB64.cutsceneRuntime.compileAsync(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:fixture.input,signal:cancelled.signal}),e=>e.name==='AbortError');
 assert.equal(JSON.stringify(fixture.input),before);
 console.log(JSON.stringify({status:'pass',hashes,fixtureSha256:hash(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-chair-camera.json'))),rows,invalidInputs:invalid.length,staticUpdates:0,resumedUpdates:1,mainActorFrames:18,scope:fixture.contract},null,2));
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
