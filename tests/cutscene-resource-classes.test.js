'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const editor = path.resolve(process.env.OB64_EDITOR_ROOT || path.join(__dirname, '..'));
// Use the same module setup as the authoring workflow, without running its tests.
new Function('require', '__dirname', fs.readFileSync(path.join(__dirname,
  'cutscene-authoring-workflow.test.js'), 'utf8').split('const raw =')[0])(require, __dirname);
const name = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';
const master = [process.env.OB64_MASTER_ROM, path.join(editor, '..', name),
  path.join(editor, '../../OgreBattlel64', name)].filter(Boolean).find(file => fs.existsSync(file));

function saveFrame(file, frame) {
  const chunk = (type, data) => {
    const content = Buffer.concat([Buffer.from(type), data]);
    let crc = 0xffffffff;
    for (const byte of content) { crc ^= byte; for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0); }
    const out = Buffer.alloc(content.length + 8); out.writeUInt32BE(data.length); content.copy(out, 4); out.writeUInt32BE((crc ^ 0xffffffff) >>> 0, out.length - 4); return out;
  };
  const header = Buffer.alloc(13); header.writeUInt32BE(frame.width); header.writeUInt32BE(frame.height, 4); header[8] = 8; header[9] = 6;
  const rows = Buffer.alloc(frame.height * (frame.width * 4 + 1));
  for (let y = 0; y < frame.height; y++) Buffer.from(frame.rgba.buffer, frame.rgba.byteOffset + y * frame.width * 4, frame.width * 4).copy(rows, y * (frame.width * 4 + 1) + 1);
  fs.writeFileSync(file, Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]), chunk('IHDR', header), chunk('IDAT', zlib.deflateSync(rows)), chunk('IEND', Buffer.alloc(0))]));
}

(async () => {
  const raw = fs.readFileSync(master), z64 = Uint8Array.from(raw, (_, i) => raw[i ^ 1]);
  const rom = { z64, archives: OB64.findArchives(z64), layout: { id: 'us-rev0' } };
  const ui = OB64.cutsceneUI.initialize(rom), results = [];
  const cases = [
    ['rom-custom-lz:01FA4FAE', 5, 5, 'modeled-termination', 100],
    ['rom-custom-lz:01FA4C2C', 5, 0, 'modeled-termination', 250],
    ['rom-custom-lz:01F3E836', 5, 2, 'modeled-termination', 400],
    ['rom-director:01F40958', 8, 2, 'modeled-handoff', 250],
    ['rom-director:01F66DE4', 8, 2, 'modeled-termination', 1100],
    ['rom-director:01F4DBBE', 8, 5, 'modeled-termination', 650]
  ];
  for (const [id, resourceClass, actorCount, ending, tick] of cases.filter(row => process.argv.length < 3 || process.argv.slice(2).includes(row[0]))) {
    const scene = ui.catalog.getScene(id);
    const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene);
    const projection = OB64.cutsceneCodec.projectSceneDocument(scene, source, ui.catalog);
    const contract = OB64.cutsceneRomStart.analyze(projection.program);
    assert(contract.supported); assert.equal(contract.terminalClass, resourceClass);
    assert(OB64.cutsceneRomStart.supports({ ...scene, assetId: 'unrelated-id' }, projection.program));
    const doc = await OB64.cutsceneUI.loadScene(rom, ui, scene), run = ui.runtimeByAssetId[id];
    assert(run, JSON.stringify(ui.sourceErrors));
    assert.equal(run.outcome, ending, JSON.stringify(run.missingInputs));
    assert.deepStrictEqual(run.missingInputs, []); assert.equal(run.unresolvedQuery, null);
    assert.equal(Math.max(...run.states.map(state => state.actors.length)), actorCount);
    assert(run.retainedStateBytes < run.limits.maxStateBytes);
    const preview = OB64.cutsceneRuntime.evaluate(run, tick);
    let frame;
    if (resourceClass === 5) frame = await OB64.cutsceneUI.capturePreviewFrame(rom, ui, doc,
      { preview, pass: tick, nodeId: 'resource-class-check', backgroundPolicy: 'require' });
    else {
      const backgrounds = [];
      for (const layer of preview.background.layers) backgrounds.push({ layer,
        image: await OB64.cutsceneUI.decodeImageAsset(rom, ui, ui.catalog.getImageAsset(layer.assetId)) });
      const projection = preview.background.projection;
      frame = OB64.cutsceneRenderer.renderFrame(doc, preview, { backgrounds,
        backgroundProjection: projection, showMovementPaths: false,
        actorFrames: OB64.cutsceneSprites.framesForPreview(ui.spriteState, preview),
        scenePropFrames: OB64.cutsceneSprites.framesForStageProps(ui.spriteState, projection, tick),
        projection: preview.actorProjection, camera: preview.cameraState, overlays: preview.overlays,
        colorModulation: preview.sceneColor, screenTransition: preview.screenTransition });
      OB64.cutsceneUI.composeDialogue(rom, ui, preview, true).rows.forEach(row => OB64.cutsceneDialogueDraw.paint(frame, row));
    }
    assert(!frame.error, frame.error); assert(frame.rgba.some((v, i) => i % 4 !== 3 && v));
    if (id === 'rom-custom-lz:01FA4FAE') {
      const frames = OB64.cutsceneSprites.framesForPreview(ui.spriteState, preview);
      for (const actor of preview.actors) {
        const geometry = OB64.cutsceneRenderer.nativeActorGeometry(actor, preview);
        assert(geometry && geometry.nativePacked);
        const points = frames[actor.id].nativeLayers.flatMap(layer => OB64.cutsceneRenderer.nativeActorLayerGeometry(geometry, layer).polygon);
        assert(points.length);
        assert(Math.max(...points.map(p => p.y)) - Math.min(...points.map(p => p.y)) < 100,
          'the 28-percent actors must not fill the 240-pixel Stage');
      }
    }
    if (resourceClass === 8) {
      assert.equal(ui.romStartupByAssetId[id], 'preview-party');
      assert(OB64.cutsceneUI.playbackTimingLabel(ui, scene).includes('sample party'));
      assert(OB64.cutsceneUI.playbackTimingLabel(ui, scene).includes('first option'));
      assert.equal(run.preservedStageMemory.released, true);
      assert(run.preservedStageMemory.workingBytes < run.preservedStageMemory.workingLimitBytes);
    }
    if (id === 'rom-director:01F40958') assert(run.states.some(state => state.runtime.actorPresentationStatus === 'preview-approximation'));
    if (id === 'rom-director:01F66DE4') assert(run.trace.some(row => row.actions?.some(action => action.service === 'automatic-dialogue-choice')));
    if (id === 'rom-director:01F4DBBE') assert(run.trace.some(row => row.opcode === '0x0000002A'));
    if (process.env.OB64_CUTSCENE_RENDER_DIR) {
      fs.mkdirSync(process.env.OB64_CUTSCENE_RENDER_DIR, { recursive: true });
      saveFrame(path.join(process.env.OB64_CUTSCENE_RENDER_DIR, scene.directorKey.replace(/^0x/, '') + '.png'), frame);
    }
    const missingEnvironment = structuredClone(projection.program); missingEnvironment.primitives.shift();
    assert(!OB64.cutsceneRomStart.analyze(missingEnvironment).supported);
    results.push({ id, resourceClass, actors: actorCount, ticks: run.states.length, ending });
    console.log(JSON.stringify(results.at(-1)));
    delete ui.runtimeByAssetId[id];
  }
  console.log(JSON.stringify({ status: 'pass', scenes: results.length, visibleFrames: results.length }));
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
