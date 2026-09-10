'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
global.window=global;vm.runInThisContext('var OB64 = window.OB64 = {};');
const sourceHashes={};
for(const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-runtime.js']) {
 const text=fs.readFileSync(path.join(root,'editor',file),'utf8');sourceHashes[file]=hash(text);vm.runInThisContext(text,{filename:file});
}
const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
const z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
const fixturePath=path.join(__dirname,'fixtures/cutscene-materializer-setup.json'),fixture=JSON.parse(fs.readFileSync(fixturePath,'utf8'));
(async()=>{
 const scene=catalog.directorScenes.find(s=>s.friendlyName==='Graduation Ceremony'),source=await OB64.cutsceneCodec.loadSceneSource(z64,scene);
 const document=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog).document;
 const words=[0x45,-1,-1,0x80000001],bytes=new Uint8Array(words.length*4),view=new DataView(bytes.buffer);words.forEach((w,i)=>view.setUint32(i*4,w>>>0));
 const selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:bytes.length,decodedWordCount:words.length},launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:2}}};
 const ir=OB64.cutsceneCodec.createIr(selected,bytes),nodeId=ir.program.primitives[0].id;
 function input(f){f=JSON.parse(JSON.stringify(f));const registry=new Map();
  for(const p of f.services.preparations){const [art,context,flagB,flagA]=p.values,key=[art,context,flagA,flagB].join(':');
   if(!registry.has(key))registry.set(key,{sourceArt:art|0,ownerContext:context|0,flagA,flagB:(flagB<<16)>>16,handle:1,programs:[-1,0,10,36,38,50,60,86,88].map(state=>({state,programHex:'00'}))});}
  return {schema:'ob64-cutscene-launch-inputs.v1',assetId:scene.assetId,invocationId:f.name,sourceIdentity:fixture.romZ64Sha256+':'+fixture.generatorSha256,evidenceGrade:'Candidate',
   actorInputRows:{status:'known',value:f.rows},existingActors:{status:'known',value:{slots:f.slots,otherJobsEmpty:true}},schedulerBranch:{status:'known',value:'normal'},
   rosterConstruction:{status:'known',value:f.construction},rosterStateServices:{status:'known',value:[{nodeId,occurrence:0,...f.services}]},poseRegistry:{status:'known',value:{alternate:[...registry.values()],ordinary:[]}}};}
 function run(launch,opcode=0x45){const b=bytes.slice();new DataView(b.buffer).setUint32(0,opcode);const current=OB64.cutsceneCodec.createIr(selected,b);launch.rosterStateServices.value[0].nodeId=current.program.primitives[0].id;return OB64.cutsceneRuntime.compile(document,current.program,selected,catalog,{z64,maxTicks:2,diagnosticAssumptions:false,nativeLaunchInputs:launch});}
 const checked=[];
 for(const f of fixture.cases){const result=run(input(f),f.opcode);assert(!result.outcome.startsWith('roster-'),f.name+': '+result.outcome+' '+result.missingInputs.join('; '));
  for(const expected of f.after){const actual=result.states[0].actors.find(a=>a.slot===expected.slot);assert(actual,f.name+' missing slot '+expected.slot);
   assert.strictEqual(actual.source.recordHex.toLowerCase(),expected.recordHex.toLowerCase(),f.name+' full native record at slot '+expected.slot);
   if(expected.identity.startsWith('existing'))assert.strictEqual(actual.id,expected.identity);}
  assert.deepStrictEqual(result.trace.filter(t=>t.kind==='roster-setup-entry').map(t=>[t.slot,...t.args.map(v=>v>>>0)]),f.setupArgs,f.name+' original recursive arguments');
  assert.strictEqual(result.trace.filter(t=>t.kind==='roster-state-preparation').length,f.services.preparations.length);
  checked.push({name:f.name,records:f.after.length,setupCalls:f.setupArgs.length,markerCalls:f.services.markerLookups.length});
 }
 const noAppearance=input(fixture.cases[0]);noAppearance.rosterStateServices.value[0].appearances=[];
 const seeded=run(noAppearance);assert.strictEqual(seeded.outcome,'roster-state-appearance');const seededRecord=Buffer.from(seeded.states[0].actors.find(a=>a.slot===2).source.recordHex,'hex');
 assert.strictEqual(seededRecord.readInt32BE(0xF0),-1);assert.strictEqual(seededRecord.readInt32BE(0xF4),0);assert.strictEqual(seededRecord[0x13D],1);
 const noPreparation=input(fixture.cases[0]);noPreparation.rosterStateServices.value[0].preparations[0].status='unavailable';assert.strictEqual(run(noPreparation).outcome,'roster-state-preparation');
 const noRegistry=input(fixture.cases[0]);noRegistry.poseRegistry.value.alternate=[];assert.strictEqual(run(noRegistry).outcome,'alternate-pose-registration');
 const noMarker=input(fixture.cases[1]);delete noMarker.rosterStateServices.value[0].markerLookups;
 const exhausted=run(noMarker);assert.strictEqual(exhausted.outcome,'roster-marker-termination');assert.strictEqual(exhausted.trace.filter(t=>t.kind==='roster-marker-lookup').length,256);
 const incompleteMarker=input(fixture.cases[1]);incompleteMarker.rosterStateServices.value[0].markerLookups=[];assert.strictEqual(run(incompleteMarker).outcome,'roster-marker-input');
 const wrongEffect=input(fixture.cases[6]);wrongEffect.rosterStateServices.value[0].poseEffects[0].beforeRecordHex='00'.repeat(336);assert.strictEqual(run(wrongEffect).outcome,'roster-state-effects');
 const physicalMarker=input(fixture.cases[1]);delete physicalMarker.rosterStateServices.value[0].markerLookups;
 physicalMarker.poseRegistry.value.alternate.find(p=>p.sourceArt===83).programs.find(p=>p.state===36).programHex='0401110403020dff630400';
 const physical=run(physicalMarker),physicalActor=Buffer.from(physical.states[0].actors.find(a=>a.slot===2).source.recordHex,'hex');
 assert(!physical.outcome.startsWith('roster-'));assert.strictEqual(physicalActor.readInt32BE(0xF8),17);assert.strictEqual(physicalActor.readInt32BE(0xF4),0);assert.strictEqual(physicalActor.readInt32BE(0xF0),1);assert.strictEqual(physicalActor[0],255);
 console.log(JSON.stringify({status:'pass',command:process.argv,node:process.version,sourceHashes,fixtureSha256:hash(fs.readFileSync(fixturePath)),checked,negativeCases:['appearance unavailable after seed','preparation nonreturn','missing pose registry','exhausted marker zero','missing marker service','mismatched effect preimage']},null,2));
})().catch(error=>{console.error(error);process.exitCode=1;});
