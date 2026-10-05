'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const context={OB64:{}};context.window=context;vm.createContext(context);
for(const file of ['native-rebuild-data.js','tools-data.js'])
 vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../..',file),'utf8'),context);
const cases=['character-card-luck','squad-menu-alignment'].map(id=>{
 const components=context.OB64_NATIVE_REBUILD_DATA.payload.phase3.components.filter(c=>c.owner==='tool.'+id);
 const payload=components.find(c=>c.name==='source-module');
 if(!payload)throw Error('Missing source module: '+id);
 return {id,texture:payload.dataHex.slice(0x520*2,0x720*2),components,
  legacy:context.OB64.TOOLS_FEATURES.find(f=>f.id===id).writes};
});
fs.writeFileSync(process.argv[2],JSON.stringify({cases}));
console.log('Current browser Luck and six-row Alignment source references');
