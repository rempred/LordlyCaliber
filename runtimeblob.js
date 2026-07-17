/*
 * runtimeblob.js — shared ROM-tail/upper-RAM runtime override composer.
 *
 * The proven Squads-only path remains in squadblob.js. When a project edits a
 * shop, this composer packs the squad resolver (if any) and the shop resolver
 * into the same OBMR blob, then installs one sentinel-checked PI-DMA loader in
 * the existing bootstrap/cave allocation. Hook delay slots select a module:
 *   t9=0 — squad template resolver
 *   t9=1 — per-shop combined item-list resolver
 *
 * Shop runtime format at MOD_BASE+0xEC00 / ROM tail+0xEC00:
 *   +0x00  'OBSH'
 *   +0x04  shopCount u16 BE
 *   +0x06  overrideCount u16 BE
 *   +0x08  shopCount * u16 BE record offsets (0 = run retail producer)
 *   records are terminated u16 lists: plain IDs are consumables; bit 15 marks
 *   equipment, matching the retail source-list contract.
 */
(function (OB64) {
  'use strict';

  if (!OB64.squad) throw new Error('runtimeblob.js requires squadblob.js');

  var S = OB64.squad;
  var M = S._enc;
  var assemble = S._assemble;

  var SHOP_HOOK_ROM = 0x19BF18;
  var SHOP_CLEANUP_ROM_DELTA = 0xAC;
  var SHOP_RESOLVER_OFF = 0xEB00;
  var SHOP_TABLE_OFF = 0xEC00;
  var SHOP_MAGIC = 0x4F425348; // 'OBSH'
  var SHARED_SENTINEL = 0x4F424D32; // 'OBM2': distinguish shared blobs from squad-only OBMR
  var SHOP_DISPATCH_ID = 1;
  var SQUAD_DISPATCH_ID = 0;
  var SQUAD_RESOLVER_OFF = 0x08;
  var CACHE_CONT_BYTES = 0xC0;
  var ICACHE_INVALIDATE_RAM = 0x800900C0;
  var DCACHE_INVALIDATE_RAM = 0x80090010;
  var DISP_SLTIU = 0x2EA2001E;
  var DISP_XORI = 0x38420001;
  var SHOP_ORIGINAL_WORDS = [
    0x00404021, // move t0,v0
    0x3C078022, // lui a3,0x8022
    0x8CE79F20, // lw a3,-0x60E0(a3)
    0x00031840, // sll v1,v1,1
    0x00681821, // addu v1,v1,t0
    0x90650000  // lbu a1,0(v1)
  ];

  function runtimeLayout(romOrLayout) {
    var profile = romOrLayout && romOrLayout.layout ? romOrLayout.layout : romOrLayout;
    if (!profile && OB64.currentRomLayout) profile = OB64.currentRomLayout;
    var layout = S.patchLayout(romOrLayout);
    // A distinct upper-RAM marker forces the first shared invocation to DMA
    // even if a savestate still contains the older squad-only OBMR payload.
    layout.SENTINEL = SHARED_SENTINEL;
    var shopPatch = (profile && profile.shopPatch) || {};
    layout.SHOP_HOOK_ROM = shopPatch.HOOK_ROM != null ? shopPatch.HOOK_ROM : SHOP_HOOK_ROM;
    layout.SHOP_CLEANUP_ROM = shopPatch.CLEANUP_ROM != null ? shopPatch.CLEANUP_ROM :
      (layout.SHOP_HOOK_ROM + SHOP_CLEANUP_ROM_DELTA) >>> 0;
    layout.supportsShopOverrides = !profile || profile.supportsShopOverrides !== false;
    return layout;
  }

  function writeU16(buf, off, value) {
    buf[off] = (value >>> 8) & 0xFF;
    buf[off + 1] = value & 0xFF;
  }

  function writeU32(buf, off, value) {
    buf[off] = (value >>> 24) & 0xFF;
    buf[off + 1] = (value >>> 16) & 0xFF;
    buf[off + 2] = (value >>> 8) & 0xFF;
    buf[off + 3] = value & 0xFF;
  }

  function readU16(buf, off) {
    return ((buf[off] << 8) | buf[off + 1]) >>> 0;
  }

  function readU32(buf, off) {
    return ((buf[off] << 24) | (buf[off + 1] << 16) |
      (buf[off + 2] << 8) | buf[off + 3]) >>> 0;
  }

  function regionEquals(buf, off, bytes) {
    if (!buf || off < 0 || off + bytes.length > buf.length) return false;
    for (var i = 0; i < bytes.length; i++) if (buf[off + i] !== bytes[i]) return false;
    return true;
  }

  function arraysEqual(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function shopCleanupBranchOffset(layout) {
    // Both instructions relocate inside the same side-loaded overlay. Encode
    // their ROM-relative distance instead of the decompiler's nominal RAM
    // label, which is not the address used by every live overlay placement.
    var branchRom = (layout.SHOP_HOOK_ROM + 16) >>> 0;
    if (typeof layout.SHOP_CLEANUP_ROM !== 'number' || !isFinite(layout.SHOP_CLEANUP_ROM)) {
      throw new Error('shop cleanup branch target is missing');
    }
    var delta = layout.SHOP_CLEANUP_ROM - (branchRom + 4);
    if ((delta & 3) !== 0) throw new Error('shop cleanup branch target is not word-aligned');
    var words = delta / 4;
    if (words < -0x8000 || words > 0x7FFF) throw new Error('shop cleanup branch target is out of range');
    return words;
  }

  function buildShopHook(layout) {
    return S.wordsToBytes([
      M.addu('t8', 'v0', 'zero'),       // preserve archive buffer across loader
      M.addu('a0', 'v1', 'zero'),       // a0 = direct ktenmain/shopcsv index
      M.jal(layout.BOOT_RAM),
      M.ori('t9', 'zero', SHOP_DISPATCH_ID),
      M.beq('zero', 'zero', shopCleanupBranchOffset(layout)),
      M.nop()
    ]);
  }

  function shopHookState(z64, layout) {
    if (!z64) return 'unknown';
    if (regionEquals(z64, layout.SHOP_HOOK_ROM, S.wordsToBytes(SHOP_ORIGINAL_WORDS))) return 'retail';
    if (regionEquals(z64, layout.SHOP_HOOK_ROM, buildShopHook(layout))) return 'shared';
    return 'foreign';
  }

  function buildShopResolver(layout) {
    var tableAddr = (layout.MOD_BASE + SHOP_TABLE_OFF) >>> 0;
    var lines = [
      ['lui', 'a3', 0x8022],
      ['lw', 'a3', 0x9F20, 'a3'],             // destination: cleared 0x200B source list
      ['lui', 't3', (tableAddr >>> 16) & 0xFFFF],
      ['ori', 't3', 't3', tableAddr & 0xFFFF],
      ['lhu', 't2', 4, 't3'],                 // table shopCount
      ['sltu', 'v0', 'a0', 't2'],
      ['beq', 'v0', 'zero', 'fallback'],
      ['nop'],
      ['sll', 't1', 'a0', 1],
      ['addu', 't1', 't3', 't1'],
      ['lhu', 't1', 8, 't1'],                 // relative record offset
      ['beq', 't1', 'zero', 'fallback'],
      ['nop'],
      ['addu', 't1', 't3', 't1'],
      ['label', 'custom_loop'],
      ['lhu', 'v0', 0, 't1'],
      ['sh', 'v0', 0, 'a3'],
      ['addiu', 't1', 't1', 2],
      ['beq', 'v0', 'zero', 'done'],
      ['addiu', 'a3', 'a3', 2],
      ['beq', 'zero', 'zero', 'custom_loop'],
      ['nop'],

      // No override for this direct shop index: reproduce func_0019BE40's
      // exact source-list tail (IDs 1-6,8 plus bit-15 equipment from shopcsv).
      ['label', 'fallback']
    ];
    var vanillaConsumables = [1, 2, 3, 4, 5, 6, 8];
    for (var i = 0; i < vanillaConsumables.length; i++) {
      lines.push(['ori', 'v0', 'zero', vanillaConsumables[i]]);
      lines.push(['sh', 'v0', i * 2, 'a3']);
    }
    lines = lines.concat([
      ['addiu', 'a3', 'a3', 0x0E],
      ['sll', 't1', 'a0', 1],
      ['addu', 't1', 't8', 't1'],             // t8 = decompressed shopcsv base
      ['lhu', 'a1', 0, 't1'],                 // start offset
      ['lhu', 'a2', 2, 't1'],                 // next offset / end
      ['subu', 'a2', 'a2', 'a1'],             // byte length
      ['addu', 'a1', 't8', 'a1'],
      ['beq', 'a2', 'zero', 'terminate'],
      ['nop'],
      ['label', 'vanilla_loop'],
      ['lhu', 'v0', 0, 'a1'],
      ['ori', 'v0', 'v0', 0x8000],
      ['sh', 'v0', 0, 'a3'],
      ['addiu', 'a1', 'a1', 2],
      ['addiu', 'a3', 'a3', 2],
      ['addiu', 'a2', 'a2', -2],
      ['bne', 'a2', 'zero', 'vanilla_loop'],
      ['nop'],
      ['label', 'terminate'],
      ['sh', 'zero', 0, 'a3'],
      ['label', 'done'],
      ['jr', 'ra'],
      ['nop']
    ]);
    var words = assemble((layout.MOD_BASE + SHOP_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > SHOP_TABLE_OFF - SHOP_RESOLVER_OFF) {
      throw new Error('shop resolver exceeds its 0x100-byte module slot');
    }
    return words;
  }

  function checkedIds(ids, maxCount, label) {
    if (!Array.isArray(ids)) throw new Error(label + ' must be an array');
    if (ids.length > maxCount) {
      throw new Error(label + ' has ' + ids.length + ' entries; static consumer capacity is ' + maxCount);
    }
    var out = [];
    var seen = {};
    for (var i = 0; i < ids.length; i++) {
      var id = Number(ids[i]);
      if (!Number.isInteger(id) || id <= 0 || id > 0x7FFF) {
        throw new Error(label + ' contains invalid ID ' + ids[i]);
      }
      if (seen[id]) throw new Error(label + ' contains duplicate ID ' + id);
      seen[id] = true;
      out.push(id);
    }
    return out;
  }

  function buildShopTable(overrides, shopCount) {
    shopCount = shopCount == null ? 36 : Number(shopCount);
    if (!Number.isInteger(shopCount) || shopCount <= 0 || shopCount > 256) {
      throw new Error('invalid shop count ' + shopCount);
    }
    var byIndex = {};
    var normalized = [];
    for (var i = 0; i < overrides.length; i++) {
      var src = overrides[i] || {};
      var shopIndex = Number(src.shopIndex != null ? src.shopIndex : src.index);
      if (!Number.isInteger(shopIndex) || shopIndex < 0 || shopIndex >= shopCount) {
        throw new Error('shop override ' + i + ' has invalid index ' + shopIndex);
      }
      if (byIndex[shopIndex]) throw new Error('duplicate runtime override for shop #' + shopIndex);
      byIndex[shopIndex] = true;
      normalized.push({
        shopIndex: shopIndex,
        items: checkedIds(src.items || [], OB64.SHOP_MAX_EQUIPMENT_PER_SHOP || 50,
          'shop #' + shopIndex + ' equipment'),
        consumables: checkedIds(src.consumables || [], OB64.SHOP_MAX_CONSUMABLES_PER_SHOP || 15,
          'shop #' + shopIndex + ' consumables')
      });
    }
    normalized.sort(function (a, b) { return a.shopIndex - b.shopIndex; });

    var headerSize = 8 + shopCount * 2;
    var size = headerSize;
    for (i = 0; i < normalized.length; i++) {
      size += (normalized[i].consumables.length + normalized[i].items.length + 1) * 2;
    }
    if (size > 0x10000 - SHOP_TABLE_OFF) {
      throw new Error('shop override table exceeds its shared 0x1400-byte tail allocation');
    }
    var table = new Uint8Array(size);
    writeU32(table, 0, SHOP_MAGIC);
    writeU16(table, 4, shopCount);
    writeU16(table, 6, normalized.length);
    var cursor = headerSize;
    for (i = 0; i < normalized.length; i++) {
      var o = normalized[i];
      writeU16(table, 8 + o.shopIndex * 2, cursor);
      for (var c = 0; c < o.consumables.length; c++) {
        writeU16(table, cursor, o.consumables[c]);
        cursor += 2;
      }
      for (var e = 0; e < o.items.length; e++) {
        writeU16(table, cursor, 0x8000 | o.items[e]);
        cursor += 2;
      }
      writeU16(table, cursor, 0);
      cursor += 2;
    }
    return table;
  }

  function buildSharedBlob(squadOverrides, shopOverrides, shopCount, layout) {
    var squadBlob = S.buildBlob(squadOverrides || [], layout);
    if (squadBlob.length > SHOP_RESOLVER_OFF) {
      var maxSquads = Math.floor((SHOP_RESOLVER_OFF - S.consts.ENTRIES_OFF) / S.consts.ENTRY_STRIDE);
      throw new Error('shared runtime blob leaves room for at most ' + maxSquads + ' squad overrides when shops are enabled');
    }
    var resolver = S.wordsToBytes(buildShopResolver(layout));
    var table = buildShopTable(shopOverrides || [], shopCount);
    var size = SHOP_TABLE_OFF + table.length;
    while (size % 8) size++;
    if (size > 0xFFFF) throw new Error('shared runtime blob exceeds the single-DMA length encoding');
    var blob = new Uint8Array(size);
    blob.set(squadBlob, 0);
    blob.set(resolver, SHOP_RESOLVER_OFF);
    blob.set(table, SHOP_TABLE_OFF);
    return blob;
  }

  function buildSharedBootstrap(blobLen, layout) {
    var lines = [
      ['lui', 't0', 0xA040],
      ['lw', 't1', 0, 't0'],
      ['lui', 't2', (layout.SENTINEL >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', layout.SENTINEL & 0xFFFF],
      ['beq', 't1', 't2', 'loaded'],
      ['nop'],
      ['raw', M.j(layout.CACHE_CONT_RAM)],
      ['nop'],
      ['label', 'loaded'],
      ['bne', 't9', 'zero', 'shop'],
      ['nop'],
      ['lui', 't0', (layout.MOD_BASE >>> 16) & 0xFFFF],
      ['ori', 't0', 't0', SQUAD_RESOLVER_OFF],
      ['jr', 't0'],
      ['nop'],
      ['label', 'shop'],
      ['lui', 't0', (layout.MOD_BASE >>> 16) & 0xFFFF],
      ['ori', 't0', 't0', SHOP_RESOLVER_OFF],
      ['jr', 't0'],
      ['nop']
    ];
    var words = assemble(layout.BOOT_RAM, lines);
    if (words.length * 4 > 108) throw new Error('shared bootstrap exceeds the 108-byte cave');
    return words;
  }

  function buildSharedContinuation(blobLen, layout) {
    if (blobLen <= 0 || blobLen > 0xFFFF) throw new Error('invalid shared blob DMA length ' + blobLen);
    var lines = [
      ['addu', 't4', 'ra', 'zero'],            // preserve original hook return
      ['addu', 't5', 'a0', 'zero'],            // shop index survives cache-helper calls
      ['lui', 'a0', (layout.MOD_BASE >>> 16) & 0xFFFF],
      ['ori', 'a1', 'zero', blobLen & 0xFFFF],
      ['raw', M.jal(ICACHE_INVALIDATE_RAM)],
      ['nop'],
      ['raw', M.jal(DCACHE_INVALIDATE_RAM)],
      ['nop'],
      ['addu', 'ra', 't4', 'zero'],
      ['lui', 't0', 0xA460],
      ['label', 'wait_idle'],
      ['lw', 't1', 0x10, 't0'],
      ['andi', 't1', 't1', 3],
      ['bne', 't1', 'zero', 'wait_idle'],
      ['nop'],
      ['lui', 't1', (layout.MOD_PHYS >>> 16) & 0xFFFF],
      ['sw', 't1', 0, 't0'],
      ['lui', 't1', (layout.PI_CART >>> 16) & 0xFFFF],
      ['sw', 't1', 4, 't0'],
      ['ori', 't1', 'zero', (blobLen - 1) & 0xFFFF],
      ['sw', 't1', 0x0C, 't0'],
      ['label', 'wait_dma'],
      ['lw', 't1', 0x10, 't0'],
      ['andi', 't1', 't1', 3],
      ['bne', 't1', 'zero', 'wait_dma'],
      ['nop'],
      ['addu', 'a0', 't5', 'zero'],
      ['raw', M.j(layout.BOOT_RAM)],            // sentinel now matches; dispatch by t9
      ['nop']
    ];
    var words = assemble(layout.CACHE_CONT_RAM, lines);
    if (words.length * 4 > CACHE_CONT_BYTES) {
      throw new Error('shared cache/DMA continuation exceeds its reserved cave slot');
    }
    return words;
  }

  function buildRuntimeOverrideWrites(squadOverrides, shopOverrides, shopCount, romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    if (!layout.supportsShopOverrides) throw new Error('shop runtime overrides are unavailable for this ROM revision');
    if (!shopOverrides || !shopOverrides.length) throw new Error('shared runtime build requires at least one shop override');
    var z64 = romOrLayout && romOrLayout.z64;
    if (z64 && shopHookState(z64, layout) === 'foreign') {
      throw new Error('shop producer hook contains unrecognized bytes; refusing to overwrite another patch');
    }
    var blob = buildSharedBlob(squadOverrides || [], shopOverrides, shopCount, layout);
    var boot = S.wordsToBytes(buildSharedBootstrap(blob.length, layout));
    var cont = S.wordsToBytes(buildSharedContinuation(blob.length, layout));
    var squadHook = (squadOverrides && squadOverrides.length)
      ? S.wordsToBytes([M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', SQUAD_DISPATCH_ID)])
      : S.wordsToBytes([DISP_SLTIU, DISP_XORI]);
    return {
      crcWindow: true,
      writes: [
        { offset: layout.HOOK_ROM, label: 'shared squad dispatch hook', bytes: squadHook },
        { offset: layout.SHOP_HOOK_ROM, label: 'per-shop source-list dispatch hook', bytes: buildShopHook(layout) },
        { offset: layout.BOOT_ROM, label: 'shared bootstrap (sentinel dispatch)', bytes: boot },
        { offset: layout.CACHE_CONT_Z64, label: 'shared cache-invalidate + DMA continuation', bytes: cont },
        { offset: layout.TAIL_Z64, label: 'shared OBM2 runtime blob', bytes: blob }
      ],
      blob: blob,
      shopOverrideCount: shopOverrides.length,
      squadCount: squadOverrides ? squadOverrides.length : 0
    };
  }

  function restoreShopHook(z64, romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    if (shopHookState(z64, layout) === 'foreign') {
      throw new Error('shop producer hook contains unrecognized bytes; refusing to restore over another patch');
    }
    z64.set(S.wordsToBytes(SHOP_ORIGINAL_WORDS), layout.SHOP_HOOK_ROM);
  }

  function restoreAll(z64, romOrLayout) {
    S.restoreVanilla(z64, romOrLayout);
    restoreShopHook(z64, romOrLayout);
  }

  function patchRegions(romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    var regions = S.patchRegions(romOrLayout).slice();
    regions.push({ kind: 'rom', start: layout.SHOP_HOOK_ROM, size: 24, label: 'shop source-list dispatch hook' });
    return regions;
  }

  function parseShopOverrides(z64, romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    var hook = buildShopHook(layout);
    if (!regionEquals(z64, layout.SHOP_HOOK_ROM, hook)) return {};
    if (readU32(z64, layout.TAIL_Z64) !== layout.SENTINEL) return {};
    var tableBase = layout.TAIL_Z64 + SHOP_TABLE_OFF;
    if (readU32(z64, tableBase) !== SHOP_MAGIC) return {};
    var shopCount = readU16(z64, tableBase + 4);
    if (!shopCount || shopCount > 256 || tableBase + 8 + shopCount * 2 > z64.length) return {};
    var out = {};
    for (var shopIndex = 0; shopIndex < shopCount; shopIndex++) {
      var relative = readU16(z64, tableBase + 8 + shopIndex * 2);
      if (!relative) continue;
      var cursor = tableBase + relative;
      var items = [];
      var consumables = [];
      var terminated = false;
      for (var n = 0; n <= 65 && cursor + 2 <= layout.TAIL_Z64 + 0x10000; n++, cursor += 2) {
        var encoded = readU16(z64, cursor);
        if (encoded === 0) { terminated = true; break; }
        if (encoded & 0x8000) items.push(encoded & 0x7FFF);
        else consumables.push(encoded);
      }
      if (terminated && items.length <= 50 && consumables.length <= 15) {
        out[shopIndex] = { shopIndex: shopIndex, items: items, consumables: consumables };
      }
    }
    return out;
  }

  function applyParsedShopOverrides(rom) {
    if (!rom || !rom.shops || !rom.z64) return 0;
    var parsed = parseShopOverrides(rom.z64, rom);
    var count = 0;
    for (var key in parsed) {
      var index = Number(key);
      if (!rom.shops[index]) continue;
      rom.shops[index].items = parsed[key].items.slice();
      rom.shops[index].consumables = parsed[key].consumables.slice();
      rom.shops[index].runtimeOverride = true;
      count++;
    }
    rom.shopRuntimeOverridesDetected = count;
    return count;
  }

  function refreshShopOverrideState(rom, shopIndex) {
    var shop = rom && rom.shops && rom.shops[shopIndex];
    if (!shop) return false;
    var original = rom.original && rom.original.shops && rom.original.shops[shopIndex];
    var changed = !original || !arraysEqual(shop.items, original.items) ||
      !arraysEqual(shop.consumables, original.consumables);
    shop.runtimeOverride = !!((original && original.runtimeOverride) || changed);
    return shop.runtimeOverride;
  }

  function collectShopOverrides(rom) {
    var out = [];
    if (!rom || !rom.shops) return out;
    for (var i = 0; i < rom.shops.length; i++) {
      var shop = rom.shops[i];
      var original = rom.original && rom.original.shops && rom.original.shops[i];
      var changed = original && (!arraysEqual(shop.items, original.items) ||
        !arraysEqual(shop.consumables, original.consumables));
      if (!shop.runtimeOverride && !changed) continue;
      out.push({
        shopIndex: i,
        items: (shop.items || []).slice(),
        consumables: (shop.consumables || []).slice()
      });
    }
    return out;
  }

  OB64.runtimeOverrides = {
    buildRuntimeOverrideWrites: buildRuntimeOverrideWrites,
    buildSharedBlob: buildSharedBlob,
    buildShopTable: buildShopTable,
    buildShopResolver: buildShopResolver,
    buildSharedBootstrap: buildSharedBootstrap,
    buildSharedContinuation: buildSharedContinuation,
    buildShopHook: buildShopHook,
    parseShopOverrides: parseShopOverrides,
    applyParsedShopOverrides: applyParsedShopOverrides,
    refreshShopOverrideState: refreshShopOverrideState,
    collectShopOverrides: collectShopOverrides,
    restoreShopHook: restoreShopHook,
    restoreAll: restoreAll,
    patchRegions: patchRegions,
    patchLayout: runtimeLayout,
    consts: {
      SHOP_HOOK_ROM: SHOP_HOOK_ROM,
      SHOP_CLEANUP_ROM_DELTA: SHOP_CLEANUP_ROM_DELTA,
      SHOP_RESOLVER_OFF: SHOP_RESOLVER_OFF,
      SHOP_TABLE_OFF: SHOP_TABLE_OFF,
      SHOP_MAGIC: SHOP_MAGIC,
      SHARED_SENTINEL: SHARED_SENTINEL,
      SHOP_ORIGINAL_WORDS: SHOP_ORIGINAL_WORDS.slice()
    }
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = OB64;
})(typeof OB64 !== 'undefined' ? OB64 :
  (typeof window !== 'undefined' ? (window.OB64 = window.OB64 || {}) : (this.OB64 = this.OB64 || {})));
