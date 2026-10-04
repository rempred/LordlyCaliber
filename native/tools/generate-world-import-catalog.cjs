'use strict';
// Compatibility identities and hashes only. No decoded game records are emitted.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),rom=fs.readFileSync(process.argv[2]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex'),sourceHash=hash(rom);
assert.equal(sourceHash,'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
const context={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};context.window=context;context.OB64={};vm.createContext(context);
for(const name of ['parsers.js','scenario-eset-data.js','scenario-treasure-data.js'])vm.runInContext(fs.readFileSync(path.join(root,name),'utf8'),context,{filename:name});
const o=context.OB64,archives=o.findArchives(rom);
function member(index){
 const a=archives[index],at=a.offset;let name='';
 if(a.level===2){for(let off=24;rom.readUInt16LE(at+off);off+=rom.readUInt16LE(at+off)){
  const n=rom.readUInt16LE(at+off);assert(n>=3&&off+n+2<=a.totalHeaderSize);
  if(rom[at+off+2]===1)name=rom.toString('latin1',at+off+3,at+off+n);
 }}else{assert.equal(a.level,0);name=rom.toString('latin1',at+22,at+22+rom[at+21]);}
 return {name,bytes:Buffer.from(o.extractArchive(rom,a)),offset:at};
}
function collection(slot,first,count){
 const at=rom.readUInt32BE(0x2743c7c+slot*4)+0x594280,end=at+4+rom.readUInt32BE(at);let cursor=at+4;
 for(let i=0;i<count;i++){const a=archives[first+i];assert.equal(a.offset,cursor);cursor+=a.totalHeaderSize+a.compSize;}
 assert.equal(cursor,end-1);assert.equal(rom[cursor],0);
}
collection(0,648,40);collection(2,691,61);
const treasures={},sites={},scenarios={};
for(let i=648;i<688;i++){
 const m=member(i),meta=o.SCENARIO_TREASURE_DATA.archives[i];assert.equal(m.name,meta.filename);
 assert.equal(m.bytes.toString('hex'),meta.rawHex.toLowerCase());
 treasures[i]={filename:m.name,sha256:hash(m.bytes),member:i-648};
}
for(let i=692;i<=750;i++){
 const m=member(i),meta=o.SCENARIO_ESET_DATA.scincsvArchives[i];assert.equal(m.name,meta.filename);
 assert.equal(m.bytes.toString('hex'),meta.payloadHex.toLowerCase());
 sites[i]={filename:m.name,sha256:hash(m.bytes),member:i-691};
}
const towns=member(691);assert.equal(towns.name,'ktenmain.bin');assert.equal(towns.bytes.length,316*28);
let mapped=0,absent=0;
for(const meta of o.SCENARIO_ESET_DATA.scenarios){
 const key=meta.runtimeKey,t=rom[0x13c556+key*12],s=rom[0x13c555+key*12],selectors={};
 assert.equal(o.SCENARIO_TREASURE_DATA.runtimeKeyToArchive[key],648+t);
 for(const site of meta.sites||[]){
  const d=site.siteDescriptor;
  if(!d?.descriptorPresent){selectors[site.selector]={present:false};absent++;continue;}
  assert.equal(d.scincsvArchive,691+s);const raw=member(691+s).bytes,off=d.descriptorByteOffset;
  assert(off>=3&&(off-3)%4===0&&(off-3)/4<raw[0]);
  assert.equal(raw.readUInt16BE(off-2),d.ownKtenmainRecordIndex+1);assert.equal(raw.readUInt16BE(off),d.descriptorAddend);
  const name=towns.bytes.subarray(d.ownKtenmainRecordIndex*28+1,d.ownKtenmainRecordIndex*28+21).toString('latin1').split('\0')[0];
  assert.equal(name,d.ownKtenmainName);
  selectors[site.selector]={present:true,row:(off-3)/4,siteId:d.ownKtenmainRecordIndex+1};mapped++;
 }
 scenarios[key]={treasureArchive:648+t,siteArchive:s?691+s:null,selectors};
}
fs.writeFileSync(path.join(root,'native/game/scenarios/world-import-catalog.json'),JSON.stringify({
 generator:'native/tools/generate-world-import-catalog.cjs',inputs:['scenario-eset-data.js','scenario-treasure-data.js','parsers.js','private US Rev0 ROM'],sourceRomSha256:sourceHash,
 townsSha256:hash(towns.bytes),treasures,sites,scenarios},null,2)+'\n');
console.log(`Verified 40 treasure identities, 59 site resources, ${mapped} site-selector mappings and ${absent} absent descriptors.`);
