'use strict';
// Compile maintained marker scans, restore the exact original span, and compare
// execution at its actual overlay address. This is not a whole-game heap test.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),vm=require('node:vm');
const crypto=require('node:crypto'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const native=path.resolve(__dirname,'..'),tools=process.env.LORDLY_N64_TOOLCHAIN||path.join(native,'.local/n64-toolchain');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json')));
for(const [file,expected] of Object.entries(pins.tools))assert.equal(hash(fs.readFileSync(path.join(tools,file))),expected,file);
const work=fs.mkdtempSync(path.join(native,'build/body-markers-'));
const start=0x801c2b48,end=0x801c2c24,offset=0x205fd8,size=end-start;
const source=fs.readFileSync(path.join(native,'game/art/body-markers.s'),'utf8');
function compile(wide){
 const stem='markers'+wide;
 fs.writeFileSync(path.join(work,stem+'.s'),`.set noat\n.set noreorder\n.equ LC_WIDE,${wide}\n.section .owner,"ax"\n.balign 4\n`+source);
 fs.writeFileSync(path.join(work,stem+'.ld'),`lc_pose_widths = 0x801ceee0;\nSECTIONS { .owner 0x${start.toString(16)} : { *(.owner) } }\n`);
 const run=(exe,args)=>cp.execFileSync(path.join(tools,exe),args,{cwd:work,encoding:'utf8'});
 run('mips-kmc-elf-as.exe',['--vr4300mul-off','-G','0','-mips3','-mabi=32','-EB','-o',stem+'.o',stem+'.s']);
 run('mips-kmc-elf-ld.exe',['-EB','-T',stem+'.ld','-o',stem+'.elf',stem+'.o']);
 run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.text','.data','.bss','.reginfo','.mdebug','.pdr','.comment'].map(s=>'--remove-section='+s),stem+'.elf',stem+'.bin']);
 const bytes=fs.readFileSync(path.join(work,stem+'.bin'));assert.equal(bytes.length,size);return bytes;
}
const original=compile(0),wide=compile(1),rom=fs.readFileSync(process.argv[2]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();assert.deepEqual(original,rom.subarray(offset,offset+size));
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(native,'../art.js'),'utf8'));
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decode=k=>Buffer.from(OB64.art.readCompressedResource(rom,k).decoded);
const widths=[1,3,3,2,2,2,1,1,1,1,1,1,4,3,3,2,3,3,3,3,3,4];
const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);const body=0x80200010,poses=0x80210000;
cpu.load(0x801bdb70,rom.subarray(0x201000,0x213a00));cpu.load(0x801ceee0,Buffer.from(widths));
function run(code,bank,count){
 const record=Buffer.alloc(0xd0,0xa7);record.writeUInt32BE(poses,16+0x44);record.writeUInt32BE(count,16+0x4c);
 cpu.load(body-16,record);cpu.load(poses,bank);cpu.load(start,code);
 for(const reg of ['s0','s1','s2','s3','s4','s5','s6','s7','fp','sp'])cpu.set(reg,reg==='s3'?body:0x80370000+Number(reg==='sp'?20:reg==='fp'?21:reg.slice(1))*32);
 const saved=['s0','s1','s2','s3','s4','s5','s6','s7','fp','sp'].map(r=>[r,cpu.get(r)]);
 cpu.call(start,end,100000);
 for(const [reg,value] of saved)assert.equal(cpu.get(reg),value,reg);
 const actual=Buffer.from(cpu.ram.subarray(cpu.phys(body-16),cpu.phys(body-16)+record.length));
 const result=[0xa8,0xaa,0xac,0xae].map(p=>cpu.r16(body+p));record.copy(actual,16+0xa8,16+0xa8,16+0xb0);assert.deepEqual(actual,record,'writes remain inside marker fields');
 assert.deepEqual(Buffer.from(cpu.ram.subarray(cpu.phys(poses),cpu.phys(poses)+bank.length)),bank,'pose bytes remain intact');return result;
}
// Optional freshly exported ROM: run the entire allocation owner, including
// setup, marker traversal and cleanup. Resource services provide the input bank;
// widths, control flow, marker writes and return behavior execute as ROM MIPS.
let full, baselineFull;
function fullOwner(image) {
 const m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(0x801bdb70,image.subarray(0x201000,0x213a00));m.load(0x801ceee0,Buffer.from(widths));
 const body=0x80200010,poses=0x80210000,meta=0x80220000,fields=0x80221000,desc=0x80222000,table=0x80223000;
 m.w32(0x801d0690,0x80240000);
 const policies=image.subarray(0x27c5100,0x27c5108).toString()==='LC05VPOL';
 let variants=[];
 m.trap(0x801be138,c=>c.set('v0',1));m.trap(0x801be18c,c=>c.set('v0',0));
 for(const p of [0x801be0dc,0x8016f5e0])m.trap(p,c=>{variants.push(c.get('a0'));c.set('v0',c.get('a0'));});
 let heap;
 for(const p of [0x80071c04,0x80070f30])m.trap(p,c=>{const at=heap;heap+=(c.get('a0')+15)&~15;c.set('v0',at);});
 m.trap(0x8009daf4,c=>c.set('v0',[0x3b6cd0,image.readUInt32BE(0x2060e8)].includes(c.get('a0'))?4:16));
 m.trap(0x8009dbb8,c=>{if([0x3b6cd0,image.readUInt32BE(0x2060e8)].includes(c.get('a1')))m.w32(c.get('a0'),1);else [1,2,3,4].forEach((v,i)=>m.w32(c.get('a0')+i*4,v));});
 for(const p of [0x8009dd38,0x8009df48])m.trap(p,c=>c.set('v0',({1:meta,2:poses,3:fields,4:table})[c.get('a0')]));
 m.trap(0x800712c4,()=>{});m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
 m.trap(0x80093060,c=>m.load(c.get('a1'),Buffer.from(m.ram.subarray(m.phys(c.get('a0')),m.phys(c.get('a0'))+c.get('a2')))));
 const fn=(bank,count,id=2,item=0,policy=511)=>{
  variants=[];if(policies)m.w16(0x80240000-image.readUInt16BE(0x207a6c)+id*2,policy);
  // The real constructor derives count from directory[0], so fixtures use a
  // valid directory and cannot fake a conflicting count beside it.
  assert.equal(bank.readUInt32BE(0)>>>2,count);
  m.load(body-16,Buffer.alloc(208,0xa7));m.load(poses,bank);m.load(fields,Buffer.alloc(128));m.w32(meta,4);heap=0x80300000;
  m.set('sp',0x803ffe00);[body,id,id,0].forEach((v,i)=>m.set('a'+i,v));m.w32(0x803ffe10,0);m.w32(0x803ffe14,item);
  m.call(0x801c27f8,0x80000100,100000);assert.equal(m.get('v0'),body);assert.equal(m.get('sp'),0x803ffe00);
  assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(body-16),m.phys(body))),Buffer.alloc(16,0xa7));
  assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(body+176),m.phys(body+192))),Buffer.alloc(16,0xa7));
  assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(poses),m.phys(poses)+bank.length)),bank);
  fn.variants=variants;
  return [0xa8,0xaa,0xac,0xae].map(p=>m.r16(body+p));
 };
 return fn;
}
if(process.argv[4]) {
 const image=fs.readFileSync(process.argv[4]);if(image.readUInt32BE(0)===0x37804012)image.swap16();
 full=fullOwner(image);baselineFull=fullOwner(rom);
}
const routes=raw(0x315736),directory=raw(0x3b6cd0),keys=new Set();
for(let id=1;id<165;id++)for(let flags=0;flags<4;flags++){
 const handle=routes.readUInt16BE(routes.readUInt16BE(0)+id*8+flags*2)&4095;
 keys.add(directory.readUInt32BE((handle-1)*4));
}
let realCases=0;
for(const key of keys){
 const members=raw(key),bank=decode(members.readUInt32BE(4));const count=bank.readUInt32BE(0)>>>2; // Original205E30/205E3C/205E44 publishes this count.
 assert.deepEqual(run(wide,bank,count),run(original,bank,count),'unchanged descriptor '+key.toString(16));if(full)assert.deepEqual(full(bank,count),run(original,bank,count));realCases++;
}
function bank(first,second,count=100){
 const programs=Array.from({length:count},()=>Buffer.from([0]));if(count>38)programs[38]=Buffer.from([first.length,...first.flat()]);if(count>88)programs[88]=Buffer.from([second.length,...second.flat()]);
 const bytes=Buffer.alloc(count*4);let cursor=count*4;programs.forEach((p,i)=>{bytes.writeUInt32BE(cursor,i*4);cursor+=p.length;});return Buffer.concat([bytes,...programs]);
}
function lastTwo(records,useWide){let last=65535,previous=65535;for(const r of records)if(r[0]===1||(useWide&&r[0]===21)){previous=last;last=r[1]+(r[0]===21?256*r[2]:0);}return [last,previous];}
const fixtures=[[],[[1,17,6]],[[21,44,1,2]],[[1,7,0],[3,4],[21,255,255,2],[4,0],[21,0,1,2]],widths.map((n,i)=>[i,...Array(n-1).fill(i)]),Array.from({length:255},(_,i)=>i%2?[1,i,2]:[21,i,2,2])];
let syntheticCases=0;
for(const first of fixtures)for(const second of fixtures)for(const count of [50,87,88,89,100]){
 const bytes=bank(first,second);
 for(const [code,mode] of [[original,false],[wide,true]]){
  const expected=[...lastTwo(first,mode),...(count<(mode?89:88)?[65535,65535]:lastTwo(second,mode))];
  assert.deepEqual(run(code,bytes,count),expected);syntheticCases++;
 }
}
for(const first of fixtures)for(const count of [0,1,38,39,88]){assert.deepEqual(run(wide,bank(first,[],count),count),[...(count<39?[65535,65535]:lastTwo(first,true)),65535,65535]);syntheticCases++;}
let fullCases=0;
if(full)for(const first of fixtures)for(const second of fixtures)for(const count of [1,38,39,50,87,88,89,100]) {
 const bytes=bank(first,second,count),expected=[...(count>38?lastTwo(first,true):[65535,65535]),...(count>88?lastTwo(second,true):[65535,65535])];
 assert.deepEqual(full(bytes,count),expected);fullCases++;
}
let equipmentCases=0;
if(full)for(let id=0;id<256;id++)for(const item of [0,277]) {
 const bytes=bank([],[],100);
 baselineFull(bytes,100,id,item);const expected=baselineFull.variants;
 full(bytes,100,id,item,expected[0]);assert.deepEqual(full.variants,expected,`equipment class ${id} item ${item}`);equipmentCases++;
}
const report={fullCases,equipmentCases,offset,address:start,size,widthsAddress:0x801ceee0,retailSha256:hash(original),wideSha256:hash(wide),realCases,syntheticCases};
if(process.argv[3])fs.writeFileSync(process.argv[3],JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report));
