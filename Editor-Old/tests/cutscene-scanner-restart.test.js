'use strict';

// Native failure scan (z64 0x2862FC): a failing condition scans forward to the
// next bridge and needs a barrier in between; with none the scanner returns -1
// and the parser restarts the stream from word 0 in the same call. Live-proven
// 2026-09-26 with Test ROM v1 (block inserted before the opening ceremony's
// barrier-less first wait looped and re-created its window until the dialogue
// pool ran out) and Test ROM v2 (block after the first wait ran cleanly). The
// preview runtime must reproduce the restart, and the codec must withhold such
// insertion points.

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
const M = OB64.cutsceneModel;
const layout = { id: 'us-rev0' };

async function build(z64, variant) {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scene = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { allowModified: true });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  const document = structuredClone(baseline);
  const arms = scene.source.nodes.filter(node => node.name === 'registered_counter_arm');
  const boundary = variant === 'v1' ? arms[0] : arms.find(node => node.insertBefore);
  if (variant === 'v1') {
    assert.equal(boundary.insertBefore, false, 'the codec withholds the boundary before the barrier-less first wait');
    assert.equal(boundary.insertionWithheld, 'next-condition-barrier-less');
    boundary.insertBefore = true; delete boundary.insertionWithheld; // force it, test vector only
  }
  const mover = document.actors.find(actor => actor.slot === 2);
  function track(type, actorId, label) {
    let row = document.tracks.find(candidate => candidate.type === type && candidate.actorId === (actorId || null));
    if (!row) { row = M.createTrack({ id: 'track:authored:' + type + (actorId ? ':' + actorId : ''), type, actorId: actorId || null, label }); document.tracks.push(row); }
    return row;
  }
  const clip = (id, kind, start, duration, payload) => M.createClip({ id, kind, startFrame: start, durationFrames: duration, capability: 'native', payload, source: { insertBeforeNodeId: boundary.id } });
  track('effect', null, 'Scene fades').clips.push(clip('clip:authored:fade', 'fade', 100, 45, { direction: 'from-black', red: 0, green: 0, blue: 0, startAlpha: 255, targetAlpha: 0 }));
  track('flow', null, 'Flow').clips.push(clip('clip:authored:wait', 'wait', 101, 60, {}));
  track('camera', null, 'Camera').clips.push(clip('clip:authored:camera', 'camera', 102, 45, { presentationKind: 'projection-transform', target: { translateX: 0, translateY: -30, scaleX: 1, scaleY: 1 } }));
  track('movement', mover.id, 'move').clips.push(clip('clip:authored:move', 'movement', 103, 60, { from: { x: mover.initial.x, y: 0, z: mover.initial.z }, to: { x: 200, y: 0, z: -55 }, nativeSpeed: 1, durationMode: 'duration' }));
  const template = document.native.commands.find(command => command.words[0] === 0xBF && command.words.length === 14);
  const selector = template.words[2];
  const entry = OB64.cutsceneAuthoring.readDialogue(z64, selector).entries.length;
  const windowId = Math.max(...document.native.commands.filter(command => command.words[0] === 0xBF).map(command => command.words[1])) + 1;
  track('dialogue', mover.id, 'speak').clips.push(clip('clip:authored:speak', 'dialogue', 104, 90, { sourceSystem: 'serifu-authored-native', nativeDialogueAuthored: true, nativeTemplateNodeId: template.source.nodeId, presentationArchiveSelector: selector, presentationEntrySelector: entry, windowId, nativeDelayTicks: 30, ownerSlot: 2, portrait: 120, speaker: 'Editor', text: 'EDITOR TEST LINE.' }));
  M.validateSceneDocument(document);
  const state = { catalog: { scenes: [scene] }, histories: {}, originalSerialized: {}, sourceByAssetId: {} };
  state.histories[scene.storageId] = M.createHistory(document);
  state.originalSerialized[scene.storageId] = M.serializeSceneDocument(baseline, 0);
  state.sourceByAssetId[scene.assetId] = source;
  const candidate = { z64: z64.slice(), layout };
  const plan = await OB64.cutsceneExport.prepare({ z64, layout, cutsceneStudio: state }, candidate);
  OB64.cutsceneExport.apply(candidate, plan); OB64.cutsceneExport.validateApplied(candidate, plan);
  const ui = OB64.cutsceneUI.initialize({ z64: candidate.z64, layout });
  const reopened = ui.catalog.getScene(scene.assetId); ui.selectedSceneId = reopened.sceneId;
  await OB64.cutsceneUI.loadScene({ z64: candidate.z64, layout }, ui, reopened);
  return { run: ui.runtimeByAssetId[reopened.assetId], scene: reopened, boundary, windowId, ui };
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);

  const v1 = await build(z64, 'v1');
  assert(v1.run.scannerRestarts >= 1, 'v1 layout makes the parser restart the stream');
  const pause = v1.scene.source.nodes.find(node => node.name === 'dialogue_pause_query' && node.startWord > v1.boundary.startWord);
  const restart = v1.run.trace.find(entry => entry.kind === 'scanner-restart');
  assert(restart && restart.nodeId === pause.id, 'the restart is triggered by the authored dialogue pause query');
  assert.equal(restart.reason, 'barrier-depth-1-absent-before-bridge');
  const opens = v1.run.trace.filter(entry => entry.kind === 'composite' && entry.label === 'Open dialogue window ' + v1.windowId + ' and wait for text').map(entry => entry.tick);
  assert(new Set(opens).size >= 2, 'the authored window is created again after the restart: ' + opens.join(','));

  const v2 = await build(z64, 'v2');
  assert.equal(v2.run.scannerRestarts, 0, 'v2 layout runs without a restart');
  assert(v2.run.outcome === 'modeled-termination', v2.run.outcome);
  assert(!v2.run.trace.some(entry => entry.kind === 'scanner-restart'));

  // The creator toolbar surfaces restarts as an export warning.
  global.document = { createElement: () => ({ children: [], attrs: {}, style: {}, className: '', textContent: '', classList: { contains() { return false; }, toggle() {}, add(name) { this.names = (this.names || []).concat(name); } }, appendChild(child) { this.children.push(child); return child; }, setAttribute(key, value) { this.attrs[key] = value; }, addEventListener() {}, focus() {} }) };
  v1.ui.ui = {};
  const root = global.document.createElement('main');
  const source = fs.readFileSync(path.join(editor, 'cutscene-ui.js'), 'utf8').replace('    render: render,', '    render: render, testCreator: renderCreatorWorkspace,');
  vm.runInThisContext(source, { filename: 'cutscene-ui-test.js' });
  OB64.cutsceneUI.testCreator(root, { z64, layout }, v1.ui, v1.scene, v1.ui.histories[v1.scene.storageId].present);
  const texts = []; (function walk(node) { if (node.textContent) texts.push(node.textContent); (node.children || []).forEach(walk); })(root);
  assert(texts.some(text => /restart this scene from the top/.test(text)), 'toolbar warns about the restart');
  console.log('scanner restart: ok (v1 restarts ' + v1.run.scannerRestarts + ', v2 restarts 0)');
})().catch(error => { console.error(error && (error.stack || error)); process.exit(1); });
