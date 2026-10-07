"use strict";
// Execute the exported selector against its actual cartridge library. Protects
// pool-stride expansion, original side choice, and custom squads in added pools.
const fs = require("node:fs"), assert = require("node:assert/strict");
const {Mips} = require("../../tests/lib/mips-interpreter.cjs");
const f = JSON.parse(fs.readFileSync(process.argv[2])), S = f.symbols;
const data = Buffer.from(f.data, "hex"), m = new Mips();
m.ram = new Uint8Array(0x400000); m.load(f.address, Buffer.from(f.code, "hex"));
m.trap(S.lc_dma_read, c => {
    const offset = c.get("a0") - f.dataOffset, size = c.get("a2");
    assert.ok(offset >= 0 && offset + size <= data.length);
    c.load(c.get("a1"), data.subarray(offset, offset + size));
});
function select(pool, slot, side, scenario) {
    m.w8(S.lc_staged_scenario, scenario); m.set("sp", 0x803ffe00);
    m.set("a0", 164); m.set("a1", pool);
    m.set("a2", 0x801ed794 + (pool - 1) * 20 + slot * 2 + side);
    m.call(f.entries.lc_custom_select, 0x80000100);
    return m.get("v0");
}
let checks = 0;
for (const pool of [1, 40, 45]) for (const slot of [0, 9, 10, 31]) for (const side of [0, 1]) {
    assert.equal(select(pool, slot, side, 1), (pool + slot + side) % 165);
    assert.equal(m.r32(S.lc_custom_state), 0); checks++;
}
assert.equal(select(40, 31, 1, 54), 5); // Added pool's authored leader overrides either ordinary choice.
assert.equal(m.r32(S.lc_custom_state), 1); checks++;
m.ram.fill(1, m.phys(S.lc_character_records), m.phys(S.lc_character_records + 100 * 52));
assert.equal(select(40, 31, 0, 54), 0); // Capacity refusal still occurs before staging.
assert.equal(m.r32(S.lc_custom_state), 0); checks++;
console.log(`PASS: ${checks} expanded-pool selector and capacity cases using exported data.`);
