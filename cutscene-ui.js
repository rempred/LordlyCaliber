// Lordly Caliber - visual Cutscene Studio browser, Stage, Storyboard, and Timeline.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var IMAGE_CACHE_LIMIT = 32 * 1024 * 1024;

  function UiError(message) {
    this.name = 'CutsceneUiError';
    this.message = message;
  }
  UiError.prototype = Object.create(Error.prototype);
  UiError.prototype.constructor = UiError;

  function fail(message) { throw new UiError(message); }

  function node(tag, className, text) {
    var output = document.createElement(tag);
    if (className) output.className = className;
    if (text != null) output.textContent = text;
    return output;
  }

  function button(text, className, action) {
    var output = node('button', className || 'btn-secondary', text);
    output.type = 'button';
    if (action) output.addEventListener('click', action);
    return output;
  }

  function field(label, control, hint) {
    var wrapper = node('label', 'cutscene-field');
    wrapper.appendChild(node('span', 'cutscene-field-label', label));
    wrapper.appendChild(control);
    if (hint) wrapper.appendChild(node('small', 'cutscene-field-hint', hint));
    return wrapper;
  }

  function capabilityBadge(value, scope) {
    var labels = OB64.cutsceneModel.capabilityLabels;
    var label = labels[value] || value;
    if (scope === 'command' && value === 'native') label = 'Native command';
    else if (scope) label = scope + ': ' + label;
    return node('span', 'cutscene-capability cutscene-capability-' + value,
      label);
  }

  function ensureState(rom) {
    if (!rom) fail('Load a ROM before opening Cutscene Studio.');
    if (rom.cutsceneStudio) return rom.cutsceneStudio;
    if (!rom.layout || rom.layout.id !== 'us-rev0') {
      fail('Cutscene Studio currently writes only the US Rev 0 layout.');
    }
    var catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
    var preferred = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene') ||
      catalog.scenes[0];
    rom.cutsceneStudio = {
      catalog: catalog,
      selectedSceneId: preferred.sceneId,
      histories: {},
      originalSerialized: {},
      sourceByAssetId: {},
      programByAssetId: {},
      runtimeByAssetId: {},
      z64: rom.z64,
      sourceErrors: {},
      loadingByAssetId: {},
      projectionLoadingByAssetId: {},
      concurrentRuntimeByLaunchContext: {},
      views: {},
      imageCache: {},
      imageLoading: {},
      imageCacheBytes: 0,
      imageClock: 0,
      imageRequest: 0,
      combatEffectCache: {},
      combatEffectAnimations: null,
      combatEffectFilter: 'all',
      openRequest: 0,
      ui: null,
      callbacks: {},
      search: '',
      renderedStage: null,
      spriteState: OB64.cutsceneSprites
        ? OB64.cutsceneSprites.create(rom.z64, catalog) : null,
      drag: null,
      animationFrame: null,
      lastAnimationTime: null
    };
    return rom.cutsceneStudio;
  }

  function initialize(rom) {
    return ensureState(rom);
  }

  function selectedScene(state) {
    return state.catalog.getScene(state.selectedSceneId);
  }

  function initialViewFrame(state, sceneId) {
    var scene = state.catalog.getScene(sceneId);
    var projection = scene && scene.backgroundRuntimeObservation &&
      scene.backgroundRuntimeObservation.stageProjection;
    var preferred = projection && Number.isInteger(projection.initialPreviewFrame)
      ? Math.max(0, projection.initialPreviewFrame) : null;
    var runtime = scene && state.runtimeByAssetId && state.runtimeByAssetId[scene.assetId];
    if (!runtime || !Array.isArray(runtime.states) || !runtime.states.length) {
      return preferred == null ? 0 : preferred;
    }
    function inspectable(runtimeState) {
      if (!runtimeState) return false;
      var hasVisual = runtimeState.background &&
          Array.isArray(runtimeState.background.layers) && runtimeState.background.layers.length ||
        Array.isArray(runtimeState.actors) && runtimeState.actors.some(function(actor) {
          return actor.visible;
        }) || Array.isArray(runtimeState.effects) && runtimeState.effects.length ||
        runtimeState.sceneVignette && runtimeState.sceneVignette.sourceAssetId;
      var overlayClear = !Array.isArray(runtimeState.overlays) ||
        runtimeState.overlays.every(function(overlay) { return overlay.alpha <= 0; });
      var color = runtimeState.sceneColor || { red: 255, green: 255, blue: 255 };
      return !!hasVisual && overlayClear && Math.max(color.red, color.green, color.blue) > 0;
    }
    if (preferred != null && inspectable(runtime.states[preferred])) return preferred;
    for (var actorIndex = 0; actorIndex < runtime.states.length; actorIndex++) {
      var actorState = runtime.states[actorIndex];
      if (inspectable(actorState) && Array.isArray(actorState.actors) &&
          actorState.actors.some(function(actor) { return actor.visible; })) {
        return actorIndex;
      }
    }
    for (var index = 0; index < runtime.states.length; index++) {
      if (inspectable(runtime.states[index])) return index;
    }
    return preferred == null ? 0 : preferred;
  }

  function viewFor(state, sceneId) {
    if (!state.views[sceneId]) {
      state.views[sceneId] = {
        frame: initialViewFrame(state, sceneId), pathId: 'default', selectedActorId: null,
        selectedClipId: null, selectedSourceId: null, timelineMode: 'runtime',
        launchContextId: null, loop: false, snap: 'frame', zoom: 1
      };
    }
    return state.views[sceneId];
  }

  function launchContextChoices(state, scene) {
    if (!scene || !scene.launchProfile ||
        !Array.isArray(scene.launchProfile.parentEventLaunches)) return [];
    var choices = [];
    scene.launchProfile.parentEventLaunches.forEach(function(launch) {
      (launch.eventInvocationContexts || []).forEach(function(context, contextIndex) {
        var owner = context.concurrentDirectorSceneId
          ? state.catalog.getScene(context.concurrentDirectorSceneId) : null;
        var id = launch.launchId + ':invocation:' + context.eventInvocationCursor +
          ':context:' + contextIndex;
        var ownerLabel = owner
          ? 'after ' + OB64.cutsceneCatalog.displayName(owner)
          : 'no exact earlier Director';
        choices.push({
          id: id,
          label: 'Event row ' + launch.eventDirectoryRow + ' · byte 0x' +
            launch.decodedByteOffset.toString(16).toUpperCase().padStart(4, '0') +
            ' · ' + ownerLabel,
          launch: launch,
          context: context,
          contextScene: owner
        });
      });
    });
    return choices;
  }

  function launchContextChoice(state, scene, preferredId) {
    var choices = launchContextChoices(state, scene);
    if (!choices.length) return null;
    var view = viewFor(state, scene.sceneId);
    var selectedId = preferredId || view.launchContextId;
    var selected = choices.find(function(choice) { return choice.id === selectedId; }) ||
      choices[0];
    if (!preferredId) view.launchContextId = selected.id;
    return selected;
  }

  function precedingLaunchContextChoice(state, scene, parentContext) {
    var choices = launchContextChoices(state, scene);
    var exact = choices.find(function(choice) {
      return choice.launch.launchId === parentContext.precedingDirectorLaunchId &&
        (parentContext.precedingDirectorInvocationCursor === null ||
          choice.context.eventInvocationCursor ===
            parentContext.precedingDirectorInvocationCursor);
    });
    return exact || launchContextChoice(state, scene, null);
  }

  function launchContextCacheKey(scene, choice) {
    return scene.assetId + '|' + (choice ? choice.id : 'standalone');
  }

  function launchOperandTranslations(scene, choice) {
    var output = {};
    var context = choice && choice.context;
    var values = context && context.launchTranslationTable;
    var indexes = scene.launchProfile && scene.launchProfile.operandTranslation &&
      scene.launchProfile.operandTranslation.tableIndexes || [];
    if (!Array.isArray(values)) return output;
    indexes.forEach(function(tableIndex) {
      if (Number.isInteger(values[tableIndex])) {
        output[tableIndex] = values[tableIndex];
      }
    });
    return output;
  }

  function historyFor(state, scene) {
    return state.histories[scene.storageId] || null;
  }

  function selectedDocument(state) {
    var scene = selectedScene(state);
    var history = scene && historyFor(state, scene);
    return history ? history.present : null;
  }

  function presentationDocument(state, scene) {
    var document = OB64.cutsceneCatalog.createSceneDocument(scene);
    var association = scene.dialogueAssociations && scene.dialogueAssociations[0];
    var entry = association && state.catalog.getDialogueEntry(association.entryId);
    if (entry) {
      var archive = state.catalog.getDialogueArchive(entry.archiveIndex);
      document.tracks.push(OB64.cutsceneModel.createTrack({
        id: 'track:presentation:dialogue',
        type: 'dialogue',
        actorId: null,
        label: 'Associated dialogue',
        clips: [OB64.cutsceneModel.createClip({
          id: 'clip:presentation:dialogue:0',
          kind: 'dialogue',
          startFrame: 0,
          durationFrames: Math.max(90, Math.ceil(String(entry.text || '').length * 2.5)),
          capability: OB64.cutsceneModel.capabilities.PREVIEW_ONLY,
          payload: {
            sourceSystem: archive.presentationFamily + '-preview',
            dialogueArchiveId: archive.archiveId,
            dialogueEntryId: entry.entryId,
            presentationArchiveSelector: association.archiveSelector,
            presentationEntrySelector: association.entrySelector,
            presentationResourceKey: archive.presentationResourceKey,
            speaker: entry.speakerLabel ||
              (entry.speakerId == null ? 'Narrator' : 'Speaker ' + entry.speakerId),
            text: entry.text,
            rawText: entry.rawText,
            associationStatus: association.status
          },
          source: {
            associationStatus: association.status,
            adapterStatus: scene.source.adapterStatus
          }
        })]
      }));
      OB64.cutsceneModel.validateSceneDocument(document);
    }
    return document;
  }

  function cacheLoadedDocument(state, scene, document, source, program) {
    if (source && OB64.cutsceneExport) {
      document.exportRequirements = {
        capability: 'native',
        reasons: [],
        allocationBytes: source.consumedEncodedBytes,
        features: ['director-fixed-slot']
      };
    }
    var history = OB64.cutsceneModel.createHistory(document, 200);
    state.histories[scene.storageId] = history;
    state.originalSerialized[scene.storageId] =
      OB64.cutsceneModel.serializeSceneDocument(history.present, 0);
    if (source) state.sourceByAssetId[scene.assetId] = source;
    if (program) state.programByAssetId[scene.assetId] = program;
    delete state.sourceErrors[scene.assetId];
    return history.present;
  }

  function compileRuntimeDocument(state, scene, document, choice, contextRuntime) {
    var program = state.programByAssetId[scene.assetId];
    if (!program) return null;
    var runtimeOptions = {
      z64: state.z64,
      launchContext: choice ? choice.context : null,
      launchOperandTranslations: launchOperandTranslations(scene, choice)
    };
    if (contextRuntime) {
      runtimeOptions.contextRuntime = contextRuntime;
      runtimeOptions.contextTickOffset = Number.isInteger(
        choice.context.concurrentDirectorTickOffset)
        ? choice.context.concurrentDirectorTickOffset : 1;
    }
    return OB64.cutsceneRuntime.compile(
      document, program, scene, state.catalog, runtimeOptions);
  }

  function publishRuntime(state, scene, document, runtime) {
    if (!runtime) return null;
    state.runtimeByAssetId[scene.assetId] = runtime;
    OB64.cutsceneRuntime.bind(document, runtime);
    delete state.sourceErrors['runtime:' + scene.assetId];
    return runtime;
  }

  function refreshRuntime(state, scene, document) {
    if (!OB64.cutsceneRuntime || scene.engine !== 'director') return null;
    var choice = launchContextChoice(state, scene, null);
    var contextRuntime = choice && state.concurrentRuntimeByLaunchContext
      ? state.concurrentRuntimeByLaunchContext[
        launchContextCacheKey(scene, choice)] || null
      : null;
    try {
      return publishRuntime(state, scene, document,
        compileRuntimeDocument(state, scene, document, choice, contextRuntime));
    } catch (error) {
      delete state.runtimeByAssetId[scene.assetId];
      state.sourceErrors['runtime:' + scene.assetId] = error && error.message || String(error);
      return null;
    }
  }

  function ensureProjectedDocument(rom, state, scene) {
    state.projectionLoadingByAssetId = state.projectionLoadingByAssetId || {};
    var existing = historyFor(state, scene);
    if (existing) return Promise.resolve(existing.present);
    if (state.projectionLoadingByAssetId[scene.assetId]) {
      return state.projectionLoadingByAssetId[scene.assetId];
    }
    if (scene.engine !== 'director') {
      return Promise.resolve(cacheLoadedDocument(
        state, scene, presentationDocument(state, scene), null, null));
    }
    var promise = OB64.cutsceneCodec.loadSceneSource(rom.z64, scene).then(function(source) {
      var projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, state.catalog);
      return cacheLoadedDocument(
        state, scene, projected.document, source, projected.program);
    }).catch(function(error) {
      var document = OB64.cutsceneCatalog.createSceneDocument(scene);
      cacheLoadedDocument(state, scene, document, null, null);
      state.sourceErrors[scene.assetId] = error && error.message || String(error);
      return document;
    }).finally(function() {
      delete state.projectionLoadingByAssetId[scene.assetId];
    });
    state.projectionLoadingByAssetId[scene.assetId] = promise;
    return promise;
  }

  function ensureContextualRuntime(rom, state, scene, document, forcedChoice, ancestry,
      compactOutput) {
    state.concurrentRuntimeByLaunchContext =
      state.concurrentRuntimeByLaunchContext || {};
    var choice = forcedChoice || launchContextChoice(state, scene, null);
    var contextScene = choice && choice.contextScene;
    ancestry = ancestry || [];
    if (!contextScene) {
      var standaloneRuntime = compileRuntimeDocument(
        state, scene, document, choice, null);
      return Promise.resolve(compactOutput
        ? OB64.cutsceneRuntime.compactContextRuntime(standaloneRuntime)
        : standaloneRuntime);
    }
    if (ancestry.indexOf(contextScene.assetId) !== -1 ||
        contextScene.assetId === scene.assetId) {
      state.sourceErrors['runtime-context:' + scene.assetId] =
        'The parent event launch chain is cyclic; this preview stops before reusing ' +
        contextScene.assetId + '.';
      var boundedRuntime = compileRuntimeDocument(
        state, scene, document, choice, null);
      return Promise.resolve(compactOutput
        ? OB64.cutsceneRuntime.compactContextRuntime(boundedRuntime)
        : boundedRuntime);
    }
    return ensureProjectedDocument(rom, state, contextScene).then(function(contextDocument) {
      var precedingChoice = precedingLaunchContextChoice(
        state, contextScene, choice.context);
      return ensureContextualRuntime(rom, state, contextScene, contextDocument,
        precedingChoice, ancestry.concat(scene.assetId), true);
    }).then(function(contextRuntime) {
      state.concurrentRuntimeByLaunchContext[
        launchContextCacheKey(scene, choice)] = contextRuntime;
      delete state.sourceErrors['runtime-context:' + scene.assetId];
      var runtime = compileRuntimeDocument(
        state, scene, document, choice, contextRuntime);
      return compactOutput
        ? OB64.cutsceneRuntime.compactContextRuntime(runtime) : runtime;
    });
  }

  function loadScene(rom, state, scene) {
    if (state.loadingByAssetId[scene.assetId]) return state.loadingByAssetId[scene.assetId];
    var promise = ensureProjectedDocument(rom, state, scene).then(function(document) {
      if (scene.engine !== 'director' || !state.programByAssetId[scene.assetId]) {
        return document;
      }
      return ensureContextualRuntime(rom, state, scene, document, null, [])
        .then(function(runtime) {
          publishRuntime(state, scene, document, runtime);
          return document;
        });
    }).catch(function(error) {
      var history = historyFor(state, scene);
      if (history && state.programByAssetId[scene.assetId]) {
        refreshRuntime(state, scene, history.present);
      }
      state.sourceErrors['runtime-context:' + scene.assetId] =
        error && error.message || String(error);
      return history ? history.present : null;
    }).finally(function() {
      delete state.loadingByAssetId[scene.assetId];
    });
    state.loadingByAssetId[scene.assetId] = promise;
    return promise;
  }

  function activeKey(element) {
    return element && element.closest ? element.closest('[data-cutscene-focus-key]') : null;
  }

  function uiSceneId(panel) {
    var shell = panel && panel.querySelector ? panel.querySelector('.cutscene-studio') : null;
    return shell && shell.getAttribute
      ? shell.getAttribute('data-cutscene-scene-id') : null;
  }

  function scrollPosition(element) {
    return element ? { top: element.scrollTop || 0, left: element.scrollLeft || 0 } : null;
  }

  function restoreScrollPosition(element, position) {
    if (!element || !position) return;
    element.scrollTop = position.top;
    element.scrollLeft = position.left;
  }

  function pageScrollElement() {
    if (typeof document === 'undefined') return null;
    return document.scrollingElement || document.documentElement || document.body || null;
  }

  function selectHasValue(element, value) {
    var options = element && element.options;
    if (!options && element && element.querySelectorAll) options = element.querySelectorAll('option');
    for (var index = 0; options && index < options.length; index++) {
      if (String(options[index].value) === String(value)) return true;
    }
    return false;
  }

  function captureUi(panel) {
    var viewport = panel && panel.closest ? panel.closest('.content') : null;
    var output = {
      sceneId: uiSceneId(panel),
      scrolls: {},
      viewport: scrollPosition(viewport),
      page: scrollPosition(pageScrollElement()),
      focus: null
    };
    panel.querySelectorAll('[data-cutscene-scroll]').forEach(function(element) {
      output.scrolls[element.getAttribute('data-cutscene-scroll')] = {
        top: element.scrollTop, left: element.scrollLeft
      };
    });
    var focused = activeKey(document.activeElement);
    if (focused && panel.contains(focused)) {
      output.focus = {
        key: focused.getAttribute('data-cutscene-focus-key'),
        value: focused.options ? String(focused.value) : null,
        start: typeof focused.selectionStart === 'number' ? focused.selectionStart : null,
        end: typeof focused.selectionEnd === 'number' ? focused.selectionEnd : null
      };
    }
    return output;
  }

  function restoreUi(panel, snapshot) {
    if (!snapshot) return;
    var sameScene = !snapshot.sceneId || !uiSceneId(panel) ||
      snapshot.sceneId === uiSceneId(panel);
    if (sameScene && snapshot.focus) {
      var candidates = panel.querySelectorAll('[data-cutscene-focus-key]');
      for (var index = 0; index < candidates.length; index++) {
        if (candidates[index].getAttribute('data-cutscene-focus-key') !== snapshot.focus.key) continue;
        if (snapshot.focus.value != null &&
            selectHasValue(candidates[index], snapshot.focus.value)) {
          candidates[index].value = snapshot.focus.value;
        }
        try {
          candidates[index].focus({ preventScroll: true });
        } catch (error) {
          candidates[index].focus();
        }
        if (snapshot.focus.start != null && candidates[index].setSelectionRange) {
          candidates[index].setSelectionRange(snapshot.focus.start, snapshot.focus.end);
        }
        break;
      }
    }
    // Focus can move both a local scroller and the page, especially for a
    // native select on mobile. Restore positions after focus so it cannot win.
    panel.querySelectorAll('[data-cutscene-scroll]').forEach(function(element) {
      restoreScrollPosition(element,
        snapshot.scrolls[element.getAttribute('data-cutscene-scroll')]);
    });
    restoreScrollPosition(panel && panel.closest ? panel.closest('.content') : null,
      snapshot.viewport);
    restoreScrollPosition(pageScrollElement(), snapshot.page);
  }

  function notifyChange(state, label) {
    if (state.callbacks.onChange) state.callbacks.onChange(label);
    if (state.callbacks.onStatus) state.callbacks.onStatus(label);
  }

  function rerender(rom, state, preserve) {
    if (!state.ui || !state.ui.panel) return;
    var panel = state.ui.panel;
    var snapshot = preserve === false ? null : captureUi(panel);
    render(panel, rom, state.callbacks, snapshot);
  }

  function baselineDocument(state, scene) {
    var serialized = state.originalSerialized[scene.storageId];
    return typeof serialized === 'string'
      ? OB64.cutsceneModel.parseSceneDocument(JSON.parse(serialized)) : null;
  }

  function refreshExportRequirements(state, scene, document) {
    if (scene.engine !== 'director') {
      document.exportRequirements = {
        capability: 'needs-research',
        reasons: [scene.source.adapterStatus],
        allocationBytes: 0,
        features: ['presentation-adapter']
      };
      return;
    }
    var baseline = baselineDocument(state, scene);
    var source = state.sourceByAssetId[scene.assetId];
    if (!baseline || !source || !OB64.cutsceneExport) {
      document.exportRequirements.capability = 'needs-research';
      document.exportRequirements.reasons = [
        state.sourceErrors[scene.assetId] || 'The verified ROM source is unavailable.'
      ];
      document.exportRequirements.allocationBytes = 0;
      return;
    }
    try {
      var normalized = OB64.cutsceneModel.cloneSceneDocument(document);
      normalized.exportRequirements = OB64.cutsceneModel.cloneJson(
        baseline.exportRequirements, 'exportRequirements');
      if (OB64.cutsceneModel.serializeSceneDocument(normalized, 0) ===
          OB64.cutsceneModel.serializeSceneDocument(baseline, 0)) {
        document.exportRequirements = normalized.exportRequirements;
        return;
      }
      OB64.cutsceneModel.validateSceneDocument(document);
      var assessDelta = OB64.cutsceneExport.assessNativeDelta ||
        OB64.cutsceneExport.assessFixedSlotDelta;
      document.exportRequirements = assessDelta(
        scene, baseline, document, source);
    } catch (error) {
      document.exportRequirements = {
        capability: 'preview-only', reasons: [error && error.message || String(error)],
        allocationBytes: 0, features: ['director-fixed-slot']
      };
    }
  }

  function findClipRow(document, clipId) {
    if (!clipId) return null;
    for (var trackIndex = 0; trackIndex < document.tracks.length; trackIndex++) {
      var track = document.tracks[trackIndex];
      for (var clipIndex = 0; clipIndex < track.clips.length; clipIndex++) {
        if (track.clips[clipIndex].id === clipId) {
          return { track: track, clip: track.clips[clipIndex], trackIndex: trackIndex,
            clipIndex: clipIndex };
        }
      }
    }
    return null;
  }

  function executeEdit(rom, state, label, mutator) {
    var scene = selectedScene(state);
    var history = historyFor(state, scene);
    if (!history) return;
    OB64.cutsceneModel.execute(history, label, function(document) {
      mutator(document);
      OB64.cutsceneModel.validateSceneDocument(document);
      refreshExportRequirements(state, scene, document);
    });
    refreshRuntime(state, scene, history.present);
    var view = viewFor(state, scene.sceneId);
    var duration = OB64.cutscenePreview.sceneDurationFrames(history.present, view.pathId);
    view.frame = Math.min(view.frame, duration - 1);
    if (view.selectedActorId && !history.present.actors.some(function(actor) {
      return actor.id === view.selectedActorId;
    })) {
      view.selectedActorId = history.present.actors.length ? history.present.actors[0].id : null;
    }
    if (view.selectedClipId && !findClipRow(history.present, view.selectedClipId)) {
      view.selectedClipId = null;
    }
    notifyChange(state, label);
    rerender(rom, state);
  }

  function sceneHasChanges(state, scene) {
    var history = historyFor(state, scene);
    return !!history && OB64.cutsceneModel.serializeSceneDocument(history.present, 0) !==
      state.originalSerialized[scene.storageId];
  }

  function editCount(state) {
    return state.catalog.scenes.reduce(function(total, scene) {
      return total + (sceneHasChanges(state, scene) ? 1 : 0);
    }, 0);
  }

  function evictImages(state, protectedId) {
    while (state.imageCacheBytes > IMAGE_CACHE_LIMIT) {
      var candidates = Object.keys(state.imageCache).filter(function(assetId) {
        return assetId !== protectedId;
      }).sort(function(left, right) {
        return state.imageCache[left].lastUsed - state.imageCache[right].lastUsed;
      });
      if (!candidates.length) break;
      var remove = candidates[0];
      state.imageCacheBytes -= state.imageCache[remove].bytes;
      delete state.imageCache[remove];
    }
  }

  function imageSourceLabel(asset) {
    if (asset.sourceKind === 'lha-archive') return '#' + asset.archiveIndex;
    if (asset.sourceKind === 'rom-resource') {
      return 'Resource 0x' + Number(asset.source.resourceKey).toString(16).toUpperCase().padStart(8, '0');
    }
    if (asset.sourceKind === 'section-c-njpg') {
      return 'Section C ' + asset.assetId.split(':').pop();
    }
    return asset.sourceKind || 'Unresolved source';
  }

  function imageEvidenceHint(asset) {
    var hints = [];
    if (asset.unsupportedReason) hints.push(asset.unsupportedReason);
    if (asset.consumerEvidence) {
      hints.push(asset.consumerEvidence.consumerStatus + '. First missing join: ' +
        asset.consumerEvidence.firstMissingJoin + '.');
    }
    return hints.join(' ');
  }

  function loadImageBytes(rom, asset) {
    if (asset.sourceKind === 'lha-archive') {
      var archive = rom.archives && rom.archives[asset.archiveIndex];
      if (!archive) fail('Archive ' + asset.archiveIndex + ' is unavailable in this ROM.');
      return Promise.resolve(OB64.extractArchive(rom.z64, archive)).then(function(decoded) {
        return { bytes: decoded, expectedHash: asset.source.decodedSha256 };
      });
    }
    if (asset.sourceKind === 'rom-resource') {
      var resource = asset.source.compressionKind === 'ob64-custom-lz'
        ? OB64.art.readCompressedResource(rom.z64, asset.source.resourceKey)
        : OB64.art.readResource(rom.z64, asset.source.resourceKey);
      var decoded = asset.source.compressionKind === 'ob64-custom-lz'
        ? resource.decoded : resource.stored;
      return Promise.resolve({ bytes: decoded, expectedHash: asset.source.decodedSha256 });
    }
    if (asset.sourceKind === 'section-c-njpg') {
      var start = asset.source.z64Start;
      var end = asset.source.z64EndExclusive;
      if (!Number.isInteger(start) || !Number.isInteger(end) || start < 0 ||
          end <= start || end > rom.z64.length) {
        fail(asset.displayName + ' has an invalid Section C ROM extent.');
      }
      return Promise.resolve({
        bytes: rom.z64.slice(start, end),
        expectedHash: asset.source.storedSha256
      });
    }
    fail(asset.displayName + ' uses an unsupported image source.');
  }

  function decodeImageAsset(rom, state, asset) {
    var cached = state.imageCache[asset.assetId];
    if (cached) {
      cached.lastUsed = ++state.imageClock;
      return Promise.resolve(cached.result);
    }
    if (state.imageLoading[asset.assetId]) return state.imageLoading[asset.assetId];
    var promise = new Promise(function(resolve) { setTimeout(resolve, 0); }).then(function() {
      var sourceAssets = [asset];
      if (asset.compound && Array.isArray(asset.compound.members)) {
        if (!state.catalog || typeof state.catalog.getImageAsset !== 'function') {
          fail(asset.displayName + ' needs the Cutscene image catalog to load its members.');
        }
        sourceAssets = asset.compound.members.map(function(member) {
          var memberAsset = state.catalog.getImageAsset(member.assetId);
          if (!memberAsset) fail('Compound image member ' + member.assetId + ' is unavailable.');
          return memberAsset;
        });
      }
      return Promise.all(sourceAssets.map(function(sourceAsset) {
        return loadImageBytes(rom, sourceAsset).then(function(loaded) {
          return OB64.romCompatibility.sha256Hex(loaded.bytes).then(function(hash) {
            if (hash !== loaded.expectedHash) {
              fail(sourceAsset.displayName + ' does not match its catalogued source bytes.');
            }
            return {
              assetId: sourceAsset.assetId,
              image: OB64.cutsceneAssets.parseImageAsset(loaded.bytes, sourceAsset)
            };
          });
        });
      }));
    }).then(function(parsedMembers) {
        var parsed = asset.compound
          ? OB64.cutsceneAssets.composeCompoundImageAsset(asset, parsedMembers)
          : parsedMembers[0].image;
        var result = {
          asset: asset,
          container: parsed.container || asset.container,
          format: parsed.format == null ? asset.format : parsed.format,
          renderable: parsed.renderable,
          capability: parsed.capability,
          width: parsed.width,
          height: parsed.height,
          rgba: parsed.rgba,
          frames: parsed.frames || null,
          reference: parsed.reference ? {
            x: parsed.reference.x,
            y: parsed.reference.y
          } : null,
          originX: Number.isFinite(parsed.originX) ? parsed.originX : null,
          originY: Number.isFinite(parsed.originY) ? parsed.originY : null,
          compoundAssembled: parsed.compoundAssembled === true,
          warning: parsed.warnings && parsed.warnings.join(' ') || ''
        };
        var byteSize = result.rgba ? result.rgba.byteLength : 256;
        state.imageCache[asset.assetId] = {
          result: result, bytes: byteSize, lastUsed: ++state.imageClock
        };
        state.imageCacheBytes += byteSize;
        evictImages(state, asset.assetId);
        return result;
    }).finally(function() {
      delete state.imageLoading[asset.assetId];
    });
    state.imageLoading[asset.assetId] = promise;
    return promise;
  }

  function requestActiveEffects(rom, state, document, preview) {
    preview.effects.forEach(function(row) {
      if (row.payload.sourceSystem === 'combat-animation-preview' ||
          row.payload.sourceSystem === 'cutscene-sprite-native') return;
      var asset = row.payload.assetId && state.catalog.getImageAsset(row.payload.assetId);
      if (!asset || state.imageCache[asset.assetId] || state.imageLoading[asset.assetId]) return;
      decodeImageAsset(rom, state, asset).then(function() {
        if (selectedDocument(state) === document) paintStage(rom, state);
      }).catch(function(error) {
        state.sourceErrors['image:' + asset.assetId] = error && error.message || String(error);
      });
    });
  }

  function combatEffectAnimations(rom, state) {
    if (state.combatEffectAnimations) return state.combatEffectAnimations;
    var animationState = rom.art && rom.art.animations;
    if (!animationState || !animationState.supported) return [];
    state.combatEffectAnimations = animationState.specs.slice();
    return state.combatEffectAnimations;
  }

  function combatAnimationRoleSummary(animation) {
    if (animation.cutsceneRoleSummary) return animation.cutsceneRoleSummary;
    var present = { body: false, equipment: false, 'element-effect': false };
    animation.frames.forEach(function(frame) {
      frame.layers.forEach(function(layer) {
        var source = animation.artByKey && animation.artByKey[layer.sourceKey];
        var role = layer.sourceRole || source && source.sourceRole;
        if (Object.prototype.hasOwnProperty.call(present, role)) present[role] = true;
      });
    });
    var roles = ['body', 'equipment', 'element-effect'].filter(function(role) {
      return present[role];
    });
    animation.cutsceneRoleSummary = {
      roles: roles,
      body: present.body,
      equipment: present.equipment,
      elementEffect: present['element-effect'],
      label: roles.map(function(role) {
        return role === 'element-effect' ? 'element effect' : role;
      }).join(' + ') || 'unclassified'
    };
    return animation.cutsceneRoleSummary;
  }

  function combatAnimationMatchesFilter(animation, filterName) {
    var roles = combatAnimationRoleSummary(animation);
    if (filterName === 'body-only') {
      return roles.body && !roles.equipment && !roles.elementEffect;
    }
    if (filterName === 'equipment') return roles.equipment;
    if (filterName === 'element-effect') return roles.elementEffect;
    return true;
  }

  function combatAnimationIdentity(animation) {
    return {
      key: animation.key,
      classId: animation.spec.classId,
      className: animation.spec.className,
      actionId: animation.spec.actionId,
      actionName: animation.spec.actionName,
      variantLabel: animation.spec.variantLabel,
      roles: combatAnimationRoleSummary(animation).roles.slice()
    };
  }

  function combatEffectFrame(rom, state, row, preview) {
    var animationState = rom.art && rom.art.animations;
    var animation = animationState && animationState.byKey[row.payload.combatAnimationKey];
    if (!animation || !animation.frames.length || !OB64.animationUI) return null;
    var duration = animation.frames.reduce(function(total, frame) {
      return total + Math.max(1, frame.ticks);
    }, 0);
    var position = Math.max(0, preview.frame - row.startFrame) % duration;
    var frameIndex = 0;
    for (; frameIndex < animation.frames.length; frameIndex++) {
      var ticks = Math.max(1, animation.frames[frameIndex].ticks);
      if (position < ticks) break;
      position -= ticks;
    }
    frameIndex = Math.min(frameIndex, animation.frames.length - 1);
    var revision = Number(animationState.editRevision) || 0;
    var key = animation.key + ':' + frameIndex + ':' + revision;
    if (!state.combatEffectCache[key]) {
      state.combatEffectCache[key] = {
        width: animation.canvas.width,
        height: animation.canvas.height,
        rgba: OB64.animationUI.framePixels(
          animation, animation.frames[frameIndex], animationState, null, null, null)
      };
    }
    return state.combatEffectCache[key];
  }

  function activeEffectFrames(rom, state, preview) {
    return preview.effects.map(function(row) {
      if (row.payload.sourceSystem === 'cutscene-sprite-native') {
        if (!state.spriteState || row.payload.nativeLifetimeEmpty) return null;
        var cutsceneImage = OB64.cutsceneSprites.frameForActor(state.spriteState, {
          poseId: row.payload.poseId,
          bank: row.payload.bank,
          animationKey: row.payload.animationKey,
          nativeFacing: row.payload.nativeFacing,
          variantSelector: row.payload.variantSelector,
          poseFrame: Number.isFinite(row.payload.poseFrame)
            ? row.payload.poseFrame : Math.max(0, preview.frame - row.startFrame),
          poseLoop: row.payload.poseLoop,
          displayedFrameToken: Number.isInteger(row.payload.displayedFrameToken)
            ? row.payload.displayedFrameToken : null
        });
        return cutsceneImage ? {
          image: cutsceneImage,
          x: Number.isFinite(row.payload.stageX) ? row.payload.stageX : 160,
          y: Number.isFinite(row.payload.stageY) ? row.payload.stageY : 120,
          scale: Number.isFinite(row.payload.scale) ? row.payload.scale : 1,
          rotationDegrees: Number.isFinite(row.payload.rotationDegrees)
            ? row.payload.rotationDegrees : 0,
          anchorX: cutsceneImage.anchorX,
          anchorY: cutsceneImage.anchorY
        } : null;
      }
      if (row.payload.sourceSystem === 'combat-animation-preview') {
        var combatImage = combatEffectFrame(rom, state, row, preview);
        return combatImage ? {
          image: combatImage,
          x: Number.isFinite(row.payload.stageX) ? row.payload.stageX : 160,
          y: Number.isFinite(row.payload.stageY) ? row.payload.stageY : 120,
          scale: Number.isFinite(row.payload.scale) ? row.payload.scale : 1
        } : null;
      }
      var cached = state.imageCache[row.payload.assetId];
      if (!cached || !cached.result.renderable) return null;
      var result = cached.result;
      var frames = result.frames && result.frames.length ? result.frames : [result];
      var elapsed = Math.max(0, preview.frame - row.startFrame);
      return {
        image: frames[elapsed % frames.length],
        x: Number.isFinite(row.payload.stageX) ? row.payload.stageX : 160,
        y: Number.isFinite(row.payload.stageY) ? row.payload.stageY : 120,
        scale: Number.isFinite(row.payload.scale) ? row.payload.scale : 1
      };
    }).filter(Boolean);
  }

  function selectedBackgroundLayers(state, source) {
    var background = source && source.background ? source.background : source;
    if (!background) return [];
    var layers = Array.isArray(background.layers) && background.layers.length
      ? background.layers.filter(function(layer) { return layer.visible !== false; })
      : (background.assetId ? [{
        assetId: background.assetId, depth: 0, visible: true
      }] : []);
    return layers.slice().sort(function(left, right) {
      return (Number(left.depth) || 0) - (Number(right.depth) || 0);
    }).map(function(layer) {
      return { layer: layer, cached: state.imageCache[layer.assetId] || null };
    });
  }

  function selectedStageImageRows(state, source) {
    var rows = selectedBackgroundLayers(state, source);
    var vignetteAssetId = source && source.sceneVignette &&
      source.sceneVignette.sourceAssetId;
    if (vignetteAssetId && !rows.some(function(entry) {
      return entry.layer.assetId === vignetteAssetId;
    })) {
      rows.push({
        layer: {
          assetId: vignetteAssetId,
          role: 'scene-vignette-source',
          visible: true
        },
        cached: state.imageCache[vignetteAssetId] || null
      });
    }
    return rows;
  }

  function requestBackground(rom, state, document) {
    var preview = previewState(state);
    var layers = selectedStageImageRows(state, preview || document);
    if (!layers.length) return;
    var request = ++state.imageRequest;
    Promise.all(layers.map(function(entry) {
      var asset = state.catalog.getImageAsset(entry.layer.assetId);
      if (!asset) return Promise.resolve(null);
      return decodeImageAsset(rom, state, asset).catch(function(error) {
        state.sourceErrors['image:' + asset.assetId] = error && error.message || String(error);
        return null;
      });
    })).then(function() {
      if (request !== state.imageRequest || selectedDocument(state) !== document) return;
      paintStage(rom, state);
      updateBackgroundStatus(state);
    });
  }

  function previewState(state) {
    var scene = selectedScene(state);
    var document = selectedDocument(state);
    if (!scene || !document) return null;
    var view = viewFor(state, scene.sceneId);
    var preview = OB64.cutscenePreview.evaluateAtFrame(document, view.frame, { pathId: view.pathId });
    if (state.drag && state.drag.position) {
      preview.actors.forEach(function(actor) {
        if (actor.id === state.drag.actorId) {
          actor.x = state.drag.position.x;
          actor.y = state.drag.position.y;
          actor.z = state.drag.position.z;
        }
      });
    }
    return preview;
  }

  function paintStage(rom, state) {
    if (!state.ui || !state.ui.canvas) return;
    var scene = selectedScene(state);
    var document = selectedDocument(state);
    var preview = previewState(state);
    if (!scene || !document || !preview) return;
    var view = viewFor(state, scene.sceneId);
    var backgroundRows = selectedBackgroundLayers(state, preview);
    var stageImageRows = selectedStageImageRows(state, preview);
    if (stageImageRows.some(function(entry) {
      return !entry.cached && !state.imageLoading[entry.layer.assetId];
    })) requestBackground(rom, state, document);
    var backgrounds = backgroundRows.map(function(entry) {
      return entry.cached && entry.cached.result.renderable ? {
        image: entry.cached.result,
        layer: entry.layer
      } : null;
    }).filter(Boolean);
    requestActiveEffects(rom, state, document, preview);
    var actorFrames = state.spriteState
      ? OB64.cutsceneSprites.framesForPreview(state.spriteState, preview) : {};
    var backgroundProjection = preview.background && preview.background.projection ||
      document.background.projection;
    var vignetteAssetId = preview.sceneVignette &&
      preview.sceneVignette.sourceAssetId;
    var vignetteCached = vignetteAssetId && state.imageCache[vignetteAssetId];
    var scenePropFrames = state.spriteState
      ? OB64.cutsceneSprites.framesForStageProps(
        state.spriteState, backgroundProjection, preview.frame) : [];
    var rendered = OB64.cutsceneRenderer.renderFrame(document, preview, {
      backgrounds: backgrounds,
      backgroundProjection: backgroundProjection,
      projection: preview.actorProjection || null,
      actorFrames: actorFrames,
      scenePropFrames: scenePropFrames,
      effectFrames: activeEffectFrames(rom, state, preview),
      camera: preview.cameraState,
      overlays: preview.overlays || [],
      colorModulation: preview.sceneColor || null,
      screenTransition: preview.screenTransition || null,
      sceneVignette: preview.sceneVignette || null,
      sceneVignetteImage: vignetteCached && vignetteCached.result.renderable
        ? vignetteCached.result : null,
      oversizedImageView: preview.oversizedImageView || null,
      selectedActorId: view.selectedActorId
    });
    state.renderedStage = rendered;
    OB64.cutsceneRenderer.paintCanvas(state.ui.canvas, rendered, 2);
    if (state.ui.stageFrame) {
      var visibleActors = preview.actors.filter(function(actor) { return actor.visible; }).length;
      var spriteActors = Object.keys(actorFrames).length;
      var spriteFallbacks = state.spriteState ? preview.actors.filter(function(actor) {
        return actor.visible && actor.poseId && state.spriteState.errors[actor.poseId];
      }).length : 0;
      var appearanceFallbacks = Object.keys(actorFrames).reduce(function(total, actorId) {
        return total + (actorFrames[actorId].childFallbackCount || 0);
      }, 0);
      var clockLabel = preview.runtime ? 'Director tick ' : 'Preview frame ';
      var runtimeLabel = preview.runtime
        ? ' · runtime ' + preview.runtime.status.replace(/-/g, ' ') : '';
      state.ui.stageFrame.textContent = clockLabel + preview.frame + runtimeLabel +
        ' · display time ' + OB64.cutscenePreview.formatTime(preview.frame) +
        ' · sprite art ' + spriteActors + '/' + visibleActors +
        (scenePropFrames.length ? ' · native scene props ' + scenePropFrames.length : '') +
        (appearanceFallbacks ? ' · native appearance-0 layers ' + appearanceFallbacks : '') +
        (spriteFallbacks ? ' · schematic fallback ' + spriteFallbacks : '');
    }
    updateStageOverlay(rom, state, preview);
    updateTransport(state, preview);
  }

  function dialoguePortraitCanvas(rom, payload) {
    if (!payload.portraitAppearanceKey || !rom.art || !rom.art.avatar ||
        !rom.art.avatar.byKey[payload.portraitAppearanceKey] ||
        !OB64.art || !OB64.art.currentWords || !OB64.art.rgba5551) return null;
    var words = OB64.art.currentWords(rom.art, 'avatar', payload.portraitAppearanceKey);
    if (!words || words.length !== 40 * 48) return null;
    var canvas = node('canvas', 'cutscene-preview-portrait');
    canvas.width = 40;
    canvas.height = 48;
    var context = canvas.getContext('2d');
    var image = context.createImageData(40, 48);
    words.forEach(function(word, index) {
      image.data.set(OB64.art.rgba5551(word), index * 4);
    });
    context.putImageData(image, 0, 0);
    return canvas;
  }

  function updateStageOverlay(rom, state, preview) {
    if (!state.ui || !state.ui.stageOverlay) return;
    var overlay = state.ui.stageOverlay;
    overlay.innerHTML = '';
    if (preview.dialogue.length) {
      var dialogue = preview.dialogue[preview.dialogue.length - 1].payload;
      var box = node('div', 'cutscene-preview-dialogue');
      var portrait = dialoguePortraitCanvas(rom, dialogue);
      if (portrait) {
        box.classList.add('cutscene-preview-dialogue-has-portrait');
        box.appendChild(portrait);
      }
      var copy = node('div', 'cutscene-preview-dialogue-copy');
      if (dialogue.speaker) copy.appendChild(node('strong', '', dialogue.speaker));
      copy.appendChild(node('span', '', dialogue.text || 'Dialogue preview'));
      box.appendChild(copy);
      overlay.appendChild(box);
    }
    var cues = [];
    preview.audio.forEach(function(row) {
      cues.push('Sound · ' + (row.payload.cue || row.payload.assetId || 'unresolved cue'));
    });
    preview.effects.forEach(function(row) {
      if (row.payload.sourceSystem === 'cutscene-sprite-native') {
        cues.push('Native Cutscene sprite · bank ' + row.payload.bank +
          ' · animation ' + row.payload.animationKey +
          ' · facing ' + row.payload.nativeFacing);
      } else if (row.payload.sourceSystem === 'combat-animation-preview') {
        cues.push('Combat preview · ' +
          (row.payload.combatAnimationKey || 'unresolved action'));
      } else {
        cues.push('Archive effect preview · ' +
          (row.payload.assetId || row.payload.effect || 'unresolved effect'));
      }
    });
    preview.camera.forEach(function(row) {
      cues.push(row.payload && row.payload.presentationKind === 'projection-transform'
        ? 'Projection change' : 'Camera pose');
    });
    if (preview.cameraState && preview.cameraState.activeClipId) {
      cues.push('Stage projection · X ' + preview.cameraState.translateX.toFixed(1) +
        ' · Y ' + preview.cameraState.translateY.toFixed(1) +
        ' · Scale ' + preview.cameraState.scaleX.toFixed(2) + '×' +
        preview.cameraState.scaleY.toFixed(2));
    }
    if (cues.length) overlay.appendChild(node('div', 'cutscene-preview-cues', cues.join('  |  ')));
  }

  function updateTransport(state, preview) {
    if (!state.ui || !preview) return;
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    if (state.ui.scrubber) {
      state.ui.scrubber.max = Math.max(0, preview.durationFrames - 1);
      state.ui.scrubber.value = preview.frame;
    }
    if (state.ui.playButton) state.ui.playButton.textContent = state.ui.clock.playing ? 'Pause' : 'Play';
    if (state.ui.loopButton) state.ui.loopButton.setAttribute('aria-pressed', view.loop ? 'true' : 'false');
    if (state.ui.timelinePlayhead) {
      state.ui.timelinePlayhead.style.left = preview.frame / preview.durationFrames * 100 + '%';
    }
  }

  function updateBackgroundStatus(state) {
    if (!state.ui || !state.ui.backgroundStatus) return;
    var document = selectedDocument(state);
    var preview = document ? previewState(state) : null;
    var background = preview && preview.background || document && document.background;
    if (!background || !background.assetId) {
      var scene = selectedScene(state);
      var candidates = scene && scene.backgroundCandidateAssetIds || [];
      state.ui.backgroundStatus.textContent = scene && scene.backgroundAssociationStatus ||
        (candidates.length
          ? candidates.length + ' scene-table image candidate' + (candidates.length === 1 ? '' : 's') +
            ' located.'
          : 'No scene association is located. Choose any catalogued image to preview.');
      return;
    }
    var layers = selectedBackgroundLayers(state, background);
    var projection = background.projection || {};
    var nativeGroupLabel = background.capability === 'native' &&
        Number.isInteger(projection.selectedSelector)
      ? 'Native scene group ' + projection.selectedSelector + ' · ' : '';
    var hasEnvironmentBase = layers.some(function(entry) {
      return entry.layer.role === 'environment-base';
    });
    var foregroundOnly = layers.length && !hasEnvironmentBase && layers.every(function(entry) {
      return entry.layer.role === 'foreground-mask';
    });
    if (layers.length > 1) {
      var errors = layers.map(function(entry) {
        return state.sourceErrors['image:' + entry.layer.assetId];
      }).filter(Boolean);
      var decoded = layers.filter(function(entry) {
        return entry.cached && entry.cached.result.renderable;
      }).length;
      var calibratedStatus = projection.mode === 'b5-reference-capture'
        ? 'stored-frame scene registration · environment + correctly placed foreground pieces'
        : null;
      state.ui.backgroundStatus.textContent = errors.length
        ? errors[0]
        : (decoded < layers.length
          ? nativeGroupLabel + 'decoding ordered group · ' + decoded + '/' + layers.length + ' layers ready…'
          : (calibratedStatus || nativeGroupLabel + layers.length + ' ordered layers' +
            (foregroundOnly ? ' · foreground pieces only; the launch-time environment is not mapped'
              : '')));
      return;
    }
    var asset = state.catalog.getImageAsset(background.assetId);
    var cached = state.imageCache[background.assetId];
    var error = state.sourceErrors['image:' + background.assetId];
    if (error) state.ui.backgroundStatus.textContent = error;
    else if (!asset) state.ui.backgroundStatus.textContent = background.runtimeStatus ||
      'The runtime background selector has no catalogued image.';
    else if (!cached) state.ui.backgroundStatus.textContent = 'Decoding ' + asset.displayName + '…';
    else state.ui.backgroundStatus.textContent = cached.result.renderable
      ? nativeGroupLabel + asset.displayName + ' · ' + cached.result.width + '×' + cached.result.height +
        (foregroundOnly ? ' · foreground piece only; the launch-time environment is not mapped' : '') +
        (cached.result.warning ? ' · ' + cached.result.warning : '') +
        (asset.consumerEvidence ? ' · ' + asset.consumerEvidence.consumerStatus : '')
      : (cached.result.warning || asset.displayName + ' needs a decoder.');
  }

  function pauseAnimation(state) {
    if (state.animationFrame != null && window.cancelAnimationFrame) {
      window.cancelAnimationFrame(state.animationFrame);
    }
    state.animationFrame = null;
    state.lastAnimationTime = null;
  }

  function animationTick(rom, state, timestamp) {
    if (!state.ui || !state.ui.panel || !state.ui.panel.classList.contains('active')) {
      state.ui.clock = OB64.cutscenePreview.pause(state.ui.clock);
      pauseAnimation(state);
      return;
    }
    var elapsed = state.lastAnimationTime == null ? 0 : timestamp - state.lastAnimationTime;
    state.lastAnimationTime = timestamp;
    state.ui.clock = OB64.cutscenePreview.advance(state.ui.clock, elapsed);
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    view.frame = state.ui.clock.frame;
    paintStage(rom, state);
    if (state.ui.clock.playing && window.requestAnimationFrame) {
      state.animationFrame = window.requestAnimationFrame(function(next) {
        animationTick(rom, state, next);
      });
    } else {
      pauseAnimation(state);
    }
  }

  function startAnimation(rom, state) {
    pauseAnimation(state);
    state.lastAnimationTime = null;
    if (window.requestAnimationFrame) {
      state.animationFrame = window.requestAnimationFrame(function(timestamp) {
        animationTick(rom, state, timestamp);
      });
    }
  }

  function renderSceneBrowser(shell, rom, state) {
    var browser = node('aside', 'cutscene-browser');
    browser.setAttribute('data-cutscene-scroll', 'browser');
    browser.appendChild(node('h3', '', 'Scenes'));
    var search = node('input', 'cutscene-search');
    search.type = 'search'; search.placeholder = 'Search name, alias, or key';
    search.value = state.search;
    search.setAttribute('data-cutscene-focus-key', 'scene-search');
    search.addEventListener('input', function() {
      state.search = search.value;
      renderSceneRows(list, rom, state);
    });
    browser.appendChild(search);
    var count = node('div', 'cutscene-browser-count');
    browser.appendChild(count);
    var list = node('div', 'cutscene-scene-list');
    list.setAttribute('data-cutscene-scroll', 'scene-list');
    browser.appendChild(list);
    shell.appendChild(browser);
    state.ui.sceneCount = count;
    renderSceneRows(list, rom, state);
  }

  function renderSceneRows(list, rom, state) {
    var scroll = list.scrollTop;
    list.innerHTML = '';
    var scenes = state.catalog.searchScenes(state.search);
    state.ui.sceneCount.textContent = scenes.length + ' of ' + state.catalog.scenes.length +
      ' selectable scenes';
    scenes.forEach(function(scene) {
      var row = button('', 'cutscene-scene-row', function() {
        if (state.selectedSceneId === scene.sceneId) return;
        pauseAnimation(state);
        state.selectedSceneId = scene.sceneId;
        rerender(rom, state);
      });
      row.classList.toggle('active', scene.sceneId === state.selectedSceneId);
      row.setAttribute('data-cutscene-focus-key', 'scene:' + scene.sceneId);
      row.appendChild(node('strong', '', OB64.cutsceneCatalog.displayName(scene)));
      row.appendChild(node('span', '', scene.engine === 'director'
        ? (scene.friendlyName ? scene.technicalName : 'Key ' + scene.directorKey)
        : scene.engine + ' · native adapter unresolved'));
      var badges = node('span', 'cutscene-row-badges');
      badges.appendChild(capabilityBadge(scene.previewCapability, 'Visual'));
      if (sceneHasChanges(state, scene)) badges.appendChild(node('span', 'cutscene-edited-badge', 'Edited'));
      row.appendChild(badges);
      list.appendChild(row);
    });
    list.scrollTop = scroll;
  }

  function transportButton(label, action, title) {
    var output = button(label, 'btn-secondary cutscene-transport-button', action);
    if (title) output.setAttribute('aria-label', title);
    return output;
  }

  function renderStageArea(shell, rom, state, scene, document) {
    var center = node('main', 'cutscene-main');
    var heading = node('div', 'cutscene-heading');
    var copy = node('div');
    copy.appendChild(node('h2', '', OB64.cutsceneCatalog.displayName(scene)));
    var captureStages = Array.isArray(document.identity.captures)
      ? document.identity.captures.filter(function(capture) {
        return capture.stageLabel;
      }) : [];
    var captureStageCount = captureStages.length;
    copy.appendChild(node('p', '', scene.technicalName +
      (scene.engine === 'director'
        ? (captureStageCount ? ' · ' + captureStageCount + ' captured presentation stages' : '') +
          ' · Preview clock: 30 frames/second · Native waits remain integer ticks'
        : ' · Visual Storyboard workspace · Native presentation adapter unresolved')));
    heading.appendChild(copy);
    var badges = node('div', 'cutscene-heading-badges');
    badges.appendChild(capabilityBadge(scene.previewCapability, 'Visual'));
    badges.appendChild(capabilityBadge(document.exportRequirements.capability, 'ROM'));
    heading.appendChild(badges);
    center.appendChild(heading);

    if (state.sourceErrors[scene.assetId]) {
      center.appendChild(node('div', 'cutscene-source-warning',
        'ROM source unavailable: ' + state.sourceErrors[scene.assetId]));
    }
    if (document.exportRequirements.reasons.length) {
      center.appendChild(node('div', document.exportRequirements.capability === 'native'
        ? 'cutscene-export-note' : 'cutscene-source-warning',
      document.exportRequirements.reasons.join(' ')));
    }
    var stagePanel = node('section', 'cutscene-stage-panel');
    var canvasWrap = node('div', 'cutscene-stage-wrap');
    var canvas = node('canvas', 'cutscene-stage');
    canvas.width = 320; canvas.height = 240; canvas.tabIndex = 0;
    canvas.setAttribute('aria-label', 'Cutscene Stage. Select or drag actors.');
    canvas.setAttribute('data-cutscene-focus-key', 'stage');
    canvasWrap.appendChild(canvas);
    var stageOverlay = node('div', 'cutscene-stage-overlay');
    stageOverlay.setAttribute('aria-live', 'polite');
    canvasWrap.appendChild(stageOverlay);
    stagePanel.appendChild(canvasWrap);
    var stageFooter = node('div', 'cutscene-stage-footer');
    var frame = node('span', 'cutscene-stage-frame');
    stageFooter.appendChild(frame);
    var projection = document.background && document.background.projection || {};
    var actorProjection = projection.actorProjection || null;
    var calibrated = projection.calibrationStatus === 'capture-calibrated' &&
      actorProjection && actorProjection.mode === 'native-perspective-capture';
    var stageRuntime = state.runtimeByAssetId[scene.assetId] || null;
    var nativeRuntime = stageRuntime && stageRuntime.states.length &&
      stageRuntime.states[0].actorProjection &&
      stageRuntime.states[0].actorProjection.mode === 'native-perspective-runtime';
    var launchActorCamera = scene.launchProfile && scene.launchProfile.cameras &&
      scene.launchProfile.cameras.actor || null;
    var calibrationText;
    if (nativeRuntime && stageRuntime.directorMode === 0) {
      calibrationText = 'Native mode-zero staging: registered-camera Actor prepass → centered scene transform → Actor camera.';
    } else if (nativeRuntime && stageRuntime.directorMode === 2 &&
        launchActorCamera && launchActorCamera.evidenceStatus === 'external-unresolved') {
      calibrationText = 'Mode-two runtime active; the unresolved launch Actor camera uses a fit-to-scene preview.';
    } else if (nativeRuntime && stageRuntime.directorMode === 2) {
      calibrationText = 'Native mode-two Actor camera and scene-projection staging.';
    } else if (nativeRuntime) {
      calibrationText = stageRuntime.states[0].actorProjection.evidenceStatus ===
        'external-unresolved'
        ? 'The unresolved launch Actor camera uses a fit-to-scene preview.'
        : 'Native Director Actor-camera staging.';
    } else if (calibrated) {
      calibrationText = 'Native actor projection and B5 layering match the stored Graduation capture.';
    } else {
      calibrationText = 'Fit-to-scene storyboard projection; no compiled Director runtime is available.';
    }
    if (nativeRuntime && stageRuntime.missingInputs.length) {
      calibrationText += ' ' + stageRuntime.missingInputs.length +
        ' launch inputs remain unresolved because they live outside this stream.';
    }
    var calibration = node('span', 'cutscene-stage-calibration', calibrationText);
    calibration.title = nativeRuntime
      ? [stageRuntime.directorModeStatus,
        stageRuntime.states[0].actorProjection.source].filter(Boolean).join(' ')
      : (calibrated ? [projection.calibrationResult,
        actorProjection.calibrationResult].filter(Boolean).join(' ') : '');
    stageFooter.appendChild(calibration);
    stagePanel.appendChild(stageFooter);
    center.appendChild(stagePanel);

    var view = viewFor(state, scene.sceneId);
    var duration = OB64.cutscenePreview.sceneDurationFrames(document, view.pathId);
    var controls = node('div', 'cutscene-transport');
    var back = transportButton('−1', function() {
      state.ui.clock = OB64.cutscenePreview.step(state.ui.clock, -1);
      view.frame = state.ui.clock.frame; paintStage(rom, state);
    }, 'Previous frame');
    var play = transportButton('Play', function() {
      state.ui.clock = state.ui.clock.playing
        ? OB64.cutscenePreview.pause(state.ui.clock) : OB64.cutscenePreview.play(state.ui.clock);
      if (state.ui.clock.playing) startAnimation(rom, state);
      else { pauseAnimation(state); paintStage(rom, state); }
    });
    var forward = transportButton('+1', function() {
      state.ui.clock = OB64.cutscenePreview.step(state.ui.clock, 1);
      view.frame = state.ui.clock.frame; paintStage(rom, state);
    }, 'Next frame');
    var loop = transportButton('Loop', function() {
      view.loop = !view.loop;
      state.ui.clock = OB64.cutscenePreview.setLoop(state.ui.clock, view.loop);
      updateTransport(state, previewState(state));
    });
    loop.setAttribute('aria-pressed', view.loop ? 'true' : 'false');
    controls.appendChild(back); controls.appendChild(play); controls.appendChild(forward); controls.appendChild(loop);
    var scrubber = node('input', 'cutscene-scrubber');
    scrubber.type = 'range'; scrubber.min = 0; scrubber.max = Math.max(0, duration - 1);
    scrubber.step = 1; scrubber.value = Math.min(view.frame, duration - 1);
    scrubber.setAttribute('aria-label', 'Preview playhead');
    scrubber.setAttribute('data-cutscene-focus-key', 'playhead');
    scrubber.addEventListener('input', function() {
      state.ui.clock = OB64.cutscenePreview.seek(state.ui.clock, Number(scrubber.value));
      view.frame = state.ui.clock.frame; paintStage(rom, state);
    });
    controls.appendChild(scrubber);
    center.appendChild(controls);

    if (captureStages.length) {
      var captureNavigation = node('div', 'cutscene-capture-navigation');
      captureNavigation.appendChild(node('span', 'cutscene-capture-label', 'Stored captures'));
      var captureButtons = [];
      captureStages.forEach(function(capture) {
        var exactFrame = Number.isInteger(capture.previewFrame);
        var captureButton = button(capture.stageLabel, 'btn-secondary cutscene-capture-button',
          exactFrame ? function() {
            pauseAnimation(state);
            var target = Math.max(0, Math.min(capture.previewFrame, duration - 1));
            view.frame = target;
            if (state.ui.clock) {
              state.ui.clock = OB64.cutscenePreview.pause(
                OB64.cutscenePreview.seek(state.ui.clock, target));
            }
            captureButtons.forEach(function(entry) {
              entry.button.setAttribute('aria-pressed',
                entry.capture.previewFrame === target ? 'true' : 'false');
            });
            paintStage(rom, state);
          } : function() {});
        captureButton.disabled = !exactFrame;
        captureButton.setAttribute('aria-pressed', exactFrame && view.frame === capture.previewFrame
          ? 'true' : 'false');
        captureButton.setAttribute('data-cutscene-focus-key',
          'capture:' + (capture.stageId || capture.stageLabel));
        captureButton.title = exactFrame
          ? (capture.previewTimingStatus || 'Exact preview-frame association.')
          : (capture.dialogueAssociation
            ? capture.dialogueAssociation.speaker + ' · Serifu selector ' +
              capture.dialogueAssociation.archiveSelector + ', entry ' +
              capture.dialogueAssociation.entrySelector + '. ' +
              capture.dialogueAssociation.timelineStatus
            : 'Stored capture retained as evidence; its exact native timeline position is unresolved.');
        captureButtons.push({ button: captureButton, capture: capture });
        captureNavigation.appendChild(captureButton);
      });
      center.appendChild(captureNavigation);
    }

    var viewControls = node('div', 'cutscene-view-controls');
    var pathSelect = node('select');
    document.branches.forEach(function(branch) {
      var option = node('option', '', branch.label); option.value = branch.id;
      pathSelect.appendChild(option);
    });
    pathSelect.value = view.pathId;
    pathSelect.setAttribute('data-cutscene-focus-key', 'preview-path');
    pathSelect.addEventListener('change', function() {
      view.pathId = pathSelect.value;
      view.frame = 0;
      rerender(rom, state);
    });
    viewControls.appendChild(field('Preview path', pathSelect,
      document.branches.length > 1 ? 'Conditions remain unresolved; paths show explicit alternatives.' : 'Default path'));
    var snapSelect = node('select');
    [['frame', '1 preview frame'], ['tenth', '0.1 preview second'],
      ['half', '0.5 preview second'], ['second', '1 preview second']].forEach(function(entry) {
      var option = node('option', '', entry[1]); option.value = entry[0]; snapSelect.appendChild(option);
    });
    snapSelect.value = view.snap;
    snapSelect.setAttribute('data-cutscene-focus-key', 'timeline-snap');
    snapSelect.addEventListener('change', function() { view.snap = snapSelect.value; });
    viewControls.appendChild(field('Snap', snapSelect));
    var zoomSelect = node('select');
    [[0.25, '25%'], [0.5, '50%'], [1, '100%'], [2, '200%'], [4, '400%'],
      [8, '800%']].forEach(function(entry) {
      var option = node('option', '', entry[1]); option.value = entry[0]; zoomSelect.appendChild(option);
    });
    zoomSelect.value = String(view.zoom);
    zoomSelect.setAttribute('data-cutscene-focus-key', 'timeline-zoom');
    zoomSelect.addEventListener('change', function() {
      view.zoom = Number(zoomSelect.value); rerender(rom, state);
    });
    viewControls.appendChild(field('Timeline zoom', zoomSelect));
    var budget = node('div', 'cutscene-capacity');
    if (scene.engine === 'director') {
      var source = state.sourceByAssetId[scene.assetId];
      var used = document.exportRequirements.allocationBytes ||
        source && source.consumedEncodedBytes || 0;
      var capacity = scene.source.storedPayloadLength;
      var relocated = (document.exportRequirements.features || [])
        .indexOf('director-relocation-required') !== -1;
      budget.appendChild(node('strong', '', relocated
        ? 'Native relocated payload' : 'Native compressed budget'));
      budget.appendChild(node('span', '', used ? (relocated
        ? used + ' bytes · exceeds the ' + capacity + '-byte retail slot'
        : used + ' / ' + capacity + ' bytes · ' +
          Math.max(0, capacity - used) + ' free') : 'Measured after the first edit'));
      var meter = node('meter');
      meter.min = 0; meter.max = Math.max(capacity, used); meter.value = used;
      budget.appendChild(meter);
    } else {
      budget.appendChild(node('strong', '', 'Native presentation adapter'));
      budget.appendChild(node('span', '', scene.source.adapterStatus));
    }
    viewControls.appendChild(budget);
    center.appendChild(viewControls);

    renderTimeline(center, rom, state, scene, document);
    shell.appendChild(center);
    state.ui.canvas = canvas;
    state.ui.stageOverlay = stageOverlay;
    state.ui.stageFrame = frame;
    state.ui.scrubber = scrubber;
    state.ui.playButton = play;
    state.ui.loopButton = loop;
    state.ui.clock = OB64.cutscenePreview.createClock(duration, {
      frame: Math.min(view.frame, duration - 1), loop: view.loop
    });
    installStagePointer(canvas, rom, state);
  }

  function clipLabel(row, document) {
    var actor = row.track.actorId && document.actors.find(function(candidate) {
      return candidate.id === row.track.actorId;
    });
    var who = actor ? actor.label : 'Scene';
    var seconds = (row.clip.durationFrames / 30).toFixed(2).replace(/\.00$/, '');
    var semanticLabel = row.clip.payload.semanticLabel || row.clip.payload.displayLabel || null;
    if (row.clip.kind === 'pose') return who + ' · ' + (semanticLabel || 'Set pose');
    if (row.clip.kind === 'movement') return who + ' · ' + (semanticLabel || 'Move') +
      ' for ' + seconds + ' preview s';
    if (row.clip.kind === 'wait') return 'Hold for ' + seconds + ' preview s';
    if (row.clip.kind === 'dialogue') return who + ' · Speak for ' + seconds + ' preview s';
    if (row.clip.kind === 'audio') return 'Play sound · ' + seconds + ' preview s';
    if (row.clip.kind === 'effect') return row.clip.payload.sourceSystem ===
        'combat-animation-preview'
      ? 'Battle action · ' + seconds + ' preview s'
      : 'Effect · ' + seconds + ' preview s';
    if (row.clip.kind === 'effect-remove') return row.clip.payload.displayLabel || 'Remove sprite effect';
    if (row.clip.kind === 'branch') return 'Conditional flow';
    if (row.clip.kind === 'camera') {
      return row.clip.payload.displayLabel ||
        (row.clip.payload.presentationKind === 'projection-transform'
          ? 'Animate scene projection' : 'Set camera pose');
    }
    if (row.clip.kind === 'end') return 'End scene';
    if (row.clip.kind === 'enter') return who + ' · Enter';
    if (row.clip.kind === 'exit') return who + ' · Exit';
    return who + ' · ' + row.clip.kind;
  }

  function orderedClips(document) {
    var rows = [];
    document.tracks.forEach(function(track, trackIndex) {
      track.clips.forEach(function(clip, clipIndex) {
        rows.push({ track: track, clip: clip, trackIndex: trackIndex, clipIndex: clipIndex });
      });
    });
    return rows.sort(function(left, right) {
      return left.clip.startFrame - right.clip.startFrame ||
        left.trackIndex - right.trackIndex || left.clipIndex - right.clipIndex;
    });
  }

  function selectClip(rom, state, scene, row) {
    var view = viewFor(state, scene.sceneId);
    view.selectedClipId = row.clip.id;
    view.frame = row.clip.startFrame;
    if (row.track.actorId) view.selectedActorId = row.track.actorId;
    rerender(rom, state);
  }

  function installTimelineDrag(element, rom, state, scene, row, duration, surfaceWidth) {
    element.addEventListener('pointerdown', function(event) {
      if (event.button !== 0) return;
      var mode = event.target && event.target.getAttribute('data-cutscene-resize') === 'end'
        ? 'resize' : 'move';
      var view = viewFor(state, scene.sceneId);
      view.selectedClipId = row.clip.id;
      if (row.track.actorId) view.selectedActorId = row.track.actorId;
      var startX = event.clientX;
      var originalStart = row.clip.startFrame;
      var originalDuration = row.clip.durationFrames;
      var moved = false;
      element.classList.add('dragging');
      event.preventDefault();

      function deltaFrames(clientX) {
        var raw = (clientX - startX) * duration / surfaceWidth;
        var snapped = OB64.cutscenePreview.snapFrame(Math.abs(raw), view.snap);
        return raw < 0 ? -snapped : snapped;
      }
      function move(moveEvent) {
        var delta = deltaFrames(moveEvent.clientX);
        moved = moved || Math.abs(moveEvent.clientX - startX) >= 3;
        if (mode === 'resize') {
          var minimum = row.clip.durationFrames === 0 ? 0 : 1;
          var nextDuration = Math.max(minimum, originalDuration + delta);
          element.style.width = Math.max(1.5, Math.max(1, nextDuration) / duration * 100) + '%';
        } else {
          var nextStart = Math.max(0, originalStart + delta);
          element.style.left = nextStart / duration * 100 + '%';
          view.frame = nextStart;
          if (state.ui.clock) state.ui.clock = OB64.cutscenePreview.seek(state.ui.clock,
            Math.min(nextStart, state.ui.clock.durationFrames - 1));
          paintStage(rom, state);
        }
      }
      function finish(upEvent, cancelled) {
        window.removeEventListener('pointermove', move);
        window.removeEventListener('pointerup', up);
        window.removeEventListener('pointercancel', cancel);
        element.classList.remove('dragging');
        element._cutsceneDragged = moved;
        if (cancelled || !moved) return;
        var delta = deltaFrames(upEvent.clientX);
        executeEdit(rom, state, mode === 'resize' ? 'Resize Cutscene clip' : 'Move Cutscene clip',
          function(document) {
            var next = findClipRow(document, row.clip.id);
            if (!next) return;
            if (mode === 'resize') {
              var minimum = next.clip.durationFrames === 0 ? 0 : 1;
              next.clip.durationFrames = Math.max(minimum, originalDuration + delta);
              if (next.clip.kind === 'movement') next.clip.payload.durationMode = 'duration';
            } else {
              next.clip.startFrame = Math.max(0, originalStart + delta);
              if (!next.clip.source.nodeId && next.clip.source.insertBeforeNodeId) {
                var boundary = boundaryForFrame(scene, document, next.clip.startFrame);
                next.clip.source.insertBeforeNodeId = boundary && boundary.id ||
                  next.clip.source.insertBeforeNodeId;
              }
            }
          });
      }
      function up(upEvent) { finish(upEvent, false); }
      function cancel(cancelEvent) { finish(cancelEvent, true); }
      window.addEventListener('pointermove', move);
      window.addEventListener('pointerup', up);
      window.addEventListener('pointercancel', cancel);
    });
  }

  function directorSourceRows(program, mode) {
    if (mode === 'native') {
      return program.primitives.map(function(primitive) {
        return {
          id: primitive.id,
          kind: 'primitive',
          label: primitive.label,
          summary: primitive.summary,
          category: primitive.category,
          confidence: primitive.confidence,
          clock: primitive.clock,
          startWord: primitive.startWord,
          endWord: primitive.endWord,
          wordCount: primitive.wordCount,
          nodeCount: 1,
          nodeIds: [primitive.id],
          nativeTicks: null,
          editable: primitive.editPolicy !== 'preserve-native'
        };
      });
    }
    return program.composites;
  }

  function directorSourceSelection(program, rows, view) {
    var selected = rows.find(function(row) { return row.id === view.selectedSourceId; });
    if (!selected) {
      selected = rows[0] || null;
      view.selectedSourceId = selected ? selected.id : null;
    }
    return selected;
  }

  function directorEditorTarget(document, program, row) {
    if (!document || !program || !row || !Array.isArray(row.nodeIds)) return null;
    var owned = {};
    row.nodeIds.forEach(function(nodeId) { owned[nodeId] = true; });
    for (var commandIndex = 0; commandIndex < document.native.commands.length; commandIndex++) {
      var command = document.native.commands[commandIndex];
      if (!command.source || !owned[command.source.nodeId]) continue;
      var clipIds = command.source.projectedClipIds || [];
      for (var clipIndex = 0; clipIndex < clipIds.length; clipIndex++) {
        var clipRow = findClipRow(document, clipIds[clipIndex]);
        if (!clipRow || clipRow.clip.capability !== 'native') continue;
        return {
          kind: 'clip',
          clipId: clipRow.clip.id,
          actorId: clipRow.track.actorId,
          label: clipRow.clip.payload.displayLabel || clipRow.track.label || clipRow.clip.kind
        };
      }
    }
    var actor = document.actors.find(function(candidate) {
      return candidate.source && owned[candidate.source.placeNodeId] &&
        candidate.source.editPolicy === 'typed-place';
    });
    if (actor) {
      return { kind: 'actor', actorId: actor.id, clipId: null, label: actor.label };
    }
    var background = row.nodeIds.some(function(nodeId) {
      var primitive = program.primitiveById[nodeId];
      return primitive && primitive.editPolicy === 'typed-background-group';
    });
    if (background && document.background.projection &&
        document.background.projection.nativeEditable === true) {
      return { kind: 'background', actorId: null, clipId: null, label: 'Background picker' };
    }
    return null;
  }

  function renderDirectorSourceDetails(section, program, selected, scene, catalog) {
    if (!selected) return;
    var details = node('section', 'cutscene-director-details');
    var heading = node('div', 'cutscene-director-details-heading');
    heading.appendChild(node('h4', '', selected.label));
    heading.appendChild(node('span', 'cutscene-source-confidence', selected.confidence));
    details.appendChild(heading);
    details.appendChild(node('p', '', selected.summary));
    var facts = node('dl', 'cutscene-source-summary');
    [
      ['Source words', selected.startWord + '..' + selected.endWord +
        ' (' + selected.wordCount + ')'],
      ['Native commands', String(selected.nodeCount)],
      ['Clock', selected.clock],
      ['Editing', selected.editorTarget
        ? 'Native fields available in the Inspector'
        : 'Source inspection only'],
      ['Native duration', selected.nativeTicks == null
        ? 'No single duration' : selected.nativeTicks + ' registered-counter updates']
    ].forEach(function(entry) {
      facts.appendChild(node('dt', '', entry[0]));
      facts.appendChild(node('dd', '', entry[1]));
    });
    details.appendChild(facts);
    if (Number.isInteger(selected.runtimeStartTick)) {
      var runtimeFacts = node('p', 'cutscene-director-edit-hint',
        'Executed at Director tick ' + selected.runtimeStartTick +
        ' · active span ' + selected.runtimeDurationTicks + ' scheduler update' +
        (selected.runtimeDurationTicks === 1 ? '' : 's') + '.');
      details.appendChild(runtimeFacts);
    }
    if (selected.editorTarget) {
      details.appendChild(node('p', 'cutscene-director-edit-hint',
        'Inspector target: ' + selected.editorTarget.label +
        '. Edits change that preserved native command boundary; other commands in this action remain intact.'));
    }
    if (selected.category === 'dialogue') {
      var dialogueEvidence = node('div', 'cutscene-director-dialogue-evidence');
      dialogueEvidence.appendChild(node('h5', '', 'Separate Serifu presentation'));
      var associations = scene.dialogueAssociations || [];
      if (!associations.length) {
        dialogueEvidence.appendChild(node('p', 'cutscene-field-hint',
          'The Director commands own the dialogue-window lifecycle. No exact Serifu text selector is mapped to this stream yet.'));
      } else {
        associations.forEach(function(association) {
          var entry = catalog && catalog.getDialogueEntry
            ? catalog.getDialogueEntry(association.entryId) : null;
          dialogueEvidence.appendChild(node('p', '',
            (association.speaker || entry && entry.speakerLabel || 'Speaker') + ': ' +
            (entry && entry.text || association.entryId)));
          dialogueEvidence.appendChild(node('small', 'cutscene-field-hint',
            'Scene-level evidence only · Serifu selector ' + association.archiveSelector +
            ', entry ' + association.entrySelector + '. The exact Director action binding remains separate.'));
        });
      }
      details.appendChild(dialogueEvidence);
    }
    var nativeList = node('div', 'cutscene-native-command-list');
    selected.nodeIds.forEach(function(nodeId) {
      var primitive = program.primitiveById[nodeId];
      if (!primitive) return;
      var row = node('article', 'cutscene-native-command');
      var nativeHeading = node('div', 'cutscene-native-command-heading');
      nativeHeading.appendChild(node('code', '', primitive.opcodeHex));
      nativeHeading.appendChild(node('strong', '', primitive.label));
      nativeHeading.appendChild(node('span', '', 'word ' + primitive.startWord));
      row.appendChild(nativeHeading);
      row.appendChild(node('p', '', primitive.summary));
      if (primitive.query) {
        row.appendChild(node('p', 'cutscene-native-operands',
          primitive.query.recordKind + ' · compare ' + primitive.query.compareMode +
          ' · target ' + primitive.query.target +
          (primitive.query.producerInput == null ? '' :
            ' · producer input ' + primitive.query.producerInput)));
      } else if (primitive.operands.length) {
        row.appendChild(node('p', 'cutscene-native-operands',
          primitive.operands.map(function(operand) {
            return operand.role + ' = ' + operand.signed;
          }).join(' · ')));
      }
      row.appendChild(node('code', 'cutscene-native-words',
        primitive.rawWords.map(function(word) {
          return '0x' + word.toString(16).toUpperCase().padStart(8, '0');
        }).join(' ')));
      nativeList.appendChild(row);
    });
    details.appendChild(nativeList);
    section.appendChild(details);
  }

  function renderDirectorSource(section, rom, state, scene, document, program, mode) {
    var view = viewFor(state, scene.sceneId);
    var rows = directorSourceRows(program, mode).map(function(row) {
      var editorTarget = directorEditorTarget(document, program, row);
      return Object.assign({}, row, {
        editorTarget: editorTarget,
        editable: !!editorTarget
      });
    });
    var selected = directorSourceSelection(program, rows, view);
    var summary = node('div', 'cutscene-director-source-summary');
    summary.appendChild(node('strong', '', mode === 'native'
      ? program.stats.primitiveCount + ' exact native commands'
      : program.stats.compositeCount + ' editor actions · ' +
        program.stats.multiPrimitiveCompositeCount + ' multi-command bundles'));
    summary.appendChild(node('span', '',
      program.wordCount + ' source words · ' + program.stats.registeredWaitCount +
      ' registered waits · ' + program.stats.skippableWaitCount + ' A-skippable waits'));
    summary.appendChild(node('small', '',
      'Horizontal position is physical Director-stream order. It is not elapsed time.'));
    section.appendChild(summary);

    var legend = node('div', 'cutscene-director-legend');
    program.lanes.forEach(function(lane) {
      legend.appendChild(node('span', 'cutscene-director-legend-' + lane,
        OB64.cutsceneDirector.laneLabels[lane]));
    });
    section.appendChild(legend);

    var viewport = node('div', 'cutscene-director-source');
    viewport.setAttribute('data-cutscene-scroll', 'director-source');
    var surface = node('div', 'cutscene-director-source-surface');
    var pixelsPerWord = mode === 'native' ? 8 : 6;
    var surfaceWidth = Math.max(900, program.wordCount * pixelsPerWord);
    var laneHeight = 42;
    surface.style.width = surfaceWidth + 'px';
    surface.style.height = (32 + program.lanes.length * laneHeight) + 'px';
    var ruler = node('div', 'cutscene-director-word-ruler');
    for (var markIndex = 0; markIndex <= 10; markIndex++) {
      var word = Math.round(program.wordCount * markIndex / 10);
      var mark = node('span', '', 'word ' + word);
      mark.style.left = markIndex * 10 + '%';
      ruler.appendChild(mark);
    }
    surface.appendChild(ruler);
    program.lanes.forEach(function(lane, laneIndex) {
      var laneRow = node('div', 'cutscene-director-lane cutscene-director-lane-' + lane);
      laneRow.style.top = (28 + laneIndex * laneHeight) + 'px';
      laneRow.style.height = laneHeight + 'px';
      surface.appendChild(laneRow);
    });
    rows.forEach(function(row) {
      var laneIndex = program.lanes.indexOf(row.category);
      if (laneIndex < 0) laneIndex = program.lanes.length - 1;
      var block = button(row.label,
        'cutscene-director-block cutscene-director-block-' + row.category, function() {
          view.selectedSourceId = row.id;
          view.selectedClipId = null;
          if (row.editorTarget) {
            view.selectedClipId = row.editorTarget.clipId;
            if (row.editorTarget.actorId) view.selectedActorId = row.editorTarget.actorId;
          }
          rerender(rom, state);
        });
      block.style.left = row.startWord / program.wordCount * 100 + '%';
      block.style.top = (33 + laneIndex * laneHeight) + 'px';
      block.style.width = Math.max(4, row.wordCount * pixelsPerWord) + 'px';
      block.classList.toggle('selected', selected && selected.id === row.id);
      block.setAttribute('aria-pressed', selected && selected.id === row.id ? 'true' : 'false');
      block.setAttribute('data-cutscene-focus-key', 'source:' + row.id);
      block.title = row.label + ' · words ' + row.startWord + '..' + row.endWord +
        ' · ' + row.nodeCount + ' native command' + (row.nodeCount === 1 ? '' : 's');
      surface.appendChild(block);
    });
    viewport.appendChild(surface);
    section.appendChild(viewport);
    renderDirectorSourceDetails(section, program, selected, scene, state.catalog);
  }

  function renderDirectorRuntime(section, rom, state, scene, document, program, runtime) {
    var view = viewFor(state, scene.sceneId);
    var entries = runtime.trace.filter(function(entry) { return entry.kind === 'composite'; });
    var rows = entries.map(function(entry) {
      var composite = program.compositeById[entry.compositeId];
      var duration = 1;
      var startState = runtime.states[entry.tick];
      if (startState && startState.runtime.blockLabel === composite.label) {
        var endTick = entry.tick + 1;
        while (endTick < runtime.states.length &&
            runtime.states[endTick].runtime.blockLabel === composite.label) endTick++;
        duration = Math.max(1, endTick - entry.tick);
      }
      var editorTarget = directorEditorTarget(document, program, composite);
      return Object.assign({}, composite, {
        runtimeStartTick: entry.tick,
        runtimeDurationTicks: duration,
        editorTarget: editorTarget,
        editable: !!editorTarget
      });
    });
    var selected = directorSourceSelection(program, rows, view);
    var summary = node('div', 'cutscene-director-source-summary');
    summary.appendChild(node('strong', '', runtime.durationTicks + ' Director scheduler updates'));
    summary.appendChild(node('span', '', entries.length + ' executed composite actions · ' +
      runtime.executedPrimitiveCount + ' dispatched visual/state commands'));
    summary.appendChild(node('small', '',
      'Horizontal position is execution time. Waits and completion gates occupy their scheduler duration.'));
    summary.appendChild(node('small', '', 'Runtime inputs · ' + runtime.assumptions.length +
      ' assumptions · ' + runtime.missingInputs.length + ' missing launch/resource inputs'));
    section.appendChild(summary);

    var legend = node('div', 'cutscene-director-legend');
    program.lanes.forEach(function(lane) {
      legend.appendChild(node('span', 'cutscene-director-legend-' + lane,
        OB64.cutsceneDirector.laneLabels[lane]));
    });
    section.appendChild(legend);

    var viewport = node('div', 'cutscene-director-source');
    viewport.setAttribute('data-cutscene-scroll', 'director-runtime');
    var surface = node('div', 'cutscene-director-source-surface');
    var surfaceWidth = Math.max(900, runtime.durationTicks);
    var laneHeight = 42;
    surface.style.width = surfaceWidth + 'px';
    surface.style.height = (32 + program.lanes.length * laneHeight) + 'px';
    var ruler = node('div', 'cutscene-director-word-ruler');
    for (var markIndex = 0; markIndex <= 10; markIndex++) {
      var tick = Math.round(runtime.durationTicks * markIndex / 10);
      var mark = node('span', '', 'tick ' + tick);
      mark.style.left = markIndex * 10 + '%';
      ruler.appendChild(mark);
    }
    surface.appendChild(ruler);
    program.lanes.forEach(function(lane, laneIndex) {
      var laneRow = node('div', 'cutscene-director-lane cutscene-director-lane-' + lane);
      laneRow.style.top = (28 + laneIndex * laneHeight) + 'px';
      laneRow.style.height = laneHeight + 'px';
      surface.appendChild(laneRow);
    });
    var collisionCounts = {};
    rows.forEach(function(row) {
      var laneIndex = program.lanes.indexOf(row.category);
      if (laneIndex < 0) laneIndex = program.lanes.length - 1;
      var collisionKey = laneIndex + ':' + row.runtimeStartTick;
      var collision = collisionCounts[collisionKey] || 0;
      collisionCounts[collisionKey] = collision + 1;
      var block = button(row.label,
        'cutscene-director-block cutscene-director-block-' + row.category, function() {
          view.selectedSourceId = row.id;
          view.selectedClipId = null;
          view.frame = row.runtimeStartTick;
          if (row.editorTarget) {
            view.selectedClipId = row.editorTarget.clipId;
            if (row.editorTarget.actorId) view.selectedActorId = row.editorTarget.actorId;
          }
          if (state.ui.clock) state.ui.clock = OB64.cutscenePreview.pause(
            OB64.cutscenePreview.seek(state.ui.clock, view.frame));
          rerender(rom, state);
        });
      block.style.left = row.runtimeStartTick / runtime.durationTicks * 100 + '%';
      block.style.top = (33 + laneIndex * laneHeight + Math.min(2, collision) * 10) + 'px';
      block.style.width = Math.max(4,
        row.runtimeDurationTicks / runtime.durationTicks * surfaceWidth) + 'px';
      block.classList.toggle('selected', selected && selected.id === row.id);
      block.setAttribute('aria-pressed', selected && selected.id === row.id ? 'true' : 'false');
      block.setAttribute('data-cutscene-focus-key', 'runtime:' + row.id);
      block.title = row.label + ' · tick ' + row.runtimeStartTick +
        ' · ' + row.runtimeDurationTicks + ' scheduler updates';
      surface.appendChild(block);
    });
    var playhead = node('div', 'cutscene-timeline-playhead');
    playhead.style.left = view.frame / Math.max(1, runtime.durationTicks) * 100 + '%';
    surface.appendChild(playhead);
    state.ui.timelinePlayhead = playhead;
    viewport.appendChild(surface);
    section.appendChild(viewport);
    renderDirectorSourceDetails(section, program, selected, scene, state.catalog);
  }

  function renderTimeline(center, rom, state, scene, document) {
    var section = node('section', 'cutscene-sequence');
    var tabs = node('div', 'cutscene-sequence-heading');
    tabs.appendChild(node('h3', '', 'Cutscene sequence'));
    var view = viewFor(state, scene.sceneId);
    var program = state.programByAssetId[scene.assetId] || null;
    var runtime = state.runtimeByAssetId[scene.assetId] || null;
    var actions = node('div', 'cutscene-story-actions');
    var modes = program ? [
      ['Runtime playback', 'runtime'],
      ['Composite actions', 'composite'],
      ['Native opcodes', 'native'],
      ['Projected clips', 'preview']
    ] : [['Projected clips', 'preview']];
    if (!program) view.timelineMode = 'preview';
    if (view.timelineMode === 'runtime' && !runtime) view.timelineMode = 'composite';
    modes.forEach(function(entry) {
      var modeButton = button(entry[0], 'btn-secondary', function() {
        view.timelineMode = entry[1];
        view.selectedSourceId = null;
        view.selectedClipId = null;
        rerender(rom, state);
      });
      modeButton.classList.toggle('active', view.timelineMode === entry[1]);
      modeButton.setAttribute('aria-pressed', view.timelineMode === entry[1] ? 'true' : 'false');
      modeButton.setAttribute('data-cutscene-focus-key', 'timeline-mode:' + entry[1]);
      actions.appendChild(modeButton);
    });
    if (program && view.timelineMode === 'runtime') {
      tabs.appendChild(actions);
      section.appendChild(tabs);
      renderDirectorRuntime(section, rom, state, scene, document, program, runtime);
      center.appendChild(section);
      return;
    }
    if (program && view.timelineMode !== 'preview') {
      tabs.appendChild(actions);
      section.appendChild(tabs);
      renderDirectorSource(section, rom, state, scene, document, program, view.timelineMode);
      center.appendChild(section);
      return;
    }
    [['Enter', addEnter], ['Exit', addExit], ['Set pose', addPose], ['Move', addMove],
      ['Opacity', addOpacity],
      ['Hold', addHold], ['Speak', addDialogue], ['Sound', addAudio],
      ['Effect', addEffect], ['Battle action', addCombatAction],
      ['Projection', addCamera], ['Branch', addBranch],
      ['End', addEnd]].forEach(function(entry) {
      actions.appendChild(button(entry[0], 'btn-secondary', function() { entry[1](rom, state); }));
    });
    actions.appendChild(button('Undo', 'btn-secondary', function() {
      var history = historyFor(state, scene);
      if (OB64.cutsceneModel.undo(history)) {
        refreshRuntime(state, scene, history.present);
        notifyChange(state, 'Undo Cutscene edit'); rerender(rom, state);
      }
    }));
    actions.appendChild(button('Redo', 'btn-secondary', function() {
      var history = historyFor(state, scene);
      if (OB64.cutsceneModel.redo(history)) {
        refreshRuntime(state, scene, history.present);
        notifyChange(state, 'Redo Cutscene edit'); rerender(rom, state);
      }
    }));
    tabs.appendChild(actions);
    section.appendChild(tabs);

    var storyboard = node('div', 'cutscene-storyboard');
    storyboard.setAttribute('data-cutscene-scroll', 'storyboard');
    var rows = orderedClips(document).filter(function(row) {
      return OB64.cutscenePreview.appliesToPath(row.clip, view.pathId);
    });
    if (!rows.length) storyboard.appendChild(node('p', 'cutscene-empty-note', 'No timed commands are mapped.'));
    rows.forEach(function(row) {
      var item = button('', 'cutscene-story-card', function() {
        selectClip(rom, state, scene, row);
      });
      item.classList.toggle('selected', row.clip.id === view.selectedClipId);
      item.setAttribute('aria-pressed', row.clip.id === view.selectedClipId ? 'true' : 'false');
      item.setAttribute('data-cutscene-focus-key', 'clip:' + row.clip.id);
      item.appendChild(node('strong', '', clipLabel(row, document)));
      item.appendChild(node('span', '', OB64.cutscenePreview.formatTime(row.clip.startFrame)));
      item.appendChild(capabilityBadge(row.clip.capability, 'command'));
      storyboard.appendChild(item);
    });
    section.appendChild(storyboard);

    var duration = Math.max(1, OB64.cutscenePreview.sceneDurationFrames(document, view.pathId));
    var timeline = node('div', 'cutscene-timeline');
    timeline.setAttribute('data-cutscene-scroll', 'timeline');
    var surface = node('div', 'cutscene-timeline-surface');
    var surfaceWidth = Math.max(620, Math.max(620, duration * 3) * view.zoom);
    surface.style.width = surfaceWidth + 'px';
    var ruler = node('div', 'cutscene-time-ruler');
    var seconds = Math.ceil(duration / 30);
    for (var second = 0; second <= seconds; second++) {
      var mark = node('span', '', second + ' preview s');
      mark.style.left = second * 30 / duration * 100 + '%';
      ruler.appendChild(mark);
    }
    surface.appendChild(ruler);
    rows.forEach(function(row) {
      var clip = node('button', 'cutscene-timeline-clip cutscene-timeline-' + row.track.type,
        clipLabel(row, document));
      clip.type = 'button';
      clip.style.left = row.clip.startFrame / duration * 100 + '%';
      clip.style.width = Math.max(1.5, Math.max(1, row.clip.durationFrames) / duration * 100) + '%';
      clip.setAttribute('data-cutscene-focus-key', 'timeline:' + row.clip.id);
      clip.classList.toggle('selected', row.clip.id === view.selectedClipId);
      clip.setAttribute('aria-pressed', row.clip.id === view.selectedClipId ? 'true' : 'false');
      var resize = node('span', 'cutscene-timeline-resize');
      resize.setAttribute('data-cutscene-resize', 'end');
      resize.setAttribute('aria-hidden', 'true');
      clip.appendChild(resize);
      clip.addEventListener('click', function() {
        if (clip._cutsceneDragged) { clip._cutsceneDragged = false; return; }
        selectClip(rom, state, scene, row);
      });
      installTimelineDrag(clip, rom, state, scene, row, duration, surfaceWidth);
      surface.appendChild(clip);
    });
    var playhead = node('div', 'cutscene-timeline-playhead');
    playhead.style.left = view.frame / duration * 100 + '%';
    surface.appendChild(playhead);
    timeline.appendChild(surface);
    state.ui.timelinePlayhead = playhead;
    section.appendChild(timeline);
    center.appendChild(section);
  }

  function insertionBoundaries(scene, document) {
    var cursorFrame = 0;
    return scene.source.nodes.map(function(sourceNode) {
      var row = null;
      document.tracks.some(function(track) {
        return track.clips.some(function(clip) {
          if (clip.source && clip.source.nodeId === sourceNode.id) {
            row = { track: track, clip: clip };
            return true;
          }
          return false;
        });
      });
      var frame = row ? row.clip.startFrame : cursorFrame;
      if (row && row.clip.kind === 'wait') cursorFrame = frame + row.clip.durationFrames;
      else cursorFrame = Math.max(cursorFrame, frame);
      return {
        id: sourceNode.id,
        frame: frame,
        sourceNode: sourceNode,
        approved: sourceNode.insertBefore === true && sourceNode.nodeType !== 'gap' &&
          sourceNode.editPolicy !== 'immutable-gap'
      };
    }).filter(function(boundary) { return boundary.approved; });
  }

  function boundaryForFrame(scene, document, frame) {
    var boundaries = insertionBoundaries(scene, document);
    if (!boundaries.length) return null;
    return boundaries.sort(function(left, right) {
      var leftAfter = left.frame >= frame ? 0 : 1;
      var rightAfter = right.frame >= frame ? 0 : 1;
      return leftAfter - rightAfter || Math.abs(left.frame - frame) - Math.abs(right.frame - frame) ||
        left.sourceNode.startWord - right.sourceNode.startWord;
    })[0].sourceNode;
  }

  function trackOrCreate(document, type, actorId, label) {
    var track = document.tracks.find(function(candidate) {
      return candidate.type === type && candidate.actorId === actorId;
    });
    if (track) return track;
    var base = actorId ? actorId.replace(/^actor:/, '').replace(/[^A-Za-z0-9:_-]/g, '-') : 'global';
    var id = 'track:authored:' + base + ':' + type;
    var suffix = 1;
    while (document.tracks.some(function(candidate) { return candidate.id === id; })) {
      id = 'track:authored:' + base + ':' + type + ':' + suffix++;
    }
    track = OB64.cutsceneModel.createTrack({ id: id, type: type, actorId: actorId, label: label });
    document.tracks.push(track);
    return track;
  }

  function authoredClipId(document, kind) {
    var index = 1, id;
    do { id = 'clip:authored:' + kind + ':' + index++; }
    while (document.tracks.some(function(track) {
      return track.clips.some(function(clip) { return clip.id === id; });
    }));
    return id;
  }

  function selectedActorState(state) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var document = selectedDocument(state);
    var actor = document.actors.find(function(candidate) { return candidate.id === view.selectedActorId; });
    if (!actor) {
      if (state.callbacks.onStatus) state.callbacks.onStatus('Select an actor first.');
      return null;
    }
    var preview = OB64.cutscenePreview.evaluateAtFrame(document, view.frame, { pathId: view.pathId });
    return {
      actor: actor,
      preview: preview.actors.find(function(candidate) { return candidate.id === actor.id; }) || actor
    };
  }

  function addAuthoredClip(rom, state, options) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var document = selectedDocument(state);
    var boundary = options.nativePreferred === true
      ? boundaryForFrame(scene, document, view.frame) : null;
    var id = authoredClipId(document, options.kind);
    view.selectedClipId = id;
    if (options.actorId) view.selectedActorId = options.actorId;
    executeEdit(rom, state, options.label, function(next) {
      var actor = options.actorId && next.actors.find(function(candidate) {
        return candidate.id === options.actorId;
      });
      var trackLabel = options.trackLabel || (actor ? actor.label + ' ' + options.type : 'Scene ' + options.type);
      var track = trackOrCreate(next, options.type, options.actorId || null, trackLabel);
      track.clips.push(OB64.cutsceneModel.createClip({
        id: id,
        kind: options.kind,
        startFrame: view.frame,
        durationFrames: options.durationFrames,
        pathIds: options.pathIds || [],
        capability: options.capability || (boundary ? 'native' : 'preview-only'),
        payload: options.payload,
        source: boundary ? { insertBeforeNodeId: boundary.id } : {}
      }));
    });
  }

  function poseSelectionsForBank(catalog, bank) {
    return catalog.poseSelectionsForBank
      ? catalog.poseSelectionsForBank(bank) : catalog.poseProgramsForBank(bank);
  }

  function preferredPoseSelection(catalog, bank) {
    var rows = poseSelectionsForBank(catalog, bank);
    return rows.find(function(row) { return row.programId && row.frames.length; }) ||
      rows.find(function(row) { return row.programId; }) || rows[0] || null;
  }

  function poseSelectionId(selection) {
    return selection ? selection.programId || selection.selectorId || '' : '';
  }

  function poseSelectionLabel(selection) {
    return 'Key ' + selection.animationKey + ' · facing ' + selection.facing +
      ' · state ' + selection.stateIndex + ' · ' + (selection.programId
        ? (selection.frames && selection.frames.length
          ? selection.durationFrames + ' frames' + (selection.sourceProgramDefined
            ? '' : ' · ROM physical state')
          : (selection.emptyProgram
            ? 'empty pose program · no frame change'
            : 'control-only program · no visual frame'))
        : 'structural state · no source program');
  }

  function poseSelectionNeedsResearch(selection) {
    return !selection || !selection.programId;
  }

  function poseSelectionStatus(selection) {
    if (!selection) return 'pose selector unresolved';
    if (!selection.programId) return 'physical state located; no source program is available';
    if (!selection.frames || !selection.frames.length) {
      return selection.emptyProgram
        ? 'zero-entry pose program located; it yields without changing the actor frame'
        : 'control-only pose program located; no visual frame is available';
    }
    return selection.sourceProgramDefined
      ? 'physical state and scene-published visual pose program located'
      : 'physical ROM state and visual pose program located; no scene-local publication is required';
  }

  function getPoseSelection(catalog, identity) {
    return catalog.getPoseProgramById(identity) || catalog.getPoseSelectorById(identity);
  }

  function actorPoseSelectionAtFrame(document, actor, frame, pathId, catalog) {
    var source = actor.source || {};
    var bankMatch = String(actor.artSourceId || '').match(/:(\d+)$/);
    var facingMatch = String(actor.initial.facing || '').match(/^native-(\d+)$/);
    var bank = Number.isInteger(source.bank)
      ? source.bank : (bankMatch ? Number(bankMatch[1]) : 1);
    var animationKey = Number.isInteger(source.animationKey) ? source.animationKey : 0;
    var nativeFacing = facingMatch ? Number(facingMatch[1]) : 0;
    var rows = [];
    document.tracks.forEach(function(track, trackIndex) {
      if (track.type !== 'pose' || track.actorId !== actor.id) return;
      track.clips.forEach(function(clip, clipIndex) {
        if (clip.startFrame > frame ||
            Array.isArray(clip.pathIds) && clip.pathIds.length &&
            clip.pathIds.indexOf(pathId) === -1) return;
        rows.push({ clip: clip, trackIndex: trackIndex, clipIndex: clipIndex });
      });
    });
    rows.sort(function(left, right) {
      return left.clip.startFrame - right.clip.startFrame ||
        left.trackIndex - right.trackIndex || left.clipIndex - right.clipIndex;
    });
    rows.forEach(function(row) {
        var clip = row.clip;
        var poseMatch = String(clip.payload.poseId || '')
          .match(/^cutscene-pose:(\d+):(\d+):(\d+)$/);
        if (Number.isInteger(clip.payload.bank) && clip.payload.bank >= 0) {
          bank = clip.payload.bank;
        } else if (poseMatch) {
          bank = Number(poseMatch[1]);
        }
        if (Number.isInteger(clip.payload.animationKey) && clip.payload.animationKey >= 0) {
          animationKey = clip.payload.animationKey;
        } else if (poseMatch) {
          animationKey = Number(poseMatch[2]);
        }
        if (Number.isInteger(clip.payload.nativeFacing) && clip.payload.nativeFacing >= 0) {
          nativeFacing = clip.payload.nativeFacing;
        } else if (poseMatch) {
          nativeFacing = Number(poseMatch[3]);
        }
    });
    return {
      bank: bank,
      animationKey: animationKey,
      nativeFacing: nativeFacing,
      selection: catalog.getPoseProgram(bank, animationKey, nativeFacing) ||
        catalog.getPoseSelector(bank, animationKey, nativeFacing)
    };
  }

  function addHold(rom, state) {
    addAuthoredClip(rom, state, {
      label: 'Add hold', type: 'flow', kind: 'wait', durationFrames: 30,
      nativePreferred: true, payload: { timingStatus: 'authored-preview-frame' }
    });
  }

  function addMove(rom, state) {
    var selected = selectedActorState(state);
    if (!selected) return;
    var current = selected.preview;
    addAuthoredClip(rom, state, {
      label: 'Add movement', type: 'movement', kind: 'movement', actorId: selected.actor.id,
      durationFrames: 30, nativePreferred: true,
      payload: {
        from: { x: current.x, y: current.y, z: current.z },
        to: { x: current.x + 50, y: current.y, z: current.z },
        nativeSpeed: 1.667, durationMode: 'duration',
        timingStatus: 'authored-preview-frame'
      }
    });
  }

  function addPoseProgram(rom, state, program, positionOverride, label) {
    var selected = selectedActorState(state);
    if (!selected) return;
    var actor = selected.actor;
    var current = selected.preview;
    var bankMatch = String(current.artSourceId || actor.artSourceId || '').match(/:(\d+)$/);
    var poseMatch = String(current.poseId || actor.initial.poseId || '').match(/^cutscene-pose:(\d+):(\d+):(\d+)$/);
    var facingMatch = String(current.facing || actor.initial.facing || '').match(/^native-(\d+)$/);
    var bank = bankMatch ? Number(bankMatch[1]) : Number(actor.source.bank) || 1;
    var animationKey = poseMatch ? Number(poseMatch[2]) : Number(actor.source.animationKey) || 0;
    var nativeFacing = facingMatch ? Number(facingMatch[1]) : 0;
    program = program || state.catalog.getPoseProgram(bank, animationKey, nativeFacing) ||
      state.catalog.getPoseSelector(bank, animationKey, nativeFacing);
    var selectionNeedsResearch = poseSelectionNeedsResearch(program);
    if (program) {
      bank = program.bank;
      animationKey = program.animationKey;
      nativeFacing = program.facing;
    }
    var position = positionOverride || current;
    var appearance = Number.isInteger(current.variantSelector) && current.variantSelector >= 0
      ? current.variantSelector
      : (Number.isInteger(actor.source.variantSelector) && actor.source.variantSelector >= 0
        ? actor.source.variantSelector : 0);
    addAuthoredClip(rom, state, {
      label: label || (positionOverride ? 'Place actor on Stage' : 'Set actor pose'),
      type: 'pose', kind: 'pose', actorId: actor.id,
      durationFrames: program && program.durationFrames || 30, nativePreferred: true,
      capability: selectionNeedsResearch ? 'needs-research' : null,
      payload: {
        poseId: 'cutscene-pose:' + bank + ':' + animationKey + ':' + nativeFacing,
        bank: bank, animationKey: animationKey,
        physicalStateId: program && program.physicalStateId || null,
        stateIndex: program && program.stateIndex != null ? program.stateIndex : null,
        selectorStatus: poseSelectionStatus(program),
        variantSelector: appearance,
        variantSelectorStatus: 'preserved actor appearance selector',
        facing: 'native-' + nativeFacing, nativeFacing: nativeFacing,
        x: position.x, y: position.y, z: position.z, loop: true,
        timingStatus: 'authored-preview-frame'
      }
    });
  }

  function addPose(rom, state, positionOverride) {
    addPoseProgram(rom, state, null, positionOverride);
  }

  function addActorVisibility(rom, state, visible) {
    var selected = selectedActorState(state);
    if (!selected) return;
    var current = selected.preview;
    addAuthoredClip(rom, state, {
      label: visible ? 'Enter actor' : 'Exit actor', type: 'actor',
      kind: visible ? 'enter' : 'exit', actorId: selected.actor.id,
      durationFrames: 0, nativePreferred: true,
      payload: {
        visible: visible,
        bank: current.bank,
        animationKey: current.animationKey,
        facing: current.facing,
        nativeFacing: current.nativeFacing,
        x: current.x, y: current.y, z: current.z,
        variantSelector: current.variantSelector,
        renderMode: Number.isInteger(selected.actor.source.renderMode)
          ? selected.actor.source.renderMode : 0
      }
    });
  }

  function addEnter(rom, state) { addActorVisibility(rom, state, true); }
  function addExit(rom, state) { addActorVisibility(rom, state, false); }

  function addOpacity(rom, state) {
    var selected = selectedActorState(state);
    if (!selected) return;
    addAuthoredClip(rom, state, {
      label: 'Set actor opacity',
      type: 'actor', kind: 'opacity', actorId: selected.actor.id,
      durationFrames: 0, nativePreferred: true,
      payload: {
        opacityByte: Number.isInteger(selected.preview.opacityByte)
          ? (selected.preview.opacityByte === 255 ? 128 : selected.preview.opacityByte) : 128,
        semantics: 'actor render alpha multiplier'
      }
    });
  }

  function addDialogue(rom, state) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var actor = selectedDocument(state).actors.find(function(candidate) {
      return candidate.id === view.selectedActorId;
    });
    addAuthoredClip(rom, state, {
      label: 'Add dialogue', type: 'dialogue', kind: 'dialogue',
      actorId: actor ? actor.id : null, durationFrames: 90,
      payload: {
        sourceSystem: 'authored-dialogue',
        speaker: actor ? actor.label : 'Narrator', text: 'New dialogue',
        associationStatus: 'authored preview metadata'
      }
    });
  }

  function addAudio(rom, state) {
    addAuthoredClip(rom, state, {
      label: 'Add sound cue', type: 'audio', kind: 'audio', durationFrames: 30,
      payload: {
        sourceSystem: 'authored-audio', cue: 'Unresolved sound cue',
        associationStatus: 'authored preview metadata'
      }
    });
  }

  function availableNativeEffectSlot(document) {
    var used = {};
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        if (clip.kind === 'effect' &&
            clip.payload.sourceSystem === 'cutscene-sprite-native' &&
            Number.isInteger(clip.payload.nativeEffectSlot)) {
          used[clip.payload.nativeEffectSlot] = true;
        }
      });
    });
    for (var slot = 0; slot < 30; slot++) if (!used[slot]) return slot;
    return null;
  }

  function addEffect(rom, state) {
    var document = selectedDocument(state);
    var slot = availableNativeEffectSlot(document);
    var firstPose = state.catalog.posePrograms.find(function(program) {
      return program.frames.length > 0;
    });
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var boundary = boundaryForFrame(scene, document, view.frame);
    if (slot != null && firstPose && boundary) {
      addAuthoredClip(rom, state, {
        label: 'Add native Cutscene sprite effect', type: 'effect', kind: 'effect',
        durationFrames: Math.max(1, firstPose.durationFrames || 30), nativePreferred: true,
        payload: {
          sourceSystem: 'cutscene-sprite-native', nativeOpcode: '0x46',
          nativeEffectSlot: slot, resourceRootKey: 0x0109F95E,
          poseId: firstPose.poseId, bank: firstPose.bank,
          animationKey: firstPose.animationKey, nativeFacing: firstPose.facing,
          physicalStateId: firstPose.physicalStateId,
          stateIndex: firstPose.stateIndex,
          variantSelector: 0, renderPassSelector: 1,
          nativeModelX: 0, nativeModelY: 0,
          stageX: 160, stageY: 120, scale: 1,
          selectorStatus: poseSelectionStatus(firstPose),
          coordinateStatus:
            'center-origin Stage preview; native model translation is exact but final viewport calibration remains unresolved',
          timingStatus:
            'authored native registered-wait lifetime; Preview seconds remain provisional',
          associationStatus: 'exact opcode-0x46 Cutscene-art renderer and pose selector'
        }
      });
      return;
    }
    var firstEffect = ensureState(rom).catalog.imageAssets.find(function(asset) {
      return asset.family === 'effect';
    });
    addAuthoredClip(rom, state, {
      label: 'Add visual effect', type: 'effect', kind: 'effect', durationFrames: 30,
      payload: {
        assetId: firstEffect ? firstEffect.assetId : 'effect:unresolved',
        sourceSystem: 'archive-preview', stageX: 160, stageY: 120, scale: 1
      }
    });
    if (state.callbacks.onStatus) state.callbacks.onStatus(
      slot == null ? 'All 30 native sprite-effect slots are reserved; added a Preview-only archive effect.' :
        (!firstPose ? 'No visual Cutscene pose was available; added a Preview-only archive effect.' :
          'No approved Director insertion boundary was available; added a Preview-only archive effect.'));
  }

  function addCombatAction(rom, state) {
    var animations = combatEffectAnimations(rom, state);
    var animation = animations[0];
    if (!animation) {
      if (state.callbacks.onStatus) state.callbacks.onStatus(
        'Combat action previews are unavailable for this loaded ROM.');
      return;
    }
    var duration = animation.frames.reduce(function(total, frame) {
      return total + Math.max(1, frame.ticks);
    }, 0);
    addAuthoredClip(rom, state, {
      label: 'Add battle action preview',
      type: 'effect',
      kind: 'effect',
      durationFrames: Math.max(1, duration),
      capability: 'preview-only',
      payload: {
        sourceSystem: 'combat-animation-preview',
        combatAnimationKey: animation.key,
        combatIdentity: combatAnimationIdentity(animation),
        combatSourceRoles: combatAnimationRoleSummary(animation).roles.slice(),
        stageX: 160,
        stageY: 120,
        scale: 1,
        conversionStatus:
          'visual Preview only; no Cutscene resource ownership or teardown adapter'
      }
    });
  }

  function addCamera(rom, state) {
    var scene = selectedScene(state);
    var document = selectedDocument(state);
    var view = viewFor(state, scene.sceneId);
    var camera = OB64.cutscenePreview.evaluateAtFrame(document, view.frame,
      { pathId: view.pathId }).cameraState;
    addAuthoredClip(rom, state, {
      label: 'Add projection transition', type: 'camera', kind: 'camera', durationFrames: 30,
      nativePreferred: true,
      payload: {
        presentationKind: 'projection-transform',
        displayLabel: 'Animate scene projection',
        target: {
          translateX: camera.translateX, translateY: camera.translateY,
          scaleX: camera.scaleX, scaleY: camera.scaleY
        },
        timingStatus:
          'authored projection-update countdown; preview seconds are provisional'
      }
    });
  }

  function addBranch(rom, state) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var document = selectedDocument(state);
    var branchIndex = 1;
    var branchId;
    do { branchId = 'branch:authored:' + branchIndex++; }
    while (document.branches.some(function(branch) { return branch.id === branchId; }));
    var clipIdValue = authoredClipId(document, 'branch');
    view.selectedClipId = clipIdValue;
    executeEdit(rom, state, 'Add branch', function(next) {
      next.branches.push({ id: branchId, label: 'Authored path ' + (branchIndex - 1),
        condition: { kind: 'preview-choice' } });
      var track = trackOrCreate(next, 'flow', null, 'Scene flow');
      track.clips.push(OB64.cutsceneModel.createClip({
        id: clipIdValue, kind: 'branch', startFrame: view.frame, durationFrames: 0,
        capability: 'preview-only', payload: { targetPathId: branchId,
          displayLabel: 'Choose ' + branchId }, source: {}
      }));
    });
  }

  function addEnd(rom, state) {
    addAuthoredClip(rom, state, {
      label: 'Add scene end', type: 'flow', kind: 'end', durationFrames: 0,
      payload: { displayLabel: 'End scene' }
    });
  }

  function placeActorAtStagePoint(rom, state, actorId, position) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var document = selectedDocument(state);
    var selectedRow = findClipRow(document, view.selectedClipId);
    if (selectedRow && selectedRow.track.type === 'movement' &&
        selectedRow.track.actorId === actorId) {
      executeEdit(rom, state, 'Move movement endpoint on Stage', function(next) {
        var row = findClipRow(next, selectedRow.clip.id);
        row.clip.payload.to = { x: position.x, y: position.y, z: position.z };
        row.clip.payload.durationMode = 'duration';
      });
      return;
    }
    var poseRow = null;
    document.tracks.forEach(function(track) {
      if (track.type !== 'pose' || track.actorId !== actorId) return;
      track.clips.forEach(function(clip) {
        if (clip.startFrame === view.frame) poseRow = { track: track, clip: clip };
      });
    });
    if (poseRow) {
      view.selectedClipId = poseRow.clip.id;
      executeEdit(rom, state, 'Place actor on Stage', function(next) {
        var row = findClipRow(next, poseRow.clip.id);
        row.clip.payload.x = position.x;
        row.clip.payload.y = position.y;
        row.clip.payload.z = position.z;
      });
      return;
    }
    view.selectedActorId = actorId;
    addPose(rom, state, position);
  }

  function installStagePointer(canvas, rom, state) {
    function coordinate(event) {
      var rect = canvas.getBoundingClientRect();
      return {
        x: (event.clientX - rect.left) * 320 / rect.width,
        y: (event.clientY - rect.top) * 240 / rect.height
      };
    }
    canvas.addEventListener('pointerdown', function(event) {
      if (!state.renderedStage) return;
      var point = coordinate(event);
      var actorId = OB64.cutsceneRenderer.hitTest(state.renderedStage, point.x, point.y);
      if (!actorId) return;
      var scene = selectedScene(state);
      var view = viewFor(state, scene.sceneId);
      view.selectedActorId = actorId;
      var actor = previewState(state).actors.find(function(candidate) { return candidate.id === actorId; });
      state.drag = {
        actorId: actorId,
        original: { x: actor.x, y: actor.y, z: actor.z },
        position: { x: actor.x, y: actor.y, z: actor.z }
      };
      canvas.setPointerCapture(event.pointerId);
      paintStage(rom, state);
    });
    canvas.addEventListener('pointermove', function(event) {
      if (!state.drag || !state.renderedStage) return;
      var point = coordinate(event);
      point = OB64.cutsceneRenderer.untransformStagePoint(
        point, state.renderedStage.camera);
      state.drag.position = OB64.cutsceneRenderer.unprojectPoint(
        point, state.renderedStage.projection, state.drag.original.y);
      state.drag.position.x = Math.round(state.drag.position.x * 1000) / 1000;
      state.drag.position.z = Math.round(state.drag.position.z * 1000) / 1000;
      paintStage(rom, state);
    });
    function finish(event, cancelled) {
      if (!state.drag) return;
      var drag = state.drag;
      state.drag = null;
      if (canvas.hasPointerCapture && canvas.hasPointerCapture(event.pointerId)) {
        canvas.releasePointerCapture(event.pointerId);
      }
      if (cancelled || drag.position.x === drag.original.x && drag.position.z === drag.original.z) {
        rerender(rom, state); return;
      }
      placeActorAtStagePoint(rom, state, drag.actorId, drag.position);
    }
    canvas.addEventListener('pointerup', function(event) { finish(event, false); });
    canvas.addEventListener('pointercancel', function(event) { finish(event, true); });
  }

  function numericInput(value, step, change, focusKey) {
    var input = node('input'); input.type = 'number'; input.value = value;
    input.step = step || '1'; input.setAttribute('data-cutscene-focus-key', focusKey);
    input.addEventListener('change', function() {
      var number = Number(input.value);
      if (Number.isFinite(number)) change(number);
    });
    return input;
  }

  function textInput(value, change, focusKey, multiline) {
    var input = node(multiline ? 'textarea' : 'input');
    if (!multiline) input.type = 'text';
    input.value = value == null ? '' : String(value);
    input.setAttribute('data-cutscene-focus-key', focusKey);
    input.addEventListener('change', function() { change(input.value); });
    return input;
  }

  function editClip(rom, state, clipId, label, mutator) {
    executeEdit(rom, state, label, function(document) {
      var row = findClipRow(document, clipId);
      if (row) mutator(row, document);
    });
  }

  function reassignClipActor(document, clipId, actorId) {
    var row = findClipRow(document, clipId);
    if (!row || row.track.actorId === actorId) return;
    var clip = row.clip;
    row.track.clips.splice(row.clipIndex, 1);
    var actor = actorId && document.actors.find(function(candidate) { return candidate.id === actorId; });
    var target = trackOrCreate(document, row.track.type, actorId,
      actor ? actor.label + ' ' + row.track.type : 'Scene ' + row.track.type);
    target.clips.push(clip);
  }

  function addPositionFields(host, rom, state, row, fieldName, label) {
    var value = row.clip.payload[fieldName] || { x: 0, y: 0, z: 0 };
    var grid = node('div', 'cutscene-coordinate-grid');
    ['x', 'y', 'z'].forEach(function(axis) {
      grid.appendChild(field(label + ' ' + axis.toUpperCase(), numericInput(
        Number(value[axis]) || 0, '0.001', function(number) {
          editClip(rom, state, row.clip.id, 'Edit ' + label + ' ' + axis.toUpperCase(),
            function(next) {
              if (!next.clip.payload[fieldName]) next.clip.payload[fieldName] = { x: 0, y: 0, z: 0 };
              next.clip.payload[fieldName][axis] = number;
            });
        }, 'clip-' + row.clip.id + '-' + fieldName + '-' + axis)));
    });
    host.appendChild(grid);
  }

  function splitMovementAtPlayhead(rom, state, row) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var offset = view.frame - row.clip.startFrame;
    if (offset <= 0 || offset >= row.clip.durationFrames) {
      if (state.callbacks.onStatus) state.callbacks.onStatus(
        'Place the playhead inside the movement before adding a keyframe.');
      return;
    }
    executeEdit(rom, state, 'Add movement keyframe', function(document) {
      var next = findClipRow(document, row.clip.id);
      var from = next.clip.payload.from;
      var to = next.clip.payload.to;
      var amount = offset / next.clip.durationFrames;
      var middle = {
        x: from.x + (to.x - from.x) * amount,
        y: from.y + (to.y - from.y) * amount,
        z: from.z + (to.z - from.z) * amount
      };
      var remaining = next.clip.durationFrames - offset;
      next.clip.durationFrames = offset;
      next.clip.payload.to = middle;
      var secondId = authoredClipId(document, 'movement');
      next.track.clips.push(OB64.cutsceneModel.createClip({
        id: secondId, kind: 'movement', startFrame: view.frame,
        durationFrames: remaining, capability: 'preview-only',
        payload: {
          from: middle, to: to, nativeSpeed: next.clip.payload.nativeSpeed,
          durationMode: 'duration', timingStatus: 'authored-keyframe-preview'
        }, source: {}
      }));
      view.selectedClipId = secondId;
    });
  }

  function renderClipInspector(inspector, rom, state, scene, document, row) {
    inspector.appendChild(node('h3', '', 'Selected action editor'));
    if (!row) {
      inspector.appendChild(node('p', 'cutscene-empty-note',
        'Select an editable composite action, Storyboard card, or Preview timing clip.'));
      return;
    }
    var heading = node('div', 'cutscene-clip-heading');
    heading.appendChild(node('strong', '', clipLabel(row, document)));
    heading.appendChild(capabilityBadge(row.clip.capability, 'command'));
    inspector.appendChild(heading);
    inspector.appendChild(node('p', 'cutscene-field-hint', row.clip.source.nodeId
      ? 'Source-backed command · its native boundary is preserved.'
      : (row.clip.source.insertBeforeNodeId
        ? 'Authored at an approved native insertion boundary.'
        : 'Preview-only Storyboard content.')));

    var timing = node('div', 'cutscene-timing-grid');
    if (row.clip.kind === 'wait' &&
        row.clip.payload.registeredCounterEditable === true) {
      var nativeWaitInput = numericInput(row.clip.payload.nativeTicks, '1', function(value) {
        editClip(rom, state, row.clip.id, 'Change native counter wait', function(next) {
          var ticks = Math.max(1, Math.round(value));
          next.clip.durationFrames = ticks;
          next.clip.payload.nativeTicks = ticks;
          next.clip.payload.registeredCounterTarget = ticks;
          next.clip.capability = 'native';
        });
      }, 'clip-native-wait:' + row.clip.id);
      nativeWaitInput.min = '1';
      nativeWaitInput.max = String(0x7FFFFFFF);
      timing.appendChild(field('Native counter updates', nativeWaitInput,
        'This changes the target inside the complete arm, query, and reset bundle.'));
    } else {
      timing.appendChild(field('Start frame', numericInput(row.clip.startFrame, '1', function(value) {
      editClip(rom, state, row.clip.id, 'Change clip start', function(next, nextDocument) {
        next.clip.startFrame = Math.max(0, Math.round(value));
        if (!next.clip.source.nodeId && next.clip.source.insertBeforeNodeId) {
          var boundary = boundaryForFrame(scene, nextDocument, next.clip.startFrame);
          if (boundary) next.clip.source.insertBeforeNodeId = boundary.id;
        }
      });
    }, 'clip-start-frame:' + row.clip.id)));
    timing.appendChild(field('Start preview seconds', numericInput((row.clip.startFrame / 30).toFixed(2),
      '0.1', function(value) {
        editClip(rom, state, row.clip.id, 'Change clip start', function(next, nextDocument) {
          next.clip.startFrame = Math.max(0, Math.round(value * 30));
          if (!next.clip.source.nodeId && next.clip.source.insertBeforeNodeId) {
            var boundary = boundaryForFrame(scene, nextDocument, next.clip.startFrame);
            if (boundary) next.clip.source.insertBeforeNodeId = boundary.id;
          }
        });
      }, 'clip-start-seconds:' + row.clip.id)));
    timing.appendChild(field('Duration frames', numericInput(row.clip.durationFrames, '1', function(value) {
      editClip(rom, state, row.clip.id, 'Change clip duration', function(next) {
        var minimum = next.clip.kind === 'wait' || next.clip.kind === 'movement' ||
          next.clip.kind === 'pose' || next.clip.kind === 'camera' ||
          next.clip.kind === 'effect' &&
            next.clip.payload.sourceSystem === 'cutscene-sprite-native' &&
            !next.clip.source.nodeId ? 1 : 0;
        next.clip.durationFrames = Math.max(minimum, Math.round(value));
        if (next.clip.kind === 'movement') next.clip.payload.durationMode = 'duration';
      });
    }, 'clip-duration-frame:' + row.clip.id)));
    timing.appendChild(field('Duration preview seconds', numericInput((row.clip.durationFrames / 30).toFixed(2),
      '0.1', function(value) {
        editClip(rom, state, row.clip.id, 'Change clip duration', function(next) {
          var minimum = next.clip.kind === 'wait' || next.clip.kind === 'movement' ||
            next.clip.kind === 'pose' || next.clip.kind === 'camera' ||
            next.clip.kind === 'effect' &&
              next.clip.payload.sourceSystem === 'cutscene-sprite-native' &&
              !next.clip.source.nodeId ? 1 : 0;
          next.clip.durationFrames = Math.max(minimum, Math.round(value * 30));
          if (next.clip.kind === 'movement') next.clip.payload.durationMode = 'duration';
        });
      }, 'clip-duration-seconds:' + row.clip.id),
    'Preview seconds are an editing aid. Native director timing remains stored in integer ticks.'));
    }
    inspector.appendChild(timing);

    var pathSelect = node('select');
    var allPaths = node('option', '', 'All preview paths'); allPaths.value = '';
    pathSelect.appendChild(allPaths);
    document.branches.forEach(function(branch) {
      var option = node('option', '', branch.label); option.value = branch.id; pathSelect.appendChild(option);
    });
    pathSelect.value = row.clip.pathIds.length === 1 ? row.clip.pathIds[0] : '';
    pathSelect.setAttribute('data-cutscene-focus-key', 'clip-path:' + row.clip.id);
    pathSelect.addEventListener('change', function() {
      editClip(rom, state, row.clip.id, 'Assign preview path', function(next) {
        next.clip.pathIds = pathSelect.value ? [pathSelect.value] : [];
      });
    });
    inspector.appendChild(field('Preview path', pathSelect,
      'Native condition targets remain blocked until their branch adapter is reviewed.'));

    if (['actor', 'pose', 'movement', 'dialogue'].indexOf(row.track.type) !== -1) {
      var actorSelect = node('select');
      if (row.track.type === 'dialogue') {
        var narrator = node('option', '', 'Narrator / scene'); narrator.value = '';
        actorSelect.appendChild(narrator);
      }
      document.actors.forEach(function(actor) {
        var option = node('option', '', actor.label + ' · slot ' + actor.slot);
        option.value = actor.id; actorSelect.appendChild(option);
      });
      actorSelect.value = row.track.actorId || '';
      actorSelect.setAttribute('data-cutscene-focus-key', 'clip-actor:' + row.clip.id);
      actorSelect.addEventListener('change', function() {
        editClip(rom, state, row.clip.id, 'Assign clip actor', function(next, nextDocument) {
          reassignClipActor(nextDocument, next.clip.id, actorSelect.value || null);
        });
      });
      inspector.appendChild(field('Actor', actorSelect));
    }

    if (row.clip.kind === 'opacity') {
      var actorFieldName = 'opacityByte';
      var actorFieldLabel = 'Opacity';
      var actorFieldInput = numericInput(row.clip.payload[actorFieldName], '1', function(value) {
        editClip(rom, state, row.clip.id, 'Change actor presentation', function(next) {
          next.clip.payload[actorFieldName] = Math.max(0, Math.min(255, Math.round(value)));
          if (scene.engine === 'director' &&
              (next.clip.source.nodeId || next.clip.source.insertBeforeNodeId) &&
              next.clip.payload.nativeSlotSelector !== -1) {
            next.clip.capability = 'native';
          }
        });
      }, 'clip-actor-' + actorFieldName + ':' + row.clip.id);
      actorFieldInput.min = '0'; actorFieldInput.max = '255';
      inspector.appendChild(field(actorFieldLabel, actorFieldInput,
        'The exact native byte multiplies actor render alpha. Zero is visually transparent.'));
    } else if (row.clip.kind === 'pose') {
      var bank = Number(row.clip.payload.bank) || 1;
      var bankSelect = node('select');
      state.catalog.actorArtSources.forEach(function(asset) {
        var option = node('option', '', asset.label + ' · ' + asset.poseCount + ' poses');
        option.value = asset.bank; bankSelect.appendChild(option);
      });
      bankSelect.value = String(bank);
      bankSelect.setAttribute('data-cutscene-focus-key', 'clip-pose-bank:' + row.clip.id);
      bankSelect.addEventListener('change', function() {
        var nextBank = Number(bankSelect.value);
        var selection = preferredPoseSelection(state.catalog, nextBank);
        editClip(rom, state, row.clip.id, 'Change pose Actor Art Source', function(next) {
          next.clip.payload.bank = nextBank;
          if (selection) {
            next.clip.payload.animationKey = selection.animationKey;
            next.clip.payload.facing = 'native-' + selection.facing;
            next.clip.payload.nativeFacing = selection.facing;
            next.clip.payload.poseId = selection.poseId;
            next.clip.payload.physicalStateId = selection.physicalStateId;
            next.clip.payload.stateIndex = selection.stateIndex;
            next.clip.payload.selectorStatus = poseSelectionStatus(selection);
            if (poseSelectionNeedsResearch(selection)) next.clip.capability = 'needs-research';
          } else {
            next.clip.payload.animationKey = null;
            next.clip.payload.nativeFacing = null;
            next.clip.payload.poseId = null;
            next.clip.capability = 'needs-research';
          }
        });
      });
      inspector.appendChild(field('Actor Art Source', bankSelect));
      var poseSelect = node('select');
      var poseSelections = poseSelectionsForBank(state.catalog, bank);
      poseSelections.forEach(function(selection) {
        var option = node('option', '', poseSelectionLabel(selection));
        option.value = poseSelectionId(selection); poseSelect.appendChild(option);
      });
      var currentProgram = state.catalog.getPoseProgram(bank,
        row.clip.payload.animationKey, row.clip.payload.nativeFacing);
      var currentSelector = currentProgram || state.catalog.getPoseSelector(
        bank, row.clip.payload.animationKey, row.clip.payload.nativeFacing);
      poseSelect.value = poseSelectionId(currentSelector);
      poseSelect.setAttribute('data-cutscene-focus-key', 'clip-pose:' + row.clip.id);
      poseSelect.addEventListener('change', function() {
        var selection = getPoseSelection(state.catalog, poseSelect.value);
        if (!selection) return;
        editClip(rom, state, row.clip.id, 'Change actor pose', function(next) {
          next.clip.payload.bank = selection.bank;
          next.clip.payload.animationKey = selection.animationKey;
          next.clip.payload.facing = 'native-' + selection.facing;
          next.clip.payload.nativeFacing = selection.facing;
          next.clip.payload.poseId = selection.poseId;
          next.clip.payload.physicalStateId = selection.physicalStateId;
          next.clip.payload.stateIndex = selection.stateIndex;
          next.clip.payload.selectorStatus = poseSelectionStatus(selection);
          if (poseSelectionNeedsResearch(selection)) next.clip.capability = 'needs-research';
        });
      });
      inspector.appendChild(field('Pose / animation', poseSelect,
        poseSelections.length + ' exact physical ROM states. Empty choices yield without changing the actor frame.'));
      var clipAppearance = node('select');
      for (var child = 0; child < 8; child++) {
        var childOption = node('option', '', 'Appearance ' + child);
        childOption.value = child; clipAppearance.appendChild(childOption);
      }
      clipAppearance.value = String(Number.isInteger(row.clip.payload.variantSelector) &&
        row.clip.payload.variantSelector >= 0 ? row.clip.payload.variantSelector : 0);
      clipAppearance.setAttribute('data-cutscene-focus-key',
        'clip-pose-appearance:' + row.clip.id);
      clipAppearance.addEventListener('change', function() {
        editClip(rom, state, row.clip.id, 'Change actor appearance', function(next) {
          next.clip.payload.variantSelector = Number(clipAppearance.value);
          next.clip.payload.variantSelectorStatus =
            'explicit sprite-child selector; unavailable children use native child-0 fallback';
        });
      });
      inspector.appendChild(field('Appearance', clipAppearance,
        'The runtime accepts child selectors 0–7. Each layer falls back to appearance 0 when its art has fewer children.'));
      var poseCoordinates = node('div', 'cutscene-coordinate-grid');
      ['x', 'y', 'z'].forEach(function(axis) {
        poseCoordinates.appendChild(field(axis.toUpperCase(), numericInput(
          row.clip.payload[axis] == null ? 0 : row.clip.payload[axis], '0.001', function(value) {
            editClip(rom, state, row.clip.id, 'Change pose position', function(next) {
              next.clip.payload[axis] = value;
            });
          }, 'clip-pose-' + axis + ':' + row.clip.id)));
      });
      inspector.appendChild(poseCoordinates);
    } else if (row.clip.kind === 'movement') {
      addPositionFields(inspector, rom, state, row, 'from', 'From');
      addPositionFields(inspector, rom, state, row, 'to', 'To');
      inspector.appendChild(button('Add keyframe at playhead', 'btn-secondary', function() {
        splitMovementAtPlayhead(rom, state, row);
      }));
    } else if (row.clip.kind === 'dialogue') {
      var dialogueEntryId = row.clip.payload.dialogueEntryId || row.clip.payload.serifuEntryId;
      var dialogueArchiveId = row.clip.payload.dialogueArchiveId || row.clip.payload.serifuArchiveId;
      var selectedDialogueEntry = dialogueEntryId &&
        state.catalog.getDialogueEntry(dialogueEntryId);
      var selectedDialogueArchive = dialogueArchiveId &&
        state.catalog.getDialogueArchive(dialogueArchiveId);
      if (!selectedDialogueArchive && selectedDialogueEntry) {
        selectedDialogueArchive = state.catalog.getDialogueArchive(selectedDialogueEntry.archiveIndex);
      }
      var dialogueArchiveSelect = node('select');
      var authoredDialogueOption = node('option', '', 'Authored text');
      authoredDialogueOption.value = '';
      dialogueArchiveSelect.appendChild(authoredDialogueOption);
      state.catalog.dialogueArchives.forEach(function(archive) {
        var familyLabel = archive.presentationFamily === 'meswin' ? 'MESWIN' : 'Serifu';
        var option = node('option', '', familyLabel + ' selector ' + archive.presentationSelector + ' · ' +
          archive.label + ' · ' + archive.entryCount + ' entries');
        option.value = archive.archiveId;
        dialogueArchiveSelect.appendChild(option);
      });
      dialogueArchiveSelect.value = selectedDialogueArchive ? selectedDialogueArchive.archiveId : '';
      dialogueArchiveSelect.setAttribute('data-cutscene-focus-key',
        'clip-dialogue-archive:' + row.clip.id);
      dialogueArchiveSelect.addEventListener('change', function() {
        var archive = dialogueArchiveSelect.value &&
          state.catalog.getDialogueArchive(dialogueArchiveSelect.value);
        editClip(rom, state, row.clip.id, 'Change dialogue archive', function(next) {
          if (!archive) {
            next.clip.payload.sourceSystem = 'authored-dialogue';
            delete next.clip.payload.dialogueArchiveId;
            delete next.clip.payload.dialogueEntryId;
            delete next.clip.payload.serifuArchiveId;
            delete next.clip.payload.serifuEntryId;
            delete next.clip.payload.presentationArchiveSelector;
            delete next.clip.payload.presentationEntrySelector;
            delete next.clip.payload.presentationResourceKey;
            delete next.clip.payload.rawText;
            return;
          }
          var entry = archive.entries[0];
          next.clip.payload.sourceSystem = archive.presentationFamily + '-preview';
          next.clip.payload.dialogueArchiveId = archive.archiveId;
          next.clip.payload.dialogueEntryId = entry.entryId;
          delete next.clip.payload.serifuArchiveId;
          delete next.clip.payload.serifuEntryId;
          next.clip.payload.presentationArchiveSelector = archive.presentationSelector;
          next.clip.payload.presentationEntrySelector = entry.entryIndex;
          next.clip.payload.presentationResourceKey = archive.presentationResourceKey;
          next.clip.payload.speaker = entry.speakerLabel ||
            (entry.speakerId == null ? next.clip.payload.speaker : 'Speaker ' + entry.speakerId);
          next.clip.payload.text = entry.text;
          next.clip.payload.rawText = entry.rawText;
          next.clip.payload.associationStatus = entry.associationStatus;
        });
      });
      inspector.appendChild(field('Dialogue archive', dialogueArchiveSelect,
        'All 348 Serifu archives and the 631-entry MESWIN system-message bank are available for visual Preview. Their named scene schedulers remain unresolved.'));
      if (selectedDialogueArchive) {
        var dialogueEntrySelect = node('select');
        selectedDialogueArchive.entries.forEach(function(entry) {
          var previewText = String(entry.text || '').replace(/\s+/g, ' ').slice(0, 72);
          var option = node('option', '', 'Selector ' + entry.entryIndex + ' · Entry ' +
            entry.displayEntryNumber +
            (entry.speakerLabel ? ' · ' + entry.speakerLabel : '') +
            (previewText ? ' · ' + previewText : ''));
          option.value = entry.entryId;
          dialogueEntrySelect.appendChild(option);
        });
        dialogueEntrySelect.value = selectedDialogueEntry ? selectedDialogueEntry.entryId :
          selectedDialogueArchive.entries[0].entryId;
        dialogueEntrySelect.setAttribute('data-cutscene-focus-key',
          'clip-dialogue-entry:' + row.clip.id);
        dialogueEntrySelect.addEventListener('change', function() {
          var entry = state.catalog.getDialogueEntry(dialogueEntrySelect.value);
          if (!entry) return;
          editClip(rom, state, row.clip.id, 'Select dialogue entry', function(next) {
            next.clip.payload.sourceSystem = selectedDialogueArchive.presentationFamily + '-preview';
            next.clip.payload.dialogueArchiveId = selectedDialogueArchive.archiveId;
            next.clip.payload.dialogueEntryId = entry.entryId;
            delete next.clip.payload.serifuArchiveId;
            delete next.clip.payload.serifuEntryId;
            next.clip.payload.presentationArchiveSelector = selectedDialogueArchive.presentationSelector;
            next.clip.payload.presentationEntrySelector = entry.entryIndex;
            next.clip.payload.presentationResourceKey = selectedDialogueArchive.presentationResourceKey;
            next.clip.payload.speaker = entry.speakerLabel ||
              (entry.speakerId == null ? next.clip.payload.speaker : 'Speaker ' + entry.speakerId);
            next.clip.payload.text = entry.text;
            next.clip.payload.rawText = entry.rawText;
            next.clip.payload.associationStatus = entry.associationStatus;
          });
        });
        inspector.appendChild(field('Archive entry', dialogueEntrySelect,
          'The zero-based entry index is the exact presentation selector. ' +
          selectedDialogueArchive.associationStatus));
      }
      if (rom.art && rom.art.avatar && Array.isArray(rom.art.avatar.appearances)) {
        var portraitSelect = node('select');
        var noPortrait = node('option', '', 'No portrait');
        noPortrait.value = '';
        portraitSelect.appendChild(noPortrait);
        rom.art.avatar.appearances.forEach(function(appearance) {
          appearance.selectorIndices.forEach(function(routeSelector) {
            var option = node('option', '', appearance.className + ' · ' + appearance.label +
              ' · route ' + routeSelector);
            option.value = appearance.key + '@' + routeSelector;
            portraitSelect.appendChild(option);
          });
        });
        portraitSelect.value = row.clip.payload.portraitAppearanceKey
          ? row.clip.payload.portraitAppearanceKey + '@' +
            Number(row.clip.payload.portraitRouteSelector || 0) : '';
        portraitSelect.setAttribute('data-cutscene-focus-key',
          'clip-dialogue-portrait:' + row.clip.id);
        portraitSelect.addEventListener('change', function() {
          var split = portraitSelect.value.lastIndexOf('@');
          var appearanceKey = split === -1 ? '' : portraitSelect.value.slice(0, split);
          var routeSelector = split === -1 ? 0 : Number(portraitSelect.value.slice(split + 1));
          var appearance = appearanceKey && rom.art.avatar.byKey[appearanceKey];
          editClip(rom, state, row.clip.id, 'Change dialogue portrait', function(next) {
            if (!appearance) {
              delete next.clip.payload.portraitAppearanceKey;
              delete next.clip.payload.portraitClassId;
              delete next.clip.payload.portraitRouteSelector;
              delete next.clip.payload.nativeSignedPortraitSelector;
              delete next.clip.payload.portraitAssociationStatus;
              return;
            }
            next.clip.payload.portraitAppearanceKey = appearance.key;
            next.clip.payload.portraitClassId = appearance.classId;
            next.clip.payload.portraitRouteSelector = routeSelector;
            next.clip.payload.nativeSignedPortraitSelector =
              routeSelector && appearance.classId === 0 ? null :
                (routeSelector ? -appearance.classId : appearance.classId);
            next.clip.payload.portraitAssociationStatus = routeSelector && appearance.classId === 0
              ? 'preview-only: signed negative zero cannot encode route 1 for class 0'
              : 'exact class-avatar portrait route; upstream dialogue selector writer unresolved';
          });
        });
        inspector.appendChild(field('Portrait', portraitSelect,
          'The 40×48 portrait and route are exact. Cutscene Studio keeps the upstream scene writer explicitly unresolved.'));
      }
      inspector.appendChild(field('Speaker', textInput(row.clip.payload.speaker, function(value) {
        editClip(rom, state, row.clip.id, 'Edit dialogue speaker', function(next) {
          next.clip.payload.speaker = value;
        });
      }, 'clip-speaker:' + row.clip.id)));
      inspector.appendChild(field('Dialogue', textInput(row.clip.payload.text, function(value) {
        editClip(rom, state, row.clip.id, 'Edit dialogue', function(next) {
          next.clip.payload.text = value;
        });
      }, 'clip-dialogue:' + row.clip.id, true)));
    } else if (row.clip.kind === 'audio') {
      var audioSourceSelect = node('select');
      var originalNativeAudio = row.clip.payload.originalNativeAudio || null;
      var nativeBlockSubstitution = row.clip.payload.nativeBlockRequestEditable === true ||
        !!(originalNativeAudio && originalNativeAudio.nativeBlockRequestEditable === true);
      var authoredAudio = node('option', '', 'Authored cue metadata');
      authoredAudio.value = 'authored'; audioSourceSelect.appendChild(authoredAudio);
      if (originalNativeAudio) {
        var nativeAudio = node('option', '', (originalNativeAudio.audioBlockId ||
          originalNativeAudio.registeredAudioRequestAssetId)
          ? 'Restore original Native Director command'
          : 'Restore original Native Director command · resource route unresolved');
        nativeAudio.value = 'native'; audioSourceSelect.appendChild(nativeAudio);
      }
      var registeredGroup = node('optgroup');
      registeredGroup.label = 'Registered request payloads';
      state.catalog.registeredAudioRequests.forEach(function(request) {
        var option = node('option', '', request.label + ' · semantic name unresolved');
        option.value = 'registered:' + request.requestAssetId;
        registeredGroup.appendChild(option);
      });
      audioSourceSelect.appendChild(registeredGroup);
      var blockGroup = node('optgroup');
      blockGroup.label = '63 sequenced-audio blocks · exact native request rows';
      state.catalog.audioBlocks.forEach(function(block) {
        var option = node('option', '', block.label + ' · ' + block.channels + ' channels' +
          (block.contextLabel ? ' · ' + block.contextLabel : ''));
        option.value = 'block:' + block.blockId;
        blockGroup.appendChild(option);
      });
      audioSourceSelect.appendChild(blockGroup);
      audioSourceSelect.value = row.clip.payload.registeredAudioRequestAssetId
        ? 'registered:' + row.clip.payload.registeredAudioRequestAssetId
        : (row.clip.payload.audioBlockId ? 'block:' + row.clip.payload.audioBlockId
          : (row.clip.payload.sourceSystem === 'director-native' ? 'native' : 'authored'));
      audioSourceSelect.setAttribute('data-cutscene-focus-key', 'clip-audio-source:' + row.clip.id);
      audioSourceSelect.addEventListener('change', function() {
        var value = audioSourceSelect.value;
        if (value === 'native') {
          editClip(rom, state, row.clip.id, 'Restore native audio request', function(next) {
            var original = next.clip.payload.originalNativeAudio;
            if (!original) return;
            next.clip.payload.sourceSystem = original.sourceSystem;
            next.clip.payload.nativeOpcode = original.nativeOpcode;
            next.clip.payload.nativeOperands = original.nativeOperands.slice();
            next.clip.payload.nativeRequestValue = original.nativeRequestValue;
            next.clip.payload.nativeBlockRequestEditable = original.nativeBlockRequestEditable;
            if (original.audioBlockId) {
              next.clip.payload.audioBlockId = original.audioBlockId;
            } else {
              delete next.clip.payload.audioBlockId;
            }
            if (original.registeredAudioRequestAssetId) {
              next.clip.payload.registeredAudioRequestAssetId =
                original.registeredAudioRequestAssetId;
            } else {
              delete next.clip.payload.registeredAudioRequestAssetId;
            }
            next.clip.payload.cue = original.cue;
            next.clip.payload.associationStatus = original.associationStatus;
            next.clip.capability = original.nativeBlockRequestEditable ? 'native' :
              ((original.audioBlockId || original.registeredAudioRequestAssetId)
                ? 'preview-only' : 'needs-research');
          });
          return;
        }
        editClip(rom, state, row.clip.id, 'Change audio preview source', function(next) {
          delete next.clip.payload.registeredAudioRequestAssetId;
          delete next.clip.payload.audioBlockId;
          if (value.indexOf('registered:') === 0) {
            var requestId = value.slice('registered:'.length);
            var request = state.catalog.getRegisteredAudioRequest(requestId);
            next.clip.payload.sourceSystem = 'registered-audio-preview';
            next.clip.payload.registeredAudioRequestAssetId = requestId;
            next.clip.payload.cue = request ? request.label : requestId;
            next.clip.payload.associationStatus = request ? request.associationStatus :
              'conditional request payload';
            next.clip.capability = 'preview-only';
          } else if (value.indexOf('block:') === 0) {
            var blockId = value.slice('block:'.length);
            var block = state.catalog.getAudioBlock(blockId);
            var nativeSource = next.clip.payload.originalNativeAudio;
            var canCompileBlock = !!(block && Number.isInteger(block.nativeRequestValue) &&
              nativeSource && nativeSource.nativeBlockRequestEditable === true);
            next.clip.payload.sourceSystem = canCompileBlock
              ? 'director-native' : 'sequenced-audio-block-preview';
            next.clip.payload.audioBlockId = blockId;
            if (canCompileBlock) {
              next.clip.payload.nativeOpcode = nativeSource.nativeOpcode;
              next.clip.payload.nativeOperands = nativeSource.nativeOperands.slice();
              next.clip.payload.nativeOperands[2] = block.nativeRequestValue;
              next.clip.payload.nativeRequestValue = block.nativeRequestValue;
              next.clip.payload.nativeBlockRequestEditable = true;
              next.clip.capability = 'native';
            } else {
              next.clip.capability = 'preview-only';
            }
            next.clip.payload.cue = block ? block.label : blockId;
            next.clip.payload.associationStatus = block ? block.associationStatus :
              'Director join unresolved';
          } else {
            next.clip.payload.sourceSystem = 'authored-audio';
            next.clip.payload.associationStatus = 'authored preview metadata';
            next.clip.capability = 'preview-only';
          }
        });
      });
      inspector.appendChild(field('Audio source', audioSourceSelect,
        nativeBlockSubstitution
          ? 'Replacing this existing selector-0 request is Native. Browser playback and semantic track names remain unresolved.'
          : 'These sources are Preview only because their native replacement contract is unresolved.'));
      inspector.appendChild(field('Sound cue', textInput(row.clip.payload.cue, function(value) {
        editClip(rom, state, row.clip.id, 'Edit sound cue', function(next) {
          next.clip.payload.cue = value;
        });
      }, 'clip-audio:' + row.clip.id), row.clip.payload.associationStatus ||
        'Preview metadata until the native audio command map is resolved.'));
    } else if (row.clip.kind === 'effect') {
      var effectSystems = node('select');
      [['cutscene-sprite-native', 'Native Cutscene sprite effect'],
        ['archive-preview', 'Archive effect image'],
        ['combat-animation-preview', 'Combat action animation']]
        .forEach(function(entry) {
          var systemOption = node('option', '', entry[1]);
          systemOption.value = entry[0]; effectSystems.appendChild(systemOption);
        });
      effectSystems.value = row.clip.payload.sourceSystem === 'cutscene-sprite-native'
        ? 'cutscene-sprite-native'
        : (row.clip.payload.sourceSystem === 'combat-animation-preview'
          ? 'combat-animation-preview' : 'archive-preview');
      effectSystems.setAttribute('data-cutscene-focus-key', 'clip-effect-system:' + row.clip.id);
      effectSystems.addEventListener('change', function() {
        editClip(rom, state, row.clip.id, 'Change visual effect system', function(next, nextDocument) {
          next.clip.payload.sourceSystem = effectSystems.value;
          if (effectSystems.value === 'combat-animation-preview') {
            var firstCombat = combatEffectAnimations(rom, state)[0];
            next.clip.payload.combatAnimationKey = firstCombat ? firstCombat.key : null;
            next.clip.payload.combatIdentity = firstCombat
              ? combatAnimationIdentity(firstCombat) : null;
            next.clip.payload.combatSourceRoles = firstCombat
              ? combatAnimationRoleSummary(firstCombat).roles.slice() : [];
            next.clip.payload.conversionStatus =
              'visual Preview only; no Cutscene resource ownership or teardown adapter';
            next.clip.capability = 'preview-only';
          } else if (effectSystems.value === 'cutscene-sprite-native') {
            var firstPose = state.catalog.posePrograms.find(function(program) {
              return program.frames.length > 0;
            });
            var effectSlot = Number.isInteger(next.clip.payload.nativeEffectSlot)
              ? next.clip.payload.nativeEffectSlot : availableNativeEffectSlot(nextDocument);
            var boundary = next.clip.source.nodeId ? null :
              boundaryForFrame(scene, nextDocument, next.clip.startFrame);
            next.clip.payload.poseId = firstPose ? firstPose.poseId : null;
            next.clip.payload.bank = firstPose ? firstPose.bank : null;
            next.clip.payload.animationKey = firstPose ? firstPose.animationKey : null;
            next.clip.payload.nativeFacing = firstPose ? firstPose.facing : null;
            next.clip.payload.physicalStateId = firstPose ? firstPose.physicalStateId : null;
            next.clip.payload.stateIndex = firstPose ? firstPose.stateIndex : null;
            next.clip.payload.variantSelector = 0;
            next.clip.payload.nativeEffectSlot = effectSlot;
            next.clip.payload.renderPassSelector = Number.isInteger(
              next.clip.payload.renderPassSelector) ? next.clip.payload.renderPassSelector : 1;
            next.clip.payload.resourceRootKey = 0x0109F95E;
            next.clip.payload.stageX = Number.isFinite(next.clip.payload.stageX)
              ? next.clip.payload.stageX : 160;
            next.clip.payload.stageY = Number.isFinite(next.clip.payload.stageY)
              ? next.clip.payload.stageY : 120;
            next.clip.payload.scale = Number.isFinite(next.clip.payload.scale)
              ? next.clip.payload.scale : 1;
            next.clip.payload.selectorStatus = poseSelectionStatus(firstPose);
            next.clip.payload.timingStatus = next.clip.source.nodeId
              ? 'source lifetime ends at its later replacement or cleanup command'
              : 'authored native registered-wait lifetime; Preview seconds remain provisional';
            if (!next.clip.source.nodeId && boundary) {
              next.clip.source.insertBeforeNodeId = boundary.id;
            }
            next.clip.capability = scene.engine === 'director' && firstPose && effectSlot != null &&
              (next.clip.source.nodeId || next.clip.source.insertBeforeNodeId)
              ? 'native' : 'needs-research';
          } else {
            var firstArchive = state.catalog.imageAssets.find(function(asset) {
              return asset.family === 'effect';
            });
            next.clip.payload.assetId = firstArchive ? firstArchive.assetId : 'effect:unresolved';
            next.clip.capability = 'preview-only';
          }
        });
      });
      inspector.appendChild(field('Effect source', effectSystems,
        'Native Cutscene sprites compile through opcode 0x46. Archive and combat imports remain visual Preview only.'));
      if (effectSystems.value === 'cutscene-sprite-native') {
        var effectBank = Number(row.clip.payload.bank) || 1;
        var effectBankSelect = node('select');
        state.catalog.actorArtSources.forEach(function(asset) {
          var option = node('option', '', asset.label + ' · ' + asset.poseCount + ' poses');
          option.value = asset.bank; effectBankSelect.appendChild(option);
        });
        effectBankSelect.value = String(effectBank);
        effectBankSelect.setAttribute('data-cutscene-focus-key',
          'clip-effect-bank:' + row.clip.id);
        effectBankSelect.addEventListener('change', function() {
          var bank = Number(effectBankSelect.value);
          var selection = preferredPoseSelection(state.catalog, bank);
          editClip(rom, state, row.clip.id, 'Change effect Actor Art Source', function(next) {
            next.clip.payload.bank = bank;
            if (selection) {
              next.clip.payload.animationKey = selection.animationKey;
              next.clip.payload.nativeFacing = selection.facing;
              next.clip.payload.poseId = selection.poseId;
              next.clip.payload.physicalStateId = selection.physicalStateId;
              next.clip.payload.stateIndex = selection.stateIndex;
              next.clip.payload.selectorStatus = poseSelectionStatus(selection);
              next.clip.capability = scene.engine === 'director' &&
                !poseSelectionNeedsResearch(selection) &&
                (next.clip.source.nodeId || next.clip.source.insertBeforeNodeId)
                ? 'native' : 'needs-research';
            } else {
              next.clip.payload.animationKey = null;
              next.clip.payload.nativeFacing = null;
              next.clip.payload.poseId = null;
              next.clip.capability = 'needs-research';
            }
          });
        });
        inspector.appendChild(field('Actor Art Source', effectBankSelect,
          'All 68 Cutscene sprite banks are available to the native effect renderer.'));

        var effectPoseSelect = node('select');
        var effectPoseSelections = poseSelectionsForBank(state.catalog, effectBank);
        effectPoseSelections.forEach(function(selection) {
          var option = node('option', '', poseSelectionLabel(selection));
          option.value = poseSelectionId(selection); effectPoseSelect.appendChild(option);
        });
        var currentEffectPose = state.catalog.getPoseProgram(effectBank,
          row.clip.payload.animationKey, row.clip.payload.nativeFacing) ||
          state.catalog.getPoseSelector(effectBank, row.clip.payload.animationKey,
            row.clip.payload.nativeFacing);
        effectPoseSelect.value = poseSelectionId(currentEffectPose);
        effectPoseSelect.setAttribute('data-cutscene-focus-key',
          'clip-effect-pose:' + row.clip.id);
        effectPoseSelect.addEventListener('change', function() {
          var selection = getPoseSelection(state.catalog, effectPoseSelect.value);
          if (!selection) return;
          editClip(rom, state, row.clip.id, 'Change native effect animation', function(next) {
            next.clip.payload.bank = selection.bank;
            next.clip.payload.animationKey = selection.animationKey;
            next.clip.payload.nativeFacing = selection.facing;
            next.clip.payload.poseId = selection.poseId;
            next.clip.payload.physicalStateId = selection.physicalStateId;
            next.clip.payload.stateIndex = selection.stateIndex;
            next.clip.payload.selectorStatus = poseSelectionStatus(selection);
            next.clip.capability = scene.engine === 'director' &&
              !poseSelectionNeedsResearch(selection) &&
              (next.clip.source.nodeId || next.clip.source.insertBeforeNodeId)
              ? 'native' : 'needs-research';
          });
        });
        inspector.appendChild(field('Pose / animation', effectPoseSelect,
          'The selected physical pose program plays through the same visual pipeline as Cutscene actors.'));

        var effectNativeFields = node('div', 'cutscene-coordinate-grid');
        var effectSlotInput = numericInput(row.clip.payload.nativeEffectSlot, '1', function() {},
          'clip-effect-slot:' + row.clip.id);
        effectSlotInput.disabled = true;
        effectNativeFields.appendChild(field('Effect slot', effectSlotInput,
          row.clip.source.nodeId
            ? 'Source slots stay fixed so later cleanup remains exact.'
            : 'Authored effects receive one unused slot from the 30-slot native table.'));
        [['variantSelector', 'Appearance', 0, 255],
          ['renderPassSelector', 'Render pass', 0, 255]].forEach(function(entry) {
          var input = numericInput(Number.isInteger(row.clip.payload[entry[0]])
            ? row.clip.payload[entry[0]] : 0, '1', function(value) {
              editClip(rom, state, row.clip.id, 'Change native effect field', function(next) {
                next.clip.payload[entry[0]] = Math.round(value);
              });
            }, 'clip-effect-' + entry[0] + ':' + row.clip.id);
          input.min = String(entry[2]); input.max = String(entry[3]);
          effectNativeFields.appendChild(field(entry[1], input));
        });
        inspector.appendChild(effectNativeFields);
        inspector.appendChild(node('p', 'cutscene-field-hint',
          row.clip.payload.selectorStatus ||
          'Native Cutscene sprite selector with center-origin Stage placement.'));
        inspector.appendChild(node('p', 'cutscene-field-hint', row.clip.source.nodeId
          ? 'This stock effect remains active until its source replacement or opcode 0x66 cleanup.'
          : 'This authored clip compiles as effect placement, a native Hold for its duration, then opcode 0x66 cleanup.'));
      } else if (effectSystems.value === 'combat-animation-preview') {
        var combatFilter = node('select');
        [['all', 'All battle actions'], ['body-only', 'Body-only actions'],
          ['equipment', 'Actions with equipment'],
          ['element-effect', 'Actions with element effects']]
          .forEach(function(entry) {
            var filterOption = node('option', '', entry[1]);
            filterOption.value = entry[0];
            combatFilter.appendChild(filterOption);
          });
        combatFilter.value = state.combatEffectFilter || 'all';
        combatFilter.setAttribute('data-cutscene-focus-key',
          'clip-combat-filter:' + row.clip.id);
        combatFilter.addEventListener('change', function() {
          state.combatEffectFilter = combatFilter.value;
          rerender(rom, state);
        });
        inspector.appendChild(field('Action filter', combatFilter,
          'Filters use exact body, equipment, and element-effect layer roles from the combat corpus.'));

        var combatSelect = node('select');
        var combatAnimations = combatEffectAnimations(rom, state);
        var filteredCombatAnimations = combatAnimations.filter(function(animation) {
          return combatAnimationMatchesFilter(animation, state.combatEffectFilter || 'all');
        });
        var currentCombatAnimation = combatAnimations.find(function(animation) {
          return animation.key === row.clip.payload.combatAnimationKey;
        });
        if (currentCombatAnimation && filteredCombatAnimations.indexOf(currentCombatAnimation) < 0) {
          filteredCombatAnimations.unshift(currentCombatAnimation);
        }
        filteredCombatAnimations.forEach(function(animation) {
          var roleSummary = combatAnimationRoleSummary(animation);
          var option = node('option', '', animation.spec.className + ' · ' +
            animation.spec.actionName + ' · ' + animation.spec.variantLabel +
            ' · ' + roleSummary.label);
          option.value = animation.key; combatSelect.appendChild(option);
        });
        combatSelect.value = row.clip.payload.combatAnimationKey || '';
        combatSelect.setAttribute('data-cutscene-focus-key', 'clip-combat-effect:' + row.clip.id);
        combatSelect.addEventListener('change', function() {
          editClip(rom, state, row.clip.id, 'Change combat animation preview', function(next) {
            next.clip.payload.combatAnimationKey = combatSelect.value || null;
            var selectedCombat = combatAnimations.find(function(animation) {
              return animation.key === combatSelect.value;
            });
            next.clip.payload.combatIdentity = selectedCombat
              ? combatAnimationIdentity(selectedCombat) : null;
            next.clip.payload.combatSourceRoles = selectedCombat
              ? combatAnimationRoleSummary(selectedCombat).roles.slice() : [];
            next.clip.payload.conversionStatus =
              'visual Preview only; no Cutscene resource ownership or teardown adapter';
          });
        });
        inspector.appendChild(field('Combat animation', combatSelect,
          filteredCombatAnimations.length + ' of ' + combatAnimations.length +
          ' catalogued action sequences match this filter. The full composite preserves body, equipment, and element-effect layers.'));
      } else {
        var effectSelect = node('select');
        var unresolvedEffect = node('option', '', 'Unresolved effect');
        unresolvedEffect.value = 'effect:unresolved'; effectSelect.appendChild(unresolvedEffect);
        state.catalog.imageAssets.filter(function(asset) {
          return asset.family === 'effect';
        }).forEach(function(asset) {
          var option = node('option', '', imageSourceLabel(asset) + ' · ' + asset.displayName);
          option.title = imageEvidenceHint(asset);
          option.value = asset.assetId; effectSelect.appendChild(option);
        });
        effectSelect.value = row.clip.payload.assetId || 'effect:unresolved';
        effectSelect.setAttribute('data-cutscene-focus-key', 'clip-effect:' + row.clip.id);
        effectSelect.addEventListener('change', function() {
          editClip(rom, state, row.clip.id, 'Edit visual effect', function(next) {
            next.clip.payload.assetId = effectSelect.value || 'effect:unresolved';
          });
        });
        inspector.appendChild(field('Effect asset', effectSelect,
          'Mapped archive effects are visual Preview only; no director-opcode consumer is joined.'));
      }
      var effectPosition = node('div', 'cutscene-coordinate-grid');
      [['stageX', 'Stage X', 160], ['stageY', 'Stage Y', 120], ['scale', 'Scale', 1]]
        .forEach(function(entry) {
          effectPosition.appendChild(field(entry[1], numericInput(
            Number.isFinite(row.clip.payload[entry[0]]) ? row.clip.payload[entry[0]] : entry[2],
            entry[0] === 'scale' ? '0.05' : '1', function(value) {
              editClip(rom, state, row.clip.id, 'Position visual effect', function(next) {
                next.clip.payload[entry[0]] = value;
                if (next.clip.payload.sourceSystem === 'cutscene-sprite-native' &&
                    scene.engine === 'director' &&
                    (next.clip.source.nodeId || next.clip.source.insertBeforeNodeId)) {
                  next.clip.capability = 'native';
                }
              });
            }, 'clip-effect-' + entry[0] + ':' + row.clip.id)));
        });
      inspector.appendChild(effectPosition);
    } else if (row.clip.kind === 'camera') {
      var presentationKind = row.clip.payload.presentationKind;
      if (presentationKind === 'camera-pose') {
        function vectorText(value) {
          value = value || {};
          return 'X ' + Number(value.x || 0).toFixed(3) + ' · Y ' +
            Number(value.y || 0).toFixed(3) + ' · Z ' +
            Number(value.z || 0).toFixed(3);
        }
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'Target ' + vectorText(row.clip.payload.target)));
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'Eye ' + vectorText(row.clip.payload.eye) + ' · FOV Y ' +
            Number(row.clip.payload.fovY || 0).toFixed(3)));
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'This is a source-exact native camera pose assignment. Stage rendering remains Preview only.'));
      } else if (presentationKind === 'projection-identity-transition') {
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'This native command returns the scene projection transform to identity.'));
      } else {
        var projectionFields = [
          ['translateX', 'Translation X', 0, '1'],
          ['translateY', 'Translation Y', 0, '1'],
          ['scaleX', 'Scale X', 1, '0.05'],
          ['scaleY', 'Scale Y', 1, '0.05']
        ];
        var projectionGroups = row.clip.payload.target
          ? [['target', 'Native projection target']] : [['from', 'From'], ['to', 'To']];
        projectionGroups.forEach(function(group) {
          var values = row.clip.payload[group[0]] || {};
          var grid = node('div', 'cutscene-coordinate-grid');
          grid.appendChild(node('strong', '', group[1]));
          projectionFields.forEach(function(definition) {
            grid.appendChild(field(definition[1], numericInput(
              Number.isFinite(values[definition[0]]) ? values[definition[0]] : definition[2],
              definition[3], function(value) {
                editClip(rom, state, row.clip.id, 'Adjust scene projection', function(next) {
                  if (!next.clip.payload[group[0]]) next.clip.payload[group[0]] = {};
                  next.clip.payload[group[0]][definition[0]] = value;
                  next.clip.capability = group[0] === 'from' ? 'preview-only' :
                    (next.clip.source.nodeId || next.clip.source.insertBeforeNodeId
                      ? 'native' : 'preview-only');
                });
              }, 'clip-projection-' + group[0] + '-' + definition[0] + ':' + row.clip.id)));
          });
          inspector.appendChild(grid);
        });
      }
      if (Number.isInteger(row.clip.payload.nativeCountdown)) {
        var effectiveProjectionCountdown = Number.isInteger(row.clip.payload.nativeCountdownLow16)
          ? row.clip.payload.nativeCountdownLow16 : row.clip.payload.nativeCountdown & 0xFFFF;
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'Native rate divisor ' + row.clip.payload.nativeCountdown +
          ' · effective low-16 countdown ' + effectiveProjectionCountdown +
          ' scheduler invocations. The Timeline shows one Preview step per invocation without claiming display frames or seconds.'));
      } else if (presentationKind === 'projection-transform') {
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'Projection translation and scale compile to native x1000 targets. Duration uses scheduler countdown steps; Timeline seconds remain a Preview aid.'));
      }
    }

    var actions = node('div', 'cutscene-inspector-actions');
    actions.appendChild(button('Remove step', 'btn-secondary cutscene-remove-action', function() {
      editClip(rom, state, row.clip.id, 'Remove Cutscene step', function(next, nextDocument) {
        nextDocument.tracks[next.trackIndex].clips.splice(next.clipIndex, 1);
        viewFor(state, scene.sceneId).selectedClipId = null;
      });
    }));
    inspector.appendChild(actions);
  }

  function availableActorSlot(document, scene) {
    var used = {};
    document.actors.forEach(function(actor) { if (actor.slot != null) used[actor.slot] = true; });
    (scene.actors || []).forEach(function(actor) {
      if (actor.slot != null) used[actor.slot] = true;
    });
    for (var slot = 0; slot < 20; slot++) if (!used[slot]) return slot;
    return null;
  }

  function actorHasNativePlace(actor) {
    var source = actor && actor.source || {};
    return typeof source.placeNodeId === 'string' || source.authored === true &&
      typeof source.insertBeforeNodeId === 'string';
  }

  function addPreviewActor(rom, state, sourceActor) {
    var scene = selectedScene(state);
    var document = selectedDocument(state);
    var view = viewFor(state, scene.sceneId);
    var slot = availableActorSlot(document, scene);
    if (slot == null) {
      if (state.callbacks.onStatus) state.callbacks.onStatus(
        'All observed director actor slots 0 through 19 are in use.');
      return;
    }
    var boundary = boundaryForFrame(scene, document, view.frame);
    var id = 'actor:authored:slot:' + String(slot).padStart(2, '0');
    var suffix = 1;
    while (document.actors.some(function(actor) { return actor.id === id; })) {
      id = 'actor:authored:slot:' + String(slot).padStart(2, '0') + ':' + suffix++;
    }
    var preview = sourceActor && OB64.cutscenePreview.evaluateAtFrame(document, view.frame,
      { pathId: view.pathId }).actors.find(function(actor) { return actor.id === sourceActor.id; });
    var defaultArt = state.catalog.actorArtSources[0];
    var artSourceId = sourceActor && sourceActor.artSourceId || defaultArt && defaultArt.assetId || null;
    var initial = preview ? {
      visible: true, x: preview.x, y: preview.y, z: preview.z,
      facing: preview.facing, poseId: preview.poseId
    } : { visible: true, x: 0, y: 0, z: 0, facing: 'native-0', poseId: null };
    view.selectedActorId = id;
    view.selectedClipId = null;
    executeEdit(rom, state, sourceActor ? 'Duplicate actor' : 'Activate unused actor slot',
      function(next) {
        next.actors.push(OB64.cutsceneModel.createActor({
          id: id,
          label: sourceActor ? sourceActor.label + ' copy' : 'Actor slot ' + slot,
          slot: slot,
          artSourceId: artSourceId,
          capability: boundary ? 'native' : 'preview-only',
          initial: initial,
          source: {
            authored: true,
            insertBeforeNodeId: boundary && boundary.id || null,
            authoredStartFrame: view.frame,
            bank: Number(String(artSourceId || '').match(/:(\d+)$/) &&
              String(artSourceId).match(/:(\d+)$/)[1]) || 1,
            animationKey: sourceActor && sourceActor.source.animationKey || 0,
            variantSelector: sourceActor && sourceActor.source.variantSelector || 0,
            renderMode: sourceActor && Number.isInteger(sourceActor.source.renderMode)
              ? sourceActor.source.renderMode : 0,
            renderModeStatus:
              'exact opcode-0x14 actor render-mode byte; authored actors default to native mode 0'
          }
        }));
      });
  }

  function renderInspector(shell, rom, state, scene, document) {
    var inspector = node('aside', 'cutscene-inspector');
    inspector.setAttribute('data-cutscene-scroll', 'inspector');
    inspector.appendChild(node('h3', '', 'Scene'));
    var sourceSummary = node('dl', 'cutscene-source-summary');
    var parseLabel = scene.parseStatus === 'runtime-tiled-static'
      ? 'Complete boundaries · static'
      : (scene.parseStatus === 'runtime-tiled-adapter-unresolved'
        ? 'Complete boundaries · adapter needed' : scene.parseStatus);
    var sourceRows = [['Catalog resource', scene.assetId], ['Engine', scene.engine],
      ['Director key', scene.directorKey || 'Not a Director stream'],
      ['Actors', String(document.actors.length)], ['Parse', parseLabel]];
    if (scene.launchProfile) {
      var launchMode = scene.launchProfile.directorMode;
      sourceRows.push(['Launch mode', launchMode.value == null
        ? 'External · unresolved'
        : 'Mode ' + launchMode.value + ' · ' + launchMode.evidenceStatus.replace(/-/g, ' ')]);
      var launchActorCamera = scene.launchProfile.cameras.actor;
      sourceRows.push(['Actor camera', launchActorCamera.evidenceStatus.replace(/-/g, ' ')]);
      var launchBackground = scene.launchProfile.background.requests[0] ||
        scene.launchProfile.background.inheritedPresentation || null;
      var launchObservation = scene.backgroundRuntimeObservation || null;
      var launchContext = document.background && document.background.projection &&
        document.background.projection.launchContext;
      var activeLaunchContext = launchContext && launchContext.override === true &&
        launchContext.mode === 2 ? launchContext : null;
      if (launchMode.value === 2) {
        var environmentSelector = activeLaunchContext
          ? activeLaunchContext.environmentSelector
          : (launchObservation && Number.isInteger(launchObservation.environmentSelector)
            ? launchObservation.environmentSelector
            : (launchBackground ? launchBackground.environmentSelector : null));
        var foregroundSelector = activeLaunchContext
          ? activeLaunchContext.foregroundSelector
          : (launchObservation && Number.isInteger(launchObservation.foregroundSelector)
            ? launchObservation.foregroundSelector
            : (launchBackground ? launchBackground.foregroundSelector : null));
        var backgroundEvidence = activeLaunchContext
          ? 'document launch input'
          : (launchObservation ? 'runtime observed'
            : (launchBackground
              ? launchBackground.evidenceStatus.replace(/-/g, ' ')
              : 'external unresolved'));
        sourceRows.push(['Environment selector', Number.isInteger(environmentSelector)
          ? environmentSelector + ' · ' + backgroundEvidence : 'External · unresolved']);
        sourceRows.push(['Foreground selector', Number.isInteger(foregroundSelector)
          ? foregroundSelector + ' · ' + backgroundEvidence
          : (!activeLaunchContext && launchObservation &&
              Object.prototype.hasOwnProperty.call(launchObservation, 'foregroundSelector')
            ? 'Inactive in stored launch' : 'External · unresolved')]);
      } else {
        sourceRows.push(['Background route', launchBackground &&
            launchBackground.selectorTableId !== null && launchBackground.selector !== null
          ? 'Scene ' + launchBackground.selector + ' · ' +
            launchBackground.evidenceStatus.replace(/-/g, ' ')
          : (launchBackground ? 'External · unresolved' : 'No Director request')]);
      }
      sourceRows.push(['Launch roster', scene.launchProfile.roster.templateCount +
        ' templates · ' + scene.launchProfile.roster.evidenceStatus.replace(/-/g, ' ')]);
      if (scene.launchProfile.parentEventLaunches.length) {
        sourceRows.push(['Parent event launches',
          scene.launchProfile.parentEventLaunches.length +
          ' direct Director source site' +
          (scene.launchProfile.parentEventLaunches.length === 1 ? '' : 's')]);
      }
      var operandTranslation = scene.launchProfile.operandTranslation;
      if (operandTranslation.required) {
        var selectedEventContext = launchContextChoice(state, scene, null);
        var translatedValues = launchOperandTranslations(scene, selectedEventContext);
        var translationResolved = operandTranslation.tableIndexes.every(function(index) {
          return Number.isInteger(translatedValues[index]);
        });
        sourceRows.push(['Launch operand table',
          operandTranslation.placeholderCount + ' placeholders · indexes ' +
          operandTranslation.tableIndexes.join(', ') + ' · ' +
          (translationResolved ? 'selected event invocation resolved' : 'input required')]);
      }
    }
    if (scene.source.tailRecovery) {
      sourceRows.push(['Recovered tail',
        scene.source.tailRecovery.recoveredWordCount + ' words · ' +
        scene.source.tailRecovery.recoveredNodeCount + ' boundaries']);
      sourceRows.push(['Runtime skips',
        String(scene.source.tailRecovery.unknownSkipNodeCount)]);
    }
    if (scene.recoveredMediaRequests && scene.recoveredMediaRequests.length) {
      sourceRows.push(['Recovered media',
        scene.recoveredMediaRequests.length + ' exact command boundaries']);
    }
    if (scene.recoveredActorEvents && scene.recoveredActorEvents.length) {
      var recoveredActorApplied = scene.recoveredActorEvents.filter(function(event) {
        return event.eventStatus === 'applied';
      }).length;
      var recoveredActorWithheld = scene.recoveredActorEvents.length - recoveredActorApplied;
      sourceRows.push(['Recovered actor flow',
        scene.recoveredActorEvents.length + ' commands · ' + recoveredActorApplied +
        ' applied' + (recoveredActorWithheld ? ' · ' + recoveredActorWithheld + ' withheld' : '')]);
    }
    if (scene.recoveredNativeSpriteEffects && scene.recoveredNativeSpriteEffects.length) {
      var recoveredEffectStates = new Set(scene.recoveredNativeSpriteEffects.map(function(effect) {
        return effect.physicalStateId;
      })).size;
      sourceRows.push(['Recovered sprite effects',
        scene.recoveredNativeSpriteEffects.length + ' commands · ' +
        recoveredEffectStates + ' physical states · exact render closure']);
    }
    if (scene.audioAssociations && scene.audioAssociations.length) {
      sourceRows.push(['Mapped cues',
        scene.audioAssociations.length + ' exact payload association' +
        (scene.audioAssociations.length === 1 ? '' : 's')]);
    }
    var capturedStages = Array.isArray(document.identity.captures)
      ? document.identity.captures.filter(function(capture) {
        return capture.stageLabel;
      }) : [];
    if (capturedStages.length) {
      sourceRows.push(['Captured stages', capturedStages.map(function(capture) {
        return capture.stageLabel;
      }).join(' · ')]);
    }
    sourceRows.forEach(function(row) {
      sourceSummary.appendChild(node('dt', '', row[0])); sourceSummary.appendChild(node('dd', '', row[1]));
    });
    inspector.appendChild(sourceSummary);

    var eventContextChoices = launchContextChoices(state, scene);
    if (eventContextChoices.length) {
      inspector.appendChild(node('h3', '', 'Native launch context'));
      var activeEventContext = launchContextChoice(state, scene, null);
      var eventContextSelect = node('select', 'cutscene-background-select');
      eventContextSelect.setAttribute('data-cutscene-focus-key', 'native-launch-context');
      eventContextChoices.forEach(function(choice) {
        var option = node('option', '', choice.label);
        option.value = choice.id;
        eventContextSelect.appendChild(option);
      });
      eventContextSelect.value = activeEventContext.id;
      eventContextSelect.addEventListener('change', function() {
        var selectedId = eventContextSelect.value;
        var view = viewFor(state, scene.sceneId);
        view.launchContextId = selectedId;
        var selectedChoice = launchContextChoice(state, scene, selectedId);
        if (state.callbacks.onStatus) {
          state.callbacks.onStatus('Loading ' + selectedChoice.label + '…');
        }
        ensureContextualRuntime(rom, state, scene, document, selectedChoice, [])
          .then(function(runtime) {
            if (view.launchContextId !== selectedId) return;
            publishRuntime(state, scene, document, runtime);
            var duration = OB64.cutscenePreview.sceneDurationFrames(
              document, view.pathId);
            view.frame = Math.min(view.frame, Math.max(0, duration - 1));
            rerender(rom, state);
          }).catch(function(error) {
            state.sourceErrors['runtime-context:' + scene.assetId] =
              error && error.message || String(error);
            rerender(rom, state);
          });
      });
      inspector.appendChild(field('Event invocation', eventContextSelect,
        'This selection changes launch-time constants and concurrent shared scene state. It never edits Director words.'));
      if (activeEventContext.contextScene) {
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'Concurrent state source: ' +
          OB64.cutsceneCatalog.displayName(activeEventContext.contextScene) +
          ' · offset ' + activeEventContext.context.concurrentDirectorTickOffset +
          ' native update' +
          (activeEventContext.context.concurrentDirectorTickOffset === 1 ? '' : 's') + '.'));
      }
    }

    var untimedDialogueCaptures = capturedStages.filter(function(capture) {
      return capture.dialogueAssociation;
    });
    if (untimedDialogueCaptures.length) {
      inspector.appendChild(node('h3', '', 'Stored dialogue'));
      untimedDialogueCaptures.forEach(function(capture) {
        var association = capture.dialogueAssociation;
        var entry = state.catalog.getDialogueEntry(association.entryId);
        var dialogue = node('div', 'cutscene-untimed-dialogue');
        var heading = node('div', 'cutscene-untimed-dialogue-heading');
        heading.appendChild(node('strong', '', association.speaker));
        heading.appendChild(capabilityBadge(association.capability, 'Dialogue'));
        dialogue.appendChild(heading);
        dialogue.appendChild(node('p', '', entry && entry.text || association.entryId));
        dialogue.appendChild(node('p', 'cutscene-field-hint',
          'Serifu archive selector ' + association.archiveSelector + ' · entry selector ' +
          association.entrySelector + ' · ' + capture.stageLabel));
        dialogue.appendChild(node('p', 'cutscene-field-hint', association.status));
        dialogue.appendChild(node('p', 'cutscene-field-hint', association.timelineStatus));
        inspector.appendChild(dialogue);
      });
    }

    inspector.appendChild(node('h3', '', 'Background'));
    var backgroundProjection = document.background.projection || {};
    var modeTwoLaunch = scene.launchProfile &&
      scene.launchProfile.directorMode.value === 2;
    if (modeTwoLaunch) {
      var environmentTable = state.catalog.getBackgroundSelectorTable(
        'background-table:mode2-environment:80');
      var foregroundTable = state.catalog.getBackgroundSelectorTable(
        'background-table:mode2-overlay:80');
      var launchBackground = scene.launchProfile.background.requests[0] ||
        scene.launchProfile.background.inheritedPresentation || null;
      var modeTwoContext = Object.assign({
        mode: 2,
        environmentSelector: launchBackground &&
          Number.isInteger(launchBackground.environmentSelector)
          ? launchBackground.environmentSelector : null,
        foregroundSelector: launchBackground &&
          Number.isInteger(launchBackground.foregroundSelector)
          ? launchBackground.foregroundSelector : null
      }, backgroundProjection.launchContext || {});
      inspector.appendChild(node('p', 'cutscene-field-hint',
        'The native launch pre-scan can seed the mode-two environment from the Director command. Foreground stays independent because launch flag bit 0x08 can replace the initializer copy. These controls override launch state without editing the stream.'));
      if (launchBackground && launchBackground.foregroundStatus) {
        inspector.appendChild(node('p', 'cutscene-field-hint',
          launchBackground.foregroundStatus));
      }

      function selectorLabel(entry, role) {
        var assetIds = role === 'environment'
          ? (entry.stageLayers || []).map(function(layer) { return layer.assetId; })
          : (entry.archiveAssetIds || []);
        var labels = assetIds.map(function(assetId) {
          var asset = state.catalog.getImageAsset(assetId);
          return asset ? asset.displayName : assetId;
        });
        return 'Selector ' + entry.selector + ' · ' +
          (labels.length ? labels.join(' + ') : 'inactive / empty');
      }

      function launchSelectorControl(label, field, table, role) {
        var wrapper = node('label', 'cutscene-field');
        wrapper.appendChild(node('span', '', label));
        var select = node('select', 'cutscene-background-select');
        select.setAttribute('data-cutscene-focus-key', 'mode-two-' + role + '-selector');
        var unresolved = node('option', '', 'External / unresolved');
        unresolved.value = '';
        select.appendChild(unresolved);
        (table && table.entries || []).forEach(function(entry) {
          var hasEnvironment = (entry.stageLayers || []).length > 0;
          if (role === 'environment' && !hasEnvironment) return;
          var option = node('option', '', selectorLabel(entry, role));
          option.value = String(entry.selector);
          option.title = entry.associationStatus;
          select.appendChild(option);
        });
        select.value = Number.isInteger(modeTwoContext[field])
          ? String(modeTwoContext[field]) : '';
        select.addEventListener('change', function() {
          var selector = select.value === '' ? null : Number(select.value);
          executeEdit(rom, state, 'Set mode-two launch ' + role + ' selector', function(next) {
            var projection = Object.assign({}, next.background.projection || {});
            var context = Object.assign({
              mode: 2,
              environmentSelector: launchBackground &&
                Number.isInteger(launchBackground.environmentSelector)
                ? launchBackground.environmentSelector : null,
              foregroundSelector: launchBackground &&
                Number.isInteger(launchBackground.foregroundSelector)
                ? launchBackground.foregroundSelector : null,
              evidenceStatus: 'user-supplied-launch-context'
            }, projection.launchContext || {});
            context[field] = selector;
            context.override = Number.isInteger(context.environmentSelector) ||
              Number.isInteger(context.foregroundSelector);
            projection.launchContext = context;
            if (context.override) {
              projection.mode = 'stage-fit';
              projection.previewOverride = false;
            }
            next.background.projection = projection;
            next.exportRequirements.capability = 'preview-only';
            next.exportRequirements.reasons = (next.exportRequirements.reasons || [])
              .filter(function(reason) {
                return !/mode-two launch selectors/i.test(reason);
              });
            if (context.override) {
              next.exportRequirements.reasons.push(
                'Mode-two launch selectors are project context and are not encoded in the Director stream.');
            }
          });
        });
        wrapper.appendChild(select);
        return wrapper;
      }

      inspector.appendChild(launchSelectorControl(
        'Native launch environment', 'environmentSelector', environmentTable, 'environment'));
      inspector.appendChild(launchSelectorControl(
        'Native launch foreground', 'foregroundSelector', foregroundTable, 'foreground'));
    }
    if (backgroundProjection.nativeEditable === true &&
        Array.isArray(backgroundProjection.nativeGroups) &&
        backgroundProjection.nativeGroups.length) {
      inspector.appendChild(node('p', 'cutscene-field-hint',
        'Native scene groups replace the complete ordered background set. Individual archive choices below remain visual-preview overrides.'));
      var nativeGroupSelect = node('select', 'cutscene-background-select');
      nativeGroupSelect.setAttribute('data-cutscene-focus-key', 'native-background-group');
      var chooseNativeGroup = node('option', '', 'Choose a Native scene group');
      chooseNativeGroup.value = '';
      nativeGroupSelect.appendChild(chooseNativeGroup);
      backgroundProjection.nativeGroups.forEach(function(group) {
        var labels = group.archiveAssetIds.map(function(assetId) {
          var asset = state.catalog.getImageAsset(assetId);
          return asset ? asset.displayName : assetId;
        });
        var option = node('option', '', 'Group ' + group.selector + ' · ' + labels.join(' + '));
        option.value = String(group.selector);
        option.title = group.associationStatus;
        nativeGroupSelect.appendChild(option);
      });
      nativeGroupSelect.value = document.background.capability === 'native' &&
          Number.isInteger(backgroundProjection.selectedSelector)
        ? String(backgroundProjection.selectedSelector) : '';
      nativeGroupSelect.addEventListener('change', function() {
        if (nativeGroupSelect.value === '') return;
        var selector = Number(nativeGroupSelect.value);
        var selectedGroup = backgroundProjection.nativeGroups.find(function(group) {
          return group.selector === selector;
        });
        if (!selectedGroup) return;
        executeEdit(rom, state, 'Select native scene background group', function(next) {
          var projection = next.background.projection;
          next.background.assetId = selectedGroup.archiveAssetIds[0] || null;
          next.background.capability = 'native';
          var members = Array.isArray(selectedGroup.members) &&
              selectedGroup.members.length === selectedGroup.archiveAssetIds.length
            ? selectedGroup.members : selectedGroup.archiveAssetIds.map(function(assetId, index) {
              return { ordinal: index, assetId: assetId };
            });
          next.background.layers = members.map(function(member, index) {
            var assetId = member.assetId;
            var asset = state.catalog.getImageAsset(assetId);
            return {
              id: index === 0 ? 'background:base' : 'background:layer:' + index,
              assetId: assetId,
              label: asset ? asset.displayName : assetId,
              visible: true,
              depth: member.ordinal,
              nativeOrdinal: member.ordinal,
              capability: 'native',
              source: {
                sourceKind: 'native-scene-group',
                selector: selectedGroup.selector,
                groupResourceKey: selectedGroup.groupResourceKey,
                traversalOrdinal: member.ordinal,
                associationStatus: selectedGroup.associationStatus
              }
            };
          });
          projection.mode = next.background.layers.length ? 'stage-fit' : 'unresolved';
          projection.selectedSelector = selectedGroup.selector;
          projection.selectedGroupResourceKey = selectedGroup.groupResourceKey;
          projection.previewOverride = false;
          var priorCapability = next.exportRequirements.capability;
          next.exportRequirements.reasons = (next.exportRequirements.reasons || []).filter(function(reason) {
            return !/background/i.test(reason);
          });
          next.exportRequirements.capability = next.exportRequirements.reasons.length &&
              priorCapability !== 'native' ? 'preview-only' : 'native';
        });
      });
      inspector.appendChild(nativeGroupSelect);
    } else {
      inspector.appendChild(node('p', 'cutscene-field-hint',
        'This scene has no writable mode-0 background route. Archive choices are visual previews only.'));
    }
    var backgroundSelect = node('select', 'cutscene-background-select');
    backgroundSelect.setAttribute('data-cutscene-focus-key', 'background-select');
    var none = node('option', '', 'Unresolved / none'); none.value = '';
    backgroundSelect.appendChild(none);
    if (scene.backgroundAssetIds && scene.backgroundAssetIds.length) {
      var exactGroup = node('optgroup');
      exactGroup.label = 'Runtime-observed scene background';
      scene.backgroundAssetIds.forEach(function(assetId) {
        var asset = state.catalog.getImageAsset(assetId);
        if (!asset) return;
        var option = node('option', '', imageSourceLabel(asset) + ' · ' + asset.displayName +
          ' · Runtime observed');
        option.title = imageEvidenceHint(asset);
        option.value = asset.assetId;
        exactGroup.appendChild(option);
      });
      backgroundSelect.appendChild(exactGroup);
    }
    var exactBackgroundIds = scene.backgroundAssetIds || [];
    var candidateBackgroundIds = (scene.backgroundCandidateAssetIds || []).filter(function(assetId) {
      return exactBackgroundIds.indexOf(assetId) === -1;
    });
    if (candidateBackgroundIds.length) {
      var associatedGroup = node('optgroup');
      associatedGroup.label = 'Candidate scene-table matches (' + candidateBackgroundIds.length + ')';
      candidateBackgroundIds.forEach(function(assetId) {
        var asset = state.catalog.getImageAsset(assetId);
        if (!asset) return;
        var option = node('option', '', imageSourceLabel(asset) + ' · ' + asset.displayName +
          ' · Candidate');
        option.title = imageEvidenceHint(asset);
        option.value = asset.assetId;
        associatedGroup.appendChild(option);
      });
      backgroundSelect.appendChild(associatedGroup);
    }
    [
      ['Scene background archives', function(asset) { return asset.family === 'background'; }],
      ['Section C still candidates', function(asset) { return asset.family === 'background-candidate'; }],
      ['Effect images', function(asset) { return asset.family === 'effect'; }],
      ['Other images and sprites', function(asset) {
        return ['background', 'background-candidate', 'effect'].indexOf(asset.family) === -1;
      }]
    ].forEach(function(groupDefinition) {
      var assets = state.catalog.imageAssets.filter(groupDefinition[1]);
      if (!assets.length) return;
      var group = node('optgroup');
      group.label = groupDefinition[0] + ' (' + assets.length + ')';
      assets.forEach(function(asset) {
        var option = node('option', '', imageSourceLabel(asset) + ' · ' + asset.displayName +
          (asset.renderable ? '' : ' · Needs research'));
        option.title = imageEvidenceHint(asset);
        option.value = asset.assetId;
        group.appendChild(option);
      });
      backgroundSelect.appendChild(group);
    });
    backgroundSelect.value = document.background.assetId || '';
    backgroundSelect.addEventListener('change', function() {
      var assetId = backgroundSelect.value || null;
      var asset = assetId && state.catalog.getImageAsset(assetId);
      executeEdit(rom, state, assetId ? 'Replace scene background' : 'Remove scene background',
        function(next) {
          if (!asset) {
            next.background.assetId = null;
            next.background.capability = 'needs-research';
            next.background.layers = [];
            next.background.projection = Object.assign({}, next.background.projection, {
              mode: 'unresolved',
              selectedSelector: null,
              selectedGroupResourceKey: null,
              previewOverride: true
            });
            next.exportRequirements.capability = 'preview-only';
            next.exportRequirements.reasons = (next.exportRequirements.reasons || [])
              .filter(function(reason) { return !/background/i.test(reason); })
              .concat(['The source background was removed in the visual storyboard only.']);
            return;
          }
          next.background.assetId = assetId;
          next.background.capability = asset ? asset.previewCapability : 'needs-research';
          next.background.layers = asset ? [{
            id: 'background:base', assetId: asset.assetId, label: asset.displayName,
            visible: true, depth: 0, capability: asset.previewCapability,
            source: {
              sourceKind: asset.sourceKind,
              archiveIndex: asset.archiveIndex,
              z64Start: asset.source.z64Start == null ? null : asset.source.z64Start
            }
          }] : [];
          next.background.projection = Object.assign({}, next.background.projection, {
            mode: 'stage-fit',
            selectedSelector: null,
            selectedGroupResourceKey: null,
            previewOverride: true
          });
          next.exportRequirements.capability = 'preview-only';
          next.exportRequirements.reasons = (next.exportRequirements.reasons || [])
            .filter(function(reason) { return !/background/i.test(reason); })
            .concat(['This individual background archive is a visual-preview override.']);
        });
    });
    inspector.appendChild(backgroundSelect);
    var backgroundStatus = node('p', 'cutscene-background-status');
    inspector.appendChild(backgroundStatus);
    if (scene.backgroundRequests && scene.backgroundRequests.length) {
      var backgroundRoute = scene.backgroundRequests[0];
      var routeText = 'Native request operand ' + backgroundRoute.commandOperand + '. ' +
        backgroundRoute.modeStatus + '. Mode 2 ignores this operand and uses a separate runtime selector. Other modes use ' +
        (backgroundRoute.nonMode2Route.groupResourceKey == null
          ? 'an out-of-range scene-table selector.'
          : 'scene group 0x' + backgroundRoute.nonMode2Route.groupResourceKey
            .toString(16).toUpperCase().padStart(8, '0') + '.');
      inspector.appendChild(node('p', 'cutscene-field-hint', routeText));
    }

    var view = viewFor(state, scene.sceneId);
    renderClipInspector(inspector, rom, state, scene, document,
      findClipRow(document, view.selectedClipId));

    inspector.appendChild(node('h3', '', 'Cast'));
    var castActions = node('div', 'cutscene-inspector-actions');
    castActions.appendChild(button('Activate unused slot', 'btn-secondary', function() {
      addPreviewActor(rom, state, null);
    }));
    inspector.appendChild(castActions);
    var occupiedActorSlots = {};
    document.actors.concat(scene.actors || []).forEach(function(actor) {
      if (actor.slot != null && actor.slot < 20) occupiedActorSlots[actor.slot] = true;
    });
    inspector.appendChild(node('p', 'cutscene-field-hint', document.actors.length +
      ' actors · ' + (20 - Object.keys(occupiedActorSlots).length) +
      ' unclaimed slots in the observed 0–19 range. Place activates a free slot; Exit emits opcode 0x13; Deactivate removes that slot’s Place commands.'));
    var cast = node('div', 'cutscene-cast');
    if (!view.selectedActorId && document.actors.length) view.selectedActorId = document.actors[0].id;
    document.actors.forEach(function(actor) {
      var row = button(actor.label, 'cutscene-cast-row', function() {
        view.selectedActorId = actor.id; rerender(rom, state);
      });
      row.classList.toggle('active', actor.id === view.selectedActorId);
      row.appendChild(node('span', '', 'Slot ' + actor.slot));
      cast.appendChild(row);
    });
    inspector.appendChild(cast);
    var actor = document.actors.find(function(candidate) { return candidate.id === view.selectedActorId; });
    if (actor) renderActorInspector(inspector, rom, state, actor);
    shell.appendChild(inspector);
    state.ui.backgroundStatus = backgroundStatus;
    updateBackgroundStatus(state);
  }

  function renderActorInspector(inspector, rom, state, actor) {
    var title = node('div', 'cutscene-clip-heading');
    title.appendChild(node('h4', '', actor.label));
    title.appendChild(capabilityBadge(actor.capability));
    inspector.appendChild(title);
    var labelInput = node('input'); labelInput.type = 'text'; labelInput.value = actor.label;
    labelInput.setAttribute('data-cutscene-focus-key', 'actor-label:' + actor.id);
    labelInput.addEventListener('change', function() {
      executeEdit(rom, state, 'Rename actor', function(document) {
        document.actors.find(function(candidate) { return candidate.id === actor.id; }).label =
          labelInput.value.trim() || 'Actor slot ' + actor.slot;
      });
    });
    inspector.appendChild(field('Display name', labelInput, 'Project label; it does not change game text.'));
    var bankMatch = String(actor.artSourceId || '').match(/:(\d+)$/);
    var bank = bankMatch ? Number(bankMatch[1]) : 1;
    var artSelect = node('select');
    artSelect.setAttribute('data-cutscene-focus-key', 'actor-bank:' + actor.id);
    state.catalog.actorArtSources.forEach(function(asset) {
      var option = node('option', '', asset.label + ' · ' + asset.poseCount + ' poses');
      option.value = asset.bank; artSelect.appendChild(option);
    });
    artSelect.value = String(bank);
    artSelect.addEventListener('change', function() {
      var nextBank = Number(artSelect.value);
      var program = preferredPoseSelection(state.catalog, nextBank);
      executeEdit(rom, state, 'Replace actor art source', function(document) {
        var next = document.actors.find(function(candidate) { return candidate.id === actor.id; });
        next.artSourceId = 'cutscene-art-bank:' + nextBank;
        next.source.bank = nextBank;
        if (program) {
          next.source.animationKey = program.animationKey;
          next.source.physicalStateId = program.physicalStateId;
          next.source.stateIndex = program.stateIndex;
          next.source.selectorStatus = poseSelectionStatus(program);
          next.initial.poseId = program.poseId;
          next.initial.facing = 'native-' + program.facing;
          next.capability = poseSelectionNeedsResearch(program) ? 'needs-research' :
            (actorHasNativePlace(next) ? 'native' : 'preview-only');
        } else {
          next.source.animationKey = null;
          next.initial.poseId = null;
          next.capability = 'needs-research';
        }
      });
    });
    inspector.appendChild(field('Default Actor Art Source', artSelect,
      'All 68 catalogued cutscene art banks are available. Exact Place-backed actors compile directly.'));

    var poseSelect = node('select');
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var poseAtFrame = actorPoseSelectionAtFrame(
      selectedDocument(state), actor, view.frame, view.pathId, state.catalog);
    var poseBank = poseAtFrame.bank;
    var poseSelections = poseSelectionsForBank(state.catalog, poseBank);
    poseSelections.forEach(function(selection) {
      var option = node('option', '', poseSelectionLabel(selection));
      option.value = poseSelectionId(selection); poseSelect.appendChild(option);
    });
    var actorSelection = poseAtFrame.selection;
    poseSelect.value = actorSelection ? poseSelectionId(actorSelection) : '';
    poseSelect.setAttribute('data-cutscene-focus-key', 'actor-pose:' + actor.id);
    poseSelect.addEventListener('change', function() {
      var selection = getPoseSelection(state.catalog, poseSelect.value);
      if (!selection) return;
      viewFor(state, selectedScene(state).sceneId).selectedActorId = actor.id;
      addPoseProgram(rom, state, selection, null, 'Set actor pose from catalog');
    });
    inspector.appendChild(field('Pose / animation at playhead', poseSelect,
      poseSelections.length + ' exact physical ROM states. Visual and empty choices are labeled separately.'));
    var appearance = node('select');
    for (var appearanceIndex = 0; appearanceIndex < 8; appearanceIndex++) {
      var appearanceOption = node('option', '', 'Appearance ' + appearanceIndex);
      appearanceOption.value = appearanceIndex; appearance.appendChild(appearanceOption);
    }
    appearance.value = String(Number.isInteger(actor.source.variantSelector) &&
      actor.source.variantSelector >= 0 ? actor.source.variantSelector : 0);
    appearance.setAttribute('data-cutscene-focus-key', 'actor-appearance:' + actor.id);
    appearance.addEventListener('change', function() {
      executeEdit(rom, state, 'Change actor appearance', function(document) {
        var next = document.actors.find(function(candidate) { return candidate.id === actor.id; });
        next.source.variantSelector = Number(appearance.value);
        next.source.variantSelectorStatus =
          'explicit sprite-child selector; unavailable children use native child-0 fallback';
        next.capability = actorHasNativePlace(next) ? 'native' : 'preview-only';
      });
    });
    inspector.appendChild(field('Default appearance', appearance,
      'Appearance is independent from the animation. Art with fewer children follows the native appearance-0 fallback.'));
    var facing = node('select'); facing.setAttribute('data-cutscene-focus-key', 'actor-facing:' + actor.id);
    ['Right / 0', 'Left / 1', 'Away / 2', 'Toward / 3'].forEach(function(label, index) {
      var option = node('option', '', label); option.value = index; facing.appendChild(option);
    });
    var facingMatch = String(actor.initial.facing).match(/^native-([0-3])$/);
    facing.value = facingMatch ? facingMatch[1] : '0';
    facing.addEventListener('change', function() {
      executeEdit(rom, state, 'Change actor facing', function(document) {
        document.actors.find(function(candidate) { return candidate.id === actor.id; }).initial.facing =
          'native-' + facing.value;
      });
    });
    inspector.appendChild(field('Default facing', facing,
      'Place-backed actors compile this initial facing. Use a pose step for a timed facing change.'));
    var coordinates = node('div', 'cutscene-coordinate-grid');
    ['x', 'y', 'z'].forEach(function(axis) {
      coordinates.appendChild(field(axis.toUpperCase(), numericInput(actor.initial[axis], '0.001',
        function(value) {
          executeEdit(rom, state, 'Move actor ' + axis.toUpperCase(), function(document) {
            document.actors.find(function(candidate) { return candidate.id === actor.id; }).initial[axis] = value;
          });
        }, 'actor-' + axis + ':' + actor.id)));
    });
    inspector.appendChild(coordinates);
    inspector.appendChild(node('p', 'cutscene-field-hint',
      'These coordinates compile into the actor Place command when that exact boundary exists. Stage dragging adds or edits a timed pose.'));
    var actions = node('div', 'cutscene-inspector-actions');
    actions.appendChild(button('Set pose now', 'btn-secondary', function() {
      viewFor(state, selectedScene(state).sceneId).selectedActorId = actor.id;
      addPose(rom, state);
    }));
    actions.appendChild(button('Enter', 'btn-secondary', function() {
      viewFor(state, selectedScene(state).sceneId).selectedActorId = actor.id; addEnter(rom, state);
    }));
    actions.appendChild(button('Exit', 'btn-secondary', function() {
      viewFor(state, selectedScene(state).sceneId).selectedActorId = actor.id; addExit(rom, state);
    }));
    actions.appendChild(button('Duplicate', 'btn-secondary', function() {
      addPreviewActor(rom, state, actor);
    }));
    actions.appendChild(button('Deactivate actor', 'btn-secondary cutscene-remove-action', function() {
      executeEdit(rom, state, 'Remove actor', function(document) {
        OB64.cutsceneModel.removeActor(document, actor.id);
      });
    }));
    inspector.appendChild(actions);
  }

  function renderLoading(panel, scene) {
    panel.innerHTML = '';
    var loading = node('div', 'cutscene-loading');
    loading.appendChild(node('h2', '', 'Opening ' + OB64.cutsceneCatalog.displayName(scene)));
    loading.appendChild(node('p', '', scene.engine === 'director'
      ? 'Verifying and decoding the selected Director resource…'
      : 'Opening the visual Storyboard workspace for this presentation…'));
    panel.appendChild(loading);
  }

  function render(panel, rom, callbacks, restoreSnapshot) {
    var state = ensureState(rom);
    if (!restoreSnapshot && state.ui && state.ui.panel === panel && panel.firstChild) {
      restoreSnapshot = captureUi(panel);
    }
    state.callbacks = callbacks || state.callbacks || {};
    pauseAnimation(state);
    var scene = selectedScene(state);
    var history = historyFor(state, scene);
    var runtimeMissing = history && scene.engine === 'director' &&
      state.programByAssetId[scene.assetId] &&
      !state.runtimeByAssetId[scene.assetId];
    if (!history || runtimeMissing) {
      state.ui = { panel: panel };
      renderLoading(panel, scene);
      restoreUi(panel, restoreSnapshot);
      var request = ++state.openRequest;
      loadScene(rom, state, scene).then(function() {
        if (request !== state.openRequest || state.selectedSceneId !== scene.sceneId) return;
        render(panel, rom, state.callbacks, restoreSnapshot);
      });
      return;
    }
    panel.innerHTML = '';
    var shell = node('div', 'cutscene-studio');
    shell.setAttribute('data-cutscene-scene-id', scene.sceneId);
    panel.appendChild(shell);
    state.ui = { panel: panel };
    renderSceneBrowser(shell, rom, state);
    renderStageArea(shell, rom, state, scene, history.present);
    renderInspector(shell, rom, state, scene, history.present);
    paintStage(rom, state);
    requestBackground(rom, state, history.present);
    restoreUi(panel, restoreSnapshot);
  }

  function resetAll(state) {
    state.histories = {};
    state.originalSerialized = {};
    state.sourceByAssetId = {};
    state.programByAssetId = {};
    state.runtimeByAssetId = {};
    state.projectionLoadingByAssetId = {};
    state.concurrentRuntimeByLaunchContext = {};
    state.sourceErrors = {};
    state.views = {};
    state.imageCache = {};
    state.imageCacheBytes = 0;
    pauseAnimation(state);
  }

  OB64.cutsceneUI = {
    UiError: UiError,
    imageCacheLimit: IMAGE_CACHE_LIMIT,
    initialize: initialize,
    ensureState: ensureState,
    selectedScene: selectedScene,
    selectedDocument: selectedDocument,
    initialViewFrame: initialViewFrame,
    launchContextChoices: launchContextChoices,
    launchContextChoice: launchContextChoice,
    launchOperandTranslations: launchOperandTranslations,
    loadScene: loadScene,
    presentationDocument: presentationDocument,
    decodeImageAsset: decodeImageAsset,
    captureUi: captureUi,
    restoreUi: restoreUi,
    sceneHasChanges: sceneHasChanges,
    editCount: editCount,
    refreshExportRequirements: refreshExportRequirements,
    insertionBoundaries: insertionBoundaries,
    boundaryForFrame: boundaryForFrame,
    findClipRow: findClipRow,
    directorSourceRows: directorSourceRows,
    directorEditorTarget: directorEditorTarget,
    actorPoseSelectionAtFrame: actorPoseSelectionAtFrame,
    render: render,
    resetAll: resetAll,
    pause: pauseAnimation
  };
})(window.OB64);
