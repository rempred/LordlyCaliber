'use strict';
// Generates addresses and hashes, never game instruction bytes. Input is the
// user's unmodified US Rev 0 ROM; boundaries are original boot function extents.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const rom=fs.readFileSync(process.argv[2]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const hash=data=>crypto.createHash('sha256').update(data).digest('hex');
if(hash(rom)!=='571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a')throw Error('US Rev 0 ROM required');
const rows=[
  [0x4C5C,0x4DC0,'lc_save_game'],[0x4DC0,0x4ED4,'lc_load_game'],[0x4ED4,0x4FF0,'lc_save_summary'],
  [0x50F0,0x51A0,'lc_save_copy'],[0x51A0,0x539C,'lc_save_headers_missing'],
  [0x539C,0x553C,'lc_save_initialize_slot'],[0x553C,0x5624,'lc_save_suspend'],
  [0x5624,0x5760,'lc_load_suspend'],[0x5760,0x581C,'lc_save_check_slot'],
  [0x581C,0x5978,'lc_save_read_normal'],[0x5978,0x5A88,'lc_save_read_suspend'],
  [0x5A88,0x5B8C,'lc_save_read_options'],[0x5B8C,0x5C58,'lc_save_write_normal'],
  [0x5C58,0x5CFC,'lc_save_write_suspend'],[0x5CFC,0x5D9C,'lc_save_write_options'],
  [0x5D9C,0x5E84,'lc_save_sign']
];
const owners=rows.map(([start,end,symbol])=>({start,size:end-start,symbol,retailSha256:hash(rom.subarray(start,end))}));
const callbacks={start:0x3864C,size:13*28,retailSha256:hash(rom.subarray(0x3864C,0x3864C+13*28))};
// Placement from full-function direct signatures in two captured inputs of the
// parent overlay atlas: input-0499/0503, validated slabs 4705/4763 (42 anchors).
// Both map z64 74F90 to RAM8019E4B0, delta80129520; not the early-boot delta.
const pakOwners=[
  {start:0x74A9C,size:0x4F4,fragment:0x74CB4,fragmentSize:0xD4,source:'pak-import.s',
   symbol:'lc_pak_import',resume:0x74D88,reject:0x74E14},
  {start:0x74F90,size:0x370,fragment:0x75028,fragmentSize:0xF0,source:'pak-export.s',
   symbol:'lc_pak_export',resume:0x75118,reject:0x752D0}
].map(row=>({...row,delta:0x80129520,retailSha256:hash(rom.subarray(row.start,row.start+row.size))}));
fs.writeFileSync(path.join(__dirname,'../game/classes/save-owners.json'),JSON.stringify({
  generator:'native/tools/generate-save-runtime-catalog.cjs',input:'US Rev 0 z64 SHA256 '+hash(rom),owners,callbacks,pakOwners
},null,2)+'\n');
