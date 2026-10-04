'use strict';
// Execute linked C/ASM entries with the live registers of their original sites.
// Retail services are explicit traps; installation/authentication is a separate check.
const fs=require('node:fs'),assert=require('node:assert/strict');
const path=require('node:path');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixtures=JSON.parse(fs.readFileSync(process.argv[2]));
const callers=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/encounters/custom-owners/catalog.json')));
const dispatcher=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/encounters/dispatcher.json')));
let cases=0;
for(const f of fixtures) {
 const rom=fs.readFileSync(f.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
 const m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
 const S=f.symbols,E=f.entries,state=f.state,p=state+16,sp=0x803ffe00,STOP=0x80000100;
 const clobber=c=>{for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(r,0xdeadbeef);};
 function reset(active=0) {
  m.load(state,Buffer.alloc(144));m.w32(state+4,active);m.set('sp',sp);
  for(const [i,r] of ['s0','s1','s2','s3','s4','s5','s6','s7','fp'].entries())m.set(r,0xcafe1000+i);
  m.load(sp-0x400,Buffer.alloc(32,0xa6));m.load(sp,Buffer.alloc(32,0xb7));
 }
 function call(name,stop=STOP,ra=STOP) {
  const saved=['s0','s1','s2','s3','s4','s5','s6','s7','fp'].map(r=>[r,m.get(r)]);
  let entry=E[name];
  if(f.installed) {
   const owner=name==='lc_selection_site'?dispatcher:callers.owners.find(o=>o.sites.some(s=>s.entry===name&&!s.keepDelay));
   assert(owner,'An installed owner must exercise '+name);
   const site=name==='lc_selection_site'?dispatcher.selection:owner.sites.find(s=>s.entry===name&&!s.keepDelay);
   m.load(owner.address,rom.subarray(owner.start,owner.start+owner.size));entry=owner.address+site.offset;
   if(stop===STOP)stop=entry+8;
  }
  assert(Number.isInteger(entry),'Missing compiled entry '+name);
  m.call(entry,stop,200000,ra);
  assert.equal(m.get('sp'),sp);for(const [r,v] of saved)assert.equal(m.get(r),v,r);
  for(let i=0;i<32;i++){assert.equal(m.r8(sp-0x400+i),0xa6);assert.equal(m.r8(sp+i),0xb7);}
  cases++;
 }
 for(const custom of [0,1])for(const retail of [0,1,71,255])for(const pool of [1,45])for(const slot of [0,9])for(const side of [0,1]) {
  reset();m.w8(S.lc_staged_scenario,33);m.set('v0',retail);m.set('s0',pool);
  m.set('v1',0x801ed794+(pool-1)*20+slot*2+side);let dma=0;
  m.trap(S.lc_dma_read,c=>{
   if(!dma){assert.equal(c.get('a0'),S.lc_custom_index+(33*10+slot)*4);assert.equal(c.get('a2'),4);c.w32(c.get('a1'),custom?0x2910000:0);}
   else {assert.equal(c.get('a0'),0x2910000);assert.equal(c.get('a2'),128);const b=Buffer.alloc(128);b[0]=1;b[1]=2;b[5]=33;b[6]=slot;b[8]=165;c.load(c.get('a1'),b);}
   dma++;clobber(c);
  });
  const selected=custom?165:retail;
  call('lc_selection_site',selected?S.lc_selection_success:STOP);assert.equal(m.get('v0'),selected+256);
  assert.equal(dma,custom?2:1);assert.equal(m.r32(state+4),custom);
 }
 const source=0x801971f0+40*25;
 for(const active of [0,1])for(const cl of [1,5,255]) {
  reset(active);m.set('s1',source);m.set('s2',10);m.set('s3',cl);m.w32(state,active);
  m.w8(p+1,2);m.w8(p+8,165);m.w8(p+9,255);m.w8(p+10,8);m.w16(p+12,277);
  m.w8(p+16,255);m.w8(p+17,127);m.w8(p+18,6);m.w16(p+22,1);
  m.load(S.lc_character_records,Buffer.alloc(100*52,1));for(const id of [1,98])m.w8(S.lc_character_records+id*52+17,0);
  m.load(source,Buffer.alloc(25));const calls=[];
  m.trap(S.lc_character_create,c=>{calls.push([c.get('a0'),c.get('a1'),c.get('a2'),c.get('a3'),c.r32(c.get('sp')+16)]);c.w8(c.get('a0')+17,1);clobber(c);});
  call('lc_materializer_site',active?S.lc_materializer_epilogue:cl===1?S.lc_materializer_class_one:S.lc_materializer_generic);
  if(active) {
   assert.deepEqual(calls,[[S.lc_character_records+52,165,9,277,0],[S.lc_character_records+98*52,255,137,0,1]]);
   assert.equal(m.r8(source+2),1);assert.equal(m.r8(source+3),98);assert.equal(m.r8(source+7),8);assert.equal(m.r8(source+8),6);
  } else {assert.deepEqual(calls,[]);assert.equal(m.get('v0'),1);assert.equal(m.get('a0'),0);}
 }
 const actor=0x80260000,leader=0x80260200;
 for(const active of [0,1])for(const enabled of [0,1])for(const chance of [0,100]) {
  reset(active);m.w8(p+2,enabled);m.w8(p+3,chance);m.set('s0',actor);m.set('s1',19);m.set('s2',leader);
  m.set('a0',0x12345678);m.w32(actor+0x4c,0x87654321);let original=0;
  m.trap(S.lc_persuasion_helper,c=>{original++;assert.equal(c.get('a0'),0x12345678);assert.equal(c.get('a1'),0x87654321);clobber(c);c.set('v0',0x11223344);});
  m.trap(S.lc_actor_at_index,()=>assert.fail('Index19 has no later actor'));m.w8(leader+0x4b,5);
  call('lc_persuasion_site',active&&enabled?S.lc_persuasion_epilogue:STOP);
  assert.equal(original,active&&enabled?0:1);assert.equal(m.get('v0'),active&&enabled?(chance?0:2):0x11223344);
 }
 for(const active of [0,1])for(const hp of [0,50,51,100]) {
  reset(active);m.set('s0',actor);m.w8(p+4,50);m.w16(actor+0x20,hp);m.w16(actor+0x22,100);
  call('lc_retreat_site',active?S.lc_retreat_epilogue:STOP);
  if(active)assert.equal(m.get('v0'),hp<=50?1:0);else{assert.equal(m.get('a0'),0);assert.equal(m.get('a1'),0x310000);}
 }
 for(const active of [0,1]) {
  reset(active);m.set('a1',65535);m.set('v0',5);call('lc_card_message_site');assert.equal(m.get('a1'),active?15:4);
  for(const high of [0,1,0x40,0x80,0xc0,0xff])for(const low of [0,15,16]) {
   reset(active);m.w8(S.lc_message_index,high);m.w8(S.lc_message_index+1,low);
   m.set('a0',actor);m.set('s0',0x80123400);m.set('v1',0x567);
   call('lc_card_text_site');assert.equal(m.r32(actor+4),active&&!(high&63)&&low===15?p+68:0x80123967);
  }
  for(const record of [0,actor]) {
   reset(active);m.w32(state+12,record);let original=0;m.set('a0',123);m.set('a1',456);
   m.trap(S.lc_persuasion_target,c=>{original++;assert.equal(c.get('a0'),123);assert.equal(c.get('a1'),456);clobber(c);c.set('v0',leader);});
   call('lc_talk_target_site');assert.equal(m.get('v0'),active&&record?record:leader);assert.equal(original,active&&record?0:1);
  }
  for(const value of [0,1,2,0xffffffff])for(const [name,cost] of [['lc_fight_budget_site',1],['lc_talk_budget_site',2]]) {
   reset(active);m.set('v0',value);call(name);assert.equal(m.get('v0'),active?2:(value-cost)>>>0);
  }
  for(const value of [0,1,7,0xffffffff]) {
   reset(active);let original=0;m.trap(S.lc_round_mode,c=>{original++;clobber(c);c.set('v0',value);});
   call('lc_round_site');assert.equal(original,1);assert.equal(m.get('v0'),value||active);
  }
 }
 for(const original of [0,1,5,0xffffffff])for(const padding of [0,1,20])for(const missing of [false,true]) {
  reset();m.set('a0',0);m.w32(S.lc_menu_context,missing?0:actor);m.w32(actor+0x814,20);
  const stream=Buffer.alloc(20,0x41);for(let i=0;i<padding;i++)stream[i]=i%2?255:0;m.load(actor+16,stream);
  m.trap(S.lc_menu_iterator,c=>{assert.equal(c.get('a0'),0);clobber(c);c.set('v0',original);});
  call('lc_menu_iterator_site');assert.equal(m.get('v0'),original|| (missing||padding===20?0xffffffff:Math.max(1,padding)));
 }
}
console.log(`PASS ${cases} custom adapter cases across ${fixtures.length} resident placements (${fixtures.filter(f=>f.installed).length} start at installed caller instructions); original/fallback continuations, register/stack preservation, exact Talk target, message namespace, repeated-action budgets and bounded menu progress. Retail services are trapped; complete battles are not simulated.`);
