// OB64 Mod Editor — Patch format (JSON save/load of user edits)
//
// A "patch" is a JSON document capturing the user's changes relative to the
// pristine ROM state that was loaded. It's small, human-readable, and portable
// — the same patch can be applied to any compatible ROM to reproduce the
// edits without re-exporting a full v64.
//
// See docs/editor.md "JSON Patch Format" for the schema.
//
// v1 scope: shops (full replacement of each shop's item list) + item_prices
// (prices edited via the shop modal's inline editor). Other tabs are stubbed
// out in the patches object but not yet wired up.

window.OB64 = window.OB64 || {};

(function() {
  var PATCH_FORMAT = 'ob64-patch';
  var PATCH_VERSION = 1;

  // --------------------------------------------------------------
  // Snapshot pristine state — call once, right after OB64.loadROM().
  // Every subsequent edit writes through to rom.shops / rom.itemStats;
  // collectPatch() diffs against rom.original.
  // --------------------------------------------------------------
  function snapshotOriginal(rom) {
    rom.original = rom.original || {};
    rom.original.shops = rom.shops.map(function(s) {
      return { items: s.items.slice() };
    });
    rom.original.itemPrices = {};
    for (var i = 0; i < rom.itemStats.length; i++) {
      var stat = rom.itemStats[i];
      if (stat && typeof stat.price === 'number') {
        rom.original.itemPrices[i] = stat.price;
      }
    }
  }

  // --------------------------------------------------------------
  // collectPatch(rom) → JSON object matching the schema in docs/editor.md.
  // Returns { patches: {...}, summary: {...}, ... } with only the fields
  // that differ from rom.original. Never throws.
  // --------------------------------------------------------------
  function collectPatch(rom) {
    if (!rom.original) throw new Error('OB64.snapshotOriginal() was not called on this rom');

    var shopsOut = {};
    for (var i = 0; i < rom.shops.length; i++) {
      var a = rom.shops[i].items;
      var b = rom.original.shops[i].items;
      if (!arraysEqual(a, b)) {
        shopsOut[String(i)] = { items: a.slice() };
      }
    }

    var pricesOut = {};
    for (var id = 0; id < rom.itemStats.length; id++) {
      var stat = rom.itemStats[id];
      if (!stat || typeof stat.price !== 'number') continue;
      var orig = rom.original.itemPrices[id];
      if (typeof orig === 'number' && stat.price !== orig) {
        pricesOut[String(id)] = stat.price;
      }
    }

    return {
      format: PATCH_FORMAT,
      version: PATCH_VERSION,
      created_at: new Date().toISOString(),
      editor_version: '2026-04-19',
      rom_hint: {
        archives_count: rom.archives ? rom.archives.length : null,
        shop_count:     rom.shops ? rom.shops.length : null,
      },
      summary: {
        shops_modified:        Object.keys(shopsOut).length,
        item_prices_modified:  Object.keys(pricesOut).length,
      },
      patches: {
        shops:        shopsOut,
        item_prices:  pricesOut,
        // Reserved for future tabs — not yet wired
        enemies:      {},
        items:        {},
        classDefs:    {},
      },
    };
  }

  // --------------------------------------------------------------
  // applyPatch(rom, patch, dirtyFlags) → in-place mutation of rom.
  // Marks dirtyFlags so the Export pipeline knows which archives to rewrite.
  // Returns { applied: {shops, prices}, warnings: [...] }.
  // Throws PatchFormatError on invalid format.
  // --------------------------------------------------------------
  function PatchFormatError(msg) { this.name = 'PatchFormatError'; this.message = msg; }
  PatchFormatError.prototype = new Error();

  function applyPatch(rom, patch, dirtyFlags) {
    if (!patch || patch.format !== PATCH_FORMAT) {
      throw new PatchFormatError('Not an ob64-patch file (format field missing or wrong)');
    }
    if (!Number.isInteger(patch.version) || patch.version > PATCH_VERSION) {
      throw new PatchFormatError('Patch version ' + patch.version +
        ' is newer than this editor understands (' + PATCH_VERSION + '). Update the editor.');
    }

    var warnings = [];
    if (patch.rom_hint && patch.rom_hint.archives_count &&
        patch.rom_hint.archives_count !== rom.archives.length) {
      warnings.push('ROM archive count differs from patch (patch: ' +
        patch.rom_hint.archives_count + ', rom: ' + rom.archives.length +
        '). Applying anyway.');
    }
    if (patch.rom_hint && patch.rom_hint.shop_count &&
        patch.rom_hint.shop_count !== rom.shops.length) {
      warnings.push('ROM shop count differs from patch (patch: ' +
        patch.rom_hint.shop_count + ', rom: ' + rom.shops.length + ').');
    }

    var shopsApplied = 0;
    var pricesApplied = 0;
    var p = patch.patches || {};

    // Shops
    var shopsPatch = p.shops || {};
    for (var k in shopsPatch) {
      var idx = parseInt(k, 10);
      if (!isFinite(idx) || idx < 0 || idx >= rom.shops.length) {
        warnings.push('Patch references shop #' + k + ' but ROM only has ' + rom.shops.length + ' shops — skipping.');
        continue;
      }
      var entry = shopsPatch[k];
      if (!entry || !Array.isArray(entry.items)) continue;
      rom.shops[idx].items = entry.items.slice();
      shopsApplied++;
    }
    if (shopsApplied > 0) dirtyFlags.shops = true;

    // Item prices
    var pricesPatch = p.item_prices || {};
    for (var ks in pricesPatch) {
      var id = parseInt(ks, 10);
      if (!isFinite(id) || id < 0 || id >= rom.itemStats.length) continue;
      var stat = rom.itemStats[id];
      if (!stat) continue;
      var price = pricesPatch[ks];
      if (typeof price !== 'number' || price < 0 || price > 65535) continue;
      stat.price = price;
      pricesApplied++;
    }
    if (pricesApplied > 0) dirtyFlags.items = true;

    return {
      applied: { shops: shopsApplied, prices: pricesApplied },
      warnings: warnings,
    };
  }

  // --------------------------------------------------------------
  // downloadPatch(patch, [filename]) → triggers browser download of JSON.
  // --------------------------------------------------------------
  function downloadPatch(patch, filename) {
    if (!filename) {
      var ts = (patch.created_at || new Date().toISOString()).replace(/[:.]/g, '-');
      filename = 'ob64_patch_' + ts + '.json';
    }
    var blob = new Blob([JSON.stringify(patch, null, 2)], { type: 'application/json' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
  }

  // --------------------------------------------------------------
  // parsePatchFile(fileText) → validates + returns parsed patch object.
  // Throws on invalid JSON or wrong format.
  // --------------------------------------------------------------
  function parsePatchFile(fileText) {
    var parsed;
    try {
      parsed = JSON.parse(fileText);
    } catch (e) {
      throw new PatchFormatError('File is not valid JSON: ' + e.message);
    }
    if (!parsed || typeof parsed !== 'object') {
      throw new PatchFormatError('Patch file is not a JSON object');
    }
    if (parsed.format !== PATCH_FORMAT) {
      throw new PatchFormatError('File is not an ob64-patch (format="' +
        parsed.format + '", expected "' + PATCH_FORMAT + '")');
    }
    return parsed;
  }

  // --------------------------------------------------------------
  // Helpers
  // --------------------------------------------------------------
  function arraysEqual(a, b) {
    if (a === b) return true;
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  // --------------------------------------------------------------
  // Exports
  // --------------------------------------------------------------
  OB64.patch = {
    FORMAT:         PATCH_FORMAT,
    VERSION:        PATCH_VERSION,
    snapshotOriginal: snapshotOriginal,
    collectPatch:     collectPatch,
    applyPatch:       applyPatch,
    downloadPatch:    downloadPatch,
    parsePatchFile:   parsePatchFile,
    PatchFormatError: PatchFormatError,
  };
})();
