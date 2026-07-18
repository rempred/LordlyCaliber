'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

global.window = global;
global.OB64 = {
  CLASS_NAMES: { 1: 'Soldier', 2: 'Amazon', 3: 'Fighter' },
  ITEM_NAMES: { 1: 'Short Sword', 2: 'Long Sword', 3: 'Round Shield' },
  className(id) { return this.CLASS_NAMES[id] || `Class ${id}`; },
  itemName(id) { return this.ITEM_NAMES[id] || `Item ${id}`; },
  TERRAIN_NAMES: { 0: 'Plains', 1: 'Forest' },
  tools: {
    features() { return [{ id: 'counter', name: 'Chaos Frame Counter' }]; }
  },
  scenarioKeyInfo(key) { return { label: key === 6 ? 'Tenne Plains' : `Key ${key}` }; },
  SCENARIO_ESET_DATA: {
    scenarios: [{ runtimeKey: 6, sites: [{ selector: 2, siteName: 'Mulsuk' }] }]
  }
};

const source = fs.readFileSync(path.join(__dirname, '..', 'changelog.js'), 'utf8');
vm.runInThisContext(source, { filename: 'changelog.js' });

function makeRom() {
  const originalClass = {};
  for (let i = 0; i < 72; i++) originalClass[String(i)] = 0;
  return {
    layout: { name: 'US retail header rev 0' },
    strongholds: [
      { index: 0, name: 'Mulsuk', shopIdx: 1, missionId: 2, isObjective: false, population: 500, morale: 40 }
    ],
    consumables: [{ name: 'Heal Leaf' }, { name: 'Heal Seed' }],
    original: {
      shops: [null, { items: [1, 3], consumables: [0] }],
      itemPrices: { 2: 500 },
      itemStats: [null, null, { str: 2 }],
      classDefBytes: [null, null, originalClass],
      neutralEncounters: {
        records: [{ s0: 4, slots: [[1, 0]] }],
        terrainRates: [{ terrainByte: 7, rate: 12, rawLookup: 1 }]
      },
      creatureDrops: [],
      consumables: [{ flagHi: 1, price: 100, flagLo: [0, 1, 2, 3] }],
      statGates: { 1: { str: 20 } },
      neutralGlobalRate: { microBasisPoints: 700 }
    }
  };
}

function makePatch() {
  return {
    created_at: '2026-07-18T12:00:00.000Z',
    summary: {
      shops_modified: 1,
      item_prices_modified: 1,
      item_stats_modified: 1,
      class_defs_modified: 1,
      neutral_slices_modified: 1,
      terrain_rates_modified: 1,
      creature_drop_records_modified: 0,
      consumables_modified: 1,
      stat_gates_modified: 1,
      neutral_global_rate_modified: 1,
      tools_modified: 1,
      squad_overrides_modified: 1,
      scenario_modified: 2
    },
    patches: {
      shops: { 1: { items: [2, 3], consumables: [0, 1] } },
      item_prices: { 2: 750 },
      items: { 2: { str: 5 } },
      classDefs: { 1: { record_index: 2, bytes: { 70: 8 } } },
      neutral_encounters: {
        scenario_slices: { 4: { slots: { 0: [2, 3] } } },
        terrain_rates: { 7: { rate: 20, rawLookup: 2 } }
      },
      creatureDrops: {},
      consumables: { 0: { flagHi: 1, price: 150, flagLo: [0, 1, 2, 3] } },
      statGates: { 1: { str: 30 } },
      neutral_global_rate: { multiplier: 2, percent: 0.14 },
      tools: { counter: true },
      squadOverrides: {
        '6:10': {
          scenario_id: 6,
          edat_id: 10,
          original: '0100000000000100000000000000000000000000000000000000000000000000000000',
          record: '0200000000000103000000000004050000000000000000000000000000000000000000'
        }
      },
      scenario: {
        modifiedEsets: { 6: { filename: 'eset04.bin', rawHex: '00' } },
        modifiedTreasures: {},
        siteAllegiances: { 6: { 2: 'allied' } },
        strongholdFields: {},
        addedSquads: []
      },
      enemies: {}
    }
  };
}

const report = OB64.changelog.build(makeRom(), makePatch(), { projectName: 'my_mod.json' });

assert.equal(report.totalChanges, 13);
assert.ok(report.sections.length >= 8);
assert.match(report.text, /Shop #1 - Mulsuk/);
assert.match(report.text, /Added equipment: Long Sword/);
assert.match(report.text, /Removed equipment: Short Sword/);
assert.match(report.text, /Price: 500 -> 750 Goth/);
assert.match(report.text, /HP growth: 0 \(0x00\) -> 8 \(0x08\)/);
assert.match(report.text, /Terrain slot 1: Soldier .* -> Amazon .* \/ Fighter/);
assert.match(report.text, /Chaos Frame Counter/);
assert.match(report.text, /Scenario key 6 - Tenne Plains/);
assert.match(report.text, /Mulsuk/);
assert.match(report.text, /Initial allegiance set to allied/);
assert.match(report.text, /Save-game edits are separate/);

const empty = OB64.changelog.build(makeRom(), {
  created_at: '2026-07-18T12:00:00.000Z',
  summary: {},
  patches: {}
});
assert.equal(empty.totalChanges, 0);
assert.equal(empty.sections.length, 0);
assert.match(empty.text, /No ROM-project changes are currently recorded/);

console.log('changelog tests passed');
