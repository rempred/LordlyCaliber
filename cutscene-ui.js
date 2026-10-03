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
      sceneFilterRow: '',
      renderedStage: null,
      spriteState: OB64.cutsceneSprites
        ? OB64.cutsceneSprites.create(rom.z64, catalog) : null,
      drag: null,
      animationFrame: null,
      lastAnimationTime: null
    };
    var created = rom.cutsceneStudio;
    // ROM-only reopen: authored scenes live in the Cutscene arena. Register them
    // once discovered; a disagreement between records and manifest fails closed.
    // Event bindings are read afterwards, since a bound site may launch one of
    // those scenes. Both fail closed together: export and Project save refuse.
    created.eventBindings = [];
    created.romEventBindings = [];
    if (!OB64.cutsceneAuthored) {
      created.authoredDiscovery = { status: 'unavailable', count: 0 };
      created.authoredReady = Promise.resolve();
      return created;
    }
    created.authoredDiscovery = { status: 'pending', count: 0 };
    var foundCount = 0;
    created.authoredReady = OB64.cutsceneAuthored.discover(rom.z64, catalog).then(function(found) {
      found.forEach(function(item) { catalog.registerAuthoredScene(item.scene); });
      foundCount = found.length;
      return OB64.cutsceneEvents ? OB64.cutsceneEvents.discover(rom.z64, catalog) : [];
    }).then(function(bindings) {
      return discoverWipes(rom, created).then(function() { return bindings; });
    }).then(function(bindings) {
      created.eventBindings = JSON.parse(JSON.stringify(bindings));
      created.romEventBindings = JSON.parse(JSON.stringify(bindings));
      created.authoredDiscovery = { status: 'ok', count: foundCount };
      if ((foundCount || bindings.length) && created.ui) rerender(rom, created);
    }).catch(function(error) {
      created.authoredDiscovery = { status: 'failed', message: error && error.message || String(error) };
      created.sourceErrors['authored-discovery'] = created.authoredDiscovery.message;
      if (created.ui) rerender(rom, created);
    });
    return created;
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
        selectedClipId: null, selectedSourceId: null, timelineMode: 'preview',
        editPanel: 'actors',
        launchContextId: null, loop: false, snap: 'frame', zoom: 1
      };
    }
    return state.views[sceneId];
  }

  // A bound launch site keeps its retail launch record; the scene it now
  // launches previews with that site's invocation contexts.
  function boundLaunchRecords(state, scene) {
    var events = OB64.cutsceneEvents;
    if (!events || !state.eventBindings || !state.eventBindings.length) return [];
    return events.bindingsForScene(state.catalog, state.eventBindings, scene).map(function(binding) {
      var site = events.site(state.catalog, binding.launchSiteId);
      var owner = site && state.catalog.getScene(site.retailSceneId);
      var launches = owner && owner.launchProfile && owner.launchProfile.parentEventLaunches || [];
      return launches.find(function(launch) { return launch.launchId === binding.launchSiteId; }) || null;
    }).filter(Boolean);
  }

  function effectivePredecessorContext(state, context) {
    var launchId = context && context.precedingDirectorLaunchId;
    var binding = launchId && (state.eventBindings || []).find(function(item) { return item.launchSiteId === launchId; });
    if (!binding) return context;
    var target = binding.target;
    var targetScene = target.kind === 'authored'
      ? state.catalog.getScene('scene:authored:' + target.authoredId) : state.catalog.getScene(target.sceneId);
    if (!targetScene) return context;
    return Object.assign({}, context, {
      precedingDirectorSelector: target.kind === 'authored' ? targetScene.authored.selectorRow : target.selectorRow,
      concurrentDirectorSceneId: targetScene.sceneId,
      reboundPredecessorLaunchId: launchId
    });
  }

  function launchContextChoices(state, scene) {
    if (!scene || !scene.launchProfile ||
        !Array.isArray(scene.launchProfile.parentEventLaunches)) return [];
    var choices = [];
    // Retail sites now bound to another scene no longer launch this one; keep
    // them only when nothing else would give the preview a context.
    var reboundAway = {};
    (state.eventBindings || []).forEach(function(binding) { reboundAway[binding.launchSiteId] = true; });
    var bound = boundLaunchRecords(state, scene);
    var ownLaunches = scene.launchProfile.parentEventLaunches.filter(function(launch) {
      return !reboundAway[launch.launchId] || OB64.cutsceneEvents &&
        OB64.cutsceneEvents.bindingsForScene(state.catalog, state.eventBindings, scene)
          .some(function(binding) { return binding.launchSiteId === launch.launchId; });
    });
    if (!ownLaunches.length && !bound.length) ownLaunches = scene.launchProfile.parentEventLaunches;
    var boundIds = {};
    bound.forEach(function(launch) { boundIds[launch.launchId] = true; });
    ownLaunches.filter(function(launch) { return !boundIds[launch.launchId]; }).concat(bound).forEach(function(launch) {
      (launch.eventInvocationContexts || []).forEach(function(retailContext, contextIndex) {
        // The generated context is retail. When the site that launched the
        // preceding scene is bound elsewhere, the predecessor is the bound
        // target: its scene, selector row and inherited state.
        var context = effectivePredecessorContext(state, retailContext);
        var owner = context.concurrentDirectorSceneId
          ? state.catalog.getScene(context.concurrentDirectorSceneId) : null;
        var id = launch.launchId + ':invocation:' + context.eventInvocationCursor +
          ':context:' + contextIndex;
        var ownerLabel = owner
          ? 'after ' + OB64.cutsceneCatalog.displayName(owner)
          : 'no exact earlier Director';
        choices.push({
          id: id,
          label: (boundIds[launch.launchId] ? 'Assigned · ' : '') +
            'Event row ' + launch.eventDirectoryRow + ' · byte 0x' +
            launch.decodedByteOffset.toString(16).toUpperCase().padStart(4, '0') +
            ' · ' + ownerLabel,
          bound: !!boundIds[launch.launchId],
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

  function importedFreshLaunch(state, scene) {
    var input = state.nativeLaunchInputsByAssetId && state.nativeLaunchInputsByAssetId[scene.assetId];
    var external = input && input.externalProducers;
    return !!(external && external.status === 'known' && external.value &&
      external.value.directorLaunch !== undefined);
  }

  function runtimeLaunchChoice(state, scene, forcedChoice) {
    // Fresh launch owns its initial namespace. Preserve the event selection for
    // ordinary contextual playback after the imported profile is cleared.
    if (importedFreshLaunch(state, scene)) return null;
    return forcedChoice || launchContextChoice(state, scene, null);
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

  // Story-cast scenes (event rows 66 and 67): the event program scans the
  // player's army for up to five story characters and fills launch translation
  // entries 0..16 before the Director parses the stream. T0 is Actor 5's
  // variant, T1..5 the characters' primary classes, T6..10 their secondary
  // classes, T11..15 the dialogue classifier of each primary class, and T16
  // Actor 5's portrait. None of them is a coordinate. The army is not in the
  // ROM, so the preview uses a declared sample cast; exports keep every
  // 0x0888xxxx word unchanged.
  var STORY_CAST_CLASSIFIER = [
    [[0x56, 0x57], 0], [[0x54, 0x55], 1], [[0x61], 2], [[0x58], 3], [[0x5B], 4], [[0x5C], 5],
    [[0x59], 6], [[0x5A], 7], [[0x5E], 8], [[0x77], 9], [[0x7A], 10], [[0x78], 11], [[0x75], 12],
    [[0x7B], 13], [[0x5D], 14], [[0x79], 15], [[0x5F, 0x60], 16], [[0x76], 17], [[0x7C], 18], [[0x04], 19],
    // The event program's filler pair when fewer than five story characters
    // are in the army: an ordinary Fighter, dialogue classifier 20.
    [[0x02], 20]
  ];
  var STORY_CAST_SAMPLE = [0x58, 0x59, 0x5A, 0x5B, 0x5C];
  var STORY_CAST_FIXED = { 0: 2, 16: 83 };

  function storyCastClassifier(classId) {
    var row = STORY_CAST_CLASSIFIER.find(function(entry) { return entry[0].indexOf(classId) !== -1; });
    return row ? row[1] : 20;
  }

  function isStoryCastScene(scene) {
    var translation = scene && scene.launchProfile && scene.launchProfile.operandTranslation;
    var indexes = translation && translation.tableIndexes || [];
    return !!translation && translation.required === true && indexes.length === 17 &&
      indexes.every(function(tableIndex, position) { return tableIndex === position; }) &&
      (scene.launchProfile.parentEventLaunches || []).some(function(launch) {
        return launch.eventDirectoryRow === 66 || launch.eventDirectoryRow === 67;
      });
  }

  function storyCastSample(state, scene) {
    var view = viewFor(state, scene.sceneId);
    return Array.isArray(view.previewCast) && view.previewCast.length === 5
      ? view.previewCast.slice() : STORY_CAST_SAMPLE.slice();
  }

  function storyCastTranslations(cast) {
    var table = Object.assign({}, STORY_CAST_FIXED);
    cast.forEach(function(classId, index) {
      table[index + 1] = classId;
      table[index + 6] = classId;
      table[index + 11] = storyCastClassifier(classId);
    });
    return table;
  }

  function launchOperandTranslations(scene, choice, state, imported) {
    var output = {};
    var context = choice && choice.context;
    var values = context && context.launchTranslationTable;
    var indexes = scene.launchProfile && scene.launchProfile.operandTranslation &&
      scene.launchProfile.operandTranslation.tableIndexes || [];
    // An imported table is explicit input and wins; exact launch-context
    // values come next. Only then does the preview sample fill what is still
    // missing: each character's primary class first, then its secondary class
    // and dialogue classifier from that resolved primary, so a partly supplied
    // character stays one coherent character.
    function known(tableIndex) {
      if (imported && Object.prototype.hasOwnProperty.call(imported, String(tableIndex))) return imported[tableIndex];
      if (Array.isArray(values) && Number.isInteger(values[tableIndex])) return values[tableIndex];
      return undefined;
    }
    var resolved = {};
    indexes.forEach(function(tableIndex) {
      var value = known(tableIndex);
      if (value !== undefined) resolved[tableIndex] = value;
    });
    if (state && isStoryCastScene(scene)) {
      var cast = storyCastSample(state, scene);
      cast.forEach(function(sampleClass, position) {
        var primaryIndex = position + 1;
        if (resolved[primaryIndex] === undefined) resolved[primaryIndex] = sampleClass;
        var primary = resolved[primaryIndex];
        if (resolved[primaryIndex + 5] === undefined) resolved[primaryIndex + 5] = primary;
        if (resolved[primaryIndex + 10] === undefined) resolved[primaryIndex + 10] = storyCastClassifier(primary);
      });
      Object.keys(STORY_CAST_FIXED).forEach(function(tableIndex) {
        if (resolved[tableIndex] === undefined) resolved[tableIndex] = STORY_CAST_FIXED[tableIndex];
      });
    }
    indexes.forEach(function(tableIndex) {
      if (resolved[tableIndex] !== undefined) output[tableIndex] = resolved[tableIndex];
    });
    return output;
  }

  function setPreviewCast(rom, state, scene, cast) {
    if (!isStoryCastScene(scene)) fail('This scene does not take its cast from the army.');
    if (!Array.isArray(cast) || cast.length !== 5 || cast.some(function(classId) {
      return !STORY_CAST_CLASSIFIER.some(function(entry) { return entry[0].indexOf(classId) !== -1; });
    })) fail('A preview cast needs five story characters.');
    viewFor(state, scene.sceneId).previewCast = cast.slice();
    var history = historyFor(state, scene);
    if (history) return refreshRuntime(state, scene, history.present, rom);
    return null;
  }

  function renderPreviewCast(header, rom, state, scene) {
    if (!isStoryCastScene(scene)) return;
    var cast = storyCastSample(state, scene);
    var line = node('div', 'cutscene-preview-cast');
    line.setAttribute('data-cutscene-focus-key', 'preview-cast');
    line.appendChild(node('span', 'cutscene-field-hint',
      'The game fills this cast from the story characters in the player\'s army. Preview cast (not exported):'));
    cast.forEach(function(classId, index) {
      var select = node('select', 'cutscene-preview-cast-member');
      select.setAttribute('aria-label', 'Preview cast member ' + (index + 1));
      select.setAttribute('data-cutscene-focus-key', 'preview-cast:' + index);
      STORY_CAST_CLASSIFIER.forEach(function(entry) {
        var option = node('option', '', OB64.className ? OB64.className(entry[0][0]) : 'Class ' + entry[0][0]);
        option.value = String(entry[0][0]);
        select.appendChild(option);
      });
      var shown = STORY_CAST_CLASSIFIER.find(function(entry) { return entry[0].indexOf(classId) !== -1; });
      select.value = String(shown ? shown[0][0] : classId);
      select.addEventListener('change', function() {
        var next = storyCastSample(state, scene);
        next[index] = Number(select.value);
        try { setPreviewCast(rom, state, scene, next); } catch (error) {
          if (state.callbacks.onStatus) state.callbacks.onStatus(error.message);
        }
        rerender(rom, state);
      });
      line.appendChild(select);
    });
    header.appendChild(line);
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
    if(program&&OB64.cutsceneRomStart)scene.previewResourceKind=OB64.cutsceneRomStart.analyze(program,{explicitlyWiped:!!scene.wiped}).resourceKind||null;
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

  // Runtime tick at which a card's native command executes, from the execution
  // trace. Null when no runtime, trace, or compiled position is available.
  function clipRuntimeTick(state, scene, clip) {
    var runtime = state.runtimeByAssetId && state.runtimeByAssetId[scene.assetId];
    var compiled = state.compiledClipWordsByAssetId && state.compiledClipWordsByAssetId[scene.assetId];
    // An edited scene executes its recompiled program, whose node ids follow the
    // compiled word positions; an unedited scene executes the source program.
    var program = compiled ? compiled.program : state.programByAssetId && state.programByAssetId[scene.assetId];
    if (!runtime || !program || !Array.isArray(runtime.trace) || !clip) return null;
    var nodeId = null;
    if (compiled) {
      if (Number.isInteger(compiled.words[clip.id])) {
        nodeId = 'node:' + scene.assetId.split(':').pop().toUpperCase() + ':w' +
          compiled.words[clip.id].toString(16).toUpperCase().padStart(4, '0');
      }
    } else if (clip.source && clip.source.nodeId) nodeId = clip.source.nodeId;
    if (!nodeId) return null;
    var cache = runtime.clipTickByNodeId = runtime.clipTickByNodeId || {};
    if (cache[nodeId] !== undefined) return cache[nodeId];
    var tick = null;
    for (var index = 0; index < runtime.trace.length; index++) {
      var entry = runtime.trace[index];
      if (entry.kind !== 'composite' || entry.streamAssetId && entry.streamAssetId !== scene.assetId) continue;
      var composite = program.compositeById[entry.compositeId];
      if (composite && composite.nodeIds.indexOf(nodeId) !== -1) { tick = entry.tick; break; }
    }
    cache[nodeId] = tick;
    return tick;
  }

  async function compileRuntimeDocument(state, scene, document, choice, contextRuntime, signal, rom, contextChain) {
    var program = state.programByAssetId[scene.assetId];
    if (!program) return null;
    state.compiledClipWordsByAssetId = state.compiledClipWordsByAssetId || {};
    state.compiledClipWordsByAssetId[scene.assetId] = null;
    var playbackRom=state.z64;
    var sharedTextEdited = Object.keys(state.histories).some(function(id) {
      return state.histories[id].present.tracks.some(function(track) {
        return track.clips.some(function(clip) {
          return clip.payload.nativeDialogueEditable && clip.payload.rawText !== clip.payload.originalRawText;
        });
      });
    });
    if(scene.authored || (scene.wiped && scene.wiped.pending) || sceneHasChanges(state,scene) || sharedTextEdited || (contextChain||[]).some(entry=>sceneHasChanges(state,entry.scene))){
      var source=state.sourceByAssetId[scene.assetId], baseline=baselineDocument(state,scene);
      var assessment=OB64.cutsceneExport.assessNativeDelta(scene,baseline,document,source);
      if(assessment.capability!=='native')throw new UiError('These changes are storyboard-only: '+assessment.reasons.join(' '));
      // Dialogue entries are shared between scenes. Include native edits from
      // the Project so this preview reads the same resources as the exported ROM.
      var histories = {};
      Object.keys(state.histories).forEach(function(id) {
        var history = state.histories[id];
        if (id === scene.storageId || history.present.exportRequirements.capability === 'native') histories[id] = history;
      });
      var single=Object.assign({},state,{histories:histories});
      var candidate={z64:state.z64.slice(),layout:{id:'us-rev0'}};
      var plan=await OB64.cutsceneExport.prepare({z64:state.z64,layout:candidate.layout,cutsceneStudio:single},candidate);
      if(signal&&signal.aborted){var cancelled=new Error('Cutscene preparation cancelled.');cancelled.name='AbortError';throw cancelled;}
      OB64.cutsceneExport.apply(candidate,plan);OB64.cutsceneExport.validateApplied(candidate,plan);
      playbackRom=candidate.z64;
      var compiled=OB64.cutsceneCodec.compileSceneDocument(scene,source,document);
      // Remember where each card's command sits in the compiled stream so card
      // selection can seek to its exact runtime tick.
      var clipWords={};
      compiled.changes.forEach(function(change){if(change.operation==='insert'&&change.clipId)clipWords[change.clipId]=change.startWord;});
      document.tracks.forEach(function(track){track.clips.forEach(function(clip){
        var nodeId=clip.source&&clip.source.nodeId;
        if(nodeId&&Number.isInteger(compiled.nodeStartWords[nodeId]))clipWords[clip.id]=compiled.nodeStartWords[nodeId];
      });});
      scene=OB64.cutsceneCodec.sceneForBytes(scene,compiled.decodedBytes);
      // Plan entries without a scene (dialogue, manifest, event rows) are skipped.
      var plannedScene = plan.changedEntries.find(function(entry) { return entry.scene && entry.scene.assetId === scene.assetId; });
      // ROM startup parses directorKey as hex, so keep the codec's string form.
      if(plannedScene && plannedScene.result.resourceKey)scene.directorKey='0x'+plannedScene.result.resourceKey.toString(16).toUpperCase().padStart(8,'0');
      program=OB64.cutsceneCodec.createIr(scene,compiled.decodedBytes).program;
      state.compiledClipWordsByAssetId[scene.assetId]={words:clipWords,program:program};
    }
    var runtimeOptions = {
      z64: playbackRom,
      signal: signal,
      diagnosticAssumptions: state.diagnosticAssumptions === true,
      launchContext: choice ? choice.context : null,
      launchOperandTranslations: launchOperandTranslations(scene, choice, state)
    };
    runtimeOptions.captureFrame=function(request){return capturePreviewFrame(rom,state,document,request);};
    runtimeOptions.nativeLaunchInputs = state.nativeLaunchInputsByAssetId &&
      state.nativeLaunchInputsByAssetId[scene.assetId] || null;
    var importedLaunch = runtimeOptions.nativeLaunchInputs && runtimeOptions.nativeLaunchInputs.externalProducers &&
      runtimeOptions.nativeLaunchInputs.externalProducers.value &&
      runtimeOptions.nativeLaunchInputs.externalProducers.value.directorLaunch || null;
    if (importedLaunch && importedLaunch.operandTranslations) {
      runtimeOptions.launchOperandTranslations = launchOperandTranslations(scene, choice, state,
        importedLaunch.operandTranslations);
      // A per-run copy: the stored inputs stay exactly as imported.
      var importedProducers = runtimeOptions.nativeLaunchInputs.externalProducers;
      runtimeOptions.nativeLaunchInputs = Object.assign({}, runtimeOptions.nativeLaunchInputs, {
        externalProducers: Object.assign({}, importedProducers, { value: Object.assign({}, importedProducers.value, {
          directorLaunch: Object.assign({}, importedLaunch, {
            operandTranslations: Object.assign({}, runtimeOptions.launchOperandTranslations) }) }) }) });
    }
    if(!state.romStartupByAssetId)state.romStartupByAssetId={};
    state.romStartupByAssetId[scene.assetId]=false;
    if(!runtimeOptions.nativeLaunchInputs&&OB64.cutsceneRomStart){
      var startupScene=scene,startupProgram=program;
      if(contextChain&&contextChain.length){
        runtimeOptions.romContextChain=contextChain.map(function(entry){
          var contextScene=entry.scene,contextProgram=entry.program;
          if(sceneHasChanges(state,contextScene)){
            var compiledContext=OB64.cutsceneCodec.compileSceneDocument(contextScene,state.sourceByAssetId[contextScene.assetId],entry.document);
            contextScene=OB64.cutsceneCodec.sceneForBytes(contextScene,compiledContext.decodedBytes);
            var plannedContext=plan&&plan.changedEntries.find(change=>change.scene&&change.scene.assetId===contextScene.assetId);
            if(plannedContext&&plannedContext.result.resourceKey)contextScene.directorKey='0x'+plannedContext.result.resourceKey.toString(16).toUpperCase().padStart(8,'0');
            contextProgram=OB64.cutsceneCodec.createIr(contextScene,compiledContext.decodedBytes).program;
          }
          return Object.assign({},entry,{scene:contextScene,program:contextProgram});
        });
        startupScene=runtimeOptions.romContextChain[0].scene;
        startupProgram=runtimeOptions.romContextChain[0].program;
      }
      runtimeOptions.nativeLaunchInputs=OB64.cutsceneRomStart.input(playbackRom,startupScene,startupProgram);
      // Each choice previews with its own chosen option (1 by default), in
      // the order the choices appear.
      var previewSequence=choicePreviewSequence(state,scene,document);
      var schedule=runtimeOptions.nativeLaunchInputs.externalProducers&&runtimeOptions.nativeLaunchInputs.externalProducers.value&&runtimeOptions.nativeLaunchInputs.externalProducers.value.resourceSchedule;
      if(schedule&&previewSequence.some(function(option){return option===2;})){schedule.dialogueChoicePolicy='sequence';schedule.dialogueChoiceSequence=previewSequence;}
      runtimeOptions.nativeLaunchInputs.assetId=scene.assetId;
      var generatedLaunch=runtimeOptions.nativeLaunchInputs&&runtimeOptions.nativeLaunchInputs.externalProducers.value.directorLaunch;
      if(!(contextChain&&contextChain.length)&&generatedLaunch&&Object.keys(runtimeOptions.launchOperandTranslations).length)
        generatedLaunch.operandTranslations=Object.assign({},runtimeOptions.launchOperandTranslations);
      if(contextChain&&contextChain.length){
        generatedLaunch.operandTranslations=contextChain[0].translations;
        runtimeOptions.romContextStartTick=contextChain.at(-1).nextStartTick;
        runtimeOptions.romContextTailCallSelector=choice&&choice.context.tailCallSelector;
      }
      state.romStartupByAssetId[scene.assetId]=generatedLaunch&&generatedLaunch.interfacePreview?'interface':generatedLaunch&&generatedLaunch.preservedStage?'preserved-stage':generatedLaunch&&generatedLaunch.previewParty?'preview-party':generatedLaunch&&generatedLaunch.sceneMode===0?'room':!!generatedLaunch;
      if(contextChain&&contextChain.length)state.romStartupByAssetId[scene.assetId]='inherited';
      else if(generatedLaunch&&generatedLaunch.previewEnvironmentDefault)state.romStartupByAssetId[scene.assetId]='preview-party-terrain';
    }
    if (contextRuntime) {
      runtimeOptions.contextRuntime = contextRuntime;
      runtimeOptions.contextTickOffset = Number.isInteger(
        choice.context.concurrentDirectorTickOffset)
        ? choice.context.concurrentDirectorTickOffset : 1;
    }
    return OB64.cutsceneRuntime.compileAsync(
      document, program, scene, state.catalog, runtimeOptions);
  }

  // Roster Actors are created at launch, not by the stream, so the projection
  // only knows their slot. Once the runtime has run, name them by class, and mark
  // members of the inferred encountered unit.
  function rosterActorLabels(runtime) {
    var labels = {};
    if (!runtime || !Array.isArray(runtime.states) || !OB64.className) return labels;
    var launch = runtime.nativeLaunchInputs && runtime.nativeLaunchInputs.externalProducers &&
      runtime.nativeLaunchInputs.externalProducers.value.directorLaunch || {};
    var inferred = launch.previewEncounter ? launch.previewEncounter.classes : [];
    var party = launch.previewClasses || [];
    runtime.states.forEach(function(snapshot) {
      (snapshot.actors || []).forEach(function(actor) {
        if (labels[actor.slot] !== undefined || !actor.source ||
            actor.source.invocationId !== 'rom-start' || !Number.isInteger(actor.bank)) return;
        var name = actor.bank >= 81 && actor.bank <= 83 && launch.previewHeroName
          ? launch.previewHeroName : OB64.className(actor.bank);
        if (!name || /^Unknown_/.test(name)) return;
        var guessed = inferred.indexOf(actor.bank) !== -1 && party.indexOf(actor.bank) === -1;
        labels[actor.slot] = guessed ? name + ' (inferred)' : name;
      });
    });
    return labels;
  }

  function applyRosterActorLabels(state, scene, document, runtime) {
    var labels = rosterActorLabels(runtime);
    function relabel(target) {
      var changed = false;
      target.actors.forEach(function(actor) {
        if (labels[actor.slot] && /^Actor slot \d+$/.test(actor.label)) {
          actor.label = labels[actor.slot]; changed = true;
        }
      });
      return changed;
    }
    if (!Object.keys(labels).length || !relabel(document)) return;
    var original = state.originalSerialized[scene.storageId];
    if (original) {
      var baseline = OB64.cutsceneModel.parseSceneDocument(original);
      if (relabel(baseline)) state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(baseline, 0);
    }
  }

  function publishRuntime(state, scene, document, runtime) {
    if (!runtime) return null;
    applyRosterActorLabels(state, scene, document, runtime);
    if (state.boundRuntimeDocument) OB64.cutsceneRuntime.unbind(state.boundRuntimeDocument);
    state.boundRuntimeDocument = document;
    // Runtime bindings otherwise retain evicted frames through document history.
    Object.keys(state.runtimeByAssetId).forEach(function(assetId) {
      if (assetId === scene.assetId) return;
      var oldScene = state.catalog.getScene(assetId);
      var history = oldScene && historyFor(state, oldScene);
      if (history) OB64.cutsceneRuntime.unbind(history.present);
      delete state.runtimeByAssetId[assetId];
    });
    state.runtimeByAssetId[scene.assetId] = runtime;
    OB64.cutsceneRuntime.bind(document, runtime);
    delete state.sourceErrors['runtime:' + scene.assetId];
    delete state.sourceErrors['runtime-context:' + scene.assetId];
    return runtime;
  }

  function refreshRuntime(state, scene, document, rom) {
    if (!OB64.cutsceneRuntime || scene.engine !== 'director') return null;
    var choice = runtimeLaunchChoice(state, scene, null);
    var contextRuntime = choice && state.concurrentRuntimeByLaunchContext
      ? state.concurrentRuntimeByLaunchContext[
        launchContextCacheKey(scene, choice)] || null
      : null;
    if (state.runtimeController) state.runtimeController.abort();
    var controller = state.runtimeController = new AbortController();
    state.loadingAssetId = scene.assetId;
    return ensureContextualRuntime(rom,state,scene,document,choice,[],false,
      controller.signal).then(function(runtime) {
      if (controller.signal.aborted) return null;
      publishRuntime(state, scene, document, runtime);
      if (state.ui && state.ui.panel) rerender(rom, state);
      return runtime;
    }).catch(function(error) {
      if (error.name === 'AbortError') return null;
      delete state.runtimeByAssetId[scene.assetId];
      state.sourceErrors['runtime:' + scene.assetId] = error && error.message || String(error);
      return null;
    });
  }

  function ensureProjectedDocument(rom, state, scene) {
    var epoch = state.projectionEpoch || 0;
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
    var promise = new Promise(function(resolve) { setTimeout(resolve, 0); }).then(function() {
      if (epoch !== (state.projectionEpoch || 0)) throw new Error('Project replaced during scene loading.');
      // Authored scenes carry their own stream; they have no retail payload to read.
      if (scene.authored) return OB64.cutsceneCodec.authoredSceneSource(scene, scene.authored.initialDecodedBytes, { rom: rom.z64 });
      return OB64.cutsceneCodec.loadSceneSource(rom.z64, scene, {allowModified:true});
    }).then(function(source) {
      if (epoch !== (state.projectionEpoch || 0)) throw new Error('Project replaced during scene loading.');
      var projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, state.catalog);
      return cacheLoadedDocument(
        state, scene, projected.document, source, projected.program);
    }).catch(function(error) {
      if (epoch !== (state.projectionEpoch || 0)) throw error;
      var document = OB64.cutsceneCatalog.createSceneDocument(scene);
      cacheLoadedDocument(state, scene, document, null, null);
      state.sourceErrors[scene.assetId] = error && error.message || String(error);
      return document;
    }).finally(function() {
      if (state.projectionLoadingByAssetId[scene.assetId] === promise) {
        delete state.projectionLoadingByAssetId[scene.assetId];
      }
    });
    state.projectionLoadingByAssetId[scene.assetId] = promise;
    return promise;
  }

  async function tailCallChoice(rom,state,scene,signal){
    if(!state.romTailCalls){
      var calls={};
      for(var index=0;index<state.catalog.directorScenes.length;index++){
        if(signal&&signal.aborted){var cancelled=new Error('Cutscene preparation cancelled.');cancelled.name='AbortError';throw cancelled;}
        var parent=state.catalog.directorScenes[index];
        try{
          var source=await OB64.cutsceneCodec.loadSceneSource(rom.z64,parent,{allowModified:true});
          var words=new DataView(source.decodedBytes.buffer,source.decodedBytes.byteOffset,source.decodedBytes.byteLength),hasCall=false;
          for(var at=0;at<words.byteLength;at+=4)if(words.getUint32(at)===0x80000003){hasCall=true;break;}
          if(hasCall){
            var nodes=OB64.cutsceneCodec.createIr(parent,source.decodedBytes).program.primitives;
            nodes.forEach(function(node,i){if(node.opcode!==0x80000003)return;var selector=node.rawWords[1],query=nodes[i-1];
              (calls[selector]||(calls[selector]=[])).push({scene:parent,chaosBand:query&&query.opcode===0xbe&&query.rawWords[1]===0?query.rawWords[2]:null});});
          }
        }catch(error){if(error.name==='AbortError')throw error;}
        if(index%32===0)await new Promise(resolve=>setTimeout(resolve,0));
      }
      state.romTailCalls=calls;
    }
    for(var selector of scene.source.directorSelectorRows||[]){
      var caller=(state.romTailCalls[selector]||[])[0];
      if(caller)return {id:'rom-tail-call:'+caller.scene.assetId+':'+selector,contextScene:caller.scene,
        context:{concurrentDirectorTickOffset:1,tailCallSelector:selector,previewChaosBand:caller.chaosBand}};
    }
    return null;
  }

  async function ensureContextualRuntime(rom, state, scene, document, forcedChoice, ancestry,
      compactOutput, signal) {
    state.concurrentRuntimeByLaunchContext =
      state.concurrentRuntimeByLaunchContext || {};
    var choice = runtimeLaunchChoice(state, scene, forcedChoice);
    var contextScene = choice && choice.contextScene;
    if(!contextScene&&!(state.nativeLaunchInputsByAssetId&&state.nativeLaunchInputsByAssetId[scene.assetId])&&OB64.cutsceneRomStart&&OB64.cutsceneRomStart.analyze(state.programByAssetId[scene.assetId],{explicitlyWiped:!!scene.wiped}).code==='rom-start-inherited-stage'){
      var callerChoice=await tailCallChoice(rom,state,scene,signal);
      if(callerChoice){choice=callerChoice;contextScene=callerChoice.contextScene;}
    }
    if(!(state.nativeLaunchInputsByAssetId&&state.nativeLaunchInputsByAssetId[scene.assetId])&&OB64.cutsceneRomStart&&OB64.cutsceneRomStart.supports(scene,state.programByAssetId[scene.assetId]))contextScene=null;
    ancestry = ancestry || [];
    if (signal && signal.aborted) {
      var abortError = new Error('Cutscene preparation cancelled.');
      abortError.name = 'AbortError';
      return Promise.reject(abortError);
    }
    if (!contextScene) {
      var standaloneRuntime = compileRuntimeDocument(
        state, scene, document, choice, null, signal, rom);
      return Promise.resolve(standaloneRuntime).then(function(runtime) {
        return compactOutput ? OB64.cutsceneRuntime.compactContextRuntime(runtime) : runtime;
      });
    }
    if(!(state.nativeLaunchInputsByAssetId&&state.nativeLaunchInputsByAssetId[scene.assetId])&&OB64.cutsceneRomStart){
      // Event requests can overlap. Materialize their programs once, then run
      // them against one live scene and dialogue pool instead of replaying snapshots.
      async function predecessors(child,childChoice,seen){
        var parent=childChoice&&childChoice.contextScene;
        if(!parent)throw new UiError('No ROM event predecessor supplies this scene\'s starting state.');
        if(seen.includes(parent.assetId)||seen.length>=8)throw new UiError('The ROM event predecessor chain is cyclic or exceeds eight streams.');
        var parentDocument=await ensureProjectedDocument(rom,state,parent);
        var parentProgram=state.programByAssetId[parent.assetId];
        var parentChoice=precedingLaunchContextChoice(state,parent,childChoice.context);
        var earlier=OB64.cutsceneRomStart.supports(parent,parentProgram)?[]:await predecessors(parent,parentChoice,seen.concat(parent.assetId));
        var startTick=earlier.length?earlier.at(-1).nextStartTick:0;
        var distance=childChoice.context.concurrentDirectorTickOffset;
        earlier.push({scene:parent,document:parentDocument,program:parentProgram,
          translations:launchOperandTranslations(parent,parentChoice,state),startTick:startTick,
          previewChaosBand:childChoice.context.previewChaosBand,
          nextStartTick:startTick+(Number.isInteger(distance)?Math.max(1,distance):1)});
        return earlier;
      }
      return predecessors(scene,choice,[scene.assetId]).then(function(chain){
        return compileRuntimeDocument(state,scene,document,choice,null,signal,rom,chain);
      }).then(function(runtime){return compactOutput?OB64.cutsceneRuntime.compactContextRuntime(runtime):runtime;});
    }
    if (ancestry.indexOf(contextScene.assetId) !== -1 ||
        contextScene.assetId === scene.assetId) {
      state.sourceErrors['runtime-context:' + scene.assetId] =
        'The parent event launch chain is cyclic; this preview stops before reusing ' +
        contextScene.assetId + '.';
      var boundedRuntime = compileRuntimeDocument(
        state, scene, document, choice, null, signal, rom);
      return Promise.resolve(boundedRuntime).then(function(runtime) {
        return compactOutput ? OB64.cutsceneRuntime.compactContextRuntime(runtime) : runtime;
      });
    }
    return ensureProjectedDocument(rom, state, contextScene).then(function(contextDocument) {
      var precedingChoice = precedingLaunchContextChoice(
        state, contextScene, choice.context);
      return Promise.resolve().then(function() {
        return ensureContextualRuntime(rom, state, contextScene, contextDocument,
          precedingChoice, ancestry.concat(scene.assetId), true, signal);
      }).catch(function(error) {
          if (error && error.name === 'AbortError') throw error;
          var attributed = new Error('Preceding Director ' + contextScene.assetId +
            ' could not start: ' + (error && error.message || String(error)));
          attributed.code = error && error.code;
          throw attributed;
        });
    }).then(function(contextRuntime) {
      state.concurrentRuntimeByLaunchContext = {};
      state.concurrentRuntimeByLaunchContext[
        launchContextCacheKey(scene, choice)] = contextRuntime;
      delete state.sourceErrors['runtime-context:' + scene.assetId];
      var runtime = compileRuntimeDocument(
        state, scene, document, choice, contextRuntime, signal, rom);
      return Promise.resolve(runtime).then(function(result) {
        return compactOutput ? OB64.cutsceneRuntime.compactContextRuntime(result) : result;
      });
    });
  }

  function loadScene(rom, state, scene) {
    if (state.loadingByAssetId[scene.assetId] && state.loadingAssetId === scene.assetId &&
        state.runtimeController && !state.runtimeController.signal.aborted) {
      return state.loadingByAssetId[scene.assetId];
    }
    if (state.runtimeController) state.runtimeController.abort();
    var controller = state.runtimeController = new AbortController();
    state.loadingAssetId = scene.assetId;
    var promise = ensureProjectedDocument(rom, state, scene).then(function(document) {
      if (scene.engine !== 'director' || !state.programByAssetId[scene.assetId]) {
        return document;
      }
      return ensureContextualRuntime(rom, state, scene, document, null, [], false, controller.signal)
        .then(function(runtime) {
          if (controller.signal.aborted) return null;
          publishRuntime(state, scene, document, runtime);
          return document;
        });
    }).catch(function(error) {
      if (controller.signal.aborted || error.name === 'AbortError') return null;
      var history = historyFor(state, scene);
      delete state.runtimeByAssetId[scene.assetId];
      if (history && state.boundRuntimeDocument === history.present) {
        OB64.cutsceneRuntime.unbind(history.present);
        state.boundRuntimeDocument = null;
      }
      state.sourceErrors['runtime-context:' + scene.assetId] =
        error && error.message || String(error);
      return history ? history.present : null;
    }).finally(function() {
      if (state.loadingByAssetId[scene.assetId] === promise) {
        delete state.loadingByAssetId[scene.assetId];
      }
    });
    state.loadingByAssetId[scene.assetId] = promise;
    return promise;
  }

  function setNativeLaunchInputs(state, scene, input) {
    var validated = OB64.cutsceneRuntime.validateLaunchInputs(input, scene.assetId);
    state.launchImportRequest = (state.launchImportRequest || 0) + 1;
    if (state.runtimeController) state.runtimeController.abort();
    if (state.boundRuntimeDocument) OB64.cutsceneRuntime.unbind(state.boundRuntimeDocument);
    state.nativeLaunchInputsByAssetId = state.nativeLaunchInputsByAssetId || {};
    if (validated) state.nativeLaunchInputsByAssetId[scene.assetId] = validated;
    else delete state.nativeLaunchInputsByAssetId[scene.assetId];
    state.runtimeByAssetId = {};
    state.concurrentRuntimeByLaunchContext = {};
    delete state.sourceErrors['runtime-context:' + scene.assetId];
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
    if (snapshot && state.ui.resetInspectorScroll && snapshot.scrolls.inspector) {
      snapshot.scrolls.inspector.top = 0;
    }
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
    var preparation=refreshRuntime(state, scene, history.present, rom);
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
    return preparation;
  }

  function sceneHasChanges(state, scene) {
    var history = historyFor(state, scene);
    return !!history && OB64.cutsceneModel.serializeSceneDocument(history.present, 0) !==
      state.originalSerialized[scene.storageId];
  }

  function editCount(state) {
    var project = OB64.cutsceneProject;
    return state.catalog.scenes.reduce(function(total, scene) {
      var changed = sceneHasChanges(state, scene) || !!(scene.wiped && scene.wiped.pending) ||
        !!(scene.authored && project && project.authoredDiffersFromRom(state, scene));
      return total + (changed ? 1 : 0);
    }, (project && project.bindingsDifferFromRom(state) ? 1 : 0) + (state.deletedAuthored || []).length);
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
    var position = Math.max(0, (preview.runtime ? preview.runtime.tick : preview.frame) - row.startFrame) % duration;
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
            ? row.payload.poseFrame : Math.max(0, (preview.runtime ? preview.runtime.tick : preview.frame) - row.startFrame),
          poseLoop: row.payload.poseLoop,
          displayedFrameToken: Number.isInteger(row.payload.displayedFrameToken)
            ? row.payload.displayedFrameToken : null
        });
        return cutsceneImage ? {
          image: cutsceneImage,
          renderPassSelector:row.payload.renderPassSelector,
          x: Number.isFinite(row.payload.stageX) ? row.payload.stageX : 160,
          y: Number.isFinite(row.payload.stageY) ? row.payload.stageY : 120,
          scale: Number.isFinite(row.payload.scale) ? row.payload.scale : 1,
          rotationDegrees: Number.isFinite(row.payload.rotationDegrees)
            ? row.payload.rotationDegrees : 0,
          anchorX: cutsceneImage.anchorX,
          anchorY: cutsceneImage.anchorY,
          renderPipeline: row.payload.renderPipeline || null,
          sceneTransform: row.payload.sceneTransform || null
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
      var elapsed = Math.max(0, (preview.runtime ? preview.runtime.tick : preview.frame) - row.startFrame);
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

  function playbackTimingLabel(state,scene) {
    var input=state.nativeLaunchInputsByAssetId&&state.nativeLaunchInputsByAssetId[scene.assetId];
    if(!input&&state.romStartupByAssetId&&state.romStartupByAssetId[scene.assetId]==='inherited')return 'ROM event sequence with shared Actors, dialogue and background. Predecessor streams supply starting state. Opening dialogue gates select entry; event-update distances and automatic dialogue acknowledgement use simulated timing. Scenario and player choices use preview defaults.';
    if(!input&&state.romStartupByAssetId&&state.romStartupByAssetId[scene.assetId]==='preserved-stage')return 'ROM startup with a sample party selected from the stream\'s class requests. Members use level-one ROM class data and preview names. Inherited environment defaults to 0; scenario/event values are zero. Party returns and dialogue timing are simulated. Audio queue only.';
    if(!input&&state.romStartupByAssetId&&state.romStartupByAssetId[scene.assetId]==='preview-party-terrain')return 'Battle caller selects terrain in-game. This ROM preview uses sample environment 0 and a sample party from the stream\'s class requests. The exported stream retains its caller-selected terrain. Dialogue timing is simulated; choices confirm the first option.';
    if(!input&&state.romStartupByAssetId&&state.romStartupByAssetId[scene.assetId]==='preview-party')return 'ROM scene with a sample party selected from the stream\'s class requests. The scene supplies its environment, camera and actions. Members use level-one ROM class data and preview names; scenario/event values are zero. Party returns are approximate. Automatic dialogue advance: simulated timing; choices confirm the first option.';
    if(!input&&state.romStartupByAssetId&&state.romStartupByAssetId[scene.assetId]==='interface')return 'ROM interface stream. Chapter titles use ROM artwork and timing with a smooth reveal. The preview uses the first matching chapter variant. Interactive screens require their own interface services.';
    if(!input&&state.romStartupByAssetId&&state.romStartupByAssetId[scene.assetId]==='room')return 'Standalone ROM room startup: current camera commands, Actors and registered layers. Declared preview defaults: empty roster, neutral controls, white world tint, protagonist Magnus and army Preview Army (text only). Automatic dialogue advance: simulated timing. Playback stops at unsupported dependencies; optional event predecessors are not reconstructed.';
    if(!input&&state.romStartupByAssetId&&state.romStartupByAssetId[scene.assetId])return 'ROM startup from the loaded scene and caller rules. Preview defaults: isolated scene, empty roster and audio queue, standard appearance, zero scenario/event state, protagonist name Magnus (text only), neutral controls, text speed 150, no proximity checks, white world tint. Automatic dialogue advance: simulated timing.';
    var services=input&&((input.capturedResume&&input.capturedResume.value.resourceServices)||(input.externalProducers&&input.externalProducers.value));
    return services&&services.resourceSchedule&&services.resourceSchedule.pageAdvancePolicy==='automatic'?'Automatic dialogue advance: simulated timing.':'';
  }

  function composeDialogue(rom,state,preview,strict) {
    var rows=[],composedIds=[];
    (preview.dialogue||[]).forEach(function(row){
      var input=row.payload.nativeDialogue&&row.payload.nativeDialogue.drawingState;
      try {
        if(!input||!OB64.cutsceneDialogueDraw)fail('Framebuffer capture of active dialogue requires its supported native compositor inputs.');
        if(!state.dialogueDrawer)state.dialogueDrawer=new OB64.cutsceneDialogueDraw(rom.z64);
        rows.push(state.dialogueDrawer.compose(input.recordHex,input.payloadHex));composedIds.push(row.id);
      } catch(error){if(strict)throw error;}
    });
    return {rows:rows,ids:composedIds};
  }

  async function capturePreviewFrame(rom,state,document,request) {
    if(!rom||!state.spriteState)fail('Product framebuffer capture requires the loaded ROM and sprite decoder.');
    var dialogueComposition=composeDialogue(rom,state,request.preview,true);
    var preview=request.preview,omit=request.backgroundPolicy==='omit';
    var rows=omit?[]:selectedBackgroundLayers(state,preview),vignetteId=preview.sceneVignette&&preview.sceneVignette.sourceAssetId;
    var ids=rows.map(function(r){return r.layer.assetId;});if(vignetteId)ids.push(vignetteId);
    await Promise.all(Array.from(new Set(ids)).map(async function(id){var asset=state.catalog.getImageAsset(id);if(!asset)fail('Capture image '+id+' is unavailable.');var image=await decodeImageAsset(rom,state,asset);if(!image.renderable)fail('Capture image '+id+' is not renderable.');}));
    var backgrounds=rows.map(function(row){return {layer:row.layer,image:state.imageCache[row.layer.assetId].result};}),actorFrames=OB64.cutsceneSprites.framesForPreview(state.spriteState,preview),effects=activeEffectFrames(rom,state,preview);
    if(Object.keys(actorFrames).length!==preview.actors.length)fail('Capture lacks a current Actor sprite.');
    if(effects.length!==preview.effects.filter(function(e){return !e.payload.nativeLifetimeEmpty;}).length)fail('Capture lacks a current effect sprite.');
    var projection=preview.background&&preview.background.projection||document.background.projection;
    if(OB64.cutsceneSprites.framesForStageProps(state.spriteState,projection,preview.runtime ? preview.runtime.tick : preview.frame).length)fail('Framebuffer capture lacks native layer ownership for scene props.');
    var image=OB64.cutsceneRenderer.renderFrame(document,preview,{showMovementPaths:false,titleImages:preview.titlePresentation?OB64.cutsceneRomStart.titleImages(rom.z64,preview.titlePresentation.variant):null,backgrounds:backgrounds,actorFrames:actorFrames,effectFrames:effects,projection:preview.actorProjection,camera:preview.cameraState,overlays:preview.overlays||[],colorModulation:preview.sceneColor,sceneVignette:preview.sceneVignette,sceneVignetteImage:vignetteId?state.imageCache[vignetteId].result:null,oversizedImageView:preview.oversizedImageView});
    OB64.cutsceneDialogueDraw&&dialogueComposition.rows.forEach(function(row){OB64.cutsceneDialogueDraw.paint(image,row);});
    image.targetId='product-stage:pass:'+request.pass+':before:'+request.nodeId;return image;
  }

  function paintStage(rom, state) {
    if (!state.ui || !state.ui.canvas) return;
    var scene = selectedScene(state);
    var document = selectedDocument(state);
    var preview = previewState(state);
    if (!scene || !document || !preview) return;
    var view = viewFor(state, scene.sceneId);
    var launchInput=state.nativeLaunchInputsByAssetId&&state.nativeLaunchInputsByAssetId[scene.assetId],framebuffer=launchInput&&launchInput.externalProducers&&launchInput.externalProducers.value&&launchInput.externalProducers.value.framebuffer;
    var backgroundRows = framebuffer&&framebuffer.backgroundPolicy==='omit'?[]:selectedBackgroundLayers(state, preview);
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
        state.spriteState, backgroundProjection, preview.runtime ? preview.runtime.tick : preview.frame) : [];
    var rendered = OB64.cutsceneRenderer.renderFrame(document, preview, {
      titleImages:preview.titlePresentation?OB64.cutsceneRomStart.titleImages(rom.z64,preview.titlePresentation.variant):null,
      backgrounds: backgrounds,
      showMovementPaths: !framebuffer,
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
    var dialogueComposition=composeDialogue(rom,state,preview,false);
    dialogueComposition.rows.forEach(function(row){OB64.cutsceneDialogueDraw.paint(rendered,row);});
    state.composedDialogueIds=dialogueComposition.ids;
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
    overlay.classList.toggle('cutscene-stage-overlay-native',preview.dialogue.some(function(row){return !!row.payload.nativeDialogue;}));
    var dialogueRows=preview.dialogue.some(function(row){return !!row.payload.nativeDialogue;})?preview.dialogue:preview.dialogue.slice(-1);
    dialogueRows.forEach(function(row) {
      if((state.composedDialogueIds||[]).indexOf(row.id)!==-1)return;
      var dialogue = row.payload;
      var box = node('div', 'cutscene-preview-dialogue');
      if(dialogue.nativeDialogue && dialogue.nativeDialogue.rectangle) {
        var rect=dialogue.nativeDialogue.rectangle;
        box.classList.add('cutscene-preview-dialogue-native');
        box.style.left=rect[0]/320*100+'%';box.style.top=rect[1]/240*100+'%';
        box.style.width=Math.max(0,rect[2]-rect[0]+1)/320*100+'%';
        box.style.height=Math.max(0,rect[3]-rect[1]+1)/240*100+'%';
        box.title='Native window timing and position; text appearance is approximate.';
      }
      var portrait = dialogue.nativeDialogue?null:dialoguePortraitCanvas(rom, dialogue);
      if (portrait) {
        box.classList.add('cutscene-preview-dialogue-has-portrait');
        box.appendChild(portrait);
      }
      var copy = node('div', 'cutscene-preview-dialogue-copy');
      if (dialogue.speaker) copy.appendChild(node('strong', '', dialogue.speaker));
      copy.appendChild(node('span', '', dialogue.nativeDialogue?(dialogue.text||''):(dialogue.text || 'Dialogue preview')));
      box.appendChild(copy);
      overlay.appendChild(box);
    });
    var cues = [];
    preview.audio.forEach(function(row) {
      // Native request events describe service requests rather than timeline clips.
      cues.push(row.payload
        ? 'Sound · ' + (row.payload.cue || row.payload.assetId || 'unresolved cue')
        : 'Sound request');
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
    // Cues describe the frame; they sit under the Stage so they never cover a
    // dialogue box or the scene itself.
    if (state.ui.stageCues) state.ui.stageCues.textContent = cues.join('  |  ');
    else if (cues.length) overlay.appendChild(node('div', 'cutscene-preview-cues', cues.join('  |  ')));
  }

  function transportPlayLabel(state, scene, playing) {
    var storyboard = scene.engine === 'director' && !state.runtimeByAssetId[scene.assetId];
    return (playing ? 'Pause' : 'Play') + (storyboard ? ' storyboard' : '');
  }

  function updateTransport(state, preview) {
    if (!state.ui || !preview) return;
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    if (state.ui.scrubber) {
      state.ui.scrubber.max = Math.max(0, preview.durationFrames - 1);
      state.ui.scrubber.value = preview.frame;
    }
    if (state.ui.playButton) state.ui.playButton.textContent = transportPlayLabel(state, scene, state.ui.clock.playing);
    if (state.ui.loopButton) state.ui.loopButton.setAttribute('aria-pressed', view.loop ? 'true' : 'false');
    if (state.ui.timelinePlayhead) {
      state.ui.timelinePlayhead.style.left = preview.frame / preview.durationFrames * 100 + '%';
    }
    (state.ui.creatorCards || []).forEach(function(row) {
      var start = clipRuntimeTick(state, scene, row.clip);
      if (start == null) start = row.clip.startFrame;
      var active = preview.frame >= start &&
        preview.frame < start + Math.max(1, row.clip.durationFrames);
      row.element.classList.toggle('at-playhead', active);
    });
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

  // "Loads from" text for a scene: the scenario / mission and world-map node of
  // the event-directory rows that launch it (cutscene-provenance-data.js).
  function provenanceSummary(scene, detailed) {
    var launches = OB64.cutsceneCatalog.sceneProvenance ? OB64.cutsceneCatalog.sceneProvenance(scene) : [];
    if (!launches.length) return null;
    var seen = {};
    var parts = [];
    // Shared event programs (class substitution, interludes) launch scenes for
    // many missions; name them only when no mission launches the scene.
    var missions = launches.filter(function(launch) { return launch.missionOrScenario; });
    (missions.length ? missions : launches).forEach(function(launch) {
      var place = launch.missionOrScenario || null;
      var node_ = launch.worldMapNode && launch.worldMapNode !== place ? launch.worldMapNode : null;
      var label = [place, node_].filter(Boolean).join(' · ') ||
        (Number.isInteger(launch.row) ? 'shared event program (event row ' + launch.row + ')' : null);
      if (!label) return;
      if (detailed && Number.isInteger(launch.row) && label.indexOf('event row') === -1) {
        label += ' (event row ' + launch.row + (launch.confidence ? ', ' + launch.confidence.toLowerCase() + ' confidence' : '') + ')';
      }
      if (!seen[label]) { seen[label] = true; parts.push(label); }
    });
    return parts.length ? parts.join('; ') : null;
  }

  function renderSceneBrowser(shell, rom, state) {
    var browser = node('aside', 'cutscene-browser');
    browser.setAttribute('data-cutscene-scroll', 'browser');
    browser.appendChild(node('h3', '', 'Scenes'));
    if (OB64.cutsceneAuthored) {
      var newScene = button('+ New scene', 'btn-secondary cutscene-new-scene', function() {
        newScene.disabled = true;
        createAuthoredScene(rom, state).catch(function(error) {
          newScene.disabled = false;
          if (state.callbacks.onStatus) state.callbacks.onStatus(error && error.message || String(error));
        });
      });
      newScene.setAttribute('data-cutscene-focus-key', 'scene-new');
      newScene.title = 'Create a new, empty scene with its own Director selector row';
      if (state.authoredDiscovery && state.authoredDiscovery.status === 'failed') {
        newScene.disabled = true;
        newScene.title = 'New scenes are unavailable: ' + state.authoredDiscovery.message;
      }
      browser.appendChild(newScene);
    }
    var search = node('input', 'cutscene-search');
    search.type = 'search'; search.placeholder = 'Search name, alias, or key';
    search.value = state.search;
    search.setAttribute('data-cutscene-focus-key', 'scene-search');
    search.addEventListener('input', function() {
      state.search = search.value;
      renderSceneRows(list, rom, state);
    });
    browser.appendChild(search);
    // Event-row filter: the mission / scenario whose event-directory row launches
    // the scene (cutscene-provenance-data.js), or shared programs only.
    var rowsTable = OB64.cutsceneProvenance && OB64.cutsceneProvenance.rows || {};
    var missionRows = Object.keys(rowsTable).map(Number).filter(function(row) {
      return rowsTable[row] && rowsTable[row].missionOrScenario;
    }).sort(function(a, b) { return a - b; });
    if (missionRows.length) {
      var filter = node('select', 'cutscene-browser-filter');
      filter.setAttribute('aria-label', 'Filter scenes by the event row that loads them');
      filter.setAttribute('data-cutscene-focus-key', 'scene-filter-row');
      var all = node('option', '', 'All scenes'); all.value = ''; filter.appendChild(all);
      var shared = node('option', '', 'Shared event programs only (no mission)'); shared.value = 'shared'; filter.appendChild(shared);
      missionRows.forEach(function(row) {
        var entry = rowsTable[row];
        var option = node('option', '', 'Row ' + row + ' · ' + entry.missionOrScenario +
          (entry.worldMapNode && entry.worldMapNode !== entry.missionOrScenario ? ' (' + entry.worldMapNode + ')' : ''));
        option.value = String(row);
        filter.appendChild(option);
      });
      filter.value = state.sceneFilterRow == null ? '' : String(state.sceneFilterRow);
      filter.addEventListener('change', function() {
        state.sceneFilterRow = filter.value === '' ? '' : (filter.value === 'shared' ? 'shared' : Number(filter.value));
        renderSceneRows(list, rom, state);
      });
      browser.appendChild(filter);
    }
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
    var scenes = state.catalog.searchScenes(state.search, { provenanceRow: state.sceneFilterRow });
    state.ui.sceneCount.textContent = scenes.length + ' of ' + state.catalog.scenes.length +
      ' selectable scenes';
    scenes.forEach(function(scene) {
      var row = button('', 'cutscene-scene-row', function() {
        if (state.selectedSceneId === scene.sceneId) {
          viewFor(state, scene.sceneId).creatorDrawer = null;
          rerender(rom, state);
          return;
        }
        if (state.runtimeController) state.runtimeController.abort();
        state.openRequest++;
        pauseAnimation(state);
        state.selectedSceneId = scene.sceneId;
        viewFor(state, scene.sceneId).creatorDrawer = null;
        rerender(rom, state);
      });
      row.classList.toggle('active', scene.sceneId === state.selectedSceneId);
      row.setAttribute('data-cutscene-focus-key', 'scene:' + scene.sceneId);
      row.appendChild(node('strong', '', OB64.cutsceneCatalog.displayName(scene)));
      row.appendChild(node('span', '', scene.authored
        ? 'New scene · selector row ' + scene.authored.selectorRow + ' · ' + launchCountLabel(state, scene)
        : scene.engine === 'director'
        ? (scene.friendlyName ? scene.technicalName : 'Key ' + scene.directorKey)
        : scene.engine + ' · native adapter unresolved'));
      var provenance = provenanceSummary(scene);
      if (provenance) row.appendChild(node('span', 'cutscene-row-provenance', 'Loads from ' + provenance));
      var badges = node('span', 'cutscene-row-badges');
      badges.appendChild(capabilityBadge(scene.previewCapability, 'Visual'));
      if (scene.authored) badges.appendChild(node('span', 'cutscene-new-badge', 'New'));
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

  function renderStageArea(shell, rom, state, scene, document, creator) {
    var center = node('main', 'cutscene-main');
    var heading = node('div', 'cutscene-heading');
    var copy = node('div');
    copy.appendChild(node('h2', '', document.identity.friendlyName||OB64.cutsceneCatalog.displayName(scene)));
    var captureStages = Array.isArray(document.identity.captures)
      ? document.identity.captures.filter(function(capture) {
        return capture.stageLabel;
      }) : [];
    var captureStageCount = captureStages.length;
    copy.appendChild(node('p', '',
      'Set Actor placement and appearance, then edit movement and dialogue along the timeline.'));
    heading.appendChild(copy);
    var badges = node('div', 'cutscene-heading-badges');
    badges.appendChild(capabilityBadge(scene.previewCapability, 'Visual'));
    badges.appendChild(capabilityBadge(document.exportRequirements.capability, 'ROM'));
    heading.appendChild(badges);
    if (!creator) center.appendChild(heading);
    var overview = node('div', 'cutscene-edit-overview');
    var overviewView = viewFor(state, scene.sceneId);
    var sceneClips = orderedClips(document);
    [
      ['Actors', document.actors.length, 'actors'],
      ['Movement & poses', sceneClips.filter(function(row) {
        return row.clip.kind === 'movement' || row.clip.kind === 'pose';
      }).length, 'actions'],
      ['Dialogue boxes', sceneClips.filter(function(row) {
        return row.clip.kind === 'dialogue';
      }).length, 'dialogue']
    ].forEach(function(entry) {
      var card = button('', 'cutscene-overview-card', function() {
        overviewView.editPanel = entry[2];
        if (state.ui) state.ui.resetInspectorScroll = true;
        rerender(rom, state);
      });
      card.appendChild(node('strong', '', String(entry[1])));
      card.appendChild(node('span', '', entry[0]));
      card.setAttribute('aria-label', 'Edit ' + entry[0]);
      overview.appendChild(card);
    });
    if (!creator) center.appendChild(overview);

    if (state.sourceErrors[scene.assetId]) {
      center.appendChild(node('div', 'cutscene-source-warning',
        'ROM source unavailable: ' + state.sourceErrors[scene.assetId]));
    }
    var startupError = !state.runtimeByAssetId[scene.assetId] &&
      (state.sourceErrors['runtime-context:' + scene.assetId] ||
       state.sourceErrors['runtime:' + scene.assetId]);
    if (startupError) {
      var startupWarning = node('div', 'cutscene-source-warning',
        'Director playback unavailable: ' + startupError +
        ' Approximate storyboard only; Play storyboard does not execute the Director stream.');
      startupWarning.setAttribute('role', 'status');
      center.appendChild(startupWarning);
    }
    if (!creator && document.exportRequirements.reasons.length) {
      center.appendChild(node('div', document.exportRequirements.capability === 'native'
        ? 'cutscene-export-note' : 'cutscene-source-warning',
      document.exportRequirements.reasons.join(' ')));
    }
    var stagePanel = node('section', 'cutscene-stage-panel');
    var stageHeading = node('div', 'cutscene-stage-heading');
    stageHeading.appendChild(node('strong', '', 'Stage'));
    stageHeading.appendChild(node('span', '',
      creator ? 'Select an Actor or drag it to place a pose.' :
        'Select an Actor. Dragging it adds or edits a timed pose at the playhead.'));
    if (creator && viewFor(state, scene.sceneId).creatorDestinationClipId) {
      stageHeading.appendChild(node('span', 'cutscene-destination-hint',
        'Click a destination. Press Escape to cancel.'));
    }
    stagePanel.appendChild(stageHeading);
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
    var stageCues = node('div', 'cutscene-stage-cues');
    stageFooter.appendChild(stageCues);
    state.ui.stageCues = stageCues;
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
    if (nativeRuntime && stageRuntime.unresolvedQuery) {
      calibrationText += ' Playback boundary: ' + stageRuntime.unresolvedQuery.label +
        '. Earlier computed frames remain available.';
    } else if (nativeRuntime && stageRuntime.missingInputs.length) {
      calibrationText += ' ' + stageRuntime.missingInputs.length +
        ' launch or service inputs remain unresolved.';
    }
    var timingLabel=playbackTimingLabel(state,scene);if(timingLabel)calibrationText+=' '+timingLabel;
    if (nativeRuntime && stageRuntime.assumptions.some(function(note) { return note.indexOf('Path-ribbon shapes') === 0; })) {
      calibrationText += ' Path ribbons: approximate shapes, textures and reveal timing.';
    }
    if (nativeRuntime && stageRuntime.assumptions.some(function(note) { return note.indexOf('Map panels advance') === 0; })) {
      calibrationText += ' Menus advance automatically; the preview chooses the last option.';
    }
    var calibration = node('span', 'cutscene-stage-calibration', calibrationText);
    calibration.title = nativeRuntime
      ? [stageRuntime.directorModeStatus,
        stageRuntime.states[0].actorProjection.source].filter(Boolean).join(' ')
      : (calibrated ? [projection.calibrationResult,
        actorProjection.calibrationResult].filter(Boolean).join(' ') : '');
    var previewDetails = node('details', 'cutscene-stage-details');
    previewDetails.appendChild(node('summary', '', 'Preview details'));
    previewDetails.appendChild(calibration);
    previewDetails.appendChild(node('p', '', scene.technicalName +
      (scene.engine === 'director'
        ? (captureStageCount ? ' · ' + captureStageCount + ' captured presentation stages' : '') +
          ' · Preview clock: 30 frames/second · Native waits remain integer ticks'
        : ' · Visual Storyboard workspace · Native presentation adapter unresolved')));
    stageFooter.appendChild(previewDetails);
    stagePanel.appendChild(stageFooter);
    center.appendChild(stagePanel);

    var view = viewFor(state, scene.sceneId);
    var duration = OB64.cutscenePreview.sceneDurationFrames(document, view.pathId);
    var controls = node('div', 'cutscene-transport');
    var back = transportButton('−1', function() {
      state.ui.clock = OB64.cutscenePreview.step(state.ui.clock, -1);
      view.frame = state.ui.clock.frame; paintStage(rom, state);
    }, 'Previous frame');
    var play = transportButton(transportPlayLabel(state, scene, false), function() {
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

    if (!creator && captureStages.length) {
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
    if (creator) {
      state.ui.creatorViewControls = viewControls;
      renderCreatorCast(center, rom, state, scene, document);
    } else {
      center.appendChild(viewControls);
      renderTimeline(center, rom, state, scene, document);
    }
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
        clipSequence(left.clip) - clipSequence(right.clip) ||
        left.trackIndex - right.trackIndex || left.clipIndex - right.clipIndex;
    });
  }

  // Explicit order among actions that share a start; assigned by drag
  // reordering. Unsequenced clips keep their track and clip order.
  function clipSequence(clip) {
    var value = clip.source && clip.source.sequence;
    return Number.isInteger(value) ? value : Number.MAX_SAFE_INTEGER;
  }

  function clipIsReorderable(clip) {
    return !(clip.source && clip.source.nodeId);
  }

  // Moves an authored action card next to another card in the Actions list.
  // Native commands keep their stream positions and cannot be dragged; an
  // authored action takes the start of the card it lands before and the
  // insertion point that keeps it there, so the compiled stream follows the
  // list. Returns the reason when the move is not possible.
  function reorderClip(rom, state, scene, clipId, targetClipId, after) {
    var view = viewFor(state, scene.sceneId);
    var document = selectedDocument(state);
    var rows = creatorActionRows(document, view);
    var moving = rows.find(function(row) { return row.clip.id === clipId; });
    var target = rows.find(function(row) { return row.clip.id === targetClipId; });
    if (!moving || !target || moving === target) return 'unknown-card';
    if (!clipIsReorderable(moving.clip)) return 'native-command';
    var remaining = rows.filter(function(row) { return row !== moving; });
    var at = remaining.indexOf(target) + (after ? 1 : 0);
    var ordered = remaining.slice(0, at).concat([moving], remaining.slice(at));
    var next = ordered[at + 1] || null;
    var previous = at > 0 ? ordered[at - 1] : null;
    var startFrame;
    var boundaryId = null;
    var boundaries = scene.engine === 'director' ? insertionBoundaries(scene, document, state) : [];
    if (next) {
      startFrame = next.clip.startFrame;
      if (clipIsReorderable(next.clip)) boundaryId = next.clip.source.insertBeforeNodeId || null;
      else {
        var nextNode = boundaries.find(function(boundary) { return boundary.id === next.clip.source.nodeId; });
        if (!nextNode) {
          nextNode = boundaries.filter(function(boundary) {
            return boundary.frame >= next.clip.startFrame;
          })[0] || null;
        }
        boundaryId = nextNode ? nextNode.id : null;
      }
    } else if (previous) {
      startFrame = previous.clip.startFrame;
      if (clipIsReorderable(previous.clip)) boundaryId = previous.clip.source.insertBeforeNodeId || null;
      else {
        var previousNode = scene.source.nodes.find(function(node) { return node.id === previous.clip.source.nodeId; });
        var later = boundaries.filter(function(boundary) {
          return previousNode && boundary.sourceNode.startWord > previousNode.startWord;
        })[0] || null;
        boundaryId = later ? later.id : null;
      }
    } else {
      return 'no-neighbour';
    }
    var needsBoundary = moving.clip.capability === 'native' && !!moving.clip.source.insertBeforeNodeId;
    if (needsBoundary && !boundaryId) return 'no-insertion-point';
    view.selectedClipId = clipId;
    executeEdit(rom, state, 'Reorder action', function(nextDocument) {
      var order = {};
      ordered.forEach(function(row, index) { order[row.clip.id] = index; });
      nextDocument.tracks.forEach(function(track) {
        track.clips.forEach(function(clip) {
          if (!clipIsReorderable(clip) || !(clip.id in order)) return;
          clip.source.sequence = order[clip.id];
          if (clip.id !== clipId) return;
          clip.startFrame = startFrame;
          if (needsBoundary) clip.source.insertBeforeNodeId = boundaryId;
        });
      });
    });
    return null;
  }

  function selectClip(rom, state, scene, row) {
    var view = viewFor(state, scene.sceneId);
    view.timelineMode = 'preview';
    view.selectedClipId = row.clip.id;
    view.creatorDestinationClipId = null;
    // Show the moment this action happens: the exact runtime tick when the
    // stream has been executed, otherwise the projected start.
    var tick = clipRuntimeTick(state, scene, row.clip);
    view.frame = tick == null ? row.clip.startFrame : tick;
    pauseAnimation(state);
    if (state.ui && state.ui.clock) {
      state.ui.clock = OB64.cutscenePreview.pause(OB64.cutscenePreview.seek(state.ui.clock, view.frame));
    }
    if (row.track.actorId) view.selectedActorId = row.track.actorId;
    view.editPanel = row.clip.kind === 'dialogue' ? 'dialogue' : 'actions';
    if (state.ui && !state.ui.creator) state.ui.resetInspectorScroll = true;
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
      view.editPanel = row.clip.kind === 'dialogue' ? 'dialogue' : 'actions';
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
              if (next.clip.payload.nativeDialogueAuthored === true) {
                retimeAuthoredDialogue(document, next.clip, next.clip.startFrame, scene, state);
              } else if (!next.clip.source.nodeId && next.clip.source.insertBeforeNodeId) {
                var boundary = boundaryForFrame(scene, document, next.clip.startFrame, state);
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
      ['Stream', selected.streamAssetId || scene.assetId],
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
            var targetRow = findClipRow(document, row.editorTarget.clipId);
            view.editPanel = row.editorTarget.kind === 'actor' ? 'actors'
              : (targetRow && targetRow.clip.kind === 'dialogue' ? 'dialogue' : 'actions');
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
    var entries = runtime.trace.filter(function(entry) { return entry.kind === 'composite'; }).slice(0, 1000);
    var rows = entries.map(function(entry, index) {
      var owner = runtime.programsByAssetId && runtime.programsByAssetId[entry.streamAssetId] || program;
      var composite = owner.compositeById[entry.compositeId];
      if (!composite) return null;
      var duration = 1;
      var startState = runtime.states[entry.tick];
      if (startState && startState.runtime.activeStreamAssetId === entry.streamAssetId &&
          startState.runtime.blockLabel === composite.label) {
        var endTick = entry.tick + 1;
        while (endTick < runtime.states.length &&
            runtime.states[endTick].runtime.activeStreamAssetId === entry.streamAssetId &&
            runtime.states[endTick].runtime.blockLabel === composite.label) endTick++;
        duration = Math.max(1, endTick - entry.tick);
      }
      var editorTarget = owner === program ? directorEditorTarget(document, owner, composite) : null;
      return Object.assign({}, composite, {
        id: entry.streamAssetId + ':' + composite.id + ':' + index,
        ownerProgram: owner,
        streamAssetId: entry.streamAssetId,
        runtimeStartTick: entry.tick,
        runtimeDurationTicks: duration,
        editorTarget: editorTarget,
        editable: !!editorTarget
      });
    }).filter(Boolean);
    var selected = directorSourceSelection(program, rows, view);
    var summary = node('div', 'cutscene-director-source-summary');
    summary.appendChild(node('strong', '', runtime.durationTicks + ' Director scheduler updates'));
    summary.appendChild(node('span', '', entries.length + ' executed composite actions · ' +
      runtime.executedPrimitiveCount + ' dispatched visual/state commands'));
    summary.appendChild(node('small', '',
      'Horizontal position is execution time. Waits and completion gates occupy their scheduler duration.'));
    summary.appendChild(node('small', '', 'Runtime inputs · ' + runtime.assumptions.length +
      ' assumptions · ' + runtime.missingInputs.length + ' missing launch/resource inputs'));
    summary.appendChild(node('small', '', 'Preview outcome: ' + runtime.outcome +
      '. Modeled endings do not prove native completion.'));
    summary.appendChild(node('small', '', 'Showing ' + rows.length +
      ' actions (maximum 1,000) from ' + runtime.trace.length + ' retained trace entries. ' +
      (runtime.traceTruncated ? 'The trace is truncated. ' : '') +
      (runtime.unsupportedCommands || []).length + ' command kinds have unsupported behavior.'));
    if (runtime.unresolvedQuery) summary.appendChild(node('small', '',
      'Missing input: ' + runtime.unresolvedQuery.label + '.'));
    if (runtime.pendingWait) summary.appendChild(node('small', '',
      'Pending native wait: ' + runtime.pendingWait.label + ' (' + runtime.pendingWait.kind + ').'));
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
            var targetRow = findClipRow(document, row.editorTarget.clipId);
            view.editPanel = row.editorTarget.kind === 'actor' ? 'actors'
              : (targetRow && targetRow.clip.kind === 'dialogue' ? 'dialogue' : 'actions');
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
    renderDirectorSourceDetails(section, selected && selected.ownerProgram || program, selected, scene, state.catalog);
  }

  function renderTimeline(center, rom, state, scene, document) {
    var section = node('section', 'cutscene-sequence');
    var tabs = node('div', 'cutscene-sequence-heading');
    tabs.appendChild(node('h3', '', 'Scene timeline'));
    var diagnosticToggle = button(state.diagnosticAssumptions
      ? 'Diagnostic assumptions: on' : 'Diagnostic assumptions: off', 'btn-secondary', function() {
      state.diagnosticAssumptions = !state.diagnosticAssumptions;
      if (state.boundRuntimeDocument) OB64.cutsceneRuntime.unbind(state.boundRuntimeDocument);
      state.runtimeByAssetId = {};
      state.concurrentRuntimeByLaunchContext = {};
      delete state.sourceErrors['runtime-context:' + scene.assetId];
      if (state.runtimeController) state.runtimeController.abort();
      rerender(rom, state);
    });
    diagnosticToggle.setAttribute('aria-pressed', state.diagnosticAssumptions ? 'true' : 'false');
    diagnosticToggle.setAttribute('data-cutscene-focus-key', 'diagnostic-assumptions');
    diagnosticToggle.title = 'Allow labeled completion assumptions for unsupported external queries. This does not reproduce native input.';
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
    var advanced = node('details', 'cutscene-timeline-advanced');
    advanced.open = view.timelineMode !== 'preview';
    advanced.appendChild(node('summary', '', 'Advanced playback views'));
    var advancedActions = node('div', 'cutscene-story-actions');
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
      advancedActions.appendChild(modeButton);
    });
    advancedActions.appendChild(diagnosticToggle);
    advanced.appendChild(advancedActions);
    tabs.appendChild(advanced);
    if (program && view.timelineMode === 'runtime') {
      section.appendChild(tabs);
      renderDirectorRuntime(section, rom, state, scene, document, program, runtime);
      center.appendChild(section);
      return;
    }
    if (program && view.timelineMode !== 'preview') {
      section.appendChild(tabs);
      renderDirectorSource(section, rom, state, scene, document, program, view.timelineMode);
      center.appendChild(section);
      return;
    }
    actions.appendChild(node('span', 'cutscene-story-actions-label', 'Add at playhead'));
    [['Appear', addEnter], ['Disappear', addExit], ['Pose', addPose],
      ['Move', addMove], ['Hold', addHold], ['Dialogue box', addDialogue]].forEach(function(entry) {
      actions.appendChild(button(entry[0], 'btn-secondary', function() { entry[1](rom, state); }));
    });
    var moreActions = node('details', 'cutscene-more-actions');
    moreActions.appendChild(node('summary', '', 'More actions'));
    var moreActionButtons = node('div', 'cutscene-story-actions');
    [['Opacity', addOpacity], ['Sound (storyboard only)', addAudio],
      ['Effect', addEffect], ['Battle action', addCombatAction],
      ['Projection', addCamera], ['Branch', addBranch], ['End', addEnd]].forEach(function(entry) {
      moreActionButtons.appendChild(button(entry[0], 'btn-secondary', function() { entry[1](rom, state); }));
    });
    moreActions.appendChild(moreActionButtons);
    actions.appendChild(moreActions);
    actions.appendChild(button('Undo', 'btn-secondary', function() {
      var history = historyFor(state, scene);
      if (OB64.cutsceneModel.undo(history)) {
        refreshRuntime(state, scene, history.present, rom);
        notifyChange(state, 'Undo Cutscene edit'); rerender(rom, state);
      }
    }));
    actions.appendChild(button('Redo', 'btn-secondary', function() {
      var history = historyFor(state, scene);
      if (OB64.cutsceneModel.redo(history)) {
        refreshRuntime(state, scene, history.present, rom);
        notifyChange(state, 'Redo Cutscene edit'); rerender(rom, state);
      }
    }));
    section.appendChild(tabs);
    section.appendChild(actions);

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

  // Boundary frames use the runtime tick of each source command when the scene
  // has been executed, so the playhead and the insertion point agree.
  function insertionBoundaries(scene, document, state) {
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
      var tick = row && state ? clipRuntimeTick(state, scene, row.clip) : null;
      var frame = row ? (tick == null ? row.clip.startFrame : tick) : cursorFrame;
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

  function boundaryForFrame(scene, document, frame, state) {
    var boundaries = insertionBoundaries(scene, document, state);
    if (!boundaries.length) return null;
    return boundaries.sort(function(left, right) {
      var leftAfter = left.frame >= frame ? 0 : 1;
      var rightAfter = right.frame >= frame ? 0 : 1;
      return leftAfter - rightAfter || Math.abs(left.frame - frame) - Math.abs(right.frame - frame) ||
        left.sourceNode.startWord - right.sourceNode.startWord;
    })[0].sourceNode;
  }

  function dialogueInsertionBoundaries(document, scene, state) {
    var cursorFrame = 0;
    var clipsByNode = {};
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        if (clip.source && clip.source.nodeId) clipsByNode[clip.source.nodeId] = clip;
      });
    });
    return document.native.commands.map(function(command) {
      var row = clipsByNode[command.source.nodeId];
      var tick = row && scene && state ? clipRuntimeTick(state, scene, row) : null;
      var at = row ? (tick == null ? row.startFrame : tick) : cursorFrame;
      if (row && row.kind === 'wait') cursorFrame = at + row.durationFrames;
      else cursorFrame = Math.max(cursorFrame, at);
      return { id: command.source.nodeId, frame: at,
        sourceNode: { startWord: command.source.startWord },
        approved: [0x01, 0xBF, 0x80000001].indexOf(command.words[0]) !== -1 &&
          command.source.editPolicy !== 'immutable-gap' && !command.source.insertionWithheld };
    }).filter(function(boundary) { return boundary.approved; });
  }

  function dialogueBoundaryForFrame(document, frame, scene, state) {
    var boundaries = dialogueInsertionBoundaries(document, scene, state);
    if (!boundaries.length) return null;
    var preceding = boundaries.filter(function(boundary) { return boundary.frame <= frame; });
    return (preceding.length ? preceding : boundaries).sort(function(left, right) {
      return preceding.length
        ? right.frame - left.frame || right.sourceNode.startWord - left.sourceNode.startWord
        : left.frame - right.frame || left.sourceNode.startWord - right.sourceNode.startWord;
    })[0];
  }

  function retimeAuthoredDialogue(document, clip, frame, scene, state) {
    var boundary = dialogueBoundaryForFrame(document, frame, scene, state);
    if (!boundary) return;
    clip.startFrame = Math.max(frame, boundary.frame);
    clip.source.insertBeforeNodeId = boundary.id;
    clip.payload.nativeDelayTicks = clip.startFrame - boundary.frame;
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

  // Card kinds that start an asynchronous native job and hand over at once.
  var ASYNC_CARD_KINDS = ['movement', 'camera', 'fade'];

  function allClipRows(document) {
    var rows = [];
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) { rows.push({ track: track, clip: clip }); });
    });
    return rows;
  }

  function secondsText(ticks) {
    return (Math.round(ticks / 3) / 10).toFixed(1) + ' s';
  }

  // Where each card starts in the native run, and what happens after it.
  function cardTimingText(state, scene, clip) {
    var start = clipRuntimeTick(state, scene, clip);
    var parts = [];
    if (start != null) parts.push('Starts at ' + secondsText(start));
    if (clip.kind === 'wait') parts.push('holds ' + secondsText(clip.durationFrames));
    else if (ASYNC_CARD_KINDS.indexOf(clip.kind) >= 0) {
      var waits = clip.payload && clip.payload.waitForCompletion === true;
      parts.push('lasts ' + secondsText(clip.durationFrames) + (waits
        ? (clip.kind === 'movement' ? ', then the next action once the Actor arrives' : ', then the next action')
        : ' alongside the next action'));
    }
    return parts.join(' · ');
  }

  // New scenes end at their native exit, which stops anything still running.
  // Asynchronous cards whose runtime start plus duration passes the exit tick.
  function runningAtExit(state, scene, document) {
    if (!scene || !(scene.authored || scene.wiped)) return null;
    var runtime = state.runtimeByAssetId && state.runtimeByAssetId[scene.assetId];
    if (!runtime || !Array.isArray(runtime.states) || !runtime.states.length) return null;
    var exitTick = runtime.states.length - 1;
    var late = [];
    allClipRows(document).forEach(function(row) {
      // Native cards count too: after a ROM-only reopen every card is one.
      if (ASYNC_CARD_KINDS.indexOf(row.clip.kind) < 0) return;
      var start = clipRuntimeTick(state, scene, row.clip);
      if (start == null) return;
      var end = start + row.clip.durationFrames + OB64.cutsceneCodec.completionExtraUpdates(row.clip.kind);
      if (end > exitTick) late.push({ row: row, end: end });
    });
    // A native fade projected back from the ROM is not a Fade card, so also
    // read the final native state: a fade overlay with updates left is cut short.
    var finalState = runtime.states[exitTick];
    if (!late.some(function(item) { return item.row.clip.kind === 'fade'; })) {
      (finalState && finalState.overlays || []).forEach(function(overlay) {
        if (!overlay.native || !(overlay.remaining > 0)) return;
        // A fade with its whole duration left has not had its first native
        // pass yet, which applies no change: it needs one update more.
        var pendingFirstPass = overlay.remaining >= overlay.duration ? 1 : 0;
        late.push({ row: { track: { actorId: null }, clip: { id: 'runtime:fade', kind: 'fade', payload: {} } },
          end: exitTick + overlay.remaining + pendingFirstPass });
      });
    }
    if (!late.length) return null;
    return { exitTick: exitTick, late: late, needed: Math.max.apply(null, late.map(function(item) { return item.end; })) - exitTick };
  }

  // Add one explicit Wait card before the native exit, long enough for every
  // running action to finish.
  function holdUntilFinished(rom, state) {
    var scene = selectedScene(state);
    var document = selectedDocument(state);
    var running = runningAtExit(state, scene, document);
    if (!running) return null;
    var exit = (scene.source.nodes || []).find(function(node) {
      return node.insertBefore && (node.opcodeU32 === 0x7D || node.opcodeU32 === 0x39);
    });
    if (!exit) throw new UiError('This scene has no native exit to hold before.');
    var last = 0;
    allClipRows(document).forEach(function(row) { last = Math.max(last, row.clip.startFrame + 1); });
    return addAuthoredClip(rom, state, {
      label: 'Hold until actions finish', type: 'flow', kind: 'wait', durationFrames: running.needed,
      boundary: exit, startFrame: last, payload: { timingStatus: 'authored-preview-frame' }
    });
  }

  function addAuthoredClip(rom, state, options) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var document = selectedDocument(state);
    var boundary = options.boundary || (options.nativePreferred === true
      ? boundaryForFrame(scene, document, view.frame, state) : null);
    var id = authoredClipId(document, options.kind);
    view.timelineMode = 'preview';
    view.selectedClipId = id;
    if (options.actorId) view.selectedActorId = options.actorId;
    view.editPanel = options.kind === 'dialogue' ? 'dialogue' : 'actions';
    var payload = options.payload;
    if ((scene.authored || scene.wiped) && ASYNC_CARD_KINDS.indexOf(options.kind) >= 0 && payload &&
        payload.waitForCompletion === undefined) {
      payload = Object.assign({}, payload, { waitForCompletion: true });
    }
    return executeEdit(rom, state, options.label, function(next) {
      var actor = options.actorId && next.actors.find(function(candidate) {
        return candidate.id === options.actorId;
      });
      var trackLabel = options.trackLabel || (actor ? actor.label + ' ' + options.type : 'Scene ' + options.type);
      var track = trackOrCreate(next, options.type, options.actorId || null, trackLabel);
      var created = OB64.cutsceneModel.createClip({
        id: id,
        kind: options.kind,
        startFrame: options.startFrame == null ? view.frame : options.startFrame,
        durationFrames: options.durationFrames,
        pathIds: options.pathIds || [],
        capability: options.capability || (boundary ? 'native' : 'preview-only'),
        payload: payload,
        source: boundary ? { insertBeforeNodeId: boundary.id } : {}
      });
      // A card added where others share its start goes after them, in the
      // list and in the compiled stream alike: give the tied added cards
      // explicit sequence numbers in their current list order, then this one.
      var tied = orderedClips(next).filter(function(row) {
        return clipIsReorderable(row.clip) && row.clip.startFrame === created.startFrame;
      });
      tied.forEach(function(row, index) { row.clip.source.sequence = index; });
      created.source.sequence = tied.length;
      track.clips.push(created);
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

  function createFromTemplate(rom,state){
    var scene=selectedScene(state);
    var preparation=executeEdit(rom,state,'Create replacement from template',function(document){
      document.identity.friendlyName='New scene - '+OB64.cutsceneCatalog.displayName(scene);
    });
    viewFor(state,scene.sceneId).timelineMode='preview';
    if(state.callbacks.onStatus)state.callbacks.onStatus('New scene uses this template and replaces '+OB64.cutsceneCatalog.displayName(scene)+' at every existing game trigger. Edit its actions, save the Project, then export.');
    rerender(rom,state);
    return preparation;
  }

  function addHold(rom, state) {
    return addAuthoredClip(rom, state, {
      label: 'Add hold', type: 'flow', kind: 'wait', durationFrames: 30,
      nativePreferred: true, payload: { timingStatus: 'authored-preview-frame' }
    });
  }

  function addMove(rom, state) {
    var selected = selectedActorState(state);
    if (!selected) return;
    var current = selected.preview;
    return addAuthoredClip(rom, state, {
      label: 'Add movement', type: 'movement', kind: 'movement', actorId: selected.actor.id,
      durationFrames: 30, nativePreferred: true,
      payload: {
        from: nativePosition(current),
        to: nativePosition({ x: current.x + 50, y: current.y, z: current.z }),
        nativeSpeed: 1.667, durationMode: 'duration',
        timingStatus: 'authored-preview-frame'
      }
    });
  }

  // Native coordinates are fixed point at x1000, and -1.000 is the native
  // "keep the current coordinate" value. Preview positions (interpolated
  // floats) are rounded to that precision and kept off the reserved value.
  function nativeCoordinate(value) {
    if (!Number.isFinite(value)) return value;
    var rounded = Math.round(value * 1000) / 1000;
    return rounded === -1 ? -1.001 : rounded;
  }

  function nativePosition(position) {
    return { x: nativeCoordinate(position.x), y: nativeCoordinate(position.y), z: nativeCoordinate(position.z) };
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
    // A plain Pose changes appearance only and keeps the Actor where it is when
    // it runs (native -1000). Place on Stage sets an explicit native position.
    var position = positionOverride ? nativePosition(positionOverride) : { x: null, y: null, z: null };
    var appearance = Number.isInteger(current.variantSelector) && current.variantSelector >= 0
      ? current.variantSelector
      : (Number.isInteger(actor.source.variantSelector) && actor.source.variantSelector >= 0
        ? actor.source.variantSelector : 0);
    return addAuthoredClip(rom, state, {
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
    return addPoseProgram(rom, state, null, positionOverride);
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
        x: nativeCoordinate(current.x), y: nativeCoordinate(current.y), z: nativeCoordinate(current.z),
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

  function addDialogue(rom, state, extra) {
    var scene = selectedScene(state);
    var view = viewFor(state, scene.sceneId);
    var document = selectedDocument(state);
    var actor = document.actors.find(function(candidate) {
      return candidate.id === view.selectedActorId;
    });
    // A new scene takes every action, dialogue included, in front of its
    // native exit; its only other candidate is the terminal hold after release.
    var boundary = scene.authored || scene.wiped
      ? insertionBoundaries(scene, document, state).filter(function(candidate) {
        return candidate.sourceNode.opcodeU32 === 0x7D || candidate.sourceNode.opcodeU32 === 0x39;
      })[0] || null
      : dialogueBoundaryForFrame(document, view.frame, scene, state);
    var templates = document.native.commands.filter(function(command) {
      return command.words[0] === 0xBF && command.words.length === 14;
    }).sort(function(left, right) {
      var leftMatch = actor && left.words[4] === actor.slot ? 0 : 1;
      var rightMatch = actor && right.words[4] === actor.slot ? 0 : 1;
      return leftMatch - rightMatch || left.source.startWord - right.source.startWord;
    });
    if (boundary && templates.length && OB64.cutsceneAuthoring) {
      var template = null;
      var archive = null;
      templates.some(function(candidate) {
        try {
          archive = OB64.cutsceneAuthoring.readDialogue(rom.z64, candidate.words[2]);
          template = candidate;
          return true;
        } catch (_error) {
          return false;
        }
      });
      if (template) {
        var selector = template.words[2];
        var usedWindows = {};
        document.native.commands.forEach(function(command) {
          if (command.words[0] === 0xBF) usedWindows[command.words[1]] = true;
        });
        document.tracks.forEach(function(track) {
          track.clips.forEach(function(clip) {
            if (clip.payload.nativeDialogueAuthored) usedWindows[clip.payload.windowId] = true;
          });
        });
        var windowId = 0;
        while (usedWindows[windowId] && windowId <= 255) windowId++;
        if (windowId <= 255) {
          var entryIndex = archive.entries.length;
          var projectDocuments = [document].concat(Object.keys(state.histories || {}).map(function(key) {
            return state.histories[key] && state.histories[key].present;
          }).filter(Boolean));
          projectDocuments.forEach(function(projectDocument) {
            projectDocument.tracks.forEach(function(track) {
              track.clips.forEach(function(clip) {
                if (clip.payload.nativeDialogueAuthored === true &&
                    clip.payload.presentationArchiveSelector === selector) {
                  entryIndex = Math.max(entryIndex, clip.payload.presentationEntrySelector + 1);
                }
              });
            });
          });
          // The selected Actor speaks; otherwise the template's owner does.
          var owner = actor || document.actors.find(function(candidate) {
            return candidate.slot === template.words[4];
          });
          var speaker = owner && owner.label;
          if (!speaker || speaker.length > 28 || /[^\x20-\x7e]|[@{}]/.test(speaker)) {
            speaker = 'Speaker';
          }
          // New and wiped scenes run cards in list order from the playhead;
          // a retail insertion keeps its start delay after the boundary.
          var sequential = !!(scene.authored || scene.wiped);
          var startFrame = sequential ? view.frame : Math.max(view.frame, boundary.frame);
          return addAuthoredClip(rom, state, {
            label: 'Add native dialogue', type: 'dialogue', kind: 'dialogue',
            actorId: owner ? owner.id : null, durationFrames: 90,
            nativePreferred: true, boundary: boundary, startFrame: startFrame,
            payload: Object.assign({
              sourceSystem: 'serifu-authored-native', nativeDialogueAuthored: true,
              nativeTemplateNodeId: template.source.nodeId,
              presentationArchiveSelector: selector,
              presentationEntrySelector: entryIndex,
              windowId: windowId, nativeDelayTicks: sequential ? 0 : startFrame - boundary.frame,
              ownerSlot: owner && Number.isInteger(owner.slot) ? owner.slot : null,
              portrait: 0,
              speaker: speaker, text: 'New dialogue',
              timingStatus: 'native registered-counter start delay; preview seconds are approximate'
            }, extra || {})
          });
        }
      }
    }
    // No usable window in this scene: use the verified fixed-placement window,
    // so every scene with an insertion point can speak natively.
    if (boundary && OB64.cutsceneAuthoring && OB64.cutsceneCodec.dialogueWindowRecipes) {
      var recipe = OB64.cutsceneCodec.dialogueWindowRecipes['fixed-placement'];
      var recipeArchive = null;
      try { recipeArchive = OB64.cutsceneAuthoring.readDialogue(rom.z64, recipe[2]); } catch (_error) { recipeArchive = null; }
      if (recipeArchive) {
        var recipeWindows = {};
        document.native.commands.forEach(function(command) {
          if (command.words[0] === 0xBF) recipeWindows[command.words[1]] = true;
        });
        document.tracks.forEach(function(track) {
          track.clips.forEach(function(clip) {
            if (clip.payload.nativeDialogueAuthored) recipeWindows[clip.payload.windowId] = true;
          });
        });
        var recipeWindow = 0;
        while (recipeWindows[recipeWindow] && recipeWindow <= 255) recipeWindow++;
        if (recipeWindow <= 255) {
          // Entry indexes on a shared archive are reserved across the Project.
          var recipeEntry = recipeArchive.entries.length;
          [document].concat(Object.keys(state.histories || {}).map(function(key) {
            return state.histories[key] && state.histories[key].present;
          }).filter(Boolean)).forEach(function(projectDocument) {
            projectDocument.tracks.forEach(function(track) {
              track.clips.forEach(function(clip) {
                if (clip.payload.nativeDialogueAuthored === true &&
                    clip.payload.presentationArchiveSelector === recipe[2]) {
                  recipeEntry = Math.max(recipeEntry, clip.payload.presentationEntrySelector + 1);
                }
              });
            });
          });
          var recipeSpeaker = actor && actor.label;
          if (!recipeSpeaker || recipeSpeaker.length > 28 || /[^\x20-\x7e]|[@{}]/.test(recipeSpeaker)) {
            recipeSpeaker = 'Speaker';
          }
          var recipeSequential = !!(scene.authored || scene.wiped);
          var recipeStart = recipeSequential ? view.frame : Math.max(view.frame, boundary.frame);
          return addAuthoredClip(rom, state, {
            label: 'Add native dialogue', type: 'dialogue', kind: 'dialogue',
            actorId: actor ? actor.id : null, durationFrames: 90,
            nativePreferred: true, boundary: boundary, startFrame: recipeStart,
            payload: Object.assign({
              sourceSystem: 'serifu-authored-native', nativeDialogueAuthored: true,
              nativeTemplateNodeId: null, nativeWindow: 'fixed-placement',
              presentationArchiveSelector: recipe[2],
              presentationEntrySelector: recipeEntry,
              windowId: recipeWindow, nativeDelayTicks: recipeSequential ? 0 : recipeStart - boundary.frame,
              ownerSlot: actor && Number.isInteger(actor.slot) ? actor.slot : null,
              portrait: 0,
              speaker: recipeSpeaker, text: 'New dialogue',
              timingStatus: 'native registered-counter start delay; preview seconds are approximate'
            }, extra || {})
          });
        }
      }
    }
    if (extra && extra.choice) throw new UiError('A choice needs a native dialogue window here; this scene has no insertion point for one.');
    addAuthoredClip(rom, state, {
      label: 'Add dialogue preview', type: 'dialogue', kind: 'dialogue',
      actorId: actor ? actor.id : null, durationFrames: 90,
      payload: {
        sourceSystem: 'authored-dialogue',
        speaker: actor ? actor.label : 'Narrator', text: 'New dialogue',
        associationStatus: 'authored preview metadata'
      }
    });
  }

  // A two-option choice: the player picks option 1 or 2, and actions set to
  // run after an option play only for it (native control byte 3 queries).
  function addChoice(rom, state) {
    return addDialogue(rom, state, { choice: { option1: 'Yes', option2: 'No' }, speaker: 'Choice', text: 'Yes / No' });
  }

  // A choice's two options: from a new Choice card, or read back from the
  // text of a reopened native choice window.
  function choiceOptions(clip) {
    var payload = clip && clip.payload || {};
    if (payload.choice) return payload.choice;
    if (payload.nativeChoice && OB64.cutsceneAuthoring && OB64.cutsceneAuthoring.choiceOptionsFromRawText) {
      return OB64.cutsceneAuthoring.choiceOptionsFromRawText(payload.rawText) || { option1: '?', option2: '?' };
    }
    return null;
  }

  // Which option of which choice a card runs after: a tag on a new card, a
  // native branch member, or a new card placed at a reopened branch end.
  function cardBranch(document, clip) {
    var payload = clip.payload || {};
    if (payload.choiceBranch) return payload.choiceBranch;
    if (payload.nativeChoiceBranch) return payload.nativeChoiceBranch;
    var target = clip.source && clip.source.insertBeforeNodeId;
    if (!target || clip.source.nodeId) return null;
    var found = null;
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(candidate) {
        var native = candidate.payload && candidate.payload.nativeChoice;
        var at = native ? native.branchEndNodeIds.indexOf(target) : -1;
        if (at >= 0) found = { choiceClipId: candidate.id, option: at + 1 };
      });
    });
    return found;
  }

  // The preview option of every choice in the scene, in list order.
  function choicePreviewSequence(state, scene, document) {
    var chosen = state.views && state.views[scene.sceneId] && state.views[scene.sceneId].previewChoices || {};
    return orderedClips(document).filter(function(row) {
      return row.clip.kind === 'dialogue' && row.clip.payload && (row.clip.payload.choice || row.clip.payload.nativeChoice);
    }).map(function(row) { return chosen[row.clip.id] === 2 ? 2 : 1; });
  }

  // Earlier choices at the same insertion point that a card can run after.
  function choicesBefore(document, row) {
    var rows = creatorActionRows(document, viewForDocument(document));
    var index = rows.findIndex(function(candidate) { return candidate.clip.id === row.clip.id; });
    return rows.slice(0, Math.max(0, index)).filter(function(candidate) {
      return candidate.clip.payload && candidate.clip.payload.choice &&
        candidate.clip.source.insertBeforeNodeId === row.clip.source.insertBeforeNodeId;
    });
  }

  function viewForDocument(document) {
    return { pathId: 'default' };
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
    var boundary = boundaryForFrame(scene, document, view.frame, state);
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

  function addFade(rom, state) {
    var preset = FADE_PRESETS['to-black'];
    return addAuthoredClip(rom, state, {
      label: 'Add fade', type: 'effect', kind: 'fade', durationFrames: 60,
      nativePreferred: true, trackLabel: 'Scene fades',
      payload: {
        direction: 'to-black', red: preset.red, green: preset.green, blue: preset.blue,
        startAlpha: preset.startAlpha, targetAlpha: preset.targetAlpha,
        timingStatus: 'authored-preview-frame'
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
    view.timelineMode = 'preview';
    view.selectedClipId = clipIdValue;
    view.editPanel = 'actions';
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
      view.editPanel = 'actions';
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
      view.editPanel = 'actions';
      executeEdit(rom, state, 'Place actor on Stage', function(next) {
        var row = findClipRow(next, poseRow.clip.id);
        var placed = nativePosition(position);
        row.clip.payload.x = placed.x;
        row.clip.payload.y = placed.y;
        row.clip.payload.z = placed.z;
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
      var scene = selectedScene(state);
      var view = viewFor(state, scene.sceneId);
      if (view.creatorDestinationClipId) {
        var target = findClipRow(selectedDocument(state), view.creatorDestinationClipId);
        view.creatorDestinationClipId = null;
        if (target && target.clip.kind === 'movement') {
          var worldPoint = OB64.cutsceneRenderer.unprojectPoint(
            OB64.cutsceneRenderer.untransformStagePoint(point, state.renderedStage.camera),
            state.renderedStage.projection, Number((target.clip.payload.from || {}).y) || 0);
          editClip(rom, state, target.clip.id, 'Choose movement destination', function(next) {
            next.clip.payload.to = { x: Math.round(worldPoint.x * 1000) / 1000,
              y: worldPoint.y, z: Math.round(worldPoint.z * 1000) / 1000 };
          });
        }
        return;
      }
      var actorId = OB64.cutsceneRenderer.hitTest(state.renderedStage, point.x, point.y);
      if (!actorId) return;
      view.selectedActorId = actorId;
      view.editPanel = 'actors';
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
    canvas.addEventListener('keydown', function(event) {
      if (event.key !== 'Escape') return;
      viewFor(state, selectedScene(state).sceneId).creatorDestinationClipId = null;
      rerender(rom, state);
    });
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
    if (clip.payload.nativeDialogueAuthored === true) {
      // The box is placed near whichever Actor it is assigned to. The avatar is
      // chosen separately, so it does not change here.
      clip.payload.ownerSlot = actor && Number.isInteger(actor.slot) ? actor.slot : null;
    }
  }

  // Classes whose appearance route at z64 0x64BE0 names a portrait. Identity 0
  // (class None) has an empty route and draws a clear portrait area.
  function portraitClassIds(rom, state) {
    if (state.portraitClassIds) return state.portraitClassIds;
    var ids = [];
    var z64 = rom && rom.z64;
    if (z64 && z64.length > 0x64BE0 + 165 * 6) {
      var view = new DataView(z64.buffer, z64.byteOffset, z64.byteLength);
      for (var id = 1; id < 165; id++) if (view.getUint16(0x64BE0 + id * 6)) ids.push(id);
    }
    state.portraitClassIds = ids;
    return ids;
  }

  function portraitClassLabel(id) {
    var name = OB64.CLASS_NAMES && OB64.CLASS_NAMES[id];
    return (name || 'Class ' + id) + ' (' + id + ')';
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
      view.editPanel = 'actions';
    });
  }

  function renderClipInspector(inspector, rom, state, scene, document, row, title, compact) {
    if (!compact) inspector.appendChild(node('h3', '', title || 'Selected action editor'));
    if (!row) {
      inspector.appendChild(node('p', 'cutscene-empty-note',
        'Select a step from the list or timeline to edit it.'));
      return;
    }
    var heading = node('div', 'cutscene-clip-heading');
    heading.appendChild(node('strong', '', clipLabel(row, document)));
    heading.appendChild(capabilityBadge(row.clip.capability, 'command'));
    if (!compact) inspector.appendChild(heading);
    if (!compact) inspector.appendChild(node('p', 'cutscene-field-hint', row.clip.source.nodeId
      ? 'Source-backed command · its native boundary is preserved.'
      : (row.clip.source.insertBeforeNodeId
        ? 'Authored at an approved native insertion boundary.'
        : 'Preview-only Storyboard content.')));

    var timing = node('div', 'cutscene-timing-grid');
    var fixedWait = row.clip.kind === 'wait' && row.clip.payload.registeredCounterEditable === true;
    function setDuration(next, frames) {
      var minimum = next.clip.kind === 'wait' || next.clip.kind === 'movement' ||
        next.clip.kind === 'pose' || next.clip.kind === 'camera' || next.clip.kind === 'fade' ||
        next.clip.kind === 'effect' &&
          next.clip.payload.sourceSystem === 'cutscene-sprite-native' &&
          !next.clip.source.nodeId ? 1 : 0;
      next.clip.durationFrames = Math.max(minimum, frames);
      if (next.clip.kind === 'movement') next.clip.payload.durationMode = 'duration';
      if (next.clip.kind === 'wait' && next.clip.payload.registeredCounterEditable === true) {
        next.clip.payload.nativeTicks = next.clip.durationFrames;
        next.clip.payload.registeredCounterTarget = next.clip.durationFrames;
        next.clip.capability = 'native';
      }
    }
    if (compact) {
      // Simple cards use seconds only. Native update counts stay in Advanced.
      if (!row.clip.source.nodeId) {
        timing.appendChild(field('Starts at (seconds)', numericInput(
          (row.clip.startFrame / 30).toFixed(2), '0.1', function(value) {
            editClip(rom, state, row.clip.id, 'Change clip start', function(next, nextDocument) {
              next.clip.startFrame = Math.max(0, Math.round(value * 30));
              if (next.clip.payload.nativeDialogueAuthored === true) {
                retimeAuthoredDialogue(nextDocument, next.clip, next.clip.startFrame, scene, state);
              } else if (next.clip.source.insertBeforeNodeId) {
                var boundary = boundaryForFrame(scene, nextDocument, next.clip.startFrame, state);
                if (boundary) next.clip.source.insertBeforeNodeId = boundary.id;
              }
            });
          }, 'clip-start-seconds:' + row.clip.id)));
      }
      if (!row.clip.source.nodeId || fixedWait || row.clip.kind === 'movement' ||
          row.clip.kind === 'camera') {
        timing.appendChild(field(row.clip.kind === 'wait' ? 'Wait (seconds)' : 'Duration (seconds)',
          numericInput((row.clip.durationFrames / 30).toFixed(2), '0.1', function(value) {
            editClip(rom, state, row.clip.id, 'Change clip duration', function(next) {
              setDuration(next, Math.round(value * 30));
            });
          }, 'clip-duration-seconds:' + row.clip.id)));
      }
    } else if (fixedWait) {
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
        if (next.clip.payload.nativeDialogueAuthored === true) {
          retimeAuthoredDialogue(nextDocument, next.clip, next.clip.startFrame, scene, state);
        } else if (!next.clip.source.nodeId && next.clip.source.insertBeforeNodeId) {
          var boundary = boundaryForFrame(scene, nextDocument, next.clip.startFrame, state);
          if (boundary) next.clip.source.insertBeforeNodeId = boundary.id;
        }
      });
    }, 'clip-start-frame:' + row.clip.id)));
    timing.appendChild(field('Start preview seconds', numericInput((row.clip.startFrame / 30).toFixed(2),
      '0.1', function(value) {
        editClip(rom, state, row.clip.id, 'Change clip start', function(next, nextDocument) {
          next.clip.startFrame = Math.max(0, Math.round(value * 30));
          if (next.clip.payload.nativeDialogueAuthored === true) {
            retimeAuthoredDialogue(nextDocument, next.clip, next.clip.startFrame, scene, state);
          } else if (!next.clip.source.nodeId && next.clip.source.insertBeforeNodeId) {
            var boundary = boundaryForFrame(scene, nextDocument, next.clip.startFrame, state);
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

    if (!compact || document.branches.length > 1) {
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
      inspector.appendChild(field('Preview path', pathSelect, compact ? null :
        'Native condition targets remain blocked until their branch adapter is reviewed.'));
    }

    if (['actor', 'pose', 'movement', 'dialogue'].indexOf(row.track.type) !== -1) {
      var actorSelect = node('select');
      if (row.track.type === 'dialogue') {
        var narrator = node('option', '', 'Narrator / scene'); narrator.value = '';
        actorSelect.appendChild(narrator);
      }
      document.actors.forEach(function(actor) {
        var option = node('option', '', actor.label + (compact ? '' : ' · slot ' + actor.slot));
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

    if (row.clip.kind === 'fade') {
      var fadeSelect = node('select');
      Object.keys(FADE_PRESETS).forEach(function(key) {
        var option = node('option', '', FADE_PRESETS[key].label);
        option.value = key; fadeSelect.appendChild(option);
      });
      fadeSelect.value = FADE_PRESETS[row.clip.payload.direction] ? row.clip.payload.direction : 'to-black';
      fadeSelect.setAttribute('data-cutscene-focus-key', 'clip-fade-direction:' + row.clip.id);
      fadeSelect.addEventListener('change', function() {
        var preset = FADE_PRESETS[fadeSelect.value];
        if (!preset) return;
        editClip(rom, state, row.clip.id, 'Change fade', function(next) {
          next.clip.payload.direction = fadeSelect.value;
          next.clip.payload.red = preset.red;
          next.clip.payload.green = preset.green;
          next.clip.payload.blue = preset.blue;
          next.clip.payload.startAlpha = preset.startAlpha;
          next.clip.payload.targetAlpha = preset.targetAlpha;
        });
      });
      inspector.appendChild(field('Fade', fadeSelect,
        'The whole screen fades over the duration. Fade in reveals the scene again.'));
    } else if (row.clip.kind === 'opacity') {
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
      var movementHost = compact ? creatorDetails(viewFor(state, scene.sceneId),
        'coordinates:' + row.clip.id, 'Coordinates') : inspector;
      addPositionFields(movementHost, rom, state, row, 'from', 'From');
      addPositionFields(movementHost, rom, state, row, 'to', 'To');
      if (compact) {
        inspector.appendChild(button('Choose destination on Stage', 'btn-secondary', function() {
          viewFor(state, scene.sceneId).creatorDestinationClipId = row.clip.id;
          rerender(rom, state);
          if (state.ui.canvas && state.ui.canvas.focus) state.ui.canvas.focus();
        }));
        inspector.appendChild(movementHost);
      } else {
        inspector.appendChild(button('Add keyframe at playhead', 'btn-secondary', function() {
          splitMovementAtPlayhead(rom, state, row);
        }));
      }
    } else if (row.clip.kind === 'dialogue' && row.clip.payload.nativeChoice && row.clip.payload.nativeDialogueEditable) {
      var nativeOptions = choiceOptions(row.clip);
      [1, 2].forEach(function(option) {
        inspector.appendChild(field('Option ' + option, textInput(nativeOptions['option' + option], function(value) {
          editClip(rom, state, row.clip.id, 'Edit choice option', function(next) {
            var current = choiceOptions(next.clip);
            var edited = Object.assign({}, current);
            edited['option' + option] = value;
            next.clip.payload.rawText = OB64.cutsceneAuthoring.choiceRawText(edited);
          });
        }, 'clip-choice-option' + option + ':' + row.clip.id), option === 1
          ? 'The player picks one option; up to 24 characters each.' : null));
      });
      var nativePreview = viewFor(state, scene.sceneId);
      nativePreview.previewChoices = nativePreview.previewChoices || {};
      var nativePreviewRow = node('div', 'cutscene-choice-preview');
      nativePreviewRow.appendChild(node('span', 'cutscene-field-label', 'Preview this choice as option'));
      [1, 2].forEach(function(option) {
        var pick = button(String(option), 'btn-secondary', function() {
          nativePreview.previewChoices[row.clip.id] = option;
          delete state.runtimeByAssetId[scene.assetId];
          var history = historyFor(state, scene);
          if (history) refreshRuntime(state, scene, history.present, rom);
          rerender(rom, state);
        });
        pick.setAttribute('aria-pressed', (nativePreview.previewChoices[row.clip.id] || 1) === option ? 'true' : 'false');
        pick.setAttribute('data-cutscene-focus-key', 'clip-choice-preview:' + option);
        nativePreviewRow.appendChild(pick);
      });
      inspector.appendChild(nativePreviewRow);
    } else if (row.clip.kind === 'dialogue' && row.clip.payload.nativeDialogueAuthored === true && row.clip.payload.choice) {
      [1, 2].forEach(function(option) {
        inspector.appendChild(field('Option ' + option, textInput(row.clip.payload.choice['option' + option], function(value) {
          editClip(rom, state, row.clip.id, 'Edit choice option', function(next) {
            var choice = Object.assign({}, next.clip.payload.choice);
            choice['option' + option] = value;
            next.clip.payload.choice = choice;
            next.clip.payload.text = choice.option1 + ' / ' + choice.option2;
          });
        }, 'clip-choice-option' + option + ':' + row.clip.id), option === 1
          ? 'The player picks one option; up to 24 characters each.' : null));
      });
      var previewView = viewFor(state, scene.sceneId);
      previewView.previewChoices = previewView.previewChoices || {};
      var previewRow = node('div', 'cutscene-choice-preview');
      previewRow.appendChild(node('span', 'cutscene-field-label', 'Preview this choice as option'));
      [1, 2].forEach(function(option) {
        var pick = button(String(option), 'btn-secondary', function() {
          previewView.previewChoices[row.clip.id] = option;
          delete state.runtimeByAssetId[scene.assetId];
          var history = historyFor(state, scene);
          if (history) refreshRuntime(state, scene, history.present, rom);
          rerender(rom, state);
        });
        pick.setAttribute('aria-pressed', (previewView.previewChoices[row.clip.id] || 1) === option ? 'true' : 'false');
        pick.setAttribute('data-cutscene-focus-key', 'clip-choice-preview:' + option);
        previewRow.appendChild(pick);
      });
      inspector.appendChild(previewRow);
      inspector.appendChild(node('p', 'cutscene-field-hint',
        'Set other actions to run after option 1 or option 2 with their “Runs” setting.'));
    } else if (row.clip.kind === 'dialogue' && row.clip.payload.nativeDialogueAuthored === true) {
      inspector.appendChild(field('Speaker', textInput(row.clip.payload.speaker, function(value) {
        editClip(rom, state, row.clip.id, 'Edit dialogue speaker', function(next) {
          next.clip.payload.speaker = value;
        });
      }, 'clip-speaker:' + row.clip.id),
      'This name appears in the game dialogue box.'));
      var faceSelect = node('select');
      var faceChoices = [['0', 'No face']].concat(portraitClassIds(rom, state).map(function(id) {
        return [String(id), portraitClassLabel(id)];
      }), [['actor', 'Assigned Actor’s own face']]);
      faceChoices.forEach(function(entry) {
        var option = node('option', '', entry[1]); option.value = entry[0]; faceSelect.appendChild(option);
      });
      var portrait = row.clip.payload.portrait;
      faceSelect.value = portrait === 'actor' ? 'actor'
        : (Number.isInteger(portrait) && portrait > 0 ? String(portrait) : '0');
      faceSelect.setAttribute('data-cutscene-focus-key', 'clip-dialogue-face:' + row.clip.id);
      faceSelect.addEventListener('change', function() {
        editClip(rom, state, row.clip.id, 'Change dialogue avatar', function(next) {
          next.clip.payload.portrait = faceSelect.value === 'actor' ? 'actor' : Number(faceSelect.value);
        });
      });
      inspector.appendChild(field('Avatar', faceSelect,
        'The portrait drawn beside the text. Any class with a portrait can speak, whoever is on the Stage.'));
      inspector.appendChild(field('Dialogue', textInput(row.clip.payload.text, function(value) {
        editClip(rom, state, row.clip.id, 'Edit dialogue text', function(next) {
          next.clip.payload.text = value;
        });
      }, 'clip-dialogue:' + row.clip.id, true),
      'The game waits for A after this text. New lines are supported.'));
      inspector.appendChild(field('Delay before opening (seconds)', numericInput(
        (row.clip.payload.nativeDelayTicks / 30).toFixed(2), '0.1', function(value) {
          editClip(rom, state, row.clip.id, 'Change dialogue start delay', function(next, nextDocument) {
            var ticks = Math.max(0, Math.round(value * 30));
            var boundary = dialogueInsertionBoundaries(nextDocument, scene, state).find(function(candidate) {
              return candidate.id === next.clip.source.insertBeforeNodeId;
            });
            next.clip.payload.nativeDelayTicks = ticks;
            if (boundary) next.clip.startFrame = boundary.frame + ticks;
          });
        }, 'clip-dialogue-delay:' + row.clip.id),
      'The box opens after this delay. The game counts it in its own update steps.'));
      if (!compact) inspector.appendChild(node('p', 'cutscene-field-hint',
        'The scene uses a new entry in serifu archive selector ' +
        row.clip.payload.presentationArchiveSelector + '. Timeline duration is a preview aid.'));
    } else if (row.clip.kind === 'dialogue' && row.clip.payload.nativeDialogueEditable) {
      inspector.appendChild(node('h4', '', 'Readable dialogue'));
      inspector.appendChild(node('p', 'cutscene-dialogue-readable',
        readableNativeDialogue(row.clip.payload.rawText)));
      inspector.appendChild(node('p', 'cutscene-field-hint',
        'Control codes are hidden here. Unresolved game markers appear in brackets.'));
      var nativeSourceView = viewFor(state, scene.sceneId);
      var nativeSource = node('details', 'cutscene-source-details');
      nativeSource.open = nativeSourceView.openNativeDialogueSourceId === row.clip.id;
      nativeSource.addEventListener('toggle', function() {
        nativeSourceView.openNativeDialogueSourceId = nativeSource.open ? row.clip.id : null;
      });
      var nativeSourceSummary = node('summary', '', 'Raw ROM dialogue source · advanced');
      nativeSourceSummary.setAttribute('data-cutscene-focus-key',
        'clip-native-dialogue-source:' + row.clip.id);
      nativeSource.appendChild(nativeSourceSummary);
      nativeSource.appendChild(field('Raw source', textInput(row.clip.payload.rawText,
        function(value) {
          editClip(rom, state, row.clip.id, 'Edit native dialogue text', function(next) {
            next.clip.payload.rawText = value;
          });
        }, 'clip-native-dialogue:' + row.clip.id, true),
      'Keep the @ control tokens. Printable ASCII only. This shared archive entry changes wherever the game uses it. Longer text uses the shared Cutscene allocation area.'));
      inspector.appendChild(nativeSource);
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
              boundaryForFrame(scene, nextDocument, next.clip.startFrame, state);
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
          ['translateX', 'Pan X', 0, '1'],
          ['translateY', 'Pan Y', 0, '1'],
          ['scaleX', 'Zoom X', 1, '0.05'],
          ['scaleY', 'Zoom Y', 1, '0.05']
        ];
        var projectionGroups = row.clip.payload.target
          ? [['target', compact ? 'Pan to' : 'Native projection target']] : [['from', 'From'], ['to', 'To']];
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
      } else if (presentationKind === 'projection-transform' && !compact) {
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

  // Event launch sites. Bindings are one validated set on the Studio state;
  // export writes them, and ROM-only reopen reads them back.
  function sceneBindings(state, scene) {
    return OB64.cutsceneEvents ? OB64.cutsceneEvents.bindingsForScene(state.catalog, state.eventBindings, scene) : [];
  }

  function reboundRetailSites(state, scene) {
    if (!OB64.cutsceneEvents || scene.authored) return [];
    var bound = {};
    (state.eventBindings || []).forEach(function(binding) { bound[binding.launchSiteId] = true; });
    return OB64.cutsceneEvents.sitesForScene(state.catalog, scene).filter(function(site) { return bound[site.launchSiteId]; });
  }

  function launchCountLabel(state, scene) {
    var count = sceneBindings(state, scene).length;
    return count ? 'launched by ' + count + ' event site' + (count === 1 ? '' : 's') : 'not launched yet';
  }

  function launchSummary(state, scene) {
    var count = sceneBindings(state, scene).length;
    if (scene.authored && !count) return 'Not launched by any event yet.';
    return count ? 'Launched by ' + count + ' assigned event site' + (count === 1 ? '' : 's') + '.' : '';
  }

  function siteLabel(site) {
    return 'Event row ' + site.row + ' · site b' + site.offset.toString(16).toUpperCase().padStart(4, '0');
  }

  function bindingTargetName(state, binding) {
    var target = binding.target;
    var scene = target.kind === 'authored'
      ? state.catalog.getScene('scene:authored:' + target.authoredId)
      : state.catalog.getScene(target.sceneId);
    return scene ? OB64.cutsceneCatalog.displayName(scene) : 'an unknown scene';
  }

  function siteCurrentTarget(state, site) {
    var binding = (state.eventBindings || []).find(function(item) { return item.launchSiteId === site.launchSiteId; });
    if (binding) return { binding: binding, name: bindingTargetName(state, binding) };
    var retail = state.catalog.getScene(site.retailSceneId);
    return { binding: null, name: retail ? OB64.cutsceneCatalog.displayName(retail) : 'selector row ' + site.selector };
  }

  function sceneTarget(scene) {
    if (scene.authored) return { kind: 'authored', authoredId: scene.authored.authoredId };
    var rows = scene.source && scene.source.directorSelectorRows || [];
    if (!rows.length) throw new UiError('This scene has no Director selector row to launch.');
    return { kind: 'physical', sceneId: scene.sceneId, selectorRow: rows[0] };
  }

  function setEventBindings(rom, state, next, label) {
    var events = OB64.cutsceneEvents;
    var normalized = events.normalize(state.catalog, next);
    var before = state.eventBindings || [];
    state.eventBindings = events.publicBindings(normalized);
    invalidateAfterBindingChange(rom, state, before);
    notifyChange(state, label);
    if (state.ui) rerender(rom, state);
    return state.eventBindings;
  }

  // A binding change moves launch contexts: the old and new targets, the
  // retail owners of changed sites, and every scene launched after one of
  // those sites (its predecessor changes). Cached context runtimes and other
  // scenes' runtimes are dropped; chosen contexts reset where they changed.
  function invalidateAfterBindingChange(rom, state, before) {
    var events = OB64.cutsceneEvents;
    var affected = {};
    before.concat(state.eventBindings).forEach(function(binding) {
      var target = binding.target;
      affected[target.kind === 'authored' ? 'scene:authored:' + target.authoredId : target.sceneId] = true;
      var site = events.site(state.catalog, binding.launchSiteId);
      if (site) affected[site.retailSceneId] = true;
    });
    // A binding also changes the predecessor of every scene launched after
    // that site, so every cached context runtime and other scene runtime is
    // dropped and rebuilt on open; chosen contexts reset where they changed.
    state.concurrentRuntimeByLaunchContext = {};
    Object.keys(affected).forEach(function(sceneId) {
      var affectedScene = state.catalog.getScene(sceneId);
      if (affectedScene && state.views[affectedScene.sceneId]) state.views[affectedScene.sceneId].launchContextId = null;
    });
    // The selected scene is refreshed in place when a ROM is at hand;
    // otherwise (Project load) its preview is dropped and rebuilt on open.
    state.catalog.scenes.forEach(function(other) {
      if (other.sceneId !== state.selectedSceneId || !rom) delete state.runtimeByAssetId[other.assetId];
    });
    affected[state.selectedSceneId] = true;
    var selected = selectedScene(state);
    var selectedHistory = selected && affected[selected.sceneId] && historyFor(state, selected);
    if (selectedHistory && rom) refreshRuntime(state, selected, selectedHistory.present, rom);
  }

  // Make one launch site start this scene. A site already bound elsewhere is
  // reassigned; binding a retail site back to its own scene removes the binding.
  function assignLaunchSite(rom, state, scene, launchSiteId) {
    var events = OB64.cutsceneEvents;
    if (!events) throw new UiError('This editor build cannot bind event launch sites.');
    var site = events.site(state.catalog, launchSiteId);
    if (!site) throw new UiError('That launch site is not in the retail catalog.');
    var rest = (state.eventBindings || []).filter(function(binding) { return binding.launchSiteId !== launchSiteId; });
    if (!scene.authored && site.retailSceneId === scene.sceneId) {
      return setEventBindings(rom, state, rest, siteLabel(site) + ' launches its retail scene again');
    }
    return setEventBindings(rom, state, rest.concat([{ launchSiteId: launchSiteId, target: sceneTarget(scene) }]),
      siteLabel(site) + ' now launches ' + OB64.cutsceneCatalog.displayName(scene));
  }

  function removeLaunchSite(rom, state, launchSiteId) {
    var site = OB64.cutsceneEvents.site(state.catalog, launchSiteId);
    return setEventBindings(rom, state, (state.eventBindings || []).filter(function(binding) {
      return binding.launchSiteId !== launchSiteId;
    }), (site ? siteLabel(site) : launchSiteId) + ' launches its retail scene again');
  }

  // Background: the scene's own background command. Classes 4 and 5 select a
  // scene group (0..30); classes 1 and 8 select a mode-two environment (0..79),
  // whose foreground pieces and scenery follow the same selector. Export
  // rewrites only that command's operand.
  function backgroundRouteLabel(projection) {
    return projection.route === 'native-environment' ? 'Environment' : 'Scene group';
  }

  function nativeBackgroundGroupLabel(state, projection, group) {
    if (projection.route === 'native-environment') {
      var pieces = (group.members || []).filter(function(member) { return member.role === 'foreground-mask'; }).length;
      return 'Environment ' + group.selector + (pieces ? ' · with ' + (pieces === 1 ? 'a foreground piece' : pieces + ' foreground pieces') +
        ' over the Actors' : '');
    }
    var names = group.archiveAssetIds.map(function(assetId) {
      var asset = state.catalog.getImageAsset(assetId);
      return asset ? asset.displayName : assetId;
    });
    return backgroundRouteLabel(projection) + ' ' + group.selector + (names.length ? ' · ' + names.join(' + ') : '');
  }

  function selectNativeBackground(rom, state, scene, selector) {
    var document = historyFor(state, scene).present;
    var projection = document.background.projection || {};
    if (projection.nativeEditable !== true) fail(backgroundReadOnlyReason(state, scene, document));
    var group = (projection.nativeGroups || []).find(function(candidate) { return candidate.selector === selector; });
    if (!group) fail(backgroundRouteLabel(projection) + ' ' + selector + ' has no complete background in the ROM.');
    if (projection.selectedSelector === selector && document.background.capability === 'native') return;
    // executeEdit acts on the selected scene.
    state.selectedSceneId = scene.sceneId;
    executeEdit(rom, state, 'Select background ' + backgroundRouteLabel(projection).toLowerCase() + ' ' + selector, function(next) {
      var nextProjection = next.background.projection;
      next.background.layers = OB64.cutsceneCodec.backgroundGroupLayers(group, state.catalog, 'native',
        nextProjection.route === 'native-environment' ? 'native-environment' : 'native-scene-group');
      next.background.assetId = next.background.layers.length ? next.background.layers[0].assetId : null;
      next.background.capability = 'native';
      nextProjection.mode = next.background.layers.length ? 'stage-fit' : 'unresolved';
      nextProjection.selectedSelector = group.selector;
      nextProjection.selectedGroupResourceKey = group.groupResourceKey;
      nextProjection.previewOverride = false;
      var priorCapability = next.exportRequirements.capability;
      next.exportRequirements.reasons = (next.exportRequirements.reasons || []).filter(function(reason) {
        return !/background/i.test(reason);
      });
      next.exportRequirements.capability = next.exportRequirements.reasons.length &&
          priorCapability !== 'native' ? 'preview-only' : 'native';
    });
  }

  function backgroundReadOnlyReason(state, scene, document) {
    var projection = document.background.projection || {};
    var profile = scene.launchProfile && scene.launchProfile.background || null;
    var request = profile && profile.requests && profile.requests[0] || null;
    if (request && request.commandOperand === -1) {
      return 'The game picks this background from the battlefield when the scene starts, so it cannot be chosen here.';
    }
    if (!request && profile && profile.inheritedPresentation) {
      var contexts = profile.inheritedPresentation.inheritanceContexts || profile.inheritanceContexts || [];
      var predecessor = contexts.length && contexts[0].precedingSceneId
        ? state.catalog.getScene(contexts[0].precedingSceneId) : null;
      return 'This scene has no background command. It keeps the background already on screen' +
        (predecessor ? ' from ' + OB64.cutsceneCatalog.displayName(predecessor) : ' from the scene before it') + '.';
    }
    if (!request) return 'This scene has no background command. It keeps whatever the game already shows.';
    return 'This scene\'s background is not chosen by its own command on this route (' +
      (projection.modeStatus || 'launch mode unresolved') + ').';
  }

  function backgroundThumbnail(rom, state, assetId) {
    var canvas = node('canvas', 'cutscene-background-thumb');
    canvas.width = 96; canvas.height = 72;
    var asset = assetId && state.catalog.getImageAsset(assetId);
    if (!asset || typeof canvas.getContext !== 'function') return canvas;
    decodeImageAsset(rom, state, asset).then(function(image) {
      if (!image || !image.renderable || !image.rgba || typeof document === 'undefined') return;
      var full = document.createElement('canvas');
      full.width = image.width; full.height = image.height;
      var fullContext = full.getContext('2d');
      var data = fullContext.createImageData(image.width, image.height);
      data.data.set(image.rgba);
      fullContext.putImageData(data, 0, 0);
      canvas.getContext('2d').drawImage(full, 0, 0, canvas.width, canvas.height);
    }).catch(function() {});
    return canvas;
  }

  function renderBackgroundDrawer(drawer, rom, state, scene, document) {
    var panel = node('div', 'cutscene-background-panel');
    panel.setAttribute('data-cutscene-focus-key', 'background-panel');
    panel.appendChild(node('h3', '', 'Background'));
    var projection = document.background.projection || {};
    if (projection.nativeEditable !== true || !(projection.nativeGroups || []).length) {
      var reason = node('p', 'cutscene-field-hint', backgroundReadOnlyReason(state, scene, document));
      reason.setAttribute('data-cutscene-focus-key', 'background-read-only');
      panel.appendChild(reason);
      drawer.appendChild(panel);
      return;
    }
    panel.appendChild(node('p', 'cutscene-field-hint', projection.route === 'native-environment'
      ? 'The game loads this environment when the scene starts. Its foreground pieces and scenery come with it.'
      : 'The game loads this scene group when the scene starts. All of its layers come together.'));
    var groups = projection.nativeGroups;
    var byselector = {};
    groups.forEach(function(group) { byselector[group.selector] = group; });
    var current = byselector[projection.selectedSelector];
    var original = byselector[projection.sourceSelector];
    var status = node('div', 'cutscene-background-current');
    status.setAttribute('data-cutscene-focus-key', 'background-current');
    status.appendChild(node('span', '', 'Now: ' + (current ? nativeBackgroundGroupLabel(state, projection, current) : 'unresolved')));
    if (projection.selectedSelector !== projection.sourceSelector) {
      status.appendChild(node('span', 'cutscene-field-hint', 'Original: ' +
        (original ? nativeBackgroundGroupLabel(state, projection, original) : backgroundRouteLabel(projection) + ' ' + projection.sourceSelector)));
      var restore = button('Restore original', 'btn-secondary', function() {
        try { selectNativeBackground(rom, state, scene, projection.sourceSelector); } catch (error) {
          if (state.callbacks.onStatus) state.callbacks.onStatus(error.message);
        }
      });
      restore.setAttribute('data-cutscene-focus-key', 'background-restore');
      status.appendChild(restore);
    }
    panel.appendChild(status);
    var grid = node('div', 'cutscene-background-grid');
    groups.forEach(function(group) {
      var choice = button('', 'cutscene-background-choice', function() {
        try { selectNativeBackground(rom, state, scene, group.selector); } catch (error) {
          if (state.callbacks.onStatus) state.callbacks.onStatus(error.message);
        }
      });
      choice.setAttribute('data-cutscene-focus-key', 'background-choice:' + group.selector);
      choice.setAttribute('aria-pressed', group.selector === projection.selectedSelector ? 'true' : 'false');
      choice.title = nativeBackgroundGroupLabel(state, projection, group);
      choice.appendChild(backgroundThumbnail(rom, state, group.archiveAssetIds[0]));
      choice.appendChild(node('span', 'cutscene-background-caption', backgroundRouteLabel(projection) + ' ' + group.selector +
        (group.selector === projection.sourceSelector ? ' · original' : '')));
      grid.appendChild(choice);
    });
    panel.appendChild(grid);
    drawer.appendChild(panel);
  }

  function renderLaunchDrawer(drawer, rom, state, scene) {
    var events = OB64.cutsceneEvents;
    var panel = node('div', 'cutscene-launch-panel');
    panel.setAttribute('data-cutscene-focus-key', 'launch-panel');
    panel.appendChild(node('h3', '', 'Event launch sites'));
    if (!events) {
      panel.appendChild(node('p', 'cutscene-source-warning', 'This editor build cannot bind event launch sites.'));
      drawer.appendChild(panel);
      return;
    }
    var view = viewFor(state, scene.sceneId);
    function act(action) {
      try { action(); } catch (error) {
        if (state.callbacks.onStatus) state.callbacks.onStatus(error && error.message || String(error));
      }
    }
    panel.appendChild(node('p', 'cutscene-field-hint', 'A launch site is one exact place in an event program that ' +
      'starts a cutscene. Assigning it here makes that site start this scene instead. Export keeps the retail ' +
      'event program and writes a relocated copy; removing every assignment restores it.'));

    var current = node('div', 'cutscene-launch-current');
    current.appendChild(node('h4', '', 'Launches this scene'));
    var bindings = sceneBindings(state, scene);
    var retailSites = scene.authored ? [] : events.sitesForScene(state.catalog, scene);
    var rebound = reboundRetailSites(state, scene);
    if (!bindings.length && !retailSites.length) current.appendChild(node('p', '', 'No event launches this scene yet.'));
    bindings.forEach(function(binding) {
      var site = events.site(state.catalog, binding.launchSiteId);
      var row = node('div', 'cutscene-launch-row');
      row.appendChild(node('span', 'cutscene-launch-site', siteLabel(site)));
      row.appendChild(node('span', 'cutscene-launch-context', (site.mission || site.role || 'Event program') +
        ' · assigned in this Project'));
      var remove = button('Remove', 'btn-secondary', function() { act(function() { removeLaunchSite(rom, state, binding.launchSiteId); }); });
      remove.setAttribute('data-cutscene-focus-key', 'launch-remove:' + binding.launchSiteId);
      row.appendChild(remove);
      current.appendChild(row);
    });
    if (retailSites.length) {
      current.appendChild(node('p', 'cutscene-field-hint', retailSites.length + ' retail launch site' +
        (retailSites.length === 1 ? '' : 's') + ' start this scene' +
        (rebound.length ? '; ' + rebound.length + ' now launch' + (rebound.length === 1 ? 'es' : '') + ' another scene.' : '.')));
    }
    panel.appendChild(current);

    var assign = node('div', 'cutscene-launch-assign');
    assign.appendChild(node('h4', '', 'Assign a launch site'));
    var search = node('input', 'cutscene-search');
    search.type = 'search';
    search.placeholder = 'Event row number, site, mission or scene name';
    search.value = view.launchQuery || '';
    search.setAttribute('data-cutscene-focus-key', 'launch-search');
    search.addEventListener('input', function() { view.launchQuery = search.value; rerender(rom, state); });
    assign.appendChild(search);
    var results = events.searchSites(state.catalog, view.launchQuery || '', 30);
    if (!results.length) assign.appendChild(node('p', '', 'No launch site matches.'));
    results.forEach(function(site) {
      var target = siteCurrentTarget(state, site);
      var row = node('div', 'cutscene-launch-row');
      row.appendChild(node('span', 'cutscene-launch-site', siteLabel(site)));
      var context = (site.mission || site.role || 'Event program') + ' · launches ' + target.name +
        (target.binding ? ' (assigned)' : '');
      row.appendChild(node('span', 'cutscene-launch-context', context));
      if (site.shared) {
        var shared = node('span', 'cutscene-launch-shared', 'Shared program');
        shared.title = 'This event program is shared: ' + site.role + '. Every mission that runs it is affected.';
        row.appendChild(shared);
      }
      var targetsThis = target.binding
        ? events.bindingsForScene(state.catalog, [target.binding], scene).length > 0
        : !scene.authored && site.retailSceneId === scene.sceneId;
      var control = button(targetsThis ? 'Launches this scene' : 'Assign', 'btn-secondary', function() {
        act(function() { assignLaunchSite(rom, state, scene, site.launchSiteId); });
      });
      control.setAttribute('data-cutscene-focus-key', 'launch-assign:' + site.launchSiteId);
      if (site.form !== 'long') {
        control.title = 'Short launch form: export redirects it through a small stub added to the event program.';
      }
      if (targetsThis) control.disabled = true;
      row.appendChild(control);
      assign.appendChild(row);
    });
    panel.appendChild(assign);
    drawer.appendChild(panel);
  }

  // Wipe: replace a retail scene's stream with its valid empty form (its own
  // leading records, camera pair for classes 4 and 5, and parsed native exit),
  // then edit it like a new scene. Everything that shares the stream plays it.
  var WIPED_BLANKS = {
    actors: [], actorCount: 0, recoveredActorEvents: [], recoveredMediaRequests: [],
    recoveredNativeSpriteEffects: [], dialogueAssociations: [], audioAssociations: [],
    reviewedTimelineOverlay: null
  };

  // Everything that plays this stream now: its selector rows and its
  // effective launch sites (retail sites not rebound away, plus sites bound
  // to it), each once.
  function wipeSharers(state, scene) {
    var rows = (scene.source.directorSelectorRows || []).slice();
    var events = OB64.cutsceneEvents;
    if (!events) return { rows: rows, sites: [] };
    var reboundAway = {};
    (state.eventBindings || []).forEach(function(binding) { reboundAway[binding.launchSiteId] = true; });
    var seen = {}, sites = [];
    events.sitesForScene(state.catalog, scene).filter(function(site) { return !reboundAway[site.launchSiteId]; })
      .concat(events.bindingsForScene(state.catalog, state.eventBindings, scene).map(function(binding) {
        return events.site(state.catalog, binding.launchSiteId);
      })).forEach(function(site) {
        if (site && !seen[site.launchSiteId]) { seen[site.launchSiteId] = true; sites.push(site); }
      });
    return { rows: rows, sites: sites };
  }

  // Build the wiped scene without changing anything yet. `info.pending` marks
  // a wipe this session (or Project) adds; a wipe read from the ROM is not.
  function prepareWipe(rom, state, scene, pending) {
    if (!OB64.cutsceneAuthored || scene.authored || scene.engine !== 'director') {
      return Promise.reject(new UiError('Only original Director scenes can be wiped.'));
    }
    if (scene.wiped) return Promise.reject(new UiError('This scene is already wiped.'));
    return ensureProjectedDocument(rom, state, scene).then(function() {
      var source = state.sourceByAssetId[scene.assetId];
      if (!source) throw new UiError('The scene source is not loaded.');
      var result = OB64.cutsceneAuthored.wipeStream(source.decodedBytes, source.nodeDefinitions);
      if (result.status !== 'ok') throw new UiError(result.reason);
      var info = {
        pending: pending !== false, exitKind: result.exitKind, sceneClass: result.sceneClass,
        setupWordCount: result.setupWordCount, exitStartWord: result.exitStartWord,
        keptCameras: result.keptCameras, captureFlagRemoved: result.captureFlagRemoved,
        romDecodedSha256: source.decodedSha256,
        actorRecipe: OB64.cutsceneAuthored.wipedActorRecipe(source.decodedBytes, source.nodeDefinitions)
      };
      var flagged = Object.assign({}, scene, WIPED_BLANKS, { wiped: info });
      var adapted = OB64.cutsceneCodec.sceneForBytes(flagged, result.bytes);
      return OB64.cutsceneCodec.authoredSceneSource(adapted, result.bytes, { rom: rom.z64 }).then(function(wipedSource) {
        var document = OB64.cutsceneCodec.projectSceneDocument(adapted, wipedSource, state.catalog).document;
        return { scene: scene, adapted: adapted, source: wipedSource, document: document, info: info,
          romScene: Object.assign({}, scene) };
      });
    });
  }

  function applyWipe(rom, state, prepared) {
    var scene = prepared.scene;
    var backup = { source: scene.source };
    Object.keys(WIPED_BLANKS).forEach(function(key) { backup[key] = scene[key]; });
    Object.assign(scene, WIPED_BLANKS, { source: prepared.adapted.source, wiped: prepared.info,
      wipeBackup: { fields: backup, romScene: prepared.romScene } });
    state.sourceByAssetId[scene.assetId] = prepared.source;
    state.programByAssetId = state.programByAssetId || {};
    state.programByAssetId[scene.assetId] = OB64.cutsceneCodec.createIr(
      scene, prepared.source.decodedBytes, prepared.source.nodeDefinitions).program;
    state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(prepared.document, 0);
    state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(prepared.document, 200);
    delete state.runtimeByAssetId[scene.assetId];
    state.concurrentRuntimeByLaunchContext = {};
    var view = viewFor(state, scene.sceneId);
    view.frame = 0; view.selectedClipId = null; view.selectedActorId = null; view.wipeConfirm = false;
    return scene;
  }

  function wipeScene(rom, state, scene) {
    return prepareWipe(rom, state, scene, true).then(function(prepared) {
      applyWipe(rom, state, prepared);
      notifyChange(state, 'Wiped ' + OB64.cutsceneCatalog.displayName(scene));
      var history = historyFor(state, scene);
      if (history) refreshRuntime(state, scene, history.present, rom);
      if (state.ui) rerender(rom, state);
      return scene;
    });
  }

  // ROM-only reopen: scenes the manifest names as wiped get the wiped state
  // back from their current stream, which must still have the recorded exit.
  function discoverWipes(rom, state) {
    var arenaApi = OB64.cutsceneExport && OB64.cutsceneExport.arena;
    if (!arenaApi || !OB64.cutsceneAuthored) return Promise.resolve();
    var arena = arenaApi.read(rom.z64);
    var manifestRow = arena.records.find(function(row) { return row[0] >>> 28 === arenaApi.KIND.MANIFEST; });
    var listed = manifestRow ? arenaApi.readManifest(rom.z64, manifestRow).wipedScenes || [] : [];
    var chain = Promise.resolve();
    listed.forEach(function(entry) {
      chain = chain.then(function() {
        var scene = state.catalog.getScene(entry.sceneId);
        if (!scene || scene.authored || scene.engine !== 'director') {
          throw new UiError('The Cutscene manifest names a wiped scene that is not an original Director scene.');
        }
        return ensureProjectedDocument(rom, state, scene).then(function() {
          var source = state.sourceByAssetId[scene.assetId];
          var result = OB64.cutsceneAuthored.wipeStream(source.decodedBytes, source.nodeDefinitions);
          if (result.status !== 'ok' || result.exitKind !== entry.exitKind) {
            throw new UiError('Wiped scene ' + entry.sceneId + ' no longer has its recorded native exit.');
          }
          var info = {
            pending: false, exitKind: result.exitKind, sceneClass: result.sceneClass,
            setupWordCount: result.setupWordCount, exitStartWord: result.sourceExitStartWord,
            keptCameras: result.keptCameras, captureFlagRemoved: result.captureFlagRemoved,
            romDecodedSha256: source.decodedSha256,
            // The manifest keeps the recipe from the original cast; a
            // manifest written before that is derived from the stream.
            actorRecipe: entry.actorRecipe ? Object.assign({}, entry.actorRecipe)
              : OB64.cutsceneAuthored.wipedActorRecipe(source.decodedBytes, source.nodeDefinitions)
          };
          var adapted = OB64.cutsceneCodec.sceneForBytes(Object.assign({}, scene, WIPED_BLANKS, { wiped: info }),
            source.decodedBytes);
          return OB64.cutsceneCodec.authoredSceneSource(adapted, source.decodedBytes, { rom: rom.z64 }).then(function(wipedSource) {
            var document = OB64.cutsceneCodec.projectSceneDocument(adapted, wipedSource, state.catalog).document;
            applyWipe(rom, state, { scene: scene, adapted: adapted, source: wipedSource, document: document,
              info: info, romScene: Object.assign({}, scene) });
          });
        });
      });
    });
    return chain;
  }

  // Undo a wipe made this session. A wipe already in the loaded ROM is undone
  // by loading the original ROM.
  function restoreWipedScene(rom, state, scene) {
    if (!scene.wiped || !scene.wiped.pending || !scene.wipeBackup) {
      throw new UiError('Only a wipe made in this session can be restored here.');
    }
    Object.assign(scene, scene.wipeBackup.fields);
    delete scene.wiped;
    delete scene.wipeBackup;
    delete state.sourceByAssetId[scene.assetId];
    if (state.programByAssetId) delete state.programByAssetId[scene.assetId];
    delete state.originalSerialized[scene.storageId];
    delete state.histories[scene.storageId];
    delete state.runtimeByAssetId[scene.assetId];
    state.concurrentRuntimeByLaunchContext = {};
    notifyChange(state, 'Restored ' + OB64.cutsceneCatalog.displayName(scene));
    return loadScene(rom, state, scene).then(function() {
      if (state.ui) rerender(rom, state);
      return scene;
    });
  }

  function renderWipeControls(header, rom, state, scene, view) {
    var line = node('div', 'cutscene-creator-provenance cutscene-wipe-line');
    line.setAttribute('data-cutscene-focus-key', 'scene-wipe');
    function report(error) {
      if (state.callbacks.onStatus) state.callbacks.onStatus(error && error.message || String(error));
    }
    if (scene.wiped) {
      line.appendChild(node('span', '', 'Wiped · empty scene with its original setup and ' +
        (scene.wiped.exitKind === 'presentation-handoff' ? 'presentation handoff' : 'Director release') +
        '. Its Actors and actions go before that exit.'));
      if (scene.wiped.pending) {
        var restore = button('Restore original', 'btn-secondary', function() {
          try { restoreWipedScene(rom, state, scene).catch(report); } catch (error) { report(error); }
        });
        restore.setAttribute('data-cutscene-focus-key', 'scene-wipe-restore');
        line.appendChild(restore);
      }
      header.appendChild(line);
      return;
    }
    if (!view.wipeConfirm) {
      var start = button('Wipe scene…', 'btn-secondary', function() { view.wipeConfirm = true; rerender(rom, state); });
      start.setAttribute('data-cutscene-focus-key', 'scene-wipe-start');
      start.title = 'Remove every Actor, line and action, keeping only what the scene needs to start and end';
      line.appendChild(start);
      header.appendChild(line);
      return;
    }
    var sharers = wipeSharers(state, scene);
    var text = 'Wipe “' + OB64.cutsceneCatalog.displayName(scene) + '”? Every Actor, line and action is removed; ' +
      'the scene keeps only its setup records and native exit. Unsaved edits to it are discarded. ' +
      'Every place that plays this stream plays the empty scene: Director selector row' +
      (sharers.rows.length === 1 ? ' ' : 's ') + sharers.rows.join(', ') +
      (sharers.sites.length ? '; ' + sharers.sites.length + ' event launch site' + (sharers.sites.length === 1 ? '' : 's') +
        ' (' + sharers.sites.slice(0, 8).map(function(site) { return siteLabel(site); }).join('; ') +
        (sharers.sites.length > 8 ? '; …' : '') + ')' : '') + '.';
    var confirmText = node('span', 'cutscene-wipe-confirm-text', text);
    confirmText.setAttribute('data-cutscene-focus-key', 'scene-wipe-confirm-text');
    line.appendChild(confirmText);
    var confirm = button('Wipe scene', 'btn-secondary', function() {
      wipeScene(rom, state, scene).catch(function(error) {
        view.wipeConfirm = false;
        report(error);
        if (state.ui) rerender(rom, state);
      });
    });
    confirm.setAttribute('data-cutscene-focus-key', 'scene-wipe-confirm');
    var cancel = button('Cancel', 'btn-secondary', function() { view.wipeConfirm = false; rerender(rom, state); });
    cancel.setAttribute('data-cutscene-focus-key', 'scene-wipe-cancel');
    line.appendChild(confirm);
    line.appendChild(cancel);
    header.appendChild(line);
  }

  // A genuinely new scene: random identity, the first free selector row that is
  // zero in the loaded ROM and unclaimed, and the validated class-5 scaffold.
  function createAuthoredScene(rom, state, name) {
    return Promise.resolve(state.authoredReady).then(function() {
      if (state.authoredDiscovery && state.authoredDiscovery.status === 'failed') {
        throw new UiError('New scenes are unavailable: ' + state.authoredDiscovery.message);
      }
      var others = state.catalog.authoredScenes().map(function(item) { return item.authored; });
      var free = OB64.cutsceneAuthored.freeSelectorRows(state.z64, state.catalog, others);
      if (!free.length) throw new UiError('Every free Director selector row is already used by a scene.');
      return OB64.cutsceneAuthored.create(state.z64, state.catalog, {
        name: typeof name === 'string' && name.trim() ? name.trim() : 'New scene ' + (others.length + 1),
        selectorRow: free[0], otherAuthored: others
      });
    }).then(function(made) {
      state.catalog.registerAuthoredScene(made.scene);
      if (state.runtimeController) state.runtimeController.abort();
      state.openRequest++;
      pauseAnimation(state);
      state.selectedSceneId = made.scene.sceneId;
      viewFor(state, made.scene.sceneId).creatorDrawer = null;
      notifyChange(state, 'Created ' + made.scene.authored.name);
      if (state.ui) rerender(rom, state);
      return made.scene;
    });
  }

  // Delete a new scene. One made this session simply goes; one the loaded ROM
  // holds is recorded so export releases its arena record and zeroes its
  // selector row. Launch sites assigned to it return to their retail scenes.
  function deleteAuthoredScene(rom, state, scene) {
    if (!scene || !scene.authored) throw new UiError('Only new scenes can be deleted.');
    var bound = OB64.cutsceneEvents
      ? OB64.cutsceneEvents.bindingsForScene(state.catalog, state.eventBindings, scene) : [];
    var bindingsBefore = state.eventBindings || [];
    if (bound.length) {
      state.eventBindings = bindingsBefore.filter(function(binding) { return bound.indexOf(binding) < 0; });
    }
    if (scene.authored.romState) {
      state.deletedAuthored = (state.deletedAuthored || []).concat([{
        authoredId: scene.authored.authoredId, selectorRow: scene.authored.selectorRow }]);
    }
    state.catalog.unregisterAuthoredScene(scene);
    delete state.histories[scene.storageId];
    delete state.sourceByAssetId[scene.assetId];
    delete state.originalSerialized[scene.storageId];
    delete state.runtimeByAssetId[scene.assetId];
    if (state.programByAssetId) delete state.programByAssetId[scene.assetId];
    delete state.views[scene.sceneId];
    state.concurrentRuntimeByLaunchContext = {};
    if (state.selectedSceneId === scene.sceneId) {
      var next = state.catalog.directorScenes[0];
      state.selectedSceneId = next ? next.sceneId : null;
    }
    // Its released sites change the predecessors of later scenes too.
    if (bound.length && OB64.cutsceneEvents) invalidateAfterBindingChange(rom, state, bindingsBefore);
    notifyChange(state, 'Deleted ' + scene.authored.name +
      (bound.length ? '; ' + bound.length + ' launch site' + (bound.length === 1 ? '' : 's') + ' launch their retail scenes again' : ''));
    if (state.ui) rerender(rom, state);
    return bound.length;
  }

  function renameAuthoredScene(rom, state, scene, value) {
    var name = String(value || '').trim();
    if (!scene.authored || !name || name === scene.authored.name) return;
    scene.authored.name = name;
    scene.friendlyName = name;
    scene.technicalName = name;
    // Keep the document title equal to the persisted name in every undo and
    // redo state too, so undoing another edit never brings the old title back.
    var history = state.histories[scene.storageId];
    if (history) {
      [history.present].concat(history.past.map(function(entry) { return entry.before; }),
        history.past.map(function(entry) { return entry.after; }),
        history.future.map(function(entry) { return entry.before; }),
        history.future.map(function(entry) { return entry.after; })).forEach(function(document) {
        if (document && document.identity) document.identity.friendlyName = name;
      });
    }
    notifyChange(state, 'Renamed scene to ' + name);
    if (state.ui) rerender(rom, state);
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
    var boundary = boundaryForFrame(scene, document, view.frame, state);
    var id = 'actor:authored:slot:' + String(slot).padStart(2, '0');
    var suffix = 1;
    while (document.actors.some(function(actor) { return actor.id === id; })) {
      id = 'actor:authored:slot:' + String(slot).padStart(2, '0') + ':' + suffix++;
    }
    var preview = sourceActor && OB64.cutscenePreview.evaluateAtFrame(document, view.frame,
      { pathId: view.pathId }).actors.find(function(actor) { return actor.id === sourceActor.id; });
    var defaultArt = state.catalog.actorArtSources[0];
    // New and wiped scenes start Actors from the frozen visible recipe.
    var recipe = !sourceActor && (scene.authored && scene.authored.actorRecipe ||
      scene.wiped && scene.wiped.actorRecipe) || null;
    var artSourceId = sourceActor && sourceActor.artSourceId ||
      (recipe ? 'cutscene-art-bank:' + recipe.bank : defaultArt && defaultArt.assetId) || null;
    // New Actors in an authored scene start from the template's visible recipe,
    // stepping left across the Stage so they do not stack on one spot.
    var spacing = recipe ? -35 * document.actors.filter(function(actor) {
      return actor.source && actor.source.authored;
    }).length : 0;
    var initial = preview ? {
      visible: true, x: nativeCoordinate(preview.x), y: nativeCoordinate(preview.y), z: nativeCoordinate(preview.z),
      facing: preview.facing, poseId: preview.poseId
    } : recipe ? {
      visible: true, x: recipe.x + spacing, y: recipe.y, z: recipe.z,
      facing: 'native-' + recipe.nativeFacing, poseId: null
    } : { visible: true, x: 0, y: 0, z: 0, facing: 'native-0', poseId: null };
    view.selectedActorId = id;
    view.selectedClipId = null;
    view.editPanel = 'actors';
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
            animationKey: sourceActor && sourceActor.source.animationKey || (recipe ? recipe.animationKey : 0),
            variantSelector: sourceActor && sourceActor.source.variantSelector || (recipe ? recipe.variantSelector : 0),
            renderMode: sourceActor && Number.isInteger(sourceActor.source.renderMode)
              ? sourceActor.source.renderMode : (recipe ? recipe.renderMode : 0),
            renderModeStatus:
              'exact opcode-0x14 actor render-mode byte; authored actors default to native mode 0'
          }
        }));
        // Authored scenes give the new Actor the channel and scale their cast
        // uses. A duplicate takes the source Actor's live channel and scale from
        // playback, which also covers Actors whose 0x1C/0x1D are native records.
        var livePresentation = sourceActor && (scene.authored || scene.wiped) && preview &&
          Number.isInteger(preview.transformChannel) && Number.isFinite(preview.uniformScale) ? {
            transformChannel: preview.transformChannel,
            uniformScalePercent: Math.round(preview.uniformScale * 100)
          } : null;
        var presentation = sourceActor && sourceActor.source.presentation || livePresentation || (recipe ? {
          transformChannel: recipe.transformChannel,
          uniformScalePercent: recipe.uniformScalePercent
        } : null);
        if (presentation) next.actors[next.actors.length - 1].source.presentation = presentation;
      });
  }

  function readableNativeDialogue(raw) {
    var value = String(raw || '')
      .replace(/\{T\d+\}/g, ' [game marker] ')
      .replace(/\{[^}]*\}/g, '')
      .replace(/@(?:%|!|z|:)./g, ' [dynamic text] ')
      .replace(/@&../g, ' [dynamic text] ')
      .replace(/@w.{4}/g, '')
      .replace(/@(?:T|t|f|F|e|v|y|<)\d{3}/g, '')
      .replace(/@(?:l|S)./g, '')
      .replace(/@n/g, '\n')
      .replace(/@(?:p|c|s|a|i|\/)/g, '\n\n')
      .replace(/@./g, '')
      .replace(/\r/g, '')
      .replace(/[ \t]*\n[ \t]*/g, '\n')
      .replace(/\n{3,}/g, '\n\n')
      .replace(/[ \t]{2,}/g, ' ')
      .trim();
    return value || 'No readable dialogue text';
  }

  function dialogueCardText(clip) {
    var payload = clip.payload || {};
    var value = payload.nativeDialogueEditable && typeof payload.rawText === 'string'
      ? readableNativeDialogue(payload.rawText)
      : String(payload.text || payload.rawText || 'Text unavailable');
    value = value.replace(/\n{2,}/g, ' · ').replace(/\s+/g, ' ').trim();
    if (value.length <= 105) return value;
    var end = value.lastIndexOf(' ', 105);
    return value.slice(0, end > 70 ? end : 105).trimEnd() + '…';
  }

  function renderEditClipList(host, rom, state, scene, document, dialogueOnly) {
    var view = viewFor(state, scene.sceneId);
    var rows = orderedClips(document).filter(function(row) {
      return (row.clip.kind === 'dialogue') === dialogueOnly &&
        OB64.cutscenePreview.appliesToPath(row.clip, view.pathId);
    });
    var list = node('div', 'cutscene-edit-list');
    if (!rows.length) list.appendChild(node('p', 'cutscene-empty-note', dialogueOnly
      ? 'No dialogue boxes are timed in this scene.'
      : 'No actions are mapped to this preview path.'));
    rows.forEach(function(row) {
      var item = button('', 'cutscene-edit-row', function() {
        selectClip(rom, state, scene, row);
      });
      item.classList.toggle('active', row.clip.id === view.selectedClipId);
      item.setAttribute('aria-pressed', row.clip.id === view.selectedClipId ? 'true' : 'false');
      item.setAttribute('data-cutscene-focus-key', 'edit-clip:' + row.clip.id);
      item.appendChild(node('strong', '', dialogueOnly
        ? (row.clip.payload.speaker || 'Dialogue box')
        : clipLabel(row, document)));
      item.appendChild(node('span', 'cutscene-edit-row-time',
        OB64.cutscenePreview.formatTime(row.clip.startFrame)));
      if (dialogueOnly) {
        item.appendChild(node('span', 'cutscene-edit-row-preview',
          dialogueCardText(row.clip)));
      }
      item.appendChild(capabilityBadge(row.clip.capability, 'ROM'));
      list.appendChild(item);
    });
    host.appendChild(list);
    return rows;
  }

  function renderStoredDialogueReferences(host, state, capturedStages) {
    var untimedDialogueCaptures = capturedStages.filter(function(capture) {
      return capture.dialogueAssociation;
    });
    if (!untimedDialogueCaptures.length) return;
    host.appendChild(node('h3', '', 'Stored dialogue references'));
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
      host.appendChild(dialogue);
    });
  }

  function renderSceneSettings(inspector, rom, state, scene, document, capturedStages) {
    inspector.appendChild(node('h3', '', 'Scene settings'));
    inspector.appendChild(field('Project scene name',textInput(document.identity.friendlyName||OB64.cutsceneCatalog.displayName(scene),function(value){
      executeEdit(rom,state,'Name authored scene',function(next){next.identity.friendlyName=value||null;});
    },'scene-project-name'),'Export replaces '+OB64.cutsceneCatalog.displayName(scene)+' at its existing game triggers. This name stays in the Project.'));
    inspector.appendChild(button('Create replacement from this template', 'btn-secondary',
      function() { createFromTemplate(rom, state); }));
    inspector.appendChild(node('p', 'cutscene-field-hint',
      'The replacement uses this scene’s game triggers. The Project name does not add a new trigger.'));
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
    if (capturedStages.length) {
      sourceRows.push(['Captured stages', capturedStages.map(function(capture) {
        return capture.stageLabel;
      }).join(' · ')]);
    }
    sourceRows.forEach(function(row) {
      sourceSummary.appendChild(node('dt', '', row[0])); sourceSummary.appendChild(node('dd', '', row[1]));
    });
    var sourceDetails = node('details', 'cutscene-source-details');
    sourceDetails.appendChild(node('summary', '', 'Source and launch facts'));
    sourceDetails.appendChild(sourceSummary);
    inspector.appendChild(sourceDetails);

    var eventContextChoices = launchContextChoices(state, scene);
    if (scene.engine === 'director') {
      inspector.appendChild(node('h3', '', 'Playback inputs'));
      var snapshot = state.nativeLaunchInputsByAssetId && state.nativeLaunchInputsByAssetId[scene.assetId];
      inspector.appendChild(node('p', 'cutscene-field-hint', snapshot
        ? 'Using ' + snapshot.sourceIdentity + ' · invocation ' + snapshot.invocationId + ' · ' + snapshot.evidenceGrade
        : 'No playback inputs supplied. Existing Actors and external producer state remain unknown.'));
      var snapshotFile = node('input', 'cutscene-background-select');
      snapshotFile.type = 'file'; snapshotFile.accept = '.json,application/json';
      snapshotFile.setAttribute('data-cutscene-focus-key', 'actor-launch-snapshot');
      snapshotFile.addEventListener('change', async function() {
        var file = snapshotFile.files && snapshotFile.files[0];
        if (!file) return;
        var epoch = state.projectionEpoch || 0;
        var importRequest = state.launchImportRequest = (state.launchImportRequest || 0) + 1;
        try {
          if (file.size > 131072) throw new Error('Launch snapshot exceeds 128 KiB.');
          var input = JSON.parse(await file.text());
          if (importRequest !== state.launchImportRequest || epoch !== (state.projectionEpoch || 0) || state.selectedSceneId !== scene.sceneId) return;
          setNativeLaunchInputs(state, scene, input);
          await loadScene(rom, state, scene);
          if (state.selectedSceneId === scene.sceneId) {
            var view = viewFor(state, scene.sceneId);
            view.frame = Math.min(view.frame, Math.max(0, OB64.cutscenePreview.sceneDurationFrames(document, view.pathId) - 1));
            rerender(rom, state);
          }
        } catch (error) {
          if (state.callbacks.onStatus) state.callbacks.onStatus(error.message);
        }
      });
      inspector.appendChild(field('Import playback inputs', snapshotFile,
        'Loads a repeatable snapshot for this resource in this session. Project load clears the snapshot.'));
      if (snapshot) inspector.appendChild(button('Clear playback inputs', 'btn-secondary', function() {
        setNativeLaunchInputs(state, scene, null);
        loadScene(rom, state, scene).then(function() {
          if (state.selectedSceneId === scene.sceneId) {
            var view = viewFor(state, scene.sceneId);
            view.frame = Math.min(view.frame, Math.max(0, OB64.cutscenePreview.sceneDurationFrames(document, view.pathId) - 1));
            rerender(rom, state);
          }
        });
      }));
    }
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
        if (state.nativeLaunchInputsByAssetId) delete state.nativeLaunchInputsByAssetId[scene.assetId];
        state.launchImportRequest = (state.launchImportRequest || 0) + 1;
        var view = viewFor(state, scene.sceneId);
        view.launchContextId = selectedId;
        var selectedChoice = launchContextChoice(state, scene, selectedId);
        if (state.callbacks.onStatus) {
          state.callbacks.onStatus('Loading ' + selectedChoice.label + '…');
        }
        if (state.runtimeController) state.runtimeController.abort();
        loadScene(rom, state, scene).then(function() {
            if (view.launchContextId !== selectedId || state.selectedSceneId !== scene.sceneId) return;
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
        'This selection changes launch-time constants and concurrent shared scene state. It clears the Actor snapshot for the previous invocation.'));
      if (importedFreshLaunch(state, scene)) {
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'Imported fresh launch uses its own initial scene state. Clear playback inputs to use this event invocation.'));
      } else if (activeEventContext.contextScene) {
        inspector.appendChild(node('p', 'cutscene-field-hint',
          'Concurrent state source: ' +
          OB64.cutsceneCatalog.displayName(activeEventContext.contextScene) +
          ' · offset ' + activeEventContext.context.concurrentDirectorTickOffset +
          ' native update' +
          (activeEventContext.context.concurrentDirectorTickOffset === 1 ? '' : 's') + '.'));
      }
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
        var option = node('option', '', nativeBackgroundGroupLabel(state, backgroundProjection, group));
        option.value = String(group.selector);
        option.title = group.associationStatus;
        nativeGroupSelect.appendChild(option);
      });
      nativeGroupSelect.value = document.background.capability === 'native' &&
          Number.isInteger(backgroundProjection.selectedSelector)
        ? String(backgroundProjection.selectedSelector) : '';
      nativeGroupSelect.addEventListener('change', function() {
        if (nativeGroupSelect.value === '') return;
        try { selectNativeBackground(rom, state, scene, Number(nativeGroupSelect.value)); } catch (error) {
          if (state.callbacks.onStatus) state.callbacks.onStatus(error.message);
        }
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

    return backgroundStatus;
  }

  function renderInspector(shell, rom, state, scene, document) {
    var view = viewFor(state, scene.sceneId);
    var capturedStages = Array.isArray(document.identity.captures)
      ? document.identity.captures.filter(function(capture) {
        return capture.stageLabel;
      }) : [];
    var inspector = node('section', 'cutscene-settings-pane');
    var backgroundStatus = view.editPanel === 'scene'
      ? renderSceneSettings(inspector, rom, state, scene, document, capturedStages)
      : null;
    var sceneSettings = inspector;
    var storedDialoguePanel = node('div', 'cutscene-stored-dialogue');
    if (view.editPanel === 'dialogue') {
      renderStoredDialogueReferences(storedDialoguePanel, state, capturedStages);
    }
    var selectedRow = findClipRow(document, view.selectedClipId);
    if (['actors', 'actions', 'dialogue', 'scene'].indexOf(view.editPanel) === -1) {
      view.editPanel = 'actors';
    }
    if (!view.selectedActorId && document.actors.length) {
      view.selectedActorId = document.actors[0].id;
    }
    var actorPane = null;
    if (view.editPanel === 'actors') {
      actorPane = node('section', 'cutscene-workspace-pane');
      inspector = actorPane;
      inspector.appendChild(node('h3', '', 'Actors and placement'));
      inspector.appendChild(node('p', 'cutscene-pane-intro',
        'Choose an Actor to set its starting position and appearance. Use the playhead for timed changes.'));
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
        ' Actors · ' + (20 - Object.keys(occupiedActorSlots).length) +
        ' free observed slots. Appear and disappear actions use the current playhead.'));
      var cast = node('div', 'cutscene-cast');
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
      if (actor) {
        renderActorInspector(inspector, rom, state, actor);
        var actorSteps = orderedClips(document).filter(function(row) {
          return row.track.actorId === actor.id &&
            OB64.cutscenePreview.appliesToPath(row.clip, view.pathId);
        });
        inspector.appendChild(node('h4', '', 'Timed Actor actions'));
        var actorStepList = node('div', 'cutscene-edit-list cutscene-actor-steps');
        if (!actorSteps.length) actorStepList.appendChild(node('p', 'cutscene-empty-note',
          'No timed actions for this Actor on the selected preview path.'));
        actorSteps.forEach(function(row) {
          var step = button('', 'cutscene-edit-row', function() {
            selectClip(rom, state, scene, row);
          });
          step.appendChild(node('strong', '', clipLabel(row, document)));
          step.appendChild(node('span', 'cutscene-edit-row-time',
            OB64.cutscenePreview.formatTime(row.clip.startFrame)));
          actorStepList.appendChild(step);
        });
        inspector.appendChild(actorStepList);
      }
    }

    var actionPane = null;
    if (view.editPanel === 'actions') {
      actionPane = node('section', 'cutscene-workspace-pane');
      actionPane.appendChild(node('h3', '', 'Movement and timing'));
      actionPane.appendChild(node('p', 'cutscene-pane-intro',
        'Select an action to change when it starts, how long it lasts, or where an Actor moves.'));
      renderClipInspector(actionPane, rom, state, scene, document,
        selectedRow && selectedRow.clip.kind !== 'dialogue' ? selectedRow : null,
        'Edit selected action');
      actionPane.appendChild(node('h4', '', 'Scene actions'));
      renderEditClipList(actionPane, rom, state, scene, document, false);
    }

    var dialoguePane = null;
    if (view.editPanel === 'dialogue') {
      dialoguePane = node('section', 'cutscene-workspace-pane');
      dialoguePane.appendChild(node('h3', '', 'Dialogue boxes'));
      dialoguePane.appendChild(node('p', 'cutscene-pane-intro',
        'Select a box to edit its text and timing. New native boxes get their own text entry; editing an existing native entry changes every use of that entry.'));
      dialoguePane.appendChild(button('Add dialogue box', 'btn-secondary', function() {
        view.editPanel = 'dialogue'; addDialogue(rom, state);
      }));
      renderClipInspector(dialoguePane, rom, state, scene, document,
        selectedRow && selectedRow.clip.kind === 'dialogue' ? selectedRow : null,
        'Edit selected dialogue box');
      dialoguePane.appendChild(node('h4', '', 'Timed dialogue boxes'));
      renderEditClipList(dialoguePane, rom, state, scene, document, true);
      if (storedDialoguePanel.firstChild) dialoguePane.appendChild(storedDialoguePanel);
    }

    var sidebar = node('aside', 'cutscene-inspector');
    sidebar.setAttribute('data-cutscene-scroll', 'inspector');
    sidebar.appendChild(node('div', 'cutscene-workspace-kicker', 'EDIT THIS SCENE'));
    var navigation = node('nav', 'cutscene-workspace-tabs');
    navigation.setAttribute('aria-label', 'Cutscene editing tools');
    var panels = { actors: actorPane, actions: actionPane,
      dialogue: dialoguePane, scene: sceneSettings };
    [['actors', 'Actors'], ['actions', 'Movement'],
      ['dialogue', 'Dialogue'], ['scene', 'Scene']].forEach(function(entry) {
      var tab = button(entry[1], 'cutscene-workspace-tab', function() {
        view.editPanel = entry[0];
        if (state.ui) state.ui.resetInspectorScroll = true;
        rerender(rom, state);
      });
      tab.classList.toggle('active', view.editPanel === entry[0]);
      tab.setAttribute('aria-pressed', view.editPanel === entry[0] ? 'true' : 'false');
      tab.setAttribute('data-cutscene-focus-key', 'edit-panel:' + entry[0]);
      navigation.appendChild(tab);
    });
    sidebar.appendChild(navigation);
    sidebar.appendChild(panels[view.editPanel]);
    shell.appendChild(sidebar);
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
    inspector.appendChild(node('h4', '', 'Starting appearance'));
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

    inspector.appendChild(node('h4', '', 'Pose at playhead'));
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
    if (actor.source && actor.source.authored === true) {
      var presentationNow = actor.source.presentation || {};
      inspector.appendChild(field('Size (%)', numericInput(
        Number.isInteger(presentationNow.uniformScalePercent) ? presentationNow.uniformScalePercent : 100, '1',
        function(value) {
          var percent = Math.max(1, Math.min(400, Math.round(Number(value) || 100)));
          executeEdit(rom, state, 'Change actor size', function(document) {
            var next = document.actors.find(function(candidate) { return candidate.id === actor.id; });
            next.source.presentation = Object.assign({}, next.source.presentation || {}, { uniformScalePercent: percent });
          });
        }, 'actor-size:' + actor.id),
        'Native uniform scale (0x1D), 1 to 400 percent. The scene camera decides how large 100% looks.'));
    }
    inspector.appendChild(node('h4', '', 'Starting position'));
    var coordinates = node('div', 'cutscene-coordinate-grid');
    ['x', 'y', 'z'].forEach(function(axis) {
      coordinates.appendChild(field(axis.toUpperCase(), numericInput(actor.initial[axis], '0.001',
        function(value) {
          executeEdit(rom, state, 'Move actor ' + axis.toUpperCase(), function(document) {
            document.actors.find(function(candidate) { return candidate.id === actor.id; }).initial[axis] = nativeCoordinate(value);
          });
        }, 'actor-' + axis + ':' + actor.id)));
    });
    inspector.appendChild(coordinates);
    inspector.appendChild(node('p', 'cutscene-field-hint',
      'These coordinates compile into the actor Place command when that exact boundary exists. Stage dragging adds or edits a timed pose.'));
    inspector.appendChild(node('h4', '', 'Appearance timing'));
    var actions = node('div', 'cutscene-inspector-actions');
    actions.appendChild(button('Set pose now', 'btn-secondary', function() {
      viewFor(state, selectedScene(state).sceneId).selectedActorId = actor.id;
      addPose(rom, state);
    }));
    actions.appendChild(button('Appear at playhead', 'btn-secondary', function() {
      viewFor(state, selectedScene(state).sceneId).selectedActorId = actor.id; addEnter(rom, state);
    }));
    actions.appendChild(button('Disappear at playhead', 'btn-secondary', function() {
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

  function creatorDetails(view, key, label) {
    var details = node('details', 'cutscene-creator-details');
    var open = view.creatorOpenPanels || (view.creatorOpenPanels = {});
    details.open = open[key] === true;
    var summary = node('summary', '', label);
    summary.setAttribute('data-cutscene-focus-key', 'creator-details:' + key);
    details.appendChild(summary);
    details.addEventListener('toggle', function() { open[key] = details.open; });
    return details;
  }

  function creatorActionRows(document, view) {
    return orderedClips(document).filter(function(row) {
      return ['pose', 'movement', 'dialogue', 'wait', 'camera', 'effect', 'fade',
        'enter', 'exit', 'opacity', 'audio'].indexOf(row.clip.kind) !== -1 &&
        OB64.cutscenePreview.appliesToPath(row.clip, view.pathId);
    });
  }

  var FADE_PRESETS = {
    'to-black': { label: 'Fade to black', red: 0, green: 0, blue: 0, startAlpha: null, targetAlpha: 255 },
    'from-black': { label: 'Fade in from black', red: 0, green: 0, blue: 0, startAlpha: 255, targetAlpha: 0 },
    'to-white': { label: 'Fade to white', red: 255, green: 255, blue: 255, startAlpha: null, targetAlpha: 255 },
    'from-white': { label: 'Fade in from white', red: 255, green: 255, blue: 255, startAlpha: 255, targetAlpha: 0 }
  };

  function fadeLabel(clip) {
    var preset = FADE_PRESETS[clip.payload.direction];
    return preset ? preset.label : 'Fade';
  }

  function creatorActionLabel(row, document) {
    var actor = document.actors.find(function(candidate) { return candidate.id === row.track.actorId; });
    var name = actor ? actor.label : 'Actor';
    var labels = { movement: name + ' moves', pose: name + ' changes pose',
      enter: name + ' appears', exit: name + ' disappears', opacity: name + ' changes opacity',
      wait: 'Wait', camera: 'Camera pan', effect: 'Show an effect', audio: 'Play sound' };
    if (row.clip.kind === 'fade') return fadeLabel(row.clip);
    var options = row.clip.kind === 'dialogue' ? choiceOptions(row.clip) : null;
    if (options) return 'Choice: ' + options.option1 + ' / ' + options.option2;
    // The native Director release is the scene's end, not a visual effect.
    if (row.clip.kind === 'effect' && row.clip.payload && row.clip.payload.nativeOpcode === '0x7D') return 'Scene ends';
    if (row.clip.kind === 'dialogue') {
      var speaker = row.clip.payload.speaker;
      return speaker && speaker !== 'Native dialogue' ? speaker + ' speaks' : 'Dialogue';
    }
    return labels[row.clip.kind] || 'Scene action';
  }

  function renderCreatorToolbar(shell, rom, state, scene, document) {
    var view = viewFor(state, scene.sceneId);
    var header = node('header', 'cutscene-creator-toolbar');
    // A new scene has one name, persisted with its identity (Project and ROM
    // manifest); the document title follows it. Retail scenes name the document.
    var name = scene.authored
      ? textInput(scene.authored.name, function(value) {
        renameAuthoredScene(rom, state, scene, value);
      }, 'creator-scene-name')
      : textInput(document.identity.friendlyName || OB64.cutsceneCatalog.displayName(scene),
      function(value) {
        executeEdit(rom, state, 'Name scene', function(next) {
          next.identity.friendlyName = value.trim() || null;
        });
      }, 'creator-scene-name');
    name.className = 'cutscene-creator-name';
    name.setAttribute('aria-label', 'Scene name');
    header.appendChild(name);
    var actions = node('div', 'cutscene-creator-toolbar-actions');
    var browserToggle = button(view.creatorBrowserHidden ? 'Show scenes' : 'Hide scenes', 'btn-secondary', function() {
      view.creatorBrowserHidden = !view.creatorBrowserHidden;
      rerender(rom, state);
    });
    browserToggle.setAttribute('aria-expanded', view.creatorBrowserHidden ? 'false' : 'true');
    browserToggle.setAttribute('data-cutscene-focus-key', 'creator-browser-toggle');
    actions.appendChild(browserToggle);
    [['Event launch', 'launch'], ['Background', 'background'], ['Advanced', 'advanced']].forEach(function(entry) {
      var control = button(entry[0], 'btn-secondary', function() {
        view.creatorDrawer = view.creatorDrawer === entry[1] ? null : entry[1];
        rerender(rom, state);
      });
      control.setAttribute('aria-expanded', view.creatorDrawer === entry[1] ? 'true' : 'false');
      control.setAttribute('data-cutscene-focus-key', 'creator-drawer:' + entry[1]);
      actions.appendChild(control);
    });
    actions.appendChild(button('Use as template', 'btn-secondary', function() {
      createFromTemplate(rom, state);
    }));
    [['Undo', 'undo'], ['Redo', 'redo']].forEach(function(entry) {
      actions.appendChild(button(entry[0], 'btn-secondary', function() {
        var history = historyFor(state, scene);
        if (OB64.cutsceneModel[entry[1]](history)) {
          refreshRuntime(state, scene, history.present, rom);
          notifyChange(state, entry[0] + ' scene edit');
          rerender(rom, state);
        }
      }));
    });
    header.appendChild(actions);
    var status = node('div', 'cutscene-creator-export-status',
      document.exportRequirements.capability === 'native' ? 'Ready to export' : 'Export needs attention');
    status.setAttribute('role', 'status');
    if (document.exportRequirements.capability !== 'native') {
      status.appendChild(node('span', '', ' · ' +
        (document.exportRequirements.reasons[0] || 'Some actions are preview only.')));
    }
    if (scene.authored) {
      var identityLine = node('div', 'cutscene-creator-provenance cutscene-authored-identity');
      identityLine.setAttribute('data-cutscene-focus-key', 'scene-authored-identity');
      identityLine.appendChild(node('span', '', 'New scene · Director selector row ' + scene.authored.selectorRow +
        '. ' + launchSummary(state, scene) + ' Its Actors and actions go before its native exit.'));
      if (!view.deleteConfirm) {
        var deleteStart = button('Delete scene…', 'btn-secondary', function() { view.deleteConfirm = true; rerender(rom, state); });
        deleteStart.setAttribute('data-cutscene-focus-key', 'scene-delete-start');
        identityLine.appendChild(deleteStart);
      } else {
        identityLine.appendChild(node('span', 'cutscene-wipe-confirm-text', 'Delete “' + scene.authored.name + '”?' +
          (scene.authored.romState ? ' It is removed from the ROM on the next export and its selector row is freed.' : '') +
          (sceneBindings(state, scene).length ? ' Its launch sites go back to their retail scenes.' : '')));
        var deleteConfirm = button('Delete scene', 'btn-secondary', function() {
          view.deleteConfirm = false;
          deleteAuthoredScene(rom, state, scene);
        });
        deleteConfirm.setAttribute('data-cutscene-focus-key', 'scene-delete-confirm');
        var deleteCancel = button('Cancel', 'btn-secondary', function() { view.deleteConfirm = false; rerender(rom, state); });
        deleteCancel.setAttribute('data-cutscene-focus-key', 'scene-delete-cancel');
        identityLine.appendChild(deleteConfirm);
        identityLine.appendChild(deleteCancel);
      }
      header.appendChild(identityLine);
    } else if (OB64.cutsceneEvents && state.eventBindings && state.eventBindings.length) {
      var rebound = reboundRetailSites(state, scene);
      var bindingLine = node('div', 'cutscene-creator-provenance', launchSummary(state, scene) +
        (rebound.length ? ' ' + rebound.length + ' of its retail launch site' + (rebound.length === 1 ? ' now launches' : 's now launch') +
          ' another scene.' : ''));
      bindingLine.setAttribute('data-cutscene-focus-key', 'scene-launch-summary');
      header.appendChild(bindingLine);
    }
    if (!scene.authored && scene.engine === 'director' && OB64.cutsceneAuthored) {
      renderWipeControls(header, rom, state, scene, view);
      renderPreviewCast(header, rom, state, scene);
    }
    var loadsFrom = provenanceSummary(scene, true);
    if (loadsFrom) {
      var provenanceLine = node('div', 'cutscene-creator-provenance', 'Loads from ' + loadsFrom);
      provenanceLine.setAttribute('data-cutscene-focus-key', 'scene-provenance');
      header.appendChild(provenanceLine);
    }
    var runtime = state.runtimeByAssetId && state.runtimeByAssetId[scene.assetId];
    if (runtime && runtime.scannerRestarts > 0) {
      status.classList.add('cutscene-scanner-restart');
      status.appendChild(node('span', 'cutscene-source-warning', ' · The game would restart this scene from the top ' +
        runtime.scannerRestarts + (runtime.scannerRestarts === 1 ? ' time' : ' times') +
        ': a wait or dialogue has no barrier before the next condition.'));
    }
    header.appendChild(status);
    shell.appendChild(header);
  }

  function renderCreatorCast(center, rom, state, scene, document) {
    var view = viewFor(state, scene.sceneId);
    var panel = node('section', 'cutscene-creator-cast');
    var cast = node('div', 'cutscene-creator-cast-list');
    document.actors.forEach(function(actor) {
      var choice = button(actor.label, 'btn-secondary', function() {
        view.selectedActorId = actor.id;
        view.selectedClipId = null;
        view.creatorDestinationClipId = null;
        rerender(rom, state);
      });
      choice.setAttribute('aria-pressed', view.selectedActorId === actor.id ? 'true' : 'false');
      choice.setAttribute('data-cutscene-focus-key', 'creator-actor:' + actor.id);
      cast.appendChild(choice);
    });
    var addActor = button('+ Actor', 'btn-secondary', function() {
      addPreviewActor(rom, state, null);
      if (view.selectedActorId) {
        var open = view.creatorOpenPanels || (view.creatorOpenPanels = {});
        open['actor:' + view.selectedActorId] = true;
        rerender(rom, state);
      }
    });
    addActor.disabled = availableActorSlot(document, scene) === null;
    cast.appendChild(addActor);
    panel.appendChild(cast);
    var actor = document.actors.find(function(candidate) { return candidate.id === view.selectedActorId; });
    if (actor) {
      var controls = node('div', 'cutscene-creator-actor-controls');
      controls.appendChild(field('Name', textInput(actor.label, function(value) {
        executeEdit(rom, state, 'Rename Actor', function(next) {
          next.actors.find(function(item) { return item.id === actor.id; }).label = value.trim() || actor.label;
        });
      }, 'creator-actor-name:' + actor.id)));
      var appearance = node('select');
      for (var index = 0; index < 8; index++) {
        var option = node('option', '', 'Appearance ' + index);
        option.value = index; appearance.appendChild(option);
      }
      appearance.value = String(Math.max(0, actor.source.variantSelector || 0));
      appearance.setAttribute('data-cutscene-focus-key', 'creator-appearance:' + actor.id);
      appearance.addEventListener('change', function() {
        executeEdit(rom, state, 'Change Actor appearance', function(next) {
          var target = next.actors.find(function(item) { return item.id === actor.id; });
          target.source.variantSelector = Number(appearance.value);
          target.capability = actorHasNativePlace(target) ? 'native' : 'preview-only';
        });
      });
      controls.appendChild(field('Appearance', appearance));
      var facing = node('select');
      ['Right', 'Left', 'Away', 'Toward'].forEach(function(label, index) {
        var option = node('option', '', label); option.value = index; facing.appendChild(option);
      });
      var match = String(actor.initial.facing).match(/^native-([0-3])$/);
      facing.value = match ? match[1] : '0';
      facing.setAttribute('data-cutscene-focus-key', 'creator-facing:' + actor.id);
      facing.addEventListener('change', function() {
        executeEdit(rom, state, 'Change Actor facing', function(next) {
          next.actors.find(function(item) { return item.id === actor.id; }).initial.facing = 'native-' + facing.value;
        });
      });
      controls.appendChild(field('Facing', facing));
      controls.appendChild(button('Remove', 'btn-secondary cutscene-remove-action', function() {
        executeEdit(rom, state, 'Remove Actor', function(next) { OB64.cutsceneModel.removeActor(next, actor.id); });
      }));
      panel.appendChild(controls);
      var more = creatorDetails(view, 'actor:' + actor.id, 'Art, animation and coordinates');
      if (more.open) renderActorInspector(more, rom, state, actor);
      more.addEventListener('toggle', function() {
        view.creatorOpenPanels['actor:' + actor.id] = more.open;
        // The large art catalogue is built only when requested.
        if (more.open && more.childElementCount === 1) rerender(rom, state);
      });
      panel.appendChild(more);
    } else {
      panel.appendChild(node('p', 'cutscene-empty-note', document.actors.length
        ? 'Select an Actor on the Stage to edit it.' : 'This scene has no Actors yet. Use + Actor to add one.'));
    }
    center.appendChild(panel);
  }

  function renderCreatorSequence(shell, rom, state, scene, document) {
    var view = viewFor(state, scene.sceneId);
    var panel = node('aside', 'cutscene-creator-sequence');
    panel.setAttribute('data-cutscene-scroll', 'inspector');
    panel.appendChild(node('h3', '', 'Actions'));
    var menu = creatorDetails(view, 'add-action', '+ Add action');
    // Move, Pose, Appear and Disappear act on the selected Actor. A new or
    // wiped scene starts with none, so say where + Actor is.
    var actorHint = !document.actors.length
      ? 'Move, Pose, Appear and Disappear need an Actor. Use + Actor under the Stage to add one.'
      : !view.selectedActorId ? 'Select an Actor on the Stage or under it to use Move, Pose, Appear and Disappear.' : '';
    function actionButtons(host, entries) {
      entries.forEach(function(entry) {
        var action = button(entry[0], 'btn-secondary', function() {
          view.creatorOpenPanels['add-action'] = false;
          entry[1](rom, state);
        });
        action.setAttribute('data-cutscene-focus-key', 'creator-add:' + entry[0]);
        action.disabled = !!entry[2] && !view.selectedActorId;
        if (action.disabled) action.title = actorHint;
        host.appendChild(action);
      });
    }
    var choices = node('div', 'cutscene-creator-action-menu');
    actionButtons(choices, [['Move', addMove, true], ['Pose', addPose, true],
      ['Speak', addDialogue], ['Wait', addHold], ['Camera pan', addCamera], ['Fade', addFade]]);
    menu.appendChild(choices);
    var more = creatorDetails(view, 'more-actions', 'More actions');
    var moreChoices = node('div', 'cutscene-creator-action-menu');
    actionButtons(moreChoices, [['Appear', addEnter, true], ['Disappear', addExit, true],
      ['Effect', addEffect], ['Choice', function(targetRom, targetState) {
        try { addChoice(targetRom, targetState); } catch (error) {
          if (targetState.callbacks.onStatus) targetState.callbacks.onStatus(error.message);
        }
      }]]);
    more.appendChild(moreChoices);
    menu.appendChild(more);
    if (actorHint) {
      var hint = node('p', 'cutscene-field-hint cutscene-actor-hint', actorHint);
      hint.setAttribute('data-cutscene-focus-key', 'creator-actor-hint');
      menu.appendChild(hint);
    }
    menu.appendChild(node('p', 'cutscene-field-hint', 'Adds at the current preview position.'));
    panel.appendChild(menu);
    var list = node('div', 'cutscene-creator-action-list');
    var rows = creatorActionRows(document, view);
    state.ui.creatorCards = [];
    if (!rows.length) list.appendChild(node('p', 'cutscene-empty-note', 'Add an action to start building the scene.'));
    function clearDropMarks() {
      state.ui.creatorCards.forEach(function(entry) {
        entry.element.classList.toggle('drop-before', false);
        entry.element.classList.toggle('drop-after', false);
      });
    }
    function dropPlacesAfter(card, event) {
      var rect = card.getBoundingClientRect ? card.getBoundingClientRect() : null;
      if (!rect || !Number.isFinite(event.clientY)) return event.placeAfter === true;
      return event.clientY > rect.top + rect.height / 2;
    }
    function moveReport(reason) {
      if (!reason || reason === 'unknown-card') return;
      var messages = {
        'native-command': 'Native commands keep their place; drag the added actions instead.',
        'no-insertion-point': 'No safe insertion point exists there for a native action.',
        'no-neighbour': 'Nothing to reorder against.'
      };
      view.creatorNotice = messages[reason] || reason;
      rerender(rom, state);
    }
    if (view.creatorNotice) {
      list.appendChild(node('p', 'cutscene-source-warning', view.creatorNotice));
      view.creatorNotice = null;
    }
    var running = runningAtExit(state, scene, document);
    if (running) {
      var exitWarning = node('div', 'cutscene-source-warning cutscene-creator-exit-warning');
      exitWarning.setAttribute('data-cutscene-focus-key', 'creator-exit-warning');
      exitWarning.appendChild(node('span', '', 'The scene ends at ' + secondsText(running.exitTick) + ' while ' +
        running.late.map(function(item) { return creatorActionLabel(item.row, document); }).join(', ') +
        (running.late.length === 1 ? ' is' : ' are') + ' still running. Its native exit stops them. '));
      var holdButton = button('Hold until finished', 'btn-secondary', function() { holdUntilFinished(rom, state); });
      holdButton.setAttribute('data-cutscene-focus-key', 'creator-hold-until-finished');
      exitWarning.appendChild(holdButton);
      list.appendChild(exitWarning);
    }
    rows.forEach(function(row, index) {
      var card = node('section', 'cutscene-creator-action');
      card.classList.toggle('selected', row.clip.id === view.selectedClipId);
      var movable = clipIsReorderable(row.clip);
      card.classList.toggle('movable', movable);
      card.setAttribute('data-cutscene-clip-id', row.clip.id);
      if (movable) {
        card.draggable = true;
        card.setAttribute('draggable', 'true');
        card.addEventListener('dragstart', function(event) {
          state.ui.draggingClipId = row.clip.id;
          if (event.dataTransfer) {
            event.dataTransfer.setData('text/plain', row.clip.id);
            event.dataTransfer.effectAllowed = 'move';
          }
          card.classList.toggle('dragging', true);
        });
        card.addEventListener('dragend', function() {
          state.ui.draggingClipId = null;
          card.classList.toggle('dragging', false);
          clearDropMarks();
        });
      }
      card.addEventListener('dragover', function(event) {
        var dragging = state.ui.draggingClipId;
        if (!dragging || dragging === row.clip.id) return;
        if (event.preventDefault) event.preventDefault();
        if (event.dataTransfer) event.dataTransfer.dropEffect = 'move';
        var after = dropPlacesAfter(card, event);
        clearDropMarks();
        card.classList.toggle('drop-before', !after);
        card.classList.toggle('drop-after', after);
      });
      card.addEventListener('dragleave', function() {
        card.classList.toggle('drop-before', false);
        card.classList.toggle('drop-after', false);
      });
      card.addEventListener('drop', function(event) {
        if (event.preventDefault) event.preventDefault();
        var dragging = state.ui.draggingClipId ||
          (event.dataTransfer && event.dataTransfer.getData && event.dataTransfer.getData('text/plain'));
        var after = dropPlacesAfter(card, event);
        state.ui.draggingClipId = null;
        clearDropMarks();
        if (!dragging || dragging === row.clip.id) return;
        moveReport(reorderClip(rom, state, scene, dragging, row.clip.id, after));
      });
      var select = button('', 'cutscene-creator-action-heading', function() {
        if (view.selectedClipId === row.clip.id) {
          view.selectedClipId = null;
          view.creatorDestinationClipId = null;
          rerender(rom, state);
        } else selectClip(rom, state, scene, row);
      });
      select.setAttribute('data-cutscene-focus-key', 'creator-action:' + row.clip.id);
      select.setAttribute('aria-expanded', view.selectedClipId === row.clip.id ? 'true' : 'false');
      select.appendChild(node('span', 'cutscene-creator-action-number', String(index + 1)));
      if (movable) {
        var grip = node('span', 'cutscene-creator-action-grip', '⋮⋮');
        grip.title = 'Drag to reorder';
        grip.setAttribute('aria-hidden', 'true');
        select.appendChild(grip);
      }
      select.appendChild(node('strong', '', creatorActionLabel(row, document)));
      card.appendChild(select);
      var timingText = row.clip.source.nodeId ? '' : cardTimingText(state, scene, row.clip);
      var branchTag = cardBranch(document, row.clip);
      if (branchTag) timingText = 'After option ' + branchTag.option + (timingText ? ' · ' + timingText : '');
      if (branchTag && !timingText) timingText = 'After option ' + branchTag.option;
      if (timingText) {
        var timingLine = node('p', 'cutscene-creator-action-timing', timingText);
        timingLine.setAttribute('data-cutscene-focus-key', 'creator-timing:' + row.clip.id);
        card.appendChild(timingLine);
      }
      if (row.clip.kind === 'dialogue') card.appendChild(node('p', 'cutscene-creator-action-copy', dialogueCardText(row.clip)));
      if (row.clip.id === view.selectedClipId) {
        var controls = node('div', 'cutscene-creator-action-controls');
        if (row.clip.capability !== 'native') controls.appendChild(node('p', 'cutscene-source-warning',
          'This action is currently preview only.'));
        if (movable) {
          var order = node('div', 'cutscene-creator-action-order');
          var earlier = button('▲ Move earlier', 'btn-secondary', function() {
            var before = rows[index - 1];
            moveReport(before ? reorderClip(rom, state, scene, row.clip.id, before.clip.id, false) : 'no-neighbour');
          });
          earlier.setAttribute('data-cutscene-focus-key', 'creator-reorder-earlier:' + row.clip.id);
          earlier.disabled = index === 0;
          var later = button('▼ Move later', 'btn-secondary', function() {
            var following = rows[index + 1];
            moveReport(following ? reorderClip(rom, state, scene, row.clip.id, following.clip.id, true) : 'no-neighbour');
          });
          later.setAttribute('data-cutscene-focus-key', 'creator-reorder-later:' + row.clip.id);
          later.disabled = index === rows.length - 1;
          order.appendChild(earlier);
          order.appendChild(later);
          order.appendChild(node('span', 'cutscene-field-hint', 'Or drag the card to a new place in the list.'));
          controls.appendChild(order);
        } else {
          controls.appendChild(node('p', 'cutscene-field-hint',
            'This native command keeps its place in the scene; added actions can be dragged around it.'));
        }
        if (ASYNC_CARD_KINDS.indexOf(row.clip.kind) >= 0 && !row.clip.source.nodeId) {
          var waitLabel = node('label', 'cutscene-creator-wait-toggle');
          var waitBox = node('input');
          waitBox.type = 'checkbox';
          waitBox.checked = !!(row.clip.payload && row.clip.payload.waitForCompletion === true);
          waitBox.setAttribute('data-cutscene-focus-key', 'creator-wait-until-done:' + row.clip.id);
          waitBox.addEventListener('change', function() {
            var wanted = !!waitBox.checked;
            executeEdit(rom, state, wanted ? 'Wait until the action finishes' : 'Run alongside the next action', function(next) {
              var target = findClipRow(next, row.clip.id);
              if (target) target.clip.payload = Object.assign({}, target.clip.payload, { waitForCompletion: wanted });
            });
          });
          waitLabel.appendChild(waitBox);
          waitLabel.appendChild(node('span', '', ' Wait until it finishes before the next action'));
          controls.appendChild(waitLabel);
        }
        if (!row.clip.source.nodeId && !(row.clip.payload && row.clip.payload.choice)) {
          var earlierChoices = choicesBefore(document, row);
          var nativeChoices = orderedClips(document).filter(function(candidate) {
            return candidate.clip.payload && candidate.clip.payload.nativeChoice;
          });
          if (earlierChoices.length || nativeChoices.length) {
            var runs = node('select');
            runs.setAttribute('data-cutscene-focus-key', 'creator-runs:' + row.clip.id);
            [['', 'Always']].concat(earlierChoices.concat(nativeChoices).flatMap(function(choiceRow) {
              var name = choiceOptions(choiceRow.clip);
              return [[choiceRow.clip.id + '|1', 'After option 1 (' + name.option1 + ')'],
                [choiceRow.clip.id + '|2', 'After option 2 (' + name.option2 + ')']];
            })).forEach(function(entry) {
              var option = node('option', '', entry[1]); option.value = entry[0]; runs.appendChild(option);
            });
            var tag = cardBranch(document, row.clip);
            runs.value = tag ? tag.choiceClipId + '|' + tag.option : '';
            runs.addEventListener('change', function() {
              var parts = runs.value.split('|');
              var nativeTarget = runs.value && nativeChoices.find(function(candidate) { return candidate.clip.id === parts[0]; });
              var exitBoundary = insertionBoundaries(scene, document, state).filter(function(candidate) {
                return candidate.sourceNode.opcodeU32 === 0x7D || candidate.sourceNode.opcodeU32 === 0x39;
              })[0];
              editClip(rom, state, row.clip.id, 'Choose when this action runs', function(next) {
                var payload = Object.assign({}, next.clip.payload);
                delete payload.choiceBranch;
                if (nativeTarget) {
                  // A reopened choice: run in that branch, before it rejoins.
                  next.clip.source.insertBeforeNodeId = nativeTarget.clip.payload.nativeChoice.branchEndNodeIds[Number(parts[1]) - 1];
                } else {
                  if (runs.value) payload.choiceBranch = { choiceClipId: parts[0], option: Number(parts[1]) };
                  if (exitBoundary && cardBranch(document, row.clip) && !(row.clip.payload && row.clip.payload.choiceBranch)) {
                    next.clip.source.insertBeforeNodeId = exitBoundary.id;
                  }
                }
                next.clip.payload = payload;
              });
            });
            controls.appendChild(field('Runs', runs, 'Only for one option of an earlier choice, or always.'));
          }
        }
        renderClipInspector(controls, rom, state, scene, document, row, null, true);
        card.appendChild(controls);
      }
      state.ui.creatorCards.push({ element: card, clip: row.clip });
      list.appendChild(card);
    });
    panel.appendChild(list);
    shell.appendChild(panel);
  }

  function renderCreatorDrawer(shell, rom, state, scene, document) {
    var view = viewFor(state, scene.sceneId);
    if (!view.creatorDrawer) return;
    var drawer = node('section', 'cutscene-creator-drawer');
    drawer.appendChild(button('Close', 'btn-secondary', function() {
      view.creatorDrawer = null; rerender(rom, state);
    }));
    if (view.creatorDrawer === 'scenes') {
      renderSceneBrowser(drawer, rom, state);
    } else if (view.creatorDrawer === 'launch') {
      renderLaunchDrawer(drawer, rom, state, scene);
    } else if (view.creatorDrawer === 'background') {
      renderBackgroundDrawer(drawer, rom, state, scene, document);
    } else {
      drawer.appendChild(state.ui.creatorViewControls);
      renderTimeline(drawer, rom, state, scene, document);
      var settings = creatorDetails(view, 'scene-settings', 'Scene, background and playback settings');
      if (settings.open) {
        state.ui.backgroundStatus = renderSceneSettings(settings, rom, state, scene, document,
          document.identity.captures || []);
      }
      settings.addEventListener('toggle', function() {
        view.creatorOpenPanels['scene-settings'] = settings.open;
        if (settings.open && settings.childElementCount === 1) rerender(rom, state);
      });
      drawer.appendChild(settings);
    }
    shell.appendChild(drawer);
  }

  function renderCreatorWorkspace(shell, rom, state, scene, document) {
    state.ui.creator = true;
    var view = viewFor(state, scene.sceneId);
    renderCreatorToolbar(shell, rom, state, scene, document);
    // The scene browser is a persistent left bar with event-row filters; the
    // toolbar's Hide/Show scenes toggle collapses it.
    shell.classList.toggle('cutscene-creator-no-browser', !!view.creatorBrowserHidden);
    if (!view.creatorBrowserHidden) renderSceneBrowser(shell, rom, state);
    renderStageArea(shell, rom, state, scene, document, true);
    renderCreatorSequence(shell, rom, state, scene, document);
    renderCreatorDrawer(shell, rom, state, scene, document);
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
    if (state.loadingAssetId && state.loadingAssetId !== scene.assetId && state.runtimeController) {
      state.runtimeController.abort();
    }
    var history = historyFor(state, scene);
    var runtimeMissing = history && scene.engine === 'director' &&
      state.programByAssetId[scene.assetId] &&
      !state.runtimeByAssetId[scene.assetId] && !state.sourceErrors['runtime-context:' + scene.assetId];
    if (!history || runtimeMissing) {
      state.ui = { panel: panel };
      renderLoading(panel, scene);
      renderSceneBrowser(panel, rom, state);
      restoreUi(panel, restoreSnapshot);
      var request = ++state.openRequest;
      loadScene(rom, state, scene).then(function() {
        if (request !== state.openRequest || state.selectedSceneId !== scene.sceneId) return;
        render(panel, rom, state.callbacks, restoreSnapshot);
      });
      return;
    }
    panel.innerHTML = '';
    var shell = node('div', 'cutscene-studio cutscene-creator');
    shell.setAttribute('data-cutscene-scene-id', scene.sceneId);
    panel.appendChild(shell);
    state.ui = { panel: panel };
    renderCreatorWorkspace(shell, rom, state, scene, history.present);
    paintStage(rom, state);
    requestBackground(rom, state, history.present);
    restoreUi(panel, restoreSnapshot);
  }

  function resetAll(state) {
    state.projectionEpoch = (state.projectionEpoch || 0) + 1;
    if (state.boundRuntimeDocument) OB64.cutsceneRuntime.unbind(state.boundRuntimeDocument);
    state.boundRuntimeDocument = null;
    if (state.runtimeController) state.runtimeController.abort();
    Object.keys(state.histories).forEach(function(key) {
      OB64.cutsceneRuntime.unbind(state.histories[key].present);
    });
    state.openRequest++;
    state.loadingByAssetId = {};
    state.histories = {};
    state.originalSerialized = {};
    state.sourceByAssetId = {};
    state.programByAssetId = {};
    state.runtimeByAssetId = {};
    state.projectionLoadingByAssetId = {};
    state.nativeLaunchInputsByAssetId = {};
    state.concurrentRuntimeByLaunchContext = {};
    state.sourceErrors = {};
    state.views = {};
    state.imageCache = {};
    state.imageCacheBytes = 0;
    state.dialogueDrawer = null;
    state.composedDialogueIds = [];
    pauseAnimation(state);
  }

  OB64.cutsceneUI = {
    reorderAction: reorderClip,
    setNativeLaunchInputs: setNativeLaunchInputs,
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
    capturePreviewFrame:capturePreviewFrame,
    composeDialogue:composeDialogue,
    playbackTimingLabel:playbackTimingLabel,
    captureUi: captureUi,
    restoreUi: restoreUi,
    sceneHasChanges: sceneHasChanges,
    editCount: editCount,
    refreshExportRequirements: refreshExportRequirements,
    executeEdit: executeEdit,
    createFromTemplate: createFromTemplate,
    addHold: addHold,
    addCamera: addCamera,
    addFade: addFade,
    runningAtExit: runningAtExit,
    holdUntilFinished: holdUntilFinished,
    cardTimingText: cardTimingText,
    viewFor: viewFor,
    nativeCoordinate: nativeCoordinate,
    createAuthoredScene: createAuthoredScene,
    prepareWipe: prepareWipe,
    applyWipe: applyWipe,
    wipeScene: wipeScene,
    restoreWipedScene: restoreWipedScene,
    wipeSharers: wipeSharers,
    assignLaunchSite: assignLaunchSite,
    selectNativeBackground: selectNativeBackground,
    setPreviewCast: setPreviewCast,
    isStoryCastScene: isStoryCastScene,
    launchOperandTranslations: launchOperandTranslations,
    backgroundReadOnlyReason: backgroundReadOnlyReason,
    removeLaunchSite: removeLaunchSite,
    renameAuthoredScene: renameAuthoredScene,
    deleteAuthoredScene: deleteAuthoredScene,
    invalidateAfterBindingChange: invalidateAfterBindingChange,
    addActor: function(rom, state, sourceActor) { return addPreviewActor(rom, state, sourceActor || null); },
    addDialogue: addDialogue,
    addChoice: addChoice,
    addMove: addMove,
    addPose: addPose,
    insertionBoundaries: insertionBoundaries,
    boundaryForFrame: boundaryForFrame,
    clipRuntimeTick: clipRuntimeTick,
    findClipRow: findClipRow,
    directorSourceRows: directorSourceRows,
    directorEditorTarget: directorEditorTarget,
    actorPoseSelectionAtFrame: actorPoseSelectionAtFrame,
    render: render,
    resetAll: resetAll,
    pause: pauseAnimation
  };
})(window.OB64);
