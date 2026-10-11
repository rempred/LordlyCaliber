'use strict';
// Full owner hashes and individual preimages, no executable payload. Input is
// the private US Rev 0 ROM. Shop slab mapping is ROM1977e0 -> RAM80214f80.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const rom=fs.readFileSync(process.argv[2]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
if(hash(rom)!=='571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a')throw Error('US Rev 0 ROM required');
const patches=[
  [0x1978d8,'store'],[0x1978dc,'stride'],[0x197958,'load'],
  [0x197da4,'index'],[0x197db0,'load'],[0x198020,'index'],[0x19802c,'load'],
  [0x198cdc,'index'],[0x198ce8,'offset'],[0x198f58,'index'],[0x198f60,'load'],
  [0x199550,'index'],[0x19955c,'offset'],[0x19a330,'index'],[0x19a33c,'load']
].map(([site,kind])=>({site,kind,original:rom.readUInt32BE(site)}));
for(const [site,kind] of [[0x19795c,'nop'],[0x197d6c,'guardCall'],[0x197d70,'guardArgs'],[0x197d74,'guardBranch']])
  patches.push({site,kind,version:2,original:rom.readUInt32BE(site)});
const owners=[[0x1977e0,0x1989c4],[0x1989c4,0x1994b4],[0x1994b4,0x19a1ac],
  [0x19a294,0x19aa04],[0x19be40,0x19bff0]].map(([start,end])=>{
  const bytes=Buffer.from(rom.subarray(start,end));
  const retailSha256=hash(bytes);
  if(start===0x19be40)bytes.fill(0,0xd8,0x184);
  return {start,size:end-start,retailSha256,maskedSha256:hash(bytes)};
});
if(!rom.subarray(0x27c4000,0x27c4040).every(b=>b===255))throw Error('Shop descriptor slot occupied');
fs.writeFileSync(path.join(__dirname,'../game/shops/owners.json'),JSON.stringify({
  generator:'native/tools/generate-shop-catalog.cjs',input:'US Rev 0 z64 SHA256 '+hash(rom),
  owners,patches
},null,2)+'\n');
