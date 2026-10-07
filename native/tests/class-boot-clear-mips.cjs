'use strict';
// Execute the real cartridge entry, before the resident loader. This catches
// moving the BSS clear together with a table which happens to share its base.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
for (const file of process.argv.slice(2)) {
  const rom = fs.readFileSync(file);
  if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
  assert.equal(rom.readUInt32BE(0), 0x80371240);
  const m = new Mips(); m.ram = new Uint8Array(0x400000); m.ram.fill(0xa5);
  m.load(0x80070c00, rom.subarray(0x1000, 0x1060));
  const expected = m.ram.slice(); expected.fill(0, 0xaedb0, 0xe9c20);
  m.call(0x80070c00, 0x8007f880);
  const mismatch = m.ram.findIndex((byte, i) => byte !== expected[i]);
  assert.equal(mismatch, -1, `boot clear changed the wrong memory at RAM 0x${(0x80000000 + mismatch).toString(16)}: ${file}`);
  assert.equal(m.get('sp'), 0x800c6d60);
  console.log(`PASS cartridge boot clear and initial stack: ${file}`);
}
