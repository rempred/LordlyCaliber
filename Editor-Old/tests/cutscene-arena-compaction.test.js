'use strict';

// Cutscene arena version 2 and export-time compaction. Every export that needs
// the shared Cutscene arena packs the records it keeps from the arena start,
// repoints their selector/archive references through one relocation map, and
// rebuilds records it replaces, so repeated export/reopen cycles do not leak
// arena space. Kind-0 only arenas stay version 1; unknown or malformed arenas
// are refused before anything is written.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
const master = path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-model.js',
  'cutscene-catalog.js', 'cutscene-director.js', 'cutscene-codec.js', 'cutscene-authored.js', 'cutscene-authoring.js',
  'cutscene-project.js', 'cutscene-export.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}
const raw = fs.readFileSync(master);
const retail = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
const layout = { id: 'us-rev0' };
const A = OB64.cutsceneExport.arena;
const clips = doc => doc.tracks.flatMap(track => track.clips);
const u32 = (z64, at) => ((z64[at] << 24) | (z64[at + 1] << 16) | (z64[at + 2] << 8) | z64[at + 3]) >>> 0;

async function load(z64, assetId) {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scene = catalog.getScene(assetId);
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { allowModified: true });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  return { scene, source, baseline, document: structuredClone(baseline) };
}

async function plan(z64, rows) {
  const state = { catalog: { scenes: rows.map(row => row.scene) }, histories: {}, originalSerialized: {}, sourceByAssetId: {} };
  for (const row of rows) {
    state.histories[row.scene.storageId] = OB64.cutsceneModel.createHistory(row.document);
    state.originalSerialized[row.scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(row.baseline, 0);
    state.sourceByAssetId[row.scene.assetId] = row.source;
  }
  const candidate = { z64: z64.slice(), layout };
  const result = await OB64.cutsceneExport.prepare({ z64, layout, cutsceneStudio: state }, candidate);
  assert.deepStrictEqual(candidate.z64, z64, 'planning is detached');
  return { candidate, plan: result };
}

async function exportScenes(z64, rows) {
  const { candidate, plan: result } = await plan(z64, rows);
  OB64.cutsceneExport.apply(candidate, result);
  OB64.cutsceneExport.validateApplied(candidate, result);
  return { z64: candidate.z64, plan: result };
}

function addHolds(row, count, start = 0) {
  const boundary = row.scene.source.nodes.find(node => node.insertBefore && node.nodeType !== 'gap');
  let track = row.document.tracks.find(track => track.type === 'flow');
  if (!track) {
    track = OB64.cutsceneModel.createTrack({ id: 'track:authored:flow', type: 'flow', actorId: null, label: 'Flow' });
    row.document.tracks.push(track);
  }
  for (let index = start; index < count + start; index++) {
    track.clips.push(OB64.cutsceneModel.createClip({
      id: 'clip:authored:wait:' + index, kind: 'wait', startFrame: index,
      durationFrames: 1 + ((index * 7919) % 8000), capability: 'native', payload: {},
      source: { insertBeforeNodeId: boundary.id }
    }));
  }
}

function packedEnd(records) {
  let cursor = A.start;
  for (const row of records) {
    const entry = (cursor + 1) & ~1;
    assert.strictEqual(row[2], entry, 'records are packed from the arena start in order');
    cursor = (entry + 4 + row[3] + 1) & ~1;
  }
  return cursor;
}

function textFor(seedValue, length) {
  let seed = seedValue;
  return Array.from({ length }, () => {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    return String.fromCharCode(65 + seed % 26);
  }).join('');
}

(async () => {
  // 1. First export from retail: one relocated Director stream and one
  // relocated dialogue archive, recorded as kind-0 records in a version-1 arena.
  const graduation = await load(retail, 'loading-magnus-walk-opening-ceremony-cutscene');
  const dialogue = clips(graduation.document).find(clip => clip.payload.nativeDialogueEditable);
  const selector = dialogue.payload.presentationArchiveSelector;
  const entry = dialogue.payload.presentationEntrySelector;
  dialogue.payload.rawText += ' First change.' + textFor(43, 9000);
  const travel = await load(retail, 'rom-custom-lz:01FA4D0A');
  addHolds(travel, 100);
  const first = await exportScenes(retail, [graduation, travel]);
  const arenaOne = A.read(first.z64);
  assert.strictEqual(arenaOne.version, 1, 'a kind-0 only arena stays version 1');
  assert.strictEqual(arenaOne.records.length, 2);
  assert(arenaOne.records.every(row => row[0] >>> 28 === A.KIND.RETAIL));
  assert.strictEqual(arenaOne.dataEnd, packedEnd(arenaOne.records));
  const travelKeyOne = u32(first.z64, travel.scene.source.directorSelectorWordZ64[0]);

  // 2. Reopen and re-edit both resources: both old records are replaced and the
  // arena is rebuilt without the superseded bytes.
  const reopened = await load(first.z64, travel.scene.assetId);
  addHolds(reopened, 100, 1000);
  const textAgain = await load(first.z64, graduation.scene.assetId);
  clips(textAgain.document).find(clip => clip.payload.nativeDialogueEditable).payload.rawText += ' Another edit.';
  const second = await exportScenes(first.z64, [reopened, textAgain]);
  const arenaTwo = A.read(second.z64);
  assert.strictEqual(arenaTwo.records.length, 2, 'replaced records do not accumulate');
  assert.strictEqual(arenaTwo.dataEnd, packedEnd(arenaTwo.records), 'no superseded bytes remain in the arena');
  const grownTravel = await load(second.z64, travel.scene.assetId);
  assert(grownTravel.source.decodedBytes.length > reopened.source.decodedBytes.length);
  assert.strictEqual(OB64.cutsceneAuthoring.readDialogue(second.z64, selector).entries[entry].rawText,
    dialogue.payload.rawText + ' Another edit.');

  // 3a. Edit only the Director stream. Its record (first) is rebuilt at the end,
  // so the kept dialogue archive moves to the arena start and every archive
  // selector word is repointed from the same relocation map.
  const dialogueBefore = OB64.cutsceneAuthoring.readDialogue(second.z64, selector);
  const streamOnly = await load(second.z64, travel.scene.assetId);
  addHolds(streamOnly, 10, 2000);
  const third = await exportScenes(second.z64, [streamOnly]);
  const arenaThree = A.read(third.z64);
  assert.strictEqual(arenaThree.records.length, 2);
  assert.strictEqual(arenaThree.dataEnd, packedEnd(arenaThree.records));
  assert(third.plan.arenaReferenceWrites.length >= 1, 'the kept dialogue archive moved and its references were rewritten');
  const dialogueAfter = OB64.cutsceneAuthoring.readDialogue(third.z64, selector);
  assert.deepStrictEqual(Array.from(dialogueAfter.data), Array.from(dialogueBefore.data),
    'a kept relocated dialogue archive decodes unchanged after compaction');
  assert.strictEqual(dialogueAfter.selectorWords.length, dialogueBefore.selectorWords.length,
    'every archive alias points at the moved archive');

  // 3b. Edit only the dialogue text. Now the Director stream is the kept record
  // that moves to the start, with all of its selector aliases repointed.
  const travelBefore = await load(third.z64, travel.scene.assetId);
  const travelKeyBefore = u32(third.z64, travel.scene.source.directorSelectorWordZ64[0]);
  const textOnly = await load(third.z64, graduation.scene.assetId);
  clips(textOnly.document).find(clip => clip.payload.nativeDialogueEditable).payload.rawText += ' Third edit.';
  const fourth = await exportScenes(third.z64, [textOnly]);
  const arenaFour = A.read(fourth.z64);
  assert.strictEqual(arenaFour.records.length, 2);
  assert.strictEqual(arenaFour.dataEnd, packedEnd(arenaFour.records));
  assert(fourth.plan.arenaReferenceWrites.length >= 1, 'the kept stream moved and its references were rewritten');
  const travelAfter = await load(fourth.z64, travel.scene.assetId);
  assert.deepStrictEqual(Array.from(travelAfter.source.decodedBytes), Array.from(travelBefore.source.decodedBytes),
    'a kept relocated Director stream decodes unchanged after compaction');
  const travelKeyAfter = u32(fourth.z64, travel.scene.source.directorSelectorWordZ64[0]);
  assert.notStrictEqual(travelKeyAfter, travelKeyBefore);
  assert.strictEqual(A.referenceWords(fourth.z64, travelKeyAfter).length,
    travel.scene.source.directorSelectorWordZ64.length, 'every selector alias points at the moved stream');
  assert.strictEqual(OB64.cutsceneAuthoring.readDialogue(fourth.z64, selector).entries[entry].rawText,
    dialogue.payload.rawText + ' Another edit. Third edit.');

  // 4. A dialogue archive past its 64 KiB decoded bound is refused during
  // planning, before any byte is written. (This is the archive bound, not
  // arena exhaustion; step 4c covers the shared arena.)
  const huge = await load(fourth.z64, graduation.scene.assetId);
  clips(huge.document).find(clip => clip.payload.nativeDialogueEditable).payload.rawText += textFor(7, 60000);
  const before = fourth.z64.slice();
  await assert.rejects(plan(fourth.z64, [huge]), /exceed/);
  assert.deepStrictEqual(fourth.z64, before, 'a rejected export leaves the loaded ROM untouched');

  // 4c. True arena exhaustion: several individually valid authored streams
  // (setup, random-operand moves, native exit) each fit on their own, but
  // together exceed the 188 KiB shared arena. Planning refuses with exactly
  // relocation-capacity and the detached candidate is unchanged.
  const Au = OB64.cutsceneAuthored;
  const heavyCatalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const templateSource = await OB64.cutsceneCodec.loadSceneSource(retail, heavyCatalog.getScene(Au.CLASS5_TEMPLATE), {});
  const scaffoldWords = [];
  const scaffold = Au.scaffoldFromTemplate(templateSource.decodedBytes);
  for (let at = 0; at < scaffold.length; at += 4) scaffoldWords.push(u32(scaffold, at));
  const freeRows = Au.freeSelectorRows(retail, heavyCatalog, []);
  let heavySeed = 20260927;
  const nextWord = () => (heavySeed = (Math.imul(heavySeed, 1664525) + 1013904223) >>> 0);
  function heavyStream() {
    const body = [];
    for (let move = 0; move < 1990; move++) body.push(7, nextWord(), nextWord(), nextWord(), nextWord(), nextWord(), nextWord(), nextWord());
    const all = scaffoldWords.slice(0, 18).concat(body, scaffoldWords.slice(18));
    const bytes = new Uint8Array(all.length * 4);
    all.forEach((value, index) => { bytes[index * 4] = value >>> 24; bytes[index * 4 + 1] = value >>> 16; bytes[index * 4 + 2] = value >>> 8; bytes[index * 4 + 3] = value; });
    return bytes;
  }
  const heavy = [];
  for (let index = 0; index < 4; index++) {
    const made = await Au.create(retail, heavyCatalog, { name: 'Heavy ' + index, selectorRow: freeRows[index],
      decodedBytes: heavyStream(), otherAuthored: heavy.map(item => item.authored) });
    heavy.push(heavyCatalog.registerAuthoredScene(made.scene));
  }
  const heavyState = scenes => ({ catalog: { scenes }, histories: {}, originalSerialized: {}, sourceByAssetId: {} });
  const single = { z64: retail.slice(), layout };
  const singlePlan = await OB64.cutsceneExport.prepare({ z64: retail, layout, cutsceneStudio: heavyState([heavy[0]]) }, single);
  assert(singlePlan.relocationArena.dataEnd < A.descriptorStart, 'one heavy stream fits the arena on its own');
  assert(singlePlan.relocationArena.dataEnd - A.start > (A.descriptorStart - A.start) / 4,
    'each heavy stream uses more than a quarter of the arena');
  const exhausted = { z64: retail.slice(), layout };
  await assert.rejects(OB64.cutsceneExport.prepare({ z64: retail, layout, cutsceneStudio: heavyState(heavy) }, exhausted),
    error => error.code === 'relocation-capacity', 'four heavy streams exhaust the shared arena');
  assert.deepStrictEqual(exhausted.z64, retail, 'an exhausted export leaves its detached candidate unchanged');

  // 4b. A cached source is never trusted over the ROM being exported. The
  // Graduation size header is corrupted after its source was cached: export
  // must reject it on its own and when another scene triggers compaction.
  const cachedGraduation = await load(retail, graduation.scene.assetId);
  clips(cachedGraduation.document).find(clip => clip.payload.nativeDialogueEditable).payload.rawText += ' Cached.';
  const corrupted = retail.slice();
  const prefix = cachedGraduation.scene.source.z64PrefixStart;
  corrupted[prefix + 3] ^= 0x01;
  await assert.rejects(plan(corrupted, [cachedGraduation]), 'a stale cached source is rejected without compaction');
  const grower = await load(retail, 'rom-custom-lz:01FA4D0A');
  addHolds(grower, 100);
  await assert.rejects(plan(corrupted, [cachedGraduation, grower]),
    'a stale cached source is rejected when compaction runs too');

  // 5. Malformed arenas are refused: unknown version, an event-row record that
  // does not own its directory word, an unknown kind, and a duplicate manifest.
  function withDescriptor(records, dataEnd, version) {
    const z64 = fourth.z64.slice();
    const descriptor = A.descriptor(records, dataEnd);
    if (version) descriptor.set([0, 0, 0, version], 4);
    z64.set(descriptor, A.descriptorStart);
    return z64;
  }
  const kept = arenaFour.records;
  assert.throws(() => A.read(withDescriptor(kept, arenaFour.dataEnd, 3)), /unrecognized data/);
  const eventRow = kept.map(row => row.slice());
  // A kind-2 (event row) record must own exactly its event directory word;
  // this one names row 66 while the directory still holds the retail key.
  eventRow[0][0] = ((A.KIND.EVENT_ROW << 28) | 66) >>> 0;
  assert.throws(() => A.read(withDescriptor(eventRow, arenaFour.dataEnd)), /does not own its event directory row/);
  const unknownKind = kept.map(row => row.slice());
  unknownKind[0][0] = ((4 << 28) | 1) >>> 0;
  assert.throws(() => A.read(withDescriptor(unknownKind, arenaFour.dataEnd)), /unknown kind/);
  const manifests = kept.map(row => row.slice());
  manifests[0][0] = (A.KIND.MANIFEST << 28) >>> 0; manifests[0][4] = 0;
  manifests[1][0] = (A.KIND.MANIFEST << 28) >>> 0; manifests[1][4] = 0;
  assert.throws(() => A.read(withDescriptor(manifests, arenaFour.dataEnd)), /invalid/);
  const kindOneInVersionOne = kept.map(row => row.slice());
  kindOneInVersionOne[0][0] = ((A.KIND.AUTHORED << 28) | 400) >>> 0;
  assert.throws(() => A.read(withDescriptor(kindOneInVersionOne, arenaFour.dataEnd, 1)), /invalid/);

  console.log('PASS arena v1 identity, reopen/edit-again compaction, kept-record moves with repointed selectors, dialogue decoded bound, true arena exhaustion with individually valid streams, stale cached-source refusal, and malformed-arena refusal.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
