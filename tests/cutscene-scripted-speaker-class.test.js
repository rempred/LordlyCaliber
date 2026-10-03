'use strict';

// After the deployed-unit rebuild, a scene slot that 0xA6 binds to a member reads
// unit 30, the encountered unit. A box that slot owns names the member's class,
// so the preview infers the encountered unit from it instead of copying the
// sample party (01F40650: member 0 speaks as a Wizard, class 12).

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
  const rom = { z64, layout: { id: 'us-rev0' } };
  const state = OB64.cutsceneUI.initialize(rom);
  const scene = state.catalog.getScene('rom-director:01F40650');
  state.selectedSceneId = scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  const runtime = state.runtimeByAssetId[scene.assetId];
  assert(runtime, JSON.stringify(state.sourceErrors));
  const program = state.programByAssetId[scene.assetId];
  const binder = program.primitives.find(node => node.opcode === 0xA6);
  assert.deepStrictEqual(binder.rawWords.slice(1), [1, 0], 'scene slot 1 is bound to current-unit member 0');
  const box = program.primitives.find(node => node.opcode === 0xBF && node.rawWords[4] === 1);
  assert.strictEqual(box.rawWords[11], 12, 'the box slot 1 owns uses the Wizard portrait');

  const launch = runtime.nativeLaunchInputs.externalProducers.value.directorLaunch;
  assert.deepStrictEqual(launch.previewEncounter, { classes: [12], leaderOnly: false, evidence: 'dialogue-portrait-class' },
    'the encountered unit is inferred from the box the bound slot owns');
  assert.strictEqual(launch.previewClasses[0], 81, 'the sample party itself is unchanged');

  let slotOne = null;
  for (const snapshot of runtime.states) {
    slotOne = (snapshot.actors || []).find(actor => actor.slot === 1);
    if (slotOne) break;
  }
  assert(slotOne, 'slot 1 is materialized');
  assert.strictEqual(slotOne.bank, 12, 'the bound Actor is drawn with the Wizard art');
  const banks = {};
  for (const snapshot of runtime.states) for (const actor of (snapshot.actors || [])) if (!(actor.slot in banks)) banks[actor.slot] = actor.bank;
  assert.deepStrictEqual(banks, { 0: 81, 1: 12, 2: 2, 3: 4, 4: 5, 5: 13, 15: 4 },
    "the player's sample unit, the inferred enemy leader, and the scripted NPC are all present");
  const document = state.histories[scene.storageId].present;
  const actor = document.actors.find(row => row.slot === 1);
  assert(actor && /Wizard/.test(actor.label), 'the card names the Actor by its scripted class: ' + (actor && actor.label));
  console.log('cutscene scripted speaker class: ok (' + scene.assetId + ', slot 1 → class 12)');
})().catch(error => { console.error(error); process.exit(1); });
