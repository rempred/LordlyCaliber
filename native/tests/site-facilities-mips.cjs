'use strict';
// Original mission site initializer and town-menu facility selector. Resource
// loading, rendering and the story-flag service are outside this bounded check.
const fs = require('node:fs'), assert = require('node:assert/strict');
const { Mips } = require('../../tests/lib/mips-interpreter.cjs');
const fixture = JSON.parse(fs.readFileSync(process.argv[2]));
const rom = fs.readFileSync(fixture.rom);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const descriptor = 0x387c0 + 7 * 40;
const base = rom.readUInt32BE(descriptor), start = rom.readUInt32BE(descriptor + 8);
const end = rom.readUInt32BE(descriptor + 12), delta = base - start;
assert.equal(base, 0x801ad5a0); assert.equal(start, 0x101ce0);
const live = offset => (offset + delta) >>> 0;
const templates = 0x801e89a8, site = 0x801951cc, record = 0x80300000;
// Template +108 dispatches Shop, +144 dispatches Witch's den.
assert.equal(rom.readUInt32BE(templates - delta + 108 + 24), live(0x10bb2c));
assert.equal(rom.readUInt32BE(templates - delta + 144 + 24), live(0x10b528));
let checked = 0;
for (const row of fixture.cases) for (const resume of [0, 1]) {
    const m = new Mips(); m.ram = new Uint8Array(0x400000);
    m.load(base, rom.subarray(start, end)); m.set('sp', 0x803ffe00);
    m.load(record, Buffer.from(row.town, 'hex'));
    m.w16(site, 9); m.w8(0x801f1038, resume);
    m.set('s0', 0); m.set('fp', record); m.set('s1', site + 4);
    m.set('s5', site); m.set('s6', site - 28); m.set('s2', row.addend);
    m.trap(0x80186210, c => { assert.equal(c.get('a0'), 17); c.set('v0', 0); });
    m.call(live(0x127998), live(0x1279e8));
    const bytes = Buffer.from(row.town, 'hex');
    assert.equal(m.r16(site), resume ? 9 : bytes.readUInt16BE(25) + row.addend,
                 `${row.name}: fresh entry loads the flags; resume preserves them`);
    const picked = [];
    m.set('fp', 0); m.set('s0', 0); m.set('s1', 0); m.set('s4', 0); m.set('s6', 0);
    m.trap(0x80093060, c => {
        assert.equal(c.get('a2'), 36);
        picked.push(c.get('a0') - templates);
    });
    m.call(live(0x11d000), live(0x11d08c));
    assert.deepEqual(picked, resume ? [] : row.menu, `${row.name}: menu entry`);
    checked++;
}
console.log(`PASS ${checked} original initializer/menu cases: Shop, Witch's den, Shop priority, and suspend preservation. UI/rendering/services are doubles.`);
