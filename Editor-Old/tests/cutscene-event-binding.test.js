'use strict';

// Long-site event binding. A binding makes one exact retail launch site start
// another scene. Export rebuilds the bound event row from its verified retail
// preimage, rewrites only that site's selector bytes, relocates the row into
// the arena as a kind-2 record, and repoints the event directory. The retail
// row is never overwritten. Reopen reads bindings back by diffing against the
// retail preimage; unbinding restores the retail directory key.

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

const E = OB64.cutsceneEvents;
const A = OB64.cutsceneExport.arena;
const Au = OB64.cutsceneAuthored;
const pinned = OB64.cutsceneEventRows;
const u32 = (z64, at) => ((z64[at] << 24) | (z64[at + 1] << 16) | (z64[at + 2] << 8) | z64[at + 3]) >>> 0;
const OPENING = 'event-director:1:b0862';
const SHORT = 'event-director:1:b0032';

function romFrom(z64) { return { z64, layout: { id: 'us-rev0' }, archives: OB64.findArchives(z64) }; }

async function exportState(rom, state) {
  const candidate = { z64: rom.z64.slice(), layout: rom.layout };
  const plan = await OB64.cutsceneExport.prepare(rom, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  return { candidate, plan };
}

async function reopen(z64) {
  const rom = romFrom(z64.slice());
  const state = OB64.cutsceneUI.initialize(rom);
  await state.authoredReady;
  return { rom, state };
}

function relocatedRow(z64, row) {
  const record = A.read(z64).records.find(item => item[0] >>> 28 === A.KIND.EVENT_ROW && (item[0] & 0x0FFFFFFF) === row);
  assert(record, 'event row ' + row + ' has a kind-2 record');
  assert.strictEqual(u32(z64, E.directoryWordZ64(row)), record[1], 'the directory points at the relocated row');
  return OB64.cutsceneCodec.decodeCustomLz(z64.slice(record[2] + 4, record[2] + 4 + record[3]), { requireExact: true }).bytes;
}

(async () => {
  const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);

  // 1. The pinned table is the retail ROM: every populated row decodes to its
  // pinned hash, and the catalog's 1,998 sites split into 1,596 long and 402 short.
  const { rom, state } = await reopen(z64);
  const catalog = state.catalog;
  for (let row = 0; row < pinned.rowCount; row++) {
    if (pinned.rows[row]) await E.retailRow(z64, row);
  }
  const sites = E.siteIndex(catalog).list;
  assert.strictEqual(sites.length, 1998);
  assert.deepStrictEqual([sites.filter(site => site.form === 'long').length, sites.filter(site => site.form === 'short').length], [1596, 402]);
  const opening = E.site(catalog, OPENING);
  assert.deepStrictEqual([opening.row, opening.offset, opening.selector, opening.form], [1, 0x862, 970, 'long']);
  assert.strictEqual(E.site(catalog, SHORT).form, 'short');
  assert(opening.shared, 'row 1 is flagged as a shared program: ' + opening.role);
  assert.deepStrictEqual(state.eventBindings, [], 'a retail ROM has no bindings');

  // 2. Validation refuses unknown sites, repeats, no-op targets, rows
  // that do not launch the named scene, and new scenes that are not open.
  const traveling = catalog.getScene(Au.CLASS5_TEMPLATE);
  const travelingRow = traveling.source.directorSelectorRows[0];
  const openingScene = catalog.getScene(opening.retailSceneId);
  const physical = { kind: 'physical', sceneId: traveling.sceneId, selectorRow: travelingRow };
  assert.throws(() => E.normalize(catalog, [{ launchSiteId: 'event-director:1:b9999', target: physical }]), /not in the retail catalog/);
  assert.throws(() => E.normalize(catalog, [{ launchSiteId: OPENING, target: physical }, { launchSiteId: OPENING, target: physical }]), /bound twice/);
  assert.throws(() => E.normalize(catalog, [{ launchSiteId: OPENING,
    target: { kind: 'physical', sceneId: openingScene.sceneId, selectorRow: 970 } }]), /already launches that scene/);
  assert.throws(() => E.normalize(catalog, [{ launchSiteId: OPENING,
    target: { kind: 'physical', sceneId: traveling.sceneId, selectorRow: 970 } }]), /does not launch that scene/);
  assert.throws(() => E.normalize(catalog, [{ launchSiteId: OPENING,
    target: { kind: 'authored', authoredId: '0'.repeat(32) } }]), /not open/);

  // 3. Bind the Opening Title site to a new scene and export.
  const scene = await OB64.cutsceneUI.createAuthoredScene(rom, state, 'Bound scene');
  const authoredTarget = { kind: 'authored', authoredId: scene.authored.authoredId };
  state.eventBindings = [{ launchSiteId: OPENING, target: authoredTarget }];
  assert.strictEqual(OB64.cutsceneProject.bindingsDifferFromRom(state), true);
  const first = await exportState(rom, state);
  const retail = await E.retailRow(z64, 1);
  const bound = relocatedRow(first.candidate.z64, 1);
  const differences = [];
  for (let at = 0; at < retail.length; at++) if (retail[at] !== bound[at]) differences.push(at);
  assert.strictEqual(bound.length, retail.length);
  assert.deepStrictEqual(differences, [0x85F, 0x861].filter(at => retail[at] !== bound[at]),
    'only the site selector bytes change');
  assert.strictEqual(bound[0x85F] | (bound[0x861] << 8), scene.authored.selectorRow, 'the site launches the new scene row');
  const retailPrefix = pinned.resourceBaseZ64 + pinned.rows[1].key;
  assert.deepStrictEqual(first.candidate.z64.slice(retailPrefix, retailPrefix + 4 + pinned.rows[1].storedLength),
    z64.slice(retailPrefix, retailPrefix + 4 + pinned.rows[1].storedLength), 'the retail row bytes stay readable and unchanged');
  const manifest = A.readManifest(first.candidate.z64, A.read(first.candidate.z64).records.find(row => row[0] >>> 28 === A.KIND.MANIFEST));
  assert.deepStrictEqual(manifest.eventBindings, [{ launchSiteId: OPENING, target: authoredTarget }]);
  assert(first.plan.entries.some(entry => entry.result.placement === 'event-row' && entry.result.eventRow === 1));

  // 4. ROM-only reopen reads the binding back and counts nothing as changed.
  const second = await reopen(first.candidate.z64);
  assert.deepStrictEqual(second.state.authoredDiscovery, { status: 'ok', count: 1 });
  assert.deepStrictEqual(second.state.eventBindings, [{ launchSiteId: OPENING, target: authoredTarget }]);
  assert.strictEqual(OB64.cutsceneUI.editCount(second.state), 0, 'a reopened binding is not an edit');
  assert.strictEqual(OB64.cutsceneProject.collect(second.state), null);

  // 5. Re-export is deterministic from retail: the same binding set gives the
  // same row. A second site in the row, bound to a retail scene, adds its own
  // selector change only.
  const again = await exportState(second.rom, second.state);
  assert.deepStrictEqual(Array.from(relocatedRow(again.candidate.z64, 1)), Array.from(bound), 'rebinding rebuilds from retail');
  const otherSite = sites.find(site => site.row === 1 && site.form === 'long' && site.launchSiteId !== OPENING &&
    site.selector !== travelingRow);
  second.state.eventBindings = [{ launchSiteId: OPENING, target: authoredTarget }, { launchSiteId: otherSite.launchSiteId, target: physical }];
  const twoSites = await exportState(second.rom, second.state);
  const twoBound = relocatedRow(twoSites.candidate.z64, 1);
  assert.strictEqual(twoBound[otherSite.offset - 3] | (twoBound[otherSite.offset - 1] << 8), travelingRow);
  const third = await reopen(twoSites.candidate.z64);
  assert.deepStrictEqual(third.state.eventBindings.map(binding => binding.launchSiteId).sort(), [OPENING, otherSite.launchSiteId].sort());
  assert.deepStrictEqual(third.state.eventBindings.find(binding => binding.launchSiteId === otherSite.launchSiteId).target, physical);
  assert.strictEqual(A.read(twoSites.candidate.z64).records.filter(row => row[0] >>> 28 === A.KIND.EVENT_ROW).length, 1,
    'one record per row, never one per export');

  // 6. Unbinding everything restores the retail directory key and releases the
  // record; the new scene stays; reopen finds no bindings.
  third.state.eventBindings = [];
  const unbound = await exportState(third.rom, third.state);
  assert.strictEqual(u32(unbound.candidate.z64, E.directoryWordZ64(1)), pinned.rows[1].key);
  assert.strictEqual(A.read(unbound.candidate.z64).records.filter(row => row[0] >>> 28 === A.KIND.EVENT_ROW).length, 0);
  assert(unbound.plan.entries.some(entry => entry.result.placement === 'event-row-restore'));
  const unboundManifest = A.readManifest(unbound.candidate.z64, A.read(unbound.candidate.z64).records.find(row => row[0] >>> 28 === A.KIND.MANIFEST));
  assert.strictEqual(unboundManifest.eventBindings, undefined);
  assert.strictEqual(unboundManifest.authoredScenes.length, 1);
  const fourth = await reopen(unbound.candidate.z64);
  assert.deepStrictEqual(fourth.state.eventBindings, []);
  assert.deepStrictEqual(fourth.state.authoredDiscovery, { status: 'ok', count: 1 });

  // 7. A binding to a retail scene alone, with no new scene: the manifest holds
  // only bindings, and reopen resolves the retail target.
  const plain = await reopen(z64);
  plain.state.eventBindings = [{ launchSiteId: OPENING, target: physical }];
  const physicalOnly = await exportState(plain.rom, plain.state);
  const physicalManifest = A.readManifest(physicalOnly.candidate.z64,
    A.read(physicalOnly.candidate.z64).records.find(row => row[0] >>> 28 === A.KIND.MANIFEST));
  assert.deepStrictEqual(physicalManifest.authoredScenes, []);
  const fifth = await reopen(physicalOnly.candidate.z64);
  assert.deepStrictEqual(fifth.state.eventBindings, [{ launchSiteId: OPENING, target: physical }]);

  // 8. Fail closed: a directory row changed outside the editor fails discovery
  // (and so export), and a changed retail preimage refuses to rebuild its row.
  const foreign = physicalOnly.candidate.z64.slice();
  foreign.set([0, 0, 0, 2], E.directoryWordZ64(5));
  const foreignOpen = await reopen(foreign);
  assert.strictEqual(foreignOpen.state.authoredDiscovery.status, 'failed');
  assert(/changed outside Cutscene Studio/.test(foreignOpen.state.authoredDiscovery.message), foreignOpen.state.authoredDiscovery.message);
  await assert.rejects(OB64.cutsceneExport.prepare(foreignOpen.rom, { z64: foreign.slice(), layout: rom.layout }), /could not be verified/);
  const damaged = z64.slice();
  damaged[retailPrefix + 40] ^= 0xFF;
  const damagedOpen = await reopen(damaged);
  damagedOpen.state.eventBindings = [{ launchSiteId: OPENING, target: physical }];
  await assert.rejects(OB64.cutsceneExport.prepare(damagedOpen.rom, { z64: damaged.slice(), layout: rom.layout }), /Retail event row 1 has been changed/);

  // 8b. The export re-verifies the candidate's event rows, not only what was
  // read at open: a relocated row changed afterwards (same envelope, still
  // valid LZ, one byte off its retail diff) is refused, never overwritten.
  const record = A.read(first.candidate.z64).records.find(row => row[0] >>> 28 === A.KIND.EVENT_ROW);
  const payload = first.candidate.z64.slice(record[2] + 4, record[2] + 4 + record[3]);
  let tampered = null;
  for (let at = 4; at < payload.length && !tampered; at++) {
    const next = payload.slice();
    next[at] ^= 1;
    try {
      const decoded = OB64.cutsceneCodec.decodeCustomLz(next, { requireExact: true }).bytes;
      if (decoded.length === bound.length && decoded.some((value, index) => value !== bound[index] &&
          index !== 0x85F && index !== 0x861)) tampered = next;
    } catch (error) { /* not a valid stream; try the next byte */ }
  }
  assert(tampered, 'found a same-envelope valid mutation');
  const opened = await reopen(first.candidate.z64);
  const changedCandidate = { z64: first.candidate.z64.slice(), layout: rom.layout };
  changedCandidate.z64.set(tampered, record[2] + 4);
  const beforeRefusal = changedCandidate.z64.slice();
  await assert.rejects(OB64.cutsceneExport.prepare(opened.rom, changedCandidate), /event rows could not be verified/);
  assert.deepStrictEqual(changedCandidate.z64, beforeRefusal, 'a refused export leaves the candidate untouched');
  opened.state.eventBindings = [];
  await assert.rejects(OB64.cutsceneExport.prepare(opened.rom, { z64: beforeRefusal.slice(), layout: rom.layout }),
    /event rows could not be verified/, 'unbinding a changed row is refused too');
  const swapped = await reopen(physicalOnly.candidate.z64);
  await assert.rejects(OB64.cutsceneExport.prepare(swapped.rom, { z64: first.candidate.z64.slice(), layout: rom.layout }),
    /differ from the ones Cutscene Studio opened|could not be verified/, 'a candidate that is not the opened ROM is refused');

  // 9. Projects: a changed binding set is saved whole with schema 2, restored
  // onto a retail ROM together with its new scene, and refused when a binding
  // names an unknown site or a new scene the Project does not hold.
  const project = OB64.cutsceneProject.collect(state);
  assert.strictEqual(project.schemaVersion, 2);
  assert.deepStrictEqual(project.eventBindings, [{ launchSiteId: OPENING, target: authoredTarget }]);
  const restore = await reopen(z64);
  const prepared = await OB64.cutsceneProject.prepareImport(restore.rom, project, {});
  assert.strictEqual(OB64.cutsceneProject.applyPrepared(restore.state, prepared), 2, 'the new scene and the binding set');
  assert.deepStrictEqual(restore.state.eventBindings, project.eventBindings);
  const restoredExport = await exportState(restore.rom, restore.state);
  assert.deepStrictEqual(Array.from(relocatedRow(restoredExport.candidate.z64, 1)), Array.from(bound),
    'a restored Project exports the same bound row');
  const unknownProject = JSON.parse(JSON.stringify(project));
  unknownProject.eventBindings[0].launchSiteId = 'event-director:1:b9999';
  await assert.rejects(OB64.cutsceneProject.prepareImport((await reopen(z64)).rom, unknownProject, {}), /not in the retail catalog/);
  const orphan = JSON.parse(JSON.stringify(project));
  orphan.authoredScenes = [];
  orphan.selectedSceneId = traveling.sceneId;
  await assert.rejects(OB64.cutsceneProject.prepareImport((await reopen(z64)).rom, orphan, {}), /not open/);
  const clearing = await reopen(first.candidate.z64);
  clearing.state.eventBindings = [];
  const clearProject = OB64.cutsceneProject.collect(clearing.state);
  assert.deepStrictEqual(clearProject.eventBindings, [], 'removing every binding is saved as an empty set');

  // 10. Short sites: the four site bytes become a long jump to a ten-byte stub
  // appended after the retail program, which launches the new selector and
  // jumps back. A long site in the same row still changes in place.
  const shortRun = await reopen(z64);
  const shortScene = await OB64.cutsceneUI.createAuthoredScene(shortRun.rom, shortRun.state, 'Short site scene');
  const shortTarget = { kind: 'authored', authoredId: shortScene.authored.authoredId };
  shortRun.state.eventBindings = [{ launchSiteId: SHORT, target: shortTarget }, { launchSiteId: OPENING, target: physical }];
  const shortExport = await exportState(shortRun.rom, shortRun.state);
  const withStub = relocatedRow(shortExport.candidate.z64, 1);
  const siteAt = E.site(catalog, SHORT).offset, S = retail.length;
  assert.strictEqual(withStub.length, retail.length + 10, 'one ten-byte stub is appended');
  const out = ((S - (siteAt + 2)) / 2) & 0xFFFF, back = ((siteAt + 2 - (S + 10)) / 2) & 0xFFFF;
  assert.deepStrictEqual(Array.from(withStub.slice(siteAt - 2, siteAt + 2)), [0x68, out & 0xFF, 0x02, out >>> 8], 'the site jumps to its stub');
  assert.deepStrictEqual(Array.from(withStub.slice(S, S + 10)), [0x68, shortScene.authored.selectorRow & 0xFF, 0x88,
    shortScene.authored.selectorRow >>> 8, 0x10, retail[siteAt + 1], 0x68, back & 0xFF, 0x02, back >>> 8],
    'the stub launches the new scene with the retail operand, then returns after the site');
  assert.strictEqual(withStub[0x85F] | (withStub[0x861] << 8), travelingRow, 'the long site in the same row changes in place');
  for (let at = 0; at < retail.length; at++) {
    if (withStub[at] !== retail[at]) assert([siteAt - 1, siteAt, siteAt + 1, 0x85F, 0x861].includes(at), 'no other retail byte changes: ' + at);
  }
  const shortOpen = await reopen(shortExport.candidate.z64);
  assert.deepStrictEqual(shortOpen.state.eventBindings.map(binding => binding.launchSiteId).sort(), [OPENING, SHORT].sort());
  assert.deepStrictEqual(shortOpen.state.eventBindings.find(binding => binding.launchSiteId === SHORT).target, shortTarget);
  // siteAt changed stub byte is refused as an outside change.
  const badStub = withStub.slice();
  badStub[S + 8] ^= 1;
  assert.throws(() => E.diffRow(catalog, 1, retail, badStub), /changed outside Cutscene Studio/);
  const extraBytes = new Uint8Array(withStub.length + 10);
  extraBytes.set(withStub);
  assert.throws(() => E.diffRow(catalog, 1, retail, extraBytes), /changed outside Cutscene Studio/);
  // Unbinding the short site removes its stub; the long binding stays.
  shortOpen.state.eventBindings = shortOpen.state.eventBindings.filter(binding => binding.launchSiteId !== SHORT);
  const noStub = await exportState(shortOpen.rom, shortOpen.state);
  const longOnly = relocatedRow(noStub.candidate.z64, 1);
  assert.strictEqual(longOnly.length, retail.length);
  assert.strictEqual(longOnly[siteAt - 1], retail[siteAt - 1]);

  // 11. Preview context: a scene bound to a site previews with that site's
  // retail invocation contexts, marked as assigned; the retail scene that site
  // used to launch keeps a usable context.
  const contextRun = await reopen(z64);
  const contextScene = await OB64.cutsceneUI.createAuthoredScene(contextRun.rom, contextRun.state, 'Context scene');
  await OB64.cutsceneUI.loadScene(contextRun.rom, contextRun.state, contextScene);
  OB64.cutsceneUI.assignLaunchSite(contextRun.rom, contextRun.state, contextScene, OPENING);
  const boundChoices = OB64.cutsceneUI.launchContextChoices(contextRun.state, contextScene);
  assert(boundChoices.length && boundChoices.every(choice => choice.bound && choice.launch.launchId === OPENING),
    'the bound scene offers the site contexts: ' + JSON.stringify(boundChoices.map(choice => choice.label)));
  assert(/^Assigned · Event row 1 · byte 0x0862/.test(boundChoices[0].label));
  const openingRetail = contextRun.state.catalog.getScene(E.site(catalog, OPENING).retailSceneId);
  assert(OB64.cutsceneUI.launchContextChoices(contextRun.state, openingRetail).length > 0,
    'the retail scene keeps a context when its only site is rebound');
  delete contextRun.state.runtimeByAssetId[contextScene.assetId];
  await OB64.cutsceneUI.loadScene(contextRun.rom, contextRun.state, contextScene);
  const contextRuntime = contextRun.state.runtimeByAssetId[contextScene.assetId];
  assert(contextRuntime, JSON.stringify(contextRun.state.sourceErrors));
  assert.strictEqual(contextRuntime.states.at(-1).runtime.terminalReason, 'terminal-state-release');
  assert(String(contextRun.state.views[contextScene.sceneId].launchContextId).startsWith(OPENING),
    'the preview runs with the assigned site context');

  // 12. Rebound predecessors: Traveling Cutscene 3 (site b08A8) follows
  // Traveling Cutscene 2 (site b0890). Binding b0890 to a new scene makes that
  // scene Traveling 3's predecessor in its preview contexts.
  const chainRun = await reopen(z64);
  const replacement = await OB64.cutsceneUI.createAuthoredScene(chainRun.rom, chainRun.state, 'Replacement predecessor');
  const traveling3 = chainRun.state.catalog.getScene(Au.CLASS5_TEMPLATE);
  const before = OB64.cutsceneUI.launchContextChoices(chainRun.state, traveling3)
    .filter(choice => choice.context.precedingDirectorLaunchId === 'event-director:1:b0890');
  assert(before.length && before.every(choice => choice.context.precedingDirectorSelector === 972),
    'retail: Traveling 3 follows selector 972 at b0890');
  chainRun.state.selectedSceneId = replacement.sceneId;
  OB64.cutsceneUI.assignLaunchSite(chainRun.rom, chainRun.state, replacement, 'event-director:1:b0890');
  const after = OB64.cutsceneUI.launchContextChoices(chainRun.state, traveling3)
    .filter(choice => choice.context.precedingDirectorLaunchId === 'event-director:1:b0890');
  assert(after.length === before.length && after.every(choice => choice.context.precedingDirectorSelector === replacement.authored.selectorRow &&
    choice.contextScene && choice.contextScene.sceneId === replacement.sceneId && /after Replacement predecessor/.test(choice.label)),
    'bound: the replacement is the predecessor: ' + JSON.stringify(after.map(choice => choice.label)));

  // 13. The Wipe confirmation names the effective sites of a stream: a site
  // bound to it counts, a retail site bound away does not.
  const shareRun = await reopen(z64);
  const openingTitle = shareRun.state.catalog.getScene(E.site(catalog, OPENING).retailSceneId);
  const shareTarget = await OB64.cutsceneUI.createAuthoredScene(shareRun.rom, shareRun.state, 'Takes the opening site');
  const extraSite = sites.find(site => site.form === 'long' && site.retailSceneId !== openingTitle.sceneId &&
    site.selector !== openingTitle.source.directorSelectorRows[0]);
  shareRun.state.eventBindings = [
    { launchSiteId: OPENING, target: { kind: 'authored', authoredId: shareTarget.authored.authoredId } },
    { launchSiteId: extraSite.launchSiteId, target: { kind: 'physical', sceneId: openingTitle.sceneId, selectorRow: openingTitle.source.directorSelectorRows[0] } }
  ];
  assert.deepStrictEqual(OB64.cutsceneUI.wipeSharers(shareRun.state, openingTitle).sites.map(site => site.launchSiteId), [extraSite.launchSiteId]);

  console.log('PASS long-site event binding: pinned retail rows, 1,596/402 site forms, validation refusals, bind to a new scene with only the selector bytes changed and the retail row intact, ROM-only rediscovery, deterministic rebind, a second site to a retail scene, unbind to the retail key, a bindings-only manifest, fail-closed outside changes and damaged preimages, Project save/restore, and short-site stubs with rediscovery, tamper refusal and removal, bound-site preview contexts, and rebound predecessors in dependent contexts.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
