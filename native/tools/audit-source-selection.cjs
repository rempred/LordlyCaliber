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
  let decomp, json = false, requireClean = false, requireWholeOwners = false;
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--decomp' && args[i + 1]) decomp = path.resolve(args[++i]);
    else if (args[i] === '--json') json = true;
    else if (args[i] === '--require-clean') requireClean = true;
    else if (args[i] === '--require-whole-owners') requireWholeOwners = true;
    else throw Error('Use --decomp <checkout> [--json] [--require-clean | --require-whole-owners]');
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
    const selection = owner.acceptedC ? 'compiled-c-with-bounded-hooks' : owner.cSource ?
      owner.cSourceClass === 'HYBRID_C' ? 'adapted-hybrid-c' : 'adapted-c' : owner.cFragment ? 'c-fragment-with-retained-owner' :
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
  const consumableC = game('consumables/effects-c.json');
  if (digest(fs.readFileSync(path.join(native,'game',consumableC.source))) !== consumableC.sourceSha256.toUpperCase())
    throw Error('Changed consumable C adaptation');
  const consumableOriginal=accepted.find(t=>t.symbol==='func_00041098');
  if (!consumableOriginal || consumableOriginal.sourceSha256 !== consumableC.original.sourceSha256)
    throw Error('Consumable C provenance differs from accepted source');
  check('consumable effects', Number(effectsOffset[1]), Number(effectsSize[1]), 'adapted-c',
    'rebuild/compiler.cpp', 'game/consumables/effects.c');
  const dataRules = new Set(['tactics', 'elements', 'weapons', 'soldierPopulation']);
  const partialRules = new Set(['deployment', 'deploymentMenu', 'deploymentMessage', 'trainingXp', 'resistance']);
  for (const [name, owner] of Object.entries(game('tools/game-rules.json'))) {
    if (name === 'goth') {
      const c = game('tools/goth-owner.json').plain;
      check(name, c.start, c.size + 16, 'adapted-c', 'rebuild/goth_runtime.cpp', 'game/' + c.source);
      continue;
    }
    check(name, owner.offset, owner.size, dataRules.has(name) ? 'data' : partialRules.has(name) ? 'instruction-patch' :
      name === 'reserve' ? 'new-editor-module' : 'assembly-owner', 'engine/game_rules.cpp; rebuild/compiler.cpp',
      'game/tools/game-rules.json');
  }
  for (const [name, owner] of Object.entries(game('tools/status-effects.json')))
    check(name, owner.offset, owner.size, name === 'statusPoison' ? 'data' :
      ['statusSleep', 'statusParalysis', 'statusWake'].includes(name) ? 'compiled-c-with-bounded-hooks' :
      ['statusPowerUp', 'statusPowerDown'].includes(name) ? 'instruction-patch' : 'assembly-owner',
      'engine/status_rules.cpp; rebuild/compiler.cpp; rebuild/weapon_status.cpp',
      ['statusSleep', 'statusParalysis', 'statusWake'].includes(name) ? compiledSource(owner.offset) : 'game/tools/status-effects.json');
  for (const owner of game('shops/owners.json').owners)
    check('shop ' + hex(owner.start), owner.start, owner.size,
      'adapted-c', 'rebuild/shop_runtime.cpp',
      owner.start === 0x19be40 ? 'game/shops/stock.c' :
        'game/' + game('shops/consumers.json').owners.find(c => c.start === owner.start).source);

  // A complete-owner check alone misses assembly edits inside accepted C.
  // Read the actual native block definitions so added artwork adapters enter
  // the review automatically instead of inheriting a whole-file exemption.
  const visual = fs.readFileSync(path.join(native, 'engine/body_visual_policy.cpp'), 'utf8');
  for (const match of visual.matchAll(/\{(0x[0-9a-f]+),\s*(\d+),\s*"([a-z-]+)"\}/gi))
    check(match[3], Number(match[1]), Number(match[2]), 'instruction-patch',
      'rebuild/body_visual_policy.cpp', 'game/art/' + match[3] + '.s');
  const marker = fs.readFileSync(path.join(native, 'engine/body_markers.h'), 'utf8')
    .match(/offset\s*=\s*(0x[0-9a-f]+),\s*size\s*=\s*(\d+)/i);
  if (!marker) throw Error('Equipment marker allocation needs audit support');
  check('wide-frame equipment markers', Number(marker[1]), Number(marker[2]), 'instruction-patch',
    'rebuild/body_markers.cpp', 'game/art/body-markers.s');

  for (const [header, name, startKey, sizeKey, consumer] of [
    ['persuasion', 'persuasion table lookup', 'ownerOffset', 'ownerSize', 'rebuild/persuasion.cpp'],
    ['leader_affinity', 'combat leader affinity lookup', 'offset', 'size', 'rebuild/compiler.cpp']
  ]) {
    const text = fs.readFileSync(path.join(native, 'engine', header + '.h'), 'utf8');
    const start = text.match(new RegExp('\\b' + startKey + '\\s*=\\s*(0x[0-9a-f]+|[0-9]+)', 'i'));
    const size = text.match(new RegExp('\\b' + sizeKey + '\\s*=\\s*(0x[0-9a-f]+|[0-9]+)', 'i'));
    if (!start || !size) throw Error(name + ' allocation needs audit support');
    check(name, Number(start[1]), Number(size[1]), 'assembly-owner', consumer, 'engine/' + header + '.cpp');
  }
  // Small extension entry/return hooks are not C-only either. Keep them
  // separate from the larger behavior adapters, but join every site to C.
  for (const [file, consumer, selection] of [
    ['tools/experience.json', 'rebuild/compiler.cpp', 'instruction-patch'],
    ['tools/chaos.json', 'rebuild/chaos_counter.cpp', 'extension-hook'],
    ['tools/high-attack.json', 'rebuild/high_attack.cpp', 'extension-hook'],
    ['art/animation-overrides.json', 'rebuild/animation_overrides.cpp', 'extension-hook']
  ]) for (const site of game(file).sites) {
    if (file === 'tools/chaos.json' && (site.rom ?? site.offset) === 0x23f7c) {
      const c = game('tools/chaos-call.json').active;
      check('Chaos counter task submission', c.start, c.size, 'adapted-c', consumer, 'game/' + c.source);
      continue;
    }
    check(file + ': ' + (site.name || site.label), site.rom ?? site.offset, site.size,
      selection, consumer, 'game/' + file);
  }
  for (const feature of game('tools/displays.json').features)
    for (const site of feature.sites)
      check(feature.name + ': ' + site.name, site.rom, site.size, 'extension-hook',
        'rebuild/display_tools.cpp', 'game/tools/' + site.source);
  for (const owner of game('classes/save-owners.json').pakOwners)
    check(owner.symbol, owner.fragment, owner.fragmentSize, 'extension-hook',
      'rebuild/save_runtime.cpp', 'game/classes/' + owner.source);
  for (const file of ['capacity-predicates.json', 'capacity-extra.json'])
    for (const site of game('scenarios/' + file).patches)
      check(site.name, site.hook ?? site.site, site.expected.length * 4, 'extension-hook',
        'rebuild/capacity_runtime.cpp', 'game/scenarios/' + file);

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
  const unused = new Set(['consumables/effects.s', 'promotion/family-menu.s', 'promotion/family-lookup.s', 'shops/producer.s',
    'shops/consumers.s', 'shops/selection-guard.s', 'tools/goth-retail.s', 'tools/goth-reward.s',
    'tools/reserve-bootstrap.s', 'tools/chaos-hook.s']);
  const assembly = files(path.join(native, 'game')).filter(file => file.endsWith('.s')).map(file => {
    const relative = path.relative(path.join(native, 'game'), file).replaceAll('\\', '/');
    return {file: 'game/' + relative, sha256: digest(fs.readFileSync(file)),
      classification: unused.has(relative) ? 'not-selected-by-native-export' : whole.has(relative) ? 'owner-checked-above' :
        fragments.has(relative) ? 'patch-adapter-or-new-module' : 'unclassified'};
  });
  const callSites = files(path.join(native, 'rebuild')).filter(file => file.endsWith('.cpp')).flatMap(file =>
    fs.readFileSync(file, 'utf8').split('\n').flatMap((text, i) => text.includes('assembleOwner(') && !text.includes('Compiler::assembleOwner(') ?
      [{file: path.relative(native, file).replaceAll('\\', '/'), line: i + 1, text: text.trim()}] : []));
  evidence.assertStable();
  const gaps = checks.filter(row => row.gap);
  const partialWithC = checks.filter(row => row.selection === 'instruction-patch' && row.alternatives.length);
  const hooksWithC = checks.filter(row => row.selection === 'extension-hook' && row.alternatives.length);
  const unknown = assembly.filter(row => row.classification === 'unclassified');
  const highAttackText = fs.readFileSync(path.join(native, 'game/tools/high-attack-module.s'), 'utf8');
  const highAttackReferences = [...highAttackText.slice(highAttackText.indexOf('.org 0x940')).matchAll(/\.word (0x[0-9a-f]+),(0x[0-9a-f]+)/gi)]
    .map(m => ({address: Number(m[1]), expected: Number(m[2])}));
  if (highAttackReferences.length !== 546) throw Error('High Attack runtime reference census needs review');
  const deferredGroups = [
    {name: 'Body/equipment visuals and wide markers', reason: 'The adapter includes three fragments without complete accepted C; markers share its constructor.'},
    {name: 'Expanded capacity', reason: 'Only 17 of 102 entry/return hooks have accepted C. Compiler-derived settlement data relocations are preserved separately.'},
    {name: 'High Attack and animation overrides', reason: 'High Attack also changes 546 runtime references in mixed C/assembly owners. Animation overrides share constructor 0021CBC4 and its fixed register/continuation contract.'},
    {name: 'Custom encounters', reason: 'Materialization, persuasion, retreat, recruit and result consumers still include unrecovered full functions.'}
  ];
  const result = {generator: 'native/tools/audit-source-selection.cjs',
    status: gaps.length || unknown.length ? 'gaps-found' : partialWithC.length || hooksWithC.length ? 'partial-c-migrations-remain' : 'no-catalogued-gaps',
    fingerprint: state.fingerprint, verifiedAt: state.verifiedAt,
    evidence: {pureOwners: accepted.filter(t => t.sourceClass === 'PURE_C').length,
      hybridOwners: accepted.filter(t => t.sourceClass === 'HYBRID_C').length, authenticatedFiles: authenticated.size},
    scope: 'Native export owner catalogs, game/status-rule instruction emitters, artwork adapters, affinity/persuasion lookups, tool/animation/capacity/Pak extension hooks, bundled assembly census and rebuild assembleOwner call sites. Excludes Editor-Old. Whole-owner gaps, partial C-covered adapters and extension hooks are reported separately; --require-clean includes all three, while --require-whole-owners retains the narrower historical gate. Static selection audit, not a ROM execution test or a semantic replacement proof.',
    gaps, partialWithC, hooksWithC, checks, assembly, callSites, deferredGroups, highAttackReferences, unclassifiedAssembly: unknown};
  if (json) console.log(JSON.stringify(result, null, 2));
  else {
    console.log(`Accepted evidence: ${state.fingerprint.slice(0, 8)}; ${result.evidence.pureOwners} PURE_C / ${result.evidence.hybridOwners} HYBRID_C owners.`);
    console.log(`Whole-owner assembly gaps: ${gaps.length}; assembly files: ${assembly.length}; unclassified: ${unknown.length}.`);
    console.log(`Partial instruction adapters inside accepted C: ${partialWithC.length}. These are not reported as converted.`);
    console.log(`Extension hook sites inside accepted C: ${hooksWithC.length}. These retain assembly ABI bridges.`);
    for (const row of gaps) console.log(`${hex(row.start)} ${row.name}: ${row.alternatives.map(t => t.sourceClass + ' ' + t.symbol).join(', ')} — ${row.consumer}`);
    for (const row of partialWithC) console.log(`Partial ${hex(row.start)} ${row.name}: ${row.alternatives.map(t => t.symbol).join(', ')} — ${row.consumer}`);
    console.log(result.scope);
  }
  if ((requireClean || requireWholeOwners) && (gaps.length || unknown.length) ||
      requireClean && (partialWithC.length || hooksWithC.length)) process.exitCode = 2;
}
try { main(process.argv.slice(2)); }
catch (error) { console.error(error.message); process.exitCode = 1; }
