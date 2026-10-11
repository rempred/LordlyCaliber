'use strict';

// The default Cutscene Studio workspace offers six plain actions (Move, Pose,
// Speak, Wait, Camera pan, Fade), keeps timing in seconds, and hides native
// update counts and opcode language in Advanced.

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
function keys(root, prefix) { return all(root).filter(item => String(item.attrs['data-cutscene-focus-key'] || '').startsWith(prefix)); }

async function open(state, sceneId, z64) {
  const scene = state.catalog.getScene(sceneId);
  state.selectedSceneId = scene.sceneId;
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { allowModified: true });
  const document = OB64.cutsceneCodec.projectSceneDocument(scene, source, state.catalog).document;
  state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(document);
  state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(document, 0);
  state.sourceByAssetId[scene.assetId] = source;
  return { scene, source };
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' } };
  const state = OB64.cutsceneUI.initialize(rom);
  global.document = { createElement: element };
  OB64.cutsceneRuntime = null;

  async function exercise(sceneId, label) {
    const { scene, source } = await open(state, sceneId, z64);
    const current = () => state.histories[scene.storageId].present;
    function render() {
      state.ui = {};
      const root = element('main');
      OB64.cutsceneUI.testCreator(root, rom, state, scene, current());
      return root;
    }
    let root = render();
    const view = state.views[scene.sceneId];
    for (const name of ['Move', 'Pose', 'Speak', 'Wait', 'Camera pan', 'Fade']) {
      assert(control(root, 'creator-add:' + name), label + ': main palette offers ' + name);
    }
    const more = all(root).find(item => item.tag === 'details' && item.children[0].textContent === 'More actions');
    assert(more, label + ': Appear, Disappear, and Effect live under More actions');
    for (const name of ['Appear', 'Disappear', 'Effect']) assert(all(more).some(item => item.attrs['data-cutscene-focus-key'] === 'creator-add:' + name));
    assert(!control(root, 'creator-add:Projection'));

    // Add a fade and a wait; both are native and sit at an insertion point.
    control(root, 'creator-add:Fade').fire('click');
    root = render();
    const fade = current().tracks.flatMap(track => track.clips).find(clip => clip.kind === 'fade');
    assert(fade, label + ': Fade adds a fade clip');
    assert.equal(fade.capability, 'native');
    assert.equal(typeof fade.source.insertBeforeNodeId, 'string');
    assert.equal(view.selectedClipId, fade.id);
    const fadeCard = byClass(root, 'cutscene-creator-action').find(card => card.classList.contains('selected'));
    assert(fadeCard && text(fadeCard).includes('Fade to black'));
    assert(control(fadeCard, 'clip-fade-direction:' + fade.id));
    assert(control(fadeCard, 'clip-duration-seconds:' + fade.id));
    assert(control(fadeCard, 'clip-start-seconds:' + fade.id));
    assert(!control(fadeCard, 'clip-duration-frame:' + fade.id), 'frames are not shown on simple cards');
    assert(!control(fadeCard, 'clip-start-frame:' + fade.id));
    assert(!control(fadeCard, 'clip-path:' + fade.id), 'single-path scenes do not show a path chooser');
    const cardText = text(fadeCard);
    for (const jargon of ['native update', 'Native counter', 'tick', 'opcode', 'scheduler', 'registered']) {
      assert(!cardText.toLowerCase().includes(jargon.toLowerCase()), label + ': card text avoids "' + jargon + '"');
    }
    control(fadeCard, 'clip-fade-direction:' + fade.id).value = 'from-white';
    control(fadeCard, 'clip-fade-direction:' + fade.id).fire('change');
    control(fadeCard, 'clip-duration-seconds:' + fade.id).value = '1.5';
    control(fadeCard, 'clip-duration-seconds:' + fade.id).fire('change');
    const edited = current().tracks.flatMap(track => track.clips).find(clip => clip.id === fade.id);
    assert.deepStrictEqual([edited.payload.red, edited.payload.startAlpha, edited.payload.targetAlpha, edited.durationFrames], [255, 255, 0, 45]);
    root = render();
    assert(text(root).includes('Fade in from white'));

    control(root, 'creator-add:Wait').fire('click');
    root = render();
    const wait = current().tracks.flatMap(track => track.clips).find(clip => clip.kind === 'wait' && !clip.source.nodeId);
    assert(wait && wait.capability === 'native');
    const waitCard = byClass(root, 'cutscene-creator-action').find(card => card.classList.contains('selected'));
    assert(control(waitCard, 'clip-duration-seconds:' + wait.id));
    assert(!keys(waitCard, 'creator-wait:').length, 'the tick-based wait field is gone');
    assert(!text(waitCard).toLowerCase().includes('native updates'));

    // Speak binds the box to the selected Actor and resolves that Actor's own face.
    const hasTemplate = current().native.commands.some(command => command.words[0] === 0xBF && command.words.length === 14);
    let speakerActor = null;
    if (hasTemplate && current().actors.length > 1) {
      speakerActor = current().actors[current().actors.length - 1];
      view.selectedActorId = speakerActor.id;
      root = render();
      control(root, 'creator-add:Speak').fire('click');
      root = render();
      const speak = current().tracks.flatMap(track => track.clips).find(clip => clip.payload.nativeDialogueAuthored === true);
      assert(speak && speak.capability === 'native', label + ': Speak adds a native box');
      assert.equal(speak.payload.ownerSlot, speakerActor.slot, 'the box is placed near the selected Actor');
      assert.equal(speak.payload.portrait, 0, 'the avatar starts as No face and is chosen directly');
      assert.equal(speak.payload.speaker, speakerActor.label.length <= 28 ? speakerActor.label : 'Speaker');
      const speakCard = byClass(root, 'cutscene-creator-action').find(card => card.classList.contains('selected'));
      const face = control(speakCard, 'clip-dialogue-face:' + speak.id);
      assert(face, 'the card offers an avatar choice');
      const choices = face.children.map(option => option.value);
      assert(choices.length > 100 && choices[0] === '0' && choices.includes('83') && choices.at(-1) === 'actor',
        'avatar choices are the classes with portraits plus No face and the Actor’s own face');
      const windowWords = () => {
        const bytes = OB64.cutsceneCodec.compileSceneDocument(scene, source, current()).decodedBytes;
        const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        const streamWords = Array.from({ length: bytes.length / 4 }, (_, index) => dv.getInt32(index * 4));
        const box = streamWords.findIndex((word, index) => word === 0xBF && streamWords[index + 1] === speak.payload.windowId);
        assert(box > 0, label + ': compiled stream contains the authored window');
        return streamWords.slice(box, box + 14);
      };
      let box = windowWords();
      assert.equal(box[4], speakerActor.slot, 'operand 3 places the box near the assigned Actor');
      assert.equal(box[11], 0, 'No face compiles to class None');
      face.value = '83';
      face.fire('change');
      box = windowWords();
      assert.equal(box[11], 83, 'a chosen avatar compiles to its class ID');
      face.value = 'actor';
      face.fire('change');
      assert.equal(windowWords()[11], -1, 'the Actor’s own face compiles to the owner sentinel');
      face.value = '0';
      face.fire('change');
    }

    // The edited scene compiles natively with the fade in front of its insertion point.
    const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, source, current());
    const view32 = new DataView(compiled.decodedBytes.buffer, compiled.decodedBytes.byteOffset, compiled.decodedBytes.byteLength);
    const words = Array.from({ length: compiled.decodedBytes.length / 4 }, (_, index) => view32.getInt32(index * 4));
    const at = words.findIndex((word, index) => word === 0x1B && words[index + 1] === 45 && words[index + 2] === 255 && words[index + 6] === 0);
    assert(at > 0, label + ': compiled stream contains the authored fade');
    const assessment = OB64.cutsceneExport.assessNativeDelta(scene,
      OB64.cutsceneModel.parseSceneDocument(state.originalSerialized[scene.storageId]), current(), source);
    assert.equal(assessment.capability, 'native', label + ': ' + assessment.reasons.join(' '));
    console.log(label + ': ok');
  }

  await exercise('loading-magnus-walk-opening-ceremony-cutscene', 'reviewed opening scene');
  const retail = OB64.cutsceneData.scenes.find(scene => scene.source.dynamicGrammar === true &&
    scene.source.runtimeNodeCount > 40);
  await exercise(retail.assetId, 'unreviewed retail scene ' + retail.assetId);
})().catch(error => { console.error(error); process.exit(1); });
