'use strict';

// Execute independently assembled old and newly compiled C menu owners with
// identical inputs. No ROM, emulator, or game installation is needed.
const fs = require('node:fs');
const path = require('node:path');
const cp = require('node:child_process');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const native = path.resolve(__dirname, '..');
const tools = process.env.LORDLY_N64_TOOLCHAIN || path.join(native, '.local/n64-toolchain');
const pins = JSON.parse(fs.readFileSync(path.join(native, 'rebuild/toolchain.json')));
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
for (const [file, expected] of Object.entries(pins.tools))
  assert.equal(hash(fs.readFileSync(path.join(tools, file))), expected, file);
fs.mkdirSync(path.join(native, 'build'), {recursive: true});
const work = fs.mkdtempSync(path.join(native, 'build/class-menu-'));
const run = (exe, args) => cp.execFileSync(path.join(tools, exe), args, {cwd: work, encoding: 'utf8'});
const menuAddress = 0x801D27E8, lookupAddress = 0x801CF2F0, checkAddress = 0x801D2358;
const source = fs.readFileSync(path.join(native, 'game/promotion/family-menu.c'), 'utf8');
const config = fs.readFileSync(path.join(native, 'game/promotion/class_menu_config.h'), 'utf8');
function extract(name, address, section = '.text', bindings = '') {
  fs.writeFileSync(path.join(work, name + '.ld'), `${bindings}\nFAMILY_LOOKUP = 0x${lookupAddress.toString(16)}; CHECK_CHANGE = 0x${checkAddress.toString(16)};\nSECTIONS { ${section} 0x${address.toString(16)} : { *(${section}) } }\n`);
  run('mips-kmc-elf-as.exe', ['-G', '0', '-mips3', '-mabi=32', '-force-n64align', '-EB', '-o', name+'.o', name+'.s']);
  run('mips-kmc-elf-ld.exe', ['-EB', '-m', 'elf32bmip', '-T', name+'.ld', '-o', name+'.elf', name+'.o']);
  run('mips-kmc-elf-objcopy.exe', ['-O', 'binary', ...['.text','.data','.bss','.rdata','.reginfo','.pdr','.mdebug','.comment','.lit4','.lit8'].filter(x=>x!==section).map(x=>'--remove-section='+x), name+'.elf', name+'.bin']);
  return fs.readFileSync(path.join(work, name+'.bin'));
}
function compile(name, configuration, code = source, address = menuAddress, bindings = '') {
  fs.writeFileSync(path.join(work, 'class_menu_config.h'), configuration);
  fs.writeFileSync(path.join(work, name+'.c'), code);
  run('cpp.exe', ['-P', '-undef', '-nostdinc', '-D__mips=3', '-D__mips__', '-D_MIPSEB', '-D__MIPSEB__', name+'.c', name+'.i']);
  run('cc1.exe', ['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char',name+'.i','-o',name+'.s']);
  return extract(name, address, '.text', bindings);
}
function assembly(name, filename, address) {
  fs.writeFileSync(path.join(work, name+'.s'), '.set noat\n.set noreorder\n.section .owner,"ax"\n.balign 4\n' + fs.readFileSync(path.join(native, 'game/promotion', filename), 'utf8'));
  return extract(name, address, '.owner');
}
const previous = assembly('previous', 'family-menu.s', menuAddress);
assert.equal(hash(previous), 'dd8d2826376f6c4831f249a6e3e0f6f211c0b9f24fdda393188dae6f8686b298');
const lookup = assembly('lookup', 'family-lookup.s', lookupAddress);
const compiled = compile('menu', config);
assert(compiled.length <= 528, `C menu exceeds the existing function extent (${compiled.length})`);
const padded = Buffer.alloc(528); compiled.copy(padded);
const menuOwner = JSON.parse(fs.readFileSync(path.join(native, 'game/promotion/owners.json'))).owners.find(x => x.start === 0xbf248);
assert.equal(hash(padded), menuOwner.cSourceSha256, 'export owner identity');
const expanded = compile('expanded', config.replace('LC_CLASS_COUNT 81', 'LC_CLASS_COUNT 256').replace('LC_LIST_CAPACITY 82', 'LC_LIST_CAPACITY 257').replace('LC_DISABLED_FLAG 128', 'LC_DISABLED_FLAG 32768').replace('LC_LIST_OFFSET 0x1C0E', 'LC_LIST_OFFSET 0x2528').replace('LC_DISCOVERY_ADDRESS 0x80196A76U', 'LC_DISCOVERY_ADDRESS 0x80308000U'));
const owner = 0x80260000, resource = 0x80300000;
function machine(code, count, listOffset, selected, families, discovery, eligible) {
  const cpu = new Mips();
  cpu.ram = new Uint8Array(0x400000);
  cpu.load(menuAddress, code); cpu.load(lookupAddress, lookup);
  cpu.set('sp', 0x80060000);
  cpu.load(owner, Buffer.alloc(0x2800, 0xA6));
  cpu.w32(0x80196AF8, owner); cpu.w32(owner+0x80, resource);
  cpu.w16(owner+0x5E8, selected); cpu.w8(owner+0x18B, 73);
  families.forEach((value, id) => cpu.w32(resource+648+id*4, value));
  cpu.load(count > 81 ? 0x80308000 : 0x80196A76, discovery);
  cpu.load(0x801EF370, Buffer.from([1,2,4,8,16,32,64,128]));
  const calls = [];
  cpu.trap(checkAddress, c => {
    assert.equal(c.get('a0'), 73);
    const target = c.get('a1'); assert(target < count);
    calls.push(target);
    c.set('v0', eligible(target));
    for (const reg of ['v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9']) c.set(reg, 0xCCCCCCCC);
  });
  const before = Buffer.from(cpu.ram.subarray(owner & 0x1FFFFFFF, (owner & 0x1FFFFFFF)+0x2800));
  cpu.call(menuAddress);
  const size = cpu.r16(owner+0x1BFC);
  assert(size <= count);
  assert.equal(cpu.r16(owner+listOffset+size*2), 0, 'terminator');
  const entries = Array.from({length: size}, (_, i) => cpu.r16(owner+listOffset+i*2));
  const after = Buffer.from(cpu.ram.subarray(owner & 0x1FFFFFFF, (owner & 0x1FFFFFFF)+0x2800));
  // Only the reported count and the produced list may change in the Army owner.
  after.copy(before, 0x1BFC, 0x1BFC, 0x1BFE);
  after.copy(before, listOffset, listOffset, listOffset+(size+1)*2);
  assert.deepEqual(after, before, 'write outside list/count');
  return {entries, calls};
}
let comparisons = 0;
for (let behavior = 0; behavior <= 10; behavior++) {
  const families = Array.from({length:81}, (_, id) => id%3 === 0 ? (behavior<<16)|0x1234 : (5<<16)|0x4321);
  families[0] = 0;
  if (!behavior) families[3] = 0;
  for (const selected of [0,3,6,38,43,80,81,127,128,164,255]) {
    for (const pattern of [0,0xFF,0xA5]) {
      const discovery = Buffer.alloc(11, pattern);
      for (const eligibility of [() => 0, () => 1, id => id%2]) {
        const old = machine(previous, 81, 0x1C0E, selected, families, discovery, eligibility);
        const next = machine(compiled, 81, 0x1C0E, selected, families, discovery, eligibility);
        assert.deepEqual(next, old, JSON.stringify({behavior, selected, pattern}));
        comparisons++;
      }
    }
  }
}
// Independent explicit expectations cover the widened format, full list bound,
// upper IDs, the selected-row disable flag, and discovery across byte boundaries.
const families = Array(256).fill((5<<16)|16);
families[0] = 0;
const full = machine(expanded, 256, 0x2528, 165, families, Buffer.alloc(32, 255), () => 1);
assert.deepEqual(full.entries, Array.from({length:255}, (_, i) => i+1 === 165 ? 0x80A5 : i+1));
const sparse = Buffer.alloc(32);
for (const id of [1,127,128,165,254,255]) sparse[id>>3] |= 1 << (id&7);
const partial = machine(expanded, 256, 0x2528, 128, families, sparse, id => id%2);
assert.deepEqual(partial.entries, [1,127,0x8080,165,0x80FE,255]);
// All 256 IDs can be members; one extra halfword is reserved for termination.
families[0] = (5<<16)|16;
assert.equal(machine(expanded, 256, 0x2528, 255, families, Buffer.alloc(32,255), () => 1).entries.length, 256);
const selectionAddress = 0x801DC740;
const selectionSource = fs.readFileSync(path.join(native, 'game/promotion/class-selection.c'), 'utf8');
const selectionBindings = 'lc_army_owner = 0x80196AF8; lc_input_buttons = 0x800E8100; lc_repeat_buttons = 0x800E8700; lc_menu_sound = 0x801D8A60; lc_menu_update = 0x801CFC24; lc_menu_equipment = 0x801D2B7C;';
const selection = compile('selection', config, selectionSource, selectionAddress, selectionBindings);
assert(selection.length <= 388, 'selection owner extent');
const selectionPadded = Buffer.alloc(388); selection.copy(selectionPadded);
const selectionOwner = JSON.parse(fs.readFileSync(path.join(native, 'game/promotion/owners.json'))).owners.find(x=>x.start===0xc91a0);
assert.equal(hash(selectionPadded), selectionOwner.cSourceSha256, 'selection export owner identity');
const selectionExpanded = compile('selection_expanded', config.replace('LC_DISABLED_FLAG 128', 'LC_DISABLED_FLAG 32768').replace('LC_LIST_OFFSET 0x1C0E', 'LC_LIST_OFFSET 0x2528'), selectionSource, selectionAddress, selectionBindings);
function select(code, entries, selected, buttons, repeat, flags, listOffset) {
  const cpu = new Mips(); cpu.ram = new Uint8Array(0x400000);
  cpu.load(selectionAddress, code); cpu.set('sp', 0x80060000);
  cpu.w32(0x80196AF8, owner);
  cpu.w16(owner+0x5EA, selected); cpu.w16(owner+0x5E8, 23);
  cpu.w16(owner+0x1BFC, entries.length); cpu.w16(owner+0x130, flags);
  cpu.w16(0x800E8100, buttons); cpu.w16(0x800E8700, repeat);
  entries.forEach((entry, i) => cpu.w16(owner+listOffset+i*2, entry));
  const calls = [];
  for (const [address, name] of [[0x801D8A60,'sound'],[0x801CFC24,'update'],[0x801D2B7C,'equipment']]) {
    cpu.trap(address, c => {
      if (name === 'sound') assert.equal(c.get('a0'), 1);
      calls.push(name);
      for (const reg of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9']) c.set(reg, 0xCCCCCCCC);
    });
  }
  cpu.call(selectionAddress);
  return {result:cpu.get('v0'), selected:cpu.r16(owner+0x5EA), classId:cpu.r16(owner+0x5E8), calls};
}
let selectionComparisons = 0;
if (process.env.LORDLY_TEST_ROM) {
  const rom = fs.readFileSync(process.env.LORDLY_TEST_ROM);
  if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
  assert.equal(hash(rom), '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
  const original = rom.subarray(0xC91A0,0xC9324);
  for (const count of [1,2,3,4,5,8,17,81]) {
    const entries = Array.from({length:count}, (_,i) => ((i*17)%81) | (i%2 ? 128 : 0));
    for (let selected=0; selected<count; selected++)
      for (const [buttons,repeat] of [[0,0],[0,0x800],[0,0x400],[0,0x200],[0,0x100],[0,0xF00],[0x1000,0xF00],[0x8000,0xF00],[0x4000,0xF00],[0xD000,0xF00]])
        for (const flags of [0,0x200]) {
          const args = [entries,selected,buttons,repeat,flags,0x1C0E];
          assert.deepEqual(select(selection,...args), select(original,...args), JSON.stringify({count,selected,buttons,repeat,flags}));
          selectionComparisons++;
        }
  }
}
const wideEntries = Array.from({length:256}, (_,i) => i | (i%2 ? 0x8000 : 0));
for (const [start, repeat, target] of [[126,0x400,127],[129,0x800,128],[161,0x100,165],[250,0x100,254],[251,0x100,255],[255,0x400,255]]) {
  const result = select(selectionExpanded,wideEntries,start,0,repeat,0,0x2528);
  assert.equal(result.selected,target);
  assert.equal(result.classId,start===target ? 23 : target);
}
assert.deepEqual(select(selectionExpanded,[],0,0,0x400,0,0x2528), {result:0,selected:0,classId:23,calls:[]});
console.log(JSON.stringify({comparisons, expandedCases:3, compiledBytes:compiled.length, ownerSha256:hash(padded), selectionComparisons, selectionExpandedCases:7, selectionBytes:selection.length, selectionSha256:hash(selectionPadded), work}, null, 2));
