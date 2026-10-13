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
for(const [folder,names] of Object.entries({classes:['save-format.c','save_format.h','save_checksums.h','save_compression.h'],
 scenarios:['capacity-save.c','capacity-save.h','capacity-tags.c','capacity-quick-save.c','enemy-capacity.h']}))
 for(const name of names)fs.copyFileSync(path.join(native,'game',folder,name),path.join(work,name));
fs.writeFileSync(path.join(work,'test.c'),['save-format.c','capacity-save.c','capacity-tags.c','capacity-quick-save.c'].map(n=>fs.readFileSync(path.join(work,n),'utf8')).join('\n'));
const run=(tool,args)=>cp.execFileSync(path.join(tools,tool),args,{cwd:work,encoding:'utf8'});
run('cpp.exe',['-P','-undef','-nostdinc','test.c','test.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','test.i','-o','test.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','test.o','test.s']);
const storage={lc_enemy_types:0x80320000,lc_enemy_soldier_hp:0x80320200,lc_enemy_soldier_state:0x80320500,
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
const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(0x80280000,fs.readFileSync(path.join(work,'test.bin')));
const recordAt=0x80300000,sp=0x803f0000,read=(at,n)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+n));
function call(name,...args){m.set('sp',sp);args.forEach((v,i)=>m.set('a'+i,v));m.call(entries[name]||name,0x80000100,20000000);assert.equal(m.get('sp'),sp);return m.get('v0');}
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
let migrations=0,refusals=0,tagCases=0;
for(let seed=0;seed<6;seed++){
 const old=Buffer.from({length:19208},()=>next()&255),q=old.subarray(6256),types=Buffer.alloc(256);
 for(let id=1;id<100;id++){const cls=seed&1?(id%3?4:0):0;setBits(q,id*273+112,8,cls);if(cls)types[id]=1;}
 for(let id=100;id<220;id++){const hp=seed&2?(id%4?437:0):0;setBits(old,42*8+41426+(id-100)*10,10,hp);if(hp)types[id]=2;}
 for(let i=0;i<20;i++)for(let j=0;j<5;j++){const id=(i*5+j+seed*20)%220;setBits(q,27300+i*137+14+j*8,8,id);if(id)types[id]=id<100?1:2;}
 m.load(recordAt-16,Buffer.alloc(27068+32,0xa6));m.load(recordAt,old);
 assert.equal(call('lc_save_record_seal',recordAt,19208,15,1),1);
 const before=read(recordAt,27068),signed=before.subarray(0,19208);
 for(const capacity of [0,19208,27067]){assert.equal(call('lc_save_expand_capacity',recordAt,capacity),0);assert.deepEqual(read(recordAt,27068),before);refusals++;}
 assert.equal(call('lc_save_expand_capacity',recordAt,27068),1);const now=read(recordAt,27068),nq=now.subarray(6256);
 assert.equal(call('lc_save_record_valid',recordAt,27068,15,3),1);
 assert.deepEqual(now.subarray(12,6256),signed.subarray(12,6256));
 for(const [a,b,bits] of [[0,0,27300],[69888,27300,2740],[73998,30040,256]])
  for(let i=0;i<bits;i++)assert.equal(getBits(nq,a+i,1),getBits(q,b+i,1));
 for(const [a,b] of [[27300,69888],[72628,73998]])for(let i=a;i<b;i++)assert.equal(getBits(nq,i,1),0);
 assert.equal(nq[9283],q[3787]);assert.equal(nq[9282],0);assert.equal(nq[9281]&3,0);
 assert.deepEqual(now.subarray(15540,15796),types);assert.deepEqual(now.subarray(15796,16204),Buffer.alloc(408));
 for(const [to,from,n] of [[16204,10044,40],[16264,10084,8400],[26344,18484,724]])assert.deepEqual(now.subarray(to,to+n),signed.subarray(from,from+n));
 assert.deepEqual(now.subarray(16244,16264),Buffer.alloc(20));assert.deepEqual(now.subarray(24664,26344),Buffer.alloc(1680));
 assert.equal(call('lc_save_expand_capacity',recordAt,27068),1);assert.deepEqual(read(recordAt,27068),now);
 assert.deepEqual(read(recordAt-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(recordAt+27068,16),Buffer.alloc(16,0xa6));
 // Original quick decoder with only descriptor bases/counts changed independently
 // verifies that the bit migration preserves every saved field and adds zeros.
 for(const [at,value] of [[0x80187618,0x802a0000],[0x80187624,256],[0x80187628,0x802b0000],[0x80187634,30],[0x80187638,0x802b1000]])m.w32(at,value);
 m.set('sp',sp);m.set('a0',recordAt+6256);m.set('a1',0x80187618);m.call(0x800742f4,0x80000100,10000000);
 for(let g=0;g<quick.length;g++){const group=quick[g],base=[0x802a0000,0x802b0000,0x802b1000][g],count=[256,30,16][g];
  for(let i=0;i<count;i++)for(const f of group.fields){const expected=i<group.count?getBits(q,group.bit+i*group.each+f.bit,f.bits):0;let actual=0;for(let j=0;j<f.width;j++)actual=(actual*256+m.r8(base+i*group.stride+f.offset+j))>>>0;assert.equal(actual,expected);}}
 migrations++;
}
// Corrupt, unsupported and invalid legacy member IDs refuse before any mutation.
for(const kind of ['checksum','format2','bad-member','null']){
 const data=Buffer.alloc(27068,0x5a);data.fill(0,6256,10044);m.load(recordAt,data);
 call('lc_save_record_seal',recordAt,27068,15,kind==='format2'?2:1);
 if(kind==='checksum')m.w8(recordAt+32,m.r8(recordAt+32)^1);
 if(kind==='bad-member'){const q=read(recordAt+6256,3788);setBits(q,27300+14,8,255);m.load(recordAt+6256,q);call('lc_save_record_seal',recordAt,27068,15,1);}
 const before=read(recordAt,27068);assert.equal(call('lc_save_expand_capacity',kind==='null'?0:recordAt,27068),0);assert.deepEqual(read(recordAt,27068),before);refusals++;
}
// A signed cleared suspend remains cleared; opaque old payload is still retained.
m.load(recordAt,Buffer.alloc(27068,0x73));call('lc_save_record_seal',recordAt,27068,15,1);m.w32(recordAt,0);
assert.equal(call('lc_save_expand_capacity',recordAt,27068),1);assert.equal(m.r32(recordAt),0);assert.equal(call('lc_save_record_valid',recordAt,27068,15,3),1);migrations++;
for(let seed=0;seed<4;seed++){
 const types=Buffer.from({length:256},()=>next()&255),hp=Buffer.from({length:512},()=>next()&255),state=Buffer.from({length:256},()=>next()&255);
 m.load(storage.lc_enemy_types,types);m.load(storage.lc_enemy_soldier_hp,hp);m.load(storage.lc_enemy_soldier_state,state);
 m.load(recordAt-16,Buffer.alloc(664+32,0xa6));call('lc_capacity_tags_save',recordAt);
 m.load(storage.lc_enemy_types,Buffer.alloc(256));m.load(storage.lc_enemy_soldier_hp,Buffer.alloc(512,0xb7));m.load(storage.lc_enemy_soldier_state,Buffer.alloc(256,0xb7));
 call('lc_capacity_tags_load',recordAt);assert.deepEqual(read(storage.lc_enemy_types,256),types);
 for(let id=0;id<256;id++){assert.equal(m.r16(storage.lc_enemy_soldier_hp+id*2),id<100||id>=220?hp.readUInt16BE(id*2):0xb7b7);assert.equal(m.r8(storage.lc_enemy_soldier_state+id),id<100||id>=220?state[id]:0xb7);}
 assert.deepEqual(read(recordAt-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(recordAt+664,16),Buffer.alloc(16,0xa6));tagCases++;
}
// Full150-member fixtures go through the actual name converters and packed
// codec, then the maintained save callback. This catches loss hidden by testing
// compression alone (names, high IDs, unused slots, and split Soldier storage).
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(storage.lc_capacity_quick_descriptor,read(0x80187618,64));
for(const [at,value] of [[0,storage.lc_enemy_characters],[12,256],[16,0x802b0000],[28,30],[32,0x802b1000]])
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
 const fixture=fs.readFileSync(path.join(root,'tmp/enemy-capacity-20261012/tagged-armies',mode+'-1.bin'));
 // Budget fixtures deliberately zero unused rows. A packed empty game name
 // starts with255; zero is a text-control token and is not an empty string.
 for(let id=0;id<256;id++)if(!fixture[15540+id])setBits(fixture,6256*8+id*273,8,255);
 m.load(recordAt,fixture);m.load(storage.lc_enemy_characters,Buffer.alloc(256*52,0xb7));
 m.load(0x802b0000,Buffer.alloc(30*25,0xb7));
 m.load(storage.lc_enemy_soldier_hp,Buffer.alloc(512,0xb7));m.load(storage.lc_enemy_soldier_state,Buffer.alloc(256,0xb7));
 call(0x800742f4,recordAt+42,0x80187444);
 call('lc_capacity_quick_load',recordAt+6256);
 const runtime=read(storage.lc_enemy_characters,256*52),player=read(0x80193bc0,100*56),sources=read(0x802b0000,30*25);
 const used=new Set();for(let i=0;i<30;i++)for(let j=0;j<5;j++){
  const id=sources[i*25+j+2];assert(id>0&&id<255);assert(!used.has(id));used.add(id);
  assert.equal(m.r8(storage.lc_enemy_types+id),mode==='soldier-heavy'&&j>0?2:1);
 }
 assert.equal(used.size,150);
 heap=0x80360000;call(0x8016d068,recordAt+6256);
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),runtime,'original and new quick-load agree');
 m.load(recordAt-16,Buffer.alloc(9948+32,0xa6));m.load(recordAt,Buffer.alloc(9948));
 const beforeOriginal=Buffer.from(m.ram);
 heap=0x80360000;call(0x8016d100,recordAt);
 const originalSaved=read(recordAt,9284);
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),runtime,'original quick-save restores runtime');
 // The retail text parser has global state. Both implementations must start
 // from the same complete state, not the state left by the first conversion.
 m.ram.set(beforeOriginal);
 m.load(recordAt-16,Buffer.alloc(9948+32,0xa6));m.load(recordAt,Buffer.alloc(9948));
 call('lc_capacity_quick_save',recordAt);
 const saved=read(recordAt,9284),differences=[];for(let i=0;i<saved.length;i++)if(saved[i]!==originalSaved[i])differences.push([i,saved[i],originalSaved[i]]);
 assert.equal(differences.length,0,'original and new quick-save agree: '+JSON.stringify(differences.slice(0,24)));
 assert.deepEqual(read(recordAt+9284,664),fixture.subarray(15540,16204));
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),runtime);
 assert.deepEqual(read(0x80193bc0,100*56),player);assert.deepEqual(read(0x802b0000,30*25),sources);
 assert.deepEqual(read(recordAt-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(recordAt+9948,16),Buffer.alloc(16,0xa6));
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
  for(const p of owner.patches){assert.equal(rom.readUInt32BE(p.site),p.expected);const value={enemyCount:count-30,mapCount:count,snapshotOffset:(count-30)*2+p.addend,tailOffset:(count-30)*2+count*168+p.addend}[p.kind];updates.set(p.site,((p.expected&0xffff0000)|value)>>>0);}
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
 const old=callbacks(50),expanded=callbacks(60),globals=Buffer.from({length:0x2cb0},()=>next()&255);
 old.load(0x80190f10,globals);expanded.load(0x80190f10,globals);
 const snapshots=Buffer.from({length:60*192},()=>next()&255),drops=Buffer.from({length:60},()=>next()&255);
 old.load(0x801910f0,snapshots.subarray(0,50*192));old.load(0x801936b8,drops.subarray(0,40));
 expanded.load(memory.backups-16,Buffer.alloc(60*192+32,0xa6));expanded.load(memory.backups,snapshots);
 expanded.load(memory.drops-16,Buffer.alloc(60+32,0xa6));expanded.load(memory.drops,drops);
 old.load(missionAt-16,Buffer.alloc(9164+32,0xa6));expanded.load(missionAt-16,Buffer.alloc(10864+32,0xa6));
 missionCall(old,false);missionCall(expanded,false);
 const legacy=bytes(old,missionAt,9164),actual=bytes(expanded,missionAt,10864);
 assert.deepEqual(actual.subarray(0,60),drops);
 assert.deepEqual(actual.subarray(60,60+50*168),legacy.subarray(40,40+50*168));
 assert.deepEqual(actual.subarray(10140),legacy.subarray(8440));
 // Independently run the retail callback with each of the ten added records in
 // its first slot. This checks all fields and padding without a copied schema.
 for(let i=50;i<60;i++){
  old.load(0x801910f0,snapshots.subarray(i*192,(i+1)*192));missionCall(old,false);
  assert.deepEqual(actual.subarray(60+i*168,60+(i+1)*168),bytes(old,missionAt+40,168));
 }
 assert.deepEqual(bytes(expanded,memory.backups,60*192),snapshots);
 assert.deepEqual(bytes(expanded,memory.drops,60),drops);
 for(const [at,n] of [[missionAt,10864],[memory.backups,60*192],[memory.drops,60]]){
  assert.deepEqual(bytes(expanded,at-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(bytes(expanded,at+n,16),Buffer.alloc(16,0xa6));}
 old.load(missionAt,legacy);old.load(0x801910f0,Buffer.alloc(50*192,0xb7));missionCall(old,true);
 expanded.load(memory.backups,Buffer.alloc(60*192,0xb7));expanded.load(memory.drops,Buffer.alloc(60,0xb7));missionCall(expanded,true);
 assert.deepEqual(bytes(expanded,memory.backups,50*192),bytes(old,0x801910f0,50*192));
 for(let i=50;i<60;i++){
  old.load(missionAt+40,actual.subarray(60+i*168,60+(i+1)*168));old.load(0x801910f0,Buffer.alloc(192,0xb7));missionCall(old,true);
  assert.deepEqual(bytes(expanded,memory.backups+i*192,192),bytes(old,0x801910f0,192));
 }
 assert.deepEqual(bytes(expanded,memory.drops,60),drops);
 assert.deepEqual(bytes(expanded,0x801936e8,0x2dd),bytes(old,0x801936e8,0x2dd));
 assert.equal(expanded.r32(0x80186ff0),0xffffffff);assert.equal(expanded.r32(0x801951a8),60);assert.equal(expanded.r8(0x80193990),1);
 for(let i=0;i<60;i++)for(const off of [0x68,0xa0,0xa4,0xa8])assert.equal(expanded.r32(memory.backups+i*192+off),0);
 missionCases++;
}
const report={migrations,refusals,tagCases,fullQuickArmies,missionCases,originalDecoder:'all quick descriptor fields compared',work,
 scope:'Compiled N64 migration/type callbacks and differential original/expanded mission callbacks; no installed game or SRAM dispatch claim.'};
fs.writeFileSync(path.join(work,'report.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
