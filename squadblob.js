/*
 * squadblob.js — per-scenario squad override engine (editor-side).
 *
 * Ports the proven Python builders (tools/build_modloader_*.py + mips_encode.py)
 * that were validated in-game (edat 298, count/grow tests, edat 13 cross-mission).
 * Emits the three ROM writes that install the runtime override system:
 *   1. trampoline  jal 0x80097fc4  at the revision-specific record-builder hook
 *                  (header rev 0 ROM 0x195584; header rev 1 ROM 0x1955A4)
 *   2. bootstrap   at ROM 0x283c4 / RAM 0x80097fc4  (uncached sentinel + PI-DMA
 *                  the blob from ROM tail into the free upper 4MB, then jump in)
 *   3. blob        at ROM tail 0x2780000 -> RAM 0x80400000
 *                  (sentinel + count + resolver + override table)
 *
 * Blob layout (multi-entry; resolver-first so the table can grow):
 *   +0x000 sentinel 'OBMR'
 *   +0x004 entryCount (u32 BE)
 *   +0x008 resolver code  (bootstrap jumps here)
 *   +0x100 entries: each 72B = gateId(1) original[35] replacement[35] pad(1)
 *
 * The resolver, per entry: gate on the live runtime scenario key (0x801936A7),
 * byte-match the live template at [s0] against original[35], and on a hit memcpy
 * replacement[35] over [s0].
 */
(function (OB64) {
  'use strict';

  // ---- constants (header rev 0 proven in-game; header rev 1 hook is the same routine at +0x20) ----
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
  var ENTRY_STRIDE = 72;
  var REC_LEN = 35;
  var ENTRY_ORIGINAL_OFF = 1;
  var ENTRY_REPLACEMENT_OFF = 36;

  // ---- cache-coherency hardening ----
  // The N64 CPU's instruction/data caches do not observe PI DMA, so code DMA'd into RAM
  // must have its destination range invalidated before it executes (a stale I-cache line
  // executes garbage; a dirty D-cache line can write back OVER the fresh copy). The game's
  // own resident resource loader (RAM 0x800761E4-0x80076324) solves this by calling two
  // resident leaf helpers - I-cache invalidate then D-cache invalidate over (a0=start vaddr,
  // a1=length) - before its PI DMA. This module calls the same two helpers, mirroring the
  // shipped, hardware-exercised pattern instead of reimplementing CACHE-instruction loops.
  // Both helpers were confirmed by live register trace and static disassembly to be pure
  // (a0,a1) functions that clobber only $at,$t0-$t3, never write back a0/a1, and have no
  // $gp or stack dependency.
  var ICACHE_INVALIDATE_RAM = 0x800900C0;
  var DCACHE_INVALIDATE_RAM = 0x80090010;
  // The tiny trampoline cave (BOOT_ROM/BOOT_RAM, ~108B hard ceiling) has no room left for
  // the extra invalidate-call instructions, so the DMA-trigger logic plus the invalidate
  // calls live in a continuation in a larger shared code cave (z64 0x03054C..0x03086C,
  // verified clean 0x00 padding). The tiny cave becomes a bare sentinel-check dispatch that
  // `j`s (not `jal`s, so $ra survives for the module's own caller) into the continuation on
  // first load only; already-loaded re-entries take the unchanged fast path.
  // The shared cave is divided into 0xC0-byte slots so the Squads, High Attack, and Chaos
  // Frame features can each place a cache-invalidate continuation without colliding:
  //   slot 0 (this file, Squads):       z64 0x03054C / RAM 0x800A014C
  //   slot 1 (High Attack main):        z64 0x03060C / RAM 0x800A020C
  //   slot 3 (Chaos Frame):             z64 0x03078C / RAM 0x800A038C
  // Slot 2 (z64 0x0306CC / RAM 0x800A02CC) is reserved address space for High Attack's
  // slot0-completion bootstrap, a header-rev-1-only path; the rev-0/editor build never
  // writes or validates that slot.
  // Slot 0 (this file) is cold-boot regression tested: an editor export re-emitted through
  // this hardened bootstrap+continuation deployed all squads and applied every override on
  // a full cold boot. The High Attack and Chaos Frame slots are still static-build only.
  var CACHE_CONT_Z64 = 0x03054C;
  var CACHE_CONT_RAM = 0x800A014C;
  var CACHE_CONT_BYTES = 0xC0; // reserved slot; real usage is ~108B

  function patchLayout(romOrLayout) {
    var layout = romOrLayout && romOrLayout.layout ? romOrLayout.layout : romOrLayout;
    if (!layout && OB64.currentRomLayout) layout = OB64.currentRomLayout;
    var squadPatch = (layout && layout.squadPatch) || {};
    var out = {
      id: layout && layout.id ? layout.id : 'us-rev0',
      HOOK_ROM: HOOK_ROM,
      BOOT_ROM: BOOT_ROM,
      BOOT_RAM: BOOT_RAM,
      TAIL_Z64: TAIL_Z64,
      MOD_BASE: MOD_BASE,
      SENTINEL: SENTINEL,
      CACHE_CONT_Z64: CACHE_CONT_Z64,
      CACHE_CONT_RAM: CACHE_CONT_RAM
    };
    for (var k in squadPatch) out[k] = squadPatch[k];
    out.PI_CART = (0x10000000 + out.TAIL_Z64) >>> 0;
    out.MOD_PHYS = (out.MOD_BASE & 0x1FFFFFFF) >>> 0;
    return out;
  }

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
    j:    function (tgt) { return ((0x02 << 26) | ((tgt >>> 2) & 0x03FFFFFF)) >>> 0; },
    jal:  function (tgt) { return ((0x03 << 26) | ((tgt >>> 2) & 0x03FFFFFF)) >>> 0; },
    addu: function (rd, rs, rt) { return ((r(rs) << 21) | (r(rt) << 16) | (r(rd) << 11) | 0x21) >>> 0; }
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

  // ---- resolver: per-entry gate + original-35B match + replacement-35B memcpy ----
  function buildResolver(layout) {
    layout = layout || patchLayout();
    var lines = [
      ['lui', 't3', (layout.MOD_BASE >>> 16) & 0xFFFF], // t3 = MOD_BASE
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
      ['addiu', 't1', 't5', ENTRY_ORIGINAL_OFF], // expected vanilla template
      ['ori', 't2', 's0', 0],                 // live template
      ['ori', 't9', 'zero', REC_LEN],
      ['label', 'cmp'],
      ['lbu', 't0', 0, 't1'],
      ['lbu', 't6', 0, 't2'],
      ['bne', 't0', 't6', 'next'],
      ['nop'],
      ['addiu', 't1', 't1', 1],
      ['addiu', 't2', 't2', 1],
      ['addiu', 't9', 't9', -1],
      ['bne', 't9', 'zero', 'cmp'],
      ['nop'],
      ['addiu', 't1', 't5', ENTRY_REPLACEMENT_OFF], // src = &replacement
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
      ['beq', 'zero', 'zero', 'done'],        // one override per template; avoid replacement chaining
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
    return assemble((layout.MOD_BASE + RESOLVER_OFF) >>> 0, lines);
  }

  // ---- bootstrap dispatch (CRC cave, <=108B): sentinel check only ----
  // On first load, `j`s (not `jal`s - $ra must survive for the resolver's
  // eventual return through the record-builder hook) into the cache-invalidate
  // + PI-DMA continuation built by buildCacheInvalidateContinuation(), which
  // lives in the spacious cave because this tiny cave has no room left for
  // the extra invalidate-call instructions. Already-loaded re-entries (the
  // common case - this hook fires per squad build) take the unchanged fast
  // path straight to the resolver.
  function buildBootstrap(blobLen, layout) {
    layout = layout || patchLayout();
    if ((blobLen - 1) > 0xFFFF) throw new Error('blob too large for single-imm PI length');
    var lines = [
      ['lui', 't0', 0xA040],
      ['lw', 't1', 0, 't0'],                  // sentinel (uncached 0xA0400000)
      ['lui', 't2', (layout.SENTINEL >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', layout.SENTINEL & 0xFFFF],
      ['beq', 't1', 't2', 'loaded'],
      ['nop'],
      ['raw', M.j(layout.CACHE_CONT_RAM)],
      ['nop'],
      ['label', 'loaded'],
      ['lui', 't0', (layout.MOD_BASE >>> 16) & 0xFFFF],
      ['ori', 't0', 't0', RESOLVER_OFF],
      ['jr', 't0'],
      ['nop']
    ];
    var words = assemble(layout.BOOT_RAM, lines);
    if (words.length * 4 > 108) throw new Error('bootstrap exceeds the 108B cave');
    return words;
  }

  // ---- cache-invalidate + PI-DMA continuation (spacious cave) ----
  function buildCacheInvalidateContinuation(blobLen, layout) {
    layout = layout || patchLayout();
    if ((blobLen - 1) > 0xFFFF) throw new Error('blob too large for single-imm PI length');
    var lines = [
      ['addu', 't4', 'ra', 'zero'],            // stash real $ra (helpers clobber $at,t0-t3 only)
      ['lui', 'a0', (layout.MOD_BASE >>> 16) & 0xFFFF],
      ['ori', 'a1', 'zero', blobLen & 0xFFFF],
      ['raw', M.jal(ICACHE_INVALIDATE_RAM)],
      ['nop'],
      ['raw', M.jal(DCACHE_INVALIDATE_RAM)],   // a0/a1 survive: neither helper writes them back
      ['nop'],
      ['addu', 'ra', 't4', 'zero'],            // restore real $ra

      ['lui', 't0', 0xA460],                   // PI regs base
      ['label', 'w1'],
      ['lw', 't1', 0x10, 't0'],                // PI_STATUS
      ['andi', 't1', 't1', 3],
      ['bne', 't1', 'zero', 'w1'],
      ['nop'],
      ['lui', 't1', (layout.MOD_PHYS >>> 16) & 0xFFFF], // PI_DRAM_ADDR = module phys
      ['sw', 't1', 0, 't0'],
      ['lui', 't1', (layout.PI_CART >>> 16) & 0xFFFF],  // PI_CART_ADDR
      ['sw', 't1', 4, 't0'],
      ['ori', 't1', 'zero', (blobLen - 1) & 0xFFFF],
      ['sw', 't1', 0x0C, 't0'],               // PI_WR_LEN -> trigger
      ['label', 'w2'],
      ['lw', 't1', 0x10, 't0'],
      ['andi', 't1', 't1', 3],
      ['bne', 't1', 'zero', 'w2'],
      ['nop'],
      ['lui', 't0', (layout.MOD_BASE >>> 16) & 0xFFFF],
      ['ori', 't0', 't0', RESOLVER_OFF],
      ['jr', 't0'],
      ['nop']
    ];
    var words = assemble(layout.CACHE_CONT_RAM, lines);
    if (words.length * 4 > CACHE_CONT_BYTES) throw new Error('cache-invalidate continuation exceeds its reserved cave slot');
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
  // overrides: [{ gateId:int, original:Uint8Array(35), record:Uint8Array(35) }]
  function buildBlob(overrides, layout) {
    layout = layout || patchLayout();
    var resolver = buildResolver(layout);
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
      var original = o.original || o.originalRecord || o.matchRecord;
      if (!original || original.length !== REC_LEN) throw new Error('squad override ' + i + ' missing original 35-byte record');
      if (!o.record || o.record.length !== REC_LEN) throw new Error('squad override ' + i + ' missing replacement 35-byte record');
      blob[off] = gate & 0xFF;
      blob.set(original.subarray(0, REC_LEN), off + ENTRY_ORIGINAL_OFF);
      blob.set(o.record.subarray(0, REC_LEN), off + ENTRY_REPLACEMENT_OFF);
    }
    return blob;
  }

  // ---- top-level: the three ROM writes for the export pipeline ----
  // Returns { writes: [{offset, label, bytes:Uint8Array}], crcWindow: true }.
  // crcWindow is true because the bootstrap cave lies inside z64 0x1000-0x101000.
  function buildSquadOverrideWrites(overrides) {
    var layout = patchLayout(arguments.length > 1 ? arguments[1] : null);
    var blob = buildBlob(overrides, layout);
    var boot = buildBootstrap(blob.length, layout);
    var cont = buildCacheInvalidateContinuation(blob.length, layout);
    var tramp = new Uint8Array(8);
    tramp.set(wordsToBytes([M.jal(layout.BOOT_RAM), M.nop()]), 0);
    return {
      crcWindow: true,
      writes: [
        { offset: layout.HOOK_ROM, label: 'record-builder trampoline', bytes: tramp },
        { offset: layout.BOOT_ROM, label: 'bootstrap (sentinel dispatch)', bytes: wordsToBytes(boot) },
        { offset: layout.CACHE_CONT_Z64, label: 'cache-invalidate + DMA continuation', bytes: wordsToBytes(cont) },
        { offset: layout.TAIL_Z64, label: 'override blob (' + overrides.length + ' entries)', bytes: blob }
      ]
    };
  }

  // Restore the three patch sites to retail (used when the last override is
  // removed): trampoline back to the displaced sltiu/xori, clear the cave, and
  // zero the blob region. Only the trampoline + cave matter (the cave is in the
  // CRC window); the tail is cleared for tidiness (outside CRC).
  function restoreVanilla(z64, romOrLayout) {
    var layout = patchLayout(romOrLayout);
    z64.set(wordsToBytes([DISP_SLTIU, DISP_XORI]), layout.HOOK_ROM);
    var i;
    for (i = 0; i < 108; i++) z64[layout.BOOT_ROM + i] = 0;
    for (i = 0; i < CACHE_CONT_BYTES; i++) z64[layout.CACHE_CONT_Z64 + i] = 0;
    for (i = 0; i < 0x10000; i++) z64[layout.TAIL_Z64 + i] = 0;
  }

  function patchRegions(romOrLayout) {
    var layout = patchLayout(romOrLayout);
    return [
      { kind: 'rom', start: layout.HOOK_ROM, size: 8, label: 'record-builder trampoline' },
      { kind: 'rom', start: layout.BOOT_ROM, size: 108, label: 'bootstrap cave' },
      { kind: 'rom', start: layout.CACHE_CONT_Z64, size: CACHE_CONT_BYTES, label: 'cache-invalidate continuation cave' },
      { kind: 'rom', start: layout.TAIL_Z64, size: 0x10000, label: 'squad override tail lane' },
      { kind: 'ram', start: layout.BOOT_RAM, size: 108, label: 'bootstrap runtime' },
      { kind: 'ram', start: layout.CACHE_CONT_RAM, size: CACHE_CONT_BYTES, label: 'cache-invalidate continuation runtime' },
      { kind: 'ram', start: layout.MOD_BASE, size: 0x10000, label: 'squad override runtime lane' },
    ];
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
      for (i = 0; i < spec.classC.cells.length && i < 3; i++) rec[22 + i] = spec.classC.cells[i] & 0xFF;
    }
    return rec;
  }

  function specFromRecord(rec) {
    function nz(arr) { var o = []; for (var i = 0; i < arr.length; i++) if (rec[arr[i]]) o.push(rec[arr[i]]); return o; }
    return {
      leader: { cls: rec[0], cell: rec[6], equip: rec[3] },
      classB: { cls: rec[7], equip: rec[8], cells: nz([13, 14, 15]) },
      classC: { cls: rec[16], equip: rec[17], cells: nz([22, 23, 24]) }
    };
  }

  function memberCount(rec) {
    var c = 1, k, cells = [13, 14, 15, 22, 23, 24];
    for (k = 0; k < cells.length; k++) if (rec[cells[k]]) c++;
    return c;
  }

  OB64.squad = {
    buildSquadOverrideWrites: buildSquadOverrideWrites,
    restoreVanilla: restoreVanilla,
    patchRegions: patchRegions,
    buildBlob: buildBlob,
    buildBootstrap: buildBootstrap,
    patchLayout: patchLayout,
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
