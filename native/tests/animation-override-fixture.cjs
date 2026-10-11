'use strict';
// Test-only compatibility fixtures from the browser's three historical formats.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..'),context={window:{OB64:{}}};context.OB64=context.window.OB64;vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root,'combat-animation-overrides-data.js'),'utf8'),context);
const d=context.OB64.COMBAT_ANIMATION_OVERRIDE_DATA,L=d.layout;
const versions=[d.legacyV1,d.legacyV2,d].map((version,i)=>{
  const module=Buffer.from(version.moduleTemplateHex,'hex'),table=module.subarray(0x400);
  // Two wildcard records; the second uses an exact body route in v3.
  table.set([2,2,5,0,0,40,i?41:0,0],0);
  table.set([85,85,30,i===2?2:0,0,42,i?43:0,i===2?1:0],8);
  module.writeUInt16BE(2,12);crypto.createHash('sha256').update(table).digest().copy(module,0x20);
  const boot=Buffer.alloc(L.bootstrapSize);Buffer.from(version.bootstrapHex,'hex').copy(boot);
  const regions=[{offset:L.bootstrapZ64,bytes:boot.toString('hex')},{offset:L.moduleZ64,bytes:module.toString('hex')},
    {offset:L.hookZ64,bytes:version.ownedHookHex},{offset:L.selectorReturnZ64,bytes:i?d.selectorReturnPostimageHex:d.selectorReturnPreimageHex},
    ...d.carrierPatches.map(p=>({offset:p.z64,bytes:i?p.postimageHex:p.preimageHex}))];
  return {version:i+1,regions};
});
const candidates=d.classes.map(c=>({id:c.id,owner:c.rawOwner,selectors:c.selectors.map(s=>s.id)}));
if(process.argv[3]){
  for(const file of ['parsers.js','cutscene-data.js','cutscene-model.js','cutscene-assets.js','art.js','animation-art.js'])
    vm.runInContext(fs.readFileSync(path.join(root,file),'utf8'),context,{filename:file});
  const rom=fs.readFileSync(process.argv[3]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();else if(rom.readUInt32BE(0)===0x40123780)rom.swap32();
  const A=context.OB64.art,raw=key=>Buffer.from(A.readResource(rom,key).stored),decoded=key=>Buffer.from(A.readCompressedResource(rom,key).decoded);
  const handles=raw(0x315736),directory=raw(0x3b6cd0),base=handles.readUInt16BE(0),cache=new Map();
  for(const row of candidates){
    row.currentSelectors=[];
    for(let selector=40;selector<50;selector++){
      let valid=true;
      for(let flags=0;flags<4;flags++){
        const handle=handles.readUInt16BE(base+row.id*8+flags*2),descriptor=directory.readUInt32BE(((handle&4095)-1)*4);
        if(!cache.has(descriptor))cache.set(descriptor,decoded(raw(descriptor).readUInt32BE(4)));
        const poses=cache.get(descriptor),count=poses.readUInt32BE(0)/4;
        if(selector>=count){valid=false;break;}
        const start=poses.readUInt32BE(selector*4),end=selector+1<count?poses.readUInt32BE(selector*4+4):poses.length;
        if(start===end||!context.OB64.animationArt.parsePoseProgram(poses,selector,'body').records.length){valid=false;break;}
      }
      if(valid)row.currentSelectors.push(selector);
    }
  }
}
fs.writeFileSync(process.argv[2],JSON.stringify({versions,candidates}));
