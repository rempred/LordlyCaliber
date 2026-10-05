'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
global.window=global;global.OB64={};
for(const file of ['tools-data.js','tools.js'])vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../..',file),'utf8'));
const source=fs.readFileSync(process.argv[2]);if(source.readUInt32BE(0)===0x37804012)source.swap16();
const feature=OB64.tools.getFeature('experience-size-scale'),cases=[];
for(const percent of [0,75,137,300,100]){
 const rom={z64:Uint8Array.from(source),layout:{supportsTools:true,unsupportedTools:{}}};
 OB64.tools.initState(rom);OB64.tools.setDesiredPercent(rom,feature.id,percent);OB64.tools.applyDesired(rom);
 assert.equal(OB64.tools.parameterValue(rom.z64,feature),percent);
 cases.push({percent,writes:feature.writes.map(w=>({offset:w.offset,bytes:Buffer.from(rom.z64.slice(w.offset,w.offset+w.patched.length/2)).toString('hex')}))});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));console.log('Five browser experience exports');
