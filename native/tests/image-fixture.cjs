'use strict';
// Independent browser decode of real user-ROM image members. Emit hashes and
// offsets only; native tests read the payload from the same private source ROM.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const name of ['parsers.js','cutscene-assets.js'])vm.runInThisContext(fs.readFileSync(path.join(root,name),'utf8'),{filename:name});
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const rows=[],types={};let rejected=0;
for(const archive of OB64.findArchives(rom)){
 const data=OB64.extractArchive(rom,archive);
 if(data[0]!==0x36||data[1]!==0x34)continue;
 let image;try{image=OB64.cutsceneAssets.parseN64Image(data);}catch(e){rejected++;continue;}
 if(!image.renderable)continue;
 const type=`${image.type}/${image.subtype}`;types[type]=(types[type]||0)+1;
 rows.push({offset:archive.offset,width:image.width,height:image.height,
  sha256:crypto.createHash('sha256').update(image.rgba).digest('hex')});
}
if(!rows.length)throw Error('No native image fixtures found');
fs.writeFileSync(process.argv[3],JSON.stringify({rows,types,rejected}));
console.log(`Browser decoded ${rows.length} native images: ${JSON.stringify(types)}; ${rejected} malformed containers rejected.`);
