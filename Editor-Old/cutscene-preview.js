// Lordly Caliber - deterministic Cutscene Studio preview state and clock.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var M = OB64.cutsceneModel;
  if (!M) throw new Error('cutscene-preview.js requires cutscene-model.js');

  var FPS = M.previewFps;
  var SNAP_STEPS = Object.freeze({
    frame: 1,
    tenth: 3,
    half: 15,
    second: 30
  });

  function PreviewError(message) {
    this.name = 'CutscenePreviewError';
    this.message = message;
  }
  PreviewError.prototype = Object.create(Error.prototype);
  PreviewError.prototype.constructor = PreviewError;

  function fail(message) { throw new PreviewError(message); }

  function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
  }

  function appliesToPath(clip, pathId) {
    return !clip.pathIds.length || clip.pathIds.indexOf(pathId) !== -1;
  }

  function clipEnd(clip) {
    return clip.startFrame + Math.max(1, clip.durationFrames);
  }

  function activeAt(clip, frame) {
    if (clip.durationFrames === 0) return frame === clip.startFrame;
    return frame >= clip.startFrame && frame < clip.startFrame + clip.durationFrames;
  }

  function tracksOfType(document, type, actorId) {
    return document.tracks.filter(function(track) {
      return track.type === type && (actorId == null || track.actorId === actorId);
    });
  }

  function orderedClips(document, type, actorId, pathId) {
    var rows = [];
    tracksOfType(document, type, actorId).forEach(function(track, trackIndex) {
      track.clips.forEach(function(clip, clipIndex) {
        if (!appliesToPath(clip, pathId)) return;
        rows.push({ track: track, clip: clip, trackIndex: trackIndex, clipIndex: clipIndex });
      });
    });
    rows.sort(function(left, right) {
      return left.clip.startFrame - right.clip.startFrame ||
        left.trackIndex - right.trackIndex || left.clipIndex - right.clipIndex;
    });
    return rows;
  }

  function branchIds(document) {
    return document.branches.map(function(branch) { return branch.id; });
  }

  function resolvePath(document, requestedPathId) {
    var ids = branchIds(document);
    var pathId = requestedPathId || ids[0];
    if (ids.indexOf(pathId) === -1) fail('Unknown preview path "' + pathId + '".');
    return pathId;
  }

  function sceneDurationFrames(document, requestedPathId) {
    M.validateSceneDocument(document);
    var runtime = OB64.cutsceneRuntime && OB64.cutsceneRuntime.forDocument(document);
    if (runtime) return runtime.durationTicks;
    var pathId = resolvePath(document, requestedPathId);
    var duration = 1;
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        if (appliesToPath(clip, pathId)) duration = Math.max(duration, clipEnd(clip));
      });
    });
    return duration;
  }

  function position(value, fallback) {
    value = value || {};
    return {
      x: Number.isFinite(value.x) ? value.x : fallback.x,
      y: Number.isFinite(value.y) ? value.y : fallback.y,
      z: Number.isFinite(value.z) ? value.z : fallback.z
    };
  }

  function mix(left, right, amount) {
    return left + (right - left) * amount;
  }

  function actorStateAt(document, actor, frame, pathId) {
    var actorSource = actor.source || {};
    var authoredStartFrame = actorSource.authored === true &&
      Number.isInteger(actorSource.authoredStartFrame) ? actorSource.authoredStartFrame : null;
    var state = {
      id: actor.id,
      label: actor.label,
      artSourceId: actor.artSourceId,
      capability: actor.capability,
      visible: actor.initial.visible &&
        (authoredStartFrame == null || frame >= authoredStartFrame),
      opacityByte: 255,
      x: actor.initial.x,
      y: actor.initial.y,
      z: actor.initial.z,
      facing: actor.initial.facing,
      poseId: actor.initial.poseId,
      bank: Number.isInteger(actorSource.bank) ? actorSource.bank : null,
      animationKey: Number.isInteger(actorSource.animationKey)
        ? actorSource.animationKey : null,
      nativeFacing: /^native-\d+$/.test(actor.initial.facing)
        ? Number(actor.initial.facing.slice(7)) : null,
      variantSelector: Number.isInteger(actorSource.variantSelector)
        ? actorSource.variantSelector : 0,
      poseFrame: 0,
      movementFrame: 0,
      activeMovementId: null,
      source: M.cloneJson(actorSource, 'actor.source')
    };

    orderedClips(document, 'actor', actor.id, pathId).forEach(function(row) {
      var clip = row.clip;
      if (clip.startFrame > frame) return;
      if (clip.kind === 'enter') state.visible = true;
      if (clip.kind === 'exit') state.visible = false;
      if (clip.kind === 'visibility' && typeof clip.payload.visible === 'boolean') {
        state.visible = clip.payload.visible;
      }
      if (clip.kind === 'opacity' && Number.isInteger(clip.payload.opacityByte)) {
        state.opacityByte = clamp(clip.payload.opacityByte, 0, 255);
      }
    });

    orderedClips(document, 'pose', actor.id, pathId).forEach(function(row) {
      var clip = row.clip;
      if (clip.startFrame > frame) return;
      if (Number.isInteger(clip.payload.bank)) {
        state.artSourceId = 'cutscene-art-bank:' + clip.payload.bank;
        state.bank = clip.payload.bank;
      }
      if (Number.isInteger(clip.payload.animationKey)) state.animationKey = clip.payload.animationKey;
      if (Object.prototype.hasOwnProperty.call(clip.payload, 'poseId')) {
        state.poseId = clip.payload.poseId;
      }
      if (typeof clip.payload.facing === 'string') state.facing = clip.payload.facing;
      if (Number.isInteger(clip.payload.nativeFacing)) state.nativeFacing = clip.payload.nativeFacing;
      if (Number.isInteger(clip.payload.variantSelector) && clip.payload.variantSelector !== -1) {
        state.variantSelector = clip.payload.variantSelector;
      }
      if (Number.isFinite(clip.payload.x)) state.x = clip.payload.x;
      if (Number.isFinite(clip.payload.y)) state.y = clip.payload.y;
      if (Number.isFinite(clip.payload.z)) state.z = clip.payload.z;
      var elapsed = Math.max(0, frame - clip.startFrame);
      if (clip.durationFrames > 0 && clip.payload.loop !== false) {
        state.poseFrame = elapsed % clip.durationFrames;
      } else {
        state.poseFrame = Math.min(elapsed, Math.max(0, clip.durationFrames - 1));
      }
    });

    orderedClips(document, 'movement', actor.id, pathId).forEach(function(row) {
      var clip = row.clip;
      if (clip.startFrame > frame) return;
      var current = { x: state.x, y: state.y, z: state.z };
      var from = position(clip.payload.from, current);
      var to = position(clip.payload.to, from);
      if (clip.durationFrames === 0 || frame >= clip.startFrame + clip.durationFrames) {
        state.x = to.x;
        state.y = to.y;
        state.z = to.z;
        return;
      }
      var amount = clamp((frame - clip.startFrame) / clip.durationFrames, 0, 1);
      state.x = mix(from.x, to.x, amount);
      state.y = mix(from.y, to.y, amount);
      state.z = mix(from.z, to.z, amount);
      state.activeMovementId = clip.id;
      state.movementFrame = Math.max(0, frame - clip.startFrame);
    });

    return state;
  }

  function activeTrackClips(document, type, frame, pathId) {
    return orderedClips(document, type, null, pathId).filter(function(row) {
      return activeAt(row.clip, frame);
    }).map(function(row) {
      return {
        id: row.clip.id,
        kind: row.clip.kind,
        trackId: row.track.id,
        label: row.track.label,
        startFrame: row.clip.startFrame,
        durationFrames: row.clip.durationFrames,
        capability: row.clip.capability,
        payload: M.cloneJson(row.clip.payload, 'clip.payload')
      };
    });
  }

  function cameraValue(value, fallback) {
    value = value || {};
    return {
      translateX: Number.isFinite(value.translateX) ? value.translateX : fallback.translateX,
      translateY: Number.isFinite(value.translateY) ? value.translateY : fallback.translateY,
      scaleX: Number.isFinite(value.scaleX) ? value.scaleX : fallback.scaleX,
      scaleY: Number.isFinite(value.scaleY) ? value.scaleY : fallback.scaleY
    };
  }

  function mixCamera(from, to, amount) {
    return {
      translateX: mix(from.translateX, to.translateX, amount),
      translateY: mix(from.translateY, to.translateY, amount),
      scaleX: mix(from.scaleX, to.scaleX, amount),
      scaleY: mix(from.scaleY, to.scaleY, amount)
    };
  }

  function cameraStateAt(document, frame, pathId) {
    var state = { translateX: 0, translateY: 0, scaleX: 1, scaleY: 1 };
    var rows = orderedClips(document, 'camera', null, pathId);
    var activeClipId = null;
    var timingStatus = 'default Stage camera';
    for (var index = 0; index < rows.length; index++) {
      var clip = rows[index].clip;
      if (clip.startFrame > frame) break;
      var from = cameraValue(clip.payload.from, state);
      var to = cameraValue(clip.payload.target || clip.payload.to, from);
      var end = clip.startFrame + Math.max(1, clip.durationFrames);
      if (frame < end) {
        var amount = clamp((frame - clip.startFrame) / Math.max(1, clip.durationFrames), 0, 1);
        state = mixCamera(from, to, amount);
        activeClipId = clip.id;
        timingStatus = clip.payload.timingStatus || 'Preview interpolation';
        break;
      }
      state = to;
      timingStatus = clip.payload.timingStatus || timingStatus;
    }
    state.activeClipId = activeClipId;
    state.timingStatus = timingStatus;
    return state;
  }

  function evaluateAtFrame(document, requestedFrame, options) {
    M.validateSceneDocument(document);
    options = options || {};
    if (!Number.isInteger(requestedFrame)) fail('Preview frame must be an integer.');
    var runtime = options.runtime ||
      (OB64.cutsceneRuntime && OB64.cutsceneRuntime.forDocument(document));
    if (runtime) return OB64.cutsceneRuntime.evaluate(runtime, requestedFrame);
    var pathId = resolvePath(document, options.pathId);
    var durationFrames = sceneDurationFrames(document, pathId);
    var frame = clamp(requestedFrame, 0, durationFrames - 1);
    var actorOrder = {};
    document.actors.forEach(function(actor, index) { actorOrder[actor.id] = index; });
    var actors = document.actors.map(function(actor) {
      return actorStateAt(document, actor, frame, pathId);
    });

    actors.sort(function(left, right) {
      return left.z - right.z || actorOrder[left.id] - actorOrder[right.id];
    });

    return {
      frame: frame,
      timeSeconds: frame / FPS,
      durationFrames: durationFrames,
      pathId: pathId,
      background: M.cloneJson(document.background, 'background'),
      actors: actors,
      dialogue: activeTrackClips(document, 'dialogue', frame, pathId),
      audio: activeTrackClips(document, 'audio', frame, pathId),
      camera: activeTrackClips(document, 'camera', frame, pathId),
      cameraState: cameraStateAt(document, frame, pathId),
      effects: activeTrackClips(document, 'effect', frame, pathId),
      flow: activeTrackClips(document, 'flow', frame, pathId)
    };
  }

  function createClock(durationFrames, options) {
    options = options || {};
    if (!Number.isInteger(durationFrames) || durationFrames < 1) {
      fail('Clock duration must be a positive integer frame count.');
    }
    var frame = options.frame == null ? 0 : options.frame;
    if (!Number.isInteger(frame)) fail('Clock frame must be an integer.');
    return {
      frame: clamp(frame, 0, durationFrames - 1),
      durationFrames: durationFrames,
      playing: !!options.playing,
      loop: !!options.loop,
      remainderMs: 0
    };
  }

  function cloneClock(clock) {
    return {
      frame: clock.frame,
      durationFrames: clock.durationFrames,
      playing: clock.playing,
      loop: clock.loop,
      remainderMs: clock.remainderMs
    };
  }

  function play(clock) {
    var next = cloneClock(clock);
    next.playing = true;
    return next;
  }

  function pause(clock) {
    var next = cloneClock(clock);
    next.playing = false;
    next.remainderMs = 0;
    return next;
  }

  function seek(clock, frame) {
    if (!Number.isInteger(frame)) fail('Seek frame must be an integer.');
    var next = cloneClock(clock);
    next.frame = clamp(frame, 0, next.durationFrames - 1);
    next.remainderMs = 0;
    return next;
  }

  function step(clock, deltaFrames) {
    if (!Number.isInteger(deltaFrames)) fail('Step distance must be an integer.');
    var next = cloneClock(clock);
    var requested = next.frame + deltaFrames;
    if (next.loop) {
      requested %= next.durationFrames;
      if (requested < 0) requested += next.durationFrames;
      next.frame = requested;
    } else {
      next.frame = clamp(requested, 0, next.durationFrames - 1);
    }
    next.remainderMs = 0;
    return next;
  }

  function setLoop(clock, enabled) {
    var next = cloneClock(clock);
    next.loop = !!enabled;
    return next;
  }

  function advance(clock, elapsedMs) {
    if (typeof elapsedMs !== 'number' || !Number.isFinite(elapsedMs) || elapsedMs < 0) {
      fail('Elapsed time must be a non-negative finite number.');
    }
    var next = cloneClock(clock);
    if (!next.playing || elapsedMs === 0) return next;

    var totalMs = next.remainderMs + elapsedMs;
    var frameCount = Math.floor(totalMs * FPS / 1000 + 1e-9);
    next.remainderMs = totalMs - frameCount * (1000 / FPS);
    if (frameCount < 1) return next;

    if (next.loop) {
      next.frame = (next.frame + frameCount) % next.durationFrames;
      return next;
    }

    next.frame += frameCount;
    if (next.frame >= next.durationFrames - 1) {
      next.frame = next.durationFrames - 1;
      next.playing = false;
      next.remainderMs = 0;
    }
    return next;
  }

  function snapFrame(frame, mode) {
    if (!Number.isFinite(frame)) fail('Frame must be a finite number.');
    var stepFrames = SNAP_STEPS[mode || 'frame'];
    if (!stepFrames) fail('Unknown snap mode "' + mode + '".');
    return Math.max(0, Math.round(frame / stepFrames) * stepFrames);
  }

  function formatTime(frame) {
    if (!Number.isInteger(frame) || frame < 0) fail('Frame must be a non-negative integer.');
    var totalSeconds = Math.floor(frame / FPS);
    var minutes = Math.floor(totalSeconds / 60);
    var seconds = totalSeconds % 60;
    var frameInSecond = frame % FPS;
    return minutes + ':' + String(seconds).padStart(2, '0') + '.' +
      String(frameInSecond).padStart(2, '0');
  }

  OB64.cutscenePreview = {
    fps: FPS,
    snapSteps: SNAP_STEPS,
    PreviewError: PreviewError,
    sceneDurationFrames: sceneDurationFrames,
    evaluateAtFrame: evaluateAtFrame,
    cameraStateAt: cameraStateAt,
    createClock: createClock,
    play: play,
    pause: pause,
    seek: seek,
    step: step,
    setLoop: setLoop,
    advance: advance,
    snapFrame: snapFrame,
    formatTime: formatTime,
    appliesToPath: appliesToPath,
    activeAt: activeAt
  };
})(window.OB64);
