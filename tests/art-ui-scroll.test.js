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
  return {
    scrollTop: top || 0,
    scrollLeft: left || 0,
    getAttribute(name) {
      return name === 'data-art-scroll-key' ? key : null;
    },
  };
}

function panel(current) {
  return {
    querySelector(selector) {
      assert.strictEqual(selector, '[data-art-scroll-key]');
      return current;
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

const unknown = list('icons:unknown', 25, 3);
OB64.artUI.restoreBrowserScroll(ui, panel(unknown));
assert.strictEqual(unknown.scrollTop, 25);
assert.strictEqual(unknown.scrollLeft, 3);

const source = fs.readFileSync(path.join(EDITOR, 'art-ui.js'), 'utf8');
assert(source.includes("data-art-scroll-key', 'avatars"));
assert(source.includes("data-art-scroll-key', 'icons:' + ui.iconPack"));
assert(source.includes('captureBrowserScroll(ui, panel);'));
assert(source.includes('restoreBrowserScroll(ui, panel);'));

console.log('PASS Art avatar and icon selector scroll positions survive rerenders');
