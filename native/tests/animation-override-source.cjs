'use strict';
// Compile maintained sources and execute the existing OBSO-v3 patch contract.
// Cache invalidation and PI DMA are controlled services, not console emulation.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process');
const crypto=require('node:crypto'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const native=path.resolve(__dirname,'..');
const tools=process.env.LORDLY_N64_TOOLCHAIN||path.join(native,'.local/n64-toolchain');
const config=JSON.parse(fs.readFileSync(path.join(native,'game/art/animation-overrides.json')));
const pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json')));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
for(const [file,expected] of Object.entries(pins.tools))assert.equal(hash(fs.readFileSync(path.join(tools,file))),expected,file);
fs.mkdirSync(path.join(native,'build'),{recursive:true});
const work=fs.mkdtempSync(path.join(native,'build/animation-override-source-'));
const run=(exe,args)=>cp.execFileSync(path.join(tools,exe),args,{cwd:work,encoding:'utf8'});
function assemble(name,install=1){
  const spec=config.sources[name],stem=name+install;
  const source=fs.readFileSync(path.join(native,spec.file),'utf8');
  fs.writeFileSync(path.join(work,stem+'.s'),`.set noat\n.set noreorder\n.equ LC_INSTALLED, ${install}\n.section .owner,"ax"\n.balign 4\n`+source);
  const bindings=Object.entries(spec.symbols).map(([key,value])=>`${key} = 0x${value.toString(16)};`).join('\n');
  fs.writeFileSync(path.join(work,stem+'.ld'),bindings+`\nSECTIONS { .owner 0x${spec.address.toString(16)} : { *(.owner) } }\n`);
  run('mips-kmc-elf-as.exe',['--vr4300mul-off','-G','0','-mips3','-mabi=32','-EB','-o',stem+'.o',stem+'.s']);
  run('mips-kmc-elf-ld.exe',['-EB','-T',stem+'.ld','-o',stem+'.elf',stem+'.o']);
  run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.text','.data','.bss','.reginfo','.mdebug','.pdr','.comment'].map(s=>'--remove-section='+s),stem+'.elf',stem+'.bin']);
  const bytes=fs.readFileSync(path.join(work,stem+'.bin'));
  if(spec.sha256){assert.equal(bytes.length,spec.size,name+' size');assert.equal(hash(bytes),spec.sha256,name+' producer equivalence');}
  return bytes;
}
const compiled=Object.fromEntries(Object.keys(config.sources).map(name=>[name,assemble(name)]));
for(const name of ['hook','return','carriers']){
  const retail=assemble(name,0),active=compiled[name];
  const sites=config.sites.filter(s=>name==='carriers'?s.name.startsWith('carrier'):s.name===name);
  assert.equal(active.length,sites.reduce((n,s)=>n+s.size,0));assert.equal(retail.length,active.length);
  let at=0;for(const site of sites){
    assert.equal(hash(retail.subarray(at,at+site.size)),site.retailSha256,site.name+' restore');
    assert.equal(hash(active.subarray(at,at+site.size)),site.activeSha256,site.name+' install');at+=site.size;
  }
}
const L=config.layout,moduleBytes=Buffer.alloc(L.moduleSize);
moduleBytes.write('OBSO');[3,0x40,8,128,0,0].forEach((v,i)=>moduleBytes.writeUInt16BE(v,4+i*2));
[0x800,0x40,0x400,1].forEach((v,i)=>moduleBytes.writeUInt32BE(v,0x10+i*4));
compiled.entry.copy(moduleBytes,0x40);compiled['mode-return'].copy(moduleBytes,0x180);compiled.lookup.copy(moduleBytes,0x200);
assert.equal(hash(moduleBytes),config.versions[2].moduleTemplateSha256,'assembled v3 immutable template');
assert.equal(hash(compiled.bootstrap),config.versions[2].bootstrapSha256,'assembled bootstrap');

const rows=[
  [2,2,5,2,0,43,44,1], // body-specific row takes precedence over wildcard
  [2,2,5,0,0,41,42,0],
  [200,200,30,3,40,44,45,2], // new IDs, exact original and exact body flags
  [200,200,30,1,41,46,47,0],
];
// Fill the complete128-record table and include a match at its final slot.
while(rows.length<127)rows.push([254,253,rows.length,0,0,50,51,0]);
rows.push([1,99,200,0,0,52,53,0]);
rows.forEach((r,i)=>moduleBytes.set(r,0x400+i*8));moduleBytes.writeUInt16BE(rows.length,12);
crypto.createHash('sha256').update(moduleBytes.subarray(0x400)).digest().copy(moduleBytes,0x20);
const cpu=new Mips(),actor=0x80280000,stack=0x80060000;
cpu.load(L.moduleRam,moduleBytes);cpu.w32(0x801CE8C0,0x12345678);
function expected(op,source,owner,action,original,mode,flags,present=true){
  if(!present||![6,7,11].includes(op)||mode>2)return original;
  for(const r of rows)if(r[0]===source&&r[1]===owner&&r[2]===action&&(!(r[3]&1)||r[4]===original)&&(!(r[3]&2)||r[7]===flags))return r[mode===2?6:5];
  return original;
}
function setup(c,op,source,owner,action,original,mode,flags,present=true){
  c.set('sp',stack);c.set('a0',op);c.set('a1',0x182);c.set('a2',present?actor:0);c.set('a3',0x91);
  c.w32(actor+0x48,source);c.w32(actor+0x4C,owner);c.w32(actor+0x7C,action);
  c.w32(actor+0x40,((flags>>1)<<10)|((flags&1)<<8));
  c.load(stack,Buffer.alloc(48,0x59));
  const slot=stack+(op===11?0x1B:0x13);
  if(op===11)c.w8(stack+0x13,action);
  c.w8(slot,original);c.w8(slot-1,mode);return slot;
}
let cases=0;
for(const op of [6,7,11,1])for(const source of [2,200,1,254])for(const owner of [2,200,99])
for(const action of [5,30,200])for(const original of [0,40,41,255])for(const mode of [0,1,2,3])for(let flags=0;flags<4;flags++){
  const slot=setup(cpu,op,source,owner,action,original,mode,flags);
  const before=Buffer.from(cpu.ram.slice(cpu.phys(stack),cpu.phys(stack)+48));
  const want=expected(op,source,owner,action,original,mode,flags);before[slot-stack]=want;
  cpu.call(L.moduleRam+0x40,0x801D98FC);
  assert.equal(cpu.r8(slot),want,`${op}/${source}/${owner}/${action}/${original}/${mode}/${flags}`);
  assert.deepEqual(Buffer.from(cpu.ram.slice(cpu.phys(stack),cpu.phys(stack)+48)),before,'only outgoing selector byte changes');
  assert.equal(cpu.get('v0'),0x12345678,'displaced global read replay');cases++;
}
for(const op of [6,7,11]){
  const slot=setup(cpu,op,2,2,5,40,2,1,false);cpu.call(L.moduleRam+0x40,0x801D98FC);assert.equal(cpu.r8(slot),40);
}
for(const mode of [0,1,2,255])for(const original of [0,40,255,0xDEAD]){
  cpu.set('a3',mode);cpu.set('v0',original);cpu.call(L.moduleRam+0x180);assert.equal(cpu.get('v0'),(mode<<8)|(original&255));
}

// Exercise first-load and resident-fast-path entries with modeled PI register IO.
function bootstrap(selectorReturn,preloaded){
  const c=new Mips();c.load(L.bootstrapRam,compiled.bootstrap);c.w32(0x801CE8C0,0x12345678);
  if(preloaded)c.load(L.moduleRam,moduleBytes);
  const io={},base=0xA4600000;let dma=0,cache=0;
  const oldRead=c.r32.bind(c),oldWrite=c.w32.bind(c);
  c.r32=a=>((a>>>0)>=base&&(a>>>0)<base+32)?0:oldRead(a);
  c.w32=(a,v)=>{
    a>>>=0;if(a<base||a>=base+32)return oldWrite(a,v);
    io[a-base]=v;
    if(a-base===12){assert.equal(io[0],L.moduleRam&0x1FFFFFFF);assert.equal(io[4],0x10000000+L.moduleZ64);assert.equal(v,L.moduleSize-1);c.load(L.moduleRam,moduleBytes);dma++;}
  };
  for(const address of [0x800900C0,0x80090010])c.trap(address,x=>{assert.equal(x.get('a0'),L.moduleRam);assert.equal(x.get('a1'),L.moduleSize);x.set('v0',0xFFFFFFFF);cache++;});
  const slot=setup(c,6,2,2,5,40,2,1);c.set('a3',2);c.set('v0',40);
  c.call(L.bootstrapRam+(selectorReturn?12:0),selectorReturn?0x80000100:0x801D98FC);
  assert.equal(dma,preloaded?0:1);assert.equal(cache,preloaded?0:2);assert.equal(c.get('sp'),stack);
  assert.equal(c.get('a0'),6);assert.equal(c.get('a1'),0x182);assert.equal(c.get('a2'),actor);assert.equal(c.get('a3'),2);
  if(selectorReturn)assert.equal(c.get('v0'),0x228);else assert.equal(c.r8(slot),44);
}
for(const selectorReturn of [false,true])for(const preloaded of [false,true])bootstrap(selectorReturn,preloaded);
console.log(`Seven maintained sources match the accepted patch; ${cases} lookup cases, 16 mode-return cases, null actors and four bootstrap paths passed. Existing OBSO uses 8 MiB model memory; no full-game/hardware timing claim.`);

// Optional current-project snapshot: compare the desktop selector with the
// cartridge selector followed by the freshly assembled override lookup.
if(process.argv[2]) {
  const input=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
  const rom=fs.readFileSync(input.rom);
  if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
  else if(rom.readUInt32BE(0)===0x40123780)rom.swap32();
  const data=Buffer.from(input.resource,'hex');
  cpu.load(0x801BE308,rom.subarray(0x201798,0x201838));
  cpu.load(0x80187C14,Buffer.from(input.classes,'hex'));
  cpu.load(0x802C0000,data);
  cpu.w32(0x801D0694,0x802C0000+data.readUInt16BE(14));
  const nativeTable=Buffer.from(input.table,'hex');
  cpu.load(L.moduleRam+0x400,nativeTable);cpu.w16(L.moduleRam+12,input.count);
  for(const row of input.cases) {
    const [source,owner,action,mode,flags,want]=row;
    cpu.set('sp',stack);cpu.set('a0',source);cpu.set('a1',owner);cpu.set('a2',action);cpu.set('a3',mode);
    cpu.call(0x801BE308);
    const original=cpu.get('v0');
    const slot=setup(cpu,6,source,owner,action,original,mode,flags);
    cpu.call(L.moduleRam+0x40,0x801D98FC);
    assert.equal(cpu.r8(slot),want,'desktop preview '+row.join('/'));
  }
  console.log(`${input.cases.length} project action previews match the original selector and compiled override patch.`);
}
