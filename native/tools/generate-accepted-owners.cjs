'use strict';
// Qualify the complete accepted C library for a source-built ROM baseline.
// Upstream is read-only; only its named, hash-bound proofs supply placement.
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const {readAcceptedEvidence, digest} = require('./decomp-evidence.cjs');
function generate(root, native) {
  const evidence = readAcceptedEvidence(root);
  const imports = JSON.parse(fs.readFileSync(path.join(native, 'game/accepted/library-imports.json')));
  assert.equal(imports.fingerprint, evidence.state.fingerprint, 'Import accepted sources first');
  const groups = new Map();
  for (const t of evidence.targets.filter(t => ['PURE_C', 'HYBRID_C'].includes(t.sourceClass))) {
    const descriptor = t.provenance.proof;
    const bytes = fs.readFileSync(path.join(evidence.state.output, descriptor.path));
    assert.equal(digest(bytes), descriptor.sha256);
    const proof = JSON.parse(bytes), producer = proof.textContract.producer;
    const base = producer ? producer.vramStart : t.owners[0].vramStart;
    let group = groups.get(t.source);
    if (!group) {
      const source = 'game/accepted/library/' + t.source;
      assert.equal(digest(fs.readFileSync(path.join(native, source))), t.sourceSha256);
      const headers = t.dependencies.map(d => {
        const file = 'game/accepted/library/' + d.path;
        assert.equal(digest(fs.readFileSync(path.join(native, file))), d.sha256);
        return {path: d.path, file, sha256: d.sha256};
      });
      group = {name: t.symbol, source, sourcePath: t.source, sourceSha256: t.sourceSha256, headers,
        address: base, bindings: {}, sections: {'.text': base}, projections: [], parts: [], targets: []};
      groups.set(t.source, group);
    }
    assert.equal(group.address, base, 'Non-affine producer text: ' + t.source);
    group.targets.push(t.symbol);
    const bind = (name, value) => {
      value >>>= 0;
      if (!name) {
        assert.ok(proof.textContract.retainedLiteralBinding, 'Unidentified relocation: ' + t.symbol);
        assert.equal(value, proof.textContract.retainedLiteralBinding.base, t.symbol + ': retained literal base');
        return;
      }
      if (name.startsWith('.')) {
        assert.ok(['.text', '.rodata'].includes(name), 'Unknown source section: ' + name);
        if (name in group.sections) assert.equal(group.sections[name], value, t.symbol + ': ' + name);
        group.sections[name] = value;
      } else {
        assert.match(name, /^[A-Za-z_][A-Za-z0-9_]*$/);
        if (name in group.bindings) assert.equal(group.bindings[name], value, t.symbol + ': ' + name);
        group.bindings[name] = value;
      }
    };
    for (const o of t.owners) group.parts.push({section: '.text', offset: o.vramStart - base,
      start: o.romStart, address: o.vramStart, size: o.bytes, sha256: o.linkedSha256, code: 1});
    const literal = proof.textContract.retainedLiteralBinding;
    if (literal) {
      bind(literal.contract.compilerSection, literal.base);
      group.parts.push({section: literal.contract.compilerSection, offset: 0,
        start: literal.contract.romStart, address: literal.base, size: literal.contract.bytes,
        sha256: literal.contract.expectedPayloadSha256, code: 0});
    }
    const mixed = proof.objectEvidence.mixedReadonly?.binding;
    if (mixed) bind('.rodata', mixed.base);
    const projection = proof.objectEvidence.auxiliaryProjection;
    for (const pair of projection?.occurrence?.pairs || []) {
      if (!pair.anchorDelta) continue;
      for (const [offset, info] of [[pair.highPlace, pair.highInfo], [pair.lowPlace, pair.lowInfo]])
        group.projections.push({offset, info, value: pair.effectiveAddress - pair.addend});
    }
    // A grouped proof describes projected owners. Use its original compiler
    // relocation addends when recovering the unsplit object's link bindings.
    const pending = new Map(), signed = n => (n << 16) >> 16;
    for (const linked of proof.linkEvidence.relocations) {
      assert.equal(linked.linkedWordExact, true); assert.equal(linked.linkedWord, linked.expectedWord);
      const raw = producer?.relocations.find(r => r.offset === linked.groupOffset);
      if (producer) assert.ok(raw, 'Missing original producer relocation: ' + t.symbol);
      const r = {...linked, symbol: raw ? raw.symbol : linked.symbol, rawWord: raw ? raw.word : linked.rawWord};
      if (r.type === 'R_MIPS_26')
        bind(r.symbol, ((r.linkedWord & 0x3ffffff) - (r.rawWord & 0x3ffffff)) * 4 + (base & 0xf0000000));
      else if (r.type === 'R_MIPS_HI16') {
        if (!pending.has(r.symbol)) pending.set(r.symbol, []);
        pending.get(r.symbol).push(r);
      } else {
        assert.equal(r.type, 'R_MIPS_LO16');
        const highs = pending.get(r.symbol);
        if (!highs?.length) {
          // Some literal loads reuse an earlier high half. The accepted literal
          // contract or another complete relocation must establish the section.
          const name = r.symbol || '.rodata';
          assert.equal((r.linkedWord - r.rawWord) & 65535, group.sections[name] & 65535,
            'Unbound low-half relocation: ' + t.symbol);
          assert.ok(name in group.sections);
        } else {
          for (const h of highs) bind(r.symbol,
            ((h.linkedWord & 65535) - (h.rawWord & 65535)) * 65536 + signed(r.linkedWord) - signed(r.rawWord));
          pending.delete(r.symbol);
        }
      }
    }
    assert.equal(pending.size, 0);
    for (const table of proof.finalTarget.auxiliarySections || []) {
      const section = table.compilerSection;
      if (!(section in group.sections)) bind(section, Number(table.vramStart));
      const segment = projection?.contract?.segments.find(s => s.outputSection === table.outputSection);
      group.parts.push({section, offset: segment ? segment.offset : Number(table.vramStart) - group.sections[section],
        start: Number(table.romStart), address: Number(table.vramStart), size: table.bytes,
        sha256: table.expectedLinkedSha256, code: 0});
    }
  }
  const sources = [...groups.values()].sort((a,b) => a.source.localeCompare(b.source));
  for (const group of sources) {
    const unique = new Map();
    for (const part of group.parts) {
      const key = part.start + ':' + part.size;
      if (unique.has(key)) assert.deepEqual(unique.get(key), part);
      unique.set(key, part);
    }
    group.parts = [...unique.values()].sort((a,b) => a.start - b.start);
    group.targets.sort();
  }
  assert.equal(sources.length, imports.counts.producerSources);
  assert.equal(sources.reduce((sum,g) => sum + g.targets.length, 0), imports.counts.owners);
  evidence.assertStable();
  return {schemaVersion: 1, generator: 'native/tools/generate-accepted-owners.cjs',
    fingerprint: evidence.state.fingerprint, revision: imports.revision, counts: imports.counts, sources};
}
if (require.main === module) {
  const native = path.resolve(__dirname, '..');
  const result = generate(path.resolve(process.argv[2]), native);
  fs.writeFileSync(path.join(native, 'game/accepted/rom-owners.json'), JSON.stringify(result, null, 2) + '\n');
  console.log('Qualified ' + result.sources.length + ' complete accepted C sources');
}
module.exports = {generate};
