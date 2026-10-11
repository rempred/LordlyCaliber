'use strict';

// Added actions in the Actions list can be reordered by dragging a card, or
// with Move earlier / Move later on the open card. The compiled stream follows
// the list order; native commands keep their places and refuse to move.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-model.js',
  'cutscene-catalog.js', 'cutscene-director.js', 'cutscene-codec.js', 'cutscene-authoring.js',
  'cutscene-runtime.js', 'cutscene-preview.js', 'cutscene-renderer.js', 'cutscene-project.js',
  'cutscene-export.js', 'cutscene-ui.js']) {
  let source = fs.readFileSync(path.join(editor, file), 'utf8');
  if (file === 'cutscene-ui.js') source = source.replace('    render: render,',
    '    render: render, testCreator: renderCreatorWorkspace,');
  vm.runInThisContext(source, { filename: file });
}
function element(tag) {
  const result = { tag, children: [], attrs: {}, events: {}, style: {}, className: '',
    appendChild(child) { this.children.push(child); return child; },
    setAttribute(key, value) { this.attrs[key] = value; },
    addEventListener(key, listener) { (this.events[key] ||= []).push(listener); },
    fire(key, event = {}) { (this.events[key] || []).forEach(fn => fn(event)); },
    focus() {},
    get childElementCount() { return this.children.length; },
    get firstChild() { return this.children[0]; },
    getBoundingClientRect() { return { left: 0, top: 100, width: 320, height: 40 }; }
  };
  result.classList = { contains(name) { return result.className.split(' ').includes(name); },
    toggle(name, value) {
      const classes = new Set(result.className.split(' ').filter(Boolean));
      if (value === undefined ? !classes.has(name) : value) classes.add(name); else classes.delete(name);
      result.className = [...classes].join(' ');
    }
  };
  return result;
}
function all(root) { return [root, ...root.children.flatMap(all)]; }
function byClass(root, name) { return all(root).filter(item => item.classList.contains(name)); }
function control(root, key) { return all(root).find(item => item.attrs['data-cutscene-focus-key'] === key); }
function dataTransfer() {
  const store = {};
  return { setData(type, value) { store[type] = value; }, getData(type) { return store[type]; } };
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' } };
  const state = OB64.cutsceneUI.initialize(rom);
  global.document = { createElement: element };
  OB64.cutsceneRuntime = null;

  const scene = state.catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  state.selectedSceneId = scene.sceneId;
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { allowModified: true });
  const initial = OB64.cutsceneCodec.projectSceneDocument(scene, source, state.catalog).document;
  state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(initial);
  state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(initial, 0);
  state.sourceByAssetId[scene.assetId] = source;
  const history = state.histories[scene.storageId];
  const current = () => history.present;
  function render() {
    state.ui = {};
    const root = element('main');
    OB64.cutsceneUI.testCreator(root, rom, state, scene, current());
    return root;
  }
  const clipsById = () => Object.fromEntries(current().tracks.flatMap(track => track.clips).map(clip => [clip.id, clip]));
  const cardOrder = root => byClass(root, 'cutscene-creator-action').map(card => card.attrs['data-cutscene-clip-id']);
  const cardFor = (root, id) => byClass(root, 'cutscene-creator-action').find(card => card.attrs['data-cutscene-clip-id'] === id);
  const compiledOrder = ids => {
    const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, current());
    return compiled.changes.filter(change => change.operation === 'insert' && ids.includes(change.clipId))
      .sort((left, right) => left.startWord - right.startWord).map(change => change.clipId);
  };

  let root = render();
  const view = state.views[scene.sceneId];
  for (const name of ['Fade', 'Wait', 'Camera pan']) {
    control(root, 'creator-add:' + name).fire('click');
    root = render();
  }
  const fade = Object.values(clipsById()).find(clip => clip.kind === 'fade');
  const wait = Object.values(clipsById()).find(clip => clip.kind === 'wait' && !clip.source.nodeId);
  const camera = Object.values(clipsById()).find(clip => clip.kind === 'camera' && !clip.source.nodeId);
  assert(fade && wait && camera, 'three authored actions were added');
  const authored = [fade.id, wait.id, camera.id];
  const relative = order => order.filter(id => authored.includes(id));
  assert.deepStrictEqual(compiledOrder(authored), relative(cardOrder(root)), 'the stream follows the list');
  // Cards added at one start keep their creation order (each new card gets an
  // explicit sequence after the tied ones), in the list and the stream alike.
  assert.deepStrictEqual(relative(cardOrder(root)), [fade.id, wait.id, camera.id],
    'equal starts keep creation order');

  // Added cards are draggable; native cards are not.
  assert.equal(cardFor(root, fade.id).attrs.draggable, 'true');
  const nativeCard = byClass(root, 'cutscene-creator-action').find(card => !authored.includes(card.attrs['data-cutscene-clip-id']));
  assert(nativeCard && nativeCard.attrs.draggable === undefined, 'native commands are not draggable');
  assert.equal(OB64.cutsceneUI.reorderAction(rom, state, scene, nativeCard.attrs['data-cutscene-clip-id'], fade.id, false),
    'native-command');

  // Drag the camera pan and drop it on the lower half of the wait card: it lands after the wait.
  const transfer = dataTransfer();
  cardFor(root, camera.id).fire('dragstart', { dataTransfer: transfer });
  assert.equal(state.ui.draggingClipId, camera.id);
  const waitCard = cardFor(root, wait.id);
  waitCard.fire('dragover', { clientY: 135, dataTransfer: transfer, preventDefault() {} });
  assert(waitCard.classList.contains('drop-after'), 'the drop marker shows below the target');
  waitCard.fire('drop', { clientY: 135, dataTransfer: transfer, preventDefault() {} });
  root = render();
  assert.deepStrictEqual(relative(cardOrder(root)), [fade.id, wait.id, camera.id], 'the dragged card moved after the wait');
  assert.deepStrictEqual(compiledOrder(authored), [fade.id, wait.id, camera.id], 'the compiled stream follows the new order');
  assert.equal(view.selectedClipId, camera.id, 'the moved card stays selected');

  // Dropping on the upper half of the fade card places it before the fade.
  const transfer2 = dataTransfer();
  cardFor(root, camera.id).fire('dragstart', { dataTransfer: transfer2 });
  const fadeCard = cardFor(root, fade.id);
  fadeCard.fire('dragover', { clientY: 105, dataTransfer: transfer2, preventDefault() {} });
  assert(fadeCard.classList.contains('drop-before'), 'the drop marker shows above the target');
  fadeCard.fire('drop', { clientY: 105, dataTransfer: transfer2, preventDefault() {} });
  root = render();
  assert.deepStrictEqual(relative(cardOrder(root)), [camera.id, fade.id, wait.id]);
  assert.deepStrictEqual(compiledOrder(authored), [camera.id, fade.id, wait.id]);
  const moved = clipsById()[camera.id];
  assert.equal(moved.startFrame, clipsById()[fade.id].startFrame, 'the moved action takes the start of the card after it');
  assert.equal(moved.source.insertBeforeNodeId, clipsById()[fade.id].source.insertBeforeNodeId, 'and its insertion point');
  assert(moved.source.sequence < clipsById()[fade.id].source.sequence, 'sequence keys order equal starts');

  // Move earlier / Move later on the open card reorder without dragging.
  view.selectedClipId = camera.id;
  root = render();
  control(root, 'creator-reorder-later:' + camera.id).fire('click');
  root = render();
  assert.deepStrictEqual(relative(cardOrder(root)), [fade.id, camera.id, wait.id], 'Move later swaps with the card below');
  assert.deepStrictEqual(compiledOrder(authored), [fade.id, camera.id, wait.id]);
  control(root, 'creator-reorder-earlier:' + camera.id).fire('click');
  root = render();
  assert.deepStrictEqual(relative(cardOrder(root)), [camera.id, fade.id, wait.id], 'Move earlier swaps back');

  // Dropping before a later native command adopts a safe insertion point there.
  const laterNative = byClass(root, 'cutscene-creator-action').map(card => card.attrs['data-cutscene-clip-id'])
    .slice(cardOrder(root).indexOf(camera.id) + 1)
    .map(id => clipsById()[id]).find(clip => clip.source.nodeId &&
      scene.source.nodes.some(node => node.id === clip.source.nodeId && node.insertBefore === true));
  assert(laterNative, 'the scene has a later native command that accepts insertions');
  assert.equal(OB64.cutsceneUI.reorderAction(rom, state, scene, wait.id, laterNative.id, false), null);
  root = render();
  assert.equal(clipsById()[wait.id].source.insertBeforeNodeId, laterNative.source.nodeId, 'the wait now inserts before that command');
  assert.equal(clipsById()[wait.id].startFrame, laterNative.startFrame);
  const order = cardOrder(root);
  assert(order.indexOf(wait.id) === order.indexOf(laterNative.id) - 1, 'the card sits right above the native command');
  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, current());
  assert.equal(compiled.changes.find(change => change.clipId === wait.id).beforeNodeId, laterNative.source.nodeId);
  const assessment = OB64.cutsceneExport.assessNativeDelta(scene,
    OB64.cutsceneModel.parseSceneDocument(state.originalSerialized[scene.storageId]), current(), source);
  assert.equal(assessment.capability, 'native', assessment.reasons.join(' '));

  // Every reorder is one undo step.
  OB64.cutsceneModel.undo(history);
  root = render();
  assert.deepStrictEqual(relative(cardOrder(root)), [camera.id, fade.id, wait.id], 'undo restores the previous order');
  console.log('cutscene action reorder: ok (' + scene.assetId + ')');
})().catch(error => { console.error(error); process.exit(1); });
