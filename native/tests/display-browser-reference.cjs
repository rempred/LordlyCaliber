'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const assemble=require('../tools/lib/assemble-display-tool.cjs');
const catalog=require('../game/tools/displays.json');
const context={OB64:{}};context.window=context;vm.createContext(context);
for(const file of ['native-rebuild-data.js','tools-data.js'])
 vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../..',file),'utf8'),context);
const cases=['character-card-luck','squad-menu-alignment'].map(id=>{
 const components=context.OB64_NATIVE_REBUILD_DATA.payload.phase3.components.filter(c=>c.owner==='tool.'+id);
 const payload=components.find(c=>c.name==='source-module');
 if(!payload)throw Error('Missing source module: '+id);
 const definition=catalog.features.find(f=>f.id===id);
 const current=components.map(c=>{const site=definition.sites.find(s=>s.name===c.name);
  const bytes=assemble(definition.name,site,definition);
  if(c.name==='source-module')Buffer.from(payload.dataHex,'hex').copy(bytes,0x520,0x520,0x720);
  return {...c,dataHex:bytes.toString('hex')};});
 return {id,texture:payload.dataHex.slice(0x520*2,0x720*2),components:current,previousComponents:components,
  legacy:context.OB64.TOOLS_FEATURES.find(f=>f.id===id).writes};
});
fs.writeFileSync(process.argv[2],JSON.stringify({cases}));
console.log('Current browser Luck and six-row Alignment source references');
