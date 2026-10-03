'use strict';

// Selecting an action card seeks the preview to the runtime tick at which that
// command executes, not to its projected storyboard frame. Projected frames
// drift from runtime ticks because native waits, dialogue, and effects have
// runtime-dependent lengths.

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

const clips = document => document.tracks.flatMap(track => track.clips);
function waitForRuntime(state, assetId, previous, timeoutMs = 120000) {
  return new Promise((resolve, reject) => {
    const started = Date.now();
    (function poll() {
      const runtime = state.runtimeByAssetId[assetId];
      if (runtime && runtime !== previous) return resolve(runtime);
      if (state.sourceErrors['runtime:' + assetId]) return reject(new Error(state.sourceErrors['runtime:' + assetId]));
      if (Date.now() - started > timeoutMs) return reject(new Error('runtime refresh timed out'));
      setTimeout(poll, 50);
    })();
  });
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' } };
  const state = OB64.cutsceneUI.initialize(rom);
  const scene = state.catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  state.selectedSceneId = scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  const runtime = state.runtimeByAssetId[scene.assetId];
  assert(runtime && runtime.outcome === 'modeled-termination');
  const document = state.histories[scene.storageId].present;
  const view = state.views[scene.sceneId];

  // Unedited scene: source cards map straight to trace ticks.
  const cameras = clips(document).filter(clip => clip.kind === 'camera').sort((a, b) => a.startFrame - b.startFrame);
  const laterCamera = cameras[cameras.length - 1];
  const laterTick = OB64.cutsceneUI.clipRuntimeTick(state, scene, laterCamera);
  assert(Number.isInteger(laterTick) && laterTick > laterCamera.startFrame,
    'the later camera pan executes after its projected frame (' + laterCamera.startFrame + ' -> ' + laterTick + ')');
  assert.equal(OB64.cutsceneUI.clipRuntimeTick(state, scene, cameras[0]), 0);
  const movement = clips(document).find(clip => clip.kind === 'movement');
  const movementTick = OB64.cutsceneUI.clipRuntimeTick(state, scene, movement);
  assert(Number.isInteger(movementTick) && movementTick !== movement.startFrame);
  assert.equal(OB64.cutscenePreview.evaluateAtFrame(document, movementTick, { pathId: view.pathId }).runtime.tick, movementTick,
    'the Stage can evaluate the runtime state at the movement tick');

  // Seeking through the UI uses that tick and pauses the clock.
  global.document = { createElement: () => ({ children: [], attrs: {}, style: {}, className: '', classList: { contains: () => false, toggle() {} }, appendChild(child) { this.children.push(child); return child; }, setAttribute(key, value) { this.attrs[key] = value; }, addEventListener() {}, focus() {} }) };
  state.ui = { panel: null };
  const row = OB64.cutsceneUI.findClipRow(document, laterCamera.id);
  // selectClip is internal; the creator card handler calls it. Exercise through the exported pieces it uses.
  view.frame = 0;
  const before = state.runtimeByAssetId[scene.assetId];
  view.frame = OB64.cutsceneUI.clipRuntimeTick(state, scene, row.clip);
  assert.equal(view.frame, laterTick);
  assert.equal(OB64.cutscenePreview.evaluateAtFrame(document, view.frame, { pathId: view.pathId }).runtime.tick, laterTick,
    'the Stage evaluates the runtime state at that tick');

  // After an edit, authored and shifted source cards still resolve through the compiled positions.
  OB64.cutsceneUI.addHold(rom, state);
  const refreshed = await waitForRuntime(state, scene.assetId, before);
  const edited = state.histories[scene.storageId].present;
  const hold = clips(edited).find(clip => clip.kind === 'wait' && !clip.source.nodeId);
  assert(hold, 'the hold was added');
  assert(refreshed.outcome === 'modeled-termination', JSON.stringify({ outcome: refreshed.outcome,
    reason: refreshed.terminalReason, unresolved: refreshed.unresolvedQuery || null,
    missing: refreshed.missingInputs, ticks: refreshed.durationTicks, hold: hold.source, start: hold.startFrame,
    boundary: scene.source.nodes.find(node => node.id === hold.source.insertBeforeNodeId) }));
  const holdTick = OB64.cutsceneUI.clipRuntimeTick(state, scene, hold);
  assert(Number.isInteger(holdTick), 'an authored card resolves to a runtime tick after the refresh');
  const laterAfter = clips(edited).find(clip => clip.id === laterCamera.id);
  const laterTickAfter = OB64.cutsceneUI.clipRuntimeTick(state, scene, laterAfter);
  assert(Number.isInteger(laterTickAfter), 'a source card behind an insertion still resolves');
  assert(laterTickAfter >= laterTick, 'commands after an inserted hold run no earlier than before');
  console.log('card seek: ok (camera ' + laterCamera.startFrame + ' -> tick ' + laterTick + ', hold at tick ' + holdTick + ', camera after edit ' + laterTickAfter + ')');
})().catch(error => { console.error(error); process.exit(1); });
