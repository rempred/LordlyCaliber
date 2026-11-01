'use strict';
// Build-time compilation only: no ROM, Decomp checkout, or downloaded objects.
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const {digest} = require('./decomp-evidence.cjs');
function sections(data) {
  assert.equal(data.subarray(0,7).toString('hex'), '7f454c46010201');
  assert.equal(data.readUInt16BE(18), 8);
  const at = data.readUInt32BE(32), count = data.readUInt16BE(48), stride = data.readUInt16BE(46);
  assert.equal(stride, 40); assert.ok(at + count * stride <= data.length);
  const nameHeader = at + data.readUInt16BE(50) * stride;
  const names = data.subarray(data.readUInt32BE(nameHeader+16), data.readUInt32BE(nameHeader+16)+data.readUInt32BE(nameHeader+20));
  const result = {};
  for (let i=0;i<count;i++) {
    const h=at+i*stride, n=data.readUInt32BE(h), end=names.indexOf(0,n);
    assert.ok(end>=n); const name=names.toString('ascii',n,end);
    const offset=data.readUInt32BE(h+16), size=data.readUInt32BE(h+20), type=data.readUInt32BE(h+4);
    if (type!==8) assert.ok(offset+size<=data.length);
    result[name]={header:h,address:data.readUInt32BE(h+12),size,
      bytes:type===8?Buffer.alloc(0):data.subarray(offset,offset+size)};
  }
  return result;
}
function build(native, tools, output) {
  const manifest=JSON.parse(fs.readFileSync(path.join(native,'game/accepted/effect-owners.json')));
  const pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json'))).tools;
  for (const [name,hash] of Object.entries(pins)) assert.equal(digest(fs.readFileSync(path.join(tools,name))).toLowerCase(),hash);
  const work=path.join(path.dirname(output),'accepted-effects-work'); fs.mkdirSync(work,{recursive:true});
  const env={...process.env};
  for(const key of ['C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH','DEPENDENCIES_OUTPUT','SUNPRO_DEPENDENCIES']) delete env[key];
  const run=(cwd,tool,args)=>execFileSync(path.join(tools,tool),args,{cwd,env,windowsHide:true,timeout:60000,stdio:'pipe'});
  const blocks=[];
  for (const o of manifest.owners) {
    const dir=path.join(work,o.symbol), includes=path.join(dir,'include'); fs.mkdirSync(includes,{recursive:true});
    const auth=(file,hash)=>{const bytes=fs.readFileSync(path.join(native,file));assert.equal(digest(bytes),hash,'Changed accepted source: '+file);return bytes;};
    fs.writeFileSync(path.join(dir,'source.c'),auth(o.source,o.sourceSha256));
    for (const h of o.headers) {
      assert.match(h.name,/^[A-Za-z0-9_/-]+\.h$/);
      const file=path.join(includes,h.name); fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,auth(h.file,h.sha256));
    }
    try {
      run(dir,'cpp.exe',['-P','-undef','-nostdinc','-D__mips=3','-D__mips__','-D_MIPSEB','-D__MIPSEB__','-I-', '-I',includes,'source.c','source.i']);
      run(dir,'cc1.exe',['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char','source.i','-o','source.s']);
      run(dir,'mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','source.o','source.s']);
      const object=fs.readFileSync(path.join(dir,'source.o')), raw=sections(object);
      object.writeUInt32BE(4,raw['.text'].header+32); // placement alignment only; no instruction rewriting
      fs.writeFileSync(path.join(dir,'source-link.o'),object);
      assert.ok(o.tables.length<=1);
      const table=o.tables[0];
      const rodata=table?`.rodata ${table.vramStart} : { *(.rodata) }`:'';
      const script=Object.entries(o.bindings).map(([name,value])=>`${name} = 0x${value.toString(16)};`).join('\n')+
        `\nSECTIONS { .text 0x${o.address.toString(16)} : { *(.text) } ${rodata} }\n`;
      fs.writeFileSync(path.join(dir,'source.ld'),script);
      run(dir,'mips-kmc-elf-ld.exe',['-EB','-m','elf32bmip','-T','source.ld','-o','source.elf','source-link.o']);
      const linked=sections(fs.readFileSync(path.join(dir,'source.elf'))), text=linked['.text'];
      assert.equal(text.address,o.address); assert.ok(text.size>=o.size);
      assert.ok(text.bytes.subarray(o.size).every(n=>n===0), 'Unexpected text tail');
      let extent=0;
      for(const part of o.parts) {
        assert.equal(part.offset,extent);extent+=part.size;
        assert.equal(digest(text.bytes.subarray(part.offset,extent)),part.sha256,'Compiled C differs: '+o.symbol);
      }
      assert.equal(extent,o.size);
      blocks.push({start:o.start,address:o.address,code:1,bytes:text.bytes.subarray(0,o.size)});
      if(table) {
        const data=linked[table.compilerSection];assert.equal(data.address,Number(table.vramStart));
        assert.ok(data.size>=table.bytes);assert.ok(data.bytes.subarray(table.bytes).every(n=>n===0));
        assert.equal(digest(data.bytes.subarray(0,table.bytes)),table.expectedLinkedSha256);
        blocks.push({start:Number(table.romStart),address:data.address,code:0,bytes:data.bytes.subarray(0,table.bytes)});
      }
      for(const name of ['.data','.bss','.rdata','.lit4','.lit8',...(table?[]:['.rodata'])])
        assert.ok(!linked[name]?.size,'Unqualified allocated section: '+name);
    } catch(error) { throw Error(o.symbol+': '+error.message+'\n'+(error.stderr||'')); }
  }
  const header=Buffer.alloc(44);header.write('LCCODE01');header.writeUInt32BE(blocks.length,8);
  Buffer.from(manifest.fingerprint,'hex').copy(header,12);
  const chunks=[header];
  for (const b of blocks) {
    const h=Buffer.alloc(16);h.writeUInt32BE(b.start);h.writeUInt32BE(b.address,4);h.writeUInt32BE(b.bytes.length,8);h.writeUInt32BE(b.code,12);
    chunks.push(h,b.bytes);
  }
  fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output+'.tmp',Buffer.concat(chunks));fs.renameSync(output+'.tmp',output);
  console.log(`Compiled ${manifest.owners.length} accepted C owners / ${blocks.length} exact code and table blocks`);
}
if(require.main===module) build(path.resolve(process.argv[2]),path.resolve(process.argv[3]),path.resolve(process.argv[4]));
module.exports={build, sections};
