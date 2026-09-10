'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..');global.window=global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for(const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-dialogue-data.js','cutscene-dialogue.js','cutscene-runtime.js','cutscene-preview.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',file),'utf8'),{filename:file});
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);
for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
const fixture=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-native-dialogue.json'),'utf8'));
(async()=>{
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.directorScenes.find(s=>s.friendlyName==='Graduation Ceremony');
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),document=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog).document;
 function program(words){const b=new Uint8Array(words.length*4),v=new DataView(b.buffer);words.forEach((w,i)=>v.setUint32(i*4,w>>>0));const selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:b.length,decodedWordCount:words.length},launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:2}}};return {selected,ir:OB64.cutsceneCodec.createIr(selected,b)};}
 function launch(events){return {schema:'ob64-cutscene-launch-inputs.v1',assetId:scene.assetId,invocationId:'dialogue-fixture',sourceIdentity:'explicit-native-service-fixture',evidenceGrade:'Candidate',existingActors:{status:'known',value:{slots:Array(28).fill(null),otherJobsEmpty:true}},schedulerBranch:{status:'known',value:'normal'},externalProducers:{status:'known',value:{throughTick:5,initialDialogue:fixture.initialDialogue,menuCreates:[],colorCreates:[],poseCalls:[],events}}};}
 const service={tick:1,phase:'before-director',kind:'dialogue',service:'callback',slot:0,ownerId:'text-owner',eligible:true,storage:{restoreHandle:0x80630000,restoreReturned:true,freeReturned:true,saveHandle:0x80630000,saveReturned:true},helpers:[],controller:{actionMask:0,directionMask:0,dummyMask:0,historyMask:0,queueHead:0}};
 const p=program([0,0x80000000,0x10,0,2,0,0x80000001]);
 const result=OB64.cutsceneRuntime.compile(document,p.ir.program,p.selected,catalog,{z64,maxTicks:5,diagnosticAssumptions:false,nativeLaunchInputs:launch([service])});
 assert(result.terminated,JSON.stringify({outcome:result.outcome,missing:result.missingInputs,trace:result.trace}));assert.strictEqual(result.states.length,2);assert.strictEqual(result.states[1].nativeExternal.dialogue.owners[0].payloadHex.slice(0x3c*2,0x3c*2+2),'04');
 const waiting=OB64.cutsceneRuntime.compile(document,p.ir.program,p.selected,catalog,{z64,maxTicks:3,diagnosticAssumptions:false,nativeLaunchInputs:launch([])});assert(!waiting.terminated);assert.strictEqual(waiting.states.length,3);
 const unknown=launch([]);delete unknown.externalProducers.value.initialDialogue;
 const boundary=OB64.cutsceneRuntime.compile(document,p.ir.program,p.selected,catalog,{z64,maxTicks:3,diagnosticAssumptions:false,nativeLaunchInputs:unknown});assert.strictEqual(boundary.outcome,'external-input');
 const missingStorage={...service};delete missingStorage.storage;
 const storageBoundary=OB64.cutsceneRuntime.compile(document,p.ir.program,p.selected,catalog,{z64,maxTicks:3,diagnosticAssumptions:false,nativeLaunchInputs:launch([missingStorage])});assert.strictEqual(storageBoundary.unresolvedQuery.code,'dialogue-payload-allocation');
 const choiceLaunch=launch([service]),choiceInput=choiceLaunch.externalProducers.value.initialDialogue=structuredClone(fixture.initialDialogue);
 choiceInput.memory.find(r=>r.address===0x80600000).hex=Buffer.from('@w3001@s\0','latin1').toString('hex');
 const choiceProgram=program([0,0x80000000,0x25,0,1,3,0x80000001]);
 const choice=OB64.cutsceneRuntime.compile(document,choiceProgram.ir.program,choiceProgram.selected,catalog,{z64,maxTicks:3,diagnosticAssumptions:false,nativeLaunchInputs:choiceLaunch});assert(choice.terminated,JSON.stringify(choice.missingInputs));assert.strictEqual(choice.states.length,2);
 const cancelLaunch=launch([service]);cancelLaunch.externalProducers.value.initialDialogue=structuredClone(fixture.initialDialogue);
 cancelLaunch.externalProducers.value.initialDialogue.memory.find(r=>r.address===0x80600000).hex=Buffer.from('@#'.repeat(10000)+'@s\0','latin1').toString('hex');
 const abort=new AbortController(),originalStep=OB64.cutsceneDialogue.Machine.prototype.step;let steps=0;
 OB64.cutsceneDialogue.Machine.prototype.step=function(pc){const value=originalStep.call(this,pc);if(++steps===2000)abort.abort();return value;};
 try{await assert.rejects(()=>OB64.cutsceneRuntime.compileAsync(document,p.ir.program,p.selected,catalog,{z64,maxTicks:3,nativeLaunchInputs:cancelLaunch,signal:abort.signal}),e=>e.name==='AbortError');assert(steps<2500,'native parsing must yield before exhausting its service instruction ceiling');}finally{OB64.cutsceneDialogue.Machine.prototype.step=originalStep;}
 const createWords=[0xbf,0,0,0,0,0,0,0,0,0,0,0,0,0],createdProgram=program([...createWords,0,0x80000000,0x10,0,2,0,0x80000001]);
 const createLaunch=launch([]),e=createLaunch.externalProducers.value;e.throughTick=20;e.initialDialogue=structuredClone(fixture.initialDialogue);e.initialDialogue.owners=Array(6).fill(null);
 e.initialDialogue.memory.find(r=>r.address===0x800e82c8).hex='00'.repeat(0xa8*6);
 e.initialDialogue.memory.find(r=>r.address===0x80600000).hex='00000004'+Buffer.from('@S0ABC@s\0','latin1').toString('hex');
 function mem(address,data){e.initialDialogue.memory.push({address,hex:Buffer.from(data).toString('hex'),writable:true});}
 mem(0x8018fc70,Buffer.from('8061000080611000','hex'));mem(0x80190f74,Buffer.from('ffff','hex'));mem(0x80610000,new Uint8Array(166).fill(7));mem(0x80611000,new Uint8Array(166).fill(7));mem(0x800c49d0,[0,1]);mem(0x800e79a0,[0,0,0,1]);
 const record=Buffer.alloc(0xa8);record.writeUInt16BE(0xc800);record.writeUInt32BE(0x80198be8,0x10);
 e.dialogueCreates=[{nodeId:createdProgram.ir.program.primitives[0].id,occurrence:0,ownerId:'created-text',slot:0,directorWords:createWords,recordHex:record.toString('hex')}];
 e.events.push({...service,tick:1,ownerId:'created-text',service:'initialize',helpers:[{address:0x8007938c,args:[27506610,0,4294967292,0],result:0x80600000,writes:[],preservesOtherRegisters:true}]});
 for(let tick=2;tick<=18;tick++)e.events.push({tick,phase:'before-director',kind:'dialogue',service:'opening',ownerId:'resource-pool',eligible:true,helpers:[]},{...service,tick,phase:'after-director',ownerId:'created-text'});
 const created=OB64.cutsceneRuntime.compile(document,createdProgram.ir.program,createdProgram.selected,catalog,{z64,maxTicks:20,diagnosticAssumptions:false,nativeLaunchInputs:createLaunch});assert(created.terminated,JSON.stringify({outcome:created.outcome,missing:created.missingInputs,query:created.unresolvedQuery,trace:created.trace.slice(-4)}));assert(created.states.length>8);assert(created.states.at(-1).dialogue[0].payload.nativeDialogue.outputHex.startsWith('414243'));
 const releaseProgram=program([...createWords,0,0x80000000,0x10,0,2,0,5,0,0,0x80000000,0x10,0,1,0,0x80000001]),releaseLaunch=structuredClone(createLaunch),re=releaseLaunch.externalProducers.value,releaseTick=created.states.length+8;
 re.throughTick=releaseTick+1;re.dialogueCreates[0].nodeId=releaseProgram.ir.program.primitives[0].id;re.events=re.events.filter(row=>row.tick===1);
 for(let tick=2;tick<=releaseTick;tick++){
   re.events.push({tick,phase:'before-director',kind:'dialogue',service:'opening',ownerId:'resource-pool',eligible:true,helpers:[]},
    {tick,phase:'before-director',kind:'dialogue',service:'closing',ownerId:'resource-pool',eligible:true,helpers:tick===releaseTick?[{address:0x800712c4,args:[0x80630000],result:0,writes:[],preservesOtherRegisters:true}]:[]});
   if(tick<releaseTick)re.events.push({...service,tick,phase:'after-director',ownerId:'created-text'});
 }
 const released=OB64.cutsceneRuntime.compile(document,releaseProgram.ir.program,releaseProgram.selected,catalog,{z64,maxTicks:releaseTick+2,diagnosticAssumptions:false,nativeLaunchInputs:releaseLaunch});assert(released.terminated,JSON.stringify({outcome:released.outcome,missing:released.missingInputs,query:released.unresolvedQuery,states:released.states.length}));assert.strictEqual(released.states.length,releaseTick+1);assert.deepStrictEqual(released.states.at(-1).dialogue,[]);assert.strictEqual(released.states.at(-1).nativeExternal.dialogue.owners[0],null);
 console.log(JSON.stringify({status:'PASS',actualPausePlayback:true,explicitNativeWait:true,missingInitialState:true,controlBytePlayback:true,payloadAllocationBoundary:true,cooperativeCancellationSteps:steps,creationPauseTick:created.states.length-1,releaseTick}));
})().catch(error=>{console.error(error);process.exitCode=1;});
