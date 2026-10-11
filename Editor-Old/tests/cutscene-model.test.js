'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
vm.runInThisContext(fs.readFileSync(path.join(EDITOR, 'cutscene-model.js'), 'utf8'), {
  filename: 'cutscene-model.js',
});

const M = OB64.cutsceneModel;

function actor(id, slot) {
  return M.createActor({
    id,
    label: id,
    slot,
    artSourceId: 'art:30:1',
    capability: M.capabilities.NATIVE,
  });
}

function populatedDocument() {
  const document = M.createSceneDocument({
    identity: {
      sceneId: 'scene:graduation',
      technicalName: 'Director 01F3E836',
      friendlyName: 'Graduation Ceremony',
      engine: 'director',
      sourceRevision: 'us-rev0',
      directorKey: 'director:01F3E836',
      aliases: ['graduation-ceremony'],
      triggerStatus: 'reviewed',
    },
    background: {
      assetId: 'background:graduation',
      capability: M.capabilities.NATIVE,
      layers: [{
        id: 'background:base',
        assetId: 'bg2:graduation-base',
        label: 'Room',
        visible: true,
        depth: 0,
        capability: M.capabilities.NATIVE,
        source: { resourceKey: 'bg2:graduation-base' },
      }],
      projection: { mode: 'scene-profile', profileId: 'projection:graduation' },
    },
    branches: [
      { id: 'default', label: 'Default', condition: null },
      { id: 'alternate', label: 'Alternate', condition: { kind: 'unknown-gate' } },
    ],
    native: {
      sourceAssetId: 'asset:01F3E836',
      commands: [{
        id: 'native:unknown:12',
        boundaryId: 'boundary:12',
        words: [0xDEADBEEF, 0x00000000],
        kind: 'preserved-unknown',
        source: { wordStart: 12 },
      }],
      gaps: [{ id: 'gap:1', reason: 'unresolved command family' }],
    },
    exportRequirements: {
      capability: M.capabilities.NATIVE,
      reasons: [],
      allocationBytes: 0,
      features: ['director-fixed-slot'],
    },
  });

  M.addActor(document, actor('actor:magnus', 0));
  M.addTrack(document, M.createTrack({
    id: 'track:magnus:pose',
    type: 'pose',
    actorId: 'actor:magnus',
    label: 'Magnus pose',
  }));
  M.addClip(document, 'track:magnus:pose', M.createClip({
    id: 'clip:magnus:stand',
    kind: 'pose',
    startFrame: 0,
    durationFrames: 6,
    capability: M.capabilities.NATIVE,
    payload: { poseId: 'pose:stand', facing: 'south', loop: true },
    source: { eventId: 'event:pose:1' },
  }));
  return document;
}

const empty = M.createSceneDocument();
assert.strictEqual(empty.format, 'ob64-cutscene-scene');
assert.strictEqual(empty.schemaVersion, 1);
assert.strictEqual(M.previewFps, 30);
assert.deepStrictEqual(Object.values(M.capabilities), [
  'native', 'converted', 'preview-only', 'needs-research',
]);
assert.deepStrictEqual(empty.branches, [
  { id: 'default', label: 'Default', condition: null },
]);

const document = populatedDocument();
assert.strictEqual(M.validateSceneDocument(document), document);
assert.strictEqual(document.actors.length, 1);
assert.strictEqual(document.tracks.length, 1);
assert.strictEqual(document.tracks[0].clips.length, 1);
assert.deepStrictEqual(document.native.commands[0].words, [0xDEADBEEF, 0]);

const serialized = M.serializeSceneDocument(document);
const reparsed = M.parseSceneDocument(serialized);
assert.deepStrictEqual(reparsed, document);
assert.strictEqual(M.serializeSceneDocument(reparsed), serialized);

const reordered = JSON.parse(serialized);
reordered.identity = {
  triggerStatus: reordered.identity.triggerStatus,
  aliases: reordered.identity.aliases,
  directorKey: reordered.identity.directorKey,
  sourceRevision: reordered.identity.sourceRevision,
  engine: reordered.identity.engine,
  friendlyName: reordered.identity.friendlyName,
  technicalName: reordered.identity.technicalName,
  sceneId: reordered.identity.sceneId,
};
assert.strictEqual(M.serializeSceneDocument(M.parseSceneDocument(reordered)), serialized,
  'serialization must be deterministic regardless of object insertion order');

assert.throws(() => M.parseSceneDocument(Object.assign({}, document, { extra: true })),
  /unsupported field/);
assert.throws(() => M.createSceneDocument({ schemaVersion: 2 }), /unsupported schema version/);

const invalidPath = M.cloneSceneDocument(document);
invalidPath.tracks[0].clips[0].pathIds = ['missing'];
assert.throws(() => M.validateSceneDocument(invalidPath), /unknown preview path/);

const invalidTrackKind = M.cloneSceneDocument(document);
invalidTrackKind.tracks[0].clips[0].kind = 'movement';
assert.throws(() => M.validateSceneDocument(invalidTrackKind), /incompatible with pose track/);

const duplicateSlot = M.cloneSceneDocument(document);
duplicateSlot.actors.push(actor('actor:duplicate', 0));
assert.throws(() => M.validateSceneDocument(duplicateSlot), /duplicates native slot/);

const history = M.createHistory(document, 3);
M.execute(history, 'Add movement track', next => {
  M.addTrack(next, M.createTrack({
    id: 'track:magnus:movement',
    type: 'movement',
    actorId: 'actor:magnus',
    label: 'Magnus movement',
  }));
});
assert.strictEqual(history.present.tracks.length, 2);
assert.strictEqual(history.revision, 1);
assert.strictEqual(M.undo(history), true);
assert.strictEqual(history.present.tracks.length, 1);
assert.strictEqual(M.redo(history), true);
assert.strictEqual(history.present.tracks.length, 2);

const beforeFailedCommand = M.serializeSceneDocument(history.present);
assert.throws(() => M.execute(history, 'Invalid fractional clip', next => {
  M.addClip(next, 'track:magnus:movement', M.createClip({
    id: 'clip:bad',
    kind: 'movement',
    startFrame: 1.5,
  }));
}), /must be an integer/);
assert.strictEqual(M.serializeSceneDocument(history.present), beforeFailedCommand,
  'a failed command must not mutate the present document');

M.execute(history, 'Add valid movement', next => {
  M.addClip(next, 'track:magnus:movement', M.createClip({
    id: 'clip:magnus:walk',
    kind: 'movement',
    startFrame: 0,
    durationFrames: 30,
    capability: M.capabilities.NATIVE,
    payload: { from: { x: 0, y: 0, z: 0 }, to: { x: 30, y: 0, z: 0 } },
  }));
});
assert.strictEqual(history.present.tracks[1].clips.length, 1);

const removable = M.cloneSceneDocument(history.present);
assert.strictEqual(M.removeActor(removable, 'actor:magnus'), true);
assert.strictEqual(removable.actors.length, 0);
assert.strictEqual(removable.tracks.length, 0,
  'removing an actor must remove its actor-owned tracks');
assert.strictEqual(M.removeActor(removable, 'actor:missing'), false);

const sourceSnapshot = document.native.commands[0].words.slice();
const cloned = M.cloneSceneDocument(document);
cloned.native.commands[0].words[0] = 0;
assert.deepStrictEqual(document.native.commands[0].words, sourceSnapshot,
  'clones must not share preserved command storage');

console.log('PASS Cutscene Studio SceneDocument schema, preservation, history, and deterministic Project serialization');
