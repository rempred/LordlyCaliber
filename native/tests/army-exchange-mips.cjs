"use strict";
// Direct Army exchange and its actual scratch-to-squad commit; no menu/input emulation.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips(),dir=process.argv[2],mode=process.argv[3],payload=0x80350000,descriptor=0x80187444;
const roster=0x80193bc0,squads=0x801971f0,soldiers=0x80190f80;
if(mode==='generate'){
 execFileSync(process.execPath,[path.join(__dirname,'save-unit-mips.cjs'),dir,'generate']);
 const original=fs.readFileSync(path.join(dir,'unit-ram.bin'));
 const variant=(name,edit)=>{m.load(0x80000000,original);m.set('sp',0x803f0000);
  m.w8(roster+17,44);m.w8(roster+18,44);m.w8(squads+2*25+24,7);m.w8(squads+3*25+24,7);edit();
  m.set('a0',payload);m.set('a1',descriptor);m.call(0x80074494,0x80000100,5000000);
  fs.writeFileSync(path.join(dir,name),Buffer.from(m.ram.slice(m.phys(payload),m.phys(payload)+6180)));};
 variant('exchange.bin',()=>{});
 variant('exchange-duplicate.bin',()=>m.w8(squads+3*25+3,100));
 variant('exchange-zero-hp.bin',()=>m.w16(soldiers+4,0));
 variant('exchange-overload.bin',()=>{for(let i=13;i<16;++i)m.w8(squads+2*25+i,9);});
 variant('exchange-missing.bin',()=>m.w8(roster+2*56+17,0));
 variant('exchange-leader-flag.bin',()=>m.w8(roster+2*56+51,2));
 console.log('Generated direct-exchange fixtures with original save encoder.');process.exit(0);
}
assert.equal(mode,'verify');const data=JSON.parse(fs.readFileSync(path.join(dir,'exchange-verify.json')));
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const live=at=>(at+0x801135a0)>>>0,root=0x80260000,saved=0x80310000;
for(const[a,b]of[[0xb7978,0xb7b5c],[0xb3210,0xb3438],[0xb8300,0xb848c],[0xd6fa8,0xd7030],
 [0xbd154,0xbd26c],[0xbcee4,0xbd0d8],[0xc188c,0xc192c],[0xc1ed0,0xc1f58]])m.load(live(a),rom.subarray(a,b));
const read=(at,n)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+n));
const pairs=r=>Array.from({length:5},(_,i)=>[r[2+i],r[7+i]]).filter(x=>x[0]).sort((a,b)=>a[0]-b[0]);
for(const row of data.cases){const before=Buffer.from(row.before,'hex'),after=Buffer.from(row.after,'hex');
 m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.set('sp',0x803f0000);
 m.load(roster,Buffer.from(row.characters,'hex'));m.load(soldiers,Buffer.from(row.soldiers,'hex'));
 // Two scratch squads are the inputs used by the original exchange menu.
 for(const [index,temp]of [[row.from,30],[row.to,31]]){const record=before.subarray(index*25,index*25+25),active=root+0x117c+temp*54;
  m.load(saved,record);m.load(active,Buffer.alloc(54,255));m.w8(active+1,0);
  m.w8(active+31,record[12]);m.w8(active+29,record[24]);m.w8(active+30,record[23]);
  for(let i=0;i<10;++i)m.w16(active+34+i*2,record[13+i]);
  m.set('s4',saved);m.set('s2',active);m.set('s8',8);m.call(live(0xc188c),live(0xc192c));
 }
 const oldSoldiers=read(soldiers,364),oldRoster=read(roster,5600);
 m.set('a0',30);m.set('a1',row.fromCell);m.set('a2',31);m.set('a3',row.toCell);m.call(live(0xb7978));
 m.w16(root+402,row.from);m.w16(root+404,row.to);
 m.call(live(0xd6fa8),live(0xd7030));
 assert.deepEqual(read(soldiers,364),oldSoldiers,'direct exchange preserves HP, companion and reserve');
 assert.deepEqual(read(roster,5600),oldRoster,'direct exchange preserves ordinary roster');
 for(const index of [row.from,row.to]){const active=root+0x117c+index*54,record=after.subarray(index*25,index*25+25);
  assert.equal(m.r8(active+31),record[12],'direct exchange and commit retain Unity');
  assert.equal(m.r8(active+29),record[24],'original movement finalizer');
  assert.equal(m.r8(active+3),record[7],'leader stays in its grid cell');
  m.load(saved,Buffer.alloc(25));m.set('s0',saved);m.set('s1',active);m.set('s3',255);m.set('s6',roster);
  m.call(live(0xc1ed0),live(0xc1f58));assert.deepEqual(pairs(read(saved,25)),pairs(record),'persistent member/cell pairs');
  const actual=Array.from({length:10},(_,i)=>m.r16(active+34+i*2));
  const supplies=[...record.subarray(13,23)].filter(Boolean);while(supplies.length<10)supplies.push(0);
  assert.deepEqual(actual,supplies,'original finalizer retains carried supplies');
 }
}
for(const row of data.saves){m.load(payload,Buffer.from(row.payload,'hex'));m.set('sp',0x803f0000);m.set('a0',payload);m.set('a1',descriptor);
 m.call(0x800742f4,0x80000100,5000000);assert.deepEqual(read(soldiers,364),Buffer.from(row.soldiers,'hex'));
 assert.deepEqual(read(squads,750),Buffer.from(row.units,'hex'));assert.deepEqual(read(roster,5600),Buffer.from(row.characters,'hex'));}
console.log(`PASS ${data.cases.length} direct exchange/commit/finalizer sequences and ${data.saves.length} original save decodes. Synthetic Army allocation; no natural menu run.`);
