#!/usr/bin/env node
'use strict';

// Generates the browser-safe Phase 1 through Phase 4 native-owner bundle.
// The generated file contains no complete ROM. It contains only authenticated
// owner payloads, their retail preimages, and composition metadata.

const childProcess = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const EXPECTED_COMMIT = '05623e67b295f4f968e4a5cb416b9c002fb09bdd';
const EXPECTED_TREE = '90b193077b61eee0f6656a8d3f6848b2f17bdfaa';
const RETAIL_SHA256 = '571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A';
const PHASE3_APPROVAL_SHA256 =
  'B458CD6BFC5964938D61BE855AD9792D59A2509D73CF6FCB57844275A97D2ABA';
const PHASE4_APPROVAL_SHA256 =
  'B1ED23C9248BF7D893CF1EE9DBEF2ED8AE2D266B84EFF4045FE03BD348797DEF';
const PHASE3_APPROVED_COMMIT = '000fb0dc2ef5cb3d658e0e0ffbf6b49d84c600fb';
const PHASE3_APPROVED_TREE = '917b52c27df857c6793f6320dc68e61244eb25a1';
const GENERATOR_SOURCE_IDENTITY = 'sha256-canonical-utf8-lf-v1';

const INPUTS = Object.freeze({
  'config/lordly-native-composition.json':
    'DBCA8D456D8D786DE9A69130CBABF9CCF6FB27B8E746DEC0E0616511B4D420A3',
  'config/lordly-native-phase3.json':
    '831C896339C6F106A3D361D1A76C058E2EE8F77F1F3C2905E418450D12E76259',
  'config/lordly-native-phase4-shops.json':
    'FE5E08569B8AD75F1BA0E9BA07FF46B814A085404B30ED5257B9EE2D9BBDBCCE',
  'docs/contracts/lordly-native-shop-membership-v1.json':
    'C6D71952A0AE7A813C3F79D8EEAAA4CCAADF53168E2AA627BE577726F64FD200',
  'tools/lib/lordly_native_rebuild.js':
    '611F207C4FE67AC8DDC106F968EBBD1608EDECA549CA6C86F429E49D95DE6001',
  'build/lordly-native-rebuild/phase1/build-report.json':
    'FA3A3BF5B77AC973F4E197ECF82EA0873EB91E53C9BD70E8B283EB9C9AC87B29',
  'build/lordly-native-rebuild/phase1/artifact-manifest.json':
    '2EAB291E0609CA2CAF8422F5DA7C8E220DE1951967E3B6D4F33511D3484D351D',
  'build/lordly-native-rebuild/phase1/relocations.json':
    '15A3650FE13FF850CB0988DE8665FFCFCCDBF791A64F66ABA3180D1C9656972E',
  'build/lordly-native-rebuild/phase1/retail/layout.json':
    'CF5A7462E49F7F47F5E072D55843455B1950E86DC9D12B9CF9800A9A5FA01206',
  'build/lordly-native-rebuild/phase1/retail/ram-layout.json':
    '61F7A42F6D7AE9F71CD11FBEBBA635CB513FC497DF77D291E02BF10385E03802',
  'build/lordly-native-rebuild/phase1/modified/layout.json':
    '4FB3576F2CFB236CA158484CB73E06244D205B0B90A92A907669923C790895E0',
  'build/lordly-native-rebuild/phase1/modified/ram-layout.json':
    '874FF4DA02066744536BC49E9190BEDEAC49AEFD8153995F6D87E24AD077105A',
  'build/lordly-native-rebuild/phase1/inputs/accepted-current/layout.json':
    'F614C7076999294721A2F9DAFCF7EABF9136A77217124C583CC980F7906CBC27',
  'build/lordly-native-rebuild/phase1/inputs/accepted-current/objects/manifest.json':
    '1ABFBB9BDB47DE2BC6D9C1837277DE020B1D8F626838925F2DE180A2B25C7F7C',
  'build/lordly-native-rebuild/phase2-correction-r1/build-report.json':
    'C8E92CEEA3ED76AD4187F86040E4A20FDB27452FADE7C7CB84973A362FA84F2B',
  'build/lordly-native-rebuild/phase2-correction-r1/artifact-manifest.json':
    '7FD7D2585643A5778F6C437CDDE1B317ACBC35CF8C8638A4F2BD5DABF6808082',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/native-payload.json':
    '427694EB8D127EC7FF8BCD068CE22F14656B893D1D0E9D40319CBED6E2753E2B',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/build-report.json':
    '9AF435EA6980A9258ED605FED355B2486C0F9AD10AF291FDA13BBE54E6EF631E',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/artifact-manifest.json':
    '95935D41809577733A9BE843A73038140B5CC51A5A6D926741112B38AA42ACDC',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/relocation-ledger.json':
    '4954C19921A46410B9A2D32C83BE68CE2C77679AAB68838A2F4A8DC930BDF872',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-000.json':
    'E6A70B5851103C5E75B1CCA22A3A8ADEABADDB94891ABB52E8B845EE5D06AE9F',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-001.json':
    '22E695F7011547B62B4CF8108FBDF85DA79D38305E7D28F6115F5431DFED0602',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-010.json':
    '24E409B6F4953AB15791AEA1E87ABE5259EF20DA380BFC2E39591BC9F0170B2A',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-011.json':
    '0639360E3F2EF3B88517F09A14D9D987AFEAD1C8BBA8A15A240E0D2FDAFE85B0',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-100.json':
    '651C1A98DC28937FAC38F3F4829598D6773E6BDA53197D4561B5C1E628F5066B',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-101.json':
    'F448D479B5F1FE7F83A1D2BC25E2A4E3863C272A52ED24087FED90AC7DB580E7',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-110.json':
    'C8F211FFCE1F833FCB2A9DE0A56DB736D7D87787012F3AD77979373B29A94B13',
  'build/lordly-native-rebuild/phase3-luck-alignment-chaos-native-correction-r1/layout/migrated-111.json':
    '39E5D48AADBC77324D7EB9972AD51D0B9782B2658EB141605E22381811A12ED9',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/native-shop-payload.json':
    'A3154FD9FF6D115FB5BF682FBA174F834A56340CD103D1BDBEBB726148241322',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/build-report.json':
    '6DBCB2FEF7D03C1277B63AE7B91C8AFF7D0515BC17065D6FF1016FA9F9D54BE3',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/composition-matrix.json':
    '86419B194D7984FB0CD939AADC150D231472A280363D7BEE8D63B30617FAF96E',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/artifact-manifest.json':
    '83B91092381359D6FCD28FA8D74D21D5B02E2F229B70ECA3980FE42E96DBB497',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/relocation-ledger.json':
    '20BCD6ED0611A1AEA42B682EB66BB075B2B7D833B10AE674BBD90F3D27631B98',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/layout/exact-retail.json':
    '8B2D600640BBAC0C9A205C3D89BDB83889C9450A3213BCA02C80DAE1E92D7D10',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/layout/native-shops.json':
    'BF0941542E448D84FF3073BA2EBD871925700C14ADFE1174791906D8F7BE560D',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/layout/canonical-native-shops.json':
    'FA2A98393B549143B4D2A0D507E2366479927DB4B6361ECA054DB86642C5868F',
  'build/lordly-native-rebuild/phase4-native-shops-tooling-correction-r2/verification-report.json':
    '9B5E93C1FB60C0CF51B62890628674874A21E2BC6097491AEBC2D8D89D1AF6ED',
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
    if (name === '--decomp' || name === '--out' ||
        name === '--phase3-approval' || name === '--phase4-approval') {
      if (!argv[index + 1]) fail(name + ' needs a path');
      const key = {
        '--decomp': 'decomp',
        '--out': 'out',
        '--phase3-approval': 'phase3Approval',
        '--phase4-approval': 'phase4Approval',
      }[name];
      args[key] = path.resolve(argv[++index]);
    } else {
      fail('unknown argument ' + name);
    }
  }
  if (!args.decomp) fail('--decomp is required');
  if (!args.phase3Approval) fail('--phase3-approval is required');
  if (!args.phase4Approval) fail('--phase4-approval is required');
  if (!args.out) args.out = path.resolve(__dirname, '..', 'native-rebuild-data.js');
  return args;
}

function verifyApproval(target, expectedSha256, phase, expectedCommit, expectedTree) {
  if (!fs.existsSync(target)) fail('missing ' + phase + ' source-approval record');
  const bytes = fs.readFileSync(target);
  const actual = sha256(bytes);
  if (actual !== expectedSha256) fail(phase + ' source-approval identity drift');
  const record = JSON.parse(bytes.toString('utf8'));
  if (record.approvedCommit !== expectedCommit || record.approvedTree !== expectedTree) {
    fail(phase + ' source-approval commit or tree drift');
  }
  return {
    phase,
    filename: path.basename(target),
    bytes: bytes.length,
    sha256: actual,
    approvedCommit: record.approvedCommit,
    approvedTree: record.approvedTree,
  };
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

function read(root, relative) {
  const target = path.resolve(root, relative);
  if (path.relative(root, target).startsWith('..')) fail('input escaped the decomp root');
  if (!fs.existsSync(target)) fail('missing input ' + relative);
  return fs.readFileSync(target);
}

function readJson(root, relative) {
  return JSON.parse(read(root, relative).toString('utf8'));
}

function git(root, args) {
  return childProcess.execFileSync('git', ['-C', root].concat(args), {
    encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'],
  }).trim();
}

function verifyRoot(root) {
  const top = path.resolve(git(root, ['rev-parse', '--show-toplevel']));
  if (top.toLowerCase() !== path.resolve(root).toLowerCase()) {
    fail('decomp path is not its Git worktree root');
  }
  const commit = git(root, ['rev-parse', 'HEAD']);
  const tree = git(root, ['rev-parse', 'HEAD^{tree}']);
  if (commit !== EXPECTED_COMMIT) fail('decomp commit drift: ' + commit);
  if (tree !== EXPECTED_TREE) fail('decomp tree drift: ' + tree);
  return { commit, tree };
}

function verifyInputs(root) {
  return Object.keys(INPUTS).map((relative) => {
    const bytes = read(root, relative);
    const actual = sha256(bytes);
    if (actual !== INPUTS[relative]) fail('input identity drift: ' + relative);
    return { path: relative, bytes: bytes.length, sha256: actual };
  });
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

function buildPayload(root, identity, inputs, approvals, generatorIdentity) {
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
  const phase4Extension = phase4Config.compositionExtension;
  if (!phase4Extension ||
      phase4Extension.baseManifestSha256 !==
        INPUTS['config/lordly-native-composition.json'] ||
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

  return {
    generator: generatorIdentity,
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
      manifestSha256: INPUTS['config/lordly-native-composition.json'],
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
      ],
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
      payloadSha256: INPUTS[PHASE3_PAYLOAD],
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
      payloadSha256: INPUTS[PHASE4_PAYLOAD],
      contractSha256: INPUTS['docs/contracts/lordly-native-shop-membership-v1.json'],
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
  };
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const identity = verifyRoot(args.decomp);
  const inputs = verifyInputs(args.decomp);
  const approvals = [
    verifyApproval(args.phase3Approval, PHASE3_APPROVAL_SHA256, 'Phase 3',
      PHASE3_APPROVED_COMMIT, PHASE3_APPROVED_TREE),
    verifyApproval(args.phase4Approval, PHASE4_APPROVAL_SHA256, 'Phase 4',
      EXPECTED_COMMIT, EXPECTED_TREE),
  ];
  const generatorIdentity = Object.assign({
    path: 'tools/generate-native-rebuild-data.js',
  }, identifyGeneratorSource(fs.readFileSync(__filename)));
  const payload = buildPayload(args.decomp, identity, inputs, approvals, generatorIdentity);
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
    bytes: Buffer.byteLength(source),
    sha256: sha256(Buffer.from(source)),
    payloadSha256,
    acceptedCommit: identity.commit,
    acceptedTree: identity.tree,
    inputCount: inputs.length,
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
  GENERATOR_SOURCE_IDENTITY,
  canonicalizeGeneratorSource,
  identifyGeneratorSource,
});

if (require.main === module) main();
