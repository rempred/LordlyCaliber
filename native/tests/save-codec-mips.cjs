'use strict';
// Original ROM codec and boot-load instructions in isolated 4 MiB RAM.
// No emulator, cartridge SRAM, source ROM or existing save is modified.
const fs = require('node:fs');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const path = require('node:path');
const cp = require('node:child_process');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const filename = process.argv[2] || process.env.LORDLY_TEST_ROM;
if (!filename) { console.log('SKIP: supply the private Rev 0 ROM'); process.exit(77); }
const rom = fs.readFileSync(filename);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
assert.equal(crypto.createHash('sha256').update(rom).digest('hex'),
  '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
const m = new Mips(); m.ram = new Uint8Array(0x400000); m.set('sp', 0x80060000);
m.load(0x80070C00, rom.subarray(0x1000, 0x2F000));
const transfers = [];
for (const address of [0x800EA714, 0x80073FD4, 0x800900C0, 0x80090010])
  m.trap(address, () => {});
m.trap(0x8009DA50, c => {
  const from = c.get('a0'), to = c.get('a1'), length = c.get('a2');
  transfers.push([from,to,length]); c.load(to, rom.subarray(from,from+length));
});
m.call(0x80071FF4, 0x80072078);
assert.deepEqual(transfers, [[0x40E80,0x8016AF80,0x25F90]]);
const descriptor = 0x80187444, groups = [];
let bits = 0, discoveryBit;
for (let at = descriptor; m.r32(at); at += 16) {
  assert(groups.length < 32);
  const base = m.r32(at), stride = m.r32(at+4), commands = m.r32(at+8), count = m.r32(at+12);
  assert(base >= 0x80000000 && base + stride * count <= 0x80400000);
  const fields = [];
  for (let cmd = commands; m.r8(cmd+1) & 127; cmd += 3) {
    const field = {offset:m.r8(cmd),bytes:m.r8(cmd+1)&127,bits:m.r8(cmd+2)};
    if (base === 0x80196A58 && field.offset === 30) discoveryBit = bits;
    assert(field.bytes <= 4 && field.bits <= 32 && fields.length < 256);
    fields.push(field); bits += field.bits * count;
  }
  groups.push({base,stride,count,fields});
}
assert.equal(groups.length,21); assert.equal(bits,49433); assert.equal(Math.ceil(bits/8),6180);
assert.equal(discoveryBit,43196);
const roster = groups.find(g => g.base === 0x80193BC0);
assert.equal(roster.stride,56); assert.equal(roster.count,100);
for (const offset of [17,18])
  assert.deepEqual(roster.fields.find(f => f.offset === offset),{offset,bytes:1,bits:8});
const state = groups.find(g => g.base === 0x80196A58);
for (let offset = 30; offset < 41; ++offset)
  assert.deepEqual(state.fields.find(f => f.offset === offset),{offset,bytes:1,bits:8});
let classValues = 0;
const payload = 0x80350000;
for (let batch = 0; batch < 3; ++batch) {
  const expected = [];
  for (let actor = 0; actor < 100; ++actor) {
    const primary = (batch*100+actor)&255, secondary = 255-primary;
    const at = roster.base + actor*56;
    m.w8(at+17,primary); m.w8(at+18,secondary); m.w8(at+19,99);
    expected.push([primary,secondary,99]);
  }
  m.load(payload-16,Buffer.alloc(6180+32,0xA6));
  m.set('a0',payload); m.set('a1',descriptor); m.call(0x80074494,0x80000100,5000000);
  assert.equal(m.r32(0x800AEFB4),6180);
  assert.equal(m.r8(payload-1),0xA6); assert.equal(m.r8(payload+6180),0xA6);
  for (let actor = 0; actor < 100; ++actor) {
    const at = roster.base + actor*56;
    m.w8(at+17,0xA6); m.w8(at+18,0xA6); m.w8(at+19,0xA6);
  }
  m.set('a0',payload); m.set('a1',descriptor); m.call(0x800742F4,0x80000100,5000000);
  for (let actor = 0; actor < 100; ++actor) {
    const at = roster.base + actor*56;
    assert.deepEqual([m.r8(at+17),m.r8(at+18),m.r8(at+19)],expected[actor]); classValues+=2;
  }
}
console.log(JSON.stringify({groups:groups.length,packedBits:bits,packedBytes:6180,discoveryBit,
  classFieldValues:classValues,distinctClassIds:256,discoveryBytes:11,
  scope:'original descriptor/codec execution; save IO and extended discovery remain separate'}));

// Compile the maintained converter with the same pinned N64 toolchain used in
// exports, then compare its legacy checksum with the original game routine.
const native = path.resolve(__dirname,'..');
const tools = process.env.LORDLY_N64_TOOLCHAIN || path.join(native,'.local/n64-toolchain');
const pins = JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json')));
for (const [file,expected] of Object.entries(pins.tools))
  assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(tools,file))).digest('hex'),expected);
fs.mkdirSync(path.join(native,'build'),{recursive:true});
const work = fs.mkdtempSync(path.join(native,'build/save-format-'));
for (const file of ['save-format.c','save_format.h','save_checksums.h'])
  fs.copyFileSync(path.join(native,'game/classes',file),path.join(work,file));
fs.appendFileSync(path.join(work,'save-format.c'),'\n'+fs.readFileSync(path.join(native,'game/classes/save-runtime.c'),'utf8'));
const run = (tool,args) => cp.execFileSync(path.join(tools,tool),args,{cwd:work,encoding:'utf8'});
run('cpp.exe',['-P','-undef','-nostdinc','save-format.c','save-format.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','save-format.i','-o','save-format.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','save-format.o','save-format.s']);
fs.writeFileSync(path.join(work,'save-format.ld'),
  'lc_save_cache = 0x800A83B8; lc_save_dirty = 0x800A83BC; lc_discovery_flags = 0x80250000;\n'+
  'lc_legacy_discovery = 0x80196A76; lc_native_save_alloc = 0x80070F30;\n'+
  'lc_native_sram_io = 0x8008A0F0; lc_native_main_save = 0x8016CF64;\n'+
  'lc_native_save_free = 0x800712C4; lc_native_save_cleanup = 0x80074BF0; lc_save_callbacks = 0x800A824C;\n'+
  'lc_native_options_init = 0x8016CD90; lc_native_options_load = 0x8016CDCC; lc_native_options_save = 0x8016CDF4;\n'+
  'lc_native_clear = 0x80093380;\n'+
  'SECTIONS { .text 0x80200000 : { *(.text) } }\n');
run('mips-kmc-elf-ld.exe',['-EB','-T','save-format.ld','-o','save-format.elf','save-format.o']);
run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(x=>'--remove-section='+x),'save-format.elf','save-format.bin']);
const elf=fs.readFileSync(path.join(work,'save-format.elf')), entries={};
const sections=elf.readUInt32BE(32), sectionCount=elf.readUInt16BE(48);
for (let i=0;i<sectionCount;++i) {
  const h=sections+i*40;
  if (elf.readUInt32BE(h+4)!==2) continue;
  const start=elf.readUInt32BE(h+16),end=start+elf.readUInt32BE(h+20);
  const names=sections+elf.readUInt32BE(h+24)*40,strings=elf.readUInt32BE(names+16);
  for(let s=start;s<end;s+=16) if((elf[s+12]&15)===2) {
    const text=strings+elf.readUInt32BE(s),name=elf.toString('ascii',text,elf.indexOf(0,text));
    if(name.startsWith('lc_save_') || name.startsWith('lc_load_') || name.startsWith('lc_discovery_') || name.startsWith('lc_pak_')) entries[name]=elf.readUInt32BE(s+4);
  }
}
const code=fs.readFileSync(path.join(work,'save-format.bin'));
m.load(0x80200000,code);
m.load(0x800A8240,rom.subarray(0x38640,0x38648));
m.trap(0x80093060,c=>c.load(c.get('a1'),Buffer.from(c.ram.slice(c.phys(c.get('a0')),c.phys(c.get('a0'))+c.get('a2')))));
function call(name,...args) {
  args.forEach((value,i)=>m.set('a'+i,value));
  const entry=typeof name==='number'?name:entries[name];assert(entry,name);
  m.call(entry,0x80000100,8000000);return m.get('v0');
}
const read=(at,length)=>Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+length));
const source=0x80300000, output=0x80310000;
let formats=0,rejections=0;
for (const slot of [0,1,14,15]) for (const pattern of [0,0xA5,0xFF,37]) {
  const oldSize=slot===14?16:slot===15?0x4AE8:0x1850;
  const size=slot===14?16:slot===15?0x4B08:0x1870;
  m.load(source,Buffer.alloc(size+16,pattern));
  const discovery=Buffer.from({length:11},(_,i)=>(i*37+pattern)&255);
  if(slot!==14) {
    m.load(0x80196A76,discovery);
    call(0x80074494,source+0x2A,descriptor);
  }
  const unsealed=read(source,size+16);m.load(output,unsealed);
  call(0x8007599C,slot,source);
  assert.equal(call('lc_save_record_seal',output,size,slot,0),1);
  assert.deepEqual(read(output,oldSize),read(source,oldSize),'legacy checksum/signature compatibility');
  for (const cleared of slot===15?[false,true]:[false]) {
    const old=read(source,size+16);if(cleared)old.writeUInt32BE(0,0);
    m.load(output,old);
    assert.equal(call('lc_save_expand_record',output,size-1,slot),0);
    assert.deepEqual(read(output,size+16),old,'short buffer unchanged');rejections++;
    if(!cleared) {
      m.w8(output+12,m.r8(output+12)^1);const broken=read(output,size+16);
      assert.equal(call('lc_save_expand_record',output,size,slot),0);
      assert.deepEqual(read(output,size+16),broken,'corrupt record unchanged');rejections++;
    }
    m.load(output,old);
    assert.equal(call('lc_save_expand_record',output,size,slot),1);
    assert.equal(call('lc_save_record_valid',output,size,slot,1),1);
    const converted=read(output,size+16);
    assert.deepEqual(converted.subarray(size),old.subarray(size),'bytes after allocation unchanged');
    if(slot===14) assert.deepEqual(converted,old,'options unchanged');
    else {
      assert.deepEqual(converted.subarray(12,0x1850),old.subarray(12,0x1850),'packed main data unchanged');
      assert.deepEqual(converted.subarray(0x1850,0x185B),cleared?Buffer.alloc(11):discovery);
      assert.deepEqual(converted.subarray(0x185B,0x1870),Buffer.alloc(21));
      if(slot===15)assert.deepEqual(converted.subarray(0x1870,size),old.subarray(0x1850,oldSize));
      assert.equal(call('lc_save_record_valid',output,size,slot,0),0);
      assert.equal(call('lc_save_expand_record',output,size,slot),0);
      assert.deepEqual(read(output,size+16),converted,'second conversion refused before writes');rejections++;
    }
    formats++;
  }
}
console.log(JSON.stringify({convertedRecords:formats,rejections,codeBytes:code.length,
  scope:'source-built record conversion and original checksum comparison; no SRAM/Pak IO installation'}));

let images=0;
for(let corruptMask=0;corruptMask<8;++corruptMask) {
  const image=Buffer.from({length:0x8000},(_,i)=>(i*19+37)&255),oldRecords=[];
  for(const slot of [0,1,14,15]) {
    const offset=slot===14?0:slot===15?0x30B0:16+slot*0x1850;
    const size=slot===14?16:slot===15?0x4AE8:0x1850;
    m.load(source,image.subarray(offset,offset+size));call(0x8007599C,slot,source);
    if(slot!==14 && (corruptMask & (1<<(slot===15?2:slot))))m.w8(source+12,m.r8(source+12)^1);
    const record=read(source,size);record.copy(image,offset);oldRecords.push({slot,record});
  }
  m.load(source,image);
  assert.equal(call('lc_save_image_version',source,0x8000),0);
  assert.equal(call('lc_save_expand_image',source,0x7FFF),0);
  assert.deepEqual(read(source,0x8000),image);
  assert.equal(call('lc_save_expand_image',source,0x8000),1);
  assert.equal(call('lc_save_image_version',source,0x8000),1);
  const converted=read(source,0x8000);
  assert.deepEqual(converted.subarray(0,16),image.subarray(0,16),'options retained');
  assert.deepEqual(converted.subarray(0x7C08),image.subarray(0x7C08),'unallocated tail retained');
  for(const {slot,record} of oldRecords) if(slot!==14) {
    const offset=slot===15?0x30F0:16+slot*0x1870,size=slot===15?0x4B08:0x1870;
    const invalid=!!(corruptMask&(1<<(slot===15?2:slot)));
    assert.equal(call('lc_save_record_valid',source+offset,size,slot,1),invalid?0:1);
    assert.deepEqual(converted.subarray(offset+12,offset+0x1850),record.subarray(12,0x1850));
    if(invalid)assert.deepEqual(converted.subarray(offset,offset+12),record.subarray(0,12),'invalid header not resealed');
    if(slot===15)assert.deepEqual(converted.subarray(offset+0x1870,offset+size),record.subarray(0x1850));
  }
  assert.equal(call('lc_save_expand_image',source,0x8000),1);
  assert.deepEqual(read(source,0x8000),converted,'image migration is idempotent');
  m.w8(source+0x7C07,m.r8(source+0x7C07)^1);const damaged=read(source,0x8000);
  assert.equal(call('lc_save_image_version',source,0x8000),0xFFFFFFFF);
  assert.equal(call('lc_save_expand_image',source,0x8000),0);
  assert.deepEqual(read(source,0x8000),damaged,'damaged format marker rejected before writes');
  images++;
}
console.log(JSON.stringify({imageConversions:images,invalidSlotCombinations:8,
  scope:'complete synthetic SRAM buffer conversion; no cartridge write or runtime dispatch'}));

// Runtime services plus the unchanged native flush against a simulated device.
// Real files and emulator saves are never passed to these write callbacks.
const cartridge=Buffer.alloc(0x8000,0xA6),cache=0x80360000;
for(const slot of [0,1,14,15]) {
  const offset=slot===14?0:slot===15?0x30B0:16+slot*0x1850;
  const size=slot===14?16:slot===15?0x4AE8:0x1850;
  m.load(source,Buffer.alloc(size,slot+3));call(0x8007599C,slot,source);
  read(source,size).copy(cartridge,offset);
}
const originalCartridge=Buffer.from(cartridge),io=[],flushWork=0x80359000;
let allocations=0,frees=0,workAllocations=0,workFrees=0;
m.w32(0x800A83B8,0);m.w8(0x800A83BC,0);
m.w8(0x800C4800,0); // Original flush's write-disable flag.
m.trap(0x80070F30,c=>{
  if(c.get('a0')===16){workAllocations++;c.set('v0',flushWork);}
  else{assert.equal(c.get('a0'),0x8000);allocations++;c.set('v0',cache);}
});
m.trap(0x800712C4,c=>{
  if(c.get('a0')===flushWork)workFrees++;
  else{assert.equal(c.get('a0'),cache);frees++;}
});
m.trap(0x8008A0F0,c=>{
  const at=c.get('a0'),address=c.get('a1'),size=c.get('a2'),write=c.get('a3');
  assert.equal(size,256);assert(at+size<=0x8000);assert.equal(at&255,0);
  io.push([at,write]);
  if(write)read(address,size).copy(cartridge,at);else c.load(address,cartridge.subarray(at,at+size));
});
assert.equal(call('lc_save_read_normal',0,output),1);
assert.equal(allocations,1);assert.equal(io.length,128);assert(io.every(x=>x[1]===0));
assert.equal(m.r8(0x800A83BC),0);assert.deepEqual(cartridge,originalCartridge);
assert.equal(call('lc_save_read_normal',1,output),1);assert.equal(io.length,128);
call(0x80074BF0,0x37081383); // Unmodified cleanup/flush.
assert.equal(frees,1);assert.equal(io.length,128);assert.deepEqual(cartridge,originalCartridge);
assert.equal(m.r32(0x800A83B8),0);
assert.equal(call('lc_save_read_normal',1,output),1);
assert.equal(allocations,2);assert.equal(io.length,256);
m.w8(output+0x1850+31,0x80);call('lc_save_sign',1,output);
call('lc_save_write_normal',1,output);assert.equal(m.r8(0x800A83BC),1);
const expectedCartridge=read(cache,0x8000);
assert.deepEqual(cartridge,originalCartridge,'staging a save has not yet written the device');
call(0x80074BF0,0x37081383);
assert.equal(io.length,384);assert(io.slice(256).every(x=>x[1]===1));
assert.equal(frees,2);assert.equal(m.r8(0x800A83BC),0);assert.equal(m.r32(0x800A83B8),0);
assert.equal(workAllocations,1);assert.equal(workFrees,1);
assert.deepEqual(cartridge,expectedCartridge,'native flush writes exactly the normalized image');
assert.equal(call('lc_save_read_normal',1,output),1);
assert.equal(m.r8(output+0x1850+31),0x80,'high discovery bit survives reload');
const beforeInvalid=read(cache,0x8000),oldDirty=m.r8(0x800A83BC);
call('lc_save_write_normal',2,output);assert.equal(m.r8(0x800A83BC),oldDirty);
assert.deepEqual(read(cache,0x8000),beforeInvalid,'invalid slot cannot modify cache');
call(0x80074BF0,0x37081383);
// Discovery callbacks synchronize stock bits without touching neighboring flags.
const flags=Buffer.from({length:32},(_,i)=>(i*29+7)&255);
m.load(0x80250000-1,Buffer.alloc(34,0xA6));m.load(0x80196A76-1,Buffer.alloc(13,0xA6));
m.load(source,flags);call('lc_discovery_load',source);
assert.deepEqual(read(0x80250000,32),flags);assert.deepEqual(read(0x80196A76,11),flags.subarray(0,11));
m.load(output,Buffer.alloc(33,0xA6));call('lc_discovery_save',output);
assert.deepEqual(read(output,32),flags);assert.equal(m.r8(output+32),0xA6);
m.load(0x80196A76,Buffer.alloc(11));let mainCalls=0;
m.trap(0x8016CF64,c=>{assert.equal(c.get('a0'),output);assert.deepEqual(read(0x80196A76,11),flags.subarray(0,11));mainCalls++;});
call('lc_save_main_payload',output);assert.equal(mainCalls,1);
call('lc_discovery_init',output);
const initialFlags=Buffer.alloc(32);flags.copy(initialFlags,0,0,11);
assert.deepEqual(read(0x80250000,32),initialFlags);
assert.deepEqual(read(output,32),initialFlags);
assert.deepEqual(read(0x80196A76,11),flags.subarray(0,11));
for(const address of [0x8024FFFF,0x80250020,0x80196A75,0x80196A81])assert.equal(m.r8(address),0xA6);
console.log(JSON.stringify({runtimeCacheReads:3,explicitSaveFlushes:1,highDiscoveryBitReloaded:true,
  scope:'source-built cache/callbacks and original flush with simulated SRAM; ROM installation still pending'}));

// Execute the complete new dispatch with independent callback payloads, guarded
// allocations and the original cleanup/flush. This detects old record strides,
// overlap with suspend-only data, stale discovery bits and invalid-slot writes.
let runtimeFlags=0x80250000;
if(process.argv[3]) {
  const candidate=fs.readFileSync(process.argv[3]);
  if(candidate.readUInt32BE(0)===0x37804012)candidate.swap16();
  const header=candidate.subarray(0x5DC80,0x5DD00);
  assert.equal(header.toString('ascii',0,8),'LC05CLAS');assert([2,3].includes(header.readUInt32BE(8)));
  runtimeFlags=header.readUInt32BE(44);
  const codeEnd=0x80187F00+header.readUInt32BE(84),pools=[],heaps=[],cacheRanges=[];
  m.load(0x80070C00,candidate.subarray(0x1000,0x3A000));
  m.trap(0x800900C0,c=>cacheRanges.push([c.get('a0'),c.get('a1')]));
  m.trap(0x8009DA50,c=>c.load(c.get('a1'),candidate.subarray(c.get('a0'),c.get('a0')+c.get('a2'))));
  m.call(0x80071FF4,0x80072078);
  assert.equal(cacheRanges.length,1);assert.equal(cacheRanges[0][0]+cacheRanges[0][1],codeEnd);
  m.trap(0x80070C60,c=>heaps.push([c.get('a0'),c.get('a1')]));
  m.trap(0x80070D20,c=>pools.push([c.get('a0'),c.get('a1')]));
  m.trap(0x80070F14,()=>{});
  call(header.readUInt32BE(60),0x80243DB0,0x1BC250);
  assert.deepEqual(heaps,[[header.readUInt32BE(48),0x80400000-header.readUInt32BE(48)]]);
  const reclaimed=(codeEnd+15)&~15;
  assert.deepEqual(pools,[[reclaimed>>>0,0x8018AA70-(reclaimed>>>0)],[0x8018ECE0,0x3D0],[0x8018FEC0,0x4B0]]);
  for(const entry of ['heap','secondary']) {
    pools.length=0;
    call(header.readUInt32BE(entry==='heap'?64:68),0x80243DB0,0x1BC250);
    assert.equal(pools[0][0],reclaimed>>>0);
  }
  const catalog=JSON.parse(fs.readFileSync(path.join(native,'game/classes/save-owners.json')));
  if(header.readUInt32BE(96)>=3) {
    m.load(0x80196A57,Buffer.alloc(0x96,0xA6));
    m.load(runtimeFlags-1,Buffer.alloc(34,0xA6));m.load(runtimeFlags,Buffer.alloc(32,0xFF));
    m.trap(0x80093380,c=>{
      assert.equal(c.get('a0'),0x80196A58);assert.equal(c.get('a1'),0x94);
      c.load(c.get('a0'),Buffer.alloc(c.get('a1')));
    });
    m.set('a0',0x80196A58);m.call(0x80184EA8,0x80184EB0);
    assert.deepEqual(read(0x80196A58,0x94),Buffer.alloc(0x94));
    assert.deepEqual(read(runtimeFlags,32),Buffer.alloc(32));
    for(const at of [0x80196A57,0x80196AEC,runtimeFlags-1,runtimeFlags+32])assert.equal(m.r8(at),0xA6);
  }
  for(const owner of catalog.owners)entries[owner.symbol]=0x8006FC00+owner.start;
  if(header.readUInt32BE(96)>=2)for(const owner of catalog.pakOwners) {
    m.load(owner.delta+owner.start,candidate.subarray(owner.start,owner.start+owner.size));
    const calls=[];
    for(let at=owner.fragment;at<owner.fragment+owner.fragmentSize;at+=4) {
      const word=candidate.readUInt32BE(at),target=(0x80000000|((word&0x3FFFFFF)<<2))>>>0;
      if(word>>>26===3&&target>=0x80187F00&&target<codeEnd)calls.push(target);
    }
    assert.equal(calls.length,1);entries[owner.symbol]=calls[0];
    const workspace=0x803A0000,file=0x80390000;
    for(const slot of [0,1])for(const result of [0,1]) {
      m.w32(0x8019780C,workspace);m.w32(workspace+0x48,slot);m.w32(workspace+0x54+3*32,0x1900);
      m.set('s5',file);m.set('s6',3);m.set('v0',file);
      m.trap(calls[0],c=>{assert.deepEqual([c.get('a0'),c.get('a1'),c.get('a2')],[slot,file,0x1900]);c.set('v0',result);});
      m.call(owner.delta+owner.fragment,owner.delta+(result?owner.resume:owner.reject));
      if(owner.symbol==='lc_pak_export')assert.equal(m.get('s4'),file);
    }
    m.traps.delete(calls[0]);
  }
  const table=0x800A824C;
  entries.lc_save_main_payload=m.r32(table+28+8);
  entries.lc_discovery_init=m.r32(table+4*28);
  entries.lc_discovery_load=m.r32(table+4*28+4);
  entries.lc_discovery_save=m.r32(table+4*28+8);
  console.log(JSON.stringify({installedSaveCodeBytes:header.readUInt32BE(84),
    sourceBuiltBootAndHeapResets:true,instructionCacheEnd:codeEnd.toString(16),scope:'actual candidate instructions in isolated 4 MiB RAM'}));
}
const live=new Map(),recordBuffer=0x80370000,summaryBuffer=0x8037F000;
m.trap(0x80070F30,c=>{
  const size=c.get('a0'),address=size===0x8000?cache:size===26?summaryBuffer:size===16?flushWork:recordBuffer;
  assert([16,26,0x1870,0x4B08,0x8000].includes(size));assert(!live.has(address));
  live.set(address,size);c.load(address-16,Buffer.alloc(size+32,0xA6));c.set('v0',address);
});
m.trap(0x800712C4,c=>{
  const address=c.get('a0'),size=live.get(address);assert(size);
  assert.deepEqual(read(address-16,16),Buffer.alloc(16,0xA6));
  assert.deepEqual(read(address+size,16),Buffer.alloc(16,0xA6));live.delete(address);
});
const table=0x800A824C; m.load(table,Buffer.alloc(13*28));
const callbackLog=[],mainPayload=Buffer.from({length:6180},(_,i)=>(i*17+3)&255),
  quickPayload=Buffer.from({length:0x3298},(_,i)=>(i*31+5)&255),summaryPayload=Buffer.alloc(26,0x47);
let loadedMain,loadedQuick,loadedOptions;
function callback(address,fn){m.trap(address,c=>fn(c.get('a0')));return address;}
function row(index,init,load,save,offset,quickLoad=0,quickSave=0,quickOffset=0){
  [init,load,save,offset,quickLoad,quickSave,quickOffset].forEach((v,i)=>m.w32(table+index*28+i*4,v));
}
row(0,callback(0x80000200,p=>m.load(p,Buffer.alloc(26))),callback(0x80000204,()=>{}),
  callback(0x80000208,p=>m.load(p,summaryPayload)),4);
row(1,callback(0x80000210,p=>{m.load(p,Buffer.alloc(6180));m.load(0x80196A76,Buffer.alloc(11));}),
  callback(0x80000214,p=>{loadedMain=read(p,6180);callbackLog.push('main');}),entries.lc_save_main_payload,0x1E,
  callback(0x80000218,p=>{loadedQuick=read(p,0x3298);callbackLog.push('quick');}),
  callback(0x8000021C,p=>m.load(p,quickPayload)));
row(4,entries.lc_discovery_init,entries.lc_discovery_load,entries.lc_discovery_save,0x1844);
m.trap(0x8016CF64,c=>{
  assert.deepEqual(read(0x80196A76,11),read(runtimeFlags,11));c.load(c.get('a0'),mainPayload);
});
callback(0x8016CD90,p=>m.load(p,Buffer.from([1,2,3,4])));
callback(0x8016CDF4,p=>m.load(p,Buffer.from([5,6,7,8])));
callback(0x8016CDCC,p=>{loadedOptions=read(p,4);});
cartridge.fill(0xFF);m.w32(0x800A83B8,0);m.w8(0x800A83BC,0);
assert.equal(call('lc_save_headers_missing'),1);
for(const slot of [0,1,14,15]) call('lc_save_initialize_slot',slot);
call(0x80074BF0,0x37081383);assert.equal(live.size,0);
assert.equal(call('lc_save_headers_missing'),0);
for(const slot of [0,1,14,15])assert.equal(call('lc_save_check_slot',slot),1);
call(0x80074BF0,0x37081383);assert.equal(live.size,0);
for(const slot of [0,1,15]) {
  m.load(runtimeFlags,flags);call('lc_save_game',slot);assert.equal(live.size,0);
  const offset=slot===15?0x30F0:16+slot*0x1870;
  assert.deepEqual(cartridge.subarray(offset+0x1850,offset+0x1870),flags);
  m.load(runtimeFlags,Buffer.alloc(32));callbackLog.length=0;loadedQuick=null;
  call('lc_load_game',slot);assert.equal(live.size,0);
  assert.deepEqual(loadedMain,mainPayload);assert.deepEqual(read(runtimeFlags,32),flags);
  assert.deepEqual(callbackLog,slot===15?['main','quick']:['main']);
  if(slot===15)assert.deepEqual(loadedQuick,quickPayload);
  const summary=call('lc_save_summary',slot);assert.equal(summary,summaryBuffer);
  assert.deepEqual(read(summary,26),summaryPayload);call(0x800712C4,summary);assert.equal(live.size,0);
}
assert.equal(cartridge.readUInt32BE(16+12),1);assert.equal(cartridge.readUInt32BE(0x1880+12),1);
call('lc_save_game',1);assert.equal(cartridge.readUInt32BE(0x1880+12),2);
call('lc_save_game',14);call('lc_load_game',14);assert.deepEqual(loadedOptions,Buffer.from([5,6,7,8]));
const beforeClear=Buffer.from(cartridge);call('lc_save_initialize_slot',15);call(0x80074BF0,0x37081383);
assert.equal(cartridge.readUInt32BE(0x30F0),0);
assert.deepEqual(cartridge.subarray(0x30FC),beforeClear.subarray(0x30FC),'clear suspend preserves payload');
callbackLog.length=0;call('lc_load_game',15);assert.deepEqual(callbackLog,[]);
assert.equal(call('lc_save_summary',15),0);
const beforeRepair=Buffer.from(cartridge);cartridge[0x1880+0x300]^=1;
assert.equal(call('lc_save_check_slot',1),0);call(0x80074BF0,0x37081383);
assert.deepEqual(cartridge.subarray(0,0x1880),beforeRepair.subarray(0,0x1880));
assert.deepEqual(cartridge.subarray(0x30F0),beforeRepair.subarray(0x30F0));
assert.equal(call('lc_save_check_slot',1),1);call(0x80074BF0,0x37081383);
const pakBuffer=0x80390000,pakSentinel=Buffer.alloc(16,0xA6);
let pakRoundTrips=0,pakRejections=0;
for(const legacy of [false,true])for(const slot of [0,1]) {
  m.load(runtimeFlags,flags);call('lc_save_game',slot);
  m.load(pakBuffer-16,Buffer.alloc(0x1920,0xA6));
  assert.equal(call('lc_pak_export',slot,pakBuffer,0x1900),1);
  assert.deepEqual(read(pakBuffer-16,16),pakSentinel);assert.deepEqual(read(pakBuffer+0x1900,16),pakSentinel);
  assert.deepEqual(read(pakBuffer,26),summaryPayload);
  assert.deepEqual(read(pakBuffer+0x1870+0x20,0x70),Buffer.alloc(0x70));
  const sum=call(0x80075AC4,pakBuffer+0x2C,0x1864)&65535;
  const count=call(0x80075B60,pakBuffer+0x2C,0x1864)&65535;
  assert.equal(m.r32(pakBuffer+0x20),((sum<<16)|count)>>>0,'original zero-seed checksum functions');
  if(legacy) {
    const legacyOffset=16+slot*0x1850;
    m.load(pakBuffer+0x20,originalCartridge.subarray(legacyOffset,legacyOffset+0x1850));
    const oldSum=call(0x80075AC4,pakBuffer+0x2C,0x1844)&65535;
    const oldCount=call(0x80075B60,pakBuffer+0x2C,0x1844)&65535;
    m.w32(pakBuffer+0x20,((oldSum<<16)|oldCount)>>>0);
  }
  const file=read(pakBuffer,0x1900),size=legacy?0x1870:0x1900;
  assert.equal(call('lc_pak_import',1-slot,pakBuffer,size),1);pakRoundTrips++;
  assert.deepEqual(read(pakBuffer,0x1900),file,'import preserves its input file');
  const destination=16+(1-slot)*0x1870;
  if(!legacy)assert.deepEqual(cartridge.subarray(destination+0x1850,destination+0x1870),flags);
  else assert.deepEqual(cartridge.subarray(destination+0x1850+11,destination+0x1870),Buffer.alloc(21));
  const beforeBad=Buffer.from(cartridge);
  m.w8(pakBuffer+0x300,m.r8(pakBuffer+0x300)^1);
  assert.equal(call('lc_pak_import',1-slot,pakBuffer,size),0);pakRejections++;
  assert.deepEqual(cartridge,beforeBad);assert.equal(live.size,0);
  m.load(pakBuffer,file);
  for(const shortSize of [0,31,0x186F,...(legacy?[]:[0x188F])]) {
    assert.equal(call('lc_pak_import',slot,pakBuffer,shortSize),0);pakRejections++;
    assert.deepEqual(cartridge,beforeBad);
  }
}
console.log(JSON.stringify({pakRoundTrips,pakRejections,pakFileBytes:0x1900,
  originalChecksumComparison:true,scope:'source-built Pak conversion with synthetic files; device UI installation checked separately'}));
cartridge[0x7C07]^=1;const damaged=Buffer.from(cartridge),writesBefore=io.filter(x=>x[1]).length;
assert.equal(call('lc_save_headers_missing'),0);
for(const slot of [0,1,14,15]){assert.equal(call('lc_save_check_slot',slot),0);call('lc_save_initialize_slot',slot);}
call('lc_save_game',0);call('lc_load_game',0);
assert.deepEqual(cartridge,damaged);assert.equal(io.filter(x=>x[1]).length,writesBefore);assert.equal(live.size,0);
console.log(JSON.stringify({dispatchRoundTrips:4,normalCounters:[1,2],suspendClear:true,
  isolatedSlotRepair:true,damagedLayoutWrites:0,allocationGuards:true,
  scope:'source-built dispatch with synthetic callback data and simulated SRAM; no full-game boot proof'}));
