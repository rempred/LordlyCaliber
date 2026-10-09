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
// A title card does not establish a playable scenario. These four US Rev 0
// slots have zero 12-byte descriptors at z64 0x13C550 + 12*key and select
// the default eset0_00.bin at 0x654A0 + 9*key + 8. Keep their titles and access,
// but distinguish them from the real story variants. See docs/architecture.md.
// The browser editor's curated DEV_KEYS also includes special/internal 54/63/64.
// Joe uses "Dev Key" for this entire group, not a claim about developer intent.
const devKeys=[10,22,35,54,62,63,64];
const notes={};
for(const key of [10,22,35,62]){
  notes[key]='This slot has no scenario setup of its own in the original ROM and uses the default squad data. '
    +(titles[key]?`Its title card reads "${titles[key]}", but that does not establish which map it runs. `
      :'There is no entry-title card for this key. ')
    +'The slot remains available for editing; its original development purpose is unconfirmed.';
}
notes[54]='Internal scenario with a different loading path. Editing is not supported. Its title card reads "The Sleeping Goddess"; this does not establish its runtime map.';
notes[63]='Known broken internal scenario. The earlier Infernal Aura / Dardunnelles label was an unconfirmed match.';
notes[64]='Internal scenario. The earlier A New Beginning label was an unconfirmed match; its natural story launch and map identity are not established.';
for(const key of devKeys){
  annotations[key]='Dev Key';
  labels[key]='Dev Key';
}
notes[20]='The Grim Path: decline peace with the Western Division; Ankiseth is the opposing commander.';
notes[21]='The Grim Path: make peace with the Western Division; Baldwin is the opposing commander.';
notes[22]+=' The known Grim Path story branches use keys 20 and 21; a third playable branch has not been established.';
const output=path.join(__dirname,'../game/encounters/labels.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify({generator:'native/tools/generate-encounter-labels.cjs',
  input:'Verified native/game/encounters/entry-titles.json titles with parent wiki/runtime-key-branch-labels.json location/branch annotations; Dev Keys follow browser editor/scenario.js classification and US Rev 0 placeholder descriptors (see native/docs/architecture.md)',
  titles,annotations,notes,devKeys,labels},null,2)+'\n');
console.log(`Generated ${Object.keys(labels).length} encounter navigation labels`);
