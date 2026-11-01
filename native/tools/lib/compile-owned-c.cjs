'use strict';
// Qualify a maintained C adaptation at fixed original code/table addresses.
// Only ELF section alignment changes; instructions are entirely compiler output.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');
const {sections}=require('../build-accepted-effects.cjs');
const {digest}=require('../decomp-evidence.cjs');
module.exports=function compileOwned(native,g,source,work,bindings={}) {
 const tools=path.join(native,'.local/n64-toolchain'), pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json'))).tools;
 for(const [name,hash] of Object.entries(pins))assert.equal(digest(fs.readFileSync(path.join(tools,name))).toLowerCase(),hash);
 const dir=path.join(work,g.name);fs.mkdirSync(dir,{recursive:true});
 fs.writeFileSync(path.join(dir,'source.c'),source);
 for(const h of g.headers){const data=fs.readFileSync(path.join(native,h.file));assert.equal(digest(data),h.sha256);const f=path.join(dir,h.path);fs.mkdirSync(path.dirname(f),{recursive:true});fs.writeFileSync(f,data);}
 const env={...process.env};for(const k of ['C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH','DEPENDENCIES_OUTPUT','SUNPRO_DEPENDENCIES'])delete env[k];
 const run=(tool,args)=>execFileSync(path.join(tools,tool),args,{cwd:dir,env,windowsHide:true,timeout:60000,stdio:'pipe'});
 run('cpp.exe',['-P','-undef','-nostdinc','-D__mips=3','-D__mips__','-D_MIPSEB','-D__MIPSEB__','-I',dir,'-I',path.join(dir,'include'),'source.c','source.i']);
 run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','source.i','-o','source.s']);
 run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','source.o','source.s']);
 const object=fs.readFileSync(path.join(dir,'source.o')),raw=sections(object);
 for(const name of Object.keys(g.sections)){assert.ok(raw[name]);object.writeUInt32BE(4,raw[name].header+32);}
 fs.writeFileSync(path.join(dir,'source.o'),object);
 const symbols={...g.bindings,...bindings};
 fs.writeFileSync(path.join(dir,'source.ld'),Object.entries(symbols).map(([k,v])=>`${k} = 0x${v.toString(16)};`).join('\n')+
  '\nSECTIONS { '+Object.entries(g.sections).map(([k,v])=>`${k} 0x${v.toString(16)} : { *(${k}) }`).join(' ')+' }');
 run('mips-kmc-elf-ld.exe',['-EB','-m','elf32bmip','-T','source.ld','-o','source.elf','source.o']);
 const linked=sections(fs.readFileSync(path.join(dir,'source.elf')));
 for(const name of ['.data','.bss','.rdata','.lit4','.lit8'])assert.ok(!linked[name]?.size,'Unexpected section '+name);
 const refs=[], pending=new Map(),rel=raw['.rel.text']?.bytes,sy=raw['.symtab'].bytes,st=raw['.strtab'].bytes,text=raw['.text'].bytes;
 for(let i=0;rel&&i<rel.length;i+=8){const at=rel.readUInt32BE(i),info=rel.readUInt32BE(i+4),type=info&255,sym=info>>>8;
  if(sy.readUInt16BE(sym*16+14)!==0||![5,6].includes(type))continue;
  if(type===5){if(!pending.has(sym))pending.set(sym,[]);pending.get(sym).push(at);continue;}
  const highs=pending.get(sym);assert.ok(highs?.length);pending.delete(sym);const str=sy.readUInt32BE(sym*16),name=st.toString('utf8',str,st.indexOf(0,str));
  const low=text.readUInt32BE(at),lo=low>>>26===13?low&65535:text.readInt16BE(at+2);
  for(const high of highs){const addend=(((text.readUInt32BE(high)&65535)<<16)+lo)|0;refs.push({symbol:name,high,low:at,addend});}
 }
 assert.equal(pending.size,0);
 return {linked,refs,symbols};
};
