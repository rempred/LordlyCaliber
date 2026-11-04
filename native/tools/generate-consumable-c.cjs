'use strict';
// Qualify the maintained C adaptation against pinned compiler inputs. Carries
// no compiled payload: normal ROM export compiles the same authenticated C.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');
const {digest}=require('./decomp-evidence.cjs');
const {sections}=require('./build-accepted-effects.cjs');
const native=path.resolve(__dirname,'..'),tools=path.resolve(process.argv[2]||path.join(native,'.local/n64-toolchain'));
const read=p=>JSON.parse(fs.readFileSync(path.join(native,p)));
const spec=read('game/consumables/effects.json'),sources=read('game/accepted/rom-owners.json').sources;
const original=sources.find(s=>s.targets.includes('func_00041098'));assert(original);
assert.equal(digest(fs.readFileSync(path.join(native,original.source))),original.sourceSha256);
const source='consumables/effects.c',input=fs.readFileSync(path.join(native,'game',source));
const work=path.join(native,'build/consumable-c-qualification');fs.mkdirSync(work,{recursive:true});
const headers=original.headers.map(h=>({name:h.path.slice(8),source:h.file.slice(5),sha256:h.sha256.toLowerCase()}));
for(const [name,hash]of Object.entries(read('rebuild/toolchain.json').tools))assert.equal(digest(fs.readFileSync(path.join(tools,name))).toLowerCase(),hash);
const headerRoot=path.join(work,'include');fs.mkdirSync(headerRoot,{recursive:true});
for(const h of headers){const data=fs.readFileSync(path.join(native,'game',h.source));assert.equal(digest(data).toLowerCase(),h.sha256);const to=path.join(headerRoot,h.name);fs.mkdirSync(path.dirname(to),{recursive:true});fs.writeFileSync(to,data);}
const env={...process.env};
for(const key of ['C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH','DEPENDENCIES_OUTPUT','SUNPRO_DEPENDENCIES'])delete env[key];
const run=(name,args)=>execFileSync(path.join(tools,name),args,{cwd:work,env,windowsHide:true,timeout:60000,stdio:'pipe'});
fs.writeFileSync(path.join(work,'source.c'),input);
run('cpp.exe',['-P','-undef','-nostdinc','-D__mips=3','-D__mips__','-D_MIPSEB','-D__MIPSEB__','-I-', '-I',headerRoot,'source.c','source.i']);
run('cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','source.i','-o','source.s']);
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','source.o','source.s']);
const object=fs.readFileSync(path.join(work,'source.o')),parts=sections(object);object.writeUInt32BE(4,parts['.text'].header+32);fs.writeFileSync(path.join(work,'source.o'),object);
const size=spec.size-80,symbols={...original.bindings,lc_effect_parameters:spec.address+size+48};
fs.writeFileSync(path.join(work,'source.ld'),Object.entries(symbols).map(([n,v])=>`${n} = 0x${v.toString(16)};`).join('\n')+`\nSECTIONS { .text 0x${spec.address.toString(16)} : { *(.text) } }\n`);
run('mips-kmc-elf-ld.exe',['-EB','-m','elf32bmip','-T','source.ld','-o','source.elf','source.o']);
const linked=sections(fs.readFileSync(path.join(work,'source.elf'))),text=linked['.text'];assert.equal(text.address,spec.address);assert(text.size<=size);
for(const name of ['.rodata','.rdata','.data','.bss','.lit4','.lit8'])assert(!linked[name]?.size,`Unexpected allocation ${name}`);
const code=Buffer.alloc(size);text.bytes.copy(code);
const manifest={generator:'native/tools/generate-consumable-c.cjs',source,sourceSha256:digest(input).toLowerCase(),
  original:{source:original.source,sourceSha256:original.sourceSha256,start:spec.start,size:spec.size},
  address:spec.address,size,padToOwner:true,sha256:digest(code).toLowerCase(),symbols,headers,
  metadata:{magic:'LCCONS01',version:1,size:80,parametersOffset:48,parameterCount:15}};
fs.writeFileSync(path.join(native,'game/consumables/effects-c.json'),JSON.stringify(manifest,null,2)+'\n');
console.log(`Qualified consumable C: ${text.size} code bytes in ${size}; 80 metadata/data bytes; original ${spec.size}-byte allocation retained.`);
