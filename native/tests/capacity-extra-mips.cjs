'use strict';
// Mixed-layout reads/writes and added-call ABI preservation in private RAM.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
module.exports=function({m,read,call,rom,symbols,entries,run,work,native,reset,guards}){
 const catalog=JSON.parse(fs.readFileSync(path.join(native,'game/scenarios/capacity-extra.json'))),base=0x802f0000;
 const availability=0x80315000,bindings={...symbols,...entries,lc_capacity_availability:availability},rows=new Map();let source='.text\n';
 for(const [index,row] of catalog.patches.entries()){
  row.entry=base+index*0x400;rows.set(row.site,row);
  row.expected.forEach((w,i)=>assert.equal(rom.readUInt32BE(row.site+i*4),w));
  let s=row.source;
  for(const label of [...s.matchAll(/^([a-z_][a-z_0-9]*):/gm)].map(m=>m[1]).filter(n=>n!==row.name))s=s.replace(new RegExp('\\b'+label+'\\b','g'),row.name+'_'+label);
  for(const [name,value] of Object.entries(row.bindings)){const n=row.name+'_'+name;bindings[n]=value;s=s.replaceAll(name,n);}
  source+=`.org 0x${(row.entry-base).toString(16)}\n`+s;
 }
 fs.writeFileSync(path.join(work,'capacity_extra.s'),source);
 fs.writeFileSync(path.join(work,'capacity_extra.ld'),Object.entries(bindings).map(([n,a])=>`${n} = 0x${a.toString(16)};`).join('\n')+`\nSECTIONS { .text 0x${base.toString(16)} : { *(.text) } }\n`);
 run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','capacity_extra.o','capacity_extra.s']);
 run('mips-kmc-elf-ld.exe',['-EB','-T','capacity_extra.ld','-o','capacity_extra.elf','capacity_extra.o']);
 run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(n=>'--remove-section='+n),'capacity_extra.elf','capacity_extra.bin']);
 m.load(base,fs.readFileSync(path.join(work,'capacity_extra.bin')));
 const actor=0x803b0000,sp=0x803f0000,stop=0x80000100;let storageChecks=0,hookChecks=0;
 function setup(){reset();for(let r=1;r<32;r++)m.r[r]=0x80340000+r*256;m.set('sp',sp);m.set('ra',stop);m.load(actor,Buffer.alloc(256));}
 function execute(site){
  const row=rows.get(site),stops=[...Object.values(row.bindings),stop];let pc=row.entry,next=pc+4,n=0;
  while(!stops.includes(pc)){
   assert(++n<50000,'extra hook did not exit '+site.toString(16));
   const trap=m.traps.get(pc);if(trap){trap(m);pc=m.get('ra');next=pc+4;}
   else [pc,next]=m.exec(pc,next);
  }
  assert.equal(m.get('sp'),sp);return pc;
 }
 for(const enemy of [false,true])for(const id of enemy?[1,99,100,128,219,220,254]:[100,119,219]){
  const legacy=enemy?0x801953f0:0x80190f80;
  const hp=enemy?symbols.lc_enemy_soldier_hp+2*id:legacy+2*id-196;
  const state=enemy?symbols.lc_enemy_soldier_state+id:legacy+id+144;
  setup();m.set('s3',enemy?59:29);m.set('v1',id);execute(0x119240);
  assert.equal(m.get('a0'),enemy?symbols.lc_enemy_soldier_hp+196:legacy);assert.equal(m.get('v0'),2*id);storageChecks++;
  setup();m.set('s2',legacy);m.set('a1',id);m.set('v0',legacy+2*id);m.set('v1',173);const before=m.get('v0');
  assert.equal(execute(0x21c308),rows.get(0x21c308).bindings.lc_original_call);assert.equal(m.r16(hp),173);assert.equal(m.get('v0'),before);assert.equal(m.get('ra'),rows.get(0x21c308).bindings.lc_continue);storageChecks++;
  for(const site of [0x21c328,0x21c33c]){
   setup();m.set('s2',legacy);m.set('s1',actor);m.w8(actor+246,id);m.w8(state,7);execute(site);
   assert.equal(m.get('v1')+144,state);assert.equal(m.r8(m.get('v1')+144),7);storageChecks++;
  }
  setup();m.set('s1',legacy);m.set('v1',id);m.w16(hp,413);execute(0x23369c);assert.equal(m.get('v1'),(id-100)>>>0);assert.equal(m.get('a0'),413);storageChecks++;
  setup();m.set('s1',legacy);m.set('v1',id-100);m.w8(state,5);execute(0x2336b0);assert.equal(m.get('v0'),5);storageChecks++;
  setup();m.set('s1',legacy);m.w16(legacy+2,827);m.w16(symbols.lc_enemy_soldier_max_hp,739);
  assert.equal(execute(0x2336c4),rows.get(0x2336c4).bindings.lc_original_call);assert.equal(m.get('a1'),enemy?739:827);assert.equal(m.get('ra'),rows.get(0x2336c4).bindings.lc_continue);storageChecks++;
  if(enemy){
   setup();m.set('v0',2*id);execute(0x133cb0);assert.equal(m.get('a2'),hp);assert.equal(m.get('t0'),symbols.lc_enemy_soldier_max_hp);storageChecks++;
   setup();m.set('v1',id);m.w16(hp,433);m.w16(symbols.lc_enemy_soldier_max_hp,827);execute(0x20b7d0);assert.equal(m.get('t0'),433);assert.equal(m.get('t1'),827);storageChecks++;
  }
  guards();
 }
 for(const enemy of [false,true])for(const stillReferenced of [false,true]){
  setup();const id=254,unit=symbols.lc_capacity_sources+(enemy?59:29)*25;
  m.set('v1',id);m.set('s1',unit+2);m.set('s3',enemy?59:29);m.set('s5',actor);m.w32(actor,0x803bb000);
  m.w8(unit+2,id);if(stillReferenced)m.w8(symbols.lc_capacity_sources+58*25+3,id);
  m.w8(symbols.lc_enemy_types+id,2);m.w16(symbols.lc_enemy_soldier_hp+id*2,92);
  m.hi=12345;m.lo=67890;const before=Array.from(m.r);
  execute(0x119278);assert.equal(m.r8(unit+2),0);assert.equal(m.get('s0'),0x803bb000);
  for(let r=2;r<32;r++)if(r!==16)assert.equal(m.r[r],before[r],'death hook live register '+r);
  assert.equal(m.hi,12345);assert.equal(m.lo,67890);assert.equal(m.r8(symbols.lc_enemy_types+id),enemy&&!stillReferenced?0:2);guards();hookChecks++;
 }
 for(const id of [0,1,99]){
  setup();m.set('s1',id);m.set('s2',actor);m.w8(actor+246,220);m.w8(symbols.lc_enemy_types+220,1);const dest=execute(0x214318),row=rows.get(0x214318);assert.equal(dest,id?row.bindings.lc_continue:row.bindings.lc_refuse);if(id)assert.equal(m.get('v0'),id*7);hookChecks++;
 }
 for(const id of [1,99,100,220,254,255]){
  setup();if(id!==255){m.w8(symbols.lc_enemy_types+id,1);m.w8(symbols.lc_enemy_characters+id*52+17,12);}
  m.set('a0',12);execute(0x249dd4);assert.equal(m.get('s1'),id);assert.equal(m.get('v0'),255);hookChecks++;
 }
 for(const id of [0,1,99,100,128,220,254])for(const tag of [0,1,2]){
  setup();m.w8(symbols.lc_enemy_types+id,tag);m.w8(symbols.lc_enemy_characters+id*52+17,7);
  m.set('fp',id);m.set('s6',symbols.lc_enemy_characters+id*52);m.set('s4',3);
  const row=rows.get(0x1303f4);assert.equal(execute(row.site),tag===1?row.bindings.lc_continue:row.bindings.lc_skipped);assert.equal(m.get('a0'),tag===1?7:0);assert.equal(m.get('s4'),tag===1?3:4);hookChecks++;
  setup();m.w8(symbols.lc_enemy_types+id,tag);m.w8(symbols.lc_enemy_characters+id*52+17,7);m.set('s0',id);m.set('s1',id*52);m.set('s3',actor);
  const nameRow=rows.get(0x41dec);assert.equal(execute(nameRow.site),tag===1?nameRow.bindings.lc_continue:nameRow.bindings.lc_skipped);assert.equal(m.get('v0'),tag===1?7:0);assert.equal(m.get('a0'),actor);hookChecks++;
  setup();m.w8(symbols.lc_enemy_types+id,tag);m.w8(symbols.lc_enemy_characters+id*52+17,7);m.set('a0',7);m.set('a1',id);m.set('a2',id*52);
  const find=rows.get(0x28381c);assert.equal(execute(find.site),id===0||tag===1?find.bindings.lc_found:find.bindings.lc_continue);assert.equal(m.get('a1'),id+1);hookChecks++;
  for(const busy of [0,1]){
   setup();m.load(availability,Buffer.alloc(356));m.w8(symbols.lc_enemy_types+id,tag);m.w8(availability+100+id,busy);m.set('s1',100+id);m.set('a3',id);m.set('a2',actor);
   const r=rows.get(0x133298),skip=busy||tag!==1;assert.equal(execute(r.site),skip?r.bindings.lc_skipped:r.bindings.lc_continue);assert.equal(m.get('a2'),actor+(skip?52:0));hookChecks++;
  }
 }
 return {storageChecks,hookChecks};
};
