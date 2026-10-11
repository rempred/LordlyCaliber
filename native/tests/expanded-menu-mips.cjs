'use strict';
// Execute instruction spans from an actual internal expanded-menu candidate.
// Rendering services are traps: this verifies IDs/counts/addresses, not pixels.
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const catalog = JSON.parse(fs.readFileSync(path.join(__dirname, '../game/promotion/owners.json')));
const layout = JSON.parse(fs.readFileSync(process.argv[2]));
const rom = fs.readFileSync(layout.rom);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
assert.equal(rom.readUInt32BE(0), 0x80371240);
const ram = offset => (offset + 0x801135A0) >>> 0;
const owner = 0x80280000, stack = 0x80060000, display = 0x80350000;
function cpu() {
  const m = new Mips(); m.ram = new Uint8Array(0x400000);
  for (const entry of [...catalog.owners, ...catalog.expandedOwners])
    m.load(ram(entry.start), rom.subarray(entry.start, entry.start + entry.size));
  const header = 0x5DC80;
  assert.equal(rom.toString('ascii', header, header + 8), 'LC05CLAS');
  const dataRom = rom.readUInt32BE(header + 12), size = rom.readUInt32BE(header + 16);
  m.load(rom.readUInt32BE(header + 24), rom.subarray(dataRom, dataRom + size));
  m.load(owner - 16, Buffer.alloc(layout.ownerBytes + 32, 0xA6));
  m.w32(0x80196AF8, owner); m.w32(0x800E9BA0, display); m.set('sp', stack);
  return m;
}
const clobber = m => {
  for (const name of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])
    m.set(name, 0xCCCCCCCC);
};
// The first allocator and memset must agree, and not touch either boundary.
{
  const m = cpu(), calls = [];
  m.trap(0x80070F30, c => { calls.push(['allocate', c.get('a0')]); clobber(c); c.set('v0', owner); });
  m.trap(0x80093380, c => {
    calls.push(['clear', c.get('a0'), c.get('a1')]);
    assert.equal(c.get('a0'), owner); assert.equal(c.get('a1'), layout.ownerBytes);
    c.load(owner, Buffer.alloc(c.get('a1'))); clobber(c);
  });
  m.call(ram(0xAE3F0), ram(0xAE420));
  assert.deepEqual(calls, [['allocate', layout.ownerBytes], ['clear', owner, layout.ownerBytes]]);
  assert.equal(m.r32(0x80196AF8), owner);
  assert(m.ram.slice(m.phys(owner), m.phys(owner) + layout.ownerBytes).every(x => x === 0));
  assert.equal(m.r8(owner - 1), 0xA6); assert.equal(m.r8(owner + layout.ownerBytes), 0xA6);
}
let cases = 0;
for (const index of [0, 31, 80, layout.count - 2]) {
  const id = layout.count - 1, current = 57, eligible = 99;
  function rows() {
    const m = cpu();
    m.w16(owner + layout.ids + index * 2, id);
    m.w16(owner + layout.current + index * 2, current);
    m.w16(owner + layout.eligible + index * 2, eligible);
    // Legacy storage stays poisoned; the expected name points into new data.
    return m;
  }
  // Single-row name lookup, including the full-width ID used for record stride.
  {
    const m = rows(), calls = [];
    m.set('s2', index); m.set('v1', owner); m.set('s3', 10); m.set('s6', 20);
    m.trap(0x80179034, c => { calls.push([c.get('a0'), c.get('a1'), c.get('a2')]); clobber(c); });
    m.call(ram(0x9B648), ram(0x9B680));
    assert.deepEqual(calls, [[m.r32(layout.records + id * 72), 52, 48]]); cases++;
  }
  // Both count draws share the same selected row, despite caller-clobbered registers.
  {
    const m = rows(), calls = [];
    m.set('s2', index); m.set('s0', 20); m.set('s3', 10); m.set('v1', display);
    m.trap(0x8019B8A4, c => { calls.push(c.get('a2')); clobber(c); });
    m.trap(0x8019A7A0, clobber);
    m.call(ram(0x9B918), ram(0x9B980));
    assert.deepEqual(calls, [current, eligible]); cases++;
  }
  // List-view name loop stops at its last row instead of reading past the list.
  {
    const m = rows(), calls = [];
    m.set('s6', owner + 0x120); m.set('s2', 0); m.set('s5', 10); m.set('s0', 20);
    m.w16(owner + 0x122, index); m.w16(owner + 0x126, index);
    m.trap(0x80179034, c => { calls.push(c.get('a0')); clobber(c); });
    m.call(ram(0x9BF04), ram(0x9BF64));
    assert.deepEqual(calls, [m.r32(layout.records + id * 72)]); cases++;
  }
  // The list-view count/sprite loop passes the same high ID to the sprite service.
  {
    const m = rows(), calls = [], sprites = [];
    m.set('s6', owner + 0x120); m.set('fp', 20); m.set('s5', 10);
    m.w16(owner + 0x122, index); m.w16(owner + 0x126, index);
    for (const address of [0x8019B0D0, 0x8019A7A0]) m.trap(address, clobber);
    m.trap(0x8019B8A4, c => { calls.push(c.get('a2')); clobber(c); });
    m.trap(0x8019FA14, c => { sprites.push(c.get('a0')); clobber(c); });
    m.call(ram(0x9C410), ram(0x9C520));
    assert.deepEqual(calls, [current, eligible]); assert.deepEqual(sprites, [id]); cases++;
  }
  // Category selection commits the selected halfword to the shared class field.
  {
    const m = rows(); m.set('s1', owner + 0x120); m.w16(owner + 0x120, index);
    m.trap(0x801CBD0C, clobber);
    m.call(ram(0xCD108), ram(0xCD134));
    assert.equal(m.r16(owner + 0x5E8), id); cases++;
  }
}
let aliasCases = 0;
for (const entry of [...catalog.owners, ...catalog.expandedOwners]) {
  for (const reference of entry.expandedDataReferences || []) {
    // Execute each original indexed lookup with values chosen independently of
    // its instruction encoding. The old map deliberately contains a different alias.
    for (const id of [0, 5, 90, layout.count - 1]) for (const value of [0, 24]) {
      const m = cpu(), begin = entry.start + reference.high;
      const high = rom.readUInt32BE(begin), add = rom.readUInt32BE(begin + 4), low = rom.readUInt32BE(begin + 8);
      assert.equal(high >>> 26, 15); assert.equal(low >>> 26, 36);
      assert.equal(add & 63, 33); // original indexed ADDU
      const base = (high >>> 16) & 31, rs = (add >>> 21) & 31, rt = (add >>> 16) & 31;
      assert(rs === base || rt === base);
      m.r[rs === base ? rt : rs] = id;
      m.w8(0x801EF3B8 + id, value ^ 0x7F); m.w8(layout.aliases + id, value);
      m.call(ram(begin), ram(begin + 12));
      assert.equal(m.r[(low >>> 16) & 31], value); aliasCases++;
    }
  }
}
// The retail special-ID rule must not classify appended ordinary IDs by their
// number alone. Every original ID keeps its old result and default-class rule.
let sourceClassCases = 0;
{
  const m = cpu();
  for (let secondary = 0; secondary < layout.count; ++secondary)
    for (const defaultClass of [0, 5, 41, 42, 90, 164, 165, 255])
      for (const eligible of [0, 1]) {
        m.set('fp', secondary); m.set('s0', eligible); m.w8(stack + 0x1F, defaultClass);
        m.call(ram(0xC3ABC), ram(0xC3AEC));
        const ordinary = secondary < 42 || secondary >= 165;
        assert.equal(m.get('s0'), eligible && (ordinary || (defaultClass > 0 && defaultClass < 42)) ? 1 : 0,
          `secondary=${secondary}, default=${defaultClass}, eligible=${eligible}`);
        sourceClassCases++;
      }
}
let actionClassCases = 0;
{
  const m = cpu(), defaultClass = m.r8(layout.records + 5 * 72 + 69);
  for (let primary = 0; primary < layout.count; ++primary) {
    const ordinary = primary < 81 || primary >= 165;
    for (const alias of [0, 24]) {
      m.set('v1', primary); m.set('v0', alias); m.set('a0', 5);
      m.call(ram(0xC02BC), ram(0xC02E8));
      assert.equal(m.get('v0'), ordinary ? 1 : 0);
      assert.equal(m.get('a0'), alias || defaultClass);
      m.set('a0', primary); m.set('v0', alias); m.set('v1', 5);
      m.call(ram(0xC038C), ram(0xC03B8));
      assert.equal(m.get('v0'), ordinary ? 1 : 0);
      assert.equal(m.get('v1'), alias || defaultClass);
      actionClassCases += 2;
    }
    m.set('s5', owner); m.w8(owner + 17, primary);
    m.call(ram(0xC0620), ram(0xC062C));
    assert.equal(m.get('v0'), ordinary ? 0 : 1);actionClassCases++;
  }
}
let equipmentCases = 0;
for (const id of [1, 5, layout.count - 1]) for (const allowed of [false, true])
  for (const missing of [-1, 0, 1, 2, 3]) {
    const m = cpu(), getters = [], lookups = [];
    m.w8(stack + 0x17, id); m.set('s0', allowed ? 1 : 0);
    for (let i = 0; i < 4; ++i) {
      const address = 0x80050000 + i * 16;
      m.w32(0x801EF288 + i * 4, address);
      m.trap(address, c => {
        getters.push([c.get('a0'), c.get('a1')]); clobber(c); c.set('v0', 100 + i);
      });
      m.w8(0x80196B03 + i * 4, i === missing ? 0 : 1);
    }
    m.trap(0x8016B6FC, c => {
      const item = c.get('a0'); lookups.push(item); clobber(c); c.set('v0', item - 100);
    });
    m.call(ram(0xC3CE4), ram(0xC3D7C));
    const check = allowed && id >= 2;
    assert.deepEqual(getters, check ? Array.from({length:4}, () => [id,id]) : []);
    assert.deepEqual(lookups, check ? [100,101,102,103] : []);
    assert.equal(m.get('v0'), allowed && (!check || missing === -1) ? 1 : 0);
    equipmentCases++;
  }
let descriptionCases = 0, helpCases = 0, specialDescriptionCases = 0, descriptionRecords = 0;
{
  const m = cpu(), header = 0x5DC80;
  assert.equal(rom.readUInt32BE(header + 8), 3);
  m.load(0x80187C14, rom.subarray(0x5DB14, 0x5DC80));
  m.load(0x801805D0, rom.subarray(0x564D0, 0x566F4));
  m.load(0x8017FC28, rom.subarray(0x55B28, 0x564B4));
  for (let bank = 0; bank < 21; ++bank) for (let id = 0; id <= 256; ++id) {
    m.set('sp', stack); m.set('s6', 0x12345678); m.set('a0', 0x80051000);
    m.set('a1', bank); m.set('a2', id); m.set('a3', 0x87654321);
    m.call(0x801805D0, 0x801805D8);
    const expected = bank !== 5 || id < 165 ? id : id === 256 ? 224 : id + 60;
    assert.equal(m.get('a2'), expected);
    assert.equal(m.get('a0'), 0x80051000); assert.equal(m.get('a1'), bank);
    assert.equal(m.get('a3'), 0x87654321); assert.equal(m.get('sp'), stack - 0x88);
    assert.equal(m.r32(stack - 8), 0x12345678); descriptionCases++;
  }
  for (let help = 0; help < 23; ++help) {
    m.load(0x80051000, Buffer.from(String(help).padStart(2, '0')));
    m.w8(0x801939D8, 1); m.set('s6', 0x80051000); m.set('s1', 0x80052000);
    m.call(0x801804E0, 0x80180544);
    assert.equal(m.get('a0'), 0x80052000); assert.equal(m.get('a2'), 200 + help);
    assert.equal(m.get('a1'), rom.readUInt32BE(0x66D60)); helpCases++;
  }
  // This copied instruction span contains a relative selection branch and its
  // original absolute exit. It tests the Prince exception, not overlay loading.
  m.load(0x80052000, rom.subarray(0x15E558, 0x15E584));
  for (const state of [0, 4, 5]) for (const id of [1, 98, 164, 165, 224, 255]) {
    m.w8(0x8018F481, state); m.set('a0', id);
    m.call(0x80052000, 0x8020D4F0);
    const special = state === 4 && id === 98;
    assert.equal(m.get('v0'), 0x5000 | (special ? 256 : id)); specialDescriptionCases++;
  }
  if (rom.toString('ascii', 0x2378854, 0x237885C) === 'LC05TEXT') {
    // Independent browser codec supplies the decoded bank. Execute the original
    // game's SJIS-aware record selector; allocation/free are the only traps.
    const vm = require('node:vm');
    const context = vm.createContext({window:{}, Uint8Array, DataView});
    vm.runInContext('var OB64 = window.OB64 = {};\n' + fs.readFileSync(path.join(__dirname, '../../parsers.js'), 'utf8'), context);
    const resourceOffset = rom.readUInt32BE(0x2378854 + 12);
    const decoded = Buffer.from(context.OB64.lzssDecode(rom, resourceOffset + 8, rom.readUInt32BE(resourceOffset + 4)).output);
    const entries = []; let pos = 0;
    while (pos < decoded.length) {
      const end = decoded.indexOf(Buffer.from([13,10]), pos);
      assert(end >= pos); entries.push(decoded.subarray(pos,end)); pos = end + 2;
    }
    assert.equal(entries.length, layout.count + 60);
    const resource = 0x80080000, output = 0x80054000;
    m.load(0x8017FAF8, rom.subarray(0x559F8, 0x55B28));
    m.trap(0x8009DD38, c => {
      assert.equal(c.get('a0'), resourceOffset - 0x594280);
      c.load(resource, decoded); c.set('v0', resource);
    });
    m.trap(0x800712C4, c => assert.equal(c.get('a0'), resource));
    for (const id of [...Array(layout.count).keys(), 256]) {
      m.set('sp', stack); m.set('a0', output); m.set('a1', 5); m.set('a2', id); m.set('a3', 0);
      m.call(0x801805D0, 0x801805D8);
      m.set('sp', stack); m.set('a1', resourceOffset - 0x594280);
      m.load(output - 1, Buffer.alloc(0x502, 0xA5));
      m.call(0x8017FAF8, 0x80000100, 2000000);
      const index = id === 256 ? 224 : id < 165 ? id : id + 60;
      const expected = Buffer.concat([entries[index], Buffer.from([13,10])]);
      assert.deepEqual(Buffer.from(m.ram.slice(m.phys(output), m.phys(output) + expected.length)), expected);
      assert.equal(m.r8(output - 1), 0xA5); assert.equal(m.r8(output + expected.length), 0xA5);
      assert(expected.length < 0x500); descriptionRecords++;
    }
  }
}
console.log(JSON.stringify({classes:layout.count, categoryCases:cases, aliasCases, sourceClassCases, actionClassCases, equipmentCases, descriptionCases, helpCases, specialDescriptionCases, descriptionRecords, allocationBytes:layout.ownerBytes,
  scope:'executed selection/name/count/sprite-call spans; not raster output or full game'}));
