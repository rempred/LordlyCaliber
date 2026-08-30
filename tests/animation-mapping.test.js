'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const ROM = path.join(ROOT,
  'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.module = undefined;
if (!window.crypto || !window.crypto.subtle) {
  Object.defineProperty(window, 'crypto', { value: crypto.webcrypto });
}
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');

vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js', 'parsers.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'animation-ui.js', 'combat-animation-overrides-data.js',
  'combat-animation-overrides.js'
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

function findVariant(state, classId, flags) {
  return state.specs.find(animation => animation.spec.classId === classId &&
    animation.mappingStatus.selectorFlags === flags);
}

function firstActionRows(state, classId) {
  const first = state.specs.find(animation => animation.spec.classId === classId);
  assert(first, `class 0x${classId.toString(16)} must have a corpus sequence`);
  return state.specs.filter(animation => animation.spec.classId === classId &&
    animation.spec.actionId === first.spec.actionId);
}

function preferredFlags(state, classId) {
  const preferred = OB64.animationUI.preferredAnimation(
    firstActionRows(state, classId), { rawMode: 0 });
  return OB64.animationUI.selectorFlags(preferred);
}

function changedPixels(source, childOrdinal) {
  const original = OB64.animationArt.originalChild(source, childOrdinal);
  const indices = original.indices.slice();
  const intensity = original.intensity.slice();
  let pixel = intensity.findIndex(value => value > 0);
  if (pixel < 0) pixel = 0;
  indices[pixel] = (indices[pixel] + 1) & 0xFF;
  intensity[pixel] = intensity[pixel] === 15 ? 14 : intensity[pixel] + 1;
  return { indices, intensity };
}

(async function run() {
  const rom = {
    z64: normalizeV64(fs.readFileSync(ROM)),
    layout: { id: 'us-rev0' }
  };
  await OB64.art.initialize(rom);
  const state = rom.art.animations;
  assert.strictEqual(state.supported, true, state.unavailableReason);

  const previewTimeline = OB64.animationUI.animationPreviewTimeline({
    frames: [
      { sequenceIndex: 0, ticks: 0 },
      { sequenceIndex: 1, ticks: 15 },
      { sequenceIndex: 2, ticks: 15 },
    ],
  });
  assert.strictEqual(previewTimeline.ticksPerSecond, 30);
  assert.strictEqual(previewTimeline.totalTicks, 30);
  assert.deepStrictEqual(previewTimeline.entries.map(entry => entry.frameIndex),
    [1, 2], 'zero-tick setup frames must not consume preview time');
  assert.strictEqual(OB64.animationUI.animationPreviewFrameAtMs(
    previewTimeline, 499).frameIndex, 1);
  assert.strictEqual(OB64.animationUI.animationPreviewFrameAtMs(
    previewTimeline, 500).frameIndex, 2);
  assert.strictEqual(OB64.animationUI.animationPreviewFrameAtMs(
    previewTimeline, 1000).frameIndex, 1,
  '30 ticks must loop after exactly one second');
  const exportFrames = OB64.animationUI.animationExportFrames(previewTimeline);
  assert.strictEqual(exportFrames.length, 30,
    'WebM export must emit one video frame for every game tick');
  assert(exportFrames.slice(0, 15).every(entry => entry.frameIndex === 1));
  assert(exportFrames.slice(15).every(entry => entry.frameIndex === 2));
  function Recorder() {}
  Recorder.isTypeSupported = type => type === 'video/webm;codecs=vp8';
  assert.strictEqual(OB64.animationUI.animationWebmMimeType(Recorder),
    'video/webm;codecs=vp8');

  const fighterIdleRows = OB64.animationUI.idleAnimationRows(state, 0x02);
  assert.strictEqual(fighterIdleRows.length, 4,
    'Fighter must expose one selector-0 idle loop for every mapped art route: ' +
      JSON.stringify(state.idleSequenceFailures[0x02]));
  assert.strictEqual(OB64.animationUI.idleAnimationRows(state, 0x02),
    fighterIdleRows, 'idle loops must be resolved once and cached by class');
  assert.deepStrictEqual(fighterIdleRows.map(animation =>
    OB64.animationUI.selectorFlags(animation)), ['0/0', '0/1', '1/0', '1/1']);
  assert(fighterIdleRows.every(animation =>
    animation.spec.actionId === -1 && animation.spec.selector === 0 &&
    animation.spec.rawMode === 0 && animation.spec.idleSequence &&
    OB64.animationUI.animationLaneKey(animation) === 'idle' &&
    OB64.animationUI.animationLaneLabel(animation) === 'Idle Loop'));
  const fighterIdleBase = fighterIdleRows.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '0/0');
  assert.deepStrictEqual(fighterIdleBase.frames.map(frame => frame.ticks),
    [16, 8, 16, 8], 'Fighter selector 0 must expose its complete 48-tick loop');
  const fighterIdleBaseCatalog = OB64.animationUI.animationSequenceCatalogRows(
    state, null, 0x02, 0, { idleOnly: true, flags: '0/0' });
  assert.deepStrictEqual(fighterIdleBaseCatalog, [fighterIdleBase],
    'Idle selection must show only the selected side and art route');
  const fighterIdleChoices = OB64.animationUI.animationClassVariantChoices(
    fighterIdleBaseCatalog);
  assert.strictEqual(fighterIdleChoices.length, 1);
  assert(fighterIdleChoices[0].label.includes('Idle / Rest') &&
    fighterIdleChoices[0].label.includes('Idle Loop'));
  const fighterCopyCatalogOptions =
    OB64.animationUI.animationCopyCatalogOptions(false, false);
  assert.deepStrictEqual(fighterCopyCatalogOptions, {
    includeIdle: true,
    includeClassMotion: true
  });
  assert.strictEqual(OB64.animationUI.animationCopyCatalogOptions(
    false, true), null,
  'Replace From must retain its current source catalog');
  assert.deepStrictEqual(OB64.animationUI.animationCopyCatalogOptions(
    true, false), { idleOnly: true });
  assert.deepStrictEqual(OB64.animationUI.animationCopyCatalogOptions(
    false, true, true), {
    includeIdle: true,
    includeClassMotion: true
  }, 'movement replacement must retain idle, movement, and combat donors');
  const fighterCopySourceRows = [0, 1].flatMap(side =>
    OB64.animationUI.animationSequenceCatalogRows(
      state, null, 0x02, side, { includeIdle: true }));
  const fighterCopyIdleRows = fighterCopySourceRows.filter(animation =>
    animation.spec.idleSequence);
  assert.strictEqual(fighterCopyIdleRows.length, 4,
    'Copy From and Separate must list selector 0 for every Fighter art route');
  assert.deepStrictEqual(fighterCopyIdleRows.map(animation =>
    OB64.animationUI.selectorFlags(animation)), ['0/0', '1/0', '0/1', '1/1']);
  const fighterActions = OB64.animationUI.animationActionChoices(
    state.specs.filter(animation => animation.spec.classId === 0x02),
    fighterIdleBase);
  assert.deepStrictEqual(fighterActions.slice(0, 3).map(animation =>
    animation.spec.actionId), [-1, -2, -3],
  'Idle, movement advance, and movement return must lead the Action menu');
  assert.strictEqual(fighterActions[0].spec.actionName, 'Idle / Rest');
  assert(!state.specs.some(OB64.animationUI.isIdleAnimation),
    'on-demand idle loops must not expand the normal startup corpus');

  const gatekeeperSearchRow = state.specs.find(animation =>
    animation.spec.classId === 0x71);
  assert.strictEqual(OB64.animationUI.layerDisplayLabel({ ordinal: 0 },
    { weaponSelectable: true, sourceRole: 'equipment' }), 'Layer 1 · Weapon');
  assert.strictEqual(OB64.animationUI.layerDisplayLabel({ ordinal: 1 },
    { weaponSelectable: false, sourceRole: 'element-effect' }), 'Layer 2');
  assert.strictEqual(OB64.animationUI.layerDisplayLabel({ ordinal: 2 },
    { weaponSelectable: false, sourceRole: 'body' }), 'Layer 3');
  assert(OB64.animationUI.classSearchMatches(gatekeeperSearchRow, 'gate'));
  assert(OB64.animationUI.classSearchMatches(gatekeeperSearchRow, '0x71'));
  assert(OB64.animationUI.classSearchMatches(gatekeeperSearchRow, '113'));
  assert(OB64.animationUI.classSearchMatches(gatekeeperSearchRow, ''));
  assert(!OB64.animationUI.classSearchMatches(gatekeeperSearchRow, 'siren'));
  const classChoices = OB64.animationUI.animationClassChoices(
    state.artRouteTemplates);
  assert.strictEqual(classChoices.length, 164);
  assert.deepStrictEqual(classChoices.filter(row => row.missingAnimation)
    .map(row => row.spec.classId), [],
  'every named class must expose at least one readable combat-art route');
  assert(OB64.animationUI.classSearchMatches(
    classChoices.find(row => row.spec.classId === 0x64), '0x64'));
  assert.strictEqual(state.artRouteTemplates.length, 164 * 4,
    'every class must expose all four ROM class-art handles');
  assert.deepStrictEqual(state.artRouteFailures, [],
    'all class-art table rows must parse directly from the loaded ROM');
  for (const classId of Object.keys(OB64.CLASS_NAMES).map(Number)
      .filter(classId => classId > 0)) {
    assert.deepStrictEqual(Object.keys(state.artRouteTemplatesByClass[classId]).sort(),
      ['0/0', '0/1', '1/0', '1/1'],
    `class 0x${classId.toString(16)} must expose all four art routes`);
  }
  const amriusPlayerArt = state.artRouteTemplatesByClass[0x63]['1/0'];
  assert.strictEqual(amriusPlayerArt.spec.route.rawHandleU16, 0x00A5);
  assert.strictEqual(amriusPlayerArt.spec.descriptorKey, 0x00F3BBD6);
  assert(amriusPlayerArt.frames.length > 0,
    'Dark Prince player art must contain readable frames');
  const discoveredSource = Object.values(amriusPlayerArt.artByKey).find(source =>
    source.key.startsWith('dynamic-binding-') && source.editable);
  assert(discoveredSource,
    'newly exposed class art must create a stable editable ROM binding');
  const discoveredChild = discoveredSource.editableChildOrdinals[0];
  const discoveredOriginal = OB64.animationArt.originalChild(
    discoveredSource, discoveredChild);
  const discoveredEdit = changedPixels(discoveredSource, discoveredChild);
  assert.strictEqual(OB64.animationArt.setEdit(state, discoveredSource.key,
    discoveredChild, discoveredEdit.indices, discoveredEdit.intensity), true);
  const discoveredPayload = OB64.animationArt.collectProject(state);
  assert(discoveredPayload[discoveredSource.key]);
  const discoveredReload = OB64.animationArt.initialize(rom.z64);
  const discoveredPrepared = OB64.animationArt.prepareProject(
    discoveredReload, discoveredPayload);
  assert(discoveredPrepared.edits[discoveredSource.key],
    'directly discovered class art edits must survive Project validation');
  assert.strictEqual(OB64.animationArt.setEdit(state, discoveredSource.key,
    discoveredChild, discoveredOriginal.indices,
    discoveredOriginal.intensity), true);

  assert.deepStrictEqual(state.mappingAudit.counts, {
    mapped: 1938,
    sharedSpecial: 210,
    dedicatedSpecial: 48,
    soldierAlias: 504,
    visibleFailure: 12,
    missingVariantRows: 0,
  });
  assert.deepStrictEqual(state.mappingAudit.byClass[0x93].missingFlags, []);
  assert.deepStrictEqual(state.mappingAudit.byClass[0x9B].missingFlags, []);
  const missingFlagClasses = Object.values(state.mappingAudit.byClass)
    .filter(row => row.missingFlags.length)
    .map(row => row.classId);
  assert.deepStrictEqual(missingFlagClasses, []);
  for (let classId = 0x87; classId <= 0xA4; classId++) {
    assert(state.mappingAudit.byClass[classId],
      `special class 0x${classId.toString(16)} must remain in the selector`);
  }
  const specialStates = new Set([
    'soldier-alias', 'shared-special', 'dedicated-special', 'visible-failure',
  ]);
  for (const animation of state.specs.filter(row => row.spec.classId >= 0x87)) {
    assert(specialStates.has(animation.mappingStatus.state),
      `${animation.spec.className} has an unclassified special mapping`);
  }

  const paladin = findVariant(state, 0x13, '1/1');
  const archmage = findVariant(state, 0x1B, '1/1');
  assert(paladin.mappingStatus.transformedSourceCount > 0);
  assert(archmage.mappingStatus.transformedSourceCount > 0);
  assert.deepStrictEqual(paladin.mappingStatus.emptyFrameIndices, []);
  assert.deepStrictEqual(archmage.mappingStatus.emptyFrameIndices, []);

  assert.strictEqual(findVariant(state, 0x87, '0/0').mappingStatus.state,
    'soldier-alias');
  assert.strictEqual(findVariant(state, 0x71, '0/0').mappingStatus.state,
    'soldier-alias');
  assert.notStrictEqual(findVariant(state, 0x75, '0/0').mappingStatus.state,
    'soldier-alias',
    'a shared descriptor with a non-Soldier body child must remain mapped');
  assert.strictEqual(findVariant(state, 0x94, '1/1').mappingStatus.title,
    'Shared Archmage mapping');
  assert.strictEqual(findVariant(state, 0xA0, '1/1').mappingStatus.title,
    'Shared Paladin mapping');
  assert.strictEqual(findVariant(state, 0xA3, '1/1').mappingStatus.state,
    'dedicated-special');
  assert.strictEqual(findVariant(state, 0x9F, '1/0').mappingStatus.state,
    'visible-failure');
  assert.strictEqual(findVariant(state, 0x72, '0/0')
    .mappingStatus.artClassName, 'Soldier');
  assert.strictEqual(findVariant(state, 0x72, '1/0')
    .mappingStatus.artClassName, 'Grappler');
  assert.strictEqual(findVariant(state, 0x94, '1/1')
    .mappingStatus.artClassName, 'Archmage');

  const valkyrieBasePlayer = findVariant(state, 0x0F, '0/0');
  const valkyrieAlternatePlayer = findVariant(state, 0x0F, '1/0');
  const valkyrieAlternateEnemy = findVariant(state, 0x0F, '0/1');
  const valkyrieBaseEnemy = findVariant(state, 0x0F, '1/1');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    valkyrieBasePlayer), 'Base Art');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    valkyrieAlternatePlayer), 'Alternate Art');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    valkyrieAlternateEnemy), 'Alternate Art');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    valkyrieBaseEnemy), 'Base Art');

  const freyaAlternateEnemy = findVariant(state, 0x1E, '0/1');
  const freyaBaseEnemy = findVariant(state, 0x1E, '1/1');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    freyaAlternateEnemy), 'Alternate Art');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    freyaBaseEnemy), 'Base Art');

  const efeminetteEnemy = findVariant(state, 0x8C, '1/1');
  assert.strictEqual(efeminetteEnemy.mappingStatus.artClassId, 0x0F);
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    efeminetteEnemy), 'Base Art');

  const fighterBaseEnemy = findVariant(state, 0x02, '0/1');
  const fighterAlternateEnemy = findVariant(state, 0x02, '1/1');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    fighterBaseEnemy), 'Base Art');
  assert.strictEqual(OB64.animationUI.animationArtVariantLabel(
    fighterAlternateEnemy), 'Alternate Art');

  assert.strictEqual(preferredFlags(state, 0x01), '0/0');
  assert.strictEqual(preferredFlags(state, 0x02), '0/0');
  assert.strictEqual(preferredFlags(state, 0x63), '1/1');
  assert.strictEqual(preferredFlags(state, 0x65), '1/1');
  assert.strictEqual(preferredFlags(state, 0x71), '1/1');
  assert.strictEqual(preferredFlags(state, 0x72), '1/0');
  assert.strictEqual(preferredFlags(state, 0x87), '1/1');
  assert.strictEqual(preferredFlags(state, 0x94), '1/1');
  assert.strictEqual(preferredFlags(state, 0xA0), '1/1');
  assert.strictEqual(preferredFlags(state, 0xA3), '0/1');
  assert.strictEqual(preferredFlags(state, 0xA4), '0/1');

  const actionGroups = new Map();
  for (const animation of state.specs) {
    const key = `${animation.spec.classId}:${animation.spec.actionId}`;
    if (!actionGroups.has(key)) actionGroups.set(key, []);
    actionGroups.get(key).push(animation);
  }
  for (const [key, rows] of actionGroups) {
    const rawModeZero = rows.filter(row => row.spec.rawMode === 0);
    if (!rawModeZero.some(row => OB64.animationUI.variantQuality(row) === 0)) {
      continue;
    }
    const preferred = OB64.animationUI.preferredAnimation(rows, { rawMode: 0 });
    assert.strictEqual(OB64.animationUI.variantQuality(preferred), 0,
      `${key} default preview must not choose Soldier or a visible failure`);
  }

  const grapplerRendChoices = OB64.animationUI.animationVariantChoices(
    state.specs.filter(animation => animation.spec.classId === 0x72 &&
      animation.spec.actionId === 0x06));
  assert.strictEqual(grapplerRendChoices.length, 8);
  assert.strictEqual(grapplerRendChoices.filter(choice =>
    choice.laneKey === 'normal').length, 4);
  assert.strictEqual(grapplerRendChoices.filter(choice =>
    choice.laneKey === 'blocked').length, 4);
  grapplerRendChoices.filter(choice => choice.laneKey === 'normal')
    .forEach(choice => assert.deepStrictEqual(choice.rawModes, [0, 1]));
  grapplerRendChoices.forEach(choice => assert.strictEqual(choice.optionClass,
    choice.flags.endsWith('/1')
      ? 'animation-variant-enemy' : 'animation-variant-player'));
  grapplerRendChoices.filter(choice => choice.laneKey === 'blocked')
    .forEach(choice => {
      assert.deepStrictEqual(choice.rawModes, [2]);
      assert.strictEqual(choice.pointsToNormalRoute, true);
      assert(choice.linkedToKey);
    });
  const grapplerRendLabels = grapplerRendChoices.map(choice => choice.label);
  assert(grapplerRendLabels.some(label =>
    label.startsWith('Base Art · Normal Attack · ')));
  assert(grapplerRendLabels.some(label =>
    label.startsWith('Alternate Art · Normal Attack · ')));
  assert.deepStrictEqual([...new Set(grapplerRendChoices.map(choice =>
    choice.artVariantLabel))].sort(),
  ['Alternate Art', 'Base Art']);
  grapplerRendLabels.forEach(label => {
    assert(!/Raw mode|flags|class-072|Mapped|Soldier alias|Player Side|Enemy Side|Variant/.test(label));
  });

  const grapplerFatalDanceChoices = OB64.animationUI.animationVariantChoices(
    state.specs.filter(animation => animation.spec.classId === 0x72 &&
      animation.spec.actionId === 0x9E));
  assert.strictEqual(grapplerFatalDanceChoices.length, 8);
  grapplerFatalDanceChoices.filter(choice => choice.laneKey === 'blocked')
    .forEach(choice => assert.strictEqual(choice.pointsToNormalRoute, false));

  const freyaCleaveChoices = OB64.animationUI.animationVariantChoices(
    state.specs.filter(animation => animation.spec.classId === 0x1E &&
      animation.spec.actionId === 0x05));
  const freyaBasePlayer = freyaCleaveChoices.find(choice =>
    choice.flags === '0/0' && choice.laneKey === 'normal');
  const freyaAlternatePlayer = freyaCleaveChoices.find(choice =>
    choice.flags === '1/0' && choice.laneKey === 'normal');
  assert(freyaBasePlayer && freyaAlternatePlayer);
  assert.strictEqual(freyaBasePlayer.linkedToKey, undefined);
  assert.strictEqual(freyaAlternatePlayer.linkedToKey, undefined);
  assert.notStrictEqual(freyaBasePlayer.representative.spec.selectedBodyChild,
    freyaAlternatePlayer.representative.spec.selectedBodyChild);

  const thunderDragonBiteChoices = OB64.animationUI.animationVariantChoices(
    state.specs.filter(animation => animation.spec.classId === 0x39 &&
      animation.spec.actionId === 0x08));
  const thunderDragonBasePlayer = thunderDragonBiteChoices.find(choice =>
    choice.flags === '0/0' && choice.laneKey === 'normal');
  const thunderDragonAlternatePlayer = thunderDragonBiteChoices.find(choice =>
    choice.flags === '1/0' && choice.laneKey === 'normal');
  assert(thunderDragonBasePlayer && thunderDragonAlternatePlayer);
  assert.strictEqual(thunderDragonAlternatePlayer.linkedToKey,
    thunderDragonBasePlayer.key);
  assert(!thunderDragonAlternatePlayer.label.includes('Linked'));
  assert(!thunderDragonAlternatePlayer.label.includes('Same as'));
  assert(thunderDragonAlternatePlayer.linkedTitle.includes(
    'points to the exact same body program'));

  let visibleVariantChoices = 0;
  for (const rows of actionGroups.values()) {
    const choices = OB64.animationUI.animationVariantChoices(rows);
    visibleVariantChoices += choices.length;
    assert.strictEqual(new Set(choices.map(choice => choice.key)).size,
      choices.length, 'every visible variant needs a unique internal key');
    const firstByIdentity = new Map();
    choices.forEach(choice => {
      const identity = OB64.animationUI.animationSequenceIdentity(
        choice.representative);
      const first = firstByIdentity.get(identity);
      if (first) {
        assert.strictEqual(choice.linkedToKey, first.key);
        assert(choice.linkedTitle.includes('exact same body program'));
        assert(!choice.label.includes('Linked'));
        assert.strictEqual(OB64.animationUI.animationSequenceStorageIdentity(
          choice.representative),
        OB64.animationUI.animationSequenceStorageIdentity(first.representative),
        'linked rows must share one body-program storage owner');
        assert.deepStrictEqual(
          [...choice.representative.poseProgram.program],
          [...first.representative.poseProgram.program],
        'linked rows must share every body-program byte');
      } else {
        assert.strictEqual(choice.linkedToKey, undefined);
        firstByIdentity.set(identity, choice);
      }
    });
    choices.filter(choice => choice.laneKey === 'normal').forEach(choice =>
      assert.deepStrictEqual(choice.rawModes, [0, 1]));
    choices.filter(choice => choice.laneKey === 'blocked').forEach(choice =>
      assert.deepStrictEqual(choice.rawModes, [2]));
  }
  assert.strictEqual(visibleVariantChoices, 1808);

  const fighterRows = firstActionRows(state, 0x02).filter(animation =>
    animation.mappingStatus.selectorFlags === '0/0');
  const fighterModeZero = fighterRows.find(animation =>
    animation.spec.rawMode === 0);
  const fighterModeOne = fighterRows.find(animation =>
    animation.spec.rawMode === 1);
  assert(fighterModeZero && fighterModeOne,
    'Fighter must expose raw modes 0 and 1 for shared-edit verification');
  const sharedFighterLayer = fighterModeZero.frames
    .flatMap(frame => frame.layers)
    .find(layer => {
      const source = state.artByKey[layer.sourceKey];
      if (!source || source.sourceRole !== 'body' ||
          !source.animationKeys.includes(fighterModeOne.key)) return false;
      return fighterModeOne.frames.some(frame => frame.layers.some(other =>
        other.sourceKey === layer.sourceKey &&
        other.selectedChildOrdinal === layer.selectedChildOrdinal));
    });
  assert(sharedFighterLayer,
    'Fighter raw modes must share at least one exact body binding and child');
  const sharedFighterSource = state.artByKey[sharedFighterLayer.sourceKey];
  const sharedFighterChild = sharedFighterLayer.selectedChildOrdinal;
  const sharedScope = OB64.animationUI.spriteEditScope(state,
    sharedFighterSource.key, sharedFighterChild);
  assert(sharedScope.frameUses > 1);
  assert(sharedScope.routeCount > 1);
  assert(sharedScope.routes.some(route =>
    route.animationKeys.includes(fighterModeOne.key)));

  const sharedOriginal = OB64.animationArt.originalChild(sharedFighterSource,
    sharedFighterChild);
  const sharedDesired = changedPixels(sharedFighterSource, sharedFighterChild);
  const previewRevisionBeforeEdit = state.editRevision;
  assert.strictEqual(OB64.animationArt.setEdit(state, sharedFighterSource.key,
    sharedFighterChild, sharedDesired.indices, sharedDesired.intensity), true);
  assert.strictEqual(state.editRevision, previewRevisionBeforeEdit + 1,
    'animation preview caches must invalidate after a sprite edit');
  const liveFromOtherRoute = OB64.animationArt.displayChild(state,
    fighterModeOne.artByKey[sharedFighterSource.key].key, sharedFighterChild);
  assert.deepStrictEqual(Array.from(liveFromOtherRoute.indices),
    Array.from(sharedDesired.indices));
  assert.deepStrictEqual(Array.from(liveFromOtherRoute.intensity),
    Array.from(sharedDesired.intensity));
  assert.strictEqual(OB64.animationArt.setEdit(state, sharedFighterSource.key,
    sharedFighterChild, sharedOriginal.indices, sharedOriginal.intensity), true);
  assert.strictEqual(OB64.animationArt.hasEdit(state, sharedFighterSource.key,
    sharedFighterChild), false);

  const uniqueSources = new Map();
  for (const source of Object.values(state.artByKey)) {
    if (!uniqueSources.has(source.physicalSourceId)) {
      uniqueSources.set(source.physicalSourceId, source);
    }
  }
  let directChildren = 0;
  let deltaChildren = 0;
  for (const source of uniqueSources.values()) {
    for (const child of source.sprite.children) {
      if (child.discriminator === child.ordinal) directChildren++;
      else deltaChildren++;
    }
  }
  assert.strictEqual(uniqueSources.size, 3811,
    'attack sources, complete class-art routes, and Fighter selector-0 sources ' +
      'must be loaded once');
  assert.strictEqual(directChildren, 11103);
  assert.strictEqual(deltaChildren, 3967);

  const deltaSource = Object.values(state.artByKey).find(source =>
    source.editable && source.editableChildOrdinals.some(ordinal =>
      source.sprite.children[ordinal].discriminator !== ordinal));
  assert(deltaSource, 'an editable delta child must be present');
  const deltaOrdinal = deltaSource.editableChildOrdinals.find(ordinal =>
    deltaSource.sprite.children[ordinal].discriminator !== ordinal);
  const desired = changedPixels(deltaSource, deltaOrdinal);
  const siblingBefore = deltaSource.sprite.children.map(child =>
    OB64.animationArt.decodeChild(deltaSource.sprite, child.ordinal));
  const rebuiltBytes = OB64.animationArt.buildDecoded(deltaSource, {
    [deltaOrdinal]: desired,
  });
  const rebuilt = OB64.animationArt.parseSpriteObject(rebuiltBytes,
    deltaSource.resourceKey);
  const changed = OB64.animationArt.decodeChild(rebuilt, deltaOrdinal);
  assert.deepStrictEqual(Array.from(changed.indices), Array.from(desired.indices));
  assert.deepStrictEqual(Array.from(changed.intensity), Array.from(desired.intensity));
  rebuilt.children.forEach(child => {
    if (child.ordinal === deltaOrdinal) return;
    const siblingAfter = OB64.animationArt.decodeChild(rebuilt, child.ordinal);
    assert.deepStrictEqual(Array.from(siblingAfter.indices),
      Array.from(siblingBefore[child.ordinal].indices));
    assert.deepStrictEqual(Array.from(siblingAfter.intensity),
      Array.from(siblingBefore[child.ordinal].intensity));
  });

  rom.classDefs = OB64.parseClassDefs(rom.z64);
  OB64.combatAnimationOverrides.initialize(rom);
  let effectiveCatalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  assert.strictEqual(effectiveCatalog.failures.length, 78,
    '13 newly exposed art routes must report both actions and all three raw modes');
  assert(effectiveCatalog.failures.every(failure =>
    failure.flags === '1/0' && failure.selector === 0x28 &&
    failure.message.includes('no drawable layers')),
  'unreferenced player art must remain visible as an exact selector issue');
  assert.strictEqual(effectiveCatalog.specs.filter(animation =>
    animation.spec.assignmentPlaceholder).length, 78,
  'every unresolved body program must retain a visible assignment target');
  assert.strictEqual(effectiveCatalog.specs.length, state.specs.length + 144,
    'the effective catalog must add corrected special routes, visible unresolved ' +
      'routes, and four idle routes for each formerly disabled class');
  let effectiveFighter = effectiveCatalog.specs.filter(animation =>
    animation.spec.classId === 0x02);
  assert.strictEqual(effectiveFighter.length, 12);
  assert.deepStrictEqual([...new Set(effectiveFighter.map(animation =>
    animation.spec.actionId))], [0x04]);
  assert(effectiveFighter.every(animation =>
    animation.effectiveMapping.source === 'vanilla'));

  rom.combatAnimationOverrides.readOnly = true;
  rom.combatAnimationOverrides.disabledReason =
    'Attack Animation override lanes are foreign or partial.';
  const foreignCatalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  assert.strictEqual(foreignCatalog.effective, false);
  assert(foreignCatalog.diagnostic.includes('foreign or partial'));
  rom.combatAnimationOverrides.readOnly = false;
  rom.combatAnimationOverrides.disabledReason = '';

  const fighterDefinition = rom.classDefs[0x02 + 1];
  fighterDefinition.b43Raw = fighterDefinition.b45Raw =
    fighterDefinition.b47Raw = 0x06;
  OB64.combatAnimationOverrides.setEntry(rom.combatAnimationOverrides, {
    classId: 0x02, actionId: 0x06,
    normalSelector: 0x28, blockedSelector: 0x29
  });
  effectiveCatalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  effectiveFighter = effectiveCatalog.specs.filter(animation =>
    animation.spec.classId === 0x02);
  assert.strictEqual(effectiveFighter.length, 12);
  assert(effectiveFighter.every(animation =>
    animation.spec.actionId === 0x06 &&
    animation.spec.actionName === 'Rend' &&
    animation.effectiveMapping.source === 'override'));
  const effectiveFighterBase = effectiveFighter.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '0/0' &&
    animation.spec.rawMode === 0);
  assert(OB64.animationUI.sameAnimationSequence(effectiveFighterBase,
    fighterModeZero));
  const fighterPlayerSequences = OB64.animationUI.animationSequenceCatalogRows(
    state, null, 0x02, 0);
  assert.deepStrictEqual([...new Set(fighterPlayerSequences.map(animation =>
    animation.spec.actionId))], [0x04],
  'live Class Combat actions must not enter the vanilla sequence catalog');
  assert(fighterPlayerSequences.every(animation =>
    OB64.animationUI.selectorFlagParts(animation)[1] === 0));
  const fighterBaseSequences = OB64.animationUI.animationSequenceCatalogRows(
    state, null, 0x02, 0, { flags: '0/0' });
  assert(fighterBaseSequences.length > 0 &&
    fighterBaseSequences.length < fighterPlayerSequences.length &&
    fighterBaseSequences.every(animation =>
      OB64.animationUI.selectorFlags(animation) === '0/0'),
  'the Body Sprite Sequence menu must follow the exact selected art variant');
  const fighterPlayerChoices = OB64.animationUI.animationClassVariantChoices(
    fighterPlayerSequences);
  assert.strictEqual(fighterPlayerChoices.length, 4);
  assert(!fighterPlayerChoices.some(choice =>
    choice.representative.spec.nativeSelectorCandidate),
  'mapped Fighter rows must replace redundant native-program labels');
  OB64.combatAnimationOverrides.selectorOptions(0x02).forEach(option => {
    assert(fighterPlayerChoices.some(choice =>
      choice.representative.spec.selector === option.id),
    'every valid Fighter body program must remain selectable');
  });
  assert(fighterPlayerChoices.every(choice =>
    choice.sequenceKind === 'vanilla' && !choice.label.startsWith('Vanilla') &&
    !/Player Side|Enemy Side|Fighter Art|Variant/.test(choice.label)));
  assert(!fighterPlayerChoices.some(choice => choice.sourceActionId === 0x06),
    'switching the live attack to Rend must not create a Rend sequence entry');
  const fighterEnemySequences = OB64.animationUI.animationSequenceCatalogRows(
    state, null, 0x02, 1);
  assert(fighterEnemySequences.length > 0 && fighterEnemySequences.every(animation =>
    OB64.animationUI.selectorFlagParts(animation)[1] === 1),
  'the sequence catalog must expose only the selected enemy side');

  const warriorDefinition = rom.classDefs[0x55 + 1];
  warriorDefinition.b47Raw = 0x2E;
  OB64.combatAnimationOverrides.setEntry(rom.combatAnimationOverrides, {
    classId: 0x55, actionId: 0x2E,
    normalSelector: 0x28, blockedSelector: 0x28,
  });
  effectiveCatalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  const liveWarrior = effectiveCatalog.specs.filter(animation =>
    animation.spec.classId === 0x55);
  const warriorPlayerSequences = OB64.animationUI.animationSequenceCatalogRows(
    state, null, 0x55, 0);
  assert.deepStrictEqual([...new Set(warriorPlayerSequences.map(animation =>
    animation.spec.actionId))], [0x04, 0x2D],
  'the vanilla catalog must retain Tier 1 and reject the live Tier 2 assignment');
  const warriorPlayerChoices = OB64.animationUI.animationClassVariantChoices(
    warriorPlayerSequences);
  assert(warriorPlayerChoices.some(choice => choice.sourceActionId === 0x2D &&
    choice.label.includes('Elemental Tier 1 Spell Template')));
  assert(!warriorPlayerChoices.some(choice => choice.sourceActionId === 0x2E),
    'a Class-tab Tier 2 assignment must not add a sequence entry');

  const uiRoute = {};
  OB64.animationUI.requestAnimationRoute(uiRoute, {
    classId: 0x02, actionId: 0x06, laneKey: 'blocked'
  });
  assert.deepStrictEqual(uiRoute.animationRouteRequest, {
    classId: 0x02, actionId: 0x06, laneKey: 'blocked'
  });
  const directlyOpened = OB64.animationUI.selectedAnimation(
    rom.art, uiRoute, effectiveCatalog);
  assert.strictEqual(directlyOpened.spec.classId, 0x02);
  assert.strictEqual(directlyOpened.spec.actionId, 0x06);
  assert.strictEqual(OB64.animationUI.animationLaneKey(directlyOpened), 'blocked');
  assert.strictEqual(uiRoute.animationRouteRequest, undefined);
  assert.strictEqual(uiRoute.animationFrame, 0);
  assert.strictEqual(uiRoute.animationLayer,
    directlyOpened.frames[0].layers[0].ordinal);

  const classOnlyRoute = {};
  OB64.animationUI.requestAnimationRoute(classOnlyRoute, {
    classId: 0x55,
  });
  assert.deepStrictEqual(classOnlyRoute.animationRouteRequest, {
    classId: 0x55,
  });
  const classOnlyOpened = OB64.animationUI.selectedAnimation(
    rom.art, classOnlyRoute, effectiveCatalog);
  assert.strictEqual(classOnlyOpened.spec.classId, 0x55);
  assert.strictEqual(OB64.animationUI.animationLaneKey(classOnlyOpened), 'normal');
  assert.strictEqual(classOnlyRoute.animationTargetClassId, 0x55);
  assert.strictEqual(classOnlyRoute.animationTargetActionId,
    classOnlyOpened.spec.actionId);
  assert.strictEqual(classOnlyRoute.animationTargetFlags,
    OB64.animationUI.selectorFlags(classOnlyOpened));
  assert.strictEqual(classOnlyRoute.animationTargetLaneKey, 'normal');
  const warriorArtRoutes = OB64.animationUI.animationArtRouteChoices(
    effectiveCatalog.specs.filter(animation =>
      animation.spec.classId === 0x55 &&
      animation.spec.actionId === classOnlyOpened.spec.actionId));
  assert.strictEqual(warriorArtRoutes.length, 4);
  assert.deepStrictEqual(warriorArtRoutes.map(route => route.flags),
    ['0/0', '0/1', '1/0', '1/1']);

  const soldierDefinition = rom.classDefs[0x01 + 1];
  soldierDefinition.b43Raw = soldierDefinition.b45Raw =
    soldierDefinition.b47Raw = 0x06;
  OB64.combatAnimationOverrides.setEntry(rom.combatAnimationOverrides, {
    classId: 0x01, actionId: 0x06,
    normalSelector: 0x2A, blockedSelector: 0x2B
  });
  const usedBeforeDynamic = state.corpus.usedBindingCount;
  effectiveCatalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  const effectiveSoldier = effectiveCatalog.specs.filter(animation =>
    animation.spec.classId === 0x01);
  assert.strictEqual(effectiveSoldier.length, 12);
  assert(effectiveSoldier.filter(animation => animation.spec.rawMode !== 2)
    .every(animation => animation.spec.selector === 0x2A));
  assert(effectiveSoldier.filter(animation => animation.spec.rawMode === 2)
    .every(animation => animation.spec.selector === 0x2B));
  assert.strictEqual(effectiveCatalog.failures.filter(failure =>
    failure.classId === 0x01).length, 0);
  assert(state.corpus.usedBindingCount >= usedBeforeDynamic);
  assert(effectiveSoldier.every(animation => animation.frames.length > 0));
  const dynamicSoldierBase = effectiveSoldier.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '0/0' &&
    animation.spec.rawMode === 0);
  assert.strictEqual(dynamicSoldierBase.spec.dynamicSelectorCandidate, true);
  assert(dynamicSoldierBase.effectiveMapping.candidateKey.includes(
    'selector-2a'));
  assert.notStrictEqual(effectiveSoldier.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '0/0').spec.selectedBodyChild,
  effectiveSoldier.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '1/0').spec.selectedBodyChild);

  const activeDescriptors = new Set(state.specs.map(animation =>
    animation.spec.descriptorKey));
  const dormantBinding = Object.values(OB64.animationCorpusData.physicalSources)
    .filter(physical => physical.format.kind === 'indexed-ci8')
    .flatMap(physical => physical.bindings)
    .find(binding => activeDescriptors.has(binding.descriptorKey) &&
      !state.artByKey[binding.id]);
  assert(dormantBinding, 'the accepted corpus must retain a dormant CI8 binding');
  const dormantSource = state.resolveBindingSource(dormantBinding.id, 0);
  assert.strictEqual(dormantSource.onDemandBinding, true);
  const dormantEdit = changedPixels(dormantSource, 0);
  assert.strictEqual(OB64.animationArt.setEdit(state, dormantSource.key, 0,
    dormantEdit.indices, dormantEdit.intensity), true);
  const dormantPayload = OB64.animationArt.collectProject(state);
  assert(dormantPayload[dormantSource.key]);
  const freshState = OB64.animationArt.initialize(rom.z64);
  const preparedDormant = OB64.animationArt.prepareProject(freshState,
    dormantPayload);
  assert.strictEqual(preparedDormant.count, 1);
  assert(preparedDormant.edits[dormantSource.key]);

  const idleClassIds = Object.keys(OB64.CLASS_NAMES).map(Number)
    .filter(classId => classId > 0);
  const allIdleRows = idleClassIds.flatMap(classId =>
    OB64.animationUI.idleAnimationRows(state, classId));
  assert.strictEqual(idleClassIds.length, 164);
  assert.strictEqual(allIdleRows.length, 656,
    'every class and art route must expose its selector-0 idle loop');
  assert.strictEqual(Object.values(state.idleSequenceFailures).flat().length, 0);
  assert(allIdleRows.every(animation =>
    animation.spec.selector === 0 && animation.frames.length > 0 &&
    animation.frames.every((frame, index) => frame.sequenceIndex === index)));
  assert.strictEqual(new Set(Object.values(state.artByKey).map(source =>
    source.physicalSourceId)).size, 4781,
  'all accepted and directly discovered class-art sources must load once');

  const fighterCompleteCopyRows = [0, 1].flatMap(side =>
    OB64.animationUI.animationSequenceCatalogRows(
      state, null, 0x02, side, fighterCopyCatalogOptions));
  const fighterCopyMotionRows = fighterCompleteCopyRows.filter(
    OB64.animationUI.isClassMotionAnimation);
  assert.strictEqual(fighterCopyMotionRows.length, 8,
    'Copy From and Separate must list both movement selectors for every Fighter art route');
  assert.deepStrictEqual([...new Set(fighterCopyMotionRows.map(animation =>
    animation.spec.classMotionKind))], ['advance', 'return']);
  const fighterCompleteCopyChoices =
    OB64.animationUI.animationClassVariantChoices(fighterCompleteCopyRows);
  assert(fighterCompleteCopyChoices.some(choice =>
    choice.label.includes('Idle / Rest')));
  assert(fighterCompleteCopyChoices.some(choice =>
    choice.label.includes('Walk / Run · Advance')));
  assert(fighterCompleteCopyChoices.some(choice =>
    choice.label.includes('Walk / Run · Return')));
  const baldwinCopyChoices = OB64.animationUI.animationClassVariantChoices(
    [0, 1].flatMap(side => OB64.animationUI.animationSequenceCatalogRows(
      state, null, 0x67, side, fighterCopyCatalogOptions)));
  assert(baldwinCopyChoices.some(choice => choice.label.includes('Idle / Rest')),
    'Baldwin Copy From must list Idle / Rest');
  assert(baldwinCopyChoices.some(choice =>
    choice.label.includes('Walk / Run · Advance')),
  'Baldwin Copy From must list movement advance');
  assert(baldwinCopyChoices.some(choice =>
    choice.label.includes('Walk / Run · Return')),
  'Baldwin Copy From must list movement return');

  const giantMovementRows = OB64.animationUI.classMotionAnimationRows(
    state, 0x4D);
  assert.strictEqual(giantMovementRows.length, 6,
    'Giant must expose both fixed movement selectors on each drawable art route');
  assert.strictEqual(OB64.animationUI.classMotionAnimationRows(state, 0x4D),
    giantMovementRows,
  'fixed movement selectors must be resolved once and cached by class');
  assert.deepStrictEqual(giantMovementRows.map(animation =>
    animation.spec.actionId), [-2, -2, -2, -3, -3, -3]);
  assert.deepStrictEqual(giantMovementRows.map(animation =>
    animation.spec.selector), [0x05, 0x05, 0x05, 0x0B, 0x0B, 0x0B]);
  assert.deepStrictEqual(giantMovementRows.map(animation =>
    OB64.animationUI.selectorFlags(animation)),
  ['0/0', '1/0', '1/1', '0/0', '1/0', '1/1']);
  assert(giantMovementRows.slice(0, 3).every(animation =>
    OB64.animationUI.isClassMotionAnimation(animation) &&
    OB64.animationUI.animationLaneKey(animation) === 'advance' &&
    OB64.animationUI.animationLaneLabel(animation) ===
      'Walk / Run · Advance'));
  assert(giantMovementRows.slice(3).every(animation =>
    OB64.animationUI.isClassMotionAnimation(animation) &&
    OB64.animationUI.animationLaneKey(animation) === 'return' &&
    OB64.animationUI.animationLaneLabel(animation) ===
      'Walk / Run · Return'));
  assert.deepStrictEqual(state.classMotionSequenceFailures[0x4D].map(failure =>
    [failure.kind, failure.selector, failure.flags]), [
      ['advance', 0x05, '0/1'],
      ['return', 0x0B, '0/1'],
    ], 'Giant must retain its unreadable player-side alternate route as an issue');
  const giantIdleRows = OB64.animationUI.idleAnimationRows(state, 0x4D);
  const giantEnemyBaseIdle = giantIdleRows.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '0/1');
  const giantEnemyAlternateIdle = giantIdleRows.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '1/1');
  assert(giantEnemyBaseIdle && giantEnemyAlternateIdle);
  assert.strictEqual(OB64.animationUI.animationArtName(
    giantEnemyBaseIdle), 'Giant');
  assert.strictEqual(OB64.animationUI.animationArtName(
    giantEnemyAlternateIdle), 'Soldier');
  const giantRouteUi = {};
  OB64.animationUI.rememberAnimationTarget(
    giantRouteUi, giantEnemyBaseIdle);
  const giantAdvanceFallback = OB64.animationUI.preferredAnimationTarget(
    giantMovementRows.filter(animation =>
      animation.spec.classMotionKind === 'advance'),
    giantRouteUi, 0x4D, -2, 'advance', '0/1');
  assert.strictEqual(OB64.animationUI.selectorFlags(
    giantAdvanceFallback), '1/1',
  'Giant movement must use the available enemy-side fallback route');
  OB64.animationUI.rememberAnimationTarget(
    giantRouteUi, giantAdvanceFallback);
  assert.strictEqual(OB64.animationUI.preferredAnimationTarget(
    giantIdleRows, giantRouteUi, 0x4D, -1, 'idle',
    OB64.animationUI.selectorFlags(giantAdvanceFallback)),
  giantEnemyBaseIdle,
  'returning to Idle must restore the earlier Giant enemy-side Base Art route');
  const giantAdvanceBase = giantMovementRows.find(animation =>
    animation.spec.classMotionKind === 'advance' &&
    OB64.animationUI.selectorFlags(animation) === '0/0');
  const giantReturnBase = giantMovementRows.find(animation =>
    animation.spec.classMotionKind === 'return' &&
    OB64.animationUI.selectorFlags(animation) === '0/0');
  assert.deepStrictEqual(giantAdvanceBase.frames.map(frame =>
    [frame.token, frame.ticks]), giantReturnBase.frames.map(frame =>
    [frame.token, frame.ticks]),
  'Giant advance and return selectors must expose their shared eight-frame program');
  const giantAdvanceCatalog = OB64.animationUI.animationSequenceCatalogRows(
    state, null, 0x4D, 0, {
      classMotionKind: 'advance', flags: '0/0'
    });
  assert.deepStrictEqual(giantAdvanceCatalog, [giantAdvanceBase],
    'movement selection must show only its fixed selector and art route');
  const giantAdvanceChoices = OB64.animationUI.animationClassVariantChoices(
    giantAdvanceCatalog);
  assert.strictEqual(giantAdvanceChoices.length, 1);
  assert(giantAdvanceChoices[0].label.includes('Walk / Run · Advance'));
  assert(giantAdvanceChoices[0].optionTitle.includes(
    'Fixed class movement selector 0x05'));
  const giantActions = OB64.animationUI.animationActionChoices(
    [], giantAdvanceBase);
  assert.deepStrictEqual(giantActions.map(animation =>
    animation.spec.actionId), [-1, -2, -3],
  'a class without combat attacks must still expose idle and movement art');

  const giantDefinition = rom.classDefs[0x4D + 1];
  assert.strictEqual(giantDefinition.isTerm, true,
    'the retail Giant slot must begin as a terminator-classified record');
  giantDefinition.stats[0].base = 0x005A;
  giantDefinition.b43Raw = giantDefinition.b45Raw = 0x0B;
  giantDefinition.b47Raw = 0x29;
  OB64.refreshClassDefClassification(giantDefinition);
  effectiveCatalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  const customGiantRows = effectiveCatalog.specs.filter(animation =>
    animation.spec.classId === 0x4D);
  assert.deepStrictEqual([...new Set(customGiantRows.map(animation =>
    animation.spec.actionId))], [0x0B, 0x29],
  'a converted Giant class record must expose Smash and Earthquake routes');
  assert.deepStrictEqual(OB64.animationUI.animationActionChoices(
    customGiantRows, customGiantRows[0]).map(animation => animation.spec.actionId),
  [-1, -2, -3, 0x0B, 0x29],
  'the Giant Action dropdown must include fixed movement and live attacks');
  assert(!state.specs.some(OB64.animationUI.isClassMotionAnimation),
    'on-demand movement routes must not expand the startup combat corpus');

  console.log('PASS combat mapping, live Class Combat routes, and selector previews');
})().catch(error => {
  console.error(error && error.stack ? error.stack : error);
  process.exitCode = 1;
});
