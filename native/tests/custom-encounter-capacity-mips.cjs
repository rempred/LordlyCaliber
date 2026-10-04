'use strict';
// Exercise the installed selection call and the original dispatch continuation.
// Rejection must reach the ordinary no-encounter return without staging a spawn.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const fixtures = JSON.parse(fs.readFileSync(process.argv[2]));
let cases = 0;
for (const f of fixtures) for (const freeCount of [0, 1, 2, 5]) {
  for (const freeSource of [-1, 29, 30, 49]) {
    const rom = fs.readFileSync(f.rom);
    if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
    const m = new Mips(); m.ram = new Uint8Array(0x400000);
    // The success continuation loads two position arguments, with no FP math.
    const floating = new Uint32Array(32), integerStep = m.exec.bind(m);
    m.exec = function(pc, next) {
      const word = this.r32(pc);
      if ((word >>> 26) !== 49) return integerStep(pc, next);
      floating[(word >>> 16) & 31] = this.r32(this.r[(word >>> 21) & 31] + ((word << 16) >> 16));
      return [next, (next + 4) >>> 0];
    };
    const S = f.symbols, state = f.state, stack = 0x803ffe00, STOP = 0x80000100;
    const ram = at => (at + 0x800ab8c0) >>> 0;
    m.load(0x8016af80, rom.subarray(0x40e80, 0x66e10));
    m.load(ram(0x13c060), rom.subarray(0x13c060, 0x13c49c));
    const characters = Buffer.alloc(100 * 52 + 32, 0xa5);
    for (let id = 0; id < 100; id++) characters[id * 52 + 17] = 5;
    for (const id of [1, 2, 50, 97, 98].slice(0, freeCount)) characters[id * 52 + 17] = 0;
    characters[99 * 52 + 17] = 0; // Outside the original transient reset; do not allocate it.
    // These zero bytes just beyond the valid array must never supply characters.
    characters.fill(0, 100 * 52);
    m.load(S.lc_character_records, characters);
    const sources = Buffer.alloc(50 * 25, 0xa5);
    for (let id = 0; id < 50; id++) sources[id * 25 + 1] = id === freeSource ? 0 : 1;
    m.load(0x801971f0, sources);
    m.trap(S.lc_dma_read, c => {
      const at = c.get('a0'), size = c.get('a2');
      assert(at >= 0x2800000 && at + size <= rom.length);
      c.load(c.get('a1'), rom.subarray(at, at + size));
    });
    m.load(stack - 0x100, Buffer.alloc(0x180, 0x6c));
    const saved = ['s0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 'fp'];
    saved.forEach((r, i) => m.w32(stack + 0x40 + i * 4, 0xa1230000 + i));
    m.w32(stack + 0x64, STOP); m.set('sp', stack);
    m.set('s0', 41); m.set('s1', 0x80260000); m.set('s3', 1);
    m.set('s4', 29); m.set('s5', 0); m.set('s6', 7);
    m.set('v0', 71); m.set('v1', 0x801ed794 + 40 * 20 + 18);
    m.w8(S.lc_staged_scenario, 33);
    m.w32(0x80260008, 0x3f800000); m.w32(0x8026000c, 0x40000000);
    m.w32(0x801f0e04, 0x12345678); m.w32(0x801f0e00, 0x23456789);
    let spawns = 0;
    m.trap(0x801b8d44, c => {
      spawns++;
      assert.equal(c.r32(0x801f0e04), 261);
      assert.equal(c.r32(0x801f0e00), 7);
      assert.equal(c.r32(c.get('sp') + 16), 46);
      assert.equal(floating[12], 0x3f800000); assert.equal(floating[14], 0x40000000);
    });
    m.call(ram(0x13c414), STOP);
    const allowed = freeCount >= 2 && freeSource >= 30;
    assert.equal(spawns, allowed ? 1 : 0, `free=${freeCount}, source=${freeSource}`);
    assert.equal(m.r32(state), allowed ? 1 : 0);
    assert.equal(m.r32(state + 4), allowed ? 1 : 0);
    if (!allowed) {
      assert.equal(m.r32(0x801f0e04), 0x12345678);
      assert.equal(m.r32(0x801f0e00), 0x23456789);
    }
    assert.equal(m.get('sp'), stack + 0x68);
    saved.forEach((r, i) => assert.equal(m.get(r), 0xa1230000 + i));
    assert.deepEqual(Buffer.from(m.ram.slice(m.phys(S.lc_character_records),
      m.phys(S.lc_character_records) + characters.length)), characters);
    assert.deepEqual(Buffer.from(m.ram.slice(m.phys(0x801971f0),
      m.phys(0x801971f0) + sources.length)), sources);
    cases++;
  }
}
console.log(`PASS ${cases} installed custom capacity decisions through the original dispatcher: no spawn or record writes on insufficient character/source capacity; guarded scratch boundary and preserved return frame. Later world-state changes and full battles are outside this check.`);
