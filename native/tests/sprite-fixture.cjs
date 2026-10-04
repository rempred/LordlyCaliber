'use strict';
// Test-only exposure of the prior renderer's private pixel conversion. Its
// decoder and resource reads run unchanged; no browser code ships in the engine.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['parsers.js','cutscene-data.js','cutscene-model.js','cutscene-assets.js','art.js','animation-art.js','cutscene-sprites.js']) {
 let text=fs.readFileSync(path.join(root,f),'utf8');
 if(f==='cutscene-sprites.js')text=text.replace('  OB64.cutsceneSprites = {',
  '  OB64.testSpritePixels = spritePixels; OB64.testPrepareBank = prepareBank; OB64.testLoadSprite = loadSprite;\n  OB64.cutsceneSprites = {');
 vm.runInThisContext(text,{filename:f});
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex'),rows=[],palettes={},seen=new Set();
let children=0;
for(const source of OB64.cutsceneData.actorArtSources){
 const state={z64:rom,banks:{}};
 const bank=OB64.testPrepareBank(state,source),program={bank,program:{selectedChildOrdinal:0}};
 for(let art=0;art<source.artCount;art++){
  const first=OB64.testLoadSprite(state,program,art,0);
  // Use the external bank even when child zero owns an embedded palette.
  const external=Buffer.alloc(512),bankIndex=bank.config.bankMap[art];
  if(bank.palettes[bankIndex])bank.palettes[bankIndex].forEach((w,i)=>external.writeUInt16BE(w,i*2));
  const paletteHash=hash(external),identity=first.resourceKey+':'+paletteHash;
  if(seen.has(identity))continue;seen.add(identity);palettes[paletteHash]=external.toString('hex');
  const sprite=first.sprite,variants=[];
  for(let child=0;child<sprite.childCount;child++){
   const selected=OB64.testLoadSprite(state,program,art,child),lanes=selected.lanes;
   variants.push({color:lanes.first?hash(lanes.first):null,alpha:lanes.second?hash(lanes.second):null,
    palette:lanes.lookup?hash(lanes.lookup):null,rgba:lanes.first?hash(OB64.testSpritePixels(selected)):null});
   children++;
  }
  rows.push({bank:source.bank,art,key:first.resourceKey,width:sprite.width,height:sprite.height,
   colorFormat:sprite.firstFormat,alphaFormat:sprite.secondFormat,paletteHash,variants});
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({rows,palettes,children}));
console.log(`Actor sprite references: ${rows.length} resource/palette pairs, ${children} child variants.`);
