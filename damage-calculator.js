// Ogre Battle 64 damage calculator.
//
// This module is deliberately read-only: it consumes parsed ROM records and
// keeps calculator inputs/overrides in panel-local UI state. It never mutates
// the ROM model and never calls the editor's change tracker.
(function(root, factory) {
  var api = factory(root && root.OB64 ? root.OB64 : {});
  if (typeof module === 'object' && module.exports) {
    module.exports = api;
    return;
  }
  if (root) {
    root.OB64 = root.OB64 || {};
    root.OB64.damageCalculator = Object.freeze({
      render: api.render
    });
  }
})(typeof window !== 'undefined' ? window : globalThis, function(OB64) {
  'use strict';

  var ROW_NAMES = ['Front', 'Middle', 'Rear'];
  var SLOT_NAMES = ['Weapon', 'Body', 'Off-hand', 'Head / Accessory'];
  var ELEMENT_NAMES = ['Physical', 'Wind', 'Flame', 'Earth', 'Water', 'Virtue', 'Bane'];
  var BOOK_NAMES = ['None / generic', 'Wind', 'Flame', 'Earth', 'Water'];
  var MOVEMENT_NAMES = ['Unknown', 'Sky', 'Plain', 'Forest', 'Mountain', 'Snow', 'Marsh', 'Immobile'];
  var TERRAIN_NAMES = [
    'Raw terrain 0', 'Highway', 'Bridge', 'Plain', 'Barrens', 'Forest', 'Marsh',
    'Highlands', 'River', 'Snowy Highway', 'Snowy Bridge', 'Snowy Plain',
    'Snowy Barrens', 'Snowy Forest', 'Snowy Highlands', 'Stronghold',
    'Castle Wall', 'Castle Gate', 'Castle Gate (alternate)', 'Castle',
    'Within Castle Wall 1', 'Within Castle Wall 2', 'Within Castle Wall 3',
    'Bridge (alternate)', 'Malefic Woods', 'Generator', 'Building'
  ];

  // Rows are movement types Sky..Immobile; columns are terrain IDs 0..26.
  var TERRAIN_MOVEMENT = [
    [0,1,1,0,0,-1,0,0,0,1,1,0,0,-1,0,0,0,0,-2,-5,0,0,0,1,-1,-1,0],
    [0,1,1,0,-3,0,-4,-4,-2,1,1,-2,-3,0,-4,0,0,0,0,0,0,0,0,1,0,0,0],
    [0,1,1,0,-2,2,-3,-4,-2,1,1,1,-1,2,-1,0,0,0,0,0,0,0,0,1,2,2,0],
    [0,1,1,0,0,1,-1,2,-2,1,1,-1,1,0,1,0,0,0,0,0,0,0,0,1,1,1,0],
    [0,1,1,0,-2,2,-3,-3,-1,1,1,2,1,3,0,0,0,0,0,0,0,0,0,1,2,2,0],
    [0,1,1,0,-1,1,1,-2,0,1,1,-1,-1,0,-1,0,0,0,0,0,0,0,0,1,1,1,0],
    [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
  ];

  // D0 action-family rows x front/middle/rear formation rank.
  var FORMATION_ADJUSTMENT = [
    [5, 0, -5],
    [-5, 0, 5],
    [5, 0, -5],
    [-5, 0, 5],
    [-5, 0, 5],
    [5, 0, -5]
  ];

  var DYNAMIC_ELEMENTAL_ACTIONS = {
    45: true, 46: true, 47: true, 48: true,
    51: true,
    145: true
  };

  var VARIABLE_ELEMENT_ACTIONS = {
    45: true, 46: true, 47: true, 48: true, 145: true
  };

  var ACTION55_PRODUCT_TEMPLATES = { 45: true, 51: true };
  var TIER1_RESOLVED_ACTIONS = { 1: 55, 2: 63, 3: 71, 4: 79, 5: 87, 6: 91 };
  var RESOLVED_SPELL_NAMES = {
    55: 'Lightning',
    63: 'Fireball',
    71: 'Acid Vapor',
    79: 'Ice Blast',
    87: 'Healing',
    91: 'Word of Pain'
  };

  var CONSTANT_DEFS = [
    { key: 'magicCoefficientDivisor', label: 'Magic Attack Coefficient division value', value: 6, modes: ['magic'] },
    { key: 'magicIntOffset', label: 'Magic Intelligence added amount', value: 50, modes: ['magic'] },
    { key: 'magicIntDivisor', label: 'Magic Intelligence division value', value: 50, modes: ['magic'] },
    { key: 'magicMenOffset', label: 'Magic Mentality added amount', value: 100, modes: ['magic'] },
    { key: 'magicMenDivisor', label: 'Magic Mentality division value', value: 100, modes: ['magic'] },
    { key: 'magicDefenseCoefficientDivisor', label: 'Magic Defense Coefficient division value', value: 20, modes: ['magic'] },
    { key: 'magicDefenseMenOffset', label: 'Magic Defense Mentality added amount', value: 25, modes: ['magic'] },
    { key: 'magicDefenseMenDivisor', label: 'Magic Defense Mentality division value', value: 25, modes: ['magic'] },
    { key: 'magicDefenseIntOffset', label: 'Magic Defense Intelligence added amount', value: 100, modes: ['magic'] },
    { key: 'magicDefenseIntDivisor', label: 'Magic Defense Intelligence division value', value: 100, modes: ['magic'] },
    { key: 'physicalCoefficientDivisor', label: 'Physical Attack Coefficient division value', value: 6, modes: ['physical'] },
    { key: 'physicalStrOffset', label: 'Physical Strength added amount', value: 50, modes: ['physical'] },
    { key: 'physicalStrDivisor', label: 'Physical Strength division value', value: 50, modes: ['physical'] },
    { key: 'physicalDexOffset', label: 'Physical Dexterity added amount', value: 100, modes: ['physical'] },
    { key: 'physicalDexDivisor', label: 'Physical Dexterity division value', value: 100, modes: ['physical'] },
    { key: 'defenseCoefficientDivisor', label: 'Physical Defense Coefficient division value', value: 20, modes: ['physical'] },
    { key: 'defenseVitOffset', label: 'Defense Vitality added amount', value: 25, modes: ['physical'] },
    { key: 'defenseVitDivisor', label: 'Defense Vitality division value', value: 25, modes: ['physical'] },
    { key: 'defenseStrOffset', label: 'Defense Strength added amount', value: 100, modes: ['physical'] },
    { key: 'defenseStrDivisor', label: 'Defense Strength division value', value: 100, modes: ['physical'] },
    { key: 'modifierBase', label: 'Base value before battle adjustments', value: 100, modes: ['magic', 'physical'] },
    { key: 'percentDivisor', label: 'Percent conversion value', value: 100, modes: ['magic', 'physical'] },
    { key: 'damageMinimum', label: 'Damage minimum', value: 1, modes: ['magic', 'physical'] },
    { key: 'damageMaximum', label: 'Damage maximum', value: 9999, modes: ['magic', 'physical'] },
    { key: 'varianceMinimum', label: 'Minimum random damage adjustment (%)', value: -10, modes: ['magic', 'physical'] },
    { key: 'varianceMaximum', label: 'Maximum random damage adjustment (%)', value: 10, modes: ['magic', 'physical'] },
    { key: 'magicResistanceMinimum', label: 'Magic resistance minimum (%)', value: 0, modes: ['magic'] },
    { key: 'magicResistanceMaximum', label: 'Magic resistance maximum (%)', value: 100, modes: ['magic'] },
    { key: 'accuracyDexOffset', label: 'Attacker Dexterity added amount', value: 50, modes: ['physical'] },
    { key: 'accuracyAgiOffset', label: 'Attacker Agility added amount', value: 100, modes: ['physical'] },
    { key: 'accuracyAgiDivisor', label: 'Attacker Agility division value', value: 55, modes: ['physical'] },
    { key: 'evasionAgiOffset', label: 'Defender Agility added amount', value: 50, modes: ['physical'] },
    { key: 'evasionDexOffset', label: 'Defender Dexterity added amount', value: 100, modes: ['physical'] },
    { key: 'evasionDexDivisor', label: 'Defender Dexterity division value', value: 50, modes: ['physical'] },
    { key: 'hitLuckBase', label: 'Base value for the hit-chance Luck comparison', value: 100, modes: ['physical'] },
    { key: 'chanceMinimum', label: 'Minimum displayed chance (%)', value: 0, modes: ['physical'] },
    { key: 'chanceMaximum', label: 'Maximum displayed chance (%)', value: 100, modes: ['physical'] },
    { key: 'criticalLuckDivisor', label: 'Critical-hit Luck division value', value: 2, modes: ['physical'] },
    { key: 'criticalBase', label: 'Base critical-hit chance', value: 5, modes: ['physical'] },
    { key: 'criticalMultiplier', label: 'Critical damage multiplier', value: 2, modes: ['physical'] }
  ];

  var CONSTANT_DEFAULTS = {};
  CONSTANT_DEFS.forEach(function(def) { CONSTANT_DEFAULTS[def.key] = def.value; });

  var STAT_FIELD_USES = {
    hp: {
      attacker: 'Not consumed by either damage formula; shown so the projected attacker is complete.',
      defender: 'Supplies the default Defender current Hit Points used by the Normal and Critical resulting-Hit-Points outputs; it does not change Physical damage.'
    },
    str: {
      attacker: 'Used by the Physical Attack Score. It is not consumed by the bounded action-55 Magic calculation.',
      defender: 'Used with equipment Strength in the Physical Defense Score.'
    },
    vit: {
      attacker: 'Not consumed while this character is the attacker in either retained formula.',
      defender: 'Used with equipment Vitality in the Physical Defense Score.'
    },
    int: {
      attacker: 'Used in the Magic Attack Score. It is not used by the documented standard physical damage calculation.',
      defender: 'Used with equipment Intelligence in the Magic Defense Score. It is not used by the standard Physical Defense Score.'
    },
    men: {
      attacker: 'Used with equipment Mentality in the Magic Attack Score.',
      defender: 'Used with equipment Mentality in the Magic Defense Score. It is not used by the standard Physical Defense Score.'
    },
    agi: {
      attacker: 'Used with equipment Agility in the Attacker accuracy score.',
      defender: 'Used with equipment Agility in the Defender evasion score.'
    },
    dex: {
      attacker: 'Used in the Physical Attack Score and Attacker accuracy score.',
      defender: 'Used with equipment Dexterity in the Defender evasion score.'
    },
    lck: {
      attacker: 'Used by the Physical damage effective-Luck difference and hit-chance Luck comparison. Character Luck without equipment B12 also feeds the critical-hit chance calculation.',
      defender: 'Subtracted in the Physical damage effective-Luck difference and hit-chance Luck comparison. Character Luck without equipment B12 also feeds the critical-hit chance calculation.'
    },
    alignment: {
      attacker: 'Used by the Snow/Marsh attack adjustment; it does not feed Attacker accuracy.',
      defender: 'Used only by the supported Snow/Marsh defender adjustment.'
    }
  };

  var EQUIPMENT_FIELD_USES = {
    str: {
      attacker: 'Shown as the total positive or negative Strength from Current Gear. Physical Attack uses its weapon and non-weapon portions separately.',
      defender: 'Added to defender Strength inside the Physical Defense Score.'
    },
    int: {
      attacker: 'Added to the class Magic Attack Coefficient in the first part of the Magic Attack Score.',
      defender: 'Added to defender Intelligence in the Magic Defense Score.'
    },
    agi: {
      attacker: 'Added to attacker Agility inside the Attacker accuracy score.',
      defender: 'Added to defender Agility inside the Defender evasion score.'
    },
    dex: {
      attacker: 'Added to attacker Dexterity inside the Physical Attack Score and Attacker accuracy score.',
      defender: 'Added to defender Dexterity inside the Defender evasion score.'
    },
    vit: {
      attacker: 'Not consumed while this character is the attacker.',
      defender: 'Added to defender Vitality inside the Physical Defense Score.'
    },
    men: {
      attacker: 'Added to attacker Mentality inside the Magic Attack Score.',
      defender: 'Added to defender Mentality inside the Magic Defense Score.'
    },
    b12: {
      attacker: 'Added to attacker Luck for Physical damage and hit chance; never used by the critical-hit check.',
      defender: 'Added to defender Luck before subtraction in Physical damage and hit chance; never used by the critical-hit check.'
    }
  };

  var STAT_DISPLAY_NAMES = {
    hp: 'Hit Points',
    str: 'Strength',
    vit: 'Vitality',
    int: 'Intelligence',
    men: 'Mentality',
    agi: 'Agility',
    dex: 'Dexterity',
    lck: 'Luck',
    alignment: 'Alignment',
    b12: 'Luck adjustment (item record field B12)'
  };

  var DERIVED_FIELD_INFO = {
    'defender.currentHp': {
      source: 'Defaults to projected Defender maximum Hit Points and can be replaced with the defender\'s actual current Hit Points.',
      use: 'Normal and critical damage are subtracted from it for the two resulting-Hit-Points outputs.'
    },
    'attacker.equip.weaponStr': {
      source: 'Strength from the first equipped item the game recognizes as a weapon.',
      use: 'Added to the class Physical Attack Coefficient before division by 6 in the Physical Attack Score.'
    },
    'attacker.equip.nonweaponStr': {
      source: 'Total positive or negative Strength from Current Gear other than the recognized weapon.',
      use: 'Added to attacker Strength in the second part of the Physical Attack Score.'
    },
    'attacker.equip.weaponElement': {
      source: 'Element of the first recognized weapon in Current Gear.',
      use: 'Compared with the action element for the -2, 0, or +2 magic adjustment and used when a variable spell template has no Spellbook.'
    },
    'attacker.equip.bookVariant': {
      source: 'The character\'s saved Generic Spellbook selector at character field +0x1A; it is not inferred from the equipped Spellbook and therefore defaults to zero.',
      use: 'Selects the Spellbook-related battle adjustment and resolves a variable spell used with a Generic Spellbook.'
    },
    'defender.equip.bookVariant': {
      source: 'The defender\'s saved Generic Spellbook selector at character field +0x1A; it is not inferred from equipment and defaults to zero.',
      use: 'Selects the defender\'s Spellbook-related adjustment used by Magic Defense, Physical Defense, and evasion.'
    },
    'action.family': {
      source: 'The selected action\'s family byte, documented as action-record field D0.',
      use: 'Combines with Front, Middle, or Rear formation to choose the attacker\'s formation adjustment.'
    },
    'action.element': {
      source: 'The selected action\'s element byte, documented as D1. Combined elements are reduced to a primary element; variable spells check Spellbook, weapon, then the class default-element field B58.',
      use: 'Selects the weapon-versus-spell adjustment and the defender resistance multiplier used by both damage formulas.'
    },
    'attacker.moveType': {
      source: 'The selected attacker class\'s movement type, stored in class field B32.',
      use: 'Selects terrain/movement and movement/book-Alignment adjustments.'
    },
    'defender.moveType': {
      source: 'The selected defender class\'s movement type, stored in class field B32.',
      use: 'Selects defender terrain/movement and movement/book-Alignment adjustments.'
    },
    'attacker.context.terrainMovement': {
      source: 'Positive or negative value from the selected Attacker terrain and movement-type table entry.',
      use: 'Added to the attacker\'s total magic or physical battle adjustment. Terrain is not part of Attacker accuracy in the documented calculation.'
    },
    'defender.context.terrainMovement': {
      source: 'Positive or negative value from the selected Defender terrain and movement-type table entry.',
      use: 'Added to the defender\'s Physical Defense adjustment, Magic Defense adjustment, and evasion adjustment.'
    },
    'attacker.context.bookOrAlignment': {
      source: 'For Sky through Mountain movement, the game uses the character\'s Generic Spellbook selector; for Snow or Marsh, it uses Alignment.',
      use: 'Added to the attacker\'s total magic or physical battle adjustment.'
    },
    'defender.context.bookOrAlignment': {
      source: 'For Sky through Mountain movement, the game uses the defender\'s Generic Spellbook selector; for Snow or Marsh, it uses Alignment.',
      use: 'Added to the defender\'s total Physical Defense and Magic Defense adjustments.'
    },
    'attacker.context.weaponSpellElement': {
      source: 'Comparison of the first recognized weapon\'s element with the resolved action element.',
      use: 'Adds -2, 0, or +2 only to the attacker\'s total magic battle adjustment.'
    },
    'attacker.context.formation': {
      source: 'Table lookup using the action family byte (D0) and selected Front, Middle, or Rear formation.',
      use: 'Added to the attacker\'s total magic and physical battle adjustments; it is not used in either defender Defense Score.'
    },
    'attacker.context.antiDragon': {
      source: 'Adds 5 when the target is dragon-class and attacker class/item conditions are met.',
      use: 'Added to the attacker\'s total magic or physical battle adjustment.'
    },
    'defender.context.antiDragon': {
      source: 'Adds 5 when the attacker is dragon-class and defender class/item conditions are met.',
      use: 'Added to the defender\'s total Physical Defense and Magic Defense adjustments.'
    },
    'attacker.context.leaderAffinity': {
      source: 'The game\'s built-in table comparing the selected squad leader class with the attacker\'s monster group.',
      use: 'Added to the attacker\'s magic/physical battle adjustment and accuracy adjustment.'
    },
    'defender.context.leaderAffinity': {
      source: 'The game\'s built-in table comparing the selected squad leader class with the defender\'s monster group.',
      use: 'Added to the defender\'s Physical Defense adjustment, Magic Defense adjustment, and evasion adjustment.'
    },
    'attacker.context.fatiguePenalty': {
      source: 'The attacker\'s Fatigue value selects a penalty of 0, 5, 18, or 40.',
      use: 'Subtracted from the attacker\'s magic/physical battle adjustment and accuracy adjustment.'
    },
    'defender.context.fatiguePenalty': {
      source: 'The defender\'s Fatigue value selects a penalty of 0, 5, 18, or 40.',
      use: 'Subtracted from the defender\'s Physical Defense adjustment, Magic Defense adjustment, and evasion adjustment.'
    },
    'defender.context.specialTerrainState': {
      source: 'Defaults to zero because the exact retail condition that adds 3 remains unresolved.',
      use: 'Added to the shared defender adjustment used by the Physical Defense and Magic Defense Scores.'
    },
    'attacker.context.magicModifier': {
      source: 'Attacker terrain plus Spellbook/Alignment, weapon-versus-spell element, formation, anti-dragon, and leader compatibility adjustments, minus Fatigue.',
      use: 'Becomes (100 + Magic attack adjustment) / 100 in the Magic Attack Score. For example, +10 becomes a 1.10 multiplier.'
    },
    'attacker.context.physicalModifier': {
      source: 'Attacker terrain plus Spellbook/Alignment, formation, anti-dragon, and leader compatibility adjustments, minus Fatigue.',
      use: 'Becomes (100 + Physical attack adjustment) / 100 in the Physical Attack Score. For example, -5 becomes a 0.95 multiplier.'
    },
    'defender.context.physicalDefenseModifier': {
      source: 'Defender terrain plus Spellbook/Alignment, anti-dragon, leader compatibility, and the unresolved special-state adjustment, minus Fatigue.',
      use: 'Becomes (100 + Physical defense adjustment) / 100 in the Physical Defense Score.'
    },
    'defender.context.magicDefenseModifier': {
      source: 'Defender terrain plus Spellbook/Alignment, anti-dragon, leader compatibility, and the unresolved special-state adjustment, minus Fatigue.',
      use: 'Becomes (100 + Magic Defense adjustment) / 100 in the Magic Defense Score.'
    },
    'attacker.context.accuracyModifier': {
      source: 'Attacker leader compatibility bonus minus the Fatigue penalty.',
      use: 'Becomes the final percentage adjustment in the Attacker accuracy score.'
    },
    'defender.context.evasionModifier': {
      source: 'Defender terrain/movement plus leader compatibility bonus minus the Fatigue penalty.',
      use: 'Becomes the final percentage adjustment in the Defender evasion score.'
    },
    'attacker.coefficient.physicalAttack': {
      source: 'The selected attacker class\'s Physical Attack Coefficient, stored in class field B49.',
      use: 'Combined with weapon Strength and divided by 6 in the Physical Attack Score.'
    },
    'attacker.coefficient.magicAttack': {
      source: 'The selected attacker class\'s Magic Attack Coefficient, stored in class field B50.',
      use: 'Combined with equipment Intelligence and divided by 6 in the Magic Attack Score.'
    },
    'defender.coefficient.physicalDefense': {
      source: 'The selected defender class\'s Physical Defense Coefficient, stored in class field B51.',
      use: 'Divided by 20 in the Physical Defense Score.'
    },
    'defender.coefficient.magicDefense': {
      source: 'The selected defender class\'s Magic Defense Coefficient, stored in class field B52.',
      use: 'Divided by 20 in the Magic Defense Score, which the mode-3 damage kernel subtracts from incoming magic power.'
    },
    'score.magicAttack': {
      source: 'The decimal portion is discarded after multiplying the Magic Attack Coefficient/equipment Intelligence, attacker Intelligence, attacker Mentality/equipment Mentality, and total magic battle-adjustment parts.',
      use: 'Supplies the primary target\'s incoming magic power before Magic Defense, effective Luck, elemental resistance, and random variation.'
    },
    'score.magicDefense': {
      source: 'The decimal portion is discarded after multiplying the Magic Defense Coefficient, defender Mentality/equipment Mentality, defender Intelligence/equipment Intelligence, and total Magic Defense adjustment parts.',
      use: 'Subtracted from the primary target\'s incoming Magic Attack Score before effective Luck, elemental resistance, and random variation.'
    },
    'score.physicalAttack': {
      source: 'The decimal portion is discarded after multiplying the Physical Attack Coefficient/weapon Strength, attacker/non-weapon Strength, Dexterity, and total physical attack-adjustment parts.',
      use: 'This is the starting attack amount before subtracting Physical Defense, adding the Luck difference, and applying resistance and random variation.'
    },
    'score.physicalDefense': {
      source: 'The decimal portion is discarded after multiplying the Physical Defense Coefficient, defender Vitality, defender Strength, and total Physical Defense-adjustment parts.',
      use: 'Subtracted from the Physical Attack Score before resistance and random variation.'
    },
    'score.sourceEffectiveLuck': {
      source: 'Attacker Luck plus the total positive or negative equipment Luck adjustment stored in item field B12.',
      use: 'Feeds the Physical damage effective-Luck difference and hit chance; critical hits use character Luck without equipment B12.'
    },
    'score.targetEffectiveLuck': {
      source: 'Defender Luck plus the total positive or negative equipment Luck adjustment stored in item field B12.',
      use: 'Subtracted in the Physical damage effective-Luck difference and hit chance; critical hits use character Luck without equipment B12.'
    },
    'score.damageLuckDifference': {
      source: 'Attacker effective Luck minus Defender effective Luck.',
      use: 'Added after the Physical Defense Score is subtracted from the Physical Attack Score.'
    },
    'score.targetResistance': {
      source: 'Defender class resistance plus Current Gear resistance for the resolved action element, limited to 0 through 100.',
      use: 'Magic and physical damage multiply by (100 - resistance) / 100.'
    },
    'score.sourceAccuracy': {
      source: 'The decimal portion is discarded after multiplying attacker Dexterity/equipment Dexterity, Agility/equipment Agility, and the Attacker accuracy adjustment.',
      use: 'Divided by Defender evasion inside the base hit-chance calculation.'
    },
    'score.targetEvasion': {
      source: 'The decimal portion is discarded after multiplying defender Agility/equipment Agility, Dexterity/equipment Dexterity, and the Defender evasion adjustment.',
      use: 'Divides Attacker accuracy inside the base hit-chance calculation.'
    },
    'score.specialHitBonus': {
      source: 'Defaults to zero; one helper can add 10 but its player-facing condition is unresolved.',
      use: 'Added after comparing accuracy, evasion, and effective Luck, before limiting the result to 0 through 100.'
    },
    'score.baseHitThreshold': {
      source: 'Attacker accuracy divided by Defender evasion, multiplied by 100 plus Attacker effective Luck minus Defender effective Luck; the decimal portion is discarded, the special bonus is added, and the result is limited to 0 through 100.',
      use: 'Displayed as Hit chance unless the Hit/miss override forces a hit or miss.'
    },
    'score.criticalThreshold': {
      source: 'Attacker Luck minus Defender Luck, divided by 2 with the decimal portion discarded, then plus 5. Equipment Luck adjustments are not used.',
      use: 'Compared with the game\'s random number to determine the exact critical-hit chance.'
    }
  };

  var DERIVED_FIELD_RULES = {
    'defender.currentHp': 'Physical remaining Hit Points = maximum(current Hit Points - applied Normal or Critical damage, 0).',
    'attacker.equip.weaponStr': 'Recognized weapon Strength = Strength on the first Current Gear item whose equipment type is a supported weapon type.',
    'attacker.equip.nonweaponStr': 'Non-weapon equipment Strength = sum of Strength on the remaining Current Gear items.',
    'attacker.equip.weaponElement': 'Recognized weapon element = element on the first recognized Current Gear weapon; no recognized weapon gives Physical/None.',
    'attacker.equip.bookVariant': 'This is the saved character selector 0=None/generic, 1=Wind, 2=Flame, 3=Earth, 4=Water; equipment does not infer it.',
    'defender.equip.bookVariant': 'This is the saved character selector 0=None/generic, 1=Wind, 2=Flame, 3=Earth, 4=Water; equipment does not infer it.',
    'action.family': 'Formation adjustment = action-family table[D0][Front, Middle, or Rear].',
    'action.element': 'Ordinary elements use D1 directly; combined elements reduce to their primary element, and variable templates resolve Spellbook, then weapon, then class B58.',
    'attacker.moveType': 'Movement type selects one row of the terrain table and the Spellbook/Alignment rule.',
    'defender.moveType': 'Movement type selects one row of the terrain table and the Spellbook/Alignment rule.',
    'attacker.context.terrainMovement': 'Terrain adjustment = terrain table[movement type][selected terrain]. Matching terrain selections can still produce different values because each side uses its own movement type; equal values scale separate scores rather than cancelling.',
    'defender.context.terrainMovement': 'Terrain adjustment = terrain table[movement type][selected terrain]. Matching terrain selections can still produce different values because each side uses its own movement type; defender terrain also feeds evasion.',
    'attacker.context.bookOrAlignment': 'Sky through Mountain use the Generic Spellbook selector table; Snow = (Alignment - 50) / 10; Marsh = (50 - Alignment) / 10; Immobile = 0.',
    'defender.context.bookOrAlignment': 'Sky through Mountain use the Generic Spellbook selector table; Snow = (Alignment - 50) / 10; Marsh = (50 - Alignment) / 10; Immobile = 0.',
    'attacker.context.weaponSpellElement': 'Matching ordinary nonphysical weapon/action elements = +2; Wind/Earth, Flame/Water, and Virtue/Bane opposition = -2; every other pairing = 0.',
    'attacker.context.formation': 'D0 families 0, 2, and 5 use Front/Middle/Rear +5/0/-5; families 1, 3, and 4 use -5/0/+5.',
    'attacker.context.antiDragon': 'Attack +5 requires dragon target AND (Dragoon or B23-bit-0 gear). Dragon classes are IDs 0x38–0x44 or 0xA4. Attack gear: Fafnir, Sword of Firedrake, Sword of Tiamat, Balmung, Gram, Axe of Wyrm, or Cyanic Claw.',
    'defender.context.antiDragon': 'Defense +5 requires dragon attacker AND (Dragoon or B24-bit-0 gear). Dragon classes are IDs 0x38–0x44 or 0xA4. Defense gear: Dragon Shield, Dragon Armor, Dragon Helm, or Fang of Firedrake.',
    'attacker.context.leaderAffinity': 'Leader compatibility = built-in table[selected leader class][member monster group], producing 0, +10, or +15.',
    'defender.context.leaderAffinity': 'Leader compatibility = built-in table[selected leader class][member monster group], producing 0, +10, or +15.',
    'attacker.context.fatiguePenalty': 'Fatigue below 70 = 0; 70–79 = 5; 80–89 = 18; 90 or above = 40.',
    'defender.context.fatiguePenalty': 'Fatigue below 70 = 0; 70–79 = 5; 80–89 = 18; 90 or above = 40.',
    'defender.context.specialTerrainState': 'Default = 0. Supply +3 only through the explicit override when the documented but player-facing-unresolved condition is independently known.',
    'attacker.context.magicModifier': 'Total Magic Attack adjustment = terrain + Spellbook/Alignment + weapon/action element + formation + anti-dragon + leader - Fatigue.',
    'attacker.context.physicalModifier': 'Total Physical Attack adjustment = terrain + Spellbook/Alignment + formation + anti-dragon + leader - Fatigue.',
    'defender.context.physicalDefenseModifier': 'Total Physical Defense adjustment = terrain + Spellbook/Alignment + anti-dragon + leader + unresolved special Defense - Fatigue.',
    'defender.context.magicDefenseModifier': 'Total Magic Defense adjustment = terrain + Spellbook/Alignment + anti-dragon + leader + unresolved special Defense - Fatigue.',
    'attacker.context.accuracyModifier': 'Attacker accuracy adjustment = leader compatibility - Fatigue.',
    'defender.context.evasionModifier': 'Defender evasion adjustment = terrain/movement + leader compatibility - Fatigue.',
    'attacker.coefficient.magicAttack': 'The selected class record supplies B50; the first Magic Attack factor is (B50 + Current Gear equipment INT) / 6 with retail defaults.',
    'defender.coefficient.magicDefense': 'The selected class record supplies B52; the first Magic Defense factor is B52 / 20 with retail defaults.',
    'attacker.coefficient.physicalAttack': 'The selected class record supplies B49; the first Physical Attack factor is (B49 + recognized Current Gear weapon STR) / 6 with retail defaults.',
    'defender.coefficient.physicalDefense': 'The selected class record supplies B51; the first Physical Defense factor is B51 / 20 with retail defaults.',
    'score.magicAttack': 'With retail defaults: whole-number part of ((B50 + equipment INT) / 6) × ((attacker INT + 50) / 50) × ((attacker MEN + equipment MEN + 100) / 100) × ((100 + Magic Attack adjustment) / 100).',
    'score.magicDefense': 'With retail defaults: whole-number part of (B52 / 20) × ((defender MEN + equipment MEN + 25) / 25) × ((defender INT + equipment INT + 100) / 100) × ((100 + Magic Defense adjustment) / 100).',
    'score.physicalAttack': 'With retail defaults: whole-number part of ((B49 + weapon STR) / 6) × ((attacker STR + non-weapon STR + 50) / 50) × ((attacker DEX + equipment DEX + 100) / 100) × ((100 + Physical Attack adjustment) / 100).',
    'score.physicalDefense': 'With retail defaults: whole-number part of (B51 / 20) × ((defender VIT + equipment VIT + 25) / 25) × ((defender STR + equipment STR + 100) / 100) × ((100 + Physical Defense adjustment) / 100).',
    'score.sourceEffectiveLuck': 'Attacker effective Luck = attacker Luck + Current Gear item-field-B12 total.',
    'score.targetEffectiveLuck': 'Defender effective Luck = defender Luck + Current Gear item-field-B12 total.',
    'score.damageLuckDifference': 'Damage Luck difference = attacker effective Luck - defender effective Luck.',
    'score.targetResistance': 'Resistance = limit(class resistance + Current Gear resistance, 0, 100); damage multiplier = (100 - resistance) / 100.',
    'score.sourceAccuracy': 'With retail defaults: whole-number part of (attacker DEX + equipment DEX + 50) × ((attacker AGI + equipment AGI + 100) / 55) × ((100 + accuracy adjustment) / 100).',
    'score.targetEvasion': 'With retail defaults: whole-number part of (defender AGI + equipment AGI + 50) × ((defender DEX + equipment DEX + 100) / 50) × ((100 + evasion adjustment) / 100).',
    'score.specialHitBonus': 'Default = 0; the documented unresolved condition contributes +10 through the explicit override.',
    'score.baseHitThreshold': 'Limit(whole-number part of (accuracy / evasion) × (100 + attacker effective Luck - defender effective Luck) + special hit bonus, 0, 100).',
    'score.criticalThreshold': 'Critical comparison number = whole-number part of ((attacker Luck - defender Luck) / 2) + 5; equipment B12 is excluded.'
  };

  var CONSTANT_USES = {
    magicCoefficientDivisor: 'Divides the class Magic Attack Coefficient plus equipment Intelligence in the first part of the Magic Attack Score.',
    magicIntOffset: 'Added to attacker Intelligence in the second part of the Magic Attack Score.',
    magicIntDivisor: 'Divides attacker Intelligence plus the added amount in the Magic Attack Score.',
    magicMenOffset: 'Added to attacker Mentality plus equipment Mentality in the third part of the Magic Attack Score.',
    magicMenDivisor: 'Divides attacker Mentality, equipment Mentality, and the added amount.',
    magicDefenseCoefficientDivisor: 'Divides the defender class Magic Defense Coefficient in the Magic Defense Score.',
    magicDefenseMenOffset: 'Added to defender Mentality plus equipment Mentality in the Magic Defense Score.',
    magicDefenseMenDivisor: 'Divides that Mentality part of the Magic Defense Score.',
    magicDefenseIntOffset: 'Added to defender Intelligence plus equipment Intelligence in the Magic Defense Score.',
    magicDefenseIntDivisor: 'Divides that Intelligence part of the Magic Defense Score.',
    physicalCoefficientDivisor: 'Divides the class Physical Attack Coefficient plus recognized weapon Strength in the first part of the Physical Attack Score.',
    physicalStrOffset: 'Added to attacker Strength plus non-weapon equipment Strength in the Physical Attack Score.',
    physicalStrDivisor: 'Divides that Strength part of the Physical Attack Score.',
    physicalDexOffset: 'Added to attacker Dexterity plus equipment Dexterity in the Physical Attack Score.',
    physicalDexDivisor: 'Divides that Dexterity part of the Physical Attack Score.',
    defenseCoefficientDivisor: 'Divides the defender class Physical Defense Coefficient in the Physical Defense Score.',
    defenseVitOffset: 'Added to defender Vitality plus equipment Vitality in the Physical Defense Score.',
    defenseVitDivisor: 'Divides that Vitality part of the Physical Defense Score.',
    defenseStrOffset: 'Added to defender Strength plus equipment Strength in the Physical Defense Score.',
    defenseStrDivisor: 'Divides that Strength part of the Physical Defense Score.',
    modifierBase: 'Added to a battle adjustment before converting it into a percentage multiplier.',
    percentDivisor: 'Converts percentage adjustments and resistance into multipliers.',
    damageMinimum: 'Lowest allowed result after the decimal portion of the ordinary magic or physical damage result is discarded.',
    damageMaximum: 'Highest allowed result after the decimal portion of the ordinary magic or physical damage result is discarded.',
    varianceMinimum: 'Lowest random damage adjustment used for the displayed damage range.',
    varianceMaximum: 'Highest random damage adjustment used for the displayed damage range.',
    magicResistanceMinimum: 'Lowest target resistance allowed by the bounded Magic amount calculation.',
    magicResistanceMaximum: 'Highest target resistance allowed by the bounded Magic amount calculation.',
    accuracyDexOffset: 'Added to attacker Dexterity plus equipment Dexterity in the Attacker accuracy score.',
    accuracyAgiOffset: 'Added to attacker Agility plus equipment Agility in the Attacker accuracy score.',
    accuracyAgiDivisor: 'Divides the Agility part of the Attacker accuracy score.',
    evasionAgiOffset: 'Added to defender Agility plus equipment Agility in the Defender evasion score.',
    evasionDexOffset: 'Added to defender Dexterity plus equipment Dexterity in the Defender evasion score.',
    evasionDexDivisor: 'Divides the Dexterity part of the Defender evasion score.',
    hitLuckBase: 'Added before Attacker effective Luck minus Defender effective Luck in the hit comparison number.',
    chanceMinimum: 'Lowest allowed resistance or displayed hit comparison number.',
    chanceMaximum: 'Highest allowed resistance or displayed hit comparison number, and the value used by Force hit.',
    criticalLuckDivisor: 'Divides Attacker Luck minus Defender Luck before the decimal portion is discarded.',
    criticalBase: 'Added after the decimal portion of the divided Luck difference is discarded.',
    criticalMultiplier: 'Multiplies already calculated normal damage when the Luck-based critical check passes.'
  };

  function fieldExplanation(what, source, use, rule) {
    return 'What: ' + what + '\nSource: ' + source + '\nUse: ' + use +
      (rule ? '\nRule: ' + rule : '');
  }

  function buildDerivedFieldTooltip(key, label, note, mode) {
    var renderMode = mode === 'magic' ? 'magic' : 'physical';
    var info = DERIVED_FIELD_INFO[key];
    var rule = DERIVED_FIELD_RULES[key] || '';
    if (key === 'defender.currentHp' && renderMode === 'magic') {
      info = {
        source: 'Defaults to projected Defender maximum Hit Points and can be replaced with the defender\'s actual current Hit Points.',
        use: 'The selected amount is subtracted to show one selected action-55 nonlethal resulting-Hit-Points output. A lethal result is withheld by the bounded Magic product.'
      };
      rule = 'When selected action-55 damage is less than current Hit Points, remaining Hit Points = current Hit Points - selected damage; when it is lethal, the Hit-Points result is withheld.';
    }
    if ((key === 'score.sourceEffectiveLuck' || key === 'score.targetEffectiveLuck') &&
        renderMode === 'magic') {
      info = {
        source: key === 'score.sourceEffectiveLuck'
          ? 'Attacker Luck plus the total positive or negative equipment Luck adjustment stored in item field B12.'
          : 'Defender Luck plus the total positive or negative equipment Luck adjustment stored in item field B12.',
        use: 'In bounded Magic, this feeds the effective Luck difference used by the selected action-55 damage amount. Magic hit/success and critical/doubling outputs are unavailable.'
      };
    }
    if (key === 'score.damageLuckDifference' && renderMode === 'magic') {
      info = {
        source: 'Attacker effective Luck minus Defender effective Luck.',
        use: 'Added after the Magic Defense Score is subtracted from the Magic Attack Score in the selected action-55 damage calculation. Magic hit/success and critical/doubling outputs are unavailable.'
      };
    }
    if (!info) {
      var statMatch = /^(attacker|defender)\.(hp|str|vit|int|men|agi|dex|lck|alignment)$/.exec(key);
      if (statMatch) {
        var side = statMatch[1];
        var stat = statMatch[2];
        var sideLabel = side === 'attacker' ? 'Attacker' : 'Defender';
        if (stat === 'alignment') {
          var adjustmentName = renderMode === 'magic' ? 'Magic' : 'Physical';
          info = {
            source: sideLabel + ' Alignment defaults directly from the resolved selected class and is not projected by level or Growth Gear; override it with the actual Alignment when needed.',
            use: side === 'attacker'
              ? 'Used by the Snow/Marsh ' + adjustmentName + ' Attack adjustment. It does not feed the displayed Attacker accuracy adjustment.'
              : 'Used only by the supported Snow/Marsh ' + adjustmentName + ' Defense adjustment.'
          };
          rule = 'Snow = (Alignment - 50) / 10; Marsh = (50 - Alignment) / 10; Sky through Mountain use the saved Generic Spellbook selector, and Immobile uses 0.';
        } else {
          var statUse = STAT_FIELD_USES[stat][side];
          if (renderMode === 'magic' && stat === 'lck') {
            statUse = 'In bounded Magic, this contributes through effective Luck to the selected action-55 damage amount. Magic hit/success and critical/doubling outputs are unavailable.';
          } else if (renderMode === 'magic' && side === 'defender' && stat === 'hp') {
            statUse = 'Supplies the default Defender current Hit Points for one selected action-55 nonlethal resulting-Hit-Points output. A lethal Hit-Points result is withheld; this field does not change damage.';
          }
          info = {
            source: 'Expected ' + sideLabel + ' ' + STAT_DISPLAY_NAMES[stat] + ' projected from class growth history, level, and Growth Gear; override it with the actual final value.',
            use: statUse
          };
          rule = 'Projected value = resolved level-1 class base + expected class growth through the selected level + Growth Gear bonuses applied at each projected level-up.';
        }
      }
    }
    if (!info) {
      var equipMatch = /^(attacker|defender)\.equip\.(str|int|agi|dex|vit|men|b12)$/.exec(key);
      if (equipMatch) {
        var equipSide = equipMatch[1];
        var equipStat = equipMatch[2];
        var equipSideLabel = equipSide === 'attacker' ? 'Attacker' : 'Defender';
        var equipUse = EQUIPMENT_FIELD_USES[equipStat][equipSide];
        if (renderMode === 'magic' && equipStat === 'b12') {
          equipUse = 'In bounded Magic, this contributes to ' + equipSideLabel +
            ' effective Luck in the selected action-55 damage amount. Magic hit/success and critical/doubling outputs are unavailable.';
        }
        info = {
          source: 'Total positive or negative ' + STAT_DISPLAY_NAMES[equipStat] + ' supplied by the selected ' + equipSideLabel + ' Current Gear.',
          use: equipUse
        };
        rule = 'Equipment value = sum of this stat across the selected Current Gear records.';
      }
    }
    if (!info) {
      info = {
        source: 'Derived from the current class, level, action, gear, terrain, formation, and override selections.',
        use: 'Replaces the named value wherever it appears in the displayed calculation; values outside that calculation are shown for transparency.'
      };
    }
    var use = info.use;
    if (note) use += ' Guardrail: ' + note;
    return fieldExplanation(label, info.source, use, rule);
  }

  function constantTooltip(def) {
    return fieldExplanation(
      def.label,
      'Fixed number used by the documented retail-game calculation; default ' + def.value + '.',
      CONSTANT_USES[def.key] || 'Feeds the named formula term wherever it is displayed.',
      'The retail default is ' + def.value + '; enabling Override replaces this number everywhere the current calculation uses it.'
    );
  }

  function trunc(value) {
    if (!isFinite(value)) return value;
    return value < 0 ? Math.ceil(value) : Math.floor(value);
  }

  function clamp(value, minimum, maximum) {
    return Math.min(maximum, Math.max(minimum, value));
  }

  // The retail routines use MIPS single-precision add, subtract, multiply, and
  // divide instructions. Round after each operation so edge cases match that
  // arithmetic rather than JavaScript's default double precision.
  function float32(value) {
    return Math.fround(value);
  }

  function float32Add(left, right) {
    return Math.fround(Math.fround(left) + Math.fround(right));
  }

  function float32Subtract(left, right) {
    return Math.fround(Math.fround(left) - Math.fround(right));
  }

  function float32Multiply(left, right) {
    return Math.fround(Math.fround(left) * Math.fround(right));
  }

  function float32Divide(left, right) {
    return Math.fround(Math.fround(left) / Math.fround(right));
  }

  function float32Product(factors) {
    var value = Math.fround(factors[0]);
    for (var index = 1; index < factors.length; index++) {
      value = float32Multiply(value, factors[index]);
    }
    return value;
  }

  function numeric(value, fallback) {
    try {
      var n = Number(value);
      return isFinite(n) ? n : fallback;
    } catch (error) {
      return fallback;
    }
  }

  function hasOwn(object, key) {
    return !!object && Object.prototype.hasOwnProperty.call(object, key);
  }

  function getClassDef(rom, classId) {
    return rom && rom.classDefs ? rom.classDefs[classId + 1] || null : null;
  }

  function getActionDef(rom, actionId) {
    return rom && rom.actionDefs ? rom.actionDefs[actionId - 1] || null : null;
  }

  function getItem(rom, itemId) {
    if (!itemId || !rom || !Array.isArray(rom.itemStats) ||
        typeof itemId !== 'number' || !isFinite(itemId) || trunc(itemId) !== itemId ||
        itemId <= 0 || itemId >= rom.itemStats.length ||
        !hasOwn(rom.itemStats, itemId)) return null;
    var record = rom.itemStats[itemId];
    return record && typeof record === 'object' ? record : null;
  }

  function className(classId) {
    return OB64.className ? OB64.className(classId) : ('Class 0x' + Number(classId).toString(16).padStart(2, '0'));
  }

  function actionName(actionId) {
    if (OB64.actionEditorName) return OB64.actionEditorName(actionId);
    return OB64.actionName ? OB64.actionName(actionId) : ('Action ' + actionId);
  }

  function itemName(itemId) {
    if (!itemId) return '(none)';
    return OB64.itemName ? OB64.itemName(itemId) : ('Item 0x' + Number(itemId).toString(16));
  }

  function gearPresentationName(itemId) {
    if (typeof itemId !== 'number' || !isFinite(itemId) ||
        trunc(itemId) !== itemId || itemId < 0) {
      return '(invalid item ID)';
    }
    return itemName(itemId);
  }

  function validClassIds(rom) {
    var out = [];
    for (var id = 1; id <= 0xA4; id++) {
      var def = getClassDef(rom, id);
      if (def && !def.isTerm && !def.isSentinel) out.push(id);
    }
    return out;
  }

  function actionRoute(actionId, def) {
    // These fixed records are healing/status-capable and must never fall
    // through to a tuple-shaped damaging Magic route.
    if (actionId === 52 || actionId === 53 || actionId === 54) return null;
    if (DYNAMIC_ELEMENTAL_ACTIONS[actionId]) return 'magic';
    if (!def) return null;
    if (def.family === 3 && (def.category === 7 || def.category === 8) &&
        (def.effectType === 1 || def.effectType === 8)) {
      return 'magic';
    }
    // The verified standard physical families use this tuple with D0 0 or 1
    // (basic attacks and Shoot). Other physical-looking modes stay excluded.
    if (def.category === 1 && def.effectType === 1 && def.attackMode === 1 &&
        (def.family === 0 || def.family === 1)) {
      return 'physical';
    }
    return null;
  }

  function nativeActionsForClass(rom, classId, mode) {
    var def = getClassDef(rom, classId);
    if (!def || !def.rowAttacks) return [];
    var byId = {};
    for (var row = 0; row < 3; row++) {
      var entry = def.rowAttacks[row];
      if (!entry || !entry.attackId) continue;
      var actionDef = getActionDef(rom, entry.attackId);
      var route = actionRoute(entry.attackId, actionDef);
      if (route !== mode) continue;
      if (!byId[entry.attackId]) {
        byId[entry.attackId] = {
          id: entry.attackId,
          def: actionDef,
          route: route,
          rows: [],
          counts: []
        };
      }
      byId[entry.attackId].rows.push(row);
      byId[entry.attackId].counts.push(entry.count);
    }
    return Object.keys(byId).map(function(id) { return byId[id]; });
  }

  function sanitizeGear(rom, gear, strictItemIds) {
    var out = [0, 0, 0, 0];
    for (var i = 0; i < out.length; i++) {
      var rawId = gear && gear[i];
      var id = strictItemIds
        ? (typeof rawId === 'number' && isFinite(rawId) &&
            trunc(rawId) === rawId ? rawId : 0)
        : numeric(rawId, 0);
      out[i] = id > 0 && getItem(rom, id) ? id : 0;
    }
    return out;
  }

  function gearPathLabel(path) {
    var labels = {
      'attacker.growthGear': 'Attacker Growth Gear',
      'attacker.currentGear': 'Attacker Current Gear',
      'defender.growthGear': 'Defender Growth Gear',
      'defender.currentGear': 'Defender Current Gear'
    };
    return labels[path] || path;
  }

  function invalidGearValidation(path, reasonCode, kind, slot, itemId) {
    var label = gearPathLabel(path);
    var reason;
    if (reasonCode === 'unknown-gear') {
      reason = label + ' slot ' + (slot + 1) + ' contains unknown item ID ' + itemId +
        '. Magic requires four explicit slots containing item 0 or real parsed item records.';
    } else if (kind === 'length') {
      reason = label + ' must contain exactly four explicit slots; Magic is unavailable.';
    } else if (kind === 'inherited-property') {
      reason = label + ' is inherited rather than owned by the combatant. Magic requires an explicit own four-slot vector.';
    } else if (kind === 'missing-property') {
      reason = label + ' is not an own combatant property. Magic requires an explicit own four-slot vector.';
    } else if (kind === 'inherited-slot') {
      reason = label + ' slot ' + (slot + 1) +
        ' is inherited rather than owned by the array. Magic requires four explicit own slots.';
    } else if (kind === 'missing-slot') {
      reason = label + ' slot ' + (slot + 1) +
        ' is a sparse hole. Magic requires four explicit own slots; item 0 means an empty slot.';
    } else if (kind === 'missing') {
      reason = label + ' is missing. Magic requires an explicit four-slot vector; item 0 means an empty slot.';
    } else if (kind === 'non-array') {
      reason = label + ' is not an array. Magic requires an explicit four-slot vector; item 0 means an empty slot.';
    } else {
      reason = label + ' slot ' + (slot + 1) +
        ' must contain a finite nonnegative integer item ID; Magic is unavailable.';
    }
    return {
      valid: false,
      path: path,
      reasonCode: reasonCode,
      kind: kind,
      slot: slot === undefined ? null : slot,
      itemId: itemId === undefined ? null : itemId,
      reason: reason
    };
  }

  function validateMagicGearVector(rom, gear, path) {
    if (gear === null || gear === undefined) {
      return invalidGearValidation(path, 'malformed-gear', 'missing');
    }
    if (!Array.isArray(gear)) {
      return invalidGearValidation(path, 'malformed-gear', 'non-array');
    }
    if (gear.length !== 4) {
      return invalidGearValidation(path, 'malformed-gear', 'length');
    }
    for (var slot = 0; slot < gear.length; slot++) {
      if (!hasOwn(gear, slot)) {
        return invalidGearValidation(
          path,
          'malformed-gear',
          slot in gear ? 'inherited-slot' : 'missing-slot',
          slot
        );
      }
      var itemId = gear[slot];
      if (typeof itemId !== 'number' || !isFinite(itemId) || trunc(itemId) !== itemId || itemId < 0) {
        return invalidGearValidation(path, 'malformed-gear', 'invalid-id', slot, itemId);
      }
      if (itemId !== 0 && !getItem(rom, itemId)) {
        return invalidGearValidation(path, 'unknown-gear', 'unknown-id', slot, itemId);
      }
    }
    return {
      valid: true,
      path: path,
      reasonCode: null,
      kind: null,
      slot: null,
      itemId: null,
      reason: null
    };
  }

  function validateMagicGearState(rom, state) {
    var paths = [
      { side: 'attacker', key: 'growthGear' },
      { side: 'attacker', key: 'currentGear' },
      { side: 'defender', key: 'growthGear' },
      { side: 'defender', key: 'currentGear' }
    ];
    for (var index = 0; index < paths.length; index++) {
      var entry = paths[index];
      if (!state || !state[entry.side]) {
        return {
          valid: false,
          path: entry.side,
          reasonCode: 'malformed-state',
          kind: 'missing-combatant',
          slot: null,
          itemId: null,
          reason: 'The retained calculator state is incomplete, so Magic is unavailable.'
        };
      }
      if (!hasOwn(state[entry.side], entry.key)) {
        return invalidGearValidation(
          entry.side + '.' + entry.key,
          'malformed-gear',
          entry.key in state[entry.side] ? 'inherited-property' : 'missing-property'
        );
      }
      var validation = validateMagicGearVector(
        rom,
        state[entry.side][entry.key],
        entry.side + '.' + entry.key
      );
      if (!validation.valid) return validation;
    }
    return {
      valid: true,
      path: null,
      reasonCode: null,
      kind: null,
      slot: null,
      itemId: null,
      reason: null
    };
  }

  function validatedMagicGearCopy(rom, gear, path) {
    var validation = validateMagicGearVector(rom, gear, path);
    return {
      validation: validation,
      gear: validation.valid ? [gear[0], gear[1], gear[2], gear[3]] : null
    };
  }

  function rawDefaultGearForClass(rom, classId) {
    var def = getClassDef(rom, classId);
    return def && hasOwn(def, 'defaultEquip') ? def.defaultEquip : null;
  }

  function defaultGearForClass(rom, classId) {
    var def = getClassDef(rom, classId);
    return sanitizeGear(rom, def && def.defaultEquip ? def.defaultEquip : []);
  }

  function growthBonuses(rom, gear, strictItemIds) {
    var total = { hp: 0, str: 0, vit: 0, int: 0, men: 0, agi: 0, dex: 0, lck: 0 };
    sanitizeGear(rom, gear, strictItemIds).forEach(function(id) {
      var item = getItem(rom, id);
      if (!item) return;
      total.hp += numeric(item.growthHpStr, 0);
      total.str += numeric(item.growthHpStr, 0);
      total.int += numeric(item.growthInt, 0);
      total.agi += numeric(item.growthAgi, 0);
      total.dex += numeric(item.growthDex, 0);
      total.vit += numeric(item.growthVit, 0);
      total.men += numeric(item.growthMen, 0);
      total.lck += numeric(item.growthLck, 0);
    });
    return total;
  }

  function resolveGrowthClass(rom, targetClassId, level) {
    var target = getClassDef(rom, targetClassId);
    if (!target) return targetClassId;
    var first = numeric(target.baseTransitionLevel, 0);
    var second = numeric(target.finalTransitionLevel, 0);
    if (first > 0 && level < first && getClassDef(rom, target.baseClass)) return target.baseClass;
    if (second > 0 && level < second && getClassDef(rom, target.intermediateClass)) return target.intermediateClass;
    return targetClassId;
  }

  function projectExpectedStats(rom, targetClassId, level, growthGear, strictItemIds) {
    level = clamp(trunc(numeric(level, 1)), 1, 50);
    var startingClassId = resolveGrowthClass(rom, targetClassId, 1);
    var start = getClassDef(rom, startingClassId) || getClassDef(rom, targetClassId);
    if (!start) {
      return { hp: 0, str: 0, vit: 0, int: 0, men: 0, agi: 0, dex: 0, lck: 0, alignment: 0 };
    }
    var stats = {
      hp: numeric(start.baseHp, 0),
      str: numeric(start.stats && start.stats[0] && start.stats[0].base, 0),
      vit: numeric(start.stats && start.stats[1] && start.stats[1].base, 0),
      int: numeric(start.stats && start.stats[2] && start.stats[2].base, 0),
      men: numeric(start.stats && start.stats[3] && start.stats[3].base, 0),
      agi: numeric(start.stats && start.stats[4] && start.stats[4].base, 0),
      dex: numeric(start.stats && start.stats[5] && start.stats[5].base, 0),
      lck: numeric(start.lck, 0),
      alignment: numeric((getClassDef(rom, targetClassId) || start).alignment, 0)
    };
    var gearGrowth = growthBonuses(rom, growthGear, strictItemIds);
    var statNames = ['str', 'vit', 'int', 'men', 'agi', 'dex'];
    for (var currentLevel = 2; currentLevel <= level; currentLevel++) {
      var growthClassId = resolveGrowthClass(rom, targetClassId, currentLevel);
      var growthClass = getClassDef(rom, growthClassId) || start;
      stats.hp = clamp(stats.hp + numeric(growthClass.hpGrowth, 0) + 1 + gearGrowth.hp, 0, 999);
      for (var s = 0; s < statNames.length; s++) {
        var stat = statNames[s];
        var baseGain = numeric(growthClass.stats && growthClass.stats[s] && growthClass.stats[s].g1, 0);
        stats[stat] = clamp(stats[stat] + baseGain + 1 + gearGrowth[stat], 0, 999);
      }
      stats.lck = clamp(stats.lck + gearGrowth.lck, 0, 100);
    }
    return stats;
  }

  function isWeapon(item) {
    return !!item && ((item.equipType >= 1 && item.equipType <= 13) || item.equipType === 24);
  }

  function equipmentTotals(rom, gear, strictItemIds) {
    var totals = {
      str: 0, int: 0, agi: 0, dex: 0, vit: 0, men: 0, b12: 0,
      resistances: [0, 0, 0, 0, 0, 0, 0],
      weaponStr: 0, nonweaponStr: 0, weaponElement: 0, bookVariant: 0,
      hasWeapon: false, hasSpellbook: false, spellbookElement: -1,
      antiDragonAttack: false, antiDragonDefense: false,
      specialItem84: false
    };
    var weaponFound = false;
    sanitizeGear(rom, gear, strictItemIds).forEach(function(id) {
      var item = getItem(rom, id);
      if (!item) return;
      if (id === 0x84) totals.specialItem84 = true;
      totals.str += numeric(item.str, 0);
      totals.int += numeric(item.int, 0);
      totals.agi += numeric(item.agi, 0);
      totals.dex += numeric(item.dex, 0);
      totals.vit += numeric(item.vit, 0);
      totals.men += numeric(item.men, 0);
      totals.b12 += numeric(item.b12, 0);
      totals.resistances[0] += numeric(item.resPhys, 0);
      totals.resistances[1] += numeric(item.resWind, 0);
      totals.resistances[2] += numeric(item.resFire, 0);
      totals.resistances[3] += numeric(item.resEarth, 0);
      totals.resistances[4] += numeric(item.resWater, 0);
      totals.resistances[5] += numeric(item.resVirtue, 0);
      totals.resistances[6] += numeric(item.resBane, 0);
      if (isWeapon(item)) {
        if (!weaponFound) {
          weaponFound = true;
          totals.hasWeapon = true;
          totals.weaponStr = numeric(item.str, 0);
          totals.weaponElement = item.element >= 0 && item.element <= 6 ? item.element : 0;
        }
      } else {
        totals.nonweaponStr += numeric(item.str, 0);
      }
      if (item.equipType === 23 && !totals.hasSpellbook) {
        totals.hasSpellbook = true;
        totals.spellbookElement = numeric(item.element, 0);
      }
      if ((numeric(item.b23Raw, 0) & 1) !== 0) totals.antiDragonAttack = true;
      if ((numeric(item.b24Raw, 0) & 1) !== 0) totals.antiDragonDefense = true;
    });
    return totals;
  }

  function terrainMovement(moveType, terrainId) {
    var row = TERRAIN_MOVEMENT[moveType - 1];
    return row && terrainId >= 0 && terrainId < row.length ? row[terrainId] : 0;
  }

  function bookOrAlignment(moveType, bookVariant, alignment) {
    if (moveType === 1) return [0, 4, 0, -4, 0][bookVariant] || 0;
    if (moveType === 2) return [0, 0, 4, 0, -4][bookVariant] || 0;
    if (moveType === 3) return [0, -4, 0, 4, 0][bookVariant] || 0;
    if (moveType === 4) return [0, 0, -4, 0, 4][bookVariant] || 0;
    if (moveType === 5) return (alignment - 50) / 10;
    if (moveType === 6) return (50 - alignment) / 10;
    return 0;
  }

  function elementAdjustment(weaponElement, actionElement) {
    if (weaponElement < 1 || weaponElement > 6 || actionElement < 1 || actionElement > 6) return 0;
    if (weaponElement === actionElement) return 2;
    if ((weaponElement === 1 && actionElement === 3) || (weaponElement === 3 && actionElement === 1) ||
        (weaponElement === 2 && actionElement === 4) || (weaponElement === 4 && actionElement === 2) ||
        (weaponElement === 5 && actionElement === 6) || (weaponElement === 6 && actionElement === 5)) return -2;
    return 0;
  }

  // The runtime modifier reduces combined action elements to the first
  // (primary) ordinary element before consulting the weapon/action table.
  // Raw D1: 9 Wind+Flame, 10 Flame+Wind, 11 Earth+Flame,
  // 12 Water+Earth, 13 Wind+Bane, 14 Flame+Bane.
  function primaryActionElement(rawElement) {
    rawElement = numeric(rawElement, 0);
    if (rawElement >= 0 && rawElement <= 6) return rawElement;
    if (rawElement === 9 || rawElement === 13) return 1;
    if (rawElement === 10 || rawElement === 14) return 2;
    if (rawElement === 11) return 3;
    if (rawElement === 12) return 4;
    return 0;
  }

  function resolveVariableActionElement(gear, classDefaultElement, genericBookVariant) {
    gear = gear || {};
    if (gear.hasSpellbook) {
      var bookElement = numeric(gear.spellbookElement, 0);
      if (bookElement >= 1 && bookElement <= 6) return bookElement;
      if (bookElement === 8) {
        var variant = numeric(genericBookVariant, 0);
        if (variant >= 1 && variant <= 4) return variant;
        return primaryActionElement(classDefaultElement);
      }
      return primaryActionElement(bookElement);
    }
    var weaponElement = numeric(gear.weaponElement, 0);
    if (weaponElement >= 1 && weaponElement <= 6) return weaponElement;
    return primaryActionElement(classDefaultElement);
  }

  function nativeProductActionsForClass(rom, classId, mode) {
    if (mode === 'physical') return nativeActionsForClass(rom, classId, 'physical');
    if (mode !== 'magic') return [];
    return nativeActionsForClass(rom, classId, 'magic').filter(function(action) {
      return !!ACTION55_PRODUCT_TEMPLATES[action.id] && !!action.def;
    });
  }

  function acceptedAction55Definition(def) {
    return !!def && def.family === 3 && def.element === 1 && def.row === 1 &&
      def.effectType === 1 && def.attackMode === 1 && def.category === 7 &&
      def.secondaryA === 0xFF && def.secondaryB === 0xFF;
  }

  function templateDisplayName(templateId) {
    if (templateId === 45) return 'T1';
    if (templateId === 51) return 'Fixed Lightning';
    return 'Template';
  }

  function resolvedSpellDisplayName(actionId) {
    return RESOLVED_SPELL_NAMES[actionId] || (actionId ? actionName(actionId) : 'Unresolved');
  }

  function resolveTier1Context(rom, state) {
    var attackerDef = getClassDef(rom, state.attacker.classId);
    var gear = equipmentTotals(rom, state.attacker.currentGear);
    if (gear.hasSpellbook) {
      if (gear.spellbookElement === 7) {
        return { element: null, source: 'Drakonite Spellbook redirect', reasonCode: 'drakonite-redirect' };
      }
      if (gear.spellbookElement === 8) {
        var selector = state.overrides && Object.prototype.hasOwnProperty.call(state.overrides, 'attacker.equip.bookVariant')
          ? numeric(state.overrides['attacker.equip.bookVariant'], 0)
          : numeric(gear.bookVariant, 0);
        if (selector >= 1 && selector <= 4 && trunc(selector) === selector) {
          return { element: selector, source: 'Generic Spellbook saved selector', reasonCode: null };
        }
        return { element: null, source: 'Generic Spellbook saved selector', reasonCode: 'generic-selector-required' };
      }
      if (gear.spellbookElement >= 1 && gear.spellbookElement <= 6) {
        return { element: gear.spellbookElement, source: 'equipped Spellbook', reasonCode: null };
      }
      return { element: null, source: 'equipped Spellbook', reasonCode: 'unresolved-spellbook-element' };
    }
    if (gear.hasWeapon) {
      if (gear.weaponElement >= 1 && gear.weaponElement <= 6) {
        return { element: gear.weaponElement, source: 'first recognized weapon', reasonCode: null };
      }
      return { element: null, source: 'first recognized weapon', reasonCode: 'unresolved-weapon-element' };
    }
    var classElement = numeric(attackerDef && attackerDef.dragonElement, -1);
    if (classElement >= 1 && classElement <= 6 && trunc(classElement) === classElement) {
      return { element: classElement, source: 'class default element (B58)', reasonCode: null };
    }
    return { element: null, source: 'class default element (B58)', reasonCode: 'unresolved-class-element' };
  }

  function resolveAction55ProductPolicy(rom, state) {
    var templateId = state && numeric(state.actionId, 0);
    var policy = {
      templateId: templateId || 0,
      templateLabel: templateDisplayName(templateId),
      native: false,
      resolvedElement: null,
      resolutionSource: null,
      resolvedActionId: null,
      resolvedSpellName: 'Unresolved',
      selector: 0,
      targetPattern: 1,
      casterCount: 1,
      fullPowerPrimaryTarget: true,
      eligible: false,
      reasonCode: 'malformed-state',
      reason: 'The retained calculator state is incomplete, so Magic is unavailable.',
      effectiveFamily: null,
      effectiveElement: null,
      gearValidation: null
    };
    if (!state || !state.attacker || !getClassDef(rom, state.attacker.classId)) return policy;

    var gearValidation = validateMagicGearState(rom, state);
    policy.gearValidation = gearValidation;
    if (!gearValidation.valid) {
      policy.reasonCode = gearValidation.reasonCode;
      policy.reason = gearValidation.reason;
      return policy;
    }

    var candidates = nativeProductActionsForClass(rom, state.attacker.classId, 'magic');
    var selected = null;
    for (var index = 0; index < candidates.length; index++) {
      if (candidates[index].id === templateId && candidates[index].rows.indexOf(state.actionRow) !== -1) {
        selected = candidates[index];
        break;
      }
    }
    if (!selected) {
      policy.reasonCode = ACTION55_PRODUCT_TEMPLATES[templateId]
        ? 'template-not-native-in-selected-row'
        : 'selected-template-outside-slice';
      policy.reason = ACTION55_PRODUCT_TEMPLATES[templateId]
        ? 'The selected template is not native to this class in the selected row.'
        : 'Only native template 45 (T1) and native template 51 (Fixed Lightning) can enter this Magic slice.';
      return policy;
    }
    policy.native = true;

    if (templateId === 51) {
      policy.resolvedElement = 1;
      policy.resolutionSource = 'fixed template 51';
      policy.resolvedActionId = 55;
    } else if (templateId === 45) {
      var tier1 = resolveTier1Context(rom, state);
      policy.resolvedElement = tier1.element;
      policy.resolutionSource = tier1.source;
      if (tier1.reasonCode) {
        policy.reasonCode = tier1.reasonCode;
        policy.reason = tier1.reasonCode === 'drakonite-redirect'
          ? 'The selected Spellbook redirects to a Drakonite action, which is unavailable in this product slice.'
          : tier1.reasonCode === 'generic-selector-required'
            ? 'The Generic Spellbook needs the character\'s saved element selector before its spell can be resolved.'
            : 'The template element is unresolved, Random/None, or unknown, so no Magic amount is available.';
        return policy;
      }
      policy.resolvedActionId = TIER1_RESOLVED_ACTIONS[tier1.element] || null;
    }
    policy.resolvedSpellName = resolvedSpellDisplayName(policy.resolvedActionId);

    if (policy.resolvedActionId !== 55) {
      policy.reasonCode = 'resolved-action-outside-slice';
      policy.reason = 'The template resolves to ' + policy.resolvedSpellName + ' (' + policy.resolvedActionId + '), not Lightning (55).';
      return policy;
    }
    if (!acceptedAction55Definition(getActionDef(rom, 55))) {
      policy.reasonCode = 'action55-definition-mismatch';
      policy.reason = 'Resolved action 55 does not match the accepted ordinary selector-0 / pattern-1 action definition.';
      return policy;
    }

    var hasFamilyOverride = state.overrides &&
      Object.prototype.hasOwnProperty.call(state.overrides, 'action.family');
    policy.effectiveFamily = hasFamilyOverride
      ? numeric(state.overrides['action.family'], null)
      : numeric(selected.def && selected.def.family, null);
    if (policy.effectiveFamily !== 3) {
      policy.reasonCode = hasFamilyOverride
        ? 'action-family-override-outside-slice'
        : 'action-family-outside-slice';
      policy.reason = hasFamilyOverride
        ? 'The action-family override no longer matches accepted action-55 D0 value 3, so the bounded Magic amount is unavailable.'
        : 'The selected template no longer supplies accepted action-55 D0 value 3, so the bounded Magic amount is unavailable.';
      return policy;
    }

    var hasElementOverride = state.overrides &&
      Object.prototype.hasOwnProperty.call(state.overrides, 'action.element');
    policy.effectiveElement = hasElementOverride
      ? numeric(state.overrides['action.element'], null)
      : templateId === 45
        ? policy.resolvedElement
        : primaryActionElement(numeric(selected.def && selected.def.element, 0));
    if (policy.effectiveElement !== 1) {
      policy.reasonCode = hasElementOverride
        ? 'element-override-outside-slice'
        : 'action-element-outside-slice';
      policy.reason = hasElementOverride
        ? 'The action-element override no longer represents Wind/Lightning, so the bounded Magic amount is unavailable.'
        : 'The selected template no longer supplies the effective Wind/Lightning element required by action 55.';
      return policy;
    }

    policy.eligible = true;
    policy.reasonCode = null;
    policy.reason = 'Eligible resolved action-55 ordinary single-caster full-power primary-target slice.';
    return policy;
  }

  function formationAdjustment(family, row) {
    if (family === 0xFF) family = 0;
    return FORMATION_ADJUSTMENT[family] ? FORMATION_ADJUSTMENT[family][row] || 0 : 0;
  }

  function isDragonClass(classId) {
    return (classId >= 0x38 && classId <= 0x44) || classId === 0xA4;
  }

  function leaderMonsterAffinity(leaderClassId, memberClassId) {
    if (!leaderClassId) return 0;
    var groupB = memberClassId >= 0x45 && memberClassId <= 0x4C;
    var groupG = memberClassId >= 0x4E && memberClassId <= 0x50;
    var groupD = memberClassId >= 0x38 && memberClassId <= 0x44;
    var groupX = memberClassId === 0x31 || memberClassId === 0x34 || memberClassId === 0x35 || memberClassId === 0x37;
    if (leaderClassId === 0x09 && (groupB || groupG || groupD || groupX)) return 10;
    if (leaderClassId === 0x0A && (groupG || groupD || groupX)) return 10;
    if (leaderClassId === 0x0E && (groupD || groupX)) return 10;
    if (leaderClassId === 0x18 || leaderClassId === 0x5A) return groupB ? 15 : (groupG || groupD || groupX ? 10 : 0);
    if (leaderClassId === 0x19 || leaderClassId === 0x7B) return groupG ? 15 : (groupD || groupX ? 10 : 0);
    if (leaderClassId === 0x1D) return groupD ? 15 : (groupX ? 10 : 0);
    if ((leaderClassId === 0x2B || leaderClassId === 0x36) && groupX) return 10;
    if (leaderClassId === 0x74 && groupX) return 15;
    return 0;
  }

  function fatiguePenalty(fatigue) {
    if (fatigue < 70) return 0;
    if (fatigue < 80) return 5;
    if (fatigue < 90) return 18;
    return 40;
  }

  function exactCriticalProbability(threshold) {
    threshold = trunc(threshold);
    if (threshold <= 0) return 0;
    if (threshold >= 100) return 1;
    if (threshold <= 68) return (328 * threshold) / 32768;
    return (327 * threshold + 68) / 32768;
  }

  function physicalDamage(attackScore, defenseScore, luckDifference, resistance, variance, constants) {
    resistance = clamp(resistance, constants.chanceMinimum, constants.chanceMaximum);
    var scoreAfterDefenseAndLuck = float32Add(
      float32Subtract(attackScore, defenseScore),
      luckDifference
    );
    var resistanceMultiplier = float32Divide(
      float32Subtract(constants.percentDivisor, resistance),
      constants.percentDivisor
    );
    var randomMultiplier = float32Divide(
      float32Add(constants.percentDivisor, variance),
      constants.percentDivisor
    );
    var raw = trunc(float32Multiply(
      float32Multiply(scoreAfterDefenseAndLuck, resistanceMultiplier),
      randomMultiplier
    ));
    return {
      raw: raw,
      damage: clamp(raw, constants.damageMinimum, constants.damageMaximum)
    };
  }

  function constantValues(state) {
    var out = {};
    CONSTANT_DEFS.forEach(function(def) {
      out[def.key] = state && state.constantOverrides && Object.prototype.hasOwnProperty.call(state.constantOverrides, def.key)
        ? numeric(state.constantOverrides[def.key], def.value)
        : def.value;
    });
    return out;
  }

  function derive(rom, state, strictItemIds) {
    var base = {};
    var values = {};
    var overrides = state.overrides || {};
    function field(key, derived) {
      base[key] = derived;
      values[key] = Object.prototype.hasOwnProperty.call(overrides, key)
        ? numeric(overrides[key], derived)
        : derived;
      return values[key];
    }

    var constants = constantValues(state);
    var attackerDef = getClassDef(rom, state.attacker.classId);
    var defenderDef = getClassDef(rom, state.defender.classId);
    var attackerProjected = projectExpectedStats(
      rom,
      state.attacker.classId,
      state.attacker.level,
      state.attacker.growthGear,
      strictItemIds
    );
    var defenderProjected = projectExpectedStats(
      rom,
      state.defender.classId,
      state.defender.level,
      state.defender.growthGear,
      strictItemIds
    );
    var attackerGear = equipmentTotals(rom, state.attacker.currentGear, strictItemIds);
    var defenderGear = equipmentTotals(rom, state.defender.currentGear, strictItemIds);
    var actionDef = getActionDef(rom, state.actionId);

    ['hp', 'str', 'vit', 'int', 'men', 'agi', 'dex', 'lck', 'alignment'].forEach(function(stat) {
      field('attacker.' + stat, attackerProjected[stat]);
      field('defender.' + stat, defenderProjected[stat]);
    });
    field('defender.currentHp', values['defender.hp']);

    ['str', 'int', 'agi', 'dex', 'vit', 'men', 'b12'].forEach(function(stat) {
      field('attacker.equip.' + stat, attackerGear[stat]);
      field('defender.equip.' + stat, defenderGear[stat]);
    });
    field('attacker.equip.weaponStr', attackerGear.weaponStr);
    field('attacker.equip.nonweaponStr', attackerGear.nonweaponStr);
    field('attacker.equip.weaponElement', attackerGear.weaponElement);
    field('attacker.equip.bookVariant', attackerGear.bookVariant);
    field('defender.equip.bookVariant', defenderGear.bookVariant);

    var familyDefault = actionDef ? actionDef.family : 0;
    if (state.actionId === 1 && familyDefault === 0xFF) familyDefault = 0;
    var actionFamily = field('action.family', familyDefault);
    var actionRecordElement = numeric(actionDef && actionDef.element,
      numeric(attackerDef && attackerDef.dragonElement, 0));
    var rawDefaultElement = state.mode === 'physical' ? 0 :
      (VARIABLE_ELEMENT_ACTIONS[state.actionId]
        ? resolveVariableActionElement({
            hasSpellbook: attackerGear.hasSpellbook,
            spellbookElement: attackerGear.spellbookElement,
            weaponElement: values['attacker.equip.weaponElement']
          }, numeric(attackerDef && attackerDef.dragonElement, 0), values['attacker.equip.bookVariant'])
        : actionRecordElement);
    var defaultElement = primaryActionElement(rawDefaultElement);
    var actionElement = field('action.element', defaultElement);

    var attackerMove = numeric(attackerDef && attackerDef.moveType, 0);
    var defenderMove = numeric(defenderDef && defenderDef.moveType, 0);
    field('attacker.moveType', attackerMove);
    field('defender.moveType', defenderMove);
    var attackerTerrain = field('attacker.context.terrainMovement', terrainMovement(values['attacker.moveType'], state.attacker.terrainId));
    var defenderTerrain = field('defender.context.terrainMovement', terrainMovement(values['defender.moveType'], state.defender.terrainId));
    var attackerBook = field('attacker.context.bookOrAlignment', bookOrAlignment(
      values['attacker.moveType'], values['attacker.equip.bookVariant'], values['attacker.alignment']
    ));
    var defenderBook = field('defender.context.bookOrAlignment', bookOrAlignment(
      values['defender.moveType'], values['defender.equip.bookVariant'], values['defender.alignment']
    ));
    var weaponElement = field('attacker.context.weaponSpellElement', elementAdjustment(
      values['attacker.equip.weaponElement'], actionElement
    ));
    var formation = field('attacker.context.formation', formationAdjustment(actionFamily, state.actionRow));
    var attackerLeader = field('attacker.context.leaderAffinity', leaderMonsterAffinity(state.attacker.leaderClassId, state.attacker.classId));
    var defenderLeader = field('defender.context.leaderAffinity', leaderMonsterAffinity(state.defender.leaderClassId, state.defender.classId));
    var attackerFatigue = field('attacker.context.fatiguePenalty', fatiguePenalty(state.attacker.fatigue));
    var defenderFatigue = field('defender.context.fatiguePenalty', fatiguePenalty(state.defender.fatigue));
    var antiDragonAttack = field('attacker.context.antiDragon',
      isDragonClass(state.defender.classId) && (state.attacker.classId === 0x14 || attackerGear.antiDragonAttack) ? 5 : 0);
    var antiDragonDefense = field('defender.context.antiDragon',
      isDragonClass(state.attacker.classId) && (state.defender.classId === 0x14 || defenderGear.antiDragonDefense) ? 5 : 0);
    var specialDefenseTerrain = field('defender.context.specialTerrainState', 0);

    var magicAttackAdjustment = field('attacker.context.magicModifier',
      attackerTerrain + attackerBook + weaponElement + formation + antiDragonAttack + attackerLeader - attackerFatigue);
    var physicalAttackAdjustment = field('attacker.context.physicalModifier',
      attackerTerrain + attackerBook + formation + antiDragonAttack + attackerLeader - attackerFatigue);
    var derivedDefenseAdjustment =
      defenderTerrain + defenderBook + antiDragonDefense + defenderLeader - defenderFatigue + specialDefenseTerrain;
    var physicalDefenseAdjustment = field('defender.context.physicalDefenseModifier', derivedDefenseAdjustment);
    var magicDefenseAdjustment = field('defender.context.magicDefenseModifier', derivedDefenseAdjustment);

    var b49 = field('attacker.coefficient.physicalAttack', numeric(attackerDef && attackerDef.physAtk, 0));
    var b50 = field('attacker.coefficient.magicAttack', numeric(attackerDef && attackerDef.magAtk, 0));
    var b51 = field('defender.coefficient.physicalDefense', numeric(defenderDef && defenderDef.physDef, 0));
    var b52 = field('defender.coefficient.magicDefense', numeric(defenderDef && defenderDef.magDef, 0));

    var magicAttackScore = field('score.magicAttack', trunc(float32Product([
      float32Divide(b50 + values['attacker.equip.int'], constants.magicCoefficientDivisor),
      float32Divide(values['attacker.int'] + constants.magicIntOffset, constants.magicIntDivisor),
      float32Divide(values['attacker.men'] + values['attacker.equip.men'] + constants.magicMenOffset, constants.magicMenDivisor),
      float32Divide(float32Add(magicAttackAdjustment, constants.modifierBase), constants.percentDivisor)
    ])));

    var physicalAttackScore = field('score.physicalAttack', trunc(float32Product([
      float32Divide(b49 + values['attacker.equip.weaponStr'], constants.physicalCoefficientDivisor),
      float32Divide(values['attacker.str'] + values['attacker.equip.nonweaponStr'] + constants.physicalStrOffset, constants.physicalStrDivisor),
      float32Divide(values['attacker.dex'] + values['attacker.equip.dex'] + constants.physicalDexOffset, constants.physicalDexDivisor),
      float32Divide(float32Add(physicalAttackAdjustment, constants.modifierBase), constants.percentDivisor)
    ])));
    var physicalDefenseScore = field('score.physicalDefense', trunc(float32Product([
      float32Divide(b51, constants.defenseCoefficientDivisor),
      float32Divide(values['defender.vit'] + values['defender.equip.vit'] + constants.defenseVitOffset, constants.defenseVitDivisor),
      float32Divide(values['defender.str'] + values['defender.equip.str'] + constants.defenseStrOffset, constants.defenseStrDivisor),
      float32Divide(float32Add(physicalDefenseAdjustment, constants.modifierBase), constants.percentDivisor)
    ])));
    var magicDefenseScore = field('score.magicDefense', trunc(float32Product([
      float32Divide(b52, constants.magicDefenseCoefficientDivisor),
      float32Divide(values['defender.men'] + values['defender.equip.men'] + constants.magicDefenseMenOffset, constants.magicDefenseMenDivisor),
      float32Divide(values['defender.int'] + values['defender.equip.int'] + constants.magicDefenseIntOffset, constants.magicDefenseIntDivisor),
      float32Divide(float32Add(magicDefenseAdjustment, constants.modifierBase), constants.percentDivisor)
    ])));

    var sourceEffectiveLuck = field('score.sourceEffectiveLuck', values['attacker.lck'] + values['attacker.equip.b12']);
    var targetEffectiveLuck = field('score.targetEffectiveLuck', values['defender.lck'] + values['defender.equip.b12']);
    var damageLuckDifference = field('score.damageLuckDifference', sourceEffectiveLuck - targetEffectiveLuck);

    var classResistance = defenderDef && defenderDef.resistances && defenderDef.resistances[actionElement] !== undefined
      ? numeric(defenderDef.resistances[actionElement], 0) : 0;
    var equipmentResistance = defenderGear.resistances[actionElement] || 0;
    var resistanceMinimum = state.mode === 'magic' ? constants.magicResistanceMinimum : constants.chanceMinimum;
    var resistanceMaximum = state.mode === 'magic' ? constants.magicResistanceMaximum : constants.chanceMaximum;
    var resistance = field('score.targetResistance', clamp(classResistance + equipmentResistance, resistanceMinimum, resistanceMaximum));

    var sourceAccuracyContext = field('attacker.context.accuracyModifier', attackerLeader - attackerFatigue);
    var targetEvasionContext = field('defender.context.evasionModifier', defenderTerrain + defenderLeader - defenderFatigue);
    var sourceAccuracy = field('score.sourceAccuracy', trunc(float32Product([
      float32(values['attacker.dex'] + values['attacker.equip.dex'] + constants.accuracyDexOffset),
      float32Divide(values['attacker.agi'] + values['attacker.equip.agi'] + constants.accuracyAgiOffset, constants.accuracyAgiDivisor),
      float32Divide(float32Add(sourceAccuracyContext, constants.modifierBase), constants.percentDivisor)
    ])));
    var targetEvasion = field('score.targetEvasion', trunc(float32Product([
      float32(values['defender.agi'] + values['defender.equip.agi'] + constants.evasionAgiOffset),
      float32Divide(values['defender.dex'] + values['defender.equip.dex'] + constants.evasionDexOffset, constants.evasionDexDivisor),
      float32Divide(float32Add(targetEvasionContext, constants.modifierBase), constants.percentDivisor)
    ])));
    var specialHitBonus = field('score.specialHitBonus', 0);
    var baseHitThreshold = targetEvasion === 0 ? null : field('score.baseHitThreshold', clamp(
      trunc(float32Multiply(
        float32Divide(sourceAccuracy, targetEvasion),
        constants.hitLuckBase + sourceEffectiveLuck - targetEffectiveLuck
      )) + specialHitBonus,
      constants.chanceMinimum,
      constants.chanceMaximum
    ));
    if (targetEvasion === 0) {
      base['score.baseHitThreshold'] = null;
      values['score.baseHitThreshold'] = Object.prototype.hasOwnProperty.call(overrides, 'score.baseHitThreshold')
        ? numeric(overrides['score.baseHitThreshold'], null)
        : null;
      baseHitThreshold = values['score.baseHitThreshold'];
    }

    var criticalThreshold = field('score.criticalThreshold',
      trunc((values['attacker.lck'] - values['defender.lck']) / constants.criticalLuckDivisor) + constants.criticalBase);
    var criticalChance = exactCriticalProbability(criticalThreshold);

    var normalDamage;
    var varianceLow = null;
    var varianceHigh = null;
    var rawNormal = null;
    var selectedRandomAdjustment = 0;
    if (state.mode === 'magic') {
      var magicDamageConstants = Object.assign({}, constants, {
        chanceMinimum: constants.magicResistanceMinimum,
        chanceMaximum: constants.magicResistanceMaximum
      });
      var varianceMinimum = Math.min(constants.varianceMinimum, constants.varianceMaximum);
      var varianceMaximum = Math.max(constants.varianceMinimum, constants.varianceMaximum);
      selectedRandomAdjustment = clamp(trunc(numeric(state.magicRandomAdjustment, 0)), varianceMinimum, varianceMaximum);
      var magicNormal = physicalDamage(magicAttackScore, magicDefenseScore, damageLuckDifference, resistance, selectedRandomAdjustment, magicDamageConstants);
      var magicLow = physicalDamage(magicAttackScore, magicDefenseScore, damageLuckDifference, resistance, varianceMinimum, magicDamageConstants);
      var magicHigh = physicalDamage(magicAttackScore, magicDefenseScore, damageLuckDifference, resistance, varianceMaximum, magicDamageConstants);
      rawNormal = magicNormal.raw;
      normalDamage = magicNormal.damage;
      varianceLow = Math.min(magicLow.damage, magicHigh.damage);
      varianceHigh = Math.max(magicLow.damage, magicHigh.damage);
    } else {
      var normal = physicalDamage(physicalAttackScore, physicalDefenseScore, damageLuckDifference, resistance, 0, constants);
      var low = physicalDamage(physicalAttackScore, physicalDefenseScore, damageLuckDifference, resistance, constants.varianceMinimum, constants);
      var high = physicalDamage(physicalAttackScore, physicalDefenseScore, damageLuckDifference, resistance, constants.varianceMaximum, constants);
      rawNormal = normal.raw;
      normalDamage = normal.damage;
      varianceLow = Math.min(low.damage, high.damage);
      varianceHigh = Math.max(low.damage, high.damage);
    }
    var criticalDamage = normalDamage * constants.criticalMultiplier;
    var successThreshold = baseHitThreshold;
    if (state.forcedSuccess === 'hit') successThreshold = constants.chanceMaximum;
    if (state.forcedSuccess === 'miss') successThreshold = constants.chanceMinimum;

    var warnings = [];
    if (attackerGear.specialItem84 || defenderGear.specialItem84) {
      warnings.push('Item 0x84 receives some equipment-stat bonuses from live battle state instead of fixed item data. Manually override the affected equipment totals for an exact scenario.');
    }
    if (state.mode === 'magic') {
      warnings.push('The defense-only +3 context condition remains unresolved. Its default is 0; use the explicit Unresolved special Defense adjustment override only when the intended battle context is known.');
    }
    if (VARIABLE_ELEMENT_ACTIONS[state.actionId]) {
      if (attackerGear.hasSpellbook && attackerGear.spellbookElement === 7) {
        warnings.push('This special Spellbook redirects the selected spell to a Drakonite action. No supported Magic amount is available.');
      } else if (actionElement === 0 && attackerGear.hasSpellbook && attackerGear.spellbookElement === 8) {
        warnings.push('The Generic Spellbook needs the character-specific saved element selector. Set that selector; an action-element override cannot grant product eligibility.');
      } else {
        warnings.push('Template 45 resolves from the equipped Spellbook first, otherwise the first recognized weapon, otherwise class B58. This resolution is Supported static rather than separately runtime accepted.');
      }
    }
    if (!nativeProductActionsForClass(rom, state.attacker.classId, state.mode).some(function(a) { return a.id === state.actionId; })) {
      warnings.push('The selected action is not one of this class\'s native actions with a documented calculator formula.');
    }
    if (targetEvasion === 0) warnings.push('Defender evasion is zero; the documented calculation would divide by zero, so no hit comparison number is displayed.');

    return {
      base: base,
      values: values,
      constants: constants,
      actionDef: actionDef,
      warnings: warnings,
      outputs: {
        normalDamage: normalDamage,
        rawNormalDamage: rawNormal,
        criticalDamage: criticalDamage,
        varianceLow: varianceLow,
        varianceHigh: varianceHigh,
        successThreshold: successThreshold,
        criticalThreshold: criticalThreshold,
        criticalChance: criticalChance,
        selectedRandomAdjustment: selectedRandomAdjustment,
        hpAfterNormal: Math.max(0, values['defender.currentHp'] - normalDamage),
        hpAfterCritical: Math.max(0, values['defender.currentHp'] - criticalDamage)
      }
    };
  }

  function deriveProduct(rom, state) {
    var productPolicy = state && state.mode === 'magic'
      ? resolveAction55ProductPolicy(rom, state)
      : {
          templateId: state ? state.actionId : 0,
          resolvedActionId: state ? state.actionId : 0,
          eligible: true,
          reasonCode: null,
          reason: 'Supported Physical product route.'
        };
    var result = derive(rom, state, state && state.mode === 'magic');
    result.productPolicy = productPolicy;
    if (state.mode === 'magic' && !productPolicy.eligible) {
      result.warnings.push(productPolicy.reason);
      result.productOutputs = {
        selectedRandomAdjustment: result.outputs.selectedRandomAdjustment,
        damageAtSelectedAdjustment: null,
        rangeLow: null,
        rangeHigh: null,
        hpAfterSelectedDamage: null,
        nonlethal: null
      };
      return result;
    }

    var nonlethal = result.outputs.normalDamage < result.values['defender.currentHp'];
    result.productOutputs = {
      selectedRandomAdjustment: result.outputs.selectedRandomAdjustment,
      damageAtSelectedAdjustment: result.outputs.normalDamage,
      rangeLow: result.outputs.varianceLow,
      rangeHigh: result.outputs.varianceHigh,
      hpAfterSelectedDamage: nonlethal ? result.values['defender.currentHp'] - result.outputs.normalDamage : null,
      nonlethal: nonlethal
    };
    if (state.mode === 'magic' && !nonlethal) {
      result.warnings.push('The selected amount reaches the lethal branch. Lethal handling is unavailable in this product slice, so no post-damage Hit Points number is shown.');
    }
    return result;
  }

  function firstSupportedClass(rom, mode, preferred) {
    if (preferred && nativeProductActionsForClass(rom, preferred, mode).length) return preferred;
    var ids = validClassIds(rom);
    for (var i = 0; i < ids.length; i++) {
      if (nativeProductActionsForClass(rom, ids[i], mode).length) return ids[i];
    }
    return ids[0] || 1;
  }

  // Product policy boundary: a structurally valid retained Magic state may
  // remain Magic, but each mode finishes with one of its native product
  // actions. Incomplete top-level state falls back to Physical; invalid Magic
  // gear identity stays intact so policy can reject it without materialization.
  function normalizeProductState(rom, state) {
    if (!state || !state.attacker || !state.defender ||
        (state.mode !== 'physical' && state.mode !== 'magic')) return makeDefaultState(rom);
    state.attacker.classId = trunc(numeric(state.attacker.classId, 0));
    state.defender.classId = trunc(numeric(state.defender.classId, 0));
    if (!getClassDef(rom, state.attacker.classId) || !getClassDef(rom, state.defender.classId)) {
      return makeDefaultState(rom);
    }
    var retainedMagicGear = state.mode === 'magic'
      ? validateMagicGearState(rom, state)
      : null;
    state.overrides = state.overrides && typeof state.overrides === 'object' ? state.overrides : {};
    state.constantOverrides = state.constantOverrides && typeof state.constantOverrides === 'object' ? state.constantOverrides : {};
    state.magicRandomAdjustment = clamp(trunc(numeric(state.magicRandomAdjustment, 0)), -10, 10);
    state.forcedSuccess = state.mode === 'physical' &&
      (state.forcedSuccess === 'hit' || state.forcedSuccess === 'miss') ? state.forcedSuccess : 'none';
    ['attacker', 'defender'].forEach(function(side) {
      var defaults = defaultGearForClass(rom, state[side].classId);
      if (state.mode === 'magic') {
        if (retainedMagicGear.valid) {
          state[side].growthGear = [
            state[side].growthGear[0],
            state[side].growthGear[1],
            state[side].growthGear[2],
            state[side].growthGear[3]
          ];
          state[side].currentGear = [
            state[side].currentGear[0],
            state[side].currentGear[1],
            state[side].currentGear[2],
            state[side].currentGear[3]
          ];
        }
      } else {
        state[side].growthGear = sanitizeGear(rom,
          Array.isArray(state[side].growthGear) ? state[side].growthGear : defaults);
        state[side].currentGear = sanitizeGear(rom,
          Array.isArray(state[side].currentGear) ? state[side].currentGear : defaults);
      }
      state[side].level = clamp(trunc(numeric(state[side].level, 1)), 1, 50);
      state[side].terrainId = clamp(trunc(numeric(state[side].terrainId, 3)), 0, 26);
      state[side].fatigue = clamp(trunc(numeric(state[side].fatigue, 0)), 0, 255);
      state[side].leaderClassId = trunc(numeric(state[side].leaderClassId, 0));
    });
    if (!nativeProductActionsForClass(rom, state.attacker.classId, state.mode).length) {
      if (state.mode === 'magic' && !retainedMagicGear.valid) return state;
      var preferred = state.mode === 'physical' ? 0x01 : 0;
      resetClassDefaults(rom, state, 'attacker', firstSupportedClass(rom, state.mode, preferred));
    }
    if (!ensureSupportedAction(rom, state)) {
      return state.mode === 'magic' ? state : makeDefaultState(rom);
    }
    return state;
  }

  function firstEligibleMagicDestination(rom, state) {
    var retainedValidation = validateMagicGearState(rom, state);
    if (!retainedValidation.valid) return null;
    var defenderGrowth = validatedMagicGearCopy(
      rom,
      state.defender.growthGear,
      'defender.growthGear'
    ).gear;
    var defenderCurrent = validatedMagicGearCopy(
      rom,
      state.defender.currentGear,
      'defender.currentGear'
    ).gear;
    var classIds = validClassIds(rom);
    if (classIds.indexOf(state.attacker.classId) !== -1) {
      classIds.splice(classIds.indexOf(state.attacker.classId), 1);
      classIds.unshift(state.attacker.classId);
    }
    for (var classIndex = 0; classIndex < classIds.length; classIndex++) {
      var classId = classIds[classIndex];
      var actions = nativeProductActionsForClass(rom, classId, 'magic');
      if (!actions.length) continue;
      var useCurrentGear = classId === state.attacker.classId;
      var candidateGrowthGear = useCurrentGear
        ? state.attacker.growthGear
        : rawDefaultGearForClass(rom, classId);
      var candidateCurrentGear = useCurrentGear
        ? state.attacker.currentGear
        : rawDefaultGearForClass(rom, classId);
      var growthCandidate = validatedMagicGearCopy(
        rom,
        candidateGrowthGear,
        'attacker.growthGear'
      );
      var currentCandidate = validatedMagicGearCopy(
        rom,
        candidateCurrentGear,
        'attacker.currentGear'
      );
      if (!growthCandidate.validation.valid || !currentCandidate.validation.valid) continue;
      for (var actionIndex = 0; actionIndex < actions.length; actionIndex++) {
        for (var rowIndex = 0; rowIndex < actions[actionIndex].rows.length; rowIndex++) {
          var probe = Object.assign({}, state, {
            mode: 'magic',
            actionId: actions[actionIndex].id,
            actionRow: actions[actionIndex].rows[rowIndex],
            forcedSuccess: 'none',
            overrides: Object.assign({}, state.overrides),
            constantOverrides: Object.assign({}, state.constantOverrides),
            attacker: Object.assign({}, state.attacker, {
              classId: classId,
              growthGear: growthCandidate.gear.slice(),
              currentGear: currentCandidate.gear.slice()
            }),
            defender: Object.assign({}, state.defender, {
              growthGear: defenderGrowth.slice(),
              currentGear: defenderCurrent.slice()
            })
          });
          delete probe.overrides['action.family'];
          delete probe.overrides['action.element'];
          var policy = resolveAction55ProductPolicy(rom, probe);
          if (policy.eligible) {
            return {
              classId: classId,
              growthGear: growthCandidate.gear,
              currentGear: currentCandidate.gear,
              actionId: actions[actionIndex].id,
              actionRow: actions[actionIndex].rows[rowIndex],
              resolvedActionId: policy.resolvedActionId
            };
          }
        }
      }
    }
    return null;
  }

  function transitionProductMode(rom, state, mode) {
    var normalized = normalizeProductState(rom, state);
    if (mode !== 'physical' && mode !== 'magic') return makeDefaultState(rom);
    if (mode === 'magic') {
      var destination = firstEligibleMagicDestination(rom, normalized);
      if (!destination) {
        return normalized.mode === 'physical' ? normalized : makeDefaultState(rom);
      }
      normalized.mode = 'magic';
      normalized.forcedSuccess = 'none';
      delete normalized.overrides['action.family'];
      delete normalized.overrides['action.element'];
      normalized.attacker.classId = destination.classId;
      normalized.attacker.growthGear = destination.growthGear.slice();
      normalized.attacker.currentGear = destination.currentGear.slice();
      normalized.actionId = destination.actionId;
      normalized.actionRow = destination.actionRow;
      var installed = normalizeProductState(rom, normalized);
      var installedPolicy = resolveAction55ProductPolicy(rom, installed);
      if (!installedPolicy.eligible ||
          installed.attacker.classId !== destination.classId ||
          installed.actionId !== destination.actionId ||
          installed.actionRow !== destination.actionRow ||
          installedPolicy.resolvedActionId !== destination.resolvedActionId) {
        return makeDefaultState(rom);
      }
      return installed;
    }
    normalized.mode = 'physical';
    normalized.forcedSuccess = 'none';
    delete normalized.overrides['action.family'];
    delete normalized.overrides['action.element'];
    if (!nativeProductActionsForClass(rom, normalized.attacker.classId, mode).length) {
      resetClassDefaults(rom, normalized, 'attacker', firstSupportedClass(rom, mode, mode === 'physical' ? 0x01 : 0));
    }
    ensureSupportedAction(rom, normalized);
    return normalizeProductState(rom, normalized);
  }

  function makeDefaultState(rom) {
    var attackerClass = firstSupportedClass(rom, 'physical', 0x01);
    var defenderClass = getClassDef(rom, 0x02) ? 0x02 : firstSupportedClass(rom, 'physical', 0x01);
    var actions = nativeProductActionsForClass(rom, attackerClass, 'physical');
    var action = actions[0] || null;
    var attackerGear = defaultGearForClass(rom, attackerClass);
    var defenderGear = defaultGearForClass(rom, defenderClass);
    return {
      mode: 'physical',
      actionId: action ? action.id : 0,
      actionRow: action && action.rows.length ? action.rows[0] : 1,
      magicRandomAdjustment: 0,
      forcedSuccess: 'none',
      overrides: {},
      constantOverrides: {},
      attacker: {
        classId: attackerClass,
        level: 1,
        growthGear: attackerGear.slice(),
        currentGear: attackerGear.slice(),
        terrainId: 3,
        fatigue: 0,
        leaderClassId: 0
      },
      defender: {
        classId: defenderClass,
        level: 1,
        growthGear: defenderGear.slice(),
        currentGear: defenderGear.slice(),
        terrainId: 3,
        fatigue: 0,
        leaderClassId: 0
      }
    };
  }

  function elementOptions() {
    return ELEMENT_NAMES.map(function(label, value) { return { value: value, label: label }; });
  }

  function bookOptions() {
    return BOOK_NAMES.map(function(label, value) { return { value: value, label: label }; });
  }

  function movementOptions() {
    return MOVEMENT_NAMES.map(function(label, index) { return { value: index, label: label }; });
  }

  function htmlElement(tag, className, textValue) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (textValue !== undefined && textValue !== null) node.textContent = textValue;
    return node;
  }

  var activeTooltipAnchor = null;

  function damageTooltipPopup() {
    var popup = document.getElementById('damage-tooltip-popup');
    if (!popup) {
      popup = document.createElement('div');
      popup.id = 'damage-tooltip-popup';
      popup.className = 'damage-tooltip-popup';
      popup.setAttribute('role', 'tooltip');
      popup.hidden = true;
      document.body.appendChild(popup);
    }
    return popup;
  }

  function showDamageTooltip(anchor, text) {
    var popup = damageTooltipPopup();
    activeTooltipAnchor = anchor;
    popup.textContent = text;
    popup.hidden = false;
    popup.style.left = '0px';
    popup.style.top = '0px';
    var margin = 10;
    var gap = 8;
    var anchorRect = anchor.getBoundingClientRect();
    var popupRect = popup.getBoundingClientRect();
    var left = anchorRect.left + (anchorRect.width - popupRect.width) / 2;
    left = Math.max(margin, Math.min(left, window.innerWidth - popupRect.width - margin));
    var top = anchorRect.top - popupRect.height - gap;
    if (top < margin) top = Math.min(window.innerHeight - popupRect.height - margin, anchorRect.bottom + gap);
    popup.style.left = Math.max(margin, left) + 'px';
    popup.style.top = Math.max(margin, top) + 'px';
  }

  function hideDamageTooltip(anchor) {
    if (anchor && activeTooltipAnchor !== anchor) return;
    var popup = document.getElementById('damage-tooltip-popup');
    if (popup) popup.hidden = true;
    activeTooltipAnchor = null;
  }

  function tooltipBadge(text) {
    var badge = htmlElement('span', 'damage-tooltip-badge', '?');
    badge.tabIndex = 0;
    badge.setAttribute('role', 'button');
    badge.setAttribute('aria-label', text);
    badge.addEventListener('mouseenter', function() { showDamageTooltip(badge, text); });
    badge.addEventListener('mouseleave', function() {
      if (document.activeElement !== badge) hideDamageTooltip(badge);
    });
    badge.addEventListener('focus', function() { showDamageTooltip(badge, text); });
    badge.addEventListener('blur', function() { hideDamageTooltip(badge); });
    badge.addEventListener('click', function(event) {
      event.preventDefault();
      event.stopPropagation();
      showDamageTooltip(badge, text);
    });
    badge.addEventListener('keydown', function(event) {
      if (event.key === 'Escape') {
        hideDamageTooltip(badge);
        badge.blur();
      }
    });
    return badge;
  }

  function labelWithTooltip(tag, className, label, tooltip) {
    var node = htmlElement(tag, className);
    node.appendChild(document.createTextNode(label));
    node.appendChild(tooltipBadge(tooltip || fieldExplanation(
      label,
      'Selected or derived from the current calculator scenario.',
      'Feeds the named value wherever it appears in the displayed calculation.'
    )));
    return node;
  }

  function formatNumber(value, maximumDecimals) {
    if (value === null || value === undefined || !isFinite(value)) return '—';
    if (Math.round(value) === value) return String(value);
    return Number(value).toFixed(maximumDecimals === undefined ? 3 : maximumDecimals).replace(/0+$/, '').replace(/\.$/, '');
  }

  function formatSignedPercent(value) {
    return (Number(value) > 0 ? '+' : '') + formatNumber(value) + '%';
  }

  function hexId(value, width) {
    return '0x' + Number(value || 0).toString(16).toUpperCase().padStart(width || 2, '0');
  }

  function ensureSupportedAction(rom, state) {
    var actions = nativeProductActionsForClass(rom, state.attacker.classId, state.mode);
    var selected = null;
    for (var i = 0; i < actions.length; i++) {
      if (actions[i].id === state.actionId) selected = actions[i];
    }
    if (!selected) {
      selected = actions[0] || null;
      state.actionId = selected ? selected.id : 0;
    }
    if (selected && selected.rows.indexOf(state.actionRow) === -1) state.actionRow = selected.rows[0];
    return selected;
  }

  function resetClassDefaults(rom, state, side, classId) {
    state[side].classId = classId;
    var gear = state.mode === 'magic'
      ? rawDefaultGearForClass(rom, classId)
      : defaultGearForClass(rom, classId);
    if (state.mode === 'magic') {
      var growthGear = validatedMagicGearCopy(rom, gear, side + '.growthGear');
      var currentGear = validatedMagicGearCopy(rom, gear, side + '.currentGear');
      state[side].growthGear = growthGear.validation.valid ? growthGear.gear : gear;
      state[side].currentGear = currentGear.validation.valid ? currentGear.gear : gear;
    } else {
      state[side].growthGear = Array.isArray(gear) ? gear.slice() : gear;
      state[side].currentGear = Array.isArray(gear) ? gear.slice() : gear;
    }
    if (side === 'attacker') ensureSupportedAction(rom, state);
  }

  function render(panel, rom, options) {
    if (!panel || !rom) return;
    options = options || {};
    if (!panel._damageCalculatorState || panel._damageCalculatorRom !== rom) {
      panel._damageCalculatorState = makeDefaultState(rom);
      panel._damageCalculatorRom = rom;
    }
    var state = normalizeProductState(rom, panel._damageCalculatorState);
    panel._damageCalculatorState = state;
    var result = deriveProduct(rom, state);
    hideDamageTooltip();
    panel.innerHTML = '';

    function rerender() {
      render(panel, rom, options);
    }

    function pickerButton(label, value, onClick, disabled, tooltip) {
      var wrap = htmlElement('div', 'damage-picker-field');
      wrap.appendChild(labelWithTooltip('span', 'damage-control-label', label, tooltip));
      var button = htmlElement('button', 'damage-picker-button', value);
      button.type = 'button';
      button.disabled = !!disabled;
      button.addEventListener('click', onClick);
      wrap.appendChild(button);
      return wrap;
    }

    function numberControl(label, value, min, max, onChange, tooltip) {
      var wrap = htmlElement('label', 'damage-control');
      wrap.appendChild(labelWithTooltip('span', 'damage-control-label', label, tooltip));
      var input = document.createElement('input');
      input.type = 'number';
      input.value = value;
      if (min !== undefined) input.min = min;
      if (max !== undefined) input.max = max;
      input.addEventListener('change', function() {
        var next = numeric(input.value, value);
        if (min !== undefined) next = Math.max(min, next);
        if (max !== undefined) next = Math.min(max, next);
        onChange(next);
      });
      wrap.appendChild(input);
      return wrap;
    }

    function selectControl(label, value, choices, onChange, tooltip) {
      var wrap = htmlElement('label', 'damage-control');
      wrap.appendChild(labelWithTooltip('span', 'damage-control-label', label, tooltip));
      var select = document.createElement('select');
      choices.forEach(function(choice) {
        var option = document.createElement('option');
        option.value = choice.value;
        option.textContent = choice.label;
        if (String(choice.value) === String(value)) option.selected = true;
        select.appendChild(option);
      });
      select.addEventListener('change', function() { onChange(select.value); });
      wrap.appendChild(select);
      return wrap;
    }

    function openClassPicker(side, allowNone) {
      if (!options.openPicker) return;
      var items = [];
      if (allowNone) items.push({ id: 0, name: '(none)', kind: 'class', kindLabel: 'No leader bonus' });
      validClassIds(rom).forEach(function(id) {
        var count = nativeProductActionsForClass(rom, id, state.mode).length;
        items.push({
          id: id,
          name: className(id),
          kind: 'class',
          kindLabel: allowNone ? hexId(id) : (hexId(id) + ' • ' + count + ' supported native ' + (count === 1 ? 'action' : 'actions'))
        });
      });
      options.openPicker({
        title: allowNone ? 'Choose squad leader class' : 'Choose ' + side + ' class',
        items: items,
        currentId: state[side].classId,
        withIcons: false,
        onSelect: function(id) {
          if (allowNone) {
            state[side].leaderClassId = id;
          } else {
            resetClassDefaults(rom, state, side, id);
          }
          rerender();
        }
      });
    }

    function openLeaderPicker(side) {
      if (!options.openPicker) return;
      var items = [{ id: 0, name: '(none)', kind: 'class', kindLabel: 'No leader bonus' }];
      validClassIds(rom).forEach(function(id) {
        items.push({ id: id, name: className(id), kind: 'class', kindLabel: hexId(id) });
      });
      options.openPicker({
        title: 'Choose ' + side + ' squad leader class',
        items: items,
        currentId: state[side].leaderClassId,
        withIcons: false,
        onSelect: function(id) {
          state[side].leaderClassId = id;
          rerender();
        }
      });
    }

    function openActionPicker() {
      if (!options.openPicker) return;
      var actions = nativeProductActionsForClass(rom, state.attacker.classId, state.mode);
      options.openPicker({
        title: state.mode === 'magic' ? 'Choose a native action-55 template' : 'Choose a native physical action',
        items: actions.map(function(action) {
          var rowLabels = action.rows.map(function(row, index) {
            return ROW_NAMES[row] + ' ×' + action.counts[index];
          });
          return {
            id: action.id,
            name: state.mode === 'magic' ? templateDisplayName(action.id) : actionName(action.id),
            kind: 'action',
            kindLabel: hexId(action.id) + ' • ' + rowLabels.join(', ')
          };
        }),
        currentId: state.actionId,
        withIcons: false,
        onSelect: function(id) {
          state.actionId = id;
          ensureSupportedAction(rom, state);
          rerender();
        }
      });
    }

    function openGearPicker(side, gearKind, slot) {
      if (!options.openPicker) return;
      var currentVector = state[side][gearKind];
      var items = [{ id: 0, name: '(none)', kind: 'equip', kindLabel: 'Empty slot' }];
      for (var id = 1; id < rom.itemStats.length; id++) {
        var item = getItem(rom, id);
        if (!item) continue;
        var typeName = OB64.equipTypeName ? OB64.equipTypeName(item.equipType) : ('Type ' + hexId(item.equipType));
        items.push({ id: id, name: itemName(id), kind: 'equip', kindLabel: hexId(id, 3) + ' • ' + typeName });
      }
      options.openPicker({
        title: 'Choose ' + (gearKind === 'growthGear' ? 'growth' : 'current') + ' gear — ' + SLOT_NAMES[slot],
        items: items,
        currentId: Array.isArray(currentVector) && hasOwn(currentVector, slot)
          ? currentVector[slot]
          : 0,
        withIcons: true,
        onSelect: function(id) {
          var current = state[side][gearKind];
          var currentValidation = validateMagicGearVector(
            rom,
            current,
            side + '.' + gearKind
          );
          var next = currentValidation.valid
            ? [current[0], current[1], current[2], current[3]]
            : [0, 0, 0, 0];
          next[slot] = id;
          state[side][gearKind] = next;
          rerender();
        }
      });
    }

    function gearGroup(side, gearKind, title) {
      var group = htmlElement('div', 'damage-gear-group');
      group.appendChild(htmlElement('h4', '', title));
      var grid = htmlElement('div', 'damage-gear-grid');
      var gearVector = state[side][gearKind];
      for (var slot = 0; slot < 4; slot++) {
        (function(slotIndex) {
          var itemId = Array.isArray(gearVector) && hasOwn(gearVector, slotIndex)
            ? gearVector[slotIndex]
            : null;
          grid.appendChild(pickerButton(
            SLOT_NAMES[slotIndex],
            itemId === null || itemId === undefined ? '(missing)' : gearPresentationName(itemId),
            function() { openGearPicker(side, gearKind, slotIndex); },
            false,
            gearKind === 'growthGear'
              ? fieldExplanation(
                  title + ' ' + SLOT_NAMES[slotIndex],
                  'Starts from the selected class default equipment and is replaced through the item modal.',
                  'Its per-level stat-growth values (item record fields B20 and B21) are applied at every projected level-up; it does not supply current battle stats.',
                  'Projected contribution = the item\'s B20/B21 growth bonuses applied once per modeled level-up while that Growth Gear selection is used.'
                )
              : fieldExplanation(
                  title + ' ' + SLOT_NAMES[slotIndex],
                  'Starts from the selected class default equipment and reads the chosen item from the loaded game data.',
                  'Supplies current battle stats, the Luck adjustment (item record field B12), resistances, recognized weapon or Spellbook element, and anti-dragon bonuses; it does not alter projected growth.',
                  'Current equipment terms are sums across the four selected records; B23 bit 0 enables anti-dragon attack and B24 bit 0 enables anti-dragon defense when the opposing dragon condition also holds.'
                )
          ));
        })(slot);
      }
      group.appendChild(grid);
      var note = gearKind === 'growthGear'
        ? 'Applied to every projected level-up. This is an explicit calculator assumption and can differ from a real character\'s equipment history.'
        : 'Supplies the equipment terms used by the selected combat formula.';
      group.appendChild(htmlElement('p', 'damage-muted', note));
      return group;
    }

    function combatantCard(side, title) {
      var card = htmlElement('section', 'damage-input-card');
      card.appendChild(htmlElement('h3', '', title));
      var controls = htmlElement('div', 'damage-control-grid');
      controls.appendChild(pickerButton('Class', className(state[side].classId) + ' (' + hexId(state[side].classId) + ')', function() {
        openClassPicker(side, false);
      }, false, fieldExplanation(
        title + ' class',
        'Selected from the class records in the loaded game data.',
        side === 'attacker'
          ? 'Supplies base and growth stats, Physical and Magic Attack Coefficients (class fields B49 and B50), movement type, class default element (B58), default gear, and native actions.'
          : 'Supplies base and growth stats, Physical and Magic Defense Coefficients (class fields B51 and B52), movement type, resistances, default gear, and defender battle adjustments.',
        side === 'attacker'
          ? 'Physical Attack begins with B49 plus recognized weapon Strength; Magic Attack begins with B50 plus equipment Intelligence.'
          : 'Physical Defense begins with B51 divided by its fixed divisor; Magic Defense begins with B52 divided by its fixed divisor.'
      )));
      controls.appendChild(numberControl('Level', state[side].level, 1, 50, function(value) {
        state[side].level = trunc(value);
        rerender();
      }, fieldExplanation(
        title + ' level',
        'User-selected level from 1 to 50.',
        'Chooses the class transition and growth history (class fields B53 through B56) and the number of expected level-up steps used to derive final stats.',
        'Projected final stats follow the resolved class history through this level, adding expected class growth and the selected Growth Gear bonuses for each modeled level-up.'
      )));
      controls.appendChild(selectControl('Terrain', state[side].terrainId,
        TERRAIN_NAMES.map(function(name, id) { return { value: id, label: id + ' — ' + name }; }),
        function(value) { state[side].terrainId = Number(value); rerender(); },
        fieldExplanation(
          title + ' terrain',
          'Selected battlefield terrain interpreted through the game\'s 27-terrain adjustment table.',
          side === 'attacker'
            ? 'Combines with movement type to produce the terrain part of the Magic Attack and Physical Attack adjustments.'
            : 'Combines with movement type to produce the terrain part of the Magic Defense and Physical Defense adjustments and the Defender evasion score.',
          'Terrain adjustment = terrain table[this side\'s movement type][selected terrain]. The same terrain does not cancel because each side is looked up independently and scales a separate score.'
        )));
      controls.appendChild(pickerButton('Squad leader class', state[side].leaderClassId
        ? className(state[side].leaderClassId) + ' (' + hexId(state[side].leaderClassId) + ')'
        : '(none)', function() { openLeaderPicker(side); }, false, fieldExplanation(
          title + ' squad leader class',
          'Optional class selected from the loaded class table; None applies no leader compatibility bonus.',
          side === 'attacker'
            ? 'Chooses the built-in leader compatibility bonus added to the Magic Attack adjustment, Physical Attack adjustment, and Attacker accuracy adjustment.'
            : 'Chooses the built-in leader compatibility bonus added to the Magic Defense adjustment, Physical Defense adjustment, and Defender evasion adjustment.',
          'Leader compatibility = table[selected leader class][member monster group], producing 0, +10, or +15; None produces 0.'
        )));
      controls.appendChild(numberControl('Fatigue', state[side].fatigue, 0, 255, function(value) {
        state[side].fatigue = value;
        rerender();
      }, fieldExplanation(
        title + ' Fatigue',
        'User-selected character Fatigue value from 0 to 255.',
        side === 'attacker'
          ? 'Chooses the 0, 5, 18, or 40 penalty subtracted from the Magic Attack adjustment, Physical Attack adjustment, and Attacker accuracy adjustment.'
          : 'Chooses the 0, 5, 18, or 40 penalty subtracted from the Magic Defense adjustment, Physical Defense adjustment, and Defender evasion adjustment.',
        'Fatigue below 70 = 0; 70–79 = 5; 80–89 = 18; 90 or above = 40.'
      )));
      card.appendChild(controls);
      card.appendChild(gearGroup(side, 'growthGear', 'Growth gear'));
      card.appendChild(gearGroup(side, 'currentGear', 'Current gear'));
      return card;
    }

    function derivedFieldTooltip(key, label, note) {
      return buildDerivedFieldTooltip(key, label, note, state.mode);
    }

    function derivedField(meta) {
      var key = meta.key;
      var overridden = Object.prototype.hasOwnProperty.call(state.overrides, key);
      var baseValue = result.base[key];
      var actualValue = result.values[key];
      var wrap = htmlElement('div', 'damage-derived-field' + (overridden ? ' is-overridden' : ''));
      wrap.dataset.fieldKey = key;
      var heading = htmlElement('div', 'damage-derived-heading');
      heading.appendChild(labelWithTooltip('span', 'damage-derived-label', meta.label,
        derivedFieldTooltip(key, meta.label, meta.note)));
      if (overridden) heading.appendChild(htmlElement('span', 'damage-override-badge', 'Override'));
      wrap.appendChild(heading);

      var inputRow = htmlElement('div', 'damage-derived-input-row');
      var input;
      if (meta.options) {
        input = document.createElement('select');
        meta.options.forEach(function(choice) {
          var option = document.createElement('option');
          option.value = choice.value;
          option.textContent = choice.label;
          if (String(choice.value) === String(actualValue)) option.selected = true;
          input.appendChild(option);
        });
      } else {
        input = document.createElement('input');
        input.type = 'number';
        input.step = meta.step || 'any';
        input.value = actualValue === null || actualValue === undefined ? '' : actualValue;
        if (meta.min !== undefined) input.min = meta.min;
        if (meta.max !== undefined) input.max = meta.max;
        if (actualValue === null || actualValue === undefined) input.placeholder = 'Unavailable';
      }
      input.setAttribute('aria-label', meta.label);
      input.addEventListener('change', function() {
        if (!meta.options && input.value === '') {
          delete state.overrides[key];
        } else {
          state.overrides[key] = Number(input.value);
        }
        rerender();
      });
      inputRow.appendChild(input);
      var reset = htmlElement('button', 'damage-reset-field', 'Reset field');
      reset.type = 'button';
      reset.disabled = !overridden;
      reset.addEventListener('click', function() {
        delete state.overrides[key];
        rerender();
      });
      inputRow.appendChild(reset);
      wrap.appendChild(inputRow);
      wrap.appendChild(htmlElement('div', 'damage-derived-origin', 'Calculated default: ' + formatNumber(baseValue)));
      if (meta.note) wrap.appendChild(htmlElement('p', 'damage-field-note', meta.note));
      return wrap;
    }

    function derivedSection(title, fields, note) {
      var section = htmlElement('section', 'damage-variable-section');
      section.appendChild(htmlElement('h3', '', title));
      if (note) section.appendChild(htmlElement('p', 'damage-section-note', note));
      var grid = htmlElement('div', 'damage-variable-grid');
      fields.forEach(function(field) { grid.appendChild(derivedField(field)); });
      section.appendChild(grid);
      return section;
    }

    function outputCard(label, value, detail, accent, tooltip) {
      var card = htmlElement('div', 'damage-output-card' + (accent ? ' is-primary' : ''));
      card.appendChild(labelWithTooltip('span', 'damage-output-label', label, tooltip));
      card.appendChild(htmlElement('strong', 'damage-output-value', value));
      if (detail) card.appendChild(htmlElement('span', 'damage-output-detail', detail));
      return card;
    }

    function variableGuideSection() {
      var guide = htmlElement('details', 'damage-formula-reference damage-variable-guide');
      guide.open = true;
      guide.appendChild(htmlElement('summary', '',
        'Variable guide — sources, formulas, and effects'));
      guide.appendChild(htmlElement('p', 'damage-section-note',
        'Every label also has a ? button with its field-specific source, rule, and effect. This open guide gives the complete map of the current mode; collapse it when you no longer need the reference.'));

      var entries = [
        {
          title: 'Class and level',
          text: 'Class supplies base/growth stats, movement, default gear, native actions, resistances, and the relevant B49–B52 coefficient. Level follows class-transition and growth-history fields to project the expected final stats.'
        },
        {
          title: 'Growth Gear',
          text: 'Growth Gear contributes item B20/B21 growth bonuses at every projected level-up. It models an equipment history assumption; it does not supply current battle stats.'
        },
        {
          title: 'Current Gear',
          text: 'Current Gear supplies present equipment stats, weapon or Spellbook identity, item-field-B12 Luck adjustment, elemental resistance, and B23/B24 anti-dragon flags. It does not change projected growth.'
        },
        {
          title: 'Character stats',
          text: state.mode === 'physical'
            ? 'Strength (STR) and Vitality (VIT) drive Physical attack/defense. Agility (AGI) and Dexterity (DEX) drive Physical accuracy/evasion, while DEX also enters Physical Attack. Luck (LCK) affects Physical damage, hit comparison, and criticals; equipment B12 affects Physical damage/hit Luck but not criticals. Defender current HP supplies the Normal and Critical resulting-Hit-Points outputs and does not change damage.'
            : 'Intelligence (INT) and Mentality (MEN) drive Magic attack/defense. Luck (LCK) contributes through effective Luck to the selected action-55 damage amount. Magic hit/success and critical/doubling outputs are unavailable. Defender current HP supplies one selected action-55 nonlethal resulting-Hit-Points output; the lethal result is withheld and current HP does not change damage.'
        },
        {
          title: 'Terrain and movement',
          text: 'Each side independently uses terrain table[movement type][terrain]. The same terrain does not automatically cancel: different movement types can return different adjustments, and equal adjustments scale separate Attack and Defense scores. Defender terrain also affects evasion; attacker terrain does not affect accuracy.'
        },
        {
          title: 'Spellbook selector and Alignment',
          text: 'Sky through Mountain movement uses the saved Generic Spellbook selector. Snow uses (Alignment - 50) / 10; Marsh uses (50 - Alignment) / 10; Immobile uses 0. Alignment defaults directly from each resolved selected class and is not projected by level or Growth Gear. ' +
            (state.mode === 'physical'
              ? 'The Alignment term feeds the Physical Attack or Defense adjustment; Attacker Alignment does not feed accuracy.'
              : 'The Alignment term feeds the Magic Attack or Defense adjustment.') +
            ' The selector is character data and is not inferred from equipped gear.'
        },
        {
          title: 'Action/template and formation',
          text: 'The selected native action supplies family D0 and element D1. D0 plus Front/Middle/Rear chooses the ±5 or 0 formation adjustment. D1 selects resistance and, in Magic, the weapon/action element comparison.'
        },
        {
          title: 'Squad leader and Fatigue',
          text: 'Leader compatibility is a built-in leader-class/member-group lookup worth 0, +10, or +15. Fatigue subtracts 0 below 70, 5 at 70–79, 18 at 80–89, and 40 at 90 or above.'
        },
        {
          title: 'Anti-dragon',
          text: 'Dragon-class means class IDs 0x38–0x44 or 0xA4. Attack +5 requires that dragon-class target and either Dragoon (class 0x14) or attack-flag gear: Fafnir, Sword of Firedrake, Sword of Tiamat, Balmung, Gram, Axe of Wyrm, or Cyanic Claw. Defense +5 requires that dragon-class attacker and either Dragoon or defense-flag gear: Dragon Shield, Dragon Armor, Dragon Helm, or Fang of Firedrake.'
        },
        {
          title: 'Resistance and effective Luck',
          text: 'Resistance is defender class resistance plus Current Gear resistance for the resolved element, limited to 0–100; damage uses (100 - resistance) / 100. Effective Luck is character LCK plus Current Gear B12, and the attacker value minus defender value is added after Defense is subtracted.'
        },
        {
          title: 'Total adjustments',
          text: 'Attack and Defense adjustments are percentage points, not flat damage. A total of +5 makes that score use a 1.05 multiplier; -5 uses 0.95. The formula reference below shows the current component arithmetic and any override replacement.'
        },
        {
          title: 'Fixed formula numbers and overrides',
          text: 'Fixed numbers are verified retail defaults used by the equations. Enable Override only to model a deliberate alternative. Field overrides replace one calculated variable; Reset field or Reset all calculated values restores the derived/default value.'
        }
      ];

      if (state.mode === 'physical') {
        entries.push(
          {
            title: 'Physical coefficients (B49 / B51)',
            text: 'Attacker class B49 starts the Physical Attack Score and recognized weapon STR is added to it. Defender class B51 starts the Physical Defense Score. The full multipliers and current arithmetic are shown below.'
          },
          {
            title: 'Physical hit, critical, and outputs',
            text: 'Accuracy uses attacker DEX/AGI, leader bonus, and Fatigue. Evasion uses defender AGI/DEX, terrain, leader bonus, and Fatigue. Hit comparison also uses effective Luck. Critical comparison uses character LCK only. Normal damage uses 0% random adjustment; the range uses the configured endpoints.'
          }
        );
      } else {
        entries.push(
          {
            title: 'Magic coefficients (B50 / B52)',
            text: 'Attacker class B50 plus equipment INT starts the Magic Attack Score. Defender class B52 starts the Magic Defense Score; MEN and INT multipliers then apply on both sides as shown in the formula reference.'
          },
          {
            title: 'Random damage adjustment',
            text: 'The selected integer changes the eligible primary-target amount within the configured variance endpoints. It is a modeled input; only the complete accepted fixture with +1 is labeled as the captured runtime anchor.'
          },
          {
            title: 'Magic product boundary',
            text: 'A number is shown only when a native T1 or Fixed Lightning template resolves to Action 55 Lightning in the accepted ordinary one-caster, selector-0, pattern-1, full-power primary-target slice. Other spells, satellites, combined casters, status/healing, hit/critical, and lethal handling remain unavailable.'
          }
        );
      }

      var grid = htmlElement('div', 'damage-variable-grid');
      entries.forEach(function(entry) {
        var card = htmlElement('div', 'damage-derived-field damage-guide-entry');
        card.appendChild(htmlElement('strong', 'damage-derived-label', entry.title));
        card.appendChild(htmlElement('p', 'damage-field-note', entry.text));
        grid.appendChild(card);
      });
      guide.appendChild(grid);
      return guide;
    }

    var shell = htmlElement('div', 'damage-calculator-shell');
    var heading = htmlElement('div', 'damage-calculator-heading');
    var headingText = htmlElement('div');
    headingText.appendChild(htmlElement('h2', '', 'Damage Calculator'));
    headingText.appendChild(htmlElement('p', '', 'Documented retail-game formulas with defaults loaded from the game data. Calculator changes are temporary and never modify the game or project.'));
    heading.appendChild(headingText);
    var resetAll = htmlElement('button', 'damage-reset-all', 'Reset all calculated values');
    resetAll.type = 'button';
    resetAll.addEventListener('click', function() {
      state.overrides = {};
      state.constantOverrides = {};
      rerender();
    });
    heading.appendChild(resetAll);
    shell.appendChild(heading);

    var modeTabs = htmlElement('div', 'damage-mode-tabs');
    ['physical', 'magic'].forEach(function(mode) {
      var modeOption = htmlElement('div', 'damage-mode-option');
      var button = htmlElement('button', state.mode === mode ? 'active' : '', mode === 'magic' ? 'Magic' : 'Physical');
      button.type = 'button';
      button.addEventListener('click', function() {
        state = transitionProductMode(rom, state, mode);
        panel._damageCalculatorState = state;
        rerender();
      });
      modeOption.appendChild(button);
      modeOption.appendChild(tooltipBadge(fieldExplanation(
        (mode === 'magic' ? 'Magic calculator mode' : 'Physical calculator mode'),
        mode === 'magic'
          ? 'Selects only class-native T1 (45) or Fixed Lightning (51) templates. Their resolved spell identity is derived separately and must be Lightning (55).'
          : 'Selects native standard physical attacks that use the class Physical Attack Coefficient against the defender\'s Physical Defense Coefficient (class fields B49 and B51).',
        mode === 'magic'
          ? 'Enables only the ordinary one-caster selector-0 / pattern-1 full-power primary-target amount slice. Native button behavior provides keyboard activation.'
          : 'Keeps the supported Physical actions, formula, variables, fixed numbers, and results available.',
        mode === 'magic'
          ? 'A Magic amount is available only when the native T1 or Fixed Lightning template resolves to Action 55 Lightning and every bounded product-policy condition passes.'
          : 'Physical mode uses class fields B49/B51 and the selected native standard Physical action; switching modes preserves shared combatant inputs but resolves a mode-valid action.'
      )));
      modeTabs.appendChild(modeOption);
    });
    shell.appendChild(modeTabs);

    if (state.mode === 'magic') {
      var evidencePanel = htmlElement('div', 'damage-evidence-panel');
      var acceptedEvidence = htmlElement('p', 'damage-evidence-row');
      acceptedEvidence.appendChild(htmlElement('strong', '', 'Accepted runtime anchor: '));
      acceptedEvidence.appendChild(document.createTextNode('resolved action 55 Lightning; one ordinary caster; selector 0 / pattern 1; full-power primary-target amount and nonlethal Hit Points behavior.'));
      evidencePanel.appendChild(acceptedEvidence);
      var staticEvidence = htmlElement('p', 'damage-evidence-row');
      staticEvidence.appendChild(htmlElement('strong', '', 'Supported static: '));
      staticEvidence.appendChild(document.createTextNode('native template selection, template-to-spell resolution, editable inputs and scores, and the configured random-adjustment endpoint range outside the exact accepted fixture.'));
      evidencePanel.appendChild(staticEvidence);
      var unavailableEvidence = htmlElement('p', 'damage-evidence-row');
      unavailableEvidence.appendChild(htmlElement('strong', '', 'Unavailable in this product slice: '));
      unavailableEvidence.appendChild(document.createTextNode('other spells/elements, satellites, combined casters, healing/status/special/lethal branches, hit/success, and critical/doubling outputs.'));
      evidencePanel.appendChild(unavailableEvidence);
      shell.appendChild(evidencePanel);
    } else {
      var scope = htmlElement('div', 'damage-scope-note');
      scope.appendChild(htmlElement('strong', '', 'Verified Physical scope: '));
      scope.appendChild(document.createTextNode('standard physical attacks that compare the attacker\'s Physical Attack Score with the defender\'s Physical Defense Score.'));
      shell.appendChild(scope);
    }
    shell.appendChild(variableGuideSection());

    var actionCard = htmlElement('section', 'damage-action-card');
    actionCard.appendChild(htmlElement('h3', '', state.mode === 'magic' ? 'Native template, resolved spell, and formation' : 'Native action and formation'));
    var supportedAction = ensureSupportedAction(rom, state);
    var actionControls = htmlElement('div', 'damage-control-grid');
    actionControls.appendChild(pickerButton(state.mode === 'magic' ? 'Native template' : 'Action', supportedAction
      ? (state.mode === 'magic'
          ? templateDisplayName(state.actionId) + ' (' + state.actionId + ')'
          : actionName(state.actionId) + ' (' + hexId(state.actionId) + ')')
      : '(no supported native action)', openActionPicker, !supportedAction, fieldExplanation(
        state.mode === 'magic' ? 'Attacker native template' : 'Attacker action',
        'Restricted to the selected attacker class\'s Front, Middle, or Rear entries (class fields B43, B45, and B47) whose bounded product route is documented.',
        state.mode === 'magic'
          ? 'Retains the class-native template ID. A separate context resolver must produce action 55 Lightning before any Magic amount can be shown.'
          : 'Supplies the action family (record field D0), action element (record field D1), valid formation rows, resistance element, and matching damage formula.',
        state.mode === 'magic'
          ? 'Eligible native templates are T1 (45) and Fixed Lightning (51); the resolved identity must be Action 55 Lightning, with ordinary one-caster selector 0, pattern 1, and full-power primary targeting.'
          : 'Formation adjustment = table[action family D0][selected Front, Middle, or Rear row], while element D1 selects the defender resistance used by damage.'
      )));
    var rowChoices = supportedAction ? supportedAction.rows.map(function(row, index) {
      return { value: row, label: ROW_NAMES[row] + ' row — ' + supportedAction.counts[index] + ' action' + (supportedAction.counts[index] === 1 ? '' : 's') };
    }) : [{ value: state.actionRow, label: ROW_NAMES[state.actionRow] || 'Unknown row' }];
    actionControls.appendChild(selectControl('Formation row', state.actionRow, rowChoices, function(value) {
      state.actionRow = Number(value);
      rerender();
    }, fieldExplanation(
      'Formation row',
      'Restricted to rows where the selected class natively has the selected action; action counts come from the class Front, Middle, and Rear count fields (B44, B46, and B48).',
      'Combines with the action family to produce the -5, 0, or +5 formation adjustment used by Magic Attack or Physical Attack.',
      'Families 0, 2, and 5 use Front/Middle/Rear +5/0/-5; families 1, 3, and 4 use -5/0/+5.'
    )));
    if (state.mode === 'magic') {
      actionControls.appendChild(numberControl('Random damage adjustment', state.magicRandomAdjustment,
        Math.min(result.constants.varianceMinimum, result.constants.varianceMaximum),
        Math.max(result.constants.varianceMinimum, result.constants.varianceMaximum), function(value) {
          state.magicRandomAdjustment = trunc(value);
          rerender();
        }, fieldExplanation(
          'Random damage adjustment',
          'User-selected integer constrained to the configured retail variance endpoints; defaults to 0. The accepted runtime fixture used +1.',
          'Feeds the displayed primary Magic amount. A selected value is not described as the captured runtime value unless the complete fixture matches.',
          'Damage before its final limit is multiplied by (100 + selected adjustment) / 100, then its decimal portion is discarded toward zero.'
        )));
    } else {
      actionControls.appendChild(selectControl('Hit/miss override', state.forcedSuccess, [
        { value: 'none', label: 'None (use calculated hit chance)' },
        { value: 'hit', label: 'Force hit' },
        { value: 'miss', label: 'Force miss' }
      ], function(value) { state.forcedSuccess = value; rerender(); }, fieldExplanation(
        'Hit/miss override',
        'User selection that keeps the calculated hit comparison number, forces it to 100, or forces it to 0.',
        'Changes only the displayed hit result; it does not change attack, defense, normal damage, or critical-hit chance.',
        'None keeps the calculated 0–100 comparison number; Force hit replaces it with 100; Force miss replaces it with 0.'
      )));
    }
    actionCard.appendChild(actionControls);
    if (state.mode === 'magic') {
      var nativeIdentity = htmlElement('p', 'damage-action-identity');
      nativeIdentity.appendChild(htmlElement('strong', '', 'Native template: '));
      nativeIdentity.appendChild(document.createTextNode(templateDisplayName(state.actionId) + ' (' + state.actionId + ')'));
      actionCard.appendChild(nativeIdentity);
      var resolvedIdentity = htmlElement('p', 'damage-action-identity');
      resolvedIdentity.appendChild(htmlElement('strong', '', 'Resolved spell: '));
      resolvedIdentity.appendChild(document.createTextNode(result.productPolicy.resolvedActionId
        ? result.productPolicy.resolvedSpellName + ' (' + result.productPolicy.resolvedActionId + ')'
        : 'Unresolved'));
      actionCard.appendChild(resolvedIdentity);
      var resolutionEvidence = htmlElement('p', 'damage-action-identity');
      resolutionEvidence.appendChild(htmlElement('strong', '', result.productPolicy.eligible ? 'Eligible bounded output: ' : 'Magic amount unavailable: '));
      resolutionEvidence.appendChild(document.createTextNode(result.productPolicy.eligible
        ? 'action 55; one caster; selector 0 / pattern 1; full-power primary target.'
        : result.productPolicy.reason));
      actionCard.appendChild(resolutionEvidence);
      if (result.productPolicy.resolutionSource) {
        actionCard.appendChild(htmlElement('p', 'damage-action-source', 'Supported-static resolution source: ' + result.productPolicy.resolutionSource + '.'));
      }
    }
    if (supportedAction && supportedAction.def) {
      actionCard.appendChild(htmlElement('p', 'damage-action-bytes',
        (state.mode === 'magic' ? 'Native template behavior bytes' : 'Raw action behavior bytes') + ' (record fields D0–D7): ' + Array.prototype.slice.call(supportedAction.def.rawBytes || [], 0, 8)
          .map(function(value) { return Number(value).toString(16).toUpperCase().padStart(2, '0'); }).join(' ')));
    }
    shell.appendChild(actionCard);

    var combatants = htmlElement('div', 'damage-combatant-grid');
    combatants.appendChild(combatantCard('attacker', 'Attacker'));
    combatants.appendChild(combatantCard('defender', 'Defender'));
    shell.appendChild(combatants);

    if (result.warnings.length) {
      var warningBox = htmlElement('div', 'damage-warning-box');
      warningBox.appendChild(htmlElement('strong', '', 'Static-evidence guardrails'));
      var list = document.createElement('ul');
      result.warnings.forEach(function(warning) { list.appendChild(htmlElement('li', '', warning)); });
      warningBox.appendChild(list);
      shell.appendChild(warningBox);
    }

    var outputs = htmlElement('section', 'damage-output-section');
    outputs.appendChild(htmlElement('h3', '', 'Results'));
    var outputGrid = htmlElement('div', 'damage-output-grid');
    if (state.mode === 'magic') {
      if (!result.productPolicy.eligible) {
        outputGrid.appendChild(outputCard('Magic amount unavailable', 'Unavailable',
          result.productPolicy.reason, true, fieldExplanation(
            'Magic amount unavailable',
            'The native-template resolver did not produce the exact eligible action-55 context.',
            'No internal Magic amount, range, or Hit Points number is rendered for an ineligible resolution.',
            'The product policy must resolve a native T1 or Fixed Lightning template to Action 55 Lightning and accept every bounded context field before any numeric Magic output is exposed.'
          )));
      } else {
        outputGrid.appendChild(outputCard('Damage at selected random adjustment',
          formatNumber(result.productOutputs.damageAtSelectedAdjustment),
          'Selected adjustment: ' + formatSignedPercent(result.productOutputs.selectedRandomAdjustment), true,
          fieldExplanation(
            'Damage at selected random adjustment',
            'The eligible action-55 primary-target amount after Magic Defense, effective Luck, Wind resistance, the selected random adjustment, decimal removal, and the configured damage limits.',
            'Represents one ordinary caster at selector 0 / pattern 1. The accepted fixture is reproduced only when all fixture inputs, including +1, match.',
            'Limit the whole-number part of (Magic Attack - Magic Defense + attacker effective Luck - defender effective Luck) × resistance multiplier × random-adjustment multiplier to the configured damage minimum and maximum.'
          )));
        outputGrid.appendChild(outputCard('Supported-static damage endpoint range',
          formatNumber(result.productOutputs.rangeLow) + '–' + formatNumber(result.productOutputs.rangeHigh),
          'Supported static; configured adjustment ' + formatSignedPercent(result.constants.varianceMinimum) + ' through ' + formatSignedPercent(result.constants.varianceMaximum), false,
          fieldExplanation(
            'Supported-static damage endpoint range',
            'The eligible action-55 amount evaluated at the configured lowest and highest random-adjustment endpoints.',
            'Shows static-supported endpoints, not a separately runtime-accepted range or satellite-target result.',
            'Run the same eligible primary-target amount equation once at the configured minimum random adjustment and once at the configured maximum.'
          )));
        outputGrid.appendChild(outputCard('Hit Points after selected damage',
          result.productOutputs.nonlethal ? formatNumber(result.productOutputs.hpAfterSelectedDamage) : 'Unavailable',
          result.productOutputs.nonlethal
            ? 'Starting Defender Hit Points: ' + formatNumber(result.values['defender.currentHp'])
            : 'Lethal branch handling is outside this product slice', false,
          fieldExplanation(
            'Hit Points after selected damage',
            'Defender current Hit Points minus the selected eligible action-55 amount when the result remains nonlethal.',
            'Shows one nonlethal primary-target Hit Points result. No lethal-branch number is claimed.',
            'When selected damage is nonlethal, remaining Hit Points = Defender current Hit Points - selected damage; otherwise this bounded product leaves the result unavailable.'
          )));
      }
    } else {
      outputGrid.appendChild(outputCard('Normal damage', formatNumber(result.outputs.normalDamage),
        'Random damage adjustment: 0%', true,
        fieldExplanation(
          'Normal damage',
          'The standard physical damage formula with its random damage adjustment set to 0%, then limited to the configured minimum and maximum damage.',
          'Subtracted from Defender current Hit Points for the normal result and multiplied when the critical-hit check passes.',
          'Limit the whole-number part of (Physical Attack - Physical Defense + attacker effective Luck - defender effective Luck) × resistance multiplier to the configured damage minimum and maximum.'
        )));
      outputGrid.appendChild(outputCard('Critical damage', formatNumber(result.outputs.criticalDamage),
        '×' + formatNumber(result.constants.criticalMultiplier) + ' after ordinary damage', false,
        fieldExplanation(
          'Critical damage',
          'Normal damage multiplied after the ordinary calculation by the configured critical multiplier. Damage is not limited a second time after multiplication.',
          'Subtracted from Defender current Hit Points when the Luck-based critical-hit check passes.',
          'Critical damage = already limited Normal damage × critical multiplier; do not apply the damage limit again.'
        )));
      outputGrid.appendChild(outputCard('Physical damage range',
        formatNumber(result.outputs.varianceLow) + '–' + formatNumber(result.outputs.varianceHigh),
        'Random adjustment: ' + formatSignedPercent(result.constants.varianceMinimum) + ' through ' + formatSignedPercent(result.constants.varianceMaximum), false,
        fieldExplanation(
          'Physical damage range',
          'The Physical damage formula evaluated with the configured lowest and highest random percentage adjustments, including decimal removal and the damage limits.',
          'Shows the possible ordinary-damage endpoints. The central Normal damage card uses a 0% random adjustment.',
          'Run the ordinary Physical damage equation once at the configured minimum random adjustment and once at the configured maximum, applying the final damage limits to each.'
        )));
      outputGrid.appendChild(outputCard('Success chance', result.outputs.successThreshold === null
        ? 'Unavailable' : formatNumber(result.outputs.successThreshold) + '%',
        state.forcedSuccess === 'none' ? 'Nominal chance from the calculated hit comparison number' : 'Hit/miss override: ' + state.forcedSuccess, false,
        fieldExplanation(
          'Success chance',
          'The 0-through-100 comparison number calculated from Attacker accuracy, Defender evasion, effective Luck, the unresolved special bonus, and the Hit/miss override.',
          'Determines whether the action applies its damage. It is labeled nominal because the exact distribution produced by the game\'s three combined random values remains unresolved.',
          'Limit whole-number part of (accuracy / evasion) × (100 + attacker effective Luck - defender effective Luck) + special hit bonus to 0–100, unless Force hit or Force miss replaces it.'
        )));
      outputGrid.appendChild(outputCard('Critical chance', (result.outputs.criticalChance * 100).toFixed(3).replace(/0+$/, '').replace(/\.$/, '') + '%',
        'Exact chance across the game\'s full repeating random-number sequence; comparison number ' + formatNumber(result.outputs.criticalThreshold), false,
        fieldExplanation(
          'Critical chance',
          'Exact probability, across the retail game\'s full repeating random-number sequence, that a 0-through-99 result is lower than the Luck-only critical-hit comparison number.',
          'When it passes, already calculated Normal damage is multiplied by the critical multiplier.',
          'Critical comparison number = whole-number part of ((attacker character Luck - defender character Luck) / 2) + 5; Current Gear Luck is excluded.'
        )));
      outputGrid.appendChild(outputCard('Resulting Hit Points',
        formatNumber(result.outputs.hpAfterNormal) + ' normal / ' + formatNumber(result.outputs.hpAfterCritical) + ' critical',
        'Starting Defender Hit Points: ' + formatNumber(result.values['defender.currentHp']), false,
        fieldExplanation(
          'Resulting Hit Points',
          'Defender current Hit Points minus Normal or Critical damage, with negative results replaced by zero.',
          'Shows the Defender Hit Points after a successful normal or critical action; a miss leaves Hit Points unchanged.',
          'Remaining Hit Points = maximum(Defender current Hit Points - applied damage, 0).'
        )));
    }
    outputs.appendChild(outputGrid);
    shell.appendChild(outputs);

    var finalStats = [];
    if (state.mode === 'magic') {
      finalStats.push(
        { key: 'attacker.int', label: 'Attacker Intelligence (INT)', min: 0, max: 999 },
        { key: 'attacker.men', label: 'Attacker Mentality (MEN)', min: 0, max: 999 },
        { key: 'attacker.lck', label: 'Attacker Luck (LCK)', min: 0, max: 100 },
        { key: 'attacker.alignment', label: 'Attacker Alignment', min: 0, max: 100 },
        { key: 'defender.hp', label: 'Defender maximum Hit Points (HP)', min: 0, max: 999 },
        { key: 'defender.int', label: 'Defender Intelligence (INT)', min: 0, max: 999 },
        { key: 'defender.men', label: 'Defender Mentality (MEN)', min: 0, max: 999 },
        { key: 'defender.lck', label: 'Defender Luck (LCK)', min: 0, max: 100 },
        { key: 'defender.alignment', label: 'Defender Alignment', min: 0, max: 100 }
      );
    } else {
      ['attacker', 'defender'].forEach(function(side) {
        var prefix = side === 'attacker' ? 'Attacker ' : 'Defender ';
        finalStats.push(
          { key: side + '.hp', label: prefix + 'maximum Hit Points (HP)', min: 0, max: 999 },
          { key: side + '.str', label: prefix + 'Strength (STR)', min: 0, max: 999 },
          { key: side + '.vit', label: prefix + 'Vitality (VIT)', min: 0, max: 999 },
          { key: side + '.int', label: prefix + 'Intelligence (INT)', min: 0, max: 999 },
          { key: side + '.men', label: prefix + 'Mentality (MEN)', min: 0, max: 999 },
          { key: side + '.agi', label: prefix + 'Agility (AGI)', min: 0, max: 999 },
          { key: side + '.dex', label: prefix + 'Dexterity (DEX)', min: 0, max: 999 },
          { key: side + '.lck', label: prefix + 'Luck (LCK)', min: 0, max: 100 },
          { key: side + '.alignment', label: prefix + 'Alignment', min: 0, max: 100 }
        );
      });
    }
    finalStats.push({
      key: 'defender.currentHp',
      label: 'Defender current Hit Points (HP)',
      min: 0,
      note: state.mode === 'magic'
        ? 'Defaults to projected maximum Hit Points and feeds one selected action-55 nonlethal resulting-Hit-Points output; the lethal result is withheld.'
        : 'Defaults to projected maximum Hit Points and feeds the Normal and Critical resulting-Hit-Points outputs.'
    });
    shell.appendChild(derivedSection('Expected final stats', finalStats,
      'Hit Points and STR, VIT, INT, MEN, AGI, DEX, and LCK are projected from the resolved level-1 class, class transition and growth-history fields (B53 through B56), the expected +1 from the two ordinary random growth rolls, and Growth Gear at every level-up. Alignment comes directly from each resolved selected class and is not projected by level or Growth Gear.'));

    var equipmentFields = state.mode === 'magic' ? [
      { key: 'attacker.equip.int', label: 'Attacker equipment Intelligence' },
      { key: 'attacker.equip.men', label: 'Attacker equipment Mentality' },
      { key: 'attacker.equip.b12', label: 'Attacker equipment Luck adjustment (item field B12)', note: 'Used by the selected Magic amount as part of effective Luck.' },
      { key: 'attacker.equip.weaponElement', label: 'Recognized weapon element', options: elementOptions(), note: 'Used for T1 resolution only when no Spellbook is equipped.' },
      { key: 'attacker.equip.bookVariant', label: 'Attacker Generic Spellbook element selector (character field +0x1A)', options: bookOptions(), note: 'Required when a Generic Spellbook is equipped; it cannot be inferred from equipment.' },
      { key: 'defender.equip.int', label: 'Defender equipment Intelligence' },
      { key: 'defender.equip.men', label: 'Defender equipment Mentality' },
      { key: 'defender.equip.b12', label: 'Defender equipment Luck adjustment (item field B12)' },
      { key: 'defender.equip.bookVariant', label: 'Defender Generic Spellbook element selector (character field +0x1A)', options: bookOptions(), note: 'Supplies the defender Spellbook-related context adjustment.' }
    ] : [
      { key: 'attacker.equip.str', label: 'Attacker equipment Strength' },
      { key: 'attacker.equip.int', label: 'Attacker equipment Intelligence' },
      { key: 'attacker.equip.agi', label: 'Attacker equipment Agility' },
      { key: 'attacker.equip.dex', label: 'Attacker equipment Dexterity' },
      { key: 'attacker.equip.vit', label: 'Attacker equipment Vitality' },
      { key: 'attacker.equip.men', label: 'Attacker equipment Mentality' },
      { key: 'attacker.equip.b12', label: 'Attacker equipment Luck adjustment (item field B12)', note: 'Used by physical damage and hit chance, but not the critical-hit check.' },
      { key: 'attacker.equip.weaponStr', label: 'Recognized weapon Strength' },
      { key: 'attacker.equip.nonweaponStr', label: 'Non-weapon equipment Strength' },
      { key: 'attacker.equip.weaponElement', label: 'Recognized weapon element', options: elementOptions() },
      { key: 'attacker.equip.bookVariant', label: 'Attacker Generic Spellbook element selector (character field +0x1A)', options: bookOptions() },
      { key: 'defender.equip.str', label: 'Defender equipment Strength' },
      { key: 'defender.equip.int', label: 'Defender equipment Intelligence' },
      { key: 'defender.equip.agi', label: 'Defender equipment Agility' },
      { key: 'defender.equip.dex', label: 'Defender equipment Dexterity' },
      { key: 'defender.equip.vit', label: 'Defender equipment Vitality' },
      { key: 'defender.equip.men', label: 'Defender equipment Mentality' },
      { key: 'defender.equip.b12', label: 'Defender equipment Luck adjustment (item field B12)' },
      { key: 'defender.equip.bookVariant', label: 'Defender Generic Spellbook element selector (character field +0x1A)', options: bookOptions(), note: 'This is the saved character value used by the battle adjustment, not the equipped Spellbook element.' }
    ];
    shell.appendChild(derivedSection('Current equipment values', equipmentFields));

    if (state.mode === 'magic' && !result.productPolicy.eligible) {
      panel.appendChild(shell);
      return;
    }

    var contextFields = [
      { key: 'action.family', label: 'Action family (record field D0)' },
      { key: 'action.element', label: 'Resolved action element (record field D1)', options: elementOptions() },
      { key: 'attacker.moveType', label: 'Attacker movement type', options: movementOptions() },
      { key: 'defender.moveType', label: 'Defender movement type', options: movementOptions() },
      { key: 'attacker.context.terrainMovement', label: 'Attacker terrain / movement adjustment' },
      { key: 'defender.context.terrainMovement', label: 'Defender terrain / movement adjustment' },
      { key: 'attacker.context.bookOrAlignment', label: 'Attacker Spellbook / Alignment adjustment' },
      { key: 'defender.context.bookOrAlignment', label: 'Defender Spellbook / Alignment adjustment' },
      { key: 'attacker.context.weaponSpellElement', label: 'Weapon / action element adjustment' },
      { key: 'attacker.context.formation', label: 'Formation adjustment' },
      { key: 'attacker.context.antiDragon', label: 'Anti-dragon attack adjustment' },
      { key: 'defender.context.antiDragon', label: 'Anti-dragon defense adjustment' },
      { key: 'attacker.context.leaderAffinity', label: 'Attacker leader compatibility bonus' },
      { key: 'defender.context.leaderAffinity', label: 'Defender leader compatibility bonus' },
      { key: 'attacker.context.fatiguePenalty', label: 'Attacker fatigue penalty' },
      { key: 'defender.context.fatiguePenalty', label: 'Defender fatigue penalty' },
      { key: 'defender.context.specialTerrainState', label: 'Unresolved special Defense adjustment', note: 'Default 0. One documented condition adds +3 to the shared Physical/Magic Defense context, but its player-facing meaning remains unresolved.' }
    ];
    if (state.mode === 'magic') {
      contextFields.push({ key: 'attacker.context.magicModifier', label: 'Total Magic Attack adjustment' });
      contextFields.push({ key: 'defender.context.magicDefenseModifier', label: 'Total Magic Defense adjustment' });
    }
    else {
      contextFields.push({ key: 'attacker.context.accuracyModifier', label: 'Attacker accuracy adjustment' });
      contextFields.push({ key: 'defender.context.evasionModifier', label: 'Defender evasion adjustment' });
      contextFields.push({ key: 'attacker.context.physicalModifier', label: 'Total Physical Attack adjustment' });
      contextFields.push({ key: 'defender.context.physicalDefenseModifier', label: 'Total Physical Defense adjustment' });
    }
    shell.appendChild(derivedSection('Action and battle adjustments', contextFields));

    var formulaFields;
    var formulaNote;
    if (state.mode === 'magic') {
      formulaFields = [
        { key: 'attacker.coefficient.magicAttack', label: 'Attacker Magic Attack Coefficient (class field B50)' },
        { key: 'score.magicAttack', label: 'Magic Attack Score' },
        { key: 'defender.coefficient.magicDefense', label: 'Defender Magic Defense Coefficient (class field B52)' },
        { key: 'score.magicDefense', label: 'Magic Defense Score' },
        { key: 'score.sourceEffectiveLuck', label: 'Attacker effective Luck' },
        { key: 'score.targetEffectiveLuck', label: 'Defender effective Luck' },
        { key: 'score.damageLuckDifference', label: 'Attacker effective Luck minus Defender effective Luck' },
        { key: 'score.targetResistance', label: 'Defender resistance to the action element', min: 0, max: 100 }
      ];
      formulaNote = 'Supported static outside the exact accepted fixture: this eligible action-55 primary-target amount subtracts the B52-derived Magic Defense Score, adds effective Luck, applies Wind resistance and the selected random adjustment, discards the decimal portion, and applies the configured damage bounds.';
    } else {
      formulaFields = [
        { key: 'attacker.coefficient.physicalAttack', label: 'Attacker Physical Attack Coefficient (class field B49)' },
        { key: 'defender.coefficient.physicalDefense', label: 'Defender Physical Defense Coefficient (class field B51)' },
        { key: 'score.physicalAttack', label: 'Physical Attack Score' },
        { key: 'score.physicalDefense', label: 'Physical Defense Score' },
        { key: 'score.sourceEffectiveLuck', label: 'Attacker effective Luck' },
        { key: 'score.targetEffectiveLuck', label: 'Defender effective Luck' },
        { key: 'score.damageLuckDifference', label: 'Attacker effective Luck minus Defender effective Luck' },
        { key: 'score.targetResistance', label: 'Defender resistance to the action element', min: 0, max: 100 }
      ];
      formulaNote = 'The central Normal damage result uses a 0% random damage adjustment. The displayed range uses the configured lowest and highest random adjustments, discards the decimal portion, and limits damage to the configured minimum and maximum.';
    }
    shell.appendChild(derivedSection('Damage calculation values', formulaFields, formulaNote));

    if (state.mode === 'physical') {
      var chanceFields = [
        { key: 'score.sourceAccuracy', label: 'Attacker accuracy score' },
        { key: 'score.targetEvasion', label: 'Defender evasion score' },
        { key: 'score.sourceEffectiveLuck', label: 'Attacker effective Luck (Luck plus equipment Luck adjustment)' },
        { key: 'score.targetEffectiveLuck', label: 'Defender effective Luck (Luck plus equipment Luck adjustment)' },
        { key: 'score.specialHitBonus', label: 'Unresolved special hit bonus', note: 'Default 0. One documented condition adds +10, but its player-facing meaning remains unresolved.' },
        { key: 'score.baseHitThreshold', label: 'Calculated hit comparison number', min: 0, max: 100 },
        { key: 'score.criticalThreshold', label: 'Critical-hit comparison number' }
      ];
      shell.appendChild(derivedSection('Hit and critical-hit calculations', chanceFields,
        'The hit comparison uses each character\'s Luck plus equipment Luck adjustment (item field B12). The critical-hit calculation uses only the characters\' Luck stats.'));
    }

    var constantsSection = htmlElement('details', 'damage-constants-section');
    var constantsSummary = htmlElement('summary', '', 'Fixed formula numbers');
    constantsSection.appendChild(constantsSummary);
    constantsSection.appendChild(htmlElement('p', 'damage-section-note',
      'These numbers are read-only until their Override checkbox is enabled. An enabled override feeds every result that uses that number.'));
    var constantsGrid = htmlElement('div', 'damage-constant-grid');
    CONSTANT_DEFS.filter(function(def) { return def.modes.indexOf(state.mode) !== -1; }).forEach(function(def) {
      var overridden = Object.prototype.hasOwnProperty.call(state.constantOverrides, def.key);
      var field = htmlElement('div', 'damage-constant-field' + (overridden ? ' is-overridden' : ''));
      field.appendChild(labelWithTooltip('span', 'damage-derived-label', def.label, constantTooltip(def)));
      var row = htmlElement('div', 'damage-constant-row');
      var checkLabel = htmlElement('label', 'damage-constant-check');
      var checkbox = document.createElement('input');
      checkbox.type = 'checkbox';
      checkbox.checked = overridden;
      checkbox.addEventListener('change', function() {
        if (checkbox.checked) state.constantOverrides[def.key] = result.constants[def.key];
        else delete state.constantOverrides[def.key];
        rerender();
      });
      checkLabel.appendChild(checkbox);
      checkLabel.appendChild(document.createTextNode(' Override'));
      row.appendChild(checkLabel);
      var input = document.createElement('input');
      input.type = 'number';
      input.step = 'any';
      input.value = result.constants[def.key];
      input.readOnly = !overridden;
      input.addEventListener('change', function() {
        if (!overridden) return;
        state.constantOverrides[def.key] = numeric(input.value, def.value);
        rerender();
      });
      row.appendChild(input);
      field.appendChild(row);
      field.appendChild(htmlElement('div', 'damage-derived-origin', 'Retail default: ' + def.value));
      constantsGrid.appendChild(field);
    });
    constantsSection.appendChild(constantsGrid);
    shell.appendChild(constantsSection);

    var formulas = htmlElement('details', 'damage-formula-reference');
    formulas.appendChild(htmlElement('summary', '', 'Formula reference'));
    formulas.appendChild(htmlElement('p', 'damage-section-note',
      'These equations use the current values and any enabled field or fixed-number overrides shown above. The game slightly rounds each intermediate decimal result before the next multiplication or division, and the calculator follows that exact order. “Whole-number part” means discard the decimal toward zero: 12.9 becomes 12 and -12.9 becomes -12. “Limit to A through B” means replace anything below A with A and anything above B with B.'));
    var pre = document.createElement('pre');
    var currentConstant = function(key) { return formatNumber(result.constants[key]); };
    var hasFormulaOverride = function(key) {
      return Object.prototype.hasOwnProperty.call(state.overrides, key);
    };
    var formulaValue = function(key) {
      return formatNumber(result.values[key]) + (hasFormulaOverride(key) ? '*' : '');
    };
    var componentDerivation = function(key, rule) {
      var derived = formatNumber(result.base[key]);
      if (!hasFormulaOverride(key)) return rule + ' = ' + derived;
      return rule + ' = ' + derived + '; current override = ' + formulaValue(key);
    };
    var adjustmentArithmetic = function(totalKey, terms) {
      var expression = formulaValue(terms[0].key);
      for (var termIndex = 1; termIndex < terms.length; termIndex++) {
        expression += ' ' + terms[termIndex].operator + ' ' + formulaValue(terms[termIndex].key);
      }
      var derivedSubtotal = formatNumber(result.base[totalKey]);
      var text = '  Current arithmetic: ' + expression + ' = ' + derivedSubtotal;
      if (hasFormulaOverride(totalKey)) {
        text += '\n  Current total: ' + formulaValue(totalKey) +
          ' (override replaces derived subtotal ' + derivedSubtotal + ')';
      }
      return text;
    };
    var movementLabel = function(side) {
      var movement = result.values[side + '.moveType'];
      return (MOVEMENT_NAMES[movement] || ('Unknown ' + formatNumber(movement))) + ' movement';
    };
    var terrainLabel = function(side) {
      var terrainId = state[side].terrainId;
      return 'terrain ' + formatNumber(terrainId) + ' — ' +
        (TERRAIN_NAMES[terrainId] || 'Unknown terrain');
    };
    var bookOrAlignmentRule = function(side) {
      var movement = result.values[side + '.moveType'];
      var alignment = result.values[side + '.alignment'];
      if (movement === 5) {
        return '(Alignment ' + formatNumber(alignment) + ' - 50) / 10';
      }
      if (movement === 6) {
        return '(50 - Alignment ' + formatNumber(alignment) + ') / 10';
      }
      if (movement >= 1 && movement <= 4) {
        var selector = result.values[side + '.equip.bookVariant'];
        return 'selector table[' + movementLabel(side) + '][' +
          (BOOK_NAMES[selector] || ('Unknown selector ' + formatNumber(selector))) + ']';
      }
      return movementLabel(side) + ' has no Spellbook/Alignment adjustment';
    };
    var leaderLabel = function(side) {
      return state[side].leaderClassId ? hexId(state[side].leaderClassId) : 'none';
    };
    var adjustmentReference =
      (state.mode === 'magic' ? 'MAGIC' : 'PHYSICAL') +
        ' ADJUSTMENT COMPONENT DERIVATIONS\n' +
      '  Terrain/movement lookup:\n' +
      '    Each side independently reads table[movement type][selected terrain].\n' +
      '    Matching terrain does not cancel: movement types can differ, and equal values scale separate scores.\n' +
      '    Attacker: ' + componentDerivation(
        'attacker.context.terrainMovement',
        'table[' + movementLabel('attacker') + '][' + terrainLabel('attacker') + ']'
      ) + '\n' +
      '    Defender: ' + componentDerivation(
        'defender.context.terrainMovement',
        'table[' + movementLabel('defender') + '][' + terrainLabel('defender') + ']'
      ) + '\n\n' +
      '  Spellbook/Alignment rule:\n' +
      '    Sky through Mountain use the movement-specific Generic Spellbook selector table.\n' +
      '    Snow uses (Alignment - 50) / 10; Marsh uses (50 - Alignment) / 10; Immobile uses 0.\n' +
      '    Attacker: ' + componentDerivation(
        'attacker.context.bookOrAlignment',
        bookOrAlignmentRule('attacker')
      ) + '\n' +
      '    Defender: ' + componentDerivation(
        'defender.context.bookOrAlignment',
        bookOrAlignmentRule('defender')
      ) +
      (state.mode === 'magic'
        ? '\n\n' +
          '  Weapon/action element rule:\n' +
          '    matching ordinary nonphysical elements = +2; opposed pairs = -2; otherwise 0.\n' +
          '    Opposed pairs are Wind/Earth, Flame/Water, and Virtue/Bane.\n' +
          '    ' + componentDerivation(
            'attacker.context.weaponSpellElement',
            'Current comparison: ' +
              (ELEMENT_NAMES[result.values['attacker.equip.weaponElement']] || 'Unknown') +
              ' weapon versus ' +
              (ELEMENT_NAMES[result.values['action.element']] || 'Unknown') +
              ' action'
          )
        : '') + '\n\n' +
      '  Formation lookup:\n' +
      '    Families 0, 2, and 5 use Front/Middle/Rear +5/0/-5; families 1, 3, and 4 use -5/0/+5.\n' +
      '    ' + componentDerivation(
        'attacker.context.formation',
        'table[action family ' + formatNumber(result.values['action.family']) + '][' +
          (ROW_NAMES[state.actionRow] || ('Unknown row ' + formatNumber(state.actionRow))) + ']'
      ) + '\n\n' +
      '  Anti-dragon attack rule:\n' +
      '    5 when the target is dragon-class and the attacker class/item condition is met; otherwise 0.\n' +
      '    Dragon classes are IDs 0x38–0x44 or 0xA4.\n' +
      '    Requires dragon target AND (Dragoon class 0x14 or Current Gear with item field B23 bit 0).\n' +
      '    Attack gear: Fafnir, Sword of Firedrake, Sword of Tiamat, Balmung, Gram, Axe of Wyrm, or Cyanic Claw.\n' +
      '    Current result: ' + componentDerivation(
        'attacker.context.antiDragon',
        'condition ? 5 : 0'
      ) + '\n' +
      '  Anti-dragon defense rule:\n' +
      '    5 when the attacker is dragon-class and the defender class/item condition is met; otherwise 0.\n' +
      '    Dragon classes are IDs 0x38–0x44 or 0xA4.\n' +
      '    Requires dragon attacker AND (Dragoon class 0x14 or Current Gear with item field B24 bit 0).\n' +
      '    Defense gear: Dragon Shield, Dragon Armor, Dragon Helm, or Fang of Firedrake.\n' +
      '    Current result: ' + componentDerivation(
        'defender.context.antiDragon',
        'condition ? 5 : 0'
      ) + '\n\n' +
      '  Leader compatibility lookup:\n' +
      '    The selected leader class and member monster group produce 0, +10, or +15.\n' +
      '    Attacker: ' + componentDerivation(
        'attacker.context.leaderAffinity',
        'table[leader ' + leaderLabel('attacker') + '][member class ' +
          hexId(state.attacker.classId) + ']'
      ) + '\n' +
      '    Defender: ' + componentDerivation(
        'defender.context.leaderAffinity',
        'table[leader ' + leaderLabel('defender') + '][member class ' +
          hexId(state.defender.classId) + ']'
      ) + '\n\n' +
      '  Fatigue penalty rule:\n' +
      '    0 below 70; 5 from 70–79; 18 from 80–89; 40 at 90 or above.\n' +
      '    Attacker fatigue ' + formatNumber(state.attacker.fatigue) + ': ' +
        componentDerivation('attacker.context.fatiguePenalty', 'penalty') + '\n' +
      '    Defender fatigue ' + formatNumber(state.defender.fatigue) + ': ' +
        componentDerivation('defender.context.fatiguePenalty', 'penalty') + '\n\n' +
      '  Unresolved special Defense rule:\n' +
      '    defaults to 0; use the explicit override only for the documented +3 condition.\n' +
      '    Current result: ' + componentDerivation(
        'defender.context.specialTerrainState',
        'default'
      ) +
      (Object.keys(state.overrides).some(function(key) {
        return key.indexOf('.context.') !== -1 || key === 'action.family';
      }) ? '\n\n  * Active override; the current arithmetic uses the overridden value.' : '');
    pre.textContent = state.mode === 'magic'
      ? 'TOTAL MAGIC ATTACK ADJUSTMENT\n' +
        '  Formula: terrain/movement + Spellbook/Alignment + weapon/action element\n' +
        '    + formation + anti-dragon attack + leader compatibility - fatigue penalty\n' +
        adjustmentArithmetic('attacker.context.magicModifier', [
          { key: 'attacker.context.terrainMovement', operator: '+' },
          { key: 'attacker.context.bookOrAlignment', operator: '+' },
          { key: 'attacker.context.weaponSpellElement', operator: '+' },
          { key: 'attacker.context.formation', operator: '+' },
          { key: 'attacker.context.antiDragon', operator: '+' },
          { key: 'attacker.context.leaderAffinity', operator: '+' },
          { key: 'attacker.context.fatiguePenalty', operator: '-' }
        ]) + '\n\n' +
        'TOTAL MAGIC DEFENSE ADJUSTMENT\n' +
        '  Formula: terrain/movement + Spellbook/Alignment + anti-dragon defense\n' +
        '    + leader compatibility + unresolved special Defense - fatigue penalty\n' +
        adjustmentArithmetic('defender.context.magicDefenseModifier', [
          { key: 'defender.context.terrainMovement', operator: '+' },
          { key: 'defender.context.bookOrAlignment', operator: '+' },
          { key: 'defender.context.antiDragon', operator: '+' },
          { key: 'defender.context.leaderAffinity', operator: '+' },
          { key: 'defender.context.specialTerrainState', operator: '+' },
          { key: 'defender.context.fatiguePenalty', operator: '-' }
        ]) + '\n\n' +
        adjustmentReference + '\n\n' +
        'MAGIC ATTACK SCORE\n' +
        '  = whole-number part of:\n' +
        '    ((Magic Attack Coefficient + equipment Intelligence) / ' + currentConstant('magicCoefficientDivisor') + ')\n' +
        '    x ((Attacker Intelligence + ' + currentConstant('magicIntOffset') + ') / ' + currentConstant('magicIntDivisor') + ')\n' +
        '    x ((Attacker Mentality + equipment Mentality + ' + currentConstant('magicMenOffset') + ') / ' + currentConstant('magicMenDivisor') + ')\n' +
        '    x ((' + currentConstant('modifierBase') + ' + Total Magic Attack adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
        'MAGIC DEFENSE SCORE\n' +
        '  = whole-number part of:\n' +
        '    (Magic Defense Coefficient / ' + currentConstant('magicDefenseCoefficientDivisor') + ')\n' +
        '    x ((Defender Mentality + equipment Mentality + ' + currentConstant('magicDefenseMenOffset') + ') / ' + currentConstant('magicDefenseMenDivisor') + ')\n' +
        '    x ((Defender Intelligence + equipment Intelligence + ' + currentConstant('magicDefenseIntOffset') + ') / ' + currentConstant('magicDefenseIntDivisor') + ')\n' +
        '    x ((' + currentConstant('modifierBase') + ' + Total Magic Defense adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
        'ATTACKER EFFECTIVE LUCK\n' +
        '  = Attacker Luck + Attacker equipment Luck adjustment\n\n' +
        'DEFENDER EFFECTIVE LUCK\n' +
        '  = Defender Luck + Defender equipment Luck adjustment\n\n' +
        'DEFENDER WIND RESISTANCE\n' +
        '  = Defender class resistance + equipment resistance\n' +
        '  Then limit the result to ' + currentConstant('magicResistanceMinimum') + ' through ' + currentConstant('magicResistanceMaximum') + '.\n\n' +
        'ACTION-55 DAMAGE BEFORE ITS FINAL LIMIT\n' +
        '  = whole-number part of:\n' +
        '    (Magic Attack Score - Magic Defense Score\n' +
        '      + Attacker effective Luck - Defender effective Luck)\n' +
        '    x ((' + currentConstant('percentDivisor') + ' - Defender Wind resistance) / ' + currentConstant('percentDivisor') + ')\n' +
        '    x ((' + currentConstant('percentDivisor') + ' + Random damage adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
        'DAMAGE AT SELECTED RANDOM ADJUSTMENT\n' +
        '  = limit that result to ' + currentConstant('damageMinimum') + ' through ' + currentConstant('damageMaximum') + '.\n' +
        '  Selected random adjustment: ' + formatSignedPercent(result.productOutputs.selectedRandomAdjustment) + '.\n' +
        '  Configured Supported-static endpoints: ' + currentConstant('varianceMinimum') + '% through ' + currentConstant('varianceMaximum') + '%.\n' +
        '  The accepted runtime fixture used +1; other selected inputs are not relabeled as captured runtime.'
      : 'TOTAL PHYSICAL ATTACK ADJUSTMENT\n' +
        '  Formula: terrain/movement + Spellbook/Alignment + formation\n' +
        '    + anti-dragon attack + leader compatibility - fatigue penalty\n' +
        adjustmentArithmetic('attacker.context.physicalModifier', [
          { key: 'attacker.context.terrainMovement', operator: '+' },
          { key: 'attacker.context.bookOrAlignment', operator: '+' },
          { key: 'attacker.context.formation', operator: '+' },
          { key: 'attacker.context.antiDragon', operator: '+' },
          { key: 'attacker.context.leaderAffinity', operator: '+' },
          { key: 'attacker.context.fatiguePenalty', operator: '-' }
        ]) + '\n\n' +
        'TOTAL PHYSICAL DEFENSE ADJUSTMENT\n' +
        '  Formula: terrain/movement + Spellbook/Alignment + anti-dragon defense\n' +
        '    + leader compatibility + unresolved special Defense - fatigue penalty\n' +
        adjustmentArithmetic('defender.context.physicalDefenseModifier', [
          { key: 'defender.context.terrainMovement', operator: '+' },
          { key: 'defender.context.bookOrAlignment', operator: '+' },
          { key: 'defender.context.antiDragon', operator: '+' },
          { key: 'defender.context.leaderAffinity', operator: '+' },
          { key: 'defender.context.specialTerrainState', operator: '+' },
          { key: 'defender.context.fatiguePenalty', operator: '-' }
        ]) + '\n\n' +
        adjustmentReference + '\n\n' +
        'PHYSICAL ATTACK SCORE\n' +
        '  = whole-number part of:\n' +
        '    ((Physical Attack Coefficient + recognized weapon Strength) / ' + currentConstant('physicalCoefficientDivisor') + ')\n' +
        '    x ((Attacker Strength + non-weapon equipment Strength + ' + currentConstant('physicalStrOffset') + ') / ' + currentConstant('physicalStrDivisor') + ')\n' +
        '    x ((Attacker Dexterity + equipment Dexterity + ' + currentConstant('physicalDexOffset') + ') / ' + currentConstant('physicalDexDivisor') + ')\n' +
        '    x ((' + currentConstant('modifierBase') + ' + Total Physical Attack adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
        'PHYSICAL DEFENSE SCORE\n' +
        '  = whole-number part of:\n' +
        '    (Physical Defense Coefficient / ' + currentConstant('defenseCoefficientDivisor') + ')\n' +
        '    x ((Defender Vitality + equipment Vitality + ' + currentConstant('defenseVitOffset') + ') / ' + currentConstant('defenseVitDivisor') + ')\n' +
        '    x ((Defender Strength + equipment Strength + ' + currentConstant('defenseStrOffset') + ') / ' + currentConstant('defenseStrDivisor') + ')\n' +
        '    x ((' + currentConstant('modifierBase') + ' + Total Physical Defense adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
        'ATTACKER EFFECTIVE LUCK\n' +
        '  = Attacker Luck + Attacker equipment Luck adjustment\n\n' +
        'DEFENDER EFFECTIVE LUCK\n' +
        '  = Defender Luck + Defender equipment Luck adjustment\n\n' +
        'DEFENDER ELEMENTAL RESISTANCE\n' +
        '  = Defender class resistance + equipment resistance\n' +
        '  Then limit the result to ' + currentConstant('chanceMinimum') + ' through ' + currentConstant('chanceMaximum') + '.\n\n' +
        'PHYSICAL DAMAGE BEFORE ITS FINAL LIMIT\n' +
        '  = whole-number part of:\n' +
        '    (Physical Attack Score - Physical Defense Score\n' +
        '      + Attacker effective Luck - Defender effective Luck)\n' +
        '    x ((' + currentConstant('percentDivisor') + ' - Defender elemental resistance) / ' + currentConstant('percentDivisor') + ')\n' +
        '    x ((' + currentConstant('percentDivisor') + ' + Random damage adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
        'NORMAL PHYSICAL DAMAGE\n' +
        '  = limit that result to ' + currentConstant('damageMinimum') + ' through ' + currentConstant('damageMaximum') + '.\n' +
        '  The game supplies a random adjustment from ' + currentConstant('varianceMinimum') + '% through ' + currentConstant('varianceMaximum') + '%.\n' +
        '  The central Normal damage card uses 0%; the range card uses both endpoints.\n\n' +
        'CRITICAL DAMAGE\n' +
        '  = Normal physical damage x ' + currentConstant('criticalMultiplier') + '\n' +
        '  No second damage limit is applied after multiplication.';
    formulas.appendChild(pre);
    var shared = document.createElement('pre');
    shared.textContent = state.mode === 'magic'
      ? 'NONLETHAL HIT POINTS AFTER SELECTED DAMAGE\n' +
        '  = Defender current Hit Points - Damage at selected random adjustment\n' +
        '  This product slice displays the result only when it remains above 0.\n' +
        '  Lethal branch handling is unavailable.'
      : 'RESULTING HIT POINTS\n' +
      '  = Defender current Hit Points - the damage that was applied\n' +
      '  If that result is below 0, use 0. A miss leaves Hit Points unchanged.\n\n' +
      'ATTACKER ACCURACY ADJUSTMENT\n' +
      '  = Attacker leader compatibility bonus - Attacker fatigue penalty\n\n' +
      'ATTACKER ACCURACY SCORE\n' +
      '  = whole-number part of:\n' +
      '    (Attacker Dexterity + equipment Dexterity + ' + currentConstant('accuracyDexOffset') + ')\n' +
      '    x ((Attacker Agility + equipment Agility + ' + currentConstant('accuracyAgiOffset') + ') / ' + currentConstant('accuracyAgiDivisor') + ')\n' +
      '    x ((' + currentConstant('modifierBase') + ' + Attacker accuracy adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
      'DEFENDER EVASION ADJUSTMENT\n' +
      '  = Defender terrain and movement adjustment\n' +
      '  + Defender leader compatibility bonus\n' +
      '  - Defender fatigue penalty\n\n' +
      'DEFENDER EVASION SCORE\n' +
      '  = whole-number part of:\n' +
      '    (Defender Agility + equipment Agility + ' + currentConstant('evasionAgiOffset') + ')\n' +
      '    x ((Defender Dexterity + equipment Dexterity + ' + currentConstant('evasionDexOffset') + ') / ' + currentConstant('evasionDexDivisor') + ')\n' +
      '    x ((' + currentConstant('modifierBase') + ' + Defender evasion adjustment) / ' + currentConstant('percentDivisor') + ')\n\n' +
      'CALCULATED HIT COMPARISON NUMBER\n' +
      '  1. Divide Attacker accuracy score by Defender evasion score.\n' +
      '  2. Multiply by (' + currentConstant('hitLuckBase') + ' + Attacker effective Luck - Defender effective Luck).\n' +
      '  3. Discard the decimal portion.\n' +
      '  4. Add the Unresolved special hit bonus.\n' +
      '  5. Limit the result to ' + currentConstant('chanceMinimum') + ' through ' + currentConstant('chanceMaximum') + '.\n\n' +
      'The game compares a 0-through-99 value made from three random draws with\n' +
      'this comparison number. The action succeeds when the random value is lower.\n' +
      'The exact distribution of that combined value remains unresolved, so the\n' +
      'calculator labels the comparison number as a nominal success chance. Some battle\n' +
      'states can force a hit; the Hit/miss override models a forced hit or miss.\n\n' +
      'CRITICAL-HIT COMPARISON NUMBER\n' +
      '  = whole-number part of ((Attacker Luck - Defender Luck) / ' + currentConstant('criticalLuckDivisor') + ')\n' +
      '  + ' + currentConstant('criticalBase') + '\n\n' +
      'Equipment Luck adjustments are not used for this check. One random value\n' +
      'from 0 through 99 is compared with this number; a lower value doubles\n' +
      'the already calculated damage. Across the random generator\'s full cycle:\n' +
      '  comparison number 0 or lower: 0%\n' +
      '  comparison number 1 through 68: ((328 x comparison number) / 32768) x 100%\n' +
      '  comparison number 69 through 99: (((327 x comparison number) + 68) / 32768) x 100%\n' +
      '  comparison number 100 or higher: 100%';
    formulas.appendChild(shared);
    shell.appendChild(formulas);

    panel.appendChild(shell);
  }

  return {
    ROW_NAMES: ROW_NAMES,
    TERRAIN_NAMES: TERRAIN_NAMES,
    TERRAIN_MOVEMENT: TERRAIN_MOVEMENT,
    CONSTANT_DEFS: CONSTANT_DEFS,
    CONSTANT_DEFAULTS: CONSTANT_DEFAULTS,
    fieldExplanation: fieldExplanation,
    derivedFieldTooltip: buildDerivedFieldTooltip,
    constantTooltip: constantTooltip,
    trunc: trunc,
    clamp: clamp,
    actionRoute: actionRoute,
    nativeActionsForClass: nativeActionsForClass,
    nativeProductActionsForClass: nativeProductActionsForClass,
    resolveAction55ProductPolicy: resolveAction55ProductPolicy,
    resolveGrowthClass: resolveGrowthClass,
    projectExpectedStats: projectExpectedStats,
    equipmentTotals: equipmentTotals,
    terrainMovement: terrainMovement,
    bookOrAlignment: bookOrAlignment,
    elementAdjustment: elementAdjustment,
    primaryActionElement: primaryActionElement,
    resolveVariableActionElement: resolveVariableActionElement,
    formationAdjustment: formationAdjustment,
    leaderMonsterAffinity: leaderMonsterAffinity,
    fatiguePenalty: fatiguePenalty,
    exactCriticalProbability: exactCriticalProbability,
    physicalDamage: physicalDamage,
    makeDefaultState: makeDefaultState,
    normalizeProductState: normalizeProductState,
    transitionProductMode: transitionProductMode,
    derive: derive,
    deriveProduct: deriveProduct,
    render: render
  };
});
