'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
global.OB64 = {
  className(id) {
    if (id === 0x85) return 'Gladiator (Dio)';
    if (id === 0x02) return 'Fighter';
    return `Class ${id}`;
  },
  actionEditorName(id) { return id === 0x04 ? 'Slash' : `Action ${id}`; },
  animationUI: {
    animationSideLabel() { return 'Player Side'; },
    animationArtVariantLabel() { return 'Base Art'; },
  },
};

vm.runInThisContext(fs.readFileSync(path.join(EDITOR, 'changelog.js'), 'utf8'), {
  filename: 'changelog.js',
});

const animation = {
  spec: { className: 'Gladiator (Dio)', actionName: 'Slash' },
};
const idleAnimation = {
  spec: { className: 'Fighter', actionName: 'Idle / Rest' },
};
const rom = {
  layout: { name: 'US Rev 0' },
  art: {
    animations: {
      byKey: { 'dio-slash': animation },
      idleAnimationsByKey: { 'fighter-idle': idleAnimation },
      selectorCandidates: {},
    },
  },
};
const patch = {
  created_at: '2026-08-16T12:00:00.000Z',
  summary: {
    combat_sprite_art_modified: 4,
    separated_animation_sequences_modified: 1,
  },
  patches: {
    art: {
      schemaVersion: 3,
      avatars: {},
      icons: {},
      animations: {
        first: {
          animation: 'dio-slash', artId: 0x85, width: 26, height: 15,
          children: { 0: {} },
        },
        second: {
          animation: 'dio-slash', artId: 0x88, width: 71, height: 25,
          children: { 0: {}, 2: {} },
        },
      },
      separations: {
        schemaVersion: 2,
        entries: {
          '133:4:0:normal': {
            classId: 0x85,
            actionId: 0x04,
            bodyFlags: 0,
            laneKey: 'normal',
            targetRef: { key: 'dio-slash' },
            sources: { 0: {}, 1: {} },
            frames: [{}, {}, {}],
          },
          '2:-1:0:idle': {
            classId: 0x02,
            actionId: -1,
            bodyFlags: 0,
            laneKey: 'idle',
            targetRef: { key: 'fighter-idle' },
            sources: { 0: {} },
            frames: [{}, {}, {}],
          },
        },
      },
    },
  },
};

const report = OB64.changelog.build(rom, patch);
const section = report.sections.find(row => row.title === 'Combat Sprite Art');
assert(section, 'combat sprite art section must exist');
assert.strictEqual(section.count, 3,
  'two shared sprite edits and two private routes must produce three entries');

const shared = section.entries.find(row => row.title === 'Gladiator (Dio) Slash');
assert(shared, 'shared sequence entry must use the class and action title');
assert(shared.lines.includes('Edited 2 sprite source objects in this sequence.'));
assert(shared.lines.includes('Art 0x85: child 0 at 26x15 pixels.'));
assert(shared.lines.includes('Art 0x88: children 0 and 2 at 71x25 pixels.'));
assert(!section.entries.some(row => / art 0x85$/i.test(row.title)),
  'sprite source IDs must not create separate changelog cards');

const separated = section.entries.find(row =>
  row.title.includes('Player Side') && row.title.includes('Normal Attack'));
assert(separated, 'private sequence must have one route-specific entry');
assert(separated.lines.includes(
  'Created a private editable copy with 3 frames and 2 sprite source objects.'));

const idle = section.entries.find(row =>
  row.title.includes('Fighter Idle / Rest') && row.title.includes('Idle Loop'));
assert(idle, 'private idle sequence must use its idle route title');
assert(idle.lines.includes(
  "Export relocates the private idle loop and points this art route's selector 0x00 to it."));

console.log('PASS changelog groups combat sprite changes by animation sequence');
