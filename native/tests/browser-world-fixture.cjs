'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');process.env.OB64_MASTER_ROM=process.argv[2];
const harness=fs.readFileSync(path.join(root,'tests/native-rebuild-runtime-composition.test.js'),'utf8').split('function acceptedShopRecords()')[0];
const browserRequire=require('node:module').createRequire(path.join(root,'tests/native-rebuild-runtime-composition.test.js'));
const {freshRom}=new Function('require','__dirname',harness+'\nreturn {freshRom};')(browserRequire,path.join(root,'tests'));
global.document={getElementById(){return {};}};
const rom=freshRom(),s=OB64.scenario;
s.ensureState(rom);
const archive=OB64.SCENARIO_TREASURE_DATA.runtimeKeyToArchive[52],meta=OB64.SCENARIO_TREASURE_DATA.archives[archive];
const original=Buffer.from(meta.rawHex,'hex'),treasure=Buffer.concat([original,Buffer.from([231,255,0,1,1,21])]);treasure[0]++;
const input={format:'ob64-scenario-project',version:4,
 modifiedTreasures:{[archive]:{archive,filename:meta.filename,rawHex:treasure.toString('hex')}},
 siteAllegiances:{52:{1:'enemy'}},strongholdFields:{185:{population:4321,morale:99}}};
s.loadProject(rom,input);
const patch=OB64.patch.collectPatch(rom);
const result=s.exportScenarioArchives(rom);
assert.deepEqual(result.blocked,[]);assert.equal(result.validationTargets.length,3);
const expected={};
for(const row of result.validationTargets){
 assert(row.expectedRaw);expected[row.archive]=Buffer.from(row.expectedRaw).toString('hex');
}
assert(expected[archive]);assert(expected[691]);assert(expected[725]);
fs.writeFileSync(process.argv[3],JSON.stringify({patch:{format:'ob64-patch',version:36,patches:{scenario:patch.patches.scenario}},expected,treasureArchive:archive}));
console.log('Generated browser world project and three independently exported payloads: treasure addition, shared site allegiance and site metadata.');
