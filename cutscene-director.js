// Lordly Caliber - lossless Director program and composite-action model.
//
// Native words remain owned by cutscene-codec.js. This module interprets the
// corrected 153-command metadata and builds non-destructive source groupings.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var LANE_ORDER = Object.freeze([
    'flow', 'actors', 'dialogue', 'view', 'effects', 'audio',
    'resources', 'director-state'
  ]);

  var LANE_LABELS = Object.freeze({
    flow: 'Flow and waits',
    actors: 'Actors',
    dialogue: 'Dialogue presentation',
    view: 'Camera and projection',
    effects: 'Effects and color',
    audio: 'Audio',
    resources: 'Scene resources',
    'director-state': 'Director state'
  });

  function DirectorProgramError(message, code) {
    this.name = 'DirectorProgramError';
    this.message = message;
    this.code = code || 'director-program';
  }
  DirectorProgramError.prototype = Object.create(Error.prototype);
  DirectorProgramError.prototype.constructor = DirectorProgramError;

  function fail(message, code) {
    throw new DirectorProgramError(message, code);
  }

  function unsigned(value) { return Number(value) >>> 0; }
  function signed(value) { return unsigned(value) | 0; }

  function title(name) {
    return String(name || '').split('_').filter(Boolean).map(function(part) {
      return part.charAt(0).toUpperCase() + part.slice(1);
    }).join(' ');
  }

  function categoryFor(name) {
    name = String(name || '');
    if (/query|branch|barrier|bridge|counter|marker|substream|termin|handoff/.test(name)) {
      return 'flow';
    }
    if (/dialogue|text_display|portrait/.test(name)) return 'dialogue';
    if (/camera|projection|pan_zoom|screen_edge|iris/.test(name)) return 'view';
    if (/actor|body_pose/.test(name)) return 'actors';
    if (/audio/.test(name)) return 'audio';
    if (/sprite|effect|overlay|tint|color|opacity|ribbon|echo/.test(name)) {
      return 'effects';
    }
    if (/background|resource|scene_image|oversized_image/.test(name)) return 'resources';
    return 'director-state';
  }

  function clockFor(name) {
    name = String(name || '');
    if (/registered_counter/.test(name)) return 'registered-counter-updates';
    if (/actor_movement|actor_move/.test(name)) return 'actor-motion-updates';
    if (/facing_turn/.test(name)) return 'actor-turn-updates';
    if (/projection_transform/.test(name)) return 'projection-updates';
    if (/dialogue/.test(name)) return 'dialogue-handshake';
    if (/overlay|effect|ribbon|echo|iris/.test(name)) return 'effect-updates';
    return 'director-evaluation';
  }

  function primitiveFromIrNode(irNode) {
    var definition = irNode && irNode.definition;
    var words = irNode && irNode.currentWords;
    if (!definition || !Array.isArray(words) || !words.length) {
      fail('Director IR node is missing its definition or source words.', 'invalid-node');
    }
    if (words.length !== definition.wordCount ||
        definition.endWord - definition.startWord !== definition.wordCount) {
      fail(definition.id + ' does not match its corrected source width.', 'width-mismatch');
    }
    var opcode = unsigned(words[0]);
    if (opcode !== unsigned(definition.opcodeU32)) {
      fail(definition.id + ' opcode does not match the corrected corpus.', 'opcode-mismatch');
    }
    var roles = definition.operandRoles || [];
    if (roles.length !== words.length - 1) {
      fail(definition.id + ' operand roles do not cover its source words.', 'operand-role-mismatch');
    }
    var operands = words.slice(1).map(function(word, index) {
      return {
        role: roles[index],
        rawU32: unsigned(word),
        signed: signed(word)
      };
    });
    var query = null;
    if (definition.queryRecordKind) {
      var expected = definition.queryRecordKind === 'Q4' ? 4 : 3;
      if (words.length !== expected) {
        fail(definition.id + ' has an invalid ' + definition.queryRecordKind +
          ' query width.', 'query-width');
      }
      query = {
        recordKind: definition.queryRecordKind,
        compareMode: signed(words[1]),
        target: signed(words[2]),
        producerInput: definition.queryRecordKind === 'Q4' ? signed(words[3]) : null
      };
    }
    return {
      id: definition.id,
      index: -1,
      startWord: definition.startWord,
      endWord: definition.endWord,
      wordCount: definition.wordCount,
      opcode: opcode,
      opcodeHex: '0x' + opcode.toString(16).toUpperCase().padStart(8, '0'),
      name: definition.name,
      label: title(definition.name),
      summary: definition.semanticSummary,
      confidence: definition.confidence,
      category: categoryFor(definition.name),
      clock: clockFor(definition.name),
      runtimeReachability: definition.runtimeReachability,
      editPolicy: definition.editPolicy,
      insertBefore: definition.insertBefore === true,
      query: query,
      terminationKind: definition.terminationKind,
      operands: operands,
      rawWords: words.map(unsigned)
    };
  }

  function makeComposite(kind, label, nodes, options) {
    options = options || {};
    var first = nodes[0], last = nodes[nodes.length - 1];
    return {
      id: options.id || 'composite:' + kind + ':' + first.id,
      kind: kind,
      label: label,
      summary: options.summary || nodes.map(function(node) { return node.summary; }).join(' '),
      category: options.category || first.category,
      confidence: options.confidence || 'Structural',
      clock: options.clock || first.clock,
      startWord: first.startWord,
      endWord: last.endWord,
      wordCount: last.endWord - first.startWord,
      nodeIds: nodes.map(function(node) { return node.id; }),
      nodeCount: nodes.length,
      nativeTicks: options.nativeTicks == null ? null : options.nativeTicks,
      editable: options.editable === true,
      source: options.source || 'detected-composite',
      details: options.details || {}
    };
  }

  function sameContiguousNodes(nodes) {
    for (var index = 1; index < nodes.length; index++) {
      if (nodes[index - 1].index + 1 !== nodes[index].index ||
          nodes[index - 1].endWord !== nodes[index].startWord) return false;
    }
    return true;
  }

  function envelopeAt(primitives, startIndex) {
    var index = startIndex;
    var nodes = [];
    if (primitives[index] && primitives[index].name === 'handoff_marker') {
      nodes.push(primitives[index++]);
    } else {
      return null;
    }
    while (primitives[index] && primitives[index].name === 'branch_barrier') {
      nodes.push(primitives[index++]);
    }
    if (!primitives[index] ||
        primitives[index].name !== 'control_bridge_and_pending_substream_handoff') {
      return null;
    }
    nodes.push(primitives[index++]);
    if (!primitives[index] || !primitives[index].query) return null;
    nodes.push(primitives[index]);
    return { nodes: nodes, query: primitives[index], endIndex: index };
  }

  function actorSlot(primitive) {
    return primitive && primitive.operands.length ? primitive.operands[0].signed : null;
  }

  function transitionLabel(start, query) {
    var slot = actorSlot(start);
    if (start.name === 'actor_move') return 'Move actor slot ' + slot + ' until complete';
    if (start.name === 'actor_facing_turn_transition') {
      return 'Turn actor slot ' + slot + ' until complete';
    }
    if (start.name === 'actor_rgb_tint_transition') {
      return 'Tint actor slot ' + slot + ' until complete';
    }
    if (start.name === 'actor_body_pose_program_start') {
      return 'Run actor slot ' + slot + ' pose program until complete';
    }
    if (/projection_transform/.test(start.name)) {
      return 'Animate scene projection until complete';
    }
    if (start.name === 'scripted_oversized_image_pan_zoom') {
      return 'Pan and zoom oversized image until complete';
    }
    if (start.name === 'full_screen_color_overlay_fade') {
      return 'Fade full-screen color overlay until complete';
    }
    return start.label + ' and gate on ' + query.label;
  }

  var TRANSITION_QUERIES = Object.freeze({
    actor_move: ['actor_movement_countdown_query'],
    actor_facing_turn_transition: ['actor_facing_turn_activity_query'],
    actor_rgb_tint_transition: ['actor_rgb_tint_activity_query'],
    actor_body_pose_program_start: ['actor_body_pose_cycle_query'],
    scene_transform_sequence_start: ['scene_transform_sequence_query'],
    scene_projection_transform_transition: [
      'scene_projection_transform_countdown_query_mode2',
      'scene_projection_transform_countdown_query_unguarded'
    ],
    scene_projection_transform_identity_transition: [
      'scene_projection_transform_countdown_query_mode2',
      'scene_projection_transform_countdown_query_unguarded'
    ],
    scripted_oversized_image_pan_zoom: ['scripted_oversized_image_transition_query'],
    full_screen_color_overlay_fade: ['color_overlay_countdown_query']
  });

  function recognizeComposites(scene, primitives) {
    var assigned = {};
    var composites = [];
    var byId = {};
    primitives.forEach(function(node) { byId[node.id] = node; });

    function available(nodes) {
      return nodes.length && sameContiguousNodes(nodes) && nodes.every(function(node) {
        return !assigned[node.id];
      });
    }

    function claim(composite) {
      var nodes = composite.nodeIds.map(function(id) { return byId[id]; });
      if (!available(nodes)) return false;
      nodes.forEach(function(node) { assigned[node.id] = composite.id; });
      composites.push(composite);
      return true;
    }

    (scene.source.registeredWaits || []).forEach(function(wait) {
      var nodes = wait.nodeIds.map(function(id) { return byId[id]; });
      if (nodes.some(function(node) { return !node; }) || !sameContiguousNodes(nodes) ||
          nodes[0].startWord !== wait.startWord ||
          nodes[nodes.length - 1].endWord !== wait.endWord) {
        fail(wait.id + ' does not match the corrected primitive program.', 'wait-ownership');
      }
      claim(makeComposite('registered-wait',
        'Hold for ' + wait.ticks + ' native updates', nodes, {
          id: wait.id,
          summary: 'Arms the registered counter, gates on its target, then resets it.',
          category: 'flow', confidence: 'High', clock: 'registered-counter-updates',
          nativeTicks: wait.ticks, editable: false, source: 'corrected-corpus'
        }));
    });

    primitives.forEach(function(start, index) {
      if (start.name !== 'registered_counter_arm' || assigned[start.id]) return;
      var envelope = envelopeAt(primitives, index + 1);
      if (!envelope || envelope.query.name !== 'a_button_skippable_registered_wait_query') return;
      var reset = primitives[envelope.endIndex + 1];
      if (!reset || reset.name !== 'registered_counter_reset') return;
      var nodes = [start].concat(envelope.nodes, [reset]);
      claim(makeComposite('skippable-registered-wait',
        'Hold up to ' + envelope.query.query.target + ' native updates · A skips', nodes, {
          category: 'flow', confidence: 'High', clock: 'registered-counter-updates',
          nativeTicks: envelope.query.query.target, editable: false,
          summary: 'A-button-skippable registered wait with exact arm, gate, and reset ownership.'
        }));
    });

    primitives.forEach(function(start, index) {
      if (start.name !== 'registered_counter_arm' || assigned[start.id]) return;
      var cursor = index + 1;
      var stagedNodes = [];
      var actions = [];
      var openingTargets = [];
      var closingEnvelope = null;
      while (true) {
        var envelope = envelopeAt(primitives, cursor);
        if (!envelope) return;
        if (envelope.query.name === 'a_button_skippable_registered_wait_query') {
          closingEnvelope = envelope;
          break;
        }
        if (envelope.query.name !== 'registered_counter_query') return;
        var action = primitives[envelope.endIndex + 1];
        if (!action || ['actor_move', 'actor_state'].indexOf(action.name) === -1) return;
        stagedNodes = stagedNodes.concat(envelope.nodes, [action]);
        actions.push(action);
        openingTargets.push(envelope.query.query.target);
        cursor = envelope.endIndex + 2;
      }
      if (!actions.length) return;
      var reset = primitives[closingEnvelope.endIndex + 1];
      if (!reset || reset.name !== 'registered_counter_reset') return;
      var nodes = [start].concat(stagedNodes, closingEnvelope.nodes, [reset]);
      var actionLabels = actions.map(function(action) {
        return action.name === 'actor_move'
          ? 'move actor slot ' + actorSlot(action)
          : 'set actor slot ' + actorSlot(action) + ' state';
      });
      var actionLabel = actionLabels.join(', then ');
      actionLabel = actionLabel.charAt(0).toUpperCase() + actionLabel.slice(1);
      claim(makeComposite('skippable-registered-wait',
        actionLabel + ', then hold up to ' + closingEnvelope.query.query.target +
          ' native updates · A skips', nodes, {
          category: 'actors', confidence: 'High', clock: 'registered-counter-updates',
          nativeTicks: closingEnvelope.query.query.target, editable: false,
          summary: 'Arms one counter, gates before the actor action, then gates on the ' +
            'same counter until its target or an A-button skip before resetting it.',
          details: {
            shape: 'staged-actor-action',
            openingTargets: openingTargets,
            closingTarget: closingEnvelope.query.query.target,
            actionNodeIds: actions.map(function(action) { return action.id; })
          }
        }));
    });

    primitives.forEach(function(start, index) {
      if (start.name !== 'dialogue_window_create' || assigned[start.id]) return;
      var cursor = index + 1;
      var nodes = [start];
      if (primitives[cursor] && primitives[cursor].name === 'text_display_speed_override') {
        nodes.push(primitives[cursor++]);
      }
      var envelope = envelopeAt(primitives, cursor);
      if (!envelope || envelope.query.name !== 'dialogue_pause_query') return;
      var windowSlot = actorSlot(start);
      if (envelope.query.query.producerInput !== windowSlot) return;
      nodes = nodes.concat(envelope.nodes);
      claim(makeComposite('dialogue-window-open',
        'Open dialogue window ' + windowSlot + ' and wait for text', nodes, {
          category: 'dialogue', confidence: 'Structural', clock: 'dialogue-handshake',
          summary: 'Creates one dialogue window, applies an optional text speed, and gates on its Serifu pause.'
        }));
    });

    primitives.forEach(function(start, index) {
      if (assigned[start.id]) return;
      var acceptedQueries = TRANSITION_QUERIES[start.name];
      if (!acceptedQueries) return;
      var envelope = envelopeAt(primitives, index + 1);
      if (!envelope || acceptedQueries.indexOf(envelope.query.name) === -1) return;
      if (/^actor_/.test(start.name) && envelope.query.query.producerInput != null &&
          envelope.query.query.producerInput !== actorSlot(start)) return;
      var nodes = [start].concat(envelope.nodes);
      claim(makeComposite('start-and-completion-gate', transitionLabel(start, envelope.query),
        nodes, {
          category: start.category, confidence: 'Structural', clock: envelope.query.clock,
          summary: start.summary + ' The following query envelope gates on its completion.'
        }));
    });

    primitives.forEach(function(start, index) {
      if (start.name !== 'dialogue_window_resume' || assigned[start.id]) return;
      var close = primitives[index + 1];
      if (!close || close.name !== 'dialogue_window_close_release' || assigned[close.id] ||
          actorSlot(start) !== actorSlot(close)) return;
      claim(makeComposite('dialogue-window-resume-close',
        'Resume and close dialogue window ' + actorSlot(start), [start, close], {
          category: 'dialogue', confidence: 'Structural', clock: 'dialogue-handshake',
          summary: 'Resumes the selected dialogue window, then releases it.'
        }));
    });

    primitives.forEach(function(start, index) {
      if (assigned[start.id] || start.name !== 'handoff_marker') return;
      var envelope = envelopeAt(primitives, index);
      if (!envelope || !available(envelope.nodes)) return;
      claim(makeComposite('query-envelope', 'Gate on ' + envelope.query.label,
        envelope.nodes, {
          category: categoryFor(envelope.query.name),
          confidence: 'High', clock: envelope.query.clock,
          summary: 'Exact marker, optional barrier, bridge, and ' +
            envelope.query.query.recordKind + ' query envelope.'
        }));
    });

    primitives.forEach(function(node) {
      if (assigned[node.id]) return;
      claim(makeComposite('primitive', node.label, [node], {
        id: 'composite:primitive:' + node.id,
        category: node.category,
        confidence: node.confidence,
        clock: node.clock,
        editable: node.editPolicy !== 'preserve-native',
        source: 'corrected-corpus',
        summary: node.summary
      }));
    });

    composites.sort(function(left, right) {
      return left.startWord - right.startWord || left.endWord - right.endWord;
    });
    var covered = composites.reduce(function(total, composite) {
      return total + composite.nodeCount;
    }, 0);
    if (covered !== primitives.length || Object.keys(assigned).length !== primitives.length) {
      fail('Composite recognition did not preserve every Director primitive.', 'composite-coverage');
    }
    return { composites: composites, compositeByNodeId: assigned };
  }

  function createProgram(scene, irNodes) {
    if (!scene || !scene.source || !Array.isArray(irNodes)) {
      fail('A source-backed Director scene is required.', 'invalid-scene');
    }
    var primitives = irNodes.map(primitiveFromIrNode);
    primitives.forEach(function(node, index) { node.index = index; });
    var cursor = 0;
    primitives.forEach(function(node) {
      if (node.startWord !== cursor) {
        fail('Director primitives do not conserve source order at word ' + cursor + '.',
          'source-conservation');
      }
      cursor = node.endWord;
    });
    if (cursor !== scene.source.decodedWordCount) {
      fail('Director primitives do not conserve the complete source stream.',
        'source-conservation');
    }
    var recognized = recognizeComposites(scene, primitives);
    var byId = {};
    primitives.forEach(function(node) { byId[node.id] = node; });
    var compositeById = {};
    recognized.composites.forEach(function(composite) { compositeById[composite.id] = composite; });
    return {
      assetId: scene.assetId,
      wordCount: cursor,
      primitives: primitives,
      primitiveById: byId,
      composites: recognized.composites,
      compositeById: compositeById,
      compositeByNodeId: recognized.compositeByNodeId,
      lanes: LANE_ORDER.slice(),
      stats: {
        primitiveCount: primitives.length,
        compositeCount: recognized.composites.length,
        multiPrimitiveCompositeCount: recognized.composites.filter(function(row) {
          return row.nodeCount > 1;
        }).length,
        registeredWaitCount: recognized.composites.filter(function(row) {
          return row.kind === 'registered-wait';
        }).length,
        skippableWaitCount: recognized.composites.filter(function(row) {
          return row.kind === 'skippable-registered-wait';
        }).length
      }
    };
  }

  OB64.cutsceneDirector = Object.freeze({
    laneOrder: LANE_ORDER,
    laneLabels: LANE_LABELS,
    DirectorProgramError: DirectorProgramError,
    title: title,
    categoryFor: categoryFor,
    clockFor: clockFor,
    createProgram: createProgram,
    recognizeComposites: recognizeComposites
  });
})(window.OB64);
