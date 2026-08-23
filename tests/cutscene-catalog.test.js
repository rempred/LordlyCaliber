'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of ['cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}

const C = OB64.cutsceneCatalog;
const catalog = C.createCatalog(OB64.cutsceneData);
const directorScenes = catalog.scenes.filter(scene => scene.engine === 'director');

assert.strictEqual(OB64.cutsceneData.schemaVersion, 4);
assert.strictEqual(catalog.scenes.length, 70);
assert.strictEqual(directorScenes.length, 60);
assert(directorScenes.every(scene => scene.launchProfile &&
  scene.launchProfile.profileId === 'launch-profile:' + scene.assetId));
const launchModeCounts = directorScenes.reduce((counts, scene) => {
  const mode = scene.launchProfile.directorMode;
  const key = mode.evidenceStatus + ':' + mode.value;
  counts[key] = (counts[key] || 0) + 1;
  return counts;
}, {});
assert.deepStrictEqual(launchModeCounts, {
  'runtime-observed:0': 4,
  'runtime-observed:2': 6,
  'stream-structural:0': 8,
  'stream-structural:2': 26,
  'external-unresolved:null': 16
});
const launchBackgroundRequests = directorScenes.flatMap(scene =>
  scene.launchProfile.background.requests);
assert.strictEqual(launchBackgroundRequests.length, 41);
assert.strictEqual(launchBackgroundRequests.filter(request =>
  request.selectorSource === 'corpus-coordinated-command-mirror').length, 26);
const observedModeTwoRequests = directorScenes.filter(scene =>
  scene.launchProfile.directorMode.value === 2 &&
  scene.launchProfile.directorMode.evidenceStatus === 'runtime-observed' &&
  scene.backgroundRequests.length);
assert.strictEqual(observedModeTwoRequests.length, 5);
assert(observedModeTwoRequests.every(scene =>
  scene.backgroundRequests[0].commandOperand ===
    scene.launchProfile.background.requests[0].selector));
assert.strictEqual(catalog.imageAssets.length, 366);
assert.strictEqual(directorScenes.filter(scene => scene.actorBearing).length, 48);
assert.strictEqual(directorScenes.reduce((sum, scene) => sum + scene.actorCount, 0), 205);
assert.strictEqual(catalog.imageAssets.filter(asset => asset.renderable).length, 366);
assert(catalog.imageAssets.every(asset => asset.renderable
  ? asset.unsupportedReason === null
  : typeof asset.unsupportedReason === 'string' && asset.unsupportedReason.length > 0));
assert.strictEqual(catalog.actorArtSources.length, 68);
assert.strictEqual(catalog.posePrograms.length, 2275);
assert.strictEqual(OB64.cutsceneData.counts.renderablePoseSelectors, 2259);
assert.strictEqual(OB64.cutsceneData.counts.controlOnlyPoseSelectors, 16);
assert.strictEqual(OB64.cutsceneData.counts.structuralPoseStates, 0);
assert.strictEqual(OB64.cutsceneData.counts.sourceUnpublishedPoseStates, 1554);
assert.strictEqual(catalog.posePrograms.filter(program =>
  !program.sourceProgramDefined && program.frames.length).length, 1554);
assert.strictEqual(catalog.dialogueArchives.length, 349);
assert.strictEqual(catalog.dialogueArchives.reduce((sum, archive) =>
  sum + archive.entryCount, 0), 5200);
assert.strictEqual(catalog.getDialogueArchive('meswin:815').entryCount, 631);
assert.strictEqual(catalog.getImageAsset('archive:688'), null);
assert.strictEqual(catalog.getImageAsset('scene-resource:016C90D4').sourceKind, 'rom-resource');
assert.strictEqual(
  catalog.getBackgroundSelectorTable('background-table:mode2-environment:80')
    .tableResourceKey,
  4,
  'mode-2 environment bases must come from resource 4');
assert.strictEqual(
  catalog.getBackgroundSelectorEntry('background-table:mode2-environment:80', 57).assetId,
  'mode2-environment:00236B58');
assert.deepStrictEqual(
  catalog.getBackgroundSelectorEntry('background-table:mode2-overlay:80', 57).archiveAssetIds,
  ['archive:49'],
  'selector 57 foreground must remain independent from its complete environment base');
assert.strictEqual(
  catalog.getBackgroundSelectorEntry('background-table:mode2-overlay:80', 57).resourceKey,
  0x00304464);

const graduation = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
assert(graduation);
assert.strictEqual(graduation.friendlyName, 'Graduation Ceremony');
assert.strictEqual(catalog.getScene('starting-choices-cutscene'), graduation);
assert.strictEqual(catalog.getScene(graduation.assetId), graduation);
assert.strictEqual(catalog.searchScenes('graduation').length, 1);
assert.strictEqual(catalog.searchScenes('starting choices').length, 1);
assert.strictEqual(catalog.searchScenes('', { actorBearing: false }).length, 20);
assert.strictEqual(directorScenes.filter(scene => !scene.actorBearing).length, 12);

const document = C.createSceneDocument(graduation);
assert.strictEqual(document.identity.sceneId, graduation.sceneId);
assert.strictEqual(document.actors.length, 8);
assert.strictEqual(document.native.sourceAssetId, graduation.assetId);
assert.strictEqual(document.exportRequirements.capability, 'needs-research');
assert(document.actors.every(actor => actor.source.visibilityStatus));

const souko = catalog.getImageAsset('archive:100');
assert(souko);
assert.strictEqual(souko.container, 'bg2');
assert.strictEqual(souko.width, 415);
assert.strictEqual(souko.height, 303);
assert.strictEqual(souko.renderable, true);
assert.strictEqual(catalog.searchImages('souko').length, 1);
assert(catalog.searchImages('', { family: 'background' }).length >= 52);

const magnusWalk = catalog.getPoseProgram(30, 1, 2);
assert(magnusWalk);
assert.strictEqual(magnusWalk.stateIndex, 12);
assert.strictEqual(magnusWalk.durationFrames, 48);
assert.deepStrictEqual(magnusWalk.frames.map(frame => frame.frameToken), [35, 30, 31, 32, 33, 34]);
assert.strictEqual(catalog.poseProgramsForBank(30).length, 148);

const zeroDurationProgram = catalog.posePrograms.find(program =>
  program.programId === 'physical-program:bank:015:state:0027');
assert(zeroDurationProgram);
assert(zeroDurationProgram.frames.some(frame => frame.durationFrames === 0));

const duplicate = JSON.parse(JSON.stringify(OB64.cutsceneData));
duplicate.scenes[1].sceneId = duplicate.scenes[0].sceneId;
assert.throws(() => C.createCatalog(duplicate), /duplicates/);

const brokenBoundary = JSON.parse(JSON.stringify(OB64.cutsceneData));
brokenBoundary.scenes[0].source.nodes[0].startWord = 1;
assert.throws(() => C.createCatalog(brokenBoundary), /contiguous source boundary/);

console.log('PASS Cutscene catalog exposes 60 Director streams, 10 presentation rows, aliases, actors, and image assets.');
