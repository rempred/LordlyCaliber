'use strict';
// Parser widths come from the maintained recovered C, semantic labels from the
// existing Editor feature reference. Neither input contributes game payloads.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const {buildModel}=require(path.join(root,'tools/generate-cutscene-director-parser-model.js'));
const catalog=require('../game/accepted/rom-owners.json');
const dispatcher=catalog.sources.find(owner=>owner.name==='func_00284288');
const source='native/'+dispatcher.source,model=buildModel(path.join(root,source));
assert.equal(model.source.sha256,dispatcher.sourceSha256);
const context={window:{OB64:{}}};vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root,'cutscene-data.js'),'utf8'),context);
const data=context.window.OB64.cutsceneData,grammar=data.directorGrammar;
const actorFields = {
  0x90: ['compare_mode','signed_target','actor_slot'],
  0xa1: ['compare_mode','signed_target','actor_slot'],
  0xae: ['panel_slot','help_section_0_to_9'],
  0x2b: ['compare_mode_and_actor_slot', 'target', 'unused_word'],
  0xc2: ['anchor_actor_slot', 'initial_body_pose']
};
const rows=model.entries.map(entry=>{
 const semantic=grammar.find(g=>g.opcodeU32===entry.opcodeU32);assert(semantic);
 const actorOperands = entry.opcodeU32 === 0x14
   ? ['actor_slot','art_bank','animation_key','facing','x_milliunits','y_milliunits','z_milliunits','depth_flag','variant']
   : entry.opcodeU32 === 3
   ? ['actor_slot','art_bank','animation_key','facing','x_milliunits','y_milliunits','z_milliunits','variant'] : null;
 // Runtime-qualified corrections to the older browser feature reference.
 const descriptions={
  0x90:'Compares the signed actor profile. A vacant slot or zero art key gives -1. Other actors use their roster gender and alignment band, with story-class overrides.',
  0xa1:'Compares the signed roster result: 0 for a vacant or unlinked slot, 1 for a persistent character, 2 for a deployed character.',
  0xae:'Replaces the text of an open Army panel. Help 7 expands the scene subject. Help 8 and 9 expand the selected name.',
  0x54:'Returns 1 during image interpolation, -1 while echo trails drain, and 0 when finished.',
  0xc2:'The first eligible briefing row uses the occupied anchor. Each later component copies the preceding actor after its first pose. The two operands use their signed low 16 bits.'
 };
 const span=entry.widthKind==='terminal-with-trailer'?2:entry.staticWordSpan;
 assert.equal(span,semantic.sourceWordSpan);assert.equal(span-1,semantic.operandRoles.length);
 assert.equal(entry.queryRecordKind,semantic.queryRecordKind);
 return {opcode:entry.opcodeU32,words:span,name:semantic.name,description:descriptions[entry.opcodeU32] || semantic.semanticSummary,
  operands:actorFields[entry.opcodeU32] || actorOperands || semantic.operandRoles,query:entry.queryRecordKind,mode:entry.directorModeRequirement};
});
const names={};for(const scene of data.scenes){
 const key=parseInt(scene.directorKey,16);if(!Number.isInteger(key))continue;
 names[key]={name:scene.friendlyName||scene.technicalName||scene.assetId,decodedSha256:scene.source.decodedSha256};
}
const selectorNames={};
for(const scene of data.scenes){
 const key=parseInt(scene.directorKey,16);
 if(!names[key])continue;
 for(const row of scene.source && scene.source.directorSelectorRows || [])selectorNames[row]=names[key].name;
}
fs.writeFileSync(path.join(root,'native/game/director/grammar.json'),JSON.stringify({
 generator:'native/tools/generate-director-grammar.cjs',source,
 sourceSha256:model.source.sha256,sourceCommit:catalog.revision,rows,names,selectorNames,
 claimedSelectors:[...new Set(data.scenes.flatMap(s=>s.source && s.source.directorSelectorRows || []))].sort((a,b)=>a-b),
 continuations:data.directorContinuationStreams.map(s=>({selector:s.selector,key:s.resourceKey,decodedSha256:s.decodedSha256}))},null,2)+'\n');
console.log(`Generated ${rows.length} C-checked opcode descriptions and ${Object.keys(names).length} scene labels.`);
