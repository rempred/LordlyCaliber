'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const ROM = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.module = undefined;
if (!window.crypto || !window.crypto.subtle) {
  Object.defineProperty(window, 'crypto', { value: crypto.webcrypto });
}
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');

vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js',
  'art.js',
  'animation-corpus-data.js',
  'animation-art.js',
  'animation-ui.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

function normalizeV64(bytes) {
  const output = new Uint8Array(bytes);
  for (let offset = 0; offset < output.length; offset += 2) {
    const first = output[offset];
    output[offset] = output[offset + 1];
    output[offset + 1] = first;
  }
  return output;
}

function hash(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function changedPixels(state, source, childOrdinal, delta) {
  const original = OB64.animationArt.currentEdit(state, source.key, childOrdinal);
  const indices = original.indices.slice();
  const intensity = original.intensity.slice();
  let pixel = intensity.findIndex(value => value > 0);
  if (pixel < 0) pixel = 0;
  indices[pixel] = (indices[pixel] + delta) & 0xFF;
  intensity[pixel] = intensity[pixel] === 15
    ? 14 : Math.min(15, intensity[pixel] + 1);
  return { original, indices, intensity, pixel };
}

(async function run() {
  assert.strictEqual(OB64.animationUI.normalizeIntensity(undefined), 15);
  assert.strictEqual(OB64.animationUI.normalizeIntensity(0), 0);
  assert.strictEqual(OB64.animationUI.normalizeIntensity(15), 15);

  const z64 = normalizeV64(fs.readFileSync(ROM));
  const rom = { z64, layout: { id: 'us-rev0' } };
  await OB64.art.initialize(rom);
  const state = rom.art.animations;
  const corpus = OB64.animationCorpusData;

  assert.strictEqual(rom.art.supported, true);
  assert.strictEqual(state.supported, true, state.unavailableReason);
  assert.deepStrictEqual(state.corpus, {
    version: '2026-08-15-r2',
    projectionSha256: '6D6FD9528E927B5EC5EFCBBB78CE8F098C049F8BFADE4E46551705E2BC10849F',
    sequenceCount: 2712,
    physicalSourceCount: 4707,
    bindingCount: 4900,
    usedBindingCount: 3807,
    dormantBindingCount: 1093,
    equipmentGroupCount: 84,
  });
  assert.strictEqual(new Set(state.specs.map(row => row.spec.classId)).size, 152);
  assert.strictEqual(new Set(state.specs.map(row =>
    `${row.spec.classId}:${row.spec.actionId}`)).size, 234);
  assert.strictEqual(new Set(state.specs.map(row =>
    `${row.spec.classId}:${row.spec.actionName}`)).size, 225);
  assert.strictEqual(Object.keys(state.byKey).length, 2723,
    '2,712 canonical keys plus 11 legacy aliases');

  assert.strictEqual(state.specs.find(row => row.spec.classId === 0x87)
    .spec.className, 'Danika (Boss)');
  assert.strictEqual(state.specs.find(row => row.spec.classId === 0x98)
    .spec.className, 'Siren (Eudika)');
  assert.strictEqual(state.specs.find(row => row.spec.classId === 0xA4)
    .spec.className, 'Death Bahamut (Grozz Nuy)');

  const physicalSources = Object.values(corpus.physicalSources);
  assert.strictEqual(physicalSources.filter(row =>
    row.format.kind === 'indexed-ci8').length, 4704);
  assert.strictEqual(physicalSources.filter(row =>
    row.format.kind === 'direct-rgba5551').length, 3);
  const allBindings = physicalSources.flatMap(row => row.bindings);
  assert.deepStrictEqual(allBindings.reduce((counts, binding) => {
    counts[binding.sourceRole] = (counts[binding.sourceRole] || 0) + 1;
    return counts;
  }, {}), { body: 3717, equipment: 828, 'element-effect': 355 });

  const policyForRole = { body: 0, equipment: 1, 'element-effect': 2 };
  state.specs.forEach(animation => {
    animation.frames.forEach(frame => {
      frame.layers.forEach(layer => {
        const source = animation.artByKey[layer.sourceKey];
        assert(source, `${animation.key} frame ${frame.sequenceIndex} source`);
        assert.strictEqual(source.sourceRole, layer.sourceRole);
        assert.strictEqual(source.selectorPolicy, policyForRole[source.sourceRole]);
        assert.strictEqual(source.weaponSelectable, source.sourceRole === 'equipment');
        if (source.sourceRole === 'body') {
          assert.strictEqual(layer.selectedChildOrdinal,
            OB64.animationArt.childOrdinalOrFallback(source,
              animation.spec.selectedBodyChild));
        } else {
          assert.strictEqual(layer.selectedChildOrdinal, 0);
        }
      });
    });
  });

  const compatibility = state.specs.filter(row => row.compatibilityKey);
  assert.strictEqual(compatibility.length, 11);
  assert.strictEqual(compatibility.reduce((count, row) =>
    count + row.frames.length, 0), 139);
  compatibility.forEach(animation => {
    assert.strictEqual(state.byKey[animation.compatibilityKey], animation);
    const expected = animation.spec.frozenParity.rawCompositeSha256;
    const actual = animation.frames.map(frame => hash(
      OB64.animationUI.framePixels(animation, frame, state, null)));
    assert.deepStrictEqual(actual, expected,
      `${animation.compatibilityKey} frozen frame parity`);
  });

  const fighter = state.byKey['fighter-slash'];
  assert.strictEqual(fighter.spec.className, 'Fighter');
  assert.strictEqual(fighter.spec.actionId, 4);
  assert.strictEqual(fighter.equipmentGroup.equipmentFamily, 'Sword');
  assert.deepStrictEqual(OB64.animationUI.weaponItemsForChild(
    fighter, 0, fighter.spec.weaponChildCount).direct.map(row => row.name),
  ['Short Sword', 'Knoevlfer']);

  const wizard = state.byKey['wizard-elemental-magic'];
  assert.deepStrictEqual(OB64.animationUI.weaponItemsForChild(
    wizard, 0, wizard.spec.weaponChildCount).fallback.map(row => row.name),
  ['Scepter']);
  const siren = state.byKey['siren-elemental-magic'];
  assert.deepStrictEqual(OB64.animationUI.weaponItemsForChild(
    siren, 0, siren.spec.weaponChildCount).fallback.map(row => row.name),
  ['Hemlock', 'Scepter']);
  assert(OB64.animationUI.retailMappingText(wizard, 12)
    .includes('Scepter uses child 0 as a fallback'));

  const directAnimation = state.specs.find(animation =>
    Object.values(animation.artByKey).some(source => !source.editable));
  assert(directAnimation, 'complete corpus must expose direct RGBA5551 art');
  const directFrame = directAnimation.frames.find(frame => frame.layers.some(layer =>
    !directAnimation.artByKey[layer.sourceKey].editable));
  const directLayer = directFrame.layers.find(layer =>
    !directAnimation.artByKey[layer.sourceKey].editable);
  const directSource = directAnimation.artByKey[directLayer.sourceKey];
  assert.strictEqual(directSource.formatKind, 'direct-rgba5551');
  assert.strictEqual(directSource.sourceRole, 'equipment');
  assert.strictEqual(directSource.selectableChildOrdinals.length, 4);
  assert.throws(() => OB64.animationArt.originalChild(directSource, 0),
    /not editable/);
  const directDisplay = OB64.animationArt.displayChild(state, directSource.key, 0);
  assert(directDisplay.words instanceof Uint16Array);
  assert(directDisplay.alpha instanceof Uint8Array);
  assert(Array.from(directDisplay.alpha).some(value => value > 0));
  assert.strictEqual(OB64.animationUI.framePixels(
    directAnimation, directFrame, state, null).length,
  directAnimation.canvas.width * directAnimation.canvas.height * 4);

  const indexedSource = Object.values(state.artByKey).find(source =>
    source.editable && source.editableChildOrdinals.length);
  assert(indexedSource, 'complete corpus must expose indexed palette sources');
  assert.strictEqual(OB64.animationArt.childPalette(indexedSource,
    indexedSource.editableChildOrdinals[0]).length, 256);

  const collision = state.specs.reduce((groups, animation) => {
    const key = `${animation.spec.classId}:${animation.spec.actionName}`;
    if (!groups[key]) groups[key] = new Set();
    groups[key].add(animation.spec.actionId);
    return groups;
  }, {});
  assert(Object.values(collision).some(ids => ids.size > 1),
    'action IDs must disambiguate same-name actions');

  const nonLegacy = state.specs.find(animation =>
    !animation.compatibilityKey && animation.frames.some(frame =>
      frame.layers.some(layer => {
        const source = animation.artByKey[layer.sourceKey];
        return source.editable && source.editableChildOrdinals
          .includes(layer.selectedChildOrdinal);
      })));
  const nonLegacyLayer = nonLegacy.frames.flatMap(frame => frame.layers)
    .find(layer => {
      const source = nonLegacy.artByKey[layer.sourceKey];
      return source.editable && source.editableChildOrdinals
        .includes(layer.selectedChildOrdinal);
    });
  const nonLegacySource = nonLegacy.artByKey[nonLegacyLayer.sourceKey];
  const nonLegacyChild = nonLegacyLayer.selectedChildOrdinal;
  const changed = changedPixels(state, nonLegacySource, nonLegacyChild, 1);
  assert.strictEqual(OB64.animationArt.setEdit(state, nonLegacySource.key,
    nonLegacyChild, changed.indices, changed.intensity), true);
  const projectV20 = OB64.animationArt.collectProject(state);
  assert.deepStrictEqual(Object.keys(projectV20), [nonLegacySource.key]);
  assert.strictEqual(projectV20[nonLegacySource.key].bindingId,
    nonLegacySource.bindingId);
  assert.strictEqual(projectV20[nonLegacySource.key].physicalSourceId,
    nonLegacySource.physicalSourceId);
  assert.strictEqual(projectV20[nonLegacySource.key].descriptorMemberOrdinal,
    nonLegacySource.descriptorMemberIndex);
  const preparedV20 = OB64.animationArt.prepareProject(state, projectV20);
  assert.strictEqual(preparedV20.count, 1);
  OB64.animationArt.resetAll(state);
  assert.strictEqual(OB64.animationArt.applyPrepared(state, preparedV20), 1);
  assert.strictEqual(OB64.animationArt.hasEdit(state,
    nonLegacySource.key, nonLegacyChild), true);
  OB64.animationArt.resetAll(state);

  const legacyLayer = fighter.frames.flatMap(frame => frame.layers).find(layer => {
    const source = fighter.artByKey[layer.sourceKey];
    return source.editable && source.editableChildOrdinals
      .includes(layer.selectedChildOrdinal);
  });
  const legacySource = fighter.artByKey[legacyLayer.sourceKey];
  const legacyChild = legacyLayer.selectedChildOrdinal;
  const legacyChanged = changedPixels(state, legacySource, legacyChild, 2);
  OB64.animationArt.setEdit(state, legacySource.key, legacyChild,
    legacyChanged.indices, legacyChanged.intensity);
  const modernLegacyRecord = OB64.animationArt.collectProject(state)[legacySource.key];
  const legacyKey = `fighter-slash:${legacyLayer.artId}`;
  assert.strictEqual(state.sourceAliases[legacyKey], legacySource.key);
  const v19Record = {
    animation: 'fighter-slash',
    artId: legacySource.artId,
    resourceKey: OB64.animationArt.hex(legacySource.resourceKey),
    width: legacySource.sprite.width,
    height: legacySource.sprite.height,
    children: modernLegacyRecord.children,
  };
  OB64.animationArt.resetAll(state);
  const preparedV19 = OB64.animationArt.prepareProject(state,
    { [legacyKey]: v19Record });
  assert.strictEqual(preparedV19.count, 1);
  assert.deepStrictEqual(Object.keys(preparedV19.edits), [legacySource.key]);
  assert.strictEqual(OB64.animationArt.applyPrepared(state, preparedV19), 1);
  OB64.animationArt.resetAll(state);

  const sharedSource = Object.values(state.artByKey).find(source =>
    source.editable && source.descriptorReferenceCount > 1 &&
    source.editableChildOrdinals.length);
  assert(sharedSource, 'complete corpus must expose a shared copy-on-write source');
  const sharedChild = sharedSource.editableChildOrdinals[0];
  const sharedChanged = changedPixels(state, sharedSource, sharedChild, 3);
  OB64.animationArt.setEdit(state, sharedSource.key, sharedChild,
    sharedChanged.indices, sharedChanged.intensity);
  const candidate = { z64: z64.slice() };
  const plan = OB64.art.prepareExport(rom, candidate);
  assert.strictEqual(plan.animations.length, 1);
  assert.strictEqual(plan.animations[0].key, sharedSource.key);
  assert.strictEqual(plan.animations[0].placement, 'relocated');
  const result = OB64.art.applyExport(rom, candidate, plan);
  assert.strictEqual(result.editedCombatSpriteCount, 1);
  assert(result.log.some(line => line.includes('copied resource')));
  const relocatedKey = plan.animations[0].allocation.key;
  assert.strictEqual(OB64.art.readU32(candidate.z64,
    sharedSource.descriptorEntryOffset), relocatedKey);
  const readbackSprite = OB64.animationArt.parseSpriteObject(
    OB64.art.readCompressedResource(candidate.z64, relocatedKey).decoded,
    relocatedKey);
  const readbackChild = OB64.animationArt.decodeChild(readbackSprite, sharedChild);
  assert.deepStrictEqual(Array.from(readbackChild.indices),
    Array.from(sharedChanged.indices));
  assert.deepStrictEqual(Array.from(readbackChild.intensity),
    Array.from(sharedChanged.intensity));

  const sharedPhysical = Object.values(corpus.physicalSources).find(physical =>
    physical.bindings.filter(binding => state.artByKey[binding.id]).length > 1);
  const sharedBindings = sharedPhysical.bindings
    .map(binding => state.artByKey[binding.id]).filter(Boolean);
  assert(sharedBindings.length > 1);
  assert.notStrictEqual(sharedBindings[0].key, sharedBindings[1].key,
    'logical bindings sharing physical bytes must remain independently detachable');

  console.log('PASS complete combat-animation corpus, rendering, Project compatibility, and relocation readback');
})().catch(error => {
  console.error(error && error.stack ? error.stack : error);
  process.exitCode = 1;
});
