'use strict';
// Generate private user assets at test time from the read-only supplied ROM.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
global.window=global;global.OB64={};global.btoa=x=>Buffer.from(x,'binary').toString('base64');global.atob=x=>Buffer.from(x,'base64').toString('binary');
for(const name of ['data.js','parsers.js','art.js','sprite-library.js','animation-corpus-data.js','animation-art.js','combat-animation-overrides-data.js','combat-animation-overrides.js','animation-sequences.js','animation-ui.js'])
 vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../..',name),'utf8'),{filename:name});
(async()=>{
 const bytes=fs.readFileSync(process.argv[2]);if(bytes.readUInt32BE(0)===0x37804012)bytes.swap16();
 const fresh=async()=>{const r={z64:new Uint8Array(bytes),layout:{id:'us-rev0'}};await OB64.art.initialize(r);r.classDefs=OB64.parseClassDefs(r.z64);OB64.combatAnimationOverrides.initialize(r);OB64.animationSequences.initialize(r);return r;};
 const rom=await fresh();
 const api=OB64.animationSequences;
 const route=(cls,action,mode=0)=>OB64.animationUI.effectiveAnimationCatalog(rom.art,rom).specs.find(a=>a.spec.classId===cls&&a.spec.actionId===action&&OB64.animationUI.selectorFlags(a)==='0/0'&&a.spec.rawMode===mode);
 const idle=OB64.animationUI.idleAnimationRows(rom.art.animations,2).find(a=>OB64.animationUI.selectorFlags(a)==='0/0');assert(idle);
 const copy=api.separateAndAssign(rom,idle,null,idle);
 const baseline=api.collectProject(rom),legacy=[];
 const oldDocument=(current,version)=>{
  const old=JSON.parse(JSON.stringify(current));old.schemaVersion=version;
  for(const entry of Object.values(old.entries)){
   delete entry.poseProgramBase64;
   if(version===1){delete entry.frames;for(const [key,s] of Object.entries(entry.sources)){if(!s.editable){delete entry.sources[key];continue;}entry.sources[key]={width:s.width,height:s.height,childCount:s.childCount,children:s.children};}}
   if(version===2)for(const frame of entry.frames)delete frame.sourceFrameIndex;
  }
  return old;
 };
 for(const version of [1,2,3]){
  const old=oldDocument(baseline,version);api.prepareProject(rom,old);legacy.push({input:old,expected:baseline});
 }
 const trimmed=oldDocument(baseline,3);trimmed.entries[copy.id].frames.splice(1,1);trimmed.entries[copy.id].frames.forEach((f,i)=>f.sequenceIndex=i);
 const trimRom=await fresh();api.applyProject(trimRom,api.prepareProject(trimRom,trimmed));legacy.push({input:trimmed,expected:api.collectProject(trimRom)});
 const variantRom=await fresh(),variant=OB64.animationUI.idleAnimationRows(variantRom.art.animations,1).find(a=>a.spec.selectedBodyChild>0);assert(variant);
 api.separateAndAssign(variantRom,variant,null,variant);const variantBase=api.collectProject(variantRom),oldVariant=oldDocument(variantBase,1);
 api.prepareProject(variantRom,oldVariant);legacy.push({input:oldVariant,expected:variantBase});
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
 const linked=route(10,13);assert(linked);api.separateAndAssign(rom,linked,OB64.combatAnimationOverrides.vanillaPairForLiveAction(10,rom.classDefs[11],13),linked);
 const payload=api.collectProject(rom);
 // Compare decoded authoring results; compression/allocation is checked by native export.
 OB64.art.bootLzCompress=b=>b;
 const plan=api.buildPlan(rom,rom.z64);
 const expected=plan.groups.map(g=>({id:g.id,baselineHandle:g.originalHandle,
  controls:g.controls.map(x=>Buffer.from(x.decoded).toString('base64')),
  sources:g.sourceRows.map(x=>({data:Buffer.from(x.decoded).toString('base64'),policy:x.source.selectorPolicy,palette:Buffer.from(new Uint8Array(x.source.palette.buffer)).swap16().toString('base64')})),
  routes:g.separations.map(s=>({id:s.id,classId:s.classId,actionId:s.actionId,bodyFlags:s.bodyFlags,laneKey:s.laneKey,selector:s.selector}))}));
 fs.writeFileSync(process.argv[3],JSON.stringify({project:{format:'ob64-patch',version:36,patches:{art:{schemaVersion:5,separations:payload},combatAnimationOverrides:{entries:rom.combatAnimationOverrides.desired}}},expected,baseline,legacy}));
 console.log(`Browser schema ${payload.schemaVersion}: ${Object.keys(payload.entries).length} sequences in ${expected.length} groups`);
})().catch(e=>{console.error(e);process.exitCode=1;});
