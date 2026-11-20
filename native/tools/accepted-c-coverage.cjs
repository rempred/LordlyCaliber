'use strict';
// Join accepted source with editor consumers and optional original-instruction sound replays.
const fs = require('node:fs');
const path = require('node:path');
const cp = require('node:child_process');
const crypto = require('node:crypto');

function options(args) {
  const result = {};
  for (let i = 0; i < args.length; ++i) {
    if (!['--native', '--decomp', '--base', '--head', '--rom', '--output', '--table'].includes(args[i]) || !args[i + 1])
      throw Error('Use --native <directory> --decomp <directory> --base <git revision> --head <git revision> [--rom <ROM>] [--output <JSON>] [--table <TSV>]');
    result[args[i].slice(2)] = args[++i];
  }
  for (const required of ['native', 'decomp', 'base', 'head'])
    if (!result[required]) throw Error('Missing --' + required);
  result.native = path.resolve(result.native);
  result.decomp = path.resolve(result.decomp);
  return result;
}
const read = file => JSON.parse(fs.readFileSync(file, 'utf8').replace(/^\uFEFF/, ''));
const sha = data => crypto.createHash('sha256').update(data).digest('hex').toUpperCase();
const norm = value => value.toLowerCase();
const hex = n => '0x' + n.toString(16).toUpperCase().padStart(8, '0');
const tokenKey = value => /^0x/i.test(value) ? '0x' + parseInt(value, 16).toString(16) : norm(value);
function walk(root) {
  return fs.readdirSync(root, {withFileTypes: true}).flatMap(entry => {
    const file = path.join(root, entry.name);
    return entry.isDirectory() ? walk(file) : [file];
  });
}
function gitJson(root, revision, file) {
  return JSON.parse(cp.execFileSync('git', ['-C', root, 'show', revision + ':' + file],
    {encoding: 'utf8', maxBuffer: 32 * 1024 * 1024}));
}
function noComments(text) {
  // Preserve string literals and line positions while removing C/C++ comments.
  return text.replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, match =>
    match.startsWith('/*') || match.startsWith('//') ? match.replace(/[^\n]/g, ' ') : match);
}
function main() {
  const opt = options(process.argv.slice(2));
  const before = gitJson(opt.decomp, opt.base, 'config/matching-c-targets.json').targets;
  const after = gitJson(opt.decomp, opt.head, 'config/matching-c-targets.json').targets;
  const old = new Set(before.map(t => norm(t.symbol)));
  const additions = after.filter(t => !old.has(norm(t.symbol)));
  const original = gitJson(opt.decomp, opt.head, 'asm/original/rev0/manifest.json').chunks.flatMap(c => c.parts);
  const physical = new Map(original.map(p => [norm(p.name), p]));
  const logical = new Map(gitJson(opt.decomp, opt.head, 'config/logical-functions.json').bodies.map(p => [norm(p.symbol), p]));
  const imports = read(path.join(opt.native, 'game/accepted/library-imports.json'));
  const recipes = read(path.join(opt.native, 'game/accepted/rom-owners.json'));
  const interests = read(path.join(opt.native, 'game/decomp-interest.json')).groups;
  const effectRecipes = read(path.join(opt.native, 'game/accepted/effect-owners.json'));
  const evidence = [], evidenceKeys = new Map();
  function item(kind, file, location, text, extra = {}) {
    const key = JSON.stringify([kind, file, location, text, extra]);
    if (!evidenceKeys.has(key)) {
      evidenceKeys.set(key, evidence.length);
      evidence.push({kind, file, ...location, text: text.slice(0, 300), ...extra});
    }
    return evidenceKeys.get(key);
  }
  const bySymbol = new Map(), byRom = [];
  for (const target of additions) {
    const owner = physical.get(norm(target.symbol));
    if (!owner) throw Error('Missing original owner for ' + target.symbol);
    const body = logical.get(norm(target.symbol));
    const start = body ? body.romStart : Number(owner.romStart);
    const end = body ? body.romEndExclusive : Number(owner.romEndExclusive);
    const row = {symbol: target.symbol, source: target.source, romStart: hex(start),
      bytes: end - start, sourceClass: 'unavailable-in-editor-manifest', aliases: [],
      baseline: 'pending-library-and-ROM-recipe', host: [], services: [], extension: [], references: [], context: [],
      interestGroups: []};
    row._start = start;
    row._end = end;
    if (Number(owner.romEndExclusive) > end) {
      const field = body.evidence.caseId.startsWith('routine-row') ? 'retainedOriginalSuffix' : 'followingEnvelope';
      row[field] = {romStart: hex(end), bytes: Number(owner.romEndExclusive) - end,
        caseId: body.evidence.caseId, rationale: body.evidence.rationale};
    }
    row._aliases = new Set([norm(target.symbol)]);
    bySymbol.set(norm(target.symbol), row);
    byRom.push(row);
  }
  byRom.sort((a, b) => a._start - b._start);
  function at(address) {
    let lo = 0, hi = byRom.length;
    while (lo < hi) {
      const mid = (lo + hi) >>> 1;
      if (byRom[mid]._start <= address) lo = mid + 1;
      else hi = mid;
    }
    const row = byRom[lo - 1];
    return row && address < row._end ? row : null;
  }
  for (const entry of imports.imports) {
    const candidates = new Set([...(entry.symbols || []),
      ...(entry.owners || []).flatMap(o => [o.symbol, ...(o.functions || [])])].map(norm));
    for (const symbol of candidates) {
      const row = bySymbol.get(symbol);
      if (!row) continue;
      const file = path.join(opt.native, entry.file);
      if (!fs.existsSync(file) || sha(fs.readFileSync(file)) !== entry.sourceSha256)
        throw Error('Missing or changed bundled source for ' + row.symbol);
      row.sourceClass = entry.sourceClass;
      row.libraryFile = entry.file;
      row.librarySourceSha256 = entry.sourceSha256;
      for (const owner of entry.owners || [])
        if (norm(owner.symbol) === symbol || (owner.functions || []).map(norm).includes(symbol)) {
          row.ownerSymbols = [...new Set([owner.symbol, ...(owner.functions || [])])];
          for (const alias of row.ownerSymbols) row._aliases.add(norm(alias));
        }
      row.baseline = 'bundled-without-ROM-recipe';
    }
  }
    for (const recipe of recipes.sources) {
    const recipeSymbols = new Set([recipe.name, ...(recipe.targets || [])].map(norm));
    for (const symbol of recipeSymbols) {
      const row = bySymbol.get(symbol);
      if (!row) continue;
      const spans = recipe.parts.filter(p => p.start < row._end && p.start + p.size > row._start);
      if (spans.some(p => p.start <= row._start && p.start + p.size >= row._end)) {
        if (!row.libraryFile || recipe.sourceSha256 !== row.librarySourceSha256)
          throw Error('ROM recipe differs from bundled source for ' + row.symbol);
        row.baseline = 'registered-compiled-ROM-owner';
        row.romRecipe = recipe.name;
      }
      for (const span of spans) {
        const address = span.address + row._start - span.start;
        row.aliases.push({address: hex(address), source: 'game/accepted/rom-owners.json', recipe: recipe.name});
        for (const alias of [hex(address).toLowerCase(), 'func_' + address.toString(16).padStart(8, '0')]) {
          row._aliases.add(alias);
        }
      }
    }
  }
  const sourceFiles = ['app', 'engine', 'rebuild', 'game'].flatMap(dir => walk(path.join(opt.native, dir)))
    .filter(file => !path.relative(opt.native, file).split(path.sep).includes('accepted'))
    .filter(file => /\.(?:cpp|c|h|hpp|s|json|cjs)$/.test(file))
    .filter(file => !/(?:coverage|inventory-report|accepted-c-coverage|source-selection-audit)\.(?:json|cjs)$/i.test(path.basename(file)));
  // Bind canonical symbols using current editor catalogs, including relocated overlays.
  function bind(value, file, trail) {
    if (!value || typeof value !== 'object') return;
    if (!Array.isArray(value) && Number.isSafeInteger(value.address)) {
      const start = value.start ?? value.rom ?? value.offset;
      if (Number.isSafeInteger(start)) {
        const row = at(start);
        if (row && start === row._start) {
          const alias = {address: hex(value.address), source: file, jsonPath: trail};
          if (!row.aliases.some(a => a.address === alias.address)) row.aliases.push(alias);
          row._aliases.add(hex(value.address).toLowerCase());
          row._aliases.add('func_' + value.address.toString(16).padStart(8, '0'));
        }
      }
    }
    for (const [key, child] of Object.entries(value)) bind(child, file, trail + '.' + key);
  }
  for (const file of sourceFiles.filter(f => f.endsWith('.json')))
    bind(read(file), path.relative(opt.native, file).split(path.sep).join('/'), '$');
  const lookup = new Map();
  for (const row of byRom) {
    row._aliases.add(hex(row._start).toLowerCase());
    for (const alias of row._aliases) {
      const key = tokenKey(alias);
      if (!lookup.has(key)) lookup.set(key, new Set());
      lookup.get(key).add(row);
    }
    row.interestGroups = interests.filter(g => g.symbols.map(norm).includes(norm(row.symbol))).map(g => ({
      id: g.id, priority: g.priority, editorUse: g.symbolUses?.[row.symbol] || g.editorUse,
      files: g.editorFiles, benefit: g.benefit
    }));
  }
  const token = /\b(?:0[xX][0-9a-fA-F]+|[A-Za-z_][A-Za-z_0-9]*)\b/g;
  function scan(file, context = false) {
    const relative = path.relative(opt.native, file).split(path.sep).join('/');
    const text = fs.readFileSync(file, 'utf8');
    const raw = text.split('\n'), lines = noComments(text).split('\n');
    for (let i = 0; i < raw.length; ++i) {
      const code = context ? '' : (lines[i] || '');
      const comment = context ? '' : Array.from(raw[i], (c, j) => c !== code[j] ? c : ' ').join('');
      const kinds = context ? [[raw[i], 'context']] : [[code, 'references'], [comment, 'context']];
      for (const [line, field] of kinds) {
        const seen = new Set();
        for (const match of line.matchAll(token)) {
          const key = tokenKey(match[0]);
          for (const row of lookup.get(key) || []) {
            if (seen.has(row.symbol)) continue;
            seen.add(row.symbol);
            row[field].push(item(field === 'context' ? 'comment-doc-or-test-reference' : 'lexical-code-reference',
              relative, {line: i + 1}, raw[i]));
          }
        }
      }
    }
  }
  sourceFiles.forEach(f => scan(f, path.relative(opt.native, f).split(path.sep).join('/') === 'game/decomp-interest.json'));
  for (const dir of ['tests'])
    for (const file of walk(path.join(opt.native, dir)).filter(f => /\.(?:cpp|c|h|cjs)$/.test(f)))
      scan(file, true);
  const romKeys = new Set(['site', 'start', 'offset', 'rom', 'fragment', 'hook', 'romStart']);
  function catalog(value, file, trail, inheritedReason = '') {
    if (!value || typeof value !== 'object') return;
    const groups = [...new Set((Array.isArray(value.references) ? value.references : []).flatMap(r => (r.uses || []).map(u => u.group)))];
    const calls = (Array.isArray(value.calls) ? value.calls : []).map(c => c.symbol);
    const reason = value.reason || (groups.length ? 'Relocate expanded ' + groups.join(', ') + ' arrays.' :
      calls.length ? 'Redirect ' + calls.join(', ') + ' calls for expanded allocations.' :
      value.entry ? 'Intercept ' + value.entry + ' at the retail entry.' :
      value.context?.kind ? 'Preserve expanded army source ownership (' + value.context.kind + ').' : inheritedReason);
    for (const [key, child] of Object.entries(value)) {
      if (romKeys.has(key) && Number.isSafeInteger(child)) {
        const row = at(child);
        if (row) {
          const content = fs.readFileSync(path.join(opt.native, file), 'utf8');
          const number = new RegExp('"' + key + '"\\s*:\\s*' + child + '(?:\\s*[,}])');
          const found = number.exec(content);
          row.extension.push(item('catalog-ROM-address', file,
            {jsonPath: trail + '.' + key, line: found ? content.slice(0, found.index).split('\n').length : null},
            hex(child), {ownerOffset: child - row._start, reason: reason || 'ROM owner address retained in the export catalog.'}));
        }
      }
      catalog(child, file, trail + '.' + key, reason);
    }
  }
  for (const file of sourceFiles.filter(f => f.endsWith('.json')))
    catalog(read(file), path.relative(opt.native, file).split(path.sep).join('/'), '$');

  // Recognized preview loaders. "host" records static code-loading eligibility,
  // never an observed execution claim. Preserve each address space separately.
  const vm = [];
  function vmRange(file, line, start, end, address, text) {
    if (end <= start) throw Error('Invalid VM range in ' + file);
    const entry = item('VM-code-load-range', file, {line}, text, {
      romStart: hex(start), romEndExclusive: hex(end), runtimeAddress: hex(address),
      confidence: 'static-loader-range; execution-not-observed'
    });
    vm.push({start, end, address, entry, file});
    for (const row of byRom)
      if (row._start < end && row._end > start) row.host.push(entry);
  }
  for (const file of ['engine/scene_report_runtime.cpp', 'engine/sound_preview.cpp', 'engine/effect_player.cpp']) {
    const text = fs.readFileSync(path.join(opt.native, file), 'utf8');
    const lineno = offset => 1 + text.slice(0, offset).split('\n').length - 1;
    if (file.endsWith('scene_report_runtime.cpp')) {
      const constants = new Map([...text.matchAll(/\b(shared|report|resident)\s*=\s*(0x[0-9a-f]+)/gi)]
        .map(m => [m[1], Number(m[2])]));
      for (const m of text.matchAll(/\bcode\((shared|report|resident|0x[0-9a-f]+)\s*,\s*(0x[0-9a-f]+)\s*,\s*(0x[0-9a-f]+)\s*\)/gi)) {
        const start = Number(m[2]), bias = constants.get(m[1]) ?? Number(m[1]);
        vmRange(file, lineno(m.index), start, Number(m[3]), bias + start, m[0]);
      }
    }
    if (file.endsWith('sound_preview.cpp'))
      for (const m of text.matchAll(/m\.mount\((0x[0-9a-f]+)\s*,\s*bytes\.mid\((0x[0-9a-f]+)\s*,\s*(0x[0-9a-f]+)\)\s*,\s*true\s*\)/gi))
        vmRange(file, lineno(m.index), Number(m[2]), Number(m[2]) + Number(m[3]), Number(m[1]), m[0]);
    if (file.endsWith('effect_player.cpp'))
      for (const m of text.matchAll(/\bload\((0x[0-9a-f]+)\s*,\s*(0x[0-9a-f]+)\s*,\s*(0x[0-9a-f]+)\s*\)/gi))
        vmRange(file, lineno(m.index), Number(m[2]), Number(m[3]), Number(m[1]), m[0]);
  }
  let romIdentity = null, dynamicOverlayCoverage = 'not-resolved-without-ROM';
  if (opt.rom) {
    const rom = fs.readFileSync(opt.rom);
    if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
    if (rom.readUInt32BE(0) !== 0x80371240) throw Error('Unsupported ROM byte order');
    romIdentity = {bytes: rom.length, normalizedSha256: sha(rom)};
    const file = 'engine/effect_player.cpp', text = fs.readFileSync(path.join(opt.native, file), 'utf8');
    const match = /for\s*\(int id\s*:\s*\{([\d,\s]+)\}\)\s*\{\s*const auto at\s*=\s*(0x[0-9a-f]+)\s*\+\s*id\s*\*\s*(\d+)/i.exec(text);
    if (!match) throw Error('Dynamic EffectPlayer loader changed; update this census');
    const ids = match[1].split(',').map(v => Number(v.trim()));
    for (const id of ids) {
      const at = Number(match[2]) + id * Number(match[3]);
      const address = rom.readUInt32BE(at), start = rom.readUInt32BE(at + 8), end = rom.readUInt32BE(at + 12);
      if (end > rom.length) throw Error('Descriptor range exceeds ROM');
      vmRange(file, text.slice(0, match.index).split('\n').length, start, end, address,
        'Runtime descriptor ' + id + ' from ROM ' + hex(at));
    }
    dynamicOverlayCoverage = 'resolved-from-input-ROM-descriptors';
  }
  for (const file of [...new Set(vm.map(v => v.file))]) {
    const text = fs.readFileSync(path.join(opt.native, file), 'utf8');
    for (const match of text.matchAll(/\.service\(\s*(0x[0-9a-f]+)/gi)) {
      const address = Number(match[1]);
      for (const range of vm.filter(v => v.file === file))
        for (const row of byRom) {
          const entry = range.address + row._start - range.start;
          if (row._start >= range.start && row._end <= range.end && address >= entry && address < entry + row.bytes)
            row.services.push(item('VM-host-service-override', file,
              {line: text.slice(0, match.index).split('\n').length}, match[0],
              {runtimeAddress: hex(address), ownerOffset: address - entry,
                confidence: 'literal service registration; implementation and reachability require feature review'}));
        }
    }
  }
  // Accepted-effect recipes identify explicit compiled VM replacement eligibility.
  for (const owner of effectRecipes.owners) {
    const row = bySymbol.get(norm(owner.symbol));
    if (!row) continue;
    row.host.push(item('compiled-preview-recipe', 'game/accepted/effect-owners.json',
      {symbol: owner.symbol}, owner.source, {runtimeAddress: hex(owner.address),
        confidence: 'recipe-present; runtime-installation-conditional-on-ROM-and-RAM-bytes'}));
  }

  const cmake = fs.readFileSync(path.join(opt.native, 'CMakeLists.txt'), 'utf8');
  const hostSources = new Set();
  for (const block of cmake.matchAll(/(?:add_library|target_sources)\(lordly_engine\b[\s\S]*?\)/g))
    for (const file of block[0].matchAll(/\bgame\/[A-Za-z_0-9/.-]+\.c\b/g)) hostSources.add(file[0]);
  const reusedAddresses = new Map();
  for (const recipe of recipes.sources)
    for (const part of recipe.parts.filter(p => p.code)) {
      const key = hex(part.address);
      if (!reusedAddresses.has(key)) reusedAddresses.set(key, new Set());
      reusedAddresses.get(key).add(recipe.name);
    }
  const featureFor = file => {
    if (file.includes('/consumables/')) return 'Consumable item menus and results';
    if (file.includes('/shops/') || file.includes('shop_runtime')) return 'Shop inventory and item display';
    if (file.includes('/attacks/') || file.includes('/items/') || file.includes('attack_runtime')) return 'Battle attacks, target rules and weapon effects';
    if (file.includes('/art/')) return 'Actor appearance, body and equipment rendering';
    if (file.includes('/director/') || file.includes('scene_')) return 'Story cutscene and briefing playback';
    if (file.includes('/scenarios/') || file.includes('capacity_runtime')) return 'Expanded army, deployment and mission state';
    if (file.includes('/movement/')) return 'Scenario movement and army source lookup';
    if (file.includes('/classes/') || file.includes('promotion_runtime')) return 'Expanded classes, memory layout and resets';
    if (file.includes('sound_preview')) return 'Attack sound audition';
    if (file.includes('/tools/') || file.includes('game_rules')) return 'Game rule editing';
    if (file.includes('battlefield')) return 'Battlefield model inspection';
    if (file.includes('save_runtime')) return 'Save and SRAM export';
    if (file.includes('encounter')) return 'Custom encounters';
    return 'Existing Editor runtime';
  };
  const semanticNames = new Map([
    ['func_000ae30c', 'Push the current item-menu state and enter the requested menu state.'],
    ['func_000b1f00', 'Invoke the original default unit-selection continuation.'],
    ['func_000b4a70', 'Select the original item-menu notification route from menu state.'],
    ['func_000b4b08', 'Create or refresh the item-detail notification window.'],
    ['func_000b6b70', 'Close the item-menu windows and reset its selection state.'],
    ['func_000b8a94', 'Create the item-menu selection window with the current bounds.'],
    ['func_000b8aec', 'Update the existing selection-window callbacks and bounds.'],
    ['func_000b8b60', 'Refresh selection-window callbacks and clipping bounds.'],
    ['func_0015239c', 'Emit the palette-load commands used by shop item rendering.'],
    ['func_00152448', 'Emit the palette-load commands used by item and shop rendering.'],
    ['func_00152bb4', 'Sum equipment stat contributions for the displayed unit.'],
    ['func_0015394c', 'Initialize the item-stat panel drawing state.'],
    ['func_00153a14', 'Draw the current/maximum pair in the item-stat panel.'],
    ['func_00233f94', 'Select an eligible opposing target in the relevant lane.'],
    ['func_002340ec', 'Rank eligible recovery targets using missing-health scoring.'],
    ['func_0023422c', 'Apply a battle class change and refresh equipment, actions and body state.'],
    ['func_00237750', 'Choose the retail physical/DEX/magic score builder before calculating the amount.'],
    ['func_00237810', 'Perform the retail Luck-difference random check.'],
    ['func_00237890', 'Filter inflicted status bits against equipment and class protection.'],
    ['func_002379ac', 'Test a secondary effect against the target protection state.'],
    ['func_00237d5c', 'Resolve encoded actor or grid targets into formation coordinates.'],
    ['func_00239aa4', 'Check whether the weapon secondary effect applies to this target.']
  ]);
  const opcodeRoles = new Map();
  const grammar = new Map(read(path.join(opt.native, 'game/director/grammar.json')).rows.map(r => [r.opcode, r]));
  const directorText = fs.readFileSync(path.join(opt.native, 'game/director/story-logic.c'), 'utf8');
  let opcode = null;
  for (const [line, text] of directorText.split('\n').entries()) {
    const label = /^        case (0x[0-9a-f]+|\d+):/i.exec(text);
    if (label) opcode = Number(label[1]);
    if (opcode === null) continue;
    for (const call of text.matchAll(/\b(func_[0-9a-f]{8})\s*\(/gi)) {
      const row = bySymbol.get(norm(call[1]));
      if (!row) continue;
      if (!opcodeRoles.has(norm(row.symbol))) opcodeRoles.set(norm(row.symbol), []);
      opcodeRoles.get(norm(row.symbol)).push({opcode: hex(opcode),
        meaning: grammar.get(opcode)?.name || 'retail Director operation',
        evidence: item('Director-command-call', 'game/director/story-logic.c', {line: line + 1}, text)});
    }
  }
  const decisions = [
    ['func_00014530', 'Imported attack sound audition', 'native-imported-sound-tempo',
      'The imported 3,200-sample tone reaches this tempo-command entry in the original sequencer. The desktop regression checks its rendered stereo output.',
      [['engine/sound_preview.cpp', 'call(0x8008202c, {nextBank'], ['tests/audio_cues_tests.cpp', 'void stereoPanFromCurrentRom']]],
    ['func_00045e5c', 'Dynamic cast, roster and party identity', 'original-C-called-by-host',
      'Host roster selection calls accepted C directly. Story cast and party equivalence share this predicate.',
      [['game/director/actor_roster.c', 'return func_00045e5c'], ['game/director/story_cast.c', 'lordly_roster_same'], ['engine/scene_party.cpp', 'lordly_roster_same']]],
    ['func_002864f8', 'Story cutscene controller query', 'portable-host-adaptation',
      'Opcode 57 returns the effective pad word for mask zero and uses the full nonzero mask. Input remains per playback instance.',
      [['engine/scene_playback.cpp', 'value = mask ?']]],
    ['func_00016cdc', 'Attack sound audition', 'portable-host-adaptation',
      'Host stereo gains read the current-ROM signed Q15 pan table. Native final mixing and audio hardware remain host services.',
      [['engine/sound_preview.cpp', 'qint16(m.half(0x800a9d40']]],
    ['func_00045f30', 'Expanded army reset', 'extended-C-export-adaptation',
      'Expanded reset clears members, source rows and flagged deployment metadata while preserving the reserved character slot.',
      [['game/scenarios/enemy-capacity.c', 'void lc_enemy_reset_army'], ['game/scenarios/capacity-extra.json', 'lc_capacity_extra_00045f30']]],
    ['func_0004271c', 'Global day and night duration', 'source-built-ROM-adaptation',
      'Retail exports compile the accepted owner. Edited day/night durations compile the C adaptation. Existing authored clock markers remain readable.',
      [['rebuild/compiler.cpp', 'rule_clock'], ['game/tools/clock.c', 'void clock_step']]],
    ['func_002831e0', 'Cutscene audio gain', 'portable-host-adaptation',
      'The silent preview consumes the native two-argument gain command through host audio handles and preserves the complete gain value.',
      [['game/director/scene_audio.c', 'case 0x6f:']]],
    ['func_002834c4', 'Cutscene audio controller', 'portable-host-adaptation',
      'The silent preview preserves controller suppression and per-context/all-context routing through host audio handles.',
      [['game/director/scene_audio.c', 'case 0x70:']]],
    ['func_00283564', 'Cutscene audio time, pan and pitch', 'portable-host-adaptation',
      'The silent preview preserves signed sentinel values and pitch conversion through host audio handles.',
      [['game/director/scene_audio.c', 'case 0x71:']]],
    ['func_00283318', 'Cutscene audio envelope disposal', 'portable-host-adaptation',
      'Envelope lifecycle uses host-owned audio state and handles in the silent preview.',
      [['game/director/scene_audio.c', 's->envelope_present = 0;']]],
    ['func_002a90ec', 'Cutscene camera and projection initialization', 'portable-host-adaptation',
      'Host initialization preserves the retail record layout and default transform values.',
      [['game/director/scene_projection.c', 'void lordly_projection_initialize']]],
    ['func_002a9258', 'Cutscene projection progression', 'portable-host-adaptation',
      'Host progression preserves the projection record, countdown narrowing and float operation order.',
      [['game/director/scene_projection.c', 'int lordly_projection_advance']]],
    ['func_0029b78c', 'Cutscene actor material override', 'portable-host-adaptation',
      'The host record uses the accepted material construction behavior with caller-selected actor data.',
      [['game/director/actor_material.c', 'void lordly_material_begin']]],
    ['func_002adf9c', 'Cutscene actor environment tint', 'portable-host-adaptation',
      'The host renderer applies the accepted environment contribution after actor tint state.',
      [['game/director/actor_render.c', 'int lordly_actor_world_environment']]],
    ['func_000b86c4', 'Save editor Soldier reserve allocation', 'portable-host-adaptation',
      'The host free-ID scan uses the Save document HP pool and preserves native slot boundaries and truncation.',
      [['game/army/soldier_reserve.c', 'int lordly_soldier_free_id']]],
    ['func_0012da9c', 'Expanded army source ownership', 'retained-hook-and-relocation',
      'The tagged source lookup and relocated character/source arrays remain necessary for expanded capacity.',
      [['game/scenarios/capacity-predicates.json', 'lc_capacity_type_0012dac0']]],
    ['func_0015df7c', 'Expanded enemy ownership', 'retained-hook-with-alternate-entry',
      'The alternate entry targets the adapter interior. A whole-owner migration must preserve both entries.',
      [['game/scenarios/capacity-predicates.json', 'lc_capacity_type_0015dfc8']]],
    ['func_00283808', 'Expanded cutscene source lookup', 'retained-hook-and-relocation',
      'Tagged source lookup and relocated army arrays remain necessary for expanded capacity.',
      [['game/scenarios/capacity-extra.json', 'lc_capacity_extra_0028381c']]]
  ];
  const soundRuns = new Map();
  if (opt.rom) {
    const oracle = path.join(opt.native, 'tests/sound-preview-mips.cjs');
    for (const request of [303, 304, 305, 306, 280, 281, 11]) {
      const result = JSON.parse(cp.execFileSync(process.execPath,
        [oracle, path.resolve(opt.rom), String(request), '--executed-rom-words'], {encoding: 'utf8'}));
      if (result.limited || !result.voices) throw Error('Incomplete audio replay for request ' + request);
      const words = new Set(result.executedRomWords);
      for (const row of byRom) {
        if (!words.has(row._start)) continue;
        if (!soundRuns.has(row.symbol)) soundRuns.set(row.symbol, []);
        soundRuns.get(row.symbol).push(request);
      }
    }
  }
  for (const row of byRom) {
    row.consumers = [];
    if (soundRuns.has(row.symbol)) {
      row.consumers.push({feature: 'Attack sound audition', kind: 'native-audio-entry-observed',
        reason: 'Original-instruction sequencer entered this function for sound request(s) ' + soundRuns.get(row.symbol).join(', ') + '.',
        evidence: [item('original-instruction sound replay', 'tests/sound-preview-mips.cjs', {},
          'Primary entry observed in the resident sound engine or game request queue. Hardware synthesis is a host service.')]});
    }
    row.referenceKinds = [];
    for (const alias of row.aliases) {
      alias.scope = /descriptor_(\d+)/.exec(row.source)?.[0] || 'accepted physical ROM owner ' + row.romStart;
      alias.otherLinkedOwnersAtSameAddress = [...(reusedAddresses.get(alias.address) || [])]
        .filter(name => norm(name) !== norm(row.romRecipe || row.symbol));
      alias.confidence = 'qualified placement; callers still require matching overlay context';
    }
    for (const id of row.references) {
      const ev = evidence[id], text = ev.text, file = ev.file;
      const tokens = [...text.matchAll(token)].map(m => tokenKey(m[0]));
      const exactSymbol = tokens.includes(norm(row.symbol));
      const romLiteral = tokens.includes(tokenKey(row.romStart));
      const runtimeSymbol = [...row._aliases].filter(a => /^func_8/.test(a)).some(a => tokens.includes(a));
      let kind = 'supporting-source-declaration';
      let reason = 'Retained declaration or source context; the call is classified separately.';
      if (file.endsWith('.json')) {
        kind = file.includes('event-semantics') ? 'event-semantic-metadata' :
          /"name"\s*:/.test(text) ? 'catalog-owner-record' : 'export-link-binding';
        reason = kind === 'event-semantic-metadata' ?
          'The Logic model uses the original helper identity when describing the retail event instruction.' :
          kind === 'catalog-owner-record' ? 'The catalog names this owner. Its individual relocation or hook is classified separately.' :
          'The exporter binds an adapted game-code call to the retail function or resource address.';
      } else if (file.endsWith('.s')) {
        kind = 'runtime-instruction-fixup';
        reason = 'High Attack retains an authenticated runtime operand fixup in this owner.';
      } else if (file === 'engine/battlefield.cpp') {
        kind = 'inspector-address-range-guard';
        reason = 'The literal selects a geometry ROM address space and does not establish a function call.';
      } else if (norm(row.symbol) === 'func_00213b10' && !exactSymbol && !romLiteral) {
        kind = 'different-overlay-address-coincidence';
        reason = 'Encounter construction uses the same RAM entry in another overlay. This does not call the battle-state reset.';
      } else if (exactSymbol && ['memset','memmove','strcmp','strcpy'].includes(norm(row.symbol)) &&
                 (hostSources.has(file) || file.startsWith('engine/') || file.startsWith('rebuild/') || file.startsWith('app/'))) {
        kind = 'host-standard-library-name';
        reason = 'The PC build resolves this name in the host C library. No call to the N64 owner is established.';
      } else if ((exactSymbol || runtimeSymbol) && /\(/.test(text)) {
        const declaration = /^\s*(?:extern\s+)?(?:void|int|float|double|s32|u32|u8|u16|s16|uint\d+_t|M2C_UNK)\s+\**\s*func_/i.test(text);
        if (!declaration && file.startsWith('game/') && file.endsWith('.c')) {
          kind = /^game\/director\/func_/.test(file) ? 'retained-source-context' :
            hostSources.has(file) ? 'host-adapter-direct-call' : 'exported-direct-call';
          reason = semanticNames.get(norm(row.symbol)) || 'The adapted game-code caller continues to invoke this exact retail helper.';
          if (runtimeSymbol && !exactSymbol) reason += ' The catalog and caller use the shown relocated symbol alias.';
        }
      } else if ((file.startsWith('engine/') || file.startsWith('rebuild/')) && /\.(?:cpp|h)$/.test(file)) {
        kind = /\.service\(/.test(text) ? 'host-native-service-registration' :
          /\b(?:call|[A-Za-z_]+\.call)\(/.test(text) ? 'host-native-call' :
          file.startsWith('rebuild/') && /\{"[A-Za-z_]+",\s*0x/i.test(text) ? 'export-native-function-binding' :
          romLiteral ? 'retail-owner-validation-or-inspection' : 'runtime-address-reference';
        reason = 'The source preserves this address at its explicit native or inspection boundary. VM eligibility is independent.';
      }
      row.referenceKinds.push({evidence: id, kind, reason});
      if (!['supporting-source-declaration', 'retained-source-context', 'different-overlay-address-coincidence',
        'runtime-address-reference', 'inspector-address-range-guard', 'catalog-owner-record', 'host-standard-library-name'].includes(kind))
        row.consumers.push({feature: featureFor(file), kind, reason, evidence: [id]});
    }
    for (const id of row.extension) {
      const ev = evidence[id];
      row.consumers.push({feature: featureFor(ev.file), kind: 'export-hook-or-data-relocation',
        reason: ev.reason, evidence: [id]});
    }
    for (const role of opcodeRoles.get(norm(row.symbol)) || [])
      row.consumers.push({feature: 'Story cutscene and briefing commands', kind: 'exported-Director-command-call',
        reason: role.opcode + ' ' + role.meaning, evidence: [role.evidence]});
    for (const [symbol, feature, kind, reason, anchors] of decisions.filter(d => d[0] === norm(row.symbol))) {
      const refs = [];
      for (const [file, fragment] of anchors) {
        const disk = path.join(opt.native, file);
        if (!fs.existsSync(disk)) continue;
        const lines = fs.readFileSync(disk, 'utf8').split('\n');
        const line = lines.findIndex(l => l.includes(fragment));
        if (line !== -1) refs.push(item('reviewed-feature-consumer', file, {line: line + 1}, lines[line]));
      }
      if (!refs.length) throw Error('Reviewed feature anchor disappeared: ' + symbol);
      row.consumers.push({feature, kind, reason, evidence: refs});
    }
    const consolidated = new Map();
    for (const consumer of row.consumers) {
      const key = JSON.stringify([consumer.feature, consumer.kind, consumer.reason]);
      if (!consolidated.has(key)) consolidated.set(key, {...consumer, evidence: []});
      consolidated.get(key).evidence.push(...consumer.evidence);
    }
    row.consumers = [...consolidated.values()].map(c => ({...c, evidence: [...new Set(c.evidence)]}));
    row.previewEligible = row.host.length > 0;
    row.featureDisposition = row.consumers.length ? 'separate-editor-consumer-identified' :
      'retained-baseline-no-separate-editor-consumer-identified';
    row.features = [...new Set(row.consumers.map(c => c.feature))];
    row.consumerFiles = [...new Set(row.consumers.flatMap(c => c.evidence.map(id => {
      const ev = evidence[id];
      return ev.file + (ev.line ? ':' + ev.line : ev.jsonPath ? '#' + ev.jsonPath : '');
    })))];
  }

  for (const row of byRom) {
    for (const field of ['host', 'services', 'extension', 'references', 'context'])
      row[field] = [...new Set(row[field])].sort((a, b) => a - b);
    row.disposition = row.featureDisposition;
    delete row._start;
    delete row._end;
    delete row._aliases;
  }
  const count = field => Object.fromEntries([...new Set(byRom.map(r => r[field]))]
    .map(key => [key, byRom.filter(r => r[field] === key).length]));
  const result = {schemaVersion: 2, generator: 'native/tools/accepted-c-coverage.cjs',
    inputs: {base: opt.base, head: opt.head, libraryRevision: imports.revision,
      libraryFingerprint: imports.fingerprint, romRecipeRevision: recipes.revision,
      romRecipeFingerprint: recipes.fingerprint, rom: romIdentity},
    scope: 'Every newly registered owner. The inventory links ROM recipes, source and catalog references, VM ranges, and separately identified documentation and tests. When a ROM is supplied, sound replays also record the original instructions executed for the tested requests.',
    limitations: ['ROM baseline registration is a recipe, with execution verified separately.',
      'A VM range identifies loaded instructions and may contain service replacements or unreachable paths.',
      'Sound execution observations apply only to the tested requests. Other static references and VM ranges do not establish execution or semantic parity.',
      'Missing direct references require feature review before claiming no present consumer.',
      'Unrecognized dynamic loaders and address arithmetic require manual review.',
      'C library names shared with host stdlib are lexical evidence requiring semantic review.',
      'Literal VM service entries are recorded separately. Dynamic service registration requires manual review.',
      'Original owners plus explicit logical-function refinements resolve target bounds. Shared-source aliases use authenticated import/ROM manifests.'],
    dynamicOverlayCoverage, counts: {targets: byRom.length, baseline: count('baseline'), disposition: count('disposition'),
      previewEligible: byRom.filter(r => r.previewEligible).length,
      serviceOverrides: byRom.filter(r => r.services.length).length,
      consumerKinds: Object.fromEntries([...new Set(byRom.flatMap(r => r.consumers.map(c => c.kind)))]
        .map(kind => [kind, byRom.filter(r => r.consumers.some(c => c.kind === kind)).length]))},
    evidence, targets: byRom};
  const json = JSON.stringify(result, null, 2) + '\n';
  if (opt.output) fs.writeFileSync(opt.output, json);
  else process.stdout.write(json);
  if (opt.table) {
    const fields = ['symbol', 'romStart', 'bytes', 'source', 'sourceClass', 'baseline', 'disposition',
      'previewEligible', 'features', 'consumerKinds', 'consumerFiles', 'consumerReasons',
      'VMServiceOverrides', 'interestGroups', 'excludedReferenceKinds', 'referenceFiles'];
    const rows = byRom.map(row => [row.symbol, row.romStart, row.bytes, row.source, row.sourceClass,
      row.baseline, row.disposition, row.previewEligible, row.features.join('; '),
      [...new Set(row.consumers.map(c => c.kind))].join('; '), row.consumerFiles.join('; '),
      (row.consumers.length ? [...new Set(row.consumers.map(c => c.reason))].join(' | ') :
        row.referenceKinds.length ? [...new Set(row.referenceKinds.map(r => r.reason))].join(' | ') :
        'Retain the exact game baseline. No separate current Editor consumer was identified; preview range eligibility is shown independently.').replace(/[\t\r\n]+/g, ' '),
      row.services.length, row.interestGroups.map(g => g.id).join(','),
      [...new Set(row.referenceKinds.filter(r => !row.consumers.some(c => c.evidence.includes(r.evidence))).map(r => r.kind))].join('; '),
      [...new Set(row.references.map(id => evidence[id].file + ':' + evidence[id].line))].join('; ')]);
    fs.writeFileSync(opt.table, [fields, ...rows].map(row => row.map(field => field === '' ? '-' : field).join('\t')).join('\n') + '\n');
  }
  if (opt.output) console.log(JSON.stringify(result.counts));
}
try { main(); } catch (error) { console.error(error.message); process.exitCode = 1; }
