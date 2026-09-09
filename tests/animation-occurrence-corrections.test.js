'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const crypto = require('crypto');
const EDITOR = path.resolve(__dirname, '..');
global.window = global;
global.module = undefined;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
if (!global.crypto?.subtle) Object.defineProperty(global, 'crypto', { value: crypto.webcrypto });
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['data.js', 'parsers.js', 'art.js', 'sprite-library.js', 'animation-corpus-data.js',
  'animation-art.js', 'combat-animation-overrides-data.js', 'combat-animation-overrides.js',
  'animation-sequences.js', 'animation-ui.js', 'sprite-editor-ui.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}
const A = OB64.art, S = OB64.animationSequences, M = OB64.animationArt, U = OB64.animationUI;
const master = fs.readFileSync(path.join(EDITOR, '..', 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
assert.strictEqual(crypto.createHash('sha256').update(master).digest('hex').toUpperCase(),
  '6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12');
const z64 = new Uint8Array(master);
for (let i = 0; i < z64.length; i += 2) [z64[i], z64[i + 1]] = [z64[i + 1], z64[i]];
async function fresh() {
  const rom = { z64: z64.slice(), layout: { id: 'us-rev0' } };
  await A.initialize(rom); rom.classDefs = OB64.parseClassDefs(rom.z64);
  OB64.combatAnimationOverrides.initialize(rom); S.initialize(rom); return rom;
}
function candidateFor(plan) {
  const bytes = z64.slice(), allocations = {};
  let cursor = A.constants.ARENA_START;
  for (const row of plan.relocatedResources) {
    const pad = row.stored.length & 1;
    const allocation = { name: row.name, stored: row.stored, entry: cursor,
      key: cursor - A.constants.RESOURCE_BASE, pad, end: cursor + 4 + row.stored.length + pad };
    assert(allocation.end <= A.constants.ARENA_END, 'test candidate retains the production arena bound');
    allocations[row.name] = allocation; cursor = allocation.end;
  }
  S.finalizeAllocations(plan, allocations);
  for (const row of Object.values(allocations)) {
    A.writeU32(bytes, row.entry, row.stored.length); bytes.set(row.stored, row.entry + 4);
    if (row.pad) bytes[row.end - 1] = 0;
  }
  S.applyPlan(plan, bytes, [], []); return bytes;
}
(async () => {
  const rom = await fresh();
  const sources = OB64.spriteEditorUI.animationSources(rom, 77);
  for (const lane of ['idle', 'advance', 'return', 'hit']) {
    assert(sources.some(row => S.laneFor(row) === lane), 'shared source catalog includes ' + lane);
  }
  assert(sources.some(row => row.spriteSourceAssigned), 'catalog identifies current assigned sources');
  assert(sources.some(row => S.laneFor(row) === 'normal' || S.laneFor(row) === 'blocked'), 'catalog retains attack/native routes: ' + sources.map(row => row.spec.actionName + ':' + S.laneFor(row)).join(', '));
  const donor = sources.find(row => S.laneFor(row) === 'advance' && U.selectorFlags(row) === '1/1');
  assert(donor);
  const sep = S.separateAndAssign(rom, donor, null, donor);
  const animation = sep.syntheticAnimation;
  const pairs = [];
  animation.frames.forEach((frame, index) => {
    const earlier = animation.frames.findIndex(other => other.token === frame.token);
    if (earlier !== index) pairs.push([earlier, index]);
  });
  assert(pairs.length, 'verified Giant movement contains repeated native tokens');
  const [earlier, later] = pairs[0];
  const oldPosition = animation.frames[later].layers[0].drawOffsetX;
  S.setLayerPosition(rom, sep, later, 0, oldPosition - 7, animation.frames[later].layers[0].drawOffsetY - 5);
  assert.strictEqual(animation.frames[earlier].token, animation.frames[later].token,
    'legacy repeated tokens remain valid editor input');
  // A frame copy creates independent art despite the repeated token.
  S.copyFrameFrom(rom, sep, later, animation, earlier);
  const editedLayer = animation.frames[later].layers[0];
  const child = M.currentEdit(rom.art.animations, editedLayer.sourceKey, editedLayer.selectedChildOrdinal);
  const indices = child.indices.slice(), intensity = child.intensity.slice();
  indices[0] = (indices[0] + 1) & 255; intensity[0] = 15;
  M.setEdit(rom.art.animations, editedLayer.sourceKey, editedLayer.selectedChildOrdinal, indices, intensity);
  S.setLayerPosition(rom, sep, later, 0, oldPosition - 7, editedLayer.drawOffsetY - 5);
  S.setFrameTicks(rom, sep, earlier, 6); S.setFrameTicks(rom, sep, later, 2);
  const beforeCopyTicks = animation.frames[later].ticks;
  S.copyFrameFrom(rom, sep, later, animation, earlier);
  assert.strictEqual(animation.frames[later].ticks, beforeCopyTicks, 'art-only copy retains destination duration');
  S.copyFrameFrom(rom, sep, later, animation, earlier, { includeDuration: true });
  assert.strictEqual(animation.frames[later].ticks, 6, 'art-and-duration copy retains source duration');
  const duplicated = S.duplicateFrame(rom, sep, later);
  assert.strictEqual(animation.frames[duplicated].ticks, 6);
  assert.notStrictEqual(animation.frames[duplicated].layers[0].sourceKey, animation.frames[later].layers[0].sourceKey);
  S.setLayerPosition(rom, sep, later, 0, oldPosition - 9, -14);
  const changedLayer = animation.frames[later].layers[0];
  const changedChild = M.currentEdit(rom.art.animations, changedLayer.sourceKey, changedLayer.selectedChildOrdinal);
  const changedIndices = changedChild.indices.slice(); changedIndices[0] = (changedIndices[0] + 1) & 255;
  M.setEdit(rom.art.animations, changedLayer.sourceKey, changedLayer.selectedChildOrdinal, changedIndices, changedChild.intensity);
  const ui = {};
  const fixedView = U.stablePreviewAnimation(rom.art, animation, ui);
  const before = U.framePixels(fixedView, animation.frames[later], rom.art.animations);
  S.translateFrames(rom, [{ separation: sep, frames: [later] }], -7, -5);
  const afterView = U.stablePreviewAnimation(rom.art, animation, ui);
  assert.deepStrictEqual(afterView.canvas, fixedView.canvas, 'stable viewport does not follow moved art');
  assert.notDeepStrictEqual(U.framePixels(afterView, animation.frames[later], rom.art.animations), before);
  assert.strictEqual(U.stablePreviewAnimation(rom.art, animation, { animationAutoFit: true }), animation);
  const beforeRejected = JSON.stringify(S.collectProject(rom));
  assert.throws(() => S.translateFrames(rom, [{ separation: sep, frames: [earlier] },
    { separation: sep, frames: [99999] }], 1, 1));
  assert.strictEqual(JSON.stringify(S.collectProject(rom)), beforeRejected, 'batch validation is atomic');
  const brokenDonor = Object.assign({}, animation, { artByKey: {} });
  assert.throws(() => S.copyFrameFrom(rom, sep, earlier, brokenDonor, later), /unavailable/);
  assert.strictEqual(JSON.stringify(S.collectProject(rom)), beforeRejected, 'copy validation is atomic');
  const library = OB64.spriteLibrary.initialize(rom);
  const reusable = OB64.spriteLibrary.assetFromFrames(library, {
    name: 'Giant aligned sequence', kind: 'sequence', width: animation.canvas.width, height: animation.canvas.height,
    anchor: { x: -animation.canvas.originX, y: -animation.canvas.originY },
    frames: OB64.spriteEditorUI.framesFromAnimation(rom, animation, 'sequence', 0, 0, 0)
  });
  const assetReload = OB64.spriteLibrary.prepareAssetFile(OB64.spriteLibrary.assetFileText(reusable));
  const reusableSource = OB64.spriteEditorUI.selectedAssetSource(assetReload, later, 0, true);
  const preparedLayer = U.prepareLibrarySpriteLayer(reusableSource, undefined, undefined, { placementMode: 'original' });
  assert.deepStrictEqual(preparedLayer.alignmentAnchor, reusableSource.anchor);
  const importedLayer = U.importLibrarySpriteLayer(rom, sep, animation, animation.frames[later], animation.frames[later].layers[0], preparedLayer);
  assert.strictEqual(animation.frames[later].layers[importedLayer].drawOffsetX, -reusableSource.anchor.x);
  assert.strictEqual(animation.frames[later].layers[importedLayer].drawOffsetY, -reusableSource.anchor.y);
  const anchoredSnapshot = JSON.stringify(S.collectProject(rom));
  assert.throws(() => U.importLibrarySpriteLayer(rom, sep, animation, animation.frames[later], animation.frames[later].layers[0],
    Object.assign({}, preparedLayer, { alignmentAnchor: { x: Infinity, y: 0 } })), /Imported layer X/);
  assert.strictEqual(JSON.stringify(S.collectProject(rom)), anchoredSnapshot, 'invalid native anchor cannot append partial art');
  const payload = S.collectProject(rom);
  const restored = await fresh();
  S.applyProject(restored, S.prepareProject(restored, payload));
  assert.deepStrictEqual(S.collectProject(restored), payload, 'legacy repeated occurrences round-trip unchanged');
  const plan = S.buildPlan(restored, z64), candidate = candidateFor(plan);
  S.verifyPlan(plan, candidate);
  const row = plan.groups[0].sequenceRows.find(row => row.separation.id === sep.id);
  const metadata = A.readCompressedResource(candidate, plan.groups[0].controls[0].allocation.key).decoded;
  const pose = A.readCompressedResource(candidate, plan.groups[0].controls[1].allocation.key).decoded;
  const decoded = M.parsePoseProgram(pose, row.separation.selector, 'occurrence regression');
  assert.notStrictEqual(decoded.frames[earlier][0], decoded.frames[later][0], 'divergent repeated token exports distinct metadata');
  const occurrence = M.parseMetadataFrame(metadata, decoded.frames[later][0]);
  assert.strictEqual(occurrence.layers[0].drawOffsetX, row.animation.frames[later].layers[0].drawOffsetX);
  // Bytes still equal the plan; differing intended semantics must independently fail verification.
  row.animation.frames[later].layers[0].drawOffsetX++;
  assert.throws(() => S.verifyPlan(plan, candidate), /differs from editor state/);
  row.animation.frames[later].layers[0].drawOffsetX--;
  row.animation.frames[later].ticks++;
  assert.throws(() => S.verifyPlan(plan, candidate), /duration differs/);
  row.animation.frames[later].ticks--;
  S.verifyPlan(plan, candidate);
  assert.deepStrictEqual(rom.z64, z64, 'offline candidates leave loaded source ROM unchanged');
  console.log('PASS Giant repeated occurrences, semantic export rejection, migration, copy duration, duplication, alignment, catalog, and source isolation');
})().catch(error => { console.error(error.stack); process.exitCode = 1; });
