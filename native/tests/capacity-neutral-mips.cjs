'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
module.exports=function({m,read,call,rom,symbols,entries,run,work,native,reset,guards}){
 for(const n of ['custom.h','custom.c'])fs.copyFileSync(path.join(native,'game/encounters',n),path.join(work,n));
 fs.copyFileSync(path.join(native,'game/scenarios/capacity-neutral.c'),path.join(work,'capacity-neutral.c'));
 const services={lc_capacity_create_record:0x80000800,lc_capacity_original_persuasion:0x80000810,
  lc_dma_read:0x80000820,lc_random:0x80000830,lc_character_create:0x80000800,
  lc_capacity_original_talk_target:0x800008b0,lc_actor_at_index:0x80000840,lc_persuasion_filter_a:0x80000850,lc_persuasion_filter_b:0x80000860,
  lc_persuasion_filter_c:0x80000870,lc_persuasion_filter_d:0x80000880,lc_persuasion_eligible:0x80000890,lc_persuasion_random:0x800008a0};
 const state=0x803a8000,binds={...symbols,...entries,...services,lc_custom_state:state,lc_custom_index:0x2900000,
  lc_staged_scenario:0x801936a7,lc_character_records:symbols.lc_enemy_characters,lc_source_records:symbols.lc_capacity_sources};
 function build(custom){
  const name='neutral_'+custom,entry=0x802e0000;
  fs.writeFileSync(path.join(work,name+'.c'),(custom?'#define LC_CAPACITY_CUSTOM 1\n#define LC_ENEMY_CAPACITY 1\n#include "custom.c"\n':'')+'#include "capacity-neutral.c"\n');
  run('cpp.exe',['-P','-undef','-nostdinc',name+'.c',name+'.i']);
  run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char',name+'.i','-o',name+'.s']);
  run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o',name+'.o',name+'.s']);
  fs.writeFileSync(path.join(work,name+'.ld'),Object.entries(binds).map(([n,a])=>`${n} = 0x${a.toString(16)};`).join('\n')+`\nSECTIONS { .text 0x${entry.toString(16)} : { *(.text) } }\n`);
  run('mips-kmc-elf-ld.exe',['-EB','-T',name+'.ld','-o',name+'.elf',name+'.o']);
  run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(n=>'--remove-section='+n),name+'.elf',name+'.bin']);
  m.load(entry,fs.readFileSync(path.join(work,name+'.bin')));
  const elf=fs.readFileSync(path.join(work,name+'.elf')),sh=elf.readUInt32BE(32),result={};
  for(let i=0;i<elf.readUInt16BE(48);i++){
   const s=sh+i*40;if(elf.readUInt32BE(s+4)!==2)continue;
   const begin=elf.readUInt32BE(s+16),end=begin+elf.readUInt32BE(s+20),strings=elf.readUInt32BE(sh+elf.readUInt32BE(s+24)*40+16);
   for(let a=begin;a<end;a+=16)if((elf[a+12]&15)===2){const p=strings+elf.readUInt32BE(a);result[elf.toString('ascii',p,elf.indexOf(0,p))]=elf.readUInt32BE(a+4);}
  }
  return result;
 }
 let createCalls=[],persuasionResult=0,neutralChecks=0,lifecycleChecks=0;
 m.trap(services.lc_capacity_create_record,c=>{
  const base=c.get('a0'),cls=c.get('a1'),level=c.get('a2');
  createCalls.push([cls,level,c.get('a3'),c.r32(c.get('sp')+16)]);
  c.load(base,Buffer.from({length:52},(_,i)=>(i*11+cls)&255));c.w8(base+17,cls);c.w8(base+18,cls);
 });
 m.trap(services.lc_capacity_original_persuasion,c=>c.set('v0',persuasionResult));
 m.trap(services.lc_capacity_original_talk_target,c=>c.set('v0',0x803b8000));
 const playerBase=0x80193bc0;
 function fullArmy(){
  reset();createCalls=[];m.load(state,Buffer.alloc(144));
  m.load(playerBase,Buffer.from({length:5600},(_,i)=>i*17&255));
  m.load(symbols.lc_capacity_sources,Buffer.from({length:750},(_,i)=>i*19&255));
  const spec=0x803a0000,b=Buffer.alloc(35);b[0]=5;b[6]=1;b[7]=8;b[13]=2;b[14]=3;b[15]=4;b[16]=9;b[22]=5;m.load(spec,b);
  for(let source=30;source<60;source++)assert.equal(call('lc_enemy_construct',spec,source),1);
  m.w8(0x803b8000+246,1);
 }
 function mapSnapshot(){return Buffer.concat([read(symbols.lc_capacity_sources,60*25),read(symbols.lc_enemy_characters,151*52),read(symbols.lc_enemy_types,151),read(playerBase,5600)]);}
 let extra;
 for(const custom of [false,true]){
  const api=build(custom);
  for(const cls of [1,2,99])for(const special of [false,true]){
   fullArmy();const before=mapSnapshot();m.w8(0x801976e8,cls);m.w8(0x801936ac,8);m.w8(0x801936e0,+special);m.w8(0x801936a7,special?63:5);m.w8(0x80193671,1);
   call(api.lc_capacity_neutral_construct);
   assert.equal(m.r8(0x801976e8),60);const unit=symbols.lc_capacity_sources+60*25,id=m.r8(unit+3);assert.equal(id,151);assert.equal(m.r8(unit+1),0x81);assert.equal(m.r8(unit+8),special?7:4);
   assert.equal(m.r8(symbols.lc_enemy_types+id),cls===1?2:1);
   if(cls===1){assert.equal(m.r16(symbols.lc_enemy_soldier_hp+id*2),437);assert.equal(m.r8(symbols.lc_enemy_soldier_state+id),1);assert.deepEqual(createCalls,[]);}
   else assert.deepEqual(createCalls,[[cls,special?23:8,0,0]]);
   assert.deepEqual(mapSnapshot(),before);guards();call(api.lc_capacity_neutral_cleanup);
   assert.deepEqual(mapSnapshot(),before);assert.deepEqual(read(unit,25),Buffer.alloc(25));assert.equal(m.r8(symbols.lc_enemy_types+id),0);assert.equal(m.r8(0x801976e8),cls);assert.equal(m.r8(0x80193671),0);guards();neutralChecks++;
  }
  if(custom){
   fullArmy();const before=mapSnapshot(),p=state+16;
   m.w32(state,1);m.w32(state+4,1);m.w8(p+1,5);
   for(let i=0;i<5;i++){m.w8(p+8+i*8,i+2);m.w8(p+9+i*8,i);m.w8(p+10+i*8,i*2);m.w16(p+12+i*8,i+10);m.w16(p+14+i*8,i+20);}
   m.w8(0x801976e8,2);m.w8(0x801936ac,10);m.w8(0x801936e0,0);
   call(api.lc_capacity_neutral_construct);const unit=symbols.lc_capacity_sources+60*25;
   assert.deepEqual([...read(unit+2,5)],[151,152,153,154,155]);assert.deepEqual([...read(unit+7,5)],[0,2,4,6,8]);
   assert.deepEqual(createCalls,Array.from({length:5},(_,i)=>[i+2,i+10,i+10,i+20]));assert.deepEqual(mapSnapshot(),before);
   call(api.lc_capacity_neutral_cleanup);assert.deepEqual(mapSnapshot(),before);for(let i=151;i<=155;i++)assert.equal(m.r8(symbols.lc_enemy_types+i),0);guards();neutralChecks++;
  }
  for(const free of [0,1,99])for(const result of [0,1,2]){
   fullArmy();for(let id=0;id<100;id++)m.w8(playerBase+id*56+17,id===free?0:2);
   m.w32(state+8,1);const before=read(playerBase,5600);persuasionResult=result;
   assert.equal(call(api.lc_capacity_persuasion),result===0&&free===0?2:result);assert.deepEqual(read(playerBase,5600),before);
   if(custom)assert.equal(m.r32(state+8),result===0&&free===0?0:1);lifecycleChecks++;
  }
  for(const tag of [0,2]){
   fullArmy();m.w8(playerBase+56+17,0);m.w8(symbols.lc_enemy_types+1,tag);persuasionResult=0;
   const before=read(playerBase,5600);assert.equal(call(api.lc_capacity_persuasion),2);assert.deepEqual(read(playerBase,5600),before);lifecycleChecks++;
  }
  for(const id of [99,100,128,220,254]){
   reset();m.w8(symbols.lc_enemy_types+id,1);m.load(symbols.lc_enemy_characters+id*52,Buffer.alloc(52,7));
   m.w8(symbols.lc_capacity_sources+59*25+2,id);call(api.lc_capacity_delete_enemy,id);
   assert.equal(m.r8(symbols.lc_enemy_types+id),1);for(const at of [17,18,51])assert.equal(m.r8(symbols.lc_enemy_characters+id*52+at),0);
   m.w8(symbols.lc_capacity_sources+59*25+2,0);call(api.lc_capacity_delete_enemy,id);assert.equal(m.r8(symbols.lc_enemy_types+id),0);guards();lifecycleChecks++;
  }
  for(const freeCount of [0,1,3,5])for(const soldier of [false,true]){
   fullArmy();const unit=symbols.lc_capacity_sources+59*25,slots=0x803a9000;
   m.load(unit+2,Buffer.from([148,0,149,0,150]));if(soldier)m.w8(symbols.lc_enemy_types+149,2);
   for(let id=0;id<100;id++)m.w8(playerBase+id*56+17,id===0||id>99-freeCount?0:2);
   m.load(slots,Buffer.alloc(10,0xa5));const before=mapSnapshot(),success=freeCount>=3&&!soldier;
   assert.equal(call(api.lc_capacity_transfer_slots,59,slots),+success);assert.deepEqual(mapSnapshot(),before);
   if(success)assert.deepEqual(Array.from({length:5},(_,i)=>m.r16(slots+i*2)),[100-freeCount,0,101-freeCount,0,102-freeCount]);else assert.deepEqual(read(slots,10),Buffer.alloc(10,0xa5));
   guards();lifecycleChecks++;
  }
  if(custom)extra=require('./capacity-extra-mips.cjs')({m,read,call,rom,symbols,entries:{...entries,...api},run,work,native,reset,guards});
 }
 return {neutralChecks,lifecycleChecks,...extra};
};
