// Produce compatibility input through the shipping browser exporter, not a
// hand-written approximation of its arena format. Input ROM remains untouched.
const fs=require('fs'),path=require('path'),vm=require('vm'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['parsers.js','repack.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js',
 'cutscene-director.js','cutscene-codec.js','cutscene-authored.js','cutscene-project.js',
 'cutscene-event-rows-data.js','cutscene-provenance-data.js','cutscene-events.js','cutscene-export.js'])
 vm.runInThisContext(fs.readFileSync(path.join(root,f),'utf8'),{filename:f});
(async()=>{
 const raw=fs.readFileSync(process.argv[2]),rom=raw[0]===0x37?Uint8Array.from(raw,(_,i)=>raw[i^1]):Uint8Array.from(raw);
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
 const id='fedcba9876543210fedcba9876543210';
 const made=await OB64.cutsceneAuthored.create(rom,catalog,{authoredId:id,name:'Browser ROM scene',selectorRow:319});
 catalog.registerAuthoredScene(made.scene);
 const document=OB64.cutsceneCodec.projectSceneDocument(made.scene,made.source,catalog).document;
 const bindings=[{launchSiteId:'event-director:1:b0032',target:{kind:'authored',authoredId:id}}];
 const state={catalog,histories:{},sourceByAssetId:{},originalSerialized:{},romEventBindings:[],eventBindings:bindings,
  authoredDiscovery:{status:'ready'}};
 const source={z64:rom,layout:{id:'us-rev0'},cutsceneStudio:state},candidate={z64:rom.slice()};
 const plan=await OB64.cutsceneExport.prepare(source,candidate,{hashBytes:bytes=>crypto.createHash('sha256').update(bytes).digest('hex')});
 OB64.cutsceneExport.apply(candidate,plan);OB64.cutsceneExport.validateApplied(candidate,plan);
 fs.writeFileSync(process.argv[3],candidate.z64);
 fs.writeFileSync(process.argv[4],JSON.stringify({id,name:made.scene.authored.name,selector:319,
  project:{format:'ob64-cutscene-project',schemaVersion:2,sourceRevision:'us-rev0',scenes:[],eventBindings:bindings,
   authoredScenes:[{authoredId:id,name:made.scene.authored.name,selectorRow:319,sceneClass:5,
    templateSceneId:made.scene.authored.templateSceneId,initialStreamHex:Buffer.from(made.source.decodedBytes).toString('hex'),document,view:null}]}}));
})().catch(e=>{console.error(e);process.exitCode=1;});
