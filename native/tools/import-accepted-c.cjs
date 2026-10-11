'use strict';
// Import only the source identities authenticated by Decomp's completed verifier.
// Never invokes its tools or accesses its production output directory.
const fs = require('node:fs'), path = require('node:path'), crypto = require('node:crypto');
const {execFileSync} = require('node:child_process');
const native = path.resolve(__dirname, '..');
const args = process.argv.slice(2), write = args.includes('--write');
const index = args.indexOf('--decomp');
if (index < 0 || !args[index + 1]) throw Error('Use --decomp <checkout> [--write]');
const root = path.resolve(args[index + 1]);
const digest = b => crypto.createHash('sha256').update(b).digest('hex').toUpperCase();
const reportPaths = ['state', 'verification', 'fresh-compilation'].map(n => path.join(root, 'build/current', n + '.json'));
const raw = reportPaths.map(p => fs.readFileSync(p));
const [state, verification, fresh] = raw.map(b => JSON.parse(b));
if (state.schemaVersion !== 5 || verification.schemaVersion !== 5 || fresh.schemaVersion !== 5 ||
    !state.verifiedAt || digest(raw[1]) !== state.verificationSha256 ||
    digest(raw[2]) !== state.freshCompilationSha256 || verification.status !== 'pass' ||
    verification.verification?.status !== 'pass' || fresh.status !== 'pass')
    throw Error('Missing, changed or unsupported accepted Decomp evidence');
const exact = new Map(verification.verification.targets.map(t => [t.symbol, t]));
const files = new Map(), producers = new Map();
function retain(relative, expected) {
    if (!/^(src|include|third_party\/lha\/ob64)\/[A-Za-z0-9_./-]+\.(c|h)$/.test(relative) || relative.split('/').includes('..'))
        throw Error('Unsupported source dependency: ' + relative);
    const data = fs.readFileSync(path.join(root, relative));
    if (digest(data) !== expected) throw Error('Checkout differs from accepted source: ' + relative);
    if (files.has(relative) && !files.get(relative).equals(data)) throw Error('Inconsistent source: ' + relative);
    files.set(relative, data);
}
let owners = 0;
for (const t of fresh.targets) {
    if (t.sourceClass !== 'PURE_C') continue;
    const linked = exact.get(t.symbol);
    if (linked?.source !== t.source || !linked.linkEvidence?.fullOwnerExact)
        throw Error('Missing exact linked owner: ' + t.symbol);
    retain(t.source, t.sourceSha256);
    for (const d of t.dependencies || []) retain(d.path, d.sha256);
    const functions = t.textContract.compilerTextFunctions.map(b => b.symbol);
    if (!functions.length) throw Error('Missing compiler function census: ' + t.symbol);
    let entry = producers.get(t.source);
    if (!entry) {
        entry = {source: t.source, sourceSha256: t.sourceSha256,
            file: 'game/accepted/library/' + t.source, symbols: [], owners: [],
            dependencies: (t.dependencies || []).map(d => ({...d, file: 'game/accepted/library/' + d.path}))};
        producers.set(t.source, entry);
    }
    entry.symbols = [...new Set([...entry.symbols, ...functions])].sort();
    entry.owners.push({symbol: t.symbol, functions});
    owners++;
}
for (const entry of producers.values()) entry.owners.sort((a, b) => a.symbol.localeCompare(b.symbol));
for (let i = 0; i < raw.length; i++)
    if (!fs.readFileSync(reportPaths[i]).equals(raw[i])) throw Error('Decomp verification changed during import');
const manifest = {schemaVersion: 1, upstream: 'OB64 Decomp',
    revision: execFileSync('git', ['rev-parse', 'HEAD'], {cwd: root, encoding: 'utf8'}).trim(),
    verifiedAt: state.verifiedAt, sourceClass: 'PURE_C', fingerprint: state.fingerprint,
    verificationSha256: state.verificationSha256, freshCompilationSha256: state.freshCompilationSha256,
    purpose: 'Complete accepted pure-C source library. Source availability is separate from active editor adoption; existing adaptation manifests retain their own baselines.',
    counts: {owners, functions: new Set([...producers.values()].flatMap(t => t.symbols)).size,
        producerSources: producers.size, sourceAndHeaderFiles: files.size},
    imports: [...producers.values()].sort((a, b) => a.source.localeCompare(b.source))};
if (write) {
    for (const [relative, data] of files) {
        const dest = path.join(native, 'game/accepted/library', relative);
        fs.mkdirSync(path.dirname(dest), {recursive: true});
        if (!fs.existsSync(dest) || !fs.readFileSync(dest).equals(data)) fs.writeFileSync(dest, data);
    }
    fs.writeFileSync(path.join(native, 'game/accepted/library-imports.json'), JSON.stringify(manifest, null, 2) + '\n');
}
console.log(JSON.stringify({mode: write ? 'imported' : 'checked', verifiedAt: state.verifiedAt, ...manifest.counts}));
