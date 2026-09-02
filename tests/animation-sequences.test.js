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
  'data.js', 'parsers.js', 'art.js', 'sprite-library.js',
  'animation-corpus-data.js',
  'animation-art.js', 'combat-animation-overrides-data.js',
  'combat-animation-overrides.js', 'animation-sequences.js', 'animation-ui.js'
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

async function freshRom(z64) {
  const rom = { z64: z64.slice(), layout: { id: 'us-rev0' } };
  await OB64.art.initialize(rom);
  rom.classDefs = OB64.parseClassDefs(rom.z64);
  OB64.combatAnimationOverrides.initialize(rom);
  OB64.animationSequences.initialize(rom);
  return rom;
}

function route(rom, classId, actionId, flags, rawMode) {
  const catalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  return catalog.specs.find(animation =>
    animation.spec.classId === classId &&
    animation.spec.actionId === actionId &&
    OB64.animationUI.selectorFlags(animation) === flags &&
    animation.spec.rawMode === rawMode);
}

(async function run() {
  assert.deepStrictEqual(
    OB64.animationUI.animationBrushIndices(5, 5, 2, 2, 1), [12],
    'a one-pixel brush must edit only the selected pixel');
  assert.deepStrictEqual(
    OB64.animationUI.animationBrushIndices(5, 5, 2, 2, 3),
    [6, 7, 8, 11, 12, 13, 16, 17, 18],
    'a three-pixel brush must edit a centered square');
  assert.deepStrictEqual(
    OB64.animationUI.animationBrushIndices(5, 5, 0, 0, 3),
    [0, 1, 5, 6],
    'a brush must clip cleanly at sprite bounds');
  const transformIndices = new Uint8Array([1, 2, 3, 4, 5, 6]);
  const transformIntensity = new Uint8Array([6, 5, 4, 3, 2, 1]);
  const rotatedRight = OB64.animationSequences.rotateIndexedPixels(
    transformIndices, transformIntensity, 2, 3, 'right');
  assert.deepStrictEqual(
    { width: rotatedRight.width, height: rotatedRight.height },
    { width: 3, height: 2 });
  assert.deepStrictEqual([...rotatedRight.indices], [5, 3, 1, 6, 4, 2],
    'right rotation must preserve exact palette indexes');
  assert.deepStrictEqual([...rotatedRight.intensity], [2, 4, 6, 1, 3, 5],
    'right rotation must keep each I4 value with its source pixel');
  const rotatedBack = OB64.animationSequences.rotateIndexedPixels(
    rotatedRight.indices, rotatedRight.intensity,
    rotatedRight.width, rotatedRight.height, 'left');
  assert.deepStrictEqual([...rotatedBack.indices], [...transformIndices],
    'left rotation must reverse a right rotation exactly');
  assert.deepStrictEqual([...rotatedBack.intensity], [...transformIntensity]);
  const resizedPixels = OB64.animationSequences.resizeIndexedPixels(
    new Uint8Array([1, 2, 3, 4]), new Uint8Array([4, 3, 2, 1]),
    2, 2, 4, 4);
  assert.deepStrictEqual([...resizedPixels.indices], [
    1, 1, 2, 2,
    1, 1, 2, 2,
    3, 3, 4, 4,
    3, 3, 4, 4,
  ], 'nearest-neighbor resize must replicate source pixels without blending');
  assert.deepStrictEqual([...resizedPixels.intensity], [
    4, 4, 3, 3,
    4, 4, 3, 3,
    2, 2, 1, 1,
    2, 2, 1, 1,
  ], 'nearest-neighbor resize must preserve the matching I4 values');
  const colorRichRgba = new Uint8ClampedArray(17 * 17 * 4);
  for (let pixel = 0; pixel < 17 * 17; pixel++) {
    colorRichRgba[pixel * 4] = (pixel % 17) * 16;
    colorRichRgba[pixel * 4 + 1] = Math.floor(pixel / 17) * 16;
    colorRichRgba[pixel * 4 + 2] = (pixel * 37) & 0xFF;
    colorRichRgba[pixel * 4 + 3] = pixel === 0 ? 0 : 255;
  }
  const quantizedFrame = OB64.art.prepareAnimationFrameImport(
    colorRichRgba, 17, 17, 17, 17,
    { resizeMode: 'nearest', dither: true });
  assert(quantizedFrame.colorCount <= 256,
    'frame import must fit the native CI8 palette limit');
  assert.strictEqual(quantizedFrame.paletteWords.length, 256);
  assert.strictEqual(quantizedFrame.intensity[0], 0,
    'frame import must preserve transparent pixels in the I4 lane');
  assert(quantizedFrame.intensity.slice(1).every(value => value === 15),
    'opaque frame-import pixels must use maximum I4 intensity');

  const z64 = normalizeV64(fs.readFileSync(ROM));
  const rom = await freshRom(z64);
  assert.strictEqual(rom.animationSequences.supported, true);
  const initialCatalog = OB64.animationUI.effectiveAnimationCatalog(rom.art, rom);
  const allClassChoices = OB64.animationUI.animationClassChoices(
    initialCatalog.specs);
  assert.strictEqual(allClassChoices.length, 164);
  assert.deepStrictEqual(allClassChoices.filter(row => row.missingAnimation), [],
    'the effective animation catalog must keep every class selectable');
  const formerlyDisabled = [
    0x4D, 0x64, 0x7D, 0x7E, 0x7F, 0x80,
    0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
  ];
  formerlyDisabled.forEach(classId => {
    const rows = initialCatalog.specs.filter(animation =>
      animation.spec.classId === classId);
    assert.strictEqual(rows.length, 4,
      `class 0x${classId.toString(16)} must expose four idle art routes`);
    assert(rows.every(OB64.animationUI.isIdleAnimation));
    assert.deepStrictEqual(rows.map(OB64.animationUI.selectorFlags).sort(),
      ['0/0', '0/1', '1/0', '1/1']);
  });
  const amriusPlayerRoute = route(rom, 0x63, 0x04, '1/0', 0);
  assert(amriusPlayerRoute,
    'Dark Prince player-side art must remain selectable without a vanilla attack route');
  assert.strictEqual(amriusPlayerRoute.spec.assignmentPlaceholder, true);
  assert.strictEqual(amriusPlayerRoute.spec.descriptorKey, 0x00F3BBD6);
  assert.strictEqual(amriusPlayerRoute.spec.route.rawHandleU16, 0x00A5);
  assert.strictEqual(amriusPlayerRoute.effectiveMapping.assignmentRequired, true);
  assert.strictEqual(amriusPlayerRoute.effectiveMapping.selector, 0x28);
  assert(amriusPlayerRoute.frames.length > 0,
    'an unassigned art route must still provide a visible editable preview');
  const bossCopySourceKey = 'binding-00453ec0-129-009e8358';
  assert.strictEqual(initialCatalog.sourceAnimations[bossCopySourceKey].some(
    animation => animation.spec.classId === 0xA0), false,
  'the current-assignment catalog reproduces the missing boss-copy consumer');
  rom.art.animations.activeSourceAnimations =
    OB64.animationUI.editScopeSourceIndex(
      rom.art.animations, initialCatalog, rom.animationSequences);
  const bossCopyScope = OB64.animationUI.spriteEditScope(
    rom.art.animations, bossCopySourceKey, 0);
  assert.deepStrictEqual([...new Set(bossCopyScope.routes.map(route => route.classId))]
    .sort((left, right) => left - right),
  [0x13, 0x54, 0x55, 0xA0, 0xA2],
  'shared-sprite scope must retain every vanilla boss-copy consumer');
  assert(bossCopyScope.routes.filter(route =>
    route.classId === 0xA0 || route.classId === 0xA2).every(route => !route.assigned),
  'vanilla boss-copy routes outside the live assignment catalog must be labeled unassigned');

  const grapplerSource = rom.art.animations.artByKey[
    'binding-005da450-025-00b6e748'];
  assert.deepStrictEqual(
    OB64.animationUI.identicalSpriteSlots(grapplerSource, 5), [4],
    'Vad Base Art must expose its pixel-identical but independent Grappler slot');
  const vadBaseScope = OB64.animationUI.spriteEditScope(
    rom.art.animations, grapplerSource.key, 5);
  const grapplerSlotScope = OB64.animationUI.spriteEditScope(
    rom.art.animations, grapplerSource.key, 4);
  assert.deepStrictEqual([...new Set(vadBaseScope.routes.map(route => route.classId))],
    [0x5D], 'Vad Base Art child 5 must remain an independent edit target');
  assert.deepStrictEqual([...new Set(grapplerSlotScope.routes.map(route => route.classId))]
    .sort((left, right) => left - right),
  [0x5D, 0x72],
  'Grappler child 4 must remain shared with Vad Alternate Art');
  const descriptorRoot = OB64.art.readResource(z64, 0x003B6CD0);
  const classHandles = OB64.art.readResource(z64, 0x00315736);
  const handleContracts = new Map();
  rom.art.animations.specs.forEach(animation => {
    const index = OB64.animationSequences.handleIndex(rom, animation);
    const rawHandle = OB64.art.readU16(
      classHandles.stored, 0x24 + index * 2);
    assert.strictEqual(rawHandle, animation.spec.route.rawHandleU16,
      'the computed class/body handle index must contain the accepted raw handle');
    const descriptorSlot = (rawHandle & 0x0FFF) - 1;
    assert(descriptorSlot >= 0, 'the accepted descriptor handle must be one-based');
    assert.strictEqual(
      OB64.art.readU32(descriptorRoot.stored, descriptorSlot * 4),
      animation.spec.descriptorKey,
      'the accepted raw handle must resolve the selected descriptor key'
    );
    const contract = [
      animation.spec.descriptorKey,
      animation.spec.route.rawHandleU16,
    ].join(':');
    if (handleContracts.has(index)) {
      assert.strictEqual(handleContracts.get(index), contract,
        'one class/body handle index must not resolve incompatible descriptors');
    } else {
      handleContracts.set(index, contract);
    }
  });

  const fighter = route(rom, 0x02, 0x04, '0/0', 0);
  assert(fighter, 'Fighter Slash player/base normal route must resolve');
  assert.strictEqual(OB64.animationSequences.handleIndex(rom, fighter), 8,
    'ordinary Fighter flags 0/0 must use source-art handle index 8');
  const ariosh = route(rom, 0x90, 0x04, '0/0', 0);
  assert(ariosh, 'Ariosh Knight Slash player/base route must resolve');
  const arioshFields = ariosh.spec.route.actorFields;
  assert.strictEqual(
    rom.classDefs[arioshFields.physicalClassRecord].classCopyMatch,
    arioshFields.rawOwnerContext,
    'Ariosh must exercise the MIPS class-copy-match branch'
  );
  assert.strictEqual(
    OB64.animationSequences.handleIndex(rom, ariosh),
    4 * arioshFields.sourceArtId,
    'a boss copy whose B57 matches its owner must keep its own source-art handle'
  );
  assert.notStrictEqual(
    OB64.animationSequences.handleIndex(rom, ariosh),
    4 * arioshFields.rawOwnerContext,
    'a boss copy must not overwrite the ordinary class handle it borrows as owner context'
  );
  const pair = OB64.combatAnimationOverrides.vanillaPairForLiveAction(
    0x02, rom.classDefs[0x03], 0x04);
  assert(pair && Number.isInteger(pair.normalSelector) &&
    Number.isInteger(pair.blockedSelector));

  const idleRom = await freshRom(z64);
  const fighterIdle = OB64.animationUI.idleAnimationRows(
    idleRom.art.animations, 0x02).find(animation =>
      OB64.animationUI.selectorFlags(animation) === '0/0');
  assert(fighterIdle, 'Fighter player-side Base Art idle loop must resolve');
  assert.strictEqual(fighterIdle.spec.actionId, -1);
  assert.strictEqual(fighterIdle.spec.selector, 0);
  const idleDesiredBefore = JSON.stringify(
    idleRom.combatAnimationOverrides.desired);
  const idleFrameCount = fighterIdle.frames.length;
  const idleSeparation = OB64.animationSequences.separateAndAssign(
    idleRom, fighterIdle, null, fighterIdle);
  assert.strictEqual(idleSeparation.id, '2:-1:0:idle');
  assert.strictEqual(idleSeparation.selector, 0,
    'a private idle loop must replace selector 0 inside its cloned art route');
  assert.strictEqual(idleSeparation.syntheticAnimation.frames.length,
    idleFrameCount);
  assert.strictEqual(idleSeparation.syntheticAnimation.spec.idleSequence, true);

  const specialIdleRom = await freshRom(z64);
  const specialClassIdle = OB64.animationUI.idleAnimationRows(
    specialIdleRom.art.animations, 0x64).find(animation =>
      OB64.animationUI.selectorFlags(animation) === '1/0');
  assert(specialClassIdle,
    'a class with no vanilla attack must still expose its player-side art route');
  const specialClassSeparation = OB64.animationSequences.separateAndAssign(
    specialIdleRom, specialClassIdle, null, specialClassIdle);
  assert.strictEqual(specialClassSeparation.classId, 0x64);
  assert.strictEqual(specialClassSeparation.bodyFlags, 2);
  assert.strictEqual(specialClassSeparation.syntheticAnimation.frames.length,
    specialClassIdle.frames.length,
  'a previously disabled class must accept a private editable sequence');

  const unassignedRom = await freshRom(z64);
  const amriusUnassigned = route(unassignedRom, 0x63, 0x04, '1/0', 0);
  assert(amriusUnassigned && amriusUnassigned.spec.assignmentPlaceholder);
  const amriusSeparation = OB64.animationSequences.separateAndAssign(
    unassignedRom, amriusUnassigned,
    { normalSelector: 0x28, blockedSelector: 0x28 }, amriusUnassigned);
  assert.strictEqual(amriusSeparation.classId, 0x63);
  assert.strictEqual(amriusSeparation.bodyFlags, 2);
  assert.strictEqual(OB64.animationSequences.handleIndex(
    unassignedRom, amriusUnassigned), 0x63 * 4 + 2,
  'a newly exposed player route must repoint its own class-art handle');
  assert(OB64.combatAnimationOverrides.exactEntry(
    unassignedRom.combatAnimationOverrides, 0x63, 0x04, 2),
  'copying an unassigned player route must create an exact route assignment');
  assert.strictEqual(JSON.stringify(idleRom.combatAnimationOverrides.desired),
    idleDesiredBefore,
  'idle separation must not consume or alter an attack assignment record');
  assert.deepStrictEqual(idleRom.animationSequences.routeBaselines, {},
    'idle separation must not create an attack selector baseline');
  assert.strictEqual(OB64.animationSequences.routeSeparationFor(
    fighterIdle, idleRom.animationSequences), idleSeparation);
  const privateIdleRecords = idleSeparation.syntheticAnimation.poseProgram.records;
  const privateIdleLoop = privateIdleRecords[privateIdleRecords.length - 1];
  assert.strictEqual(privateIdleLoop.opcode, 0x04,
    'a private idle body program must keep its final loop command');
  assert.strictEqual(privateIdleLoop.operands[0], 0,
    'a private idle body program must start at its first editable loop frame');
  assert.strictEqual(privateIdleRecords[privateIdleLoop.operands[0]].opcode, 0x01,
    'the rebuilt idle loop must jump to a visible frame command');
  assert.strictEqual(privateIdleRecords.filter(record =>
    record.opcode === 0x01).length, idleFrameCount,
  'a private idle body program must not retain hidden native startup frames');
  assert.strictEqual(privateIdleRecords.slice(privateIdleLoop.operands[0])
    .filter(record => record.opcode === 0x01).length, idleFrameCount,
  'the rebuilt idle jump must cover every visible loop frame');
  const idleCatalog = OB64.animationUI.animationSequenceCatalogRows(
    idleRom.art.animations, idleRom.animationSequences, 0x02, 0,
    { idleOnly: true, flags: '0/0' });
  assert(idleCatalog.includes(fighterIdle));
  assert(idleCatalog.includes(idleSeparation.syntheticAnimation),
    'the private idle loop must remain selectable beside the original ROM loop');
  const idleCatalogChoices = OB64.animationUI.animationClassVariantChoices(
    idleCatalog);
  assert.strictEqual(idleCatalogChoices.length, 2);
  const privateIdleChoice = idleCatalogChoices.find(choice =>
    choice.sequenceKind === 'modified' &&
    choice.representative === idleSeparation.syntheticAnimation);
  assert(privateIdleChoice,
  'the private idle loop must remain a separate selectable entry');
  assert.strictEqual(OB64.animationUI.currentSequenceChoiceForTarget(
    idleCatalogChoices, idleSeparation.syntheticAnimation), privateIdleChoice,
  'the idle dropdown must identify the assigned private route as current');
  const idleTabRestoreUi = {
    animationKey: fighterIdle.key,
    animationTargetClassId: fighterIdle.spec.classId,
    animationTargetActionId: fighterIdle.spec.actionId,
    animationTargetFlags: '0/0',
    animationTargetLaneKey: 'idle',
    animationRestoreAssignedRoute: true,
  };
  assert.strictEqual(OB64.animationUI.selectedAnimation(
    idleRom.art, idleTabRestoreUi,
    OB64.animationUI.effectiveAnimationCatalog(idleRom.art, idleRom), idleRom),
  idleSeparation.syntheticAnimation,
  'returning to the animation tab must restore the assigned private idle route');
  assert.strictEqual(idleTabRestoreUi.animationRestoreAssignedRoute, undefined,
    'fixed-route restoration must be a one-render request');

  const giantRouteRom = await freshRom(z64);
  const giantIdleRows = OB64.animationUI.idleAnimationRows(
    giantRouteRom.art.animations, 0x4D);
  const giantEnemyBase = giantIdleRows.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '0/1');
  const giantEnemyAlternate = giantIdleRows.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '1/1');
  const giantPlayerBase = giantIdleRows.find(animation =>
    OB64.animationUI.selectorFlags(animation) === '0/0');
  assert(giantEnemyBase && giantEnemyAlternate && giantPlayerBase,
    'Giant must expose both enemy body-flag routes and one player route');
  const giantIdleSeparation = OB64.animationSequences.separateAndAssign(
    giantRouteRom, giantEnemyBase, null, giantEnemyBase);
  const giantIdleFrame = giantIdleSeparation.syntheticAnimation.frames.length - 1;
  const originalGiantTick = giantIdleSeparation.syntheticAnimation
    .frames[giantIdleFrame].ticks;
  const giantIdleTick = originalGiantTick === 0xFE
    ? 0xFD : originalGiantTick + 1;
  OB64.animationSequences.setFrameTicks(
    giantRouteRom, giantIdleSeparation, giantIdleFrame, giantIdleTick);
  const giantEditedSource = Object.values(
    giantIdleSeparation.syntheticAnimation.artByKey).find(source =>
    source.editable);
  const giantEditedChild = giantEditedSource.editableChildOrdinals[0];
  const giantPixels = OB64.animationArt.currentEdit(
    giantRouteRom.art.animations, giantEditedSource.key, giantEditedChild);
  const giantEditedIndices = giantPixels.indices.slice();
  giantEditedIndices[0] = (giantEditedIndices[0] + 1) & 0xFF;
  OB64.animationArt.setEdit(giantRouteRom.art.animations,
    giantEditedSource.key, giantEditedChild,
    giantEditedIndices, giantPixels.intensity);
  const giantMotionRows = OB64.animationUI.classMotionAnimationRows(
    giantRouteRom.art.animations, 0x4D);
  const giantAdvance = giantMotionRows.find(animation =>
    animation.spec.classMotionKind === 'advance' &&
    OB64.animationUI.selectorFlags(animation) === '1/1');
  const giantReturn = giantMotionRows.find(animation =>
    animation.spec.classMotionKind === 'return' &&
    OB64.animationUI.selectorFlags(animation) === '1/1');
  assert(giantAdvance && giantReturn,
    'Giant flags 1/1 must expose both fixed movement routes');
  const giantAdvanceSeparation = OB64.animationSequences.separateAndAssign(
    giantRouteRom, giantAdvance, null, giantAdvance);
  const giantReturnSeparation = OB64.animationSequences.separateAndAssign(
    giantRouteRom, giantReturn, null, giantReturn);
  const giantEnemyTargets = OB64.animationUI.fixedSideRouteTargets(
    giantRouteRom.art.animations, giantEnemyBase);
  assert.deepStrictEqual(giantEnemyTargets.map(animation =>
    OB64.animationUI.selectorFlags(animation)), ['0/1', '1/1'],
  'same-side fixed assignment must include both Giant enemy routes');
  const giantRoutesBeforeInvalid = Object.keys(
    giantRouteRom.animationSequences.separations).sort();
  assert.throws(() => OB64.animationSequences.assignFixedToTargets(
    giantRouteRom, giantIdleSeparation.syntheticAnimation,
    [giantEnemyBase, giantPlayerBase]), /one player or enemy side/);
  assert.deepStrictEqual(Object.keys(
    giantRouteRom.animationSequences.separations).sort(),
  giantRoutesBeforeInvalid,
  'mixed-side validation must not change any private route');
  const giantDesiredBefore = JSON.stringify(
    giantRouteRom.combatAnimationOverrides.desired);
  const assignedGiantIdles = OB64.animationSequences.assignFixedToTargets(
    giantRouteRom, giantIdleSeparation.syntheticAnimation, giantEnemyTargets);
  assert.deepStrictEqual(assignedGiantIdles.map(row => row.bodyFlags), [1, 3]);
  const alternateGiantIdle = giantRouteRom.animationSequences.separations[
    '77:-1:3:idle'];
  assert(alternateGiantIdle,
    'same-side assignment must create the runtime Giant flags 1/1 idle route');
  const alternateGiantSource = Object.values(
    alternateGiantIdle.syntheticAnimation.artByKey).find(source =>
    source.separationSourceOrdinal ===
      giantEditedSource.separationSourceOrdinal);
  assert(alternateGiantSource,
    'same-side assignment must preserve each private source ordinal');
  assert.strictEqual(OB64.animationArt.currentEdit(
    giantRouteRom.art.animations, alternateGiantSource.key,
    giantEditedChild).indices[0], giantEditedIndices[0],
  'the runtime Giant route must receive the selected private sprite pixels');
  assert(giantRouteRom.animationSequences.separations[
    giantAdvanceSeparation.id]);
  assert(giantRouteRom.animationSequences.separations[
    giantReturnSeparation.id],
  'same-side idle assignment must preserve fixed movement routes');
  assert.strictEqual(JSON.stringify(
    giantRouteRom.combatAnimationOverrides.desired), giantDesiredBefore,
  'same-side fixed assignment must not create an attack assignment');
  const giantPlan = OB64.animationSequences.buildPlan(giantRouteRom, z64);
  const alternateGiantGroup = giantPlan.groups.find(group =>
    group.id === '77:3');
  assert(alternateGiantGroup,
    'Giant flags 1/1 must build a private descriptor group');
  assert.deepStrictEqual(alternateGiantGroup.separations.map(row => row.laneKey)
    .sort(), ['advance', 'idle', 'return']);
  const alternateGiantPose = alternateGiantGroup.controls.find(row =>
    /-pose$/.test(row.name));
  const builtGiantIdle = OB64.animationArt.parsePoseProgram(
    alternateGiantPose.decoded, 0, 'private Giant flags 1/1 idle');
  assert.strictEqual(builtGiantIdle.records.filter(record =>
    record.opcode === 0x01).length,
  alternateGiantIdle.syntheticAnimation.frames.length,
  'the exported Giant idle must contain only its editable custom frame commands');
  assert.strictEqual(builtGiantIdle.records[
    builtGiantIdle.records.length - 1].operands[0], 0,
  'the exported Giant idle must enter its custom loop immediately');
  assert.strictEqual(builtGiantIdle.frames[
    builtGiantIdle.frames.length - 1][1], giantIdleTick,
  'the runtime Giant route must receive the selected private idle timing');
  [0x01, 0x02, 0x03, 0x04].forEach(selector => {
    const builtGiantFallback = OB64.animationArt.parsePoseProgram(
      alternateGiantPose.decoded, selector,
      `private Giant flags 1/1 fallback ${selector}`);
    assert.deepStrictEqual(builtGiantFallback.frames, builtGiantIdle.frames,
    `the exported Giant fallback ${selector} must enter private idle immediately`);
    assert.deepStrictEqual(builtGiantFallback.records.map(record => ({
      opcode: record.opcode,
      operands: record.operands,
    })), builtGiantIdle.records.map(record => ({
      opcode: record.opcode,
      operands: record.operands,
    })), `the exported Giant fallback ${selector} must use the private idle program`);
  });
  const builtGiantSelector8 = OB64.animationArt.parsePoseProgram(
    alternateGiantPose.decoded, 0x08, 'private Giant flags 1/1 selector 8');
  assert.deepStrictEqual(builtGiantSelector8.frames, builtGiantIdle.frames,
    'the persuaded stop route must enter private idle immediately');
  assert.deepStrictEqual(builtGiantSelector8.records.map(record => ({
    opcode: record.opcode,
    operands: record.operands,
  })), builtGiantIdle.records.map(record => ({
    opcode: record.opcode,
    operands: record.operands,
  })), 'the persuaded stop route must use the private idle program');
  const originalGiantHitRecovery = OB64.animationArt.parsePoseProgram(
    giantEnemyAlternate.pose, 0x12,
    'original Giant flags 1/1 Get Hit recovery');
  const builtGiantHitRecovery = OB64.animationArt.parsePoseProgram(
    alternateGiantPose.decoded, 0x12,
    'private Giant flags 1/1 Get Hit recovery');
  assert.deepStrictEqual(builtGiantHitRecovery.frames.map(frame => frame[0]),
    originalGiantHitRecovery.frames.map(() => builtGiantIdle.frames[0][0]),
  'Get Hit recovery must use the first private idle frame');
  assert.deepStrictEqual(builtGiantHitRecovery.frames.map(frame => frame[1]),
    originalGiantHitRecovery.frames.map(frame => frame[1]),
  'Get Hit recovery must preserve its native timing');
  assert.deepStrictEqual(builtGiantHitRecovery.records.map(record =>
    record.opcode), originalGiantHitRecovery.records.map(record =>
    record.opcode),
  'Get Hit recovery must remain finite');

  const legacyGiantPayload = OB64.animationSequences.collectProject(
    giantRouteRom);
  const legacyGiantProgram = giantEnemyBase.poseProgram.program.slice();
  const legacyGiantLoop = giantEnemyBase.poseProgram.records[
    giantEnemyBase.poseProgram.records.length - 1];
  const legacyGiantFrames = giantEnemyBase.poseProgram.records
    .slice(legacyGiantLoop.operands[0])
    .filter(record => record.opcode === 0x01);
  legacyGiantFrames.forEach((record, frameIndex) => {
    const relative = record.offset - giantEnemyBase.poseProgram.start;
    const frame = alternateGiantIdle.syntheticAnimation.frames[frameIndex];
    legacyGiantProgram[relative + 1] = frame.token;
    legacyGiantProgram[relative + 2] = frame.ticks;
  });
  legacyGiantPayload.entries[alternateGiantIdle.id].poseProgramBase64 =
    Buffer.from(legacyGiantProgram).toString('base64');
  const legacyGiantRom = await freshRom(z64);
  const preparedLegacyGiant = OB64.animationSequences.prepareProject(
    legacyGiantRom, legacyGiantPayload);
  OB64.animationSequences.applyProject(legacyGiantRom, preparedLegacyGiant);
  const restoredLegacyGiant = legacyGiantRom.animationSequences.separations[
    alternateGiantIdle.id].syntheticAnimation;
  assert.strictEqual(restoredLegacyGiant.poseProgram.records.filter(record =>
    record.opcode === 0x01).length, restoredLegacyGiant.frames.length,
  'Project reload must remove hidden native startup frames from old idle data');
  assert.strictEqual(restoredLegacyGiant.poseProgram.records[
    restoredLegacyGiant.poseProgram.records.length - 1].operands[0], 0,
  'Project reload must make an old private idle sequence enter its custom loop');

  const idleDonorRom = await freshRom(z64);
  const idleAttackDonor = OB64.animationUI.idleAnimationRows(
    idleDonorRom.art.animations, 0x02).find(animation =>
      OB64.animationUI.selectorFlags(animation) === '0/0');
  const idleAttackTarget = route(idleDonorRom, 0x02, 0x04, '0/0', 0);
  const idleAttackPair = OB64.combatAnimationOverrides.vanillaPairForLiveAction(
    0x02, idleDonorRom.classDefs[0x03], 0x04);
  assert(idleAttackDonor && idleAttackTarget && idleAttackPair);
  const idleAttackCopy = OB64.animationSequences.separateAndAssign(
    idleDonorRom, idleAttackDonor, idleAttackPair, idleAttackTarget);
  assert.strictEqual(idleAttackCopy.laneKey, 'normal');
  assert.strictEqual(idleAttackCopy.syntheticAnimation.spec.idleSequence, false);
  assert.deepStrictEqual(idleAttackCopy.syntheticAnimation.poseProgram.frames,
    idleAttackDonor.frames.map(frame => [frame.token, frame.ticks]),
  'an idle donor must become a complete one-shot sequence for an attack target');
  assert.notStrictEqual(idleAttackCopy.syntheticAnimation.poseProgram.records[
    idleAttackCopy.syntheticAnimation.poseProgram.records.length - 1].opcode, 0x04,
  'an attack copy must not retain the idle-loop jump');
  const movementAttackDonor = OB64.animationUI.classMotionAnimationRows(
    idleDonorRom.art.animations, 0x02, 'advance').find(animation =>
      OB64.animationUI.selectorFlags(animation) === '1/0');
  const movementAttackTarget = route(
    idleDonorRom, 0x02, 0x04, '1/0', 0);
  const movementAttackPair =
    OB64.combatAnimationOverrides.vanillaPairForLiveAction(
      0x02, idleDonorRom.classDefs[0x03], 0x04);
  assert(movementAttackDonor && movementAttackTarget && movementAttackPair);
  const movementAttackCopy = OB64.animationSequences.separateAndAssign(
    idleDonorRom, movementAttackDonor, movementAttackPair,
    movementAttackTarget);
  assert.strictEqual(movementAttackCopy.laneKey, 'normal');
  assert.strictEqual(
    !!movementAttackCopy.syntheticAnimation.spec.classMotionKind, false,
  'a movement donor must become an editable attack sequence after copying');
  assert.deepStrictEqual(
    movementAttackCopy.syntheticAnimation.poseProgram.frames,
    movementAttackDonor.frames.map(frame => [frame.token, frame.ticks]),
  'a movement donor must copy its complete frame program into the attack target');
  const idleAttackPayload = OB64.animationSequences.collectProject(idleDonorRom);
  const idleAttackRestored = await freshRom(z64);
  const idleAttackPrepared = OB64.animationSequences.prepareProject(
    idleAttackRestored, idleAttackPayload);
  assert.strictEqual(OB64.animationSequences.applyProject(
    idleAttackRestored, idleAttackPrepared), 2);
  assert.deepStrictEqual(Array.from(idleAttackRestored.animationSequences
    .separations[idleAttackCopy.id].syntheticAnimation.poseProgram.program),
  Array.from(idleAttackCopy.syntheticAnimation.poseProgram.program),
  'an idle-donor attack copy must preserve its body program after Project reload');
  assert.deepStrictEqual(Array.from(idleAttackRestored.animationSequences
    .separations[movementAttackCopy.id].syntheticAnimation.poseProgram.program),
  Array.from(movementAttackCopy.syntheticAnimation.poseProgram.program),
  'a movement-donor attack copy must preserve its body program after Project reload');

  const legacyStructurePayload = OB64.animationSequences.collectProject(idleRom);
  legacyStructurePayload.schemaVersion = 3;
  Object.values(legacyStructurePayload.entries).forEach(entry => {
    delete entry.poseProgramBase64;
  });
  const legacyStructureRom = await freshRom(z64);
  const preparedLegacyStructure = OB64.animationSequences.prepareProject(
    legacyStructureRom, legacyStructurePayload);
  assert.strictEqual(OB64.animationSequences.applyProject(
    legacyStructureRom, preparedLegacyStructure), 1,
  'schema 3 private sequences must remain readable after the body-program upgrade');

  const privateIdleAnimation = idleSeparation.syntheticAnimation;
  const idleTemplateLayer = privateIdleAnimation.frames[0].layers.find(layer =>
    privateIdleAnimation.artByKey[layer.sourceKey].editable);
  const addedIdleIndex = OB64.animationSequences.addBlankFrame(
    idleRom, idleSeparation, 0, idleTemplateLayer.ordinal);
  const addedIdleFrame = privateIdleAnimation.frames[addedIdleIndex];
  OB64.animationSequences.moveFrame(idleRom, idleSeparation,
    addedIdleIndex, privateIdleAnimation.frames.length - 1);
  assert.strictEqual(privateIdleAnimation.frames[
    privateIdleAnimation.frames.length - 1], addedIdleFrame,
  'an added idle frame must be reorderable within the visible loop');
  const changedIdleRecords = privateIdleAnimation.poseProgram.records;
  const changedIdleLoop = changedIdleRecords[changedIdleRecords.length - 1];
  assert.strictEqual(changedIdleRecords[changedIdleLoop.operands[0]].opcode, 0x01);
  assert.strictEqual(changedIdleRecords.slice(changedIdleLoop.operands[0])
    .filter(record => record.opcode === 0x01).length, idleFrameCount + 1,
  'idle insertion and reorder must keep every visible frame inside the loop');
  const changedIdleFrameCount = privateIdleAnimation.frames.length;

  const idlePayload = OB64.animationSequences.collectProject(idleRom);
  assert.deepStrictEqual(idlePayload.routeBaselines, {});
  assert.strictEqual(idlePayload.entries[idleSeparation.id].laneKey, 'idle');
  const restoredIdleRom = await freshRom(z64);
  const preparedIdle = OB64.animationSequences.prepareProject(
    restoredIdleRom, idlePayload);
  assert.strictEqual(OB64.animationSequences.applyProject(
    restoredIdleRom, preparedIdle), 1);
  const restoredIdle = restoredIdleRom.animationSequences.separations[
    idleSeparation.id];
  assert(restoredIdle && restoredIdle.syntheticAnimation.spec.idleSequence,
    'Project reload must restore the private idle route');
  assert.strictEqual(restoredIdle.syntheticAnimation.frames.length,
    changedIdleFrameCount);
  assert.strictEqual(JSON.stringify(
    restoredIdleRom.combatAnimationOverrides.desired), idleDesiredBefore,
  'Project reload of an idle loop must not create an attack assignment');
  OB64.animationSequences.removeSeparation(restoredIdleRom, restoredIdle);
  assert.strictEqual(Object.keys(
    restoredIdleRom.animationSequences.separations).length, 0);

  const fixedMovementRom = await freshRom(z64);
  const fighterMovementRows = OB64.animationUI.classMotionAnimationRows(
    fixedMovementRom.art.animations, 0x02);
  const fighterAdvance = fighterMovementRows.find(animation =>
    animation.spec.classMotionKind === 'advance' &&
    OB64.animationUI.selectorFlags(animation) === '0/0');
  const fighterReturn = fighterMovementRows.find(animation =>
    animation.spec.classMotionKind === 'return' &&
    OB64.animationUI.selectorFlags(animation) === '0/0');
  const fighterHit = OB64.animationUI.fixedClassActionAnimationRows(
    fixedMovementRom.art.animations, 0x02, 'hit').find(animation =>
      OB64.animationUI.selectorFlags(animation) === '0/0');
  assert(fighterAdvance && fighterReturn && fighterHit,
    'Fighter must expose movement and Get Hit routes');
  const movementDesiredBefore = JSON.stringify(
    fixedMovementRom.combatAnimationOverrides.desired);
  const originalMovementPose = OB64.art.readCompressedResource(
    z64, fighterAdvance.spec.poseKey).decoded;
  const originalMovementStateCount = OB64.art.readU32(
    originalMovementPose, 0) / 4;
  const originalAdvanceProgram = OB64.animationArt.parsePoseProgram(
    originalMovementPose, 0x05, 'original Fighter advance');
  const originalReturnProgram = OB64.animationArt.parsePoseProgram(
    originalMovementPose, 0x0B, 'original Fighter return');
  const originalHitProgram = OB64.animationArt.parsePoseProgram(
    originalMovementPose, 0x11, 'original Fighter Get Hit');
  const originalNeighborProgram = OB64.animationArt.parsePoseProgram(
    originalMovementPose, 0x04, 'original Fighter selector 4');
  const advanceSeparation = OB64.animationSequences.separateAndAssign(
    fixedMovementRom, fighterAdvance, null, fighterAdvance);
  const returnSeparation = OB64.animationSequences.separateAndAssign(
    fixedMovementRom, fighterReturn, null, fighterReturn);
  const hitSeparation = OB64.animationSequences.separateAndAssign(
    fixedMovementRom, fighterHit, null, fighterHit);
  assert.strictEqual(advanceSeparation.id, '2:-2:0:advance');
  assert.strictEqual(returnSeparation.id, '2:-3:0:return');
  assert.strictEqual(hitSeparation.id, '2:-4:0:hit');
  assert.strictEqual(advanceSeparation.selector, 0x05);
  assert.strictEqual(returnSeparation.selector, 0x0B);
  assert.strictEqual(hitSeparation.selector, 0x11);
  assert.strictEqual(
    advanceSeparation.syntheticAnimation.spec.classMotionKind, 'advance');
  assert.strictEqual(
    returnSeparation.syntheticAnimation.spec.classMotionKind, 'return');
  assert.strictEqual(
    hitSeparation.syntheticAnimation.spec.fixedSequenceKind, 'hit');
  assert.strictEqual(
    hitSeparation.syntheticAnimation.spec.classMotionKind, null);
  assert.strictEqual(JSON.stringify(
    fixedMovementRom.combatAnimationOverrides.desired), movementDesiredBefore,
  'movement detachment must not add or change a Class Combat assignment');
  assert.deepStrictEqual(fixedMovementRom.animationSequences.routeBaselines, {},
    'movement detachment must not create a combat selector baseline');
  assert.deepStrictEqual(OB64.animationSequences.separationConsumers(
    fixedMovementRom, advanceSeparation), [],
  'a fixed movement route must not treat attack users of selector 0x05 as owners');
  assert.strictEqual(OB64.animationSequences.routeSeparationFor(
    fighterAdvance, fixedMovementRom.animationSequences), advanceSeparation);
  assert.strictEqual(OB64.animationSequences.routeAnimation(
    fixedMovementRom, 0x02, -3, 0, 'return'),
  returnSeparation.syntheticAnimation);
  const movementCatalog = OB64.animationUI.animationSequenceCatalogRows(
    fixedMovementRom.art.animations, fixedMovementRom.animationSequences,
    0x02, 0, { classMotionKind: 'advance', flags: '0/0' });
  assert(movementCatalog.includes(fighterAdvance));
  assert(movementCatalog.includes(advanceSeparation.syntheticAnimation),
    'movement selection must retain the ROM route beside its private copy');
  const movementChoices = OB64.animationUI.animationClassVariantChoices(
    movementCatalog);
  assert.strictEqual(movementChoices.length, 2,
    'movement selection must list the ROM route and private copy separately');
  const privateMovementChoice = movementChoices.find(choice =>
    choice.sequenceKind === 'modified');
  assert(privateMovementChoice &&
    privateMovementChoice.representative ===
      advanceSeparation.syntheticAnimation,
  'the private movement route must remain directly selectable');
  assert.strictEqual(OB64.animationUI.currentSequenceChoiceForTarget(
    movementChoices, advanceSeparation.syntheticAnimation),
  privateMovementChoice,
  'the movement dropdown must identify the private route as current');
  const movementPreviewUi = {
    animationKey: fighterAdvance.key,
    animationTargetClassId: fighterAdvance.spec.classId,
    animationTargetActionId: fighterAdvance.spec.actionId,
    animationTargetFlags: '0/0',
    animationTargetLaneKey: 'advance',
  };
  assert.strictEqual(OB64.animationUI.selectedAnimation(
    fixedMovementRom.art, movementPreviewUi,
    OB64.animationUI.effectiveAnimationCatalog(
      fixedMovementRom.art, fixedMovementRom), fixedMovementRom),
  fighterAdvance,
  'the ROM movement route must remain previewable during the current tab visit');
  movementPreviewUi.animationRestoreAssignedRoute = true;
  assert.strictEqual(OB64.animationUI.selectedAnimation(
    fixedMovementRom.art, movementPreviewUi,
    OB64.animationUI.effectiveAnimationCatalog(
      fixedMovementRom.art, fixedMovementRom), fixedMovementRom),
  advanceSeparation.syntheticAnimation,
  'returning to the animation tab must restore the assigned private movement route');

  const advanceAnimation = advanceSeparation.syntheticAnimation;
  const importWidth = advanceAnimation.canvas.width;
  const importHeight = advanceAnimation.canvas.height;
  const movementRgba = new Uint8ClampedArray(
    importWidth * importHeight * 4);
  movementRgba[0] = 248;
  movementRgba[1] = 80;
  movementRgba[2] = 40;
  movementRgba[3] = 255;
  const preparedMovementFrame = OB64.art.prepareAnimationFrameImport(
    movementRgba, importWidth, importHeight, importWidth, importHeight,
    { resizeMode: 'nearest', dither: false });
  OB64.animationSequences.importFrame(
    fixedMovementRom, advanceSeparation, 0, preparedMovementFrame,
    { keepEquipment: false });
  const advanceTicks = advanceAnimation.frames[0].ticks === 255
    ? 254 : advanceAnimation.frames[0].ticks + 1;
  OB64.animationSequences.setFrameTicks(
    fixedMovementRom, advanceSeparation, 0, advanceTicks);
  const returnAnimation = returnSeparation.syntheticAnimation;
  const returnTicks = returnAnimation.frames[0].ticks === 255
    ? 254 : returnAnimation.frames[0].ticks + 1;
  OB64.animationSequences.setFrameTicks(
    fixedMovementRom, returnSeparation, 0, returnTicks);
  const hitAnimation = hitSeparation.syntheticAnimation;
  const hitTicks = hitAnimation.frames[0].ticks === 255
    ? 254 : hitAnimation.frames[0].ticks + 1;
  OB64.animationSequences.setFrameTicks(
    fixedMovementRom, hitSeparation, 0, hitTicks);
  assert.strictEqual(advanceAnimation.frames[0].ticks, advanceTicks);
  assert.strictEqual(returnAnimation.frames[0].ticks, returnTicks);
  assert.strictEqual(hitAnimation.frames[0].ticks, hitTicks);

  const movementPlan = OB64.animationSequences.buildPlan(
    fixedMovementRom, z64);
  assert(movementPlan && movementPlan.groups.length === 1);
  const movementPoseRow = movementPlan.groups[0].controls.find(row =>
    /-pose$/.test(row.name));
  assert(movementPoseRow, 'movement export must build a private pose resource');
  const builtMovementPose = movementPoseRow.decoded;
  assert.strictEqual(OB64.art.readU32(builtMovementPose, 0) / 4,
    originalMovementStateCount,
  'fixed movement detachment must not append pose selectors');
  const builtAdvanceProgram = OB64.animationArt.parsePoseProgram(
    builtMovementPose, 0x05, 'private Fighter advance');
  const builtReturnProgram = OB64.animationArt.parsePoseProgram(
    builtMovementPose, 0x0B, 'private Fighter return');
  const builtHitProgram = OB64.animationArt.parsePoseProgram(
    builtMovementPose, 0x11, 'private Fighter Get Hit');
  const builtNeighborProgram = OB64.animationArt.parsePoseProgram(
    builtMovementPose, 0x04, 'private Fighter selector 4');
  assert.strictEqual(builtAdvanceProgram.frames[0][1], advanceTicks);
  assert.strictEqual(builtReturnProgram.frames[0][1], returnTicks);
  assert.strictEqual(builtHitProgram.frames[0][1], hitTicks);
  assert.notDeepStrictEqual([...builtAdvanceProgram.program],
    [...originalAdvanceProgram.program],
  'movement export must replace the detached selector 0x05 program');
  assert.notDeepStrictEqual([...builtReturnProgram.program],
    [...originalReturnProgram.program],
  'movement export must replace the detached selector 0x0B program');
  assert.notDeepStrictEqual([...builtHitProgram.program],
    [...originalHitProgram.program],
  'Get Hit export must replace the detached selector 0x11 program');
  assert.deepStrictEqual([...builtNeighborProgram.program],
    [...originalNeighborProgram.program],
  'movement export must preserve an adjacent fixed pose program');

  const movementPayload = OB64.animationSequences.collectProject(
    fixedMovementRom);
  assert.deepStrictEqual(movementPayload.routeBaselines, {});
  assert.strictEqual(movementPayload.entries[advanceSeparation.id].laneKey,
    'advance');
  assert.strictEqual(movementPayload.entries[returnSeparation.id].laneKey,
    'return');
  assert.strictEqual(movementPayload.entries[hitSeparation.id].laneKey,
    'hit');
  const restoredMovementRom = await freshRom(z64);
  const preparedMovementProject = OB64.animationSequences.prepareProject(
    restoredMovementRom, movementPayload);
  assert.strictEqual(OB64.animationSequences.applyProject(
    restoredMovementRom, preparedMovementProject), 3);
  const restoredAdvance = restoredMovementRom.animationSequences.separations[
    advanceSeparation.id];
  const restoredReturn = restoredMovementRom.animationSequences.separations[
    returnSeparation.id];
  const restoredHit = restoredMovementRom.animationSequences.separations[
    hitSeparation.id];
  assert(restoredAdvance && restoredReturn && restoredHit);
  assert.strictEqual(restoredAdvance.selector, 0x05);
  assert.strictEqual(restoredReturn.selector, 0x0B);
  assert.strictEqual(restoredHit.selector, 0x11);
  assert.strictEqual(restoredAdvance.syntheticAnimation.frames[0].ticks,
    advanceTicks);
  assert.strictEqual(restoredReturn.syntheticAnimation.frames[0].ticks,
    returnTicks);
  assert.strictEqual(restoredHit.syntheticAnimation.frames[0].ticks,
    hitTicks);
  assert.strictEqual(JSON.stringify(
    restoredMovementRom.combatAnimationOverrides.desired), movementDesiredBefore,
  'Project reload of movement routes must not create attack assignments');
  OB64.animationSequences.removeSeparation(
    restoredMovementRom, restoredAdvance);
  OB64.animationSequences.removeSeparation(
    restoredMovementRom, restoredReturn);
  OB64.animationSequences.removeSeparation(
    restoredMovementRom, restoredHit);
  assert.strictEqual(Object.keys(
    restoredMovementRom.animationSequences.separations).length, 0);
  assert.strictEqual(JSON.stringify(
    restoredMovementRom.combatAnimationOverrides.desired), movementDesiredBefore);

  const movementConsumerRom = await freshRom(z64);
  const movementConsumerAdvance = OB64.animationUI.classMotionAnimationRows(
    movementConsumerRom.art.animations, 0x02, 'advance').find(animation =>
      OB64.animationUI.selectorFlags(animation) === '0/0');
  const movementConsumerSeparation =
    OB64.animationSequences.separateAndAssign(
      movementConsumerRom, movementConsumerAdvance, null,
      movementConsumerAdvance);
  const movementConsumerPair =
    OB64.combatAnimationOverrides.vanillaPairForLiveAction(
      0x02, movementConsumerRom.classDefs[0x03], 0x04);
  OB64.combatAnimationOverrides.setEntry(
    movementConsumerRom.combatAnimationOverrides, {
      classId: 0x02,
      actionId: 0x04,
      bodyFlags: 0,
      normalSelector: 0x05,
      blockedSelector: movementConsumerPair.blockedSelector
    });
  const movementConsumerRoute = route(
    movementConsumerRom, 0x02, 0x04, '0/0', 0);
  assert(movementConsumerRoute && movementConsumerRoute.separationId ===
    movementConsumerSeparation.id);
  assert.strictEqual(
    OB64.animationUI.isClassMotionAnimation(movementConsumerRoute), false,
  'an attack that uses selector 0x05 must remain labeled as an attack');
  assert.strictEqual(movementConsumerRoute.spec.actionId, 0x04);
  OB64.animationSequences.removeSeparation(
    movementConsumerRom, movementConsumerSeparation);
  assert.strictEqual(OB64.combatAnimationOverrides.exactEntry(
    movementConsumerRom.combatAnimationOverrides, 0x02, 0x04, 0)
    .normalSelector, 0x05,
  'removing movement detachment must preserve an attack selector consumer');

  const loopRom = await freshRom(z64);
  const loopingAttack = loopRom.art.animations.specs.find(animation =>
    animation.frames.length > 1 && animation.poseProgram.records.some(record =>
      record.opcode === 0x04));
  assert(loopingAttack,
    'the retail corpus must retain an attack with a frame-loop command');
  const loopFields = loopingAttack.spec.route.actorFields;
  const loopPair = OB64.combatAnimationOverrides.vanillaPairForLiveAction(
    loopingAttack.spec.classId,
    loopRom.classDefs[loopFields.physicalClassRecord],
    loopingAttack.spec.actionId);
  const loopSeparation = OB64.animationSequences.separateAndAssign(
    loopRom, loopingAttack, loopPair, loopingAttack);
  const loopAnimation = loopSeparation.syntheticAnimation;
  const originalLoopRecord = loopAnimation.poseProgram.records.find(record =>
    record.opcode === 0x04);
  const loopTargetFrameIndex = loopAnimation.poseProgram.records
    .filter(record => record.opcode === 0x01)
    .findIndex(record => record.ordinal === originalLoopRecord.operands[0]);
  assert(loopTargetFrameIndex >= 0,
    'the retail attack loop must target a visible frame');
  OB64.animationSequences.removeFrame(
    loopRom, loopSeparation, loopTargetFrameIndex);
  const updatedLoopRecord = loopAnimation.poseProgram.records.find(record =>
    record.opcode === 0x04);
  assert.strictEqual(loopAnimation.poseProgram.records[
    updatedLoopRecord.operands[0]].opcode, 0x01,
  'removing a loop target must retarget the jump to a surviving frame');
  assert.notStrictEqual(updatedLoopRecord.operands[0], updatedLoopRecord.ordinal,
    'removing a final loop frame must not turn the jump into a self-loop');

  const crossActionRom = await freshRom(z64);
  const crossDefinition = crossActionRom.classDefs[0x03];
  crossDefinition.b43Raw = crossDefinition.b45Raw =
    crossDefinition.b47Raw = 0x06;
  const crossDonor = crossActionRom.art.animations.specs.find(animation =>
    animation.spec.classId === 0x02 && animation.spec.actionId === 0x04 &&
    OB64.animationUI.selectorFlags(animation) === '0/0' &&
    animation.spec.rawMode === 0);
  const crossBlockedDonor = crossActionRom.art.animations.specs.find(animation =>
    animation.spec.classId === 0x02 && animation.spec.actionId === 0x04 &&
    OB64.animationUI.selectorFlags(animation) === '0/0' &&
    animation.spec.rawMode === 2);
  OB64.combatAnimationOverrides.setEntry(
    crossActionRom.combatAnimationOverrides, {
      classId: 0x02, actionId: 0x06,
      normalSelector: crossDonor.spec.selector,
      blockedSelector: crossBlockedDonor.spec.selector,
    });
  const crossTargetNormal = route(crossActionRom, 0x02, 0x06, '0/0', 0);
  const crossTargetBlocked = route(crossActionRom, 0x02, 0x06, '0/0', 2);
  assert(crossTargetNormal && crossTargetBlocked && crossDonor,
    'cross-action assignment requires live Rend and canonical Slash routes');
  const crossPair = {
    normalSelector: crossTargetNormal.spec.selector,
    blockedSelector: crossTargetBlocked.spec.selector,
  };
  OB64.animationSequences.assignShared(crossActionRom, crossDonor, crossPair,
    crossTargetNormal);
  const crossExact = OB64.combatAnimationOverrides.exactEntry(
    crossActionRom.combatAnimationOverrides, 0x02, 0x06, 0);
  assert(crossExact, 'shared donor assignment must target the selected action');
  assert.strictEqual(crossExact.normalSelector, crossDonor.spec.selector);
  assert.strictEqual(OB64.combatAnimationOverrides.exactEntry(
    crossActionRom.combatAnimationOverrides, 0x02, 0x04, 0), null,
  'cross-action assignment must not rewrite the donor action');
  assert.strictEqual(OB64.animationSequences.sharedAssignmentIssue(
    crossDonor, crossTargetBlocked), '',
  'the same art route may reuse a Normal source sequence for Blocked mode');
  OB64.animationSequences.assignShared(crossActionRom, crossDonor, crossPair,
    crossTargetBlocked);
  assert.strictEqual(OB64.combatAnimationOverrides.exactEntry(
    crossActionRom.combatAnimationOverrides, 0x02, 0x06, 0).blockedSelector,
  crossDonor.spec.selector,
  'the selected Blocked target must accept the Normal source selector');
  const otherArtRoute = crossActionRom.art.animations.specs.find(animation =>
    animation.spec.classId === 0x02 && animation.spec.actionId === 0x04 &&
    OB64.animationUI.selectorFlags(animation) === '0/1' &&
    animation.spec.rawMode === 0);
  assert(OB64.animationSequences.sharedAssignmentIssue(
    otherArtRoute, crossTargetBlocked).includes('sprite resource and body appearance'),
  'direct assignment must not misrepresent a different art route as the same sequence');
  const warriorBase = route(crossActionRom, 0x55, 0x04, '0/0', 0);
  const warriorAlternateBlocked = route(crossActionRom, 0x55, 0x04, '1/0', 2);
  assert(warriorBase && warriorAlternateBlocked,
    'Warrior player-side Base and Alternate routes must resolve');
  assert.notStrictEqual(OB64.animationUI.selectorFlags(warriorBase),
    OB64.animationUI.selectorFlags(warriorAlternateBlocked));
  assert.strictEqual(warriorBase.spec.descriptorKey,
    warriorAlternateBlocked.spec.descriptorKey,
    'the linked Warrior routes must use one sprite resource');
  assert.strictEqual(warriorBase.spec.selectedBodyChild,
    warriorAlternateBlocked.spec.selectedBodyChild,
    'the linked Warrior routes must use one body appearance');
  assert.strictEqual(OB64.animationSequences.sharedAssignmentIssue(
    warriorAlternateBlocked, warriorBase), '',
  'linked Base and Alternate labels must remain directly assignable');
  const warriorActionRom = await freshRom(z64);
  const warriorDefinition = warriorActionRom.classDefs[0x56];
  const warriorActionChange =
    OB64.combatAnimationOverrides.applyLiveAttackChange(
    warriorActionRom.combatAnimationOverrides, warriorDefinition,
    0x55, 'b45Raw', 0x2E);
  assert.strictEqual(warriorActionChange.requiresAnimationSelection, true,
    'a new unmapped action must wait for the user to choose its body animation');
  assert.strictEqual(warriorActionRom.combatAnimationOverrides.desired.length, 0,
    'changing an attack action must not silently create an animation override');
  let warriorMagicTarget = route(
    warriorActionRom, 0x55, 0x2E, '0/0', 0);
  assert(warriorMagicTarget,
    'the newly assigned Warrior Tier 2 action must expose its game fallback');
  assert.strictEqual(warriorMagicTarget.spec.selector, 0x28);
  assert.strictEqual(warriorMagicTarget.effectiveMapping.source, 'fallback');
  assert.strictEqual(warriorMagicTarget.effectiveMapping.assignmentRequired, true);
  const warriorChoices = OB64.animationUI.animationClassVariantChoices(
    OB64.animationUI.animationSequenceCatalogRows(
      warriorActionRom.art.animations, warriorActionRom.animationSequences,
      0x55, 0));
  const warriorBaseChoices = warriorChoices.filter(choice =>
    choice.flags === '0/0');
  const soldierChoices = OB64.animationUI.animationClassVariantChoices(
    OB64.animationUI.animationSequenceCatalogRows(
      warriorActionRom.art.animations, warriorActionRom.animationSequences,
      0x01, 0, { flags: '0/0' }));
  const soldierNativePrograms = soldierChoices.filter(choice =>
    choice.representative.spec.nativeSelectorCandidate);
  assert(soldierNativePrograms.length > 0);
  soldierNativePrograms.forEach(choice => {
    assert.strictEqual(choice.laneKey, 'source');
    assert.strictEqual(choice.laneLabel, '');
    assert(!/Normal Attack|Attack Blocked/.test(choice.label));
    assert(!soldierChoices.some(other =>
      !other.representative.spec.nativeSelectorCandidate &&
      OB64.animationUI.sameAnimationSequence(
        other.representative, choice.representative)),
    'a mapped source must replace its redundant native-program label');
  });
  for (const selector of [0x28, 0x29, 0x2E]) {
    assert(warriorBaseChoices.some(choice =>
      choice.representative.spec.selector === selector),
    'every distinct structurally valid Warrior native body program must remain selectable');
  }
  const soldierNative2A = soldierNativePrograms.find(choice =>
    choice.representative.spec.selector === 0x2A);
  assert(soldierNative2A);
  const soldierNative2ANormal = soldierNative2A.rows.find(row =>
    row.spec.rawMode === 0);
  const soldierNative2ABlocked = soldierNative2A.rows.find(row =>
    row.spec.rawMode === 2);
  assert(soldierNative2ANormal && soldierNative2ABlocked);
  assert.strictEqual(OB64.animationUI.currentSequenceChoiceForTarget(
    soldierChoices, soldierNative2ANormal), soldierNative2A);
  assert.strictEqual(OB64.animationUI.currentSequenceChoiceForTarget(
    soldierChoices, soldierNative2ABlocked), soldierNative2A,
  'one lane-neutral native label must serve normal and blocked targets');
  assert.strictEqual(OB64.animationUI.sequenceAssignmentSummary(
    soldierNative2A, soldierNative2ANormal, false),
  'Assigned to Normal Attack · ' + soldierNative2A.label);
  assert.strictEqual(OB64.animationUI.sequenceAssignmentSummary(
    soldierNative2A, soldierNative2ABlocked, false),
  'Assigned to Attack Blocked · ' + soldierNative2A.label,
  'the target lane must remain separate from the native source label');
  const soldierPreviewChoice = soldierChoices.find(choice =>
    choice.key !== soldierNative2A.key);
  assert(soldierPreviewChoice);
  assert.strictEqual(OB64.animationUI.sequenceAssignmentSummary(
    soldierNative2A, soldierNative2ANormal, false, soldierPreviewChoice),
  'Previewing · ' + soldierPreviewChoice.label +
    ' · Normal Attack assignment unchanged',
  'previewing a choice must change the summary without changing its target');
  const warriorPose28 = OB64.animationUI.animationPoseOffsetSummary(
    warriorBaseChoices.find(choice =>
      choice.representative.spec.selector === 0x28).representative);
  const warriorPose2A = OB64.animationUI.animationPoseOffsetSummary(
    warriorChoices.find(choice => choice.flags === '0/0' &&
      choice.representative.spec.selector === 0x2A).representative);
  const warriorPose2E = OB64.animationUI.animationPoseOffsetSummary(
    warriorBaseChoices.find(choice =>
      choice.representative.spec.selector === 0x2E).representative);
  assert.strictEqual(warriorPose28.label, 'Pose Offsets Return');
  assert.deepStrictEqual(warriorPose28.peak, [0, 0, 7]);
  assert.strictEqual(warriorPose2A.label, 'Pose Offsets Return');
  assert.deepStrictEqual(warriorPose2A.peak, [0, 0, 3]);
  assert.strictEqual(warriorPose2E.label, 'No Pose Offset');
  assert.deepStrictEqual(warriorPose2E.peak, [0, 0, 0]);
  assert(warriorPose28.title.includes(
    'do not prove a visible art shift or battlefield movement'));
  const signedPoseOffsets = OB64.animationUI.animationPoseOffsetSummary({
    poseProgram: { records: [
      { opcode: 0x0C, operands: [0x01, 0x02, 0x03] },
      { opcode: 0x0C, operands: [0xFF, 0xFE, 0xFD] },
    ] },
  });
  assert.deepStrictEqual(signedPoseOffsets.peak, [1, 2, 3]);
  assert.deepStrictEqual(signedPoseOffsets.end, [0, 0, 0]);
  assert.strictEqual(signedPoseOffsets.label, 'Pose Offsets Return');
  warriorChoices.forEach(choice => {
    assert(!/Vanilla|Player Side|Enemy Side|Warrior \(Dio\) Art|Art Variant/.test(
      choice.label));
    assert(choice.label.includes(choice.poseOffsets.label));
    assert(!/Advance|Stationary/.test(choice.label));
  });
  const warriorLinkedChoice = warriorChoices.find(choice => choice.linkedToKey);
  assert(warriorLinkedChoice && warriorLinkedChoice.linkedTitle);
  assert(!warriorLinkedChoice.label.includes('Linked'));
  assert.strictEqual(OB64.animationUI.currentSequenceChoiceForTarget(
    warriorChoices, warriorMagicTarget), null,
  'an unmapped fallback must not be presented as a user-selected sequence');
  const warriorMagicDonor = route(
    warriorActionRom, 0x55, 0x2D, '0/0', 0);
  assert(warriorMagicDonor,
    'the Warrior Tier 1 magic sequence must remain available as a donor');
  OB64.animationSequences.assignShared(warriorActionRom, warriorMagicDonor, {
    normalSelector: 0x28,
    blockedSelector: 0x28,
  }, warriorMagicTarget);
  warriorMagicTarget = route(
    warriorActionRom, 0x55, 0x2E, '0/0', 0);
  const warriorCurrentChoice =
    OB64.animationUI.currentSequenceChoiceForTarget(
      warriorChoices, warriorMagicTarget);
  assert(warriorCurrentChoice,
    'the user-selected Warrior Tier 2 body sequence must become current');
  assert.strictEqual(warriorCurrentChoice.sourceActionId, 0x2D,
    'the selected Tier 1 magic donor must label the Tier 2 body assignment');
  const warriorExact = OB64.combatAnimationOverrides.exactEntry(
    warriorActionRom.combatAnimationOverrides, 0x55, 0x2E, 0);
  assert.strictEqual(warriorExact.normalSelector, 0x2A);
  assert.strictEqual(warriorExact.blockedSelector, 0x28);
  assert.strictEqual(OB64.animationUI.animationActionFamily(
    warriorCurrentChoice.sourceActionId),
  OB64.animationUI.animationActionFamily(warriorMagicTarget.spec.actionId),
  'the presented current source must use the target action command family');
  const warriorSelector28 = warriorBaseChoices.find(choice =>
    choice.representative.spec.selector === 0x28);
  OB64.animationSequences.assignShared(warriorActionRom,
    warriorSelector28.representative, warriorExact, warriorMagicTarget);
  warriorMagicTarget = route(
    warriorActionRom, 0x55, 0x2E, '0/0', 0);
  const warriorNativeCurrent = OB64.animationUI.currentSequenceChoiceForTarget(
    warriorChoices, warriorMagicTarget);
  assert(warriorNativeCurrent &&
    warriorNativeCurrent.representative.spec.selector === 0x28,
  'a distinct source program must remain assignable after another program was selected');
  assert.strictEqual(OB64.combatAnimationOverrides.exactEntry(
    warriorActionRom.combatAnimationOverrides, 0x55, 0x2E, 0).normalSelector,
  0x28);
  const crossSeparation = OB64.animationSequences.separateAndAssign(
    crossActionRom, crossDonor, crossPair, crossTargetNormal);
  assert.strictEqual(crossSeparation.actionId, 0x06);
  assert.strictEqual(crossSeparation.targetRef.actionId, 0x06);
  assert.strictEqual(crossSeparation.donorRef.actionId, 0x04);
  assert.strictEqual(crossSeparation.syntheticAnimation.spec.actionId, 0x06);
  assert.strictEqual(crossSeparation.syntheticAnimation.spec.actionName, 'Rend');
  assert.strictEqual(crossSeparation.syntheticAnimation.frames.length,
    crossDonor.frames.length);
  const crossPayload = OB64.animationSequences.collectProject(crossActionRom);
  const crossRestored = await freshRom(z64);
  const crossPrepared = OB64.animationSequences.prepareProject(
    crossRestored, crossPayload);
  assert.strictEqual(OB64.animationSequences.applyProject(
    crossRestored, crossPrepared), 1);
  const restoredCrossSeparation = crossRestored.animationSequences.separations[
    crossSeparation.id];
  assert(restoredCrossSeparation);
  assert.strictEqual(restoredCrossSeparation.targetRef.actionId, 0x06);
  assert.strictEqual(restoredCrossSeparation.donorRef.actionId, 0x04);
  assert.strictEqual(restoredCrossSeparation.syntheticAnimation.spec.actionId,
    0x06);

  crossDefinition.b43Raw = 0x04;
  const crossSlashTarget = route(crossActionRom, 0x02, 0x04, '0/0', 0);
  const crossSlashPair = OB64.combatAnimationOverrides.vanillaPairForLiveAction(
    0x02, crossDefinition, 0x04);
  assert(crossSlashTarget && crossSlashPair,
    'the modified-sequence reuse check requires a live Slash target');
  OB64.animationSequences.assignShared(crossActionRom,
    crossSeparation.syntheticAnimation, crossSlashPair, crossSlashTarget);
  const reusedModified = route(crossActionRom, 0x02, 0x04, '0/0', 0);
  assert(reusedModified && reusedModified.separationId === crossSeparation.id,
    'an action assigned to a modified selector must resolve that project sequence');
  assert.strictEqual(reusedModified.spec.actionId, 0x04,
    'a reused modified sequence must retain the consumer action identity');
  assert.strictEqual(OB64.animationUI.animationSequenceStorageIdentity(
    reusedModified), OB64.animationUI.animationSequenceStorageIdentity(
      crossSeparation.syntheticAnimation),
  'every consumer of one private sequence must retain its storage identity');
  const alteredPrivateProgram = Object.assign({},
    crossSeparation.syntheticAnimation, {
      poseProgram: Object.assign({},
        crossSeparation.syntheticAnimation.poseProgram, {
          program: crossSeparation.syntheticAnimation.poseProgram.program.slice()
        })
    });
  alteredPrivateProgram.poseProgram.program[0] =
    (alteredPrivateProgram.poseProgram.program[0] + 1) & 0xFF;
  assert.strictEqual(OB64.animationUI.animationSequenceStorageIdentity(
    alteredPrivateProgram), OB64.animationUI.animationSequenceStorageIdentity(
      crossSeparation.syntheticAnimation));
  assert.notStrictEqual(OB64.animationUI.animationSequenceIdentity(
    alteredPrivateProgram), OB64.animationUI.animationSequenceIdentity(
      crossSeparation.syntheticAnimation),
  'program-byte changes must change sequence identity within one storage owner');
  assert.strictEqual(OB64.animationSequences.separationConsumers(
    crossActionRom, crossSeparation).length, 1);
  assert.throws(() => OB64.animationSequences.removeSeparation(
    crossActionRom, crossSeparation), /still assigned/,
  'a modified sequence must not disappear while another target uses it');
  const modifiedDonorSource = Object.values(
    crossSeparation.syntheticAnimation.artByKey).find(source => source.editable);
  const modifiedDonorChild = modifiedDonorSource.editableChildOrdinals[0];
  const modifiedDonorPixels = OB64.animationArt.currentEdit(
    crossActionRom.art.animations, modifiedDonorSource.key, modifiedDonorChild);
  const modifiedIndices = modifiedDonorPixels.indices.slice();
  const modifiedIntensity = modifiedDonorPixels.intensity.slice();
  modifiedIndices[0] = (modifiedIndices[0] + 1) & 0xFF;
  OB64.animationArt.setEdit(crossActionRom.art.animations,
    modifiedDonorSource.key, modifiedDonorChild,
    modifiedIndices, modifiedIntensity);
  const crossSlashBlockedTarget = route(
    crossActionRom, 0x02, 0x04, '0/0', 2);
  assert(crossSlashBlockedTarget,
    'selector-reindex testing requires the Slash blocked target');
  OB64.animationSequences.separateAndAssign(
    crossActionRom, crossDonor, crossSlashPair, crossSlashBlockedTarget);
  const refreshedModifiedDonorSource = Object.values(
    crossSeparation.syntheticAnimation.artByKey).find(source =>
    source.separationSourceOrdinal === modifiedDonorSource.separationSourceOrdinal);
  assert.strictEqual(OB64.animationArt.currentEdit(
    crossActionRom.art.animations, refreshedModifiedDonorSource.key,
    modifiedDonorChild).indices[0], modifiedIndices[0],
  'adding another modified sequence must preserve existing staged pixels');
  assert.strictEqual(route(crossActionRom, 0x02, 0x04, '0/0', 0).separationId,
    crossSeparation.id,
  'selector reindexing must preserve actions that share a modified sequence');
  assert.strictEqual(OB64.animationSequences.separationConsumers(
    crossActionRom, crossSeparation).length, 1);
  OB64.animationSequences.assignShared(
    crossActionRom, crossDonor, crossSlashPair, crossSlashTarget);
  assert.strictEqual(OB64.animationSequences.separationConsumers(
    crossActionRom, crossSeparation).length, 0);
  const enemyCopyTarget = route(crossActionRom, 0x02, 0x04, '0/1', 0);
  assert(enemyCopyTarget, 'modified-sequence copying requires an enemy-side target');
  const copiedModified = OB64.animationSequences.separateAndAssign(
    crossActionRom, crossSeparation.syntheticAnimation,
    crossSlashPair, enemyCopyTarget);
  assert.deepStrictEqual(
    [...copiedModified.syntheticAnimation.poseProgram.program],
    [...crossSeparation.syntheticAnimation.poseProgram.program],
  'an independent private copy can begin with equal body-program bytes');
  assert.notStrictEqual(OB64.animationUI.animationSequenceStorageIdentity(
    copiedModified.syntheticAnimation),
  OB64.animationUI.animationSequenceStorageIdentity(
    crossSeparation.syntheticAnimation),
  'independent private copies must retain different storage identities');
  assert.notStrictEqual(OB64.animationUI.animationSequenceIdentity(
    copiedModified.syntheticAnimation), OB64.animationUI.animationSequenceIdentity(
      crossSeparation.syntheticAnimation),
  'equal body-program bytes must not link independent private copies');
  const copiedModifiedSource = Object.values(copiedModified.syntheticAnimation.artByKey)
    .find(source => source.separationSourceOrdinal ===
      refreshedModifiedDonorSource.separationSourceOrdinal);
  assert(copiedModifiedSource, 'the copied modified source ordinal must remain stable');
  assert.strictEqual(OB64.animationArt.currentEdit(
    crossActionRom.art.animations, copiedModifiedSource.key,
    modifiedDonorChild).indices[0], modifiedIndices[0],
  'Copy From and Separate must preserve staged pixels from a modified donor');
  const copiedModifiedPayload = OB64.animationSequences.collectProject(
    crossActionRom).entries[copiedModified.id];
  assert.strictEqual(copiedModifiedPayload.donorRef.selector,
    crossDonor.spec.selector,
  'a modified donor copy must retain a vanilla-resolvable structural reference');
  assert.strictEqual(atob(copiedModifiedPayload.sources[
    copiedModifiedSource.separationSourceOrdinal].children[
      modifiedDonorChild].ci8IndicesBase64).charCodeAt(0), modifiedIndices[0],
  'Project serialization must retain pixels copied from a modified donor');

  OB64.animationSequences.assignShared(rom, fighter, pair);
  let exact = OB64.combatAnimationOverrides.exactEntry(
    rom.combatAnimationOverrides, 0x02, 0x04, 0);
  assert(exact);
  assert.strictEqual(exact.normalSelector, fighter.spec.selector);
  let compiled = OB64.combatAnimationOverrides.compileTable(
    rom.combatAnimationOverrides.desired);
  const exactRecord = compiled.records.find(record =>
    record.action === 0x04 && record.flags === 2 && record.bodyFlags === 0);
  assert(exactRecord, 'shared assignment must compile as one exact body-route record');
  assert.strictEqual(OB64.combatAnimationOverrides.lookupRecords(
    compiled.records, exactRecord.sourceArt, exactRecord.rawOwner,
    exactRecord.action, 0x7F, 0, 0), fighter.spec.selector);
  assert.strictEqual(OB64.combatAnimationOverrides.lookupRecords(
    compiled.records, exactRecord.sourceArt, exactRecord.rawOwner,
    exactRecord.action, 0x7F, 0, 1), 0x7F,
  'an exact route assignment must not affect another body-flag route');

  const separation = OB64.animationSequences.separateAndAssign(
    rom, fighter, pair);
  const revisionAfterSeparation = rom.animationSequences.revision;
  assert(revisionAfterSeparation > 0,
    'sequence creation must invalidate the effective animation catalog');
  assert.strictEqual(separation.classId, 0x02);
  assert.strictEqual(separation.actionId, 0x04);
  assert.strictEqual(separation.bodyFlags, 0);
  assert.strictEqual(separation.laneKey, 'normal');
  assert(separation.selector >= fighter.poseProgram.stateCount);
  const synthetic = separation.syntheticAnimation;
  assert.strictEqual(synthetic.frames.length, fighter.frames.length);
  assert.strictEqual(synthetic.spec.classId, fighter.spec.classId);
  assert.strictEqual(synthetic.spec.actionId, fighter.spec.actionId);
  Object.values(synthetic.artByKey).forEach(source => {
    assert.strictEqual(source.resource.storedLength, 0,
      'sequence cloning must defer compression until ROM export');
    assert.strictEqual(source.resource.stored.length, 0,
      'sequence cloning must not retain a duplicate compressed payload');
  });
  const sequenceCatalog = OB64.animationUI.animationSequenceCatalogRows(
    rom.art.animations, rom.animationSequences, fighter.spec.classId, 0);
  assert(sequenceCatalog.includes(synthetic),
    'a separated project sequence must enter its class and side catalog');
  assert(!OB64.animationUI.animationSequenceCatalogRows(
    rom.art.animations, rom.animationSequences, fighter.spec.classId, 1)
    .includes(synthetic),
  'a modified player-side sequence must not appear on the enemy side');
  const sequenceChoices = OB64.animationUI.animationClassVariantChoices(
    sequenceCatalog);
  assert(sequenceChoices.some(choice => choice.sequenceKind === 'modified' &&
    choice.representative === synthetic &&
    !/Modified|Vanilla/.test(choice.label)),
  'the sequence dropdown must distinguish the modified project sequence');
  assert(sequenceChoices.some(choice => choice.sequenceKind === 'vanilla'),
    'the modified sequence must not replace the fixed vanilla catalog');
  const modifiedPreviewUi = {
    animationKey: synthetic.key,
    animationTargetClassId: fighter.spec.classId,
    animationTargetActionId: fighter.spec.actionId,
    animationTargetFlags: '0/0',
    animationTargetLaneKey: 'normal',
  };
  assert.strictEqual(OB64.animationUI.selectedAnimation(
    rom.art, modifiedPreviewUi,
    OB64.animationUI.effectiveAnimationCatalog(rom.art, rom)), synthetic,
  'a modified catalog entry must remain directly previewable');
  Object.values(synthetic.artByKey).forEach(source => {
    if (source.sourceRole === 'body') assert.strictEqual(source.sprite.childCount, 1);
    if (source.weaponSelectable) {
      assert(source.sprite.childCount > 0);
      assert.strictEqual(source.selectableChildOrdinals.length,
        source.sprite.childCount);
    }
  });
  exact = OB64.combatAnimationOverrides.exactEntry(
    rom.combatAnimationOverrides, 0x02, 0x04, 0);
  assert.strictEqual(exact.normalSelector, separation.selector);

  const separatedRoute = route(rom, 0x02, 0x04, '0/0', 0);
  assert(separatedRoute && separatedRoute.separationId === separation.id);
  assert.strictEqual(separatedRoute.effectiveMapping.source, 'separated');
  assert.strictEqual(route(rom, 0x02, 0x04, '0/0', 1).separationId,
    separation.id, 'normal raw modes 0 and 1 must share the separated copy');

  const editableSource = Object.values(synthetic.artByKey).find(source =>
    source.editable);
  assert(editableSource);
  const child = editableSource.editableChildOrdinals[0];
  const original = OB64.animationArt.currentEdit(
    rom.art.animations, editableSource.key, child);
  const indices = original.indices.slice();
  const intensity = original.intensity.slice();
  indices[0] = (indices[0] + 1) & 0xFF;
  OB64.animationArt.setEdit(
    rom.art.animations, editableSource.key, child, indices, intensity);
  assert.strictEqual(OB64.animationArt.currentEdit(
    rom.art.animations, editableSource.key, child).indices[0], indices[0]);

  const wizard = rom.art.animations.specs.find(animation =>
    animation.spec.classId === 0x15 && animation.spec.rawMode === 0 &&
    animation.frames.length > 0);
  assert(wizard, 'Wizard source sequence must exist');
  OB64.animationSequences.copyFrom(rom, separation, wizard);
  assert(rom.animationSequences.revision > revisionAfterSeparation,
    'donor replacement must invalidate the effective animation catalog');
  assert.strictEqual(separation.syntheticAnimation.frames.length,
    wizard.frames.length);
  assert.strictEqual(separation.syntheticAnimation.spec.classId, 0x02,
    'Copy From must retain the target class route');
  assert.strictEqual(separation.syntheticAnimation.spec.actionId, 0x04,
    'Copy From must retain the target attack route');
  Object.values(separation.syntheticAnimation.artByKey).forEach(source => {
    assert.strictEqual(source.resource.storedLength, 0,
      'Replace From must defer cloned-source compression until ROM export');
  });
  const copiedWeapons = Object.values(separation.syntheticAnimation.artByKey)
    .filter(source => source.weaponSelectable);
  const wizardWeapons = Object.values(wizard.artByKey)
    .filter(source => source.weaponSelectable);
  assert.deepStrictEqual(copiedWeapons.map(source => source.sprite.childCount),
    wizardWeapons.map(source => source.sprite.childCount),
  'Copy From must retain every physical weapon child from its donor');

  const privateAnimation = separation.syntheticAnimation;
  const bodyLayerUses = [];
  privateAnimation.frames.forEach(frame => frame.layers.forEach(layer => {
    const source = privateAnimation.artByKey[layer.sourceKey];
    if (source && source.editable && !source.weaponSelectable) {
      bodyLayerUses.push({ frame, layer, source });
    }
  }));
  const rotatedBodyUse = bodyLayerUses[0];
  assert(rotatedBodyUse,
    'the private donor must expose an editable body layer');
  const untouchedBodyUse = bodyLayerUses.find(use =>
    use.frame !== rotatedBodyUse.frame);
  assert(untouchedBodyUse,
    'the private donor must expose editable body layers in two frames');
  const sharedBodySource = rotatedBodyUse.source;
  Object.assign(untouchedBodyUse.layer, {
    artId: rotatedBodyUse.layer.artId,
    sourceKey: sharedBodySource.key,
    bindingId: sharedBodySource.bindingId,
    physicalSourceId: sharedBodySource.physicalSourceId,
    sourceRole: sharedBodySource.sourceRole,
    resourceKey: sharedBodySource.resourceKey,
    childCount: sharedBodySource.sprite.childCount,
    requestedChildOrdinal: 0,
    selectedChildOrdinal: 0,
    width: sharedBodySource.sprite.width,
    height: sharedBodySource.sprite.height,
  });
  const oldBodyKey = sharedBodySource.key;
  const oldBodyWidth = sharedBodySource.sprite.width;
  const oldBodyHeight = sharedBodySource.sprite.height;
  const oldBodyX = rotatedBodyUse.layer.drawOffsetX;
  const oldBodyY = rotatedBodyUse.layer.drawOffsetY;
  const oldBodyPixels = OB64.animationArt.currentEdit(
    rom.art.animations, oldBodyKey, 0);
  const expectedBodyRotation = OB64.animationSequences.rotateIndexedPixels(
    oldBodyPixels.indices, oldBodyPixels.intensity,
    oldBodyWidth, oldBodyHeight, 17);
  const rotatedBodyOrdinal = OB64.animationSequences.rotateLayer(
    rom, separation, rotatedBodyUse.frame.sequenceIndex,
    rotatedBodyUse.layer.ordinal, 17);
  const rotatedBodyLayer = rotatedBodyUse.frame.layers[rotatedBodyOrdinal];
  const rotatedBodySource = privateAnimation.artByKey[rotatedBodyLayer.sourceKey];
  assert.notStrictEqual(rotatedBodySource.key, oldBodyKey,
    'a layer transform must create a private copy-on-write source');
  assert.strictEqual(untouchedBodyUse.frame.layers[
    untouchedBodyUse.layer.ordinal].sourceKey, oldBodyKey,
  'another frame using the original sprite must remain unchanged');
  assert(privateAnimation.artByKey[oldBodyKey],
    'the original source must remain while another frame still uses it');
  assert.strictEqual(rotatedBodyLayer.width, expectedBodyRotation.width);
  assert.strictEqual(rotatedBodyLayer.height, expectedBodyRotation.height);
  assert.strictEqual(rotatedBodyLayer.drawOffsetX,
    oldBodyX + Math.floor((oldBodyWidth - expectedBodyRotation.width) / 2));
  assert.strictEqual(rotatedBodyLayer.drawOffsetY,
    oldBodyY + Math.floor((oldBodyHeight - expectedBodyRotation.height) / 2));
  assert.deepStrictEqual(OB64.animationArt.currentEdit(
    rom.art.animations, rotatedBodySource.key, 0).indices,
  expectedBodyRotation.indices,
  'a structural rotation must preserve the exact rotated CI8 pixels');
  assert.deepStrictEqual(OB64.animationArt.currentEdit(
    rom.art.animations, rotatedBodySource.key, 0).intensity,
  expectedBodyRotation.intensity,
  'a structural rotation must preserve the exact rotated I4 pixels');

  const resizedBodyWidth = rotatedBodySource.sprite.width + 1;
  const resizedBodyHeight = rotatedBodySource.sprite.height + 2;
  const expectedBodyResize = OB64.animationSequences.resizeIndexedPixels(
    expectedBodyRotation.indices, expectedBodyRotation.intensity,
    rotatedBodySource.sprite.width, rotatedBodySource.sprite.height,
    resizedBodyWidth, resizedBodyHeight);
  OB64.animationSequences.resizeLayer(
    rom, separation, rotatedBodyUse.frame.sequenceIndex,
    rotatedBodyOrdinal, resizedBodyWidth, resizedBodyHeight);
  const resizedBodyLayer = rotatedBodyUse.frame.layers[rotatedBodyOrdinal];
  const resizedBodySource = privateAnimation.artByKey[resizedBodyLayer.sourceKey];
  assert.notStrictEqual(resizedBodySource.key, rotatedBodySource.key,
    'resizing must replace only the selected private layer source');
  assert.deepStrictEqual(OB64.animationArt.currentEdit(
    rom.art.animations, resizedBodySource.key, 0).indices,
  expectedBodyResize.indices,
  'a structural resize must use exact nearest-neighbor CI8 pixels');
  assert.strictEqual(privateAnimation.artByKey[rotatedBodySource.key], undefined,
    'an otherwise-unused intermediate transform source must be pruned');

  const weaponTransformSource = Object.values(privateAnimation.artByKey)
    .find(source => source.editable && source.weaponSelectable &&
      source.sprite.childCount > 1);
  assert(weaponTransformSource,
    'the private donor must include a multi-child equipment sprite');
  let weaponTransformUse = null;
  privateAnimation.frames.some(frame => frame.layers.some(layer => {
    if (layer.sourceKey !== weaponTransformSource.key) return false;
    weaponTransformUse = { frame, layer }; return true;
  }));
  assert(weaponTransformUse);
  const weaponChildrenBefore = weaponTransformSource.sprite.children.map(child => ({
    pixels: OB64.animationArt.currentEdit(
      rom.art.animations, weaponTransformSource.key, child.ordinal),
    palette: OB64.animationArt.childPalette(
      weaponTransformSource, child.ordinal).slice(),
  }));
  const weaponWidthBefore = weaponTransformSource.sprite.width;
  const weaponHeightBefore = weaponTransformSource.sprite.height;
  const rotatedWeaponOrdinal = OB64.animationSequences.rotateLayer(
    rom, separation, weaponTransformUse.frame.sequenceIndex,
    weaponTransformUse.layer.ordinal, 'left');
  const rotatedWeaponLayer =
    weaponTransformUse.frame.layers[rotatedWeaponOrdinal];
  const rotatedWeaponSource =
    privateAnimation.artByKey[rotatedWeaponLayer.sourceKey];
  assert.strictEqual(rotatedWeaponSource.sprite.childCount,
    weaponTransformSource.sprite.childCount,
  'weapon transforms must retain every equipped-item appearance');
  weaponChildrenBefore.forEach((child, childOrdinal) => {
    const expected = OB64.animationSequences.rotateIndexedPixels(
      child.pixels.indices, child.pixels.intensity,
      weaponWidthBefore, weaponHeightBefore, 'left');
    const observed = OB64.animationArt.currentEdit(
      rom.art.animations, rotatedWeaponSource.key, childOrdinal);
    assert.deepStrictEqual(observed.indices, expected.indices,
      'weapon child ' + childOrdinal + ' must rotate with its pose');
    assert.deepStrictEqual(observed.intensity, expected.intensity);
    assert.deepStrictEqual(OB64.animationArt.childPalette(
      rotatedWeaponSource, childOrdinal), child.palette,
    'weapon child ' + childOrdinal + ' must retain its palette');
  });
  const targetFrame = privateAnimation.frames[0];
  const blankTemplateLayer = targetFrame.layers.find(layer =>
    privateAnimation.artByKey[layer.sourceKey].editable);
  assert(blankTemplateLayer,
    'a private frame must expose an editable palette for a blank layer');
  const blankTemplateSource = privateAnimation.artByKey[
    blankTemplateLayer.sourceKey];
  const blankTemplatePalette = OB64.animationArt.childPalette(
    blankTemplateSource, blankTemplateLayer.selectedChildOrdinal).slice();
  const libraryLayersBefore = targetFrame.layers.slice();
  const libraryPixelsBefore = libraryLayersBefore.map(layer => {
    const source = privateAnimation.artByKey[layer.sourceKey];
    const child = OB64.animationArt.childOrdinalOrFallback(
      source, layer.selectedChildOrdinal);
    const pixels = OB64.animationArt.currentEdit(
      rom.art.animations, source.key, child);
    return {
      indices: pixels.indices.slice(),
      intensity: pixels.intensity.slice(),
    };
  });
  const libraryLayerOrdinal = OB64.animationUI.importLibrarySpriteLayer(
    rom, separation, privateAnimation, targetFrame, blankTemplateLayer,
    blankTemplateLayer.selectedChildOrdinal, {
      name: 'Regression Library Sprite',
      width: 2,
      height: 1,
      rgba: new Uint8ClampedArray([
        255, 255, 255, 255,
        0, 0, 0, 0,
      ]),
    });
  assert.strictEqual(targetFrame.layers.length, libraryLayersBefore.length + 1,
    'a Sprite Library import must append exactly one frame layer');
  libraryLayersBefore.forEach((layer, index) => {
    assert.strictEqual(targetFrame.layers[index], layer,
      'a Sprite Library import must retain existing layer ' + index);
    const source = privateAnimation.artByKey[layer.sourceKey];
    const child = OB64.animationArt.childOrdinalOrFallback(
      source, layer.selectedChildOrdinal);
    const pixels = OB64.animationArt.currentEdit(
      rom.art.animations, source.key, child);
    assert.deepStrictEqual(pixels.indices, libraryPixelsBefore[index].indices,
      'a Sprite Library import must retain existing layer indexes');
    assert.deepStrictEqual(pixels.intensity, libraryPixelsBefore[index].intensity,
      'a Sprite Library import must retain existing layer intensity');
  });
  assert.strictEqual(libraryLayerOrdinal, targetFrame.layers.length - 1,
    'the imported Sprite Library layer must be appended and selected');
  const libraryLayer = targetFrame.layers[libraryLayerOrdinal];
  const libraryLayerSource = privateAnimation.artByKey[libraryLayer.sourceKey];
  assert.notStrictEqual(libraryLayerSource.key, blankTemplateSource.key,
    'the imported layer must own an independent sprite source');
  assert.strictEqual(libraryLayerSource.sourceRole, 'body');
  assert.strictEqual(libraryLayerSource.sprite.width,
    blankTemplateSource.sprite.width);
  assert.strictEqual(libraryLayerSource.sprite.height,
    blankTemplateSource.sprite.height);
  assert.deepStrictEqual(libraryLayerSource.palette, blankTemplatePalette,
    'the imported layer must use the selected layer palette');
  assert.strictEqual(libraryLayer.drawOffsetX, blankTemplateLayer.drawOffsetX);
  assert.strictEqual(libraryLayer.drawOffsetY, blankTemplateLayer.drawOffsetY);
  assert.strictEqual(libraryLayer.flags, blankTemplateLayer.flags);
  assert.strictEqual(libraryLayer.scaleXRaw, blankTemplateLayer.scaleXRaw);
  assert.strictEqual(libraryLayer.scaleYRaw, blankTemplateLayer.scaleYRaw);
  const libraryLayerPixels = OB64.animationArt.currentEdit(
    rom.art.animations, libraryLayerSource.key, 0);
  assert.strictEqual(libraryLayerPixels.intensity[0], 15,
    'opaque imported pixels must retain full I4 intensity');
  assert.strictEqual(
    libraryLayerPixels.intensity[libraryLayerPixels.intensity.length - 1], 0,
    'transparent imported pixels must remain transparent');
  const libraryLayerSourceOrdinal = libraryLayerSource.separationSourceOrdinal;
  const libraryLayerFrameIdentity = targetFrame.sourceFrameIndex;
  const libraryLayerExpected = {
    indices: libraryLayerPixels.indices.slice(),
    intensity: libraryLayerPixels.intensity.slice(),
  };
  const blankLayerOrdinal = OB64.animationSequences.addBlankLayer(
    rom, separation, targetFrame.sequenceIndex, blankTemplateLayer.ordinal);
  const blankLayer = targetFrame.layers[blankLayerOrdinal];
  const blankLayerSource = privateAnimation.artByKey[blankLayer.sourceKey];
  assert.strictEqual(blankLayer.width, privateAnimation.canvas.width);
  assert.strictEqual(blankLayer.height, privateAnimation.canvas.height);
  assert.strictEqual(blankLayer.drawOffsetX, privateAnimation.canvas.originX);
  assert.strictEqual(blankLayer.drawOffsetY, privateAnimation.canvas.originY);
  assert.deepStrictEqual(blankLayerSource.palette, blankTemplatePalette,
    'a blank layer must copy the selected layer palette');
  assert(OB64.animationArt.currentEdit(
    rom.art.animations, blankLayerSource.key, 0).intensity.every(value => !value),
  'a blank layer must start fully transparent');
  const blankLayerSourceOrdinal = blankLayerSource.separationSourceOrdinal;
  const addedDonorFrame = fighter.frames[0];
  const addedDonorLayer = addedDonorFrame.layers[0];
  const originalLayerCount = targetFrame.layers.length;
  const addedOrdinal = OB64.animationSequences.addLayerFrom(
    rom, separation, targetFrame.sequenceIndex, fighter,
    addedDonorFrame.sequenceIndex, addedDonorLayer.ordinal);
  assert.strictEqual(targetFrame.layers.length, originalLayerCount + 1,
    'a private frame must accept a copied layer');
  assert.strictEqual(addedOrdinal, targetFrame.layers.length - 1);
  const addedLayer = targetFrame.layers[addedOrdinal];
  assert(privateAnimation.artByKey[addedLayer.sourceKey].separationId === separation.id,
    'an added layer must own a private cloned sprite source');
  const movedOrdinal = OB64.animationSequences.moveLayer(
    rom, separation, targetFrame.sequenceIndex, addedOrdinal, 0);
  assert.strictEqual(movedOrdinal, 0);
  assert.strictEqual(targetFrame.layers[0], addedLayer,
    'layer reordering must change the serialized draw order');
  assert.strictEqual(OB64.animationSequences.setLayerPosition(
    rom, separation, targetFrame.sequenceIndex, 0, -123, 45), true);
  assert.strictEqual(targetFrame.layers[0].drawOffsetX, -123);
  assert.strictEqual(targetFrame.layers[0].drawOffsetY, 45);
  const updatedFrameTicks = targetFrame.ticks === 255
    ? targetFrame.ticks - 1 : targetFrame.ticks + 1;
  assert.strictEqual(OB64.animationSequences.setFrameTicks(
    rom, separation, targetFrame.sequenceIndex, updatedFrameTicks), true);
  assert.strictEqual(targetFrame.ticks, updatedFrameTicks);
  assert.strictEqual(privateAnimation.poseProgram.frames[
    targetFrame.sequenceIndex][1], updatedFrameTicks,
  'frame tick editing must rewrite the matching body-program command');
  assert.deepStrictEqual(privateAnimation.spec.frames,
    privateAnimation.frames.map(frame => [frame.token, frame.ticks]));
  assert.strictEqual(OB64.animationSequences.setFrameTicks(
    rom, separation, targetFrame.sequenceIndex, updatedFrameTicks), false);
  assert.throws(() => OB64.animationSequences.setFrameTicks(
    rom, separation, targetFrame.sequenceIndex, 256), /frame ticks/);
  OB64.animationSequences.copyLayerFrom(
    rom, separation, targetFrame.sequenceIndex, 0, fighter,
    fighter.frames[1].sequenceIndex, fighter.frames[1].layers[0].ordinal);
  assert.strictEqual(targetFrame.layers[0].drawOffsetX, -123,
    'sprite-only Copy From must preserve the target layer X position');
  assert.strictEqual(targetFrame.layers[0].drawOffsetY, 45,
    'sprite-only Copy From must preserve the target layer Y position');
  const copiedFrameTarget = privateAnimation.frames[1];
  const copiedFrameToken = copiedFrameTarget.token;
  const copiedFrameTicks = copiedFrameTarget.ticks;
  OB64.animationSequences.copyFrameFrom(
    rom, separation, copiedFrameTarget.sequenceIndex, fighter,
    fighter.frames[0].sequenceIndex);
  assert.strictEqual(copiedFrameTarget.layers.length, fighter.frames[0].layers.length,
    'frame Copy From must replace the complete layer stack');
  assert.strictEqual(copiedFrameTarget.token, copiedFrameToken,
    'frame Copy From must retain the target body-program token');
  assert.strictEqual(copiedFrameTarget.ticks, copiedFrameTicks,
    'frame Copy From must retain the target body-program timing');

  const cutsceneBodyLayer = fighter.frames[0].layers.find(layer =>
    fighter.artByKey[layer.sourceKey].sourceRole === 'body');
  assert(cutsceneBodyLayer,
    'cutscene complete-frame copying requires one body sprite fixture');
  const cutsceneBodySource = fighter.artByKey[cutsceneBodyLayer.sourceKey];
  const cutsceneSourceKey = 'test-cutscene-native-source';
  const cutsceneSource = Object.assign({}, cutsceneBodySource, {
    key: cutsceneSourceKey,
    bindingId: cutsceneSourceKey,
    physicalSourceId: cutsceneSourceKey,
    childSelectionPolicy: 'cutscene-actor-appearance',
  });
  const cutsceneLayer = Object.assign({}, cutsceneBodyLayer, {
    ordinal: 0,
    sourceKey: cutsceneSourceKey,
    bindingId: cutsceneSourceKey,
    physicalSourceId: cutsceneSourceKey,
    drawOffsetX: 300,
    drawOffsetY: -300,
    flags: 3,
    scaleXRaw: 512,
    scaleYRaw: 2048,
  });
  const cutsceneDonor = {
    key: 'test-cutscene-native-sequence',
    artByKey: { [cutsceneSourceKey]: cutsceneSource },
    frames: [{ sequenceIndex: 0, ticks: 8, layers: [cutsceneLayer] }],
  };
  OB64.animationSequences.copyFrameFrom(
    rom, separation, copiedFrameTarget.sequenceIndex, cutsceneDonor, 0);
  const copiedCutsceneLayer = copiedFrameTarget.layers[0];
  const copiedCutsceneSource = privateAnimation.artByKey[
    copiedCutsceneLayer.sourceKey];
  const copiedCutsceneStableFrame = copiedFrameTarget.sourceFrameIndex;
  const copiedCutsceneSourceOrdinal =
    copiedCutsceneSource.separationSourceOrdinal;
  assert.strictEqual(copiedFrameTarget.layers.length, 1,
    'complete cutscene-frame copying must retain the complete donor layer stack');
  assert.strictEqual(copiedCutsceneSource.childSelectionPolicy,
    'cutscene-actor-appearance',
    'complete cutscene-frame copying must retain native transform provenance');
  assert.strictEqual(copiedCutsceneLayer.drawOffsetX, 300);
  assert.strictEqual(copiedCutsceneLayer.drawOffsetY, -300);
  assert.strictEqual(copiedCutsceneLayer.flags, 3);
  assert.strictEqual(copiedCutsceneLayer.scaleXRaw, 512);
  assert.strictEqual(copiedCutsceneLayer.scaleYRaw, 2048);
  assert.strictEqual(privateAnimation.canvas.originY, -600,
    'a copied cutscene frame must scale its native Y position for preview bounds');
  assert(privateAnimation.canvas.endX >= 150 && privateAnimation.canvas.endX < 300,
    'a copied cutscene frame must scale its native X position for preview bounds');
  const cutsceneProject = OB64.animationSequences.collectProject(rom)
    .entries[separation.id];
  assert(Object.values(cutsceneProject.sources).some(source =>
    source.childSelectionPolicy === 'cutscene-actor-appearance'),
  'Project data must preserve native cutscene transform provenance');

  OB64.animationSequences.copyLayerFrom(
    rom, separation, targetFrame.sequenceIndex, 0, cutsceneDonor, 0, 0);
  const copiedCutsceneSpriteSource = privateAnimation.artByKey[
    targetFrame.layers[0].sourceKey];
  assert.strictEqual(copiedCutsceneSpriteSource.childSelectionPolicy, null,
    'sprite-only copying must keep the target combat-layer transform');

  const importedFrame = privateAnimation.frames[2];
  const equipmentLayersBeforeImport = importedFrame.layers.filter(layer =>
    privateAnimation.artByKey[layer.sourceKey].sourceRole === 'equipment').length;
  const importedRgba = new Uint8ClampedArray([
    255, 0, 0, 255, 0, 255, 0, 255,
    0, 0, 255, 128, 255, 255, 255, 0,
  ]);
  const preparedFrame = OB64.art.prepareAnimationFrameImport(
    importedRgba, 2, 2, privateAnimation.canvas.width,
    privateAnimation.canvas.height, { resizeMode: 'nearest' });
  const importedLayerOrdinal = OB64.animationSequences.importFrame(
    rom, separation, importedFrame.sequenceIndex, preparedFrame,
    { keepEquipment: true });
  assert.strictEqual(importedFrame.layers.length, equipmentLayersBeforeImport + 1,
    'frame import must flatten body art while retaining equipment layers');
  const importedLayer = importedFrame.layers[importedLayerOrdinal];
  const importedSource = privateAnimation.artByKey[importedLayer.sourceKey];
  assert.strictEqual(importedSource.sourceRole, 'body');
  assert.strictEqual(importedSource.sprite.width, privateAnimation.canvas.width);
  assert.strictEqual(importedSource.sprite.height, privateAnimation.canvas.height);
  assert.strictEqual(importedSource.sprite.firstFormat, 1);
  assert.strictEqual(importedSource.sprite.secondFormat, 0);
  assert.strictEqual(importedSource.palette.length, 256);
  assert.deepStrictEqual(
    OB64.animationArt.decodeChild(importedSource.sprite, 0).indices,
    preparedFrame.indices,
    'imported CI8 pixels must round-trip through the native sprite object');
  assert.deepStrictEqual(
    OB64.animationArt.decodeChild(importedSource.sprite, 0).intensity,
    preparedFrame.intensity,
    'imported I4 intensity must round-trip through the native sprite object');
  const importedStableFrame = importedFrame.sourceFrameIndex;
  const importedSourceOrdinal = importedSource.separationSourceOrdinal;

  const removableLayerOrdinal = OB64.animationSequences.addLayerFrom(
    rom, separation, targetFrame.sequenceIndex, fighter,
    addedDonorFrame.sequenceIndex, addedDonorLayer.ordinal);
  const removableLayerKey = targetFrame.layers[removableLayerOrdinal].sourceKey;
  const layerCountBeforeRemoval = targetFrame.layers.length;
  const survivingLayerOrdinal = OB64.animationSequences.removeLayer(
    rom, separation, targetFrame.sequenceIndex, removableLayerOrdinal);
  assert.strictEqual(targetFrame.layers.length, layerCountBeforeRemoval - 1,
    'a private frame must remove the selected sprite layer');
  assert.strictEqual(survivingLayerOrdinal, targetFrame.layers.length - 1);
  assert.deepStrictEqual(targetFrame.layers.map(layer => layer.ordinal),
    targetFrame.layers.map((layer, ordinal) => ordinal),
    'remaining private layers must be renumbered');
  assert.strictEqual(privateAnimation.artByKey[removableLayerKey], undefined,
    'a removed layer must release its otherwise-unused private sprite source');

  const frameCountBeforeRemoval = privateAnimation.frames.length;
  const removedFrameIndex = frameCountBeforeRemoval - 1;
  const removedStableFrame = privateAnimation.frames[removedFrameIndex].sourceFrameIndex;
  const survivingFrameIndex = OB64.animationSequences.removeFrame(
    rom, separation, removedFrameIndex);
  assert.strictEqual(privateAnimation.frames.length, frameCountBeforeRemoval - 1,
    'a private sequence must remove the selected frame');
  assert.strictEqual(survivingFrameIndex, privateAnimation.frames.length - 1);
  assert.strictEqual(privateAnimation.poseProgram.frames.length,
    privateAnimation.frames.length,
    'frame removal must remove the matching body-program frame command');
  assert(!privateAnimation.frames.some(frame =>
    frame.sourceFrameIndex === removedStableFrame));
  assert.deepStrictEqual(privateAnimation.frames.map(frame => frame.sequenceIndex),
    privateAnimation.frames.map((frame, ordinal) => ordinal),
    'remaining private frames must be renumbered');

  const frameCountBeforeAddition = privateAnimation.frames.length;
  const blankFrameTemplate = privateAnimation.frames[0];
  const blankFrameTemplateLayer = blankFrameTemplate.layers.find(layer =>
    privateAnimation.artByKey[layer.sourceKey].editable);
  const blankFrameIndex = OB64.animationSequences.addBlankFrame(
    rom, separation, blankFrameTemplate.sequenceIndex,
    blankFrameTemplateLayer.ordinal);
  const blankFrame = privateAnimation.frames[blankFrameIndex];
  assert.strictEqual(privateAnimation.frames.length, frameCountBeforeAddition + 1,
    'a private sequence must accept a new blank frame');
  assert.strictEqual(blankFrame.ticks, blankFrameTemplate.ticks,
    'a blank frame must copy the selected frame duration');
  assert.strictEqual(blankFrame.layers.length, 1,
    'a blank frame must begin with one full-frame sprite layer');
  const blankFrameSource = privateAnimation.artByKey[
    blankFrame.layers[0].sourceKey];
  assert(OB64.animationArt.currentEdit(
    rom.art.animations, blankFrameSource.key, 0).intensity.every(value => !value),
  'a blank frame must start fully transparent');
  const blankFrameStableIdentity = blankFrame.sourceFrameIndex;
  const blankFrameSourceOrdinal = blankFrameSource.separationSourceOrdinal;
  const frameOrderBeforeMove = privateAnimation.frames.map(frame =>
    frame.sourceFrameIndex);
  const movedFrameIndex = OB64.animationSequences.moveFrame(
    rom, separation, blankFrameIndex, privateAnimation.frames.length - 1);
  assert.strictEqual(movedFrameIndex, privateAnimation.frames.length - 1);
  assert.strictEqual(privateAnimation.frames[movedFrameIndex], blankFrame,
    'frame dragging must change the serialized frame order');
  assert.notDeepStrictEqual(privateAnimation.frames.map(frame =>
    frame.sourceFrameIndex), frameOrderBeforeMove);
  assert.deepStrictEqual(privateAnimation.poseProgram.frames,
    privateAnimation.frames.map(frame => [frame.token, frame.ticks]),
  'frame reordering must rewrite the visible frame commands in body-program order');
  const remainingFrameCount = privateAnimation.frames.length;

  const payload = OB64.animationSequences.collectProject(rom);
  assert.strictEqual(payload.schemaVersion, 4);
  assert(payload.entries[separation.id]);
  assert(payload.entries[separation.id].poseProgramBase64,
    'Project data must store the exact private body program');
  const restored = await freshRom(z64);
  const malformed = JSON.parse(JSON.stringify(payload));
  const malformedEntry = malformed.entries[separation.id];
  const malformedSource = malformedEntry.sources[Object.keys(malformedEntry.sources)[0]];
  const malformedChild = malformedSource.children[Object.keys(malformedSource.children)[0]];
  malformedChild.ci8IndicesBase64 = '';
  const beforeMalformedDesired = JSON.stringify(
    restored.combatAnimationOverrides.desired);
  assert.throws(() => OB64.animationSequences.prepareProject(restored, malformed),
    /expected/);
  assert.strictEqual(Object.keys(restored.animationSequences.separations).length, 0,
    'Project preparation must not create a partial separated sequence');
  assert.strictEqual(JSON.stringify(restored.combatAnimationOverrides.desired),
    beforeMalformedDesired,
  'Project preparation must not create a partial selector assignment');
  const malformedPose = JSON.parse(JSON.stringify(payload));
  malformedPose.entries[separation.id].poseProgramBase64 = btoa('\x00');
  assert.throws(() => OB64.animationSequences.prepareProject(
    restored, malformedPose), /body program/,
  'Project preparation must reject a malformed private body program');
  const controlRecord = privateAnimation.poseProgram.records.find(record =>
    record.opcode !== 0x01 && record.opcode !== 0x04 &&
    record.operands.length > 0);
  assert(controlRecord,
    'the private donor must retain a non-frame control operand for validation');
  const changedControl = JSON.parse(JSON.stringify(payload));
  const changedControlBytes = Buffer.from(
    changedControl.entries[separation.id].poseProgramBase64, 'base64');
  const changedControlOffset = controlRecord.offset -
    privateAnimation.poseProgram.start + 1;
  changedControlBytes[changedControlOffset] ^= 1;
  changedControl.entries[separation.id].poseProgramBase64 =
    changedControlBytes.toString('base64');
  assert.throws(() => OB64.animationSequences.prepareProject(
    restored, changedControl), /non-frame control operand/,
  'Project preparation must reject edits to locked non-frame controls');
  const prepared = OB64.animationSequences.prepareProject(restored, payload);
  assert.strictEqual(OB64.animationSequences.applyProject(restored, prepared), 1);
  const restoredSeparation = restored.animationSequences.separations[separation.id];
  assert(restoredSeparation);
  assert.strictEqual(restoredSeparation.syntheticAnimation.frames.length,
    remainingFrameCount);
  assert.strictEqual(restoredSeparation.syntheticAnimation.poseProgram.frames.length,
    remainingFrameCount,
    'Project reload must preserve the changed private body program');
  assert.deepStrictEqual(
    Array.from(restoredSeparation.syntheticAnimation.poseProgram.program),
    Array.from(privateAnimation.poseProgram.program),
    'Project reload must preserve exact private body-program bytes');
  assert.deepStrictEqual(restoredSeparation.syntheticAnimation.frames.map(frame =>
    frame.sourceFrameIndex), privateAnimation.frames.map(frame =>
    frame.sourceFrameIndex),
  'Project reload must preserve added-frame identity and reordered frame order');
  assert.strictEqual(restoredSeparation.syntheticAnimation.frames[0]
    .layers[0].drawOffsetX, -123,
  'Project reload must preserve private layer positions');
  assert.strictEqual(restoredSeparation.syntheticAnimation.frames[0]
    .layers[0].drawOffsetY, 45,
  'Project reload must preserve private layer positions');
  const restoredCutsceneFrame = restoredSeparation.syntheticAnimation.frames
    .find(frame => frame.sourceFrameIndex === copiedCutsceneStableFrame);
  const restoredCutsceneSource = Object.values(
    restoredSeparation.syntheticAnimation.artByKey).find(source =>
      source.separationSourceOrdinal === copiedCutsceneSourceOrdinal);
  assert(restoredCutsceneFrame && restoredCutsceneSource,
    'Project reload must preserve a copied cutscene frame and source');
  assert.strictEqual(restoredCutsceneFrame.layers.length, 1,
    'Project reload must preserve the complete cutscene layer stack');
  assert.strictEqual(restoredCutsceneSource.childSelectionPolicy,
    'cutscene-actor-appearance',
    'Project reload must preserve native cutscene transform provenance');
  const restoredImportedFrame = restoredSeparation.syntheticAnimation.frames
    .find(frame => frame.sourceFrameIndex === importedStableFrame);
  const restoredImportedSource = Object.values(
    restoredSeparation.syntheticAnimation.artByKey).find(source =>
      source.separationSourceOrdinal === importedSourceOrdinal);
  assert(restoredImportedSource,
    'Project reload must preserve an imported frame sprite source');
  assert(restoredImportedFrame.layers.some(layer =>
    layer.sourceKey === restoredImportedSource.key),
  'Project reload must preserve the imported frame layer');
  assert.deepStrictEqual(restoredImportedSource.palette,
    importedSource.palette,
  'Project reload must preserve the imported frame palette');
  const restoredBlankLayerSource = Object.values(
    restoredSeparation.syntheticAnimation.artByKey).find(source =>
      source.separationSourceOrdinal === blankLayerSourceOrdinal);
  assert(restoredBlankLayerSource,
    'Project reload must preserve a blank layer sprite source');
  assert(OB64.animationArt.currentEdit(
    restored.art.animations, restoredBlankLayerSource.key, 0)
    .intensity.every(value => !value),
    'Project reload must preserve blank layer transparency');
  const restoredLibraryLayerSource = Object.values(
    restoredSeparation.syntheticAnimation.artByKey).find(source =>
      source.separationSourceOrdinal === libraryLayerSourceOrdinal);
  const restoredLibraryLayerFrame = restoredSeparation.syntheticAnimation.frames
    .find(frame => frame.sourceFrameIndex === libraryLayerFrameIdentity);
  assert(restoredLibraryLayerSource && restoredLibraryLayerFrame,
    'Project reload must preserve the imported Sprite Library layer');
  assert(restoredLibraryLayerFrame.layers.some(layer =>
    layer.sourceKey === restoredLibraryLayerSource.key));
  const restoredLibraryLayerPixels = OB64.animationArt.currentEdit(
    restored.art.animations, restoredLibraryLayerSource.key, 0);
  assert.deepStrictEqual(restoredLibraryLayerPixels.indices,
    libraryLayerExpected.indices);
  assert.deepStrictEqual(restoredLibraryLayerPixels.intensity,
    libraryLayerExpected.intensity);
  const restoredBlankFrame = restoredSeparation.syntheticAnimation.frames
    .find(frame => frame.sourceFrameIndex === blankFrameStableIdentity);
  assert(restoredBlankFrame,
    'Project reload must preserve an added frame');
  assert(restoredBlankFrame.layers.some(layer =>
    restoredSeparation.syntheticAnimation.artByKey[layer.sourceKey]
      .separationSourceOrdinal === blankFrameSourceOrdinal),
  'Project reload must preserve the added frame blank source');
  assert(OB64.combatAnimationOverrides.exactEntry(
    restored.combatAnimationOverrides, 0x02, 0x04, 0));

  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, 'patch.js'), 'utf8'), {
    filename: 'patch.js',
  });
  assert.strictEqual(OB64.patch.VERSION, 34);

  OB64.animationSequences.removeSeparation(rom, separation);
  assert.strictEqual(rom.animationSequences.separations[separation.id], undefined);
  exact = OB64.combatAnimationOverrides.exactEntry(
    rom.combatAnimationOverrides, 0x02, 0x04, 0);
  assert(exact, 'removal must restore the pre-separation exact assignment');
  assert.strictEqual(exact.normalSelector, fighter.spec.selector);

  OB64.animationSequences.removeSeparation(restored, restoredSeparation);
  OB64.combatAnimationOverrides.removeEntry(
    restored.combatAnimationOverrides, 0x02, 0x04, 0);
  const cleanNormal = route(restored, 0x02, 0x04, '0/0', 0);
  const cleanBlocked = route(restored, 0x02, 0x04, '0/0', 2);
  const cleanPair = OB64.combatAnimationOverrides.vanillaPairForLiveAction(
    0x02, restored.classDefs[0x03], 0x04);
  const laneSeparation = OB64.animationSequences.separateAndAssign(
    restored, cleanNormal, cleanPair);
  assert.strictEqual(
    restored.animationSequences.routeBaselines['2:4:0'].entry,
    null,
    'a first separation without an earlier exact assignment must retain a null baseline'
  );
  OB64.animationSequences.assignShared(restored, cleanBlocked, cleanPair);
  assert(restored.animationSequences.routeBaselines['2:4:0'].entry,
    'a shared Blocked assignment beside a Normal separation must become the new baseline');
  OB64.animationSequences.removeSeparation(restored, laneSeparation);
  const retainedLane = OB64.combatAnimationOverrides.exactEntry(
    restored.combatAnimationOverrides, 0x02, 0x04, 0);
  assert(retainedLane,
    'removing the Normal separation must retain the later shared Blocked assignment');
  assert.strictEqual(retainedLane.normalSelector, cleanPair.normalSelector);
  assert.strictEqual(retainedLane.blockedSelector, cleanBlocked.spec.selector);

  console.log('PASS stable sequence catalog, assignment, separation, reuse, and Project round-trip');
})().catch(error => {
  console.error(error && error.stack ? error.stack : error);
  process.exitCode = 1;
});
