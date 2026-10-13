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
const work=fs.mkdtempSync(path.join(native,'build/save-compression-'));
for(const name of ['save-compression.c','save_compression.h','save-format.c','save_format.h','save_checksums.h','save-image.c'])
 fs.copyFileSync(path.join(native,'game/classes',name),path.join(work,name));
fs.appendFileSync(path.join(work,'save-compression.c'),'\n'+fs.readFileSync(path.join(work,'save-format.c'),'utf8')+'\n'+fs.readFileSync(path.join(work,'save-image.c'),'utf8'));
const run=(name,args)=>cp.execFileSync(path.join(toolchain,name),args,{cwd:work,encoding:'utf8'});
run('cpp.exe',['-P','-undef','-nostdinc','save-compression.c','codec.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','codec.i','-o','codec.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','codec.o','codec.s']);
fs.writeFileSync(path.join(work,'codec.ld'),'SECTIONS { .text 0x80280000 : { *(.text) } }\n');
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
const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(0x80280000,fs.readFileSync(path.join(work,'codec.bin')));
const source=0x80300000,packed=0x80310000,decoded=0x80320000,scratch=0x80360000,sp=0x803f0000;
const read=(at,size)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+size));
function call(name,...args){
 m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));
 m.call(entries[name]||name,0x80000100,150000000);assert.equal(m.get('sp'),sp);return m.get('v0');
}
function crc(data){let c=0xffffffff;for(const b of data){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return(~c)>>>0;}
assert.equal(crc(Buffer.from('123456789')),0xcbf43926);
let originalOracle=false;
if(process.env.LORDLY_TEST_ROM){
 const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
 assert.equal(crypto.createHash('sha256').update(rom).digest('hex'),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
 m.load(0x80070c00,rom.subarray(0x1000,0x2f000));originalOracle=true;
}
let roundTrips=0,rejections=0,oracles=0;const population=[];
function check(data,label){
 const capacity=32800,workBytes=2*(4096+data.length);
 m.load(source,data);m.load(packed-16,Buffer.alloc(capacity+32,0xa6));
 m.load(scratch-16,Buffer.alloc(workBytes+32,0xb7));
 const length=call('lc_save_frame_encode',source,data.length,packed,capacity,scratch);
 assert(length>=32&&length<=capacity,label);
 assert.deepEqual(read(source,data.length),data);
 assert.deepEqual(read(packed-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(packed+capacity,16),Buffer.alloc(16,0xa6));
 assert.deepEqual(read(scratch-16,16),Buffer.alloc(16,0xb7));assert.deepEqual(read(scratch+workBytes,16),Buffer.alloc(16,0xb7));
 const frame=read(packed,length);assert.equal(frame.readUInt32BE(16),crc(data));
 assert.equal(frame.readUInt32BE(20),crc(frame.subarray(32)));assert.equal(frame.readUInt32BE(28),crc(frame.subarray(0,28)));
 m.load(decoded-16,Buffer.alloc(data.length+32,0xc8));
 assert.equal(call('lc_save_frame_decode',decoded,data.length,packed,length),data.length,label);
 assert.deepEqual(read(decoded,data.length),data,label);
 assert.deepEqual(read(decoded-16,16),Buffer.alloc(16,0xc8));assert.deepEqual(read(decoded+data.length,16),Buffer.alloc(16,0xc8));
 assert.equal(call('lc_save_frame_decode',decoded,data.length-1,packed,length),0);rejections++;
 assert.equal(call('lc_save_frame_encode',source,data.length,packed,length-1,scratch),0);rejections++;
 m.load(packed,frame);
 for(const n of [0,1,31,32,length-1]){assert.equal(call('lc_save_frame_decode',decoded,data.length,packed,n),0);rejections++;}
 for(const at of [0,4,8,12,16,20,24,28,length-1]){
  m.load(packed,frame);m.w8(packed+at,m.r8(packed+at)^1);
  assert.equal(call('lc_save_frame_decode',decoded,data.length,packed,length),0);rejections++;
 }
 m.load(packed,frame);
 if(originalOracle&&frame.readUInt32BE(24)===1){
  const prefix=Buffer.alloc(4);prefix.writeUInt32BE(data.length);
  m.load(0x80340000,Buffer.concat([prefix,frame.subarray(32)]));
  call(0x8007a110,decoded,0x80340000);assert.deepEqual(read(decoded,data.length),data);oracles++;
 }
 roundTrips++;return length;
}
let state=0x12345678;const next=()=>{state^=state<<13;state^=state>>>17;state^=state<<5;return state>>>0;};
for(const size of [1,2,3,31,63,64,65,258,260,2048,16384,32768]){
 for(const kind of ['zero','ff','random','repeat']){
  const data=Buffer.alloc(size);for(let i=0;i<size;i++)data[i]=kind==='zero'?0:kind==='ff'?255:kind==='random'?next()&255:i%257;
  check(data,`${size}-${kind}`);
 }
}
const fixtureDir=process.argv[2];
if(fixtureDir)for(const name of fs.readdirSync(fixtureDir).filter(n=>n.endsWith('.bin'))){
 const data=fs.readFileSync(path.join(fixtureDir,name));const length=check(data,name);
 population.push({name,logicalBytes:data.length,frameBytes:length,physicalProjection:16+12512+length+16,spareBytes:32768-(16+12512+length+16)});
 const budget=32768-16-12512-16;
 m.load(packed-16,Buffer.alloc(budget+32,0xa6));
 const bounded=call('lc_save_frame_encode',source,data.length,packed,budget,scratch);
 assert.equal(bounded,length<=budget?length:0,name);
 assert.deepEqual(read(source,data.length),data);
 assert.deepEqual(read(packed-16,16),Buffer.alloc(16,0xa6));assert.deepEqual(read(packed+budget,16),Buffer.alloc(16,0xa6));
 if(name.startsWith('populated')||name.startsWith('soldier-heavy'))assert(bounded>0,name+' must fit');
 else assert.equal(bounded,0,name+' must refuse overflow');
}
for(const stream of [Buffer.from([3]),Buffer.from([0x80,0]),Buffer.from([0x40]),Buffer.from([0x20])]){
 m.load(packed,stream);assert.equal(call('lc_save_lzss_decode',decoded,1,packed,stream.length),0);rejections++;
}
const report={roundTrips,rejections,originalDecoderComparisons:oracles,codeBytes:fs.statSync(path.join(work,'codec.bin')).size,population,
 scope:'Compiled N64 codec only. No save-menu/SRAM commit path or expanded-squad runtime tested.'};
const imageAt=0x80340000;
m.load(imageAt,Buffer.alloc(32768,0xa6));
for(const slot of [0,1,14,15]){
 const at=slot===14?0:slot===15?0x30b0:16+slot*0x1850,size=slot===14?16:slot===15?0x4ae8:0x1850;
 m.load(imageAt+at,Buffer.alloc(size,slot+5));assert.equal(call('lc_save_record_seal',imageAt+at,size,slot,0),1);
}
assert.equal(call('lc_save_expand_image',imageAt,32768),1);
const before=read(imageAt,32768);let commits=0,refusals=0;
if(fixtureDir)for(const name of fs.readdirSync(fixtureDir).filter(n=>n.endsWith('.bin'))){
 const data=fs.readFileSync(path.join(fixtureDir,name)),size=data.length,workBytes=20224+size+2*(4096+size);
 if(size!==22820)continue; // Other proposed layouts exercise only the generic codec above.
 m.load(source,data);assert.equal(call('lc_save_record_seal',source,size,15,2),1);
 const signed=read(source,size);m.load(imageAt,before);
 for(const available of [0,workBytes-1]){
  assert.equal(call('lc_save_commit_compressed',imageAt,32768,source,size,scratch,available),0);
  assert.deepEqual(read(imageAt,32768),before);refusals++;
 }
 const result=call('lc_save_commit_compressed',imageAt,32768,source,size,scratch,workBytes);
 if(name.startsWith('populated')||name.startsWith('soldier-heavy')){
  assert.equal(result,1,name);commits++;
  assert.deepEqual(read(imageAt,0x30f0),before.subarray(0,0x30f0));
  assert.equal(call('lc_save_image_version',imageAt,32768),2);
  assert.equal(call('lc_save_read_compressed',imageAt,32768,decoded,32768),size);
  assert.deepEqual(read(decoded,size),signed);assert.deepEqual(read(source,size),signed);
  const committed=read(imageAt,32768);
  m.w8(imageAt+0x30f0+32,m.r8(imageAt+0x30f0+32)^1);
  assert.equal(call('lc_save_read_compressed',imageAt,32768,decoded,32768),0);refusals++;
  m.load(imageAt,committed);m.w8(imageAt+0x7fff,m.r8(imageAt+0x7fff)^1);
  const damaged=read(imageAt,32768);
  assert.equal(call('lc_save_image_version',imageAt,32768),0xffffffff);
  assert.equal(call('lc_save_commit_compressed',imageAt,32768,source,size,scratch,workBytes),0);
  assert.deepEqual(read(imageAt,32768),damaged);refusals++;
 }else{
  assert.equal(result,0,name);assert.deepEqual(read(imageAt,32768),before);refusals++;
 }
}
report.transactionalCommits=commits;report.transactionalRefusals=refusals;
report.scope=commits ? 'Compiled N64 codec plus transactional 32-KiB image replacement; normal slots/options preserved. No save-menu or expanded-squad gameplay proof.' :
 'Compiled N64 codec only. These proposed layouts are not accepted by the installed save-image path; no expanded-squad gameplay proof.';
fs.writeFileSync(path.join(work,'report.json'),JSON.stringify(report,null,2)+'\n');
for(const mode of ['populated','soldier-heavy','opaque-stress','incompressible']){
 const rows=population.filter(r=>r.name.startsWith(mode));if(rows.length)console.log(JSON.stringify({mode,cases:rows.length,minSpare:Math.min(...rows.map(r=>r.spareBytes)),maxFrame:Math.max(...rows.map(r=>r.frameBytes))}));
}
console.log(JSON.stringify({...report,population:population.length,work}));
