'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const ROM_PATHS = {
  'us-rev0': path.join(
    ROOT,
    'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'
  ),
  'us-rev1': path.join(
    ROOT,
    'Rev 1.1',
    'Ogre Battle 64 - Person of Lordly Caliber (USA) (Rev 1).z64'
  ),
};

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js',
  'parsers.js',
  'repack.js',
  'source-redirect.js',
  'stat-gate-relocation.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

let failures = 0;

function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

function expectThrow(name, action, fragment) {
  let message = '';
  try {
    action();
  } catch (error) {
    message = error && error.message || String(error);
  }
  check(name, !!message && (!fragment || message.includes(fragment)), message);
}

function equalBytes(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

function sha256(bytes) {
  return crypto.createHash('sha256').update(Buffer.from(bytes))
    .digest('hex').toUpperCase();
}

function readU32(bytes, offset) {
  return (((bytes[offset] << 24) >>> 0) |
    (bytes[offset + 1] << 16) |
    (bytes[offset + 2] << 8) |
    bytes[offset + 3]) >>> 0;
}

function loadRom(layoutId) {
  const bytes = new Uint8Array(fs.readFileSync(ROM_PATHS[layoutId]));
  const rom = OB64.loadROM(bytes.slice().buffer);
  if (rom.layout.id !== layoutId) {
    throw new Error('Expected ' + layoutId + ', loaded ' + rom.layout.id);
  }
  return rom;
}

function modelFromBytes(base, decoded) {
  const byClass = {};
  const fields = ['str', 'vit', 'int', 'men', 'agi', 'dex', 'alnMin', 'alnMax'];
  for (let classId = 0; classId < 81; classId++) {
    const offset = classId * 8;
    const row = { classId, offset };
    fields.forEach((field, index) => { row[field] = decoded[offset + index]; });
    byClass[classId] = row;
  }
  return {
    raw: decoded.slice(),
    byClass,
    meta: base.meta,
  };
}

function sequentialEdits(stock, count) {
  const bytes = stock.slice();
  for (let index = 0; index < count; index++) {
    const offset = 8 + index;
    bytes[offset] = bytes[offset] === 0xFF ? 0xFE : bytes[offset] + 1;
  }
  return bytes;
}

function deBruijn(k, n) {
  const a = new Array(k * n + 1).fill(0);
  const sequence = [];
  function db(t, p) {
    if (t > n) {
      if (n % p === 0) {
        for (let j = 1; j <= p; j++) sequence.push(a[j]);
      }
      return;
    }
    a[t] = a[t - p];
    db(t + 1, p);
    for (let j = a[t - p] + 1; j < k; j++) {
      a[t] = j;
      db(t + 1, t);
    }
  }
  db(1, 1);
  return sequence;
}

function maximumWitness() {
  return Uint8Array.from(deBruijn(9, 3).slice(0, 648), value => value + 1);
}

function verifyHeaderCrc(z64) {
  const expected = z64.slice(0x10, 0x18);
  const copy = z64.slice();
  OB64.recalcN64CRC(copy);
  return equalBytes(expected, copy.subarray(0x10, 0x18));
}

function applyStatPlan(rom, model) {
  const candidate = rom.z64.slice();
  const statPlan = OB64.statGateRelocation.prepare(
    model,
    candidate,
    rom.layout
  );
  OB64.statGateRelocation.apply(statPlan, candidate);
  const currentRequests = OB64.statGateRelocation.currentRequests(rom.statGates);
  const redirectPlan = OB64.sourceRedirect.prepare(
    candidate,
    statPlan.redirectRequests,
    { knownCurrentRequests: currentRequests }
  );
  const redirectResult = OB64.sourceRedirect.apply(candidate, redirectPlan);
  if (redirectResult.crc) OB64.recalcN64CRC(candidate);
  return { candidate, statPlan, redirectPlan, redirectResult };
}

function fixtureCases(rom) {
  const stock = rom.statGates.raw;
  return {
    boundary376: sequentialEdits(stock, 3),
    boundary377: sequentialEdits(stock, 4),
    relocatedEven: sequentialEdits(stock, 5),
    maximum659: maximumWitness(),
  };
}

function runRevisionMatrix(layoutId, summaries) {
  const rom = loadRom(layoutId);
  const cases = fixtureCases(rom);
  const expected = {
    boundary376: {
      logical: 376, stored: 376, payload: 380, container: 384,
      mode: 'in-place', redirects: 0,
    },
    boundary377: {
      logical: 377, stored: 378, payload: 382, container: 386,
      mode: 'relocated', redirects: 2,
    },
    relocatedEven: {
      logical: 378, stored: 378, payload: 382, container: 386,
      mode: 'relocated', redirects: 2,
    },
    maximum659: {
      logical: 659, stored: 660, payload: 664, container: 668,
      mode: 'relocated', redirects: 3,
    },
  };

  const profile = OB64.statGateRelocation.profileFor(rom.z64, rom.layout);
  check(layoutId + ' uses an explicit revision profile',
    profile.id === layoutId && profile.revision === rom.layout.version,
    JSON.stringify(profile));
  check(layoutId + ' retail table resolves in place',
    rom.statGates.meta.relocation.state === 'in-place' &&
      rom.statGates.meta.logicalStreamBytes === 376,
    JSON.stringify(rom.statGates.meta.relocation));

  summaries.revisions[layoutId] = {
    profile: {
      revision: profile.revision,
      headerOffset: profile.headerOffset,
      sizeSourceOffset: profile.sizeSourceOffset,
      payloadSourceOffset: profile.payloadSourceOffset,
      secondPayloadSourceOffset: profile.secondPayloadSourceOffset,
    },
    cases: {},
  };

  for (const [name, decoded] of Object.entries(cases)) {
    const built = applyStatPlan(rom, modelFromBytes(rom.statGates, decoded));
    const wanted = expected[name];
    const plan = built.statPlan;
    check(layoutId + ' ' + name + ' chooses ' + wanted.mode,
      plan.mode === wanted.mode, plan.mode);
    check(layoutId + ' ' + name + ' has exact bounded sizes',
      plan.logicalStreamBytes === wanted.logical &&
        plan.storedStreamBytes === wanted.stored &&
        plan.payloadBytes === wanted.payload &&
        plan.containerBytes === wanted.container,
      JSON.stringify({
        logical: plan.logicalStreamBytes,
        stored: plan.storedStreamBytes,
        payload: plan.payloadBytes,
        container: plan.containerBytes,
      }));
    check(layoutId + ' ' + name + ' has the required redirect count',
      plan.redirectRequests.length === wanted.redirects,
      String(plan.redirectRequests.length));
    check(layoutId + ' ' + name + ' preserves a valid CIC-6102 checksum',
      verifyHeaderCrc(built.candidate));

    const validation = OB64.statGateRelocation.validate(
      built.candidate,
      rom.layout,
      decoded
    );
    check(layoutId + ' ' + name + ' validates after writing',
      validation.ok, validation.reason);
    const parsed = OB64.statGateRelocation.parse(built.candidate, rom.layout);
    check(layoutId + ' ' + name + ' parses the exact 648-byte table',
      parsed.raw.length === 648 && equalBytes(parsed.raw, decoded));
    check(layoutId + ' ' + name + ' has the expected ownership state',
      parsed.meta.relocation.state ===
        (wanted.mode === 'relocated' ? 'owned' : 'in-place'),
      parsed.meta.relocation.state);
    check(layoutId + ' ' + name + ' shared redirect validates exactly',
      OB64.sourceRedirect.validate(
        built.candidate,
        plan.redirectRequests
      ).ok);

    const originalHeader = rom.z64.subarray(
      profile.headerOffset,
      profile.headerOffset + 8
    );
    check(layoutId + ' ' + name + ' preserves the two retail header words',
      equalBytes(
        originalHeader,
        built.candidate.subarray(profile.headerOffset, profile.headerOffset + 8)
      ));

    const constants = OB64.statGateRelocation.constants;
    const owner = built.candidate.subarray(
      constants.OWNER_START,
      constants.OWNER_START + constants.OWNER_SIZE
    );
    if (wanted.mode === 'in-place') {
      check(layoutId + ' ' + name + ' leaves no relocation artifact',
        owner.every(byte => byte === 0xFF) &&
          OB64.sourceRedirect.classify(built.candidate).state === 'retail');
    } else {
      check(layoutId + ' ' + name + ' publishes exact container headers',
        readU32(built.candidate, constants.OWNER_START) === wanted.payload &&
          readU32(built.candidate, constants.OWNER_START + 4) === 648);
    }

    summaries.revisions[layoutId].cases[name] = {
      logicalStreamBytes: plan.logicalStreamBytes,
      storedStreamBytes: plan.storedStreamBytes,
      payloadBytes: plan.payloadBytes,
      containerBytes: plan.containerBytes,
      redirectCount: plan.redirectRequests.length,
      decodedSha256: sha256(decoded),
      compressedSha256: sha256(plan.compressed),
      descriptorSha256: plan.descriptorBytes
        ? sha256(plan.descriptorBytes)
        : null,
      ownerSha256: plan.ownerBytes ? sha256(plan.ownerBytes) : null,
    };
  }

  for (const byteOrder of ['z64', 'v64', 'n64']) {
    const orderedInput = OB64.serializeRomImage(rom.z64, byteOrder);
    const normalized = OB64.normalizeRomImage(orderedInput);
    const orderedModel = OB64.statGateRelocation.parse(
      normalized.z64,
      rom.layout
    );
    const plan = OB64.statGateRelocation.prepare(
      modelFromBytes(orderedModel, cases.boundary377),
      normalized.z64,
      rom.layout
    );
    OB64.statGateRelocation.apply(plan, normalized.z64);
    const redirect = OB64.sourceRedirect.prepare(
      normalized.z64,
      plan.redirectRequests,
      { knownCurrentRequests: [] }
    );
    const redirectResult = OB64.sourceRedirect.apply(normalized.z64, redirect);
    if (redirectResult.crc) OB64.recalcN64CRC(normalized.z64);
    const orderedOutput = OB64.serializeRomImage(
      normalized.z64,
      normalized.byteOrder
    );
    const reopened = OB64.normalizeRomImage(orderedOutput);
    check(layoutId + ' preserves ' + byteOrder + ' export byte order',
      normalized.byteOrder === byteOrder &&
        reopened.byteOrder === byteOrder &&
        equalBytes(reopened.z64, normalized.z64));
  }
}

function runCompositionAndRestoration() {
  const rom = loadRom('us-rev0');
  const maxModel = modelFromBytes(rom.statGates, maximumWitness());
  const candidate = rom.z64.slice();
  const statPlan = OB64.statGateRelocation.prepare(maxModel, candidate, rom.layout);
  OB64.statGateRelocation.apply(statPlan, candidate);

  const scenarioRequest = {
    owner: 'scenario',
    sourceRomOffset: 0x0274783E,
    destinationRomOffset: 0x027C0000,
    label: 'Synthetic Scenario fixture',
  };
  const combinedPlan = OB64.sourceRedirect.prepare(
    candidate,
    statPlan.redirectRequests.concat([scenarioRequest]),
    { knownCurrentRequests: [] }
  );
  const combinedResult = OB64.sourceRedirect.apply(candidate, combinedPlan);
  if (combinedResult.crc) OB64.recalcN64CRC(candidate);
  check('stat-only plus Scenario-only requests share one installed controller',
    combinedResult.entryCount === statPlan.redirectRequests.length + 1 &&
      OB64.sourceRedirect.classify(candidate).state === 'installed');
  check('combined controller still resolves the owned stat container',
    OB64.statGateRelocation.validate(
      candidate,
      rom.layout,
      maximumWitness()
    ).ok);

  const activeModel = OB64.statGateRelocation.parse(candidate, rom.layout);
  const fittingBytes = sequentialEdits(rom.statGates.raw, 3);
  const fittingModel = modelFromBytes(activeModel, fittingBytes);
  const removeStatPlan = OB64.statGateRelocation.prepare(
    fittingModel,
    candidate,
    rom.layout
  );
  OB64.statGateRelocation.apply(removeStatPlan, candidate);
  const removeStatRedirect = OB64.sourceRedirect.prepare(
    candidate,
    [scenarioRequest],
    {
      knownCurrentRequests: statPlan.redirectRequests.concat([scenarioRequest]),
    }
  );
  const removalResult = OB64.sourceRedirect.apply(
    candidate,
    removeStatRedirect
  );
  if (removalResult.crc) OB64.recalcN64CRC(candidate);
  check('removing stat relocation preserves the Scenario entry',
    OB64.sourceRedirect.validate(candidate, [scenarioRequest]).ok &&
      OB64.sourceRedirect.validateSubset(candidate, [scenarioRequest]).entryCount === 1);
  check('removing stat relocation restores its complete 1 KiB owner',
    candidate.subarray(
      OB64.statGateRelocation.constants.OWNER_START,
      OB64.statGateRelocation.constants.OWNER_START +
        OB64.statGateRelocation.constants.OWNER_SIZE
    ).every(byte => byte === 0xFF));
  check('restored in-place table retains exact intended bytes',
    OB64.statGateRelocation.validate(candidate, rom.layout, fittingBytes).ok);

  const removeFinal = OB64.sourceRedirect.prepare(candidate, [], {
    knownCurrentRequests: [scenarioRequest],
  });
  const finalResult = OB64.sourceRedirect.apply(candidate, removeFinal);
  if (finalResult.crc) OB64.recalcN64CRC(candidate);
  const finalState = OB64.sourceRedirect.classify(candidate);
  check('removing the last shared entry restores retail hook and zero cave',
    finalState.ok && finalState.state === 'retail' && verifyHeaderCrc(candidate));
}

function runOwnershipFailures() {
  const rom = loadRom('us-rev0');
  const model = modelFromBytes(rom.statGates, maximumWitness());
  const built = applyStatPlan(rom, model);
  const constants = OB64.statGateRelocation.constants;

  const badDescriptor = built.candidate.slice();
  badDescriptor[constants.DESCRIPTOR_OFFSET + 28] ^= 1;
  const descriptorState = OB64.statGateRelocation.inspect(
    badDescriptor,
    rom.layout
  );
  check('descriptor corruption is classified as foreign ownership',
    descriptorState.state === 'foreign' &&
      descriptorState.reason.includes('CRC32'), descriptorState.reason);
  expectThrow('descriptor corruption blocks planning before writes', () => {
    OB64.statGateRelocation.prepare(model, badDescriptor, rom.layout);
  }, 'ownership is malformed');

  const badTail = rom.z64.slice();
  badTail[constants.OWNER_START + 10] = 0;
  expectThrow('occupied owner without OBSG descriptor blocks export', () => {
    OB64.statGateRelocation.prepare(model, badTail, rom.layout);
  }, 'without a valid OBSG descriptor');

  const badHook = rom.z64.slice();
  badHook[OB64.sourceRedirect.constants.HOOK_ROM] ^= 1;
  expectThrow('foreign shared hook blocks redirect planning', () => {
    OB64.sourceRedirect.prepare(badHook, [], { knownCurrentRequests: [] });
  }, 'ownership is malformed');

  const sameSource = {
    owner: 'scenario', sourceRomOffset: 0x200000,
    destinationRomOffset: 0x210000,
  };
  const duplicate = Object.assign({}, sameSource, { owner: 'stat-gates' });
  const deduplicated = OB64.sourceRedirect.normalizeRequests(
    [sameSource, duplicate],
    rom.z64.length
  );
  check('identical source-destination pairs deduplicate across owners',
    deduplicated.length === 1 && deduplicated[0].owners.length === 2);
  expectThrow('one source with two destinations is rejected', () => {
    OB64.sourceRedirect.normalizeRequests([
      sameSource,
      Object.assign({}, sameSource, { destinationRomOffset: 0x220000 }),
    ], rom.z64.length);
  }, 'two features requested different destinations');

  const installedUnknown = rom.z64.slice();
  const foreignRequest = {
    owner: 'scenario', sourceRomOffset: 0x200000,
    destinationRomOffset: 0x210000,
  };
  const foreignPlan = OB64.sourceRedirect.prepare(
    installedUnknown,
    [foreignRequest],
    { knownCurrentRequests: [] }
  );
  OB64.sourceRedirect.apply(installedUnknown, foreignPlan);
  expectThrow('installed entries without session ownership are rejected', () => {
    OB64.sourceRedirect.prepare(
      installedUnknown,
      [foreignRequest],
      { knownCurrentRequests: [] }
    );
  }, 'cannot safely own');

  const currentModel = rom.statGates;
  const changedPreimage = rom.z64.slice();
  const alternate = modelFromBytes(currentModel, sequentialEdits(currentModel.raw, 1));
  OB64.serializeStatGatesForComparison(alternate, changedPreimage);
  expectThrow('valid but changed retail preimage is rejected by production planning', () => {
    OB64.statGateRelocation.prepare(currentModel, changedPreimage, rom.layout);
  }, 'changed after the ROM was loaded');

  const invalidModel = modelFromBytes(rom.statGates, rom.statGates.raw);
  invalidModel.byClass[1].str = 256;
  expectThrow('non-byte stat model is rejected before compression', () => {
    OB64.statGateRelocation.prepare(invalidModel, rom.z64, rom.layout);
  }, 'whole byte');
}

function runControllerCapacity() {
  const rom = loadRom('us-rev0');
  const maximum = OB64.sourceRedirect.constants.MAX_ENTRIES;
  const requests = [];
  for (let index = 0; index < maximum; index++) {
    requests.push({
      owner: 'capacity-fixture',
      sourceRomOffset: 0x100000 + index * 4,
      destinationRomOffset: 0x200000 + index * 4,
    });
  }
  const candidate = rom.z64.slice();
  const plan = OB64.sourceRedirect.prepare(candidate, requests, {
    knownCurrentRequests: [],
  });
  const result = OB64.sourceRedirect.apply(candidate, plan);
  check('shared redirect accepts its exact 83-entry capacity',
    result.entryCount === maximum &&
      OB64.sourceRedirect.validate(candidate, requests).ok,
    String(result.entryCount));
  expectThrow('shared redirect rejects an 84th entry before writing', () => {
    OB64.sourceRedirect.prepare(rom.z64, requests.concat([{
      owner: 'capacity-fixture',
      sourceRomOffset: 0x300000,
      destinationRomOffset: 0x310000,
    }]), { knownCurrentRequests: [] });
  }, 'verified capacity');
}

function runLegacyScenarioStubRegression() {
  // Frozen from Scenario's accepted relocationStubWords implementation. The
  // shared controller must remain byte-identical to that already-proven stub.
  const expectedHex =
    '3c19800a3739155c8f380000130000090000000014580004000000008f220004' +
    '0802854400000000273900080802853900000000ac82000003e0000800000000';
  const words = OB64.sourceRedirect._test.stubWords();
  const actual = Buffer.alloc(words.length * 4);
  words.forEach((word, index) => actual.writeUInt32BE(word >>> 0, index * 4));

  check('shared redirect preserves the byte-identical proven Scenario stub',
    actual.length === 64 && actual.toString('hex') === expectedHex,
    actual.toString('hex'));
  check('proven Scenario stub has its frozen SHA-256 identity',
    sha256(actual) ===
      'E5973592BDA9C9DC0982A109968869D68CCD5878D413247924D811C2D7610F9A',
    sha256(actual));
}

function main() {
  const summaries = {
    schemaVersion: 1,
    generatedBy: 'tests/stat-gate-relocation.test.js',
    revisions: {},
  };
  runRevisionMatrix('us-rev0', summaries);
  runRevisionMatrix('us-rev1', summaries);
  runCompositionAndRestoration();
  runOwnershipFailures();
  runControllerCapacity();
  runLegacyScenarioStubRegression();

  const fixturePath = path.join(
    __dirname,
    'fixtures',
    'stat-gate-relocation-fixtures.json'
  );
  if (fs.existsSync(fixturePath)) {
    const fixture = JSON.parse(fs.readFileSync(fixturePath, 'utf8'));
    check('revision fixture manifest matches deterministic generated values',
      JSON.stringify(fixture) === JSON.stringify(summaries));
  }

  if (failures) {
    console.error('\n' + failures + ' stat-gate relocation test(s) failed.');
    process.exit(1);
  }
  console.log('\nAll stat-gate relocation tests passed.');
  console.log('FIXTURE_JSON ' + JSON.stringify(summaries));
}

main();
