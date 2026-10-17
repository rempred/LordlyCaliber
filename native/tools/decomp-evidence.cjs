'use strict';
// Read accepted evidence only. Never runs upstream tools or changes their output.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
const canonical = value => Array.isArray(value) ? value.map(canonical) :
  value && typeof value === 'object' ? Object.fromEntries(Object.keys(value).sort().map(k => [k, canonical(value[k])])) : value;
const same = (a, b) => JSON.stringify(canonical(a)) === JSON.stringify(canonical(b));
const hashObject = value => digest(Buffer.from(JSON.stringify(canonical(value))));
const isHash = value => typeof value === 'string' && /^[A-F0-9]{64}$/.test(value);
const nonempty = value => Array.isArray(value) && value.length > 0;
function requireEvidence(condition, message) {
  if (!condition) throw Error(message);
}
function sourcePath(relative) {
  requireEvidence(typeof relative === 'string' &&
    /^(src|include|third_party\/lha\/ob64)\/[A-Za-z0-9_./-]+\.(c|h)$/.test(relative) &&
    !relative.split('/').some(part => part === '..' || part === '.' || !part),
  'Unsupported source dependency: ' + relative);
  return relative;
}
function confinedFile(root, relative) {
  const base = fs.realpathSync(root), file = path.join(base, relative);
  const resolved = path.relative(base, fs.realpathSync(file));
  requireEvidence(resolved && !path.isAbsolute(resolved) && !resolved.split(path.sep).includes('..'),
    'Evidence path escapes its root: ' + relative);
  return fs.readFileSync(file);
}
function indexed(records) {
  requireEvidence(nonempty(records), 'Missing accepted target census');
  const result = new Map();
  for (const record of records) {
    requireEvidence(typeof record.symbol === 'string' && /^[A-Za-z_][A-Za-z0-9_]*$/.test(record.symbol) &&
      !result.has(record.symbol), 'Missing or duplicate accepted target: ' + record.symbol);
    result.set(record.symbol, record);
  }
  return result;
}
function exactOwners(owners, symbol) {
  requireEvidence(nonempty(owners), 'Missing physical owners: ' + symbol);
  for (const owner of owners) {
    requireEvidence(Number.isSafeInteger(owner.romStart) && owner.romStart >= 0 &&
      Number.isSafeInteger(owner.bytes) && owner.bytes > 0 && owner.romEndExclusive === owner.romStart + owner.bytes &&
      owner.rawBytesExact === true && isHash(owner.linkedSha256) && owner.linkedSha256 === owner.expectedSha256,
    'Missing exact physical owner: ' + symbol);
  }
}
function readAcceptedEvidence(root) {
  const reportPaths = ['state', 'verification', 'fresh-compilation'].map(name => path.join(root, 'build/current', name + '.json'));
  const raw = reportPaths.map(file => fs.readFileSync(file));
  const [state, verification, fresh] = raw.map(bytes => JSON.parse(bytes));
  const version = state.schemaVersion;
  requireEvidence([5, 6].includes(version) && verification.schemaVersion === version && fresh.schemaVersion === version &&
    state.verifiedAt && isHash(state.fingerprint) && digest(raw[1]) === state.verificationSha256 &&
    digest(raw[2]) === state.freshCompilationSha256 && verification.status === 'pass' &&
    verification.verification?.status === 'pass' && fresh.status === 'pass',
  'Missing, changed or unsupported accepted Decomp evidence');
  const linked = indexed(verification.verification.targets), compiled = indexed(fresh.targets);
  requireEvidence(linked.size === compiled.size, 'Fresh/linked target census differs');
  if (version === 6) {
    requireEvidence(verification.verification.schemaVersion === 6 &&
      same(state.buildReport, verification.buildReport) && same(state.buildReport, fresh.buildReport) &&
      isHash(state.buildReport?.sha256) && state.buildReport.bytes > 0 &&
      typeof state.output === 'string' && path.isAbsolute(state.output) &&
      isHash(state.rom?.sha256) && state.rom.sha256 === verification.verification.outputs?.rom?.sha256 &&
      state.rom.bytes === verification.verification.outputs.rom.bytes,
    'Acceptance reports refer to different builds');
  }
  const targets = [];
  for (const target of compiled.values()) {
    const match = linked.get(target.symbol);
    requireEvidence(match?.source === target.source && ['PURE_C', 'HYBRID_C'].includes(target.sourceClass) &&
      isHash(target.sourceSha256), 'Missing accepted source binding: ' + target.symbol);
    sourcePath(target.source);
    requireEvidence(Array.isArray(target.dependencies), 'Missing dependency census: ' + target.symbol);
    for (const dependency of target.dependencies) {
      sourcePath(dependency.path);
      requireEvidence(isHash(dependency.sha256), 'Missing dependency identity: ' + dependency.path);
    }
    let functions, owners, provenance;
    if (version === 5) {
      requireEvidence(match.linkEvidence?.fullOwnerExact === true, 'Missing exact linked owner: ' + target.symbol);
      functions = target.textContract?.compilerTextFunctions;
      owners = target.textContract?.owners;
    } else {
      requireEvidence(match.rawBytesExact === true && match.differingByteCount === 0 &&
        isHash(match.linkedTargetSha256) && match.linkedTargetSha256 === match.expectedTargetSha256,
      'Missing exact linked target: ' + target.symbol);
      exactOwners(match.owners, target.symbol);
      const descriptor = target.sourceObjectProof, binding = match.sourceObjectEvidence;
      requireEvidence(descriptor?.path === `generated/c/${target.symbol}.source-object-proof.json` &&
        Number.isSafeInteger(descriptor.bytes) && descriptor.bytes > 0 && isHash(descriptor.sha256) &&
        same(descriptor, binding?.proof), 'Missing or inconsistent source-object proof: ' + target.symbol);
      // v6 moves representation contracts out of the compact reports. Read only
      // their explicitly named, hash-bound proof files; never scan the build tree.
      const bytes = confinedFile(state.output, descriptor.path);
      requireEvidence(bytes.length === descriptor.bytes && digest(bytes) === descriptor.sha256,
        'Changed source-object proof: ' + target.symbol);
      const proof = JSON.parse(bytes), identity = proof.target;
      requireEvidence(proof.schemaVersion === 4 && proof.kind === 'ob64-source-to-object-load-evidence' &&
        identity?.symbol === target.symbol && identity.source === target.source && identity.sourceSha256 === target.sourceSha256 &&
        identity.sourceClass === target.sourceClass && identity.sourceClass === binding.sourceClass &&
        identity.sourcePolicyDigest === target.sourcePolicyDigest && identity.sourcePolicyDigest === binding.sourcePolicyDigest &&
        isHash(identity.sourcePolicyDigest) && identity.sectionName === match.sectionName &&
        same(identity.ownerSections, match.owners.map(owner => owner.sectionName)) &&
        same(identity.dependencies, target.dependencies) && same(identity.dependencies, binding.dependencies) &&
        same(identity.compilationInput, binding.compilationInput) &&
        same(target.compilationInput, {path: target.source, ...identity.compilationInput}) &&
        proof.linkEvidence?.fullOwnerExact === true && proof.assemblyContract?.compilerAssemblyRewritten === false &&
        target.compilerAssemblyRewritten === false && binding.compilerAssemblyRewritten === false,
      'Source-object proof binding differs: ' + target.symbol);
      requireEvidence(hashObject({textContract: proof.textContract, objectEvidence: proof.objectEvidence,
        linkEvidence: proof.linkEvidence}) === match.representationSha256 &&
        hashObject({textContract: proof.textContract, objectEvidence: proof.objectEvidence}) === target.freshRepresentationSha256,
      'Representation identity differs: ' + target.symbol);
      functions = proof.textContract?.compilerTextFunctions;
      owners = match.owners;
      const physicalIdentity = owner => ({sectionName: owner.sectionName, bytes: owner.bytes,
        linkedSha256: owner.linkedSha256, expectedSha256: owner.expectedSha256, rawBytesExact: owner.rawBytesExact});
      requireEvidence(same(proof.finalTarget?.compilerTextFunctions, match.compilerTextFunctions) &&
        same(functions?.map(f => f.symbol), match.compilerTextFunctions?.map(f => f.symbol)) &&
        same(proof.finalTarget?.textOwners, owners.map(physicalIdentity)),
      'Compiler function/owner census differs: ' + target.symbol);
      const compiler = proof.toolchain?.compiler;
      requireEvidence(isHash(compiler?.executableSha256) && compiler.executableSha256 === fresh.compilerSha256 &&
        isHash(compiler.manifestSha256) && nonempty(compiler.flags) &&
        same(proof.toolchain.assembler, fresh.toolchain), 'Missing compiler identity: ' + target.symbol);
      const producer = proof.textContract.producer;
      provenance = {proof: descriptor, compiler, textMode: proof.textContract.mode,
        ...(producer ? {producer: {id: producer.id, mode: producer.mode, source: producer.source, members: producer.members}} : {})};
    }
    requireEvidence(nonempty(functions) && functions.every(f => typeof f.symbol === 'string') && nonempty(owners),
      'Missing compiler function/owner census: ' + target.symbol);
    targets.push({...target, functions, owners, provenance});
  }
  const bySymbol = new Map(targets.map(t => [t.symbol, t]));
  for (const target of targets) {
    const producer = target.provenance?.producer;
    if (!producer) continue;
    requireEvidence(producer.source === target.source && nonempty(producer.members) &&
      producer.members.some(m => m.symbol === target.symbol) && producer.members.every(member => {
        const companion = bySymbol.get(member.symbol);
        return companion?.source === target.source && companion.sourceSha256 === target.sourceSha256 &&
          companion.sourceClass === target.sourceClass && same(companion.dependencies, target.dependencies) &&
          same(companion.provenance?.producer, producer);
      }), 'Incomplete or inconsistent complete producer: ' + target.source);
  }
  const buffers = new Map();
  function readSource(relative, expected) {
    sourcePath(relative);
    requireEvidence(isHash(expected), 'Missing source identity: ' + relative);
    if (!buffers.has(relative)) buffers.set(relative, confinedFile(root, relative));
    const bytes = buffers.get(relative);
    requireEvidence(digest(bytes) === expected, 'Checkout differs from accepted source: ' + relative);
    return bytes;
  }
  function assertStable() {
    for (let i = 0; i < raw.length; i++)
      requireEvidence(fs.readFileSync(reportPaths[i]).equals(raw[i]), 'Decomp verification changed during read');
  }
  assertStable();
  return {state, targets, readSource, assertStable};
}
module.exports = {readAcceptedEvidence, sourcePath, digest, same};
