"use strict";
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');const{Mips}=require('../../tests/lib/mips-interpreter.cjs');
class ArmyMips extends Mips{exec(pc,next){const w=this.r32(pc);if((w>>>26)!==42)return super.exec(pc,next);const a=(this.r[(w>>>21)&31]+((w<<16)>>16))>>>0,v=this.r[(w>>>16)&31];for(let i=0;i<4-(a&3);++i)this.w8(a+i,v>>>(24-i*8));return[next,(next+4)>>>0];}}
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();const m=new ArmyMips(),dir=process.argv[2],mode=process.argv[3];
const roster=0x80193bc0,squads=0x801971f0,legions=0x801969b8,order=0x80193ab0,root=0x80260000,payload=0x80350000,descriptor=0x80187444;
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const read=(a,n)=>Buffer.from(m.ram.slice(m.phys(a),m.phys(a)+n));
if(mode==='generate'){
 m.w8(roster+17,1);m.w8(roster+18,1);
 for(let id=1;id<=20;++id){const a=roster+id*56,name=Buffer.alloc(13);Buffer.from('Member'+id).forEach((c,i)=>name[i]=c+16);name[('Member'+id).length]=255;m.load(a,name);
  const cls=[1,6,11].includes(id)?33:5;m.w8(a+17,cls);m.w8(a+18,cls);m.w8(a+19,10);m.w16(a+22,100);m.w16(a+24,90);m.w8(a+51,id<=17?2:0);m.w8(a+52,id);}
 m.load(0x801977d8,Buffer.alloc(30,31));
 for(let i=0;i<17;++i){const a=squads+i*25,deployed=i>=5&&i<10,head=[0,5,10].includes(i);m.w8(a,i+1);m.w8(a+1,1|(deployed?4:0)|(head?2:0));m.w8(a+2,i+1);m.w8(a+7,4);m.w8(a+12,23);m.w8(a+13,9);m.w8(a+23,2);m.w8(a+24,2);m.w8(0x801977d8+i,i);}
 m.w8(squads+3,18);m.w8(squads+8,8);m.w8(squads+2*25+3,100);m.w8(squads+2*25+8,8);
 m.w8(0x80190f81,9);m.w16(0x80190f82,300);m.w16(0x80190f84,83);m.w8(0x80191074,2);m.w16(0x80193ac0,9);m.w8(0x80193ac3,20);
 for(const [index,first]of[[0,0],[2,5],[4,10]]){const a=legions+index*11;m.load(a,Buffer.from([index+1,index===2?5:1,first,first+1,first+2,first+3,first+4,1,4,2,6]));}
 m.load(order,Buffer.from([4,0,2,7,7,7]));const original=Buffer.from(m.ram);
 const variant=(name,edit)=>{m.load(0x80000000,original);m.set('sp',0x803f0000);edit();m.set('a0',payload);m.set('a1',descriptor);m.call(0x80074494,0x80000100,5000000);fs.writeFileSync(path.join(dir,name),read(payload,6180));};
 variant('legions.bin',()=>{});
 variant('legion-last.bin',()=>{for(const i of[2,4])m.w8(legions+i*11+1,0);for(let i=5;i<15;++i)m.w8(squads+i*25+1,1);m.load(order,Buffer.from([0,7,7,7,7,7]));});
 variant('legion-order-duplicate.bin',()=>m.w8(order+1,4));
 variant('legion-order-missing.bin',()=>m.w8(order+2,7));
 variant('legion-member-duplicate.bin',()=>m.w8(legions+3,0));
 variant('legion-member-missing.bin',()=>m.w8(legions+3,29));
 variant('legion-member-shared.bin',()=>m.w8(legions+2*11+2,0));
 variant('legion-no-commander.bin',()=>m.w8(legions+2,255));
 variant('legion-unrepresented.bin',()=>m.w8(legions+7*11+1,1));
 console.log('Generated saved legions through original save encoder.');process.exit(0);
}
assert.equal(mode,'verify');const data=JSON.parse(fs.readFileSync(path.join(dir,'legion-verify.json'))),live=a=>(a+0x801135a0)>>>0;m.load(live(0xb0000),rom.subarray(0xb0000,0xe6000));
for(const row of data.cases){const units=Buffer.from(row.units,'hex'),groups=Buffer.from(row.legions,'hex'),chars=Buffer.from(row.characters,'hex');
 m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.set('sp',0x803f0000);m.load(squads,units);m.load(legions,groups);m.load(roster,chars);m.load(0x80190f80,Buffer.from(row.soldiers,'hex'));m.load(0x80193ac0,Buffer.from(row.inventory,'hex'));
 const ranks=Buffer.from(row.unitOrder,'hex');m.load(0x801977d8,Buffer.alloc(30,255));let count=0;
 for(let i=0;i<32;++i)m.w8(root+4476+i*54+3,255);
 for(let i=0;i<30;++i){const r=units.subarray(i*25,i*25+25);if((r[1]&129)!==1)continue;++count;const a=root+4476+i*54;m.load(a,Buffer.alloc(54));m.w8(a,r[0]);m.w8(a+1,(r[1]&4)?16:0);m.w8(a+2,255);m.w8(a+13,i);m.load(a+4,Buffer.alloc(9,255));
  m.w8(a+29,r[24]);m.w8(a+30,r[23]);m.w8(a+31,r[12]);for(let j=0;j<10;++j)m.w16(a+34+j*2,r[13+j]);m.set('s4',squads+i*25);m.set('s2',a);m.set('s8',8);m.call(live(0xc188c),live(0xc192c));m.w8(squads+i*25+1,r[1]&126);m.w8(0x801977d8+ranks[i],i);for(let j=0;j<5;++j)if(r[2+j])m.w8(root+6258+r[2+j]*2,0x10);
 }
 let legionCount=0,grouped=0;m.load(order,Buffer.alloc(10,255));Buffer.from(row.order,'hex').forEach((v,i)=>m.w8(order+i,v===7?255:v));
 for(let i=0;i<10;++i){const r=groups.subarray(i*11,i*11+11);if((r[1]&129)!==1)continue;++legionCount;const a=root+4308+i*14;m.load(a,Buffer.alloc(14));m.w8(a,r[0]);m.w8(a+1,(r[1]&4)?16:0);m.w8(a+13,i);m.w8(a+7,r[8]);m.w8(a+10,r[9]);m.w8(a+11,r[10]);m.w8(a+12,r[7]);
  for(let j=0;j<5;++j){const id=r[2+j];m.w8(a+2+j,id);if(id===255)continue;++grouped;m.w8(a+8,m.r8(a+8)+1);const u=root+4476+id*54;m.w8(u+1,8|((r[1]&4)?16:0));m.w8(u+2,i);for(let cell=0;cell<9;++cell){const member=m.r8(u+4+cell);if(member!==255)m.w8(root+6258+member*2,0x30);}}
  m.w8(legions+i*11+1,r[1]&126);
 }
 m.w32(root+704,count-grouped);m.w32(root+708,count);m.w32(root+1500,legionCount);
 const leaderEligibility=()=>{m.set('a0',18);m.set('a1',0x100);m.set('a2',5);m.call(live(0xc01dc));return m.get('v0')>>>0;};
 assert.equal(leaderEligibility(),10,'ordinary candidate cannot command a current legion');
 m.set('a0',row.index);m.call(live(0xb7164));
 assert.equal(leaderEligibility(),row.index===0?0xffffffff:10,'restriction follows actual legion membership');
 m.call(live(0xc1d68));
 assert.deepEqual(read(squads,750),Buffer.from(row.afterUnits,'hex'),'whole saved squad records');
 assert.deepEqual(read(legions,110),Buffer.from(row.afterLegions,'hex'),'whole saved legion records');
 assert.deepEqual(read(order,6).map(v=>v===255?7:v),Buffer.from(row.afterOrder,'hex'),'saved legion order');
 assert.deepEqual(read(roster,5600),chars,'character leader/status records retained');assert.deepEqual(read(0x80190f80,364),Buffer.from(row.soldiers,'hex'));assert.deepEqual(read(0x80193ac0,160),Buffer.from(row.inventory,'hex'));
}
for(const row of data.saves){m.load(payload,Buffer.from(row.payload,'hex'));m.set('sp',0x803f0000);m.set('a0',payload);m.set('a1',descriptor);m.call(0x800742f4,0x80000100,5000000);
 assert.deepEqual(read(squads,750),Buffer.from(row.units,'hex'));assert.deepEqual(read(legions,110),Buffer.from(row.legions,'hex'));assert.deepEqual(read(order,6),Buffer.from(row.order,'hex'));}
console.log(`PASS ${data.cases.length} original legion disband/store sequences and ${data.saves.length} original save decodes. Synthetic Army allocation; no natural menu run.`);
