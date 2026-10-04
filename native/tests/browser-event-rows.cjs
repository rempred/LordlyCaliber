const fs=require('fs'),path=require('path'),vm=require('vm');
const root=path.resolve(__dirname,'../..'); global.window=global; global.OB64={};
for(const file of ['parsers.js','repack.js','cutscene-data.js','cutscene-catalog.js','cutscene-model.js','cutscene-director.js','cutscene-codec.js','cutscene-event-rows-data.js','cutscene-provenance-data.js','cutscene-events.js'])
 vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'),{filename:file});
(async()=>{
 const raw=fs.readFileSync(process.argv[2]);
 const rom=raw[0]===0x37?Uint8Array.from(raw,(_,i)=>raw[i^1]):Uint8Array.from(raw);
 const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData), sites=OB64.cutsceneEvents.siteIndex(catalog).list;
 const rows=[];
 for(const row of [...new Set(sites.map(s=>s.row))]) {
  const retail=await OB64.cutsceneEvents.retailRow(rom,row), matches=sites.filter(s=>s.row===row);
  const bind=selector=>matches.map(s=>({...s,selector,retailSelector:s.selector}));
  rows.push({row,high:Buffer.from(OB64.cutsceneEvents.boundBytes(retail,bind(319))).toString('hex'),
    low:Buffer.from(OB64.cutsceneEvents.boundBytes(retail,bind(2))).toString('hex')});
 }
 fs.writeFileSync(process.argv[3],JSON.stringify({rows}));
})().catch(e=>{console.error(e);process.exitCode=1;});
