'use strict';
// Compile every accepted pure-C producer into authenticated ROM replacement
// blocks. No original ROM, upstream build output or prebuilt game code is used.
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const {execFileSync} = require('node:child_process');
const {digest, sourcePath} = require('./decomp-evidence.cjs');
const {sections} = require('./build-accepted-effects.cjs');
function projectReadonly(object, projections) {
  if (!projections.length) return object;
  const raw = sections(object), symbols = raw['.symtab'], rels = raw['.rel.text'];
  assert.ok(symbols && rels);
  const entries = [symbols.bytes];
  const anchors = new Map();
  let count = symbols.size / 16;
  for (const p of projections) {
    let found = false;
    for (let i = 0; i < rels.size; i += 8) if (rels.bytes.readUInt32BE(i) === p.offset) {
      assert.equal(rels.bytes.readUInt32BE(i + 4), p.info);
      if (!anchors.has(p.value)) {
        const symbol = Buffer.alloc(16);
        symbol.writeUInt32BE(p.value >>> 0, 4);
        symbol[12] = 0x10; // Global absolute symbol; no instruction or addend changes.
        symbol.writeUInt16BE(0xfff1, 14);
        entries.push(symbol); anchors.set(p.value, count++);
      }
      rels.bytes.writeUInt32BE((anchors.get(p.value) << 8) | (p.info & 255), i + 4);
      found = true; break;
    }
    assert.ok(found, 'Missing qualified read-only relocation');
  }
  const start = (object.length + 3) & ~3;
  object.writeUInt32BE(start, symbols.header + 16);
  object.writeUInt32BE(count * 16, symbols.header + 20);
  return Buffer.concat([object, Buffer.alloc(start - object.length), ...entries]);
}
function build(native, tools, output, sourceFilter) {
  const manifest = JSON.parse(fs.readFileSync(path.join(native, 'game/accepted/rom-owners.json')));
  const pins = JSON.parse(fs.readFileSync(path.join(native, 'rebuild/toolchain.json'))).tools;
  for (const [name, hash] of Object.entries(pins))
    assert.equal(digest(fs.readFileSync(path.join(tools, name))).toLowerCase(), hash);
  const work = path.join(path.dirname(output), 'accepted-rom-work');
  fs.mkdirSync(work, {recursive: true});
  const env = {...process.env};
  for (const key of ['C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH','DEPENDENCIES_OUTPUT','SUNPRO_DEPENDENCIES']) delete env[key];
  const run = (cwd, tool, args) => execFileSync(path.join(tools, tool), args,
    {cwd, env, windowsHide: true, timeout: 60000, stdio: 'pipe'});
  const blocks = [], seen = new Map();
  const sources = manifest.sources.filter(g => !sourceFilter || sourceFilter(g));
  for (const g of sources) {
    const dir = path.join(work, g.name), tree = path.join(dir, 'inputs');
    fs.mkdirSync(tree, {recursive: true});
    const stage = (relative, file, hash) => {
      sourcePath(relative);
      const data = fs.readFileSync(path.join(native, file));
      assert.equal(digest(data), hash, 'Changed accepted source: ' + file);
      const dest = path.join(tree, relative);
      fs.mkdirSync(path.dirname(dest), {recursive: true}); fs.writeFileSync(dest, data);
      return dest;
    };
    const input = stage(g.sourcePath, g.source, g.sourceSha256);
    for (const h of g.headers) stage(h.path, h.file, h.sha256);
    try {
      run(dir, 'cpp.exe', ['-P','-undef','-nostdinc','-D__mips=3','-D__mips__','-D_MIPSEB','-D__MIPSEB__',
        '-I',path.join(tree,'include'),'-I',tree,input,'source.i']);
      run(dir, 'cc1.exe', ['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC',
        '-mno-abicalls','-fno-builtin','-funsigned-char','source.i','-o','source.s']);
      run(dir, 'mips-kmc-elf-as.exe', ['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','source.o','source.s']);
      let object = fs.readFileSync(path.join(dir, 'source.o'));
      const raw = sections(object);
      for (const section of Object.keys(g.sections)) {
        assert.ok(raw[section], 'Missing compiled section: ' + section);
        object.writeUInt32BE(4, raw[section].header + 32); // Placement only, never instruction changes.
        // GNU 2.6's implicit li.d literal pool can start with non-allocatable
        // flags. Accepted read-only placement requires PROGBITS/ALLOC; retain
        // the compiler's payload and relocations, changing only ELF attributes.
        if (section === '.rodata') {
          assert.equal(object.readUInt32BE(raw[section].header + 4), 1);
          object.writeUInt32BE(2, raw[section].header + 8);
        }
      }
      object = projectReadonly(object, g.projections);
      fs.writeFileSync(path.join(dir, 'source-link.o'), object);
      const script = Object.entries(g.bindings).map(([name, value]) => `${name} = 0x${value.toString(16)};`).join('\n') +
        '\nSECTIONS {\n' + Object.entries(g.sections).map(([name, address]) =>
          `${name} 0x${address.toString(16)} : { *(${name}) }`).join('\n') + '\n}\n';
      fs.writeFileSync(path.join(dir, 'source.ld'), script);
      run(dir, 'mips-kmc-elf-ld.exe', ['-EB','-m','elf32bmip','-T','source.ld','-o','source.elf','source-link.o']);
      const linked = sections(fs.readFileSync(path.join(dir, 'source.elf')));
      for (const part of g.parts) {
        const section = linked[part.section];
        assert.ok(section && part.offset >= 0 && part.size > 0 && part.offset + part.size <= section.size);
        if (part.code) assert.equal(section.address + part.offset, part.address);
        const bytes = section.bytes.subarray(part.offset, part.offset + part.size);
        assert.equal(digest(bytes), part.sha256, 'Compiled C differs at ROM ' + part.start.toString(16));
        const key = part.start + ':' + part.size;
        if (seen.has(key)) {
          const old = seen.get(key);
          assert.equal(old.address, part.address); assert.equal(old.code, part.code); assert.ok(old.bytes.equals(bytes));
        } else {
          const block = {start: part.start, address: part.address, code: part.code, bytes};
          blocks.push(block); seen.set(key, block);
        }
      }
      for (const name of ['.data','.bss','.rdata','.lit4','.lit8',...('.rodata' in g.sections ? [] : ['.rodata'])])
        assert.ok(!linked[name]?.size, 'Unqualified allocated section: ' + name);
    } catch (error) { throw Error(g.source + ': ' + error.message + '\n' + (error.stderr || '')); }
  }
  blocks.sort((a,b) => a.start - b.start || a.bytes.length - b.bytes.length);
  for (let i=0;i<blocks.length;++i) for (let j=i+1;j<blocks.length && blocks[j].start<blocks[i].start+blocks[i].bytes.length;++j) {
    const a=blocks[i],b=blocks[j],offset=b.start-a.start,length=Math.min(b.bytes.length,a.bytes.length-offset);
    assert.equal(a.address+offset,b.address); assert.ok(a.bytes.subarray(offset,offset+length).equals(b.bytes.subarray(0,length)), 'Conflicting accepted allocations');
  }
  const header = Buffer.alloc(44); header.write('LCCODE01'); header.writeUInt32BE(blocks.length,8);
  Buffer.from(manifest.fingerprint,'hex').copy(header,12);
  const chunks = [header];
  for (const b of blocks) {
    const h=Buffer.alloc(16);h.writeUInt32BE(b.start);h.writeUInt32BE(b.address,4);h.writeUInt32BE(b.bytes.length,8);h.writeUInt32BE(b.code,12);
    chunks.push(h,b.bytes);
  }
  fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output+'.tmp',Buffer.concat(chunks));fs.renameSync(output+'.tmp',output);
  console.log(`Compiled ${sources.length} complete sources / ${blocks.length} exact ROM blocks${sourceFilter ? ' (filtered check)' : ' / ' + manifest.counts.functions + ' accepted C functions'}`);
}
if (require.main === module) build(path.resolve(process.argv[2]), path.resolve(process.argv[3]), path.resolve(process.argv[4]));
module.exports = {build};
