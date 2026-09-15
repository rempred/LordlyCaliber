'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-dialogue-lifecycle.json');
global.window=global;vm.runInThisContext('var OB64=window.OB64={};');
for(const f of ['data.js','parsers.js','art.js','animation-corpus-data.js','animation-art.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-dialogue-data.js','cutscene-dialogue-lifecycle-data.js','cutscene-dialogue.js','cutscene-dialogue-lifecycle.js','cutscene-runtime.js','cutscene-preview.js','cutscene-renderer.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',f),'utf8'),{filename:f});
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),rom=new Uint8Array(raw.length);
for(let i=0;i<raw.length;i+=2){rom[i]=raw[i+1];rom[i+1]=raw[i];}
const initial=fixture.secondPath.input.externalProducers.value.initialDialogue;
const drain=g=>{let r;do{r=g.next();}while(!r.done);return r.value;};
const engine=(input=structuredClone(initial),bytes=rom)=>new OB64.cutsceneDialogue.Engine(input,bytes);
const hex=(e,a,n)=>Buffer.from(Array.from({length:n},(_,i)=>e.machine.get(a+i,1))).toString('hex');
const invalid=structuredClone(initial);invalid.lifecycle=null;assert.throws(()=>engine(invalid),x=>x.code==='dialogue-lifecycle-input');
const changedCode=rom.slice();changedCode[0xf9ff8]^=1;assert.throws(()=>engine(structuredClone(initial),changedCode),x=>x.code==='dialogue-image-identity');
const e=engine(),address=e.lifecycle.archive(0);assert.equal(hex(e,address,585),fixture.archive0Hex);assert.equal(e.lifecycle.archive(0),address);assert.equal(e.lifecycle.used,592);
assert.throws(()=>e.lifecycle.archive(503),x=>x.code==='dialogue-archive-input');
assert.throws(()=>e.lifecycle.archive(0xffffffff),x=>x.code==='dialogue-archive-input');
const small=structuredClone(initial);small.lifecycle.archiveArena.byteLength=16;const exhausted=engine(small);assert.throws(()=>exhausted.lifecycle.archive(0),x=>x.code==='dialogue-archive-exhaustion');assert.equal(exhausted.lifecycle.used,0);assert.equal(exhausted.lifecycle.archives.size,0);
const overlap=structuredClone(initial);overlap.lifecycle.archiveArena.address=overlap.payloadStorage.address;assert.throws(()=>engine(overlap),x=>x.code==='dialogue-lifecycle-input');
const corrupt=rom.slice();corrupt[0x1fd0216+21]^=1;const bad=engine(structuredClone(initial),corrupt);assert.throws(()=>bad.lifecycle.archive(0),x=>x.code==='dialogue-archive-input');assert.equal(bad.lifecycle.used,0);
// Changing the ROM selector table changes resolution; no catalog outcome is used.
const remap=rom.slice(),rv=new DataView(remap.buffer);rv.setUint32(0x1fcfa36,0x01a3c0f2);const remapInput=structuredClone(initial);remapInput.lifecycle.archiveArena.byteLength=4096;remapInput.memory.find(r=>r.address===0x80370000).hex+='00'.repeat(3072);const mapped=engine(remapInput,remap);const mappedAddress=mapped.lifecycle.archive(0);assert.notEqual(hex(mapped,mappedAddress,585),fixture.archive0Hex);
const stale=engine();stale.machine.r[4]=0x80380000;assert.throws(()=>stale.lifecycle.helper(0x800712c4),x=>x.code==='dialogue-cleanup-owner');
const full=engine();for(let i=0;i<6;i++)full.machine.put(0x800e82c8+i*168,0x8000,2);assert.throws(()=>full.lifecycle.create(fixture.constructorControls[0].words,'full'),x=>x.code==='dialogue-resource-exhaustion');
const unsupported=fixture.constructorControls[0].words.slice();unsupported[11]=0xffffffff;assert.throws(()=>engine().lifecycle.create(unsupported,'unsupported'),x=>x.code==='dialogue-constructor-input');
(async()=>{
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.getScene(fixture.secondPath.input.assetId),source=await OB64.cutsceneCodec.loadSceneSource(rom,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog),at=p.program.primitives.findIndex(n=>n.id==='node:01F3E836:w007F');
 assert.deepStrictEqual(p.program.primitives.slice(at,at+20).flatMap(n=>n.rawWords),fixture.secondPath.words.slice(0,-1));
 function run(words,input,context,maxTicks=220){const data=OB64.cutsceneCodec.wordsToBytes(words),selected={...scene,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:data.length,decodedWordCount:words.length},launchProfile:{...scene.launchProfile,directorMode:{...scene.launchProfile.directorMode,value:0}}};return OB64.cutsceneRuntime.compile(p.document,OB64.cutsceneCodec.createIr(selected,data).program,selected,catalog,{z64:rom,maxTicks,diagnosticAssumptions:false,nativeLaunchInputs:input,contextRuntime:context,contextTickOffset:0});}
 function record(state){const d=state.nativeExternal.dialogue,a=0x800e82c8+168,r=d.memory.find(r=>r.address<=a&&r.address+r.hex.length/2>=a+168);return r.hex.slice((a-r.address)*2,(a-r.address+168)*2);}
 const second=fixture.secondPath,result=run(second.words,second.input,second.context);assert.equal(result.outcome,'modeled-termination',JSON.stringify(result.unresolvedQuery));assert.equal(result.states.length,fixture.expected.length);
 for(const row of fixture.expected){assert.equal(record(result.states[row.tick]),row.recordHex,'record '+row.tick);assert.equal(result.states[row.tick].nativeExternal.dialogue.owners[1]?.payloadHex??null,row.payloadHex,'payload '+row.tick);}
 assert.equal(result.states[fixture.releasedTick].nativeExternal.dialogue.owners[1],null);assert(result.states[14].dialogue[0].payload.nativeDialogue.text.includes("You're leaving tomorrow?"));
 for(const row of fixture.constructorControls){const input=structuredClone(second.input),context=structuredClone(second.context);input.externalProducers.value.events=[];input.externalProducers.value.throughTick=0;if(row.point){const a=context.states[0].actors[0];[a.x,a.y,a.z]=row.point;[a.baseX,a.baseY,a.baseZ]=row.point;}
  const actual=run([...row.words,0x80000001],input,context,1);assert.equal(actual.outcome,'modeled-termination',JSON.stringify(actual.unresolvedQuery));assert.equal(record(actual.states[0]),row.recordHex,row.name);
 }
 assert(!second.input.externalProducers.value.dialogueCreates);assert(second.input.externalProducers.value.events.templates.every(e=>!e.storage&&['helpers','releaseHelpers'].every(k=>(e[k]||[]).every(h=>![0x8007938c,0x800712c4].includes(h.address)))));
 console.log(JSON.stringify({status:'pass',retailUpdates:result.states.length,nativeRecords:fixture.expected.length,nativePayloads:fixture.expected.filter(r=>r.payloadHex!==null).length,releasedTick:fixture.releasedTick,changedNativeConstructors:fixture.constructorControls.length,archiveBytes:585,changedRomSelector:true,cache:true,exhaustion:true,crcRejection:true,ownership:true}));
})().catch(e=>{console.error(e);process.exitCode=1;});

