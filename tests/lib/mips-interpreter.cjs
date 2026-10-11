'use strict';

// Minimal big-endian MIPS III integer interpreter for executing native module
// entry points in tests. It models 32-bit o32 code: loads, stores, ALU,
// multiply/divide, branches with delay slots, J/JAL/JR/JALR. Addresses are
// KSEG0/KSEG1 virtual and map to one flat 8 MiB RDRAM. Retail functions are
// modelled as traps: when the PC reaches a trap address the handler runs and
// execution returns to $ra.

const REG = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
  's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra'];

class Mips {
  constructor() {
    this.ram = new Uint8Array(0x800000);
    this.r = new Uint32Array(32);
    this.hi = 0;
    this.lo = 0;
    this.traps = new Map();
    this.steps = 0;
  }

  phys(address) {
    const a = (address >>> 0) & 0x1FFFFFFF;
    if (a + 4 > this.ram.length) throw new Error('RDRAM access out of range 0x' + (address >>> 0).toString(16));
    return a;
  }
  r8(a) { return this.ram[this.phys(a)]; }
  r16(a) { const p = this.phys(a); return (this.ram[p] << 8) | this.ram[p + 1]; }
  r32(a) {
    const p = this.phys(a);
    return ((this.ram[p] << 24) | (this.ram[p + 1] << 16) | (this.ram[p + 2] << 8) | this.ram[p + 3]) >>> 0;
  }
  w8(a, v) { this.ram[this.phys(a)] = v & 0xFF; }
  w16(a, v) { const p = this.phys(a); this.ram[p] = (v >>> 8) & 0xFF; this.ram[p + 1] = v & 0xFF; }
  w32(a, v) {
    const p = this.phys(a);
    this.ram[p] = (v >>> 24) & 0xFF; this.ram[p + 1] = (v >>> 16) & 0xFF;
    this.ram[p + 2] = (v >>> 8) & 0xFF; this.ram[p + 3] = v & 0xFF;
  }
  load(address, bytes) { this.ram.set(bytes, this.phys(address)); }
  get(name) { return this.r[REG.indexOf(name)]; }
  set(name, value) { if (name !== 'zero') this.r[REG.indexOf(name)] = value >>> 0; }
  trap(address, handler) { this.traps.set(address >>> 0, handler); }

  // Calls `entry` with ra set to `ra` (default: the stop address) and runs
  // until the PC reaches `returnAddress`.
  call(entry, returnAddress = 0x80000100, limit = 200000, ra = returnAddress) {
    this.set('ra', ra);
    let pc = entry >>> 0;
    let next = (pc + 4) >>> 0;
    this.steps = 0;
    while (pc !== returnAddress) {
      if (++this.steps > limit) throw new Error('step limit at 0x' + pc.toString(16));
      const trap = this.traps.get(pc);
      if (trap) {
        trap(this);
        pc = this.get('ra');
        next = (pc + 4) >>> 0;
        continue;
      }
      const result = this.exec(pc, next);
      pc = result[0];
      next = result[1];
    }
    return this;
  }

  exec(pc, next) {
    const w = this.r32(pc);
    const op = w >>> 26, rs = (w >>> 21) & 31, rt = (w >>> 16) & 31, rd = (w >>> 11) & 31;
    const sa = (w >>> 6) & 31, fn = w & 63, imm = w & 0xFFFF, simm = (imm << 16) >> 16;
    const r = this.r;
    const setr = (i, v) => { if (i) r[i] = v >>> 0; };
    let target = null;
    const branch = (cond) => { if (cond) target = (next + (simm << 2)) >>> 0; };
    let likely = false;
    switch (op) {
      case 0:
        switch (fn) {
          case 0: setr(rd, r[rt] << sa); break;
          case 2: setr(rd, r[rt] >>> sa); break;
          case 3: setr(rd, (r[rt] | 0) >> sa); break;
          case 4: setr(rd, r[rt] << (r[rs] & 31)); break;
          case 6: setr(rd, r[rt] >>> (r[rs] & 31)); break;
          case 7: setr(rd, (r[rt] | 0) >> (r[rs] & 31)); break;
          case 8: target = r[rs]; break;
          case 9: setr(rd || 31, next + 4); target = r[rs]; break;
          case 16: setr(rd, this.hi); break;
          case 18: setr(rd, this.lo); break;
          case 24: { const p = BigInt.asIntN(64, BigInt(r[rs] | 0) * BigInt(r[rt] | 0));
            this.lo = Number(p & 0xFFFFFFFFn) >>> 0; this.hi = Number((p >> 32n) & 0xFFFFFFFFn) >>> 0; break; }
          case 25: { const p = BigInt(r[rs]) * BigInt(r[rt]);
            this.lo = Number(p & 0xFFFFFFFFn) >>> 0; this.hi = Number((p >> 32n) & 0xFFFFFFFFn) >>> 0; break; }
          case 26: if (r[rt]) { this.lo = ((r[rs] | 0) / (r[rt] | 0)) | 0; this.hi = (r[rs] | 0) % (r[rt] | 0); } break;
          case 27: if (r[rt]) { this.lo = Math.floor(r[rs] / r[rt]) >>> 0; this.hi = (r[rs] % r[rt]) >>> 0; } break;
          case 33: setr(rd, r[rs] + r[rt]); break;
          case 32: setr(rd, r[rs] + r[rt]); break;
          case 35: setr(rd, r[rs] - r[rt]); break;
          case 34: setr(rd, r[rs] - r[rt]); break;
          case 36: setr(rd, r[rs] & r[rt]); break;
          case 37: setr(rd, r[rs] | r[rt]); break;
          case 38: setr(rd, r[rs] ^ r[rt]); break;
          case 39: setr(rd, ~(r[rs] | r[rt])); break;
          case 42: setr(rd, (r[rs] | 0) < (r[rt] | 0) ? 1 : 0); break;
          case 43: setr(rd, r[rs] < r[rt] ? 1 : 0); break;
          case 13: throw new Error('break at 0x' + pc.toString(16));
          default: throw new Error('SPECIAL fn ' + fn + ' at 0x' + pc.toString(16));
        }
        break;
      case 1:
        if (rt === 0) branch((r[rs] | 0) < 0);
        else if (rt === 1) branch((r[rs] | 0) >= 0);
        else if (rt === 2) { likely = true; branch((r[rs] | 0) < 0); }
        else if (rt === 3) { likely = true; branch((r[rs] | 0) >= 0); }
        else if (rt === 17) { setr(31, next + 4); branch((r[rs] | 0) >= 0); }
        else throw new Error('REGIMM ' + rt + ' at 0x' + pc.toString(16));
        break;
      case 2: target = ((next & 0xF0000000) | ((w & 0x03FFFFFF) << 2)) >>> 0; break;
      case 3: setr(31, next + 4); target = ((next & 0xF0000000) | ((w & 0x03FFFFFF) << 2)) >>> 0; break;
      case 4: branch(r[rs] === r[rt]); break;
      case 5: branch(r[rs] !== r[rt]); break;
      case 6: branch((r[rs] | 0) <= 0); break;
      case 7: branch((r[rs] | 0) > 0); break;
      case 20: likely = true; branch(r[rs] === r[rt]); break;
      case 21: likely = true; branch(r[rs] !== r[rt]); break;
      case 22: likely = true; branch((r[rs] | 0) <= 0); break;
      case 23: likely = true; branch((r[rs] | 0) > 0); break;
      case 8: case 9: setr(rt, r[rs] + simm); break;
      case 10: setr(rt, (r[rs] | 0) < simm ? 1 : 0); break;
      case 11: setr(rt, r[rs] < (simm >>> 0) ? 1 : 0); break;
      case 12: setr(rt, r[rs] & imm); break;
      case 13: setr(rt, r[rs] | imm); break;
      case 14: setr(rt, r[rs] ^ imm); break;
      case 15: setr(rt, imm << 16); break;
      case 32: setr(rt, (this.r8(r[rs] + simm) << 24) >> 24); break;
      case 33: setr(rt, (this.r16(r[rs] + simm) << 16) >> 16); break;
      case 35: setr(rt, this.r32(r[rs] + simm)); break;
      case 36: setr(rt, this.r8(r[rs] + simm)); break;
      case 37: setr(rt, this.r16(r[rs] + simm)); break;
      case 40: this.w8(r[rs] + simm, r[rt]); break;
      case 41: this.w16(r[rs] + simm, r[rt]); break;
      case 43: this.w32(r[rs] + simm, r[rt]); break;
      case 47: break; // cache
      default: throw new Error('opcode ' + op + ' at 0x' + pc.toString(16));
    }
    if (target === null) {
      if (likely) return [(next + 4) >>> 0, (next + 8) >>> 0]; // likely not taken: skip delay slot
      return [next, (next + 4) >>> 0];
    }
    // Execute the delay slot, then continue at the target.
    const delay = this.exec(next, (next + 4) >>> 0);
    if (delay[0] !== ((next + 4) >>> 0)) throw new Error('branch in delay slot at 0x' + next.toString(16));
    return [target, (target + 4) >>> 0];
  }
}

module.exports = { Mips, REG };
