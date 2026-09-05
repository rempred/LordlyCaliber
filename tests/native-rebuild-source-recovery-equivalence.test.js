'use strict';

const assert = require('assert');
const childProcess = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ACCEPTED_COMMIT = '4106740a52b0df6b3d8528a201ea066d7d76ad44';
const ACCEPTED_TREE = '7931c1ac7e9226f7f1d95445e6d796a8810daa9b';
const SOURCE_RECOVERY_BASE = '2a5e3166a84f339071be41bfc00d38a0b952ecc6';
const NATIVE_RECOVERY_BASELINE = 'bd7aa1f9de0842db0628ab58a30e472b24729ec0';
const EDITOR_RECOVERY_BASELINE = 'e282410f0e9b2885da8bd9fe7c84d47c35987b1b';
const bundleFile = path.resolve(process.env.OB64_NATIVE_SOURCE_BUNDLE || '');
const generatorApi = require(path.join(EDITOR, 'tools', 'generate-native-rebuild-data.js'));

function sha256(bytes) {
  return crypto.createHash('sha256').update(Buffer.from(bytes)).digest('hex').toUpperCase();
}

function git(args) {
  return childProcess.execFileSync('git', ['-C', EDITOR].concat(args), {
    encoding: 'utf8', maxBuffer: 16 * 1024 * 1024,
  });
}

function loadSource(source, filename) {
  const sandbox = { module: { exports: {} } };
  sandbox.exports = sandbox.module.exports;
  sandbox.globalThis = sandbox;
  vm.runInNewContext(source, sandbox, { filename });
  return JSON.parse(JSON.stringify(sandbox.module.exports));
}

function approvalIdentities(payload) {
  return payload.approvals.map((approval) => ({
    phase: approval.phase,
    filename: approval.filename,
    bytes: approval.bytes,
    sha256: approval.sha256,
    approvedCommit: approval.approvedCommit,
    approvedTree: approval.approvedTree,
  }));
}

function semanticProjection(payload) {
  const consumableSource = payload.consumables.acceptedSource;
  return {
    acceptedSource: {
      commit: payload.acceptedSource.commit,
      tree: payload.acceptedSource.tree,
      runtimeRdramBytes: payload.acceptedSource.runtimeRdramBytes,
      retailBytes: payload.acceptedSource.retailBytes,
      retailSha256: payload.acceptedSource.retailSha256,
    },
    composition: payload.composition,
    experience: payload.experience,
    phase3: {
      generator: payload.phase3.generator,
      components: payload.phase3.components,
      toolOwnerMap: payload.phase3.toolOwnerMap,
      matrix: payload.phase3.matrix,
    },
    shops: payload.shops,
    consumables: {
      effect: payload.consumables.effect,
      descriptions: payload.consumables.descriptions,
      metadata: payload.consumables.metadata,
      acceptedSource: {
        commit: consumableSource.commit,
        tree: consumableSource.tree,
        ownerLedgerSha256: consumableSource.ownerLedgerSha256,
        relocationLedgerSha256: consumableSource.relocationLedgerSha256,
        sourceModelLedgerSha256: consumableSource.sourceModelLedgerSha256,
        products: consumableSource.products,
        nativeOwnerImages: consumableSource.nativeOwnerImages,
      },
    },
  };
}

function main() {
  assert.ok(process.env.OB64_NATIVE_SOURCE_BUNDLE,
    'OB64_NATIVE_SOURCE_BUNDLE must name the source-regenerated Editor bundle');
  assert.ok(fs.existsSync(bundleFile), 'the source-regenerated Editor bundle is missing');
  assert.strictEqual(git(['rev-parse', `${ACCEPTED_COMMIT}^{tree}`]).trim(), ACCEPTED_TREE,
    'the accepted Editor comparison commit or tree is unavailable');

  const accepted = loadSource(git(['show', `${ACCEPTED_COMMIT}:native-rebuild-data.js`]),
    `${ACCEPTED_COMMIT}:native-rebuild-data.js`);
  delete require.cache[bundleFile];
  const generated = JSON.parse(JSON.stringify(require(bundleFile)));
  assert.strictEqual(sha256(Buffer.from(JSON.stringify(accepted.payload), 'utf8')),
    accepted.payloadSha256, 'the accepted bundle failed self-verification');
  assert.strictEqual(sha256(Buffer.from(JSON.stringify(generated.payload), 'utf8')),
    generated.payloadSha256, 'the source-regenerated bundle failed self-verification');
  assert.strictEqual(generated.payload.inputContract.schemaVersion, 3,
    'the source-regenerated bundle did not use the committed-source input contract');
  assert.strictEqual(generated.payload.inputContract.id,
    'lordly-native-rebuild.phase5a.editor-inputs.source-recovery.v3',
  'the source-regenerated input contract ID drifted');
  const recoverySource = generated.payload.recoverySource;
  assert.ok(recoverySource && recoverySource.baseCommit === SOURCE_RECOVERY_BASE &&
    recoverySource.sourceIdentityMode === 'bounded-clean-committed-descendant' &&
    recoverySource.native.baselineCommit === NATIVE_RECOVERY_BASELINE &&
    recoverySource.editor.baselineCommit === EDITOR_RECOVERY_BASELINE &&
    recoverySource.native.commit !== NATIVE_RECOVERY_BASELINE &&
    recoverySource.editor.commit !== EDITOR_RECOVERY_BASELINE &&
    recoverySource.native.commitDistance > 0 && recoverySource.editor.commitDistance > 0,
  'the source-regeneration provenance is missing or wrong');
  assert.strictEqual(recoverySource.editor.commit, git(['rev-parse', 'HEAD']).trim(),
    'the generated provenance does not identify the executing Editor commit');
  assert.strictEqual(recoverySource.editor.tree, git(['rev-parse', 'HEAD^{tree}']).trim(),
    'the generated provenance does not identify the executing Editor tree');

  const inputRoot = path.join(path.dirname(bundleFile), 'input-root');
  const inputContractFile = path.join(
    inputRoot, 'source-recovery', 'editor-input-contract-v3.json');
  assert.ok(fs.existsSync(inputContractFile), 'the committed-source input contract is missing');
  const loadedContract = generatorApi.loadInputContract(inputRoot, inputContractFile);
  assert.strictEqual(loadedContract.identity.schemaVersion, 3,
    'the Editor generator rejected the committed-source input contract');

  const driftRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'ob64-source-contract-drift-'));
  try {
    const driftSourceRoot = path.join(driftRoot, 'source-recovery');
    fs.mkdirSync(driftSourceRoot, { recursive: true });
    const contract = JSON.parse(fs.readFileSync(inputContractFile, 'utf8'));
    const manifestSource = path.join(inputRoot, contract.recoverySource.sourceManifest.path);
    const manifest = JSON.parse(fs.readFileSync(manifestSource, 'utf8'));
    manifest.native.committedFiles.push({
      status: 'M',
      path: 'src/undeclared-drift.c',
      bytes: 1,
      sha256: 'A'.repeat(64),
    });
    const manifestBytes = Buffer.from(`${JSON.stringify(manifest, null, 2)}\n`, 'utf8');
    const driftManifest = path.join(driftSourceRoot, 'native-source-manifest.json');
    fs.writeFileSync(driftManifest, manifestBytes);
    contract.recoverySource.sourceManifest.bytes = manifestBytes.length;
    contract.recoverySource.sourceManifest.sha256 = sha256(manifestBytes);
    const driftContract = path.join(driftSourceRoot, 'editor-input-contract-v3.json');
    fs.writeFileSync(driftContract, `${JSON.stringify(contract, null, 2)}\n`, 'utf8');
    assert.throws(() => generatorApi.loadInputContract(driftRoot, driftContract),
      /native committed source inventory drift/,
      'the Editor generator accepted committed source outside the authenticated contract');
  } finally {
    fs.rmSync(driftRoot, { recursive: true, force: false });
  }

  assert.deepStrictEqual(semanticProjection(generated.payload), semanticProjection(accepted.payload),
    'source regeneration changed accepted game data, layout, or ownership');
  assert.deepStrictEqual(approvalIdentities(generated.payload),
    approvalIdentities(accepted.payload), 'source regeneration changed approval identities');

  process.stdout.write(`${JSON.stringify({
    status: 'pass',
    acceptedEditorCommit: ACCEPTED_COMMIT,
    acceptedEditorTree: ACCEPTED_TREE,
    recoveryNativeCommit: recoverySource.native.commit,
    recoveryNativeTree: recoverySource.native.tree,
    recoveryEditorCommit: recoverySource.editor.commit,
    recoveryEditorTree: recoverySource.editor.tree,
    committedSourceDriftRejected: true,
    generatedPayloadSha256: generated.payloadSha256,
    semanticProjectionSha256: sha256(Buffer.from(JSON.stringify(
      semanticProjection(generated.payload)), 'utf8')),
    approvalCount: generated.payload.approvals.length,
    preserved: [
      'retail-bytes', 'modified-products', 'composition-layout', 'ownership',
      'experience-owner', 'Phase-3-owners', 'shop-owners', 'consumable-owners',
      'historical-approval-identities',
    ],
  }, null, 2)}\n`);
}

main();
