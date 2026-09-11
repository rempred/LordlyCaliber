'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-chair-resume.json');
global.window=global;vm.runInThisContext('var OB64=window.OB64={};');
const sources={};
for(const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-runtime.js','cutscene-assets.js','cutscene-sprites.js']){
 const code=fs.readFileSync(path.join(root,'editor',file),'utf8');sources[file]=crypto.createHash('sha256').update(code).digest('hex');vm.runInThisContext(code,{filename:file});
}
(async()=>{
 const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);
 for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.scenes.find(s=>s.assetId===fixture.input.assetId);
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),projected=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
 const run=input=>OB64.cutsceneRuntime.compile(projected.document,projected.program,scene,catalog,{z64,nativeLaunchInputs:input,diagnosticAssumptions:false});
 const unchanged=JSON.stringify(fixture.input),result=run(fixture.input);
 assert(Buffer.byteLength(unchanged,'utf8')<=131072);
 assert.equal(result.outcome,'qualified-resume-update-complete');assert.equal(result.states.length,1);
 assert.equal(result.capturedSnapshot.executedUpdates,1);assert.equal(JSON.stringify(fixture.input),unchanged);
 const frames=OB64.cutsceneSprites.framesForPreview(OB64.cutsceneSprites.create(z64,catalog),result.states[0]);
 assert.equal(Object.keys(frames).length,fixture.expected.length);
 for(const expected of fixture.expected){const a=result.states[0].actors.find(a=>a.slot===expected.slot),n=OB64.cutsceneRuntime.decodeNativeActorState(a.nativeActorState),v=expected.values;
  assert.deepStrictEqual([a.baseX,a.baseY,a.baseZ,n.poseCursor,n.poseDelay,a.displayedFrameToken,n.poseStateIndex],[v.x,v.y,v.z,v.cursor,v.delay,v.frameToken,v.state]);}
 assert.equal(result.resumedState.parserWord,fixture.input.capturedSnapshot.value.observedParserCursor);
 assert.equal(result.resumedState.registeredCounter,fixture.expectedCounter);
 for(const expected of fixture.expected){const job=result.resumedState.movementSlots[expected.slot];
  if(!expected.movement)assert.equal(job,null);
  else assert.deepStrictEqual([job.vx,job.vz,job.remaining,job.pauseByte],[expected.movement.velocityX,expected.movement.velocityZ,expected.movement.countdown,expected.movement.pause]);}
 assert.equal(result.resumedMenuSelection.outcome,'empty-selected-list');
 assert.equal(result.resumedMenuSelection.rootHex,fixture.input.capturedResume.value.menuRootHex);
 assert.deepStrictEqual(result.resumedMenuSelection.menuOwners,fixture.input.capturedResume.value.menuOwners);
 assert.equal(result.resumedMenuSelection.rootHex,fixture.expectedMenu.rootHex);
 assert.deepStrictEqual(result.resumedMenuSelection.menuOwners,fixture.expectedMenu.menuOwners);
 const composite=result.trace.findIndex(t=>t.kind==='composite'),menu=result.trace.findIndex(t=>t.kind==='resumed-menu-selection');
 assert(composite>=0&&menu>composite);
 const staticInput=JSON.parse(unchanged);delete staticInput.capturedResume;
 assert.equal(run(staticInput).outcome,'captured-snapshot-resume-input');
 staticInput.capturedResume={status:'unknown'};
 assert.equal(run(staticInput).capturedSnapshot.executedUpdates,0);
 const oneTick=OB64.cutsceneRuntime.compile(projected.document,projected.program,scene,catalog,{z64,nativeLaunchInputs:fixture.input,maxTicks:1});
 assert.equal(oneTick.outcome,result.outcome);assert.equal(oneTick.states.length,1);
 function alterHex(input,key,offset,value,width=4){const bytes=Buffer.from(input.capturedResume.value[key],'hex');if(width===4)bytes.writeUInt32BE(value,offset);else bytes[offset]=value;input.capturedResume.value[key]=bytes.toString('hex');}
 const negatives=[
  ['resume-job-input',i=>alterHex(i,'primaryOwnerHex',0x1D8,1)],
  ['resume-job-input',i=>alterHex(i,'primaryOwnerHex',0x2B8,1)],
  ['resume-job-input',i=>alterHex(i,'primaryOwnerHex',0x1CA4,1)],
  ['resume-job-input',i=>alterHex(i,'primaryOwnerHex',0x1CB1,1,1)],
  ['resume-job-input',i=>alterHex(i,'secondaryOwnerHex',0x840,1,1)],
  ['resume-menu-input',i=>alterHex(i,'menuRootHex',4,0x80300000)],
  ['resume-menu-input',i=>i.capturedResume.value.menuOwners.pop()],
  ['resume-memory-input',i=>i.capturedResume.value.menuRootAddress=i.capturedResume.value.primaryOwnerAddress],
  ['resume-input',i=>i.capturedResume.value.updates=2],
  ['resume-input',i=>i.capturedResume.value.origin='historical'],
  ['resume-input',i=>i.capturedResume.value.registeredCounter=1],
  ['resume-input',i=>i.capturedResume.value.tailTimer=0],
  ['resume-input',i=>i.schedulerBranch.value='alternate'],
  ['resume-transition-input',i=>i.capturedSnapshot.value.observedParserCursor++],
  ['resume-transition-input',i=>{const b=Buffer.from(i.capturedSnapshot.value.slots[0].movementHex,'hex');b.writeUInt16BE(26,12);i.capturedSnapshot.value.slots[0].movementHex=b.toString('hex');}],
  ['resume-movement-input',i=>{const b=Buffer.from(i.capturedSnapshot.value.slots[0].movementHex,'hex');b.writeUInt16BE(1,12);i.capturedSnapshot.value.slots[0].movementHex=b.toString('hex');}]
 ];
 for(const [code,mutate] of negatives){const input=JSON.parse(unchanged);mutate(input);assert.throws(()=>run(input),e=>e.code===code,code);}
 const asyncResult=await OB64.cutsceneRuntime.compileAsync(projected.document,projected.program,scene,catalog,{z64,nativeLaunchInputs:fixture.input});
 assert.deepStrictEqual(asyncResult.states,result.states);
 const controller=new AbortController();controller.abort();await assert.rejects(OB64.cutsceneRuntime.compileAsync(projected.document,projected.program,scene,catalog,{z64,nativeLaunchInputs:fixture.input,signal:controller.signal}),e=>e.name==='AbortError');
 console.log(JSON.stringify({status:'pass',generatorSha256:crypto.createHash('sha256').update(fs.readFileSync(__filename)).digest('hex'),fixtureSha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-chair-resume.json'))).digest('hex'),sources,outcome:result.outcome,actors:fixture.expected.length,spriteFrames:Object.keys(frames).length,trace:result.trace,menu:result.resumedMenuSelection,resumedState:result.resumedState,negativeControls:negatives.length,retainedStateBytes:result.retainedStateBytes,pendingWait:result.pendingWait,staticDefault:true,cancellation:true,asyncParity:true},null,2));
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
