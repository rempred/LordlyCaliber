'use strict';

// Export-based playback of an edit that relocates its Director stream must launch the
// relocated resource. The planned key is stored in the codec's hex-string form that
// ROM startup and the runtime parse; a numeric key made the launch miss its selector.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of [
  'data.js', 'parsers.js', 'repack.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-authored.js', 'cutscene-authoring.js', 'cutscene-dialogue-data.js',
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

const M = OB64.cutsceneModel;
const A = OB64.cutsceneExport.arena;

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) };
  const state = OB64.cutsceneUI.initialize(rom);
  await state.authoredReady;
  const scene = state.catalog.getScene('rom-custom-lz:01FA4D0A');
  state.selectedSceneId = scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  assert(state.runtimeByAssetId[scene.assetId], 'the unedited scene previews from ROM startup');

  // Grow the stream past its retail slot so export-based playback relocates it.
  const document = M.cloneSceneDocument(state.histories[scene.storageId].present);
  const boundary = scene.source.nodes.find(node => node.insertBefore && node.nodeType !== 'gap');
  let flow = document.tracks.find(track => track.type === 'flow');
  if (!flow) { flow = M.createTrack({ id: 'track:authored:flow', type: 'flow', actorId: null, label: 'Flow' }); document.tracks.push(flow); }
  for (let index = 0; index < 100; index++) {
    flow.clips.push(M.createClip({ id: 'clip:authored:wait:' + index, kind: 'wait', startFrame: index,
      durationFrames: 1 + ((index * 7919) % 8000), capability: 'native', payload: {},
      source: { insertBeforeNodeId: boundary.id } }));
  }
  state.histories[scene.storageId] = M.createHistory(document);
  delete state.runtimeByAssetId[scene.assetId];
  await OB64.cutsceneUI.loadScene(rom, state, scene);

  // The relocated resource key is stored in the codec's hex-string form, so ROM
  // startup finds its selector and launches the relocated stream. (With the
  // earlier numeric key this preview failed: "Selected Director resource is
  // absent from the loaded ROM directory.")
  const runtime = state.runtimeByAssetId[scene.assetId];
  assert(runtime, JSON.stringify(state.sourceErrors));
  const launch = runtime.trace.find(row => row.kind === 'director-launch-initializer');
  assert(launch, 'the relocated preview starts through the native launch');
  const arenaFirstKey = A.start - 0x00594280, arenaEndKey = A.descriptorStart - 0x00594280;
  assert(launch.resourceKey >= arenaFirstKey && launch.resourceKey < arenaEndKey,
    'the launch resolves the selector to the relocated arena resource');
  console.log('PASS a relocating edit previews through ROM startup, whose selector resolves to the relocated resource.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
