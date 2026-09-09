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
  'art.js',
  'sprite-library.js',
  'animation-corpus-data.js',
  'animation-art.js',
  'combat-animation-overrides-data.js',
  'combat-animation-overrides.js',
  'animation-sequences.js',
  'animation-ui.js',
  'sprite-editor-ui.js'
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

function giantAdvance(rom) {
  const rows = OB64.spriteEditorUI.animationSources(rom, 77);
  const lanes = new Set(rows.map(row => S.laneFor(row)));
  for (const lane of ['idle', 'advance', 'return', 'hit']) {
    assert(lanes.has(lane), 'Giant source catalog contains ' + lane);
  }
  assert(rows.some(row => row.spriteSourceAssigned));
  const donor = rows.find(row =>
    S.laneFor(row) === 'advance' && U.selectorFlags(row) === '1/1');
  assert(donor, 'Giant player-side advance route exists');
  return { donor, rows };
}

function candidateFor(plan) {
  const bytes = z64.slice();
  const allocations = {};
  let cursor = A.constants.ARENA_START;
  for (const row of plan.relocatedResources) {
    const pad = row.stored.length & 1;
    const allocation = {
      name: row.name,
      stored: row.stored,
      entry: cursor,
      key: cursor - A.constants.RESOURCE_BASE,
      pad,
      end: cursor + 4 + row.stored.length + pad
    };
    assert(allocation.end <= A.constants.ARENA_END);
    allocations[row.name] = allocation;
    cursor = allocation.end;
  }
  S.finalizeAllocations(plan, allocations);
  for (const row of Object.values(allocations)) {
    A.writeU32(bytes, row.entry, row.stored.length);
    bytes.set(row.stored, row.entry + 4);
    if (row.pad) bytes[row.end - 1] = 0;
  }
  S.applyPlan(plan, bytes, [], []);
  return bytes;
}


function assetFor(rom, animation, count) {
  const library = L.initialize(rom);
  const pixels = U.framePixels(animation, animation.frames[0], rom.art.animations, null, null, 0);
  return L.prepareAssetFile(L.assetFileText(L.assetFromFrames(library, {
    name: 'Whole sequence regression', kind: 'sequence',
    width: animation.canvas.width, height: animation.canvas.height,
    anchor: { x: -animation.canvas.originX + 7, y: -animation.canvas.originY - 5 },
    frames: Array.from({ length: count }, (_, i) => ({ name: 'Frame ' + i, ticks: i + 2,
      layers: [{ name: 'Frame', rgba: (() => { const frame = pixels.slice(); frame.set([i * 16, 248 - i * 8, 80, 255], i * 4); return frame; })() }] }))
  })));
}
function stateSnapshot(rom, ui) {
  return JSON.stringify({ project: S.collectProject(rom), overrides: rom.combatAnimationOverrides,
    artSourceKeys: Object.keys(rom.art.animations.artByKey).sort(),
    edits: rom.art.animations.edits, history: rom.art.animations.history,
    revision: rom.animationSequences.revision, dirty: rom.animationSequences.dirty,
    editRevision: rom.art.animations.editRevision, ui });
}
(async () => {
  for (const delta of [-1, 0, 2]) {
    const rom = await freshRom(), { donor } = giantAdvance(rom);
    const sep = S.separateAndAssign(rom, donor, null, donor), original = sep.syntheticAnimation;
    const count = delta < 0 ? 1 : original.frames.length + delta;
    const asset = assetFor(rom, original, count);
    const prepared = U.prepareLibrarySequence(asset, original, {});
    const ui = { animationFrame: 2, animationLayer: 1 }, notices = [];
    let changes = 0, renders = 0;
    assert.strictEqual(U.importLibrarySequence(rom.art, rom, sep, donor, original, asset, ui,
      { onChange() { changes++; }, onNotice(text) { notices.push(text); } }, () => renders++), true);
    assert.strictEqual(changes, 1); assert.strictEqual(renders, 1);
    assert.strictEqual(rom.animationSequences.separations[sep.id], sep, 'stable separation identity');
    assert.strictEqual(sep.syntheticAnimation.frames.length, count);
    sep.syntheticAnimation.frames.forEach((frame, i) => {
      assert.strictEqual(frame.ticks, asset.frames[i].ticks);
      assert.strictEqual(frame.layers[0].drawOffsetX, -prepared[i].prepared.alignmentAnchor.x);
      assert.strictEqual(frame.layers[0].drawOffsetY, -prepared[i].prepared.alignmentAnchor.y);
      const child = M.currentEdit(rom.art.animations, frame.layers[0].sourceKey, 0);
      assert.deepStrictEqual(child.indices, prepared[i].prepared.indices);
      assert.deepStrictEqual(child.intensity, prepared[i].prepared.intensity);
    });
    const project = S.collectProject(rom), loaded = await freshRom();
    S.applyProject(loaded, S.prepareProject(loaded, project));
    assert.deepStrictEqual(S.collectProject(loaded), project, 'whole sequence Project round trip');
    const plan = S.buildPlan(rom, z64); S.verifyPlan(plan, candidateFor(plan));
    console.log('PASS whole sequence count', count, 'anchor/content/ticks/Project/native extract-back');
  }
  for (const existing of [false, true]) {
    const rom = await freshRom(), { donor } = giantAdvance(rom);
    const sep = existing ? S.separateAndAssign(rom, donor, null, donor) : null;
    const animation = sep ? sep.syntheticAnimation : donor;
    const asset = assetFor(rom, animation, 2), prepared = U.prepareLibrarySequence(asset, animation, {});
    // Fail on the second frame, after candidate structure and first frame have changed.
    prepared[1].prepared.paletteWords = new Uint16Array(1);
    const ui = { animationKey: animation.key, animationFrame: 1, animationLayer: 0 };
    const before = stateSnapshot(rom, ui); let changes = 0, renders = 0;
    assert.strictEqual(U.importLibrarySequence(rom.art, rom, sep, donor, animation, asset, ui,
      { onChange() { changes++; } }, () => renders++, prepared), false);
    assert.strictEqual(stateSnapshot(rom, ui), before, 'late native rejection is atomic');
    assert.strictEqual(changes, 0); assert.strictEqual(renders, 0);
    const invalid = Object.assign({}, asset, { anchor: { x: 1000000, y: 0 } });
    U.importLibrarySequence(rom.art, rom, sep, donor, animation, invalid, ui,
      { onChange() { changes++; } }, () => renders++);
    assert.strictEqual(stateSnapshot(rom, ui), before, 'conversion rejection is atomic');
    assert.strictEqual(changes, 0); assert.strictEqual(renders, 0);
    console.log('PASS atomic rejection', existing ? 'existing target' : 'new route');
  }
  {
    const rom = await freshRom(), { donor, rows } = giantAdvance(rom);
    const other = S.separateAndAssign(rom, donor, null, donor);
    const otherProject = S.collectProject(rom).entries[other.id];
    const attack = U.effectiveAnimationCatalog(rom.art, rom).specs.find(row => row.spec.classId === 2 && row.spec.actionId === 4 && row.spec.rawMode === 0 && U.selectorFlags(row) === '0/0');
    assert(attack, 'appended attack route fixture');
    const asset = assetFor(rom, attack, 2), ui = {};
    let changes = 0, routeChanges = 0;
    assert.strictEqual(U.importLibrarySequence(rom.art, rom, null, attack, attack, asset, ui,
      { onChange() { changes++; }, onAnimationRouteChange() { routeChanges++; } }, () => {}), true);
    assert.strictEqual(changes, 0); assert.strictEqual(routeChanges, 1);
    assert.deepStrictEqual(S.collectProject(rom).entries[other.id], otherProject, 'unrelated private sequence preserved');
    const plan = S.buildPlan(rom, z64); S.verifyPlan(plan, candidateFor(plan));
    console.log('PASS appended combat route and unrelated private sequence preservation');
  }
  // Exercise the product modal using a deterministic DOM substitute, without a browser.
  class Element {
    constructor(tag) { this.tag = tag; this.children = []; this.listeners = {}; this.style = {}; this.attrs = {}; }
    appendChild(child) { this.children.push(child); child.parentNode = this; return child; }
    removeChild(child) { this.children.splice(this.children.indexOf(child), 1); child.parentNode = null; }
    setAttribute(key, value) { this.attrs[key] = value; }
    addEventListener(name, callback) { this.listeners[name] = callback; }
    focus() {}
    getContext() { return { fillRect() {} }; }
    set innerHTML(value) { this.children = []; }
  }
  global.document = { body: new Element('body'), createElement: tag => new Element(tag),
    addEventListener() {}, removeEventListener() {} };
  function descendants(node) { return [node, ...node.children.flatMap(descendants)]; }
  const rom = await freshRom(), { donor } = giantAdvance(rom);
  const asset = assetFor(rom, donor, 2), ui = { animationFrame: 3, animationLayer: 0 };
  const before = stateSnapshot(rom, ui); let changes = 0;
  const open = () => U.openLibrarySequenceImportModal(rom.art, rom, null, donor, donor, asset, ui,
    { onChange() { changes++; } }, () => {});
  open();
  let nodes = descendants(document.body);
  const sizing = nodes.find(node => node.tag === 'select' && node.children.some(child => child.value === 'stretch'));
  assert(sizing); assert.deepStrictEqual(sizing.children.map(row => row.value), ['original', 'fit', 'fill', 'stretch']);
  for (const mode of ['original', 'fit', 'fill', 'stretch']) {
    sizing.value = mode; sizing.listeners.change();
    const stats = nodes.find(node => node.className === 'art-import-stats');
    assert(/Source .*Output .*Scale .*Anchor .*→/.test(stats.textContent));
    assert.strictEqual(stateSnapshot(rom, ui), before, 'preparation never changes target or selection');
  }
  nodes.find(node => node.textContent === 'Cancel').listeners.click();
  assert.strictEqual(document.body.children.length, 0); assert.strictEqual(changes, 0);
  open(); nodes = descendants(document.body);
  nodes.find(node => node.textContent === 'Import Frame Sequence').listeners.click();
  assert.strictEqual(document.body.children.length, 0); assert.strictEqual(changes, 1);
  assert.strictEqual(Object.keys(rom.animationSequences.separations).length, 1);
  assert.strictEqual(Object.values(rom.animationSequences.separations)[0].syntheticAnimation.frames.length, 2);
  console.log('PASS modal sizing/info/cancel/apply and new private route');
  // All sizing transforms carry the source anchor into prepared output coordinates.
  const small = { canvas: { width: 8, height: 6 } };
  const sample = { kind: 'sequence', width: 4, height: 2, anchor: { x: 1, y: 1 },
    frames: [{ ticks: 3, layers: [{ width: 4, height: 2, x: 0, y: 0,
      pixels: new Uint8ClampedArray(32).fill(255), visible: true }] }] };
  const anchors = { original: { x: 1, y: 1 }, fit: { x: 2, y: 3 }, fill: { x: 1, y: 3 }, stretch: { x: 2, y: 3 } };
  for (const mode of Object.keys(anchors)) {
    const result = U.prepareLibrarySequence(sample, small, { placementMode: mode })[0].prepared;
    assert.deepStrictEqual(result.alignmentAnchor, anchors[mode], mode + ' anchor transform');
  }
  console.log('PASS sequence sizing transforms');
  const css = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');
  assert(css.includes('.sprite-paint-color-panel { grid-column: 2; grid-row: 2 / span 2; }'));
  assert(css.includes('.sprite-used-colors-panel { grid-column: 2; grid-row: 4; }'));
  assert(!css.includes('.sprite-color-panel { grid-column: 2; grid-row: 2 / span 3; }'));
  console.log('PASS distinct responsive color-panel source placement');
})().catch(error => { console.error(error); process.exitCode = 1; });
