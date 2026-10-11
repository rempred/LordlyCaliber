'use strict';
// Integrity metadata only. No executable bytes or game payload are emitted.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const image=fs.readFileSync(process.argv[2]);if(image.readUInt32BE(0)===0x37804012)image.swap16();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const input=hash(image);
if(input!=='571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a')throw Error('US Rev0 required');
const spans=[
 ['Scenario archive selection',0xec178,0xec1ec],
 ['Resource command dispatcher',0x978c,0x9a18],
 ['Resource transfer',0x1a380,0x1a440],
 ['Resource key and envelope readers',0x2de50,0x2e138]
].map(([name,start,end])=>({name,start,size:end-start,sha256:hash(image.subarray(start,end))}));
const out=path.resolve(__dirname,'../game/scenarios/resource-owners.json');fs.mkdirSync(path.dirname(out),{recursive:true});
fs.writeFileSync(out,JSON.stringify({generator:'native/tools/generate-scenario-resource-owners.cjs',inputRomSha256:input,spans},null,2)+'\n');
