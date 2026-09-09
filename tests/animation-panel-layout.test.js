'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const PARENT = path.resolve(EDITOR, '..');
const MASTER = path.join(PARENT,
  'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
const MASTER_SHA256 =
  '6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12';

global.window = global;
global.module = undefined;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
if (!global.crypto?.subtle) {
  Object.defineProperty(global, 'crypto', { value: crypto.webcrypto });
}
const preferences = new Map();
global.localStorage = {
  getItem: key => preferences.has(key) ? preferences.get(key) : null,
  setItem: (key, value) => preferences.set(key, value)
};

vm.runInThisContext('var OB64 = window.OB64 = {};');
const moduleFiles = [
  'data.js',
  'parsers.js',
  'editor-interaction.js',
  'art.js',
  'army-sprites.js',
  'sprite-library.js',
  'animation-corpus-data.js',
  'animation-art.js',
  'combat-animation-overrides-data.js',
  'combat-animation-overrides.js',
  'animation-sequences.js',
  'animation-ui.js',
  'sprite-editor-ui.js', 'art-ui.js', 'army-sprite-ui.js'
];
for (const filename of moduleFiles) {
  let source = fs.readFileSync(path.join(EDITOR, filename), 'utf8');
  vm.runInThisContext(source, { filename });
}

const A = OB64.art;
const L = OB64.spriteLibrary;
const M = OB64.animationArt;
const S = OB64.animationSequences;
const U = OB64.animationUI;

const master = fs.readFileSync(MASTER);
const masterHash = crypto.createHash('sha256').update(master)
  .digest('hex').toUpperCase();
assert.strictEqual(masterHash, MASTER_SHA256);
const z64 = new Uint8Array(master);
for (let index = 0; index < z64.length; index += 2) {
  [z64[index], z64[index + 1]] = [z64[index + 1], z64[index]];
}

async function freshRom() {
  const rom = { z64: z64.slice(), layout: { id: 'us-rev0' } };
  await A.initialize(rom);
  rom.classDefs = OB64.parseClassDefs(rom.z64);
  OB64.combatAnimationOverrides.initialize(rom);
  S.initialize(rom);
  return rom;
}

const { installDom } = require('./helpers/editor-dom');
(async () => {
  const rom = await freshRom();
  const dom = installDom();
  const panel = document.createElement('main'); document.body.appendChild(panel);
  let changes = 0;
  const options = { onChange() { changes++; } };
  OB64.artUI.render(panel, rom, options, false);
  rom.art.ui.subtab = 'animations'; rom.art.ui.animationAlignmentOpen = true;
  OB64.artUI.render(panel, rom, options, true);
  const ui = rom.art.ui;
  const byKey = key => panel.querySelector('[data-art-focus-key="' + key + '"]');
  let alignment = panel.querySelector('.animation-alignment-panel');
  assert(alignment);
  assert.deepStrictEqual(alignment.querySelectorAll('h4').map(n => n.textContent),
    ['Position selected Layer', 'Move Layers across Frames', 'Compare Frame Sequences (read-only)']);
  assert(alignment.textContent.includes('Positive X moves right'));
  assert(alignment.textContent.includes('Positive Y moves down'));
  assert(alignment.textContent.includes('No editable private Frame Sequences exist'));
  assert(alignment.textContent.includes('Copy From and Separate…'));
  assert(alignment.textContent.includes('does not move artwork or align it automatically'));
  assert(byKey('animation-layer-x').disabled);
  assert(byKey('animation-move-x').parentNode.parentNode.hidden);
  const before = JSON.stringify(S.collectProject(rom));
  let compare = byKey('animation-compare'); assert(compare.options.length > 1 && !compare.disabled);
  compare.value = compare.options[1].value; compare.dispatch('change');
  assert.strictEqual(JSON.stringify(S.collectProject(rom)), before);
  assert.strictEqual(changes, 0, 'comparison never creates or moves artwork');
  let previews = panel.querySelector('.animation-alignment-previews');
  assert.strictEqual(previews.children.length, 2);
  for (const preview of previews.children) {
    assert.strictEqual(preview.tagName, 'FIGURE');
    assert.strictEqual(preview.children[0].tagName, 'FIGCAPTION');
    assert.strictEqual(preview.children[1].tagName, 'CANVAS');
    assert(preview.querySelector('.editor-transport'));
  }
  assert.strictEqual(previews.children[0].querySelector('canvas').width, previews.children[1].querySelector('canvas').width);
  assert.strictEqual(previews.children[0].querySelector('canvas').height, previews.children[1].querySelector('canvas').height);
  const workbench = panel.querySelector('.animation-frame-workbench');
  assert.strictEqual(workbench.children.length, 3);
  for (const card of workbench.children) {
    assert.strictEqual(card.children[0].tagName, 'FIGCAPTION');
    assert.strictEqual(card.children[1].tagName, 'CANVAS', 'guidance must follow canvas, not displace its top');
  }
  const edit = panel.querySelector('.animation-edit-stage');
  const canvas = edit.querySelector('canvas');
  const dimensions = [canvas.width, canvas.height];
  canvas.focus(); edit.scrollLeft = 9; edit.scrollTop = 13;
  OB64.artUI.render(panel, rom, options, true);
  assert.deepStrictEqual([byKey('animation-edit-canvas').width, byKey('animation-edit-canvas').height], dimensions);
  assert.strictEqual(document.activeElement, byKey('animation-edit-canvas'));
  assert.strictEqual(panel.querySelector('.animation-edit-stage').scrollLeft, 9);
  assert.strictEqual(panel.querySelector('.animation-edit-stage').scrollTop, 13);
  const plays = panel.querySelectorAll('button').filter(n => n.textContent === 'Play');
  assert.strictEqual(plays.length, 1); plays[0].click();
  assert.strictEqual(dom.scheduled.size, 1); dom.tick(0); dom.tick(80);
  OB64.artUI.render(panel, rom, options, true); assert(dom.scheduled.size <= 1);
  OB64.editorInteraction.stopAll(); assert.strictEqual(dom.scheduled.size, 0);
  console.log('PASS read-only prerequisites, contained card structure, shared-origin size, focus/scroll and shared playback');

  // The existing copy workflow unlocks positioning; selecting modes alone does not mutate.
  const donor = OB64.spriteEditorUI.animationSources(rom, 77).find(row => S.laneFor(row) === 'advance' && U.selectorFlags(row) === '1/1'); assert(donor);
  const copy = S.separateAndAssign(rom, donor, null, donor);
  ui.animationClassId = 77; ui.animationKey = copy.syntheticAnimation.key;
  OB64.artUI.render(panel, rom, options, true);
  const copiedBefore = JSON.stringify(S.collectProject(rom));
  assert(!byKey('animation-layer-x').disabled);
  assert(!byKey('animation-move-x').parentNode.parentNode.hidden);
  let mode = byKey('alignment-frame-mode');
  const frameLabel = () => byKey('animation-align-frames').parentNode;
  const visible = () => panel.querySelector('.animation-visible-frame-selection');
  for (const value of ['all', 'current', 'advanced', 'visible']) {
    mode.value = value; mode.dispatch('change');
    assert.strictEqual(frameLabel().hidden, value !== 'advanced');
    assert.strictEqual(visible().hidden, value !== 'visible');
    OB64.artUI.render(panel, rom, options, true);
    mode = byKey('alignment-frame-mode'); assert.strictEqual(mode.value, value);
  }
  assert.strictEqual(JSON.stringify(S.collectProject(rom)), copiedBefore);
  assert.strictEqual(changes, 0);
  console.log('PASS editable-copy unlock, mode-specific visibility and durable mode state without mutation');
  const positions = () => copy.syntheticAnimation.frames.map(f => f.layers.map(l => [l.drawOffsetX, l.drawOffsetY]));
  const initialPositions = positions();
  const frameIndex = ui.animationFrame, layerIndex = ui.animationLayer;
  const x = byKey('animation-layer-x'); x.value = String(Number(x.value) + 1); x.dispatch('change');
  const expected = JSON.parse(JSON.stringify(initialPositions)); expected[frameIndex][layerIndex][0]++;
  assert.deepStrictEqual(positions(), expected, 'single-layer position retains all other Layers and Frames');
  mode = byKey('alignment-frame-mode'); mode.value = 'all'; mode.dispatch('change');
  const dx = byKey('animation-move-x'), dy = byKey('animation-move-y');
  dx.value = '2'; dx.dispatch('input'); dy.value = '-3'; dy.dispatch('input');
  const apply = dom.findText(panel, 'Translate Previewed Frames'); assert(!apply.disabled);
  assert(panel.querySelector('.animation-alignment-summary').textContent.includes('Move X 2, Y -3'));
  apply.click();
  assert.deepStrictEqual(positions(), expected.map(f => f.map(([x, y]) => [x + 2, y - 3])));
  assert.strictEqual(changes, 2, 'only explicit position change and batch apply mutate');
  console.log('PASS retained single-Layer and batch movement callbacks with exact scope');

  // These are CSS contracts, not a browser-rendering or pixel-geometry measurement.
  const css = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');
  assert(css.includes('grid-template-columns: repeat(auto-fit, minmax(min(100%, 300px), 1fr))'));
  assert(!css.includes('grid-template-columns: max-content minmax(320px, 520px) max-content'));
  assert(css.includes('.animation-alignment-panel [hidden] { display: none; }'), 'author display rules must respect hidden');
  assert(!css.includes('.animation-alignment-panel > figure { display: inline-flex'));
  assert(css.includes('.animation-alignment-previews figure { display: grid; }'));
  assert(css.includes('.animation-alignment-previews canvas { width: 100%; }'));
  assert(css.includes('.animation-frame-workbench canvas,\n.animation-alignment-previews canvas { max-width: 100%; height: auto; box-sizing: border-box; }'));
  assert(css.includes('.animation-frame-workbench .editor-transport > *,\n.animation-alignment-previews .editor-transport > * { min-width: 0; max-width: 100%; overflow-wrap: anywhere; }'));
  assert(css.includes('@media (max-width: 760px) {\n  .animation-alignment-previews { grid-template-columns: minmax(0, 1fr); }'));
  console.log('PASS wide/narrow source layout contracts: shrinkable tracks, contained transport, aspect ratio, comparison stack and hidden override');
})().catch(error => { console.error(error); process.exitCode = 1; });
