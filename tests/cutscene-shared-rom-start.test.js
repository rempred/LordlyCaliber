"use strict";
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),R=path.resolve(__dirname,'../..');
new Function('require','__dirname',fs.readFileSync(path.join(__dirname,'cutscene-shared-actor-integration.test.js'),'utf8').split('\n(async()=>')[0])(require,__dirname);
for(const file of ['cutscene-rom-start-data.js','cutscene-rom-start.js','cutscene-njpg.js','cutscene-dialogue-draw.js'])vm.runInThisContext(fs.readFileSync(path.join(R,'editor',file),'utf8'),{filename:file});
(async()=>{
 const raw=fs.readFileSync(path.join(R,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}},ui=OB64.cutsceneUI.initialize(rom),runs=[];
 for(const [id,passes,actors,env,selector]of [['01F3F242',3032,11,51,9],['01FA4D0A',235,5,61,971]]){
  const scene=ui.catalog.getScene('rom-custom-lz:'+id),source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,ui.catalog),input=OB64.cutsceneRomStart.input(z64,scene,p.program);
  const launch=input.externalProducers.value.directorLaunch;assert.equal(launch.sceneMode,2);assert.equal(launch.world.mapKind,24);assert.equal(launch.environmentSelector,env);assert.equal(launch.selector,selector);
  await OB64.cutsceneUI.loadScene(rom,ui,scene);const r=ui.runtimeByAssetId[scene.assetId];assert(r,JSON.stringify(ui.sourceErrors));assert.equal(r.outcome,'modeled-termination');assert.equal(r.states.length,passes);assert.deepStrictEqual(r.missingInputs,[]);assert.equal(r.states.at(-1).runtime.terminalReason,'terminal-state-release');assert.equal(r.states.at(-1).actors.length,actors);assert(r.retainedStateBytes<128*1024*1024);assert.equal(r.limits.maxStateBytes,128*1024*1024);
  const renamed={...scene,assetId:'test-unrelated-identity',name:'arbitrary',title:'arbitrary'};assert(OB64.cutsceneRomStart.supports(renamed,p.program));const renamedInput=OB64.cutsceneRomStart.input(z64,renamed,p.program);assert.deepStrictEqual(renamedInput.externalProducers,input.externalProducers);
  const renamedProjection=OB64.cutsceneCodec.projectSceneDocument(renamed,source,ui.catalog),renamedRun=OB64.cutsceneRuntime.compile(renamedProjection.document,renamedProjection.program,renamed,ui.catalog,{z64,diagnosticAssumptions:false,nativeLaunchInputs:renamedInput,maxTicks:120}),facts=s=>({actors:s.actors.map(a=>[a.slot,a.baseX,a.baseY,a.baseZ,a.bank,a.animationKey,a.displayedFrameToken]),camera:s.cameraState,projection:s.actorProjection});
  assert.equal(renamedRun.unresolvedQuery,null);for(const pass of [0,50,119])assert.deepStrictEqual(facts(renamedRun.states[pass]),facts(r.states[pass]),'identity-independent execution '+pass);
  const altered=structuredClone(p.program);altered.primitives.at(-1).rawWords[altered.primitives.at(-1).rawWords.length-1]=0xff000004;
  assert.equal(OB64.cutsceneRomStart.analyze(altered).code,'rom-start-room-group');assert(!OB64.cutsceneRomStart.supports({...scene,assetId:'rom-custom-lz:01F3EAD2'},altered));
  const noStage=structuredClone(p.program);noStage.primitives.shift();assert.equal(OB64.cutsceneRomStart.analyze(noStage).code,'rom-start-inherited-stage');
  for(const value of [0xffffffff,0xfffffffe]){const q=structuredClone(p.program);q.primitives[0].rawWords[1]=value;assert.equal(OB64.cutsceneRomStart.analyze(q).code,'rom-start-derived-environment');}
  const duplicate=structuredClone(p.program);duplicate.primitives.splice(1,0,structuredClone(duplicate.primitives[0]));assert.equal(OB64.cutsceneRomStart.analyze(duplicate).code,'rom-start-stage-sequence');
  const delayed=structuredClone(p.program);delayed.primitives.splice(1,0,delayed.primitives.shift());assert.equal(OB64.cutsceneRomStart.analyze(delayed).code,'rom-start-stage-sequence');
  const noTerminal=structuredClone(p.program);noTerminal.primitives.at(-1).rawWords.shift();assert.equal(OB64.cutsceneRomStart.analyze(noTerminal).code,'rom-start-stream');
  const bad=structuredClone(p.program);bad.primitives[0].rawWords[1]=NaN;assert.equal(OB64.cutsceneRomStart.analyze(bad).code,'rom-start-stream');
  const changed=z64.slice();changed[0x283f10]^=1;assert.throws(()=>OB64.cutsceneRomStart.input(changed,scene,p.program),e=>e.code==='rom-start-code');assert.throws(()=>OB64.cutsceneRomStart.installCode({},[],changed),e=>e.code==='rom-start-code');
  const point=OB64.cutsceneRuntime.evaluate(r,100);point.actors[0].x=9999;point.nativeExternal.dialogue.memory[0].hex='bad';assert.notEqual(OB64.cutsceneRuntime.evaluate(r,100).actors[0].x,9999);assert.notEqual(OB64.cutsceneRuntime.evaluate(r,100).nativeExternal.dialogue.memory[0].hex,'bad');
  runs.push({scene,p,input,r});
 }

 // Direct guard controls retain the native empty-name branches. Populated roster
 // names and other names are not replaced by the protagonist default.
 const M=OB64.cutsceneDialogue.Machine,allocate=rows=>new M({},rows.map(([address,n])=>({address,bytes:new Uint8Array(n),writable:true}))),names=allocate([[0x8018f500,16],[0x80383400,17]]),caller=allocate([[0x8022a974,4],[0x801ce8bc,4],[0x80300000,256],[0x80301000,336],[0x80302000,0x2000]]),launch={input:{previewHeroName:'Joe'},machine:caller};
 caller.put(0x8022a974,0x80300000);caller.put(0x801ce8bc,0x80302000);
 OB64.cutsceneRomStart.prepareDialogue(launch,{machine:names},0);assert.equal(names.get(0x8018f500),names.get(0x8018f504));assert.equal(names.get(0x80383400,1),74);
 caller.put(0x80300018,0x80301000);caller.put(0x80301147,20,1);OB64.cutsceneRomStart.prepareDialogue(launch,{machine:names},0);assert.equal(names.get(0x8018f504),0x80383400);
 caller.put(0x80301147,0,1);OB64.cutsceneRomStart.prepareDialogue(launch,{machine:names},0);assert.equal(names.get(0x8018f504),0x80383400);
 caller.put(0x8030220c,1);OB64.cutsceneRomStart.prepareDialogue(launch,{machine:names},0);
 for(const i of [1,2,3])assert.throws(()=>names.get(0x8018f500+i*4),e=>e.code==='rom-start-dialogue-name');assert.equal(names.get(0x8018f500),0x80383400);
 for(const name of ['', '12345678901234567','bad\nname']){launch.input.previewHeroName=name;assert.throws(()=>OB64.cutsceneRomStart.prepareDialogue(launch,{machine:names},0),e=>e.code==='rom-start-player-name');}
 const {scene,p,input,r}=runs[0],runtime=OB64.cutsceneRuntime;
 // Compare initial and late dialogue snapshots with the previous ordinary representation.
 const options={z64,diagnosticAssumptions:false,nativeLaunchInputs:input,maxTicks:2200},compact=runtime.compile(p.document,p.program,scene,ui.catalog,options);
 const plainSource=fs.readFileSync(path.join(R,'editor/cutscene-runtime.js'),'utf8').replace('if(sharedActorProfile||romOnlyStart){nextSnapshot.actors=compactRecords','if(false){nextSnapshot.actors=compactRecords').replace("if(sharedActorProfile||romOnlyStart)['actors','effects']","if(false)['actors','effects']").replace('dialogueEngine.snapshot(sharedActorProfile||romOnlyStart?256:undefined)','dialogueEngine.snapshot()');
 vm.runInThisContext(plainSource);const plain=OB64.cutsceneRuntime.compile(p.document,p.program,scene,ui.catalog,options);OB64.cutsceneRuntime=runtime;
 function merge(rows){const out=[];for(const row of rows.slice().sort((a,b)=>a.address-b.address)){const last=out.at(-1);if(last&&last.address+last.hex.length/2===row.address)last.hex+=row.hex;else out.push({...row});}return out;}
 for(const i of [...Array.from({length:120},(_,i)=>i),500,1000,2163,2199]){const a=runtime.evaluate(compact,i),b=runtime.evaluate(plain,i);a.nativeExternal.dialogue.memory=merge(a.nativeExternal.dialogue.memory);b.nativeExternal.dialogue.memory=merge(b.nativeExternal.dialogue.memory);assert.deepStrictEqual(a,b,'snapshot equivalence '+i);}
 assert(compact.retainedStateBytes<plain.retainedStateBytes);
 // Native text measurement reaches the substitution in the loaded Formation text.
 const named=structuredClone(input);named.externalProducers.value.directorLaunch.previewHeroName='Joe';
 const step=OB64.cutsceneDialogue.Machine.prototype.step,readNames=[];
 OB64.cutsceneDialogue.Machine.prototype.step=function(pc){if(pc===0x8019cc74){const index=this.get(this.r[19],1),pointer=this.get(this.r[22]+index*4);let name='';for(let i=0;i<17&&this.get(pointer+i,1);i++)name+=String.fromCharCode(this.get(pointer+i,1));readNames.push({index,name});}return step.call(this,pc);};
 try{const namedRun=runtime.compile(p.document,p.program,scene,ui.catalog,{z64,diagnosticAssumptions:false,nativeLaunchInputs:named,maxTicks:2250});assert.equal(namedRun.unresolvedQuery,null,JSON.stringify(namedRun.unresolvedQuery));}finally{OB64.cutsceneDialogue.Machine.prototype.step=step;}
 assert(readNames.some(x=>x.index===48&&x.name==='Joe'),JSON.stringify(readNames));
 // Native consumers must receive a precise boundary for missing non-default names.
 OB64.cutsceneDialogue.Machine.prototype.step=function(pc){if(pc===0x8019cc74)this.put(this.r[19],50,1);return step.call(this,pc);};
 try{const missing=runtime.compile(p.document,p.program,scene,ui.catalog,{z64,diagnosticAssumptions:false,nativeLaunchInputs:input,maxTicks:2250});assert(missing.unresolvedQuery,JSON.stringify({outcome:missing.outcome,missing:missing.missingInputs,passes:missing.states.length}));assert.equal(missing.unresolvedQuery.code,'rom-start-dialogue-name');assert(missing.unresolvedQuery.label.includes('player army name'));}finally{OB64.cutsceneDialogue.Machine.prototype.step=step;}
 // Supplied inputs keep priority, including their name and controller policy.
 const supplied=structuredClone(named);supplied.invocationId='supplied-name';supplied.externalProducers.value.resourceSchedule.pageAdvancePolicy='neutral';
 OB64.cutsceneUI.setNativeLaunchInputs(ui,scene,supplied);await OB64.cutsceneUI.loadScene(rom,ui,scene);assert.equal(ui.romStartupByAssetId[scene.assetId],false);assert.equal(ui.runtimeByAssetId[scene.assetId].outcome,'awaiting-dialogue-input');
 OB64.cutsceneUI.setNativeLaunchInputs(ui,scene,null);await OB64.cutsceneUI.loadScene(rom,ui,scene);assert.equal(ui.romStartupByAssetId[scene.assetId],true);assert.equal(ui.runtimeByAssetId[scene.assetId].states.length,3032);assert(OB64.cutsceneUI.playbackTimingLabel(ui,scene).includes('Magnus (text only)'));
 console.log(JSON.stringify({status:'pass',scenes:runs.map(x=>({assetId:x.scene.assetId,passes:x.r.states.length,actors:x.r.states.at(-1).actors.length,retainedBytes:x.r.retainedStateBytes})),identityIndependent:true,rejectedClassInheritanceMalformed:true,currentRomCallerQualification:true,plainSnapshotEquivalence:124,lateSnapshotPasses:[500,1000,2163,2199],substitutionName:readNames,missingArmyNameBoundary:true,importPriorityAndClearing:true}));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
