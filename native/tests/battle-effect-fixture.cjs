'use strict';
// Initialize the native effect pools for a battle fixture. The same machine
// runs effect callbacks, pose advancement, command playback and HP updates.
class BattleEffectFixture {
 constructor(machine, rom) {
  this.m = machine;
  // This is the normal combat overlay group. The continuation between
  // descriptors 10 and 11 is already mounted by AttackPlaybackMachine.
  for (const id of [2, 4, 11, 12, 13, 14]) {
   const at = 0x387c0 + id * 40;
   machine.load(rom.readUInt32BE(at), rom.subarray(rom.readUInt32BE(at + 8), rom.readUInt32BE(at + 12)));
  }
  machine.trap(0x80089f80, c => {
   const at = c.get('a0') & 0x0fffffff, size = c.get('a2');
   if (at + size > rom.length) throw Error('Native effect resource outside the ROM');
   c.load(c.get('a1'), rom.subarray(at, at + size));
  });
  // Use a stable viewport like the native editor preview. The game still
  // updates every effect task, contact callback and completion-ring entry.
  machine.trap(0x8020ac50, () => {}); // func_0025F800 (camera matrices).
 }
 reset() {
  const m = this.m;
  const chain = (base, count, stride, offset = 0) => {
   for (let i = 0; i < count; i++) m.w16(base + i * stride + offset, i === count - 1 ? 65535 : i + 1);
  };
  m.w32(0x80220d80, 24);
  m.w32(0x80220d84, 0x80700000);
  m.w32(0x80220d90, 24);
  m.w32(0x80220d94, 0x80700000 + 24 * 156);
  chain(0x80700000, 24, 156);
  m.w32(0x80220d68, 32);
  m.w32(0x80220d6c, 0x80760000);
  m.w32(0x80220d70, 0x807a0000);
  chain(0x80760000, 32, 148);
  m.w32(0x80220da4, 256);
  m.w32(0x80220da8, 0x80790000);
  m.w32(0x80220dac, 0x807a0400);
  chain(0x80790000, 256, 120);
  m.w32(0x80220d54, 256);
  m.w32(0x80220d58, 0x807b0000);
  chain(0x807b0000, 256, 128);
  m.w32(0x80220e14, 0x807c0000);
  chain(0x807c0000, 64, 16, 12);
  m.w32(0x80220ca0, 128);
  m.w32(0x80220ca8, 0x80770000);
  m.w32(0x80220cac, 32);
  chain(0x80770000, 128, 20, 16);
  m.w32(0x80220f20, 0x80780000);
  for (const base of [0x80220ee0, 0x80220ff0])
   for (let i = 0; i < 16; i++) m.writeFloat(base + i * 4, i % 5 === 0 ? 1 : 0);
  m.writeFloat(0x80220e90, 1 / 160);
  m.writeFloat(0x80220ea4, 1 / 120);
  m.writeFloat(0x80220eb8, 1 / 1000);
  m.writeFloat(0x80220ecc, 1);
  m.writeFloat(0x80220e20, .75);
  m.writeFloat(0x80220e34, .75);
  m.writeFloat(0x80220e44, -.15);
  m.writeFloat(0x80220e48, 1);
  m.writeFloat(0x80220e54, -43.2);
  m.writeFloat(0x80220e5c, 1);
  // Retail battle callback table at normalized ROM 0x228D70. It includes
  // result consumption, effect completion and actor height queries.
  m.w32(0x801d0810, 0x801e5aa0);
  // The summon scripts sample a rendered battle backdrop. Supply an empty
  // RGBA5551 backdrop with the game's texture header and dimensions.
  m.w32(0x801d0770, 0x807d0000);
  m.w32(0x807d0000, 0x36340002);
  m.w16(0x807d0004, 320);
  m.w16(0x807d0006, 240);
  m.w16(0x800e810e, 12);
  this.buffers();
 }
 buffers() {
  this.m.w32(0x800e9ba0, 0x80600000);
  this.m.w32(0x801969b0, 0x80680000);
  this.m.w32(0x80197178, 0x806c0000);
 }
 advance(call) {
  this.buffers();
  call(0x8020b104, []); // func_0025FCB4. Game task update and retirement.
  call(0x8020e224, [0]);
  call(0x801b0300, []);
  call(0x8020e3a0, [1]);
 }
}
module.exports = { BattleEffectFixture };
