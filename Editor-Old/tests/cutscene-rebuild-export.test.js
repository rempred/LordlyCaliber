'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
const nativeHarness = fs.readFileSync(path.join(__dirname, 'native-rebuild-runtime-composition.test.js'), 'utf8')
  .split('assert.strictEqual(data.composition.phase4Matrix.stateCount')[0];
const { freshRom } = new Function('require', '__dirname', nativeHarness + '\nreturn { freshRom };')(require, __dirname);
for (const file of ['cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-authoring.js', 'cutscene-project.js', 'cutscene-export.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}

(async () => {
  const rom = freshRom();
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scene = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  const source = await OB64.cutsceneCodec.loadSceneSource(rom.z64, scene);
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  const document = structuredClone(baseline);
  document.actors[0].initial.x += 15;
  const dialogue = document.tracks.flatMap(track => track.clips).find(clip => clip.payload.nativeDialogueEditable);
  dialogue.payload.rawText = dialogue.payload.rawText.replace('Six years later', 'Our tale starts');
  rom.cutsceneStudio = {
    catalog, histories: { [scene.storageId]: OB64.cutsceneModel.createHistory(document) },
    originalSerialized: { [scene.storageId]: OB64.cutsceneModel.serializeSceneDocument(baseline, 0) },
    sourceByAssetId: { [scene.assetId]: source }
  };
  OB64.buildModes.set(rom, 'rebuild');
  rom.tools.desired['character-card-luck'] = true;
  const nativePlan = OB64.nativeRebuild.prepareExport(rom, { shopsDirty: false });
  const candidate = Object.assign({}, rom, { z64: nativePlan.baseZ64.slice() });
  const cutscenePlan = await OB64.cutsceneExport.prepare(rom, candidate);
  const owners = nativePlan.owners.concat(OB64.cutsceneExport.patchOwner(cutscenePlan));
  assert.deepStrictEqual(OB64.consumableEffects.findRegionConflicts(owners), []);
  nativePlan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(owners);
  OB64.cutsceneExport.apply(candidate, cutscenePlan);
  const nativeResult = OB64.nativeRebuild.applyPlan(nativePlan, candidate, null);
  if (nativeResult.crc) OB64.recalcN64CRC(candidate.z64);
  OB64.nativeRebuild.validateApplied(candidate, nativePlan, null);
  OB64.cutsceneExport.validateApplied(candidate, cutscenePlan);
  assert(OB64.consumableEffects.verifyIndependentCrc(candidate.z64).ok);
  assert.equal(OB64.cutsceneAuthoring.dialogueEntry(candidate.z64,
    dialogue.payload.presentationArchiveSelector, dialogue.payload.presentationEntrySelector).rawText,
    dialogue.payload.rawText);
  if (process.env.OB64_CUTSCENE_OUTPUT) {
    fs.mkdirSync(path.dirname(process.env.OB64_CUTSCENE_OUTPUT), { recursive: true });
    const v64 = Uint8Array.from(candidate.z64, (_, index) => candidate.z64[index ^ 1]);
    fs.writeFileSync(process.env.OB64_CUTSCENE_OUTPUT, v64);
    fs.writeFileSync(path.join(path.dirname(process.env.OB64_CUTSCENE_OUTPUT), 'director.bin'),
      cutscenePlan.changedEntries[0].result.intendedDecodedBytes);
    fs.writeFileSync(path.join(path.dirname(process.env.OB64_CUTSCENE_OUTPUT), 'demo.json'), JSON.stringify({
      scene: scene.sceneId, actorSlot: document.actors[0].slot,
      oldPosition: baseline.actors[0].initial, newPosition: document.actors[0].initial,
      text: 'Six years later -> Our tale starts', nativeFeature: 'Character Card Luck'
    }, null, 2));
  }
  console.log('PASS Rebuild ROM composes Actor/text edits with the native Luck feature, preserves resources, and has a valid cartridge checksum.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
