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
const routes=raw(0x315736),directory=raw(0x3b6cd0),keys=new Set();
for(let id=1;id<165;id++)for(let flags=0;flags<4;flags++){
 const handle=routes.readUInt16BE(routes.readUInt16BE(0)+id*8+flags*2)&4095;
 keys.add(directory.readUInt32BE((handle-1)*4));
}
let realCases=0;
for(const key of keys){
 const members=raw(key),bank=decode(members.readUInt32BE(4));const count=bank.readUInt32BE(0)>>>2; // Original205E30/205E3C/205E44 publishes this count.
 assert.deepEqual(run(wide,bank,count),run(original,bank,count),'unchanged descriptor '+key.toString(16));realCases++;
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
const report={offset,address:start,size,widthsAddress:0x801ceee0,retailSha256:hash(original),wideSha256:hash(wide),realCases,syntheticCases};
if(process.argv[3])fs.writeFileSync(process.argv[3],JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report));
