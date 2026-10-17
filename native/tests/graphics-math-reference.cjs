'use strict';
// Read-only Rev 0 CPU reference. Execute the original sine/cosine bodies;
// do not substitute host libm. Code ranges are recorded in library-imports.json.
function loadGraphicsMath(cpu, rom) {
  for (const [ram, start, end] of [
    [0x800907f0, 0x20bf0, 0x20d40], [0x80092db0, 0x231b0, 0x23350],
    [0x800ae890, 0x3ec90, 0x3ef00]
  ]) cpu.load(ram, rom.subarray(start, end));
  // Qualified in accepted CURRENT171A02BF ELF: PT_LOAD VRAM800AD9C0,
  // physical ROM3DDC0, 5104 bytes contains both coefficient tables and the
  // perspective constants. This is a specific data segment, not a boot delta.
}
module.exports = {loadGraphicsMath};
