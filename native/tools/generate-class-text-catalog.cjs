'use strict';
// Addresses and hashes only. Both resource-key readers are in the resident DMA
// segment (ROM40E80 -> RAM8016AF80); no early-boot address conversion applies.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const rom=fs.readFileSync(process.argv[2]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const hash=data=>crypto.createHash('sha256').update(data).digest('hex');
if(hash(rom)!=='571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a')throw Error('US Rev 0 ROM required');
const owners=[
  {start:0x55B28,size:0x98C,high:0x8DC,low:0x8E0},
  {start:0x66D4C,size:21*4,word:20}
].map(row=>({...row,retailSha256:hash(rom.subarray(row.start,row.start+row.size))}));
if(rom.readUInt32BE(0x56404)!==0x3C0501DE || rom.readUInt32BE(0x56408)!==0x34A545D4 ||
   rom.readUInt32BE(0x66D60)!==0x01DE45D4)throw Error('Unexpected class text resource references');
fs.writeFileSync(path.join(__dirname,'../game/classes/text-owners.json'),JSON.stringify({
  generator:'native/tools/generate-class-text-catalog.cjs',input:'US Rev 0 z64 SHA256 '+hash(rom),owners
},null,2)+'\n');
