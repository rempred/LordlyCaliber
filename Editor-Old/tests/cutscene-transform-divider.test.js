'use strict';

// Opcode 0x5F installs the automatic Actor transform-channel divider. The normal
// mode-zero scheduler classifies every existing Actor once per update (z64
// 0x002AF5F4), so Actors in front of the divider line join the channel whose
// layer pass draws after the desk sprite while Actors behind it stay in the
// earlier pass. Scene 01F4789A (Frederick's office) relies on it: the guard
// at (15, 40) must draw in front of the layer-2 desk record.

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

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) };
  const state = OB64.cutsceneUI.initialize(rom);
  const scene = state.catalog.getScene('rom-director:01F4789A');
  state.selectedSceneId = scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  const runtime = state.runtimeByAssetId[scene.assetId];
  assert(runtime, JSON.stringify(state.sourceErrors));
  const program = state.programByAssetId[scene.assetId];
  const dividerNode = program.primitives.find(node => node.opcode === 0x5F);
  assert.deepStrictEqual(dividerNode.rawWords.slice(1).map(word => word | 0), [-44, 17, 80, -73, 2, 0]);

  const divider = runtime.states[10].transformDivider;
  assert(divider, 'the runtime must retain the constructed divider');
  assert.strictEqual(divider.trueChannel, 2);
  assert.strictEqual(divider.falseChannel, 0);
  assert(Math.abs(divider.a - (-90 / 124)) < 1e-6, 'A = (Z2 - Z1) / (X2 - X1)');
  assert(Math.abs(divider.b - (17 - divider.a * -44)) < 1e-5, 'B = Z1 - A * X1');

  function channels(tick) {
    const preview = OB64.cutsceneRuntime.evaluate(runtime, tick);
    const result = {};
    preview.actors.forEach(actor => { result[actor.slot] = actor.transformChannel; });
    return result;
  }
  // 0x1C [-1, 1] assigns every Actor channel 1; the classifier overrides it each
  // update. Frederick (0, -75) and the guard at (5, -83) sit behind the line;
  // the guard at (15, 40), Magnus (10, 280), and the guard at (130, 250) are in front.
  assert.deepStrictEqual(channels(60), { 0: 2, 1: 0, 2: 2, 3: 0, 4: 2 },
    'the divider must classify each Actor by its X/Z placement');
  // After the scripted walk to (-45, 35) the guard remains on the front side.
  const walked = OB64.cutsceneRuntime.evaluate(runtime, 351).actors.find(actor => actor.slot === 2);
  assert(Math.abs(walked.x - -45) < 0.01 && Math.abs(walked.z - 35) < 0.01);
  assert.strictEqual(walked.transformChannel, 2);
  // Slot 4 is destroyed by 0x13 [4]; the classifier must skip absent records.
  assert.deepStrictEqual(channels(351), { 0: 2, 1: 0, 2: 2, 3: 0 });

  // The desk is the layer-2 scene-sprite record; the layered renderer draws it
  // before the channel-2 guard, so the guard's full sprite reaches the frame.
  if (!state.spriteState) state.spriteState = OB64.cutsceneSprites.create(rom.z64, state.catalog);
  const document = OB64.cutsceneUI.selectedDocument(state);
  const preview = OB64.cutscenePreview.evaluateAtFrame(document, 60, {});
  const desk = preview.effects.find(row => row.payload.nativeEffectSlot === 7);
  assert.strictEqual(desk.payload.renderPassSelector, 2, 'the desk record owns render pass 2');
  const image = await OB64.cutsceneUI.capturePreviewFrame(rom, state, document,
    { preview, backgroundPolicy: 'require', pass: 60, nodeId: 'test' });
  const guard = image.hitRegions.find(row => row.actorId === 'actor:01f4789a:slot:02');
  assert(guard, 'the front guard must be drawn');
  // Compare against the same frame with the desk record withheld: the guard's
  // region must be unchanged by the desk draw, while the desk itself is visible.
  const withoutDesk = OB64.cutscenePreview.evaluateAtFrame(document, 60, {});
  withoutDesk.effects.find(row => row.payload.nativeEffectSlot === 7).payload.nativeLifetimeEmpty = true;
  const control = await OB64.cutsceneUI.capturePreviewFrame(rom, state, document,
    { preview: withoutDesk, backgroundPolicy: 'require', pass: 60, nodeId: 'test-control' });
  let same = 0, total = 0, changedAnywhere = 0;
  for (let y = 0; y < image.height; y++) for (let x = 0; x < image.width; x++) {
    const at = (y * image.width + x) * 4;
    const equal = image.rgba[at] === control.rgba[at] && image.rgba[at + 1] === control.rgba[at + 1] &&
      image.rgba[at + 2] === control.rgba[at + 2];
    if (!equal) changedAnywhere++;
    if (x >= guard.left && x < guard.right && y >= guard.top && y < guard.bottom) { total++; if (equal) same++; }
  }
  assert(changedAnywhere > 500, 'the desk record must be drawn: ' + changedAnywhere + ' pixels differ');
  assert.strictEqual(same, total, 'the desk record must not overdraw the front guard: ' + same + '/' + total);
  console.log('PASS Opcode 0x5F classifies Actor transform channels per update and layered rendering keeps front Actors above their layer\'s scene sprites.');
})().catch(error => { console.error(error); process.exit(1); });
