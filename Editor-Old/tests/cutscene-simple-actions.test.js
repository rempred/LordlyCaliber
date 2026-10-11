'use strict';

// Simple authoring actions (Move, Pose, Wait, Speak, Camera pan, Fade) must
// compile to native Director commands in any retail Director scene, not only
// the sixty reviewed scenes. This checks the generic insertion-point rule, the
// Fade adapter, and export through the shared cutscene exporter.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(process.env.OB64_EDITOR_ROOT || path.join(__dirname, '..'));
const masterName = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';
const master = [process.env.OB64_MASTER_ROM, path.join(require('./project-paths.cjs').PARENT, masterName),
  path.join(editor, '../../OgreBattlel64', masterName)].filter(Boolean).find(file => fs.existsSync(file));
if (!master) throw new Error('Set OB64_MASTER_ROM to the US Rev 0 ROM.');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-model.js',
  'cutscene-catalog.js', 'cutscene-director.js', 'cutscene-codec.js', 'cutscene-authoring.js',
  'cutscene-project.js', 'cutscene-export.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}
const raw = fs.readFileSync(master);
const retail = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
const layout = { id: 'us-rev0' };
const M = OB64.cutsceneModel;

function words(bytes, start, count) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return Array.from({ length: count }, (_, index) => view.getInt32((start + index) * 4));
}

async function load(catalog, z64, assetId) {
  const scene = catalog.getScene(assetId);
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { allowModified: true });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  return { scene, source, baseline, document: structuredClone(baseline) };
}

async function exportScene(z64, row) {
  const state = { catalog: { scenes: [row.scene] }, histories: {}, originalSerialized: {}, sourceByAssetId: {} };
  state.histories[row.scene.storageId] = M.createHistory(row.document);
  state.originalSerialized[row.scene.storageId] = M.serializeSceneDocument(row.baseline, 0);
  state.sourceByAssetId[row.scene.assetId] = row.source;
  const candidate = { z64: z64.slice(), layout };
  const plan = await OB64.cutsceneExport.prepare({ z64, layout, cutsceneStudio: state }, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  return { candidate, plan };
}

function trackFor(document, type, actorId, label) {
  let track = document.tracks.find(row => row.type === type && row.actorId === (actorId || null));
  if (!track) {
    track = M.createTrack({ id: 'track:authored:' + type + (actorId ? ':' + actorId : ''), type,
      actorId: actorId || null, label });
    document.tracks.push(track);
  }
  return track;
}

function clip(id, kind, startFrame, durationFrames, payload, boundary) {
  return M.createClip({ id, kind, startFrame, durationFrames, capability: 'native', payload,
    source: { insertBeforeNodeId: boundary.id } });
}

(async () => {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const retailScenes = OB64.cutsceneData.scenes.filter(scene => scene.source.dynamicGrammar === true);
  assert(retailScenes.length > 1400, 'retail-grammar scenes are the bulk of the corpus');

  // Find one unreviewed retail scene with a native move and a registered wait.
  let row = null;
  for (const data of retailScenes) {
    const candidate = await load(catalog, retail, data.assetId);
    const nodes = candidate.scene.source.nodes;
    if (nodes.some(node => node.opcodeU32 === 0x07) &&
        nodes.some(node => node.name === 'registered_counter_arm') &&
        candidate.document.actors.length) { row = candidate; break; }
  }
  assert(row, 'a retail scene with moves, waits, and actors exists');
  const { scene, source, document } = row;
  assert.equal(scene.source.runtimeTiled, true, 'retail scenes adopt their tiled nodes on load');
  assert.equal(scene.source.dynamicGrammar, true, 'the retail-grammar identity is retained');
  assert(scene.source.nodes.length > 0);
  assert.equal(scene.source.nodes[0].insertBefore, false, 'nothing is inserted before scene setup');
  assert(scene.source.nodes.some(node => node.editPolicy === 'typed-move'), 'existing moves are typed edits');

  // Insertion points never fall inside a composite (wait, envelope, or dialogue handshake).
  const program = OB64.cutsceneCodec.createIr(scene, source.decodedBytes, scene.source.nodes).program;
  const boundaries = scene.source.nodes.filter(node => node.insertBefore);
  assert(boundaries.length > 5, 'a retail scene exposes many insertion points, not only its terminator');
  for (const node of boundaries) {
    const composite = program.compositeById[program.compositeByNodeId[node.id]];
    assert(composite && composite.nodeIds[0] === node.id, node.name + ' must start its composite');
  }
  for (const node of scene.source.nodes) {
    if (['registered_counter_query', 'branch_barrier', 'control_bridge_and_pending_substream_handoff',
      'registered_counter_reset', 'dialogue_pause_query'].includes(node.name)) {
      assert.equal(node.insertBefore, false, node.name + ' is never an insertion point');
    }
  }
  const waitComposites = program.composites.filter(row => row.kind === 'registered-wait').length;
  assert(waitComposites > 0, 'registered waits are recognized structurally in retail scenes');

  // Author a sequence in front of a mid-stream wait: fade to black, wait, move, camera pan.
  const boundary = boundaries.find(node => node.name === 'registered_counter_arm' &&
    scene.source.nodes.indexOf(node) > 3);
  assert(boundary, 'a registered wait is an insertion point');
  const actor = document.actors[0];
  trackFor(document, 'effect', null, 'Scene fades').clips.push(clip('clip:authored:fade', 'fade', 100, 60,
    { direction: 'to-black', red: 0, green: 0, blue: 0, startAlpha: null, targetAlpha: 255 }, boundary));
  trackFor(document, 'flow', null, 'Flow').clips.push(clip('clip:authored:wait', 'wait', 101, 30, {}, boundary));
  trackFor(document, 'movement', actor.id, actor.label + ' movement').clips.push(clip('clip:authored:move', 'movement', 102, 60,
    { from: { x: actor.initial.x, y: actor.initial.y, z: actor.initial.z },
      to: { x: actor.initial.x + 60, y: actor.initial.y, z: actor.initial.z }, nativeSpeed: 1, durationMode: 'duration' }, boundary));
  trackFor(document, 'camera', null, 'Camera').clips.push(clip('clip:authored:camera', 'camera', 103, 45,
    { presentationKind: 'projection-transform', target: { translateX: 0, translateY: -20, scaleX: 1, scaleY: 1 } }, boundary));
  M.validateSceneDocument(document);

  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, document);
  const at = boundary.startWord;
  const inserted = Object.fromEntries(compiled.changes.filter(change => change.clipId).map(change => [change.clipId, change.startWord]));
  assert.deepStrictEqual(inserted, { 'clip:authored:fade': at, 'clip:authored:wait': at + 7,
    'clip:authored:move': at + 15, 'clip:authored:camera': at + 23 }, 'the compiler reports each authored command position');
  assert.equal(compiled.nodeStartWords[boundary.id], at + 29, 'retained nodes report their shifted position');
  assert.equal(compiled.nodeStartWords[scene.source.nodes[1].id], scene.source.nodes[1].startWord, 'nodes before the insertion keep their position');
  assert.deepStrictEqual(words(compiled.decodedBytes, at, 7), [0x1B, 60, 0, 0, 0, -1, 255], 'fade compiles to opcode 0x1B');
  assert.deepStrictEqual(words(compiled.decodedBytes, at + 7, 8), [1, 0, -0x80000000 + 2, -0x80000000, 0x0E, 0, 30, 2], 'wait compiles to the registered wait unit');
  assert.deepStrictEqual(words(compiled.decodedBytes, at + 15, 8), [7, actor.slot, -1000, -1000, Math.round((actor.initial.x + 60) * 1000), Math.round(actor.initial.z * 1000), 0, 1000], 'move compiles to opcode 0x07');
  assert.deepStrictEqual(words(compiled.decodedBytes, at + 23, 6), [0x2C, 0, -20000, 1000, 1000, 45], 'camera pan compiles to opcode 0x2C');
  const reparsed = OB64.cutsceneCodec.sceneForBytes(scene, compiled.decodedBytes);
  const reparsedProgram = OB64.cutsceneCodec.createIr(reparsed, compiled.decodedBytes).program;
  assert.equal(reparsedProgram.composites.filter(row => row.kind === 'registered-wait').length, waitComposites + 1,
    'the authored wait is one more complete registered wait; existing waits stay intact');
  assert.equal(reparsedProgram.primitives.length, program.primitives.length + 1 + 6 + 1 + 1,
    'four authored actions add fade, six wait primitives, move, and camera nodes');

  // The edited scene exports through the shared exporter and reopens with the fade in place.
  const { candidate } = await exportScene(retail, row);
  const reopened = await load(OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData), candidate.z64, scene.assetId);
  assert(reopened.scene.source.nodes.some(node => node.opcodeU32 === 0x1B), 'exported ROM contains the fade command');
  assert(reopened.scene.source.nodes.filter(node => node.insertBefore).length > 5, 'reopened scenes keep insertion points');

  // A reviewed scene keeps working with the same adapters.
  const opening = await load(catalog, retail, catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene').assetId);
  const openingBoundary = opening.scene.source.nodes.find(node => node.insertBefore && node.nodeType !== 'gap');
  trackFor(opening.document, 'effect', null, 'Scene fades').clips.push(clip('clip:authored:fade', 'fade', 10, 45,
    { direction: 'from-black', red: 0, green: 0, blue: 0, startAlpha: 255, targetAlpha: 0 }, openingBoundary));
  const openingCompiled = OB64.cutsceneCodec.compileSceneDocument(opening.scene, opening.source, opening.document);
  assert.deepStrictEqual(words(openingCompiled.decodedBytes, openingBoundary.startWord, 7), [0x1B, 45, 0, 0, 0, 255, 0]);
  await exportScene(retail, opening);

  // A Wait inside a staged counter span (arm, gate 5, fade, camera, gate 35, reset)
  // must not re-arm or reset the shared counter: it becomes a bare gate at the
  // running target plus the hold, and later gates in the span shift by the hold.
  const staged = await load(catalog, retail, catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene').assetId);
  const cameraNode = staged.scene.source.nodes.find(node => node.id === 'node:01F3EAD2:w00EC');
  const laterGate = staged.scene.source.nodes.find(node => node.id === 'node:01F3EAD2:w00F5');
  assert(cameraNode && cameraNode.insertBefore && laterGate && laterGate.name === 'registered_counter_query');
  trackFor(staged.document, 'flow', null, 'Flow').clips.push(clip('clip:authored:span-wait', 'wait', 600, 30, {}, cameraNode));
  const stagedCompiled = OB64.cutsceneCodec.compileSceneDocument(staged.scene, staged.source, staged.document);
  assert.deepStrictEqual(words(stagedCompiled.decodedBytes, cameraNode.startWord, 6),
    [0, -0x80000000 + 2, -0x80000000, 0x0E, 0, 5 + 30], 'an in-span wait is a bare gate at the running target plus the hold');
  assert.deepStrictEqual(words(stagedCompiled.decodedBytes, cameraNode.startWord + 6, 2), [0x2C, 10000], 'the camera pan follows the gate');
  assert.deepStrictEqual(words(stagedCompiled.decodedBytes, laterGate.startWord + 6, 3), [0x0E, 0, 35 + 30], 'the later gate in the span shifts by the hold');
  assert(stagedCompiled.changes.some(change => change.operation === 'retime' && change.nodeId === laterGate.id && change.shift === 30));
  assert.deepStrictEqual(words(stagedCompiled.decodedBytes, laterGate.startWord + 9, 1), [2], 'the span still resets once');
  await exportScene(retail, staged);

  // Invalid fade values are refused before any ROM write.
  const broken = await load(catalog, retail, scene.assetId);
  trackFor(broken.document, 'effect', null, 'Scene fades').clips.push(clip('clip:authored:fade', 'fade', 10, 0,
    { red: 0, green: 0, blue: 0, startAlpha: null, targetAlpha: 255 }, boundary));
  await assert.rejects(exportScene(retail, broken), error => error.code === 'fade-values');
  console.log('cutscene simple actions: ok (' + scene.assetId + ', ' + boundaries.length + ' insertion points)');
})().catch(error => { console.error(error); process.exit(1); });
