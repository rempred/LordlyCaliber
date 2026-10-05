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
fs.writeFileSync(process.argv[2],JSON.stringify({versions}));
