'use strict';
// Parser widths come from the maintained recovered C, semantic labels from the
// existing Editor feature reference. Neither input contributes game payloads.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const {buildModel}=require(path.join(root,'tools/generate-cutscene-director-parser-model.js'));
const source=path.join(root,'native/game/director/func_00284288.c'),model=buildModel(source);
const context={window:{OB64:{}}};vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root,'cutscene-data.js'),'utf8'),context);
const data=context.window.OB64.cutsceneData,grammar=data.directorGrammar;
const rows=model.entries.map(entry=>{
 const semantic=grammar.find(g=>g.opcodeU32===entry.opcodeU32);assert(semantic);
 const actorOperands = entry.opcodeU32 === 0x14
   ? ['actor_slot','art_bank','animation_key','facing','x_milliunits','y_milliunits','z_milliunits','depth_flag','variant']
   : entry.opcodeU32 === 3
   ? ['actor_slot','art_bank','animation_key','facing','x_milliunits','y_milliunits','z_milliunits','variant'] : null;
 const span=entry.widthKind==='terminal-with-trailer'?2:entry.staticWordSpan;
 assert.equal(span,semantic.sourceWordSpan);assert.equal(span-1,semantic.operandRoles.length);
 assert.equal(entry.queryRecordKind,semantic.queryRecordKind);
 return {opcode:entry.opcodeU32,words:span,name:semantic.name,description:semantic.semanticSummary,
  operands:actorOperands || semantic.operandRoles,query:entry.queryRecordKind,mode:entry.directorModeRequirement};
});
const names={};for(const scene of data.scenes){
 const key=parseInt(scene.directorKey,16);if(!Number.isInteger(key))continue;
 names[key]={name:scene.friendlyName||scene.technicalName||scene.assetId,decodedSha256:scene.source.decodedSha256};
}
fs.writeFileSync(path.join(root,'native/game/director/grammar.json'),JSON.stringify({
 generator:'native/tools/generate-director-grammar.cjs',source:'native/game/director/func_00284288.c',
 sourceSha256:model.source.sha256,sourceCommit:'8012b84aec7c04dd61e383cbf40d5b2ae5dcd2a8',rows,names,
 continuations:data.directorContinuationStreams.map(s=>({selector:s.selector,key:s.resourceKey,decodedSha256:s.decodedSha256}))},null,2)+'\n');
console.log(`Generated ${rows.length} C-checked opcode descriptions and ${Object.keys(names).length} scene labels.`);
