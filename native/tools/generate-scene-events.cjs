// Event identities and verified source digests; no game program bytes are shipped.
const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..'),context={window:{OB64:{}}};
vm.createContext(context);
for(const file of ['cutscene-data.js','cutscene-catalog.js','cutscene-event-rows-data.js',
 'cutscene-provenance-data.js','cutscene-events.js'])
 vm.runInContext(fs.readFileSync(path.join(root,file),'utf8'),context,{filename:file});
const O=context.window.OB64,catalog=O.cutsceneCatalog.createCatalog(O.cutsceneData);
const sites=O.cutsceneEvents.siteIndex(catalog).list.map(site=>{
 const scene=catalog.getScene(site.retailSceneId);
 return {...site,name:O.cutsceneCatalog.displayName(scene)};
});
const source=O.cutsceneEventRows;
fs.writeFileSync(path.join(root,'native/game/director/events.json'),JSON.stringify({
 generator:'native/tools/generate-scene-events.cjs',format:'lordly-event-sites',version:1,
 sourceRevision:source.sourceRevision,resourceBaseZ64:source.resourceBaseZ64,
 directoryZ64:source.directoryZ64,rows:source.rows,sites
},null,2)+'\n');
console.log(`Generated ${sites.length} launch sites and ${source.rows.length} row identities.`);
