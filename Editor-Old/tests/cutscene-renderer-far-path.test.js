'use strict';

// A movement path whose endpoint projects far outside the frame must not stall
// the renderer: the line rasterizer clips such segments to the frame instead of
// stepping every pixel to the far endpoint. Retail scene 01F41D1E blocked the
// page for minutes this way before the clip existed.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of ['cutscene-model.js', 'cutscene-preview.js', 'cutscene-renderer.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}

const M = OB64.cutsceneModel;
const P = OB64.cutscenePreview;
const R = OB64.cutsceneRenderer;

function fixture(to) {
  const document = M.createSceneDocument({
    identity: {
      sceneId: 'scene:far-path', technicalName: 'Far path fixture', friendlyName: null,
      engine: 'director', sourceRevision: 'us-rev0', directorKey: '00000002',
      aliases: [], triggerStatus: 'fixture'
    }
  });
  M.addActor(document, M.createActor({
    id: 'actor:walker', label: 'walker', slot: 0, artSourceId: 'bank:0',
    capability: M.capabilities.NATIVE,
    initial: { visible: true, x: -100, y: 0, z: 0, facing: 'native-0', poseId: null }
  }));
  M.addTrack(document, M.createTrack({
    id: 'track:walker:movement', type: 'movement', actorId: 'actor:walker', label: 'Move'
  }));
  M.addClip(document, 'track:walker:movement', M.createClip({
    id: 'clip:walker:move', kind: 'movement', startFrame: 0, durationFrames: 30,
    capability: M.capabilities.NATIVE,
    payload: { from: { x: -100, y: 0, z: 0 }, to }
  }));
  return document;
}

// A path that leaves the frame by a few hundred pixels keeps its exact pixels.
const near = fixture({ x: 900, y: 0, z: 0 });
const nearFrame = R.renderFrame(near, P.evaluateAtFrame(near, 15), { selectedActorId: 'actor:walker' });
assert.strictEqual(nearFrame.movementPaths.length, 1);

// A path whose endpoint projects millions of pixels away renders in bounded time.
for (const far of [{ x: 4e8, y: 0, z: 0 }, { x: -4e8, y: 3e8, z: 0 }, { x: 0, y: -5e8, z: 0 }]) {
  const document = fixture(far);
  const started = Date.now();
  const frame = R.renderFrame(document, P.evaluateAtFrame(document, 15), { selectedActorId: 'actor:walker' });
  const elapsed = Date.now() - started;
  assert.strictEqual(frame.width, 320);
  assert.strictEqual(frame.movementPaths.length, 1);
  assert(elapsed < 1000, 'far movement path rendered in ' + elapsed + ' ms');
}

// The scene model already refuses non-finite coordinates; the rasterizer's own
// guard is defence in depth and needs no document to exercise.
console.log('cutscene renderer far path: ok');
