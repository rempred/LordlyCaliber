const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['parsers.js','repack.js','cutscene-data.js','cutscene-model.js','cutscene-catalog.js','cutscene-director.js','cutscene-codec.js'])
 vm.runInThisContext(fs.readFileSync(path.join(root,f),'utf8'),{filename:f});
(async()=>{
 const raw=fs.readFileSync(process.argv[2]),rom=raw[0]===0x37?Uint8Array.from(raw,(_,i)=>raw[i^1]):Uint8Array.from(raw);
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),result=[];
 for(const scene of catalog.scenes.filter(s=>s.engine==='director'&&(s.source.registeredWaits||[]).length)) {
  const source=await OB64.cutsceneCodec.loadSceneSource(rom,scene,{allowModified:true});
  const document=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog).document;
  result.push({id:scene.sceneId,document,source:Buffer.from(source.decodedBytes).toString('hex'),
   expected:Buffer.from(OB64.cutsceneCodec.compileSceneDocument(scene,source,document).decodedBytes).toString('hex')});
 }
 fs.writeFileSync(process.argv[3],JSON.stringify(result));
})().catch(e=>{console.error(e);process.exitCode=1;});
