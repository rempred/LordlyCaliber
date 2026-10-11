'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const c={OB64:{}};c.window=c;vm.createContext(c);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../tools-data.js'),'utf8'),c);
fs.writeFileSync(process.argv[2],JSON.stringify(c.OB64.TOOLS_FEATURES.find(f=>f.id==='high-attack-streamsplit').writes));
