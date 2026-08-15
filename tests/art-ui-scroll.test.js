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
const weaponBefore = list('animations:weapons:fighter-slash', 0, 743);
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
assert.strictEqual(weaponAfter.scrollLeft, 743);
assert.strictEqual(layersAfter.scrollTop, 238);
assert.strictEqual(layersAfter.scrollLeft, 5);
assert.strictEqual(viewportAfter.scrollTop, 914);

const unknown = list('icons:unknown', 25, 3);
OB64.artUI.restoreBrowserScroll(ui, panel(unknown));
assert.strictEqual(unknown.scrollTop, 25);
assert.strictEqual(unknown.scrollLeft, 3);

const source = fs.readFileSync(path.join(EDITOR, 'art-ui.js'), 'utf8');
const animationSource = fs.readFileSync(path.join(EDITOR, 'animation-ui.js'), 'utf8');
assert(source.includes("data-art-scroll-key', 'avatars"));
assert(source.includes("data-art-scroll-key', 'icons:' + ui.iconPack"));
assert(animationSource.includes("data-art-scroll-key', 'animations:sequence:' + animation.key"));
assert(animationSource.includes("data-art-scroll-key', 'animations:weapons:' +"));
assert(animationSource.includes("data-art-scroll-key', 'animations:layers:' +"));
assert(source.includes("panel.querySelectorAll('[data-art-scroll-key]')"));
assert(source.includes("panel.closest('.content')"));
assert(source.includes('captureBrowserScroll(ui, panel, preserveViewport);'));
assert(source.includes('restoreBrowserScroll(ui, panel, preserveViewport);'));
assert(source.includes('render(panel, rom, options, true);'));

console.log('PASS every Art and Animation scrollbar survives same-context rerenders');
