'use strict';
// Navigation labels only. Pool membership always comes from the loaded ROM.
const fs=require('node:fs'),path=require('node:path');
const input=JSON.parse(fs.readFileSync(process.argv[2]));
if(!Array.isArray(input.runtimeKeys))throw Error('Expected runtime-key identity catalog');
const labels={};
for(const row of input.runtimeKeys){
  if(!Number.isInteger(row.runtimeKey)||row.runtimeKey<1||row.runtimeKey>66)throw Error('Invalid runtime key');
  // Match the browser editor's navigation labels, including branch/alias notes.
  // Branch confidence does not determine whether a mission has a usable name.
  if(typeof row.editorLabel==='string'&&row.editorLabel.trim())labels[row.runtimeKey]=row.editorLabel;
}
const output=path.join(__dirname,'../game/encounters/labels.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify({generator:'native/tools/generate-encounter-labels.cjs',
  input:'Parent wiki/runtime-key-branch-labels.json; existing editorLabel values including branch/alias qualifications; keys without labels retain numeric identity',labels},null,2)+'\n');
console.log(`Generated ${Object.keys(labels).length} encounter navigation labels`);
