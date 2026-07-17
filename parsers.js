// OB64 Mod Editor — ROM Loader & Binary Parsers
// Ported from Node.js scripts to browser (Uint8Array/DataView, no Buffer)

window.OB64 = window.OB64 || {};

// ============================================================
// Byte-swap: v64 <-> z64
// ============================================================
OB64.v64ToZ64 = function(v64) {
  var z64 = new Uint8Array(v64.length);
  for (var i = 0; i < v64.length - 1; i += 2) {
    z64[i] = v64[i + 1];
    z64[i + 1] = v64[i];
  }
  return z64;
};

OB64.z64ToV64 = function(z64) {
  return OB64.v64ToZ64(z64); // Same swap
};

OB64.n64ToZ64 = function(n64) {
  var z64 = new Uint8Array(n64.length);
  for (var i = 0; i < n64.length - 3; i += 4) {
    z64[i] = n64[i + 3];
    z64[i + 1] = n64[i + 2];
    z64[i + 2] = n64[i + 1];
    z64[i + 3] = n64[i];
  }
  return z64;
};

OB64.z64ToN64 = function(z64) {
  return OB64.n64ToZ64(z64); // Same word swap
};

OB64.detectRomByteOrder = function(bytes) {
  if (!bytes || bytes.length < 4) return 'unknown';
  var b0 = bytes[0], b1 = bytes[1], b2 = bytes[2], b3 = bytes[3];
  if (b0 === 0x37 && b1 === 0x80 && b2 === 0x40 && b3 === 0x12) return 'v64';
  if (b0 === 0x80 && b1 === 0x37 && b2 === 0x12 && b3 === 0x40) return 'z64';
  if (b0 === 0x40 && b1 === 0x12 && b2 === 0x37 && b3 === 0x80) return 'n64';
  return 'unknown';
};

// ============================================================
// DataView helpers (big-endian reads on Uint8Array)
// ============================================================
OB64.readU16BE = function(buf, off) {
  return (buf[off] << 8) | buf[off + 1];
};

OB64.readU32BE = function(buf, off) {
  return ((buf[off] << 24) | (buf[off + 1] << 16) | (buf[off + 2] << 8) | buf[off + 3]) >>> 0;
};

OB64.readU16LE = function(buf, off) {
  return buf[off] | (buf[off + 1] << 8);
};

OB64.readU32LE = function(buf, off) {
  return (buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16) | (buf[off + 3] << 24)) >>> 0;
};

OB64.writeU16BE = function(buf, off, val) {
  buf[off] = (val >>> 8) & 0xFF;
  buf[off + 1] = val & 0xFF;
};

OB64.writeU32BE = function(buf, off, val) {
  val >>>= 0;
  buf[off] = (val >>> 24) & 0xFF;
  buf[off + 1] = (val >>> 16) & 0xFF;
  buf[off + 2] = (val >>> 8) & 0xFF;
  buf[off + 3] = val & 0xFF;
};

// ============================================================
// ROM byte-order normalization + revision-specific layout profiles
// ============================================================
OB64.normalizeRomImage = function(data) {
  var input = new Uint8Array(data);
  var byteOrder = OB64.detectRomByteOrder(input);
  if (byteOrder === 'v64') return { z64: OB64.v64ToZ64(input), byteOrder: byteOrder };
  if (byteOrder === 'z64') return { z64: new Uint8Array(input), byteOrder: byteOrder };
  if (byteOrder === 'n64') return { z64: OB64.n64ToZ64(input), byteOrder: byteOrder };
  return { z64: null, byteOrder: byteOrder };
};

OB64.serializeRomImage = function(z64, byteOrder) {
  if (byteOrder === 'v64') return OB64.z64ToV64(z64);
  if (byteOrder === 'n64') return OB64.z64ToN64(z64);
  return new Uint8Array(z64);
};

OB64.romByteOrderExtension = function(byteOrder) {
  if (byteOrder === 'v64') return 'v64';
  if (byteOrder === 'n64') return 'n64';
  return 'z64';
};

OB64.ROM_LAYOUTS = {
  'us-rev0': {
    id: 'us-rev0',
    name: 'US retail header rev 0',
    version: 0x00,
    gameId: 'NO',
    country: 'E',
    vanillaCrc1: 0xE6419BC5,
    vanillaCrc2: 0x69011DE3,
    supportsTools: true,
    supportsSquadOverrides: true,
    supportsShopOverrides: true,
    unsupportedTools: {},
    unsupportedFeaturesReason: '',
    squadPatch: {
      HOOK_ROM: 0x195584
    },
    shopPatch: {
      HOOK_ROM: 0x19BF18,
      CLEANUP_ROM: 0x19BFC4
    },
    offsets: {
      LZSS_GAP_START: 0x20248C2,
      STAT_GATE_GAP_OFFSET: 0x3A960C,
      NEUTRAL_ENCOUNTER_OFFSET: 0x141ED0,
      NEUTRAL_TERRAIN_RATE_OFFSET: 0x141E80,
      NEUTRAL_TERRAIN_SLOT_OFFSET: 0x141EA0,
      NEUTRAL_GLOBAL_DIV_HI_OFFSET: 0x13C1E8,
      NEUTRAL_GLOBAL_DIV_LO_OFFSET: 0x13C1EC,
      NEUTRAL_GLOBAL_NORMAL_OFFSET: 0x13C1FC,
      NEUTRAL_GLOBAL_ALT_OFFSET: 0x13C200,
      NEUTRAL_GLOBAL_BRANCH_OFFSET: 0x13C228,
      CREATURE_DROP_OFFSET: 0x142258,
      ITEM_STAT_OFFSET: 0x62310,
      MAP_EDGE_OFFSET: 0x858E4,
      MAP_NAME_OFFSET: 0x85984,
      GROWTH_TIER_OFFSET: 0x17F7F0,
      GROWTH_CURVE_OFFSET: 0x17F668,
      EVOLUTION_OFFSET: 0x654A0,
      CLASS_GROUP_OFFSET: 0x6592C,
      CLASS_DEF_OFFSET: 0x5DAD8,
      CONSUMABLE_TABLE_OFFSET: 0x645CC
    }
  },
  'us-rev1': {
    id: 'us-rev1',
    name: 'US retail header rev 1',
    version: 0x01,
    gameId: 'NO',
    country: 'E',
    vanillaCrc1: 0x0ADAECA7,
    vanillaCrc2: 0xB17F9795,
    supportsTools: true,
    supportsSquadOverrides: true,
    supportsShopOverrides: true,
    unsupportedTools: {
      'high-attack-streamsplit': 'Header revision 1 changed the high-attack battle-stream owner/global references; regenerate and Project64-verify a header revision 1 payload before enabling.'
    },
    unsupportedFeaturesReason: 'Header revision 1 supports normal data edits, Chaos Frame Counter, and shared Shops+Squads runtime overrides. High Attack Streamsplit remains disabled until its changed header revision 1 code path is rebuilt.',
    squadPatch: {
      HOOK_ROM: 0x1955A4
    },
    shopPatch: {
      // Same side-loaded overlay routine as rev 0, shifted by the accepted
      // header-revision +0x20 ROM delta. The hook branches by the relative
      // distance so both instructions follow the overlay's live relocation.
      HOOK_ROM: 0x19BF38,
      CLEANUP_ROM: 0x19BFE4
    },
    offsets: {
      LZSS_GAP_START: 0x2024516,
      STAT_GATE_GAP_OFFSET: 0x3A960C,
      NEUTRAL_ENCOUNTER_OFFSET: 0x141EF0,
      NEUTRAL_TERRAIN_RATE_OFFSET: 0x141EA0,
      NEUTRAL_TERRAIN_SLOT_OFFSET: 0x141EC0,
      NEUTRAL_GLOBAL_DIV_HI_OFFSET: 0x13C208,
      NEUTRAL_GLOBAL_DIV_LO_OFFSET: 0x13C20C,
      NEUTRAL_GLOBAL_NORMAL_OFFSET: 0x13C21C,
      NEUTRAL_GLOBAL_ALT_OFFSET: 0x13C220,
      NEUTRAL_GLOBAL_BRANCH_OFFSET: 0x13C248,
      CREATURE_DROP_OFFSET: 0x142278,
      ITEM_STAT_OFFSET: 0x62330,
      MAP_EDGE_OFFSET: 0x85904,
      MAP_NAME_OFFSET: 0x859A4,
      GROWTH_TIER_OFFSET: 0x17F810,
      GROWTH_CURVE_OFFSET: 0x17F688,
      EVOLUTION_OFFSET: 0x654C0,
      CLASS_GROUP_OFFSET: 0x6594C,
      CLASS_DEF_OFFSET: 0x5DAF8,
      CONSUMABLE_TABLE_OFFSET: 0x645EC
    }
  }
};

OB64.cloneRomLayout = function(layout) {
  var out = {};
  for (var k in layout) {
    if (k === 'offsets') continue;
    out[k] = layout[k];
  }
  out.offsets = {};
  for (var ok in layout.offsets) out.offsets[ok] = layout.offsets[ok];
  return out;
};

OB64.romHeaderText = function(z64, off, len) {
  var chars = [];
  for (var i = 0; i < len && off + i < z64.length; i++) {
    var b = z64[off + i];
    if (b === 0) break;
    chars.push(String.fromCharCode(b));
  }
  return chars.join('');
};

OB64.detectRomLayout = function(z64) {
  if (!z64 || z64.length < 0x40) return null;
  var imageName = OB64.romHeaderText(z64, 0x20, 20).trim();
  var gameId = OB64.romHeaderText(z64, 0x3B, 2);
  var country = OB64.romHeaderText(z64, 0x3E, 1);
  var version = z64[0x3F];
  if (imageName !== 'OgreBattle64' || gameId !== 'NO' || country !== 'E') {
    return null;
  }
  var layout = version === 0x00 ? OB64.ROM_LAYOUTS['us-rev0']
    : version === 0x01 ? OB64.ROM_LAYOUTS['us-rev1']
    : null;
  if (!layout) return null;
  var out = OB64.cloneRomLayout(layout);
  out.imageName = imageName;
  out.headerCrc1 = OB64.readU32BE(z64, 0x10);
  out.headerCrc2 = OB64.readU32BE(z64, 0x14);
  return out;
};

OB64.applyRomLayout = function(layout) {
  if (!layout || !layout.offsets) throw new Error('Missing ROM layout profile');
  for (var k in layout.offsets) OB64[k] = layout.offsets[k];
  OB64.currentRomLayout = layout;
};

// ============================================================
// LZSS decoder for the 7 MB LZSS region (game's ROM-side LZSS format
// at decompressor ROM 0xA510). Ported from scripts/ob64_lzss_compress.js.
// Returns {output: Uint8Array, bytesConsumed: number}.
// ============================================================
OB64.lzssDecode = function(data, startOff, maxDecompSize) {
  var output = new Uint8Array(maxDecompSize + 4096);
  var outPos = 0, ip = startOff;
  var maxIp = Math.min(data.length, startOff + maxDecompSize * 4);
  while (outPos < maxDecompSize && ip < maxIp) {
    var b = data[ip];
    if (b >= 0x80) {
      var b2 = data[ip + 1] || 0;
      var len = ((b >> 3) & 0xF) + 3;
      var dist = ((b & 7) << 8 | b2) + 1;
      var srcPos = outPos - dist;
      for (var j = 0; j < len && outPos < maxDecompSize; j++) {
        output[outPos] = srcPos + j >= 0 ? output[srcPos + j] : 0; outPos++;
      }
      ip += 2;
    } else if (b >= 0x40) {
      var count = (b & 0x3F) + 1;
      for (var j2 = 0; j2 < count && outPos < maxDecompSize; j2++) output[outPos++] = data[ip + 1 + j2] || 0;
      ip += 1 + count;
    } else if (b >= 0x20) {
      var count3 = (b & 0x1F) + 2;
      for (var j3 = 0; j3 < count3 && outPos < maxDecompSize; j3++) output[outPos++] = 0;
      ip += 1;
    } else if (b >= 0x10) {
      var b2b = data[ip + 1] || 0, b3b = data[ip + 2] || 0;
      var dist2 = ((b2b & 0x3F) << 8 | b3b) + 1;
      var len2 = ((b & 0xF) | ((b2b & 0xC0) >> 2)) + 4;
      var srcPos2 = outPos - dist2;
      for (var j4 = 0; j4 < len2 && outPos < maxDecompSize; j4++) {
        output[outPos] = srcPos2 + j4 >= 0 ? output[srcPos2 + j4] : 0; outPos++;
      }
      ip += 3;
    } else if (b === 0x00) {
      var b2c = data[ip + 1] || 0, b3c = data[ip + 2] || 0, b4c = data[ip + 3] || 0;
      var len3 = b2c + 5, dist3 = ((b3c << 8) | b4c) + 1;
      var srcPos3 = outPos - dist3;
      for (var j5 = 0; j5 < len3 && outPos < maxDecompSize; j5++) {
        output[outPos] = srcPos3 + j5 >= 0 ? output[srcPos3 + j5] : 0; outPos++;
      }
      ip += 4;
    } else if (b === 0x01) {
      var count4 = (data[ip + 1] || 0) + 3;
      for (var j6 = 0; j6 < count4 && outPos < maxDecompSize; j6++) output[outPos++] = 0xFF;
      ip += 2;
    } else if (b === 0x02) {
      var count5 = (data[ip + 1] || 0) + 3;
      for (var j7 = 0; j7 < count5 && outPos < maxDecompSize; j7++) output[outPos++] = 0;
      ip += 2;
    } else { ip += 1; } // NOP 0x03-0x0F
  }
  return { output: output.subarray(0, outPos), bytesConsumed: ip - startOff };
};

// ============================================================
// Stat gate promotion threshold table
// Lives in LZSS block at z64 GAP_START + 0x3A960C.
// 81 records × 8 bytes, class_id-indexed. z64 byte order:
//   [STR, VIT, INT, MEN, AGI, DEX, ALN_MIN, ALN_MAX]
// Value 0 = no requirement. ALN range is inclusive.
// Promotion to target class X requires the character's stats to meet
// statGates[X] AND alignment ∈ [ALN_MIN, ALN_MAX].
// Verified by in-game promotion tests across the humanoid classes.
// ============================================================
OB64.LZSS_GAP_START = 0x20248C2;
OB64.STAT_GATE_GAP_OFFSET = 0x3A960C;
OB64.STAT_GATE_STRIDE = 8;
OB64.STAT_GATE_COUNT = 81;

OB64.parseStatGates = function(z64) {
  var absOff = OB64.LZSS_GAP_START + OB64.STAT_GATE_GAP_OFFSET;
  // Block header: payload_size (u32) + decomp_size (u32) + compressed data
  var payloadSize = OB64.readU32BE(z64, absOff);
  var decompSize = OB64.readU32BE(z64, absOff + 4);
  var compDataOff = absOff + 8;
  var compDataSize = payloadSize - 4;

  var decompressed;
  try {
    decompressed = OB64.lzssDecode(z64, compDataOff, decompSize).output;
  } catch (e) {
    console.warn('Stat gate LZSS decompress failed:', e);
    return { byClass: {}, raw: new Uint8Array(0), meta: { payloadSize: 0, decompSize: 0, compDataOff: 0, compDataSize: 0 } };
  }

  // 81 records × 8 bytes. classId index = i (NOT i+1 like class defs).
  var byClass = {};
  var count = Math.min(OB64.STAT_GATE_COUNT, Math.floor(decompressed.length / OB64.STAT_GATE_STRIDE));
  for (var i = 0; i < count; i++) {
    var o = i * OB64.STAT_GATE_STRIDE;
    byClass[i] = {
      classId: i,
      str: decompressed[o + 0],
      vit: decompressed[o + 1],
      int: decompressed[o + 2],
      men: decompressed[o + 3],
      agi: decompressed[o + 4],
      dex: decompressed[o + 5],
      alnMin: decompressed[o + 6],
      alnMax: decompressed[o + 7],
      offset: o
    };
  }
  return {
    byClass: byClass,
    raw: new Uint8Array(decompressed),
    meta: {
      payloadSize: payloadSize,
      decompSize: decompSize,
      compDataOff: compDataOff,
      compDataSize: compDataSize
    }
  };
};

// ============================================================
// Neutral encounter pool — the wild creatures that spawn during tactical-
// map walking. ROM 0x141ED0, 816 B total. Outside CIC-6102 CRC window.
//
// Record structure (verified against the ogrebattle64.net wiki plus a
// byte-level slot-to-terrain cross-check across 19 confident scenes):
//
//   The table is NOT 51 × 16B rows as docs previously suggested. It is
//   40 × 20B SLICES, each slice being one scenario (indexed by dispatcher
//   variable $s0 starting at 1). The first 4 bytes of the table are
//   leading padding; scenario 1 (Tenne Plains) begins at offset 4.
//
//   Per-slice layout: 10 slots × 2 bytes. Each slot holds a creature pair
//   (u8 classA, u8 classB); when equal, spawns single; when different,
//   spawns 50/50 pair; when both 0, terrain is empty.
//
//   Slot → terrain mapping is GLOBALLY CONSISTENT:
//     0 = Plains / Roads
//     1 = Plains
//     2 = Barrens
//     3 = Forests
//     4 = Marshes
//     5 = Highlands
//     6 = Snowy Plains
//     7 = Snowy Barrens
//     8 = Snowy Forests
//     9 = Snowy Highlands
//
//   Slice offset formula: offsetFromTableStart = (s0 - 1) * 20 + 4
//   i.e. scenario $s0 lives at ROM 0x141ED0 + 4 + (s0 - 1) * 20.
//
//   Verified in-ROM matches for 19 scenes (Tenne Plains s0=1, Mylesia I
//   s0=4, Zenobian Border s0=5, Dardunnelles I s0=8, Alba s0=9,
//   Highland of Soathon s0=12, Sable Lowlands s0=13, Azure Plains s0=16,
//   Mt Keryoleth I s0=17, Fair Heights s0=21, Capitrium s0=22, Temple of
//   Berthe s0=26, Gules Hills I/II s0=29, Argent s0=34, Tybell s0=35,
//   Latium s0=36, Aurua Plains I s0=37, Mt Keryoleth II s0=39).
// ============================================================
OB64.NEUTRAL_ENCOUNTER_OFFSET = 0x141ED0;
OB64.NEUTRAL_ENCOUNTER_LEADING_PAD = 4;          // first scenario starts 4 B in
OB64.NEUTRAL_ENCOUNTER_STRIDE = 20;              // 10 slots × 2 B
OB64.NEUTRAL_ENCOUNTER_SLOTS  = 10;
OB64.NEUTRAL_ENCOUNTER_COUNT  = 40;              // fits (816 - 4) / 20 = 40.6
OB64.NEUTRAL_TERRAIN_RATE_OFFSET = 0x141E80;     // runtime 0x801ED740
OB64.NEUTRAL_TERRAIN_SLOT_OFFSET = 0x141EA0;     // runtime 0x801ED760
OB64.NEUTRAL_TERRAIN_TABLE_LEN   = 0x20;

// Global/base neutral roll in the walk-step dispatcher. This is the first
// probability gate, before the per-unit terrain-rate roll.
OB64.NEUTRAL_GLOBAL_DIV_HI_OFFSET = 0x13C1E8;       // lui $s1, divisor_hi
OB64.NEUTRAL_GLOBAL_DIV_LO_OFFSET = 0x13C1EC;       // ori $s1, $s1, divisor_lo
OB64.NEUTRAL_GLOBAL_NORMAL_OFFSET = 0x13C1FC;       // addiu $s0, $zero, threshold
OB64.NEUTRAL_GLOBAL_ALT_OFFSET    = 0x13C200;       // alternate branch threshold
OB64.NEUTRAL_GLOBAL_BRANCH_OFFSET = 0x13C228;       // fail branch after sltu
OB64.NEUTRAL_GLOBAL_SLIDER_DIVISOR = 10000;         // coarse multiplier patch divisor
OB64.NEUTRAL_GLOBAL_SMOOTH_DIVISOR = 1000000;       // smooth x1-x3 patch divisor
OB64.NEUTRAL_GLOBAL_SMOOTH_MAX_PASS = 0x8000;       // addiu positive threshold + 1
OB64.NEUTRAL_GLOBAL_BRANCH_CHECK = 0x14600090;      // bne $v1,$zero,exit
OB64.NEUTRAL_GLOBAL_BRANCH_NEVER = 0x10000090;      // beq $zero,$zero,exit
OB64.NEUTRAL_GLOBAL_BRANCH_ALWAYS = 0x00000000;     // legacy/nuclear NOP
OB64.NEUTRAL_GLOBAL_VANILLA_THRESHOLD = 50;         // vanilla normal path, pass count = 51
OB64.NEUTRAL_GLOBAL_VANILLA_DIVISOR = 72000;
OB64.NEUTRAL_GLOBAL_VANILLA_BASIS_POINTS = 7;       // nearest 10000-divisor patch to 51/72000
OB64.NEUTRAL_GLOBAL_VANILLA_MICRO_BASIS_POINTS = 700; // 7 bp, in hundredths of a bp
OB64.NEUTRAL_GLOBAL_SAFE_MAX_MULTIPLIER = 3;
OB64.NEUTRAL_GLOBAL_HARD_MAX_MULTIPLIER = 100;      // 100 * 7 bp = 7.00%

OB64.TERRAIN_NAMES = {
  0: 'Plains / Roads',
  1: 'Plains',
  2: 'Barrens',
  3: 'Forests',
  4: 'Marshes',
  5: 'Highlands',
  6: 'Snowy Plains',
  7: 'Snowy Barrens',
  8: 'Snowy Forests',
  9: 'Snowy Highlands'
};

OB64.parseNeutralTerrainRates = function(z64) {
  var entries = [];
  for (var i = 0; i < OB64.NEUTRAL_TERRAIN_TABLE_LEN; i++) {
    var rate = z64[OB64.NEUTRAL_TERRAIN_RATE_OFFSET + i];
    var rawLookup = z64[OB64.NEUTRAL_TERRAIN_SLOT_OFFSET + i];
    var encounterSlot = rawLookup ? rawLookup - 1 : null;
    entries.push({
      terrainByte: i,
      rate: rate,
      rawLookup: rawLookup,
      encounterSlot: encounterSlot,
      terrainName: encounterSlot == null ? 'Disabled' : (OB64.TERRAIN_NAMES[encounterSlot] || ('Slot ' + encounterSlot)),
      enabled: rawLookup !== 0
    });
  }
  return {
    entries: entries,
    rateOffset: OB64.NEUTRAL_TERRAIN_RATE_OFFSET,
    slotOffset: OB64.NEUTRAL_TERRAIN_SLOT_OFFSET
  };
};

OB64.parseNeutralGlobalRate = function(z64) {
  function decodeDivisor() {
    var hiWord = OB64.readU32BE(z64, OB64.NEUTRAL_GLOBAL_DIV_HI_OFFSET);
    var loWord = OB64.readU32BE(z64, OB64.NEUTRAL_GLOBAL_DIV_LO_OFFSET);
    var valid = ((hiWord & 0xFFFF0000) === 0x3C110000) &&
                ((loWord & 0xFFFF0000) === 0x36310000);
    return {
      hiWord: hiWord,
      loWord: loWord,
      valid: valid,
      value: valid ? ((((hiWord & 0xFFFF) << 16) | (loWord & 0xFFFF)) >>> 0) : 0
    };
  }
  function decodeThreshold(off) {
    var word = OB64.readU32BE(z64, off);
    var valid = (word & 0xFFFF0000) === 0x24100000; // addiu $s0,$zero,imm
    var imm = word & 0xFFFF;
    var signed = imm >= 0x8000 ? imm - 0x10000 : imm;
    return {
      word: word,
      valid: valid && signed >= 0,
      value: signed
    };
  }
  function passBasisPoints(threshold, divisor) {
    if (!divisor || threshold < 0) return 0;
    return Math.max(0, Math.min(10000, Math.round(((threshold + 1) * 10000) / divisor)));
  }
  function passMicroBasisPoints(threshold, divisor) {
    if (!divisor || threshold < 0) return 0;
    return Math.max(0, Math.min(1000000, Math.round(((threshold + 1) * 1000000) / divisor)));
  }

  var divisor = decodeDivisor();
  var normal = decodeThreshold(OB64.NEUTRAL_GLOBAL_NORMAL_OFFSET);
  var alternate = decodeThreshold(OB64.NEUTRAL_GLOBAL_ALT_OFFSET);
  var branchWord = OB64.readU32BE(z64, OB64.NEUTRAL_GLOBAL_BRANCH_OFFSET);
  var mode = 'unknown';
  var basisPoints = 0;
  var microBasisPoints = 0;
  if (branchWord === OB64.NEUTRAL_GLOBAL_BRANCH_NEVER) {
    mode = 'never';
    basisPoints = 0;
    microBasisPoints = 0;
  } else if (branchWord === OB64.NEUTRAL_GLOBAL_BRANCH_ALWAYS) {
    mode = 'always';
    basisPoints = 10000;
    microBasisPoints = 1000000;
  } else if (branchWord === OB64.NEUTRAL_GLOBAL_BRANCH_CHECK && divisor.valid && normal.valid) {
    mode = 'threshold';
    basisPoints = passBasisPoints(normal.value, divisor.value);
    microBasisPoints = passMicroBasisPoints(normal.value, divisor.value);
  }

  return {
    divisor: divisor.value,
    divisorValid: divisor.valid,
    normalThreshold: normal.value,
    normalValid: normal.valid,
    alternateThreshold: alternate.value,
    alternateValid: alternate.valid,
    branchWord: branchWord,
    mode: mode,
    basisPoints: basisPoints,
    microBasisPoints: microBasisPoints,
    originalBasisPoints: basisPoints,
    originalMicroBasisPoints: microBasisPoints,
    normalBasisPoints: divisor.valid && normal.valid ? passBasisPoints(normal.value, divisor.value) : null,
    alternateBasisPoints: divisor.valid && alternate.valid ? passBasisPoints(alternate.value, divisor.value) : null,
    normalMicroBasisPoints: divisor.valid && normal.valid ? passMicroBasisPoints(normal.value, divisor.value) : null,
    alternateMicroBasisPoints: divisor.valid && alternate.valid ? passMicroBasisPoints(alternate.value, divisor.value) : null,
    modified: false
  };
};

// Resolve the value shown when the Encounters tab is rendered. The source ROM
// instruction pattern remains unchanged until export, so a session/project edit
// must take precedence over a still-vanilla parsed pattern on later renders.
OB64.neutralGlobalRateDisplayMicroBasisPoints = function(globalRate) {
  var vanillaMicroBp = OB64.NEUTRAL_GLOBAL_VANILLA_MICRO_BASIS_POINTS || 700;
  if (!globalRate) return vanillaMicroBp;

  var currentMicroBp = globalRate.microBasisPoints != null
    ? Number(globalRate.microBasisPoints)
    : Number(globalRate.basisPoints || 0) * 100;
  function clamp(value) {
    return Math.max(0, Math.min(1000000, Math.round(value)));
  }

  if (globalRate.modified && isFinite(currentMicroBp)) {
    return clamp(currentMicroBp);
  }

  var isVanillaPattern = globalRate.mode === 'threshold' &&
    globalRate.divisor === OB64.NEUTRAL_GLOBAL_VANILLA_DIVISOR &&
    globalRate.normalThreshold === OB64.NEUTRAL_GLOBAL_VANILLA_THRESHOLD;
  if (isVanillaPattern) return vanillaMicroBp;

  return isFinite(currentMicroBp) ? clamp(currentMicroBp) : 0;
};

OB64.parseNeutralEncounters = function(z64) {
  var tableStart = OB64.NEUTRAL_ENCOUNTER_OFFSET;
  var lead = OB64.NEUTRAL_ENCOUNTER_LEADING_PAD;
  var stride = OB64.NEUTRAL_ENCOUNTER_STRIDE;
  var slotCount = OB64.NEUTRAL_ENCOUNTER_SLOTS;
  var records = [];
  for (var i = 0; i < OB64.NEUTRAL_ENCOUNTER_COUNT; i++) {
    var s0 = i + 1;
    var off = tableStart + lead + i * stride;
    var slots = [];
    var isEmpty = true;
    for (var s = 0; s < slotCount; s++) {
      var a = z64[off + s * 2];
      var b = z64[off + s * 2 + 1];
      if (a !== 0 || b !== 0) isEmpty = false;
      slots.push({
        slotIdx: s,
        terrainName: OB64.TERRAIN_NAMES[s] || ('Slot ' + s),
        classA: a,
        classB: b
      });
    }
    records.push({
      s0: s0,           // dispatcher's scenario index (1-40)
      row: i,           // array index used elsewhere
      offset: off,
      isEmpty: isEmpty,
      slots: slots
    });
  }
  return {
    records: records,
    leadingPad: lead,
    tableStart: tableStart,
    globalRate: OB64.parseNeutralGlobalRate(z64),
    terrainRates: OB64.parseNeutralTerrainRates(z64)
  };
};

// ============================================================
// Creature drop table — per-class drop list, 36 × 8 B at ROM 0x142258.
// Record: [pad:u8, classId:u8, slot1:u16BE, slot2:u16BE, slot3:u16BE]
// High bit of each slot (0x8000) is preserved as raw bit 15. Its runtime
// meaning is not verified; low 15 bits are the item ID.
// Record 35 is an all-zero sentinel. Outside CRC window.
// Indexed BY CLASS ID — editing affects every scenario using that class.
// ============================================================
OB64.CREATURE_DROP_OFFSET = 0x142258;
OB64.CREATURE_DROP_COUNT  = 36;
OB64.CREATURE_DROP_STRIDE = 8;

OB64.parseCreatureDrops = function(z64) {
  var base = OB64.CREATURE_DROP_OFFSET;
  var byClass = {};
  var records = [];
  for (var i = 0; i < OB64.CREATURE_DROP_COUNT; i++) {
    var off = base + i * OB64.CREATURE_DROP_STRIDE;
    var padByte = z64[off];
    var classId = z64[off + 1];
    var slotRaw = [
      OB64.readU16BE(z64, off + 2),
      OB64.readU16BE(z64, off + 4),
      OB64.readU16BE(z64, off + 6)
    ];
    var isSentinel = (padByte === 0 && classId === 0 && slotRaw[0] === 0 && slotRaw[1] === 0 && slotRaw[2] === 0);
    var slots = slotRaw.map(function(raw) {
      return {
        raw: raw,
        itemId: raw & 0x7FFF,
        isEquipment: (raw & 0x8000) !== 0
      };
    });
    var rec = {
      recordIndex: i,
      offset: off,
      padByte: padByte,
      classId: classId,
      slots: slots,
      isSentinel: isSentinel
    };
    records.push(rec);
    if (!isSentinel) byClass[classId] = rec;
  }
  return { records: records, byClass: byClass };
};

// ============================================================
// Scenario $s0 → name table for neutral encounter cards.
// Complete mapping for all 39 non-empty slices, derived by best-match
// scoring every $s0 slice against every scraped wiki scene.
//
// $s0=40 is empty (all zeros) — skipped at render time.
// ============================================================
OB64.ENCOUNTER_SCENARIO_NAMES = {
  1:  'Tenne Plains',
  2:  'Volmus Mine I',
  3:  'Crenel Canyon I',
  4:  'Mylesia I',
  5:  'Zenobian Border',
  6:  'Volmus Mine II',
  7:  'Gunther Piedmont',
  8:  'Dardunnelles I',
  9:  'Alba',
  10: 'Crenel Canyon II',
  11: 'Mylesia II',
  12: 'Highland of Soathon',
  13: 'Sable Lowlands',
  14: 'Audvera Heights',
  15: 'Mount Ithaca',
  16: 'Azure Plains',
  17: 'Mount Keryoleth I',
  18: 'Wentinus I',
  19: 'Dardunnelles II',
  20: 'Gules Hills I',
  21: 'Fair Heights',
  22: 'Capitrium',
  23: 'Vert Plateau',
  24: 'Celesis',
  25: 'Tremos Mountains (North)',
  26: 'Temple of Berthe I',
  27: 'Temple of Berthe II',
  28: 'Tremos Mountains (South)',
  29: 'Gules Hills II',
  30: 'Romulus',
  31: 'Blue Basilica',
  32: 'Barpheth',
  33: 'Ptia',
  34: 'Tundra of Argent',
  35: 'Tybell',
  36: 'Latium',
  37: 'Aurua Plains I',
  38: 'Wentinus II',
  39: 'Mount Keryoleth II'
};

// ============================================================
// Wiki-sourced back-row attack counts — SUPERSEDED by direct ROM decode.
//
// A cross-check against a community class chart's rear-attack column
// showed B48 matches the rear count at 79/79 classes (100%). B48, which
// was previously mislabeled "atkType (uncertain)" in both docs and editor,
// IS the rear-row attack count. The record's `rearAtks` field reads it
// directly from ROM and is fully editable, like frontAtks / midAtks.
//
// This lookup table is kept as a read-only cross-reference only (useful
// for sanity-checking a patched ROM's B48 values against the community
// chart). UI no longer reads it for display. See
// scripts/ob64_csv_cross_check.js for the verification.
// ============================================================
OB64.WIKI_BACK_ROW_ATKS = {
  0x01: 1,  // Soldier
  0x02: 1,  // Fighter
  0x03: 1,  // Lycanthrope
  0x04: 2,  // Amazon
  0x05: 1,  // Knight
  0x06: 1,  // Berserker
  0x07: 1,  // Fencer
  0x08: 1,  // Phalanx
  0x09: 1,  // Beast Tamer
  0x0A: 1,  // Doll Master
  0x0B: 1,  // Ninja
  0x0C: 2,  // Wizard
  0x0D: 2,  // Archer
  0x0E: 1,  // Dragon Tamer
  0x0F: 2,  // Valkyrie
  0x10: 2,  // Witch
  0x11: 2,  // Sorceress
  0x12: 2,  // Cleric
  0x13: 2,  // Paladin
  0x14: 2,  // Dragoon
  0x15: 2,  // Black Knight
  0x16: 2,  // Sword Master
  0x17: 2,  // Cataphract
  0x18: 2,  // Beast Master
  0x19: 2,  // Enchanter
  0x1A: 2,  // Ninja Master
  0x1B: 2,  // Archmage
  0x1C: 3,  // Diana
  0x1D: 2,  // Dragon Master
  0x1E: 2,  // Freya
  0x1F: 2,  // Siren
  0x20: 2,  // Priest
  0x21: 2,  // Princess
  0x22: 1,  // Centurion (Male)
  0x23: 1,  // Centurion (Female)
  0x24: 2,  // Angel Knight
  0x25: 2,  // Seraph
  0x26: 3,  // Lich
  0x27: 1,  // Hawkman
  0x28: 2,  // Vultan
  0x29: 2,  // Raven
  0x2A: 1,  // Werewolf
  0x2B: 2,  // Vampire
  0x2C: 1,  // Vampire (in coffin)
  0x2D: 1,  // Zombie (Male)
  0x2E: 1,  // Zombie (Female)
  0x2F: 1,  // Skeleton
  0x30: 2,  // Ghost
  0x31: 2,  // Gorgon
  0x32: 1,  // Pumpkinhead
  0x33: 2,  // Faerie
  0x34: 2,  // Gremlin
  0x35: 1,  // Goblin
  0x36: 2,  // Saturos
  0x37: 1,  // Ogre
  0x38: 1,  // Young Dragon
  0x39: 1,  // Thunder Dragon
  0x3A: 1,  // Red Dragon
  0x3B: 1,  // Earth Dragon
  0x3C: 1,  // Blue Dragon
  0x3D: 1,  // Platinum Dragon
  0x3E: 1,  // Black Dragon
  0x3F: 2,  // Quetzalcoatl
  0x40: 2,  // Flarebrass
  0x41: 2,  // Ahzi Dahaka
  0x42: 2,  // Hydra
  0x43: 2,  // Bahamut
  0x44: 2,  // Tiamat
  0x45: 1,  // Wyrm
  0x46: 2,  // Wyvern
  0x47: 2,  // Griffin
  0x48: 2,  // Opinincus
  0x49: 2,  // Cockatrice
  0x4A: 2,  // Sphinx
  0x4B: 1,  // Hellhound
  0x4C: 2,  // Cerberus
  0x4E: 1,  // Golem
  0x4F: 1,  // Stone Golem
  0x50: 1,  // Baldr Golem
  0x51: 2,  // Gladiator
  0x52: 2,  // Vanguard
  0x53: 2,  // General
  0x56: 2,  // Blaze Knight
  0x57: 2   // Rune Knight
  // Giant (0x4D) has a terminator ROM record — CSV has data but the
  // class def is missing, so the editor never reaches this row.
};

// ============================================================
// Find all LHA archives in z64 ROM
// ============================================================
OB64.findArchives = function(z64) {
  var archives = [];
  for (var i = 2; i < z64.length - 20; i++) {
    if (z64[i] !== 0x2D || z64[i + 1] !== 0x6C || z64[i + 2] !== 0x68) continue;
    // Check -lh?- pattern
    var c3 = z64[i + 3], c4 = z64[i + 4];
    if (c4 !== 0x2D) continue;
    if (!((c3 >= 0x30 && c3 <= 0x39) || c3 === 0x73)) continue; // 0-9 or 's'

    var hs = i - 2;
    var cs = OB64.readU32LE(z64, i + 5);
    var us = OB64.readU32LE(z64, i + 9);
    if (cs <= 0 || cs >= 0x1000000 || us <= 0 || us >= 0x1000000) continue;
    var lv = z64[i + 18];
    var ths = lv === 2 ? OB64.readU16LE(z64, hs) : 2 + z64[hs];
    archives.push({
      idx: archives.length,
      offset: hs,
      compSize: cs,
      uncompSize: us,
      totalHeaderSize: ths,
      method: String.fromCharCode(z64[i], z64[i+1], z64[i+2], z64[i+3], z64[i+4]),
      level: lv
    });
  }
  return archives;
};

// ============================================================
// LH5 Decompressor — Huffman + LZSS, 8KB sliding window
// ============================================================
(function() {
  var DICBIT = 13, DICSIZ = 1 << DICBIT;
  var NC = 510, NP = 14, NT = 19;
  var THRESHOLD = 3;
  var CBIT = 9, TBIT = 5, PBIT = 4;

  // Bit reader state (module-level for decode functions)
  var brData, brPos, bitbuf, subbitbuf, bitcount;

  function initBitReader(data) {
    brData = data; brPos = 0;
    bitbuf = 0; subbitbuf = 0; bitcount = 0;
    fillbuf(16);
  }

  function fillbuf(n) {
    bitbuf = (bitbuf << n) & 0xFFFF;
    while (n > bitcount) {
      n -= bitcount;
      bitbuf |= (subbitbuf << n) & 0xFFFF;
      subbitbuf = brPos < brData.length ? brData[brPos++] : 0;
      bitcount = 8;
    }
    bitcount -= n;
    bitbuf |= (subbitbuf >>> bitcount) & ((1 << n) - 1);
  }

  function getbits(n) {
    var x = (bitbuf >>> (16 - n)) & ((1 << n) - 1);
    fillbuf(n);
    return x;
  }

  // Shared tree arrays (reused across blocks)
  var left = new Uint16Array(2 * NC);
  var right = new Uint16Array(2 * NC);

  // Build decode table from canonical Huffman code lengths
  function makeTable(nchar, bitlen, tablebits, table) {
    var count = new Uint16Array(17);
    for (var i = 0; i < nchar; i++) if (bitlen[i]) count[bitlen[i]]++;

    // Compute left-justified (16-bit) start codes for each length
    var start = new Array(18);
    start[1] = 0;
    for (var i = 1; i <= 16; i++) {
      start[i + 1] = start[i] + (count[i] << (16 - i));
    }

    var jutbits = 16 - tablebits;
    var avail = nchar;

    for (var ch = 0; ch < nchar; ch++) {
      var len = bitlen[ch];
      if (len === 0) continue;

      var ljcode = start[len];
      start[len] += (1 << (16 - len));

      if (len <= tablebits) {
        // Fill direct table entries
        var idx = ljcode >>> jutbits;
        var num = 1 << (tablebits - len);
        for (var i = 0; i < num; i++) table[idx + i] = ch;
      } else {
        // Overflow into binary tree
        var tblIdx = ljcode >>> jutbits;
        if (table[tblIdx] < nchar) {
          left[avail] = 0; right[avail] = 0;
          table[tblIdx] = avail++;
        }

        var node = table[tblIdx];
        // Walk remaining bits (tablebits+1 .. len-1 are internal, len is leaf)
        for (var j = tablebits; j < len - 1; j++) {
          var bit = (ljcode >>> (15 - j)) & 1;
          if (bit) {
            if (right[node] === 0) { left[avail] = 0; right[avail] = 0; right[node] = avail++; }
            node = right[node];
          } else {
            if (left[node] === 0) { left[avail] = 0; right[avail] = 0; left[node] = avail++; }
            node = left[node];
          }
        }
        // Place leaf at last bit
        if ((ljcode >>> (15 - (len - 1))) & 1) right[node] = ch;
        else left[node] = ch;
      }
    }
  }

  // Decode one symbol using table + overflow tree
  function decode(table, lens, nchar, tablebits) {
    var c = table[(bitbuf >>> (16 - tablebits)) & ((1 << tablebits) - 1)];
    if (c >= nchar) {
      // Walk overflow tree — peek at bits beyond tablebits
      var mask = 1 << (15 - tablebits);
      do {
        c = (bitbuf & mask) ? right[c] : left[c];
        mask >>>= 1;
      } while (c >= nchar);
    }
    fillbuf(lens[c]);
    return c;
  }

  // Read pre-tree / position-tree lengths
  function readPTLen(lens, nn, nbit, iSpecial) {
    var n = getbits(nbit);
    if (n === 0) {
      var c = getbits(nbit);
      for (var i = 0; i < nn; i++) lens[i] = 0;
      return c; // single symbol
    }
    var i = 0;
    while (i < Math.min(n, nn)) {
      var c = (bitbuf >>> 13) & 7; // peek 3 bits
      if (c < 7) {
        fillbuf(3);
      } else {
        var mask = 1 << 12;
        while (mask & bitbuf) { mask >>>= 1; c++; }
        fillbuf(c - 3);
      }
      lens[i++] = c;
      if (i === iSpecial) {
        var skip = getbits(2);
        while (skip-- > 0 && i < nn) lens[i++] = 0;
      }
    }
    while (i < nn) lens[i++] = 0;
    return -1; // no single symbol
  }

  // Read character tree lengths (using pre-tree)
  function readCLen(c_len, pt_table, pt_len, ptSingle) {
    var n = getbits(CBIT);
    if (n === 0) {
      var c = getbits(CBIT);
      for (var i = 0; i < NC; i++) c_len[i] = 0;
      return c; // single symbol
    }
    var i = 0;
    while (i < Math.min(n, NC)) {
      var c;
      if (ptSingle >= 0) {
        c = ptSingle;
      } else {
        c = decode(pt_table, pt_len, NT, 8);
      }
      if (c === 0) {
        c_len[i++] = 0;
      } else if (c === 1) {
        var run = getbits(4) + 3;
        while (run-- > 0 && i < NC) c_len[i++] = 0;
      } else if (c === 2) {
        var run = getbits(9) + 20;
        while (run-- > 0 && i < NC) c_len[i++] = 0;
      } else {
        c_len[i++] = c - 2;
      }
    }
    while (i < NC) c_len[i++] = 0;
    return -1; // no single symbol
  }

  OB64.lh5Decompress = function(compData, uncompSize) {
    initBitReader(compData);

    var output = new Uint8Array(uncompSize);
    var outPos = 0;
    var dtext = new Uint8Array(DICSIZ);
    var dPos = 0;

    // Allocate tables
    var c_len = new Uint8Array(NC);
    var c_table = new Uint16Array(4096);  // 1 << 12
    var pt_len = new Uint8Array(NT);
    var pt_table = new Uint16Array(256);  // 1 << 8
    var p_len = new Uint8Array(NP);
    var p_table = new Uint16Array(256);   // 1 << 8

    var blocksize = 0;
    var cSingle = -1, pSingle = -1;

    while (outPos < uncompSize) {
      if (blocksize === 0) {
        blocksize = getbits(16);
        if (blocksize === 0) break;

        // Read pre-tree
        var ptSingle = readPTLen(pt_len, NT, TBIT, 3);
        if (ptSingle >= 0) {
          pt_table.fill(ptSingle);
        } else {
          pt_table.fill(0);
          makeTable(NT, pt_len, 8, pt_table);
        }

        // Read character tree
        cSingle = readCLen(c_len, pt_table, pt_len, ptSingle);
        if (cSingle >= 0) {
          c_table.fill(cSingle);
        } else {
          c_table.fill(0);
          makeTable(NC, c_len, 12, c_table);
        }

        // Read position tree
        pSingle = readPTLen(p_len, NP, PBIT, -1);
        if (pSingle >= 0) {
          p_table.fill(pSingle);
        } else {
          p_table.fill(0);
          makeTable(NP, p_len, 8, p_table);
        }
      }

      blocksize--;

      // Decode character
      var c;
      if (cSingle >= 0) {
        c = cSingle;
      } else {
        c = decode(c_table, c_len, NC, 12);
      }

      if (c < 256) {
        output[outPos++] = c;
        dtext[dPos] = c;
        dPos = (dPos + 1) & (DICSIZ - 1);
      } else {
        var length = c - 256 + THRESHOLD;

        // Decode position
        var pCode;
        if (pSingle >= 0) {
          pCode = pSingle;
        } else {
          pCode = decode(p_table, p_len, NP, 8);
        }

        var position;
        if (pCode <= 1) {
          position = pCode;
        } else {
          position = (1 << (pCode - 1)) | getbits(pCode - 1);
        }

        var matchPos = (dPos - position - 1 + DICSIZ) & (DICSIZ - 1);
        for (var k = 0; k < length && outPos < uncompSize; k++) {
          var ch = dtext[(matchPos + k) & (DICSIZ - 1)];
          output[outPos++] = ch;
          dtext[dPos] = ch;
          dPos = (dPos + 1) & (DICSIZ - 1);
        }
      }
    }

    return output;
  };
})();

// ============================================================
// Extract a single archive's decompressed data
// ============================================================
OB64.extractArchive = function(z64, archive) {
  var dataStart = archive.offset + archive.totalHeaderSize;
  var compData = z64.subarray(dataStart, dataStart + archive.compSize);
  if (archive.method === '-lh0-') {
    // Uncompressed
    return compData.slice();
  }
  return OB64.lh5Decompress(compData, archive.uncompSize);
};

// ============================================================
// Parse enemydat.bin — 556 records x 35 bytes
// ============================================================
OB64.parseEnemydat = function(buf) {
  var RECSZ = 35;
  var count = Math.floor(buf.length / RECSZ);
  var squads = [];
  for (var r = 0; r < count; r++) {
    var off = r * RECSZ;
    squads.push({
      index: r,
      rawBytes: Array.prototype.slice.call(buf.subarray(off, off + RECSZ)),
      classA:  buf[off + 0],
      countA:  buf[off + 1],
      equipA:  buf[off + 3],
      flagA:   buf[off + 5],
      posA:    buf[off + 6],  // leader formation cell; posB kept as legacy alias
      posB:    buf[off + 6],
      classB:  buf[off + 7],
      equipB:  buf[off + 8],
      flagB:   buf[off + 10],
      posB1:   buf[off + 13],
      posB2:   buf[off + 14],
      posB3:   buf[off + 15],
      field13: buf[off + 13],
      field14: buf[off + 14],
      field15: buf[off + 15],
      classC:  buf[off + 16],
      equipC:  buf[off + 17],
      field19: buf[off + 19],
      posC1:   buf[off + 22],
      posC2:   buf[off + 23],
      posC3:   buf[off + 24],
      field22: buf[off + 22],
      field23: buf[off + 23],
      field24: buf[off + 24],
    });
  }
  return squads;
};

// ============================================================
// Shop capacities
// ============================================================
// Legacy archive #751 budget: 324 total equipment IDs was the measured
// compression-dependent point that fit one specific 549-byte ROM slot. It is
// not a runtime item-count limit and is no longer used by the editor's runtime
// shop overrides.
OB64.SHOP_ARCHIVE_ITEM_BUDGET = 324;
OB64.SHOP_ITEM_LIMIT = OB64.SHOP_ARCHIVE_ITEM_BUDGET; // compatibility alias

// The vanilla data happens to top out at 24 equipment entries in one shop.
// Static code at z64 0x19782C clears a 0xC8-byte u32 equipment array and a
// 0x3C-byte u32 consumable array before filling them without bounds checks:
// 50 equipment and 15 consumables are therefore the narrow static
// non-overflow ceilings. Menu behavior at those exact maxima is not yet
// cold-boot proven.
OB64.SHOP_VANILLA_MAX_EQUIPMENT = 24;
OB64.SHOP_MAX_EQUIPMENT_PER_SHOP = 50;
OB64.SHOP_MAX_CONSUMABLES_PER_SHOP = 15;
OB64.SHOP_MAX_ITEMS_PER_SHOP = OB64.SHOP_MAX_EQUIPMENT_PER_SHOP; // compatibility alias

// func_0019BE40's source-list producer writes these exact plain u16 IDs before
// appending bit-15-tagged equipment. Record 7 (Altar of Resurrection) is not
// written. Focused static cross-references found no alternate writer in this
// shop-list path, and the accepted guide corpus lists 46 storefronts with no
// Altar entry, so the retail editor baseline intentionally leaves it clear.
OB64.VANILLA_SHOP_CONSUMABLE_IDS = [1, 2, 3, 4, 5, 6, 8];

OB64.totalShopItems = function(shops) {
  var total = 0;
  for (var i = 0; i < shops.length; i++) total += shops[i].items.length;
  return total;
};

// ============================================================
// Parse shopcsv.bin — offset table + uint16 BE item ID lists
// ============================================================
OB64.parseShops = function(buf) {
  var firstOffset = OB64.readU16BE(buf, 0);
  // First non-zero offset == start of shop data; numShops entries in header.
  // A few early offsets may be 0 (sentinel "empty shop slot"). Walk header
  // forwards until we find the first non-zero to locate the data region.
  var headerEnd = 0;
  for (var i = 0; i < 512; i += 2) {
    var off = OB64.readU16BE(buf, i);
    if (off > 0) { headerEnd = off; break; }
  }
  if (headerEnd === 0) return [];
  var numShops = headerEnd / 2;
  var shops = [];
  for (var s = 0; s < numShops; s++) {
    var start = OB64.readU16BE(buf, s * 2);
    var items = [];
    if (start > 0) {
      // Next non-zero offset AFTER this one (or buf.length for the last shop)
      var end = buf.length;
      for (var ns = s + 1; ns < numShops; ns++) {
        var no = OB64.readU16BE(buf, ns * 2);
        if (no > 0 && no >= start) { end = no; break; }
      }
      for (var j = start; j + 2 <= end; j += 2) {
        items.push(OB64.readU16BE(buf, j));
      }
    }
    shops.push({
      index: s,
      items: items,
      consumables: OB64.VANILLA_SHOP_CONSUMABLE_IDS.slice(),
      runtimeOverride: false
    });
  }
  return shops;
};

// ============================================================
// Parse eset file — header + 3 sections
// ============================================================
OB64.parseEset = function(buf, archiveIdx) {
  var sec2off = OB64.readU16BE(buf, 4);
  var sec3off = OB64.readU16BE(buf, 6);
  var missionSeq = buf[8];
  var fmtVariant = buf[10];
  var subFlag = buf[11];
  var squadCount = OB64.readU16BE(buf, 14);

  var squads = [];
  for (var e = 0; e < squadCount; e++) {
    var off = 16 + e * 18;
    squads.push({
      flags: (buf[off] << 8) | buf[off + 1],
      enemydatIdx: buf[off + 2],
      entryType: buf[off + 3],
      params: buf.subarray(off + 4, off + 15),
    });
  }

  var sec2count = buf[sec2off] || 0;
  var mapNodes = [];
  for (var e = 0; e < sec2count; e++) {
    var off = sec2off + 1 + e * 18;
    mapNodes.push({
      nodeId: buf[off + 1],
      data: buf.subarray(off, off + 18),
    });
  }

  var sec3count = sec3off ? (buf[sec3off] || 0) : 0;
  var extra = [];
  for (var e = 0; e < sec3count; e++) {
    var off = sec3off + 1 + e * 10;
    extra.push(buf.subarray(off, off + 10));
  }

  return {
    archive: archiveIdx,
    missionSeq: missionSeq,
    fmtVariant: fmtVariant,
    subFlag: subFlag,
    squadCount: squadCount,
    squads: squads,
    mapNodeCount: sec2count,
    mapNodes: mapNodes,
    extraCount: sec3count,
    extra: extra,
  };
};

// ============================================================
// Parse scincsv file — 1 byte count + N x 4-byte entries
// ============================================================
OB64.parseScincsv = function(buf, archiveIdx) {
  var count = buf[0];
  var entries = [];
  for (var i = 0; i < count; i++) {
    var off = 1 + i * 4;
    entries.push({
      enemydatIdx: OB64.readU16BE(buf, off),      // bytes 0-1: enemydat record index
      flags: OB64.readU16BE(buf, off + 2),         // bytes 2-3: squad flags
    });
  }
  return { archive: archiveIdx, count: count, entries: entries };
};

// ============================================================
// Parse item stat table — 278 logical 32B records from ROM: ID 0 sentinel
// plus equipment IDs 0x01-0x115. ITEM_STAT_OFFSET is stat-framed: B0 begins
// there, while the current record's logical B28-B31 name pointer is stored at
// statOff-4..statOff-1. The final B27 is immediately before the consumable
// master table at 0x645CC (rev0) / 0x645EC (rev1).
// ============================================================
OB64.ITEM_STAT_OFFSET = 0x62310;
OB64.ITEM_STAT_COUNT = 278;
OB64.ITEM_STAT_SIZE = 32;

// Signed byte helper: values 128-255 → -128 to -1
OB64.signedByte = function(b) {
  return b > 127 ? b - 256 : b;
};

OB64.parseItemStats = function(z64) {
  var base = OB64.ITEM_STAT_OFFSET;
  var items = [];
  for (var i = 0; i < OB64.ITEM_STAT_COUNT; i++) {
    var off = base + i * OB64.ITEM_STAT_SIZE;
    // Present one logical 32-byte record to callers even though the source is
    // stat-framed: stats/tail B0-B27, then the same item's preceding pointer.
    var logicalRaw = new Uint8Array(OB64.ITEM_STAT_SIZE);
    logicalRaw.set(z64.subarray(off, off + 28), 0);
    logicalRaw.set(z64.subarray(off - 4, off), 28);
    items.push({
      index: i,
      gameId: i,                     // stat table is 0-indexed: index 0 = sentinel, index N = game_id N
      equipType: z64[off],           // byte 0: equipment type/slot
      element: z64[off + 1],         // byte 1: element
      grade: z64[off + 2],           // byte 2: quality tier within equip type (0-11)
      b3Raw: z64[off + 3],           // byte 3: reserved/unknown raw byte
      price: OB64.readU16BE(z64, off + 4), // bytes 4-5: price (uint16 BE)
      // --- Character stats (all signed bytes, confirmed via wiki cross-ref) ---
      strRaw: z64[off + 6],          // byte 6: STR (Strength)
      intRaw: z64[off + 7],          // byte 7: INT (Intelligence)
      agiRaw: z64[off + 8],          // byte 8: AGI (Agility)
      dexRaw: z64[off + 9],          // byte 9: DEX (Dexterity)
      vitRaw: z64[off + 10],         // byte 10: VIT (Vitality)
      menRaw: z64[off + 11],         // byte 11: MEN (Mentality)
      b12Raw: z64[off + 12],         // byte 12: unknown stat (not shown in game UI)
      str: OB64.signedByte(z64[off + 6]),
      int: OB64.signedByte(z64[off + 7]),
      agi: OB64.signedByte(z64[off + 8]),
      dex: OB64.signedByte(z64[off + 9]),
      vit: OB64.signedByte(z64[off + 10]),
      men: OB64.signedByte(z64[off + 11]),
      b12: OB64.signedByte(z64[off + 12]),
      // --- Elemental resistances (B13-B19, all signed) ---
      resPhys: OB64.signedByte(z64[off + 13]),   // byte 13: Physical/Strike resist
      resWind: OB64.signedByte(z64[off + 14]),    // byte 14: Wind resist
      resFire: OB64.signedByte(z64[off + 15]),    // byte 15: Flame/Fire resist
      resEarth: OB64.signedByte(z64[off + 16]),   // byte 16: Earth resist
      resWater: OB64.signedByte(z64[off + 17]),   // byte 17: Water resist
      resVirtue: OB64.signedByte(z64[off + 18]),  // byte 18: Virtue/Holy resist
      resBane: OB64.signedByte(z64[off + 19]),    // byte 19: Bane/Dark resist
      // --- Permanent level-up additions (B20-B21, eight packed 2-bit lanes) ---
      // func_00044AA4 sums the seven functioning lanes across the character's
      // four resolved equipment slots. Retail calls B20 bits 7:6 for both HP
      // and STR. Layout symmetry suggests bits 5:4 may have been intended for
      // STR, but its accessor has no accepted retail caller; intent is unproven.
      growthByte20: z64[off + 20],
      growthByte21: z64[off + 21],
      growthHpStr: (z64[off + 20] >>> 6) & 0x03,  // B20 7:6: retail adds to HP + STR
      growthUnknown: (z64[off + 20] >>> 4) & 0x03,// B20 5:4: likely intended STR; unused/unresolved
      growthInt: (z64[off + 20] >>> 2) & 0x03,    // B20 bits 3:2
      growthAgi: z64[off + 20] & 0x03,            // B20 bits 1:0
      growthDex: (z64[off + 21] >>> 6) & 0x03,    // B21 bits 7:6
      growthVit: (z64[off + 21] >>> 4) & 0x03,    // B21 bits 5:4
      growthMen: (z64[off + 21] >>> 2) & 0x03,    // B21 bits 3:2
      growthLck: z64[off + 21] & 0x03,            // B21 bits 1:0
      // B22-B27 have partial consumers but no complete accepted semantic map.
      // Logical B28-B31 are the current runtime item-name pointer, physically
      // stored at statOff-4..statOff-1. Expose each byte without inventing
      // labels so advanced edits can still round-trip exactly.
      b22Raw: z64[off + 22],
      b23Raw: z64[off + 23],
      b24Raw: z64[off + 24],
      b25Raw: z64[off + 25],
      b26Raw: z64[off + 26],
      b27Raw: z64[off + 27],
      b28Raw: z64[off - 4],
      b29Raw: z64[off - 3],
      b30Raw: z64[off - 2],
      b31Raw: z64[off - 1],
      namePtr: OB64.readU32BE(z64, off - 4),
      rawBytes: logicalRaw,
    });
  }
  return items;
};

// Repack the decoded B20/B21 lanes. Missing logical fields fall back to the
// parsed raw byte so callers that carry an older item object do not zero data.
OB64.packItemGrowthByte20 = function(item) {
  var raw = item && typeof item.growthByte20 === 'number' ? item.growthByte20 : 0;
  function lane(field, shift) {
    return item && typeof item[field] === 'number' ? item[field] & 0x03 : (raw >>> shift) & 0x03;
  }
  return (lane('growthHpStr', 6) << 6) |
    (lane('growthUnknown', 4) << 4) |
    (lane('growthInt', 2) << 2) |
    lane('growthAgi', 0);
};

OB64.packItemGrowthByte21 = function(item) {
  var raw = item && typeof item.growthByte21 === 'number' ? item.growthByte21 : 0;
  function lane(field, shift) {
    return item && typeof item[field] === 'number' ? item[field] & 0x03 : (raw >>> shift) & 0x03;
  }
  return (lane('growthDex', 6) << 6) |
    (lane('growthVit', 4) << 4) |
    (lane('growthMen', 2) << 2) |
    lane('growthLck', 0);
};

// ============================================================
// Parse ktenmain.bin — 316 records x 28 bytes (stronghold database)
// Archive #691: master stronghold definitions with names, types,
// capabilities, and shop assignments
// Record layout: B0=groupFlag, B1-20=name, B21=padding(0), B22-23=u16BE population,
// B24=morale(bits 0-6)+neutral flag(bit 7), B25=capabilities, B26=type, B27=shopIdx
// B24=0xFF (255) = mission objective marker (neutral + morale 127)
// ============================================================
OB64.KTENMAIN_REC_SIZE = 28;

OB64.parseKtenmain = function(buf) {
  var count = Math.floor(buf.length / OB64.KTENMAIN_REC_SIZE);
  var records = [];
  var currentGroup = 0;
  for (var i = 0; i < count; i++) {
    var off = i * OB64.KTENMAIN_REC_SIZE;
    var groupFlag = buf[off];
    if (groupFlag) currentGroup = groupFlag;

    // Read ASCII name at bytes 1-20, strip trailing nulls
    var nameBytes = [];
    for (var j = 1; j <= 20; j++) {
      if (buf[off + j] === 0) break;
      nameBytes.push(buf[off + j]);
    }
    var name = String.fromCharCode.apply(null, nameBytes);

    var b24 = buf[off + 24];

    records.push({
      index: i,
      groupFlag: groupFlag,       // non-zero = first record in mission group (mission ID)
      missionId: currentGroup,     // inherited mission ID for all records in group
      name: name,                  // stronghold name (ASCII)
      population: OB64.readU16BE(buf, off + 22), // bytes 22-23: u16BE population
      morale: b24 & 0x7F,         // byte 24 bits 0-6: morale (0-127)
      neutral: (b24 & 0x80) !== 0, // byte 24 bit 7: neutral flag (true = neutral at mission start)
      isObjective: b24 === 0xFF,  // 0xFF = mission objective marker
      capabilities: buf[off + 25], // byte 25: bitmask (bit0=shop, bit1=temple, bit2=treasure, bit3=mine)
      type: buf[off + 26],         // byte 26: stronghold type (0x09=town, 0x29=fort, 0x49=boss, 0x89=castle)
      shopIdx: buf[off + 27],      // byte 27: shop index (1-based into shopcsv, 0=no shop)
    });
  }
  return records;
};

// Build shop → stronghold name mapping from ktenmain records
// Returns object: { shopIdx: [name1, name2, ...], ... }
OB64.buildShopStrongholds = function(strongholds) {
  var map = {};
  for (var i = 0; i < strongholds.length; i++) {
    var s = strongholds[i];
    if (s.shopIdx === 0) continue;
    if (s.isObjective) continue; // B24=0xFF objective strongholds ignore shopIdx (dead data; Tacikent m1, Dardunnelles m44)
    if (!map[s.shopIdx]) map[s.shopIdx] = [];
    // Only add unique names
    if (map[s.shopIdx].indexOf(s.name) === -1) {
      map[s.shopIdx].push(s.name);
    }
  }
  return map;
};

// ============================================================
// Parse world map — 38 edges + 38 location names
// ============================================================
OB64.MAP_EDGE_OFFSET = 0x858E4;
OB64.MAP_EDGE_COUNT = 38;
OB64.MAP_NAME_OFFSET = 0x85984;

OB64.LOCATION_NAMES = {
  3: "Crenel Canyon", 4: "Volmus Mine", 5: "Volmus Mine",
  6: "Tenne Plains", 7: "Alba", 8: "Dardunnelles, the Crossroads",
  9: "Gunther Piedmont", 10: "Mylesia", 11: "Gules Hills",
  12: "Tremos Mountains", 13: "Fair Heights", 14: "Temple of Berthe",
  15: "Capitrium, the Land of Advent", 16: "Celesis, the Eastern Church",
  17: "Tremos Mountains", 18: "Sable Lowlands", 19: "Audvera Heights",
  20: "The Highland of Soathon", 21: "Mount Ithaca", 22: "Azure Plains",
  23: "Wentinus", 24: "Mount Keryoleth", 25: "Tybell, the Wicked Land",
  26: "The Tundra of Argent", 27: "Vert Plateau", 28: "Aurua Plains",
  29: "Barpheth", 30: "Latium", 31: "The Blue Basilica",
  32: "Ptia, the Secluded Land", 33: "Romulus", 34: "Wentinus",
  35: "Dardunnelles, the Crossroads", 36: "Mount Keryoleth",
  37: "Winnea, Capital of Palatinus", 38: "Castle Talpaea",
  39: "Fort Romulus", 40: "Alba",
};

OB64.parseWorldMap = function(z64) {
  var edges = [];
  for (var i = 0; i < OB64.MAP_EDGE_COUNT; i++) {
    var off = OB64.MAP_EDGE_OFFSET + i * 2;
    edges.push({ nodeA: z64[off], nodeB: z64[off + 1] });
  }
  return { edges: edges, locations: OB64.LOCATION_NAMES };
};

// ============================================================
// Legacy packed-table candidate — 72 x 3 bytes at ROM 0x17F7F0.
// Older editor notes called these class growth tiers. No accepted static
// consumer ties this table to level-up growth, and the Classes UI does not use
// it. Keep the parser shape only for backward-compatible research access.
// ============================================================
OB64.GROWTH_TIER_OFFSET = 0x17F7F0;
OB64.GROWTH_TIER_COUNT = 72;

OB64.parseClassGrowth = function(z64) {
  var base = OB64.GROWTH_TIER_OFFSET;
  var entries = [];
  for (var i = 0; i < OB64.GROWTH_TIER_COUNT; i++) {
    var off = base + i * 3;
    var b0 = z64[off], b1 = z64[off + 1], b2 = z64[off + 2];
    // 6 nibbles packed into 3 bytes — high nibble first, low nibble second
    entries.push({
      classId: i,
      tiers: [
        (b0 >> 4) & 0xF,  // stat 0
        b0 & 0xF,          // stat 1
        (b1 >> 4) & 0xF,  // stat 2
        b1 & 0xF,          // stat 3
        (b2 >> 4) & 0xF,  // stat 4
        b2 & 0xF           // stat 5
      ],
      raw: [b0, b1, b2]
    });
  }
  return entries;
};

// ============================================================
// Legacy 16 x 24 table candidate at ROM 0x17F668.
// The former "growth probability curves" interpretation has no accepted
// consumer and is not part of the verified level-up formula. Preserve the raw
// parser for compatibility; do not present these bytes as class growth data.
// ============================================================
OB64.GROWTH_CURVE_OFFSET = 0x17F668;
OB64.GROWTH_CURVE_TIERS = 16;
OB64.GROWTH_CURVE_ENTRIES = 24;

OB64.parseGrowthCurves = function(z64) {
  var base = OB64.GROWTH_CURVE_OFFSET;
  var curves = [];
  for (var t = 0; t < OB64.GROWTH_CURVE_TIERS; t++) {
    var values = [];
    for (var e = 0; e < OB64.GROWTH_CURVE_ENTRIES; e++) {
      values.push(z64[base + t * OB64.GROWTH_CURVE_ENTRIES + e]);
    }
    curves.push(values);
  }
  return curves;
};

// ============================================================
// Parse class evolution tree — 69 x 9 bytes at ROM 0x654A0
// Maps promotion categories, tree positions, and target class IDs
// ============================================================
OB64.EVOLUTION_OFFSET = 0x654A0;
OB64.EVOLUTION_COUNT = 69;
OB64.EVOLUTION_RECORD_SIZE = 9;

OB64.EVOLUTION_TIERS = {
  0: 'Base',
  1: 'Basic',
  2: 'Intermediate',
  3: 'Advanced',
  4: 'Master',
  9: 'Dragon/Special'
};

OB64.parseClassEvolution = function(z64) {
  var base = OB64.EVOLUTION_OFFSET;
  var entries = [];
  for (var i = 0; i < OB64.EVOLUTION_COUNT; i++) {
    var off = base + i * OB64.EVOLUTION_RECORD_SIZE;
    var b = [];
    for (var j = 0; j < 9; j++) b.push(z64[off + j]);
    // Byte 6 should always be 0xFF as a marker
    entries.push({
      index: i,
      category: b[0],
      tree: b[1],
      branch: b[2],
      spriteIdx: b[3],
      valA: b[4],
      valB: b[5],
      marker: b[6],
      valC: b[7],
      classId: b[8],
      isSeparator: (b[0] === 0 && b[1] === 0 && b[2] === 0 && b[3] === 0 && b[4] === 0 && b[8] === 0)
    });
  }
  return entries;
};

// ============================================================
// Parse class grouping array — ROM 0x6592C
// Class IDs separated by 0xFE, terminated by 0xFF
// Groups correspond to class change menu display tiers
// ============================================================
OB64.CLASS_GROUP_OFFSET = 0x6592C;

OB64.parseClassGroups = function(z64) {
  var pos = OB64.CLASS_GROUP_OFFSET;
  var groups = [];
  var current = [];
  while (pos < OB64.CLASS_GROUP_OFFSET + 120) {
    var b = z64[pos];
    if (b === 0xFF) {
      if (current.length > 0) groups.push(current);
      break;
    } else if (b === 0xFE) {
      if (current.length > 0) groups.push(current);
      current = [];
    } else {
      current.push(b);
    }
    pos++;
  }
  return groups;
};

// ============================================================
// Parse class definition table — 166 x 72 bytes at ROM 0x5DAD8
// Mapping: record_index = class_id + 1.
// Verified by cross-referencing H2F Mod class chart CSV against ROM hex data:
// every stat, growth, resistance, and combat multiplier matches perfectly.
// Record 0 = pointer table header. Record 1 = terminator (class 0x00 "None").
// Records 2-N cover the full 164-class set (class IDs 0x01-0xA4) per the
// authoritative GameShark mapping; intermediate terminators separate categories.
// Stat order: STR, VIT, INT, MEN, AGI, DEX (6 x [u16 base, u8 growth, u8 pad])
// B24 = Alignment, B25-31 = 7 resistances (Phys/Air/Fire/Earth/Water/Virtue/Bane)
// B43/B44 = front attack ID/count, B45/B46 = middle attack ID/count,
// B47/B48 = rear attack ID/count.
// B49-B52 = combat mults (PhysAtk, MagAtk, PhysDef, MagDef).
// B53-B57 = base/intermediate/final level-progression chain + class-copy match.
// ============================================================
OB64.CLASS_DEF_OFFSET = 0x5DAD8;
OB64.CLASS_DEF_RECORD_SIZE = 72;
OB64.CLASS_DEF_TOTAL = 166;

OB64.parseClassDefs = function(z64) {
  var base = OB64.CLASS_DEF_OFFSET;
  var RS = OB64.CLASS_DEF_RECORD_SIZE;
  var records = [];
  for (var i = 0; i < OB64.CLASS_DEF_TOTAL; i++) {
    var off = base + i * RS;
    var b0 = z64[off];
    var isTerm = (b0 === 0xFF && z64[off + 1] === 0xFF);
    var isSentinel = (b0 === 0x80);

    // B0-23: 6 stats (u16BE base + u8 per-level base gain + u8 raw), then LCK
    // base at B23. Stat order: STR, VIT, INT, MEN, AGI, DEX. For ordinary
    // classes, each B2/B6/B10/B14/B18/B22 byte is the minimum before two 0/1
    // RNG rolls and equipment growth are added. B3/B7/B11/B15/B19 are not read
    // by the recovered level-up routine; their broader meaning remains unknown.
    var stats = [];
    for (var s = 0; s < 6; s++) {
      stats.push({
        base: OB64.readU16BE(z64, off + s * 4),
        g1: z64[off + s * 4 + 2],
        g2: z64[off + s * 4 + 3]
      });
    }
    var strGrowth = z64[off + 2];
    var vitGrowth = z64[off + 6];
    var intGrowth = z64[off + 10];
    var menGrowth = z64[off + 14];
    var agiGrowth = z64[off + 18];
    var dexGrowth = z64[off + 22];
    var b3Raw  = z64[off + 3];
    var b7Raw  = z64[off + 7];
    var b11Raw = z64[off + 11];
    var b15Raw = z64[off + 15];
    var b19Raw = z64[off + 19];

    // B23 = LCK (Luck base stat, range 40-60, default 50) — confirmed via level-up diff
    var lck = z64[off + 23];

    // Alignment (byte 24) and resistances (bytes 25-31)
    var alignment = z64[off + 24];
    var resistances = [];
    for (var r = 25; r <= 31; r++) resistances.push(z64[off + r]);

    // Movement type (byte 32) and padding byte B33
    var moveType = z64[off + 32];
    var b33Raw = z64[off + 33];

    // Default equipment (bytes 34-41): 4 x u16BE item IDs
    // B34-35 = weapon, B36-37 = body armor, B38-39 = shield/off-hand, B40-41 = headgear/accessory
    // Verified by cross-referencing all human classes against H2F Mod CSV.
    var defaultEquip = [];
    for (var de = 34; de <= 40; de += 2) {
      defaultEquip.push(OB64.readU16BE(z64, off + de));
    }

    // B42-48 row attack block.
    // Counts were verified in-game with a B44=10/B46=10 patch. The three
    // action IDs were verified from the ROM reader path and discriminating
    // class-chart rows on 2026-07-10:
    //   B42 = fixed equipment slot mask
    //   B43 = front row attack ID
    //   B44 = front row attack count (EDITABLE, VERIFIED)
    //   B45 = middle row attack ID
    //   B46 = middle row attack count (EDITABLE, VERIFIED)
    //   B47 = rear row attack ID
    //   B48 = rear row attack count
    var b42Raw    = z64[off + 42];
    var b43Raw    = z64[off + 43];
    var frontAtks = z64[off + 44];
    var b45Raw    = z64[off + 45];
    var midAtks   = z64[off + 46];
    var b47Raw    = z64[off + 47];

    // Deprecated pre-decode shape retained for external compatibility only.
    // These values are not equipment slot types/groups.
    var equipSlots = [
      { slotType: b42Raw,    equipGroup: b43Raw },
      { slotType: frontAtks, equipGroup: b45Raw },
      { slotType: midAtks,   equipGroup: b47Raw }
    ];

    // B48 = rear-row attack count. Previously mislabeled "atkType" — cross-check
    // against "Class Chart.csv" Rear Attack # column matched 79/79 classes (100%).
    // Combined with B44 (front count) and B46 (mid count), all three row-attack
    // counts now decoded. See scripts/ob64_csv_cross_check.js.
    // B49-B53 = combat multipliers + flags.
    var rearAtks   = z64[off + 48];
    var atkTypeRaw = rearAtks; // legacy alias — keep for any external consumer
    var rowAttacks = [
      { attackId: b43Raw, count: frontAtks },
      { attackId: b45Raw, count: midAtks },
      { attackId: b47Raw, count: rearAtks }
    ];
    var physAtk    = z64[off + 49];
    var magAtk     = z64[off + 50];
    var physDef    = z64[off + 51];
    var magDef     = z64[off + 52];
    // B53-B57 are the level-progression class chain, not combat flags plus
    // promotion prerequisites. func_00044934 resolves the class row for an
    // absolute level as:
    //   level < B54 -> B53
    //   level < B56 -> B55
    //   otherwise   -> the target/current class
    // B57 is compared with the character's +0x12 class copy to decide whether
    // the primary or copied class identity supplies the row.
    var baseClass = z64[off + 53];
    var baseTransitionLevel = z64[off + 54];
    var intermediateClass = z64[off + 55];
    var finalTransitionLevel = z64[off + 56];
    var classCopyMatch = z64[off + 57];

    // Back-compat aliases for old patch consumers. These names were based on
    // superseded labels; new code must use the canonical names above.
    var flagsRaw = baseClass;
    var reqLevel = baseTransitionLevel;
    var reqClass = intermediateClass;
    var reqClassLevel = finalTransitionLevel;
    var additionalReqRaw = classCopyMatch;
    var attacks = [atkTypeRaw, physAtk, magAtk, physDef, magDef, flagsRaw];

    // B58 = default damage element (0xFF=Random/None, 0x00-0x04=element index)
    var dragonElement = z64[off + 58];

    // B59 = per-character contribution to the squad's carried-item capacity.
    // Effective squad capacity is min(sum(member B59), 10 physical item slots).
    var itemCapacity = z64[off + 59];
    var category = itemCapacity; // Backward-compatible alias for older class-object consumers.

    // Current-class name pointer at name-framed H+0..H+3 / logical B60-B63.
    // Physical storage is statOff-12..-9 because `off` is stat-framed.
    var namePtr = OB64.readU32BE(z64, off - 12);
    var namePtr0Raw = z64[off - 12];
    var namePtr1Raw = z64[off - 11];
    var namePtr2Raw = z64[off - 10];
    var namePtr3Raw = z64[off - 9];

    // Unit SIZE / footprint (0x01=regular/1-cell, 0x02=large/blocks adjacent).
    // The size byte lives in the name-framed class record at +4, which is
    // stat-framed off-8 for the current class ID. The old off+64 view is shifted
    // one class ahead (e.g. Saturos 0x36 incorrectly inherited Ogre's size).
    var unitSize = z64[off - 8];

    // nameOff+5 / statOff-7: guide-facing sex/voice/body code (consumer TBD).
    var sexOrVoice = z64[off - 7];

    // nameOff+6 / statOff-6: guide-facing leadership byte.
    var leadership = z64[off - 6];

    // B67 padding, B68 sentinel (0xFF only for Stone Golem/Barkeep — those are isTerm)
    var headerPad = z64[off - 5];
    var baseHp = OB64.readU16BE(z64, off - 4);

    // nameOff+10 HP growth per level.
    var hpGrowth = z64[off - 2];

    // nameOff+11 padding/raw tail.
    var headerTailRaw = z64[off - 1];

    // Back-compat aliases. Older code called these B65-B71, but they now mirror
    // the current class's name-framed header instead of the next class's header.
    var ptr = namePtr;
    var spriteType = sexOrVoice;
    var combatBehavior = leadership;
    var b67Raw = headerPad;
    var b68Raw = (baseHp >> 8) & 0xFF;
    var powerRating = baseHp & 0xFF;
    var unitCount = hpGrowth;
    var b71Raw = headerTailRaw;

    records.push({
      index: i,
      offset: off,
      isTerm: isTerm,
      isSentinel: isSentinel,
      stats: stats,
      // Named per-level base gains — mirror of stats[i].g1. Edit dispatch must
      // keep both in sync (see renderClasses edit callbacks).
      strGrowth: strGrowth, vitGrowth: vitGrowth, intGrowth: intGrowth,
      menGrowth: menGrowth, agiGrowth: agiGrowth, dexGrowth: dexGrowth,
      b3Raw: b3Raw, b7Raw: b7Raw, b11Raw: b11Raw, b15Raw: b15Raw, b19Raw: b19Raw,
      lck: lck,                     // B23
      alignment: alignment,         // B24
      resistances: resistances,     // B25-31: [Phys, Wind, Fire, Earth, Water, Virtue, Bane]
      moveType: moveType,           // B32
      b33Raw: b33Raw,               // B33 padding
      defaultEquip: defaultEquip,   // B34-41: [weapon, body, offhand, headgear] u16BE
      // B42-48 row-attack block (was mislabeled equipSlots)
      b42Raw: b42Raw, b43Raw: b43Raw,
      frontAtks: frontAtks,         // B44 — front row attack count (EDITABLE, verified)
      b45Raw: b45Raw,
      midAtks: midAtks,             // B46 — middle row attack count (EDITABLE, verified)
      b47Raw: b47Raw,
      rowAttacks: rowAttacks,       // canonical [{attackId,count}] front/middle/rear
      equipSlots: equipSlots,       // LEGACY back-compat shape (do not use in new code)
      // B48 = rear-row attack count (verified against CSV 79/79)
      rearAtks: rearAtks,           // B48
      atkTypeRaw: atkTypeRaw,       // legacy alias — same byte as rearAtks
      physAtk: physAtk,             // B49
      magAtk: magAtk,               // B50
      physDef: physDef,             // B51
      magDef: magDef,               // B52
      // Level-progression chain (MIPS-verified in func_00044934).
      baseClass: baseClass,                         // B53
      baseTransitionLevel: baseTransitionLevel,     // B54 (absolute level)
      intermediateClass: intermediateClass,         // B55
      finalTransitionLevel: finalTransitionLevel,   // B56 (absolute level)
      classCopyMatch: classCopyMatch,               // B57
      // Superseded aliases retained for patch compatibility.
      flagsRaw: flagsRaw,
      attacks: attacks,
      reqLevel: reqLevel,
      reqClass: reqClass,
      reqClassLevel: reqClassLevel,
      additionalReqRaw: additionalReqRaw,
      additionalReq: additionalReqRaw,
      dragonElement: dragonElement, // B58
      itemCapacity: itemCapacity,   // B59
      category: category,           // Superseded B59 alias retained for compatibility
      ptr: ptr,                     // logical B60-63 current-class name pointer
      unitSize: unitSize,           // name-framed +4 (regular/large footprint)
      namePtr: namePtr,
      namePtr0Raw: namePtr0Raw,     // name-framed +0 / logical B60
      namePtr1Raw: namePtr1Raw,     // name-framed +1 / logical B61
      namePtr2Raw: namePtr2Raw,     // name-framed +2 / logical B62
      namePtr3Raw: namePtr3Raw,     // name-framed +3 / logical B63
      sexOrVoice: sexOrVoice,       // nameOff+5 / statOff-7
      leadership: leadership,       // nameOff+6 / statOff-6
      headerPad: headerPad,         // nameOff+7 / statOff-5
      baseHp: baseHp,               // nameOff+8..9 / statOff-4..-3
      hpGrowth: hpGrowth,           // nameOff+10 / statOff-2
      headerTailRaw: headerTailRaw, // nameOff+11 / statOff-1
      spriteType: spriteType,       // legacy alias for sexOrVoice
      combatBehavior: combatBehavior, // legacy alias for leadership
      b67Raw: b67Raw,               // legacy alias for headerPad
      b68Raw: b68Raw,               // legacy alias for baseHp high byte
      powerRating: powerRating,     // legacy alias for baseHp low byte
      unitCount: unitCount,         // legacy alias for hpGrowth
      b71Raw: b71Raw                // legacy alias for headerTailRaw
    });
  }
  return records;
};

// ============================================================
// Parse consumable/quest-item table — stride 12, ROM 0x645CC.
// 45 records covering the in-game Item menu (Consumable + Treasure tabs).
// Record layout: [name_ptr:u32BE][flag_hi:u16BE][price:u16BE][flag_lo:u32BE].
// name_ptr is a RAM address 0x801905xx..0x801906xx pointing at ASCII names
// in the code-region string block; RAM to ROM delta for these strings is
// 0x8012A100 (Heal Leaf RAM 0x801906B0 → ROM 0x665B0).
// flag_hi metadata groups (empirical; these do not grant shop membership):
//   0xFFFF = "None" sentinel (rec 0)
//   0x0000 = common/curative group (Heal Leaf..Altar of Resurrection)
//   0x0200 = warp group (Quit Gate)
//   0x0100 = quest-only (price 2550 sentinel)
//   0x0201 = special 0x0201 group (Silver Hourglass, Dowsing Rod, Love and Peace)
//   0x0300 = bestowal (price 10 nominal, never in shops)
//   0x0401 = story/Pedras (never in shops)
// ============================================================
OB64.CONSUMABLE_TABLE_OFFSET = 0x645CC;
OB64.CONSUMABLE_RECORD_SIZE = 12;
OB64.CONSUMABLE_MAX_RECORDS = 64; // walk until non-pointer
OB64.CONSUMABLE_NAME_RAM_TO_ROM = 0x8012A100; // ROM = RAM - this

OB64.CONSUMABLE_FLAG_HI = {
  0xFFFF: 'sentinel',
  0x0000: 'common',
  0x0200: 'warp',
  0x0100: 'quest',
  0x0201: 'special-0201',
  0x0300: 'bestowal',
  0x0401: 'story',
};

// flag_lo byte[1] = effect category
OB64.CONSUMABLE_EFFECT = {
  0x00: '',
  0x02: 'Boost',
  0x04: 'Warp',
  0x07: 'Curative',
};

OB64.parseConsumables = function(z64) {
  var base = OB64.CONSUMABLE_TABLE_OFFSET;
  var stride = OB64.CONSUMABLE_RECORD_SIZE;
  var recs = [];
  for (var i = 0; i < OB64.CONSUMABLE_MAX_RECORDS; i++) {
    var off = base + i * stride;
    if (off + stride > z64.length) break;
    var namePtr = OB64.readU32BE(z64, off);
    // Stop when we leave the RAM-pointer region (00000000 terminator or invalid)
    if (((namePtr >>> 24) & 0xFF) !== 0x80) break;

    var flagHi = OB64.readU16BE(z64, off + 4);
    var price = OB64.readU16BE(z64, off + 6);
    var flagLo0 = z64[off + 8];
    var flagLo1 = z64[off + 9];
    var flagLo2 = z64[off + 10];
    var flagLo3 = z64[off + 11];

    // Resolve name string from ROM. Multi-line format wraps two-line labels as
    //   0x0E <line1> 0x10 <format-byte> <line2> 0x0F
    // The format-byte (commonly 0x63) styles line 2; it's NOT a blanket
    // "strip every 0x63" rule (real letter 'c' would go missing in
    // "Pack", "Resurrection", etc.). Strip it only when it follows 0x10.
    var nameRom = namePtr - OB64.CONSUMABLE_NAME_RAM_TO_ROM;
    var name = '';
    if (nameRom >= 0 && nameRom < z64.length) {
      var bytes = [];
      var end = nameRom;
      var lastWasLineBreak = false;
      while (end < z64.length && z64[end] !== 0 && end - nameRom < 48) {
        var b = z64[end];
        if (b === 0x0E || b === 0x0F) { end++; lastWasLineBreak = false; continue; }
        if (b === 0x10) { bytes.push(0x20); end++; lastWasLineBreak = true; continue; }
        if (lastWasLineBreak) {
          // Consume format directive byte (style/color/etc.)
          end++;
          lastWasLineBreak = false;
          continue;
        }
        bytes.push(b);
        end++;
      }
      name = String.fromCharCode.apply(null, bytes);
    }

    recs.push({
      index: i,
      name: name,
      namePtr: namePtr,
      flagHi: flagHi,
      category: OB64.CONSUMABLE_FLAG_HI[flagHi] || 'unknown',
      price: price,
      flagLo: [flagLo0, flagLo1, flagLo2, flagLo3],
      effect: OB64.CONSUMABLE_EFFECT[flagLo1] || '',
      romOffset: off,
    });
  }
  return recs;
};

// Exact vanilla source-list records from func_0019BE40. shopcsv.bin does NOT
// store consumables; its high IDs remain equipment/display IDs.
OB64.KNOWN_SHOP_EXPENDABLE_IDS = {};
for (var shopConsumableIndex = 0;
     shopConsumableIndex < OB64.VANILLA_SHOP_CONSUMABLE_IDS.length;
     shopConsumableIndex++) {
  OB64.KNOWN_SHOP_EXPENDABLE_IDS[OB64.VANILLA_SHOP_CONSUMABLE_IDS[shopConsumableIndex]] = true;
}

OB64.isKnownShopExpendable = function(c) {
  return !!(c && OB64.KNOWN_SHOP_EXPENDABLE_IDS[c.index]);
};

OB64.shopExpendables = function(consumables) {
  return consumables.filter(function(c) {
    return OB64.isKnownShopExpendable(c);
  });
};

// ============================================================
// Master ROM loader — loads and parses everything
// ============================================================
OB64.loadROM = function(romData) {
  var normalized = OB64.normalizeRomImage(romData);
  if (!normalized.z64) {
    throw new Error('Unsupported or unrecognized ROM image. Please load a US retail header revision 0 or 1 .v64/.z64/.n64 ROM.');
  }
  var z64 = normalized.z64;
  var layout = OB64.detectRomLayout(z64);
  if (!layout) {
    throw new Error('Unsupported ROM revision. This editor supports US retail header revision 0 and 1 only.');
  }
  OB64.applyRomLayout(layout);
  var archives = OB64.findArchives(z64);

  // Extract key archives
  var enemydatBuf = OB64.extractArchive(z64, archives[647]);
  var ktenmainBuf = OB64.extractArchive(z64, archives[691]);
  var shopBuf = OB64.extractArchive(z64, archives[751]);

  // Parse eset files (archives 752-814)
  var esets = [];
  for (var i = 752; i <= 814; i++) {
    if (i < archives.length) {
      try {
        var buf = OB64.extractArchive(z64, archives[i]);
        esets.push(OB64.parseEset(buf, i));
      } catch(e) { /* skip invalid */ }
    }
  }

  // Parse scincsv files (archives 692-750)
  var scincsvs = [];
  for (var i = 692; i <= 750; i++) {
    if (i < archives.length) {
      try {
        var buf = OB64.extractArchive(z64, archives[i]);
        scincsvs.push(OB64.parseScincsv(buf, i));
      } catch(e) { /* skip invalid */ }
    }
  }

  var strongholds = OB64.parseKtenmain(ktenmainBuf);

  return {
    z64: z64,
    byteOrder: normalized.byteOrder,
    exportByteOrder: normalized.byteOrder,
    layout: layout,
    archives: archives,
    enemySquads: OB64.parseEnemydat(enemydatBuf),
    strongholds: strongholds,
    shopStrongholds: OB64.buildShopStrongholds(strongholds),
    shops: OB64.parseShops(shopBuf),
    esets: esets,
    scincsvs: scincsvs,
    itemStats: OB64.parseItemStats(z64),
    worldMap: OB64.parseWorldMap(z64),
    classGrowth: OB64.parseClassGrowth(z64),
    growthCurves: OB64.parseGrowthCurves(z64),
    classEvolution: OB64.parseClassEvolution(z64),
    classGroups: OB64.parseClassGroups(z64),
    classDefs: OB64.parseClassDefs(z64),
    consumables: OB64.parseConsumables(z64),
    statGates: OB64.parseStatGates(z64),
    neutralEncounters: OB64.parseNeutralEncounters(z64),
    creatureDrops: OB64.parseCreatureDrops(z64),
  };
};

/* ============================================================================
   SAVE-GAME PARSING
   Loads a RetroArch Mupen64Plus-Next .state (RZIP or raw) or a raw 8 MB
   RDRAM .bin, returns an 8 MB unswapped RDRAM Uint8Array + metadata the
   editor uses to round-trip.
   ============================================================================ */

OB64.RZIP_MAGIC = new Uint8Array([0x23, 0x52, 0x5a, 0x49, 0x50, 0x76, 0x01, 0x23]);

OB64.detectSaveFormat = function(arrayBuffer) {
  var bytes = new Uint8Array(arrayBuffer);
  if (OB64.isBizhawkSaveRam(bytes)) return 'bizhawk-saveram';
  if (OB64.isPj64Sra(bytes)) return 'pj64-sra';
  if (bytes.length >= 20) {
    var match = true;
    for (var i = 0; i < 8; i++) if (bytes[i] !== OB64.RZIP_MAGIC[i]) { match = false; break; }
    if (match) return 'rzip';
  }
  if (bytes.length === OB64.SAVE.RDRAM_SIZE) return 'bin';
  if (bytes.length >= OB64.SAVE.RDRAM_SIZE) return 'state-raw';
  return 'unknown';
};

OB64.SAVERAM_MAGIC = "QuestOG3";

OB64.isAsciiAt = function(bytes, off, text) {
  if (off < 0 || off + text.length > bytes.length) return false;
  for (var i = 0; i < text.length; i++) {
    if (bytes[off + i] !== text.charCodeAt(i)) return false;
  }
  return true;
};

OB64.isBizhawkSaveRam = function(bytes) {
  if (!bytes || bytes.length !== OB64.SAVE.SAVERAM_SIZE) return false;
  for (var i = 0; i < OB64.SAVE.SAVERAM_SLOT_COUNT; i++) {
    var base = OB64.SAVE.SAVERAM_SLOT_BASE + i * OB64.SAVE.SAVERAM_SLOT_STRIDE;
    if (OB64.isAsciiAt(bytes, base + OB64.SAVE.SAVERAM_MAGIC_OFFSET, OB64.SAVERAM_MAGIC)) return true;
  }
  return false;
};

// "QuestOG3" as it appears in a PJ64 .sra: the slot magic is 4-aligned and two
// words long, so word-reversal maps it to this fixed string at the same offset.
OB64.PJ64_SRA_MAGIC = "seuQ3GOt";

OB64.wordSwap32 = function(bytes) {
  var out = new Uint8Array(bytes.length);
  for (var i = 0; i + 3 < bytes.length; i += 4) {
    out[i]     = bytes[i + 3];
    out[i + 1] = bytes[i + 2];
    out[i + 2] = bytes[i + 1];
    out[i + 3] = bytes[i];
  }
  return out;
};

OB64.isPj64Sra = function(bytes) {
  if (!bytes || bytes.length !== OB64.SAVE.PJ64_SRA_SIZE) return false;
  for (var i = 0; i < OB64.SAVE.SAVERAM_SLOT_COUNT; i++) {
    var base = OB64.SAVE.SAVERAM_SLOT_BASE + i * OB64.SAVE.SAVERAM_SLOT_STRIDE;
    if (OB64.isAsciiAt(bytes, base + OB64.SAVE.SAVERAM_MAGIC_OFFSET, OB64.PJ64_SRA_MAGIC)) return true;
  }
  return false;
};

OB64.unwrapRzip = function(arrayBuffer) {
  if (typeof fflate === 'undefined' || !fflate.unzlibSync) {
    throw new Error('fflate not loaded \u2014 cannot decompress RZIP state.');
  }
  var bytes = new Uint8Array(arrayBuffer);
  var dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (var i = 0; i < 8; i++) {
    if (bytes[i] !== OB64.RZIP_MAGIC[i]) throw new Error('RZIP magic mismatch at byte ' + i);
  }
  var chunkSize = dv.getUint32(8, true);
  var totalSize;
  if (typeof dv.getBigUint64 === 'function') {
    totalSize = Number(dv.getBigUint64(12, true));
  } else {
    var lo = dv.getUint32(12, true), hi = dv.getUint32(16, true);
    totalSize = hi * 0x100000000 + lo;
  }
  if (!(chunkSize > 0 && totalSize > 0 && totalSize < 128 * 1024 * 1024)) {
    throw new Error('RZIP header invalid: chunk_size=' + chunkSize + ' total_size=' + totalSize);
  }
  var out = new Uint8Array(totalSize);
  var off = 20, written = 0;
  while (off < bytes.length && written < totalSize) {
    var compSize = dv.getUint32(off, true); off += 4;
    if (compSize === 0) break;
    var inflated = fflate.unzlibSync(bytes.subarray(off, off + compSize));
    out.set(inflated, written);
    written += inflated.length;
    off += compSize;
  }
  if (written < totalSize) {
    throw new Error('RZIP decompress short: got ' + written + ' of ' + totalSize + ' bytes');
  }
  return { blob: out, chunkSize: chunkSize };
};

OB64.extractRdram = function(blob) {
  if (blob.length < OB64.SAVE.RDRAM_SIZE) {
    throw new Error('Blob too small for RDRAM: ' + blob.length + ' bytes');
  }
  var rdram = new Uint8Array(OB64.SAVE.RDRAM_SIZE);
  for (var i = 0; i < OB64.SAVE.RDRAM_SIZE; i += 4) {
    rdram[i]     = blob[i + 3];
    rdram[i + 1] = blob[i + 2];
    rdram[i + 2] = blob[i + 1];
    rdram[i + 3] = blob[i];
  }
  return rdram;
};

// Does `off` look like the first roster record? Structural test that does not
// depend on the protagonist's (player-chosen) name: printable NUL-terminated
// name, valid class with intact mirror byte, sane level — repeated on the
// next record (which may instead be empty).
OB64.looksLikeRosterSlot = function(rdram, off, allowEmpty) {
  var F = OB64.SAVE.FIELD;
  var cls = rdram[off + F.CLASS_ID];
  if (cls === 0) return !!allowEmpty;
  if (cls > 0xA4) return false;
  if (rdram[off + F.CLASS_ID_COPY] !== cls) return false;
  var lvl = rdram[off + F.LEVEL];
  if (lvl === 0 || lvl > 60) return false;
  var b0 = rdram[off];
  if (b0 < 0x20 || b0 > 0x7E) return false;       // name starts printable
  var terminated = false;
  for (var i = 1; i < OB64.SAVE.NAME_MAX_LEN; i++) {
    var b = rdram[off + i];
    if (b === 0) { terminated = true; break; }
    if (b < 0x20 || b > 0x7E) return false;
  }
  return terminated;
};

OB64.findArmyBase = function(rdram, anchorName) {
  var name = anchorName || OB64.SAVE.ANCHOR_NAME;
  var stride = OB64.SAVE.CHAR_STRIDE;
  var F = OB64.SAVE.FIELD;
  // Fast path: the roster sits at its known runtime address in every save
  // we've seen — accept it on structural validation alone so a renamed
  // protagonist still loads.
  var fixed = OB64.SAVE.SAVERAM_CHARACTER_BASE + stride;
  if (fixed + stride * 2 <= rdram.length &&
      OB64.looksLikeRosterSlot(rdram, fixed, false) &&
      OB64.looksLikeRosterSlot(rdram, fixed + stride, true)) {
    return fixed;
  }
  var pat = new Uint8Array(name.length);
  for (var i = 0; i < name.length; i++) pat[i] = name.charCodeAt(i);
  outer:
  for (var off = 0; off < rdram.length - stride * 2; off += 4) {
    for (var j = 0; j < pat.length; j++) {
      if (rdram[off + j] !== pat[j]) continue outer;
    }
    if (rdram[off + pat.length] !== 0) continue;
    var cls = rdram[off + F.CLASS_ID];
    var lvl = rdram[off + F.LEVEL];
    if (cls === 0 || cls > 0xA4) continue;
    if (lvl === 0 || lvl > 60) continue;
    var cls1 = rdram[off + stride + F.CLASS_ID];
    var lvl1 = rdram[off + stride + F.LEVEL];
    if (cls1 !== 0 && cls1 > 0xA4) continue;
    if (cls1 !== 0 && (lvl1 === 0 || lvl1 > 60)) continue;
    return off;
  }
  // Last resort: structural scan with no name anchor. Require two
  // consecutive populated records (slot 1 is occupied in any real save
  // this early scan could matter for) to keep false positives out.
  for (var soff = 0; soff < rdram.length - stride * 2; soff += 4) {
    if (OB64.looksLikeRosterSlot(rdram, soff, false) &&
        OB64.looksLikeRosterSlot(rdram, soff + stride, false)) {
      return soff;
    }
  }
  return -1;
};

OB64.decodeCharName = function(rdram, off) {
  var end = off, max = off + OB64.SAVE.NAME_MAX_LEN;
  while (end < max && rdram[end] !== 0) end++;
  var s = '';
  for (var i = off; i < end; i++) s += String.fromCharCode(rdram[i]);
  return s;
};

OB64.readU16BE_rdram = function(rdram, off) {
  return (rdram[off] << 8) | rdram[off + 1];
};

OB64.parseCharacter = function(rdram, slotOff) {
  var F = OB64.SAVE.FIELD;
  var classId = rdram[slotOff + F.CLASS_ID];
  if (classId === 0) return null;
  return {
    slotOff:   slotOff,
    slotIndex: rdram[slotOff + F.SLOT_INDEX],
    name:      OB64.decodeCharName(rdram, slotOff + F.NAME),
    classId:   classId,
    level:     rdram[slotOff + F.LEVEL],
    gender:    rdram[slotOff + F.GENDER],
    element:   rdram[slotOff + F.ELEMENT],
    alignment: rdram[slotOff + F.ALIGNMENT],
    luck:      rdram[slotOff + F.LUCK],
    exp:       rdram[slotOff + F.EXP],
    hpMax:     rdram[slotOff + F.HP_MAX],
    hpCur:     rdram[slotOff + F.HP_CUR],
    stats: {
      STR: OB64.readU16BE_rdram(rdram, slotOff + F.STR),
      VIT: OB64.readU16BE_rdram(rdram, slotOff + F.VIT),
      INT: OB64.readU16BE_rdram(rdram, slotOff + F.INT),
      MEN: OB64.readU16BE_rdram(rdram, slotOff + F.MEN),
      AGI: OB64.readU16BE_rdram(rdram, slotOff + F.AGI),
      DEX: OB64.readU16BE_rdram(rdram, slotOff + F.DEX),
    },
    // Equipment: 0 = use class default. Non-zero = u8 override item id.
    // "offhand" is the shield / spellbook / accessory slot.
    equip: {
      weapon:  rdram[slotOff + F.WEAPON],
      body:    rdram[slotOff + F.BODY],
      offhand: rdram[slotOff + F.OFFHAND],
      head:    rdram[slotOff + F.HEAD],
    },
  };
};

OB64.parseGameState = function(rdram) {
  var G = OB64.SAVE.GAME_STATE;
  var gothOff = G.GOTH;
  var goth = (rdram[gothOff] << 24) | (rdram[gothOff+1] << 16) | (rdram[gothOff+2] << 8) | rdram[gothOff+3];
  goth = goth >>> 0;
  return {
    timeOfDay:       rdram[G.TIME_OF_DAY],
    chapter:         rdram[G.CHAPTER],
    missionProgress: rdram[G.MISSION_PROGRESS],
    day:             rdram[G.DAY],
    month:           rdram[G.MONTH],
    scenario:        rdram[G.SCENARIO],
    mapLocation:     rdram[G.MAP_LOCATION],
    goth:            goth,
    chaosFrame:      rdram[G.CHAOS_FRAME],
  };
};

/**
 * Parse the army equipment inventory at phys 0x196CCC.
 * Flat list of 4-byte entries, zero-record terminated:
 *   [u16 BE item_id, u8 equipped_count, u8 owned_count]
 * Returns { entries: [{off, itemId, equipped, owned}, ...] }.
 */
OB64.parseInventory = function(rdram) {
  var base = OB64.SAVE.INVENTORY_BASE;
  var size = OB64.SAVE.INVENTORY_ENTRY_SIZE;
  var max  = OB64.SAVE.INVENTORY_MAX_ENTRIES;
  var entries = [];
  for (var i = 0; i < max; i++) {
    var off = base + i * size;
    var itemId = (rdram[off] << 8) | rdram[off + 1];
    var equipped = rdram[off + 2];
    var owned    = rdram[off + 3];
    if (itemId === 0 && equipped === 0 && owned === 0) break;
    entries.push({ off: off, itemId: itemId, equipped: equipped, owned: owned });
  }
  return { entries: entries };
};

/**
 * Parse the army consumable + treasure inventory at phys 0x193AC0.
 * Flat list of 4-byte records, zero-record terminated:
 *   [u16BE consumable_id, u16BE count]
 * consumable_id indexes into the 45-entry consumable master table. Quest /
 * treasure items (e.g. Ansate Cross = id 25) are in the same list and are
 * filtered to the Treasure tab by flagHi category.
 */
OB64.parseConsumableInventory = function(rdram) {
  var base = OB64.SAVE.CONSUMABLE_INV_BASE;
  var size = OB64.SAVE.CONSUMABLE_INV_ENTRY_SIZE;
  var max  = OB64.SAVE.CONSUMABLE_INV_MAX_ENTRIES;
  var entries = [];
  for (var i = 0; i < max; i++) {
    var off = base + i * size;
    var id = (rdram[off] << 8) | rdram[off + 1];
    var count = (rdram[off + 2] << 8) | rdram[off + 3];
    if (id === 0 && count === 0) break;
    entries.push({ off: off, consumableId: id, count: count });
  }
  return { entries: entries };
};

OB64.parseSaveRamCodecCommands = function(group) {
  if (group._cmdList) return group._cmdList;
  var out = [];
  var hex = group.cmds || "";
  for (var i = 0; i + 5 < hex.length; i += 6) {
    out.push({
      off: parseInt(hex.slice(i, i + 2), 16),
      bytes: parseInt(hex.slice(i + 2, i + 4), 16) & 0x7F,
      signed: !!(parseInt(hex.slice(i + 2, i + 4), 16) & 0x80),
      bits: parseInt(hex.slice(i + 4, i + 6), 16)
    });
  }
  group._cmdList = out;
  return out;
};

OB64.saveRamBitMask = function(bits) {
  return bits >= 32 ? 0xFFFFFFFF : ((1 << bits) - 1);
};

OB64.saveRamReadBits = function(src, state, bitCount) {
  var acc = 0;
  var need = bitCount;
  while (state.avail < need) {
    need -= state.avail;
    if (state.avail > 0) {
      acc |= (state.cur & OB64.saveRamBitMask(state.avail)) << need;
    }
    state.cur = state.pos < src.length ? src[state.pos++] : 0;
    state.avail = 8;
  }
  var shift = state.avail - need;
  var value = (state.cur >>> shift) & OB64.saveRamBitMask(need);
  state.avail = shift;
  return (acc | value) >>> 0;
};

OB64.saveRamSignExtend = function(value, bits) {
  if (bits <= 0 || bits >= 32) return value >>> 0;
  var sign = 1 << (bits - 1);
  return (value ^ sign) - sign;
};

OB64.unpackSaveRamPayload = function(payload) {
  var rdram = new Uint8Array(OB64.SAVE.RDRAM_SIZE);
  var state = { pos: 0, cur: 0, avail: 0 };
  var codec = OB64.SAVERAM_MAIN_CODEC || [];
  for (var g = 0; g < codec.length; g++) {
    var group = codec[g];
    var cmds = OB64.parseSaveRamCodecCommands(group);
    for (var rec = 0; rec < group.count; rec++) {
      var base = group.base + rec * group.stride;
      for (var c = 0; c < cmds.length; c++) {
        var cmd = cmds[c];
        var value = OB64.saveRamReadBits(payload, state, cmd.bits);
        if (cmd.signed) value = OB64.saveRamSignExtend(value, cmd.bits);
        for (var b = cmd.bytes - 1; b >= 0; b--) {
          rdram[base + cmd.off + (cmd.bytes - 1 - b)] = (value >>> (b * 8)) & 0xFF;
        }
      }
    }
  }
  return rdram;
};

OB64.decodeSaveRamNameField = function(rdram, off) {
  var decoded = [];
  for (var i = 0; i < OB64.SAVE.NAME_MAX_LEN; i++) {
    var b = rdram[off + i];
    if (b === 0xFF) break;
    if (b < 0x30) {
      // Control-token names are rare for roster entries; use a readable
      // placeholder rather than dropping the rest of the name.
      var t = String(b + 1).padStart(2, "0");
      decoded.push("{T" + t + "}");
    } else {
      var ch = b - 0x10;
      decoded.push(ch >= 0x20 && ch < 0x7F && ch !== 0x7B ? String.fromCharCode(ch) : ".");
    }
  }
  for (var j = 0; j < OB64.SAVE.NAME_MAX_LEN; j++) {
    rdram[off + j] = j < decoded.join("").length ? decoded.join("").charCodeAt(j) & 0xFF : 0;
  }
};

OB64.decodeSaveRamRosterNames = function(rdram) {
  var base = OB64.SAVE.SAVERAM_CHARACTER_BASE || 0x193BC0;
  for (var i = 0; i < 100; i++) {
    OB64.decodeSaveRamNameField(rdram, base + i * OB64.SAVE.CHAR_STRIDE);
  }
};

OB64.readSaveRamAscii = function(bytes, off, maxLen) {
  var s = "";
  for (var i = 0; i < maxLen && off + i < bytes.length; i++) {
    var b = bytes[off + i];
    if (b === 0 || b === 0xFF) break;
    if (b >= 0x20 && b < 0x7F) s += String.fromCharCode(b);
  }
  return s;
};

OB64.calcSaveRamChecksums = function(bytes, base) {
  var sum = base & 0xFFFF;
  var bits = base & 0xFFFF;
  var start = base + OB64.SAVE.SAVERAM_CHECKSUM_REGION_OFFSET;
  var end = start + OB64.SAVE.SAVERAM_CHECKSUM_REGION_SIZE;
  for (var i = start; i < end; i++) {
    var b = bytes[i] || 0;
    sum = (sum + b) & 0xFFFF;
    for (var mask = b; mask; mask >>>= 1) bits = (bits + (mask & 1)) & 0xFFFF;
  }
  return { sum: sum, bits: bits };
};

OB64.parseSaveRamSlots = function(bytes) {
  var slots = [];
  for (var i = 0; i < OB64.SAVE.SAVERAM_SLOT_COUNT; i++) {
    var base = OB64.SAVE.SAVERAM_SLOT_BASE + i * OB64.SAVE.SAVERAM_SLOT_STRIDE;
    var hasMagic = OB64.isAsciiAt(bytes, base + OB64.SAVE.SAVERAM_MAGIC_OFFSET, OB64.SAVERAM_MAGIC);
    var storedSum = OB64.readU16BE(bytes, base);
    var storedBits = OB64.readU16BE(bytes, base + 2);
    var calc = OB64.calcSaveRamChecksums(bytes, base);
    var status0 = bytes[base + OB64.SAVE.SAVERAM_CHECKSUM_REGION_OFFSET];
    var empty = !hasMagic || status0 === 0xFF;
    var valid = hasMagic && !empty && storedSum === calc.sum && storedBits === calc.bits;
    if (valid) {
      // The game formats unused slots with a checksum-valid but rosterless
      // payload (seen in PJ64 .sra slot 2). Probe for the roster so the
      // picker can show them as empty instead of offering a slot that
      // errors on selection.
      try {
        var packedOff = base + OB64.SAVE.SAVERAM_PACKED_OFFSET;
        var probe = OB64.unpackSaveRamPayload(bytes.subarray(packedOff, packedOff + OB64.SAVE.SAVERAM_PACKED_SIZE));
        OB64.decodeSaveRamRosterNames(probe);
        if (OB64.findArmyBase(probe) < 0) { valid = false; empty = true; }
      } catch (e) {
        valid = false; empty = true;
      }
    }
    slots.push({
      slotIndex: i,
      base: base,
      hasMagic: hasMagic,
      empty: empty,
      valid: valid,
      checksumOk: storedSum === calc.sum && storedBits === calc.bits,
      storedSum: storedSum,
      storedBits: storedBits,
      calcSum: calc.sum,
      calcBits: calc.bits,
      name: OB64.readSaveRamAscii(bytes, base + OB64.SAVE.SAVERAM_HEADER_NAME_OFFSET, 16)
    });
  }
  return slots;
};

OB64.countEquippedItems = function(characters) {
  var counts = {};
  for (var i = 0; i < characters.length; i++) {
    var eq = characters[i].equip || {};
    ["weapon", "body", "offhand", "head"].forEach(function(k) {
      var id = eq[k] || 0;
      if (id) counts[id] = (counts[id] || 0) + 1;
    });
  }
  return counts;
};

OB64.parseSaveRamInventory = function(rdram, characters) {
  var base = OB64.SAVE.SAVERAM_EQUIPMENT_INV_BASE;
  var size = OB64.SAVE.SAVERAM_EQUIPMENT_INV_ENTRY_SIZE;
  var max = OB64.SAVE.SAVERAM_EQUIPMENT_INV_MAX_ENTRIES;
  var equipped = OB64.countEquippedItems(characters || []);
  var entries = [];
  for (var i = 0; i < max; i++) {
    var off = base + i * size;
    var itemId = (rdram[off] << 8) | rdram[off + 1];
    var owned = rdram[off + 3];
    if (itemId === 0 && owned === 0) break;
    entries.push({
      off: off,
      itemId: itemId,
      equipped: equipped[itemId] || 0,
      owned: owned,
      nativeSaveRam: true
    });
  }
  return { entries: entries };
};

OB64.parseSaveRamConsumableInventory = function(rdram) {
  var base = OB64.SAVE.SAVERAM_CONSUMABLE_INV_BASE;
  var size = OB64.SAVE.SAVERAM_CONSUMABLE_INV_ENTRY_SIZE;
  var max = OB64.SAVE.SAVERAM_CONSUMABLE_INV_MAX_ENTRIES;
  var entries = [];
  for (var i = 0; i < max; i++) {
    var off = base + i * size;
    var id = (rdram[off] << 8) | rdram[off + 1];
    var count = (rdram[off + 2] << 8) | rdram[off + 3];
    if (id === 0 && count === 0) break;
    entries.push({ off: off, consumableId: id, count: count, nativeSaveRam: true });
  }
  return { entries: entries };
};

OB64.pickSaveRamSlot = function(slots, requestedSlotIndex) {
  var selected = null;
  if (typeof requestedSlotIndex === 'number' && requestedSlotIndex >= 0) {
    for (var i = 0; i < slots.length; i++) {
      if (slots[i].slotIndex === requestedSlotIndex) { selected = slots[i]; break; }
    }
    if (!selected) {
      throw new Error('BizHawk .SaveRAM slot ' + (requestedSlotIndex + 1) + ' is outside the supported slot range.');
    }
    if (!selected.valid) {
      throw new Error('BizHawk .SaveRAM slot ' + (requestedSlotIndex + 1) + ' is empty or failed checksum validation.');
    }
    return selected;
  }
  for (var j = 0; j < slots.length; j++) {
    if (slots[j].valid) { selected = slots[j]; break; }
  }
  if (!selected) {
    throw new Error('BizHawk .SaveRAM detected, but no valid Ogre Battle 64 save slot passed checksum validation.');
  }
  return selected;
};

OB64.parseBizhawkSaveRamFromBytes = function(bytes, requestedSlotIndex) {
  bytes = bytes.slice();
  var slots = OB64.parseSaveRamSlots(bytes);
  var selected = OB64.pickSaveRamSlot(slots, requestedSlotIndex);
  var packedOff = selected.base + OB64.SAVE.SAVERAM_PACKED_OFFSET;
  var rdram = OB64.unpackSaveRamPayload(bytes.subarray(packedOff, packedOff + OB64.SAVE.SAVERAM_PACKED_SIZE));
  OB64.decodeSaveRamRosterNames(rdram);
  // The packed-save codec always rebuilds the character array at its fixed
  // RDRAM address; the protagonist (record 1 — record 0 is the army name)
  // is wherever the player's chosen name put him, so don't anchor on
  // "Magnus" — validate structurally and only fall back to the full scan.
  var armyBase = OB64.SAVE.SAVERAM_CHARACTER_BASE + OB64.SAVE.CHAR_STRIDE;
  if (!OB64.looksLikeRosterSlot(rdram, armyBase, false)) {
    armyBase = OB64.findArmyBase(rdram);
  }
  if (armyBase < 0) {
    throw new Error('Battery save slot ' + (selected.slotIndex + 1) + ' decoded, but the character roster was not found.');
  }
  var characters = [];
  for (var c = 0; c < OB64.SAVE.MAX_SLOTS; c++) {
    var off = armyBase + c * OB64.SAVE.CHAR_STRIDE;
    if (off + OB64.SAVE.CHAR_STRIDE > rdram.length) break;
    var ch = OB64.parseCharacter(rdram, off);
    if (ch) { ch.arrayIndex = c; characters.push(ch); }
  }
  // Goth (0x196A6C) and Chaos Frame (0x1936A9) are both codec-mapped, so
  // parseGameState reads real values from the unpacked payload and edits
  // pack back into the save. Other fields stay hidden for native saves
  // (see buildGameStatePanel).
  var gameState = OB64.parseGameState(rdram);
  gameState.nativeSaveRam = true;
  return {
    format: 'bizhawk-saveram',
    chunkSize: 0,
    blob: null,
    saveram: bytes,
    rdram: rdram,
    armyBase: armyBase,
    characters: characters,
    gameState: gameState,
    inventory: OB64.parseSaveRamInventory(rdram, characters),
    consumableInventory: OB64.parseSaveRamConsumableInventory(rdram),
    slots: slots,
    slotIndex: selected.slotIndex,
    slotBase: selected.base,
    slotName: selected.name,
    origBytes: bytes.slice(),
  };
};

OB64.parseBizhawkSaveRam = function(input, requestedSlotIndex) {
  var bytes = input instanceof Uint8Array ? input : new Uint8Array(input);
  return OB64.parseBizhawkSaveRamFromBytes(bytes, requestedSlotIndex);
};

OB64.parseSaveFile = function(arrayBuffer) {
  var format = OB64.detectSaveFormat(arrayBuffer);
  if (format === 'unknown') {
    throw new Error('Unrecognized save format. Expected RetroArch .state, BizHawk .SaveRAM, Project64 .sra, raw libretro state, or 8 MB .bin RDRAM dump (got ' + arrayBuffer.byteLength + ' bytes).');
  }
  if (format === 'pj64-sra') {
    // Word-swap to native order and zero-pad to the BizHawk container size;
    // from there the SRAM slot layout is identical. sourceFormat makes the
    // export path truncate + swap back so the file round-trips as a .sra.
    var native = OB64.wordSwap32(new Uint8Array(arrayBuffer));
    var container = new Uint8Array(OB64.SAVE.SAVERAM_SIZE);
    container.set(native, 0);
    var save = OB64.parseBizhawkSaveRamFromBytes(container);
    save.sourceFormat = 'pj64-sra';
    return save;
  }
  if (format === 'bizhawk-saveram') {
    return OB64.parseBizhawkSaveRam(arrayBuffer);
  }
  var rdram, blob = null, chunkSize = 0;
  if (format === 'rzip') {
    var u = OB64.unwrapRzip(arrayBuffer);
    blob = u.blob; chunkSize = u.chunkSize;
    rdram = OB64.extractRdram(blob);
  } else if (format === 'state-raw') {
    blob = new Uint8Array(arrayBuffer);
    rdram = OB64.extractRdram(blob);
  } else {
    rdram = new Uint8Array(arrayBuffer).slice();
  }
  var armyBase = OB64.findArmyBase(rdram);
  if (armyBase < 0) {
    throw new Error('Character roster not found in RDRAM. Format: ' + format + ', blob: ' + (blob ? blob.length : rdram.length) + ' bytes. Save may be from an unsupported core/version or a game state without a populated roster.');
  }
  var characters = [];
  for (var i = 0; i < OB64.SAVE.MAX_SLOTS; i++) {
    var off = armyBase + i * OB64.SAVE.CHAR_STRIDE;
    if (off + OB64.SAVE.CHAR_STRIDE > rdram.length) break;
    var ch = OB64.parseCharacter(rdram, off);
    if (ch) { ch.arrayIndex = i; characters.push(ch); }
  }
  return {
    format: format,
    chunkSize: chunkSize,
    blob: blob,
    rdram: rdram,
    armyBase: armyBase,
    characters: characters,
    gameState: OB64.parseGameState(rdram),
    inventory: OB64.parseInventory(rdram),
    consumableInventory: OB64.parseConsumableInventory(rdram),
    origBytes: new Uint8Array(arrayBuffer).slice(),
  };
};
