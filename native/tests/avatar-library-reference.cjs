'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
global.window=global;global.OB64={};
vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const key=((rom.readUInt32BE(0x45afc)&65535)*65536)+(rom.readUInt32BE(0x45b00)&65535);
const directory=Buffer.from(OB64.art.readResource(rom,key).stored),variants=[],seen=new Set();
for(let member=1;member<directory.length/4;++member){
 const key=directory.readUInt32BE(member*4);if(seen.has(key))continue;seen.add(key);
 const source=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded);
 for(let palette=0;palette<(source.length-1920)/160;++palette){
  const words=Buffer.alloc(3840),rgba=Buffer.alloc(7680);
  for(let pixel=0;pixel<1920;++pixel){const word=source.readUInt16BE(1920+palette*160+source[pixel]*2);words.writeUInt16BE(word,pixel*2);rgba.set(OB64.art.rgba5551(word),pixel*4);}
  variants.push({token:member*8+palette,words:words.toString('base64'),rgba:rgba.toString('base64')});
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({members:directory.length/4,bundles:seen.size,variants}));
console.log(`${seen.size} portrait bundles; ${variants.length} palette variants`);
