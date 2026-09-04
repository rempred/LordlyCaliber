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

const decomp = firstExisting('decomp worktree', 'OB64_NATIVE_DECOMP', [
  process.env.OB64_NATIVE_DECOMP,
  path.resolve(EDITOR, '..', 'lordly-native-rebuild-decomp'),
]);
const parentCandidates = [
  process.env.OB64_PARENT_ROOT,
  path.resolve(EDITOR, '..'),
  path.resolve(EDITOR, '..', '..', 'OgreBattlel64'),
];
const phase3Approval = firstExisting('Phase 3 approval record', 'OB64_PHASE3_APPROVAL', [
  process.env.OB64_PHASE3_APPROVAL,
].concat(parentCandidates.map(root => root && path.join(root, 'docs', 'Plans', 'task-logs',
  'lordly-native-rebuild-phase3-accepted-source-approval-20260902.json'))));
const phase4Approval = firstExisting('Phase 4 approval record', 'OB64_PHASE4_APPROVAL', [
  process.env.OB64_PHASE4_APPROVAL,
].concat(parentCandidates.map(root => root && path.join(root, 'docs', 'Plans', 'task-logs',
  'lordly-native-rebuild-phase4-accepted-source-approval-20260903.json'))));
const phase5aReview = firstExisting('Phase 5A Critical-review record', 'OB64_PHASE5A_REVIEW', [
  process.env.OB64_PHASE5A_REVIEW,
].concat(parentCandidates.map(root => root && path.join(root, 'wiki',
  'after-action-reports',
  '20260904-lordly-native-rebuild-phase5a-native-consumables-critical-review-r1.md'))));

const outputParent = path.resolve(process.env.OB64_NATIVE_GENERATOR_TEST_OUTPUT || os.tmpdir());
fs.mkdirSync(outputParent, { recursive: true });
const outputRoot = fs.mkdtempSync(path.join(outputParent, 'ob64-native-generator-'));
const lfGenerator = path.join(outputRoot, 'generator-lf.js');
const crlfGenerator = path.join(outputRoot, 'generator-crlf.js');
const lfOutput = path.join(outputRoot, 'native-lf.js');
const crlfOutput = path.join(outputRoot, 'native-crlf.js');

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

function runGenerator(script, output) {
  const result = childProcess.spawnSync(process.execPath, [
    script,
    '--decomp', decomp,
    '--phase3-approval', phase3Approval,
    '--phase4-approval', phase4Approval,
    '--phase5a-review', phase5aReview,
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
assert.deepStrictEqual(lfBundleBytes, checkedInBundleBytes,
  'the checked-in native bundle must match the supported generator result');

const generatedBundle = require(lfOutput);
assert.strictEqual(sha256(Buffer.from(JSON.stringify(generatedBundle.payload), 'utf8')),
  generatedBundle.payloadSha256, 'the generated payload hash must verify');
assert.deepStrictEqual(generatedBundle.payload.generator, Object.assign({
  path: 'tools/generate-native-rebuild-data.js',
}, generatorApi.identifyGeneratorSource(canonicalSource)),
  'the generated bundle must record canonical generator provenance');

global.window = global;
global.OB64 = {
  consumableEffects: require(path.join(EDITOR, 'consumable-effects.js')),
};
global.OB64_NATIVE_REBUILD_DATA = generatedBundle;
require(path.join(EDITOR, 'native-rebuild.js'));
const verifiedData = global.OB64.nativeRebuild.verifyData();
assert.strictEqual(verifiedData.generator.sha256, lfResult.generatorSha256,
  'the Editor runtime must accept the regenerated bundle and its generator identity');
assert.strictEqual(verifiedData.acceptedSource.commit,
  'e50fe500839288c94c538969acc7a5b966075626',
  'the bundle must authenticate the accepted Phase 5A commit');
assert.strictEqual(verifiedData.consumables.effect.bytes, 1380,
  'the bundle must own the complete Phase 5A effect section');
assert.strictEqual(verifiedData.consumables.descriptions.streamCapacity, 2055,
  'the bundle must retain the accepted consumable description capacity');

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
  lfRawSha256: sha256(canonicalSource),
  crlfRawSha256: sha256(crlfSource),
}, null, 2));
console.log('\nNative rebuild generator provenance tests passed.');
