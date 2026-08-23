// Lordly Caliber - validated Cutscene Studio catalog access.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var FORMAT = 'ob64-cutscene-catalog';
  var SCHEMA_VERSION = 4;

  function CatalogError(message) {
    this.name = 'CutsceneCatalogError';
    this.message = message;
  }
  CatalogError.prototype = Object.create(Error.prototype);
  CatalogError.prototype.constructor = CatalogError;

  function fail(message) { throw new CatalogError(message); }

  function object(value, label) {
    if (!value || Object.prototype.toString.call(value) !== '[object Object]') {
      fail(label + ' must be an object.');
    }
  }

  function nonEmptyString(value, label) {
    if (typeof value !== 'string' || !value.length) fail(label + ' must be a non-empty string.');
  }

  function integer(value, label, minimum) {
    if (!Number.isInteger(value) || value < (minimum || 0)) {
      fail(label + ' must be an integer at least ' + (minimum || 0) + '.');
    }
  }

  function finiteNumber(value, label, minimumExclusive) {
    if (typeof value !== 'number' || !Number.isFinite(value) ||
        minimumExclusive != null && value <= minimumExclusive) {
      fail(label + ' must be a finite number' +
        (minimumExclusive != null ? ' greater than ' + minimumExclusive : '') + '.');
    }
  }

  function unique(value, seen, label) {
    nonEmptyString(value, label);
    if (seen[value]) fail(label + ' duplicates ' + value + '.');
    seen[value] = true;
  }

  function validateActorProjection(actorProjection, label) {
    object(actorProjection, label);
    nonEmptyString(actorProjection.mode, label + ' mode');
    nonEmptyString(actorProjection.coordinateSpace, label + ' coordinate space');
    integer(actorProjection.screenWidth, label + ' width', 1);
    integer(actorProjection.screenHeight, label + ' height', 1);
    ['modelScale', 'fovYDegrees', 'aspect', 'near', 'far'].forEach(function(field) {
      finiteNumber(actorProjection[field], label + ' ' + field, 0);
    });
    ['eye', 'target', 'up'].forEach(function(vectorName) {
      object(actorProjection[vectorName], label + ' ' + vectorName);
      ['x', 'y', 'z'].forEach(function(field) {
        finiteNumber(actorProjection[vectorName][field],
          label + ' ' + vectorName + ' ' + field);
      });
    });
    nonEmptyString(actorProjection.calibrationStatus, label + ' calibration status');
    nonEmptyString(actorProjection.calibrationResult, label + ' calibration result');
  }

  function validateNativeSceneProps(stageProps, label) {
    object(stageProps, label);
    nonEmptyString(stageProps.coordinateSpace, label + ' coordinate space');
    integer(stageProps.placementResourceKey, label + ' placement resource key', 1);
    integer(stageProps.orthographicTableHeaderEntry,
      label + ' orthographic table header entry', 0);
    integer(stageProps.perspectiveTableHeaderEntry,
      label + ' perspective table header entry', 0);
    nonEmptyString(stageProps.evidenceStatus, label + ' evidence status');
    var source = stageProps.source;
    object(source, label + ' sprite source');
    nonEmptyString(source.bank, label + ' sprite bank');
    [
      'descriptorKey', 'descriptorMemberCount', 'metadataKey', 'poseKey',
      'configKey', 'lookupKey', 'metadataDecodedLength', 'poseDecodedLength',
      'configDecodedLength', 'lookupDecodedLength', 'artCount',
      'lookupBankCount', 'selectedChildOrdinal'
    ].forEach(function(field) {
      integer(source[field], label + ' sprite source ' + field,
        field === 'selectedChildOrdinal' ? 0 : 1);
    });
    if (!Array.isArray(stageProps.orthographicPlacements) ||
        !Array.isArray(stageProps.perspectivePlacements)) {
      fail(label + ' placements must be arrays.');
    }
    var ids = {};
    stageProps.orthographicPlacements.concat(stageProps.perspectivePlacements)
      .forEach(function(placement, index) {
        var placementLabel = label + ' placement ' + index;
        object(placement, placementLabel);
        nonEmptyString(placement.id, placementLabel + ' id');
        if (ids[placement.id]) fail(label + ' duplicates placement ' + placement.id + '.');
        ids[placement.id] = true;
        nonEmptyString(placement.projection, placementLabel + ' projection');
        nonEmptyString(placement.depthPass, placementLabel + ' depth pass');
        integer(placement.poseSelector, placementLabel + ' pose selector', 0);
        ['x', 'y', 'z'].forEach(function(field) {
          finiteNumber(placement[field], placementLabel + ' ' + field);
        });
      });
  }

  function validateLaunchCamera(camera, label) {
    object(camera, label);
    nonEmptyString(camera.kind, label + ' kind');
    nonEmptyString(camera.evidenceStatus, label + ' evidence status');
    nonEmptyString(camera.status, label + ' status');
    if (camera.projection != null) {
      validateActorProjection(camera.projection, label + ' projection');
    }
    if ((camera.kind === 'runtime-observed' ||
        camera.kind === 'mode-two-corpus-family') && camera.projection == null) {
      fail(label + ' requires a projection.');
    }
  }

  function validateLaunchProfile(scene) {
    var profile = scene.launchProfile;
    object(profile, scene.assetId + ' launch profile');
    nonEmptyString(profile.profileId, scene.assetId + ' launch profileId');
    object(profile.directorMode, scene.assetId + ' launch Director mode');
    if (profile.directorMode.value !== null &&
        profile.directorMode.value !== 0 && profile.directorMode.value !== 2) {
      fail(scene.assetId + ' launch Director mode must be 0, 2, or null.');
    }
    nonEmptyString(profile.directorMode.evidenceStatus,
      scene.assetId + ' launch Director mode evidence');
    nonEmptyString(profile.directorMode.source,
      scene.assetId + ' launch Director mode source');
    nonEmptyString(profile.directorMode.status,
      scene.assetId + ' launch Director mode status');
    object(profile.cameras, scene.assetId + ' launch cameras');
    validateLaunchCamera(profile.cameras.registered,
      scene.assetId + ' registered launch camera');
    validateLaunchCamera(profile.cameras.actor,
      scene.assetId + ' Actor launch camera');

    object(profile.background, scene.assetId + ' launch background');
    integer(profile.background.requestCount,
      scene.assetId + ' launch background request count');
    if (!Array.isArray(profile.background.requests) ||
        profile.background.requests.length !== profile.background.requestCount ||
        profile.background.requestCount !== scene.backgroundRequests.length) {
      fail(scene.assetId + ' launch background request count is inconsistent.');
    }
    var requestWordStarts = {};
    profile.background.requests.forEach(function(request, index) {
      object(request, scene.assetId + ' launch background request ' + index);
      nonEmptyString(request.requestId, scene.assetId + ' launch background requestId');
      integer(request.wordStart, request.requestId + ' wordStart');
      if (requestWordStarts[request.wordStart]) {
        fail(scene.assetId + ' duplicates launch background word ' + request.wordStart + '.');
      }
      requestWordStarts[request.wordStart] = true;
      if (request.selectorTableId !== null) {
        nonEmptyString(request.selectorTableId, request.requestId + ' selector table');
      }
      if (request.selector !== null) integer(request.selector, request.requestId + ' selector');
      nonEmptyString(request.selectorSource, request.requestId + ' selector source');
      nonEmptyString(request.evidenceStatus, request.requestId + ' evidence status');
      nonEmptyString(request.status, request.requestId + ' status');
      if (!Array.isArray(request.members) || !Array.isArray(request.assetIds) ||
          !Array.isArray(request.stageLayers) || !Array.isArray(request.stageAssetIds)) {
        fail(request.requestId + ' launch background assets must be arrays.');
      }
      if (request.members.length !== request.assetIds.length) {
        fail(request.requestId + ' ordered launch background members are inconsistent.');
      }
      request.members.forEach(function(member, memberIndex) {
        object(member, request.requestId + ' member ' + memberIndex);
        integer(member.ordinal, request.requestId + ' member ordinal');
        nonEmptyString(member.assetId, request.requestId + ' member assetId');
        if (member.assetId !== request.assetIds[memberIndex]) {
          fail(request.requestId + ' member order does not match its asset IDs.');
        }
      });
    });
    scene.backgroundRequests.forEach(function(request) {
      if (!requestWordStarts[request.wordStart]) {
        fail(scene.assetId + ' launch profile omits background word ' + request.wordStart + '.');
      }
    });

    object(profile.roster, scene.assetId + ' launch roster');
    integer(profile.roster.templateCount, scene.assetId + ' launch roster template count');
    if (!Array.isArray(profile.roster.records) ||
        profile.roster.records.length !== profile.roster.templateCount ||
        profile.roster.templateCount !== scene.actors.length ||
        !Array.isArray(profile.roster.recordProducerSlots) ||
        !Array.isArray(profile.roster.externalTemplateSlots) ||
        !Array.isArray(profile.roster.materializationWordStarts)) {
      fail(scene.assetId + ' launch roster is inconsistent.');
    }
    nonEmptyString(profile.roster.evidenceStatus,
      scene.assetId + ' launch roster evidence status');
    nonEmptyString(profile.roster.status, scene.assetId + ' launch roster status');
    profile.roster.records.forEach(function(record, index) {
      object(record, scene.assetId + ' launch roster record ' + index);
      nonEmptyString(record.actorId, scene.assetId + ' launch roster actorId');
      integer(record.slot, record.actorId + ' launch roster slot');
      if (typeof record.recordProducer !== 'boolean') {
        fail(record.actorId + ' launch roster recordProducer must be boolean.');
      }
      nonEmptyString(record.initializationStatus,
        record.actorId + ' launch roster initialization status');
      var actor = scene.actors.find(function(candidate) {
        return candidate.actorId === record.actorId && candidate.slot === record.slot;
      });
      if (!actor || actor.recordProducer !== record.recordProducer) {
        fail(record.actorId + ' launch roster does not match the scene Actor template.');
      }
    });
  }

  function validateNodes(scene) {
    var source = scene.source;
    if (!Array.isArray(source.nodes) || !source.nodes.length) {
      fail(scene.assetId + ' has no source-boundary catalog.');
    }
    var cursor = 0;
    var ids = {};
    source.nodes.forEach(function(node, index) {
      object(node, scene.assetId + ' node ' + index);
      unique(node.id, ids, scene.assetId + ' node id');
      integer(node.startWord, node.id + ' startWord');
      integer(node.endWord, node.id + ' endWord');
      integer(node.wordCount, node.id + ' wordCount', 1);
      nonEmptyString(node.name, node.id + ' semantic name');
      nonEmptyString(node.semanticSummary, node.id + ' semantic summary');
      nonEmptyString(node.confidence, node.id + ' confidence');
      nonEmptyString(node.opcode, node.id + ' opcode');
      integer(node.opcodeU32, node.id + ' opcodeU32');
      nonEmptyString(node.editPolicy, node.id + ' editPolicy');
      if (!Array.isArray(node.operandRoles) ||
          node.operandRoles.length !== node.wordCount - 1) {
        fail(node.id + ' operand roles do not match its exact source width.');
      }
      node.operandRoles.forEach(function(role, roleIndex) {
        nonEmptyString(role, node.id + ' operand role ' + roleIndex);
      });
      if (node.startWord !== cursor || node.endWord <= node.startWord ||
          node.endWord - node.startWord !== node.wordCount) {
        fail(node.id + ' does not own one exact contiguous source boundary.');
      }
      cursor = node.endWord;
    });
    if (cursor !== source.decodedWordCount || source.decodedLength !== cursor * 4) {
      fail(scene.assetId + ' source boundaries do not own the complete decoded payload.');
    }
    if (source.corpusNodeCount !== source.nodes.length ||
        !Array.isArray(source.registeredWaits) ||
        source.corpusRegisteredWaitCount !== source.registeredWaits.length) {
      fail(scene.assetId + ' corrected corpus counts are inconsistent.');
    }
    var waitIds = {};
    var claimedNodes = {};
    source.registeredWaits.forEach(function(wait, waitIndex) {
      object(wait, scene.assetId + ' registered wait ' + waitIndex);
      unique(wait.id, waitIds, scene.assetId + ' registered wait id');
      integer(wait.startWord, wait.id + ' startWord');
      integer(wait.endWord, wait.id + ' endWord', 1);
      integer(wait.ticks, wait.id + ' ticks');
      if (!Array.isArray(wait.nodeIds) || wait.nodeIds.length !== 6) {
        fail(wait.id + ' must own the six corrected primitive nodes.');
      }
      wait.nodeIds.forEach(function(nodeId) {
        if (!ids[nodeId]) fail(wait.id + ' references unknown node ' + nodeId + '.');
        if (claimedNodes[nodeId]) fail(wait.id + ' overlaps registered wait ' + claimedNodes[nodeId] + '.');
        claimedNodes[nodeId] = wait.id;
      });
      var first = source.nodes.find(function(node) { return node.id === wait.nodeIds[0]; });
      var last = source.nodes.find(function(node) {
        return node.id === wait.nodeIds[wait.nodeIds.length - 1];
      });
      if (!first || !last || first.startWord !== wait.startWord ||
          last.endWord !== wait.endWord) {
        fail(wait.id + ' source span does not match its primitive owners.');
      }
    });
  }

  function validateScene(scene, seen) {
    object(scene, 'scene');
    unique(scene.sceneId, seen.sceneIds, 'sceneId');
    unique(scene.assetId, seen.assetIds, 'assetId');
    unique(scene.storageId, seen.storageIds, 'storageId');
    unique(scene.directorKey, seen.directorKeys, 'directorKey');
    nonEmptyString(scene.technicalName, scene.assetId + ' technicalName');
    nonEmptyString(scene.engine, scene.assetId + ' engine');
    nonEmptyString(scene.sourceRevision, scene.assetId + ' sourceRevision');
    if (scene.friendlyName !== null && typeof scene.friendlyName !== 'string') {
      fail(scene.assetId + ' friendlyName must be null or a string.');
    }
    if (!Array.isArray(scene.aliases) || !Array.isArray(scene.actors)) {
      fail(scene.assetId + ' aliases and actors must be arrays.');
    }
    if (scene.reviewedTimelineOverlay != null) {
      var overlay = scene.reviewedTimelineOverlay;
      object(overlay, scene.assetId + ' reviewedTimelineOverlay');
      nonEmptyString(overlay.overlayId, scene.assetId + ' reviewed overlayId');
      nonEmptyString(overlay.reviewStatus, scene.assetId + ' reviewed overlay status');
      if (overlay.featuredSequence != null) {
        object(overlay.featuredSequence, overlay.overlayId + ' featuredSequence');
        integer(overlay.featuredSequence.actorSlot,
          overlay.overlayId + ' featured actor slot');
        nonEmptyString(overlay.featuredSequence.actorLabel,
          overlay.overlayId + ' featured actor label');
      }
      if (!Array.isArray(overlay.runtimeDependencies)) {
        fail(overlay.overlayId + ' runtimeDependencies must be an array.');
      }
      overlay.runtimeDependencies.forEach(function(dependency) {
        object(dependency, overlay.overlayId + ' runtime dependency');
        nonEmptyString(dependency.id, overlay.overlayId + ' runtime dependency id');
        integer(dependency.afterWord, dependency.id + ' afterWord');
        integer(dependency.targetWord, dependency.id + ' targetWord');
        if (!Array.isArray(dependency.viaWords)) {
          fail(dependency.id + ' viaWords must be an array.');
        }
      });
    }
    if (!Array.isArray(scene.backgroundAssetIds) ||
        !Array.isArray(scene.backgroundCandidateAssetIds) ||
        !Array.isArray(scene.backgroundRequests) ||
        !Array.isArray(scene.dialogueAssociations) ||
        !Array.isArray(scene.recoveredMediaRequests) ||
        !Array.isArray(scene.recoveredActorEvents) ||
        !Array.isArray(scene.recoveredNativeSpriteEffects)) {
      fail(scene.assetId + ' background associations must be arrays.');
    }
    if (scene.backgroundRuntimeObservation && scene.backgroundRuntimeObservation.stageLayers) {
      var stageObservation = scene.backgroundRuntimeObservation;
      if (Array.isArray(stageObservation.captures)) {
        stageObservation.captures.forEach(function(capture, index) {
          object(capture, scene.assetId + ' observed capture ' + index);
          if (capture.previewFrame != null) {
            integer(capture.previewFrame,
              scene.assetId + ' observed capture preview frame', 0);
            nonEmptyString(capture.previewTimingStatus,
              scene.assetId + ' observed capture preview timing status');
          }
        });
      }
      if (!Array.isArray(stageObservation.stageLayers) || !stageObservation.stageLayers.length) {
        fail(scene.assetId + ' observed Stage layers must be a non-empty array.');
      }
      stageObservation.stageLayers.forEach(function(layer, index) {
        object(layer, scene.assetId + ' observed Stage layer ' + index);
        nonEmptyString(layer.assetId, scene.assetId + ' observed Stage assetId');
        nonEmptyString(layer.role, scene.assetId + ' observed Stage role');
        integer(layer.depth, scene.assetId + ' observed Stage depth');
        nonEmptyString(layer.evidenceStatus,
          scene.assetId + ' observed Stage evidence status');
        nonEmptyString(layer.associationStatus,
          scene.assetId + ' observed Stage association status');
      });
      object(stageObservation.stageProjection,
        scene.assetId + ' observed Stage projection');
      var stageProjection = stageObservation.stageProjection;
      nonEmptyString(stageProjection.mode,
        scene.assetId + ' observed Stage projection mode');
      finiteNumber(stageProjection.scale,
        scene.assetId + ' observed Stage scale', 0);
      if (stageProjection.mode === 'b5-reference-capture') {
        finiteNumber(stageProjection.cropWorldX,
          scene.assetId + ' observed Stage crop X');
        finiteNumber(stageProjection.cropWorldY,
          scene.assetId + ' observed Stage crop Y');
        if (stageProjection.screenAnchorX != null) {
          finiteNumber(stageProjection.screenAnchorX,
            scene.assetId + ' observed Stage screen anchor X');
        }
        if (stageProjection.screenAnchorY != null) {
          finiteNumber(stageProjection.screenAnchorY,
            scene.assetId + ' observed Stage screen anchor Y');
        }
      }
      if (stageObservation.stageProjection.initialPreviewFrame != null) {
        integer(stageObservation.stageProjection.initialPreviewFrame,
          scene.assetId + ' observed Stage initial preview frame', 0);
      }
      object(stageObservation.stageProjection.viewport,
        scene.assetId + ' observed Stage viewport');
      ['left', 'top', 'width', 'height'].forEach(function(field) {
        integer(stageObservation.stageProjection.viewport[field],
          scene.assetId + ' observed Stage viewport ' + field, 0);
      });
      if (stageObservation.stageProjection.actorProjection != null) {
        var actorProjection = stageObservation.stageProjection.actorProjection;
        validateActorProjection(actorProjection,
          scene.assetId + ' observed actor projection');
      }
      if (stageProjection.nativeSceneProps != null) {
        validateNativeSceneProps(stageProjection.nativeSceneProps,
          scene.assetId + ' observed native Stage props');
      }
    }
    if (scene.actorCameraObservation != null) {
      validateActorProjection(scene.actorCameraObservation,
        scene.assetId + ' observed Actor camera');
    }
    scene.recoveredActorEvents.forEach(function(event) {
      object(event, scene.assetId + ' recovered actor event');
      nonEmptyString(event.eventId, scene.assetId + ' recovered actor eventId');
      integer(event.wordStart, event.eventId + ' wordStart');
      nonEmptyString(event.eventStatus, event.eventId + ' eventStatus');
      nonEmptyString(event.renderClosureStatus, event.eventId + ' renderClosureStatus');
      if (event.eventStatus === 'applied') {
        integer(event.slot, event.eventId + ' slot');
        integer(event.bank, event.eventId + ' bank', 1);
        integer(event.stateIndex, event.eventId + ' stateIndex');
        nonEmptyString(event.physicalStateId, event.eventId + ' physicalStateId');
      }
    });
    scene.recoveredNativeSpriteEffects.forEach(function(effect) {
      object(effect, scene.assetId + ' recovered sprite effect');
      nonEmptyString(effect.effectId, scene.assetId + ' recovered sprite effectId');
      integer(effect.wordStart, effect.effectId + ' wordStart');
      integer(effect.nativeEffectSlot, effect.effectId + ' nativeEffectSlot');
      integer(effect.bank, effect.effectId + ' bank', 1);
      integer(effect.stateIndex, effect.effectId + ' stateIndex');
      nonEmptyString(effect.physicalStateId, effect.effectId + ' physicalStateId');
      nonEmptyString(effect.renderClosureStatus, effect.effectId + ' renderClosureStatus');
    });
    var requestIds = {};
    scene.backgroundRequests.forEach(function(request) {
      object(request, scene.assetId + ' background request');
      unique(request.requestId, requestIds, scene.assetId + ' background requestId');
      integer(request.wordStart, request.requestId + ' wordStart');
      object(request.mode2Route, request.requestId + ' mode2Route');
      object(request.nonMode2Route, request.requestId + ' nonMode2Route');
      nonEmptyString(request.mode2Route.selectorTableId,
        request.requestId + ' mode2 selectorTableId');
      nonEmptyString(request.nonMode2Route.selectorTableId,
        request.requestId + ' nonMode2 selectorTableId');
      if (!Array.isArray(request.nonMode2Route.archiveAssetIds)) {
        fail(request.requestId + ' nonMode2 archiveAssetIds must be an array.');
      }
    });
    if (scene.actorBearing !== (scene.actors.length > 0) ||
        scene.actorCount !== scene.actors.length) {
      fail(scene.assetId + ' actor counts are inconsistent.');
    }
    var actorIds = {};
    var slots = {};
    scene.actors.forEach(function(actor) {
      object(actor, scene.assetId + ' actor');
      unique(actor.actorId, actorIds, scene.assetId + ' actorId');
      integer(actor.slot, actor.actorId + ' slot');
      if (typeof actor.recordProducer !== 'boolean') {
        fail(actor.actorId + ' recordProducer must be boolean.');
      }
      nonEmptyString(actor.initializationStatus, actor.actorId + ' initializationStatus');
      nonEmptyString(actor.selectorStatus, actor.actorId + ' selectorStatus');
      if (slots[actor.slot]) fail(scene.assetId + ' duplicates actor slot ' + actor.slot + '.');
      slots[actor.slot] = true;
    });
    validateLaunchProfile(scene);
    object(scene.source, scene.assetId + ' source');
    integer(scene.source.z64PrefixStart, scene.assetId + ' z64PrefixStart');
    integer(scene.source.z64PayloadStart, scene.assetId + ' z64PayloadStart');
    integer(scene.source.z64PayloadEndExclusive, scene.assetId + ' z64PayloadEndExclusive');
    integer(scene.source.storedPayloadLength, scene.assetId + ' storedPayloadLength', 1);
    integer(scene.source.dmaExtent, scene.assetId + ' dmaExtent', 1);
    integer(scene.source.decodedLength, scene.assetId + ' decodedLength', 4);
    integer(scene.source.decodedWordCount, scene.assetId + ' decodedWordCount', 1);
    nonEmptyString(scene.source.decodedSha256, scene.assetId + ' decodedSha256');
    nonEmptyString(scene.source.directorSelectorTableResourceKey,
      scene.assetId + ' directorSelectorTableResourceKey');
    integer(scene.source.directorSelectorTablePayloadZ64,
      scene.assetId + ' directorSelectorTablePayloadZ64', 1);
    if (!Array.isArray(scene.source.directorSelectorRows) ||
        !Array.isArray(scene.source.directorSelectorWordZ64) ||
        !scene.source.directorSelectorRows.length ||
        scene.source.directorSelectorRows.length !==
          scene.source.directorSelectorWordZ64.length) {
      fail(scene.assetId + ' Director selector ownership is incomplete.');
    }
    scene.source.directorSelectorRows.forEach(function(row, index) {
      integer(row, scene.assetId + ' Director selector row', 0);
      integer(scene.source.directorSelectorWordZ64[index],
        scene.assetId + ' Director selector word', 1);
      if (scene.source.directorSelectorWordZ64[index] !==
          scene.source.directorSelectorTablePayloadZ64 + row * 4) {
        fail(scene.assetId + ' Director selector word does not match its row.');
      }
    });
    if (scene.source.z64PayloadStart + scene.source.storedPayloadLength !==
        scene.source.z64PayloadEndExclusive) {
      fail(scene.assetId + ' stored payload range is inconsistent.');
    }
    if (scene.source.dmaExtent < scene.source.storedPayloadLength) {
      fail(scene.assetId + ' DMA extent is smaller than stored capacity.');
    }
    if (!Array.isArray(scene.source.historicalGaps)) {
      fail(scene.assetId + ' historicalGaps must be an array.');
    }
    if (scene.source.tailRecovery) {
      object(scene.source.tailRecovery, scene.assetId + ' tailRecovery');
      integer(scene.source.tailRecovery.recoveredWordCount,
        scene.assetId + ' recoveredWordCount', 1);
      integer(scene.source.tailRecovery.recoveredNodeCount,
        scene.assetId + ' recoveredNodeCount', 1);
      if (scene.source.tailRecovery.remainingGapWordCount !== 0 || scene.source.gaps.length) {
        fail(scene.assetId + ' recovered source must not retain a structural gap.');
      }
      nonEmptyString(scene.source.tailRecovery.recoveredTailSha256,
        scene.assetId + ' recoveredTailSha256');
    }
    validateNodes(scene);
  }

  function validatePresentationScene(scene, seen) {
    object(scene, 'presentation scene');
    unique(scene.sceneId, seen.sceneIds, 'sceneId');
    unique(scene.assetId, seen.assetIds, 'assetId');
    unique(scene.storageId, seen.storageIds, 'storageId');
    nonEmptyString(scene.technicalName, scene.assetId + ' technicalName');
    nonEmptyString(scene.engine, scene.assetId + ' engine');
    if (scene.directorKey !== null) fail(scene.assetId + ' must not claim a Director key.');
    object(scene.source, scene.assetId + ' source');
    if (!Array.isArray(scene.actors)) {
      fail(scene.assetId + ' presentation actors must be an array.');
    }
    if (scene.actorBearing !== (scene.actors.length > 0) ||
        scene.actorCount !== scene.actors.length) {
      fail(scene.assetId + ' presentation actor counts are inconsistent.');
    }
    if (scene.actors.length && !scene.source.partialDirectorResourceId) {
      fail(scene.assetId + ' cannot claim actors without a Director Partial resource.');
    }
    if (!Array.isArray(scene.backgroundAssetIds) ||
        !Array.isArray(scene.backgroundCandidateAssetIds) ||
        !Array.isArray(scene.backgroundRequests) ||
        !Array.isArray(scene.dialogueAssociations) || !Array.isArray(scene.audioAssociations) ||
        !Array.isArray(scene.recoveredMediaRequests) ||
        !Array.isArray(scene.recoveredActorEvents) ||
        !Array.isArray(scene.recoveredNativeSpriteEffects)) {
      fail(scene.assetId + ' presentation associations must be arrays.');
    }
    scene.recoveredActorEvents.forEach(function(event) {
      object(event, scene.assetId + ' recovered actor event');
      nonEmptyString(event.eventId, scene.assetId + ' recovered actor eventId');
      integer(event.wordStart, event.eventId + ' wordStart');
      nonEmptyString(event.eventStatus, event.eventId + ' eventStatus');
      nonEmptyString(event.renderClosureStatus, event.eventId + ' renderClosureStatus');
    });
    scene.recoveredNativeSpriteEffects.forEach(function(effect) {
      object(effect, scene.assetId + ' recovered sprite effect');
      nonEmptyString(effect.effectId, scene.assetId + ' recovered sprite effectId');
      integer(effect.wordStart, effect.effectId + ' wordStart');
      integer(effect.bank, effect.effectId + ' bank', 1);
      integer(effect.stateIndex, effect.effectId + ' stateIndex');
      nonEmptyString(effect.physicalStateId, effect.effectId + ' physicalStateId');
      nonEmptyString(effect.renderClosureStatus, effect.effectId + ' renderClosureStatus');
    });
    nonEmptyString(scene.source.sourceKind, scene.assetId + ' sourceKind');
    nonEmptyString(scene.source.adapterStatus, scene.assetId + ' adapterStatus');
    if (!Array.isArray(scene.source.nodes) || scene.source.nodes.length ||
        !Array.isArray(scene.source.gaps)) {
      fail(scene.assetId + ' unresolved adapter source boundary is inconsistent.');
    }
    var actorIds = {}, slots = {};
    scene.actors.forEach(function(actor) {
      object(actor, scene.assetId + ' actor');
      unique(actor.actorId, actorIds, scene.assetId + ' actorId');
      integer(actor.slot, actor.actorId + ' slot');
      if (typeof actor.recordProducer !== 'boolean') {
        fail(actor.actorId + ' recordProducer must be boolean.');
      }
      nonEmptyString(actor.initializationStatus, actor.actorId + ' initializationStatus');
      nonEmptyString(actor.selectorStatus, actor.actorId + ' selectorStatus');
      if (slots[actor.slot]) fail(scene.assetId + ' duplicates actor slot ' + actor.slot + '.');
      slots[actor.slot] = true;
    });
  }

  function validatePartialDirectorResource(resource, seen) {
    object(resource, 'Director Partial resource');
    unique(resource.resourceId, seen, 'Director Partial resourceId');
    nonEmptyString(resource.directorKey, resource.resourceId + ' directorKey');
    if (resource.disposition !== 'director-partial' || resource.parseStatus !== 'partial') {
      fail(resource.resourceId + ' must remain classified as Director Partial.');
    }
    integer(resource.parsedWordCount, resource.resourceId + ' parsedWordCount', 1);
    integer(resource.decodedWordCount, resource.resourceId + ' decodedWordCount', 1);
    integer(resource.gapWordCount, resource.resourceId + ' gapWordCount', 1);
    integer(resource.structurallyOwnedWordCount,
      resource.resourceId + ' structurallyOwnedWordCount', 1);
    if (resource.structurallyOwnedWordCount !== resource.decodedWordCount ||
        resource.remainingGapWordCount !== 0) {
      fail(resource.resourceId + ' runtime structural ownership is incomplete.');
    }
    object(resource.tailRecovery, resource.resourceId + ' tailRecovery');
    nonEmptyString(resource.tailRecovery.recoveredTailSha256,
      resource.resourceId + ' recoveredTailSha256');
    if (!Array.isArray(resource.recoveredMediaRequests)) {
      fail(resource.resourceId + ' recoveredMediaRequests must be an array.');
    }
    if (!Array.isArray(resource.recoveredActorEvents)) {
      fail(resource.resourceId + ' recoveredActorEvents must be an array.');
    }
    if (!Array.isArray(resource.recoveredNativeSpriteEffects)) {
      fail(resource.resourceId + ' recoveredNativeSpriteEffects must be an array.');
    }
    object(resource.source, resource.resourceId + ' source');
    integer(resource.source.z64PrefixStart, resource.resourceId + ' z64PrefixStart', 1);
    integer(resource.source.z64PayloadStart, resource.resourceId + ' z64PayloadStart', 1);
    integer(resource.source.z64PayloadEndExclusive,
      resource.resourceId + ' z64PayloadEndExclusive', 1);
    integer(resource.source.storedPayloadLength,
      resource.resourceId + ' storedPayloadLength', 1);
    integer(resource.source.decodedLength, resource.resourceId + ' decodedLength', 4);
    nonEmptyString(resource.source.decodedSha256, resource.resourceId + ' decodedSha256');
    if (resource.source.z64PayloadStart + resource.source.storedPayloadLength !==
        resource.source.z64PayloadEndExclusive) {
      fail(resource.resourceId + ' stored payload range is inconsistent.');
    }
  }

  function validateImageAsset(asset, seen) {
    object(asset, 'image asset');
    unique(asset.assetId, seen, 'image assetId');
    nonEmptyString(asset.sourceKind, asset.assetId + ' sourceKind');
    nonEmptyString(asset.displayName, asset.assetId + ' displayName');
    nonEmptyString(asset.container, asset.assetId + ' container');
    if (asset.renderable) {
      if (asset.unsupportedReason !== null) {
        fail(asset.assetId + ' renderable asset must not have an unsupported reason.');
      }
    } else {
      nonEmptyString(asset.unsupportedReason, asset.assetId + ' unsupportedReason');
    }
    object(asset.source, asset.assetId + ' source');
    if (asset.consumerEvidence != null) {
      object(asset.consumerEvidence, asset.assetId + ' consumerEvidence');
      nonEmptyString(asset.consumerEvidence.evidenceGrade,
        asset.assetId + ' consumer evidenceGrade');
      nonEmptyString(asset.consumerEvidence.consumerStatus,
        asset.assetId + ' consumer status');
      nonEmptyString(asset.consumerEvidence.firstMissingJoin,
        asset.assetId + ' first missing join');
    }
    if (asset.sourceKind === 'lha-archive') {
      integer(asset.archiveIndex, asset.assetId + ' archiveIndex');
      integer(asset.source.archiveHeaderZ64, asset.assetId + ' archiveHeaderZ64');
      integer(asset.source.decodedSize, asset.assetId + ' decodedSize', 1);
      nonEmptyString(asset.source.decodedSha256, asset.assetId + ' decodedSha256');
    } else if (asset.sourceKind === 'section-c-njpg') {
      if (asset.archiveIndex !== null) fail(asset.assetId + ' must not claim an LHA archive index.');
      integer(asset.source.z64Start, asset.assetId + ' z64Start');
      integer(asset.source.z64EndExclusive, asset.assetId + ' z64EndExclusive', 1);
      integer(asset.source.storedSize, asset.assetId + ' storedSize', 1);
      nonEmptyString(asset.source.storedSha256, asset.assetId + ' storedSha256');
      nonEmptyString(asset.source.normalizedZ64Sha256,
        asset.assetId + ' normalizedZ64Sha256');
      if (asset.source.z64EndExclusive - asset.source.z64Start !== asset.source.storedSize) {
        fail(asset.assetId + ' Section C ROM extent is inconsistent.');
      }
    } else if (asset.sourceKind === 'rom-resource') {
      if (asset.archiveIndex !== null) fail(asset.assetId + ' must not claim an LHA archive index.');
      integer(asset.source.resourceKey, asset.assetId + ' resourceKey', 1);
      integer(asset.source.sizeWordZ64, asset.assetId + ' sizeWordZ64', 1);
      integer(asset.source.z64Start, asset.assetId + ' z64Start', 1);
      integer(asset.source.z64EndExclusive, asset.assetId + ' z64EndExclusive', 1);
      integer(asset.source.storedSize, asset.assetId + ' storedSize', 1);
      integer(asset.source.decodedSize, asset.assetId + ' decodedSize', 1);
      nonEmptyString(asset.source.storedSha256, asset.assetId + ' storedSha256');
      nonEmptyString(asset.source.decodedSha256, asset.assetId + ' decodedSha256');
      nonEmptyString(asset.source.compressionKind, asset.assetId + ' compressionKind');
      if (asset.source.z64EndExclusive - asset.source.z64Start !== asset.source.storedSize ||
          asset.source.z64Start !== asset.source.sizeWordZ64 + 4) {
        fail(asset.assetId + ' ROM resource envelope is inconsistent.');
      }
    } else {
      fail(asset.assetId + ' uses unsupported sourceKind ' + asset.sourceKind + '.');
    }
  }

  function validateActorArtSource(asset, seenIds, seenBanks) {
    object(asset, 'Actor Art Source');
    unique(asset.assetId, seenIds, 'Actor Art Source assetId');
    integer(asset.bank, asset.assetId + ' bank', 1);
    if (seenBanks[asset.bank]) fail('Actor Art Source bank duplicates ' + asset.bank + '.');
    seenBanks[asset.bank] = true;
    nonEmptyString(asset.label, asset.assetId + ' label');
    nonEmptyString(asset.identityStatus, asset.assetId + ' identityStatus');
    nonEmptyString(asset.identityScope, asset.assetId + ' identityScope');
    integer(asset.stockActorChannelCount, asset.assetId + ' stockActorChannelCount');
    integer(asset.stockSceneUseCount, asset.assetId + ' stockSceneUseCount');
    if (!Array.isArray(asset.stockSceneIds) ||
        asset.stockSceneIds.length !== asset.stockSceneUseCount) {
      fail(asset.assetId + ' stock scene-use metadata is inconsistent.');
    }
    if (!Array.isArray(asset.animationKeys) || !Array.isArray(asset.facings)) {
      fail(asset.assetId + ' selectors must be arrays.');
    }
    integer(asset.descriptorKey, asset.assetId + ' descriptorKey', 1);
    integer(asset.descriptorMemberCount, asset.assetId + ' descriptorMemberCount', 5);
    integer(asset.artCount, asset.assetId + ' artCount', 1);
    if (asset.descriptorMemberCount !== asset.artCount + 4) {
      fail(asset.assetId + ' descriptor controls and art count are inconsistent.');
    }
    ['metadataKey', 'poseKey', 'configKey', 'lookupKey'].forEach(function(field) {
      integer(asset[field], asset.assetId + ' ' + field, 1);
    });
    integer(asset.lookupBankCount, asset.assetId + ' lookupBankCount', 1);
    integer(asset.selectedChildOrdinal, asset.assetId + ' selectedChildOrdinal');
    integer(asset.poseCount, asset.assetId + ' poseCount');
    integer(asset.renderablePoseCount, asset.assetId + ' renderablePoseCount');
    integer(asset.physicalProgramCount, asset.assetId + ' physicalProgramCount');
    if (asset.renderablePoseCount > asset.poseCount) {
      fail(asset.assetId + ' renderable pose count exceeds its selector count.');
    }
  }

  function validatePoseProgram(program, seen) {
    object(program, 'pose program');
    unique(program.programId, seen, 'pose programId');
    nonEmptyString(program.poseId, program.programId + ' poseId');
    integer(program.bank, program.programId + ' bank', 1);
    integer(program.animationKey, program.programId + ' animationKey');
    integer(program.facing, program.programId + ' facing');
    integer(program.stateIndex, program.programId + ' stateIndex');
    if (program.variant !== null) integer(program.variant, program.programId + ' variant');
    if (program.descriptorSlot !== null) {
      integer(program.descriptorSlot, program.programId + ' descriptorSlot');
    }
    if (typeof program.sourceProgramDefined !== 'boolean' ||
        !Array.isArray(program.sourceProgramIdentityIds)) {
      fail(program.programId + ' source-program provenance is invalid.');
    }
    if (typeof program.emptyProgram !== 'boolean') {
      fail(program.programId + ' empty-program disposition is invalid.');
    }
    nonEmptyString(program.executionStatus, program.programId + ' executionStatus');
    integer(program.durationFrames, program.programId + ' durationFrames');
    integer(program.alternativeProgramCount, program.programId + ' alternativeProgramCount', 1);
    integer(program.selectedChildOrdinal, program.programId + ' selectedChildOrdinal');
    if (!Array.isArray(program.frames) || !Array.isArray(program.controlOpcodes)) {
      fail(program.programId + ' frame and control metadata must be arrays.');
    }
    if (program.emptyProgram !== (!program.frames.length && !program.controlOpcodes.length)) {
      fail(program.programId + ' empty-program disposition disagrees with its records.');
    }
    program.frames.forEach(function(frame, index) {
      object(frame, program.programId + ' frame ' + index);
      integer(frame.frameToken, program.programId + ' frameToken');
      integer(frame.durationFrames, program.programId + ' frame duration');
    });
  }

  function validatePoseSelector(selector, seenIds, seenRuntimeKeys) {
    object(selector, 'pose selector');
    unique(selector.selectorId, seenIds, 'pose selectorId');
    nonEmptyString(selector.poseId, selector.selectorId + ' poseId');
    integer(selector.bank, selector.selectorId + ' bank', 1);
    integer(selector.animationKey, selector.selectorId + ' animationKey');
    integer(selector.facing, selector.selectorId + ' facing');
    integer(selector.stateIndex, selector.selectorId + ' stateIndex');
    nonEmptyString(selector.physicalStateId, selector.selectorId + ' physicalStateId');
    var runtimeKey = [selector.bank, selector.animationKey, selector.facing].join(':');
    unique(runtimeKey, seenRuntimeKeys, 'pose runtime selector');
  }

  function validateDialogueArchive(archive, seenArchives, seenEntries) {
    object(archive, 'dialogue archive');
    unique(archive.archiveId, seenArchives, 'dialogue archiveId');
    integer(archive.archiveIndex, archive.archiveId + ' archiveIndex');
    nonEmptyString(archive.presentationFamily, archive.archiveId + ' presentationFamily');
    if (archive.presentationFamily !== 'serifu' && archive.presentationFamily !== 'meswin') {
      fail(archive.archiveId + ' has an unsupported presentationFamily.');
    }
    integer(archive.presentationFamilyBits,
      archive.archiveId + ' presentationFamilyBits');
    integer(archive.presentationSelector, archive.archiveId + ' presentationSelector');
    integer(archive.presentationResourceKey,
      archive.archiveId + ' presentationResourceKey', 1);
    if (archive.presentationFamily === 'serifu' && archive.presentationFamilyBits !== 0) {
      fail(archive.archiveId + ' Serifu presentationFamilyBits must be zero.');
    }
    if (archive.presentationFamily === 'meswin') {
      if (archive.archiveIndex !== 815 || archive.presentationFamilyBits !== 0x80 ||
          archive.presentationSelector !== 0 ||
          archive.presentationResourceKey !== 0x021B8BA4 ||
          archive.presentationLeafResourceKey !== 0x021B8BAC) {
        fail(archive.archiveId + ' MESWIN presentation route is inconsistent.');
      }
    }
    nonEmptyString(archive.filename, archive.archiveId + ' filename');
    integer(archive.entryCount, archive.archiveId + ' entryCount', 1);
    if (!Array.isArray(archive.entries) || archive.entries.length !== archive.entryCount) {
      fail(archive.archiveId + ' dialogue entry count is inconsistent.');
    }
    object(archive.source, archive.archiveId + ' source');
    integer(archive.source.archiveHeaderZ64, archive.archiveId + ' archiveHeaderZ64', 1);
    integer(archive.source.decodedSize, archive.archiveId + ' decodedSize', 1);
    nonEmptyString(archive.source.decodedSha256, archive.archiveId + ' decodedSha256');
    archive.entries.forEach(function(entry) {
      object(entry, archive.archiveId + ' entry');
      unique(entry.entryId, seenEntries, 'dialogue entryId');
      if (entry.archiveIndex !== archive.archiveIndex) {
        fail(entry.entryId + ' references the wrong dialogue archive.');
      }
      integer(entry.entryIndex, entry.entryId + ' entryIndex');
      integer(entry.displayEntryNumber, entry.entryId + ' displayEntryNumber', 1);
      integer(entry.sourceOffset, entry.entryId + ' sourceOffset');
      integer(entry.sourceLength, entry.entryId + ' sourceLength');
      if (typeof entry.text !== 'string' || typeof entry.rawText !== 'string') {
        fail(entry.entryId + ' dialogue text must be strings.');
      }
      if (entry.speakerLabel !== null && typeof entry.speakerLabel !== 'string') {
        fail(entry.entryId + ' speakerLabel must be null or a string.');
      }
    });
  }

  function validateAudioBlock(block, seen) {
    object(block, 'sequenced-audio block');
    unique(block.blockId, seen, 'sequenced-audio blockId');
    integer(block.blockIndex, block.blockId + ' blockIndex');
    integer(block.nativeRequestValue, block.blockId + ' nativeRequestValue');
    integer(block.channels, block.blockId + ' channels', 1);
    integer(block.runtimeChannelStride, block.blockId + ' runtimeChannelStride', 1);
    integer(block.storedSize, block.blockId + ' storedSize', 1);
    object(block.source, block.blockId + ' source');
    integer(block.source.z64Start, block.blockId + ' z64Start', 1);
    integer(block.source.z64EndExclusive, block.blockId + ' z64EndExclusive', 1);
    if (block.source.z64EndExclusive - block.source.z64Start !== block.storedSize) {
      fail(block.blockId + ' ROM extent is inconsistent.');
    }
  }

  function validateRegisteredAudioRequest(request, seen) {
    object(request, 'registered audio request');
    unique(request.requestAssetId, seen, 'registered audio requestAssetId');
    integer(request.requestId, request.requestAssetId + ' requestId');
    object(request.source, request.requestAssetId + ' source');
    ['indexRecordZ64', 'payloadOffset', 'z64Start', 'z64EndExclusive'].forEach(function(field) {
      integer(request.source[field], request.requestAssetId + ' ' + field);
    });
    if (request.source.z64EndExclusive <= request.source.z64Start) {
      fail(request.requestAssetId + ' has an empty audio payload envelope.');
    }
  }

  function validateDirectorEvent(event, seen) {
    object(event, 'Director event');
    unique(event.eventId, seen, 'Director eventId');
    nonEmptyString(event.opcode, event.eventId + ' opcode');
    nonEmptyString(event.family, event.eventId + ' family');
    nonEmptyString(event.label, event.eventId + ' label');
    integer(event.occurrenceCount, event.eventId + ' occurrenceCount');
    integer(event.sceneCount, event.eventId + ' sceneCount');
  }

  function validateBackgroundSelectorTable(table, seen) {
    object(table, 'background selector table');
    unique(table.tableId, seen, 'background selector tableId');
    nonEmptyString(table.label, table.tableId + ' label');
    nonEmptyString(table.owner, table.tableId + ' owner');
    integer(table.tableResourceKey, table.tableId + ' tableResourceKey', 1);
    integer(table.tableEntryZ64, table.tableId + ' tableEntryZ64', 1);
    integer(table.entryCount, table.tableId + ' entryCount', 1);
    if (!Array.isArray(table.entries) || table.entries.length !== table.entryCount) {
      fail(table.tableId + ' entry count is inconsistent.');
    }
    table.entries.forEach(function(entry, index) {
      object(entry, table.tableId + ' entry ' + index);
      if (entry.selector !== index) fail(table.tableId + ' selectors must be contiguous.');
      if (!Array.isArray(entry.archiveAssetIds)) {
        fail(table.tableId + ' entry ' + index + ' archiveAssetIds must be an array.');
      }
      if (entry.members != null) {
        if (!Array.isArray(entry.members) || entry.members.length !== entry.archiveAssetIds.length) {
          fail(table.tableId + ' entry ' + index + ' ordered members are inconsistent.');
        }
        entry.members.forEach(function(member, memberIndex) {
          object(member, table.tableId + ' entry ' + index + ' member ' + memberIndex);
          integer(member.ordinal, table.tableId + ' member ordinal');
          nonEmptyString(member.assetId, table.tableId + ' member assetId');
          if (member.assetId !== entry.archiveAssetIds[memberIndex]) {
            fail(table.tableId + ' entry ' + index + ' member order is inconsistent.');
          }
        });
      }
    });
  }

  function validateData(data, options) {
    options = options || {};
    object(data, 'Cutscene catalog');
    if (data.format !== FORMAT) fail('Unsupported Cutscene catalog format.');
    if (data.schemaVersion !== SCHEMA_VERSION) {
      fail('Unsupported Cutscene catalog schema version ' + data.schemaVersion + '.');
    }
    if (!Array.isArray(data.scenes) || !data.scenes.length) fail('Cutscene catalog has no scenes.');
    if (!Array.isArray(data.presentationScenes)) {
      fail('Cutscene catalog presentationScenes must be an array.');
    }
    if (!Array.isArray(data.partialDirectorResources)) {
      fail('Cutscene catalog partialDirectorResources must be an array.');
    }
    if (!Array.isArray(data.imageAssets)) fail('Cutscene catalog imageAssets must be an array.');
    if (!Array.isArray(data.actorArtSources) || !Array.isArray(data.poseSelectors) ||
        !Array.isArray(data.posePrograms)) {
      fail('Cutscene catalog Actor Art Sources, poseSelectors, and posePrograms must be arrays.');
    }
    if (!Array.isArray(data.dialogueArchives) ||
        !Array.isArray(data.serifuPresentationSelectors) ||
        !Array.isArray(data.audioBlocks) ||
        !Array.isArray(data.registeredAudioRequests) || !Array.isArray(data.directorEvents) ||
        !Array.isArray(data.backgroundSelectorTables)) {
      fail('Cutscene catalog dialogue, audio, and Director event assets must be arrays.');
    }
    var seen = { sceneIds: {}, assetIds: {}, storageIds: {}, directorKeys: {} };
    data.scenes.forEach(function(scene) { validateScene(scene, seen); });
    data.presentationScenes.forEach(function(scene) { validatePresentationScene(scene, seen); });
    var partialDirectorIds = {};
    data.partialDirectorResources.forEach(function(resource) {
      validatePartialDirectorResource(resource, partialDirectorIds);
    });
    data.presentationScenes.forEach(function(scene) {
      var resourceId = scene.source.partialDirectorResourceId;
      if (resourceId && !partialDirectorIds[resourceId]) {
        fail(scene.assetId + ' references unknown Director Partial resource ' + resourceId + '.');
      }
    });
    var imageIds = {};
    data.imageAssets.forEach(function(asset) { validateImageAsset(asset, imageIds); });
    var backgroundTableIds = {};
    var backgroundTablesById = {};
    data.backgroundSelectorTables.forEach(function(table) {
      validateBackgroundSelectorTable(table, backgroundTableIds);
      backgroundTablesById[table.tableId] = table;
      table.entries.forEach(function(entry) {
        entry.archiveAssetIds.forEach(function(assetId) {
          if (!imageIds[assetId]) fail(table.tableId + ' references unknown image ' + assetId + '.');
        });
      });
    });
    data.scenes.concat(data.presentationScenes).forEach(function(scene) {
      scene.backgroundAssetIds.concat(scene.backgroundCandidateAssetIds).forEach(function(assetId) {
        if (!imageIds[assetId]) fail(scene.assetId + ' references unknown background ' + assetId + '.');
      });
      var observedLayers = scene.backgroundRuntimeObservation &&
        scene.backgroundRuntimeObservation.stageLayers || [];
      observedLayers.forEach(function(layer) {
        if (!imageIds[layer.assetId]) {
          fail(scene.assetId + ' references unknown observed Stage layer ' + layer.assetId + '.');
        }
      });
      if (!scene.launchProfile) return;
      scene.launchProfile.background.requests.forEach(function(request) {
        request.stageAssetIds.forEach(function(assetId) {
          if (!imageIds[assetId]) {
            fail(request.requestId + ' references unknown profiled Stage asset ' + assetId + '.');
          }
        });
        if (request.selectorTableId === null) {
          if (request.selector !== null || request.assetIds.length) {
            fail(request.requestId + ' unresolved route claims a selector or table asset.');
          }
          return;
        }
        var table = backgroundTablesById[request.selectorTableId];
        var entry = table && table.entries[request.selector];
        if (!entry) {
          fail(request.requestId + ' references an unknown background selector route.');
        }
        if (entry.archiveAssetIds.length !== request.assetIds.length ||
            entry.archiveAssetIds.some(function(assetId, index) {
              return assetId !== request.assetIds[index];
            })) {
          fail(request.requestId + ' profiled assets do not match the selector table.');
        }
      });
    });
    var artIds = {}, artBanks = {};
    data.actorArtSources.forEach(function(asset) {
      validateActorArtSource(asset, artIds, artBanks);
    });
    var selectorIds = {}, selectorRuntimeKeys = {};
    data.poseSelectors.forEach(function(selector) {
      validatePoseSelector(selector, selectorIds, selectorRuntimeKeys);
    });
    var programIds = {};
    data.posePrograms.forEach(function(program) { validatePoseProgram(program, programIds); });
    var dialogueArchiveIds = {}, dialogueEntryIds = {};
    data.dialogueArchives.forEach(function(archive) {
      validateDialogueArchive(archive, dialogueArchiveIds, dialogueEntryIds);
    });
    var serifuSelectorIds = {};
    data.serifuPresentationSelectors.forEach(function(selector, index) {
      object(selector, 'Serifu presentation selector ' + index);
      integer(selector.selector, 'Serifu presentation selector index');
      unique(String(selector.selector), serifuSelectorIds, 'Serifu presentation selector');
      if (selector.selector !== index) fail('Serifu presentation selector order is not contiguous.');
      if (selector.archiveId !== null && !dialogueArchiveIds[selector.archiveId]) {
        fail('Serifu presentation selector references unknown archive ' + selector.archiveId + '.');
      }
    });
    data.dialogueArchives.forEach(function(archive) {
      if (archive.presentationFamily !== 'serifu') return;
      var selector = data.serifuPresentationSelectors[archive.presentationSelector];
      if (!selector || selector.archiveId !== archive.archiveId) {
        fail(archive.archiveId + ' presentation selector does not round-trip.');
      }
    });
    data.scenes.concat(data.presentationScenes).forEach(function(scene) {
      scene.dialogueAssociations.forEach(function(association) {
        object(association, scene.assetId + ' dialogue association');
        if (!dialogueEntryIds[association.entryId]) {
          fail(scene.assetId + ' references unknown dialogue entry ' + association.entryId + '.');
        }
      });
    });
    var audioBlockIds = {};
    data.audioBlocks.forEach(function(block) { validateAudioBlock(block, audioBlockIds); });
    var registeredAudioIds = {};
    data.registeredAudioRequests.forEach(function(request) {
      validateRegisteredAudioRequest(request, registeredAudioIds);
    });
    data.scenes.concat(data.presentationScenes).forEach(function(scene) {
      scene.audioAssociations.forEach(function(association) {
        object(association, scene.assetId + ' audio association');
        if (association.audioBlockId && !audioBlockIds[association.audioBlockId]) {
          fail(scene.assetId + ' references unknown audio block ' + association.audioBlockId + '.');
        }
        if (association.registeredAudioRequestAssetId &&
            !registeredAudioIds[association.registeredAudioRequestAssetId]) {
          fail(scene.assetId + ' references unknown registered audio request ' +
            association.registeredAudioRequestAssetId + '.');
        }
      });
    });
    var directorEventIds = {};
    data.directorEvents.forEach(function(event) { validateDirectorEvent(event, directorEventIds); });
    if (data.directorEvents.length !== 153 || !data.counts ||
        data.counts.directorOpcodeDefinitions !== 153 ||
        data.counts.directorNodes !== 8451 || data.counts.directorWords !== 21927 ||
        data.counts.registeredDirectorWaits !== 464 ||
        data.counts.remainingDirectorGapWords !== 0) {
      fail('Corrected 153-command Director corpus counts are stale.');
    }
    var directorNodeCount = data.scenes.reduce(function(total, scene) {
      return total + scene.source.nodes.length;
    }, 0);
    var directorWordCount = data.scenes.reduce(function(total, scene) {
      return total + scene.source.decodedWordCount;
    }, 0);
    var registeredWaitCount = data.scenes.reduce(function(total, scene) {
      return total + scene.source.registeredWaits.length;
    }, 0);
    if (directorNodeCount !== data.counts.directorNodes ||
        directorWordCount !== data.counts.directorWords ||
        registeredWaitCount !== data.counts.registeredDirectorWaits) {
      fail('Corrected Director scene totals do not match the catalog.');
    }
    if (options.requireComplete !== false && data.scenes.length !== 60) {
      fail('US Rev 0 Cutscene catalog must contain all 60 physical director resources.');
    }
    if (data.counts && data.counts.scenes !== data.scenes.length) {
      fail('Cutscene catalog scene count is stale.');
    }
    if (data.counts && data.counts.presentationScenes !== data.presentationScenes.length) {
      fail('Cutscene catalog presentation scene count is stale.');
    }
    if (data.counts &&
        data.counts.partialDirectorResources !== data.partialDirectorResources.length) {
      fail('Cutscene catalog Director Partial resource count is stale.');
    }
    if (data.counts && data.counts.imageAssets !== data.imageAssets.length) {
      fail('Cutscene catalog image count is stale.');
    }
    if (data.counts && (data.counts.actorArtSources !== data.actorArtSources.length ||
        data.counts.posePhysicalStates !== data.poseSelectors.length ||
        data.counts.posePrograms !== data.posePrograms.length)) {
      fail('Cutscene catalog pose counts are stale.');
    }
    if (data.counts && (data.counts.dialogueArchives !== data.dialogueArchives.length ||
        data.counts.audioBlocks !== data.audioBlocks.length ||
        data.counts.registeredAudioRequests !== data.registeredAudioRequests.length)) {
      fail('Cutscene catalog dialogue or audio counts are stale.');
    }
    if (data.counts && data.counts.serifuPresentationSelectorSlots !==
        data.serifuPresentationSelectors.length) {
      fail('Cutscene catalog Serifu selector count is stale.');
    }
    if (data.counts && data.counts.backgroundSelectorTables !==
        data.backgroundSelectorTables.length) {
      fail('Cutscene catalog background selector-table count is stale.');
    }
    return data;
  }

  function displayName(scene) {
    return scene.friendlyName || scene.technicalName;
  }

  function normalizeSearch(value) {
    return String(value || '').trim().toLowerCase();
  }

  function sceneSearchText(scene) {
    return [
      displayName(scene), scene.technicalName, scene.canonicalScene,
      scene.directorKey, scene.assetId
    ].concat(scene.aliases || [], scene.aliasNames || []).join(' ').toLowerCase();
  }

  function imageSearchText(asset) {
    return [
      asset.displayName, asset.filename, asset.assetId, asset.archiveIndex,
      asset.family, asset.container
    ].join(' ').toLowerCase();
  }

  function createCatalog(data, options) {
    data = validateData(data, options);
    var directorScenes = data.scenes.slice();
    var presentationScenes = data.presentationScenes.slice();
    var partialDirectorResources = data.partialDirectorResources.slice();
    var scenes = directorScenes.concat(presentationScenes);
    var images = data.imageAssets.slice();
    var actorArtSources = data.actorArtSources.slice();
    var poseSelectors = data.poseSelectors.slice();
    var posePrograms = data.posePrograms.slice();
    var dialogueArchives = data.dialogueArchives.slice();
    var serifuPresentationSelectors = data.serifuPresentationSelectors.slice();
    var audioBlocks = data.audioBlocks.slice();
    var registeredAudioRequests = data.registeredAudioRequests.slice();
    var directorEvents = data.directorEvents.slice();
    var backgroundSelectorTables = data.backgroundSelectorTables.slice();
    var identities = {};
    scenes.forEach(function(scene) {
      [scene.sceneId, scene.assetId, scene.storageId, scene.directorKey,
        scene.canonicalScene].concat(scene.aliases || []).forEach(function(identity) {
        if (!identity) return;
        if (identities[identity] && identities[identity] !== scene) {
          fail('Scene identity ' + identity + ' resolves to multiple physical resources.');
        }
        identities[identity] = scene;
      });
    });
    var imageById = {};
    images.forEach(function(asset) { imageById[asset.assetId] = asset; });
    var artById = {}, artByBank = {};
    actorArtSources.forEach(function(asset) {
      artById[asset.assetId] = asset;
      artByBank[asset.bank] = asset;
    });
    var poseSelectorById = {}, poseSelectorByRuntimeKey = {}, poseSelectorsByBank = {};
    poseSelectors.forEach(function(selector) {
      poseSelectorById[selector.selectorId] = selector;
      poseSelectorByRuntimeKey[[selector.bank, selector.animationKey,
        selector.facing].join(':')] = selector;
      if (!poseSelectorsByBank[selector.bank]) poseSelectorsByBank[selector.bank] = [];
      poseSelectorsByBank[selector.bank].push(selector);
    });
    var poseById = {}, programById = {}, posesByBank = {}, poseByRuntimeSelector = {};
    posePrograms.forEach(function(program) {
      programById[program.programId] = program;
      if (!poseById[program.poseId] ||
          !poseById[program.poseId].frames.length && program.frames.length) {
        poseById[program.poseId] = program;
      }
      if (!posesByBank[program.bank]) posesByBank[program.bank] = [];
      posesByBank[program.bank].push(program);
      if (Number.isInteger(program.variant)) {
        var runtimeKey = [program.bank, program.animationKey,
          program.facing, program.variant].join(':');
        if (!poseByRuntimeSelector[runtimeKey]) poseByRuntimeSelector[runtimeKey] = program;
      }
    });
    var dialogueArchiveById = {}, dialogueArchiveByIndex = {}, dialogueEntryById = {};
    dialogueArchives.forEach(function(archive) {
      dialogueArchiveById[archive.archiveId] = archive;
      dialogueArchiveByIndex[archive.archiveIndex] = archive;
      archive.entries.forEach(function(entry) { dialogueEntryById[entry.entryId] = entry; });
    });
    var serifuArchiveBySelector = {};
    serifuPresentationSelectors.forEach(function(selector) {
      if (selector.archiveId) serifuArchiveBySelector[selector.selector] =
        dialogueArchiveById[selector.archiveId];
    });
    var audioBlockById = {};
    audioBlocks.forEach(function(block) { audioBlockById[block.blockId] = block; });
    var registeredAudioById = {}, registeredAudioByRequest = {};
    registeredAudioRequests.forEach(function(request) {
      registeredAudioById[request.requestAssetId] = request;
      registeredAudioByRequest[request.requestId] = request;
    });
    var directorEventByOpcode = {};
    directorEvents.forEach(function(event) { directorEventByOpcode[event.opcode.toUpperCase()] = event; });
    var backgroundTableById = {};
    backgroundSelectorTables.forEach(function(table) { backgroundTableById[table.tableId] = table; });
    var partialDirectorById = {};
    partialDirectorResources.forEach(function(resource) {
      partialDirectorById[resource.resourceId] = resource;
    });

    return {
      data: data,
      scenes: scenes,
      directorScenes: directorScenes,
      presentationScenes: presentationScenes,
      partialDirectorResources: partialDirectorResources,
      imageAssets: images,
      actorArtSources: actorArtSources,
      poseSelectors: poseSelectors,
      posePrograms: posePrograms,
      dialogueArchives: dialogueArchives,
      serifuPresentationSelectors: serifuPresentationSelectors,
      audioBlocks: audioBlocks,
      registeredAudioRequests: registeredAudioRequests,
      directorEvents: directorEvents,
      backgroundSelectorTables: backgroundSelectorTables,
      getScene: function(identity) { return identities[identity] || null; },
      getPartialDirectorResource: function(resourceId) {
        return partialDirectorById[resourceId] || null;
      },
      getImageAsset: function(assetId) { return imageById[assetId] || null; },
      getDialogueArchive: function(identity) {
        return dialogueArchiveById[identity] || dialogueArchiveByIndex[Number(identity)] || null;
      },
      getDialogueEntry: function(entryId) { return dialogueEntryById[entryId] || null; },
      getSerifuArchiveForPresentationSelector: function(selector) {
        return serifuArchiveBySelector[Number(selector)] || null;
      },
      getAudioBlock: function(blockId) { return audioBlockById[blockId] || null; },
      getRegisteredAudioRequest: function(identity) {
        return registeredAudioById[identity] || registeredAudioByRequest[Number(identity)] || null;
      },
      getDirectorEvent: function(opcode) {
        return directorEventByOpcode[String(opcode || '').toUpperCase()] || null;
      },
      getBackgroundSelectorTable: function(tableId) {
        return backgroundTableById[tableId] || null;
      },
      getBackgroundSelectorEntry: function(tableId, selector) {
        var table = backgroundTableById[tableId];
        selector = Number(selector);
        return table && Number.isInteger(selector) && selector >= 0 && selector < table.entries.length
          ? table.entries[selector] : null;
      },
      getActorArtSource: function(identity) {
        return artById[identity] || artByBank[Number(identity)] || null;
      },
      getPoseProgram: function(poseIdOrBank, animationKey, facing) {
        var poseId = arguments.length === 1 ? poseIdOrBank :
          'cutscene-pose:' + Number(poseIdOrBank) + ':' + Number(animationKey) + ':' + Number(facing);
        return poseById[poseId] || null;
      },
      getPoseProgramById: function(programId) { return programById[programId] || null; },
      getPoseSelector: function(bank, animationKey, facing) {
        return poseSelectorByRuntimeKey[[Number(bank), Number(animationKey),
          Number(facing)].join(':')] || null;
      },
      getPoseSelectorById: function(selectorId) {
        return poseSelectorById[selectorId] || null;
      },
      getPhysicalPoseProgram: function(bank, animationKey, facing, variantSelector) {
        return poseByRuntimeSelector[[Number(bank), Number(animationKey), Number(facing),
          Number(variantSelector)].join(':')] || poseById[
          'cutscene-pose:' + Number(bank) + ':' + Number(animationKey) + ':' + Number(facing)] || null;
      },
      poseProgramsForBank: function(bank, options) {
        options = options || {};
        var rows = (posesByBank[Number(bank)] || []).filter(function(program) {
          if (options.animationKey != null && program.animationKey !== Number(options.animationKey)) return false;
          if (options.facing != null && program.facing !== Number(options.facing)) return false;
          return true;
        });
        if (options.physical === true) return rows.slice();
        return rows.filter(function(program) { return poseById[program.poseId] === program; });
      },
      poseSelectionsForBank: function(bank) {
        bank = Number(bank);
        var programs = (posesByBank[bank] || []).filter(function(program) {
          return poseById[program.poseId] === program;
        });
        var represented = {};
        programs.forEach(function(program) { represented[program.poseId] = true; });
        var structural = (poseSelectorsByBank[bank] || []).filter(function(selector) {
          return !represented[selector.poseId];
        }).map(function(selector) {
          return {
            selectorOnly: true,
            selectorId: selector.selectorId,
            programId: null,
            poseId: selector.poseId,
            bank: selector.bank,
            animationKey: selector.animationKey,
            facing: selector.facing,
            stateIndex: selector.stateIndex,
            physicalStateId: selector.physicalStateId,
            durationFrames: 0,
            frames: [],
            previewCapability: 'needs-research',
            noProgramReason: selector.noProgramReason
          };
        });
        return programs.concat(structural).sort(function(left, right) {
          return left.animationKey - right.animationKey || left.facing - right.facing ||
            left.stateIndex - right.stateIndex || Number(left.selectorOnly) - Number(right.selectorOnly);
        });
      },
      searchScenes: function(query, filters) {
        filters = filters || {};
        var text = normalizeSearch(query);
        return scenes.filter(function(scene) {
          if (filters.engine && scene.engine !== filters.engine) return false;
          if (filters.actorBearing != null && scene.actorBearing !== !!filters.actorBearing) return false;
          if (filters.parseStatus && scene.parseStatus !== filters.parseStatus) return false;
          if (filters.capability && scene.previewCapability !== filters.capability &&
              scene.exportCapability !== filters.capability) return false;
          return !text || sceneSearchText(scene).indexOf(text) !== -1;
        });
      },
      searchImages: function(query, filters) {
        filters = filters || {};
        var text = normalizeSearch(query);
        return images.filter(function(asset) {
          if (filters.family && asset.family !== filters.family) return false;
          if (filters.container && asset.container !== filters.container) return false;
          if (filters.renderable != null && asset.renderable !== !!filters.renderable) return false;
          return !text || imageSearchText(asset).indexOf(text) !== -1;
        });
      }
    };
  }

  function createSceneDocument(scene) {
    var M = OB64.cutsceneModel;
    if (!M) fail('cutscene-model.js is required to create a SceneDocument.');
    var isDirector = scene.engine === 'director';
    var reviewedOverlay = scene.reviewedTimelineOverlay || null;
    var featuredSequence = reviewedOverlay && reviewedOverlay.featuredSequence || null;
    var identityAliases = (scene.aliases || []).concat(scene.aliasNames || [])
      .filter(function(alias, index, aliases) {
        return alias && aliases.indexOf(alias) === index;
      });
    var backgroundObservation = scene.backgroundRuntimeObservation || null;
    var observedStageLayers = backgroundObservation &&
      Array.isArray(backgroundObservation.stageLayers) &&
      backgroundObservation.stageLayers.length
      ? backgroundObservation.stageLayers : null;
    var launchBackgroundRequest = isDirector && scene.launchProfile &&
      scene.launchProfile.background.requests.find(function(request) {
        return request.stageAssetIds.length > 0;
      }) || null;
    var profiledStageLayers = launchBackgroundRequest
      ? (launchBackgroundRequest.stageLayers.length
        ? launchBackgroundRequest.stageLayers
        : launchBackgroundRequest.members.map(function(member, index) {
          return {
            assetId: member.assetId,
            role: index === 0 ? 'environment-base' : 'ordered-layer',
            depth: member.ordinal,
            evidenceStatus: launchBackgroundRequest.evidenceStatus,
            associationStatus: launchBackgroundRequest.status,
            sourceKind: 'launch-profile-selector'
          };
        })) : null;
    var initialBackgroundLayers = observedStageLayers || profiledStageLayers ||
      scene.backgroundAssetIds.map(
      function(assetId, index) {
        return {
          assetId: assetId,
          role: index === 0 ? 'environment-base' : 'ordered-layer',
          depth: index,
          evidenceStatus: 'runtime-observed',
          associationStatus: scene.backgroundAssetIds.length > 1
            ? 'exact load and display-list traversal order; final depth and blend semantics unresolved'
            : 'exact single-resource association'
        };
      });
    var initialBackgroundAssetId = initialBackgroundLayers.length
      ? initialBackgroundLayers[0].assetId : null;
    var document = M.createSceneDocument({
      identity: {
        sceneId: scene.sceneId,
        technicalName: scene.technicalName,
        friendlyName: scene.friendlyName,
        engine: scene.engine,
        sourceRevision: scene.sourceRevision,
        directorKey: scene.directorKey,
        aliases: identityAliases,
        triggerStatus: scene.triggerStatus,
        captures: backgroundObservation && Array.isArray(backgroundObservation.captures)
          ? JSON.parse(JSON.stringify(backgroundObservation.captures)) : []
      },
      background: {
        assetId: initialBackgroundAssetId,
        capability: initialBackgroundAssetId
          ? M.capabilities.PREVIEW_ONLY : M.capabilities.NEEDS_RESEARCH,
        layers: initialBackgroundLayers.map(function(stageLayer, index) {
          var assetId = stageLayer.assetId;
          return {
            id: index === 0 ? 'background:base' : 'background:layer:' + index,
            assetId: assetId,
            label: stageLayer.role || assetId,
            visible: true,
            depth: Number.isFinite(stageLayer.depth) ? stageLayer.depth : index,
            capability: M.capabilities.PREVIEW_ONLY,
            source: {
              sourceKind: stageLayer.sourceKind ||
                (stageLayer.evidenceStatus === 'runtime-observed'
                ? 'runtime-observed-scene-association'
                : 'capture-registered-scene-association'),
              role: stageLayer.role || null,
              evidenceStatus: stageLayer.evidenceStatus || 'runtime-observed',
              traversalOrdinal: index,
              compositingStatus: stageLayer.associationStatus || null
            }
          };
        }),
        projection: backgroundObservation && backgroundObservation.stageProjection
          ? JSON.parse(JSON.stringify(backgroundObservation.stageProjection))
          : { mode: initialBackgroundAssetId ? 'stage-fit' : 'unresolved' }
      },
      native: {
        sourceAssetId: scene.assetId,
        commands: [],
        gaps: (scene.source.gaps || []).slice()
      },
      exportRequirements: {
        capability: scene.exportCapability,
        reasons: [isDirector
          ? 'Load the matching ROM source before export.'
          : scene.source.adapterStatus],
        allocationBytes: 0,
        features: [isDirector ? 'director-fixed-slot' : 'presentation-adapter']
      }
    });
    scene.actors.forEach(function(sourceActor) {
      var reviewedActorLabel = featuredSequence &&
        featuredSequence.actorSlot === sourceActor.slot
        ? featuredSequence.actorLabel : null;
      M.addActor(document, M.createActor({
        id: sourceActor.actorId,
        label: sourceActor.label || reviewedActorLabel || 'Actor slot ' + sourceActor.slot,
        slot: sourceActor.slot,
        artSourceId: sourceActor.recordProducer && sourceActor.bank != null
          ? 'cutscene-art-bank:' + sourceActor.bank : null,
        capability: M.capabilities.NEEDS_RESEARCH,
        initial: {
          visible: sourceActor.recordProducer === true,
          x: sourceActor.x == null ? 0 : sourceActor.x,
          y: sourceActor.y == null ? 0 : sourceActor.y,
          z: sourceActor.z == null ? 0 : sourceActor.z,
          facing: sourceActor.facing == null ? 'unresolved' : 'native-' + sourceActor.facing,
          poseId: sourceActor.poseResolutionId
        },
        source: {
           catalogActorId: sourceActor.actorId,
           bank: sourceActor.bank,
           animationKey: sourceActor.animationKey,
           variantSelector: sourceActor.variantSelector,
           variantSelectorStatus: sourceActor.variantSelectorStatus,
           rawVariantSelector: sourceActor.rawVariantSelector,
           controlEntryAlias: sourceActor.controlEntryAlias || null,
           recordProducer: sourceActor.recordProducer,
           initializationStatus: sourceActor.initializationStatus,
           selectorStatus: sourceActor.selectorStatus,
           physicalStateId: sourceActor.physicalStateId,
           stateIndex: sourceActor.stateIndex,
           visibilityStatus: sourceActor.visibilityStatus,
           reviewedPresentation: reviewedActorLabel ? {
             overlayId: reviewedOverlay.overlayId,
             reviewStatus: reviewedOverlay.reviewStatus,
             actorLabel: reviewedActorLabel,
             initialSemanticLabel: featuredSequence.initialState &&
               featuredSequence.initialState.semanticLabel || null
           } : null
        }
      }));
    });
    return document;
  }

  OB64.cutsceneCatalog = {
    format: FORMAT,
    schemaVersion: SCHEMA_VERSION,
    CatalogError: CatalogError,
    validateData: validateData,
    createCatalog: createCatalog,
    createSceneDocument: createSceneDocument,
    displayName: displayName
  };
})(window.OB64);
