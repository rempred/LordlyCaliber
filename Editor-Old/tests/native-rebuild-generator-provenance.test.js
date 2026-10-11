'use strict';

const assert = require('assert');
const childProcess = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');

const EDITOR = path.resolve(__dirname, '..');
const GENERATOR = path.join(EDITOR, 'tools', 'generate-native-rebuild-data.js');
const CHECKED_IN_BUNDLE = path.join(EDITOR, 'native-rebuild-data.js');
const HISTORICAL_BUNDLE_COMMIT = 'aaeffde75b394dc2c41162933299d099c317d792';
const generatorApi = require(GENERATOR);

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function firstExisting(label, environmentName, candidates) {
  const target = candidates.filter(Boolean).map(candidate => path.resolve(candidate))
    .find(candidate => fs.existsSync(candidate));
  if (!target) {
    throw new Error('Set ' + environmentName + ' to the accepted ' + label + '.');
  }
  return target;
}

function canonicalText(bytes) {
  return Buffer.from(Buffer.from(bytes).toString('utf8').replace(/\r\n/g, '\n'), 'utf8');
}

const inputRoot = firstExisting('restored native input root', 'OB64_NATIVE_RECOVERY_ROOT', [
  process.env.OB64_NATIVE_RECOVERY_ROOT,
  process.env.OB64_NATIVE_DECOMP,
  path.resolve(EDITOR, '..', 'lordly-native-rebuild-decomp', 'build',
    'lordly-native-recovery', 'r1', 'empty-restore'),
]);
const inputContract = firstExisting('native input contract', 'OB64_NATIVE_INPUT_CONTRACT', [
  process.env.OB64_NATIVE_INPUT_CONTRACT,
  path.join(inputRoot, 'config', 'lordly-native-editor-inputs-v1.json'),
]);

const outputParent = path.resolve(process.env.OB64_NATIVE_GENERATOR_TEST_OUTPUT || os.tmpdir());
fs.mkdirSync(outputParent, { recursive: true });
const outputRoot = fs.mkdtempSync(path.join(outputParent, 'ob64-native-generator-'));
const lfGenerator = path.join(outputRoot, 'generator-lf.js');
const crlfGenerator = path.join(outputRoot, 'generator-crlf.js');
const lfOutput = path.join(outputRoot, 'native-lf.js');
const crlfOutput = path.join(outputRoot, 'native-crlf.js');
const crlfContract = path.join(outputRoot, 'input-contract-crlf.json');
const mutatedContract = path.join(outputRoot, 'input-contract-mutated.json');

const workingSource = fs.readFileSync(GENERATOR);
const canonicalSource = generatorApi.canonicalizeGeneratorSource(workingSource);
const crlfSource = Buffer.from(canonicalSource.toString('utf8').replace(/\n/g, '\r\n'), 'utf8');
fs.writeFileSync(lfGenerator, canonicalSource);
fs.writeFileSync(crlfGenerator, crlfSource);

assert.notStrictEqual(sha256(canonicalSource), sha256(crlfSource),
  'the LF and CRLF fixtures must have different raw identities');
assert.deepStrictEqual(
  generatorApi.identifyGeneratorSource(canonicalSource),
  generatorApi.identifyGeneratorSource(crlfSource),
  'generator provenance must be invariant across LF and CRLF checkouts');

const mutatedSource = Buffer.from(canonicalSource.toString('utf8').replace(
  'browser-safe Phase 1', 'browser-safe accepted Phase 1'), 'utf8');
assert.notStrictEqual(
  generatorApi.identifyGeneratorSource(canonicalSource).sha256,
  generatorApi.identifyGeneratorSource(mutatedSource).sha256,
  'a source-content change must change generator provenance');

const contractSource = canonicalText(fs.readFileSync(inputContract));
fs.writeFileSync(crlfContract,
  Buffer.from(contractSource.toString('utf8').replace(/\n/g, '\r\n'), 'utf8'));
assert.strictEqual(generatorApi.loadInputContract(inputRoot, crlfContract).identity.sha256,
  generatorApi.EXPECTED_INPUT_CONTRACT_SHA256,
  'the input-contract identity must be invariant across LF and CRLF checkouts');
fs.writeFileSync(mutatedContract, Buffer.from(contractSource.toString('utf8').replace(
  'phase5a.editor-inputs.v1', 'phase5a.editor-inputs.changed'), 'utf8'));
assert.throws(() => generatorApi.loadInputContract(inputRoot, mutatedContract),
  /input contract canonical identity drift/,
  'an input-contract content change must fail closed');

function runGenerator(script, output) {
  const result = childProcess.spawnSync(process.execPath, [
    script,
    '--input-root', inputRoot,
    '--input-contract', inputContract,
    '--out', output,
  ], { encoding: 'utf8' });
  if (result.status !== 0) {
    throw new Error('generator failed with exit ' + result.status + ': ' +
      (result.stderr || result.stdout));
  }
  return JSON.parse(result.stdout);
}

const lfResult = runGenerator(lfGenerator, lfOutput);
const crlfResult = runGenerator(crlfGenerator, crlfOutput);
const lfBundleBytes = fs.readFileSync(lfOutput);
const crlfBundleBytes = fs.readFileSync(crlfOutput);
const checkedInBundleBytes = canonicalText(fs.readFileSync(CHECKED_IN_BUNDLE));

assert.deepStrictEqual(lfResult, Object.assign({}, crlfResult, { output: lfResult.output }),
  'LF and CRLF generator reports must differ only by requested output path');
assert.deepStrictEqual(lfBundleBytes, crlfBundleBytes,
  'LF and CRLF checkouts must generate byte-identical native bundles');
const historicalBundleBytes = childProcess.execFileSync('git', ['-C', EDITOR,
  'show', HISTORICAL_BUNDLE_COMMIT + ':native-rebuild-data.js']);
assert.strictEqual(sha256(historicalBundleBytes),
  '354BD5A73CBB103A7C08C200BE354CEA1F1BBAFF6D0DCEA2B5F4FDEE3127984F',
  'the historical bundle fixture must retain its exact reviewed identity');
assert.deepStrictEqual(lfBundleBytes, historicalBundleBytes,
  'the historical v1 bundle must match historical generation');

const generatedBundle = require(lfOutput);
assert.strictEqual(sha256(Buffer.from(JSON.stringify(generatedBundle.payload), 'utf8')),
  generatedBundle.payloadSha256, 'the generated payload hash must verify');
assert.deepStrictEqual(generatedBundle.payload.generator,
  generatorApi.LEGACY_GENERATOR_IDENTITY,
  'the accepted v1 bundle must retain its reviewed generator identity');
assert.strictEqual(generatedBundle.payload.inputContract.sha256,
  generatorApi.EXPECTED_INPUT_CONTRACT_SHA256,
  'the generated bundle must record the versioned input-contract identity');

global.window = global;
global.OB64 = {
  consumableEffects: require(path.join(EDITOR, 'consumable-effects.js')),
};
global.OB64_NATIVE_REBUILD_DATA = generatedBundle;
require(path.join(EDITOR, 'native-rebuild.js'));
const verifiedData = global.OB64.nativeRebuild.verifyData();
assert.deepStrictEqual(verifiedData.generator, generatorApi.LEGACY_GENERATOR_IDENTITY,
  'the runtime must retain historical generator provenance independently of the executing generator');
assert.strictEqual(verifiedData.acceptedSource.commit,
  'e50fe500839288c94c538969acc7a5b966075626',
  'the bundle must authenticate the accepted Phase 5A commit');
assert.strictEqual(verifiedData.consumables.effect.bytes, 1380,
  'the bundle must own the complete Phase 5A effect section');
assert.strictEqual(verifiedData.consumables.descriptions.streamCapacity, 2055,
  'the bundle must retain the accepted consumable description capacity');

// Current recovery inputs are explicit because source runs use unique output roots.
// A missing current input is an error, never permission to skip the current gate.
const currentInputRoot = firstExisting('current v4 input root', 'OB64_NATIVE_CURRENT_INPUT_ROOT', [
  process.env.OB64_NATIVE_CURRENT_INPUT_ROOT,
]);
const currentInputContract = firstExisting('current v4 input contract', 'OB64_NATIVE_CURRENT_INPUT_CONTRACT', [
  process.env.OB64_NATIVE_CURRENT_INPUT_CONTRACT,
  path.join(currentInputRoot, 'source-recovery', 'editor-input-contract-v4.json'),
]);
const correctionReport = firstExisting('current Alignment correction report', 'OB64_ALIGNMENT_CORRECTION_REPORT', [
  process.env.OB64_ALIGNMENT_CORRECTION_REPORT,
]);
const currentContract = JSON.parse(fs.readFileSync(currentInputContract, 'utf8'));
assert.strictEqual(currentContract.schemaVersion, 4);
assert.strictEqual(currentContract.contractId, 'lordly-native-rebuild.phase5a.editor-inputs.source-recovery.v4');
const currentIdentity = generatorApi.loadInputContract(currentInputRoot, currentInputContract).identity;
const currentCrlfContract = path.join(outputRoot, 'current-contract-crlf.json');
fs.writeFileSync(currentCrlfContract, canonicalText(fs.readFileSync(currentInputContract)).toString('utf8').replace(/\n/g, '\r\n'));
assert.deepStrictEqual(generatorApi.loadInputContract(currentInputRoot, currentCrlfContract).identity.sha256,
  currentIdentity.sha256, 'current contract identity must be line-ending invariant');
const invalidCurrentContract = path.join(outputRoot, 'current-contract-unsupported.json');
fs.writeFileSync(invalidCurrentContract, JSON.stringify(Object.assign({}, currentContract, {schemaVersion: 5})));
assert.throws(() => generatorApi.loadInputContract(currentInputRoot, invalidCurrentContract),
  'unsupported current input contracts must reject');
const currentOutput = path.join(outputRoot, 'native-current.js');
const currentRun = childProcess.spawnSync(process.execPath, [GENERATOR,
  '--input-root', currentInputRoot, '--input-contract', currentInputContract,
  '--alignment-correction', correctionReport, '--runtime', '--out', currentOutput,
], {encoding: 'utf8'});
assert.strictEqual(currentRun.status, 0, currentRun.stderr || currentRun.stdout);
const currentResult = JSON.parse(currentRun.stdout);
assert.strictEqual(currentResult.inputContractId, currentContract.contractId);
assert.deepStrictEqual(fs.readFileSync(currentOutput), checkedInBundleBytes,
  'the delivered v10 bundle must exactly match authenticated current v4 generation');
const currentBundle = require(currentOutput);
assert.notStrictEqual(currentBundle.payloadSha256, generatedBundle.payloadSha256,
  'historical and current products must retain distinct identities');
assert.deepStrictEqual(currentBundle.payload.alignmentCorrection.editorGenerator,
  generatorApi.identifyGeneratorSource(workingSource));
assert.deepStrictEqual(currentBundle.payload.consumables.acceptedSource,
  generatedBundle.payload.consumables.acceptedSource);
// Reload the consumer to avoid its verified-data cache from the historical check.
global.OB64_NATIVE_REBUILD_DATA = currentBundle;
delete require.cache[require.resolve(path.join(EDITOR, 'native-rebuild.js'))];
require(path.join(EDITOR, 'native-rebuild.js'));
assert.strictEqual(global.OB64.nativeRebuild.verifyData().alignmentCorrection.version, 10);

console.log(JSON.stringify({
  result: 'pass',
  outputRoot,
  generatorSourceIdentity: lfResult.generatorSourceIdentity,
  generatorCanonicalBytes: lfResult.generatorCanonicalBytes,
  generatorSha256: lfResult.generatorSha256,
  payloadSha256: lfResult.payloadSha256,
  bundleSha256: lfResult.sha256,
  acceptedCommit: lfResult.acceptedCommit,
  acceptedTree: lfResult.acceptedTree,
  inputCount: lfResult.inputCount,
  inputContractSha256: lfResult.inputContractSha256,
  lfRawSha256: sha256(canonicalSource),
  crlfRawSha256: sha256(crlfSource),
  current: currentResult,
}, null, 2));
console.log('\nNative rebuild generator provenance tests passed.');
