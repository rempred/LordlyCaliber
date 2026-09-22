'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const editor = path.resolve(process.env.OB64_EDITOR_ROOT || path.join(__dirname, '..'));
const masterName = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';
const master = [process.env.OB64_MASTER_ROM, path.join(editor, '..', masterName),
  path.join(editor, '../../OgreBattlel64', masterName)].filter(Boolean).find(file => fs.existsSync(file));
if (!master) throw new Error('Set OB64_MASTER_ROM to the US Rev 0 ROM.');

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of [
  'data.js', 'parsers.js', 'repack.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-authoring.js', 'cutscene-dialogue-data.js',
  'cutscene-dialogue-lifecycle-data.js', 'cutscene-dialogue.js', 'cutscene-dialogue-lifecycle.js',
  'cutscene-resource-scheduler-data.js', 'cutscene-resource-scheduler.js',
  'cutscene-director-launch-data.js', 'cutscene-director-launch.js', 'cutscene-rom-start-data.js',
  'cutscene-rom-start.js', 'cutscene-preserved-stage-data.js', 'cutscene-preserved-stage.js',
  'cutscene-framebuffer-data.js', 'cutscene-framebuffer.js', 'cutscene-image-echo-data.js',
  'cutscene-image-echo.js', 'cutscene-shared-actor-data.js', 'cutscene-shared-actor.js',
  'cutscene-map-menu-data.js', 'cutscene-map-menu.js', 'cutscene-runtime.js', 'cutscene-preview.js',
  'cutscene-assets.js', 'cutscene-njpg.js', 'cutscene-sprites.js', 'cutscene-renderer.js',
  'cutscene-dialogue-draw.js', 'cutscene-project.js', 'cutscene-export.js', 'rom-compatibility.js', 'cutscene-ui.js'
]) vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });

const raw = fs.readFileSync(master);
const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
const freshRom = bytes => ({ z64: bytes, archives: OB64.findArchives(bytes), layout: { id: 'us-rev0' } });
const runFor = (state, scene) => {
  const runtime = state.runtimeByAssetId[scene.assetId];
  assert(runtime && !runtime.missingInputs.length, JSON.stringify(state.sourceErrors));
  assert.equal(runtime.outcome, 'modeled-termination', JSON.stringify(runtime.unresolvedQuery));
  return runtime;
};
const textAppears = (run, text) => run.states.some(state => state.dialogue.some(dialogue =>
  dialogue.payload.nativeDialogue?.text.includes(text)));
async function exported(rom) {
  const candidate = freshRom(rom.z64.slice());
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  candidate.archives = OB64.findArchives(candidate.z64);
  return candidate;
}

(async () => {
  const rom = freshRom(z64);
  const state = OB64.cutsceneUI.initialize(rom);
  const scene = state.catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  state.selectedSceneId = scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  const baseline = runFor(state, scene);
  await OB64.cutsceneUI.executeEdit(rom, state, 'Change Actor and dialogue', document => {
    document.actors[0].initial.x += 15;
    const dialogue = document.tracks.flatMap(track => track.clips).find(clip => clip.payload.nativeDialogueEditable);
    assert(dialogue);
    dialogue.payload.rawText = dialogue.payload.rawText.replace('Six years later', 'Our tale starts');
  });
  const edited = runFor(state, scene);
  assert(textAppears(edited, 'Our tale starts'));
  assert(edited.states.some((frame, index) => JSON.stringify(frame.actors.map(actor => [actor.x, actor.z])) !==
    JSON.stringify(baseline.states[index]?.actors.map(actor => [actor.x, actor.z]))), 'Actor placement changes');

  const payload = JSON.parse(JSON.stringify(OB64.cutsceneProject.collect(state)));
  const importedRom = freshRom(z64.slice());
  const imported = await OB64.cutsceneProject.prepareImport(importedRom, payload);
  OB64.cutsceneProject.applyPrepared(importedRom.cutsceneStudio, imported);
  const importedScene = importedRom.cutsceneStudio.catalog.getScene(scene.assetId);
  await OB64.cutsceneUI.loadScene(importedRom, importedRom.cutsceneStudio, importedScene);
  assert.equal(runFor(importedRom.cutsceneStudio, importedScene).states.length, edited.states.length);

  const candidate = await exported(rom);
  const reopened = OB64.cutsceneUI.initialize(candidate);
  const target = reopened.catalog.getScene(scene.assetId);
  reopened.selectedSceneId = target.sceneId;
  await OB64.cutsceneUI.loadScene(candidate, reopened, target);
  assert(textAppears(runFor(reopened, target), 'Our tale starts'));
  // A Project saved against an exported ROM must load against that ROM again.
  await OB64.cutsceneUI.executeEdit(candidate, reopened, 'Continue editing exported ROM', document => {
    document.actors[0].initial.x += 1;
  });
  const reopenedProject = JSON.parse(JSON.stringify(OB64.cutsceneProject.collect(reopened)));
  const sameBase = freshRom(candidate.z64.slice());
  await OB64.cutsceneProject.prepareImport(sameBase, reopenedProject);
  await exported(candidate);

  const template = state.catalog.getScene('rom-custom-lz:01FA4D0A');
  state.selectedSceneId = template.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, template);
  await OB64.cutsceneUI.createFromTemplate(rom, state);
  await OB64.cutsceneUI.executeEdit(rom, state, 'Author template Actor', document => {
    document.actors[0].initial.x += 20;
    const pose = document.tracks.flatMap(track => track.clips).find(clip => clip.kind === 'pose' && clip.capability === 'native');
    assert(pose);
    pose.payload.nativeFacing = (pose.payload.nativeFacing + 1) % 4;
    pose.payload.facing = 'native-' + pose.payload.nativeFacing;
  });
  const beforeHold = runFor(state, template).states.length;
  await OB64.cutsceneUI.addHold(rom, state);
  const created = runFor(state, template);
  assert(created.states.length > beforeHold, 'added hold changes playback');
  assert(state.histories[template.storageId].present.identity.friendlyName.startsWith('New scene - '));
  const both = await exported(rom);
  const bothState = OB64.cutsceneUI.initialize(both);
  const newTarget = bothState.catalog.getScene(template.assetId);
  await OB64.cutsceneUI.loadScene(both, bothState, newTarget);
  assert.equal(runFor(bothState, newTarget).states.length, created.states.length);
  console.log('PASS Actor/text edits, Project reload, exported-ROM re-editing, template replacement, and added-hold playback.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
