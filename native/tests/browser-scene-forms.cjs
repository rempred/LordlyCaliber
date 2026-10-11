const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['parsers.js','repack.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js','cutscene-authored.js'])
 vm.runInThisContext(fs.readFileSync(path.join(root,f),'utf8'),{filename:f});
(async()=>{
 const raw=fs.readFileSync(process.argv[2]),rom=raw[0]===0x37?Uint8Array.from(raw,(_,i)=>raw[i^1]):Uint8Array.from(raw);
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),wipes=[],scenes=[],expected={};
 const selected=['rom-custom-lz:01F3E836','rom-director:01F3EC88','rom-director:01F450D0'];
 for(const scene of catalog.scenes.filter(s=>s.engine==='director')) {
  const source=await OB64.cutsceneCodec.loadSceneSource(rom,scene,{allowModified:true});
  const result=OB64.cutsceneAuthored.wipeStream(source.decodedBytes,source.nodeDefinitions);
  wipes.push({id:scene.sceneId,source:Buffer.from(source.decodedBytes).toString('hex'),expected:result.status==='ok'?Buffer.from(result.bytes).toString('hex'):null});
  if(!selected.includes(scene.assetId))continue;
  if(result.status!=='ok')throw Error(result.reason);
  const adapted=OB64.cutsceneCodec.sceneForBytes(Object.assign({},scene,{actors:[],actorCount:0,recoveredActorEvents:[],recoveredMediaRequests:[],recoveredNativeSpriteEffects:[],dialogueAssociations:[],audioAssociations:[],reviewedTimelineOverlay:null,wiped:{pending:true,exitKind:result.exitKind,sceneClass:result.sceneClass,setupWordCount:result.setupWordCount,exitStartWord:result.exitStartWord}}),result.bytes);
  const wipedSource=await OB64.cutsceneCodec.authoredSceneSource(adapted,result.bytes,{rom});
  const document=OB64.cutsceneCodec.projectSceneDocument(adapted,wipedSource,catalog).document;
  scenes.push({sceneId:scene.sceneId,assetId:scene.assetId,storageId:scene.storageId,document,view:null,wiped:true});
  expected[scene.sceneId]=Buffer.from(OB64.cutsceneCodec.compileSceneDocument(adapted,wipedSource,document).decodedBytes).toString('hex');
 }
 const id='0123456789abcdef0123456789abcdef';
 const made=await OB64.cutsceneAuthored.create(rom,catalog,{authoredId:id,name:'Imported new scene',selectorRow:319});
 const document=OB64.cutsceneCodec.projectSceneDocument(made.scene,made.source,catalog).document;
 const initial=Buffer.from(made.source.decodedBytes).toString('hex');
 const project={format:'ob64-cutscene-project',schemaVersion:2,sourceRevision:'us-rev0',scenes,
  authoredScenes:[{authoredId:id,name:made.scene.authored.name,selectorRow:319,sceneClass:5,templateSceneId:made.scene.authored.templateSceneId,initialStreamHex:initial,document,view:null}],
  eventBindings:[{launchSiteId:'event-director:1:b0032',target:{kind:'authored',authoredId:id}}]};
 fs.writeFileSync(process.argv[3],JSON.stringify({project,wipes,expected,authored:Buffer.from(OB64.cutsceneCodec.compileSceneDocument(made.scene,made.source,document).decodedBytes).toString('hex')}));
})().catch(e=>{console.error(e);process.exitCode=1;});
