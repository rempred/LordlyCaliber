'use strict';
// Navigation labels only. Pool membership always comes from the loaded ROM.
const fs=require('node:fs'),path=require('node:path');
const input=JSON.parse(fs.readFileSync(process.argv[2]));
if(!Array.isArray(input.runtimeKeys))throw Error('Expected runtime-key identity catalog');
const cards=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/encounters/entry-titles.json')));
const labels={};
for(const row of input.runtimeKeys){
  if(!Number.isInteger(row.runtimeKey)||row.runtimeKey<1||row.runtimeKey>66)throw Error('Invalid runtime key');
  // Keep legacy annotations and the names of keys without an entry-title card.
  if(typeof row.editorLabel==='string'&&row.editorLabel.trim())labels[row.runtimeKey]=row.editorLabel;
}
const titles={},annotations={};
if(Object.keys(cards.titles).length!==62)throw Error('Expected the 62 verified entry-title cards');
for(let key=0;key<=61;key++){
  const title=cards.titles[key];
  if(typeof title!=='string'||!title.trim())throw Error(`Missing entry title for scenario key ${key}`);
  titles[key]=title;
  const old=labels[key]||'';
  // Geography and actual branch descriptions are useful alongside the title.
  // Numeric alias/flag notes add no title information once its card is verified.
  let annotation='';
  if(!old.startsWith('Runtime Key ')){
    const suffix=old.search(/ \(| - /);
    if(suffix>=0)annotation=old.slice(suffix);
    annotation=annotation.replace(/ - (?:flagged key \d+|runtime key \d+ alias|branch key \d+)$/,'');
  }
  if(annotation)annotations[key]=annotation.trim();
  labels[key]=title+annotation;
}
const output=path.join(__dirname,'../game/encounters/labels.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify({generator:'native/tools/generate-encounter-labels.cjs',
  input:'Verified native/game/encounters/entry-titles.json titles with parent wiki/runtime-key-branch-labels.json location/branch annotations; keys beyond the title directory retain existing labels or numeric identity',
  titles,annotations,labels},null,2)+'\n');
console.log(`Generated ${Object.keys(labels).length} encounter navigation labels`);
