"use strict";
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips(),dir=process.argv[2],mode=process.argv[3],payload=0x80350000,descriptor=0x80187444;
if(mode==='generate'){
 execFileSync(process.execPath,[path.join(__dirname,'save-unit-mips.cjs'),dir,'generate']);
 const original=fs.readFileSync(path.join(dir,'unit-ram.bin'));
 for(let id=1;id<=9;++id){original[0x193bc0+id*56+17]=5;original[0x193bc0+id*56+18]=5;}
 const variant=(name,edit)=>{m.load(0x80000000,original);m.set('sp',0x803f0000);edit();m.set('a0',payload);m.set('a1',descriptor);
  m.call(0x80074494,0x80000100,5000000);fs.writeFileSync(path.join(dir,name),Buffer.from(m.ram.slice(m.phys(payload),m.phys(payload)+6180)));};
 variant('unit-payload.bin',()=>{});
 variant('leader-no-command.bin',()=>{m.w8(0x80193bc0+112+17,2);m.w8(0x80193bc0+112+18,2);});
 variant('leader-dead.bin',()=>m.w16(0x80193bc0+112+24,0));
 variant('leader-incapacitated.bin',()=>m.w8(0x80193bc0+112+51,4));
 for(const primary of [81,82,83,163])variant(`fixed-leader-${primary}.bin`,()=>m.w8(0x80193bc0+56+17,primary));
 for(const [name,primary,secondary]of[['commander-good',33,33],['commander-bad',5,5],['commander-pair',33,2],['commander-new',165,165]])
  variant(`${name}.bin`,()=>{m.w8(0x801971f1,3);m.load(0x801969b8,Buffer.from([1,1,0,255,255,255,255,0,0,2,2]));m.w8(0x80193bc0+112+17,primary);m.w8(0x80193bc0+112+18,secondary);});
 variant('leader-conflict.bin',()=>m.w8(0x80193bc0+112+51,2));
 variant('leader-duplicate.bin',()=>m.w8(0x801971f0+5*25+3,2));
 variant('leader-missing.bin',()=>m.w8(0x80193bc0+112+17,0));
 console.log('Generated leader fixtures with the original save encoder.');process.exit(0);
}
assert.equal(mode,'verify');const data=JSON.parse(fs.readFileSync(path.join(dir,'leader-verify.json')));
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const live=at=>(at+0x801135a0)>>>0,root=0x80260000,saved=0x80310000,active=root+0x117c,roster=0x80193bc0;
m.load(live(0xb0000),rom.subarray(0xb0000,0xe6000));
for(const[a,b]of[[0xb7910,0xb7978],[0xbea34,0xbea90],[0xbd154,0xbd26c],[0xc188c,0xc192c],[0xc1d98,0xc1db4],[0xc1ed0,0xc1f58]])m.load(live(a),rom.subarray(a,b));
const read=(at,n)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+n));m.set('sp',0x803f0000);
for(const row of data.fixed){if(row.member<100)m.w8(roster+row.member*56+17,row.primary);m.set('a0',row.member);m.call(live(0xbea34));assert.equal(m.get('v0'),row.fixed);}
m.load(0x80187c14,Buffer.from(data.classes,'hex'));
for(const [primary,secondary,type]of data.types){m.set('a0',primary);m.set('a1',secondary);m.call(0x8016dec4);assert.equal(m.get('v0'),type,'paired command type');}
for(const [cls,hp,flags,expected]of[[5,90,0,0xffffffff],[2,90,0,9],[5,0,0,11],[5,90,4,11]]){
 m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.w8(roster+112+17,cls);m.w8(roster+112+18,cls);m.w16(roster+112+24,hp);m.w8(roster+112+51,flags);
 m.set('a0',2);m.set('a1',0x100);m.set('a2',5);m.call(live(0xc01dc));assert.equal(m.get('v0')>>>0,expected,'original replacement-leader eligibility');
}
for(const row of data.cases){const before=Buffer.from(row.before,'hex'),after=Buffer.from(row.after,'hex');
 m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.load(roster,Buffer.from(row.characters,'hex'));
 m.load(saved,before);m.load(active,Buffer.alloc(54,255));m.w8(active+31,before[12]);
 m.set('s4',saved);m.set('s2',active);m.set('s8',8);m.call(live(0xc188c),live(0xc192c));
 m.set('a0',0);m.set('a1',row.cell);m.call(live(0xb7910));
 assert.equal(m.r8(active+3),row.cell);assert.equal(m.r8(active+31),before[12],'leader setter preserves Unity');assert.equal(m.r8(active+29),after[24]);
 m.set('s2',0);m.set('v1',roster);m.call(live(0xc1d98),live(0xc1db4));
 m.set('s0',saved);m.set('s1',active);m.set('s3',255);m.set('s6',roster);m.call(live(0xc1ed0),live(0xc1f58));
 assert.deepEqual(read(saved+2,10),after.subarray(2,12),'native leader-first and grid-order serialization');
 const flags=Buffer.from(row.characters,'hex');const old=before[2],next=after[2];
 assert.equal(m.r8(roster+old*56+51),flags[old*56+51]&~2);assert.equal(m.r8(roster+next*56+51),flags[next*56+51]|2);
}
for(const row of data.saves){m.load(payload,Buffer.from(row.payload,'hex'));m.set('a0',payload);m.set('a1',descriptor);m.call(0x800742f4,0x80000100,5000000);
 assert.deepEqual(read(roster,5600),Buffer.from(row.characters,'hex'));assert.deepEqual(read(0x801971f0,750),Buffer.from(row.units,'hex'));}
console.log(`PASS 4 original eligibility checks, ${data.fixed.length} fixed-leader cases, ${data.types.length} class pairs, ${data.cases.length} setter/writer sequences and ${data.saves.length} original save decodes. Synthetic Army allocation; no natural menu execution.`);
