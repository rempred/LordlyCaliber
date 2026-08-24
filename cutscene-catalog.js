// Lordly Caliber - validated Cutscene Studio catalog access.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var FORMAT = 'ob64-cutscene-catalog';
  var SCHEMA_VERSION = 16;

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

  function validateNativeStagePropSource(source, label) {
    object(source, label + ' sprite source');
    nonEmptyString(source.bank, label + ' sprite bank');
    [
      'descriptorKey', 'descriptorMemberCount', 'metadataKey', 'poseKey',
      'configKey', 'lookupKey', 'metadataDecodedLength', 'poseDecodedLength',
      'configDecodedLength', 'lookupDecodedLength', 'artCount',
      'lookupBankCount', 'selectedChildOrdinal'
    ].forEach(function(field) {
      integer(source[field], label + ' sprite source ' + field,
        field === 'selectedChildOrdinal' || field === 'lookupDecodedLength' ||
          field === 'lookupBankCount' ? 0 : 1);
    });
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
    if (stageProps.foregroundSelector != null) {
      integer(stageProps.foregroundSelector, label + ' foreground selector', 0);
    }
    if (stageProps.source != null) {
      validateNativeStagePropSource(stageProps.source, label);
    }
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
        if (placement.descriptorHandle != null) {
          integer(placement.descriptorHandle, placementLabel + ' descriptor handle', 1);
        }
        if (placement.rowParameter != null && !Number.isInteger(placement.rowParameter)) {
          fail(placementLabel + ' row parameter must be an integer.');
        }
        if (placement.rawFields != null) {
          if (!Array.isArray(placement.rawFields) || placement.rawFields.some(function(value) {
            return !Number.isInteger(value);
          })) {
            fail(placementLabel + ' raw fields must be an integer array.');
          }
        }
        var placementSource = placement.source || stageProps.source;
        if (!placementSource) fail(placementLabel + ' lacks a sprite source.');
        validateNativeStagePropSource(placementSource, placementLabel);
      });
    var specialRows = stageProps.specialRows || [];
    if (!Array.isArray(specialRows)) fail(label + ' special rows must be an array.');
    specialRows.forEach(function(row, index) {
      var rowLabel = label + ' special row ' + index;
      object(row, rowLabel);
      nonEmptyString(row.id, rowLabel + ' id');
      nonEmptyString(row.tableKind, rowLabel + ' table kind');
      if (row.status !== -2) fail(rowLabel + ' must retain native status -2.');
      integer(row.descriptorHandle, rowLabel + ' descriptor handle', 1);
      if (!Array.isArray(row.rawFields) || row.rawFields.some(function(value) {
        return !Number.isInteger(value);
      })) {
        fail(rowLabel + ' raw fields must be an integer array.');
      }
      validateNativeStagePropSource(row.source, rowLabel);
      nonEmptyString(row.evidenceStatus, rowLabel + ' evidence status');
      nonEmptyString(row.renderStatus, rowLabel + ' render status');
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

  function validateExternalRequestProfile(request, label) {
    object(request, label);
    integer(request.decodedByteOffset, label + ' decoded byte offset', 0);
    integer(request.operand, label + ' operand', 0);
    integer(request.requestCode, label + ' request code', 0);
    if (request.operand > 0xFF || request.requestCode > 0xFFFF) {
      fail(label + ' exceeds its native field width.');
    }
    nonEmptyString(request.requestCodeStorage, label + ' request code storage');
    if (!Array.isArray(request.stateWrites)) {
      fail(label + ' state writes must be an array.');
    }
    request.stateWrites.forEach(function(write, index) {
      var writeLabel = label + ' state write ' + index;
      object(write, writeLabel);
      nonEmptyString(write.field, writeLabel + ' field');
      nonEmptyString(write.ramAddress, writeLabel + ' RAM address');
      integer(write.value, writeLabel + ' value', 0);
      if (write.value > 0xFF) fail(writeLabel + ' exceeds one byte.');
    });
    nonEmptyString(request.requestAcceptanceSignal,
      label + ' request acceptance signal');
    nonEmptyString(request.requestAcceptanceCondition,
      label + ' request acceptance condition');
    nonEmptyString(request.resumeTiming, label + ' resume timing');
    nonEmptyString(request.evidenceStatus, label + ' evidence status');
  }

  function validateTranslationWrite(write, label) {
    object(write, label);
    integer(write.eventDirectoryRow, label + ' event directory row', 0);
    nonEmptyString(write.eventResourceKey, label + ' event resource key');
    integer(write.eventEntryCursor, label + ' event entry cursor', 0);
    integer(write.decodedByteOffset, label + ' decoded byte offset', 0);
    integer(write.eventInvocationCursor, label + ' event invocation cursor', 0);
    integer(write.eventInvocationOffset, label + ' event invocation offset', 0);
    integer(write.precedingDirectorLaunchCount,
      label + ' preceding Director launch count', 0);
    if (write.eventInvocationCursor + write.eventInvocationOffset !==
        write.decodedByteOffset) {
      fail(label + ' invocation cursor does not reach its setter site.');
    }
    integer(write.opcode, label + ' opcode', 0x70);
    integer(write.operand, label + ' operand', 0xD0);
    integer(write.sourceRegister, label + ' source register', 0);
    integer(write.indexRegister, label + ' index register', 0);
    if (write.opcode > 0x77 || write.operand > 0xD7 ||
        write.sourceRegister !== (write.opcode & 7) ||
        write.indexRegister !== (write.operand & 7)) {
      fail(label + ' does not match the native event translation-setter dispatch.');
    }
    if (write.tableIndex !== null) {
      integer(write.tableIndex, label + ' table index', 0);
      if (write.tableIndex > 0xFF) fail(label + ' table index exceeds one byte.');
    }
    if (write.value !== null) {
      integer(write.value, label + ' replacement value', 0);
      if (write.value > 0xFFFF) fail(label + ' replacement exceeds one halfword.');
    }
    var expectedStatus = write.tableIndex === null
      ? 'table-index-unresolved'
      : (write.value === null ? 'replacement-value-unresolved' : 'exact');
    if (write.resolutionStatus !== expectedStatus) {
      fail(label + ' resolution status is inconsistent.');
    }
    nonEmptyString(write.evidenceStatus, label + ' evidence status');
  }

  function validateSubstitutionSource(source, label) {
    object(source, label);
    if (source.sourceId !== 'A' && source.sourceId !== 'B') {
      fail(label + ' source ID must be A or B.');
    }
    var expectedSemantic = source.sourceId === 'A'
      ? 'primary-class-id' : 'secondary-class-id';
    var expectedOffset = source.sourceId === 'A' ? 0x11 : 0x12;
    if (source.semantic !== expectedSemantic ||
        source.characterRecordBaseRamAddress !== '0x80193BC0' ||
        source.characterRecordFieldOffset !== expectedOffset ||
        source.characterRecordStride !== 56 || source.slotCount !== 5) {
      fail(label + ' does not match the native character-record source contract.');
    }
    nonEmptyString(source.storageRamRange, label + ' storage RAM range');
    nonEmptyString(source.getterFunctionZ64, label + ' getter function');
    nonEmptyString(source.setterFunctionZ64, label + ' setter function');
    nonEmptyString(source.evidenceStatus, label + ' evidence status');
  }

  function validateSubstitutionSourceWrite(write, label) {
    object(write, label);
    integer(write.eventDirectoryRow, label + ' event directory row', 0);
    nonEmptyString(write.eventResourceKey, label + ' event resource key');
    integer(write.eventEntryCursor, label + ' event entry cursor', 0);
    integer(write.decodedByteOffset, label + ' decoded byte offset', 0);
    integer(write.eventInvocationCursor, label + ' event invocation cursor', 0);
    integer(write.eventInvocationOffset, label + ' event invocation offset', 0);
    integer(write.precedingDirectorLaunchCount,
      label + ' preceding Director launch count', 0);
    if (write.eventInvocationCursor + write.eventInvocationOffset !==
        write.decodedByteOffset) {
      fail(label + ' invocation cursor does not reach its setter site.');
    }
    integer(write.opcode, label + ' opcode', 0x70);
    integer(write.operand, label + ' operand', 0xC0);
    integer(write.sourceRegister, label + ' source register', 0);
    integer(write.indexRegister, label + ' index register', 0);
    if (write.opcode > 0x77 || write.operand > 0xCF ||
        write.sourceRegister !== (write.opcode & 7) ||
        write.indexRegister !== (write.operand & 7)) {
      fail(label + ' does not match the native source-bank setter dispatch.');
    }
    var expectedSourceId = write.operand < 0xC8 ? 'A' : 'B';
    var expectedSemantic = expectedSourceId === 'A'
      ? 'primary-class-id' : 'secondary-class-id';
    var expectedOffset = expectedSourceId === 'A' ? 0x11 : 0x12;
    if (write.sourceId !== expectedSourceId ||
        write.sourceSemantic !== expectedSemantic ||
        write.characterRecordFieldOffset !== expectedOffset ||
        write.characterRecordStride !== 56) {
      fail(label + ' character-record source identity is inconsistent.');
    }
    if (write.sourceIndex !== null) {
      integer(write.sourceIndex, label + ' source index', 0);
      if (write.sourceIndex >= 5) fail(label + ' source index exceeds five slots.');
    }
    if (write.value !== null) {
      integer(write.value, label + ' source value', 0);
      if (write.value > 0xFF) fail(label + ' source value exceeds one byte.');
    }
    var expectedOrigin = write.value === null
      ? 'runtime-character-record-or-branch-dependent'
      : 'event-program-constant';
    if (write.sourceValueOrigin !== expectedOrigin) {
      fail(label + ' value origin is inconsistent.');
    }
    var expectedStatus = write.sourceIndex === null
      ? 'source-index-unresolved'
      : (write.value === null ? 'source-value-unresolved' : 'exact');
    if (write.resolutionStatus !== expectedStatus) {
      fail(label + ' resolution status is inconsistent.');
    }
    nonEmptyString(write.evidenceStatus, label + ' evidence status');
  }

  function validateOversizedImagePresentation(scene, profile, launchIds) {
    var presentation = profile.oversizedImagePresentation;
    var classFour = profile.launchContext && profile.launchContext.classId === 4;
    if (presentation === null) {
      if (classFour) {
        fail(scene.assetId + ' class-4 launch omits oversized-image ownership.');
      }
      return;
    }
    if (!classFour) {
      fail(scene.assetId + ' attaches oversized-image ownership outside launch class 4.');
    }
    object(presentation, scene.assetId + ' oversized-image presentation');
    if (presentation.active !== true ||
        ['director-launch-prescan-opcode-0x80000007',
          'event-property-0xE9-fallback',
          'event-property-0xE9-unresolved'].indexOf(presentation.source) === -1) {
      fail(scene.assetId + ' oversized-image source is unsupported.');
    }
    integer(presentation.contextCount,
      scene.assetId + ' oversized-image context count', 0);
    integer(presentation.exactContextCount,
      scene.assetId + ' oversized-image exact context count', 0);
    if (!Array.isArray(presentation.contexts) ||
        presentation.contexts.length !== presentation.contextCount) {
      fail(scene.assetId + ' oversized-image launch contexts are inconsistent.');
    }
    var exactContextCount = 0;
    presentation.contexts.forEach(function(context, index) {
      var label = scene.assetId + ' oversized-image context ' + index;
      object(context, label);
      nonEmptyString(context.launchId, label + ' launchId');
      if (!launchIds[context.launchId]) {
        fail(label + ' references an unknown parent event launch.');
      }
      integer(context.eventDirectoryRow, label + ' event directory row', 0);
      integer(context.eventEntryCursor, label + ' event entry cursor', 0);
      integer(context.eventInvocationCursor, label + ' event invocation cursor', 0);
      if (context.rowSelector !== null) {
        integer(context.rowSelector, label + ' row selector', 0);
        if (context.rowSelector >= 69) fail(label + ' row selector exceeds row 68.');
        exactContextCount += 1;
      }
    });
    if (exactContextCount !== presentation.exactContextCount) {
      fail(scene.assetId + ' oversized-image exact context count is stale.');
    }
    object(presentation.initialView,
      scene.assetId + ' oversized-image initial view');
    if (presentation.initialView.x !== 0 || presentation.initialView.y !== 0 ||
        presentation.initialView.scale !== 1 || presentation.initialView.zoomState !== 4) {
      fail(scene.assetId + ' oversized-image initial view is not the native initializer.');
    }
    nonEmptyString(presentation.evidenceStatus,
      scene.assetId + ' oversized-image evidence status');
    nonEmptyString(presentation.status,
      scene.assetId + ' oversized-image status');
    var structural = presentation.source ===
      'director-launch-prescan-opcode-0x80000007';
    if (structural) {
      nonEmptyString(presentation.sourceNodeId,
        scene.assetId + ' oversized-image source node');
      integer(presentation.wordStart,
        scene.assetId + ' oversized-image source word', 0);
      integer(presentation.rawRowSelector,
        scene.assetId + ' oversized-image raw row selector', 0);
    } else if (presentation.sourceNodeId !== null ||
        presentation.wordStart !== null || presentation.rawRowSelector !== null) {
      fail(scene.assetId + ' event-owned oversized-image selector claims a Director word.');
    }
    if (presentation.rowSelector === null) {
      if (presentation.mediaSelector !== null || presentation.childSelector !== null ||
          presentation.resourceKey !== null || presentation.archiveIndex !== null ||
          presentation.assetId !== null ||
          presentation.evidenceStatus !== 'launch-inputs-unresolved') {
        fail(scene.assetId + ' unresolved oversized-image launch claims native media.');
      }
      return;
    }
    integer(presentation.rowSelector,
      scene.assetId + ' oversized-image row selector', 0);
    integer(presentation.mediaSelector,
      scene.assetId + ' oversized-image media selector');
    integer(presentation.childSelector,
      scene.assetId + ' oversized-image child selector');
    integer(presentation.resourceKey,
      scene.assetId + ' oversized-image resource key', 1);
    integer(presentation.archiveIndex,
      scene.assetId + ' oversized-image archive index', 120);
    nonEmptyString(presentation.assetId,
      scene.assetId + ' oversized-image assetId');
    if (presentation.rowSelector >= 69 ||
        presentation.childSelector !== presentation.mediaSelector - 4 ||
        presentation.assetId !== 'archive:' + presentation.archiveIndex ||
        presentation.evidenceStatus !== 'native-static-exact') {
      fail(scene.assetId + ' oversized-image media projection is inconsistent.');
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
    object(profile.stageTransform, scene.assetId + ' launch Stage transform');
    object(profile.stageTransform.initial,
      scene.assetId + ' initial Stage transform');
    ['translateX', 'translateY', 'scaleX', 'scaleY'].forEach(function(field) {
      finiteNumber(profile.stageTransform.initial[field],
        scene.assetId + ' initial Stage transform ' + field);
    });
    nonEmptyString(profile.stageTransform.evidenceStatus,
      scene.assetId + ' launch Stage transform evidence');
    nonEmptyString(profile.stageTransform.status,
      scene.assetId + ' launch Stage transform status');
    if (!Array.isArray(profile.stageTransform.ownerFunctionsZ64)) {
      fail(scene.assetId + ' launch Stage transform owners must be an array.');
    }
    profile.stageTransform.ownerFunctionsZ64.forEach(function(owner, index) {
      nonEmptyString(owner, scene.assetId + ' launch Stage transform owner ' + index);
    });
    if (profile.directorMode.value === 2 &&
        (profile.stageTransform.evidenceStatus !==
          'native-static-mode-two-stage-initializers' ||
         profile.stageTransform.initial.translateX !== 0 ||
         profile.stageTransform.initial.translateY !== 0 ||
         profile.stageTransform.initial.scaleX !== 1 ||
         profile.stageTransform.initial.scaleY !== 1 ||
         profile.stageTransform.ownerFunctionsZ64.length !== 3)) {
      fail(scene.assetId + ' mode-two launch Stage transform is inconsistent.');
    }
    if (!Array.isArray(profile.parentEventLaunches)) {
      fail(scene.assetId + ' parent event launches must be an array.');
    }
    var launchIds = {};
    profile.parentEventLaunches.forEach(function(launch, index) {
      object(launch, scene.assetId + ' parent event launch ' + index);
      unique(launch.launchId, launchIds, scene.assetId + ' parent event launchId');
      integer(launch.eventDirectoryRow, launch.launchId + ' event directory row', 0);
      nonEmptyString(launch.eventResourceKey, launch.launchId + ' event resource key');
      integer(launch.decodedByteOffset, launch.launchId + ' decoded byte offset', 0);
      integer(launch.eventEntryCursor, launch.launchId + ' event entry cursor', 0);
      integer(launch.eventEntryOffset, launch.launchId + ' event entry offset', 0);
      if (!Array.isArray(launch.eventEntryPaths) ||
          launch.eventEntryPaths.length === 0) {
        fail(launch.launchId + ' event entry paths must be a nonempty array.');
      }
      launch.eventEntryPaths.forEach(function(entryPath, pathIndex) {
        var pathLabel = launch.launchId + ' event entry path ' + pathIndex;
        object(entryPath, pathLabel);
        if (entryPath.kind !== 'nested-scheduler-sequence' &&
            entryPath.kind !== 'direct-outer-sequence') {
          fail(pathLabel + ' has an unsupported kind.');
        }
        if (!Array.isArray(entryPath.outerEntryIndexes) ||
            entryPath.outerEntryIndexes.length === 0) {
          fail(pathLabel + ' outer entry indexes must be a nonempty array.');
        }
        entryPath.outerEntryIndexes.forEach(function(entryIndex) {
          integer(entryIndex, pathLabel + ' outer entry index', 0);
        });
        if (entryPath.kind === 'nested-scheduler-sequence') {
          integer(entryPath.sequenceTableCursor,
            pathLabel + ' sequence table cursor', 0);
          if (!Array.isArray(entryPath.sequenceEntryIndexes) ||
              entryPath.sequenceEntryIndexes.length === 0) {
            fail(pathLabel + ' sequence entry indexes must be a nonempty array.');
          }
        } else if (entryPath.sequenceTableCursor !== null ||
            !Array.isArray(entryPath.sequenceEntryIndexes) ||
            entryPath.sequenceEntryIndexes.length !== 0) {
          fail(pathLabel + ' direct sequence must not identify a nested table entry.');
        }
        entryPath.sequenceEntryIndexes.forEach(function(entryIndex) {
          integer(entryIndex, pathLabel + ' sequence entry index', 0);
        });
      });
      if (!Array.isArray(launch.eventInvocationContexts) ||
          launch.eventInvocationContexts.length === 0) {
        fail(launch.launchId + ' event invocation contexts must be a nonempty array.');
      }
      launch.eventInvocationContexts.forEach(function(context, contextIndex) {
        var contextLabel = launch.launchId + ' event invocation context ' + contextIndex;
        object(context, contextLabel);
        integer(context.eventInvocationCursor,
          contextLabel + ' cursor', 0);
        integer(context.eventInvocationOffset,
          contextLabel + ' offset', 0);
        if (context.eventInvocationCursor + context.eventInvocationOffset !==
            launch.decodedByteOffset) {
          fail(contextLabel + ' does not end at its Director launch.');
        }
        integer(context.precedingDirectorLaunchCount,
          contextLabel + ' preceding Director launch count', 0);
        if (context.precedingDirectorLaunchCount > 0) {
          if (context.precedingDirectorLaunchOffset !== null) {
            integer(context.precedingDirectorLaunchOffset,
              contextLabel + ' preceding Director launch offset', 0);
          }
          if (context.precedingDirectorInvocationCursor !== null) {
            integer(context.precedingDirectorInvocationCursor,
              contextLabel + ' preceding Director invocation cursor', 0);
          }
          if (context.concurrentDirectorTickOffset !== null) {
            integer(context.concurrentDirectorTickOffset,
              contextLabel + ' concurrent Director tick offset', 1);
          }
        } else if (context.precedingDirectorLaunchOffset !== null ||
            context.precedingDirectorInvocationCursor !== null ||
            context.concurrentDirectorTickOffset !== null) {
          fail(contextLabel + ' has timing fields without a preceding Director request.');
        }
        if (context.precedingDirectorSelector !== null) {
          integer(context.precedingDirectorSelector,
            contextLabel + ' preceding Director selector', 0);
          if (context.precedingDirectorSelector >= 1693) {
            fail(contextLabel + ' preceding Director selector exceeds the retail table.');
          }
          nonEmptyString(context.precedingDirectorResourceKey,
            contextLabel + ' preceding Director resource key');
          nonEmptyString(context.precedingDirectorLaunchId,
            contextLabel + ' preceding Director launchId');
          nonEmptyString(context.concurrentDirectorSceneId,
            contextLabel + ' concurrent Director sceneId');
          nonEmptyString(context.concurrentDirectorAssetId,
            contextLabel + ' concurrent Director assetId');
          if (context.precedingDirectorLaunchCount === 0) {
            fail(contextLabel + ' has a predecessor selector without a prior launch.');
          }
          if (context.sceneStateRelation !==
              'previous-event-request-concurrent-scene-state') {
            fail(contextLabel + ' omits its concurrent scene-state relation.');
          }
        } else if (context.precedingDirectorResourceKey !== null ||
            context.precedingDirectorLaunchId !== null ||
            context.concurrentDirectorSceneId !== null ||
            context.concurrentDirectorAssetId !== null) {
          fail(contextLabel + ' has a partial concurrent Director owner.');
        } else if (context.sceneStateRelation !==
            'no-exact-previous-director-request') {
          fail(contextLabel + ' has an unsupported unresolved scene-state relation.');
        }
        if (!Array.isArray(context.launchTranslationTable) ||
            context.launchTranslationTable.length !== 17) {
          fail(contextLabel + ' launch translation table must contain 17 entries.');
        }
        context.launchTranslationTable.forEach(function(value, tableIndex) {
          if (value === null) return;
          integer(value, contextLabel + ' translation ' + tableIndex, 0);
          if (value > 0xFFFF) {
            fail(contextLabel + ' translation ' + tableIndex + ' exceeds a halfword.');
          }
        });
        if (!Array.isArray(context.eventPropertyValues)) {
          fail(contextLabel + ' event property values must be an array.');
        }
        var propertyOperands = {};
        context.eventPropertyValues.forEach(function(property, propertyIndex) {
          var propertyLabel = contextLabel + ' event property ' + propertyIndex;
          object(property, propertyLabel);
          integer(property.propertyOperand, propertyLabel + ' operand', 0);
          integer(property.value, propertyLabel + ' value', 0);
          if (property.propertyOperand > 0xFF || property.value > 0xFFFF) {
            fail(propertyLabel + ' exceeds its native field width.');
          }
          unique(String(property.propertyOperand), propertyOperands,
            contextLabel + ' event property operand');
        });
        if (context.launchFlagBit08 !== false) {
          fail(contextLabel + ' direct event launch flag bit 0x08 must be false.');
        }
        if (context.eventPropertyE6 !== null) {
          integer(context.eventPropertyE6, contextLabel + ' event property 0xE6', 0);
          if (context.eventPropertyE6 > 0xFFFF) {
            fail(contextLabel + ' event property 0xE6 exceeds a halfword.');
          }
        }
        [
          ['eventPropertyE9', 0xE9],
          ['eventPropertyFB', 0xFB],
          ['eventPropertyFC', 0xFC],
          ['eventPropertyFD', 0xFD]
        ].forEach(function(field) {
          var fieldValue = context[field[0]];
          var property = context.eventPropertyValues.find(function(row) {
            return row.propertyOperand === field[1];
          });
          if (fieldValue !== null) {
            integer(fieldValue, contextLabel + ' event property 0x' +
              field[1].toString(16).toUpperCase(), 0);
            if (fieldValue > 0xFFFF || !property || property.value !== fieldValue) {
              fail(contextLabel + ' event-property projection is inconsistent.');
            }
          } else if (property) {
            fail(contextLabel + ' omits a statically exact event-property projection.');
          }
        });
        if (context.scenarioKey !== context.eventPropertyE9 ||
            context.battleTerrain !== context.eventPropertyFC ||
            context.currentUnitSelector !== context.eventPropertyFD) {
          fail(contextLabel + ' derived-environment inputs are inconsistent.');
        }
        if (context.launchPreservationSnapshot !== null &&
            typeof context.launchPreservationSnapshot !== 'boolean') {
          fail(contextLabel + ' launch preservation snapshot must be boolean or null.');
        }
        if (context.eventPropertyE6 === null !==
            (context.launchPreservationSnapshot === null) ||
            context.eventPropertyE6 !== null &&
            context.launchPreservationSnapshot !== (context.eventPropertyE6 !== 0)) {
          fail(contextLabel + ' launch preservation snapshot is inconsistent.');
        }
        if (context.secondRosterUnitLeaderOnly !== null &&
            typeof context.secondRosterUnitLeaderOnly !== 'boolean') {
          fail(contextLabel + ' second-roster-unit leader limit must be boolean or null.');
        }
        if (context.eventPropertyE6 === null !==
            (context.secondRosterUnitLeaderOnly === null) ||
            context.eventPropertyE6 !== null &&
            context.secondRosterUnitLeaderOnly !==
              ((context.eventPropertyE6 & 0x8000) !== 0)) {
          fail(contextLabel + ' second-roster-unit leader limit is inconsistent.');
        }
        if (context.precedingExternalRequest !== null) {
          validateExternalRequestProfile(context.precedingExternalRequest,
            contextLabel + ' preceding external request');
          if (context.eventInvocationCursor !==
              context.precedingExternalRequest.decodedByteOffset + 2) {
            fail(contextLabel + ' does not resume after its external request.');
          }
        }
        nonEmptyString(context.evidenceStatus, contextLabel + ' evidence status');
      });
      integer(launch.directorSelector, launch.launchId + ' Director selector', 0);
      nonEmptyString(launch.directorResourceKey,
        launch.launchId + ' Director resource key');
      nonEmptyString(launch.evidenceStatus, launch.launchId + ' evidence status');
      if (scene.source && Array.isArray(scene.source.directorSelectorRows) &&
          scene.source.directorSelectorRows.indexOf(launch.directorSelector) === -1) {
        fail(launch.launchId + ' does not select this Director resource.');
      }
    });
    object(profile.operandTranslation, scene.assetId + ' launch operand translation');
    var translation = profile.operandTranslation;
    if (typeof translation.required !== 'boolean') {
      fail(scene.assetId + ' launch operand translation required flag must be boolean.');
    }
    integer(translation.placeholderCount,
      scene.assetId + ' launch translation placeholder count');
    if (!Array.isArray(translation.tableIndexes) ||
        !Array.isArray(translation.occurrences) ||
        translation.occurrences.length !== translation.placeholderCount) {
      fail(scene.assetId + ' launch operand translation inventory is inconsistent.');
    }
    nonEmptyString(translation.evidenceStatus,
      scene.assetId + ' launch translation evidence status');
    nonEmptyString(translation.status, scene.assetId + ' launch translation status');
    var translationIndexes = {};
    translation.tableIndexes.forEach(function(tableIndex) {
      integer(tableIndex, scene.assetId + ' launch translation table index', 0);
      if (tableIndex > 255) {
        fail(scene.assetId + ' launch translation table index exceeds 255.');
      }
      unique(String(tableIndex), translationIndexes,
        scene.assetId + ' launch translation table index');
    });
    translation.occurrences.forEach(function(occurrence, index) {
      object(occurrence, scene.assetId + ' launch translation occurrence ' + index);
      nonEmptyString(occurrence.nodeId,
        scene.assetId + ' launch translation occurrence nodeId');
      integer(occurrence.wordStart,
        scene.assetId + ' launch translation occurrence wordStart', 0);
      integer(occurrence.wordOffset,
        scene.assetId + ' launch translation occurrence wordOffset', 0);
      integer(occurrence.sourceWord,
        scene.assetId + ' launch translation occurrence sourceWord', 0);
      integer(occurrence.tableIndex,
        scene.assetId + ' launch translation occurrence tableIndex', 0);
      if (occurrence.tableIndex > 255) {
        fail(scene.assetId + ' launch translation occurrence tableIndex exceeds 255.');
      }
      nonEmptyString(occurrence.operandRole,
        scene.assetId + ' launch translation occurrence operandRole');
      if (!translationIndexes[String(occurrence.tableIndex)] ||
          occurrence.sourceWord !== occurrence.wordStart + occurrence.wordOffset) {
        fail(scene.assetId + ' launch translation occurrence is inconsistent.');
      }
    });
    if (!Array.isArray(translation.launchContexts)) {
      fail(scene.assetId + ' launch translation contexts must be an array.');
    }
    integer(translation.resolvedContextCount,
      scene.assetId + ' resolved launch translation context count', 0);
    integer(translation.unresolvedContextCount,
      scene.assetId + ' unresolved launch translation context count', 0);
    var resolvedTranslationContexts = 0;
    translation.launchContexts.forEach(function(context, contextIndex) {
      var contextLabel = scene.assetId + ' launch translation context ' + contextIndex;
      object(context, contextLabel);
      nonEmptyString(context.launchId, contextLabel + ' launchId');
      if (!launchIds[context.launchId]) {
        fail(contextLabel + ' references an unknown parent event launch.');
      }
      integer(context.eventInvocationCursor, contextLabel + ' invocation cursor', 0);
      integer(context.precedingDirectorLaunchCount,
        contextLabel + ' preceding Director launch count', 0);
      if (!Array.isArray(context.tableValues) ||
          context.tableValues.length !== translation.tableIndexes.length) {
        fail(contextLabel + ' table values do not match the translation indexes.');
      }
      var resolved = true;
      context.tableValues.forEach(function(value, valueIndex) {
        if (value === null) {
          resolved = false;
          return;
        }
        integer(value, contextLabel + ' table value ' + valueIndex, 0);
        if (value > 0xFFFF) fail(contextLabel + ' table value exceeds a halfword.');
      });
      if (resolved) resolvedTranslationContexts += 1;
      nonEmptyString(context.evidenceStatus, contextLabel + ' evidence status');
    });
    if (resolvedTranslationContexts !== translation.resolvedContextCount ||
        translation.launchContexts.length - resolvedTranslationContexts !==
          translation.unresolvedContextCount) {
      fail(scene.assetId + ' launch translation resolution totals are inconsistent.');
    }
    if (translation.required !== (translation.placeholderCount > 0) ||
        translation.required !== (translation.tableIndexes.length > 0)) {
      fail(scene.assetId + ' launch operand translation requirement is inconsistent.');
    }
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
    if (!Array.isArray(profile.background.inheritanceContexts)) {
      fail(scene.assetId + ' launch background inheritance contexts must be an array.');
    }
    var inheritedPresentation = profile.background.inheritedPresentation;
    if (inheritedPresentation !== null) {
      object(inheritedPresentation, scene.assetId + ' inherited Stage presentation');
      nonEmptyString(inheritedPresentation.presentationId,
        scene.assetId + ' inherited Stage presentationId');
      if (inheritedPresentation.sourceKind !== 'parent-event-predecessor' ||
          inheritedPresentation.sentinel !== -2) {
        fail(scene.assetId + ' inherited Stage must identify the native -2 predecessor route.');
      }
      integer(inheritedPresentation.contextCount,
        scene.assetId + ' inherited Stage context count', 1);
      integer(inheritedPresentation.lineageDepth,
        scene.assetId + ' inherited Stage lineage depth', 1);
      if (inheritedPresentation.contextCount !==
          profile.background.inheritanceContexts.length) {
        fail(scene.assetId + ' inherited Stage context count is inconsistent.');
      }
      if (!Array.isArray(inheritedPresentation.immediatePredecessorSceneIds) ||
          !Array.isArray(inheritedPresentation.immediatePredecessorResourceKeys) ||
          !Array.isArray(inheritedPresentation.rootRequestIds) ||
          !Array.isArray(inheritedPresentation.members) ||
          !Array.isArray(inheritedPresentation.assetIds) ||
          !Array.isArray(inheritedPresentation.stageLayers) ||
          !Array.isArray(inheritedPresentation.stageAssetIds) ||
          inheritedPresentation.stageAssetIds.length === 0) {
        fail(scene.assetId + ' inherited Stage ownership is incomplete.');
      }
      if (inheritedPresentation.members.length !==
          inheritedPresentation.assetIds.length ||
          inheritedPresentation.members.some(function(member, index) {
            return !member || member.assetId !== inheritedPresentation.assetIds[index];
          })) {
        fail(scene.assetId + ' inherited Stage member order is inconsistent.');
      }
      inheritedPresentation.immediatePredecessorSceneIds.forEach(function(sceneId) {
        nonEmptyString(sceneId, inheritedPresentation.presentationId + ' predecessor scene');
      });
      inheritedPresentation.immediatePredecessorResourceKeys.forEach(function(resourceKey) {
        nonEmptyString(resourceKey,
          inheritedPresentation.presentationId + ' predecessor resource');
      });
      inheritedPresentation.rootRequestIds.forEach(function(requestId) {
        nonEmptyString(requestId, inheritedPresentation.presentationId + ' root request');
      });
      nonEmptyString(inheritedPresentation.evidenceStatus,
        inheritedPresentation.presentationId + ' evidence status');
      nonEmptyString(inheritedPresentation.status,
        inheritedPresentation.presentationId + ' status');
    }
    profile.background.inheritanceContexts.forEach(function(context, contextIndex) {
      var contextLabel = scene.assetId + ' Stage inheritance context ' + contextIndex;
      object(context, contextLabel);
      nonEmptyString(context.launchId, contextLabel + ' launchId');
      if (!launchIds[context.launchId]) {
        fail(contextLabel + ' references an unknown parent event launch.');
      }
      integer(context.eventDirectoryRow, contextLabel + ' event directory row', 0);
      integer(context.eventInvocationCursor, contextLabel + ' invocation cursor', 0);
      integer(context.precedingDirectorLaunchCount,
        contextLabel + ' preceding Director launch count', 0);
      if (context.precedingDirectorSelector !== null) {
        integer(context.precedingDirectorSelector,
          contextLabel + ' preceding Director selector', 0);
        nonEmptyString(context.precedingDirectorResourceKey,
          contextLabel + ' preceding Director resource key');
        nonEmptyString(context.precedingSceneId,
          contextLabel + ' preceding sceneId');
      } else if (context.precedingDirectorResourceKey !== null ||
          context.precedingSceneId !== null) {
        fail(contextLabel + ' has partial predecessor ownership.');
      }
      if (context.presentationFingerprint !== null) {
        nonEmptyString(context.presentationFingerprint,
          contextLabel + ' presentation fingerprint');
      }
      if (['no-preceding-director', 'predecessor-stage-unresolved',
        'context-stage-resolved-launch-selection-required',
        'resolved-unanimous-stage'].indexOf(context.resolutionStatus) === -1) {
        fail(contextLabel + ' has an unsupported resolution status.');
      }
      if ((context.presentationFingerprint === null) !==
          (context.resolutionStatus === 'no-preceding-director' ||
            context.resolutionStatus === 'predecessor-stage-unresolved')) {
        fail(contextLabel + ' fingerprint and resolution status disagree.');
      }
      if (inheritedPresentation &&
          context.resolutionStatus !== 'resolved-unanimous-stage') {
        fail(contextLabel + ' disagrees with the inherited Stage presentation.');
      }
    });
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
      if (request.foregroundSelectorTableId !== null) {
        nonEmptyString(request.foregroundSelectorTableId,
          request.requestId + ' foreground selector table');
      }
      if (request.foregroundSelector !== null) {
        integer(request.foregroundSelector, request.requestId + ' foreground selector');
        if (request.foregroundSelectorTableId === null) {
          fail(request.requestId + ' foreground selector has no table.');
        }
      }
      nonEmptyString(request.selectorSource, request.requestId + ' selector source');
      if (!Array.isArray(request.environmentSelectorCandidates)) {
        fail(request.requestId + ' environment selector candidates must be an array.');
      }
      request.environmentSelectorCandidates.forEach(function(selector, candidateIndex) {
        integer(selector, request.requestId + ' environment selector candidate ' +
          candidateIndex, 0);
        if (selector >= 80) {
          fail(request.requestId + ' environment selector candidate is outside the 80 rows.');
        }
      });
      nonEmptyString(request.foregroundSelectorSource,
        request.requestId + ' foreground selector source');
      if (!Array.isArray(request.foregroundSelectorCandidates)) {
        fail(request.requestId + ' foreground selector candidates must be an array.');
      }
      request.foregroundSelectorCandidates.forEach(function(selector, candidateIndex) {
        integer(selector, request.requestId + ' foreground selector candidate ' + candidateIndex,
          0);
        if (selector >= 80) {
          fail(request.requestId + ' foreground selector candidate is outside the 80 rows.');
        }
      });
      nonEmptyString(request.foregroundStatus, request.requestId + ' foreground status');
      if (request.derivedEnvironment !== null) {
        if (request.commandOperand !== -1 ||
            request.selectorSource !== 'director-launch-prescan-derived-sentinel') {
          fail(request.requestId + ' attaches a derived profile to a non-derived request.');
        }
        validateModeTwoDerivedEnvironmentProfile(
          request.derivedEnvironment, request, launchIds);
      } else if (request.selectorSource === 'director-launch-prescan-derived-sentinel') {
        fail(request.requestId + ' omits its derived-environment profile.');
      }
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

    validateOversizedImagePresentation(scene, profile, launchIds);

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
    nonEmptyString(profile.roster.nativeActorInputSource,
      scene.assetId + ' native Actor-input source');
    integer(profile.roster.nativeActorInputRowCapacity,
      scene.assetId + ' native Actor-input row capacity', 1);
    integer(profile.roster.nativeUnitMemberCapacity,
      scene.assetId + ' native unit member capacity', 1);
    integer(profile.roster.nativeMaximumCurrentUnitRows,
      scene.assetId + ' native maximum current-unit rows', 1);
    integer(profile.roster.secondUnitLeaderOnlyPropertyMask,
      scene.assetId + ' second-unit leader-only property mask', 1);
    if (profile.roster.modeTwoForceInitialization !== true ||
        profile.roster.fixedOverlayForceInitialization !== false ||
        profile.roster.externalRosterDependency !== true) {
      fail(scene.assetId + ' native Actor-input ownership is inconsistent.');
    }

    var preservation = profile.launchPreservationSnapshot;
    object(preservation, scene.assetId + ' launch preservation snapshot');
    nonEmptyString(preservation.condition,
      scene.assetId + ' launch preservation snapshot condition');
    if (preservation.directEventLaunchFlagBit08 !== null &&
        preservation.directEventLaunchFlagBit08 !== false) {
      fail(scene.assetId + ' direct event launch flag bit 0x08 must be false or null.');
    }
    [
      'contextCount', 'exactContextCount', 'requiredContextCount',
      'notRequiredContextCount', 'unresolvedContextCount'
    ].forEach(function(field) {
      integer(preservation[field], scene.assetId + ' launch preservation ' + field, 0);
    });
    if (preservation.exactContextCount !== preservation.requiredContextCount +
        preservation.notRequiredContextCount ||
        preservation.contextCount !== preservation.exactContextCount +
        preservation.unresolvedContextCount ||
        preservation.contextCount !== profile.parentEventLaunches.reduce(
          function(total, launch) {
            return total + launch.eventInvocationContexts.length;
          }, 0)) {
      fail(scene.assetId + ' launch preservation snapshot counts are inconsistent.');
    }
    nonEmptyString(preservation.evidenceStatus,
      scene.assetId + ' launch preservation snapshot evidence');
    nonEmptyString(preservation.status,
      scene.assetId + ' launch preservation snapshot status');
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
    if (source.dynamicGrammar === true) {
      if (!Array.isArray(source.nodes) || source.nodes.length !== 0 ||
          !Array.isArray(source.registeredWaits) || source.registeredWaits.length !== 0 ||
          source.corpusNodeCount !== 0 || source.corpusRegisteredWaitCount !== 0) {
        fail(scene.assetId + ' runtime-tiled source must not embed Director boundaries.');
      }
      integer(source.runtimeNodeCount, scene.assetId + ' runtimeNodeCount', 1);
      integer(source.runtimeQueryCount, scene.assetId + ' runtimeQueryCount');
      if (source.decodedLength !== source.decodedWordCount * 4) {
        fail(scene.assetId + ' runtime-tiled decoded size is inconsistent.');
      }
      return;
    }
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
      object(request.launchPreloadRoute, request.requestId + ' launchPreloadRoute');
      object(request.mode2Route, request.requestId + ' mode2Route');
      object(request.nonMode2Route, request.requestId + ' nonMode2Route');
      if (typeof request.launchPreloadRoute.active !== 'boolean') {
        fail(request.requestId + ' launch preload active flag must be boolean.');
      }
      nonEmptyString(request.launchPreloadRoute.selectorTableId,
        request.requestId + ' launch preload selectorTableId');
      if (!Array.isArray(request.launchPreloadRoute.archiveAssetIds)) {
        fail(request.requestId + ' launch preload archiveAssetIds must be an array.');
      }
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
    if (!Array.isArray(program.frames) || !Array.isArray(program.controlOpcodes) ||
        !Array.isArray(program.records)) {
      fail(program.programId + ' frame, control, and record metadata must be arrays.');
    }
    if (program.emptyProgram !== (!program.frames.length && !program.controlOpcodes.length)) {
      fail(program.programId + ' empty-program disposition disagrees with its records.');
    }
    program.frames.forEach(function(frame, index) {
      object(frame, program.programId + ' frame ' + index);
      integer(frame.frameToken, program.programId + ' frameToken');
      integer(frame.durationFrames, program.programId + ' frame duration');
    });
    program.records.forEach(function(record, index) {
      object(record, program.programId + ' record ' + index);
      integer(record.ordinal, program.programId + ' record ordinal');
      integer(record.opcode, program.programId + ' record opcode', 0, 0x15);
      if (record.ordinal !== index || !Array.isArray(record.operands)) {
        fail(program.programId + ' record ' + index + ' has invalid physical ownership.');
      }
      record.operands.forEach(function(operand, operandIndex) {
        integer(operand, program.programId + ' record ' + index +
          ' operand ' + operandIndex, 0, 0xFF);
      });
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

  function validateDirectorGrammar(definition, seen) {
    object(definition, 'Director grammar definition');
    integer(definition.opcodeU32, definition.name + ' opcodeU32');
    unique(String(definition.opcodeU32), seen, 'Director grammar opcode');
    nonEmptyString(definition.name, 'Director grammar semantic name');
    nonEmptyString(definition.semanticSummary, definition.name + ' semantic summary');
    nonEmptyString(definition.confidence, definition.name + ' confidence');
    integer(definition.sourceWordSpan, definition.name + ' sourceWordSpan', 1);
    nonEmptyString(definition.widthKind, definition.name + ' widthKind');
    nonEmptyString(definition.nodeType, definition.name + ' nodeType');
    if (!Array.isArray(definition.operandRoles) ||
        definition.operandRoles.length !== definition.sourceWordSpan - 1) {
      fail(definition.name + ' operand roles do not match its source width.');
    }
    definition.operandRoles.forEach(function(role, index) {
      nonEmptyString(role, definition.name + ' operand role ' + index);
    });
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

  function validateModeTwoDerivedEnvironmentProfile(profile, request, launchIds) {
    object(profile, request.requestId + ' derived-environment profile');
    if (profile.mapperId !== 'unsigned-existing-context-loader') {
      fail(request.requestId + ' uses an unsupported derived-environment mapper.');
    }
    nonEmptyString(profile.mapperFunctionZ64,
      request.requestId + ' derived-environment mapper address');
    nonEmptyString(profile.selectorConversion,
      request.requestId + ' derived-environment selector conversion');
    integer(profile.contextCount, request.requestId + ' derived context count', 0);
    integer(profile.exactContextCount,
      request.requestId + ' exact derived context count', 0);
    integer(profile.unresolvedContextCount,
      request.requestId + ' unresolved derived context count', 0);
    if (!Array.isArray(profile.contexts) ||
        profile.contexts.length !== profile.contextCount ||
        profile.exactContextCount + profile.unresolvedContextCount !==
          profile.contextCount) {
      fail(request.requestId + ' derived-environment context totals are inconsistent.');
    }
    if (!Array.isArray(profile.environmentSelectorCandidates) ||
        !Array.isArray(profile.outOfRangeEnvironmentSelectorCandidates) ||
        !Array.isArray(profile.requiredInputs) || profile.requiredInputs.length !== 4) {
      fail(request.requestId + ' derived-environment inputs or candidates are incomplete.');
    }
    var candidates = {};
    profile.environmentSelectorCandidates.forEach(function(selector) {
      integer(selector, request.requestId + ' derived environment candidate', 0);
      if (selector >= 80) {
        fail(request.requestId + ' derived environment candidate exceeds the native table.');
      }
      unique(String(selector), candidates,
        request.requestId + ' derived environment candidate');
    });
    profile.outOfRangeEnvironmentSelectorCandidates.forEach(function(selector) {
      integer(selector, request.requestId + ' out-of-range derived candidate', 0);
      if (selector < 80) {
        fail(request.requestId + ' mislabeled an in-range derived candidate.');
      }
    });
    profile.requiredInputs.forEach(function(input, index) {
      nonEmptyString(input, request.requestId + ' derived required input ' + index);
    });
    var exactContexts = 0;
    var exactSelectors = {};
    profile.contexts.forEach(function(context, index) {
      var label = request.requestId + ' derived context ' + index;
      object(context, label);
      nonEmptyString(context.launchId, label + ' launchId');
      if (!launchIds[context.launchId]) {
        fail(label + ' references an unknown parent event launch.');
      }
      integer(context.eventDirectoryRow, label + ' event directory row', 0);
      integer(context.eventEntryCursor, label + ' event entry cursor', 0);
      integer(context.eventInvocationCursor, label + ' event invocation cursor', 0);
      if (context.mapperId !== profile.mapperId) {
        fail(label + ' mapper does not match its aggregate profile.');
      }
      object(context.inputs, label + ' inputs');
      ['scenarioKey', 'currentUnitSelector', 'battleTerrain', 'randomChoice',
        'unitRecordFlags', 'auxiliaryHighTerrainState'].forEach(function(field) {
        var value = context.inputs[field];
        if (value !== null) integer(value, label + ' input ' + field, 0);
      });
      if (!Array.isArray(context.environmentSelectorCandidates) ||
          !Array.isArray(context.outOfRangeEnvironmentSelectorCandidates)) {
        fail(label + ' candidate lists are missing.');
      }
      context.environmentSelectorCandidates.forEach(function(selector) {
        integer(selector, label + ' environment candidate', 0);
        if (selector >= 80) fail(label + ' environment candidate exceeds row 79.');
      });
      context.outOfRangeEnvironmentSelectorCandidates.forEach(function(selector) {
        integer(selector, label + ' out-of-range environment candidate', 80);
      });
      if (context.resolutionStatus === 'exact-native-mapper-result') {
        exactContexts += 1;
        integer(context.nativeEnvironmentNumber,
          label + ' native environment number', 1);
        integer(context.environmentSelector, label + ' environment selector', 0);
        if (context.environmentSelector !== context.nativeEnvironmentNumber - 1) {
          fail(label + ' does not apply the native one-based conversion.');
        }
        nonEmptyString(context.resolutionSource, label + ' resolution source');
        exactSelectors[String(context.environmentSelector)] = true;
      } else if (context.resolutionStatus === 'launch-inputs-unresolved') {
        if (context.nativeEnvironmentNumber !== null ||
            context.environmentSelector !== null || context.resolutionSource !== null) {
          fail(label + ' claims values despite unresolved launch inputs.');
        }
      } else {
        fail(label + ' has an unsupported resolution status.');
      }
      nonEmptyString(context.evidenceStatus, label + ' evidence status');
    });
    if (exactContexts !== profile.exactContextCount) {
      fail(request.requestId + ' exact derived context count is stale.');
    }
    if (profile.environmentSelector !== null) {
      integer(profile.environmentSelector,
        request.requestId + ' aggregate derived environment selector', 0);
      if (profile.exactContextCount !== profile.contextCount ||
          Object.keys(exactSelectors).length !== 1 ||
          !exactSelectors[String(profile.environmentSelector)]) {
        fail(request.requestId + ' aggregate derived selector is not unanimous.');
      }
    }
    if (request.environmentSelector !== profile.environmentSelector) {
      fail(request.requestId + ' does not apply its derived environment result.');
    }
    nonEmptyString(profile.evidenceStatus,
      request.requestId + ' derived-environment evidence status');
    nonEmptyString(profile.status, request.requestId + ' derived-environment status');
  }

  function validateModeTwoDerivedEnvironmentRules(rules) {
    object(rules, 'Mode-two derived-environment rules');
    nonEmptyString(rules.evidenceStatus,
      'Mode-two derived-environment evidence status');
    nonEmptyString(rules.selectorConversion,
      'Mode-two derived-environment selector conversion');
    object(rules.inputProperties, 'Mode-two derived-environment input properties');
    if (rules.inputProperties.scenarioKey !== 0xE9 ||
        rules.inputProperties.currentUnitSelector !== 0xFD ||
        rules.inputProperties.battleTerrain !== 0xFC) {
      fail('Mode-two derived-environment property ownership is stale.');
    }
    nonEmptyString(rules.randomChoiceSourceRam,
      'Mode-two derived-environment random source');
    if (!Array.isArray(rules.scenarioOverrides) ||
        rules.scenarioOverrides.length !== 11 ||
        !Array.isArray(rules.mappers) || rules.mappers.length !== 2) {
      fail('Mode-two derived-environment rule inventory is incomplete.');
    }
    rules.scenarioOverrides.forEach(function(rule, index) {
      var label = 'Mode-two derived-environment scenario override ' + index;
      object(rule, label);
      integer(rule.scenarioKey, label + ' scenario key', 0);
      integer(rule.currentUnitSelector, label + ' current unit selector', 0);
      integer(rule.nativeEnvironmentNumber, label + ' native environment number', 1);
      integer(rule.environmentSelector, label + ' environment selector', 0);
      if (rule.environmentSelector !== rule.nativeEnvironmentNumber - 1) {
        fail(label + ' does not apply the native one-based conversion.');
      }
    });
    var mapperIds = {};
    rules.mappers.forEach(function(mapper, index) {
      var label = 'Mode-two derived-environment mapper ' + index;
      object(mapper, label);
      nonEmptyString(mapper.mapperId, label + ' mapperId');
      unique(mapper.mapperId, mapperIds, 'Mode-two derived-environment mapperId');
      nonEmptyString(mapper.functionZ64, label + ' function address');
      integer(mapper.terrainTableZ64, label + ' terrain table address', 0);
      nonEmptyString(mapper.terrainTableSha256, label + ' terrain table hash');
      integer(mapper.scenarioTableZ64, label + ' scenario table address', 0);
      nonEmptyString(mapper.scenarioTableSha256, label + ' scenario table hash');
      if (!Array.isArray(mapper.terrainRows) || mapper.terrainRows.length !== 26 ||
          mapper.terrainRows.some(function(row) {
            return !Array.isArray(row) || row.length !== 4 ||
              row.some(function(value) {
                return !Number.isInteger(value) || value < 0 || value > 0xFF;
              });
          }) || !Array.isArray(mapper.scenarioValues) ||
          mapper.scenarioValues.length !== 62 ||
          mapper.scenarioValues.some(function(value) {
            return !Number.isInteger(value) || value < 0 || value > 0xFF;
          }) || typeof mapper.signedScenarioBytes !== 'boolean') {
        fail(label + ' lookup tables are malformed.');
      }
    });
    if (!mapperIds['signed-direct-loader'] ||
        !mapperIds['unsigned-existing-context-loader'] ||
        JSON.stringify(rules.mappers[0].terrainRows) !==
          JSON.stringify(rules.mappers[1].terrainRows) ||
        JSON.stringify(rules.mappers[0].scenarioValues) !==
          JSON.stringify(rules.mappers[1].scenarioValues)) {
      fail('Mode-two derived-environment mapper pairing is stale.');
    }
    nonEmptyString(rules.status, 'Mode-two derived-environment status');
  }

  function validateOversizedImagePresentationRules(rules) {
    object(rules, 'Oversized-image presentation rules');
    if (rules.classTableZ64 !== 0x000654A0 ||
        rules.classTableRowCount !== 69 || rules.classTableRowBytes !== 9 ||
        rules.classTableSelectorOffset !== 3 ||
        rules.rootResourceKey !== 0x018BD022 ||
        rules.rootPrefixZ64 !== 0x01E512A2 ||
        rules.rootPayloadZ64 !== 0x01E512A6) {
      fail('Oversized-image native table ownership is stale.');
    }
    nonEmptyString(rules.classTableSha256,
      'Oversized-image class table hash');
    nonEmptyString(rules.rootPayloadSha256,
      'Oversized-image resource-root hash');
    if (!Array.isArray(rules.children) || rules.children.length !== 41 ||
        !Array.isArray(rules.rows) || rules.rows.length !== 69) {
      fail('Oversized-image native selector inventory is incomplete.');
    }
    var nullChildren = [];
    rules.children.forEach(function(child, index) {
      var label = 'Oversized-image root child ' + index;
      object(child, label);
      integer(child.childSelector, label + ' selector', 0);
      if (child.childSelector !== index) {
        fail(label + ' does not preserve its native ordinal.');
      }
      if (child.assetId === null) {
        nullChildren.push(index);
        if (child.archiveIndex !== null || child.filename !== null ||
            child.resourceKey !== null || child.resourcePrefixZ64 !== null ||
            child.disposition !== 'native-null-child') {
          fail(label + ' has an inconsistent null-child record.');
        }
        return;
      }
      integer(child.archiveIndex, label + ' archive index', 120);
      nonEmptyString(child.assetId, label + ' assetId');
      nonEmptyString(child.filename, label + ' filename');
      integer(child.resourceKey, label + ' resource key', 1);
      integer(child.resourcePrefixZ64, label + ' resource prefix', 1);
      if (child.archiveIndex !== 120 + index -
          nullChildren.filter(function(nullIndex) { return nullIndex < index; }).length ||
          child.assetId !== 'archive:' + child.archiveIndex ||
          child.disposition !== 'exact-native-resource-child') {
        fail(label + ' does not map to the contiguous oversized-image archives.');
      }
    });
    if (JSON.stringify(nullChildren) !== JSON.stringify([0, 34, 37])) {
      fail('Oversized-image null-child ownership is stale.');
    }
    rules.rows.forEach(function(row, index) {
      var label = 'Oversized-image class row ' + index;
      object(row, label);
      integer(row.rowSelector, label + ' selector', 0);
      if (!Number.isInteger(row.mediaSelector) || !Number.isInteger(row.childSelector)) {
        fail(label + ' selectors must be signed integers.');
      }
      nonEmptyString(row.disposition, label + ' disposition');
      if (row.rowSelector !== index || row.childSelector !== row.mediaSelector - 4) {
        fail(label + ' does not apply the native minus-four child conversion.');
      }
      var child = row.childSelector >= 0 && row.childSelector < rules.children.length
        ? rules.children[row.childSelector] : null;
      if (!child) {
        if (row.assetId !== null || row.archiveIndex !== null ||
            row.resourceKey !== null || row.disposition !== 'child-selector-outside-root') {
          fail(label + ' claims an out-of-range resource child.');
        }
        return;
      }
      if (row.assetId !== child.assetId || row.archiveIndex !== child.archiveIndex ||
          row.resourceKey !== child.resourceKey || row.disposition !== child.disposition) {
        fail(label + ' does not match its native resource child.');
      }
    });
  }

  function validateSceneResourcePathPoint(point, label) {
    object(point, label);
    if (!Number.isInteger(point.linkedSpriteSlot) ||
        !Number.isInteger(point.x) || !Number.isInteger(point.y)) {
      fail(label + ' must contain signed integer link and coordinates.');
    }
  }

  function validateSceneResourcePath(path, index, seenIds, seenSlots, groupEntries) {
    var label = 'Scene resource path ' + index;
    object(path, label);
    unique(path.pathId, seenIds, label + ' pathId');
    integer(path.groupIndex, label + ' group index', 0);
    integer(path.entryIndex, label + ' entry index', 0);
    integer(path.groupResourceKey, label + ' group resource key', 1);
    nonEmptyString(path.status, label + ' status');
    integer(path.pointCount, label + ' point count', 0);
    var slot = path.groupIndex + ':' + path.entryIndex;
    unique(slot, seenSlots, label + ' group entry');
    if (path.pathId !== 'scene-resource-path:' + slot) {
      fail(label + ' pathId does not match its native group and entry.');
    }
    if (!groupEntries[path.groupIndex]) groupEntries[path.groupIndex] = [];
    groupEntries[path.groupIndex].push(path.entryIndex);
    if (path.resourceKey === null) {
      if (path.status !== 'empty-native-entry' || path.pointCount !== 0 ||
          path.nativeStoredHeading !== null || path.rotationDegrees !== null) {
        fail(label + ' empty entry claims decoded path data.');
      }
      return;
    }
    integer(path.resourceKey, label + ' resource key', 1);
    if (path.status !== 'native-static-path-heading' || path.pointCount < 2) {
      fail(label + ' populated entry lacks its native path heading.');
    }
    ['firstPoint', 'secondPoint', 'finalPoint'].forEach(function(field) {
      validateSceneResourcePathPoint(path[field], label + ' ' + field);
    });
    finiteNumber(path.nativeMeasuredLength, label + ' measured length', 0);
    integer(path.nativeDenseSampleCount, label + ' dense sample count', 3);
    finiteNumber(path.nativeHeadingDeltaX, label + ' heading delta X');
    finiteNumber(path.nativeHeadingDeltaY, label + ' heading delta Y');
    if (!Number.isInteger(path.nativeStoredHeading) ||
        path.nativeStoredHeading < -180 || path.nativeStoredHeading > 180 ||
        !Number.isInteger(path.rotationDegrees) || path.rotationDegrees < 0 ||
        path.rotationDegrees > 359 || path.rotationDegrees !==
          (path.nativeStoredHeading === 180 ? 0 : path.nativeStoredHeading + 180)) {
      fail(label + ' native heading conversion is invalid.');
    }
    ['z64PrefixStart', 'z64PayloadStart', 'storedLength', 'decodedLength'].forEach(
      function(field) { integer(path[field], label + ' ' + field, 1); });
    nonEmptyString(path.storedPayloadSha256, label + ' stored payload hash');
    nonEmptyString(path.decodedSha256, label + ' decoded hash');
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
        !Array.isArray(data.registeredAudioRequests) || !Array.isArray(data.directorGrammar) ||
        !Array.isArray(data.directorContinuationStreams) ||
        !Array.isArray(data.directorEvents) ||
        !Array.isArray(data.backgroundSelectorTables) ||
        !Array.isArray(data.parentEventExternalRequests) ||
        !Array.isArray(data.parentEventTranslationWrites) ||
        !Array.isArray(data.parentEventSubstitutionSources) ||
        !Array.isArray(data.parentEventSubstitutionSourceWrites) ||
        !Array.isArray(data.modeTwoStagePlacementProfiles) ||
        !Array.isArray(data.sceneResourcePaths)) {
      fail('Cutscene catalog dialogue, audio, and Director event assets must be arrays.');
    }
    validateModeTwoDerivedEnvironmentRules(data.modeTwoDerivedEnvironmentRules);
    validateOversizedImagePresentationRules(data.oversizedImagePresentationRules);
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
    data.oversizedImagePresentationRules.children.forEach(function(child) {
      if (child.assetId !== null && !imageIds[child.assetId]) {
        fail('Oversized-image root references unknown image ' + child.assetId + '.');
      }
    });
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
    var modeTwoStagePlacementSelectors = {};
    data.modeTwoStagePlacementProfiles.forEach(function(profile, index) {
      validateNativeSceneProps(profile, 'Mode-two Stage placement profile ' + index);
      integer(profile.foregroundSelector,
        'Mode-two Stage placement profile foreground selector', 0);
      if (profile.foregroundSelector !== index ||
          modeTwoStagePlacementSelectors[profile.foregroundSelector]) {
        fail('Mode-two Stage placement selectors must be unique and contiguous.');
      }
      modeTwoStagePlacementSelectors[profile.foregroundSelector] = true;
    });
    var sceneResourcePathIds = {};
    var sceneResourcePathSlots = {};
    var sceneResourcePathGroupEntries = {};
    data.sceneResourcePaths.forEach(function(path, index) {
      validateSceneResourcePath(path, index, sceneResourcePathIds,
        sceneResourcePathSlots, sceneResourcePathGroupEntries);
    });
    var sceneResourcePathGroups = Object.keys(sceneResourcePathGroupEntries)
      .map(Number).sort(function(left, right) { return left - right; });
    if (sceneResourcePathGroups.length !== 59 ||
        sceneResourcePathGroups.some(function(groupIndex, index) {
          if (groupIndex !== index) return true;
          var entries = sceneResourcePathGroupEntries[groupIndex]
            .slice().sort(function(left, right) { return left - right; });
          return entries.some(function(entryIndex, ordinal) {
            return entryIndex !== ordinal;
          });
        })) {
      fail('Scene resource-path groups must preserve all 59 native rows.');
    }
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
      var oversizedPresentation = scene.launchProfile.oversizedImagePresentation;
      if (oversizedPresentation && oversizedPresentation.assetId !== null) {
        if (!imageIds[oversizedPresentation.assetId]) {
          fail(scene.assetId + ' references unknown oversized image ' +
            oversizedPresentation.assetId + '.');
        }
        var oversizedRow = data.oversizedImagePresentationRules.rows[
          oversizedPresentation.rowSelector];
        if (!oversizedRow || oversizedRow.assetId !== oversizedPresentation.assetId ||
            oversizedRow.archiveIndex !== oversizedPresentation.archiveIndex ||
            oversizedRow.resourceKey !== oversizedPresentation.resourceKey) {
          fail(scene.assetId + ' oversized image does not match its native class row.');
        }
      }
      scene.launchProfile.background.requests.forEach(function(request) {
        request.stageAssetIds.forEach(function(assetId) {
          if (!imageIds[assetId]) {
            fail(request.requestId + ' references unknown profiled Stage asset ' + assetId + '.');
          }
        });
        if (request.selectorTableId === null || request.selector === null) {
          if (request.selector !== null || request.assetIds.length ||
              request.stageAssetIds.length) {
            fail(request.requestId + ' unresolved route claims a selector or table asset.');
          }
          return;
        }
        var table = backgroundTablesById[request.selectorTableId];
        var entry = table && table.entries[request.selector];
        if (!entry) {
          fail(request.requestId + ' references an unknown background selector route.');
        }
        var expectedAssetIds = entry.archiveAssetIds.slice();
        if (request.foregroundSelector !== null) {
          var foregroundTable = backgroundTablesById[request.foregroundSelectorTableId];
          var foregroundEntry = foregroundTable &&
            foregroundTable.entries[request.foregroundSelector];
          if (!foregroundEntry) {
            fail(request.requestId + ' references an unknown foreground selector route.');
          }
          expectedAssetIds = expectedAssetIds.concat(foregroundEntry.archiveAssetIds);
        }
        if (expectedAssetIds.length !== request.assetIds.length ||
            expectedAssetIds.some(function(assetId, index) {
              return assetId !== request.assetIds[index];
            })) {
          fail(request.requestId +
            ' profiled assets do not match the independent selector tables.');
        }
      });
      var inherited = scene.launchProfile.background.inheritedPresentation;
      if (inherited) {
        inherited.stageAssetIds.forEach(function(assetId) {
          if (!imageIds[assetId]) {
            fail(inherited.presentationId +
              ' references unknown inherited Stage asset ' + assetId + '.');
          }
        });
        var inheritedTable = backgroundTablesById[inherited.selectorTableId];
        var inheritedEntry = inheritedTable && inheritedTable.entries[inherited.selector];
        if (!inheritedEntry) {
          fail(inherited.presentationId + ' references an unknown selector route.');
        }
        var inheritedExpectedAssetIds = inheritedEntry.archiveAssetIds.slice();
        if (inherited.foregroundSelector !== null) {
          var inheritedForegroundTable =
            backgroundTablesById[inherited.foregroundSelectorTableId];
          var inheritedForegroundEntry = inheritedForegroundTable &&
            inheritedForegroundTable.entries[inherited.foregroundSelector];
          if (!inheritedForegroundEntry) {
            fail(inherited.presentationId +
              ' references an unknown foreground selector route.');
          }
          inheritedExpectedAssetIds = inheritedExpectedAssetIds.concat(
            inheritedForegroundEntry.archiveAssetIds);
        }
        if (inheritedExpectedAssetIds.length !== inherited.assetIds.length ||
            inheritedExpectedAssetIds.some(function(assetId, index) {
              return assetId !== inherited.assetIds[index];
            })) {
          fail(inherited.presentationId +
            ' assets do not match the inherited selector tables.');
        }
      }
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
    var directorGrammarOpcodes = {};
    data.directorGrammar.forEach(function(definition) {
      validateDirectorGrammar(definition, directorGrammarOpcodes);
    });
    data.directorEvents.forEach(function(event) { validateDirectorEvent(event, directorEventIds); });
    var continuationSelectors = {};
    data.directorContinuationStreams.forEach(function(stream, index) {
      object(stream, 'Director continuation stream ' + index);
      integer(stream.selector, 'Director continuation selector', 0);
      integer(stream.resourceKey, 'Director continuation resource key', 1);
      integer(stream.z64PrefixStart, 'Director continuation prefix', 0);
      integer(stream.z64PayloadStart, 'Director continuation payload', 0);
      integer(stream.z64PayloadEndExclusive, 'Director continuation payload end', 1);
      integer(stream.storedPayloadLength, 'Director continuation stored length', 1);
      integer(stream.decodedLength, 'Director continuation decoded length', 1);
      integer(stream.decodedWordCount, 'Director continuation decoded words', 1);
      integer(stream.runtimeNodeCount, 'Director continuation runtime nodes', 1);
      nonEmptyString(stream.assetId, 'Director continuation asset ID');
      nonEmptyString(stream.decodedSha256, 'Director continuation decoded hash');
      nonEmptyString(stream.evidenceStatus, 'Director continuation evidence status');
      if (stream.selector !== index || stream.terminalWithoutTrailer !== true ||
          continuationSelectors[stream.selector]) {
        fail('Director continuation selector inventory is inconsistent.');
      }
      continuationSelectors[stream.selector] = true;
    });
    var externalRequestPhysicalSites = {};
    data.parentEventExternalRequests.forEach(function(request, index) {
      var requestLabel = 'Parent event external request ' + index;
      validateExternalRequestProfile(request, requestLabel);
      integer(request.eventDirectoryRow, requestLabel + ' event directory row', 0);
      nonEmptyString(request.eventResourceKey, requestLabel + ' event resource key');
      integer(request.eventEntryCursor, requestLabel + ' event entry cursor', 0);
      integer(request.eventInvocationCursor,
        requestLabel + ' event invocation cursor', 0);
      integer(request.eventInvocationOffset,
        requestLabel + ' event invocation offset', 0);
      integer(request.precedingDirectorLaunchCount,
        requestLabel + ' preceding Director launch count', 0);
      if (request.eventInvocationCursor + request.eventInvocationOffset !==
          request.decodedByteOffset) {
        fail(requestLabel + ' invocation cursor does not reach its opcode site.');
      }
      if (request.precedingDirectorSelector !== null) {
        integer(request.precedingDirectorSelector,
          requestLabel + ' preceding Director selector', 0);
        if (request.precedingDirectorSelector >= 1693) {
          fail(requestLabel + ' preceding Director selector exceeds the retail table.');
        }
      }
      externalRequestPhysicalSites[
        request.eventDirectoryRow + ':' + request.decodedByteOffset
      ] = true;
    });
    var translationPhysicalSites = {};
    var retailTranslationPhysicalSites = {};
    var nonretailTranslationPhysicalSites = {};
    var exactTranslationWriteContexts = 0;
    var unresolvedTranslationWriteContexts = 0;
    var retailTranslationWriteContexts = 0;
    var exactRetailTranslationWriteContexts = 0;
    var unresolvedRetailTranslationWriteContexts = 0;
    var nonretailTranslationWriteContexts = 0;
    data.parentEventTranslationWrites.forEach(function(write, index) {
      var writeLabel = 'Parent event translation write ' + index;
      validateTranslationWrite(write, writeLabel);
      var site = write.eventDirectoryRow + ':' + write.decodedByteOffset;
      translationPhysicalSites[site] = true;
      if (write.resolutionStatus === 'exact') exactTranslationWriteContexts += 1;
      if (write.resolutionStatus === 'replacement-value-unresolved') {
        unresolvedTranslationWriteContexts += 1;
      }
      if (write.tableIndex !== null && write.tableIndex < 17) {
        retailTranslationPhysicalSites[site] = true;
        retailTranslationWriteContexts += 1;
        if (write.resolutionStatus === 'exact') exactRetailTranslationWriteContexts += 1;
        if (write.resolutionStatus === 'replacement-value-unresolved') {
          unresolvedRetailTranslationWriteContexts += 1;
        }
      } else if (write.tableIndex === 0xFF) {
        nonretailTranslationPhysicalSites[site] = true;
        nonretailTranslationWriteContexts += 1;
      }
    });
    var substitutionSourceIds = {};
    data.parentEventSubstitutionSources.forEach(function(source, index) {
      var sourceLabel = 'Parent event substitution source ' + index;
      validateSubstitutionSource(source, sourceLabel);
      unique(source.sourceId, substitutionSourceIds, 'Parent event substitution source ID');
    });
    if (data.parentEventSubstitutionSources.length !== 2 ||
        !substitutionSourceIds.A || !substitutionSourceIds.B) {
      fail('Parent event substitution source definitions are incomplete.');
    }
    var substitutionSourcePhysicalSites = {};
    var substitutionSourceAWriteContexts = 0;
    var substitutionSourceBWriteContexts = 0;
    var exactSubstitutionSourceIndexContexts = 0;
    var unresolvedSubstitutionSourceIndexContexts = 0;
    var exactSubstitutionSourceValueContexts = 0;
    var unresolvedSubstitutionSourceValueContexts = 0;
    data.parentEventSubstitutionSourceWrites.forEach(function(write, index) {
      var writeLabel = 'Parent event substitution source write ' + index;
      validateSubstitutionSourceWrite(write, writeLabel);
      substitutionSourcePhysicalSites[
        write.eventDirectoryRow + ':' + write.decodedByteOffset
      ] = true;
      if (write.sourceId === 'A') substitutionSourceAWriteContexts += 1;
      if (write.sourceId === 'B') substitutionSourceBWriteContexts += 1;
      if (write.sourceIndex === null) unresolvedSubstitutionSourceIndexContexts += 1;
      else exactSubstitutionSourceIndexContexts += 1;
      if (write.value === null) unresolvedSubstitutionSourceValueContexts += 1;
      else exactSubstitutionSourceValueContexts += 1;
    });
    if (data.directorEvents.length !== 154 || data.directorGrammar.length !== 154 ||
        !data.counts ||
        data.counts.directorOpcodeDefinitions !== 153 ||
        data.counts.directorNodes !== 8451 || data.counts.directorWords !== 21927 ||
        data.counts.registeredDirectorWaits !== 464 ||
        data.counts.remainingDirectorGapWords !== 0) {
      fail('Corrected 153-command Director corpus counts are stale.');
    }
    if (data.counts.retailDirectorOpcodeDefinitions !== 154 ||
        data.counts.retailDirectorSelectorRows !== 1693 ||
        data.counts.populatedRetailDirectorSelectorRows !== 1548 ||
        data.counts.retailDirectorResources !== 1498 ||
        data.counts.directorContinuationStreams !== 5 ||
        data.directorContinuationStreams.length !== 5 ||
        data.counts.retailDirectorWords !== 550019 ||
        data.counts.runtimeTiledDirectorResources !== 1498 ||
        data.counts.profiledDirectorResources !== 60 ||
        data.counts.directEventDirectorLaunches !== 1998 ||
        data.counts.directEventDirectorSelectors !== 1520 ||
        data.counts.directEventDirectorResources !== 1472 ||
        data.counts.sceneGroupPreloadDirectorResources !== 209 ||
        data.counts.sceneGroupPreloadBackgroundCommands !== 191 ||
        data.counts.inheritedStageDirectorResources !== 90 ||
        data.counts.inheritedStageLaunchContexts !== 177 ||
        data.counts.contextOnlyResolvedStageInheritanceContexts !== 18 ||
        data.counts.unresolvedStageInheritanceContexts !== 209 ||
        data.counts.parentEventExternalRequestPhysicalSites !== 45 ||
        data.counts.parentEventExternalRequestHandoffs !== 47 ||
        data.parentEventExternalRequests.length !== 47 ||
        Object.keys(externalRequestPhysicalSites).length !== 45 ||
        data.counts.parentEventTranslationPhysicalSites !== 69 ||
        data.counts.parentEventTranslationWriteContexts !== 416 ||
        data.parentEventTranslationWrites.length !== 416 ||
        Object.keys(translationPhysicalSites).length !== 69 ||
        data.counts.parentEventExactTranslationWriteContexts !== 128 ||
        exactTranslationWriteContexts !== 128 ||
        data.counts.parentEventUnresolvedTranslationWriteContexts !== 288 ||
        unresolvedTranslationWriteContexts !== 288 ||
        data.counts.parentEventSubstitutionSourcePhysicalSites !== 42 ||
        Object.keys(substitutionSourcePhysicalSites).length !== 42 ||
        data.counts.parentEventSubstitutionSourceWriteContexts !== 42 ||
        data.parentEventSubstitutionSourceWrites.length !== 42 ||
        data.counts.parentEventSubstitutionSourceAWriteContexts !== 21 ||
        substitutionSourceAWriteContexts !== 21 ||
        data.counts.parentEventSubstitutionSourceBWriteContexts !== 21 ||
        substitutionSourceBWriteContexts !== 21 ||
        data.counts.parentEventExactSubstitutionSourceIndexContexts !== 2 ||
        exactSubstitutionSourceIndexContexts !== 2 ||
        data.counts.parentEventUnresolvedSubstitutionSourceIndexContexts !== 40 ||
        unresolvedSubstitutionSourceIndexContexts !== 40 ||
        data.counts.parentEventExactSubstitutionSourceValueContexts !== 4 ||
        exactSubstitutionSourceValueContexts !== 4 ||
        data.counts.parentEventUnresolvedSubstitutionSourceValueContexts !== 38 ||
        unresolvedSubstitutionSourceValueContexts !== 38 ||
        data.parentEventSubstitutionSourceWrites.some(function(write) {
          return write.eventDirectoryRow !== 67 ||
            write.eventResourceKey !== '0x003B0BBC';
        }) ||
        data.counts.parentEventRetailTranslationPhysicalSites !== 21 ||
        Object.keys(retailTranslationPhysicalSites).length !== 21 ||
        data.counts.parentEventRetailTranslationWriteContexts !== 344 ||
        retailTranslationWriteContexts !== 344 ||
        data.counts.parentEventExactRetailTranslationWriteContexts !== 74 ||
        exactRetailTranslationWriteContexts !== 74 ||
        data.counts.parentEventUnresolvedRetailTranslationWriteContexts !== 270 ||
        unresolvedRetailTranslationWriteContexts !== 270 ||
        data.counts.parentEventNonretailTranslationPhysicalSites !== 48 ||
        Object.keys(nonretailTranslationPhysicalSites).length !== 48 ||
        data.counts.parentEventNonretailTranslationWriteContexts !== 72 ||
        nonretailTranslationWriteContexts !== 72 ||
        data.counts.launchTranslatedDirectorResources !== 19 ||
        data.counts.launchTranslationPlaceholders !== 398 ||
        data.counts.launchTranslationIndexes !== 17) {
      fail('Retail Director selector-table counts are stale.');
    }
    var profiledScenes = data.scenes.filter(function(scene) {
      return scene.source.dynamicGrammar !== true;
    });
    var directorNodeCount = profiledScenes.reduce(function(total, scene) {
      return total + scene.source.nodes.length;
    }, 0);
    var directorWordCount = profiledScenes.reduce(function(total, scene) {
      return total + scene.source.decodedWordCount;
    }, 0);
    var retailDirectorWordCount = data.scenes.reduce(function(total, scene) {
      return total + scene.source.decodedWordCount;
    }, 0);
    var retailDirectorNodeCount = data.scenes.reduce(function(total, scene) {
      return total + (scene.source.dynamicGrammar === true
        ? scene.source.runtimeNodeCount : scene.source.nodes.length);
    }, 0);
    var registeredWaitCount = profiledScenes.reduce(function(total, scene) {
      return total + scene.source.registeredWaits.length;
    }, 0);
    var directEventLaunchCount = data.scenes.reduce(function(total, scene) {
      return total + scene.launchProfile.parentEventLaunches.length;
    }, 0);
    var eventInvocationContextCount = 0;
    var multiInvocationLaunchCount = 0;
    var distinctInvocationCursors = {};
    var exactEventPropertyE9Contexts = 0;
    var exactEventPropertyFCContexts = 0;
    var exactEventPropertyFDContexts = 0;
    data.scenes.forEach(function(scene) {
      scene.launchProfile.parentEventLaunches.forEach(function(launch) {
        eventInvocationContextCount += launch.eventInvocationContexts.length;
        if (launch.eventInvocationContexts.length > 1) multiInvocationLaunchCount += 1;
        launch.eventInvocationContexts.forEach(function(context) {
          if (context.eventPropertyE9 !== null) exactEventPropertyE9Contexts += 1;
          if (context.eventPropertyFC !== null) exactEventPropertyFCContexts += 1;
          if (context.eventPropertyFD !== null) exactEventPropertyFDContexts += 1;
          distinctInvocationCursors[
            launch.eventDirectoryRow + ':' + context.eventInvocationCursor
          ] = true;
        });
      });
    });
    var launchTranslatedScenes = data.scenes.filter(function(scene) {
      return scene.launchProfile.operandTranslation.required === true;
    });
    var launchTranslationPlaceholderCount = launchTranslatedScenes.reduce(
      function(total, scene) {
        return total + scene.launchProfile.operandTranslation.placeholderCount;
      }, 0);
    if (directorNodeCount !== data.counts.directorNodes ||
        directorWordCount !== data.counts.directorWords ||
        registeredWaitCount !== data.counts.registeredDirectorWaits) {
      fail('Corrected Director scene totals do not match the catalog.');
    }
    if (retailDirectorWordCount !== data.counts.retailDirectorWords ||
        retailDirectorNodeCount !== data.counts.retailDirectorNodes) {
      fail('Retail Director scene totals do not match the catalog.');
    }
    if (directEventLaunchCount !== data.counts.directEventDirectorLaunches ||
        eventInvocationContextCount !== data.counts.directEventInvocationContexts ||
        exactEventPropertyE9Contexts !== data.counts.directEventExactPropertyE9Contexts ||
        exactEventPropertyFCContexts !== data.counts.directEventExactPropertyFCContexts ||
        exactEventPropertyFDContexts !== data.counts.directEventExactPropertyFDContexts ||
        multiInvocationLaunchCount !== data.counts.directEventMultiInvocationLaunches ||
        Object.keys(distinctInvocationCursors).length !==
          data.counts.parentEventDistinctInvocationCursors ||
        launchTranslatedScenes.length !== data.counts.launchTranslatedDirectorResources ||
        launchTranslationPlaceholderCount !== data.counts.launchTranslationPlaceholders) {
      fail('Director launch-context totals do not match the catalog.');
    }
    if (options.requireComplete !== false && data.scenes.length !== 1498) {
      fail('US Rev 0 Cutscene catalog must contain all 1,498 retail Director resources.');
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
    if (data.counts && data.counts.modeTwoStagePlacementProfiles !==
        data.modeTwoStagePlacementProfiles.length) {
      fail('Cutscene catalog mode-two Stage placement count is stale.');
    }
    if (data.counts && (data.counts.sceneResourcePathGroups !== 59 ||
        data.counts.sceneResourcePathEntries !== data.sceneResourcePaths.length ||
        data.counts.populatedSceneResourcePaths !==
          data.sceneResourcePaths.filter(function(path) {
            return path.resourceKey !== null;
          }).length)) {
      fail('Cutscene catalog scene resource-path counts are stale.');
    }
    var modeTwoScenes = data.scenes.filter(function(scene) {
      return scene.launchProfile.directorMode.value === 2;
    });
    var derivedRequests = modeTwoScenes.reduce(function(rows, scene) {
      return rows.concat(scene.launchProfile.background.requests.filter(function(request) {
        return request.derivedEnvironment !== null;
      }));
    }, []);
    var derivedContexts = derivedRequests.reduce(function(rows, request) {
      return rows.concat(request.derivedEnvironment.contexts);
    }, []);
    var unresolvedForegroundScenes = modeTwoScenes.filter(function(scene) {
      return scene.launchProfile.background.requests.length === 0 ||
        !scene.launchProfile.background.requests.some(function(request) {
          return Number.isInteger(request.foregroundSelector);
        });
    });
    if (derivedRequests.length !== data.counts.modeTwoDerivedEnvironmentSentinels ||
        derivedContexts.length !==
          data.counts.modeTwoDerivedEnvironmentInvocationContexts ||
        derivedContexts.filter(function(context) {
          return context.resolutionStatus === 'exact-native-mapper-result';
        }).length !== data.counts.exactModeTwoDerivedEnvironmentInvocationContexts ||
        unresolvedForegroundScenes.length !==
          data.counts.unresolvedModeTwoForegroundSelections) {
      fail('Cutscene catalog derived-environment or foreground totals are stale.');
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
    var directorContinuationStreams = data.directorContinuationStreams.slice();
    var backgroundSelectorTables = data.backgroundSelectorTables.slice();
    var parentEventExternalRequests = data.parentEventExternalRequests.slice();
    var parentEventTranslationWrites = data.parentEventTranslationWrites.slice();
    var parentEventSubstitutionSources = data.parentEventSubstitutionSources.slice();
    var parentEventSubstitutionSourceWrites =
      data.parentEventSubstitutionSourceWrites.slice();
    var modeTwoDerivedEnvironmentRules = data.modeTwoDerivedEnvironmentRules;
    var modeTwoStagePlacementProfiles = data.modeTwoStagePlacementProfiles.slice();
    var sceneResourcePaths = data.sceneResourcePaths.slice();
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
    var poseById = {}, programById = {}, posesByBank = {}, poseByRuntimeSelector = {},
      poseByStateIndex = {};
    posePrograms.forEach(function(program) {
      programById[program.programId] = program;
      if (!poseById[program.poseId] ||
          !poseById[program.poseId].frames.length && program.frames.length) {
        poseById[program.poseId] = program;
      }
      if (!posesByBank[program.bank]) posesByBank[program.bank] = [];
      posesByBank[program.bank].push(program);
      poseByStateIndex[[program.bank, program.stateIndex].join(':')] = program;
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
    var modeTwoStagePlacementBySelector = {};
    modeTwoStagePlacementProfiles.forEach(function(profile) {
      modeTwoStagePlacementBySelector[profile.foregroundSelector] = profile;
    });
    var sceneResourcePathBySlot = {};
    sceneResourcePaths.forEach(function(path) {
      sceneResourcePathBySlot[path.groupIndex + ':' + path.entryIndex] = path;
    });
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
      directorContinuationStreams: directorContinuationStreams,
      backgroundSelectorTables: backgroundSelectorTables,
      parentEventExternalRequests: parentEventExternalRequests,
      parentEventTranslationWrites: parentEventTranslationWrites,
      parentEventSubstitutionSources: parentEventSubstitutionSources,
      parentEventSubstitutionSourceWrites: parentEventSubstitutionSourceWrites,
      modeTwoDerivedEnvironmentRules: modeTwoDerivedEnvironmentRules,
      modeTwoStagePlacementProfiles: modeTwoStagePlacementProfiles,
      sceneResourcePaths: sceneResourcePaths,
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
      getDirectorContinuationStream: function(selector) {
        selector = Number(selector);
        return Number.isInteger(selector) && selector >= 0 &&
          selector < directorContinuationStreams.length
          ? directorContinuationStreams[selector] : null;
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
      getModeTwoStagePlacementProfile: function(foregroundSelector) {
        return modeTwoStagePlacementBySelector[Number(foregroundSelector)] || null;
      },
      getSceneResourcePath: function(groupIndex, entryIndex) {
        return sceneResourcePathBySlot[
          Number(groupIndex) + ':' + Number(entryIndex)] || null;
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
      getPhysicalPoseProgramByStateIndex: function(bank, stateIndex) {
        return poseByStateIndex[[Number(bank), Number(stateIndex)].join(':')] || null;
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
      }) || isDirector && scene.launchProfile &&
        scene.launchProfile.background.inheritedPresentation || null;
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
            sourceKind: launchBackgroundRequest.sourceKind === 'parent-event-predecessor'
              ? 'parent-event-predecessor' : 'launch-profile-selector'
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
          : {
            mode: initialBackgroundAssetId ? 'stage-fit' : 'unresolved',
            evidenceStatus: isDirector && scene.launchProfile
              ? scene.launchProfile.stageTransform.evidenceStatus : 'unresolved',
            calibrationStatus: isDirector && scene.launchProfile
              ? scene.launchProfile.stageTransform.status :
                'No launch Stage transform is registered.',
            initialStageTransform: isDirector && scene.launchProfile
              ? JSON.parse(JSON.stringify(scene.launchProfile.stageTransform.initial)) : null
          }
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
