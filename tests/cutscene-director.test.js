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
  'cutscene-director.js', 'cutscene-codec.js'
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
  const scenes = catalog.scenes.filter(scene => scene.engine === 'director');
  const z64 = normalizeV64(fs.readFileSync(MASTER));
  let primitiveCount = 0;
  let wordCount = 0;
  let compositeCount = 0;
  let multiPrimitiveCount = 0;
  let registeredWaitCount = 0;
  let skippableWaitCount = 0;
  let dialogueOpenCount = 0;
  let transitionGateCount = 0;
  let queryCount = 0;
  const observedOpcodes = new Set();

  assert.strictEqual(scenes.length, 60);
  assert.strictEqual(OB64.cutsceneData.directorEvents.length, 153);

  for (const scene of scenes) {
    assert.strictEqual(scene.source.gaps.length, 0, scene.assetId + ' must have no source gaps');
    assert(scene.source.nodes.every(node => !node.controlEntryAlias && !node.rawGap));
    const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
    const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog);
    const program = projected.program;
    assert(program);
    assert.strictEqual(program.wordCount, scene.source.decodedWordCount);
    assert.strictEqual(program.primitives.length, scene.source.nodes.length);
    assert.strictEqual(program.stats.registeredWaitCount,
      scene.source.registeredWaits.length);

    const coveredNodeIds = program.composites.flatMap(composite => composite.nodeIds);
    assert.strictEqual(coveredNodeIds.length, program.primitives.length);
    assert.strictEqual(new Set(coveredNodeIds).size, program.primitives.length);
    assert(program.primitives.every(primitive =>
      program.compositeByNodeId[primitive.id]));

    for (const primitive of program.primitives) {
      observedOpcodes.add(primitive.opcode);
      assert.strictEqual(primitive.rawWords.length, primitive.wordCount);
      if (primitive.name === 'control_bridge_and_pending_substream_handoff') {
        assert.strictEqual(primitive.wordCount, 1);
      }
      if (primitive.query) {
        queryCount++;
        assert.strictEqual(primitive.wordCount,
          primitive.query.recordKind === 'Q4' ? 4 : 3);
      }
    }

    primitiveCount += program.primitives.length;
    wordCount += program.wordCount;
    compositeCount += program.composites.length;
    multiPrimitiveCount += program.stats.multiPrimitiveCompositeCount;
    registeredWaitCount += program.stats.registeredWaitCount;
    skippableWaitCount += program.stats.skippableWaitCount;
    dialogueOpenCount += program.composites.filter(row =>
      row.kind === 'dialogue-window-open').length;
    transitionGateCount += program.composites.filter(row =>
      row.kind === 'start-and-completion-gate').length;
  }

  assert.strictEqual(primitiveCount, 8451);
  assert.strictEqual(wordCount, 21927);
  assert.strictEqual(observedOpcodes.size, 58);
  assert.strictEqual(queryCount, 1249);
  assert.strictEqual(registeredWaitCount, 464);
  assert.strictEqual(skippableWaitCount, 108);
  assert(multiPrimitiveCount > registeredWaitCount + skippableWaitCount);
  assert(dialogueOpenCount > 0);
  assert(transitionGateCount > 0);

  const projection = OB64.cutsceneData.directorEvents.find(event =>
    event.opcode === '0x2C');
  const actorCamera = OB64.cutsceneData.directorEvents.find(event =>
    event.opcode === '0x36');
  assert.strictEqual(projection.semanticName, 'scene_projection_transform_transition');
  assert.strictEqual(actorCamera.semanticName, 'actor_side_camera_pose_set');

  console.log('PASS corrected Director model preserves 60 streams, 8,451 primitives, ' +
    '21,927 words, and recognizes 572 exact counter-wait composites; ' +
    multiPrimitiveCount + ' total multi-command bundles were recognized across ' +
    compositeCount + ' editor actions.');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
