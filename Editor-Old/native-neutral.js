// Lordly Caliber Editor — Rebuild ROM native Neutral encounter owner.
//
// Rebuild ROM writes the source-built Neutral module (OB64 Decomp
// src/lordly/neutral) instead of the Patch ROM shared runtime. The module is
// loaded once at boot and every rebuilt retail site calls it directly. This
// file composes the module image with the Editor's neutral data tables,
// applies the generated boot stub and site words, and reads them back.
window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var OWNER_ID = 'runtime.neutral-native';
  var SCHEMA = 'ob64-native-neutral-data';

  function fail(message) {
    throw new Error('Native Neutral encounters: ' + message);
  }

  function hexBytes(hex) {
    var out = new Uint8Array(hex.length / 2);
    for (var i = 0; i < out.length; i++) out[i] = parseInt(hex.substr(i * 2, 2), 16);
    return out;
  }

  function wordBytes(words) {
    var out = new Uint8Array(words.length * 4);
    words.forEach(function(word, index) {
      out[index * 4] = (word >>> 24) & 0xFF;
      out[index * 4 + 1] = (word >>> 16) & 0xFF;
      out[index * 4 + 2] = (word >>> 8) & 0xFF;
      out[index * 4 + 3] = word & 0xFF;
    });
    return out;
  }

  function equalAt(bytes, offset, expected) {
    for (var i = 0; i < expected.length; i++) {
      if (bytes[offset + i] !== expected[i]) return false;
    }
    return true;
  }

  function data() {
    var value = OB64.NATIVE_NEUTRAL_DATA;
    if (!value || value.schema !== SCHEMA || value.schemaVersion !== 1) {
      fail('the generated native Neutral data is missing. Reload the editor.');
    }
    return value;
  }

  // Every region this owner writes. The authorization ledger requires the
  // exported owner to match this layout exactly.
  function components() {
    var d = data();
    var layout = d.layout;
    var list = [
      { name: 'Neutral module image', romStart: layout.romBase, ramStart: layout.ramBase,
        bytes: layout.imageBytes },
      { name: 'boot load stub', romStart: layout.bootStubRom, ramStart: layout.bootStubRam,
        bytes: d.bootStub.bytes },
    ];
    d.sites.forEach(function(site) {
      list.push({ name: 'rebuilt ' + site.id + ' site', romStart: site.rom, bytes: site.words.length * 4 });
    });
    return list;
  }

  function ownerRegions() {
    var regions = [];
    components().forEach(function(component) {
      regions.push({ kind: 'rom', start: component.romStart, size: component.bytes, label: component.name });
      if (Number.isInteger(component.ramStart)) {
        regions.push({ kind: 'ram', start: component.ramStart, size: component.bytes,
          label: component.name + ' runtime' });
      }
    });
    return regions;
  }

  function collect(rom) {
    var runtime = OB64.runtimeOverrides;
    return {
      rates: runtime ? runtime.collectScenarioRateOverrides(rom) : [],
      drops: runtime ? runtime.collectWeightedDropOverrides(rom) : [],
      squads: runtime ? runtime.collectCustomNeutralSquads(rom) : [],
    };
  }

  function isActive(rom) {
    var c = collect(rom);
    return !!(c.rates.length || c.drops.length || c.squads.length);
  }

  function buildImage(rom) {
    var d = data();
    var layout = d.layout;
    var runtime = OB64.runtimeOverrides;
    var c = collect(rom);
    var image = new Uint8Array(layout.imageBytes);
    var code = hexBytes(d.module.hex);
    if (code.length > layout.codeLimit) fail('module code exceeds its code region');
    image.set(code, 0);
    var rateTable = runtime.buildRateTable(c.rates);
    var dropTable = runtime.buildDropTable(c.drops);
    var profileTable = runtime.buildTypedProfileTable(c.squads, { MOD_BASE: layout.ramBase }).bytes;
    var pieces = [
      { off: layout.profileOffset, bytes: profileTable, end: layout.rateOffset - 0x100, label: 'Neutral Squad profiles' },
      { off: layout.rateOffset, bytes: rateTable, end: layout.dropOffset, label: 'per-scenario rates' },
      { off: layout.dropOffset, bytes: dropTable, end: layout.scratchOffset, label: 'weighted creature rewards' },
    ];
    pieces.forEach(function(piece) {
      if (piece.off + piece.bytes.length > piece.end) fail(piece.label + ' exceed their module allocation');
      image.set(piece.bytes, piece.off);
    });
    return { image: image, counts: { rates: c.rates.length, drops: c.drops.length, squads: c.squads.length } };
  }

  function prepare(rom, z64) {
    var d = data();
    var layout = d.layout;
    var region = z64.subarray(layout.romBase, layout.romBase + layout.imageBytes);
    for (var i = 0; i < region.length; i++) {
      if (region[i] !== 0xFF) fail('the module ROM region is not retail fill');
    }
    var boot = hexBytes(d.bootStub.hex);
    for (var j = 0; j < boot.length; j++) {
      if (z64[layout.bootStubRom + j] !== 0) fail('the boot load cave is not retail zero fill');
    }
    d.sites.forEach(function(site) {
      if (!equalAt(z64, site.rom, wordBytes(site.retail))) {
        fail('the retail ' + site.id + ' site at z64 0x' + site.rom.toString(16) + ' is not retail');
      }
    });
    var built = buildImage(rom);
    var writes = [
      { offset: layout.romBase, bytes: built.image, label: 'native Neutral module' },
      { offset: layout.bootStubRom, bytes: boot, label: 'native Neutral boot load' },
    ].concat(d.sites.map(function(site) {
      return { offset: site.rom, bytes: wordBytes(site.words), label: 'rebuilt ' + site.id + ' site' };
    }));
    return {
      writes: writes,
      counts: built.counts,
      owner: { id: OWNER_ID, name: 'Native Neutral Encounters', category: 'nativeRebuild',
        regions: ownerRegions() },
      crc: true,
    };
  }

  function apply(candidate, plan) {
    plan.writes.forEach(function(write) { candidate.z64.set(write.bytes, write.offset); });
  }

  function validateApplied(candidate, plan) {
    plan.writes.forEach(function(write) {
      if (!equalAt(candidate.z64, write.offset, write.bytes)) {
        fail(write.label + ' did not read back from the finished ROM');
      }
    });
    return { summary: 'Native Neutral module, boot load, and ' + (plan.writes.length - 2) +
      ' rebuilt sites read back correctly.' };
  }

  OB64.nativeNeutral = {
    OWNER_ID: OWNER_ID,
    isActive: isActive,
    ownerRegions: ownerRegions,
    prepare: prepare,
    apply: apply,
    validateApplied: validateApplied,
  };
})(window.OB64);
