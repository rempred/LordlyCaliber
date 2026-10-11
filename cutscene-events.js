// Lordly Caliber - Cutscene event launch-site bindings.
//
// A parent event row (custom-LZ, event directory at z64 0x009398E8) launches a
// Director scene with `68 NN 88 HH 10 00` (long form) or `68 NN 10 00` (short
// form). A binding makes one exact launch site start another scene: a new
// (authored) scene or a retail scene's own selector row.
//
// Rows are never edited in place. Every export rebuilds each bound row from its
// retail preimage, verified against the pinned table in
// cutscene-event-rows-data.js, rewrites the bound selector bytes, and relocates
// the result into the Cutscene arena as a kind-2 record. A row whose last
// binding is removed gets its retail directory key back. ROM-only reopen reads
// the bindings back by diffing each relocated row against its retail preimage.
//
// A short site (`68 NN 10 00`) holds only an 8-bit selector. It is redirected
// without moving any entry cursor: its four bytes become a long jump
// `68 outLo 02 outHi` to a ten-byte stub appended after the retail program,
// `68 selLo 88 selHi 10 op 68 retLo 02 retHi`, which launches the new selector
// and jumps back to the instruction after the site. Stubs are appended in
// launch-site ID order. Every one of the 402 retail short sites was audited
// for this rewrite (tests/cutscene-event-stub-audit.test.js).

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var SHARED_ROLE = /shared/i;
  var STUB_BYTES = 10;
  var siteCache = typeof WeakMap === 'function' ? new WeakMap() : null;

  function CutsceneEventError(message, code) {
    this.name = 'CutsceneEventError';
    this.message = message;
    this.code = code || 'event-binding';
  }
  CutsceneEventError.prototype = Object.create(Error.prototype);
  CutsceneEventError.prototype.constructor = CutsceneEventError;
  function fail(message, code) { throw new CutsceneEventError(message, code); }

  function readU32(input, offset) {
    return ((input[offset] << 24) | (input[offset + 1] << 16) |
      (input[offset + 2] << 8) | input[offset + 3]) >>> 0;
  }

  function stable(value) { return OB64.cutsceneModel.stableStringify(value, 0); }

  function data() {
    var pinned = OB64.cutsceneEventRows;
    if (!pinned || pinned.format !== 'ob64-cutscene-event-rows' || pinned.schemaVersion !== 1 ||
        pinned.sourceRevision !== 'us-rev0' || !Array.isArray(pinned.rows) || pinned.rows.length !== pinned.rowCount) {
      fail('This editor build has no pinned retail event rows.', 'event-data');
    }
    return pinned;
  }

  function directoryWordZ64(row) { return data().directoryZ64 + 4 + row * 4; }

  function rowProvenance(row) {
    var provenance = OB64.cutsceneProvenance && OB64.cutsceneProvenance.rows;
    return provenance && provenance[row] || null;
  }

  // Every catalogued retail launch site, keyed by its launch ID, with its form
  // from the pinned table. IDs name the retail layout forever.
  function siteIndex(catalog) {
    var cached = siteCache && siteCache.get(catalog);
    if (cached) return cached;
    var rows = data().rows, byId = Object.create(null), list = [];
    catalog.directorScenes.forEach(function(scene) {
      if (scene.authored || !scene.launchProfile) return;
      (scene.launchProfile.parentEventLaunches || []).forEach(function(launch) {
        var pinned = rows[launch.eventDirectoryRow];
        if (!pinned || pinned.key !== parseInt(launch.eventResourceKey, 16)) {
          fail('The catalog and the pinned event rows disagree about row ' + launch.eventDirectoryRow + '.', 'event-data');
        }
        if (byId[launch.launchId]) fail('Launch site ' + launch.launchId + ' is catalogued twice.', 'event-data');
        var provenance = rowProvenance(launch.eventDirectoryRow);
        var role = provenance && provenance.role || null;
        var site = {
          launchSiteId: launch.launchId,
          row: launch.eventDirectoryRow,
          offset: launch.decodedByteOffset,
          selector: launch.directorSelector,
          form: pinned.shortSites.indexOf(launch.decodedByteOffset) >= 0 ? 'short' : 'long',
          retailSceneId: scene.sceneId,
          retailKey: pinned.key,
          entryCursor: launch.eventEntryCursor,
          role: role,
          mission: provenance && provenance.missionOrScenario || null,
          shared: !!(role && SHARED_ROLE.test(role))
        };
        byId[site.launchSiteId] = site;
        list.push(site);
      });
    });
    list.sort(function(left, right) { return left.row - right.row || left.offset - right.offset; });
    var index = { byId: byId, list: list };
    if (siteCache) siteCache.set(catalog, index);
    return index;
  }

  function site(catalog, launchSiteId) {
    return siteIndex(catalog).byId[launchSiteId] || null;
  }

  function cleanTarget(target, label) {
    if (!target || typeof target !== 'object') fail(label + ' has no target.');
    if (target.kind === 'authored') {
      if (Object.keys(target).sort().join() !== 'authoredId,kind' || !/^[0-9a-f]{32}$/.test(target.authoredId)) {
        fail(label + ' has an invalid new-scene target.');
      }
      return { kind: 'authored', authoredId: target.authoredId };
    }
    if (target.kind === 'physical') {
      if (Object.keys(target).sort().join() !== 'kind,sceneId,selectorRow' || typeof target.sceneId !== 'string' ||
          !Number.isInteger(target.selectorRow)) {
        fail(label + ' has an invalid retail-scene target.');
      }
      return { kind: 'physical', sceneId: target.sceneId, selectorRow: target.selectorRow };
    }
    fail(label + ' has an unknown target kind.');
  }

  // The selector row a target launches. Authored rows come from the catalog, or
  // from `resolveAuthored` while a Project is being prepared.
  function targetSelector(catalog, target, options, label) {
    if (target.kind === 'authored') {
      var row = options && options.resolveAuthored ? options.resolveAuthored(target.authoredId) : null;
      if (!Number.isInteger(row)) {
        var scene = catalog.getScene('scene:authored:' + target.authoredId);
        row = scene && scene.authored ? scene.authored.selectorRow : null;
      }
      if (!Number.isInteger(row)) fail(label + ' targets a new scene that is not open.', 'event-target');
      return row;
    }
    var physical = catalog.getScene(target.sceneId);
    if (!physical || physical.authored || physical.engine !== 'director' || physical.sceneId !== target.sceneId) {
      fail(label + ' targets a scene that is not a catalogued retail Director scene.', 'event-target');
    }
    if ((physical.source.directorSelectorRows || []).indexOf(target.selectorRow) < 0) {
      fail(label + ' names selector row ' + target.selectorRow + ', which does not launch that scene.', 'event-target');
    }
    return target.selectorRow;
  }

  // Validate and order a binding set. Each site is bound at most once, is a
  // long-form site in this increment, and must change what it launches.
  function normalize(catalog, bindings, options) {
    if (!Array.isArray(bindings)) fail('Event bindings must be a list.');
    var seen = Object.create(null);
    return bindings.map(function(binding, index) {
      var label = 'Event binding ' + index;
      if (!binding || typeof binding !== 'object' || Object.keys(binding).sort().join() !== 'launchSiteId,target') {
        fail(label + ' must have exactly a launch site and a target.');
      }
      var found = site(catalog, binding.launchSiteId);
      if (!found) fail(label + ' names launch site ' + binding.launchSiteId + ', which is not in the retail catalog.', 'event-site');
      label = 'Launch site ' + found.launchSiteId;
      if (seen[found.launchSiteId]) fail(label + ' is bound twice.', 'event-site');
      seen[found.launchSiteId] = true;
      var target = cleanTarget(binding.target, label);
      var selector = targetSelector(catalog, target, options, label);
      if (selector === found.selector) fail(label + ' already launches that scene.', 'event-target');
      return {
        launchSiteId: found.launchSiteId, target: target, row: found.row, offset: found.offset,
        form: found.form, selector: selector, retailSelector: found.selector
      };
    }).sort(function(left, right) { return left.row - right.row || left.offset - right.offset; });
  }

  function publicBindings(bindings) {
    return bindings.map(function(binding) {
      return { launchSiteId: binding.launchSiteId, target: cleanTarget(binding.target, binding.launchSiteId) };
    });
  }

  function hashUpper(bytes, hashBytes) {
    return OB64.cutsceneCodec.hashBytes(bytes, hashBytes).then(function(hash) { return String(hash).toUpperCase(); });
  }

  // The retail preimage of one row, read from its retail key (never from the
  // current directory) and verified against the pinned sizes and hash.
  function retailRow(z64, row, options) {
    return Promise.resolve().then(function() {
      var pinned = data().rows[row];
      if (!pinned) fail('Event row ' + row + ' has no retail program.', 'event-preimage');
      var prefix = data().resourceBaseZ64 + pinned.key;
      if (readU32(z64, prefix) !== pinned.storedLength) {
        fail('Retail event row ' + row + ' has been changed in this ROM.', 'event-preimage');
      }
      var decoded = OB64.cutsceneCodec.decodeCustomLz(
        z64.slice(prefix + 4, prefix + 4 + pinned.storedLength), { requireExact: true }).bytes;
      if (decoded.length !== pinned.decodedLength) fail('Retail event row ' + row + ' has been changed in this ROM.', 'event-preimage');
      return hashUpper(decoded, options && options.hashBytes).then(function(hash) {
        if (hash !== pinned.decodedSha256) fail('Retail event row ' + row + ' has been changed in this ROM.', 'event-preimage');
        return decoded;
      });
    });
  }

  // Rewrite each bound site, checking that the retail bytes are exactly the
  // catalogued launch first: long sites in place, short sites through an
  // appended stub.
  function boundBytes(retail, rowBindings) {
    var shorts = rowBindings.filter(function(binding) { return binding.form === 'short'; })
      .sort(function(left, right) { return left.launchSiteId < right.launchSiteId ? -1 : left.launchSiteId > right.launchSiteId ? 1 : 0; });
    if (shorts.length && retail.length % 2) fail('An event program with an odd length cannot take a launch stub.', 'event-site');
    var output = new Uint8Array(retail.length + shorts.length * STUB_BYTES);
    output.set(retail, 0);
    shorts.forEach(function(binding, index) {
      var at = binding.offset, stub = retail.length + index * STUB_BYTES;
      if (retail[at - 2] !== 0x68 || retail[at - 1] !== binding.retailSelector || retail[at] !== 0x10) {
        fail('Launch site ' + binding.launchSiteId + ' does not hold its catalogued launch.', 'event-site');
      }
      // Cursors are 16-bit word indexes; the stub and its return must fit.
      if ((stub + STUB_BYTES) / 2 > 0xFFFF) fail('A launch stub would exceed the event cursor range.', 'event-site');
      var operand = retail[at + 1];
      var out = ((stub - (at + 2)) / 2) & 0xFFFF;
      var back = ((at + 2 - (stub + STUB_BYTES)) / 2) & 0xFFFF;
      output.set([0x68, out & 0xFF, 0x02, out >>> 8], at - 2);
      output.set([0x68, binding.selector & 0xFF, 0x88, binding.selector >>> 8, 0x10, operand,
        0x68, back & 0xFF, 0x02, back >>> 8], stub);
    });
    rowBindings.filter(function(binding) { return binding.form !== 'short'; }).forEach(function(binding) {
      var at = binding.offset, old = binding.retailSelector;
      if (retail[at - 4] !== 0x68 || retail[at - 3] !== (old & 0xFF) || retail[at - 2] !== 0x88 ||
          retail[at - 1] !== (old >>> 8) || retail[at] !== 0x10 || retail[at + 1] !== 0x00) {
        fail('Launch site ' + binding.launchSiteId + ' does not hold its catalogued launch.', 'event-site');
      }
      if (binding.selector < 0 || binding.selector > 0xFFFF) fail('Selector ' + binding.selector + ' does not fit a long launch.', 'event-site');
      output[at - 3] = binding.selector & 0xFF;
      output[at - 1] = binding.selector >>> 8;
    });
    return output;
  }

  // One rebuilt program per bound row, from the retail preimage.
  function buildRows(z64, bindings, options) {
    var rows = [];
    bindings.forEach(function(binding) {
      var entry = rows.find(function(item) { return item.row === binding.row; });
      if (!entry) rows.push(entry = { row: binding.row, bindings: [] });
      entry.bindings.push(binding);
    });
    rows.sort(function(left, right) { return left.row - right.row; });
    var chain = Promise.resolve();
    rows.forEach(function(entry) {
      chain = chain.then(function() {
        return retailRow(z64, entry.row, options).then(function(retail) {
          entry.retailKey = data().rows[entry.row].key;
          entry.decoded = boundBytes(retail, entry.bindings);
        });
      });
    });
    return chain.then(function() { return rows; });
  }

  // Directory rows whose key is not the retail key.
  function changedDirectoryRows(z64) {
    var pinned = data(), changed = [];
    for (var row = 0; row < pinned.rowCount; row++) {
      var retail = pinned.rows[row] ? pinned.rows[row].key : 0;
      var current = readU32(z64, directoryWordZ64(row));
      if (current !== retail) changed.push({ row: row, current: current, retail: retail });
    }
    return changed;
  }

  // Which sites a relocated row rebinds: bytes may differ from retail only at
  // the selector bytes of catalogued long sites in that row.
  function diffRow(catalog, row, retail, bytes) {
    var outside = 'Relocated event row ' + row + ' was changed outside Cutscene Studio.';
    var extra = bytes.length - retail.length;
    if (extra < 0 || extra % STUB_BYTES) fail(outside, 'event-discovery');
    var sites = siteIndex(catalog).list.filter(function(item) { return item.row === row; });
    var allowed = Object.create(null);
    sites.forEach(function(item) {
      if (item.form === 'long') { allowed[item.offset - 3] = item; allowed[item.offset - 1] = item; }
      else { allowed[item.offset - 1] = item; allowed[item.offset] = item; allowed[item.offset + 1] = item; }
    });
    var changed = [];
    for (var at = 0; at < retail.length; at++) {
      if (bytes[at] === retail[at]) continue;
      var owner = allowed[at];
      if (!owner) fail(outside, 'event-discovery');
      if (changed.indexOf(owner) < 0) changed.push(owner);
    }
    var shorts = changed.filter(function(item) { return item.form === 'short'; }).sort(function(left, right) {
      return left.launchSiteId < right.launchSiteId ? -1 : left.launchSiteId > right.launchSiteId ? 1 : 0;
    });
    if (shorts.length * STUB_BYTES !== extra) fail(outside, 'event-discovery');
    var derived = changed.map(function(item) {
      var selector = item.form === 'long'
        ? bytes[item.offset - 3] | (bytes[item.offset - 1] << 8)
        : (function(stub) { return bytes[stub + 1] | (bytes[stub + 3] << 8); })(retail.length + shorts.indexOf(item) * STUB_BYTES);
      return { launchSiteId: item.launchSiteId, offset: item.offset, form: item.form,
        retailSelector: item.selector, selector: selector };
    });
    // The whole program must be exactly what the rebuild produces.
    var expected = boundBytes(retail, derived);
    if (expected.length !== bytes.length || expected.some(function(value, index) { return value !== bytes[index]; })) {
      fail(outside, 'event-discovery');
    }
    return derived.map(function(item) { return { launchSiteId: item.launchSiteId, selector: item.selector }; });
  }

  // The scene a selector row launches: a new scene holding that row, otherwise
  // the retail scene that owns it.
  function resolveTarget(catalog, selector, deletedAuthored) {
    var authored = (catalog.authoredScenes ? catalog.authoredScenes() : []).find(function(scene) {
      return scene.authored.selectorRow === selector;
    });
    if (authored) return { kind: 'authored', authoredId: authored.authored.authoredId };
    // A new scene deleted in this session still holds its row in the ROM.
    var deleted = (deletedAuthored || []).find(function(entry) { return entry.selectorRow === selector; });
    if (deleted) return { kind: 'authored', authoredId: deleted.authoredId };
    var physical = catalog.directorScenes.find(function(scene) {
      return !scene.authored && (scene.source.directorSelectorRows || []).indexOf(selector) >= 0;
    });
    if (!physical) fail('A relocated event row launches selector row ' + selector + ', which no scene owns.', 'event-discovery');
    return { kind: 'physical', sceneId: physical.sceneId, selectorRow: selector };
  }

  // ROM-only reopen. Every changed directory row must be a kind-2 arena record;
  // each record is diffed against its verified retail preimage; the result
  // must equal the manifest's bindings exactly. Authored scenes must already be
  // registered so their rows resolve.
  function discover(z64, catalog, options) {
    var arenaApi = OB64.cutsceneExport && OB64.cutsceneExport.arena;
    if (!arenaApi) return Promise.resolve([]);
    return Promise.resolve().then(function() {
      var changed = changedDirectoryRows(z64);
      var arena = arenaApi.read(z64);
      var records = arena.records.filter(function(row) { return row[0] >>> 28 === arenaApi.KIND.EVENT_ROW; });
      changed.forEach(function(change) {
        if (!records.some(function(row) { return (row[0] & 0x0FFFFFFF) === change.row && row[1] === change.current; })) {
          fail('Event row ' + change.row + ' was changed outside Cutscene Studio.', 'event-discovery');
        }
      });
      var manifestRow = arena.records.find(function(row) { return row[0] >>> 28 === arenaApi.KIND.MANIFEST; });
      var manifest = manifestRow ? arenaApi.readManifest(z64, manifestRow) : null;
      if (!records.length) {
        if (manifest && manifest.eventBindings && manifest.eventBindings.length) {
          fail('The Cutscene manifest lists event bindings that this ROM does not hold.', 'event-discovery');
        }
        return [];
      }
      if (!manifest) fail('Relocated event rows have no Cutscene manifest.', 'event-discovery');
      var found = [], chain = Promise.resolve();
      records.sort(function(left, right) { return (left[0] & 0x0FFFFFFF) - (right[0] & 0x0FFFFFFF); }).forEach(function(record) {
        chain = chain.then(function() {
          var row = record[0] & 0x0FFFFFFF;
          var bytes = OB64.cutsceneCodec.decodeCustomLz(
            z64.slice(record[2] + 4, record[2] + 4 + record[3]), { requireExact: true }).bytes;
          if (bytes.length !== record[5]) fail('Relocated event row ' + row + ' has the wrong decoded length.', 'event-discovery');
          return hashUpper(bytes, options && options.hashBytes).then(function(hash) {
            if (parseInt(hash.slice(0, 8), 16) !== record[6]) fail('Relocated event row ' + row + ' fails its digest.', 'event-discovery');
            return retailRow(z64, row, options);
          }).then(function(retail) {
            var sites = diffRow(catalog, row, retail, bytes);
            if (!sites.length) fail('Relocated event row ' + row + ' carries no binding.', 'event-discovery');
            sites.forEach(function(item) {
              found.push({ launchSiteId: item.launchSiteId,
                target: resolveTarget(catalog, item.selector, options && options.deletedAuthored) });
            });
          });
        });
      });
      return chain.then(function() {
        var deletedRows = {};
        (options && options.deletedAuthored || []).forEach(function(entry) { deletedRows[entry.authoredId] = entry.selectorRow; });
        var derived = publicBindings(normalize(catalog, found, {
          resolveAuthored: function(authoredId) { return deletedRows[authoredId] !== undefined ? deletedRows[authoredId] : null; }
        }));
        if (stable(derived) !== stable(manifest.eventBindings || [])) {
          fail('The Cutscene manifest does not match the event rows in this ROM.', 'event-discovery');
        }
        return derived;
      });
    });
  }

  // Sites whose retail launch is this scene, and sites bound to it.
  function sitesForScene(catalog, scene) {
    return siteIndex(catalog).list.filter(function(item) { return item.retailSceneId === scene.sceneId; });
  }

  function bindingsForScene(catalog, bindings, scene) {
    return (bindings || []).filter(function(binding) {
      var target = binding.target;
      return scene.authored
        ? target.kind === 'authored' && target.authoredId === scene.authored.authoredId
        : target.kind === 'physical' && target.sceneId === scene.sceneId;
    });
  }

  // Launch sites matching a text query (row number, site ID, mission or retail
  // scene name), long sites first within each row.
  function searchSites(catalog, query, limit) {
    var text = String(query || '').trim().toLowerCase();
    var rowOnly = /^\d+$/.test(text) ? Number(text) : null;
    var results = siteIndex(catalog).list.filter(function(item) {
      if (!text) return true;
      if (rowOnly !== null) return item.row === rowOnly;
      var scene = catalog.getScene(item.retailSceneId);
      var name = scene ? OB64.cutsceneCatalog.displayName(scene) : '';
      return item.launchSiteId.toLowerCase().indexOf(text) >= 0 ||
        String(item.mission || '').toLowerCase().indexOf(text) >= 0 ||
        name.toLowerCase().indexOf(text) >= 0;
    });
    return results.slice(0, limit || 50);
  }

  OB64.cutsceneEvents = {
    CutsceneEventError: CutsceneEventError,
    directoryWordZ64: directoryWordZ64,
    siteIndex: siteIndex,
    site: site,
    normalize: normalize,
    publicBindings: publicBindings,
    retailRow: retailRow,
    boundBytes: boundBytes,
    buildRows: buildRows,
    stubBytes: STUB_BYTES,
    changedDirectoryRows: changedDirectoryRows,
    diffRow: diffRow,
    resolveTarget: resolveTarget,
    discover: discover,
    sitesForScene: sitesForScene,
    bindingsForScene: bindingsForScene,
    searchSites: searchSites
  };
})(window.OB64);
