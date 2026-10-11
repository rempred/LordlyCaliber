"use strict";
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
// Same big-endian SWL needed by the original bzero as in army-create-mips.cjs.
class ArmyMips extends Mips{exec(pc,next){const w=this.r32(pc);if((w>>>26)!==42)return super.exec(pc,next);
 const a=(this.r[(w>>>21)&31]+((w<<16)>>16))>>>0,v=this.r[(w>>>16)&31];for(let i=0;i<4-(a&3);++i)this.w8(a+i,v>>>(24-i*8));return[next,(next+4)>>>0];}}
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new ArmyMips(),dir=process.argv[2],mode=process.argv[3],payload=0x80350000,descriptor=0x80187444;
const roster=0x80193bc0,squads=0x801971f0,order=0x801977d8,root=0x80260000,inventory=0x80193ac0,soldiers=0x80190f80;
if(mode==='generate'){
 execFileSync(process.execPath,[path.join(__dirname,'army-create-mips.cjs'),dir,'generate']);
 m.load(0x80000000,fs.readFileSync(path.join(dir,'unit-ram.bin')));m.load(payload,fs.readFileSync(path.join(dir,'create.bin')));
 m.set('sp',0x803f0000);m.set('a0',payload);m.set('a1',descriptor);m.call(0x800742f4,0x80000100,5000000);const original=Buffer.from(m.ram);
 const variant=(name,edit)=>{m.load(0x80000000,original);m.set('sp',0x803f0000);edit();m.set('a0',payload);m.set('a1',descriptor);m.call(0x80074494,0x80000100,5000000);
  fs.writeFileSync(path.join(dir,name),Buffer.from(m.ram.slice(m.phys(payload),m.phys(payload)+6180)));};
 variant('disband.bin',()=>{});
 variant('disband-multi.bin',()=>{m.w8(squads+2*25+4,101);m.w8(squads+2*25+9,0);m.w16(soldiers+6,200);m.w8(0x80191075,5);});
 variant('disband-last.bin',()=>{for(let i=0;i<30;++i)if(i!==2)m.w8(squads+i*25+1,m.r8(squads+i*25+1)&~1);
  m.load(order,Buffer.alloc(30,31));m.w8(order+2,0);for(const id of[1,5,6,7])m.w8(roster+id*56+51,0);});
 variant('disband-overflow.bin',()=>m.w8(soldiers+1,255));
 variant('disband-duplicate.bin',()=>m.w8(squads+3*25+3,100));
 variant('disband-fixed.bin',()=>m.w8(roster+4*56+17,81));
 variant('disband-missing.bin',()=>m.w8(roster+4*56+17,0));
 variant('disband-flags.bin',()=>m.w8(roster+2*56+51,2));
 variant('disband-legion.bin',()=>{m.w8(0x801969b9,1);[2,5,31,31,31].forEach((v,i)=>m.w8(0x801969ba+i,v));m.w8(squads+2*25+1,3);});
 variant('disband-order.bin',()=>m.w8(order+2,2));
 console.log('Generated disband fixtures through original save encoder.');process.exit(0);
}
assert.equal(mode,'verify');const data=JSON.parse(fs.readFileSync(path.join(dir,'disband-verify.json')));
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const live=at=>(at+0x801135a0)>>>0;m.load(live(0xb0000),rom.subarray(0xb0000,0xe6000));
const read=(at,n)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+n));
for(const row of data.cases){const before=Buffer.from(row.before,'hex'),characters=Buffer.from(row.characters,'hex'),ranks=Buffer.from(row.order,'hex');
 m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.set('sp',0x803f0000);m.load(roster,characters);m.load(squads,before);
 m.load(inventory,Buffer.from(row.inventory,'hex'));m.load(soldiers,Buffer.from(row.soldiers,'hex'));
 m.call(0x8016b774,0x8016b874);m.set('sp',0x803f0000);
 for(let i=0;i<32;++i)m.w8(root+0x117c+i*54+3,255);
 const activeIds=[];
 for(let i=0;i<30;++i){const r=before.subarray(i*25,i*25+25);if((r[1]&129)!==1)continue;
  const index=activeIds.length,a=root+0x117c+index*54;activeIds.push(i);m.load(a,Buffer.alloc(54));
  m.w8(a,r[0]);m.w8(a+2,255);m.w8(a+13,i);m.load(a+4,Buffer.alloc(9,255));m.w8(a+31,r[12]);m.w8(a+30,r[23]);m.w8(a+29,r[24]);
  for(let j=0;j<10;++j)m.w16(a+34+j*2,r[13+j]);m.set('s4',squads+i*25);m.set('s2',a);m.set('s8',8);m.call(live(0xc188c),live(0xc192c));m.w8(squads+i*25+1,r[1]&126);
 }
 m.load(order,Buffer.alloc(30,255));activeIds.forEach((id,i)=>m.w8(order+ranks[id],i));m.w32(root+704,activeIds.length);m.w32(root+708,activeIds.length);
 m.set('a0',activeIds.indexOf(row.index));m.call(live(0xb6fe8));m.call(live(0xc1d68));
 assert.deepEqual(read(squads+row.index*25,25),Buffer.from(row.record,'hex'),'disband leaves inactive saved payload');
 assert.deepEqual(read(order,30).map(v=>v===255?31:v),Buffer.from(row.afterOrder,'hex'),'compacted Army order');
 assert.deepEqual(read(soldiers,364),Buffer.from(row.afterSoldiers,'hex'),'returned groups and cleared HP/state');
 assert.deepEqual(read(roster,5600),Buffer.from(row.afterCharacters,'hex'),'returned roster and leader flags');
 const owned=Buffer.from(row.inventory,'hex');for(let i=0;i<40;++i){assert.equal(m.r16(inventory+i*4),owned.readUInt16BE(i*4));assert.equal(m.r8(inventory+i*4+3),owned[i*4+3],'total ownership unchanged');}
}
for(const row of data.saves){m.load(payload,Buffer.from(row.payload,'hex'));m.set('sp',0x803f0000);m.set('a0',payload);m.set('a1',descriptor);m.call(0x800742f4,0x80000100,5000000);
 assert.deepEqual(read(squads,750),Buffer.from(row.units,'hex'));assert.deepEqual(read(order,30),Buffer.from(row.order,'hex'));
 assert.deepEqual(read(soldiers,364),Buffer.from(row.soldiers,'hex'));assert.deepEqual(read(roster,5600),Buffer.from(row.characters,'hex'));}
console.log(`PASS ${data.cases.length} original disband/store sequences and ${data.saves.length} original save decodes. Synthetic nonlegion Army allocation, not a natural menu run.`);
