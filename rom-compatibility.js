// OB64 Mod Editor - permissive ROM intake and compatibility diagnostics

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var TAB_REQUIREMENTS = {
    shops: ['shops', 'strongholds', 'items', 'consumables', 'runtime-shop-readback'],
    consumables: ['consumables', 'consumable-effects'],
    squads: ['enemy-squads', 'classes'],
    scenario: ['archive-catalog', 'enemy-squads', 'strongholds', 'classes', 'scenario-models'],
    classes: ['classes', 'class-support', 'items', 'actions', 'stat-gates'],
    items: ['items'],
    art: ['native-art'],
    sprites: ['native-art', 'sprite-library'],
    cutscenes: ['cutscene-studio'],
    encounters: ['encounters', 'creature-drops'],
    tools: ['tools-patches'],
    damage: ['classes', 'items', 'actions', 'consumables'],
    map: ['world-map'],
    changelog: ['project-baseline'],
    save: []
  };

  var TAB_CAUTIONS = {
    shops: ['shop-runtime-hook'],
    consumables: ['descriptions'],
    scenario: ['source-redirect'],
    classes: ['combat-animation-overrides', 'stat-gate-writeback', 'source-redirect', 'descriptions'],
    items: ['descriptions']
  };

  function asHex(value, width) {
    if (value == null) return 'unknown';
    return '0x' + (Number(value) >>> 0).toString(16).toUpperCase()
      .padStart(width || 8, '0');
  }

  function equalBytes(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var i = 0; i < left.length; i++) if (left[i] !== right[i]) return false;
    return true;
  }

  function wordsToBytes(words) {
    var out = new Uint8Array(words.length * 4);
    for (var i = 0; i < words.length; i++) {
      var value = Number(words[i]) >>> 0;
      out[i * 4] = value >>> 24;
      out[i * 4 + 1] = (value >>> 16) & 0xFF;
      out[i * 4 + 2] = (value >>> 8) & 0xFF;
      out[i * 4 + 3] = value & 0xFF;
    }
    return out;
  }

  async function sha256Hex(bytes) {
    if (!window.crypto || !window.crypto.subtle) {
      throw new Error('This browser cannot calculate the normalized ROM SHA-256.');
    }
    var digest = new Uint8Array(await window.crypto.subtle.digest('SHA-256', bytes));
    return Array.prototype.map.call(digest, function(value) {
      return value.toString(16).padStart(2, '0');
    }).join('').toUpperCase();
  }

  function component(report, id) {
    if (!report) return null;
    for (var i = 0; i < report.components.length; i++) {
      if (report.components[i].id === id) return report.components[i];
    }
    return null;
  }

  function setComponent(report, row) {
    row = Object.assign({
      category: 'feature', status: 'readable', reason: '', details: {},
      affectsTabs: [], requiredForEditing: false, requiredForExport: false
    }, row || {});
    return OB64.setRomCompatibilityComponent(report, row);
  }

  function addReason(report, reason) {
    if (reason && report.reasons.indexOf(reason) === -1) report.reasons.push(reason);
  }

  function blockedRequiredParsers(report) {
    return report.components.filter(function(row) {
      return row.requiredForEditing && row.status === 'blocked';
    });
  }

  function exportBlockingComponents(report) {
    return report.components.filter(function(row) {
      return row.id !== 'export-safety' && row.requiredForExport &&
        (row.status === 'blocked' || row.status === 'conflict');
    });
  }

  function refreshTabStates(report) {
    var tabs = {};
    Object.keys(TAB_REQUIREMENTS).forEach(function(tab) {
      var requirements = TAB_REQUIREMENTS[tab];
      var blocked = [];
      var cautions = [];
      requirements.forEach(function(id) {
        var row = component(report, id);
        if (!row || row.status === 'blocked') {
          blocked.push(row || {
            id: id, label: id, status: 'blocked',
            reason: 'This compatibility check did not run.'
          });
        } else if (row.status === 'warning' || row.status === 'conflict') {
          cautions.push(row);
        }
      });
      (TAB_CAUTIONS[tab] || []).forEach(function(id) {
        var row = component(report, id);
        if (row && row.status !== 'readable') cautions.push(row);
      });
      var problems = blocked.concat(cautions);
      tabs[tab] = {
        status: blocked.length ? 'blocked' : (cautions.length ? 'warning' : 'readable'),
        reasons: problems.map(function(row) {
          return row.label + ': ' + row.reason;
        })
      };
    });
    report.tabs = tabs;
    return tabs;
  }

  function recompute(report) {
    var parserBlocks = blockedRequiredParsers(report);
    var exportBlocks = exportBlockingComponents(report);
    var tabs = refreshTabStates(report);
    var readableEditorTabs = Object.keys(tabs).filter(function(tab) {
      return tab !== 'save' && tabs[tab].status !== 'blocked';
    });
    report.canEdit = !!report.source.selectedLayout && readableEditorTabs.length > 0;
    report.canExport = !!report.source.selectedLayout &&
      parserBlocks.length === 0 && exportBlocks.length === 0;

    var conflicts = report.components.filter(function(row) { return row.status === 'conflict'; });
    var warnings = report.components.filter(function(row) { return row.status === 'warning'; });
    var featureBlocks = report.components.filter(function(row) {
      return row.status === 'blocked' && !row.requiredForEditing;
    });
    if (!report.source.selectedLayout || parserBlocks.length) {
      report.overall = 'blocked';
    } else if (!report.exactVanilla || conflicts.length || warnings.length ||
               featureBlocks.length || exportBlocks.length) {
      report.overall = 'warning';
    } else {
      report.overall = 'verified';
    }
    return report;
  }

  function vanillaHashFor(rom) {
    var constants = OB64.art && OB64.art.constants;
    if (!constants || !rom || !rom.layout) return '';
    if (rom.layout.id === 'us-rev0') return constants.REV0_Z64_SHA256;
    if (rom.layout.id === 'us-rev1') return constants.REV1_Z64_SHA256;
    return '';
  }

  async function identify(rom, sourceIdentity) {
    var report = rom && rom.compatibility;
    if (!report) throw new Error('The ROM loader did not create a compatibility report.');
    report.source.normalizedSha256 = await sha256Hex(rom.z64);
    report.source.rawSha256 = sourceIdentity && sourceIdentity.facts
      ? sourceIdentity.facts.sha256 || '' : '';
    report.source.filename = sourceIdentity && sourceIdentity.facts
      ? sourceIdentity.facts.filename || '' : '';

    var expectedHash = vanillaHashFor(rom);
    report.exactVanilla = !!expectedHash && report.source.normalizedSha256 === expectedHash;
    if (!rom.layout) {
      setComponent(report, {
        id: 'identity', label: 'ROM identity', category: 'identity', status: 'blocked',
        reason: 'The ROM header has no verified Ogre Battle 64 parser profile.',
        details: { normalizedSha256: report.source.normalizedSha256 },
        requiredForEditing: true, requiredForExport: true
      });
      report.classification = 'unknown-layout';
    } else if (report.exactVanilla) {
      setComponent(report, {
        id: 'identity', label: 'ROM identity', category: 'identity', status: 'readable',
        reason: 'The normalized bytes match the approved vanilla ' + rom.layout.name + ' ROM.',
        details: { normalizedSha256: report.source.normalizedSha256, expectedSha256: expectedHash }
      });
      report.classification = rom.layout.id + '-vanilla';
    } else {
      setComponent(report, {
        id: 'identity', label: 'ROM identity', category: 'identity', status: 'warning',
        reason: 'The header uses the known ' + rom.layout.name + ' layout, but the normalized ROM differs from vanilla. Parsers and patch owners are checked separately below.',
        details: {
          normalizedSha256: report.source.normalizedSha256,
          expectedSha256: expectedHash,
          headerCrc1: asHex(report.source.crc1),
          headerCrc2: asHex(report.source.crc2)
        }
      });
      report.classification = rom.layout.id + '-modified';
      addReason(report,
        'Known parser and ownership checks cannot prove that every unknown modification is compatible with the game. Cold-boot and test every exported result.');
    }

    var archive = component(report, 'archive-catalog');
    if (archive && archive.status === 'readable' &&
        archive.details && archive.details.archiveCount !== archive.details.retailArchiveCount) {
      archive.status = 'warning';
      archive.reason = 'The key archives retain their expected sizes, but the scan found ' +
        archive.details.archiveCount + ' members instead of the retail 825. Added LHA-like data may change ordinal assumptions elsewhere.';
    }

    recompute(report);
    return report;
  }

  async function runInitializer(rom, spec, action) {
    var report = rom.compatibility;
    try {
      var result = await action();
      if (!component(report, spec.id)) {
        setComponent(report, {
          id: spec.id, label: spec.label, category: 'feature', status: 'readable',
          reason: spec.successReason || 'Initialization completed.',
          affectsTabs: spec.affectsTabs || [],
          requiredForExport: !!spec.requiredForExport
        });
      }
      return result;
    } catch (error) {
      setComponent(report, {
        id: spec.id, label: spec.label, category: 'feature', status: 'blocked',
        reason: error && error.message ? error.message : String(error),
        details: { exception: error && error.name || 'Error' },
        affectsTabs: spec.affectsTabs || [],
        requiredForExport: !!spec.requiredForExport
      });
      return null;
    }
  }

  function runAssessment(report, spec, action) {
    try {
      action();
    } catch (error) {
      setComponent(report, {
        id: spec.id,
        label: spec.label,
        category: 'feature',
        status: spec.failureStatus || 'blocked',
        reason: 'Compatibility assessment failed: ' +
          (error && error.message ? error.message : String(error)),
        details: { exception: error && error.name || 'Error' },
        affectsTabs: spec.affectsTabs || [],
        requiredForExport: !!spec.requiredForExport
      });
    }
  }

  function availabilityFailures(state) {
    var out = [];
    if (!state || !state.availability) return ['Consumable-effect availability was not initialized.'];
    ['range', 'common', 'descriptions'].forEach(function(key) {
      var facet = state.availability[key];
      if (facet && !facet.ok) out.push(facet.reason || (key + ' guard failed.'));
    });
    Object.keys(state.availability.magnitudes || {}).forEach(function(key) {
      var facet = state.availability.magnitudes[key];
      if (facet && !facet.ok) out.push(facet.reason || (key + ' guard failed.'));
    });
    return out.filter(function(value, index, values) { return values.indexOf(value) === index; });
  }

  function assessArt(rom, report) {
    var state = rom.art;
    if (state && state.supported) {
      var armyReady = !!(state.armySprites && state.armySprites.supported);
      var combatReady = !!(state.animations && state.animations.supported);
      var unavailable = [];
      if (!combatReady) unavailable.push(state.animations &&
        state.animations.unavailableReason ||
        'Combat sprite initialization did not complete.');
      if (!armyReady) unavailable.push(state.armySprites &&
        state.armySprites.unavailableReason ||
        'Army sprite initialization did not complete.');
      setComponent(report, {
        id: 'native-art',
        label: 'Avatars, item icons, combat sprites, and Army sprites',
        status: combatReady && armyReady ? 'readable' : 'warning',
        reason: unavailable.length
          ? unavailable.join(' ')
          : (state.exactRetail
            ? 'Native avatar, icon, combat sprite, and Army sprite resources match the verified retail structures.'
            : 'Native avatar, icon, combat sprite, and Army sprite resources pass their structural checks on this modified ROM.'),
        details: {
          exactRetail: !!state.exactRetail,
          avatarAppearances: state.avatar && state.avatar.appearances.length,
          icons: state.icons && state.icons.icons.length,
          combatSpriteSequences: state.animations && state.animations.specs.length,
          armySpriteAtlases: state.armySprites && state.armySprites.atlases.length,
          armySpritePlanes: state.armySprites && state.armySprites.sourceModels.length,
          armySpriteRoutedPlanes: state.armySprites && state.armySprites.models.length,
          armySpriteClassRoutes: state.armySprites && state.armySprites.classRoutes.length
        },
        affectsTabs: ['art', 'sprites']
      });
    } else {
      setComponent(report, {
        id: 'native-art',
        label: 'Avatars, item icons, combat sprites, and Army sprites',
        status: 'blocked',
        reason: state && state.unavailableReason || 'Native art initialization did not complete.',
        affectsTabs: ['art', 'sprites']
      });
    }
  }

  function assessChecksum(rom, report) {
    if (!rom.layout || !OB64.consumableEffects ||
        !OB64.consumableEffects.verifyIndependentCrc) return;
    var verification = OB64.consumableEffects.verifyIndependentCrc(rom.z64);
    setComponent(report, {
      id: 'rom-checksum', label: 'N64 boot checksum', category: 'identity',
      status: verification.ok ? 'readable' : 'conflict',
      reason: verification.ok
        ? 'The CIC-6102 checksum in the ROM header matches the independently calculated value.'
        : 'The CIC-6102 checksum in the ROM header does not match the ROM data. Hardware or emulators may reject or mis-handle this image.',
      details: verification,
      requiredForExport: !verification.ok
    });
    if (!verification.ok) {
      addReason(report,
        'The source ROM has an invalid CIC-6102 checksum. Repair its source provenance before using this editor to export it.');
    }
  }

  function assessConsumableEffects(rom, report) {
    var prior = component(report, 'consumable-effects');
    if (prior && prior.status === 'blocked') return;
    var state = rom.consumableEffects;
    if (!state) return;
    var failures = availabilityFailures(state);
    setComponent(report, {
      id: 'consumable-effects', label: 'Consumable effect code and text',
      status: failures.length ? 'conflict' : 'readable',
      reason: failures.length
        ? failures.join(' | ')
        : 'Opcode, dispatch, metadata, description, and current-value guards match the selected revision.',
      details: { failedGuardCount: failures.length, failures: failures },
      affectsTabs: ['consumables'],
      requiredForExport: false
    });
  }

  function assessCombatOverrides(rom, report) {
    var prior = component(report, 'combat-animation-overrides');
    if (prior && prior.status === 'blocked') return;
    var state = rom.combatAnimationOverrides;
    if (!state) return;
    var status = 'readable';
    var reason = 'The selector-override lane is retail-clean or editor-owned.';
    if (!state.supported) {
      status = 'blocked'; reason = state.disabledReason || 'This revision has no verified selector-override lane.';
    } else if (state.readOnly) {
      status = state.laneState === 'foreign' ? 'conflict' : 'warning';
      reason = state.disabledReason || state.diagnostic || 'The selector-override lane is read-only.';
    }
    setComponent(report, {
      id: 'combat-animation-overrides', label: 'Combat animation override lane',
      status: status, reason: reason,
      details: { laneState: state.laneState, readOnly: !!state.readOnly },
      affectsTabs: ['classes']
    });
  }

  function assessTools(rom, report) {
    var prior = component(report, 'tools-patches');
    if (prior && prior.status === 'blocked') return;
    if (!rom.tools || !OB64.tools) return;
    var foreign = [], unsupported = [], applied = [], outdated = [];
    var features = OB64.tools.features();
    features.forEach(function(feature) {
      var state = rom.tools.initial[feature.id];
      if (state === 'foreign') foreign.push(feature.name);
      else if (state === 'unsupported') unsupported.push(feature.name);
      else if (state === 'applied') applied.push(feature.name);
      else if (state === 'outdated') outdated.push(feature.name);
    });
    var status = foreign.length ? 'conflict' : (unsupported.length || outdated.length ? 'warning' : 'readable');
    var reasons = [];
    if (foreign.length) reasons.push('Foreign bytes block: ' + foreign.join(', ') + '.');
    if (unsupported.length) reasons.push('Unavailable for this revision: ' + unsupported.join(', ') + '.');
    if (outdated.length) reasons.push('Known older editor payloads will be upgraded on export: ' + outdated.join(', ') + '.');
    if (!reasons.length) reasons.push('Every Tools feature is retail-clean, current, or safely disabled.');
    setComponent(report, {
      id: 'tools-patches', label: 'Tools patch regions', status: status,
      reason: reasons.join(' '),
      details: { foreign: foreign, unsupported: unsupported, applied: applied, outdated: outdated },
      affectsTabs: ['tools']
    });
  }

  function assessStatGateOwnership(rom, report) {
    var relocation = rom.statGates && rom.statGates.meta && rom.statGates.meta.relocation;
    if (!relocation) return;
    var unavailable = relocation.state === 'unavailable';
    setComponent(report, {
      id: 'stat-gate-writeback', label: 'Stat-gate relocation owner',
      status: relocation.state === 'foreign' ? 'conflict' : (unavailable ? 'blocked' : 'readable'),
      reason: relocation.state === 'foreign'
        ? (relocation.error || 'The stat-gate relocation owner is foreign or malformed.')
        : (unavailable
          ? (relocation.error || 'The stat-gate relocation owner could not be assessed because its parser did not run.')
          : 'The stat-gate stream is retail/in-place or has a recognized editor-owned relocation.'),
      details: relocation,
      affectsTabs: ['classes']
    });
  }

  function assessSourceRedirect(rom, report) {
    if (!OB64.sourceRedirect || !rom.layout) return;
    var state = OB64.sourceRedirect.classify(rom.z64);
    var status = 'readable';
    var reason = 'The shared PI-source redirect controller is retail-clean.';
    if (!state.ok || state.state === 'foreign') {
      status = 'conflict'; reason = state.reason || 'The shared PI-source redirect controller is foreign.';
    } else if (state.state === 'installed') {
      status = 'warning';
      reason = 'A recognized shared PI-source redirect is installed. Its entries are preserved, but Scenario adoption can still require the matching Project.';
    }
    setComponent(report, {
      id: 'source-redirect', label: 'Shared ROM-source redirects', status: status,
      reason: reason,
      details: { state: state.state, entryCount: state.entries && state.entries.length || 0 },
      affectsTabs: ['scenario', 'classes']
    });
  }

  function assessShopHook(rom, report) {
    if (!OB64.runtimeOverrides || !rom.layout || !OB64.squad) return;
    var layout = OB64.runtimeOverrides.patchLayout(rom);
    var start = layout.SHOP_HOOK_ROM;
    var observed = rom.z64.slice(start, start + 24);
    var installed = OB64.runtimeOverrides.buildShopHook(layout);
    var retail = wordsToBytes(layout.SHOP_ORIGINAL_WORDS);
    var status = 'readable';
    var reason = 'The shop producer hook is retail-clean.';
    if (equalBytes(observed, installed)) {
      var count = OB64.runtimeOverrides.parseShopOverrides(rom.z64, rom);
      reason = 'The shop producer hook is editor-owned; ' + Object.keys(count).length + ' runtime shop overrides were decoded.';
    } else if (!equalBytes(observed, retail)) {
      status = 'conflict';
      reason = 'The shop producer hook matches neither retail nor this editor. Shop membership export will refuse to overwrite it.';
    }
    setComponent(report, {
      id: 'shop-runtime-hook', label: 'Shop runtime hook', status: status,
      reason: reason, details: { z64Offset: start }, affectsTabs: ['shops']
    });
  }

  function assessFeatures(rom) {
    var report = rom.compatibility;
    runAssessment(report, {
      id: 'rom-checksum', label: 'N64 boot checksum',
      failureStatus: 'conflict', requiredForExport: true
    }, function() { assessChecksum(rom, report); });
    runAssessment(report, {
      id: 'native-art',
      label: 'Avatars, item icons, combat sprites, and Army sprites',
      affectsTabs: ['art', 'sprites']
    }, function() { assessArt(rom, report); });
    runAssessment(report, {
      id: 'consumable-effects', label: 'Consumable effect code and text',
      affectsTabs: ['consumables'], requiredForExport: true
    }, function() { assessConsumableEffects(rom, report); });
    runAssessment(report, {
      id: 'combat-animation-overrides', label: 'Combat animation override lane',
      affectsTabs: ['classes']
    }, function() { assessCombatOverrides(rom, report); });
    runAssessment(report, {
      id: 'tools-patches', label: 'Tools patch regions', affectsTabs: ['tools']
    }, function() { assessTools(rom, report); });
    runAssessment(report, {
      id: 'stat-gate-writeback', label: 'Stat-gate relocation owner',
      affectsTabs: ['classes'], failureStatus: 'conflict'
    }, function() { assessStatGateOwnership(rom, report); });
    runAssessment(report, {
      id: 'source-redirect', label: 'Shared ROM-source redirects',
      affectsTabs: ['scenario', 'classes'], failureStatus: 'conflict'
    }, function() { assessSourceRedirect(rom, report); });
    runAssessment(report, {
      id: 'shop-runtime-hook', label: 'Shop runtime hook',
      affectsTabs: ['shops'], failureStatus: 'conflict'
    }, function() { assessShopHook(rom, report); });

    var parserBlocks = blockedRequiredParsers(report);
    var exportBlocks = exportBlockingComponents(report);
    if (parserBlocks.length) {
      report.summary = 'The ROM loaded, but ' + parserBlocks.length +
        ' required parser' + (parserBlocks.length === 1 ? '' : 's') +
        ' could not read its expected structure. Readable tabs remain available, but complete ROM export is disabled.';
    } else if (exportBlocks.length) {
      report.summary = 'The ROM loaded and readable tabs remain available. Complete ROM export is disabled because ' +
        exportBlocks.map(function(row) { return row.label; }).join(', ') +
        ' did not pass its source-safety check.';
    } else if (report.exactVanilla) {
      report.summary = 'The ROM is an exact supported vanilla image. Known revision limits are listed below.';
    } else {
      report.summary = 'The ROM is not an exact vanilla image. Readable systems can be inspected and edited; conflicting features will block only their unsafe writes.';
    }
    setComponent(report, {
      id: 'export-safety', label: 'ROM export safety', category: 'export',
      status: parserBlocks.length || exportBlocks.length
        ? 'blocked' : (report.exactVanilla ? 'readable' : 'warning'),
      reason: parserBlocks.length
        ? 'Required source structures are missing, so the complete export validator cannot prove a safe candidate.'
        : (exportBlocks.length
          ? 'Export is disabled because these source checks failed: ' +
            exportBlocks.map(function(row) { return row.label; }).join(', ') + '.'
          : (report.exactVanilla
          ? 'Export starts from an approved retail baseline and still runs feature ownership, readback, byte-range, and checksum validation.'
          : 'Export starts from the loaded modified ROM. Each changed feature must pass its own preimage and ownership guards before the finished-ROM validator runs.')),
      requiredForExport: parserBlocks.length > 0 || exportBlocks.length > 0
    });
    recompute(report);
    return report;
  }

  function tabState(report, tab) {
    return report && report.tabs && report.tabs[tab]
      ? report.tabs[tab]
      : { status: 'blocked', reasons: ['Compatibility was not assessed for this tab.'] };
  }

  function counts(report) {
    var out = { readable: 0, warning: 0, blocked: 0, conflict: 0 };
    (report && report.components || []).forEach(function(row) {
      if (Object.prototype.hasOwnProperty.call(out, row.status)) out[row.status]++;
    });
    return out;
  }

  function logLine(report) {
    var totals = counts(report);
    return '[ROM compatibility] classification=' + report.classification +
      '; overall=' + report.overall +
      '; layout=' + (report.source.selectedLayout || 'none') +
      '; normalizedSha256=' + (report.source.normalizedSha256 || 'unavailable') +
      '; readable=' + totals.readable +
      '; warnings=' + totals.warning +
      '; conflicts=' + totals.conflict +
      '; blocked=' + totals.blocked +
      '; edit=' + (report.canEdit ? 'enabled' : 'disabled') +
      '; export=' + (report.canExport ? 'enabled' : 'disabled') + '.';
  }

  OB64.romCompatibility = {
    identify: identify,
    runInitializer: runInitializer,
    assessFeatures: assessFeatures,
    recompute: recompute,
    component: component,
    setComponent: setComponent,
    tabState: tabState,
    counts: counts,
    logLine: logLine,
    sha256Hex: sha256Hex,
    tabRequirements: TAB_REQUIREMENTS
  };
})(window.OB64);
