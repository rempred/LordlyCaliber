'use strict';
// Execute the maintained legacy migration and type-extension callbacks compiled
// for N64. Descriptor offsets are checked independently against the retail ROM.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process');
const assert=require('node:assert/strict'),crypto=require('node:crypto');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const {FloatMips}=require('./float-mips.cjs');
const native=path.resolve(__dirname,'..'),root=path.resolve(native,'../..');
const tools=path.join(native,'.local/n64-toolchain'),pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json')));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
for(const [name,digest] of Object.entries(pins.tools))assert.equal(hash(fs.readFileSync(path.join(tools,name))),digest);
const work=fs.mkdtempSync(path.join(native,'build/capacity-save-'));
for(const [folder,names] of Object.entries({classes:['save-format.c','save-runtime.c','save-image.c','save-compression.c','save_format.h','save_checksums.h','save_compression.h'],
 scenarios:['capacity-save.c','capacity-save.h','capacity-tags.c','capacity-quick-save.c','enemy-capacity.h']}))
 for(const name of names)fs.copyFileSync(path.join(native,'game',folder,name),path.join(work,name));
fs.writeFileSync(path.join(work,'test.c'),['save-format.c','save-compression.c','save-image.c','capacity-save.c','capacity-tags.c','capacity-quick-save.c','save-runtime.c'].map(n=>fs.readFileSync(path.join(work,n),'utf8')).join('\n'));
const run=(tool,args)=>cp.execFileSync(path.join(tools,tool),args,{cwd:work,encoding:'utf8'});
run('cpp.exe',['-P','-undef','-nostdinc','-DLC_SAVE_RUNTIME_FORMAT=3','test.c','test.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','test.i','-o','test.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','test.o','test.s']);
const storage={lc_save_cache:0x800a83b8,lc_save_dirty:0x800a83bc,lc_discovery_flags:0x80250000,
 lc_legacy_discovery:0x80196a76,lc_native_save_alloc:0x80070f30,lc_native_sram_io:0x8008a0f0,
 lc_native_main_save:0x8016cf64,lc_native_save_free:0x800712c4,lc_native_save_cleanup:0x80074bf0,
 lc_save_callbacks:0x800a824c,lc_native_options_init:0x8016cd90,lc_native_options_load:0x8016cdcc,
 lc_native_options_save:0x8016cdf4,lc_native_clear:0x80093380,
 lc_enemy_types:0x80320000,lc_enemy_soldier_hp:0x80320200,lc_enemy_soldier_state:0x80320500,
 lc_enemy_characters:0x802a0000,lc_capacity_name_work:0x802c0000,lc_capacity_quick_descriptor:0x802b2000,
 lc_capacity_pack:0x80074494,lc_capacity_unpack:0x800742f4,lc_capacity_name_encode:0x8017aa18,lc_capacity_name_decode:0x8017aaf4};
fs.writeFileSync(path.join(work,'test.ld'),Object.entries(storage).map(([n,a])=>`${n} = 0x${a.toString(16)};`).join('\n')+'\nSECTIONS { .text 0x80280000 : { *(.text) } }\n');
run('mips-kmc-elf-ld.exe',['-EB','-T','test.ld','-o','test.elf','test.o']);
run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(n=>'--remove-section='+n),'test.elf','test.bin']);
const elf=fs.readFileSync(path.join(work,'test.elf')),sh=elf.readUInt32BE(32),entries={};
for(let i=0;i<elf.readUInt16BE(48);i++){
 const h=sh+i*40;if(elf.readUInt32BE(h+4)!==2)continue;
 const start=elf.readUInt32BE(h+16),end=start+elf.readUInt32BE(h+20),strings=elf.readUInt32BE(sh+elf.readUInt32BE(h+24)*40+16);
 for(let p=start;p<end;p+=16)if((elf[p+12]&15)===2){const n=strings+elf.readUInt32BE(p);entries[elf.toString('ascii',n,elf.indexOf(0,n))]=elf.readUInt32BE(p+4);}
}
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x80280000,fs.readFileSync(path.join(work,'test.bin')));
const recordAt=0x80300000,sp=0x803f0000,read=(at,n)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+n));
function call(name,...args){m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+4*(i-4),v));m.call(entries[name]||name,0x80000100,200000000);assert.equal(m.get('sp'),sp);return m.get('v0');}
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM||path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();assert.equal(hash(rom),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
function descriptor(at){let bit=0;const groups=[];for(;m.r32(at);at+=16){let each=0;const fields=[];for(let p=m.r32(at+8);m.r8(p+1)&127;p+=3){fields.push({offset:m.r8(p),width:m.r8(p+1)&127,bits:m.r8(p+2),bit:each});each+=m.r8(p+2);}groups.push({base:m.r32(at),stride:m.r32(at+4),count:m.r32(at+12),bit,each,fields});bit+=each*m.r32(at+12);}return groups;}
const main=descriptor(0x80187444),quick=descriptor(0x80187618);
assert.equal(main.find(g=>g.base===0x801953f4).bit,41426);
assert.deepEqual(quick.map(g=>[g.count,g.each,g.bit]),[[100,273,0],[20,137,27300],[16,16,30040]]);
assert.equal(quick[0].fields.find(f=>f.offset===18).bit,112);
assert.equal(quick[1].fields.find(f=>f.offset===2).bit,14);
const getBits=(b,at,n)=>{let v=0;for(let j=0;j<n;j++)v=(v*2+((b[(at+j)>>3]>>>(7-((at+j)&7)))&1))>>>0;return v;};
function setBits(b,at,n,value){for(let j=0;j<n;j++){const bit=at+j,mask=1<<(7-(bit&7));b[bit>>3]=(b[bit>>3]&~mask)|(((value>>>(n-j-1))&1)?mask:0);}}
let random=0x12345678;const next=()=>{random^=random<<13;random^=random>>>17;random^=random<<5;return random>>>0;};
let migrations=0,refusals=0,tagCases=0,legacyFixture,legacyExpanded;
for(let seed=0;seed<6;seed++){
 const old=Buffer.from({length:19208},()=>next()&255),q=old.subarray(6256),types=Buffer.alloc(256);
 for(let id=1;id<100;id++){const cls=seed&1?(id%3?4:0):0;setBits(q,id*273+112,8,cls);if(cls)types[id]=1;}
 for(let id=100;id<220;id++){const hp=seed&2?(id%4?437:0):0;setBits(old,42*8+41426+(id-100)*10,10,hp);if(hp)types[id]=2;}
 for(let i=0;i<20;i++)for(let j=0;j<5;j++){const id=(i*5+j+seed*20)%220;setBits(q,27300+i*137+14+j*8,8,id);if(id)types[id]=id<100?1:2;}
 m.load(recordAt-16,Buffer.alloc(27256+32,0xa6));m.load(recordAt,old);
 assert.equal(call('lc_save_record_seal',recordAt,19208,15,1),1);
 const before=read(recordAt,27256),signed=before.subarray(0,19208);
 if(seed===0)legacyFixture=Buffer.from(signed);
 for(const capacity of [0,19208,27255]){assert.equal(call('lc_save_expand_capacity',recordAt,capacity),0);assert.deepEqual(read(recordAt,27256),before);refusals++;}
 assert.equal(call('lc_save_expand_capacity',recordAt,27256),1);const now=read(recordAt,27256),nq=now.subarray(6256);
 assert.equal(call('lc_save_record_valid',recordAt,27256,15,3),1);
 if(seed===0)legacyExpanded=Buffer.from(now);
 assert.deepEqual(now.subarray(12,6256),signed.subarray(12,6256));
 for(const [a,b,bits] of [[0,0,27300],[69888,27300,2740],[74135,30040,256]])
  for(let i=0;i<bits;i++)assert.equal(getBits(nq,a+i,1),getBits(q,b+i,1));
 for(const [a,b] of [[27300,69888],[72628,74135]])for(let i=a;i<b;i++)assert.equal(getBits(nq,i,1),0);
 assert.equal(nq[9299],q[3787]);assert.equal(nq[9298]&1,0);
 assert.deepEqual(now.subarray(15556,15812),types);assert.deepEqual(now.subarray(15812,16220),Buffer.alloc(408));
 for(const [to,from,n] of [[16220,10044,40],[16284,10084,8400],[26532,18484,724]])assert.deepEqual(now.subarray(to,to+n),signed.subarray(from,from+n));
 assert.deepEqual(now.subarray(16260,16284),Buffer.alloc(24));assert.deepEqual(now.subarray(24684,26532),Buffer.alloc(1848));
 assert.equal(call('lc_save_expand_capacity',recordAt,27256),1);assert.deepEqual(read(recordAt,27256),now);
 assert.deepEqual(read(recordAt-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(recordAt+27256,16),Buffer.alloc(16,0xa6));
 // Original quick decoder with only descriptor bases/counts changed independently
 // verifies that the bit migration preserves every saved field and adds zeros.
 for(const [at,value] of [[0x80187618,0x802a0000],[0x80187624,256],[0x80187628,0x802b0000],[0x80187634,31],[0x80187638,0x802b1000]])m.w32(at,value);
 m.set('sp',sp);m.set('a0',recordAt+6256);m.set('a1',0x80187618);m.call(0x800742f4,0x80000100,10000000);
 for(let g=0;g<quick.length;g++){const group=quick[g],base=[0x802a0000,0x802b0000,0x802b1000][g],count=[256,31,16][g];
  for(let i=0;i<count;i++)for(const f of group.fields){const expected=i<group.count?getBits(q,group.bit+i*group.each+f.bit,f.bits):0;let actual=0;for(let j=0;j<f.width;j++)actual=(actual*256+m.r8(base+i*group.stride+f.offset+j))>>>0;assert.equal(actual,expected);}}
 migrations++;
}
// Corrupt, unsupported and invalid legacy member IDs refuse before any mutation.
for(const kind of ['checksum','format2','bad-member','null']){
 const data=Buffer.alloc(27256,0x5a);data.fill(0,6256,10044);m.load(recordAt,data);
 call('lc_save_record_seal',recordAt,27256,15,kind==='format2'?2:1);
 if(kind==='checksum')m.w8(recordAt+32,m.r8(recordAt+32)^1);
 if(kind==='bad-member'){const q=read(recordAt+6256,3788);setBits(q,27300+14,8,255);m.load(recordAt+6256,q);call('lc_save_record_seal',recordAt,27256,15,1);}
 const before=read(recordAt,27256);assert.equal(call('lc_save_expand_capacity',kind==='null'?0:recordAt,27256),0);assert.deepEqual(read(recordAt,27256),before);refusals++;
}
// A signed cleared suspend remains cleared; opaque old payload is still retained.
m.load(recordAt,Buffer.alloc(27256,0x73));call('lc_save_record_seal',recordAt,27256,15,1);m.w32(recordAt,0);
assert.equal(call('lc_save_expand_capacity',recordAt,27256),1);assert.equal(m.r32(recordAt),0);assert.equal(call('lc_save_record_valid',recordAt,27256,15,3),1);migrations++;
for(let seed=0;seed<4;seed++){
 const types=Buffer.from({length:256},()=>next()&255),hp=Buffer.from({length:512},()=>next()&255),state=Buffer.from({length:256},()=>next()&255);
 m.load(storage.lc_enemy_types,types);m.load(storage.lc_enemy_soldier_hp,hp);m.load(storage.lc_enemy_soldier_state,state);
 m.load(recordAt-16,Buffer.alloc(664+32,0xa6));call('lc_capacity_tags_save',recordAt);
 m.load(storage.lc_enemy_types,Buffer.alloc(256));m.load(storage.lc_enemy_soldier_hp,Buffer.alloc(512,0xb7));m.load(storage.lc_enemy_soldier_state,Buffer.alloc(256,0xb7));
 call('lc_capacity_tags_load',recordAt);assert.deepEqual(read(storage.lc_enemy_types,256),types);
 for(let id=0;id<256;id++){assert.equal(m.r16(storage.lc_enemy_soldier_hp+id*2),id<100||id>=220?hp.readUInt16BE(id*2):0xb7b7);assert.equal(m.r8(storage.lc_enemy_soldier_state+id),id<100||id>=220?state[id]:0xb7);}
 assert.deepEqual(read(recordAt-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(recordAt+664,16),Buffer.alloc(16,0xa6));tagCases++;
}
// Full155-member fixtures go through the actual name converters and packed
// codec, then the maintained save callback. This catches loss hidden by testing
// compression alone (names, high IDs, unused slots, and split Soldier storage).
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(storage.lc_capacity_quick_descriptor,read(0x80187618,64));
for(const [at,value] of [[0,storage.lc_enemy_characters],[12,256],[16,0x802b0000],[28,31],[32,0x802b1000]])
 m.w32(storage.lc_capacity_quick_descriptor+at,value);
for(let at=0x80187444;m.r32(at);at+=16){
 if(m.r32(at)===0x801953f4)m.w32(at,storage.lc_enemy_soldier_hp+200);
 if(m.r32(at)===0x801954e4)m.w32(at,storage.lc_enemy_soldier_state+100);
}
let fullQuickArmies=0;
// Original quick callbacks, widened only at their explicit array/count operands,
// provide the independent name-format oracle. Synthetic packed empty names are
// not canonical game strings, so compare with retail conversion, not invented
// expectations about its terminator/padding normalization.
const quickChanges=new Map([[0x42f84,0x24053400],[0x42fd4,0x2a020100],
 [0x4300c,0x24040400],[0x43070,0x2a420100],[0x430c0,0x2a420100]]);
for(const [high,low,address] of [[0x42f78,0x42f7c,storage.lc_enemy_characters],
 [0x43034,0x43038,storage.lc_enemy_characters],[0x43094,0x43098,storage.lc_enemy_characters],
 [0x42f94,0x42f98,storage.lc_capacity_quick_descriptor],[0x4307c,0x43080,storage.lc_capacity_quick_descriptor]]){
 quickChanges.set(high,((rom.readUInt32BE(high)&0xffff0000)|((address+0x8000)>>>16))>>>0);
 quickChanges.set(low,((rom.readUInt32BE(low)&0xffff0000)|(address&65535))>>>0);
}
for(const [at,w] of quickChanges)m.w32(at+0x8012a100,w);
let heap=0x80360000;
m.trap(0x80070f30,c=>{const size=c.get('a0');assert(heap+size<0x803e0000);c.set('v0',heap);heap=(heap+size+15)&~15;heap>>>=0;});
m.trap(0x800712c4,()=>{});
m.trap(0x80093380,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80093060,c=>c.load(c.get('a1'),read(c.get('a0'),c.get('a2'))));
for(const mode of ['populated','soldier-heavy']){
 const fixture=fs.readFileSync(path.join(root,'tmp/enemy-capacity-20261012/tagged-neutral',mode+'-1.bin'));
 // Budget fixtures deliberately zero unused rows. A packed empty game name
 // starts with255; zero is a text-control token and is not an empty string.
 for(let id=0;id<256;id++)if(!fixture[15556+id])setBits(fixture,6256*8+id*273,8,255);
 m.load(recordAt,fixture);m.load(storage.lc_enemy_characters,Buffer.alloc(256*52,0xb7));
 m.load(0x802b0000,Buffer.alloc(31*25,0xb7));
 m.load(storage.lc_enemy_soldier_hp,Buffer.alloc(512,0xb7));m.load(storage.lc_enemy_soldier_state,Buffer.alloc(256,0xb7));
 call(0x800742f4,recordAt+42,0x80187444);
 call('lc_capacity_quick_load',recordAt+6256);
 const runtime=read(storage.lc_enemy_characters,256*52),player=read(0x80193bc0,100*56),sources=read(0x802b0000,31*25);
 const used=new Set();for(let i=0;i<31;i++)for(let j=0;j<5;j++){
  const id=sources[i*25+j+2];assert(id>0&&id<255);assert(!used.has(id));used.add(id);
  assert.equal(m.r8(storage.lc_enemy_types+id),mode==='soldier-heavy'&&j>0?2:1);
 }
 assert.equal(used.size,155);
 heap=0x80360000;call(0x8016d068,recordAt+6256);
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),runtime,'original and new quick-load agree');
 m.load(recordAt-16,Buffer.alloc(9964+32,0xa6));m.load(recordAt,Buffer.alloc(9964));
 const beforeOriginal=Buffer.from(m.ram);
 heap=0x80360000;call(0x8016d100,recordAt);
 const originalSaved=read(recordAt,9300);
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),runtime,'original quick-save restores runtime');
 // The retail text parser has global state. Both implementations must start
 // from the same complete state, not the state left by the first conversion.
 m.ram.set(beforeOriginal);
 m.load(recordAt-16,Buffer.alloc(9964+32,0xa6));m.load(recordAt,Buffer.alloc(9964));
 call('lc_capacity_quick_save',recordAt);
 const saved=read(recordAt,9300),differences=[];for(let i=0;i<saved.length;i++)if(saved[i]!==originalSaved[i])differences.push([i,saved[i],originalSaved[i]]);
 assert.equal(differences.length,0,'original and new quick-save agree: '+JSON.stringify(differences.slice(0,24)));
 assert.deepEqual(read(recordAt+9300,664),fixture.subarray(15556,16220));
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),runtime);
 assert.deepEqual(read(0x80193bc0,100*56),player);assert.deepEqual(read(0x802b0000,31*25),sources);
 assert.deepEqual(read(recordAt-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(recordAt+9964,16),Buffer.alloc(16,0xa6));
 fullQuickArmies++;
}
// Differentially execute the exact original mission callbacks and their guarded
// operand adaptations. Preserve load-only resets and opaque auxiliary fields.
const catalog=JSON.parse(fs.readFileSync(path.join(native,'game/scenarios/capacity-save-owners.json')));
const memory={drops:0x80321000,backups:0x80323000},missionAt=0x80330000;
function callbacks(count){
 const c=new FloatMips();c.ram=new Uint8Array(0x400000);c.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
 for(const owner of catalog.owners){
  assert.equal(hash(rom.subarray(owner.start,owner.start+owner.size)),owner.retailSha256);
  const updates=new Map();
  for(const p of owner.patches){assert.equal(rom.readUInt32BE(p.site),p.expected);const value={enemyCount:count-30,mapCount:count,snapshotOffset:((count-30)*2+3&~3)+p.addend,tailOffset:((count-30)*2+3&~3)+count*168+p.addend}[p.kind];updates.set(p.site,((p.expected&0xffff0000)|value)>>>0);}
  for(const p of owner.references){
   assert.equal(rom.readUInt32BE(p.high),p.highWord);assert.equal(rom.readUInt32BE(p.low),p.lowWord);
   const address=(count===50?{drops:0x801936b8,backups:0x801910f0}:memory)[p.group]+p.addend;
   updates.set(p.high,((p.highWord&0xffff0000)|((address+0x8000)>>>16))>>>0);
   updates.set(p.low,((p.lowWord&0xffff0000)|(address&65535))>>>0);
  }
  for(const [site,value] of updates){if(count===50)assert.equal(value,rom.readUInt32BE(site));c.w32(site+0x8012a100,value);}
 }
 return c;
}
function missionCall(c,load){c.set('sp',sp);c.set('a0',missionAt);c.call(load?0x80185108:0x80185964,0x80000100,1000000);}
function bytes(c,at,n){return Buffer.from(c.ram.subarray(c.phys(at),c.phys(at)+n));}
let missionCases=0;
for(let seed=0;seed<3;seed++){
 const old=callbacks(50),expanded=callbacks(61),globals=Buffer.from({length:0x2cb0},()=>next()&255);
 old.load(0x80190f10,globals);expanded.load(0x80190f10,globals);
 const snapshots=Buffer.from({length:61*192},()=>next()&255),drops=Buffer.from({length:62},()=>next()&255);
 old.load(0x801910f0,snapshots.subarray(0,50*192));old.load(0x801936b8,drops.subarray(0,40));
 expanded.load(memory.backups-16,Buffer.alloc(61*192+32,0xa6));expanded.load(memory.backups,snapshots);
 expanded.load(memory.drops-16,Buffer.alloc(62+32,0xa6));expanded.load(memory.drops,drops);
 old.load(missionAt-16,Buffer.alloc(9164+32,0xa6));expanded.load(missionAt-16,Buffer.alloc(11036+32,0xa6));
 missionCall(old,false);missionCall(expanded,false);
 const legacy=bytes(old,missionAt,9164),actual=bytes(expanded,missionAt,11036);
 assert.deepEqual(actual.subarray(0,62),drops);
 assert.deepEqual(actual.subarray(64,64+50*168),legacy.subarray(40,40+50*168));
 assert.deepEqual(actual.subarray(10312),legacy.subarray(8440));
 // Independently run the retail callback with each of the eleven added records in
 // its first slot. This checks all fields and padding without a copied schema.
 for(let i=50;i<61;i++){
  old.load(0x801910f0,snapshots.subarray(i*192,(i+1)*192));missionCall(old,false);
  assert.deepEqual(actual.subarray(64+i*168,64+(i+1)*168),bytes(old,missionAt+40,168));
 }
 assert.deepEqual(bytes(expanded,memory.backups,61*192),snapshots);
 assert.deepEqual(bytes(expanded,memory.drops,62),drops);
 for(const [at,n] of [[missionAt,11036],[memory.backups,61*192],[memory.drops,62]]){
  assert.deepEqual(bytes(expanded,at-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(bytes(expanded,at+n,16),Buffer.alloc(16,0xa6));}
 old.load(missionAt,legacy);old.load(0x801910f0,Buffer.alloc(50*192,0xb7));missionCall(old,true);
 expanded.load(memory.backups,Buffer.alloc(61*192,0xb7));expanded.load(memory.drops,Buffer.alloc(62,0xb7));missionCall(expanded,true);
 assert.deepEqual(bytes(expanded,memory.backups,50*192),bytes(old,0x801910f0,50*192));
 for(let i=50;i<61;i++){
  old.load(missionAt+40,actual.subarray(64+i*168,64+(i+1)*168));old.load(0x801910f0,Buffer.alloc(192,0xb7));missionCall(old,true);
  assert.deepEqual(bytes(expanded,memory.backups+i*192,192),bytes(old,0x801910f0,192));
 }
 assert.deepEqual(bytes(expanded,memory.drops,62),drops);
 assert.deepEqual(bytes(expanded,0x801936e8,0x2dd),bytes(old,0x801936e8,0x2dd));
 assert.equal(expanded.r32(0x80186ff0),0xffffffff);assert.equal(expanded.r32(0x801951a8),61);assert.equal(expanded.r8(0x80193990),1);
 for(let i=0;i<61;i++)for(const off of [0x68,0xa0,0xa4,0xa8])assert.equal(expanded.r32(memory.backups+i*192+off),0);
 missionCases++;
}
// Whole save-menu dispatch: real normal/quick/mission callbacks, real codec and
// native flush. Only allocation and cartridge DMA are simulated. Start from a
// full saved army, serialize, erase its live data, and reload the physical SRAM.
const missionCode=callbacks(61);
for(const owner of catalog.owners)m.load(owner.start+0x8012a100,bytes(missionCode,owner.start+0x8012a100,owner.size));
const table=storage.lc_save_callbacks;
m.load(table,rom.subarray(0x3864c,0x3864c+13*28));
m.w32(table+28+8,entries.lc_save_main_payload);
m.w32(table+28+16,entries.lc_capacity_quick_load);m.w32(table+28+20,entries.lc_capacity_quick_save);
m.w32(table+3*28+24,9964);
for(const [i,v] of [entries.lc_discovery_init,entries.lc_discovery_load,entries.lc_discovery_save,0x1844].entries())m.w32(table+4*28+i*4,v);
// Native flush needs this original cache-control constant as well as its code.
m.load(0x800a8240,rom.subarray(0x38640,0x38648));m.w8(0x800c4800,0);
const cartridge=Buffer.alloc(32768),live=new Map(),cacheAt=0x80360000,bufferAt=0x80370000,
 scratchAt=0x80390000,summaryAt=0x80380000,flushAt=0x80381000,
 scratchBytes=20224+2*(4096+27256);
let failSize=0,writes=0;
m.trap(storage.lc_native_save_alloc,c=>{
 const n=c.get('a0');if(n===failSize){c.set('v0',0);return;}
 let at=n===32768?cacheAt:n===scratchBytes?scratchAt:n===26?summaryAt:n===16||n===400?flushAt:bufferAt;
 assert([32768,scratchBytes,26,16,400,27256,6256].includes(n),'allocation '+n);
 if(n===16||n===400){while([...live].some(([p,size])=>at+n+16>p-16&&p+size+16>at-16))at+=16;assert(at+n<scratchAt-16);}
 assert(!live.has(at));
 for(const [other,size] of live)assert(at+n+16<=other-16||other+size+16<=at-16);
 live.set(at,n);c.load(at-16,Buffer.alloc(n+32,0xa6));c.set('v0',at);
});
m.trap(storage.lc_native_save_free,c=>{
 const at=c.get('a0'),n=live.get(at);assert(n,'free '+at.toString(16));
 assert.deepEqual(read(at-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(at+n,16),Buffer.alloc(16,0xa6));live.delete(at);
});
m.trap(storage.lc_native_sram_io,c=>{
 const at=c.get('a0'),p=c.get('a1'),n=c.get('a2');assert.equal(n,256);assert.equal(at&255,0);assert(at+n<=32768);
 if(c.get('a3')){read(p,n).copy(cartridge,at);writes++;}else c.load(p,cartridge.subarray(at,at+n));
});
const flush=()=>{call(storage.lc_native_save_cleanup,0x37081383);assert.equal(live.size,0);};
function seedCartridge(fixture){
 m.load(recordAt,Buffer.alloc(32768,255));assert.equal(call('lc_save_expand_image',recordAt,32768),1);
 read(recordAt,32768).copy(cartridge);
 for(const slot of [0,1]){m.load(recordAt,fixture.subarray(0,6256));m.w32(recordAt+12,slot+7);call('lc_save_record_seal',recordAt,6256,slot,1);read(recordAt,6256).copy(cartridge,16+slot*6256);}
 m.load(recordAt,Buffer.alloc(16));call('lc_save_record_seal',recordAt,16,14,1);read(recordAt,16).copy(cartridge);
 m.load(recordAt,Buffer.alloc(19208));call('lc_save_record_seal',recordAt,19208,15,1);m.w32(recordAt,0);read(recordAt,19208).copy(cartridge,0x30f0);
 m.w32(storage.lc_save_cache,0);m.w8(storage.lc_save_dirty,0);
}
function loadFixture(fixture){
 m.load(recordAt,fixture);call(0x8016cec4,recordAt+42);call('lc_capacity_quick_load',recordAt+6256);
 call(0x80185108,recordAt+16220);call('lc_discovery_load',recordAt+6224);
 call(0x8016af88,recordAt+16);call(0x80173b40,recordAt+12+0x1843);
}
function eraseArmy(){
 for(const group of main)for(let i=0;i<group.count;i++)for(const f of group.fields)
  for(let j=0;j<f.width;j++)m.w8((group.base===0x801953f4?storage.lc_enemy_soldier_hp+200:group.base===0x801954e4?storage.lc_enemy_soldier_state+100:group.base)+i*group.stride+f.offset+j,0);
 for(const [at,n] of [[storage.lc_enemy_characters,256*52],[0x802b0000,31*25],[0x802b1000,32],
  [storage.lc_enemy_types,256],[storage.lc_enemy_soldier_hp,512],[storage.lc_enemy_soldier_state,256],
  [memory.backups,61*192],[memory.drops,62],[storage.lc_discovery_flags,32]])m.load(at,Buffer.alloc(n));
}
let dispatchArmies=0,dispatchRefusals=0,legacyReads=0;
for(const mode of ['populated','soldier-heavy']){
 const fixture=fs.readFileSync(path.join(root,'tmp/enemy-capacity-20261012/tagged-neutral',mode+'-1.bin'));
 for(let id=0;id<256;id++)if(!fixture[15556+id])setBits(fixture,6256*8+id*273,8,255);
 // The compression stress fixtures include arbitrary reference IDs. Real load
 // callbacks also rebuild inventory counts from equipment and class defaults;
 // give them a complete, consistent dictionary and supported retail classes.
 const setField=(g,i,off,v)=>{const f=g.fields.find(f=>f.offset===off);assert(f);setBits(fixture,42*8+g.bit+i*g.each+f.bit,f.bits,v);};
 const chars=main.find(g=>g.base===0x80193bc0),items=main.find(g=>g.base===0x80196b00),consumables=main.find(g=>g.base===0x80193ac0);
 for(let i=0;i<items.count;i++)setField(items,i,0,i);
 for(let i=0;i<consumables.count;i++)setField(consumables,i,0,i);
 setField(chars,0,0,255);
 for(let i=1;i<100;i++){setField(chars,i,17,i);setField(chars,i,18,1+i%164);for(let off=42;off<=48;off+=2)setField(chars,i,off,1+(i+off)%277);}
 seedCartridge(fixture);
 // Raw legacy summaries/checks must use private migration, never overrun SRAM.
 assert.equal(call('lc_save_check_slot',15),1);assert.equal(call('lc_save_summary',15),0);assert.equal(live.size,0);legacyReads++;
 loadFixture(fixture);
 const playerBefore=read(0x80193bc0,100*56),enemyBefore=read(storage.lc_enemy_characters,256*52),
  playerSquads=read(0x801971f0,30*25),enemySquads=read(0x802b0000,31*25),
  typesBefore=read(storage.lc_enemy_types,256),hpBefore=read(storage.lc_enemy_soldier_hp,512),
  stateBefore=read(storage.lc_enemy_soldier_state,256),backupsBefore=read(memory.backups,61*192),dropsBefore=read(memory.drops,62);
 const normalBefore=Buffer.from(cartridge.subarray(0,0x30f0));
 assert.equal(call('lc_save_suspend_menu'),0xfffd);assert.equal(live.size,0);
 assert.deepEqual(cartridge.subarray(0,0x30f0),normalBefore,'both normal slots and options intact');
 assert.deepEqual(read(0x80193bc0,100*56),playerBefore,'save restores player names');
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),enemyBefore,'save restores enemy names');
 assert.equal(call('lc_save_read_suspend',recordAt),1);const saved=read(recordAt,27256);flush();
 eraseArmy();call('lc_load_game',15);assert.equal(live.size,0);
 // Retail callbacks normalize fields not present in the save; compare every
 // actually saved field by serializing the restored runtime a second time.
 assert.deepEqual(read(0x801971f0,30*25),playerSquads,'player sources '+JSON.stringify([...playerSquads.keys()].filter(i=>playerSquads[i]!==m.r8(0x801971f0+i)).map(i=>[i,playerSquads[i],m.r8(0x801971f0+i)])));assert.deepEqual(read(0x802b0000,31*25),enemySquads);
 assert.deepEqual(read(storage.lc_enemy_types,256),typesBefore);assert.deepEqual(read(storage.lc_enemy_soldier_hp,512),hpBefore);
 assert.deepEqual(read(storage.lc_enemy_soldier_state,256),stateBefore);assert.deepEqual(read(memory.backups,61*192),backupsBefore);assert.deepEqual(read(memory.drops,62),dropsBefore);
 assert.equal(call('lc_save_suspend_menu'),0xfffd);assert.equal(call('lc_save_read_suspend',recordAt),1);
 assert.deepEqual(read(recordAt+12,27256-12),saved.subarray(12),'every saved field round trips through dispatcher');flush();
 const beforeFailure=Buffer.from(cartridge);failSize=scratchBytes;
 assert.equal(call('lc_save_suspend_menu'),0xfffe);assert.deepEqual(cartridge,beforeFailure);assert.equal(live.size,0);dispatchRefusals++;failSize=0;
 // Normal writes after a suspend retain its entire compressed frame.
 const suspendBefore=Buffer.from(cartridge.subarray(0x30f0));
 assert.equal(call('lc_save_game',0),1);assert.equal(call('lc_save_game',1),1);
 assert.deepEqual(cartridge.subarray(0x30f0),suspendBefore);assert.equal(live.size,0);
 dispatchArmies++;
}
// Old populated suspends must migrate on read in either physical layout. These
// use the same independently checked legacy fixture as the bit-migration oracle.
for(const compressed of [false,true]){
 seedCartridge(Buffer.alloc(27256));legacyFixture.copy(cartridge,0x30f0);
 if(compressed){
  m.load(recordAt,cartridge);m.load(bufferAt,legacyFixture);
  assert.equal(call('lc_save_commit_compressed',recordAt,32768,bufferAt,19208,scratchAt,scratchBytes),1);
  read(recordAt,32768).copy(cartridge);
 }
 const before=Buffer.from(cartridge),count=writes;
 assert.equal(call('lc_save_read_suspend',recordAt),1);assert.deepEqual(read(recordAt,27256),legacyExpanded);flush();
 assert.deepEqual(cartridge,before);assert.equal(writes,count);legacyReads++;
 if(compressed){
  cartridge[0x30f0+40]^=1;const damaged=Buffer.from(cartridge);
  assert.equal(call('lc_save_check_slot',15),0);call('lc_save_initialize_slot',15);flush();
  assert.deepEqual(cartridge,damaged);assert.equal(writes,count);dispatchRefusals++;
 }
}
const report={migrations,refusals,tagCases,fullQuickArmies,missionCases,dispatchArmies,dispatchRefusals,legacyReads,writes,originalDecoder:'all quick descriptor fields compared',work,
 scope:'Compiled N64 save dispatcher, real game callbacks and native flush with simulated SRAM; no installed ROM/gameplay claim.'};
fs.writeFileSync(path.join(work,'report.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
