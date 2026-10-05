'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'../..'),context={window:{OB64:{}}};context.OB64=context.window.OB64;vm.createContext(context);
for(const file of ['combat-animation-overrides-data.js','combat-animation-overrides.js'])vm.runInContext(fs.readFileSync(path.join(root,file),'utf8'),context,{filename:file});
const bytes=fs.readFileSync(process.argv[2]);if(bytes.readUInt32BE(0)===0x37804012)bytes.swap16();else if(bytes.readUInt32BE(0)===0x40123780)bytes.swap32();
process.stdout.write(JSON.stringify(context.OB64.combatAnimationOverrides.classify(bytes)));
