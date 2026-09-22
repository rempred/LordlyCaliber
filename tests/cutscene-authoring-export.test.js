'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(process.env.OB64_EDITOR_ROOT || path.join(__dirname, '..'));
const masterName = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';
const master = [process.env.OB64_MASTER_ROM, path.join(editor, '..', masterName),
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
const clips = doc => doc.tracks.flatMap(track => track.clips);

async function load(z64, assetId) {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scene = catalog.getScene(assetId);
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { allowModified: true });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  return { catalog, scene, source, baseline, document: structuredClone(baseline) };
}

async function exportScenes(z64, rows) {
  const catalog = { scenes: rows.map(row => row.scene) };
  const state = { catalog, histories: {}, originalSerialized: {}, sourceByAssetId: {} };
  for (const row of rows) {
    state.histories[row.scene.storageId] = OB64.cutsceneModel.createHistory(row.document);
    state.originalSerialized[row.scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(row.baseline, 0);
    state.sourceByAssetId[row.scene.assetId] = row.source;
  }
  const candidate = { z64: z64.slice(), layout };
  const plan = await OB64.cutsceneExport.prepare({ z64, layout, cutsceneStudio: state }, candidate);
  assert.deepStrictEqual(candidate.z64, z64, 'planning is detached');
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  return { candidate, plan };
}

function addHolds(row, count, start = 0) {
  const boundary = row.scene.source.nodes.find(node => node.insertBefore && node.nodeType !== 'gap');
  assert(boundary);
  let track = row.document.tracks.find(track => track.type === 'flow');
  if (!track) {
    track = OB64.cutsceneModel.createTrack({ id: 'track:authored:flow', type: 'flow', actorId: null, label: 'Flow' });
    row.document.tracks.push(track);
  }
  for (let index = start; index < count + start; index++) {
    track.clips.push(OB64.cutsceneModel.createClip({
      id: 'clip:authored:wait:' + index, kind: 'wait', startFrame: index,
      durationFrames: 1 + ((index * 7919) % 8000), capability: 'native', payload: {},
      source: { insertBeforeNodeId: boundary.id }
    }));
  }
}

(async () => {
  const graduation = await load(retail, 'loading-magnus-walk-opening-ceremony-cutscene');
  const dialogue = clips(graduation.document).find(clip => clip.payload.nativeDialogueEditable);
  const selector = dialogue.payload.presentationArchiveSelector;
  const entry = dialogue.payload.presentationEntrySelector;
  const before = OB64.cutsceneAuthoring.readDialogue(retail, selector);
  const secondEntry = (entry + 1) % before.entries.length;
  const shared = structuredClone(graduation.document);
  const sharedClip = clips(shared).find(clip => clip.id === dialogue.id);
  sharedClip.payload.presentationEntrySelector = secondEntry;
  sharedClip.payload.originalRawText = before.entries[secondEntry].rawText;
  sharedClip.payload.rawText = before.entries[secondEntry].rawText + ' Added line.';
  dialogue.payload.rawText += ' First change.';
  const merged = OB64.cutsceneAuthoring.dialogueResources(retail, [graduation.document, shared]);
  assert.equal(merged.length, 1, 'different entries in one archive share one write');
  const conflicting = structuredClone(graduation.document);
  clips(conflicting).find(clip => clip.id === dialogue.id).payload.rawText += ' Conflict.';
  assert.throws(() => OB64.cutsceneAuthoring.dialogueResources(retail, [graduation.document, conflicting]), /different text/);

  let seed = 43;
  const longText = Array.from({ length: 9000 }, () => {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    return String.fromCharCode(65 + seed % 26);
  }).join('');
  dialogue.payload.rawText += longText;
  const travel = await load(retail, 'rom-custom-lz:01FA4D0A');
  addHolds(travel, 100);
  const exported = await exportScenes(retail, [graduation, travel]);
  assert.equal(exported.plan.relocatedDialogueCount, 1, 'long text relocates');
  assert.equal(exported.plan.relocatedSceneCount, 1, 'long Director stream relocates');
  const after = OB64.cutsceneAuthoring.readDialogue(exported.candidate.z64, selector);
  assert.equal(after.entries[entry].rawText, dialogue.payload.rawText);
  before.entries.forEach((row, index) => {
    if (index !== entry) assert.equal(after.entries[index].rawText, row.rawText, 'unmodified shared entries survive');
  });

  // Reopen a relocated ROM, edit it again, and grow an existing allocation.
  const reopened = await load(exported.candidate.z64, travel.scene.assetId);
  addHolds(reopened, 100, 1000);
  const textAgain = await load(exported.candidate.z64, graduation.scene.assetId);
  clips(textAgain.document).find(clip => clip.payload.nativeDialogueEditable).payload.rawText += ' Another edit.';
  const second = await exportScenes(exported.candidate.z64, [reopened, textAgain]);
  const final = await load(second.candidate.z64, travel.scene.assetId);
  assert(final.source.decodedBytes.length > reopened.source.decodedBytes.length);
  assert.equal(OB64.cutsceneAuthoring.readDialogue(second.candidate.z64, selector).entries[entry].rawText,
    dialogue.payload.rawText + ' Another edit.');
  assert.equal(retail[0x2780000], 255, 'master input remains unchanged');

  // Unknown arena contents must never be overwritten.
  const occupied = retail.slice();
  occupied[0x2780000] = 1;
  await assert.rejects(exportScenes(occupied, [graduation, travel]), /unrecognized data/);
  console.log('PASS shared dialogue merge/conflict, text and stream growth, exported-ROM re-editing, and allocation bounds.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
