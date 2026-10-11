"use strict";
// Original Army helper/field blocks, with synthetic menu allocation and no emulator.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const dir=process.argv[2],mode=process.argv[3],m=new Mips();
const payload=0x80350000,descriptor=0x80187444,delta=0x801135a0,live=at=>(at+delta)>>>0;
if(mode==='generate'){
 execFileSync(process.execPath,[path.join(__dirname,'save-unit-mips.cjs'),dir,'generate']);
 const original=fs.readFileSync(path.join(dir,'unit-ram.bin'));
 const variant=(name,edit)=>{m.load(0x80000000,original);m.set('sp',0x803f0000);edit();
  m.set('a0',payload);m.set('a1',descriptor);m.call(0x80074494,0x80000100,5000000);
  fs.writeFileSync(path.join(dir,name),Buffer.from(m.ram.slice(m.phys(payload),m.phys(payload)+6180)));};
 for(const pool of [0,1,2,255])variant(`soldier-pool-${pool}.bin`,()=>{m.w8(0x80190f81,pool);m.w16(0x80190f82,301);});
 variant('soldier-zero-maximum.bin',()=>m.w16(0x80190f82,0));
 variant('soldier-no-free.bin',()=>{for(let i=0;i<120;++i)m.w16(0x80190f84+i*2,1);});
 variant('soldier-free-assigned.bin',()=>m.w16(0x80190f84,0));
 variant('soldier-duplicate.bin',()=>m.w8(0x801971f0+3*25+3,100));
 console.log('Generated Soldier reserve fixtures through original packed-save encoder.');process.exit(0);
}
assert.equal(mode,'verify');const data=JSON.parse(fs.readFileSync(path.join(dir,'soldier-verify.json')));
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
for(const[a,b]of[[0xb86c4,0xb876c],[0xb7ea4,0xb7fd8],[0xbaad4,0xbab6c],[0xb81cc,0xb8258]])m.load(live(a),rom.subarray(a,b));
const root=0x80260000;const read=(at,n)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+n));
for(const row of data.counts){m.w16(0x80190f82,row.maximum);m.w16(0x80190f84,row.hp);m.set('sp',0x803f0000);m.set('a0',100);
 m.call(live(0xb86f8));assert.equal(m.get('v0'),row.count,'original HP-derived count');}
for(const row of data.free){row.hp.forEach((hp,i)=>m.w16(0x80190f84+i*2,hp));m.call(live(0xb86c4));assert.equal(m.get('v0'),row.id,'original free group ID');}
for(const row of data.creation){const id=219;m.w8(0x80190f81,row.pool);m.w16(0x80190f82,row.maximum);
 m.w16(0x80190f84+119*2,0);m.w8(0x80191074+119,0xa7);
 m.set('a2',0x80190f81);m.set('a1',id*2);m.set('t0',id);m.set('s7',0);
 m.call(live(0xb81cc),live(0xb82b0));assert.equal(m.r8(0x80190f81),row.remaining);
 assert.equal(m.r16(0x80190f84+119*2),row.hp);assert.equal(m.r8(0x80191074+119),0);}
for(const row of data.removal){const id=row.id,unit=2,cell=7,active=root+0x117c+unit*54;
 m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.w8(active+3,1);m.w8(active+4+cell,id);m.w8(active+31,23);
 m.w8(root+0x1872+id*2,0xc2);m.w8(0x80190f81,row.pool);m.w16(0x80190f82,row.maximum);
 m.w16(0x80190f84+(id-100)*2,row.hp);m.w8(0x80191074+id-100,5);
 m.set('s2',id);m.set('s3',unit);m.set('s4',1);m.set('s5',cell);m.set('s7',0);m.set('sp',0x803f0000);
 m.call(live(0xb7ea4),live(0xb82b0));assert.equal(m.r8(0x80190f81),row.remaining);
 assert.equal(m.r16(0x80190f84+(id-100)*2),0);assert.equal(m.r8(0x80191074+id-100),0);
 assert.equal(m.r8(active+4+cell),255);assert.equal(m.r8(active+31),23,'Soldier removal retains Unity');}
for(const row of data.saves){m.load(payload,Buffer.from(row.payload,'hex'));m.set('sp',0x803f0000);m.set('a0',payload);m.set('a1',descriptor);
 m.call(0x800742f4,0x80000100,5000000);
 assert.deepEqual(read(0x80190f80,364),Buffer.from(row.soldiers,'hex'),'original decoder Soldier pool, HP and state');
 assert.deepEqual(read(0x801971f0,750),Buffer.from(row.units,'hex'),'original decoder squad records');}
console.log(`PASS ${data.counts.length} HP counts, ${data.free.length} free-ID scans, ${data.creation.length} creation cases, ${data.removal.length} removals, ${data.saves.length} packed saves. Synthetic menu allocation; no natural game launch.`);
