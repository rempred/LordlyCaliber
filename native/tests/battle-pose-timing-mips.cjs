'use strict';
// Compare the original pose duration estimator with actual frame advancement
// for retail classes, linked identities and both combat sides.
const fs = require('node:fs'), assert = require('node:assert/strict');
const { FloatMips } = require('./float-mips.cjs');
const { BattleBodyFixture } = require('./battle-body-fixture.cjs');
const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const m = new FloatMips(), sp = 0x807ff000;
const actor = 0x80600000, child = actor + 0x1000, marks = actor + 0x8000;
m.load(0x80070000, rom.subarray(0x400, 0x2f000));
m.load(0x8016af80, rom.subarray(0x40e80, 0x66e10));
m.load(0x801ad5a0, rom.subarray(0x1f0a30, 0x213b10));
m.load(0x801d0840, rom.subarray(0x213b10, 0x24bca0));
const body = new BattleBodyFixture(m, rom);
m.trap(0x8009c7cc, c => c.set('v0', 0));
const call = (pc, args) => {
 m.set('sp', sp);
 args.forEach((value, index) => index < 4 ? m.set('a' + index, value) : m.w32(sp + 16 + (index - 4) * 4, value));
 m.call(pc, 0x80000100, 1000000);
 assert.equal(m.get('sp'), sp);
 return m.get('v0') | 0;
};
const rows = [];
const looping = new Set([5, 8, 11, 17, 21, 23, 25, 32, 34, 38]);
for (const source of [2, 5, 18, 69, 81, 117, 118, 146, 150, 162, 164]) {
 for (const current of new Set([rom[0x5db14 + source * 72 + 69] || source, 5])) {
  for (const flags of [0, 0x500]) {
   body.reset(call);
   m.load(actor, Buffer.alloc(0xf8));
   m.w32(actor, child);
   m.w32(actor + 0x40, flags);
   m.w32(actor + 0x48, source);
   m.w32(actor + 0x4c, current);
   m.w32(child + 0x40, actor);
   body.register(call, 0, actor, source, current, flags);
   for (const pose of [5, 6, 8, 11, 12, 17, 18, 21, 22, 23, 24, 25, 26, 29, 32, 33, 34, 35, 38, 39, 40]) {
    m.load(child, Buffer.alloc(0x200));
    m.w32(child + 0x40, actor);
    m.w32(child + 0x90, child);
    m.load(marks, Buffer.alloc(128, 0xff));
    call(0x801ad9d4, [child + 0x44, pose]);
    const duration = call(0x801df2ac, [child + 0x44, pose, marks]);
    const contacts = [];
    for (let i = 0; i < 32 && m.r32(marks + i * 4) !== 0xffffffff; i++)
     contacts.push(m.r32(marks + i * 4));
    const frames = [];
    let completion = null;
    for (let tick = 0; tick < Math.max(duration + 5, 12); tick++) {
     call(0x801adcbc, [actor]);
     frames.push([m.r16(child + 0x48), m.r16(child + 0x4a), m.r16(child + 0x4e)]);
     if (m.r16(child + 0x48) % 50 === 0) {
      completion = tick;
      break;
     }
    }
    const label = `class ${source}/${current}, flags ${flags}, pose ${pose}`;
    assert(duration >= 0 && contacts.every(tick => tick >= 0 && tick <= duration), label);
    if (completion === null) assert(looping.has(pose), `expected a finite pose, ${label}`);
    else assert.equal(completion, duration, `duration estimator disagrees with native advancement, ${label}`);
    rows.push({ source, current, flags, pose, duration, contacts, completion, frames });
   }
  }
 }
}
assert.equal(rows.length, 882);
const finished = rows.filter(row => row.completion !== null).length;
assert.equal(finished, 498, 'retail finite-pose census');
if (process.argv[3]) fs.writeFileSync(process.argv[3], JSON.stringify(rows));
console.log(`${rows.length} retail pose checks, ${finished} finished at the estimated duration, ${rows.length - finished} deliberate holds.`);
