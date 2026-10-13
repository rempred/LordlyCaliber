'use strict';
// Source-built handle allocator/constructor checks in private RAM. This does not
// claim installation of the capacity feature or a complete battle/save-menu run.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process');
const assert=require('node:assert/strict'),crypto=require('node:crypto');
const {FloatMips}=require('./float-mips.cjs');
const native=path.resolve(__dirname,'..'),root=path.resolve(native,'../..');
const tools=path.join(native,'.local/n64-toolchain');
const pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json')));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
for(const [name,digest] of Object.entries(pins.tools))assert.equal(hash(fs.readFileSync(path.join(tools,name))),digest);
const work=fs.mkdtempSync(path.join(native,'build/enemy-capacity-'));
for(const name of ['enemy-capacity.c','enemy-capacity.h'])fs.copyFileSync(path.join(native,'game/scenarios',name),path.join(work,name));
const symbols={lc_enemy_characters:0x80300000,lc_enemy_types:0x80304000,
 lc_enemy_soldier_hp:0x80305000,lc_enemy_soldier_state:0x80306000,
 lc_capacity_sources:0x80307000,lc_capacity_active_sources:0x80308000,
 lc_enemy_soldier_max_hp:0x80309000,lc_capacity_clear:0x80000110,
 lc_capacity_character:0x80000120,lc_capacity_finalize:0x80000130,lc_capacity_player_construct:0x80000140};
const run=(name,args)=>cp.execFileSync(path.join(tools,name),args,{cwd:work,encoding:'utf8'});
run('cpp.exe',['-P','-undef','-nostdinc','enemy-capacity.c','capacity.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','capacity.i','-o','capacity.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','capacity.o','capacity.s']);
fs.writeFileSync(path.join(work,'capacity.ld'),Object.entries(symbols).map(([n,a])=>`${n} = 0x${a.toString(16)};`).join('\n')+'\nSECTIONS { .text 0x80280000 : { *(.text) } }\n');
run('mips-kmc-elf-ld.exe',['-EB','-T','capacity.ld','-o','capacity.elf','capacity.o']);
run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(n=>'--remove-section='+n),'capacity.elf','capacity.bin']);
const elf=fs.readFileSync(path.join(work,'capacity.elf')),entries={},sh=elf.readUInt32BE(32);
for(let i=0;i<elf.readUInt16BE(48);i++){
 const at=sh+i*40;if(elf.readUInt32BE(at+4)!==2)continue;
 const begin=elf.readUInt32BE(at+16),end=begin+elf.readUInt32BE(at+20),strings=elf.readUInt32BE(sh+elf.readUInt32BE(at+24)*40+16);
 for(let s=begin;s<end;s+=16)if((elf[s+12]&15)===2){const p=strings+elf.readUInt32BE(s);entries[elf.toString('ascii',p,elf.indexOf(0,p))]=elf.readUInt32BE(s+4);}
}
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x80280000,fs.readFileSync(path.join(work,'capacity.bin')));
const sp=0x803f0000,specAt=0x803a0000,read=(at,n)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+n));
function call(name,...args){m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+4*(i-4),v));m.call(entries[name]||name,0x80000100,2000000);assert.equal(m.get('sp'),sp);return m.get('v0');}
const ranges=[['lc_enemy_characters',256*52],['lc_enemy_types',256],['lc_enemy_soldier_hp',512],
 ['lc_enemy_soldier_state',256],['lc_capacity_sources',61*25],['lc_capacity_active_sources',61]];
function reset(){for(const [n,size] of ranges){m.load(symbols[n]-16,Buffer.alloc(size+32,0xa6));m.load(symbols[n],Buffer.alloc(size));}m.w16(symbols.lc_enemy_soldier_max_hp,437);}
function guards(){for(const [n,size] of ranges){assert.deepEqual(read(symbols[n]-16,16),Buffer.alloc(16,0xa6),n);assert.deepEqual(read(symbols[n]+size,16),Buffer.alloc(16,0xa6),n);}}
function snapshot(){return Buffer.concat(ranges.map(([n,size])=>read(symbols[n],size)));}
let callbacks=0,playerCalls=0;
m.trap(symbols.lc_capacity_clear,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
function character(base,c){
 const id=c.get('a0'),selector=c.get('a2'),source=c.r32(c.get('sp')+16),spec=c.get('a1');
 assert(id>0&&id<255);assert(source>=30&&source<60);callbacks++;
 const offset=selector===0?0:selector===1?7:16;
 const data=Buffer.from({length:52},(_,i)=>(i+selector*17+source*3)&255);
 data[18]=c.r8(spec+offset);c.load(base+id*52,data);
}
m.trap(symbols.lc_capacity_character,c=>character(symbols.lc_enemy_characters,c));
m.trap(symbols.lc_capacity_finalize,c=>c.set('v0',7));
m.trap(symbols.lc_capacity_player_construct,c=>{assert(c.get('a1')<30||(c.get('a1')&127)<30);playerCalls++;});
function template(soldiers){const data=Buffer.alloc(35);data[0]=5;data[6]=1;data[7]=soldiers?1:8;data[13]=2;data[14]=3;data[15]=4;data[16]=9;data[22]=5;return data;}
let fullArmies=0,boundaries=0,refusals=0;
for(const soldiers of [false,true]){
 reset();m.load(specAt,template(soldiers));
 const player=Buffer.from({length:30*25},(_,i)=>(i*19+3)&255);m.load(symbols.lc_capacity_sources,player);
 for(let source=30;source<60;source++)assert.equal(call('lc_enemy_construct',specAt,source|(source&1?128:0)),1);
 assert.deepEqual(read(symbols.lc_capacity_sources,player.length),player);
 const used=new Set();for(let source=30;source<60;source++){
  const row=read(symbols.lc_capacity_sources+source*25,25);assert.equal(row[0],source+1);assert.equal(row[1],source&1?1:0x81);
  assert.deepEqual([...row.subarray(7,12)],[0,1,2,3,4]);assert.equal(row[24],7);assert.deepEqual(row.subarray(13,23),Buffer.alloc(10));
  for(let j=0;j<5;j++){const handle=row[j+2];assert(!used.has(handle));used.add(handle);
   const isSoldier=soldiers&&j>0&&j<4;assert.equal(call('lc_member_is_soldier',source,handle),+isSoldier);
   assert.equal(m.r8(symbols.lc_enemy_types+handle),isSoldier?2:1);
   if(isSoldier)assert.equal(m.r16(symbols.lc_enemy_soldier_hp+handle*2),437);
   else assert.equal(m.r8(symbols.lc_enemy_characters+handle*52+18),j===0?5:j===4?9:8);
  }
 }
 assert.equal(used.size,150);guards();fullArmies++;
}
// Every boundary must preserve player classification while consulting enemy tags.
reset();for(const id of [1,99,100,127,128,219,220,254])for(const kind of [0,1,2]){
 m.w8(symbols.lc_enemy_types+id,kind);assert.equal(call('lc_member_is_soldier',29,id),+(id>=100));
 assert.equal(call('lc_member_is_soldier',30,id),+(kind===2));boundaries++;
}
for(const id of [0,255,256,0xffffffff]){const before=snapshot();assert.equal(call('lc_enemy_is_soldier',id),0);call('lc_enemy_release',id);assert.deepEqual(snapshot(),before);}
reset();for(let id=1;id<=254;id++)assert.equal(call('lc_enemy_allocate',id&1?1:2),id);
const full=snapshot();for(const kind of [0,1,2,3,255]){assert.equal(call('lc_enemy_allocate',kind),0);assert.deepEqual(snapshot(),full);refusals++;}
m.load(specAt,template(false));assert.equal(call('lc_enemy_construct',specAt,59),0);assert.deepEqual(snapshot(),full);refusals++;
// Four available handles are insufficient for a five-member squad: no partial writes.
for(const id of [99,100,128,254])call('lc_enemy_release',id);
const four=snapshot();assert.equal(call('lc_enemy_construct',specAt,59),0);assert.deepEqual(snapshot(),four);refusals++;
call('lc_enemy_release',127);assert.equal(call('lc_enemy_construct',specAt,59),1);
assert.deepEqual([...read(symbols.lc_capacity_sources+59*25+2,5)],[99,100,127,128,254]);guards();
for(const source of [60,127,188,255]){const before=snapshot();assert.equal(call('lc_enemy_construct',specAt,source),0);assert.deepEqual(snapshot(),before);refusals++;}
const bad=template(false);bad[23]=6;m.load(specAt,bad);const beforeBad=snapshot();assert.equal(call('lc_enemy_construct',specAt,30),0);assert.deepEqual(snapshot(),beforeBad);refusals++;
assert.equal(call('lc_enemy_construct',0,30),0);assert.deepEqual(snapshot(),beforeBad);refusals++;
m.load(specAt,template(false));const beforePlayer=snapshot();for(const source of [0,29,128,157])assert.equal(call('lc_enemy_construct',specAt,source),1);assert.equal(playerCalls,4);assert.deepEqual(snapshot(),beforePlayer);
// Compare constructor output semantics against the actual retail instructions.
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM||path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();assert.equal(hash(rom),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
const originalEntry=0x80215340;m.load(originalEntry,rom.subarray(0x1957d0,0x195d9c));
m.trap(0x80093380,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x802150bc,c=>character(0x80195560,c));m.trap(0x801d4928,c=>c.set('v0',7));
m.trap(0x80093540,()=>assert.fail('Retail constructor allocation failed'));
let comparisons=0;
for(const soldiers of [false,true])for(const source of [30,31,158]){
 reset();m.load(specAt,template(soldiers));m.load(0x801953f0,Buffer.alloc(0x2300));m.w16(0x801953f2,437);
 call(originalEntry,specAt,source);assert.equal(call('lc_enemy_construct',specAt,source),1);
 const id=source&127,old=read(0x801971f0+id*25,25),now=read(symbols.lc_capacity_sources+id*25,25);
 assert.deepEqual(old.subarray(0,2),now.subarray(0,2));assert.deepEqual(old.subarray(7),now.subarray(7));
 for(let j=0;j<5;j++){
  if(old[j+2]>=100)assert.equal(m.r16(0x801953f4+(old[j+2]-100)*2),m.r16(symbols.lc_enemy_soldier_hp+now[j+2]*2));
  else assert.deepEqual(read(0x80195560+old[j+2]*52,52),read(symbols.lc_enemy_characters+now[j+2]*52,52));
 }
 comparisons++;guards();
}
// Compile all byte-qualified adapters with relocated enemy/source storage and
// execute the actual tagged predicates. Game services outside the selected
// behavior stay controlled callbacks; no installed-image claim is made here.
const catalog=JSON.parse(fs.readFileSync(path.join(native,'game/scenarios/capacity/owners.json')));
const mapBase=0x80310000,pointerBase=0x80314000,adapters={},binding={};let nextModule=0x80290000;
function relocate(a){
 if(a>=0x80195560&&a<0x801969b0)return symbols.lc_enemy_characters+a-0x80195560;
 if(a>=0x801971f0&&a<0x801976d2)return symbols.lc_capacity_sources+a-0x801971f0;
 if(a>=0x801f1080&&a<0x801f3600)return mapBase+a-0x801f1080;
 if(a>=0x801f0cb0&&a<0x801f0d78)return pointerBase+a-0x801f0cb0;
 if(a>=0x801f0bd8&&a<0x801f0ca0)return 0x80315000+a-0x801f0bd8;
 if(a>=0x801977d8&&a<0x8019780a)return symbols.lc_capacity_active_sources+a-0x801977d8;
 if(a===0x8019532c)return symbols.lc_enemy_soldier_hp;
 if(a===0x80195480)return symbols.lc_enemy_soldier_state;
 return a;
}
for(const owner of catalog.owners){
 const name=path.basename(owner.adaptedSource,'.c'),source=fs.readFileSync(path.join(native,owner.adaptedSource));
 assert.equal(hash(source),owner.adaptedSha256);assert.equal(hash(rom.subarray(owner.start,owner.start+owner.size)),owner.retailSha256);
 fs.writeFileSync(path.join(work,name+'.c'),source);
 run('cpp.exe',['-P','-undef','-nostdinc',name+'.c',name+'.i']);
 run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char',name+'.i','-o',name+'.s']);
 run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o',name+'.o',name+'.s']);
 const binds={...Object.fromEntries(Object.entries(owner.symbols).map(([n,a])=>[n,relocate(a)])),...symbols,...entries};
 binding[name]=binds;
 for(const [n,a] of Object.entries(owner.symbols))if(n.startsWith('func_'))m.trap(a,c=>c.set('v0',0));
 fs.writeFileSync(path.join(work,name+'.ld'),Object.entries(binds).map(([n,a])=>`${n} = 0x${a.toString(16)};`).join('\n')+`\nSECTIONS { .text 0x${nextModule.toString(16)} : { *(.text) } }\n`);
 run('mips-kmc-elf-ld.exe',['-EB','-T',name+'.ld','-o',name+'.elf',name+'.o']);
 run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(n=>'--remove-section='+n),name+'.elf',name+'.bin']);
 const module=fs.readFileSync(path.join(work,name+'.bin'));m.load(nextModule,module);adapters[name]=nextModule;nextModule=((nextModule+module.length+15)&~15)>>>0;
}
let adapterChecks=0;
m.trap(0x80093380,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
function setupMember(handle,kind,source=35){
 reset();m.load(mapBase,Buffer.alloc(61*192));m.load(pointerBase,Buffer.alloc(61*4));
 m.w32(pointerBase+source*4,mapBase+source*192);m.w8(mapBase+source*192+4,source);
 const row=symbols.lc_capacity_sources+source*25; m.w8(row,source+1);m.w8(row+1,0x81);m.w8(row+2,handle);
 m.w8(symbols.lc_enemy_types+handle,kind);m.w16(symbols.lc_enemy_characters+handle*52+24,123);
 m.w8(symbols.lc_enemy_characters+handle*52+51,2);m.w16(symbols.lc_enemy_soldier_hp+handle*2,87);
 return {row,unit:mapBase+source*192,character:symbols.lc_enemy_characters+handle*52};
}
for(const handle of [1,99,100,127,128,219,220,254])for(const kind of [1,2]){
 const {row,unit,character:ch}=setupMember(handle,kind);
 assert.equal(call(adapters.func_0012DA10,unit),kind===1?1:0);adapterChecks++;
 assert.equal(call(adapters.func_0012E7E8,unit),ch);adapterChecks++;
 assert.equal(call(adapters.func_0012F010,35),0);adapterChecks++;
 if(kind===1)m.w8(ch+51,6);else m.w8(symbols.lc_enemy_soldier_state+handle,4);
 assert.equal(call(adapters.func_0012F010,35),1);adapterChecks++;
 // Live/dead scans must visit the selected payload, including Soldier handles<100.
 m.w8(binding.func_0012ECDC.D_8018F481,1);
 assert.equal(call(adapters.func_0012ECDC,35),1);adapterChecks++;
 if(kind===1)m.w16(ch+24,0);else m.w16(symbols.lc_enemy_soldier_hp+handle*2,0);
 assert.equal(call(adapters.func_0012ECDC,35),0);adapterChecks++;guards();
}
// Shared movement classifier: an ordinary high handle uses its class movement;
// a low Soldier handle uses the Soldier movement value2.
for(const kind of [1,2]){
 const {row,character:ch}=setupMember(220,kind);m.w8(ch+18,9);m.w8(0x80187c14+9*72+44,17);m.w8(0x8018f481,1);
 m.trap(binding.func_00129068.func_00040f88,c=>{assert.equal(c.get('a0'),kind===1?17:2);assert.equal(c.get('a1'),0);c.set('v0',c.get('a0'));});
 assert.equal(call(adapters.func_00129068,row),kind===1?17:2);adapterChecks++;
}
// Healing tests the separate Soldier maximum-HP scalar, including handle99.
{
 const {row,unit,character:ch}=setupMember(220,1);m.w16(ch+22,600);m.w16(ch+24,10);
 m.w8(row+3,99);m.w8(symbols.lc_enemy_types+99,2);m.w16(symbols.lc_enemy_soldier_hp+198,400);m.w8(row+13,3);
 m.trap(binding.func_00124018.func_0014F300,c=>c.set('v0',3));
 assert.equal(call(adapters.func_00124018,35,0,220),1);assert.equal(m.r16(ch+24),160);
 assert.equal(m.r16(symbols.lc_enemy_soldier_hp+198),437);assert.equal(m.r8(row+13),0);adapterChecks++;
}
// Combat writes both sides through the selected pool; enemy/allied need not be
// on a particular screen side. Other battle callbacks are controlled above.
const actor=0x80318000;
for(const side of [0,1])for(const handle of [99,100,128,254])for(const kind of [1,2]){
 const {character:ch}=setupMember(handle,kind);m.load(actor,Buffer.alloc(248));m.w8(actor+0xf6,handle);m.w16(actor+0x20,71);m.w8(actor+0x34,28);
 m.w8(0x801976dc,side?35:0);m.w8(0x801976e8,side?0:35);
 const binds=binding.func_0021C3B0;m.trap(binds.func_0020C478,c=>c.set('v0',c.get('a0')===0?actor:0));m.trap(binds.func_0020C014,c=>c.set('v0',side));
 call(adapters.func_0021C3B0);
 assert.equal(kind===1?m.r16(ch+24):m.r16(symbols.lc_enemy_soldier_hp+handle*2),71);
 if(kind===1)assert.equal(m.r8(ch+27),28);adapterChecks++;guards();
}
// Release and cleanup must make every high/low tagged handle reusable, preserve
// record0's Soldier template and leave the complete player roster untouched.
for(const handle of [1,99,100,127,128,219,220,254])for(const kind of [1,2]){
 const {row,unit}=setupMember(handle,kind,59);m.w32(unit,0xffffffff);
 call(adapters.func_0012A828,59);
 assert.equal(m.r8(symbols.lc_enemy_types+handle),0);
 assert.deepEqual(read(symbols.lc_enemy_characters+handle*52,52),Buffer.alloc(52));
 assert.equal(m.r16(symbols.lc_enemy_soldier_hp+handle*2),0);
 assert.equal(m.r8(symbols.lc_enemy_soldier_state+handle),0);
 assert.equal(m.r8(row+1),0x80);assert.equal(m.r32(unit)&0x19,0);adapterChecks++;
}
{
 reset();const template0=Buffer.from({length:52},(_,i)=>i*3&255),players=Buffer.from({length:750},(_,i)=>i*7&255);
 m.load(symbols.lc_enemy_characters,template0);m.load(symbols.lc_capacity_sources,players);
 m.load(specAt,template(false));for(let source=30;source<60;source++)assert.equal(call('lc_enemy_construct',specAt,source),1);
 m.load(mapBase,Buffer.alloc(61*192));for(let i=0;i<61;i++)m.w32(pointerBase+i*4,mapBase+i*192);
 m.w32(binding.func_0012A050.D_801F367C,61);call(adapters.func_0012A050);
 assert.deepEqual(read(symbols.lc_enemy_characters,52),template0);
 assert.deepEqual(read(symbols.lc_enemy_characters+52,255*52),Buffer.alloc(255*52));
 assert.deepEqual(read(symbols.lc_enemy_types,256),Buffer.alloc(256));
 assert.deepEqual(read(symbols.lc_enemy_soldier_hp,512),Buffer.alloc(512));
 assert.deepEqual(read(symbols.lc_capacity_sources,750),players);
 assert.deepEqual(read(symbols.lc_capacity_sources+750,775),Buffer.alloc(775));guards();adapterChecks++;
}
{
 reset();m.load(0x80315000-16,Buffer.alloc(356+32,0xa6));
 for(const handle of [99,100,128,220,254]){
  m.w8(symbols.lc_enemy_types+handle,1);m.w8(symbols.lc_enemy_characters+handle*52+18,4);m.w16(symbols.lc_enemy_characters+handle*52+24,71);
 }
 m.w8(symbols.lc_capacity_sources+59*25+1,0x81);m.w8(symbols.lc_capacity_sources+59*25+2,254);
 m.w8(symbols.lc_enemy_types+128,2);
 call(adapters.func_00128E80);
 for(const [handle,expected] of [[0,1],[99,0],[100,0],[128,1],[220,0],[254,1],[255,1]])assert.equal(m.r8(0x80315000+100+handle)&1,expected);
 assert.deepEqual(read(0x80315000-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(0x80315000+356,16),Buffer.alloc(16,0xa6));adapterChecks++;
}
const extraChecks=require('./capacity-battle-training.cjs')({m,read,call,rom,symbols,entries,run,work,native,reset,guards});
const neutralChecks=require('./capacity-neutral-mips.cjs')({m,read,call,rom,symbols,entries,run,work,native,reset,guards});
const report={fullArmies,boundaries,refusals,retailConstructorComparisons:comparisons,adapters:catalog.owners.length,adapterChecks,...extraChecks,...neutralChecks,codeBytes:fs.statSync(path.join(work,'capacity.bin')).size,
 scope:'Compiled allocator/constructor and selected map/supply/combat writeback adapters, with controlled external callbacks. No installed ROM, save/load or emulator claim.'};
fs.writeFileSync(path.join(work,'report.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({...report,work}));
