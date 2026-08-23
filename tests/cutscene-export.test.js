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
  'cutscene-director.js', 'cutscene-codec.js', 'cutscene-project.js', 'cutscene-export.js'
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
  const z64 = normalizeV64(fs.readFileSync(MASTER));
  const scene = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  const document = OB64.cutsceneModel.cloneSceneDocument(baseline);
  const pose = document.tracks.flatMap(track => track.clips)
    .find(clip => clip.kind === 'pose' && clip.capability === 'native');
  pose.payload.nativeFacing = (pose.payload.nativeFacing + 1) % 4;
  pose.payload.facing = 'native-' + pose.payload.nativeFacing;

  const state = {
    catalog,
    histories: { [scene.storageId]: OB64.cutsceneModel.createHistory(document) },
    originalSerialized: {
      [scene.storageId]: OB64.cutsceneModel.serializeSceneDocument(baseline, 0)
    },
    sourceByAssetId: { [scene.assetId]: source }
  };
  const sourceRom = { z64: z64.slice(), layout: { id: 'us-rev0' }, cutsceneStudio: state };
  const candidateRom = { z64: sourceRom.z64.slice(), layout: sourceRom.layout };
  const beforePlan = candidateRom.z64.slice();
  const plan = await OB64.cutsceneExport.prepare(sourceRom, candidateRom, { hashBytes });
  assert.strictEqual(plan.changedSceneCount, 1);
  assert(OB64.cutsceneCodec.equalBytes(candidateRom.z64, beforePlan),
    'planning must not mutate the detached candidate');
  const owner = OB64.cutsceneExport.patchOwner(plan);
  assert.strictEqual(owner.regions.length, 1);
  const applied = OB64.cutsceneExport.apply(candidateRom, plan);
  assert.strictEqual(applied.changedSceneCount, 1);
  assert(!OB64.cutsceneCodec.equalBytes(candidateRom.z64, beforePlan));
  const readback = OB64.cutsceneExport.validateApplied(candidateRom, plan);
  assert.strictEqual(readback.details.sceneCount, 1);

  sourceRom.z64 = candidateRom.z64.slice();
  OB64.cutsceneExport.adopt(sourceRom, plan);
  const range = plan.changedEntries[0].result.changedRange;
  assert(OB64.cutsceneCodec.equalBytes(
    sourceRom.z64.slice(range.start, range.endExclusive),
    plan.changedEntries[0].result.originalSlotBytes
  ), 'authoring source slot must stay available for deterministic re-export');

  const blockedDocument = OB64.cutsceneModel.cloneSceneDocument(document);
  blockedDocument.background.assetId = 'archive:100';
  blockedDocument.background.capability = 'native';
  blockedDocument.background.layers = [];
  const blockedState = {
    catalog,
    histories: { [scene.storageId]: OB64.cutsceneModel.createHistory(blockedDocument) },
    originalSerialized: {
      [scene.storageId]: OB64.cutsceneModel.serializeSceneDocument(baseline, 0)
    },
    sourceByAssetId: { [scene.assetId]: source }
  };
  const blockedRom = { z64: z64.slice(), layout: { id: 'us-rev0' }, cutsceneStudio: blockedState };
  const blockedCandidate = { z64: blockedRom.z64.slice(), layout: blockedRom.layout };
  await assert.rejects(
    OB64.cutsceneExport.prepare(blockedRom, blockedCandidate, { hashBytes }),
    error => error.code === 'preview-only-background'
  );
  assert(OB64.cutsceneCodec.equalBytes(blockedCandidate.z64, z64));

  const insertedHold = OB64.cutsceneModel.cloneSceneDocument(baseline);
  const flowTrack = insertedHold.tracks.find(track => track.type === 'flow');
  const boundary = scene.source.nodes.find(node => node.insertBefore && node.nodeType !== 'gap');
  flowTrack.clips.push(OB64.cutsceneModel.createClip({
    id: 'clip:authored:wait:test', kind: 'wait', startFrame: 0,
    durationFrames: 5, capability: 'native', payload: {},
    source: { insertBeforeNodeId: boundary.id }
  }));
  assert.doesNotThrow(() => OB64.cutsceneExport.assertFixedSlotDelta(
    scene, baseline, insertedHold),
  'adding a clip to a shared flow track must not make preserved markers look edited');

  const changedActor = OB64.cutsceneModel.cloneSceneDocument(baseline);
  changedActor.actors[0].initial.x += 1;
  assert.doesNotThrow(() => OB64.cutsceneExport.assertFixedSlotDelta(
    scene, baseline, changedActor),
  'an actor backed by a typed opcode-0x14 Place command must remain editable');

  const immutableScene = catalog.getScene('scene:director:01f56d8e');
  assert(immutableScene, 'catalog needs a display-only pose scene');
  const immutableSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, immutableScene, { hashBytes });
  const immutableBaseline = OB64.cutsceneCodec.projectSceneDocument(
    immutableScene, immutableSource, catalog).document;
  const immutableDocument = OB64.cutsceneModel.cloneSceneDocument(immutableBaseline);
  const immutablePose = immutableDocument.tracks.flatMap(track => track.clips)
    .find(clip => clip.kind === 'pose' && clip.capability === 'preview-only');
  assert(immutablePose, 'fixture needs one display-only pose');
  immutablePose.payload.nativeFacing = (immutablePose.payload.nativeFacing + 1) % 4;
  immutablePose.payload.facing = 'native-' + immutablePose.payload.nativeFacing;
  assert.throws(
    () => OB64.cutsceneExport.assertFixedSlotDelta(
      immutableScene, immutableBaseline, immutableDocument),
    error => error.code === 'immutable-clip'
  );

  console.log('PASS Cutscene export plans without writes, applies bounded slots, and reparses the result.');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
