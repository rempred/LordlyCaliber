'use strict';
// Integrity metadata only; no game bytes are emitted.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const image=fs.readFileSync(process.argv[2]);if(image.readUInt32BE(0)===0x37804012)image.swap16();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const input=hash(image);
if(input!=='571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a')throw Error('US Rev0 required');
const spans=[
 ['Site archive selection',0x1278d4,0x127934],
 ['Treasure archive selection',0x196138,0x196174],
 ['Resource command dispatcher',0x978c,0x9a18],
 ['Resource transfer',0x1a380,0x1a440],
 ['Resource key and envelope readers',0x2de50,0x2e138]
].map(([name,start,end])=>({name,start,size:end-start,sha256:hash(image.subarray(start,end))}));
fs.writeFileSync(path.resolve(__dirname,'../game/scenarios/world-owners.json'),JSON.stringify({
 generator:'native/tools/generate-world-resource-owners.cjs',inputRomSha256:input,spans},null,2)+'\n');
