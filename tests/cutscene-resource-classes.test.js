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
    ['rom-director:01F4DBBE', 8, 5, 'modeled-termination', 650],
    ['rom-director:01F40860', 8, 2, 'modeled-handoff', 650],
    ['rom-director:01F3F074', 8, 1, 'modeled-handoff', 650],
    ['rom-custom-lz:01FAA4DA', 6, 0, 'modeled-termination', 350],
    ['rom-director:01F93B88', 1, 2, 'modeled-termination', 350],
    ['rom-director:01F93F60', 1, 2, 'modeled-termination', 350],
    ['rom-director:01F70FB6', 7, 9, 'modeled-handoff', 900],
    ['rom-director:01F4015C', 1, 3, 'modeled-termination', 650],
    ['rom-director:01F516AC', 8, 2, 'modeled-handoff', 300],
    ['rom-director:01F820BE', 1, 12, 'modeled-termination', 950],
    ['rom-director:01FA6206', 2, 2, 'modeled-termination', 250],
    ['rom-director:01F8909A', 1, 12, 'modeled-termination', 300]
  ];
  for (const [id, resourceClass, actorCount, ending, tick] of cases.filter(row => process.argv.length < 3 || process.argv.slice(2).includes(row[0]))) {
    const scene = ui.catalog.getScene(id);
    const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene);
    const projection = OB64.cutsceneCodec.projectSceneDocument(scene, source, ui.catalog);
    const contract = OB64.cutsceneRomStart.analyze(projection.program);
    const inherited=['rom-director:01F4015C','rom-director:01F8909A'].includes(id);
    if(inherited)assert.equal(contract.code,'rom-start-inherited-stage');
    else {assert(contract.supported); assert.equal(contract.terminalClass, resourceClass);
      assert(OB64.cutsceneRomStart.supports({ ...scene, assetId: 'unrelated-id' }, projection.program));}
    const doc = await OB64.cutsceneUI.loadScene(rom, ui, scene), run = ui.runtimeByAssetId[id];
    assert(run, JSON.stringify(ui.sourceErrors));
    assert.equal(run.outcome, ending, JSON.stringify(run.missingInputs));
    assert.deepStrictEqual(run.missingInputs, []); assert.equal(run.unresolvedQuery, null);
    assert.equal(Math.max(...run.states.map(state => state.actors.length)), actorCount);
    assert(run.retainedStateBytes < run.limits.maxStateBytes);
    const preview = OB64.cutsceneRuntime.evaluate(run, tick);
    assert.equal(preview.nativeExternal.dialogue.memoryScope, 'resource-records-and-preview-name');
    assert(preview.nativeExternal.dialogue.owners.every(owner => !owner || !('payloadHex' in owner)));
    const storedMemory = preview.nativeExternal.dialogue.memory[0].hex;
    preview.nativeExternal.dialogue.memory[0].hex = 'changed';
    assert.equal(OB64.cutsceneRuntime.evaluate(run, tick).nativeExternal.dialogue.memory[0].hex, storedMemory);
    preview.nativeExternal.dialogue.memory[0].hex = storedMemory;
    let frame;
    if ([5,6].includes(resourceClass)) frame = await OB64.cutsceneUI.capturePreviewFrame(rom, ui, doc,
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
      assert.equal(ui.romStartupByAssetId[id], contract.previewEnvironmentDefault?'preview-party-terrain':'preview-party');
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
    if(![2,6,7].includes(resourceClass))assert(!OB64.cutsceneRomStart.analyze(missingEnvironment).supported);
    if(resourceClass===6){assert.equal(preview.titlePresentation.alpha,255);assert(preview.titlePresentation.revealTicks>0);assert.equal(run.states.length,492);assert(frame.rgba.some((v,i)=>i%4===0&&v>200&&Math.floor(i/4/frame.width)>90));}
    if(inherited){assert.equal(ui.romStartupByAssetId[id],'inherited');assert(run.eventDirectors.every(entry=>entry.terminal));assert(run.previewStartTick>500);assert.equal(run.states[0].actors.length,actorCount);assert.equal(run.eventDirectors.at(-1).enteredTick,run.previewStartTick);}
    if(id==='rom-director:01F4015C')assert(run.trace.some(row=>row.kind==='shared-pose-request'&&row.opcode===18));
    if(id==='rom-director:01F8909A')assert(run.trace.some(row=>row.kind==='top-level-tail-call'&&row.selector===535));
    if(id==='rom-director:01F820BE')assert(run.states.some(state=>state.sceneColorEffect?.amount===1));
    if(id==='rom-director:01FA6206'){assert.equal(preview.specialActorFadeAlpha,255);assert(preview.actors.some(actor=>actor.bank===163));}
    results.push({ id, resourceClass, actors: actorCount, ticks: run.states.length, ending });
    console.log(JSON.stringify(results.at(-1)));
    delete ui.runtimeByAssetId[id];
  }
  const helpScene=ui.catalog.getScene('rom-director:01F3E500');
  await OB64.cutsceneUI.loadScene(rom,ui,helpScene);
  assert.equal(OB64.cutsceneCatalog.displayName(helpScene),"Hugo's Report interface");
  assert(Object.values(ui.sourceErrors).some(message=>message.includes("Hugo's Report")));
  console.log(JSON.stringify({ status: 'pass', scenes: results.length, visibleFrames: results.length }));
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
