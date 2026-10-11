'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const ROOT = path.resolve(__dirname, '..');
global.window = global; global.OB64 = {};
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
const preferences = new Map();
global.localStorage = { getItem: key => preferences.has(key) ? preferences.get(key) : null,
  setItem: (key, value) => preferences.set(key, value) };
for (const file of ['art.js', 'sprite-library.js', 'sprite-editor-ui.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(ROOT, file), 'utf8'), { filename: file });
}
const L = OB64.spriteLibrary, U = OB64.spriteEditorUI, A = OB64.art;
const rom = {}; const state = L.initialize(rom);
let asset = L.addAsset(state, L.assetFromRgba(state, { width: 3, height: 2,
  rgba: new Uint8ClampedArray([1, 2, 3, 255, 9, 8, 7, 128, 30, 20, 10, 255,
    4, 5, 6, 255, 0, 0, 0, 0, 7, 8, 9, 17]), anchor: { x: 1, y: 2 }, ticks: 0 }));
const original = new Uint8ClampedArray(asset.frames[0].layers[0].pixels);
L.setLayerPosition(state, asset.id, 0, 0, -2, 1);
assert.deepStrictEqual(asset.frames[0].layers[0].pixels, original, 'movement preserves all stored pixels');
assert.deepStrictEqual([...L.compositeFrame(asset, 0).slice(12, 16)], [30, 20, 10, 255]);
assert.deepStrictEqual(U.compositeWithLayer(asset, 0, 0, original), L.compositeFrame(asset, 0));
const project = L.collectProjectPayload(rom);
assert.strictEqual(project.schemaVersion, 2);
const imported = L.prepareProjectPayload(JSON.parse(JSON.stringify(project)));
assert.deepStrictEqual(imported.assets[0].anchor, { x: 1, y: 2 });
assert.deepStrictEqual(imported.assets[0].frames[0].layers[0].pixels, original);
assert.strictEqual(imported.assets[0].frames[0].ticks, 0, 'native zero duration persists');
const file = L.prepareAssetFile(L.assetFileText(asset));
assert.deepStrictEqual(file.anchor, asset.anchor);
assert.strictEqual(file.frames[0].layers[0].x, -2);
L.setLayerPosition(state, asset.id, 0, 0, 0, 0);
assert.deepStrictEqual(L.compositeFrame(asset, 0), original, 'moving back recovers hidden pixels');
L.undo(state, asset.id); asset = L.assetFor(state, asset.id);
assert.strictEqual(asset.frames[0].layers[0].x, -2);
L.redo(state, asset.id); asset = L.assetFor(state, asset.id);
assert.strictEqual(asset.frames[0].layers[0].x, 0);
const legacy = JSON.parse(JSON.stringify(project)); legacy.schemaVersion = 1;
delete legacy.assets[0].anchor;
for (const frame of legacy.assets[0].frames) for (const layer of frame.layers) {
  delete layer.width; delete layer.height; delete layer.x; delete layer.y;
}
const migrated = L.prepareProjectPayload(legacy).assets[0];
assert.deepStrictEqual(migrated.anchor, { x: 0, y: 0 });
assert.strictEqual(migrated.frames[0].layers[0].width, 3);
assert.strictEqual(migrated.frames[0].layers[0].height, 2);
assert.deepStrictEqual(L.prepareAssetFile({ format: L.FILE_FORMAT, version: 1, asset: legacy.assets[0] }).frames[0].layers[0].pixels, original);
for (const version of [0, 3, '2', null]) assert.throws(() => L.prepareProjectPayload({ ...project, schemaVersion: version }));
const snapshot = L.assetFileText(asset), history = L.historyFor(state, asset.id).undo.length;
assert.throws(() => L.setLayerPosition(state, asset.id, 0, 0, 32768, 0));
assert.throws(() => L.setAnchor(state, asset.id, 0, Infinity));
assert.throws(() => L.replaceLayerImage(state, asset.id, 0, 0, original, 4097, 2));
assert.strictEqual(L.assetFileText(asset), snapshot, 'invalid geometry never partly mutates the asset');
assert.strictEqual(L.historyFor(state, asset.id).undo.length, history);
const oversized = JSON.parse(JSON.stringify(project));
oversized.assets[0].frames[0].layers[0].width = 4097;
assert.throws(() => L.prepareProjectPayload(oversized), /width/);
const badAnchor = JSON.parse(JSON.stringify(project)); badAnchor.assets[0].anchor = { x: 1.5, y: 0 };
assert.throws(() => L.prepareProjectPayload(badAnchor), /anchor/);
L.replaceLayerImage(state, asset.id, 0, 0, new Uint8ClampedArray([10, 20, 30, 255, 40, 50, 60, 255,
  70, 80, 90, 255, 100, 110, 120, 255]), 4, 1);
L.setLayerPosition(state, asset.id, 0, 0, -1, 0);
const layerSource = U.selectedAssetSource(asset, 0, 0, true);
assert.strictEqual(layerSource.width, 4);
assert.deepStrictEqual(layerSource.anchor, { x: 2, y: 2 });
const part = L.copyPart(state, asset.id, 'sprite', 0, 0);
assert.deepStrictEqual(part.anchor, layerSource.anchor);
assert.strictEqual(part.width, 4);
assert.deepStrictEqual(part.frames[0].layers[0].pixels, layerSource.rgba);
L.rotateAsset(state, asset.id, true); L.rotateAsset(state, asset.id, false);
assert.strictEqual(asset.frames[0].layers[0].x, -1);
assert.deepStrictEqual(asset.frames[0].layers[0].pixels, layerSource.rgba);
L.cropLayerToCanvas(state, asset.id, 0, 0);
assert.strictEqual(asset.frames[0].layers[0].width, 3);
assert.strictEqual(asset.frames[0].layers[0].pixels.length, 24);
assert.deepStrictEqual([...asset.frames[0].layers[0].pixels.slice(0, 4)], [40, 50, 60, 255]);
L.undo(state, asset.id); asset = L.assetFor(state, asset.id);
assert.strictEqual(asset.frames[0].layers[0].width, 4, 'explicit crop is undoable');
L.addFrame(state, asset.id, 0, true);
L.addLayer(state, asset.id, 1);
const layerColors = L.usedColors(asset, 'layer', 0, 0);
assert.strictEqual(layerColors.length, 4);
assert.strictEqual(L.usedColors(asset, 'sequence', 0, 0).length, 5);
assert.strictEqual(L.usedColors(asset, 'frame', 1, 0).length, 5);
const beforeOversizedResize = L.assetFileText(asset);
assert.throws(() => L.resizeAsset(state, asset.id, 4096, 2), /Resized layer width/);
assert.strictEqual(L.assetFileText(asset), beforeOversizedResize, 'resizing validates layer dimensions before allocation or adoption');
const source = new Uint8ClampedArray([3, 5, 7, 255, 11, 13, 17, 128, 19, 23, 29, 0, 31, 37, 41, 255]);
const originalImport = A.prepareSpriteImageImport(source, 4, 1, 4, 1,
  { placementMode: 'original', preserveRgba: true });
assert.deepStrictEqual(originalImport.rgba, source, 'Original Size preserves exact RGBA including invisible RGB');
const fitted = A.prepareSpriteImageImport(source, 4, 1, 4, 4,
  { placementMode: 'fit', preserveRgba: true });
assert.strictEqual(fitted.crop.width, 4); assert.strictEqual(fitted.crop.height, 4);
assert.strictEqual(fitted.rgba[3], 0, 'Fit pads instead of stretching');
const stretched = A.prepareSpriteImageImport(source, 4, 1, 4, 4,
  { placementMode: 'stretch', preserveRgba: true });
assert.strictEqual(stretched.crop.width, 4); assert.strictEqual(stretched.crop.height, 1);
const filled = A.prepareSpriteImageImport(source, 4, 1, 2, 2,
  { placementMode: 'fill', preserveRgba: true });
assert.strictEqual(filled.crop.width, 1); assert.strictEqual(filled.crop.height, 1);
assert.throws(() => A.prepareSpriteImageImport(source, 4, 1, 4, 1, { placementMode: 'invalid' }));
const alphaRamp = new Uint8ClampedArray(256 * 4);
for (let i = 0; i < 256; i++) alphaRamp.set([255, 0, 0, i], i * 4);
const native = A.prepareAnimationFrameImport(alphaRamp, 256, 1, 256, 1,
  { placementMode: 'original', anchor: { x: 12, y: -3 } });
assert.deepStrictEqual([...native.intensity], Array.from({ length: 256 }, (_, i) => Math.round(i * 15 / 255)));
assert.strictEqual(new Set(native.intensity).size, 16);
assert.deepStrictEqual(native.alignmentAnchor, { x: 12, y: -3 });
assert.throws(() => A.prepareAnimationFrameImport(source, 4, 1, 4, 1, { anchor: { x: Infinity, y: 0 } }));
L.keepEyedropper(true); assert.strictEqual(L.keepEyedropper(), true);
assert.strictEqual(preferences.get('ob64.keepEyedropper'), 'true');
L.keepEyedropper(false); assert.strictEqual(L.keepEyedropper(), false);
// Execute the real pointer handlers with a deterministic canvas substitute.
const context = new Proxy({ createImageData: (w, h) => ({ data: new Uint8ClampedArray(w * h * 4) }) },
  { get: (object, key) => key in object ? object[key] : () => {} });
global.document = { createElement: () => ({ getContext: () => context }) };
function canvas() {
  const handlers = {}, captured = new Set();
  return { width: 30, height: 20, style: {},
    getContext: () => context, setAttribute() {}, focus() {},
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 30, height: 20 }),
    addEventListener: (type, fn) => { assert(!handlers[type], 'one handler per event'); handlers[type] = fn; },
    setPointerCapture: id => captured.add(id), hasPointerCapture: id => captured.has(id), releasePointerCapture: id => captured.delete(id),
    emit(type, x, y) { handlers[type]({ type, clientX: x, clientY: y, pointerId: 1, preventDefault() {} }); },
    captured };
}
L.setLayerPosition(state, asset.id, 0, 0, 0, 0);
const pointerUi = { frameIndex: 0, layerIndex: 0, tool: 'move', zoom: 10, background: 'checkerboard', showGrid: false };
let renders = 0; const surface = canvas();
U.installCanvasEditing(surface, state, asset, pointerUi, {}, () => renders++);
const pixelsBeforeMove = new Uint8ClampedArray(asset.frames[0].layers[0].pixels);
surface.emit('pointerdown', 5, 5); surface.emit('pointermove', -35, 25); surface.emit('pointerup', -35, 25);
assert.strictEqual(asset.frames[0].layers[0].x, -4, 'drag continues outside the canvas');
assert.strictEqual(asset.frames[0].layers[0].y, 2);
assert.deepStrictEqual(asset.frames[0].layers[0].pixels, pixelsBeforeMove);
assert.strictEqual(surface.captured.size, 0); assert.strictEqual(renders, 1);
const canceled = canvas();
U.installCanvasEditing(canceled, state, asset, pointerUi, {}, () => renders++);
canceled.emit('pointerdown', 5, 5); canceled.emit('pointermove', 25, 15); canceled.emit('pointercancel', 25, 15);
assert.strictEqual(asset.frames[0].layers[0].x, -4, 'cancel restores the pre-drag position');
assert.strictEqual(canceled.captured.size, 0);
L.setLayerPosition(state, asset.id, 0, 0, 0, 0);
L.keepEyedropper(true); pointerUi.tool = 'eyedropper';
const sampling = canvas(); U.installCanvasEditing(sampling, state, asset, pointerUi, {}, () => {});
sampling.emit('pointerdown', 5, 5); assert.strictEqual(pointerUi.tool, 'eyedropper');
assert.deepStrictEqual(pointerUi.color, [10, 20, 30, 255]);
L.keepEyedropper(false); sampling.emit('pointerdown', 5, 5); assert.strictEqual(pointerUi.tool, 'pencil');
console.log('PASS asset v1 migration/v2 persistence, anchors, hidden pixels, crop, transforms, budgets, sizing, opacity, used colors, and pointer lifecycle');
