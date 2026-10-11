'use strict';

const assert = require('assert');
const childProcess = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ACCEPTED_COMMIT = '4106740a52b0df6b3d8528a201ea066d7d76ad44';
const ACCEPTED_TREE = '7931c1ac7e9226f7f1d95445e6d796a8810daa9b';
const ACCEPTED_PAYLOAD_SHA256 =
  'CEC4C606CD03F7D7968632BB868F0CC3CD43C9EC7A4E0D31FC230A213A9E6CEC';
const EXPECTED_INPUT_CONTRACT_SHA256 =
  'A7BE40F1F386239BCA42AAACBF60CCAF0B673051ACD74DFE90D50BA53C11223E';
const GENERATED_BUNDLE = path.resolve(process.env.OB64_NATIVE_BUNDLE ||
  path.join(EDITOR, 'native-rebuild-data.js'));

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function git(args) {
  return childProcess.execFileSync('git', ['-C', EDITOR].concat(args), {
    encoding: 'utf8',
    maxBuffer: 2 * 1024 * 1024,
    stdio: ['ignore', 'pipe', 'pipe'],
  });
}

function loadSource(source, filename) {
  const sandbox = { module: { exports: {} } };
  sandbox.exports = sandbox.module.exports;
  sandbox.globalThis = sandbox;
  vm.runInNewContext(source, sandbox, { filename });
  return JSON.parse(JSON.stringify(sandbox.module.exports));
}

function gamePayload(payload) {
  return {
    acceptedSource: payload.acceptedSource,
    composition: payload.composition,
    experience: payload.experience,
    phase3: payload.phase3,
    shops: payload.shops,
    consumables: payload.consumables,
  };
}

function approvalIdentities(payload) {
  return payload.approvals.map(approval => ({
    phase: approval.phase,
    filename: approval.filename,
    bytes: approval.bytes,
    sha256: approval.sha256,
    approvedCommit: approval.approvedCommit,
    approvedTree: approval.approvedTree,
  }));
}

function main() {
  const tree = git(['rev-parse', ACCEPTED_COMMIT + '^{tree}']).trim();
  assert.strictEqual(tree, ACCEPTED_TREE, 'the accepted Editor commit or tree is unavailable');
  const baselineSource = git(['show', ACCEPTED_COMMIT + ':native-rebuild-data.js']);
  const baseline = loadSource(baselineSource, ACCEPTED_COMMIT + ':native-rebuild-data.js');
  delete require.cache[GENERATED_BUNDLE];
  const generated = JSON.parse(JSON.stringify(require(GENERATED_BUNDLE)));

  assert.strictEqual(baseline.payloadSha256, ACCEPTED_PAYLOAD_SHA256,
    'the accepted baseline payload identity drifted');
  assert.strictEqual(sha256(Buffer.from(JSON.stringify(baseline.payload), 'utf8')),
    baseline.payloadSha256, 'the accepted baseline payload failed self-verification');
  assert.strictEqual(sha256(Buffer.from(JSON.stringify(generated.payload), 'utf8')),
    generated.payloadSha256, 'the regenerated payload failed self-verification');
  assert.ok(generated.payload.inputContract &&
    generated.payload.inputContract.sha256 === EXPECTED_INPUT_CONTRACT_SHA256,
  'the regenerated payload lacks the accepted input contract');

  const baselineGame = gamePayload(baseline.payload);
  const generatedGame = gamePayload(generated.payload);
  assert.deepStrictEqual(generatedGame, baselineGame,
    'restoration-backed regeneration changed a game-bearing payload');
  assert.deepStrictEqual(approvalIdentities(generated.payload),
    approvalIdentities(baseline.payload),
    'restoration-backed regeneration changed an accepted approval identity');

  const gamePayloadSha256 = sha256(Buffer.from(JSON.stringify(generatedGame), 'utf8'));
  process.stdout.write(`${JSON.stringify({
    status: 'pass',
    acceptedEditorCommit: ACCEPTED_COMMIT,
    acceptedEditorTree: ACCEPTED_TREE,
    acceptedPayloadSha256: baseline.payloadSha256,
    regeneratedPayloadSha256: generated.payloadSha256,
    gamePayloadSha256,
    inputContractSha256: generated.payload.inputContract.sha256,
    preservedSurfaces: [
      'accepted-source-ledger',
      'composition-layout-and-ownership',
      'experience-owner',
      'phase3-owners-and-products',
      'shop-owners-and-products',
      'consumable-owners-and-products',
      'approval-identities',
    ],
  }, null, 2)}\n`);
}

main();
