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

  // 'applied'  - every write region holds the patched bytes
  // 'clean'    - every write region holds the original retail bytes
  // 'foreign'  - anything else (partial apply, other mod, different build)
  function featureState(z64, feature) {
    var allPatched = true;
    var allClean = true;
    for (var i = 0; i < feature.writes.length; i++) {
      var w = feature.writes[i];
      if (!regionEquals(z64, w.offset, patchedBytes(w))) allPatched = false;
      if (!regionEquals(z64, w.offset, originalBytes(w))) allClean = false;
      if (!allPatched && !allClean) return 'foreign';
    }
    if (allPatched) return 'applied';
    if (allClean) return 'clean';
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
  //   rom.tools.desired[id]  - the toggle; starts as "already applied"
  function initState(rom) {
    rom.tools = { initial: {}, desired: {} };
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      var st = featureState(rom.z64, f);
      rom.tools.initial[f.id] = st;
      rom.tools.desired[f.id] = (st === 'applied');
    }
  }

  // Number of features whose toggle differs from what the z64 currently holds.
  function pendingChanges(rom) {
    if (!rom.tools) return 0;
    var n = 0;
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      var cur = featureState(rom.z64, f);
      if (cur === 'foreign') continue;
      var want = !!rom.tools.desired[f.id];
      if (want !== (cur === 'applied')) n++;
    }
    return n;
  }

  // Write every pending toggle into rom.z64. Returns
  // { applied: [names], removed: [names], skipped: [names], crc: bool }.
  // crc is true when any write touched the CIC-6102 CRC window, in which case
  // the caller must run OB64.recalcN64CRC before exporting.
  function applyDesired(rom) {
    var res = { applied: [], removed: [], skipped: [], crc: false };
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
      if (want && cur === 'clean') {
        for (var wi = 0; wi < f.writes.length; wi++) {
          writeRegion(rom.z64, f.writes[wi].offset, patchedBytes(f.writes[wi]));
        }
        res.applied.push(f.name);
        if (f.crcWindow) res.crc = true;
      } else if (!want && cur === 'applied') {
        for (var wj = 0; wj < f.writes.length; wj++) {
          writeRegion(rom.z64, f.writes[wj].offset, originalBytes(f.writes[wj]));
        }
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
