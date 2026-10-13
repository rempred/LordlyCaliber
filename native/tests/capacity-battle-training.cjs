'use strict';
// Called by enemy-capacity-mips.cjs with its compiled allocator and private RAM.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
module.exports=function({m,read,call,rom,symbols,entries,run,work,native,reset,guards}){
 const binds={...symbols,...entries},entry=0x802d0000;
 function link(name,address,bindings){
  fs.writeFileSync(path.join(work,name+'.ld'),Object.entries(bindings).map(([n,a])=>`${n} = 0x${a.toString(16)};`).join('\n')+`\nSECTIONS { .text 0x${address.toString(16)} : { *(.text) } }\n`);
  run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o',name+'.o',name+'.s']);
  run('mips-kmc-elf-ld.exe',['-EB','-T',name+'.ld','-o',name+'.elf',name+'.o']);
  run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(n=>'--remove-section='+n),name+'.elf',name+'.bin']);
  const code=fs.readFileSync(path.join(work,name+'.bin'));m.load(address,code);return code;
 }
 let battleChecks=0,trainingChecks=0;
 for(const [side,sourceAddress] of [[0,0x801976e8],[1,0x801976dc]])for(const kind of ['type','hp','fields']){
  const name=`battle_${side}_${kind}`,ordinary=0x80000200,soldier=0x80000210,done=0x80000220;
  fs.copyFileSync(path.join(native,'game/scenarios/capacity-battle-'+kind+'.s'),path.join(work,name+'.s'));
  link(name,entry,{...binds,lc_battle_source:sourceAddress,lc_battle_ordinary:ordinary,lc_battle_soldier:soldier,lc_battle_hp_continue:done,lc_battle_fields_continue:done});
  let result;
  m.trap(ordinary,()=>result=1);m.trap(soldier,()=>result=2);m.trap(done,()=>result=3);
  for(const source of [0,29,30,59])for(const handle of [1,99,100,127,128,219,220,254])for(const tag of [1,2]){
   if(source<30&&handle<100&&kind!=='type')continue;
   reset();m.w8(sourceAddress,source);m.w8(symbols.lc_enemy_types+handle,tag);
   m.w16(symbols.lc_enemy_soldier_hp+handle*2,321);m.w8(symbols.lc_enemy_soldier_state+handle,5);m.w16(symbols.lc_enemy_soldier_max_hp,777);
   const playerBase=0x80318000,actor=0x80319000;
   m.load(playerBase-200,Buffer.alloc(1024));m.load(actor,Buffer.alloc(248));
   m.w16(playerBase+handle*2-196,123);m.w16(playerBase+2,888);m.w8(playerBase+handle+144,4);m.w8(actor+246,handle);
   for(let r=1;r<32;r++)m.r[r]=(0x81110000+r*256)>>>0;
   m.set('s1',actor);m.set('s2',handle);m.set('s3',playerBase);m.set('v0',handle*2);
   const before=Array.from(m.r);result=0;call(entry);
   assert.equal(result,kind==='type'?(source<30?handle<100:tag!==2)?1:2:3);
   if(kind==='type')assert.equal(m.get('v0'),handle*2);
   else if(kind==='hp')assert.equal(m.get('v0'),source<30?123:321);
   else{assert.equal(m.r16(actor+32),source<30?123:321);assert.equal(m.r16(actor+34),source<30?888:777);assert.equal(m.get('v0'),source<30?4:5);}
   for(let r=3;r<29;r++)if(kind!=='fields'||r!==3)assert.equal(m.r[r]>>>0,before[r]>>>0,`preserved register ${r}`);
   guards();battleChecks++;
  }
  for(const at of [ordinary,soldier,done])m.traps.delete(at);
 }
 // Compile the same maintained training source with its capacity option. The
 // retail constructor is the oracle for template/level/formation/transition.
 const trainingName='capacity_training',trainingEntry=0x802d1000;
 fs.writeFileSync(path.join(work,trainingName+'.c'),'#define LC_ENEMY_CAPACITY 1\n'+fs.readFileSync(path.join(native,'game/tools/training-constructor.c'),'utf8'));
 run('cpp.exe',['-P','-undef','-nostdinc',trainingName+'.c',trainingName+'.i']);
 run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char',trainingName+'.i','-o',trainingName+'.s']);
 const services={load_resource:0x8009dbb8,resource_size:0x8009daf4,free_resource:0x800712c4,load_archive:0x8007acb0,
  copy_bytes:0x80080998,clear_bytes:0x80093380,construct_member:0x801d0840,prepare_battle:0x80172394,set_mode:0x8017223c,release_battle:0x801707f4};
 link(trainingName,trainingEntry,{...binds,...services});
 function training(expanded,record,levels,mode,already=false){
  const c=expanded?m:new FloatMips(),calls=[],transitions=[];if(expanded)reset();else c.ram=new Uint8Array(0x400000);
  if(!expanded)c.load(0x801d0a8c,rom.subarray(0x23a5ec,0x23b048));
  const sourceBase=expanded?symbols.lc_capacity_sources:0x801971f0,chars=expanded?symbols.lc_enemy_characters:0x80195560;
  c.set('sp',0x803f0000);c.load(0x803a0000,record);c.w32(0x8039f000,0x123456);c.w8(0x80193672,30);
  c.w8(0x801976d8,mode);c.w8(0x801976dc,2);c.w8(0x801976e8,already?30:0);c.w16(0x80197b60,0);c.w16(0x80193670,0);
  for(let i=0;i<5;i++)c.w8(sourceBase+52+i,i<levels.length?i+1:0);
  levels.forEach((level,i)=>c.w8(0x80193bd3+(i+1)*56,level));
  c.trap(services.load_resource,x=>x.set('v0',x.get('a1')===0x21ae74c?0x8039f000:0x8039e000));
  c.trap(services.resource_size,x=>x.set('v0',record.length));c.trap(services.free_resource,()=>{});
  c.trap(services.load_archive,x=>x.w32(x.get('a1')+8,0x803a0000));
  c.trap(services.copy_bytes,x=>x.load(x.get('a0'),x.ram.slice(x.phys(x.get('a1')),x.phys(x.get('a1'))+x.get('a2'))));
  c.trap(services.clear_bytes,x=>x.load(x.get('a0'),new Uint8Array(x.get('a1'))));
  c.trap(services.construct_member,x=>{const a=['a0','a1','a2','a3'].map(n=>x.get(n));a.push(x.r32(x.get('sp')+16));calls.push(a.slice(1));x.w8(a[0]+17,a[1]);x.w16(a[0]+22,100);});
  for(const at of [services.prepare_battle,services.set_mode,services.release_battle])c.trap(at,x=>transitions.push([at,at===services.set_mode?x.get('a0'):0]));
  c.call(expanded?trainingEntry:0x801d0a8c,0x80000100,1000000);
  assert.equal(c.get('sp'),0x803f0000);
  const source=sourceBase+30*25,unit=Array.from(c.ram.subarray(c.phys(source),c.phys(source)+25)),members=[];
  for(const id of unit.slice(2,7))if(id){
   const soldier=expanded?c.r8(symbols.lc_enemy_types+id)===2:id>=100;
   members.push(soldier?['soldier',c.r16(expanded?symbols.lc_enemy_soldier_hp+id*2:0x801953f4+(id-100)*2),c.r8(expanded?symbols.lc_enemy_soldier_state+id:0x801954e4+id-100)]:['ordinary',c.r8(chars+id*52+17)]);
  }
  if(expanded)guards();return {calls,transitions,members,unit:[...unit.slice(0,2),...unit.slice(7)]};
 }
 for(const cls of [1,2,9])for(const followers of [0,1,4])for(const levels of [[],[4,9,7]])for(const mode of [0,8]){
  const r=Buffer.alloc(35);r[0]=cls;r[1]=253;r[6]=9;r[7]=5;r[8]=2;r[16]=1;r[17]=255;r.writeUInt16BE(23,2);r.writeUInt16BE(255,4);
  for(let i=0;i<followers;i++)r[i<3?13+i:22]=[1,3,5,7][i];
  assert.deepEqual(training(true,r,levels,mode),training(false,r,levels,mode));trainingChecks++;
 }
 const ls=Buffer.alloc(175);ls.write('LS',25);ls[6]=9;
 for(let i=0;i<5;i++){ls[i*35]=i+2;ls[i*35+1]=(i-2)&255;ls.writeUInt16BE(20+i,i*35+2);ls.writeUInt16BE(30+i,i*35+4);if(i){ls.writeUInt16BE(i,25+i*2);ls[i<4?12+i:22]=[0,1,3,5,7][i];}}
 ls.set(ls.subarray(35,41),7);ls.set(ls.subarray(140,146),16);
 const independent=training(true,ls,[12,7,9],8);
 assert.deepEqual(independent.calls.slice(1),Array.from({length:5},(_,i)=>[i+2,10+i,20+i,30+i]));
 assert.deepEqual(independent.unit.slice(2,7),[8,0,2,4,6]);trainingChecks++;
 assert.deepEqual(training(true,ls,[12],8,true).calls,[]);trainingChecks++;
 return {battleChecks,trainingChecks};
};
