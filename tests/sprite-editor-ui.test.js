'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
global.OB64 = {};
for (const filename of ['art.js', 'sprite-library.js', 'sprite-editor-ui.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, filename), 'utf8'), {
    filename,
  });
}

const pixels = new Uint8ClampedArray(3 * 3 * 4);
pixels.set([10, 20, 30, 255], (1 * 3 + 1) * 4);
assert.strictEqual(OB64.spriteEditorUI.flood(
  pixels, 3, 3, 0, 0, [255, 0, 0, 255]
), true);
assert.deepStrictEqual([...pixels.slice(0, 4)], [255, 0, 0, 255]);
assert.deepStrictEqual([...pixels.slice((1 * 3 + 1) * 4, (1 * 3 + 1) * 4 + 4)],
  [10, 20, 30, 255]);

const brush = new Uint8ClampedArray(3 * 3 * 4);
assert.strictEqual(OB64.spriteEditorUI.applyBrush(
  brush, 3, 3, { x: 0, y: 0 }, 3, [1, 2, 3, 4]
), true);
assert.deepStrictEqual([...brush.slice(0, 4)], [1, 2, 3, 4]);
assert.deepStrictEqual([...brush.slice((1 * 3 + 1) * 4, (1 * 3 + 1) * 4 + 4)],
  [1, 2, 3, 4]);
assert.deepStrictEqual([...brush.slice((2 * 3 + 2) * 4, (2 * 3 + 2) * 4 + 4)],
  [0, 0, 0, 0]);

const templateRom = {
  art: { animations: { specs: [
    { key: 'fighter-slash-base', canvas: { width: 41, height: 31 },
      frames: [{}], spec: { classId: 1, actionId: 4,
        className: '\x0eFighter\x10cHero\x0f', actionName: 'Slash' } },
    { key: 'fighter-slash-other-route', canvas: { width: 41, height: 31 },
      frames: [{}], spec: { classId: 1, actionId: 4,
        className: '\x0eFighter\x10cHero\x0f', actionName: 'Slash' } },
    { key: 'fighter-slash-wide', canvas: { width: 76, height: 58 },
      frames: [{}], spec: { classId: 1, actionId: 4,
        className: '\x0eFighter\x10cHero\x0f', actionName: 'Slash' } },
  ] } },
  animationSequences: { separations: {} },
};
const templates = OB64.spriteEditorUI.knownSpriteTemplates(templateRom);
assert.deepStrictEqual(templates.slice(0, 4).map(template => [
  template.id, template.width, template.height,
]), [
  ['class-avatar', 40, 48],
  ['item-icon', 16, 16],
  ['army-formation-small', 16, 24],
  ['army-formation-large', 32, 28],
]);
assert.deepStrictEqual(templates.slice(4).map(template => [
  template.kind, template.width, template.height,
]), [
  ['frame', 41, 31],
  ['frame', 76, 58],
]);
assert(templates[4].label.includes('Fighter Hero'));
assert.strictEqual(OB64.spriteEditorUI.spriteImageColorLimit({
  provenance: { template: 'class-avatar' },
}), 80);
assert.strictEqual(OB64.spriteEditorUI.spriteImageColorLimit({
  provenance: { template: 'item-icon' },
}), 255);
assert.strictEqual(OB64.spriteEditorUI.spriteImageColorLimit({
  provenance: { template: 'army-formation-small' },
}), 256);

const importedPixels = new Uint8ClampedArray([
  255, 0, 0, 255,
  0, 0, 255, 128,
  0, 0, 0, 0,
]);
const preparedImport = OB64.art.prepareSpriteImageImport(
  importedPixels, 3, 1, 3, 1, {
    resizeMode: 'nearest',
    panX: 0.5,
    panY: 0.5,
    maximumColors: 1,
    dither: true,
  }
);
assert.strictEqual(preparedImport.rgba.length, 3 * 1 * 4);
assert.deepStrictEqual([
  preparedImport.rgba[3],
  preparedImport.rgba[7],
  preparedImport.rgba[11],
], [255, 128, 0]);
assert.strictEqual(preparedImport.sourceNativeColorCount, 2);
assert.strictEqual(preparedImport.colorCount, 1);
assert.strictEqual(preparedImport.quantized, true);
assert.strictEqual(preparedImport.dithered, true);

const cropSource = new Uint8ClampedArray(4 * 2 * 4);
for (let y = 0; y < 2; y++) {
  for (let x = 0; x < 4; x++) {
    cropSource.set([x * 40, y * 80, 0, 255], (y * 4 + x) * 4);
  }
}
const resizedImport = OB64.art.prepareSpriteImageImport(
  cropSource, 4, 2, 2, 2, {
    resizeMode: 'smooth',
    panX: 1,
    panY: 0.5,
    maximumColors: 256,
  }
);
assert.strictEqual(resizedImport.rgba.length, 2 * 2 * 4);
assert.strictEqual(resizedImport.resizeMode, 'smooth');
assert.strictEqual(resizedImport.crop.width, 2);
assert.strictEqual(resizedImport.crop.height, 2);
assert.strictEqual(resizedImport.crop.x, 2);

const baseCrop = OB64.art.imageCropRect(4, 2, 2, 2, 0.5, 0.5);
assert.deepStrictEqual([
  baseCrop.x, baseCrop.y, baseCrop.width, baseCrop.height, baseCrop.zoom,
  baseCrop.horizontalPanAvailable, baseCrop.verticalPanAvailable,
], [1, 0, 2, 2, 1, true, false]);
const zoomedInCrop = OB64.art.imageCropRect(4, 2, 2, 2, 0.5, 0.5, 2);
assert.deepStrictEqual([
  zoomedInCrop.x, zoomedInCrop.y,
  zoomedInCrop.width, zoomedInCrop.height,
  zoomedInCrop.horizontalPanAvailable, zoomedInCrop.verticalPanAvailable,
], [1.5, 0.5, 1, 1, true, true]);
const zoomedOutCrop = OB64.art.imageCropRect(4, 2, 2, 2, 0.5, 0.5, 0.5);
assert.deepStrictEqual([
  zoomedOutCrop.x, zoomedOutCrop.y,
  zoomedOutCrop.width, zoomedOutCrop.height,
  zoomedOutCrop.horizontalPanAvailable, zoomedOutCrop.verticalPanAvailable,
], [0, -1, 4, 4, false, true]);

const opaqueRedSource = new Uint8ClampedArray(2 * 2 * 4);
for (let pixel = 0; pixel < 4; pixel++) {
  opaqueRedSource.set([255, 0, 0, 255], pixel * 4);
}
const paddedImport = OB64.art.prepareSpriteImageImport(
  opaqueRedSource, 2, 2, 4, 4, {
    resizeMode: 'smooth', zoom: 0.5, maximumColors: 256,
  }
);
const paddedAlpha = [];
for (let pixel = 0; pixel < 16; pixel++) {
  paddedAlpha.push(paddedImport.rgba[pixel * 4 + 3]);
}
assert.deepStrictEqual(paddedAlpha, [
  0, 0, 0, 0,
  0, 255, 255, 0,
  0, 255, 255, 0,
  0, 0, 0, 0,
]);
assert.strictEqual(paddedImport.outputNonOpaquePixels, 12);
const paddedAnimation = OB64.art.prepareAnimationFrameImport(
  opaqueRedSource, 2, 2, 4, 4, {
    resizeMode: 'smooth', zoom: 0.5,
  }
);
assert.strictEqual(paddedAnimation.transparentPixels, 12);
assert.deepStrictEqual(Array.from(paddedAnimation.intensity), [
  0, 0, 0, 0,
  0, 15, 15, 0,
  0, 15, 15, 0,
  0, 0, 0, 0,
]);

const defaultUpscale = OB64.art.prepareSpriteImageImport(
  opaqueRedSource, 2, 2, 8, 8, {
    resizeMode: 'smooth', maximumColors: 256,
  }
);
assert.strictEqual(defaultUpscale.rgba[3], 255);
assert.strictEqual(defaultUpscale.rgba[(8 * 8 - 1) * 4 + 3], 255);
const paddedUpscale = OB64.art.prepareSpriteImageImport(
  opaqueRedSource, 2, 2, 8, 8, {
    resizeMode: 'smooth', zoom: 0.5, maximumColors: 256,
  }
);
assert.strictEqual(paddedUpscale.rgba[3], 0);
assert.strictEqual(paddedUpscale.rgba[(3 * 8 + 3) * 4 + 3], 255);

const avatarBackground = OB64.art.rgba5551Word(0, 0, 31, true);
const paddedAvatar = OB64.art.prepareAvatarImport(
  opaqueRedSource, 2, 2, {
    resizeMode: 'nearest', zoom: 0.5, backgroundWord: avatarBackground,
  }
);
assert.strictEqual(paddedAvatar.words[0], avatarBackground);
assert.strictEqual(paddedAvatar.words[24 * 40 + 20],
  OB64.art.rgba5551Word(31, 0, 0, true));

const iconColorSource = new Uint8ClampedArray(16 * 16 * 4);
for (let color = 0; color < 256; color++) {
  iconColorSource.set(OB64.art.rgba5551(OB64.art.rgba5551Word(
    color & 31, (color >>> 5) & 31, 0, true
  )), color * 4);
}
const preparedIconImport = OB64.art.prepareSpriteImageImport(
  iconColorSource, 16, 16, 16, 16, {
    resizeMode: 'nearest',
    maximumColors: OB64.spriteEditorUI.spriteImageColorLimit({
      provenance: { template: 'item-icon' },
    }),
  }
);
assert.strictEqual(preparedIconImport.sourceNativeColorCount, 256);
assert.strictEqual(preparedIconImport.colorCount, 255);
assert.strictEqual(preparedIconImport.quantized, true);

function scrollNode(key, top, left) {
  return {
    scrollTop: top,
    scrollLeft: left,
    getAttribute(name) {
      return name === 'data-sprite-scroll-key' ? key : null;
    },
    setAttribute(name, value) {
      if (name === 'data-sprite-scroll-key') key = value;
    },
  };
}

function panel(rows, viewport) {
  return {
    querySelectorAll(selector) {
      assert.strictEqual(selector, '[data-sprite-scroll-key]');
      return rows;
    },
    closest(selector) {
      assert.strictEqual(selector, '.content');
      return viewport || null;
    },
  };
}

const ui = { scroll: {} };
const beforeLibrary = scrollNode('sprite:library', 420, 3);
const beforeFrames = scrollNode('sprite:frames:sprite-1', 0, 725);
const beforeViewport = scrollNode(null, 880, 0);
OB64.spriteEditorUI.captureScroll(
  panel([beforeLibrary, beforeFrames], beforeViewport), ui, true
);
const afterLibrary = scrollNode('sprite:library', 0, 0);
const afterFrames = scrollNode('sprite:frames:sprite-1', 0, 0);
const afterViewport = scrollNode('sprite:viewport', 0, 0);
OB64.spriteEditorUI.restoreScroll(
  panel([afterLibrary, afterFrames], afterViewport), ui, true
);
assert.deepStrictEqual([afterLibrary.scrollTop, afterLibrary.scrollLeft], [420, 3]);
assert.deepStrictEqual([afterFrames.scrollTop, afterFrames.scrollLeft], [0, 725]);
assert.strictEqual(afterViewport.scrollTop, 880);

const indexSource = fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8');
const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
const librarySource = fs.readFileSync(path.join(EDITOR, 'sprite-library.js'), 'utf8');
const uiSource = fs.readFileSync(path.join(EDITOR, 'sprite-editor-ui.js'), 'utf8');
const artModelSource = fs.readFileSync(path.join(EDITOR, 'art.js'), 'utf8');
const artSource = fs.readFileSync(path.join(EDITOR, 'art-ui.js'), 'utf8');
const animationSource = fs.readFileSync(path.join(EDITOR, 'animation-ui.js'), 'utf8');
const patchSource = fs.readFileSync(path.join(EDITOR, 'patch.js'), 'utf8');
const compatibilitySource = fs.readFileSync(
  path.join(EDITOR, 'rom-compatibility.js'), 'utf8');
const changelogSource = fs.readFileSync(path.join(EDITOR, 'changelog.js'), 'utf8');
const armyModelSource = fs.readFileSync(path.join(EDITOR, 'army-sprites.js'), 'utf8');
const armyUiSource = fs.readFileSync(path.join(EDITOR, 'army-sprite-ui.js'), 'utf8');
const cssSource = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');
const readmeSource = fs.readFileSync(path.join(EDITOR, 'README.md'), 'utf8');

assert(indexSource.includes('<button data-tab="sprites">Sprite Editor</button>'));
assert(indexSource.includes('<div class="tab-panel" id="panel-sprites"></div>'));
assert(indexSource.indexOf('<script src="sprite-library.js"></script>') <
  indexSource.indexOf('<script src="patch.js"></script>'));
assert(indexSource.indexOf('<script src="sprite-editor-ui.js"></script>') <
  indexSource.indexOf('<script src="app.js"></script>'));
assert(indexSource.indexOf('<script src="army-sprites.js"></script>') >
  indexSource.indexOf('<script src="art.js"></script>'));
assert(indexSource.indexOf('<script src="army-sprite-ui.js"></script>') >
  indexSource.indexOf('<script src="art-ui.js"></script>'));
assert(indexSource.indexOf('<script src="army-sprite-ui.js"></script>') <
  indexSource.indexOf('<script src="app.js"></script>'));

assert(appSource.includes("id: 'sprite-library', label: 'Sprite Editor Project library'"));
assert(appSource.includes("case 'sprites':"));
assert(appSource.includes('OB64.spriteEditorUI.render(panel, rom'));
assert(appSource.includes("activateTab('art')"));
assert(appSource.includes('sprite_library_assets'));
assert(appSource.includes('result.applied.spriteLibrary'));

assert(librarySource.includes("var FILE_FORMAT = 'lordlycaliber-sprite-asset'"));
assert(librarySource.includes('MAX_DIMENSION: MAX_DIMENSION'));
assert(librarySource.includes('knownSpriteFormats: knownSpriteFormats'));
assert(librarySource.includes('collectProjectPayload: collectProjectPayload'));
assert(librarySource.includes('prepareProjectPayload: prepareProjectPayload'));
assert(librarySource.includes('applyPreparedProjectPayload: applyPreparedProjectPayload'));
assert(librarySource.includes('copyPart: copyPart'));

for (const tool of ['Pencil', 'Eraser', 'Fill', 'Eyedropper', 'Replace Color',
  'Select', 'Undo', 'Redo', 'Flip H', 'Flip V', 'Rotate Left', 'Rotate Right',
  'Resize Canvas…']) {
  assert(uiSource.includes(tool), 'missing Sprite Editor tool: ' + tool);
}
assert(uiSource.includes("element('h3', '', 'Frame Collage')"));
assert(uiSource.includes("['avatar', 'Class avatars']"));
assert(uiSource.includes("['icon', 'Item icons']"));
assert(uiSource.includes("['combat', 'Combat animations']"));
assert(uiSource.includes("['cutscene', 'Cutscene actors']"));
assert(uiSource.includes("['army', 'Army sprites']"));
assert(uiSource.includes('OB64.cutsceneSprites.actorSequence('));
assert(uiSource.includes('OB64.armySpriteUI.sourceForModel('));
assert(uiSource.includes("Import PNG/JPEG"));
assert(uiSource.includes("element('span', '', 'Sprite target')"));
assert(!uiSource.includes("field('Width', 'number'"));
assert(!uiSource.includes("field('Height', 'number'"));
assert(uiSource.includes("zoomInput.min = '1'"));
assert(uiSource.includes('Math.max(asset.width, asset.height) > 512'));
assert(uiSource.includes("Import Asset File"));
assert(uiSource.includes("Save Layer as Sprite"));
assert(uiSource.includes("Save Frame as Asset"));
assert(uiSource.includes("Save Sequence as Asset"));
assert(uiSource.includes("Import Image into Layer\\u2026"));
assert(uiSource.includes("'Prepare Layer Image'"));
assert(uiSource.includes('OB64.art.prepareSpriteImageImport('));
assert(uiSource.includes("'Image zoom'"));
assert(uiSource.includes('zoom: settings.zoom'));
assert(uiSource.includes("'Horizontal crop position'"));
assert(uiSource.includes('Ordered dithering'));
assert(uiSource.includes('Item icons allow 255 opaque colors.'));
assert(uiSource.includes('L.replaceLayerPixels('));
assert(uiSource.includes("Export Frame PNG"));
assert(uiSource.includes("Export Sequence WebM"));
assert(uiSource.includes("getContext('2d', { alpha: true })"));
assert(uiSource.includes("data-sprite-scroll-key', 'sprite:frames:' + asset.id"));
assert(uiSource.includes("captureScroll(panel, ui, preserveViewport);\n    panel.innerHTML = '';"));
assert(artModelSource.includes('prepareSpriteImageImport: prepareSpriteImageImport'));
assert(artModelSource.includes('options.panY, options.zoom'));
assert(artSource.includes("'Image zoom'"));
assert(animationSource.includes("'Image zoom'"));
assert(armyUiSource.includes("'Image zoom'"));
assert(armyModelSource.includes('zoom: options.zoom'));

assert(artSource.includes("Import from Sprite Library…"));
assert(artSource.includes("actionLabel: 'Convert to Avatar'"));
assert(artSource.includes("actionLabel: 'Convert to Item Icon'"));
assert(animationSource.includes("Import Library Frame…"));
assert(animationSource.includes("Import Library Sprite…"));
assert(animationSource.includes("Import Library Sequence…"));
assert(animationSource.includes("Replace from Library…"));
assert(animationSource.includes('importLibrarySequence('));
assert(animationSource.includes('importLibrarySpritePixels('));
assert(animationSource.includes('singleLayerPixels: singleLayerPixels'));

assert(patchSource.includes('var PATCH_VERSION = 34;'));
assert(patchSource.includes('spriteLibrary: spriteLibraryOut'));
assert(patchSource.includes('sprite_library_assets: spriteLibraryChanges'));
assert(patchSource.includes('preparedSpriteLibrary'));
assert(compatibilitySource.includes("sprites: ['native-art', 'sprite-library']"));
assert(changelogSource.includes("addSection(sections, 'Sprite Library', entries)"));
assert(armyModelSource.includes('retailModelCount: 56, targetModelCount: 56'));
assert(armyModelSource.includes('retailModelCount: 25, targetModelCount: 26'));
assert(armyUiSource.includes("element('span', '', 'Atlas')"));
assert(armyUiSource.includes("['classes', 'Classes']"));
assert(armyUiSource.includes("'Formation sprite dimensions'"));
assert(armyUiSource.includes('M.setClassModelSize('));
assert(armyUiSource.includes('M.syncClassModelSizes('));
assert(armyModelSource.includes('setClassModelSize: setClassModelSize'));
assert(appSource.includes('onClassDefinitionChange: function()'));
assert(armyUiSource.includes("['player', 'Player / Back']"));
assert(armyUiSource.includes('Create Blank Player Sprite'));
assert(armyUiSource.includes('Copy Enemy Sprite'));
assert(armyUiSource.includes('No player sprite'));
assert(armyUiSource.includes('Import PNG/JPEG'));
assert(armyUiSource.includes('Ordered dithering'));
assert(armyUiSource.includes('both fixed palettes'));
assert(cssSource.includes('.sprite-editor-shell {'));
assert(cssSource.includes('.sprite-frame-strip,'));
assert(cssSource.includes('.sprite-edit-canvas {'));
assert(readmeSource.includes('| Sprite Editor |'));

console.log('PASS Sprite Editor tab, tools, sources, persistence, integration, and UI state');
