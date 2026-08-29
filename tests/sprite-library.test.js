'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
vm.runInThisContext(
  fs.readFileSync(path.join(EDITOR, 'sprite-library.js'), 'utf8'),
  { filename: 'sprite-library.js' }
);

const L = OB64.spriteLibrary;
assert.strictEqual(L.MAX_DIMENSION, 4096);
assert.deepStrictEqual(L.knownSpriteFormats().map(format => [
  format.id, format.width, format.height,
]), [
  ['class-avatar', 40, 48],
  ['item-icon', 16, 16],
  ['army-formation-small', 16, 24],
  ['army-formation-large', 32, 28],
]);
const state = L.createState();
let first = L.blankAsset(state, {
  name: 'Test Sprite', width: 2, height: 3,
});
first = L.addAsset(state, first);

assert.strictEqual(state.assets.length, 1);
assert.strictEqual(first.kind, 'sprite');
assert.strictEqual(first.frames.length, 1);
assert.strictEqual(first.frames[0].layers.length, 1);
assert.strictEqual(first.frames[0].layers[0].pixels.length, 24);

const edited = new Uint8ClampedArray(24);
edited.set([255, 0, 0, 255], 0);
edited.set([0, 0, 255, 128], 4);
L.replaceLayerPixels(state, first.id, 0, 0, edited);
assert.deepStrictEqual(
  [...L.compositeFrame(first, 0).slice(0, 8)],
  [255, 0, 0, 255, 0, 0, 255, 128]
);

assert.strictEqual(L.undo(state, first.id), true);
first = L.assetFor(state, first.id);
assert.deepStrictEqual([...first.frames[0].layers[0].pixels], new Array(24).fill(0));
assert.strictEqual(L.redo(state, first.id), true);
first = L.assetFor(state, first.id);
assert.deepStrictEqual([...first.frames[0].layers[0].pixels.slice(0, 8)],
  [255, 0, 0, 255, 0, 0, 255, 128]);

L.addLayer(state, first.id, 0);
assert.strictEqual(first.kind, 'frame');
assert.strictEqual(first.frames[0].layers.length, 2);
const overlay = new Uint8ClampedArray(24);
overlay.set([0, 255, 0, 128], 0);
L.replaceLayerPixels(state, first.id, 0, 1, overlay);
const composite = L.compositeFrame(first, 0);
assert.strictEqual(composite[3], 255);
assert(composite[0] >= 126 && composite[0] <= 128);
assert(composite[1] >= 127 && composite[1] <= 129);

L.addFrame(state, first.id, 0, true);
assert.strictEqual(first.kind, 'sequence');
assert.strictEqual(first.frames.length, 2);
L.setFrameTicks(state, first.id, 1, 13);
assert.strictEqual(first.frames[1].ticks, 13);
L.moveFrame(state, first.id, 1, 0);
assert.strictEqual(first.frames[0].ticks, 13);

const beforeRotate = new Uint8ClampedArray(first.frames[0].layers[0].pixels);
L.rotateAsset(state, first.id, true);
assert.strictEqual(first.width, 3);
assert.strictEqual(first.height, 2);
assert.strictEqual(first.frames[0].layers[0].pixels.length, beforeRotate.length);
L.resizeAsset(state, first.id, 4, 4);
assert.strictEqual(first.width, 4);
assert.strictEqual(first.height, 4);

const derivedSprite = L.copyPart(state, first.id, 'sprite', 0, 0);
assert.strictEqual(derivedSprite.kind, 'sprite');
assert.strictEqual(derivedSprite.frames.length, 1);
assert.strictEqual(derivedSprite.frames[0].layers.length, 1);
const derivedFrame = L.copyPart(state, first.id, 'frame', 0, 0);
assert.strictEqual(derivedFrame.kind, 'frame');
assert.strictEqual(derivedFrame.frames.length, 1);
const derivedSequence = L.copyPart(state, first.id, 'sequence', 0, 0);
assert.strictEqual(derivedSequence.kind, 'sequence');
assert.strictEqual(derivedSequence.frames.length, 2);

const rom = { spriteLibrary: state };
const payload = L.collectProjectPayload(rom);
assert.strictEqual(payload.schemaVersion, 1);
assert.strictEqual(payload.assets.length, 4);
assert.strictEqual(typeof payload.assets[0].frames[0].layers[0].pixelsRgbaBase64,
  'string');

const prepared = L.prepareProjectPayload(payload);
assert.strictEqual(prepared.count, 4);
const restoredRom = {};
assert.strictEqual(L.applyPreparedProjectPayload(restoredRom, prepared), 4);
assert.strictEqual(restoredRom.spriteLibrary.assets.length, 4);
assert.deepStrictEqual(
  [...restoredRom.spriteLibrary.assets[0].frames[0].layers[0].pixels],
  [...state.assets[0].frames[0].layers[0].pixels]
);

const fileText = L.assetFileText(derivedSequence);
const fileAsset = L.prepareAssetFile(fileText);
assert.strictEqual(fileAsset.kind, 'sequence');
assert.strictEqual(fileAsset.frames.length, 2);
assert(/\.ob64-sprite\.json$/.test(L.filename(fileAsset)));
const wrongEncodedLength = JSON.parse(fileText);
wrongEncodedLength.asset.frames[0].layers[0].pixelsRgbaBase64 += 'AAAA';
assert.throws(() => L.prepareAssetFile(wrongEncodedLength),
  /invalid encoded RGBA byte length/);

assert.throws(() => L.prepareProjectPayload({
  schemaVersion: 1,
  assets: [{
    id: 'too-wide', name: 'Too Wide', kind: 'sprite', width: 4097, height: 1,
    frames: [], provenance: {},
  }],
}), /width must be from 1 through 4096/);

const reportedPngWidth = 1159;
const reportedPngHeight = 1356;
const reportedPngAsset = L.assetFromRgba(state, {
  name: 'Reported PNG',
  width: reportedPngWidth,
  height: reportedPngHeight,
  rgba: new Uint8ClampedArray(reportedPngWidth * reportedPngHeight * 4),
  provenance: { source: 'image-file', format: 'PNG' },
});
assert.strictEqual(reportedPngAsset.width, reportedPngWidth);
assert.strictEqual(reportedPngAsset.height, reportedPngHeight);
const importedReportedPngAsset = L.addAsset(state, reportedPngAsset);
assert.strictEqual(importedReportedPngAsset.width, reportedPngWidth);
assert.strictEqual(importedReportedPngAsset.height, reportedPngHeight);
assert.throws(() => L.prepareAssetFile('{bad json'), /not valid JSON/);
const oversizedFrame = {
  id: 'frame', name: 'Frame', ticks: 1,
  layers: Array.from({ length: 64 }, (_, index) => ({
    id: 'layer-' + index, name: 'Layer', pixelsRgbaBase64: '',
  })),
};
assert.throws(() => L.prepareProjectPayload({
  schemaVersion: 1,
  assets: [{
    id: 'oversized', name: 'Oversized', kind: 'sequence',
    width: 512, height: 512,
    frames: Array.from({ length: 5 }, (_, index) => Object.assign({},
      oversizedFrame, { id: 'frame-' + index })),
  }],
}), /pixel Project limit/);

console.log('PASS Sprite Library editing, transforms, Project round-trip, and asset files');
