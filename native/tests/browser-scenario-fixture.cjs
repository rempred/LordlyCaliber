'use strict';
// Produce a real browser project for the native importer; no generated game
// data is checked into source. Reuse the browser's existing read-only ROM setup.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
process.env.OB64_MASTER_ROM=process.argv[2];
const harness=fs.readFileSync(path.join(root,'tests/native-rebuild-runtime-composition.test.js'),'utf8')
 .split('function acceptedShopRecords()')[0];
const browserRequire=require('node:module').createRequire(path.join(root,'tests/native-rebuild-runtime-composition.test.js'));
const {freshRom}=new Function('require','__dirname',harness+'\nreturn {freshRom};')(browserRequire,path.join(root,'tests'));
// Only headless authoring APIs are used. A style lookup prevents DOM creation;
// there is no selected browser ROM reference and therefore no rendered panel.
global.document={getElementById(){return {};}};
const rom=freshRom(),s=OB64.scenario;
s.setEnemyBaseLevel(rom,10,12);
s.setSquadLevelOffsetSigned(rom,6,0,'A',1);
s._modelTest.createAddedSquadAt(rom,1,500,500,{
 imageToWorld(){return {x:0,z:0};},worldToImage(x,z){return {x:1000+x*100,y:1000+z*100};}
});
const state=s.ensureState(rom),ordinary=state.models[2].section1[0].edatOneBased-1;
const extra=Uint8Array.from(Buffer.from(OB64.SCENARIO_ESET_DATA.enemydat.records[ordinary],'hex'));
extra[2]=1;extra[3]=21; // Equipment 277, without a level-copy ownership record.
rom.squadOverrides[`2:${ordinary}`]=extra;
const collected=OB64.patch.collectPatch(rom),scenario=collected.patches.scenario,overrides=collected.patches.squadOverrides;
assert.equal(scenario.addedSquads.length,1);assert(scenario.squadLevelCopies.length>0);
const keys=new Set([...Object.keys(scenario.modifiedEsets).map(Number),...Object.keys(overrides).map(k=>+k.split(':')[0])]);
const expected={};
for(const key of keys) {
 const raw=OB64.scenarioCodec.serializeEset(state.models[key]),model=OB64.scenarioCodec.parseEset(raw),squads=[];
 for(const row of model.section1) {
  const id=row.edatOneBased-1,record=rom.squadOverrides[`${key}:${id}`];
  squads.push(record?Buffer.from(record).toString('hex'):OB64.SCENARIO_ESET_DATA.enemydat.records[id].toLowerCase());
  row.bytes[1]=row.bytes[2]=0;
 }
 expected[key]={layout:Buffer.from(OB64.scenarioCodec.serializeEset(model)).toString('hex'),squads};
}
const classCapacities=Array.from({length:165},(_,id)=>rom.classDefs[id+1].itemCapacity),capacities={};
for(const [key,model] of Object.entries(state.models)) {
 if(+key===54)continue;
 for(const row of model.section1) {
  const id=row.edatOneBased-1,record=rom.squadOverrides[`${key}:${id}`]||Buffer.from(OB64.SCENARIO_ESET_DATA.enemydat.records[id],'hex');
  const checked=s.calculateSupplyCapacityForRecord(rom,record);
  assert(checked.safe,`Browser capacity source ${key}:${id}: ${checked.issues}`);
  capacities[Buffer.from(record).toString('hex')]=checked.capacity;
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({patch:{format:'ob64-patch',version:36,patches:{scenario,squadOverrides:overrides}},expected,classCapacities,capacities}));
console.log(`Generated browser authoring fixture with ${keys.size} scenarios, one added squad and explicit base/level-copy intent.`);
