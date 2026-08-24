'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of ['cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}

const C = OB64.cutsceneCatalog;
const catalog = C.createCatalog(OB64.cutsceneData);
const directorScenes = catalog.scenes.filter(scene => scene.engine === 'director');
const profiledDirectorScenes = directorScenes.filter(scene => scene.source.dynamicGrammar !== true);

assert.strictEqual(OB64.cutsceneData.schemaVersion, 16);
assert.strictEqual(OB64.cutsceneData.directorContinuationStreams.length, 5);
assert.deepStrictEqual(OB64.cutsceneData.directorContinuationStreams.map(row =>
  [row.selector, row.resourceKey, row.decodedLength, row.terminalWithoutTrailer]), [
  [0, 0x018909B6, 260, true],
  [1, 0x01890A14, 144, true],
  [2, 0x01890A52, 96, true],
  [3, 0x01890A8A, 124, true],
  [4, 0x01890ACC, 92, true],
]);
assert.strictEqual(catalog.scenes.length, 1508);
assert.strictEqual(directorScenes.length, 1498);
assert.strictEqual(profiledDirectorScenes.length, 60);
assert.strictEqual(directorScenes.filter(scene => scene.source.dynamicGrammar === true).length, 1438);
assert.strictEqual(OB64.cutsceneData.counts.retailDirectorSelectorRows, 1693);
assert.strictEqual(OB64.cutsceneData.counts.populatedRetailDirectorSelectorRows, 1548);
assert.strictEqual(OB64.cutsceneData.counts.retailDirectorWords, 550019);
assert.strictEqual(OB64.cutsceneData.counts.retailDirectorNodes, 193771);
assert.strictEqual(OB64.cutsceneData.directorGrammar.length, 154);
assert.strictEqual(OB64.cutsceneData.directorEvents.length, 154);
assert.strictEqual(catalog.sceneResourcePaths.length, 134);
assert.strictEqual(OB64.cutsceneData.counts.sceneResourcePathGroups, 59);
assert.strictEqual(OB64.cutsceneData.counts.sceneResourcePathEntries, 134);
assert.strictEqual(OB64.cutsceneData.counts.populatedSceneResourcePaths, 121);
assert.strictEqual(OB64.cutsceneData.counts.animatedSceneSpriteDirectRotationCommands, 2001);
assert.strictEqual(OB64.cutsceneData.counts.animatedSceneSpriteSampledPathRotationCommands, 0);
assert.strictEqual(OB64.cutsceneData.counts.animatedSceneSpriteResourcePathRotationCommands, 0);
const firstSceneResourcePath = catalog.getSceneResourcePath(0, 0);
assert.deepStrictEqual([
  firstSceneResourcePath.pointCount,
  firstSceneResourcePath.firstPoint.x, firstSceneResourcePath.firstPoint.y,
  firstSceneResourcePath.secondPoint.x, firstSceneResourcePath.secondPoint.y,
  firstSceneResourcePath.nativeDenseSampleCount,
  firstSceneResourcePath.nativeStoredHeading,
  firstSceneResourcePath.rotationDegrees,
], [3, 60, -50, 60, 50, 130, 76, 256]);
const curvedSceneResourcePath = catalog.getSceneResourcePath(0, 7);
assert.deepStrictEqual([
  curvedSceneResourcePath.pointCount,
  curvedSceneResourcePath.nativeDenseSampleCount,
  curvedSceneResourcePath.nativeStoredHeading,
  curvedSceneResourcePath.rotationDegrees,
], [9, 202, -49, 131]);
assert.notStrictEqual(curvedSceneResourcePath.nativeStoredHeading,
  Math.trunc(Math.atan2(
    curvedSceneResourcePath.secondPoint.y - curvedSceneResourcePath.firstPoint.y,
    curvedSceneResourcePath.secondPoint.x - curvedSceneResourcePath.firstPoint.x) *
    180 / Math.PI),
'resource-path heading must use the native spline sampler, not the first authored segment');
assert.strictEqual(catalog.getSceneResourcePath(1, 99), null);
assert.strictEqual(catalog.modeTwoStagePlacementProfiles.length, 80);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoStagePlacementProfiles, 80);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoStagePlacementSelectors, 25);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoOrthographicStagePlacementRows, 105);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoPerspectiveStagePlacementRows, 63);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoNormalStagePlacements, 165);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoSpecialStagePlacementRows, 3);
const formationStageProps = catalog.getModeTwoStagePlacementProfile(51);
const graduationStageProps = catalog.getModeTwoStagePlacementProfile(57);
assert.strictEqual(formationStageProps.orthographicPlacements.length, 9);
assert.strictEqual(formationStageProps.perspectivePlacements.length, 1);
assert(formationStageProps.orthographicPlacements.concat(
  formationStageProps.perspectivePlacements).every(placement =>
  placement.source.descriptorKey === 0x00FEA716));
assert.deepStrictEqual(formationStageProps.perspectivePlacements.map(placement =>
  [placement.poseSelector, placement.x, placement.y, placement.z]), [[7, -120, 0, 95]]);
assert.strictEqual(graduationStageProps.orthographicPlacements.length, 11);
assert.strictEqual(graduationStageProps.perspectivePlacements.length, 1);
assert(graduationStageProps.orthographicPlacements.concat(
  graduationStageProps.perspectivePlacements).every(placement =>
  placement.source.descriptorKey === 0x01004020));
assert.deepStrictEqual(graduationStageProps.perspectivePlacements.map(placement =>
  [placement.poseSelector, placement.x, placement.y, placement.z]), [[8, -100, 0, 90]]);
assert.deepStrictEqual(catalog.modeTwoStagePlacementProfiles.flatMap(profile =>
  profile.specialRows).map(row => [row.tableKind, row.status, row.descriptorHandle]), [
  ['orthographic', -2, 201],
  ['orthographic', -2, 201],
  ['perspective', -2, 201]
]);
assert(directorScenes.every(scene => scene.launchProfile &&
  scene.launchProfile.profileId === 'launch-profile:' + scene.assetId));
const launchModeCounts = profiledDirectorScenes.reduce((counts, scene) => {
  const mode = scene.launchProfile.directorMode;
  const key = mode.evidenceStatus + ':' + mode.value;
  counts[key] = (counts[key] || 0) + 1;
  return counts;
}, {});
assert.deepStrictEqual(launchModeCounts, {
  'runtime-observed:0': 4,
  'runtime-observed:2': 6,
  'stream-structural:0': 2,
  'stream-structural:2': 26,
  'native-static-launch-class:0': 8,
  'parent-event-context-unresolved:null': 12,
  'external-unresolved:null': 2
});
assert.strictEqual(directorScenes.reduce((total, scene) =>
  total + scene.launchProfile.parentEventLaunches.length, 0), 1998);
assert.strictEqual(OB64.cutsceneData.counts.directEventDirectorSelectors, 1520);
assert.strictEqual(OB64.cutsceneData.counts.directEventDirectorResources, 1472);
assert.strictEqual(OB64.cutsceneData.counts.parentEventOuterEntries, 653);
assert.strictEqual(OB64.cutsceneData.counts.parentEventDistinctOuterCursors, 640);
assert.strictEqual(OB64.cutsceneData.counts.parentEventSequenceTables, 623);
assert.strictEqual(OB64.cutsceneData.counts.parentEventSequenceEntries, 1496);
assert.strictEqual(OB64.cutsceneData.counts.parentEventDirectOuterSequences, 17);
assert.strictEqual(OB64.cutsceneData.counts.parentEventDistinctSequenceCursors, 1513);
assert.strictEqual(OB64.cutsceneData.counts.directEventLaunchEntryCursors, 1443);
assert.strictEqual(OB64.cutsceneData.counts.directEventInvocationContexts, 2076);
assert.strictEqual(OB64.cutsceneData.counts.directEventConcurrentContextOwners, 433);
assert.strictEqual(OB64.cutsceneData.counts.directEventExactConcurrentLaunchOwners, 433);
assert.strictEqual(OB64.cutsceneData.counts.directEventMultiInvocationLaunches, 38);
assert.strictEqual(OB64.cutsceneData.counts.parentEventDistinctInvocationCursors, 1717);
assert.strictEqual(OB64.cutsceneData.counts.parentEventExternalRequestPhysicalSites, 45);
assert.strictEqual(OB64.cutsceneData.counts.parentEventExternalRequestHandoffs, 47);
assert.strictEqual(OB64.cutsceneData.counts.directEventExactPropertyE6Contexts, 18);
assert.strictEqual(
  OB64.cutsceneData.counts.directEventRequiredLaunchPreservationSnapshotContexts, 16);
assert.strictEqual(
  OB64.cutsceneData.counts.directEventOmittedLaunchPreservationSnapshotContexts, 2);
assert.strictEqual(OB64.cutsceneData.counts.directEventSecondRosterUnitLeaderOnlyContexts, 4);
const directEventInvocationContexts = directorScenes.flatMap(scene =>
  scene.launchProfile.parentEventLaunches.flatMap(launch => launch.eventInvocationContexts));
assert.strictEqual(directEventInvocationContexts.length, 2076);
assert(directEventInvocationContexts.every(context => context.launchFlagBit08 === false));
assert.strictEqual(directEventInvocationContexts.filter(context =>
  context.eventPropertyE6 !== null).length, 18);
assert.strictEqual(directEventInvocationContexts.filter(context =>
  context.launchPreservationSnapshot === true).length, 16);
assert.strictEqual(directEventInvocationContexts.filter(context =>
  context.launchPreservationSnapshot === false).length, 2);
assert.strictEqual(directEventInvocationContexts.filter(context =>
  context.secondRosterUnitLeaderOnly === true).length, 4);
assert.strictEqual(directEventInvocationContexts.filter(context =>
  context.precedingExternalRequest !== null).length, 14);
const externalRequests = OB64.cutsceneData.parentEventExternalRequests;
assert.strictEqual(externalRequests.length, 47);
assert.strictEqual(new Set(externalRequests.map(request =>
  request.eventDirectoryRow + ':' + request.decodedByteOffset)).size, 45);
assert(externalRequests.every(request =>
  request.requestAcceptanceCondition === 'set-synchronously-by-opcode-0x13' &&
  request.resumeTiming === 'next-event-state-processor-call'));
assert.deepStrictEqual(externalRequests.reduce((counts, request) => {
  counts[request.operand] = (counts[request.operand] || 0) + 1;
  return counts;
}, {}), { 14: 1, 20: 1, 21: 2, 22: 36, 23: 1, 24: 1, 25: 1, 26: 4 });
assert.deepStrictEqual(Array.from(new Set(externalRequests.map(request =>
  request.eventDirectoryRow + ':' + request.decodedByteOffset)))
  .filter(site => externalRequests.filter(request =>
    request.eventDirectoryRow + ':' + request.decodedByteOffset === site).length > 1),
['4:108', '35:682']);
const translationWrites = catalog.parentEventTranslationWrites;
assert.strictEqual(translationWrites.length, 416);
assert.strictEqual(OB64.cutsceneData.counts.parentEventTranslationPhysicalSites, 69);
assert.strictEqual(OB64.cutsceneData.counts.parentEventExactTranslationWriteContexts, 128);
assert.strictEqual(OB64.cutsceneData.counts.parentEventUnresolvedTranslationWriteContexts, 288);
assert.strictEqual(OB64.cutsceneData.counts.parentEventRetailTranslationPhysicalSites, 21);
assert.strictEqual(OB64.cutsceneData.counts.parentEventRetailTranslationWriteContexts, 344);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventExactRetailTranslationWriteContexts, 74);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventUnresolvedRetailTranslationWriteContexts, 270);
assert.strictEqual(OB64.cutsceneData.counts.parentEventNonretailTranslationPhysicalSites, 48);
assert.strictEqual(OB64.cutsceneData.counts.parentEventNonretailTranslationWriteContexts, 72);
assert.deepStrictEqual(translationWrites.reduce((counts, write) => {
  counts[write.tableIndex] = (counts[write.tableIndex] || 0) + 1;
  return counts;
}, {}), {
  0: 38,
  1: 18,
  2: 18,
  3: 18,
  4: 18,
  5: 18,
  6: 18,
  7: 18,
  8: 18,
  9: 18,
  10: 18,
  11: 18,
  12: 18,
  13: 18,
  14: 18,
  15: 18,
  16: 36,
  255: 72
});
assert.deepStrictEqual(Array.from(new Set(translationWrites.filter(write =>
  write.eventDirectoryRow === 66 && write.tableIndex < 17 && write.value !== null)
  .map(write => JSON.stringify([
    write.decodedByteOffset, write.tableIndex, write.value
  ])))).map(value => JSON.parse(value)), [
  [0x1B2C, 0, 0],
  [0x1B32, 16, 81],
  [0x1B3A, 0, 2],
  [0x1B40, 16, 83]
]);
assert.deepStrictEqual(translationWrites.filter(write =>
  write.eventDirectoryRow === 98 && write.tableIndex < 17 && write.value !== null)
  .map(write => [write.decodedByteOffset, write.tableIndex, write.value]), [
  [0x0214, 0, 88],
  [0x022C, 0, 90]
]);
assert.deepStrictEqual(catalog.parentEventSubstitutionSources.map(source => [
  source.sourceId,
  source.semantic,
  source.characterRecordFieldOffset,
  source.characterRecordStride,
  source.slotCount
]), [
  ['A', 'primary-class-id', 0x11, 56, 5],
  ['B', 'secondary-class-id', 0x12, 56, 5]
]);
const substitutionSourceWrites = catalog.parentEventSubstitutionSourceWrites;
assert.strictEqual(substitutionSourceWrites.length, 42);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventSubstitutionSourcePhysicalSites, 42);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventSubstitutionSourceAWriteContexts, 21);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventSubstitutionSourceBWriteContexts, 21);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventExactSubstitutionSourceIndexContexts, 2);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventUnresolvedSubstitutionSourceIndexContexts, 40);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventExactSubstitutionSourceValueContexts, 4);
assert.strictEqual(
  OB64.cutsceneData.counts.parentEventUnresolvedSubstitutionSourceValueContexts, 38);
assert(substitutionSourceWrites.every(write =>
  write.eventDirectoryRow === 67 &&
  write.eventResourceKey === '0x003B0BBC' &&
  write.characterRecordStride === 56 &&
  write.characterRecordFieldOffset === (write.sourceId === 'A' ? 0x11 : 0x12)));
assert.deepStrictEqual(substitutionSourceWrites.slice(0, 2).map(write => [
  write.decodedByteOffset,
  write.sourceId,
  write.sourceIndex,
  write.sourceSemantic
]), [
  [0x1C36, 'A', 0, 'primary-class-id'],
  [0x1C38, 'B', 0, 'secondary-class-id']
]);
assert.deepStrictEqual(substitutionSourceWrites.filter(write => write.value !== null)
  .map(write => [write.decodedByteOffset, write.sourceId, write.value]), [
  [0x22E4, 'A', 4],
  [0x22E6, 'B', 4],
  [0x22F6, 'A', 2],
  [0x22F8, 'B', 2]
]);
assert.deepStrictEqual(OB64.cutsceneData.counts.retailDirectorLaunchContextClasses, {
  1: 369,
  2: 47,
  4: 117,
  5: 92,
  6: 3,
  7: 1,
  8: 869
});
const classFourScenes = directorScenes.filter(scene =>
  scene.launchProfile.launchContext.classId === 4);
const exactClassFourImages = classFourScenes.filter(scene =>
  scene.launchProfile.oversizedImagePresentation.assetId !== null);
assert.strictEqual(classFourScenes.length, 117);
assert.strictEqual(exactClassFourImages.length, 106);
assert.strictEqual(classFourScenes.length - exactClassFourImages.length, 11);
assert.strictEqual(OB64.cutsceneData.counts.classFourOversizedImageScenes, 117);
assert.strictEqual(OB64.cutsceneData.counts.exactClassFourOversizedImageScenes, 106);
assert.strictEqual(OB64.cutsceneData.counts.unresolvedClassFourOversizedImageScenes, 11);
assert.strictEqual(OB64.cutsceneData.counts.sceneVignetteDirectorResources, 107);
assert.strictEqual(OB64.cutsceneData.counts.exactSceneVignetteDirectorResources, 106);
assert.strictEqual(OB64.cutsceneData.counts.unresolvedSceneVignetteDirectorResources, 1);
assert(classFourScenes.every(scene => scene.launchProfile.oversizedImagePresentation &&
  scene.launchProfile.oversizedImagePresentation.active === true),
'every terminal-class-4 launch must preserve its separate scene-image owner');
assert(exactClassFourImages.every(scene =>
  scene.launchProfile.oversizedImagePresentation.source ===
    'director-launch-prescan-opcode-0x80000007'),
'every exact class-4 image must come from its physical launch-row selector');
const oversizedRules = OB64.cutsceneData.oversizedImagePresentationRules;
assert.strictEqual(oversizedRules.classTableRowCount, 69);
assert.strictEqual(oversizedRules.classTableRowBytes, 9);
assert.strictEqual(oversizedRules.classTableSelectorOffset, 3);
assert.strictEqual(oversizedRules.children.length, 41);
assert.strictEqual(oversizedRules.rows.length, 69);
assert.deepStrictEqual(oversizedRules.children.filter(child => child.assetId === null)
  .map(child => child.childSelector), [0, 34, 37]);
assert.deepStrictEqual(oversizedRules.children.filter(child => child.assetId !== null)
  .map(child => child.archiveIndex),
Array.from({ length: 38 }, (_, index) => 120 + index));
assert(directorScenes.every(scene => scene.launchProfile.launchContext &&
  scene.launchProfile.launchContext.source === 'terminal-trailer-low-byte'));
const nativeResourceRouteByTerminalClass = {
  1: -3, 2: -6, 3: -5, 4: -4, 5: -7, 6: -8, 7: -9, 8: -10
};
assert(directorScenes.every(scene =>
  scene.launchProfile.launchContext.effectiveResourceRoute ===
    nativeResourceRouteByTerminalClass[scene.launchProfile.launchContext.classId]),
'every Director resource must carry its terminal-class dispatcher route');
assert(directorScenes.filter(scene => scene.launchProfile.launchContext.classId === 2)
  .every(scene => scene.launchProfile.launchContext.resourceLoaderModeWrite === 1));
assert(directorScenes.filter(scene => scene.launchProfile.launchContext.classId === 7)
  .every(scene => scene.launchProfile.launchContext.resourceLoaderModeWrite === 2));
assert(directorScenes.filter(scene => ![2, 7].includes(
  scene.launchProfile.launchContext.classId)).every(scene =>
    scene.launchProfile.launchContext.resourceLoaderModeWrite === null));
const eventOwnedBackground = directorScenes.find(scene =>
  scene.assetId === 'rom-director:01F6864C');
assert.strictEqual(eventOwnedBackground.launchProfile.directorMode.evidenceStatus,
  'native-static-launch-class');
assert.strictEqual(eventOwnedBackground.launchProfile.directorMode.value, 0);
assert.strictEqual(eventOwnedBackground.launchProfile.background.requests[0].selectorTableId,
  'background-table:scene:31',
'terminal class 5 must use the native scene-group preload instead of the mode-two environment table');
assert.strictEqual(eventOwnedBackground.launchProfile.background.requests[0].selector, 11);
assert(eventOwnedBackground.launchProfile.background.requests[0].stageAssetIds.length > 0);
const multiEntryLaunchScene = directorScenes.find(scene =>
  scene.assetId === 'rom-director:01F56D8E');
assert.deepStrictEqual(multiEntryLaunchScene.launchProfile.parentEventLaunches.map(launch => [
  launch.eventDirectoryRow,
  launch.decodedByteOffset,
  launch.eventEntryCursor,
  launch.eventEntryOffset,
  launch.eventEntryPaths
]), [
  [19, 0x0524, 0x04A4, 0x0080, [{
    kind: 'nested-scheduler-sequence',
    outerEntryIndexes: [5],
    sequenceTableCursor: 0x03A2,
    sequenceEntryIndexes: [3]
  }]],
  [19, 0x0592, 0x053E, 0x0054, [{
    kind: 'nested-scheduler-sequence',
    outerEntryIndexes: [5],
    sequenceTableCursor: 0x03A2,
    sequenceEntryIndexes: [4]
  }]],
  [66, 0x0676, 0x0660, 0x0016, [{
    kind: 'nested-scheduler-sequence',
    outerEntryIndexes: [0, 1, 2, 3, 4, 5],
    sequenceTableCursor: 0x000C,
    sequenceEntryIndexes: [63]
  }]],
  [66, 0x069C, 0x0686, 0x0016, [{
    kind: 'nested-scheduler-sequence',
    outerEntryIndexes: [0, 1, 2, 3, 4, 5],
    sequenceTableCursor: 0x000C,
    sequenceEntryIndexes: [64]
  }]]
]);
assert.deepStrictEqual(multiEntryLaunchScene.launchProfile.parentEventLaunches.map(launch =>
  launch.eventInvocationContexts.map(context => [
    context.eventInvocationCursor,
    context.eventInvocationOffset,
    context.precedingDirectorLaunchCount,
    context.precedingDirectorSelector,
    context.precedingDirectorResourceKey
  ])), [
  [[0x050C, 0x0018, 1, 152, '0x019C21E6']],
  [[0x057A, 0x0018, 1, 153, '0x019C267C']],
  [[0x0664, 0x0012, 1, 152, '0x019C21E6']],
  [[0x068A, 0x0012, 1, 153, '0x019C267C']]
]);
assert.deepStrictEqual(multiEntryLaunchScene.launchProfile.parentEventLaunches.map(launch =>
  launch.eventInvocationContexts.map(context => [
    context.precedingDirectorLaunchId,
    context.precedingDirectorInvocationCursor,
    context.concurrentDirectorTickOffset,
    context.concurrentDirectorSceneId,
    context.concurrentDirectorAssetId,
    context.sceneStateRelation
  ])), [
  [['event-director:19:b050A', 0x04A4, 1,
    'scene:director:01f5646a', 'rom-director:01F5646A',
    'previous-event-request-concurrent-scene-state']],
  [['event-director:19:b0578', 0x053E, 1,
    'scene:director:01f56900', 'rom-director:01F56900',
    'previous-event-request-concurrent-scene-state']],
  [['event-director:66:b0662', 0x0660, 1,
    'scene:director:01f5646a', 'rom-director:01F5646A',
    'previous-event-request-concurrent-scene-state']],
  [['event-director:66:b0688', 0x0686, 1,
    'scene:director:01f56900', 'rom-director:01F56900',
    'previous-event-request-concurrent-scene-state']]
]);
assert.strictEqual(multiEntryLaunchScene.backgroundRequests.length, 0,
  'inherited Stage presentation must not synthesize a Director background command');
assert.strictEqual(
  multiEntryLaunchScene.launchProfile.background.inheritedPresentation.sentinel, -2);
assert.deepStrictEqual(
  multiEntryLaunchScene.launchProfile.background.inheritedPresentation.stageAssetIds, [
    'archive:94',
    'scene-resource:016C90D4',
    'scene-resource:016CA720',
    'scene-resource:016CBCDE',
    'scene-resource:016CC968'
  ]);
assert(multiEntryLaunchScene.launchProfile.background.inheritanceContexts.every(context =>
  context.resolutionStatus === 'resolved-unanimous-stage'));
const inheritedStageDocument = C.createSceneDocument(multiEntryLaunchScene);
assert.deepStrictEqual(inheritedStageDocument.background.layers.map(layer => layer.assetId), [
  'archive:94',
  'scene-resource:016C90D4',
  'scene-resource:016CA720',
  'scene-resource:016CBCDE',
  'scene-resource:016CC968'
], 'the initial editor document must show the inherited Stage without editing the stream');
const launchBackgroundRequests = profiledDirectorScenes.flatMap(scene =>
  scene.launchProfile.background.requests);
assert.strictEqual(launchBackgroundRequests.length, 41);
assert.strictEqual(launchBackgroundRequests.filter(request =>
  request.selectorSource === 'director-launch-prescan-command-operand' &&
  request.selectorTableId === 'background-table:mode2-environment:80').length, 26,
'the native launch pre-scan must route mode-two environment operands without editing streams');
assert(launchBackgroundRequests.filter(request =>
  request.selectorSource === 'director-launch-prescan-command-operand').every(request =>
  request.environmentSelector === request.commandOperand &&
  request.stageLayers.some(layer => layer.role === 'environment-base') &&
  (request.foregroundSelectorSource === 'event-context-mode-two-environment-copy'
    ? request.foregroundSelector === request.commandOperand &&
      request.foregroundSelectorCandidates.length === 1 &&
      request.foregroundSelectorCandidates[0] === request.commandOperand
    : request.foregroundSelector === null &&
      request.foregroundSelectorSource === 'source-only-launch-route-unresolved' &&
      request.foregroundSelectorCandidates.length === 0)),
'a pre-scanned mode-two operand must preserve its exact event foreground or withhold an unowned source-only foreground');
const observedModeTwoRequests = profiledDirectorScenes.filter(scene =>
  scene.launchProfile.directorMode.value === 2 &&
  scene.launchProfile.directorMode.evidenceStatus === 'runtime-observed' &&
  scene.backgroundRequests.length);
assert.strictEqual(observedModeTwoRequests.length, 5);
assert(observedModeTwoRequests.every(scene =>
  scene.launchProfile.background.requests[0].selectorSource ===
    'runtime-observed-environment-selector' &&
  scene.launchProfile.background.requests[0].environmentSelector ===
    scene.backgroundRuntimeObservation.environmentSelector &&
  scene.launchProfile.background.requests[0].foregroundSelector ===
    scene.backgroundRuntimeObservation.foregroundSelector));
assert.strictEqual(profiledDirectorScenes.filter(scene =>
  scene.launchProfile.cameras.actor.evidenceStatus === 'runtime-observed').length, 6);
assert(profiledDirectorScenes.filter(scene =>
  scene.launchProfile.directorMode.evidenceStatus === 'stream-structural' &&
  scene.launchProfile.directorMode.value === 2).every(scene =>
    scene.launchProfile.cameras.actor.kind === 'mode-two-overlay-initializer' &&
    scene.launchProfile.cameras.actor.evidenceStatus === 'native-static' &&
    scene.launchProfile.cameras.actor.projection.modelScale === 0.10000000149011612 &&
    scene.launchProfile.cameras.actor.projection.fovYDegrees === 12.880000114440918 &&
    scene.launchProfile.cameras.actor.projection.eye.x === 82.38899993896484),
'unobserved mode-two scenes must use the immutable overlay Actor camera');
assert.strictEqual(catalog.imageAssets.length, 366);
assert.strictEqual(profiledDirectorScenes.filter(scene => scene.actorBearing).length, 48);
assert.strictEqual(profiledDirectorScenes.reduce((sum, scene) => sum + scene.actorCount, 0), 205);
assert.strictEqual(directorScenes.filter(scene => scene.actorBearing).length, 1190);
assert.strictEqual(directorScenes.reduce((sum, scene) => sum + scene.actorCount, 0), 3041);
assert.strictEqual(catalog.imageAssets.filter(asset => asset.renderable).length, 366);
assert(catalog.imageAssets.every(asset => asset.renderable
  ? asset.unsupportedReason === null
  : typeof asset.unsupportedReason === 'string' && asset.unsupportedReason.length > 0));
assert.strictEqual(catalog.actorArtSources.length, 68);
assert.strictEqual(catalog.posePrograms.length, 2275);
assert.strictEqual(OB64.cutsceneData.counts.renderablePoseSelectors, 2259);
assert.strictEqual(OB64.cutsceneData.counts.controlOnlyPoseSelectors, 16);
assert.strictEqual(OB64.cutsceneData.counts.structuralPoseStates, 0);
assert.strictEqual(OB64.cutsceneData.counts.sourceUnpublishedPoseStates, 1554);
assert.strictEqual(OB64.cutsceneData.counts.evidenceBackedDirectorStages, 1428);
assert.strictEqual(OB64.cutsceneData.counts.sceneGroupPreloadDirectorResources, 209);
assert.strictEqual(OB64.cutsceneData.counts.sceneGroupPreloadBackgroundCommands, 191);
assert.strictEqual(OB64.cutsceneData.counts.inheritedStageDirectorResources, 90);
assert.strictEqual(OB64.cutsceneData.counts.inheritedStageLaunchContexts, 177);
assert.strictEqual(
  OB64.cutsceneData.counts.contextOnlyResolvedStageInheritanceContexts, 18);
assert.strictEqual(OB64.cutsceneData.counts.unresolvedStageInheritanceContexts, 209);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoCommandSeededEnvironmentStages, 1146);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoDerivedEnvironmentSentinels, 17);
assert.strictEqual(
  OB64.cutsceneData.counts.modeTwoDerivedEnvironmentInvocationContexts, 18);
assert.strictEqual(
  OB64.cutsceneData.counts.exactModeTwoDerivedEnvironmentInvocationContexts, 0);
assert.strictEqual(OB64.cutsceneData.counts.modeTwoScenesWithoutBackgroundCommand, 12);
assert.strictEqual(OB64.cutsceneData.counts.unresolvedModeTwoLaunchStages, 25);
assert.strictEqual(OB64.cutsceneData.counts.unresolvedModeTwoForegroundSelections, 43);
assert.strictEqual(OB64.cutsceneData.counts.directEventExactPropertyE9Contexts, 183);
assert.strictEqual(OB64.cutsceneData.counts.directEventExactPropertyFCContexts, 0);
assert.strictEqual(OB64.cutsceneData.counts.directEventExactPropertyFDContexts, 25);
const translatedScenes = directorScenes.filter(scene =>
  scene.launchProfile.operandTranslation.required);
assert.strictEqual(translatedScenes.length, 19);
assert.strictEqual(translatedScenes.reduce((total, scene) =>
  total + scene.launchProfile.operandTranslation.placeholderCount, 0), 398);
const translatedTemplate = directorScenes.find(scene =>
  scene.assetId === 'rom-director:01FA64D2');
assert.strictEqual(translatedTemplate.launchProfile.operandTranslation.placeholderCount, 22);
assert.deepStrictEqual(translatedTemplate.launchProfile.operandTranslation.tableIndexes,
  Array.from({ length: 17 }, (_, index) => index));
assert.strictEqual(translatedTemplate.launchProfile.operandTranslation.resolvedContextCount, 0);
assert.strictEqual(translatedTemplate.launchProfile.operandTranslation.unresolvedContextCount, 1);
assert(translatedTemplate.launchProfile.operandTranslation.launchContexts[0].tableValues
  .every(value => value === null));
const exactTranslatedScene = directorScenes.find(scene =>
  scene.launchProfile.operandTranslation.resolvedContextCount === 2);
assert(exactTranslatedScene);
assert.deepStrictEqual(exactTranslatedScene.launchProfile.operandTranslation.tableIndexes, [0]);
assert.deepStrictEqual(exactTranslatedScene.launchProfile.operandTranslation.launchContexts
  .map(context => context.tableValues[0]).sort((left, right) => left - right), [0x58, 0x5A]);
const translatedVariantActor = translatedTemplate.actors.find(actor =>
  actor.variantSelectorTranslationIndex === 0);
assert(translatedVariantActor);
assert.strictEqual(translatedVariantActor.variantSelector, 0);
assert.strictEqual(OB64.cutsceneData.counts.observedActorLaunchCameras, 6);
const modeZeroDirectorScenes = directorScenes.filter(scene =>
  scene.launchProfile.directorMode.value === 0);
assert.strictEqual(modeZeroDirectorScenes.length, 211);
assert.strictEqual(modeZeroDirectorScenes.filter(scene =>
  scene.launchProfile.background.requests.length === 0).length, 20);
const modeZeroCommandStages = modeZeroDirectorScenes.filter(scene =>
  scene.launchProfile.background.requests.length > 0);
assert.strictEqual(modeZeroCommandStages.length, 191);
assert(modeZeroCommandStages.every(scene =>
  scene.launchProfile.background.requests.every(request =>
    request.selectorSource === 'director-launch-prescan-class-4-or-5-scene-group' &&
    request.stageLayers.length === request.members.length &&
    request.stageLayers.every((layer, index) =>
      layer.assetId === request.members[index].assetId &&
      layer.nativeOrdinal === request.members[index].ordinal))),
'every proven mode-zero background command must publish its exact ordered Stage members');
assert.strictEqual(catalog.posePrograms.filter(program =>
  !program.sourceProgramDefined && program.frames.length).length, 1554);
assert.strictEqual(catalog.dialogueArchives.length, 349);
assert.strictEqual(catalog.dialogueArchives.reduce((sum, archive) =>
  sum + archive.entryCount, 0), 5200);
assert.strictEqual(catalog.getDialogueArchive('meswin:815').entryCount, 631);
assert.strictEqual(catalog.getImageAsset('archive:688'), null);
assert.strictEqual(catalog.getImageAsset('scene-resource:016C90D4').sourceKind, 'rom-resource');
assert.strictEqual(
  catalog.getBackgroundSelectorTable('background-table:mode2-environment:80')
    .tableResourceKey,
  4,
  'mode-2 environment bases must come from resource 4');
assert.strictEqual(
  catalog.getBackgroundSelectorEntry('background-table:mode2-environment:80', 57).assetId,
  'mode2-environment:00236B58');
assert.deepStrictEqual(
  catalog.getBackgroundSelectorEntry('background-table:mode2-environment:80', 57)
    .stageLayers.map(layer => [layer.assetId, layer.role]),
  [['mode2-environment:00236B58', 'environment-base']],
  'an environment-table row must not silently select the same-index foreground row');
assert.deepStrictEqual(
  catalog.getBackgroundSelectorEntry('background-table:mode2-overlay:80', 57).archiveAssetIds,
  ['archive:49'],
  'selector 57 foreground must remain independent from its complete environment base');
assert.strictEqual(
  catalog.getBackgroundSelectorEntry('background-table:mode2-overlay:80', 57).resourceKey,
  0x00304464);

const preScannedModeTwo = catalog.getScene('rom-director:01F53488');
const preScannedModeTwoRequest = preScannedModeTwo.launchProfile.background.requests[0];
assert.strictEqual(preScannedModeTwoRequest.wordStart, 0);
assert.strictEqual(preScannedModeTwoRequest.commandOperand, 20);
assert.strictEqual(preScannedModeTwoRequest.environmentSelector, 20);
assert.strictEqual(preScannedModeTwoRequest.foregroundSelector, 20);
assert.strictEqual(preScannedModeTwoRequest.foregroundSelectorSource,
  'event-context-mode-two-environment-copy');
assert.deepStrictEqual(preScannedModeTwoRequest.stageAssetIds,
  ['section-c-njpg:20', 'archive:32']);
assert.deepStrictEqual(preScannedModeTwoRequest.foregroundSelectorCandidates, [20]);
const derivedModeTwo = catalog.getScene('rom-director:01F516AC');
const derivedModeTwoRequest = derivedModeTwo.launchProfile.background.requests[0];
assert.strictEqual(derivedModeTwoRequest.commandOperand, -1);
assert.strictEqual(derivedModeTwoRequest.environmentSelector, null);
assert.strictEqual(derivedModeTwoRequest.selectorSource,
  'director-launch-prescan-derived-sentinel');
assert.deepStrictEqual(derivedModeTwoRequest.stageAssetIds, []);
assert.strictEqual(derivedModeTwoRequest.derivedEnvironment.mapperId,
  'unsigned-existing-context-loader');
assert.strictEqual(derivedModeTwoRequest.derivedEnvironment.mapperFunctionZ64,
  '0x00067600');
assert.strictEqual(derivedModeTwoRequest.derivedEnvironment.contextCount, 1);
assert.strictEqual(derivedModeTwoRequest.derivedEnvironment.exactContextCount, 0);
assert.strictEqual(derivedModeTwoRequest.derivedEnvironment.unresolvedContextCount, 1);
assert.deepStrictEqual(
  derivedModeTwoRequest.derivedEnvironment.outOfRangeEnvironmentSelectorCandidates,
  [254]);
assert.deepStrictEqual(derivedModeTwoRequest.foregroundSelectorCandidates,
  derivedModeTwoRequest.derivedEnvironment.environmentSelectorCandidates);
assert(derivedModeTwoRequest.derivedEnvironment.environmentSelectorCandidates
  .includes(56));
assert.strictEqual(
  derivedModeTwoRequest.derivedEnvironment.contexts[0].inputs.scenarioKey, null);
assert.strictEqual(
  derivedModeTwoRequest.derivedEnvironment.contexts[0].inputs.currentUnitSelector, null);
assert.strictEqual(
  derivedModeTwoRequest.derivedEnvironment.contexts[0].inputs.battleTerrain, null);
assert.strictEqual(OB64.cutsceneData.modeTwoDerivedEnvironmentRules.mappers.length, 2);
assert.deepStrictEqual(
  OB64.cutsceneData.modeTwoDerivedEnvironmentRules.mappers[0].terrainRows,
  OB64.cutsceneData.modeTwoDerivedEnvironmentRules.mappers[1].terrainRows);
assert.deepStrictEqual(
  OB64.cutsceneData.modeTwoDerivedEnvironmentRules.mappers[0].scenarioValues,
  OB64.cutsceneData.modeTwoDerivedEnvironmentRules.mappers[1].scenarioValues);

const graduation = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
assert(graduation);
assert.strictEqual(graduation.backgroundRuntimeObservation.environmentSelector, 57);
assert.strictEqual(graduation.backgroundRuntimeObservation.foregroundSelector, 57);
const prologue = catalog.getScene('prologue-title');
assert.strictEqual(prologue.backgroundRuntimeObservation.environmentSelector, 62);
assert.strictEqual(prologue.backgroundRuntimeObservation.foregroundSelector, null,
  'an inactive stored foreground selection must remain distinct from selector zero');
assert.strictEqual(graduation.friendlyName, 'Graduation Ceremony');
assert.strictEqual(catalog.getScene('starting-choices-cutscene'), graduation);
assert.strictEqual(catalog.getScene(graduation.assetId), graduation);
assert.strictEqual(catalog.searchScenes('graduation').length, 1);
assert.strictEqual(catalog.searchScenes('starting choices').length, 1);
assert.strictEqual(catalog.searchScenes('', { actorBearing: false }).length, 316);
assert.strictEqual(profiledDirectorScenes.filter(scene => !scene.actorBearing).length, 12);

const document = C.createSceneDocument(graduation);
assert.strictEqual(document.identity.sceneId, graduation.sceneId);
assert.strictEqual(document.actors.length, 8);
assert.strictEqual(document.native.sourceAssetId, graduation.assetId);
assert.strictEqual(document.exportRequirements.capability, 'needs-research');
assert(document.actors.every(actor => actor.source.visibilityStatus));

const souko = catalog.getImageAsset('archive:100');
assert(souko);
assert.strictEqual(souko.container, 'bg2');
assert.strictEqual(souko.width, 415);
assert.strictEqual(souko.height, 303);
assert.strictEqual(souko.renderable, true);
assert.strictEqual(catalog.searchImages('souko').length, 1);
assert(catalog.searchImages('', { family: 'background' }).length >= 52);

const magnusWalk = catalog.getPoseProgram(30, 1, 2);
assert(magnusWalk);
assert.strictEqual(magnusWalk.stateIndex, 12);
assert.strictEqual(magnusWalk.durationFrames, 48);
assert.deepStrictEqual(magnusWalk.frames.map(frame => frame.frameToken), [35, 30, 31, 32, 33, 34]);
assert.strictEqual(catalog.poseProgramsForBank(30).length, 148);

const zeroDurationProgram = catalog.posePrograms.find(program =>
  program.programId === 'physical-program:bank:015:state:0027');
assert(zeroDurationProgram);
assert(zeroDurationProgram.frames.some(frame => frame.durationFrames === 0));

const duplicate = JSON.parse(JSON.stringify(OB64.cutsceneData));
duplicate.scenes[1].sceneId = duplicate.scenes[0].sceneId;
assert.throws(() => C.createCatalog(duplicate), /duplicates/);

const brokenBoundary = JSON.parse(JSON.stringify(OB64.cutsceneData));
brokenBoundary.scenes.find(scene => scene.source.dynamicGrammar !== true)
  .source.nodes[0].startWord = 1;
assert.throws(() => C.createCatalog(brokenBoundary), /contiguous source boundary/);

console.log('PASS Cutscene catalog exposes 1,498 retail Director resources, 60 enriched profiles, ' +
  '1,146 command-seeded mode-two environments, 191 class-owned scene groups, 90 inherited Stages, 1,428 evidence-backed Stages, 1,998 direct parent-event launches, and 398 launch-translated operands without embedding Director words.');
