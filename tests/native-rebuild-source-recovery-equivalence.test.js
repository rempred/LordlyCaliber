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
const SOURCE_RECOVERY_BASE = '2a5e3166a84f339071be41bfc00d38a0b952ecc6';
const SOURCE_RECOVERY_HEAD = '8caccda2075972e3039f40cdd3dd63cf7f663d77';
const bundleFile = path.resolve(process.env.OB64_NATIVE_SOURCE_BUNDLE || '');

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
  assert.strictEqual(generated.payload.inputContract.schemaVersion, 2,
    'the source-regenerated bundle did not use the v2 input contract');
  assert.strictEqual(generated.payload.inputContract.id,
    'lordly-native-rebuild.phase5a.editor-inputs.source-recovery.v2',
  'the source-regenerated input contract ID drifted');
  assert.ok(generated.payload.recoverySource &&
    generated.payload.recoverySource.baseCommit === SOURCE_RECOVERY_BASE &&
    generated.payload.recoverySource.recoveryHead === SOURCE_RECOVERY_HEAD,
  'the source-regeneration provenance is missing or wrong');

  assert.deepStrictEqual(semanticProjection(generated.payload), semanticProjection(accepted.payload),
    'source regeneration changed accepted game data, layout, or ownership');
  assert.deepStrictEqual(approvalIdentities(generated.payload),
    approvalIdentities(accepted.payload), 'source regeneration changed approval identities');

  process.stdout.write(`${JSON.stringify({
    status: 'pass',
    acceptedEditorCommit: ACCEPTED_COMMIT,
    acceptedEditorTree: ACCEPTED_TREE,
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
