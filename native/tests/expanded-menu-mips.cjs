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
console.log(JSON.stringify({classes:layout.count, categoryCases:cases, aliasCases, sourceClassCases, equipmentCases, allocationBytes:layout.ownerBytes,
  scope:'executed selection/name/count/sprite-call spans; not raster output or full game'}));
