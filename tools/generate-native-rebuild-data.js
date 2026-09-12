#!/usr/bin/env node
'use strict';

// Generates the browser-safe Phase 1 through Phase 5A native-owner bundle.
// The generated file contains no complete ROM. It contains only authenticated
// owner payloads, their retail preimages, and composition metadata.

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const EXPECTED_COMMIT = 'e50fe500839288c94c538969acc7a5b966075626';
const EXPECTED_TREE = 'ef58ea271b2298be71b09be9318a359805c266c0';
const RETAIL_SHA256 = '571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A';
const GENERATOR_SOURCE_IDENTITY = 'sha256-canonical-utf8-lf-v1';
const INPUT_CONTRACT_SCHEMA = 'ob64-native-rebuild-editor-input-contract';
const INPUT_CONTRACT_VERSION = 1;
const INPUT_CONTRACT_ID = 'lordly-native-rebuild.phase5a.editor-inputs.v1';
const LEGACY_SOURCE_RECOVERY_CONTRACT_VERSION = 2;
const LEGACY_SOURCE_RECOVERY_CONTRACT_ID =
  'lordly-native-rebuild.phase5a.editor-inputs.source-recovery.v2';
const SOURCE_RECOVERY_CONTRACT_VERSION = 3;
const SOURCE_RECOVERY_CONTRACT_ID =
  'lordly-native-rebuild.phase5a.editor-inputs.source-recovery.v3';
const INPUT_CONTRACT_RELATIVE = 'config/lordly-native-editor-inputs-v1.json';
const EXPECTED_INPUT_CONTRACT_SHA256 =
  'A7BE40F1F386239BCA42AAACBF60CCAF0B673051ACD74DFE90D50BA53C11223E';
const EXPECTED_EDITOR_COMMIT = '4106740a52b0df6b3d8528a201ea066d7d76ad44';
const EXPECTED_EDITOR_TREE = '7931c1ac7e9226f7f1d95445e6d796a8810daa9b';
const SOURCE_RECOVERY_BASE_COMMIT = '2a5e3166a84f339071be41bfc00d38a0b952ecc6';
const SOURCE_RECOVERY_BASE_TREE = 'c7239d5992f3988cae3fa4c742008e5102b131e5';
const LEGACY_SOURCE_RECOVERY_HEAD = '8caccda2075972e3039f40cdd3dd63cf7f663d77';
const LEGACY_SOURCE_RECOVERY_HEAD_TREE = '8184ef3033a94808cd448ba2bda7b45306ce4252';
const SOURCE_RECOVERY_IDENTITY_MODE = 'bounded-clean-committed-descendant';
const SOURCE_RECOVERY_NATIVE_BASELINE = Object.freeze({
  commit: 'bd7aa1f9de0842db0628ab58a30e472b24729ec0',
  tree: 'dec1746f8af996cac9ea6d4e03068fe0d8e02ad7',
});
const SOURCE_RECOVERY_EDITOR_BASELINE = Object.freeze({
  commit: 'e282410f0e9b2885da8bd9fe7c84d47c35987b1b',
  tree: '958e14bef90045a69bf30b7ff11901dece8dd03d',
});
const SOURCE_RECOVERY_NATIVE_PATHS = Object.freeze([
  'config/lordly-native-source-recovery-v3.json',
  'docs/LORDLY_NATIVE_RECOVERY.md',
  'tests/lordly_native_source_recovery.js',
  'tools/lib/lordly_native_source_recovery.js',
]);
const SOURCE_RECOVERY_EDITOR_PATHS = Object.freeze([
  'tests/native-rebuild-source-recovery-equivalence.test.js',
  'tools/generate-native-rebuild-data.js',
]);
const ALIGNMENT_SOURCE_NATIVE_BASELINE = Object.freeze({commit:'774b8aa41b1c3780a8aa568816d5137c85501918',tree:'a2e6482b75070fd59a1dad040189556226a0716e'});
const ALIGNMENT_SOURCE_EDITOR_BASELINE = Object.freeze({commit:'aaeffde75b394dc2c41162933299d099c317d792',tree:'ab7009686e932db2c326319fa08cf5142a44ef8c'});
const ALIGNMENT_SOURCE_NATIVE_PATHS = Object.freeze(['config/lordly-native-source-recovery-v4.json','tools/build_squad_alignment_correction.py','tools/lib/lordly_native_source_recovery.js','tests/squad_alignment_correction.py','tests/lordly_native_source_recovery.js']);
const ALIGNMENT_SOURCE_EDITOR_PATHS = Object.freeze(['tools/generate-native-rebuild-data.js','tests/native-rebuild-alignment-correction.test.js','native-rebuild.js','tests/native-rebuild-phase5b-consumables.test.js']);
const EXPECTED_APPROVALS = Object.freeze([
  Object.freeze({ phase: 'Phase 3',
    sha256: 'B458CD6BFC5964938D61BE855AD9792D59A2509D73CF6FCB57844275A97D2ABA',
    approvedCommit: '000fb0dc2ef5cb3d658e0e0ffbf6b49d84c600fb',
    approvedTree: '917b52c27df857c6793f6320dc68e61244eb25a1' }),
  Object.freeze({ phase: 'Phase 4',
    sha256: 'B1ED23C9248BF7D893CF1EE9DBEF2ED8AE2D266B84EFF4045FE03BD348797DEF',
    approvedCommit: '05623e67b295f4f968e4a5cb416b9c002fb09bdd',
    approvedTree: '90b193077b61eee0f6656a8d3f6848b2f17bdfaa' }),
  Object.freeze({ phase: 'Phase 5A',
    sha256: '578C512E89429746888772AB8150BA56FFB888E4883B79344F023B8236031743',
    approvedCommit: EXPECTED_COMMIT,
    approvedTree: EXPECTED_TREE }),
]);
const LEGACY_GENERATOR_IDENTITY = Object.freeze({
  path: 'tools/generate-native-rebuild-data.js',
  sourceIdentity: GENERATOR_SOURCE_IDENTITY,
  canonicalBytes: 34899,
  sha256: 'C65F81C124AFABA7A6C444E7A7A76578C1EF6A50147596E7A24FE26EAD69D69D',
});

const PHASE3_PAYLOAD =
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/native-payload.json';
const PHASE3_REPORT =
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/build-report.json';
const PHASE4_PAYLOAD =
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/native-shop-payload.json';
const PHASE4_REPORT =
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/build-report.json';
const PHASE4_MATRIX =
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/composition-matrix.json';
const RETAIL_ROM =
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/products/exact-retail/exact-retail.us_rev0.z64';
const PHASE5A_ROOT = 'build/lordly-native-rebuild/phase5a-consumables-r1';
const PHASE5A_REPORT = PHASE5A_ROOT + '/build-report.json';
const PHASE5A_OWNER_LEDGER = PHASE5A_ROOT + '/owner-ledger.json';
const PHASE5A_RELOCATION_LEDGER = PHASE5A_ROOT + '/relocation-ledger.json';
const PHASE5A_SOURCE_MODEL_LEDGER = PHASE5A_ROOT + '/source-model-ledger.json';
const PHASE5A_STATIC_MATRIX = PHASE5A_ROOT + '/static-matrix.json';
const PHASE5A_EXACT_RETAIL =
  PHASE5A_ROOT + '/products/exact-retail/exact-retail.us_rev0.z64';
const PHASE5A_NATIVE =
  PHASE5A_ROOT + '/products/native-consumables/native-consumables.us_rev0.z64';
const PHASE5A_CANONICAL = PHASE5A_ROOT +
  '/products/canonical-native-consumables/canonical-native-consumables.us_rev0.z64';

const PRODUCTION_OWNER_IDS = Object.freeze({
  items: 'direct.item-records',
  classes: 'direct.class-definitions',
  encounters: 'direct.neutral-encounters',
  'creature-drops': 'direct.creature-drops',
  'consumable-metadata': 'direct.consumable-metadata',
  'item-descriptions': 'text.item-descriptions',
  'consumable-descriptions-pending': 'text.consumable-descriptions',
  'class-descriptions': 'text.class-descriptions',
  'action-descriptions': 'text.action-descriptions',
  'stat-gates': 'stat.class-change-gates',
  'tool-cf-army-counter': 'tool.chaos-frame-counter',
  'tool-high-attack-streamsplit': 'tool.high-attack-streamsplit',
  'tool-experience-size-scale': 'tool.experience-size-scale',
  'tool-character-card-luck': 'tool.character-card-luck',
  'tool-squad-menu-alignment': 'tool.squad-menu-alignment',
  'scenario-archives': 'scenario.archive-composer',
  'scenario-eset-relocation': 'scenario.archive-composer',
  'pi-source-redirect': 'source.pi-redirect',
  'consumable-effects': 'effects.consumable-code',
  'consumable-healing-descriptions': 'effects.healing-descriptions',
  'combat-animation-overrides': 'combat.animation-overrides',
  'native-art': 'art.native-resource-composer',
  'cutscene-director-resources': 'cutscene.director-resources',
  'runtime-overrides': 'runtime.shared-overrides',
});

function fail(message) {
  throw new Error('Native rebuild data generation failed: ' + message);
}

function parseArgs(argv) {
  const args = {};
  for (let index = 0; index < argv.length; index += 1) {
    const name = argv[index];
    if (name === '--runtime') { args.runtime = true; continue; }
    if (name === '--input-root' || name === '--decomp' ||
        name === '--input-contract' || name === '--out' || name === '--alignment-correction') {
      if (!argv[index + 1]) fail(name + ' needs a path');
      const key = {
        '--input-root': 'inputRoot',
        '--decomp': 'inputRoot',
        '--input-contract': 'inputContract',
        '--out': 'out',
        '--alignment-correction': 'alignmentCorrection',
      }[name];
      if (args[key]) fail('duplicate ' + key + ' argument');
      args[key] = path.resolve(argv[++index]);
    } else {
      fail('unknown argument ' + name);
    }
  }
  if (!args.inputRoot) fail('--input-root is required');
  if (!args.inputContract) {
    args.inputContract = path.join(args.inputRoot, ...INPUT_CONTRACT_RELATIVE.split('/'));
  }
  if (!args.out) args.out = path.resolve(__dirname, '..', 'native-rebuild-data.js');
  return args;
}

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function canonicalizeGeneratorSource(bytes) {
  const sourceBytes = Buffer.from(bytes);
  const source = sourceBytes.toString('utf8');
  if (!Buffer.from(source, 'utf8').equals(sourceBytes)) {
    fail('generator source is not valid UTF-8');
  }
  const canonical = source.replace(/\r\n/g, '\n');
  if (canonical.includes('\r')) {
    fail('generator source uses an unsupported line ending');
  }
  return Buffer.from(canonical, 'utf8');
}

function identifyGeneratorSource(bytes) {
  const canonicalBytes = canonicalizeGeneratorSource(bytes);
  return {
    sourceIdentity: GENERATOR_SOURCE_IDENTITY,
    canonicalBytes: canonicalBytes.length,
    sha256: sha256(canonicalBytes),
  };
}

function safeRelative(value, label) {
  if (typeof value !== 'string' || !value || path.isAbsolute(value) ||
      value.includes('\\') || value.includes(':')) {
    fail(label + ' is not a portable relative path: ' + value);
  }
  const parts = value.split('/');
  if (!parts.every(part => part && part !== '.' && part !== '..')) {
    fail(label + ' contains an unsafe segment: ' + value);
  }
  return parts.join('/');
}

function canonicalizeContract(bytes) {
  const sourceBytes = Buffer.from(bytes);
  const source = sourceBytes.toString('utf8');
  if (!Buffer.from(source, 'utf8').equals(sourceBytes)) fail('input contract is not valid UTF-8');
  const canonical = source.replace(/\r\n/g, '\n');
  if (canonical.includes('\r')) fail('input contract uses an unsupported line ending');
  return Buffer.from(canonical, 'utf8');
}

function sourceManifest(root, recovery) {
  if (!recovery.sourceManifest) fail('source-recovery manifest reference is missing');
  const manifestPath = safeRelative(recovery.sourceManifest.path, 'source manifest path');
  const manifest = read(root, manifestPath);
  if (manifest.length !== recovery.sourceManifest.bytes ||
      sha256(manifest) !== recovery.sourceManifest.sha256) {
    fail('source-recovery manifest identity drift');
  }
  return JSON.parse(manifest.toString('utf8'));
}

function verifyCommittedSourceFiles(files, expectedPaths, label) {
  if (!Array.isArray(files) || files.length !== expectedPaths.length) {
    fail(`${label} committed source inventory drift`);
  }
  const expected = new Set(expectedPaths);
  const actual = new Set();
  files.forEach((entry) => {
    if (!entry || (entry.status !== 'A' && entry.status !== 'M') ||
        typeof entry.path !== 'string' || !expected.has(entry.path) ||
        !Number.isSafeInteger(entry.bytes) || entry.bytes <= 0 ||
        typeof entry.sha256 !== 'string' || !/^[0-9A-F]{64}$/.test(entry.sha256) ||
        actual.has(entry.path)) {
      fail(`${label} committed source inventory drift`);
    }
    actual.add(entry.path);
  });
  if (actual.size !== expected.size || expectedPaths.some((entry) => !actual.has(entry))) {
    fail(`${label} committed source inventory drift`);
  }
}

function verifyCommittedSourceIdentity(root, recovery, currentAlignment = false) {
  const native = recovery.native;
  const editor = recovery.editor;
  const gitObject = (value) => typeof value === 'string' && /^[0-9a-f]{40}$/.test(value);
  const validIdentity = (identity, baseline) => identity &&
    identity.baselineCommit === baseline.commit && identity.baselineTree === baseline.tree &&
    gitObject(identity.commit) && gitObject(identity.tree) &&
    Number.isSafeInteger(identity.commitDistance) && identity.commitDistance > 0;
  if (recovery.status !== 'source-regenerated-unreviewed' ||
      recovery.sourceIdentityMode !== SOURCE_RECOVERY_IDENTITY_MODE ||
      recovery.baseCommit !== SOURCE_RECOVERY_BASE_COMMIT ||
      recovery.baseTree !== SOURCE_RECOVERY_BASE_TREE ||
      !validIdentity(native, currentAlignment ? ALIGNMENT_SOURCE_NATIVE_BASELINE : SOURCE_RECOVERY_NATIVE_BASELINE) ||
      !validIdentity(editor, currentAlignment ? ALIGNMENT_SOURCE_EDITOR_BASELINE : SOURCE_RECOVERY_EDITOR_BASELINE)) {
    fail('committed source-recovery identity drift');
  }

  const manifest = sourceManifest(root, recovery);
  if (manifest.schemaVersion !== 2 || manifest.sourceMode !== SOURCE_RECOVERY_IDENTITY_MODE ||
      !manifest.native || !manifest.editor ||
      manifest.native.head !== native.commit || manifest.native.tree !== native.tree ||
      manifest.native.baselineCommit !== native.baselineCommit ||
      manifest.native.baselineTree !== native.baselineTree ||
      manifest.native.commitDistance !== native.commitDistance ||
      manifest.native.baseCommit !== SOURCE_RECOVERY_BASE_COMMIT ||
      manifest.native.baseTree !== SOURCE_RECOVERY_BASE_TREE ||
      manifest.editor.head !== editor.commit || manifest.editor.tree !== editor.tree ||
      manifest.editor.baselineCommit !== editor.baselineCommit ||
      manifest.editor.baselineTree !== editor.baselineTree ||
      manifest.editor.commitDistance !== editor.commitDistance ||
      !Array.isArray(manifest.native.workingChanges) ||
      manifest.native.workingChanges.length !== 0 ||
      !Array.isArray(manifest.editor.workingChanges) ||
      manifest.editor.workingChanges.length !== 0) {
    fail('committed source-recovery manifest facts drift');
  }
  verifyCommittedSourceFiles(
    manifest.native.committedFiles, currentAlignment ? ALIGNMENT_SOURCE_NATIVE_PATHS : SOURCE_RECOVERY_NATIVE_PATHS, 'native');
  verifyCommittedSourceFiles(
    manifest.editor.committedFiles, currentAlignment ? ALIGNMENT_SOURCE_EDITOR_PATHS : SOURCE_RECOVERY_EDITOR_PATHS, 'Editor');
}

function loadInputContract(root, target) {
  const canonical = canonicalizeContract(fs.readFileSync(target));
  const contract = JSON.parse(canonical.toString('utf8'));
  const legacySourceRecovery =
    contract.schemaVersion === LEGACY_SOURCE_RECOVERY_CONTRACT_VERSION &&
    contract.contractId === LEGACY_SOURCE_RECOVERY_CONTRACT_ID;
  const currentAlignment = contract.schemaVersion === 4 && contract.contractId === 'lordly-native-rebuild.phase5a.editor-inputs.source-recovery.v4';
  const committedSourceRecovery = currentAlignment || (contract.schemaVersion === SOURCE_RECOVERY_CONTRACT_VERSION &&
    contract.contractId === SOURCE_RECOVERY_CONTRACT_ID);
  const sourceRecovery = legacySourceRecovery || committedSourceRecovery;
  const identity = {
    schema: INPUT_CONTRACT_SCHEMA,
    schemaVersion: contract.schemaVersion,
    id: contract.contractId,
    canonicalBytes: canonical.length,
    sha256: sha256(canonical),
  };
  if (!sourceRecovery && identity.sha256 !== EXPECTED_INPUT_CONTRACT_SHA256) {
    fail('input contract canonical identity drift');
  }
  if (!sourceRecovery) {
    const rootedContract = canonicalizeContract(read(root, INPUT_CONTRACT_RELATIVE));
    if (sha256(rootedContract) !== identity.sha256) {
      fail('input-root contract differs from the requested input contract');
    }
  }
  if (contract.schema !== INPUT_CONTRACT_SCHEMA ||
      (!sourceRecovery && (contract.schemaVersion !== INPUT_CONTRACT_VERSION ||
        contract.contractId !== INPUT_CONTRACT_ID)) ||
      (legacySourceRecovery && contract.contractId !== LEGACY_SOURCE_RECOVERY_CONTRACT_ID) ||
      (committedSourceRecovery && !currentAlignment && contract.contractId !== SOURCE_RECOVERY_CONTRACT_ID)) {
    fail('input contract schema, version, or ID drift');
  }
  if (!contract.nativeSource || contract.nativeSource.commit !== EXPECTED_COMMIT ||
      contract.nativeSource.tree !== EXPECTED_TREE || !contract.editorSource ||
      contract.editorSource.commit !== EXPECTED_EDITOR_COMMIT ||
      contract.editorSource.tree !== EXPECTED_EDITOR_TREE) {
    fail('input contract accepted source identity drift');
  }
  if (legacySourceRecovery) {
    const recovery = contract.recoverySource;
    if (!recovery || recovery.status !== 'source-regenerated-unreviewed' ||
        recovery.baseCommit !== SOURCE_RECOVERY_BASE_COMMIT ||
        recovery.baseTree !== SOURCE_RECOVERY_BASE_TREE ||
        recovery.recoveryHead !== LEGACY_SOURCE_RECOVERY_HEAD ||
        recovery.recoveryHeadTree !== LEGACY_SOURCE_RECOVERY_HEAD_TREE ||
        !recovery.sourceManifest) {
      fail('source-recovery identity drift');
    }
    const parsedManifest = sourceManifest(root, recovery);
    if (parsedManifest.schemaVersion !== 1 ||
        parsedManifest.native.head !== LEGACY_SOURCE_RECOVERY_HEAD ||
        parsedManifest.native.tree !== LEGACY_SOURCE_RECOVERY_HEAD_TREE ||
        parsedManifest.native.baseCommit !== SOURCE_RECOVERY_BASE_COMMIT ||
        parsedManifest.native.baseTree !== SOURCE_RECOVERY_BASE_TREE ||
        parsedManifest.editor.head !== EXPECTED_EDITOR_COMMIT &&
          parsedManifest.editor.head !== '75e27609888c6389154c7632ffbcd3bfc8f4f4f2') {
      fail('source-recovery manifest facts drift');
    }
  }
  if (committedSourceRecovery) verifyCommittedSourceIdentity(root, contract.recoverySource || {}, currentAlignment);
  if (!contract.retailRom || contract.retailRom.bytes !== 0x02800000 ||
      contract.retailRom.normalizedZ64Sha256 !== RETAIL_SHA256) {
    fail('input contract retail identity drift');
  }
  if (!Array.isArray(contract.inputs) || contract.inputs.length !== contract.inputCount ||
      contract.inputCount !== 60 || contract.bundleInputLedgerCount !== 52 ||
      !contract.recoveryClassCounts || contract.recoveryClassCounts['tracked-source'] !== 14 ||
      (!sourceRecovery && contract.recoveryClassCounts['frozen-evidence'] !== 46) ||
      (sourceRecovery && contract.recoveryClassCounts['source-generated'] !== 46)) {
    fail('input contract file census drift');
  }
  const byPath = new Map();
  const classCounts = sourceRecovery
    ? { 'tracked-source': 0, 'source-generated': 0 }
    : { 'tracked-source': 0, 'frozen-evidence': 0 };
  let ledgerCount = 0;
  contract.inputs.forEach(entry => {
    const relative = safeRelative(entry.path, 'input path');
    if (byPath.has(relative) || !/^[A-F0-9]{64}$/.test(entry.sha256) ||
        !Object.prototype.hasOwnProperty.call(classCounts, entry.recoveryClass) ||
        typeof entry.bundleLedger !== 'boolean' ||
        (entry.archiveRole !== undefined &&
          (entry.bundleLedger || entry.archiveRole !== 'verification-support'))) {
      fail('input contract entry drift: ' + relative);
    }
    byPath.set(relative, entry);
    classCounts[entry.recoveryClass] += 1;
    if (entry.bundleLedger) ledgerCount += 1;
  });
  if (ledgerCount !== contract.bundleInputLedgerCount ||
      Object.entries(classCounts).some(([name, count]) =>
        count !== contract.recoveryClassCounts[name])) {
    fail('input contract recovery-class or bundle census drift');
  }
  if (!Array.isArray(contract.approvals) || contract.approvals.length !== 3 ||
      contract.approvals.map(approval => approval.phase).join('|') !==
        'Phase 3|Phase 4|Phase 5A') {
    fail('input contract approval census drift');
  }
  contract.approvals.forEach((approval, index) => {
    const expected = EXPECTED_APPROVALS[index];
    safeRelative(approval.path, 'approval path');
    safeRelative(approval.sourcePath, 'approval source path');
    if (!/^[A-F0-9]{64}$/.test(approval.sha256) ||
        !/^[0-9a-f]{40}$/.test(approval.approvedCommit) ||
        !/^[0-9a-f]{40}$/.test(approval.approvedTree) ||
        approval.verdict !== 'Accepted' || approval.phase !== expected.phase ||
        approval.sha256 !== expected.sha256 ||
        approval.approvedCommit !== expected.approvedCommit ||
        approval.approvedTree !== expected.approvedTree) {
      fail('input contract approval facts drift: ' + approval.phase);
    }
  });
  return { contract, identity, byPath };
}

function read(root, relative) {
  const safe = safeRelative(relative, 'input path');
  const target = path.resolve(root, ...safe.split('/'));
  const relation = path.relative(path.resolve(root), target);
  if (!relation || relation.startsWith('..') || path.isAbsolute(relation)) {
    fail('input escaped the input root');
  }
  if (!fs.existsSync(target)) fail('missing input ' + relative);
  return fs.readFileSync(target);
}

function readJson(root, relative) {
  return JSON.parse(read(root, relative).toString('utf8'));
}

function verifyInputs(root, inputContract) {
  const ledger = [];
  inputContract.contract.inputs.forEach(entry => {
    const bytes = read(root, entry.path);
    const actual = sha256(bytes);
    if (actual !== entry.sha256) fail('input identity drift: ' + entry.path);
    if (entry.bundleLedger) ledger.push({ path: entry.path, bytes: bytes.length, sha256: actual });
  });
  return ledger;
}

function verifyApprovals(root, inputContract) {
  return inputContract.contract.approvals.map(approval => {
    const bytes = read(root, approval.path);
    const actual = sha256(bytes);
    if (actual !== approval.sha256) fail(approval.phase + ' approval identity drift');
    return {
      phase: approval.phase,
      filename: path.basename(approval.sourcePath),
      bytes: bytes.length,
      sha256: actual,
      approvedCommit: approval.approvedCommit,
      approvedTree: approval.approvedTree,
      verdict: approval.verdict,
    };
  });
}

function inputSha256(inputContract, relative) {
  const entry = inputContract.byPath.get(relative);
  if (!entry) fail('input contract has no entry for ' + relative);
  return entry.sha256;
}

function normalizeComponent(component, retail) {
  const payloadBytes = Buffer.from(component.dataHex, 'hex');
  if (payloadBytes.length !== component.bytes) fail(component.name + ' byte count drift');
  if (sha256(payloadBytes) !== component.sha256) fail(component.name + ' payload hash drift');
  const start = Number(component.romStart);
  const budget = Number(component.budget);
  if (!Number.isInteger(start) || !Number.isInteger(budget) ||
      budget < payloadBytes.length || start < 0 || start + budget > retail.length) {
    fail(component.name + ' ROM range is invalid');
  }
  const retailRegion = Buffer.from(retail.subarray(start, start + budget));
  const nativeRegion = Buffer.from(retailRegion);
  payloadBytes.copy(nativeRegion, 0);
  return {
    owner: component.owner,
    name: component.name,
    kind: component.sourceKind || component.kind,
    romStart: start,
    ramStart: Number(component.ramStart),
    bytes: budget,
    payloadBytes: payloadBytes.length,
    budget,
    payloadSha256: component.sha256,
    sha256: sha256(nativeRegion),
    dataHex: nativeRegion.toString('hex').toUpperCase(),
    retailHex: retailRegion.toString('hex').toUpperCase(),
  };
}

function verifyProductFile(root, relative, expectedSha256) {
  const bytes = read(root, relative);
  if (bytes.length !== 0x02800000 || sha256(bytes) !== expectedSha256) {
    fail('accepted product identity drift: ' + relative);
  }
  return bytes;
}

function verifyComponentRegions(label, components, product) {
  components.forEach((component) => {
    const actual = product.subarray(component.romStart,
      component.romStart + component.bytes);
    if (!actual.equals(Buffer.from(component.dataHex, 'hex'))) {
      fail(label + ' owner region drift: ' + component.name);
    }
  });
}

function selectedOwner(componentOwner) {
  const map = {
    'tool.chaos-frame-counter': 'cf-army-counter',
    'tool.character-card-luck': 'character-card-luck',
    'tool.squad-menu-alignment': 'squad-menu-alignment',
  };
  return map[componentOwner] || componentOwner;
}

function ownerImageIdentity(product, start, bytes) {
  const image = Buffer.from(product.subarray(start, start + bytes));
  if (image.length !== bytes) fail('accepted consumable owner lies outside its product');
  return { bytes, sha256: sha256(image) };
}

function buildPayload(root, identity, inputs, approvals, generatorIdentity, inputContract) {
  const retail = read(root, RETAIL_ROM);
  if (retail.length !== 0x02800000 || sha256(retail) !== RETAIL_SHA256) {
    fail('accepted retail product identity drift');
  }

  const phase1Api = require(path.join(root, 'tools', 'lib', 'lordly_native_rebuild.js'));
  const phase1Config = phase1Api.loadConfig();
  const phase1Report = readJson(root,
    'build/lordly-native-rebuild/phase1/build-report.json');
  const phase1Retail = phase1Api.generateTargetSource(100);
  const phase1Modified = phase1Api.generateTargetSource(200);
  const phase1Coverage = Array.from({ length: 301 }, (_, percent) => {
    const generated = phase1Api.generateTargetSource(percent);
    return sha256(generated.bytes);
  });
  const experienceStart = Number(phase1Config.target.romStart);
  if (!phase1Retail.bytes.equals(retail.subarray(
    experienceStart, experienceStart + phase1Retail.bytes.length))) {
    fail('Phase 1 retail owner differs from accepted retail product');
  }

  const phase3 = readJson(root, PHASE3_PAYLOAD);
  const phase3Report = readJson(root, PHASE3_REPORT);
  const phase4 = readJson(root, PHASE4_PAYLOAD);
  const phase4Report = readJson(root, PHASE4_REPORT);
  const phase4Matrix = readJson(root, PHASE4_MATRIX);
  const phase4Config = readJson(root, 'config/lordly-native-phase4-shops.json');
  const contract = readJson(root, 'docs/contracts/lordly-native-shop-membership-v1.json');
  const composition = readJson(root, 'config/lordly-native-composition.json');
  const phase5aConfig = readJson(root, 'config/lordly-native-phase5a-consumables.json');
  const phase5aReport = readJson(root, PHASE5A_REPORT);
  const phase5aOwnerLedger = readJson(root, PHASE5A_OWNER_LEDGER);
  const phase5aRelocationLedger = readJson(root, PHASE5A_RELOCATION_LEDGER);
  const phase5aSourceModelLedger = readJson(root, PHASE5A_SOURCE_MODEL_LEDGER);
  const phase5aStaticMatrix = readJson(root, PHASE5A_STATIC_MATRIX);
  const phase4Extension = phase4Config.compositionExtension;
  if (!phase4Extension ||
      phase4Extension.baseManifestSha256 !==
        inputSha256(inputContract, 'config/lordly-native-composition.json') ||
      phase4Extension.mode !== 'immutable-overlay' ||
      !phase4Extension.ownershipTransition ||
      phase4Extension.ownershipTransition.contributorId !== 'runtime.shop-overrides' ||
      phase4Extension.ownershipTransition.legacyPhysicalOwner !== 'runtime.shared-overrides' ||
      phase4Extension.ownershipTransition.nativePhysicalOwner !== 'runtime.shop-overrides' ||
      phase4Extension.ownershipTransition.mixedState !== 'reject') {
    fail('Phase 4 composition extension identity or ownership transition drift');
  }
  const compositionOwnerIds = new Set(composition.owners.map((owner) => owner.id));
  Object.keys(PRODUCTION_OWNER_IDS).forEach((productionId) => {
    if (!compositionOwnerIds.has(PRODUCTION_OWNER_IDS[productionId])) {
      fail('production owner map references an unknown manifest owner: ' + productionId);
    }
  });

  if (phase4Matrix.schemaVersion !== 1 || phase4Matrix.status !== 'pass' ||
      phase4Matrix.stateCount !== 32 || phase4Matrix.states.length !== 32) {
    fail('accepted Phase 4 composition matrix is incomplete');
  }
  const seenPhase4States = new Set();
  phase4Matrix.states.forEach((state) => {
    const expectedId = 'native-shop-' + (state.nativeShops ? 1 : 0) +
      '-remaining-' + Number(state.remainingMask).toString(2).padStart(4, '0');
    if (state.id !== expectedId || state.remainingMask < 0 || state.remainingMask > 15 ||
        !state.deterministic || !state.attributionComplete ||
        !state.crc6102 || !state.crc6102.ok || seenPhase4States.has(state.id)) {
      fail('accepted Phase 4 composition state drift: ' + state.id);
    }
    seenPhase4States.add(state.id);
  });

  const phase3Components = phase3.components.map((component) =>
    normalizeComponent(component, retail));
  const phase4Components = phase4.components.map((component) =>
    normalizeComponent(component, retail));
  const phase4Module = phase4Components.find((component) =>
    component.name === 'source-linked-shop-module-and-membership-contract');
  if (!phase4Module) fail('Phase 4 module component is missing');

  const sourceModule = phase4Config.owner.sourceModule;
  const contractOffset = Number(sourceModule.contractOffset);
  const contractBytes = Number(phase4Config.contract.totalBytes);
  const moduleBytes = Buffer.from(phase4Module.dataHex, 'hex');
  const embeddedContract = moduleBytes.subarray(contractOffset, contractOffset + contractBytes);
  if (embeddedContract.length !== contractBytes ||
      sha256(embeddedContract) !== contract.roundTrip.modified.sha256) {
    fail('Phase 4 embedded Shop contract identity drift');
  }

  const matrix = phase3Report.products.matrix.map((entry) => ({
    id: entry.id,
    mask: entry.mask,
    activeOwners: entry.activeOwners.slice(),
    sha256: entry.rom.sha256,
  }));
  const phase3All = phase3Report.products.matrix.find((entry) => entry.mask === 7);
  const phase3AllProduct = verifyProductFile(root,
    'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/products/migrated-111/migrated-111.us_rev0.z64',
    phase3All.rom.sha256);
  verifyComponentRegions('Phase 3', phase3Components, phase3AllProduct);
  const phase4Products = contract.compatibility.modified.map((entry) => ({
    id: entry.id,
    sha256: entry.exactSha256,
    projectionSha256: entry.projectionSha256,
  }));
  if (phase4Report.products.nativeShops.sha256 !== phase4Products[0].sha256 ||
      phase4Report.products.canonicalNativeShops.sha256 !== phase4Products[1].sha256) {
    fail('Phase 4 report and contract product identities disagree');
  }
  const phase4NativeProduct = verifyProductFile(root,
    'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/products/native-shops/native-shops.us_rev0.z64',
    phase4Report.products.nativeShops.sha256);
  verifyComponentRegions('Phase 4', phase4Components, phase4NativeProduct);

  const effectOwner = phase5aOwnerLedger.rom.find((owner) =>
    owner.id === 'effects.consumable-code');
  const descriptionOwner = phase5aOwnerLedger.rom.find((owner) =>
    owner.id === 'text.consumable-descriptions');
  const metadataOwner = phase5aOwnerLedger.rom.find((owner) =>
    owner.id === 'direct.consumable-metadata');
  const effectRamOwner = phase5aOwnerLedger.ram.find((owner) =>
    owner.id === 'effects.consumable-code');
  if (!effectOwner || effectOwner.start !== 0x41098 || effectOwner.bytes !== 1380 ||
      effectOwner.endExclusive !== 0x415FC || !effectRamOwner ||
      effectRamOwner.start !== 0x8016B198 || effectRamOwner.endExclusive !== 0x8016B6FC ||
      descriptionOwner.start !== 0x237744A || descriptionOwner.bytes !== 2064 ||
      descriptionOwner.endExclusive !== 0x2377C5A || !metadataOwner ||
      metadataOwner.start !== 0x645D0 || metadataOwner.endExclusive !== 0x648CC ||
      phase5aOwnerLedger.newRamBytes !== 0 || phase5aOwnerLedger.tailCodeBytes !== 0) {
    fail('accepted Phase 5A owner geometry drift');
  }
  if (phase5aRelocationLedger.effectSection.moved ||
      phase5aRelocationLedger.effectSection.relocations.length ||
      phase5aRelocationLedger.descriptionSlot.moved ||
      phase5aRelocationLedger.descriptionSlot.relocations.length ||
      phase5aRelocationLedger.tailCodeBytes !== 0 ||
      phase5aRelocationLedger.newRamBytes !== 0) {
    fail('accepted Phase 5A placement or relocation policy drift');
  }
  if (phase5aSourceModelLedger.owner !== effectOwner.id ||
      phase5aSourceModelLedger.completeSection.bytes !== effectOwner.bytes ||
      phase5aSourceModelLedger.modelOrder.length !== 10 ||
      phase5aSourceModelLedger.models.length !== 10 ||
      phase5aSourceModelLedger.sourceWords.length !== 16 ||
      phase5aSourceModelLedger.productionWritePolicy !==
        'The assembler emits the complete section. Sixteen fixed-offset writes exist only in the frozen legacy oracle.') {
    fail('accepted Phase 5A source-model ledger drift');
  }
  if (phase5aStaticMatrix.status !== 'pass' || phase5aStaticMatrix.casesPassed !== 50 ||
      phase5aStaticMatrix.effectSourceCases !== 31 ||
      phase5aStaticMatrix.acceptedLegacyOracleComparisons !== 32 ||
      phase5aReport.status !== 'static-build-pass') {
    fail('accepted Phase 5A static evidence drift');
  }
  if (phase5aConfig.owners.effectCode.id !== effectOwner.id ||
      phase5aConfig.owners.descriptions.id !== descriptionOwner.id ||
      phase5aConfig.owners.descriptions.streamCapacity !== 2055 ||
      phase5aConfig.lockedEffectRecords.length !== 16 ||
      phase5aConfig.hiddenRecords.length !== 13) {
    fail('accepted Phase 5A configuration contract drift');
  }

  const phase5aRetail = verifyProductFile(root, PHASE5A_EXACT_RETAIL, RETAIL_SHA256);
  if (!phase5aRetail.equals(retail)) fail('Phase 5A exact-retail product differs from accepted retail');
  const phase5aNative = verifyProductFile(root, PHASE5A_NATIVE,
    '6F7781ADB562EFD9131C03BC127CC3AF26AD32D575CA6BF5AC2EA60FB6843A27');
  const phase5aCanonical = verifyProductFile(root, PHASE5A_CANONICAL,
    '496EA6070FEB0EC5D689A821957BCD3101D9C03C0037357E72772B906731FB37');
  const retailEffect = Buffer.from(retail.subarray(effectOwner.start, effectOwner.endExclusive));
  const retailDescription = Buffer.from(retail.subarray(
    descriptionOwner.start, descriptionOwner.endExclusive));
  const nativeOwnerImages = [phase5aNative, phase5aCanonical].map((product, index) => ({
    product: index === 0 ? 'native-consumables' : 'canonical-native-consumables',
    romSha256: sha256(product),
    effectSha256: ownerImageIdentity(product, effectOwner.start, effectOwner.bytes).sha256,
    descriptionSha256: ownerImageIdentity(
      product, descriptionOwner.start, descriptionOwner.bytes).sha256,
  }));
  if (nativeOwnerImages[0].effectSha256 !== nativeOwnerImages[1].effectSha256 ||
      nativeOwnerImages[0].descriptionSha256 !== nativeOwnerImages[1].descriptionSha256) {
    fail('accepted Phase 5A native products disagree on consumable owner after-images');
  }

  return {
    generator: generatorIdentity,
    inputContract: inputContract.identity,
    ...(inputContract.contract.recoverySource
      ? { recoverySource: inputContract.contract.recoverySource } : {}),
    acceptedSource: {
      commit: identity.commit,
      tree: identity.tree,
      runtimeRdramBytes: 0x800000,
      retailBytes: retail.length,
      retailSha256: RETAIL_SHA256,
      inputs,
    },
    composition: {
      manifestId: composition.manifestId,
      manifestSha256: inputSha256(inputContract, 'config/lordly-native-composition.json'),
      order: composition.ordering.map((entry) => ({
        ordinal: entry.ordinal,
        step: entry.step,
        owners: entry.owners.slice(),
      })),
      productionOwnerIds: PRODUCTION_OWNER_IDS,
      owners: composition.owners.map((owner) => ({
        id: owner.id,
        order: owner.order,
        ownershipClass: owner.ownershipClass,
        generator: owner.generator,
        profile: owner.profiles['us-rev0'],
        rom: owner.rom.filter((region) =>
          !region.profiles || region.profiles.includes('us-rev0')).map((region) => ({
            start: region.start,
            size: region.size,
            rule: region.rule,
            label: region.label,
          })),
        ram: owner.ram.filter((region) =>
          !region.profiles || region.profiles.includes('us-rev0')).map((region) => ({
            start: region.start,
            size: region.size,
            rule: region.rule,
            label: region.label,
          })),
      })),
      migratedOwners: [
        'tool.experience-size-scale',
        'tool.chaos-frame-counter',
        'tool.character-card-luck',
        'tool.squad-menu-alignment',
        'runtime.shop-overrides',
        'effects.consumable-code',
        'text.consumable-descriptions',
      ],
      retiredOwners: ['effects.healing-descriptions'],
      remainingRuntimeContributors: phase4Config.remainingContributors.slice(),
      phase4Extension,
      phase4Matrix: {
        schemaVersion: phase4Matrix.schemaVersion,
        status: phase4Matrix.status,
        stateCount: phase4Matrix.stateCount,
        states: phase4Matrix.states.map((state) => ({
          id: state.id,
          nativeShops: state.nativeShops,
          remainingMask: state.remainingMask,
          remainingContributors: state.remainingContributors.slice(),
          runtimeMode: state.runtimeMode,
          sha256: state.sha256,
          changedBytes: state.changedBytes,
          ownerChangedBytes: state.ownerChangedBytes,
          deterministic: state.deterministic,
          crc6102: state.crc6102,
          legacyShopFindings: state.legacyShopFindings,
          attributionComplete: state.attributionComplete,
        })),
      },
      phase5aExtension: {
        baseManifestSha256: inputSha256(
          inputContract, 'config/lordly-native-composition.json'),
        acceptedCommit: identity.commit,
        acceptedTree: identity.tree,
        effectOwner: effectOwner.id,
        descriptionOwner: descriptionOwner.id,
        retiredDescriptionOwner: 'effects.healing-descriptions',
        mixedState: 'reject',
      },
    },
    approvals,
    experience: {
      owner: 'tool.experience-size-scale',
      toolId: 'experience-size-scale',
      generator: 'tools/lib/lordly_native_rebuild.js::generateTargetSource',
      romStart: experienceStart,
      ramStart: Number.parseInt(phase1Report.target.liveStart, 16),
      bytes: phase1Retail.bytes.length,
      minimumPercent: 0,
      maximumPercent: 300,
      retailPercent: 100,
      acceptedSource: {
        path: path.relative(root, phase1Retail.acceptedSource.path).replace(/\\/g, '/'),
        bytes: phase1Retail.acceptedSource.bytes,
        sha256: phase1Retail.acceptedSource.sha256,
      },
      retailHex: phase1Retail.bytes.toString('hex').toUpperCase(),
      proof200Sha256: sha256(phase1Modified.bytes),
      coverageSha256: phase1Coverage,
      sites: phase1Modified.sites.map((site) => ({
        label: site.label,
        romStart: site.romStart,
        originalWords: site.originalWords.map((word) => word >>> 0),
        modifiedWords: site.modifiedWords.map((word) => word >>> 0),
      })),
    },
    phase3: {
      generator: phase3.generator,
      payloadSha256: inputSha256(inputContract, PHASE3_PAYLOAD),
      components: phase3Components,
      toolOwnerMap: phase3Components.reduce((out, component) => {
        out[selectedOwner(component.owner)] = component.owner;
        return out;
      }, {}),
      matrix,
    },
    shops: {
      owner: 'runtime.shop-overrides',
      generator: phase4.generator,
      payloadSha256: inputSha256(inputContract, PHASE4_PAYLOAD),
      contractSha256: inputSha256(
        inputContract, 'docs/contracts/lordly-native-shop-membership-v1.json'),
      components: phase4Components,
      module: {
        romStart: sourceModule.romStart,
        budget: sourceModule.budget,
        loadedBytes: sourceModule.loadedBytes,
        contractOffset: sourceModule.contractOffset,
        contractRomStart: sourceModule.romStart + sourceModule.contractOffset,
        metricsOffset: sourceModule.metricsOffset,
        expendableArrayOffset: sourceModule.expendableArrayOffset,
      },
      legacy: Object.assign({}, phase4Config.owner.legacyShop, {
        retailLaneHex: retail.subarray(
          phase4Config.owner.legacyShop.sharedTailRom +
            phase4Config.owner.legacyShop.resolverOffset,
          phase4Config.owner.legacyShop.sharedTailRom +
            phase4Config.owner.legacyShop.laneEndOffset
        ).toString('hex').toUpperCase(),
      }),
      contract: phase4Config.contract,
      contractIdentityHex: embeddedContract.subarray(0x28, 0x38).toString('hex').toUpperCase(),
      compatibility: contract.compatibility,
      products: phase4Products,
    },
    consumables: {
      generator: 'tools/lib/lordly_native_phase5a_consumables.js::build',
      effect: {
        owner: effectOwner.id,
        section: effectOwner.section,
        romStart: effectOwner.start,
        ramStart: effectRamOwner.start,
        bytes: effectOwner.bytes,
        retailSha256: sha256(retailEffect),
        retailHex: retailEffect.toString('hex').toUpperCase(),
        modelOrder: phase5aSourceModelLedger.modelOrder.slice(),
        models: phase5aSourceModelLedger.models.map((model) => Object.assign({}, model)),
        sourceWords: phase5aSourceModelLedger.sourceWords.map((word) => ({
          modelKey: word.modelKey,
          role: word.role,
          buildInput: word.buildInput,
          romStart: word.z64RomOffset,
          ramStart: word.ramVirtualAddress,
          expression: word.expression,
        })),
        productionWritePolicy: phase5aSourceModelLedger.productionWritePolicy,
      },
      descriptions: {
        owner: descriptionOwner.id,
        retiredOwner: 'effects.healing-descriptions',
        romStart: descriptionOwner.start,
        bytes: descriptionOwner.bytes,
        streamCapacity: phase5aConfig.owners.descriptions.streamCapacity,
        recordCount: 50,
        retailSha256: sha256(retailDescription),
        retailHex: retailDescription.toString('hex').toUpperCase(),
        logicalOrder: ['manual-state', 'healing-synchronization'],
      },
      metadata: {
        owner: metadataOwner.id,
        romStart: metadataOwner.start,
        bytes: metadataOwner.bytes,
        mode: 'passthrough',
      },
      acceptedSource: {
        commit: identity.commit,
        tree: identity.tree,
        artifactManifestSha256: inputSha256(
          inputContract, PHASE5A_ROOT + '/artifact-manifest.json'),
        buildReportSha256: inputSha256(inputContract, PHASE5A_REPORT),
        ownerLedgerSha256: inputSha256(inputContract, PHASE5A_OWNER_LEDGER),
        relocationLedgerSha256: inputSha256(inputContract, PHASE5A_RELOCATION_LEDGER),
        sourceModelLedgerSha256: inputSha256(inputContract, PHASE5A_SOURCE_MODEL_LEDGER),
        staticMatrixSha256: inputSha256(inputContract, PHASE5A_STATIC_MATRIX),
        products: {
          exactRetail: RETAIL_SHA256,
          nativeConsumables: nativeOwnerImages[0].romSha256,
          canonicalNativeConsumables: nativeOwnerImages[1].romSha256,
        },
        nativeOwnerImages,
      },
    },
  };
}

function applyAlignmentCorrection(payload, file, runtime = false) {
  const correction = JSON.parse(fs.readFileSync(file, 'utf8'));
  if (correction.schemaVersion !== 1 || correction.contractId !== 'lordly-native-rebuild.squad-alignment.v10' ||
      correction.status !== 'pass' || correction.review !== 'pending' || correction.runtime !== 'not-tested' ||
      correction.module.version !== 10 || correction.module.bytes !== 3328 || correction.components.length !== 2) {
    fail('Alignment correction contract drift');
  }
  const expectedComponents = {'source-loader':'32917C8B0B4B70551C5AF40F5EF5639EC654FEFA02E18A420CD40E2BABF1BB14', 'source-module':'7B934CDE557BCF400DFCCE5F0E8DA5723A048E3E9F07E181DEB574C500EEAEB8'};
  if (new Set(correction.components.map(x=>x.before.name)).size !== 2) fail('duplicate Alignment component');
  const supersededComponents = JSON.parse(JSON.stringify(payload.phase3.components.filter(x=>x.owner==='tool.squad-menu-alignment')));
  const historicalProducts = {phase3: JSON.parse(JSON.stringify(payload.phase3.matrix)),
    phase4: JSON.parse(JSON.stringify(payload.shops.products || [])),
    consumables: JSON.parse(JSON.stringify(payload.consumables.acceptedSource.products))};
  for (const change of correction.components) {
    const before = change.before, after = change.after;
    const target = payload.phase3.components.find(item => item.owner === before.owner && item.name === before.name);
    if (!target || before.owner !== 'tool.squad-menu-alignment' ||
        !['source-loader','source-module'].includes(before.name) ||
        target.romStart !== before.romStart || target.ramStart !== before.ramStart ||
        target.bytes !== before.bytes || target.dataHex !== before.dataHex ||
        after.romStart !== before.romStart || after.ramStart !== before.ramStart ||
        after.bytes !== before.bytes || Buffer.from(after.dataHex,'hex').length !== before.bytes ||
        sha256(Buffer.from(after.dataHex,'hex')) !== after.sha256 || after.sha256 !== expectedComponents[before.name]) fail('Alignment component owner or preimage drift');
    const linked = fs.readFileSync(change.linked.path);
    if (sha256(linked) !== after.sha256 || linked.toString('hex').toUpperCase() !== after.dataHex) {
      fail('Alignment linked component identity drift');
    }
    target.dataHex = after.dataHex; target.sha256 = after.sha256;
    if (before.name === 'source-module') target.payloadBytes = 3328;
    target.payloadSha256 = sha256(Buffer.from(target.dataHex, 'hex').subarray(0, target.payloadBytes));
  }
  const replacements = new Map();
  for (const entry of Object.values(correction.products)) {
    if (!entry.unrelatedBytesPreserved || !/^[A-F0-9]{64}$/.test(entry.beforeSha256) ||
        sha256(fs.readFileSync(entry.path)) !== entry.sha256) fail('Alignment corrected product identity drift');
    const existing = replacements.get(entry.beforeSha256);
    if (existing && existing !== entry.sha256) fail('Alignment product mapping conflict');
    replacements.set(entry.beforeSha256, entry.sha256);
  }
  function update(value) {
    if (typeof value === 'string') return replacements.get(value) || value;
    if (Array.isArray(value)) return value.map(update);
    if (value && typeof value === 'object') return Object.fromEntries(Object.entries(value).map(([k,v])=>[k,update(v)]));
    return value;
  }
  // Update product identities only. Approval and historical input ledgers retain their own identities.
  payload.phase3.matrix = update(payload.phase3.matrix);
  payload.shops.products = update(payload.shops.products);
  const currentConsumableOwnerImages = update(payload.consumables.acceptedSource.nativeOwnerImages).filter((x,i)=>x.romSha256!==payload.consumables.acceptedSource.nativeOwnerImages[i].romSha256);
  payload.phase3.historicalSource = {generator:payload.phase3.generator, payloadSha256:payload.phase3.payloadSha256};
  payload.phase3.generator = 'tools/build_squad_alignment_correction.py';
  payload.phase3.payloadSha256 = sha256(Buffer.from(JSON.stringify(payload.phase3.components)));
  payload.alignmentCorrection = {contractId:correction.contractId, version:10, review:'pending', runtime:'not-tested',
    reportSha256:sha256(fs.readFileSync(file)), generator:correction.generator, parentGenerator:correction.parentGenerator,
    historicalProducts, supersededComponents, currentConsumableOwnerImages, products:correction.products};
  if (runtime) {
    delete payload.alignmentCorrection.reportSha256;
    for (const key of ['generator','parentGenerator']) payload.alignmentCorrection[key].path = key === 'generator' ? 'tools/build_squad_alignment_correction.py' : 'tools/luck_alignment_display_patch.py';
    payload.alignmentCorrection.editorGenerator = identifyGeneratorSource(fs.readFileSync(__filename));
    payload.alignmentCorrection.products = Object.fromEntries(Object.entries(correction.products).map(([key,value])=>[key,{bytes:value.bytes,beforeSha256:value.beforeSha256,sha256:value.sha256,alignmentEnabled:value.alignmentEnabled,unrelatedBytesPreserved:value.unrelatedBytesPreserved}]));
  }
  return payload;
}

function historicalRuntimePayload() {
  const cp = require('child_process'), vm = require('vm');
  const source = cp.execFileSync('git', ['-C',path.resolve(__dirname,'..'),'show',
    'aaeffde75b394dc2c41162933299d099c317d792:native-rebuild-data.js']);
  if (sha256(source) !== '354BD5A73CBB103A7C08C200BE354CEA1F1BBAFF6D0DCEA2B5F4FDEE3127984F') fail('historical runtime bundle identity drift');
  const context = {}; vm.runInNewContext(source.toString('utf8'),context);
  return JSON.parse(JSON.stringify(context.OB64_NATIVE_REBUILD_DATA.payload));
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const inputContract = loadInputContract(args.inputRoot, args.inputContract);
  const identity = Object.assign({}, inputContract.contract.nativeSource);
  const inputs = verifyInputs(args.inputRoot, inputContract);
  const approvals = verifyApprovals(args.inputRoot, inputContract);
  const generatorIdentity = inputContract.contract.schemaVersion === INPUT_CONTRACT_VERSION
    ? Object.assign({}, LEGACY_GENERATOR_IDENTITY)
    : Object.assign({ path: 'tools/generate-native-rebuild-data.js' },
      identifyGeneratorSource(fs.readFileSync(__filename)));
  const payload = args.runtime ? historicalRuntimePayload() : buildPayload(
    args.inputRoot, identity, inputs, approvals, generatorIdentity, inputContract);
  if (args.runtime && !args.alignmentCorrection) fail('runtime generation requires an explicit Alignment correction');
  if (args.alignmentCorrection) applyAlignmentCorrection(payload, args.alignmentCorrection, args.runtime);
  const payloadSha256 = sha256(Buffer.from(JSON.stringify(payload), 'utf8'));
  const bundle = { schema: 'ob64-native-rebuild-data', schemaVersion: 1, payloadSha256, payload };
  const source = [
    '/* GENERATED FILE. DO NOT EDIT. */',
    '/* Generator: tools/generate-native-rebuild-data.js */',
    '(function(root) {',
    "  'use strict';",
    '  var data = ' + JSON.stringify(bundle, null, 2).replace(/\n/g, '\n  ') + ';',
    '  root.OB64_NATIVE_REBUILD_DATA = data;',
    "  if (typeof module !== 'undefined' && module.exports) module.exports = data;",
    "})(typeof window !== 'undefined' ? window : globalThis);",
    '',
  ].join('\n');
  fs.mkdirSync(path.dirname(args.out), { recursive: true });
  fs.writeFileSync(args.out, source, 'utf8');
  process.stdout.write(JSON.stringify({
    output: args.out,
    currentRuntimeCorrection: !!args.runtime,
    bytes: Buffer.byteLength(source),
    sha256: sha256(Buffer.from(source)),
    payloadSha256,
    acceptedCommit: identity.commit,
    acceptedTree: identity.tree,
    inputCount: inputs.length,
    inputContractId: inputContract.identity.id,
    inputContractSha256: inputContract.identity.sha256,
    inputContractCanonicalBytes: inputContract.identity.canonicalBytes,
    generatorSourceIdentity: generatorIdentity.sourceIdentity,
    generatorCanonicalBytes: generatorIdentity.canonicalBytes,
    generatorSha256: generatorIdentity.sha256,
    approvals: approvals.map((approval) => ({
      phase: approval.phase,
      sha256: approval.sha256,
    })),
  }, null, 2) + '\n');
}

module.exports = Object.freeze({
  EXPECTED_INPUT_CONTRACT_SHA256,
  GENERATOR_SOURCE_IDENTITY,
  LEGACY_GENERATOR_IDENTITY,
  canonicalizeGeneratorSource,
  identifyGeneratorSource,
  loadInputContract,
  applyAlignmentCorrection,
});

if (require.main === module) main();
