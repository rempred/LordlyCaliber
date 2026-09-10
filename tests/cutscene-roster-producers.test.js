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
const fixture=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-roster-reset.json'),'utf8'));
const constructionFixture=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-roster-construction.json'),'utf8'));
(async()=>{
 const scene=catalog.directorScenes.find(s=>s.friendlyName==='Graduation Ceremony'),source=await OB64.cutsceneCodec.loadSceneSource(z64,scene);
 const document=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog).document;
 function program(words,mode=2){const bytes=new Uint8Array(words.length*4),view=new DataView(bytes.buffer);words.forEach((w,i)=>view.setUint32(i*4,w>>>0));
  const selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:bytes.length,decodedWordCount:words.length},launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:mode}}};
  return {selected,ir:OB64.cutsceneCodec.createIr(selected,bytes)};}
 function input(rows,slots=Array(28).fill(null)){return {schema:'ob64-cutscene-launch-inputs.v1',assetId:scene.assetId,invocationId:'roster-fixture',sourceIdentity:'accepted-synthetic-native-service-fixture',evidenceGrade:'Candidate',actorInputRows:{status:'known',value:rows},existingActors:{status:'known',value:{slots,otherJobsEmpty:true}},schedulerBranch:{status:'known',value:'normal'}};}
 function run(p,launch){return OB64.cutsceneRuntime.compile(document,p.ir.program,p.selected,catalog,{z64,maxTicks:2,diagnosticAssumptions:false,nativeLaunchInputs:launch});}
 const resetProgram=program([0x96,999,0x80000001]),nodeId=resetProgram.ir.program.primitives[0].id;
 function resetInput(nativeCase){nativeCase=JSON.parse(JSON.stringify(nativeCase));const e=nativeCase.export,launch=input(e.rows),rowFinalizers=e.rowFinalizers;
  const s={nodeId,occurrence:0,sceneRoot:0x100000,unitHex:e.unitHex,currentUnit:7,records:e.records,objects:e.objects,primaryRegistry:[0x300000],secondaryRegistry:[0x300100],
   releases:[{handle:99,status:'returned'}],rowFinalizers,descriptions:rowFinalizers.map(f=>({row:f.row,rowHex:f.afterRowHex,variant:0,handle:Buffer.from(f.afterRowHex,'hex').readUInt32BE(0x48)*10})),
   scratch:Object.fromEntries(['art','handle','variant','orientationA','orientationB','context'].map(k=>[k,Array(9).fill(0xA5A5A5A5)])),
   preparations:nativeCase.calls.filter(c=>c.event==='prepare').map(c=>({...c,status:'returned'})),decodes:e.decodes,random:[]};
  launch.rosterResets={status:'known',value:[s]};return launch;}
 for(const nativeCase of fixture.cases){const launch=resetInput(nativeCase),result=run(resetProgram,launch),actual=result.nativeRosterResult;
  assert(actual,'reset output');assert.strictEqual(actual.completed,true,JSON.stringify({outcome:result.outcome,missing:result.missingInputs}));
  assert.strictEqual(actual.currentUnit,nativeCase.currentUnit);
  for(const row of nativeCase.rows)assert.strictEqual(actual.rows[row.ordinal].toUpperCase(),row.rowHex,'complete native row '+row.ordinal);
  assert.strictEqual(actual.primaryRegistry.length,nativeCase.primaryRegistryCount);assert.strictEqual(actual.secondaryRegistry.length,nativeCase.secondaryRegistryCount);
  for(const p of [0x300000,0x300100])assert.strictEqual(Buffer.from(actual.objects[p],'hex').readUInt32BE(0x18),0);
  for(const row of nativeCase.rows)if(row.primary)assert.strictEqual(Buffer.from(actual.objects[row.primary],'hex').readInt16BE(0x4C),row.primaryDelay);
 }
 // Unknown unit follows successful clearing; it is neither an empty unit nor rollback.
 const unknown=resetInput(fixture.cases[0]);unknown.rosterResets.value[0].unitHex=null;
 const stopped=run(resetProgram,unknown);assert.strictEqual(stopped.outcome,'roster-reset-unit');assert.strictEqual(stopped.nativeRosterResult.currentUnit,7);
 assert.strictEqual(stopped.nativeRosterResult.rows[1],'00'.repeat(248));assert.strictEqual(stopped.nativeRosterResult.rows[0],fixture.cases[0].export.rows[0]);
 const missingScratch=resetInput(fixture.cases[0]);missingScratch.rosterResets.value[0].scratch.handle[2]=null;
 const grouped=run(resetProgram,missingScratch);assert.strictEqual(grouped.outcome,'roster-reset-scratch');assert.strictEqual(grouped.nativeRosterResult.currentUnit,7);
 assert.strictEqual(grouped.trace.filter(t=>t.kind==='reset-preparation').length,1);
 const equalScratch=resetInput(fixture.cases[0]),equalService=equalScratch.rosterResets.value[0];equalService.scratch.handle[2]=30;
 equalService.preparations[1].count=2;equalService.preparations[1].values.forEach(a=>a.push(0xA5A5A5A5));
 const equal=run(resetProgram,equalScratch);assert.strictEqual(equal.nativeRosterResult.completed,true);assert.deepStrictEqual(equal.trace.filter(t=>t.kind==='reset-preparation').map(t=>t.count),[1,2]);
 const duplicates=resetInput(fixture.cases[1]);duplicates.rosterResets.value[0].primaryRegistry=[0x300000,0x300000];
 const duplicateResult=run(resetProgram,duplicates);assert.deepStrictEqual(duplicateResult.nativeRosterResult.primaryRegistry,[0x300000]);
 const sound=resetInput(fixture.cases[0]);sound.rosterResets.value[0].decodes[0].code=17;
 const soundStop=run(resetProgram,sound);assert.strictEqual(soundStop.outcome,'roster-reset-sound');assert.strictEqual(soundStop.nativeRosterResult.currentUnit,7);
 const badLink=resetInput(fixture.cases[0]),firstFinal=badLink.rosterResets.value[0].rowFinalizers[0],firstPointer=fixture.cases[0].rows[1].primary;
 const badObject=Buffer.from(firstFinal.objects[firstPointer],'hex');badObject.writeUInt32BE(0,0x90);firstFinal.objects[firstPointer]=badObject.toString('hex');
 assert.strictEqual(run(resetProgram,badLink).outcome,'roster-reset-child');
 const delayed=resetInput(fixture.cases[0]),delayService=delayed.rosterResets.value[0],delayObject=Buffer.from(delayService.rowFinalizers[0].objects[firstPointer],'hex');
 delayObject.writeInt16BE(4,0x4C);delayObject[0x56]=250;delayObject[0x57]=2;delayObject[0x66]=10;delayObject[0x67]=252;
 delayService.rowFinalizers[0].objects[firstPointer]=delayObject.toString('hex');delayService.decodes=delayService.decodes.filter(d=>d.pointer!==firstPointer);
 const delayedResult=run(resetProgram,delayed),delayedChild=Buffer.from(delayedResult.nativeRosterResult.objects[firstPointer],'hex');
 assert.strictEqual(delayedResult.nativeRosterResult.completed,true);assert.strictEqual(delayedChild.readInt16BE(0x4C),2);assert.deepStrictEqual([delayedChild[0x56],delayedChild[0x57]],[255,0]);
 const missingDecoder=resetInput(fixture.cases[0]);missingDecoder.rosterResets.value[0].decodes=[];
 const poseStopped=run(resetProgram,missingDecoder);assert.strictEqual(poseStopped.outcome,'roster-reset-decoder');assert.strictEqual(poseStopped.nativeRosterResult.currentUnit,7);
 assert.strictEqual(Buffer.from(poseStopped.nativeRosterResult.objects[fixture.cases[0].rows[1].primary],'hex').readInt16BE(0x4A),0);
 const bypass=run(program([0x96,999,0x80000001],0),input(fixture.cases[0].export.rows));assert.strictEqual(bypass.nativeRosterResult,null);
 // Actual materializer constructors preserve native fields before unavailable State-selection inputs.
 const row=Buffer.alloc(248);row.writeUInt32BE(2,0x48);row.writeUInt32BE(10,0x4C);row.writeUInt32BE(256,0x40);row.writeUInt32BE(1,0);row.writeUInt32BE(2,4);
 const rows=Array(20).fill('00'.repeat(248));rows[7]=row.toString('hex');
 const ordinary=input(rows);ordinary.rosterConstruction={status:'known',value:{route:0,presentationByte:128,links:[1,2].map((pointer,i)=>({pointer,x:10+i,y:40+i,z:-20-i,terrainHeight:123.75,allocationSucceeded:true}))}};
 const built=run(program([0x45,-1,-1,0x80000001]),ordinary);assert.strictEqual(built.outcome,'roster-state-selection');
 for(let i=0;i<2;i++){const actor=built.states[0].actors.find(a=>a.slot===i),b=Buffer.from(actor.source.recordHex,'hex');assert.strictEqual(b[0x147],7);assert.strictEqual(b[0x149],i);assert.strictEqual(b.readInt16BE(0x138),i?60:10);assert.strictEqual(b.readFloatBE(0x120),123);}
 // Current-slot finalization can evaluate A twice and skip B entirely.
 const slots=Array(28).fill(null);for(let i=0;i<2;i++){const b=Buffer.alloc(0x150);b.writeInt32BE(i,0xE4);b.writeInt16BE(10,0x138);b[0x147]=2;slots[i]={identity:i?'B':'A',recordHex:b.toString('hex'),movementHex:null};}
 const finalRows=Array(20).fill('00'.repeat(248)),finalRow=Buffer.alloc(248);finalRow.writeUInt32BE(0x300,0x40);finalRows[2]=finalRow.toString('hex');
 const finalized=run(program([0x45,0,0,0x80000001]),input(finalRows,slots));
 assert.deepStrictEqual(finalized.trace.filter(t=>t.kind==='roster-finalizer-visit').map(t=>t.actorIdentity),['A','A']);
 assert.strictEqual(finalized.states[0].actors.find(a=>a.slot===0).id,'B');assert.strictEqual(finalized.states[0].actors.find(a=>a.slot===1).id,'A');
 for(const example of constructionFixture.constructors){const info=example.input,rows=Array(20).fill('00'.repeat(248)),slots=Array(28).fill(null),row=Buffer.alloc(248);
  for(const slot of info.occupiedSlots){const b=Buffer.alloc(336);b.writeInt32BE(slot,0xE4);b[0x147]=19;slots[slot]={identity:'occupied-'+slot,recordHex:b.toString('hex'),movementHex:null};}
  row.writeUInt32BE(info.sourceArt,0x48);row.writeUInt32BE(info.context,0x4C);row.writeUInt32BE(256,0x40);
  for(let i=0;i<3;i++)if(info.linkedMask&(1<<i))row.writeUInt32BE(0x300000+i*256,4*i);rows[7]=row.toString('hex');
  if(info.variant){const prior=Buffer.alloc(248);prior.writeUInt32BE(135,0x48);rows[0]=prior.toString('hex');}
  const launch=input(rows,slots);launch.rosterConstruction={status:'known',value:{route:0,presentationByte:2,links:[0,1,2].map(i=>({pointer:0x300000+i*256,x:10+i,y:40+i,z:-20-i,terrainHeight:123.75,allocationSucceeded:true}))}};
  const result=run(program([0x45,-1,-1,0x80000001]),launch),constructed=result.trace.filter(t=>t.kind==='roster-construction'&&t.sourceRow===7);
  assert.strictEqual(constructed.length,example.records.length);
  for(const record of example.records){const found=constructed.find(t=>t.linkedOrdinal===record.linkedOrdinal),b=Buffer.from(found.recordHex,'hex');
   assert.strictEqual(found.slot,record.slot);assert.strictEqual(b[0x147],record.sourceRow);assert.strictEqual(b.readUInt32BE(0xE8),record.sourceArt);assert.strictEqual(b.readUInt32BE(0xEC),record.context);assert.strictEqual(b.readUInt16BE(0x138),record.state);assert.deepStrictEqual([0x11C,0x120,0x124].map(at=>b.readFloatBE(at)),record.position);}
 }
 for(const example of constructionFixture.finalizers){const rows=Array(20).fill('00'.repeat(248)),slots=example.before.map((identity,slot)=>{
   if(!identity)return null;const info=example.actors[identity],b=Buffer.alloc(336);b.writeInt32BE(slot,0xE4);b.writeInt16BE(info.state,0x138);b[0x147]=info.row;return {identity,recordHex:b.toString('hex'),movementHex:null};});
  for(const [ordinal,info] of Object.entries(example.rows)){const b=Buffer.alloc(248);b.writeUInt32BE(info.flags,0x40);b.writeUInt32BE(info.context,0x4C);rows[ordinal]=b.toString('hex');}
  const result=run(program([0x45,0,0,0x80000001]),input(rows,slots)),after=Array(28).fill(null);result.states[0].actors.forEach(a=>after[a.slot]=a.id);assert.deepStrictEqual(after,example.after);
  assert.deepStrictEqual(result.trace.filter(t=>t.kind==='roster-finalizer-visit').map(t=>t.actorIdentity),example.evaluatedActors);
 }
 for(const expected of constructionFixture.resetRows.examples['mixed-members-preserved-row'].produced){
  const launch=resetInput(fixture.cases[1]),service=launch.rosterResets.value[0],unit=Buffer.alloc(25),rows=Array(20).fill('00'.repeat(248));
  for(let i=0;i<expected.destinationRow;i++){const b=Buffer.alloc(248);b.writeUInt32BE(44,0x48);rows[i]=b.toString('hex');}
  unit[2+expected.sourceMemberSlot]=expected.memberId;unit[7+expected.sourceMemberSlot]=expected.formationByte;
  launch.actorInputRows.value=rows;service.unitHex=unit.toString('hex');service.records=constructionFixture.resetRows.sourceRecords;service.releases=[];service.rowFinalizers=[];service.specialOverrides={101:{halfword:51,flags:4}};
  const result=run(resetProgram,launch);assert.strictEqual(result.outcome,'roster-reset-finalizer');
  assert.strictEqual(result.trace.find(t=>t.kind==='reset-row-initialized').rowHex.toUpperCase(),expected.preFinalizerRowHex);
 }
 console.log(JSON.stringify({status:'pass',command:process.argv,node:process.version,sourceHashes,fixtureSha256:hash(fs.readFileSync(path.join(__dirname,'fixtures/cutscene-roster-reset.json'))),constructorExamples:88,correctedFinalizerExamples:3,deployedFieldExamples:3,checks:['native complete reset and empty-unit rows','cleanup preservation and first duplicate removal','partial missing unit','residual scratch after prior preparation and native total-count lookahead','cursor preincrement before unavailable decoder','child parent-link rejection','external sound boundary before selector publication','positive-delay material saturation','mode bypass','actual ordinary constructors','mutable current-slot finalizer']},null,2));
})().catch(error=>{console.error(error);process.exitCode=1;});
