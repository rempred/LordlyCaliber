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

const experienceFeature = {
  id: 'experience-size-scale',
  name: 'Experience Size Weight Scale',
  kind: 'percent-scale',
};
OB64.tools = {
  features() { return [experienceFeature]; },
  isParameterized(feature) { return feature === experienceFeature; },
  effectiveWeights(_feature, percent) {
    return [
      { label: 'Size 0', percent: percent * 0.8 },
      { label: 'Size 1 / default', percent: percent },
      { label: 'Size 2', percent: percent * 1.5 },
    ];
  },
};

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
    armySprites: {
      byModelKey: {
        'player-back-ordinary:03': {
          atlasKey: 'player-back-ordinary', modelId: 3,
        },
        'player-back-special:20': {
          atlasKey: 'player-back-special', modelId: 0x20,
        },
      },
      byKey: {
        'player-back-ordinary': {
          label: 'Player / Back · Ordinary',
        },
        'player-back-special': {
          label: 'Player / Back · Special',
        },
      },
    },
  },
};
const patch = {
  created_at: '2026-08-16T12:00:00.000Z',
  summary: {
    combat_sprite_art_modified: 4,
    separated_animation_sequences_modified: 1,
    custom_neutral_squads_modified: 1,
    tools_modified: 1,
  },
  patches: {
    art: {
      schemaVersion: 4,
      avatars: {},
      icons: {},
      armySprites: {
        'player-back-ordinary:03': {
          atlas: 'player-back-ordinary', modelId: 3,
          width: 16, height: 24, retailPlane: true,
        },
        'player-back-special:20': {
          atlas: 'player-back-special', modelId: 0x20,
          width: 16, height: 24, retailPlane: false,
        },
      },
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
    neutral_encounters: {
      custom_squads: {
        '1:0': {
          record: '0501000000000847000000000003000000000000000000000000000000000000000000',
          persuasion: { mode: 'fixed', chance: 10 },
          retreat: { hp_threshold: 25 },
        },
      },
    },
    tools: {
      'experience-size-scale': { schema: 1, percent: 125 },
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

const army = report.sections.find(row => row.title === 'Army Sprites');
assert(army, 'Army sprite section must exist');
assert.strictEqual(army.count, 2);
assert.strictEqual(army.entries[0].title,
  'Player / Back · Ordinary · Model 0x03');
assert(army.entries[0].lines.includes(
  'Edited one existing 16x24 CI8 formation plane.'));
assert(army.entries[0].lines.includes(
  'Export preserves both fixed palettes and rebuilds the complete atlas in place.'));
const customArmy = army.entries.find(row =>
  row.title === 'Player / Back · Special · Model 0x20');
assert(customArmy, 'custom missing Army sprite entry must exist');
assert(customArmy.lines.includes(
  'Added one custom 16x24 player-side CI8 formation plane.'));
assert(customArmy.lines.includes(
  'Export expands and relocates the complete player atlas.'));

const neutral = report.sections.find(row => row.title === 'Neutral Encounters');
assert(neutral, 'neutral encounter section must exist');
const customSquad = neutral.entries.find(row => row.title === 'Custom neutral squad 1:0');
assert(customSquad, 'custom neutral squad entry must exist');
assert(customSquad.lines.includes('Persuasion: 10%'));
assert(customSquad.lines.includes('Retreat: at or below 25% HP'));

const tools = report.sections.find(row =>
  row.title === 'Optional Tools and Quality-of-Life Features');
assert(tools, 'Tools changelog section must exist');
const experienceScale = tools.entries.find(row =>
  row.title === 'Experience Size Weight Scale');
assert(experienceScale, 'Experience percentage must use the generated feature name');
assert(experienceScale.lines.includes('XP size-weight scale: 125%.'));
assert(experienceScale.lines.includes(
  'Effective weights: Size 0 100%, Size 1 / default 125%, Size 2 187.5%.'));

console.log('PASS changelog groups combat sprites and reports custom neutral behavior');
