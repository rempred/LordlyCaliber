"use strict";
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
class ArmyMips extends Mips {
 exec(pc,next){const word=this.r32(pc);if((word>>>26)!==42)return super.exec(pc,next);
  const address=(this.r[(word>>>21)&31]+((word<<16)>>16))>>>0,value=this.r[(word>>>16)&31];
  // Big-endian SWL stores the high register bytes from address to the word end.
  for(let i=0;i<4-(address&3);++i)this.w8(address+i,value>>>(24-i*8));
  return [next,(next+4)>>>0];
 }
}
const m=new ArmyMips(),dir=process.argv[2],mode=process.argv[3],payload=0x80350000,descriptor=0x80187444;
const roster=0x80193bc0,squads=0x801971f0,order=0x801977d8,root=0x80260000;
if(mode==='generate'){
 execFileSync(process.execPath,[path.join(__dirname,'save-unit-mips.cjs'),dir,'generate']);
 const original=fs.readFileSync(path.join(dir,'unit-ram.bin'));
 const variant=(name,edit)=>{m.load(0x80000000,original);m.set('sp',0x803f0000);
  for(const id of [10,11,40]){const at=roster+id*56;m.load(at,Buffer.from(original.subarray(0x193bc0+112,0x193bc0+168)));
   m.w8(at+51,0);m.w8(at+52,id);m.w8(at+17,id===11?33:5);m.w8(at+18,id===11?33:5);}
  m.load(order,Buffer.alloc(30,31));[0,2,3,4,5].forEach((id,i)=>m.w8(order+id,[2,0,4,1,3][i]));
  edit();m.set('a0',payload);m.set('a1',descriptor);m.call(0x80074494,0x80000100,5000000);
  fs.writeFileSync(path.join(dir,name),Buffer.from(m.ram.slice(m.phys(payload),m.phys(payload)+6180)));};
 variant('create.bin',()=>{});
 variant('create-empty.bin',()=>{m.load(squads,Buffer.alloc(750));m.load(order,Buffer.alloc(30,31));for(let id=1;id<100;++id)m.w8(roster+id*56+51,0);});
 variant('create-full.bin',()=>{for(let i=0;i<30;++i){const at=squads+i*25;m.load(at,Buffer.alloc(25));m.w8(at,i+1);m.w8(at+1,1);m.w8(at+2,i+1);m.w8(order+i,i);
  m.w8(roster+(i+1)*56+17,2);m.w8(roster+(i+1)*56+18,2);m.w16(roster+(i+1)*56+24,90);m.w8(roster+(i+1)*56+51,2);}});
 variant('create-no-record.bin',()=>{for(let i=0;i<30;++i)m.w8(squads+i*25+1,129);});
 variant('create-order-conflict.bin',()=>m.w8(order+2,2));
 variant('create-number-conflict.bin',()=>m.w8(squads+2*25,1));
 variant('create-dead.bin',()=>m.w16(roster+10*56+24,0));
 variant('create-unavailable.bin',()=>m.w8(roster+10*56+51,4));
 variant('create-no-command.bin',()=>{m.w8(roster+10*56+17,1);m.w8(roster+10*56+18,1);});
 variant('create-flag-conflict.bin',()=>m.w8(roster+10*56+51,2));
 console.log('Generated squad-creation fixtures through original save encoder.');process.exit(0);
}
assert.equal(mode,'verify');const data=JSON.parse(fs.readFileSync(path.join(dir,'create-verify.json')));
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const live=at=>(at+0x801135a0)>>>0;m.load(live(0xb0000),rom.subarray(0xb0000,0xe6000));
const read=(at,n)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+n));
for(let alignment=0;alignment<4;++alignment)for(let size=0;size<=64;++size){
 const address=0x80330008+alignment;m.load(0x80330000,Buffer.alloc(80,0xa7));
 m.set('a0',address);m.set('a1',size);m.call(0x80093380);
 const expected=Buffer.alloc(80,0xa7);expected.fill(0,8+alignment,8+alignment+size);
 assert.deepEqual(read(0x80330000,80),expected,'original bzero preserves adjacent bytes at every alignment');
}

for(const row of data.eligibility){m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.set('sp',0x803f0000);
 const at=roster+10*56;m.w8(at+17,row.primary);m.w8(at+18,row.secondary);m.w8(at+51,row.flags);m.w16(at+24,row.hp);
 m.set('a0',10);m.set('a1',352);m.set('a2',3);m.call(live(0xc01dc));
 assert.equal((m.get('v0')|0)===-1,row.allowed,'original new-leader eligibility');}
for(const row of data.cases){const before=Buffer.from(row.before,'hex'),characters=Buffer.from(row.characters,'hex'),ranks=Buffer.from(row.order,'hex');
 m.load(root,Buffer.alloc(0x3000));m.w32(0x80196af8,root);m.set('sp',0x803f0000);m.load(roster,characters);m.load(squads,before);
 for(let i=0;i<32;++i)m.w8(root+0x117c+i*54+3,255);
 const activeIds=[];
 for(let i=0;i<30;++i){const record=before.subarray(i*25,i*25+25);if((record[1]&129)!==1)continue;
  const index=activeIds.length,active=root+0x117c+index*54;activeIds.push(i);
  m.load(active,Buffer.alloc(54));m.w8(active,record[0]);m.w8(active+2,255);m.w8(active+13,i);m.load(active+4,Buffer.alloc(9,255));
  m.w8(active+31,record[12]);m.w8(active+30,record[23]);m.w8(active+29,record[24]);
  for(let j=0;j<10;++j)m.w16(active+34+j*2,record[13+j]);
  m.set('s4',squads+i*25);m.set('s2',active);m.set('s8',8);m.call(live(0xc188c),live(0xc192c));
  m.w8(squads+i*25+1,record[1]&126);
 }
 m.load(order,Buffer.alloc(30,255));activeIds.forEach((id,i)=>m.w8(order+ranks[id],i));
 m.w32(root+704,activeIds.length);m.w32(root+708,activeIds.length);
 m.set('a0',30);m.call(live(0xb851c));const scratch=root+0x117c+30*54;
 m.w8(scratch+3,row.cell);m.w8(scratch+4+row.cell,row.leader);m.w8(scratch+31,99);
 m.set('a0',30);m.call(live(0xb75c8));
 assert.equal(m.r8(root+0x117c+activeIds.length*54+31),0,'new commit resets stale scratch Unity');
 m.call(live(0xc1d68));
 assert.deepEqual(read(squads+row.index*25,25),Buffer.from(row.record,'hex'),'original new squad record');
 const normalizedOrder=read(order,30).map(v=>v===255?31:v);
 assert.deepEqual(normalizedOrder,Buffer.from(row.afterOrder,'hex'),'original appended persistent rank');
 assert.equal(m.r8(roster+row.leader*56+51),characters[row.leader*56+51]|2,'new leader flag');
}
for(const row of data.saves){m.load(payload,Buffer.from(row.payload,'hex'));m.set('sp',0x803f0000);m.set('a0',payload);m.set('a1',descriptor);m.call(0x800742f4,0x80000100,5000000);
 assert.deepEqual(read(squads,750),Buffer.from(row.units,'hex'));assert.deepEqual(read(order,30),Buffer.from(row.order,'hex'));assert.deepEqual(read(roster,5600),Buffer.from(row.characters,'hex'));}
console.log(`PASS 260 original bzero boundary cases, ${data.eligibility.length} leader eligibility cases, ${data.cases.length} original create/store sequences and ${data.saves.length} original save decodes. Synthetic Army allocation, not a natural menu run.`);
