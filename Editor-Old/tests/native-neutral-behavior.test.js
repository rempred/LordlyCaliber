'use strict';

// Executes the source-built Neutral module's site entries in a MIPS
// interpreter and checks each retail register contract.
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const { Mips } = require('./lib/mips-interpreter.cjs');

const nativeHarness = fs.readFileSync(path.join(__dirname, 'native-rebuild-runtime-composition.test.js'), 'utf8')
  .split('assert.strictEqual(data.composition.phase4Matrix.stateCount')[0];
const { freshRom } = new Function('require', '__dirname', nativeHarness + '\nreturn { freshRom };')(require, __dirname);
for (const file of ['rom-export-candidate.js', 'native-neutral-data.js', 'native-neutral.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(__dirname, '..', file), 'utf8'), { filename: file });
}

const D = OB64.NATIVE_NEUTRAL_DATA;
const L = D.layout;
const E = D.entries;
const RNG = 0x80092A60;
const STAGED_SCENARIO = 0x8018F481;
const REWARD_MODE = 0x80193670;
const REWARD_TABLE = 0x801EDB1A;

function machineFor(rom, rolls) {
  const plan = OB64.nativeNeutral.prepare(rom, rom.z64.slice());
  const cpu = new Mips();
  cpu.load(L.ramBase, plan.writes[0].bytes);
  const queue = rolls.slice();
  cpu.rngCalls = 0;
  cpu.trap(RNG, m => {
    m.rngCalls++;
    if (!queue.length) throw new Error('unexpected RNG call');
    m.set('v0', queue.shift());
  });
  cpu.set('sp', 0x80300000);
  return cpu;
}

function neutralRom() {
  const rom = freshRom();
  const row1 = rom.neutralEncounters.scenarioRates.find(row => row.runtimeKey === 1 && Number.isInteger(row.slice));
  row1.rateOverride = {
    normal: { mode: 'override', passCount: 720, divisor: 72000 },
    alternate: { mode: 'disabled' },
  };
  return rom;
}

// Rate site: override, disabled, inherit, and out-of-table scenario.
{
  const rom = neutralRom();
  // Helper result 1 selects table branch 0 (the "normal" override).
  let cpu = machineFor(rom, [12345]);
  cpu.w8(STAGED_SCENARIO, 1);
  cpu.set('s0', 50); cpu.set('s1', 72000); cpu.set('v0', 1);
  cpu.call(E.neutral_rate_site);
  assert.deepStrictEqual([cpu.get('s0'), cpu.get('s1'), cpu.get('v0')], [719, 72000, 12345], 'override sets threshold and divisor');

  // Helper result 0 selects the disabled branch: forced retail rejection, no RNG.
  cpu = machineFor(rom, []);
  cpu.w8(STAGED_SCENARIO, 1);
  cpu.set('s0', 10); cpu.set('s1', 72000); cpu.set('v0', 0);
  cpu.call(E.neutral_rate_site);
  assert.deepStrictEqual([cpu.get('s0'), cpu.get('s1'), cpu.get('v0')], [0, 2, 1], 'disabled forces 0 < 1 % 2');
  assert(cpu.get('s0') < cpu.get('v0') % cpu.get('s1'), 'retail comparison rejects');

  // Another scenario inherits the retail thresholds untouched.
  cpu = machineFor(rom, [777]);
  cpu.w8(STAGED_SCENARIO, 2);
  cpu.set('s0', 50); cpu.set('s1', 72000); cpu.set('v0', 1);
  cpu.call(E.neutral_rate_site);
  assert.deepStrictEqual([cpu.get('s0'), cpu.get('s1'), cpu.get('v0')], [50, 72000, 777], 'inherit keeps retail');

  cpu = machineFor(rom, [5]);
  cpu.w8(STAGED_SCENARIO, 200);
  cpu.set('s0', 10); cpu.set('s1', 72000); cpu.set('v0', 0);
  cpu.call(E.neutral_rate_site);
  assert.deepStrictEqual([cpu.get('s0'), cpu.get('s1'), cpu.get('v0')], [10, 72000, 5], 'out-of-table key inherits');
}

// Drop site: weighted class and retail class.
{
  const rom = freshRom();
  const record = rom.creatureDrops.records.find(rec => rec && !rec.isSentinel && rec.isRuntimeActive && rec.classId && rec.slots);
  const other = rom.creatureDrops.records.find(rec => rec && rec !== record && !rec.isSentinel && rec.isRuntimeActive && rec.classId);
  record.slots[0].weight = 0; record.slots[1].weight = 5; record.slots[2].weight = 1;
  // total 6: roll 4 -> slot 1, roll 5 -> slot 2; slot 0 is unreachable.
  for (const [roll, slot] of [[4, 1], [5, 2], [0, 1], [11, 2]]) {
    const cpu = machineFor(rom, [roll]);
    cpu.set('s4', record.classId);
    cpu.call(E.neutral_drop_site);
    assert.strictEqual(cpu.get('v0'), slot, 'weighted roll ' + roll);
  }
  const cpu = machineFor(rom, [0xDEADBEEF]);
  cpu.set('s4', other.classId);
  cpu.call(E.neutral_drop_site);
  assert.strictEqual(cpu.get('v0'), 0xDEADBEEF, 'unweighted class returns the retail RNG value');
}

// Reward site: retail replay of z64 0x001024B4..0x001024D0 and ra + 0x18 resume.
{
  const rom = neutralRom();
  const cpu = machineFor(rom, []);
  cpu.w8(REWARD_MODE, 3);
  cpu.w16(REWARD_TABLE + (7 * 4 + 2) * 2, 0x8123);
  cpu.set('s1', 7); cpu.set('v0', 2); cpu.set('s3', 0);
  const liveRa = 0x80210000;
  cpu.call(E.neutral_reward_site, liveRa + 0x18, 1000, liveRa);
  // call() stops at the resume address; the entry must jump to ra + 0x18.
  assert.strictEqual(cpu.get('s3'), 0x8123, 'retail reward value');
  assert.strictEqual(cpu.get('v0'), 3, 'reward mode byte');
}

// ---------------------------------------------------------------------------
// Neutral Squads and combat behavior.
// ---------------------------------------------------------------------------
const STATE = L.ramBase + L.scratchOffset;
const TOKEN = 0x4E545950;
const PROFILES = L.ramBase + L.profileOffset;
const LIVE = {
  selectionSuccess: 0x801E7A0C,
  classOne: 0x801D15A8, generic: 0x801D1600, epilogue: 0x801D1694, constructor: 0x801D0840,
  persuasionHelper: 0x801D1480, persuasionEpilogue: 0x801D18E8, persuasionRng: 0x8009C7CC,
  actorAt: 0x801C8FE8, filters: [0x801C8E30, 0x801C8E9C, 0x801C8E64, 0x801C8E80], eligible: 0x801C8B68,
  talkTarget: 0x801C940C, retreatEpilogue: 0x801D1A48, menuIterator: 0x801D96A0, roundMode: 0x801C8AFC,
};

function squadRom() {
  const rom = freshRom();
  const row = rom.neutralEncounters.scenarioRates.find(r => r.runtimeKey === 1 && Number.isInteger(r.slice));
  rom.neutralEncounters.customSquads = [{
    profileId: 1, runtimeKey: 1, slice: row.slice, terrainSlot: 2,
    members: [
      { classId: 0x05, levelOffsetRaw: 1, cell: 6, cohort: 'A' },
      { classId: 0x47, levelOffsetRaw: 0, cell: 8, cohort: 'B' },
      { classId: 0x10, levelOffsetRaw: 0, cell: 2, cohort: 'C' },
      { classId: 0x10, levelOffsetRaw: 0, cell: 0, cohort: 'C' },
    ],
    equipment: { A: [0x10, 0x11], B: [0x20, 0], C: [0x30, 0x31] },
    label: 'Bandits!',
    persuasion: { mode: 'fixed', chance: 37, classBonuses: [
      { classId: 0x05, bonus: 15 }, { classId: 0x10, bonus: 25 }, { classId: 0x11, bonus: 40 }] },
    retreat: { hpThreshold: 25 },
    rewards: { slots: [{ raw: 0x8001, weight: 1 }, { raw: 0x0002, weight: 2 }, { raw: 0x0000, weight: 3 }] },
  }];
  return rom;
}

function activate(cpu) {
  const profile = PROFILES + 8;
  cpu.w32(STATE + 8, TOKEN); cpu.w32(STATE + 12, profile);
  cpu.w32(STATE + 16, TOKEN); cpu.w32(STATE + 20, profile);
  return profile;
}

// Selection: match, unmatched nonzero class, and empty pair.
{
  const rom = squadRom();
  let cpu = machineFor(rom, []);
  const profile = PROFILES + 8;
  assert.strictEqual(cpu.r8(PROFILES + 5), 1, 'one typed profile');
  const pointerA = cpu.r32(profile);
  cpu.w8(STAGED_SCENARIO, 1);
  cpu.set('v0', 0x4E); cpu.set('v1', pointerA);
  cpu.call(E.neutral_selection_site, LIVE.selectionSuccess, 5000, 0x801E7CDC);
  assert.strictEqual(cpu.get('v0'), 0x105, 'leader class is staged as 0x100 + class');
  assert.deepStrictEqual([cpu.r32(STATE + 8), cpu.r32(STATE + 12), cpu.r32(STATE + 16), cpu.r32(STATE + 20)],
    [TOKEN, profile, TOKEN, profile], 'custom encounter state starts');

  cpu = machineFor(rom, []);
  cpu.w32(STATE + 16, TOKEN);
  cpu.w8(STAGED_SCENARIO, 2);
  cpu.set('v0', 0x30); cpu.set('v1', pointerA);
  cpu.call(E.neutral_selection_site, LIVE.selectionSuccess, 5000, 0x801E7CDC);
  assert.strictEqual(cpu.get('v0'), 0x130, 'another scenario keeps the retail class');
  assert.strictEqual(cpu.r32(STATE + 16), 0, 'stale state is cleared');

  cpu = machineFor(rom, []);
  cpu.w8(STAGED_SCENARIO, 2);
  cpu.set('v0', 0); cpu.set('v1', pointerA);
  cpu.call(E.neutral_selection_site, 0x801E7CDC, 5000);
  assert.strictEqual(cpu.get('v0'), 0x100, 'empty pair continues with the retail delay-slot value');
}

// Materializer: typed members through the retail constructor, and retail paths.
{
  const rom = squadRom();
  let cpu = machineFor(rom, []);
  activate(cpu);
  const calls = [];
  cpu.trap(LIVE.constructor, m => {
    const a0 = m.get('a0');
    calls.push([a0, m.get('a1'), m.get('a2'), m.get('a3'), m.r32(m.get('sp') + 0x10)]);
    m.w8(a0 + 0x11, 1);
  });
  const source = 0x80196000;
  cpu.set('s1', source); cpu.set('s2', 10); cpu.set('s3', 0x4E);
  cpu.set('s4', 0x44); cpu.set('s7', 0x77);
  const sp = cpu.get('sp');
  cpu.call(E.neutral_materializer_site, LIVE.epilogue, 50000);
  assert.deepStrictEqual(calls.map(c => [c[1], c[2], c[3], c[4]]), [
    [0x05, 11, 0x10, 0x11], [0x47, 10, 0x20, 0], [0x10, 10, 0x30, 0x31], [0x10, 10, 0x30, 0x31]],
  'class, level (base + raw offset & 0xFF), and cohort equipment');
  assert.deepStrictEqual(calls.map(c => c[0]), [0x80195594, 0x801955C8, 0x801955FC, 0x80195630], 'free slots');
  assert.deepStrictEqual([0, 1, 2, 3].map(i => cpu.r8(source + 2 + i)), [1, 2, 3, 4], 'source member IDs');
  assert.deepStrictEqual([0, 1, 2, 3].map(i => cpu.r8(source + 7 + i)), [6, 8, 2, 0], 'source formation cells');
  assert.deepStrictEqual([cpu.get('s4'), cpu.get('s7'), cpu.get('sp')], [0x44, 0x77, sp], 's4-s7 and sp restored');
  assert.strictEqual(cpu.r32(STATE + 8), 0, 'pending token consumed');
  assert.strictEqual(cpu.r32(STATE + 16), TOKEN, 'encounter stays active for combat');

  cpu = machineFor(rom, []);
  cpu.set('s3', 1); cpu.set('a0', 99);
  cpu.call(E.neutral_materializer_site, LIVE.classOne, 1000);
  assert.deepStrictEqual([cpu.get('v0'), cpu.get('a0')], [1, 0], 'retail class-1 branch');
  cpu = machineFor(rom, []);
  cpu.set('s3', 0x4E); cpu.set('a0', 99);
  cpu.call(E.neutral_materializer_site, LIVE.generic, 1000);
  assert.deepStrictEqual([cpu.get('v0'), cpu.get('a0')], [1, 0], 'retail generic branch keeps the delay-slot a0');
}

// Encounter card: MESWIN entry 15 and its text pointer.
{
  const rom = squadRom();
  let cpu = machineFor(rom, []);
  const profile = activate(cpu);
  cpu.set('a1', 400); cpu.set('v0', 41);
  cpu.call(E.neutral_message_site);
  assert.strictEqual(cpu.get('a1'), 15, 'active encounter selects entry 15');
  cpu = machineFor(rom, []);
  cpu.set('a1', 400); cpu.set('v0', 41);
  cpu.call(E.neutral_message_site);
  assert.strictEqual(cpu.get('a1'), 441, 'retail message ID');

  cpu = machineFor(rom, []);
  activate(cpu);
  const text = cpu.r32(profile + 68);
  assert(text >= PROFILES && text < PROFILES + 0x1000, 'card text pointer targets the native table');
  const encoded = OB64.runtimeOverrides.encodeEncounterText('Bandits!');
  assert.deepStrictEqual(Array.from(cpu.ram.subarray(text & 0x1FFFFFFF, (text & 0x1FFFFFFF) + encoded.length)),
    Array.from(encoded), 'card text is the encoded MESWIN string');
  cpu.w8(0x800E7AB8, 0); cpu.w8(0x800E7AB9, 15);
  cpu.set('a0', 0x80197000); cpu.set('s0', 0x80100000); cpu.set('v1', 0x20);
  cpu.call(E.neutral_message_text_site);
  assert.strictEqual(cpu.r32(0x80197004), text, 'entry 15 resolves to the card text');
  cpu.w8(0x800E7AB9, 14);
  cpu.set('v1', 0x20);
  cpu.call(E.neutral_message_text_site);
  assert.strictEqual(cpu.r32(0x80197004), 0x80100020, 'other entries keep the retail pointer');
}

// Persuasion: sole target, leader bonus, second eligible target, and retail.
{
  const rom = squadRom();
  const talk = (rolls, { leaderClass = 0x10, secondEligible = false, active = true } = {}) => {
    const cpu = machineFor(rom, []);
    if (active) activate(cpu);
    const queue = rolls.slice();
    cpu.trap(LIVE.persuasionRng, m => m.set('v0', queue.shift()));
    cpu.trap(LIVE.actorAt, m => m.set('v0', m.get('a0') === 5 ? 0x80198000 : 0));
    LIVE.filters.forEach(f => cpu.trap(f, m => m.set('v0', 0)));
    cpu.trap(LIVE.eligible, m => m.set('v0', secondEligible ? 1 : 0));
    cpu.trap(LIVE.persuasionHelper, m => m.set('v0', 0x55 + m.get('a1')));
    const leader = 0x80199000;
    cpu.w8(leader + 0x4B, leaderClass);
    cpu.set('s0', 0x80197800); cpu.w32(0x80197800 + 0x4C, 3);
    cpu.set('s1', 2); cpu.set('s2', leader);
    return cpu;
  };
  // Chance 37 + 25 (class 0x10) = 62.
  let cpu = talk([161]);
  cpu.call(E.neutral_persuasion_site, LIVE.persuasionEpilogue, 50000);
  assert.deepStrictEqual([cpu.get('v0'), cpu.r32(STATE + 24), cpu.r32(STATE + 28)], [0, 2, 0x80197800], '61 < 62 persuades');
  cpu = talk([162]);
  cpu.call(E.neutral_persuasion_site, LIVE.persuasionEpilogue, 50000);
  assert.deepStrictEqual([cpu.get('v0'), cpu.r32(STATE + 24)], [2, 0], '62 fails without retreat');
  cpu = talk([36], { leaderClass: 0x40 });
  cpu.call(E.neutral_persuasion_site, LIVE.persuasionEpilogue, 50000);
  assert.strictEqual(cpu.get('v0'), 0, 'no bonus: 36 < 37 persuades');
  cpu = talk([0], { secondEligible: true });
  cpu.call(E.neutral_persuasion_site, LIVE.persuasionEpilogue, 50000);
  assert.strictEqual(cpu.get('v0'), 2, 'a second eligible enemy prevents persuasion');
  cpu = talk([], { active: false });
  cpu.call(E.neutral_persuasion_site);
  assert.strictEqual(cpu.get('v0'), 0x58, 'retail helper runs with a1 = 0x4C(s0)');

  cpu = talk([], {});
  cpu.w32(STATE + 28, 0x80197800);
  cpu.call(E.neutral_talk_target_site);
  assert.strictEqual(cpu.get('v0'), 0x80197800, 'result states use the persuaded record');
  cpu = talk([], { active: false });
  cpu.trap(LIVE.talkTarget, m => m.set('v0', 0x1234));
  cpu.call(E.neutral_talk_target_site);
  assert.strictEqual(cpu.get('v0'), 0x1234, 'retail target lookup');
}

// Retreat, action budget, round boundary.
{
  const rom = squadRom();
  const retreat = (hp, max, active = true) => {
    const cpu = machineFor(rom, []);
    if (active) activate(cpu);
    cpu.set('s0', 0x80198800); cpu.w16(0x80198820, hp); cpu.w16(0x80198822, max);
    return cpu;
  };
  let cpu = retreat(25, 100); cpu.call(E.neutral_retreat_site, LIVE.retreatEpilogue, 500);
  assert.strictEqual(cpu.get('v0'), 1, 'threshold 25 >= 25%: retreat');
  cpu = retreat(26, 100); cpu.call(E.neutral_retreat_site, LIVE.retreatEpilogue, 500);
  assert.strictEqual(cpu.get('v0'), 0, '26% stays');
  cpu = retreat(1, 3); cpu.call(E.neutral_retreat_site, LIVE.retreatEpilogue, 500);
  assert.strictEqual(cpu.get('v0'), 0, 'floor(100/3) = 33% stays');
  cpu = retreat(5, 100, false); cpu.call(E.neutral_retreat_site);
  assert.deepStrictEqual([cpu.get('a0'), cpu.get('a1')], [0, 0x00310000], 'retail predicate continues');

  cpu = machineFor(rom, []); activate(cpu); cpu.set('v0', 3); cpu.call(E.neutral_fight_budget_site);
  assert.strictEqual(cpu.get('v0'), 2, 'custom Fight renews the budget');
  cpu = machineFor(rom, []); cpu.set('v0', 3); cpu.call(E.neutral_fight_budget_site);
  assert.strictEqual(cpu.get('v0'), 2, 'retail Fight: 3 - 1');
  cpu = machineFor(rom, []); activate(cpu); cpu.set('v0', 3); cpu.call(E.neutral_talk_budget_site);
  assert.strictEqual(cpu.get('v0'), 2, 'custom Talk renews the budget');
  cpu = machineFor(rom, []); cpu.set('v0', 3); cpu.call(E.neutral_talk_budget_site);
  assert.strictEqual(cpu.get('v0'), 1, 'retail Talk: 3 - 2');

  for (const [active, retailMode, expected] of [[true, 0, 1], [false, 0, 0], [false, 7, 7], [true, 7, 7]]) {
    cpu = machineFor(rom, []);
    if (active) activate(cpu);
    cpu.trap(LIVE.roundMode, m => m.set('v0', retailMode));
    cpu.call(E.neutral_round_site);
    assert.strictEqual(cpu.get('v0'), expected, 'round mode active=' + active + ' retail=' + retailMode);
  }
}

// Command-menu iterator: retail progress stays exact; padding is skipped.
{
  const rom = squadRom();
  const context = 0x80190000;
  const menu = (retailResult, bytes, input) => {
    const cpu = machineFor(rom, []);
    cpu.w32(0x801CE8C0, context);
    cpu.w32(context + 0x814, bytes.length);
    bytes.forEach((b, i) => cpu.w8(context + 0x10 + i, b));
    cpu.trap(LIVE.menuIterator, m => m.set('v0', retailResult === 'same' ? m.get('a0') : retailResult));
    cpu.set('a0', input);
    cpu.call(E.neutral_menu_iterator_site);
    return cpu.get('v0') | 0;
  };
  assert.strictEqual(menu(9, [0, 0, 0], 2), 9, 'advancing retail result is kept');
  assert.strictEqual(menu('same', [0x41, 0xFF, 0x00, 0xFF, 0x22, 0x10], 1), 4, 'padding skipped to next record');
  assert.strictEqual(menu('same', [0x41, 0xFF, 0x00], 1), -1, 'end of stream');
}

// Victory reward: weighted pick, persuasion suppresses it, state cleared.
{
  const rom = squadRom();
  const reward = (roll, persuaded) => {
    const cpu = machineFor(rom, roll == null ? [] : [roll]);
    activate(cpu);
    if (persuaded) cpu.w32(STATE + 24, 2);
    cpu.w8(REWARD_MODE, 4);
    cpu.set('s1', 3); cpu.set('v0', 1); cpu.set('s3', 0xAAAA);
    cpu.call(E.neutral_reward_site, 0x80210018, 5000, 0x80210000);
    return cpu;
  };
  // Weights 1, 2, 3 (total 6): roll 0 -> 0x8001, 1..2 -> 0x0002, 3..5 -> none.
  for (const [roll, raw] of [[0, 0x8001], [2, 0x0002], [5, 0], [6, 0x8001], [7, 0x0002]]) {
    const cpu = reward(roll, false);
    assert.deepStrictEqual([cpu.get('s3'), cpu.get('v0')], [raw, 4], 'custom reward roll ' + roll);
    assert.deepStrictEqual([8, 12, 16, 20, 24, 28].map(o => cpu.r32(STATE + o)), [0, 0, 0, 0, 0, 0], 'state cleared');
  }
  const cpu = reward(null, true);
  assert.strictEqual(cpu.get('s3'), 0, 'a persuaded squad gives no victory reward');
}

console.log('native neutral behavior: all checks passed');
