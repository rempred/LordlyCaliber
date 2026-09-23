const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const editor = path.resolve(__dirname, '..');
const master = process.env.OB64_MASTER_ROM || path.join(editor, '..',
  'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-model.js',
  'cutscene-catalog.js', 'cutscene-director.js', 'cutscene-codec.js',
  'cutscene-authoring.js', 'cutscene-project.js', 'cutscene-export.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}

const sourceBytes = fs.readFileSync(master);
const rom = Uint8Array.from(sourceBytes, (_, index) => sourceBytes[index ^ 1]);
const layout = { id: 'us-rev0' };
const clips = document => document.tracks.flatMap(track => track.clips);

(async () => {
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scene = catalog.getScene('rom-director:01F4F586');
  const source = await OB64.cutsceneCodec.loadSceneSource(rom, scene, { allowModified: true });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  const document = structuredClone(baseline);
  const template = document.native.commands.find(command => command.words[0] === 0xBF);
  assert(template, 'Scene 6 supplies a dialogue window template');
  const selector = template.words[2];
  const before = OB64.cutsceneAuthoring.readDialogue(rom, selector);
  const entry = before.entries.length;
  const windowId = Math.max(...document.native.commands.filter(command =>
    command.words[0] === 0xBF).map(command => command.words[1])) + 1;
  const track = document.tracks.find(row => row.type === 'dialogue');
  const authored = OB64.cutsceneModel.createClip({
    id: 'clip:authored:dialogue:1', kind: 'dialogue', startFrame: 12,
    durationFrames: 90, capability: 'native',
    payload: {
      sourceSystem: 'serifu-authored-native', nativeDialogueAuthored: true,
      nativeTemplateNodeId: template.source.nodeId,
      presentationArchiveSelector: selector, presentationEntrySelector: entry,
      windowId, nativeDelayTicks: 12, speaker: 'Ariosh', text: 'A new line.'
    },
    source: { insertBeforeNodeId: template.source.nodeId }
  });
  track.clips.push(authored);
  const saved = OB64.cutsceneModel.parseSceneDocument(JSON.parse(
    OB64.cutsceneModel.serializeSceneDocument(document, 0)));
  assert.deepStrictEqual(clips(saved).find(clip => clip.id === authored.id).payload,
    authored.payload, 'Project save retains the new dialogue');

  const state = { catalog: { scenes: [scene] }, histories: {},
    originalSerialized: {}, sourceByAssetId: {} };
  state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(document);
  state.originalSerialized[scene.storageId] =
    OB64.cutsceneModel.serializeSceneDocument(baseline, 0);
  state.sourceByAssetId[scene.assetId] = source;
  const candidate = { z64: rom.slice(), layout };
  const plan = await OB64.cutsceneExport.prepare(
    { z64: rom, layout, cutsceneStudio: state }, candidate);
  OB64.cutsceneExport.apply(candidate, plan);
  OB64.cutsceneExport.validateApplied(candidate, plan);
  const after = OB64.cutsceneAuthoring.readDialogue(candidate.z64, selector);
  assert.equal(after.entries.length, before.entries.length + 1);
  assert.equal(after.entries[entry].rawText,
    OB64.cutsceneAuthoring.authoredDialogueRawText(authored.payload));
  before.entries.forEach((row, index) => {
    assert.equal(after.entries[index].rawText, row.rawText,
      'existing dialogue entries survive');
  });

  const reopened = await OB64.cutsceneCodec.loadSceneSource(candidate.z64, scene,
    { allowModified: true });
  const projection = OB64.cutsceneCodec.projectSceneDocument(scene, reopened, catalog);
  const created = projection.ir.nodes.findIndex(node => node.rawWords[0] === 0xBF &&
    node.rawWords[1] === windowId);
  assert(created > 0, 'exported Director stream contains the new window');
  assert.deepStrictEqual(projection.ir.nodes.slice(created - 6, created).map(node =>
    node.rawWords[0]), [1, 0, 0x80000002, 0x80000000, 0x0E, 2]);
  assert.deepStrictEqual(projection.ir.nodes.slice(created + 1, created + 7).map(node =>
    node.rawWords[0]), [0, 0x80000002, 0x80000000, 0x10, 0x06, 0x05]);
  const reopenedClip = clips(projection.document).find(clip =>
    clip.payload.nativeDialogueEditable &&
    clip.payload.presentationEntrySelector === entry);
  assert(reopenedClip, 'exported dialogue remains editable after reopening');
  assert.equal(reopenedClip.payload.rawText, after.entries[entry].rawText);
  assert.equal(sourceBytes[0], fs.readFileSync(master)[0], 'master ROM is unchanged');
  console.log('PASS Scene 6 native dialogue insertion, text append, Project save, ROM export, and reopen.');
})().catch(error => { console.error(error.stack || error); process.exitCode = 1; });
