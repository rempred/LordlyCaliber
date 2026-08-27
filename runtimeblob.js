/*
 * runtimeblob.js — shared ROM-tail/upper-RAM runtime override composer.
 *
 * The proven Squads-only path remains in squadblob.js. When a project edits a
 * shop, this composer packs the squad resolver (if any) and the shop resolver
 * into the same OBMR blob, then installs one sentinel-checked PI-DMA loader in
 * the existing bootstrap/cave allocation. Hook delay slots select a module:
 *   t9=0 — squad template resolver
 *   t9=1 — per-shop combined item-list resolver
 *   t9=2/3 — per-scenario rate resolver (low bit is the structural bit-17 path)
 *   t9=4 — weighted three-slot creature-drop resolver
 *   t9=5 — custom neutral-squad selection
 *   t9=6 — custom neutral-squad materialization
 *   t9=7 — custom encounter-card message routing
 *   t9=8 — custom-squad persuasion chance
 *   t9=9 — command-menu zero-width padding recovery
 *   t9=10 — custom-squad retreat HP threshold
 *   t9=11 — custom-squad weighted victory reward
 *   t9=12/13 — direct custom-cycle budget selectors (Fight/Talk)
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
  var LEGACY_NEUTRAL_SENTINEL = 0x4F424D33; // 'OBM3': legacy 40-slice rate table
  var PROTOTYPE_TYPED_SENTINEL = 0x4F424D34; // 'OBM4': parent-side one-profile prototype
  var PREVIOUS_NEUTRAL_SHARED_SENTINEL = 0x4F424D35; // 'OBM5': pre-persuasion-target routing
  var OBM6_NEUTRAL_SHARED_SENTINEL = 0x4F424D36; // 'OBM6': exact persuasion-target routing
  var OBM7_NEUTRAL_SHARED_SENTINEL = 0x4F424D37; // 'OBM7': cycle budget and dynamic reward return
  var OBM8_NEUTRAL_SHARED_SENTINEL = 0x4F424D38; // 'OBM8': superseded terminal-state continuation
  var OBM9_NEUTRAL_SHARED_SENTINEL = 0x4F424D39; // 'OBM9': native opcode-0x41 round-gate continuation
  var OBMA_NEUTRAL_SHARED_SENTINEL = 0x4F424D41; // 'OBMA': fixed Bandits MESWIN entry
  var NEUTRAL_SHARED_SENTINEL = 0x4F424D42; // 'OBMB': per-profile encounter-card text
  var SHOP_DISPATCH_ID = 1;
  var SQUAD_DISPATCH_ID = 0;
  var RATE_DISPATCH_BASE = 2;
  var DROP_DISPATCH_ID = 4;
  var TYPED_SELECTION_DISPATCH_ID = 5;
  var TYPED_MATERIALIZER_DISPATCH_ID = 6;
  var TYPED_MESSAGE_DISPATCH_ID = 7;
  var TYPED_PERSUASION_DISPATCH_ID = 8;
  // Dispatch 9 belonged to superseded cleanup hooks. Current builds restore
  // those hooks to retail, so the fixed bootstrap slot now owns command-menu
  // stream iteration without growing the already-full 108-byte table.
  var TYPED_CLEANUP_DISPATCH_ID = 9;
  var TYPED_MENU_ITERATOR_DISPATCH_ID = 9;
  var TYPED_RETREAT_DISPATCH_ID = 10;
  var TYPED_REWARD_DISPATCH_ID = 11;
  var TYPED_FIGHT_BUDGET_SELECTOR = 12;
  var TYPED_TALK_BUDGET_SELECTOR = 13;
  var SQUAD_RESOLVER_OFF = 0x08;
  var TYPED_PROFILE_TABLE_OFF = 0xC000;
  var RATE_RESOLVER_OFF = 0xD000;
  var RATE_TABLE_OFF = 0xD100;
  var DROP_RESOLVER_OFF = 0xD520;
  var DROP_TABLE_OFF = 0xD700;
  var TYPED_SELECTION_RESOLVER_OFF = 0xE000;
  var TYPED_MATERIALIZER_RESOLVER_OFF = 0xE100;
  var TYPED_ROUND_CONTINUATION_RESOLVER_OFF = 0xE280;
  var TYPED_MENU_ITERATOR_RESOLVER_OFF = 0xE300;
  var TYPED_MESSAGE_RESOLVER_OFF = 0xE400;
  var TYPED_PERSUASION_RESOLVER_OFF = 0xE500;
  var TYPED_PERSUASION_TARGET_RESOLVER_OFF = 0xE700;
  var TYPED_CLEANUP_RESOLVER_OFF = 0xE800;
  var TYPED_RETREAT_RESOLVER_OFF = 0xE840;
  var TYPED_CYCLE_BUDGET_RESOLVER_OFF = 0xE900;
  var TYPED_REWARD_RESOLVER_OFF = 0xE980;
  // The reward hook replaces ROM 0x1024B4..0x1024D0. Its JAL leaves the
  // live address for ROM 0x1024BC in ra, so the continuation is ra + 0x18.
  var REWARD_CONTINUATION_FROM_RA = 0x18;
  var RATE_MAGIC = 0x4F424E52; // 'OBNR'
  var DROP_MAGIC = 0x4F424457; // 'OBDW'
  var RATE_SCENARIO_COUNT = 65; // direct rows 0..64; staged scenario byte is the index
  var RATE_ENTRY_STRIDE = 16;
  var RATE_BRANCH_STRIDE = 8;
  var DROP_CLASS_COUNT = 256;
  var DROP_ENTRY_STRIDE = 8;
  var RATE_MODE_INHERIT = 0;
  var RATE_MODE_OVERRIDE = 1;
  var RATE_MODE_DISABLED = 2;
  var TYPED_PROFILE_MAGIC = 0x4F424E54; // 'OBNT'
  var PREVIOUS_TYPED_PROFILE_VERSION = 4;
  var TYPED_PROFILE_VERSION = 5;
  var TYPED_PROFILE_ENTRY_STRIDE = 72;
  var TYPED_MEMBER_STRIDE = 4;
  var TYPED_MEMBER_TABLE_OFF = 16;
  var TYPED_EQUIPMENT_TABLE_OFF = 36;
  var TYPED_CLASS_BONUS_TABLE_OFF = 48;
  var TYPED_REWARD_TABLE_OFF = 56;
  var TYPED_MESSAGE_POINTER_OFF = 68;
  var TYPED_CLASS_BONUS_COUNT = 3;
  var TYPED_REWARD_COUNT = 3;
  var TYPED_DEFAULT_PERSUASION_CHANCE = 10;
  var TYPED_DEFAULT_RETREAT_HP_THRESHOLD = 0;
  var TYPED_SCRATCH_UNCACHED = 0xA040FF00;
  var TYPED_PROFILE_TOKEN = 0x4E545950; // 'NTYP'
  var TYPED_MAX_PROFILES = Math.floor((RATE_RESOLVER_OFF -
    (TYPED_PROFILE_TABLE_OFF + 8)) / TYPED_PROFILE_ENTRY_STRIDE);
  var NEUTRAL_TABLE_DISPATCH_PREBASE = 0x801ED77E;
  var BANDITS_MESSAGE_ENTRY = 15;
  var TYPED_MESSAGE_POINTER_SELECTOR = 1;
  var TYPED_MESSAGE_MAX_CHARS = 24;
  var BANDITS_MESSAGE_BYTES = new Uint8Array([
    0x40, 0x62, 0x40, 0x25, 0x30, 0x40, 0x6E, 0x22,
    0x42, 0x61, 0x6E, 0x64, 0x69, 0x74, 0x73, 0x21,
    0x22, 0x40, 0x61, 0x40, 0x63, 0x00
  ]); // @b@%0@n"Bandits!"@a@c\0
  var CACHE_CONT_BYTES = 0xC0;
  var ICACHE_INVALIDATE_RAM = 0x800900C0;
  var DCACHE_INVALIDATE_RAM = 0x80090010;
  var DISP_SLTIU = 0x2EA2001E;
  var DISP_XORI = 0x38420001;
  var RNG_RAM = 0x80092A60;
  var RATE_ORIGINAL_WORDS = [
    0x0C024A98, // jal 0x80092A60
    0x00000000  // nop
  ];
  var DROP_ORIGINAL_WORDS = RATE_ORIGINAL_WORDS.slice();
  var SELECTION_ORIGINAL_WORDS = [0x1440FF4D, 0x24420100];
  var MATERIALIZER_ORIGINAL_WORDS = [0x24020001, 0x16620017];
  var MESSAGE_ORIGINAL_WORDS = [0x00A22821, 0x30A5FFFF];
  var MESSAGE_TEXT_ORIGINAL_WORDS = [0x02031821, 0xAC830004];
  var PERSUASION_ORIGINAL_WORDS = [0x0C074520, 0x8E05004C];
  var PERSUASION_TARGET_ORIGINAL_WORDS = [0x0C072503, 0x00000000];
  var PERSUASION_RECRUIT_TARGET_ORIGINAL_WORDS = [0x0C072503, 0xAFB00018];
  var PRE_ELIGIBILITY_PERSUASION_HOOK_ROM = 0x21495C;
  var PRE_ELIGIBILITY_PERSUASION_ORIGINAL_WORDS = [0x3C028019, 0x904236E0];
  var LEGACY_CLEANUP_ORIGINAL_WORDS = [0x9463606A, 0x3C01801D];
  var CLEANUP_ORIGINAL_WORDS = [0x3C02801D, 0x8C42E8BC];
  var RETREAT_ORIGINAL_WORDS = [0x00002021, 0x3C050031];
  var FIGHT_BUDGET_ORIGINAL_WORDS = [0x2442FFFF, 0x00000000];
  var TALK_BUDGET_ORIGINAL_WORDS = [0x2442FFFE, 0x00000000];
  var MENU_ITERATOR_ORIGINAL_WORDS = [0x0C0765A8, 0x00000000];
  var LEGACY_ROUND_CONTINUATION_ORIGINAL_WORDS = [0x24030009, 0xA043606E];
  var ROUND_CONTINUATION_ORIGINAL_WORDS = [0x0C0722BF, 0x00000000];
  var REWARD_ORIGINAL_WORDS = [0x00111880, 0x00621821];
  var SHOP_ORIGINAL_WORDS = [
    0x00404021, // move t0,v0
    0x3C078022, // lui a3,0x8022
    0x8CE79F20, // lw a3,-0x60E0(a3)
    0x00031840, // sll v1,v1,1
    0x00681821, // addu v1,v1,t0
    0x90650000  // lbu a1,0(v1)
  ];

  function splitAddress(address) {
    address = address >>> 0;
    return {
      hi: (((address + 0x8000) >>> 16) & 0xFFFF) >>> 0,
      lo: address & 0xFFFF
    };
  }

  function splitOriAddress(address) {
    address = address >>> 0;
    return {
      hi: (address >>> 16) & 0xFFFF,
      lo: address & 0xFFFF
    };
  }

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
    layout.SHOP_LIST_RAM = shopPatch.LIST_RAM != null ? shopPatch.LIST_RAM >>> 0 : 0x80219F20;
    layout.SHOP_ORIGINAL_WORDS = Array.isArray(shopPatch.ORIGINAL_WORDS)
      ? shopPatch.ORIGINAL_WORDS.slice() : SHOP_ORIGINAL_WORDS.slice();
    var neutralPatch = (profile && profile.neutralRuntimePatch) || {};
    layout.RATE_HOOK_ROM = neutralPatch.RATE_HOOK_ROM != null
      ? neutralPatch.RATE_HOOK_ROM : 0x13C204;
    layout.DROP_HOOK_ROM = neutralPatch.DROP_HOOK_ROM != null
      ? neutralPatch.DROP_HOOK_ROM : 0x102488;
    layout.STAGED_SCENARIO_RAM = neutralPatch.STAGED_SCENARIO_RAM != null
      ? neutralPatch.STAGED_SCENARIO_RAM >>> 0 : 0x8018F481;
    layout.SCENARIO_META_NEUTRAL_BASE_RAM = neutralPatch.SCENARIO_META_NEUTRAL_BASE_RAM != null
      ? neutralPatch.SCENARIO_META_NEUTRAL_BASE_RAM >>> 0 : 0x801E7E18;
    layout.SELECTION_HOOK_ROM = neutralPatch.SELECTION_HOOK_ROM;
    layout.SELECTION_SUCCESS_LIVE = neutralPatch.SELECTION_SUCCESS_LIVE;
    layout.SELECTION_EMPTY_LIVE = neutralPatch.SELECTION_EMPTY_LIVE;
    layout.MATERIALIZER_HOOK_ROM = neutralPatch.MATERIALIZER_HOOK_ROM;
    layout.RETAIL_CLASS_ONE_LIVE = neutralPatch.RETAIL_CLASS_ONE_LIVE;
    layout.RETAIL_GENERIC_LIVE = neutralPatch.RETAIL_GENERIC_LIVE;
    layout.RETAIL_EPILOGUE_LIVE = neutralPatch.RETAIL_EPILOGUE_LIVE;
    layout.BATTLE_CHARACTER_CONSTRUCTOR_LIVE = neutralPatch.BATTLE_CHARACTER_CONSTRUCTOR_LIVE;
    layout.MESSAGE_HOOK_ROM = neutralPatch.MESSAGE_HOOK_ROM;
    layout.MESSAGE_CONTINUATION_LIVE = neutralPatch.MESSAGE_CONTINUATION_LIVE;
    layout.MESSAGE_TEXT_HOOK_ROM = neutralPatch.MESSAGE_TEXT_HOOK_ROM;
    layout.MESSAGE_INDEX_HIGH_RAM = neutralPatch.MESSAGE_INDEX_HIGH_RAM;
    layout.MESSAGE_INDEX_LOW_RAM = neutralPatch.MESSAGE_INDEX_LOW_RAM;
    layout.PERSUASION_HOOK_ROM = neutralPatch.PERSUASION_HOOK_ROM;
    layout.PERSUASION_CONTINUATION_LIVE = neutralPatch.PERSUASION_CONTINUATION_LIVE;
    layout.PERSUASION_RETAIL_HELPER_LIVE = neutralPatch.PERSUASION_RETAIL_HELPER_LIVE;
    layout.PERSUASION_EPILOGUE_LIVE = neutralPatch.PERSUASION_EPILOGUE_LIVE;
    layout.PERSUASION_RNG_LIVE = neutralPatch.PERSUASION_RNG_LIVE;
    layout.PERSUASION_ACTOR_AT_INDEX_LIVE = neutralPatch.PERSUASION_ACTOR_AT_INDEX_LIVE;
    layout.PERSUASION_FILTER_A_LIVE = neutralPatch.PERSUASION_FILTER_A_LIVE;
    layout.PERSUASION_FILTER_B_LIVE = neutralPatch.PERSUASION_FILTER_B_LIVE;
    layout.PERSUASION_FILTER_C_LIVE = neutralPatch.PERSUASION_FILTER_C_LIVE;
    layout.PERSUASION_FILTER_D_LIVE = neutralPatch.PERSUASION_FILTER_D_LIVE;
    layout.PERSUASION_ELIGIBLE_LIVE = neutralPatch.PERSUASION_ELIGIBLE_LIVE;
    layout.PERSUASION_RECRUIT_TARGET_HOOK_ROM =
      neutralPatch.PERSUASION_RECRUIT_TARGET_HOOK_ROM >>> 0;
    layout.PERSUASION_TARGET_HOOK_ROMS = Array.isArray(
      neutralPatch.PERSUASION_TARGET_HOOK_ROMS)
      ? neutralPatch.PERSUASION_TARGET_HOOK_ROMS.map(function (value) {
        return value >>> 0;
      }) : [];
    layout.PERSUASION_TARGET_RETAIL_LIVE = neutralPatch.PERSUASION_TARGET_RETAIL_LIVE;
    layout.LEGACY_CLEANUP_HOOK_ROM = neutralPatch.LEGACY_CLEANUP_HOOK_ROM;
    layout.LEGACY_CLEANUP_CONTINUATION_LIVE =
      neutralPatch.LEGACY_CLEANUP_CONTINUATION_LIVE;
    layout.CLEANUP_HOOK_ROM = neutralPatch.CLEANUP_HOOK_ROM;
    layout.CLEANUP_CONTINUATION_LIVE = neutralPatch.CLEANUP_CONTINUATION_LIVE;
    layout.RETREAT_HOOK_ROM = neutralPatch.RETREAT_HOOK_ROM;
    layout.RETREAT_CONTINUATION_LIVE = neutralPatch.RETREAT_CONTINUATION_LIVE;
    layout.RETREAT_EPILOGUE_LIVE = neutralPatch.RETREAT_EPILOGUE_LIVE;
    layout.FIGHT_BUDGET_HOOK_ROM = neutralPatch.FIGHT_BUDGET_HOOK_ROM;
    layout.TALK_BUDGET_HOOK_ROM = neutralPatch.TALK_BUDGET_HOOK_ROM;
    layout.FIGHT_RESULT_BUDGET_HOOK_ROM =
      neutralPatch.FIGHT_RESULT_BUDGET_HOOK_ROM;
    layout.MENU_ITERATOR_HOOK_ROM = neutralPatch.MENU_ITERATOR_HOOK_ROM;
    layout.MENU_ITERATOR_RETAIL_LIVE = neutralPatch.MENU_ITERATOR_RETAIL_LIVE;
    layout.MENU_STREAM_CONTEXT_PTR_LIVE =
      neutralPatch.MENU_STREAM_CONTEXT_PTR_LIVE;
    layout.LEGACY_ROUND_CONTINUATION_HOOK_ROM =
      neutralPatch.LEGACY_ROUND_CONTINUATION_HOOK_ROM;
    layout.ROUND_CONTINUATION_HOOK_ROM =
      neutralPatch.ROUND_CONTINUATION_HOOK_ROM;
    layout.ROUND_CONTINUATION_RETAIL_MODE_LIVE =
      neutralPatch.ROUND_CONTINUATION_RETAIL_MODE_LIVE;
    layout.REWARD_HOOK_ROM = neutralPatch.REWARD_HOOK_ROM;
    layout.supportsShopOverrides = !profile || profile.supportsShopOverrides !== false;
    layout.supportsNeutralRuntimeOverrides = !profile ||
      profile.supportsNeutralRuntimeOverrides !== false;
    layout.supportsNeutralCustomSquads = !profile ||
      profile.supportsNeutralCustomSquads !== false;
    return layout;
  }

  function neutralRuntimeLayout(romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    layout.SENTINEL = NEUTRAL_SHARED_SENTINEL;
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
    if (regionEquals(z64, layout.SHOP_HOOK_ROM,
        S.wordsToBytes(layout.SHOP_ORIGINAL_WORDS))) return 'retail';
    if (regionEquals(z64, layout.SHOP_HOOK_ROM, buildShopHook(layout))) return 'shared';
    return 'foreign';
  }

  function buildRateHook(layout) {
    return S.wordsToBytes([
      M.jal(layout.BOOT_RAM),
      // The state helper returns 0/1. OR-ing bit 1 selects dispatch 2/3 while
      // preserving that structural path across the cold-DMA continuation.
      M.ori('t9', 'v0', RATE_DISPATCH_BASE)
    ]);
  }

  function buildDropHook(layout) {
    return S.wordsToBytes([
      M.jal(layout.BOOT_RAM),
      M.ori('t9', 'zero', DROP_DISPATCH_ID)
    ]);
  }

  function simpleHookState(z64, off, originalWords, patchedBytes) {
    if (!z64) return 'unknown';
    if (regionEquals(z64, off, S.wordsToBytes(originalWords))) return 'retail';
    if (regionEquals(z64, off, patchedBytes)) return 'shared';
    return 'foreign';
  }

  function rateHookState(z64, layout) {
    return simpleHookState(z64, layout.RATE_HOOK_ROM,
      RATE_ORIGINAL_WORDS, buildRateHook(layout));
  }

  function dropHookState(z64, layout) {
    return simpleHookState(z64, layout.DROP_HOOK_ROM,
      DROP_ORIGINAL_WORDS, buildDropHook(layout));
  }

  function preEligibilityPersuasionHook(layout) {
    return S.wordsToBytes([
      M.j((layout.CACHE_CONT_RAM + 33 * 4) >>> 0),
      M.nop()
    ]);
  }

  function preEligibilityPersuasionHookState(z64, layout) {
    return simpleHookState(z64, PRE_ELIGIBILITY_PERSUASION_HOOK_ROM,
      PRE_ELIGIBILITY_PERSUASION_ORIGINAL_WORDS,
      preEligibilityPersuasionHook(layout));
  }

  function buildShopResolver(layout) {
    var tableAddr = (layout.MOD_BASE + SHOP_TABLE_OFF) >>> 0;
    var listAddress = splitAddress(layout.SHOP_LIST_RAM);
    var lines = [
      ['lui', 'a3', listAddress.hi],
      ['lw', 'a3', listAddress.lo, 'a3'],      // destination: cleared 0x200B source list
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

  function buildRateResolver(layout) {
    var tableAddr = (layout.MOD_BASE + RATE_TABLE_OFF + 8) >>> 0;
    var stagedScenarioAddress = splitAddress(layout.STAGED_SCENARIO_RAM);
    var lines = [
      // The staged scenario byte is the direct row index. This deliberately
      // avoids the retail neutral-slice alias, so two scenarios that share one
      // creature table can still use different encounter rates.
      ['lui', 't0', stagedScenarioAddress.hi],
      ['lbu', 't0', stagedScenarioAddress.lo, 't0'],
      ['sltiu', 't1', 't0', RATE_SCENARIO_COUNT],
      ['beq', 't1', 'zero', 'inherit'],
      ['nop'],
      ['sll', 't1', 't0', 4],
      ['lui', 't0', (tableAddr >>> 16) & 0xFFFF],
      ['ori', 't0', 't0', tableAddr & 0xFFFF],
      ['addu', 't0', 't0', 't1'],
      ['andi', 't1', 't9', 1],
      // The helper's nonzero result takes retail's 51/72000 delay-slot path.
      // Table branch 0 is `normal`, so invert 1->0 and 0->1 before indexing.
      ['xori', 't1', 't1', 1],
      ['sll', 't1', 't1', 3],
      ['addu', 't0', 't0', 't1'],
      ['lbu', 't1', 0, 't0'],
      ['beq', 't1', 'zero', 'inherit'],
      ['ori', 't2', 'zero', RATE_MODE_DISABLED],
      ['beq', 't1', 't2', 'disabled'],
      ['ori', 't2', 'zero', RATE_MODE_OVERRIDE],
      ['bne', 't1', 't2', 'inherit'],
      ['nop'],
      ['lhu', 's0', 2, 't0'],
      ['lw', 's1', 4, 't0'],
      ['label', 'inherit'],
      ['addiu', 'sp', 'sp', -8],
      ['sw', 'ra', 4, 'sp'],
      ['raw', M.jal(RNG_RAM)],
      ['nop'],
      ['lw', 'ra', 4, 'sp'],
      ['addiu', 'sp', 'sp', 8],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'disabled'],
      // Existing code computes v0 % s1 and rejects when s0 < remainder.
      // 1 % 2 = 1, so 0 < 1 deterministically takes that retail failure path.
      ['addu', 's0', 'zero', 'zero'],
      ['ori', 's1', 'zero', 2],
      ['ori', 'v0', 'zero', 1],
      ['jr', 'ra'],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + RATE_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > RATE_TABLE_OFF - RATE_RESOLVER_OFF) {
      throw new Error('neutral rate resolver exceeds its 0x100-byte module slot');
    }
    return words;
  }

  function normalizeRateBranch(source, label) {
    source = source || {};
    var mode = source.mode || 'inherit';
    if (mode === 'inherit') return { mode: RATE_MODE_INHERIT, threshold: 0, divisor: 0 };
    if (mode === 'disabled') return { mode: RATE_MODE_DISABLED, threshold: 0, divisor: 0 };
    if (mode !== 'override') throw new Error(label + ' has invalid mode ' + mode);
    var divisor = Number(source.divisor);
    if (!Number.isInteger(divisor) || divisor <= 0 || divisor > 0xFFFFFFFF) {
      throw new Error(label + ' divisor must be an integer from 1 through 4294967295');
    }
    var passCount = Number(source.passCount != null ? source.passCount : source.pass_count);
    if (!Number.isInteger(passCount) && isFinite(Number(source.microBasisPoints))) {
      passCount = Math.round((Number(source.microBasisPoints) * divisor) / 1000000);
    }
    var maxPass = Math.min(0x10000, divisor);
    if (!Number.isInteger(passCount) || passCount < 1 || passCount > maxPass) {
      throw new Error(label + ' pass count must be an integer from 1 through ' + maxPass);
    }
    return { mode: RATE_MODE_OVERRIDE, threshold: passCount - 1, divisor: divisor };
  }

  function buildRateTable(overrides) {
    var table = new Uint8Array(8 + RATE_SCENARIO_COUNT * RATE_ENTRY_STRIDE);
    writeU32(table, 0, RATE_MAGIC);
    writeU16(table, 4, RATE_SCENARIO_COUNT);
    writeU16(table, 6, RATE_ENTRY_STRIDE);
    var seen = {};
    overrides = overrides || [];
    for (var i = 0; i < overrides.length; i++) {
      var source = overrides[i] || {};
      var runtimeKey = Number(source.runtimeKey != null ? source.runtimeKey : source.key);
      if (!Number.isInteger(runtimeKey) || runtimeKey < 1 ||
          runtimeKey >= RATE_SCENARIO_COUNT) {
        throw new Error('neutral rate override #' + i +
          ' has invalid runtime scenario key ' + runtimeKey);
      }
      if (seen[runtimeKey]) {
        throw new Error('duplicate neutral rate override for runtime scenario key ' + runtimeKey);
      }
      seen[runtimeKey] = true;
      var branches = [
        normalizeRateBranch(source.normal, 'scenario key ' + runtimeKey + ' state-bit-set path'),
        normalizeRateBranch(source.alternate, 'scenario key ' + runtimeKey + ' state-bit-clear path')
      ];
      for (var branch = 0; branch < branches.length; branch++) {
        var entry = branches[branch];
        var off = 8 + runtimeKey * RATE_ENTRY_STRIDE + branch * RATE_BRANCH_STRIDE;
        table[off] = entry.mode;
        writeU16(table, off + 2, entry.threshold);
        writeU32(table, off + 4, entry.divisor);
      }
    }
    return table;
  }

  function normalizeCustomMembers(source, label) {
    if (!Array.isArray(source) || source.length < 2 || source.length > 5) {
      throw new Error(label + ' must contain two through five typed members');
    }
    var members = [];
    var cells = {};
    var cohorts = { A: [], B: [], C: [] };
    var formationSlots = 0;
    for (var index = 0; index < source.length; index++) {
      var raw = source[index] || {};
      var classId = Number(raw.classId);
      var levelOffsetRaw = Number(raw.levelOffsetRaw);
      var cell = Number(raw.cell);
      var cohort = String(raw.cohort || '').toUpperCase();
      if (!Number.isInteger(classId) || classId <= 1 || classId > 0xFF) {
        throw new Error(label + ' member ' + (index + 1) + ' has an unsupported class');
      }
      if (!Number.isInteger(levelOffsetRaw) || levelOffsetRaw < 0 || levelOffsetRaw > 0xFF) {
        throw new Error(label + ' member ' + (index + 1) + ' has an invalid level offset byte');
      }
      if (!Number.isInteger(cell) || cell < 0 || cell > 8 || cells[cell]) {
        throw new Error(label + ' formation cells must be unique values from 0 through 8');
      }
      if (!cohorts[cohort]) {
        throw new Error(label + ' member ' + (index + 1) + ' must use cohort A, B, or C');
      }
      var member = {
        classId: classId,
        levelOffsetRaw: levelOffsetRaw,
        cell: cell,
        cohort: cohort
      };
      members.push(member);
      cohorts[cohort].push(member);
      cells[cell] = true;
      formationSlots += OB64.SQUAD_DATA && OB64.SQUAD_DATA.largeSizes &&
        OB64.SQUAD_DATA.largeSizes[classId] ? 2 : 1;
    }
    if (cohorts.A.length !== 1 || members[0].cohort !== 'A') {
      throw new Error(label + ' requires exactly one Group A leader as its first member');
    }
    ['B', 'C'].forEach(function(cohort) {
      var group = cohorts[cohort];
      if (group.length > 3) throw new Error(label + ' Group ' + cohort + ' supports at most three members');
      for (var i = 1; i < group.length; i++) {
        if (group[i].classId !== group[0].classId ||
            group[i].levelOffsetRaw !== group[0].levelOffsetRaw) {
          throw new Error(label + ' Group ' + cohort + ' must share one class and level offset');
        }
      }
    });
    if (formationSlots > 5) throw new Error(label + ' exceeds the five-slot formation limit');
    function adjacent(a, b) {
      var rowA = a / 3 | 0;
      var colA = 2 - (a % 3);
      var rowB = b / 3 | 0;
      var colB = 2 - (b % 3);
      return a !== b && Math.abs(rowA - rowB) <= 1 && Math.abs(colA - colB) <= 1;
    }
    for (var m = 0; m < members.length; m++) {
      var large = OB64.SQUAD_DATA && OB64.SQUAD_DATA.largeSizes &&
        OB64.SQUAD_DATA.largeSizes[members[m].classId];
      if (!large) continue;
      for (var other = 0; other < members.length; other++) {
        if (m !== other && adjacent(members[m].cell, members[other].cell)) {
          throw new Error(label + ' places another member next to a large unit');
        }
      }
    }
    return members;
  }

  function normalizeCustomEquipment(source, label) {
    source = source || {};
    var equipment = {};
    ['A', 'B', 'C'].forEach(function(cohort) {
      var values = source[cohort] == null ? [0, 0] : source[cohort];
      if (!Array.isArray(values) || values.length !== 2) {
        throw new Error(label + ' Group ' + cohort + ' equipment must contain two choices');
      }
      equipment[cohort] = values.map(function(raw, index) {
        var value = Number(raw);
        if (!Number.isInteger(value) || value < 0 || value > 0x115) {
          throw new Error(label + ' Group ' + cohort + ' equipment choice ' +
            (index + 1) + ' is outside the supported item list');
        }
        return value;
      });
    });
    return equipment;
  }

  function normalizeClassBonuses(source, label) {
    if (source == null) return [];
    if (!Array.isArray(source) || source.length > TYPED_CLASS_BONUS_COUNT) {
      throw new Error(label + ' supports at most three player-leader class bonuses');
    }
    var seen = {};
    return source.map(function(raw, index) {
      raw = raw || {};
      var classId = Number(raw.classId);
      var bonus = Number(raw.bonus);
      if (!Number.isInteger(classId) || classId <= 1 || classId > 0xFF) {
        throw new Error(label + ' class bonus ' + (index + 1) + ' has an unsupported class');
      }
      if (seen[classId]) throw new Error(label + ' cannot repeat player-leader class ' + classId);
      if (!Number.isInteger(bonus) || bonus < 1 || bonus > 100) {
        throw new Error(label + ' class bonus ' + (index + 1) + ' must add 1 through 100 percent');
      }
      seen[classId] = true;
      return { classId: classId, bonus: bonus };
    });
  }

  function normalizeRewards(source, label) {
    var slots = source && source.slots;
    if (slots == null) slots = [
      { raw: 0, weight: 1 }, { raw: 0, weight: 1 }, { raw: 0, weight: 1 }
    ];
    if (!Array.isArray(slots) || slots.length !== TYPED_REWARD_COUNT) {
      throw new Error(label + ' reward table must contain exactly three choices');
    }
    var total = 0;
    var normalized = slots.map(function(raw, index) {
      raw = raw || {};
      var item = Number(raw.raw);
      var weight = Number(raw.weight);
      if (!Number.isInteger(item) || item < 0 || item > 0xFFFF) {
        throw new Error(label + ' reward choice ' + (index + 1) + ' has an invalid raw item');
      }
      if (!Number.isInteger(weight) || weight < 0 || weight > 0xFFFF) {
        throw new Error(label + ' reward choice ' + (index + 1) + ' has an invalid weight');
      }
      total += weight;
      return { raw: item, weight: weight };
    });
    if (total === 0) throw new Error(label + ' reward weights cannot all be zero');
    return { slots: normalized };
  }

  function normalizeEncounterText(source) {
    var text = source == null ? 'Bandits!' : String(source).trim();
    if (!text.length) throw new Error('encounter-card text cannot be empty');
    if (text.length > TYPED_MESSAGE_MAX_CHARS) {
      throw new Error('encounter-card text supports at most ' +
        TYPED_MESSAGE_MAX_CHARS + ' characters');
    }
    for (var i = 0; i < text.length; i++) {
      var code = text.charCodeAt(i);
      if (code < 0x20 || code > 0x7E || text[i] === '@' ||
          text[i] === '"' || text[i] === '\\') {
        throw new Error('encounter-card text uses an unsupported character at position ' +
          (i + 1));
      }
    }
    return text;
  }

  function encodeEncounterText(source) {
    var text = normalizeEncounterText(source);
    var prefix = '@b@%0@n"';
    var suffix = '"@a@c';
    var bytes = new Uint8Array(prefix.length + text.length + suffix.length + 1);
    var cursor = 0;
    var i;
    for (i = 0; i < prefix.length; i++) bytes[cursor++] = prefix.charCodeAt(i);
    for (i = 0; i < text.length; i++) bytes[cursor++] = text.charCodeAt(i);
    for (i = 0; i < suffix.length; i++) bytes[cursor++] = suffix.charCodeAt(i);
    bytes[cursor] = 0;
    return bytes;
  }

  function decodeEncounterText(bytes) {
    var prefix = '@b@%0@n"';
    var suffix = '"@a@c';
    if (!bytes || bytes.length < prefix.length + suffix.length + 2 ||
        bytes[bytes.length - 1] !== 0) return null;
    var value = '';
    for (var i = 0; i < bytes.length - 1; i++) value += String.fromCharCode(bytes[i]);
    if (value.slice(0, prefix.length) !== prefix ||
        value.slice(value.length - suffix.length) !== suffix) return null;
    try {
      return normalizeEncounterText(value.slice(prefix.length, -suffix.length));
    } catch (e) {
      return null;
    }
  }

  function normalizeCustomProfile(source, index) {
    source = source || {};
    var profileLabel = 'custom neutral squad #' + (index + 1);
    var runtimeKey = Number(source.runtimeKey);
    var slice = Number(source.slice != null ? source.slice : source.s0);
    var terrainSlot = Number(source.terrainSlot);
    if (!Number.isInteger(runtimeKey) || runtimeKey < 1 || runtimeKey >= RATE_SCENARIO_COUNT) {
      throw new Error(profileLabel + ' has an invalid runtime scenario key');
    }
    if (!Number.isInteger(slice) || slice < 1 || slice > 40) {
      throw new Error(profileLabel + ' has an invalid neutral slice');
    }
    if (!Number.isInteger(terrainSlot) || terrainSlot < 0 || terrainSlot > 9) {
      throw new Error(profileLabel + ' has an invalid terrain slot');
    }
    var members = normalizeCustomMembers(source.members, profileLabel);
    var equipment = normalizeCustomEquipment(source.equipment, profileLabel);
    var persuasion = source.persuasion || {};
    var persuasionChance = persuasion.chance == null
      ? TYPED_DEFAULT_PERSUASION_CHANCE : Number(persuasion.chance);
    if (!Number.isInteger(persuasionChance) ||
        persuasionChance < 0 || persuasionChance > 100) {
      throw new Error(profileLabel + ' persuasion chance must be an integer from 0 through 100');
    }
    var classBonuses = normalizeClassBonuses(persuasion.classBonuses, profileLabel);
    var retreat = source.retreat || {};
    var retreatHpThreshold = retreat.hpThreshold == null
      ? TYPED_DEFAULT_RETREAT_HP_THRESHOLD : Number(retreat.hpThreshold);
    if (!Number.isInteger(retreatHpThreshold) ||
        retreatHpThreshold < 0 || retreatHpThreshold > 100) {
      throw new Error(profileLabel + ' retreat HP threshold must be an integer from 0 through 100');
    }
    var pointerA = (NEUTRAL_TABLE_DISPATCH_PREBASE + slice * 20 +
      (terrainSlot + 1) * 2) >>> 0;
    return {
      profileId: Number.isInteger(Number(source.profileId)) ? Number(source.profileId) : index + 1,
      runtimeKey: runtimeKey,
      slice: slice,
      terrainSlot: terrainSlot,
      selectedPointerA: pointerA,
      selectedPointerB: (pointerA + 1) >>> 0,
      label: normalizeEncounterText(source.label),
      members: members,
      equipment: equipment,
      persuasion: {
        mode: 'fixed',
        chance: persuasionChance,
        classBonuses: classBonuses
      },
      retreat: {
        hpThreshold: retreatHpThreshold
      },
      rewards: normalizeRewards(source.rewards, profileLabel)
    };
  }

  function buildTypedProfileTable(profiles, layout) {
    profiles = profiles || [];
    if (profiles.length > TYPED_MAX_PROFILES) {
      throw new Error('custom neutral squad table supports at most ' +
        TYPED_MAX_PROFILES + ' profiles');
    }
    var normalized = [];
    var seen = {};
    var seenIds = {};
    for (var i = 0; i < profiles.length; i++) {
      var profile = normalizeCustomProfile(profiles[i], i);
      var profileKey = profile.runtimeKey + ':' + profile.terrainSlot;
      if (seen[profileKey]) throw new Error('duplicate custom neutral squad for ' + profileKey);
      if (!Number.isInteger(profile.profileId) || profile.profileId < 1 ||
          profile.profileId > 255 || seenIds[profile.profileId]) {
        throw new Error('custom neutral squad profile IDs must be unique bytes from 1 through 255');
      }
      seen[profileKey] = true;
      seenIds[profile.profileId] = true;
      normalized.push(profile);
    }
    normalized.sort(function(a, b) {
      return a.runtimeKey - b.runtimeKey || a.terrainSlot - b.terrainSlot;
    });
    var recordsBytes = 8 + normalized.length * TYPED_PROFILE_ENTRY_STRIDE;
    var messageOffsets = Object.create(null);
    var messagePieces = [];
    var tableBytes = recordsBytes;
    for (i = 0; i < normalized.length; i++) {
      var encounterText = normalized[i].label;
      if (messageOffsets[encounterText] != null) continue;
      var encodedText = encodeEncounterText(encounterText);
      messageOffsets[encounterText] = tableBytes;
      messagePieces.push({ off: tableBytes, bytes: encodedText });
      tableBytes += encodedText.length;
    }
    if (tableBytes > RATE_RESOLVER_OFF - TYPED_PROFILE_TABLE_OFF) {
      throw new Error('custom neutral squad profiles and encounter-card text exceed the ' +
        '0x1000-byte typed-profile allocation');
    }
    if (!layout || !Number.isInteger(layout.MOD_BASE)) {
      throw new Error('custom neutral squad table requires a runtime module layout');
    }
    var table = new Uint8Array(tableBytes);
    writeU32(table, 0, TYPED_PROFILE_MAGIC);
    table[4] = TYPED_PROFILE_VERSION;
    table[5] = normalized.length;
    table[6] = TYPED_PROFILE_ENTRY_STRIDE;
    table[7] = TYPED_MEMBER_STRIDE;
    for (i = 0; i < normalized.length; i++) {
      profile = normalized[i];
      var off = 8 + i * TYPED_PROFILE_ENTRY_STRIDE;
      writeU32(table, off, profile.selectedPointerA);
      writeU32(table, off + 4, profile.selectedPointerB);
      table[off + 8] = profile.profileId;
      table[off + 9] = profile.runtimeKey;
      table[off + 10] = profile.members.length;
      table[off + 11] = 1;
      table[off + 12] = profile.persuasion.chance;
      table[off + 13] = profile.retreat.hpThreshold;
      table[off + 14] = profile.slice;
      table[off + 15] = profile.terrainSlot;
      for (var memberIndex = 0; memberIndex < profile.members.length; memberIndex++) {
        var member = profile.members[memberIndex];
        var memberOff = off + TYPED_MEMBER_TABLE_OFF + memberIndex * TYPED_MEMBER_STRIDE;
        table[memberOff] = member.classId;
        table[memberOff + 1] = member.levelOffsetRaw;
        table[memberOff + 2] = member.cell;
        table[memberOff + 3] = member.cohort === 'A' ? 0 : (member.cohort === 'B' ? 1 : 2);
      }
      var equipmentOff = off + TYPED_EQUIPMENT_TABLE_OFF;
      ['A', 'B', 'C'].forEach(function(cohort, cohortIndex) {
        writeU16(table, equipmentOff + cohortIndex * 4, profile.equipment[cohort][0]);
        writeU16(table, equipmentOff + cohortIndex * 4 + 2, profile.equipment[cohort][1]);
      });
      for (var bonusIndex = 0; bonusIndex < profile.persuasion.classBonuses.length; bonusIndex++) {
        var bonusOff = off + TYPED_CLASS_BONUS_TABLE_OFF + bonusIndex * 2;
        table[bonusOff] = profile.persuasion.classBonuses[bonusIndex].classId;
        table[bonusOff + 1] = profile.persuasion.classBonuses[bonusIndex].bonus;
      }
      table[off + 54] = TYPED_REWARD_COUNT;
      for (var rewardIndex = 0; rewardIndex < TYPED_REWARD_COUNT; rewardIndex++) {
        var rewardOff = off + TYPED_REWARD_TABLE_OFF + rewardIndex * 4;
        writeU16(table, rewardOff, profile.rewards.slots[rewardIndex].raw);
        writeU16(table, rewardOff + 2, profile.rewards.slots[rewardIndex].weight);
      }
      writeU32(table, off + TYPED_MESSAGE_POINTER_OFF,
        (layout.MOD_BASE + TYPED_PROFILE_TABLE_OFF +
          messageOffsets[profile.label]) >>> 0);
    }
    for (i = 0; i < messagePieces.length; i++) {
      table.set(messagePieces[i].bytes, messagePieces[i].off);
    }
    return { bytes: table, profiles: normalized };
  }

  function buildSelectionResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var tableAddress = splitOriAddress((layout.MOD_BASE + TYPED_PROFILE_TABLE_OFF) >>> 0);
    var staged = splitAddress(layout.STAGED_SCENARIO_RAM);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 'v0', 0, 't0'],
      ['lw', 'v1', 4, 't0'],
      ['sw', 'zero', 8, 't0'],
      ['sw', 'zero', 12, 't0'],
      ['sw', 'zero', 16, 't0'],
      ['sw', 'zero', 20, 't0'],
      ['sw', 'zero', 24, 't0'],
      ['sw', 'zero', 28, 't0'],
      ['lui', 't2', tableAddress.hi],
      ['ori', 't2', 't2', tableAddress.lo],
      ['lbu', 't3', 5, 't2'],
      ['addiu', 't2', 't2', 8],
      ['lui', 't4', staged.hi],
      ['lbu', 't4', staged.lo, 't4'],
      ['label', 'loop'],
      // A custom profile may intentionally replace an empty retail slot.
      // Check the scenario/pointer profile before treating class zero as empty.
      ['blez', 't3', 'fallback'],
      ['nop'],
      ['lbu', 't5', 9, 't2'],
      ['bne', 't4', 't5', 'next'],
      ['nop'],
      ['lw', 't5', 0, 't2'],
      ['beq', 'v1', 't5', 'match'],
      ['nop'],
      ['lw', 't5', 4, 't2'],
      ['bne', 'v1', 't5', 'next'],
      ['nop'],
      ['label', 'match'],
      // Downstream neutral presentation subtracts 0x100 and treats the low
      // byte as a real class ID. A custom profile may replace an empty retail
      // slot, so stage its leader class instead of propagating class zero.
      ['lbu', 'v0', 16, 't2'],
      ['sw', 'v0', 0, 't0'],
      ['lui', 't6', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't6', 't6', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['sw', 't6', 8, 't0'],
      ['sw', 't2', 12, 't0'],
      ['sw', 't6', 16, 't0'],
      ['sw', 't2', 20, 't0'],
      ['beq', 'zero', 'zero', 'selected'],
      ['nop'],
      ['label', 'next'],
      ['addiu', 't2', 't2', TYPED_PROFILE_ENTRY_STRIDE],
      ['addiu', 't3', 't3', -1],
      ['beq', 'zero', 'zero', 'loop'],
      ['nop'],
      ['label', 'fallback'],
      ['beq', 'v0', 'zero', 'empty'],
      ['nop'],
      ['label', 'selected'],
      ['addiu', 'v0', 'v0', 0x100],
      ['raw', M.j(layout.SELECTION_SUCCESS_LIVE)],
      ['nop'],
      ['label', 'empty'],
      ['raw', M.j(layout.SELECTION_EMPTY_LIVE)],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_SELECTION_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_MATERIALIZER_RESOLVER_OFF - TYPED_SELECTION_RESOLVER_OFF) {
      throw new Error('custom neutral selection resolver exceeds its 0x100-byte slot');
    }
    return words;
  }

  function buildMaterializerResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 8, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'retail'],
      ['sw', 'zero', 8, 't0'],
      ['lw', 't4', 12, 't0'],
      ['sw', 'zero', 12, 't0'],
      ['beq', 't4', 'zero', 'retail'],
      ['nop'],
      ['lbu', 't5', 10, 't4'],
      ['addiu', 't4', 't4', TYPED_MEMBER_TABLE_OFF],
      ['lui', 't0', 0x8019],
      ['addiu', 't0', 't0', 0x5594],
      ['ori', 't1', 'zero', 119],
      ['addu', 't2', 'zero', 'zero'],
      ['label', 'preflight'],
      ['lbu', 't3', 0x11, 't0'],
      ['bne', 't3', 'zero', 'preflight_next'],
      ['nop'],
      ['addiu', 't2', 't2', 1],
      ['sltu', 't3', 't2', 't5'],
      ['beq', 't3', 'zero', 'typed_begin'],
      ['nop'],
      ['label', 'preflight_next'],
      ['addiu', 't0', 't0', 52],
      ['addiu', 't1', 't1', -1],
      ['bne', 't1', 'zero', 'preflight'],
      ['nop'],
      ['beq', 'zero', 'zero', 'retail'],
      ['nop'],
      ['label', 'typed_begin'],
      ['addiu', 'sp', 'sp', -40],
      ['sw', 's4', 20, 'sp'],
      ['sw', 's5', 24, 'sp'],
      ['sw', 's6', 28, 'sp'],
      ['sw', 's7', 32, 'sp'],
      ['addu', 's4', 't4', 'zero'],
      ['addu', 's5', 't5', 'zero'],
      ['addu', 's6', 'zero', 'zero'],
      ['label', 'member_loop'],
      ['lui', 's0', 0x8019],
      ['addiu', 's0', 's0', 0x5594],
      ['ori', 's7', 'zero', 1],
      ['label', 'find_free'],
      ['lbu', 't0', 0x11, 's0'],
      ['beq', 't0', 'zero', 'found_free'],
      ['nop'],
      ['addiu', 's0', 's0', 52],
      ['addiu', 's7', 's7', 1],
      ['beq', 'zero', 'zero', 'find_free'],
      ['nop'],
      ['label', 'found_free'],
      ['lbu', 'a1', 0, 's4'],
      ['lbu', 't0', 1, 's4'],
      ['addu', 'a2', 's2', 't0'],
      ['andi', 'a2', 'a2', 0xFF],
      ['addu', 'a0', 's0', 'zero'],
      // Each typed member carries its A/B/C cohort. Resolve that cohort's two
      // starting-equipment choices from profile+36 and pass them through the
      // retail constructor's fourth and fifth arguments.
      ['sll', 't6', 's6', 2],
      ['subu', 't7', 's4', 't6'],
      ['addiu', 't7', 't7', TYPED_EQUIPMENT_TABLE_OFF - TYPED_MEMBER_TABLE_OFF],
      ['lbu', 't6', 3, 's4'],
      ['sll', 't6', 't6', 2],
      ['addu', 't7', 't7', 't6'],
      ['lhu', 'a3', 0, 't7'],
      ['lhu', 't6', 2, 't7'],
      ['sw', 't6', 0x10, 'sp'],
      ['raw', M.jal(layout.BATTLE_CHARACTER_CONSTRUCTOR_LIVE)],
      ['nop'],
      ['addu', 't0', 's1', 's6'],
      ['sb', 's7', 2, 't0'],
      ['lbu', 't1', 2, 's4'],
      ['sb', 't1', 7, 't0'],
      ['addiu', 's4', 's4', TYPED_MEMBER_STRIDE],
      ['addiu', 's6', 's6', 1],
      ['addiu', 's5', 's5', -1],
      ['bne', 's5', 'zero', 'member_loop'],
      ['nop'],
      ['lw', 's4', 20, 'sp'],
      ['lw', 's5', 24, 'sp'],
      ['lw', 's6', 28, 'sp'],
      ['lw', 's7', 32, 'sp'],
      ['addiu', 'sp', 'sp', 40],
      ['raw', M.j(layout.RETAIL_EPILOGUE_LIVE)],
      ['nop'],
      ['label', 'retail'],
      ['ori', 't0', 'zero', 1],
      ['bne', 's3', 't0', 'retail_generic'],
      ['nop'],
      ['addu', 'a0', 'zero', 'zero'],
      ['raw', M.j(layout.RETAIL_CLASS_ONE_LIVE)],
      ['nop'],
      ['label', 'retail_generic'],
      ['raw', M.j(layout.RETAIL_GENERIC_LIVE)],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_MATERIALIZER_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_ROUND_CONTINUATION_RESOLVER_OFF -
        TYPED_MATERIALIZER_RESOLVER_OFF) {
      throw new Error('custom neutral materializer exceeds its reserved module slot');
    }
    return words;
  }

  function buildRoundContinuationResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      // Replace only the native secondary mode accessor. A nonzero retail
      // result remains exact. A custom encounter token supplies the missing
      // nonzero result, then the untouched opcode-0x41 gate performs every
      // native death, retreat, timer, actor-reset, and stream-rebuild check.
      ['addiu', 'sp', 'sp', -0x18],
      ['sw', 'ra', 0x14, 'sp'],
      ['raw', M.jal(layout.ROUND_CONTINUATION_RETAIL_MODE_LIVE)],
      ['nop'],
      ['bne', 'v0', 'zero', 'done'],
      ['nop'],
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'done'],
      ['nop'],
      ['lw', 't3', 20, 't0'],
      ['beq', 't3', 'zero', 'done'],
      ['nop'],
      ['ori', 'v0', 'zero', 1],
      ['label', 'done'],
      ['lw', 'ra', 0x14, 'sp'],
      ['jr', 'ra'],
      ['addiu', 'sp', 'sp', 0x18]
    ];
    var words = assemble((layout.MOD_BASE +
      TYPED_ROUND_CONTINUATION_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_MENU_ITERATOR_RESOLVER_OFF -
        TYPED_ROUND_CONTINUATION_RESOLVER_OFF) {
      throw new Error('custom round-continuation resolver exceeds its module slot');
    }
    return words;
  }

  function buildMenuIteratorResolver(layout) {
    var contextPointer = splitAddress(layout.MENU_STREAM_CONTEXT_PTR_LIVE);
    var lines = [
      // Preserve the retail result whenever it advances or terminates. The
      // Training stream can instead expose alternating 0xFF/0x00 padding whose
      // zero width makes the retail helper return the input offset unchanged.
      ['addiu', 'sp', 'sp', -0x18],
      ['sw', 'ra', 0x14, 'sp'],
      ['sw', 'a0', 0x10, 'sp'],
      ['raw', M.jal(layout.MENU_ITERATOR_RETAIL_LIVE)],
      ['nop'],
      ['lw', 't0', 0x10, 'sp'],
      ['bne', 'v0', 't0', 'done'],
      ['nop'],
      // Scope recovery to this command-menu caller. Walk forward inside the
      // declared stream only, skip the observed padding bytes, and return the
      // first real record so func_002159D0 still evaluates and inserts against
      // that record. Reaching the end retains the retail -1 termination shape.
      ['lui', 't1', contextPointer.hi],
      ['lw', 't1', contextPointer.lo, 't1'],
      ['lw', 't2', 0x814, 't1'],
      ['addiu', 'v0', 't0', 1],
      ['label', 'scan'],
      ['sltu', 't3', 'v0', 't2'],
      ['beq', 't3', 'zero', 'exhausted'],
      ['nop'],
      ['addu', 't4', 't1', 'v0'],
      ['lbu', 't5', 0x10, 't4'],
      ['beq', 't5', 'zero', 'advance'],
      ['nop'],
      ['ori', 't6', 'zero', 0xFF],
      ['bne', 't5', 't6', 'done'],
      ['nop'],
      ['label', 'advance'],
      ['addiu', 'v0', 'v0', 1],
      ['beq', 'zero', 'zero', 'scan'],
      ['nop'],
      ['label', 'exhausted'],
      ['addiu', 'v0', 'zero', -1],
      ['label', 'done'],
      ['lw', 'ra', 0x14, 'sp'],
      ['jr', 'ra'],
      ['addiu', 'sp', 'sp', 0x18]
    ];
    var words = assemble((layout.MOD_BASE +
      TYPED_MENU_ITERATOR_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_MESSAGE_RESOLVER_OFF -
        TYPED_MENU_ITERATOR_RESOLVER_OFF) {
      throw new Error('command-menu iterator resolver exceeds its module slot');
    }
    return words;
  }

  function buildMessageResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['addiu', 't0', 'zero', TYPED_MESSAGE_POINTER_SELECTOR],
      ['beq', 't9', 't0', 'text_pointer'],
      ['nop'],
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'retail'],
      ['nop'],
      ['ori', 'a1', 'zero', BANDITS_MESSAGE_ENTRY],
      // The encounter-sequence overlay is relocated at runtime. The hook's
      // jal already placed its exact live continuation in ra, and the shared
      // loader preserves ra across a first-use DMA.
      ['jr', 'ra'],
      ['nop'],
      ['label', 'retail'],
      ['addu', 'a1', 'a1', 'v0'],
      ['andi', 'a1', 'a1', 0xFFFF],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'text_pointer'],
      // Entry 15 is empty in retail. The encounter-sequence hook reserves it
      // as a safe marker, while this generic MESWIN pointer hook redirects
      // only that marker to the active profile's runtime string.
      ['lui', 't0', (layout.MESSAGE_INDEX_HIGH_RAM >>> 16) & 0xFFFF],
      ['lbu', 't1', layout.MESSAGE_INDEX_HIGH_RAM & 0xFFFF, 't0'],
      ['andi', 't1', 't1', 0x3F],
      ['bne', 't1', 'zero', 'text_retail'],
      ['nop'],
      ['lbu', 't1', layout.MESSAGE_INDEX_LOW_RAM & 0xFFFF, 't0'],
      ['addiu', 't2', 'zero', BANDITS_MESSAGE_ENTRY],
      ['bne', 't1', 't2', 'text_retail'],
      ['nop'],
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'text_retail'],
      ['nop'],
      ['lw', 't3', 20, 't0'],
      ['beq', 't3', 'zero', 'text_retail'],
      ['nop'],
      ['lw', 'v1', TYPED_MESSAGE_POINTER_OFF, 't3'],
      ['beq', 'v1', 'zero', 'text_retail'],
      ['nop'],
      ['jr', 'ra'],
      ['sw', 'v1', 4, 'a0'],
      ['label', 'text_retail'],
      ['addu', 'v1', 's0', 'v1'],
      ['jr', 'ra'],
      ['sw', 'v1', 4, 'a0']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_MESSAGE_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_PERSUASION_RESOLVER_OFF - TYPED_MESSAGE_RESOLVER_OFF) {
      throw new Error('custom encounter-card resolver exceeds its 0x100-byte slot');
    }
    return words;
  }

  function buildPersuasionResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'retail'],
      ['nop'],
      ['lw', 't3', 20, 't0'],
      ['beq', 't3', 'zero', 'retail'],
      ['nop'],
      ['lbu', 't1', 11, 't3'],
      ['beq', 't1', 'zero', 'retail'],
      ['nop'],
      // Retail stops on the first eligible target. Continue the same scan
      // after that target; Talk can succeed only when no second eligible unit
      // remains. Keep locals above the o32 argument-home area because every
      // eligibility helper may use the first sixteen bytes of our frame.
      ['addiu', 'sp', 'sp', -0x20],
      ['sw', 't3', 0x10, 'sp'],
      // The Talk result states must describe the exact record selected by the
      // persuasion scan, not retail's later first-flagged-record lookup.
      ['sw', 'zero', 24, 't0'],
      ['sw', 's0', 28, 't0'],
      ['addiu', 's3', 's1', 1],
      ['label', 'target_scan'],
      ['sltiu', 't4', 's3', 20],
      ['beq', 't4', 'zero', 'sole_target'],
      ['nop'],
      ['addu', 'a0', 's3', 'zero'],
      ['raw', M.jal(layout.PERSUASION_ACTOR_AT_INDEX_LIVE)],
      ['nop'],
      ['sw', 'v0', 0x14, 'sp'],
      ['beq', 'v0', 'zero', 'target_next'],
      ['addu', 'a0', 'v0', 'zero'],
      ['raw', M.jal(layout.PERSUASION_FILTER_A_LIVE)],
      ['nop'],
      ['bne', 'v0', 'zero', 'target_next'],
      ['nop'],
      ['lw', 'a0', 0x14, 'sp'],
      ['raw', M.jal(layout.PERSUASION_FILTER_B_LIVE)],
      ['nop'],
      ['bne', 'v0', 'zero', 'target_next'],
      ['nop'],
      ['lw', 'a0', 0x14, 'sp'],
      ['raw', M.jal(layout.PERSUASION_FILTER_C_LIVE)],
      ['nop'],
      ['bne', 'v0', 'zero', 'target_next'],
      ['nop'],
      ['lw', 'a0', 0x14, 'sp'],
      ['raw', M.jal(layout.PERSUASION_FILTER_D_LIVE)],
      ['nop'],
      ['bne', 'v0', 'zero', 'target_next'],
      ['nop'],
      ['lw', 'a0', 0x14, 'sp'],
      ['raw', M.jal(layout.PERSUASION_ELIGIBLE_LIVE)],
      ['nop'],
      ['bne', 'v0', 'zero', 'too_many_targets'],
      ['nop'],
      ['label', 'target_next'],
      ['addiu', 's3', 's3', 1],
      ['beq', 'zero', 'zero', 'target_scan'],
      ['nop'],
      ['label', 'too_many_targets'],
      ['beq', 'zero', 'zero', 'failure'],
      ['nop'],
      ['label', 'sole_target'],
      ['lw', 't3', 0x10, 'sp'],
      ['lbu', 't2', 12, 't3'],
      // Add at most one exact current player-leader class bonus.
      ['beq', 's2', 'zero', 'chance_ready'],
      ['nop'],
      ['lbu', 't4', 0x4B, 's2'],
      ['addiu', 't5', 't3', TYPED_CLASS_BONUS_TABLE_OFF],
      ['ori', 't6', 'zero', TYPED_CLASS_BONUS_COUNT],
      ['label', 'bonus_loop'],
      ['lbu', 't7', 0, 't5'],
      ['bne', 't7', 't4', 'bonus_next'],
      ['nop'],
      ['lbu', 't8', 1, 't5'],
      ['addu', 't2', 't2', 't8'],
      ['beq', 'zero', 'zero', 'bonus_done'],
      ['nop'],
      ['label', 'bonus_next'],
      ['addiu', 't5', 't5', 2],
      ['addiu', 't6', 't6', -1],
      ['bne', 't6', 'zero', 'bonus_loop'],
      ['nop'],
      ['label', 'bonus_done'],
      ['ori', 't1', 'zero', 100],
      ['sltu', 't4', 't1', 't2'],
      ['beq', 't4', 'zero', 'chance_ready'],
      ['nop'],
      ['addu', 't2', 't1', 'zero'],
      ['label', 'chance_ready'],
      ['beq', 't2', 'zero', 'failure'],
      ['ori', 't1', 'zero', 100],
      ['beq', 't2', 't1', 'success'],
      ['nop'],
      ['sw', 't2', 0x18, 'sp'],
      ['raw', M.jal(layout.PERSUASION_RNG_LIVE)],
      ['nop'],
      ['lw', 't2', 0x18, 'sp'],
      ['ori', 't1', 'zero', 100],
      ['divu', 'v0', 't1'],
      ['mfhi', 'v0'],
      ['sltu', 't1', 'v0', 't2'],
      ['beq', 't1', 'zero', 'failure'],
      ['nop'],
      ['label', 'success'],
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['ori', 't1', 'zero', 2],
      ['sw', 't1', 24, 't0'],
      // Retail result zero enters the joined-battalion success state.
      ['addu', 'v0', 'zero', 'zero'],
      ['addiu', 'sp', 'sp', 0x20],
      ['raw', M.j(layout.PERSUASION_EPILOGUE_LIVE)],
      ['nop'],
      ['label', 'failure'],
      // Retail result two enters the nonterminal "not persuaded" state.
      // Result one is deliberately avoided because it retreats the leader.
      ['ori', 'v0', 'zero', 2],
      ['addiu', 'sp', 'sp', 0x20],
      ['raw', M.j(layout.PERSUASION_EPILOGUE_LIVE)],
      ['nop'],
      ['label', 'retail'],
      // Replay the displaced retail helper call. Preserve the hook-provided
      // continuation because the helper's JAL replaces $ra.
      ['addiu', 'sp', 'sp', -0x20],
      ['sw', 'ra', 0x1C, 'sp'],
      ['raw', M.jal(layout.PERSUASION_RETAIL_HELPER_LIVE)],
      ['lw', 'a1', 0x4C, 's0'],
      ['lw', 'ra', 0x1C, 'sp'],
      ['addiu', 'sp', 'sp', 0x20],
      ['jr', 'ra'],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_PERSUASION_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_PERSUASION_TARGET_RESOLVER_OFF -
        TYPED_PERSUASION_RESOLVER_OFF) {
      throw new Error('custom persuasion resolver exceeds its reserved module slot');
    }
    return words;
  }

  function buildPersuasionTargetResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'retail'],
      ['nop'],
      ['lw', 'v0', 28, 't0'],
      ['beq', 'v0', 'zero', 'retail'],
      ['nop'],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'retail'],
      // Tail-call the displaced retail lookup so its own return reaches the
      // original Talk-state continuation held in ra.
      ['raw', M.j(layout.PERSUASION_TARGET_RETAIL_LIVE)],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE +
      TYPED_PERSUASION_TARGET_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_CLEANUP_RESOLVER_OFF -
        TYPED_PERSUASION_TARGET_RESOLVER_OFF) {
      throw new Error('custom persuasion-target resolver exceeds its reserved module slot');
    }
    return words;
  }

  function buildCleanupResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      // This resolver remains recognizable so an exported ROM containing the
      // superseded cleanup hook can be upgraded safely. It must not clear the
      // custom profile: retail consumes the reward after this cleanup point.
      // Replay the two displaced retail words if an older hook invokes it.
      ['lui', 'v0', 0x801D],
      ['lw', 'v0', -0x1744, 'v0'],
      // The hook is a JAL, so RA is the exact continuation for the currently
      // relocated post-combat overlay.
      ['jr', 'ra'],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_CLEANUP_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_RETREAT_RESOLVER_OFF - TYPED_CLEANUP_RESOLVER_OFF) {
      throw new Error('custom neutral cleanup resolver exceeds its module slot');
    }
    return words;
  }

  function buildRetreatResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'retail'],
      ['nop'],
      ['lw', 't3', 20, 't0'],
      ['beq', 't3', 'zero', 'retail'],
      ['nop'],
      ['lbu', 't2', 13, 't3'],
      // Zero disables withdrawal for the custom squad. Retail actors cannot
      // reach zero HP through this live predicate, but the explicit branch
      // also makes the editor's "Never" value unambiguous.
      ['beq', 't2', 'zero', 'no_retreat'],
      ['nop'],
      ['lhu', 'v0', 0x20, 's0'],
      ['lhu', 't1', 0x22, 's0'],
      ['beq', 't1', 'zero', 'no_retreat'],
      ['nop'],
      // Match retail: floor(current HP * 100 / max HP).
      ['sll', 'v1', 'v0', 1],
      ['addu', 'v1', 'v1', 'v0'],
      ['sll', 'v1', 'v1', 3],
      ['addu', 'v1', 'v1', 'v0'],
      ['sll', 'v1', 'v1', 2],
      ['divu', 'v1', 't1'],
      ['raw', 0x00001812], // mflo v1
      // Return true when threshold >= current HP percentage.
      ['sltu', 'v0', 't2', 'v1'],
      ['xori', 'v0', 'v0', 1],
      ['raw', M.j(layout.RETREAT_EPILOGUE_LIVE)],
      ['nop'],
      ['label', 'no_retreat'],
      ['addu', 'v0', 'zero', 'zero'],
      ['raw', M.j(layout.RETREAT_EPILOGUE_LIVE)],
      ['nop'],
      ['label', 'retail'],
      ['addu', 'a0', 'zero', 'zero'],
      ['lui', 'a1', 0x0031],
      ['jr', 'ra'],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_RETREAT_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > TYPED_CYCLE_BUDGET_RESOLVER_OFF - TYPED_RETREAT_RESOLVER_OFF) {
      throw new Error('custom retreat resolver exceeds its module slot');
    }
    return words;
  }

  function buildCycleBudgetResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'retail'],
      ['nop'],
      ['lw', 't3', 20, 't0'],
      ['beq', 't3', 'zero', 'retail'],
      ['nop'],
      // Retail starts a neutral encounter with three action-budget points.
      // Three is the untouched pre-action state; restoring it after a choice
      // prevents the result controller from observing that an action ran.
      // Two is the retail post-Fight state and still enables both menu choices,
      // so renewing at two permits another full custom encounter cycle. Both
      // Fight decrement sites and the Talk decrement site dispatch here.
      ['ori', 'v0', 'zero', 2],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'retail'],
      ['ori', 't0', 'zero', TYPED_TALK_BUDGET_SELECTOR],
      ['beq', 't9', 't0', 'retail_talk'],
      ['nop'],
      ['addiu', 'v0', 'v0', -1],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'retail_talk'],
      ['addiu', 'v0', 'v0', -2],
      ['jr', 'ra'],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_CYCLE_BUDGET_RESOLVER_OFF) >>> 0,
      lines);
    if (words.length * 4 > TYPED_REWARD_RESOLVER_OFF -
        TYPED_CYCLE_BUDGET_RESOLVER_OFF) {
      throw new Error('custom cycle-budget resolver exceeds its module slot');
    }
    return words;
  }

  function buildRewardResolver(layout) {
    var scratch = splitOriAddress(TYPED_SCRATCH_UNCACHED);
    var lines = [
      ['lui', 't0', scratch.hi],
      ['ori', 't0', 't0', scratch.lo],
      ['lw', 't1', 16, 't0'],
      ['lui', 't2', (TYPED_PROFILE_TOKEN >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', TYPED_PROFILE_TOKEN & 0xFFFF],
      ['bne', 't1', 't2', 'retail'],
      ['nop'],
      ['lw', 't3', 20, 't0'],
      ['beq', 't3', 'zero', 'retail'],
      ['nop'],
      // Scratch+24 is zero before a result and two after persuasion success.
      // A persuasion result suppresses the custom victory reward.
      ['lw', 't4', 24, 't0'],
      ['bne', 't4', 'zero', 'no_reward'],
      ['nop'],
      ['lbu', 't4', 54, 't3'],
      ['ori', 't5', 'zero', TYPED_REWARD_COUNT],
      ['bne', 't4', 't5', 'no_reward'],
      ['addiu', 't6', 't3', TYPED_REWARD_TABLE_OFF],
      ['lhu', 't4', 2, 't6'],
      ['lhu', 't5', 6, 't6'],
      ['addu', 't4', 't4', 't5'],
      ['lhu', 't5', 10, 't6'],
      ['addu', 't4', 't4', 't5'],
      ['beq', 't4', 'zero', 'no_reward'],
      ['nop'],
      // Keep saved locals above the o32 argument-home area used by a callee.
      ['addiu', 'sp', 'sp', -0x20],
      ['sw', 't0', 0x10, 'sp'],
      ['sw', 't4', 0x14, 'sp'],
      ['sw', 't6', 0x18, 'sp'],
      ['sw', 'ra', 0x1C, 'sp'],
      ['raw', M.jal(RNG_RAM)],
      ['nop'],
      ['lw', 't0', 0x10, 'sp'],
      ['lw', 't4', 0x14, 'sp'],
      ['lw', 't6', 0x18, 'sp'],
      ['lw', 'ra', 0x1C, 'sp'],
      ['addiu', 'sp', 'sp', 0x20],
      ['divu', 'v0', 't4'],
      ['mfhi', 'v0'],
      ['lhu', 't4', 2, 't6'],
      ['sltu', 't5', 'v0', 't4'],
      ['bne', 't5', 'zero', 'reward_selected'],
      ['nop'],
      ['subu', 'v0', 'v0', 't4'],
      ['addiu', 't6', 't6', 4],
      ['lhu', 't4', 2, 't6'],
      ['sltu', 't5', 'v0', 't4'],
      ['bne', 't5', 'zero', 'reward_selected'],
      ['nop'],
      ['addiu', 't6', 't6', 4],
      ['label', 'reward_selected'],
      ['lhu', 's3', 0, 't6'],
      ['beq', 'zero', 'zero', 'custom_done'],
      ['nop'],
      ['label', 'no_reward'],
      ['addu', 's3', 'zero', 'zero'],
      ['label', 'custom_done'],
      // The reward handler is the last custom-context consumer. Clear the
      // encounter state here, after reward selection, with the token last.
      ['sw', 'zero', 8, 't0'],
      ['sw', 'zero', 12, 't0'],
      ['sw', 'zero', 20, 't0'],
      ['sw', 'zero', 24, 't0'],
      ['sw', 'zero', 28, 't0'],
      ['sw', 'zero', 16, 't0'],
      // The retail continuation branches on this mode byte, not the RNG
      // remainder. Recreate the displaced load on the custom exit.
      ['lui', 'v0', 0x8019],
      ['lbu', 'v0', 0x3670, 'v0'],
      // This ROM slab has more than one live placement. Derive its actual
      // continuation from the hook return address instead of a fixed address.
      ['addiu', 't7', 'ra', REWARD_CONTINUATION_FROM_RA],
      ['jr', 't7'],
      ['nop'],
      ['label', 'retail'],
      // Replay ROM 0x1024B4..0x1024D0, which maps the retail RNG remainder
      // and class-table row to the raw reward held in s3.
      ['sll', 'v1', 's1', 2],
      ['addu', 'v1', 'v1', 'v0'],
      ['lui', 'v0', 0x8019],
      ['lbu', 'v0', 0x3670, 'v0'],
      ['sll', 'v1', 'v1', 1],
      ['lui', 's3', 0x801F],
      ['addu', 's3', 's3', 'v1'],
      ['lhu', 's3', -0x24E6, 's3'],
      ['addiu', 't7', 'ra', REWARD_CONTINUATION_FROM_RA],
      ['jr', 't7'],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + TYPED_REWARD_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > SHOP_RESOLVER_OFF - TYPED_REWARD_RESOLVER_OFF) {
      throw new Error('custom reward resolver exceeds its reserved module slot');
    }
    return words;
  }

  function buildTypedHooks(layout, stubs) {
    return {
      selection: S.wordsToBytes([M.jal(stubs.selectionStubRam), M.nop()]),
      materializer: S.wordsToBytes([
        M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', TYPED_MATERIALIZER_DISPATCH_ID)
      ]),
      roundContinuation: S.wordsToBytes([
        M.jal((layout.MOD_BASE + TYPED_ROUND_CONTINUATION_RESOLVER_OFF) >>> 0),
        M.nop()
      ]),
      menuIterator: S.wordsToBytes([
        M.jal(layout.BOOT_RAM),
        M.ori('t9', 'zero', TYPED_MENU_ITERATOR_DISPATCH_ID)
      ]),
      message: S.wordsToBytes([
        M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', TYPED_MESSAGE_DISPATCH_ID)
      ]),
      messageText: S.wordsToBytes([
        M.jal(stubs.messageTextStubRam), M.nop()
      ]),
      persuasion: S.wordsToBytes([
        M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', TYPED_PERSUASION_DISPATCH_ID)
      ]),
      persuasionTarget: S.wordsToBytes([
        M.jal((layout.MOD_BASE + TYPED_PERSUASION_TARGET_RESOLVER_OFF) >>> 0),
        M.nop()
      ]),
      persuasionRecruitTarget: S.wordsToBytes([
        M.jal((layout.MOD_BASE + TYPED_PERSUASION_TARGET_RESOLVER_OFF) >>> 0),
        PERSUASION_RECRUIT_TARGET_ORIGINAL_WORDS[1]
      ]),
      // Selection already loaded this module. Calling the resolver directly
      // avoids invoking the shared DMA bootstrap during final battle teardown.
      cleanup: S.wordsToBytes([
        M.jal((layout.MOD_BASE + TYPED_CLEANUP_RESOLVER_OFF) >>> 0), M.nop()
      ]),
      retreat: S.wordsToBytes([
        M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', TYPED_RETREAT_DISPATCH_ID)
      ]),
      // Selection always loads the shared module before either menu route can
      // reach these overlay hooks. Call the tail resolver directly so the
      // fixed 108-byte bootstrap dispatch table does not grow.
      fightBudget: S.wordsToBytes([
        M.jal((layout.MOD_BASE + TYPED_CYCLE_BUDGET_RESOLVER_OFF) >>> 0),
        M.ori('t9', 'zero', TYPED_FIGHT_BUDGET_SELECTOR)
      ]),
      talkBudget: S.wordsToBytes([
        M.jal((layout.MOD_BASE + TYPED_CYCLE_BUDGET_RESOLVER_OFF) >>> 0),
        M.ori('t9', 'zero', TYPED_TALK_BUDGET_SELECTOR)
      ]),
      reward: S.wordsToBytes([
        M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', TYPED_REWARD_DISPATCH_ID)
      ])
    };
  }

  function buildLegacyCleanupHook(layout) {
    return S.wordsToBytes([
      M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', TYPED_CLEANUP_DISPATCH_ID)
    ]);
  }

  function legacyCleanupHookState(z64, layout) {
    if (!z64 || !layout.LEGACY_CLEANUP_HOOK_ROM) return 'unknown';
    return simpleHookState(z64, layout.LEGACY_CLEANUP_HOOK_ROM,
      LEGACY_CLEANUP_ORIGINAL_WORDS, buildLegacyCleanupHook(layout));
  }

  function legacyRoundContinuationHookState(z64, layout) {
    if (!z64 || !layout.LEGACY_ROUND_CONTINUATION_HOOK_ROM) return 'unknown';
    return simpleHookState(z64, layout.LEGACY_ROUND_CONTINUATION_HOOK_ROM,
      LEGACY_ROUND_CONTINUATION_ORIGINAL_WORDS, S.wordsToBytes([
        M.jal((layout.MOD_BASE + TYPED_ROUND_CONTINUATION_RESOLVER_OFF) >>> 0),
        M.nop()
      ]));
  }

  function typedHookStates(z64, layout, hooks) {
    if (!z64 || !layout.SELECTION_HOOK_ROM) return null;
    var states = {
      selection: simpleHookState(z64, layout.SELECTION_HOOK_ROM,
        SELECTION_ORIGINAL_WORDS, hooks.selection),
      materializer: simpleHookState(z64, layout.MATERIALIZER_HOOK_ROM,
        MATERIALIZER_ORIGINAL_WORDS, hooks.materializer),
      roundContinuation: simpleHookState(z64,
        layout.ROUND_CONTINUATION_HOOK_ROM,
        ROUND_CONTINUATION_ORIGINAL_WORDS, hooks.roundContinuation),
      menuIterator: simpleHookState(z64, layout.MENU_ITERATOR_HOOK_ROM,
        MENU_ITERATOR_ORIGINAL_WORDS, hooks.menuIterator),
      message: simpleHookState(z64, layout.MESSAGE_HOOK_ROM,
        MESSAGE_ORIGINAL_WORDS, hooks.message),
      messageText: simpleHookState(z64, layout.MESSAGE_TEXT_HOOK_ROM,
        MESSAGE_TEXT_ORIGINAL_WORDS, hooks.messageText),
      persuasion: simpleHookState(z64, layout.PERSUASION_HOOK_ROM,
        PERSUASION_ORIGINAL_WORDS, hooks.persuasion),
      persuasionRecruitTarget: simpleHookState(z64,
        layout.PERSUASION_RECRUIT_TARGET_HOOK_ROM,
        PERSUASION_RECRUIT_TARGET_ORIGINAL_WORDS,
        hooks.persuasionRecruitTarget),
      cleanup: simpleHookState(z64, layout.CLEANUP_HOOK_ROM,
        CLEANUP_ORIGINAL_WORDS, hooks.cleanup),
      retreat: simpleHookState(z64, layout.RETREAT_HOOK_ROM,
        RETREAT_ORIGINAL_WORDS, hooks.retreat),
      fightBudget: simpleHookState(z64, layout.FIGHT_BUDGET_HOOK_ROM,
        FIGHT_BUDGET_ORIGINAL_WORDS, hooks.fightBudget),
      talkBudget: simpleHookState(z64, layout.TALK_BUDGET_HOOK_ROM,
        TALK_BUDGET_ORIGINAL_WORDS, hooks.talkBudget),
      fightResultBudget: simpleHookState(z64,
        layout.FIGHT_RESULT_BUDGET_HOOK_ROM,
        FIGHT_BUDGET_ORIGINAL_WORDS, hooks.fightBudget),
      reward: simpleHookState(z64, layout.REWARD_HOOK_ROM,
        REWARD_ORIGINAL_WORDS, hooks.reward)
    };
    for (var targetIndex = 0;
      targetIndex < layout.PERSUASION_TARGET_HOOK_ROMS.length; targetIndex++) {
      states['persuasionTarget' + targetIndex] = simpleHookState(z64,
        layout.PERSUASION_TARGET_HOOK_ROMS[targetIndex],
        PERSUASION_TARGET_ORIGINAL_WORDS, hooks.persuasionTarget);
    }
    return states;
  }

  function bytesAtEqual(bytes, off, expected) {
    if (off < 0 || off + expected.length > bytes.length) return false;
    for (var i = 0; i < expected.length; i++) if (bytes[off + i] !== expected[i]) return false;
    return true;
  }

  function buildBanditsMessageArchiveWrite(z64, enabled) {
    if (!OB64.findArchives || !OB64.extractArchive || !OB64.lh5Compress ||
        !OB64.buildLHAArchiveFixedSlot) {
      throw new Error('MESWIN archive tools are unavailable');
    }
    var archives = OB64.findArchives(z64);
    var archive = archives[815];
    if (!archive) throw new Error('MESWIN archive #815 is missing');
    var decoded = OB64.extractArchive(z64, archive);
    if (decoded.length < 631 * 4 || readU32(decoded, 0) !== 631 * 4) {
      throw new Error('MESWIN archive #815 has an invalid pointer table');
    }
    var start = readU32(decoded, BANDITS_MESSAGE_ENTRY * 4);
    var end = readU32(decoded, (BANDITS_MESSAGE_ENTRY + 1) * 4);
    if (start < 631 * 4 || end < start || end > decoded.length) {
      throw new Error('MESWIN entry 15 has invalid bounds');
    }
    var isEmpty = end - start === 1 && decoded[start] === 0;
    var isBandits = end - start === BANDITS_MESSAGE_BYTES.length &&
      bytesAtEqual(decoded, start, BANDITS_MESSAGE_BYTES);
    if (enabled ? isBandits : isEmpty) return null;
    if (!enabled && !isEmpty && !isBandits) return null;
    if (!isEmpty && !isBandits) {
      throw new Error('MESWIN entry 15 contains unrecognized data; refusing to replace another patch');
    }
    var replacement = enabled ? BANDITS_MESSAGE_BYTES : new Uint8Array([0]);
    var delta = replacement.length - (end - start);
    var next = new Uint8Array(decoded.length + delta);
    next.set(decoded.subarray(0, start), 0);
    next.set(replacement, start);
    next.set(decoded.subarray(end), start + replacement.length);
    for (var pointerIndex = BANDITS_MESSAGE_ENTRY + 1; pointerIndex < 631; pointerIndex++) {
      writeU32(next, pointerIndex * 4, readU32(decoded, pointerIndex * 4) + delta);
    }
    var compressed = OB64.lh5Compress(next);
    if (compressed.length > archive.compSize) {
      throw new Error('Bandits! MESWIN stream exceeds archive #815 by ' +
        (compressed.length - archive.compSize) + ' bytes');
    }
    var verify = OB64.lh5Decompress(compressed, next.length);
    if (!arraysEqual(verify, next)) throw new Error('Bandits! MESWIN recompression failed readback');
    var header = z64.subarray(archive.offset, archive.offset + archive.totalHeaderSize);
    var archiveBytes = OB64.buildLHAArchiveFixedSlot(
      compressed, next, header, archive.totalHeaderSize + archive.compSize);
    return {
      offset: archive.offset,
      label: enabled ? 'custom neutral Bandits! MESWIN entry' : 'restored empty MESWIN entry 15',
      bytes: archiveBytes
    };
  }

  function buildDropResolver(layout) {
    var tableAddr = (layout.MOD_BASE + DROP_TABLE_OFF + 8) >>> 0;
    var lines = [
      ['sll', 't0', 's4', 3],
      ['lui', 't4', (tableAddr >>> 16) & 0xFFFF],
      ['ori', 't4', 't4', tableAddr & 0xFFFF],
      ['addu', 't4', 't4', 't0'],
      ['lbu', 't0', 0, 't4'],
      ['beq', 't0', 'zero', 'retail'],
      ['nop'],
      ['lhu', 't0', 2, 't4'],
      ['lhu', 't1', 4, 't4'],
      ['addu', 't0', 't0', 't1'],
      ['lhu', 't1', 6, 't4'],
      ['addu', 't0', 't0', 't1'],
      ['beq', 't0', 'zero', 'retail'],
      ['nop'],
      ['addiu', 'sp', 'sp', -16],
      ['sw', 'ra', 12, 'sp'],
      ['sw', 't0', 8, 'sp'],
      ['sw', 't4', 4, 'sp'],
      ['raw', M.jal(RNG_RAM)],
      ['nop'],
      ['lw', 't4', 4, 'sp'],
      ['lw', 't0', 8, 'sp'],
      ['lw', 'ra', 12, 'sp'],
      ['addiu', 'sp', 'sp', 16],
      ['divu', 'v0', 't0'],
      ['mfhi', 'v0'],
      ['lhu', 't0', 2, 't4'],
      ['sltu', 't1', 'v0', 't0'],
      ['bne', 't1', 'zero', 'slot0'],
      ['nop'],
      ['subu', 'v0', 'v0', 't0'],
      ['lhu', 't0', 4, 't4'],
      ['sltu', 't1', 'v0', 't0'],
      ['bne', 't1', 'zero', 'slot1'],
      ['nop'],
      ['ori', 'v0', 'zero', 2],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'slot1'],
      ['ori', 'v0', 'zero', 1],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'slot0'],
      ['addu', 'v0', 'zero', 'zero'],
      ['jr', 'ra'],
      ['nop'],
      ['label', 'retail'],
      ['addiu', 'sp', 'sp', -8],
      ['sw', 'ra', 4, 'sp'],
      ['raw', M.jal(RNG_RAM)],
      ['nop'],
      ['lw', 'ra', 4, 'sp'],
      ['addiu', 'sp', 'sp', 8],
      ['jr', 'ra'],
      ['nop']
    ];
    var words = assemble((layout.MOD_BASE + DROP_RESOLVER_OFF) >>> 0, lines);
    if (words.length * 4 > DROP_TABLE_OFF - DROP_RESOLVER_OFF) {
      throw new Error('weighted drop resolver exceeds its 0x200-byte module slot');
    }
    return words;
  }

  function buildDropTable(overrides) {
    var table = new Uint8Array(8 + DROP_CLASS_COUNT * DROP_ENTRY_STRIDE);
    writeU32(table, 0, DROP_MAGIC);
    writeU16(table, 4, DROP_CLASS_COUNT);
    writeU16(table, 6, DROP_ENTRY_STRIDE);
    var seen = {};
    overrides = overrides || [];
    for (var i = 0; i < overrides.length; i++) {
      var source = overrides[i] || {};
      var classId = Number(source.classId);
      if (!Number.isInteger(classId) || classId <= 0 || classId >= DROP_CLASS_COUNT) {
        throw new Error('weighted drop override #' + i + ' has invalid class ID ' + classId);
      }
      if (seen[classId]) throw new Error('duplicate weighted drop override for class ' + classId);
      seen[classId] = true;
      var weights = source.weights;
      if (!Array.isArray(weights) || weights.length !== 3) {
        throw new Error('class ' + classId + ' weighted drop override must contain exactly three slot weights');
      }
      var total = 0;
      var off = 8 + classId * DROP_ENTRY_STRIDE;
      table[off] = 1;
      for (var slot = 0; slot < 3; slot++) {
        var weight = Number(weights[slot]);
        if (!Number.isInteger(weight) || weight < 0 || weight > 0xFFFF) {
          throw new Error('class ' + classId + ' slot ' + (slot + 1) + ' weight must be 0..65535');
        }
        total += weight;
        writeU16(table, off + 2 + slot * 2, weight);
      }
      if (!total) throw new Error('class ' + classId + ' weighted drop override has zero total weight');
    }
    return table;
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

  function buildNeutralSharedBlob(options, layout) {
    options = options || {};
    var customProfiles = options.customNeutralSquads || [];
    var squadBlob = S.buildBlob(options.squadOverrides || [], layout);
    var squadLimit = customProfiles.length ? TYPED_PROFILE_TABLE_OFF : RATE_RESOLVER_OFF;
    if (squadBlob.length > squadLimit) {
      var maxSquads = Math.floor((squadLimit - S.consts.ENTRIES_OFF) /
        S.consts.ENTRY_STRIDE);
      throw new Error('neutral runtime blob leaves room for at most ' + maxSquads +
        ' scenario squad overrides' + (customProfiles.length ?
          ' while custom neutral squads are enabled' : ''));
    }
    var pieces = [];
    var size = squadBlob.length;
    if (options.scenarioRateOverrides && options.scenarioRateOverrides.length) {
      var rateResolver = S.wordsToBytes(buildRateResolver(layout));
      var rateTable = buildRateTable(options.scenarioRateOverrides);
      pieces.push({ off: RATE_RESOLVER_OFF, bytes: rateResolver });
      pieces.push({ off: RATE_TABLE_OFF, bytes: rateTable });
      size = Math.max(size, RATE_TABLE_OFF + rateTable.length);
    }
    if (options.weightedDropOverrides && options.weightedDropOverrides.length) {
      var dropResolver = S.wordsToBytes(buildDropResolver(layout));
      var dropTable = buildDropTable(options.weightedDropOverrides);
      pieces.push({ off: DROP_RESOLVER_OFF, bytes: dropResolver });
      pieces.push({ off: DROP_TABLE_OFF, bytes: dropTable });
      size = Math.max(size, DROP_TABLE_OFF + dropTable.length);
    }
    if (customProfiles.length) {
      if (!layout.supportsNeutralCustomSquads || !layout.SELECTION_HOOK_ROM) {
        throw new Error('custom neutral squads are unavailable for this ROM revision');
      }
      var profileTable = buildTypedProfileTable(customProfiles, layout);
      pieces.push({ off: TYPED_PROFILE_TABLE_OFF, bytes: profileTable.bytes });
      pieces.push({ off: TYPED_SELECTION_RESOLVER_OFF,
        bytes: S.wordsToBytes(buildSelectionResolver(layout)) });
      pieces.push({ off: TYPED_MATERIALIZER_RESOLVER_OFF,
        bytes: S.wordsToBytes(buildMaterializerResolver(layout)) });
      var roundContinuationResolver = S.wordsToBytes(
        buildRoundContinuationResolver(layout));
      pieces.push({ off: TYPED_ROUND_CONTINUATION_RESOLVER_OFF,
        bytes: roundContinuationResolver });
      var menuIteratorResolver = S.wordsToBytes(
        buildMenuIteratorResolver(layout));
      pieces.push({ off: TYPED_MENU_ITERATOR_RESOLVER_OFF,
        bytes: menuIteratorResolver });
      pieces.push({ off: TYPED_MESSAGE_RESOLVER_OFF,
        bytes: S.wordsToBytes(buildMessageResolver(layout)) });
      pieces.push({ off: TYPED_PERSUASION_RESOLVER_OFF,
        bytes: S.wordsToBytes(buildPersuasionResolver(layout)) });
      var persuasionTargetResolver = S.wordsToBytes(
        buildPersuasionTargetResolver(layout));
      pieces.push({ off: TYPED_PERSUASION_TARGET_RESOLVER_OFF,
        bytes: persuasionTargetResolver });
      var cleanupResolver = S.wordsToBytes(buildCleanupResolver(layout));
      pieces.push({ off: TYPED_CLEANUP_RESOLVER_OFF, bytes: cleanupResolver });
      var retreatResolver = S.wordsToBytes(buildRetreatResolver(layout));
      pieces.push({ off: TYPED_RETREAT_RESOLVER_OFF, bytes: retreatResolver });
      var cycleBudgetResolver = S.wordsToBytes(buildCycleBudgetResolver(layout));
      pieces.push({ off: TYPED_CYCLE_BUDGET_RESOLVER_OFF, bytes: cycleBudgetResolver });
      var rewardResolver = S.wordsToBytes(buildRewardResolver(layout));
      pieces.push({ off: TYPED_REWARD_RESOLVER_OFF, bytes: rewardResolver });
      size = Math.max(size,
        TYPED_ROUND_CONTINUATION_RESOLVER_OFF + roundContinuationResolver.length,
        TYPED_MENU_ITERATOR_RESOLVER_OFF + menuIteratorResolver.length,
        TYPED_PERSUASION_TARGET_RESOLVER_OFF + persuasionTargetResolver.length,
        TYPED_CLEANUP_RESOLVER_OFF + cleanupResolver.length,
        TYPED_RETREAT_RESOLVER_OFF + retreatResolver.length,
        TYPED_CYCLE_BUDGET_RESOLVER_OFF + cycleBudgetResolver.length,
        TYPED_REWARD_RESOLVER_OFF + rewardResolver.length,
        TYPED_PROFILE_TABLE_OFF + profileTable.bytes.length);
    }
    if (options.shopOverrides && options.shopOverrides.length) {
      var shopResolver = S.wordsToBytes(buildShopResolver(layout));
      var shopTable = buildShopTable(options.shopOverrides, options.shopCount);
      pieces.push({ off: SHOP_RESOLVER_OFF, bytes: shopResolver });
      pieces.push({ off: SHOP_TABLE_OFF, bytes: shopTable });
      size = Math.max(size, SHOP_TABLE_OFF + shopTable.length);
    }
    while (size % 8) size++;
    if (size > 0xFFFF) throw new Error('neutral shared runtime blob exceeds the single-DMA length encoding');
    var blob = new Uint8Array(size);
    blob.set(squadBlob, 0);
    for (var i = 0; i < pieces.length; i++) blob.set(pieces[i].bytes, pieces[i].off);
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

  function buildNeutralSharedBootstrap(blobLen, layout) {
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
      ['lui', 't0', (layout.BOOT_RAM >>> 16) & 0xFFFF],
      ['ori', 't0', 't0', 0], // patched below after assembly locates dispatch_table
      ['sll', 't1', 't9', 2],
      ['addu', 't0', 't0', 't1'],
      ['lw', 't0', 0, 't0'],
      ['jr', 't0'],
      ['nop'],
      ['label', 'dispatch_table'],
      ['raw', (layout.MOD_BASE + SQUAD_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + SHOP_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + RATE_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + RATE_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + DROP_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + TYPED_SELECTION_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + TYPED_MATERIALIZER_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + TYPED_MESSAGE_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + TYPED_PERSUASION_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + TYPED_MENU_ITERATOR_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + TYPED_RETREAT_RESOLVER_OFF) >>> 0],
      ['raw', (layout.MOD_BASE + TYPED_REWARD_RESOLVER_OFF) >>> 0]
    ];
    var words = assemble(layout.BOOT_RAM, lines);
    // The dispatch table begins after fifteen executable words.
    var tableAddr = (layout.BOOT_RAM + 15 * 4) >>> 0;
    words[8] = M.lui('t0', (tableAddr >>> 16) & 0xFFFF);
    words[9] = M.ori('t0', 't0', tableAddr & 0xFFFF);
    if (words.length * 4 > 108) throw new Error('neutral shared bootstrap exceeds the 108-byte cave');
    return words;
  }

  function buildSharedContinuation(blobLen, layout, includeTypedStubs) {
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
    if (includeTypedStubs) lines = lines.concat([
      // The selection hook jumps here so its volatile class/pointer pair
      // survives a possible first-use DMA before module 5 scans profiles.
      ['label', 'selection_stub'],
      ['lui', 't0', (TYPED_SCRATCH_UNCACHED >>> 16) & 0xFFFF],
      ['ori', 't0', 't0', TYPED_SCRATCH_UNCACHED & 0xFFFF],
      ['sw', 'v0', 0, 't0'],
      ['sw', 'v1', 4, 't0'],
      ['raw', M.jal(layout.BOOT_RAM)],
      ['ori', 't9', 'zero', TYPED_SELECTION_DISPATCH_ID],
      // Generic message presentation can run before any neutral encounter.
      // Do not invoke the DMA bootstrap there: its cache helpers may clobber
      // the retail v1 offset. Dispatch only when the typed module is already
      // resident; otherwise replay the two displaced retail instructions.
      ['label', 'message_text_stub'],
      ['lui', 't0', 0xA040],
      ['lw', 't1', 0, 't0'],
      ['lui', 't2', (layout.SENTINEL >>> 16) & 0xFFFF],
      ['ori', 't2', 't2', layout.SENTINEL & 0xFFFF],
      ['bne', 't1', 't2', 'message_text_retail'],
      ['nop'],
      ['raw', M.j((layout.MOD_BASE + TYPED_MESSAGE_RESOLVER_OFF) >>> 0)],
      ['ori', 't9', 'zero', TYPED_MESSAGE_POINTER_SELECTOR],
      ['label', 'message_text_retail'],
      ['addu', 'v1', 's0', 'v1'],
      ['jr', 'ra'],
      ['sw', 'v1', 4, 'a0']
    ]);
    var words = assemble(layout.CACHE_CONT_RAM, lines);
    if (words.length * 4 > CACHE_CONT_BYTES) {
      throw new Error('shared cache/DMA continuation exceeds its reserved cave slot');
    }
    if (includeTypedStubs) {
      words.selectionStubRam = (layout.CACHE_CONT_RAM + 27 * 4) >>> 0;
      if (words[27] !== M.lui('t0', (TYPED_SCRATCH_UNCACHED >>> 16) & 0xFFFF)) {
        throw new Error('custom neutral continuation stub layout drifted');
      }
      words.messageTextStubRam = (layout.CACHE_CONT_RAM + 33 * 4) >>> 0;
      if (words[33] !== M.lui('t0', 0xA040)) {
        throw new Error('custom neutral message-text stub layout drifted');
      }
    }
    return words;
  }

  function buildRuntimeOverrideWrites(squadOverrides, shopOverrides, shopCount, romOrLayout, neutralOptions) {
    // Four-argument callers predate neutral runtime support. Preserve their
    // exact OBM2 write plan, including its write count, unless the caller
    // explicitly asks this composer to own the two neutral hooks.
    var manageNeutralHooks = arguments.length > 4;
    neutralOptions = neutralOptions || {};
    var scenarioRateOverrides = neutralOptions.scenarioRateOverrides || [];
    var weightedDropOverrides = neutralOptions.weightedDropOverrides || [];
    var customNeutralSquads = neutralOptions.customNeutralSquads || [];
    var hasNeutralRuntime = scenarioRateOverrides.length > 0 ||
      weightedDropOverrides.length > 0 || customNeutralSquads.length > 0;
    var layout = hasNeutralRuntime ? neutralRuntimeLayout(romOrLayout) : runtimeLayout(romOrLayout);
    if (!layout.supportsShopOverrides) throw new Error('shop runtime overrides are unavailable for this ROM revision');
    if (hasNeutralRuntime && !layout.supportsNeutralRuntimeOverrides) {
      throw new Error('neutral runtime overrides are unavailable for this ROM revision');
    }
    if (customNeutralSquads.length && !layout.supportsNeutralCustomSquads) {
      throw new Error('custom neutral squads are unavailable for this ROM revision');
    }
    if ((!shopOverrides || !shopOverrides.length) && !hasNeutralRuntime) {
      throw new Error('shared runtime build requires at least one shop or neutral runtime override');
    }
    var z64 = romOrLayout && romOrLayout.z64;
    if (z64 && shopHookState(z64, layout) === 'foreign') {
      throw new Error('shop producer hook contains unrecognized bytes; refusing to overwrite another patch');
    }
    if (manageNeutralHooks && z64 && rateHookState(z64, layout) === 'foreign') {
      throw new Error('neutral-rate RNG hook contains unrecognized bytes; refusing to overwrite another patch');
    }
    if (manageNeutralHooks && z64 && dropHookState(z64, layout) === 'foreign') {
      throw new Error('creature-drop RNG hook contains unrecognized bytes; refusing to overwrite another patch');
    }
    var blob = hasNeutralRuntime
      ? buildNeutralSharedBlob({
          squadOverrides: squadOverrides || [],
          shopOverrides: shopOverrides || [],
          shopCount: shopCount,
          scenarioRateOverrides: scenarioRateOverrides,
          weightedDropOverrides: weightedDropOverrides,
          customNeutralSquads: customNeutralSquads
        }, layout)
      : buildSharedBlob(squadOverrides || [], shopOverrides, shopCount, layout);
    var boot = S.wordsToBytes(hasNeutralRuntime
      ? buildNeutralSharedBootstrap(blob.length, layout)
      : buildSharedBootstrap(blob.length, layout));
    var continuationWords = buildSharedContinuation(blob.length, layout, hasNeutralRuntime);
    var cont = S.wordsToBytes(continuationWords);
    var squadHook = (squadOverrides && squadOverrides.length)
      ? S.wordsToBytes([M.jal(layout.BOOT_RAM), M.ori('t9', 'zero', SQUAD_DISPATCH_ID)])
      : S.wordsToBytes([DISP_SLTIU, DISP_XORI]);
    var shopHook = (shopOverrides && shopOverrides.length)
      ? buildShopHook(layout) : S.wordsToBytes(layout.SHOP_ORIGINAL_WORDS);
    var rateHook = scenarioRateOverrides.length
      ? buildRateHook(layout) : S.wordsToBytes(RATE_ORIGINAL_WORDS);
    var dropHook = weightedDropOverrides.length
      ? buildDropHook(layout) : S.wordsToBytes(DROP_ORIGINAL_WORDS);
    var writes = [
      { offset: layout.HOOK_ROM, label: 'shared squad dispatch hook', bytes: squadHook },
      { offset: layout.SHOP_HOOK_ROM, label: shopOverrides && shopOverrides.length
        ? 'per-shop source-list dispatch hook' : 'retail shop source-list hook', bytes: shopHook }
    ];
    if (manageNeutralHooks) {
      writes.push({ offset: layout.RATE_HOOK_ROM, label: scenarioRateOverrides.length
        ? 'per-scenario neutral-rate dispatch hook' : 'retail neutral-rate RNG hook', bytes: rateHook });
      writes.push({ offset: layout.DROP_HOOK_ROM, label: weightedDropOverrides.length
        ? 'weighted creature-drop dispatch hook' : 'retail creature-drop RNG hook', bytes: dropHook });
    }
    if (manageNeutralHooks && layout.SELECTION_HOOK_ROM) {
      var typedHooks = buildTypedHooks(layout, continuationWords);
      var typedStates = z64 ? typedHookStates(z64, layout, typedHooks) : null;
      var typedEnabled = customNeutralSquads.length > 0;
      var legacyCleanupState = z64
        ? legacyCleanupHookState(z64, layout) : 'unknown';
      var legacyRoundContinuationState = z64
        ? legacyRoundContinuationHookState(z64, layout) : 'unknown';
      var preEligibilityPersuasionState = z64
        ? preEligibilityPersuasionHookState(z64, layout) : 'unknown';
      if (typedEnabled && typedStates) {
        for (var typedKey in typedStates) {
          if (typedStates[typedKey] === 'foreign') {
            throw new Error('custom neutral ' + typedKey +
              ' hook contains unrecognized bytes; refusing to overwrite another patch');
          }
        }
        if (preEligibilityPersuasionState === 'foreign') {
          throw new Error('superseded custom neutral persuasion hook contains ' +
            'unrecognized bytes; refusing to install a second persuasion hook');
        }
        if (legacyCleanupState === 'foreign') {
          throw new Error('superseded custom neutral cleanup hook contains ' +
            'unrecognized bytes; refusing to install a second cleanup hook');
        }
        if (legacyRoundContinuationState === 'foreign') {
          throw new Error('superseded custom neutral round hook contains ' +
            'unrecognized bytes; refusing to install a second round hook');
        }
      }
      var typedSpecs = [
        ['selection', layout.SELECTION_HOOK_ROM, typedHooks.selection, SELECTION_ORIGINAL_WORDS],
        ['materializer', layout.MATERIALIZER_HOOK_ROM, typedHooks.materializer, MATERIALIZER_ORIGINAL_WORDS],
        ['roundContinuation', layout.ROUND_CONTINUATION_HOOK_ROM,
          typedHooks.roundContinuation, ROUND_CONTINUATION_ORIGINAL_WORDS],
        ['menuIterator', layout.MENU_ITERATOR_HOOK_ROM,
          typedHooks.menuIterator, MENU_ITERATOR_ORIGINAL_WORDS],
        ['message', layout.MESSAGE_HOOK_ROM, typedHooks.message, MESSAGE_ORIGINAL_WORDS],
        ['messageText', layout.MESSAGE_TEXT_HOOK_ROM,
          typedHooks.messageText, MESSAGE_TEXT_ORIGINAL_WORDS],
        ['persuasion', layout.PERSUASION_HOOK_ROM, typedHooks.persuasion, PERSUASION_ORIGINAL_WORDS],
        ['persuasionRecruitTarget', layout.PERSUASION_RECRUIT_TARGET_HOOK_ROM,
          typedHooks.persuasionRecruitTarget, PERSUASION_RECRUIT_TARGET_ORIGINAL_WORDS],
        ['cleanup', layout.CLEANUP_HOOK_ROM, typedHooks.cleanup, CLEANUP_ORIGINAL_WORDS],
        ['retreat', layout.RETREAT_HOOK_ROM, typedHooks.retreat, RETREAT_ORIGINAL_WORDS],
        ['fightBudget', layout.FIGHT_BUDGET_HOOK_ROM, typedHooks.fightBudget,
          FIGHT_BUDGET_ORIGINAL_WORDS],
        ['talkBudget', layout.TALK_BUDGET_HOOK_ROM, typedHooks.talkBudget,
          TALK_BUDGET_ORIGINAL_WORDS],
        ['fightResultBudget', layout.FIGHT_RESULT_BUDGET_HOOK_ROM,
          typedHooks.fightBudget, FIGHT_BUDGET_ORIGINAL_WORDS],
        ['reward', layout.REWARD_HOOK_ROM, typedHooks.reward, REWARD_ORIGINAL_WORDS]
      ];
      for (var targetHookIndex = 0;
        targetHookIndex < layout.PERSUASION_TARGET_HOOK_ROMS.length; targetHookIndex++) {
        typedSpecs.push(['persuasionTarget' + targetHookIndex,
          layout.PERSUASION_TARGET_HOOK_ROMS[targetHookIndex],
          typedHooks.persuasionTarget, PERSUASION_TARGET_ORIGINAL_WORDS]);
      }
      for (var typedIndex = 0; typedIndex < typedSpecs.length; typedIndex++) {
        var typedSpec = typedSpecs[typedIndex];
        if (!typedEnabled && (!typedStates || typedStates[typedSpec[0]] !== 'shared')) continue;
        // Retail performs this cleanup before its reward handler. Leave that
        // site untouched and clear custom context from the reward resolver.
        var installTypedHook = typedEnabled && typedSpec[0] !== 'cleanup';
        writes.push({
          offset: typedSpec[1],
          label: installTypedHook ? ('custom neutral ' + typedSpec[0] + ' hook')
            : ('restored retail neutral ' + typedSpec[0] + ' hook'),
          bytes: installTypedHook ? typedSpec[2] : S.wordsToBytes(typedSpec[3])
        });
      }
      if (preEligibilityPersuasionState === 'shared') {
        writes.push({
          offset: PRE_ELIGIBILITY_PERSUASION_HOOK_ROM,
          label: 'restored superseded pre-eligibility persuasion hook',
          bytes: S.wordsToBytes(PRE_ELIGIBILITY_PERSUASION_ORIGINAL_WORDS)
        });
      }
      if (legacyCleanupState === 'shared') {
        writes.push({
          offset: layout.LEGACY_CLEANUP_HOOK_ROM,
          label: 'restored premature custom neutral cleanup hook',
          bytes: S.wordsToBytes(LEGACY_CLEANUP_ORIGINAL_WORDS)
        });
      }
      if (legacyRoundContinuationState === 'shared') {
        writes.push({
          offset: layout.LEGACY_ROUND_CONTINUATION_HOOK_ROM,
          label: 'restored superseded custom neutral terminal-state round hook',
          bytes: S.wordsToBytes(LEGACY_ROUND_CONTINUATION_ORIGINAL_WORDS)
        });
      }
      // OBMA stored fixed text in retail MESWIN entry 15. OBMB keeps that
      // entry retail-empty and resolves each profile's runtime string instead.
      var messageWrite = z64 ? buildBanditsMessageArchiveWrite(z64, false) : null;
      if (messageWrite) writes.push(messageWrite);
    }
    writes.push(
      { offset: layout.BOOT_ROM, label: 'shared bootstrap (sentinel dispatch)', bytes: boot },
      { offset: layout.CACHE_CONT_Z64, label: 'shared cache-invalidate + DMA continuation', bytes: cont },
      { offset: layout.TAIL_Z64, label: hasNeutralRuntime
        ? 'shared OBMB runtime blob' : 'shared OBM2 runtime blob', bytes: blob }
    );
    return {
      crcWindow: true,
      writes: writes,
      blob: blob,
      shopOverrideCount: shopOverrides ? shopOverrides.length : 0,
      squadCount: squadOverrides ? squadOverrides.length : 0,
      scenarioRateOverrideCount: scenarioRateOverrides.length,
      weightedDropOverrideCount: weightedDropOverrides.length,
      customNeutralSquadCount: customNeutralSquads.length,
      sentinel: layout.SENTINEL
    };
  }

  function restoreShopHook(z64, romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    if (shopHookState(z64, layout) === 'foreign') {
      throw new Error('shop producer hook contains unrecognized bytes; refusing to restore over another patch');
    }
    z64.set(S.wordsToBytes(layout.SHOP_ORIGINAL_WORDS), layout.SHOP_HOOK_ROM);
  }

  function restoreNeutralHooks(z64, romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    if (rateHookState(z64, layout) === 'foreign') {
      throw new Error('neutral-rate RNG hook contains unrecognized bytes; refusing to restore over another patch');
    }
    if (dropHookState(z64, layout) === 'foreign') {
      throw new Error('creature-drop RNG hook contains unrecognized bytes; refusing to restore over another patch');
    }
    if (layout.SELECTION_HOOK_ROM) {
      var continuation = buildSharedContinuation(8, layout, true);
      var hooks = buildTypedHooks(layout, continuation);
      var states = typedHookStates(z64, layout, hooks);
      var restoreSpecs = [
        ['selection', layout.SELECTION_HOOK_ROM, SELECTION_ORIGINAL_WORDS],
        ['materializer', layout.MATERIALIZER_HOOK_ROM, MATERIALIZER_ORIGINAL_WORDS],
        ['roundContinuation', layout.ROUND_CONTINUATION_HOOK_ROM,
          ROUND_CONTINUATION_ORIGINAL_WORDS],
        ['menuIterator', layout.MENU_ITERATOR_HOOK_ROM,
          MENU_ITERATOR_ORIGINAL_WORDS],
        ['message', layout.MESSAGE_HOOK_ROM, MESSAGE_ORIGINAL_WORDS],
        ['messageText', layout.MESSAGE_TEXT_HOOK_ROM, MESSAGE_TEXT_ORIGINAL_WORDS],
        ['persuasion', layout.PERSUASION_HOOK_ROM, PERSUASION_ORIGINAL_WORDS],
        ['persuasionRecruitTarget', layout.PERSUASION_RECRUIT_TARGET_HOOK_ROM,
          PERSUASION_RECRUIT_TARGET_ORIGINAL_WORDS],
        ['cleanup', layout.CLEANUP_HOOK_ROM, CLEANUP_ORIGINAL_WORDS],
        ['retreat', layout.RETREAT_HOOK_ROM, RETREAT_ORIGINAL_WORDS],
        ['fightBudget', layout.FIGHT_BUDGET_HOOK_ROM, FIGHT_BUDGET_ORIGINAL_WORDS],
        ['talkBudget', layout.TALK_BUDGET_HOOK_ROM, TALK_BUDGET_ORIGINAL_WORDS],
        ['fightResultBudget', layout.FIGHT_RESULT_BUDGET_HOOK_ROM,
          FIGHT_BUDGET_ORIGINAL_WORDS],
        ['reward', layout.REWARD_HOOK_ROM, REWARD_ORIGINAL_WORDS]
      ];
      for (var targetRestoreIndex = 0;
        targetRestoreIndex < layout.PERSUASION_TARGET_HOOK_ROMS.length;
        targetRestoreIndex++) {
        restoreSpecs.push(['persuasionTarget' + targetRestoreIndex,
          layout.PERSUASION_TARGET_HOOK_ROMS[targetRestoreIndex],
          PERSUASION_TARGET_ORIGINAL_WORDS]);
      }
      for (var restoreIndex = 0; restoreIndex < restoreSpecs.length; restoreIndex++) {
        var restoreSpec = restoreSpecs[restoreIndex];
        if (states[restoreSpec[0]] === 'shared') {
          z64.set(S.wordsToBytes(restoreSpec[2]), restoreSpec[1]);
        }
      }
      if (preEligibilityPersuasionHookState(z64, layout) === 'shared') {
        z64.set(S.wordsToBytes(PRE_ELIGIBILITY_PERSUASION_ORIGINAL_WORDS),
          PRE_ELIGIBILITY_PERSUASION_HOOK_ROM);
      }
      if (legacyCleanupHookState(z64, layout) === 'shared') {
        z64.set(S.wordsToBytes(LEGACY_CLEANUP_ORIGINAL_WORDS),
          layout.LEGACY_CLEANUP_HOOK_ROM);
      }
      if (legacyRoundContinuationHookState(z64, layout) === 'shared') {
        z64.set(S.wordsToBytes(LEGACY_ROUND_CONTINUATION_ORIGINAL_WORDS),
          layout.LEGACY_ROUND_CONTINUATION_HOOK_ROM);
      }
      var messageWrite = buildBanditsMessageArchiveWrite(z64, false);
      if (messageWrite) z64.set(messageWrite.bytes, messageWrite.offset);
    }
    z64.set(S.wordsToBytes(RATE_ORIGINAL_WORDS), layout.RATE_HOOK_ROM);
    z64.set(S.wordsToBytes(DROP_ORIGINAL_WORDS), layout.DROP_HOOK_ROM);
  }

  function restoreAll(z64, romOrLayout) {
    S.restoreVanilla(z64, romOrLayout);
    restoreShopHook(z64, romOrLayout);
    restoreNeutralHooks(z64, romOrLayout);
  }

  function patchRegions(romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    var regions = S.patchRegions(romOrLayout).slice();
    regions.push({ kind: 'rom', start: layout.SHOP_HOOK_ROM, size: 24, label: 'shop source-list dispatch hook' });
    regions.push({ kind: 'rom', start: layout.RATE_HOOK_ROM, size: 8, label: 'neutral-rate RNG dispatch hook' });
    regions.push({ kind: 'rom', start: layout.DROP_HOOK_ROM, size: 8, label: 'creature-drop RNG dispatch hook' });
    if (layout.SELECTION_HOOK_ROM) {
      regions.push({ kind: 'rom', start: layout.SELECTION_HOOK_ROM, size: 8,
        label: 'custom neutral selection hook' });
      regions.push({ kind: 'rom', start: layout.MATERIALIZER_HOOK_ROM, size: 8,
        label: 'custom neutral materializer hook' });
      regions.push({ kind: 'rom', start: layout.ROUND_CONTINUATION_HOOK_ROM, size: 8,
        label: 'custom neutral native round-gate hook' });
      regions.push({ kind: 'rom', start: layout.MENU_ITERATOR_HOOK_ROM, size: 8,
        label: 'battle command-menu stream iterator hook' });
      if (layout.LEGACY_ROUND_CONTINUATION_HOOK_ROM) {
        regions.push({ kind: 'rom', start: layout.LEGACY_ROUND_CONTINUATION_HOOK_ROM,
          size: 8, label: 'superseded custom neutral terminal-state round hook upgrade' });
      }
      regions.push({ kind: 'rom', start: layout.MESSAGE_HOOK_ROM, size: 8,
        label: 'custom neutral encounter-card hook' });
      regions.push({ kind: 'rom', start: layout.MESSAGE_TEXT_HOOK_ROM, size: 8,
        label: 'custom neutral encounter-card text-pointer hook' });
      regions.push({ kind: 'rom', start: layout.PERSUASION_HOOK_ROM, size: 8,
        label: 'custom neutral persuasion hook' });
      regions.push({ kind: 'rom', start: layout.PERSUASION_RECRUIT_TARGET_HOOK_ROM,
        size: 8, label: 'custom neutral persuasion recruitment target hook' });
      for (var targetRegionIndex = 0;
        targetRegionIndex < layout.PERSUASION_TARGET_HOOK_ROMS.length;
        targetRegionIndex++) {
        regions.push({ kind: 'rom',
          start: layout.PERSUASION_TARGET_HOOK_ROMS[targetRegionIndex], size: 8,
          label: 'custom neutral persuasion outcome target hook' });
      }
      regions.push({ kind: 'rom', start: layout.CLEANUP_HOOK_ROM, size: 8,
        label: 'custom neutral cleanup hook' });
      if (layout.LEGACY_CLEANUP_HOOK_ROM) {
        regions.push({ kind: 'rom', start: layout.LEGACY_CLEANUP_HOOK_ROM, size: 8,
          label: 'premature custom neutral cleanup hook upgrade' });
      }
      regions.push({ kind: 'rom', start: layout.RETREAT_HOOK_ROM, size: 8,
        label: 'custom neutral retreat hook' });
      regions.push({ kind: 'rom', start: layout.FIGHT_BUDGET_HOOK_ROM, size: 8,
        label: 'custom neutral Fight-cycle budget hook' });
      regions.push({ kind: 'rom', start: layout.TALK_BUDGET_HOOK_ROM, size: 8,
        label: 'custom neutral Talk-cycle budget hook' });
      regions.push({ kind: 'rom', start: layout.FIGHT_RESULT_BUDGET_HOOK_ROM,
        size: 8, label: 'custom neutral Fight-result budget hook' });
      regions.push({ kind: 'rom', start: layout.REWARD_HOOK_ROM, size: 8,
        label: 'custom neutral victory-reward hook' });
      regions.push({ kind: 'rom', start: PRE_ELIGIBILITY_PERSUASION_HOOK_ROM, size: 8,
        label: 'superseded pre-eligibility persuasion hook upgrade' });
      var z64 = romOrLayout && romOrLayout.z64;
      var legacyMessageWrite = z64
        ? buildBanditsMessageArchiveWrite(z64, false) : null;
      if (legacyMessageWrite && OB64.findArchives) {
        var meswin = OB64.findArchives(z64)[815];
        if (meswin) regions.push({ kind: 'rom', start: meswin.offset,
          size: meswin.totalHeaderSize + meswin.compSize,
          label: 'MESWIN archive #815 legacy Bandits slot restoration' });
      }
    }
    return regions;
  }

  function parseShopOverrides(z64, romOrLayout) {
    var layout = runtimeLayout(romOrLayout);
    var hook = buildShopHook(layout);
    if (!regionEquals(z64, layout.SHOP_HOOK_ROM, hook)) return {};
    var sentinel = readU32(z64, layout.TAIL_Z64);
    if (sentinel !== SHARED_SENTINEL && sentinel !== LEGACY_NEUTRAL_SENTINEL &&
        sentinel !== PROTOTYPE_TYPED_SENTINEL &&
        sentinel !== PREVIOUS_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM6_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM7_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM8_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM9_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBMA_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== NEUTRAL_SHARED_SENTINEL) return {};
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

  function parseSliceRateOverrides(z64, romOrLayout) {
    var layout = neutralRuntimeLayout(romOrLayout);
    if (rateHookState(z64, layout) !== 'shared') return [];
    var sentinel = readU32(z64, layout.TAIL_Z64);
    if (sentinel !== LEGACY_NEUTRAL_SENTINEL &&
        sentinel !== PROTOTYPE_TYPED_SENTINEL) return [];
    var base = layout.TAIL_Z64 + RATE_TABLE_OFF;
    var legacySliceCount = 40;
    if (readU32(z64, base) !== RATE_MAGIC || readU16(z64, base + 4) !== legacySliceCount ||
        readU16(z64, base + 6) !== RATE_ENTRY_STRIDE) return [];
    var out = [];
    function branchAt(off) {
      var mode = z64[off];
      if (mode === RATE_MODE_INHERIT) return { mode: 'inherit', passCount: null, divisor: null };
      if (mode === RATE_MODE_DISABLED) return { mode: 'disabled', passCount: null, divisor: null };
      if (mode !== RATE_MODE_OVERRIDE) return null;
      var threshold = readU16(z64, off + 2);
      var divisor = readU32(z64, off + 4);
      if (!divisor || threshold + 1 > Math.min(0x10000, divisor)) return null;
      return { mode: 'override', passCount: threshold + 1, divisor: divisor };
    }
    for (var slice = 1; slice <= legacySliceCount; slice++) {
      var off = base + 8 + (slice - 1) * RATE_ENTRY_STRIDE;
      var normal = branchAt(off);
      var alternate = branchAt(off + RATE_BRANCH_STRIDE);
      if (!normal || !alternate) continue;
      if (normal.mode !== 'inherit' || alternate.mode !== 'inherit') {
        out.push({ slice: slice, normal: normal, alternate: alternate });
      }
    }
    return out;
  }

  function parseScenarioRateOverrides(z64, romOrLayout) {
    var layout = neutralRuntimeLayout(romOrLayout);
    if (rateHookState(z64, layout) !== 'shared') return [];
    var sentinel = readU32(z64, layout.TAIL_Z64);
    if (sentinel !== PREVIOUS_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM6_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM7_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM8_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM9_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBMA_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== NEUTRAL_SHARED_SENTINEL) return [];
    var base = layout.TAIL_Z64 + RATE_TABLE_OFF;
    if (readU32(z64, base) !== RATE_MAGIC ||
        readU16(z64, base + 4) !== RATE_SCENARIO_COUNT ||
        readU16(z64, base + 6) !== RATE_ENTRY_STRIDE) return [];
    function branchAt(off) {
      var mode = z64[off];
      if (mode === RATE_MODE_INHERIT) return { mode: 'inherit', passCount: null, divisor: null };
      if (mode === RATE_MODE_DISABLED) return { mode: 'disabled', passCount: null, divisor: null };
      if (mode !== RATE_MODE_OVERRIDE) return null;
      var threshold = readU16(z64, off + 2);
      var divisor = readU32(z64, off + 4);
      if (!divisor || threshold + 1 > Math.min(0x10000, divisor)) return null;
      return { mode: 'override', passCount: threshold + 1, divisor: divisor };
    }
    var out = [];
    for (var runtimeKey = 1; runtimeKey < RATE_SCENARIO_COUNT; runtimeKey++) {
      var off = base + 8 + runtimeKey * RATE_ENTRY_STRIDE;
      var normal = branchAt(off);
      var alternate = branchAt(off + RATE_BRANCH_STRIDE);
      if (!normal || !alternate) continue;
      if (normal.mode !== 'inherit' || alternate.mode !== 'inherit') {
        out.push({ runtimeKey: runtimeKey, normal: normal, alternate: alternate });
      }
    }
    return out;
  }

  function parseWeightedDropOverrides(z64, romOrLayout) {
    var layout = neutralRuntimeLayout(romOrLayout);
    if (dropHookState(z64, layout) !== 'shared') return [];
    var sentinel = readU32(z64, layout.TAIL_Z64);
    if (sentinel !== LEGACY_NEUTRAL_SENTINEL &&
        sentinel !== PROTOTYPE_TYPED_SENTINEL &&
        sentinel !== PREVIOUS_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM6_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM7_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM8_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM9_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBMA_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== NEUTRAL_SHARED_SENTINEL) return [];
    var base = layout.TAIL_Z64 +
      (sentinel === PREVIOUS_NEUTRAL_SHARED_SENTINEL ||
        sentinel === OBM6_NEUTRAL_SHARED_SENTINEL ||
        sentinel === OBM7_NEUTRAL_SHARED_SENTINEL ||
        sentinel === OBM8_NEUTRAL_SHARED_SENTINEL ||
        sentinel === OBM9_NEUTRAL_SHARED_SENTINEL ||
        sentinel === OBMA_NEUTRAL_SHARED_SENTINEL ||
        sentinel === NEUTRAL_SHARED_SENTINEL ? DROP_TABLE_OFF : 0xD600);
    if (readU32(z64, base) !== DROP_MAGIC || readU16(z64, base + 4) !== DROP_CLASS_COUNT ||
        readU16(z64, base + 6) !== DROP_ENTRY_STRIDE) return [];
    var out = [];
    for (var classId = 1; classId < DROP_CLASS_COUNT; classId++) {
      var off = base + 8 + classId * DROP_ENTRY_STRIDE;
      if (z64[off] !== 1) continue;
      var weights = [readU16(z64, off + 2), readU16(z64, off + 4), readU16(z64, off + 6)];
      if (weights[0] + weights[1] + weights[2] === 0) continue;
      out.push({ classId: classId, weights: weights });
    }
    return out;
  }

  function terrainIdentityFromPointer(pointer) {
    for (var slice = 1; slice <= 40; slice++) {
      for (var terrainSlot = 0; terrainSlot < 10; terrainSlot++) {
        var pointerA = (NEUTRAL_TABLE_DISPATCH_PREBASE + slice * 20 +
          (terrainSlot + 1) * 2) >>> 0;
        if (pointer === pointerA || pointer === (pointerA + 1 >>> 0)) {
          return { slice: slice, terrainSlot: terrainSlot };
        }
      }
    }
    return null;
  }

  function parseCustomNeutralSquads(z64, romOrLayout) {
    var layout = neutralRuntimeLayout(romOrLayout);
    var sentinel = readU32(z64, layout.TAIL_Z64);
    if (sentinel !== PREVIOUS_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM6_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM7_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM8_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBM9_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== OBMA_NEUTRAL_SHARED_SENTINEL &&
        sentinel !== NEUTRAL_SHARED_SENTINEL) return [];
    var base = layout.TAIL_Z64 + TYPED_PROFILE_TABLE_OFF;
    if (readU32(z64, base) !== TYPED_PROFILE_MAGIC) return [];
    var version = z64[base + 4];
    var count = z64[base + 5];
    var stride = z64[base + 6];
    if ((version !== TYPED_PROFILE_VERSION &&
        version !== PREVIOUS_TYPED_PROFILE_VERSION) ||
        count > TYPED_MAX_PROFILES || stride !== TYPED_PROFILE_ENTRY_STRIDE ||
        z64[base + 7] !== TYPED_MEMBER_STRIDE) return [];
    var recordsBytes = 8 + count * stride;
    var profileTableLimit = base + (RATE_RESOLVER_OFF - TYPED_PROFILE_TABLE_OFF);
    if (base + recordsBytes > profileTableLimit || profileTableLimit > z64.length) return [];
    var out = [];
    for (var index = 0; index < count; index++) {
      var off = base + 8 + index * stride;
      var memberCount = z64[off + 10];
      if (memberCount < 2 || memberCount > 5) return [];
      var members = [];
      for (var memberIndex = 0; memberIndex < memberCount; memberIndex++) {
        var memberOff = off + TYPED_MEMBER_TABLE_OFF + memberIndex * TYPED_MEMBER_STRIDE;
        var cohortCode = z64[memberOff + 3];
        if (cohortCode > 2) return [];
        members.push({
          classId: z64[memberOff],
          levelOffsetRaw: z64[memberOff + 1],
          cell: z64[memberOff + 2],
          cohort: cohortCode === 0 ? 'A' : (cohortCode === 1 ? 'B' : 'C')
        });
      }
      var identity = {
        slice: z64[off + 14], terrainSlot: z64[off + 15]
      };
      var equipment = {};
      ['A', 'B', 'C'].forEach(function(cohort, cohortIndex) {
        var equipmentOff = off + TYPED_EQUIPMENT_TABLE_OFF + cohortIndex * 4;
        equipment[cohort] = [readU16(z64, equipmentOff), readU16(z64, equipmentOff + 2)];
      });
      var classBonuses = [];
      for (var bonusIndex = 0; bonusIndex < TYPED_CLASS_BONUS_COUNT; bonusIndex++) {
        var bonusOff = off + TYPED_CLASS_BONUS_TABLE_OFF + bonusIndex * 2;
        if (z64[bonusOff] === 0) continue;
        classBonuses.push({ classId: z64[bonusOff], bonus: z64[bonusOff + 1] });
      }
      if (z64[off + 54] !== TYPED_REWARD_COUNT) continue;
      var rewardSlots = [];
      for (var rewardIndex = 0; rewardIndex < TYPED_REWARD_COUNT; rewardIndex++) {
        var rewardOff = off + TYPED_REWARD_TABLE_OFF + rewardIndex * 4;
        rewardSlots.push({
          raw: readU16(z64, rewardOff),
          weight: readU16(z64, rewardOff + 2)
        });
      }
      var encounterText = 'Bandits!';
      if (version === TYPED_PROFILE_VERSION) {
        var messagePointer = readU32(z64, off + TYPED_MESSAGE_POINTER_OFF);
        var messageRelative = messagePointer -
          ((layout.MOD_BASE + TYPED_PROFILE_TABLE_OFF) >>> 0);
        if (!Number.isInteger(messageRelative) || messageRelative < recordsBytes ||
            messageRelative >= RATE_RESOLVER_OFF - TYPED_PROFILE_TABLE_OFF) continue;
        var messageStart = base + messageRelative;
        var messageEnd = messageStart;
        var messageMax = Math.min(profileTableLimit,
          messageStart + TYPED_MESSAGE_MAX_CHARS + 32);
        while (messageEnd < messageMax && z64[messageEnd] !== 0) messageEnd++;
        if (messageEnd >= messageMax || z64[messageEnd] !== 0) continue;
        encounterText = decodeEncounterText(
          z64.subarray(messageStart, messageEnd + 1));
        if (encounterText == null) continue;
      }
      try {
        var normalized = normalizeCustomProfile({
          profileId: z64[off + 8],
          runtimeKey: z64[off + 9],
          slice: identity.slice,
          terrainSlot: identity.terrainSlot,
          members: members,
          equipment: equipment,
          persuasion: {
            mode: 'fixed',
            chance: z64[off + 12],
            classBonuses: classBonuses
          },
          retreat: { hpThreshold: z64[off + 13] },
          label: encounterText,
          rewards: { slots: rewardSlots }
        }, index);
      } catch (e) {
        continue;
      }
      out.push({
        profileId: normalized.profileId,
        runtimeKey: normalized.runtimeKey,
        slice: normalized.slice,
        terrainSlot: normalized.terrainSlot,
        members: normalized.members,
        equipment: normalized.equipment,
        label: normalized.label,
        persuasion: normalized.persuasion,
        retreat: normalized.retreat,
        rewards: normalized.rewards
      });
    }
    return out;
  }

  function applyParsedNeutralOverrides(rom) {
    if (!rom || !rom.z64) return {
      sliceRates: 0, scenarioRates: 0, weightedDrops: 0, customSquads: 0
    };
    var rateOverrides = parseSliceRateOverrides(rom.z64, rom);
    var scenarioRows = rom.neutralEncounters && rom.neutralEncounters.scenarioRates || [];
    var i;
    var scenarioRateOverrides = parseScenarioRateOverrides(rom.z64, rom);
    var scenarioByKey = {};
    for (i = 0; i < scenarioRows.length; i++) {
      scenarioByKey[scenarioRows[i].runtimeKey] = scenarioRows[i];
    }
    for (i = 0; i < scenarioRateOverrides.length; i++) {
      var scenarioRow = scenarioByKey[scenarioRateOverrides[i].runtimeKey];
      if (!scenarioRow) continue;
      scenarioRow.rateOverride = {
        normal: Object.assign({}, scenarioRateOverrides[i].normal),
        alternate: Object.assign({}, scenarioRateOverrides[i].alternate)
      };
    }
    var customSquads = parseCustomNeutralSquads(rom.z64, rom);
    if (rom.neutralEncounters) rom.neutralEncounters.customSquads = customSquads;
    var dropOverrides = parseWeightedDropOverrides(rom.z64, rom);
    var drops = rom.creatureDrops && rom.creatureDrops.byClass || {};
    var appliedDrops = 0;
    for (i = 0; i < dropOverrides.length; i++) {
      var rec = drops[dropOverrides[i].classId];
      if (!rec || !rec.slots || rec.slots.length < 3) continue;
      for (var slot = 0; slot < 3; slot++) rec.slots[slot].weight = dropOverrides[i].weights[slot];
      rec.weightedRuntimeOverride = true;
      appliedDrops++;
    }
    rom.neutralRuntimeOverridesDetected = {
      sliceRates: rateOverrides.length,
      scenarioRates: scenarioRateOverrides.length,
      weightedDrops: appliedDrops,
      customSquads: customSquads.length
    };
    return rom.neutralRuntimeOverridesDetected;
  }

  function collectSliceRateOverrides(rom) {
    var out = [];
    var rows = rom && rom.neutralEncounters && rom.neutralEncounters.records || [];
    function copyBranch(branch) {
      branch = branch || {};
      return {
        mode: branch.mode || 'inherit',
        passCount: branch.passCount == null ? null : Number(branch.passCount),
        divisor: branch.divisor == null ? null : Number(branch.divisor)
      };
    }
    for (var i = 0; i < rows.length; i++) {
      var rate = rows[i].rateOverride || {};
      var normal = copyBranch(rate.normal);
      var alternate = copyBranch(rate.alternate);
      if (normal.mode === 'inherit' && alternate.mode === 'inherit') continue;
      out.push({ slice: rows[i].s0, normal: normal, alternate: alternate });
    }
    return out;
  }

  function collectScenarioRateOverrides(rom) {
    var out = [];
    var encounters = rom && rom.neutralEncounters;
    var scenarioRows = encounters && encounters.scenarioRates || [];
    function copyBranch(branch) {
      branch = branch || {};
      return {
        mode: branch.mode || 'inherit',
        passCount: branch.passCount == null ? null : Number(branch.passCount),
        divisor: branch.divisor == null ? null : Number(branch.divisor)
      };
    }
    for (var i = 0; i < scenarioRows.length; i++) {
      var row = scenarioRows[i];
      if (!row || !Number.isInteger(row.slice)) continue;
      var scenarioRate = row.rateOverride || {};
      var normal = copyBranch(scenarioRate.normal);
      var alternate = copyBranch(scenarioRate.alternate);
      if (normal.mode === 'inherit' && alternate.mode === 'inherit') continue;
      out.push({ runtimeKey: row.runtimeKey, normal: normal, alternate: alternate });
    }
    return out;
  }

  function collectCustomNeutralSquads(rom) {
    var source = rom && rom.neutralEncounters && rom.neutralEncounters.customSquads || [];
    var out = [];
    for (var i = 0; i < source.length; i++) {
      var normalized = normalizeCustomProfile(source[i], i);
      out.push({
        profileId: normalized.profileId,
        runtimeKey: normalized.runtimeKey,
        slice: normalized.slice,
        terrainSlot: normalized.terrainSlot,
        members: normalized.members,
        equipment: normalized.equipment,
        label: normalized.label,
        persuasion: normalized.persuasion,
        retreat: normalized.retreat,
        rewards: normalized.rewards
      });
    }
    out.sort(function(a, b) {
      return a.runtimeKey - b.runtimeKey || a.terrainSlot - b.terrainSlot;
    });
    return out;
  }

  function collectWeightedDropOverrides(rom) {
    var out = [];
    var records = rom && rom.creatureDrops && rom.creatureDrops.records || [];
    for (var i = 0; i < records.length; i++) {
      var rec = records[i];
      if (!rec || rec.isSentinel || !rec.isRuntimeActive || !rec.classId || !rec.slots) continue;
      var weights = [];
      var changed = false;
      for (var slot = 0; slot < 3; slot++) {
        var weight = Number(rec.slots[slot] && rec.slots[slot].weight);
        if (!Number.isInteger(weight) || weight < 0) weight = 1;
        weights.push(weight);
        if (weight !== 1) changed = true;
      }
      if (changed) out.push({ classId: rec.classId, weights: weights });
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
    buildNeutralSharedBlob: buildNeutralSharedBlob,
    buildShopTable: buildShopTable,
    buildShopResolver: buildShopResolver,
    buildRateTable: buildRateTable,
    buildRateResolver: buildRateResolver,
    buildTypedProfileTable: buildTypedProfileTable,
    buildSelectionResolver: buildSelectionResolver,
    buildMaterializerResolver: buildMaterializerResolver,
    buildRoundContinuationResolver: buildRoundContinuationResolver,
    buildMenuIteratorResolver: buildMenuIteratorResolver,
    buildMessageResolver: buildMessageResolver,
    buildPersuasionResolver: buildPersuasionResolver,
    buildPersuasionTargetResolver: buildPersuasionTargetResolver,
    buildCleanupResolver: buildCleanupResolver,
    buildRetreatResolver: buildRetreatResolver,
    buildCycleBudgetResolver: buildCycleBudgetResolver,
    buildRewardResolver: buildRewardResolver,
    buildBanditsMessageArchiveWrite: buildBanditsMessageArchiveWrite,
    normalizeEncounterText: normalizeEncounterText,
    encodeEncounterText: encodeEncounterText,
    normalizeCustomProfile: normalizeCustomProfile,
    buildDropTable: buildDropTable,
    buildDropResolver: buildDropResolver,
    buildSharedBootstrap: buildSharedBootstrap,
    buildNeutralSharedBootstrap: buildNeutralSharedBootstrap,
    buildSharedContinuation: buildSharedContinuation,
    buildShopHook: buildShopHook,
    buildRateHook: buildRateHook,
    buildDropHook: buildDropHook,
    parseShopOverrides: parseShopOverrides,
    parseSliceRateOverrides: parseSliceRateOverrides,
    parseScenarioRateOverrides: parseScenarioRateOverrides,
    parseWeightedDropOverrides: parseWeightedDropOverrides,
    parseCustomNeutralSquads: parseCustomNeutralSquads,
    applyParsedShopOverrides: applyParsedShopOverrides,
    applyParsedNeutralOverrides: applyParsedNeutralOverrides,
    refreshShopOverrideState: refreshShopOverrideState,
    collectShopOverrides: collectShopOverrides,
    collectSliceRateOverrides: collectSliceRateOverrides,
    collectScenarioRateOverrides: collectScenarioRateOverrides,
    collectWeightedDropOverrides: collectWeightedDropOverrides,
    collectCustomNeutralSquads: collectCustomNeutralSquads,
    restoreShopHook: restoreShopHook,
    restoreNeutralHooks: restoreNeutralHooks,
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
      LEGACY_NEUTRAL_SENTINEL: LEGACY_NEUTRAL_SENTINEL,
      PROTOTYPE_TYPED_SENTINEL: PROTOTYPE_TYPED_SENTINEL,
      PREVIOUS_NEUTRAL_SHARED_SENTINEL: PREVIOUS_NEUTRAL_SHARED_SENTINEL,
      OBM6_NEUTRAL_SHARED_SENTINEL: OBM6_NEUTRAL_SHARED_SENTINEL,
      OBM7_NEUTRAL_SHARED_SENTINEL: OBM7_NEUTRAL_SHARED_SENTINEL,
      OBM8_NEUTRAL_SHARED_SENTINEL: OBM8_NEUTRAL_SHARED_SENTINEL,
      OBM9_NEUTRAL_SHARED_SENTINEL: OBM9_NEUTRAL_SHARED_SENTINEL,
      OBMA_NEUTRAL_SHARED_SENTINEL: OBMA_NEUTRAL_SHARED_SENTINEL,
      NEUTRAL_SHARED_SENTINEL: NEUTRAL_SHARED_SENTINEL,
      SQUAD_RESOLVER_OFF: SQUAD_RESOLVER_OFF,
      SHOP_ORIGINAL_WORDS: SHOP_ORIGINAL_WORDS.slice(),
      RATE_ORIGINAL_WORDS: RATE_ORIGINAL_WORDS.slice(),
      DROP_ORIGINAL_WORDS: DROP_ORIGINAL_WORDS.slice(),
      RATE_RESOLVER_OFF: RATE_RESOLVER_OFF,
      RATE_TABLE_OFF: RATE_TABLE_OFF,
      RATE_MAGIC: RATE_MAGIC,
      RATE_SCENARIO_COUNT: RATE_SCENARIO_COUNT,
      RATE_ENTRY_STRIDE: RATE_ENTRY_STRIDE,
      DROP_RESOLVER_OFF: DROP_RESOLVER_OFF,
      DROP_TABLE_OFF: DROP_TABLE_OFF,
      DROP_MAGIC: DROP_MAGIC,
      DROP_CLASS_COUNT: DROP_CLASS_COUNT,
      DROP_ENTRY_STRIDE: DROP_ENTRY_STRIDE,
      TYPED_PROFILE_TABLE_OFF: TYPED_PROFILE_TABLE_OFF,
      TYPED_SELECTION_RESOLVER_OFF: TYPED_SELECTION_RESOLVER_OFF,
      TYPED_MATERIALIZER_RESOLVER_OFF: TYPED_MATERIALIZER_RESOLVER_OFF,
      TYPED_ROUND_CONTINUATION_RESOLVER_OFF:
        TYPED_ROUND_CONTINUATION_RESOLVER_OFF,
      TYPED_MENU_ITERATOR_RESOLVER_OFF: TYPED_MENU_ITERATOR_RESOLVER_OFF,
      TYPED_MESSAGE_RESOLVER_OFF: TYPED_MESSAGE_RESOLVER_OFF,
      TYPED_PERSUASION_RESOLVER_OFF: TYPED_PERSUASION_RESOLVER_OFF,
      TYPED_PERSUASION_TARGET_RESOLVER_OFF:
        TYPED_PERSUASION_TARGET_RESOLVER_OFF,
      TYPED_CLEANUP_RESOLVER_OFF: TYPED_CLEANUP_RESOLVER_OFF,
      TYPED_RETREAT_RESOLVER_OFF: TYPED_RETREAT_RESOLVER_OFF,
      TYPED_CYCLE_BUDGET_RESOLVER_OFF: TYPED_CYCLE_BUDGET_RESOLVER_OFF,
      TYPED_REWARD_RESOLVER_OFF: TYPED_REWARD_RESOLVER_OFF,
      REWARD_CONTINUATION_FROM_RA: REWARD_CONTINUATION_FROM_RA,
      TYPED_PROFILE_MAGIC: TYPED_PROFILE_MAGIC,
      PREVIOUS_TYPED_PROFILE_VERSION: PREVIOUS_TYPED_PROFILE_VERSION,
      TYPED_PROFILE_VERSION: TYPED_PROFILE_VERSION,
      TYPED_PROFILE_ENTRY_STRIDE: TYPED_PROFILE_ENTRY_STRIDE,
      TYPED_MESSAGE_POINTER_OFF: TYPED_MESSAGE_POINTER_OFF,
      TYPED_MESSAGE_MAX_CHARS: TYPED_MESSAGE_MAX_CHARS,
      TYPED_MAX_PROFILES: TYPED_MAX_PROFILES,
      TYPED_DEFAULT_PERSUASION_CHANCE: TYPED_DEFAULT_PERSUASION_CHANCE,
      TYPED_DEFAULT_RETREAT_HP_THRESHOLD: TYPED_DEFAULT_RETREAT_HP_THRESHOLD,
      TYPED_MENU_ITERATOR_DISPATCH_ID: TYPED_MENU_ITERATOR_DISPATCH_ID,
      TYPED_FIGHT_BUDGET_SELECTOR: TYPED_FIGHT_BUDGET_SELECTOR,
      TYPED_TALK_BUDGET_SELECTOR: TYPED_TALK_BUDGET_SELECTOR,
      TYPED_SCRATCH_UNCACHED: TYPED_SCRATCH_UNCACHED,
      TYPED_PROFILE_TOKEN: TYPED_PROFILE_TOKEN,
      BANDITS_MESSAGE_ENTRY: BANDITS_MESSAGE_ENTRY,
      BANDITS_MESSAGE_BYTES: Array.prototype.slice.call(BANDITS_MESSAGE_BYTES),
      SELECTION_ORIGINAL_WORDS: SELECTION_ORIGINAL_WORDS.slice(),
      MATERIALIZER_ORIGINAL_WORDS: MATERIALIZER_ORIGINAL_WORDS.slice(),
      MESSAGE_ORIGINAL_WORDS: MESSAGE_ORIGINAL_WORDS.slice(),
      MESSAGE_TEXT_ORIGINAL_WORDS: MESSAGE_TEXT_ORIGINAL_WORDS.slice(),
      PERSUASION_ORIGINAL_WORDS: PERSUASION_ORIGINAL_WORDS.slice(),
      PERSUASION_TARGET_ORIGINAL_WORDS: PERSUASION_TARGET_ORIGINAL_WORDS.slice(),
      PERSUASION_RECRUIT_TARGET_ORIGINAL_WORDS:
        PERSUASION_RECRUIT_TARGET_ORIGINAL_WORDS.slice(),
      PRE_ELIGIBILITY_PERSUASION_HOOK_ROM: PRE_ELIGIBILITY_PERSUASION_HOOK_ROM,
      PRE_ELIGIBILITY_PERSUASION_ORIGINAL_WORDS:
        PRE_ELIGIBILITY_PERSUASION_ORIGINAL_WORDS.slice(),
      LEGACY_CLEANUP_ORIGINAL_WORDS: LEGACY_CLEANUP_ORIGINAL_WORDS.slice(),
      CLEANUP_ORIGINAL_WORDS: CLEANUP_ORIGINAL_WORDS.slice(),
      RETREAT_ORIGINAL_WORDS: RETREAT_ORIGINAL_WORDS.slice(),
      FIGHT_BUDGET_ORIGINAL_WORDS: FIGHT_BUDGET_ORIGINAL_WORDS.slice(),
      TALK_BUDGET_ORIGINAL_WORDS: TALK_BUDGET_ORIGINAL_WORDS.slice(),
      MENU_ITERATOR_ORIGINAL_WORDS: MENU_ITERATOR_ORIGINAL_WORDS.slice(),
      LEGACY_ROUND_CONTINUATION_ORIGINAL_WORDS:
        LEGACY_ROUND_CONTINUATION_ORIGINAL_WORDS.slice(),
      ROUND_CONTINUATION_ORIGINAL_WORDS:
        ROUND_CONTINUATION_ORIGINAL_WORDS.slice(),
      REWARD_ORIGINAL_WORDS: REWARD_ORIGINAL_WORDS.slice()
    }
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = OB64;
})(typeof OB64 !== 'undefined' ? OB64 :
  (typeof window !== 'undefined' ? (window.OB64 = window.OB64 || {}) : (this.OB64 = this.OB64 || {})));
