'use strict';

// The Event launch drawer: a new scene is assigned to an exact launch site
// through the rendered controls, the scene reports what launches it, short
// sites can be assigned too, shared programs are flagged, and a
// retail scene can take its own site back.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-model.js', 'cutscene-provenance-data.js',
  'cutscene-catalog.js', 'cutscene-director.js', 'cutscene-codec.js', 'cutscene-authored.js',
  'cutscene-event-rows-data.js', 'cutscene-events.js', 'cutscene-authoring.js',
  'cutscene-runtime.js', 'cutscene-preview.js', 'cutscene-renderer.js', 'cutscene-project.js',
  'cutscene-export.js', 'cutscene-ui.js']) {
  let source = fs.readFileSync(path.join(editor, file), 'utf8');
  if (file === 'cutscene-ui.js') source = source.replace('    render: render,',
    '    render: render, testCreator: renderCreatorWorkspace,');
  vm.runInThisContext(source, { filename: file });
}
function element(tag) {
  const result = { tag, children: [], attrs: {}, events: {}, style: {}, className: '', disabled: false,
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
    },
    add(name) { this.toggle(name, true); }
  };
  return result;
}
function all(root) { return [root, ...root.children.flatMap(all)]; }
function text(root) { return all(root).map(item => item.textContent || '').join(' '); }
function byClass(root, name) { return all(root).filter(item => item.classList.contains(name)); }
function control(root, key) { return all(root).find(item => item.attrs['data-cutscene-focus-key'] === key); }

const OPENING = 'event-director:1:b0862';
const SHORT = 'event-director:1:b0032';

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' } };
  global.document = { createElement: element };
  const state = OB64.cutsceneUI.initialize(rom);
  await state.authoredReady;
  const notices = [];
  state.callbacks.onChange = label => notices.push(label);
  OB64.cutsceneRuntime = null;

  function openDocument(scene, source) {
    const document = OB64.cutsceneCodec.projectSceneDocument(scene, source, state.catalog).document;
    state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(document);
    state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(document, 0);
    state.sourceByAssetId[scene.assetId] = source;
  }
  function render(scene) {
    state.ui = {};
    const root = element('main');
    OB64.cutsceneUI.testCreator(root, rom, state, scene, state.histories[scene.storageId].present);
    return root;
  }

  // 1. A new scene says it is not launched, and its Event launch drawer opens.
  const scene = await OB64.cutsceneUI.createAuthoredScene(rom, state, 'Drawer scene');
  openDocument(scene, await OB64.cutsceneCodec.authoredSceneSource(scene, scene.authored.initialDecodedBytes));
  let root = render(scene);
  assert(/Not launched by any event yet/.test(text(control(root, 'scene-authored-identity'))));
  control(root, 'creator-drawer:launch').fire('click');
  root = render(scene);
  assert(control(root, 'launch-panel'), 'the drawer shows the launch panel');
  assert(/No event launches this scene yet/.test(text(control(root, 'launch-panel'))));

  // 1b. With no Actors, the Actions panel says where + Actor is, the Actor
  // actions carry the same reason, and + Actor is available.
  assert(text(control(root, 'creator-actor-hint')).includes('Use + Actor under the Stage'));
  assert(control(root, 'creator-add:Move').disabled && control(root, 'creator-add:Move').title.includes('+ Actor'));
  const addActorButton = all(root).find(item => item.textContent === '+ Actor');
  assert(addActorButton && !addActorButton.disabled, '+ Actor is available in an empty scene');
  assert(/no Actors yet/.test(text(root)));

  // 2. Searching row 1 lists its sites: the short site can be assigned, and
  // the shared program is flagged.
  control(root, 'launch-search').value = '1';
  control(root, 'launch-search').fire('input');
  root = render(scene);
  const shortControl = control(root, 'launch-assign:' + SHORT);
  assert(shortControl && !shortControl.disabled && shortControl.textContent === 'Assign' && /stub/.test(shortControl.title));
  assert(byClass(root, 'cutscene-launch-shared').length > 0, 'row 1 is marked as a shared program');
  const assign = control(root, 'launch-assign:' + OPENING);
  assert(assign && !assign.disabled && assign.textContent === 'Assign');

  // 3. Assign: the binding is set, the app is notified, and the scene reports it.
  assign.fire('click');
  assert.deepStrictEqual(state.eventBindings, [{ launchSiteId: OPENING,
    target: { kind: 'authored', authoredId: scene.authored.authoredId } }]);
  assert(/now launches Drawer scene/.test(notices.at(-1)), notices.at(-1));
  assert.strictEqual(OB64.cutsceneUI.editCount(state), 2, 'the new scene and the binding set');
  root = render(scene);
  assert(/Launched by 1 assigned event site/.test(text(control(root, 'scene-authored-identity'))));
  assert(control(root, 'launch-remove:' + OPENING), 'the site is listed with a Remove control');
  assert.strictEqual(control(root, 'launch-assign:' + OPENING).textContent, 'Launches this scene');

  // 4. The retail scene that site used to launch says so, and can take it back.
  const retail = state.catalog.getScene(OB64.cutsceneEvents.site(state.catalog, OPENING).retailSceneId);
  openDocument(retail, await OB64.cutsceneCodec.loadSceneSource(z64, retail, {}));
  render(retail);
  Object.assign(state.views[retail.sceneId], { creatorDrawer: 'launch', launchQuery: '1' });
  root = render(retail);
  assert(/now launches another scene/.test(text(control(root, 'scene-launch-summary'))));
  const takeBack = control(root, 'launch-assign:' + OPENING);
  assert.strictEqual(takeBack.textContent, 'Assign');
  takeBack.fire('click');
  assert.deepStrictEqual(state.eventBindings, [], 'assigning a site to its own retail scene removes the binding');

  // 5. Remove from the new scene's drawer after assigning again.
  OB64.cutsceneUI.assignLaunchSite(rom, state, scene, OPENING);
  root = render(scene);
  control(root, 'launch-remove:' + OPENING).fire('click');
  assert.deepStrictEqual(state.eventBindings, []);
  assert(/launches its retail scene again/.test(notices.at(-1)));

  // 5b. The Background drawer: a thumbnail grid of the scene's own table rows,
  // the current row pressed, selection, and Restore original.
  root = render(scene);
  control(root, 'creator-drawer:background').fire('click');
  root = render(scene);
  const groups = state.histories[scene.storageId].present.background.projection.nativeGroups;
  const choices = all(root).filter(item => item.classList.contains('cutscene-background-choice'));
  assert.strictEqual(choices.length, groups.length, 'one choice per complete table row');
  assert.strictEqual(control(root, 'background-choice:21').attrs['aria-pressed'], 'true');
  assert(/Now: Scene group 21/.test(text(control(root, 'background-current'))));
  assert(!control(root, 'background-restore'), 'no restore before a change');
  control(root, 'background-choice:10').fire('click');
  root = render(scene);
  assert.strictEqual(state.histories[scene.storageId].present.background.projection.selectedSelector, 10);
  assert.strictEqual(control(root, 'background-choice:10').attrs['aria-pressed'], 'true');
  assert(/Original: Scene group 21/.test(text(control(root, 'background-current'))));
  control(root, 'background-restore').fire('click');
  assert.strictEqual(state.histories[scene.storageId].present.background.projection.selectedSelector, 21);
  control(root, 'creator-drawer:background').fire('click');

  // 6. The toolbar name is the one name control of a new scene: it renames the
  // persisted identity and the document title, including in undo history, and
  // the Project saves that name.
  root = render(scene);
  const nameField = control(root, 'creator-scene-name');
  assert.strictEqual(nameField.value, 'Drawer scene');
  assert(!/Scene name/.test(text(control(root, 'scene-authored-identity'))), 'no second name field');
  nameField.value = 'Renamed in toolbar';
  nameField.fire('change');
  assert.strictEqual(scene.authored.name, 'Renamed in toolbar');
  const history = state.histories[scene.storageId];
  assert.strictEqual(history.present.identity.friendlyName, 'Renamed in toolbar');
  assert(history.past.every(entry => entry.before.identity.friendlyName === 'Renamed in toolbar'));
  assert.strictEqual(OB64.cutsceneProject.collect(state).authoredScenes[0].name, 'Renamed in toolbar');

  console.log('PASS Event launch drawer: not-launched state, row search, short sites assignable, shared programs flagged, assign with notification and edit count, launch summary, retail scene takes its site back, remove, and the single toolbar name control of a new scene, the + Actor hint of an empty scene, and the Background drawer.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
