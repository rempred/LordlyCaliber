// Read-only join of editor interests with Decomp's existing matching evidence.
// Does not invoke Decomp tooling, compile, import sources or start a watcher.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const native = path.resolve(__dirname, '..');
const key = value => value.toLowerCase();
const read = file => JSON.parse(fs.readFileSync(file, 'utf8'));
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex').toUpperCase();

function main(args) {
  let decomp, query = '', ready = false, json = false;
  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg === '--decomp' || arg === '--match') {
      if (!args[i + 1] || args[i + 1].startsWith('--')) throw Error(`Missing value for ${arg}`);
      if (arg === '--decomp') decomp = path.resolve(args[++i]);
      else query = key(args[++i]).replaceAll('\\', '/');
    } else if (arg === '--ready') ready = true;
    else if (arg === '--json') json = true;
    else if (arg === '--help') {
      console.log('node native/tools/decomp-interest.cjs [--decomp <checkout>] [--match <symbol-or-file>] [--ready] [--json]');
      return;
    } else throw Error(`Unknown argument: ${arg}`);
  }
  if (ready && !decomp) throw Error('--ready needs --decomp to read existing matching records');
  const catalog = read(path.join(native, 'game/decomp-interest.json'));
  if (catalog.schemaVersion !== 1) throw Error('Unsupported interest catalog schema');
  const imported = new Map();
  for (const manifest of ['animation-imports.json', 'gameplay-imports.json']) {
    const importsFile = path.join(native, 'game/accepted', manifest);
    if (fs.existsSync(importsFile))
      for (const entry of read(importsFile).imports)
        for (const symbol of entry.symbols) imported.set(key(symbol), entry);
  }
  const sources = new Map(), evidence = new Map();
  let verifiedAt = null;
  if (decomp) {
    const registry = read(path.join(decomp, 'config/matching-c-targets.json'));
    for (const entry of registry.targets) sources.set(key(entry.symbol), entry.source);
    const state = read(path.join(decomp, 'build/current/state.json'));
    // Read the two small canonical reports only, never the producer's build tree.
    const verificationPath = path.join(decomp, 'build/current/verification.json');
    const freshPath = path.join(decomp, 'build/current/fresh-compilation.json');
    const verificationHash = hash(verificationPath), freshHash = hash(freshPath);
    const verified = read(verificationPath), fresh = read(freshPath);
    if (!state.verifiedAt || verificationHash !== state.verificationSha256 ||
        freshHash !== state.freshCompilationSha256 || verified.schemaVersion !== 5 ||
        fresh.schemaVersion !== 5 || verified.status !== 'pass' || fresh.status !== 'pass' ||
        verified.verification?.status !== 'pass')
      throw Error('Decomp reports are unavailable, incomplete or changing; retry after its verification finishes');
    const exact = new Map(verified.verification.targets.map(t => [key(t.symbol), t]));
    for (const target of fresh.targets) {
      const linked = exact.get(key(target.symbol));
      if (linked?.source === target.source && linked.linkEvidence?.fullOwnerExact === true)
        evidence.set(key(target.symbol), target);
    }
    // Some accepted owners contain several functions. Use their recorded body
    // names, not guessed addresses or merely registered compilation groups.
    for (const target of [...evidence.values()])
      for (const body of target.textContract?.compilerTextFunctions || [])
        if (!evidence.has(key(body.symbol))) evidence.set(key(body.symbol), target);
    verifiedAt = state.verifiedAt;
    if (hash(verificationPath) !== verificationHash || hash(freshPath) !== freshHash)
      throw Error('Decomp reports changed during this read; retry');
  }
  const checked = new Map();
  function unchanged(relative, expected) {
    if (!checked.has(relative)) {
      const file = path.join(decomp, relative);
      checked.set(relative, fs.existsSync(file) ? hash(file) : null);
    }
    return checked.get(relative) === expected;
  }
  const rows = [];
  for (const group of catalog.groups) {
    for (const symbol of group.symbols) {
      const target = evidence.get(key(symbol));
      const source = target?.source || sources.get(key(symbol)) || null;
      let matching = decomp ? 'Not in verified matching records' : 'Not checked';
      let available = false;
      if (target) {
        const same = unchanged(target.source, target.sourceSha256) &&
          (target.dependencies || []).every(d => unchanged(d.path, d.sha256));
        matching = target.sourceClass === 'PURE_C' ? 'Matching C' : `${target.sourceClass} (exact)`;
        if (!same) matching += ' - checkout source/dependencies differ';
        available = same && target.sourceClass === 'PURE_C';
      }
      // "Ready" means available for an integration review, not a drop-in port.
      const editorUse = group.symbolUses?.[symbol] || group.editorUse;
      const baseline = imported.get(key(symbol));
      const dependencyIdentity = entries => JSON.stringify((entries || [])
        .map(d => [d.path, d.sha256]).sort((a, b) => a[0].localeCompare(b[0])));
      const upstreamUpdate = Boolean(target && baseline &&
        (target.source !== baseline.source || target.sourceSha256 !== baseline.sourceSha256 ||
         (baseline.dependencies && dependencyIdentity(target.dependencies) !== dependencyIdentity(baseline.dependencies))));
      const needsIntegration = !['adapted', 'original', 'companion'].includes(editorUse);
      if (ready && !(available && (needsIntegration || (upstreamUpdate && editorUse !== 'companion')))) continue;
      const searchable = [symbol, source || '', group.id, group.behavior,
        ...(target?.dependencies || []).map(d => d.path)].map(key);
      const symbolQuery = path.posix.basename(query).replace(/\.c$/, '');
      if (query && key(symbol) !== symbolQuery && !searchable.some(s => s.includes(query) || (source && query.endsWith('/' + key(source))))) continue;
      rows.push({ priority: group.priority, group: group.id, behavior: group.behavior,
        symbol, source, matching, available, editorUse, groupUse: group.editorUse,
        upstreamUpdate, importedSourceSha256: baseline?.sourceSha256 || null,
        importedFile: baseline?.file || null, acceptedOwner: target?.symbol || null,
        editorFiles: group.editorFiles, benefit: group.benefit, preserve: group.preserve,
        verifiedSourceSha256: target?.sourceSha256 || null });
    }
  }
  rows.sort((a, b) => a.priority - b.priority || a.group.localeCompare(b.group) || a.symbol.localeCompare(b.symbol));
  if (json) { console.log(JSON.stringify({ generator: 'native/tools/decomp-interest.cjs',
    catalog: 'native/game/decomp-interest.json', decomp, verifiedAt, rows }, null, 2)); return; }
  console.log('# Editor Decomp interests\n');
  console.log('Generated by native/tools/decomp-interest.cjs from native/game/decomp-interest.json.');
  if (decomp) console.log(`Decomp: ${decomp}\n\nExisting verified report: ${verifiedAt}. Source availability is checked separately from editor adoption.`);
  else console.log('Matching status not checked; supply --decomp to join the existing reports.');
  console.log(`\n${rows.length} functions shown. P1: animation/actors/resources; P2: existing behavior; P3: optional expansion.\n`);
  let prior;
  for (const row of rows) {
    if (prior !== row.group) {
      console.log(`\n## P${row.priority}: ${row.behavior}\n\nEditor use: **${row.groupUse}**. ${row.benefit}\n`);
      console.log(`Consumers: ${row.editorFiles.map(f => '`native/' + f + '`').join(', ')}.\n`);
      console.log(`Keep: ${row.preserve}\n\n| Function | Upstream source | Matching record | Editor use |\n|---|---|---|---|`);
      prior = row.group;
    }
    console.log(`| ${row.symbol} | ${row.source || 'Resolve when admitted'} | ${row.matching} | ${row.editorUse}${row.upstreamUpdate ? ' (upstream changed)' : ''} |`);
  }
  if (!rows.length) console.log('No matching interests for this filter.');
}
try { main(process.argv.slice(2)); }
catch (error) { console.error(error.message); process.exitCode = 1; }
