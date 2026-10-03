'use strict';

// Background selection. A scene's own background command (0x80000006) picks a
// row of a native table: classes 4 and 5 a scene group (0..30), classes 1 and
// 8 a fresh mode-two environment (0..79), whose foreground follows the same
// selector. Selecting a row changes the preview, export rewrites only that
// operand, ROM-only reopen and Project load bring the choice back, and routes
// the scene's command does not own explain why they cannot be chosen.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of [
  'data.js', 'parsers.js', 'repack.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-provenance-data.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-authored.js', 'cutscene-event-rows-data.js', 'cutscene-events.js',
  'cutscene-authoring.js', 'cutscene-dialogue-data.js',
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

const UI = OB64.cutsceneUI;
const romFrom = z64 => ({ z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) });
const present = (state, scene) => state.histories[scene.storageId].present;

async function open(rom, state, sceneId) {
  const scene = state.catalog.getScene(sceneId);
  assert(scene, sceneId + ' exists');
  state.selectedSceneId = scene.sceneId;
  await UI.loadScene(rom, state, scene);
  return scene;
}

async function replay(rom, state, scene) {
  delete state.runtimeByAssetId[scene.assetId];
  await UI.loadScene(rom, state, scene);
  return state.runtimeByAssetId[scene.assetId];
}

function stageAssets(runtime) {
  const frame = runtime.states.find(item => item.background && item.background.layers && item.background.layers.length);
  return frame ? frame.background.layers.map(layer => layer.assetId) : [];
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const rom = romFrom(z64.slice());
  const state = UI.initialize(rom);
  await state.authoredReady;

  // 1. A class-1 mode-two scene (01FA8BF4, army-dependent cast) and a class-8
  // scene with a runtime observation offer the 80-row environment table.
  const cases = [
    // Targets with a foreground (occlusion) piece: 57 draws archive 49 and
    // 51 draws archive 45 over the Actors. 01FA8BF4 previews with its sample cast.
    { sceneId: 'scene:director:01fa8bf4', route: 'native-environment', original: 34, targetSelector: 57,
      foreground: ['archive:49'], runtimeCheck: true },
    { sceneId: 'scene:director:01f3ead2', route: 'native-environment', original: 57, targetSelector: 51,
      foreground: ['archive:45'], runtimeCheck: true },
    { sceneId: 'rom-custom-lz:01F3E836', route: 'native-scene-group', original: 10, runtimeCheck: true }
  ];
  for (const item of cases) {
    const scene = item.scene = await open(rom, state, item.sceneId);
    const projection = present(state, scene).background.projection;
    assert.strictEqual(projection.route, item.route, item.sceneId + ' route');
    assert.strictEqual(projection.nativeEditable, true);
    assert.strictEqual(projection.sourceSelector, item.original);
    assert.strictEqual(projection.selectedSelector, item.original);
    const groups = projection.nativeGroups;
    assert(groups.length > (item.route === 'native-environment' ? 30 : 10), item.sceneId + ' offers the table');
    assert(groups.every(group => group.selector >= 0 && group.selector < (item.route === 'native-environment' ? 80 : 31)));
    // Pick a row whose base image differs from the original.
    const originalBase = groups.find(group => group.selector === item.original).archiveAssetIds[0];
    item.target = Number.isInteger(item.targetSelector)
      ? groups.find(group => group.selector === item.targetSelector)
      : groups.find(group => group.selector !== item.original && group.archiveAssetIds[0] !== originalBase);
    UI.selectNativeBackground(rom, state, scene, item.target.selector);
    const background = present(state, scene).background;
    assert.strictEqual(background.projection.selectedSelector, item.target.selector);
    assert.strictEqual(background.capability, 'native');
    assert.deepStrictEqual(background.layers.map(layer => layer.assetId), item.target.archiveAssetIds);
    assert(background.layers.every(layer => layer.source.sourceKind === item.route));
    if (item.foreground) {
      assert.deepStrictEqual(background.layers.filter(layer => layer.role === 'foreground-mask').map(layer => layer.assetId),
        item.foreground, item.sceneId + ' carries the foreground of its selector');
    }

    // 2. The preview renders the chosen row, not the retail request.
    if (item.runtimeCheck) {
      const runtime = await replay(rom, state, scene);
      assert(runtime, item.sceneId + ' replays: ' + JSON.stringify(state.sourceErrors));
      const assets = stageAssets(runtime);
      assert.strictEqual(assets[0], item.target.archiveAssetIds[0], item.sceneId + ' previews the chosen background: ' + assets);
      if (item.route === 'native-environment') {
        const frame = runtime.states.find(entry => entry.background && entry.background.layers.length);
        assert.strictEqual(frame.background.environmentSelector, item.target.selector);
        assert.strictEqual(frame.background.foregroundSelector, item.target.selector, 'the foreground follows the environment');
        assert.deepStrictEqual(frame.background.layers.filter(layer => layer.role === 'foreground-mask').map(layer => layer.assetId),
          item.foreground, item.sceneId + ' previews the foreground over the Actors');
      }
    }
  }

  // 3. The scene-group route never writes a selector outside 0..30.
  const flashback = cases[2].scene;
  const bad = JSON.parse(JSON.stringify(present(state, flashback)));
  bad.background.projection.selectedSelector = 31;
  assert.throws(() => OB64.cutsceneCodec.compileSceneDocument(flashback, state.sourceByAssetId[flashback.assetId], bad),
    /0 through 30/);

  // 4. Export rewrites only the operand word, and a ROM-only reopen brings
  // each choice back as the scene's current background.
  const project = JSON.parse(JSON.stringify(OB64.cutsceneProject.collect(state)));
  const candidate = { z64: rom.z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  const reopenedRom = romFrom(candidate.z64.slice());
  const reopened = UI.initialize(reopenedRom);
  await reopened.authoredReady;
  for (const item of cases) {
    const scene = await open(reopenedRom, reopened, item.sceneId);
    const source = reopened.sourceByAssetId[scene.assetId];
    const retail = state.catalog.getScene(item.sceneId);
    const words = OB64.cutsceneCodec.wordsFromBytes(source.decodedBytes || source.bytes);
    const start = retail.launchProfile.background.requests[0].wordStart;
    assert.strictEqual(words[start], 0x80000006);
    assert.strictEqual(words[start + 1], item.target.selector, item.sceneId + ' exported operand');
    const projection = present(reopened, scene).background.projection;
    assert.strictEqual(projection.selectedSelector, item.target.selector, item.sceneId + ' reopens with its choice');
    assert.deepStrictEqual(present(reopened, scene).background.layers.map(layer => layer.assetId), item.target.archiveAssetIds);
    if (item.runtimeCheck) {
      const runtime = await replay(reopenedRom, reopened, scene);
      assert.strictEqual(stageAssets(runtime)[0], item.target.archiveAssetIds[0], item.sceneId + ' reopened preview');
    }
  }

  // 5. Project load restores the choices, and Restore original returns the
  // retail row.
  const loadRom = romFrom(z64.slice());
  const loadState = UI.initialize(loadRom);
  await loadState.authoredReady;
  const prepared = await OB64.cutsceneProject.prepareImport(loadRom, project, {});
  OB64.cutsceneProject.applyPrepared(loadState, prepared);
  for (const item of cases) {
    const scene = loadState.catalog.getScene(item.sceneId);
    assert.strictEqual(present(loadState, scene).background.projection.selectedSelector, item.target.selector,
      item.sceneId + ' Project load');
    UI.selectNativeBackground(loadRom, loadState, scene, item.original);
    assert.strictEqual(present(loadState, scene).background.projection.selectedSelector, item.original);
  }

  // 6. Routes the scene's own command does not own stay read-only and say why.
  const readOnly = [
    ['scene:director:01f516ac', /from the battlefield/],
    ['scene:director:01f3f192', /no background command\. It keeps the background already on screen from /],
    ['scene:director:01f3e500', /no background command\. It keeps whatever the game already shows/]
  ];
  for (const [sceneId, reason] of readOnly) {
    const scene = await open(rom, state, sceneId);
    assert.notStrictEqual(present(state, scene).background.projection.nativeEditable, true, sceneId + ' read-only');
    assert.match(UI.backgroundReadOnlyReason(state, scene, present(state, scene)), reason, sceneId);
    assert.throws(() => UI.selectNativeBackground(rom, state, scene, 1), reason);
  }

  // 7. A new scene uses its class-5 scene-group command.
  const created = await UI.createAuthoredScene(rom, state, 'Background check');
  await open(rom, state, created.sceneId);
  const createdProjection = present(state, created).background.projection;
  assert.strictEqual(createdProjection.route, 'native-scene-group');
  assert.strictEqual(createdProjection.selectedSelector, 21);

  // 8. A Project saved before background selection keeps its background
  // exactly as the older projection derived it. Import replaces only that
  // unchanged background with today's baseline and keeps the real edit; a real
  // preview-only background override is kept and stays preview-only.
  async function legacyProject(mutate) {
    const oldRom = romFrom(z64.slice());
    const old = UI.initialize(oldRom);
    await old.authoredReady;
    const scene = await open(oldRom, old, 'rom-director:01F93B88');
    const source = old.sourceByAssetId[scene.assetId];
    const legacy = OB64.cutsceneCodec.projectSceneDocument(scene, source, old.catalog, { legacyBackground: true }).document;
    assert.notStrictEqual(legacy.background.projection.route, present(old, scene).background.projection.route);
    OB64.cutsceneModel.execute(old.histories[scene.storageId], 'Saved before background selection', document => {
      document.background = JSON.parse(JSON.stringify(legacy.background));
      document.actors[0].initial.x += 1;
      if (mutate) mutate(document);
    });
    return { project: JSON.parse(JSON.stringify(OB64.cutsceneProject.collect(old))), x: present(old, scene).actors[0].initial.x };
  }
  async function importProject(project) {
    const newRom = romFrom(z64.slice());
    const fresh = UI.initialize(newRom);
    await fresh.authoredReady;
    OB64.cutsceneProject.applyPrepared(fresh, await OB64.cutsceneProject.prepareImport(newRom, project, {}));
    const scene = fresh.catalog.getScene('rom-director:01F93B88');
    return { rom: newRom, state: fresh, document: present(fresh, scene) };
  }
  const unchanged = await legacyProject(null);
  const migrated = await importProject(unchanged.project);
  assert.strictEqual(migrated.document.exportRequirements.capability, 'native', 'the old Project still exports natively');
  assert.strictEqual(migrated.document.actors[0].initial.x, unchanged.x, 'the real edit is kept');
  assert.strictEqual(migrated.document.background.projection.route, 'native-environment');
  const migratedTarget = { z64: migrated.rom.z64.slice(), layout: migrated.rom.layout };
  const migratedPlan = await OB64.cutsceneExport.prepare(migrated.rom, migratedTarget);
  OB64.cutsceneExport.apply(migratedTarget, migratedPlan);
  OB64.cutsceneExport.validateApplied(migratedTarget, migratedPlan);
  const override = await legacyProject(document => {
    document.background.assetId = 'archive:94';
    document.background.projection.previewOverride = true;
  });
  const kept = await importProject(override.project);
  assert.strictEqual(kept.document.background.assetId, 'archive:94', 'a real preview override is kept as saved');
  assert.notStrictEqual(kept.document.exportRequirements.capability, 'native', 'and stays preview-only');

  console.log('PASS Background selection: mode-two environments (classes 1 and 8, foreground follows) and class 4/5 scene groups ' +
    'select a complete table row; the preview renders it; export rewrites only the operand; ROM-only reopen and Project load ' +
    'restore it; Restore original; the 0..30 group bound; battlefield, inherited and missing commands explain why they are read-only; ' +
    'new scenes use their scene group; pre-feature Projects migrate only an unchanged background and keep real overrides.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
