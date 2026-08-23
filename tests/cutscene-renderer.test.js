'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of ['cutscene-model.js', 'cutscene-preview.js', 'cutscene-renderer.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}

const M = OB64.cutsceneModel;
const P = OB64.cutscenePreview;
const R = OB64.cutsceneRenderer;
const document = M.createSceneDocument({
  identity: {
    sceneId: 'scene:renderer', technicalName: 'Renderer fixture', friendlyName: null,
    engine: 'director', sourceRevision: 'us-rev0', directorKey: '00000001',
    aliases: [], triggerStatus: 'fixture'
  }
});
for (const [id, slot, x, z] of [['left', 0, -100, 0], ['right', 1, 100, 100]]) {
  M.addActor(document, M.createActor({
    id: 'actor:' + id, label: id, slot, artSourceId: 'bank:' + slot,
    capability: M.capabilities.NATIVE,
    initial: { visible: true, x, y: 0, z, facing: 'native-0', poseId: null }
  }));
}
M.addTrack(document, M.createTrack({
  id: 'track:left:movement', type: 'movement', actorId: 'actor:left', label: 'Move'
}));
M.addClip(document, 'track:left:movement', M.createClip({
  id: 'clip:left:move', kind: 'movement', startFrame: 0, durationFrames: 30,
  capability: M.capabilities.NATIVE,
  payload: { from: { x: -100, y: 0, z: 0 }, to: { x: 80, y: 0, z: 80 } }
}));

const preview = P.evaluateAtFrame(document, 15);
const first = R.renderFrame(document, preview, { selectedActorId: 'actor:left' });
const second = R.renderFrame(document, preview, { selectedActorId: 'actor:left' });
assert.deepStrictEqual(first.rgba, second.rgba);
assert.strictEqual(first.width, 320);
assert.strictEqual(first.height, 240);
assert.strictEqual(first.hitRegions.length, 2);
assert.strictEqual(first.movementPaths.length, 1);
const selected = first.hitRegions.find(region => region.actorId === 'actor:left');
assert.strictEqual(R.hitTest(first, (selected.left + selected.right) / 2,
  (selected.top + selected.bottom) / 2), 'actor:left');
assert.strictEqual(R.hitTest(first, 319, 0), null);
assert.strictEqual(
  crypto.createHash('sha256').update(first.rgba).digest('hex').toUpperCase(),
  'D2A392A880B7FC10C9024A807AAAA7ECAA7A793B691975CFB43C6008267B05F6'
);
const nextFrame = R.renderFrame(document, P.evaluateAtFrame(document, 16), {
  selectedActorId: 'actor:left'
});
assert.notDeepStrictEqual(first.rgba, nextFrame.rgba,
  'the honest fallback actor should still show pose/movement motion');

const nativeProjection = {
  mode: 'native-perspective-runtime', modelScale: 0.1,
  eye: { x: 0, y: 0, z: 340 }, target: { x: 0, y: 0, z: 0 },
  up: { x: 0, y: 1, z: 0 }, fovYDegrees: 38, aspect: 4 / 3,
  near: 1, far: 4000, screenWidth: 320, screenHeight: 240,
};
assert.deepStrictEqual(R.projectPoint({ x: 0, y: 0, z: 0 }, nativeProjection),
  { x: 160, y: 120 });
assert.strictEqual(R.spritePerspectiveScale({ x: 0, y: 0, z: 0 }, nativeProjection), 1,
  'native sprite art must retain its stored pixel size on the camera target plane');
assert(Math.abs(R.spritePerspectiveScale({ x: 0, y: 0, z: 1000 }, nativeProjection) -
  340 / 240) < 0.000001,
  'an Actor closer to the camera must grow by the native perspective-depth ratio');
assert(Math.abs(R.spritePerspectiveScale({ x: 0, y: 0, z: -1000 }, nativeProjection) -
  340 / 440) < 0.000001,
  'an Actor farther from the camera must shrink by the native perspective-depth ratio');

const formationProjection = {
  mode: 'native-perspective-runtime', modelScale: 0.1,
  eye: { x: 82.38899993896484, y: 41.6349983215332, z: 40.12200164794922 },
  target: { x: -1.5, y: 0, z: -1.2699999809265137 },
  up: { x: 0, y: 1, z: 0 }, fovYDegrees: 12.880000114440918,
  aspect: 4 / 3, near: 1, far: 5000, screenWidth: 320, screenHeight: 240,
};
assert(Math.abs(R.perspectivePixelsPerModelUnit({ x: -15, y: 0, z: -12.7 },
  formationProjection) - 1.038286942130806) < 0.000001,
  'mode-2 Actor art must use the native pixels-per-sprite-unit scale, not a target-plane value forced to one');

function assertPointClose(actual, expected, message) {
  assert(Math.abs(actual.x - expected.x) < 0.000001 &&
    Math.abs(actual.y - expected.y) < 0.000001, message + ': ' + JSON.stringify(actual));
}

const nativeBackground = {
  container: 'bg2', format: 0, originX: -2, originY: -1,
  width: 2, height: 2,
  rgba: new Uint8ClampedArray([
    255, 0, 0, 255, 0, 255, 0, 255,
    0, 0, 255, 255, 255, 255, 255, 255,
  ]),
};
const nativeBackgroundLayer = {
  nativeOrdinal: 0,
  renderPipeline: 'mode-zero-b5-actor-camera',
  sceneTransform: {
    rotationX: 0, rotationY: 0,
    translateX: 0, translateY: 0, translateZ: 0, uniformScale: 1,
  },
};
const backgroundGeometry = R.modeZeroBackgroundGeometry(
  nativeBackground, nativeBackgroundLayer, nativeProjection);
assert(backgroundGeometry, 'mode-zero B5 geometry must use the native Actor camera');
assertPointClose(backgroundGeometry.screenQuad[0], { x: 158, y: 119 },
  'signed B5 record origin must place the top-left pixel in the shared scene plane');
assertPointClose(backgroundGeometry.screenQuad[2], { x: 160, y: 121 },
  'B5 dimensions must remain native pixels on the Actor-camera target plane');

const translatedBackgroundLayer = M.cloneJson(nativeBackgroundLayer, 'background layer');
translatedBackgroundLayer.sceneTransform.translateX = 10;
translatedBackgroundLayer.sceneTransform.translateY = 5;
const translatedGeometry = R.modeZeroBackgroundGeometry(
  nativeBackground, translatedBackgroundLayer, nativeProjection);
assertPointClose(translatedGeometry.screenQuad[0], { x: 168, y: 114 },
  'each B5 layer must follow its matching Director transform channel');

const formatFiveGeometry = R.modeZeroBackgroundGeometry(Object.assign({}, nativeBackground, {
  format: 5, originX: -360, originY: -320, width: 320, height: 240,
  rgba: new Uint8ClampedArray(320 * 240 * 4),
}), nativeBackgroundLayer, nativeProjection);
assertPointClose(formatFiveGeometry.screenQuad[0], { x: 0, y: 0 },
  'format 5 must use the native full-screen loader origin');
assertPointClose(formatFiveGeometry.screenQuad[2], { x: 320, y: 240 },
  'format 5 must span the native framebuffer');

const compoundFormatFiveGeometry = R.modeZeroBackgroundGeometry(Object.assign({},
  nativeBackground, { format: 5, compoundAssembled: true }),
nativeBackgroundLayer, nativeProjection);
assertPointClose(compoundFormatFiveGeometry.screenQuad[0], { x: 158, y: 119 },
  'an assembled format-5 background must use its complete native record bounds');
assertPointClose(compoundFormatFiveGeometry.screenQuad[2], { x: 160, y: 121 },
  'assembled format-5 geometry must not be forced back to a 320×240 origin');

const convertedRawGeometry = R.modeZeroBackgroundGeometry({
  container: '64', width: 2, height: 2,
  rgba: nativeBackground.rgba,
}, nativeBackgroundLayer, nativeProjection);
assertPointClose(convertedRawGeometry.screenQuad[0], { x: 159, y: 119 },
  'mode-zero raw images must use the centered layer object created by the game');
assertPointClose(convertedRawGeometry.screenQuad[2], { x: 161, y: 121 },
  'raw scene layers must receive their Director transform instead of staying centered');

const backgroundDocument = M.createSceneDocument({
  identity: {
    sceneId: 'scene:background-renderer', technicalName: 'Background renderer fixture',
    friendlyName: null, engine: 'director', sourceRevision: 'us-rev0',
    directorKey: '00000002', aliases: [], triggerStatus: 'fixture'
  }
});
const backgroundRender = R.renderFrame(backgroundDocument, { actors: [] }, {
  backgrounds: [{ image: nativeBackground, layer: nativeBackgroundLayer }],
  backgroundProjection: {}, projection: nativeProjection,
});
function renderedPixel(rendered, x, y) {
  const offset = (y * rendered.width + x) * 4;
  return Array.from(rendered.rgba.slice(offset, offset + 4));
}
assert.deepStrictEqual(renderedPixel(backgroundRender, 158, 119), [255, 0, 0, 255]);
assert.deepStrictEqual(renderedPixel(backgroundRender, 159, 119), [0, 255, 0, 255]);
assert.deepStrictEqual(renderedPixel(backgroundRender, 158, 120), [0, 0, 255, 255]);
assert.deepStrictEqual(renderedPixel(backgroundRender, 159, 120), [255, 255, 255, 255]);

const solidStage = {
  width: 320, height: 240,
  rgba: new Uint8ClampedArray(320 * 240 * 4).fill(255),
};
const viewportRender = R.renderFrame(backgroundDocument, { actors: [] }, {
  backgrounds: [{ image: solidStage, layer: { role: 'environment-base' } }],
  backgroundProjection: {
    mode: 'stage-fit', scale: 1,
    viewport: { left: 0, top: 23, width: 320, height: 191 },
  },
});
assert.deepStrictEqual(renderedPixel(viewportRender, 160, 22), [0, 0, 0, 255],
  'the native cutscene viewport must mask rows above the scene');
assert.deepStrictEqual(renderedPixel(viewportRender, 160, 23), [255, 255, 255, 255]);
assert.deepStrictEqual(renderedPixel(viewportRender, 160, 213), [255, 255, 255, 255]);
assert.deepStrictEqual(renderedPixel(viewportRender, 160, 214), [0, 0, 0, 255],
  'the native cutscene viewport must mask rows below the scene');

const wideStage = {
  width: 400, height: 240,
  rgba: new Uint8ClampedArray(400 * 240 * 4).fill(255),
};
const pannedWideStage = R.renderFrame(backgroundDocument, { actors: [] }, {
  backgrounds: [{ image: wideStage, layer: { role: 'environment-base' } }],
  backgroundProjection: { mode: 'stage-fit', scale: 1 },
  camera: { translateX: 40, translateY: 0, scaleX: 1, scaleY: 1 },
});
assert.deepStrictEqual(renderedPixel(pannedWideStage, 0, 120), [255, 255, 255, 255],
  'camera movement must draw off-screen background pixels instead of exposing a black seam');
assert.deepStrictEqual(renderedPixel(pannedWideStage, 319, 120), [255, 255, 255, 255]);

const registeredStage = Object.assign({}, solidStage, {
  originX: 0, originY: 0, reference: { x: 0, y: 0 },
});
const registeredCameraRender = R.renderFrame(backgroundDocument, { actors: [] }, {
  backgrounds: [{ image: registeredStage, layer: { role: 'environment-base' } }],
  backgroundProjection: {
    mode: 'b5-reference-capture', cropWorldX: 0, cropWorldY: 0, scale: 1,
    calibrationCamera: { translateX: 40, translateY: -20, scaleX: 1, scaleY: 1 },
  },
  camera: { translateX: 40, translateY: -20, scaleX: 1, scaleY: 1 },
});
assert.deepStrictEqual(renderedPixel(registeredCameraRender, 0, 0), [255, 255, 255, 255],
  'a capture-registered background must not receive its calibration camera twice');
assert.deepStrictEqual(renderedPixel(registeredCameraRender, 319, 239),
  [255, 255, 255, 255]);

const spritePreview = M.cloneJson(preview, 'preview');
const spriteActor = spritePreview.actors.find(actor => actor.id === 'actor:left');
spriteActor.uniformScale = 2;
spriteActor.tint = { red: 255, green: 0, blue: 0 };
const whiteSprite = {
  width: 2, height: 2, anchorX: 1, anchorY: 2,
  rgba: new Uint8ClampedArray([
    255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255,
  ]),
};

const nativeLayerActor = {
  id: 'actor:native-layers', slot: 0, visible: true,
  x: 0, y: 0, z: 0, uniformScale: 1,
  renderModeByte: 0, opacityByte: 255,
  tint: { red: 255, green: 255, blue: 255 },
  facing: 'native-0', poseFrame: 0, movementFrame: 0,
};
const nativeLayerFrame = {
  width: 1, height: 1, anchorX: 0, anchorY: 0,
  rgba: new Uint8ClampedArray([255, 255, 255, 255]),
  nativeLayers: [
    { width: 2, height: 1, drawOffsetX: -2, drawOffsetY: -1,
      scaleX: 1, scaleY: 1, rgba: new Uint8ClampedArray([
        255, 0, 0, 255, 255, 0, 0, 255,
      ]) },
    { width: 1, height: 1, drawOffsetX: 1, drawOffsetY: 0,
      scaleX: 2, scaleY: 1, rgba: new Uint8ClampedArray([0, 255, 0, 255]) },
  ],
};
const nativeLayerRender = R.renderFrame(backgroundDocument, { actors: [nativeLayerActor] }, {
  projection: {
    mode: 'fit-native-preview', xMin: -1, xMax: 1, zMin: -1, zMax: 1,
    left: 159, right: 161, top: 119, bottom: 121,
  },
  actorFrames: { 'actor:native-layers': nativeLayerFrame },
});
const nativeLayerRegion = nativeLayerRender.hitRegions[0];
assert.deepStrictEqual(nativeLayerRegion,
  { actorId: 'actor:native-layers', left: 158, top: 119, right: 164, bottom: 121 },
  'native metadata offsets and per-layer scales must remain relative to the Actor origin');
assert.deepStrictEqual(renderedPixel(nativeLayerRender, 158, 119), [255, 0, 0, 255]);
assert.deepStrictEqual(renderedPixel(nativeLayerRender, 162, 120), [0, 255, 0, 255]);
const spriteRender = R.renderFrame(document, spritePreview, {
  actorFrames: { 'actor:left': whiteSprite },
});
const spriteRegion = spriteRender.hitRegions.find(region => region.actorId === 'actor:left');
assert.strictEqual(spriteRegion.right - spriteRegion.left, 4);
assert.strictEqual(spriteRegion.bottom - spriteRegion.top, 4);
const spritePixel = (spriteRegion.top * spriteRender.width + spriteRegion.left) * 4;
assert.deepStrictEqual(Array.from(spriteRender.rgba.slice(spritePixel, spritePixel + 3)),
  [255, 0, 0]);

function renderPassActor(renderModeByte, opacityByte) {
  const actor = {
    id: 'actor:render-pass', slot: 0, visible: true,
    x: 0, y: 0, z: 0, uniformScale: 1,
    renderModeByte, opacityByte,
    tint: { red: 255, green: 255, blue: 255 },
    facing: 'native-0', poseFrame: 0, movementFrame: 0,
  };
  return R.renderFrame(backgroundDocument, { actors: [actor] }, {
    projection: nativeProjection,
    actorFrames: { 'actor:render-pass': whiteSprite },
  });
}

for (const mode of [0, 2]) {
  const passRender = renderPassActor(mode, 0);
  assert.strictEqual(passRender.hitRegions.length, 1,
    'even render mode ' + mode + ' must keep the opacity-bypass main Actor pass');
  const region = passRender.hitRegions[0];
  assert.deepStrictEqual(renderedPixel(passRender, region.left, region.top),
    [255, 255, 255, 255],
  'the flattened main Actor pass must remain opaque when its native pass bypasses opacity');
}
assert.strictEqual(renderPassActor(1, 0).hitRegions.length, 0,
  'render mode 1 must still respect zero opacity in its ordinary-only pass');
assert.strictEqual(renderPassActor(3, 255).hitRegions.length, 0,
  'render mode 3 must remain hidden because it selects neither Actor pass');

const overlayRender = R.renderFrame(document, preview, {
  overlays: [{ red: 0, green: 0, blue: 255, alpha: 255 }],
});
assert.deepStrictEqual(Array.from(overlayRender.rgba.slice(0, 4)), [0, 0, 255, 255]);

console.log('PASS Cutscene Stage rendering is deterministic with native projection, scale, tint, overlays, actor hit regions, and movement paths.');
