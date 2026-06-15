/*
 * squadblob.js — per-scenario squad override engine (editor-side).
 *
 * Ports the proven Python builders (tools/build_modloader_*.py + mips_encode.py)
 * that were validated in-game (edat 298, count/grow tests, edat 13 cross-mission).
 * Emits the three ROM writes that install the runtime override system:
 *   1. trampoline  jal 0x80097fc4  at ROM 0x195584  (record-builder hook)
 *   2. bootstrap   at ROM 0x283c4 / RAM 0x80097fc4  (uncached sentinel + PI-DMA
 *                  the blob from ROM tail into the free upper 4MB, then jump in)
 *   3. blob        at ROM tail 0x2780000 -> RAM 0x80400000
 *                  (sentinel + count + resolver + override table)
 *
 * Blob layout (multi-entry; resolver-first so the table can grow):
 *   +0x000 sentinel 'OBMR'
 *   +0x004 entryCount (u32 BE)
 *   +0x008 resolver code  (bootstrap jumps here)
 *   +0x100 entries: each 40B = gateId(1) matchA(1) matchB(1) matchC(1) record[35] pad(1)
 *
 * The resolver, per entry: gate on the live runtime scenario key (0x801936A7),
 * match the template classA/B/C at [s0], and on a hit memcpy the 35-byte record
 * over [s0].
 */
(function (OB64) {
  'use strict';

  // ---- constants (all proven in-game) ----
  var HOOK_ROM   = 0x195584;     // record-builder trampoline site (outside CRC)
  var BOOT_ROM   = 0x283C4;      // bootstrap cave (z64; inside CRC -> recalc)
  var BOOT_RAM   = 0x80097FC4;   // bootstrap runtime address (linear, resident)
  var TAIL_Z64   = 0x2780000;    // blob home in ROM tail padding (outside CRC)
  var PI_CART    = (0x10000000 + TAIL_Z64) >>> 0;  // 0x12780000 (cartridge phys)
  var MOD_BASE   = 0x80400000;   // free upper RAM (Expansion Pak)
  var SENTINEL   = 0x4F424D52;   // 'OBMR'
  var DISP_SLTIU = 0x2EA2001E;   // sltiu $v0,$s5,30  (displaced from 0x195584)
  var DISP_XORI  = 0x38420001;   // xori  $v0,$v0,1   (displaced from 0x195588)
  var RESOLVER_OFF = 0x08;       // resolver immediately after sentinel+count
  var ENTRIES_OFF  = 0x100;      // table fixed past the resolver (resolver < 0xF8B)
  var ENTRY_STRIDE = 40;
  var REC_LEN = 35;

  // ---- MIPS R4300i encoder (port of mips_encode.py) ----
  var REG = {
    zero:0, at:1, v0:2, v1:3, a0:4, a1:5, a2:6, a3:7,
    t0:8, t1:9, t2:10, t3:11, t4:12, t5:13, t6:14, t7:15,
    s0:16, s1:17, s2:18, s3:19, s4:20, s5:21, s6:22, s7:23,
    t8:24, t9:25, k0:26, k1:27, gp:28, sp:29, s8:30, ra:31
  };
  function r(x) { return (typeof x === 'string') ? REG[x] : (x | 0); }
  function i16(v) { return v & 0xFFFF; }
  var M = {
    nop:  function () { return 0; },
    lui:  function (rt, imm) { return ((0x0F << 26) | (r(rt) << 16) | i16(imm)) >>> 0; },
    ori:  function (rt, rs, imm) { return ((0x0D << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    andi: function (rt, rs, imm) { return ((0x0C << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    addiu:function (rt, rs, imm) { return ((0x09 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    lw:   function (rt, imm, rs) { return ((0x23 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    lbu:  function (rt, imm, rs) { return ((0x24 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    lhu:  function (rt, imm, rs) { return ((0x25 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    sb:   function (rt, imm, rs) { return ((0x28 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    sh:   function (rt, imm, rs) { return ((0x29 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    sw:   function (rt, imm, rs) { return ((0x2B << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(imm)) >>> 0; },
    beq:  function (rs, rt, off) { return ((0x04 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(off)) >>> 0; },
    bne:  function (rs, rt, off) { return ((0x05 << 26) | (r(rs) << 21) | (r(rt) << 16) | i16(off)) >>> 0; },
    blez: function (rs, off) { return ((0x06 << 26) | (r(rs) << 21) | i16(off)) >>> 0; },
    jr:   function (rs) { return ((r(rs) << 21) | 0x08) >>> 0; },
    jal:  function (tgt) { return ((0x03 << 26) | ((tgt >>> 2) & 0x03FFFFFF)) >>> 0; }
  };

  // ---- two-pass label assembler ----
  // line forms: ['label', name] | ['raw', word] | [op, ...args]
  //   branches: ['bne'|'beq', rs, rt, labelName] | ['blez', rs, labelName]
  function assemble(base, lines) {
    var addr = base >>> 0, labels = {}, flat = [], i;
    for (i = 0; i < lines.length; i++) {
      if (lines[i][0] === 'label') labels[lines[i][1]] = addr;
      else { flat.push([addr, lines[i]]); addr = (addr + 4) >>> 0; }
    }
    var out = [];
    for (i = 0; i < flat.length; i++) {
      var a = flat[i][0], ln = flat[i][1], op = ln[0];
      if (op === 'raw') { out.push(ln[1] >>> 0); continue; }
      if (op === 'bne' || op === 'beq') {
        out.push(M[op](ln[1], ln[2], ((labels[ln[3]] - (a + 4)) | 0) >> 2));
      } else if (op === 'blez') {
        out.push(M.blez(ln[1], ((labels[ln[2]] - (a + 4)) | 0) >> 2));
      } else {
        out.push(M[op].apply(null, ln.slice(1)));
      }
    }
    return out;
  }

  // ---- resolver: per-entry gate + classA/B/C match + 35B memcpy ----
  function buildResolver() {
    var lines = [
      ['lui', 't3', 0x8040],                 // t3 = MOD_BASE
      ['lw', 't4', 4, 't3'],                  // t4 = entryCount
      ['ori', 't5', 't3', ENTRIES_OFF],       // t5 = &entry[0]
      ['lui', 't8', 0x8019],
      ['lbu', 't7', 0x36A7, 't8'],            // t7 = live runtime scenario key
      ['label', 'loop'],
      ['blez', 't4', 'done'],
      ['nop'],
      ['lbu', 't0', 0, 't5'],                 // entry.gate
      ['bne', 't0', 't7', 'next'],
      ['nop'],
      ['lbu', 't0', 0, 's0'], ['lbu', 't6', 1, 't5'], ['bne', 't0', 't6', 'next'], ['nop'],
      ['lbu', 't0', 7, 's0'], ['lbu', 't6', 2, 't5'], ['bne', 't0', 't6', 'next'], ['nop'],
      ['lbu', 't0', 16, 's0'], ['lbu', 't6', 3, 't5'], ['bne', 't0', 't6', 'next'], ['nop'],
      ['addiu', 't1', 't5', 4],               // src = &record
      ['ori', 't2', 's0', 0],                 // dst = s0
      ['ori', 't9', 'zero', REC_LEN],
      ['label', 'cpy'],
      ['lbu', 't0', 0, 't1'],
      ['sb', 't0', 0, 't2'],
      ['addiu', 't1', 't1', 1],
      ['addiu', 't2', 't2', 1],
      ['addiu', 't9', 't9', -1],
      ['bne', 't9', 'zero', 'cpy'],
      ['nop'],
      ['label', 'next'],
      ['addiu', 't5', 't5', ENTRY_STRIDE],
      ['addiu', 't4', 't4', -1],
      ['beq', 'zero', 'zero', 'loop'],
      ['nop'],
      ['label', 'done'],
      ['raw', DISP_SLTIU],
      ['raw', DISP_XORI],
      ['jr', 'ra'],
      ['nop']
    ];
    return assemble((MOD_BASE + RESOLVER_OFF) >>> 0, lines);
  }

  // ---- bootstrap (CRC cave): uncached sentinel -> PI-DMA -> jump to resolver ----
  function buildBootstrap(blobLen) {
    if ((blobLen - 1) > 0xFFFF) throw new Error('blob too large for single-imm PI length');
    var lines = [
      ['lui', 't0', 0xA040],
      ['lw', 't1', 0, 't0'],                  // sentinel (uncached 0xA0400000)
      ['lui', 't2', (SENTINEL >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', SENTINEL & 0xFFFF],
      ['beq', 't1', 't2', 'loaded'],
      ['nop'],
      ['lui', 't0', 0xA460],                  // PI regs base
      ['label', 'w1'],
      ['lw', 't1', 0x10, 't0'],               // PI_STATUS
      ['andi', 't1', 't1', 3],
      ['bne', 't1', 'zero', 'w1'],
      ['nop'],
      ['lui', 't1', 0x0040],                  // PI_DRAM_ADDR = 0x00400000
      ['sw', 't1', 0, 't0'],
      ['lui', 't1', (PI_CART >>> 16) & 0xFFFF],  // PI_CART_ADDR = 0x12780000
      ['sw', 't1', 4, 't0'],
      ['ori', 't1', 'zero', (blobLen - 1) & 0xFFFF],
      ['sw', 't1', 0x0C, 't0'],               // PI_WR_LEN -> trigger
      ['label', 'w2'],
      ['lw', 't1', 0x10, 't0'],
      ['andi', 't1', 't1', 3],
      ['bne', 't1', 'zero', 'w2'],
      ['nop'],
      ['label', 'loaded'],
      ['lui', 't0', 0x8040],
      ['ori', 't0', 't0', RESOLVER_OFF],
      ['jr', 't0'],
      ['nop']
    ];
    var words = assemble(BOOT_RAM, lines);
    if (words.length * 4 > 108) throw new Error('bootstrap exceeds the 108B cave');
    return words;
  }

  function wordsToBytes(words) {
    var b = new Uint8Array(words.length * 4);
    for (var i = 0; i < words.length; i++) {
      b[i * 4] = (words[i] >>> 24) & 0xFF;
      b[i * 4 + 1] = (words[i] >>> 16) & 0xFF;
      b[i * 4 + 2] = (words[i] >>> 8) & 0xFF;
      b[i * 4 + 3] = words[i] & 0xFF;
    }
    return b;
  }

  // ---- blob (sentinel + count + resolver + table) ----
  // overrides: [{ gateId:int, matchSig:[a,b,c], record:Uint8Array(35) }]
  function buildBlob(overrides) {
    var resolver = buildResolver();
    var resolverEnd = RESOLVER_OFF + resolver.length * 4;
    if (resolverEnd > ENTRIES_OFF) throw new Error('resolver overruns the table offset');
    var n = overrides.length;
    var size = ENTRIES_OFF + n * ENTRY_STRIDE;
    while (size % 8) size++;
    var blob = new Uint8Array(size);
    // header
    blob[0] = (SENTINEL >>> 24) & 0xFF; blob[1] = (SENTINEL >>> 16) & 0xFF;
    blob[2] = (SENTINEL >>> 8) & 0xFF; blob[3] = SENTINEL & 0xFF;
    blob[4] = (n >>> 24) & 0xFF; blob[5] = (n >>> 16) & 0xFF;
    blob[6] = (n >>> 8) & 0xFF; blob[7] = n & 0xFF;
    // resolver
    blob.set(wordsToBytes(resolver), RESOLVER_OFF);
    // entries
    for (var i = 0; i < n; i++) {
      var o = overrides[i], off = ENTRIES_OFF + i * ENTRY_STRIDE;
      var gate = (o.gateId != null) ? o.gateId : o.gateLoc; // gateLoc kept for old tests/patches
      blob[off] = gate & 0xFF;
      blob[off + 1] = o.matchSig[0] & 0xFF;
      blob[off + 2] = o.matchSig[1] & 0xFF;
      blob[off + 3] = o.matchSig[2] & 0xFF;
      blob.set(o.record.subarray(0, REC_LEN), off + 4);
    }
    return blob;
  }

  // ---- top-level: the three ROM writes for the export pipeline ----
  // Returns { writes: [{offset, label, bytes:Uint8Array}], crcWindow: true }.
  // crcWindow is true because the bootstrap cave lies inside z64 0x1000-0x101000.
  function buildSquadOverrideWrites(overrides) {
    var blob = buildBlob(overrides);
    var boot = buildBootstrap(blob.length);
    var tramp = new Uint8Array(8);
    tramp.set(wordsToBytes([M.jal(BOOT_RAM), M.nop()]), 0);
    return {
      crcWindow: true,
      writes: [
        { offset: HOOK_ROM, label: 'record-builder trampoline', bytes: tramp },
        { offset: BOOT_ROM, label: 'bootstrap (DMA loader)', bytes: wordsToBytes(boot) },
        { offset: TAIL_Z64, label: 'override blob (' + overrides.length + ' entries)', bytes: blob }
      ]
    };
  }

  // Restore the three patch sites to retail (used when the last override is
  // removed): trampoline back to the displaced sltiu/xori, clear the cave, and
  // zero the blob region. Only the trampoline + cave matter (the cave is in the
  // CRC window); the tail is cleared for tidiness (outside CRC).
  function restoreVanilla(z64) {
    z64.set(wordsToBytes([DISP_SLTIU, DISP_XORI]), HOOK_ROM);
    var i;
    for (i = 0; i < 108; i++) z64[BOOT_ROM + i] = 0;
    for (i = 0; i < 0x10000; i++) z64[TAIL_Z64 + i] = 0;
  }

  // ---- record <-> high-level squad spec (for the UI) ----
  // spec = { leader:{cls,cell,equip}, classB:{cls,equip,cells:[]}, classC:{cls,equip,cells:[]} }
  function recordFromSpec(spec) {
    var rec = new Uint8Array(REC_LEN), i;
    rec[0] = spec.leader.cls & 0xFF;
    rec[1] = 0x01;                       // B1 = 1 (matches all proven replacement records)
    rec[3] = (spec.leader.equip || 0) & 0xFF;
    rec[6] = spec.leader.cell & 0xFF;
    if (spec.classB && spec.classB.cells && spec.classB.cells.length) {
      rec[7] = spec.classB.cls & 0xFF;
      rec[8] = (spec.classB.equip || 0) & 0xFF;
      for (i = 0; i < spec.classB.cells.length && i < 3; i++) rec[13 + i] = spec.classB.cells[i] & 0xFF;
    }
    if (spec.classC && spec.classC.cells && spec.classC.cells.length) {
      rec[16] = spec.classC.cls & 0xFF;
      rec[17] = (spec.classC.equip || 0) & 0xFF;
      for (i = 0; i < spec.classC.cells.length && i < 2; i++) rec[22 + i] = spec.classC.cells[i] & 0xFF;
    }
    return rec;
  }

  function specFromRecord(rec) {
    function nz(arr) { var o = []; for (var i = 0; i < arr.length; i++) if (rec[arr[i]]) o.push(rec[arr[i]]); return o; }
    return {
      leader: { cls: rec[0], cell: rec[6], equip: rec[3] },
      classB: { cls: rec[7], equip: rec[8], cells: nz([13, 14, 15]) },
      classC: { cls: rec[16], equip: rec[17], cells: nz([22, 23]) }
    };
  }

  function memberCount(rec) {
    var c = 1, k, cells = [13, 14, 15, 22, 23];
    for (k = 0; k < cells.length; k++) if (rec[cells[k]]) c++;
    return c;
  }

  OB64.squad = {
    buildSquadOverrideWrites: buildSquadOverrideWrites,
    restoreVanilla: restoreVanilla,
    buildBlob: buildBlob,
    buildBootstrap: buildBootstrap,
    recordFromSpec: recordFromSpec,
    specFromRecord: specFromRecord,
    memberCount: memberCount,
    wordsToBytes: wordsToBytes,
    _enc: M, _assemble: assemble,
    consts: {
      HOOK_ROM: HOOK_ROM, BOOT_ROM: BOOT_ROM, BOOT_RAM: BOOT_RAM,
      TAIL_Z64: TAIL_Z64, PI_CART: PI_CART, MOD_BASE: MOD_BASE,
      SENTINEL: SENTINEL, ENTRIES_OFF: ENTRIES_OFF, ENTRY_STRIDE: ENTRY_STRIDE
    }
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = OB64;
})(typeof OB64 !== 'undefined' ? OB64 : (typeof window !== 'undefined' ? (window.OB64 = window.OB64 || {}) : (this.OB64 = this.OB64 || {})));
