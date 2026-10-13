'use strict';
// Save/load through the public export's actual code, descriptors and relocated
// storage. Only allocation and cartridge I/O are substituted with guarded RAM
// and a 32 KiB byte array. This is not a console/emulator session.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),cp=require('node:child_process');
const {FloatMips}=require('./float-mips.cjs');
const native=path.resolve(__dirname,'..'),root=path.resolve(native,'../..');
const work=path.resolve(process.argv[2]||path.join(root,'tmp/enemy-capacity-20261012/public-export'));
const fixtureInfo=JSON.parse(fs.readFileSync(process.argv[3]||path.join(work,'capacity-export.json')));
const fixtures=path.join(work,'full-army-inputs');
if(!fs.existsSync(path.join(fixtures,'populated-1.bin')))
 cp.execFileSync(process.execPath,[path.join(root,'tools/enemy_capacity_full_armies.cjs'),fixtures,
  '--tagged-handles','--enemy-squads','30','--player-deployed','20','--retained-player-history','--neutral-slot'],{stdio:'pipe'});
const rom=fs.readFileSync(fixtureInfo.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const entries={};
for(const file of ['save_runtime','save_compression','capacity_migration','capacity_core']) {
 const elf=fs.readFileSync(path.join(work,file+'.elf')),sh=elf.readUInt32BE(32);
 for(let i=0;i<elf.readUInt16BE(48);i++){
  const at=sh+i*40;if(elf.readUInt32BE(at+4)!==2)continue;
  const begin=elf.readUInt32BE(at+16),end=begin+elf.readUInt32BE(at+20),strings=elf.readUInt32BE(sh+elf.readUInt32BE(at+24)*40+16);
  for(let p=begin;p<end;p+=16)if((elf[p+12]&15)===2){const n=strings+elf.readUInt32BE(p);entries[elf.toString('ascii',n,elf.indexOf(0,n))]=elf.readUInt32BE(p+4);}
 }
}
const base=fixtureInfo.base,sources=base+0x3800,enemySources=sources+750;
const storage={lc_save_cache:0x800a83b8,lc_save_dirty:0x800a83bc,lc_discovery_flags:rom.readUInt32BE(0x5dc80+44),
 lc_native_save_alloc:0x80070f30,lc_native_save_free:0x800712c4,lc_native_sram_io:0x8008a0f0,
 lc_native_save_cleanup:0x80074bf0,lc_enemy_characters:base,lc_enemy_types:base+0x3400,
 lc_enemy_soldier_hp:base+0x3500,lc_enemy_soldier_state:base+0x3700};
const memory={drops:base+0x6e80,backups:base+0x6f00};
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const dataAt=rom.readUInt32BE(0x5dc8c),dataBytes=rom.readUInt32BE(0x5dc90),dataAddress=rom.readUInt32BE(0x5dc98);
assert(dataAt>=0x2800000&&dataAddress+dataBytes<=0x80400000);
m.load(dataAddress,rom.subarray(dataAt,dataAt+dataBytes));
m.load(0x800a824c,rom.subarray(0x3864c,0x3864c+13*28));m.load(0x800a8240,rom.subarray(0x38640,0x38648));m.w8(0x800c4800,0);
const recordAt=0x80300000,sp=0x803f0000,read=(at,n)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+n));
function call(name,...args){m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+4*(i-4),v));m.call(entries[name]||name,0x80000100,200000000);assert.equal(m.get('sp'),sp);return m.get('v0');}
const heapCalls=[],poolCalls=[],dmas=[],invalidations=[];
m.trap(0x80070c60,c=>heapCalls.push([c.get('a0'),c.get('a1')]));
m.trap(0x80070d20,c=>poolCalls.push([c.get('a0'),c.get('a1')]));
m.trap(0x80070f14,()=>{});
m.trap(0x8009da50,c=>{const at=c.get('a0'),to=c.get('a1'),n=c.get('a2');dmas.push([at,to,n]);c.load(to,rom.subarray(at,at+n));});
m.trap(0x800900c0,c=>invalidations.push([c.get('a0'),c.get('a1')]));
const descriptorAt=0x5dc80,heapEnd=rom.readUInt32BE(descriptorAt+48),
 reclaimStart=rom.readUInt32BE(descriptorAt+108)?rom.readUInt32BE(descriptorAt+104)+rom.readUInt32BE(descriptorAt+108)
 :0x80187f00+((rom.readUInt32BE(descriptorAt+84)+15)&~15);
const reclaimed=[];
if(reclaimStart<0x8018aa70)reclaimed.push([reclaimStart,0x8018aa70-reclaimStart]);
reclaimed.push([0x8018ece0,0x3d0],[0x8018fec0,0x4b0]);
for(const [entry,start] of [[60,0x80243db0],[64,0x801ba710],[64,0x80243db0]]){
 heapCalls.length=poolCalls.length=0;call(rom.readUInt32BE(descriptorAt+entry),start,0x80400000-start);
 assert.deepEqual(heapCalls,[[heapEnd,0x80400000-heapEnd]]);
 assert.deepEqual(poolCalls,[...reclaimed,...(start<0x80243db0?[[start,0x80243db0-start]]:[])]);
}
heapCalls.length=poolCalls.length=0;call(rom.readUInt32BE(descriptorAt+68));
assert.deepEqual(poolCalls,reclaimed);assert.equal(heapCalls.length,0);
assert.deepEqual(dmas,[[dataAt,dataAddress,dataBytes]]);assert.deepEqual(invalidations,[[dataAddress,dataBytes]]);
let activationChecks=0;
for(const count of [0,14,15,25,26,29,30,31])for(const mode of ['queued','route','single','group']){
 const t=new FloatMips(),unit=0x80300000;
 t.load(0x801cf0b0,rom.subarray(0x1237f0,0x123e38));t.set('sp',0x803f0000);
 t.w8(0x801f0fde,count);t.w32(unit,mode==='group'?0x51:0x11);t.w8(unit+0xba,mode==='queued'?20:4);
 t.w8(0x801f0ebf,mode==='single'||mode==='group'?2:1);t.w8(0x801f0ecf,7);
 t.w8(0x801f0ec8,mode==='single'||mode==='group'?1:0);
 t.trap(0x801ced18,c=>c.set('v0',1));t.trap(0x801da1ac,c=>c.set('v0',5));
 t.set('a0',unit);t.call(0x801cf0b0,0x80000100);
 assert.equal(t.get('v0'),count+(mode==='group'?5:1)<=30?(mode==='queued'?1:7):0,`activation ${mode}/${count}`);
 assert.equal(t.get('sp'),0x803f0000);activationChecks++;
}
// Original text initialization clears the pending glyph to -1. Zero-filled BSS
// alone injects a spurious {T01} into the first encoded nonempty name.
call(0x80179354);
m.trap(0x80093380,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80093060,c=>c.load(c.get('a1'),read(c.get('a0'),c.get('a2'))));
function descriptor(at){let bit=0;const groups=[];for(;m.r32(at);at+=16){let each=0;const fields=[];for(let p=m.r32(at+8);m.r8(p+1)&127;p+=3){fields.push({offset:m.r8(p),width:m.r8(p+1)&127,bits:m.r8(p+2),bit:each});each+=m.r8(p+2);}groups.push({base:m.r32(at),stride:m.r32(at+4),count:m.r32(at+12),bit,each,fields});bit+=each*m.r32(at+12);}return groups;}
const main=descriptor(0x80187444),quick=descriptor(base+0xacc0);
assert.deepEqual(quick.map(g=>[g.count,g.each,g.bit]),[[256,273,0],[31,137,69888],[16,16,74135]]);
assert.equal(main.find(g=>g.base===sources).count,30);
assert.equal(main.find(g=>g.base===storage.lc_enemy_soldier_hp+200).count,120);
function setBits(b,at,n,value){for(let j=0;j<n;j++){const bit=at+j,mask=1<<(7-(bit&7));b[bit>>3]=(b[bit>>3]&~mask)|(((value>>>(n-j-1))&1)?mask:0);}}
const cartridge=Buffer.alloc(32768),live=new Map(),cacheAt=0x80360000,bufferAt=0x80370000,
 scratchAt=0x80390000,summaryAt=0x80380000,flushAt=0x80381000,
 scratchBytes=20224+27256+2*(4096+27256);
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
 for(const [at,n] of [[storage.lc_enemy_characters,256*52],[enemySources,31*25],[quick[2].base,32],
  [storage.lc_enemy_types,256],[storage.lc_enemy_soldier_hp,512],[storage.lc_enemy_soldier_state,256],
  [memory.backups,61*192],[memory.drops,62],[storage.lc_discovery_flags,32]])m.load(at,Buffer.alloc(n));
}
let dispatchArmies=0,dispatchRefusals=0,legacyReads=0;
for(const mode of ['populated','soldier-heavy']){
 const fixture=fs.readFileSync(path.join(fixtures,mode+'-1.bin'));
 for(let id=0;id<256;id++)if(!fixture[15556+id])setBits(fixture,6256*8+id*273,8,255);
 // The compression stress fixtures include arbitrary reference IDs. Real load
 // callbacks also rebuild inventory counts from equipment and class defaults;
 // give them a complete, consistent dictionary and supported retail classes.
 const setField=(g,i,off,v)=>{const f=g.fields.find(f=>f.offset===off);assert(f);setBits(fixture,42*8+g.bit+i*g.each+f.bit,f.bits,v);};
 const chars=main.find(g=>g.base===0x80193bc0),items=main.find(g=>g.base===0x80196b00),consumables=main.find(g=>g.base===0x80193ac0);
 for(let i=0;i<items.count;i++)setField(items,i,0,i);
 for(let i=0;i<consumables.count;i++)setField(consumables,i,0,i);
 setField(chars,0,0,255);
 for(let i=1;i<100;i++){setField(chars,i,17,[5,8,9][i%3]);setField(chars,i,18,[5,8,9][i%3]);for(let off=42;off<=48;off+=2)setField(chars,i,off,1+(i+off)%277);}
 seedCartridge(fixture);
 // Raw legacy summaries/checks must use private migration, never overrun SRAM.
 assert.equal(call('lc_save_check_slot',15),1);assert.equal(call('lc_save_summary',15),0);assert.equal(live.size,0);legacyReads++;
 loadFixture(fixture);
 const playerBefore=read(0x80193bc0,100*56),enemyBefore=read(storage.lc_enemy_characters,256*52),
  playerSquads=read(sources,30*25),enemySquads=read(enemySources,31*25),
  typesBefore=read(storage.lc_enemy_types,256),hpBefore=read(storage.lc_enemy_soldier_hp,512),
  stateBefore=read(storage.lc_enemy_soldier_state,256),backupsBefore=read(memory.backups,61*192),dropsBefore=read(memory.drops,62);
 const normalBefore=Buffer.from(cartridge.subarray(0,0x30f0));
 assert.equal(call('lc_save_suspend_menu'),0xfffd);assert.equal(live.size,0);
 assert.deepEqual(cartridge.subarray(0,0x30f0),normalBefore,'both normal slots and options intact');
 assert.deepEqual(read(0x80193bc0,100*56),playerBefore,'save restores player names');
 assert.deepEqual(read(storage.lc_enemy_characters,256*52),enemyBefore,'save restores enemy names');
 assert.equal(call('lc_save_read_suspend',recordAt),1);const saved=read(recordAt,27256);flush();
 eraseArmy();
 call('lc_load_game',15);assert.equal(live.size,0);
 // Retail callbacks normalize fields not present in the save; compare every
 // actually saved field by serializing the restored runtime a second time.
 assert.deepEqual(read(sources,30*25),playerSquads,'player sources '+JSON.stringify([...playerSquads.keys()].filter(i=>playerSquads[i]!==m.r8(sources+i)).map(i=>[i,playerSquads[i],m.r8(sources+i)])));assert.deepEqual(read(enemySources,31*25),enemySquads);
 assert.deepEqual(read(storage.lc_enemy_types,256),typesBefore);assert.deepEqual(read(storage.lc_enemy_soldier_hp,512),hpBefore);
 assert.deepEqual(read(storage.lc_enemy_soldier_state,256),stateBefore);assert.deepEqual(read(memory.backups,61*192),backupsBefore);assert.deepEqual(read(memory.drops,62),dropsBefore);
 assert.equal(call('lc_save_suspend_menu'),0xfffd);assert.equal(call('lc_save_read_suspend',recordAt),1);
 const again=read(recordAt,27256);
 if(!again.subarray(12).equals(saved.subarray(12))){
  const differences=[];for(let i=12;i<27256;i++)if(again[i]!==saved[i])differences.push(i);
  const fields=[];for(const g of main)for(let i=0;i<g.count;i++)for(const f of g.fields){
   const start=42*8+g.bit+i*g.each+f.bit,end=start+f.bits;
   if(differences.some(byte=>byte*8<end&&byte*8+8>start))fields.push({address:(g.base+i*g.stride+f.offset).toString(16),record:i,offset:f.offset});
  }
  console.error(JSON.stringify({differences,fields}));
  fs.writeFileSync(path.join(work,'first-save.bin'),saved);fs.writeFileSync(path.join(work,'roundtrip-save.bin'),again);
  fs.writeFileSync(path.join(work,'player-before.bin'),playerBefore);fs.writeFileSync(path.join(work,'player-after.bin'),read(0x80193bc0,5600));
 }
 assert.deepEqual(read(recordAt+12,27256-12),saved.subarray(12),'every saved field round trips through dispatcher');flush();
 const beforeFailure=Buffer.from(cartridge);failSize=scratchBytes;
 assert.equal(call('lc_save_suspend_menu'),0xfffe);assert.deepEqual(cartridge,beforeFailure);assert.equal(live.size,0);dispatchRefusals++;failSize=0;
 // Normal writes after a suspend retain its entire compressed frame.
 const suspendBefore=Buffer.from(cartridge.subarray(0x30f0));
 assert.equal(call('lc_save_game',0),1);assert.equal(call('lc_save_game',1),1);
 assert.deepEqual(cartridge.subarray(0x30f0),suspendBefore);assert.equal(live.size,0);
 fs.writeFileSync(path.join(work,mode+'-full-army.sra'),cartridge);
 dispatchArmies++;
}
// Exercise the separately linked migration on nonempty legacy images. Compare
// all preserved bits and opaque mission bytes, including unused old records.
const legacy=Buffer.alloc(19208);for(let i=12;i<legacy.length;i++)legacy[i]=(i*37+(i>>>4))&255;
const oldQuick=legacy.subarray(6256);oldQuick.fill(0,0,3787);
for(let id=1;id<100;id++)setBits(oldQuick,id*273+112,8,5);
for(let i=0;i<20;i++)for(let j=0;j<5;j++)setBits(oldQuick,27300+i*137+14+j*8,8,i*5+j+1);
m.load(recordAt,legacy);call('lc_save_record_seal',recordAt,19208,15,1);read(recordAt,19208).copy(legacy);
const getBit=(b,n)=>(b[n>>>3]>>>(7-(n&7)))&1;
for(const compressed of [false,true]){
 seedCartridge(Buffer.alloc(27256));legacy.copy(cartridge,0x30f0);
 if(compressed){m.load(recordAt,cartridge);m.load(bufferAt,legacy);
  assert.equal(call('lc_save_commit_compressed',recordAt,32768,bufferAt,19208,scratchAt,scratchBytes),1);
  read(recordAt,32768).copy(cartridge);}
 const before=Buffer.from(cartridge),oldWrites=writes;
 assert.equal(call('lc_save_read_suspend',recordAt),1);const migrated=read(recordAt,27256);
 assert.deepEqual(migrated.subarray(12,6256),legacy.subarray(12,6256));
 for(const [to,from,n] of [[0,0,27300],[69888,27300,2740],[74135,30040,256]])
  for(let i=0;i<n;i++)assert.equal(getBit(migrated,6256*8+to+i),getBit(legacy,6256*8+from+i));
 for(const [to,from,n] of [[16220,10044,40],[16284,10084,8400],[26532,18484,724]])
  assert.deepEqual(migrated.subarray(to,to+n),legacy.subarray(from,from+n));
 flush();assert.deepEqual(cartridge,before);assert.equal(writes,oldWrites);legacyReads++;
}
const report={loaderChecks:4,activationChecks,dispatchArmies,dispatchRefusals,legacyReads,writes,sourceSlots:61,physicalBytes:32768,
 scope:'Actual public-export code and native callbacks in private MIPS RAM with guarded allocation and simulated SRAM; no emulator session.'};
fs.writeFileSync(path.join(work,'save-load-report.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report));
