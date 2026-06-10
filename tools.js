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

  function regionEquals(z64, offset, bytes) {
    for (var i = 0; i < bytes.length; i++) {
      if (z64[offset + i] !== bytes[i]) return false;
    }
    return true;
  }

  function writeRegion(z64, offset, bytes) {
    z64.set(bytes, offset);
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
    rom.tools = { initial: {}, desired: {} };
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
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
    var n = 0;
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
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
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
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
  };

})();
