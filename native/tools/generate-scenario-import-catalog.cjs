'use strict';
// Browser compatibility identities only: no game code, records or art are emitted.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const sourceHash=hash(rom);
assert.equal(sourceHash,'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
const context={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};context.window=context;context.OB64={};vm.createContext(context);
for(const name of ['parsers.js','scenario-eset-data.js'])vm.runInContext(fs.readFileSync(path.join(root,name),'utf8'),context,{filename:name});
const members=[];
for(let at=0x274783e;rom[at]||rom[at+1];) {
 const header=rom.readUInt16LE(at),packed=rom.readUInt32LE(at+7),size=rom.readUInt32LE(at+11);assert.equal(rom[at+20],2);
 let name='';for(let off=24;rom.readUInt16LE(at+off);off+=rom.readUInt16LE(at+off)) {
  const n=rom.readUInt16LE(at+off);assert(n>=3&&off+n+2<=header);
  if(rom[at+off+2]===1)name=rom.toString('latin1',at+off+3,at+off+n);
 }
 const bytes=Buffer.from(context.OB64.lh5Decompress(rom.subarray(at+header,at+header+packed),size));
 members.push({name,sha256:hash(bytes),base:bytes[8]});at+=header+packed;
}
assert.equal(members.length,63);
const rows={};
for(const entry of context.OB64.SCENARIO_ESET_DATA.scenarios) {
 const key=entry.runtimeKey, member=members[rom[0x654a8+9*key]];
 assert.equal(member.name,entry.filename);assert.equal(member.sha256,entry.sha256.toLowerCase());assert.equal(member.base,entry.originalBaseLevel);
 rows[key]={archive:entry.archive,filename:member.name,resourcePath:entry.resourcePath,sha256:member.sha256,baseLevel:member.base,aliases:entry.resourceAliasKeys,excluded:!!entry.levelEditing?.excluded};
}
const result={generator:'native/tools/generate-scenario-import-catalog.cjs',inputs:['scenario-eset-data.js','parsers.js','private US Rev0 ROM'],sourceRomSha256:sourceHash,scenarios:rows};
fs.writeFileSync(path.join(root,'native/game/scenarios/import-catalog.json'),JSON.stringify(result,null,2)+'\n');
console.log(`Verified ${Object.keys(rows).length} browser scenario identities against original member selection and decoded ROM resources.`);
