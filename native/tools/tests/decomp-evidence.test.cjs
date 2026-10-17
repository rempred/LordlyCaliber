'use strict';
// Synthetic, hash-bound evidence exercises intake failure paths without altering
// Decomp. This tests authentication/import behavior, not upstream C matching.
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {readAcceptedEvidence, digest} = require('../decomp-evidence.cjs');
const {importAccepted} = require('../import-accepted-c.cjs');
const local = path.resolve(__dirname, '../../.local');
const canonical = v => Array.isArray(v) ? v.map(canonical) : v && typeof v === 'object' ?
  Object.fromEntries(Object.keys(v).sort().map(k => [k, canonical(v[k])])) : v;
const hashObject = v => digest(Buffer.from(JSON.stringify(canonical(v))));
const hash = digest(Buffer.from('fixture'));
function fixture(t, version = 6) {
  fs.mkdirSync(local, {recursive: true});
  const root = fs.mkdtempSync(path.join(local, 'decomp-intake-test-'));
  t.after(() => {
    // This exact, newly-created fixture is the only recursive removal target.
    assert.equal(path.dirname(fs.realpathSync(root)), fs.realpathSync(local));
    assert.ok(path.basename(root).startsWith('decomp-intake-test-'));
    fs.rmSync(root, {recursive: true});
  });
  const destination = path.join(root, 'imported'), output = path.join(root, 'proofs');
  function write(relative, bytes) {
    const file = path.join(root, relative);
    fs.mkdirSync(path.dirname(file), {recursive: true});
    fs.writeFileSync(file, bytes);
  }
  const source = Buffer.from('#include "pair.h"\nint alpha(void) { return 1; }\nint beta(void) { return 2; }\n');
  const header = Buffer.from('/* retained header bytes */\r\n');
  write('src/lib/pair.c', source); write('include/pair.h', header);
  const dependencies = [{path: 'include/pair.h', sha256: digest(header)}];
  const producer = {id: 'paired', source: 'src/lib/pair.c', mode: 'native-text-owner-projection',
    members: [{symbol: 'alpha', functionSymbols: ['alpha', 'interior']}, {symbol: 'beta', functionSymbols: ['beta']}]};
  const compiler = {executableSha256: hash, manifestSha256: hash, flags: ['-O2', '-funsigned-char']};
  const buildReport = {bytes: 100, sha256: hash};
  const state = {schemaVersion: version, fingerprint: hash, verifiedAt: '2026-10-17T00:00:00Z',
    output, buildReport, rom: {bytes: 40, sha256: hash}};
  const verification = {schemaVersion: version, status: 'pass', buildReport,
    verification: {schemaVersion: version, status: 'pass', outputs: {rom: state.rom}, targets: []}};
  const fresh = {schemaVersion: version, status: 'pass', buildReport, compilerSha256: hash, toolchain: {id: 'fixture'}, targets: []};
  const proofs = [];
  for (const [index, symbol] of ['alpha', 'beta'].entries()) {
    const functions = (index ? ['beta'] : ['alpha', 'interior']).map((symbol, i) => ({symbol, offset: i * 4, bytes: 4}));
    // Alpha spans two physical sections; its complete C producer also owns beta.
    const owners = functions.map((_, i) => ({sectionName: `.part${index}_${i}`, romStart: 16 * index + 4 * i,
      romEndExclusive: 16 * index + 4 * i + 4, bytes: 4, linkedSha256: hash, expectedSha256: hash, rawBytesExact: true}));
    const target = {symbol, source: 'src/lib/pair.c', sourceSha256: digest(source), sourceClass: 'PURE_C',
      sourcePolicyDigest: hash, dependencies: structuredClone(dependencies), compilerAssemblyRewritten: false,
      compilationInput: {path: 'src/lib/pair.c', bytes: source.length, sha256: digest(source)}};
    const linkedFunctions = functions.map(f => ({...f, offset: '0x' + f.offset.toString(16)}));
    const match = {symbol, source: target.source, sectionName: owners[0].sectionName, owners,
      compilerTextFunctions: linkedFunctions, rawBytesExact: true, differingByteCount: 0,
      linkedTargetSha256: hash, expectedTargetSha256: hash,
      sourceObjectEvidence: {symbol, sourceClass: target.sourceClass, sourcePolicyDigest: hash,
        dependencies: structuredClone(dependencies), compilerAssemblyRewritten: false,
        compilationInput: {bytes: source.length, sha256: digest(source)}}};
    const proof = {schemaVersion: 4, kind: 'ob64-source-to-object-load-evidence',
      target: {...target, compilationInput: match.sourceObjectEvidence.compilationInput,
        sectionName: match.sectionName, ownerSections: owners.map(o => o.sectionName)},
      textContract: {mode: 'native', owners, compilerTextFunctions: functions, producer}, objectEvidence: {},
      linkEvidence: {fullOwnerExact: true}, assemblyContract: {compilerAssemblyRewritten: false},
      toolchain: {compiler, assembler: fresh.toolchain}, finalTarget: {compilerTextFunctions: linkedFunctions,
        textOwners: owners.map(({romStart, romEndExclusive, ...o}) => o)}};
    fresh.targets.push(target); verification.verification.targets.push(match); proofs.push(proof);
  }
  function publish() {
    for (let i = 0; i < proofs.length; i++) {
      const target = fresh.targets.find(t => t.symbol === proofs[i].target.symbol);
      const match = verification.verification.targets.find(t => t.symbol === proofs[i].target.symbol);
      if (!target || !match) continue;
      if (version === 5) {
        target.textContract = proofs[i].textContract;
        match.linkEvidence = proofs[i].linkEvidence;
      } else {
        const bytes = Buffer.from(JSON.stringify(proofs[i]));
        const descriptor = {path: `generated/c/${target.symbol}.source-object-proof.json`, bytes: bytes.length, sha256: digest(bytes)};
        write('proofs/' + descriptor.path, bytes);
        target.sourceObjectProof = descriptor; match.sourceObjectEvidence.proof = structuredClone(descriptor);
        match.representationSha256 = hashObject({textContract: proofs[i].textContract,
          objectEvidence: proofs[i].objectEvidence, linkEvidence: proofs[i].linkEvidence});
        target.freshRepresentationSha256 = hashObject({textContract: proofs[i].textContract, objectEvidence: proofs[i].objectEvidence});
      }
    }
    reports();
  }
  function reports() {
    const v = Buffer.from(JSON.stringify(verification)), f = Buffer.from(JSON.stringify(fresh));
    state.verificationSha256 = digest(v); state.freshCompilationSha256 = digest(f);
    write('build/current/verification.json', v); write('build/current/fresh-compilation.json', f);
    write('build/current/state.json', JSON.stringify(state));
  }
  publish();
  return {root, destination, state, verification, fresh, proofs, source, header, write, publish, reports};
}
for (const version of [5, 6]) test(`schema ${version}: complete producer, interior function and exact source bytes import`, t => {
  const f = fixture(t, version), manifest = importAccepted(f.root, f.destination, true);
  assert.deepEqual(manifest.counts, {owners: 2, functions: 3, producerSources: 1, sourceAndHeaderFiles: 2});
  assert.deepEqual(fs.readFileSync(path.join(f.destination, 'game/accepted/library/src/lib/pair.c')), f.source);
  assert.deepEqual(fs.readFileSync(path.join(f.destination, 'game/accepted/library/include/pair.h')), f.header);
  assert.deepEqual(manifest.imports[0].symbols, ['alpha', 'beta', 'interior']);
  if (version === 6) {
    assert.equal(manifest.imports[0].producer.members.length, 2);
    assert.equal(manifest.imports[0].owners[0].physicalOwners.length, 2);
    assert.deepEqual(manifest.imports[0].compiler.flags, ['-O2', '-funsigned-char']);
  }
});
test('dry run does not create a destination', t => {
  const f = fixture(t); importAccepted(f.root, f.destination);
  assert.equal(fs.existsSync(f.destination), false);
});
const failures = [
  ['unsupported schema', f => { f.state.schemaVersion = 7; f.reports(); }, /unsupported/],
  ['changed report', f => f.write('build/current/verification.json', '{}'), /unsupported/],
  ['different build identity', f => { f.fresh.buildReport = {bytes: 9, sha256: hash}; f.reports(); }, /different builds/],
  ['missing linked target', f => { f.verification.verification.targets.pop(); f.reports(); }, /census differs/],
  ['duplicate target', f => { f.fresh.targets.push(f.fresh.targets[0]); f.reports(); }, /duplicate/],
  ['changed source', f => f.write('src/lib/pair.c', 'unfinished work'), /Checkout differs/],
  ['changed included header', f => f.write('include/pair.h', 'unfinished work'), /Checkout differs/],
  ['unsafe dependency', f => { f.fresh.targets[0].dependencies[0].path = '../outside.h'; f.reports(); }, /Unsupported source/],
  ['nonexact physical owner', f => { f.verification.verification.targets[0].owners[1].rawBytesExact = false; f.reports(); }, /exact physical owner/],
  ['nonexact target', f => { f.verification.verification.targets[0].differingByteCount = 4; f.reports(); }, /exact linked target/],
  ['changed proof file', f => f.write('proofs/generated/c/alpha.source-object-proof.json', '{}'), /Changed source-object proof/],
  ['unbound proof', f => { f.fresh.targets[0].sourceObjectProof.sha256 = hash; f.reports(); }, /inconsistent source-object proof/],
  ['proof from another source', f => { f.proofs[0].target.sourceSha256 = hash; f.publish(); }, /proof binding differs/],
  ['unmatched representation', f => { f.fresh.targets[0].freshRepresentationSha256 = hash; f.reports(); }, /Representation identity/],
  ['missing function census', f => { f.proofs[0].textContract.compilerTextFunctions = []; f.publish(); }, /function\/owner census/],
  ['missing complete-producer companion', f => {
    f.fresh.targets.pop(); f.verification.verification.targets.pop(); f.reports();
  }, /complete producer/],
  ['inconsistent producer compiler flags', f => {
    f.proofs[1].toolchain.compiler = {...f.proofs[1].toolchain.compiler, flags: ['-O0']}; f.publish();
  }, /Inconsistent producer identity/],
  ['unknown source class', f => { f.fresh.targets[0].sourceClass = 'UNKNOWN'; f.reports(); }, /source binding/]
];
for (const [name, change, message] of failures) test(`reject ${name} before writing`, t => {
  const f = fixture(t); change(f);
  assert.throws(() => importAccepted(f.root, f.destination, true), message);
  assert.equal(fs.existsSync(f.destination), false);
});
test('an acceptance change after read is detected', t => {
  const f = fixture(t), snapshot = readAcceptedEvidence(f.root);
  f.state.verifiedAt = '2026-10-18T00:00:00Z'; f.reports();
  assert.throws(() => snapshot.assertStable(), /changed during read/);
});
for (const modified of [false, true]) test(`retired source refresh ${modified ? 'preserves local edits' : 'removes only prior imports'}`, t => {
  const f = fixture(t), manifest = importAccepted(f.root, f.destination, true);
  const library = path.join(f.destination, 'game/accepted/library/src/lib');
  const retired = Buffer.from('/* previous accepted producer */');
  fs.writeFileSync(path.join(library, 'retired.c'), modified ? 'local work' : retired);
  fs.writeFileSync(path.join(library, 'unrelated.c'), 'unrelated work');
  manifest.imports.push({source: 'src/lib/retired.c', sourceSha256: digest(retired),
    file: 'game/accepted/library/src/lib/retired.c', symbols: ['retired'], dependencies: []});
  const manifestPath = path.join(f.destination, 'game/accepted/library-imports.json');
  fs.writeFileSync(manifestPath, JSON.stringify(manifest));
  const before = fs.readFileSync(manifestPath);
  if (modified) {
    assert.throws(() => importAccepted(f.root, f.destination, true), /local edits/);
    assert.deepEqual(fs.readFileSync(manifestPath), before);
    assert.equal(fs.readFileSync(path.join(library, 'retired.c'), 'utf8'), 'local work');
  } else {
    importAccepted(f.root, f.destination); // Dry runs preserve retired files, too.
    assert.ok(fs.existsSync(path.join(library, 'retired.c')));
    importAccepted(f.root, f.destination, true);
    assert.equal(fs.existsSync(path.join(library, 'retired.c')), false);
  }
  assert.equal(fs.readFileSync(path.join(library, 'unrelated.c'), 'utf8'), 'unrelated work');
});
