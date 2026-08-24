'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const MASTER = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.module = undefined;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of [
  'data.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js', 'animation-ui.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js',
  'cutscene-director.js', 'cutscene-codec.js', 'cutscene-runtime.js',
  'cutscene-preview.js', 'cutscene-assets.js', 'cutscene-sprites.js', 'cutscene-renderer.js',
]) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}

function normalizeV64(raw) {
  const output = new Uint8Array(raw.length);
  for (let index = 0; index < raw.length; index += 2) {
    output[index] = raw[index + 1];
    output[index + 1] = raw[index];
  }
  return output;
}

function hashBytes(input) {
  return crypto.createHash('sha256').update(Buffer.from(input)).digest('hex').toUpperCase();
}

(async function main() {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const z64 = normalizeV64(fs.readFileSync(MASTER));
  const spriteState = OB64.cutsceneSprites.create(z64, catalog);
  const scene = catalog.directorScenes.find(row => row.friendlyName === 'Graduation Ceremony');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
  const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog);
  const runtime = OB64.cutsceneRuntime.compile(
    projected.document, projected.program, scene, catalog, { z64 });

  async function compileScene(identity, runtimeOptions) {
    const selectedScene = catalog.getScene(identity);
    const selectedSource = await OB64.cutsceneCodec.loadSceneSource(
      z64, selectedScene, { hashBytes });
    const selectedProjection = OB64.cutsceneCodec.projectSceneDocument(
      selectedScene, selectedSource, catalog);
    return OB64.cutsceneRuntime.compile(selectedProjection.document,
      selectedProjection.program, selectedScene, catalog,
      Object.assign({ z64 }, runtimeOptions || {}));
  }

  const translatedTemplateScene = catalog.getScene('rom-director:01FA64D2');
  const translatedTemplateSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, translatedTemplateScene, { hashBytes });
  const translatedTemplateProjected = OB64.cutsceneCodec.projectSceneDocument(
    translatedTemplateScene, translatedTemplateSource, catalog);
  const rawPlaceholderCount = translatedTemplateProjected.program.primitives.reduce(
    (total, primitive) => total + primitive.rawWords.filter(word =>
      ((word >>> 0) & 0xFFFFFF00) === 0x08880000).length, 0);
  assert.strictEqual(rawPlaceholderCount, 22,
    'the source-backed program must preserve every native launch placeholder word');
  const unresolvedTranslationRuntime = OB64.cutsceneRuntime.compile(
    translatedTemplateProjected.document, translatedTemplateProjected.program,
    translatedTemplateScene, catalog, { z64 });
  assert.deepStrictEqual(unresolvedTranslationRuntime.launchOperandTranslation.missingIndexes,
    Array.from({ length: 17 }, (_, index) => index));
  assert(unresolvedTranslationRuntime.trace.some(row =>
    row.kind === 'launch-translation-missing' &&
      row.nodeId === 'node:01FA64D2:w007F'),
  'unresolved dynamic body-pose operands must be withheld instead of parsed literally');
  const translatedPreviewState = unresolvedTranslationRuntime.states[0];
  const translatedPreviewFrames = OB64.cutsceneSprites.framesForPreview(
    spriteState, translatedPreviewState);
  assert.strictEqual(Object.keys(translatedPreviewFrames).length,
    translatedPreviewState.actors.filter(actor => actor.visible).length,
    'withheld launch-dependent body poses must retain the preceding renderable Actor poses');
  const suppliedTranslations = Object.fromEntries(
    Array.from({ length: 17 }, (_, index) => [index, 0]));
  const resolvedTranslationRuntime = OB64.cutsceneRuntime.compile(
    translatedTemplateProjected.document, translatedTemplateProjected.program,
    translatedTemplateScene, catalog, { z64, launchOperandTranslations: suppliedTranslations });
  assert.strictEqual(resolvedTranslationRuntime.launchOperandTranslation.status, 'resolved');
  assert.deepStrictEqual(resolvedTranslationRuntime.launchOperandTranslation.missingIndexes, []);
  assert(resolvedTranslationRuntime.trace.some(row =>
    row.kind === 'launch-translation'),
  'supplied launch-table halfwords must produce a translated execution view');
  assert.strictEqual(translatedTemplateProjected.program.primitives.reduce(
    (total, primitive) => total + primitive.rawWords.filter(word =>
      ((word >>> 0) & 0xFFFFFF00) === 0x08880000).length, 0), 22,
  'runtime translation must never rewrite the source-backed Director program');

  const comparisonRuntime = await compileScene('rom-director:01F3E500', { maxTicks: 20 });
  assert(!comparisonRuntime.assumptions.some(message =>
    /set to its passing value/.test(message)),
  'immediate branch queries must not receive query-specific synthetic passing values');
  const normalScan = comparisonRuntime.trace.find(row =>
    row.kind === 'branch-query' && row.nodeId === 'node:01F3E500:w00B4');
  assert(normalScan, 'the ordinary primitive query must execute');
  assert.deepStrictEqual([
    normalScan.passed, normalScan.resynchronization,
    normalScan.destinationNodeId, normalScan.bridgeNodeId,
  ], [false, false, 'node:01F3E500:w00DE', 'node:01F3E500:w00DF'],
  'normal query failure must select the depth-matched barrier before the next bridge');

  const gracefulCloseRuntime = await compileScene(
    'rom-director:01F3E500', { maxTicks: 12 });
  const gracefulCloseQueries = gracefulCloseRuntime.trace.filter(row =>
    row.kind === 'branch-query' && row.nodeId === 'node:01F3E500:w00B4');
  assert.deepStrictEqual(gracefulCloseQueries.map(row => row.actual),
    [0, 0, 0, 0, 0, 0, 0, -6],
    'opcode 0x83 must retain the entity during seven close updates and detach it on update eight');
  assert(gracefulCloseRuntime.trace.some(row =>
    row.kind === 'transient-render-entity-detach' && row.tick === 11 && row.slot === 0),
  'the graceful close must publish the exact native eight-update detach boundary');
  assert(!gracefulCloseQueries.some(row => row.actual === -2),
    'graceful close state must not be confused with the preset-specific cancel result -2');

  const transientMenuRuntime = await compileScene(
    'rom-director:01F3F70E', { maxTicks: 1630 });
  const transientNeutralUpdate = transientMenuRuntime.trace.find(row =>
    row.kind === 'transient-render-entity-status' && row.slot === 0 &&
      row.preset === 3);
  assert.deepStrictEqual([
    transientNeutralUpdate && transientNeutralUpdate.tick,
    transientNeutralUpdate && transientNeutralUpdate.status,
    transientNeutralUpdate && transientNeutralUpdate.source,
  ], [1627, -1, 'native-main-menu-neutral'],
  'main-overlay menu presets must replace their creation status on the next native update');
  const transientNeutralQuery = transientMenuRuntime.trace.find(row =>
    row.kind === 'branch-query' && row.nodeId === 'node:01F3F70E:w079F');
  assert.strictEqual(transientNeutralQuery.actual, -1,
    'the menu decision tree must see neutral -1 instead of permanent creation status zero');
  assert(transientMenuRuntime.missingInputs.some(message =>
    /requires a controller\/menu result/.test(message)),
  'a neutral menu entity must report its exact external completion owner');

  const suppliedTransientMenuRuntime = await compileScene(
    'rom-director:01F3F70E', {
      maxTicks: 1630,
      externalQueryValues: { 'node:01F3F70E:w07A8': 1 }
    });
  const suppliedTransientMenuQuery = suppliedTransientMenuRuntime.trace.find(row =>
    row.kind === 'branch-query' && row.nodeId === 'node:01F3F70E:w07A8');
  assert.deepStrictEqual([
    suppliedTransientMenuQuery.actual, suppliedTransientMenuQuery.passed,
  ], [1, true],
  'an explicit query result must drive the native branch without rewriting Director bytes');

  const resynchronizedBranchRuntime = await compileScene(
    'rom-director:01F440B2', { maxTicks: 760 });
  const sceneVignetteState = resynchronizedBranchRuntime.states.find(state =>
    state.sceneVignette &&
      state.sceneVignette.nodeId === 'node:01F440B2:w0073');
  assert(sceneVignetteState,
    'opcode 0x3A must activate the launch-owned class-4 scene image');
  assert.deepStrictEqual([
    sceneVignetteState.sceneVignette.sourceAssetId,
    sceneVignetteState.sceneVignette.sourceArchiveIndex,
    sceneVignetteState.sceneVignette.launchRowSelector,
    sceneVignetteState.sceneVignette.slot,
    sceneVignetteState.sceneVignette.activeSlotByte,
    sceneVignetteState.sceneVignette.translateX,
    sceneVignetteState.sceneVignette.translateY,
    sceneVignetteState.sceneVignette.ignoredPayload,
    sceneVignetteState.sceneVignette.scaleXPercent,
    sceneVignetteState.sceneVignette.scaleYPercent,
    sceneVignetteState.sceneVignette.alphaCap,
    sceneVignetteState.sceneVignette.transitionAlphaByte,
    sceneVignetteState.sceneVignette.orientationFlags,
    sceneVignetteState.sceneVignette.orientationBit08,
    sceneVignetteState.sceneVignette.evidenceStatus,
  ], [
    'archive:121', 121, 3, 2, 2, -102, 41, 0, 43, 43, 130, 130, 4, false,
    'native-static-exact'
  ],
  'the vignette must retain its exact launch image and all eight opcode operands');
  assert.deepStrictEqual(sceneVignetteState.sceneVignette.baseRotationDegrees,
    { x: 0, y: -90, z: 5 },
    'orientation bit 0x08 must remain separate from the class-row media selector');
  const animatedSpriteCreationState = resynchronizedBranchRuntime.states[124];
  const animatedSpriteOne = animatedSpriteCreationState.effects.find(row =>
    row.slot === 1 && row.payload && row.payload.nativeEffectSlot === 1);
  assert(animatedSpriteOne,
    'opcode 0x63 must materialize its native animated scene-sprite slot');
  assert.deepStrictEqual([
    animatedSpriteOne.bank, animatedSpriteOne.animationKey,
    animatedSpriteOne.stateIndex, animatedSpriteOne.displayedFrameToken,
    animatedSpriteOne.programCursor, animatedSpriteOne.currentOpcode,
    animatedSpriteOne.delay, animatedSpriteOne.positionX,
    animatedSpriteOne.positionY, animatedSpriteOne.payload.stageX,
    animatedSpriteOne.payload.stageY, animatedSpriteOne.rotationDegrees,
  ], [56, 500, 20, 15, 0, 1, 6, 16, 42, 176, 162, 180],
  'opcode 0x63 must use the native constructor, pose record, Y sign, and rotation route');
  const animatedSpriteDelayState = resynchronizedBranchRuntime.states[125];
  const delayedSpriteOne = animatedSpriteDelayState.effects.find(row => row.slot === 1);
  assert.strictEqual(delayedSpriteOne.delay, 4,
    'the native animated scene-sprite updater must subtract two pose ticks per update');
  const animatedSpriteRestartState = resynchronizedBranchRuntime.states[157];
  const restartedSpriteFive = animatedSpriteRestartState.effects.find(row => row.slot === 5);
  assert.deepStrictEqual([
    restartedSpriteFive.payload.nativeOpcode, restartedSpriteFive.animationKey,
    restartedSpriteFive.stateIndex, restartedSpriteFive.displayedFrameToken,
  ], ['0x64', 601, 25, 19],
  'opcode 0x64 must preserve the slot while resetting and immediately advancing its native program');
  const oversizedPanStart = resynchronizedBranchRuntime.trace.find(row =>
    row.kind === 'oversized-image-transition-start' &&
      row.nodeId === 'node:01F440B2:w01A5');
  assert.deepStrictEqual([
    oversizedPanStart.tick, oversizedPanStart.currentPositionStart,
    oversizedPanStart.rateStartX, oversizedPanStart.rateStartY,
    oversizedPanStart.targetX, oversizedPanStart.targetY,
    oversizedPanStart.rateX, oversizedPanStart.rateY,
    oversizedPanStart.duration, oversizedPanStart.zoomDirection,
  ], [157, false, 0, 0, 0, 30, 0, Math.fround(30 / 45), 45, 0],
  'opcode 0x76 must derive the native pan rates without snapping the image view to its declared start');
  assert.deepStrictEqual([
    resynchronizedBranchRuntime.states[157].oversizedImageView.y,
    resynchronizedBranchRuntime.states[157].oversizedImageTransition.progress,
    resynchronizedBranchRuntime.states[202].oversizedImageTransition.progress,
    resynchronizedBranchRuntime.states[203].oversizedImageTransition,
  ], [0, 0, 45, null],
  'the scripted image job must perform 45 updates and release on the following scene update');
  assert(Math.abs(resynchronizedBranchRuntime.states[202].oversizedImageView.y -
    29.999988555908203) < 1e-9,
  'the oversized-image view must preserve native single-precision accumulation');
  assert(!resynchronizedBranchRuntime.assumptions.some(message =>
    /scripted oversized-image/i.test(message)),
  'opcode 0x77 must read the modeled job pointer instead of assuming external completion');
  const resynchronizedScan = resynchronizedBranchRuntime.trace.find(row =>
    row.kind === 'branch-query' && row.nodeId === 'node:01F440B2:w0437');
  assert(resynchronizedScan, 'the grammar-owned primitive branch query must execute');
  assert.deepStrictEqual([
    resynchronizedScan.passed, resynchronizedScan.resynchronization,
    resynchronizedScan.destinationNodeId, resynchronizedScan.resumeNodeId,
  ], [false, true, 'node:01F440B2:w0440', 'node:01F440B2:w0441'],
  'opcode 0x56 must make a failed query scan directly to the next barrier');
  assert(!resynchronizedBranchRuntime.executedNodeIds.includes('node:01F440B2:w043B'),
    'the command between a failed resynchronized query and its barrier must be skipped');

  const cursorReplacementRuntime = await compileScene('rom-director:01F89BB2');
  const cursorReplacement = cursorReplacementRuntime.trace.find(row =>
    row.kind === 'cursor-replacement' && row.sourceNodeId === 'node:01F89BB2:w0084');
  assert(cursorReplacement, 'opcode 0x59 must publish its native cursor replacement');
  assert.deepStrictEqual([
    cursorReplacement.marker, cursorReplacement.destinationNodeId,
    cursorReplacement.destinationWord,
  ], [1, 'node:01F89BB2:w0087', 0x0087]);
  assert.strictEqual(cursorReplacementRuntime.terminated, true,
    'the forward label jump must bypass the inactive branch and reach the terminal hold');
  assert(cursorReplacementRuntime.executedNodeIds.includes('node:01F89BB2:w0087'),
    'the runtime must resume on the exact native label opcode selected by 0x59');

  const reloadHandoffOne = await compileScene(
    'rom-director:01F4CEB0', { maxTicks: 500 });
  const reloadHandoffTwo = await compileScene(
    'rom-director:01FAA648', { maxTicks: 500 });
  [reloadHandoffOne, reloadHandoffTwo].forEach(reloadRuntime => {
    assert.strictEqual(reloadRuntime.terminated, true,
      'opcode 0x39 operand zero must hand the old presentation to the native reload owner');
    assert.strictEqual(reloadRuntime.safetyLimited, false,
      'a native presentation reload must not re-enter the exhausted Director stream');
    assert.strictEqual(reloadRuntime.terminationReason, 'presentation-reload-handoff');
    const reloadTrace = reloadRuntime.trace.find(row =>
      row.kind === 'presentation-reload-handoff');
    assert(reloadTrace, 'the runtime must publish the presentation reload lifecycle boundary');
    assert.strictEqual(reloadTrace.requestCode, 0xD7,
      'the lifecycle handoff must retain the native presentation request byte');
    assert.strictEqual(reloadRuntime.states.at(-1).runtime.presentationLifecycleRequest, 0xD7);
  });

  const lifecycleExitRuntime = await compileScene(
    'rom-director:01F450D0', { maxTicks: 60 });
  const continuationCall = lifecycleExitRuntime.trace.find(row =>
    row.kind === 'director-substream-call');
  const continuationReturn = lifecycleExitRuntime.trace.find(row =>
    row.kind === 'director-substream-return');
  assert.deepStrictEqual([
    continuationCall.tick, continuationCall.selector,
    continuationCall.parentAssetId, continuationCall.childAssetId,
    continuationCall.destinationNodeId, continuationCall.destinationWord,
  ], [0, 1, 'rom-director:01F450D0', 'director-continuation:1',
    'node:1:w0000', 0],
  'opcode 0x99 and its marked bridge must enter continuation row 1 at word zero');
  assert.deepStrictEqual([
    continuationReturn.tick, continuationReturn.childAssetId,
    continuationReturn.destinationAssetId,
    continuationReturn.destinationNodeId, continuationReturn.destinationWord,
  ], [51, 'director-continuation:1', 'rom-director:01F450D0',
    'node:01F450D0:w0007', 7],
  'opcode 0x9A and its bridge must restore the exact marked parent cursor');
  assert.deepStrictEqual([
    lifecycleExitRuntime.states[35].screenTransition.progress,
    lifecycleExitRuntime.states[35].screenTransition.currentFirst,
    lifecycleExitRuntime.states[35].screenTransition.currentSecond,
    lifecycleExitRuntime.states[35].screenTransition.duration,
    lifecycleExitRuntime.states[35].screenTransition.persistence,
    lifecycleExitRuntime.states[35].screenTransition.presentationKind,
  ], [0, 0, 0, 15, 0, 'cutscene-crop'],
  'the second continuation transition must begin from its exact authored crop edges');
  assert.deepStrictEqual([
    lifecycleExitRuntime.states[49].screenTransition.progress,
    lifecycleExitRuntime.states[49].screenTransition.currentFirst,
    lifecycleExitRuntime.states[49].screenTransition.currentSecond,
  ], [14, 22, 22],
  'screen crop interpolation must use native integer progress over fifteen updates');
  assert.strictEqual(lifecycleExitRuntime.states[50].screenTransition, null,
    'zero persistence must release the completed screen-transition record');
  const lifecycleExit = lifecycleExitRuntime.trace.find(row =>
    row.kind === 'presentation-lifecycle-switch');
  assert(lifecycleExit,
    'opcode 0x39 operand one must publish the alternate-scheduler exit operation');
  assert.strictEqual(lifecycleExit.tick, 52,
    'the parent stream must resume one Director update after the child return bridge');
  assert.strictEqual(lifecycleExit.operand, 1);
  assert.strictEqual(lifecycleExitRuntime.states.at(-1).runtime.presentationLifecycleRequest, 0);
  assert.strictEqual(lifecycleExitRuntime.states.at(-1).runtime.alternateDirectorScheduling, false);

  const primaryContinuationRuntime = await compileScene(
    'rom-director:01F3EC88', { maxTicks: 600 });
  const primaryContinuationCall = primaryContinuationRuntime.trace.find(row =>
    row.kind === 'director-substream-call');
  const primaryContinuationReturn = primaryContinuationRuntime.trace.find(row =>
    row.kind === 'director-substream-return');
  assert.deepStrictEqual([
    primaryContinuationCall.tick, primaryContinuationCall.selector,
    primaryContinuationReturn.tick, primaryContinuationReturn.destinationWord,
  ], [417, 0, 474, 165],
  'continuation row zero must execute and restore its exact parent cursor');
  assert.strictEqual(primaryContinuationRuntime.states[426].cameraState.activeClipId,
    'node:0:w000F',
    'opcode 0x3D must start the shared eighteen-update identity transition');
  assert.strictEqual(primaryContinuationRuntime.states[426].cameraState.translateY, -10);
  assert.deepStrictEqual([
    primaryContinuationRuntime.states[444].cameraState.translateX,
    primaryContinuationRuntime.states[444].cameraState.translateY,
    primaryContinuationRuntime.states[444].cameraState.scaleX,
    primaryContinuationRuntime.states[444].cameraState.scaleY,
    primaryContinuationRuntime.states[444].cameraState.activeClipId,
  ], [0, 0, 1, 1, null],
  'the unguarded 0x3E query must release only after the projection reaches identity');
  assert.deepStrictEqual([
    primaryContinuationRuntime.states[444].screenTransition.authoredInitialFirst,
    primaryContinuationRuntime.states[444].screenTransition.authoredInitialSecond,
    primaryContinuationRuntime.states[444].screenTransition.initialFirst,
    primaryContinuationRuntime.states[444].screenTransition.initialSecond,
    primaryContinuationRuntime.states[444].screenTransition.finalFirst,
    primaryContinuationRuntime.states[444].screenTransition.duration,
    primaryContinuationRuntime.states[444].screenTransition.persistence,
  ], [-1, -1, 24, 24, 0, 15, 1],
  'the main cutscene continuation must expand the native -1 edge preset to 24 pixels');
  assert(primaryContinuationRuntime.missingInputs.some(message =>
    message.includes('Actor-presentation bootstrap')),
  'opcode 0x3F must expose its external persistent-character inputs instead of inventing Actors');
  assert(primaryContinuationRuntime.assumptions.some(message =>
    message.includes('Actor-presentation lifecycle input')),
  'the deterministic preview must label its completed-state assumption for query 0x40');

  const longBoundsRuntime = await compileScene('rom-director:01F440B2', { maxTicks: 17000 });
  const longBoundsActorSamples = longBoundsRuntime.states.reduce((total, runtimeState) =>
    total + runtimeState.actors.filter(actor => actor.visible).length, 0);
  assert(longBoundsActorSamples > 125000,
    'the bounds regression fixture must exceed JavaScript variadic argument limits');
  assert.strictEqual(longBoundsRuntime.states.length, 17000,
    'large runtimes must finish their streaming Actor-bounds pass without a call-stack overflow');

  const transformResource = OB64.cutsceneRuntime.decodeSceneTransformResource(z64, 0);
  assert.strictEqual(transformResource.keyframeCount, 4);
  assert.strictEqual(transformResource.groupCount, 5);
  assert.deepStrictEqual(transformResource.groups[0].keyframeIds, [0, 1]);
  assert.strictEqual(transformResource.keyframes[0][0].translateX, 50);
  assert.strictEqual(transformResource.keyframes[1][0].translateX, -40);

  const modeZeroScene = catalog.getScene('rom-director:01F5F81C');
  const modeZeroSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, modeZeroScene, { hashBytes });
  const modeZeroProjected = OB64.cutsceneCodec.projectSceneDocument(
    modeZeroScene, modeZeroSource, catalog);
  const modeZeroRuntime = OB64.cutsceneRuntime.compile(
    modeZeroProjected.document, modeZeroProjected.program, modeZeroScene, catalog, { z64 });
  assert.strictEqual(modeZeroRuntime.directorMode, 0);
  assert.strictEqual(modeZeroRuntime.states[0].actorProjection.eye.x, 360,
    'mode-zero Actors must use the native Actor-side camera bank');
  assert.strictEqual(modeZeroRuntime.states[0].actorProjection.eye.z, 0);
  assert.strictEqual(modeZeroRuntime.states[0].registeredProjection.eye.z, 360,
    'mode-zero snapshots must retain the registered-object camera bank');
  const staticModeZeroScene = catalog.getScene('rom-director:01F504FA');
  const staticModeZeroSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, staticModeZeroScene, { hashBytes });
  const staticModeZeroProjected = OB64.cutsceneCodec.projectSceneDocument(
    staticModeZeroScene, staticModeZeroSource, catalog);
  const staticModeZeroRuntime = OB64.cutsceneRuntime.compile(
    staticModeZeroProjected.document, staticModeZeroProjected.program,
    staticModeZeroScene, catalog, { z64 });
  assert.deepStrictEqual(staticModeZeroProjected.document.background.layers.map(layer =>
    layer.assetId), ['scene-resource:018207D0'],
  'a structurally proven mode-zero stream must expose its command-owned scene group');
  assert.strictEqual(staticModeZeroScene.launchProfile.background.requests[0].selector, 14);
  assert.deepStrictEqual(staticModeZeroRuntime.states[0].background.layers.map(layer => ({
    assetId: layer.assetId,
    nativeOrdinal: layer.nativeOrdinal,
    pipeline: layer.renderPipeline,
  })), [{
    assetId: 'scene-resource:018207D0',
    nativeOrdinal: 0,
    pipeline: 'mode-zero-b5-actor-camera',
  }], 'the exact command-selected mode-zero group must reach the Stage without launch guessing');
  const flashbackScene = catalog.getScene('rom-custom-lz:01F3E836');
  const flashbackSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, flashbackScene, { hashBytes });
  const flashbackProjected = OB64.cutsceneCodec.projectSceneDocument(
    flashbackScene, flashbackSource, catalog);
  const flashbackRuntime = OB64.cutsceneRuntime.compile(
    flashbackProjected.document, flashbackProjected.program, flashbackScene, catalog, { z64 });
  const flashbackState = flashbackRuntime.states[0];
  assert.deepStrictEqual(flashbackState.background.layers.map(layer => layer.nativeOrdinal),
    [0, 1], 'Flashback B5 layers must retain their native traversal ordinals');
  assert.deepStrictEqual(flashbackState.background.projection.viewport,
    { left: 0, top: 23, width: 320, height: 191 },
  'the Flashback courtyard must use the stored native cutscene viewport');
  flashbackState.background.layers.forEach(layer => {
    assert.strictEqual(layer.transformChannel, layer.nativeOrdinal);
    assert.strictEqual(layer.renderPipeline, 'mode-zero-b5-actor-camera');
    assert.deepStrictEqual(layer.sceneTransform,
      flashbackState.transformChannels[layer.nativeOrdinal],
      'each runtime background must publish its matching Director transform channel');
  });
  const flashbackActor = flashbackState.actors.find(actor => actor.slot === 0);
  assert.strictEqual(flashbackActor.x, flashbackActor.baseX,
    'mode-zero scene transforms must not be added directly to Director world coordinates');
  assert.strictEqual(flashbackActor.y,
    flashbackActor.baseY + flashbackActor.secondaryY);
  assert.strictEqual(flashbackActor.renderPipeline,
    'mode-zero-registered-prepass-actor-camera');
  const flashbackGeometry = OB64.cutsceneRenderer.modeZeroActorGeometry(
    flashbackActor, flashbackState, flashbackState.actorProjection);
  assert(flashbackGeometry,
    'Flashback Actor 0 must compose the registered prepass into the Actor camera');
  assert.deepStrictEqual(flashbackGeometry.sourcePoint, { x: 100, y: 0, z: 100 });
  assert(Math.abs(flashbackGeometry.registeredScreenPoint.x - 7.518223464325445) < 0.000001);
  assert(Math.abs(flashbackGeometry.registeredScreenPoint.y + 30.648569679729917) < 0.000001,
    'the registered prepass must convert canvas Y to the native bottom-origin value');
  assert(Math.abs(flashbackGeometry.scenePoint.x + 39.53926786046515) < 0.000001);
  assert(Math.abs(flashbackGeometry.scenePoint.y + 59.930617697572934) < 0.000001);
  assert(Math.abs(flashbackGeometry.scenePoint.z + 67.79168693947906) < 0.000001);
  assert.deepStrictEqual(flashbackGeometry.screenPoint, { x: 58, y: 275 });
  assert(Math.abs(flashbackGeometry.actorPlaneScale - 0.5362278943643554) < 0.000001,
    'mode-zero scale must retain both native record-scale applications');
  assert(Math.abs(flashbackGeometry.scale - 1.3849458860045847) < 0.000001);
  const flashbackActorOne = flashbackState.actors.find(actor => actor.slot === 1);
  const flashbackGeometryOne = OB64.cutsceneRenderer.modeZeroActorGeometry(
    flashbackActorOne, flashbackState, flashbackState.actorProjection);
  assert.deepStrictEqual(flashbackGeometryOne.screenPoint, { x: 159, y: 167 },
    'the visible retail Flashback Actor must land on its registered-camera anchor');
  assert(Math.abs(flashbackGeometryOne.scenePoint.x + 0.4583602088705196) < 0.000001);
  assert(Math.abs(flashbackGeometryOne.scenePoint.y + 18.345455332828337) < 0.000001);
  assert(Math.abs(flashbackGeometryOne.actorPlaneScale - 0.3803203929234068) < 0.000001);
  assert(Math.abs(flashbackGeometryOne.scale - 0.9822748295615937) < 0.000001);
  const alteredRegisteredState = Object.assign({}, flashbackState, {
    registeredProjection: Object.assign({}, flashbackState.registeredProjection, {
      eye: { x: 9000, y: 7000, z: -5000 }
    })
  });
  assert.notDeepStrictEqual(
    OB64.cutsceneRenderer.modeZeroActorGeometry(
      flashbackActor, alteredRegisteredState, flashbackState.actorProjection),
    flashbackGeometry,
    'registered-object camera changes must affect the native Actor prepass');

  const preScannedModeTwoScene = catalog.getScene('rom-director:01F53488');
  const preScannedModeTwoSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, preScannedModeTwoScene, { hashBytes });
  const preScannedModeTwoProjected = OB64.cutsceneCodec.projectSceneDocument(
    preScannedModeTwoScene, preScannedModeTwoSource, catalog);
  const preScannedModeTwoRuntime = OB64.cutsceneRuntime.compile(
    preScannedModeTwoProjected.document, preScannedModeTwoProjected.program,
    preScannedModeTwoScene, catalog, { z64 });
  const preScannedRequest = preScannedModeTwoScene.launchProfile.background.requests[0];
  const preScannedModeTwoState = preScannedModeTwoRuntime.states.find(state =>
    /native launch pre-scan/.test(state.background.runtimeStatus));
  assert(preScannedModeTwoState,
    'the runtime must present the command-seeded mode-two environment');
  assert.strictEqual(preScannedModeTwoRuntime.directorMode, 2);
  assert.strictEqual(preScannedRequest.commandOperand, 20);
  assert.strictEqual(preScannedRequest.selector, 20);
  assert.strictEqual(preScannedRequest.environmentSelector, 20);
  assert.strictEqual(preScannedRequest.foregroundSelector, 20);
  assert.strictEqual(preScannedRequest.selectorSource,
    'director-launch-prescan-command-operand');
  assert.deepStrictEqual(preScannedModeTwoState.background.layers.map(layer =>
    [layer.assetId, layer.role]), [
    ['section-c-njpg:20', 'environment-base'],
    ['archive:32', 'foreground-mask']
  ]);
  assert.strictEqual(preScannedModeTwoState.background.selectorTableId,
    'background-table:mode2-environment:80');
  assert(!preScannedModeTwoRuntime.missingInputs.some(message =>
    /external flag bit 0x08/.test(message)));
  assert(!preScannedModeTwoRuntime.missingInputs.some(message =>
    /no resolved B5 crop/.test(message)));
  assert.deepStrictEqual(preScannedModeTwoRuntime.launchStageTransform.initial,
    { translateX: 0, translateY: 0, scaleX: 1, scaleY: 1 });
  assert.strictEqual(preScannedModeTwoRuntime.launchStageTransform.evidenceStatus,
    'native-static-mode-two-stage-initializers');
  assert.deepStrictEqual(preScannedModeTwoRuntime.launchStageTransform.ownerFunctionsZ64,
    ['0x001FFA8C', '0x001FFAD0', '0x001FB32C']);
  assert(!preScannedModeTwoRuntime.assumptions.some(message => /mirror/i.test(message)));
  assert.strictEqual(preScannedModeTwoScene.launchProfile.cameras.actor.evidenceStatus,
    'native-static',
  'the native overlay initializer must supply the common mode-two Actor camera');
  assert.strictEqual(preScannedModeTwoScene.launchProfile.cameras.actor.kind,
    'mode-two-overlay-initializer');
  assert.strictEqual(preScannedModeTwoScene.launchProfile.cameras.actor.projection.modelScale,
    0.10000000149011612);
  assert.strictEqual(preScannedModeTwoScene.launchProfile.cameras.actor.projection.fovYDegrees,
    12.880000114440918);
  assert.strictEqual(preScannedModeTwoRuntime.states[0].actorProjection.evidenceStatus,
    'native-static');
  const classOwnedSceneGroupScene = catalog.getScene('rom-director:01F6864C');
  const classOwnedSceneGroupSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, classOwnedSceneGroupScene, { hashBytes });
  const classOwnedSceneGroupProjected = OB64.cutsceneCodec.projectSceneDocument(
    classOwnedSceneGroupScene, classOwnedSceneGroupSource, catalog);
  const classOwnedSceneGroupRuntime = OB64.cutsceneRuntime.compile(
    classOwnedSceneGroupProjected.document, classOwnedSceneGroupProjected.program,
    classOwnedSceneGroupScene, catalog, { z64, maxTicks: 2 });
  assert.strictEqual(classOwnedSceneGroupRuntime.directorMode, 0);
  assert.strictEqual(classOwnedSceneGroupScene.launchProfile.directorMode.evidenceStatus,
    'native-static-launch-class');
  assert.deepStrictEqual(classOwnedSceneGroupRuntime.states[0].background.layers.map(layer =>
    layer.assetId), ['scene-resource:017F4B30', 'scene-resource:017FD27C'],
  'terminal class 5 must activate its preloaded scene group, not selector 11 from the mode-two environment table');
  const launchContextDocument = JSON.parse(JSON.stringify(preScannedModeTwoProjected.document));
  launchContextDocument.background.projection.launchContext = {
    mode: 2,
    override: true,
    environmentSelector: 57,
    foregroundSelector: 57,
    evidenceStatus: 'user-supplied-launch-context'
  };
  const launchContextRuntime = OB64.cutsceneRuntime.compile(
    launchContextDocument, preScannedModeTwoProjected.program,
    preScannedModeTwoScene, catalog, { z64 });
  const launchContextState = launchContextRuntime.states[0];
  assert.strictEqual(launchContextState.background.environmentSelector, 57);
  assert.strictEqual(launchContextState.background.foregroundSelector, 57);
  assert(launchContextState.background.layers.some(layer =>
    layer.assetId === 'mode2-environment:00236B58' &&
    layer.role === 'environment-base'));
  assert(launchContextState.background.layers.some(layer =>
    layer.assetId === 'archive:49' && layer.role === 'foreground-mask'));
  assert.strictEqual(
    launchContextState.background.projection.nativeSceneProps.foregroundSelector, 57);
  assert.strictEqual(
    launchContextState.background.projection.nativeSceneProps.orthographicPlacements.length, 11);
  assert.strictEqual(
    launchContextState.background.projection.nativeSceneProps.perspectivePlacements.length, 1);
  assert(!launchContextRuntime.missingInputs.some(message =>
    /no resolved B5 crop/.test(message)));
  const unresolvedScaleState = preScannedModeTwoRuntime.states.find(state => state.frame === 144);
  const unresolvedScaleFrames = OB64.cutsceneSprites.framesForPreview(
    spriteState, unresolvedScaleState);
  const unresolvedScaleRender = OB64.cutsceneRenderer.renderFrame(
    preScannedModeTwoProjected.document, unresolvedScaleState, {
      backgrounds: [],
      backgroundProjection: unresolvedScaleState.background.projection,
      projection: unresolvedScaleState.actorProjection,
      actorFrames: unresolvedScaleFrames,
      camera: unresolvedScaleState.cameraState,
      overlays: []
    });
  assert.strictEqual(unresolvedScaleRender.projection.mode, 'native-perspective-runtime');
  assert.strictEqual(unresolvedScaleRender.projection.evidenceStatus, 'native-static');
  assert.strictEqual(unresolvedScaleRender.hitRegions.length, 2);
  assert(unresolvedScaleRender.hitRegions.every(region =>
    region.right - region.left >= 20 && region.bottom - region.top >= 35),
  'the exact common mode-two camera must keep valid native Actor sprites visible');

  const crowdedScaleScene = catalog.getScene('rom-director:01F581EA');
  const crowdedScaleSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, crowdedScaleScene, { hashBytes });
  const crowdedScaleProjected = OB64.cutsceneCodec.projectSceneDocument(
    crowdedScaleScene, crowdedScaleSource, catalog);
  const crowdedScaleRuntime = OB64.cutsceneRuntime.compile(
    crowdedScaleProjected.document, crowdedScaleProjected.program,
    crowdedScaleScene, catalog, { z64 });
  const crowdedScaleState = crowdedScaleRuntime.states.find(state => state.frame === 237);
  assert.strictEqual(crowdedScaleState.background.environmentSelector, 39);
  assert.strictEqual(crowdedScaleState.background.foregroundSelector, 39);
  assert.deepStrictEqual(crowdedScaleState.background.layers.map(layer =>
    [layer.assetId, layer.role]), [['archive:6', 'environment-base']],
  'the seven-Actor scene must receive its launch-pre-scanned environment');
  const crowdedScaleFrames = OB64.cutsceneSprites.framesForPreview(spriteState, crowdedScaleState);
  const crowdedScaleRender = OB64.cutsceneRenderer.renderFrame(
    crowdedScaleProjected.document, crowdedScaleState, {
      backgrounds: [],
      backgroundProjection: crowdedScaleState.background.projection,
      projection: crowdedScaleState.actorProjection,
      actorFrames: crowdedScaleFrames,
      camera: crowdedScaleState.cameraState,
      overlays: []
    });
  assert.strictEqual(crowdedScaleRender.projection.mode, 'native-perspective-runtime');
  assert.strictEqual(crowdedScaleRender.hitRegions.length, 7);
  assert(crowdedScaleRender.hitRegions.every(region =>
    region.right - region.left >= 20 && region.bottom - region.top >= 30),
  'the seven-Actor scene must keep decoded sprite art at a readable preview scale');
  assert(crowdedScaleRuntime.states.every(state =>
    state.actorProjection.evidenceStatus === 'native-static'),
  'the common mode-two Actor camera must remain stable while scrubbing');
  const crowdedLeftEdges = crowdedScaleRender.hitRegions.map(region => region.left);
  assert(Math.max.apply(null, crowdedLeftEdges) - Math.min.apply(null, crowdedLeftEdges) >= 200,
  'moving Actors must retain their authored X separation instead of clamping to one edge');

  const inferredModeZeroScene = catalog.getScene('rom-director:01F849CA');
  const inferredModeZeroSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, inferredModeZeroScene, { hashBytes });
  const inferredModeZeroProjected = OB64.cutsceneCodec.projectSceneDocument(
    inferredModeZeroScene, inferredModeZeroSource, catalog);
  const inferredModeZeroRuntime = OB64.cutsceneRuntime.compile(
    inferredModeZeroProjected.document, inferredModeZeroProjected.program,
    inferredModeZeroScene, catalog, { z64 });
  const inferredModeZeroState = inferredModeZeroRuntime.states.find(state =>
    state.background.selectorTableId === 'background-table:scene:31');
  assert(inferredModeZeroState);
  assert.strictEqual(inferredModeZeroRuntime.directorMode, 0);
  assert.strictEqual(inferredModeZeroState.background.selector, 5);
  assert.deepStrictEqual(inferredModeZeroState.background.layers.map(layer => layer.assetId),
    ['archive:97', 'archive:98', 'scene-resource:0172D0F6']);
  assert.deepStrictEqual(inferredModeZeroState.background.layers.map(layer => layer.role),
    ['environment-base', 'ordered-layer', 'ordered-layer'],
  'runtime scene-group tiles must retain source order unless a capture identifies a mask');
  assert(!fs.readFileSync(path.join(EDITOR, 'cutscene-runtime.js'), 'utf8')
    .includes('selector > 30'),
  'Director launch mode must never be guessed from the command operand domain');

  assert.strictEqual(runtime.terminated, true);
  assert.strictEqual(runtime.safetyLimited, false);
  assert(runtime.trace.some(row => row.kind === 'composite'),
    'runtime must publish execution-time composite events for the timeline');
  assert.deepStrictEqual(runtime.states[0].background.layers.map(row => row.assetId),
    ['mode2-environment:00236B58', 'archive:49']);
  assert.deepStrictEqual(runtime.states[0].background.layers.map(row => row.role),
    ['environment-base', 'foreground-mask'],
    'Graduation must draw the hall before Actors and its occlusion pieces afterward');
  assert.strictEqual(runtime.states[0].actorProjection.mode, 'native-perspective-runtime');
  assert.strictEqual(runtime.states[0].background.projection.calibrationCamera.previewFrame, 567);
  assert(Math.abs(runtime.states[0].actorProjection.eye.x - 82.38899993896484) < 1e-9);
  assert(Math.abs(runtime.states[0].actorProjection.fovYDegrees - 12.880000114440918) < 1e-9);

  const formationRuntime = await compileScene('formation-cuutscene');
  assert.deepStrictEqual(formationRuntime.states[0].background.layers.map(row =>
    [row.assetId, row.role]), [
    ['archive:12', 'environment-base'],
    ['archive:45', 'foreground-mask'],
  ], 'Formation must combine its launch-time castle with the Director-selected foreground');
  assert.strictEqual(formationRuntime.states[0].background.projection.scale, 1);
  assert.deepStrictEqual([
    formationRuntime.states[0].background.projection.cropWorldX,
    formationRuntime.states[0].background.projection.cropWorldY,
  ], [95.002, 109.482],
  'Formation must keep the measured native environment-plate registration');
  assert.deepStrictEqual(formationRuntime.states[0].background.projection.calibrationCamera, {
    previewFrame: 250, translateX: 25.416658401489258, translateY: -31.875,
    scaleX: 1, scaleY: 1, countdown: 65,
  }, 'Formation must move its registered background relative to the captured camera state');
  const formationHeightActor = formationRuntime.states[250].actors.find(actor => actor.slot === 0);
  assert(formationHeightActor, 'Formation slot 0 must exist at the stored capture frame');
  assert.deepStrictEqual([
    formationHeightActor.baseY, formationHeightActor.secondaryY,
    formationHeightActor.heightModeByte, formationHeightActor.y,
  ], [27, 27, 0x02, 27],
  'opcode 0x69 mode 2 must render secondary Y alone instead of doubling Actor height');

  const travelingTwoRuntime = await compileScene('traveling-cutscene-2');
  assert.deepStrictEqual(travelingTwoRuntime.states[0].background.layers.map(row =>
    [row.assetId, row.role]), [['archive:15', 'environment-base']],
  'Traveling 2 must restore its launch-time forest-camp environment');
  assert.deepStrictEqual(travelingTwoRuntime.states[0].background.projection.viewport,
    { left: 0, top: 23, width: 320, height: 191 });
  const travelingTwoActorState = travelingTwoRuntime.states.find(state =>
    state.actors.some(actor => actor.visible && actor.renderModeByte === 2));
  assert(travelingTwoActorState,
    'Traveling 2 must reach an Actor using its native opacity-bypass render pass');
  const travelingTwoActor = travelingTwoActorState.actors.find(actor =>
    actor.visible && actor.renderModeByte === 2);
  assert.strictEqual(travelingTwoActor.renderModeByte, 2,
    'opcode 0x14 must preserve the Actor render-pass selector');

  const travelingFourScene = catalog.getScene('traveling-cutscene-4');
  const travelingFourSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, travelingFourScene, { hashBytes });
  const travelingFourProjected = OB64.cutsceneCodec.projectSceneDocument(
    travelingFourScene, travelingFourSource, catalog);
  const travelingFourRuntime = OB64.cutsceneRuntime.compile(
    travelingFourProjected.document, travelingFourProjected.program,
    travelingFourScene, catalog, { z64 });
  const travelingFourBackground = travelingFourRuntime.states.find(state =>
    state.background.selector === 62);
  assert(travelingFourBackground);
  assert.deepStrictEqual(travelingFourBackground.background.layers.map(row =>
    [row.assetId, row.role]), [
    ['mode2-environment:0027C5F4', 'environment-base'],
    ['archive:51', 'foreground-mask'],
  ], 'Traveling 4 must combine its complete resource-4 base with the castle-wall foreground');

  const firstTravelingFourCreate = travelingFourProjected.program.primitives.find(node =>
    node.name === 'actor_create' && node.rawWords[1] === 0);
  const travelingFourTemplate = travelingFourProjected.document.actors.find(actor =>
    actor.slot === 0);
  assert(firstTravelingFourCreate && travelingFourTemplate);
  assert.notStrictEqual(travelingFourTemplate.source.placeNodeId,
    firstTravelingFourCreate.id,
    'the regression scene must reuse slot 0 with a later editor-owned placement');
  const isolatedPrimitiveById = {};
  isolatedPrimitiveById[firstTravelingFourCreate.id] = firstTravelingFourCreate;
  const isolatedCreateProgram = {
    assetId: travelingFourProjected.program.assetId,
    primitives: [firstTravelingFourCreate],
    primitiveById: isolatedPrimitiveById,
    composites: [{
      id: 'test:first-traveling-four-create',
      kind: 'single-primitive',
      label: 'First Traveling 4 Actor creation',
      category: 'actor',
      nodeIds: [firstTravelingFourCreate.id]
    }],
    compositeById: {}, compositeByNodeId: {}, lanes: [], stats: {}
  };
  const isolatedCreateRuntime = OB64.cutsceneRuntime.compile(
    travelingFourProjected.document, isolatedCreateProgram,
    travelingFourScene, catalog, { z64 });
  const isolatedActor = isolatedCreateRuntime.states[0].actors.find(actor =>
    actor.slot === 0);
  assert.deepStrictEqual([
    isolatedActor.bank, isolatedActor.animationKey,
    isolatedActor.nativeFacing, isolatedActor.variantSelector,
  ], [30, 0, 2, 4],
  'an earlier creation must use its own native pose instead of the later editor node pose');

  const openingScene = catalog.getScene('opening-title-cutscene');
  const openingRuntime = await compileScene('opening-title-cutscene');
  assert.strictEqual(
    openingScene.backgroundRuntimeObservation.stageProjection.initialPreviewFrame, 46,
    'the editor must open the title sequence after its first native fade reveals ATLUS');
  assert.deepStrictEqual(openingRuntime.states[1].background.layers.map(row =>
    row.sceneTransform.translateY), [-230, 17, -130, -130, -130],
  'the first title state must hide the tapestry and later cards offscreen');
  assert.deepStrictEqual(openingRuntime.states[111].background.layers.map(row =>
    row.sceneTransform.translateY), [-230, -130, 20, -130, -130],
  'the second title transform must replace ATLUS with the Quest/Nintendo card');
  assert.deepStrictEqual(openingRuntime.states[178].background.layers.map(row =>
    row.sceneTransform.translateY), [-230, -130, -130, 17, -130],
  'the third title transform must present only the episode card');
  assert.deepStrictEqual(openingRuntime.states[313].background.layers.map(row =>
    row.sceneTransform.translateY), [-20, -130, -130, -130, 12],
  'the final title transform must reveal the tapestry and subtitle together');

  const moveTrace = runtime.trace.find(row => row.name === 'actor_move');
  assert(moveTrace, 'Graduation must execute its Magnus movement command');
  const moveNode = projected.program.primitiveById[moveTrace.nodeId];
  const slot = moveNode.rawWords[1] | 0;
  const atStart = runtime.states[moveTrace.tick].actors.find(row => row.slot === slot);
  const afterOneUpdate = runtime.states[moveTrace.tick + 1].actors.find(row => row.slot === slot);
  assert.strictEqual(atStart.x, 350,
    'movement creation must not integrate on the command scheduler call');
  assert(afterOneUpdate.x < atStart.x,
    'the following scheduler call must apply the native movement velocity');
  assert.strictEqual(afterOneUpdate.movementFrame, 1);
  assert.strictEqual(afterOneUpdate.poseFrame - atStart.poseFrame, 2,
    'normal pose playback must consume two pose-duration units per scheduler update');
  assert.strictEqual(atStart.poseLoop, true,
    'the native 0x04 pose control must keep the walking animation cycling');
  const firstSprite = OB64.cutsceneSprites.frameForActor(spriteState, atStart);
  const laterActor = runtime.states[moveTrace.tick + 4].actors.find(row => row.slot === slot);
  const laterSprite = OB64.cutsceneSprites.frameForActor(spriteState, laterActor);
  assert(firstSprite && laterSprite, 'Magnus pose states must resolve to native sprite frames: ' +
    JSON.stringify(spriteState.errors));
  assert.notStrictEqual(hashBytes(firstSprite.rgba), hashBytes(laterSprite.rgba),
    'advancing the native pose clock must display a different walk-cycle frame');

  const launchRosterScene = catalog.scenes.find(row =>
    row.sceneId === 'scene:director:01f56d8e');
  const launchRosterSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, launchRosterScene, { hashBytes });
  const launchRosterProjected = OB64.cutsceneCodec.projectSceneDocument(
    launchRosterScene, launchRosterSource, catalog);
  const standaloneLaunchRosterRuntime = OB64.cutsceneRuntime.compile(
    launchRosterProjected.document, launchRosterProjected.program,
    launchRosterScene, catalog, { z64, maxTicks: 100 });
  assert(!standaloneLaunchRosterRuntime.states.some(state =>
    state.actors.some(actor => actor.slot === 2)),
    'Actor State must not invent a launch-time Actor record that the stream never produced');
  assert(standaloneLaunchRosterRuntime.missingInputs.some(message =>
    /requires concurrent Director scene state/.test(message)),
  'the standalone runtime must report the exact missing parent-event state owner');

  const concurrentOwnerRuntime = await compileScene(
    'rom-director:01F5646A', { maxTicks: 3000, controllerMask: 0x8000 });
  assert.strictEqual(concurrentOwnerRuntime.durationTicks, 1904);
  assert.strictEqual(concurrentOwnerRuntime.terminated, true);
  const overlayRetries = concurrentOwnerRuntime.trace.filter(row =>
    row.kind === 'branch-query' && row.nodeId === 'node:01F5646A:w011A');
  assert.deepStrictEqual(overlayRetries.map(row => row.actual), [5, 4, 3, 2, 1, 0],
    'a failed local scan must retry the persistent query until its native job completes');
  assert.strictEqual(concurrentOwnerRuntime.trace.filter(row =>
    row.kind === 'parser-resume-commit' &&
      row.sourceNodeId === 'node:01F5646A:w0122').length, 1,
  'only the marker-bearing successful invocation may commit past the query');
  assert.strictEqual(concurrentOwnerRuntime.executedNodeIds.filter(nodeId =>
    nodeId === 'node:01F5646A:w011E').length, 1,
    'failure scanning must not execute the later counter arm');

  const resetModeTwoScene = catalog.getScene('rom-custom-lz:01FA4D0A');
  const resetModeTwoSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, resetModeTwoScene, { hashBytes });
  const resetModeTwoProjected = OB64.cutsceneCodec.projectSceneDocument(
    resetModeTwoScene, resetModeTwoSource, catalog);
  const resetModeTwoContext = resetModeTwoScene.launchProfile.parentEventLaunches[0]
    .eventInvocationContexts[0];
  const resetModeTwoRuntime = OB64.cutsceneRuntime.compile(
    resetModeTwoProjected.document, resetModeTwoProjected.program,
    resetModeTwoScene, catalog, {
      z64,
      contextRuntime: concurrentOwnerRuntime,
      contextTickOffset: resetModeTwoContext.concurrentDirectorTickOffset,
      launchContext: resetModeTwoContext
    });
  assert.strictEqual(resetModeTwoRuntime.launchSceneStatePolicy,
    'mode-two-zero-loader-preview-clears-scene-root');
  assert(resetModeTwoRuntime.assumptions.some(message =>
    message.includes('resource-loader mode 0x8023A981')),
  'the bounded fresh-root preview must disclose its unresolved loader-mode input');
  assert.strictEqual(resetModeTwoRuntime.concurrentContext, null,
    'an explicit mode-two initializer must not import the preceding scene root');
  assert.strictEqual(resetModeTwoRuntime.states[0].background.environmentSelector, 61);
  assert.strictEqual(resetModeTwoRuntime.states[0].actorProjection.fovYDegrees,
    12.880000114440918);
  assert(resetModeTwoRuntime.states[0].transformChannels.every(channel =>
    channel.rotationX === 0 && channel.rotationY === 0 &&
      channel.translateX === 0 && channel.translateY === 0 &&
      channel.translateZ === 0 && channel.uniformScale === 1));
  assert(resetModeTwoRuntime.states[0].actors.filter(actor => actor.visible)
    .every(actor => actor.y === -170),
  'the cleared mode-two scene root must preserve the new stream camera and Actor coordinates');

  const selectedLaunchContext = launchRosterScene.launchProfile.parentEventLaunches[0]
    .eventInvocationContexts[0];
  const contextualLaunchRosterRuntime = OB64.cutsceneRuntime.compile(
    launchRosterProjected.document, launchRosterProjected.program,
    launchRosterScene, catalog, {
      z64,
      controllerMask: 0x8000,
      contextRuntime: concurrentOwnerRuntime,
      contextTickOffset: selectedLaunchContext.concurrentDirectorTickOffset,
      launchContext: selectedLaunchContext
    });
  assert.strictEqual(contextualLaunchRosterRuntime.terminated, true);
  assert.strictEqual(contextualLaunchRosterRuntime.durationTicks, 2139);
  assert.deepStrictEqual(contextualLaunchRosterRuntime.concurrentContext, {
    assetId: 'rom-director:01F5646A',
    tickOffset: 1,
    evidenceStatus: 'native-static-parent-event-request-order'
  });
  const contextualFinalState = contextualLaunchRosterRuntime.states.at(-1);
  assert.strictEqual(contextualFinalState.actors.filter(actor => actor.visible).length, 9);
  assert.deepStrictEqual(contextualFinalState.background.layers.map(layer => layer.assetId), [
    'archive:94',
    'scene-resource:016C90D4',
    'scene-resource:016CA720',
    'scene-resource:016CBCDE',
    'scene-resource:016CC968'
  ]);
  const contextualSlot2 = contextualFinalState.actors.find(actor => actor.slot === 2);
  const contextualSlot4 = contextualFinalState.actors.find(actor => actor.slot === 4);
  assert.deepStrictEqual([
    contextualSlot2.bank, contextualSlot2.animationKey,
    contextualSlot2.nativeFacing, contextualSlot2.variantSelector,
    contextualSlot2.baseX, contextualSlot2.baseY, contextualSlot2.baseZ
  ], [31, 61, 1, 0, -4, 0, 91]);
  assert.deepStrictEqual([
    contextualSlot4.bank, contextualSlot4.animationKey,
    contextualSlot4.nativeFacing, contextualSlot4.variantSelector,
    contextualSlot4.baseX, contextualSlot4.baseY, contextualSlot4.baseZ
  ], [16, 0, 1, 4, -1, 0, 79]);
  assert.strictEqual(contextualLaunchRosterRuntime.missingInputs.length, 0,
    'the exact concurrent launch owner must supply selector 154 without invented inputs');
  const compactConcurrentOwner = OB64.cutsceneRuntime.compactContextRuntime(
    concurrentOwnerRuntime);
  assert.strictEqual(compactConcurrentOwner.contextFrames.length, 1904);
  assert.strictEqual(Object.prototype.hasOwnProperty.call(
    compactConcurrentOwner, 'states'), false,
  'a concurrent owner must not retain its complete presentation snapshots');
  assert(compactConcurrentOwner.contextFrames.some(frame => frame &&
    frame.actors && frame.actors.some(actor => Object.keys(actor).length < 10)),
  'the compact timeline must store field deltas instead of full Actor copies each tick');
  const compactContextualRuntime = OB64.cutsceneRuntime.compile(
    launchRosterProjected.document, launchRosterProjected.program,
    launchRosterScene, catalog, {
      z64,
      controllerMask: 0x8000,
      contextRuntime: compactConcurrentOwner,
      contextTickOffset: selectedLaunchContext.concurrentDirectorTickOffset,
      launchContext: selectedLaunchContext
    });
  assert.strictEqual(compactContextualRuntime.durationTicks,
    contextualLaunchRosterRuntime.durationTicks);
  function firstRuntimeDifference(left, right, pathLabel) {
    if (Object.is(left, right)) return null;
    if (left == null || right == null || typeof left !== 'object' ||
        typeof right !== 'object') {
      return pathLabel + ': ' + JSON.stringify(left) + ' !== ' + JSON.stringify(right);
    }
    const keys = Array.from(new Set(Object.keys(left).concat(Object.keys(right)))).sort();
    for (const key of keys) {
      const difference = firstRuntimeDifference(
        left[key], right[key], pathLabel + '.' + key);
      if (difference) return difference;
    }
    return null;
  }
  [0, 1899, contextualLaunchRosterRuntime.durationTicks - 1].forEach(tick => {
    const difference = firstRuntimeDifference(
      compactContextualRuntime.states[tick],
      contextualLaunchRosterRuntime.states[tick], 'state');
    assert.strictEqual(
      difference, null,
      'context delta replay must preserve the full runtime state at tick ' + tick +
        (difference ? ': ' + difference : ''));
  });

  const materializerRuntime = await compileScene('rom-director:01F81FE8');
  assert(!materializerRuntime.states.some(state => state.actors.some(actor => actor.slot === 0)),
    'roster materializers must not promote a catalog template into a launch record');
  assert(materializerRuntime.states.some(state => state.actors.some(actor => actor.slot === 1)),
    'same-stream Actor constructors remain independent of unresolved roster inputs');
  assert(materializerRuntime.missingInputs.some(message =>
    /requires the caller's 20 Actor-input rows/.test(message)));

  const bodyScene = catalog.scenes.find(row => row.sceneId === 'scene:director:01f88edc');
  const bodySource = await OB64.cutsceneCodec.loadSceneSource(z64, bodyScene, { hashBytes });
  const bodyProjected = OB64.cutsceneCodec.projectSceneDocument(bodyScene, bodySource, catalog);
  const bodyRuntime = OB64.cutsceneRuntime.compile(
    bodyProjected.document, bodyProjected.program, bodyScene, catalog, { z64 });
  assert.strictEqual(bodyRuntime.directorMode, 2);
  const bodyState = bodyRuntime.states.find(state => state.actors.filter(actor =>
    actor.bodyPoseProgram).length === 2);
  assert(bodyState, 'mode-two body-pose commands must publish both alternate programs');
  const general = bodyState.actors.find(actor => actor.slot === 0);
  const lord = bodyState.actors.find(actor => actor.slot === 1);
  assert.deepStrictEqual(general.bodyPoseProgram, {
    decoder: 'alternate-body-pose', artSource: 83, selector: 7,
    flagB: 0, flagA: 0, ownerContext: 83, displayedFrameToken: 0,
    initialization: 'native-cleared-frame-state'
  });
  assert.deepStrictEqual(lord.bodyPoseProgram, {
    decoder: 'alternate-body-pose', artSource: 88, selector: 7,
    flagB: 1, flagA: 0, ownerContext: 88, displayedFrameToken: 0,
    initialization: 'native-cleared-frame-state'
  });
  assert.deepStrictEqual([general.baseX, lord.baseX], [-25, 25],
    'mode-two Actor positions must retain their exact Director coordinates');
  const generalBody = OB64.cutsceneSprites.frameForActor(spriteState, general);
  const lordBody = OB64.cutsceneSprites.frameForActor(spriteState, lord);
  assert(generalBody && lordBody,
    'alternate body-pose routes must decode native class-art pixels: ' +
      JSON.stringify(spriteState.errors));
  assert.deepStrictEqual([generalBody.frameToken, generalBody.width, generalBody.height],
    [0, 43, 44]);
  assert.deepStrictEqual([lordBody.frameToken, lordBody.width, lordBody.height],
    [0, 57, 46]);
  assert.notStrictEqual(hashBytes(generalBody.rgba), hashBytes(lordBody.rgba));

  const dialogueState = runtime.states.find(state => state.dialogue.length &&
    state.dialogue[0].payload.text);
  assert(dialogueState, 'the Director BF selector must join to Serifu text');

  OB64.cutsceneRuntime.bind(projected.document, runtime);
  assert.strictEqual(OB64.cutscenePreview.sceneDurationFrames(projected.document),
    runtime.durationTicks);
  const scrubbed = OB64.cutscenePreview.evaluateAtFrame(
    projected.document, moveTrace.tick + 1, { pathId: 'default' });
  assert.strictEqual(scrubbed.runtime.engine, 'director-scheduler');
  assert.strictEqual(scrubbed.actors.find(row => row.slot === slot).x, afterOneUpdate.x);
  const repeated = OB64.cutscenePreview.evaluateAtFrame(
    projected.document, moveTrace.tick + 1, { pathId: 'default' });
  assert.strictEqual(OB64.cutsceneModel.stableStringify(scrubbed, 0),
    OB64.cutsceneModel.stableStringify(repeated, 0));

  const screenPoint = OB64.cutsceneRenderer.projectPoint(afterOneUpdate,
    scrubbed.actorProjection);
  assert(Number.isFinite(screenPoint.x) && Number.isFinite(screenPoint.y));

  console.log('PASS Director runtime uses scheduler order, native movement and pose clocks, registered-prepass mode-zero Actors, captured camera geometry, split launch backgrounds, and Serifu dialogue');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
