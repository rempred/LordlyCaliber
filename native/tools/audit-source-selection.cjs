'use strict';
// Read-only audit of native export source selection against accepted Decomp
// evidence. No Decomp tools/builds, ROM writes or source imports are performed.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {readAcceptedEvidence} = require('./decomp-evidence.cjs');
const native = path.resolve(__dirname, '..');
const read = file => JSON.parse(fs.readFileSync(file, 'utf8'));
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
const game = file => read(path.join(native, 'game', file));
const hex = number => '0x' + number.toString(16).toUpperCase().padStart(8, '0');
function files(directory) {
  return fs.readdirSync(directory, {withFileTypes: true}).flatMap(entry =>
    entry.isDirectory() ? files(path.join(directory, entry.name)) : [path.join(directory, entry.name)]);
}
function main(args) {
  let decomp, json = false, requireClean = false;
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--decomp' && args[i + 1]) decomp = path.resolve(args[++i]);
    else if (args[i] === '--json') json = true;
    else if (args[i] === '--require-clean') requireClean = true;
    else throw Error('Use --decomp <checkout> [--json] [--require-clean]');
  }
  if (!decomp) throw Error('--decomp is required; absence of evidence is not a clean audit');
  const evidence = readAcceptedEvidence(decomp), {state} = evidence;
  const accepted = evidence.targets;
  const authenticated = new Set();
  for (const target of accepted) {
    for (const dependency of [{path: target.source, sha256: target.sourceSha256}, ...(target.dependencies || [])]) {
      evidence.readSource(dependency.path, dependency.sha256);
      authenticated.add(dependency.path);
    }
  }
  const checks = [];
  const compiledOwners = new Map(game('accepted/owners.json').owners.map(owner => [owner.start, owner]));
  function compiledSource(start) {
    const owner = compiledOwners.get(start);
    if (!owner || digest(fs.readFileSync(path.join(native, 'game', owner.source))) !== owner.sourceSha256.toUpperCase())
      throw Error('Missing or changed compiled C owner: ' + hex(start));
    const target = accepted.find(t => t.symbol === owner.symbol);
    if (!target || target.sourceClass !== owner.sourceClass || target.sourceSha256.toLowerCase() !== owner.sourceSha256)
      throw Error('Compiled owner disagrees with accepted evidence: ' + hex(start));
    return 'game/' + owner.source;
  }
  function check(name, start, size, selection, consumer, source) {
    if (!Number.isSafeInteger(start) || !Number.isSafeInteger(size) || start < 0 || size <= 0)
      throw Error('Invalid owner range: ' + name);
    const alternatives = accepted.filter(target => target.owners.some(owner =>
      owner.romStart < start + size && owner.romEndExclusive > start)).map(target => ({
        symbol: target.symbol, sourceClass: target.sourceClass, source: target.source,
        owners: target.owners.map(owner => ({start: owner.romStart, bytes: owner.bytes}))
      }));
    checks.push({name, start, size, selection, consumer, source, alternatives,
      gap: selection === 'assembly-owner' && alternatives.length > 0});
  }
  for (const owner of game('promotion/owners.json').owners) {
    const selection = owner.acceptedC ? 'compiled-c-with-bounded-hooks' : owner.cSource ? 'adapted-c' : owner.cFragment ? 'c-fragment-with-retained-owner' :
      owner.start === 0xbbd50 ? 'custom-family-return-abi' : 'assembly-owner';
    check(owner.name, owner.start, owner.size, selection, 'rebuild/promotion_runtime.cpp; rebuild/compiler.cpp',
      owner.acceptedC ? compiledSource(owner.start) : owner.cSource ? 'game/promotion/' + owner.cSource : owner.start === 0xbbd50 ? 'game/promotion/family-lookup.s' :
      'game/promotion/owners.json (generated instruction source)');
  }
  for (const owner of game('encounters/custom-owners/catalog.json').owners)
    check(owner.name, owner.start, owner.size, owner.acceptedC ? 'compiled-c-with-bounded-hooks' : 'assembly-owner', 'rebuild/custom_encounters.cpp',
      owner.acceptedC ? compiledSource(owner.start) : 'game/encounters/custom-owners/' + owner.name + '.s');
  for (const name of ['dispatcher', 'rewards']) {
    const owner = game('encounters/' + name + '.json');
    check(name, owner.start, owner.size, owner.acceptedC ? 'compiled-c-with-bounded-hooks' : 'assembly-owner',
      name === 'dispatcher' ? 'rebuild/scenario_rates.cpp; rebuild/compiler.cpp; rebuild/custom_encounters.cpp' :
        'rebuild/creature_rewards.cpp; rebuild/custom_encounters.cpp', owner.acceptedC ? compiledSource(owner.start) : 'game/encounters/' + name + '.s');
  }
  const effectsHeader = fs.readFileSync(path.join(native, 'engine/consumable_effects.h'), 'utf8');
  const effectsOffset = effectsHeader.match(/romOffset\s*=\s*(0x[0-9a-f]+)/i);
  const effectsSize = effectsHeader.match(/codeSize\s*=\s*(\d+)/);
  if (!effectsOffset || !effectsSize) throw Error('Consumable owner declaration needs audit support');
  check('consumable effects', Number(effectsOffset[1]), Number(effectsSize[1]), 'assembly-owner',
    'rebuild/compiler.cpp', 'game/consumables/effects.s');
  const dataRules = new Set(['tactics', 'elements', 'weapons', 'soldierPopulation']);
  const partialRules = new Set(['deployment', 'deploymentMenu', 'deploymentMessage', 'trainingXp', 'resistance', 'goth']);
  for (const [name, owner] of Object.entries(game('tools/game-rules.json')))
    check(name, owner.offset, owner.size, dataRules.has(name) ? 'data' : partialRules.has(name) ? 'instruction-patch' :
      name === 'reserve' ? 'new-editor-module' : 'assembly-owner', 'engine/game_rules.cpp; rebuild/compiler.cpp',
      'game/tools/game-rules.json');
  for (const [name, owner] of Object.entries(game('tools/status-effects.json')))
    check(name, owner.offset, owner.size, name === 'statusPoison' ? 'data' :
      ['statusSleep', 'statusParalysis', 'statusWake'].includes(name) ? 'compiled-c-with-bounded-hooks' :
      ['statusPowerUp', 'statusPowerDown'].includes(name) ? 'instruction-patch' : 'assembly-owner',
      'engine/status_rules.cpp; rebuild/compiler.cpp; rebuild/weapon_status.cpp',
      ['statusSleep', 'statusParalysis', 'statusWake'].includes(name) ? compiledSource(owner.offset) : 'game/tools/status-effects.json');
  for (const owner of game('shops/owners.json').owners)
    check('shop ' + hex(owner.start), owner.start, owner.size,
      owner.start === 0x19be40 ? 'adapted-c' : 'instruction-patch', 'rebuild/shop_runtime.cpp',
      owner.start === 0x19be40 ? 'game/shops/stock.c' : 'game/shops/consumers.s');

  // Explicit census: new assembly files cannot silently inherit an exemption.
  // These are authored hooks/register adapters, display-list data or new editor
  // modules, rather than selectable copies of complete original functions.
  const fragments = new Set(`art/animation-copy.s art/animation-override-bootstrap.s art/animation-override-carriers.s
art/animation-override-entry.s art/animation-override-hook.s art/animation-override-lookup.s
art/animation-override-mode-return.s art/animation-override-return.s art/body-construction.s
art/body-entrance-skip.s art/body-entrance-sticky.s art/body-markers.s art/body-roster-sticky.s
art/equipment-allocate.s art/equipment-image-context.s art/equipment-image-kind.s art/equipment-image.s
art/equipment-register.s classes/description-index.s classes/pak-export.s classes/pak-import.s
encounters/custom-sites.s encounters/dispatcher-hook-0.s encounters/dispatcher-hook-1.s
encounters/dispatcher-hook-2.s encounters/rewards-hook-0.s promotion/class-render-entry.s promotion/class-render-style.s
promotion/equipment-first-class.s scenarios/capacity-battle-fields.s scenarios/capacity-battle-hp.s
scenarios/capacity-battle-type.s shops/consumers.s shops/selection-guard.s tools/alignment-bootstrap.s
tools/alignment-module.s tools/alignment-templates.s tools/chaos-bootstrap.s tools/chaos-continuation.s
tools/chaos-hook.s tools/chaos-module.s tools/chaos-title-list.s tools/display-hook.s tools/experience.s
tools/goth-retail.s tools/goth-reward.s tools/high-attack-bootstrap.s tools/high-attack-continuation.s
tools/high-attack-hooks.s tools/high-attack-module.s tools/luck-bootstrap.s tools/luck-module.s
tools/luck-templates.s tools/reserve-bootstrap.s tools/reserve-xp.s`.split(/\s+/));
  const whole = new Set(checks.map(row => row.source).filter(value => value.endsWith('.s')).map(value => value.slice(5)));
  for (const name of ['tools/clock.s', 'tools/clock-retail.s']) whole.add(name);
  const unused = new Set(['promotion/family-menu.s', 'shops/producer.s']);
  const assembly = files(path.join(native, 'game')).filter(file => file.endsWith('.s')).map(file => {
    const relative = path.relative(path.join(native, 'game'), file).replaceAll('\\', '/');
    return {file: 'game/' + relative, sha256: digest(fs.readFileSync(file)),
      classification: whole.has(relative) ? 'owner-checked-above' : fragments.has(relative) ? 'patch-adapter-or-new-module' :
        unused.has(relative) ? 'not-selected-by-native-export' : 'unclassified'};
  });
  const callSites = files(path.join(native, 'rebuild')).filter(file => file.endsWith('.cpp')).flatMap(file =>
    fs.readFileSync(file, 'utf8').split('\n').flatMap((text, i) => text.includes('assembleOwner(') && !text.includes('Compiler::assembleOwner(') ?
      [{file: path.relative(native, file).replaceAll('\\', '/'), line: i + 1, text: text.trim()}] : []));
  evidence.assertStable();
  const gaps = checks.filter(row => row.gap);
  const unknown = assembly.filter(row => row.classification === 'unclassified');
  const result = {generator: 'native/tools/audit-source-selection.cjs',
    status: gaps.length || unknown.length ? 'gaps-found' : 'no-catalogued-gaps',
    fingerprint: state.fingerprint, verifiedAt: state.verifiedAt,
    evidence: {pureOwners: accepted.filter(t => t.sourceClass === 'PURE_C').length,
      hybridOwners: accepted.filter(t => t.sourceClass === 'HYBRID_C').length, authenticatedFiles: authenticated.size},
    scope: 'Native export owner catalogs, game/status-rule instruction emitters, bundled assembly census and rebuild assembleOwner call sites. Excludes Editor-Old and unchanged source-ROM bytes. Static selection audit, not a ROM execution test or an automatic proof of semantic replacement.',
    gaps, checks, assembly, callSites, unclassifiedAssembly: unknown};
  if (json) console.log(JSON.stringify(result, null, 2));
  else {
    console.log(`Accepted evidence: ${state.fingerprint.slice(0, 8)}; ${result.evidence.pureOwners} PURE_C / ${result.evidence.hybridOwners} HYBRID_C owners.`);
    console.log(`Whole-owner assembly gaps: ${gaps.length}; assembly files: ${assembly.length}; unclassified: ${unknown.length}.`);
    for (const row of gaps) console.log(`${hex(row.start)} ${row.name}: ${row.alternatives.map(t => t.sourceClass + ' ' + t.symbol).join(', ')} — ${row.consumer}`);
    console.log(result.scope);
  }
  if (requireClean && (gaps.length || unknown.length)) process.exitCode = 2;
}
try { main(process.argv.slice(2)); }
catch (error) { console.error(error.message); process.exitCode = 1; }
