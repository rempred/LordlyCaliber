// OB64 Mod Editor - Patch format (JSON save/load of user edits)
//
// A "patch" is a JSON document capturing the user's changes relative to the
// pristine ROM state that was loaded. It's small, human-readable, and portable:
// the same patch can be applied to any compatible ROM to reproduce the edits
// without re-exporting a full v64.
//
// v4 scope: all public ROM-edit tabs: shops, item_prices (legacy price-only
// mirror), item stat fields, class definition fields, neutral encounters,
// creature drops, consumables, stat gates, and the global encounter-roll slider.
// v5 adds `tools`: Tools-tab feature toggles (map of feature id -> bool),
// recorded only when the toggle differs from the state detected in the
// loaded ROM.
// v6 remaps class-def logical bytes 64-71 to the current class's name-framed
// header (physically statOff-8..-1), retiring the shifted statOff+B64..B71 view.
// v6 also carries `squadOverrides`: runtime-key/EDAT replacement records used
// by the Squads tab.
// v7 carries `scenario`: the Scenario tab project payload (modified ESETs,
// added squads, squad comp records, and site allegiance intents).
// v8 adds the eight decoded 2-bit equipment growth lanes from item B20-B21.
// v9 exposes the remaining raw equipment bytes and class name-pointer bytes.
// v10 carries each edited shop's per-shop consumable membership alongside its
// equipment IDs. Both lists compile into the shared runtime override table;
// v9 and older equipment-only projects remain readable.
// v11 carries Scenario-project v3 stronghold-field intents for global ktenmain
// Population and Morale edits.
// v12 carries the five verified consumable-effect range models. IDs 11-16 use
// one canonical shared entry under `patches.consumableEffects["11-16"]`.
// v13 adds absolute magnitude entries for consumable IDs 1-5 while preserving
// the v12 range schemas and per-key backward compatibility.
// v14 adds the legacy one-selector combat-animation override table.
// v15 replaces each v14 selector with Normal (modes 0/1) and Blocked (mode 2).
// v16 carries Scenario treasure state and shared treasure ownership metadata.
// v17 carries item, consumable, class, and combat-action descriptions.
// v18 carries exact RGBA5551 pixels for edited route-scoped avatars and item icons.
// v19 carries bounded combat-sprite CI8/I4 pixels grouped by source and child.
// v20 expands combat art to the accepted complete sequence corpus. New records
// use stable logical binding and physical-source identities; v19 records remain
// readable through the frozen eleven-sequence compatibility map.
// v21 adds exact class/body animation assignments and private, relocatable
// complete-sequence copies with Project round-trip support.
// v22 stores private copied sprites and complete ordered frame-layer metadata.
// v23 stores stable private frame identities so removed frames round-trip exactly.
// v24 stores the exact private body program so added and reordered frames round-trip.
// v25 carries validated Cutscene Studio SceneDocuments and view state for edited
// physical director resources. Native source words remain byte-preserved.
// v26 stores the two structural neutral global-rate paths independently and
// records whether the editor intentionally links them.
// v27 stores per-slice two-state runtime rates and per-slot creature-drop
// weights. Older Projects retain inherit/equal-weight retail behavior.
// v28 adds direct per-scenario two-state rates and custom neutral squads with
// formation, level offsets, and fixed persuasion chances.
// v29 adds per-custom-squad retreat HP thresholds.
// v30 replaces custom neutral squads' opaque 1-9 formation records with the
// game-native typed 0-8 model. It also carries cohort equipment, three optional
// player-leader class persuasion bonuses, and a three-choice weighted reward.
// v31 carries reusable Sprite Editor assets. These Project records do not alter
// ROM bytes until Art and Animation imports them into a supported native target.
// v32 carries edits to existing Army sprite CI8 planes in the six verified
// native resources.
// v33 carries custom player-side Army sprite planes for routed model IDs that
// are missing from the retail player atlases.

window.OB64 = window.OB64 || {};

(function() {
  var PATCH_FORMAT = 'ob64-patch';
  var PATCH_VERSION = 33;

  // Item-stat fields edited by the Items tab. Price stays in the legacy
  // item_prices map so v2 patches remain readable and easy to diff.
  var ITEM_PATCH_FIELDS = [
    'equipType', 'element', 'grade',
    'str', 'int', 'agi', 'dex', 'vit', 'men',
    'resPhys', 'resWind', 'resFire', 'resEarth',
    'resWater', 'resVirtue', 'resBane',
    'growthHpStr', 'growthUnknown', 'growthInt', 'growthAgi',
    'growthDex', 'growthVit', 'growthMen', 'growthLck',
    'b3Raw', 'b12Raw',
    'b22Raw', 'b23Raw', 'b24Raw', 'b25Raw', 'b26Raw', 'b27Raw',
    'b28Raw', 'b29Raw', 'b30Raw', 'b31Raw'
  ];
  var ITEM_APPLY_FIELDS = ITEM_PATCH_FIELDS.concat(['price']);
  var ITEM_SIGNED_RAW_FIELDS = {
    str: 'strRaw', int: 'intRaw', agi: 'agiRaw',
    dex: 'dexRaw', vit: 'vitRaw', men: 'menRaw',
  };
  var ITEM_SIGNED_FIELDS = {
    str: true, int: true, agi: true, dex: true, vit: true, men: true,
    resPhys: true, resWind: true, resFire: true, resEarth: true,
    resWater: true, resVirtue: true, resBane: true,
  };
  var ITEM_GROWTH_FIELDS = {
    growthHpStr: true, growthUnknown: true, growthInt: true, growthAgi: true,
    growthDex: true, growthVit: true, growthMen: true, growthLck: true,
  };

  var CLASS_GROWTH_FIELDS = [
    'strGrowth', 'vitGrowth', 'intGrowth',
    'menGrowth', 'agiGrowth', 'dexGrowth'
  ];
  var CLASS_G2_RAW_FIELDS = ['b3Raw', 'b7Raw', 'b11Raw', 'b15Raw', 'b19Raw'];
  var STAT_GATE_FIELDS = ['str', 'vit', 'int', 'men', 'agi', 'dex', 'alnMin', 'alnMax'];

  // --------------------------------------------------------------
  // Snapshot pristine state - call once, right after OB64.loadROM().
  // Every subsequent edit writes through to rom.*, and collectPatch()
  // diffs against rom.original.
  // --------------------------------------------------------------
  function snapshotOriginal(rom) {
    rom.original = rom.original || {};

    rom.original.shops = (rom.shops || []).map(function(s) {
      return {
        items: (s.items || []).slice(),
        consumables: (s.consumables || []).slice(),
        runtimeOverride: !!s.runtimeOverride
      };
    });

    rom.original.itemPrices = {};
    for (var i = 0; rom.itemStats && i < rom.itemStats.length; i++) {
      var stat = rom.itemStats[i];
      if (stat && typeof stat.price === 'number') {
        rom.original.itemPrices[i] = stat.price;
      }
    }

    rom.original.itemStats = rom.itemStats
      ? rom.itemStats.map(snapshotItemStat)
      : [];
    rom.original.classDefBytes = rom.classDefs
      ? rom.classDefs.map(classDefByteMap)
      : [];
    rom.original.neutralEncounters = snapshotNeutralEncounters(rom.neutralEncounters);
    rom.original.creatureDrops = snapshotCreatureDrops(rom.creatureDrops);
    rom.original.consumables = rom.consumables
      ? rom.consumables.map(snapshotConsumable)
      : [];
    rom.original.statGates = snapshotStatGates(rom.statGates);
    rom.original.descriptions = OB64.descriptionCodec
      ? OB64.descriptionCodec.snapshotAll(rom)
      : { items: [], consumables: [], classes: [], actions: [] };

    var globalRate = rom.neutralEncounters && rom.neutralEncounters.globalRate;
    rom.original.neutralGlobalRate = globalRate ? {
      basisPoints: clampBasisPoints(globalRate.basisPoints),
      microBasisPoints: clampMicroBasisPoints(globalRate.microBasisPoints),
      normalPassCount: globalRate.normalPassCount,
      alternatePassCount: globalRate.alternatePassCount,
      divisor: globalRate.divisor,
      linked: !!globalRate.linked,
      mode: globalRate.mode || 'unknown'
    } : null;
  }

  // --------------------------------------------------------------
  // collectPatch(rom) -> portable JSON patch object.
  // Returns { patches: {...}, summary: {...}, ... } with only the fields
  // that differ from rom.original.
  // --------------------------------------------------------------
  function collectPatch(rom) {
    if (!rom.original) throw new Error('OB64.snapshotOriginal() was not called on this rom');

    var shopsOut = {};
    for (var i = 0; rom.shops && i < rom.shops.length; i++) {
      var shopNow = rom.shops[i];
      var shopOriginal = rom.original.shops[i];
      if (!arraysEqual(shopNow.items, shopOriginal.items) ||
          !arraysEqual(shopNow.consumables, shopOriginal.consumables)) {
        shopsOut[String(i)] = {
          items: (shopNow.items || []).slice(),
          consumables: (shopNow.consumables || []).slice()
        };
      }
    }

    var pricesOut = {};
    for (var id = 0; rom.itemStats && id < rom.itemStats.length; id++) {
      var stat = rom.itemStats[id];
      if (!stat || typeof stat.price !== 'number') continue;
      var orig = rom.original.itemPrices[id];
      if (typeof orig === 'number' && stat.price !== orig) {
        pricesOut[String(id)] = stat.price;
      }
    }

    var itemsOut = {};
    var originalItems = rom.original.itemStats || [];
    for (var itemId = 0; rom.itemStats && itemId < rom.itemStats.length; itemId++) {
      var itemPatch = diffItemStat(rom.itemStats[itemId], originalItems[itemId]);
      if (itemPatch) itemsOut[String(itemId)] = itemPatch;
    }

    var classDefsOut = {};
    var originalClassDefBytes = rom.original.classDefBytes || [];
    for (var recIdx = 0; rom.classDefs && recIdx < rom.classDefs.length; recIdx++) {
      var rec = rom.classDefs[recIdx];
      if (!rec) continue;
      var classId = recIdx - 1;
      if (classId <= 0) continue;
      var bytePatch = diffByteMap(classDefByteMap(rec), originalClassDefBytes[recIdx]);
      if (bytePatch) {
        classDefsOut[String(classId)] = {
          record_index: recIdx,
          bytes: bytePatch
        };
      }
    }

    var neutralOut = diffNeutralEncounters(rom.neutralEncounters, rom.original.neutralEncounters);
    var creatureDropsOut = diffCreatureDrops(rom.creatureDrops, rom.original.creatureDrops);
    var weightedDropChanges = Object.keys(creatureDropsOut).filter(function(key) {
      var entry = creatureDropsOut[key] || {};
      return entry.weights_modified === true ||
        (entry.weights || []).some(function(weight) { return Number(weight) !== 1; });
    }).length;
    var consumablesOut = diffConsumables(rom.consumables, rom.original.consumables);
    var statGatesOut = diffStatGates(rom.statGates, rom.original.statGates);
    var descriptionsOut = OB64.descriptionCodec
      ? OB64.descriptionCodec.collectProjectPayload(
          rom,
          rom.original.descriptions || {}
        )
      : { items: {}, consumables: {}, classes: {}, actions: {} };

    var globalRateOut = null;
    var globalRate = rom.neutralEncounters && rom.neutralEncounters.globalRate;
    if (globalRate && globalRate.modified) {
      var divisor = Number.isInteger(globalRate.divisor) && globalRate.divisor > 0
        ? globalRate.divisor : OB64.NEUTRAL_GLOBAL_VANILLA_DIVISOR;
      function branchPayload(prefix, vanillaPassCount) {
        var passCount = Number(globalRate[prefix + 'PassCount']);
        if (!Number.isInteger(passCount) || passCount < 1) {
          var micro = OB64.neutralGlobalBranchMicroBasisPoints(globalRate, prefix);
          passCount = Math.max(1, Math.min(0x8000, Math.round((micro * divisor) / 1000000)));
        }
        var microBasisPoints = clampMicroBasisPoints((passCount * 1000000) / divisor);
        return {
          pass_count: passCount,
          micro_basis_points: microBasisPoints,
          basis_points: microBasisPoints / 100,
          percent: microBasisPoints / 10000,
          multiplier: Math.round((passCount / vanillaPassCount) * 10000) / 10000
        };
      }
      var normalBranch = branchPayload('normal', 51);
      var alternateBranch = globalRate.linked
        ? branchPayload('normal', 11)
        : branchPayload('alternate', 11);
      globalRateOut = {
        schema: 2,
        divisor: divisor,
        linked: !!globalRate.linked,
        normal: normalBranch,
        alternate: alternateBranch,
        // Retain the v25 top-level shape as a readable normal-path mirror.
        multiplier: normalBranch.multiplier,
        basis_points: normalBranch.basis_points,
        micro_basis_points: normalBranch.micro_basis_points,
        percent: normalBranch.percent
      };
    }

    var toolsOut = {};
    if (rom.tools && OB64.tools) {
      var toolFeatures = OB64.tools.features();
      for (var tf = 0; tf < toolFeatures.length; tf++) {
        var toolId = toolFeatures[tf].id;
        if (rom.tools.initial[toolId] === 'foreign') continue;
        var wasApplied = rom.tools.initial[toolId] === 'applied';
        var nowWanted = !!rom.tools.desired[toolId];
        if (nowWanted !== wasApplied) toolsOut[toolId] = nowWanted;
      }
    }

    var squadOverridesOut = collectSquadOverridePatch(rom);
    var scenarioOut = collectScenarioPatch(rom);
    var cutscenesOut = OB64.cutsceneProject && rom.cutsceneStudio
      ? OB64.cutsceneProject.collect(rom.cutsceneStudio) : null;
    var consumableEffectsOut = OB64.consumableEffects
      ? OB64.consumableEffects.collectProjectPayload(rom.consumableEffects)
      : {};
    var combatAnimationOverridesOut = OB64.combatAnimationOverrides
      ? OB64.combatAnimationOverrides.collectProjectPayload(rom.combatAnimationOverrides)
      : null;
    var combatAnimationOverrideChanges = combatAnimationOverridesOut
      ? OB64.combatAnimationOverrides.diff(
          rom.combatAnimationOverrides.baseline,
          rom.combatAnimationOverrides.desired
        ).count
      : 0;
    var artOut = OB64.art
      ? OB64.art.collectProjectPayload(rom)
      : {
          schemaVersion: 5, avatars: {}, icons: {}, armySprites: {},
          animations: {}, separations: null
        };
    var avatarArtChanges = Object.keys(artOut.avatars || {}).length;
    var iconArtChanges = Object.keys(artOut.icons || {}).length;
    var combatSpriteArtChanges = Object.keys(artOut.animations || {}).length;
    var armySpriteArtChanges = Object.keys(artOut.armySprites || {}).length;
    var separatedAnimationChanges = artOut.separations && artOut.separations.entries
      ? Object.keys(artOut.separations.entries).length : 0;
    var spriteLibraryOut = OB64.spriteLibrary
      ? OB64.spriteLibrary.collectProjectPayload(rom)
      : { schemaVersion: 1, assets: [] };
    var spriteLibraryChanges = spriteLibraryOut.assets.length;

    return {
      format: PATCH_FORMAT,
      version: PATCH_VERSION,
      created_at: new Date().toISOString(),
        editor_version: '2026-08-29',
      rom_hint: {
        archives_count: rom.archives ? rom.archives.length : null,
        shop_count:     rom.shops ? rom.shops.length : null,
      },
      summary: {
        shops_modified:        Object.keys(shopsOut).length,
        item_prices_modified:  Object.keys(pricesOut).length,
        item_stats_modified:   Object.keys(itemsOut).length,
        class_defs_modified:   Object.keys(classDefsOut).length,
        neutral_slices_modified: Object.keys(neutralOut.scenario_slices).length,
        terrain_rates_modified:  Object.keys(neutralOut.terrain_rates).length,
        neutral_slice_rates_modified: Object.keys(neutralOut.slice_rates || {}).length,
        neutral_scenario_rates_modified: Object.keys(neutralOut.scenario_rates).length,
        custom_neutral_squads_modified: Object.keys(neutralOut.custom_squads).length,
        creature_drop_records_modified: Object.keys(creatureDropsOut).length,
        weighted_drop_classes_modified: weightedDropChanges,
        consumables_modified:   Object.keys(consumablesOut).length,
        item_descriptions_modified: Object.keys(descriptionsOut.items).length,
        consumable_descriptions_modified: Object.keys(descriptionsOut.consumables).length,
        class_descriptions_modified: Object.keys(descriptionsOut.classes).length,
        action_descriptions_modified: Object.keys(descriptionsOut.actions).length,
        stat_gates_modified:    Object.keys(statGatesOut).length,
        neutral_global_rate_modified: globalRateOut ? 1 : 0,
        tools_modified:         Object.keys(toolsOut).length,
        squad_overrides_modified: Object.keys(squadOverridesOut).length,
        scenario_modified: scenarioOut ? scenarioPatchCount(scenarioOut) : 0,
        cutscenes_modified: cutscenesOut ? cutscenesOut.scenes.length : 0,
        consumable_effect_models_modified: Object.keys(consumableEffectsOut).length,
        combat_animation_overrides_modified: combatAnimationOverrideChanges,
        avatar_art_modified: avatarArtChanges,
        item_icon_art_modified: iconArtChanges,
        combat_sprite_art_modified: combatSpriteArtChanges,
        army_sprite_art_modified: armySpriteArtChanges,
        separated_animation_sequences_modified: separatedAnimationChanges,
        sprite_library_assets: spriteLibraryChanges,
      },
      patches: {
        shops:        shopsOut,
        item_prices:  pricesOut,
        items:        itemsOut,
        classDefs:    classDefsOut,
        neutral_encounters: neutralOut,
        creatureDrops: creatureDropsOut,
        consumables:  consumablesOut,
        descriptions: descriptionsOut,
        statGates:    statGatesOut,
        neutral_global_rate: globalRateOut,
        tools:        toolsOut,
        squadOverrides: squadOverridesOut,
        scenario:     scenarioOut,
        cutscenes:    cutscenesOut,
        consumableEffects: consumableEffectsOut,
        combatAnimationOverrides: combatAnimationOverridesOut,
        art: artOut,
        spriteLibrary: spriteLibraryOut
      },
    };
  }

  // --------------------------------------------------------------
  // applyPatch(rom, patch, dirtyFlags) -> in-place mutation of rom.
  // Marks dirtyFlags so the Export pipeline knows which archives/tables to
  // rewrite. Returns { applied: {...}, warnings: [...] }.
  // --------------------------------------------------------------
  function PatchFormatError(msg) { this.name = 'PatchFormatError'; this.message = msg; }
  PatchFormatError.prototype = new Error();

  function applyPatch(rom, patch, dirtyFlags, preparedImports) {
    if (!patch || patch.format !== PATCH_FORMAT) {
      throw new PatchFormatError('Not an ob64-patch file (format field missing or wrong)');
    }
    if (!Number.isInteger(patch.version) || patch.version > PATCH_VERSION) {
      throw new PatchFormatError('Patch version ' + patch.version +
        ' is newer than this editor understands (' + PATCH_VERSION + '). Update the editor.');
    }

    var p = patch.patches || {};
    preparedImports = preparedImports || {};
    var preparedDescriptions = null;
    try {
      preparedDescriptions = OB64.descriptionCodec.prepareProjectChanges(
        rom,
        Object.prototype.hasOwnProperty.call(p, 'descriptions')
          ? p.descriptions : {}
      );
    } catch (descriptionError) {
      throw new PatchFormatError(
        'Description Project data is invalid: ' + descriptionError.message
      );
    }
    if (Object.prototype.hasOwnProperty.call(p, 'enemies')) {
      var retiredEnemies = p.enemies;
      if (retiredEnemies === null || typeof retiredEnemies !== 'object' ||
          Array.isArray(retiredEnemies) || Object.keys(retiredEnemies).length !== 0) {
        throw new PatchFormatError(
          'Retired patches.enemies Project data is unsupported; only the historical empty object is accepted.'
        );
      }
    }
    var hasEffectPayload = Object.prototype.hasOwnProperty.call(p, 'consumableEffects');
    var effectPayload = hasEffectPayload ? p.consumableEffects : undefined;
    var validatedEffects = { entries: {}, modelCount: 0 };
    var hasSelectorPayload = patch.version >= 14 &&
      Object.prototype.hasOwnProperty.call(p, 'combatAnimationOverrides') &&
      p.combatAnimationOverrides !== null;
    var validatedSelectorEntries = null;
    if (hasSelectorPayload) {
      try {
        validatedSelectorEntries = OB64.combatAnimationOverrides.validateProjectPayload(
          rom, p.combatAnimationOverrides, patch.version
        );
      } catch (selectorError) {
        throw new PatchFormatError('Combat animation override Project data is invalid: ' + selectorError.message);
      }
    }
    if (hasEffectPayload) {
      if (effectPayload === null) {
        throw new PatchFormatError(
          'Consumable effect Project data is invalid: patches.consumableEffects must be an object, not null.'
        );
      }
      if (!OB64.consumableEffects || !rom.consumableEffects) {
        if (!effectPayload || typeof effectPayload !== 'object' ||
            Array.isArray(effectPayload) || Object.keys(effectPayload).length) {
          throw new PatchFormatError('This editor session cannot load consumable effect Project data.');
        }
      } else {
        try {
          validatedEffects = OB64.consumableEffects.validateProjectPayload(
            effectPayload,
            rom.consumableEffects,
            patch.version,
            rom.z64
          );
        } catch (effectError) {
          throw new PatchFormatError('Consumable effect Project data is invalid: ' + effectError.message);
        }
      }
    }

    var scenarioPatch = p.scenario || null;
    var preparedScenario = null;
    if (scenarioPatch && OB64.scenario && OB64.scenario.prepareProject) {
      try {
        preparedScenario = OB64.scenario.prepareProject(rom, scenarioPatch);
      } catch (scenarioError) {
        throw new PatchFormatError('Scenario Project data is invalid: ' + scenarioError.message);
      }
    }
    var cutscenePatch = p.cutscenes || null;
    var preparedCutscenes = preparedImports.cutscenes || null;
    if (cutscenePatch) {
      if (patch.version < 25) {
        throw new PatchFormatError(
          'Cutscene Studio Project data requires Project schema version 25 or newer.');
      }
      if (!OB64.cutsceneProject || !rom.cutsceneStudio || !preparedCutscenes) {
        throw new PatchFormatError(
          'Cutscene Studio Project data must be verified against the loaded ROM before it is applied.');
      }
    }
    var preparedArt = {
      avatars: {}, icons: {}, armySprites: { edits: {}, count: 0 },
      animations: { edits: {}, count: 0 },
      separations: null, count: 0
    };
    var artPayload = Object.prototype.hasOwnProperty.call(p, 'art') ? p.art : undefined;
    var hasArtRecords = artPayload && typeof artPayload === 'object' &&
      (Object.keys(artPayload.avatars || {}).length ||
       Object.keys(artPayload.icons || {}).length ||
       Object.keys(artPayload.armySprites || {}).length ||
       Object.keys(artPayload.animations || {}).length ||
       (artPayload.separations && artPayload.separations.entries &&
        Object.keys(artPayload.separations.entries).length));
    var hasCombatSpriteRecords = artPayload && typeof artPayload === 'object' &&
      Object.keys(artPayload.animations || {}).length;
    var hasArmySpriteRecords = artPayload && typeof artPayload === 'object' &&
      Object.keys(artPayload.armySprites || {}).length;
    var hasCustomArmySpriteRecords = hasArmySpriteRecords &&
      Object.keys(artPayload.armySprites || {}).some(function(key) {
        return artPayload.armySprites[key] &&
          artPayload.armySprites[key].retailPlane === false;
      });
    var hasSeparatedAnimationRecords = artPayload && typeof artPayload === 'object' &&
      artPayload.separations && artPayload.separations.entries &&
      Object.keys(artPayload.separations.entries).length;
    if (hasArtRecords && patch.version < 18) {
      throw new PatchFormatError(
        'Art and Animation Project data requires Project schema version 18 or newer.'
      );
    }
    if (hasCombatSpriteRecords && patch.version < 19) {
      throw new PatchFormatError(
        'Combat sprite Project data requires Project schema version 19 or newer.'
      );
    }
    if (hasArmySpriteRecords && patch.version < 32) {
      throw new PatchFormatError(
        'Army sprite Project data requires Project schema version 32 or newer.'
      );
    }
    if (hasCustomArmySpriteRecords && patch.version < 33) {
      throw new PatchFormatError(
        'Custom missing Army sprite data requires Project schema version 33 or newer.'
      );
    }
    if (hasSeparatedAnimationRecords && patch.version < 21) {
      throw new PatchFormatError(
        'Separated animation Project data requires Project schema version 21 or newer.'
      );
    }
    if (hasSeparatedAnimationRecords && artPayload.separations.schemaVersion >= 2 &&
        patch.version < 22) {
      throw new PatchFormatError(
        'Structural private animation data requires Project schema version 22 or newer.'
      );
    }
    if (hasSeparatedAnimationRecords && artPayload.separations.schemaVersion >= 3 &&
        patch.version < 23) {
      throw new PatchFormatError(
        'Private animation frame removal requires Project schema version 23 or newer.'
      );
    }
    if (hasSeparatedAnimationRecords && artPayload.separations.schemaVersion >= 4 &&
        patch.version < 24) {
      throw new PatchFormatError(
        'Private animation frame insertion and reorder require Project schema version 24 or newer.'
      );
    }
    if (OB64.art) {
      try {
        preparedArt = OB64.art.prepareProjectPayload(
          rom, artPayload
        );
      } catch (artError) {
        throw new PatchFormatError('Art and Animation Project data is invalid: ' + artError.message);
      }
    } else if (p.art && (Object.keys(p.art.avatars || {}).length ||
        Object.keys(p.art.icons || {}).length ||
        Object.keys(p.art.armySprites || {}).length ||
        Object.keys(p.art.animations || {}).length ||
        (p.art.separations && p.art.separations.entries &&
          Object.keys(p.art.separations.entries).length))) {
      throw new PatchFormatError('This editor build cannot load Art and Animation Project records.');
    }
    var spriteLibraryPayload = Object.prototype.hasOwnProperty.call(p, 'spriteLibrary')
      ? p.spriteLibrary : undefined;
    var hasSpriteLibraryRecords = spriteLibraryPayload &&
      Array.isArray(spriteLibraryPayload.assets) && spriteLibraryPayload.assets.length;
    if (hasSpriteLibraryRecords && patch.version < 31) {
      throw new PatchFormatError(
        'Sprite Editor Project data requires Project schema version 31 or newer.');
    }
    var preparedSpriteLibrary = { assets: [], count: 0 };
    if (OB64.spriteLibrary) {
      try {
        preparedSpriteLibrary = OB64.spriteLibrary.prepareProjectPayload(
          spriteLibraryPayload);
      } catch (spriteLibraryError) {
        throw new PatchFormatError('Sprite Editor Project data is invalid: ' +
          spriteLibraryError.message);
      }
    } else if (hasSpriteLibraryRecords) {
      throw new PatchFormatError(
        'This editor build cannot load Sprite Editor Project records.');
    }

    var warnings = [];
    if (patch.rom_hint && patch.rom_hint.archives_count &&
        patch.rom_hint.archives_count !== rom.archives.length) {
      warnings.push('ROM archive count differs from patch (patch: ' +
        patch.rom_hint.archives_count + ', rom: ' + rom.archives.length +
        '). Applying anyway.');
    }
    if (patch.rom_hint && patch.rom_hint.shop_count &&
        patch.rom_hint.shop_count !== rom.shops.length) {
      warnings.push('ROM shop count differs from patch (patch: ' +
        patch.rom_hint.shop_count + ', rom: ' + rom.shops.length + ').');
    }

    var shopsApplied = 0;
    var pricesApplied = 0;
    var itemStatsApplied = 0;
    var classDefsApplied = 0;
    var neutralSlicesApplied = 0;
    var terrainRatesApplied = 0;
    var creatureDropsApplied = 0;
    var consumablesApplied = 0;
    var itemDescriptionsApplied = 0;
    var consumableDescriptionsApplied = 0;
    var classDescriptionsApplied = 0;
    var actionDescriptionsApplied = 0;
    var statGatesApplied = 0;
    var neutralGlobalRateApplied = 0;
    var neutralSliceRatesApplied = 0;
    var neutralScenarioRatesApplied = 0;
    var customNeutralSquadsApplied = 0;
    var weightedDropRatesApplied = 0;
    var toolsApplied = 0;
    var squadOverridesApplied = 0;
    var scenarioApplied = 0;
    var cutscenesApplied = 0;
    var consumableEffectsApplied = 0;
    var combatAnimationOverridesApplied = 0;
    var artApplied = 0;
    var spriteLibraryApplied = 0;

    // Shops.
    var shopsPatch = p.shops || {};
    for (var k in shopsPatch) {
      var idx = parseInt(k, 10);
      if (!isFinite(idx) || idx < 0 || idx >= rom.shops.length) {
        warnings.push('Patch references shop #' + k + ' but ROM only has ' + rom.shops.length + ' shops - skipping.');
        continue;
      }
      var entry = shopsPatch[k];
      if (!entry || (!Array.isArray(entry.items) && !Array.isArray(entry.consumables))) continue;
      if (Array.isArray(entry.items)) rom.shops[idx].items = entry.items.slice();
      // Absent in v9 and older projects: retain the currently parsed vanilla
      // (or already-patched) consumable list in that case.
      if (Array.isArray(entry.consumables)) {
        rom.shops[idx].consumables = entry.consumables.slice();
      }
      rom.shops[idx].runtimeOverride = true;
      shopsApplied++;
    }
    if (shopsApplied > 0) dirtyFlags.shops = true;

    // Item prices (legacy v2 key).
    var pricesPatch = p.item_prices || {};
    for (var ks in pricesPatch) {
      var id = parseInt(ks, 10);
      if (!isFinite(id) || id < 0 || id >= rom.itemStats.length) continue;
      var stat = rom.itemStats[id];
      if (!stat) continue;
      var price = pricesPatch[ks];
      if (typeof price !== 'number' || price < 0 || price > 65535) continue;
      stat.price = price;
      pricesApplied++;
    }
    if (pricesApplied > 0) dirtyFlags.items = true;

    // Item stat fields.
    var itemsPatch = p.items || {};
    for (var ik in itemsPatch) {
      var itemId = parseInt(ik, 10);
      if (!isFinite(itemId) || itemId < 0 || itemId >= rom.itemStats.length) {
        warnings.push('Patch references item #' + ik + ' but ROM only has ' + rom.itemStats.length + ' item-stat records - skipping.');
        continue;
      }
      var item = rom.itemStats[itemId];
      if (!item) continue;
      var entryItem = itemsPatch[ik];
      if (!entryItem || typeof entryItem !== 'object') continue;
      var appliedThisItem = false;
      for (var fi = 0; fi < ITEM_APPLY_FIELDS.length; fi++) {
        var field = ITEM_APPLY_FIELDS[fi];
        if (!Object.prototype.hasOwnProperty.call(entryItem, field)) continue;
        if (applyItemField(item, field, entryItem[field])) appliedThisItem = true;
      }
      if (appliedThisItem) itemStatsApplied++;
    }
    if (itemStatsApplied > 0) dirtyFlags.items = true;

    // Class definition byte patches. Keys are class IDs; record_index is kept
    // as a human/debug hint and is accepted when present.
    var classPatch = p.classDefs || {};
    var warnedLegacyClassHeader = false;
    for (var ck in classPatch) {
      var cid = parseInt(ck, 10);
      var entryClass = classPatch[ck];
      if (!entryClass || typeof entryClass !== 'object') continue;
      var recIdx = Number.isInteger(entryClass.record_index) ? entryClass.record_index : cid + 1;
      if (!isFinite(cid) || cid <= 0 || recIdx < 0 || recIdx >= rom.classDefs.length) {
        warnings.push('Patch references class #' + ck + ' but no matching class-def record exists - skipping.');
        continue;
      }
      if (recIdx !== cid + 1) {
        warnings.push('Patch class #' + ck + ' has record_index ' + recIdx +
          ' (expected ' + (cid + 1) + '); applying the explicit record_index.');
      }
      var classDef = rom.classDefs[recIdx];
      if (!classDef) {
        warnings.push('Patch references class #' + ck + ' but its class-def record is missing - skipping.');
        continue;
      }
      var bytes = entryClass.bytes || {};
      var appliedThisClass = false;
      for (var bo in bytes) {
        var byteOff = parseInt(bo, 10);
        var byteVal = bytes[bo];
        if (!isFinite(byteOff) || byteOff < 0 || byteOff >= OB64.CLASS_DEF_RECORD_SIZE) continue;
        if (byteOff >= 60 && byteOff <= 63 && patch.version < 9) {
          warnings.push('Pre-v9 patch attempted to edit class #' + ck + ' pointer byte B' + byteOff + '; skipped.');
          continue;
        }
        if (patch.version < 6 && byteOff >= 65 && byteOff <= 71 && !warnedLegacyClassHeader) {
          warnings.push('This patch predates class-header migration v6. Class-def bytes B65-B71 now target the current class name-framed header, not the old shifted tail labels.');
          warnedLegacyClassHeader = true;
        }
        if (!isPatchByte(byteVal)) continue;
        applyClassDefByte(classDef, byteOff, byteVal & 0xFF);
        appliedThisClass = true;
      }
      if (appliedThisClass) {
        OB64.refreshClassDefClassification(classDef);
        classDefsApplied++;
      }
    }
    if (classDefsApplied > 0) dirtyFlags.classDefs = true;

    // Neutral encounter slices and terrain-rate rows.
    var neutralApplied = applyNeutralEncounterPatch(rom, p.neutral_encounters, warnings);
    neutralSlicesApplied = neutralApplied.slices;
    terrainRatesApplied = neutralApplied.terrainRates;
    neutralSliceRatesApplied = neutralApplied.sliceRates;
    neutralScenarioRatesApplied = neutralApplied.scenarioRates;
    customNeutralSquadsApplied = neutralApplied.customSquads;
    if (neutralSlicesApplied > 0 || terrainRatesApplied > 0) dirtyFlags.encounters = true;
    if (neutralSliceRatesApplied > 0 || neutralScenarioRatesApplied > 0 ||
        customNeutralSquadsApplied > 0) dirtyFlags.neutralRuntime = true;

    var creatureDropApplied = applyCreatureDropsPatch(rom, p.creatureDrops, warnings);
    creatureDropsApplied = creatureDropApplied.records;
    weightedDropRatesApplied = creatureDropApplied.weighted;
    if (creatureDropsApplied > 0) dirtyFlags.creatureDrops = true;
    if (weightedDropRatesApplied > 0) dirtyFlags.neutralRuntime = true;

    consumablesApplied = applyConsumablesPatch(rom, p.consumables, warnings);
    if (consumablesApplied > 0) dirtyFlags.consumables = true;

    if (preparedDescriptions) {
      var descriptionKinds = [
        ['items', 'itemDescriptions', 'itemDescriptions'],
        ['consumables', 'consumableDescriptions', 'consumableDescriptions'],
        ['classes', 'classDescriptions', 'classDescriptions'],
        ['actions', 'actionDescriptions', 'actionDescriptions']
      ];
      for (var descriptionIndex = 0;
          descriptionIndex < descriptionKinds.length; descriptionIndex++) {
        var descriptionKind = descriptionKinds[descriptionIndex];
        var descriptionCount = preparedDescriptions.counts[descriptionKind[0]] || 0;
        if (!descriptionCount) continue;
        rom[descriptionKind[1]] = preparedDescriptions.blocks[descriptionKind[1]];
        dirtyFlags[descriptionKind[2]] = OB64.descriptionCodec.hasTextChanges(
          rom[descriptionKind[1]],
          rom.original && rom.original.descriptions &&
            rom.original.descriptions[descriptionKind[0]]
        );
      }
      itemDescriptionsApplied = preparedDescriptions.counts.items || 0;
      consumableDescriptionsApplied = preparedDescriptions.counts.consumables || 0;
      classDescriptionsApplied = preparedDescriptions.counts.classes || 0;
      actionDescriptionsApplied = preparedDescriptions.counts.actions || 0;
    }

    statGatesApplied = applyStatGatesPatch(rom, p.statGates, warnings);
    if (statGatesApplied > 0) dirtyFlags.statGates = true;

    // Neutral global encounter roll.
    var globalPatch = p.neutral_global_rate || null;
    if (globalPatch) {
      if (!rom.neutralEncounters || !rom.neutralEncounters.globalRate) {
        warnings.push('Patch includes neutral_global_rate, but this ROM parse has no neutral encounter data - skipping.');
      } else {
        var globalRate = rom.neutralEncounters.globalRate;
        var divisor = Number.isInteger(globalPatch.divisor) && globalPatch.divisor > 0
          ? globalPatch.divisor
          : (Number.isInteger(globalRate.divisor) && globalRate.divisor > 0
            ? globalRate.divisor : OB64.NEUTRAL_GLOBAL_VANILLA_DIVISOR);
        function branchMicro(entry, vanillaPassCount) {
          if (!entry || typeof entry !== 'object') return null;
          if (Number.isInteger(entry.pass_count) && entry.pass_count >= 1) {
            return (entry.pass_count * 1000000) / divisor;
          }
          if (typeof entry.micro_basis_points === 'number') return entry.micro_basis_points;
          if (typeof entry.multiplier === 'number') {
            return (Math.max(1, entry.multiplier * vanillaPassCount) * 1000000) / divisor;
          }
          if (typeof entry.basis_points === 'number') return entry.basis_points * 100;
          if (typeof entry.percent === 'number') return entry.percent * 10000;
          return null;
        }
        var normalMicro = branchMicro(globalPatch.normal, 51);
        var alternateMicro = branchMicro(globalPatch.alternate, 11);
        var isV26Shape = normalMicro != null || alternateMicro != null;
        if (!isV26Shape) {
          normalMicro = branchMicro(globalPatch, 51);
          alternateMicro = normalMicro;
        } else {
          if (normalMicro == null) normalMicro = OB64.neutralGlobalBranchMicroBasisPoints(globalRate, 'normal');
          if (alternateMicro == null) alternateMicro = OB64.neutralGlobalBranchMicroBasisPoints(globalRate, 'alternate');
        }
        if (normalMicro == null || alternateMicro == null || !isFinite(normalMicro) || !isFinite(alternateMicro)) {
          warnings.push('Patch neutral_global_rate is missing valid branch rate values - skipping.');
        } else {
          globalRate.divisor = divisor;
          globalRate.linked = isV26Shape ? !!globalPatch.linked : true;
          globalRate.normalMicroBasisPoints = clampMicroBasisPoints(normalMicro);
          globalRate.alternateMicroBasisPoints = globalRate.linked
            ? globalRate.normalMicroBasisPoints : clampMicroBasisPoints(alternateMicro);
          globalRate.normalPassCount = Math.max(1, Math.min(0x8000,
            Math.round((globalRate.normalMicroBasisPoints * divisor) / 1000000)));
          globalRate.alternatePassCount = globalRate.linked ? globalRate.normalPassCount
            : Math.max(1, Math.min(0x8000,
              Math.round((globalRate.alternateMicroBasisPoints * divisor) / 1000000)));
          globalRate.microBasisPoints = globalRate.normalMicroBasisPoints;
          globalRate.basisPoints = globalRate.microBasisPoints / 100;
          globalRate.multiplier = globalRate.normalPassCount / 51;
          globalRate.normalModified = true;
          globalRate.alternateModified = true;
          globalRate.modified = true;
          neutralGlobalRateApplied = 1;
          dirtyFlags.encounters = true;
        }
      }
    }

    // Tools-tab feature toggles (v5). Stage the desired state; the export
    // pipeline performs the actual byte writes (or restores).
    var toolsPatch = p.tools || {};
    for (var tk in toolsPatch) {
      if (!OB64.tools || !rom.tools) break;
      var toolFeature = OB64.tools.getFeature(tk);
      if (!toolFeature) {
        warnings.push('Patch enables unknown tool "' + tk + '" - this editor build does not have it. Skipping.');
        continue;
      }
      if (rom.tools.initial[tk] === 'foreign') {
        warnings.push('Tool "' + toolFeature.name + '" cannot be toggled: its ROM bytes match neither retail nor this build. Skipping.');
        continue;
      }
      if (rom.tools.initial[tk] === 'unsupported' || rom.tools.disabledReason) {
        warnings.push('Tool "' + toolFeature.name + '" is not available for this ROM revision. Skipping.');
        continue;
      }
      var wantTool = !!toolsPatch[tk];
      if (rom.tools.desired[tk] !== wantTool) {
        rom.tools.desired[tk] = wantTool;
        toolsApplied++;
      }
    }
    if (toolsApplied > 0 && OB64.tools) {
      dirtyFlags.tools = OB64.tools.pendingChanges(rom) > 0;
    }

    var squadOverridePatch = p.squadOverrides || p.squad_overrides;
    if (squadOverridePatch && rom.layout && rom.layout.supportsSquadOverrides === false) {
      warnings.push('Patch includes squad overrides, but this ROM revision does not have a verified runtime override hook yet. Skipping.');
    } else {
      squadOverridesApplied = applySquadOverridesPatch(rom, squadOverridePatch, warnings);
    }
    if (squadOverridesApplied > 0) dirtyFlags.squadOverrides = true;

    if (scenarioPatch) {
      scenarioApplied = applyScenarioPatch(rom, scenarioPatch, warnings, preparedScenario);
      if (scenarioApplied > 0) {
        dirtyFlags.scenario = true;
        dirtyFlags.squadOverrides = true;
      }
    }

    if (cutscenePatch) {
      try {
        cutscenesApplied = OB64.cutsceneProject.applyPrepared(
          rom.cutsceneStudio, preparedCutscenes);
      } catch (cutsceneError) {
        throw new PatchFormatError('Cutscene Studio Project data is invalid: ' +
          cutsceneError.message);
      }
      if (cutscenesApplied > 0) dirtyFlags.cutscenes = true;
    }

    if (validatedEffects.modelCount) {
      consumableEffectsApplied = OB64.consumableEffects.applyProjectPayload(
        rom.consumableEffects,
        validatedEffects
      );
      if (consumableEffectsApplied > 0) {
        dirtyFlags.consumableEffects = OB64.consumableEffects.refreshPending(
          rom.consumableEffects
        );
      }
    }
    if (validatedSelectorEntries) {
      combatAnimationOverridesApplied = OB64.combatAnimationOverrides.applyProjectPayload(
        rom.combatAnimationOverrides, validatedSelectorEntries
      );
      dirtyFlags.combatAnimationOverrides = rom.combatAnimationOverrides.dirty;
    }
    if (preparedArt.count) {
      artApplied = OB64.art.applyPreparedProjectPayload(rom, preparedArt);
      dirtyFlags.art = OB64.art.hasPendingExport(rom.art);
    }
    if (preparedSpriteLibrary.count) {
      spriteLibraryApplied = OB64.spriteLibrary.applyPreparedProjectPayload(
        rom, preparedSpriteLibrary);
    }

    return {
      applied: {
        shops: shopsApplied,
        prices: pricesApplied,
        itemStats: itemStatsApplied,
        classDefs: classDefsApplied,
        neutralSlices: neutralSlicesApplied,
        terrainRates: terrainRatesApplied,
        creatureDrops: creatureDropsApplied,
        consumables: consumablesApplied,
        itemDescriptions: itemDescriptionsApplied,
        consumableDescriptions: consumableDescriptionsApplied,
        classDescriptions: classDescriptionsApplied,
        actionDescriptions: actionDescriptionsApplied,
        statGates: statGatesApplied,
        neutralGlobalRate: neutralGlobalRateApplied,
        neutralSliceRates: neutralSliceRatesApplied,
        neutralScenarioRates: neutralScenarioRatesApplied,
        customNeutralSquads: customNeutralSquadsApplied,
        weightedDropRates: weightedDropRatesApplied,
        tools: toolsApplied,
        squadOverrides: squadOverridesApplied,
        scenario: scenarioApplied,
        cutscenes: cutscenesApplied,
        consumableEffects: consumableEffectsApplied,
        combatAnimationOverrides: combatAnimationOverridesApplied,
        art: artApplied,
        spriteLibrary: spriteLibraryApplied
      },
      warnings: warnings,
    };
  }

  // --------------------------------------------------------------
  // downloadPatch(patch, [filename]) -> triggers browser download of JSON.
  // --------------------------------------------------------------
  function downloadPatch(patch, filename) {
    if (!filename) {
      var ts = (patch.created_at || new Date().toISOString()).replace(/[:.]/g, '-');
      filename = 'ob64_project_' + ts + '.json';
    }
    var blob = new Blob([JSON.stringify(patch, null, 2)], { type: 'application/json' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
  }

  // --------------------------------------------------------------
  // parsePatchFile(fileText) -> validates + returns parsed project/patch object.
  // Throws on invalid JSON or wrong format.
  // --------------------------------------------------------------
  function parsePatchFile(fileText) {
    var parsed;
    try {
      parsed = JSON.parse(fileText);
    } catch (e) {
      throw new PatchFormatError('File is not valid JSON: ' + e.message);
    }
    if (!parsed || typeof parsed !== 'object') {
      throw new PatchFormatError('Project file is not a JSON object');
    }
    if (parsed.format === 'ob64-scenario-project') return patchFromScenarioProject(parsed);
    if (parsed.format !== PATCH_FORMAT) {
      throw new PatchFormatError('File is not an OB64 project (format="' +
        parsed.format + '", expected "' + PATCH_FORMAT + '" or "ob64-scenario-project")');
    }
    return parsed;
  }

  function blankProjectSummary() {
    return {
      shops_modified: 0,
      item_prices_modified: 0,
      item_stats_modified: 0,
      class_defs_modified: 0,
      neutral_slices_modified: 0,
      terrain_rates_modified: 0,
      neutral_slice_rates_modified: 0,
      neutral_scenario_rates_modified: 0,
      custom_neutral_squads_modified: 0,
      creature_drop_records_modified: 0,
      weighted_drop_classes_modified: 0,
      consumables_modified: 0,
      item_descriptions_modified: 0,
      consumable_descriptions_modified: 0,
      class_descriptions_modified: 0,
      action_descriptions_modified: 0,
      stat_gates_modified: 0,
      neutral_global_rate_modified: 0,
      tools_modified: 0,
      squad_overrides_modified: 0,
      scenario_modified: 0,
      cutscenes_modified: 0,
      consumable_effect_models_modified: 0,
      combat_animation_overrides_modified: 0,
      avatar_art_modified: 0,
      item_icon_art_modified: 0,
      combat_sprite_art_modified: 0,
      army_sprite_art_modified: 0,
      separated_animation_sequences_modified: 0,
      sprite_library_assets: 0,
    };
  }

  function blankProjectPatches() {
    return {
      shops: {},
      item_prices: {},
      items: {},
      classDefs: {},
      neutral_encounters: {
        scenario_slices: {}, terrain_rates: {},
        scenario_rates: {}, custom_squads: {}
      },
      creatureDrops: {},
      consumables: {},
      descriptions: { items: {}, consumables: {}, classes: {}, actions: {} },
      statGates: {},
      neutral_global_rate: null,
      tools: {},
      squadOverrides: {},
      scenario: null,
      cutscenes: null,
      consumableEffects: {},
      combatAnimationOverrides: null,
      art: {
        schemaVersion: 5, avatars: {}, icons: {}, armySprites: {},
        animations: {}, separations: null
      },
      spriteLibrary: { schemaVersion: 1, assets: [] }
    };
  }

  function patchFromScenarioProject(project) {
    var summary = blankProjectSummary();
    summary.scenario_modified = scenarioPatchCount(project);
    var patches = blankProjectPatches();
    patches.scenario = project;
    return {
      format: PATCH_FORMAT,
      version: PATCH_VERSION,
      created_at: project.created_at || new Date().toISOString(),
      editor_version: '2026-07-24',
      source: 'legacy scenario project',
      summary: summary,
      patches: patches,
    };
  }

  // --------------------------------------------------------------
  // Helpers
  // --------------------------------------------------------------
  function arraysEqual(a, b) {
    if (a === b) return true;
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function bytesToHex(bytes) {
    var out = '';
    for (var i = 0; bytes && i < bytes.length; i++) {
      var s = (bytes[i] & 0xFF).toString(16).toUpperCase();
      if (s.length < 2) s = '0' + s;
      out += s;
    }
    return out;
  }

  function hexToBytes(hex, expectedLen) {
    if (typeof hex !== 'string') return null;
    var clean = hex.replace(/\s+/g, '');
    if (clean.length % 2 !== 0) return null;
    var len = clean.length / 2;
    if (expectedLen && len !== expectedLen) return null;
    var out = new Uint8Array(len);
    for (var i = 0; i < len; i++) {
      var v = parseInt(clean.substr(i * 2, 2), 16);
      if (!isFinite(v)) return null;
      out[i] = v & 0xFF;
    }
    return out;
  }

  function squadScenarioById(sid) {
    var data = OB64.SQUAD_DATA || {};
    var list = data.scenarios || [];
    for (var i = 0; i < list.length; i++) if (list[i].id === sid) return list[i];
    return null;
  }

  function squadVanillaRecord(sid, eid) {
    var scn = squadScenarioById(sid);
    if (!scn || !scn.squads) return null;
    for (var i = 0; i < scn.squads.length; i++) {
      if (scn.squads[i].e === eid) return hexToBytes(scn.squads[i].rec, 35);
    }
    return null;
  }

  function parseSquadOverrideKey(key, entry) {
    var sid = entry && Number.isInteger(entry.scenario_id) ? entry.scenario_id : null;
    var eid = entry && Number.isInteger(entry.edat_id) ? entry.edat_id : null;
    if (sid == null || eid == null) {
      var parts = String(key).split(':');
      if (parts.length === 2) {
        sid = parseInt(parts[0], 10);
        eid = parseInt(parts[1], 10);
      }
    }
    if (!isFinite(sid) || !isFinite(eid)) return null;
    return { sid: sid, eid: eid, key: sid + ':' + eid };
  }

  function collectSquadOverridePatch(rom) {
    var out = {};
    if (!rom.squadOverrides) return out;
    for (var k in rom.squadOverrides) {
      var parsed = parseSquadOverrideKey(k, null);
      if (!parsed) continue;
      var rec = rom.squadOverrides[k];
      if (!rec || rec.length !== 35) continue;
      var vanilla = squadVanillaRecord(parsed.sid, parsed.eid);
      if (vanilla && arraysEqual(rec, vanilla)) continue;
      out[parsed.key] = {
        scenario_id: parsed.sid,
        edat_id: parsed.eid,
        record: bytesToHex(rec)
      };
      if (vanilla) out[parsed.key].original = bytesToHex(vanilla);
    }
    return out;
  }

  function applySquadOverridesPatch(rom, patchObj, warnings) {
    if (!patchObj || typeof patchObj !== 'object') return 0;
    rom.squadOverrides = rom.squadOverrides || {};
    var applied = 0;
    for (var k in patchObj) {
      var entry = patchObj[k];
      var parsed = parseSquadOverrideKey(k, entry);
      if (!parsed) {
        warnings.push('Patch squad override key "' + k + '" is invalid - skipping.');
        continue;
      }
      var recHex = typeof entry === 'string' ? entry : entry && entry.record;
      var rec = hexToBytes(recHex, 35);
      if (!rec) {
        warnings.push('Patch squad override ' + parsed.key + ' is not a 35-byte record - skipping.');
        continue;
      }
      var vanilla = squadVanillaRecord(parsed.sid, parsed.eid);
      if (!vanilla) {
        warnings.push('Patch squad override ' + parsed.key + ' does not match a known runtime-key EDAT row - skipping.');
        continue;
      }
      if (entry && entry.original) {
        var original = hexToBytes(entry.original, 35);
        if (original && !arraysEqual(original, vanilla)) {
          warnings.push('Patch squad override ' + parsed.key + ' was based on a different original record; applying replacement anyway.');
        }
      }
      rom.squadOverrides[parsed.key] = rec;
      applied++;
    }
    return applied;
  }

  function scenarioPatchCount(project) {
    if (!project) return 0;
    var n = 0;
    n += Object.keys(project.modifiedEsets || {}).length;
    n += (project.levelBaseEdits || []).length;
    n += (project.squadLevelCopies || []).length;
    n += Object.keys(project.modifiedTreasures || {}).length;
    n += (project.addedSquads || []).length;
    Object.keys(project.siteAllegiances || {}).forEach(function(key) {
      n += Object.keys(project.siteAllegiances[key] || {}).length;
    });
    Object.keys(project.strongholdFields || {}).forEach(function(index) {
      var edit = project.strongholdFields[index] || {};
      if (Object.prototype.hasOwnProperty.call(edit, 'population')) n++;
      if (Object.prototype.hasOwnProperty.call(edit, 'morale')) n++;
    });
    return n;
  }

  function collectScenarioPatch(rom) {
    if (!OB64.scenario || !OB64.scenario.collectProject) return null;
    var project = OB64.scenario.collectProject(rom);
    return scenarioPatchCount(project) ? project : null;
  }

  function applyScenarioPatch(rom, project, warnings, prepared) {
    if (!project || typeof project !== 'object') return 0;
    if (!OB64.scenario || !OB64.scenario.loadProject) {
      warnings.push('Patch includes scenario data, but this editor build has no Scenario tab loader - skipping.');
      return 0;
    }
    if (prepared && OB64.scenario.applyPreparedProject) {
      OB64.scenario.applyPreparedProject(rom, prepared);
      return scenarioPatchCount(project);
    }
    try {
      OB64.scenario.loadProject(rom, project);
      return scenarioPatchCount(project);
    } catch (err) {
      warnings.push('Scenario patch failed: ' + (err && err.message ? err.message : err));
      return 0;
    }
  }

  function snapshotItemStat(stat) {
    if (!stat) return null;
    var out = {};
    for (var i = 0; i < ITEM_APPLY_FIELDS.length; i++) {
      var field = ITEM_APPLY_FIELDS[i];
      if (typeof stat[field] === 'number') out[field] = stat[field];
    }
    return out;
  }

  function diffItemStat(stat, orig) {
    if (!stat || !orig) return null;
    var out = {};
    for (var i = 0; i < ITEM_PATCH_FIELDS.length; i++) {
      var field = ITEM_PATCH_FIELDS[i];
      if (typeof stat[field] === 'number' && stat[field] !== orig[field]) {
        out[field] = stat[field];
      }
    }
    return Object.keys(out).length ? out : null;
  }

  function diffByteMap(current, original) {
    if (!current || !original) return null;
    var out = {};
    for (var k in current) {
      if (current[k] !== original[k]) out[k] = current[k];
    }
    return Object.keys(out).length ? out : null;
  }

  function snapshotNeutralRateBranch(branch) {
    branch = branch || {};
    var mode = branch.mode === 'override' || branch.mode === 'disabled'
      ? branch.mode : 'inherit';
    return {
      mode: mode,
      passCount: mode === 'override' ? Number(branch.passCount) : null,
      divisor: mode === 'override' ? Number(branch.divisor) : null
    };
  }

  function snapshotNeutralEncounters(encounters) {
    var out = {
      records: [], terrainRates: [], sliceRates: [],
      scenarioRates: [], customSquads: []
    };
    if (!encounters) return out;
    var records = encounters.records || [];
    for (var i = 0; i < records.length; i++) {
      var rec = records[i];
      var slots = [];
      for (var s = 0; rec.slots && s < rec.slots.length; s++) {
        slots.push([
          clampByte(rec.slots[s].classA || 0),
          clampByte(rec.slots[s].classB || 0)
        ]);
      }
      out.records.push({ s0: rec.s0, row: rec.row, slots: slots });
      var rateOverride = rec.rateOverride || {};
      out.sliceRates.push({
        s0: rec.s0,
        normal: snapshotNeutralRateBranch(rateOverride.normal),
        alternate: snapshotNeutralRateBranch(rateOverride.alternate)
      });
    }
    var scenarioRates = encounters.scenarioRates || [];
    for (var scenarioIndex = 0; scenarioIndex < scenarioRates.length; scenarioIndex++) {
      var scenarioRate = scenarioRates[scenarioIndex];
      var scenarioOverride = scenarioRate.rateOverride || {};
      out.scenarioRates.push({
        runtimeKey: Number(scenarioRate.runtimeKey),
        neutralBase: Number(scenarioRate.neutralBase),
        slice: scenarioRate.slice == null ? null : Number(scenarioRate.slice),
        normal: snapshotNeutralRateBranch(scenarioOverride.normal),
        alternate: snapshotNeutralRateBranch(scenarioOverride.alternate)
      });
    }
    var customSquads = encounters.customSquads || [];
    for (var customIndex = 0; customIndex < customSquads.length; customIndex++) {
      var custom = customSquads[customIndex] || {};
      var persuasion = custom.persuasion || {};
      var persuasionChance = Number(persuasion.chance);
      if (!Number.isInteger(persuasionChance) ||
          persuasionChance < 0 || persuasionChance > 100) persuasionChance = 10;
      var retreat = custom.retreat || {};
      var retreatHpThreshold = Number(retreat.hpThreshold);
      if (!Number.isInteger(retreatHpThreshold) ||
          retreatHpThreshold < 0 || retreatHpThreshold > 100) retreatHpThreshold = 0;
      var members = (custom.members || []).map(function(member) {
        return {
          classId: Number(member.classId),
          levelOffsetRaw: Number(member.levelOffsetRaw),
          cell: Number(member.cell),
          cohort: String(member.cohort || '').toUpperCase()
        };
      });
      var equipment = custom.equipment || {};
      var classBonuses = Array.isArray(persuasion.classBonuses)
        ? persuasion.classBonuses.map(function(bonus) {
          return { classId: Number(bonus.classId), bonus: Number(bonus.bonus) };
        }) : [];
      var rewardSource = custom.rewards && Array.isArray(custom.rewards.slots)
        ? custom.rewards.slots : [];
      out.customSquads.push({
        profileId: Number(custom.profileId),
        runtimeKey: Number(custom.runtimeKey),
        slice: Number(custom.slice),
        terrainSlot: Number(custom.terrainSlot),
        members: members,
        equipment: {
          A: Array.isArray(equipment.A) ? equipment.A.slice(0, 2).map(Number) : [0, 0],
          B: Array.isArray(equipment.B) ? equipment.B.slice(0, 2).map(Number) : [0, 0],
          C: Array.isArray(equipment.C) ? equipment.C.slice(0, 2).map(Number) : [0, 0]
        },
        label: custom.label == null ? 'Bandits!' : String(custom.label),
        persuasion: {
          mode: 'fixed',
          chance: persuasionChance,
          classBonuses: classBonuses
        },
        retreat: { hpThreshold: retreatHpThreshold },
        rewards: {
          slots: rewardSource.map(function(slot) {
            return { raw: Number(slot.raw), weight: Number(slot.weight) };
          })
        }
      });
    }
    out.customSquads.sort(function(a, b) {
      return a.runtimeKey - b.runtimeKey || a.terrainSlot - b.terrainSlot;
    });
    var rates = encounters.terrainRates && encounters.terrainRates.entries
      ? encounters.terrainRates.entries
      : [];
    for (var r = 0; r < rates.length; r++) {
      out.terrainRates.push({
        terrainByte: rates[r].terrainByte,
        rate: clampByte(rates[r].rate || 0),
        rawLookup: clampByte(rates[r].rawLookup || 0)
      });
    }
    return out;
  }

  function diffNeutralEncounters(encounters, original) {
    var out = {
      scenario_slices: {}, terrain_rates: {},
      scenario_rates: {}, custom_squads: {}
    };
    if (!encounters || !original) return out;
    var current = snapshotNeutralEncounters(encounters);

    function branchPayload(branch) {
      var payload = { mode: branch.mode };
      if (branch.mode === 'override') {
        payload.pass_count = branch.passCount;
        payload.divisor = branch.divisor;
      }
      return payload;
    }

    for (var i = 0; i < current.records.length; i++) {
      var curRec = current.records[i];
      var origRec = original.records && original.records[i];
      if (!origRec) continue;
      var slotPatch = {};
      for (var s = 0; s < curRec.slots.length; s++) {
        var curSlot = curRec.slots[s];
        var origSlot = origRec.slots && origRec.slots[s];
        if (!origSlot || curSlot[0] !== origSlot[0] || curSlot[1] !== origSlot[1]) {
          slotPatch[String(s)] = curSlot.slice();
        }
      }
      if (Object.keys(slotPatch).length) {
        out.scenario_slices[String(curRec.s0)] = {
          row: curRec.row,
          slots: slotPatch
        };
      }
    }

    for (var tr = 0; tr < current.terrainRates.length; tr++) {
      var curRate = current.terrainRates[tr];
      var origRate = original.terrainRates && original.terrainRates[tr];
      if (!origRate || curRate.rate !== origRate.rate || curRate.rawLookup !== origRate.rawLookup) {
        out.terrain_rates[String(curRate.terrainByte)] = {
          rate: curRate.rate,
          rawLookup: curRate.rawLookup
        };
      }
    }

    // slice_rates is retired. New Projects persist only direct scenario rows,
    // and imports ignore that field instead of creating a hidden override.

    var originalScenarioRates = {};
    (original.scenarioRates || []).forEach(function(row) {
      originalScenarioRates[String(row.runtimeKey)] = row;
    });
    for (var scenarioRateIndex = 0;
        scenarioRateIndex < current.scenarioRates.length; scenarioRateIndex++) {
      var currentScenarioRate = current.scenarioRates[scenarioRateIndex];
      var originalScenarioRate = originalScenarioRates[String(currentScenarioRate.runtimeKey)];
      if (!originalScenarioRate ||
          JSON.stringify(currentScenarioRate.normal) !== JSON.stringify(originalScenarioRate.normal) ||
          JSON.stringify(currentScenarioRate.alternate) !== JSON.stringify(originalScenarioRate.alternate)) {
        out.scenario_rates[String(currentScenarioRate.runtimeKey)] = {
          slice: currentScenarioRate.slice,
          normal: branchPayload(currentScenarioRate.normal),
          alternate: branchPayload(currentScenarioRate.alternate)
        };
      }
    }

    function customKey(profile) {
      return String(profile.runtimeKey) + ':' + String(profile.terrainSlot);
    }
    function customPayload(profile) {
      return {
        profile_id: profile.profileId,
        runtime_key: profile.runtimeKey,
        slice: profile.slice,
        terrain_slot: profile.terrainSlot,
        members: profile.members.map(function(member) {
          return {
            class_id: member.classId,
            level_offset_raw: member.levelOffsetRaw,
            cell: member.cell,
            cohort: member.cohort
          };
        }),
        equipment: {
          A: profile.equipment.A.slice(),
          B: profile.equipment.B.slice(),
          C: profile.equipment.C.slice()
        },
        label: profile.label,
        persuasion: {
          mode: 'fixed',
          chance: profile.persuasion.chance,
          class_bonuses: profile.persuasion.classBonuses.map(function(bonus) {
            return {
              class_id: bonus.classId,
              bonus_percent: bonus.bonus
            };
          })
        },
        retreat: { hp_threshold: profile.retreat.hpThreshold },
        rewards: {
          slots: profile.rewards.slots.map(function(slot) {
            return { raw: slot.raw, weight: slot.weight };
          })
        }
      };
    }
    var currentCustom = {};
    var originalCustom = {};
    current.customSquads.forEach(function(profile) { currentCustom[customKey(profile)] = profile; });
    (original.customSquads || []).forEach(function(profile) {
      originalCustom[customKey(profile)] = profile;
    });
    Object.keys(currentCustom).forEach(function(key) {
      var currentProfile = currentCustom[key];
      var originalProfile = originalCustom[key];
      if (!originalProfile || JSON.stringify(currentProfile) !== JSON.stringify(originalProfile)) {
        out.custom_squads[key] = customPayload(currentProfile);
      }
    });
    Object.keys(originalCustom).forEach(function(key) {
      if (!currentCustom[key]) out.custom_squads[key] = null;
    });
    return out;
  }

  function dropSlotRaw(slot) {
    if (!slot) return 0;
    if (Number.isInteger(slot.raw)) return clampU16(slot.raw);
    return ((slot.itemId || 0) & 0x7FFF) | (slot.isEquipment ? 0x8000 : 0);
  }

  function snapshotCreatureDrops(drops) {
    if (!drops || !drops.records) return [];
    return drops.records.map(function(rec) {
      return {
        recordIndex: rec.recordIndex,
        padByte: clampByte(rec.padByte || 0),
        classId: clampByte(rec.classId || 0),
        slots: [
          dropSlotRaw(rec.slots && rec.slots[0]),
          dropSlotRaw(rec.slots && rec.slots[1]),
          dropSlotRaw(rec.slots && rec.slots[2])
        ],
        weights: [
          Number.isInteger(rec.slots && rec.slots[0] && rec.slots[0].weight) ? rec.slots[0].weight : 1,
          Number.isInteger(rec.slots && rec.slots[1] && rec.slots[1].weight) ? rec.slots[1].weight : 1,
          Number.isInteger(rec.slots && rec.slots[2] && rec.slots[2].weight) ? rec.slots[2].weight : 1
        ]
      };
    });
  }

  function diffCreatureDrops(drops, original) {
    var out = {};
    var current = snapshotCreatureDrops(drops);
    original = original || [];
    for (var i = 0; i < current.length; i++) {
      var cur = current[i];
      var orig = original[i];
      var weightsModified = !orig || !arraysEqual(cur.weights, orig.weights || [1, 1, 1]);
      if (!orig ||
          cur.padByte !== orig.padByte ||
          cur.classId !== orig.classId ||
          !arraysEqual(cur.slots, orig.slots) ||
          weightsModified) {
        out[String(i)] = {
          record_index: cur.recordIndex,
          padByte: cur.padByte,
          classId: cur.classId,
          slots: cur.slots.slice(),
          weights: cur.weights.slice(),
          weights_modified: weightsModified
        };
      }
    }
    return out;
  }

  function snapshotConsumable(rec) {
    if (!rec) return null;
    return {
      flagHi: clampU16(rec.flagHi || 0),
      price: clampU16(rec.price || 0),
      flagLo: [
        clampByte(rec.flagLo && rec.flagLo[0]),
        clampByte(rec.flagLo && rec.flagLo[1]),
        clampByte(rec.flagLo && rec.flagLo[2]),
        clampByte(rec.flagLo && rec.flagLo[3])
      ]
    };
  }

  function diffConsumables(consumables, original) {
    var out = {};
    original = original || [];
    for (var i = 0; consumables && i < consumables.length; i++) {
      var cur = snapshotConsumable(consumables[i]);
      var orig = original[i];
      if (!cur || !orig) continue;
      if (cur.flagHi !== orig.flagHi || cur.price !== orig.price || !arraysEqual(cur.flagLo, orig.flagLo)) {
        out[String(i)] = cur;
      }
    }
    return out;
  }

  function snapshotStatGates(statGates) {
    var out = {};
    var byClass = statGates && statGates.byClass ? statGates.byClass : {};
    for (var cid in byClass) {
      var gate = byClass[cid];
      var snap = {};
      for (var i = 0; i < STAT_GATE_FIELDS.length; i++) {
        var field = STAT_GATE_FIELDS[i];
        snap[field] = clampByte(gate[field] || 0);
      }
      out[String(cid)] = snap;
    }
    return out;
  }

  function diffStatGates(statGates, original) {
    var out = {};
    var current = snapshotStatGates(statGates);
    original = original || {};
    for (var cid in current) {
      var cur = current[cid];
      var orig = original[cid];
      if (!orig) continue;
      var fields = {};
      for (var i = 0; i < STAT_GATE_FIELDS.length; i++) {
        var field = STAT_GATE_FIELDS[i];
        if (cur[field] !== orig[field]) fields[field] = cur[field];
      }
      if (Object.keys(fields).length) out[cid] = fields;
    }
    return out;
  }

  function classDefByteMap(r) {
    var out = {};
    if (!r) return out;

    function b(off, value) { out[String(off)] = clampByte(value); }
    function u16(off, value) {
      var v = clampU16(value);
      b(off, (v >>> 8) & 0xFF);
      b(off + 1, v & 0xFF);
    }

    for (var s = 0; s < 6; s++) {
      var st = r.stats && r.stats[s] ? r.stats[s] : {};
      u16(s * 4, st.base || 0);
      var growthField = CLASS_GROWTH_FIELDS[s];
      var g1 = typeof r[growthField] === 'number' ? r[growthField] : st.g1;
      b(s * 4 + 2, g1 || 0);
      if (s < 5) {
        var rawField = CLASS_G2_RAW_FIELDS[s];
        var g2 = typeof r[rawField] === 'number' ? r[rawField] : st.g2;
        b(s * 4 + 3, g2 || 0);
      }
    }

    b(23, r.lck);
    b(24, r.alignment);
    for (var ri = 0; ri < 7; ri++) {
      b(25 + ri, r.resistances && r.resistances[ri]);
    }
    b(32, r.moveType);
    b(33, r.b33Raw);
    for (var de = 0; de < 4; de++) {
      u16(34 + de * 2, r.defaultEquip && r.defaultEquip[de] || 0);
    }
    b(42, r.b42Raw);
    b(43, r.b43Raw);
    b(44, r.frontAtks);
    b(45, r.b45Raw);
    b(46, r.midAtks);
    b(47, r.b47Raw);
    b(48, r.rearAtks !== undefined ? r.rearAtks : r.atkTypeRaw);
    b(49, r.physAtk);
    b(50, r.magAtk);
    b(51, r.physDef);
    b(52, r.magDef);
    b(53, r.baseClass !== undefined ? r.baseClass : r.flagsRaw);
    b(54, r.baseTransitionLevel !== undefined ? r.baseTransitionLevel : r.reqLevel);
    b(55, r.intermediateClass !== undefined ? r.intermediateClass : r.reqClass);
    b(56, r.finalTransitionLevel !== undefined ? r.finalTransitionLevel : r.reqClassLevel);
    b(57, r.classCopyMatch !== undefined ? r.classCopyMatch :
      (r.additionalReqRaw !== undefined ? r.additionalReqRaw : r.additionalReq));
    b(58, r.dragonElement);
    b(59, r.itemCapacity !== undefined ? r.itemCapacity : r.category);
    var namePtr = r.namePtr !== undefined ? r.namePtr : (r.ptr || 0);
    b(60, r.namePtr0Raw !== undefined ? r.namePtr0Raw : (namePtr >>> 24));
    b(61, r.namePtr1Raw !== undefined ? r.namePtr1Raw : (namePtr >>> 16));
    b(62, r.namePtr2Raw !== undefined ? r.namePtr2Raw : (namePtr >>> 8));
    b(63, r.namePtr3Raw !== undefined ? r.namePtr3Raw : namePtr);
    b(64, r.unitSize);
    b(65, r.sexOrVoice !== undefined ? r.sexOrVoice : r.spriteType);
    b(66, r.leadership !== undefined ? r.leadership : r.combatBehavior);
    b(67, r.headerPad !== undefined ? r.headerPad : r.b67Raw);
    var baseHp = r.baseHp !== undefined ? r.baseHp : (((r.b68Raw || 0) << 8) | (r.powerRating || 0));
    u16(68, baseHp);
    b(70, r.hpGrowth !== undefined ? r.hpGrowth : r.unitCount);
    b(71, r.headerTailRaw !== undefined ? r.headerTailRaw : r.b71Raw);
    return out;
  }

  function applyItemField(item, field, value) {
    if (ITEM_APPLY_FIELDS.indexOf(field) === -1) return false;
    var n = Math.round(Number(value));
    if (!isFinite(n)) return false;
    var min = ITEM_SIGNED_FIELDS[field] ? -128 : 0;
    var max = field === 'price' ? 65535 :
      (ITEM_GROWTH_FIELDS[field] ? 3 : (ITEM_SIGNED_FIELDS[field] ? 127 : 255));
    if (n < min || n > max) return false;
    item[field] = n;
    var rawField = ITEM_SIGNED_RAW_FIELDS[field];
    if (rawField) item[rawField] = n < 0 ? n + 256 : n;
    if (field === 'b12Raw') item.b12 = OB64.signedByte(n);
    if (/^b(?:28|29|30|31)Raw$/.test(field)) {
      item.namePtr = ((item.b28Raw << 24) | (item.b29Raw << 16) |
        (item.b30Raw << 8) | item.b31Raw) >>> 0;
    }
    return true;
  }

  function applyClassDefByte(r, off, value) {
    value &= 0xFF;
    if (!r.stats) r.stats = [];
    if (!r.resistances) r.resistances = [];
    if (!r.defaultEquip) r.defaultEquip = [];

    if (off >= 0 && off <= 23) {
      var statIdx = Math.floor(off / 4);
      var part = off % 4;
      if (!r.stats[statIdx]) r.stats[statIdx] = { base: 0, g1: 0, g2: 0 };
      if (part === 0 || part === 1) {
        var base = r.stats[statIdx].base || 0;
        if (part === 0) base = (value << 8) | (base & 0x00FF);
        else base = (base & 0xFF00) | value;
        r.stats[statIdx].base = base;
      } else if (part === 2) {
        r.stats[statIdx].g1 = value;
        r[CLASS_GROWTH_FIELDS[statIdx]] = value;
      } else if (off === 23) {
        r.lck = value;
        r.stats[statIdx].g2 = value;
      } else {
        r.stats[statIdx].g2 = value;
        r[CLASS_G2_RAW_FIELDS[statIdx]] = value;
      }
      return;
    }

    if (off === 24) { r.alignment = value; return; }
    if (off >= 25 && off <= 31) { r.resistances[off - 25] = value; return; }
    if (off === 32) { r.moveType = value; return; }
    if (off === 33) { r.b33Raw = value; return; }
    if (off >= 34 && off <= 41) {
      var equipIdx = Math.floor((off - 34) / 2);
      var isHi = ((off - 34) % 2) === 0;
      var equip = r.defaultEquip[equipIdx] || 0;
      r.defaultEquip[equipIdx] = isHi
        ? ((value << 8) | (equip & 0x00FF))
        : ((equip & 0xFF00) | value);
      return;
    }

    switch (off) {
      case 42: r.b42Raw = value; syncRowAttackState(r); return;
      case 43: r.b43Raw = value; syncRowAttackState(r); return;
      case 44: r.frontAtks = value; syncRowAttackState(r); return;
      case 45: r.b45Raw = value; syncRowAttackState(r); return;
      case 46: r.midAtks = value; syncRowAttackState(r); return;
      case 47: r.b47Raw = value; syncRowAttackState(r); return;
      case 48: r.rearAtks = value; r.atkTypeRaw = value; syncRowAttackState(r); syncAttacks(r); return;
      case 49: r.physAtk = value; syncAttacks(r); return;
      case 50: r.magAtk = value; syncAttacks(r); return;
      case 51: r.physDef = value; syncAttacks(r); return;
      case 52: r.magDef = value; syncAttacks(r); return;
      case 53: r.baseClass = value; r.flagsRaw = value; syncAttacks(r); return;
      case 54: r.baseTransitionLevel = value; r.reqLevel = value; return;
      case 55: r.intermediateClass = value; r.reqClass = value; return;
      case 56: r.finalTransitionLevel = value; r.reqClassLevel = value; return;
      case 57: r.classCopyMatch = value; r.additionalReqRaw = value; r.additionalReq = value; return;
      case 58: r.dragonElement = value; return;
      case 59: r.itemCapacity = value; r.category = value; return;
      case 60: r.namePtr0Raw = value; syncClassNamePointer(r); return;
      case 61: r.namePtr1Raw = value; syncClassNamePointer(r); return;
      case 62: r.namePtr2Raw = value; syncClassNamePointer(r); return;
      case 63: r.namePtr3Raw = value; syncClassNamePointer(r); return;
      case 64: r.unitSize = value; return;
      case 65: r.sexOrVoice = value; r.spriteType = value; return;
      case 66: r.leadership = value; r.combatBehavior = value; return;
      case 67: r.headerPad = value; r.b67Raw = value; return;
      case 68: r.baseHp = ((value << 8) | ((r.baseHp || 0) & 0x00FF)); r.b68Raw = value; r.powerRating = r.baseHp & 0xFF; return;
      case 69: r.baseHp = (((r.baseHp || 0) & 0xFF00) | value); r.b68Raw = (r.baseHp >> 8) & 0xFF; r.powerRating = value; return;
      case 70: r.hpGrowth = value; r.unitCount = value; return;
      case 71: r.headerTailRaw = value; r.b71Raw = value; return;
    }
  }

  function syncClassNamePointer(r) {
    r.namePtr = ((r.namePtr0Raw << 24) | (r.namePtr1Raw << 16) |
      (r.namePtr2Raw << 8) | r.namePtr3Raw) >>> 0;
    r.ptr = r.namePtr;
  }

  function syncRowAttackState(r) {
    r.rowAttacks = [
      { attackId: r.b43Raw, count: r.frontAtks },
      { attackId: r.b45Raw, count: r.midAtks },
      { attackId: r.b47Raw, count: r.rearAtks }
    ];
    // Deprecated pre-decode shape retained for patch compatibility only.
    r.equipSlots = [
      { slotType: r.b42Raw,    equipGroup: r.b43Raw },
      { slotType: r.frontAtks, equipGroup: r.b45Raw },
      { slotType: r.midAtks,   equipGroup: r.b47Raw }
    ];
  }

  function syncAttacks(r) {
    r.attacks = [r.atkTypeRaw, r.physAtk, r.magAtk, r.physDef, r.magDef, r.flagsRaw];
  }

  function applyNeutralEncounterPatch(rom, patch, warnings) {
    var applied = {
      slices: 0, terrainRates: 0, sliceRates: 0,
      scenarioRates: 0, customSquads: 0
    };
    if (!patch) return applied;
    if (!rom.neutralEncounters) {
      warnings.push('Patch includes neutral encounters, but this ROM parse has no neutral encounter data - skipping.');
      return applied;
    }

    var records = rom.neutralEncounters.records || [];
    var byS0 = {};
    for (var i = 0; i < records.length; i++) byS0[String(records[i].s0)] = records[i];

    var slices = patch.scenario_slices || {};
    for (var s0 in slices) {
      var rec = byS0[String(s0)];
      if (!rec) {
        warnings.push('Patch references neutral encounter slice $s0=' + s0 + ' but no matching record exists - skipping.');
        continue;
      }
      var entry = slices[s0];
      var slots = entry && entry.slots ? entry.slots : {};
      var appliedThisSlice = false;
      for (var si in slots) {
        var slotIdx = parseInt(si, 10);
        if (!isFinite(slotIdx) || slotIdx < 0 || slotIdx >= rec.slots.length) continue;
        var value = slots[si];
        var classA = Array.isArray(value) ? value[0] : value && value.classA;
        var classB = Array.isArray(value) ? value[1] : value && value.classB;
        if (!isPatchByte(classA) || !isPatchByte(classB)) continue;
        rec.slots[slotIdx].classA = classA & 0xFF;
        rec.slots[slotIdx].classB = classB & 0xFF;
        appliedThisSlice = true;
      }
      if (appliedThisSlice) {
        rec.isEmpty = rec.slots.every(function(slot) { return !slot.classA && !slot.classB; });
        applied.slices++;
      }
    }

    var rates = patch.terrain_rates || {};
    var entries = rom.neutralEncounters.terrainRates && rom.neutralEncounters.terrainRates.entries
      ? rom.neutralEncounters.terrainRates.entries
      : [];
    var byTerrainByte = {};
    for (var r = 0; r < entries.length; r++) byTerrainByte[String(entries[r].terrainByte)] = entries[r];
    for (var tb in rates) {
      var terrainByte = parseInt(tb, 10);
      var rateEntry = byTerrainByte[String(terrainByte)];
      if (!rateEntry) {
        warnings.push('Patch references terrain-rate byte ' + tb + ' but no matching row exists - skipping.');
        continue;
      }
      var ratePatch = rates[tb] || {};
      var touched = false;
      if (Object.prototype.hasOwnProperty.call(ratePatch, 'rate')) {
        var rate = Math.round(Number(ratePatch.rate));
        if (isFinite(rate) && rate >= 0 && rate <= 100) {
          rateEntry.rate = rate;
          touched = true;
        }
      }
      if (Object.prototype.hasOwnProperty.call(ratePatch, 'rawLookup')) {
        var rawLookup = ratePatch.rawLookup;
        if (isPatchByte(rawLookup)) {
          rateEntry.rawLookup = rawLookup & 0xFF;
          rateEntry.encounterSlot = rawLookup ? rawLookup - 1 : null;
          rateEntry.terrainName = rateEntry.encounterSlot == null
            ? 'Disabled'
            : (OB64.TERRAIN_NAMES[rateEntry.encounterSlot] || ('Slot ' + rateEntry.encounterSlot));
          rateEntry.enabled = rawLookup !== 0;
          touched = true;
        }
      }
      if (touched) applied.terrainRates++;
    }

    function parseSliceRateBranch(source, label) {
      source = source || {};
      var mode = source.mode || 'inherit';
      if (mode === 'inherit' || mode === 'disabled') {
        return { mode: mode, passCount: null, divisor: null };
      }
      if (mode !== 'override') {
        warnings.push(label + ' has unknown mode "' + mode + '" - using inherit.');
        return { mode: 'inherit', passCount: null, divisor: null };
      }
      var divisor = Number(source.divisor);
      var passCount = Number(source.pass_count != null ? source.pass_count : source.passCount);
      var maxPass = Number.isInteger(divisor) && divisor > 0
        ? Math.min(0x10000, divisor) : 0;
      if (!Number.isInteger(divisor) || divisor <= 0 || divisor > 0xFFFFFFFF ||
          !Number.isInteger(passCount) || passCount < 1 || passCount > maxPass) {
        warnings.push(label + ' has an invalid pass count or divisor - using inherit.');
        return { mode: 'inherit', passCount: null, divisor: null };
      }
      return { mode: 'override', passCount: passCount, divisor: divisor };
    }
    var scenarioRows = rom.neutralEncounters.scenarioRates || [];
    var scenarioByRuntimeKey = {};
    for (var scenarioIndex = 0; scenarioIndex < scenarioRows.length; scenarioIndex++) {
      scenarioByRuntimeKey[String(scenarioRows[scenarioIndex].runtimeKey)] = scenarioRows[scenarioIndex];
    }

    var sliceRates = patch.slice_rates || {};
    for (var rateS0 in sliceRates) {
      warnings.push('Shared neutral-slice rate ' + rateS0 +
        ' is no longer supported and was ignored. Set direct scenario overrides instead.');
    }

    var scenarioRates = patch.scenario_rates || {};
    for (var runtimeKeyText in scenarioRates) {
      var scenarioRow = scenarioByRuntimeKey[String(runtimeKeyText)];
      if (!scenarioRow) {
        warnings.push('Patch references neutral scenario rate key ' + runtimeKeyText +
          ' but no matching runtime scenario exists - skipping.');
        continue;
      }
      var scenarioPatch = scenarioRates[runtimeKeyText] || {};
      if (scenarioPatch.slice != null && Number(scenarioPatch.slice) !== scenarioRow.slice) {
        warnings.push('Neutral scenario key ' + runtimeKeyText + ' maps to slice ' +
          scenarioRow.slice + ', not Project slice ' + scenarioPatch.slice + '. The ROM mapping wins.');
      }
      var directNormal = parseSliceRateBranch(scenarioPatch.normal,
        'Neutral scenario key ' + runtimeKeyText + ' state-bit-set rate');
      var directAlternate = parseSliceRateBranch(scenarioPatch.alternate,
        'Neutral scenario key ' + runtimeKeyText + ' state-bit-clear rate');
      scenarioRow.rateOverride = {
        normal: directNormal,
        alternate: directAlternate
      };
      applied.scenarioRates++;
    }

    rom.neutralEncounters.customSquads = rom.neutralEncounters.customSquads || [];
    var customPatch = patch.custom_squads || {};
    function customKey(profile) {
      return String(profile.runtimeKey) + ':' + String(profile.terrainSlot);
    }
    for (var customKeyText in customPatch) {
      var currentIndex = -1;
      for (var profileIndex = 0;
          profileIndex < rom.neutralEncounters.customSquads.length; profileIndex++) {
        if (customKey(rom.neutralEncounters.customSquads[profileIndex]) === customKeyText) {
          currentIndex = profileIndex;
          break;
        }
      }
      var customEntry = customPatch[customKeyText];
      if (customEntry == null) {
        if (currentIndex >= 0) {
          rom.neutralEncounters.customSquads.splice(currentIndex, 1);
          applied.customSquads++;
        }
        continue;
      }
      var keyParts = String(customKeyText).split(':');
      var customRuntimeKey = Number(customEntry.runtime_key != null
        ? customEntry.runtime_key : keyParts[0]);
      var terrainSlot = Number(customEntry.terrain_slot != null
        ? customEntry.terrain_slot : keyParts[1]);
      var mappedScenario = scenarioByRuntimeKey[String(customRuntimeKey)];
      var profileId = Number(customEntry.profile_id);
      if (!mappedScenario || !Number.isInteger(mappedScenario.slice) ||
          !Number.isInteger(terrainSlot) || terrainSlot < 0 || terrainSlot > 9 ||
          !Array.isArray(customEntry.members) ||
          !Number.isInteger(profileId) || profileId < 1 || profileId > 255) {
        warnings.push('Custom neutral squad ' + customKeyText +
          ' must use the Project v30 typed member schema and has an invalid scenario, ' +
          'terrain, profile ID, or member list - skipping.');
        continue;
      }
      var duplicateId = rom.neutralEncounters.customSquads.some(function(existing, index) {
        return index !== currentIndex && Number(existing.profileId) === profileId;
      });
      if (duplicateId) {
        warnings.push('Custom neutral squad ' + customKeyText + ' reuses profile ID ' +
          profileId + ' - skipping.');
        continue;
      }
      var persuasion = customEntry.persuasion || {};
      var persuasionChance = persuasion.chance == null ? 10 : Number(persuasion.chance);
      var classBonuses = Array.isArray(persuasion.class_bonuses)
        ? persuasion.class_bonuses.map(function(bonus) {
          return {
            classId: Number(bonus && bonus.class_id),
            bonus: Number(bonus && bonus.bonus_percent)
          };
        }) : [];
      var retreat = customEntry.retreat || {};
      var retreatHpThreshold = retreat.hp_threshold == null
        ? 0 : Number(retreat.hp_threshold);
      var memberList = customEntry.members.map(function(member) {
        return {
          classId: Number(member && member.class_id),
          levelOffsetRaw: Number(member && member.level_offset_raw),
          cell: Number(member && member.cell),
          cohort: String(member && member.cohort || '').toUpperCase()
        };
      });
      var equipmentSource = customEntry.equipment || {};
      var equipment = {
        A: Array.isArray(equipmentSource.A) ? equipmentSource.A.slice() : [0, 0],
        B: Array.isArray(equipmentSource.B) ? equipmentSource.B.slice() : [0, 0],
        C: Array.isArray(equipmentSource.C) ? equipmentSource.C.slice() : [0, 0]
      };
      var rewards = customEntry.rewards || {};
      var rewardSlots = Array.isArray(rewards.slots)
        ? rewards.slots.map(function(slot) {
          return { raw: Number(slot && slot.raw), weight: Number(slot && slot.weight) };
        }) : undefined;
      var candidate = {
        profileId: profileId,
        runtimeKey: customRuntimeKey,
        slice: mappedScenario.slice,
        terrainSlot: terrainSlot,
        members: memberList,
        equipment: equipment,
        label: customEntry.label == null ? 'Bandits!' : String(customEntry.label),
        persuasion: {
          mode: 'fixed',
          chance: persuasionChance,
          classBonuses: classBonuses
        },
        retreat: { hpThreshold: retreatHpThreshold },
        rewards: rewardSlots ? { slots: rewardSlots } : undefined
      };
      try {
        if (OB64.runtimeOverrides && OB64.runtimeOverrides.normalizeCustomProfile) {
          var normalized = OB64.runtimeOverrides.normalizeCustomProfile(candidate, profileIndex);
          candidate.members = normalized.members;
          candidate.equipment = normalized.equipment;
          candidate.label = normalized.label;
          candidate.persuasion = normalized.persuasion;
          candidate.retreat = normalized.retreat;
          candidate.rewards = normalized.rewards;
        }
      } catch (customError) {
        warnings.push('Custom neutral squad ' + customKeyText + ' is invalid: ' +
          customError.message + ' - skipping.');
        continue;
      }
      if (currentIndex >= 0) rom.neutralEncounters.customSquads[currentIndex] = candidate;
      else rom.neutralEncounters.customSquads.push(candidate);
      applied.customSquads++;
    }
    rom.neutralEncounters.customSquads.sort(function(a, b) {
      return a.runtimeKey - b.runtimeKey || a.terrainSlot - b.terrainSlot;
    });
    return applied;
  }

  function applyCreatureDropsPatch(rom, patch, warnings) {
    var result = { records: 0, weighted: 0 };
    if (!patch) return result;
    if (!rom.creatureDrops || !rom.creatureDrops.records) {
      warnings.push('Patch includes creature drops, but this ROM parse has no creature drop data - skipping.');
      return result;
    }
    var records = rom.creatureDrops.records;
    for (var rk in patch) {
      var entry = patch[rk];
      if (!entry || typeof entry !== 'object') continue;
      var idx = Number.isInteger(entry.record_index) ? entry.record_index : parseInt(rk, 10);
      if (!isFinite(idx) || idx < 0 || idx >= records.length) {
        warnings.push('Patch references creature-drop record #' + rk + ' but no matching record exists - skipping.');
        continue;
      }
      var rec = records[idx];
      var staticTouched = false;
      if (Object.prototype.hasOwnProperty.call(entry, 'padByte') && isPatchByte(entry.padByte)) {
        var nextPadByte = entry.padByte & 0xFF;
        if (rec.padByte !== nextPadByte) {
          rec.padByte = nextPadByte;
          staticTouched = true;
        }
      }
      if (Object.prototype.hasOwnProperty.call(entry, 'classId') && isPatchByte(entry.classId)) {
        var nextClassId = entry.classId & 0xFF;
        if (rec.classId !== nextClassId) {
          rec.classId = nextClassId;
          staticTouched = true;
        }
      }
      if (Array.isArray(entry.slots)) {
        for (var s = 0; s < 3 && s < entry.slots.length; s++) {
          var raw = entry.slots[s];
          if (!isPatchU16(raw)) continue;
          if (!rec.slots[s]) rec.slots[s] = {};
          var nextRaw = raw & 0xFFFF;
          var currentRaw = dropSlotRaw(rec.slots[s]);
          rec.slots[s].raw = nextRaw;
          rec.slots[s].itemId = nextRaw & 0x7FFF;
          rec.slots[s].isEquipment = (nextRaw & 0x8000) !== 0;
          if (currentRaw !== nextRaw) staticTouched = true;
        }
      }
      if (Array.isArray(entry.weights)) {
        var weightsValid = entry.weights.length === 3;
        var totalWeight = 0;
        for (var wi = 0; wi < 3 && weightsValid; wi++) {
          var weight = Number(entry.weights[wi]);
          if (!Number.isInteger(weight) || weight < 0 || weight > 0xFFFF) {
            weightsValid = false;
          } else {
            totalWeight += weight;
          }
        }
        if (!weightsValid || !totalWeight) {
          warnings.push('Creature-drop record #' + idx +
            ' has invalid or zero-total weights - retaining its current weights.');
        } else {
          for (wi = 0; wi < 3; wi++) {
            if (!rec.slots[wi]) rec.slots[wi] = {};
            rec.slots[wi].weight = Number(entry.weights[wi]);
          }
          rec.weightedRuntimeOverride = entry.weights.some(function(value) { return Number(value) !== 1; });
          if (entry.weights_modified === true || rec.weightedRuntimeOverride) result.weighted++;
        }
      }
      if (staticTouched) {
        rec.isSentinel = (rec.padByte === 0 && rec.classId === 0 &&
          rec.slots.every(function(slot) { return !dropSlotRaw(slot); }));
        result.records++;
      }
    }
    rebuildCreatureDropByClass(rom.creatureDrops);
    return result;
  }

  function rebuildCreatureDropByClass(drops) {
    OB64.rebuildCreatureDropIndex(drops);
  }

  function applyConsumablesPatch(rom, patch, warnings) {
    if (!patch) return 0;
    if (!rom.consumables) {
      warnings.push('Patch includes consumables, but this ROM parse has no consumable table - skipping.');
      return 0;
    }
    var applied = 0;
    for (var ck in patch) {
      var idx = parseInt(ck, 10);
      if (!isFinite(idx) || idx < 0 || idx >= rom.consumables.length) {
        warnings.push('Patch references consumable #' + ck + ' but no matching record exists - skipping.');
        continue;
      }
      var rec = rom.consumables[idx];
      var entry = patch[ck] || {};
      var touched = false;
      if (Object.prototype.hasOwnProperty.call(entry, 'flagHi') && isPatchU16(entry.flagHi)) {
        rec.flagHi = entry.flagHi & 0xFFFF;
        touched = true;
      }
      if (Object.prototype.hasOwnProperty.call(entry, 'price') && isPatchU16(entry.price)) {
        rec.price = entry.price & 0xFFFF;
        touched = true;
      }
      if (Array.isArray(entry.flagLo)) {
        if (!rec.flagLo) rec.flagLo = [0, 0, 0, 0];
        for (var i = 0; i < 4 && i < entry.flagLo.length; i++) {
          if (isPatchByte(entry.flagLo[i])) {
            rec.flagLo[i] = entry.flagLo[i] & 0xFF;
            touched = true;
          }
        }
      }
      if (touched) applied++;
    }
    return applied;
  }

  function applyStatGatesPatch(rom, patch, warnings) {
    if (!patch) return 0;
    if (!rom.statGates || !rom.statGates.byClass) {
      warnings.push('Patch includes stat gates, but this ROM parse has no stat-gate table - skipping.');
      return 0;
    }
    var applied = 0;
    for (var cid in patch) {
      var gate = rom.statGates.byClass[cid];
      if (!gate) {
        warnings.push('Patch references stat-gate class #' + cid + ' but no matching record exists - skipping.');
        continue;
      }
      var entry = patch[cid] || {};
      var touched = false;
      for (var i = 0; i < STAT_GATE_FIELDS.length; i++) {
        var field = STAT_GATE_FIELDS[i];
        if (!Object.prototype.hasOwnProperty.call(entry, field)) continue;
        if (!isPatchByte(entry[field])) continue;
        gate[field] = entry[field] & 0xFF;
        touched = true;
      }
      if (touched) applied++;
    }
    return applied;
  }

  function isPatchByte(value) {
    return Number.isInteger(value) && value >= 0 && value <= 255;
  }

  function isPatchU16(value) {
    return Number.isInteger(value) && value >= 0 && value <= 65535;
  }

  function clampByte(value) {
    var n = Math.round(Number(value));
    if (!isFinite(n)) n = 0;
    if (n < 0) n = 0;
    if (n > 255) n = 255;
    return n;
  }

  function clampU16(value) {
    var n = Math.round(Number(value));
    if (!isFinite(n)) n = 0;
    if (n < 0) n = 0;
    if (n > 65535) n = 65535;
    return n;
  }

  function clampBasisPoints(value) {
    var n = Math.round(Number(value));
    if (!isFinite(n)) n = 0;
    if (n < 0) n = 0;
    if (n > 10000) n = 10000;
    return n;
  }

  function clampMicroBasisPoints(value) {
    var n = Math.round(Number(value));
    if (!isFinite(n)) n = 0;
    if (n < 0) n = 0;
    if (n > 1000000) n = 1000000;
    return n;
  }

  function vanillaGlobalBasisPoints() {
    return OB64.NEUTRAL_GLOBAL_VANILLA_BASIS_POINTS || 7;
  }

  function vanillaGlobalMicroBasisPoints() {
    return OB64.NEUTRAL_GLOBAL_VANILLA_MICRO_BASIS_POINTS || (vanillaGlobalBasisPoints() * 100);
  }

  function clampGlobalMultiplier(value) {
    var n = Math.round(Number(value) * 100) / 100;
    if (!isFinite(n)) n = 1;
    if (n < 1) n = 1;
    var max = OB64.NEUTRAL_GLOBAL_HARD_MAX_MULTIPLIER || 100;
    if (n > max) n = max;
    return n;
  }

  // --------------------------------------------------------------
  // Exports
  // --------------------------------------------------------------
  OB64.patch = {
    FORMAT:         PATCH_FORMAT,
    VERSION:        PATCH_VERSION,
    snapshotOriginal: snapshotOriginal,
    collectPatch:     collectPatch,
    applyPatch:       applyPatch,
    downloadPatch:    downloadPatch,
    parsePatchFile:   parsePatchFile,
    PatchFormatError: PatchFormatError,
  };
})();
