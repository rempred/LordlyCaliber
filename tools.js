// OB64 Mod Editor - Tools engine
//
// Byte-level ROM feature toggles for the Tools tab. Feature definitions live
// in tools-data.js (generated from the verified research builds); this file
// is the hand-written engine that detects, applies, and removes them.
//
// Each feature is a set of writes into the z64 buffer. A write knows both its
// patched bytes and the original retail bytes (or a zero-filled cave), so a
// feature can be detected in a loaded ROM, applied on export, or removed
// again by restoring the originals. Anything else occupying those bytes means
// some other patch touched them -- the feature reports 'foreign' and the UI
// refuses to toggle it rather than corrupt an unknown mod.

window.OB64 = window.OB64 || {};

(function() {

  function hexToBytes(hex) {
    var out = new Uint8Array(hex.length / 2);
    for (var i = 0; i < out.length; i++) {
      out[i] = parseInt(hex.substr(i * 2, 2), 16);
    }
    return out;
  }

  function patchedBytes(write) {
    if (!write._patched) write._patched = hexToBytes(write.patched);
    return write._patched;
  }

  function originalBytes(write) {
    if (!write._original) {
      write._original = write.original
        ? hexToBytes(write.original)
        : new Uint8Array(write.originalZeros);
    }
    return write._original;
  }

  function hex(n, width) {
    var s = (n >>> 0).toString(16).toUpperCase();
    while (width && s.length < width) s = '0' + s;
    return '0x' + s;
  }

  function patchedLength(write) {
    return patchedBytes(write).length;
  }

  function assertRomOffset(z64, offset, len, label) {
    if (!Number.isFinite(offset) || offset < 0) {
      throw new Error('Invalid ROM write offset for ' + label + ': ' + offset);
    }
    if (offset >= 0x80000000) {
      throw new Error('Patch write for ' + label + ' uses a RAM address as a ROM offset: ' + hex(offset, 8));
    }
    if (offset + len > z64.length) {
      throw new Error('Patch write for ' + label + ' exceeds ROM length: ' +
        hex(offset, 6) + ' + ' + len + ' > ' + z64.length);
    }
  }

  function regionEquals(z64, offset, bytes) {
    assertRomOffset(z64, offset, bytes.length, 'feature detection');
    for (var i = 0; i < bytes.length; i++) {
      if (z64[offset + i] !== bytes[i]) return false;
    }
    return true;
  }

  function writeRegion(z64, offset, bytes) {
    assertRomOffset(z64, offset, bytes.length, 'feature apply');
    z64.set(bytes, offset);
  }

  function normalizeRegion(owner, region, fromWrite) {
    if (!region) return null;
    var start = Number(region.start != null ? region.start : region.offset);
    var size = Number(region.size != null ? region.size : region.length);
    var kind = region.kind || (fromWrite ? 'rom' : null);
    if (!kind || !Number.isFinite(start) || !Number.isFinite(size) || size <= 0) return null;
    return {
      ownerId: owner.id,
      ownerName: owner.name,
      kind: kind,
      start: start,
      end: start + size,
      size: size,
      label: region.label || (fromWrite ? 'ROM write' : 'region'),
    };
  }

  function featureRegions(feature) {
    var out = [];
    var writes = feature.writes || [];
    for (var i = 0; i < writes.length; i++) {
      out.push(normalizeRegion(feature, {
        kind: 'rom',
        start: writes[i].offset,
        size: patchedLength(writes[i]),
        label: writes[i].label || ('write ' + i),
      }, true));
    }
    var explicit = feature.regions || [];
    for (var r = 0; r < explicit.length; r++) {
      var nr = normalizeRegion(feature, explicit[r], false);
      if (nr) out.push(nr);
    }
    return out.filter(Boolean);
  }

  function featureUnsupportedReason(rom, feature) {
    if (!rom || !rom.layout || !feature) return '';
    if (rom.layout.supportsTools === false) {
      return rom.layout.unsupportedFeaturesReason || 'Tools are not available for this ROM revision.';
    }
    var blocked = rom.layout.unsupportedTools || {};
    if (Object.prototype.hasOwnProperty.call(blocked, feature.id)) {
      return blocked[feature.id] || 'This tool is not available for this ROM revision.';
    }
    var allowed = rom.layout.supportedTools || null;
    if (allowed && allowed[feature.id] !== true) {
      return 'This tool has not been enabled for this ROM revision.';
    }
    return '';
  }

  function featureSupported(rom, feature) {
    return !featureUnsupportedReason(rom, feature);
  }

  function rangesOverlap(a, b) {
    return a.kind === b.kind && a.start < b.end && b.start < a.end;
  }

  function explicitlyExclusive(a, b) {
    var ax = a.exclusiveWith || [];
    var bx = b.exclusiveWith || [];
    return ax.indexOf(b.id) !== -1 || bx.indexOf(a.id) !== -1;
  }

  function describeConflict(c) {
    return c.a.ownerName + ' ' + c.a.kind + ' ' + hex(c.a.start, 6) + '..' + hex(c.a.end - 1, 6) +
      ' (' + c.a.label + ') overlaps ' +
      c.b.ownerName + ' ' + c.b.kind + ' ' + hex(c.b.start, 6) + '..' + hex(c.b.end - 1, 6) +
      ' (' + c.b.label + ')';
  }

  function findRegionConflicts(regionOwners, options) {
    options = options || {};
    var owners = regionOwners || [];
    var regions = [];
    var byId = {};
    for (var i = 0; i < owners.length; i++) {
      byId[owners[i].id] = owners[i];
      var rs = owners[i].regions || featureRegions(owners[i]);
      for (var r = 0; r < rs.length; r++) {
        var nr = rs[r].ownerId ? rs[r] : normalizeRegion(owners[i], rs[r], false);
        if (nr) regions.push(nr);
      }
    }
    var out = [];
    for (var a = 0; a < regions.length; a++) {
      for (var b = a + 1; b < regions.length; b++) {
        if (regions[a].ownerId === regions[b].ownerId) continue;
        if (!rangesOverlap(regions[a], regions[b])) continue;
        var ownerA = byId[regions[a].ownerId] || {};
        var ownerB = byId[regions[b].ownerId] || {};
        var exclusive = explicitlyExclusive(ownerA, ownerB);
        if (!exclusive || options.includeExclusive) {
          out.push({ a: regions[a], b: regions[b], exclusive: exclusive });
        }
      }
    }
    return out;
  }

  function validateFeatureRegistry(z64) {
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var writes = list[i].writes || [];
      for (var w = 0; w < writes.length; w++) {
        assertRomOffset(z64, writes[w].offset, patchedLength(writes[w]), list[i].name + ' / ' + (writes[w].label || w));
      }
    }
    var conflicts = findRegionConflicts(list, { includeExclusive: false });
    if (conflicts.length) {
      throw new Error('Tool patch region collision:\n  ' + conflicts.map(describeConflict).join('\n  '));
    }
  }

  function desiredRegionConflicts(rom, extraOwners) {
    if (!rom || !rom.tools) return [];
    var owners = [];
    var list = features();
    for (var i = 0; i < list.length; i++) {
      if (rom.tools.initial[list[i].id] === 'foreign') continue;
      if (!featureSupported(rom, list[i])) continue;
      if (rom.tools.desired[list[i].id]) owners.push(list[i]);
    }
    if (extraOwners && extraOwners.length) owners = owners.concat(extraOwners);
    return findRegionConflicts(owners, { includeExclusive: true });
  }

  function assertDesiredCompatible(rom, extraOwners) {
    var conflicts = desiredRegionConflicts(rom, extraOwners);
    if (conflicts.length) {
      throw new Error('Selected patches cannot be enabled together:\n  ' +
        conflicts.map(describeConflict).join('\n  '));
    }
  }

  function writesMatch(z64, writes, key) {
    for (var i = 0; i < writes.length; i++) {
      var bytes = key === 'patched' ? patchedBytes(writes[i]) : originalBytes(writes[i]);
      if (!regionEquals(z64, writes[i].offset, bytes)) return false;
    }
    return true;
  }

  // Older shipped build whose bytes fully match the ROM, if any.
  function matchedSuperseded(z64, feature) {
    var list = feature.superseded || [];
    for (var i = 0; i < list.length; i++) {
      if (writesMatch(z64, list[i].writes, 'patched')) return list[i];
    }
    return null;
  }

  // 'applied'  - every write region holds the current patched bytes
  // 'clean'    - every write region holds the original retail bytes
  // 'outdated' - the regions hold a known older build of this feature
  // 'foreign'  - anything else (partial apply, other mod, unknown build)
  function featureState(z64, feature) {
    if (writesMatch(z64, feature.writes, 'patched')) return 'applied';
    if (writesMatch(z64, feature.writes, 'original')) return 'clean';
    if (matchedSuperseded(z64, feature)) return 'outdated';
    return 'foreign';
  }

  function features() {
    return OB64.TOOLS_FEATURES || [];
  }

  function getFeature(id) {
    var list = features();
    for (var i = 0; i < list.length; i++) {
      if (list[i].id === id) return list[i];
    }
    return null;
  }

  // Call once after OB64.loadROM(). Stores per-feature state on the rom:
  //   rom.tools.initial[id]  - state detected in the loaded ROM
  //   rom.tools.desired[id]  - the toggle; starts on for applied AND
  //                            outdated (an outdated feature upgrades on the
  //                            next export unless the user switches it off)
  function initState(rom) {
    rom.tools = { initial: {}, desired: {}, unsupportedReasons: {} };
    if (rom.layout && rom.layout.supportsTools === false) {
      rom.tools.disabledReason = rom.layout.unsupportedFeaturesReason || 'Tools are not available for this ROM revision.';
      var disabledList = features();
      for (var di = 0; di < disabledList.length; di++) {
        rom.tools.initial[disabledList[di].id] = 'unsupported';
        rom.tools.desired[disabledList[di].id] = false;
        rom.tools.unsupportedReasons[disabledList[di].id] = rom.tools.disabledReason;
      }
      return;
    }
    validateFeatureRegistry(rom.z64);
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      var unsupportedReason = featureUnsupportedReason(rom, f);
      if (unsupportedReason) {
        rom.tools.initial[f.id] = 'unsupported';
        rom.tools.desired[f.id] = false;
        rom.tools.unsupportedReasons[f.id] = unsupportedReason;
        continue;
      }
      var st = featureState(rom.z64, f);
      rom.tools.initial[f.id] = st;
      rom.tools.desired[f.id] = (st === 'applied' || st === 'outdated');
    }
  }

  // Number of features whose toggle differs from what the z64 currently
  // holds. An outdated feature with the toggle on counts as pending (the
  // upgrade itself is the change).
  function pendingChanges(rom) {
    if (!rom.tools) return 0;
    if (rom.tools.disabledReason) return 0;
    var n = 0;
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      if (!featureSupported(rom, f)) continue;
      var cur = featureState(rom.z64, f);
      if (cur === 'foreign') continue;
      var want = !!rom.tools.desired[f.id];
      if (cur === 'outdated' || want !== (cur === 'applied')) n++;
    }
    return n;
  }

  function restoreWrites(z64, writes) {
    for (var i = 0; i < writes.length; i++) {
      writeRegion(z64, writes[i].offset, originalBytes(writes[i]));
    }
  }

  // Write every pending toggle into rom.z64. Returns
  // { applied: [names], upgraded: [names], removed: [names], skipped: [names],
  //   crc: bool }. crc is true when any write touched the CIC-6102 CRC
  // window, in which case the caller must run OB64.recalcN64CRC before
  // exporting.
  function applyDesired(rom) {
    var res = { applied: [], upgraded: [], removed: [], skipped: [], crc: false };
    if (!rom.tools) return res;
    if (rom.tools.disabledReason) return res;
    assertDesiredCompatible(rom);
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      if (!featureSupported(rom, f)) {
        if (rom.tools.desired[f.id]) res.skipped.push(f.name);
        continue;
      }
      var cur = featureState(rom.z64, f);
      var want = !!rom.tools.desired[f.id];
      if (cur === 'foreign') {
        if (want !== (rom.tools.initial[f.id] === 'applied')) res.skipped.push(f.name);
        continue;
      }
      var old = cur === 'outdated' ? matchedSuperseded(rom.z64, f) : null;
      if (want && (cur === 'clean' || cur === 'outdated')) {
        if (old) restoreWrites(rom.z64, old.writes);
        for (var wi = 0; wi < f.writes.length; wi++) {
          writeRegion(rom.z64, f.writes[wi].offset, patchedBytes(f.writes[wi]));
        }
        (old ? res.upgraded : res.applied).push(f.name);
        if (f.crcWindow) res.crc = true;
      } else if (!want && (cur === 'applied' || cur === 'outdated')) {
        if (old) restoreWrites(rom.z64, old.writes);
        else restoreWrites(rom.z64, f.writes);
        res.removed.push(f.name);
        if (f.crcWindow) res.crc = true;
      }
    }
    return res;
  }

  OB64.tools = {
    features: features,
    getFeature: getFeature,
    featureState: featureState,
    initState: initState,
    pendingChanges: pendingChanges,
    applyDesired: applyDesired,
    featureRegions: featureRegions,
    findRegionConflicts: findRegionConflicts,
    desiredRegionConflicts: desiredRegionConflicts,
    assertDesiredCompatible: assertDesiredCompatible,
    validateFeatureRegistry: validateFeatureRegistry,
    featureUnsupportedReason: featureUnsupportedReason,
    featureSupported: featureSupported,
  };

})();
