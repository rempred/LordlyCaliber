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
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of [
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-runtime.js',
  'cutscene-preview.js', 'cutscene-assets.js', 'cutscene-renderer.js',
  'cutscene-project.js', 'cutscene-export.js', 'cutscene-ui.js'
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
  const scene = catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
  const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog);
  const baseline = projected.document;
  const baselineText = OB64.cutsceneModel.serializeSceneDocument(baseline, 0);
  const state = {
    catalog,
    selectedSceneId: scene.sceneId,
    histories: { [scene.storageId]: OB64.cutsceneModel.createHistory(baseline) },
    originalSerialized: { [scene.storageId]: baselineText },
    sourceByAssetId: { [scene.assetId]: source },
    programByAssetId: { [scene.assetId]: projected.program },
    sourceErrors: {}, views: {}, callbacks: {}
  };

  state.runtimeByAssetId = {
    [scene.assetId]: {
      states: [
        { background: { layers: [{}] }, actors: [], effects: [],
          overlays: [{ alpha: 255 }], sceneColor: { red: 255, green: 255, blue: 255 } },
        { background: { layers: [{}] }, actors: [], effects: [],
          overlays: [{ alpha: 0 }], sceneColor: { red: 0, green: 0, blue: 0 } },
        { background: { layers: [{}] }, actors: [], effects: [],
          overlays: [{ alpha: 0 }], sceneColor: { red: 255, green: 255, blue: 255 } },
      ]
    }
  };
  assert.strictEqual(OB64.cutsceneUI.initialViewFrame(state, scene.sceneId), 2,
    'the editor must skip deliberate black opening frames when choosing its initial preview');
  state.runtimeByAssetId[scene.assetId].states = [
    { background: { layers: [{}] }, actors: [], effects: [], overlays: [],
      sceneColor: { red: 255, green: 255, blue: 255 } },
    { background: { layers: [{}] }, actors: [{ visible: true }], effects: [], overlays: [],
      sceneColor: { red: 255, green: 255, blue: 255 } },
  ];
  assert.strictEqual(OB64.cutsceneUI.initialViewFrame(state, scene.sceneId), 1,
    'an unmeasured scene must open on its first clear native Actor frame instead of a background-only frame');
  delete state.runtimeByAssetId;

  const contextualRom = { z64, layout: { id: 'us-rev0' } };
  const contextualState = OB64.cutsceneUI.ensureState(contextualRom);
  const contextualScene = catalog.getScene('rom-director:01F56D8E');
  contextualState.selectedSceneId = contextualScene.sceneId;
  const contextualChoices = OB64.cutsceneUI.launchContextChoices(
    contextualState, contextualScene);
  assert.strictEqual(contextualChoices.length, 4);
  assert.deepStrictEqual(contextualChoices.map(choice => [
    choice.context.concurrentDirectorAssetId,
    choice.context.concurrentDirectorTickOffset
  ]), [
    ['rom-director:01F5646A', 1],
    ['rom-director:01F56900', 1],
    ['rom-director:01F5646A', 1],
    ['rom-director:01F56900', 1]
  ]);
  await OB64.cutsceneUI.loadScene(contextualRom, contextualState, contextualScene);
  const contextualRuntime = contextualState.runtimeByAssetId[contextualScene.assetId];
  assert(contextualRuntime && contextualRuntime.terminated,
    'opening an event-chained stream must evaluate its native concurrent owner');
  assert.strictEqual(contextualRuntime.concurrentContext.assetId,
    'rom-director:01F5646A');
  assert.strictEqual(contextualRuntime.states.at(-1).actors.filter(actor =>
    actor.visible).length, 9,
  'the editor launch path must expose shared Actors instead of the standalone blank roster');
  assert.strictEqual(
    contextualState.views[contextualScene.sceneId].launchContextId,
    contextualChoices[0].id,
  'the default exact event invocation must live in durable view state');

  contextualState.views[contextualScene.sceneId].launchContextId = contextualChoices[1].id;
  await OB64.cutsceneUI.loadScene(contextualRom, contextualState, contextualScene);
  assert.strictEqual(
    contextualState.runtimeByAssetId[contextualScene.assetId].concurrentContext.assetId,
    'rom-director:01F56900',
  'a durable context selection must recompile against the chosen native concurrent stream');

  const translatedLaunchScene = catalog.getScene('rom-director:01FB91EC');
  const translatedLaunchChoices = OB64.cutsceneUI.launchContextChoices(
    contextualState, translatedLaunchScene);
  assert.deepStrictEqual(translatedLaunchChoices.map(choice =>
    OB64.cutsceneUI.launchOperandTranslations(translatedLaunchScene, choice)), [
    { 0: 88 },
    { 0: 90 }
  ], 'event-program constants must reach the selected Director translation-table input');

  assert.strictEqual(baseline.branches.length, 1,
    'native query gates must not be invented as alternate movie paths');
  const nativeQueries = projected.program.primitives.filter(primitive => primitive.query);
  assert(nativeQueries.length > 0, 'native query gates must remain visible in the Director program');
  assert(nativeQueries.every(primitive => projected.program.compositeByNodeId[primitive.id]));
  const movementAction = projected.program.composites.find(composite =>
    composite.nodeIds.some(nodeId =>
      projected.program.primitiveById[nodeId].name === 'actor_move'));
  const movementTarget = OB64.cutsceneUI.directorEditorTarget(
    baseline, projected.program, movementAction);
  assert(movementTarget && movementTarget.kind === 'clip');
  assert.strictEqual(OB64.cutsceneUI.findClipRow(
    baseline, movementTarget.clipId).clip.kind, 'movement');
  const actorCreate = projected.program.composites.find(composite =>
    composite.nodeIds.length === 1 &&
    projected.program.primitiveById[composite.nodeIds[0]].name === 'actor_create');
  assert.strictEqual(OB64.cutsceneUI.directorEditorTarget(
    baseline, projected.program, actorCreate).kind, 'actor');
  const fixedWait = projected.program.composites.find(composite =>
    composite.kind === 'registered-wait');
  assert(fixedWait, 'fixture needs one complete fixed-wait bundle');
  const fixedWaitTarget = OB64.cutsceneUI.directorEditorTarget(
    baseline, projected.program, fixedWait);
  assert(fixedWaitTarget && fixedWaitTarget.kind === 'clip');
  const fixedWaitClip = OB64.cutsceneUI.findClipRow(
    baseline, fixedWaitTarget.clipId).clip;
  assert.strictEqual(fixedWaitClip.kind, 'wait');
  assert.strictEqual(fixedWaitClip.payload.registeredCounterEditable, true);
  const skippableWait = projected.program.composites.find(composite =>
    composite.kind === 'skippable-registered-wait');
  assert(skippableWait, 'fixture needs one A-skippable wait bundle');
  assert.strictEqual(OB64.cutsceneUI.directorEditorTarget(
    baseline, projected.program, skippableWait), null,
  'A-skippable bundles stay read-only until their staged action grammar can lower safely');

  const boundaries = OB64.cutsceneUI.insertionBoundaries(scene, baseline);
  assert(boundaries.length > 0);
  const boundary = OB64.cutsceneUI.boundaryForFrame(scene, baseline, 45);
  assert(boundary && boundary.insertBefore === true);
  assert.strictEqual(
    OB64.cutsceneUI.boundaryForFrame(scene, baseline, 45).id,
    boundary.id,
    'the same preview frame must resolve to the same native insertion boundary'
  );

  const unchanged = OB64.cutsceneModel.cloneSceneDocument(baseline);
  OB64.cutsceneUI.refreshExportRequirements(state, scene, unchanged);
  assert.strictEqual(OB64.cutsceneModel.serializeSceneDocument(unchanged, 0), baselineText,
    'derived compatibility analysis must not make an unchanged scene dirty');

  const poseEdit = OB64.cutsceneModel.cloneSceneDocument(baseline);
  const pose = poseEdit.tracks.flatMap(track => track.clips)
    .find(clip => clip.kind === 'pose' && clip.capability === 'native');
  pose.payload.nativeFacing = (pose.payload.nativeFacing + 1) % 4;
  pose.payload.facing = 'native-' + pose.payload.nativeFacing;
  OB64.cutsceneUI.refreshExportRequirements(state, scene, poseEdit);
  assert.strictEqual(poseEdit.exportRequirements.capability, 'native');
  assert(poseEdit.exportRequirements.allocationBytes > 0);

  const actorEdit = OB64.cutsceneModel.cloneSceneDocument(baseline);
  actorEdit.actors[0].initial.x += 1;
  OB64.cutsceneUI.refreshExportRequirements(state, scene, actorEdit);
  assert.strictEqual(actorEdit.exportRequirements.capability, 'native');
  assert(actorEdit.exportRequirements.allocationBytes > 0,
    'an actor with an exact typed Place command must remain natively editable');

  const waitEdit = OB64.cutsceneModel.cloneSceneDocument(baseline);
  const editableWait = OB64.cutsceneUI.findClipRow(
    waitEdit, fixedWaitTarget.clipId).clip;
  const nextWaitTicks = editableWait.payload.nativeTicks + 1;
  editableWait.durationFrames = nextWaitTicks;
  editableWait.payload.nativeTicks = nextWaitTicks;
  editableWait.payload.registeredCounterTarget = nextWaitTicks;
  OB64.cutsceneUI.refreshExportRequirements(state, scene, waitEdit);
  assert.strictEqual(waitEdit.exportRequirements.capability, 'native');
  assert(waitEdit.exportRequirements.allocationBytes > 0);

  const holdEdit = OB64.cutsceneModel.cloneSceneDocument(baseline);
  const flow = holdEdit.tracks.find(track => track.type === 'flow');
  flow.clips.push(OB64.cutsceneModel.createClip({
    id: 'clip:authored:wait:ui-test', kind: 'wait', startFrame: 45,
    durationFrames: 30, capability: 'native', payload: {},
    source: { insertBeforeNodeId: boundary.id }
  }));
  OB64.cutsceneUI.refreshExportRequirements(state, scene, holdEdit);
  assert(!holdEdit.exportRequirements.reasons.some(reason =>
    reason.includes('display-only command')),
  'a new flow clip must not make neighboring preserved markers appear edited');

  const poseDocument = OB64.cutsceneModel.cloneSceneDocument(baseline);
  const poseActor = poseDocument.actors[0];
  const poseBankMatch = String(poseActor.artSourceId || '').match(/:(\d+)$/);
  const poseBank = Number.isInteger(poseActor.source.bank)
    ? poseActor.source.bank : Number(poseBankMatch[1]);
  const poseChoices = catalog.poseSelectionsForBank(poseBank);
  const chosenPose = poseChoices.find(choice => choice.programId &&
    (choice.animationKey !== poseActor.source.animationKey ||
      choice.facing !== Number(String(poseActor.initial.facing).replace('native-', ''))));
  assert(chosenPose, 'fixture needs an alternate durable pose dropdown choice');
  poseDocument.tracks.push(OB64.cutsceneModel.createTrack({
    id: 'track:ui-state:pose', type: 'pose', actorId: poseActor.id,
    label: 'UI state pose', clips: [OB64.cutsceneModel.createClip({
      id: 'clip:ui-state:pose', kind: 'pose', startFrame: 45,
      durationFrames: Math.max(1, chosenPose.durationFrames), capability: 'native',
      payload: {
        bank: chosenPose.bank,
        animationKey: chosenPose.animationKey,
        nativeFacing: chosenPose.facing,
        facing: 'native-' + chosenPose.facing,
        poseId: chosenPose.poseId
      }
    })]
  }));
  const durablePoseChoice = OB64.cutsceneUI.actorPoseSelectionAtFrame(
    poseDocument, poseActor, 45, baseline.branches[0].id, catalog);
  assert.strictEqual(durablePoseChoice.selection.programId, chosenPose.programId,
    'the pose dropdown must reconstruct its choice from the edited scene model');

  function uiElement(attributes, values) {
    attributes = Object.assign({}, attributes);
    values = values || {};
    return {
      scrollTop: values.top || 0,
      scrollLeft: values.left || 0,
      value: values.value == null ? '' : String(values.value),
      options: (values.options || []).map(value => ({ value: String(value) })),
      selectionStart: values.selectionStart,
      selectionEnd: values.selectionEnd,
      getAttribute(name) { return attributes[name] == null ? null : attributes[name]; },
      closest(selector) {
        return selector === '[data-cutscene-focus-key]' &&
          attributes['data-cutscene-focus-key'] ? this : null;
      },
      focus(options) {
        this.focusOptions = options || null;
        global.document.activeElement = this;
      },
      setSelectionRange(start, end) {
        this.selectionStart = start;
        this.selectionEnd = end;
      }
    };
  }

  function uiPanel(sceneId, scrolls, selects, focusables, viewport) {
    const shell = uiElement({ 'data-cutscene-scene-id': sceneId });
    return {
      firstChild: shell,
      querySelector(selector) {
        assert.strictEqual(selector, '.cutscene-studio');
        return shell;
      },
      querySelectorAll(selector) {
        if (selector === '[data-cutscene-scroll]') return scrolls;
        if (selector === '[data-cutscene-focus-key]') return focusables;
        throw new Error('Unexpected Cutscene UI selector ' + selector);
      },
      contains(element) { return focusables.includes(element); },
      closest(selector) {
        assert.strictEqual(selector, '.content');
        return viewport;
      }
    };
  }

  const oldInspector = uiElement({ 'data-cutscene-scroll': 'inspector' },
    { top: 731, left: 17 });
  const oldBrowser = uiElement({ 'data-cutscene-scroll': 'browser' },
    { top: 284, left: 0 });
  const oldTimeline = uiElement({ 'data-cutscene-scroll': 'timeline' },
    { top: 0, left: 1168 });
  const oldSelect = uiElement({ 'data-cutscene-focus-key': 'actor-pose:slot-3' }, {
    value: 'pose:chosen', options: ['pose:default', 'pose:chosen']
  });
  const oldViewport = uiElement({}, { top: 912, left: 8 });
  const oldPage = uiElement({}, { top: 1244, left: 3 });
  global.document = { activeElement: oldSelect, scrollingElement: oldPage };
  const snapshot = OB64.cutsceneUI.captureUi(uiPanel(scene.sceneId,
    [oldInspector, oldBrowser, oldTimeline], [oldSelect], [oldSelect], oldViewport));

  const newInspector = uiElement({ 'data-cutscene-scroll': 'inspector' });
  const newBrowser = uiElement({ 'data-cutscene-scroll': 'browser' });
  const newTimeline = uiElement({ 'data-cutscene-scroll': 'timeline' });
  const newSelect = uiElement({ 'data-cutscene-focus-key': 'actor-pose:slot-3' }, {
    value: 'pose:default', options: ['pose:default', 'pose:chosen']
  });
  const newViewport = uiElement({});
  const newPage = uiElement({});
  global.document.scrollingElement = newPage;
  OB64.cutsceneUI.restoreUi(uiPanel(scene.sceneId,
    [newInspector, newBrowser, newTimeline], [newSelect], [newSelect], newViewport), snapshot);
  assert.strictEqual(newInspector.scrollTop, 731,
    'the inspector must retain its vertical scroll after a dropdown rerender');
  assert.strictEqual(newInspector.scrollLeft, 17);
  assert.strictEqual(newBrowser.scrollTop, 284,
    'the outer scene browser must retain its scroll position');
  assert.strictEqual(newTimeline.scrollLeft, 1168,
    'the timeline must retain its horizontal scroll after a selection rerender');
  assert.strictEqual(newViewport.scrollTop, 912,
    'the responsive content viewport must retain its scroll position');
  assert.strictEqual(newPage.scrollTop, 1244,
    'mobile document scrolling must survive DOM replacement');
  assert.strictEqual(newSelect.value, 'pose:chosen',
    'a still-valid dropdown choice must survive DOM replacement');
  assert.deepStrictEqual(newSelect.focusOptions, { preventScroll: true },
    'focus restoration must not move a restored scroller');

  const otherSelect = uiElement({ 'data-cutscene-focus-key': 'actor-pose:slot-3' }, {
    value: 'pose:other-scene', options: ['pose:chosen', 'pose:other-scene']
  });
  const otherPage = uiElement({});
  global.document.scrollingElement = otherPage;
  OB64.cutsceneUI.restoreUi(uiPanel('scene:director:different',
    [uiElement({ 'data-cutscene-scroll': 'inspector' })], [otherSelect],
    [otherSelect], uiElement({})), snapshot);
  assert.strictEqual(otherSelect.value, 'pose:other-scene',
    'a scene change must use the new scene model instead of an old dropdown snapshot');
  assert.strictEqual(otherSelect.focusOptions, undefined,
    'focus from the previous scene must not leak into the new context');

  const extractedPixel = new Uint8Array([0xF8, 0x01]);
  const extractedPixelHash = hashBytes(extractedPixel);
  const originalExtractArchive = OB64.extractArchive;
  const originalRomCompatibility = OB64.romCompatibility;
  OB64.extractArchive = () => extractedPixel;
  OB64.romCompatibility = { sha256Hex: bytes => Promise.resolve(hashBytes(bytes)) };
  const imageState = {
    imageCache: {}, imageLoading: {}, imageCacheBytes: 0, imageClock: 0
  };
  const decodedPixel = await OB64.cutsceneUI.decodeImageAsset({
    z64: new Uint8Array(0), archives: [{ index: 0 }]
  }, imageState, {
    assetId: 'archive:test-sync-extractor',
    displayName: 'Synchronous archive extractor test',
    sourceKind: 'lha-archive',
    archiveIndex: 0,
    container: 'raw',
    format: 'rgba5551',
    layout: { pixelFormat: 'rgba5551', width: 1, height: 1 },
    source: { decodedSha256: extractedPixelHash }
  });
  assert.strictEqual(decodedPixel.renderable, true,
    'archive-backed Stage art must accept the synchronous ROM extractor');
  assert.deepStrictEqual(Array.from(decodedPixel.rgba), [255, 0, 0, 255]);

  function putU16(bytes, offset, value) {
    bytes[offset] = value >>> 8 & 0xFF;
    bytes[offset + 1] = value & 0xFF;
  }
  function putU32(bytes, offset, value) {
    bytes[offset] = value >>> 24 & 0xFF;
    bytes[offset + 1] = value >>> 16 & 0xFF;
    bytes[offset + 2] = value >>> 8 & 0xFF;
    bytes[offset + 3] = value & 0xFF;
  }
  const compoundRootBytes = new Uint8Array(32);
  compoundRootBytes.set([0x42, 0x35, 5, 2], 0);
  putU16(compoundRootBytes, 12, 1);
  putU16(compoundRootBytes, 14, 1);
  putU32(compoundRootBytes, 16, 8);
  putU16(compoundRootBytes, 24, 0xF801);
  const compoundTailBytes = new Uint8Array(24);
  putU16(compoundTailBytes, 0, 1);
  putU16(compoundTailBytes, 4, 1);
  putU16(compoundTailBytes, 6, 1);
  putU32(compoundTailBytes, 8, 8);
  putU16(compoundTailBytes, 16, 0x003F);
  const compoundRootAsset = {
    assetId: 'archive:test-compound-root', displayName: 'Compound root',
    sourceKind: 'lha-archive', archiveIndex: 0, container: 'bg2', format: 5,
    source: { decodedSha256: hashBytes(compoundRootBytes) },
    compound: {
      kind: 'b5-format5-external-records', declaredMemberCount: 2,
      originX: 0, originY: 0, width: 2, height: 1, reference: { x: 0, y: 0 },
      members: [
        { ordinal: 0, assetId: 'archive:test-compound-root', x: 0, y: 0,
          width: 1, height: 1, dataSize: 8 },
        { ordinal: 1, assetId: 'archive:test-compound-tail', x: 1, y: 0,
          width: 1, height: 1, dataSize: 8 },
      ]
    }
  };
  const compoundTailAsset = {
    assetId: 'archive:test-compound-tail', displayName: 'Compound tail',
    sourceKind: 'lha-archive', archiveIndex: 1, container: 'raw', format: 'rgba5551',
    layout: { pixelFormat: 'rgba5551', width: 1, height: 1, dataOffset: 16,
      nativeRecordHeader: true },
    source: { decodedSha256: hashBytes(compoundTailBytes) }
  };
  const compoundAssets = new Map([
    [compoundRootAsset.assetId, compoundRootAsset],
    [compoundTailAsset.assetId, compoundTailAsset]
  ]);
  OB64.extractArchive = (z64Bytes, archive) => archive.bytes;
  const compoundImageState = {
    catalog: { getImageAsset: assetId => compoundAssets.get(assetId) || null },
    imageCache: {}, imageLoading: {}, imageCacheBytes: 0, imageClock: 0
  };
  const decodedCompound = await OB64.cutsceneUI.decodeImageAsset({
    z64: new Uint8Array(0),
    archives: [{ bytes: compoundRootBytes }, { bytes: compoundTailBytes }]
  }, compoundImageState, compoundRootAsset);
  assert.strictEqual(decodedCompound.compoundAssembled, true);
  assert.strictEqual(decodedCompound.width, 2);
  assert.strictEqual(decodedCompound.height, 1);
  assert.deepStrictEqual(Array.from(decodedCompound.rgba), [
    255, 0, 0, 255,
    0, 0, 255, 255,
  ], 'the UI loader must fetch and assemble every external format-5 record');
  OB64.extractArchive = originalExtractArchive;
  OB64.romCompatibility = originalRomCompatibility;

  const uiSource = fs.readFileSync(path.join(EDITOR, 'cutscene-ui.js'), 'utf8');
  assert(uiSource.includes("window.addEventListener('pointerup', up)"));
  assert(uiSource.includes('selectedClipId'));
  assert(uiSource.includes('OB64.cutscenePreview.snapFrame'));
  assert(uiSource.includes("timelineMode: 'runtime'"));
  assert(uiSource.includes('Horizontal position is execution time'));
  assert(uiSource.includes('Horizontal position is physical Director-stream order'));
  assert(uiSource.includes(
    'Native mode-zero staging: registered-camera Actor prepass → centered scene transform → Actor camera'));
  assert(uiSource.includes(
    'Mode-two runtime active; the unresolved launch Actor camera uses a fit-to-scene preview.'));
  assert(uiSource.includes("sourceRows.push(['Environment selector'"));
  assert(uiSource.includes("sourceRows.push(['Foreground selector'"));
  assert(uiSource.includes("sourceRows.push(['Parent event launches'"));
  assert(uiSource.includes("sourceRows.push(['Launch operand table'"));
  assert(uiSource.includes("'mode-two-' + role + '-selector'"));
  assert(uiSource.includes('projection.launchContext = context'));
  assert(uiSource.includes('The native launch pre-scan can seed the mode-two environment'));
  assert(uiSource.includes('Number.isInteger(launchBackground.environmentSelector)'));
  assert(!uiSource.includes('launch inputs are synthesized'));
  assert(uiSource.includes("browser.setAttribute('data-cutscene-scroll', 'browser')"));
  assert(uiSource.includes('focus({ preventScroll: true })'));
  assert(!uiSource.includes('Actor projection is approximate until visual calibration passes.'));
  assert(!uiSource.includes('addRenderPass'));
  const indexSource = fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8');
  assert(indexSource.indexOf('cutscene-director.js') < indexSource.indexOf('cutscene-codec.js'),
    'the lossless Director model must load before the projection codec');
  assert(indexSource.indexOf('cutscene-codec.js') < indexSource.indexOf('cutscene-runtime.js') &&
    indexSource.indexOf('cutscene-runtime.js') < indexSource.indexOf('cutscene-preview.js'),
  'the execution runtime must load after the source codec and before preview evaluation');

  console.log('PASS Cutscene UI query visibility, insertion boundaries, live compatibility, and interaction state');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
