'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const E=path.resolve(__dirname,'..'),R=path.resolve(E,'..');
new Function('require','__dirname',fs.readFileSync(path.join(E,'tests/cutscene-framebuffer-integration.test.js'),'utf8').split('(async()=>')[0])(require,path.join(E,'tests'));
for(const f of ['cutscene-image-echo-data.js','cutscene-image-echo.js','cutscene-shared-actor-data.js','cutscene-shared-actor.js','cutscene-map-menu-data.js','cutscene-map-menu.js'])vm.runInThisContext(fs.readFileSync(path.join(E,f),'utf8'),{filename:f});
(async()=>{
 const raw=fs.readFileSync(path.join(R,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),input=require(path.join(E,'tests/fixtures/cutscene-shared-actor.json')).input,scene=catalog.getScene(input.assetId);
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog),runtime=OB64.cutsceneRuntime;
 const parent=runtime.compile(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:input,diagnosticAssumptions:false,maxTicks:80});
 assert(parent.states.at(-1).actors.length);assert(!parent.framebuffers.length);
 const child=catalog.scenes.find(x=>x.sceneId==='scene:director:01f56d8e'),childSource=await OB64.cutsceneCodec.loadSceneSource(z64,child),cp=OB64.cutsceneCodec.projectSceneDocument(child,childSource,catalog);
 const bytes=Uint8Array.of(0x80,0,0,1),cs={...child,source:{dynamicGrammar:true,terminalWithoutTrailer:true,decodedLength:4,decodedWordCount:1}},ir=OB64.cutsceneCodec.createIr(cs,bytes);
 function run(context){return runtime.compile(cp.document,ir.program,cs,catalog,{z64,maxTicks:1,diagnosticAssumptions:false,contextRuntime:context,contextTickOffset:79});}
 const rawChild=run(parent),jsonChild=run(JSON.parse(JSON.stringify(parent))),deltaChild=run(runtime.compactContextRuntime(parent));
 function actors(r){return runtime.evaluate(r,0).actors.map(a=>({id:a.id,slot:a.slot,bank:a.bank,baseX:a.baseX,baseY:a.baseY,baseZ:a.baseZ,visible:a.visible,artSourceId:a.artSourceId}));}
 const direct=runtime.evaluate(rawChild,0),serialized=runtime.evaluate(jsonChild,0),compact=runtime.evaluate(deltaChild,0);
 assert.deepStrictEqual(direct.actors,serialized.actors,'raw and serialized context Actors');
 assert.deepStrictEqual(direct.actors,compact.actors,'raw and compact context Actors');
 assert.deepStrictEqual(direct.effects,serialized.effects,'raw and serialized context effects');
 assert.deepStrictEqual(direct.effects,compact.effects,'raw and compact context effects');
 assert.strictEqual(direct.actors.length,7);const slotZero=direct.actors.find(a=>a.slot===0);assert.strictEqual(slotZero.bank,30);
 assert.strictEqual(slotZero.baseX,-34);assert.strictEqual(slotZero.baseZ,117);
 const before=JSON.stringify(parent),childBefore=JSON.stringify(runtime.evaluate(rawChild,0));
 direct.actors[0].bank=999;direct.actors[0].tint.r=3;direct.actors[0].sceneTransform.translateX=999;
 assert.strictEqual(JSON.stringify(parent),before,'child evaluation does not mutate retained parent');
 assert.strictEqual(JSON.stringify(runtime.evaluate(rawChild,0)),childBefore,'evaluations are independent mutable copies');
 assert.deepStrictEqual(runtime.evaluate(run(parent),0).actors,serialized.actors,'repeat import preserves values');
 assert.deepStrictEqual(Object.keys(parent.states.at(-1).actors[0]),['values'],'the test reaches shared schemas');
 console.log(JSON.stringify({status:'pass',states:parent.states.length,actors:serialized.actors.length,rawSerializedCompact:true,independentEvaluations:true,repeatImport:true,retainedStateBytes:parent.retainedStateBytes}));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
