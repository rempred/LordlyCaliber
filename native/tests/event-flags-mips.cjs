'use strict';
// Actual compiled wrapper instructions, with retail instructions as native-ID oracle.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const native=path.resolve(__dirname,'..'),scratch=path.join(native,'build/custom-story-flags-mips');
const tools=process.env.LORDLY_N64_TOOLCHAIN||path.join(native,'.local/n64-toolchain');
fs.mkdirSync(scratch,{recursive:true});
const stock=fs.readFileSync(process.env.LORDLY_TEST_ROM||path.resolve(native,'../../Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
if(stock.readUInt32BE(0)===0x37804012)stock.swap16();
assert.equal(stock.readUInt32BE(0x650e0),0x801702f8);assert.equal(stock.readUInt32BE(0x65150),0x8017049c);
function run(tool,args){const p=cp.spawnSync(path.join(tools,tool),args,{cwd:scratch,encoding:'utf8'});assert.equal(p.status,0,p.stdout+p.stderr);}
const input=path.join(native,'game/classes/event-flags.c');
run('cpp.exe',['-P','-undef','-nostdinc','-D__mips=3','-D_MIPSEB',input,'flags.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','flags.i','-o','flags.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','flags.o','flags.s']);
const codeAddress=0x80200000,bank=0x80300000;
fs.writeFileSync(path.join(scratch,'flags.ld'),'lc_custom_story_flags = 0x80300000; lc_native_story_flag_get = 0x801702f8; lc_native_story_flag_set = 0x8017049c; SECTIONS { .text 0x80200000 : { *(.text) } }\n');
run('mips-kmc-elf-ld.exe',['-EB','-m','elf32bmip','-T','flags.ld','-o','flags.elf','flags.o']);
const elf=fs.readFileSync(path.join(scratch,'flags.elf')),symbols={};
const sh=elf.readUInt32BE(32),n=elf.readUInt16BE(48),section=i=>sh+i*40;
for(let i=0;i<n;i++){let a=section(i);if(elf.readUInt32BE(a+4)!==2)continue;const stringsAt=section(elf.readUInt32BE(a+24)),strings=elf.subarray(elf.readUInt32BE(stringsAt+16),elf.readUInt32BE(stringsAt+16)+elf.readUInt32BE(stringsAt+20));for(let at=elf.readUInt32BE(a+16),end=at+elf.readUInt32BE(a+20);at<end;at+=16){const nameAt=elf.readUInt32BE(at),name=strings.toString('ascii',nameAt,strings.indexOf(0,nameAt));symbols[name]=elf.readUInt32BE(at+4);}}
let textIndex=Array.from({length:n},(_,i)=>i).find(i=>elf.readUInt32BE(section(i)+12)===codeAddress);
const textHeader=section(textIndex);
let code=elf.subarray(elf.readUInt32BE(textHeader+16),elf.readUInt32BE(textHeader+16)+elf.readUInt32BE(textHeader+20)),get=symbols.lc_event_story_flag_get,set=symbols.lc_event_story_flag_set;
let exported=process.argv[2];if(exported){const z=fs.readFileSync(exported);if(z.readUInt32BE(0)===0x37804012)z.swap16();assert.equal(z.readUInt32BE(0x5dc88),6);const data=z.readUInt32BE(0x5dc8c),size=z.readUInt32BE(0x5dc90),m=z.subarray(data+size-32,data+size);assert.equal(m.toString('ascii',0,8),'LCFLAGS1');assert.equal(m.readUInt32BE(20),256);assert.equal(m.readUInt32BE(24),512);global.exportedBank=m.readUInt32BE(8);get=z.readUInt32BE(0x650e0);set=z.readUInt32BE(0x65150);assert.equal(get,m.readUInt32BE(12));assert.equal(set,m.readUInt32BE(16));global.exportedCode=z.subarray(0x5de00,0x5de00+z.readUInt32BE(0x5dcd4));}
const actualBank=global.exportedBank||bank;
function machine(){const m=new FloatMips();m.load(0x80163910,stock.subarray(0x39810,0x66e10));m.load(global.exportedCode?0x80187f00:codeAddress,global.exportedCode||code);m.set('sp',0x807ff000);m.load(actualBank-16,Buffer.alloc(96,0xa5));m.load(actualBank,Buffer.alloc(64));return m;}
const bytes=(m,a,n)=>Buffer.from(m.ram.subarray(m.phys(a),m.phys(a)+n));
function call(m,entry,id,value=0){m.set('a0',id);m.set('a1',0x789abcde);m.set('a2',value);m.call(entry);return m.get('v0');}
let cases=0;
for(let id=0;id<256;id++)for(const v of [0,1,2,0xffffffff]){const m=machine(),oracle=machine(),initial=Buffer.alloc(128,0x5a);m.load(0x80193686,initial);oracle.load(0x80193686,initial);call(m,set,id,v);call(oracle,0x8017049c,id,v);assert.deepEqual(bytes(m,0x80193686,128),bytes(oracle,0x80193686,128));for(const query of [id,-id])assert.equal(call(m,get,query),call(oracle,0x801702f8,query));assert.deepEqual(bytes(m,actualBank,64),Buffer.alloc(64));cases++;}
for(let id=256;id<768;id++)for(const v of [1,0,2,0xffffffff]){const m=machine(),before=bytes(m,0x80193686,128);call(m,set,id,v);const expected=Buffer.alloc(64);if(v===1)expected[(id-256)>>3]=1<<((id-256)&7);assert.deepEqual(bytes(m,actualBank,64),expected);assert.equal(call(m,get,id),v===1?1:0);assert.equal(call(m,get,-id),v===1?0:1);assert.deepEqual(bytes(m,actualBank-16,16),Buffer.alloc(16,0xa5));assert.deepEqual(bytes(m,actualBank+64,16),Buffer.alloc(16,0xa5));assert.deepEqual(bytes(m,0x80193686,128),before);cases++;}
for(const id of [-32768,-768,-1,768,32767,0xffffffff]){const m=machine(),before=bytes(m,actualBank-16,96);call(m,set,id,1);assert.deepEqual(bytes(m,actualBank-16,96),before);if(id!==-1&&id!==0xffffffff)assert.equal(call(m,get,id),0);}
// The original opcode12 reset bypasses the service table and clears a byte-
// bounded native range. Execute the whole VM entry/epilogue with its real frame.
{
 const m=machine(),program=0x80700000,sp=0x807ff000;
 m.load(actualBank,Buffer.alloc(64,0xa5));m.load(0x80193686,Buffer.alloc(32,0xff));
 m.w16(sp-144+80,0xffc8);m.w16(0x80197b04,0);m.load(program,Buffer.from([0x12,3]));
 m.set('a0',program);m.call(0x80170954);
 const expected=Buffer.alloc(32,0xff);for(let id=200;id<255;id++)expected[id>>3]&=~(1<<(id&7));
 assert.deepEqual(bytes(m,0x80193686,32),expected);assert.deepEqual(bytes(m,actualBank,64),Buffer.alloc(64,0xa5));
 assert.equal(m.get('v0'),2);assert.equal(m.get('sp'),sp);
}
// Direct native callers retain their separate high native-bit namespace.
for(const id of [528,529,530]){const m=machine();call(m,0x8017049c,id,1);assert.equal(call(m,0x801702f8,id),1);assert.deepEqual(bytes(m,actualBank,64),Buffer.alloc(64));}
console.log(`PASS ${cases} compiled story flag cases: native delegation, all512 custom bits, inverse tests, write semantics, invalid bounds, canaries, native opcode12 reset and native menu namespace.`);
