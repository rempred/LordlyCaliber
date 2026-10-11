'use strict';

// Structural UI acceptance for the neutral-squad editor. Runtime and Project
// semantics are covered by neutral-runtime.test.js; this file guards the
// product-facing composition workflow and safe formation rules.

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
let failures = 0;

function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of ['data.js', 'squads-data.js', 'squads.js']) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

const defaultMembers = OB64.makeDefaultNeutralSquadMembers();
check('default Add Squad composition is a valid Knight and Griffin formation',
  Array.isArray(defaultMembers) && defaultMembers.length === 2 &&
  defaultMembers[0].classId === 0x05 && defaultMembers[0].cell === 7 &&
  defaultMembers[0].cohort === 'A' &&
  defaultMembers[1].classId === 0x47 && defaultMembers[1].cell === 2 &&
  defaultMembers[1].cohort === 'B' &&
  OB64.validateNeutralSquadMembers(defaultMembers) === '');

const oneMember = [{ cohort: 'A', classId: 5, levelOffsetRaw: 1, cell: 0 }];
check('neutral composition requires at least two members',
  /two through five/i.test(OB64.validateNeutralSquadMembers(oneMember)));

const duplicateCell = JSON.parse(JSON.stringify(defaultMembers));
duplicateCell[1].cell = duplicateCell[0].cell;
check('neutral composition rejects duplicate formation cells',
  /unique cell from 0 through 8/i.test(OB64.validateNeutralSquadMembers(duplicateCell)));

const adjacentLarge = JSON.parse(JSON.stringify(defaultMembers));
adjacentLarge[1].cell = 8;
check('neutral composition enforces large-unit spacing',
  /Large units cannot sit next/i.test(OB64.validateNeutralSquadMembers(adjacentLarge)));

const overMemberLimit = Array.from({ length: 6 }, (_, index) => ({
  cohort: index === 0 ? 'A' : (index < 4 ? 'B' : 'C'),
  classId: index === 0 ? 5 : (index < 4 ? 6 : 7),
  levelOffsetRaw: 1,
  cell: index,
}));
check('neutral composition rejects more than five materialized members',
  /two through five/i.test(OB64.validateNeutralSquadMembers(overMemberLimit)));

const bugFormation = [
  { cohort: 'A', classId: 0x05, levelOffsetRaw: 1, cell: 6 },
  { cohort: 'B', classId: 0x47, levelOffsetRaw: 0, cell: 8 },
  { cohort: 'C', classId: 0x10, levelOffsetRaw: 0, cell: 2 },
  { cohort: 'C', classId: 0x10, levelOffsetRaw: 0, cell: 0 },
];
check('the corrected Knight Griffin Witch Witch fixture preserves native cells 6,8,2,0',
  OB64.validateNeutralSquadMembers(bugFormation) === '' &&
  bugFormation.map(member => member.cell).join(',') === '6,8,2,0');

const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
const squadSource = fs.readFileSync(path.join(EDITOR, 'squads.js'), 'utf8');
const cssSource = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');

check('Encounters exposes literal Add Squad actions per selected scenario terrain',
  appSource.includes("add.textContent = 'Add Squad'") &&
  appSource.includes('scenarioRow.runtimeKey') && appSource.includes('slot.slotIdx'));
check('Add Squad disables at the runtime profile-table capacity',
  appSource.includes('TYPED_MAX_PROFILES') &&
  appSource.includes('profileCapacityReached') &&
  appSource.includes('add.disabled = !customAvailable || profileCapacityReached'));
check('each custom squad exposes validated encounter-card text with Bandits as the default',
  appSource.includes("label: 'Bandits!'") &&
  appSource.includes("messageInput.className = 'neutral-message-input'") &&
  appSource.includes('normalizeEncounterText(messageInput.value)') &&
  appSource.includes('messageInput.maxLength =') &&
  appSource.includes("title.textContent = profile.label + ' · '") &&
  cssSource.includes('.neutral-message-input'));
check('Add Squad opens a dedicated modal instead of expanding the encounter card',
  appSource.includes('function openCustomNeutralSquadModal(') &&
  appSource.includes("modal.className = 'item-modal neutral-squad-modal'") &&
  appSource.includes('openCustomNeutralSquadModal(newProfile, card, rec, scenarioRow, slot)') &&
  !appSource.includes("tile.classList.add('terrain-custom-squad')"));
check('persuasion UI is one fixed-percent slider that defaults to ten percent',
  appSource.includes("chance.className = 'neutral-persuasion-slider'") &&
  appSource.includes("chance.type = 'range'") &&
  appSource.includes("persuasion: { mode: 'fixed', chance: 10, classBonuses: [] }") &&
  appSource.includes("chance.max = '100'") &&
  !appSource.includes("['inherit', 'Use retail calculation']") &&
  cssSource.includes('.neutral-persuasion-slider-label'));
check('retreat UI uses an HP-threshold slider that defaults to never',
  appSource.includes("retreatCaption.textContent = 'Retreat at or below HP'") &&
  appSource.includes("retreatSlider.type = 'range'") &&
  appSource.includes("retreat: { hpThreshold: 0 }") &&
  appSource.includes("? 'Never' : profile.retreat.hpThreshold + '%'") &&
  appSource.includes("? 'Never retreat' : value + ' percent HP'"));
check('scenario selection exists only for genuinely shared creature pools',
  appSource.includes('if (aliases.length > 1) {') &&
  appSource.includes('card.appendChild(selectorWrap);') &&
  appSource.includes('if (selector) {'));
check('shared cards warn that retail choices are shared but squads and rates are not',
  appSource.includes('Changing either retail creature choice changes encounters for both scenarios.') &&
  appSource.includes('Custom squads and rate overrides remain scenario-specific.') &&
  cssSource.includes('.encounter-shared-warning'));
check('scenario selection rerenders only the direct-rate host and encounter body',
  appSource.includes("scenarioRateHost.innerHTML = ''") &&
  appSource.includes('card._neutralScenarioRow') &&
  appSource.includes('refreshScenarioLayer();'));
check('each scenario exposes one optional override with retail behavior as its untouched state',
  appSource.includes("title: 'Scenario rate override'") &&
  appSource.includes("['inherit', 'Use retail rate']") &&
  appSource.includes('not set; both contexts use retail rates') &&
  !appSource.includes("title: 'Shared slice fallback'") &&
  !appSource.includes("inheritLabel: 'inherits shared slice'"));
check('Project save and load counts include direct rates and custom squads',
  appSource.includes('var scenarioRatesN = patch.summary.neutral_scenario_rates_modified || 0;') &&
  appSource.includes('var customNeutralSquadsN = patch.summary.custom_neutral_squads_modified || 0;') &&
  appSource.includes('(result.applied.neutralScenarioRates || 0)') &&
  appSource.includes('(result.applied.customNeutralSquads || 0)'));
check('rate UI names the two gameplay contexts and their structural states',
  appSource.includes("label: 'Area Investigation', stateLabel: 'state bit 17 set'") &&
  appSource.includes("label: 'In-Scenario Mission', stateLabel: 'state bit 17 clear'") &&
  appSource.includes("{ key: 'normal', label: 'Area Investigation', stateLabel: 'state bit 17 set' }") &&
  appSource.includes("{ key: 'alternate', label: 'In-Scenario Mission', stateLabel: 'state bit 17 clear' }"));
check('neutral squad editor reuses formation grid, groups, drag, and level offsets',
  squadSource.includes('var NEUTRAL_GRID = [2, 1, 0, 5, 4, 3, 8, 7, 6]') &&
  squadSource.includes('neutralGridHtml(source)') &&
  squadSource.includes("data-neutral-grp=\"") &&
  squadSource.includes('data-neutral-level') &&
  squadSource.includes('cell.ondrop'));
check('custom formation validation and status use canonical game cells zero through eight',
  squadSource.includes('member.cell < 0 || member.cell > 8') &&
  squadSource.includes('game-native 0–8 values') &&
  !squadSource.includes('makeDefaultNeutralSquadRecord'));
check('neutral squads expose two existing gear choices for each cohort',
  squadSource.includes('Starting equipment changes') &&
  squadSource.includes("['A', 'B', 'C'].forEach(function(cohort)") &&
  squadSource.includes('a persuaded survivor keeps it') &&
  cssSource.includes('.neutral-item-overrides'));
check('persuasion supports up to three non-stacking current leader-class bonuses',
  appSource.includes('Player leader class bonuses') &&
  appSource.includes('Persuasion rolls only when exactly one eligible enemy remains') &&
  appSource.includes('failed Talk returns to the encounter without forcing a retreat') &&
  appSource.includes('profile.persuasion.classBonuses.length >= 3') &&
  appSource.includes('Bonuses never stack.') &&
  cssSource.includes('.neutral-class-bonus-row'));
check('custom squad rewards are exactly three weighted choices and skip persuasion success',
  appSource.includes('Squad victory reward') &&
  appSource.includes('Choose three weighted outcomes.') &&
  appSource.includes('persuasion awards nothing.') &&
  appSource.includes('profile.rewards.slots.forEach') &&
  cssSource.includes('.neutral-reward-row'));
check('neutral modal contains the formation editor and remains responsive',
  cssSource.includes('.neutral-squad-modal') &&
  cssSource.includes('.neutral-squad-modal-body') &&
  cssSource.includes('.neutral-squad-editor-host .sq-editor-grid') &&
  cssSource.includes('@media (max-width: 760px)'));

console.log(failures ? '\n' + failures + ' FAILURE(S)' : '\nALL TESTS PASSED');
process.exit(failures ? 1 : 0);
