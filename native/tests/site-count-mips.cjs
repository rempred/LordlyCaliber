'use strict';
// Execute the original initializer prefix at descriptor 7's actual load address.
// Stops after the count store, before resource allocation or site construction.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const fixture = JSON.parse(fs.readFileSync(process.argv[2]));
const rom = fs.readFileSync(fixture.rom);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
else if (rom.readUInt32BE(0) === 0x40123780) rom.swap32();
const m = new Mips();
m.ram = new Uint8Array(0x400000);
m.load(0x801d30f0, rom.subarray(0x127830, 0x1278d0));
m.w8(0x800e7ac5, 0);
m.w32(0x800e7a90, 0x80300000);
for (const row of fixture.rows) {
    m.w8(0x8018f481, row.key);
    m.w32(0x80300018, row.points);
    m.w32(0x801f1070, 0xdeadbeef);
    m.set('sp', 0x803ffe00);
    m.call(0x801d30f0, 0x801d3190);
    assert.equal(m.r32(0x801f1070) >>> 0, row.expected,
                 `scenario ${row.key}, placement count ${row.points}`);
}
console.log(`PASS ${fixture.rows.length} original initializer count comparisons. ` +
            'Placement resource and scenario-key RAM are controlled inputs; ' +
            'this does not establish natural mission launch or later dynamic placement.');
