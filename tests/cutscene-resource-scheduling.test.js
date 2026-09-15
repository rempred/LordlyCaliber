'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-resource-scheduling.json');
global.window=global;vm.runInThisContext('var OB64=window.OB64={};');
for(const f of ['data.js','parsers.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-dialogue-data.js','cutscene-dialogue-lifecycle-data.js','cutscene-dialogue.js','cutscene-dialogue-lifecycle.js','cutscene-resource-scheduler-data.js','cutscene-resource-scheduler.js','cutscene-runtime.js','cutscene-preview.js','cutscene-renderer.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',f),'utf8'),{filename:f});
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),rom=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){rom[i]=raw[i+1];rom[i+1]=raw[i];}
const drain=g=>{let r;do{r=g.next();}while(!r.done);return r.value;},clone=structuredClone;
// Compare the shared circular initializer with separately executed native manager controls.
for(const c of fixture.controls){const slots=Array.from({length:6},()=>({flags:0,initialize:0,callback:0}));
 for(const [slot,ready,priority,init,callback]of c.rows)slots[slot]={flags:0x8000|(ready?0x2000:0),initialize:init?1:0,callback:callback?1:0};
 const seen=[];drain(OB64.cutsceneResourceScheduler.initializePlan(i=>slots[i],function*(i){seen.push(['initialize',i]);slots[i].flags|=0x2000;if(c.creation?.[0]==='initialize'&&c.creation[1]===i)slots[c.creation[2]]={flags:0x8000,initialize:1,callback:1};}));
 assert.deepStrictEqual(seen,c.trace.filter(r=>r[0]==='initialize'),c.name);
 const fake=Object.create(OB64.cutsceneResourceScheduler.Scheduler.prototype);fake.budget=6;fake.read=i=>slots[i];fake.machine={put(){}};fake.dialogue=function*(i){seen.push(['callback',i]);if(c.creation?.[0]==='callback'&&c.creation[1]===i)slots[c.creation[2]]={flags:0x8000,initialize:1,callback:1};};
 for(let i=0;i<6;i++)drain(fake.callback(i));assert.deepStrictEqual(seen,c.trace.filter(r=>['initialize','callback'].includes(r[0])),c.name);
}
const original=fixture.paths[0].run.input.externalProducers.value;
function scheduler(mut=()=>{},bytes=rom){const e=new OB64.cutsceneDialogue.Engine(clone(original.initialDialogue),rom),p=clone(original.resourceSchedule);mut(e,p);return new OB64.cutsceneResourceScheduler.Scheduler(e,p,bytes);}
const badRom=rom.slice();badRom[0x69d8]^=1;assert.throws(()=>scheduler(()=>{},badRom),e=>e.code==='dialogue-scheduler-image');
assert.throws(()=>scheduler((e,p)=>p.helperOutcomes.push({address:0x8007938c,args:[],result:0,writes:[],preservesOtherRegisters:true})),e=>e.code==='dialogue-scheduler-input');
assert.throws(()=>scheduler((e,p)=>p.controller.changes[0].pass=1),e=>e.code==='dialogue-scheduler-input');
for(const [name,mut,code]of [
 ['uninitialized Director',e=>e.machine.put(0x800e82c8,0xc000,2),'dialogue-scheduler-callback'],
 ['null Director callback',e=>e.machine.put(0x800e82c8+20,0),'dialogue-scheduler-director'],
 ['queue overflow',e=>e.machine.put(0x800c49d0,7,2),'dialogue-scheduler-queue'],
 ['pending bulk cleanup',e=>e.machine.put(0x800c4c26,0,2),'dialogue-scheduler-cleanup']
]){const s=scheduler(mut);assert.throws(()=>drain(s.before(0)),e=>e.code===code,name);}
const expired=scheduler();assert.throws(()=>drain(expired.before(220)),e=>e.code==='dialogue-controller-history');
const unused=scheduler();assert.throws(()=>drain(unused.after(true)),e=>e.code==='dialogue-helper-outcome');
const empty=scheduler((e,p)=>p.helperOutcomes=[]);assert.throws(()=>empty.machine.sharedOutcome(0x800934b0),e=>e.code==='dialogue-helper-outcome');
(async()=>{
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.getScene(fixture.paths[0].run.input.assetId),source=await OB64.cutsceneCodec.loadSceneSource(rom,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
 function run(row,input=row.input){const bytes=OB64.cutsceneCodec.wordsToBytes(row.words),selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:bytes.length,decodedWordCount:row.words.length},launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:0}}};return OB64.cutsceneRuntime.compile(p.document,OB64.cutsceneCodec.createIr(selected,bytes).program,selected,catalog,{z64:rom,maxTicks:220,diagnosticAssumptions:false,nativeLaunchInputs:input,contextRuntime:row.context,contextTickOffset:0});}
 for(const f of fixture.paths){const before=JSON.stringify(f.run.input),r=run(f.run);assert.equal(r.outcome,'modeled-termination');assert.equal(r.states.length,f.expected.length);assert(r.clockUnit.includes('resource passes'));assert.equal(JSON.stringify(f.run.input),before);assert.equal(f.run.input.externalProducers.value.events.length,0);
  for(const n of f.expected){const d=r.states[n.tick].nativeExternal.dialogue,a=0x800e82c8+168*f.slot,region=d.memory.find(x=>x.address<=a&&x.address+x.hex.length/2>=a+168);assert.equal(region.hex.slice((a-region.address)*2,(a-region.address+168)*2),n.recordHex,'record '+n.tick);assert.equal(d.owners[f.slot]?.payloadHex??null,n.payloadHex,'payload '+n.tick);}
  assert.equal(r.states[f.releasedTick].nativeExternal.dialogue.owners[f.slot],null);
 }
 const stale=clone(fixture.paths[0].run.input);stale.externalProducers.value.events=[{kind:'dialogue',service:'priority',ownerId:'resource-pool',eligible:true,tick:0,phase:'before',helpers:[]}];assert.throws(()=>run(fixture.paths[0].run,stale),e=>e.code==='launch-input');
 console.log(JSON.stringify({status:'pass',nativeManagerControls:fixture.controls.length,retailPaths:fixture.paths.map(f=>({directorSlot:f.run.input.externalProducers.value.resourceSchedule.directorSlot,passes:f.expected.length,release:f.releasedTick})),recordedServiceEvents:0,eligibilityAndBounds:true,unsupportedLaunchDiagnostic:true}));
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
