// Lordly Caliber - Cutscene Studio Project serialization and guarded import.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var FORMAT = 'ob64-cutscene-project';
  // Schema 1 holds edited retail scenes only. Schema 2 adds authored scenes and
  // is written only when a Project has them, so older Projects stay unchanged.
  var SCHEMA_VERSION = 2;
  var RETAIL_ONLY_SCHEMA = 1;

  function CutsceneProjectError(message) {
    this.name = 'CutsceneProjectError';
    this.message = message;
  }
  CutsceneProjectError.prototype = Object.create(Error.prototype);
  CutsceneProjectError.prototype.constructor = CutsceneProjectError;

  function fail(message) { throw new CutsceneProjectError(message); }

  function isObject(value) {
    return !!value && Object.prototype.toString.call(value) === '[object Object]';
  }

  function onlyFields(value, allowed, label) {
    Object.keys(value).forEach(function(key) {
      if (allowed.indexOf(key) === -1) fail(label + ' contains unsupported field "' + key + '".');
    });
  }

  function string(value, label) {
    if (typeof value !== 'string' || !value.length) fail(label + ' must be a non-empty string.');
  }

  function integer(value, label, minimum, maximum) {
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
      fail(label + ' must be an integer from ' + minimum + ' through ' + maximum + '.');
    }
    return value;
  }

  function cleanView(view, scene, document) {
    view = view || {};
    var branchIds = {};
    document.branches.forEach(function(branch) { branchIds[branch.id] = true; });
    var actorIds = {};
    document.actors.forEach(function(actor) { actorIds[actor.id] = true; });
    var pathId = typeof view.pathId === 'string' && branchIds[view.pathId]
      ? view.pathId : document.branches[0].id;
    var selectedActorId = typeof view.selectedActorId === 'string' && actorIds[view.selectedActorId]
      ? view.selectedActorId : null;
    return {
      frame: Number.isInteger(view.frame) && view.frame >= 0 ? view.frame : 0,
      pathId: pathId,
      selectedActorId: selectedActorId,
      loop: view.loop === true,
      snap: ['frame', 'tenth', 'half', 'second'].indexOf(view.snap) !== -1
        ? view.snap : 'frame',
      zoom: typeof view.zoom === 'number' && Number.isFinite(view.zoom) && view.zoom > 0
        ? Math.max(0.25, Math.min(8, view.zoom)) : 1,
      selectedClipId: typeof view.selectedClipId === 'string' &&
        document.tracks.some(function(track) {
          return track.clips.some(function(clip) { return clip.id === view.selectedClipId; });
        }) ? view.selectedClipId : null
    };
  }

  function validateView(value, scene, document, label) {
    if (!isObject(value)) fail(label + ' must be an object.');
    onlyFields(value, [
      'frame', 'pathId', 'selectedActorId', 'loop', 'snap', 'zoom', 'selectedClipId'
    ], label);
    integer(value.frame, label + '.frame', 0, 0x7FFFFFFF);
    string(value.pathId, label + '.pathId');
    if (value.selectedActorId !== null) string(value.selectedActorId, label + '.selectedActorId');
    if (value.selectedClipId !== null) string(value.selectedClipId, label + '.selectedClipId');
    if (typeof value.loop !== 'boolean') fail(label + '.loop must be a boolean.');
    if (['frame', 'tenth', 'half', 'second'].indexOf(value.snap) === -1) {
      fail(label + '.snap is unsupported.');
    }
    if (typeof value.zoom !== 'number' || !Number.isFinite(value.zoom) ||
        value.zoom < 0.25 || value.zoom > 8) {
      fail(label + '.zoom must be from 0.25 through 8.');
    }
    var branch = document.branches.some(function(candidate) { return candidate.id === value.pathId; });
    if (!branch) fail(label + '.pathId does not exist in the scene.');
    if (value.selectedActorId !== null && !document.actors.some(function(actor) {
      return actor.id === value.selectedActorId;
    })) {
      fail(label + '.selectedActorId does not exist in the scene.');
    }
    if (value.selectedClipId !== null && !document.tracks.some(function(track) {
      return track.clips.some(function(clip) { return clip.id === value.selectedClipId; });
    })) {
      fail(label + '.selectedClipId does not exist in the scene.');
    }
    return cleanView(value, scene, document);
  }

  function sceneChanged(state, scene) {
    var history = state.histories[scene.storageId];
    var baseline = state.originalSerialized[scene.storageId];
    if (!history || typeof baseline !== 'string') return false;
    return OB64.cutsceneModel.serializeSceneDocument(history.present, 0) !== baseline;
  }

  function editedScenes(state) {
    if (!state || !state.catalog) return [];
    return state.catalog.scenes.filter(function(scene) {
      return !scene.authored && (sceneChanged(state, scene) || !!(scene.wiped && scene.wiped.pending));
    });
  }

  // An authored scene is a change unless it matches the loaded ROM exactly:
  // same name, same stored stream, and no unsaved document edits.
  function authoredDiffersFromRom(state, scene) {
    var info = scene && scene.authored;
    if (!info) return false;
    var rom = info.romState;
    return !rom || rom.name !== info.name || rom.streamHex !== hexOf(info.initialDecodedBytes) ||
      sceneChanged(state, scene);
  }

  function sortedBindings(bindings) {
    return (bindings || []).map(function(binding) {
      return JSON.parse(JSON.stringify(binding));
    }).sort(function(left, right) {
      return left.launchSiteId < right.launchSiteId ? -1 : left.launchSiteId > right.launchSiteId ? 1 : 0;
    });
  }

  // Event bindings are one set: a change when it differs from the set the
  // loaded ROM holds.
  function bindingsDifferFromRom(state) {
    return !!state && OB64.cutsceneModel.stableStringify(sortedBindings(state.eventBindings), 0) !==
      OB64.cutsceneModel.stableStringify(sortedBindings(state.romEventBindings), 0);
  }

  // Everything a Cutscene Project or export would carry: edited retail scenes,
  // authored scenes that differ from the loaded ROM, and a changed binding set.
  function changeCount(state) {
    if (!state || !state.catalog) return 0;
    return editedScenes(state).length + authoredScenes(state).filter(function(scene) {
      return authoredDiffersFromRom(state, scene);
    }).length + (bindingsDifferFromRom(state) ? 1 : 0);
  }

  function authoredScenes(state) {
    if (!state || !state.catalog || typeof state.catalog.authoredScenes !== 'function') return [];
    return state.catalog.authoredScenes();
  }

  function hexOf(bytes) {
    return Array.prototype.map.call(bytes, function(value) {
      return value.toString(16).padStart(2, '0');
    }).join('');
  }

  function bytesOfHex(text, label) {
    if (typeof text !== 'string' || !/^(?:[0-9a-f]{2})+$/.test(text) || text.length > 131072) {
      fail(label + ' must be lowercase hex bytes of at most 64 KiB.');
    }
    var output = new Uint8Array(text.length / 2);
    for (var index = 0; index < output.length; index++) output[index] = parseInt(text.substr(index * 2, 2), 16);
    return output;
  }

  function collect(state) {
    if (!state || !state.catalog) return null;
    // A Project saved before authored scenes were read from the ROM, or after
    // they failed verification, would silently drop them. Refuse instead.
    var discovery = state.authoredDiscovery;
    if (discovery && discovery.status === 'pending') {
      fail('Cutscene Studio is still reading new scenes from this ROM. Save the Project again in a moment.');
    }
    if (discovery && discovery.status === 'failed') {
      fail('This ROM has new-scene records that could not be verified, so the Cutscene Project cannot be saved: ' +
        discovery.message);
    }
    // An authored scene is saved with its identity, its initial stream (the
    // baseline its document edits), and its current document when opened.
    // Like retail scenes, one that matches the loaded ROM exactly is not a
    // change and is left out; the ROM already carries it.
    var authored = authoredScenes(state).filter(function(scene) {
      return authoredDiffersFromRom(state, scene);
    }).map(function(scene) {
      var history = state.histories[scene.storageId];
      var info = scene.authored;
      return {
        authoredId: info.authoredId,
        name: info.name,
        selectorRow: info.selectorRow,
        sceneClass: info.sceneClass,
        templateSceneId: info.templateSceneId,
        initialStreamHex: hexOf(info.initialDecodedBytes),
        document: history ? OB64.cutsceneModel.cloneSceneDocument(history.present) : null,
        view: history ? cleanView(state.views[scene.sceneId], scene, history.present) : null
      };
    });
    var scenes = editedScenes(state).map(function(scene) {
      var document = state.histories[scene.storageId].present;
      var row = {
        sceneId: scene.sceneId,
        storageId: scene.storageId,
        assetId: scene.assetId,
        document: OB64.cutsceneModel.cloneSceneDocument(document),
        view: cleanView(state.views[scene.sceneId], scene, document)
      };
      // A wipe made on top of the loaded ROM is replayed from that ROM on load.
      if (scene.wiped && scene.wiped.pending) row.wiped = true;
      return row;
    });
    var anyWiped = scenes.some(function(row) { return row.wiped === true; });
    // A changed binding set is saved whole, including an empty set that
    // returns every event row to retail.
    var bindingsChanged = bindingsDifferFromRom(state);
    var deleted = (state.deletedAuthored || []).map(function(entry) { return entry.authoredId; }).sort();
    if (!scenes.length && !authored.length && !bindingsChanged && !deleted.length) return null;
    var payload = {
      format: FORMAT,
      schemaVersion: authored.length || bindingsChanged || anyWiped || deleted.length ? SCHEMA_VERSION : RETAIL_ONLY_SCHEMA,
      sourceRevision: 'us-rev0',
      selectedSceneId: state.selectedSceneId,
      scenes: scenes
    };
    if (authored.length) payload.authoredScenes = authored;
    if (bindingsChanged) payload.eventBindings = sortedBindings(state.eventBindings);
    if (deleted.length) payload.deletedAuthoredScenes = deleted;
    return payload;
  }

  function validateSceneIdentity(scene, document, label) {
    if (document.identity.sceneId !== scene.sceneId ||
        document.identity.engine !== scene.engine ||
        document.identity.sourceRevision !== scene.sourceRevision ||
        document.identity.directorKey !== scene.directorKey ||
        document.native.sourceAssetId !== scene.assetId) {
      fail(label + ' does not match its catalogued physical scene identity.');
    }
  }

  function validateNativePreimage(baseline, document, label) {
    var expected = OB64.cutsceneModel.stableStringify(baseline.native, 0);
    var actual = OB64.cutsceneModel.stableStringify(document.native, 0);
    if (actual !== expected) {
      fail(label + ' changed preserved native command boundaries or words.');
    }
  }

  function validatePayloadShape(payload, catalog) {
    if (!isObject(payload)) fail('Cutscene Project data must be an object.');
    onlyFields(payload, ['format', 'schemaVersion', 'sourceRevision', 'selectedSceneId', 'scenes', 'authoredScenes',
      'eventBindings', 'deletedAuthoredScenes'], 'Cutscene Project data');
    if (payload.deletedAuthoredScenes !== undefined && (!Array.isArray(payload.deletedAuthoredScenes) ||
        payload.deletedAuthoredScenes.some(function(id) { return typeof id !== 'string' || !/^[0-9a-f]{32}$/.test(id); }))) {
      fail('Cutscene Project deletedAuthoredScenes must list authored identities.');
    }
    if (payload.format !== FORMAT) fail('Cutscene Project format is unsupported.');
    if (!Number.isInteger(payload.schemaVersion) || payload.schemaVersion > SCHEMA_VERSION) {
      fail('Cutscene Project schema version ' + payload.schemaVersion + ' is newer than this editor.');
    }
    if (payload.schemaVersion !== SCHEMA_VERSION && payload.schemaVersion !== RETAIL_ONLY_SCHEMA) {
      fail('Cutscene Project schema version ' + payload.schemaVersion + ' has no migration path.');
    }
    if (payload.schemaVersion === RETAIL_ONLY_SCHEMA &&
        (payload.authoredScenes !== undefined || payload.eventBindings !== undefined)) {
      fail('Cutscene Project schema 1 cannot contain authored scenes or event bindings.');
    }
    if (payload.eventBindings !== undefined && !Array.isArray(payload.eventBindings)) {
      fail('Cutscene Project eventBindings must be an array.');
    }
    if (payload.sourceRevision !== 'us-rev0') {
      fail('Cutscene Project source revision must be us-rev0.');
    }
    string(payload.selectedSceneId, 'Cutscene Project selectedSceneId');
    var authored = payload.authoredScenes || [];
    if (!Array.isArray(authored)) fail('Cutscene Project authoredScenes must be an array.');
    var hasBindings = payload.eventBindings !== undefined;
    var hasWiped = Array.isArray(payload.scenes) && payload.scenes.some(function(row) { return isObject(row) && row.wiped === true; });
    var hasDeleted = Array.isArray(payload.deletedAuthoredScenes) && payload.deletedAuthoredScenes.length > 0;
    if (payload.schemaVersion === SCHEMA_VERSION && !authored.length && !hasBindings && !hasWiped && !hasDeleted) {
      fail('Cutscene Project schema 2 must contain an authored scene, event bindings or a wiped scene.');
    }
    var selectsAuthored = authored.some(function(entry) {
      return isObject(entry) && payload.selectedSceneId === 'scene:authored:' + entry.authoredId;
    });
    if (!catalog.getScene(payload.selectedSceneId) && !selectsAuthored) {
      fail('Cutscene Project selectedSceneId is not in the catalog.');
    }
    if (!Array.isArray(payload.scenes) || (!payload.scenes.length && !authored.length && !hasBindings && !hasDeleted)) {
      fail('Cutscene Project must contain at least one edited physical scene.');
    }
  }

  // Authored scenes are rebuilt from the Project's own initial stream, then
  // reconciled with any authored scene already discovered in the loaded ROM:
  // the same identity must keep the same row; a row held by another identity,
  // or a repeated identity or row, is refused. Nothing is registered here.
  function prepareAuthored(rom, state, payload, options) {
    var authored = payload.authoredScenes || [];
    if (!authored.length) return Promise.resolve([]);
    if (!OB64.cutsceneAuthored) return Promise.reject(new CutsceneProjectError('This editor build cannot load authored scenes.'));
    var ids = {}, rows = {};
    var entries;
    try {
      entries = authored.map(function(entry, index) {
        var label = 'Cutscene Project authored scene ' + index;
        if (!isObject(entry)) fail(label + ' must be an object.');
        onlyFields(entry, ['authoredId', 'name', 'selectorRow', 'sceneClass', 'templateSceneId',
          'initialStreamHex', 'document', 'view'], label);
        if (!OB64.cutsceneAuthored.validId(entry.authoredId)) fail(label + ' has an invalid identity.');
        string(entry.name, label + '.name');
        integer(entry.selectorRow, label + '.selectorRow', 0, OB64.cutsceneAuthored.directorTableRows - 1);
        if (entry.sceneClass !== 5) fail(label + ' must be a class-5 scene.');
        string(entry.templateSceneId, label + '.templateSceneId');
        if (ids[entry.authoredId] || rows[entry.selectorRow]) fail(label + ' repeats an authored identity or selector row.');
        ids[entry.authoredId] = rows[entry.selectorRow] = true;
        var bytes = bytesOfHex(entry.initialStreamHex, label + '.initialStreamHex');
        OB64.cutsceneAuthored.validateAuthoredStream(bytes);
        var existing = state.catalog.getScene('scene:authored:' + entry.authoredId);
        if (existing && existing.authored.selectorRow !== entry.selectorRow) {
          fail(label + ' is already open at selector row ' + existing.authored.selectorRow + '.');
        }
        state.catalog.authoredScenes().forEach(function(scene) {
          if (scene.authored.selectorRow === entry.selectorRow && scene.authored.authoredId !== entry.authoredId) {
            fail(label + ' claims selector row ' + entry.selectorRow + ', which another authored scene holds.');
          }
        });
        return { entry: entry, label: label, bytes: bytes, existing: existing };
      });
    } catch (error) {
      return Promise.reject(error);
    }
    var chain = Promise.resolve(), prepared = [];
    entries.forEach(function(row) {
      chain = chain.then(function() {
        var rowKey = row.existing ? readRowKey(rom.z64, row.entry.selectorRow) : undefined;
        // Build a detached scene; an existing registration is replaced only on apply.
        var probeCatalog = Object.create(state.catalog);
        probeCatalog.getScene = function(identity) {
          return identity === 'scene:authored:' + row.entry.authoredId ? null : state.catalog.getScene(identity);
        };
        probeCatalog.directorScenes = state.catalog.directorScenes.filter(function(scene) {
          return !(scene.authored && scene.authored.authoredId === row.entry.authoredId);
        });
        return OB64.cutsceneAuthored.create(rom.z64, probeCatalog, {
          authoredId: row.entry.authoredId, name: row.entry.name, selectorRow: row.entry.selectorRow,
          templateSceneId: row.entry.templateSceneId, decodedBytes: row.bytes, ownedRowKey: rowKey,
          otherAuthored: prepared.map(function(item) { return item.scene.authored; })
        }).then(function(made) {
          var baseline = OB64.cutsceneCodec.projectSceneDocument(made.scene, made.source, state.catalog).document;
          var document = null, view = null;
          if (row.entry.document !== null) {
            document = OB64.cutsceneModel.parseSceneDocument(row.entry.document);
            if (document.identity.sceneId !== made.scene.sceneId || document.native.sourceAssetId !== made.scene.assetId) {
              fail(row.label + ' document does not belong to its authored scene.');
            }
            migrateLegacyBackground(made.scene, made.source, state.catalog, baseline, document);
            validateNativePreimage(baseline, document, row.label);
            if (OB64.cutsceneExport && OB64.cutsceneExport.assessNativeDelta) {
              document.exportRequirements = OB64.cutsceneExport.assessNativeDelta(made.scene, baseline, document, made.source);
            }
            OB64.cutsceneModel.validateSceneDocument(document);
            view = row.entry.view ? validateView(row.entry.view, made.scene, document, row.label + '.view')
              : cleanView(null, made.scene, document);
          }
          prepared.push({ scene: made.scene, source: made.source, baseline: baseline, document: document, view: view,
            replaces: row.existing || null,
            originalSerialized: OB64.cutsceneModel.serializeSceneDocument(baseline, 0) });
        });
      });
    });
    return chain.then(function() { return prepared; });
  }

  function readRowKey(z64, row) {
    var at = OB64.cutsceneAuthored.directorTableZ64 + row * 4;
    return ((z64[at] << 24) | (z64[at + 1] << 16) | (z64[at + 2] << 8) | z64[at + 3]) >>> 0;
  }

  function canonicalJson(value) {
    if (Array.isArray(value)) return '[' + value.map(canonicalJson).join(',') + ']';
    if (value && typeof value === 'object') {
      return '{' + Object.keys(value).sort().map(function(key) {
        return JSON.stringify(key) + ':' + canonicalJson(value[key]);
      }).join(',') + '}';
    }
    return JSON.stringify(value);
  }

  // Projects saved before background selection hold the background exactly as
  // the older projection derived it. Only that unchanged derived background is
  // replaced with today's baseline; real edits and preview overrides are kept
  // as saved, and export judges them as before.
  function migrateLegacyBackground(scene, source, catalog, baseline, document) {
    if (!document || canonicalJson(document.background) === canonicalJson(baseline.background)) return;
    var legacy = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog, { legacyBackground: true }).document;
    if (canonicalJson(document.background) !== canonicalJson(legacy.background)) return;
    document.background = JSON.parse(JSON.stringify(baseline.background));
  }

  function prepareImport(rom, payload, options) {
    options = options || {};
    if (!OB64.cutsceneUI) return Promise.reject(new CutsceneProjectError(
      'This editor build has no Cutscene Studio loader.'));
    var state;
    try {
      state = OB64.cutsceneUI.ensureState(rom);
      validatePayloadShape(payload, state.catalog);
    } catch (error) {
      return Promise.reject(error);
    }
    var seen = {};
    var rows;
    try {
      rows = payload.scenes.map(function(row, index) {
        var label = 'Cutscene Project scene ' + index;
        if (!isObject(row)) fail(label + ' must be an object.');
        onlyFields(row, ['sceneId', 'storageId', 'assetId', 'document', 'view', 'wiped'], label);
        if (row.wiped !== undefined && row.wiped !== true) fail(label + '.wiped must be true when present.');
        if (row.wiped && payload.schemaVersion !== SCHEMA_VERSION) fail(label + ' is wiped, which needs Project schema 2.');
        string(row.sceneId, label + '.sceneId');
        string(row.storageId, label + '.storageId');
        string(row.assetId, label + '.assetId');
        var scene = state.catalog.getScene(row.sceneId);
        if (!scene || scene.sceneId !== row.sceneId || scene.storageId !== row.storageId ||
            scene.assetId !== row.assetId) {
          fail(label + ' does not identify one catalogued physical scene.');
        }
        if (seen[scene.storageId]) fail(label + ' duplicates physical scene ' + scene.storageId + '.');
        seen[scene.storageId] = true;
        var document = OB64.cutsceneModel.parseSceneDocument(row.document);
        if (scene.engine !== 'director') validateSceneIdentity(scene, document, label);
        var view = validateView(row.view, scene, document, label + '.view');
        return { scene: scene, document: document, view: view, label: label, wiped: row.wiped === true };
      });
    } catch (error) {
      return Promise.reject(error);
    }

    var ready = state.authoredReady || Promise.resolve();
    return ready.then(function() { return Promise.all(rows.map(function(row) {
      if (row.scene.engine !== 'director') {
        var baseline = OB64.cutsceneUI.presentationDocument(state, row.scene);
        validateNativePreimage(baseline, row.document, row.label);
        row.document.exportRequirements = {
          capability: 'needs-research',
          reasons: [row.scene.source.adapterStatus],
          allocationBytes: 0,
          features: ['presentation-adapter']
        };
        OB64.cutsceneModel.validateSceneDocument(row.document);
        return Promise.resolve({
          scene: row.scene,
          document: row.document,
          view: row.view,
          source: null,
          baseline: baseline,
          originalSerialized: OB64.cutsceneModel.serializeSceneDocument(baseline, 0)
        });
      }
      // A wiped row is rebuilt from the loaded ROM exactly as the Wipe action
      // does; its document must then be an edit of that empty scene. A scene
      // the ROM already holds wiped loads like any other scene.
      if (row.wiped && !row.scene.wiped) {
        return OB64.cutsceneUI.prepareWipe(rom, state, row.scene, true).then(function(prepared) {
          validateSceneIdentity(row.scene, row.document, row.label);
          migrateLegacyBackground(prepared.adapted, prepared.source, state.catalog, prepared.document, row.document);
          validateNativePreimage(prepared.document, row.document, row.label);
          var assessWiped = OB64.cutsceneExport && (OB64.cutsceneExport.assessNativeDelta ||
            OB64.cutsceneExport.assessFixedSlotDelta);
          if (assessWiped) {
            row.document.exportRequirements = assessWiped(prepared.adapted, prepared.document, row.document, prepared.source);
            OB64.cutsceneModel.validateSceneDocument(row.document);
          }
          return {
            scene: row.scene, document: row.document, view: row.view, source: prepared.source,
            baseline: prepared.document, wipe: prepared,
            originalSerialized: OB64.cutsceneModel.serializeSceneDocument(prepared.document, 0)
          };
        });
      }
      return OB64.cutsceneCodec.loadSceneSource(rom.z64, row.scene,
        Object.assign({}, options, { allowModified: true })).then(function(source) {
        validateSceneIdentity(row.scene, row.document, row.label);
        var baseline = OB64.cutsceneCodec.projectSceneDocument(
          row.scene, source, state.catalog).document;
        migrateLegacyBackground(row.scene, source, state.catalog, baseline, row.document);
        validateNativePreimage(baseline, row.document, row.label);
        if (OB64.cutsceneExport && (OB64.cutsceneExport.assessNativeDelta ||
            OB64.cutsceneExport.assessFixedSlotDelta)) {
          var assessDelta = OB64.cutsceneExport.assessNativeDelta ||
            OB64.cutsceneExport.assessFixedSlotDelta;
          row.document.exportRequirements = assessDelta(
            row.scene, baseline, row.document, source);
          OB64.cutsceneModel.validateSceneDocument(row.document);
        }
        return {
          scene: row.scene,
          document: row.document,
          view: row.view,
          source: source,
          baseline: baseline,
          originalSerialized: OB64.cutsceneModel.serializeSceneDocument(baseline, 0)
        };
      });
    })); }).then(function(entries) {
      return prepareAuthored(rom, state, payload, options).then(function(authored) {
        // Bindings may launch a Project scene that is not registered yet.
        var eventBindings;
        if (payload.eventBindings !== undefined) {
          if (!OB64.cutsceneEvents) fail('This editor build cannot load event bindings.');
          eventBindings = OB64.cutsceneEvents.publicBindings(OB64.cutsceneEvents.normalize(
            state.catalog, payload.eventBindings, {
              resolveAuthored: function(authoredId) {
                var entry = authored.find(function(item) { return item.scene.authored.authoredId === authoredId; });
                return entry ? entry.scene.authored.selectorRow : null;
              }
            }));
        }
        // Deletions name new scenes this ROM holds and the Project does not.
        var deletions = (payload.deletedAuthoredScenes || []).map(function(authoredId) {
          var held = state.catalog.getScene('scene:authored:' + authoredId);
          if (!held || !held.authored || !held.authored.romState) {
            fail('Cutscene Project deletes new scene ' + authoredId + ', which this ROM does not hold.');
          }
          if (authored.some(function(item) { return item.scene.authored.authoredId === authoredId; })) {
            fail('Cutscene Project both keeps and deletes new scene ' + authoredId + '.');
          }
          return held;
        });
        return {
          format: FORMAT,
          schemaVersion: SCHEMA_VERSION,
          selectedSceneId: payload.selectedSceneId,
          entries: entries,
          authoredEntries: authored,
          eventBindings: eventBindings,
          deletions: deletions
        };
      });
    });
  }

  function applyPrepared(state, prepared) {
    if (!state || !state.catalog) fail('Cutscene Studio state is unavailable.');
    if (!prepared || prepared.format !== FORMAT || prepared.schemaVersion !== SCHEMA_VERSION ||
        !Array.isArray(prepared.entries)) {
      fail('Prepared Cutscene Project data is invalid.');
    }
    (prepared.authoredEntries || []).forEach(function(entry) {
      if (entry.replaces) {
        // The same identity already in the loaded ROM keeps its ROM state, so
        // an unchanged restore is not counted as a new change.
        if (entry.replaces.authored && entry.replaces.authored.romState) {
          entry.scene.authored.romState = entry.replaces.authored.romState;
        }
        var index = state.catalog.scenes.indexOf(entry.replaces);
        if (index >= 0) state.catalog.scenes.splice(index, 1);
        var directorIndex = state.catalog.directorScenes.indexOf(entry.replaces);
        if (directorIndex >= 0) state.catalog.directorScenes.splice(directorIndex, 1);
        delete state.histories[entry.replaces.storageId];
        delete state.runtimeByAssetId[entry.replaces.assetId];
      }
      state.catalog.registerAuthoredScene(entry.scene, { replace: !!entry.replaces });
      state.sourceByAssetId[entry.scene.assetId] = entry.source;
      state.programByAssetId = state.programByAssetId || {};
      state.programByAssetId[entry.scene.assetId] = OB64.cutsceneCodec.createIr(
        entry.scene, entry.source.decodedBytes, entry.source.nodeDefinitions).program;
      state.originalSerialized[entry.scene.storageId] = entry.originalSerialized;
      if (entry.document) {
        state.histories[entry.scene.storageId] = OB64.cutsceneModel.createHistory(entry.document, 200);
        state.views[entry.scene.sceneId] = entry.view;
      }
    });
    prepared.entries.forEach(function(entry) {
      if (entry.wipe && !entry.scene.wiped) OB64.cutsceneUI.applyWipe(null, state, entry.wipe);
      state.histories[entry.scene.storageId] =
        OB64.cutsceneModel.createHistory(entry.document, 200);
      state.originalSerialized[entry.scene.storageId] = entry.originalSerialized;
      state.sourceByAssetId[entry.scene.assetId] = entry.source;
      state.programByAssetId=state.programByAssetId||{};
      if (entry.source) state.programByAssetId[entry.scene.assetId] = OB64.cutsceneCodec.createIr(
        entry.scene, entry.source.decodedBytes, entry.source.nodeDefinitions).program;
      state.views[entry.scene.sceneId] = cleanView(entry.view, entry.scene, entry.document);
      delete state.sourceErrors[entry.scene.assetId];
    });
    if (state.catalog.getScene(prepared.selectedSceneId)) {
      state.selectedSceneId = prepared.selectedSceneId;
    }
    (prepared.deletions || []).forEach(function(scene) {
      if (state.catalog.getScene(scene.sceneId) === scene) OB64.cutsceneUI.deleteAuthoredScene(null, state, scene);
    });
    var bindingCount = 0;
    if (prepared.eventBindings) {
      var bindingsBefore = state.eventBindings || [];
      state.eventBindings = sortedBindings(prepared.eventBindings);
      // Loaded bindings move launch contexts like any binding change.
      if (OB64.cutsceneUI && OB64.cutsceneUI.invalidateAfterBindingChange && OB64.cutsceneEvents) {
        OB64.cutsceneUI.invalidateAfterBindingChange(null, state, bindingsBefore);
      }
      bindingCount = bindingsDifferFromRom(state) ? 1 : 0;
    }
    return prepared.entries.length + (prepared.authoredEntries || []).length + bindingCount + (prepared.deletions || []).length;
  }

  OB64.cutsceneProject = {
    format: FORMAT,
    schemaVersion: SCHEMA_VERSION,
    CutsceneProjectError: CutsceneProjectError,
    sceneChanged: sceneChanged,
    editedScenes: editedScenes,
    collect: collect,
    authoredScenes: authoredScenes,
    authoredDiffersFromRom: authoredDiffersFromRom,
    changeCount: changeCount,
    bindingsDifferFromRom: bindingsDifferFromRom,
    hexOf: hexOf,
    prepareImport: prepareImport,
    applyPrepared: applyPrepared,
    cleanView: cleanView
  };
})(window.OB64);
