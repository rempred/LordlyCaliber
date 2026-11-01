'use strict';
// Maintainer-only: qualify link addresses from authenticated accepted proofs.
// Normal builds need only the carried sources and this generated manifest.
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const {readAcceptedEvidence, digest} = require('./decomp-evidence.cjs');
function generate(root, native) {
  const evidence = readAcceptedEvidence(root);
  const inventoryBytes = fs.readFileSync(path.join(root, 'docs/Plans/attack-effects-program-draft/inventory.json'));
  const inventory = JSON.parse(inventoryBytes);
  const supplemental = ['000458f0', '00045934', '00045a50', '00050918', '000509f4',
    '00042dc4', '00042e64', '00042f68', '00043000', '0021D7F0', '0021DCA4', '0021E50C', '002213DC'];
  const selected = new Set([...inventory.targets.map(t => t.symbol.toLowerCase()),
    ...supplemental.map(id => 'func_' + id.toLowerCase())]);
  const imports = JSON.parse(fs.readFileSync(path.join(native, 'game/accepted/library-imports.json')));
  assert.equal(imports.fingerprint, evidence.state.fingerprint, 'Import the accepted library first');
  const owners = [];
  for (const t of evidence.targets.filter(t => t.sourceClass === 'PURE_C' && selected.has(t.symbol.toLowerCase()))) {
    const descriptor = t.provenance.proof;
    const bytes = fs.readFileSync(path.join(evidence.state.output, descriptor.path));
    assert.equal(digest(bytes), descriptor.sha256);
    const proof = JSON.parse(bytes);
    assert.ok(['section-assigned', 'native-text-tail'].includes(proof.textContract.mode), t.symbol);
    const address = t.owners[0].vramStart, start = t.owners[0].romStart;
    let size = 0;
    for (const owner of t.owners) {
      assert.equal(owner.vramStart, address + size); assert.equal(owner.romStart, start + size);
      size += owner.bytes;
    }
    if (t.functions.length > 1) {
      // A recovered local helper can cross an old assembly-owner boundary.
      // Compile the complete producer once, keeping each physical part's hash.
      // Never split a compiler function at the old continuation label.
      let extent = 0;
      for (const [index, f] of t.functions.entries()) {
        assert.equal(f.offset, extent, 'Gap or overlap in complete producer: ' + t.symbol);
        assert.ok(Number.isSafeInteger(f.bytes) && f.bytes > 0 && !(f.bytes & 3));
        assert.equal(f.binding, index ? 0 : 1, 'Only the entry may be global: ' + t.symbol);
        assert.equal(f.symbolType, 2); assert.equal(f.visibility, 0);
        extent += f.bytes;
      }
      assert.equal(t.functions[0].symbol, t.symbol);
      assert.equal(extent, size, 'Incomplete compiler coverage: ' + t.symbol);
    }
    const bindings = {}, pending = new Map();
    function bind(name, value) {
      value >>>= 0;
      if (name === '.text') { assert.equal(value, address); return; }
      if (name.startsWith('.')) { assert.equal(name, '.rodata'); return; }
      assert.match(name, /^[A-Za-z_][A-Za-z0-9_]*$/);
      if (name in bindings) assert.equal(bindings[name], value, t.symbol + ': ' + name);
      bindings[name] = value;
    }
    const signed = n => (n << 16) >> 16;
    for (const r of proof.linkEvidence.relocations) {
      assert.equal(r.linkedWordExact, true); assert.equal(r.linkedWord, r.expectedWord);
      if (r.type === 'R_MIPS_26')
        bind(r.symbol, ((r.linkedWord & 0x3ffffff) - (r.rawWord & 0x3ffffff)) * 4 + (address & 0xf0000000));
      else if (r.type === 'R_MIPS_HI16') {
        if (!pending.has(r.symbol)) pending.set(r.symbol, []);
        pending.get(r.symbol).push(r);
      } else {
        assert.equal(r.type, 'R_MIPS_LO16');
        const highs = pending.get(r.symbol); assert.ok(highs?.length);
        for (const h of highs) bind(r.symbol,
          ((h.linkedWord & 65535) - (h.rawWord & 65535)) * 65536 + signed(r.linkedWord) - signed(r.rawWord));
        pending.delete(r.symbol);
      }
    }
    assert.equal(pending.size, 0);
    const source = 'game/accepted/library/' + t.source;
    assert.equal(digest(fs.readFileSync(path.join(native, source))), t.sourceSha256);
    const headers = t.dependencies.map(d => {
      assert.ok(d.path.startsWith('include/'));
      const file = 'game/accepted/library/' + d.path;
      assert.equal(digest(fs.readFileSync(path.join(native, file))), d.sha256);
      return {name: d.path.slice(8), file, sha256: d.sha256};
    });
    // Auxiliary table information lives in the hash-bound proof's finalTarget.
    const tables = proof.finalTarget.auxiliarySections || [];
    owners.push({symbol: t.symbol, source, sourceSha256: t.sourceSha256, address, start, size,
      parts: t.owners.map(o => ({offset: o.romStart - start, size: o.bytes, sha256: o.linkedSha256})),
      ...(t.functions.length > 1 ? {functions: t.functions} : {}),
      bindings, headers, tables});
  }
  // The historical inventory includes original owner suffixes. Current accepted
  // ownership defines the C extent; retain the suffix in the loaded ROM. A former
  // entry that is now a continuation must be fully covered by its coupled producer.
  owners.sort((a,b) => a.start - b.start);
  for (let i = 1; i < owners.length; ++i)
    assert.ok(owners[i-1].start + owners[i-1].size <= owners[i].start, 'Overlapping compiled owners');
  let retainedSuffixBytes = 0;
  for (const target of inventory.targets) {
    const start = Number(target.romStart), end = Number(target.romEndExclusive), address = Number(target.ramStart);
    assert.equal(end - start, target.bytes);
    const named = owners.find(o => o.symbol.toLowerCase() === target.symbol.toLowerCase());
    if (named) { assert.equal(named.start, start); assert.equal(named.address, address); }
    const acceptedEnd = named ? Math.min(end, named.start + named.size) : end;
    retainedSuffixBytes += end - acceptedEnd;
    let cursor = start;
    for (const owner of owners) {
      if (owner.start > cursor || owner.start + owner.size <= cursor) continue;
      assert.equal(owner.address + cursor - owner.start, address + cursor - start, target.symbol + ': RAM mapping');
      cursor = Math.min(acceptedEnd, owner.start + owner.size);
      if (cursor === acceptedEnd) break;
    }
    assert.equal(cursor, acceptedEnd, 'Missing accepted C coverage: ' + target.symbol);
  }
  evidence.assertStable();
  return {schemaVersion: 1, generator: 'native/tools/generate-effect-owners.cjs',
    fingerprint: evidence.state.fingerprint, revision: imports.revision,
    scope: {inventorySha256: digest(inventoryBytes), targets: inventory.targets.length,
      inventoryBytes: inventory.targets.reduce((sum, t) => sum + t.bytes, 0), retainedSuffixBytes,
      programCallbacks: inventory.counts.programCallbacks, effectSlots: inventory.counts.attackEffectSlots},
    purpose: 'Compile accepted C for isolated previews; retain modified ROM owners and unconverted code.',
    owners};
}
if (require.main === module) {
  const root = path.resolve(process.argv[2]), native = path.resolve(__dirname, '..');
  const result = generate(root, native);
  fs.writeFileSync(path.join(native, 'game/accepted/effect-owners.json'), JSON.stringify(result, null, 2) + '\n');
  console.log('Qualified ' + result.owners.length + ' accepted C owners');
}
module.exports = {generate};
