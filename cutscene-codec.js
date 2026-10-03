// Lordly Caliber - strict Cutscene Studio Director codec and ROM planner.
//
// All source offsets stay behind this module. Callers work with catalog IDs,
// SceneDocument clips, and detached candidate ROM bytes.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var MAX_OUTPUT = 16 * 1024 * 1024;

  function CutsceneCodecError(message, code, details) {
    this.name = 'CutsceneCodecError';
    this.message = message;
    this.code = code || 'cutscene-codec-error';
    this.details = details || null;
  }
  CutsceneCodecError.prototype = Object.create(Error.prototype);
  CutsceneCodecError.prototype.constructor = CutsceneCodecError;

  function fail(message, code, details) {
    throw new CutsceneCodecError(message, code, details);
  }

  function bytes(value, label) {
    if (value instanceof Uint8Array) return value;
    if (value instanceof ArrayBuffer) return new Uint8Array(value);
    fail((label || 'Input') + ' must be bytes.', 'invalid-bytes');
  }

  function readU32(input, offset) {
    if (offset < 0 || offset + 4 > input.length) fail('Source word lies outside the ROM.', 'source-range');
    return ((input[offset] << 24) | (input[offset + 1] << 16) |
      (input[offset + 2] << 8) | input[offset + 3]) >>> 0;
  }

  function writeU32(output, offset, value) {
    value = unsigned(value);
    output[offset] = value >>> 24;
    output[offset + 1] = value >>> 16;
    output[offset + 2] = value >>> 8;
    output[offset + 3] = value;
  }

  function unsigned(word) { return Number(word) >>> 0; }
  function signed(word) { return unsigned(word) | 0; }

  function directorKey(value) {
    if (Number.isInteger(value) && value >= 0 && value <= 0xFFFFFFFF) {
      return value >>> 0;
    }
    if (typeof value === 'string') {
      var text = value.trim();
      if (/^(?:0x)?[0-9a-f]{1,8}$/i.test(text)) {
        return parseInt(text.replace(/^0x/i, ''), 16) >>> 0;
      }
    }
    fail('Director resource key is not a valid 32-bit hexadecimal value.',
      'director-key');
  }

  function wordsFromBytes(input) {
    input = bytes(input, 'Decoded director payload');
    if (input.length % 4) fail('Decoded director payload is not word-aligned.', 'word-alignment');
    var output = new Array(input.length / 4);
    for (var index = 0; index < output.length; index++) output[index] = readU32(input, index * 4);
    return output;
  }

  function wordsToBytes(words) {
    var output = new Uint8Array(words.length * 4);
    words.forEach(function(word, index) {
      word = unsigned(word);
      output[index * 4] = word >>> 24;
      output[index * 4 + 1] = word >>> 16;
      output[index * 4 + 2] = word >>> 8;
      output[index * 4 + 3] = word;
    });
    return output;
  }

  function equalBytes(left, right) {
    if (!(left instanceof Uint8Array) || !(right instanceof Uint8Array) || left.length !== right.length) {
      return false;
    }
    for (var index = 0; index < left.length; index++) if (left[index] !== right[index]) return false;
    return true;
  }

  function lzError(message, sourceOffset, outputOffset) {
    var source = sourceOffset == null ? '' : ' at source +0x' + sourceOffset.toString(16).toUpperCase();
    var output = outputOffset == null ? '' : ', output +0x' + outputOffset.toString(16).toUpperCase();
    fail('Custom-LZ ' + message + source + output + '.', 'custom-lz');
  }

  function decodeCustomLz(value, options) {
    var source = bytes(value, 'Custom-LZ source');
    options = options || {};
    var requireExact = options.requireExact !== false;
    var allowZeroPadding = options.allowZeroPadding === true;
    var maxOutput = options.maxOutput == null ? MAX_OUTPUT : options.maxOutput;
    if (source.length < 4) lzError('stream is missing its size header', 0, 0);
    var outputSize = readU32(source, 0);
    if (outputSize > maxOutput) lzError('declared output exceeds the safety limit', 0, 0);
    var output = new Uint8Array(outputSize);
    var sourceOffset = 4;
    var outputOffset = 0;

    function needSource(count, tokenOffset) {
      if (sourceOffset + count > source.length) lzError('token is truncated', tokenOffset, outputOffset);
    }
    function needOutput(count, tokenOffset) {
      if (outputOffset + count > outputSize) lzError('token exceeds declared output size', tokenOffset, outputOffset);
    }
    function copyBackReference(distance, length, tokenOffset) {
      var start = outputOffset - distance - 1;
      if (start < 0) lzError('back-reference precedes emitted output', tokenOffset, outputOffset);
      needOutput(length, tokenOffset);
      for (var index = 0; index < length; index++) {
        var readOffset = start + index;
        if (readOffset >= outputOffset + index) {
          lzError('back-reference reads unwritten output', tokenOffset, outputOffset);
        }
        output[outputOffset++] = output[readOffset];
      }
    }

    while (outputOffset < outputSize) {
      needSource(1, sourceOffset);
      var tokenOffset = sourceOffset;
      var control = source[sourceOffset++];
      var length, distance, byte1, byte2;
      if (control & 0x80) {
        needSource(1, tokenOffset);
        distance = ((control & 7) << 8) | source[sourceOffset++];
        length = ((control >> 3) & 15) + 3;
        copyBackReference(distance, length, tokenOffset);
      } else if (control & 0x40) {
        length = (control & 63) + 1;
        needSource(length, tokenOffset);
        needOutput(length, tokenOffset);
        output.set(source.subarray(sourceOffset, sourceOffset + length), outputOffset);
        sourceOffset += length;
        outputOffset += length;
      } else if (control & 0x20) {
        length = (control & 31) + 2;
        needOutput(length, tokenOffset);
        output.fill(0, outputOffset, outputOffset + length);
        outputOffset += length;
      } else if (control & 0x10) {
        needSource(2, tokenOffset);
        byte1 = source[sourceOffset++];
        byte2 = source[sourceOffset++];
        distance = ((byte1 & 63) << 8) | byte2;
        length = ((control & 15) | ((byte1 >> 2) & 48)) + 4;
        copyBackReference(distance, length, tokenOffset);
      } else if (control === 0) {
        needSource(3, tokenOffset);
        length = source[sourceOffset++] + 5;
        distance = (source[sourceOffset++] << 8) | source[sourceOffset++];
        copyBackReference(distance, length, tokenOffset);
      } else if (control === 1 || control === 2) {
        needSource(1, tokenOffset);
        length = source[sourceOffset++] + 3;
        needOutput(length, tokenOffset);
        output.fill(control === 1 ? 0xFF : 0, outputOffset, outputOffset + length);
        outputOffset += length;
      } else {
        lzError('contains unknown control byte 0x' + control.toString(16).toUpperCase(),
          tokenOffset, outputOffset);
      }
    }

    if (requireExact && sourceOffset !== source.length) {
      lzError('decoder did not consume the exact stored payload', sourceOffset, outputOffset);
    }
    if (!requireExact && sourceOffset < source.length) {
      if (!allowZeroPadding) lzError('trailing bytes need an explicit padding policy', sourceOffset, outputOffset);
      for (var padding = sourceOffset; padding < source.length; padding++) {
        if (source[padding] !== 0) lzError('non-zero bytes follow the completed stream', padding, outputOffset);
      }
    }
    return { bytes: output, consumed: sourceOffset, declaredSize: outputSize };
  }

  var TOKEN = Object.freeze({
    NONE: 0, LITERAL: 1, ZEROS_SHORT: 2, ZEROS_LONG: 3, FF: 4,
    BACKREF_SHORT: 5, BACKREF_MEDIUM: 6, BACKREF_LONG: 7
  });

  function encodeCustomLzOptimal(value) {
    var input = bytes(value, 'Custom-LZ input');
    var size = input.length;
    var infinity = Number.MAX_SAFE_INTEGER;
    var cost = new Float64Array(size + 1);
    cost.fill(infinity);
    cost[size] = 0;
    var token = new Uint8Array(size);
    var tokenLength = new Uint16Array(size);
    var tokenDistance = new Uint32Array(size);
    var nextLcp = new Uint16Array(size + 1);
    var currentLcp = new Uint16Array(size + 1);

    function choose(position, candidateCost, candidateToken, length, distance) {
      if (candidateCost < cost[position]) {
        cost[position] = candidateCost;
        token[position] = candidateToken;
        tokenLength[position] = length;
        tokenDistance[position] = distance || 0;
      }
    }

    for (var position = size - 1; position >= 0; position--) {
      var maxLiteral = Math.min(64, size - position);
      for (var literalLength = 1; literalLength <= maxLiteral; literalLength++) {
        choose(position, 1 + literalLength + cost[position + literalLength],
          TOKEN.LITERAL, literalLength, 0);
      }
      var run = 1;
      while (position + run < size && input[position + run] === input[position] && run < 258) run++;
      var length;
      if (input[position] === 0) {
        for (length = 2; length <= Math.min(run, 33); length++) {
          choose(position, 1 + cost[position + length], TOKEN.ZEROS_SHORT, length, 0);
        }
        for (length = 3; length <= Math.min(run, 258); length++) {
          choose(position, 2 + cost[position + length], TOKEN.ZEROS_LONG, length, 0);
        }
      } else if (input[position] === 0xFF) {
        for (length = 3; length <= Math.min(run, 258); length++) {
          choose(position, 2 + cost[position + length], TOKEN.FF, length, 0);
        }
      }

      currentLcp.fill(0);
      var maxPossible = Math.min(260, size - position);
      var windowStart = Math.max(0, position - 0x10000);
      var shortLength = 0, shortDistance = 0;
      var mediumLength = 0, mediumDistance = 0;
      var longLength = 0, longDistance = 0;
      for (var search = position - 1; search >= windowStart; search--) {
        var matchLength = input[search] === input[position]
          ? Math.min(maxPossible, nextLcp[search + 1] + 1) : 0;
        currentLcp[search] = matchLength;
        if (matchLength < 3) continue;
        var distance = position - search - 1;
        if (distance <= 0x7FF && Math.min(matchLength, 18) > shortLength) {
          shortLength = Math.min(matchLength, 18); shortDistance = distance;
        }
        if (distance <= 0x3FFF && Math.min(matchLength, 67) > mediumLength) {
          mediumLength = Math.min(matchLength, 67); mediumDistance = distance;
        }
        if (distance <= 0xFFFF && Math.min(matchLength, 260) > longLength) {
          longLength = Math.min(matchLength, 260); longDistance = distance;
        }
      }
      for (length = 3; length <= shortLength; length++) {
        choose(position, 2 + cost[position + length], TOKEN.BACKREF_SHORT, length, shortDistance);
      }
      for (length = 4; length <= mediumLength; length++) {
        choose(position, 3 + cost[position + length], TOKEN.BACKREF_MEDIUM, length, mediumDistance);
      }
      for (length = 5; length <= longLength; length++) {
        choose(position, 4 + cost[position + length], TOKEN.BACKREF_LONG, length, longDistance);
      }
      var swap = nextLcp; nextLcp = currentLcp; currentLcp = swap;
    }

    var output = [size >>> 24, size >>> 16 & 255, size >>> 8 & 255, size & 255];
    for (var cursor = 0; cursor < size;) {
      var kind = token[cursor];
      var chosenLength = tokenLength[cursor];
      var chosenDistance = tokenDistance[cursor];
      if (!kind || !chosenLength) fail('Custom-LZ encoder lost its optimal path.', 'custom-lz-encode');
      if (kind === TOKEN.LITERAL) {
        output.push(0x40 | chosenLength - 1);
        for (var literal = 0; literal < chosenLength; literal++) output.push(input[cursor + literal]);
      } else if (kind === TOKEN.ZEROS_SHORT) {
        output.push(0x20 | chosenLength - 2);
      } else if (kind === TOKEN.ZEROS_LONG) {
        output.push(2, chosenLength - 3);
      } else if (kind === TOKEN.FF) {
        output.push(1, chosenLength - 3);
      } else if (kind === TOKEN.BACKREF_SHORT) {
        output.push(0x80 | ((chosenLength - 3 & 15) << 3) | chosenDistance >>> 8,
          chosenDistance & 255);
      } else if (kind === TOKEN.BACKREF_MEDIUM) {
        var encodedLength = chosenLength - 4;
        output.push(0x10 | encodedLength & 15,
          (encodedLength & 48) << 2 | chosenDistance >>> 8, chosenDistance & 255);
      } else if (kind === TOKEN.BACKREF_LONG) {
        output.push(0, chosenLength - 5, chosenDistance >>> 8, chosenDistance & 255);
      }
      cursor += chosenLength;
    }
    var encoded = Uint8Array.from(output);
    if (!equalBytes(decodeCustomLz(encoded).bytes, input)) {
      fail('Custom-LZ encoder failed its own round trip.', 'custom-lz-encode');
    }
    return encoded;
  }

  function defaultHash(input) {
    if (OB64.consumableEffects && OB64.consumableEffects.sha256Hex) {
      return OB64.consumableEffects.sha256Hex(input);
    }
    if (window.crypto && window.crypto.subtle) {
      var copy = bytes(input, 'Hash input').slice();
      return window.crypto.subtle.digest('SHA-256', copy).then(function(digest) {
        return Array.prototype.map.call(new Uint8Array(digest), function(value) {
          return value.toString(16).padStart(2, '0');
        }).join('').toUpperCase();
      });
    }
    fail('This browser cannot verify Cutscene source hashes.', 'hash-unavailable');
  }

  function hashWith(input, hashBytes) {
    return Promise.resolve((hashBytes || defaultHash)(input)).then(function(hash) {
      return String(hash || '').toUpperCase();
    });
  }

  function sceneEntry(sceneOrDocument, catalog) {
    if (sceneOrDocument && sceneOrDocument.source && sceneOrDocument.assetId) return sceneOrDocument;
    var assetId = sceneOrDocument && sceneOrDocument.native && sceneOrDocument.native.sourceAssetId;
    var entry = catalog && catalog.getScene(assetId);
    if (!entry) fail('Cutscene source identity is not in the catalog.', 'catalog-source');
    return entry;
  }

  function dynamicDirectorDefinitions(scene, decodedBytes) {
    var source = scene && scene.source;
    if (!source || source.dynamicGrammar !== true) return source && source.nodes || [];
    // A loaded retail scene keeps its tiled nodes and authoring policies.
    if (source.runtimeTiled === true && Array.isArray(source.nodes) && source.nodes.length &&
        source.decodedLength === decodedBytes.length) {
      return source.nodes;
    }
    var grammar = OB64.cutsceneData && OB64.cutsceneData.directorGrammar;
    if (!Array.isArray(grammar) || !grammar.length) {
      fail('The retail Director grammar is not loaded.', 'module-order');
    }
    var byOpcode = {};
    grammar.forEach(function(template) {
      byOpcode[unsigned(template.opcodeU32)] = template;
    });
    var allWords = wordsFromBytes(decodedBytes);
    var cursor = 0;
    var prefix = scene.assetId.split(':').pop().toUpperCase();
    var definitions = [];
    while (cursor < allWords.length) {
      var opcode = unsigned(allWords[cursor]);
      var template = byOpcode[opcode];
      if (!template) {
        fail('Retail Director opcode 0x' + opcode.toString(16).toUpperCase() +
          ' has no structural grammar at word ' + cursor + '.', 'source-boundary');
      }
      var continuationTerminal = source.terminalWithoutTrailer === true &&
        opcode === 0x80000001 && cursor === allWords.length - 1;
      var wordCount = continuationTerminal ? 1 : Number(template.sourceWordSpan);
      if (!Number.isInteger(wordCount) || wordCount < 1 || cursor + wordCount > allWords.length) {
        fail('Retail Director command at word ' + cursor +
          ' exceeds the decoded payload.', 'source-boundary');
      }
      var roles = continuationTerminal ? [] : (Array.isArray(template.operandRoles)
        ? template.operandRoles.slice() : []);
      if (roles.length !== wordCount - 1) {
        fail('Retail Director opcode 0x' + opcode.toString(16).toUpperCase() +
          ' has an incomplete operand-role template.', 'source-boundary');
      }
      definitions.push({
        id: 'node:' + prefix + ':w' + cursor.toString(16).toUpperCase().padStart(4, '0'),
        startWord: cursor,
        endWord: cursor + wordCount,
        wordCount: wordCount,
        nodeType: template.nodeType,
        name: template.name,
        semanticSummary: template.semanticSummary,
        confidence: template.confidence,
        opcode: '0x' + opcode.toString(16).toUpperCase().padStart(8, '0'),
        opcodeU32: opcode,
        operandRoles: roles,
        queryRecordKind: template.queryRecordKind || null,
        terminationKind: continuationTerminal
          ? 'stream_terminator_without_trailer'
          : template.terminationKind || null,
        runtimeReachability: 'not established by static source order',
        newlyRecoveredFromDispatch: template.newlyRecoveredFromDispatch === true,
        editPolicy: 'preserve-native',
        insertBefore: false,
        segmentId: null,
        segmentIds: [],
        unknown: template.semanticStatus === 'structural-width-only'
      });
      cursor += wordCount;
    }
    if (Number.isInteger(source.runtimeNodeCount) &&
        definitions.length !== source.runtimeNodeCount) {
      fail('Retail Director runtime tiling no longer matches the generated catalog.',
        'source-boundary');
    }
    return definitions;
  }

  var AUTHORING_POLICIES = {
    0x14: 'typed-place', 0x03: 'typed-state', 0x07: 'typed-move',
    0x2C: 'typed-projection-transform', 0x48: 'typed-actor-opacity'
  };

  // Typed edit policies and safe insertion points for retail-grammar streams.
  // Authored commands may only enter a stream in front of a composite (a bare
  // command, a complete registered wait, a query envelope, or a dialogue
  // handshake), never between the arm, gate, and reset of one, and never in
  // front of the stream's first setup command.
  function applyAuthoringPolicies(scene, definitions, decodedBytes) {
    definitions.forEach(function(definition) {
      definition.editPolicy = AUTHORING_POLICIES[definition.opcodeU32] || 'preserve-native';
      definition.insertBefore = false;
    });
    var probe = Object.assign({}, scene, { source: Object.assign({}, scene.source, {
      nodes: definitions, dynamicGrammar: false, decodedLength: decodedBytes.length,
      decodedWordCount: decodedBytes.length / 4, registeredWaits: [],
      reparsedForAuthoring: true
    }) });
    var program = createIr(probe, decodedBytes, definitions).program;
    // Barriers, bridges, and labels are scan targets for failed conditions and
    // cursor jumps, so nothing is inserted in front of them.
    definitions.forEach(function(definition, index) {
      var composite = program.compositeById[program.compositeByNodeId[definition.id]];
      definition.insertBefore = index > 0 && !!composite &&
        composite.nodeIds[0] === definition.id &&
        (definition.nodeType === 'command' ||
          definition.name === 'registered_counter_arm' || definition.name === 'handoff_marker' ||
          definition.opcodeU32 === 0x80000001);
    });
    // An authored scene keeps its class-5 setup (scene group and both cameras,
    // words 0..17) ahead of every Actor, and it accepts new commands in front of
    // its native exit (0x7D), which a barrier and a bridge follow.
    // In new and wiped scenes, a jump that ends a choice branch (0x59 directly
    // followed by a label) accepts commands in front of it: they run in that
    // branch before it rejoins.
    if (scene && (scene.authored || scene.wiped)) {
      definitions.forEach(function(definition, index) {
        var next = definitions[index + 1];
        if (definition.opcodeU32 === 0x59 && definition.wordCount === 2 && next && next.opcodeU32 === 0x80000005) {
          definition.insertBefore = true;
          definition.choiceBranchEnd = true;
        }
      });
    }
    if (scene && scene.authored) {
      definitions.forEach(function(definition) {
        // A native 0x1B fade in a new scene is a typed Fade card (retail
        // scenes keep their display-only projection).
        if (definition.opcodeU32 === 0x1B && definition.wordCount === 7) definition.editPolicy = 'typed-fade';
        if (definition.startWord < 18) {
          definition.insertBefore = false;
          definition.insertionWithheld = 'authored-setup';
        } else if (definition.opcodeU32 === 0x7D) {
          definition.insertBefore = true;
        }
      });
    }
    // A wiped retail scene keeps its leading records (and camera pair) ahead of
    // everything, and takes new commands in front of its parsed exit (7D or 39).
    if (scene && scene.wiped) {
      definitions.forEach(function(definition) {
        if (definition.opcodeU32 === 0x1B && definition.wordCount === 7) definition.editPolicy = 'typed-fade';
        if (definition.startWord < scene.wiped.setupWordCount) {
          definition.insertBefore = false;
          definition.insertionWithheld = 'wiped-setup';
        } else if (definition.startWord === scene.wiped.exitStartWord &&
            (definition.opcodeU32 === 0x7D || definition.opcodeU32 === 0x39)) {
          definition.insertBefore = true;
        }
      });
    }
    return withholdBeforeFirstCondition(definitions);
  }

  // A failing native condition scans forward to the next bridge and needs a
  // barrier in between; with none, the scanner returns -1 and the parser
  // restarts the stream from word 0 in the same call (scanner z64 0x2862FC,
  // caller 0x286174; live capture 2026-09-26, opening ceremony: an inserted
  // dialogue handshake followed by the retail barrier-less first wait re-ran the
  // block and re-created its window until the dialogue pool ran out). A stream's
  // first condition, a segment's first condition, and the terminator have no
  // barrier ahead of the next bridge, so nothing is inserted in front of them.
  function withholdBeforeFirstCondition(definitions) {
    definitions.forEach(function(definition, index) {
      if (!definition.insertBefore) return;
      var barrier = false;
      var bridge = -1;
      for (var cursor = index; cursor < definitions.length; cursor++) {
        var opcode = definitions[cursor].opcodeU32;
        if (opcode === 0x80000002) barrier = true;
        if (opcode === 0x80000000) { bridge = cursor; break; }
      }
      if (bridge < 0 || !barrier) {
        definition.insertBefore = false;
        definition.insertionWithheld = bridge < 0 ? 'no-bridge-ahead' : 'next-condition-barrier-less';
      }
    });
    return definitions;
  }

  // Reparse edited streams with the same complete retail grammar. Source identity
  // stays on the native entry; physical offsets and typed boundaries are current.
  function sceneForBytes(scene, decodedBytes) {
    var next = Object.assign({}, scene, {source:Object.assign({},scene.source,{
      dynamicGrammar:true,decodedLength:decodedBytes.length,decodedWordCount:decodedBytes.length/4,
      runtimeNodeCount:null, registeredWaits:[], corpusRegisteredWaitCount:0, reparsedForAuthoring:true,
      runtimeTiled:false, nodes:[]
    })});
    var defs=applyAuthoringPolicies(next,dynamicDirectorDefinitions(next,decodedBytes),decodedBytes);
    next.source.nodes=defs;next.source.dynamicGrammar=false;
    return next;
  }

  // Authored scenes have no retail payload. The editor supplies their decoded
  // stream (a validated scaffold, or an arena record on reopen), and it is tiled
  // exactly as loadSceneSource tiles a scene adapted by sceneForBytes.
  function authoredSceneSource(scene, decodedInput, options) {
    options = options || {};
    var decoded;
    try {
      decoded = bytes(decodedInput, 'Authored Director stream');
      if (!scene || !(scene.authored || scene.wiped) || !decoded.length || decoded.length % 4 || decoded.length > 65536) {
        fail('An authored Director stream must be a nonempty whole-word stream of at most 64 KiB.', 'authored-source');
      }
    } catch (error) {
      return Promise.reject(error);
    }
    return hashWith(decoded, options.hashBytes).then(function(hash) {
      var nodeDefinitions = dynamicDirectorDefinitions(scene, decoded);
      if (!scene.source.firstConditionWithheld) {
        withholdBeforeFirstCondition(nodeDefinitions);
        scene.source = Object.assign({}, scene.source, { firstConditionWithheld: true });
      }
      return {
        scene: scene,
        // The loaded ROM, when given, lets native dialogue windows project as
        // editable lines (their text lives in the ROM's shared archives).
        rom: options.rom || null,
        payload: null,
        authored: !!scene.authored,
        wiped: !!scene.wiped,
        decodedBytes: decoded,
        decodedSha256: hash,
        consumedEncodedBytes: 0,
        nodeDefinitions: nodeDefinitions
      };
    });
  }

  function loadSceneSource(z64Input, scene, options) {
    var z64 = bytes(z64Input, 'Normalized ROM');
    options = options || {};
    var source = scene.source;
    if(options.allowModified){
      var offsets=source.directorSelectorWordZ64||[],key=offsets.length?readU32(z64,offsets[0]):directorKey(scene.directorKey);
      if(offsets.some(function(at){return readU32(z64,at)!==key;}))fail('Scene aliases point to different resources.','selector-owner-preimage');
      if(key!==directorKey(scene.directorKey)){
        var descriptor=0x27af000,count=readU32(z64,descriptor+8),owned=false;
        if(readU32(z64,descriptor)!==0x4f424353||[1,2].indexOf(readU32(z64,descriptor+4))<0||count>144)fail('Scene relocation has no supported ownership descriptor.','selector-owner-preimage');
        for(var r=0;r<count;r++){var at=descriptor+0x40+r*28;if(readU32(z64,at)===directorKey(scene.directorKey)&&readU32(z64,at+4)===key&&readU32(z64,at+8)===key+0x594280)owned=true;}
        var entry=key+0x594280,size=readU32(z64,entry);
        if(!owned||entry<0x2780000||entry+4+size>descriptor||size<1)fail('Scene relocation exceeds its owned arena.','source-range');
        scene.directorKey=typeof scene.directorKey==='string'?'0x'+key.toString(16).toUpperCase().padStart(8,'0'):key;source=scene.source=Object.assign({},source,{z64PrefixStart:entry,z64PayloadStart:entry+4,z64PayloadEndExclusive:entry+4+size,storedPayloadLength:size,dmaExtent:(size+1)&~1});
      }
    }
    var selectorWords = source && source.directorSelectorWordZ64 || [];
    var originalDirectorKey = directorKey(scene.directorKey);
    for (var selectorIndex = 0; selectorIndex < selectorWords.length; selectorIndex++) {
      var selectorOffset = selectorWords[selectorIndex];
      if (!Number.isInteger(selectorOffset) || selectorOffset < 0 ||
          selectorOffset + 4 > z64.length ||
          readU32(z64, selectorOffset) !== originalDirectorKey) {
        return Promise.reject(new CutsceneCodecError(
          'A Director selector owner no longer points to this scene’s verified retail resource. ' +
          'Reopening relocated or foreign-owned Cutscene payloads is not supported yet.',
          'selector-owner-preimage'));
      }
    }
    if (!source || source.z64PrefixStart + 4 > z64.length ||
        source.z64PayloadEndExclusive > z64.length) {
      return Promise.reject(new CutsceneCodecError(
        'Cutscene source lies outside the loaded ROM.', 'source-range'));
    }
    if (readU32(z64, source.z64PrefixStart) !== source.storedPayloadLength) {
      return Promise.reject(new CutsceneCodecError(
        'Cutscene source capacity does not match the catalog.', 'source-preimage'));
    }
    var payload = z64.slice(source.z64PayloadStart, source.z64PayloadEndExclusive);
    var decoded;
    try {
      var padded = !!(options.allowModified || options.allowZeroPadding);
      decoded = decodeCustomLz(payload, { requireExact: !padded, allowZeroPadding: padded, maxOutput:65536 });
    } catch (error) {
      return Promise.reject(error);
    }
    if (!options.allowModified && decoded.bytes.length !== source.decodedLength) {
      return Promise.reject(new CutsceneCodecError(
        'Cutscene decoded length does not match the catalog.', 'source-preimage'));
    }
    return hashWith(decoded.bytes, options.hashBytes).then(function(hash) {
      var expectedHash = String(options.expectedDecodedSha256 || source.decodedSha256).toUpperCase();
      if (options.allowModified && (hash !== expectedHash || decoded.bytes.length !== source.decodedLength)) {
        var adapted=sceneForBytes(scene,decoded.bytes);
        scene.source=source=Object.assign(adapted.source,{decodedSha256:hash});
      }
      if (!options.allowModified && hash !== expectedHash) {
        fail('Cutscene decoded bytes do not match the selected US Rev 0 source.', 'source-preimage', {
          expectedSha256: expectedHash, actualSha256: hash
        });
      }
      var nodeDefinitions = dynamicDirectorDefinitions(scene, decoded.bytes);
      if (source.dynamicGrammar !== true && !source.firstConditionWithheld) {
        withholdBeforeFirstCondition(nodeDefinitions);
        scene.source = source = Object.assign({}, source, { firstConditionWithheld: true });
      }
      if (source.dynamicGrammar === true) {
        // Retail-grammar scenes adopt their tiled nodes with authoring policies so
        // every Director scene exposes typed edits and safe insertion points.
        applyAuthoringPolicies(scene, nodeDefinitions, decoded.bytes);
        scene.source = source = Object.assign({}, source, {
          nodes: nodeDefinitions, runtimeTiled: true, reparsedForAuthoring: true
        });
      }
      return {
        scene: scene,
        rom: z64,
        payload: payload,
        decodedBytes: decoded.bytes,
        decodedSha256: hash,
        consumedEncodedBytes: decoded.consumed,
        nodeDefinitions: nodeDefinitions
      };
    });
  }

  function createIr(scene, decodedInput, nodeDefinitions) {
    var decodedBytes = bytes(decodedInput, 'Decoded director payload');
    if (decodedBytes.length !== scene.source.decodedLength) {
      fail('Decoded director payload length does not match the catalog.', 'source-preimage');
    }
    var allWords = wordsFromBytes(decodedBytes);
    nodeDefinitions = nodeDefinitions || dynamicDirectorDefinitions(scene, decodedBytes);
    var nodes = nodeDefinitions.map(function(definition) {
      var rawWords = allWords.slice(definition.startWord, definition.endWord);
      if (rawWords.length !== definition.wordCount) {
        fail('Director boundary falls outside the decoded payload.', 'source-boundary');
      }
      return {
        definition: definition,
        id: definition.id,
        rawWords: rawWords,
        currentWords: rawWords.slice(),
        deleted: false
      };
    });
    var ir = { scene: scene, originalDecodedBytes: decodedBytes.slice(), nodes: nodes };
    if (!OB64.cutsceneDirector) {
      fail('The corrected Director program module is required.', 'module-order');
    }
    ir.program = OB64.cutsceneDirector.createProgram(scene, nodes);
    return ir;
  }

  function coordinate(word) {
    var value = signed(word);
    return value === -1000 ? null : value / 1000;
  }

  function launchTranslationIndex(word) {
    word = unsigned(word);
    return (word & 0xFFFFFF00) === 0x08880000 ? word & 0xFF : null;
  }

  function actorSelectorValid(bank, key, facing) {
    return Number.isInteger(bank) && bank >= 1 && bank <= 68 &&
      Number.isInteger(key) && key >= 0 && key <= 0xFFFF &&
      Number.isInteger(facing) && facing >= 0 && facing <= 0xFFFF;
  }

  function nativeAudioBlockIndex(requestValue) {
    if (!Number.isInteger(requestValue) || requestValue < 0 || requestValue > 65 ||
        [1, 5, 15].indexOf(requestValue) !== -1) return null;
    return requestValue - [1, 5, 15].filter(function(emptyRow) {
      return emptyRow < requestValue;
    }).length;
  }

  function decodeNode(node) {
    var words = node.currentWords.map(unsigned);
    if (words[0] === 0x80000006 && words.length === 2) {
      return {
        type: 'background-group',
        selector: signed(words[1]),
        selectorTableId: 'background-table:scene:31'
      };
    }
    if (words[0] === 0x14 && words.length === 10) {
      var placeBank = signed(words[2]), placeKey = signed(words[3]);
      var placeFacing = signed(words[4]);
      var placeVariantTranslationIndex = launchTranslationIndex(words[9]);
      return { type: 'place', slot: words[1], bank: placeBank, key: placeKey,
        facing: placeFacing, selectorValid: actorSelectorValid(placeBank, placeKey, placeFacing),
        x: coordinate(words[5]), y: coordinate(words[6]), z: coordinate(words[7]),
        renderMode: words[8] & 0xFF, rawRenderMode: signed(words[8]),
        variantSelector: placeVariantTranslationIndex === null ? words[9] & 0xFF : 0,
        rawVariantSelector: signed(words[9]),
        variantSelectorTranslationIndex: placeVariantTranslationIndex,
        variantSelectorStatus: placeVariantTranslationIndex === null
          ? 'exact opcode-0x14 operand 9'
          : 'launch-translation input unresolved; appearance zero is a preview fallback' };
    }
    if (words[0] === 3 && words.length === 9) {
      var stateBank = signed(words[2]), stateKey = signed(words[3]);
      var stateFacing = signed(words[4]);
      var rawStateVariant = signed(words[8]);
      return { type: 'state', slot: words[1], bank: stateBank, key: stateKey,
        facing: stateFacing, selectorValid: actorSelectorValid(stateBank, stateKey, stateFacing),
        x: coordinate(words[5]), y: coordinate(words[6]), z: coordinate(words[7]),
        variantSelector: rawStateVariant === -1 ? -1 : words[8] & 0xFF,
        rawVariantSelector: rawStateVariant };
    }
    if (words[0] === 7 && words.length === 8) {
      return { type: 'move', slot: words[1], x: signed(words[4]) / 1000,
        z: signed(words[5]) / 1000, speed: signed(words[7]) / 1000 };
    }
    if (words[0] === 0x2A && words.length === 7) {
      return {
        type: 'actor-body-pose-program',
        opcode: '0x2A',
        operands: words.slice(1).map(signed),
        slot: signed(words[1])
      };
    }
    if (words[0] === 0x13 && words.length === 2) {
      return { type: 'actor-remove', slot: signed(words[1]) };
    }
    if (words[0] === 0x1C && words.length === 3) {
      return {
        type: 'actor-scene-transform-channel',
        slot: signed(words[1]),
        transformChannel: signed(words[2])
      };
    }
    if (words[0] === 0x48 && words.length === 3) {
      return { type: 'actor-opacity', slot: signed(words[1]), opacityByte: words[2] & 0xFF };
    }
    if (words[0] === 0x2C && words.length === 6) {
      var projectionOperands = words.slice(1).map(signed);
      return {
        type: 'projection-transform',
        operands: projectionOperands,
        target: {
          translateX: projectionOperands[0] / 1000,
          translateY: projectionOperands[1] / 1000,
          scaleX: projectionOperands[2] / 1000,
          scaleY: projectionOperands[3] / 1000
        },
        nativeCountdown: projectionOperands[4],
        nativeCountdownLow16: projectionOperands[4] & 0xFFFF,
        clockStatus: 'projection updater invocations; display-frame conversion unresolved'
      };
    }
    if ((words[0] === 0x35 || words[0] === 0x36) && words.length === 8) {
      var cameraPose = words.slice(1).map(signed);
      return {
        type: 'camera-pose',
        bank: words[0] === 0x35 ? 'registered-object' : 'actor-side',
        target: { x: cameraPose[0] / 1000, y: cameraPose[1] / 1000,
          z: cameraPose[2] / 1000 },
        eye: { x: cameraPose[3] / 1000, y: cameraPose[4] / 1000,
          z: cameraPose[5] / 1000 },
        fovY: cameraPose[6] / 1000,
        operands: cameraPose
      };
    }
    if (words[0] === 0x3D && words.length === 2) {
      return {
        type: 'projection-identity-transition',
        nativeCountdown: signed(words[1]),
        nativeCountdownLow16: words[1] & 0xFFFF,
        clockStatus: 'projection updater invocations; display-frame conversion unresolved'
      };
    }
    if (words[0] === 0x46 && words.length === 10) {
      var effectOperands = words.slice(1).map(signed);
      return {
        type: 'native-sprite-effect',
        opcode: '0x46',
        operands: effectOperands,
        slot: effectOperands[0] & 0xFF,
        bank: effectOperands[1],
        key: effectOperands[2],
        facing: effectOperands[3],
        selectorValid: actorSelectorValid(
          effectOperands[1], effectOperands[2], effectOperands[3]),
        modelX: effectOperands[4],
        modelY: -effectOperands[5],
        renderPassSelector: effectOperands[6] & 0xFF,
        scale: effectOperands[7] / 100,
        variantSelector: effectOperands[8] & 0xFF
      };
    }
    if (words[0] === 0x66 && words.length === 2) {
      return { type: 'native-sprite-effect-remove', opcode: '0x66', slot: signed(words[1]) };
    }
    if ([0x6E, 0x6F, 0x70, 0xB4].indexOf(words[0]) !== -1) {
      return {
        type: 'audio-controller',
        opcode: '0x' + words[0].toString(16).toUpperCase(),
        operands: words.slice(1).map(signed)
      };
    }
    if ([0x1A, 0x1B, 0x33, 0x47, 0x7D, 0x7E, 0x7F].indexOf(words[0]) !== -1) {
      return {
        type: 'native-effect',
        opcode: '0x' + words[0].toString(16).toUpperCase(),
        operands: words.slice(1).map(signed)
      };
    }
    return null;
  }

  function actorForSlot(document, scene, slot) {
    var existing = document.actors.find(function(actor) { return actor.slot === slot; });
    if (existing) return existing;
    var sourceActor = scene.actors.find(function(actor) { return actor.slot === slot; });
    var actorId = sourceActor ? sourceActor.actorId :
      'actor:' + scene.assetId.split(':').pop().toLowerCase() + ':slot:' + String(slot).padStart(2, '0');
    existing = OB64.cutsceneModel.createActor({
      id: actorId,
      label: sourceActor && sourceActor.label || 'Actor slot ' + slot,
      slot: slot,
      artSourceId: sourceActor && sourceActor.bank != null
        ? 'cutscene-art-bank:' + sourceActor.bank : null,
      capability: OB64.cutsceneModel.capabilities.NEEDS_RESEARCH,
      initial: { visible: false, x: 0, y: 0, z: 0, facing: 'unresolved', poseId: null },
      source: {
        catalogActorId: sourceActor && sourceActor.actorId || null,
        placeNodeId: null,
        recordProducer: sourceActor ? sourceActor.recordProducer === true : false,
        initializationStatus: sourceActor && sourceActor.initializationStatus ||
          'no record producer located'
      }
    });
    OB64.cutsceneModel.addActor(document, existing);
    return document.actors.find(function(actor) { return actor.id === actorId; });
  }

  function trackFor(document, type, actor, label) {
    var id = actor ? 'track:slot:' + String(actor.slot).padStart(2, '0') + ':' + type : 'track:global:' + type;
    var existing = document.tracks.find(function(track) { return track.id === id; });
    if (existing) return existing;
    existing = OB64.cutsceneModel.createTrack({
      id: id, type: type, actorId: actor ? actor.id : null, label: label || type
    });
    OB64.cutsceneModel.addTrack(document, existing);
    return OB64.cutsceneModel.findTrack(document, id);
  }

  function clipId(node, kind, track) {
    return 'clip:w' + node.definition.startWord.toString(16).toUpperCase() + ':' + kind +
      (track && track.actorId ? ':' + track.actorId : '');
  }

  function boundaryId(node) {
    return 'boundary:w' + node.definition.startWord.toString(16).toUpperCase();
  }

  function nativeCommand(node, projectedClipIds, compositeId) {
    return {
      id: 'native:w' + node.definition.startWord.toString(16).toUpperCase(),
      boundaryId: boundaryId(node),
      words: node.rawWords.map(unsigned),
      kind: node.definition.name || node.definition.nodeType || 'unknown',
      source: {
        nodeId: node.id,
        startWord: node.definition.startWord,
        endWord: node.definition.endWord,
        editPolicy: node.definition.editPolicy,
        insertBefore: node.definition.insertBefore === true,
        insertionWithheld: node.definition.insertionWithheld || null,
        semanticName: node.definition.name,
        semanticSummary: node.definition.semanticSummary,
        confidence: node.definition.confidence,
        category: OB64.cutsceneDirector.categoryFor(node.definition.name),
        compositeId: compositeId || null,
        projectedClipIds: projectedClipIds || []
      }
    };
  }

  function addClip(track, node, kind, startFrame, durationFrames, payload, capability) {
    var clip = OB64.cutsceneModel.createClip({
      id: clipId(node, kind, track),
      kind: kind,
      startFrame: startFrame,
      durationFrames: durationFrames,
      pathIds: [],
      capability: capability || OB64.cutsceneModel.capabilities.NATIVE,
      payload: payload,
      source: {
        nodeId: node.id,
        boundaryId: boundaryId(node),
        startWord: node.definition.startWord,
        originalStartFrame: startFrame,
        originalDurationFrames: durationFrames,
        commandKind: node.definition.name
      }
    });
    track.clips.push(clip);
    return clip;
  }

  function poseIdentity(act) {
    return 'cutscene-pose:' + act.bank + ':' + act.key + ':' + act.facing;
  }

  var ENVIRONMENT_TABLE_ID = 'background-table:mode2-environment:80';

  // One ordered layer list per selectable table row. Projection, the picker
  // and export validation all build layers here, so they cannot drift.
  function backgroundGroupLayers(entry, catalog, capability, sourceKind) {
    if (!entry || !Array.isArray(entry.archiveAssetIds)) return [];
    var members = Array.isArray(entry.members) && entry.members.length === entry.archiveAssetIds.length
      ? entry.members : entry.archiveAssetIds.map(function(assetId, index) {
        return { ordinal: index, assetId: assetId };
      });
    return members.map(function(member, index) {
      var assetId = member.assetId;
      var asset = catalog && catalog.getImageAsset ? catalog.getImageAsset(assetId) : null;
      var role = member.role || (index === 0 ? 'environment-base' : 'ordered-layer');
      return {
        id: role === 'environment-base' ? 'background:base' : 'background:layer:' + index,
        assetId: assetId,
        label: asset ? asset.displayName : assetId,
        visible: true,
        depth: member.ordinal,
        role: role,
        capability: capability,
        source: {
          sourceKind: sourceKind || 'native-scene-group',
          selector: entry.selector,
          groupResourceKey: entry.groupResourceKey,
          traversalOrdinal: member.ordinal,
          associationStatus: entry.associationStatus
        }
      };
    });
  }

  // Mode-two environment rows as selectable groups. The launch pre-scan copies
  // the Director operand into the environment selector, and the initializer
  // copies it again into the foreground selector, so one operand picks both.
  function environmentBackgroundGroups(catalog) {
    var table = catalog && catalog.getBackgroundSelectorTable
      ? catalog.getBackgroundSelectorTable(ENVIRONMENT_TABLE_ID) : null;
    if (!table) return [];
    return table.entries.filter(function(entry) {
      return Array.isArray(entry.stageLayers) && entry.stageLayers.length;
    }).map(function(entry) {
      // The same selector picks the foreground (occlusion) pieces, drawn over
      // the Actors; the catalog places them at depth 2 + index.
      var front = catalog.getBackgroundSelectorEntry('background-table:mode2-overlay:80', entry.selector);
      var members = entry.stageLayers.map(function(layer, index) {
        return { ordinal: index, assetId: layer.assetId, role: layer.role || null };
      }).concat((front && front.archiveAssetIds || []).map(function(assetId, index) {
        return { ordinal: 2 + index, assetId: assetId, role: 'foreground-mask' };
      }));
      return {
        selector: entry.selector,
        groupResourceKey: entry.resourceKey,
        archiveAssetIds: members.map(function(member) { return member.assetId; }),
        members: members,
        associationStatus: entry.associationStatus
      };
    });
  }

  function terminalClassOf(ir) {
    var last = ir && ir.nodes.length ? ir.nodes[ir.nodes.length - 1].rawWords : null;
    var word = last && last.length ? unsigned(last[last.length - 1]) : 0;
    return (word >>> 24) === 0xFF ? word & 0xFF : null;
  }

  // Classes 1 and 8 build a fresh mode-two Stage from the launch pre-scan of
  // the first command. Classes 2 and 7 keep their caller's Stage, and a
  // missing or derived (-1) operand inherits or derives it, so none of those
  // routes can take a selection.
  function environmentRouteEditable(scene, node, act, ir) {
    var mode = scene.launchProfile && scene.launchProfile.directorMode;
    var start = node.startWord != null ? node.startWord : node.definition && node.definition.startWord;
    var request = scene.launchProfile && scene.launchProfile.background.requests.find(function(candidate) {
      return candidate.wordStart === start;
    });
    var commands = ir.nodes.filter(function(candidate) {
      return unsigned(candidate.rawWords[0]) === 0x80000006;
    });
    var terminalClass = terminalClassOf(ir);
    return scene.engine === 'director' && unsigned(node.rawWords[0]) === 0x80000006 && node.rawWords.length === 2 &&
      !!mode && mode.value === 2 && (terminalClass === 1 || terminalClass === 8) &&
      start === 0 && commands.length === 1 && act.selector >= 0 && act.selector < 80 &&
      !!request && request.selectorTableId === ENVIRONMENT_TABLE_ID &&
      Number.isInteger(request.commandOperand) && request.commandOperand >= 0;
  }

  // Classes 4 and 5 start a fresh room from one initial scene-group command
  // (the first or second record, after an optional 0x80000007), the same
  // contract ROM startup applies. func_00067FA8 preloads that group's first
  // member before the Director runs, and the parser materializes the rest.
  function sceneGroupRouteEditable(scene, node, act, ir) {
    var mode = scene.launchProfile && scene.launchProfile.directorMode;
    var commands = ir.nodes.filter(function(candidate) {
      return unsigned(candidate.rawWords[0]) === 0x80000006;
    });
    var terminalClass = terminalClassOf(ir);
    return scene.engine === 'director' && unsigned(node.rawWords[0]) === 0x80000006 && node.rawWords.length === 2 &&
      !!mode && mode.value === 0 && (terminalClass === 4 || terminalClass === 5) &&
      commands.length === 1 && ir.nodes.indexOf(node) <= 1 && act.selector >= 0 && act.selector <= 30;
  }

  function registeredCounterTiming(ir) {
    var roles = {};
    var program = ir && ir.program;
    if (!program || !Array.isArray(program.composites)) return roles;
    program.composites.filter(function(composite) {
      return composite.kind === 'registered-wait' ||
        composite.kind === 'skippable-registered-wait';
    }).forEach(function(composite) {
      var primitives = composite.nodeIds.map(function(id) {
        return program.primitiveById[id];
      });
      var arm = primitives[0];
      var reset = primitives[primitives.length - 1];
      var gateName = composite.kind === 'skippable-registered-wait'
        ? 'a_button_skippable_registered_wait_query' : 'registered_counter_query';
      var gate = primitives.find(function(node) { return node && node.name === gateName; });
      if (!arm || !reset || !gate || arm.name !== 'registered_counter_arm' ||
          reset.name !== 'registered_counter_reset') {
        fail(composite.id + ' lost its corrected registered-wait ownership.',
          'registered-wait');
      }
      roles[arm.id] = { kind: 'arm', compositeId: composite.id };
      roles[gate.id] = { kind: 'gate', target: composite.nativeTicks,
        compositeId: composite.id,
        skippable: composite.kind === 'skippable-registered-wait',
        editable: composite.kind === 'registered-wait' };
      roles[reset.id] = { kind: 'reset', compositeId: composite.id };
    });
    return roles;
  }

  function sourceClipRows(document) {
    var rows = [];
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) { rows.push({ track: track, clip: clip }); });
    });
    return rows;
  }

  function applyReviewedTimelineOverlay(document, scene) {
    var overlay = scene.reviewedTimelineOverlay;
    if (!overlay || !overlay.featuredSequence) return;
    var featured = overlay.featuredSequence;
    var actor = document.actors.find(function(candidate) {
      return candidate.slot === featured.actorSlot;
    });
    if (actor) {
      actor.label = featured.actorLabel;
      actor.source.reviewedPresentation = {
        overlayId: overlay.overlayId,
        reviewStatus: overlay.reviewStatus,
        initialSemanticLabel: featured.initialState &&
          featured.initialState.semanticLabel || null,
        presentationTitle: overlay.presentation && overlay.presentation.title || null
      };
    }

    var rows = sourceClipRows(document);
    function rowAt(word, kind) {
      return rows.find(function(row) {
        return row.clip.kind === kind && row.clip.source &&
          row.clip.source.startWord === word;
      }) || null;
    }
    function relabel(step, kind) {
      if (!step || !Number.isInteger(step.word)) return null;
      var row = rowAt(step.word, kind);
      if (!row) return null;
      row.clip.payload.semanticLabel = step.semanticLabel;
      row.clip.payload.displayLabel = step.semanticLabel;
      row.clip.payload.reviewedTimelineOverlayId = overlay.overlayId;
      row.clip.payload.reviewStatus = overlay.reviewStatus;
      return row;
    }
    var movementRow = relabel(featured.movement, 'movement');
    var targetRow = relabel(featured.targetState, 'pose');

    (overlay.runtimeDependencies || []).forEach(function(dependency) {
      var movement = movementRow && movementRow.clip.source.startWord === dependency.afterWord
        ? movementRow : rowAt(dependency.afterWord, 'movement');
      var target = targetRow && targetRow.clip.source.startWord === dependency.targetWord
        ? targetRow : rowAt(dependency.targetWord, 'pose');
      if (!movement || !target) return;
      var movementEnd = movement.clip.startFrame + movement.clip.durationFrames;
      var delta = Math.max(0, movementEnd - target.clip.startFrame);
      if (delta > 0) {
        rows.forEach(function(row) {
          var clip = row.clip;
          if (!clip.source || !Number.isInteger(clip.source.startWord) ||
              clip.source.startWord < dependency.targetWord) return;
          clip.source.staticProjectionStartFrame = clip.startFrame;
          clip.startFrame += delta;
          clip.source.originalStartFrame = clip.startFrame;
        });
      }
      (dependency.viaWords || []).forEach(function(word) {
        rows.filter(function(row) {
          return row.clip.source && row.clip.source.startWord === word;
        }).forEach(function(row) {
          row.clip.source.staticProjectionStartFrame = row.clip.startFrame;
          row.clip.startFrame = movementEnd;
          row.clip.source.originalStartFrame = movementEnd;
          row.clip.payload.reviewedRuntimeDependency = dependency.id;
          row.clip.payload.displayLabel = dependency.label;
          row.clip.payload.timingStatus = dependency.timingClass +
            '; reviewed presentation placement; static query binding remains unbound';
        });
      });
      movement.clip.payload.reviewedRuntimeDependency = dependency.id;
      movement.clip.payload.timingStatus =
        'native movement-integrator duration; reviewed completion presentation';
      target.clip.payload.reviewedRuntimeDependency = dependency.id;
      target.clip.payload.timingStatus =
        'placed after the reviewed movement-completion presentation; static query binding remains unbound';
    });
  }

  // ROM-only reopen of a new or wiped scene: recognize the exact choice
  // pattern the editor generates and restore its structure. The window's
  // dialogue card becomes a native choice (nativeChoice) and native cards in
  // each branch are tagged with their option (nativeChoiceBranch).
  function recoverChoices(document, ir) {
    var nodes = ir.nodes;
    var clipsByNode = {};
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        var nodeId = clip.source && clip.source.nodeId;
        if (nodeId) (clipsByNode[nodeId] = clipsByNode[nodeId] || []).push(clip);
      });
    });
    function wordsOf(node) { return node ? node.rawWords.map(unsigned) : []; }
    function matches(node, expected) {
      var actual = wordsOf(node);
      return actual.length === expected.length && expected.every(function(value, index) {
        return value === null || actual[index] === value;
      });
    }
    for (var index = 0; index < nodes.length; index++) {
      var windowWords = wordsOf(nodes[index]);
      if (windowWords[0] !== 0xBF || windowWords.length !== 14) continue;
      var windowId = windowWords[1];
      var head = [[0], [0x80000002], [0x80000000], [0x56], [0x25, 0, 1, 3], [0x05, windowId], [0x59, null],
        [0x80000002], [0x25, 0, 255, 3], [0x05, windowId], [0x59, null], [0x80000002], [0x80000000]];
      if (!head.every(function(expected, offset) { return matches(nodes[index + 1 + offset], expected); })) continue;
      var labelA = wordsOf(nodes[index + 7])[1], labelB = wordsOf(nodes[index + 11])[1];
      var cursor = index + 1 + head.length;
      if (!matches(nodes[cursor], [0x80000005, labelA]) || !matches(nodes[cursor + 1], [0x10, 0, 1, windowId])) continue;
      var startA = cursor + 2;
      cursor = startA;
      while (cursor < nodes.length && !(wordsOf(nodes[cursor])[0] === 0x59 && matches(nodes[cursor + 1], [0x80000005, labelB]))) cursor++;
      if (cursor >= nodes.length || !matches(nodes[cursor + 2], [0x10, 0, 1, windowId])) continue;
      var endA = cursor, labelJoin = wordsOf(nodes[endA])[1];
      var startB = endA + 3;
      cursor = startB;
      while (cursor < nodes.length && !(matches(nodes[cursor], [0x59, labelJoin]) && matches(nodes[cursor + 1], [0x80000005, labelJoin]))) cursor++;
      if (cursor >= nodes.length) continue;
      var endB = cursor;
      var choiceClip = (clipsByNode[nodes[index].id] || []).find(function(clip) { return clip.kind === 'dialogue'; });
      if (!choiceClip) continue;
      choiceClip.payload.nativeChoice = { windowId: windowId, labels: [labelA, labelB, labelJoin],
        branchEndNodeIds: [nodes[endA].id, nodes[endB].id] };
      [[startA, endA, 1], [startB, endB, 2]].forEach(function(range) {
        for (var at = range[0]; at < range[1]; at++) {
          (clipsByNode[nodes[at].id] || []).forEach(function(clip) {
            if (clip !== choiceClip) clip.payload.nativeChoiceBranch = { choiceClipId: choiceClip.id, option: range[2] };
          });
        }
      });
      index = endB + 1;
    }
  }

  // options.legacyBackground reproduces the projection from before background
  // selection (no environment or route-derived scene-group ownership), so a
  // Project saved then can be recognised as unchanged on import.
  function projectSceneDocument(scene, source, catalog, options) {
    var legacyBackground = !!(options && options.legacyBackground);
    var M = OB64.cutsceneModel;
    if (!M || !OB64.cutsceneCatalog) fail('Cutscene model and catalog modules are required.', 'module-order');
    var ir = createIr(scene, source.decodedBytes || source, source.nodeDefinitions);
    var document = OB64.cutsceneCatalog.createSceneDocument(scene);
    document.native.commands = [];
    document.tracks = [];
    var cursorFrame = 0;
    var actorPositions = {};
    var poseClipsByActor = {};
    var actorSelectors = {};
    var actorRecords = {};
    var nativeEffectClipsBySlot = {};
    var counterTiming = registeredCounterTiming(ir);
    var activeCounter = null;

    function actorsForNativeSlot(slot) {
      return slot === -1 ? document.actors.slice() : document.actors.filter(function(row) {
        return row.slot === slot;
      });
    }

    function closeNativeEffects(slot) {
      Object.keys(nativeEffectClipsBySlot).forEach(function(key) {
        if (slot !== -1 && Number(key) !== slot) return;
        var activeClip = nativeEffectClipsBySlot[key];
        activeClip.durationFrames = Math.max(0, cursorFrame - activeClip.startFrame);
        activeClip.source.originalDurationFrames = activeClip.durationFrames;
        if (activeClip.durationFrames === 0) activeClip.payload.nativeLifetimeEmpty = true;
        delete nativeEffectClipsBySlot[key];
      });
    }

    document.actors.forEach(function(actor) {
      actorPositions[actor.id] = { x: actor.initial.x, y: actor.initial.y, z: actor.initial.z };
      actorSelectors[actor.id] = {
        bank: actor.source.bank,
        key: actor.source.animationKey,
        facing: /^native-\d+$/.test(actor.initial.facing)
          ? Number(actor.initial.facing.slice(7)) : null,
        variant: Number.isInteger(actor.source.variantSelector) ? actor.source.variantSelector : 0
      };
      actorRecords[actor.id] = false;
    });

    ir.nodes.forEach(function(node) {
      var act = decodeNode(node);
      var projected = [];
      var actor, track, clip, current, distance, duration;
      var counterRole = counterTiming[node.id] || null;
      if(node.rawWords[0]===0xbf && node.rawWords.length===14 && source.rom && OB64.cutsceneAuthoring){
        var dw=node.rawWords, archive=catalog.getSerifuArchiveForPresentationSelector(dw[2]);
        // A 0x0888xxNN entry word is a launch translation-table operand: the
        // caller substitutes the real line before the Director parses it, so it
        // is not an entry of this archive. Keep it symbolic and byte-preserved.
        var translatedEntry=launchTranslationIndex(dw[3]);
        if(archive&&translatedEntry!==null){
          track=trackFor(document,'dialogue',null,'Native dialogue');
          clip=addClip(track,node,'dialogue',cursorFrame,90,{nativeDialogueEditable:false,sourceSystem:'serifu-launch-translated',presentationArchiveSelector:dw[2],presentationEntrySelector:null,launchTranslationIndex:translatedEntry,dialogueArchiveId:archive.archiveId,rawText:'',originalRawText:'',text:'(Line chosen by the game at launch: translation entry '+translatedEntry+')',speaker:'Launch-chosen line',nativeWords:dw.slice(),dialogueStatus:'launch translation-table operand; the caller supplies the line, so the preview cannot show its text'},M.capabilities.PREVIEW_ONLY);
          projected.push(clip.id);
        }else if(archive){
          var de=OB64.cutsceneAuthoring.dialogueEntry(source.rom,dw[2],dw[3]);
          track=trackFor(document,'dialogue',null,'Native dialogue');
          clip=addClip(track,node,'dialogue',cursorFrame,90,{nativeDialogueEditable:true,sourceSystem:'serifu-native',presentationArchiveSelector:dw[2],presentationEntrySelector:dw[3],dialogueArchiveId:archive.archiveId,dialogueEntryId:archive.entries[dw[3]]?archive.entries[dw[3]].entryId:archive.archiveId+':entry:'+dw[3],rawText:de.rawText,originalRawText:de.rawText,text:de.rawText,speaker:'Native dialogue',nativeWords:dw.slice()},M.capabilities.NATIVE);
          projected.push(clip.id);
        }
      }
      if (counterRole && counterRole.kind === 'arm') {
        activeCounter = { startFrame: cursorFrame, target: 0 };
      } else if (counterRole && counterRole.kind === 'gate' && activeCounter) {
        var gateFrame = activeCounter.startFrame + counterRole.target;
        var gateDuration = Math.max(0, gateFrame - cursorFrame);
        track = trackFor(document, 'flow', null, 'Scene flow');
        clip = addClip(track, node, 'wait', cursorFrame, gateDuration, {
          nativeTicks: counterRole.target,
          registeredCounterTarget: counterRole.target,
          compositeId: counterRole.compositeId,
          aButtonSkippable: counterRole.skippable === true,
          registeredCounterEditable: counterRole.editable === true,
          nativeClock: 'registered-counter-updates',
          timingStatus: 'registered counter target ' + counterRole.target +
            '; Preview placement is an explicit one-step-per-update approximation'
        }, counterRole.editable === true
          ? M.capabilities.NATIVE : M.capabilities.PREVIEW_ONLY);
        projected.push(clip.id);
        cursorFrame = Math.max(cursorFrame, gateFrame);
        activeCounter.target = counterRole.target;
      } else if (counterRole && counterRole.kind === 'reset') {
        activeCounter = null;
      } else if (act && act.type === 'control-overlap') {
        track = trackFor(document, 'flow', null, 'Scene flow');
        clip = addClip(track, node, 'control-overlap', cursorFrame, 0, {
          displayLabel: 'Overlapping native control entry',
          originalOpcode: act.originalOpcode,
          entryWord: act.entryWord,
          offsetWords: act.offsetWords,
          disposition: act.disposition,
          semantics: 'source bytes are preserved; actor projection is intentionally withheld'
        }, M.capabilities.NEEDS_RESEARCH);
        projected.push(clip.id);
      } else if (act && act.type === 'background-group') {
        var backgroundTable = catalog && catalog.getBackgroundSelectorTable
          ? catalog.getBackgroundSelectorTable(act.selectorTableId) : null;
        var backgroundEntry = catalog && catalog.getBackgroundSelectorEntry
          ? catalog.getBackgroundSelectorEntry(act.selectorTableId, act.selector) : null;
        var backgroundObservation = scene.backgroundRuntimeObservation || null;
        var calibratedStageProjection = backgroundObservation &&
          backgroundObservation.stageProjection
          ? JSON.parse(JSON.stringify(backgroundObservation.stageProjection)) : null;
        var launchDirectorMode = scene.launchProfile &&
          scene.launchProfile.directorMode || null;
        // Both routes need the catalog tables to offer complete rows.
        var environmentEditable = !legacyBackground && !!catalog && environmentRouteEditable(scene, node, act, ir);
        if (environmentEditable) {
          backgroundTable = catalog.getBackgroundSelectorTable(ENVIRONMENT_TABLE_ID);
          backgroundEntry = environmentBackgroundGroups(catalog).find(function(group) {
            return group.selector === act.selector;
          }) || null;
        }
        var nativeBackgroundEditable = !environmentEditable && (scene.engine === 'director' &&
          node.definition.editPolicy === 'typed-background-group' &&
          launchDirectorMode && launchDirectorMode.value === 0 ||
          !legacyBackground && !!catalog && sceneGroupRouteEditable(scene, node, act, ir));
        var nativeGroups = environmentEditable ? environmentBackgroundGroups(catalog) :
          nativeBackgroundEditable && backgroundTable
          ? backgroundTable.entries.filter(function(entry) {
            return Array.isArray(entry.archiveAssetIds) && entry.archiveAssetIds.length;
          }).map(function(entry) {
            return {
              selector: entry.selector,
              groupResourceKey: entry.groupResourceKey,
              archiveAssetIds: entry.archiveAssetIds.slice(),
              members: Array.isArray(entry.members) ? entry.members.map(function(member) {
                return { ordinal: member.ordinal, assetId: member.assetId };
              }) : entry.archiveAssetIds.map(function(assetId, index) {
                return { ordinal: index, assetId: assetId };
              }),
              associationStatus: entry.associationStatus
            };
          }) : [];
        document.background.projection = Object.assign({}, calibratedStageProjection || {}, {
          mode: calibratedStageProjection && calibratedStageProjection.mode ||
            (document.background.assetId ? 'stage-fit' : 'unresolved'),
          route: environmentEditable ? 'native-environment' : nativeBackgroundEditable ? 'native-scene-group' :
            (backgroundObservation && backgroundObservation.directorMode === 2
              ? 'mode-2-runtime-selector' : 'conditional-scene-group'),
          sourceNodeId: node.id,
          selectorTableId: environmentEditable ? ENVIRONMENT_TABLE_ID : act.selectorTableId,
          sourceSelector: act.selector,
          selectedSelector: nativeBackgroundEditable || environmentEditable ? act.selector : null,
          selectedGroupResourceKey: (nativeBackgroundEditable || environmentEditable) && backgroundEntry
            ? backgroundEntry.groupResourceKey : null,
          nativeEditable: nativeBackgroundEditable || environmentEditable,
          nativeGroups: nativeGroups,
          modeStatus: backgroundObservation && Number.isInteger(backgroundObservation.directorMode)
            ? 'runtime-observed Director mode ' + backgroundObservation.directorMode
            : (launchDirectorMode ? launchDirectorMode.status :
              'runtime Director mode unresolved for this asset'),
          previewOverride: false
        });
        if ((nativeBackgroundEditable || environmentEditable) && backgroundEntry) {
          document.background.assetId = backgroundEntry.archiveAssetIds[0] || null;
          document.background.capability = M.capabilities.NATIVE;
          document.background.layers = backgroundGroupLayers(
            backgroundEntry, catalog, M.capabilities.NATIVE,
            environmentEditable ? 'native-environment' : 'native-scene-group');
          document.background.projection.mode = document.background.layers.length
            ? 'stage-fit' : 'unresolved';
        }
      } else if (act && act.type === 'place') {
        actor = actorForSlot(document, scene, act.slot);
        actorRecords[actor.id] = true;
        var placeSelector = act.selectorValid && catalog && catalog.getPoseSelector
          ? catalog.getPoseSelector(act.bank, act.key, act.facing) : null;
        var placeSelectorResolved = !!(act.selectorValid && (!catalog || placeSelector));
        var placeProgram = placeSelectorResolved && catalog && catalog.getPhysicalPoseProgram
          ? catalog.getPhysicalPoseProgram(
            act.bank, act.key, act.facing, act.variantSelector) : null;
        actor.source.placeNodeId = node.id;
        actor.source.originalPlaceStartFrame = cursorFrame;
        actor.initial.x = act.x == null ? actor.initial.x : act.x;
        actor.initial.y = act.y == null ? actor.initial.y : act.y;
        actor.initial.z = act.z == null ? actor.initial.z : act.z;
        if (placeSelectorResolved) {
          actor.capability = act.variantSelectorTranslationIndex !== null
            ? M.capabilities.NEEDS_RESEARCH
            : (placeProgram || !catalog
              ? M.capabilities.NATIVE : M.capabilities.NEEDS_RESEARCH);
          actor.artSourceId = 'cutscene-art-bank:' + act.bank;
          actor.initial.facing = 'native-' + act.facing;
          actor.initial.poseId = poseIdentity(act);
          actor.source.bank = act.bank;
          actor.source.animationKey = act.key;
          actor.source.renderMode = act.renderMode;
          actor.source.renderModeStatus =
            'exact opcode-0x14 actor render-mode byte; native values 0 through 3 are observed';
          actor.source.variantSelector = act.variantSelector;
          actor.source.variantSelectorStatus = act.variantSelectorStatus;
          actor.source.variantSelectorTranslationIndex =
            act.variantSelectorTranslationIndex;
          actor.source.physicalStateId = placeSelector ? placeSelector.physicalStateId : null;
          actor.source.stateIndex = placeSelector ? placeSelector.stateIndex : null;
          actor.source.selectorStatus = placeProgram || !catalog
            ? 'physical state and pose program located'
            : 'physical state located; selected descriptor variant has no source program';
          actorSelectors[actor.id] = {
            bank: act.bank, key: act.key, facing: act.facing,
            variant: act.variantSelector
          };
        } else {
          actor.capability = M.capabilities.NEEDS_RESEARCH;
          actor.source.selectorStatus = act.selectorValid
            ? 'opcode-0x14 tuple has no physical state in the bank selector table'
            : 'opcode-0x14 selector lies outside the supported numeric domain';
          actor.source.rawSelector = {
            bank: act.bank, animationKey: act.key, facing: act.facing,
            variantSelector: act.variantSelector
          };
        }
        actor.source.original = act;
        actor.source.editPolicy = node.definition.editPolicy;
        actor.source.displayInitial = {
          x: actor.initial.x, y: actor.initial.y, z: actor.initial.z,
          facing: actor.initial.facing
        };
        actorPositions[actor.id] = { x: actor.initial.x, y: actor.initial.y, z: actor.initial.z };
        if (cursorFrame > 0) {
          actor.initial.visible = false;
          track = trackFor(document, 'actor', actor, actor.label + ' visibility');
          clip = addClip(track, node, 'enter', cursorFrame, 0, { visible: true });
          projected.push(clip.id);
        }
      } else if (act && act.type === 'state') {
        actor = actorForSlot(document, scene, act.slot);
        var recordAvailable = actorRecords[actor.id] === true;
        var previousSelector = actorSelectors[actor.id] || {};
        var effectiveVariant = act.variantSelector === -1
          ? previousSelector.variant : act.variantSelector;
        var selectorState = act.selectorValid && catalog && catalog.getPoseSelector
          ? catalog.getPoseSelector(act.bank, act.key, act.facing) : null;
        var selectorResolved = !!(act.selectorValid && (!catalog || selectorState));
        var selectorProgram = selectorResolved && catalog && catalog.getPhysicalPoseProgram
          ? catalog.getPhysicalPoseProgram(act.bank, act.key, act.facing, effectiveVariant) : null;
        var selectorProgramLocated = !!(selectorResolved && (!catalog || selectorProgram));
        var selectorApplied = !!(recordAvailable && selectorResolved);
        track = trackFor(document, 'pose', actor, actor.label + ' pose');
        clip = addClip(track, node, 'pose', cursorFrame, 1, {
          poseId: selectorApplied ? poseIdentity(act) : actor.initial.poseId,
          bank: selectorApplied ? act.bank :
            (previousSelector.bank == null ? null : previousSelector.bank),
          animationKey: selectorApplied ? act.key :
            (previousSelector.key == null ? null : previousSelector.key),
          facing: selectorApplied ? 'native-' + act.facing : actor.initial.facing,
          nativeFacing: selectorApplied ? act.facing :
            (previousSelector.facing == null ? null : previousSelector.facing),
          variantSelector: selectorApplied ? effectiveVariant :
            (previousSelector.variant == null ? null : previousSelector.variant),
          variantSelectorStatus: act.variantSelector === -1
            ? 'preserve current runtime selector' : 'exact opcode-0x03 operand 8',
          selectorStatus: selectorApplied && selectorProgramLocated
            ? 'physical state and selected pose program located'
            : (selectorApplied
              ? 'physical state located; selected descriptor variant has no source program'
            : (!recordAvailable
              ? 'no earlier actor-record producer in this source path'
              : (act.selectorValid
                ? 'selector tuple has no physical state in the bank table'
                : 'selector lies outside the supported numeric domain'))),
          recordStatus: recordAvailable
            ? 'actor record produced earlier in this source path'
            : 'native command requires an external or missing actor-record producer',
          nativeApplied: selectorApplied,
          physicalStateId: selectorState ? selectorState.physicalStateId : null,
          stateIndex: selectorState ? selectorState.stateIndex : null,
          rawSelector: selectorApplied ? null : {
            bank: act.bank, animationKey: act.key, facing: act.facing,
            variantSelector: act.variantSelector
          },
          x: act.x,
          y: act.y,
          z: act.z,
          loop: true,
          timingStatus: 'replacement-window-preview'
        }, selectorApplied && selectorProgramLocated && node.definition.editPolicy === 'typed-state'
          ? M.capabilities.NATIVE : M.capabilities.PREVIEW_ONLY);
        projected.push(clip.id);
        if (!poseClipsByActor[actor.id]) poseClipsByActor[actor.id] = [];
        poseClipsByActor[actor.id].push(clip);
        current = actorPositions[actor.id] || { x: 0, y: 0, z: 0 };
        if (selectorApplied && act.x != null) current.x = act.x;
        if (selectorApplied && act.y != null) current.y = act.y;
        if (selectorApplied && act.z != null) current.z = act.z;
        if (selectorApplied) actorPositions[actor.id] = current;
        if (selectorApplied) {
          actorSelectors[actor.id] = {
            bank: act.bank, key: act.key, facing: act.facing,
            variant: effectiveVariant
          };
        }
      } else if (act && act.type === 'move') {
        actor = actorForSlot(document, scene, act.slot);
        track = trackFor(document, 'movement', actor, actor.label + ' movement');
        current = actorPositions[actor.id] || { x: 0, y: 0, z: 0 };
        distance = Math.hypot(act.x - current.x, act.z - current.z);
        duration = act.speed > 0 ? Math.max(1, Math.trunc(distance / act.speed)) : 1;
        clip = addClip(track, node, 'movement', cursorFrame, duration, {
          from: { x: current.x, y: current.y, z: current.z },
          to: { x: act.x, y: current.y, z: act.z },
          nativeSpeed: act.speed,
          durationMode: 'speed',
          timingStatus: 'native distance-over-speed truncation shown as Preview steps'
        }, node.definition.editPolicy === 'typed-move'
          ? M.capabilities.NATIVE : M.capabilities.PREVIEW_ONLY);
        projected.push(clip.id);
        actorPositions[actor.id] = { x: act.x, y: current.y, z: act.z };
      } else if (act && act.type === 'actor-remove') {
        actorsForNativeSlot(act.slot).forEach(function(targetActor) {
          track = trackFor(document, 'actor', targetActor, targetActor.label + ' lifecycle');
          clip = addClip(track, node, 'exit', cursorFrame, 0, {
            visible: false,
            recordRemoved: true,
            nativeSlotSelector: act.slot,
            semantics: 'actor record freed and slot pointer cleared'
          }, M.capabilities.NATIVE);
          projected.push(clip.id);
          actorRecords[targetActor.id] = false;
        });
      } else if (act && act.type === 'actor-opacity') {
        actorsForNativeSlot(act.slot).forEach(function(targetActor) {
          track = trackFor(document, 'actor', targetActor, targetActor.label + ' opacity');
          clip = addClip(track, node, 'opacity', cursorFrame, 0, {
            opacityByte: act.opacityByte,
            nativeSlotSelector: act.slot,
            semantics: 'actor render alpha multiplier'
          }, scene.engine === 'director' && act.slot !== -1 &&
              node.definition.editPolicy === 'typed-actor-opacity'
            ? M.capabilities.NATIVE : M.capabilities.PREVIEW_ONLY);
          projected.push(clip.id);
        });
      } else if (act && act.type === 'wait') {
        track = trackFor(document, 'flow', null, 'Scene flow');
        clip = addClip(track, node, 'wait', cursorFrame, act.ticks, {
          nativeTicks: act.ticks,
          timingStatus: 'registered-native-tick-shown-as-preview-frame'
        });
        projected.push(clip.id);
        cursorFrame += act.ticks;
      } else if (act && act.type === 'projection-transform') {
        track = trackFor(document, 'camera', null, 'Scene projection');
        clip = addClip(track, node, 'camera', cursorFrame,
          Math.max(1, act.nativeCountdownLow16), {
          presentationKind: 'projection-transform',
          displayLabel: 'Animate scene projection',
          nativeOperands: act.operands,
          target: act.target,
          nativeCountdown: act.nativeCountdown,
          nativeCountdownLow16: act.nativeCountdownLow16,
          timingStatus: act.clockStatus +
            '; shown one-for-one as Preview steps without claiming display frames',
          fieldMapStatus: 'render-matrix-consumer-joined'
        }, M.capabilities.NATIVE);
        projected.push(clip.id);
      } else if (act && act.type === 'camera-pose') {
        track = trackFor(document, 'camera', null, 'Camera pose');
        clip = addClip(track, node, 'camera', cursorFrame, 0, {
          presentationKind: 'camera-pose',
          displayLabel: act.bank === 'registered-object'
            ? 'Set registered-object camera pose' : 'Set actor-side camera pose',
          cameraBank: act.bank,
          target: act.target,
          eye: act.eye,
          fovY: act.fovY,
          nativeOperands: act.operands,
          timingStatus: 'instant native camera pose assignment'
        }, M.capabilities.PREVIEW_ONLY);
        projected.push(clip.id);
      } else if (act && act.type === 'projection-identity-transition') {
        track = trackFor(document, 'camera', null, 'Scene projection');
        clip = addClip(track, node, 'camera', cursorFrame,
          Math.max(1, act.nativeCountdownLow16), {
          presentationKind: 'projection-identity-transition',
          displayLabel: 'Return scene projection to identity',
          nativeCountdown: act.nativeCountdown,
          nativeCountdownLow16: act.nativeCountdownLow16,
          timingStatus: act.clockStatus
        }, M.capabilities.PREVIEW_ONLY);
        projected.push(clip.id);
      } else if (act && act.type === 'audio-controller') {
        var audioAssociation = (scene.audioAssociations || []).find(function(association) {
          return association.occurrenceId === node.id ||
            association.wordStart === node.definition.startWord;
        }) || null;
        var nativeBlockIndex = act.opcode === '0x6E' && act.operands[0] === 0 &&
          act.operands[1] === 0 ? nativeAudioBlockIndex(act.operands[2]) : null;
        var derivedAudioBlockId = nativeBlockIndex == null ? null :
          'sequenced-audio:' + String(nativeBlockIndex).padStart(2, '0');
        var audioBlockId = audioAssociation && audioAssociation.audioBlockId ||
          derivedAudioBlockId;
        var audioBlock = audioBlockId && catalog
          ? catalog.getAudioBlock(audioBlockId) : null;
        var registeredAudio = audioAssociation &&
          audioAssociation.registeredAudioRequestAssetId && catalog
          ? catalog.getRegisteredAudioRequest(
            audioAssociation.registeredAudioRequestAssetId) : null;
        var nativeBlockRequestEditable = act.opcode === '0x6E' &&
          act.operands[0] === 0 && act.operands[1] === 0 &&
          nativeBlockIndex !== null &&
          node.definition.editPolicy === 'typed-audio-block-request' &&
          !!audioBlockId && (!catalog || !!audioBlock);
        track = trackFor(document, 'audio', null, 'Native audio controller');
        clip = addClip(track, node, 'audio', cursorFrame, 0, {
          cue: audioBlock ? audioBlock.label :
            (registeredAudio ? registeredAudio.label :
              act.opcode + ' · operands ' + act.operands.join(', ')),
          sourceSystem: 'director-native',
          nativeOpcode: act.opcode,
          nativeOperands: act.operands,
          nativeRequestValue: nativeBlockRequestEditable ? act.operands[2] : null,
          nativeBlockRequestEditable: nativeBlockRequestEditable,
          audioBlockId: audioBlockId,
          registeredAudioRequestAssetId: audioAssociation &&
            audioAssociation.registeredAudioRequestAssetId || null,
          originalNativeAudio: {
            sourceSystem: 'director-native',
            nativeOpcode: act.opcode,
            nativeOperands: act.operands.slice(),
            nativeRequestValue: nativeBlockRequestEditable ? act.operands[2] : null,
            nativeBlockRequestEditable: nativeBlockRequestEditable,
            audioBlockId: audioBlockId,
            registeredAudioRequestAssetId: audioAssociation &&
              audioAssociation.registeredAudioRequestAssetId || null,
            cue: audioBlock ? audioBlock.label :
              (registeredAudio ? registeredAudio.label :
                act.opcode + ' · operands ' + act.operands.join(', ')),
            associationStatus: audioAssociation
              ? audioAssociation.associationStatus
              : (derivedAudioBlockId
                ? 'exact selector-zero sequenced-audio block route'
                : 'controller-to-resource join unresolved')
          },
          associationStatus: audioAssociation
            ? audioAssociation.associationStatus
            : (derivedAudioBlockId
              ? 'exact selector-zero sequenced-audio block route'
            : (act.opcode === '0x70'
              ? 'runtime handle parameter; not a resource identifier'
              : 'controller-to-resource join unresolved'))
        }, nativeBlockRequestEditable ? M.capabilities.NATIVE :
          (audioAssociation ? M.capabilities.PREVIEW_ONLY : M.capabilities.NEEDS_RESEARCH));
        projected.push(clip.id);
      } else if (act && act.type === 'native-sprite-effect') {
        closeNativeEffects(act.slot);
        var effectSelector = act.selectorValid && catalog && catalog.getPoseSelector
          ? catalog.getPoseSelector(act.bank, act.key, act.facing) : null;
        var effectSelectorResolved = act.selectorValid && (!catalog || effectSelector);
        var effectProgram = effectSelectorResolved && catalog && catalog.getPhysicalPoseProgram
          ? catalog.getPhysicalPoseProgram(
            act.bank, act.key, act.facing, act.variantSelector) : null;
        track = trackFor(document, 'effect', null, 'Native Cutscene sprite effects');
        clip = addClip(track, node, 'effect', cursorFrame, 1, {
          sourceSystem: 'cutscene-sprite-native',
          nativeOpcode: act.opcode,
          nativeOperands: act.operands,
          nativeEffectSlot: act.slot,
          resourceRootKey: 0x0109F95E,
          poseId: effectSelectorResolved ? poseIdentity(act) : null,
          bank: act.bank,
          animationKey: act.key,
          nativeFacing: act.facing,
          variantSelector: act.variantSelector,
          renderPassSelector: act.renderPassSelector,
          nativeModelX: act.modelX,
          nativeModelY: act.modelY,
          stageX: 160 + act.modelX,
          // modelY is the y-up value the record stores; Stage rows run downward.
          stageY: 120 - act.modelY,
          scale: act.scale,
          coordinateStatus: 'center-origin Stage preview; native model translation is exact but final viewport calibration remains unresolved',
          selectorStatus: effectSelectorResolved
            ? (effectProgram || !catalog
              ? 'physical state and pose program located'
              : 'physical state located; selected descriptor variant has no source program')
            : 'effect selector has no physical state in the bank table',
          associationStatus: 'exact opcode-0x46 cutscene-art renderer and pose selector'
        }, effectProgram || !catalog
          ? (scene.engine === 'director' &&
              node.definition.editPolicy === 'typed-native-sprite-effect'
            ? M.capabilities.NATIVE : M.capabilities.PREVIEW_ONLY)
          : M.capabilities.NEEDS_RESEARCH);
        projected.push(clip.id);
        nativeEffectClipsBySlot[act.slot] = clip;
      } else if (act && act.type === 'native-sprite-effect-remove') {
        closeNativeEffects(act.slot);
        track = trackFor(document, 'flow', null, 'Scene flow');
        clip = addClip(track, node, 'effect-remove', cursorFrame, 0, {
          displayLabel: act.slot === -1
            ? 'Remove all Cutscene sprite effects' : 'Remove Cutscene sprite effect ' + act.slot,
          nativeEffectSlot: act.slot,
          semantics: 'free 0x50-byte effect record and clear its slot pointer'
        }, M.capabilities.NATIVE);
        projected.push(clip.id);
      } else if (act && act.type === 'native-effect' && act.opcode === '0x1B' &&
          node.definition.editPolicy === 'typed-fade' && act.operands.length === 6) {
        track = trackFor(document, 'effect', null, 'Scene fades');
        clip = addClip(track, node, 'fade', cursorFrame, Math.max(1, act.operands[0]), nativeFadePayload(act.operands),
          M.capabilities.NATIVE);
        projected.push(clip.id);
      } else if (act && act.type === 'native-effect') {
        track = trackFor(document, 'effect', null, 'Native Director effects');
        clip = addClip(track, node, 'effect', cursorFrame, 0, {
          sourceSystem: 'director-native',
          nativeOpcode: act.opcode,
          nativeOperands: act.operands,
          resourceRootKey: null,
          associationStatus: 'native state event; visual resource selector unresolved'
        }, M.capabilities.NEEDS_RESEARCH);
        projected.push(clip.id);
      } else if (node.definition.terminationKind) {
        track = trackFor(document, 'flow', null, 'Scene flow');
        clip = addClip(track, node, 'end', cursorFrame, 0, {
          displayLabel: 'End Director stream',
          terminationKind: node.definition.terminationKind
        });
        projected.push(clip.id);
      }
      document.native.commands.push(nativeCommand(node, projected,
        ir.program.compositeByNodeId[node.id] || null));
    });

    applyReviewedTimelineOverlay(document, scene);
    if (scene.authored || scene.wiped) recoverChoices(document, ir);

    var sceneEnd = Math.max(1, cursorFrame + 1);
    Object.keys(poseClipsByActor).forEach(function(actorId) {
      var clips = poseClipsByActor[actorId];
      clips.forEach(function(clip, index) {
        var end = clips[index + 1] ? clips[index + 1].startFrame : sceneEnd;
        clip.durationFrames = Math.max(1, end - clip.startFrame);
        clip.source.originalDurationFrames = clip.durationFrames;
      });
    });
    Object.keys(nativeEffectClipsBySlot).forEach(function(slot) {
      var activeClip = nativeEffectClipsBySlot[slot];
      activeClip.durationFrames = Math.max(1, sceneEnd - activeClip.startFrame);
      activeClip.source.originalDurationFrames = activeClip.durationFrames;
    });
    document.native.gaps = scene.source.gaps.slice();
    document.exportRequirements = {
      capability: M.capabilities.NATIVE,
      reasons: [],
      allocationBytes: 0,
      features: ['director-fixed-slot', 'source-preimage-verified'].concat(
        document.background.projection.nativeEditable ? ['native-background-group'] : [])
    };
    M.validateSceneDocument(document);
    return { document: document, ir: ir, program: ir.program, source: source };
  }

  function integer(value, label, minimum, maximum) {
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
      fail(label + ' must be an integer from ' + minimum + ' through ' + maximum + '.',
        'compile-value');
    }
    return value;
  }

  function fixed(value, label, nullable) {
    if (value == null && nullable) return -1000;
    if (typeof value !== 'number' || !Number.isFinite(value)) {
      fail(label + ' must be a finite number.', 'compile-value');
    }
    var scaled = Math.round(value * 1000);
    if (scaled < -0x80000000 || scaled > 0x7FFFFFFF ||
        Math.abs(scaled / 1000 - value) > 1e-9 || nullable && scaled === -1000) {
      fail(label + ' cannot be represented at native x1000 precision.', 'compile-value');
    }
    return scaled;
  }

  function scaledInteger(value, multiplier, label, minimum, maximum) {
    if (typeof value !== 'number' || !Number.isFinite(value)) {
      fail(label + ' must be a finite number.', 'compile-value');
    }
    var scaled = Math.round(value * multiplier);
    if (scaled < minimum || scaled > maximum ||
        Math.abs(scaled / multiplier - value) > 1e-9) {
      fail(label + ' cannot be represented at native x' + multiplier + ' precision.',
        'compile-value');
    }
    return scaled;
  }

  function nativeFacing(value, fallback) {
    if (Number.isInteger(value)) return integer(value, 'Facing', 0, 3);
    var match = String(value || '').match(/^native-([0-3])$/);
    return match ? Number(match[1]) : integer(fallback, 'Facing', 0, 3);
  }

  function sameNumber(left, right) {
    return left === right || left == null && right == null;
  }

  function actorMatchesPlace(actor, act) {
    var bankMatch = String(actor.artSourceId || '').match(/^cutscene-art-bank:(\d+)$/);
    var bank = bankMatch ? Number(bankMatch[1]) : actor.source.bank;
    var facingMatch = String(actor.initial.facing || '').match(/^native-(\d+)$/);
    var displayed = actor.source.displayInitial || actor.initial;
    return actor.slot === act.slot && bank === act.bank &&
      actor.source.animationKey === act.key && facingMatch && Number(facingMatch[1]) === act.facing &&
      actor.source.variantSelector === act.variantSelector &&
      (actor.source.renderMode == null || actor.source.renderMode === act.renderMode) &&
      sameNumber(actor.initial.x, displayed.x) && sameNumber(actor.initial.y, displayed.y) &&
      sameNumber(actor.initial.z, displayed.z);
  }

  function clipMatchesState(clip, slot, act) {
    var variantMatches = act.variantSelector === -1
      ? clip.payload.variantSelectorStatus === 'preserve current runtime selector'
      : clip.payload.variantSelector === act.variantSelector;
    return slot === act.slot && clip.payload.bank === act.bank &&
      clip.payload.animationKey === act.key && clip.payload.nativeFacing === act.facing &&
      variantMatches &&
      sameNumber(clip.payload.x, act.x) && sameNumber(clip.payload.y, act.y) &&
      sameNumber(clip.payload.z, act.z);
  }

  function clipMatchesMove(clip, slot, act) {
    var to = clip.payload.to || {};
    return slot === act.slot && sameNumber(to.x, act.x) && sameNumber(to.z, act.z) &&
      sameNumber(clip.payload.nativeSpeed, act.speed) &&
      clip.durationFrames === clip.source.originalDurationFrames;
  }

  function clipMatchesCamera(clip, act) {
    var target = clip.payload.target || clip.payload.to || {};
    return sameNumber(target.translateX, act.target.translateX) &&
      sameNumber(target.translateY, act.target.translateY) &&
      sameNumber(target.scaleX, act.target.scaleX) &&
      sameNumber(target.scaleY, act.target.scaleY) &&
      clip.durationFrames === clip.source.originalDurationFrames;
  }

  function nativeEffectModelCoordinate(payload, axis) {
    var stageKey = axis === 'x' ? 'stageX' : 'stageY';
    var nativeKey = axis === 'x' ? 'nativeModelX' : 'nativeModelY';
    var stageCenter = axis === 'x' ? 160 : 120;
    if (!Number.isFinite(payload[stageKey])) return payload[nativeKey];
    // Stage X grows rightward like the model; Stage Y runs downward while the
    // stored model value is y-up.
    return axis === 'x' ? payload[stageKey] - stageCenter : stageCenter - payload[stageKey];
  }

  function clipMatchesNativeSpriteEffect(clip, act) {
    var payload = clip.payload || {};
    return payload.sourceSystem === 'cutscene-sprite-native' &&
      payload.nativeEffectSlot === act.slot && payload.bank === act.bank &&
      payload.animationKey === act.key && payload.nativeFacing === act.facing &&
      payload.variantSelector === act.variantSelector &&
      payload.renderPassSelector === act.renderPassSelector &&
      sameNumber(nativeEffectModelCoordinate(payload, 'x'), act.modelX) &&
      sameNumber(nativeEffectModelCoordinate(payload, 'y'), act.modelY) &&
      sameNumber(payload.scale, act.scale);
  }

  function clipMatchesActorByte(clip, slot, act, field) {
    var value = field === 'selector' ? act.selector : act.opacityByte;
    return slot === act.slot && clip.payload[field] === value;
  }

  function clipMatchesAudioBlockRequest(clip, act) {
    var payload = clip.payload || {};
    return payload.sourceSystem === 'director-native' &&
      payload.nativeOpcode === '0x6E' && payload.nativeBlockRequestEditable === true &&
      payload.nativeRequestValue === act.operands[2] &&
      payload.audioBlockId === 'sequenced-audio:' +
        String(nativeAudioBlockIndex(payload.nativeRequestValue)).padStart(2, '0');
  }

  function compilePlace(actor, rawWords) {
    var original = actor.source.original || {};
    var displayed = actor.source.displayInitial || {};
    var bankMatch = String(actor.artSourceId || '').match(/^cutscene-art-bank:(\d+)$/);
    var bank = bankMatch ? Number(bankMatch[1]) : original.bank;
    var renderMode = actor.source.renderMode == null
      ? (original.renderMode == null ? 0 : original.renderMode) : actor.source.renderMode;
    var renderModeWord = rawWords && renderMode === original.renderMode
      ? unsigned(rawWords[8]) : integer(renderMode, 'Actor render mode', 0, 255);
    var variantSelector = actor.source.variantSelector == null
      ? (rawWords ? unsigned(rawWords[9]) & 0xFF : 0) : actor.source.variantSelector;
    var variantWord = rawWords && original.variantSelectorTranslationIndex != null &&
      variantSelector === original.variantSelector
      ? unsigned(rawWords[9])
      : integer(variantSelector, 'Appearance selector', 0, 255);
    function compiledCoordinate(axis) {
      return original[axis] == null && sameNumber(actor.initial[axis], displayed[axis])
        ? null : actor.initial[axis];
    }
    return [0x14, integer(actor.slot, 'Actor slot', 0, 255),
      integer(bank, 'Actor Art Source bank', 0, 255),
      integer(actor.source.animationKey == null ? original.key : actor.source.animationKey,
        'Animation key', 0, 0xFFFF),
      nativeFacing(actor.initial.facing, original.facing),
      fixed(compiledCoordinate('x'), 'Actor X', true),
      fixed(compiledCoordinate('y'), 'Actor Y', true),
      fixed(compiledCoordinate('z'), 'Actor Z', true),
      renderModeWord,
      variantWord];
  }

  function compileEnter(clip, actor) {
    var payload = clip.payload || {};
    var source = actor.source || {};
    var bankMatch = String(actor.artSourceId || '').match(/^cutscene-art-bank:(\d+)$/);
    var bank = Number.isInteger(payload.bank) ? payload.bank :
      (bankMatch ? Number(bankMatch[1]) : source.bank);
    var animationKey = Number.isInteger(payload.animationKey)
      ? payload.animationKey : source.animationKey;
    var facing = Number.isInteger(payload.nativeFacing)
      ? payload.nativeFacing : nativeFacing(payload.facing, nativeFacing(actor.initial.facing, 0));
    function coordinate(axis) {
      return Number.isFinite(payload[axis]) ? payload[axis] : actor.initial[axis];
    }
    return [0x14, integer(actor.slot, 'Actor slot', 0, 255),
      integer(bank, 'Actor Art Source bank', 0, 255),
      integer(animationKey, 'Animation key', 0, 0xFFFF),
      integer(facing, 'Facing', 0, 3),
      fixed(coordinate('x'), 'Actor X', false),
      fixed(coordinate('y'), 'Actor Y', false),
      fixed(coordinate('z'), 'Actor Z', false),
      integer(Number.isInteger(payload.renderMode) ? payload.renderMode :
        (Number.isInteger(source.renderMode) ? source.renderMode : 0),
      'Actor render mode', 0, 255),
      integer(Number.isInteger(payload.variantSelector) ? payload.variantSelector :
        (Number.isInteger(source.variantSelector) ? source.variantSelector : 0),
      'Appearance selector', 0, 255)];
  }

  function compileState(clip, slotOverride) {
    var payload = clip.payload;
    var slot = slotOverride == null ? clip.source.slot : slotOverride;
    return [3, integer(slot, 'Actor slot', 0, 255),
      integer(payload.bank, 'Actor Art Source bank', 0, 255),
      integer(payload.animationKey, 'Animation key', 0, 0xFFFF),
      nativeFacing(payload.facing, payload.nativeFacing),
      fixed(payload.x, 'State X', true), fixed(payload.y, 'State Y', true),
      fixed(payload.z, 'State Z', true),
      integer(payload.variantSelector, 'Appearance selector', -1, 255)];
  }

  function compileMove(clip, slotOverride) {
    var payload = clip.payload;
    var from = payload.from || {};
    var to = payload.to || {};
    var speed = payload.nativeSpeed;
    if (clip.durationFrames !== clip.source.originalDurationFrames) {
      var distance = Math.hypot(Number(to.x) - Number(from.x), Number(to.z) - Number(from.z));
      if (!Number.isFinite(distance) || distance <= 0 || clip.durationFrames < 1) {
        fail('Movement duration needs distinct finite endpoints.', 'compile-timing');
      }
      speed = Math.round(distance / clip.durationFrames * 1000) / 1000;
    }
    if (!(speed > 0)) fail('Movement speed must be greater than zero.', 'compile-value');
    var slot = slotOverride == null ? clip.source.slot : slotOverride;
    return [7, integer(slot, 'Actor slot', 0, 255), -1000, -1000,
      fixed(to.x, 'Movement X', false), fixed(to.z, 'Movement Z', false), 0,
      fixed(speed, 'Movement speed', false)];
  }

  function compileCamera(clip, rawWords) {
    var target = clip.payload.target || clip.payload.to || {};
    var divisor = rawWords && clip.durationFrames === clip.source.originalDurationFrames
      ? signed(rawWords[5]) : integer(clip.durationFrames,
        'Projection update count', 1, 0xFFFF);
    return [0x2C,
      fixed(target.translateX, 'Projection translation X', false),
      fixed(target.translateY, 'Projection translation Y', false),
      fixed(target.scaleX, 'Projection scale X', false),
      fixed(target.scaleY, 'Projection scale Y', false),
      divisor];
  }

  function compileNativeSpriteEffect(clip, rawWords) {
    var payload = clip.payload || {};
    var rawOperands = rawWords ? rawWords.slice(1).map(signed) : null;
    var modelX = integer(nativeEffectModelCoordinate(payload, 'x'),
      'Effect model X', -0x80000000, 0x7FFFFFFF);
    var modelY = integer(nativeEffectModelCoordinate(payload, 'y'),
      'Effect model Y', -0x7FFFFFFF, 0x7FFFFFFF);
    var slot = integer(payload.nativeEffectSlot, 'Effect slot', 0, 29);
    var bank = integer(payload.bank, 'Effect Actor Art Source bank', 1, 68);
    var animationKey = integer(payload.animationKey, 'Effect animation key', 0, 0xFFFF);
    var facing = integer(payload.nativeFacing, 'Effect facing', 0, 0xFFFF);
    var renderPass = integer(payload.renderPassSelector, 'Effect render pass', 0, 255);
    var scaleWord = scaledInteger(payload.scale, 100, 'Effect scale',
      -0x80000000, 0x7FFFFFFF);
    var variant = integer(payload.variantSelector, 'Effect appearance selector', 0, 255);
    function preserved(index, semanticValue, originalSemantic) {
      return rawOperands && sameNumber(semanticValue, originalSemantic)
        ? rawOperands[index] : semanticValue;
    }
    return [0x46,
      preserved(0, slot, rawOperands && rawOperands[0] & 0xFF),
      preserved(1, bank, rawOperands && rawOperands[1]),
      preserved(2, animationKey, rawOperands && rawOperands[2]),
      preserved(3, facing, rawOperands && rawOperands[3]),
      preserved(4, modelX, rawOperands && rawOperands[4]),
      preserved(5, -modelY, rawOperands && rawOperands[5]),
      preserved(6, renderPass, rawOperands && rawOperands[6] & 0xFF),
      preserved(7, scaleWord, rawOperands && rawOperands[7]),
      preserved(8, variant, rawOperands && rawOperands[8] & 0xFF)];
  }

  function compileAuthoredNativeSpriteEffect(clip, waitUnit) {
    var slot = integer(clip.payload.nativeEffectSlot, 'Effect slot', 0, 29);
    var ticks = integer(clip.durationFrames, 'Effect lifetime', 1, 0x7FFFFFFF);
    return compileNativeSpriteEffect(clip, null).concat(
      waitUnit ? waitUnit(ticks) : [1, 0, 0x80000002, 0x80000000, 0x0E, 0, ticks, 2],
      [0x66, slot]);
  }

  // Registered-counter spans: from an arm to its reset, gates compare the one
  // shared counter against absolute targets. A wait inserted inside a span must
  // not re-arm or reset that counter, or later gates never pass. Instead it
  // becomes a bare gate at the running target, and later gates shift by the
  // same amount. Outside a span a wait is the complete arm/gate/reset unit.
  var COUNTER_QUERIES = ['registered_counter_query', 'a_button_skippable_registered_wait_query'];

  function counterSpanContext(ir) {
    var context = {};
    var armed = false;
    var spanId = null;
    var lastTarget = 0;
    var compareOp = 0;
    ir.nodes.forEach(function(node) {
      var name = node.definition.name;
      if (name === 'registered_counter_arm') {
        armed = true; spanId = node.id; lastTarget = 0; compareOp = 0;
        context[node.id] = { armed: false, spanId: null };
        return;
      }
      context[node.id] = { armed: armed, spanId: spanId, lastTarget: lastTarget, compareOp: compareOp };
      if (armed && COUNTER_QUERIES.indexOf(name) !== -1 && node.rawWords.length >= 3) {
        compareOp = signed(node.rawWords[1]);
        lastTarget = signed(node.rawWords[2]);
      }
      if (name === 'registered_counter_reset') { armed = false; spanId = null; }
    });
    return context;
  }

  // Full-screen color overlay fade (opcode 0x1B): duration in Director updates,
  // overlay RGB, starting alpha (-1 keeps the current overlay alpha), target alpha.
  // Authored card kinds whose native command starts an asynchronous job and
  // hands over at once.
  var ASYNC_CARD_KINDS = ['movement', 'camera', 'fade'];
  // Updates a job needs beyond its duration before its last change is visible.
  // Measured in the native runtime model: a Move reaches its target and a
  // projection transition completes within their duration; a 0x1B fade applies
  // its final alpha one update after its duration.
  function completionExtraUpdates(kind) { return kind === 'fade' ? 1 : 0; }

  // Card payload for a native 0x1B [duration, red, green, blue, startAlpha,
  // targetAlpha]; startAlpha -1 continues from the current overlay.
  function nativeFadePayload(operands) {
    var start = operands[4] === -1 ? null : operands[4];
    var direction = 'custom';
    if (operands[1] === 0 && operands[2] === 0 && operands[3] === 0) {
      if (operands[5] === 255 && start === null) direction = 'to-black';
      else if (operands[5] === 0 && start === 255) direction = 'from-black';
    } else if (operands[1] === 255 && operands[2] === 255 && operands[3] === 255) {
      if (operands[5] === 255 && start === null) direction = 'to-white';
      else if (operands[5] === 0 && start === 255) direction = 'from-white';
    }
    return { direction: direction, red: operands[1], green: operands[2], blue: operands[3],
      startAlpha: start, targetAlpha: operands[5], nativeOpcode: '0x1B',
      timingStatus: 'native fade update count' };
  }

  function compileFade(clip) {
    var payload = clip.payload || {};
    var start = payload.startAlpha == null ? -1
      : integer(payload.startAlpha, 'Fade starting alpha', 0, 255);
    return [0x1B, integer(clip.durationFrames, 'Fade duration', 1, 0xFFFF),
      integer(payload.red, 'Fade red', 0, 255),
      integer(payload.green, 'Fade green', 0, 255),
      integer(payload.blue, 'Fade blue', 0, 255),
      start, integer(payload.targetAlpha, 'Fade target alpha', 0, 255)];
  }

  // A dialogue window that needs no template in the scene: fixed placement
  // (mode 1) at X 7, Y 160, width 192, pointer edge 1, transparent portrait
  // (class 0), on presentation archive selector 5 (retail archive 163).
  // Verified in native-mode preview by Astra (messages 282 and 305).
  // Words 1..4 and 11 are filled per line.
  var DIALOGUE_WINDOW_RECIPES = Object.freeze({
    'fixed-placement': Object.freeze([0xBF, 0, 5, 0, 0, 7, 160, 192, 1, 0, 1, 0, 0, 1])
  });

  function dialogueWindowTemplate(payload, ir) {
    if (payload.nativeWindow != null) {
      var recipe = DIALOGUE_WINDOW_RECIPES[payload.nativeWindow];
      if (!recipe) fail('New dialogue names an unknown window recipe.', 'dialogue-template');
      return { rawWords: recipe.slice() };
    }
    var template = ir.nodes.find(function(node) {
      return node.id === payload.nativeTemplateNodeId;
    });
    if (!template || template.rawWords.length !== 14 ||
        unsigned(template.rawWords[0]) !== 0xBF) {
      fail('New dialogue lost its native window template.', 'dialogue-template');
    }
    return template;
  }

  function compileAuthoredDialogue(clip, ir, waitUnit) {
    var payload = clip.payload || {};
    if (payload.nativeDialogueAuthored !== true ||
        payload.sourceSystem !== 'serifu-authored-native') {
      fail('This dialogue clip has no native Director command adapter.', 'dialogue-source');
    }
    var template = dialogueWindowTemplate(payload, ir);
    var windowId = integer(payload.windowId, 'Dialogue window ID', 0, 255);
    var selector = integer(payload.presentationArchiveSelector,
      'Dialogue archive selector', 0, 502);
    var entry = integer(payload.presentationEntrySelector,
      'Dialogue entry selector', 0, 65535);
    var delay = integer(payload.nativeDelayTicks,
      'Dialogue start delay', 0, 0x7FFFFFFF);
    if (selector !== unsigned(template.rawWords[2])) {
      fail('New dialogue must use its source window template archive.', 'dialogue-archive');
    }
    var words = template.rawWords.slice();
    words[1] = windowId;
    words[2] = selector;
    words[3] = entry;
    // Bind the box to its speaking Actor. Operand 10 is a class ID into the
    // portrait route table; -1 resolves the owner Actor's own portrait and 0
    // (class None) draws a transparent portrait area.
    if (payload.ownerSlot != null) {
      words[4] = integer(payload.ownerSlot, 'Dialogue owner Actor slot', 0, 27);
    }
    if (payload.portrait === 'actor') words[11] = 0xFFFFFFFF;
    else if (payload.portrait === 'none') words[11] = 0;
    else if (payload.portrait != null) {
      words[11] = integer(payload.portrait, 'Dialogue portrait class', 0, 255);
    }
    var result = delay ? (waitUnit ? waitUnit(delay)
      : [1, 0, 0x80000002, 0x80000000, 0x0E, 0, delay, 2]) : [];
    return result.concat(words, [0, 0x80000002, 0x80000000, 0x10, 0, 2, windowId,
      0x06, windowId, 0x05, windowId]);
  }

  function compileActorByte(opcode, clip, slot, field, label, rawWords) {
    slot = integer(slot, 'Actor slot', 0, 255);
    var value = integer(clip.payload[field], label, 0, 255);
    var rawSlot = rawWords ? signed(rawWords[1]) : null;
    var rawValue = rawWords ? unsigned(rawWords[2]) & 0xFF : null;
    return [opcode,
      rawWords && slot === rawSlot ? signed(rawWords[1]) : slot,
      rawWords && value === rawValue ? signed(rawWords[2]) : value];
  }

  function compileRegisteredWait(clip, rawWords) {
    var ticks = integer(clip.durationFrames,
      'Registered-counter updates', 1, 0x7FFFFFFF);
    if (!rawWords || rawWords.length !== 3 || unsigned(rawWords[0]) !== 0x0E ||
        clip.payload.registeredCounterEditable !== true ||
        clip.payload.nativeTicks !== ticks ||
        clip.payload.registeredCounterTarget !== ticks) {
      fail('The fixed-wait editor lost its exact registered-counter query boundary.',
        'registered-wait-boundary');
    }
    var output = rawWords.slice();
    output[2] = ticks;
    return output;
  }

  function compileBackgroundGroup(background, rawWords) {
    var projection = background && background.projection || {};
    // Class 4/5 loaders index the 31-row group table without a bounds check,
    // so a selector outside 0..30 (including -1) must never be written.
    var selector = projection.route === 'native-environment'
      ? integer(projection.selectedSelector, 'Scene environment', 0, 79)
      : integer(projection.selectedSelector, 'Scene background group', 0, 30);
    return [unsigned(rawWords[0]), selector];
  }

  function compileAudioBlockRequest(clip, rawWords) {
    var requestValue = integer(clip.payload.nativeRequestValue,
      'Sequenced audio request', 0, 65);
    var blockIndex = nativeAudioBlockIndex(requestValue);
    if (blockIndex === null || clip.payload.audioBlockId !==
        'sequenced-audio:' + String(blockIndex).padStart(2, '0')) {
      fail('The selected audio block has no matching non-empty native request row.',
        'audio-block-request');
    }
    return [unsigned(rawWords[0]), signed(rawWords[1]), signed(rawWords[2]), requestValue];
  }

  function authoredSequence(clip) {
    var value = clip.source && clip.source.sequence;
    return Number.isInteger(value) ? value : Number.MAX_SAFE_INTEGER;
  }

  function allClips(document) {
    var clips = [];
    document.tracks.forEach(function(track) {
      track.clips.forEach(function(clip) {
        clips.push({ track: track, clip: clip });
      });
    });
    return clips;
  }

  function compileSceneDocument(scene, source, document) {
    var M = OB64.cutsceneModel;
    M.validateSceneDocument(document);
    if (document.identity.sourceRevision !== 'us-rev0' || document.identity.engine !== 'director' ||
        document.native.sourceAssetId !== scene.assetId) {
      fail('Only the matching US Rev 0 Director source can use native export.',
        'unsupported-revision');
    }
    var ir = createIr(scene, source.decodedBytes || source, source.nodeDefinitions);
    var counterTiming = registeredCounterTiming(ir);
    var counterSpans = counterSpanContext(ir);
    var spanShift = {};
    function waitUnitBefore(nodeId) {
      var span = counterSpans[nodeId];
      return function(ticks) {
        ticks = integer(ticks, 'Hold duration', 1, 0x7FFFFFFF);
        if (!span || !span.armed) return [1, 0, 0x80000002, 0x80000000, 0x0E, 0, ticks, 2];
        var shifted = (spanShift[span.spanId] || 0) + ticks;
        spanShift[span.spanId] = shifted;
        return [0, 0x80000002, 0x80000000, 0x0E, span.compareOp, span.lastTarget + shifted];
      };
    }
    var nativeByNode = {};
    document.native.commands.forEach(function(command) {
      if (command.source && command.source.nodeId) nativeByNode[command.source.nodeId] = command;
    });
    var clipRows = allClips(document);
    var clipsByNode = {};
    clipRows.forEach(function(row) {
      var nodeId = row.clip.source && row.clip.source.nodeId;
      if (!nodeId) return;
      if (!clipsByNode[nodeId]) clipsByNode[nodeId] = [];
      clipsByNode[nodeId].push(row);
    });
    var actorBySlot = {};
    document.actors.forEach(function(actor) {
      actorBySlot[actor.slot] = actor;
    });
    var changes = [];
    var outputWords = [];
    // Output word index of every retained native node, so callers can find a
    // card's command in the reparsed stream and its runtime tick.
    var nodeStartWords = {};

    function appendAuthoredBefore(nodeId) {
      document.actors.filter(function(actor) {
        return actor.source && actor.source.authored === true &&
          actor.source.insertBeforeNodeId === nodeId;
      }).sort(function(left, right) {
        return left.slot - right.slot || left.id.localeCompare(right.id);
      }).forEach(function(actor) {
        var words = compilePlace(actor, null);
        changes.push({ operation: 'insert', actorId: actor.id, beforeNodeId: nodeId,
          kind: 'place', startWord: outputWords.length });
        outputWords.push.apply(outputWords, words);
        // Per-slot presentation that the scene applies to its own cast
        // (0x1C transform channel, 0x1D uniform scale), for Actors added later.
        var presentation = actor.source.presentation;
        if (presentation && Number.isInteger(presentation.transformChannel)) {
          outputWords.push(0x1C, integer(actor.slot, 'Actor slot', 0, 27),
            integer(presentation.transformChannel, 'Transform channel', 0, 19));
        }
        if (presentation && Number.isInteger(presentation.uniformScalePercent)) {
          outputWords.push(0x1D, integer(actor.slot, 'Actor slot', 0, 27),
            integer(presentation.uniformScalePercent, 'Actor scale', 1, 400));
        }
      });
      var orderedRows = clipRows.filter(function(row) {
        return !row.clip.source.nodeId && row.clip.source.insertBeforeNodeId === nodeId;
      }).sort(function(left, right) {
        // Start order, then the explicit sequence assigned by card reordering,
        // then the id so unsequenced documents keep their historical order.
        return left.clip.startFrame - right.clip.startFrame ||
          authoredSequence(left.clip) - authoredSequence(right.clip) ||
          left.clip.id.localeCompare(right.clip.id);
      });
      // Cards tagged to a choice branch are compiled inside that choice.
      var branchOf = {};
      orderedRows.forEach(function(row) {
        var tag = row.clip.payload && row.clip.payload.choiceBranch;
        if (!tag) return;
        var owner = orderedRows.find(function(candidate) { return candidate.clip.id === tag.choiceClipId; });
        if (!owner || !(owner.clip.payload && owner.clip.payload.choice) || orderedRows.indexOf(owner) > orderedRows.indexOf(row) ||
            (tag.option !== 1 && tag.option !== 2)) {
          fail('A choice branch action must follow its choice at the same place.', 'choice-branch', { clipId: row.clip.id });
        }
        branchOf[row.clip.id] = tag;
      });
      orderedRows.forEach(function(row) {
        if (branchOf[row.clip.id]) return;
        if (row.clip.payload && row.clip.payload.choice) {
          emitChoice(row, orderedRows.filter(function(candidate) {
            return branchOf[candidate.clip.id] && branchOf[candidate.clip.id].choiceClipId === row.clip.id;
          }));
          return;
        }
        changes.push({ operation: 'insert', clipId: row.clip.id, beforeNodeId: nodeId,
          kind: row.clip.kind, startWord: outputWords.length });
        outputWords.push.apply(outputWords, rowWords(row));
      });

      // A native A/B choice (Astra messages 293 and 305): the window, then two
      // control-byte queries that close it and jump to a labeled branch, each
      // branch waiting for the window's release, and one join label.
      function emitChoice(row, branchRows) {
        var windowId = integer(row.clip.payload.windowId, 'Dialogue window ID', 0, 255);
        var open = compileAuthoredDialogue(row.clip, ir, waitUnitBefore(nodeId));
        open = open.slice(0, open.length - 11);
        var labelA = nextLabel(), labelB = nextLabel(), labelJoin = nextLabel();
        changes.push({ operation: 'insert', clipId: row.clip.id, beforeNodeId: nodeId,
          kind: row.clip.kind, startWord: outputWords.length });
        outputWords.push.apply(outputWords, open.concat([0, 0x80000002, 0x80000000, 0x56,
          0x25, 0, 1, 3, 0x05, windowId, 0x59, labelA, 0x80000002,
          0x25, 0, 255, 3, 0x05, windowId, 0x59, labelB, 0x80000002, 0x80000000]));
        [[1, labelA], [2, labelB]].forEach(function(branch) {
          outputWords.push(0x80000005, branch[1], 0x10, 0, 1, windowId);
          branchRows.filter(function(candidate) { return branchOf[candidate.clip.id].option === branch[0]; }).forEach(function(inner) {
            changes.push({ operation: 'insert', clipId: inner.clip.id, beforeNodeId: nodeId,
              kind: inner.clip.kind, startWord: outputWords.length, choiceBranch: branch[0] });
            outputWords.push.apply(outputWords, rowWords(inner));
          });
          outputWords.push(0x59, labelJoin);
        });
        outputWords.push(0x80000005, labelJoin);
      }

      function rowWords(row) {
        var words;
        var authoredActor = row.track.actorId == null ? null :
          document.actors.find(function(actor) { return actor.id === row.track.actorId; });
        var authoredSlot = authoredActor ? authoredActor.slot : null;
        var waitUnit = waitUnitBefore(nodeId);
        if (row.clip.kind === 'pose') words = compileState(row.clip, authoredSlot);
        else if (row.clip.kind === 'movement') words = compileMove(row.clip, authoredSlot);
        else if (row.clip.kind === 'wait') words = waitUnit(row.clip.durationFrames);
        else if (row.clip.kind === 'dialogue') words = compileAuthoredDialogue(row.clip, ir, waitUnit);
        else if (row.clip.kind === 'camera') words = compileCamera(row.clip, null);
        else if (row.clip.kind === 'fade') words = compileFade(row.clip);
        else if (row.clip.kind === 'effect' &&
            row.clip.payload.sourceSystem === 'cutscene-sprite-native') {
          words = compileAuthoredNativeSpriteEffect(row.clip, waitUnit);
        }
        else if (row.clip.kind === 'opacity') words = compileActorByte(
          0x48, row.clip, authoredSlot, 'opacityByte', 'Actor opacity', null);
        else if (row.clip.kind === 'enter') words = compileEnter(row.clip, authoredActor);
        else if (row.clip.kind === 'exit') words = [0x13,
          integer(authoredSlot, 'Actor slot', 0, 255)];
        else fail('This authored clip has no native Director command adapter.', 'unsupported-clip');
        // Move, Camera pan and Fade start and return at once. With "wait until
        // it finishes" the next action starts only after their duration.
        if (row.clip.payload && row.clip.payload.waitForCompletion === true &&
            ASYNC_CARD_KINDS.indexOf(row.clip.kind) >= 0) {
          if (row.clip.kind === 'movement' && Number.isInteger(authoredSlot)) {
            // The retail form (1,037 uses): marker, barrier, bridge, then the
            // 0x11 movement query until this slot's motion countdown is zero.
            // It waits for arrival however far the Actor actually walks.
            words = words.concat([0, 0x80000002, 0x80000000, 0x11, 0, 0, authoredSlot]);
          } else {
            words = words.concat(waitUnit(row.clip.durationFrames + completionExtraUpdates(row.clip.kind)));
          }
        }
        return words;
      }
    }

    // Choice labels are unique in the stream: start at 0x7000 and skip every
    // label ID the source already uses.
    var usedLabels = {};
    ir.nodes.forEach(function(node) {
      if (unsigned(node.rawWords[0]) === 0x80000005 && node.rawWords.length > 1) usedLabels[unsigned(node.rawWords[1])] = true;
    });
    var labelCursor = 0x7000;
    function nextLabel() {
      while (usedLabels[labelCursor]) labelCursor++;
      usedLabels[labelCursor] = true;
      return labelCursor++;
    }

    ir.nodes.forEach(function(node) {
      appendAuthoredBefore(node.id);
      nodeStartWords[node.id] = outputWords.length;
      var act = decodeNode(node);
      var command = nativeByNode[node.id];
      if (!command) fail('SceneDocument lost a preserved native source boundary.', 'native-boundary');
      var rows = clipsByNode[node.id] || [];
      var counterRole = counterTiming[node.id] || null;
      var compiled = node.rawWords.slice();
      var deleted = false;
      if (counterRole && counterRole.kind === 'gate' && counterRole.editable === true) {
        var fixedWait = rows.find(function(row) { return row.clip.kind === 'wait'; });
        if (!fixedWait || fixedWait.clip.capability !== M.capabilities.NATIVE ||
            fixedWait.clip.payload.compositeId !== counterRole.compositeId) {
          fail('A fixed registered-counter wait must keep its complete source bundle.',
            'registered-wait-boundary', { nodeId: node.id });
        }
        if (fixedWait.clip.startFrame !== fixedWait.clip.source.originalStartFrame) {
          fail('A fixed registered-counter wait cannot leave its native query boundary.',
            'compile-timing', { nodeId: node.id });
        }
        if (fixedWait.clip.durationFrames !== fixedWait.clip.source.originalDurationFrames ||
            fixedWait.clip.payload.nativeTicks !== counterRole.target ||
            fixedWait.clip.payload.registeredCounterTarget !== counterRole.target) {
          compiled = compileRegisteredWait(fixedWait.clip, node.rawWords);
        }
      } else if (act && act.type === 'background-group') {
        var backgroundProjection = document.background.projection || {};
        var ownsBackground = node.definition.editPolicy === 'typed-background-group' ||
          (backgroundProjection.route === 'native-environment' || backgroundProjection.route === 'native-scene-group') &&
          backgroundProjection.sourceNodeId === node.id;
        if (ownsBackground) {
          var routeTableId = backgroundProjection.route === 'native-environment'
            ? ENVIRONMENT_TABLE_ID : act.selectorTableId;
          if (backgroundProjection.nativeEditable !== true ||
              backgroundProjection.sourceNodeId !== node.id ||
              backgroundProjection.selectorTableId !== routeTableId) {
            fail('The native background route lost its source ownership.',
              'background-source-boundary', { nodeId: node.id });
          }
          if (backgroundProjection.selectedSelector !== act.selector) {
            compiled = compileBackgroundGroup(document.background, node.rawWords);
          }
        }
      } else if (act && act.type === 'place') {
        var actor = actorBySlot[act.slot];
        if (!actor || actor.source && actor.source.authored === true) {
          deleted = true;
        } else if (actor.source.placeNodeId === node.id && !actorMatchesPlace(actor, act)) {
          compiled = compilePlace(actor, node.rawWords);
        }
      } else if (act && act.type === 'state') {
        var pose = rows.find(function(row) { return row.clip.kind === 'pose'; });
        if (!pose) {
          if (node.definition.editPolicy === 'typed-state') deleted = true;
          else if (!actorBySlot[act.slot]) compiled = node.rawWords.slice();
          else fail('This display-only pose command must remain byte-preserved.', 'immutable-node', {
            nodeId: node.id
          });
        } else if (pose.clip.payload.nativeApplied !== true) {
          if (pose.clip.startFrame !== pose.clip.source.originalStartFrame ||
              pose.clip.durationFrames !== pose.clip.source.originalDurationFrames) {
            fail('An unapplied native actor-state command must remain byte-preserved.',
              'immutable-node', { nodeId: node.id });
          }
        } else {
          var poseActor = pose.track.actorId == null ? null : document.actors.find(function(actor) {
            return actor.id === pose.track.actorId;
          });
          var poseSlot = poseActor ? poseActor.slot : act.slot;
          if (pose.clip.startFrame !== pose.clip.source.originalStartFrame) {
            fail('Moving a source-backed pose needs a reviewed native wait boundary.', 'compile-timing');
          }
          if (!clipMatchesState(pose.clip, poseSlot, act)) {
            if (node.definition.editPolicy !== 'typed-state') {
              fail('This pose is visible for preview but its source boundary is read-only.',
                'immutable-node', { nodeId: node.id });
            }
            compiled = compileState(pose.clip, poseSlot);
          }
        }
      } else if (act && act.type === 'native-effect' && node.definition.editPolicy === 'typed-fade') {
        var nativeFade = rows.find(function(row) { return row.clip.kind === 'fade'; });
        if (!nativeFade) deleted = true;
        else {
          if (nativeFade.clip.startFrame !== nativeFade.clip.source.originalStartFrame) {
            fail('A native fade keeps its place; add a new Fade to fade elsewhere.', 'compile-timing');
          }
          var fadeWords = compileFade(nativeFade.clip);
          if (fadeWords.some(function(value, index) { return (value >>> 0) !== (node.rawWords[index] >>> 0); })) {
            compiled = fadeWords;
          }
        }
      } else if (act && act.type === 'move') {
        var movement = rows.find(function(row) { return row.clip.kind === 'movement'; });
        if (!movement) {
          if (node.definition.editPolicy === 'typed-move') deleted = true;
          else if (!actorBySlot[act.slot]) compiled = node.rawWords.slice();
          else fail('This display-only movement command must remain byte-preserved.', 'immutable-node', {
            nodeId: node.id
          });
        } else {
          var movementActor = movement.track.actorId == null ? null :
            document.actors.find(function(actor) { return actor.id === movement.track.actorId; });
          var movementSlot = movementActor ? movementActor.slot : act.slot;
          if (movement.clip.startFrame !== movement.clip.source.originalStartFrame) {
            fail('Moving a source-backed movement needs a reviewed native wait boundary.', 'compile-timing');
          }
          if (!clipMatchesMove(movement.clip, movementSlot, act)) {
            if (node.definition.editPolicy !== 'typed-move') {
              fail('This movement is visible for preview but its source boundary is read-only.',
                'immutable-node', { nodeId: node.id });
            }
            compiled = compileMove(movement.clip, movementSlot);
          }
        }
      } else if (act && act.type === 'projection-transform') {
        var camera = rows.find(function(row) {
          return row.clip.kind === 'camera' &&
            row.clip.payload.presentationKind === 'projection-transform';
        });
        if (!camera) {
          if (node.definition.editPolicy === 'typed-projection-transform') deleted = true;
          else fail('This display-only projection command must remain byte-preserved.',
            'immutable-node', { nodeId: node.id });
        } else {
          if (camera.clip.startFrame !== camera.clip.source.originalStartFrame) {
            fail('A source-backed projection transition cannot leave its native command boundary.',
              'compile-timing');
          }
          if (!clipMatchesCamera(camera.clip, act)) {
            if (node.definition.editPolicy !== 'typed-projection-transform') {
              fail('This projection transition is visible but its source boundary is read-only.',
                'immutable-node', { nodeId: node.id });
            }
            compiled = compileCamera(camera.clip, node.rawWords);
          }
        }
      } else if (act && act.type === 'native-sprite-effect') {
        var nativeEffect = rows.find(function(row) {
          return row.clip.kind === 'effect' &&
            row.clip.payload.sourceSystem === 'cutscene-sprite-native';
        });
        if (!nativeEffect) {
          if (node.definition.editPolicy === 'typed-native-sprite-effect') deleted = true;
          else fail('This display-only sprite effect must remain byte-preserved.',
            'immutable-node', { nodeId: node.id });
        } else {
          if (nativeEffect.clip.startFrame !== nativeEffect.clip.source.originalStartFrame) {
            fail('A source-backed sprite effect cannot leave its native command boundary.',
              'compile-timing');
          }
          if (!clipMatchesNativeSpriteEffect(nativeEffect.clip, act)) {
            if (node.definition.editPolicy !== 'typed-native-sprite-effect') {
              fail('This sprite effect is visible for preview but its source boundary is read-only.',
                'immutable-node', { nodeId: node.id });
            }
            compiled = compileNativeSpriteEffect(nativeEffect.clip, node.rawWords);
          }
        }
      } else if (act && act.type === 'audio-controller' &&
          node.definition.editPolicy === 'typed-audio-block-request') {
        var audio = rows.find(function(row) { return row.clip.kind === 'audio'; });
        if (!audio) {
          fail('A native audio request can be substituted but not removed without stop semantics.',
            'audio-block-removal', { nodeId: node.id });
        }
        if (audio.clip.startFrame !== audio.clip.source.originalStartFrame ||
            audio.clip.durationFrames !== audio.clip.source.originalDurationFrames) {
          fail('A source audio request must remain at its native command boundary.',
            'source-timing', { nodeId: node.id });
        }
        if (!clipMatchesAudioBlockRequest(audio.clip, act)) {
          compiled = compileAudioBlockRequest(audio.clip, node.rawWords);
        }
      } else if (act && act.type === 'actor-opacity') {
        var actorFieldKind = 'opacity';
        var actorFieldPolicy = 'typed-actor-opacity';
        var actorFieldName = 'opacityByte';
        var actorFieldLabel = 'Actor opacity';
        if (act.slot === -1) {
          var sourceActors = document.actors.filter(function(actor) {
            return !(actor.source && actor.source.authored === true);
          });
          var allActorsPreserved = rows.length === sourceActors.length && rows.every(function(row) {
            return row.clip.kind === actorFieldKind &&
              row.clip.payload.nativeSlotSelector === -1 &&
              row.clip.payload[actorFieldName] ===
                (actorFieldName === 'selector' ? act.selector : act.opacityByte) &&
              sourceActors.some(function(actor) { return actor.id === row.track.actorId; });
          });
          if (!allActorsPreserved) {
            fail('A scene-wide actor presentation command must remain identical for every source actor.',
              'immutable-node', { nodeId: node.id });
          }
          outputWords.push.apply(outputWords, compiled);
          return;
        }
        var actorField = rows.find(function(row) { return row.clip.kind === actorFieldKind; });
        if (!actorField) {
          if (node.definition.editPolicy === actorFieldPolicy && act.slot !== -1) deleted = true;
          else fail('This scene-wide actor presentation command must remain byte-preserved.',
            'immutable-node', { nodeId: node.id });
        } else {
          var fieldActor = actorField.track.actorId == null ? null :
            document.actors.find(function(actor) { return actor.id === actorField.track.actorId; });
          var fieldSlot = fieldActor ? fieldActor.slot : act.slot;
          if (actorField.clip.startFrame !== actorField.clip.source.originalStartFrame) {
            fail('A source-backed actor presentation step cannot leave its native boundary.',
              'compile-timing');
          }
          if (!clipMatchesActorByte(actorField.clip, fieldSlot, act, actorFieldName)) {
            if (node.definition.editPolicy !== actorFieldPolicy || act.slot === -1) {
              fail('This actor presentation step is visible for preview but remains read-only.',
                'immutable-node', { nodeId: node.id });
            }
            compiled = compileActorByte(0x48, actorField.clip, fieldSlot,
              actorFieldName, actorFieldLabel, node.rawWords);
          }
        }
      }
      if (deleted) {
        changes.push({ operation: 'delete', nodeId: node.id, kind: act.type });
        return;
      }
      var span = counterSpans[node.id];
      if (span && span.armed && spanShift[span.spanId] &&
          COUNTER_QUERIES.indexOf(node.definition.name) !== -1 && compiled.length >= 3) {
        compiled = compiled.slice();
        compiled[2] = integer(signed(compiled[2]) + spanShift[span.spanId],
          'Shifted counter target', 0, 0x7FFFFFFF);
        changes.push({ operation: 'retime', nodeId: node.id, shift: spanShift[span.spanId] });
      }
      outputWords.push.apply(outputWords, compiled);
      if (compiled.length !== node.rawWords.length || compiled.some(function(word, index) {
        return unsigned(word) !== unsigned(node.rawWords[index]);
      })) {
        changes.push({ operation: 'edit', nodeId: node.id, kind: act && act.type || node.definition.name });
      }
    });
    var decodedBytes = wordsToBytes(outputWords);
    return {
      decodedBytes: decodedBytes,
      originalDecodedBytes: ir.originalDecodedBytes,
      noOp: equalBytes(decodedBytes, ir.originalDecodedBytes),
      changes: changes,
      nodeStartWords: nodeStartWords
    };
  }

  function planFixedCapacityExport(z64Input, scene, document, options) {
    var z64 = bytes(z64Input, 'Normalized ROM');
    options = options || {};
    return (options.preloadedSource ? Promise.resolve(options.preloadedSource) :
      loadSceneSource(z64, scene, options)).then(function(source) {
      var compiled = compileSceneDocument(scene, source, document);
      // A replacement source (a wiped scene) differs from the ROM even when
      // its document has no further edits, so it is always written.
      if (options.forceWrite && compiled.noOp) compiled = Object.assign({}, compiled, { noOp: false });
      var sourceInfo = scene.source;
      if (sourceInfo.crcWindowOverlap) {
        fail('This Cutscene payload overlaps the cartridge checksum window.', 'crc-window');
      }
      var encoded = compiled.noOp ? source.payload :
        (options.encoder || encodeCustomLzOptimal)(compiled.decodedBytes);
      if (!(encoded instanceof Uint8Array)) fail('Cutscene encoder returned invalid bytes.', 'custom-lz-encode');
      if (encoded.length > sourceInfo.storedPayloadLength) {
        fail('Cutscene needs ' + encoded.length + ' compressed bytes but this scene has ' +
          sourceInfo.storedPayloadLength + '. Shorten the scene or use relocation when available.',
          'capacity-overflow', { required: encoded.length, capacity: sourceInfo.storedPayloadLength });
      }
      var slotStart = sourceInfo.z64PayloadStart;
      var slotEnd = slotStart + sourceInfo.dmaExtent;
      if (slotEnd > z64.length) fail('Cutscene DMA slot lies outside the ROM.', 'source-range');
      var candidate = z64.slice();
      if (!compiled.noOp) {
        candidate.fill(0, slotStart, slotEnd);
        candidate.set(encoded, slotStart);
      }
      if (readU32(candidate, sourceInfo.z64PrefixStart) !== sourceInfo.storedPayloadLength) {
        fail('Cutscene outer capacity prefix changed.', 'diff-containment');
      }
      var readbackPayload = candidate.slice(slotStart, slotStart + sourceInfo.storedPayloadLength);
      var readback = decodeCustomLz(readbackPayload, { requireExact: false, allowZeroPadding: true });
      if (!equalBytes(readback.bytes, compiled.decodedBytes)) {
        fail('Cutscene candidate readback differs from the intended scene.', 'readback');
      }
      return Promise.all([
        hashWith(compiled.originalDecodedBytes, options.hashBytes),
        hashWith(compiled.decodedBytes, options.hashBytes)
      ]).then(function(hashes) {
        return {
          sceneId: scene.sceneId,
          assetId: scene.assetId,
          candidateZ64: candidate,
          noOp: compiled.noOp,
          changes: compiled.changes,
          originalDecodedSha256: hashes[0],
          intendedDecodedSha256: hashes[1],
          intendedDecodedBytes: compiled.decodedBytes.slice(),
          originalEncodedBytes: source.consumedEncodedBytes,
          encodedBytes: encoded.length,
          capacityBytes: sourceInfo.storedPayloadLength,
          remainingBytes: sourceInfo.storedPayloadLength - encoded.length,
          originalSlotBytes: z64.slice(slotStart, slotEnd),
          readback: {
            consumedEncodedBytes: readback.consumed,
            zeroPaddingOnly: !compiled.noOp && readback.consumed < sourceInfo.storedPayloadLength,
            intendedBytesMatch: true
          },
          changedRange: compiled.noOp ? null : { start: slotStart, endExclusive: slotEnd }
        };
      });
    });
  }

  function planRelocatedExport(z64Input, scene, document, allocation, options) {
    var z64 = bytes(z64Input, 'Normalized ROM');
    options = options || {};
    if (!allocation || !Number.isInteger(allocation.entry) ||
        !Number.isInteger(allocation.endExclusive) ||
        !Number.isInteger(allocation.key) || allocation.entry < 0 ||
        allocation.endExclusive <= allocation.entry + 4 ||
        allocation.endExclusive > z64.length) {
      return Promise.reject(new CutsceneCodecError(
        'Cutscene relocation allocation is invalid.', 'relocation-allocation'));
    }
    // Arena compaction plans on a detached base whose old arena bytes are
    // cleared, so an owned relocation supplies the source it already verified.
    return (options.preloadedSource ? Promise.resolve(options.preloadedSource) :
      loadSceneSource(z64, scene, options)).then(function(source) {
      var compiled = compileSceneDocument(scene, source, document);
      if (options.forceWrite && compiled.noOp) compiled = Object.assign({}, compiled, { noOp: false });
      var encoded = compiled.noOp ? source.payload :
        (options.encoder || encodeCustomLzOptimal)(compiled.decodedBytes);
      if (!(encoded instanceof Uint8Array)) {
        fail('Cutscene encoder returned invalid bytes.', 'custom-lz-encode');
      }
      // A resource that already lives in the editor arena is rebuilt at its
      // compacted position even when it now fits the size it had there.
      if (encoded.length <= scene.source.storedPayloadLength && !options.relocateOwned) {
        fail('Cutscene relocation is reserved for payloads that exceed their source slot.',
          'relocation-not-required');
      }
      if (allocation.entry + 4 + encoded.length > allocation.endExclusive) {
        fail('Cutscene relocation allocation is smaller than the encoded payload.',
          'relocation-capacity');
      }
      for (var fill = allocation.entry; fill < allocation.endExclusive; fill++) {
        if (z64[fill] !== 0xFF) {
          fail('Cutscene relocation storage is not retail 0xFF fill.',
            'relocation-preimage', { z64Offset: fill });
        }
      }
      var selectorWords = scene.source.directorSelectorWordZ64 || [];
      var originalKey = directorKey(scene.directorKey);
      if (!selectorWords.length) {
        fail('Cutscene relocation has no exact Director selector owner.',
          'relocation-selector-owner');
      }
      selectorWords.forEach(function(offset) {
        if (readU32(z64, offset) !== originalKey) {
          fail('A Director selector owner no longer points to the retail resource.',
            'relocation-selector-preimage', { z64Offset: offset });
        }
      });
      var candidate = z64.slice();
      candidate.fill(0xFF, allocation.entry, allocation.endExclusive);
      writeU32(candidate, allocation.entry, encoded.length);
      candidate.set(encoded, allocation.entry + 4);
      selectorWords.forEach(function(offset) {
        writeU32(candidate, offset, allocation.key);
      });
      var readbackLength = readU32(candidate, allocation.entry);
      var readback = decodeCustomLz(candidate.slice(
        allocation.entry + 4, allocation.entry + 4 + readbackLength), {
        requireExact: true
      });
      if (!equalBytes(readback.bytes, compiled.decodedBytes)) {
        fail('Relocated Cutscene readback differs from the intended scene.',
          'relocation-readback');
      }
      return Promise.all([
        hashWith(compiled.originalDecodedBytes, options.hashBytes),
        hashWith(compiled.decodedBytes, options.hashBytes)
      ]).then(function(hashes) {
        var writes = [{
          start: allocation.entry,
          endExclusive: allocation.endExclusive,
          originalBytes: z64.slice(allocation.entry, allocation.endExclusive),
          patchedBytes: candidate.slice(allocation.entry, allocation.endExclusive),
          label: 'relocated Director resource envelope'
        }];
        selectorWords.forEach(function(offset, index) {
          writes.push({
            start: offset,
            endExclusive: offset + 4,
            originalBytes: z64.slice(offset, offset + 4),
            patchedBytes: candidate.slice(offset, offset + 4),
            label: 'Director selector owner row ' +
              scene.source.directorSelectorRows[index]
          });
        });
        return {
          sceneId: scene.sceneId,
          assetId: scene.assetId,
          candidateZ64: candidate,
          noOp: compiled.noOp,
          placement: 'relocated',
          resourceKey: allocation.key,
          relocationEntry: allocation.entry,
          relocationEndExclusive: allocation.endExclusive,
          selectorRows: scene.source.directorSelectorRows.slice(),
          selectorWordZ64: selectorWords.slice(),
          writes: writes,
          changedRanges: writes.map(function(write) {
            return { start: write.start, endExclusive: write.endExclusive };
          }),
          changes: compiled.changes,
          originalDecodedSha256: hashes[0],
          intendedDecodedSha256: hashes[1],
          intendedDecodedBytes: compiled.decodedBytes.slice(),
          originalEncodedBytes: source.consumedEncodedBytes,
          encodedBytes: encoded.length,
          capacityBytes: allocation.endExclusive - allocation.entry - 4,
          remainingBytes: allocation.endExclusive - allocation.entry - 4 - encoded.length,
          readback: {
            consumedEncodedBytes: readback.consumed,
            zeroPaddingOnly: false,
            intendedBytesMatch: true
          },
          changedRange: {
            start: allocation.entry,
            endExclusive: allocation.endExclusive
          }
        };
      });
    });
  }

  OB64.cutsceneCodec = {
    CutsceneCodecError: CutsceneCodecError,
    decodeCustomLz: decodeCustomLz,
    encodeCustomLzOptimal: encodeCustomLzOptimal,
    wordsFromBytes: wordsFromBytes,
    wordsToBytes: wordsToBytes,
    equalBytes: equalBytes,
    loadSceneSource: loadSceneSource,
    createIr: createIr,
    sceneForBytes: sceneForBytes,
    authoredSceneSource: authoredSceneSource,
    completionExtraUpdates: completionExtraUpdates,
    dialogueWindowRecipes: DIALOGUE_WINDOW_RECIPES,
    hashBytes: function(input, hashBytes) { return hashWith(input, hashBytes); },
    decodeNode: decodeNode,
    backgroundGroupLayers: backgroundGroupLayers,
    projectSceneDocument: projectSceneDocument,
    compileSceneDocument: compileSceneDocument,
    planFixedCapacityExport: planFixedCapacityExport,
    planRelocatedExport: planRelocatedExport,
    sceneEntry: sceneEntry
  };
})(window.OB64);
