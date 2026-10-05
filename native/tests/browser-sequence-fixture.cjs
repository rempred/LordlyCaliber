'use strict';
// Generate private user assets at test time from the read-only supplied ROM.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
global.window=global;global.OB64={};global.btoa=x=>Buffer.from(x,'binary').toString('base64');global.atob=x=>Buffer.from(x,'base64').toString('binary');
for(const name of ['data.js','parsers.js','art.js','sprite-library.js','animation-corpus-data.js','animation-art.js','combat-animation-overrides-data.js','combat-animation-overrides.js','animation-sequences.js','animation-ui.js'])
 vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../..',name),'utf8'),{filename:name});
(async()=>{
 const bytes=fs.readFileSync(process.argv[2]);if(bytes.readUInt32BE(0)===0x37804012)bytes.swap16();
 const rom={z64:new Uint8Array(bytes),layout:{id:'us-rev0'}};
 await OB64.art.initialize(rom);rom.classDefs=OB64.parseClassDefs(rom.z64);OB64.combatAnimationOverrides.initialize(rom);OB64.animationSequences.initialize(rom);
 const api=OB64.animationSequences;
 const route=(cls,action,mode=0)=>OB64.animationUI.effectiveAnimationCatalog(rom.art,rom).specs.find(a=>a.spec.classId===cls&&a.spec.actionId===action&&OB64.animationUI.selectorFlags(a)==='0/0'&&a.spec.rawMode===mode);
 const idle=OB64.animationUI.idleAnimationRows(rom.art.animations,2).find(a=>OB64.animationUI.selectorFlags(a)==='0/0');assert(idle);
 const copy=api.separateAndAssign(rom,idle,null,idle);
 api.setFrameTicks(rom,copy,0,21);api.setLayerPosition(rom,copy,0,0,-23,14);
 api.duplicateFrame(rom,copy,0);
 const source=Object.values(copy.syntheticAnimation.artByKey).find(s=>s.editable);
 const child=source.editableChildOrdinals[0],edit=OB64.animationArt.currentEdit(rom.art.animations,source.key,child);
 const indices=edit.indices.slice();indices[0]=(indices[0]+1)&255;
 OB64.animationArt.setEdit(rom.art.animations,source.key,child,indices,edit.intensity.slice());
 const fighter=route(2,4),donor=route(3,4);assert(fighter&&donor);
 const pair=OB64.combatAnimationOverrides.vanillaPairForLiveAction(2,rom.classDefs[3],4);
 const normal=api.separateAndAssign(rom,donor,pair,fighter);
 api.setFrameTicks(rom,normal,0,13);
 const blocked=route(2,4,2);assert(blocked);api.separateAndAssign(rom,donor,pair,blocked);
 const payload=api.collectProject(rom);
 // Compare decoded authoring results; compression/allocation is checked by native export.
 OB64.art.bootLzCompress=b=>b;
 const plan=api.buildPlan(rom,rom.z64);
 const expected=plan.groups.map(g=>({id:g.id,baselineHandle:g.originalHandle,
  controls:g.controls.map(x=>Buffer.from(x.decoded).toString('base64')),
  sources:g.sourceRows.map(x=>({data:Buffer.from(x.decoded).toString('base64'),policy:x.source.selectorPolicy,palette:Buffer.from(new Uint8Array(x.source.palette.buffer)).swap16().toString('base64')})),
  routes:g.separations.map(s=>({id:s.id,classId:s.classId,actionId:s.actionId,bodyFlags:s.bodyFlags,laneKey:s.laneKey,selector:s.selector}))}));
 fs.writeFileSync(process.argv[3],JSON.stringify({project:{format:'ob64-patch',version:36,patches:{art:{schemaVersion:5,separations:payload},combatAnimationOverrides:{entries:rom.combatAnimationOverrides.desired}}},expected}));
 console.log(`Browser schema ${payload.schemaVersion}: ${Object.keys(payload.entries).length} sequences in ${expected.length} groups`);
})().catch(e=>{console.error(e);process.exitCode=1;});
