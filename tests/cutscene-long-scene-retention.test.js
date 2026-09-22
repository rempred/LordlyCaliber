'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');

new Function('require', '__dirname', fs.readFileSync(path.join(__dirname,
  'cutscene-authoring-workflow.test.js'), 'utf8').split('const raw =')[0])(require, __dirname);

(async () => {
  const name = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';
  const editor = path.join(__dirname, '..');
  const master = [process.env.OB64_MASTER_ROM, path.join(editor, '..', name),
    path.join(editor, '..', '..', 'OgreBattlel64', name)]
    .filter(Boolean).find(file => fs.existsSync(file));
  assert(master, 'the US Rev 0 master ROM is required');
  const raw = fs.readFileSync(master);
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, archives: OB64.findArchives(z64), layout: { id: 'us-rev0' } };
  const ui = OB64.cutsceneUI.initialize(rom);
  const scene = ui.catalog.getScene('rom-director:01F58B0A');
  const prepare = OB64.cutsceneSharedActor.prototype.prepare;
  let matrixPreparations = 0;
  OB64.cutsceneSharedActor.prototype.prepare = function(records, cameras, channels) {
    matrixPreparations += records.length;
    return prepare.call(this, records, cameras, channels);
  };
  const started = performance.now();
  const document = await OB64.cutsceneUI.loadScene(rom, ui, scene);
  const preparationMs = performance.now() - started;
  const run = ui.runtimeByAssetId[scene.assetId];

  assert(run, JSON.stringify(ui.sourceErrors));
  assert.equal(run.outcome, 'modeled-termination');
  assert.deepStrictEqual(run.missingInputs, []);
  assert(run.states.length > 4109, 'the full room continuation must pass the old storage stop');
  assert(run.retainedStateBytes < run.limits.maxStateBytes);
  assert(matrixPreparations < 1000, 'unchanged native matrix inputs must reuse the preceding pass');

  const tick = 4000;
  const preview = OB64.cutsceneRuntime.evaluate(run, tick);
  assert.deepStrictEqual(preview.effects, JSON.parse(JSON.stringify(run.states[tick].effects)));
  const effect = preview.effects.find(row => row.payload.sourceSystem === 'cutscene-sprite-native');
  assert(effect, 'the late scene must retain a native sprite effect');
  const original = effect.payload.stageX;
  assert(Number.isFinite(original));
  effect.payload.stageX = 9999;
  assert.equal(OB64.cutsceneRuntime.evaluate(run, tick).effects.find(row => row.slot === effect.slot).payload.stageX, original);

  const frame = await OB64.cutsceneUI.capturePreviewFrame(rom, ui, document, {
    preview: OB64.cutsceneRuntime.evaluate(run, tick),
    pass: tick, nodeId: 'long-scene-retention', backgroundPolicy: 'require'
  });
  assert(frame.rgba.some((value, index) => index % 4 !== 3 && value));
  console.log(JSON.stringify({ status: 'pass', scene: scene.assetId,
    ticks: run.states.length, retainedBytes: run.retainedStateBytes, lateSeek: tick,
    matrixPreparations, preparationMs: Math.round(preparationMs), rendered: true }));
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
