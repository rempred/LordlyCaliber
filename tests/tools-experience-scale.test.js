'use strict';

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

function bytes(hex) {
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  return out;
}

function namedFunctionSource(source, name) {
  const start = source.indexOf('function ' + name + '(');
  if (start < 0) throw new Error('Missing function ' + name);
  const bodyStart = source.indexOf('{', start);
  let depth = 0;
  for (let i = bodyStart; i < source.length; i++) {
    if (source[i] === '{') depth++;
    if (source[i] === '}' && --depth === 0) return source.slice(start, i + 1);
  }
  throw new Error('Unclosed function ' + name);
}

global.window = global;
for (const filename of ['tools-data.js', 'tools.js', 'rom-export-validator.js']) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

const feature = OB64.tools.getFeature('experience-size-scale');
check('generated Tools data registers one percentage feature',
  !!feature && feature.kind === 'percent-scale' && feature.parameter.schema === 1);
check('percentage metadata declares one encoded value and the retail default',
  feature.parameter.default === 100 && feature.parameter.min === 0 &&
  feature.parameter.max === 300 && feature.parameter.step === 1 &&
  feature.parameter.encoding.writeIndex === 0 &&
  feature.parameter.encoding.byteOffsets.join(',') === '2,3,6,7');
check('one slider preserves the three retail Size proportions',
  JSON.stringify(OB64.tools.effectiveWeights(feature, 125).map(row => row.percent)) ===
    JSON.stringify([100, 125, 187.5]));
check('the central registry owns all six ROM and runtime ranges',
  OB64.tools.runtimeRegistry().owners.some(owner => owner.id === feature.id &&
    owner.regions.length === 6));

const clean = new Uint8Array(0x02800000);
for (const registered of OB64.tools.features()) {
  for (const write of registered.writes) clean.set(bytes(write.original), write.offset);
}
const rom = {
  z64: clean.slice(),
  layout: { supportsTools: true, unsupportedTools: {} },
};
OB64.tools.initState(rom);
check('retail bytes initialize the slider at 100 percent',
  rom.tools.initial[feature.id] === 'clean' &&
  OB64.tools.desiredPercent(rom, feature) === 100 &&
  OB64.tools.pendingChanges(rom) === 0);

OB64.tools.setDesiredPercent(rom, feature.id, 125);
check('moving the slider stages one Tool change', OB64.tools.pendingChanges(rom) === 1);
const firstResult = OB64.tools.applyDesired(rom);
check('125 percent installs without requesting a checksum rewrite',
  firstResult.applied.join(',') === feature.name && firstResult.crc === false &&
  firstResult.values[feature.id] === 125 &&
  OB64.tools.parameterValue(rom.z64, feature) === 125,
  JSON.stringify(firstResult));
check('the float32 scale is encoded as MIPS lui/ori immediate halves',
  Buffer.from(rom.z64.slice(0x21928C, 0x219294)).toString('hex').toUpperCase() ===
    '3C013FA034210000');
check('the export validator reads back the selected percentage',
  OB64.romExportValidator._test.validateTools(rom, null, firstResult).details.changedFeatureCount === 1);

OB64.tools.initState(rom);
check('a loaded custom ROM adopts its encoded value without staging a change',
  rom.tools.initial[feature.id] === 'applied' &&
  OB64.tools.desiredPercent(rom, feature) === 125 &&
  OB64.tools.pendingChanges(rom) === 0);

OB64.tools.setDesiredPercent(rom, feature.id, 75);
const updateResult = OB64.tools.applyDesired(rom);
check('a second slider value updates the existing patch in place',
  updateResult.updated.join(',') === feature.name && updateResult.applied.length === 0 &&
  OB64.tools.parameterValue(rom.z64, feature) === 75,
  JSON.stringify(updateResult));

OB64.tools.setDesiredPercent(rom, feature.id, 100);
const removeResult = OB64.tools.applyDesired(rom);
check('Retail 100 percent restores every original byte',
  removeResult.removed.join(',') === feature.name &&
  OB64.tools.featureState(rom.z64, feature) === 'clean' &&
  Buffer.from(rom.z64).equals(Buffer.from(clean)),
  JSON.stringify(removeResult));

const foreign = { z64: clean.slice(), layout: rom.layout };
foreign.z64[feature.writes[2].offset] ^= 0x01;
OB64.tools.initState(foreign);
check('unknown calculator bytes disable the percentage setting',
  foreign.tools.initial[feature.id] === 'foreign');

const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
const patchSource = fs.readFileSync(path.join(EDITOR, 'patch.js'), 'utf8');
const styleSource = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');
const renderToolsSource = appSource.slice(
  appSource.indexOf('function renderTools(panel)'),
  appSource.indexOf('function beginChangeBatch()'));
const sortableSource = appSource.slice(
  appSource.indexOf('function makeSortable(table, options)'),
  appSource.indexOf('function makeClickEditorsKeyboardAccessible'));
check('Tools renders one range control from the single percentage parameter',
  appSource.includes('data-tool-scale-id') &&
  appSource.includes('toolScaleWeightsText(f, desiredScale)') &&
  appSource.includes("OB64.tools.setDesiredPercent(rom, id, Number(e.target.value))"));
check('renderTools wires the slider after inserting its controls',
  renderToolsSource.includes('panel.innerHTML = html;') &&
  renderToolsSource.includes('wireToolScaleControls(panel);'));
check('the slider previews on input and commits dirty state on change',
  appSource.includes('function wireToolScaleControls(panel)') &&
  appSource.includes("scales[s].addEventListener('input'") &&
  appSource.includes("scales[s].addEventListener('change'") &&
  appSource.includes('syncToolScaleCard'));
check('generic table sorting does not own Tools controls',
  !sortableSource.includes('data-tool-scale-id') &&
  !sortableSource.includes('wireToolScaleControls'));

const scaleListeners = {};
const resetListeners = {};
const synchronizedValues = [];
const stagedValues = [];
let committedChanges = 0;
let resetRenders = 0;
const fakeCard = {};
const fakeScale = {
  dataset: { toolScaleId: feature.id },
  value: '125',
  addEventListener(type, listener) { scaleListeners[type] = listener; },
  closest() { return fakeCard; },
};
const fakeReset = {
  dataset: { toolScaleReset: feature.id },
  addEventListener(type, listener) { resetListeners[type] = listener; },
};
const fakePanel = {
  querySelectorAll(selector) {
    if (selector === 'input[data-tool-scale-id]') return [fakeScale];
    if (selector === 'button[data-tool-scale-reset]') return [fakeReset];
    return [];
  },
};
const wireContext = {
  panel: fakePanel,
  rom: {},
  OB64: { tools: {
    getFeature() { return feature; },
    setDesiredPercent(_rom, id, value) {
      stagedValues.push([id, value]);
      return value;
    },
  } },
  syncToolScaleCard(card, receivedFeature, value) {
    if (card === fakeCard && receivedFeature === feature) synchronizedValues.push(value);
  },
  markChanged() { committedChanges++; },
  renderTools(panel) { if (panel === fakePanel) resetRenders++; },
};
vm.runInNewContext(
  namedFunctionSource(appSource, 'wireToolScaleControls') +
    '\nwireToolScaleControls(panel);',
  wireContext);
scaleListeners.input({ target: fakeScale });
check('slider input sends the current thumb value to the live card',
  synchronizedValues.join(',') === '125' &&
  JSON.stringify(stagedValues[0]) === JSON.stringify([feature.id, 125]));
fakeScale.value = '175';
scaleListeners.change({ target: fakeScale });
check('slider change synchronizes and commits the selected value',
  synchronizedValues.join(',') === '125,175' && committedChanges === 1 &&
  JSON.stringify(stagedValues[1]) === JSON.stringify([feature.id, 175]));
resetListeners.click({ target: fakeReset });
check('Retail reset stages 100 percent and rerenders the card',
  JSON.stringify(stagedValues[2]) === JSON.stringify([feature.id, 100]) &&
  committedChanges === 2 && resetRenders === 1);
const outputNode = { textContent: '' };
const weightsNode = { textContent: '' };
const resetNode = { disabled: true };
const statusNode = { className: '', textContent: '' };
const displayCard = {
  querySelector(selector) {
    return {
      '[data-tool-scale-output]': outputNode,
      '[data-tool-scale-weights]': weightsNode,
      '[data-tool-scale-reset]': resetNode,
      '[data-tool-scale-status]': statusNode,
    }[selector] || null;
  },
};
vm.runInNewContext(
  namedFunctionSource(appSource, 'syncToolScaleCard') +
    '\nsyncToolScaleCard(card, feature, 125);',
  {
    card: displayCard,
    feature,
    rom: { z64: clean, tools: { initial: { [feature.id]: 'clean' } } },
    OB64: { tools: {
      featureState() { return 'clean'; },
      parameterValue() { return 100; },
    } },
    formatToolPercent(value) { return value + '%'; },
    toolScaleWeightsText(_feature, value) { return 'weights ' + value; },
    toolScaleStatus() { return { className: 'pending', text: 'pending 125%' }; },
  });
check('live card synchronization replaces the displayed percentage',
  outputNode.textContent === '125%' && weightsNode.textContent === 'weights 125' &&
  resetNode.disabled === false && statusNode.textContent === 'pending 125%');
check('Projects store the percentage as a schema-1 value',
  patchSource.includes('var PATCH_VERSION = 34') &&
  patchSource.includes('toolsOut[toolId] = { schema: 1, percent: desiredValue }') &&
  patchSource.includes('validatedToolPercentages[toolKey] = OB64.tools.validateParameterValue('));
check('the Tools stylesheet includes slider, output, reset, and weight-summary rules',
  styleSource.includes('.tool-scale-control input[type="range"]') &&
  styleSource.includes('.tool-scale-control output') &&
  styleSource.includes('.tool-scale-reset') &&
  styleSource.includes('.tool-scale-weights'));

if (failures) {
  console.error('\n' + failures + ' Experience Size Weight Scale test(s) failed.');
  process.exit(1);
}
console.log('\nAll Experience Size Weight Scale tests passed.');
