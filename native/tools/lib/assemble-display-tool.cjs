'use strict';
// Compile maintained display sources exactly as the native export compiler.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {execFileSync}=require('node:child_process');
module.exports=function assembleDisplayTool(name,site,definition={}) {
 const native=path.resolve(__dirname,'../..'),src=path.join(native,'game/tools');
 const dir=fs.mkdtempSync(path.join(os.tmpdir(),'lc-display-'));
 try {
  let code=fs.readFileSync(path.join(src,site.source),'utf8');
  if(name==='chaos'&&site.name==='native-hook')code='.set CF_RETAIL,0\n'+code;
  if(site.name==='source-module')code+=name==='chaos'
   ? '\n.space 384,0\n'+fs.readFileSync(path.join(src,'chaos-title-list.s'),'utf8')+'\n.space 0x600,255\n'
   : '\n.space 512,0\n'+fs.readFileSync(path.join(src,name+'-templates.s'),'utf8');
  const symbols=name==='chaos'
   ? {CF_SUBMIT_TASK:0x80090140,CF_RETURN:0x80093b84,CF_BOOTSTRAP:0x800a4a78,CF_CONTINUATION:0x800a038c,CF_ENTRY:0x80420100,CF_INVALIDATE_ICACHE:0x800900c0,CF_INVALIDATE_DCACHE:0x80090010}
   : {TOOL_NATIVE_DRAW:definition.nativeDraw,TOOL_INVALIDATE_ICACHE:0x800900c0,TOOL_INVALIDATE_DCACHE:0x80090010,
      TOOL_BOOTSTRAP:definition.sites.find(s=>s.name==='source-loader').ram,TOOL_ENTRY:definition.sites.find(s=>s.name==='source-module').ram+0x100};
  if(name!=='chaos'&&site.name==='native-hook')code=`.set TOOL_RETAIL,0\n.set TOOL_ALIGNMENT,${name==='alignment'?1:0}\n`+code;
  fs.writeFileSync(path.join(dir,'owner.s'),'.set noat\n.set noreorder\n.section .owner,"ax"\n.balign 4\n'+code);
  fs.writeFileSync(path.join(dir,'owner.ld'),Object.entries(symbols).map(([k,v])=>`${k} = 0x${v.toString(16)};`).join('\n')+`\nSECTIONS { .owner 0x${site.ram.toString(16)} : { *(.owner) } }`);
  const run=(tool,args)=>execFileSync(path.join(native,'.local/n64-toolchain',tool+'.exe'),args,{cwd:dir,windowsHide:true,stdio:'pipe'});
  run('mips-kmc-elf-as',['--vr4300mul-off','-G','0','-mips3','-mabi=32','-EB','-o','owner.o','owner.s']);
  run('mips-kmc-elf-ld',['-EB','-T','owner.ld','-o','owner.elf','owner.o']);
  run('mips-kmc-elf-objcopy',['-O','binary',...['.text','.data','.bss','.reginfo','.mdebug','.pdr','.comment'].map(s=>'--remove-section='+s),'owner.elf','owner.bin']);
  const bytes=fs.readFileSync(path.join(dir,'owner.bin'));
  if(bytes.length!==site.size)throw Error(`${name}/${site.name}: ${bytes.length} bytes, budget ${site.size}`);
  return bytes;
 } finally {
  if(path.dirname(path.resolve(dir))!==path.resolve(os.tmpdir()))throw Error('Unexpected display assembly temporary directory');
  fs.rmSync(dir,{recursive:true,force:true});
 }
};
