// OB64 Mod Editor - human-readable changelog reports
//
// The project JSON remains the canonical machine-readable diff. This module
// translates that same diff into a plain-English report for mod releases.

window.OB64 = window.OB64 || {};

(function() {
  var ITEM_FIELD_LABELS = {
    equipType: 'Equipment type', element: 'Element', grade: 'Grade',
    str: 'STR modifier', int: 'INT modifier', agi: 'AGI modifier',
    dex: 'DEX modifier', vit: 'VIT modifier', men: 'MEN modifier',
    resPhys: 'Physical resistance', resWind: 'Wind resistance',
    resFire: 'Fire resistance', resEarth: 'Earth resistance',
    resWater: 'Water resistance', resVirtue: 'Virtue resistance',
    resBane: 'Bane resistance',
    growthHpStr: 'HP/STR growth', growthUnknown: 'Unknown growth lane',
    growthInt: 'INT growth', growthAgi: 'AGI growth',
    growthDex: 'DEX growth', growthVit: 'VIT growth',
    growthMen: 'MEN growth', growthLck: 'LCK growth',
    b3Raw: 'Raw byte B3', b12Raw: 'Raw byte B12',
    b22Raw: 'Raw byte B22', b23Raw: 'Raw byte B23',
    b24Raw: 'Raw byte B24', b25Raw: 'Raw byte B25',
    b26Raw: 'Raw byte B26', b27Raw: 'Raw byte B27',
    b28Raw: 'Raw byte B28', b29Raw: 'Raw byte B29',
    b30Raw: 'Raw byte B30', b31Raw: 'Raw byte B31'
  };

  var STAT_GATE_LABELS = {
    str: 'STR', vit: 'VIT', int: 'INT', men: 'MEN',
    agi: 'AGI', dex: 'DEX', alnMin: 'Minimum alignment',
    alnMax: 'Maximum alignment'
  };

  var CLASS_STATS = ['STR', 'VIT', 'INT', 'MEN', 'AGI', 'DEX'];
  var CLASS_RESISTANCES = [
    'Physical resistance', 'Wind resistance', 'Fire resistance',
    'Earth resistance', 'Water resistance', 'Virtue resistance',
    'Bane resistance'
  ];

  function own(obj, key) {
    return !!obj && Object.prototype.hasOwnProperty.call(obj, key);
  }

  function numericKeys(obj) {
    return Object.keys(obj || {}).sort(function(a, b) {
      var an = Number(a);
      var bn = Number(b);
      if (isFinite(an) && isFinite(bn)) return an - bn;
      return String(a).localeCompare(String(b));
    });
  }

  function unique(values) {
    var out = [];
    for (var i = 0; i < values.length; i++) {
      if (out.indexOf(values[i]) === -1) out.push(values[i]);
    }
    return out;
  }

  function difference(values, other) {
    values = values || [];
    other = other || [];
    return values.filter(function(value) { return other.indexOf(value) === -1; });
  }

  function arraysEqual(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function hex(value, width) {
    var n = Number(value);
    if (!isFinite(n)) return String(value);
    return '0x' + (n >>> 0).toString(16).toUpperCase().padStart(width || 2, '0');
  }

  function scalar(value) {
    if (value === null || value === undefined) return 'not set';
    if (typeof value === 'boolean') return value ? 'enabled' : 'disabled';
    if (Array.isArray(value)) return value.join(', ');
    return String(value);
  }

  function changed(before, after) {
    return scalar(before) + ' -> ' + scalar(after);
  }

  function className(id) {
    var n = Number(id) || 0;
    return (OB64.className ? OB64.className(n) :
      ((OB64.CLASS_NAMES && OB64.CLASS_NAMES[n]) || ('Class ' + hex(n, 2)))) +
      ' (' + hex(n, 2) + ')';
  }

  function itemName(id) {
    var n = Number(id) || 0;
    return (OB64.itemName ? OB64.itemName(n) :
      ((OB64.ITEM_NAMES && OB64.ITEM_NAMES[n]) || ('Item ' + hex(n, 4)))) +
      ' (' + hex(n, 4) + ')';
  }

  function consumableName(rom, id) {
    var n = Number(id) || 0;
    var rec = rom && rom.consumables && rom.consumables[n];
    var name = rec && rec.name;
    if (!name && OB64.consumableName) name = OB64.consumableName(n);
    return (name || ('Consumable ' + n)) + ' (#' + n + ')';
  }

  function scenarioLabel(runtimeKey) {
    var key = Number(runtimeKey);
    var info = OB64.scenarioKeyInfo ? OB64.scenarioKeyInfo(key) : null;
    return 'Scenario key ' + key + (info && info.label ? ' - ' + info.label : '');
  }

  function scenarioRecord(runtimeKey) {
    var scenarios = OB64.SCENARIO_ESET_DATA && OB64.SCENARIO_ESET_DATA.scenarios;
    if (!scenarios) return null;
    for (var i = 0; i < scenarios.length; i++) {
      if (Number(scenarios[i].runtimeKey) === Number(runtimeKey)) return scenarios[i];
    }
    return null;
  }

  function siteName(runtimeKey, selector) {
    var scenario = scenarioRecord(runtimeKey);
    var sites = scenario && scenario.sites || [];
    for (var i = 0; i < sites.length; i++) {
      if (Number(sites[i].selector) === Number(selector)) {
        return sites[i].siteName || ('Site selector ' + selector);
      }
    }
    return 'Site selector ' + selector;
  }

  function shopName(rom, index) {
    var names = [];
    var strongholds = rom && rom.strongholds || [];
    for (var i = 0; i < strongholds.length; i++) {
      var rec = strongholds[i];
      if (rec.shopIdx !== Number(index) || rec.isObjective || rec.missionId === 1) continue;
      if (rec.name) names.push(rec.name);
    }
    names = unique(names);
    return 'Shop #' + index + (names.length ? ' - ' + names.join(', ') : '');
  }

  function listNames(values, formatter) {
    if (!values || !values.length) return 'none';
    return values.map(formatter).join(', ');
  }

  function addSection(sections, title, entries) {
    if (!entries.length) return;
    sections.push({ title: title, count: entries.length, entries: entries });
  }

  function buildShopSection(rom, patches, sections) {
    var entries = [];
    numericKeys(patches.shops).forEach(function(key) {
      var index = Number(key);
      var current = patches.shops[key] || {};
      var original = rom.original && rom.original.shops && rom.original.shops[index] || {};
      var beforeItems = original.items || [];
      var afterItems = current.items || [];
      var beforeConsumables = original.consumables || [];
      var afterConsumables = current.consumables || [];
      var lines = [];
      var addedItems = difference(afterItems, beforeItems);
      var removedItems = difference(beforeItems, afterItems);
      var addedConsumables = difference(afterConsumables, beforeConsumables);
      var removedConsumables = difference(beforeConsumables, afterConsumables);
      if (addedItems.length) lines.push('Added equipment: ' + listNames(addedItems, itemName));
      if (removedItems.length) lines.push('Removed equipment: ' + listNames(removedItems, itemName));
      if (!addedItems.length && !removedItems.length && !arraysEqual(beforeItems, afterItems)) {
        lines.push('Equipment order changed to: ' + listNames(afterItems, itemName));
      }
      if (addedConsumables.length) {
        lines.push('Added expendables: ' + listNames(addedConsumables, function(id) {
          return consumableName(rom, id);
        }));
      }
      if (removedConsumables.length) {
        lines.push('Removed expendables: ' + listNames(removedConsumables, function(id) {
          return consumableName(rom, id);
        }));
      }
      if (!addedConsumables.length && !removedConsumables.length &&
          !arraysEqual(beforeConsumables, afterConsumables)) {
        lines.push('Expendable order changed to: ' + listNames(afterConsumables, function(id) {
          return consumableName(rom, id);
        }));
      }
      entries.push({ title: shopName(rom, index), lines: lines });
    });
    addSection(sections, 'Shops', entries);
  }

  function buildItemSection(rom, patches, sections) {
    var entries = [];
    var keys = unique(numericKeys(patches.item_prices).concat(numericKeys(patches.items))).sort(function(a, b) {
      return Number(a) - Number(b);
    });
    keys.forEach(function(key) {
      var id = Number(key);
      var lines = [];
      var original = rom.original && rom.original.itemStats && rom.original.itemStats[id] || {};
      if (own(patches.item_prices, key)) {
        var beforePrice = rom.original && rom.original.itemPrices && rom.original.itemPrices[id];
        lines.push('Price: ' + changed(beforePrice, patches.item_prices[key]) + ' Goth');
      }
      var itemPatch = patches.items && patches.items[key] || {};
      Object.keys(itemPatch).sort().forEach(function(field) {
        lines.push((ITEM_FIELD_LABELS[field] || field) + ': ' + changed(original[field], itemPatch[field]));
      });
      entries.push({ title: itemName(id), lines: lines });
    });
    addSection(sections, 'Items and Equipment', entries);
  }

  function classByteLabel(offset) {
    var off = Number(offset);
    if (off >= 0 && off <= 23) {
      var stat = CLASS_STATS[Math.floor(off / 4)];
      return stat + ' ' + ['base high byte', 'base low byte', 'growth', 'secondary growth/raw'][off % 4];
    }
    if (off === 24) return 'Alignment';
    if (off >= 25 && off <= 31) return CLASS_RESISTANCES[off - 25];
    var labels = {
      32: 'Movement type', 33: 'Raw byte B33',
      34: 'Default equipment 1 high byte', 35: 'Default equipment 1 low byte',
      36: 'Default equipment 2 high byte', 37: 'Default equipment 2 low byte',
      38: 'Default equipment 3 high byte', 39: 'Default equipment 3 low byte',
      40: 'Default equipment 4 high byte', 41: 'Default equipment 4 low byte',
      42: 'Raw byte B42', 43: 'Front-row attack', 44: 'Front-row attack count',
      45: 'Middle-row attack', 46: 'Middle-row attack count',
      47: 'Rear-row attack', 48: 'Rear-row attack count',
      49: 'Physical attack', 50: 'Magic attack', 51: 'Physical defense',
      52: 'Magic defense', 53: 'Base class', 54: 'Base transition level',
      55: 'Intermediate class', 56: 'Final transition level',
      57: 'Class-copy match', 58: 'Dragon element', 59: 'Item capacity',
      60: 'Name pointer byte 1', 61: 'Name pointer byte 2',
      62: 'Name pointer byte 3', 63: 'Name pointer byte 4',
      64: 'Unit size', 65: 'Sex/voice/body code', 66: 'Leadership',
      67: 'Raw header byte B67', 68: 'Base HP high byte',
      69: 'Base HP low byte', 70: 'HP growth', 71: 'Raw header byte B71'
    };
    return labels[off] || ('Class byte B' + off);
  }

  function buildClassSection(rom, patches, sections) {
    var entries = [];
    numericKeys(patches.classDefs).forEach(function(key) {
      var entry = patches.classDefs[key] || {};
      var recIndex = Number.isInteger(entry.record_index) ? entry.record_index : Number(key) + 1;
      var original = rom.original && rom.original.classDefBytes && rom.original.classDefBytes[recIndex] || {};
      var lines = [];
      numericKeys(entry.bytes).forEach(function(byteKey) {
        var before = original[byteKey];
        var after = entry.bytes[byteKey];
        lines.push(classByteLabel(byteKey) + ': ' + scalar(before) + ' (' + hex(before, 2) + ') -> ' +
          scalar(after) + ' (' + hex(after, 2) + ')');
      });
      entries.push({ title: className(key), lines: lines });
    });
    addSection(sections, 'Classes', entries);
  }

  function classPair(value) {
    var pair = Array.isArray(value) ? value : [value && value.classA, value && value.classB];
    var values = [];
    if (Number(pair[0])) values.push(className(pair[0]));
    if (Number(pair[1])) values.push(className(pair[1]));
    return values.length ? values.join(' / ') : 'Empty';
  }

  function terrainTarget(rawLookup) {
    var raw = Number(rawLookup) || 0;
    if (!raw) return 'Disabled';
    var slot = raw - 1;
    return (OB64.TERRAIN_NAMES && OB64.TERRAIN_NAMES[slot] || ('Encounter slot ' + slot)) +
      ' (' + hex(raw, 2) + ')';
  }

  function buildEncounterSection(rom, patches, sections) {
    var entries = [];
    var neutral = patches.neutral_encounters || {};
    numericKeys(neutral.scenario_slices).forEach(function(key) {
      var patch = neutral.scenario_slices[key] || {};
      var originalRecords = rom.original && rom.original.neutralEncounters &&
        rom.original.neutralEncounters.records || [];
      var original = null;
      for (var i = 0; i < originalRecords.length; i++) {
        if (Number(originalRecords[i].s0) === Number(key)) original = originalRecords[i];
      }
      var lines = [];
      numericKeys(patch.slots).forEach(function(slotKey) {
        var before = original && original.slots && original.slots[Number(slotKey)];
        var after = patch.slots[slotKey];
        lines.push('Terrain slot ' + (Number(slotKey) + 1) + ': ' + classPair(before) + ' -> ' + classPair(after));
      });
      entries.push({ title: 'Neutral encounter slice ' + key, lines: lines });
    });
    numericKeys(neutral.terrain_rates).forEach(function(key) {
      var after = neutral.terrain_rates[key] || {};
      var originalRates = rom.original && rom.original.neutralEncounters &&
        rom.original.neutralEncounters.terrainRates || [];
      var before = null;
      for (var i = 0; i < originalRates.length; i++) {
        if (Number(originalRates[i].terrainByte) === Number(key)) before = originalRates[i];
      }
      var lines = [];
      if (own(after, 'rate')) lines.push('Encounter rate: ' + changed(before && before.rate, after.rate));
      if (own(after, 'rawLookup')) {
        lines.push('Encounter table: ' + terrainTarget(before && before.rawLookup) + ' -> ' +
          terrainTarget(after.rawLookup));
      }
      entries.push({ title: 'Terrain rule ' + hex(key, 2), lines: lines });
    });
    if (patches.neutral_global_rate) {
      var originalRate = rom.original && rom.original.neutralGlobalRate || {};
      var afterRate = patches.neutral_global_rate;
      var lines = [];
      lines.push('Global roll multiplier: x' + scalar(afterRate.multiplier));
      if (own(afterRate, 'percent')) {
        var beforePercent = own(originalRate, 'microBasisPoints')
          ? Number(originalRate.microBasisPoints) / 10000
          : null;
        lines.push('Per-check chance: ' + (beforePercent == null ? 'retail baseline' : beforePercent.toFixed(4) + '%') +
          ' -> ' + Number(afterRate.percent).toFixed(4) + '%');
      }
      entries.push({ title: 'Global neutral encounter roll', lines: lines });
    }
    addSection(sections, 'Neutral Encounters', entries);
  }

  function dropName(raw) {
    var value = Number(raw) || 0;
    if (!value) return 'None';
    var id = value & 0x7FFF;
    return (value & 0x8000) ? itemName(id) : ('Consumable #' + id);
  }

  function buildDropSection(rom, patches, sections) {
    var entries = [];
    numericKeys(patches.creatureDrops).forEach(function(key) {
      var after = patches.creatureDrops[key] || {};
      var index = Number.isInteger(after.record_index) ? after.record_index : Number(key);
      var before = rom.original && rom.original.creatureDrops && rom.original.creatureDrops[index] || {};
      var lines = [];
      if (before.classId !== after.classId) {
        lines.push('Creature class: ' + className(before.classId) + ' -> ' + className(after.classId));
      }
      var beforeSlots = before.slots || [];
      var afterSlots = after.slots || [];
      for (var i = 0; i < Math.max(beforeSlots.length, afterSlots.length); i++) {
        if (beforeSlots[i] !== afterSlots[i]) {
          lines.push('Drop slot ' + (i + 1) + ': ' + dropName(beforeSlots[i]) + ' -> ' + dropName(afterSlots[i]));
        }
      }
      if (before.padByte !== after.padByte) {
        lines.push('Raw padding byte: ' + changed(before.padByte, after.padByte));
      }
      entries.push({ title: 'Creature drop record #' + index, lines: lines });
    });
    addSection(sections, 'Creature Drops', entries);
  }

  function buildConsumableSection(rom, patches, sections) {
    var entries = [];
    numericKeys(patches.consumables).forEach(function(key) {
      var index = Number(key);
      var after = patches.consumables[key] || {};
      var before = rom.original && rom.original.consumables && rom.original.consumables[index] || {};
      var lines = [];
      if (before.price !== after.price) lines.push('Price: ' + changed(before.price, after.price) + ' Goth');
      if (before.flagHi !== after.flagHi) {
        lines.push('Category/effect flags: ' + hex(before.flagHi, 4) + ' -> ' + hex(after.flagHi, 4));
      }
      if (!arraysEqual(before.flagLo || [], after.flagLo || [])) {
        lines.push('Behavior bytes: ' + scalar(before.flagLo || []) + ' -> ' + scalar(after.flagLo || []));
      }
      entries.push({ title: consumableName(rom, index), lines: lines });
    });
    addSection(sections, 'Consumables', entries);
  }

  function signedNumber(value) {
    return Number(value) > 0 ? '+' + value : String(value);
  }

  function buildConsumableEffectSection(rom, patches, sections) {
    var effects = patches.consumableEffects || {};
    var entries = [];
    var definitions = {
      '10': { ids: [10], retail: [5, 10], title: consumableName(rom, 10) },
      '11-16': {
        ids: [11, 12, 13, 14, 15, 16],
        retail: [2, 4],
        title: 'Shared Stat Boosters (IDs 11\u201316)',
        targets: ['STR (C+0x1C)', 'VIT (C+0x1E)', 'INT (C+0x20)',
          'MEN (C+0x22)', 'AGI (C+0x24)', 'DEX (C+0x26)']
      },
      '17': { ids: [17], retail: [1, 3], title: consumableName(rom, 17) },
      '18': { ids: [18], retail: [-3, -1], title: consumableName(rom, 18) },
      '19': { ids: [19], retail: [-1, 1], title: consumableName(rom, 19) }
    };
    ['10', '11-16', '17', '18', '19'].forEach(function(key) {
      if (!own(effects, key)) return;
      var value = effects[key];
      var def = definitions[key];
      var width = value.deltaMax - value.deltaMin + 1;
      var lines = [
        'Effect range: ' + signedNumber(def.retail[0]) + '..' +
          signedNumber(def.retail[1]) + ' -> ' +
          signedNumber(value.deltaMin) + '..' + signedNumber(value.deltaMax) +
          ' inclusive (width ' + width + ').'
      ];
      if (key === '11-16') {
        lines.push('Shared items/targets: ' + def.ids.map(function(id, index) {
          return consumableName(rom, id) + ' -> ' + def.targets[index];
        }).join('; ') + '.');
        lines.push('All six items use one canonical range and one encoded word pair.');
      }
      entries.push({ title: def.title, lines: lines });
    });
    addSection(sections, 'Consumable Effects', entries);
  }

  function buildStatGateSection(rom, patches, sections) {
    var entries = [];
    numericKeys(patches.statGates).forEach(function(key) {
      var after = patches.statGates[key] || {};
      var before = rom.original && rom.original.statGates && rom.original.statGates[key] || {};
      var lines = [];
      Object.keys(after).sort().forEach(function(field) {
        lines.push((STAT_GATE_LABELS[field] || field) + ': ' + changed(before[field], after[field]));
      });
      entries.push({ title: className(key), lines: lines });
    });
    addSection(sections, 'Class-Change Requirements', entries);
  }

  function buildToolSection(patches, sections) {
    var entries = [];
    var features = OB64.tools && OB64.tools.features ? OB64.tools.features() : (OB64.TOOLS_FEATURES || []);
    function featureName(id) {
      for (var i = 0; i < features.length; i++) if (features[i].id === id) return features[i].name;
      return id;
    }
    Object.keys(patches.tools || {}).sort().forEach(function(id) {
      entries.push({
        title: featureName(id),
        lines: [patches.tools[id] ? 'Enabled.' : 'Disabled.']
      });
    });
    addSection(sections, 'Optional Tools and Quality-of-Life Features', entries);
  }

  function hexBytes(text) {
    if (typeof text !== 'string') return null;
    var clean = text.replace(/[^0-9a-f]/gi, '');
    if (!clean.length || clean.length % 2) return null;
    var out = [];
    for (var i = 0; i < clean.length; i += 2) out.push(parseInt(clean.slice(i, i + 2), 16));
    return out;
  }

  function squadSummary(bytes) {
    if (!bytes || bytes.length < 25) return 'Composition record changed';
    var parts = [];
    if (bytes[0]) parts.push('leader ' + className(bytes[0]));
    var countB = [13, 14, 15].filter(function(index) { return !!bytes[index]; }).length;
    var countC = [22, 23, 24].filter(function(index) { return !!bytes[index]; }).length;
    if (countB) parts.push(countB + 'x ' + className(bytes[7]));
    if (countC) parts.push(countC + 'x ' + className(bytes[16]));
    return parts.length ? parts.join(', ') : 'Empty composition';
  }

  function changedByteCount(before, after) {
    if (!before || !after) return null;
    var count = 0;
    for (var i = 0; i < Math.max(before.length, after.length); i++) {
      if (before[i] !== after[i]) count++;
    }
    return count;
  }

  function buildSquadSection(patches, sections) {
    var entries = [];
    Object.keys(patches.squadOverrides || {}).sort().forEach(function(key) {
      var entry = patches.squadOverrides[key] || {};
      var after = hexBytes(entry.record || entry);
      var before = hexBytes(entry.original);
      var lines = [];
      if (before) lines.push('Before: ' + squadSummary(before));
      lines.push('After: ' + squadSummary(after));
      var byteCount = changedByteCount(before, after);
      if (byteCount != null) lines.push(byteCount + ' of 35 composition bytes changed.');
      entries.push({
        title: scenarioLabel(entry.scenario_id) + ', squad EDAT #' + entry.edat_id,
        lines: lines
      });
    });
    addSection(sections, 'Enemy Squads', entries);
  }

  function buildScenarioSection(rom, patches, sections) {
    var project = patches.scenario;
    if (!project) return;
    var entries = [];
    numericKeys(project.modifiedEsets).forEach(function(key) {
      var entry = project.modifiedEsets[key] || {};
      entries.push({
        title: scenarioLabel(key),
        lines: ['Mission squads, placements, routes, triggers, or behaviors changed' +
          (entry.filename ? ' in ' + entry.filename : '') + '.']
      });
    });
    numericKeys(project.modifiedTreasures).forEach(function(key) {
      var entry = project.modifiedTreasures[key] || {};
      entries.push({
        title: 'Treasure archive #' + key,
        lines: ['Treasure rewards, positions, additions, or removals changed' +
          (entry.filename ? ' in ' + entry.filename : '') + '.']
      });
    });
    numericKeys(project.siteAllegiances).forEach(function(key) {
      var intents = project.siteAllegiances[key] || {};
      numericKeys(intents).forEach(function(selector) {
        entries.push({
          title: scenarioLabel(key) + ' - ' + siteName(key, selector),
          lines: ['Initial allegiance set to ' + scalar(intents[selector]) + '.']
        });
      });
    });
    numericKeys(project.strongholdFields).forEach(function(key) {
      var recordIndex = Number(key);
      var edit = project.strongholdFields[key] || {};
      var record = rom.strongholds && rom.strongholds[recordIndex] || {};
      var lines = [];
      if (own(edit, 'population')) lines.push('Population: ' + changed(record.population, edit.population));
      if (own(edit, 'morale')) lines.push('Morale: ' + changed(record.morale, edit.morale));
      entries.push({ title: record.name || ('Stronghold record #' + recordIndex), lines: lines });
    });
    (project.addedSquads || []).forEach(function(added) {
      var lines = [
        'Added map squad using source #' + scalar(added.sourceId) +
        ' and EDAT #' + scalar(added.edatId) + '.'
      ];
      var comp = hexBytes(added.compRecHex);
      if (comp) lines.push('Composition: ' + squadSummary(comp) + '.');
      entries.push({ title: scenarioLabel(added.runtimeKey), lines: lines });
    });
    addSection(sections, 'Scenarios', entries);
  }

  function isEmptyValue(value) {
    if (value === null || value === undefined) return true;
    if (Array.isArray(value)) return value.length === 0;
    if (typeof value === 'object') return Object.keys(value).length === 0;
    return false;
  }

  function friendlyKey(key) {
    return String(key)
      .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
      .replace(/_/g, ' ')
      .replace(/^./, function(letter) { return letter.toUpperCase(); });
  }

  function genericLines(value, prefix, lines, depth) {
    if (depth > 4) {
      lines.push(prefix + ': detailed project data changed.');
      return;
    }
    if (value === null || typeof value !== 'object') {
      lines.push(prefix + ': ' + scalar(value));
      return;
    }
    if (Array.isArray(value)) {
      lines.push(prefix + ': ' + value.length + ' entr' + (value.length === 1 ? 'y' : 'ies') + ' changed.');
      return;
    }
    Object.keys(value).sort().forEach(function(key) {
      var next = prefix ? prefix + ' / ' + friendlyKey(key) : friendlyKey(key);
      genericLines(value[key], next, lines, depth + 1);
    });
  }

  function addUnknownSections(patches, sections) {
    var handled = {
      shops: true, item_prices: true, items: true, classDefs: true,
      neutral_encounters: true, creatureDrops: true, consumables: true,
      statGates: true, neutral_global_rate: true, tools: true,
      squadOverrides: true, scenario: true, consumableEffects: true, enemies: true
    };
    Object.keys(patches).sort().forEach(function(key) {
      if (handled[key] || isEmptyValue(patches[key])) return;
      var lines = [];
      genericLines(patches[key], friendlyKey(key), lines, 0);
      addSection(sections, friendlyKey(key), [{ title: 'Additional project changes', lines: lines }]);
    });
  }

  function totalFromSummary(summary) {
    var total = 0;
    Object.keys(summary || {}).forEach(function(key) {
      var value = Number(summary[key]);
      if (isFinite(value)) total += value;
    });
    return total;
  }

  function build(rom, patch, options) {
    if (!rom || !patch) throw new Error('A loaded ROM and project diff are required');
    var patches = patch.patches || {};
    var sections = [];
    buildShopSection(rom, patches, sections);
    buildItemSection(rom, patches, sections);
    buildClassSection(rom, patches, sections);
    buildEncounterSection(rom, patches, sections);
    buildDropSection(rom, patches, sections);
    buildConsumableSection(rom, patches, sections);
    buildConsumableEffectSection(rom, patches, sections);
    buildStatGateSection(rom, patches, sections);
    buildToolSection(patches, sections);
    buildSquadSection(patches, sections);
    buildScenarioSection(rom, patches, sections);
    addUnknownSections(patches, sections);

    options = options || {};
    var report = {
      title: options.title || 'Ogre Battle 64 Mod Changelog',
      generatedAt: patch.created_at || new Date().toISOString(),
      projectName: options.projectName || '',
      baseline: rom.layout && rom.layout.name ? rom.layout.name : 'Loaded ROM',
      totalChanges: totalFromSummary(patch.summary),
      sections: sections,
      note: 'Changes are relative to the ROM that was loaded as this project baseline. Save-game edits are separate.'
    };
    report.text = formatText(report);
    return report;
  }

  function formatText(report) {
    var lines = [report.title, ''];
    lines.push('Generated: ' + report.generatedAt);
    lines.push('ROM baseline: ' + report.baseline);
    if (report.projectName) lines.push('Project: ' + report.projectName);
    lines.push('Summary: ' + report.totalChanges + ' changed project record' +
      (report.totalChanges === 1 ? '' : 's') + ' across ' + report.sections.length +
      ' area' + (report.sections.length === 1 ? '' : 's') + '.');
    lines.push('Note: ' + report.note);

    if (!report.sections.length) {
      lines.push('', 'No ROM-project changes are currently recorded.');
      return lines.join('\n') + '\n';
    }

    report.sections.forEach(function(section) {
      lines.push('', section.title.toUpperCase() + ' (' + section.count + ')');
      lines.push('-'.repeat(section.title.length + String(section.count).length + 3));
      section.entries.forEach(function(entry) {
        lines.push('* ' + entry.title);
        entry.lines.forEach(function(line) { lines.push('  - ' + line); });
      });
    });
    return lines.join('\n') + '\n';
  }

  function defaultFilename(report) {
    var stamp = (report.generatedAt || new Date().toISOString()).replace(/[:.]/g, '-');
    return 'ob64_changelog_' + stamp + '.txt';
  }

  function download(report, filename) {
    var blob = new Blob([report.text || formatText(report)], { type: 'text/plain;charset=utf-8' });
    var url = URL.createObjectURL(blob);
    var anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = filename || defaultFilename(report);
    document.body.appendChild(anchor);
    anchor.click();
    document.body.removeChild(anchor);
    setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
    return anchor.download;
  }

  OB64.changelog = {
    build: build,
    formatText: formatText,
    download: download,
    defaultFilename: defaultFilename
  };
})();
