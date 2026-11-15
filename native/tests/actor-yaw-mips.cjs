'use strict';
// Original US Rev0 yaw setter, at its documented relocated overlay address.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const data=JSON.parse(fs.readFileSync(process.argv[3]));
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80231de0,rom.subarray(0x2a1610,0x2a1690));
const root=0x80300000,sp=0x803fff00,addresses=[0x80303000,0x80304000,0x80305000];
for(const c of data.cases){
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.set('sp',sp);
 data.slots.forEach((slot,i)=>{m.w32(root+24+slot*4,addresses[i]);m.load(addresses[i]-8,Buffer.alloc(352,0xa7));m.load(addresses[i],Buffer.from(data.initial[i],'hex'));});
 m.set('a0',c.selector);m.set('a1',0x7fc00000);m.setSingle(0,c.degrees);m.set('a2',m.fp[0]);m.set('a3',0x7fc00000);m.call(0x80231de0);
 c.records=addresses.map(p=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+336)).toString('hex'));
 for(const p of addresses)for(let i=0;i<8;i++){assert.equal(m.r8(p-8+i),0xa7);assert.equal(m.r8(p+336+i),0xa7);}
 assert.equal(m.get('sp'),sp);
}
fs.writeFileSync(process.argv[4],JSON.stringify(data));
