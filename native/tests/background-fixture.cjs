'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['parsers.js','cutscene-data.js','cutscene-model.js','cutscene-njpg.js','cutscene-assets.js','art.js'])
 vm.runInThisContext(fs.readFileSync(path.join(root,f),'utf8'),{filename:f});
const b=fs.readFileSync(process.argv[2]);if(b.readUInt32BE(0)===0x37804012)b.swap16();
const out=process.argv[3],rows=[],types={},hash=v=>crypto.createHash('sha256').update(v).digest('hex');
const archives=OB64.findArchives(b);
for(const asset of OB64.cutsceneData.imageAssets){
 if(!['64','bg2','K','embedded-njpg','section-c-njpg'].includes(asset.container))continue;
 let bytes,source;
 if(asset.sourceKind==='lha-archive'){
  const a=archives[asset.archiveIndex];bytes=Buffer.from(OB64.extractArchive(b,a));source={kind:'lha',offset:a.offset};
 }else if(asset.sourceKind==='rom-resource'){
  const compressed=asset.source.compressionKind==='ob64-custom-lz';
  const r=compressed?OB64.art.readCompressedResource(b,asset.source.resourceKey):OB64.art.readResource(b,asset.source.resourceKey);
  bytes=Buffer.from(compressed?r.decoded:r.stored);source={kind:compressed?'custom':'stored',offset:asset.source.resourceKey+0x594280};
 }else {bytes=b.subarray(asset.source.z64Start,asset.source.z64EndExclusive);source={kind:'slice',offset:asset.source.z64Start,size:bytes.length};}
 if(asset.source.decodedSha256)assert.equal(hash(bytes),asset.source.decodedSha256.toLowerCase());
 const parsed=OB64.cutsceneAssets.parseImageAsset(bytes,asset);
 assert(parsed.renderable);
 const huff=asset.container.endsWith('njpg')||parsed.records?.some(r=>r.embeddedContainer);
 const name=`pixels-${rows.length}.rgba`;if(huff)fs.writeFileSync(path.join(out,name),Buffer.from(parsed.rgba));
 rows.push({id:asset.assetId,container:asset.container,source,width:parsed.width,height:parsed.height,
  originX:parsed.originX||0,originY:parsed.originY||0,sha256:hash(parsed.rgba),pixels:huff?name:null,
  layers:(parsed.records||[]).map(r=>({x:r.x,y:r.y,width:r.width,height:r.height}))});
 types[asset.container]=(types[asset.container]||0)+1;
}
fs.writeFileSync(path.join(out,'backgrounds.json'),JSON.stringify({rows,types}));
console.log(`Independent background references: ${rows.length}; ${JSON.stringify(types)}`);
