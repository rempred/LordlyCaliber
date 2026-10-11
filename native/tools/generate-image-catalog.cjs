'use strict';
// Source locations, layouts and labels only. No game pixels or stream payloads.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'../..'),context={window:{OB64:{}}};vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root,'cutscene-data.js'),'utf8'),context);
const data=context.window.OB64.cutsceneData;
const images=data.imageAssets.map(a=>{
 const s=a.source,kind=a.sourceKind==='lha-archive'?'lha':a.sourceKind==='section-c-njpg'?'huff':
  s.compressionKind==='ob64-custom-lz'?'custom':'stored';
 const key=kind==='lha'?s.archiveHeaderZ64-4-0x594280:kind==='huff'?s.z64Start-0x594280:s.resourceKey;
 return {id:a.assetId,name:a.displayName,container:a.container,key,kind,layout:a.layout,
  members:a.compound?.members.map(m=>m.assetId)||[],sourceSha256:s.decodedSha256||null};
});
const scenes={};for(const s of data.scenes) {
 const key=parseInt(s.directorKey,16);if(!Number.isInteger(key))continue;
 scenes[key]={sourceSha256:s.source.decodedSha256,mode:s.launchProfile?.directorMode?.value,
  modeEvidence:s.launchProfile?.directorMode?.status||''};
}
fs.writeFileSync(path.join(root,'native/game/director/images.json'),JSON.stringify({
 generator:'native/tools/generate-image-catalog.cjs',input:'cutscene-data.js',images,scenes},null,2)+'\n');
console.log(`Generated ${images.length} image layouts and ${Object.keys(scenes).length} scene context references.`);
