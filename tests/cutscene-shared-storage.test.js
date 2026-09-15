'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-shared-storage.json');
global.window=global;vm.runInThisContext('var OB64=window.OB64={};');
for(const file of ['data.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-dialogue-data.js','cutscene-dialogue.js','cutscene-runtime.js','cutscene-preview.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',file),'utf8'),{filename:file});
const drain=g=>{let r;do{r=g.next();}while(!r.done);return r.value;};
const ref=fixture.nativeStorage,Machine=OB64.cutsceneDialogue.Machine,Storage=OB64.cutsceneDialogue.PayloadStorage;
const m=new Machine({},[{address:ref.arena.address,bytes:new Uint8Array(ref.arena.byteLength).fill(ref.arena.initialByte),writable:true},
 {address:ref.source,bytes:new Uint8Array(4096),writable:true},{address:ref.destination,bytes:new Uint8Array(4096),writable:true}]);
const config={kind:'preview-arena-v1',address:ref.arena.address,byteLength:ref.arena.byteLength},store=new Storage(m,config);
const memoryHex=(address,length)=>Buffer.from(Array.from({length},(_,i)=>m.get(address+i,1))).toString('hex');
for(const row of ref.rows){
 if(row.action==='save'){
  for(let i=0;i<row.length;i++)m.put(ref.source+i,(i*37+row.seed)&255,1);
  const handle=store.save(row.owner,0xa0,ref.source,row.length);assert.equal(handle,row.handle);
  if(handle)assert.equal(memoryHex(handle,row.length+6),row.headerAndBodyHex);
 }else{store.restore(row.owner,row.handle,ref.destination);assert.equal(memoryHex(ref.destination,row.bodyHex.length/2),row.bodyHex);}
}
assert.equal(store.leases.length,0);
const first=store.save('changed',0xa0,ref.source,17),old=memoryHex(first+6,17);
store.restore('changed',first,ref.destination);m.put(ref.source,0x42,1);
assert.equal(store.save('changed',0xa0,ref.source,17),first);assert.notEqual(memoryHex(first+6,17),old);assert.equal(m.get(first+6,1),0x42);
assert.throws(()=>store.save('changed',0xa0,ref.source,17),e=>e.code==='dialogue-payload-owner');
assert.throws(()=>store.restore('other',first,ref.destination),e=>e.code==='dialogue-payload-identity');
const leaseBefore=JSON.stringify(store.leases),bytesBefore=memoryHex(config.address,config.byteLength);
assert.throws(()=>store.save('too-large',0xa0,ref.source,4090),e=>e.code==='dialogue-storage-exhaustion');
assert.equal(JSON.stringify(store.leases),leaseBefore);assert.equal(memoryHex(config.address,config.byteLength),bytesBefore);
assert.throws(()=>store.reserve('collision',first,17),e=>e.code==='dialogue-payload-owner');
assert.throws(()=>store.save('compressed',1,ref.source,17),e=>e.code==='dialogue-payload-mode');
assert.throws(()=>new Storage(m,{...config,address:config.address+1}),e=>e.code==='dialogue-storage-arena');
assert.throws(()=>new Storage(m,{...config,address:0x800e82c8}),e=>e.code==='dialogue-storage-arena');
assert.throws(()=>new Storage(m,{...config,byteLength:65552}),e=>e.code==='dialogue-storage-arena');
assert.throws(()=>new Storage(m,{...config,address:0x80340000}),e=>e.code==='dialogue-reached-memory');
store.release('changed');assert.equal(store.save('reuse',0xa0,ref.source,17),first);
(async()=>{
 const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),rom=new Uint8Array(raw.length);
 for(let i=0;i<raw.length;i+=2){rom[i]=raw[i+1];rom[i+1]=raw[i];}
 const captured=structuredClone(require('./fixtures/cutscene-native-dialogue.json').initialDialogue);
 captured.payloadStorage={kind:'preview-arena-v1',address:0x80630000,byteLength:1152};captured.memory.find(r=>r.address===0x80630000).hex+='0000';
 const engine=new OB64.cutsceneDialogue.Engine(captured,rom);assert.equal(engine.payloadStorage.leases.length,1);
 const event={service:'callback',slot:0,ownerId:'text-owner',eligible:true,helpers:[],controller:{actionMask:0,directionMask:0,dummyMask:0,historyMask:0,queueHead:0}};
 drain(engine.service(event));assert.equal(engine.payloadStorage.leases.length,1);assert.equal(engine.snapshot().owners[0].payloadHex.slice(120,122),'04');
 for(const eligible of [true,false])assert.throws(()=>drain(engine.service({...event,eligible,storage:{saveReturned:true,saveHandle:0x80630000}})),e=>e.code==='dialogue-storage-outcome');
 const outside=structuredClone(captured);outside.payloadStorage.address+=16;outside.payloadStorage.byteLength=1136;
 assert.throws(()=>new OB64.cutsceneDialogue.Engine(outside,rom),e=>e.code==='dialogue-payload-owner');
 const second=fixture.secondPath,catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.getScene(second.input.assetId);
 const source=await OB64.cutsceneCodec.loadSceneSource(rom,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
 const at=p.program.primitives.findIndex(n=>n.id==='node:01F3E836:w007F');assert.deepStrictEqual(p.program.primitives.slice(at,at+6).flatMap(n=>n.rawWords),second.words.slice(0,-1));
 const data=OB64.cutsceneCodec.wordsToBytes(second.words),selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:data.length,decodedWordCount:second.words.length},launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:2}}};
 const program=OB64.cutsceneCodec.createIr(selected,data).program;
 const result=OB64.cutsceneRuntime.compile(p.document,program,selected,catalog,{z64:rom,maxTicks:15,diagnosticAssumptions:false,nativeLaunchInputs:second.input});
 assert.equal(result.outcome,'modeled-termination');assert.equal(result.states.length,15);
 for(const row of second.expected){const d=result.states[row.tick].nativeExternal.dialogue,a=0x800e82c8+168,r=d.memory.find(r=>r.address<=a&&r.address+r.hex.length/2>=a+168);assert.equal(r.hex.slice((a-r.address)*2,(a-r.address+168)*2),row.recordHex);assert.equal(d.owners[1].payloadHex,row.payloadHex);}
 assert(result.states.at(-1).dialogue[0].payload.nativeDialogue.text.includes("You're leaving tomorrow?"));
 assert(second.input.externalProducers.value.events.every(e=>e.storage===undefined));
 const exhausted=structuredClone(second.input);exhausted.externalProducers.value.initialDialogue.payloadStorage.byteLength=16;
 const blocked=OB64.cutsceneRuntime.compile(p.document,program,selected,catalog,{z64:rom,maxTicks:15,diagnosticAssumptions:false,nativeLaunchInputs:exhausted});assert.equal(blocked.unresolvedQuery.code,'dialogue-storage-exhaustion');
 console.log(JSON.stringify({status:'pass',nativeWrapperOperations:ref.rows.length,changedPayload:true,firstFitReuse:true,exhaustionAtomic:true,capturedPayloadAdoption:true,secondRetailPathUpdates:15,secondNativePayloads:second.expected.length,secondNativeRecords:second.expected.length,scope:'Retail command slice plus explicit stop; declared constructor/archive context, not whole-scene playback'}));
})().catch(e=>{console.error(e);process.exitCode=1;});
