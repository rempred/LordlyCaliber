'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..');
global.window=global;vm.runInThisContext('var OB64 = window.OB64 = {};');
for(const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js',
 'cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-runtime.js','cutscene-preview.js']) {
 vm.runInThisContext(fs.readFileSync(path.join(root,'editor',file),'utf8'),{filename:file});
}
const native=OB64.cutsceneRuntime.nativeExternal, actorNative=OB64.cutsceneRuntime.nativeActor;
const fixture=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-native-external.json'),'utf8'));
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
const z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
const read=offset=>z64[offset]*256+z64[offset+1];
const checks=[];function check(name,fn){fn();checks.push(name);}
function menu(){return {entityId:'entity',substate:6,selection:1,cancel:-2,optionCount:3,status:0};}
function service(extra={}){return {eligible:true,entityId:'entity',actionMask:0,directionMask:0,
 readiness:{readyByte:1,alpha:255,cooldown:0},openingReturned:true,...extra};}
check('accepted menu boundary examples',()=>{
 for(const [substate,alpha,cooldown,actionMask,selection,cancel,result] of fixture.menus){
  const m={...menu(),substate,selection,cancel};native.advanceMenu(m,service({actionMask,readiness:{readyByte:1,alpha,cooldown}}));assert.strictEqual(m.status,result);
 }
});
check('menu service eligibility, ownership and direction priority',()=>{
 const m=menu();native.advanceMenu(m,service({eligible:false,actionMask:0x8000}));assert.strictEqual(m.status,0);
 assert.throws(()=>native.advanceMenu(m,service({entityId:'other'})),/owner/);
 native.advanceMenu(m,service({directionMask:0xC00}));assert.strictEqual(m.selection,0);assert.strictEqual(m.status,-1);
 native.advanceMenu(m,service({directionMask:0x800}));assert.strictEqual(m.selection,0);
 for(let i=0;i<5;i++)native.advanceMenu(m,service({directionMask:0x400}));assert.strictEqual(m.selection,2);
 native.advanceMenu(m,service({actionMask:0xC000}));assert.strictEqual(m.status,2);assert.strictEqual(m.substate,3);
});
check('menu opening phases and graceful close use eligible services',()=>{
 const m={...menu(),substate:0};native.advanceMenu(m,service({readiness:{readyByte:0,alpha:255,cooldown:0}}));assert.strictEqual(m.substate,0);
 native.advanceMenu(m,service());assert.strictEqual(m.substate,5);
 native.advanceMenu(m,service());assert.strictEqual(m.substate,6);
 Object.assign(m,{closing:true,closeRemaining:8});
 for(let i=0;i<7;i++)native.advanceMenu(m,service());assert(!m.detached);
 native.advanceMenu(m,service({eligible:false}));assert.strictEqual(m.closeRemaining,1);
 native.advanceMenu(m,service());assert(m.detached);
});
check('accepted signed color examples',()=>{
 for(const example of fixture.colors){const c=native.createColor(null,[0x1B,example.initialS16,1,2,3,255,0]),actual=[c.remaining];
  for(let i=0;i<3;i++){native.advanceColor(c,true);actual.push(c.remaining);}assert.deepStrictEqual(actual,example.threeEligibleUpdates);}
});
check('color truncation, skipped services, reuse and cleanup ownership',()=>{
 let c=native.createColor(null,[0x1B,3,257,258,259,257,254]);assert.strictEqual(c.alpha,1);assert.strictEqual(c.red,1);
 native.advanceColor(c,false);assert.strictEqual(c.remaining,3);
 native.advanceColor(c,true);assert.strictEqual(c.alpha,86);
 c=native.createColor(c,[0x1B,2,0,0,0,-1,0]);assert.strictEqual(c.startAlpha,86);assert.strictEqual(c.ownershipFlag,2);
 assert.strictEqual(native.cleanupColor(c),c);assert.strictEqual(c.remaining,0);
 c.ownershipFlag=1;assert.strictEqual(native.cleanupColor(c),null);
 assert.strictEqual(native.cleanupColor(null),null);
});
check('accepted physical shared-request selections',()=>{
 for(const example of fixture.shared.actualPrograms){const c=example.control;
  const actual=native.selectSharedRequest(c.opcode,c.operands,0,{projectionReturned:true},read);
  assert.strictEqual(actual.request,c.ordinaryModeResult.requestId);assert.strictEqual(actual.context,c.ordinaryModeResult.context);
  assert.strictEqual(actual.tableOffset,parseInt(c.ordinaryModeResult.tableReadZ64));}
});
check('shared selection boundaries and alternate owner input',()=>{
 assert(native.selectSharedRequest(17,[3,64],0,null,read).boundary);
 assert(native.selectSharedRequest(18,[0,1],0,null,read).boundary);
 assert.deepStrictEqual(native.selectSharedRequest(18,[0,1],1,{alternate:{childPresent:false}},read),{suppressed:true});
 for(const x of [105,106,212,213]){const r=native.selectSharedRequest(20,[2,78],1,{alternate:{childPresent:true,metadataPresent:true,type:1,projectedX:x}},read);
  assert.strictEqual(r.tableOffset,0x212880+2*(590+(x>=213?0:x>=106?1:2)));}
 const alt=native.selectSharedRequest(18,[255,255],1,{alternate:{childPresent:true,metadataPresent:true,type:29,projectedX:0,ownerMetaB:51}},read);
 assert.strictEqual(alt.tableOffset,0x2123A0+24);
});
check('shared registrations replace slots without pose delay',()=>{
 const a={decoderMode:0,poseDelay:0,poseCursor:-1,poseFrame:0,material:Array(16).fill(10),materialDelta:Array(16).fill(1)};
 const slots={A:-1,B:-1},records=[{opcode:17,operands:[0,1]},{opcode:17,operands:[0,2]},{opcode:19,operands:[0,3]},{opcode:1,operands:[9,4]}];
 const result=actorNative.advancePose(a,()=>({records}),256,(actor,r)=>{const s=native.selectSharedRequest(r.opcode,r.operands,actor.decoderMode,null,read);slots[s.context]=s.request;return null;});
 assert.strictEqual(result,null);assert.strictEqual(a.poseCursor,3);assert.strictEqual(a.poseDelay,2);assert.strictEqual(a.poseFrame,2);
 assert.strictEqual(a.material[0],11);assert.deepStrictEqual(slots,{A:read(0x212884),B:read(0x212886)});
});
(async()=>{
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.directorScenes.find(s=>s.friendlyName==='Graduation Ceremony');
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),document=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog).document;
 function program(words){const b=new Uint8Array(words.length*4),v=new DataView(b.buffer);words.forEach((w,i)=>v.setUint32(i*4,w>>>0));
  const selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:b.length,decodedWordCount:words.length},launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:2}}};
  return {selected,ir:OB64.cutsceneCodec.createIr(selected,b)};}
 function external(){return {throughTick:12,initialColor:null,initialMenusEmpty:true,initialRequests:{A:-1,B:-1},menuCreates:[],colorCreates:[],events:[],poseCalls:[]};}
 function launch(e){return {schema:'ob64-cutscene-launch-inputs.v1',assetId:scene.assetId,invocationId:'external-fixture',sourceIdentity:'explicit-native-service-fixture',evidenceGrade:'Candidate',
  existingActors:{status:'known',value:{slots:Array(28).fill(null),otherJobsEmpty:true}},schedulerBranch:{status:'known',value:'normal'},externalProducers:{status:'known',value:e}};}
 function run(p,e,extra={}){return OB64.cutsceneRuntime.compile(document,p.ir.program,p.selected,catalog,{z64,maxTicks:25,diagnosticAssumptions:false,nativeLaunchInputs:launch(e),...extra});}
 const colorProgram=program([0x1B,2,10,20,30,255,0,0,0x80000000,0x7F,0,0,0x80000001]);
 const ce=external();ce.colorCreates.push({nodeId:colorProgram.ir.program.primitives[0].id,occurrence:0,ownerId:'color-1',allocationReady:true,registrationReturned:true});
 ce.events=[{tick:1,phase:'before-director',kind:'color',ownerId:'color-1',eligible:false},{tick:2,phase:'before-director',kind:'color',ownerId:'color-1',eligible:true},{tick:4,phase:'before-director',kind:'color',ownerId:'color-1',eligible:true}];
 const colorResult=run(colorProgram,ce);
 check('actual color playback awaits selected callbacks',()=>{assert(colorResult.terminated,JSON.stringify({outcome:colorResult.outcome,trace:colorResult.trace,missing:colorResult.missingInputs}));assert.strictEqual(colorResult.states.length,5);assert.deepStrictEqual(colorResult.states.map(s=>s.overlays[0].remaining),[2,2,1,1,0]);});
 const menuProgram=program([0x62,0,1,0,0x80000000,0x6A,0,2,0,0x80000001]);
 const me=external();me.menuCreates.push({nodeId:menuProgram.ir.program.primitives[0].id,occurrence:0,ownerId:'menu-1',entityId:'entity',preset:1,substate:6,selection:1,cancel:-2,optionCount:3});
 me.events=[{tick:1,phase:'before-director',kind:'menu',slot:0,ownerId:'menu-1',...service({directionMask:0x400})},{tick:2,phase:'before-director',kind:'menu',slot:0,ownerId:'menu-1',...service({actionMask:0xC000})}];
 const menuResult=run(menuProgram,me);
 check('actual menu playback follows explicit choice',()=>{assert(menuResult.terminated,JSON.stringify({outcome:menuResult.outcome,trace:menuResult.trace,missing:menuResult.missingInputs}));assert.strictEqual(menuResult.states.length,3);assert.strictEqual(menuResult.states[2].nativeExternal.menus[0].status,2);});
 check('callback history exhaustion remains missing input',()=>{
  const e=JSON.parse(JSON.stringify(ce));e.events=[];e.throughTick=2;const r=run(colorProgram,e);
  assert.strictEqual(r.outcome,'external-input',JSON.stringify({trace:r.trace.slice(-12),missing:r.missingInputs,last:r.states.at(-1).runtime}));assert(!r.terminated);assert.strictEqual(r.states.at(-1).overlays[0].remaining,2);
  const m=JSON.parse(JSON.stringify(me));m.events=[];m.throughTick=2;assert.strictEqual(run(menuProgram,m).outcome,'external-input');
 });
 check('negative color wait does not become completion',()=>{
  const p=program([0x1B,65535,10,20,30,255,0,0,0x80000000,0x7F,0,0,0x80000001]),e=external();e.throughTick=3;
  e.colorCreates=[{...ce.colorCreates[0],nodeId:p.ir.program.primitives[0].id}];
  e.events=[1,2,3].map(tick=>({tick,phase:'before-director',kind:'color',ownerId:'color-1',eligible:true}));
  const r=run(p,e);assert.strictEqual(r.outcome,'external-input');assert(r.states.every(s=>s.overlays[0].remaining===-1));
 });
 check('last after-Director services run without inventing a later query',()=>{
  const e=JSON.parse(JSON.stringify(ce));e.throughTick=2;
  e.events=[0,1].map(()=>({tick:2,phase:'after-director',kind:'color',ownerId:'color-1',eligible:true}));
  const r=run(colorProgram,e);assert.strictEqual(r.outcome,'external-input');assert(!r.terminated);
  assert.strictEqual(r.states[2].overlays[0].remaining,0);assert.strictEqual(r.states[2].nativeExternal.consumedServices,2);
 });
 check('color cleanup preserves or releases native ownership',()=>{
  const p=program([0x7E,0x80000001]);
  for(const flag of [0,1,2]){const e=external(),b=Buffer.alloc(12);b.writeInt16BE(5,0);b[7]=flag;e.initialColor={ownerId:'inherited',recordHex:b.toString('hex')};
   const r=run(p,e);assert(r.terminated);assert.strictEqual(r.states[0].overlays.length,flag===1?0:1);if(flag!==1)assert.strictEqual(r.states[0].overlays[0].remaining,0);}
 });
 check('menu close, detached query, and explicit release',()=>{
  const p=program([0x62,0,1,0x83,0,0,0x80000000,0x6A,0,-6,0,0x6B,0,0x80000001]),e=external();
  e.menuCreates=[{...me.menuCreates[0],nodeId:p.ir.program.primitives[0].id}];
  e.events=Array.from({length:8},(_,i)=>({tick:i+1,phase:'before-director',kind:'menu',slot:0,ownerId:'menu-1',...service()}));
  const r=run(p,e);assert(r.terminated);assert.strictEqual(r.states.length,9);assert.strictEqual(r.states[7].nativeExternal.menus[0].detached,false);assert.deepStrictEqual(r.states[8].nativeExternal.menus,[]);
  const immediate=program([0x62,0,1,0x8B,0,0x80000001]);e.events=[];e.menuCreates[0].nodeId=immediate.ir.program.primitives[0].id;
  const detached=run(immediate,e);assert(detached.states[0].nativeExternal.menus[0].detached);
 });
 check('stale producer generation rejects without updating replacement',()=>{
  const e=JSON.parse(JSON.stringify(ce));e.events[1].ownerId='old-color';const r=run(colorProgram,e);
  assert.strictEqual(r.unresolvedQuery.code,'external-producer-owner');assert.strictEqual(r.states.at(-1).overlays[0].remaining,2);
 });
 check('explicit release establishes absence without unrelated initial slots',()=>{
  const p=program([0x62,0,1,0x6B,0,0,0x80000000,0x6A,0,-5,0,0x80000001]),e=external();delete e.initialMenusEmpty;
  e.menuCreates=[{...me.menuCreates[0],nodeId:p.ir.program.primitives[0].id}];
  const r=run(p,e);assert(r.terminated,JSON.stringify(r.missingInputs));assert.deepStrictEqual(r.states.at(-1).nativeExternal.menus,[]);
  const unknown=program([0,0x80000000,0x6A,0,-5,1,0x80000001]);assert.strictEqual(run(unknown,e).outcome,'external-input');
 });
 check('ordered and typed launch input rejects ambiguous histories',()=>{
  const e=external();e.events=[{tick:2,phase:'before-director',kind:'request-reset',context:'A'},{tick:1,phase:'before-director',kind:'request-reset',context:'A'}];
  assert.throws(()=>OB64.cutsceneRuntime.validateLaunchInputs(launch(e),scene.assetId),/ordered/);
  e.events=[];e.colorCreates=[ce.colorCreates[0],ce.colorCreates[0]];assert.throws(()=>OB64.cutsceneRuntime.validateLaunchInputs(launch(e),scene.assetId),/duplicated/);
 });
 const hold=program([1,0,0x80000000,14,2,3,2,0x80000001]);
 check('actual Actor registration and explicit request dispatch/reset',()=>{
  const e=external(),b=Buffer.alloc(336);b.writeInt32BE(0,0xE4);b.writeInt32BE(1,0xE8);b.writeInt32BE(5,0xF0);b.writeInt16BE(50,0x134);
  const l=launch(e);l.existingActors.value.slots[0]={identity:'shared-actor',recordHex:b.toString('hex'),movementHex:null};
  e.events=[{tick:1,phase:'after-director',kind:'request-dispatch',context:'A'},{tick:1,phase:'after-director',kind:'request-reset',context:'A'}];
  const r=OB64.cutsceneRuntime.compile(document,hold.ir.program,hold.selected,catalog,{z64,maxTicks:8,diagnosticAssumptions:false,nativeLaunchInputs:l});
  assert(r.terminated,JSON.stringify({outcome:r.outcome,missing:r.missingInputs}));assert(r.trace.some(t=>t.kind==='shared-pose-request'&&t.request===714));
  assert(r.states[1].audio.some(a=>a.kind==='native-shared-request'&&a.program===714));assert.strictEqual(r.states[1].nativeExternal.sharedRequests.A,-1);
 });
 check('all four physical controls integrate with Actor playback',()=>{
  for(const example of fixture.shared.actualPrograms){const c=example.control,e=external(),b=Buffer.alloc(336);
   b.writeInt32BE(0,0xE4);b.writeInt32BE(c.bank,0xE8);b.writeInt32BE(c.recordOrdinal-1,0xF0);b.writeInt16BE(c.stateIndex,0x134);
   const l=launch(e);l.existingActors.value.slots[0]={identity:'shared-actor',recordHex:b.toString('hex'),movementHex:null};
   if([18,20].includes(c.opcode))e.poseCalls=[{actorId:'shared-actor',bank:c.bank,stateIndex:c.stateIndex,recordOrdinal:c.recordOrdinal,opcode:c.opcode,projectionReturned:true}];
   e.events=[{tick:1,phase:'after-director',kind:'request-dispatch',context:c.ordinaryModeResult.context}];
   const r=OB64.cutsceneRuntime.compile(document,hold.ir.program,hold.selected,catalog,{z64,maxTicks:8,diagnosticAssumptions:false,nativeLaunchInputs:l});
   assert(r.terminated,JSON.stringify({opcode:c.opcode,outcome:r.outcome,missing:r.missingInputs}));
   assert.strictEqual(r.states[1].nativeExternal.sharedRequests[c.ordinaryModeResult.context],c.ordinaryModeResult.requestId);
   assert(r.states[1].audio.some(a=>a.mode===1&&a.program===c.ordinaryModeResult.requestId));
   if(c.ordinaryModeResult.context==='B')assert(r.states[1].audio.some(a=>a.mode===2&&a.program===0&&a.flags===2));
  }
 });
 console.log(JSON.stringify({status:'pass',checks,actualColor:colorResult.outcome,actualMenu:menuResult.outcome},null,2));
})().catch(error=>{console.error(error);process.exitCode=1;});
