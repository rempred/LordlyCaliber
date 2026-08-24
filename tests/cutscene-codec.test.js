'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const MASTER = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of [
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js',
  'cutscene-director.js', 'cutscene-codec.js', 'cutscene-preview.js'
]) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}

function normalizeV64(raw) {
  const output = new Uint8Array(raw.length);
  for (let index = 0; index < raw.length; index += 2) {
    output[index] = raw[index + 1];
    output[index + 1] = raw[index];
  }
  return output;
}

function hashBytes(input) {
  return crypto.createHash('sha256').update(Buffer.from(input)).digest('hex').toUpperCase();
}

(async function main() {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const directorScenes = catalog.scenes.filter(scene =>
    scene.engine === 'director' && scene.source.dynamicGrammar !== true);
  const z64 = normalizeV64(fs.readFileSync(MASTER));
  let totalNodes = 0;
  let actorTracks = 0;
  const actorCountMismatches = [];

  for (const scene of directorScenes) {
    const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
    const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source);
    let compiled;
    try {
      compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, projected.document);
    } catch (error) {
      error.message = scene.assetId + ': ' + error.message;
      throw error;
    }
    assert.strictEqual(compiled.noOp, true, scene.assetId +
      ' projection must compile byte-identically: ' + JSON.stringify(compiled.changes));
    assert(OB64.cutsceneCodec.equalBytes(compiled.decodedBytes, source.decodedBytes));
    const plan = await OB64.cutsceneCodec.planFixedCapacityExport(
      z64, scene, projected.document, { hashBytes });
    assert.strictEqual(plan.noOp, true, scene.assetId + ' no-op plan');
    assert(OB64.cutsceneCodec.equalBytes(plan.candidateZ64, z64));
    totalNodes += scene.source.nodes.length;
    actorTracks += projected.document.actors.length;
    if (projected.document.actors.length !== scene.actorCount) {
      actorCountMismatches.push({ assetId: scene.assetId, catalog: scene.actorCount,
        projected: projected.document.actors.length,
        slots: projected.document.actors.map(actor => actor.slot) });
    }
  }

  assert.strictEqual(totalNodes, 8451);
  assert.strictEqual(actorTracks, 205, JSON.stringify(actorCountMismatches));
  assert.deepStrictEqual(actorCountMismatches, []);

  const runtimeTiledScenes = catalog.directorScenes.filter(scene =>
    scene.source.dynamicGrammar === true);
  for (const scene of runtimeTiledScenes) {
    const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
    const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog);
    const compiled = OB64.cutsceneCodec.compileSceneDocument(
      scene, source, projected.document);
    assert.strictEqual(compiled.noOp, true,
      scene.assetId + ' runtime-tiled projection must compile byte-identically');
    assert(OB64.cutsceneCodec.equalBytes(compiled.decodedBytes, source.decodedBytes),
      scene.assetId + ' runtime-tiled projection must preserve every Director word');
  }
  assert.strictEqual(runtimeTiledScenes.length, 1438);

  const launchContextScene = catalog.getScene('rom-director:01F53488');
  const launchContextSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, launchContextScene, { hashBytes });
  const launchContextProjected = OB64.cutsceneCodec.projectSceneDocument(
    launchContextScene, launchContextSource, catalog);
  assert.deepStrictEqual(launchContextProjected.document.background.layers.map(layer =>
    layer.assetId), ['section-c-njpg:20', 'archive:32'],
  'launch pre-scan interpretation must supply the independent environment and foreground without changing Director words');
  launchContextProjected.document.background.projection.launchContext = {
    mode: 2,
    override: true,
    environmentSelector: 57,
    foregroundSelector: 57,
    evidenceStatus: 'user-supplied-launch-context'
  };
  const contextCompiled = OB64.cutsceneCodec.compileSceneDocument(
    launchContextScene, launchContextSource, launchContextProjected.document);
  assert.strictEqual(contextCompiled.noOp, true,
    'launch selectors are caller-owned project context, not Director operands');
  assert(OB64.cutsceneCodec.equalBytes(
    contextCompiled.decodedBytes, launchContextSource.decodedBytes));

  const translatedScene = catalog.getScene('rom-director:01FA64D2');
  const translatedSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, translatedScene, { hashBytes });
  const translatedProjected = OB64.cutsceneCodec.projectSceneDocument(
    translatedScene, translatedSource, catalog);
  const translatedNoOp = OB64.cutsceneCodec.compileSceneDocument(
    translatedScene, translatedSource, translatedProjected.document);
  assert.strictEqual(translatedNoOp.noOp, true);
  assert(OB64.cutsceneCodec.equalBytes(
    translatedNoOp.decodedBytes, translatedSource.decodedBytes));
  const translatedActor = translatedProjected.document.actors.find(actor =>
    actor.source.variantSelectorTranslationIndex === 0);
  assert(translatedActor);
  translatedActor.initial.x += 1;
  const translatedActorEdit = OB64.cutsceneCodec.compileSceneDocument(
    translatedScene, translatedSource, translatedProjected.document);
  const translatedActorNode = translatedProjected.program.primitives.find(node =>
    node.id === translatedActor.source.placeNodeId);
  const translatedReadback = new DataView(translatedActorEdit.decodedBytes.buffer,
    translatedActorEdit.decodedBytes.byteOffset,
    translatedActorEdit.decodedBytes.byteLength);
  assert.strictEqual(translatedReadback.getUint32(
    (translatedActorNode.startWord + 9) * 4, false), 0x08880000,
  'editing another Actor field must preserve its launch-translated appearance word');

  const opening = catalog.getScene('opening-title-cutscene');
  const openingSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, opening, { hashBytes });
  const openingProjected = OB64.cutsceneCodec.projectSceneDocument(
    opening, openingSource, catalog);
  assert.deepStrictEqual(openingProjected.document.background.layers.map(layer =>
    layer.role), [
    'environment-base', 'ordered-layer', 'ordered-layer', 'ordered-layer', 'ordered-layer',
  ], 'image encoding must not promote inferred scene-group tiles above Actors');

  const graduation = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, graduation, { hashBytes });
  const projected = OB64.cutsceneCodec.projectSceneDocument(graduation, source);
  const poseTrack = projected.document.tracks.find(track =>
    track.type === 'pose' && track.clips.length);
  assert(poseTrack, 'Graduation Ceremony needs a source-backed pose');
  const pose = poseTrack.clips[0];
  const originalFacing = pose.payload.nativeFacing;
  pose.payload.nativeFacing = (originalFacing + 1) % 4;
  pose.payload.facing = 'native-' + pose.payload.nativeFacing;
  const changed = OB64.cutsceneCodec.compileSceneDocument(
    graduation, source, projected.document);
  assert.strictEqual(changed.noOp, false);
  assert.strictEqual(changed.changes.length, 1);
  assert.strictEqual(changed.changes[0].operation, 'edit');

  const plan = await OB64.cutsceneCodec.planFixedCapacityExport(
    z64, graduation, projected.document, { hashBytes });
  assert.strictEqual(plan.noOp, false);
  assert.strictEqual(plan.readback.intendedBytesMatch, true);
  assert(plan.encodedBytes <= plan.capacityBytes);
  assert(plan.changedRange);

  await assert.rejects(
    OB64.cutsceneCodec.planFixedCapacityExport(z64, graduation, projected.document, {
      hashBytes,
      encoder() { return new Uint8Array(graduation.source.storedPayloadLength + 1); }
    }),
    error => error.code === 'capacity-overflow'
  );

  pose.startFrame += 1;
  assert.throws(
    () => OB64.cutsceneCodec.compileSceneDocument(graduation, source, projected.document),
    error => error.code === 'compile-timing'
  );

  const waitProjected = OB64.cutsceneCodec.projectSceneDocument(
    graduation, source, catalog);
  const fixedWait = waitProjected.document.tracks.flatMap(track => track.clips)
    .find(clip => clip.kind === 'wait' &&
      clip.payload.registeredCounterEditable === true);
  assert(fixedWait, 'Graduation Ceremony needs one editable fixed counter-wait bundle');
  const editedTicks = fixedWait.payload.nativeTicks + 7;
  fixedWait.durationFrames = editedTicks;
  fixedWait.payload.nativeTicks = editedTicks;
  fixedWait.payload.registeredCounterTarget = editedTicks;
  const waitChanged = OB64.cutsceneCodec.compileSceneDocument(
    graduation, source, waitProjected.document);
  assert.strictEqual(waitChanged.noOp, false);
  assert.deepStrictEqual(waitChanged.changes, [{
    operation: 'edit', nodeId: fixedWait.source.nodeId,
    kind: 'registered_counter_query'
  }]);
  const waitDefinition = graduation.source.nodes.find(node =>
    node.id === fixedWait.source.nodeId);
  const waitReadback = new DataView(waitChanged.decodedBytes.buffer,
    waitChanged.decodedBytes.byteOffset, waitChanged.decodedBytes.byteLength);
  assert.strictEqual(waitReadback.getInt32((waitDefinition.startWord + 2) * 4, false),
    editedTicks, 'fixed-wait lowering must change only the Q3 target word');

  console.log('PASS Cutscene codec byte-round-trips all 1,498 retail Director resources, preserves ' +
    totalNodes + ' enriched boundaries and every runtime-tiled word, keeps caller-owned launch ' +
    'selectors and placeholders intact, then plans bounded native edits.');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
