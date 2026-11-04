'use strict';
// Compile the portable save codec for N64 and execute it in isolated RAM.
// Original game decoder is an independent oracle for generated token streams.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process');
const crypto=require('node:crypto'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const native=path.resolve(__dirname,'..'),root=path.resolve(native,'../..');
const toolchain=process.env.LORDLY_N64_TOOLCHAIN||path.join(native,'.local/n64-toolchain');
const pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json')));
for(const [name,hash] of Object.entries(pins.tools))
 assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(toolchain,name))).digest('hex'),hash);
const work=fs.mkdtempSync(path.join(native,'build/save-custom-flags-'));
for(const name of ['save-compression.c','save_compression.h','save-format.c','save_format.h','save_checksums.h','save-image.c','save-runtime.c'])
 fs.copyFileSync(path.join(native,'game/classes',name),path.join(work,name));
fs.copyFileSync(path.join(native,'game/scenarios/capacity-save.h'),path.join(work,'capacity-save.h'));
fs.appendFileSync(path.join(work,'save-compression.c'),'\n'+fs.readFileSync(path.join(work,'save-format.c'),'utf8')+'\n'+fs.readFileSync(path.join(work,'save-image.c'),'utf8')+'\n#define LC_CUSTOM_STORY_FLAGS 1\n#define LC_SAVE_RUNTIME_FORMAT 4\n'+fs.readFileSync(path.join(work,'save-runtime.c'),'utf8')+'\n'+fs.readFileSync(path.join(native,'game/scenarios/capacity-save.c'),'utf8'));
const run=(name,args)=>cp.execFileSync(path.join(toolchain,name),args,{cwd:work,encoding:'utf8'});
run('cpp.exe',['-P','-undef','-nostdinc','save-compression.c','codec.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','codec.i','-o','codec.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','codec.o','codec.s']);
fs.writeFileSync(path.join(work,'codec.ld'),'SECTIONS { .text 0x80280000 : { *(.text) } }\n' + Object.entries({lc_save_cache:0x8027f000,lc_save_dirty:0x8027f004,lc_discovery_flags:0x8027f010,lc_legacy_discovery:0x8027f040,lc_custom_story_flags:0x8027f080,lc_save_callbacks:0x8027f100,lc_native_save_alloc:0x80001000,lc_native_sram_io:0x80001010,lc_native_main_save:0x80001020,lc_native_save_free:0x80001030,lc_native_save_cleanup:0x80001040,lc_native_options_init:0x80001050,lc_native_options_load:0x80001060,lc_native_options_save:0x80001070,lc_native_clear:0x80001080}).map(([n,a])=>n+' = 0x'+a.toString(16)+';').join('\n'));
run('mips-kmc-elf-ld.exe',['-EB','-T','codec.ld','-o','codec.elf','codec.o']);
run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(x=>'--remove-section='+x),'codec.elf','codec.bin']);
const elf=fs.readFileSync(path.join(work,'codec.elf')),entries={};
const sections=elf.readUInt32BE(32),sectionCount=elf.readUInt16BE(48);
for(let i=0;i<sectionCount;i++){
 const h=sections+i*40;if(elf.readUInt32BE(h+4)!==2)continue;
 const start=elf.readUInt32BE(h+16),end=start+elf.readUInt32BE(h+20);
 const strings=elf.readUInt32BE(sections+elf.readUInt32BE(h+24)*40+16);
 for(let s=start;s<end;s+=16)if((elf[s+12]&15)===2){
  const at=strings+elf.readUInt32BE(s),name=elf.toString('ascii',at,elf.indexOf(0,at));
  entries[name]=elf.readUInt32BE(s+4);
 }
}
// Compile both production translation units independently. This models the
// linked storage split without allowing one module's optimizer to hide imports.
fs.copyFileSync(path.join(native,'game/scenarios/capacity-save.h'),path.join(work,'capacity-save.h'));
for(const format of [4,5])for(const module of ['resident','compression']){
 const names=module==='resident'?['save-runtime.c','event-flags.c']:['save-compression.c','save-format.c','save-image.c'];
 const body='#define LC_CUSTOM_STORY_FLAGS 1\n#define LC_SAVE_RUNTIME_FORMAT '+format+'\n'+names.map(n=>fs.readFileSync(path.join(native,'game/classes',n),'utf8')).join('\n')+(module==='resident'?'\nvoid lc_native_invalidate_code(void *, unsigned int);\nvoid lc_native_class_dma(unsigned int, void *, unsigned int);\nvoid lc_save_load_data(unsigned int rom, void *data, unsigned int size) {lc_native_class_dma(rom,data,size);lc_native_invalidate_code(data,size);}\n':'');
 fs.writeFileSync(path.join(work,module+'.c'),body);
 run('cpp.exe',['-P','-undef','-nostdinc',module+'.c',module+'.i']);
 run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char',module+'.i','-o',module+'.s']);
 run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o',module+'.o',module+'.s']);
 const object=fs.readFileSync(path.join(work,module+'.o')),sh=object.readUInt32BE(32),count=object.readUInt16BE(48),str=object.readUInt32BE(sh+object.readUInt16BE(50)*40+16);
 for(let i=0;i<count;i++){const h=sh+i*40,a=str+object.readUInt32BE(h),name=object.toString('ascii',a,object.indexOf(0,a));if(name==='.text'){const size=object.readUInt32BE(h+20);if(module==='resident')assert(size<=0x2b70);console.log(JSON.stringify({format,module,textBytes:size}));}}
}
const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(0x80280000,fs.readFileSync(path.join(work,'codec.bin')));
const source=0x80300000,packed=0x80310000,decoded=0x80320000,scratch=0x80360000,sp=0x803f0000;
const read=(at,size)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+size));
function call(name,...args){
 m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));
 m.call(entries[name]||name,0x80000100,150000000);assert.equal(m.get('sp'),sp);return m.get('v0');
}
function crc(data){let c=0xffffffff;for(const b of data){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return(~c)>>>0;}
assert.equal(crc(Buffer.from('123456789')),0xcbf43926);
// Synthetic byte patterns are independent of game semantics: migration must
// preserve every old byte except the authenticated header and inserted bank.
const normal=[0x1850,0x1870,0x1870,0x1870,0x18b0,0x18b0,0x18b0];
const suspend=[0x4ae8,0x4b08,22820,27256,0x4b48,27320,22884];
let migrations=0,roundtrips=0,rejections=0;
function pattern(n,seed=0){return Buffer.from(Array.from({length:n},(_,i)=>(i*37+(i>>7)*13+seed)&255));}
function sealed(slot,format,seed=0){
 const n=slot===15?suspend[format]:normal[format],b=pattern(n,seed);
 m.load(source,b);assert.equal(call('lc_save_record_seal',source,n,slot,format),1);
 assert.equal(call('lc_save_record_valid',source,n,slot,format),1);
 return read(source,n);
}
for(let format=0;format<=3;++format)for(const slot of [0,1,15]){
 const target=format===3?5:format===2?6:4,n=slot===15?suspend[target]:normal[target];
 const old=sealed(slot,format,format+slot),at=format?0x1870:0x1850,extra=format?64:96;
 m.load(source-8,Buffer.alloc(n+16,0xa6));m.load(source,old);
 assert.equal(call('lc_save_upgrade_record',source,n,slot,format),1);
 const next=read(source,n);assert.equal(call('lc_save_record_valid',source,n,slot,target),1);
 assert.deepEqual(next.subarray(12,at),old.subarray(12,at));
 assert.deepEqual(next.subarray(0x1870,0x18b0),Buffer.alloc(64));
 if(slot===15)assert.deepEqual(next.subarray(at+extra),old.subarray(at));
 if(format)assert.deepEqual(next.subarray(0x1850,0x1870),old.subarray(0x1850,0x1870));
 assert.deepEqual(read(source-8,8),Buffer.alloc(8,0xa6));assert.deepEqual(read(source+n,8),Buffer.alloc(8,0xa6));
 migrations++;
 // A bad checksum and short output capacity must not modify even one byte.
 for(const bad of [false,true]){
  const input=Buffer.alloc(n,0x77);old.copy(input);if(bad)input[20]^=1;m.load(source,input);
  assert.equal(call('lc_save_upgrade_record',source,bad?n:n-1,slot,format),0);
  assert.deepEqual(read(source,n),input);rejections++;
 }
}
// Normal records and Controller Pak retain all64 custom bytes, including both
// boundary bits. Decode uses the destination slot's checksum seed.
for(const slot of [0,1]){
 const old=sealed(slot,4,slot+19);m.load(source,old);
 assert.equal(call('lc_save_pak_encode',source,old.length,slot),1);
 const pak=read(source,old.length);assert.equal(pak.readUInt32BE(4),0x4c433037);
 assert.equal(call('lc_save_pak_decode',source,old.length,1-slot),1);
 assert.equal(call('lc_save_record_valid',source,old.length,1-slot,4),1);
 assert.deepEqual(read(source+12,old.length-12),old.subarray(12));roundtrips++;
 pak[0x1870]^=1;m.load(source,pak);
 assert.equal(call('lc_save_pak_decode',source,pak.length,slot),0);
 assert.deepEqual(read(source,pak.length),pak);rejections++;
}
// Legacy Pak adapters still accept both historical record formats.
for(const format of [0,1]){
 const old=sealed(0,format,7),n=0x18b0;
 const p=Buffer.alloc(n);old.copy(p);let sum=0,bits=0;
 for(const b of old.subarray(12)){sum=(sum+b)&65535;for(let v=b;v;v>>=1)bits+=v&1;}
 p.writeUInt32BE(((sum<<16)|(bits&65535))>>>0);m.load(source,p);
 assert.equal(call('lc_save_pak_decode',source,n,1),1);
 assert.equal(call('lc_save_record_valid',source,n,1,1),1);
 assert.equal(call('lc_save_upgrade_record',source,n,1,1),1);
 assert.deepEqual(read(source+0x1870,64),Buffer.alloc(64));roundtrips++;
}
// Expanded-army conversion rebases quick and mission data after the custom
// bank. Expected slices come from the source layout, including opaque bytes.
{
 const old=Buffer.alloc(suspend[4]);pattern(0x18b0,37).copy(old);
 pattern(9164,91).copy(old,0x18b0+3788);
 m.load(source,old);call('lc_save_record_seal',source,old.length,15,4);const signed=read(source,old.length);
 const full=Buffer.alloc(suspend[5],0xa6);signed.copy(full);m.load(source,full);
 assert.equal(call('lc_save_expand_capacity',source,full.length-1),0);assert.deepEqual(read(source,full.length),full);rejections++;
 assert.equal(call('lc_save_expand_capacity',source,full.length),1);
 assert.equal(call('lc_save_record_valid',source,full.length,15,5),1);
 assert.deepEqual(read(source+12,0x18b0-12),signed.subarray(12,0x18b0));
 const mission=0x18b0+9300+664,oldMission=0x18b0+3788;
 assert.deepEqual(read(source+mission,40),signed.subarray(oldMission,oldMission+40));
 assert.deepEqual(read(source+mission+40,24),Buffer.alloc(24));
 assert.deepEqual(read(source+mission+64,8400),signed.subarray(oldMission+40,oldMission+8440));
 assert.deepEqual(read(source+mission+64+50*168,11*168),Buffer.alloc(11*168));
 assert.deepEqual(read(source+mission+64+61*168,724),signed.subarray(oldMission+8440));
 const before=read(source,full.length);assert.equal(call('lc_save_expand_capacity',source,full.length),1);assert.deepEqual(read(source,full.length),before);migrations++;
}
const image=0x80200000,upgrade=0x80380000,upgradeBytes=32768+27320+0x4f00+2*(4096+27320);
function oldImage(){
 const b=Buffer.alloc(0x8000,0x69);
 for(const slot of [14,0,1,15]){
  if(slot===14){m.load(source,pattern(16,99));call('lc_save_record_seal',source,16,14,0);read(source,16).copy(b);}
  else sealed(slot,0,slot+41).copy(b,slot===15?0x30b0:16+slot*0x1850);
 }
 m.load(image,b);return b;
}
for(const oldFormat of [0,1,2,3]){
 let original=oldImage();assert.equal(call('lc_save_image_version',image,0x8000),0);
 if(oldFormat){
  assert.equal(call('lc_save_expand_image',image,0x8000),1);
  if(oldFormat>=2){
   const record=sealed(15,oldFormat,61);
   m.load(source,record);
   assert.equal(call('lc_save_commit_compressed',image,0x8000,source,record.length,scratch,0x4f00+2*(4096+record.length)),1);
  }
  original=read(image,0x8000);
 }
 const normals=[0,1].map(slot=>{
  const at=16+slot*(oldFormat?0x1870:0x1850),n=oldFormat?0x1870:0x1850;
  return original.subarray(at,at+n);
 });
 assert.equal(call('lc_save_upgrade_image',image,0x8000,upgrade,upgradeBytes),1);
 const next=read(image,0x8000),version=call('lc_save_image_version',image,0x8000);
 assert.equal(version,oldFormat>=2?4:3);assert.deepEqual(next.subarray(0,16),original.subarray(0,16));
 for(const slot of [0,1]){
  const at=16+slot*0x18b0;m.load(source,next.subarray(at,at+0x18b0));
  assert.equal(call('lc_save_record_valid',source,0x18b0,slot,4),1);
  assert.deepEqual(read(source+12,normals[slot].length-12),normals[slot].subarray(12));
  assert.deepEqual(read(source+0x1870,64),Buffer.alloc(64));
 }
 if(version===4){
  const size=call('lc_save_read_compressed',image,0x8000,decoded,32768);
  assert.equal(size,suspend[oldFormat===3?5:6]);assert.deepEqual(read(decoded+0x1870,64),Buffer.alloc(64));
 }
 migrations++;
 // Upgrade is idempotent and does not touch an existing custom campaign.
 assert.equal(call('lc_save_upgrade_image',image,0x8000,0,0),1);assert.deepEqual(read(image,0x8000),next);
}
for(const format of [4,5,6]){
 oldImage();assert.equal(call('lc_save_upgrade_image',image,0x8000,upgrade,upgradeBytes),1);
 const normalBytes=read(image,0x3170),record=sealed(15,format,83);
 m.load(source,record);
 assert.equal(call('lc_save_commit_compressed',image,0x8000,source,record.length,scratch,0x4f00+2*(4096+record.length)),1);
 assert.equal(call('lc_save_image_version',image,0x8000),4);assert.deepEqual(read(image,0x3170),normalBytes);
 assert.equal(call('lc_save_read_compressed',image,0x8000,decoded,32768),record.length);
 assert.deepEqual(read(decoded,record.length),record);roundtrips++;
 const saved=read(image,0x8000);m.w8(image+0x3170+34,m.r8(image+0x3170+34)^1);
 assert.equal(call('lc_save_read_compressed',image,0x8000,decoded,32768),0);rejections++;
 m.load(image,saved);m.w8(image+0x7fff,m.r8(image+0x7fff)^1);
 const corrupt=read(image,0x8000);
 assert.equal(call('lc_save_upgrade_image',image,0x8000,upgrade,upgradeBytes),0);
 assert.deepEqual(read(image,0x8000),corrupt);rejections++;
}
// Refuse unknown layout versions before modifying even valid neighboring slots.
oldImage();m.load(image+0x7ff0,Buffer.from('LC99SRA9'));
const unsupported=read(image,0x8000);
assert.equal(call('lc_save_upgrade_image',image,0x8000,upgrade,upgradeBytes),0);
assert.deepEqual(read(image,0x8000),unsupported);rejections++;
// A valid old frame may fit its historical capacity yet fail the smaller new
// physical allocation. Migration must leave the complete old image recoverable.
oldImage();assert.equal(call('lc_save_expand_image',image,0x8000),1);
let seed=0x42c6e893,noise=Buffer.alloc(suspend[3]);
for(let i=12;i<19740;i++){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;noise[i]=seed&255;}
m.load(source,noise);call('lc_save_record_seal',source,noise.length,15,3);
assert.equal(call('lc_save_commit_compressed',image,0x8000,source,noise.length,scratch,0x4f00+2*(4096+noise.length)),1);
const oldFrame=read(image,0x8000);
assert.equal(call('lc_save_upgrade_image',image,0x8000,upgrade,upgradeBytes),0);
assert.deepEqual(read(image,0x8000),oldFrame);rejections++;
assert.equal(call('lc_save_read_compressed',image,0x8000,decoded,32768),noise.length);
// Exercise the actual callback dispatcher through normal, suspend and Pak
// runtime entry points. Device I/O is unnecessary: a private cache is bound.
let heap=0x80010000;
m.trap(0x80001000,c=>{const at=heap;heap=(heap+c.get('a0')+15)&~15;assert(heap<0x80100000);c.set('v0',at);});
for(const at of [0x80001030,0x80001040])m.trap(at,()=>{});
oldImage();assert.equal(call('lc_save_upgrade_image',image,0x8000,upgrade,upgradeBytes),1);
m.w32(0x8027f000,image);m.load(0x8027f100,Buffer.alloc(13*28));
for(const [i,name] of ['lc_custom_flags_init','lc_custom_flags_load','lc_custom_flags_save'].entries())m.w32(0x8027f100+5*28+i*4,entries[name]);
m.w32(0x8027f100+5*28+12,0x1864);
const live=pattern(64,29);m.load(0x8027f080,live);
assert.equal(call('lc_save_game',1),1);assert.deepEqual(read(image+0x18c0+0x1870,64),live);
call('lc_save_initialize_slot',0);assert.deepEqual(read(0x8027f080,64),live);assert.deepEqual(read(image+16+0x1870,64),Buffer.alloc(64));
m.load(0x8027f080,Buffer.alloc(64));call('lc_load_game',1);assert.deepEqual(read(0x8027f080,64),live);
const file=0x80350000;
assert.equal(call('lc_pak_export',1,file,0x1900),1);
assert.equal(call('lc_pak_import',0,file,0x18d0-1),0);rejections++;
const prior=Buffer.alloc(64,0x5c);m.load(0x8027f080,prior);
assert.equal(call('lc_pak_import',0,file,0x1900),1);assert.deepEqual(read(0x8027f080,64),prior);
call('lc_load_game',0);assert.deepEqual(read(0x8027f080,64),live);
assert.equal(call('lc_save_suspend'),1);m.load(0x8027f080,Buffer.alloc(64));
call('lc_load_suspend');assert.deepEqual(read(0x8027f080,64),live);roundtrips+=3;
// Callback initialization zeros only its record. Loading/saving writes exactly
// the64-byte bank; the explicit new-game reset is the live clear operation.
const bank=0x8027f080;
m.load(bank-8,Buffer.alloc(80,0xa6));m.load(source,Buffer.alloc(80,0xb7));
call('lc_custom_flags_init',source);assert.deepEqual(read(bank-8,80),Buffer.alloc(80,0xa6));
assert.deepEqual(read(source,64),Buffer.alloc(64));assert.deepEqual(read(source+64,16),Buffer.alloc(16,0xb7));
const flags=pattern(64,31);flags[0]|=1;flags[63]|=128;m.load(source,flags);call('lc_custom_flags_load',source);
assert.deepEqual(read(bank,64),flags);assert.deepEqual(read(bank-8,8),Buffer.alloc(8,0xa6));assert.deepEqual(read(bank+64,8),Buffer.alloc(8,0xa6));
call('lc_custom_flags_save',decoded);assert.deepEqual(read(decoded,64),flags);
m.trap(0x80001080,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
call('lc_clear_game_state',source,3);assert.deepEqual(read(bank,64),Buffer.alloc(64));assert.deepEqual(read(bank+64,8),Buffer.alloc(8,0xa6));
console.log(JSON.stringify({migrations,roundtrips,rejections,customCallbackLifecycle:'pass',work}));
