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
  const scene = catalog.directorScenes.find(row => row.friendlyName === 'Graduation Ceremony');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
  const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog);
  const runtime = OB64.cutsceneRuntime.compile(
    projected.document, projected.program, scene, catalog, { z64 });

  async function compileScene(identity) {
    const selectedScene = catalog.getScene(identity);
    const selectedSource = await OB64.cutsceneCodec.loadSceneSource(
      z64, selectedScene, { hashBytes });
    const selectedProjection = OB64.cutsceneCodec.projectSceneDocument(
      selectedScene, selectedSource, catalog);
    return OB64.cutsceneRuntime.compile(selectedProjection.document,
      selectedProjection.program, selectedScene, catalog, { z64 });
  }

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
  const transformedState = modeZeroRuntime.states.find(state => state.actors.some(actor =>
    actor.visible && actor.sceneTransform && actor.sceneTransform.translateY !== 0));
  assert(transformedState, 'mode-zero transform channels must reach visible Actor snapshots');
  const transformedActor = transformedState.actors.find(actor =>
    actor.visible && actor.sceneTransform.translateY !== 0);
  assert.strictEqual(transformedActor.x,
    transformedActor.baseX,
    'mode-zero scene transforms must not be added directly to Director world coordinates');
  assert.strictEqual(transformedActor.y,
    transformedActor.baseY + transformedActor.secondaryY);
  assert.strictEqual(transformedActor.renderPipeline, 'mode-zero-two-camera');

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
  const flashbackGeometry = OB64.cutsceneRenderer.modeZeroActorGeometry(
    flashbackActor, flashbackState, flashbackState.actorProjection);
  assert(flashbackGeometry, 'Flashback Actor 0 must use the native two-camera composition');
  assert(Math.abs(flashbackGeometry.registeredScreen.x - 7.502660751342773) < 0.03);
  assert(Math.abs(flashbackGeometry.registeredScreen.y - -30.644012451171875) < 0.03);
  assert(Math.abs(flashbackGeometry.scenePoint.x - -39.54388427734375) < 0.01);
  assert(Math.abs(flashbackGeometry.scenePoint.y - -59.928741455078125) < 0.01);
  assert(Math.abs(flashbackGeometry.scenePoint.z - -67.79168701171875) < 0.001);
  assert.deepStrictEqual(flashbackGeometry.screenPoint, { x: 58, y: 275 });
  assert(Math.abs(flashbackGeometry.scale - 1.38495) < 0.0001,
    'mode-zero scale must include registered-camera depth and Actor-camera depth');

  const inferredBattleScene = catalog.getScene('rom-director:01F53488');
  const inferredBattleSource = await OB64.cutsceneCodec.loadSceneSource(
    z64, inferredBattleScene, { hashBytes });
  const inferredBattleProjected = OB64.cutsceneCodec.projectSceneDocument(
    inferredBattleScene, inferredBattleSource, catalog);
  const inferredBattleRuntime = OB64.cutsceneRuntime.compile(
    inferredBattleProjected.document, inferredBattleProjected.program,
    inferredBattleScene, catalog, { z64 });
  const inferredBattleState = inferredBattleRuntime.states.find(state =>
    state.background.selectorTableId === 'background-table:mode2-environment:80');
  assert(inferredBattleState, 'the launch profile must select the mode-two environment table');
  assert.strictEqual(inferredBattleRuntime.directorMode, 2);
  assert.strictEqual(inferredBattleState.background.selector, 20);
  assert.deepStrictEqual(inferredBattleState.background.layers.map(layer =>
    [layer.assetId, layer.role]), [
    ['section-c-njpg:20', 'environment-base'],
    ['archive:32', 'foreground-mask'],
  ], 'environment and foreground selectors must remain separate but compose one Stage');
  assert(!inferredBattleRuntime.missingInputs.some(message =>
    /no complete registered Stage/.test(message)));
  assert(inferredBattleRuntime.assumptions.some(message => /stored mode-two requests mirror/.test(message)));

  const pairedHallRuntime = await compileScene('rom-director:01F62EE0');
  const pairedHallState = pairedHallRuntime.states.find(state =>
    state.background.selector === 50);
  assert(pairedHallState);
  assert.deepStrictEqual(pairedHallState.background.layers.map(layer =>
    [layer.assetId, layer.role]), [
    ['mode2-environment:00188E36', 'environment-base'],
    ['archive:44', 'foreground-mask'],
  ], 'selector 50 must combine its resource-4 environment and independent foreground');

  const pairedWestHallRuntime = await compileScene('rom-director:01F7ADA0');
  const pairedWestHallState = pairedWestHallRuntime.states.find(state =>
    state.background.selector === 53);
  assert(pairedWestHallState);
  assert.deepStrictEqual(pairedWestHallState.background.layers.map(layer =>
    [layer.assetId, layer.role]), [
    ['archive:13', 'environment-base'],
    ['archive:47', 'foreground-mask'],
  ], 'selector 53 must combine its shared-coordinate hall and foreground');

  const pairedChurchRuntime = await compileScene('rom-director:01FCD784');
  const pairedChurchState = pairedChurchRuntime.states.find(state =>
    state.background.selector === 31);
  assert(pairedChurchState);
  assert.deepStrictEqual(pairedChurchState.background.layers.map(layer =>
    [layer.assetId, layer.role]), [
    ['archive:2', 'environment-base'],
    ['archive:41', 'foreground-mask'],
  ], 'selector 31 must combine the filename-paired church scene and foreground');

  const selector23Scene = catalog.getScene('rom-director:01F705F4');
  const selector23Source = await OB64.cutsceneCodec.loadSceneSource(
    z64, selector23Scene, { hashBytes });
  const selector23Projected = OB64.cutsceneCodec.projectSceneDocument(
    selector23Scene, selector23Source, catalog);
  const selector23Runtime = OB64.cutsceneRuntime.compile(
    selector23Projected.document, selector23Projected.program,
    selector23Scene, catalog, { z64 });
  const selector23State = selector23Runtime.states.find(state =>
    state.background.selectorTableId === 'background-table:mode2-environment:80');
  assert(selector23State);
  assert.strictEqual(selector23State.background.selector, 23);
  assert.deepStrictEqual(selector23State.background.layers.map(layer =>
    [layer.assetId, layer.role]), [
    ['section-c-njpg:23', 'environment-base'],
  ], 'selector 23 must load its resource-4 base even though its overlay row is empty');

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
  assert.deepStrictEqual(openingRuntime.states[109].background.layers.map(row =>
    row.sceneTransform.translateY), [-230, -130, 20, -130, -130],
  'the second title transform must replace ATLUS with the Quest/Nintendo card');
  assert.deepStrictEqual(openingRuntime.states[175].background.layers.map(row =>
    row.sceneTransform.translateY), [-230, -130, -130, 17, -130],
  'the third title transform must present only the episode card');
  assert.deepStrictEqual(openingRuntime.states[309].background.layers.map(row =>
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
  const spriteState = OB64.cutsceneSprites.create(z64, catalog);
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
  const launchRosterRuntime = OB64.cutsceneRuntime.compile(
    launchRosterProjected.document, launchRosterProjected.program,
    launchRosterScene, catalog, { z64 });
  const synthesizedState = launchRosterRuntime.states.find(state => state.actors.some(actor =>
    actor.slot === 2 && actor.bank === 31 && actor.animationKey === 61));
  assert(synthesizedState,
    'an exact Actor State command must update its synthesized launch-roster record');
  const synthesizedActor = synthesizedState.actors.find(actor => actor.slot === 2);
  assert.deepStrictEqual([synthesizedActor.baseX, synthesizedActor.baseY, synthesizedActor.baseZ],
    [0, 0, 0], 'raw -1000 Actor State coordinates must preserve launch position');
  assert(OB64.cutsceneSprites.frameForActor(spriteState, synthesizedActor),
    'the exact post-launch Actor State selector must resolve to native pixels');

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

  console.log('PASS Director runtime uses scheduler order, native movement and pose clocks, two-camera mode-zero staging, captured camera geometry, runtime backgrounds, and Serifu dialogue');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
