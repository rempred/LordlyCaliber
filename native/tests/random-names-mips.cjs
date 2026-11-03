'use strict';
// Execute the exported naming owner, with resource/RNG/string services bounded
// at their native call boundaries. The ROM is produced by nameTablesExport.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
assert.equal(rom.subarray(0x5dc80,0x5dc88).toString(),'LC05CLAS');
const lists=[];
for(let pool=0;pool<3;pool++) {
 const bytes=Buffer.from(OB64.art.readCompressedResource(rom,rom.readUInt32BE(0x65dac+pool*4)).decoded);
 assert.equal(bytes.subarray(0,4).toString('hex'),'00524e31');
 const count=bytes.readUInt32BE(4),names=[];let at=8;
 assert.equal(count,[257,258,1024][pool]);
 for(let id=0;id<count;id++){const end=bytes.indexOf(0,at);assert(end>at&&end-at<=12);names.push(bytes.subarray(at,end).toString());at=end+1;}
 assert.equal(names[256],`Extra${pool}_256`);lists.push(names);
}
function run(pool,side,randomValue,duplicate=false) {
 const m=new FloatMips();m.ram=new Uint8Array(0x400000);
 m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
 const d=rom.subarray(0x5dc80),data=rom.subarray(d.readUInt32BE(12),d.readUInt32BE(12)+d.readUInt32BE(16));m.load(d.readUInt32BE(24),data);
 const expected=lists[pool][randomValue%lists[pool].length];let allocations=0,frees=0,draws=0;
 m.trap(0x8009dd38,c=>{const bytes=OB64.art.readCompressedResource(rom,c.get('a0')).decoded;m.load(0x803f0000,bytes);allocations++;c.set('v0',0x803f0000);});
 m.trap(0x8009c7cc,c=>{draws++;c.set('v0',randomValue);});
 m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
 const str=at=>{const b=[];while(m.r8(at)){b.push(m.r8(at++));assert(b.length<=16);}return Buffer.from(b);};
 m.trap(0x8009c550,c=>{m.load(c.get('a0'),Buffer.concat([str(c.get('a1')),Buffer.from([0])]));c.set('v0',c.get('a0'));});
 m.trap(0x8009c5e0,c=>c.set('v0',Buffer.compare(str(c.get('a0')),str(c.get('a1')))));
 m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),0x803f0000,'free original allocation, not header-adjusted text');frees++;});
 const base=side?0x80195560:0x80193bc0,stride=side?52:56;
 const records=Buffer.alloc(stride*100);records.write(duplicate?expected:'ExistingName',stride*10);records[stride*10+17]=5;
 const at=base+stride*2;records[stride*2+17]=5;records[stride*2+18]=5;records[stride*2+20]=pool;m.load(base,records);
 m.set('sp',0x803ffe00);m.set('a0',side);m.set('a1',2);m.call(0x8016bd20,0x80000100,4000000);
 assert.equal(str(at).toString(),expected);assert.equal(m.get('sp'),0x803ffe00);
 const after=Buffer.from(m.ram.subarray(m.phys(base),m.phys(base)+records.length));
 assert.deepEqual(after.subarray(0,2*stride),records.subarray(0,2*stride));
 assert.deepEqual(after.subarray(2*stride+17),records.subarray(2*stride+17));
 assert.equal(draws,duplicate?32:1);assert.equal(allocations,draws);assert.equal(frees,draws);
}
let cases=0;
for(let pool=0;pool<3;pool++)for(let side=0;side<2;side++) {
 for(const random of [0,42,255,256,lists[pool].length-1,lists[pool].length,0x7fffffff]){run(pool,side,random);cases++;}
 run(pool,side,256,true);cases++;
}
console.log(`PASS: ${cases} exported-code cases across three expanded tables and both sides; added/last names, RNG wrap, duplicate retries, resource cleanup and existing characters preserved. No full-console playthrough claim.`);
