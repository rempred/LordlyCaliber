'use strict';
// Existing browser decoder supplies independent current-ROM RGBA expectations.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
global.window=global;global.OB64={};
vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const icons=[];
const keyAt=(hi,lo)=>((rom.readUInt32BE(hi)&65535)*65536)+(rom.readUInt32BE(lo)&65535);
for(const [key,count,flag] of [[keyAt(0x2a6640,0x2a664c),277,0],[keyAt(0x2a6624,0x2a6634),44,0x200]]){
 const bytes=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded);
 for(let id=1;id<=count;++id){
  const rgba=Buffer.alloc(1024);
  for(let pixel=0;pixel<256;++pixel){
   const word=bytes.readUInt16BE(bytes[512+(id-1)*256+pixel]*2);
   const color=OB64.art.rgba5551(word);
   rgba.set(color,pixel*4);
  }
  icons.push({item:flag|id,rgba:rgba.toString('hex')});
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({icons}));console.log(`${icons.length} current-ROM item icons`);
