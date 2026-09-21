// Lordly Caliber - execution-driven Director preview runtime.
//
// The Director stream is a scheduler program, not a movie timeline. This
// module executes its corrected primitive/composite model on native update
// ticks and exposes immutable Stage snapshots for deterministic scrubbing.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var M = OB64.cutsceneModel;
  if (!M) throw new Error('cutscene-runtime.js requires cutscene-model.js');

  var DEFAULT_MAX_TICKS = 30000;
  var SCENE_TRANSFORM_TABLE_KEY = 0x019A63EC;
  var SCENE_TRANSFORM_CHANNEL_COUNT = 20;
  var SCENE_TRANSFORM_KEYFRAME_BYTES = 0x1E0;
  var SCENE_TRANSFORM_FIELDS = [
    { name: 'rotationX', offset: 0x000 },
    { name: 'rotationY', offset: 0x050 },
    { name: 'translateX', offset: 0x0A0 },
    { name: 'translateY', offset: 0x0F0 },
    { name: 'translateZ', offset: 0x140 },
    { name: 'uniformScale', offset: 0x190 }
  ];
  var PHASE_FOR_FACING = [0, 9, 6, 3];
  var FACING_FOR_PHASE = [0, 9, 8, 3, 7, 7, 2, 6, 5, 1, 4, 4];
  var bindings = typeof WeakMap === 'function' ? new WeakMap() : null;
  var retainedRecordFields = new WeakMap();
  function plainRetainedFrame(frame) {
    var result=Object.assign({},frame);
    ['actors','effects'].forEach(function(field){result[field]=(frame[field]||[]).map(function(row){var keys=retainedRecordFields.get(row);if(!keys)return row;var plain={};keys.forEach(function(key,i){plain[key]=row.values[i];});return plain;});});
    return result;
  }

  function RuntimeError(message, code) {
    this.name = 'CutsceneRuntimeError';
    this.message = message;
    this.code = code || 'director-runtime';
  }
  RuntimeError.prototype = Object.create(Error.prototype);
  RuntimeError.prototype.constructor = RuntimeError;

  function fail(message, code) { throw new RuntimeError(message, code); }
  function unsigned(value) { return Number(value) >>> 0; }
  function signed(value) { return unsigned(value) | 0; }
  function lowU16(value) { return unsigned(value) & 0xFFFF; }
  function lowS16(value) {
    value = lowU16(value);
    return value & 0x8000 ? value - 0x10000 : value;
  }
  function lowS8(value) {
    value = unsigned(value) & 0xFF;
    return value & 0x80 ? value - 0x100 : value;
  }
  function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
  }
  function mix(left, right, amount) { return left + (right - left) * amount; }
  function finite(value, fallback) { return Number.isFinite(value) ? value : fallback; }
  function fixed(value) { return signed(value) / 1000; }
  function actorCoordinate(value) { return Math.fround(Math.fround(signed(value)) / Math.fround(1000)); }

  // Reviewed finite planar contract. Motion belongs to the slot, not the Actor pointer.
  function createNativeMovement(actor, previous, words) {
    var f = Math.fround;
    var x = actorCoordinate(words[2]), z = actorCoordinate(words[3]);
    if (x === -1 && z === -1) { x = f(actor.x); z = f(actor.z); }
    else { actor.x = x; actor.z = z; }
    var tx = actorCoordinate(words[4]), tz = actorCoordinate(words[5]);
    var control = signed(words[6]), speed = signed(words[7]);
    if (control === 1) { actor.x = tx; actor.z = tz; return previous; }
    var snapX = tx - 0.1 < x && x < tx + 0.1;
    var snapZ = tz - 0.1 < z && z < tz + 0.1;
    if (snapX) actor.x = tx;
    if (snapZ) actor.z = tz;
    if (snapX && snapZ) return previous;
    var dx = f(tx - x), dz = f(tz - z);
    var distance = f(Math.sqrt(f(f(dx * dx) + f(dz * dz))));
    if (speed && distance === 0) throw new RuntimeError('Movement requires allocator or retained pause-byte state.', 'movement-external-state');
    var quotient = speed ? f(distance * (1000 / speed)) : control;
    if (!Number.isFinite(quotient) || quotient < -2147483648 || quotient >= 2147483648) {
      throw new RuntimeError('Movement float-to-integer conversion is outside the supported finite range.', 'movement-arithmetic');
    }
    var count = Math.trunc(quotient);
    var denominator = f(speed ? lowS16(count) : control);
    var vx = f(dx / denominator), vz = f(dz / denominator);
    if (!Number.isFinite(vx) || !Number.isFinite(vz)) {
      throw new RuntimeError('Movement produces a nonfinite native velocity.', 'movement-arithmetic');
    }
    return { remaining: lowU16(count), vx: vx, vz: vz, pauseByte: 0, elapsed: 0 };
  }

  function advanceNativeMovement(actor, job) {
    if (!job.pauseByte) {
      var x = Math.fround(actor.x + job.vx), z = Math.fround(actor.z + job.vz);
      if (!Number.isFinite(x) || !Number.isFinite(z)) throw new RuntimeError('Movement update produces nonfinite coordinates.', 'movement-arithmetic');
      actor.x = x;
      actor.z = z;
      job.remaining = lowU16(job.remaining - 1);
      job.elapsed += 1;
    }
    return lowS16(job.remaining) !== 0;
  }

  function advanceNativePose(actor, resolveProgram, limit, sharedControl, recordCheck) {
    var result = 1, dispatches = 0;
    while (actor.poseDelay <= 0) {
      if (++dispatches > limit) return 'pose-dispatch-limit';
      if (actor.decoderMode !== 0) actor.poseCursor = signed(actor.poseCursor + 1);
      if (actor.decoderMode !== 0 && [135,136,161,-1].includes(lowS16(actor.bank))) {
        actor.poseStateIndex = lowS16(actor.poseStateIndex) < 50 ? 0 : 50;
      }
      var program = resolveProgram(actor);
      if (!program || !Array.isArray(program.records)) return actor.decoderMode !== 0
        ? 'alternate-pose-registration' : 'missing-pose-program';
      if (actor.decoderMode === 0) actor.poseCursor = signed(actor.poseCursor + 1);
      var record = program.records[actor.poseCursor] || { opcode: 0, operands: [] };
      var op = record.opcode, p = record.operands;
      if(recordCheck){var rejected=recordCheck(record);if(rejected)return rejected;}
      result = op;
      if (op >= 17 && op <= 20) {
        if (!sharedControl) return 'shared-pose-control-' + op;
        var sharedBoundary = sharedControl(actor, record);
        if (sharedBoundary) return sharedBoundary;
      }
      else if (op === 0) actor.poseDelay = 2;
      else if (op === 1 || op === 21) {
        actor.displayedFrameToken = op === 1 ? p[0] : p[0] + 256 * p[1];
        actor.poseDelay = op === 1 ? p[1] : p[2];
      } else if (op === 2) {
        actor.x = Math.fround(actor.x - lowS8(p[0]));
        actor.z = Math.fround(actor.z - lowS8(p[1]));
      } else if (op === 3) actor.poseDelay = p[0];
      else if (op === 4) actor.poseCursor = p[0] - 1;
      else if (op === 5) {
        actor.previousPoseStateIndex = actor.poseStateIndex;
        actor.poseStateIndex = p[0];
        actor.poseCursor = -1;
      } else if (op === 12) {
        actor.x = Math.fround(actor.x + lowS8(p[0]));
        actor.y = Math.fround(actor.y + lowS8(p[1]));
        actor.z = Math.fround(actor.z + lowS8(p[2]));
      } else if (op === 13 || op === 16) {
        var bytes = op === 13 ? actor.material : actor.materialDelta;
        if (p[0] === 255) bytes.fill(p[1]);
        else if (p[0] < 16) bytes[p[0]] = p[1];
        else return 'pose-material-index';
      } else if (![6,7,8,9,10,11,14,15].includes(op)) return 'unsupported-pose-control-' + op;
    }
    for (var i = 0; i < 16; i++) actor.material[i] = clamp(actor.material[i] + lowS8(actor.materialDelta[i]), 0, 255);
    actor.poseDelay = signed(actor.poseDelay - 2);
    actor.poseSequencerResult = result & 255;
    actor.poseFrame += 2;
    return null;
  }

  // CPR-R03: these calls count eligible producer services, never video frames.
  function advanceNativeMenu(menu, service) {
    if (!service.eligible || menu.detached) return;
    if (service.entityId !== menu.entityId) fail('Menu callback owner does not match its entity.', 'external-producer-owner');
    if (menu.closing) {
      menu.closeRemaining -= 1;
      if (menu.closeRemaining === 0) { menu.detached = true; menu.closing = false; }
      return;
    }
    var input = service.readiness;
    if (!input) fail('Menu service needs explicit readiness fields.', 'menu-readiness-input');
    menu.status = -1;
    menu.statusSource = 'native-menu-callback';
    if (menu.substate === 0) {
      if (input.readyByte !== 0) menu.substate = 5;
    } else if (menu.substate === 5) {
      if (service.openingReturned !== true) fail('Menu opening helper outcome is unavailable.', 'menu-opening-input');
      menu.substate = 6;
    } else if (menu.substate === 6 && lowS16(input.alpha) === 255 && lowU16(input.cooldown) === 0) {
      if (!Number.isInteger(service.actionMask) || !Number.isInteger(service.directionMask)) fail('Menu service needs selected action and direction masks.', 'menu-controller-input');
      if (service.actionMask & 0x8000) { menu.status = lowS8(menu.selection); menu.substate = 3; }
      else if (service.actionMask & 0x4000) {
        if (lowS8(menu.cancel) !== -1) { menu.selection = lowS8(menu.cancel); menu.status = menu.selection; menu.substate = 3; }
      } else if (service.directionMask & 0x0800) {
        if (lowS8(menu.selection) > 0) menu.selection = lowS8(menu.selection - 1);
      } else if ((service.directionMask & 0x0400) && lowS8(menu.selection) + 1 < lowS8(menu.optionCount)) {
        menu.selection = lowS8(menu.selection + 1);
      }
    }
  }

  function createNativeColor(previous, words) {
    var color = previous ? Object.assign({}, previous) : { alpha:0, ownershipFlag:0 };
    color.ownershipFlag = previous ? (previous.ownershipFlag === 1 ? 0 : 2) : 0;
    color.duration = color.remaining = lowS16(words[1]);
    color.red = words[2] & 255; color.green = words[3] & 255; color.blue = words[4] & 255;
    if (signed(words[5]) !== -1) color.alpha = words[5] & 255;
    color.startAlpha = color.alpha; color.targetAlpha = words[6] & 255;
    return color;
  }
  function advanceNativeColor(color, eligible) {
    if (!color || !eligible || lowS16(color.remaining) <= 0) return;
    color.remaining = lowS16(color.remaining - 1);
    if (lowS16(color.duration) === 0) fail('Color producer has an invalid zero denominator.', 'color-initial-state');
    color.alpha = (color.targetAlpha + Math.trunc((color.startAlpha - color.targetAlpha) * color.remaining / lowS16(color.duration))) & 255;
  }
  function cleanupNativeColor(color) {
    if (!color) return null;
    color.remaining = 0;
    return color.ownershipFlag === 1 ? null : color;
  }

  // CPR-R07C: addresses are z64 table offsets; both request contexts are scalar slots.
  function selectNativeSharedRequest(opcode, operands, mode, input, readHalfword) {
    var key = (operands[0] & 255) * 256 + (operands[1] & 255), offset;
    if (opcode === 17 || opcode === 19) {
      if (key > 831) return {boundary:'shared-pose-table-input'};
      offset = 0x212880 + 2 * key;
    } else if (mode === 0) {
      if (!input || (input.projectionReturned !== true && !(input.projectionInput instanceof Uint8Array && input.projectionInput.length===12 && !input.projectionInput.some(Boolean)))) return {boundary:'shared-pose-projection-input'};
      if (key > 760) return {boundary:'shared-pose-table-input'};
      offset = key >= 590 ? 0x212880 + 2 * (key + 2) : 0x2123A0 + 4 * key;
    } else {
      var alternate = input && input.alternate;
      if (!alternate || typeof alternate.childPresent !== 'boolean') return {boundary:'shared-pose-owner-input'};
      if (!alternate.childPresent) return {suppressed:true};
      if (typeof alternate.metadataPresent !== 'boolean') return {boundary:'shared-pose-metadata-input'};
      if (!alternate.metadataPresent) {
        if (key > 727) return {boundary:'shared-pose-table-input'};
        offset = 0x2123A0 + key * 4;
      } else {
        if (!Number.isInteger(alternate.type) || !Number.isInteger(alternate.projectedX)) return {boundary:'shared-pose-projection-input'};
        if (alternate.type === 29 || alternate.type === 30) {
          var meta = alternate.ownerMetaB;
          if (!Number.isInteger(meta)) return {boundary:'shared-pose-metadata-input'};
          if (meta === 51) key = 6;
          else if (meta === 47) key = 11;
          else if (meta >= 78 && meta <= 80) key = 10;
          else if (meta >= 56 && meta <= 68) key = 8;
          else if (alternate.classification === 2) key = 9;
          else if (alternate.classification === 3 || alternate.classification >= 5) key = 7;
          else if ([0,1,4].includes(alternate.classification) && [4,5].includes(alternate.predicateKey)) key = alternate.predicateKey;
          else return {boundary:'shared-pose-classification-input'};
        }
        var x = alternate.projectedX;
        offset = key >= 590 && key <= 760 ? 0x212880 + 2 * (key + (x >= 213 ? 0 : x >= 106 ? 1 : 2)) :
          0x2123A0 + 4 * key + (x >= 160 ? 2 : 0);
        if (offset < 0x2123A0 || offset + 2 > 0x212F00) return {boundary:'shared-pose-table-input'};
      }
    }
    var request = readHalfword && readHalfword(offset);
    if (!Number.isInteger(request)) return {boundary:'shared-pose-table-input'};
    return {context:opcode === 17 || opcode === 18 ? 'A' : 'B', request:request & 65535, tableOffset:offset};
  }

  function externalEventCount(events) {
    return Array.isArray(events) ? events.length : events.sequence.length;
  }
  function externalEventAt(events,index) {
    if (Array.isArray(events)) return events[index];
    var entry=events.sequence[index];
    return Object.assign({},events.templates[entry[1]],{tick:entry[0]});
  }
  function validateExternalProducers(value) {
    function integer(v,min,max) {return Number.isInteger(v) && v >= min && v <= max;}
    function id(v) {return typeof v === 'string' && v.length > 0 && v.length <= 160;}
    if (!value || !integer(value.throughTick,0,29999) || !['menuCreates','colorCreates','poseCalls'].every(function(k){return Array.isArray(value[k]);})) fail('External producers need bounded complete service history and creation lists.', 'launch-input');
    if(value.directorLaunch!==undefined&&!value.resourceSchedule)fail('Director initialization requires the computed resource scheduler.','launch-input');
    if(value.resourceSchedule!==undefined&&!value.initialDialogue)fail('Resource scheduling requires initial native resource memory.','launch-input');
    if (!Array.isArray(value.events)) {
      var eventTable=value.events;
      if (!eventTable || !Array.isArray(eventTable.templates) || !Array.isArray(eventTable.sequence) || eventTable.sequence.length>60000 ||
          eventTable.templates.some(function(event){return !event || typeof event!=='object' || Array.isArray(event) || Object.prototype.hasOwnProperty.call(event,'tick');}) ||
          eventTable.sequence.some(function(entry){return !Array.isArray(entry) || entry.length!==2 || !integer(entry[0],0,value.throughTick) || !integer(entry[1],0,eventTable.templates.length-1);})) {
        fail('Compact service history requires explicit tick/template pairs and templates without tick fields.','launch-input');
      }
    }
    if (value.initialMenusEmpty !== undefined && value.initialMenusEmpty !== true) fail('Initial menu ownership currently supports explicitly empty slots.', 'launch-input');
    if (value.initialColor !== undefined && value.initialColor !== null) {
      if (!id(value.initialColor.ownerId)) fail('Initial color needs owner identity.', 'launch-input');
      launchBytes(value.initialColor.recordHex,12);
    }
    if (value.initialRequests !== undefined && (!value.initialRequests || !['A','B'].every(function(k){return integer(value.initialRequests[k],-2147483648,2147483647);}))) fail('Initial shared requests must be signed words.', 'launch-input');
    if(value.dialogueCreates!==undefined) {
      if(!Array.isArray(value.dialogueCreates))fail('Dialogue creation outcomes must be an array.','launch-input');
      var dialogueIds=new Set(), dialogueOwners=new Set();
      value.dialogueCreates.forEach(function(row){
        var identity=row&&row.nodeId+':'+row.occurrence;
        if(!row||!id(row.nodeId)||!id(row.ownerId)||!integer(row.slot,0,5)||!integer(row.occurrence,0,29999)||dialogueIds.has(identity)||dialogueOwners.has(row.ownerId)||!Array.isArray(row.directorWords)||row.directorWords.length!==14||!row.directorWords.every(function(v){return integer(v,0,0xffffffff);}))fail('Dialogue creation requires unique native ownership and exact Director words.','launch-input');
        launchBytes(row.recordHex,0xa8);dialogueIds.add(identity);dialogueOwners.add(row.ownerId);
      });
    }
    ['menuCreates','colorCreates'].forEach(function(k) {
      var seen = new Set(), owners = new Set();
      value[k].forEach(function(row) {
        var identity=row && row.nodeId+':'+row.occurrence;
        if (!row || !id(row.nodeId) || !id(row.ownerId) || !integer(row.occurrence,0,29999) || seen.has(identity) || owners.has(row.ownerId)) fail('External creation identity is invalid or duplicated.', 'launch-input');
        seen.add(identity); owners.add(row.ownerId);
        if (k === 'colorCreates') {
          if (row.allocationReady !== true || row.registrationReturned !== true) fail('Color creation needs successful allocation and registration outcomes.', 'launch-input');
        } else if (!id(row.entityId) || !integer(row.preset,1,24) || !integer(row.substate,0,255) ||
            !['selection','cancel','optionCount'].every(function(f){return integer(row[f],-128,127);})) fail('Menu creation needs supported preset and native initial fields.', 'launch-input');
      });
    });
    var previous=-1;
    for(var eventIndex=0;eventIndex<externalEventCount(value.events);eventIndex++) {
      var event=externalEventAt(value.events,eventIndex);
      var order=event && event.tick*2+(event.phase==='after-director'?1:0);
      if (!event || !integer(event.tick,0,value.throughTick) || !['before-director','after-director'].includes(event.phase) || order<previous || !['menu','color','dialogue','request-reset','request-dispatch'].includes(event.kind)) fail('External service history must be ordered within its declared range.', 'launch-input');
      previous=order;
      if (event.kind.startsWith('request-')) {
        if (!['A','B'].includes(event.context)) fail('Request service needs context A or B.', 'launch-input');
      } else {
        if (!id(event.ownerId) || typeof event.eligible !== 'boolean') fail('Producer service needs owner identity and eligibility.', 'launch-input');
        if(event.kind==='dialogue') {
          if(!['initialize','callback','opening','closing','priority'].includes(event.service)||!Array.isArray(event.helpers))fail('Dialogue service needs its native phase and explicit helper outcomes.','launch-input');
          if(['initialize','callback'].includes(event.service)&&!integer(event.slot,0,5))fail('Dialogue callback slot must be within the native six-slot pool.','launch-input');
          if(event.controller&&!['actionMask','directionMask','dummyMask','historyMask','queueHead'].every(function(k){return integer(event.controller[k],0,65535);}))fail('Dialogue controller fields must be unsigned halfwords.','launch-input');
          if(!OB64.cutsceneDialogue)fail('Dialogue input support is unavailable.','launch-input');
          if(event.releaseHelpers!==undefined&&!Array.isArray(event.releaseHelpers))fail('Dialogue release outcomes must be an array.','launch-input');
          if(event.registeredOwners!==undefined&&(!Array.isArray(event.registeredOwners)||event.registeredOwners.some(function(row){return !row||!integer(row.slot,0,5)||!id(row.ownerId);})))fail('Dialogue registration outcomes require native slots and owner identities.','launch-input');
          try{event.helpers.concat(event.releaseHelpers||[]).forEach(OB64.cutsceneDialogue.validateHelper);}catch(error){fail(error.message,'launch-input');}
        } else if (event.kind==='menu') {
          if (!integer(event.slot,0,65535) || !id(event.entityId)) fail('Menu service identity is invalid.', 'launch-input');
          if (event.readiness && (!integer(event.readiness.readyByte,0,255) || !integer(event.readiness.alpha,-32768,32767) || !integer(event.readiness.cooldown,0,65535))) fail('Menu readiness fields are invalid.', 'launch-input');
          ['actionMask','directionMask'].forEach(function(k){if(event[k]!==undefined&&!integer(event[k],0,65535))fail('Controller masks must be unsigned halfwords.','launch-input');});
        }
      }
    }
    value.poseCalls.forEach(function(row) {
      if (!row || !id(row.actorId) || !integer(row.bank,-32768,32767) || !integer(row.stateIndex,-32768,32767) || !integer(row.recordOrdinal,0,255) || !integer(row.opcode,17,20)) fail('Pose service needs exact Actor and counted-record identity.', 'launch-input');
      if (row.projectionReturned!==undefined && typeof row.projectionReturned!=='boolean') fail('Projection outcome must be explicit.', 'launch-input');
      if (row.alternate!==undefined) {
        var a=row.alternate;
        if (!a || typeof a.childPresent!=='boolean' || a.metadataPresent!==undefined && typeof a.metadataPresent!=='boolean') fail('Alternate request needs explicit child and metadata presence.', 'launch-input');
        [['type',0,65535],['projectedX',-2147483648,2147483647],['ownerMetaB',0,4294967295],['classification',0,4294967295],['predicateKey',4,5]].forEach(function(f){
          if(a[f[0]]!==undefined&&!integer(a[f[0]],f[1],f[2]))fail('Alternate request scalar input is invalid.','launch-input');
        });
      }
    });
  }

  function launchBytes(hex, length) {
    if (typeof hex !== 'string' || hex.length !== length * 2 || !/^[0-9a-f]+$/i.test(hex)) {
      fail('Launch record must contain exactly ' + length + ' big-endian bytes.', 'launch-input');
    }
    var bytes = new Uint8Array(length);
    for (var i = 0; i < length; i++) bytes[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
    return new DataView(bytes.buffer);
  }

  // Lossless 48-byte native state keeps long-lived snapshot histories compact.
  // Cursor, delay and physical state remain signed words; materials remain bytes.
  function encodeNativeActorState(actor) {
    var bytes = new Uint8Array(48), view = new DataView(bytes.buffer);
    view.setInt32(0, actor.poseCursor || 0);
    view.setInt32(4, actor.poseDelay || 0);
    view.setInt32(8, Number.isInteger(actor.poseStateIndex) ? actor.poseStateIndex : -1);
    bytes[14] = Number.isInteger(actor.poseStateIndex) ? 0 : 1;
    bytes[12] = actor.decoderMode || 0;
    bytes[13] = Number.isInteger(actor.sourceRowOrdinal) ? actor.sourceRowOrdinal : 255;
    for (var i=0; i<16; i++) {
      bytes[16+i] = actor.material ? actor.material[i] : 255;
      bytes[32+i] = actor.materialDelta ? actor.materialDelta[i] : 0;
    }
    return btoa(String.fromCharCode.apply(null, bytes));
  }

  function decodeNativeActorState(encoded) {
    var raw = atob(encoded);
    if (raw.length !== 48) fail('Native Actor snapshot must contain 48 bytes.', 'native-actor-snapshot');
    var bytes = Uint8Array.from(raw, function(c) { return c.charCodeAt(0); });
    var view = new DataView(bytes.buffer);
    return { poseCursor:view.getInt32(0), poseDelay:view.getInt32(4), poseStateIndex:bytes[14] ? null : view.getInt32(8),
      decoderMode:bytes[12], sourceRowOrdinal:bytes[13],
      material:Array.from(bytes.slice(16,32)), materialDelta:Array.from(bytes.slice(32,48)) };
  }

  // Accepted unsigned-byte equivalence from func_00045E5C (CPR-R12).
  function nativeClassFamilyMatch(a, b) {
    a &= 255; b &= 255;
    return a === b || [[81,82,83],[84,85],[86,87],[95,96],[99,100]].some(function(group) {
      return group.indexOf(a) !== -1 && group.indexOf(b) !== -1;
    });
  }

  // Both native lookup paths use this 22-entry record-width table.
  var NATIVE_POSE_WIDTHS = [1,3,3,2,2,2,1,1,1,1,1,1,4,3,3,2,3,3,3,3,3,4];
  function decodeQualifiedPose(hex) {
    if (typeof hex !== 'string' || !hex.length || hex.length % 2) fail('Pose program bytes are required.', 'launch-input');
    var bytes = launchBytes(hex, hex.length / 2), cursor = 1, records = [];
    for (var i=0; i<bytes.getUint8(0); i++) {
      if (cursor >= bytes.byteLength) fail('Counted pose ends before its declared records.', 'launch-input');
      var opcode = bytes.getUint8(cursor), width = NATIVE_POSE_WIDTHS[opcode];
      if (!width || cursor + width > bytes.byteLength) fail('Counted pose opcode or extent is unsupported.', 'launch-input');
      var operands = [];
      for (var j=1; j<width; j++) operands.push(bytes.getUint8(cursor+j));
      records.push({opcode:opcode, operands:operands}); cursor += width;
    }
    if (cursor !== bytes.byteLength) fail('Counted pose contains bytes outside its declared extent.', 'launch-input');
    return {records:records};
  }

  function recordHex(view) {
    return Array.from(new Uint8Array(view.buffer, view.byteOffset, view.byteLength), function(v) {
      return v.toString(16).padStart(2,'0');
    }).join('');
  }

  function validateActorServiceGroup(key, value) {
    function integer(v, min, max) { return Number.isInteger(v) && v >= min && v <= max; }
    function tuple(row) {
      return row && integer(row.sourceArt,-2147483648,2147483647) &&
        integer(row.ownerContext,-2147483648,2147483647) && integer(row.flagA,0,255) && integer(row.flagB,-32768,32767);
    }
    if (key === 'poseRegistry') {
      if (!value || !Array.isArray(value.alternate) || !Array.isArray(value.ordinary)) fail('Pose registry needs both explicit directory lists.', 'launch-input');
      ['alternate','ordinary'].forEach(function(kind) {
        var identities = new Set();
        value[kind].forEach(function(row) {
          var validTuple=kind==='alternate' ? tuple(row) && integer(row.handle,1,4095) :
            row && integer(row.sourceArt,-2147483648,2147483647) && integer(row.flagA,0,255);
          if (!validTuple || !Array.isArray(row.programs)) fail('Pose registry tuple or handle is invalid.', 'launch-input');
          var identity = (kind === 'ordinary' ? [row.sourceArt,row.flagA] :
            [row.sourceArt,row.ownerContext,row.flagA,row.flagB]).join(':');
          if (identities.has(identity)) fail('Pose registry tuple is ambiguous.', 'launch-input');
          identities.add(identity);
          var states = new Set();
          row.programs.forEach(function(program) {
            if (!integer(program.state,-32768,32767) || states.has(program.state)) fail('Pose directory state is invalid or duplicated.', 'launch-input');
            states.add(program.state); decodeQualifiedPose(program.programHex);
          });
        });
      });
      return;
    }
    if (!Array.isArray(value)) fail(key + ' needs an ordered service list.', 'launch-input');
    var identities = new Set();
    value.forEach(function(row) {
      if (!row || typeof row.nodeId !== 'string' || !row.nodeId || !integer(row.occurrence,0,29999)) fail('Actor service needs command and occurrence identity.', 'launch-input');
      var identity = row.nodeId + ':' + row.occurrence;
      if (identities.has(identity)) fail('Actor service occurrence is ambiguous.', 'launch-input');
      identities.add(identity);
      if (key === 'bodyPoseSetups') {
        if (!Array.isArray(row.words) || row.words.length !== 7 || row.words.some(function(v){return !integer(v,-2147483648,4294967295);}) ||
            row.otherActorsUnchanged !== true) fail('Body setup requires exact command words and explicit unrelated-Actor preservation.', 'launch-input');
        launchBytes(row.recordHex,0x150);
      } else {
        if (!Array.isArray(row.allocations) || row.allocations.length > 2 || row.allocations.some(function(v){return typeof v !== 'boolean';}) ||
            !Array.isArray(row.preparations) || row.preparations.length > 3) fail('Subordinate service outcomes are invalid.', 'launch-input');
        row.preparations.forEach(function(preparation) {
          if (!tuple(preparation) || !integer(preparation.equipment,0,277) ||
              !['ready','unavailable','cache-full'].includes(preparation.status)) fail('Subordinate preparation needs its exact tuple, equipment, and status.', 'launch-input');
        });
      }
    });
  }

  function validateLaunchInputs(input, assetId) {
    if (input == null) return null;
    if (JSON.stringify(input).length > 131072 || input.schema !== 'ob64-cutscene-launch-inputs.v1' ||
        input.assetId !== assetId || typeof input.invocationId !== 'string' || !input.invocationId ||
        typeof input.sourceIdentity !== 'string' || !input.sourceIdentity ||
        !['Candidate', 'Supported', 'Verified', 'Editor-ready'].includes(input.evidenceGrade)) {
      fail('Launch inputs require matching resource, invocation, source identity, and evidence grade.', 'launch-input');
    }
    ['actorInputRows', 'existingActors', 'capturedSnapshot', 'capturedPresentation', 'currentUnitMembers', 'schedulerBranch',
      'poseRegistry','bodyPoseSetups','subordinateServices','rosterConstruction','rosterResets','rosterStateServices','externalProducers','directActorCreates'].forEach(function(key) {
      var group = input[key];
      if (!group) return;
      if (!['known', 'unknown'].includes(group.status)) fail(key + ' needs known or unknown status.', 'launch-input');
      if (group.status === 'unknown') return;
      var value = group.value;
      if (key === 'directActorCreates') {
        if (!Array.isArray(value) || value.length > 256) fail('Direct creation requires a bounded service list.','actor-creation-input');
        var creationIds = new Set();
        value.forEach(function(row) {
          var identity = row && row.nodeId + ':' + row.occurrence;
          if (!row || typeof row.nodeId !== 'string' || !row.nodeId || !Number.isInteger(row.occurrence) || row.occurrence < 0 ||
              creationIds.has(identity) || !Array.isArray(row.words) || row.words.length !== 10 ||
              !row.words.every(function(v){return Number.isInteger(v) && v >= 0 && v <= 0xFFFFFFFF;}) ||
              row.registration !== 'existing' || typeof row.evidenceReference !== 'string' || !row.evidenceReference ||
              !Number.isInteger(row.allocationAddress) || row.allocationAddress < 0x80000000 || row.allocationAddress + 336 > 0x80400000 || row.allocationAddress % 4 ||
              !Number.isInteger(row.presentationByte) || row.presentationByte < 0 || row.presentationByte > 255 ||
              !Number.isInteger(row.stateIndex) || row.stateIndex < 0 || row.stateIndex > 32767) {
            fail('Direct creation requires exact command identity, allocation, existing registration, State result, and evidence reference.','actor-creation-input');
          }
          creationIds.add(identity);
        });
      } else if (key === 'capturedPresentation') {
        if (!input.capturedSnapshot || input.capturedSnapshot.status !== 'known' ||
            ![0,2].includes(input.capturedSnapshot.value.sceneMode) || !value ||
            value.scope !== (input.capturedSnapshot.value.sceneMode===2?'mode-two-main-actor-geometry':'mode-zero-main-actor-geometry') ||
            !Array.isArray(value.channels) || value.channels.length !== 20 ||
            !value.actorCamera || (input.capturedSnapshot.value.sceneMode===0 && value.actorCamera.modelScale !== 1)) {
          fail('Captured presentation requires a supported scene mode, its matching geometry scope, and twenty explicit channel entries.', 'captured-presentation-input');
        }
        if(input.capturedSnapshot.value.sceneMode===2 && value.channels.some(function(c){return c!==null;}))fail('Direct mode-two geometry does not consume mode-zero scene channels.','captured-presentation-input');
        ['actorCamera', 'registeredCamera'].forEach(function(name) {
          var camera = value[name];
          if (!camera || !Array.isArray(camera.values) || camera.values.length !== 14 ||
              !camera.values.every(Number.isFinite) || !Number.isFinite(camera.modelScale) || camera.modelScale <= 0 ||
              camera.values[0] < 1 || camera.values[0] >= 179 || camera.values[1] <= 0 ||
              camera.values[2] <= 0 || camera.values[3] <= camera.values[2] || camera.values[4] <= 0) {
            fail('Captured camera requires fourteen finite native values and a positive model scale.', 'captured-presentation-input');
          }
          var a=camera.values, dx=a[8]-a[5], dy=a[9]-a[6], dz=a[10]-a[7];
          var cx=dy*a[13]-dz*a[12], cy=dz*a[11]-dx*a[13], cz=dx*a[12]-dy*a[11];
          if (!(dx*dx+dy*dy+dz*dz>0) || !(cx*cx+cy*cy+cz*cz>0)) {
            fail('Captured camera eye, target and up must define a basis.', 'captured-presentation-input');
          }
        });
        value.channels.forEach(function(channel) {
          if (channel === null) return;
          if (!channel || !['translateX','translateY','translateZ','rotationX','rotationY','uniformScale'].every(function(k){return Number.isFinite(channel[k]);}) ||
              channel.rotationX !== 0 || channel.rotationY !== 0 || channel.uniformScale !== 1) {
            fail('Captured main-Actor geometry currently requires unit-scale unrotated scene channels.', 'captured-presentation-input');
          }
        });
        input.capturedSnapshot.value.slots.forEach(function(row) {
          if (!row) return;
          var view=launchBytes(row.recordHex,0x150);
          var channel=view.getUint8(0x13E), scale=view.getFloat32(0x104,false);
          if ((input.capturedSnapshot.value.sceneMode===0 && !value.channels[channel]) || !(scale>0) || !Number.isFinite(scale) ||
              view.getFloat32(0x108,false)!==scale || view.getFloat32(0x10C,false)!==scale || (input.capturedSnapshot.value.sceneMode===0 && view.getUint8(0x13D)!==0)) {
            fail('Captured Actor geometry requires positive uniform scale and the channel and decoder prerequisites for its scene mode.', 'captured-presentation-input');
          }
        });
      } else if (key === 'externalProducers') {
        validateExternalProducers(value);
      } else if (key === 'rosterStateServices') {
        if(!Array.isArray(value))fail('Roster State services require ordered occurrences.','launch-input');
        var stateServiceIds=new Set();
        value.forEach(function(service){
          if(!service||typeof service.nodeId!=='string'||!Number.isInteger(service.occurrence)||service.occurrence<0||service.occurrence>=30000||stateServiceIds.has(service.nodeId+':'+service.occurrence)||!Array.isArray(service.appearances)||!Array.isArray(service.preparations))fail('Roster State service identity is invalid.','launch-input');
          stateServiceIds.add(service.nodeId+':'+service.occurrence);
          function effects(effect){if(!effect)return;['actors','rows'].forEach(function(k){if(effect[k]!==undefined&&!Array.isArray(effect[k]))fail('State service effects require arrays.','launch-input');});
            if(new Set((effect.actors||[]).map(function(a){return a.slot;})).size!==(effect.actors||[]).length||new Set((effect.rows||[]).map(function(r){return r.ordinal;})).size!==(effect.rows||[]).length)fail('State effects must identify each changed record once.','launch-input');
            (effect.actors||[]).forEach(function(a){if(!Number.isInteger(a.slot)||a.slot<0||a.slot>=28)fail('Invalid effect slot.','launch-input');launchBytes(a.beforeRecordHex,336);launchBytes(a.afterRecordHex,336);});
            (effect.rows||[]).forEach(function(r){if(!Number.isInteger(r.ordinal)||r.ordinal<0||r.ordinal>=20)fail('Invalid effect row.','launch-input');launchBytes(r.beforeHex,248);launchBytes(r.afterHex,248);});}
          service.appearances.forEach(function(a){if(!a||!Number.isInteger(a.slot)||!Array.isArray(a.halfwords)||a.halfwords.length!==4||a.halfwords.some(function(v){return !Number.isInteger(v)||v<0||v>65535;})||!Number.isInteger(a.response)||a.response<-2147483648||a.response>4294967295||!['returned','unavailable'].includes(a.status))fail('Invalid appearance response.','launch-input');effects(a.effects);});
          service.preparations.forEach(function(p){if(!p||!Number.isInteger(p.slot)||!Array.isArray(p.values)||p.values.length!==5||p.values.some(function(v){return !Number.isInteger(v)||v<0||v>4294967295;})||!['returned','unavailable'].includes(p.status))fail('Invalid setup preparation response.','launch-input');if(p.localValues!==undefined&&(!Array.isArray(p.localValues)||p.localValues.length!==5||p.localValues.some(function(v){return !Number.isInteger(v)||v<0||v>4294967295;})))fail('Invalid local preparation effects.','launch-input');effects(p.effects);});
          if(service.poseEffects!==undefined&&!Array.isArray(service.poseEffects)||service.markerLookups!==undefined&&!Array.isArray(service.markerLookups))fail('Invalid pose/marker service list.','launch-input');
          if(new Set((service.poseEffects||[]).map(function(p){return p.call;})).size!==(service.poseEffects||[]).length)fail('Pose effects must identify each call once.','launch-input');
          (service.poseEffects||[]).forEach(function(p){if(!Number.isInteger(p.call)||p.call<0||!Number.isInteger(p.slot))fail('Invalid pose effect call.','launch-input');launchBytes(p.beforeRecordHex,336);effects(p.effects);});
          (service.markerLookups||[]).forEach(function(p){if(!p||!Number.isInteger(p.opcode)||p.opcode<-2147483648||p.opcode>4294967295)fail('Invalid marker lookup response.','launch-input');effects(p.effects);});
        });
      } else if (key === 'rosterResets') {
        if(!Array.isArray(value))fail('Roster resets need ordered command occurrences.','launch-input');
        var resetIds=new Set();
        value.forEach(function(reset) {
          if(!reset||typeof reset.nodeId!=='string'||!Number.isInteger(reset.occurrence)||reset.occurrence<0||reset.occurrence>=30000||resetIds.has(reset.nodeId+':'+reset.occurrence))fail('Reset occurrence identity is invalid.','launch-input');
          resetIds.add(reset.nodeId+':'+reset.occurrence);
          if(reset.unitHex!==null)launchBytes(reset.unitHex,25);
          if(!Number.isInteger(reset.sceneRoot)||reset.sceneRoot<=0||reset.sceneRoot>4294940000||!Number.isInteger(reset.currentUnit)||reset.currentUnit<0||reset.currentUnit>255||!reset.records||!reset.objects||!reset.scratch||
              !['primaryRegistry','secondaryRegistry','releases','rowFinalizers','descriptions','preparations','decodes','random'].every(function(k){return Array.isArray(reset[k]);}))fail('Reset memory and service groups must be explicit.','launch-input');
          Object.keys(reset.records).forEach(function(k){if(!/^\d+$/.test(k)||Number(k)>99)fail('Invalid deployed record ID.','launch-input');launchBytes(reset.records[k],52);});
          Object.keys(reset.objects).forEach(function(k){if(!/^\d+$/.test(k)||Number(k)<=0||Number(k)>4294967295)fail('Invalid child pointer.','launch-input');launchBytes(reset.objects[k],0x100);});
          ['primaryRegistry','secondaryRegistry'].forEach(function(k){if(reset[k].length>256||reset[k].some(function(p){return !Number.isInteger(p)||p<=0||p>4294967295;}))fail('Reset registry backing is invalid.','launch-input');});
          ['art','handle','variant','orientationA','orientationB','context'].forEach(function(k){if(!Array.isArray(reset.scratch[k])||reset.scratch[k].length!==9||reset.scratch[k].some(function(v){return v!==null&&(!Number.isInteger(v)||v<0||v>4294967295);}))fail('Reset scratch requires nine qualified or unavailable words per array.','launch-input');});
          reset.rowFinalizers.forEach(function(f){if(!f||!Number.isInteger(f.row)||f.row<0||f.row>=20||![0,1].includes(f.mode)||!['returned','unavailable'].includes(f.status)||!f.objects)fail('Invalid reset finalizer response.','launch-input');launchBytes(f.beforeRowHex,248);launchBytes(f.afterRowHex,248);Object.keys(f.objects).forEach(function(p){if(!/^\d+$/.test(p)||Number(p)<=0||Number(p)>4294967295)fail('Invalid returned child pointer.','launch-input');launchBytes(f.objects[p],256);});});
          reset.descriptions.forEach(function(d){if(!d||!Number.isInteger(d.row)||d.row<0||d.row>=20||!Number.isInteger(d.variant)||!Number.isInteger(d.handle)||d.handle<0||d.handle>4294967295)fail('Invalid reset description result.','launch-input');launchBytes(d.rowHex,248);});
        });
      } else if (key === 'rosterConstruction') {
        if (!value || !Number.isInteger(value.presentationByte) || value.presentationByte<0 || value.presentationByte>255 ||
            !Number.isInteger(value.route) || value.route < -128 || value.route > 127 || !Array.isArray(value.links)) fail('Roster construction needs qualified route, presentation byte, and linked objects.','launch-input');
        var pointers=new Set();
        value.links.forEach(function(link) {
          if (!link || !Number.isInteger(link.pointer) || link.pointer<=0 || link.pointer>4294967295 || pointers.has(link.pointer) ||
              ![link.x,link.y,link.z].every(function(v){return Number.isInteger(v)&&v>=-32768&&v<=32767;}) ||
              typeof link.allocationSucceeded!=='boolean' ||
              (link.terrainHeight!==null && (!Number.isFinite(link.terrainHeight)||Math.fround(link.terrainHeight)!==link.terrainHeight))) fail('Roster linked objects need unique pointers, signed coordinates, allocation and float-height qualification.','launch-input');
          pointers.add(link.pointer);
        });
        if (value.excludedRows!==undefined && (!Array.isArray(value.excludedRows)||value.excludedRows.length!==20||value.excludedRows.some(function(v){return v!==null&&typeof v!=='boolean';}))) fail('Roster exclusion results need 20 known or unavailable entries.','launch-input');
        if(value.sceneKey!==undefined&&(!Number.isInteger(value.sceneKey)||value.sceneKey<-32768||value.sceneKey>32767))fail('Roster State selection needs a signed scene key.','launch-input');
      } else if (['poseRegistry','bodyPoseSetups','subordinateServices'].includes(key)) {
        validateActorServiceGroup(key,value);
      } else if (key === 'actorInputRows') {
        if (!Array.isArray(value) || value.length !== 20) fail('Actor inputs require all 20 rows.', 'launch-input');
        value.forEach(function(hex) { launchBytes(hex, 0xF8); });
      } else if (key === 'currentUnitMembers') {
        if (!Array.isArray(value) || value.length !== 5 || value.some(function(id) {
          return !Number.isInteger(id) || id < 0 || id > 255;
        })) fail('Current-unit inputs require five byte member IDs.', 'launch-input');
      } else if (key === 'schedulerBranch') {
        if (!['normal', 'alternate'].includes(value)) fail('Scheduler branch must be normal or alternate.', 'launch-input');
      } else {
        var captured = key === 'capturedSnapshot';
        if (captured && (!value || value.resumeState !== 'unknown' || value.otherJobOwners !== 'unknown' ||
            Object.prototype.hasOwnProperty.call(value, 'otherJobsEmpty') ||
            !Number.isInteger(value.sceneMode) || value.sceneMode < 0 || value.sceneMode > 255 ||
            !Number.isInteger(value.observedParserCursor) || value.observedParserCursor < 0 || value.observedParserCursor > 0xFFFFFFFF ||
            input.existingActors && input.existingActors.status === 'known' ||
            input.externalProducers && input.externalProducers.status === 'known' &&
              !(input.capturedResume && input.capturedResume.status === 'known' && input.capturedResume.value &&
                input.capturedResume.value.entry === 'normal-director-continuous'))) {
          fail('Captured snapshots require unknown resume and other-job ownership, captured mode/cursor, and no launch or service-history substitution.', 'launch-input');
        }
        if (!value || !captured && value.otherJobsEmpty !== true || !Array.isArray(value.slots) || value.slots.length !== 28) {
          fail('Existing Actors require 28 slots and explicit empty unsupported job owners.', 'launch-input');
        }
        var identities = new Set();
        value.slots.forEach(function(row, slot) {
          if (row === null) return;
          if (!row || typeof row.identity !== 'string' || !row.identity || identities.has(row.identity)) {
            fail('Existing Actor identities must be present and unique.', 'launch-input');
          }
          identities.add(row.identity);
          var bytes = launchBytes(row.recordHex, 0x150);
          if (bytes.getInt32(0xE4) !== slot || [0x11C,0x120,0x124].some(function(at) {
            return !Number.isFinite(bytes.getFloat32(at));
          })) fail('Existing Actor slot identity or coordinates are invalid.', 'launch-input');
          if (row.movementHex !== null) {
            var job = launchBytes(row.movementHex, 16);
            if (!Number.isFinite(job.getFloat32(0)) || !Number.isFinite(job.getFloat32(8))) fail('Movement input must be finite.', 'launch-input');
          }
        });
      }
    });
    if (input.capturedResume) {
      var resumeGroup = input.capturedResume;
      if (!['known','unknown'].includes(resumeGroup.status)) fail('Invalid captured resume status.','launch-input');
      if (resumeGroup.status === 'known') {
        var resume = resumeGroup.value, capturedValue = input.capturedSnapshot && input.capturedSnapshot.value;
        var heldModeTwo=resume && ['normal-mode-two-held-movement-window','normal-mode-two-continuous'].includes(resume.entry);
        if (!resume || !input.capturedSnapshot || input.capturedSnapshot.status !== 'known' ||
            !input.schedulerBranch || input.schedulerBranch.status !== 'known' || input.schedulerBranch.value !== 'normal' ||
            capturedValue.sceneMode !== (heldModeTwo?2:0) || !['normal-director-held-movement-query','normal-director-continuous','normal-mode-two-held-movement-window','normal-mode-two-continuous'].includes(resume.entry) ||
            resume.origin !== 'prospective-captured-state' ||
            (resume.entry === 'normal-director-continuous' || heldModeTwo ? !Number.isInteger(resume.updates) || resume.updates < 1 || resume.updates > 30000 || resume.directorControllerMask !== 0 : resume.updates !== 1) ||
            resume.registeredCounter !== 0 || !Number.isInteger(resume.tailTimer) || resume.tailTimer < -128 || resume.tailTimer >= 0) {
          fail('Resume requires an explicit supported prospective entry and qualified counter/timer state.','resume-input');
        }
        var primary = launchBytes(resume.primaryOwnerHex,0x1CB2), secondary = heldModeTwo && resume.secondaryOwnerAddress===0 && resume.secondaryOwnerHex===null ? null : launchBytes(resume.secondaryOwnerHex,0x844);
        if (primary.getUint8(0x1CB1) !== 0 || (secondary && (secondary.getInt32(0x824) !== 0 ||
            secondary.getInt32(0x82C) !== 0 || secondary.getUint8(0x840) !== 0))) {
          fail('Resume requires qualified inactive secondary scheduler fields.','resume-job-input');
        }
        [[0x1BB4,20],[0x1BB0,1],[0x19B4,1],[0x1A4C,1],[0x1A44,1],[0x19F4,20],
          [0x1A50,28],[0x168,28],[0x1D8,28],[0x2B8,28],[0x1CA4,1]].forEach(function(range) {
          for (var i=0;i<range[1];i++) if (primary.getUint32(range[0]+i*4)!==0) {
            fail('Resume requires the guarded non-Actor owner tables to be empty.','resume-job-input');
          }
        });
        var root = launchBytes(resume.menuRootHex,0xD8);
        var ranges=[[0x8018FDC0,0x8018FDC4],[0x8022A950,0x8022A99C],
          [0x801CFC70,0x801CFC71],[0x800E7A32,0x800E7A33]];
        function range(address,size) {
          if (!Number.isInteger(address) || address < 0x80000000 || address+size > 0x80400000 || address%4 ||
              ranges.some(function(other){return address<other[1] && other[0]<address+size;})) {
            fail('Qualified resume memory regions must be valid and disjoint.','resume-memory-input');
          }
          ranges.push([address,address+size]);
        }
        range(resume.primaryOwnerAddress,0x1CB2);if(secondary)range(resume.secondaryOwnerAddress,0x844);range(resume.menuRootAddress,0xD8);
        if (root.getUint32(4)!==0) fail('This resume boundary requires an empty selected menu list.','resume-menu-input');
        if (!Array.isArray(resume.menuOwners) || resume.menuOwners.length!==14) fail('Resume requires fourteen menu owner slots.','resume-menu-input');
        resume.menuOwners.forEach(function(owner,slot) {
          var pointer=primary.getUint32(0x19B8+slot*4);
          if (owner===null) {if(pointer!==0)fail('Menu owner pointer disagrees with its supplied record.','resume-menu-input');return;}
          if (!owner || owner.address!==pointer) fail('Menu owner pointer disagrees with its supplied record.','resume-menu-input');
          launchBytes(owner.recordHex,22);range(pointer,22);
        });
        capturedValue.slots.forEach(function(row,slot) {
          var actorPointer=primary.getUint32(0x18+slot*4),movementPointer=primary.getUint32(0xF8+slot*4);
          if (!row) {if(actorPointer || movementPointer)fail('Empty Actor slot has a native owner.','resume-memory-input');return;}
          range(actorPointer,0x150);
          if (!heldModeTwo && launchBytes(row.recordHex,0x150).getUint8(0x13D)!==0) fail('Resume currently requires ordinary captured Actors.','resume-pose-input');
          if (row.movementHex===null) {if(movementPointer)fail('Movement pointer lacks its record.','resume-memory-input');}
          else {
            var movement=launchBytes(row.movementHex,16);
            if (movement.getUint8(14)!==0 || movement.getInt16(12)<=1) {
              fail('Resume movement must advance without pause, wrap, or allocator cleanup.','resume-movement-input');
            }
            range(movementPointer,16);
          }
        });
        if (input.directActorCreates && input.directActorCreates.status === 'known') {
          input.directActorCreates.value.forEach(function(row) { range(row.allocationAddress,336); });
        }
      }
    }
    return M.cloneJson(input, 'launch inputs');
  }
  function launchTranslationIndex(value) {
    value = unsigned(value);
    return (value & 0xFFFFFF00) === 0x08880000 ? value & 0xFF : null;
  }
  function poseId(bank, key, facing) {
    return 'cutscene-pose:' + bank + ':' + key + ':' + facing;
  }
  function uniquePush(rows, value) {
    if (value && rows.indexOf(value) === -1) rows.push(value);
  }

  function readU32(bytes, offset, label) {
    if (!(bytes instanceof Uint8Array) || offset < 0 || offset + 4 > bytes.length) {
      fail((label || 'Director resource') + ' ends before byte ' + (offset + 4) + '.',
        'scene-transform-bounds');
    }
    return ((bytes[offset] << 24) | (bytes[offset + 1] << 16) |
      (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
  }

  function readS32(bytes, offset, label) {
    return readU32(bytes, offset, label) | 0;
  }

  function identityTransformChannel() {
    return {
      rotationX: 0,
      rotationY: 0,
      translateX: 0,
      translateY: 0,
      translateZ: 0,
      uniformScale: 1
    };
  }

  function identityTransformChannels() {
    var channels = [];
    for (var index = 0; index < SCENE_TRANSFORM_CHANNEL_COUNT; index++) {
      channels.push(identityTransformChannel());
    }
    return channels;
  }

  function decodeSceneTransformResource(z64, resourceIndex) {
    if (!(z64 instanceof Uint8Array)) {
      fail('Normalized z64 bytes are required to decode a scene transform.',
        'scene-transform-rom');
    }
    if (!OB64.art || typeof OB64.art.readResource !== 'function' ||
        typeof OB64.art.readCompressedResource !== 'function') {
      fail('art.js is required to decode scene-transform resources.',
        'scene-transform-codec');
    }
    if (!Number.isInteger(resourceIndex) || resourceIndex < 0) {
      fail('Scene-transform resource index must be a non-negative integer.',
        'scene-transform-index');
    }
    var table = OB64.art.readResource(z64, SCENE_TRANSFORM_TABLE_KEY).stored;
    var tableOffset = resourceIndex * 4;
    if (tableOffset + 4 > table.length) {
      fail('Scene-transform resource index ' + resourceIndex + ' exceeds the ' +
        Math.floor(table.length / 4) + '-entry table.', 'scene-transform-index');
    }
    var resourceKey = readU32(table, tableOffset, 'Scene-transform resource table');
    var decoded = OB64.art.readCompressedResource(z64, resourceKey).decoded;
    var keyframeCount = readU32(decoded, 0, 'Scene-transform resource');
    var groupCount = readU32(decoded, 4, 'Scene-transform resource');
    var keyframeEnd = 8 + keyframeCount * SCENE_TRANSFORM_KEYFRAME_BYTES;
    if (keyframeCount > 0x100 || groupCount > 0x100 || keyframeEnd > decoded.length) {
      fail('Scene-transform resource ' + resourceIndex + ' has an invalid header.',
        'scene-transform-header');
    }
    var keyframes = [];
    for (var keyframeIndex = 0; keyframeIndex < keyframeCount; keyframeIndex++) {
      var keyframeBase = 8 + keyframeIndex * SCENE_TRANSFORM_KEYFRAME_BYTES;
      var channels = [];
      for (var channelIndex = 0;
          channelIndex < SCENE_TRANSFORM_CHANNEL_COUNT; channelIndex++) {
        var channel = {};
        SCENE_TRANSFORM_FIELDS.forEach(function(field) {
          channel[field.name + 'Raw'] = readS32(decoded,
            keyframeBase + field.offset + channelIndex * 4,
            'Scene-transform keyframe');
          channel[field.name] = channel[field.name + 'Raw'] / 1000;
        });
        channels.push(channel);
      }
      keyframes.push(channels);
    }
    var groups = [];
    var cursor = keyframeEnd;
    for (var groupIndex = 0; groupIndex < groupCount; groupIndex++) {
      var keyframeIds = [];
      var continuationWords = [];
      while (keyframeIds.length < SCENE_TRANSFORM_CHANNEL_COUNT) {
        var keyframeId = readS32(decoded, cursor,
          'Scene-transform sequence group ' + groupIndex);
        var continuation = readS32(decoded, cursor + 4,
          'Scene-transform sequence group ' + groupIndex);
        cursor += 8;
        if (keyframeId < 0 || keyframeId >= keyframeCount) {
          fail('Scene-transform group ' + groupIndex + ' selects invalid keyframe ' +
            keyframeId + '.', 'scene-transform-keyframe');
        }
        keyframeIds.push(keyframeId);
        continuationWords.push(continuation);
        if (continuation === -1) break;
      }
      if (!keyframeIds.length || continuationWords[continuationWords.length - 1] !== -1) {
        fail('Scene-transform group ' + groupIndex + ' has no terminator within 20 IDs.',
          'scene-transform-sequence');
      }
      groups.push({ keyframeIds: keyframeIds, continuationWords: continuationWords });
    }
    if (cursor !== decoded.length) {
      fail('Scene-transform resource ' + resourceIndex + ' has ' +
        (decoded.length - cursor) + ' unowned decoded bytes.', 'scene-transform-layout');
    }
    return {
      resourceIndex: resourceIndex,
      resourceKey: resourceKey,
      keyframeCount: keyframeCount,
      groupCount: groupCount,
      keyframes: keyframes,
      groups: groups
    };
  }

  function clonePoint(value, fallback) {
    value = value || {};
    fallback = fallback || { x: 0, y: 0, z: 0 };
    return {
      x: finite(value.x, fallback.x),
      y: finite(value.y, fallback.y),
      z: finite(value.z, fallback.z)
    };
  }

  function defaultCamera() {
    return {
      target: { x: 0, y: 0, z: 0 },
      eye: { x: 0, y: 0, z: 340 },
      fovYDegrees: 38,
      modelScale: 0.1,
      aspect: 4 / 3,
      near: 1,
      far: 5000,
      up: { x: 0, y: 1, z: 0 },
      sourceNodeId: null,
      evidenceStatus: 'external-unresolved',
      status: 'unresolved launch camera fallback'
    };
  }

  function modeZeroCamera(bank) {
    var camera = defaultCamera();
    camera.eye = bank === 'actor'
      ? { x: 360, y: 0, z: 0 }
      : { x: 0, y: 0, z: 360 };
    camera.sourceNodeId = 'mode-zero-camera-initializer';
    camera.evidenceStatus = 'native-static';
    camera.status = 'exact mode-zero ' + bank + ' camera initializer';
    return camera;
  }

  function cameraFromProjection(projection, status, sourceNodeId) {
    var camera = defaultCamera();
    camera.target = clonePoint(projection.target, camera.target);
    camera.eye = clonePoint(projection.eye, camera.eye);
    camera.up = clonePoint(projection.up, camera.up);
    camera.fovYDegrees = finite(projection.fovYDegrees, camera.fovYDegrees);
    camera.modelScale = finite(projection.modelScale, camera.modelScale);
    camera.aspect = finite(projection.aspect, camera.aspect);
    camera.near = finite(projection.near, camera.near);
    camera.far = finite(projection.far, camera.far);
    camera.screenWidth = finite(projection.screenWidth, 320);
    camera.screenHeight = finite(projection.screenHeight, 240);
    camera.sourceNodeId = sourceNodeId || 'runtime-observation';
    camera.evidenceStatus = projection.evidenceStatus || 'runtime-observed';
    camera.status = status || projection.calibrationStatus ||
      'runtime-observed Actor camera';
    return camera;
  }

  function directorModeFromProfile(scene) {
    var mode = scene && scene.launchProfile && scene.launchProfile.directorMode;
    if (!mode) fail('The Director scene has no launch profile.', 'missing-launch-profile');
    return {
      value: mode.value,
      status: mode.status,
      evidenceStatus: mode.evidenceStatus,
      source: mode.source
    };
  }

  function cameraFromLaunchProfile(profile, bank) {
    if (!profile) return defaultCamera();
    if (profile.kind === 'mode-zero-initializer') return modeZeroCamera(bank);
    if (profile.projection) {
      var projectedCamera = cameraFromProjection(profile.projection, profile.status,
        'launch-profile:' + profile.kind);
      projectedCamera.evidenceStatus = profile.evidenceStatus ||
        projectedCamera.evidenceStatus;
      return projectedCamera;
    }
    var camera = defaultCamera();
    camera.evidenceStatus = profile.evidenceStatus || 'external-unresolved';
    camera.status = profile.status;
    return camera;
  }

  function projectionFromCamera(camera, status) {
    camera = camera || defaultCamera();
    return {
      mode: 'native-perspective-runtime',
      status: status || camera.status || 'Director camera bank',
      modelScale: finite(camera.modelScale, 0.1),
      eye: clonePoint(camera.eye),
      target: clonePoint(camera.target),
      up: clonePoint(camera.up, { x: 0, y: 1, z: 0 }),
      fovYDegrees: clamp(finite(camera.fovYDegrees, 38), 1, 179),
      aspect: finite(camera.aspect, 4 / 3),
      near: finite(camera.near, 1),
      far: finite(camera.far, 5000),
      screenWidth: finite(camera.screenWidth, 320),
      screenHeight: finite(camera.screenHeight, 240),
      sourceNodeId: camera.sourceNodeId || null,
      evidenceStatus: camera.evidenceStatus || 'external-unresolved'
    };
  }

  function backgroundLayerRole(asset, index) {
    return index === 0 ? 'environment-base' : 'ordered-layer';
  }

  function backgroundLayers(entry, capability, catalog) {
    if (!entry || !Array.isArray(entry.archiveAssetIds)) return [];
    var members = Array.isArray(entry.members) &&
      entry.members.length === entry.archiveAssetIds.length
      ? entry.members : entry.archiveAssetIds.map(function(assetId, index) {
        return { ordinal: index, assetId: assetId };
      });
    return members.map(function(member, index) {
      var nativeOrdinal = Number.isFinite(member.ordinal) ? member.ordinal : index;
      var asset = catalog && catalog.getImageAsset
        ? catalog.getImageAsset(member.assetId) : null;
      var role = backgroundLayerRole(asset, index);
      return {
        id: role === 'environment-base' ? 'background:base' : 'background:layer:' + index,
        assetId: member.assetId,
        label: member.assetId,
        visible: true,
        depth: nativeOrdinal,
        nativeOrdinal: nativeOrdinal,
        role: role,
        capability: capability || M.capabilities.PREVIEW_ONLY,
        source: {
          sourceKind: 'director-runtime-background',
          selector: entry.selector,
          groupResourceKey: entry.groupResourceKey || null,
          traversalOrdinal: nativeOrdinal,
          associationStatus: entry.associationStatus || 'selector-table association'
        }
      };
    });
  }

  function modeTwoStageProps(catalog, foregroundSelector) {
    return catalog && Number.isInteger(foregroundSelector) &&
      typeof catalog.getModeTwoStagePlacementProfile === 'function'
      ? catalog.getModeTwoStagePlacementProfile(foregroundSelector) : null;
  }

  function hasRenderableStageProps(stageProps) {
    return !!(stageProps && ((stageProps.orthographicPlacements || []).length ||
      (stageProps.perspectivePlacements || []).length));
  }

  function stagedBackground(stageLayers, document, metadata) {
    metadata = metadata || {};
    stageLayers = Array.isArray(stageLayers) ? stageLayers : [];
    if (!stageLayers.length && !hasRenderableStageProps(metadata.nativeSceneProps)) return null;
    var layers = stageLayers.map(function(layer, index) {
      var role = layer.role || (index ? 'ordered-layer' : 'environment-base');
      return {
        id: role === 'environment-base' ? 'background:base' : 'background:layer:' + index,
        assetId: layer.assetId,
        label: layer.assetId,
        visible: true,
        depth: Number.isFinite(layer.depth) ? layer.depth : index,
        nativeOrdinal: Number.isFinite(layer.nativeOrdinal)
          ? layer.nativeOrdinal : (Number.isFinite(layer.depth) ? layer.depth : index),
        role: role,
        capability: M.capabilities.PREVIEW_ONLY,
        source: {
          sourceKind: metadata.sourceKind || 'staged-background',
          evidenceStatus: layer.evidenceStatus,
          associationStatus: layer.associationStatus,
          traversalOrdinal: Number.isFinite(layer.nativeOrdinal)
            ? layer.nativeOrdinal : (Number.isFinite(layer.depth) ? layer.depth : index)
        }
      };
    });
    var projection = M.cloneJson(document.background.projection || {},
      'background.projection');
    if (metadata.nativeSceneProps) {
      projection.nativeSceneProps = M.cloneJson(metadata.nativeSceneProps,
        'mode-two native Stage placements');
    }
    return {
      assetId: layers.length ? layers[0].assetId : null,
      layers: layers,
      capability: M.capabilities.PREVIEW_ONLY,
      projection: projection,
      runtimeStatus: metadata.runtimeStatus || 'The launch profile supplies a complete Stage.',
      selectorTableId: metadata.selectorTableId || null,
      selector: Number.isInteger(metadata.selector) ? metadata.selector : null,
      environmentSelector: Number.isInteger(metadata.environmentSelector)
        ? metadata.environmentSelector : null,
      foregroundSelectorTableId: metadata.foregroundSelectorTableId || null,
      foregroundSelector: Number.isInteger(metadata.foregroundSelector)
        ? metadata.foregroundSelector : null,
      foregroundSelectorCandidates: Array.isArray(metadata.foregroundSelectorCandidates)
        ? metadata.foregroundSelectorCandidates.slice() : [],
      foregroundStatus: metadata.foregroundStatus || null
    };
  }

  function observationBackground(scene, document, catalog) {
    var observation = scene && scene.backgroundRuntimeObservation;
    if (!observation) return null;
    return stagedBackground(observation.stageLayers, document, {
      sourceKind: 'runtime-observation',
      runtimeStatus: observation.associationStatus,
      selectorTableId: observation.selectorTableId,
      selector: Number.isInteger(observation.environmentSelector)
        ? observation.environmentSelector : observation.commandOperand,
      environmentSelector: observation.environmentSelector,
      foregroundSelectorTableId: observation.directorMode === 2
        ? 'background-table:mode2-overlay:80' : null,
      foregroundSelector: observation.foregroundSelector,
      nativeSceneProps: observation.directorMode === 2
        ? modeTwoStageProps(catalog, observation.foregroundSelector) : null
    });
  }

  function profileBackground(request, document, catalog) {
    if (!request) return null;
    return stagedBackground(request.stageLayers, document, {
      sourceKind: request.sourceKind === 'parent-event-predecessor'
        ? 'parent-event-predecessor' : 'launch-profile',
      runtimeStatus: request.status,
      selectorTableId: request.selectorTableId,
      selector: request.selector,
      environmentSelector: request.environmentSelector,
      foregroundSelectorTableId: request.foregroundSelectorTableId,
      foregroundSelector: request.foregroundSelector,
      foregroundSelectorCandidates: request.foregroundSelectorCandidates,
      foregroundStatus: request.foregroundStatus,
      nativeSceneProps: modeTwoStageProps(catalog, request.foregroundSelector)
    });
  }

  function documentModeTwoBackground(document, catalog) {
    var projection = document && document.background && document.background.projection || {};
    var context = projection.launchContext;
    if (!context || context.override !== true || context.mode !== 2) return null;

    var environmentTableId = 'background-table:mode2-environment:80';
    var foregroundTableId = 'background-table:mode2-overlay:80';
    var environmentSelector = Number.isInteger(context.environmentSelector)
      ? context.environmentSelector : null;
    var foregroundSelector = Number.isInteger(context.foregroundSelector)
      ? context.foregroundSelector : null;
    var environmentEntry = catalog && environmentSelector !== null
      ? catalog.getBackgroundSelectorEntry(environmentTableId, environmentSelector) : null;
    var foregroundEntry = catalog && foregroundSelector !== null
      ? catalog.getBackgroundSelectorEntry(foregroundTableId, foregroundSelector) : null;
    var nativeSceneProps = modeTwoStageProps(catalog, foregroundSelector);
    var environmentStage = stagedBackground(
      environmentEntry && environmentEntry.stageLayers || [], document, {
        sourceKind: 'document-launch-context',
        selectorTableId: environmentTableId,
        selector: environmentSelector,
        environmentSelector: environmentSelector,
        foregroundSelectorTableId: foregroundTableId,
        foregroundSelector: foregroundSelector,
        nativeSceneProps: nativeSceneProps
      });
    var layers = environmentStage ? environmentStage.layers : [];
    var foregroundLayers = backgroundLayers(
      foregroundEntry, M.capabilities.PREVIEW_ONLY, catalog).map(function(layer, index) {
        layer.id = 'background:foreground:' + index;
        layer.role = 'foreground-mask';
        layer.depth = 100 + index;
        layer.nativeOrdinal = index;
        layer.source.sourceKind = 'document-launch-context-foreground';
        layer.source.role = 'foreground-mask';
        return layer;
      });
    layers = layers.concat(foregroundLayers);

    var issues = [];
    if (!environmentEntry || !layers.some(function(layer) {
      return layer.role !== 'foreground-mask';
    })) {
      issues.push(environmentSelector === null
        ? 'Mode-two launch environment selector is unresolved.'
        : 'Mode-two launch environment selector ' + environmentSelector +
          ' has no complete renderable Stage.');
    }
    if (foregroundSelector === null || !foregroundEntry) {
      issues.push('Mode-two launch foreground selector is unresolved.');
    }
    var runtimeProjection = M.cloneJson(projection, 'background.projection');
    runtimeProjection.mode = layers.length || hasRenderableStageProps(nativeSceneProps)
      ? 'stage-fit' : 'unresolved';
    runtimeProjection.evidenceStatus = 'user-supplied-launch-context';
    if (nativeSceneProps) {
      runtimeProjection.nativeSceneProps = M.cloneJson(nativeSceneProps,
        'document mode-two native Stage placements');
    }
    return {
      background: {
        assetId: environmentStage && environmentStage.assetId || null,
        layers: layers,
        capability: M.capabilities.PREVIEW_ONLY,
        projection: runtimeProjection,
        runtimeStatus: 'Document launch context selects mode-two environment ' +
          (environmentSelector === null ? 'unresolved' : environmentSelector) +
          ' and foreground ' +
          (foregroundSelector === null ? 'unresolved' : foregroundSelector) +
          '. Director bytes are unchanged.',
        selectorTableId: environmentTableId,
        selector: environmentSelector,
        environmentSelector: environmentSelector,
        foregroundSelectorTableId: foregroundTableId,
        foregroundSelector: foregroundSelector
      },
      issues: issues
    };
  }

  function documentRows(document) {
    var rowsByNode = {};
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        var nodeId = clip.source && clip.source.nodeId;
        if (!nodeId) return;
        if (!rowsByNode[nodeId]) rowsByNode[nodeId] = [];
        rowsByNode[nodeId].push({ track: track, clip: clip });
      });
    });
    return rowsByNode;
  }

  // Both entry points execute the same iterator. Only the UI driver yields to
  // the event loop; the synchronous driver remains useful for offline audits.
  function compile(document, program, scene, catalog, options) {
    // Legacy offline callers explicitly receive diagnostic, assumed execution.
    // Browser preparation uses compileAsync, whose default stops at missing input.
    var iterator = compileSteps(document, program, scene, catalog,
      Object.assign({ diagnosticAssumptions: true }, options || {}));
    var result;
    var response;
    do {
      result = iterator.next(response);
      response = undefined;
      if (!result.done && result.value && result.value.kind === 'framebuffer-capture') {
        try {
          response = options && options.captureFrame ? options.captureFrame(result.value) : null;
          if (response && typeof response.then === 'function') {
            Promise.resolve(response).catch(function() {});
            response = { error: 'Synchronous playback requires a synchronous render-target service.' };
          }
        } catch (error) { response = { error: error.message }; }
      }
    } while (!result.done);
    return result.value;
  }

  async function compileAsync(document, program, scene, catalog, options) {
    options = options || {};
    var response;
    var iterator = compileSteps(document, program, scene, catalog, options);
    function captureTarget(request) {
      var work = Promise.resolve().then(function() {
        return options.captureFrame ? options.captureFrame(request) : null;
      });
      var signal = options.signal;
      if (!signal || typeof signal.addEventListener !== 'function') return work;
      return new Promise(function(resolve, reject) {
        function abort() {
          signal.removeEventListener('abort', abort);
          var error = new Error('Cutscene preparation cancelled.');
          error.name = 'AbortError';
          reject(error);
        }
        signal.addEventListener('abort', abort, { once: true });
        if (signal.aborted) abort();
        work.then(function(value) {
          signal.removeEventListener('abort', abort);
          resolve(value);
        }, function(error) {
          signal.removeEventListener('abort', abort);
          reject(error);
        });
      });
    }
    function cancelled() {
      if (options.signal && options.signal.aborted) {
        iterator.return();
        var error = new Error('Cutscene preparation cancelled.');
        error.name = 'AbortError';
        throw error;
      }
    }
    for (;;) {
      await new Promise(function(resolve) { setTimeout(resolve, 0); });
      cancelled();
      var start = Date.now();
      var result;
      do {
        cancelled();
        result = iterator.next(response);response=undefined;
        if(!result.done&&result.value&&result.value.kind==='framebuffer-capture'){
          try{response=await captureTarget(result.value);}catch(e){response={error:e.message};}
          cancelled();
        }
        if (result.done) return result.value;
      } while (Date.now() - start < 8);
    }
  }

  function* compileSteps(document, program, scene, catalog, options) {
    options = options || {};
    M.validateSceneDocument(document);
    if (!program || !Array.isArray(program.primitives) ||
        !Array.isArray(program.composites)) {
      fail('A corrected Director program is required.', 'invalid-program');
    }
    if (!scene || scene.engine !== 'director') {
      fail('The execution runtime accepts Director scenes only.', 'invalid-scene');
    }

    var maxTicks = Number.isInteger(options.maxTicks) && options.maxTicks > 0
      ? Math.min(options.maxTicks, DEFAULT_MAX_TICKS) : DEFAULT_MAX_TICKS;
    var maxStateBytes = 128 * 1024 * 1024;
    var maxTraceEntries = 12000;
    var maxDispatches = 100000;
    var retainedStateBytes = 0;
    var recordSchemas=new Map(),recordValueArrays=new WeakSet();
    function compactRecords(rows){return rows.map(function(row){
      var keys=Object.keys(row),signature=JSON.stringify(keys),schema=recordSchemas.get(signature);
      if(!schema){
        var prototype={toJSON:function(){var output={};keys.forEach((key,i)=>{output[key]=this.values[i];});return output;}};
        keys.forEach(function(key,i){Object.defineProperty(prototype,key,{get:function(){return this.values[i];}});});
        schema={keys:keys,prototype:Object.freeze(prototype)};recordSchemas.set(signature,schema);
        retainedStateBytes+=128+keys.reduce((n,k)=>n+96+k.length*2,0);
      }
      var record=Object.create(schema.prototype);record.values=schema.keys.map(k=>row[k]);recordValueArrays.add(record.values);retainedRecordFields.set(record,schema.keys);return record;
    });}
    // Immutable snapshots share unchanged subtrees. Count newly retained nodes,
    // keys and UTF-16 payloads conservatively; this is a storage estimate, not VM heap telemetry.
    function shareSnapshot(previous, next, budget) {
      if (previous === next) return previous;
      if (next === null || typeof next !== 'object') {
        budget.bytes += typeof next === 'string' ? next.length * 2 + 16 : 16;
        return next;
      }
      var keys = Object.keys(next);
      var before = budget.bytes;
      var same = previous && typeof previous === 'object' &&
        Object.getPrototypeOf(previous) === Object.getPrototypeOf(next) && Object.keys(previous).length === keys.length;
      keys.forEach(function(key) {
        next[key] = shareSnapshot(previous && previous[key], next[key], budget);
        if (!previous || next[key] !== previous[key]) same = false;
      });
      if (same) { budget.bytes = before; return previous; }
      budget.bytes += recordValueArrays.has(next) ? 64+next.length*16 : 64 + keys.reduce(function(sum, key) { return sum + 24 + key.length * 2; }, 0);
      return next;
    }
    var dispatchCount = 0;
    var traceCount = 0;
    var stopReason = null;
    var unresolvedQuery = null;
    var unsupportedCommands = [];
    var rowsByNode = documentRows(document);
    var actorTemplateBySlot = {};
    document.actors.forEach(function(actor) { actorTemplateBySlot[actor.slot] = actor; });
    var assumptions = [];
    var missingInputs = [];
    var trace = [];
    function recordTrace(entry) {
      traceCount++;
      if (trace.length < maxTraceEntries) {
        entry.streamAssetId = activeStreamAssetId;
        trace.push(entry);
      }
    }
    var states = [];
    var transformResourceCache = {};
    var rootProgram = program;
    var activeProgram = rootProgram;
    var activeStreamAssetId = scene.assetId;
    var programsByAssetId = {};
    programsByAssetId[scene.assetId] = rootProgram;
    var compositeIndexById = {};
    var directorLabelByMarker = {};
    var primitiveIndexById = {};
    var continuationProgramCache = {};

    function indexActiveProgram() {
      compositeIndexById = {};
      directorLabelByMarker = {};
      primitiveIndexById = {};
      activeProgram.composites.forEach(function(composite, index) {
        compositeIndexById[composite.id] = index;
      });
      activeProgram.primitives.forEach(function(node, index) {
        primitiveIndexById[node.id] = index;
        if (node.name !== 'director_label_marker' || !node.operands.length) return;
        var marker = node.operands[0].signed;
        if (directorLabelByMarker[marker]) return;
        var compositeId = activeProgram.compositeByNodeId[node.id];
        if (!Number.isInteger(compositeIndexById[compositeId])) return;
        directorLabelByMarker[marker] = {
          marker: marker,
          nodeId: node.id,
          startWord: node.startWord,
          primitiveIndex: index,
          compositeIndex: compositeIndexById[compositeId]
        };
      });
    }

    function continuationProgram(selector) {
      if (continuationProgramCache[selector]) return continuationProgramCache[selector];
      var entry = catalog && catalog.getDirectorContinuationStream
        ? catalog.getDirectorContinuationStream(selector) : null;
      if (!entry) return null;
      if (!(options.z64 instanceof Uint8Array) || !OB64.art ||
          typeof OB64.art.readCompressedResource !== 'function' ||
          !OB64.cutsceneCodec || typeof OB64.cutsceneCodec.createIr !== 'function') {
        return null;
      }
      var decoded = OB64.art.readCompressedResource(
        options.z64, entry.resourceKey).decoded;
      if (decoded.length !== entry.decodedLength) {
        fail('Director continuation ' + selector +
          ' no longer matches its generated decoded length.', 'continuation-source');
      }
      var continuationScene = {
        assetId: entry.assetId,
        source: {
          dynamicGrammar: true,
          terminalWithoutTrailer: true,
          decodedLength: entry.decodedLength,
          decodedWordCount: entry.decodedWordCount,
          runtimeNodeCount: entry.runtimeNodeCount
        }
      };
      var ir = OB64.cutsceneCodec.createIr(continuationScene, decoded);
      continuationProgramCache[selector] = ir.program;
      programsByAssetId[entry.assetId] = ir.program;
      return ir.program;
    }

    indexActiveProgram();

    function assumption(text) { uniquePush(assumptions, text); }
    function missing(text) { uniquePush(missingInputs, text); }
    function actorBoundary(text, code) {
      missing(text);
      if (options.diagnosticAssumptions !== true) stopReason = code || 'actor-input';
    }
    function rowsFor(node) { return rowsByNode[node.id] || []; }
    function rowFor(node, kind) {
      return rowsFor(node).find(function(row) { return !kind || row.clip.kind === kind; }) || null;
    }

    function transformResource(resourceIndex) {
      if (transformResourceCache[resourceIndex]) return transformResourceCache[resourceIndex];
      if (!(options.z64 instanceof Uint8Array)) return null;
      var decoded = decodeSceneTransformResource(options.z64, resourceIndex);
      transformResourceCache[resourceIndex] = decoded;
      return decoded;
    }

    var launchProfile = scene.launchProfile;
    var translationProfile = launchProfile.operandTranslation || {
      required: false, tableIndexes: []
    };
    var suppliedTranslationTable = options.launchOperandTranslations || (options.nativeLaunchInputs && options.nativeLaunchInputs.externalProducers && options.nativeLaunchInputs.externalProducers.value && options.nativeLaunchInputs.externalProducers.value.directorLaunch && options.nativeLaunchInputs.externalProducers.value.directorLaunch.operandTranslations) || {};
    var suppliedTranslationIndexes = [];
    var missingTranslationIndexes = [];
    (translationProfile.tableIndexes || []).forEach(function(tableIndex) {
      var supplied = Object.prototype.hasOwnProperty.call(
        suppliedTranslationTable, String(tableIndex));
      if (!supplied) {
        missingTranslationIndexes.push(tableIndex);
        return;
      }
      var value = suppliedTranslationTable[tableIndex];
      if (!Number.isInteger(value) || value < 0 || value > 0xFFFF) {
        fail('Launch operand translation ' + tableIndex +
          ' must be an unsigned halfword.', 'launch-translation');
      }
      suppliedTranslationIndexes.push(tableIndex);
    });
    if (missingTranslationIndexes.length) {
      missing('The native Director loader requires launch operand translation table ' +
        (missingTranslationIndexes.length === 1 ? 'index ' : 'indexes ') +
        missingTranslationIndexes.join(', ') +
        '; commands using unresolved values remain byte-preserved and are withheld from preview state.');
    }

    function executionWords(node) {
      var words = node.rawWords.map(unsigned);
      var unresolvedWordOffsets = [];
      var translatedWordOffsets = [];
      words.forEach(function(word, wordOffset) {
        var tableIndex = launchTranslationIndex(word);
        if (tableIndex === null) return;
        if (Object.prototype.hasOwnProperty.call(
            suppliedTranslationTable, String(tableIndex))) {
          words[wordOffset] = suppliedTranslationTable[tableIndex];
          translatedWordOffsets.push(wordOffset);
        } else {
          unresolvedWordOffsets.push(wordOffset);
        }
      });
      return {
        words: words,
        unresolvedWordOffsets: unresolvedWordOffsets,
        translatedWordOffsets: translatedWordOffsets
      };
    }
    var romOnlyStart=options.nativeLaunchInputs&&options.nativeLaunchInputs.externalProducers&&options.nativeLaunchInputs.externalProducers.value&&options.nativeLaunchInputs.externalProducers.value.directorLaunch&&options.nativeLaunchInputs.externalProducers.value.directorLaunch.kind==='rom-mode-two-director-v1';
    var observedBackground = romOnlyStart?null:observationBackground(scene, document, catalog);
    var documentBackground = documentModeTwoBackground(document, catalog);
    var directorMode = romOnlyStart?{value:2,status:'ROM terminal-class dispatcher selects mode two',evidenceStatus:'ROM-caller-rule'}:directorModeFromProfile(scene);
    var modeTwoCommandPreviewUsesFreshRoot = directorMode.value === 2 &&
      launchProfile.background && launchProfile.background.requestCount > 0;
    var suppliedContextRuntime = options.contextRuntime && (
      Array.isArray(options.contextRuntime.states) && options.contextRuntime.states.length ||
      Array.isArray(options.contextRuntime.contextFrames) &&
        options.contextRuntime.contextFrames.length)
      ? options.contextRuntime : null;
    var hasCapturedSnapshot=options.nativeLaunchInputs && options.nativeLaunchInputs.capturedSnapshot && options.nativeLaunchInputs.capturedSnapshot.status==='known';
    var contextRuntime = modeTwoCommandPreviewUsesFreshRoot || hasCapturedSnapshot
      ? null : suppliedContextRuntime;
    if (modeTwoCommandPreviewUsesFreshRoot && suppliedContextRuntime && !hasCapturedSnapshot) {
      assumption('The event route preserves resource-loader mode 0x8023A981, but its launch value is not statically known; this explicit mode-two background preview uses the native zero-mode fresh-root branch.');
    }
    var contextFrameCount = !contextRuntime ? 0 :
      (Array.isArray(contextRuntime.states)
        ? contextRuntime.states.length : contextRuntime.contextFrames.length);
    var contextTickOffset = Number.isInteger(options.contextTickOffset)
      ? options.contextTickOffset : 1;
    var launchInvocationContexts = (launchProfile.parentEventLaunches || [])
      .reduce(function(rows, launch) {
        return rows.concat(launch.eventInvocationContexts || []);
      }, []);
    var selectedLaunchContext = options.launchContext || null;
    var selectedContextOwner = selectedLaunchContext &&
      selectedLaunchContext.concurrentDirectorAssetId || null;
    var everyLaunchNeedsContext = !modeTwoCommandPreviewUsesFreshRoot &&
      launchInvocationContexts.length > 0 &&
      launchInvocationContexts.every(function(context) {
        return !!context.concurrentDirectorAssetId;
      });
    if (!contextRuntime && !modeTwoCommandPreviewUsesFreshRoot &&
        (selectedContextOwner || everyLaunchNeedsContext)) {
      var contextOwners = Array.from(new Set(launchInvocationContexts.map(function(context) {
        return context.concurrentDirectorAssetId;
      }).filter(Boolean)));
      missing('The selected parent-event invocation requires concurrent Director scene state' +
        (contextOwners.length ? ' from ' + contextOwners.join(' or ') : '') +
        '; a standalone stream cannot supply its shared Actors, dialogue, or transforms.');
    }
    var priorContextState = null;
    var priorContextFrameIndex = -1;
    // At most 28 slot records; imported activity must survive an empty Actor pointer.
    var vacantMovementActivity = {};
    var registeredCamera = cameraFromLaunchProfile(
      launchProfile.cameras.registered, 'registered');
    var actorCamera = cameraFromLaunchProfile(launchProfile.cameras.actor, 'actor');
    var initialStageTransform = M.cloneJson(
      launchProfile.stageTransform.initial, 'launch Stage transform');
    var directorSelectorRows = scene && scene.source &&
      Array.isArray(scene.source.directorSelectorRows)
      ? scene.source.directorSelectorRows.filter(Number.isInteger) : [];
    var screenTransitionVariant = Number.isInteger(options.directorSelector)
      ? options.directorSelector & 0x3FFF
      : (directorSelectorRows.length && directorSelectorRows.every(function(selector) {
        return selector === 0;
      }) ? 0 : (directorSelectorRows.length && directorSelectorRows.every(function(selector) {
        return selector !== 0;
      }) ? directorSelectorRows[0] : null));
    if (directorMode.evidenceStatus === 'external-unresolved') {
      missing('The launch profile cannot identify this stream\'s Director mode.');
    }
    if (!romOnlyStart && launchProfile.cameras.actor.evidenceStatus === 'external-unresolved') {
      missing('The launch profile does not contain this scene\'s initial Actor camera.');
    }
    var launchInputs = validateLaunchInputs(options.nativeLaunchInputs, scene.assetId);
    function launchValue(key) {
      var group = launchInputs && launchInputs[key];
      return group && group.status === 'known' ? group.value : null;
    }
    var actorInputRows = launchValue('actorInputRows');
    var currentUnitMembers = launchValue('currentUnitMembers');
    var externalProducers = launchValue('externalProducers');
    var capturedServices=launchValue('capturedResume') && launchValue('capturedResume').entry==='normal-mode-two-continuous' && launchValue('capturedResume').resourceServices;
    if(capturedServices){if(!capturedServices.resourceSchedule||capturedServices.resourceSchedule.directorCallback!==0x80226190||capturedServices.resourceSchedule.directorSlot!==0)fail('Captured mode-two services require the saved Director binding.','resume-resource-binding');if(externalProducers)fail('Captured services cannot combine an external timeline.','resume-context-input');externalProducers={throughTick:capturedServices.resourceSchedule.controller.throughPass,initialDialogue:capturedServices.initialDialogue,resourceSchedule:capturedServices.resourceSchedule,initialColor:null,events:[],menuCreates:[],colorCreates:[],poseCalls:[]};}

    var externalOccurrences = {}, externalEventCursor = 0, sharedPoseCallCursor = 0;
    var knownTransientSlots = new Set(), allTransientSlotsKnown = !!(externalProducers && externalProducers.initialMenusEmpty);
    var actorServiceOccurrences = {}, qualifiedPoseCache = new Map(), subordinateSerial = 0, nativeRosterResult = null;
    var state = {
      tick: 0,
      actors: {},
      movementJobs: {},
      turnJobs: {},
      tintJobs: {},
      bodyPoseJobs: {},
      yJobs: {},
      projectionJob: null,
      screenTransition: null,
      actorPresentationJob: null,
      overlayJob: null,
      sceneColorJob: null,
      sceneTransformJob: null,
      oversizedImageTransitionJob: null,
      sceneVignette: null,
      oversizedImageView: {
        x: 0,
        y: 0,
        scale: 1,
        zoomState: 4
      },
      titleJob: null,
      transientRenderEntities: {},
      armyManagementCursorLatch: 0,
      registeredCounter: null,
      transformDivider: null,
      transformChannels: identityTransformChannels(),
      directorMode: directorMode.value,
      directorModeStatus: directorMode.status,
      cameras: { registered: registeredCamera, actor: actorCamera },
      projectionTransform: initialStageTransform,
      background: documentBackground ? documentBackground.background :
        (observedBackground || M.cloneJson(document.background, 'background')),
      dialogues: {},
      spriteEffects: {},
      audioEvents: [],
      cameraEvents: [],
      effectEvents: [],
      flowEvents: [],
      scheduled: [],
      terminal: false,
      terminalReason: null,
      presentationLifecycleRequest: 0,
      alternateDirectorScheduling: launchValue('schedulerBranch') === 'alternate',
      textSpeed: 512,
      sceneColor: { red: 255, green: 255, blue: 255 },
      overlay: null,
      executedNodeIds: []
    };

    state.sharedRequests = externalProducers && externalProducers.initialRequests
      ? Object.assign({}, externalProducers.initialRequests) : { A:null, B:null };
    if (externalProducers && externalProducers.initialColor) {
      var initialColor = launchBytes(externalProducers.initialColor.recordHex,12);
      state.overlay = { ownerId:externalProducers.initialColor.ownerId,
        remaining:initialColor.getInt16(0),duration:initialColor.getInt16(2),
        red:initialColor.getUint8(4),green:initialColor.getUint8(5),blue:initialColor.getUint8(6),
        ownershipFlag:initialColor.getUint8(7),alpha:initialColor.getUint8(8),
        targetAlpha:initialColor.getUint8(9),startAlpha:initialColor.getUint8(10),native:true };
      state.overlayJob = state.overlay;
    }

    function producerBoundary(message, code) {
      missing(message);
      if (options.diagnosticAssumptions === true) return false;
      stopReason = 'external-input';
      unresolvedQuery = {kind:'producer',code:code,label:message,streamAssetId:activeStreamAssetId};
      return true;
    }
    function externalCreation(kind, node) {
      var key=kind+':'+node.id, occurrence=externalOccurrences[key] || 0;
      externalOccurrences[key]=occurrence+1;
      return externalProducers && externalProducers[kind].find(function(row) {
        return row.nodeId===node.id && row.occurrence===occurrence;
      });
    }
    var dialogueEngine = null, resourceScheduler = null, nativeLaunch = null, launchInitialization = null, launchParserRan = false;
    var framebufferProfile=externalProducers&&externalProducers.framebuffer,iris=null,pendingIris=null,framebuffers=[],framebufferBytes=0,framebufferLayerCount=0;
    var echoProfile=externalProducers&&externalProducers.imageEcho,imageEcho=null;
    var menuProfile=externalProducers&&externalProducers.mapMenu,mapMenu=null;
    var sharedActorProfile=externalProducers&&externalProducers.sharedActor,sharedActor=null;
    var nativeActorDrawing=null;
    if(sharedActorProfile&&(!echoProfile||!externalProducers.directorLaunch||!OB64.cutsceneSharedActor||externalProducers.poseCalls.length||!externalProducers.initialRequests||!['A','B'].every(k=>Number.isInteger(externalProducers.initialRequests[k]))))fail('Computed Actor projection requires shared matrices, fresh launch, initial request slots, and no recorded pose calls.','shared-actor-input');
    if(menuProfile){
      var menuEvents=Array.isArray(externalProducers.events)?externalProducers.events:externalProducers.events.templates;
      if(!echoProfile||!externalProducers.directorLaunch||!OB64.cutsceneMapMenu||
          menuProfile.kind!=='native-map-menu-v1'||menuProfile.initialEntitiesEmpty!==true||menuProfile.displayMode!==0||
          menuProfile.controllerSource!=='declared-action-mask'||externalProducers.menuCreates.length||menuEvents.some(e=>e.kind==='menu')) {
        fail('Computed map menus require fresh launch, empty entity ownership, declared action masks, and no recorded menu outcomes.','map-menu-input');
      }
    }
    if(echoProfile&&(!framebufferProfile||!OB64.cutsceneImageEcho||echoProfile.kind!=='native-image-echo-v1'))fail('Image echo requires its native profile and framebuffer lifecycle.','image-echo-input');
    if(framebufferProfile&&framebufferProfile.backgroundPolicy!==undefined&&!['omit','require'].includes(framebufferProfile.backgroundPolicy))fail('Framebuffer background policy must be explicit.','framebuffer-input');
    if(framebufferProfile&&(!OB64.cutsceneFramebuffer||framebufferProfile.kind!=='product-framebuffer-v1'||framebufferProfile.capturePolicy!=='constructor-current-state'))fail('Framebuffer playback requires its explicit product capture policy.','framebuffer-input');
    if(framebufferProfile){if(!externalProducers.directorLaunch)fail('Framebuffer playback requires fresh Director launch context.','framebuffer-input');OB64.cutsceneFramebuffer.validateRom(options.z64);}
    if(externalProducers && externalProducers.initialDialogue) {
      try {
        if(!OB64.cutsceneDialogue)fail('Native dialogue support is unavailable.','dialogue-module');
        dialogueEngine=new OB64.cutsceneDialogue.Engine(externalProducers.initialDialogue,options.z64);
        if(capturedServices)OB64.cutsceneDirectorLaunch.installAudioQueue(dialogueEngine,options.z64,function(event){state.audioEvents.push(event);});
        if(dialogueEngine.lifecycle&&(externalProducers.dialogueCreates||[]).length)fail('Shared dialogue construction must omit recorded constructor outcomes.','dialogue-constructor-input');
        if(externalProducers.resourceSchedule!==undefined){
          if(!OB64.cutsceneResourceScheduler)fail('Resource scheduling support is unavailable.','dialogue-scheduler-input');
          for(var ei=0;ei<externalEventCount(externalProducers.events);ei++)if(externalEventAt(externalProducers.events,ei).kind==='dialogue')fail('Computed scheduling must omit recorded dialogue service events.','dialogue-scheduler-input');
          resourceScheduler=new OB64.cutsceneResourceScheduler.Scheduler(dialogueEngine,externalProducers.resourceSchedule,options.z64);
          if(externalProducers.directorLaunch!==undefined){
            if(!OB64.cutsceneDirectorLaunch||launchValue('capturedSnapshot')||launchValue('existingActors')||contextRuntime)fail('Fresh Director launch cannot inherit a captured or concurrent Actor namespace.','director-launch-input');
            nativeLaunch=new OB64.cutsceneDirectorLaunch(externalProducers.directorLaunch,options.z64);
            var launchBinding=resourceScheduler.read(resourceScheduler.input.directorSlot);
            if((launchBinding.flags&0xa000)!==0x8000||launchBinding.initialize!==(nativeLaunch.input.sceneMode===2?0x802260f0:0x80225a1c))fail('Fresh launch requires an active, uninitialized Director resource.','director-launch-binding');
            if(externalEventCount(externalProducers.events)||(externalProducers.colorCreates||[]).length)fail('Fresh launch must omit recorded resource and color events.','director-launch-input');
            if(nativeLaunch.resourceKey!==parseInt(scene.directorKey,16))fail('Director selector does not resolve to the selected ROM stream.','director-launch-selector');
            resourceScheduler.initializeDirector=initializeDirectorResource;
            nativeLaunch.attachResources(dialogueEngine);resourceScheduler.colorService=serviceColorResource;resourceScheduler.beforeDirector=restoreDirectorResource;resourceScheduler.afterDirector=saveDirectorResource;
            nativeLaunch.audioRequest=function(event){state.audioEvents.push(event);};
          }
        }
        dialogueEngine.owners.forEach(function(owner,slot){
          if(!owner)return;
          var record=0x800e82c8+slot*0xa8;
          if(dialogueEngine.machine.get(record+0x10)!==0x80198be8)return;
          var id=dialogueEngine.machine.get(record+0x8f,1);
          state.dialogues[id]={windowId:id,nativeSlot:slot,nativeOwnerId:owner.ownerId,segments:[''],segmentIndex:0,
            readyTick:Number.MAX_SAFE_INTEGER,closed:false,paused:false,archive:null,entry:null,layout:{},
            speaker:'Dialogue',sourceNodeId:'native-initial:'+owner.ownerId};
        });
      } catch(error) { producerBoundary(error.message,error.code||'dialogue-native-input'); }
    }
    function* applyExternalServices(phase) {
      if (!externalProducers) return;
      var events=externalProducers.events;
      while (externalEventCursor<externalEventCount(events)) {
        var event=externalEventAt(events,externalEventCursor);
        if (event.tick!==state.tick || event.phase!==phase) break;
        externalEventCursor++;
        try {
          if(event.kind==='dialogue') {
            if(!dialogueEngine)fail('Dialogue service requires initial native memory and ownership.','dialogue-initial-input');
            yield* dialogueEngine.service(event);
          } else if (event.kind==='menu' && event.eligible) {
            var menu=state.transientRenderEntities[event.slot];
            if (!menu || !menu.native || menu.ownerId!==event.ownerId) fail('Menu service targets a missing or replaced owner.', 'external-producer-owner');
            advanceNativeMenu(menu,event);
          } else if (event.kind==='color' && event.eligible) {
            if (!state.overlay || !state.overlay.native || state.overlay.ownerId!==event.ownerId) fail('Color service targets a missing or replaced owner.', 'external-producer-owner');
            advanceNativeColor(state.overlay,true);
          } else if (event.kind==='request-reset') state.sharedRequests[event.context]=-1;
          else if (event.kind==='request-dispatch') {
            var request=state.sharedRequests[event.context];
            if (request===null) fail('Request dispatch requires its current scalar slot.', 'shared-request-initial-state');
            if (request>=0) {
              if (event.context==='B') state.audioEvents.push({kind:'native-shared-request',context:'B',mode:2,category:6,program:0,priority:16384,flags:2});
              state.audioEvents.push({kind:'native-shared-request',context:event.context,mode:1,category:event.context==='A'?5:6,program:request&65535,priority:16384,flags:event.context==='A'?0:2});
            }
          }
          recordTrace({tick:state.tick,kind:'external-producer-service',service:externalEventCursor-1,phase:phase,producer:event.kind,eligible:event.eligible});
        } catch(error) {
          if (!(error instanceof RuntimeError) && !String(error.code||'').startsWith('dialogue-')) throw error;
          producerBoundary(error.message,error.code);return;
        }
      }
    }
    function checkMenuMemory(){
      var size=[mapMenu,imageEcho,nativeLaunch,dialogueEngine,capturedScheduler].filter(Boolean).reduce(function(n,service){
        return n+service.machine.regions.reduce((v,r)=>v+r.bytes.length,0);
      },0);
      if(size>131072)fail('Combined native services exceed 128 KiB: '+size+'.','map-menu-memory-bound');
      return size;
    }
    function advanceMapMenu(){
      if(!mapMenu)return;
      try{
        checkMenuMemory();
        state.audioEvents.push.apply(state.audioEvents,mapMenu.advance(currentControllerMask(),resourceScheduler&&resourceScheduler.control?resourceScheduler.control.directionMask:0));
        Object.keys(state.transientRenderEntities).forEach(function(slot){
          var entity=state.transientRenderEntities[slot];
          if(entity.computedMenu){entity.status=mapMenu.query(Number(slot));entity.detached=entity.status===-6;}
        });
      }catch(error){producerBoundary(error.message,error.code);}
    }
    function currentControllerMask(){return resourceScheduler&&resourceScheduler.control?resourceScheduler.control.actionMask:options.controllerMask;}
    function* applyResourcePass(phase){
      if(!resourceScheduler)return;
      try{
        checkMenuMemory();
        if(phase==='before')yield* resourceScheduler.before(state.tick);
        else{yield* resourceScheduler.after(state.terminal);recordTrace({tick:state.tick,kind:'resource-pass',clock:'declared-resource-pass',actions:resourceScheduler.trace.slice()});}
      }catch(error){producerBoundary(error.message,error.code||'dialogue-scheduler-input');}
    }

    function registerSharedPoseRequest(actor, record) {
      if (capturedResume && !continuousResume) return 'resume-shared-pose-input';
      var input=null;
      if (record.opcode===18 || record.opcode===20) {
        if(sharedActorProfile&&actor.decoderMode===0){
          try{ensureSharedActor();var nativeRecord=nativeMatrixRecordForActor(actor);if(!nativeRecord)return 'shared-actor-record';input=sharedActor.project(new Uint8Array(nativeRecord.buffer),state.cameras);recordTrace({tick:state.tick,kind:'shared-actor-projection',actorId:actor.id,output:input.output,inputX:input.inputX});}
          catch(error){producerBoundary(error.message,error.code);return error.code||'shared-actor-projection';}
        }else{
        input=externalProducers && externalProducers.poseCalls[sharedPoseCallCursor];
        if (!input || input.actorId!==actor.id || input.bank!==actor.bank || input.stateIndex!==actor.poseStateIndex ||
            input.recordOrdinal!==actor.poseCursor || input.opcode!==record.opcode) return 'shared-pose-control-'+record.opcode;
        }
      }
      var selected=selectNativeSharedRequest(record.opcode,record.operands,actor.decoderMode,input,function(offset) {
        if (!(options.z64 instanceof Uint8Array) || offset+2>options.z64.length) return null;
        return options.z64[offset]*256+options.z64[offset+1];
      });
      if (selected.boundary) return selected.boundary;
      if (input && !sharedActor) sharedPoseCallCursor++;
      if (!selected.suppressed) {
        state.sharedRequests[selected.context]=selected.request;
        recordTrace({tick:state.tick,kind:'shared-pose-request',actorId:actor.id,recordOrdinal:actor.poseCursor,
          opcode:record.opcode,context:selected.context,request:selected.request,tableOffsetZ64:selected.tableOffset});
      }
      return null;
    }

    function ensureSharedActor(){
      if(sharedActor)return;
      if(state.directorMode!==0||state.alternateDirectorScheduling)fail('Computed ordinary Actor projection requires the declared normal mode-zero service path.','shared-actor-mode');
      if(!imageEcho)imageEcho=new OB64.cutsceneImageEcho(options.z64);
      sharedActor=new OB64.cutsceneSharedActor(imageEcho,sharedActorProfile,options.z64);
      checkMenuMemory();
      if(!nativeLaunch.input.audioQueueHex)fail('Shared request dispatch requires the current native audio request queue.','shared-actor-audio');
      OB64.cutsceneSharedActorCode.words.forEach(function(r){if(r[0]>=0x800ea604&&r[0]<0x800eac24)dialogueEngine.machine.code[r[0]]=r[2];});
    }
    function* prepareSharedActors(){
      if(!sharedActorProfile||state.terminal)return;
      try{
        ensureSharedActor();
        var drawingActors=[];
        for(var slot of Object.keys(state.actors)){
          var actor=state.actors[slot],record=nativeMatrixRecordForActor(actor);if(!record)fail('Matrix preparation requires a qualified ordinary Actor construction.','shared-actor-record');
          var prepared=sharedActor.prepare([{slot:Number(slot),bytes:new Uint8Array(record.buffer)}],state.cameras,state.transformChannels)[0].bytes;
          actor.matrixRecordBase=recordHex(new DataView(prepared.buffer));
          drawingActors.push({slot:Number(slot),key:OB64.cutsceneSharedActor.actorDrawingKey(actor,state.transformChannels[actor.transformChannel]),matrixHex:actor.matrixRecordBase.slice(320,448)});yield;
        }
        nativeActorDrawing={camera:sharedActor.drawingCamera(state.cameras,nativeLaunch.machine.get(0x8022a730)),cameraKey:OB64.cutsceneSharedActor.cameraDrawingKey(projectionFromCamera(state.cameras.actor),projectionFromCamera(state.cameras.registered)),actors:drawingActors};
        for(var context of ['A','B']){
          var request=state.sharedRequests[context];if(request===null)fail('Shared request dispatch requires its initial scalar slots.','shared-request-initial-state');
          if(request>=0){yield* dialogueEngine.machine.run(0x800ea604,[context==='A'?0x800eb240:0x800eb290,request],[],8192);recordTrace({tick:state.tick,kind:'shared-request-dispatch',context:context,request:request,playback:'queue-only'});}
        }
        // The reset follows dispatch; the dispatcher does not clear its slots.
        state.sharedRequests.A=-1;state.sharedRequests.B=-1;
      }catch(error){producerBoundary(error.message,error.code||'shared-actor-input');}
    }
    function nativeMatrixRecordForActor(actor){
      // This scoped record supplies only the matrix producer's read fields.
      // It does not restore the full-record authority invalidated by other commands.
      if(!actor.matrixRecordBase||actor.decoderMode!==0)return null;
      var b=launchBytes(actor.matrixRecordBase,336);
      b.setFloat32(0x11c,actor.x);b.setFloat32(0x120,actor.y);b.setFloat32(0x124,actor.z);
      b.setFloat32(0x128,actor.secondaryY);b.setFloat32(0x12c,actor.yawDegrees);b.setFloat32(0x130,actor.uniformScale);
      b.setUint8(0x13e,actor.transformChannel);b.setUint8(0x13f,actor.nativeFacing);b.setUint8(0x145,actor.heightModeByte);
      return b;
    }

    if (!launchValue('schedulerBranch')) assumption('Preview selects normal Actor update eligibility; no universal video-frame or seconds conversion is proved.');
    var capturedSnapshot = launchValue('capturedSnapshot');
    var capturedResume = launchValue('capturedResume');
    var extendedModeTwoResume=capturedResume && capturedResume.entry==='normal-mode-two-continuous';
    var heldModeTwoResume=capturedResume && ['normal-mode-two-held-movement-window','normal-mode-two-continuous'].includes(capturedResume.entry);
    var continuousResume = capturedResume && (capturedResume.entry === 'normal-director-continuous' || heldModeTwoResume);
    var capturedScheduler=null;
    if (continuousResume && Number.isInteger(options.controllerMask) && options.controllerMask !== 0) {
      fail('Continuous captured resume requires the declared neutral Director controller input.', 'launch-input');
    }
    var completedResumeUpdates = 0;
    var capturedPresentation = launchValue('capturedPresentation');
    var resumedMenuSelection = null;
    var initialActors = launchValue('existingActors') || capturedSnapshot || (nativeLaunch?{slots:new Array(28).fill(null),otherJobsEmpty:true}:null);
    if (initialActors) initialActors.slots.forEach(function(row, slot) {
      if (!row) return;
      var bytes = launchBytes(row.recordHex, 0x150);
      var actor = ensureActor(slot);
      actor.id = row.identity;
      actor.label = 'Existing Actor ' + row.identity;
      actor.source = { launchSourceIdentity: launchInputs.sourceIdentity,
        invocationId: launchInputs.invocationId, evidenceGrade: launchInputs.evidenceGrade,
        recordHex: row.recordHex };
      actor.bank = bytes.getInt32(0xE8);
      actor.animationKey = bytes.getInt16(0x138);
      actor.nativeFacing = bytes.getUint8(0x13F);
      actor.variantSelector = bytes.getUint8(0x146);
      actor.artSourceId = 'cutscene-art-bank:' + actor.bank;
      actor.poseId = poseId(actor.bank, actor.animationKey, actor.nativeFacing);
      actor.facing = 'native-' + actor.nativeFacing;
      actor.x = bytes.getFloat32(0x11C); actor.y = bytes.getFloat32(0x120); actor.z = bytes.getFloat32(0x124);
      actor.poseCursor = bytes.getInt32(0xF0); actor.poseDelay = bytes.getInt32(0xF4);
      actor.displayedFrameToken = bytes.getInt32(0xF8);
      actor.poseStateIndex = bytes.getInt16(0x134);
      actor.decoderMode = bytes.getUint8(0x13D);
      actor.sourceRowOrdinal = bytes.getUint8(0x147);
      actor.nativeRecordBase = row.recordHex;
      if (capturedPresentation) actor.capturedMainScale = bytes.getFloat32(0x104, false);
      actor.nativeOwnerContext = bytes.getInt32(0xEC);
      actor.nativeFlagB = bytes.getInt16(0x13A);
      actor.linkedOrdinal = bytes.getUint8(0x149);
      if (actor.decoderMode !== 0) actor.bodyPoseProgram = {
        decoder:'alternate-body-pose',artSource:actor.bank,ownerContext:actor.nativeOwnerContext,
        flagA:actor.variantSelector,flagB:actor.nativeFlagB,selector:actor.animationKey,
        initialization:'qualified-existing-record'
      };
      actor.material = Array.from({length:16}, function(_,i) { return bytes.getUint8(i); });
      actor.materialDelta = Array.from({length:16}, function(_,i) { return bytes.getUint8(i+16); });
      if (capturedSnapshot) {
        applyNativeRecord(actor, bytes, 'captured-snapshot');
        // Record preservation must not select the alternate sprite consumer for ordinary Actors.
        if (actor.decoderMode === 0) {
          actor.bodyPoseProgram = null;
          actor.artSourceId = 'cutscene-art-bank:' + actor.bank;
          actor.poseId = poseId(actor.bank, actor.animationKey, actor.nativeFacing);
          actor.poseProgramStatus = 'captured-ordinary-token';
        }
      }
      actor.visible = true;
      if (row.movementHex !== null) {
        var movement = launchBytes(row.movementHex, 16);
        state.movementJobs[slot] = { slot:slot, vx:movement.getFloat32(0), vz:movement.getFloat32(8),
          remaining:movement.getUint16(12), pauseByte:movement.getUint8(14), elapsed:0 };
        actor.activeMovementId = 'launch-movement:' + slot;
      }
    });

    if (documentBackground) {
      recordTrace({ tick: 0, kind: 'runtime-input', label: 'Document mode-two launch context' });
      documentBackground.issues.forEach(missing);
    } else if (observedBackground) {
      recordTrace({ tick: 0, kind: 'runtime-input', label: 'Observed background route' });
    }
    if (contextRuntime) {
      recordTrace({
        tick: 0,
        kind: 'runtime-input',
        label: 'Parent-event concurrent Director context',
        contextAssetId: contextRuntime.assetId,
        contextTickOffset: contextTickOffset
      });
    }

    function templateForSlot(slot) { return actorTemplateBySlot[slot] || null; }

    function selectorFromTemplate(template, fallback) {
      fallback = fallback || {};
      var bankMatch = template && String(template.artSourceId || '').match(/cutscene-art-bank:(\d+)/);
      var facingMatch = template && String(template.initial.facing || '').match(/^native-(\d+)$/);
      return {
        bank: bankMatch ? Number(bankMatch[1]) : finite(fallback.bank, null),
        key: template && Number.isInteger(template.source.animationKey)
          ? template.source.animationKey : finite(fallback.key, null),
        facing: facingMatch ? Number(facingMatch[1]) : finite(fallback.facing, null),
        variant: template && Number.isInteger(template.source.variantSelector)
          ? template.source.variantSelector : finite(fallback.variant, 0)
      };
    }

    function qualifiedPoseForActor(actor, kind) {
      var registry = launchValue('poseRegistry');
      if (!registry&&nativeLaunch&&nativeLaunch.input.sceneMode===2)registry={ordinary:[],alternate:nativeLaunch.poseRegistry};
      if (!registry) return null;
      var body = actor.bodyPoseProgram || {};
      var owner = Number.isInteger(body.ownerContext) ? body.ownerContext : actor.nativeOwnerContext;
      var flagB = Number.isInteger(body.flagB) ? body.flagB : actor.nativeFlagB;
      var match = registry[kind].find(function(row) {
        return row.sourceArt === actor.bank && row.flagA === actor.variantSelector &&
          (kind === 'ordinary' || (row.ownerContext === owner && row.flagB === flagB));
      });
      var program = match && match.programs.find(function(row) {return row.state === lowS16(actor.poseStateIndex);});
      if (!program) return null;
      if (!qualifiedPoseCache.has(program.programHex)) qualifiedPoseCache.set(program.programHex,decodeQualifiedPose(program.programHex));
      return qualifiedPoseCache.get(program.programHex);
    }

    function actorService(key, node) {
      var identity = key + ':' + node.id;
      var occurrence = actorServiceOccurrences[identity] || 0;
      actorServiceOccurrences[identity] = occurrence + 1;
      return (launchValue(key) || []).find(function(row) {return row.nodeId === node.id && row.occurrence === occurrence;});
    }

    function nativeRecordForActor(actor) {
      if (actor.nativeRecordUnavailable) return null;
      var hex = actor.nativeRecordBase || actor.source && actor.source.recordHex;
      if (!hex) return null;
      var bytes = launchBytes(hex,0x150), body=actor.bodyPoseProgram || {};
      for (var i=0;i<16;i++) {bytes.setUint8(i,actor.material[i]);bytes.setUint8(i+16,actor.materialDelta[i]);}
      bytes.setInt32(0xE4,actor.slot);bytes.setInt32(0xE8,actor.bank);
      var owner = Number.isInteger(body.ownerContext) ? body.ownerContext : actor.nativeOwnerContext;
      if (Number.isInteger(owner)) bytes.setInt32(0xEC,owner);
      bytes.setInt32(0xF0,actor.poseCursor);bytes.setInt32(0xF4,actor.poseDelay);bytes.setInt32(0xF8,actor.displayedFrameToken);
      bytes.setFloat32(0x11C,actor.x);bytes.setFloat32(0x120,actor.y);bytes.setFloat32(0x124,actor.z);
      if (Number.isInteger(actor.poseStateIndex)) bytes.setInt16(0x134,actor.poseStateIndex);
      if (Number.isInteger(actor.previousPoseStateIndex)) bytes.setInt16(0x136,actor.previousPoseStateIndex);
      bytes.setInt16(0x138,actor.animationKey);
      var flagB=Number.isInteger(body.flagB) ? body.flagB : actor.nativeFlagB;
      if (Number.isInteger(flagB)) bytes.setInt16(0x13A,flagB);
      bytes.setUint8(0x13D,actor.decoderMode);bytes.setUint8(0x13F,actor.nativeFacing);
      bytes.setUint8(0x146,actor.variantSelector);bytes.setUint8(0x147,actor.sourceRowOrdinal);
      if (Number.isInteger(actor.linkedOrdinal)) bytes.setUint8(0x149,actor.linkedOrdinal);
      return bytes;
    }

    function forgetNativeRecord(actor) {
      actor.nativeRecordBase=null;actor.nativeRecordUnavailable=true;
      actor.source=Object.assign({},actor.source);delete actor.source.recordHex;
    }

    function applyNativeRecord(actor, bytes, initialization) {
      actor.nativeRecordBase=recordHex(bytes);
      actor.nativeRecordUnavailable=false;
      actor.bank=bytes.getInt32(0xE8);actor.nativeOwnerContext=bytes.getInt32(0xEC);
      actor.poseCursor=bytes.getInt32(0xF0);actor.poseDelay=bytes.getInt32(0xF4);actor.displayedFrameToken=bytes.getInt32(0xF8);
      actor.poseStateIndex=bytes.getInt16(0x134);actor.previousPoseStateIndex=bytes.getInt16(0x136);
      actor.animationKey=bytes.getInt16(0x138);actor.nativeFlagB=bytes.getInt16(0x13A);
      actor.decoderMode=bytes.getUint8(0x13D);actor.nativeFacing=bytes.getUint8(0x13F);
      actor.variantSelector=bytes.getUint8(0x146);actor.sourceRowOrdinal=bytes.getUint8(0x147);actor.linkedOrdinal=bytes.getUint8(0x149);
      actor.x=bytes.getFloat32(0x11C);actor.y=bytes.getFloat32(0x120);actor.z=bytes.getFloat32(0x124);
      actor.material=Array.from({length:16},function(_,i){return bytes.getUint8(i);});
      actor.materialDelta=Array.from({length:16},function(_,i){return bytes.getUint8(i+16);});
      actor.bodyPoseProgram={decoder:'alternate-body-pose',artSource:actor.bank,ownerContext:actor.nativeOwnerContext,
        selector:actor.animationKey,flagA:actor.variantSelector,flagB:actor.nativeFlagB,initialization:initialization};
      actor.artSourceId='combat-actor-art-source:'+actor.bank;
      actor.poseId='body-pose:'+actor.bank+':'+actor.animationKey+':'+actor.variantSelector+':'+actor.nativeFlagB+':'+actor.nativeOwnerContext;
      actor.poseBlocked=null;actor.poseProgramStatus='qualified-alternate-program';
      actor.poseFrame=0;
    }

    function programForActor(actor) {
      if (actor && actor.decoderMode !== 0) return qualifiedPoseForActor(actor,'alternate');
      if (!catalog || !catalog.getPhysicalPoseProgram || !actor ||
          !Number.isInteger(actor.bank) || !Number.isInteger(actor.animationKey) ||
          !Number.isInteger(actor.nativeFacing)) return null;
      if (Number.isInteger(actor.poseStateIndex) && catalog.getPhysicalPoseProgramByStateIndex) {
        var selected = catalog.getPhysicalPoseProgram(actor.bank, actor.animationKey,
          actor.nativeFacing, actor.variantSelector);
        if (selected && selected.stateIndex === actor.poseStateIndex) return selected;
        return catalog.getPhysicalPoseProgramByStateIndex(actor.bank, actor.poseStateIndex);
      }
      return catalog.getPhysicalPoseProgram(actor.bank, actor.animationKey,
        actor.nativeFacing, actor.variantSelector);
    }

    function startPose(actor) {
      actor.poseFrame = 0;
      actor.poseStateIndex = null;
      actor.decoderMode = 0;
      var poseProgram = programForActor(actor);
      actor.poseStateIndex = poseProgram ? poseProgram.stateIndex : null;
      actor.poseCursor = -1;
      actor.poseDelay = 0;
      actor.displayedFrameToken = 0;
      actor.decoderMode = 0;
      actor.material = new Array(16).fill(255);
      actor.materialDelta = new Array(16).fill(0);
      actor.poseBlocked = null;
      var controlOpcodes = poseProgram && poseProgram.controlOpcodes || [];
      actor.poseProgramStatus = !poseProgram ? 'unresolved' :
        (Array.isArray(poseProgram.frames) && poseProgram.frames.length
          ? 'native-program' : 'empty-native-program');
      actor.poseLoop = !poseProgram || controlOpcodes.indexOf(0x04) !== -1 ||
        controlOpcodes.indexOf('0x04') !== -1;
      actor.poseDuration = poseProgram ? poseProgram.durationFrames : 0;
      actor.poseReadyTick = state.tick + Math.max(1,
        Math.ceil((actor.poseDuration || 2) / 2));
    }

    function updateActorPose(actor) {
      if (actor.poseBlocked) return;
      var boundary = advanceNativePose(actor, programForActor, 256, registerSharedPoseRequest,
        capturedScheduler ? function(record){return [0,1,4,21].includes(record.opcode)?null:'resume-pose-control';} : null);
      if (boundary) {
        actor.poseBlocked = boundary;
        actor.poseProgramStatus = boundary;
        actorBoundary('Actor ' + actor.slot + ' requires ' + boundary +
          ' at counted record ' + actor.poseCursor + '.', boundary);
      }
    }

    function ensureActor(slot) {
      if (state.actors[slot]) return state.actors[slot];
      var template = templateForSlot(slot);
      var selector = selectorFromTemplate(template);
      var actor = {
        id: template ? template.id : 'actor:runtime:slot:' + String(slot).padStart(2, '0'),
        label: template ? template.label : 'Actor slot ' + slot,
        slot: slot,
        artSourceId: template ? template.artSourceId : null,
        capability: template ? template.capability : M.capabilities.NEEDS_RESEARCH,
        visible: false,
        opacityByte: 255,
        renderModeByte: template && template.source &&
          Number.isInteger(template.source.renderMode)
          ? template.source.renderMode & 0xFF : 0,
        x: template ? template.initial.x : 0,
        y: template ? template.initial.y : 0,
        z: template ? template.initial.z : 0,
        secondaryY: 0,
        heightModeByte: 0,
        facing: Number.isInteger(selector.facing) ? 'native-' + selector.facing : 'unresolved',
        poseId: Number.isInteger(selector.bank) && Number.isInteger(selector.key) &&
          Number.isInteger(selector.facing) ? poseId(selector.bank, selector.key, selector.facing) : null,
        bank: selector.bank,
        animationKey: selector.key,
        nativeFacing: selector.facing,
        variantSelector: selector.variant,
        poseFrame: 0,
        poseLoop: true,
        poseDuration: 0,
        poseReadyTick: 0,
        bodyPoseProgram: null,
        movementFrame: 0,
        activeMovementId: null,
        uniformScale: 1,
        tint: { red: 255, green: 255, blue: 255 },
        yawDegrees: 0,
        transformChannel: 0,
        source: template ? M.cloneJson(template.source || {}, 'actor.source') : {}
      };
      state.actors[slot] = actor;
      startPose(actor);
      return actor;
    }

    function actorForCommand(slot, purpose) {
      if (state.actors[slot]) return state.actors[slot];
      actorBoundary(purpose + ' for slot ' + slot +
        ' requires a launch-time Actor record that is not stored in the Director stream.');
      return null;
    }

    function sameContextValue(left, right) {
      return JSON.stringify(left) === JSON.stringify(right);
    }

    function contextActorMap(frameState) {
      var output = {};
      (frameState && frameState.actors || []).forEach(function(actor) {
        output[actor.slot] = actor;
      });
      return output;
    }

    function applyContextActor(actorRow, priorRow) {
      delete vacantMovementActivity[actorRow.slot];
      var actor = state.actors[actorRow.slot] || ensureActor(actorRow.slot);
      if (actorRow.nativeActorState && (!priorRow || actorRow.nativeActorState !== priorRow.nativeActorState)) {
        var nativeFields = decodeNativeActorState(actorRow.nativeActorState);
        var priorNativeFields = priorRow && priorRow.nativeActorState
          ? decodeNativeActorState(priorRow.nativeActorState) : null;
        Object.keys(nativeFields).forEach(function(field) {
          if (!priorNativeFields || !sameContextValue(nativeFields[field], priorNativeFields[field])) {
            actor[field] = nativeFields[field];
          }
        });
      }
      [
        ['id', 'id'], ['label', 'label'], ['artSourceId', 'artSourceId'],
        ['capability', 'capability'], ['visible', 'visible'],
        ['opacityByte', 'opacityByte'], ['renderModeByte', 'renderModeByte'],
        ['baseX', 'x'], ['baseY', 'y'], ['baseZ', 'z'],
        ['secondaryY', 'secondaryY'], ['heightModeByte', 'heightModeByte'],
        ['facing', 'facing'], ['poseId', 'poseId'], ['bank', 'bank'],
        ['animationKey', 'animationKey'], ['nativeFacing', 'nativeFacing'],
        ['variantSelector', 'variantSelector'], ['poseFrame', 'poseFrame'],
        ['displayedFrameToken', 'displayedFrameToken'], ['poseCursor', 'poseCursor'],
        ['poseDelay', 'poseDelay'], ['poseStateIndex', 'poseStateIndex'],
        ['poseBlocked', 'poseBlocked'], ['decoderMode', 'decoderMode'],
        ['sourceRowOrdinal', 'sourceRowOrdinal'], ['material', 'material'],
        ['materialDelta', 'materialDelta'],
        ['poseProgramStatus', 'poseProgramStatus'], ['poseLoop', 'poseLoop'],
        ['poseDuration', 'poseDuration'], ['bodyPoseProgram', 'bodyPoseProgram'],
        ['movementFrame', 'movementFrame'], ['activeMovementId', 'activeMovementId'],
        ['nativeUniformScale', 'uniformScale'], ['tint', 'tint'],
        ['yawDegrees', 'yawDegrees'], ['transformChannel', 'transformChannel'],
        ['source', 'source']
      ].forEach(function(pair) {
        var sourceField = pair[0];
        var targetField = pair[1];
        if (!Object.prototype.hasOwnProperty.call(actorRow, sourceField)) return;
        if (priorRow && sameContextValue(actorRow[sourceField], priorRow[sourceField])) return;
        var value = actorRow[sourceField];
        actor[targetField] = value && typeof value === 'object'
          ? M.cloneJson(value, 'context actor ' + sourceField) : value;
      });
      actor.contextSourceAssetId = contextRuntime.assetId;
      if (actor.nativeRecordBase || actor.source && actor.source.recordHex) forgetNativeRecord(actor);
    }

    function contextDialogueMap(frameState) {
      var output = {};
      (frameState && frameState.dialogue || []).forEach(function(row) {
        var payload = row.payload || {};
        var labelMatch = String(row.label || '').match(/(\d+)$/);
        var windowId = Number.isInteger(payload.windowId)
          ? payload.windowId : Number(labelMatch && labelMatch[1]);
        if (Number.isInteger(windowId)) output[windowId] = row;
      });
      return output;
    }

    function applyContextDialogue(row, priorRow) {
      if(dialogueEngine)return;
      var payload = row.payload || {};
      var labelMatch = String(row.label || '').match(/(\d+)$/);
      var windowId = Number.isInteger(payload.windowId)
        ? payload.windowId : Number(labelMatch && labelMatch[1]);
      if (!Number.isInteger(windowId)) return;
      var window = state.dialogues[windowId];
      if (!window) {
        window = state.dialogues[windowId] = {
          windowId: windowId,
          selector: payload.presentationArchiveSelector,
          entrySelector: payload.presentationEntrySelector,
          archive: null,
          entry: null,
          segments: [payload.text || ''],
          segmentIndex: 0,
          paused: payload.paused === true,
          readyTick: Number.MAX_SAFE_INTEGER,
          closed: false,
          ownerActorSlot: payload.ownerActorSlot,
          layout: payload.layout || {},
          speaker: payload.speaker,
          rawText: payload.rawText,
          dialogueArchiveId: payload.dialogueArchiveId,
          dialogueEntryId: payload.dialogueEntryId,
          sourceNodeId: 'context:' + contextRuntime.assetId + ':window:' + windowId,
          contextSourceAssetId: contextRuntime.assetId
        };
        return;
      }
      if (!priorRow || !sameContextValue(payload.paused,
          priorRow.payload && priorRow.payload.paused)) {
        window.paused = payload.paused === true;
      }
      if (!priorRow || !sameContextValue(payload.text,
          priorRow.payload && priorRow.payload.text)) {
        window.segments = [payload.text || ''];
        window.segmentIndex = 0;
      }
      window.selector = payload.presentationArchiveSelector;
      window.entrySelector = payload.presentationEntrySelector;
      window.ownerActorSlot = payload.ownerActorSlot;
      window.layout = payload.layout || {};
      window.speaker = payload.speaker;
      window.rawText = payload.rawText;
      window.dialogueArchiveId = payload.dialogueArchiveId;
      window.dialogueEntryId = payload.dialogueEntryId;
      window.closed = false;
    }

    function applyContextDelta(delta) {
      if (!delta) return;
      if(Object.prototype.hasOwnProperty.call(delta,'nativeActorDrawing'))nativeActorDrawing=delta.nativeActorDrawing?M.cloneJson(delta.nativeActorDrawing,'context native Actor drawing'):null;
      (delta.actors || []).forEach(function(actor) {
        applyContextActor(actor, null);
      });
      (delta.removedActorSlots || []).forEach(function(slot) {
        delete vacantMovementActivity[slot];
        if (state.actors[slot] &&
            state.actors[slot].contextSourceAssetId === contextRuntime.assetId) {
          delete state.actors[slot];
        }
      });
      (delta.dialogue || []).forEach(function(row) {
        applyContextDialogue(row, null);
      });
      (delta.removedDialogueWindowIds || []).forEach(function(windowId) {
        if (state.dialogues[windowId] &&
            state.dialogues[windowId].contextSourceAssetId === contextRuntime.assetId) {
          state.dialogues[windowId].closed = true;
        }
      });
      if (Object.prototype.hasOwnProperty.call(delta, 'background')) {
        state.background = M.cloneJson(delta.background, 'context background');
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'transformChannels')) {
        state.transformChannels = M.cloneJson(
          delta.transformChannels, 'context transform channels');
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'cameraState')) {
        state.projectionTransform = Object.assign({}, delta.cameraState);
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'actorProjection')) {
        state.cameras.actor = cameraFromProjection(delta.actorProjection,
          'Concurrent parent-event Director Actor camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'registeredProjection')) {
        state.cameras.registered = cameraFromProjection(delta.registeredProjection,
          'Concurrent parent-event Director registered camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'sceneColor')) {
        state.sceneColor = Object.assign({}, delta.sceneColor);
      }
      // Explicit native ownership/history takes priority over a presentation-only context.
      if (Object.prototype.hasOwnProperty.call(delta, 'overlays') && (!externalProducers || externalProducers.initialColor===undefined)) {
        state.overlay = delta.overlays && delta.overlays.length
          ? M.cloneJson(delta.overlays[0], 'context overlay') : null;
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'sceneVignette')) {
        state.sceneVignette = delta.sceneVignette
          ? M.cloneJson(delta.sceneVignette, 'context scene vignette') : null;
      }
      if (Object.prototype.hasOwnProperty.call(delta, 'oversizedImageView')) {
        state.oversizedImageView = Object.assign({}, delta.oversizedImageView);
      }
    }

    function applyContextTimeline(tick) {
      if (!contextRuntime) return;
      if (tick + contextTickOffset >= contextFrameCount &&
          (contextRuntime.safetyLimited || (!contextRuntime.terminated &&
           contextRuntime.outcome && contextRuntime.outcome !== 'stream-exhausted'))) {
        stopReason = contextRuntime.safetyLimited ? 'context-limit' : 'context-input';
        missing('The concurrent Director context has no supported state beyond its ' +
          contextFrameCount + ' retained updates.');
        return;
      }
      var contextIndex = clamp(tick + contextTickOffset, 0,
        contextFrameCount - 1);
      if (Array.isArray(contextRuntime.contextFrames)) {
        for (var deltaIndex = priorContextFrameIndex + 1;
            deltaIndex <= contextIndex; deltaIndex++) {
          applyContextDelta(contextRuntime.contextFrames[deltaIndex]);
        }
        priorContextFrameIndex = Math.max(priorContextFrameIndex, contextIndex);
        return;
      }
      // The importer consumes own fields; retained records can share a schema.
      var frameState = plainRetainedFrame(contextRuntime.states[contextIndex]);
      nativeActorDrawing=frameState.nativeActorDrawing?M.cloneJson(frameState.nativeActorDrawing,'context native Actor drawing'):null;
      var currentActors = contextActorMap(frameState);
      var priorActors = contextActorMap(priorContextState);
      Object.keys(currentActors).forEach(function(slot) {
        applyContextActor(currentActors[slot], priorActors[slot]);
      });

      var currentDialogue = contextDialogueMap(frameState);
      var priorDialogue = contextDialogueMap(priorContextState);
      Object.keys(currentDialogue).forEach(function(windowId) {
        if (priorDialogue[windowId] && sameContextValue(
            currentDialogue[windowId], priorDialogue[windowId])) return;
        applyContextDialogue(currentDialogue[windowId], priorDialogue[windowId]);
      });
      Object.keys(priorDialogue).forEach(function(windowId) {
        if (currentDialogue[windowId]) return;
        var window = state.dialogues[windowId];
        if (window && window.contextSourceAssetId === contextRuntime.assetId) {
          window.closed = true;
        }
      });

      if (!priorContextState ||
          !sameContextValue(frameState.background, priorContextState.background)) {
        state.background = M.cloneJson(frameState.background, 'context background');
      }
      if (!priorContextState ||
          !sameContextValue(frameState.transformChannels, priorContextState.transformChannels)) {
        state.transformChannels = M.cloneJson(
          frameState.transformChannels, 'context transform channels');
      }
      if (!priorContextState ||
          !sameContextValue(frameState.cameraState, priorContextState.cameraState)) {
        state.projectionTransform = Object.assign({}, frameState.cameraState);
      }
      if (!priorContextState ||
          !sameContextValue(frameState.actorProjection, priorContextState.actorProjection)) {
        state.cameras.actor = cameraFromProjection(frameState.actorProjection,
          'Concurrent parent-event Director Actor camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (!priorContextState ||
          !sameContextValue(frameState.registeredProjection,
            priorContextState.registeredProjection)) {
        state.cameras.registered = cameraFromProjection(frameState.registeredProjection,
          'Concurrent parent-event Director registered camera',
          'context-runtime:' + contextRuntime.assetId);
      }
      if (!priorContextState ||
          !sameContextValue(frameState.sceneColor, priorContextState.sceneColor)) {
        state.sceneColor = Object.assign({}, frameState.sceneColor);
      }
      if ((!externalProducers || externalProducers.initialColor===undefined) && (!priorContextState ||
          !sameContextValue(frameState.overlays, priorContextState.overlays))) {
        state.overlay = frameState.overlays && frameState.overlays.length
          ? M.cloneJson(frameState.overlays[0], 'context overlay') : null;
      }
      if (!priorContextState ||
          !sameContextValue(frameState.sceneVignette, priorContextState.sceneVignette)) {
        state.sceneVignette = frameState.sceneVignette
          ? M.cloneJson(frameState.sceneVignette, 'context scene vignette') : null;
      }
      if (!priorContextState ||
          !sameContextValue(frameState.oversizedImageView,
            priorContextState.oversizedImageView)) {
        state.oversizedImageView = Object.assign({}, frameState.oversizedImageView);
      }
      priorContextState = frameState;
    }

    function selectedActors(selector) {
      if (selector === -1) {
        return Object.keys(state.actors).map(function(slot) { return state.actors[slot]; });
      }
      var actor = state.actors[selector];
      return actor ? [actor] : [];
    }

    function executeActorBinding(node, words) {
      if (node.opcode === 0x92 && state.directorMode !== 2) return;
      var slot = unsigned(words[1]) & 255;
      if (slot >= 28) { actorBoundary('Actor binding destination is outside the 28 primary slots.'); return; }
      if (!actorInputRows) { actorBoundary('Actor binding requires all 20 caller Actor-input rows.'); return; }
      var ordinal = -1;
      if (node.opcode === 0xA6) {
        var member = signed(words[2]);
        if (!currentUnitMembers || member < 0 || member >= 5) {
          actorBoundary('Current-unit binding requires five member IDs and a member index from 0 through 4.'); return;
        }
        for (var i=0; i<20; i++) {
          var row = launchBytes(actorInputRows[i], 0xF8);
          if ((row.getUint32(0x40) & 256) && row.getUint32(0x48) && row.getUint8(0xF6) === currentUnitMembers[member]) { ordinal=i; break; }
        }
      } else {
        for (var rowIndex=0; rowIndex<20; rowIndex++) {
          var art = launchBytes(actorInputRows[rowIndex],0xF8).getUint32(0x48);
          if (art && nativeClassFamilyMatch(art, words[2])) { ordinal=rowIndex; break; }
        }
      }
      if (ordinal < 0) return; // Known complete rows establish a native no-match return.
      for (var from=0; from<28; from++) {
        var actor = state.actors[from];
        if (!actor || actor.sourceRowOrdinal !== ordinal) continue;
        var destination = state.actors[slot];
        state.actors[slot] = actor; actor.slot = slot;
        if (destination) { state.actors[from] = destination; destination.slot = from; }
        else delete state.actors[from];
        // Native binding swaps only Actor pointers. Slot-owned jobs stay in place.
        swapMovementActivity(actor, destination, from);
        return;
      }
      if (!initialActors && !contextRuntime) actorBoundary('Binding found its caller row, but launch Actor occupancy was not supplied.');
    }

    function setActorSelector(actor, bank, key, facing, variant) {
      if (!actor) return;
      actor.bodyPoseProgram = null;
      if (Number.isInteger(bank) && bank !== -1) {
        actor.bank = bank;
        actor.artSourceId = 'cutscene-art-bank:' + bank;
      }
      if (Number.isInteger(key) && key !== -1) actor.animationKey = key;
      if (Number.isInteger(facing) && facing !== -1) {
        actor.nativeFacing = facing;
        actor.facing = 'native-' + facing;
      }
      if (Number.isInteger(variant) && variant !== -1) actor.variantSelector = variant;
      if (Number.isInteger(actor.bank) && Number.isInteger(actor.animationKey) &&
          Number.isInteger(actor.nativeFacing)) {
        actor.poseId = poseId(actor.bank, actor.animationKey, actor.nativeFacing);
      }
      startPose(actor);
      updateActorPose(actor);
    }

    function executeActorCreate(node, words, unresolvedWordOffsets) {
      var slot = signed(words[1]);
      if (slot < 0 || slot >= 28) { actorBoundary('Actor construction requires a primary slot from 0 through 27.'); return; }
      if (continuousResume || nativeLaunch) {
        var resolved=nativeLaunch&&catalog.getPhysicalPoseProgram(signed(words[2]),signed(words[3]),signed(words[4]),unsigned(words[9])&255);
        var service = nativeLaunch ? {words:words,stateIndex:resolved&&resolved.stateIndex,presentationByte:nativeLaunch.input.actorPresentationWord&255,evidenceReference:'computed ROM State selection and preview launch allocation'} : actorService('directActorCreates',node);
        if (!service || !service.words.every(function(v,i){return unsigned(v) === unsigned(words[i]);}) ||
            (state.actors[slot]&&!nativeLaunch) || unresolvedWordOffsets.length) {
          actorBoundary('Direct creation requires the exact command occurrence, an empty slot, and qualified allocation and registration.','actor-creation-input'); return;
        }
        var selectedProgram = catalog.getPhysicalPoseProgram(signed(words[2]),signed(words[3]),signed(words[4]),unsigned(words[9]) & 255);
        var x = actorCoordinate(words[5]), y = actorCoordinate(words[6]), z = actorCoordinate(words[7]);
        if (!selectedProgram || selectedProgram.stateIndex !== service.stateIndex || y === -1) {
          actorBoundary('Direct creation requires its qualified ordinary State and explicit terrain-free height.','actor-creation-state'); return;
        }
        if(nativeLaunch){try{var pointer=nativeLaunch.machine.get(launchInitialization.rootAddress+24+slot*4);service.allocationAddress=pointer||nativeLaunch.allocate(336);nativeLaunch.machine.put(launchInitialization.rootAddress+24+slot*4,service.allocationAddress);}catch(error){producerBoundary(error.message,error.code);return;}}
        // func_0029D790 clears the allocation. func_0029DC0C initializes this
        // ordinary mode-zero path, then the creator performs one immediate pose.
        // Captured resume supplies allocation; fresh launch owns its preview arena.
        var bytes = new DataView(new ArrayBuffer(336));
        bytes.setUint32(0xE0,0x8022F2CC); bytes.setInt32(0xE4,slot); bytes.setInt32(0xE8,signed(words[2]));
        bytes.setInt32(0xF0,-1); bytes.setInt16(0x134,service.stateIndex); bytes.setInt16(0x138,signed(words[3]));
        [0x104,0x108,0x10C,0x130].forEach(function(at){bytes.setFloat32(at,1);});
        if (x !== -1 || y !== -1) {bytes.setFloat32(0x11C,x);bytes.setFloat32(0x120,y);bytes.setFloat32(0x124,z);}
        bytes.setUint8(0x13C,service.presentationByte);
        bytes.setUint8(0x13F,words[4]);bytes.setUint8(0x140,words[4]);bytes.setUint8(0x141,words[8]);
        [0x142,0x143,0x144,0x147].forEach(function(at){bytes.setUint8(at,255);});
        bytes.setUint8(0x146,words[9]);
        for(var materialIndex=0;materialIndex<16;materialIndex++)bytes.setUint8(materialIndex,255);
        var created = ensureActor(slot);
        applyNativeRecord(created,bytes,'qualified-direct-creation');
        created.bodyPoseProgram=null;created.artSourceId='cutscene-art-bank:'+created.bank;
        created.poseId=poseId(created.bank,created.animationKey,created.nativeFacing);
        created.facing='native-'+created.nativeFacing;created.visible=true;
        created.capturedMainScale=1;created.uniformScale=1;created.transformChannel=0;
        if(sharedActorProfile)created.matrixRecordBase=recordHex(bytes);
        updateActorPose(created);
        if(nativeLaunch)nativeLaunch.write(service.allocationAddress,new Uint8Array(nativeRecordForActor(created).buffer));
        recordTrace({tick:state.tick,kind:'direct-actor-creation',nodeId:node.id,slot:slot,
          allocationAddress:service.allocationAddress,stateIndex:service.stateIndex,
          evidenceReference:service.evidenceReference,review:'pending',recordHex:recordHex(nativeRecordForActor(created))});
        return;
      }
      var actor = ensureActor(slot);
      var template = templateForSlot(slot);
      var variantUnresolved = unresolvedWordOffsets.indexOf(9) !== -1;
      var rawSelector = {
        bank: signed(words[2]), key: signed(words[3]), facing: signed(words[4]),
        variant: variantUnresolved && template && template.source &&
          Number.isInteger(template.source.variantSelector)
          ? template.source.variantSelector : unsigned(words[9]) & 0xFF
      };
      var templateOwnsNode = template && template.source.placeNodeId === node.id;
      var selector = templateOwnsNode
        ? selectorFromTemplate(template, rawSelector) : rawSelector;
      actor.visible = true;
      actor.x = actorCoordinate(words[5]);
      actor.y = actorCoordinate(words[6]);
      actor.z = actorCoordinate(words[7]);
      actor.sourceRowOrdinal = 0;
      actor.uniformScale = 1;
      actor.opacityByte = 255;
      actor.renderModeByte = unsigned(words[8]) & 0xFF;
      actor.secondaryY = 0;
      actor.heightModeByte = 0;
      actor.transformChannel = 0;
      actor.movementFrame = 0;
      actor.tint = { red: 255, green: 255, blue: 255 };
      actor.yawDegrees = 0;
      setActorSelector(actor, selector.bank, selector.key, selector.facing, selector.variant);
      if (variantUnresolved) {
        actor.source.launchTranslationPreviewFallback = true;
        actor.source.launchTranslationTableIndex = launchTranslationIndex(node.rawWords[9]);
      }
    }

    function executeActorState(node, words) {
      var slot = signed(words[1]);
      if (slot === -1) return;
      var actor = actorForCommand(slot, 'Actor State');
      if (!actor) return;
      var row = rowFor(node, 'pose');
      var payload = row && row.clip.payload || {};
      var bank = Number.isInteger(payload.bank) ? payload.bank : signed(words[2]);
      var key = Number.isInteger(payload.animationKey) ? payload.animationKey : signed(words[3]);
      var facing = Number.isInteger(payload.nativeFacing)
        ? payload.nativeFacing : signed(words[4]);
      var variantWord = signed(words[8]);
      var variant = Number.isInteger(payload.variantSelector)
        ? payload.variantSelector : (variantWord === -1 ? actor.variantSelector : unsigned(words[8]) & 0xFF);
      var candidate = catalog.getPhysicalPoseProgram(bank === -1 ? actor.bank : bank,
        key === -1 ? actor.animationKey : key, facing === -1 ? actor.nativeFacing : facing, variant);
      if (!candidate) return; // Native failed selector lookup preserves the existing record.
      var x = actorCoordinate(words[5]), y = actorCoordinate(words[6]), z = actorCoordinate(words[7]);
      // func_002A08C0 tests X and Y (Y twice), then writes all three coordinates.
      if (x !== -1 || y !== -1) { actor.x = x; actor.y = y; actor.z = z; }
      actor.yawDegrees = 0;
      setActorSelector(actor, bank, key, facing, variant);
    }

    function executeMove(node, words) {
      var slot = signed(words[1]);
      // The native creator returns on a null Actor only when occupancy is known.
      if (initialActors && slot >= 0 && slot < 28 && !state.actors[slot]) return;
      var actor = actorForCommand(slot, 'Movement');
      if (!actor) return;
      try {
        var previous = state.movementJobs[slot];
        var next = createNativeMovement(actor, previous, words);
        if (next && next !== previous) {
          next.nodeId = node.id; next.slot = slot;
          state.movementJobs[slot] = next;
          actor.activeMovementId = 'runtime-movement:' + node.id;
          actor.movementFrame = 0;
        }
        if (state.directorMode === 2) {
          if (actor.nativeRecordBase || actor.source && actor.source.recordHex) forgetNativeRecord(actor);
          if(!nativeLaunch||nativeLaunch.input.proximityFlags&8||actor.heightModeByte&1)missing('Mode-two movement requires terrain or proximity inputs outside the planar Actor contract.');
        }
      } catch (error) {
        if (!(error instanceof RuntimeError)) throw error;
        actorBoundary(error.message, error.code);
      }
    }

    function executeTurn(node, words) {
      var slot = signed(words[1]);
      var actor = actorForCommand(slot, 'Facing turn');
      if (!actor) return;
      var startFacing = signed(words[2]);
      var targetFacing = signed(words[3]);
      var direction = signed(words[4]);
      var budget = signed(words[5]);
      var startPhase = PHASE_FOR_FACING[startFacing];
      var targetPhase = PHASE_FOR_FACING[targetFacing];
      if (!Number.isInteger(startPhase) || !Number.isInteger(targetPhase) ||
          (direction !== -1 && direction !== 1)) {
        missing('Facing turn ' + node.id + ' has an unsupported native phase selection.');
        actor.nativeFacing = targetFacing;
        actor.facing = 'native-' + targetFacing;
        return;
      }
      var distance = direction === -1
        ? (targetPhase - startPhase + 12) % 12
        : (startPhase - targetPhase + 12) % 12;
      if (!distance) return;
      var cadence = Math.trunc((Math.trunc(budget / 3) * 3) / distance);
      cadence = Math.max(1, cadence);
      state.turnJobs[slot] = {
        nodeId: node.id,
        slot: slot,
        startPhase: startPhase,
        targetFacing: targetFacing,
        phaseStep: -direction,
        phaseDistance: distance,
        cadence: cadence,
        elapsed: 0,
        completionCalls: 1 + (distance - 1) * cadence
      };
    }

    function executeProjection(node, words, identity) {
      var row = rowFor(node, 'camera');
      var payload = row && row.clip.payload || {};
      var target = identity ? { translateX: 0, translateY: 0, scaleX: 1, scaleY: 1 } : {
        translateX: payload.target && Number.isFinite(payload.target.translateX)
          ? payload.target.translateX : fixed(words[1]),
        translateY: payload.target && Number.isFinite(payload.target.translateY)
          ? payload.target.translateY : fixed(words[2]),
        scaleX: payload.target && Number.isFinite(payload.target.scaleX)
          ? payload.target.scaleX : fixed(words[3]),
        scaleY: payload.target && Number.isFinite(payload.target.scaleY)
          ? payload.target.scaleY : fixed(words[4])
      };
      var countWord = identity ? words[1] : words[5];
      var duration = row && Number.isInteger(row.clip.durationFrames)
        ? row.clip.durationFrames : lowU16(countWord);
      if (identity && lowS16(countWord) === 0) {
        var current = state.projectionTransform;
        var exactIdentity = current.translateX === 0 && current.translateY === 0 &&
          current.scaleX === 1 && current.scaleY === 1;
        if (exactIdentity) return;
        var translationCount = Math.trunc((current.translateX * current.translateX +
          current.translateY * current.translateY) / 80);
        var scaleCount = Math.trunc((Math.min(current.scaleX, current.scaleY) - 1) * 100);
        duration = lowU16(Math.max(lowS16(translationCount), lowS16(scaleCount)));
      }
      if (duration === 0) return;
      if(nativeLaunch&&nativeLaunch.input.sceneMode===2)OB64.cutsceneRomStart.projection(nativeLaunch,target,duration);
      state.projectionJob = {
        nodeId: node.id,
        duration: duration,
        remaining: duration,
        elapsed: 0,
        from: Object.assign({}, state.projectionTransform),
        to: target
      };
      state.cameraEvents.push(eventRow(node, 'camera', 'Scene projection transition', {
        presentationKind: identity ? 'projection-identity-transition' : 'projection-transform',
        target: target,
        nativeCountdown: duration
      }));
    }

    function screenEdgeValue(initial, final, progress, duration) {
      if (duration === 0) return final;
      return initial + Math.trunc((final - initial) * progress / duration);
    }

    function executeScreenTransition(node, words) {
      var initialFirst = lowS16(words[1]);
      var initialSecond = lowS16(words[2]);
      var finalFirst = lowS16(words[3]);
      var finalSecond = lowS16(words[4]);
      var duration = lowU16(words[5]);
      var persistence = lowS16(words[6]);
      var titleVariant = screenTransitionVariant === 0;
      var cutsceneVariant = Number.isInteger(screenTransitionVariant) &&
        screenTransitionVariant !== 0;
      var resolvedInitialFirst = cutsceneVariant && initialFirst === -1 &&
        initialSecond === -1 ? 24 : initialFirst;
      var resolvedInitialSecond = cutsceneVariant && initialFirst === -1 &&
        initialSecond === -1 ? 24 : initialSecond;
      state.screenTransition = {
        nodeId: node.id,
        initialFirst: resolvedInitialFirst,
        initialSecond: resolvedInitialSecond,
        authoredInitialFirst: initialFirst,
        authoredInitialSecond: initialSecond,
        finalFirst: finalFirst,
        finalSecond: finalSecond,
        progress: 0,
        duration: duration,
        persistence: persistence,
        currentFirst: screenEdgeValue(
          resolvedInitialFirst, finalFirst, 0, duration),
        currentSecond: screenEdgeValue(
          resolvedInitialSecond, finalSecond, 0, duration),
        directorSelector: screenTransitionVariant,
        presentationKind: titleVariant ? 'title-card-reveal' :
          (cutsceneVariant ? 'cutscene-crop' : 'external-unresolved')
      };
      if (!Number.isInteger(screenTransitionVariant)) {
        missing('Screen-edge transition ' + node.id +
          ' requires the launch Director selector to choose title reveal or cutscene crop rendering.');
      }
      state.effectEvents.push(eventRow(node, 'effect', 'Screen-edge transition', {
        sourceSystem: 'director-native',
        nativeOpcode: '0x3B',
        presentationKind: state.screenTransition.presentationKind,
        initialEdges: [resolvedInitialFirst, resolvedInitialSecond],
        finalEdges: [finalFirst, finalSecond],
        nativeDuration: duration,
        persistentFinalState: persistence !== 0
      }));
    }

    function executeActorPresentationBootstrap(node) {
      state.actorPresentationJob = {
        nodeId: node.id,
        kind: 'primary',
        evidenceStatus: 'external-unresolved'
      };
      missing('Actor-presentation bootstrap ' + node.id +
        ' requires the launch scene configuration, persistent-character records, and their target coordinates; the stream operand is not an Actor roster.');
      state.effectEvents.push(eventRow(node, 'effect', 'Actor-presentation bootstrap', {
        sourceSystem: 'director-native',
        nativeOpcode: '0x3F',
        presentationKind: 'persistent-character-actor-bootstrap',
        inputStatus: 'external-unresolved'
      }));
    }

    function executeSceneVignette(node, words) {
      if(echoProfile&&imageEcho){producerBoundary('Image echo currently supports one initialized scene image per launch.','image-echo-owner');return;}
      var presentation = launchProfile.oversizedImagePresentation || null;
      var slot = signed(words[1]);
      var alphaCap = signed(words[7]);
      var orientationFlags = unsigned(words[8]);
      state.sceneVignette = {
        nodeId: node.id,
        sourceAssetId: presentation && presentation.assetId || null,
        sourceResourceKey: presentation && presentation.resourceKey || null,
        sourceArchiveIndex: presentation && presentation.archiveIndex || null,
        launchRowSelector: presentation && Number.isInteger(presentation.rowSelector)
          ? presentation.rowSelector : null,
        slot: slot,
        activeSlotByte: slot & 0xFF,
        translateX: signed(words[2]),
        translateY: signed(words[3]),
        ignoredPayload: signed(words[4]),
        scaleXPercent: signed(words[5]),
        scaleYPercent: signed(words[6]),
        alphaCap: alphaCap,
        transitionAlphaByte: unsigned(words[7]) & 0xFF,
        orientationFlags: orientationFlags,
        orientationBit08: (orientationFlags & 0x08) !== 0,
        baseRotationDegrees: (orientationFlags & 0x08) !== 0
          ? { x: 0, y: 0, z: -5 }
          : { x: 0, y: -90, z: 5 },
        sourceScaleX: 1,
        sourceScaleY: 1,
        outputScaleDivisor: 2,
        evidenceStatus: presentation && presentation.assetId
          ? 'native-static-exact' : 'launch-inputs-unresolved'
      };
      if (!presentation || !presentation.assetId) {
        missing('Scene vignette ' + node.id +
          ' requires the class-4 launch image selected through event property 0xE9.');
      }
      if(echoProfile){try{
        var image=catalog.getImageAsset(state.sceneVignette.sourceAssetId),c=state.cameras.registered;
        if(!image)fail('Image echo initialization requires current source image metadata.','image-echo-input');
        imageEcho=new OB64.cutsceneImageEcho(options.z64);
        sharedActor=null;
        if([imageEcho.machine,nativeLaunch.machine,dialogueEngine.machine].reduce(function(total,m){return total+m.regions.reduce(function(n,r){return n+r.bytes.length;},0);},0)>131072)fail('Combined launch, resource, and image matrix memory exceeds 128 KiB.','image-echo-memory-bound');
        imageEcho.initialize(state.sceneVignette,[c.fovYDegrees,c.aspect,c.near,c.far,0,c.eye.x,c.eye.y,c.eye.z,c.target.x,c.target.y,c.target.z,c.up.x,c.up.y,c.up.z],image.width,image.height);
      }catch(error){producerBoundary(error.message,error.code);return;}}
      state.effectEvents.push(eventRow(node, 'effect', 'Scene vignette', {
        sourceSystem: 'director-native',
        nativeOpcode: '0x3A',
        sourceAssetId: state.sceneVignette.sourceAssetId,
        sceneImageSlot: slot,
        nativeTranslation: {
          x: state.sceneVignette.translateX,
          y: state.sceneVignette.translateY
        },
        nativeScalePercent: {
          x: state.sceneVignette.scaleXPercent,
          y: state.sceneVignette.scaleYPercent
        },
        alphaCap: alphaCap,
        orientationFlags: orientationFlags,
        inputStatus: state.sceneVignette.evidenceStatus
      }));
    }

    function executeOversizedImageTransition(node, words) {
      var startX = signed(words[1]);
      var startY = signed(words[2]);
      var targetX = signed(words[4]);
      var targetY = signed(words[5]);
      var zoomDirection = signed(words[6]);
      var duration = signed(words[7]);
      var currentPosition = startX === -1 && startY === -1;
      var rateStartX = currentPosition ? state.oversizedImageView.x : startX;
      var rateStartY = currentPosition ? state.oversizedImageView.y : startY;
      var deltaX = targetX - rateStartX;
      var deltaY = targetY - rateStartY;
      var rateX = deltaX >= -1 && deltaX <= 1 ? 0 : Math.fround(deltaX / duration);
      var rateY = deltaY >= -1 && deltaY <= 1 ? 0 : Math.fround(deltaY / duration);
      state.oversizedImageTransitionJob = {
        nodeId: node.id,
        progress: 0,
        duration: duration,
        rateX: rateX,
        rateY: rateY,
        targetX: targetX,
        targetY: targetY,
        currentPositionStart: currentPosition
      };
      if (zoomDirection === 0) {
        if (state.oversizedImageView.zoomState === 3 ||
            state.oversizedImageView.zoomState === 1) {
          state.oversizedImageView.zoomState = 2;
        }
      } else if (state.oversizedImageView.zoomState === 4 ||
          state.oversizedImageView.zoomState === 2) {
        state.oversizedImageView.zoomState = 1;
      }
      recordTrace({
        tick: state.tick,
        kind: 'oversized-image-transition-start',
        nodeId: node.id,
        currentPositionStart: currentPosition,
        rateStartX: rateStartX,
        rateStartY: rateStartY,
        targetX: targetX,
        targetY: targetY,
        rateX: rateX,
        rateY: rateY,
        duration: duration,
        zoomDirection: zoomDirection
      });
      state.effectEvents.push(eventRow(node, 'effect',
        'Scripted oversized-image pan and zoom', {
          sourceSystem: 'director-native',
          nativeOpcode: '0x76',
          currentPositionStart: currentPosition,
          targetX: targetX,
          targetY: targetY,
          nativeDuration: duration,
          zoomDirection: zoomDirection
        }));
    }

    function executeCamera(node, words, bank) {
      var camera = {
        target: { x: fixed(words[1]), y: fixed(words[2]), z: fixed(words[3]) },
        eye: { x: fixed(words[4]), y: fixed(words[5]), z: fixed(words[6]) },
        fovYDegrees: fixed(words[7]),
        sourceNodeId: node.id,
        evidenceStatus: 'native-director-command',
        status: bank === 'actor' ? 'opcode 0x36 actor-side camera' :
          'opcode 0x35 registered-object camera'
      };
      if(nativeLaunch)camera=Object.assign({},state.cameras[bank],camera);
      state.cameras[bank] = camera;
      state.cameraEvents.push(eventRow(node, 'camera', 'Camera pose', {
        presentationKind: 'camera-pose',
        cameraBank: bank,
        target: camera.target,
        eye: camera.eye,
        fovY: camera.fovYDegrees
      }));
    }

    function executeTint(node, words) {
      var selector = signed(words[1]);
      var duration = Math.max(1, signed(words[8]));
      selectedActors(selector).forEach(function(actor) {
        actor.tint = { red: signed(words[2]), green: signed(words[3]), blue: signed(words[4]) };
        state.tintJobs[actor.slot] = {
          nodeId: node.id,
          slot: actor.slot,
          duration: duration,
          remaining: duration,
          elapsed: 0,
          from: Object.assign({}, actor.tint),
          to: { red: signed(words[5]), green: signed(words[6]), blue: signed(words[7]) }
        };
      });
    }

    function executeOverlay(node, words) {
      if(nativeLaunch||capturedServices){try{state.overlay=(nativeLaunch||capturedScheduler).createColor(words);state.overlay.sourceNodeId=node.id;state.overlayJob=state.overlay;}catch(error){producerBoundary(error.message,error.code);}return;}
      var creation=externalCreation('colorCreates',node);
      if (creation && externalProducers.initialColor !== undefined) {
        state.overlay=createNativeColor(state.overlay,words);
        state.overlay.ownerId=creation.ownerId;
        state.overlay.native=true;
        state.overlay.sourceNodeId=node.id;
        state.overlayJob=state.overlay;
        return;
      }
      if (producerBoundary('Color creation requires initial ownership and successful allocation/registration inputs.','color-creation-input')) return;
      assumption('Diagnostic color completion uses the legacy Director-update estimate; native callback history is unavailable.');
      var duration = Math.max(1, lowU16(words[1]));
      var startAlpha = signed(words[5]);
      if (startAlpha === -1 && state.overlay) startAlpha = state.overlay.alpha;
      if (startAlpha === -1) startAlpha = 0;
      state.overlay = {
        red: unsigned(words[2]) & 0xFF,
        green: unsigned(words[3]) & 0xFF,
        blue: unsigned(words[4]) & 0xFF,
        alpha: clamp(startAlpha, 0, 255),
        sourceNodeId: node.id
      };
      state.overlayJob = {
        nodeId: node.id,
        duration: duration,
        remaining: duration,
        elapsed: 0,
        startAlpha: state.overlay.alpha,
        targetAlpha: unsigned(words[6]) & 0xFF
      };
    }

    function executeSceneColor(node, words) {
      var duration = Math.max(1, lowU16(words[7]));
      state.sceneColor = {
        red: unsigned(words[1]) & 0xFF,
        green: unsigned(words[2]) & 0xFF,
        blue: unsigned(words[3]) & 0xFF
      };
      state.sceneColorJob = {
        nodeId: node.id,
        duration: duration,
        remaining: duration,
        elapsed: 0,
        from: Object.assign({}, state.sceneColor),
        to: {
          red: unsigned(words[4]) & 0xFF,
          green: unsigned(words[5]) & 0xFF,
          blue: unsigned(words[6]) & 0xFF
        }
      };
    }

    function executeBackground(node, words) {
      var commandOperand = signed(words[1]);
      if(nativeLaunch&&nativeLaunch.input.sceneMode===2){
        var selector=nativeLaunch.input.environmentSelector;
        if(!Number.isInteger(selector)||selector<0||selector>=80){producerBoundary('Fresh Stage requires an explicit ROM environment selector.','rom-start-environment');return;}
        var base=catalog.getBackgroundSelectorEntry('background-table:mode2-environment:80',selector);
        var front=catalog.getBackgroundSelectorEntry('background-table:mode2-overlay:80',selector);
        var layers=(base&&base.stageLayers||[]).concat(backgroundLayers(front,M.capabilities.PREVIEW_ONLY,catalog).map(function(layer){return {assetId:layer.assetId,role:'foreground-mask',depth:100+layer.nativeOrdinal,nativeOrdinal:layer.nativeOrdinal,evidenceStatus:'ROM-selector',associationStatus:'ROM foreground selector'};}));
        state.background=stagedBackground(layers,{background:{projection:{mode:'native-perspective-runtime'}}},{sourceKind:'ROM-launch-prescan',runtimeStatus:'ROM launch pre-scan and normal foreground copy',selectorTableId:'background-table:mode2-environment:80',selector:selector,environmentSelector:selector,foregroundSelector:selector,foregroundSelectorTableId:'background-table:mode2-overlay:80',nativeSceneProps:modeTwoStageProps(catalog,selector)});
        if(!state.background){producerBoundary('ROM environment has no supported Stage.','rom-start-environment');return;}
        return;
      }
      if(framebufferProfile&&nativeLaunch){try{if(framebufferLayerCount)fail('Additional background registrations require cumulative layer resource ownership.','framebuffer-layers');var directory=nativeLaunch.resource(0x016b3d18);if(commandOperand<0||commandOperand*4+4>directory.length)fail('Background selector exceeds its ROM directory.','framebuffer-layers');var key=new DataView(directory.buffer,directory.byteOffset,directory.byteLength).getUint32(commandOperand*4),group=nativeLaunch.resource(key);if(group.length%4)fail('Background group is not word aligned.','framebuffer-layers');framebufferLayerCount+=group.length/4;if(framebufferLayerCount>20)fail('Background registration exceeds twenty layers.','framebuffer-layers');}catch(error){producerBoundary(error.message,error.code);return;}}
      if (documentBackground && directorMode.value === 2) {
        state.background = M.cloneJson(documentBackground.background,
          'document launch background');
        return;
      }
      var observation = scene.backgroundRuntimeObservation || null;
      if (observation && Array.isArray(observation.stageLayers) &&
          observation.stageLayers.length) {
        state.background = observationBackground(scene, document, catalog);
        return;
      }
      var requestProfile = launchProfile.background.requests.find(function(request) {
        return request.wordStart === node.startWord;
      }) || null;
      if(framebufferProfile&&requestProfile&&requestProfile.stageLayers&&requestProfile.stageLayers.length&&requestProfile.selector!==commandOperand){producerBoundary('Changed background selector lacks matching current layer metadata.','framebuffer-layers');return;}
      var requestStageProps = requestProfile &&
        modeTwoStageProps(catalog, requestProfile.foregroundSelector);
      if (requestProfile && (Array.isArray(requestProfile.stageLayers) &&
          requestProfile.stageLayers.length || hasRenderableStageProps(requestStageProps))) {
        state.background = profileBackground(requestProfile, document, catalog);
        if (directorMode.value === 2) {
          if (requestProfile.foregroundSelector === null) {
            missing(requestProfile.foregroundStatus ||
              'The final mode-two foreground selector remains launch-owned.');
          }
        }
        return;
      }
      if (!requestProfile || requestProfile.selectorTableId === null ||
          requestProfile.selector === null) {
        state.background = {
          assetId: null,
          layers: [],
          capability: M.capabilities.NEEDS_RESEARCH,
          projection: M.cloneJson(document.background.projection || {},
            'background.projection'),
          runtimeStatus: requestProfile ? requestProfile.status :
            'The launch profile has no matching background request.',
          selectorTableId: requestProfile ? requestProfile.selectorTableId : null,
          selector: null,
          environmentSelector: null,
          foregroundSelectorTableId: requestProfile
            ? requestProfile.foregroundSelectorTableId : null,
          foregroundSelector: null
        };
        missing(requestProfile ? requestProfile.status :
          'The launch profile has no matching background request.');
        return;
      }
      if (directorMode.value === 2) {
        state.background = {
          assetId: null,
          layers: [],
          capability: M.capabilities.NEEDS_RESEARCH,
          projection: M.cloneJson(document.background.projection || {},
            'background.projection'),
          runtimeStatus: requestProfile.status +
            ' The selected resource is not a complete environment.',
          selectorTableId: requestProfile.selectorTableId,
          selector: requestProfile.selector,
          environmentSelector: requestProfile.environmentSelector,
          foregroundSelectorTableId: requestProfile.foregroundSelectorTableId,
          foregroundSelector: requestProfile.foregroundSelector
        };
        missing('Mode-two background selector ' + requestProfile.selector +
          ' has no complete registered Stage; its raw resource is only a partial layer or is empty.');
        return;
      }
      var tableId = requestProfile.selectorTableId;
      var runtimeSelector = requestProfile.selector;
      if (requestProfile.selectorSource === 'director-command-operand') {
        runtimeSelector = commandOperand;
      }
      var entry = catalog && catalog.getBackgroundSelectorEntry
        ? catalog.getBackgroundSelectorEntry(tableId, runtimeSelector) : null;
      var layers = backgroundLayers(entry, M.capabilities.PREVIEW_ONLY, catalog);
      state.background = {
        assetId: layers.length ? layers[0].assetId : null,
        layers: layers,
        capability: M.capabilities.PREVIEW_ONLY,
        projection: M.cloneJson(document.background.projection || {}, 'background.projection'),
        runtimeStatus: requestProfile.status,
        selectorTableId: tableId,
        selector: runtimeSelector,
        environmentSelector: requestProfile.environmentSelector,
        foregroundSelectorTableId: requestProfile.foregroundSelectorTableId,
        foregroundSelector: requestProfile.foregroundSelector
      };
      if (!layers.length) missing('Background selector ' + runtimeSelector + ' in ' + tableId +
        ' has no renderable archive member.');
    }

    function dialogueSegments(entry) {
      var text = entry && String(entry.text || '') || '';
      var segments = text.split(/\[pause\]/i).map(function(segment) {
        return segment.replace(/\[clear\]/gi, '').trim();
      });
      return segments.length ? segments : [''];
    }

    function dialogueDuration(text) {
      var scalar = Math.max(64, state.textSpeed || 512);
      return clamp(Math.ceil(Math.max(1, String(text || '').length) * scalar / 512), 12, 480);
    }

    function executeDialogueCreate(node, words, unresolvedWordOffsets) {
      var nativeSlot = null;
      if(dialogueEngine) {
        try {
          var key='dialogue:'+node.id, occurrence=externalOccurrences[key]||0;
          externalOccurrences[key]=occurrence+1;
          var registration=(externalProducers.dialogueCreates||[]).find(function(row){return row.nodeId===node.id&&row.occurrence===occurrence;});
          if(unresolvedWordOffsets.length)fail('Dialogue constructor still has unresolved Director inputs.','dialogue-constructor-input');
          if(dialogueEngine.lifecycle){
            if(registration)fail('Shared dialogue construction must not receive a recorded constructor outcome.','dialogue-constructor-input');
            var point=null;
            if((words[13]|0)===0){
              var ownerActor=state.actors[signed(words[4])],renderer=OB64.cutsceneRenderer;
              if(!ownerActor&&nativeLaunch&&signed(words[4])>=0&&signed(words[4])<28)point={absentActor:true};
              else {
              if(!ownerActor||!renderer||(!extendedModeTwoResume && !nativeLaunch && state.directorMode!==0)||state.cameras.actor.evidenceStatus==='external-unresolved'||state.cameras.registered.evidenceStatus==='external-unresolved')fail('Dialogue placement requires a current Actor and qualified mode-zero cameras.','dialogue-constructor-placement');
              if(nativeLaunch&&state.directorMode===2)point=OB64.cutsceneRomStart.dialoguePoint(nativeLaunch,ownerActor,state.cameras.actor);
              else {
              var channel=state.transformChannels[ownerActor.transformChannel]||identityTransformChannel();
              var renderedY=ownerActor.heightModeByte&4?ownerActor.y+ownerActor.secondaryY:ownerActor.heightModeByte&2?ownerActor.secondaryY:ownerActor.y;
              var geometry=renderer.modeZeroActorGeometry({x:ownerActor.x,y:renderedY,z:ownerActor.z,sceneTransform:channel,renderPipeline:'mode-zero-registered-prepass-actor-camera'},
                {registeredProjection:projectionFromCamera(state.cameras.registered)},projectionFromCamera(state.cameras.actor));
              if(!geometry)fail('Dialogue placement could not project the current Actor.','dialogue-constructor-placement');
              point=extendedModeTwoResume?capturedScheduler.dialoguePoint(ownerActor.slot):renderer.projectPointFloat(geometry.scenePoint,geometry.projection);
              }
              }
            }
            if(romOnlyStart&&nativeLaunch&&nativeLaunch.input.previewHeroName!==undefined)OB64.cutsceneRomStart.prepareDialogue(nativeLaunch,dialogueEngine,signed(words[4]));
            registration=dialogueEngine.lifecycle.create(words,'dialogue:'+node.id+':'+occurrence,point);nativeSlot=registration.slot;if(extendedModeTwoResume)capturedScheduler.compareDialogueRegistration(nativeSlot);
          }else nativeSlot=dialogueEngine.register(registration,words);
        } catch(error) {producerBoundary(error.message,error.code||'dialogue-registration-input');return;}
      } else if(producerBoundary('Dialogue requires native initial memory, constructor outcome, and complete service history.','dialogue-initial-input')) return;
      var windowId = signed(words[1]);
      var selector = signed(words[2]);
      var entrySelector = unresolvedWordOffsets.indexOf(3) === -1
        ? signed(words[3]) : null;
      var archive = catalog && catalog.getSerifuArchiveForPresentationSelector
        ? catalog.getSerifuArchiveForPresentationSelector(selector) : null;
      var entry = entrySelector == null ? null :
        archive && archive.entries && archive.entries[entrySelector] || null;
      var segments = nativeSlot===null?dialogueSegments(entry):[''];
      if (!entry && entrySelector != null) missing('Serifu selector ' + selector + ', entry ' + entrySelector +
        ' did not resolve to dialogue text.');
      state.dialogues[windowId] = {
        nativeSlot:nativeSlot,
        nativeOwnerId:nativeSlot===null?null:registration.ownerId,
        windowId: windowId,
        selector: selector,
        entrySelector: entrySelector,
        ownerActorSlot: signed(words[4]),
        archive: archive,
        entry: entry,
        segments: segments,
        segmentIndex: 0,
        readyTick: nativeSlot===null?state.tick + dialogueDuration(segments[0]):Number.MAX_SAFE_INTEGER,
        paused: false,
        closed: false,
        layout: {
          x: signed(words[5]), y: signed(words[6]), portraitSide: signed(words[8]),
          mirrorPortrait: signed(words[9]), pointerEdge: signed(words[10]),
          portraitIdentity: unresolvedWordOffsets.indexOf(11) === -1
            ? signed(words[11]) : null,
          portraitVariant: signed(words[12]),
          placementMode: signed(words[13])
        },
        launchTranslationMissing: unresolvedWordOffsets.length > 0,
        sourceNodeId: node.id
      };
    }

    function executeDialogueResume(words) {
      if(dialogueEngine) {try{dialogueEngine.resume(signed(words[1]));}catch(error){producerBoundary(error.message,error.code);}return;}
      if(producerBoundary('Dialogue resume requires current native resource ownership.','dialogue-initial-input'))return;
      var window = state.dialogues[signed(words[1])];
      if (!window || window.closed) return;
      window.segmentIndex = Math.min(window.segmentIndex + 1, window.segments.length - 1);
      window.paused = false;
      window.readyTick = state.tick + dialogueDuration(window.segments[window.segmentIndex]);
    }

    function executeDialogueClose(words) {
      if(dialogueEngine) {try{dialogueEngine.close(signed(words[1]));}catch(error){producerBoundary(error.message,error.code);}return;}
      if(producerBoundary('Dialogue close requires current native resource ownership.','dialogue-initial-input'))return;
      var window = state.dialogues[signed(words[1])];
      if (window) window.closed = true;
    }

    function programForSpriteEffect(effect) {
      if (!catalog || !effect) return null;
      if (catalog.getPhysicalPoseProgramByStateIndex &&
          Number.isInteger(effect.stateIndex) && effect.stateIndex >= 0) {
        var stateProgram = catalog.getPhysicalPoseProgramByStateIndex(
          effect.bank, effect.stateIndex);
        if (stateProgram) return stateProgram;
      }
      return catalog.getPhysicalPoseProgram && Number.isInteger(effect.bank) &&
        Number.isInteger(effect.animationKey) && Number.isInteger(effect.stateFacing)
        ? catalog.getPhysicalPoseProgram(effect.bank, effect.animationKey,
          effect.stateFacing, effect.variantSelector) : null;
    }

    function syncSpriteEffectPayload(effect) {
      var payload = effect.payload;
      var program = programForSpriteEffect(effect);
      payload.bank = effect.bank;
      payload.animationKey = program ? program.animationKey : effect.animationKey;
      payload.nativeFacing = program ? program.facing : effect.stateFacing;
      payload.variantSelector = effect.variantSelector;
      payload.renderPassSelector = effect.renderPassSelector;
      payload.scale = effect.scale;
      payload.poseId = program ? program.poseId : poseId(
        effect.bank, effect.animationKey, effect.stateFacing);
      payload.nativeStateIndex = effect.stateIndex;
      payload.nativeProgramCursor = effect.programCursor;
      payload.nativeProgramOpcode = effect.currentOpcode;
      payload.nativeProgramDelay = effect.delay;
      payload.displayedFrameToken = effect.displayedFrameToken;
      payload.stageX = Number.isFinite(effect.positionX) ? 160 + effect.positionX : null;
      payload.stageY = Number.isFinite(effect.positionY) ? 120 + effect.positionY : null;
      payload.rotationDegrees = Number.isFinite(effect.rotationDegrees)
        ? effect.rotationDegrees : null;
      payload.nativeRotationValue = Number.isInteger(effect.rotationValue)
        ? effect.rotationValue : null;
      payload.rotationEvidence = effect.rotationEvidence;
      payload.colorBlock = effect.colorBlock.slice();
      payload.materialBlock = effect.materialBlock.slice();
      payload.poseFrame = effect.poseFrame;
    }

    function resetSpriteEffectProgram(effect) {
      var program = catalog && catalog.getPhysicalPoseProgram
        ? catalog.getPhysicalPoseProgram(effect.bank, effect.animationKey,
          effect.stateFacing, effect.variantSelector) : null;
      effect.stateIndex = program ? program.stateIndex : -1;
      effect.programCursor = -1;
      effect.delay = 0;
      effect.currentOpcode = 0;
      effect.displayedFrameToken = 0;
      effect.poseFrame = 0;
      effect.colorBlock = Array(16).fill(0xFF);
      effect.materialBlock = Array(16).fill(0);
      if (!program) {
        missing('Animated scene sprite slot ' + effect.slot + ' cannot resolve Actor Art Source ' +
          effect.bank + ', Animation ' + effect.animationKey + ', Facing ' +
          effect.stateFacing + '.');
      }
    }

    function updateSpriteEffectProgram(effect) {
      if (!effect || effect.nativeProgramInterpreter !== true) return;
      effect.poseFrame += 2;
      if (effect.delay > 0) {
        effect.delay -= 2;
        syncSpriteEffectPayload(effect);
        return;
      }
      var guard = 0;
      while (effect.delay <= 0) {
        if (++guard > 1024) {
          missing('Animated scene sprite slot ' + effect.slot +
            ' exceeded the native instantaneous-program guard.');
          effect.currentOpcode = 0;
          effect.delay = 2;
          break;
        }
        effect.programCursor += 1;
        var program = programForSpriteEffect(effect);
        var records = program && Array.isArray(program.records) ? program.records : [];
        var record = records[effect.programCursor] || { opcode: 0, operands: [] };
        var opcode = record.opcode & 0xFF;
        var operands = record.operands || [];
        effect.currentOpcode = opcode;
        if (opcode === 0x00) {
          effect.delay = 2;
        } else if (opcode === 0x01) {
          effect.displayedFrameToken = operands[0] || 0;
          effect.delay = operands[1] || 0;
        } else if (opcode === 0x02) {
          effect.positionX -= lowS8(operands[0] || 0);
          effect.positionY -= lowS8(operands[1] || 0);
        } else if (opcode === 0x03) {
          effect.delay = operands[0] || 0;
        } else if (opcode === 0x04) {
          effect.programCursor = (operands[0] || 0) - 1;
        } else if (opcode === 0x05) {
          effect.stateIndex = operands[0] || 0;
          effect.programCursor = -1;
        } else if (opcode === 0x0D || opcode === 0x10) {
          var blockBytes = opcode === 0x0D ? effect.colorBlock : effect.materialBlock;
          var blockIndex = operands[0] || 0;
          var blockValue = operands[1] || 0;
          if (blockIndex === 0xFF) blockBytes.fill(blockValue);
          else if (blockIndex < blockBytes.length) blockBytes[blockIndex] = blockValue;
        } else if (opcode === 0x15) {
          effect.displayedFrameToken = (operands[0] || 0) | ((operands[1] || 0) << 8);
          effect.delay = operands[2] || 0;
        }
      }
      if (effect.delay > 0) effect.delay -= 2;
      syncSpriteEffectPayload(effect);
    }

    function newSpriteEffect(node, slot, payload, fields) {
      var effect = {
        id: 'runtime-effect:' + node.id,
        kind: 'effect',
        trackId: 'track:runtime:effect',
        label: 'Native Cutscene sprite effect',
        startFrame: state.tick,
        durationFrames: 1,
        capability: fields.capability || M.capabilities.PREVIEW_ONLY,
        payload: payload,
        slot: slot,
        bank: fields.bank,
        animationKey: fields.animationKey,
        stateFacing: fields.stateFacing,
        variantSelector: fields.variantSelector,
        positionX: fields.positionX,
        positionY: fields.positionY,
        rotationValue: fields.rotationValue,
        rotationDegrees: fields.rotationDegrees,
        rotationEvidence: fields.rotationEvidence,
        renderPassSelector: fields.renderPassSelector,
        scale: fields.scale,
        nativeProgramInterpreter: true,
        poseFrame: 0
      };
      resetSpriteEffectProgram(effect);
      state.spriteEffects[slot] = effect;
      updateSpriteEffectProgram(effect);
      return effect;
    }

    function executeSpriteEffect(node, words) {
      var row = rowFor(node, 'effect');
      var payload = row && row.clip.payload || null;
      var slot = payload && Number.isInteger(payload.nativeEffectSlot)
        ? payload.nativeEffectSlot : unsigned(words[1]) & 0xFF;
      var runtimePayload = payload ? M.cloneJson(payload, 'effect.payload') : {
          sourceSystem: 'cutscene-sprite-native',
          nativeOpcode: '0x46',
          nativeEffectSlot: slot,
          bank: signed(words[2]),
          animationKey: signed(words[3]),
          nativeFacing: signed(words[4]),
          variantSelector: unsigned(words[9]) & 0xFF,
          renderPassSelector: unsigned(words[7]) & 0xFF,
          stageX: 160 + signed(words[5]),
          stageY: 120 - signed(words[6]),
          scale: signed(words[8]) / 100,
          poseId: poseId(signed(words[2]), signed(words[3]), signed(words[4]))
        };
      newSpriteEffect(node, slot, runtimePayload, {
        bank: signed(words[2]),
        animationKey: lowS16(words[3]),
        stateFacing: signed(words[4]),
        variantSelector: unsigned(words[9]) & 0xFF,
        positionX: signed(words[5]),
        positionY: -signed(words[6]),
        rotationValue: 180,
        rotationDegrees: 0,
        rotationEvidence: 'native-constructor-identity',
        renderPassSelector: unsigned(words[7]) & 0xFF,
        scale: signed(words[8]) / 100,
        capability: row ? row.clip.capability : M.capabilities.PREVIEW_ONLY
      });
    }

    function executeAnimatedSceneSprite(node, words) {
      var slot = unsigned(words[1]) & 0xFF;
      var creationSentinel = signed(words[10]);
      var effect = state.spriteEffects[slot];
      if (creationSentinel === -1) {
        effect = newSpriteEffect(node, slot, {
          sourceSystem: 'cutscene-sprite-native',
          nativeOpcode: '0x63',
          nativeEffectSlot: slot
        }, {
          bank: signed(words[2]),
          animationKey: signed(words[3]),
          stateFacing: 0,
          variantSelector: unsigned(words[4]) & 0xFF,
          positionX: signed(words[5]),
          positionY: -signed(words[6]),
          rotationValue: 180,
          rotationDegrees: 0,
          rotationEvidence: 'native-constructor-identity-before-rotation-route',
          renderPassSelector: 1,
          scale: 1
        });
      } else if (!effect) {
        missing('Animated scene sprite opcode 0x63 reuses absent slot ' + slot + '.');
        return;
      }
      if (signed(words[9]) === -1) {
        effect.rotationValue = lowS16(words[7]);
        effect.rotationDegrees = effect.rotationValue === 180
          ? 0 : effect.rotationValue + 180;
        effect.rotationEvidence = 'native-direct-rotation-operand';
        effect.payload.nativeRotationRoute = 'direct-operand';
        effect.payload.nativeRotationPathId = null;
        effect.payload.nativeRotationPathGroup = null;
        effect.payload.nativeRotationPathEntry = null;
      } else if (signed(words[8]) === -1) {
        effect.rotationValue = null;
        effect.rotationDegrees = null;
        effect.rotationEvidence = 'sampled-scene-path-heading-unresolved';
        effect.payload.nativeRotationRoute = 'sampled-scene-path';
        effect.payload.nativeRotationPathId = null;
        effect.payload.nativeRotationPathGroup = null;
        effect.payload.nativeRotationPathEntry = signed(words[9]);
        missing('Animated scene sprite slot ' + slot +
          ' uses a launch-built sampled scene path; its path record is not available.');
      } else {
        var pathGroup = signed(words[8]);
        var pathEntry = signed(words[9]);
        var resourcePath = catalog && catalog.getSceneResourcePath
          ? catalog.getSceneResourcePath(pathGroup, pathEntry) : null;
        effect.payload.nativeRotationRoute = 'resource-path';
        effect.payload.nativeRotationPathGroup = pathGroup;
        effect.payload.nativeRotationPathEntry = pathEntry;
        effect.payload.nativeRotationPathId = resourcePath && resourcePath.pathId || null;
        if (resourcePath && resourcePath.status === 'native-static-path-heading' &&
            Number.isInteger(resourcePath.nativeStoredHeading) &&
            Number.isInteger(resourcePath.rotationDegrees)) {
          effect.rotationValue = resourcePath.nativeStoredHeading;
          effect.rotationDegrees = resourcePath.rotationDegrees;
          effect.rotationEvidence = 'native-resource-path-spline-start-heading';
        } else {
          effect.rotationValue = null;
          effect.rotationDegrees = null;
          effect.rotationEvidence = resourcePath
            ? 'native-resource-path-empty-entry'
            : 'native-resource-path-selection-invalid';
          missing('Animated scene sprite slot ' + slot + ' selects unavailable native resource ' +
            'path ' + pathGroup + ':' + pathEntry + '.');
        }
      }
      syncSpriteEffectPayload(effect);
    }

    function executeAnimatedSceneSpriteRestart(node, words) {
      var slot = unsigned(words[1]) & 0xFF;
      var effect = state.spriteEffects[slot];
      if (!effect) {
        missing('Animated scene sprite opcode 0x64 retargets absent slot ' + slot + '.');
        return;
      }
      if (signed(words[2]) !== -1) effect.bank = signed(words[2]);
      var animationKey = lowS16(words[3]);
      if (animationKey !== -1) effect.animationKey = animationKey;
      effect.stateFacing = 0;
      effect.variantSelector = unsigned(words[4]) & 0xFF;
      effect.payload.nativeOpcode = '0x64';
      effect.payload.nativeRestartNodeId = node.id;
      resetSpriteEffectProgram(effect);
      updateSpriteEffectProgram(effect);
    }

    function executeBodyPose(node, words) {
      var slot = signed(words[1]);
      var actor = actorForCommand(slot, 'Body-pose program');
      if (!actor) return;
      if(nativeLaunch&&nativeLaunch.input.sceneMode===2){
        try{var computed=OB64.cutsceneRomStart.bodyPose(nativeLaunch,words,Object.keys(state.actors).map(function(slot){var record=nativeRecordForActor(state.actors[slot]);if(!record)fail('Missing fresh Actor record '+slot,'rom-start-record');return {slot:Number(slot),bytes:new Uint8Array(record.buffer)};}));applyNativeRecord(actor,new DataView(computed.buffer),'computed-ROM-body-initializer');updateActorPose(actor);return;}
        catch(error){actorBoundary(error.message,error.code||'rom-start-body');return;}
      }
      var setup=actorService('bodyPoseSetups',node);
      if (setup) {
        if (!setup.words.every(function(v,i){return unsigned(v)===unsigned(words[i]);})) {
          actorBoundary('Body-pose setup does not match this exact command occurrence.','alternate-pose-setup'); return;
        }
        var prepared=launchBytes(setup.recordHex,0x150);
        if (prepared.getInt32(0xE4)!==slot || prepared.getUint8(0x13D)!==1 || prepared.getInt32(0xF0)!==-1 ||
            prepared.getInt32(0xF4)!==0 || prepared.getInt32(0xF8)!==0 ||
            [0x11C,0x120,0x124].some(function(at){return !Number.isFinite(prepared.getFloat32(at));}) ||
            Array.from({length:16},function(_,i){return prepared.getUint8(i)!==255 || prepared.getUint8(i+16)!==0;}).some(Boolean)) {
          actorBoundary('Body-pose setup lacks the successful initializer seed.','alternate-pose-setup'); return;
        }
        applyNativeRecord(actor,prepared,'qualified-post-preparation-before-immediate-pose');
        actor.source=Object.assign({},actor.source,{launchSourceIdentity:launchInputs.sourceIdentity,
          invocationId:launchInputs.invocationId,evidenceGrade:launchInputs.evidenceGrade});
        updateActorPose(actor);
        return;
      }
      var previous = actor.bodyPoseProgram || {};
      var artSource = signed(words[2]);
      var selector = signed(words[3]);
      var flagB = signed(words[4]);
      var flagA = signed(words[5]);
      var ownerContext = signed(words[6]);
      artSource = artSource === -1 ? actor.bank : artSource;
      selector = selector === -1 ? actor.animationKey : selector;
      flagB = flagB === -1
        ? (Number.isInteger(previous.flagB) ? previous.flagB : 0) : flagB;
      flagA = flagA === -1
        ? (Number.isInteger(previous.flagA) ? previous.flagA : actor.variantSelector) : flagA;
      ownerContext = ownerContext === -1
        ? (Number.isInteger(previous.ownerContext) ? previous.ownerContext : artSource)
        : ownerContext;
      actor.bank = artSource;
      actor.artSourceId = 'combat-actor-art-source:' + artSource;
      actor.animationKey = selector;
      actor.variantSelector = flagA & 0xFF;
      actor.poseId = 'body-pose:' + artSource + ':' + selector + ':' +
        flagA + ':' + flagB + ':' + ownerContext;
      actor.poseFrame = 0;
      actor.poseLoop = false;
      actor.poseDuration = 0;
      actor.poseReadyTick = state.tick;
      actor.bodyPoseProgram = {
        decoder: 'alternate-body-pose',
        artSource: artSource,
        selector: selector,
        flagB: flagB,
        flagA: flagA,
        ownerContext: ownerContext,
        displayedFrameToken: 0,
        initialization: 'native-cleared-frame-state'
      };
      actor.decoderMode = 1;
      actor.poseBlocked = 'alternate-pose-setup';
      actorBoundary('Body-pose playback requires qualified initializer and resource-registration inputs.', 'alternate-pose-setup');
      delete state.bodyPoseJobs[slot];
    }

    function swapMovementActivity(actor, destination, vacatedSlot) {
      // Presentation activity follows slot ownership, including imported context activity.
      var id = actor.activeMovementId, frame = actor.movementFrame;
      function assign(occupant, inheritedId, inheritedFrame) {
        var job = state.movementJobs[occupant.slot];
        occupant.activeMovementId = job ? (job.nodeId === undefined
          ? 'launch-movement:' + occupant.slot : 'runtime-movement:' + job.nodeId) : inheritedId;
        occupant.movementFrame = job ? job.elapsed : inheritedFrame;
      }
      var inherited = destination ? { id: destination.activeMovementId, frame: destination.movementFrame }
        : vacantMovementActivity[actor.slot] || { id: null, frame: 0 };
      assign(actor, inherited.id, inherited.frame);
      if (destination) assign(destination, id, frame);
      else vacantMovementActivity[vacatedSlot] = { id: id, frame: frame };
      delete vacantMovementActivity[actor.slot];
      if (destination) delete vacantMovementActivity[destination.slot];
    }

    function finalizeOrdinarySlots(node) {
      if (!initialActors) {actorBoundary('Roster finalization requires known complete slot occupancy.','roster-slot-input');return;}
      for (var slot=0;slot<28;slot++) {
        // Read the current slot after every earlier swap. Actor identities may repeat or be skipped.
        var actor=state.actors[slot];
        if (!actor) continue;
        if (!Number.isInteger(actor.sourceRowOrdinal)||actor.sourceRowOrdinal<0||actor.sourceRowOrdinal>=20) {
          actorBoundary('Roster finalization needs an ordinary source row for each occupied slot.','roster-finalizer-row');return;
        }
        var row=launchBytes(actorInputRows[actor.sourceRowOrdinal],0xF8),flags=row.getUint32(0x40);
        recordTrace({tick:state.tick,kind:'roster-finalizer-visit',nodeId:node.id,slot:slot,actorIdentity:actor.id});
        if ([10,25,123].includes(row.getUint32(0x4C))&&lowS16(actor.animationKey)>=50 || !(flags&512)) continue;
        var target=(flags&256)?1:0;
        if (slot===target) continue;
        var destination=state.actors[target];
        state.actors[target]=actor;actor.slot=target;
        if (destination) {state.actors[slot]=destination;destination.slot=slot;} else delete state.actors[slot];
        swapMovementActivity(actor, destination, slot);
        recordTrace({tick:state.tick,kind:'roster-finalizer-swap',nodeId:node.id,slot:slot,target:target,actorIdentity:actor.id});
      }
    }

    function executeOrdinaryRoster(node,words) {
      if (!actorInputRows) {actorBoundary('Actor-roster materializer requires all 20 caller rows.');return;}
      var construction=launchValue('rosterConstruction'), first=signed(words[1]),second=signed(words[2]),variant=0;
      var callerControl=(unsigned(words[0])&0x7FFFFFFF)===0xAB?1:0;
      var stateService=null,serviceRequested=false,appearanceIndex=0,preparationIndex=0,poseIndex=0,lookupIndex=0,setupCalls=0;
      function trace(kind,detail){recordTrace(Object.assign({tick:state.tick,nodeId:node.id,kind:kind},detail));}
      function need(condition,message,code){if(!condition)fail(message,code);}
      function services(){if(!serviceRequested){stateService=actorService('rosterStateServices',node);serviceRequested=true;}return stateService;}
      function effects(effect){
        if(!effect)return;
        var actors=effect.actors||[],rows=effect.rows||[];
        actors.forEach(function(change){var actor=state.actors[change.slot],before=actor&&nativeRecordForActor(actor),after=launchBytes(change.afterRecordHex,336);
          need(before&&recordHex(before).toLowerCase()===change.beforeRecordHex.toLowerCase()&&after.getInt32(0xE4)===change.slot&&[0x11C,0x120,0x124].every(function(at){return Number.isFinite(after.getFloat32(at));}),'State service Actor effects require exact current records and stable slot identity.','roster-state-effects');});
        rows.forEach(function(change){need(actorInputRows[change.ordinal].toLowerCase()===change.beforeHex.toLowerCase(),'State service row effects require the exact current row.','roster-state-effects');});
        actors.forEach(function(change){applyNativeRecord(state.actors[change.slot],launchBytes(change.afterRecordHex,336),'qualified-State-service-effect');});
        rows.forEach(function(change){actorInputRows[change.ordinal]=change.afterHex;});
      }
      function classify(actor){
        if(actor.sourceRowOrdinal===255)return 0;
        need(Number.isInteger(actor.sourceRowOrdinal)&&actor.sourceRowOrdinal>=0&&actor.sourceRowOrdinal<20,'State setup requires a valid ordinary source row.','roster-state-row');
        var row=launchBytes(actorInputRows[actor.sourceRowOrdinal],248);
        return row.getUint32(0x48)&&[10,25,123].includes(row.getUint32(0x4C))?(actor.linkedOrdinal===1?2:1):0;
      }
      function setup(slot,args,depth){
        trace('roster-setup-entry',{slot:slot,args:args.slice()});
        if(slot===-1||!state.actors[slot])return;
        need(depth<28&&++setupCalls<=256,'State propagation exceeded its bounded recursion/call prerequisites.','roster-state-recursion');
        var actor=state.actors[slot],raw=nativeRecordForActor(actor);
        need(raw,'State setup needs a complete current Actor record.','roster-state-record');
        var art=args[0]===-1?raw.getInt32(0xE8):args[0],context=args[1]===-1?raw.getInt32(0xEC):args[1];
        var requested=args[2]===-1?raw.getInt16(0x138):args[2],flagB=args[3]===-1?raw.getInt16(0x13A):args[3];
        if(classify(actor)===2){if(requested>=50){trace('roster-setup-early-return',{slot:slot});return;}requested=signed(requested+50);}
        raw.setUint32(0xE4,slot&255);if(art!==-1)raw.setInt32(0xE8,art);if(requested!==-1)raw.setUint16(0x138,requested);
        if(context!==-1)raw.setInt32(0xEC,context);raw.setUint8(0x13D,1);raw.setUint16(0x134,requested);
        raw.setInt32(0xF0,-1);raw.setInt32(0xF4,0);raw.setInt32(0xF8,0);raw.setInt32(0x12C,0);raw.setUint32(0xE0,0x8022F2CC);
        raw.setInt32(0x128,0);raw.setUint16(0x13A,flagB);raw.setFloat32(0x130,1);raw.setUint8(0x13C,construction.presentationByte);raw.setUint8(0x145,1);
        for(var i=0;i<16;i++){raw.setUint8(i,255);raw.setUint8(i+16,0);}
        if(args[4]!==-1)raw.setUint8(0x146,args[4]);var flagA=raw.getUint8(0x146);
        applyNativeRecord(actor,raw,'ordinary-State-seed-before-services');
        need(actor.sourceRowOrdinal<20,'Existing-row State setup cannot index the source-row sentinel.','roster-state-row');
        var row=launchBytes(actorInputRows[actor.sourceRowOrdinal],248),halfwords=[0x36,0x38,0x3A,0x3C].map(function(at){return row.getUint16(at);});
        var service=services(),appearance=service&&service.appearances[appearanceIndex++];
        need(appearance&&appearance.slot===slot&&JSON.stringify(appearance.halfwords)===JSON.stringify(halfwords),'State setup requires the exact appearance-selector response.','roster-state-appearance');
        effects(appearance.effects);need(appearance.status==='returned','Appearance selection did not return after prior State effects.','roster-state-appearance');
        var values=[art>>>0,context>>>0,flagB>>>0,flagA,appearance.response&65535],preparation=service.preparations[preparationIndex++];
        need(preparation&&preparation.slot===slot&&JSON.stringify(preparation.values)===JSON.stringify(values),'State setup requires qualified preparation for its exact local inputs.','roster-state-preparation');
        trace('roster-state-preparation',{slot:slot,values:values,localValues:preparation.localValues||values,status:preparation.status});
        effects(preparation.effects);need(preparation.status==='returned','State preparation did not return; seeded Actor effects remain.','roster-state-preparation');
        // Preparation changes its local words, not the already seeded Actor or recursive arguments.
        updateActorPose(actor);if(actor.poseBlocked)fail('Immediate roster pose requires '+actor.poseBlocked+'.',actor.poseBlocked);
        var call=poseIndex++,poseEffect=(service.poseEffects||[]).find(function(effect){return effect.call===call;});
        if(poseEffect){need(poseEffect.slot===slot&&recordHex(nativeRecordForActor(actor)).toLowerCase()===poseEffect.beforeRecordHex.toLowerCase(),'Post-pose effects require the exact completed pose record.','roster-state-effects');effects(poseEffect.effects);}
        trace('roster-immediate-pose',{slot:slot,call:call,recordHex:recordHex(nativeRecordForActor(actor))});
        if(classify(actor)!==1)return;
        for(var candidateSlot=0;candidateSlot<28;candidateSlot++){
          var candidate=state.actors[candidateSlot];
          if(candidate&&candidate.sourceRowOrdinal!==255&&candidate.sourceRowOrdinal===actor.sourceRowOrdinal&&classify(candidate)===2)setup(candidateSlot,args,depth+1);
        }
      }
      function marker(actor){
        actor.poseDelay=0;
        for(var lookups=0;lookups<256;lookups++){
          actor.poseCursor=signed(actor.poseCursor+1);
          var service=services(),supplied=service&&service.markerLookups,opcode;
          if(supplied){var response=supplied[lookupIndex++],request={slot:actor.slot,decoderMode:actor.decoderMode,cursor:actor.poseCursor,state:lowS16(actor.poseStateIndex),art:actor.bank,context:actor.nativeOwnerContext,flagA:actor.variantSelector,flagB:actor.nativeFlagB};
            need(response&&Object.keys(request).every(function(key){return response[key]===request[key];}),'Marker lookup requires its exact current Actor arguments.','roster-marker-input');opcode=response.opcode;effects(response.effects);
          }else{var program=actor.decoderMode!==0?qualifiedPoseForActor(actor,'alternate'):(qualifiedPoseForActor(actor,'ordinary')||programForActor(actor));
            need(program&&Array.isArray(program.records),'Marker scanning requires a qualified current pose program.','roster-marker-input');opcode=(program.records[actor.poseCursor]||{opcode:0}).opcode;}
          trace('roster-marker-lookup',{slot:actor.slot,cursor:actor.poseCursor,opcode:opcode});
          if((opcode&255)===4){actor.poseCursor=signed(actor.poseCursor-2);return;}
        }
        fail('Marker scanning did not reach opcode four within the qualified lookup bound.','roster-marker-termination');
      }
      try {
      for(var ordinal=0;ordinal<20;ordinal++) {
        var row=launchBytes(actorInputRows[ordinal],0xF8),art=row.getInt32(0x48),context=row.getInt32(0x4C),flags=row.getUint32(0x40);
        if (!art) continue;
        var side=!!(flags&256),enabled=!!(flags&512),selected=false;
        if (first===-1&&side) selected=true;
        else if(second===-1&&!side) selected=true;
        else if(first===-2&&side&&enabled || second===-2&&!side&&enabled) selected=true;
        else if(first===0&&side || second===0&&!side) selected=false;
        else if(nativeClassFamilyMatch(first,art)) {first=0;selected=true;}
        else if(nativeClassFamilyMatch(second,art)) {second=0;selected=true;}
        if (!selected) continue;
        if (!construction) {actorBoundary('Selected roster rows require qualified route, linked objects, terrain, and allocation inputs.','roster-construction-input');return;}
        if ([-3,-10].includes(construction.route)) {
          var excluded=construction.excludedRows&&construction.excludedRows[ordinal];
          if(typeof excluded!=='boolean') {actorBoundary('This roster route requires its exact exclusion predicate result.','roster-exclusion-input');return;}
          if(excluded) continue;
        }
        var pointers=[0,4,8].map(function(at){return row.getUint32(at);});
        var type=context===1?1:[10,25,123].includes(context)?2:art===135?(variant?4:0):[136,161].includes(art)?2:0;
        var free=Array.from({length:28},function(_,i){return i;}).filter(function(i){return !state.actors[i];});
        if(!initialActors) {actorBoundary('Roster construction requires known slot occupancy.','roster-slot-input');return;}
        if(free.length<pointers.filter(Boolean).length) {actorBoundary('Native roster allocation has no safe exhausted-capacity return.','roster-capacity');return;}
        var links=pointers.map(function(pointer){return pointer?construction.links.find(function(link){return link.pointer===pointer;}):null;});
        for(var check=0;check<3;check++) if(pointers[check]) {
          var link=links[check];
          if(!link||!link.allocationSucceeded||type!==4&&(link.terrainHeight===null||link.terrainHeight<-2147483648||link.terrainHeight>=2147483648)) {
            actorBoundary('Roster construction requires valid linked coordinates, successful allocation, and convertible terrain results.','roster-linked-input');return;
          }
        }
        var outputs=[-1,-1,-1,-1],orientation=row.getInt32(0x58)<4?1:0;
        for(var linked=0;linked<3;linked++) if(pointers[linked]) {
          var slot=free.shift(),raw=new DataView(new ArrayBuffer(0x150)),selector=type===4||type===2&&linked===1?60:10;
          for(var material=0;material<16;material++)raw.setUint8(material,255);
          raw.setUint32(0xE0,0x8022F2CC);raw.setInt32(0xE4,slot);
          if(art!==-1)raw.setInt32(0xE8,art);if(context!==-1)raw.setInt32(0xEC,context);
          raw.setInt32(0xF0,-1);
          [0x104,0x108,0x10C,0x130].forEach(function(at){raw.setFloat32(at,1);});
          raw.setFloat32(0x11C,links[linked].x);raw.setFloat32(0x124,links[linked].z);
          raw.setFloat32(0x120,type===4?links[linked].y:lowS16(Math.trunc(links[linked].terrainHeight)));
          raw.setInt16(0x134,selector);raw.setInt16(0x138,selector);raw.setInt16(0x13A,orientation);
          raw.setUint8(0x13C,construction.presentationByte);raw.setUint8(0x13D,1);raw.setUint8(0x145,1);
          [0x142,0x143,0x144].forEach(function(at){raw.setUint8(at,255);});
          raw.setUint8(0x146,orientation);raw.setUint8(0x147,ordinal);raw.setUint8(0x149,linked);
          var actor=ensureActor(slot);actor.id='roster:'+launchInputs.invocationId+':'+(++subordinateSerial);
          actor.visible=true;actor.source={launchSourceIdentity:launchInputs.sourceIdentity,invocationId:launchInputs.invocationId,evidenceGrade:launchInputs.evidenceGrade,linkedPointer:pointers[linked]};
          applyNativeRecord(actor,raw,'ordinary-constructor-before-State-setup');outputs[linked]=slot;
          recordTrace({tick:state.tick,kind:'roster-construction',nodeId:node.id,slot:slot,linkedOrdinal:linked,sourceRow:ordinal,actorIdentity:actor.id,recordHex:recordHex(raw)});
        }
        if(art===135)variant=1;
        if(outputs[0]!==-1){
          need(Number.isInteger(construction.sceneKey),'Materializer State selection requires the signed native scene key.','roster-state-selection');
          var adjusted=[985,543,198,245,484,485].includes(construction.sceneKey)?1-orientation:orientation;
          for(var output=0;outputs[output]!==-1;output++){
            var actor=state.actors[outputs[output]],currentRow=launchBytes(actorInputRows[ordinal],248),currentFlags=currentRow.getUint32(0x40),request=-1,scan=false;
            if((currentFlags&2)&&(currentFlags&0x300)!==0x300){request=lowS16(actor.animationKey)+28;scan=true;}
            else if([-9,-6].includes(construction.route)&&adjusted===1){
              if(callerControl===0){request=lowS16(actor.animationKey)+26;scan=true;}
            }
            setup(outputs[output],[-1,-1,request,-1,-1,0,1],0);if(scan)marker(actor);
          }
        }
      }
      finalizeOrdinarySlots(node);
      }catch(error){if(!(error instanceof RuntimeError))throw error;actorBoundary(error.message,error.code);}
    }

    function executeRosterReset(node) {
      if(state.directorMode!==2)return;
      if(!actorInputRows){actorBoundary('Roster reset requires all 20 current rows.','roster-reset-input');return;}
      var service=actorService('rosterResets',node);
      if(!service){actorBoundary('Roster reset requires qualified unit, registry, child, and native-service inputs.','roster-reset-input');return;}
      service=M.cloneJson(service,'reset service');
      if(nativeRosterResult && (service.currentUnit!==nativeRosterResult.currentUnit ||
          JSON.stringify(service.primaryRegistry)!==JSON.stringify(nativeRosterResult.primaryRegistry) ||
          JSON.stringify(service.secondaryRegistry)!==JSON.stringify(nativeRosterResult.secondaryRegistry) ||
          Object.keys(service.objects).length!==Object.keys(nativeRosterResult.objects).length ||
          Object.keys(nativeRosterResult.objects).some(function(p){return !service.objects[p]||service.objects[p].toLowerCase()!==nativeRosterResult.objects[p].toLowerCase();}))) {
        actorBoundary('A later reset must preserve the previously established registry, object, and selector state.','roster-reset-state');return;
      }
      var releaseIndex=0,finalizerIndex=0,prepareIndex=0,decodeIndex=0,randomIndex=0;
      var rows=actorInputRows.map(function(hex){return launchBytes(hex,0xF8);});
      nativeRosterResult={currentUnit:service.currentUnit,objects:service.objects,primaryRegistry:service.primaryRegistry,secondaryRegistry:service.secondaryRegistry,scratch:service.scratch,rows:actorInputRows,completed:false};
      function need(condition,message,code){if(!condition)fail(message,code||'roster-reset-service');}
      function publish(){actorInputRows=rows.map(recordHex);nativeRosterResult.rows=actorInputRows;}
      function object(pointer){need(pointer&&service.objects[pointer],'Reset child backing is unavailable.','roster-reset-child');return launchBytes(service.objects[pointer],0x100);}
      function store(pointer,bytes){service.objects[pointer]=recordHex(bytes);}
      function remove(registry,pointer){var index=registry.indexOf(pointer);if(index>=0)registry.splice(index,1);}
      function trace(kind,detail){recordTrace(Object.assign({tick:state.tick,nodeId:node.id,kind:kind},detail));}
      function advanceChild(pointer,row,extra,rowOrdinal){
        var child=object(pointer),base=0x44,steps=0;
        // Child presentation state uses halfword counters, unlike the Director Actor's word counters.
        function get(at){return child.getInt16(base+at);}
        function put(at,v){child.setInt16(base+at,lowS16(v));}
        function byte(at,v){child.setUint8(base+at,v&255);}
        function arg(at){return child.getUint8(base+at);}
        function random(){need(randomIndex<service.random.length,'Initial child pose requires an ordered random result.','roster-reset-random');var v=service.random[randomIndex++];need(Number.isInteger(v)&&v>=0&&v<=4294967295,'Invalid random result.');return v;}
        try {
          while(get(8)<=0) {
            need(++steps<=256,'Initial child pose exceeded its bounded instruction budget.','roster-reset-pose-limit');
            put(6,get(6)+1);store(pointer,child);
            var alternate=row.getInt32(0x48)===256||extra;
            if(!alternate){var linkedPrimary=object(child.getUint32(base+0x4C));need(linkedPrimary.getUint32(0x40)===service.sceneRoot+0x1C4+rowOrdinal*248,'Ordinary child decoding requires the qualified parent-row link.','roster-reset-child');}
            var request={pointer:pointer,art:row.getInt32(0x48),context:row.getInt32(0x4C),flag8:extra?0:(row.getUint32(0x40)>>>8)&1,flag10:extra?0:(row.getUint32(0x40)>>>10)&1,selection:get(4),cursor:get(6)};
            var decode=service.decodes[decodeIndex++];
            need(decode&&Object.keys(request).every(function(k){return decode[k]===request[k];})&&Array.isArray(decode.bytes)&&decode.bytes.length===3&&decode.bytes.every(function(v){return Number.isInteger(v)&&v>=0&&v<=255;})&&Number.isInteger(decode.code),'Initial child decoder result is unavailable for its exact call.','roster-reset-decoder');
            var a=decode.bytes[0],b=decode.bytes[1],c=decode.bytes[2],depth;
            trace('reset-child-decode',Object.assign({code:decode.code,bytes:decode.bytes,currentUnit:nativeRosterResult.currentUnit},request));
            switch(decode.code) {
              case 0:
                depth=arg(0x46);need(depth<=4,'Invalid child return depth.','roster-reset-pose-stack');
                if(depth){depth--;byte(0x46,depth);put(4,arg(0x32+depth));put(6,arg(0x36+depth));break;}
                var flags=arg(0x48),selection=0;
                if(flags&2)selection=((((random()<<18)&0x0C000000)|(random()<<15)|random())>>>0)%5;
                if(flags&1)selection+=50;
                put(4,selection);put(6,-1);put(8,0);[0xC,0xE,0x10].forEach(function(at){put(at,0);});byte(0x46,0);byte(0x47,0);
                for(var i=0;i<16;i++){byte(0x12+i,255);byte(0x22+i,0);}break;
              case 1:put(0xA,a);put(8,b);break;
              case 2:put(0x10,get(0x10)-(a<<24>>24));put(0xE,get(0xE)-(b<<24>>24));break;
              case 3:put(8,a);break;
              case 4:put(6,a-1);break;
              case 15:
                depth=arg(0x46);need(depth<4,'Child return stack capacity is unavailable.','roster-reset-pose-stack');
                byte(0x32+depth,get(4));byte(0x36+depth,get(6));byte(0x46,depth+1);
                // Native call-control falls through to state selection.
              case 5:put(4,a);put(6,-1);break;
              case 12:put(0x10,get(0x10)-(a<<24>>24));put(0xE,get(0xE)+(b<<24>>24));put(0xC,get(0xC)+(c<<24>>24));break;
              case 13:case 16:
                need(a===255||a<16,'Child material index exceeds backed storage.','roster-reset-pose-material');
                var start=decode.code===13?0x12:0x22;
                if(a===255)for(var j=0;j<16;j++)byte(start+j,b);else byte(start+a,b);break;
              case 14:
                depth=arg(0x47);need(depth<=4,'Invalid child loop depth.','roster-reset-pose-stack');
                if(depth&&arg(0x3A+depth-1)===get(4)&&arg(0x3E+depth-1)===get(6)) {
                  var count=(arg(0x42+depth-1)-1)&255;byte(0x42+depth-1,count);
                  if(!count){byte(0x47,depth-1);break;}
                } else {if(!b)break;need(depth<4,'Child loop stack capacity is unavailable.','roster-reset-pose-stack');byte(0x3A+depth,get(4));byte(0x3E+depth,get(6));byte(0x42+depth,b);byte(0x47,depth+1);}
                put(6,a-1);break;
              case 17:case 18:case 19:case 20:fail('Initial child pose requires its external sound-request producer.','roster-reset-sound');break;
              case 21:put(0xA,a+(b<<8));put(8,c);break;
            }
          }
          for(var index=0;index<16;index++)byte(0x12+index,clamp(arg(0x12+index)+child.getInt8(base+0x22+index),0,255));
          put(8,get(8)-2);
        } finally {store(pointer,child);}
      }
      try {
        for(var ordinal=0;ordinal<20;ordinal++) {
          var row=rows[ordinal];if(!row.getUint32(0x48)||!(row.getUint32(0x40)&256))continue;
          var release=service.releases[releaseIndex++],handle=row.getUint32(0x50);
          need(release&&release.handle===handle&&release.status==='returned','Reset resource release is unavailable.','roster-reset-release');
          trace('reset-release',{row:ordinal,handle:handle});
          for(var linked=0;linked<3;linked++) {
            var primary=row.getUint32(linked*4);if(!primary)continue;
            var child=object(primary);child.setUint32(0x18,0);store(primary,child);
            var secondary=row.getUint32(12+linked*4),partner=object(secondary);partner.setUint32(0x18,0);store(secondary,partner);
            remove(service.primaryRegistry,primary);remove(service.secondaryRegistry,secondary);
            row.setUint32(linked*4,0);row.setUint32(12+linked*4,0);
          }
          rows[ordinal]=new DataView(new ArrayBuffer(0xF8));
        }
        need(service.unitHex!==null,'Deployed unit 30 is unavailable after selective cleanup.','roster-reset-unit');
        var unit=launchBytes(service.unitHex,25),sticky=0;
        for(var memberSlot=0;memberSlot<5;memberSlot++) {
          var member=unit.getUint8(2+memberSlot);if(!member)continue;
          var at=rows.findIndex(function(r){return !r.getUint32(0x48);});need(at>=0,'Preserved rows exhaust the reset row pool.','roster-reset-capacity');
          var old=rows[at],finalizer=service.rowFinalizers[finalizerIndex++];
          if(old.getUint32(0x40)&512) {need(finalizer&&Number.isInteger(finalizer.oldAppearanceClass),'Unused-row appearance classification is unavailable.','roster-reset-old-row');if((finalizer.oldAppearanceClass&255)===2)sticky=1;}
          var source=service.records[member<100?member:0];need(source,'Selected deployed record is unavailable.','roster-reset-record');source=launchBytes(source,52);
          row=new DataView(new ArrayBuffer(0xF8));rows[at]=row;
          [[0x11,0x48,4],[0x12,0x4C,4],[0x13,0x31,1],[0x1A,0x33,1],[0x28,0x30,1],[0x32,0x3E,1]].forEach(function(pair){if(pair[2]===4)row.setUint32(pair[1],source.getUint8(pair[0]));else row.setUint8(pair[1],source.getUint8(pair[0]));});
          [[0x16,0x22],[0x18,0x20],[0x1C,0x24],[0x1E,0x26],[0x20,0x28],[0x22,0x2A],[0x24,0x2C],[0x26,0x2E],[0x2A,0x36],[0x2C,0x38],[0x2E,0x3A],[0x30,0x3C]].forEach(function(pair){row.setUint16(pair[1],source.getUint16(pair[0]));});
          row.setUint8(0x34,source.getUint8(0x1B));row.setUint8(0x3F,source.getUint8(0x1B));row.setUint8(0xF6,member<100?member:0);
          if(row.getUint32(0x4C)===1)row.setUint8(0x3E,0);
          row.setUint32(0x40,0x500|((source.getUint8(0x33)&2)?512:0)|((source.getUint8(0x33)&4)?2:0));
          var formation=unit.getUint8(7+memberSlot);row.setUint32(0x54,formation%3);row.setUint32(0x58,Math.floor(formation/3));
          if(member>=100){var override=service.specialOverrides&&service.specialOverrides[member];need(override&&Number.isInteger(override.halfword)&&override.halfword>=0&&override.halfword<=65535&&Number.isInteger(override.flags)&&override.flags>=0&&override.flags<=255,'Special-member override input is unavailable.','roster-reset-special');row.setUint8(0xF6,member);row.setUint16(0x20,override.halfword);if(override.flags&4)row.setUint32(0x40,row.getUint32(0x40)|2);}
          var mode=member<100?0:sticky;
          trace('reset-row-initialized',{row:at,member:member,mode:mode,rowHex:recordHex(row)});
          need(finalizer&&finalizer.row===at&&finalizer.mode===mode&&finalizer.beforeRowHex.toLowerCase()===recordHex(row).toLowerCase(),'Reset row finalization needs exact qualified input identity.','roster-reset-finalizer');
          var prepared=launchBytes(finalizer.afterRowHex,0xF8);
          need(finalizer.objects&&typeof finalizer.objects==='object','Reset finalizer child effects are unavailable.','roster-reset-finalizer');
          Object.keys(finalizer.objects).forEach(function(pointer){need(/^\d+$/.test(pointer)&&Number(pointer)>0&&Number(pointer)<=4294967295,'Invalid finalizer child identity.');launchBytes(finalizer.objects[pointer],0x100);service.objects[pointer]=finalizer.objects[pointer];});
          rows[at]=prepared;row=prepared;
          need(finalizer.status==='returned','Reset row finalizer did not return; its qualified partial effects remain.','roster-reset-finalizer');
          for(var pair=0;pair<3;pair++){var p=row.getUint32(pair*4);if(!p)continue;object(p);var q=row.getUint32(12+pair*4);object(q);need(service.primaryRegistry.length<256&&service.secondaryRegistry.length<256,'Reset registry capacity input is insufficient.','roster-reset-registry');service.primaryRegistry.push(p);service.secondaryRegistry.push(q);}
        }
        var descriptions=[];
        for(var rowIndex=0;rowIndex<20;rowIndex++) {
          row=rows[rowIndex];if(!row.getUint32(0x48)||!(row.getUint32(0x40)&256))continue;
          var description=service.descriptions.find(function(d){return d.row===rowIndex;});
          need(description&&description.rowHex.toLowerCase()===recordHex(row).toLowerCase()&&Number.isInteger(description.variant)&&Number.isInteger(description.handle),'Reset description services require exact current-row identity.','roster-reset-description');
          var item={art:row.getUint32(0x48),context:row.getUint32(0x4C),variant:description.variant&65535,handle:description.handle>>>0,orientationA:1,orientationB:1};
          if(!descriptions.some(function(d){return d.art===item.art&&d.variant===item.variant;})) {var insert=descriptions.findIndex(function(d){return d.handle>item.handle;});if(insert<0)descriptions.push(item);else descriptions.splice(insert,0,item);}
        }
        need(descriptions.length<=5,'Reset descriptions exceed the accepted five-member domain.','roster-reset-descriptions');
        var scratch=service.scratch,n=descriptions.length;
        descriptions.forEach(function(d,i){Object.keys(scratch).forEach(function(k){scratch[k][i]=d[k];});});
        function word(k,i){need(i<9&&scratch[k][i]!==null&&scratch[k][i]!==undefined,'Reset grouping requires an exact residual scratch word.','roster-reset-scratch');return scratch[k][i];}
        for(var start=0;start<n;) {
          var run=1;while(run<n&&word('handle',start+run)===word('handle',start))run++;
          var args=['art','context','orientationB','orientationA','variant'].map(function(k){return Array.from({length:run},function(_,i){return word(k,start+i);});});
          var preparation=service.preparations[prepareIndex++];
          need(preparation&&preparation.count===run&&JSON.stringify(preparation.values)===JSON.stringify(args),'Grouped reset preparation requires the exact ordered slice and count.','roster-reset-preparation');
          if(preparation.variants){need(Array.isArray(preparation.variants)&&preparation.variants.length===run&&preparation.variants.every(function(v){return Number.isInteger(v)&&v>=0&&v<=4294967295;}),'Invalid preparation variant writes.');preparation.variants.forEach(function(v,i){scratch.variant[start+i]=v;});}
          trace('reset-preparation',{start:start,count:run,values:args,status:preparation.status});
          need(preparation.status==='returned','Reset preparation did not return; prior effects remain.','roster-reset-preparation');start+=run;
        }
        if(n)for(var poseRow=0;poseRow<20;poseRow++) {
          row=rows[poseRow];if(!row.getUint32(0x48)||!(row.getUint32(0x40)&256))continue;
          for(var offset of [0,4,8,0x18,0x1C]){var pointer=row.getUint32(offset);if(pointer)advanceChild(pointer,row,offset>=0x18,poseRow);}
        }
        nativeRosterResult.currentUnit=30;nativeRosterResult.completed=true;
        currentUnitMembers=Array.from({length:5},function(_,i){return unit.getUint8(2+i);});
        trace('reset-selector-published',{currentUnit:30});
      } catch(error) {if(!(error instanceof RuntimeError))throw error;actorBoundary(error.message,error.code);}
      finally {publish();}
    }

    function executeSubordinate(node, words) {
      var anchorSlot=lowS16(words[1]), initialState=lowU16(words[2]);
      if (anchorSlot<0 || anchorSlot>=28) {actorBoundary('Subordinate anchor must be a primary slot.','subordinate-anchor-input');return;}
      if (!actorInputRows) {actorBoundary('Subordinate construction requires the complete caller rows.','subordinate-row-input');return;}
      var ordinal=-1, row;
      for (var i=0;i<20;i++) {
        var candidate=launchBytes(actorInputRows[i],0xF8), flags=candidate.getUint32(0x40);
        if (candidate.getUint32(0x48) && !(flags&256) && (flags&512)) {ordinal=i;row=candidate;break;}
      }
      if (ordinal<0) return;
      var anchor=state.actors[anchorSlot];
      if (!anchor) {
        if (!initialActors) actorBoundary('Subordinate construction requires known anchor occupancy.','subordinate-anchor-input');
        return;
      }
      var present=[0,4,8].map(function(at){return row.getUint32(at)!==0;});
      if (!present.some(Boolean)) return;
      if (!present[0]) {actorBoundary('A later subordinate component cannot copy a missing ordinal-zero Actor.','subordinate-topology');return;}
      if (!initialActors) {actorBoundary('Subordinate construction requires complete slot occupancy.','subordinate-anchor-input');return;}
      var raw=nativeRecordForActor(anchor);
      if (!raw) {actorBoundary('Subordinate construction requires a complete qualified anchor record.','subordinate-anchor-record');return;}
      var context=row.getInt32(0x4C), art=row.getInt32(0x48), orientation=row.getInt32(0x58)<4 ? 1 : 0;
      var cloneSlot=0;
      if ([10,25,123].includes(context)) {
        while(cloneSlot<28 && state.actors[cloneSlot]) cloneSlot++;
        if (cloneSlot===28 && (present[1]||present[2])) {actorBoundary('Subordinate clone capacity is exhausted.','subordinate-capacity');return;}
      }
      var service=actorService('subordinateServices',node), allocationIndex=0, preparationIndex=0, previous=null;
      for (var linked=0;linked<3;linked++) {
        if (!present[linked]) continue;
        var current;
        if (linked===0) current=anchor;
        else {
          if (!service || allocationIndex>=service.allocations.length) {actorBoundary('Subordinate allocation result is unavailable after prior component effects.','subordinate-allocation-input');return;}
          if (!service.allocations[allocationIndex++]) {
            delete state.actors[cloneSlot];
            actorBoundary('Subordinate allocation failed after publishing null; earlier Actor effects remain.','subordinate-allocation-failed');return;
          }
          raw=nativeRecordForActor(previous);
          current=M.cloneJson(previous,'subordinate previous Actor');
          current.id='subordinate:'+launchInputs.invocationId+':'+(++subordinateSerial);
          current.slot=cloneSlot;current.label='Subordinate component '+linked;
          state.actors[cloneSlot]=current;
          raw.setFloat32(0x11C,Math.fround(previous.x-8));raw.setFloat32(0x124,Math.fround(previous.z+8));
          recordTrace({tick:state.tick,kind:'subordinate-clone',nodeId:node.id,slot:cloneSlot,linkedOrdinal:linked,
            allocationIdentity:current.id,copySourceIdentity:previous.id});
        }
        raw.setUint8(0x145,1);raw.setUint8(0x13D,1);
        raw.setUint16(0x138,lowU16(initialState+50*linked));raw.setUint16(0x134,lowU16(initialState+50*linked));
        raw.setUint8(0x149,linked);raw.setInt32(0xF8,0);raw.setInt32(0xF4,0);raw.setUint8(0x147,ordinal);
        raw.setInt32(0xE4,current.slot);raw.setInt32(0xE8,art);raw.setInt32(0xEC,context);
        raw.setUint16(0x13A,orientation);raw.setUint8(0x146,orientation);
        applyNativeRecord(current,raw,'subordinate-field-writes-before-preparation');
        var equipment=0;
        if (!(options.z64 instanceof Uint8Array) || options.z64.length<0x62310+278*32) {
          current.poseBlocked='subordinate-equipment-input';actorBoundary('Subordinate equipment types require the qualified ROM table.',current.poseBlocked);return;
        }
        for (var itemIndex=0;itemIndex<4;itemIndex++) {
          var item=row.getUint16(0x36+2*itemIndex);
          if (item>=278) {current.poseBlocked='subordinate-equipment-input';actorBoundary('Subordinate equipment is outside the supported native table.',current.poseBlocked);return;}
          var type=options.z64[0x62310+item*32];
          if ((type>=1&&type<=13)||type===24) {equipment=item;break;}
        }
        if (art>=0x39&&art<=0x46) {
          var override=[2,8,11,13,15,18,2,2,3,4,5,6,null,8][art-0x39];
          if (override!==null) equipment=override;
        }
        var preparation=service && service.preparations[preparationIndex++];
        var preparationMatches=preparation && preparation.sourceArt===art && preparation.ownerContext===context &&
          preparation.flagA===orientation && preparation.flagB===orientation && preparation.equipment===equipment;
        if (!preparationMatches || preparation.status!=='ready') {
          current.poseBlocked=preparationMatches && preparation.status==='cache-full' ? 'subordinate-cache-full' : 'subordinate-preparation-input';
          actorBoundary('Subordinate preparation has not returned with qualified resources; current field writes remain.',current.poseBlocked);return;
        }
        recordTrace({tick:state.tick,kind:'subordinate-preparation',nodeId:node.id,slot:current.slot,
          linkedOrdinal:linked,sourceArt:art,ownerContext:context,orientation:orientation,equipment:equipment,orientationArgumentsAlias:true});
        updateActorPose(current);
        if (current.poseBlocked) return;
        previous=current;
      }
    }

    function eventRow(node, kind, label, payload) {
      return {
        id: 'runtime:' + kind + ':' + node.id + ':' + state.tick,
        kind: kind,
        trackId: 'track:runtime:' + kind,
        label: label,
        startFrame: state.tick,
        durationFrames: 1,
        capability: M.capabilities.PREVIEW_ONLY,
        payload: payload || {}
      };
    }

    function setTransformChannels(keyframe) {
      state.transformChannels = keyframe.map(function(source) {
        var channel = {};
        SCENE_TRANSFORM_FIELDS.forEach(function(field) {
          channel[field.name] = source[field.name];
        });
        return channel;
      });
    }

    function executeSceneTransform(node, words) {
      var resourceIndex = signed(words[1]);
      var groupIndex = signed(words[2]);
      var duration = signed(words[3]);
      var resource;
      try {
        resource = transformResource(resourceIndex);
      } catch (error) {
        missing('Scene transform resource ' + resourceIndex + ' could not be decoded: ' +
          (error && error.message || String(error)));
      }
      if (!resource) {
        state.sceneTransformJob = {
          nodeId: node.id,
          resourceIndex: resourceIndex,
          groupIndex: groupIndex,
          remaining: Math.max(1, duration),
          decoded: false
        };
        missing('Scene transform resource ' + resourceIndex +
          ' needs normalized ROM bytes before Stage matrices can be decoded.');
        return;
      }
      var group = resource.groups[groupIndex];
      if (!group || !group.keyframeIds.length) {
        state.sceneTransformJob = null;
        missing('Scene transform resource ' + resourceIndex + ' has no sequence group ' +
          groupIndex + '.');
        return;
      }
      if (duration <= 0) {
        state.sceneTransformJob = null;
        missing('Scene transform ' + node.id + ' has a non-positive segment duration.');
        return;
      }
      setTransformChannels(resource.keyframes[group.keyframeIds[0]]);
      if (group.keyframeIds.length === 1) {
        state.sceneTransformJob = null;
        return;
      }
      state.sceneTransformJob = {
        nodeId: node.id,
        resourceIndex: resourceIndex,
        resourceKey: resource.resourceKey,
        groupIndex: groupIndex,
        duration: duration,
        segmentIndex: 0,
        elapsed: 0,
        remaining: (group.keyframeIds.length - 1) * duration,
        keyframeIds: group.keyframeIds.slice(),
        keyframes: resource.keyframes,
        decoded: true
      };
    }

    function executePrimitive(node) {
      if (!node) return;
      var execution = executionWords(node);
      var words = execution.words;
      var opcode = unsigned(words[0]);
      dispatchCount++;
      if (state.executedNodeIds.length < maxTraceEntries) state.executedNodeIds.push(node.id);
      recordTrace({ tick: state.tick, kind: 'command', nodeId: node.id,
        opcode: node.opcodeHex, name: node.name });
      if (execution.translatedWordOffsets.length) {
        recordTrace({
          tick: state.tick,
          kind: 'launch-translation',
          nodeId: node.id,
          wordOffsets: execution.translatedWordOffsets.slice()
        });
      }
      if (execution.unresolvedWordOffsets.length) {
        recordTrace({
          tick: state.tick,
          kind: 'launch-translation-missing',
          nodeId: node.id,
          wordOffsets: execution.unresolvedWordOffsets.slice()
        });
        var previewableActorVariant = opcode === 0x14 &&
          execution.unresolvedWordOffsets.every(function(wordOffset) {
            return wordOffset === 9;
          });
        var previewableDialogueHandshake = opcode === 0xBF &&
          execution.unresolvedWordOffsets.every(function(wordOffset) {
            return wordOffset === 3 || wordOffset === 11;
          });
        if (!previewableActorVariant && !previewableDialogueHandshake) return;
      }

      if (node.name === 'handoff_marker') {
        parserResumeMarked = true;
        return;
      }
      if (node.name === 'branch_barrier') return;
      if(framebufferProfile&&node.name==='image_transform_echo_start'){
        if(!echoProfile){producerBoundary('Image-transform echo requires its matrix endpoints, three cyclic matrix slots, and recurring fade renderer.','framebuffer-echo');return;}
        try{if(!imageEcho||!iris||!iris.saved)fail('Image echo requires its current image and completed iris close.','image-echo-input');
          var echoLayer=imageEcho.create(words.slice(1),state.transformChannels[1]);
          state.transformChannels[1]=echoLayer;syncIrisTransforms();iris.layers[1].resource={kind:'image-echo',assetId:state.sceneVignette.sourceAssetId};
          recordTrace({tick:state.tick,kind:'image-echo-start',operands:words.slice(1),clock:'native-resource-pass'});
        }catch(error){producerBoundary(error.message,error.code);}return;
      }
      if(nativeLaunch&&node.name==='frozen_frame_iris_transition'){
        if(!framebufferProfile){producerBoundary('Frozen-frame iris construction requires framebuffer capture and the render-layer lifecycle.','director-launch-iris');return;}
        if((words[8]>>>0)===0){try{if(!iris)iris=irisLayers();if(iris.record)fail('Closing iris cannot replace an active singleton.','framebuffer-phase');pendingIris={node:node,words:words.slice(1)};block={kind:'framebuffer-capture'};}catch(error){producerBoundary(error.message,error.code);}}
        else finishIris(words.slice(1),null);
        return;
      }
      if (node.name === 'control_bridge_and_pending_substream_handoff') {
        if (parserResumeMarked) commitPersistentCursorAfter(node);
        consumePendingSubstream(node);
        block = {
          kind: 'parser-boundary',
          untilTick: state.tick + 1,
          label: node.label,
          clock: 'director-evaluation'
        };
        return;
      }

      if (node.query) {
        executeBranchQuery(node, words);
        return;
      }

      // A complete imported record is usable for shallow-copy construction only
      // while every intervening native byte write is represented here. Older
      // presentation commands have partial models, so they invalidate that input.
      if (!(nativeLaunch&&opcode===0x14) && !(extendedModeTwoResume && opcode===0x03) && !node.query && ![0x07,0x2A,0x45,0xAB,0x92,0x96,0xA6,0xC2].includes(opcode) &&
          (/actor|body_pose/.test(node.name) || [0x1C,0x1D,0x1E,0x22,0x48].includes(opcode))) {
        Object.keys(state.actors).forEach(function(slot) {
          var actor=state.actors[slot];
          if (!actor.nativeRecordBase && !(actor.source && actor.source.recordHex)) return;
          forgetNativeRecord(actor);
        });
      }

      if (opcode === 0x01) state.registeredCounter = { value: 1, armTick: state.tick };
      else if (opcode === 0x02) state.registeredCounter = null;
      else if (opcode === 0x03) executeActorState(node, words);
      else if (opcode === 0x05) executeDialogueClose(words);
      else if (opcode === 0x06) executeDialogueResume(words);
      else if (opcode === 0x07) executeMove(node, words);
      else if (opcode === 0x08) {
        if (state.directorMode === 0) executeSceneTransform(node, words);
      }
      else if (opcode === 0x13) {
        var releaseSelector = signed(words[1]);
        selectedActors(releaseSelector).forEach(function(actor) {
          actor.visible = false;
          delete state.movementJobs[actor.slot];
          delete state.turnJobs[actor.slot];
        });
      }
      else if (opcode === 0x14) executeActorCreate(
        node, words, execution.unresolvedWordOffsets);
      else if (opcode === 0x15) executeTurn(node, words);
      else if (opcode === 0x1A) {
        state.textSpeed = lowU16(words[1]);
        if(dialogueEngine)try{dialogueEngine.machine.put(0x800e9c0c,state.textSpeed,2);}catch(error){producerBoundary(error.message,error.code);}
      }
      else if (opcode === 0x1B) executeOverlay(node, words);
      else if (opcode === 0x1C) {
        if (state.directorMode === 0) {
          selectedActors(signed(words[1])).forEach(function(actor) {
            actor.transformChannel = signed(words[2]);
          });
        }
      }
      else if (opcode === 0x1D) selectedActors(signed(words[1])).forEach(function(actor) {
        actor.uniformScale = signed(words[2]) / 100;
        if (continuousResume && capturedPresentation) actor.capturedMainScale = Math.fround(Math.fround(signed(words[2])) / Math.fround(100));
      });
      else if (opcode === 0x1E) executeTint(node, words);
      else if (opcode === 0x22) selectedActors(signed(words[1])).forEach(function(actor) {
        actor.yawDegrees = fixed(words[3]);
      });
      else if (opcode === 0x2A) {
        if (state.directorMode === 2) executeBodyPose(node, words);
      }
      else if (opcode === 0x2C) executeProjection(node, words, false);
      else if (opcode === 0x33) executeSceneColor(node, words);
      else if (opcode === 0x35) executeCamera(node, words, 'registered');
      else if (opcode === 0x36) executeCamera(node, words, 'actor');
      else if (opcode === 0x3B) executeScreenTransition(node, words);
      else if (opcode === 0x3D) executeProjection(node, words, true);
      else if (opcode === 0x3F) executeActorPresentationBootstrap(node);
      else if (opcode === 0x3A) executeSceneVignette(node, words);
      else if (opcode === 0x45 || opcode === 0xAB) executeOrdinaryRoster(node,words);
      else if (opcode === 0x92 || opcode === 0xA6) executeActorBinding(node, words);
      else if (opcode === 0x96) executeRosterReset(node);
      else if (opcode === 0xC2) executeSubordinate(node,words);
      else if (opcode === 0x46) executeSpriteEffect(node, words);
      else if (opcode === 0x47) state.shadowLight = {
        x: signed(words[1]), y: signed(words[2]), z: signed(words[3])
      };
      else if (opcode === 0x48) selectedActors(signed(words[1])).forEach(function(actor) {
        actor.opacityByte = unsigned(words[2]) & 0xFF;
      });
      else if (opcode === 0x56) parserResynchronization = true;
      else if (opcode === 0x59) {
        var marker = signed(words[1]);
        var destination = directorLabelByMarker[marker] || {
          marker: marker,
          nodeId: activeProgram.primitives[0] && activeProgram.primitives[0].id || null,
          startWord: 0,
          primitiveIndex: 0,
          compositeIndex: 0
        };
        persistentCursorPrimitiveIndex = destination.primitiveIndex;
        installCursorAtPrimitive(destination.primitiveIndex);
        block = {
          kind: 'cursor-replacement',
          untilTick: state.tick + 1,
          label: 'Jump to Director label ' + marker,
          clock: 'director-evaluation'
        };
        recordTrace({
          tick: state.tick,
          kind: 'cursor-replacement',
          sourceNodeId: node.id,
          marker: marker,
          destinationNodeId: destination.nodeId,
          destinationWord: destination.startWord
        });
      }
      else if (opcode === 0x5F) {
        var x1 = signed(words[1]), z1 = signed(words[2]);
        var x2 = signed(words[3]), z2 = signed(words[4]);
        state.transformDivider = {
          x1: x1, z1: z1, x2: x2, z2: z2,
          trueChannel: signed(words[5]), falseChannel: signed(words[6])
        };
      }
      else if (opcode === 0x62) {
        var transientSlot = signed(words[1]);
        var menuCreation=externalCreation('menuCreates',node);
        if(menuProfile){
          if(signed(words[2])!==33&&!([1,3].includes(signed(words[2]))&&menuProfile.optionController==='declared-action-direction-v1')){producerBoundary('Transient preset '+signed(words[2])+' has no supported shared constructor.','transient-menu-constructor');return;}
          try{if(!imageEcho)fail('Map menu requires the current scene image dimensions.','map-menu-input');
            if(!mapMenu){mapMenu=new OB64.cutsceneMapMenu(options.z64,menuProfile);
              recordTrace({tick:state.tick,kind:'map-menu-memory',bytes:checkMenuMemory(),limit:131072});}
            var computedMenu=mapMenu.create(transientSlot,signed(words[2]),imageEcho.width,imageEcho.height);
            state.transientRenderEntities[transientSlot]={slot:transientSlot,preset:signed(words[2]),native:true,computedMenu:true,status:computedMenu.status,statusSource:'computed-native-map-menu',createdTick:state.tick,detached:false};knownTransientSlots.add(transientSlot);
            recordTrace({tick:state.tick,kind:'map-menu-create',slot:transientSlot,preset:signed(words[2])});
          }catch(error){producerBoundary(error.message,error.code);}return;
        }
        if(nativeLaunch&&!menuCreation){producerBoundary('Transient render-entity preset '+signed(words[2])+' requires its constructor and recurring menu service.','transient-menu-constructor');return;}
        knownTransientSlots.add(transientSlot);
        state.transientRenderEntities[transientSlot] = {
          slot: transientSlot,
          preset: signed(words[2]),
          status: 0,
          statusSource: 'native-creation-clear',
          createdTick: state.tick,
          detached: false,
          closing: false,
          closeRemaining: null
          };
        if (menuCreation) {
          if (menuCreation.preset!==signed(words[2])) { producerBoundary('Menu initializer preset does not match its command.','menu-initial-state'); return; }
          Object.assign(state.transientRenderEntities[transientSlot],{
            native:true,ownerId:menuCreation.ownerId,entityId:menuCreation.entityId,
            substate:menuCreation.substate,selection:menuCreation.selection,
            cancel:menuCreation.cancel,optionCount:menuCreation.optionCount
          });
        }
      }
      else if (opcode === 0x63) executeAnimatedSceneSprite(node, words);
      else if (opcode === 0x64) executeAnimatedSceneSpriteRestart(node, words);
      else if (opcode === 0x66) {
        var effectSlot = signed(words[1]);
        if (effectSlot === -1) state.spriteEffects = {};
        else delete state.spriteEffects[effectSlot];
      }
      else if (opcode === 0x6B) {
        var releaseTransientSlot = signed(words[1]);
        if(mapMenu){try{mapMenu.release(releaseTransientSlot);}catch(error){producerBoundary(error.message,error.code);return;}}
        if (releaseTransientSlot === -1) {
          state.transientRenderEntities = {};
          allTransientSlotsKnown = true;
          knownTransientSlots.clear();
        } else {
          delete state.transientRenderEntities[releaseTransientSlot];
          knownTransientSlots.add(releaseTransientSlot);
        }
      }
      else if (opcode === 0x69) {
        var secondary = state.actors[signed(words[1])];
        if (secondary) {
          secondary.secondaryY = fixed(words[2]);
          secondary.heightModeByte = signed(words[3]) === 0 ? 0x02 : 0x04;
        }
      }
      else if (opcode === 0x6E || opcode === 0x6F || opcode === 0x70 || opcode === 0xB4) {
        var audioRow = rowFor(node, 'audio');
        state.audioEvents.push(audioRow ? {
          id: audioRow.clip.id, kind: audioRow.clip.kind, trackId: audioRow.track.id,
          label: audioRow.track.label, startFrame: state.tick, durationFrames: 1,
          capability: audioRow.clip.capability,
          payload: M.cloneJson(audioRow.clip.payload, 'audio.payload')
        } : eventRow(node, 'audio', 'Native audio command', {
          sourceSystem: 'director-native', nativeOpcode: node.opcodeHex,
          nativeOperands: words.slice(1).map(signed)
        }));
      }
      else if (opcode === 0x73) state.effectEvents.push(eventRow(node, 'effect',
        'Sepia vignette cleanup', { sourceSystem: 'director-native', nativeOpcode: '0x73' }));
      else if (opcode === 0x76) executeOversizedImageTransition(node, words);
      else if (opcode === 0x7B) {
        var yActor = state.actors[signed(words[1])];
        if (yActor) {
          var interval = signed(words[2]);
          if (interval === -1) yActor.y = signed(words[3]);
          else state.yJobs[yActor.slot] = {
            nodeId: node.id, slot: yActor.slot, interval: Math.max(1, interval),
            delta: signed(words[3]), remaining: Math.max(0, lowU16(words[4])), elapsed: 0
          };
        }
      }
      else if (opcode === 0x39) {
        var lifecycleOperand = signed(words[1]);
        if (lifecycleOperand === 0) {
          state.presentationLifecycleRequest = 0xD7;
          state.terminalReason = 'presentation-reload-handoff';
          state.terminal = true;
          recordTrace({
            tick: state.tick,
            kind: 'presentation-reload-handoff',
            nodeId: node.id,
            requestCode: 0xD7
          });
        } else {
          state.presentationLifecycleRequest = 0;
          state.alternateDirectorScheduling = false;
          recordTrace({
            tick: state.tick,
            kind: 'presentation-lifecycle-switch',
            nodeId: node.id,
            operand: lifecycleOperand,
            alternateDirectorScheduling: false
          });
        }
      }
      else if (opcode === 0x7D) {
        if(nativeLaunch||capturedServices)(nativeLaunch||capturedScheduler).releaseColor();
        state.terminalStateReleased = true;
        state.terminalReason = 'terminal-state-release';
        state.terminal = true;
      }
      else if (opcode === 0x7E) {
        if (state.overlay && state.overlay.native) {
          // The continuous Director path calls func_002839A8: request release.
          // Cleanup belongs to a later external callback, not this command.
          if (nativeLaunch||capturedServices){(nativeLaunch||capturedScheduler).releaseColor();state.overlay=(nativeLaunch||capturedScheduler).color();}
          else if (continuousResume) state.overlay.ownershipFlag=1;
          else state.overlay=cleanupNativeColor(state.overlay);
          state.overlayJob=state.overlay;
        } else if (externalProducers && externalProducers.initialColor !== undefined) {
          state.overlay=null;state.overlayJob=null;
        } else if (!producerBoundary('Color cleanup requires the current object and ownership flag.','color-cleanup-input')) {
          state.overlay=null;state.overlayJob=null;
        }
      }
      else if (opcode === 0x83) {
        var closingTransient = state.transientRenderEntities[signed(words[1])];
        if (closingTransient && !closingTransient.detached) {
          closingTransient.closing = true;
          closingTransient.closeRemaining = 8;
        }
      }
      else if (opcode === 0x8B) {
        var detachedTransient = state.transientRenderEntities[signed(words[1])];
        if (detachedTransient) {
          detachedTransient.detached = true;
          detachedTransient.closing = false;
          detachedTransient.closeRemaining = null;
        }
      }
      else if (opcode === 0x8C) state.armyManagementCursorLatch = signed(words[1]);
      else if (opcode === 0x99) pendingSubstreamSelector = unsigned(words[1]) & 0xFF;
      else if (opcode === 0x9A) pendingSubstreamSelector = 0xFE;
      else if (opcode === 0xAF) {
        state.titleJob = { nodeId: node.id, remaining: 30, duration: 30, kind: 'alpha' };
        assumption('Prologue title alpha uses a 30-tick preview envelope.');
      }
      else if (opcode === 0xB0) {
        state.titleJob = { nodeId: node.id, remaining: 85, duration: 85, kind: 'reveal' };
        assumption('Prologue secondary-title reveal uses its static 85-update Stage envelope.');
      }
      else if (opcode === 0xBB) {
        state.directorTeardown = true;
        state.terminalReason = 'director-teardown';
        state.terminal = true;
      }
      else if (opcode === 0xBF) executeDialogueCreate(
        node, words, execution.unresolvedWordOffsets);
      else if (opcode === 0x80000001) {
        state.terminalReason = 'terminal-hold';
        state.terminal = true;
      }
      else if (opcode === 0x80000006) executeBackground(node, words);
      else if (node.name !== 'director_label_marker') {
        uniquePush(unsupportedCommands, node.name);
      }
    }

    function updateMovementJobs() {
      Object.keys(state.movementJobs).forEach(function(slot) {
        var job = state.movementJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.movementJobs[slot]; delete vacantMovementActivity[slot]; return; }
        var alive;
        try { alive = advanceNativeMovement(actor, job); }
        catch (error) { actorBoundary(error.message, error.code); job.pauseByte = 1; return; }
        actor.movementFrame = job.elapsed;
        if (!alive) {
          actor.activeMovementId = null;
          delete state.movementJobs[slot];
        }
      });
    }

    function updateTurnJobs() {
      Object.keys(state.turnJobs).forEach(function(slot) {
        var job = state.turnJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.turnJobs[slot]; return; }
        job.elapsed += 1;
        var step = Math.min(job.phaseDistance,
          1 + Math.floor((job.elapsed - 1) / job.cadence));
        var phase = (job.startPhase + job.phaseStep * step) % 12;
        if (phase < 0) phase += 12;
        var facing = FACING_FOR_PHASE[phase];
        actor.nativeFacing = facing;
        actor.facing = 'native-' + facing;
        actor.poseId = Number.isInteger(actor.bank) && Number.isInteger(actor.animationKey)
          ? poseId(actor.bank, actor.animationKey, facing) : actor.poseId;
        if (job.appliedStep !== step) {
          // func_0029C790 calls the State setter on each phase change. That
          // setter performs an immediate pose before the ordinary Actor pass.
          // It clears material values, but retains the material delta bytes.
          var retainedDelta=actor.materialDelta;
          startPose(actor);
          actor.materialDelta=retainedDelta;
          updateActorPose(actor);
          job.appliedStep=step;
        }
        if (job.elapsed >= job.completionCalls) {
          actor.nativeFacing = job.targetFacing;
          actor.facing = 'native-' + job.targetFacing;
          actor.poseId = Number.isInteger(actor.bank) && Number.isInteger(actor.animationKey)
            ? poseId(actor.bank, actor.animationKey, job.targetFacing) : actor.poseId;
          delete state.turnJobs[slot];
        }
      });
    }

    function updateTintJobs() {
      Object.keys(state.tintJobs).forEach(function(slot) {
        var job = state.tintJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.tintJobs[slot]; return; }
        job.elapsed += 1;
        job.remaining -= 1;
        var amount = clamp(job.elapsed / job.duration, 0, 1);
        actor.tint = {
          red: Math.round(mix(job.from.red, job.to.red, amount)),
          green: Math.round(mix(job.from.green, job.to.green, amount)),
          blue: Math.round(mix(job.from.blue, job.to.blue, amount))
        };
        if (job.remaining <= 0) delete state.tintJobs[slot];
      });
    }

    function updateProjectionJob() {
      var job = state.projectionJob;
      if (!job) return;
      job.elapsed += 1;
      job.remaining -= 1;
      if(nativeLaunch&&nativeLaunch.input.sceneMode===2){state.projectionTransform=OB64.cutsceneRomStart.projection(nativeLaunch);if(job.remaining<=0)state.projectionJob=null;return;}
      var amount = clamp(job.elapsed / job.duration, 0, 1);
      state.projectionTransform = {
        translateX: mix(job.from.translateX, job.to.translateX, amount),
        translateY: mix(job.from.translateY, job.to.translateY, amount),
        scaleX: mix(job.from.scaleX, job.to.scaleX, amount),
        scaleY: mix(job.from.scaleY, job.to.scaleY, amount)
      };
      if (job.remaining <= 0) state.projectionJob = null;
    }

    function updateScreenTransition() {
      var transition = state.screenTransition;
      if (!transition || transition.progress === transition.duration) return;
      transition.progress = Math.min(transition.duration, transition.progress + 1);
      transition.currentFirst = screenEdgeValue(transition.initialFirst,
        transition.finalFirst, transition.progress, transition.duration);
      transition.currentSecond = screenEdgeValue(transition.initialSecond,
        transition.finalSecond, transition.progress, transition.duration);
      if (transition.progress === transition.duration && transition.persistence === 0) {
        state.screenTransition = null;
      }
    }

    function updateColorJobs() {
      if (state.overlayJob && state.overlay && !state.overlay.native) {
        state.overlayJob.elapsed += 1;
        state.overlayJob.remaining -= 1;
        state.overlay.alpha = Math.round(mix(state.overlayJob.startAlpha,
          state.overlayJob.targetAlpha,
          clamp(state.overlayJob.elapsed / state.overlayJob.duration, 0, 1)));
        if (state.overlayJob.remaining <= 0) state.overlayJob = null;
      }
      if (state.sceneColorJob) {
        var colorJob = state.sceneColorJob;
        colorJob.elapsed += 1;
        colorJob.remaining -= 1;
        var amount = clamp(colorJob.elapsed / colorJob.duration, 0, 1);
        state.sceneColor = {
          red: Math.round(mix(colorJob.from.red, colorJob.to.red, amount)),
          green: Math.round(mix(colorJob.from.green, colorJob.to.green, amount)),
          blue: Math.round(mix(colorJob.from.blue, colorJob.to.blue, amount))
        };
        if (colorJob.remaining <= 0) state.sceneColorJob = null;
      }
    }

    function updateOtherJobs() {
      Object.keys(state.bodyPoseJobs).forEach(function(slot) {
        state.bodyPoseJobs[slot].remaining -= 1;
        if (state.bodyPoseJobs[slot].remaining <= 0) delete state.bodyPoseJobs[slot];
      });
      Object.keys(state.yJobs).forEach(function(slot) {
        var job = state.yJobs[slot];
        var actor = state.actors[slot];
        if (!actor) { delete state.yJobs[slot]; return; }
        job.elapsed += 1;
        job.remaining -= 1;
        if (job.elapsed % job.interval === 0) actor.y += job.delta;
        if (job.remaining <= 0) delete state.yJobs[slot];
      });
      if (state.sceneTransformJob) {
        var transformJob = state.sceneTransformJob;
        if (!transformJob.decoded) {
          transformJob.remaining -= 1;
          if (transformJob.remaining <= 0) state.sceneTransformJob = null;
        } else {
          transformJob.elapsed += 1;
          transformJob.remaining -= 1;
          var from = transformJob.keyframes[
            transformJob.keyframeIds[transformJob.segmentIndex]];
          var to = transformJob.keyframes[
            transformJob.keyframeIds[transformJob.segmentIndex + 1]];
          state.transformChannels = from.map(function(fromChannel, channelIndex) {
            var toChannel = to[channelIndex];
            var output = {};
            SCENE_TRANSFORM_FIELDS.forEach(function(field) {
              var deltaRaw = Math.trunc((toChannel[field.name + 'Raw'] -
                fromChannel[field.name + 'Raw']) / transformJob.duration);
              output[field.name] = (fromChannel[field.name + 'Raw'] +
                deltaRaw * transformJob.elapsed) / 1000;
            });
            return output;
          });
          if (transformJob.elapsed >= transformJob.duration) {
            setTransformChannels(to);
            transformJob.segmentIndex += 1;
            transformJob.elapsed = 0;
            if (transformJob.segmentIndex >= transformJob.keyframeIds.length - 1) {
              state.sceneTransformJob = null;
            }
          }
        }
      }
      if (state.titleJob) {
        state.titleJob.remaining -= 1;
        if (state.titleJob.remaining <= 0) state.titleJob = null;
      }
      Object.keys(state.spriteEffects).forEach(function(slot) {
        updateSpriteEffectProgram(state.spriteEffects[slot]);
      });
      Object.keys(state.actors).forEach(function(slot) {
        if (state.alternateDirectorScheduling !== true) updateActorPose(state.actors[slot]);
      });
      Object.keys(state.dialogues).forEach(function(windowId) {
        var window = state.dialogues[windowId];
        if (!window.closed && state.tick >= window.readyTick) window.paused = true;
      });
    }

    function updateOversizedImageTransition() {
      var view = state.oversizedImageView;
      if (view.zoomState === 1) {
        view.scale = Math.fround(Math.max(0.7, view.scale - 0.01));
        if (view.scale <= 0.7) view.zoomState = 3;
      } else if (view.zoomState === 2) {
        view.scale = Math.fround(Math.min(1, view.scale + 0.01));
        if (view.scale >= 1) view.zoomState = 4;
      }
      var job = state.oversizedImageTransitionJob;
      if (!job) return;
      if (job.progress === job.duration) {
        state.oversizedImageTransitionJob = null;
        return;
      }
      job.progress = (job.progress + 1) | 0;
      view.x = Math.fround(view.x + job.rateX);
      view.y = Math.fround(view.y + job.rateY);
    }

    function updateJobs() {
      updateOversizedImageTransition();
      if (state.alternateDirectorScheduling !== true) updateMovementJobs();
      updateTurnJobs();
      updateProjectionJob();
      updateScreenTransition();
      updateTintJobs();
      updateColorJobs();
      updateOtherJobs();
      Object.keys(state.transientRenderEntities).forEach(function(slot) {
        var entity = state.transientRenderEntities[slot];
        if (!entity || entity.detached) return;
        if (entity.native || options.diagnosticAssumptions !== true) return;
        if (entity.closing) {
          entity.closeRemaining -= 1;
          if (entity.closeRemaining <= 0) {
            entity.detached = true;
            entity.closing = false;
            entity.closeRemaining = null;
            recordTrace({
              tick: state.tick,
              kind: 'transient-render-entity-detach',
              slot: entity.slot,
              preset: entity.preset,
              source: 'native-eight-update-graceful-close'
            });
          }
          return;
        }
        if (entity.statusSource === 'native-creation-clear' &&
            entity.createdTick < state.tick &&
            entity.preset >= 1 && entity.preset <= 3) {
          entity.status = -1;
          entity.statusSource = 'native-main-menu-neutral';
          recordTrace({
            tick: state.tick,
            kind: 'transient-render-entity-status',
            slot: entity.slot,
            preset: entity.preset,
            status: entity.status,
            source: entity.statusSource
          });
        }
      });
      if (state.registeredCounter) {
        var registeredValue = state.registeredCounter.value >>> 0;
        if (registeredValue >= 1 && registeredValue <= 0x0FFFFFFE) {
          state.registeredCounter.value = (registeredValue + 1) >>> 0;
        }
      }
    }

    function compare(actual, mode, target) {
      if (mode === 0) return actual === target;
      if (mode === 1) return actual !== target;
      if (mode === 2) return actual >= target;
      if (mode === 3) return actual <= target;
      if (mode === 4) return actual > target;
      if (mode === 5) return actual < target;
      return false;
    }

    function passingQueryValue(query) {
      var mode = query.query.compareMode;
      var target = query.query.target;
      if (mode === 1) return target === 0 ? 1 : 0;
      if (mode === 4) return target + 1;
      if (mode === 5) return target - 1;
      return target;
    }

    function unresolvedInput(query, context) {
      missing('Native input is unavailable for ' + query.label + '.');
      if (options.diagnosticAssumptions === true) return false;
      stopReason = 'external-input';
      unresolvedQuery = { nodeId: query.id, streamAssetId: activeStreamAssetId,
        label: query.label, kind: context && context.kind || 'query' };
      return true;
    }

    function incompleteLifecycleValue(query, actual, context, message) {
      if (!context || context.kind !== 'wait' ||
          compare(actual, query.query.compareMode, query.query.target)) return actual;
      if (unresolvedInput(query, context)) return NaN;
      assumption(message);
      return passingQueryValue(query);
    }

    function externalQueryValue(query) {
      var externalValues = options.externalQueryValues || {};
      var input = query.query && query.query.producerInput;
      var inputKey = query.name + ':' + String(input == null ? '' : input);
      if (Object.prototype.hasOwnProperty.call(externalValues, query.id)) {
        return externalValues[query.id];
      }
      if (Object.prototype.hasOwnProperty.call(externalValues, inputKey)) {
        return externalValues[inputKey];
      }
      if (Object.prototype.hasOwnProperty.call(externalValues, query.name)) {
        return externalValues[query.name];
      }
      return null;
    }

    function queryActual(query, context) {
      context = context || {};
      var input = query.query && query.query.producerInput;
      if (query.name === 'registered_counter_query' ||
          query.name === 'a_button_skippable_registered_wait_query') {
        if (query.name === 'a_button_skippable_registered_wait_query' &&
            Number.isInteger(currentControllerMask()) &&
            (currentControllerMask() & 0x8000) !== 0) {
          state.registeredCounter = {
            value: (query.query.target + 1) >>> 0,
            armTick: state.tick,
            source: 'a-button-skip'
          };
        }
        var registeredValue = state.registeredCounter
          ? state.registeredCounter.value >>> 0 : 0;
        return (registeredValue - 1) | 0;
      }
      if (query.name === 'actor_movement_countdown_query') {
        return state.movementJobs[input] ? lowS16(state.movementJobs[input].remaining) : 0;
      }
      if (query.name === 'actor_facing_turn_activity_query') return state.turnJobs[input] ? 1 : 0;
      if (query.name === 'dialogue_pause_query') {
        if(dialogueEngine && state.tick<=externalProducers.throughTick) {
          try{return dialogueEngine.query(input);}catch(error){producerBoundary(error.message,error.code);return null;}
        }
        if(producerBoundary('Dialogue query requires current native resource state and complete service history.','dialogue-query-input'))return null;
        var window = state.dialogues[input];
        if (!window || window.closed) return 0;
        return window.paused ? 2 : 1;
      }
      if(query.name==='dialogue_control_byte_query') {
        if(dialogueEngine && state.tick<=externalProducers.throughTick && Number.isInteger(input)&&input>=0&&input<8) {
          try{return dialogueEngine.machine.get(0x8019ee40+input,1);}catch(error){producerBoundary(error.message,error.code);return null;}
        }
        if(producerBoundary('Dialogue control query requires its current native byte and complete writer history.','dialogue-control-input'))return null;
      }
      if (query.name === 'scene_projection_transform_countdown_query_mode2' ||
          query.name === 'scene_projection_transform_countdown_query_unguarded') {
        return state.projectionJob ? state.projectionJob.remaining : 0;
      }
      if (query.name === 'screen_edge_transition_activity_query') {
        return state.screenTransition &&
          state.screenTransition.progress !== state.screenTransition.duration ? 1 : 0;
      }
      if (query.name === 'scripted_oversized_image_transition_query') {
        return state.oversizedImageTransitionJob ? 1 : 0;
      }
      if (query.name === 'actor_presentation_activity_query') {
        var suppliedPresentationStatus = externalQueryValue(query);
        if (Number.isInteger(suppliedPresentationStatus)) {
          if (suppliedPresentationStatus === 0) state.actorPresentationJob = null;
          return suppliedPresentationStatus;
        }
        if (context.kind === 'wait') {
          if (unresolvedInput(query, context)) return NaN;
          assumption('Actor-presentation lifecycle input is unavailable; the exact native wait uses an explicit completed-state assumption.');
          state.actorPresentationJob = null;
          return passingQueryValue(query);
        }
        return state.actorPresentationJob ? 1 : 0;
      }
      if (query.name === 'color_overlay_countdown_query') {
        if (!externalProducers || externalProducers.initialColor===undefined || state.overlay && !state.overlay.native) {
          if (unresolvedInput(query,context)) return NaN;
        } else if (state.tick>externalProducers.throughTick) {
          if (unresolvedInput(query,context)) return NaN;
        }
        return state.overlayJob ? state.overlayJob.remaining : 0;
      }
      if (query.name === 'actor_state_pose_opcode_query') {
        var actor = state.actors[input];
        if (actor && actor.decoderMode !== 0) {
          var ordinary=qualifiedPoseForActor(actor,'ordinary');
          if (ordinary) {var currentRecord=ordinary.records[actor.poseCursor];return currentRecord ? currentRecord.opcode & 255 : 0;}
          actorBoundary('Alternate Actor pose query requires its separate same-context ordinary directory.','ordinary-pose-query-input');
          if (options.diagnosticAssumptions !== true) return NaN;
          assumption('Diagnostic alternate pose query uses a legacy readiness estimate; no ordinary directory was supplied.');
          return state.tick < actor.poseReadyTick ? 1 : 0;
        }
        if (!actor || actor.poseBlocked || actor.decoderMode !== 0) {
          actorBoundary('Pose opcode query requires an occupied Actor with a supported counted program.');
          if (options.diagnosticAssumptions === true) {
            assumption('Diagnostic pose-query fallback uses the previous duration estimate for unsupported Actor state; it is not the native opcode query.');
            return actor && state.tick < actor.poseReadyTick ? 1 : 0;
          }
          return NaN;
        }
        var poseProgram = programForActor(actor);
        if (!poseProgram) { actorBoundary('Pose opcode query requires its physical counted program.'); return NaN; }
        var poseRecord = poseProgram.records[actor.poseCursor];
        return poseRecord ? poseRecord.opcode & 255 : 0;
      }
      if (query.name === 'animated_scene_sprite_program_opcode_query') {
        var spriteEffect = state.spriteEffects[input];
        if (!spriteEffect) {
          missing('Animated scene sprite program query selects absent slot ' + input + '.');
          return 0;
        }
        return spriteEffect.currentOpcode & 0xFF;
      }
      if (query.name === 'scene_transform_sequence_query') {
        return state.sceneTransformJob ? 0 : 1;
      }
      if (query.name === 'prologue_title_reveal_query') {
        return state.titleJob && state.titleJob.kind === 'reveal' ? 1 : 0;
      }
      if (query.name === 'global_halfword_mask_query') {
        var controllerMask = Number.isInteger(currentControllerMask())
          ? currentControllerMask() & 0xFFFF : 0;
        if (!Number.isInteger(currentControllerMask())) {
          var neutralMaskValue = (controllerMask & (input & 0xFFFF)) !== 0 ? 1 : 0;
          return incompleteLifecycleValue(query, neutralMaskValue, context,
            'No controller input is supplied; this native input wait uses an explicit completed-state assumption.');
        }
        return (controllerMask & (input & 0xFFFF)) !== 0 ? 1 : 0;
      }
      if (query.name === 'transient_render_entity_status_query') {
        if(menuProfile){try{if(!Number.isInteger(input)||input<0||input>=14)fail('Menu query slot is outside the owner pool.','map-menu-input');return mapMenu?mapMenu.query(input):-5;}catch(error){producerBoundary(error.message,error.code);return NaN;}}
        var suppliedTransientStatus = externalQueryValue(query);
        if (Number.isInteger(suppliedTransientStatus)) return suppliedTransientStatus;
        var transientEntity = state.transientRenderEntities[input];
        if (!transientEntity && !allTransientSlotsKnown && !knownTransientSlots.has(input)) {
          if (unresolvedInput(query,context)) return NaN;
        }
        var transientStatus = !transientEntity ? -5 :
          (transientEntity.detached ? -6 : transientEntity.status);
        if (transientEntity && transientEntity.native) {
          if (state.tick>externalProducers.throughTick) {
            if (unresolvedInput(query,context)) return NaN;
          }
          return transientStatus;
        }
        if (transientEntity && (transientEntity.preset<1 || transientEntity.preset>24) && options.diagnosticAssumptions!==true) {
          actorBoundary('Transient preset '+transientEntity.preset+' has no supported menu producer.','unsupported-transient-menu-preset');
          return NaN;
        }
        if (context.kind === 'branch' && transientEntity &&
            transientEntity.statusSource === 'native-main-menu-neutral') {
          missing('Transient render-entity preset ' + transientEntity.preset +
            ' requires a controller/menu result; the native neutral status -1 is preserved.');
        }
        return incompleteLifecycleValue(query, transientStatus, context,
          'Transient render-entity preset ' +
            (transientEntity ? transientEntity.preset : 'absent') +
            ' has an external preset-specific updater; this native wait uses its completed-state assumption.');
      }
      if (query.name === 'army_management_cursor_latch_query') {
        var armyLatchStatus = state.armyManagementCursorLatch === 1 ? 0 : 1;
        return incompleteLifecycleValue(query, armyLatchStatus, context,
          'Army Management cursor input is not supplied; this native wait uses an explicit completed-state assumption.');
      }
      var externalValue = externalQueryValue(query);
      if(framebufferProfile&&query.name==='frozen_frame_iris_activity_query')return iris?iris.query():0;
      // Native v0 is 0xffffffff during the reverse echo tail; Q4 compares it with signed -1.
      if(echoProfile&&query.name==='image_transform_echo_activity_query')return imageEcho?(imageEcho.query()|0):0;
      if(nativeLaunch&&query.name==='alternate_presentation_context_presence_query'&&nativeLaunch.input.world.alternateContextPointer!==undefined)return nativeLaunch.input.world.alternateContextPointer?1:0;
      if (Number.isInteger(externalValue)) return externalValue;
      if (unresolvedInput(query, context)) return NaN;
      if (context.kind === 'wait') {
        assumption('Native wait input for ' + query.label +
          ' is outside the preview model; the wait uses an explicit completed-state assumption.');
        return passingQueryValue(query);
      }
      missing('Native query input for ' + query.label +
        ' is not supplied; the deterministic preview uses the producer\'s neutral value zero.');
      return 0;
    }

    function queryEnabled(query) {
      if (!query) return false;
      if (query.name === 'scene_transform_sequence_query') {
        return state.directorMode === 0;
      }
      if (query.name === 'actor_body_pose_cycle_query' ||
          query.name === 'scene_projection_transform_countdown_query_mode2') {
        return state.directorMode === 2;
      }
      return true;
    }

    function queryPasses(query, context) {
      if (!query || !query.query) return true;
      if (!queryEnabled(query)) return true;
      return compare(queryActual(query, context),
        query.query.compareMode, query.query.target);
    }

    function jobActiveFor(start) {
      var slot = start.operands.length ? start.operands[0].signed : null;
      if (start.name === 'actor_move') return !!state.movementJobs[slot];
      if (start.name === 'actor_facing_turn_transition') return !!state.turnJobs[slot];
      if (start.name === 'actor_rgb_tint_transition') {
        return slot === -1 ? Object.keys(state.tintJobs).length > 0 : !!state.tintJobs[slot];
      }
      if (start.name === 'actor_body_pose_program_start') return !!state.bodyPoseJobs[slot];
      if (start.name === 'scene_transform_sequence_start') return !!state.sceneTransformJob;
      if (start.name === 'scripted_oversized_image_pan_zoom') {
        return !!state.oversizedImageTransitionJob;
      }
      if (/scene_projection_transform/.test(start.name)) return !!state.projectionJob;
      if (start.name === 'screen_edge_transition_start') {
        return !!state.screenTransition &&
          state.screenTransition.progress !== state.screenTransition.duration;
      }
      if (start.name === 'full_screen_color_overlay_fade') return !!state.overlayJob && (!state.overlay.native || state.overlay.remaining!==0);
      return false;
    }

    function snapshot(block) {
      var background = M.cloneJson(state.background, 'runtime background');
      if (state.directorMode === 0 && Array.isArray(background.layers)) {
        background.layers = background.layers.map(function(layer, index) {
          var nativeOrdinal = Number.isFinite(layer.nativeOrdinal)
            ? layer.nativeOrdinal
            : (layer.source && Number.isFinite(layer.source.traversalOrdinal)
              ? layer.source.traversalOrdinal
              : (Number.isFinite(layer.depth) ? layer.depth : index));
          var channel = state.transformChannels[nativeOrdinal] ||
            identityTransformChannel();
          layer.nativeOrdinal = nativeOrdinal;
          layer.transformChannel = nativeOrdinal;
          layer.sceneTransform = Object.assign({}, channel);
          layer.renderPipeline = 'mode-zero-b5-actor-camera';
          return layer;
        });
      }
      var actors = Object.keys(state.actors).map(function(slot) {
        var actor = state.actors[slot];
        var channel = state.transformChannels[actor.transformChannel] ||
          identityTransformChannel();
        var modeZeroStage = state.directorMode === 0;
        var renderedY = actor.heightModeByte & 0x04
          ? actor.y + actor.secondaryY
          : (actor.heightModeByte & 0x02 ? actor.secondaryY : actor.y);
        return {
          id: actor.id,
          label: actor.label,
          slot: actor.slot,
          artSourceId: actor.artSourceId,
          capability: actor.capability,
          visible: actor.visible,
          opacityByte: actor.opacityByte,
          renderModeByte: actor.renderModeByte,
          x: modeZeroStage ? actor.x : actor.x + channel.translateX,
          y: modeZeroStage ? renderedY : renderedY + channel.translateY,
          z: modeZeroStage ? actor.z : actor.z + channel.translateZ,
          baseX: actor.x,
          baseY: actor.y,
          baseZ: actor.z,
          secondaryY: actor.secondaryY,
          heightModeByte: actor.heightModeByte,
          facing: actor.facing,
          poseId: actor.poseId,
          bank: actor.bank,
          animationKey: actor.animationKey,
          nativeFacing: actor.nativeFacing,
          variantSelector: actor.variantSelector,
          poseFrame: actor.poseFrame,
          displayedFrameToken: actor.displayedFrameToken,
          nativeActorState: encodeNativeActorState(actor),
          poseBlocked: actor.poseBlocked,
          poseProgramStatus: actor.poseProgramStatus,
          poseLoop: actor.poseLoop,
          poseDuration: actor.poseDuration,
          bodyPoseProgram: actor.bodyPoseProgram
            ? M.cloneJson(actor.bodyPoseProgram, 'actor.bodyPoseProgram') : null,
          movementFrame: actor.movementFrame,
          activeMovementId: actor.activeMovementId,
          uniformScale: actor.uniformScale * channel.uniformScale,
          nativeUniformScale: capturedPresentation ? actor.capturedMainScale : actor.uniformScale,
          tint: Object.assign({}, actor.tint),
          yawDegrees: actor.yawDegrees + channel.rotationY,
          pitchDegrees: channel.rotationX,
          transformChannel: actor.transformChannel,
          sceneTransform: Object.assign({}, channel),
          renderPipeline: modeZeroStage ? 'mode-zero-registered-prepass-actor-camera' :
            'actor-camera-direct',
          source: actor.nativeRecordBase ? Object.assign({},actor.source,{recordHex:recordHex(nativeRecordForActor(actor))}) : actor.source
        };
      }).sort(function(left, right) { return left.z - right.z || left.slot - right.slot; });
      var dialogueWindows=Object.keys(state.dialogues).map(function(id){return state.dialogues[id];});
      if(dialogueEngine)dialogueWindows=dialogueEngine.owners.map(function(owner,slot){
        if(!owner)return null;
        var record=0x800e82c8+slot*0xa8;
        if(dialogueEngine.machine.get(record+0x10)!==0x80198be8)return null;
        return dialogueWindows.find(function(w){return w.nativeOwnerId===owner.ownerId;})||{
          windowId:dialogueEngine.machine.get(record+0x8f,1),nativeSlot:slot,nativeOwnerId:owner.ownerId,
          segments:[''],segmentIndex:0,sourceNodeId:'native:'+owner.ownerId,layout:{},speaker:'Dialogue'};
      }).filter(Boolean);
      var dialogue = dialogueWindows.map(function(window) {
        var nativePresentation = window.nativeSlot != null && dialogueEngine
          ? dialogueEngine.presentation(window.nativeSlot,window.nativeOwnerId) : null;
        if(window.nativeSlot != null && !nativePresentation)return null;
        if (window.closed) return null;
        var entry = window.entry;
        return {
          id: 'runtime-dialogue:' + window.sourceNodeId,
          kind: 'dialogue',
          trackId: 'track:runtime:dialogue',
          label: 'Dialogue window ' + window.windowId,
          startFrame: 0,
          durationFrames: 1,
          capability: M.capabilities.PREVIEW_ONLY,
          payload: {
            windowId: window.windowId,
            sourceSystem: 'serifu-runtime',
            dialogueArchiveId: window.archive && window.archive.archiveId ||
              window.dialogueArchiveId || null,
            dialogueEntryId: entry && entry.entryId ||
              window.dialogueEntryId || null,
            presentationArchiveSelector: window.selector,
            presentationEntrySelector: window.entrySelector,
            speaker: entry && (entry.speakerLabel ||
              (entry.speakerId == null ? 'Narrator' : 'Speaker ' + entry.speakerId)) ||
              window.speaker || 'Narrator',
            text: nativePresentation?nativePresentation.text:window.segments[window.segmentIndex],
            nativeDialogue:nativePresentation,
            rawText: entry && entry.rawText || window.rawText || '',
            paused: nativePresentation?nativePresentation.paused:window.paused,
            ownerActorSlot: window.ownerActorSlot,
            layout: window.layout
          }
        };
      }).filter(Boolean);
      var effects = Object.keys(state.spriteEffects).map(function(slot) {
        return M.cloneJson(state.spriteEffects[slot], 'runtime sprite effect');
      }).concat(state.effectEvents);
      var cameraProjection = projectionFromCamera(state.cameras.actor);
      var registeredProjection = projectionFromCamera(state.cameras.registered);
      return {
        ...(nativeActorDrawing?{nativeActorDrawing:nativeActorDrawing}:{}),
        frame: state.tick,
        timeSeconds: state.tick / M.previewFps,
        pathId: 'default',
        background: background,
        actors: actors,
        dialogue: dialogue,
        audio: state.audioEvents.slice(),
        camera: state.cameraEvents.slice(),
        cameraState: Object.assign({}, state.projectionTransform, {
          activeClipId: state.projectionJob && state.projectionJob.nodeId || null,
          timingStatus: state.projectionJob
            ? 'native projection updater invocation' : 'native projection state'
        }),
        actorProjection: cameraProjection,
        registeredProjection: registeredProjection,
        effects: effects,
        flow: block ? [{
          id: 'runtime-flow:' + state.tick,
          kind: 'wait',
          trackId: 'track:runtime:flow',
          label: block.label,
          startFrame: state.tick,
          durationFrames: 1,
          capability: M.capabilities.PREVIEW_ONLY,
          payload: { nativeClock: block.clock || 'director-evaluation' }
        }] : state.flowEvents.slice(),
        overlays: state.overlay ? [Object.assign({}, state.overlay)] : [],
        framebufferEffect:iris?OB64.cutsceneFramebuffer.snapshot(iris):null,
        imageEcho:imageEcho&&imageEcho.initialized?imageEcho.snapshot():null,
        mapMenu:mapMenu?mapMenu.snapshot():null,
        nativeExternal: {
          dialogue:dialogueEngine?dialogueEngine.snapshot(sharedActorProfile||romOnlyStart?256:undefined):null,
          sharedRequests:Object.assign({},state.sharedRequests),
          menus:Object.keys(state.transientRenderEntities).map(function(slot) {
            return Object.assign({},state.transientRenderEntities[slot]);
          }),
          consumedServices:externalEventCursor,
          consumedPoseCalls:sharedPoseCallCursor
        },
        sceneColor: Object.assign({}, state.sceneColor),
        screenTransition: state.screenTransition
          ? Object.assign({}, state.screenTransition) : null,
        sceneVignette: state.sceneVignette
          ? M.cloneJson(state.sceneVignette, 'scene vignette') : null,
        oversizedImageView: Object.assign({}, state.oversizedImageView),
        oversizedImageTransition: state.oversizedImageTransitionJob
          ? Object.assign({}, state.oversizedImageTransitionJob) : null,
        transformChannels: state.transformChannels.map(function(channel) {
          return Object.assign({}, channel);
        }),
        runtime: {
          engine: 'director-scheduler',
          clock: resourceScheduler?'declared-resource-pass':'director-evaluation',
          directorMode: state.directorMode,
          directorModeStatus: state.directorModeStatus,
          directorSelector: screenTransitionVariant,
          tick: state.tick,
          activeStreamAssetId: activeStreamAssetId,
          savedParentStreamAssetId: savedStreamFrame && savedStreamFrame.assetId || null,
          compositeIndex: compositeIndex,
          blockKind: block && block.kind || null,
          blockLabel: block && block.label || null,
          assumptionCount: assumptions.length,
          missingInputCount: missingInputs.length,
          presentationLifecycleRequest: state.presentationLifecycleRequest,
          alternateDirectorScheduling: state.alternateDirectorScheduling,
          actorPresentationStatus: state.actorPresentationJob
            ? state.actorPresentationJob.evidenceStatus : null,
          terminalReason: state.terminalReason,
          status: missingInputs.length ? 'missing-inputs' :
            (assumptions.length ? 'assumed-inputs' : 'profiled')
        }
      };
    }

    function runtimeQuery(node, words) {
      var query = Object.assign({}, node.query || {});
      query.compareMode = signed(words[1]);
      query.target = signed(words[2]);
      if (query.recordKind === 'Q4') query.producerInput = signed(words[3]);
      return Object.assign({}, node, { query: query });
    }

    function installCursorAtPrimitive(primitiveIndex) {
      var node = activeProgram.primitives[primitiveIndex];
      if (!node) {
        state.terminal = true;
        return null;
      }
      var compositeId = activeProgram.compositeByNodeId[node.id];
      var destinationCompositeIndex = compositeIndexById[compositeId];
      if (!Number.isInteger(destinationCompositeIndex)) {
        fail('Director cursor destination ' + node.id +
          ' has no owning composite.', 'cursor-ownership');
      }
      compositeIndex = destinationCompositeIndex;
      compositeEntryNodeId = node.id;
      cursorRevision += 1;
      return node;
    }

    function nextBarrier(node) {
      var startIndex = primitiveIndexById[node.id] + 1;
      for (var index = startIndex; index < activeProgram.primitives.length; index++) {
        var candidate = activeProgram.primitives[index];
        if (candidate.name !== 'branch_barrier') continue;
        var compositeId = activeProgram.compositeByNodeId[candidate.id];
        return {
          node: candidate,
          compositeIndex: compositeIndexById[compositeId],
          resumePrimitiveIndex: index + 1,
          resumeNode: activeProgram.primitives[index + 1] || null
        };
      }
      return null;
    }

    function normalFailureBarrier(node, depth) {
      var startIndex = primitiveIndexById[node.id] + 1;
      var bridgeIndex = -1;
      for (var index = startIndex; index < activeProgram.primitives.length; index++) {
        if (activeProgram.primitives[index].name ===
            'control_bridge_and_pending_substream_handoff') {
          bridgeIndex = index;
          break;
        }
      }
      if (bridgeIndex < 0) return null;
      var remaining = Math.max(1, depth || 1);
      for (var cursor = bridgeIndex - 1; cursor > startIndex; cursor--) {
        var candidate = activeProgram.primitives[cursor];
        if (candidate.name !== 'branch_barrier') continue;
        remaining -= 1;
        if (remaining > 0) continue;
        var compositeId = activeProgram.compositeByNodeId[candidate.id];
        return {
          node: candidate,
          bridgeNode: activeProgram.primitives[bridgeIndex],
          compositeIndex: compositeIndexById[compositeId],
          resumePrimitiveIndex: cursor + 1,
          resumeNode: activeProgram.primitives[cursor + 1] || null
        };
      }
      return null;
    }

    function executeBranchQuery(node, words) {
      var query = runtimeQuery(node, words);
      if (!queryEnabled(query)) {
        recordTrace({
          tick: state.tick,
          kind: 'query-mode-skip',
          nodeId: node.id,
          directorMode: state.directorMode
        });
        return;
      }
      branchDepth += 1;
      var actual = queryActual(query, { kind: 'branch' });
      if (stopReason) return;
      var passes = compare(actual, query.query.compareMode, query.query.target);
      if (passes) {
        recordTrace({
          tick: state.tick,
          kind: 'branch-query',
          nodeId: node.id,
          passed: true,
          actual: actual,
          compareMode: query.query.compareMode,
          target: query.query.target,
          scannerDepth: branchDepth,
          resynchronization: parserResynchronization
        });
        return;
      }
      var destination = parserResynchronization
        ? nextBarrier(node) : normalFailureBarrier(node, branchDepth);
      branchDepth -= 1;
      if (!destination) {
        missing('Failed Director query ' + node.id +
          ' has no following native branch barrier.');
        state.terminal = true;
        return;
      }
      var resumeNode = installCursorAtPrimitive(destination.resumePrimitiveIndex);
      recordTrace({
        tick: state.tick,
        kind: 'branch-query',
        nodeId: node.id,
        passed: false,
        actual: actual,
        compareMode: query.query.compareMode,
        target: query.query.target,
        scannerDepth: branchDepth + 1,
        resynchronization: parserResynchronization,
        destinationNodeId: destination.node.id,
        destinationWord: destination.node.startWord,
        resumeNodeId: resumeNode && resumeNode.id || null,
        resumeWord: resumeNode && resumeNode.startWord || null,
        bridgeNodeId: destination.bridgeNode ? destination.bridgeNode.id : null
      });
    }

    var compositeIndex = 0;
    var compositeEntryNodeId = null;
    var cursorRevision = 0;
    var persistentCursorPrimitiveIndex = 0;
    var block = null;
    var parserResynchronization = false;
    var branchDepth = 0;
    var parserResumeMarked = false;
    var pendingSubstreamSelector = 0xFF;
    var savedStreamFrame = null;

    function activateStream(nextProgram, assetId, primitiveIndex) {
      activeProgram = nextProgram;
      activeStreamAssetId = assetId;
      indexActiveProgram();
      compositeIndex = 0;
      compositeEntryNodeId = null;
      persistentCursorPrimitiveIndex = primitiveIndex;
      return installCursorAtPrimitive(primitiveIndex);
    }

    function consumePendingSubstream(bridgeNode) {
      var selector = pendingSubstreamSelector;
      pendingSubstreamSelector = 0xFF;
      if (selector === 0xFF) return false;
      if (selector === 0xFE) {
        if (!savedStreamFrame) {
          missing('Director continuation return at ' + bridgeNode.id +
            ' has no saved parent stream.');
          state.terminal = true;
          return false;
        }
        var childAssetId = activeStreamAssetId;
        var restored = savedStreamFrame;
        savedStreamFrame = null;
        activateStream(restored.program, restored.assetId,
          restored.persistentCursorPrimitiveIndex);
        var returnDestination = persistentCursorNode();
        recordTrace({
          tick: state.tick,
          kind: 'director-substream-return',
          sourceNodeId: bridgeNode.id,
          childAssetId: childAssetId,
          destinationAssetId: activeStreamAssetId,
          destinationNodeId: returnDestination ? returnDestination.id : null,
          destinationWord: returnDestination ? returnDestination.startWord : null
        });
        return true;
      }
      var childProgram = continuationProgram(selector);
      if (!childProgram) {
        missing('Director continuation selector ' + selector +
          ' at ' + bridgeNode.id + ' could not be materialized from the ROM.');
        return false;
      }
      var parentAssetId = activeStreamAssetId;
      if (!savedStreamFrame) {
        savedStreamFrame = {
          program: activeProgram,
          assetId: activeStreamAssetId,
          persistentCursorPrimitiveIndex: persistentCursorPrimitiveIndex
        };
      }
      activateStream(childProgram, 'director-continuation:' + selector, 0);
      var callDestination = persistentCursorNode();
      recordTrace({
        tick: state.tick,
        kind: 'director-substream-call',
        sourceNodeId: bridgeNode.id,
        selector: selector,
        parentAssetId: parentAssetId,
        childAssetId: activeStreamAssetId,
        destinationNodeId: callDestination ? callDestination.id : null,
        destinationWord: callDestination ? callDestination.startWord : null
      });
      return true;
    }

    function persistentCursorNode() {
      return activeProgram.primitives[persistentCursorPrimitiveIndex] || null;
    }

    function commitPersistentCursorAfter(node) {
      var primitiveIndex = primitiveIndexById[node.id];
      persistentCursorPrimitiveIndex = Number.isInteger(primitiveIndex)
        ? primitiveIndex + 1 : persistentCursorPrimitiveIndex;
      var destination = persistentCursorNode();
      recordTrace({
        tick: state.tick,
        kind: 'parser-resume-commit',
        sourceNodeId: node.id,
        destinationNodeId: destination && destination.id || null,
        destinationWord: destination && destination.startWord || null
      });
    }

    function restartAtPersistentCursor() {
      parserResumeMarked = false;
      return installCursorAtPrimitive(persistentCursorPrimitiveIndex);
    }

    function* beginTick(tick) {
      state.tick = tick;
      parserResynchronization = false;
      branchDepth = 0;
      parserResumeMarked = false;
      pendingSubstreamSelector = 0xFF;
      state.audioEvents = [];
      state.cameraEvents = [];
      state.effectEvents = [];
      state.flowEvents = [];
      applyContextTimeline(tick);
      if (stopReason) return;
      if(dialogueEngine && tick>externalProducers.throughTick && dialogueEngine.owners.some(Boolean)) {
        producerBoundary('Dialogue service history ends before this update.','dialogue-service-history');return;
      }
      yield* applyExternalServices('before-director');
      if(!stopReason)yield* applyResourcePass('before');
      if (stopReason) return;
      if(launchParserRan){parserResynchronization=false;branchDepth=0;parserResumeMarked=false;pendingSubstreamSelector=0xff;launchParserRan=false;}
      if(capturedScheduler){
        if(extendedModeTwoResume && state.registeredCounter && state.registeredCounter.value>=1 && state.registeredCounter.value<=0x0ffffffe)state.registeredCounter.value++;
        try{if(extendedModeTwoResume&&resourceScheduler){capturedScheduler.syncResourceContext(dialogueEngine);capturedScheduler.machine.put(0x800e8100,resourceScheduler.control.actionMask,2);capturedScheduler.machine.put(0x800c4c20,resourceScheduler.input.directorSlot);}state.projectionTransform=yield* capturedScheduler.advance({
          movement:updateMovementJobs,
          dialogueQuery:function(id){if(!dialogueEngine)fail('Dialogue query requires current resource state.','dialogue-initial-input');return dialogueEngine.query(id);},
          actors:function(){Object.keys(state.actors).forEach(function(slot){updateActorPose(state.actors[slot]);});},
          records:function(){return Object.keys(state.actors).map(function(slot){return {slot:Number(slot),bytes:new Uint8Array(nativeRecordForActor(state.actors[slot]).buffer),movement:state.movementJobs[slot]};});}
        });if(Object.keys(state.actors).some(function(slot){return state.actors[slot].poseBlocked;}))fail('Captured Actor progression reached an unavailable program.','resume-pose-input');}
        catch(error){producerBoundary(error.message,error.code||'resume-native-input');return;}
      }else if (tick > 0 || capturedResume || nativeLaunch) updateJobs();
      if(imageEcho&&!state.alternateDirectorScheduling){try{imageEcho.advance();if(imageEcho.started)state.oversizedImageView.zoomState=imageEcho.snapshot().zoomState;}catch(error){producerBoundary(error.message,error.code);}}
      if(iris&&!state.alternateDirectorScheduling){syncIrisTransforms();if(iris.advance())releaseIrisActors();applyIrisLayers();}
      var scheduled = state.scheduled.filter(function(item) { return item.tick === tick; });
      state.scheduled = state.scheduled.filter(function(item) { return item.tick !== tick; });
      scheduled.forEach(function(item) { executePrimitive(item.node); });
    }

    function blockComplete(activeBlock) {
      if (!activeBlock) return true;
      if (activeBlock.kind === 'until') return state.tick >= activeBlock.untilTick;
      if (activeBlock.kind === 'parser-boundary') {
        return state.tick >= activeBlock.untilTick;
      }
      if (activeBlock.kind === 'cursor-replacement') {
        return state.tick >= activeBlock.untilTick;
      }
      if (activeBlock.kind === 'job') return !jobActiveFor(activeBlock.start);
      if (activeBlock.kind === 'query') {
        if (!queryEnabled(activeBlock.query)) return true;
        branchDepth += 1;
        var actual = queryActual(activeBlock.query, { kind: 'wait' });
        if (stopReason) return false;
        var passes = compare(actual, activeBlock.query.query.compareMode,
          activeBlock.query.query.target);
        if (!passes) branchDepth -= 1;
        return passes;
      }
      return true;
    }

    function executeNodes(nodes) {
      var revision = cursorRevision;
      for (var index = 0; index < nodes.length; index++) {
        executePrimitive(nodes[index]);
        if(pendingIris)pendingIris.resumeNodes=nodes.slice(index+1);
        if (block || state.terminal || stopReason || cursorRevision !== revision) break;
      }
    }

    function activateQueryBlock(query, composite, resumeNodes) {
      block = {
        kind: 'query',
        query: query,
        label: composite.label,
        clock: composite.clock,
        resumeNodes: (resumeNodes || []).slice(),
        resumeComposite: composite
      };
      if (!blockComplete(block)) return;
      var completed = block;
      block = null;
      if (completed.resumeNodes.length) {
        processCompositeSuffix(completed.resumeNodes, completed.resumeComposite);
      }
    }

    function processCompositeSuffix(nodes, composite) {
      var revision = cursorRevision;
      for (var index = 0; index < nodes.length; index++) {
        var node = nodes[index];
        if (node.query) {
          activateQueryBlock(node, composite, nodes.slice(index + 1));
          return;
        }
        if (node.name === 'handoff_marker') {
          parserResumeMarked = true;
          continue;
        }
        if (node.name === 'branch_barrier') continue;
        if (node.name === 'control_bridge_and_pending_substream_handoff') {
          if (parserResumeMarked) commitPersistentCursorAfter(node);
          consumePendingSubstream(node);
          block = {
            kind: 'parser-boundary',
            untilTick: state.tick + 1,
            label: composite.label,
            clock: 'director-evaluation'
          };
          return;
        }
        executePrimitive(node);
        if(pendingIris){pendingIris.resumeNodes=nodes.slice(index+1);pendingIris.resumeComposite=composite;}
        if (block || state.terminal || stopReason || cursorRevision !== revision) return;
      }
    }

    function processComposite(composite, entryNodeId) {
      var allNodes = composite.nodeIds.map(function(id) {
        return activeProgram.primitiveById[id];
      });
      var entryOffset = entryNodeId
        ? allNodes.findIndex(function(node) { return node.id === entryNodeId; }) : 0;
      if (entryOffset < 0) {
        fail('Director cursor entry ' + entryNodeId + ' is outside ' + composite.id + '.',
          'cursor-ownership');
      }
      var nodes = allNodes.slice(entryOffset);
      var first = nodes[0];
      recordTrace({
        tick: state.tick,
        kind: 'composite',
        compositeId: composite.id,
        compositeKind: composite.kind,
        label: composite.label,
        category: composite.category
      });
      if (entryOffset > 0) {
        processCompositeSuffix(nodes, composite);
        return;
      }
      if (composite.kind === 'registered-wait') {
        state.registeredCounter = { value: 1, armTick: state.tick };
        block = { kind: 'until', untilTick: state.tick + Math.max(0, composite.nativeTicks),
          label: composite.label, clock: composite.clock };
        return;
      }
      if (composite.kind === 'skippable-registered-wait') {
        if(resourceScheduler){processCompositeSuffix(nodes,composite);return;}
        state.registeredCounter = { value: 1, armTick: state.tick };
        if (!continuousResume) assumption('A-button input is not supplied; skippable waits use their authored maximum.');
        var details = composite.details || {};
        if (details.shape === 'staged-actor-action') {
          (details.actionNodeIds || []).forEach(function(nodeId, index) {
            state.scheduled.push({
              tick: state.tick + Math.max(0, details.openingTargets[index] || 0),
              node: activeProgram.primitiveById[nodeId]
            });
          });
        }
        block = { kind: 'until', untilTick: state.tick + Math.max(0, composite.nativeTicks),
          label: composite.label, clock: composite.clock };
        return;
      }
      if (composite.kind === 'start-and-completion-gate') {
        processCompositeSuffix(nodes, composite);
        return;
      }
      if (composite.kind === 'dialogue-window-open') {
        processCompositeSuffix(nodes, composite);
        return;
      }
      if (composite.kind === 'dialogue-window-resume-close') {
        executeNodes(nodes);
        return;
      }
      if (composite.kind === 'query-envelope') {
        processCompositeSuffix(nodes, composite);
        return;
      }
      executeNodes(nodes);
    }

    function irisLayers() {
      var count=framebufferLayerCount,layers=Array.from({length:20},function(_,i){var v=new DataView(new ArrayBuffer(88)),c=state.transformChannels[i]||identityTransformChannel();[c.translateX,c.translateY,c.rotationX,c.rotationY,c.translateZ,c.uniformScale].forEach(function(n,j){v.setFloat32(64+j*4,n);});return {resource:null,record:new Uint8Array(v.buffer)};});
      (state.background.layers||[]).forEach(function(row){var i=row.nativeOrdinal;if(!Number.isInteger(i)||i<0||i>=count)fail('Framebuffer layers require known native ordinals.','framebuffer-layers');layers[i].resource={kind:'background',assetId:row.assetId,ordinal:i};});
      var selected=state.sceneVignette&&state.sceneVignette.activeSlotByte;
      if(!Number.isInteger(selected)||selected<0||selected>=count)fail('Iris requires its current selected image layer.','framebuffer-layers');
      layers[selected].resource={kind:'vignette',assetId:state.sceneVignette.sourceAssetId};
      return new OB64.cutsceneFramebuffer.Iris(count,selected,layers);
    }
    function syncIrisTransforms(){if(!iris)return;iris.layers.forEach(function(row,i){var c=state.transformChannels[i];if(!c)return;var v=new DataView(row.record.buffer,row.record.byteOffset,88);[c.translateX,c.translateY,c.rotationX,c.rotationY,c.translateZ,c.uniformScale].forEach(function(n,j){v.setFloat32(64+j*4,n);});});}
    function applyIrisLayers(){if(!iris)return;if(state.sceneVignette)state.sceneVignette.activeSlotByte=iris.selected;var rows=OB64.cutsceneFramebuffer.snapshot(iris).layers;state.transformChannels=rows.map(function(row){return row.transform;});}
    function releaseIrisActors(){
      Object.keys(state.actors).forEach(function(slot){var a=launchInitialization.rootAddress+24+Number(slot)*4,p=nativeLaunch.machine.get(a),at=nativeLaunch.leases.findIndex(function(r){return r.address===p;});if(p&&at<0)fail('Iris Actor release lacks allocation ownership.','framebuffer-owner');if(at>=0)nativeLaunch.leases.splice(at,1);nativeLaunch.machine.put(a,0);});
      state.actors={};state.spriteEffects={};
    }
    function finishIris(words,target){try{
      if(!iris)iris=irisLayers();
      var imageId=null,pendingImage=null;
      if((words[7]>>>0)===0){
        if(!target||target.error||target.width!==320||target.height!==240||!(target.rgba instanceof Uint8ClampedArray)||target.rgba.length!==307200||typeof target.targetId!=='string'||!target.targetId)fail(target&&target.error||'Iris requires an identified 320-by-240 product render target.','framebuffer-target');
        if(framebuffers.length>=8||retainedStateBytes+framebufferBytes+307200>maxStateBytes)fail('Framebuffer retention exceeds the playback storage budget.','framebuffer-storage-limit');
        var rgba=new Uint8ClampedArray(target.rgba);for(var i=0;i<rgba.length;i+=4){if(rgba[i+3]!==255)fail('Product framebuffer capture requires an opaque current render target.','framebuffer-target');for(var j=0;j<3;j++)rgba[i+j]=Math.round((rgba[i+j]>>>3)*255/31);rgba[i+3]=255;}
        imageId=framebuffers.length;pendingImage={id:imageId,targetId:target.targetId,pass:state.tick,policy:'constructor-current-state',width:320,height:240,rgba:rgba};
      }
      var actors=Object.keys(state.actors).map(function(slot){return {actor:state.actors[slot],layer:state.actors[slot].transformChannel};}),effects=Object.keys(state.spriteEffects).map(function(slot){return {effect:state.spriteEffects[slot],layer:state.spriteEffects[slot].renderPassSelector};});
      syncIrisTransforms();iris.create(words,imageId,actors,effects);if(pendingImage){framebuffers.push(pendingImage);framebufferBytes+=pendingImage.rgba.length;}actors.forEach(function(row){row.actor.transformChannel=row.layer;});effects.forEach(function(row){row.effect.renderPassSelector=row.layer;syncSpriteEffectPayload(row.effect);});applyIrisLayers();
      recordTrace({tick:state.tick,kind:'framebuffer-iris',phase:words[7],captureId:imageId,targetId:target&&target.targetId||null,capturePolicy:'constructor-current-state'});
    }catch(error){producerBoundary(error.message,error.code||'framebuffer-input');}}
    function* capturePendingIris() {
      while (pendingIris && !stopReason) {
        var request = pendingIris;
        pendingIris = null;
        var target = yield {
          kind: 'framebuffer-capture', policy: 'constructor-current-state',
          pass: state.tick, nodeId: request.node.id,
          backgroundPolicy: framebufferProfile.backgroundPolicy || 'require',
          preview: snapshot(null)
        };
        block = null;
        finishIris(request.words, target);
        if (!stopReason && request.resumeNodes && request.resumeNodes.length) {
          if (request.resumeComposite) processCompositeSuffix(request.resumeNodes, request.resumeComposite);
          else executeNodes(request.resumeNodes);
        }
      }
    }
    function* evaluateDirector() {
      if (!stopReason && block && blockComplete(block)) {
        var completedBlock = block;
        if (completedBlock.kind === 'until') {
          state.registeredCounter = null;
          branchDepth = 1;
        }
        block = null;
        if (completedBlock.kind === 'parser-boundary') {
          restartAtPersistentCursor();
        } else if (completedBlock.resumeNodes && completedBlock.resumeNodes.length) {
          processCompositeSuffix(completedBlock.resumeNodes,
            completedBlock.resumeComposite);
        }
      }
      yield* capturePendingIris();
      var instantGuard = 0;
      while (!block && !state.terminal && !stopReason &&
          compositeIndex < activeProgram.composites.length) {
        if (++instantGuard > activeProgram.composites.length + 8) {
          fail('Director runtime exceeded its instantaneous-dispatch guard.', 'dispatch-loop');
        }
        var composite = activeProgram.composites[compositeIndex++];
        var entryNodeId = compositeEntryNodeId;
        compositeEntryNodeId = null;
        processComposite(composite, entryNodeId);
        yield* capturePendingIris();
        if (dispatchCount >= maxDispatches) stopReason = 'dispatch-limit';
        yield;
      }
    }
    function syncLaunchCamera() {
      var m=dialogueEngine.machine;
      ['actor','registered'].forEach(function(bank,index){var c=projectionFromCamera(state.cameras[bank]),at=0x8022a720+(index?88:0),values=[c.fovYDegrees,c.aspect,c.near,c.far,null,c.eye.x,c.eye.y,c.eye.z,c.target.x,c.target.y,c.target.z,c.up.x,c.up.y,c.up.z];
        values.forEach(function(value,i){if(value===null)return;var b=new DataView(new ArrayBuffer(4));b.setFloat32(0,value);m.put(at+i*4,b.getUint32(0));});});
    }
    function* serviceColorResource(slot,kind) {
      syncLaunchCamera();var m=dialogueEngine.machine,owner=dialogueEngine.owners[slot];
      if(!owner)fail('Color resource ownership is missing.','director-launch-color');
      yield* dialogueEngine.service({service:kind,slot:slot,ownerId:owner.ownerId,eligible:true,helpers:[],controller:Object.assign({},resourceScheduler.control,{queueHead:m.get(0x800c4c10,2)})},true,{initialize:0x8022643c,callback:0x80226538,length:92});
      state.overlay=(nativeLaunch||capturedScheduler).color();state.overlayJob=state.overlay;
    }
    function* restoreDirectorResource() {
      var e=dialogueEngine,m=e.machine,slot=resourceScheduler.input.directorSlot,a=0x800e82c8+slot*168,owner=e.owners[slot];e.copy(a,0x800e7a30,168);
      if(!owner||!owner.payload||owner.payload.length!==92)fail('Fresh Director callback requires its initialized payload.','director-launch-payload');
      e.payloadStorage.restore(owner.ownerId,m.get(a+0x24),0x800e91d0);
    }
    function* saveDirectorResource() {
      var e=dialogueEngine,m=e.machine,slot=resourceScheduler.input.directorSlot,a=0x800e82c8+slot*168,owner=e.owners[slot];
      var handle=e.payloadStorage.save(owner.ownerId,m.get(0x800e7a31,1),0x800e91d0,92);m.put(0x800e7a54,handle);e.copy(0x800e7a30,a,168);owner.payload=Uint8Array.from({length:92},function(_,i){return m.get(0x800e91d0+i,1);});
    }
    function* initializeDirectorResource(slot) {
      var e=dialogueEngine,m=e.machine,a=0x800e82c8+slot*168;
      if(m.get(a+0x10)!==(nativeLaunch.input.sceneMode===2?0x802260f0:0x80225a1c))fail('Fresh launch requires the qualified Director initializer.','director-launch-binding');
      m.put(0x800c4c20,slot);
      var record=Uint8Array.from({length:168},function(_,i){return m.get(a+i,1);});
      record[4]=6;
      if(slot===m.get(0x800c4c10,2))record[2]|=4;
      yield* nativeLaunch.initialize(record);
      launchInitialization=nativeLaunch.snapshot();
      state.directorMode=nativeLaunch.input.sceneMode;
      if(state.directorMode===2)state.projectionTransform=OB64.cutsceneRomStart.projectionState(nativeLaunch);state.directorModeStatus='computed-native-launch';
      var camera=launchBytes(launchInitialization.cameraHex,144);
      ['actor','registered'].forEach(function(bank,index){var at=index?88:0,values=Array.from({length:14},function(_,i){return camera.getFloat32(at+i*4);});
        state.cameras[bank]=cameraFromProjection({fovYDegrees:values[0],aspect:values[1],near:values[2],far:values[3],modelScale:nativeLaunch.input.sceneMode===2?new DataView(nativeLaunch.read(0x801ce8e4,4).buffer).getFloat32(0):bank==='actor'?1:Math.fround(0.1),eye:{x:values[5],y:values[6],z:values[7]},target:{x:values[8],y:values[9],z:values[10]},up:{x:values[11],y:values[12],z:values[13]},sourceNodeId:'rom-director-initializer',evidenceStatus:'computed-native-initializer'},'Computed camera from native initialization and declared preserved fields.','rom-director-initializer');});
      var context=launchBytes(launchInitialization.contextHex,30);state.sceneColor={red:context.getUint16(0),green:context.getUint16(2),blue:context.getUint16(4)};
      recordTrace({tick:state.tick,kind:'director-launch-initializer',selector:nativeLaunch.input.selector,resourceKey:nativeLaunch.resourceKey,rootAddress:launchInitialization.rootAddress});
      for(var j=0;j<168;j++)m.put(0x800e7a30+j,parseInt(launchInitialization.recordHex.slice(j*2,j*2+2),16),1);
      yield* evaluateDirector();launchParserRan=true;
      syncLaunchCamera();
      launchInitialization.initialParser={actors:Object.keys(state.actors).map(function(slot){return {slot:Number(slot),values:(function(a){return [a.x,a.y,a.z,a.bank,a.poseCursor,a.poseDelay,a.displayedFrameToken,a.poseStateIndex,a.animationKey,a.nativeFacing];})(state.actors[slot])};}),cameraHex:recordHex(new DataView(nativeLaunch.readEngine(0x8022a720,144).buffer)),blockedQuery:block&&block.query?block.query.id:null};
      if(block&&['parser-boundary','cursor-replacement'].includes(block.kind))block.untilTick=state.tick;
      var result=new DataView(Uint8Array.from({length:168},function(_,i){return m.get(0x800e7a30+i,1);}).buffer);result.setUint16(0,result.getUint16(0)|0x2000);if(slot===m.get(0x800c4c10,2))result.setUint8(2,result.getUint8(2)&~4);
      for(var i=0;i<92;i++)m.put(0x800e91d0+i,nativeLaunch.machine.get(0x800e91d0+i,1),1);
      var owner=e.owners[slot],handle=e.payloadStorage.save(owner.ownerId,result.getUint8(1),0x800e91d0,92);result.setUint32(0x24,handle);
      for(i=0;i<168;i++)m.put(a+i,result.getUint8(i),1);owner.payload=Uint8Array.from({length:92},function(_,i){return m.get(0x800e91d0+i,1);});
    }

    if (capturedSnapshot) {
      state.directorMode = capturedSnapshot.sceneMode;
      if (capturedPresentation) {
        ['actor', 'registered'].forEach(function(bank) {
          var source=capturedPresentation[bank+'Camera'], a=source.values;
          state.cameras[bank]=cameraFromProjection({fovYDegrees:a[0],aspect:a[1],near:a[2],far:a[3],
            modelScale:source.modelScale,eye:{x:a[5],y:a[6],z:a[7]},target:{x:a[8],y:a[9],z:a[10]},
            up:{x:a[11],y:a[12],z:a[13]},screenWidth:320,screenHeight:240,evidenceStatus:'qualified-captured-main-actor'},
            'Qualified captured main-Actor camera; native rounding and image agreement remain separate.');
        });
        state.transformChannels=capturedPresentation.channels.map(function(channel){return channel ? Object.assign(identityTransformChannel(),channel) : identityTransformChannel();});
      } else {
        missing('Captured main-Actor camera and scene transforms are unknown; initializer geometry is not captured presentation.');
      }
      state.directorModeStatus = 'captured-snapshot';
      if (!capturedResume) {
        stopReason = 'captured-snapshot-resume-input';
        missing('Static captured snapshot only: native scheduler phase, resume state, other job owners, inherited menu ownership, and service history are not qualified. No Director or Actor update ran.');
      } else {
        if (contextRuntime) fail('Captured resume requires an isolated qualified Director context.','resume-context-input');
        var resumeIndex=activeProgram.primitives.findIndex(function(node){return node.startWord===capturedSnapshot.observedParserCursor;});
        var resumeNode=activeProgram.primitives[resumeIndex];
        if (!resumeNode || resumeNode.name!=='actor_movement_countdown_query' || !resumeNode.query || !queryEnabled(resumeNode)) {
          fail('Resume requires a native parser cursor at an enabled movement query.','resume-transition-input');
        }
        var resumeSlot=resumeNode.query.producerInput, resumeJob=state.movementJobs[resumeSlot];
        var predictedJob=resumeJob && Object.assign({},resumeJob);
        var predictedActor=state.actors[resumeSlot] && Object.assign({},state.actors[resumeSlot]);
        var predictedAlive=predictedJob && predictedActor && advanceNativeMovement(predictedActor,predictedJob);
        var predictedValue=predictedAlive ? lowS16(predictedJob.remaining) : 0;
        if (!continuousResume && compare(predictedValue,resumeNode.query.compareMode,resumeNode.query.target)) {
          fail('A passing movement query requires qualification of the following Director effects.','resume-transition-input');
        }
        if(heldModeTwoResume){
          if((externalProducers && !capturedServices) || nativeLaunch || launchValue('directActorCreates'))fail('A held native scheduling window cannot combine another service timeline or Actor constructor.','resume-context-input');
          if(!resumeJob || resumeJob.pauseByte || (!extendedModeTwoResume && capturedResume.updates>=lowS16(resumeJob.remaining)) ||
              resumeNode.query.compareMode!==0 || resumeNode.query.target!==0)fail('Mode-two continuation requires a bounded nonpassing zero-count movement query.','resume-transition-input');
          if(!OB64.cutsceneCapturedScheduler)fail('Captured native scheduler is unavailable.','resume-native-input');
          capturedScheduler=new OB64.cutsceneCapturedScheduler(options.z64,capturedResume,capturedSnapshot);
          if(capturedServices&&resourceScheduler){capturedScheduler.attachColorResources(dialogueEngine,capturedServices.colorArena,options.z64);resourceScheduler.colorService=serviceColorResource;resourceScheduler.beforeDirector=function*(){dialogueEngine.copy(0x800e82c8+resourceScheduler.input.directorSlot*168,0x800e7a30,168);};}
          recordTrace({tick:0,kind:'captured-native-memory',bytes:capturedScheduler.memoryBytes});
        }
        persistentCursorPrimitiveIndex=resumeIndex;
        installCursorAtPrimitive(resumeIndex);
        state.directorModeStatus=continuousResume ? 'candidate-continuous-resume' : 'qualified-prospective-resume';
        recordTrace({tick:0,kind:'captured-resume-entry',entry:capturedResume.entry,parserWord:capturedSnapshot.observedParserCursor});
      }
    }
    for (var tick = 0; tick < maxTicks; tick++) {
      yield;
      if (!capturedSnapshot || capturedResume) yield* beginTick(tick);
      yield* evaluateDirector();
      if(capturedScheduler&&extendedModeTwoResume&&!stopReason){try{capturedScheduler.assertActors(true);}catch(error){producerBoundary(error.message,error.code);}}
      if (!stopReason) advanceMapMenu();
      if (!stopReason) yield* applyExternalServices('after-director');
      if (!stopReason) yield* applyResourcePass('after');
      if (!stopReason && (capturedServices||romOnlyStart) && options.stopAtInputWait!==false && block && block.kind==='query' && block.query.name==='dialogue_pause_query') {
        var waitSlot=dialogueEngine.find(block.query.query.producerInput),waitOwner=waitSlot>=0&&dialogueEngine.owners[waitSlot];
        var waitState=waitOwner&&waitOwner.payload&&waitOwner.payload[0x3c];
        var controls=resourceScheduler.input.controller.changes;
        if(resourceScheduler.input.pageAdvancePolicy!=='automatic' && [3,6].includes(waitState) && controls.filter(function(c){return c.pass>=state.tick;}).every(function(c){return c.actionMask===0&&c.historyMask===0&&c.directionMask===0&&c.dummyMask===0;}) && !resourceScheduler.control.actionMask && !resourceScheduler.control.historyMask){
          stopReason='awaiting-dialogue-input';unresolvedQuery={kind:'user-input',code:'dialogue-page-acknowledgement',nodeId:block.query.id,windowId:block.query.query.producerInput,label:'Press A to advance the dialogue page.',nativeState:waitState};
        }
      }
      if (!stopReason) yield* prepareSharedActors();
      if (continuousResume && !stopReason) {
        completedResumeUpdates++;
        if (completedResumeUpdates >= capturedResume.updates && !state.terminal) stopReason='prospective-update-limit';
      }
      if (capturedResume && !continuousResume && !stopReason) {
        if (!block || block.kind!=='query' || block.query.id!==resumeNode.id ||
            Object.keys(state.actors).some(function(slot){return !!state.actors[slot].poseBlocked;})) {
          fail('The resumed transition exceeded the qualified held-query boundary.','resume-transition-input');
        }
        // Inactive owner guards and the held query preserve selection; only then resolve the menu list.
        resumedMenuSelection={phase:'after-director',rootAddress:capturedResume.menuRootAddress,
          rootHex:capturedResume.menuRootHex,menuOwners:M.cloneJson(capturedResume.menuOwners),
          selectedEntity:null,outcome:'empty-selected-list',preservation:'guarded-normal-pass-and-held-query'};
        recordTrace({tick:tick,kind:'captured-resume-query',nodeId:resumeNode.id,
          actual:queryActual(block.query,{kind:'wait'}),target:block.query.query.target,held:true});
        recordTrace({tick:tick,kind:'resumed-menu-selection',phase:'after-director',selectedEntity:null});
        stopReason='qualified-resume-update-complete';
      }
      var frameBudget = { bytes: 0 };
      var nextSnapshot=snapshot(block);
      if(sharedActorProfile||romOnlyStart){nextSnapshot.actors=compactRecords(nextSnapshot.actors);nextSnapshot.effects=compactRecords(nextSnapshot.effects);}
      var frameState = shareSnapshot(states[states.length - 1], nextSnapshot, frameBudget);
      if(sharedActorProfile||romOnlyStart)['actors','effects'].forEach(function(k){frameState[k].forEach(function(row){Object.freeze(row.values);Object.freeze(row);});});
      var frameBytes = frameBudget.bytes;
      if (retainedStateBytes + framebufferBytes + frameBytes > maxStateBytes) {
        if (!states.length) fail('The first Director snapshot exceeds the storage budget.', 'state-storage-limit');
        stopReason = 'state-storage-limit';
        break;
      }
      states.push(frameState);
      retainedStateBytes += frameBytes;
      if (stopReason) break;
      if (state.terminal || compositeIndex >= activeProgram.composites.length && !block &&
          !Object.keys(state.movementJobs).length && !state.projectionJob &&
          !state.oversizedImageTransitionJob) break;
    }

    if (!state.terminal && states.length >= maxTicks && !(capturedResume && resumedMenuSelection) && (!continuousResume || !stopReason)) {
      stopReason = 'tick-limit';
      missing('Director preview reached the ' + maxTicks + '-tick safety limit.');
    }
    if (!states.length) states.push(snapshot(null));
    var previewFitBounds = null;
    states.forEach(function(frameState) {
      frameState.actors.forEach(function(actor) {
        if (!actor.visible || !Number.isFinite(actor.x) || !Number.isFinite(actor.z)) return;
        if (!previewFitBounds) {
          previewFitBounds = { xMin: actor.x, xMax: actor.x, zMin: actor.z, zMax: actor.z };
          return;
        }
        previewFitBounds.xMin = Math.min(previewFitBounds.xMin, actor.x);
        previewFitBounds.xMax = Math.max(previewFitBounds.xMax, actor.x);
        previewFitBounds.zMin = Math.min(previewFitBounds.zMin, actor.z);
        previewFitBounds.zMax = Math.max(previewFitBounds.zMax, actor.z);
      });
    });
    states.forEach(function(frameState) {
      frameState.durationFrames = states.length;
      frameState.runtime = Object.assign({}, frameState.runtime);
      frameState.runtime.assumptionCount = assumptions.length;
      frameState.runtime.missingInputCount = missingInputs.length;
      frameState.runtime.status = missingInputs.length ? 'missing-inputs' :
        (assumptions.length ? 'assumed-inputs' : 'profiled');
      if (previewFitBounds && frameState.actorProjection &&
          frameState.actorProjection.evidenceStatus === 'external-unresolved') {
        // The native launch camera is absent, so keep one honest preview fit
        // across the complete runtime instead of clamping moving Actors against
        // the static SceneDocument bounds or reframing every tick.
        frameState.actorProjection = Object.assign({}, frameState.actorProjection, { previewFitBounds: previewFitBounds });
      }
    });

    return {
      assetId: scene.assetId,
      engine: 'director-scheduler',
      directorMode: state.directorMode,
      directorModeStatus: state.directorModeStatus,
      directorInitialization: launchInitialization,
      clockUnit: resourceScheduler ? 'declared resource passes with explicit projection preparation; not video frames' : continuousResume ? 'declared prospective normal Director updates; no historical cadence' : capturedResume ? 'one prospective native Director update; no historical cadence' : capturedSnapshot ? 'captured snapshot; no elapsed update' : 'native scheduler update',
      capturedSnapshot: capturedSnapshot ? { observedParserCursor: capturedSnapshot.observedParserCursor,
        resumeState: continuousResume ? 'candidate-continuous-updates' : capturedResume ? 'qualified-prospective-update' : 'unknown', otherJobOwners: 'unknown',
        executedUpdates: continuousResume ? completedResumeUpdates : capturedResume && resumedMenuSelection ? 1 : 0 } : null,
      resumedMenuSelection: resumedMenuSelection,
      resumedState: capturedResume && resumedMenuSelection ? {
        parserWord: capturedSnapshot.observedParserCursor, registeredCounter: state.registeredCounter ? state.registeredCounter.value : 0,
        movementSlots: Array.from({length:28},function(_,slot){var job=state.movementJobs[slot];return job ?
          {vx:job.vx,vz:job.vz,remaining:job.remaining,pauseByte:job.pauseByte,elapsed:job.elapsed} : null;})
      } : null,
      durationTicks: states.length,
      states: states,
      assumptions: assumptions,
      missingInputs: missingInputs,
      trace: trace,
      programsByAssetId: programsByAssetId,
      outcome: stopReason || (state.terminal
        ? (missingInputs.length ? 'modeled-ending-with-missing-inputs' : state.terminalReason === 'presentation-reload-handoff' ? 'modeled-handoff' : 'modeled-termination')
        : 'stream-exhausted'),
      pendingWait: block ? { kind: block.kind, label: block.label } : null,
      unresolvedQuery: unresolvedQuery,
      unsupportedCommands: unsupportedCommands,
      inputPolicy: options.diagnosticAssumptions === true ? 'diagnostic-assumptions' : 'stop-at-missing-input',
      limits: { maxTicks: maxTicks, maxStateBytes: maxStateBytes,
        maxTraceEntries: maxTraceEntries, maxDispatches: maxDispatches, maxFramebuffers:8, maxFramebufferBytes:2457600 },
      retainedStateBytes: retainedStateBytes+framebufferBytes,
      framebuffers:framebuffers,
      traceCount: traceCount,
      traceTruncated: traceCount > trace.length,
      executedNodeIds: state.executedNodeIds.slice(),
      sourcePrimitiveCount: rootProgram.primitives.length,
      executedPrimitiveCount: dispatchCount,
      concurrentContext: contextRuntime ? {
        assetId: contextRuntime.assetId,
        tickOffset: contextTickOffset,
        evidenceStatus: 'native-static-parent-event-request-order'
      } : null,
      launchSceneStatePolicy: modeTwoCommandPreviewUsesFreshRoot
        ? 'mode-two-zero-loader-preview-clears-scene-root'
        : 'launch-may-inherit-existing-scene-root',
      nativeLaunchInputs: launchInputs,
      nativeRosterResult: nativeRosterResult,
      launchStageTransform: M.cloneJson(
        launchProfile.stageTransform, 'launch Stage transform profile'),
      launchOperandTranslation: {
        required: translationProfile.required === true,
        requiredIndexes: (translationProfile.tableIndexes || []).slice(),
        suppliedIndexes: suppliedTranslationIndexes.slice(),
        missingIndexes: missingTranslationIndexes.slice(),
        status: missingTranslationIndexes.length ? 'missing-inputs' :
          (translationProfile.required ? 'resolved' : 'not-required')
      },
      terminated: state.terminal,
      terminationReason: state.terminalReason,
      safetyLimited: /-limit$/.test(stopReason || '')
    };
  }

  function compactContextRuntime(runtime) {
    if (!runtime) fail('A Director runtime is required for context compaction.',
      'context-runtime');
    if(runtime.framebuffers&&runtime.framebuffers.length)fail('Framebuffer playback cannot yet become a concurrent Director context.','framebuffer-context');
    if (Array.isArray(runtime.contextFrames) && runtime.contextFrames.length) {
      return runtime;
    }
    if (!Array.isArray(runtime.states) || !runtime.states.length) {
      fail('The Director runtime has no states to compact.', 'context-runtime');
    }
    var actorFields = [
      'id', 'label', 'artSourceId', 'capability', 'visible',
      'opacityByte', 'renderModeByte', 'baseX', 'baseY', 'baseZ',
      'secondaryY', 'heightModeByte', 'facing', 'poseId', 'bank',
      'animationKey', 'nativeFacing', 'variantSelector', 'poseFrame',
      'displayedFrameToken', 'poseCursor', 'poseDelay', 'poseStateIndex',
      'poseBlocked', 'decoderMode', 'sourceRowOrdinal', 'material', 'materialDelta',
      'poseProgramStatus', 'poseLoop', 'poseDuration', 'bodyPoseProgram',
      'movementFrame', 'activeMovementId', 'nativeUniformScale', 'tint',
      'yawDegrees', 'transformChannel', 'source'
    ];

    function same(left, right) {
      if (left === right) return true;
      if (left == null || right == null ||
          typeof left !== 'object' || typeof right !== 'object') return false;
      return JSON.stringify(left) === JSON.stringify(right);
    }

    function bySlot(actors) {
      var output = {};
      (actors || []).forEach(function(actor) { output[actor.slot] = actor; });
      return output;
    }

    function dialogueWindowId(row) {
      var payload = row && row.payload || {};
      var match = String(row && row.label || '').match(/(\d+)$/);
      return Number.isInteger(payload.windowId)
        ? payload.windowId : Number(match && match[1]);
    }

    function byWindow(rows) {
      var output = {};
      (rows || []).forEach(function(row) {
        var windowId = dialogueWindowId(row);
        if (Number.isInteger(windowId)) output[windowId] = row;
      });
      return output;
    }

    var contextFrames = [];
    var prior = null;
    runtime.states.forEach(function(storedFrame) {
      var frameState=plainRetainedFrame(storedFrame);
      var delta = {};
      var currentActors = bySlot(frameState.actors);
      var priorActors = bySlot(prior && prior.actors);
      var actorChanges = [];
      Object.keys(currentActors).forEach(function(slot) {
        var current = currentActors[slot];
        var previous = priorActors[slot];
        if (!previous) {
          actorChanges.push(current);
          return;
        }
        var actorDelta = { slot: current.slot };
        actorFields.forEach(function(field) {
          if (!same(current[field], previous[field])) actorDelta[field] = current[field];
        });
        if (current.nativeActorState && current.nativeActorState !== previous.nativeActorState) {
          var currentNative = decodeNativeActorState(current.nativeActorState);
          var previousNative = previous.nativeActorState ? decodeNativeActorState(previous.nativeActorState) : {};
          Object.keys(currentNative).forEach(function(field) {
            if (!same(currentNative[field], previousNative[field])) actorDelta[field] = currentNative[field];
          });
        }
        if (Object.keys(actorDelta).length > 1) actorChanges.push(actorDelta);
      });
      if (actorChanges.length) delta.actors = actorChanges;
      var removedActorSlots = Object.keys(priorActors).filter(function(slot) {
        return !currentActors[slot];
      }).map(Number);
      if (removedActorSlots.length) delta.removedActorSlots = removedActorSlots;

      var currentDialogue = byWindow(frameState.dialogue);
      var priorDialogue = byWindow(prior && prior.dialogue);
      var dialogueChanges = Object.keys(currentDialogue).filter(function(windowId) {
        return !priorDialogue[windowId] ||
          !same(currentDialogue[windowId], priorDialogue[windowId]);
      }).map(function(windowId) { return currentDialogue[windowId]; });
      if (dialogueChanges.length) delta.dialogue = dialogueChanges;
      var removedDialogueWindowIds = Object.keys(priorDialogue).filter(function(windowId) {
        return !currentDialogue[windowId];
      }).map(Number);
      if (removedDialogueWindowIds.length) {
        delta.removedDialogueWindowIds = removedDialogueWindowIds;
      }

      [
        'background', 'transformChannels', 'cameraState', 'actorProjection',
        'registeredProjection', 'sceneColor', 'overlays', 'sceneVignette',
        'oversizedImageView', 'nativeActorDrawing'
      ].forEach(function(field) {
        if (!prior || !same(frameState[field], prior[field])) {
          delta[field] = frameState[field];
        }
      });
      contextFrames.push(Object.keys(delta).length ? delta : null);
      prior = frameState;
    });
    return {
      assetId: runtime.assetId,
      engine: 'director-context-delta-timeline',
      durationTicks: runtime.durationTicks,
      contextFrames: contextFrames,
      terminated: runtime.terminated,
      safetyLimited: runtime.safetyLimited,
      outcome: runtime.outcome,
      sourceRuntimeEngine: runtime.engine,
      evidenceStatus: 'lossless-runtime-state-delta'
    };
  }

  function bind(document, runtime) {
    if (!bindings) fail('This environment cannot bind Director runtime state.', 'weak-map');
    if (!document || !runtime) fail('A SceneDocument and runtime are required.', 'binding');
    bindings.set(document, runtime);
    return runtime;
  }

  function unbind(document) { if (bindings && document) bindings.delete(document); }
  function forDocument(document) { return bindings && document ? bindings.get(document) || null : null; }

  function evaluate(runtime, requestedTick) {
    if (!runtime || !Array.isArray(runtime.states) || !runtime.states.length) {
      fail('Compiled Director runtime state is unavailable.', 'invalid-runtime');
    }
    if (!Number.isInteger(requestedTick)) fail('Director tick must be an integer.', 'invalid-tick');
    var tick = clamp(requestedTick, 0, runtime.states.length - 1);
    var output = M.cloneJson(plainRetainedFrame(runtime.states[tick]), 'runtime state');
    if(runtime.framebuffers&&runtime.framebuffers.length)output.framebuffers=runtime.framebuffers;
    output.frame = tick;
    output.timeSeconds = tick / M.previewFps;
    output.durationFrames = runtime.states.length;
    return output;
  }

  OB64.cutsceneRuntime = Object.freeze({
    RuntimeError: RuntimeError,
    defaultMaxTicks: DEFAULT_MAX_TICKS,
    compile: compile,
    compileAsync: compileAsync,
    compactContextRuntime: compactContextRuntime,
    bind: bind,
    unbind: unbind,
    forDocument: forDocument,
    evaluate: evaluate,
    projectionFromCamera: projectionFromCamera,
    decodeSceneTransformResource: decodeSceneTransformResource,
    validateLaunchInputs: validateLaunchInputs,
    decodeNativeActorState: decodeNativeActorState,
    nativeActor: Object.freeze({ createMovement: createNativeMovement,
      advanceMovement: advanceNativeMovement, advancePose: advanceNativePose,
      classFamilyMatch: nativeClassFamilyMatch }),
    nativeExternal: Object.freeze({advanceMenu:advanceNativeMenu,createColor:createNativeColor,
      advanceColor:advanceNativeColor,cleanupColor:cleanupNativeColor,selectSharedRequest:selectNativeSharedRequest})
  });
})(window.OB64);
