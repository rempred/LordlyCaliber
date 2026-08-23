// Lordly Caliber - Cutscene Studio Project serialization and guarded import.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var FORMAT = 'ob64-cutscene-project';
  var SCHEMA_VERSION = 1;

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
    return state.catalog.scenes.filter(function(scene) { return sceneChanged(state, scene); });
  }

  function collect(state) {
    if (!state || !state.catalog) return null;
    var scenes = editedScenes(state).map(function(scene) {
      var document = state.histories[scene.storageId].present;
      return {
        sceneId: scene.sceneId,
        storageId: scene.storageId,
        assetId: scene.assetId,
        document: OB64.cutsceneModel.cloneSceneDocument(document),
        view: cleanView(state.views[scene.sceneId], scene, document)
      };
    });
    if (!scenes.length) return null;
    return {
      format: FORMAT,
      schemaVersion: SCHEMA_VERSION,
      sourceRevision: 'us-rev0',
      selectedSceneId: state.selectedSceneId,
      scenes: scenes
    };
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

  function validateNativePreimage(scene, baseline, document, label) {
    var expected = OB64.cutsceneModel.stableStringify(baseline.native, 0);
    var actual = OB64.cutsceneModel.stableStringify(document.native, 0);
    if (actual !== expected) {
      fail(label + ' changed preserved native command boundaries or words.');
    }
    if (document.native.commands.length !== scene.source.nodes.length) {
      fail(label + ' does not preserve every catalogued source boundary.');
    }
  }

  function validatePayloadShape(payload, catalog) {
    if (!isObject(payload)) fail('Cutscene Project data must be an object.');
    onlyFields(payload, ['format', 'schemaVersion', 'sourceRevision', 'selectedSceneId', 'scenes'],
      'Cutscene Project data');
    if (payload.format !== FORMAT) fail('Cutscene Project format is unsupported.');
    if (!Number.isInteger(payload.schemaVersion) || payload.schemaVersion > SCHEMA_VERSION) {
      fail('Cutscene Project schema version ' + payload.schemaVersion + ' is newer than this editor.');
    }
    if (payload.schemaVersion !== SCHEMA_VERSION) {
      fail('Cutscene Project schema version ' + payload.schemaVersion + ' has no migration path.');
    }
    if (payload.sourceRevision !== 'us-rev0') {
      fail('Cutscene Project source revision must be us-rev0.');
    }
    string(payload.selectedSceneId, 'Cutscene Project selectedSceneId');
    if (!catalog.getScene(payload.selectedSceneId)) {
      fail('Cutscene Project selectedSceneId is not in the catalog.');
    }
    if (!Array.isArray(payload.scenes) || !payload.scenes.length) {
      fail('Cutscene Project must contain at least one edited physical scene.');
    }
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
        onlyFields(row, ['sceneId', 'storageId', 'assetId', 'document', 'view'], label);
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
        validateSceneIdentity(scene, document, label);
        var view = validateView(row.view, scene, document, label + '.view');
        return { scene: scene, document: document, view: view, label: label };
      });
    } catch (error) {
      return Promise.reject(error);
    }

    return Promise.all(rows.map(function(row) {
      if (row.scene.engine !== 'director') {
        var baseline = OB64.cutsceneUI.presentationDocument(state, row.scene);
        validateNativePreimage(row.scene, baseline, row.document, row.label);
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
      return OB64.cutsceneCodec.loadSceneSource(rom.z64, row.scene, options).then(function(source) {
        var baseline = OB64.cutsceneCodec.projectSceneDocument(
          row.scene, source, state.catalog).document;
        validateNativePreimage(row.scene, baseline, row.document, row.label);
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
    })).then(function(entries) {
      return {
        format: FORMAT,
        schemaVersion: SCHEMA_VERSION,
        selectedSceneId: payload.selectedSceneId,
        entries: entries
      };
    });
  }

  function applyPrepared(state, prepared) {
    if (!state || !state.catalog) fail('Cutscene Studio state is unavailable.');
    if (!prepared || prepared.format !== FORMAT || prepared.schemaVersion !== SCHEMA_VERSION ||
        !Array.isArray(prepared.entries)) {
      fail('Prepared Cutscene Project data is invalid.');
    }
    prepared.entries.forEach(function(entry) {
      state.histories[entry.scene.storageId] =
        OB64.cutsceneModel.createHistory(entry.document, 200);
      state.originalSerialized[entry.scene.storageId] = entry.originalSerialized;
      state.sourceByAssetId[entry.scene.assetId] = entry.source;
      state.views[entry.scene.sceneId] = cleanView(entry.view, entry.scene, entry.document);
      delete state.sourceErrors[entry.scene.assetId];
    });
    if (state.catalog.getScene(prepared.selectedSceneId)) {
      state.selectedSceneId = prepared.selectedSceneId;
    }
    return prepared.entries.length;
  }

  OB64.cutsceneProject = {
    format: FORMAT,
    schemaVersion: SCHEMA_VERSION,
    CutsceneProjectError: CutsceneProjectError,
    sceneChanged: sceneChanged,
    editedScenes: editedScenes,
    collect: collect,
    prepareImport: prepareImport,
    applyPrepared: applyPrepared,
    cleanView: cleanView
  };
})(window.OB64);
