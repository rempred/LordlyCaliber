// Lordly Caliber - editor-authored Cutscene scenes with their own identity.
//
// An authored scene is a new Director stream the editor owns. It has a stable
// random identity, one explicit top-level selector row that is zero in retail
// and unclaimed, and a stream that starts from a validated native scaffold.
// It never claims to be a retail resource: its catalog entry is marked
// authored and it is exported as a kind-1 arena record (cutscene-export.js).

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var DIRECTOR_TABLE_Z64 = 0x01F3CA88;
  var DIRECTOR_TABLE_ROWS = 1693;
  // Traveling Cutscene 3 is the shortest retail class-5 stream with one scene
  // group and both cameras. Its first 18 words are the setup; its last five
  // words are the native Director exit (0x7D release), barrier, bridge,
  // terminal hold and class-5 trailer.
  var CLASS5_TEMPLATE = 'rom-custom-lz:01FA4FAE';
  var CLASS5_SETUP_WORDS = 18;
  var CLASS5_EXIT_TAIL = [0x7D, 0x80000002, 0x80000000, 0x80000001, 0xFF000005];
  // The scaffold and Actor recipe are frozen from the retail template, so new
  // and reopened authored scenes never depend on that scene's current, possibly
  // edited or wiped, ROM bytes. The retail source is named by its decoded
  // SHA-256; tests/cutscene-authored-scene.test.js re-derives both from it.
  var CLASS5_TEMPLATE_RETAIL_SHA256 = 'F1DACC67F5ED244B188C3915F4D4B4874DFB7AD772A10986B0D6CDCD19C7031B';
  var CLASS5_SCAFFOLD_WORDS = Object.freeze([
    0x80000006, 0x00000015,
    0x00000036, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0001D4C0, 0x00001388,
    0x00000035, 0xFFFFFB8F, 0x00000198, 0x00001770, 0x00004830, 0x00000CEF, 0x00001CCF, 0x00001388
  ].concat(CLASS5_EXIT_TAIL));
  var CLASS5_ACTOR_RECIPE = Object.freeze({
    bank: 30, animationKey: 442, nativeFacing: 2, x: -19, y: 0, z: 53,
    renderMode: 2, variantSelector: 4, transformChannel: 1, uniformScalePercent: 28,
    provenance: 'first Actor of the retail class-5 template and its all-Actor channel and scale commands'
  });

  function AuthoredSceneError(message, code) {
    this.name = 'AuthoredSceneError';
    this.message = message;
    this.code = code || 'authored-scene';
  }
  AuthoredSceneError.prototype = Object.create(Error.prototype);
  AuthoredSceneError.prototype.constructor = AuthoredSceneError;
  function fail(message, code) { throw new AuthoredSceneError(message, code); }

  function readU32(input, offset) {
    return ((input[offset] << 24) | (input[offset + 1] << 16) |
      (input[offset + 2] << 8) | input[offset + 3]) >>> 0;
  }

  function wordsOf(bytes) {
    if (!(bytes instanceof Uint8Array) || bytes.length % 4) fail('A Director stream must be whole words.');
    var words = [];
    for (var at = 0; at < bytes.length; at += 4) words.push(readU32(bytes, at));
    return words;
  }

  function bytesOf(words) {
    var output = new Uint8Array(words.length * 4);
    words.forEach(function(value, index) {
      output[index * 4] = value >>> 24; output[index * 4 + 1] = value >>> 16;
      output[index * 4 + 2] = value >>> 8; output[index * 4 + 3] = value;
    });
    return output;
  }

  // Validate the template shape instead of trusting its identity: one scene
  // group first, then the registered (0x36) and Actor (0x35) cameras with their
  // seven operands each, and the exact native exit tail at the end.
  function scaffoldFromTemplate(templateDecoded) {
    var words = wordsOf(templateDecoded);
    if (words.length < CLASS5_SETUP_WORDS + CLASS5_EXIT_TAIL.length ||
        words[0] !== 0x80000006 || words[1] >= 31 || words[2] !== 0x36 || words[10] !== 0x35) {
      fail('The class-5 template no longer opens with one scene group and both cameras.', 'authored-template');
    }
    var tail = words.slice(-CLASS5_EXIT_TAIL.length);
    if (tail.some(function(value, index) { return value !== CLASS5_EXIT_TAIL[index]; })) {
      fail('The class-5 template no longer ends with the native Director exit.', 'authored-template');
    }
    return bytesOf(words.slice(0, CLASS5_SETUP_WORDS).concat(CLASS5_EXIT_TAIL));
  }

  // The template's own cast shows how an Actor becomes visible in this scene
  // group: an 0x14 creation (facing, depth near z 50..56, render mode 2, an
  // appearance variant), then an 0x1C transform channel and an 0x1D uniform
  // scale that the template applies to every Actor. An added Actor copies the
  // first template Actor's creation fields and receives its own channel and
  // scale commands, so it is initialized even though it is created later.
  function actorRecipe(templateDecoded) {
    var words = wordsOf(templateDecoded);
    var create = -1, channel = null, scale = null;
    for (var at = CLASS5_SETUP_WORDS; at + 2 < words.length; ) {
      var opcode = words[at];
      if (opcode === 0x14 && create < 0) { create = at; at += 10; continue; }
      if (opcode === 0x1C && (words[at + 1] | 0) === -1 && channel === null) { channel = words[at + 2] | 0; at += 3; continue; }
      if (opcode === 0x1D && (words[at + 1] | 0) === -1 && scale === null) { scale = words[at + 2] | 0; at += 3; continue; }
      if (create >= 0 && channel !== null && scale !== null) break;
      at += 1;
    }
    if (create < 0 || channel === null || scale === null || channel < 0 || channel > 19 || scale < 1 || scale > 400) {
      fail('The class-5 template no longer shows how its Actors are made visible.', 'authored-template');
    }
    var w = words.slice(create, create + 10).map(function(value) { return value | 0; });
    return {
      bank: w[2], animationKey: w[3], nativeFacing: w[4],
      x: w[5] / 1000, y: w[6] / 1000, z: w[7] / 1000,
      renderMode: w[8] & 0xFF, variantSelector: w[9] & 0xFF,
      transformChannel: channel, uniformScalePercent: scale,
      provenance: 'first Actor of the class-5 template and its all-Actor channel and scale commands'
    };
  }

  // A stream accepted as an authored scene must keep the class-5 setup and end
  // with the native exit, whether it came from the scaffold or from the arena.
  function validateAuthoredStream(decoded) {
    var words = wordsOf(decoded);
    if (words.length < CLASS5_SETUP_WORDS + CLASS5_EXIT_TAIL.length || words.length > 16384 ||
        words[0] !== 0x80000006 || words[2] !== 0x36 || words[10] !== 0x35) {
      fail('An authored scene stream lost its class-5 setup.', 'authored-stream');
    }
    var tail = words.slice(-CLASS5_EXIT_TAIL.length);
    if (tail.some(function(value, index) { return value !== CLASS5_EXIT_TAIL[index]; })) {
      fail('An authored scene stream lost its native Director exit.', 'authored-stream');
    }
    return true;
  }

  function hexOf(bytes) {
    return Array.prototype.map.call(bytes, function(value) {
      return value.toString(16).padStart(2, '0');
    }).join('');
  }

  function randomId() {
    var cryptoSource = (typeof crypto !== 'undefined' && crypto.getRandomValues) ? crypto : null;
    if (!cryptoSource) fail('This browser cannot create a random scene identity.', 'authored-identity');
    var raw = new Uint8Array(16);
    cryptoSource.getRandomValues(raw);
    return Array.prototype.map.call(raw, function(value) {
      return value.toString(16).padStart(2, '0');
    }).join('');
  }

  function validId(authoredId) {
    return typeof authoredId === 'string' && /^[0-9a-f]{32}$/.test(authoredId);
  }

  // Rows the catalog assigns to retail scenes, plus rows already held by
  // authored scenes in this session or Project.
  function claimedRows(catalog, extraAuthored) {
    var claimed = {};
    catalog.directorScenes.forEach(function(scene) {
      (scene.source && scene.source.directorSelectorRows || []).forEach(function(row) { claimed[row] = scene.sceneId; });
    });
    (extraAuthored || []).forEach(function(entry) { claimed[entry.selectorRow] = 'scene:authored:' + entry.authoredId; });
    return claimed;
  }

  // A new scene may take a row only when it is zero in the loaded ROM and no
  // retail or authored scene claims it.
  function freeSelectorRows(z64, catalog, extraAuthored) {
    var claimed = claimedRows(catalog, extraAuthored), rows = [];
    for (var row = 0; row < DIRECTOR_TABLE_ROWS; row++) {
      if (!claimed[row] && readU32(z64, DIRECTOR_TABLE_Z64 + row * 4) === 0) rows.push(row);
    }
    return rows;
  }

  function templateScene(catalog, templateSceneId) {
    var template = catalog.getScene(templateSceneId || CLASS5_TEMPLATE);
    if (!template || template.engine !== 'director' || template.authored) {
      fail('The authored-scene template is not a retail Director scene.', 'authored-template');
    }
    return template;
  }

  // Build (but do not register) an authored scene. `decodedBytes` is supplied
  // when restoring a Project or reopening an exported ROM; otherwise the scene
  // starts from the frozen scaffold. The template's catalog entry (static
  // retail metadata) is the structural reference; its ROM bytes are never read.
  function create(z64, catalog, options) {
    options = options || {};
    return Promise.resolve().then(function() {
      var authoredId = options.authoredId || randomId();
      if (!validId(authoredId)) fail('An authored scene identity must be 32 lowercase hex digits.', 'authored-identity');
      if (catalog.getScene('scene:authored:' + authoredId)) fail('That authored scene identity already exists.', 'authored-identity');
      var name = typeof options.name === 'string' && options.name.trim() ? options.name.trim() : 'New scene';
      var row = options.selectorRow;
      if (!Number.isInteger(row) || row < 0 || row >= DIRECTOR_TABLE_ROWS) {
        fail('An authored scene needs a selector row from 0 through 1692.', 'authored-row');
      }
      var claimed = claimedRows(catalog, options.otherAuthored);
      if (claimed[row]) fail('Selector row ' + row + ' already belongs to ' + claimed[row] + '.', 'authored-row');
      var word = readU32(z64, DIRECTOR_TABLE_Z64 + row * 4);
      if (word !== 0 && word !== options.ownedRowKey) {
        fail('Selector row ' + row + ' is not free in the loaded ROM.', 'authored-row');
      }
      var template = templateScene(catalog, options.templateSceneId);
      if (template.assetId !== CLASS5_TEMPLATE) {
        fail('Authored scenes can only start from the class-5 template in this editor.', 'authored-template');
      }
      var recipe = Object.assign({}, CLASS5_ACTOR_RECIPE);
      return Promise.resolve(options.decodedBytes || bytesOf(CLASS5_SCAFFOLD_WORDS)).then(function(decoded) {
        validateAuthoredStream(decoded);
        var adapted = OB64.cutsceneCodec.sceneForBytes(template, decoded);
        var assetId = 'authored:' + authoredId;
        var launchProfile = JSON.parse(JSON.stringify(template.launchProfile || {}));
        launchProfile.parentEventLaunches = [];
        var scene = Object.assign({}, adapted, {
          sceneId: 'scene:authored:' + authoredId,
          assetId: assetId,
          storageId: assetId,
          directorKey: null,
          canonicalScene: null,
          aliases: [],
          aliasNames: [],
          friendlyName: name,
          technicalName: name,
          actors: [],
          actorCount: 0,
          recoveredActorEvents: [],
          recoveredMediaRequests: [],
          recoveredNativeSpriteEffects: [],
          dialogueAssociations: [],
          audioAssociations: [],
          reviewedTimelineOverlay: null,
          exportCapability: 'native',
          triggerStatus: 'not-launched',
          runtimeProof: 'authored-scene',
          launchProfile: launchProfile,
          authored: {
            authoredId: authoredId,
            name: name,
            selectorRow: row,
            sceneClass: 5,
            templateSceneId: template.assetId,
            provenance: 'editor-authored scene; not a retail resource',
            actorRecipe: recipe,
            initialDecodedBytes: decoded
          }
        });
        scene.source = Object.assign({}, adapted.source, {
          provenance: 'authored',
          directorSelectorRows: [row],
          directorSelectorWordZ64: [DIRECTOR_TABLE_Z64 + row * 4],
          directorSelectorTableResourceKey: template.source.directorSelectorTableResourceKey,
          directorSelectorTablePayloadZ64: template.source.directorSelectorTablePayloadZ64,
          z64PrefixStart: null,
          z64PayloadStart: null,
          z64PayloadEndExclusive: null,
          storedPayloadLength: 0,
          masterRomSha256: null,
          crcWindowOverlap: false
        });
        // Re-tile with the authored flag present so the authored insertion
        // policy (setup withheld, native exit open) applies.
        var retiled = OB64.cutsceneCodec.sceneForBytes(scene, decoded);
        scene.source = Object.assign({}, scene.source, { nodes: retiled.source.nodes });
        return OB64.cutsceneCodec.authoredSceneSource(scene, decoded, { rom: z64 }).then(function(source) {
          return { scene: source.scene, source: source };
        });
      });
    });
  }

  // ROM-only reopen: rebuild authored scenes from the arena. Structure is
  // verified independently of the manifest: every kind-1 record must own its
  // selector row and decode to a valid authored stream, and the manifest must
  // list exactly those rows. Any disagreement fails closed.
  function discover(z64, catalog) {
    var arenaApi = OB64.cutsceneExport && OB64.cutsceneExport.arena;
    if (!arenaApi) return Promise.resolve([]);
    return Promise.resolve().then(function() {
      var arena = arenaApi.read(z64);
      var authoredRows = arena.records.filter(function(row) { return row[0] >>> 28 === arenaApi.KIND.AUTHORED; });
      var manifestRow = arena.records.find(function(row) { return row[0] >>> 28 === arenaApi.KIND.MANIFEST; });
      if (!authoredRows.length && !manifestRow) return [];
      if (!manifestRow) fail('Authored Cutscene records have no manifest.', 'authored-discovery');
      var manifest = arenaApi.readManifest(z64, manifestRow);
      var listedRows = manifest.authoredScenes.map(function(entry) { return entry.selectorRow; }).sort(function(a, b) { return a - b; });
      var ownedRows = authoredRows.map(function(row) { return row[0] & 0x0FFFFFFF; }).sort(function(a, b) { return a - b; });
      if (listedRows.join() !== ownedRows.join()) {
        fail('The Cutscene manifest does not match the authored records in this ROM.', 'authored-discovery');
      }
      var chain = Promise.resolve(), found = [];
      manifest.authoredScenes.forEach(function(entry) {
        chain = chain.then(function() {
          var row = authoredRows.find(function(record) { return (record[0] & 0x0FFFFFFF) === entry.selectorRow; });
          var decoded = OB64.cutsceneCodec.decodeCustomLz(z64.slice(row[2] + 4, row[2] + 4 + row[3]), { requireExact: true }).bytes;
          return create(z64, catalog, {
            authoredId: entry.authoredId, name: entry.name, selectorRow: entry.selectorRow,
            templateSceneId: entry.templateSceneId, decodedBytes: decoded, ownedRowKey: row[1],
            otherAuthored: found.map(function(item) { return item.scene.authored; })
          }).then(function(made) {
            made.scene.authored.romState = { name: entry.name, streamHex: hexOf(decoded) };
            found.push(made);
          });
        });
      });
      return chain.then(function() { return found; });
    });
  }

  // Wipe: reduce a retail Director stream to a valid empty stream of the same
  // scene (Astra's contract, message 330). Kept, in order:
  //   - the leading 80000006 (environment) and 80000007 (media) records;
  //   - for classes 4 and 5, the instantaneous 36/35 camera pair that follows;
  //   - the parsed native exit: 7D followed only by barrier/bridge and the
  //     terminal hold with its class trailer, or 39 [0] followed only by
  //     bridges and the terminal (a presentation handoff, not a release).
  // Everything else (cast, dialogue, effects, waits, 1C/1D/45/AB) is removed.
  // A stream whose ending is not one of those two forms cannot be wiped.
  var WIPE_LEADING_RECORDS = [0x80000006, 0x80000007];

  function wipeStream(decodedBytes, definitions) {
    var words = wordsOf(decodedBytes);
    var nodes = definitions.slice().sort(function(left, right) { return left.startWord - right.startWord; });
    function refuse(reason, code) { return { status: 'unsupported', reason: reason, code: code }; }
    function wordsFor(node) { return words.slice(node.startWord, node.startWord + node.wordCount); }
    var terminal = nodes[nodes.length - 1];
    if (!terminal || terminal.opcodeU32 !== 0x80000001 || terminal.startWord + terminal.wordCount !== words.length ||
        (words[words.length - 1] >>> 8) !== 0xFF0000) {
      return refuse('This scene does not end with a terminal hold and class trailer.', 'no-terminal');
    }
    var sceneClass = words[words.length - 1] & 0xFF;
    var index = nodes.length - 2;
    while (index >= 0 && (nodes[index].opcodeU32 === 0x80000002 || nodes[index].opcodeU32 === 0x80000000)) index--;
    var exitNode = nodes[index];
    var exitKind = null;
    if (exitNode && exitNode.opcodeU32 === 0x7D && exitNode.wordCount === 1) {
      exitKind = 'director-release';
    } else if (exitNode && exitNode.opcodeU32 === 0x39 && exitNode.wordCount === 2 && words[exitNode.startWord + 1] === 0 &&
        nodes.slice(index + 1, -1).every(function(node, offset) {
          return node.opcodeU32 === 0x80000000 || (offset === 0 && node.opcodeU32 === 0x80000002);
        })) {
      exitKind = 'presentation-handoff';
    }
    // Two exact endings approved individually (Astra message 334), matched as
    // whole suffixes, never as a general rule: 7D then an unreachable 00
    // handoff marker (selector 497), and 39 [0] with a single 02 counter reset
    // between its bridges (selector 267).
    if (!exitKind) {
      var suffixes = [
        { kind: 'director-release', shape: [[0x7D, 1], [0x00, 1], [0x80000002, 1], [0x80000000, 1], [0x80000001, 2]] },
        { kind: 'presentation-handoff', shape: [[0x39, 2], [0x80000000, 1], [0x02, 1], [0x80000000, 1], [0x80000001, 2]] },
        { kind: 'presentation-handoff', shape: [[0x39, 2], [0x80000002, 1], [0x80000000, 1], [0x02, 1], [0x80000000, 1], [0x80000001, 2]] }
      ];
      suffixes.some(function(suffix) {
        var start = nodes.length - suffix.shape.length;
        if (start < 0) return false;
        var matches = suffix.shape.every(function(entry, offset) {
          return nodes[start + offset].opcodeU32 === entry[0] && nodes[start + offset].wordCount === entry[1];
        }) && (suffix.shape[0][0] !== 0x39 || words[nodes[start].startWord + 1] === 0);
        if (matches) { index = start; exitNode = nodes[start]; exitKind = suffix.kind; }
        return matches;
      });
    }
    if (!exitKind) {
      return refuse('This scene ends with opcode 0x' + (exitNode ? exitNode.opcodeU32.toString(16).toUpperCase() : '?') +
        ', not a supported Director release (7D) or presentation handoff (39 0).', 'unsupported-ending');
    }
    var lead = [];
    var at = 0;
    while (at < nodes.length && WIPE_LEADING_RECORDS.indexOf(nodes[at].opcodeU32) >= 0) lead.push(nodes[at++]);
    var cameras = [];
    if ((sceneClass === 4 || sceneClass === 5) && nodes[at] && nodes[at + 1] &&
        [nodes[at].opcodeU32, nodes[at + 1].opcodeU32].sort().join() === [0x35, 0x36].sort().join()) {
      cameras = [nodes[at], nodes[at + 1]];
    }
    var exitNodes = nodes.slice(index);
    // A BB teardown just before the exit relaunches the parent event (native
    // case BB frees and replaces the stream); its empty handoff is not defined
    // yet (Astra message 332).
    var before = nodes[index - 1], beforeBefore = nodes[index - 2];
    if ((before && before.opcodeU32 === 0xBB) ||
        (before && before.opcodeU32 === 0x80000000 && beforeBefore && beforeBefore.opcodeU32 === 0xBB)) {
      return refuse('This scene relaunches the parent event; its empty handoff is not yet defined.', 'relaunch-ending');
    }
    // Authored commands go in front of the exit, which needs a barrier before
    // the next bridge (a failing condition scans to the bridge). A 39 handoff
    // has none, so one barrier is normalized directly after 39; the handoff
    // opcode and everything after it are kept (Astra message 330, item 2).
    var normalizedBarrier = exitKind === 'presentation-handoff' && exitNodes[1] && exitNodes[1].opcodeU32 !== 0x80000002;
    var kept = [];
    lead.concat(cameras).forEach(function(node) { kept.push.apply(kept, wordsFor(node)); });
    exitNodes.forEach(function(node, offset) {
      kept.push.apply(kept, wordsFor(node));
      if (offset === 0 && normalizedBarrier) kept.push(0x80000002);
    });
    var setupWordCount = lead.concat(cameras).reduce(function(total, node) { return total + node.wordCount; }, 0);
    return {
      status: 'ok',
      bytes: bytesOf(kept),
      sceneClass: sceneClass,
      exitKind: exitKind,
      setupWordCount: setupWordCount,
      exitStartWord: setupWordCount,
      sourceExitStartWord: exitNode.startWord,
      keptRecords: lead.map(function(node) { return node.opcodeU32; }),
      keptCameras: cameras.length === 2,
      captureFlagRemoved: nodes.some(function(node) { return node.opcodeU32 === 0x80000008; }),
      normalizedBarrier: normalizedBarrier,
      removedNodeCount: nodes.length - lead.length - cameras.length - exitNodes.length
    };
  }

  // The Actor recipe a scene's own stream implies: its first 0x14 creation and
  // the 0x1C channel and 0x1D scale that apply to that slot (or to all Actors).
  // A wiped scene keeps its camera, so its own cast shows the size that fits.
  function actorRecipeFromStream(decodedBytes, definitions) {
    var words = wordsOf(decodedBytes);
    var nodes = definitions.slice().sort(function(left, right) { return left.startWord - right.startWord; });
    var create = nodes.find(function(node) { return node.opcodeU32 === 0x14 && node.wordCount === 10; });
    if (!create) return null;
    var w = words.slice(create.startWord, create.startWord + 10).map(function(value) { return value | 0; });
    function presentation(opcode, low, high) {
      var node = nodes.find(function(candidate) {
        var target = words[candidate.startWord + 1] | 0;
        return candidate.opcodeU32 === opcode && candidate.wordCount === 3 && (target === -1 || target === w[1]);
      });
      var value = node ? words[node.startWord + 2] | 0 : null;
      return value !== null && value >= low && value <= high ? value : null;
    }
    return {
      bank: w[2], animationKey: w[3], nativeFacing: w[4],
      x: w[5] / 1000, y: w[6] / 1000, z: w[7] / 1000,
      renderMode: w[8] & 0xFF, variantSelector: w[9] & 0xFF,
      transformChannel: presentation(0x1C, 0, 19), uniformScalePercent: presentation(0x1D, 1, 400),
      provenance: 'first Actor this scene created before it was wiped'
    };
  }

  // A wiped scene keeps its setup camera but loses the moves that framed its
  // original cast, so a new Actor starts on the X 0 line at the original depth
  // and height (checked on the Stage in tests/cutscene-wipe-scene.test.js).
  function wipedActorRecipe(decodedBytes, definitions) {
    var own = actorRecipeFromStream(decodedBytes, definitions);
    if (own) return Object.assign(own, { x: 0, provenance: own.provenance + ', started on the X 0 line' });
    return Object.assign({}, CLASS5_ACTOR_RECIPE, {
      provenance: 'class-5 template recipe; this scene creates no Actor of its own, so check the Size'
    });
  }

  OB64.cutsceneAuthored = {
    wipeStream: wipeStream,
    actorRecipeFromStream: actorRecipeFromStream,
    wipedActorRecipe: wipedActorRecipe,
    discover: discover,
    AuthoredSceneError: AuthoredSceneError,
    CLASS5_TEMPLATE: CLASS5_TEMPLATE,
    CLASS5_EXIT_TAIL: CLASS5_EXIT_TAIL.slice(),
    CLASS5_TEMPLATE_RETAIL_SHA256: CLASS5_TEMPLATE_RETAIL_SHA256,
    CLASS5_SCAFFOLD_WORDS: CLASS5_SCAFFOLD_WORDS.slice(),
    CLASS5_ACTOR_RECIPE: CLASS5_ACTOR_RECIPE,
    directorTableZ64: DIRECTOR_TABLE_Z64,
    directorTableRows: DIRECTOR_TABLE_ROWS,
    scaffoldFromTemplate: scaffoldFromTemplate,
    actorRecipe: actorRecipe,
    validateAuthoredStream: validateAuthoredStream,
    freeSelectorRows: freeSelectorRows,
    randomId: randomId,
    validId: validId,
    create: create
  };
})(window.OB64);
