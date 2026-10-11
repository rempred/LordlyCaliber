'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const { installDom } = require('./helpers/editor-dom');
const dom = installDom();
global.window = global;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
global.localStorage = { getItem() { return null; }, setItem() {} };
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['editor-interaction.js', 'art.js', 'army-sprites.js',
  'sprite-library.js', 'sprite-editor-ui.js', 'art-ui.js', 'army-sprite-ui.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(__dirname, '..', file), 'utf8'), { filename: file });
}
const L = OB64.spriteLibrary;
function fixture(kind, prior) {
  const canvas = document.createElement('canvas');
  document.body.appendChild(canvas);
  let changes = 0;
  const options = { onChange() { changes++; } };
  const ui = { tool: 'select', selection: prior, armyTool: 'select', armySelection: prior,
    armyPaletteIndex: 0, selectedAvatarColor: 1, selectedIconColor: 1 };
  let snapshot;
  if (kind === 'sprite') {
    const state = L.createState();
    const asset = L.addAsset(state, L.blankAsset(state, { name: 'Selection', width: 4, height: 4 }));
    Object.assign(ui, { frameIndex: 0, layerIndex: 0, zoom: 4, color: [255, 0, 0, 255],
      brushSize: 1, background: 'checkerboard', showGrid: false });
    snapshot = () => JSON.stringify([L.assetFileText(asset), state.history]);
    OB64.spriteEditorUI.installCanvasEditing(canvas, state, asset, ui, options, () => {});
  } else if (kind === 'army') {
    const model = { key: 'sample:0', modelId: 0, originalIndices: new Uint8Array(16).fill(1) };
    const atlas = { key: 'sample', width: 4, height: 4, targetModelCount: 1, models: [model],
      palettes: [new Uint16Array(256).fill(1)], transparentIndices: [0] };
    const state = { byKey: { sample: atlas }, edits: {}, history: {} };
    snapshot = () => JSON.stringify(state);
    OB64.armySpriteUI.installCanvas(canvas, state, atlas, model, 4, ui, options, () => {});
  } else {
    const state = {
      avatar: { edits: {}, history: {}, byKey: {} },
      icons: { edits: {}, history: {}, byKey: {} },
      blocked: { avatars: {}, icons: {} }
    };
    state[kind === 'avatar' ? 'avatar' : 'icons'].byKey.sample = {
      originalWords: new Uint16Array(16).fill(1)
    };
    snapshot = () => JSON.stringify(state);
    OB64.artUI.installCanvasEditing(canvas, state, kind, 'sample', 4, 4, 4, ui, options, () => {});
  }
  return { canvas, ui, snapshot, changes: () => changes,
    selection: () => ui[kind === 'army' ? 'armySelection' : 'selection'] };
}
for (const kind of ['sprite', 'avatar', 'icon', 'army']) {
  for (const prior of [null, { x: 0, y: 0, width: 1, height: 2 }]) {
    const f = fixture(kind, prior);
    const before = f.snapshot();
    const event = { pointerId: 7, clientX: 4, clientY: 4 };
    f.canvas.dispatch('pointerdown', event);
    f.canvas.dispatch('pointermove', { ...event, clientX: 12, clientY: 12 });
    assert.deepStrictEqual(f.selection(), { x: 1, y: 1, width: 3, height: 3 });
    f.canvas.dispatch('pointercancel', event);
    assert.deepStrictEqual(f.selection(), prior, kind + ' restores prior selection');
    assert.strictEqual(f.canvas.hasPointerCapture(7), false);
    f.canvas.dispatch('pointerup', event);
    f.canvas.dispatch('lostpointercapture', event);
    assert.deepStrictEqual(f.selection(), prior, 'late completion cannot revive cancelled selection');
    assert.strictEqual(f.snapshot(), before, kind + ' pixels and history unchanged');
    assert.strictEqual(f.changes(), 0);
    if (kind === 'sprite') {
      f.canvas.dispatch('keydown', { key: 'c', ctrlKey: true });
      if (prior) assert.deepStrictEqual([f.ui.clipboard.width, f.ui.clipboard.height], [1, 2]);
      else assert(!f.ui.clipboard, 'cancelled empty selection cannot be copied');
    }
    f.canvas.dispatch('pointerdown', event);
    f.canvas.dispatch('pointermove', { ...event, clientX: 8, clientY: 8 });
    f.canvas.dispatch('pointerup', event);
    assert.deepStrictEqual(f.selection(), { x: 1, y: 1, width: 2, height: 2 }, 'normal release retains selection');
    assert.strictEqual(f.canvas.hasPointerCapture(7), false);
    if (kind === 'sprite') {
      f.canvas.dispatch('keydown', { key: 'c', ctrlKey: true });
      assert.deepStrictEqual([f.ui.clipboard.width, f.ui.clipboard.height], [2, 2]);
    }
    assert.strictEqual(f.snapshot(), before);
    assert.strictEqual(f.changes(), 0);
    f.canvas.remove();
  }
  console.log('PASS ' + kind + ': null/prior selection cancellation, late events, normal release, unchanged pixels/history');
}
const panel = document.createElement('main');
document.body.appendChild(panel);
const rom = { spriteLibrary: L.createState() };
OB64.spriteEditorUI.render(panel, rom, {}, false);
assert(panel.textContent.includes('Create or import a Sprite Library Asset'));
L.addAsset(rom.spriteLibrary, L.blankAsset(rom.spriteLibrary, { name: 'Named asset', width: 4, height: 4 }));
OB64.spriteEditorUI.render(panel, rom, {}, false);
assert(panel.querySelector('[aria-label="Sprite Library Asset name"]'));
const animationSource = fs.readFileSync(path.join(__dirname, '..', 'animation-ui.js'), 'utf8');
for (const old of ["selectField('Sequence')", "'Replace Sequence'", "'Import Sequence'", "'Complete sequence'"]) {
  assert(!animationSource.includes(old), 'conflicting label: ' + old);
}
for (const label of ["selectField('Frame Sequence')", "'Replace Frame Sequence'", "'Import Frame Sequence'", "'Complete Frame Sequence'"]) {
  assert(animationSource.includes(label), label);
}
console.log('PASS consistent rendered Sprite Library Asset names and Frame Sequence label regression');
