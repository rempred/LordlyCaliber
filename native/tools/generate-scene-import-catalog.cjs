// Metadata only: scene identities, current-link lookup and legacy compiler context.
const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..'),context={window:{OB64:{}}};vm.createContext(context);
for(const f of ['cutscene-data.js','cutscene-catalog.js'])vm.runInContext(fs.readFileSync(path.join(root,f),'utf8'),context,{filename:f});
const O=context.window.OB64,catalog=O.cutsceneCatalog.createCatalog(O.cutsceneData),scenes={};
for(const s of catalog.scenes.filter(s=>s.engine==='director')) {
 scenes[s.sceneId]={sceneId:s.sceneId,assetId:s.assetId,storageId:s.storageId,key:parseInt(s.directorKey,16),
  selectors:s.source.directorSelectorRows||[],mode:s.launchProfile?.directorMode?.value??-1,
  decodedSha256:s.source.decodedSha256,reparsedForAuthoring:s.source.dynamicGrammar===true,
  registeredWaits:s.source.registeredWaits||[]};
}
fs.writeFileSync(path.join(root,'native/game/director/legacy-scenes.json'),JSON.stringify({generator:'native/tools/generate-scene-import-catalog.cjs',version:1,scenes},null,2)+'\n');
console.log(JSON.stringify({scenes:Object.keys(scenes).length,withWaits:Object.values(scenes).filter(s=>s.registeredWaits.length).length,sample:scenes['scene:director:01f3ead2']}));
