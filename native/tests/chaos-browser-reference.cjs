'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const context={OB64:{}};context.window=context;vm.createContext(context);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../native-rebuild-data.js'),'utf8'),context);
const components=context.window.OB64_NATIVE_REBUILD_DATA.payload.phase3.components
 .filter(c=>c.owner==='tool.chaos-frame-counter');
const payload=components.find(c=>c.name==='source-module');
if(!payload)throw Error('Missing browser Chaos Frame module');
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../tools-data.js'),'utf8'),context);
const legacy=context.OB64.TOOLS_FEATURES.find(f=>f.id==='cf-army-counter').writes;
fs.writeFileSync(process.argv[2],JSON.stringify({title:payload.dataHex.slice(0x700*2,0x880*2),components,legacy}));
console.log('Browser Chaos Frame source-module reference');
