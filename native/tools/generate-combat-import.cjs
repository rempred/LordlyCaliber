// Generates project-import identities only; no game asset or executable bytes.
// Inputs: animation-corpus-data.js, animation-art.js at the repository root.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const root=path.resolve(__dirname,'../..'),scope={OB64:{}};scope.window=scope;vm.createContext(scope);
for(const file of ['animation-corpus-data.js','animation-art.js'])vm.runInContext(fs.readFileSync(path.join(root,file),'utf8'),scope);
const descriptors={},animations={};for(const row of scope.OB64.animationCorpusData.sequences){const descriptor=row.descriptorKey.toString(16);descriptors[descriptor]??=[];if(!descriptors[descriptor].includes(row.classId))descriptors[descriptor].push(row.classId);const key='0x'+row.descriptorKey.toString(16).padStart(8,'0');animations[row.key]=key;if(row.compatibilityKey)animations[row.compatibilityKey]=key;}
for(const row of scope.OB64.animationArt.specs)animations[row.key]='0x'+row.descriptorKey.toString(16).padStart(8,'0');
fs.writeFileSync(path.join(root,'native/game/art/combat-import.json'),JSON.stringify({generator:'native/tools/generate-combat-import.cjs',inputs:['animation-corpus-data.js','animation-art.js'],animations,descriptors},null,2)+'\n');
