"use strict";
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips(),dir=process.argv[2],mode=process.argv[3],payload=0x80350000,descriptor=0x80187444;
const inventory=0x80193ac0,squads=0x801971f0;
if(mode==='generate'){
 execFileSync(process.execPath,[path.join(__dirname,'save-unit-mips.cjs'),dir,'generate']);
 const original=fs.readFileSync(path.join(dir,'unit-ram.bin'));
 const variant=(name,edit)=>{m.load(0x80000000,original);m.set('sp',0x803f0000);edit();m.set('a0',payload);m.set('a1',descriptor);
  m.call(0x80074494,0x80000100,5000000);fs.writeFileSync(path.join(dir,name),Buffer.from(m.ram.slice(m.phys(payload),m.phys(payload)+6180)));};
 variant('stock-two.bin',()=>m.w8(inventory+23*4+3,2));
 variant('stock-empty.bin',()=>m.w8(inventory+23*4+3,0));
 variant('stock-missing.bin',()=>m.w16(inventory+23*4,0));
 variant('stock-duplicate.bin',()=>{m.w16(inventory+31*4,24);m.w8(inventory+31*4+3,2);});
 variant('stock-source-overassigned.bin',()=>m.w8(inventory+8*4+3,1));
 console.log('Generated squad stock fixtures through original save encoder.');process.exit(0);
}
assert.equal(mode,'verify');const data=JSON.parse(fs.readFileSync(path.join(dir,'stock-verify.json')));
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const live=at=>(at+0x801135a0)>>>0;m.load(live(0xcba10),rom.subarray(0xcba10,0xcba30));
const read=(at,n)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+n));
// The assigned byte is runtime bookkeeping and is not encoded in battery saves.
for(const row of data.cases){m.load(inventory,Buffer.alloc(160));m.load(payload,Buffer.from(row.payload,'hex'));m.set('sp',0x803f0000);m.set('a0',payload);m.set('a1',descriptor);m.call(0x800742f4,0x80000100,5000000);
 assert.deepEqual(read(squads,750),Buffer.from(row.units,'hex'));const owned=read(inventory,160);assert.deepEqual(owned,Buffer.from(row.inventory,'hex'));
 m.set('sp',0x803f0000);m.call(0x8016b774,0x8016b874);
 for(let i=0;i<40;++i){const id=m.r16(inventory+i*4);assert.equal(m.r8(inventory+i*4+2),row.assigned[i],'original assigned-byte rebuild');
  assert.equal(m.r8(inventory+i*4+3),owned[i*4+3],'owned count is unchanged');
  if(id){m.set('v0',i*4);m.call(live(0xcba10),live(0xcba4c));assert.equal(Math.max(0,m.get('v1')|0),row.available[i],'Army owned-minus-assigned availability');}}
}
console.log(`PASS ${data.cases.length} original save decodes, inventory assignment rebuilds and per-item Army availability sequences. No natural inventory/menu execution.`);
