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

function stateFor(catalog) {
  return {
    catalog,
    selectedSceneId: catalog.scenes[0].sceneId,
    histories: {}, originalSerialized: {}, sourceByAssetId: {}, sourceErrors: {},
    loadingByAssetId: {}, views: {}, imageCache: {}, imageCacheBytes: 0
  };
}

(async function main() {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const z64 = normalizeV64(fs.readFileSync(MASTER));
  const scene = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  const state = stateFor(catalog);
  state.selectedSceneId = scene.sceneId;
  state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(baseline);
  state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(baseline, 0);
  state.sourceByAssetId[scene.assetId] = source;
  state.views[scene.sceneId] = {
    frame: 17, pathId: 'default', selectedActorId: baseline.actors[0].id,
    selectedClipId: null, loop: true, snap: 'half', zoom: 1.5
  };

  const editablePose = state.histories[scene.storageId].present.tracks
    .flatMap(track => track.clips.map(clip => ({ track, clip })))
    .find(row => row.clip.kind === 'pose' && row.clip.capability === 'native');
  assert(editablePose, 'fixture needs one native pose');
  editablePose.clip.payload.nativeFacing = (editablePose.clip.payload.nativeFacing + 1) % 4;
  editablePose.clip.payload.facing = 'native-' + editablePose.clip.payload.nativeFacing;
  state.views[scene.sceneId].selectedClipId = editablePose.clip.id;

  const payload = OB64.cutsceneProject.collect(state);
  assert.strictEqual(payload.format, 'ob64-cutscene-project');
  assert.strictEqual(payload.schemaVersion, 1);
  assert.strictEqual(payload.scenes.length, 1);
  assert.strictEqual(payload.scenes[0].assetId, scene.assetId);
  assert.strictEqual(payload.scenes[0].view.frame, 17);
  assert.strictEqual(payload.scenes[0].view.snap, 'half');
  assert.strictEqual(payload.scenes[0].view.selectedClipId, editablePose.clip.id);

  const importState = stateFor(catalog);
  const importRom = { z64: z64.slice(), layout: { id: 'us-rev0' }, cutsceneStudio: importState };
  OB64.cutsceneUI = { ensureState(rom) { return rom.cutsceneStudio; } };
  const prepared = await OB64.cutsceneProject.prepareImport(importRom, payload, { hashBytes });
  assert.strictEqual(prepared.entries.length, 1);
  assert.strictEqual(OB64.cutsceneProject.applyPrepared(importState, prepared), 1);
  assert.strictEqual(importState.selectedSceneId, scene.sceneId);
  assert.strictEqual(importState.views[scene.sceneId].selectedActorId, baseline.actors[0].id);
  assert.strictEqual(importState.views[scene.sceneId].selectedClipId, editablePose.clip.id);
  const importedDocument = importState.histories[scene.storageId].present;
  const importedPose = importedDocument.tracks.flatMap(track => track.clips)
    .find(clip => clip.id === editablePose.clip.id);
  assert.strictEqual(importedPose.payload.nativeFacing, editablePose.clip.payload.nativeFacing);
  assert.strictEqual(importedDocument.exportRequirements.capability, 'native');
  assert(importedDocument.exportRequirements.allocationBytes > 0,
    'Project import must recalculate the live compressed budget');
  assert.strictEqual(OB64.cutsceneProject.collect(importState).scenes.length, 1);

  const forgedCapability = JSON.parse(JSON.stringify(payload));
  forgedCapability.scenes[0].document.background.assetId = 'archive:0';
  forgedCapability.scenes[0].document.exportRequirements.capability = 'native';
  forgedCapability.scenes[0].document.exportRequirements.reasons = [];
  const forgedPrepared = await OB64.cutsceneProject.prepareImport(
    importRom, forgedCapability, { hashBytes });
  assert.strictEqual(forgedPrepared.entries[0].document.exportRequirements.capability,
    'preview-only', 'Project import must not trust a saved Native badge');
  assert(/background/i.test(
    forgedPrepared.entries[0].document.exportRequirements.reasons[0]));

  const tampered = JSON.parse(JSON.stringify(payload));
  tampered.scenes[0].document.native.commands[0].words[0] =
    (tampered.scenes[0].document.native.commands[0].words[0] + 1) >>> 0;
  await assert.rejects(
    OB64.cutsceneProject.prepareImport(importRom, tampered, { hashBytes }),
    /preserved native command boundaries or words/
  );

  const newer = JSON.parse(JSON.stringify(payload));
  newer.schemaVersion = 2;
  await assert.rejects(
    OB64.cutsceneProject.prepareImport(importRom, newer, { hashBytes }),
    /newer than this editor/
  );

  console.log('PASS Cutscene Project round-trips edited SceneDocuments and rejects source tampering.');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
