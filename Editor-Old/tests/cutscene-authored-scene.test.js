'use strict';

// A genuinely new scene: its own random identity, one retail-zero selector row,
// a validated class-5 scaffold that ends with the native Director exit, and a
// kind-1 arena record plus manifest on export. The empty scene must complete
// through terminal-state-release, accept Actors and actions only after its
// camera setup, and export with its selector row and manifest intact.

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
  'cutscene-codec.js', 'cutscene-authored.js', 'cutscene-authoring.js', 'cutscene-dialogue-data.js',
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

const M = OB64.cutsceneModel;
const Au = OB64.cutsceneAuthored;
const A = OB64.cutsceneExport.arena;
const u32 = (z64, at) => ((z64[at] << 24) | (z64[at + 1] << 16) | (z64[at + 2] << 8) | z64[at + 3]) >>> 0;
const wordsOf = bytes => Array.from({ length: bytes.length / 4 }, (_, i) => u32(bytes, i * 4));

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
  const pristine = z64.slice();
  const rom = { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) };
  const state = OB64.cutsceneUI.initialize(rom);
  const catalog = state.catalog;

  // 1. Free rows are retail-zero and unclaimed; identity is random and validated.
  const free = Au.freeSelectorRows(z64, catalog, []);
  assert.strictEqual(free.length, 145, 'all 145 retail-zero selector rows are free in a retail ROM');
  assert(free.every(row => row >= 319 && u32(z64, Au.directorTableZ64 + row * 4) === 0));
  const claimedRow = catalog.getScene(Au.CLASS5_TEMPLATE).source.directorSelectorRows[0];
  await assert.rejects(Au.create(z64, catalog, { selectorRow: claimedRow }), /already belongs/);
  await assert.rejects(Au.create(z64, catalog, { selectorRow: free[0], authoredId: 'not-random' }), /32 lowercase hex/);
  const idA = Au.randomId(), idB = Au.randomId();
  assert(Au.validId(idA) && Au.validId(idB) && idA !== idB, 'identities are full random 128-bit ids');

  // 2. The scaffold is the template's validated setup plus the native exit tail.
  // The editor freezes it and the Actor recipe; both must equal what the retail
  // template (named by its decoded SHA-256) yields, so they never drift.
  const templateSource = await OB64.cutsceneCodec.loadSceneSource(z64, catalog.getScene(Au.CLASS5_TEMPLATE), {});
  assert.strictEqual(templateSource.decodedSha256, Au.CLASS5_TEMPLATE_RETAIL_SHA256);
  assert.deepStrictEqual(Au.CLASS5_SCAFFOLD_WORDS, wordsOf(Au.scaffoldFromTemplate(templateSource.decodedBytes)));
  const derivedRecipe = Au.actorRecipe(templateSource.decodedBytes);
  delete derivedRecipe.provenance;
  const frozenRecipe = Object.assign({}, Au.CLASS5_ACTOR_RECIPE);
  delete frozenRecipe.provenance;
  assert.deepStrictEqual(frozenRecipe, derivedRecipe, 'the frozen Actor recipe is the retail template recipe');
  const made = await Au.create(z64, catalog, { name: 'Authored test scene', selectorRow: free[0], authoredId: idA });
  const words = wordsOf(made.source.decodedBytes);
  assert.strictEqual(words.length, 23);
  assert.deepStrictEqual(words.slice(-5), Au.CLASS5_EXIT_TAIL, 'the scaffold keeps the native Director exit');
  assert.deepStrictEqual([words[0], words[2], words[10]], [0x80000006, 0x36, 0x35]);
  const scene = catalog.registerAuthoredScene(made.scene);
  assert.throws(() => catalog.registerAuthoredScene(Object.assign({}, made.scene, { authored: null })), /authored identity/);
  assert.strictEqual(scene.source.provenance, 'authored', 'the synthetic entry does not claim to be a retail resource');
  assert.strictEqual(scene.directorKey, null);
  assert.deepStrictEqual(scene.launchProfile.parentEventLaunches, [], 'a new scene has no launch until it is bound');
  assert.deepStrictEqual(Au.freeSelectorRows(z64, catalog, [{ authoredId: idA, selectorRow: free[0] }]).includes(free[0]), false);

  // 3. Insertion: never inside the setup; the native exit is the insertion point.
  const inserts = scene.source.nodes.filter(node => node.insertBefore);
  assert.deepStrictEqual(inserts.map(node => [node.startWord, node.opcodeU32]), [[18, 0x7D]],
    'the only insertion point of an empty authored scene is its native exit');
  assert(scene.source.nodes.filter(node => node.startWord < 18).every(node => !node.insertBefore));

  // 4. The empty scene previews and completes through the native exit.
  state.selectedSceneId = scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  const empty = state.runtimeByAssetId[scene.assetId];
  assert(empty, JSON.stringify(state.sourceErrors));
  assert.strictEqual(empty.states.at(-1).runtime.terminalReason, 'terminal-state-release',
    'the empty scene ends by releasing the Director, not by holding');
  assert.deepStrictEqual(pristine, z64, 'preview playback never writes the loaded ROM');

  // 5. The real "+ Actor" and "Add hold" actions: the Actor uses the template's
  // visible recipe (creation, then its own 0x1C channel and 0x1D scale), and
  // the rendered Stage shows its sprite inside the frame with a usable hit region.
  await OB64.cutsceneUI.addActor(rom, state);
  await OB64.cutsceneUI.addHold(rom, state);
  const document = OB64.cutsceneUI.selectedDocument(state);
  assert.strictEqual(document.actors.length, 1);
  const added = document.actors[0];
  assert.deepStrictEqual([added.slot, added.artSourceId, added.initial.z, added.source.renderMode, added.source.presentation],
    [0, 'cutscene-art-bank:30', 53, 2, { transformChannel: 1, uniformScalePercent: 28 }],
    'the added Actor copies the template recipe');
  delete state.runtimeByAssetId[scene.assetId];
  await OB64.cutsceneUI.loadScene(rom, state, scene);
  const edited = state.runtimeByAssetId[scene.assetId];
  assert(edited, JSON.stringify(state.sourceErrors));
  assert(edited.states.length > 30, 'the Hold keeps the scene running before the exit');
  assert.strictEqual(edited.states.at(-1).runtime.terminalReason, 'terminal-state-release');
  const compiled = OB64.cutsceneCodec.compileSceneDocument(scene, state.sourceByAssetId[scene.assetId], document);
  const compiledWords = wordsOf(compiled.decodedBytes);
  assert.deepStrictEqual(compiledWords.slice(0, 18), words.slice(0, 18), 'the setup prefix is unchanged by edits');
  assert.deepStrictEqual(compiledWords.slice(18, 34), [0x14, 0, 30, 442, 2, -19000 >>> 0, 0, 53000, 2, 4, 0x1C, 0, 1, 0x1D, 0, 28],
    'the Actor compiles to its creation followed by its own channel and scale');
  assert.deepStrictEqual(compiledWords.slice(-5), Au.CLASS5_EXIT_TAIL, 'edits never move past the native exit');

  async function renderedActor(targetRom, targetState, targetScene, tick) {
    const targetDocument = targetState.histories[targetScene.storageId].present;
    const preview = OB64.cutscenePreview.evaluateAtFrame(targetDocument, tick, {});
    const withActors = await OB64.cutsceneUI.capturePreviewFrame(targetRom, targetState, targetDocument,
      { preview, backgroundPolicy: 'require', pass: tick, nodeId: 'test' });
    const without = await OB64.cutsceneUI.capturePreviewFrame(targetRom, targetState, targetDocument,
      { preview: Object.assign({}, preview, { actors: preview.actors.map(actor => Object.assign({}, actor, { visible: false })) }),
        backgroundPolicy: 'require', pass: tick, nodeId: 'test-bare' });
    let pixels = 0;
    for (let at = 0; at < withActors.rgba.length; at += 4) {
      if (withActors.rgba[at] !== without.rgba[at] || withActors.rgba[at + 1] !== without.rgba[at + 1] ||
          withActors.rgba[at + 2] !== without.rgba[at + 2]) pixels++;
    }
    return { pixels, hits: withActors.hitRegions };
  }
  const shown = await renderedActor(rom, state, scene, 10);
  assert(shown.pixels > 500, 'the added Actor draws a substantial sprite: ' + shown.pixels);
  const hit = shown.hits.find(region => region.actorId === added.id);
  assert(hit && hit.left >= 0 && hit.top >= 0 && hit.right <= 320 && hit.bottom <= 240,
    'the added Actor has a hit region inside the Stage: ' + JSON.stringify(shown.hits));

  // 6. Export: a kind-1 record owns the selector row; one manifest names it.
  const candidate = { z64: z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare({ z64, layout: rom.layout, cutsceneStudio: state }, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  const arena = A.read(candidate.z64);
  assert.strictEqual(arena.version, 2, 'authored records need the kind nibble');
  const authoredRecord = arena.records.find(row => row[0] >>> 28 === A.KIND.AUTHORED);
  assert.strictEqual(authoredRecord[0] & 0x0FFFFFFF, free[0]);
  assert.strictEqual(u32(candidate.z64, Au.directorTableZ64 + free[0] * 4), authoredRecord[1],
    'the selector row points at the authored stream');
  const exportedStream = OB64.cutsceneCodec.decodeCustomLz(
    candidate.z64.slice(authoredRecord[2] + 4, authoredRecord[2] + 4 + authoredRecord[3]), { requireExact: true }).bytes;
  assert.deepStrictEqual(Array.from(exportedStream), Array.from(compiled.decodedBytes));
  const manifestRow = arena.records.find(row => row[0] >>> 28 === A.KIND.MANIFEST);
  assert.deepStrictEqual(A.readManifest(candidate.z64, manifestRow).authoredScenes, [{
    authoredId: idA, name: 'Authored test scene', selectorRow: free[0], sceneClass: 5,
    templateSceneId: Au.CLASS5_TEMPLATE }]);
  assert.deepStrictEqual(pristine, z64, 'export planning and apply work on a detached candidate');

  // 7. Two authored scenes cannot share a row; a second scene takes its own.
  await assert.rejects(Au.create(z64, catalog, { selectorRow: free[0] }), /already belongs/);
  const second = await Au.create(z64, catalog, { name: 'Second', selectorRow: free[1], authoredId: idB });
  catalog.registerAuthoredScene(second.scene);
  const candidateTwo = { z64: z64.slice(), layout: rom.layout };
  const planTwo = await OB64.cutsceneExport.prepare({ z64, layout: rom.layout, cutsceneStudio: state }, candidateTwo);
  OB64.cutsceneExport.apply(candidateTwo, planTwo);
  OB64.cutsceneExport.validateApplied(candidateTwo, planTwo);
  const arenaTwo = A.read(candidateTwo.z64);
  assert.deepStrictEqual(arenaTwo.records.filter(row => row[0] >>> 28 === A.KIND.AUTHORED).map(row => row[0] & 0x0FFFFFFF),
    [free[0], free[1]]);
  assert.deepStrictEqual(A.readManifest(candidateTwo.z64, arenaTwo.records.find(row => row[0] >>> 28 === A.KIND.MANIFEST))
    .authoredScenes.map(entry => entry.authoredId), [idA, idB]);

  // 8. ROM-only reopen: a fresh editor on the exported ROM rediscovers both
  // scenes with the same identities, rows and names, and plays the edited one.
  const reopenedRom = { z64: candidateTwo.z64.slice(), layout: rom.layout, archives: OB64.findArchives(candidateTwo.z64) };
  const reopened = OB64.cutsceneUI.initialize(reopenedRom);
  assert.strictEqual(reopened.authoredDiscovery.status, 'pending');
  assert.throws(() => OB64.cutsceneProject.collect(reopened), /still reading new scenes/,
    'a Project cannot be saved before the authored scenes in the ROM are read');
  await reopened.authoredReady;
  assert.deepStrictEqual(reopened.authoredDiscovery, { status: 'ok', count: 2 });
  const found = reopened.catalog.authoredScenes().map(item => [item.authored.authoredId, item.authored.selectorRow, item.authored.name]);
  assert.deepStrictEqual(found, [[idA, free[0], 'Authored test scene'], [idB, free[1], 'Second']]);
  const reopenedScene = reopened.catalog.getScene('scene:authored:' + idA);
  assert(wordsOf(reopenedScene.authored.initialDecodedBytes).includes(0x14), 'the reopened stream carries the added Actor');
  reopened.selectedSceneId = reopenedScene.sceneId;
  await OB64.cutsceneUI.loadScene(reopenedRom, reopened, reopenedScene);
  const reopenedRuntime = reopened.runtimeByAssetId[reopenedScene.assetId];
  assert(reopenedRuntime, JSON.stringify(reopened.sourceErrors));
  assert.strictEqual(reopenedRuntime.states.at(-1).runtime.terminalReason, 'terminal-state-release');
  assert(reopenedRuntime.states.some(frame => frame.actors.some(actor => actor.slot === 0 && actor.visible)));
  const reshown = await renderedActor(reopenedRom, reopened, reopenedScene, 10);
  assert(reshown.pixels > 500 && reshown.hits.some(region => region.left >= 0 && region.right <= 320),
    'the reopened scene renders its Actor on the Stage');
  // Duplicating a reopened Actor (whose 0x1C/0x1D are now native records) keeps
  // the channel and scale it has in playback, so the copy is not a giant sprite.
  const reopenedActor = reopened.histories[reopenedScene.storageId].present.actors.find(actor => actor.slot === 0);
  await OB64.cutsceneUI.addActor(reopenedRom, reopened, reopenedActor);
  const duplicate = reopened.histories[reopenedScene.storageId].present.actors.find(actor => actor.slot !== 0);
  assert.deepStrictEqual(duplicate.source.presentation, { transformChannel: 1, uniformScalePercent: 28 },
    'the duplicate keeps the live channel and scale');
  delete reopened.runtimeByAssetId[reopenedScene.assetId];
  await OB64.cutsceneUI.loadScene(reopenedRom, reopened, reopenedScene);
  const duplicateShown = await renderedActor(reopenedRom, reopened, reopenedScene, 10);
  const duplicateHit = duplicateShown.hits.find(region => region.actorId === duplicate.id);
  assert(duplicateHit && duplicateHit.right - duplicateHit.left < 120 && duplicateHit.bottom - duplicateHit.top < 120,
    'the duplicate renders at the scene scale, not as a full-screen sprite: ' + JSON.stringify(duplicateHit));
  reopened.histories[reopenedScene.storageId] = M.createHistory(reopened.histories[reopenedScene.storageId].present);
  const withoutDuplicate = M.cloneSceneDocument(reopened.histories[reopenedScene.storageId].present);
  withoutDuplicate.actors = withoutDuplicate.actors.filter(actor => actor.id !== duplicate.id);
  reopened.histories[reopenedScene.storageId] = M.createHistory(withoutDuplicate);

  // 9. Edit again after reopen: records are rebuilt in place of the old ones,
  // never duplicated, and the manifest still lists exactly the two scenes.
  const again = M.cloneSceneDocument(reopened.histories[reopenedScene.storageId].present);
  const againExit = reopenedScene.source.nodes.find(node => node.opcodeU32 === 0x7D);
  let againFlow = again.tracks.find(track => track.type === 'flow');
  if (!againFlow) { againFlow = M.createTrack({ id: 'track:authored:flow', type: 'flow', actorId: null, label: 'Flow' }); again.tracks.push(againFlow); }
  againFlow.clips.push(M.createClip({ id: 'clip:authored:hold-again', kind: 'wait', startFrame: 2, durationFrames: 30,
    capability: 'native', payload: {}, source: { insertBeforeNodeId: againExit.id } }));
  reopened.histories[reopenedScene.storageId] = M.createHistory(again);
  const candidateThree = { z64: reopenedRom.z64.slice(), layout: rom.layout };
  const planThree = await OB64.cutsceneExport.prepare({ z64: reopenedRom.z64, layout: rom.layout, cutsceneStudio: reopened }, candidateThree);
  OB64.cutsceneExport.apply(candidateThree, planThree);
  OB64.cutsceneExport.validateApplied(candidateThree, planThree);
  const arenaThree = A.read(candidateThree.z64);
  assert.deepStrictEqual(arenaThree.records.map(row => row[0] >>> 28).sort(), [A.KIND.AUTHORED, A.KIND.AUTHORED, A.KIND.MANIFEST]);
  assert.deepStrictEqual(A.readManifest(candidateThree.z64, arenaThree.records.find(row => row[0] >>> 28 === A.KIND.MANIFEST))
    .authoredScenes.map(entry => [entry.authoredId, entry.selectorRow]), [[idA, free[0]], [idB, free[1]]]);

  // 10. A manifest that disagrees with the records fails discovery, and export
  // then refuses instead of guessing or dropping an owned scene.
  function corruptManifest(mutate) {
    const bytes = candidateThree.z64.slice();
    const row = A.read(bytes).records.find(record => record[0] >>> 28 === A.KIND.MANIFEST);
    const manifest = A.readManifest(bytes, row);
    mutate(manifest);
    // Rewrite the manifest in place, padded with JSON whitespace to its extent.
    const text = Buffer.from(JSON.stringify(manifest));
    assert(text.length <= row[3], 'the altered manifest fits its record extent');
    const padded = Buffer.alloc(row[3], 0x20);
    text.copy(padded);
    bytes.set(padded, row[2] + 4);
    return bytes;
  }
  let disagreementCases = 0;
  for (const [label, mutate] of [
    ['row mismatch', manifest => { manifest.authoredScenes[1].selectorRow = free[2]; }],
    ['missing entry', manifest => { manifest.authoredScenes.pop(); }]
  ]) {
    const bytes = corruptManifest(mutate);
    disagreementCases++;
    const brokenRom = { z64: bytes, layout: rom.layout, archives: OB64.findArchives(bytes) };
    const broken = OB64.cutsceneUI.initialize(brokenRom);
    // Export started before discovery finishes still waits for it and refuses.
    await assert.rejects(OB64.cutsceneExport.prepare({ z64: bytes, layout: rom.layout, cutsceneStudio: broken },
      { z64: bytes.slice(), layout: rom.layout }), error => error.code === 'authored-discovery', label + ' refuses an immediate export');
    await broken.authoredReady;
    assert.throws(() => OB64.cutsceneProject.collect(broken), /could not be verified/, label + ' refuses Project save');
    assert.strictEqual(broken.authoredDiscovery.status, 'failed', label + ' fails discovery');
    await assert.rejects(OB64.cutsceneExport.prepare({ z64: bytes, layout: rom.layout, cutsceneStudio: broken },
      { z64: bytes.slice(), layout: rom.layout }), error => error.code === 'authored-discovery', label + ' refuses export');
  }
  assert.strictEqual(disagreementCases, 2, 'both manifest disagreement cases ran');

  // 11. Project v2: saving writes schema 2 with each authored scene's identity,
  // initial stream and document; loading into a fresh retail session rebuilds
  // the same scenes, and loading onto the exported ROM reconciles with the
  // discovered scenes instead of duplicating them.
  const project = JSON.parse(JSON.stringify(OB64.cutsceneProject.collect(state)));
  assert.strictEqual(project.schemaVersion, 2);
  assert.deepStrictEqual(project.authoredScenes.map(entry => [entry.authoredId, entry.selectorRow, entry.name]),
    [[idA, free[0], 'Authored test scene'], [idB, free[1], 'Second']]);
  assert(project.authoredScenes[0].document, 'the opened scene saves its document');
  assert.strictEqual(project.authoredScenes[1].document, null, 'an unopened scene saves identity and stream only');
  const freshRom = { z64: pristine.slice(), layout: rom.layout, archives: OB64.findArchives(pristine) };
  const fresh = OB64.cutsceneUI.initialize(freshRom);
  const preparedFresh = await OB64.cutsceneProject.prepareImport(freshRom, project, {});
  OB64.cutsceneProject.applyPrepared(fresh, preparedFresh);
  const restored = fresh.catalog.getScene('scene:authored:' + idA);
  assert(restored && restored.authored.selectorRow === free[0]);
  assert.strictEqual(M.serializeSceneDocument(fresh.histories[restored.storageId].present, 0),
    M.serializeSceneDocument(state.histories[scene.storageId].present, 0), 'the saved document is restored exactly');
  assert(OB64.cutsceneProject.sceneChanged(fresh, restored), 'restored edits still count as edits over the scaffold');
  const reloadRom = { z64: candidateTwo.z64.slice(), layout: rom.layout, archives: OB64.findArchives(candidateTwo.z64) };
  const reload = OB64.cutsceneUI.initialize(reloadRom);
  await reload.authoredReady;
  const preparedReload = await OB64.cutsceneProject.prepareImport(reloadRom, project, {});
  OB64.cutsceneProject.applyPrepared(reload, preparedReload);
  assert.strictEqual(reload.catalog.authoredScenes().length, 2, 'loading onto the exported ROM does not duplicate scenes');
  const conflicting = JSON.parse(JSON.stringify(project));
  conflicting.authoredScenes[1].selectorRow = free[0];
  await assert.rejects(OB64.cutsceneProject.prepareImport(freshRom, conflicting, {}), /repeats an authored identity or selector row/);
  const moved = JSON.parse(JSON.stringify(project));
  moved.authoredScenes[0].selectorRow = free[5];
  await assert.rejects(OB64.cutsceneProject.prepareImport(reloadRom, moved, {}), /already open at selector row/);

  // 12. "+ New scene" creates, registers and selects a scene on the next free row.
  const uiScene = await OB64.cutsceneUI.createAuthoredScene(freshRom, fresh);
  assert.strictEqual(fresh.selectedSceneId, uiScene.sceneId);
  assert.strictEqual(uiScene.authored.selectorRow, free[2], 'the next free row after the restored scenes');
  OB64.cutsceneUI.renameAuthoredScene(freshRom, fresh, uiScene, 'Renamed scene');
  assert.strictEqual(OB64.cutsceneProject.collect(fresh).authoredScenes.find(entry => entry.authoredId === uiScene.authored.authoredId).name,
    'Renamed scene');

  // 13. Authored scenes do not depend on the editable template. Edit Traveling
  // Cutscene 3 (its first Actor's X), export it together with two new scenes,
  // then reopen: discovery succeeds, both scenes return, New scene still works.
  const indepSource = { z64: z64.slice(), layout: rom.layout, archives: rom.archives };
  const indep = OB64.cutsceneUI.initialize(indepSource);
  await indep.authoredReady;
  const indepTemplate = indep.catalog.getScene(Au.CLASS5_TEMPLATE);
  await OB64.cutsceneUI.loadScene(indepSource, indep, indepTemplate);
  const indepTemplateDocument = M.cloneSceneDocument(indep.histories[indepTemplate.storageId].present);
  indepTemplateDocument.actors[0].initial.x += 1;
  indep.histories[indepTemplate.storageId] = M.createHistory(indepTemplateDocument);
  const indepA = await OB64.cutsceneUI.createAuthoredScene(indepSource, indep, 'Mixed A');
  const indepB = await OB64.cutsceneUI.createAuthoredScene(indepSource, indep, 'Mixed B');
  const indepCandidate = { z64: z64.slice(), layout: rom.layout };
  const indepPlan = await OB64.cutsceneExport.prepare(indepSource, indepCandidate);
  OB64.cutsceneExport.apply(indepCandidate, indepPlan);
  OB64.cutsceneExport.validateApplied(indepCandidate, indepPlan);
  assert(indepPlan.entries.some(entry => entry.scene && entry.scene.sceneId === indepTemplate.sceneId && !entry.result.noOp),
    'the template edit is part of the export');
  const indepReopenRom = { z64: indepCandidate.z64.slice(), layout: rom.layout, archives: OB64.findArchives(indepCandidate.z64) };
  const indepReopen = OB64.cutsceneUI.initialize(indepReopenRom);
  await indepReopen.authoredReady;
  assert.deepStrictEqual(indepReopen.authoredDiscovery, { status: 'ok', count: 2 },
    'discovery does not read the edited template: ' + JSON.stringify(indepReopen.authoredDiscovery));
  assert.deepStrictEqual(indepReopen.catalog.authoredScenes().map(item => item.authored.authoredId).sort(),
    [indepA.authored.authoredId, indepB.authored.authoredId].sort());
  const indepC = await OB64.cutsceneUI.createAuthoredScene(indepReopenRom, indepReopen, 'Mixed C');
  assert.deepStrictEqual(wordsOf(indepC.authored.initialDecodedBytes), Au.CLASS5_SCAFFOLD_WORDS,
    'a new scene on the reopened ROM starts from the frozen scaffold');

  console.log('PASS authored scene identity, free-row selection, validated scaffold with native exit, setup-locked insertion, empty and edited preview through terminal-state-release, kind-1 plus manifest export, ROM-only reopen with the same identities, edit-again re-export, fail-closed manifest disagreement, Project v2 save/reload and reconcile, the New scene action, and independence from edits to the template scene.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
