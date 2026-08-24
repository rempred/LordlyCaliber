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
  const profiledScenes = scenes.filter(scene => scene.source.dynamicGrammar !== true);
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

  assert.strictEqual(scenes.length, 1498);
  assert.strictEqual(profiledScenes.length, 60);
  assert.strictEqual(OB64.cutsceneData.directorEvents.length, 154);

  for (const scene of profiledScenes) {
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

  let retailPrimitiveCount = 0;
  let retailWordCount = 0;
  let retailTerminalCount = 0;
  let retailOpcode5ECount = 0;
  let retailSubstreamCallCount = 0;
  let retailTailCallCount = 0;
  const retailOpcodes = new Set();
  for (const scene of scenes) {
    const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
    const ir = OB64.cutsceneCodec.createIr(
      scene, source.decodedBytes, source.nodeDefinitions);
    assert.strictEqual(ir.program.wordCount, scene.source.decodedWordCount);
    assert.strictEqual(ir.program.primitives.length,
      scene.source.dynamicGrammar === true
        ? scene.source.runtimeNodeCount : scene.source.nodes.length);
    const terminal = ir.program.primitives[ir.program.primitives.length - 1];
    assert.strictEqual(terminal.opcode, 0x80000001,
      scene.assetId + ' must end in the native terminal hold');
    assert.strictEqual(terminal.wordCount, 2,
      scene.assetId + ' terminal must own its trailer');
    retailTerminalCount++;
    retailPrimitiveCount += ir.program.primitives.length;
    retailWordCount += ir.program.wordCount;
    ir.program.primitives.forEach(primitive => {
      retailOpcodes.add(primitive.opcode);
      if (primitive.opcode === 0x5E) retailOpcode5ECount++;
      if (primitive.opcode === 0x99) retailSubstreamCallCount++;
      if (primitive.opcode === 0x80000003) retailTailCallCount++;
    });
  }
  assert.strictEqual(retailTerminalCount, 1498);
  assert.strictEqual(retailPrimitiveCount, 193771);
  assert.strictEqual(retailWordCount, 550019);
  assert.strictEqual(retailOpcodes.size, 133);
  assert.strictEqual(retailOpcode5ECount, 10);
  assert.strictEqual(retailSubstreamCallCount, 247);
  assert.strictEqual(retailTailCallCount, 4);

  const dynamicActorScene = scenes.find(scene =>
    scene.source.dynamicGrammar === true && scene.actorBearing);
  const dynamicActorSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, dynamicActorScene, { hashBytes });
  const dynamicActorDocument = OB64.cutsceneCodec.projectSceneDocument(
    dynamicActorScene, dynamicActorSource, catalog).document;
  assert(dynamicActorDocument.actors.length > 0,
    'runtime tiling must project exact opcode-0x14 actors in non-profiled retail streams');

  const projection = OB64.cutsceneData.directorEvents.find(event =>
    event.opcode === '0x2C');
  const actorCamera = OB64.cutsceneData.directorEvents.find(event =>
    event.opcode === '0x36');
  assert.strictEqual(projection.semanticName, 'scene_projection_transform_transition');
  assert.strictEqual(actorCamera.semanticName, 'actor_side_camera_pose_set');

  console.log('PASS corrected Director model preserves 60 enriched streams and tiles all ' +
    '1,498 retail resources into 193,771 primitives and 550,019 words; the enriched corpus ' +
    'recognizes 572 exact counter-wait composites; ' +
    multiPrimitiveCount + ' total multi-command bundles were recognized across ' +
    compositeCount + ' editor actions.');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
