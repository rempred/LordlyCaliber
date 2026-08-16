'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
global.OB64 = {
  art: {
    constants: {},
  },
  animationUI: {
    requestAnimationRoute(ui, request) {
      ui.animationRouteRequest = Object.assign({}, request);
    },
  },
};

vm.runInThisContext(fs.readFileSync(path.join(EDITOR, 'art-ui.js'), 'utf8'), {
  filename: 'art-ui.js',
});

function list(key, top, left) {
  let scrollKey = key;
  return {
    scrollTop: top || 0,
    scrollLeft: left || 0,
    getAttribute(name) {
      return name === 'data-art-scroll-key' ? scrollKey : null;
    },
    setAttribute(name, value) {
      if (name === 'data-art-scroll-key') scrollKey = value;
    },
  };
}

function panel(current, viewport) {
  const rows = Array.isArray(current) ? current : [current];
  return {
    querySelector(selector) {
      assert.strictEqual(selector, '[data-art-scroll-key]');
      return rows[0] || null;
    },
    querySelectorAll(selector) {
      assert.strictEqual(selector, '[data-art-scroll-key]');
      return rows;
    },
    closest(selector) {
      assert.strictEqual(selector, '.content');
      return viewport || null;
    },
  };
}

const ui = { browserScroll: {} };
const avatarBefore = list('avatars', 481, 9);
OB64.artUI.captureBrowserScroll(ui, panel(avatarBefore));

const avatarAfter = list('avatars', 0, 0);
OB64.artUI.restoreBrowserScroll(ui, panel(avatarAfter));
assert.strictEqual(avatarAfter.scrollTop, 481);
assert.strictEqual(avatarAfter.scrollLeft, 9);

const equipmentBefore = list('icons:equipment', 732, 0);
OB64.artUI.captureBrowserScroll(ui, panel(equipmentBefore));
const specialBefore = list('icons:special-item', 146, 0);
OB64.artUI.captureBrowserScroll(ui, panel(specialBefore));

const equipmentAfter = list('icons:equipment', 0, 0);
OB64.artUI.restoreBrowserScroll(ui, panel(equipmentAfter));
assert.strictEqual(equipmentAfter.scrollTop, 732);

const specialAfter = list('icons:special-item', 0, 0);
OB64.artUI.restoreBrowserScroll(ui, panel(specialAfter));
assert.strictEqual(specialAfter.scrollTop, 146);

const animationBefore = list('animations:sequence:fighter-slash', 0, 1268);
const weaponBefore = list('animations:weapons:fighter-slash', 743, 0);
const layersBefore = list('animations:layers:fighter-slash:0', 238, 5);
const viewportBefore = list(null, 914, 0);
OB64.artUI.captureBrowserScroll(ui,
  panel([animationBefore, weaponBefore, layersBefore], viewportBefore), true);
const animationAfter = list('animations:sequence:fighter-slash', 0, 0);
const weaponAfter = list('animations:weapons:fighter-slash', 0, 0);
const layersAfter = list('animations:layers:fighter-slash:0', 0, 0);
const viewportAfter = list(null, 0, 0);
OB64.artUI.restoreBrowserScroll(ui,
  panel([animationAfter, weaponAfter, layersAfter], viewportAfter), true);
assert.strictEqual(animationAfter.scrollLeft, 1268);
assert.strictEqual(weaponAfter.scrollTop, 743);
assert.strictEqual(layersAfter.scrollTop, 238);
assert.strictEqual(layersAfter.scrollLeft, 5);
assert.strictEqual(viewportAfter.scrollTop, 914);

const routeUi = {
  browserScroll: { 'animations:weapons:old': { top: 321, left: 0 } },
  subtab: 'avatars', animationKey: 'old', animationFrame: 7,
  animationLayer: 3, animationPaletteIndex: 44, animationIntensity: 12,
  animationWeaponChild: 5, animationWeaponChildren: { old: 5 },
};
const routeState = {
  supported: true,
  selectedTab: 'avatars',
  animations: { supported: true, specs: [], byKey: {} },
  ui: routeUi,
};
const routeScroll = routeUi.browserScroll;
assert.strictEqual(OB64.artUI.openAnimationRoute(routeState, {
  classId: 2, actionId: 6, laneKey: 'blocked',
}), true);
assert.strictEqual(routeState.selectedTab, 'animations');
assert.strictEqual(routeUi.subtab, 'animations');
assert.strictEqual(routeUi.animationFrame, 7);
assert.strictEqual(routeUi.animationLayer, 3);
assert.strictEqual(routeUi.animationWeaponChild, 5);
assert.strictEqual(routeUi.browserScroll, routeScroll);
assert.deepStrictEqual(routeUi.animationRouteRequest, {
  classId: 2, actionId: 6, laneKey: 'blocked',
});
assert.strictEqual(OB64.artUI.openAnimationRoute(routeState, {
  classId: 0x55,
}), true);
assert.deepStrictEqual(routeUi.animationRouteRequest, { classId: 0x55 });

const unknown = list('icons:unknown', 25, 3);
OB64.artUI.restoreBrowserScroll(ui, panel(unknown));
assert.strictEqual(unknown.scrollTop, 25);
assert.strictEqual(unknown.scrollLeft, 3);

const source = fs.readFileSync(path.join(EDITOR, 'art-ui.js'), 'utf8');
const animationSource = fs.readFileSync(path.join(EDITOR, 'animation-ui.js'), 'utf8');
const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
const cssSource = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');
assert(source.includes("data-art-scroll-key', 'avatars"));
assert(source.includes("data-art-scroll-key', 'icons:' + ui.iconPack"));
assert(animationSource.includes("data-art-scroll-key', 'animations:sequence:' + animation.key"));
assert(animationSource.includes("data-art-scroll-key', 'animations:weapons:' +"));
assert(animationSource.includes("data-art-scroll-key', 'animations:layers:' +"));
assert(animationSource.includes("element('aside', 'animation-weapon-sidebar')"));
assert(animationSource.includes("element('div', 'animation-main-column')"));
assert(animationSource.includes("element('span', 'animation-weapon-item-name'"));
assert(animationSource.includes("element('small', 'animation-weapon-fallback'"));
assert(animationSource.includes('searchableClassSelector(classes'));
assert(animationSource.includes("selector('Action', actions"));
assert(animationSource.includes("selector('Art Variant', artRoutes"));
assert(animationSource.includes("selector('Mode', modes"));
assert(animationSource.includes('sequenceDropdown('));
assert(animationSource.includes('function animationSequenceCatalogRows('));
assert(!animationSource.includes('function completeClassVariantRows('));
assert(animationSource.includes("'Body Sprite Sequence'"));
assert(animationSource.includes('function animationPoseOffsetSummary('));
assert(animationSource.includes("badge('Linked', 'linked')"));
assert(animationSource.includes("badge('Edited', 'edited')"));
assert(!animationSource.includes("'Points to ' + M.hex(representative.spec.selector, 2)"));
assert(animationSource.includes("badge('Game fallback', 'warning')"));
assert(animationSource.includes("'Choose a body animation · current game fallback '"));
assert(animationSource.includes('The editor has not selected one for you.'));
assert(animationSource.includes("button('Assign', 'animation-variant-assign'"));
assert(!animationSource.includes("button('Separate and Assign',"));
assert(animationSource.includes("? 'Replace From…' : 'Copy From and Separate…'"));
assert(animationSource.includes('openCopyFromModal('));
assert(animationSource.includes("var sequenceSelect = selectField('Sequence')"));
assert(animationSource.includes('animationClassVariantChoices(rowsForClass())'));
assert(animationSource.includes('animationSequenceCatalogRows(\n        state.animations'));
assert(animationSource.includes(
  'copyFrom.disabled = !targetAnimation ||\n      (!idleTarget && !separation && !pair) ||'));
assert(animationSource.includes(
  "? 'Create a private copy of this idle loop for the selected class and art route.'"));
assert(animationSource.includes('OB64.animationSequences.copyFrom('));
assert(animationSource.includes('OB64.animationSequences.separateAndAssign('));
assert(animationSource.includes("button('Add Layer'"));
assert(animationSource.includes("button('Add Frame'"));
assert(animationSource.includes("button('Copy Frame From…'"));
assert(animationSource.includes("button('Remove Layer'"));
assert(animationSource.includes("button('Remove Frame'"));
assert(animationSource.includes('OB64.art.imageCropRect('));
assert(animationSource.includes('OB64.art.prepareAnimationFrameImport('));
assert(!animationSource.includes('A.imageCropRect('));
assert(!animationSource.includes('A.prepareAnimationFrameImport('));
assert(animationSource.includes("'Complete frame'"));
assert(animationSource.includes("'Selected sprite only'"));
assert(animationSource.includes("'animation-copy-layer-list'"));
assert(animationSource.includes('row === wanted || row.key === wanted.key'));
assert(animationSource.includes("button('Move Layer'"));
assert(animationSource.includes("button('Rotate…'"));
assert(!animationSource.includes("button('Rotate Left'"));
assert(!animationSource.includes("button('Rotate Right'"));
assert(animationSource.includes("button('Resize…'"));
assert(animationSource.includes('openLayerRotateModal('));
assert(animationSource.includes('openLayerResizeModal('));
assert(animationSource.includes('OB64.animationSequences.rotateLayer('));
assert(animationSource.includes('OB64.animationSequences.resizeLayer('));
assert(animationSource.includes("button('Export Animation WebM'"));
assert(animationSource.includes('canvas.captureStream(0)'));
assert(animationSource.includes('OB64.animationSequences.addBlankLayer('));
assert(animationSource.includes('OB64.animationSequences.addBlankFrame('));
assert(animationSource.includes('OB64.animationSequences.copyLayerFrom('));
assert(animationSource.includes('OB64.animationSequences.copyFrameFrom('));
assert(animationSource.includes('OB64.animationSequences.moveFrame('));
assert(animationSource.includes('OB64.animationSequences.moveLayer('));
assert(animationSource.includes('OB64.animationSequences.setLayerPosition('));
assert(animationSource.includes("element('label', 'animation-brush-size-control')"));
assert(animationSource.includes('ui.animationBrushSize'));
assert(animationSource.includes(
  "separation ? 'Replace Sequence' : 'Create Separated Copy'"));
assert(animationSource.includes('setCopyBusy(true);'));
assert(animationSource.includes('await nextBrowserPaint();'));
assert(animationSource.includes("modal.setAttribute('aria-busy', busy ? 'true' : 'false')"));
assert(appSource.includes('await OB64.art.prepareExportAsync('));
assert(appSource.includes("exportProgress.update('Native art · ' + detail"));
assert(animationSource.includes("' of ' + rows.length + ' classes'"));
assert.strictEqual((animationSource.match(/badge\('Selected', 'selected'\)/g) || []).length, 3);
assert((animationSource.match(/setAttribute\('aria-pressed'/g) || []).length >= 3);
assert(cssSource.includes('grid-template-columns: clamp(190px, 19vw, 230px) minmax(0, 1fr);'));
assert(cssSource.includes('grid-template-columns: minmax(160px, 180px) minmax(0, 1fr);'));
assert(cssSource.includes('flex-direction: column;'));
assert(cssSource.includes('overflow-y: auto;'));
assert(cssSource.includes('.animation-frame-card.selected {'));
assert(cssSource.includes('.animation-frame-card.drag-target {'));
assert(cssSource.includes('.animation-layer-card.selected {'));
assert(cssSource.includes('.animation-layer-card.drag-target {'));
assert(cssSource.includes('.animation-copy-layer-choice.selected {'));
assert(cssSource.includes('.animation-remove-action {'));
assert(cssSource.includes('.animation-frame-heading-actions button,'));
assert(cssSource.includes('grid-template-columns: repeat(3, minmax(0, 1fr));'));
assert(cssSource.includes('.animation-edit-canvas-move { cursor: move; }'));
assert(cssSource.includes('.animation-brush-size-control {'));
assert(cssSource.includes('.animation-layer-transform-modal {'));
assert(cssSource.includes('.animation-layer-resize-modal {'));
assert(cssSource.includes('.animation-layer-resize-fields {'));
assert(cssSource.includes('.animation-weapon-card.selected {'));
assert(cssSource.includes('.animation-weapon-item-name {'));
assert(cssSource.includes('.animation-weapon-card .animation-weapon-fallback {'));
assert(cssSource.includes('.animation-corpus-controls {'));
assert(cssSource.includes('.art-badge-warning {'));
assert(cssSource.includes('.art-badge-linked {'));
assert(cssSource.includes('.animation-corpus-field select {'));
assert(cssSource.includes('.animation-variant-dropdown-row {'));
assert(!cssSource.includes('.animation-variant-separate'));
assert(cssSource.includes('.animation-copy-modal {'));
assert(cssSource.includes('.animation-legend-locked::before'));
assert(source.includes("state.animations.byKey['fighter-slash']"));
assert(source.includes("panel.querySelectorAll('[data-art-scroll-key]')"));
assert(source.includes("panel.closest('.content')"));
assert(source.includes('captureBrowserScroll(ui, panel, preserveViewport);'));
assert(source.includes('restoreBrowserScroll(ui, panel, preserveViewport);'));
assert(source.includes('render(panel, rom, options, true);'));
assert(source.includes('OB64.animationUI.render(state, ui, options, rerender, rom)'));
assert(appSource.includes("button.textContent = 'Animations'"));
assert(appSource.includes('openCombatAnimationClass(classId)'));
assert(appSource.includes("statusBar.textContent = 'Opened this class in Art and Animation.'"));
assert(appSource.includes('onAnimationRouteChange: function()'));

console.log('PASS Art and Animation scroll state, corpus selectors, vertical weapon rail, and explicit selections');
