'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const EXTRACTED = path.join(ROOT, 'ob64_all');
const MASTER = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.module = undefined;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of [
  'data.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js',
  'cutscene-director.js', 'cutscene-codec.js', 'cutscene-runtime.js',
  'cutscene-preview.js', 'cutscene-njpg.js', 'cutscene-assets.js', 'cutscene-sprites.js',
  'cutscene-renderer.js'
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
  const directorScenes = catalog.scenes.filter(scene => scene.engine === 'director');
  const z64 = normalizeV64(fs.readFileSync(MASTER));
  const beforeHash = hashBytes(z64);
  let nativeQueries = 0;
  let runtimeBackgroundScenes = 0;
  let runtimeVisibleBackgroundScenes = 0;
  let runtimeCameraScenes = 0;
  let runtimeActorScenes = 0;
  let runtimeVisibleActorScenes = 0;
  let runtimeAnimatedScenes = 0;
  let runtimeSpriteScenes = 0;
  let runtimePixelAnimatedScenes = 0;
  let runtimeStaticPoseScenes = 0;
  let runtimeDialogueScenes = 0;
  let runtimeTransformScenes = 0;
  let nativeStagePropScenes = 0;
  let nativeStagePropCount = 0;
  const runtimeBackgroundAssetIds = new Set();
  const spriteState = OB64.cutsceneSprites.create(z64, catalog);
  const spriteHashCache = new WeakMap();
  const spriteErrors = new Set();
  const imageCache = new Map();

  function imageForAsset(asset) {
    if (imageCache.has(asset.assetId)) return imageCache.get(asset.assetId);
    let bytes;
    if (asset.sourceKind === 'section-c-njpg') {
      bytes = z64.slice(asset.source.z64Start, asset.source.z64EndExclusive);
    } else if (asset.sourceKind === 'rom-resource') {
      const resource = asset.source.compressionKind === 'ob64-custom-lz'
        ? OB64.art.readCompressedResource(z64, asset.source.resourceKey)
        : OB64.art.readResource(z64, asset.source.resourceKey);
      bytes = asset.source.compressionKind === 'ob64-custom-lz'
        ? resource.decoded : resource.stored;
    } else {
      const directory = path.join(EXTRACTED, 'e' + asset.archiveIndex);
      const files = fs.readdirSync(directory, { withFileTypes: true })
        .filter(entry => entry.isFile());
      assert.strictEqual(files.length, 1, asset.assetId + ' decoded-file count');
      bytes = new Uint8Array(fs.readFileSync(path.join(directory, files[0].name)));
    }
    const image = asset.compound
      ? OB64.cutsceneAssets.composeCompoundImageAsset(asset,
        asset.compound.members.map(member => {
          const memberAsset = catalog.getImageAsset(member.assetId);
          assert(memberAsset, asset.assetId + ' missing compound member ' + member.assetId);
          return {
            assetId: member.assetId,
            image: member.assetId === asset.assetId
              ? OB64.cutsceneAssets.parseImageAsset(bytes, asset)
              : imageForAsset(memberAsset),
          };
        }))
      : OB64.cutsceneAssets.parseImageAsset(bytes, asset);
    assert.strictEqual(image.renderable, true,
      asset.assetId + ' active Stage image must decode to pixels');
    imageCache.set(asset.assetId, image);
    return image;
  }

  function backgroundEntries(state) {
    return state.background.layers.map(layer => {
      const asset = catalog.getImageAsset(layer.assetId);
      return { image: imageForAsset(asset), layer };
    });
  }

  for (const scene of directorScenes) {
    const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene, { hashBytes });
    const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog);
    const document = projected.document;
    const runtime = OB64.cutsceneRuntime.compile(
      document, projected.program, scene, catalog, { z64 });
    OB64.cutsceneRuntime.bind(document, runtime);
    assert.strictEqual(runtime.terminated, true,
      scene.sceneId + ' runtime must reach its native terminal hold');
    assert.strictEqual(runtime.safetyLimited, false,
      scene.sceneId + ' runtime must not hit the safety limit');
    const transformCommands = projected.program.primitives.filter(primitive =>
      (primitive.rawWords[0] >>> 0) === 0x08);
    if (transformCommands.length) {
      runtimeTransformScenes++;
      assert(!runtime.missingInputs.some(message => /^Scene transform /.test(message)),
        scene.sceneId + ' must decode every referenced scene-transform resource');
      assert(runtime.states.some(state => state.transformChannels.some(channel =>
        channel.rotationX !== 0 || channel.rotationY !== 0 ||
        channel.translateX !== 0 || channel.translateY !== 0 ||
        channel.translateZ !== 0 || channel.uniformScale !== 1)),
      scene.sceneId + ' must publish its native transform-channel values');
    }
    const queries = projected.program.primitives.filter(primitive => primitive.query);
    assert.strictEqual(document.branches.length, 1,
      scene.sceneId + ' must not invent alternate movie paths from native query gates');
    assert(queries.every(primitive => projected.program.compositeByNodeId[primitive.id]),
      scene.sceneId + ' must retain every query inside the Director action model');
    nativeQueries += queries.length;

    const duration = OB64.cutscenePreview.sceneDurationFrames(document, 'default');
    const frame = Math.floor((duration - 1) / 2);
    const first = OB64.cutscenePreview.evaluateAtFrame(document, frame, { pathId: 'default' });
    const scrubbed = OB64.cutscenePreview.evaluateAtFrame(document, frame, { pathId: 'default' });
    assert.strictEqual(
      OB64.cutsceneModel.stableStringify(first, 0),
      OB64.cutsceneModel.stableStringify(scrubbed, 0),
      scene.sceneId + ' scrub state must be deterministic'
    );

    const preview = OB64.cutscenePreview.evaluateAtFrame(document, 0, { pathId: 'default' });
    const previewBackgrounds = backgroundEntries(preview);
    const previewActorFrames = OB64.cutsceneSprites.framesForPreview(spriteState, preview);
    const previewStagePropFrames = OB64.cutsceneSprites.framesForStageProps(
      spriteState, preview.background.projection, preview.frame);
    if (previewStagePropFrames.length) {
      nativeStagePropScenes++;
      nativeStagePropCount += previewStagePropFrames.length;
    }
    const renderOptions = {
      backgrounds: previewBackgrounds,
      backgroundProjection: preview.background.projection,
      projection: preview.actorProjection,
      actorFrames: previewActorFrames,
      scenePropFrames: previewStagePropFrames,
      camera: preview.cameraState,
      overlays: preview.overlays,
      colorModulation: preview.sceneColor,
    };
    const rendered = OB64.cutsceneRenderer.renderFrame(document, preview, renderOptions);
    const repeated = OB64.cutsceneRenderer.renderFrame(document, preview, renderOptions);
    assert.strictEqual(hashBytes(rendered.rgba), hashBytes(repeated.rgba),
      scene.sceneId + ' Stage pixels must be deterministic');
    assert.strictEqual(rendered.width, 320);
    assert.strictEqual(rendered.height, 240);
    const backgroundStates = runtime.states.filter(state => state.background.layers.length);
    if (backgroundStates.length) {
      runtimeBackgroundScenes++;
      const visible = backgroundStates.some(state => {
        const blankState = Object.assign({}, state, { actors: [] });
        const common = {
          backgroundProjection: state.background.projection,
          projection: state.actorProjection,
          camera: null,
          overlays: [],
          colorModulation: null
        };
        const withoutBackground = OB64.cutsceneRenderer.renderFrame(
          document, blankState, Object.assign({}, common, { backgrounds: [] }));
        const withBackground = OB64.cutsceneRenderer.renderFrame(
          document, blankState, Object.assign({}, common, {
            backgrounds: backgroundEntries(state)
          }));
        return hashBytes(withoutBackground.rgba) !== hashBytes(withBackground.rgba);
      });
      assert.strictEqual(visible, true,
        scene.sceneId + ' routed background pixels must reach the 320x240 Stage');
      runtimeVisibleBackgroundScenes++;
    }
    runtime.states.forEach(state => state.background.layers.forEach(layer => {
      const asset = catalog.getImageAsset(layer.assetId);
      assert(asset, scene.sceneId + ' runtime background must resolve to a catalogued asset');
      assert.strictEqual(asset.renderable, true,
        scene.sceneId + ' runtime background must resolve to decoded pixels');
      if (runtime.directorMode === 0) {
        assert(Number.isInteger(layer.nativeOrdinal),
          scene.sceneId + ' mode-zero background must retain its native layer ordinal');
        assert.strictEqual(layer.transformChannel, layer.nativeOrdinal,
          scene.sceneId + ' mode-zero background must select its same-numbered transform');
        assert.strictEqual(layer.renderPipeline, 'mode-zero-b5-actor-camera');
        assert.deepStrictEqual(layer.sceneTransform,
          state.transformChannels[layer.nativeOrdinal],
          scene.sceneId + ' mode-zero background must publish its current Director transform');
      }
      runtimeBackgroundAssetIds.add(layer.assetId);
    }));
    if (runtime.states.some(state => state.actorProjection.sourceNodeId)) runtimeCameraScenes++;
    if (runtime.states.some(state => state.actors.some(actor => actor.visible))) runtimeActorScenes++;
    if (runtime.states.some(state => state.dialogue.length)) runtimeDialogueScenes++;
    if (runtime.states.some((state, index) => index > 0 && state.actors.some(actor => {
      const previous = runtime.states[index - 1].actors.find(row => row.id === actor.id);
      return actor.visible && previous && actor.poseFrame !== previous.poseFrame;
    }))) runtimeAnimatedScenes++;

    const actorSpriteHashes = new Map();
    for (const state of runtime.states) {
      for (const actor of state.actors) {
        if (!actor.visible) continue;
        const sprite = OB64.cutsceneSprites.frameForActor(spriteState, actor);
        if (!sprite) {
          if (actor.poseId && spriteState.errors[actor.poseId]) {
            spriteErrors.add(actor.poseId + ': ' + spriteState.errors[actor.poseId]);
          }
          continue;
        }
        let spriteHash = spriteHashCache.get(sprite);
        if (!spriteHash) {
          spriteHash = hashBytes(sprite.rgba);
          spriteHashCache.set(sprite, spriteHash);
        }
        if (!actorSpriteHashes.has(actor.id)) actorSpriteHashes.set(actor.id, new Set());
        actorSpriteHashes.get(actor.id).add(spriteHash);
      }
    }
    if (actorSpriteHashes.size) runtimeSpriteScenes++;
    if (actorSpriteHashes.size) {
      const visibleActorPixels = runtime.states.some(state => {
        const actorFrames = OB64.cutsceneSprites.framesForPreview(spriteState, state);
        const nativeActors = state.actors.filter(actor => actor.visible && actorFrames[actor.id]);
        if (!nativeActors.length) return false;
        const common = {
          backgrounds: [],
          backgroundProjection: state.background.projection,
          projection: state.actorProjection,
          actorFrames,
          camera: state.cameraState,
          overlays: [],
          colorModulation: null
        };
        const withoutActors = OB64.cutsceneRenderer.renderFrame(
          document, Object.assign({}, state, { actors: [] }), common);
        const withActors = OB64.cutsceneRenderer.renderFrame(
          document, Object.assign({}, state, { actors: nativeActors }), common);
        return hashBytes(withoutActors.rgba) !== hashBytes(withActors.rgba);
      });
      assert.strictEqual(visibleActorPixels, true,
        scene.sceneId + ' native Actor pixels must reach the 320x240 Stage');
      runtimeVisibleActorScenes++;
    }
    const pixelAnimated = Array.from(actorSpriteHashes.values())
      .some(hashes => hashes.size > 1);
    if (pixelAnimated) {
      runtimePixelAnimatedScenes++;
    } else if (actorSpriteHashes.size) {
      runtimeStaticPoseScenes++;
      const checkedSelectors = new Set();
      runtime.states.forEach(state => state.actors.forEach(actor => {
        if (!actor.visible || !actorSpriteHashes.has(actor.id)) return;
        if (actor.bodyPoseProgram) {
          assert.strictEqual(actor.bodyPoseProgram.selector, 7,
            scene.sceneId + ' static alternate body pose must use the empty selector-7 program');
          return;
        }
        if (checkedSelectors.has(actor.poseId)) return;
        checkedSelectors.add(actor.poseId);
        const pose = catalog.getPhysicalPoseProgram(actor.bank, actor.animationKey,
          actor.nativeFacing, actor.variantSelector) || catalog.getPoseProgram(actor.poseId);
        assert(pose, scene.sceneId + ' static Actor selector must resolve to a pose program');
        assert.strictEqual(pose.frames.length, 1,
          scene.sceneId + ' may be classified static only when its native pose has one frame');
      }));
    }
  }

  assert.strictEqual(hashBytes(z64), beforeHash, 'viewer corpus pass must not write the ROM');
  assert.strictEqual(nativeQueries, 1249);
  assert.strictEqual(runtimeBackgroundScenes, 42);
  assert.strictEqual(runtimeVisibleBackgroundScenes, 42);
  assert.strictEqual(runtimeBackgroundAssetIds.size, 49);
  assert.strictEqual(nativeStagePropScenes, 2);
  assert.strictEqual(nativeStagePropCount, 22);
  assert.strictEqual(runtimeCameraScenes, 44);
  assert.strictEqual(runtimeActorScenes, 45);
  assert.strictEqual(runtimeVisibleActorScenes, 45);
  assert.strictEqual(runtimeAnimatedScenes, 45);
  assert.strictEqual(runtimeSpriteScenes, 45);
  assert.strictEqual(runtimePixelAnimatedScenes, 34);
  assert.strictEqual(runtimeStaticPoseScenes, 11);
  assert.deepStrictEqual(Array.from(spriteErrors), [],
    'every selected native sprite resource must decode without an art error');
  assert.strictEqual(runtimeDialogueScenes, 52);
  assert.strictEqual(runtimeTransformScenes, 12);
  console.log('PASS all 60 Director scenes terminate; all 42 complete profiled Stages and all 45 native Actor scenes reach the preview, 22 native table-placed scene props render in the two measured mode-2 scenes, 44 scenes use native or profiled launch cameras, 34 show decoded pixel-changing animation, the other 11 use one-frame or empty native poses, and runtime staging scrubs deterministically');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
