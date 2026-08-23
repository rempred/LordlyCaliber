'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of ['cutscene-model.js', 'cutscene-preview.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, filename), 'utf8'), {
    filename,
  });
}

const M = OB64.cutsceneModel;
const P = OB64.cutscenePreview;

const document = M.createSceneDocument({
  identity: {
    sceneId: 'scene:timing-fixture',
    technicalName: 'Timing fixture',
    friendlyName: null,
    engine: 'director',
    sourceRevision: 'us-rev0',
    directorKey: 'director:fixture',
    aliases: [],
    triggerStatus: 'fixture',
  },
  branches: [
    { id: 'default', label: 'Default', condition: null },
    { id: 'alternate', label: 'Alternate', condition: { kind: 'fixture' } },
  ],
});

M.addActor(document, M.createActor({
  id: 'actor:one',
  label: 'Actor One',
  slot: 0,
  artSourceId: 'art:one',
  capability: M.capabilities.NATIVE,
  initial: { visible: false, x: 0, y: 5, z: 10, facing: 'south', poseId: 'pose:idle' },
}));

function track(id, type, actorId, clips) {
  M.addTrack(document, M.createTrack({ id, type, actorId, label: id, clips }));
}

track('track:actor', 'actor', 'actor:one', [
  M.createClip({ id: 'clip:enter', kind: 'enter', startFrame: 0, capability: 'native' }),
  M.createClip({ id: 'clip:exit', kind: 'exit', startFrame: 60, capability: 'native' }),
]);
track('track:pose', 'pose', 'actor:one', [
  M.createClip({
    id: 'clip:pose', kind: 'pose', startFrame: 0, durationFrames: 6,
    capability: 'native', payload: { poseId: 'pose:walk', facing: 'east', loop: true },
  }),
]);
track('track:movement', 'movement', 'actor:one', [
  M.createClip({
    id: 'clip:move', kind: 'movement', startFrame: 0, durationFrames: 30,
    capability: 'native',
    payload: { from: { x: 0, y: 5, z: 10 }, to: { x: 30, y: 5, z: 20 } },
  }),
]);
track('track:dialogue', 'dialogue', null, [
  M.createClip({
    id: 'clip:dialogue', kind: 'dialogue', startFrame: 10, durationFrames: 20,
    pathIds: ['default'], capability: 'native', payload: { text: 'Hello.' },
  }),
]);
track('track:effect', 'effect', null, [
  M.createClip({
    id: 'clip:effect', kind: 'effect', startFrame: 5, durationFrames: 3,
    pathIds: ['alternate'], capability: 'preview-only', payload: { effectId: 'effect:spark' },
  }),
]);

assert.strictEqual(P.sceneDurationFrames(document, 'default'), 61,
  'zero-duration exit at frame 60 must keep frame 60 reachable');

let state = P.evaluateAtFrame(document, 0, { pathId: 'default' });
assert.strictEqual(state.frame, 0);
assert.strictEqual(state.actors[0].visible, true);
assert.strictEqual(state.actors[0].poseId, 'pose:walk');
assert.strictEqual(state.actors[0].facing, 'east');
assert.strictEqual(state.actors[0].x, 0);

state = P.evaluateAtFrame(document, 15, { pathId: 'default' });
assert.strictEqual(state.actors[0].x, 15);
assert.strictEqual(state.actors[0].y, 5);
assert.strictEqual(state.actors[0].z, 15);
assert.strictEqual(state.actors[0].poseFrame, 3);
assert.strictEqual(state.actors[0].activeMovementId, 'clip:move');
assert.strictEqual(state.dialogue.length, 1);
assert.strictEqual(state.effects.length, 0);

state = P.evaluateAtFrame(document, 30, { pathId: 'default' });
assert.strictEqual(state.actors[0].x, 30,
  'movement endpoint must persist after the movement clip');
assert.strictEqual(state.actors[0].activeMovementId, null);
assert.strictEqual(state.dialogue.length, 0,
  'clip end is exclusive');

state = P.evaluateAtFrame(document, 6, { pathId: 'alternate' });
assert.strictEqual(state.dialogue.length, 0);
assert.strictEqual(state.effects.length, 1);
assert.strictEqual(state.effects[0].capability, 'preview-only');

state = P.evaluateAtFrame(document, 60, { pathId: 'default' });
assert.strictEqual(state.actors[0].visible, false);
assert.strictEqual(state.timeSeconds, 2);

assert.throws(() => P.evaluateAtFrame(document, 0, { pathId: 'missing' }),
  /Unknown preview path/);
assert.throws(() => P.evaluateAtFrame(document, 0.5), /integer/);

let clock = P.createClock(120, { playing: true });
clock = P.advance(clock, 1000);
assert.strictEqual(clock.frame, 30,
  'one second must advance exactly thirty preview frames');
assert.strictEqual(clock.playing, true);

clock = P.advance(clock, 3000);
assert.strictEqual(clock.frame, 119);
assert.strictEqual(clock.playing, false,
  'non-looping playback must stop at the final frame');

clock = P.setLoop(P.createClock(60, { frame: 59, playing: true }), true);
clock = P.advance(clock, 1000 / 30);
assert.strictEqual(clock.frame, 0);
assert.strictEqual(clock.playing, true);
clock = P.step(clock, -1);
assert.strictEqual(clock.frame, 59);
clock = P.seek(clock, 12);
assert.strictEqual(clock.frame, 12);
clock = P.pause(clock);
assert.strictEqual(clock.playing, false);

assert.strictEqual(P.snapFrame(16, 'frame'), 16);
assert.strictEqual(P.snapFrame(16, 'tenth'), 15);
assert.strictEqual(P.snapFrame(16, 'half'), 15);
assert.strictEqual(P.snapFrame(46, 'second'), 60);
assert.strictEqual(P.formatTime(0), '0:00.00');
assert.strictEqual(P.formatTime(45), '0:01.15');
assert.strictEqual(P.formatTime(1815), '1:00.15');

console.log('PASS Cutscene Studio deterministic 30-frame preview, paths, pose, movement, and transport');
