'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-chair-snapshot.json');
global.window=global;vm.runInThisContext('var OB64=window.OB64={};');
const sourceHashes={};
for(const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-runtime.js']){
 const source=fs.readFileSync(file==='cutscene-runtime.js'&&process.argv[2]?process.argv[2]:path.join(root,'editor',file),'utf8');
 sourceHashes[file]=crypto.createHash('sha256').update(source).digest('hex');vm.runInThisContext(source,{filename:file});
}
(async()=>{
 const v64=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(v64.length);
 for(let i=0;i<v64.length;i+=2){z64[i]=v64[i+1];z64[i+1]=v64[i];}
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.scenes.find(s=>s.assetId===fixture.input.assetId);
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),projected=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
 const options={z64,nativeLaunchInputs:fixture.input,diagnosticAssumptions:false};
 const before=JSON.stringify(fixture.input),runtime=OB64.cutsceneRuntime.compile(projected.document,projected.program,scene,catalog,options);
 assert.equal(runtime.outcome,'captured-snapshot-resume-input');assert.equal(runtime.durationTicks,1);
 assert.equal(runtime.capturedSnapshot.executedUpdates,0);assert.match(runtime.clockUnit,/no elapsed update/);
 assert(!runtime.trace.some(row=>row.kind==='composite'));assert.equal(JSON.stringify(fixture.input),before);
 for(const expected of fixture.expected){const a=runtime.states[0].actors.find(a=>a.slot===expected.slot),native=OB64.cutsceneRuntime.decodeNativeActorState(a.nativeActorState);
  assert.deepStrictEqual([a.baseX,a.baseY,a.baseZ,native.poseCursor,native.poseDelay,a.displayedFrameToken],[expected.x,expected.y,expected.z,expected.cursor,expected.delay,expected.token]);}
 const diagnostic=OB64.cutsceneRuntime.compile(projected.document,projected.program,scene,catalog,{...options,diagnosticAssumptions:true});
 assert.equal(diagnostic.outcome,runtime.outcome);assert.equal(diagnostic.states.length,1);
 const mutations=[i=>i.capturedSnapshot.value.resumeState='known',i=>i.capturedSnapshot.value.otherJobsEmpty=true,
  i=>i.existingActors={status:'known',value:{slots:fixture.input.capturedSnapshot.value.slots,otherJobsEmpty:true}},
  i=>i.externalProducers={status:'known',value:{}},i=>i.capturedSnapshot.value.slots.pop()];
 for(const mutate of mutations){const bad=JSON.parse(before);mutate(bad);assert.throws(()=>OB64.cutsceneRuntime.validateLaunchInputs(bad,scene.assetId));}
 const cancelled=new AbortController();cancelled.abort();await assert.rejects(OB64.cutsceneRuntime.compileAsync(projected.document,projected.program,scene,catalog,{...options,signal:cancelled.signal}),e=>e.name==='AbortError');
 const asyncResult=await OB64.cutsceneRuntime.compileAsync(projected.document,projected.program,scene,catalog,options);assert.equal(asyncResult.outcome,runtime.outcome);assert.deepStrictEqual(asyncResult.states,runtime.states);
 assert(runtime.retainedStateBytes>0 && runtime.retainedStateBytes<=runtime.limits.maxStateBytes);
 console.log(JSON.stringify({status:'pass',sourceHashes,actors:fixture.expected.length,invalidInputs:mutations.length,cancellation:'pass',asyncParity:'pass',outcome:runtime.outcome,retainedStateBytes:runtime.retainedStateBytes,limits:runtime.limits}));
})().catch(e=>{console.error(e.stack||e.message||e);process.exitCode=1;});
