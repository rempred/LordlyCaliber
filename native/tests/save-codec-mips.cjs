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
if (!filename) { console.log('SKIP: supply the private Rev 0 ROM'); process.exit(0); }
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
for (const file of ['save-format.c','save_format.h'])
  fs.copyFileSync(path.join(native,'game/classes',file),path.join(work,file));
const run = (tool,args) => cp.execFileSync(path.join(tools,tool),args,{cwd:work,encoding:'utf8'});
run('cpp.exe',['-P','-undef','-nostdinc','save-format.c','save-format.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','save-format.i','-o','save-format.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','save-format.o','save-format.s']);
fs.writeFileSync(path.join(work,'save-format.ld'),'SECTIONS { .text 0x80200000 : { *(.text) } }\n');
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
    if(name.startsWith('lc_save_')) entries[name]=elf.readUInt32BE(s+4);
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
