// OB64 Mod Editor — LH5 Compressor + LHA Repacker
// Ported from scripts/ob64_lha_repack.js (Buffer -> Uint8Array/DataView)

window.OB64 = window.OB64 || {};

// ============================================================
// LH5 Constants
// ============================================================
var DICBIT = 13, DICSIZ = 1 << DICBIT;
var MAXMATCH = 256, THRESHOLD = 3;
var NC = 256 + MAXMATCH + 2 - THRESHOLD; // 510
var NP = DICBIT + 1; // 14
var NT = 19;
var CBIT = 9, TBIT = 5, PBIT = 4;

// ============================================================
// Bit Writer — MSB-first bit packing
// ============================================================
function BitWriter() {
  this.bytes = [];
  this.buf = 0;
  this.pos = 8;
}

BitWriter.prototype.putBits = function(n, x) {
  while (n > 0) {
    if (n < this.pos) {
      this.buf |= (x & ((1 << n) - 1)) << (this.pos - n);
      this.pos -= n;
      return;
    }
    n -= this.pos;
    this.bytes.push((this.buf | ((x >>> n) & ((1 << this.pos) - 1))) & 0xFF);
    this.buf = 0;
    this.pos = 8;
  }
};

BitWriter.prototype.flush = function() {
  if (this.pos < 8) this.bytes.push(this.buf & 0xFF);
  return new Uint8Array(this.bytes);
};

// ============================================================
// CRC-16 (LHA variant: CRC-16/ARC, reflected poly 0xA001)
// ============================================================
var crcTable = new Uint16Array(256);
for (var i = 0; i < 256; i++) {
  var r = i;
  for (var j = 0; j < 8; j++) r = (r & 1) ? ((r >>> 1) ^ 0xA001) : (r >>> 1);
  crcTable[i] = r;
}

OB64.crc16 = function(data) {
  var crc = 0;
  for (var i = 0; i < data.length; i++)
    crc = crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >>> 8);
  return crc & 0xFFFF;
};

// ============================================================
// LZSS Encoder — Greedy matching, 8KB window
// ============================================================
function lzssEncode(data) {
  var tokens = [];
  var HASH_SIZE = 4096;
  var hashHead = new Int32Array(HASH_SIZE);
  var hashPrev = new Int32Array(data.length);
  hashHead.fill(-1);
  hashPrev.fill(-1);

  function hash3(p) {
    return ((data[p] << 8) ^ (data[p + 1] << 4) ^ data[p + 2]) & (HASH_SIZE - 1);
  }

  var pos = 0;
  while (pos < data.length) {
    var bestLen = 0, bestDist = 0;

    if (pos + 2 < data.length) {
      var h = hash3(pos);
      var mp = hashHead[h];
      var limit = Math.max(0, pos - DICSIZ);
      var chain = 0;

      while (mp >= limit && chain < 256) {
        var len = 0;
        var maxLen = Math.min(MAXMATCH, data.length - pos);
        while (len < maxLen && data[mp + len] === data[pos + len]) len++;
        if (len >= THRESHOLD && len > bestLen) {
          bestLen = len;
          bestDist = pos - mp;
          if (len === MAXMATCH) break;
        }
        mp = hashPrev[mp];
        chain++;
      }

      hashPrev[pos] = hashHead[h];
      hashHead[h] = pos;
    }

    if (bestLen >= THRESHOLD) {
      tokens.push({ length: bestLen, distance: bestDist });
      for (var k = 1; k < bestLen; k++) {
        if (pos + k + 2 < data.length) {
          var hk = hash3(pos + k);
          hashPrev[pos + k] = hashHead[hk];
          hashHead[hk] = pos + k;
        }
      }
      pos += bestLen;
    } else {
      tokens.push({ literal: data[pos] });
      pos++;
    }
  }
  return tokens;
}

// ============================================================
// Huffman Tree Builder
// ============================================================
function buildHuffmanLengths(freqs, numSymbols) {
  var active = [];
  for (var i = 0; i < numSymbols; i++) {
    if (freqs[i] > 0) active.push(i);
  }

  if (active.length === 0) return { lengths: new Uint8Array(numSymbols), single: 0 };
  if (active.length === 1) {
    var L = new Uint8Array(numSymbols);
    L[active[0]] = 1;
    return { lengths: L, single: active[0] };
  }

  var nodes = active.map(function(s) { return { freq: freqs[s], leaves: [{ sym: s, depth: 0 }] }; });
  while (nodes.length > 1) {
    nodes.sort(function(a, b) { return a.freq - b.freq; });
    var a = nodes.shift(), b = nodes.shift();
    var merged = [];
    for (var i = 0; i < a.leaves.length; i++) merged.push({ sym: a.leaves[i].sym, depth: a.leaves[i].depth + 1 });
    for (var i = 0; i < b.leaves.length; i++) merged.push({ sym: b.leaves[i].sym, depth: b.leaves[i].depth + 1 });
    nodes.push({ freq: a.freq + b.freq, leaves: merged });
  }

  var L = new Uint8Array(numSymbols);
  for (var i = 0; i < nodes[0].leaves.length; i++) {
    L[nodes[0].leaves[i].sym] = Math.min(nodes[0].leaves[i].depth, 16);
  }
  return { lengths: L, single: -1 };
}

function makeCanonicalCodes(lengths) {
  var n = lengths.length;
  var maxLen = 0;
  for (var i = 0; i < n; i++) if (lengths[i] > maxLen) maxLen = lengths[i];
  if (maxLen === 0) return new Uint16Array(n);

  var bl = new Uint32Array(maxLen + 1);
  for (var i = 0; i < n; i++) if (lengths[i] > 0) bl[lengths[i]]++;

  var next = new Uint32Array(maxLen + 1);
  var code = 0;
  for (var bits = 1; bits <= maxLen; bits++) {
    code = (code + bl[bits - 1]) << 1;
    next[bits] = code;
  }

  var codes = new Uint16Array(n);
  for (var i = 0; i < n; i++) if (lengths[i] > 0) codes[i] = next[lengths[i]]++;
  return codes;
}

// ============================================================
// Position code helpers
// ============================================================
function getPositionCode(p) {
  if (p === 0) return 0;
  var c = 0, q = p;
  while (q) { q >>>= 1; c++; }
  return c;
}

function getPositionExtra(p, code) {
  if (code <= 1) return { bits: 0, value: 0 };
  return { bits: code - 1, value: p & ((1 << (code - 1)) - 1) };
}

// ============================================================
// Write PT/position tree lengths
// ============================================================
function writePTLen(writer, lens, n, nbit, iSpecial) {
  var count = n;
  while (count > 0 && lens[count - 1] === 0) count--;

  writer.putBits(nbit, count);
  if (count === 0) { writer.putBits(nbit, 0); return; }

  var i = 0;
  while (i < count) {
    var k = lens[i++];
    if (k <= 6) {
      writer.putBits(3, k);
    } else {
      writer.putBits(k - 3, (1 << (k - 3)) - 2);
    }
    if (i === iSpecial) {
      while (i < 6 && lens[i] === 0) i++;
      writer.putBits(2, (i - iSpecial) & 3);
    }
  }
}

// ============================================================
// Write character tree
// ============================================================
function writeCharacterTree(writer, cLens) {
  var tFreq = new Uint32Array(NT);
  var cn = NC;
  while (cn > 0 && cLens[cn - 1] === 0) cn--;

  var ci = 0;
  while (ci < cn) {
    var k = cLens[ci++];
    if (k === 0) {
      var run = 1;
      while (ci < cn && cLens[ci] === 0) { ci++; run++; }
      if (run <= 2) tFreq[0] += run;
      else if (run <= 18) tFreq[1]++;
      else if (run === 19) { tFreq[0]++; tFreq[1]++; }
      else tFreq[2]++;
    } else {
      tFreq[k + 2]++;
    }
  }

  var tTree = buildHuffmanLengths(tFreq, NT);
  var tLens = tTree.lengths;
  var tCodes = makeCanonicalCodes(tLens);

  if (tTree.single >= 0) {
    writer.putBits(TBIT, 0);
    writer.putBits(TBIT, tTree.single);
  } else {
    writePTLen(writer, tLens, NT, TBIT, 3);
  }

  writer.putBits(CBIT, cn);
  if (cn === 0) { writer.putBits(CBIT, 0); return; }

  var i = 0;
  while (i < cn) {
    var k = cLens[i++];
    if (k === 0) {
      var run = 1;
      while (i < cn && cLens[i] === 0) { i++; run++; }
      if (run <= 2) {
        for (var j = 0; j < run; j++) writer.putBits(tLens[0], tCodes[0]);
      } else if (run <= 18) {
        writer.putBits(tLens[1], tCodes[1]);
        writer.putBits(4, run - 3);
      } else if (run === 19) {
        writer.putBits(tLens[0], tCodes[0]);
        writer.putBits(tLens[1], tCodes[1]);
        writer.putBits(4, 15);
      } else {
        writer.putBits(tLens[2], tCodes[2]);
        writer.putBits(9, run - 20);
      }
    } else {
      writer.putBits(tLens[k + 2], tCodes[k + 2]);
    }
  }
}

// ============================================================
// LH5 Compress — single-block encoder
// ============================================================
OB64.lh5Compress = function(data) {
  var tokens = lzssEncode(data);
  var writer = new BitWriter();

  var cFreq = new Uint32Array(NC);
  var pFreq = new Uint32Array(NP);
  var blockSize = 0;

  for (var t = 0; t < tokens.length; t++) {
    var tok = tokens[t];
    if ('literal' in tok) {
      cFreq[tok.literal]++;
    } else {
      cFreq[256 + tok.length - THRESHOLD]++;
      pFreq[getPositionCode(tok.distance - 1)]++;
    }
    blockSize++;
  }

  var cTree = buildHuffmanLengths(cFreq, NC);
  var cLens = cTree.lengths;
  var cCodes = makeCanonicalCodes(cLens);

  var pTree = buildHuffmanLengths(pFreq, NP);
  var pLens = pTree.lengths;
  var pCodes = makeCanonicalCodes(pLens);

  writer.putBits(16, blockSize);

  if (cTree.single >= 0) {
    writer.putBits(TBIT, 0);
    writer.putBits(TBIT, 0);
    writer.putBits(CBIT, 0);
    writer.putBits(CBIT, cTree.single);
  } else {
    writeCharacterTree(writer, cLens);
  }

  if (pTree.single >= 0) {
    writer.putBits(PBIT, 0);
    writer.putBits(PBIT, pTree.single);
  } else {
    writePTLen(writer, pLens, NP, PBIT, -1);
  }

  for (var t = 0; t < tokens.length; t++) {
    var tok = tokens[t];
    if ('literal' in tok) {
      writer.putBits(cLens[tok.literal], cCodes[tok.literal]);
    } else {
      var c = 256 + tok.length - THRESHOLD;
      writer.putBits(cLens[c], cCodes[c]);
      var p = tok.distance - 1;
      var pc = getPositionCode(p);
      writer.putBits(pLens[pc], pCodes[pc]);
      var extra = getPositionExtra(p, pc);
      if (extra.bits > 0) writer.putBits(extra.bits, extra.value);
    }
  }

  return writer.flush();
};

// ============================================================
// Build LHA archive (Level 2 header)
// ============================================================
OB64.buildLHAArchive = function(compressedData, originalData, filename) {
  var fnBytes = [];
  for (var i = 0; i < filename.length; i++) fnBytes.push(filename.charCodeAt(i));
  var fnLen = fnBytes.length;
  var extFnSize = 1 + fnLen + 2;
  var totalHeaderSize = 24 + 2 + extFnSize;

  var header = new Uint8Array(totalHeaderSize);
  var compSize = compressedData.length;
  var dataCRC = OB64.crc16(originalData);

  // Header size (LE 16-bit)
  header[0] = totalHeaderSize & 0xFF;
  header[1] = (totalHeaderSize >>> 8) & 0xFF;
  // Method: -lh5-
  header[2] = 0x2D; header[3] = 0x6C; header[4] = 0x68;
  header[5] = 0x35; header[6] = 0x2D;
  // Compressed size (LE 32-bit)
  header[7]  = compSize & 0xFF;
  header[8]  = (compSize >>> 8) & 0xFF;
  header[9]  = (compSize >>> 16) & 0xFF;
  header[10] = (compSize >>> 24) & 0xFF;
  // Uncompressed size (LE 32-bit)
  var uncompSize = originalData.length;
  header[11] = uncompSize & 0xFF;
  header[12] = (uncompSize >>> 8) & 0xFF;
  header[13] = (uncompSize >>> 16) & 0xFF;
  header[14] = (uncompSize >>> 24) & 0xFF;
  // Timestamp
  header[15] = 0x9C; header[16] = 0x3C; header[17] = 0x29; header[18] = 0x37;
  // Attribute
  header[19] = 0x20;
  // Level 2
  header[20] = 2;
  // CRC (LE 16-bit)
  header[21] = dataCRC & 0xFF;
  header[22] = (dataCRC >>> 8) & 0xFF;
  // OS ID
  header[23] = 0x4D;

  // Extended header: filename
  header[24] = extFnSize & 0xFF;
  header[25] = (extFnSize >>> 8) & 0xFF;
  header[26] = 0x01; // filename type
  for (var i = 0; i < fnLen; i++) header[27 + i] = fnBytes[i];
  // End of chain
  header[27 + fnLen] = 0;
  header[28 + fnLen] = 0;

  // Combine header + compressed data
  var result = new Uint8Array(totalHeaderSize + compressedData.length);
  result.set(header, 0);
  result.set(compressedData, totalHeaderSize);
  return result;
};

// ============================================================
// Splice archive into z64 ROM
// ============================================================
OB64.spliceArchive = function(z64, archive, newArchiveData) {
  var originalArcSize = archive.totalHeaderSize + archive.compSize;
  var newArcSize = newArchiveData.length;

  if (newArcSize > originalArcSize) {
    return { success: false, error: "New archive is " + (newArcSize - originalArcSize) + " bytes larger than original slot" };
  }

  // Copy new archive data
  z64.set(newArchiveData, archive.offset);

  // Zero-pad remaining space
  if (newArcSize < originalArcSize) {
    z64.fill(0, archive.offset + newArcSize, archive.offset + originalArcSize);
  }

  return { success: true, padded: originalArcSize - newArcSize };
};

// ============================================================
// Serialize shops back to binary
// ============================================================
OB64.serializeShops = function(shops) {
  // Header is numShops * 2 bytes. The shopcsv format stores a start-offset
  // for every shop slot up to and including the end-sentinel immediately
  // after the last non-empty shop — empty shops inside that range (e.g.
  // the reserved slot 0 that lets ktenmain use 1-based indices) keep their
  // start offset, which equals the next shop's start (zero-length range).
  // Header slots past the end-sentinel are zero padding.
  var numShops = shops.length;
  var headerSize = numShops * 2;
  var lastNonEmpty = -1;
  for (var s = 0; s < numShops; s++) {
    if (shops[s].items.length > 0) lastNonEmpty = s;
  }
  var totalItems = 0;
  for (var s = 0; s < numShops; s++) totalItems += shops[s].items.length;
  var buf = new Uint8Array(headerSize + totalItems * 2);
  var dataPos = headerSize;
  for (var s = 0; s < numShops; s++) {
    if (s <= lastNonEmpty + 1) {
      // In-range slot or the end-sentinel: record current dataPos.
      OB64.writeU16BE(buf, s * 2, dataPos);
    } else {
      OB64.writeU16BE(buf, s * 2, 0);
    }
    for (var i = 0; i < shops[s].items.length; i++) {
      OB64.writeU16BE(buf, dataPos, shops[s].items[i]);
      dataPos += 2;
    }
  }
  return buf;
};

// ============================================================
// Serialize enemydat back to binary
// ============================================================
OB64.serializeEnemydat = function(squads) {
  var RECSZ = 35;
  var buf = new Uint8Array(squads.length * RECSZ);
  for (var r = 0; r < squads.length; r++) {
    var s = squads[r];
    var off = r * RECSZ;
    buf[off + 0] = s.classA;
    buf[off + 1] = s.countA;
    buf[off + 3] = s.equipA;
    buf[off + 5] = s.flagA;
    buf[off + 6] = s.posB;
    buf[off + 7] = s.classB;
    buf[off + 8] = s.equipB;
    buf[off + 10] = s.flagB;
    buf[off + 13] = s.field13;
    buf[off + 14] = s.field14;
    buf[off + 15] = s.field15;
    buf[off + 16] = s.classC;
    buf[off + 17] = s.equipC;
    buf[off + 19] = s.field19;
    buf[off + 22] = s.field22;
    buf[off + 23] = s.field23;
    buf[off + 24] = s.field24;
  }
  return buf;
};

// ============================================================
// Class definition serializer — writes all editable fields back to z64
// Class def table: 166 x 72B at ROM z64 0x5DAD8.
// record_index = class_id + 1; records 2-N cover class IDs 0x01-0xA4
// per the GameShark mapping (164 classes plus terminator/sentinel rows).
// ============================================================
OB64.serializeClassDefs = function(classDefs, z64) {
  var base = OB64.CLASS_DEF_OFFSET;
  var RS = OB64.CLASS_DEF_RECORD_SIZE;
  for (var i = 0; i < classDefs.length; i++) {
    var r = classDefs[i];
    if (r.isTerm || r.isSentinel) continue;
    var off = base + i * RS;

    // B0-23: stats (u16BE base + u8 growth mean + u8 raw) then LCK at B23.
    // r.stats[i].g1 is kept in sync with r.{str,vit,int,men,agi,dex}Growth by
    // the edit dispatch in renderClasses, so this loop writes the UI's edits.
    for (var s = 0; s < r.stats.length; s++) {
      OB64.writeU16BE(z64, off + s * 4, r.stats[s].base);
      z64[off + s * 4 + 2] = r.stats[s].g1;
      z64[off + s * 4 + 3] = r.stats[s].g2;
    }

    z64[off + 23] = r.lck;            // B23: LCK base
    z64[off + 24] = r.alignment;      // B24: Alignment
    for (var ri = 0; ri < r.resistances.length; ri++) {
      z64[off + 25 + ri] = r.resistances[ri];                    // B25-31
    }
    z64[off + 32] = r.moveType;       // B32
    z64[off + 33] = r.b33Raw || 0;    // B33 padding (preserve)

    // B34-41: Default equipment (4 x u16BE)
    for (var de = 0; de < r.defaultEquip.length; de++) {
      OB64.writeU16BE(z64, off + 34 + de * 2, r.defaultEquip[de]);
    }

    // B42-47: row-attack block (was mislabeled equipSlots)
    z64[off + 42] = r.b42Raw || 0;
    z64[off + 43] = r.b43Raw || 0;
    z64[off + 44] = r.frontAtks;      // B44 — front row attack count
    z64[off + 45] = r.b45Raw || 0;
    z64[off + 46] = r.midAtks;        // B46 — middle row attack count
    z64[off + 47] = r.b47Raw || 0;

    // B48 = rear-row attack count (decoded from CSV cross-check).
    // Prefer rearAtks if the UI set it; fall back to legacy atkTypeRaw.
    z64[off + 48] = (r.rearAtks !== undefined ? r.rearAtks : r.atkTypeRaw) || 0;
    z64[off + 49] = r.physAtk;
    z64[off + 50] = r.magAtk;
    z64[off + 51] = r.physDef;
    z64[off + 52] = r.magDef;
    z64[off + 53] = r.flagsRaw || 0;

    z64[off + 54] = r.reqLevel;       // B54
    z64[off + 55] = r.reqClass;       // B55
    z64[off + 56] = r.reqClassLevel;  // B56
    z64[off + 57] = (r.additionalReqRaw !== undefined ? r.additionalReqRaw : r.additionalReq) || 0; // B57
    z64[off + 58] = r.dragonElement;  // B58
    z64[off + 59] = r.category;       // B59
    // B60-63: runtime RAM pointer — DO NOT WRITE (preserve ROM bytes)
    z64[off + 64] = r.unitType;       // B64
    z64[off + 65] = r.spriteType;     // B65
    z64[off + 66] = r.combatBehavior; // B66
    z64[off + 67] = r.b67Raw || 0;    // B67 padding
    z64[off + 68] = r.b68Raw || 0;    // B68 sentinel
    z64[off + 69] = r.powerRating;    // B69
    z64[off + 70] = r.unitCount;      // B70
    z64[off + 71] = r.b71Raw || 0;    // B71 padding
  }
};

// ============================================================
// Neutral encounter pool serializer — writes 40 × 20 B scenario slices
// back to ROM 0x141ED0 starting at leading-pad offset (scenario $s0=1
// lives 4 bytes into the table). Outside CIC-6102 CRC window.
// Also writes adjacent terrain-rate tables at 0x141E80 and 0x141EA0.
// ============================================================
OB64.serializeNeutralGlobalRate = function(globalRate, z64) {
  if (!globalRate || !globalRate.modified) return;

  var basisPoints = parseInt(globalRate.basisPoints, 10);
  if (!isFinite(basisPoints)) basisPoints = 0;
  if (basisPoints < 0) basisPoints = 0;
  if (basisPoints > 10000) basisPoints = 10000;

  // Export edited global rates with a 10,000-step divisor so the UI's
  // percent/basis-point value is the actual pass count over divisor.
  OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_DIV_HI_OFFSET, 0x3C110000);
  OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_DIV_LO_OFFSET, 0x36310000 | OB64.NEUTRAL_GLOBAL_SLIDER_DIVISOR);

  if (basisPoints === 0) {
    // The comparison is unsigned, so threshold -1 would pass everything.
    // Use an unconditional branch to the existing fail/exit target instead.
    OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_NORMAL_OFFSET, 0x24100000);
    OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_ALT_OFFSET, 0x24100000);
    OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_BRANCH_OFFSET, OB64.NEUTRAL_GLOBAL_BRANCH_NEVER);
  } else {
    var threshold = basisPoints - 1;
    var thresholdWord = 0x24100000 | (threshold & 0xFFFF);
    OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_NORMAL_OFFSET, thresholdWord);
    OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_ALT_OFFSET, thresholdWord);
    OB64.writeU32BE(z64, OB64.NEUTRAL_GLOBAL_BRANCH_OFFSET, OB64.NEUTRAL_GLOBAL_BRANCH_CHECK);
  }
};

OB64.serializeNeutralEncounters = function(encounters, z64) {
  OB64.serializeNeutralGlobalRate(encounters && encounters.globalRate, z64);

  var tableStart = OB64.NEUTRAL_ENCOUNTER_OFFSET;
  var lead = OB64.NEUTRAL_ENCOUNTER_LEADING_PAD;
  var stride = OB64.NEUTRAL_ENCOUNTER_STRIDE;
  var slotCount = OB64.NEUTRAL_ENCOUNTER_SLOTS;
  var records = encounters && encounters.records ? encounters.records : [];
  for (var i = 0; i < records.length; i++) {
    var rec = records[i];
    var off = tableStart + lead + i * stride;
    for (var s = 0; s < slotCount; s++) {
      var slot = rec.slots[s];
      z64[off + s * 2]     = (slot.classA || 0) & 0xFF;
      z64[off + s * 2 + 1] = (slot.classB || 0) & 0xFF;
    }
  }
  var rates = encounters && encounters.terrainRates && encounters.terrainRates.entries
    ? encounters.terrainRates.entries
    : [];
  for (var r = 0; r < rates.length; r++) {
    var entry = rates[r];
    var terrainByte = entry.terrainByte;
    if (terrainByte === undefined || terrainByte < 0 || terrainByte >= OB64.NEUTRAL_TERRAIN_TABLE_LEN) continue;
    z64[OB64.NEUTRAL_TERRAIN_RATE_OFFSET + terrainByte] = (entry.rate || 0) & 0xFF;
    z64[OB64.NEUTRAL_TERRAIN_SLOT_OFFSET + terrainByte] = (entry.rawLookup || 0) & 0xFF;
  }
};

// ============================================================
// Creature drop table serializer — writes 36 × 8 B records back to
// ROM 0x142258. Record layout: [pad, classId, slot1 u16BE × 3].
// Drop slot high bit (0x8000) = equipment flag; low 15 bits = item ID.
// Outside CIC-6102 CRC window.
// ============================================================
OB64.serializeCreatureDrops = function(drops, z64) {
  var base = OB64.CREATURE_DROP_OFFSET;
  var stride = OB64.CREATURE_DROP_STRIDE;
  var records = drops && drops.records ? drops.records : [];
  for (var i = 0; i < records.length; i++) {
    var rec = records[i];
    var off = base + i * stride;
    z64[off]     = (rec.padByte || 0) & 0xFF;
    z64[off + 1] = (rec.classId || 0) & 0xFF;
    for (var s = 0; s < 3; s++) {
      var slot = rec.slots[s];
      var raw = slot.raw;
      if (raw === undefined) {
        // Rebuild from logical fields if UI set itemId/isEquipment directly
        raw = ((slot.itemId || 0) & 0x7FFF) | (slot.isEquipment ? 0x8000 : 0);
      }
      OB64.writeU16BE(z64, off + 2 + s * 2, raw);
    }
  }
};

// ============================================================
// Consumable master table serializer — 45 × 12 B records at ROM 0x645CC.
// Editable fields: flagHi (u16BE @+4), price (u16BE @+6), flagLo (u8×4 @+8..+11).
// name_ptr (u32BE @+0..+3) is READ-ONLY — points into the ROM's name string
// block, don't touch. Inside CIC-6102 CRC window; export must recalc CRC.
// ============================================================
OB64.serializeConsumables = function(consumables, z64) {
  var base = OB64.CONSUMABLE_TABLE_OFFSET;
  var stride = OB64.CONSUMABLE_RECORD_SIZE;
  for (var i = 0; i < consumables.length; i++) {
    var rec = consumables[i];
    if (!rec || rec.romOffset === undefined) continue;
    var off = rec.romOffset;
    // name_ptr left intact.
    OB64.writeU16BE(z64, off + 4, rec.flagHi & 0xFFFF);
    OB64.writeU16BE(z64, off + 6, rec.price & 0xFFFF);
    z64[off + 8]  = (rec.flagLo[0] || 0) & 0xFF;
    z64[off + 9]  = (rec.flagLo[1] || 0) & 0xFF;
    z64[off + 10] = (rec.flagLo[2] || 0) & 0xFF;
    z64[off + 11] = (rec.flagLo[3] || 0) & 0xFF;
  }
};

// ============================================================
// LZSS compressor — mirror of scripts/ob64_lzss_compress.js, extended
// with 3-byte and 4-byte back-ref tokens so round-trips fit the original
// ROM slot (the Node version's greedy short-only encoder overfills by
// 1 byte even on a no-op recompress).
// Token layout (decoder in parsers.js):
//   0x80-0xFF  2-byte back-ref   len 3-18,   dist 1-2048
//   0x40-0x7F  literal run       count 1-64
//   0x20-0x3F  zero-fill short   count 2-33
//   0x10-0x1F  3-byte back-ref   len 4-67,   dist 1-16384
//   0x00       4-byte back-ref   len 5-260,  dist 1-65536
//   0x01       0xFF fill         count 3-258
//   0x02       zero-fill long    count 3-258
//   0x03-0x0F  NOP
// Browser port uses Uint8Array in place of Buffer.
// ============================================================
(function() {
  function findMatch(data, pos, minLen, maxLen, maxDist) {
    var bestLen = minLen - 1, bestDist = 0;
    var dmax = Math.min(pos, maxDist);
    for (var dist = 1; dist <= dmax; dist++) {
      var srcStart = pos - dist;
      var matchLen = 0;
      while (matchLen < maxLen && pos + matchLen < data.length) {
        var srcIdx = srcStart + matchLen;
        if (srcIdx >= pos) break;
        if (data[srcIdx] === data[pos + matchLen]) matchLen++;
        else break;
      }
      // Run-length extension for overlapping source
      if (matchLen >= minLen && dist <= matchLen) {
        var extLen = matchLen;
        while (extLen < maxLen && pos + extLen < data.length) {
          if (data[pos + extLen] === data[pos + extLen - dist]) extLen++;
          else break;
        }
        matchLen = extLen;
      }
      if (matchLen > bestLen) {
        bestLen = matchLen;
        bestDist = dist;
        if (bestLen >= maxLen) break;
      }
    }
    return bestLen >= minLen ? { len: bestLen, dist: bestDist } : null;
  }

  function countZeros(data, pos) {
    var c = 0;
    while (pos + c < data.length && data[pos + c] === 0) c++;
    return c;
  }
  function countFFs(data, pos) {
    var c = 0;
    while (pos + c < data.length && data[pos + c] === 0xFF) c++;
    return c;
  }

  OB64.lzssCompress = function(input) {
    var out = [];
    var pos = 0;
    var literalStart = -1;

    function flushLiterals() {
      if (literalStart < 0) return;
      var count = pos - literalStart;
      for (var off = 0; off < count; off += 64) {
        var chunk = Math.min(64, count - off);
        out.push(0x40 | (chunk - 1));
        for (var j = 0; j < chunk; j++) out.push(input[literalStart + off + j]);
      }
      literalStart = -1;
    }

    while (pos < input.length) {
      var zeros = countZeros(input, pos);
      var ffs = countFFs(input, pos);
      // Single search out to the long-back-ref distance, capped at the
      // long-back-ref length. Short/medium tokens reuse the same match
      // with appropriate clamps.
      var match = findMatch(input, pos, 3, 260, 65536);

      var bestAction = 'literal', bestEfficiency = 0, bestLen = 1;
      var bestMatchDist = 0;
      var zfPenalty = (match && match.len > zeros) ? 3.0 : 1.0;

      if (zeros >= 33) {
        var zlCount = Math.min(zeros, 258);
        var zlEff = zlCount / 2;
        if (zlEff > bestEfficiency) { bestEfficiency = zlEff; bestAction = 'zerofill_long'; bestLen = zlCount; }
      }
      if (zeros >= 2) {
        var zsCount = Math.min(zeros, 33);
        var zsEff = zsCount / zfPenalty;
        if (zsEff > bestEfficiency) { bestEfficiency = zsEff; bestAction = 'zerofill_short'; bestLen = zsCount; }
      }
      if (ffs >= 3) {
        var ffCount = Math.min(ffs, 258);
        var ffEff = ffCount / 2;
        if (ffEff > bestEfficiency) { bestEfficiency = ffEff; bestAction = 'fffill'; bestLen = ffCount; }
      }
      if (match) {
        // Short back-ref: len 3-18, dist 1-2048
        if (match.len >= 3 && match.dist <= 2048) {
          var sLen = Math.min(match.len, 18);
          var sEff = sLen / 2;
          if (sEff > bestEfficiency) { bestEfficiency = sEff; bestAction = 'backref2'; bestLen = sLen; bestMatchDist = match.dist; }
        }
        // 3-byte back-ref: len 4-67, dist 1-16384
        if (match.len >= 4 && match.dist <= 16384) {
          var mLen = Math.min(match.len, 67);
          var mEff = mLen / 3;
          if (mEff > bestEfficiency) { bestEfficiency = mEff; bestAction = 'backref3'; bestLen = mLen; bestMatchDist = match.dist; }
        }
        // 4-byte back-ref: len 5-260, dist 1-65536
        if (match.len >= 5) {
          var lLen = Math.min(match.len, 260);
          var lEff = lLen / 4;
          if (lEff > bestEfficiency) { bestEfficiency = lEff; bestAction = 'backref4'; bestLen = lLen; bestMatchDist = match.dist; }
        }
      }

      if (bestEfficiency <= 1.0) {
        if (literalStart < 0) literalStart = pos;
        pos++;
        continue;
      }

      flushLiterals();

      if (bestAction === 'zerofill_long') {
        out.push(0x02, bestLen - 3);
      } else if (bestAction === 'zerofill_short') {
        out.push(0x20 | (bestLen - 2));
      } else if (bestAction === 'fffill') {
        out.push(0x01, bestLen - 3);
      } else if (bestAction === 'backref2') {
        var b1 = 0x80 | ((bestLen - 3) << 3) | ((bestMatchDist - 1) >> 8);
        var b2 = (bestMatchDist - 1) & 0xFF;
        out.push(b1, b2);
      } else if (bestAction === 'backref3') {
        // 0x10-0x1F  b1[0-3]=len_lo, b2[6-7]=len_hi, b2[0-5]=dist_hi, b3=dist_lo
        var lenEnc = bestLen - 4;           // 0..63
        var distEnc = bestMatchDist - 1;    // 0..16383
        var b1b = 0x10 | (lenEnc & 0x0F);
        var b2b = ((lenEnc & 0x30) << 2) | ((distEnc >>> 8) & 0x3F);
        var b3b = distEnc & 0xFF;
        out.push(b1b, b2b, b3b);
      } else if (bestAction === 'backref4') {
        // 0x00  b2=len-5, b3/b4=dist-1 u16BE
        var lenEnc2 = bestLen - 5;          // 0..255
        var distEnc2 = bestMatchDist - 1;   // 0..65535
        out.push(0x00, lenEnc2 & 0xFF, (distEnc2 >>> 8) & 0xFF, distEnc2 & 0xFF);
      }
      pos += bestLen;
    }

    flushLiterals();
    return new Uint8Array(out);
  };
})();

// ============================================================
// Stat gate promotion threshold table serializer
// Block lives at z64 LZSS_GAP_START + 0x3A960C:
//   [u32 payloadSize][u32 decompSize][compData (payloadSize-4 bytes)]
// Decompresses to 81 × 8 B: [STR, VIT, INT, MEN, AGI, DEX, ALN_MIN, ALN_MAX].
// Past CRC window — no CRC recalc needed.
// Throws if the recompressed data exceeds the original compData slot.
// ============================================================
OB64.serializeStatGates = function(statGates, z64) {
  if (!statGates || !statGates.meta) {
    throw new Error('serializeStatGates: missing statGates/meta');
  }
  var meta = statGates.meta;
  if (!meta.compDataOff || !meta.compDataSize || !meta.decompSize) {
    throw new Error('serializeStatGates: incomplete meta from parseStatGates');
  }

  // Rebuild the decompressed block by starting from the original raw bytes
  // (preserves any tail bytes past the 81 × 8 B records) and overlaying
  // the current byClass values.
  var decompressed = new Uint8Array(meta.decompSize);
  if (statGates.raw && statGates.raw.length) {
    decompressed.set(statGates.raw.subarray(0, Math.min(meta.decompSize, statGates.raw.length)));
  }
  for (var cid in statGates.byClass) {
    var g = statGates.byClass[cid];
    var o = g.offset;
    if (o + 8 > decompressed.length) continue;
    decompressed[o + 0] = g.str    & 0xFF;
    decompressed[o + 1] = g.vit    & 0xFF;
    decompressed[o + 2] = g.int    & 0xFF;
    decompressed[o + 3] = g.men    & 0xFF;
    decompressed[o + 4] = g.agi    & 0xFF;
    decompressed[o + 5] = g.dex    & 0xFF;
    decompressed[o + 6] = g.alnMin & 0xFF;
    decompressed[o + 7] = g.alnMax & 0xFF;
  }

  var recompressed = OB64.lzssCompress(decompressed);
  if (recompressed.length > meta.compDataSize) {
    throw new Error('Stat-gate block overfill: recompressed '
      + recompressed.length + ' > slot ' + meta.compDataSize
      + ' (' + (recompressed.length - meta.compDataSize) + ' over)');
  }

  // Round-trip verify
  var verify = OB64.lzssDecode(recompressed, 0, meta.decompSize).output;
  for (var i = 0; i < decompressed.length; i++) {
    if (verify[i] !== decompressed[i]) {
      throw new Error('Stat-gate round-trip verify failed at byte ' + i
        + ': got ' + verify[i] + ', expected ' + decompressed[i]);
    }
  }

  // Splice: clear original slot, write new bytes, leave the rest as-is.
  // (Remainder after recompressed.length is already part of the following
  // block — must not overwrite past compDataSize.)
  var off = meta.compDataOff;
  for (var k = 0; k < recompressed.length; k++) z64[off + k] = recompressed[k];
  // Pad the rest of the slot with 0x00 NOPs (decoder treats 0x03-0x0F as NOP).
  // The ORIGINAL slot may contain real bytes after the last emitted token;
  // we overwrite those with NOPs so the decoder halts at the same decomp_size.
  for (var p = recompressed.length; p < meta.compDataSize; p++) z64[off + p] = 0x03;

  // Header payload/decomp sizes unchanged — slot size is fixed.
};

// ============================================================
// N64 CRC recalculation (CIC-6102)
// Class def table at z64 0x5DAD8 is within CRC region (0x1000-0x101000)
// Seed: 0xF8CA4DDC, formula uses t5^d (NOT bootWord^d)
// ============================================================
OB64.recalcN64CRC = function(z64) {
  var SEED = 0xF8CA4DDC;
  var CRC_START = 0x1000;
  var CRC_END = 0x101000;

  var t1 = SEED >>> 0;
  var t2 = SEED >>> 0;
  var t3 = SEED >>> 0;
  var t4 = SEED >>> 0;
  var t5 = SEED >>> 0;
  var t6 = SEED >>> 0;

  for (var i = CRC_START; i < CRC_END; i += 4) {
    var d = OB64.readU32BE(z64, i) >>> 0;

    var r = (t6 + d) >>> 0;
    if (r < t6) t4 = (t4 + 1) >>> 0;
    t6 = r;

    t3 = (t3 ^ d) >>> 0;

    var shift = d & 0x1F;
    var rotated = ((d << shift) | (d >>> (32 - shift))) >>> 0;
    t5 = (t5 + rotated) >>> 0;

    if (t2 > d) {
      t2 = (t2 ^ rotated) >>> 0;
    } else {
      t2 = (t2 ^ t6 ^ d) >>> 0;
    }

    t1 = (t1 + ((t5 ^ d) >>> 0)) >>> 0;
  }

  var crc1 = (t6 ^ t4 ^ t3) >>> 0;
  var crc2 = (t5 ^ t2 ^ t1) >>> 0;

  // Write CRC to header
  z64[0x10] = (crc1 >>> 24) & 0xFF;
  z64[0x11] = (crc1 >>> 16) & 0xFF;
  z64[0x12] = (crc1 >>> 8) & 0xFF;
  z64[0x13] = crc1 & 0xFF;
  z64[0x14] = (crc2 >>> 24) & 0xFF;
  z64[0x15] = (crc2 >>> 16) & 0xFF;
  z64[0x16] = (crc2 >>> 8) & 0xFF;
  z64[0x17] = crc2 & 0xFF;
};

// ============================================================
// Export modified ROM as .v64
// ============================================================
OB64.exportROM = function(z64) {
  var v64 = OB64.z64ToV64(z64);
  var blob = new Blob([v64], { type: 'application/octet-stream' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url;
  a.download = 'ob64_modified.v64';
  a.click();
  URL.revokeObjectURL(url);
};

/* ============================================================================
   SAVE-GAME SERIALIZATION
   Mirror of OB64.parseSaveFile — takes an edited save object and produces a
   Uint8Array in the same format the file was loaded in.
   ============================================================================ */

OB64.writeU16BE_rdram = function(rdram, off, val) {
  rdram[off]     = (val >> 8) & 0xFF;
  rdram[off + 1] = val & 0xFF;
};

/** Write a JS string into a 16-byte name field, null-padded. Truncates if too long. */
OB64.encodeCharName = function(name, rdram, off) {
  var max = OB64.SAVE.NAME_MAX_LEN;
  for (var i = 0; i < max; i++) {
    if (i < name.length) {
      rdram[off + i] = name.charCodeAt(i) & 0xFF;
    } else {
      rdram[off + i] = 0;
    }
  }
};

/**
 * Write a character object back into rdram at the given slot offset.
 * Equipment offsets (+0x2B/+0x2D/+0x2F/+0x31) hold u8 item ID overrides;
 * 0 means "use class default".
 */
OB64.writeCharacter = function(rdram, slotOff, ch) {
  var F = OB64.SAVE.FIELD;
  OB64.encodeCharName(ch.name, rdram, slotOff + F.NAME);
  rdram[slotOff + F.CLASS_ID]      = ch.classId & 0xFF;
  rdram[slotOff + F.CLASS_ID_COPY] = ch.classId & 0xFF;
  rdram[slotOff + F.LEVEL]         = ch.level & 0xFF;
  rdram[slotOff + F.HP_MAX]        = (ch.hpMax || 0) & 0xFF;
  rdram[slotOff + F.HP_CUR]        = (ch.hpCur || ch.hpMax || 0) & 0xFF;
  if (ch.gender    !== undefined) rdram[slotOff + F.GENDER]    = ch.gender    & 0xFF;
  if (ch.element   !== undefined) rdram[slotOff + F.ELEMENT]   = ch.element   & 0xFF;
  if (ch.alignment !== undefined) rdram[slotOff + F.ALIGNMENT] = ch.alignment & 0xFF;
  if (ch.exp       !== undefined) rdram[slotOff + F.EXP]       = ch.exp       & 0xFF;
  OB64.writeU16BE_rdram(rdram, slotOff + F.STR, ch.stats.STR & 0xFFFF);
  OB64.writeU16BE_rdram(rdram, slotOff + F.VIT, ch.stats.VIT & 0xFFFF);
  OB64.writeU16BE_rdram(rdram, slotOff + F.INT, ch.stats.INT & 0xFFFF);
  OB64.writeU16BE_rdram(rdram, slotOff + F.MEN, ch.stats.MEN & 0xFFFF);
  OB64.writeU16BE_rdram(rdram, slotOff + F.AGI, ch.stats.AGI & 0xFFFF);
  OB64.writeU16BE_rdram(rdram, slotOff + F.DEX, ch.stats.DEX & 0xFFFF);
  if (ch.equip) {
    rdram[slotOff + F.WEAPON]  = (ch.equip.weapon  || 0) & 0xFF;
    rdram[slotOff + F.BODY]    = (ch.equip.body    || 0) & 0xFF;
    rdram[slotOff + F.OFFHAND] = (ch.equip.offhand || 0) & 0xFF;
    rdram[slotOff + F.HEAD]    = (ch.equip.head    || 0) & 0xFF;
  }
};

/** Zero out a 56-byte character slot (used by "Remove character"). */
OB64.clearCharacterSlot = function(rdram, slotOff) {
  for (var i = 0; i < OB64.SAVE.CHAR_STRIDE; i++) rdram[slotOff + i] = 0;
};

OB64.writeGameState = function(rdram, gs) {
  var G = OB64.SAVE.GAME_STATE;
  rdram[G.TIME_OF_DAY]      = gs.timeOfDay       & 0xFF;
  rdram[G.CHAPTER]          = gs.chapter         & 0xFF;
  rdram[G.MISSION_PROGRESS] = gs.missionProgress & 0xFF;
  rdram[G.DAY]              = gs.day             & 0xFF;
  rdram[G.MONTH]            = gs.month           & 0xFF;
  rdram[G.SCENARIO]         = gs.scenario        & 0xFF;
  rdram[G.MAP_LOCATION]     = gs.mapLocation     & 0xFF;
  var goth = (gs.goth || 0) >>> 0;
  rdram[G.GOTH]             = (goth >>> 24) & 0xFF;
  rdram[G.GOTH + 1]         = (goth >>> 16) & 0xFF;
  rdram[G.GOTH + 2]         = (goth >>> 8)  & 0xFF;
  rdram[G.GOTH + 3]         =  goth         & 0xFF;
};

/**
 * Write one inventory entry back to the RAM dump. `entry` is the object
 * returned by parseInventory — { off, itemId, equipped, owned }.
 */
OB64.writeInventoryEntry = function(rdram, entry) {
  if (entry.nativeSaveRam) {
    rdram[entry.off]     = (entry.itemId >> 8) & 0xFF;
    rdram[entry.off + 1] = entry.itemId & 0xFF;
    rdram[entry.off + 2] = 0;
    rdram[entry.off + 3] = entry.owned & 0xFF;
    return;
  }
  rdram[entry.off]     = (entry.itemId >> 8) & 0xFF;
  rdram[entry.off + 1] = entry.itemId & 0xFF;
  rdram[entry.off + 2] = entry.equipped & 0xFF;
  rdram[entry.off + 3] = entry.owned & 0xFF;
};

/**
 * Write one consumable-inventory entry.
 * `entry` = { off, consumableId, count } from parseConsumableInventory.
 * Record layout is [u8 id, 0x00, u8 count, 0x00].
 */
OB64.writeConsumableInventoryEntry = function(rdram, entry) {
  if (entry.nativeSaveRam) {
    rdram[entry.off]     = (entry.consumableId >> 8) & 0xFF;
    rdram[entry.off + 1] = entry.consumableId & 0xFF;
    rdram[entry.off + 2] = 0;
    rdram[entry.off + 3] = entry.count & 0xFF;
    return;
  }
  rdram[entry.off]     = entry.consumableId & 0xFF;
  rdram[entry.off + 1] = 0;
  rdram[entry.off + 2] = entry.count & 0xFF;
  rdram[entry.off + 3] = 0;
};

/** In-place 4-byte word swap over a buffer. Idempotent: two swaps == no-op. */
OB64.wordSwapInPlace = function(bytes, len) {
  var end = (typeof len === 'number') ? len : bytes.length;
  for (var i = 0; i < end; i += 4) {
    var a = bytes[i], b = bytes[i + 1], c = bytes[i + 2], d = bytes[i + 3];
    bytes[i] = d; bytes[i + 1] = c; bytes[i + 2] = b; bytes[i + 3] = a;
  }
};

/**
 * Splice the modified 8 MB RDRAM back into the libretro state blob,
 * word-re-swapping in the process. Returns a fresh Uint8Array blob.
 */
OB64.patchRdramInBlob = function(blob, rdram) {
  var out = new Uint8Array(blob.length);
  out.set(blob);
  for (var i = 0; i < OB64.SAVE.RDRAM_SIZE; i += 4) {
    out[i]     = rdram[i + 3];
    out[i + 1] = rdram[i + 2];
    out[i + 2] = rdram[i + 1];
    out[i + 3] = rdram[i];
  }
  return out;
};

/**
 * Re-wrap a libretro state blob into an RZIP container with the given
 * chunk_size (matching the source file's chunking).
 */
OB64.rewrapRzip = function(blob, chunkSize) {
  if (typeof fflate === 'undefined' || !fflate.zlibSync) {
    throw new Error('fflate not loaded \u2014 cannot re-encode RZIP state.');
  }
  var numChunks = Math.ceil(blob.length / chunkSize);
  var chunkPieces = [];
  var totalComp = 0;
  for (var i = 0; i < numChunks; i++) {
    var start = i * chunkSize;
    var end = Math.min(start + chunkSize, blob.length);
    var compressed = fflate.zlibSync(blob.subarray(start, end), { level: 6 });
    chunkPieces.push(compressed);
    totalComp += 4 + compressed.length;
  }
  var out = new Uint8Array(20 + totalComp);
  out.set(OB64.RZIP_MAGIC, 0);
  var dv = new DataView(out.buffer);
  dv.setUint32(8, chunkSize, true);
  if (typeof dv.setBigUint64 === 'function') {
    dv.setBigUint64(12, BigInt(blob.length), true);
  } else {
    dv.setUint32(12, blob.length >>> 0, true);
    dv.setUint32(16, Math.floor(blob.length / 0x100000000), true);
  }
  var off = 20;
  for (var j = 0; j < chunkPieces.length; j++) {
    dv.setUint32(off, chunkPieces[j].length, true);
    off += 4;
    out.set(chunkPieces[j], off);
    off += chunkPieces[j].length;
  }
  return out;
};

OB64.saveRamWriteBits = function(out, state, value, bitCount) {
  value >>>= 0;
  var remaining = bitCount;
  while (remaining > 0) {
    if (state.avail === 0) {
      out[state.pos++] = state.cur & 0xFF;
      state.cur = 0;
      state.avail = 8;
    }
    var take = Math.min(state.avail, remaining);
    var shift = remaining - take;
    var chunk = (value >>> shift) & OB64.saveRamBitMask(take);
    state.avail -= take;
    state.cur |= chunk << state.avail;
    remaining -= take;
  }
};

OB64.encodeSaveRamNameField = function(rdram, off) {
  var name = OB64.decodeCharName(rdram, off);
  for (var i = 0; i < OB64.SAVE.NAME_MAX_LEN; i++) rdram[off + i] = 0;
  var maxChars = OB64.SAVE.NAME_MAX_LEN - 1;
  var n = Math.min(name.length, maxChars);
  for (var j = 0; j < n; j++) {
    var ch = name.charCodeAt(j) & 0x7F;
    rdram[off + j] = (ch + 0x10) & 0xFF;
  }
  rdram[off + n] = 0xFF;
};

OB64.encodeSaveRamRosterNames = function(rdram) {
  var base = OB64.SAVE.SAVERAM_CHARACTER_BASE || 0x193BC0;
  for (var i = 0; i < 100; i++) {
    OB64.encodeSaveRamNameField(rdram, base + i * OB64.SAVE.CHAR_STRIDE);
  }
};

OB64.packSaveRamPayload = function(rdram) {
  var out = new Uint8Array(OB64.SAVE.SAVERAM_PACKED_SIZE);
  var state = { pos: 0, cur: 0, avail: 8 };
  var codec = OB64.SAVERAM_MAIN_CODEC || [];
  for (var g = 0; g < codec.length; g++) {
    var group = codec[g];
    var cmds = OB64.parseSaveRamCodecCommands(group);
    for (var rec = 0; rec < group.count; rec++) {
      var base = group.base + rec * group.stride;
      for (var c = 0; c < cmds.length; c++) {
        var cmd = cmds[c];
        var value = 0;
        for (var b = 0; b < cmd.bytes; b++) value = ((value << 8) | rdram[base + cmd.off + b]) >>> 0;
        OB64.saveRamWriteBits(out, state, value, cmd.bits);
      }
    }
  }
  if (state.avail < 8 && state.pos < out.length) out[state.pos++] = state.cur & 0xFF;
  return out;
};

OB64.exportBizhawkSaveRam = function(save) {
  var out = (save.saveram || save.origBytes).slice();
  var rdram = save.rdram.slice();
  var base = save.slotBase;
  if (typeof base !== 'number') {
    base = OB64.SAVE.SAVERAM_SLOT_BASE + (save.slotIndex || 0) * OB64.SAVE.SAVERAM_SLOT_STRIDE;
  }
  OB64.encodeSaveRamRosterNames(rdram);
  var packed = OB64.packSaveRamPayload(rdram);
  out.set(packed, base + OB64.SAVE.SAVERAM_PACKED_OFFSET);
  var checks = OB64.calcSaveRamChecksums(out, base);
  OB64.writeU16BE(out, base, checks.sum);
  OB64.writeU16BE(out, base + 2, checks.bits);
  return out;
};

/**
 * Top-level export: produce a Uint8Array in the same format the save was
 * loaded in. `save` is the object returned by parseSaveFile, with `rdram`
 * possibly mutated via writeCharacter / writeGameState.
 */
OB64.exportSaveFile = function(save) {
  if (save.format === 'bizhawk-saveram') {
    return OB64.exportBizhawkSaveRam(save);
  }
  if (save.format === 'bin') {
    return save.rdram.slice();
  }
  if (save.format === 'state-raw') {
    return OB64.patchRdramInBlob(save.blob, save.rdram);
  }
  if (save.format === 'rzip') {
    var patched = OB64.patchRdramInBlob(save.blob, save.rdram);
    return OB64.rewrapRzip(patched, save.chunkSize || 0x20000);
  }
  throw new Error('exportSaveFile: unknown save format ' + save.format);
};

OB64.downloadSaveFile = function(save, originalFileName) {
  var bytes = OB64.exportSaveFile(save);
  var baseName = (originalFileName || 'save').replace(/\.(state\d*|bin|saveram)$/i, '');
  var ext = '.state';
  if (save.format === 'bin') ext = '.bin';
  else if (save.format === 'bizhawk-saveram') ext = '.SaveRAM';
  else if (originalFileName && originalFileName.match(/\.(state\d*)$/i)) ext = originalFileName.match(/\.(state\d*)$/i)[0];
  var fileName = baseName + '-edited' + ext;
  var blob = new Blob([bytes], { type: 'application/octet-stream' });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  a.click();
  URL.revokeObjectURL(url);
};
