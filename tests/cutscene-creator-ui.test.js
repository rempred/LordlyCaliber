'use strict';
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
    '    render: render, testCreator: renderCreatorWorkspace, testTransport: updateTransport,');
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
    getBoundingClientRect() { return { left: 0, top: 0, width: 320, height: 240 }; }
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
function text(root) { return all(root).map(item => item.textContent || '').join(' '); }
function byClass(root, name) { return all(root).filter(item => item.classList.contains(name)); }
function control(root, key) { return all(root).find(item => item.attrs['data-cutscene-focus-key'] === key); }
function button(root, label) { return all(root).find(item => item.tag === 'button' && item.textContent === label); }

(async () => {
  const raw = fs.readFileSync(path.join(editor, '..', 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' } };
  const state = OB64.cutsceneUI.initialize(rom);
  const scene = state.catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  state.selectedSceneId = scene.sceneId;
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene);
  const document = OB64.cutsceneCodec.projectSceneDocument(scene, source, state.catalog).document;
  state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(document);
  state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(document, 0);
  state.sourceByAssetId[scene.assetId] = source;
  global.document = { createElement: element };
  // This checks UI edits and native compilation. Runtime replay has its own integration test.
  OB64.cutsceneRuntime = null;
  const current = () => state.histories[scene.storageId].present;
  function render() {
    state.ui = {};
    const root = element('main');
    OB64.cutsceneUI.testCreator(root, rom, state, scene, current());
    return root;
  }
  let root = render();
  const view = state.views[scene.sceneId];
  assert.equal(byClass(root, 'cutscene-stage-panel').length, 1);
  assert.equal(byClass(root, 'cutscene-creator-sequence').length, 1);
  for (const hidden of ['cutscene-browser', 'cutscene-inspector', 'cutscene-sequence', 'cutscene-view-controls']) {
    assert.equal(byClass(root, hidden).length, 0, hidden + ' must be absent from the default workspace');
  }
  assert.equal(byClass(root, 'cutscene-creator-action-controls').length, 0);
  assert.equal(OB64.cutsceneModel.serializeSceneDocument(current(), 0), state.originalSerialized[scene.storageId]);

  const movement = current().tracks.flatMap(track => track.clips).find(clip => clip.kind === 'movement');
  assert(movement);
  control(root, 'creator-action:' + movement.id).fire('click');
  assert(!state.ui.resetInspectorScroll, 'opening an inline card must retain the action-list scroll position');
  root = render();
  assert.equal(byClass(root, 'cutscene-creator-action-controls').length, 1);
  button(root, 'Choose destination on Stage').fire('click');
  assert.equal(view.creatorDestinationClipId, movement.id);
  root = render();
  assert(text(root).includes('Click a destination'));
  state.renderedStage = { camera: {}, projection: {} };
  OB64.cutsceneRenderer.untransformStagePoint = point => point;
  OB64.cutsceneRenderer.unprojectPoint = point => ({ x: point.x, y: 0, z: point.y });
  state.ui.canvas.fire('pointerdown', { clientX: 42, clientY: 81 });
  const moved = OB64.cutsceneUI.findClipRow(current(), movement.id).clip;
  assert.deepStrictEqual(moved.payload.to, { x: 42, y: 0, z: 81 });
  assert.equal(view.creatorDestinationClipId, null);
  assert.equal(current().exportRequirements.capability, 'native');
  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, current());
  assert(!compiled.noOp, 'choosing a destination changes native output');
  const words = new DataView(compiled.decodedBytes.buffer, compiled.decodedBytes.byteOffset);
  assert.equal(words.getInt32((movement.source.startWord + 4) * 4), 42000);
  assert.equal(words.getInt32((movement.source.startWord + 5) * 4), 81000);
  root = render();
  button(root, 'Undo').fire('click');
  assert.notEqual(OB64.cutsceneUI.findClipRow(current(), movement.id).clip.payload.to.x, 42);
  root = render(); button(root, 'Redo').fire('click');
  assert.equal(OB64.cutsceneUI.findClipRow(current(), movement.id).clip.payload.to.x, 42);

  root = render();
  const actor = current().actors.find(item => item.id === view.selectedActorId);
  const appearance = control(root, 'creator-appearance:' + actor.id);
  appearance.value = '1'; appearance.fire('change');
  root = render();
  assert.equal(control(root, 'creator-appearance:' + actor.id).value, '1');
  const project = OB64.cutsceneProject.collect(state);
  assert.equal(project.scenes[0].document.actors.find(item => item.id === actor.id).source.variantSelector, 1);
  assert.deepStrictEqual(current().native, document.native, 'UI edits preserve the original command records');
  const art = all(root).find(item => item.tag === 'details' &&
    item.children[0].textContent === 'Art, animation and coordinates');
  art.open = true; art.fire('toggle'); root = render();
  assert(control(root, 'actor-bank:' + actor.id), 'the art catalogue opens only when requested');

  const add = all(root).find(item => item.tag === 'details' && item.children[0].textContent === '+ Add action');
  add.open = true; add.fire('toggle'); root = render();
  assert(all(root).find(item => item.tag === 'details' && item.children[0].textContent === '+ Add action').open);
  assert(!button(root, 'Together'), 'unsupported command groups must not look usable');
  OB64.cutsceneUI.testTransport(state, { frame: movement.startFrame, durationFrames: 2000 });
  assert(state.ui.creatorCards.find(row => row.clip.id === movement.id).element.classList.contains('at-playhead'));

  button(root, 'Choose scene').fire('click'); root = render();
  assert.equal(byClass(root, 'cutscene-browser').length, 1);
  button(root, 'Close').fire('click'); root = render();
  assert.equal(byClass(root, 'cutscene-browser').length, 0);
  button(root, 'Advanced').fire('click'); root = render();
  assert.equal(byClass(root, 'cutscene-view-controls').length, 1);
  assert.equal(byClass(root, 'cutscene-sequence').length, 1);
  console.log('PASS creator layout, inline selection, destination editing, native compilation, undo/redo, Project data, drawers, and playback highlight.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
