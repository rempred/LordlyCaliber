// Generate private-ROM import input with the existing browser editor, and its
// expected dialogue bytes independently of the native importer.
const fs = require('fs'), path = require('path'), vm = require('vm');
const root = path.resolve(__dirname, '../..');
global.window = global;
global.OB64 = {};
for (const name of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-model.js',
  'cutscene-catalog.js', 'cutscene-director.js', 'cutscene-codec.js',
  'cutscene-authoring.js', 'cutscene-project.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(root, name), 'utf8'), { filename: name });
}
(async () => {
  const raw = fs.readFileSync(process.argv[2]);
  const rom = raw[0] === 0x37 ? Uint8Array.from(raw, (_, i) => raw[i ^ 1]) : Uint8Array.from(raw);
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scene = catalog.getScene('rom-director:01F4F586');
  const source = await OB64.cutsceneCodec.loadSceneSource(rom, scene, { allowModified: true });
  const baseline = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog).document;
  const document = structuredClone(baseline);
  const track = document.tracks.find(t => t.clips.some(c => c.payload.nativeDialogueEditable));
  const edited = track.clips.find(c => c.payload.nativeDialogueEditable);
  const selector = edited.payload.presentationArchiveSelector;
  edited.payload.rawText = edited.payload.originalRawText.replace('Godeslas', 'Native import');
  const archive = OB64.cutsceneAuthoring.readDialogue(rom, selector);
  const first = archive.entries.length;
  function append(index, fields) {
    track.clips.push(OB64.cutsceneModel.createClip({
      id: 'clip:authored:dialogue:' + index, kind: 'dialogue', startFrame: 50 + index,
      durationFrames: 90, capability: 'native',
      payload: Object.assign({sourceSystem: 'serifu-authored-native', nativeDialogueAuthored: true,
        nativeTemplateNodeId: edited.source.nodeId, presentationArchiveSelector: selector,
        presentationEntrySelector: index, windowId: 50 + index, nativeDelayTicks: 12}, fields),
      source: {insertBeforeNodeId: edited.source.nodeId}
    }));
  }
  append(first + 1, {speaker: 'Ariosh', text: 'A newly authored dialogue that wraps over thirty columns.\r\n\r\nA second paragraph and a new page.'});
  append(first + 2, {choice: {option1: ' Continue ', option2: 'Return'}});
  const state = {catalog, selectedSceneId: scene.sceneId, histories: {}, originalSerialized: {},
    sourceByAssetId: {}, views: {}, eventBindings: [], romEventBindings: []};
  state.histories[scene.storageId] = OB64.cutsceneModel.createHistory(document);
  state.originalSerialized[scene.storageId] = OB64.cutsceneModel.serializeSceneDocument(baseline, 0);
  state.sourceByAssetId[scene.assetId] = source;
  const project = OB64.cutsceneProject.collect(state);
  const resource = OB64.cutsceneAuthoring.dialogueResources(rom, [document])[0];
  const data = Buffer.from(resource.data), expected = [];
  for (let i = 0; i < data.readUInt32BE(0) / 4; ++i) {
    const at = data.readUInt32BE(i * 4), end = data.indexOf(0, at);
    expected.push(data.subarray(at, end).toString('hex'));
  }
  fs.writeFileSync(process.argv[3], JSON.stringify({project, selector, first, expected}));
})().catch(e => {console.error(e); process.exitCode = 1;});
