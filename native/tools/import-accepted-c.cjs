'use strict';
// Import only the source identities authenticated by Decomp's completed verifier.
// Never invokes its tools; v6 reads only named, authenticated proof files.
const fs = require('node:fs'), path = require('node:path');
const {execFileSync} = require('node:child_process');
const {readAcceptedEvidence, sourcePath, digest, same} = require('./decomp-evidence.cjs');
function importAccepted(root, native, write = false) {
    const evidence = readAcceptedEvidence(root), {state} = evidence;
    const files = new Map(), producers = new Map();
    function retain(relative, expected) {
        files.set(relative, evidence.readSource(relative, expected));
    }
    let owners = 0;
    for (const t of evidence.targets) {
        if (t.sourceClass !== 'PURE_C') continue;
        retain(t.source, t.sourceSha256);
        for (const d of t.dependencies) retain(d.path, d.sha256);
        const functions = t.functions.map(b => b.symbol);
        let entry = producers.get(t.source);
        const dependencies = t.dependencies.map(d => ({...d, file: 'game/accepted/library/' + d.path}));
        if (!entry) {
            entry = {source: t.source, sourceSha256: t.sourceSha256,
                file: 'game/accepted/library/' + t.source, symbols: [], owners: [], dependencies,
                ...(t.provenance ? {compiler: t.provenance.compiler,
                    ...(t.provenance.producer ? {producer: t.provenance.producer} : {})} : {})};
            producers.set(t.source, entry);
        } else if (entry.sourceSha256 !== t.sourceSha256 || !same(entry.dependencies, dependencies) ||
            !same(entry.compiler, t.provenance?.compiler) || !same(entry.producer, t.provenance?.producer)) {
            throw Error('Inconsistent producer identity: ' + t.source);
        }
        entry.symbols = [...new Set([...entry.symbols, ...functions])].sort();
        entry.owners.push({symbol: t.symbol, functions,
            ...(t.provenance ? {proof: t.provenance.proof, textMode: t.provenance.textMode,
                physicalOwners: t.owners.map(o => ({romStart: o.romStart, bytes: o.bytes,
                    sectionName: o.sectionName, sha256: o.linkedSha256}))} : {})});
        owners++;
    }
    for (const entry of producers.values()) entry.owners.sort((a, b) => a.symbol.localeCompare(b.symbol));
    const manifest = {schemaVersion: 1, upstream: 'OB64 Decomp', evidenceSchemaVersion: state.schemaVersion,
        revision: execFileSync('git', ['rev-parse', 'HEAD'], {cwd: root, encoding: 'utf8'}).trim(),
        verifiedAt: state.verifiedAt, sourceClass: 'PURE_C', fingerprint: state.fingerprint,
        verificationSha256: state.verificationSha256, freshCompilationSha256: state.freshCompilationSha256,
        purpose: 'Complete accepted pure-C source library. Source availability is separate from active editor adoption; existing adaptation manifests retain their own baselines.',
        counts: {owners, functions: new Set([...producers.values()].flatMap(t => t.symbols)).size,
            producerSources: producers.size, sourceAndHeaderFiles: files.size},
        imports: [...producers.values()].sort((a, b) => a.source.localeCompare(b.source))};
    // A grouped upstream producer can retire older one-function files. Remove only
    // unchanged files named by our previous manifest, never user files or adapters.
    const manifestPath = path.join(native, 'game/accepted/library-imports.json'), retired = [];
    if (fs.existsSync(manifestPath)) {
        const prior = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
        if (prior.schemaVersion !== 1 || !Array.isArray(prior.imports)) throw Error('Unsupported prior library manifest');
        const previousFiles = new Map();
        for (const entry of prior.imports) {
            for (const record of [{path: entry.source, sha256: entry.sourceSha256, file: entry.file}, ...(entry.dependencies || [])]) {
                sourcePath(record.path);
                if (record.file !== 'game/accepted/library/' + record.path ||
                    (previousFiles.has(record.path) && previousFiles.get(record.path) !== record.sha256))
                    throw Error('Inconsistent prior library manifest: ' + record.path);
                previousFiles.set(record.path, record.sha256);
            }
        }
        for (const [relative, expected] of previousFiles) {
            const file = path.join(native, 'game/accepted/library', relative);
            if (files.has(relative) || !fs.existsSync(file)) continue;
            if (digest(fs.readFileSync(file)) !== expected) throw Error('Retired library source has local edits: ' + relative);
            retired.push(file);
        }
    }
    evidence.assertStable();
    if (write) {
        for (const [relative, data] of files) {
            const dest = path.join(native, 'game/accepted/library', relative);
            fs.mkdirSync(path.dirname(dest), {recursive: true});
            if (!fs.existsSync(dest) || !fs.readFileSync(dest).equals(data)) fs.writeFileSync(dest, data);
        }
        for (const file of retired) fs.unlinkSync(file);
        fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + '\n');
    }
    return manifest;
}
function main(args) {
    const native = path.resolve(__dirname, '..');
    const write = args.includes('--write');
    const index = args.indexOf('--decomp');
    if (index < 0 || !args[index + 1]) throw Error('Use --decomp <checkout> [--write]');
    const root = path.resolve(args[index + 1]);
    const manifest = importAccepted(root, native, write);
    console.log(JSON.stringify({mode: write ? 'imported' : 'checked', verifiedAt: manifest.verifiedAt, ...manifest.counts}));
}
if (require.main === module) {
    try { main(process.argv.slice(2)); }
    catch (error) { console.error(error.message); process.exitCode = 1; }
}
module.exports = {importAccepted};
