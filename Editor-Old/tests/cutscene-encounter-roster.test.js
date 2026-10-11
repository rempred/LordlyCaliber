'use strict';

// Event-launched mode-2 scenes take their cast from two gameplay units: the
// encountered unit (event property FD) first, then the player's current unit.
// The event script does not write FD, so the preview infers the encountered
// unit's members from the dialogue portraits of slots the stream never binds,
// and feeds them to the native roster builder as unit 31.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of [
  'data.js', 'parsers.js', 'repack.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-authoring.js', 'cutscene-dialogue-data.js',
  'cutscene-dialogue-lifecycle-data.js', 'cutscene-dialogue.js', 'cutscene-dialogue-lifecycle.js',
  'cutscene-resource-scheduler-data.js', 'cutscene-resource-scheduler.js',
  'cutscene-director-launch-data.js', 'cutscene-director-launch.js', 'cutscene-rom-start-data.js',
  'cutscene-rom-start.js', 'cutscene-preserved-stage-data.js', 'cutscene-preserved-stage.js',
  'cutscene-framebuffer-data.js', 'cutscene-framebuffer.js', 'cutscene-image-echo-data.js',
  'cutscene-image-echo.js', 'cutscene-shared-actor-data.js', 'cutscene-shared-actor.js',
  'cutscene-map-menu-data.js', 'cutscene-map-menu.js', 'cutscene-runtime.js', 'cutscene-preview.js',
  'cutscene-assets.js', 'cutscene-njpg.js', 'cutscene-sprites.js', 'cutscene-renderer.js',
  'cutscene-dialogue-draw.js', 'cutscene-project.js', 'cutscene-export.js', 'rom-compatibility.js', 'cutscene-ui.js'
]) vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });

async function open(rom, assetId) {
  const state = OB64.cutsceneUI.initialize(rom);
  const scene = state.catalog.getScene(assetId);
  state.selectedSceneId = scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  const runtime = state.runtimeByAssetId[scene.assetId];
  assert(runtime, JSON.stringify(state.sourceErrors));
  return { state, scene, runtime, document: state.histories[scene.storageId].present };
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = { z64, layout: { id: 'us-rev0' } };

  // Liedel (class 120) and Jeal (class 150): the stream binds Liedel into slot 0
  // and expects Jeal in slot 1 from the caller.
  const jeal = await open(rom, 'rom-director:01F3EC88');
  const launch = jeal.runtime.nativeLaunchInputs || jeal.state.nativeLaunchInputsByAssetId[jeal.scene.assetId];
  const directorLaunch = launch && launch.externalProducers.value.directorLaunch;
  assert(directorLaunch, 'ROM startup produced launch inputs');
  assert.deepStrictEqual(directorLaunch.previewEncounter, { classes: [150], leaderOnly: true, evidence: 'dialogue-portrait-class' });
  assert.equal(jeal.runtime.outcome, 'modeled-handoff', JSON.stringify(jeal.runtime.unresolvedQuery || null));
  let boxTick = -1;
  for (let tick = 0; tick < jeal.runtime.states.length; tick++) {
    if ((jeal.runtime.states[tick].dialogue || []).some(row => JSON.stringify(row).includes('surprised you caught on'))) { boxTick = tick; break; }
  }
  assert(boxTick > 0, 'Jeal’s box opens');
  const atBox = jeal.runtime.states[boxTick].actors;
  const liedel = atBox.find(actor => actor.slot === 0);
  const jealActor = atBox.find(actor => actor.slot === 1);
  assert(liedel && liedel.bank === 120 && liedel.visible, 'Liedel stays in slot 0');
  assert(jealActor && jealActor.visible, 'Jeal exists in slot 1 while he speaks');
  assert.equal(jealActor.bank, 150, 'Jeal draws with the Black Knight (Jeal) art');
  assert(Number.isFinite(jealActor.x) && Number.isFinite(jealActor.z));
  const moved = jeal.runtime.states.some(state => { const actor = state.actors.find(row => row.slot === 1); return actor && Math.abs(actor.x - jealActor.x) > 1; });
  assert(moved, 'the stream’s slot-1 move now moves Jeal');
  assert(launch.sourceIdentity.includes('encountered unit inferred from dialogue portrait class IDs 150'));
  assert.deepStrictEqual(jeal.document.actors.map(actor => actor.slot + ':' + actor.label),
    ['0:Archer (Liedel)', '1:Black Knight (Jeal) (inferred)'], 'cards name roster Actors by class and mark the inferred unit');
  assert(!OB64.cutsceneUI.sceneHasChanges(jeal.state, jeal.scene), 'naming roster Actors is not an edit');

  // A scene that binds every speaking slot itself gets no encounter unit.
  let controls = 0;
  for (const data of OB64.cutsceneData.scenes) {
    if (!(data.launchProfile && data.launchProfile.directorMode && data.launchProfile.directorMode.value === 2)) continue;
    const bound = jeal.state.catalog.getScene(data.assetId);
    const boundSource = await OB64.cutsceneCodec.loadSceneSource(z64, bound);
    const boundProgram = OB64.cutsceneCodec.createIr(bound, boundSource.decodedBytes, boundSource.nodeDefinitions).program;
    if (!boundProgram.primitives.some(node => [0x45, 0xAB].includes(node.opcode))) continue;
    const speakers = boundProgram.primitives.filter(node => node.opcode === 0xBF).map(node => node.rawWords[4] | 0);
    if (!speakers.length) continue;
    const binders = new Set(boundProgram.primitives.filter(node => [0x14, 0x92, 0x95, 0xA6].includes(node.opcode)).map(node => node.rawWords[1] | 0));
    let control;
    try { control = OB64.cutsceneRomStart.input(z64, bound, boundProgram).externalProducers.value.directorLaunch; } catch (_error) { continue; }
    const binderIndex = boundProgram.primitives.findIndex(node => node.opcode === 0xA6);
    const resetIndex = boundProgram.primitives.findIndex(node => node.opcode === 0x96);
    const deploysBeforeBinding = binderIndex >= 0 && (resetIndex < 0 || binderIndex < resetIndex);
    const memberBound = new Set(deploysBeforeBinding
      ? boundProgram.primitives.filter(node => node.opcode === 0xA6).map(node => node.rawWords[1] | 0) : []);
    const classBoxes = boundProgram.primitives.filter(node => node.opcode === 0xBF && (node.rawWords[11] | 0) > 0 && (node.rawWords[11] | 0) < 165)
      .map(node => node.rawWords[4] | 0);
    assert.equal(!!control.previewEncounter,
      speakers.some(slot => !binders.has(slot)) || classBoxes.some(slot => memberBound.has(slot)),
      data.assetId + ': an encounter unit exists exactly when a speaking slot is unbound, or (in a deploy-before-bind scene) bound by 0xA6 to a deployed member and given a class portrait');
    if (++controls >= 12) break;
  }
  assert(controls >= 6, 'several control scenes were checked');
  console.log('encounter roster: ok (Jeal in slot 1 at tick ' + boxTick + ', bank ' + jealActor.bank + ')');
})().catch(error => { console.error(error); process.exit(1); });
