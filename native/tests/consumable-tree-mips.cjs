'use strict';
const fs=require('fs'),assert=require('assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const h=0x27c5400,offset=rom.readUInt32BE(h+12),address=rom.readUInt32BE(h+16),size=rom.readUInt32BE(h+20),characters=0x80193bc0;
const entries=[[0x41098,0x8016b198],[0xc4650,0x801d7bf0],[0xc9850,0x801dcdf0],[0x1ad8cc,0x802195bc],[0x1ad0e4,0x80218dd4],[0x1a9290,0x80214f80]];
function cpu(){const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));m.load(address,rom.subarray(offset,offset+size));for(const [at,ram]of entries)m.load(ram,rom.subarray(at,at+8));m.set('sp',0x803ff000);return m;}
function run(m,item,id,apply,value=0){m.set('a0',apply);m.set('a1',value);m.set('a2',item);m.set('a3',id);m.call(0x8016b198);assert.equal(m.get('sp'),0x803ff000);return m.get('v0');}
for(const cls of [5,12])for(const random of [0,1,20,21]){
 const m=cpu(),id=1,base=characters+id*56;m.load(base,Buffer.alloc(56,0));m.w8(base+0x12,cls);m.w16(base+0x16,200);m.w16(base+0x18,50);m.w16(base+0x1c,100);m.w8(base+0x32,35);let calls=0;m.trap(0x8009c7cc,c=>{calls++;c.set('v0',random);});
 const q=run(m,1,id,0);assert.equal(m.r16(base+0x18),50);assert.equal(m.r16(base+0x1c),100);assert.equal(m.r8(base+0x32),35);assert.equal(calls,1);
 run(m,1,id,1,q);assert.equal(calls,1);assert.equal(m.r16(base+0x18),cls===5?90+random%21:50);assert.equal(m.r16(base+0x1c),cls===5?100:102+random%2);assert.equal(m.r8(base+0x32),25);
 run(m,1,id,1,q);assert.equal(m.r8(base+0x32),25); // A consumed plan cannot apply twice.
}
{const m=cpu(),base=characters+56;m.w16(base+0x16,301);m.w16(base+0x18,0);m.w8(base+0x33,0x24);run(m,7,1,0);run(m,7,1,1,50);assert.equal(m.r16(base+0x18),175,'revive then heal must execute both actions in order');assert.equal(m.r8(base+0x33),0x20);}
// The untouched Heal Seed and Revive Stone retain their native effects while
// other items use authored trees. Soldier flags are separate from characters.
{const m=cpu(),base=characters+56;m.w16(base+0x16,500);m.w16(base+0x18,100);assert.equal(run(m,2,1,0),300);run(m,2,1,1,300);assert.equal(m.r16(base+0x18),400);m.w8(0x80191010+100,0x24);run(m,6,100,1);assert.equal(m.r8(0x80191010+100),0x20);}
// Both menu gates consult the authored effects rather than Heal Leaf's old rule:
// a full-health non-Knight can use the item for strength/fatigue; a capped one cannot.
for(const canUse of [true,false]){
 const m=cpu(),base=characters+56,ui=0x803b0000;m.w8(base+0x12,12);m.w16(base+0x16,200);m.w16(base+0x18,200);m.w16(base+0x1c,canUse?100:999);m.w8(base+0x32,0);
 m.set('a0',1);m.set('a1',1);m.set('a2',0);m.call(0x802195bc);assert.equal(m.get('v0'),Number(canUse));
 m.w32(0x80196af8,ui);m.set('a0',1);m.call(0x801d7bf0);assert.equal((m.r8(ui+0x1872+2)&128)===0,canUse);
}
console.log('Exported item trees: both branches, combined effects, one-time random rolls, single commit and revival passed.');
// Group eligibility and independent member plans, including the Soldier HP cap.
{
 const m=cpu(),base=characters+56;let groups=0x801971f2;
 for(let at=rom.indexOf('LC07CAPA',0x2800000);at>=0;at=rom.indexOf('LC07CAPA',at+8))if(at%16===0&&rom.readUInt32BE(at+12)===at){groups=rom.readUInt32BE(at+24)+0x3802;break;}
 m.w8(groups,1);m.w8(groups+1,100);m.w8(base+0x12,5);m.w16(base+0x16,200);m.w16(base+0x18,100);m.w8(base+0x32,20);m.w16(0x80190f82,900);m.w16(0x80190ebc+200,450);
 m.set('a0',3);m.set('a1',0);m.set('a2',0);m.call(0x802195bc);assert.equal(m.get('v0'),1);
 run(m,3,1,0);run(m,3,100,0);run(m,3,1,1);run(m,3,100,1);
 assert.equal(m.r16(base+0x18),190);assert.equal(m.r8(base+0x32),15);assert.equal(m.r16(0x80190ebc+200),540);
}
console.log('Squad targets and Soldier limits passed.');
// Retail Heal Pack internally probes Heal Leaf. That probe must remain native
// even when Heal Leaf has an independently authored tree.
for(const injured of [true,false]){
 const m=cpu(),base=characters+56;let groups=0x801971f2;
 for(let at=rom.indexOf('LC07CAPA',0x2800000);at>=0;at=rom.indexOf('LC07CAPA',at+8))if(at%16===0&&rom.readUInt32BE(at+12)===at){groups=rom.readUInt32BE(at+24)+0x3802;break;}
 m.w32(address+32+3*4,0);m.w8(groups,1);m.w8(base+0x12,12);m.w16(base+0x16,200);m.w16(base+0x18,injured?100:200);m.w16(base+0x1c,injured?999:100);m.w8(base+0x32,injured?0:20);
 m.set('a0',3);m.set('a1',0);m.set('a2',0);m.call(0x802195bc);assert.equal(m.get('v0'),Number(injured),'unchanged Heal Pack uses native healing eligibility');
}
console.log('Independent item trees preserve retail Heal Pack eligibility.');

// Execute every default tree against the unchanged accepted-C adaptation.
// Compare character/Soldier state and RNG/growth calls, not the preview number:
// custom previews intentionally cap that number to the actual HP/fatigue change.
const retailPrograms=JSON.parse(fs.readFileSync(process.argv[3],'utf8'));
const programAddress=0x80380000;
assert.ok(address+size<programAddress,'test program storage is outside the exported runtime');
for(const [key,hex] of Object.entries(retailPrograms)){
 const item=Number(key);
 for(const id of ([1,2,3,6].includes(item)?[1,100]:[1]))for(const random of [0,1,29]){
  const results=[];
  for(const authored of [false,true]){
   const m=cpu(),base=characters+id*56;let rolls=0,growths=0;
   for(let n=1;n<45;n++)m.w32(address+32+n*4,0);
   if(authored){m.load(programAddress,Buffer.from(hex,'hex'));m.w32(address+32+item*4,programAddress);}
   if(id<100){
    m.w8(base+0x12,5);m.w8(base+0x13,20);m.w16(base+0x16,500);m.w16(base+0x18,item===7?0:100);
    for(let stat=0;stat<6;stat++)m.w16(base+0x1c+stat*2,100);
    m.w8(base+0x1b,50);m.w8(base+0x28,50);m.w8(base+0x32,40);m.w8(base+0x33,[6,7].includes(item)?0x24:0x20);
   }else{m.w16(0x80190f82,900);m.w16(0x80190ebc+id*2,450);m.w8(0x80191010+id,item===6?0x24:0x20);}
   m.trap(0x8009c7cc,c=>{rolls++;for(const r of ['a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9','v1'])c.set(r,0xdeadbeef);c.set('v0',random);});
   m.trap(0x8016eba4,c=>{growths++;const character=c.get('a0');c.w8(character+0x13,c.r8(character+0x13)+1);c.w16(character+0x16,c.r16(character+0x16)+7);});
   const amount=run(m,item,id,0);run(m,item,id,1,amount);
   results.push({character:id<100?Array.from({length:56},(_,n)=>m.r8(base+n)):null,
    soldierHp:m.r16(0x80190ebc+id*2),soldierStatus:m.r8(0x80191010+id),rolls,growths});
  }
  assert.deepEqual(results[1],results[0],`retail tree parity: item ${item}, target ${id}, random ${random}`);
 }
}
console.log('All 18 base retail item trees match native outcomes and random/growth calls.');
