// OB64 Mod Editor — Name Lookup Data
// Ported from scripts/ob64_class_ids.js, ob64_item_ids.js, ob64_spell_ids.js
// Uses window.OB64 namespace (no ES modules — avoids file:// CORS issues)

window.OB64 = window.OB64 || {};

// ============================================================
// CLASS NAMES (164 entries, 0x01-0xA4)
//
// Authoritative source: docs/gameshark-reference.md (Cralex's community
// Gameshark Class Hacking Guide). The previous 103-entry table was built
// by reverse-reading the in-ROM display-name table at 0x65DF0 and missed
// several non-displayable / variant slots (Centurion Female 0x23, Vampire
// in coffin 0x2C, Zombie Female 0x2E, every story-character duplicate in
// 0x5A-0xA4), producing a progressive positive shift from 0x27 upward
// (Werewolf at 0x29 instead of 0x2A, Wyrm at 0x42 instead of 0x45, etc.).
//
// Rebuilt 2026-04-15; verified via in-game anchors Hicks 0x27 Hawkman,
// Hariara 0x4E Golem, Leia 0x56 Blaze Knight.
// ============================================================
OB64.CLASS_NAMES = {
  0x00: "None",

  // Basic human classes (0x01-0x12)
  0x01: "Soldier",
  0x02: "Fighter",
  0x03: "Lycanthrope",              // Messenger's class (NOT Biske — Biske is 0x5F/0x60)
  0x04: "Amazon",
  0x05: "Knight",
  0x06: "Berserker",
  0x07: "Fencer",
  0x08: "Phalanx",
  0x09: "Beast Tamer",
  0x0A: "Doll Master",
  0x0B: "Ninja",
  0x0C: "Wizard",
  0x0D: "Archer",
  0x0E: "Dragon Tamer",
  0x0F: "Valkyrie",
  0x10: "Witch",
  0x11: "Sorceress",
  0x12: "Cleric",

  // Advanced human (0x13-0x1F)
  0x13: "Paladin",
  0x14: "Dragoon",
  0x15: "Black Knight",
  0x16: "Sword Master",
  0x17: "Cataphract",
  0x18: "Beast Master",
  0x19: "Enchanter",
  0x1A: "Ninja Master",
  0x1B: "Archmage",
  0x1C: "Diana",
  0x1D: "Dragon Master",
  0x1E: "Freya",
  0x1F: "Siren",

  // Priest/Angel/Lich (0x20-0x26)
  0x20: "Priest",
  0x21: "Princess",
  0x22: "Centurion (Male)",
  0x23: "Centurion (Female)",
  0x24: "Angel Knight",
  0x25: "Seraph",
  0x26: "Lich",

  // Demi-humans / undead (0x27-0x30)
  0x27: "Hawkman",
  0x28: "Vultan",
  0x29: "Raven",
  0x2A: "Werewolf",                 // GENERIC (NOT Biske)
  0x2B: "Vampire",
  0x2C: "Vampire (in coffin)",
  0x2D: "Zombie (Male)",
  0x2E: "Zombie (Female)",
  0x2F: "Skeleton",
  0x30: "Ghost",

  // Monsters (0x31-0x37)
  0x31: "Gorgon",
  0x32: "Pumpkinhead",
  0x33: "Faerie",
  0x34: "Gremlin",
  0x35: "Goblin",
  0x36: "Saturos",
  0x37: "Ogre",

  // Dragons (0x38-0x41)
  0x38: "Young Dragon",
  0x39: "Thunder Dragon",
  0x3A: "Red Dragon",
  0x3B: "Earth Dragon",
  0x3C: "Blue Dragon",
  0x3D: "Platinum Dragon",
  0x3E: "Black Dragon",
  0x3F: "Quetzalcoatl",
  0x40: "Flarebrass",
  0x41: "Ahzi Dahaka",

  // Hydra/Bahamut/etc (0x42-0x4A)
  0x42: "Hydra",
  0x43: "Bahamut",
  0x44: "Tiamat",
  0x45: "Wyrm",
  0x46: "Wyvern",
  0x47: "Griffin",
  0x48: "Opinincus",
  0x49: "Cockatrice",
  0x4A: "Sphinx",

  // Beasts/Golems (0x4B-0x50)
  0x4B: "Hellhound",
  0x4C: "Cerberus",
  0x4D: "Giant",
  0x4E: "Golem",
  0x4F: "Stone Golem",
  0x50: "Baldr Golem",

  // Magnus / Hero classes (0x51-0x53)
  0x51: "Gladiator",                // Magnus/Hero's First
  0x52: "Vanguard",                 // Magnus/Hero's Second
  0x53: "General",                  // Magnus/Hero's Third

  // Named-character classes (0x54-0x66)
  0x54: "Gladiator (Dio)",
  0x55: "Warrior (Dio)",
  0x56: "Blaze Knight",             // Leia's First
  0x57: "Rune Knight",              // Leia's Second
  0x58: "Lord (Destin)",
  0x59: "General (Debonair)",
  0x5A: "Beast Master (Gilbert)",
  0x5B: "Priest (Aisha)",
  0x5C: "Warlock (Saradin)",
  0x5D: "Grappler (Vad)",
  0x5E: "Centurion (Europea)",
  0x5F: "Lycanthrope (Biske day)",
  0x60: "Werewolf (Biske night)",
  0x61: "Solidblade (Ankiseth)",
  0x62: "Overlord (Yumil)",
  0x63: "Dark Prince (Amrius)",
  0x64: "Special Class (Prognar)",
  0x65: "Flail Monarch (Procus)",
  0x66: "Death Templar (Richard)",

  // Temple Commanders (0x67-0x6B)
  0x67: "Temple Cmdr (Baldwin)",
  0x68: "Temple Cmdr (Thamus)",
  0x69: "Temple Cmdr (Pruflas)",
  0x6A: "Temple Cmdr (Amazeroth)",
  0x6B: "Temple Cmdr (Vapula)",

  // Vanity / Superior Knight / misc (0x6C-0x74)
  0x6C: "Vanity (Godeslas)",
  0x6D: "Vanity (Kerikov)",
  0x6E: "Vanity (Count Silvis)",
  0x6F: "Superior Knight (Xevec)",
  0x70: "Superior Knight (Rhade)",
  0x71: "Gatekeeper (Danika)",
  0x72: "Grappler",                 // GENERIC (NOT Vad)
  0x73: "Knight Templar",
  0x74: "Daemon",

  // More named-character classes (0x75-0x7C)
  0x75: "Phalanx (Troi)",
  0x76: "Berserker (Asnabel)",
  0x77: "Cleric (Katreda)",
  0x78: "Archer (Liedel)",
  0x79: "Hawkman (Sheen)",
  0x7A: "Siren (Meredia)",
  0x7B: "Enchanter (Paul)",
  0x7C: "Black Knight (Carth)",

  // Deploy-crash story classes (0x7D-0x88)
  0x7D: "Special Class (Hugo)",
  0x7E: "Special Class (Frederick)",
  0x7F: "Special Class (Odiron)",
  0x80: "Special Class (Mari)",
  0x81: "Special Class (Zeda)",
  0x82: "Barkeep",
  0x83: "Elderly Man",
  0x84: "Commoner (Male)",
  0x85: "Commoner (Female)",
  0x86: "Danika (Normal)",
  0x87: "Danika",
  0x88: "Danika tendril",

  // Buggy leader classes (0x89-0xA4)
  0x89: "Archer (buggy)",
  0x8A: "Berserker (buggy)",
  0x8B: "Beast Tamer (buggy)",
  0x8C: "Valkyrie (buggy)",
  0x8D: "Wizard (buggy)",
  0x8E: "Phalanx (buggy)",
  0x8F: "Berserker (buggy 2)",
  0x90: "Knight (buggy)",
  0x91: "Ninja Master (buggy)",
  0x92: "Doll Master (buggy)",
  0x93: "Knight Templar (buggy)",
  0x94: "Archmage (buggy)",
  0x95: "Priest (buggy)",
  0x96: "Black Knight (buggy)",
  0x97: "Dragon Master (buggy)",
  0x98: "Siren (buggy)",
  0x99: "Saturos (buggy)",
  0x9A: "Sword Master (buggy)",
  0x9B: "Knight Templar (buggy 2)",
  0x9C: "Dragoon (buggy)",
  0x9D: "Gorgon (buggy)",
  0x9E: "Lich (buggy)",
  0x9F: "Daemon (buggy)",
  0xA0: "Plaladin (sic)",
  0xA1: "Danika tendril (alt)",
  0xA2: "Paladin (buggy)",
  0xA3: "Witch (Deneb)",
  0xA4: "Death Bahamut (Grozz Nuy)",
};

// Categories — updated to match the 164-entry ID mapping.
// Buggy/story-duplicate classes in 0x5A-0xA4 aren't assigned a category
// (their names in CLASS_NAMES already flag them).
OB64.CLASS_CATEGORIES = {
  "Human Male (Basic)":     [0x01, 0x02, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D],
  "Human Female (Basic)":   [0x04, 0x0E, 0x0F, 0x10, 0x11, 0x12],
  "Human Male (Advanced)":  [0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x22],
  "Human Female (Advanced)":[0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x23],
  "Angel":                  [0x24, 0x25],
  "Undead":                 [0x26, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30],
  "Demi-Human":             [0x03, 0x27, 0x28, 0x29, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37],
  "Dragon":                 [0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41],
  "Hydra/Avian":            [0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A],
  "Beast":                  [0x4B, 0x4C],
  "Giant/Golem":            [0x4D, 0x4E, 0x4F, 0x50],
  "Hero (Magnus)":          [0x51, 0x52, 0x53],
  "Hero (Story Chars)":     [0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63],
  "Boss":                   [0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x73, 0x74, 0xA3, 0xA4],
  "Generic-Dup":            [0x72, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C],
  "NPC":                    [0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88],
};

OB64.className = function(id) {
  return OB64.CLASS_NAMES[id] || ("Unknown_0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// ITEM NAMES (277 entries, 1-based IDs, max 0x115)
// ROM 0x613B0-0x62310, ID 0x00 = no item
// ============================================================
OB64.ITEM_NAMES = {
  0x01: "Short Sword",
  0x02: "Baldr Sword",
  0x03: "Falchion",
  0x04: "Flamberge",
  0x05: "Fafnir",
  0x06: "Sum Mannus",
  0x07: "Notos",
  0x08: "Sword of Firedrake",
  0x09: "Laevateinn",
  0x0A: "Glamdring",
  0x0B: "Stone Sword",
  0x0C: "Adamant Katana",
  0x0D: "Ice Blade",
  0x0E: "Nephrite Sword",
  0x0F: "Blessed Sword",
  0x10: "Penitence",
  0x11: "Oracion",
  0x12: "Evil Blade",
  0x13: "Dainslaif",
  0x14: "Noish's Promise",
  0x15: "Knoevlfer",
  0x16: "Bastard Sword",
  0x17: "Sword of Tiamat",
  0x18: "Claymore",
  0x19: "Balmung",
  0x1A: "Glaive of Champion",
  0x1B: "Sigmund",
  0x1C: "Matsukaze",
  0x1D: "Iscandelvey",
  0x1E: "Kagari-bi",
  0x1F: "Gram",
  0x20: "Yomogi-u",
  0x21: "Malachite Sword",
  0x22: "Yu-giri",
  0x23: "Chaladholg",
  0x24: "Kusanagi",
  0x25: "Durandel",
  0x26: "Ogre Blade",
  0x27: "Sonic Blade",
  0x28: "Rapier",
  0x29: "Main Gauche",
  0x2A: "Sword of Dragon Gem",
  0x2B: "Estoc",
  0x2C: "Peridot Sword",
  0x2D: "Needle of Light",
  0x2E: "Anbicion",
  0x2F: "Clau Solace",
  0x30: "Francisca",
  0x31: "Halt Hammer",
  0x32: "Baldr Club",
  0x33: "Baldr Axe",
  0x34: "Euros",
  0x35: "Gramlock",
  0x36: "Flame Flail",
  0x37: "Axe of Wyrm",
  0x38: "Aqua Hammer",
  0x39: "Frozen Axe",
  0x3A: "Celestial Hammer",
  0x3B: "Evil Axe",
  0x3C: "Bloody Cleaver",
  0x3D: "Warhammer",
  0x3E: "Paua Hammer",
  0x3F: "Heavy Axe",
  0x40: "Mjollnir",
  0x41: "Boreas",
  0x42: "Prox",
  0x43: "Sanscion",
  0x44: "Yggdrasil",
  0x45: "Urdarbrunn",
  0x46: "Rune Axe",
  0x47: "Satan's Bullova",
  0x48: "Dagda's Hammer",
  0x49: "Short Spear",
  0x4A: "Spear",
  0x4B: "Baldr Spear",
  0x4C: "Culnrikolnne",
  0x4D: "Thunder Spear",
  0x4E: "Zephyros",
  0x4F: "Volcaetus",
  0x50: "Ignis",
  0x51: "Earth Javelin",
  0x52: "Osric's Spear",
  0x53: "Bentisca",
  0x54: "Holy Lance",
  0x55: "Lance of Longinus",
  0x56: "Evil Spear",
  0x57: "Brionac",
  0x58: "Leather Whip",
  0x59: "Rupture Rose",
  0x5A: "Whip of Exorcism",
  0x5B: "Scourge of Thor",
  0x5C: "Holy Comet",
  0x5D: "Blood Whip",
  0x5E: "Iron Claw",
  0x5F: "Baldr Claw",
  0x60: "Touelno",
  0x61: "Lfal",
  0x62: "Berserk",
  0x63: "Cyanic Claw",
  0x64: "Vajra",
  0x65: "Black Cat",
  0x66: "Short Bow",
  0x67: "Great Bow",
  0x68: "Baldr Bow",
  0x69: "Composite Bow",
  0x6A: "Bow of Thunderbolt",
  0x6B: "Conflagrant Bow",
  0x6C: "Bow of Sandstorm",
  0x6D: "Bow of Tundra",
  0x6E: "Crescente",
  0x6F: "Ytival",
  0x70: "Ji'ylga's Bow",
  0x71: "Light Mace",
  0x72: "Baldr Mace",
  0x73: "Celestial Mace",
  0x74: "Gambantein",
  0x75: "Scipplay Staff",
  0x76: "Arc Wand",
  0x77: "Hraesvelg",
  0x78: "Totila",
  0x79: "Jormungand",
  0x7A: "Phorusgir",
  0x7B: "Airgetlam",
  0x7C: "Kerykeion",
  0x7D: "Hemlock",
  0x7E: "Scepter",
  0x7F: "Marionette",
  0x80: "Fool",
  0x81: "Heaven's Doll",
  0x82: "Lia Fail",
  0x83: "Doll of Curse",
  0x84: "Gallant Doll",
  0x85: "Battle Fan",
  0x86: "Caldia",
  0x87: "Round Shield",
  0x88: "Buckler",
  0x89: "Electric Shield",
  0x8A: "Flame Shield",
  0x8B: "Terra Shield",
  0x8C: "Ice Shield",
  0x8D: "Starry Sky",
  0x8E: "Kite Shield",
  0x8F: "Tower Shield",
  0x90: "Large Shield",
  0x91: "Baldr Shield",
  0x92: "Dragon Shield",
  0x93: "Shield of Nue",
  0x94: "Shield of Inferno",
  0x95: "Crystal Guard",
  0x96: "Saint's Shield",
  0x97: "Ogre Shield",
  0x98: "Hallowed Shield",
  0x99: "Half Armor",
  0x9A: "Cloth Armor",
  0x9B: "Leather Armor",
  0x9C: "Hard Leather",
  0x9D: "Ninja's Garb",
  0x9E: "Scale Armor",
  0x9F: "Chain Mail",
  0xA0: "Thunder Chain",
  0xA1: "Flame Leather",
  0xA2: "Terra Armor",
  0xA3: "Ice Chain",
  0xA4: "Saint's Garb",
  0xA5: "Idaten's Mail",
  0xA6: "Breast Leather",
  0xA7: "Breastplate",
  0xA8: "Plate Mail",
  0xA9: "Baldr Mail",
  0xAA: "Titania Mail",
  0xAB: "Peregrine Mail",
  0xAC: "Phoenix Mail",
  0xAD: "Nathalork Mail",
  0xAE: "Hwail Mail",
  0xAF: "Angelic Armor",
  0xB0: "Bloodstained Armor",
  0xB1: "Plate Armor",
  0xB2: "Baldr Armor",
  0xB3: "Heavy Armor",
  0xB4: "Dragon Armor",
  0xB5: "Wind Armor",
  0xB6: "Breidablick",
  0xB7: "Rune Plate",
  0xB8: "Ogre Armor",
  0xB9: "Armor of Death",
  0xBA: "Southern Cross",
  0xBB: "Jeulnelune",
  0xBC: "Diadora's Song",
  0xBD: "Elem Plate",
  0xBE: "Torn Cloth",
  0xBF: "Robe",
  0xC0: "Cleric's Vestment",
  0xC1: "Magician's Robe",
  0xC2: "Robe of the Wise",
  0xC3: "Vestment of Wind",
  0xC4: "Vestment of Flame",
  0xC5: "Phoenix Robe",
  0xC6: "Vestment of Earth",
  0xC7: "Vestment of Water",
  0xC8: "Cloak of Oath",
  0xC9: "Purified Robe",
  0xCA: "Bloodstained Robe",
  0xCB: "Robe of Abyss",
  0xCC: "Robe of Devus",
  0xCD: "Old Clothing",
  0xCE: "Plain Clothing",
  0xCF: "Witch's Dress",
  0xD0: "Fur Coat",
  0xD1: "Pure-White Dress",
  0xD2: "Feather Suit",
  0xD3: "Heat-Tex",
  0xD4: "Forest Tunic",
  0xD5: "Misty Coat",
  0xD6: "Stardust",
  0xD7: "Spell Robe",
  0xD8: "Tiny Clothing",
  0xD9: "Count's Garment",
  0xDA: "Quilted Cloth",
  0xDB: "Royal Garb",
  0xDC: "Iron Helm",
  0xDD: "Bone Helm",
  0xDE: "Armet",
  0xDF: "Baldr Helm",
  0xE0: "Dragon Helm",
  0xE1: "Helm of Thunderclap",
  0xE2: "Freude Helm",
  0xE3: "Ogre Helm",
  0xE4: "Helm of the Fearless",
  0xE5: "Cross Helm",
  0xE6: "Jelton Helm",
  0xE7: "Leather Hat",
  0xE8: "Bandanna",
  0xE9: "Hachigane",
  0xEA: "Jin-gasa",
  0xEB: "Plumed Headband",
  0xEC: "Pointy Hat",
  0xED: "Hannya Mask",
  0xEE: "Burning Band",
  0xEF: "Ice Bandanna",
  0xF0: "Celestial Veil",
  0xF1: "Red Branch",
  0xF2: "Decoy Cap",
  0xF3: "Spellbook",
  0xF4: "Book of Wind",
  0xF5: "Book of Flame",
  0xF6: "Book of Earth",
  0xF7: "Book of Water",
  0xF8: "Book of Bane",
  0xF9: "Tempest",
  0xFA: "Annihilation",
  0xFB: "Meteor Strike",
  0xFC: "White Mute",
  0xFD: "Amulet",
  0xFE: "Ring of Eloquence",
  0xFF: "Firecrest",
  0x100: "Bell of Thunder",
  0x101: "Fang of Firedrake",
  0x102: "Naga Ring",
  0x103: "Snow Orb",
  0x104: "Rosary",
  0x105: "Elder's Sign",
  0x106: "Feather of Archangel",
  0x107: "Ring of Branding",
  0x108: "Angel's Brooch",
  0x109: "Rai's Tear",
  0x10A: "Runic Cape",
  0x10B: "Glass Pumpkin",
  0x10C: "Dream Tiara",
  0x10D: "Royal Crown",
  0x10E: "Bloody Emblem",
  0x10F: "Ring of the Dead",
  0x110: "Valiant Mantle",
  0x111: "Fur-lined Mantle",
  0x112: "Majestic Mantle",
  0x113: "Blue Sash",
  0x114: "Tunic",
  0x115: "Guard Tunic",
};

OB64.ITEM_CATEGORIES = {
  "Swords":          [0x01, 0x2F],
  "Axes & Hammers":  [0x30, 0x48],
  "Spears & Lances": [0x49, 0x57],
  "Whips & Claws":   [0x58, 0x65],
  "Bows":            [0x66, 0x70],
  "Maces & Staves":  [0x71, 0x7E],
  "Dolls & Fans":    [0x7F, 0x86],
  "Shields":         [0x87, 0x98],
  "Body Armor":      [0x99, 0xBD],
  "Robes & Clothing":[0xBE, 0xDB],
  "Helms & Headgear":[0xDC, 0xF2],
  "Spellbooks":      [0xF3, 0xFC],
  "Accessories":     [0xFD, 0x115],
};

OB64.itemName = function(id) {
  if (id === 0) return "(None)";
  return OB64.ITEM_NAMES[id] || ("Item_0x" + id.toString(16).padStart(2, "0"));
};

OB64.itemCategory = function(id) {
  for (var cat in OB64.ITEM_CATEGORIES) {
    var range = OB64.ITEM_CATEGORIES[cat];
    if (id >= range[0] && id <= range[1]) return cat;
  }
  return "Unknown";
};

// ============================================================
// SPELL NAMES (109 entries)
// ROM 0x5D560-0x5DAD3
// ============================================================
OB64.SPELL_NAMES = {
  0x00: "Physical",
  0x01: "Wind",
  0x02: "Flame",
  0x03: "Earth",
  0x04: "Water",
  0x05: "Virtue",
  0x06: "Bane",
  0x07: "Drakonite",
  0x08: "Variable",
  0x09: "Wind+Flame",
  0x0A: "Flame+Wind",
  0x0B: "Earth+Flame",
  0x0C: "Water+Earth",
  0x0D: "Wind+Bane",
  0x0E: "Flame+Bane",
  0x0F: "Thrust",
  0x10: "Peck",
  0x11: "Pierce",
  0x12: "Slash",
  0x13: "Cleave",
  0x14: "Rend",
  0x15: "Claw",
  0x16: "Bite",
  0x17: "Strike",
  0x18: "Crush",
  0x19: "Smash",
  0x1A: "Lash",
  0x1B: "Pull Strings",
  0x1C: "Shoot",
  0x1D: "Take a Peek",
  0x1E: "Flip Over",
  0x1F: "Sonic Boom",
  0x20: "Wind Shot",
  0x21: "Wind Storm",
  0x22: "Mesmerize",
  0x23: "Life Drain",
  0x24: "Throw a Kiss",
  0x25: "Pumpkin Smash",
  0x26: "Banish",
  0x27: "Jihad",
  0x28: "Magic Missile",
  0x29: "Abyss",
  0x2A: "Fire Breath",
  0x2B: "Acid Breath",
  0x2C: "Breath of Cold",
  0x2D: "Sacred Breath",
  0x2E: "Rotten Breath",
  0x2F: "Petrify",
  0x30: "Radiant Gale",
  0x31: "Crimson Note",
  0x32: "Earthquake",
  0x33: "Divine Ray",
  0x34: "Evil Dead",
  0x35: "Evocation",
  0x36: "Ninja Art",
  0x37: "Lightning",
  0x38: "Thunder Flare",
  0x39: "Shock Bolt",
  0x3A: "Thunderbird",
  0x3B: "Fireball",
  0x3C: "Fire Storm",
  0x3D: "Salamander",
  0x3E: "Acid Vapor",
  0x3F: "Crag Press",
  0x40: "Poison Cloud",
  0x41: "Gnome",
  0x42: "Ice Blast",
  0x43: "Ice Field",
  0x44: "Slumber Mist",
  0x45: "Fenrir",
  0x46: "Healing",
  0x47: "Healing Plus",
  0x48: "Word of Pain",
  0x49: "Dark Quest",
  0x4A: "Nightmare",
  0x4B: "Dark Lore",
  0x4C: "Tempest",
  0x4D: "Annihilation",
  0x4E: "Meteor Strike",
  0x4F: "White Mute",
  0x50: "Ionosphere",
  0x51: "Atmosphere",
  0x52: "Deep Sleep",
  0x53: "Plasma Ball",
  0x54: "Plasma Storm",
  0x55: "Bind Flare",
  0x56: "Lava Shot",
  0x57: "Lava Flow",
  0x58: "Poison Plant",
  0x59: "Clay Assault",
  0x5A: "Blue Spiral",
  0x5B: "Poison Lime",
  0x5C: "Infest",
  0x5D: "Inferno",
  0x5E: "Black Breeze",
  0x5F: "Dark Blaze",
  0x60: "Dark Flame",
  0x61: "Doom",
  0x62: "Sonic Blast",
  0x63: "Wind Blast",
  0x64: "Fire Blast",
  0x65: "Earth Blast",
  0x66: "Aqua Blast",
  0x67: "Holy Blast",
  0x68: "Dark Blast",
  0x69: "Sonic Blade",
  0x6A: "Ignis Fatuus",
  0x6B: "Mirage Slash",
  0x6C: "Fatal Dance",
};

OB64.SPELL_CATEGORIES = {
  "Element Types":       [0x00, 0x08],
  "Combo Elements":      [0x09, 0x0E],
  "Physical Attacks":    [0x0F, 0x1F],
  "Special Attacks":     [0x20, 0x36],
  "Offensive Spells":    [0x37, 0x45],
  "Healing & Dark":      [0x46, 0x4B],
  "Spellbook Abilities": [0x4C, 0x4F],
  "Advanced Spells":     [0x50, 0x61],
  "Elemental Blasts":    [0x62, 0x68],
  "Ultimate Attacks":    [0x69, 0x6C],
};

OB64.spellName = function(id) {
  return OB64.SPELL_NAMES[id] || ("Spell_0x" + id.toString(16).padStart(2, "0"));
};

OB64.spellCategory = function(id) {
  for (var cat in OB64.SPELL_CATEGORIES) {
    var range = OB64.SPELL_CATEGORIES[cat];
    if (id >= range[0] && id <= range[1]) return cat;
  }
  return "Unknown";
};

// ============================================================
// EQUIPMENT TYPE NAMES (byte 0 of item stat record)
// ============================================================
OB64.EQUIP_TYPES = {
  0x01: "1H Sword",
  0x02: "2H Sword",
  0x03: "Rapier",
  0x04: "1H Axe",
  0x05: "2H Axe",
  0x07: "Spear",
  0x08: "Whip",
  0x09: "Claw",
  0x0A: "Bow",
  0x0B: "Mace",
  0x0C: "Staff",
  0x0D: "Doll",
  0x0E: "Light Shield",
  0x0F: "Heavy Shield",
  0x10: "Light Armor",
  0x11: "Medium Armor",
  0x12: "Heavy Armor",
  0x13: "Robe",
  0x14: "Clothing",
  0x15: "Helm",
  0x16: "Headgear",
  0x17: "Spellbook",
  0x18: "Fan",
  0x19: "Accessory",
  0xFF: "\u2014",
};

OB64.equipTypeName = function(id) {
  return OB64.EQUIP_TYPES[id] || ("0x" + id.toString(16).padStart(2, "0"));
};

// Returns true if the equip type byte indicates a weapon
OB64.isWeapon = function(equipType) {
  return (equipType >= 0x01 && equipType <= 0x0D) || equipType === 0x17 || equipType === 0x18;
};

// Returns true if the equip type byte indicates armor/shield/headgear
OB64.isDefensive = function(equipType) {
  return equipType >= 0x0E && equipType <= 0x16;
};

// ============================================================
// EQUIPMENT SLOT TYPE NAMES (class def B42/B44/B46 first byte)
// ============================================================
OB64.EQUIP_SLOT_TYPES = {
  0x00: "Weapon",
  0x01: "Body",
  0x02: "Off-hand",
  0x03: "Advanced",
  0x08: "Special",
  0x0F: "Fixed",
};

OB64.equipSlotTypeName = function(id) {
  return OB64.EQUIP_SLOT_TYPES[id] || ("Flag 0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// EQUIPMENT GROUP DESCRIPTIONS (class def B43/B45/B47 second byte)
// Cross-referenced from H2F Mod CSV default equipment per class.
// These are equipment profile indices that determine which item
// categories a class can equip in each slot.
// ============================================================
OB64.EQUIP_GROUPS = {
  0x01: "Spear/Light",     // Soldier, Dragon Tamer: spears + light armor
  0x03: "Spear/Heavy",     // Phalanx, Cataphract: spears + heavy armor + heavy shields
  0x04: "Sword/Armor",     // Fighter, Knight, Paladin, etc: swords + armor + shields
  0x05: "Polearm/Med",     // Valkyrie, Black Knight, Freya: polearms + medium armor
  0x06: "Claw/Ninja",      // Ninja, Ninja Master: claws + ninja garb
  0x09: "Axe/Light",       // Berserker: axes + light armor
  0x0C: "Whip/Leather",    // Beast Tamer, Beast Master: whips + leather
  0x0D: "Doll/Robe",       // Doll Master, Enchanter: dolls + robes
  0x0E: "Bow/Light",       // Amazon, Archer, Diana: bows + light armor
  0x11: "Clothing",        // Sword Master slot 4: light clothing
  0x2D: "Staff/Magic",     // Wizard, Sorceress, Archmage, Siren: staves + robes + spellbooks
  0x2E: "Adv. Magic",      // Archmage, Siren, Freya: advanced magic accessories
  0x2F: "Witch Magic",     // Witch: staves + witch dress + spellbooks
  0x32: "Ninja Adv.",      // Ninja Master slot 4: advanced ninja accessories
  0x33: "Valkyrie Arm",    // Valkyrie slot 4: special armor
  0x34: "Cleric",          // Cleric: maces + cleric vestments
  0x35: "Priest",          // Priest: maces + robes of the wise
};

OB64.equipGroupName = function(id) {
  return OB64.EQUIP_GROUPS[id] || ("Group 0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// RESISTANCE NAMES (class def B25-B31, in order)
// ============================================================
OB64.RESISTANCE_NAMES = ['Physical', 'Air', 'Fire', 'Earth', 'Water', 'Virtue', 'Bane'];

// ============================================================
// ELEMENT NAMES (byte 1 of item stat record)
// ============================================================
OB64.ELEMENT_NAMES = {
  0x00: "Physical",
  0x01: "Wind",
  0x02: "Flame",
  0x03: "Earth",
  0x04: "Water",
  0x05: "Virtue",
  0x06: "Bane",
  0x07: "Drakonite",
};

OB64.elementName = function(id) {
  return OB64.ELEMENT_NAMES[id] || ("0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// STRONGHOLD TYPE & CAPABILITY NAMES
// Parsed from ktenmain.bin (archive #691) — 316 records x 28 bytes
// ============================================================
OB64.STRONGHOLD_TYPES = {
  0x09: "Town",
  0x29: "Fort",
  0x49: "Boss",
  0x89: "Castle",
};

OB64.strongholdTypeName = function(type) {
  return OB64.STRONGHOLD_TYPES[type] || ("0x" + type.toString(16).padStart(2, "0"));
};

// Capability bitmask (byte 25): bit 0=shop, bit 1=temple, bit 2=treasure, bit 3=mine
OB64.strongholdCapabilities = function(caps) {
  var parts = [];
  if (caps & 1) parts.push("Shop");
  if (caps & 2) parts.push("Temple");
  if (caps & 4) parts.push("Treasure");
  if (caps & 8) parts.push("Mine");
  return parts.length ? parts.join(", ") : "\u2014";
};

// Get all stronghold names that use a given shop index (from rom.shopStrongholds)
OB64.shopTowns = function(shopStrongholds, idx) {
  var names = shopStrongholds ? shopStrongholds[idx] : null;
  return names ? names.join(", ") : "";
};

// ============================================================
// SCINCSV FLAG NAMES
// ============================================================
OB64.SCINCSV_FLAGS = {
  0x0000: "None",
  0x0004: "Normal",
  0x2002: "Rare",
  0x2012: "Boss",
};

OB64.scincsvFlagName = function(flags) {
  return OB64.SCINCSV_FLAGS[flags] || ("0x" + flags.toString(16).padStart(4, "0"));
};

// ============================================================
// MOVEMENT TYPES (class def B32)
// ============================================================
OB64.MOVEMENT_TYPES = {
  0x01: "Float",
  0x02: "Foot",
  0x03: "Fast/Ninja",
  0x04: "Heavy",
  0x05: "Flying",
  0x06: "Ghost",
  0x07: "Skeleton",
};

OB64.moveTypeName = function(id) {
  return OB64.MOVEMENT_TYPES[id] || ("0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// UNIT TYPES (class def B64)
// ============================================================
OB64.UNIT_TYPES = {
  0x01: "Humanoid",
  0x02: "Beast/Dragon",
};

OB64.unitTypeName = function(id) {
  return OB64.UNIT_TYPES[id] || ("0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// SPRITE / BODY TYPES (class def B65)
// ============================================================
OB64.SPRITE_TYPES = {
  0: "Std Humanoid",
  1: "Alt/Magic Humanoid",
  2: "Large Beast",
  3: "Inanimate",
  4: "Undead Monster",
};

OB64.spriteTypeName = function(id) {
  return OB64.SPRITE_TYPES[id] || ("0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// COMBAT BEHAVIOR / LEADER TIER (class def B66)
// ============================================================
OB64.COMBAT_BEHAVIORS = {
  0: "Beast/Passive",
  1: "Standard Weapon",
  2: "Leader/Command",
};

OB64.combatBehaviorName = function(id) {
  return OB64.COMBAT_BEHAVIORS[id] || ("0x" + id.toString(16).padStart(2, "0"));
};

// ============================================================
// DRAGON ELEMENTS (class def B58)
// ============================================================
// B58 — default damage element.
//
// Originally labeled "Dragon Element" but cross-checking against
// "Class Chart.csv" shows this byte is the per-class default attack element
// for ALL classes, not just dragons. Soldier (B58=0x00) and Ogre (B58=0x00)
// both show CSV "Default Element: Physical". Dragons happen to have this
// set to their breath element (Red Dragon=Flame, Blue Dragon=Water, etc.).
// 0xFF is the sentinel for classes with no fixed element (CSV "Random").
OB64.DEFAULT_ELEMENTS = {
  0x00: "Physical",
  0x01: "Wind",
  0x02: "Flame",
  0x03: "Earth",
  0x04: "Water",
  0xFF: "Random / None",
};

// Back-compat aliases — existing app.js / card rendering still references
// these names. Point them at the renamed table.
OB64.DRAGON_ELEMENTS = OB64.DEFAULT_ELEMENTS;

OB64.defaultElementName = function(id) {
  return OB64.DEFAULT_ELEMENTS[id] || ("0x" + id.toString(16).padStart(2, "0"));
};
OB64.dragonElementName = OB64.defaultElementName;

// ============================================================
// CLASS CATEGORY / TIER (class def B59)
// ============================================================
OB64.CLASS_TIERS = {
  0x01: "Base/Magic",
  0x02: "Combat",
  0x03: "Mid-Dragon",
  0x04: "High-Dragon",
};

OB64.classTierName = function(id) {
  return OB64.CLASS_TIERS[id] || ("0x" + id.toString(16).padStart(2, "0"));
};

/* ============================================================================
   SAVE-GAME EDITOR constants
   See docs/editor.md "Save tab" for the empirical RAM layout this maps to.
   ============================================================================ */

OB64.SAVE = {
  RDRAM_SIZE: 0x800000,          // 8 MB
  CHAR_STRIDE: 56,               // per-character struct size in bytes
  MAX_SLOTS: 60,                 // realistic upper bound; scan stops on empty run
  NAME_MAX_LEN: 16,              // 16-byte null-padded ASCII field

  // Character struct field offsets (big-endian RAM, post-unswap).
  FIELD: {
    NAME:          0x00,   // 16 bytes ASCII, null-padded
    CLASS_ID:      0x11,   // u8
    CLASS_ID_COPY: 0x12,   // u8 (mirror)
    LEVEL:         0x13,   // u8
    GENDER:        0x14,   // u8 (0=Male, 1=Female, 2=Beast/Dragon — per GS guide)
    ELEMENT:       0x15,   // u8 (0=class default, 1-B per GS guide) — EXPERIMENTAL
    HP_MAX:        0x17,   // u8
    FLAG_1A:       0x1A,   // u8 — seed default 0x02; observed 0x01/0x02 on real chars
    FLAG_1B:       0x1B,   // u8 — seed default 0x30; observed 0x1e-0x4B on real chars
    HP_CUR:        0x19,   // u8
    ALIGNMENT:     0x28,   // u8 (0=Chaotic, 50=Neutral, 100=Lawful). +0x28 shows
                           // values 50-56 across most player chars and 0x55=85
                           // (Lawful) for Belinda, consistent with alignment.
                           // NOT +0x2B: that offset is WEAPON (confirmed via
                           // Frost state2↔state3 diff).
    EXP:           0x35,   // u8 experience toward next level (verified vs in-game)
    // Stat offsets VERIFIED 2026-04-21 against in-game display in state2
    // (Magnus, Frost, Eva). docs/character-struct.md had the three pairs
    // swapped (STR↔VIT, INT↔MEN, AGI↔DEX). Real layout:
    STR:           0x1C,   // u16 BE
    VIT:           0x1E,   // u16 BE
    INT:           0x20,   // u16 BE
    MEN:           0x22,   // u16 BE
    AGI:           0x24,   // u16 BE
    DEX:           0x26,   // u16 BE
    // Equipment — u8 item id overrides; 0x00 = use the class default (class
    // def B34-41, which stores u16 per slot so items > 255 like Blue Sash /
    // Amulet / Spellbook are reachable only via class default).
    // Confirmed 2026-04-21 via state2↔state3 diff on Frost + Stephanie.
    // Slot names mirror docs/rom-layout.md:332-333 (B38-39 "shield/off-hand",
    // B40-41 "headgear/accessory") — off-hand holds shield/spellbook/accessory.
    WEAPON:        0x2B,   // u8 item id
    BODY:          0x2D,   // u8 item id
    OFFHAND:       0x2F,   // u8 item id (shield / spellbook / accessory)
    HEAD:          0x31,   // u8 item id (helm / headgear / accessory)

    SLOT_INDEX:    0x34,   // u8, 1-indexed slot position
  },

  // Army equipment inventory — phys 0x196CCC.
  // Flat list of 4-byte entries: [u16 BE item_id, u8 equipped_count, u8 owned_count].
  // Terminator = all-zero record. Item IDs are equipment-table IDs (weapons,
  // body armor, shields, helms, accessories, spellbooks).
  INVENTORY_BASE: 0x196CCC,
  INVENTORY_ENTRY_SIZE: 4,
  INVENTORY_MAX_ENTRIES: 128,

  // Army consumable + treasure inventory — phys 0x193C8D (found 2026-04-21 via
  // state2→state3 diff after selling consumables and Ansate Cross).
  // Format: flat list of 4-byte records, zero-terminated. Each record is
  // [u8 consumable_id, 0x00, u8 count, 0x00]. Consumable IDs index into the
  // 45-entry consumable master table (parsed by OB64.parseConsumables).
  // The "Treasure" tab filters this list by flagHi category; quest items like
  // Ansate Cross (consumable id 25) live alongside Heal Leaf (id 1) etc.
  CONSUMABLE_INV_BASE: 0x193C8D,
  CONSUMABLE_INV_ENTRY_SIZE: 4,
  CONSUMABLE_INV_MAX_ENTRIES: 64,

  // Consumable id categorization — mirrors the in-game Item menu.
  // IDs 1-24: Consumable tab (curatives + bestowals).
  // IDs 25-44: Treasure tab (quest items + pedras).
  // Derived from the cons_NN_ icon naming convention in the consumable icon set.
  TREASURE_MIN_CONSUMABLE_ID: 25,

  // Tab order for the Save Game Editor's inventory section.
  // Icons from resources/scraped sprites/ (shop-*.png for 8 categories).
  INVENTORY_TABS: [
    { id: "head",       label: "Head",       icon: "shop-equipment-head.png" },
    { id: "weapon",     label: "Weapon",     icon: "shop-weapon.png"         },
    { id: "spellbook",  label: "Spellbook",  icon: "shop-book.png"           },
    { id: "shield",     label: "Shield",     icon: "shop-shield.png"         },
    { id: "armor",      label: "Armor",      icon: "shop-equipment-body.png" },
    { id: "accessory",  label: "Accessory",  icon: "shop-accessory.png"      },
    { id: "consumable", label: "Consumable", icon: "shop-expendable.png"     },
    { id: "treasure",   label: "Treasure",   icon: "shop-treasure.png"       },
  ],

  // Item name (text) for consumable ids 0-44, derived from cons_NN_*.png
  // filenames in the consumable icon set. Stops the Save tab depending on the
  // ROM for consumable names.
  CONSUMABLE_NAMES: {
    0:  "(None)",
    1:  "Heal Leaf",          2:  "Heal Seed",          3:  "Heal Pack",
    4:  "Power Fruit",        5:  "Angel Fruit",        6:  "Revive Stone",
    7:  "Altar of Resurrection", 8: "Quit Gate",        9:  "Champion Statuette",
    10: "Cup of Life",        11: "Sword Emblem",       12: "Bracer of Protection",
    13: "Crown of Intellect", 14: "Mirror of Soul",     15: "Stone of Quickness",
    16: "Crystal of Precision", 17: "Scroll of Discipline", 18: "Urn of Chaos",
    19: "Goblet of Destiny",  20: "Flag of Unity",      21: "Silver Hourglass",
    22: "Dowsing Rod",        23: "Love and Peace",     24: "Medal of Vigor",
    25: "Ansate Cross",       26: "Marching Baton",     27: "Censer of Repose",
    28: "Figurine of Sleipnir", 29: "Manual of Warfare", 30: "Mastaba's Barrier",
    31: "Charge Horn",        32: "Letter from Father", 33: "Condrite",
    34: "Bolt of Silk",       35: "Dragon's Scale",     36: "Dark Invitation",
    37: "Package for Gelda",  38: "Letter from Gelda",  39: "Pedra of Wind",
    40: "Pedra of Flame",     41: "Pedra of Earth",     42: "Pedra of Water",
    43: "Pedra of Virtue",    44: "Pedra of Bane",
  },

  // Game-state bytes — offsets per docs/game-state.md (physical RAM offsets,
  // NOT virtual KSEG0). Re-verify these if a file comes in with garbage.
  GAME_STATE: {
    TIME_OF_DAY:      0x196A28,
    CHAPTER:          0x196A2A,
    MISSION_PROGRESS: 0x196A2E,
    DAY:              0x196A30,
    MONTH:            0x196A31,
    SCENARIO:         0x196A74,
    MAP_LOCATION:     0x196A99,
    // Goth / war funds — u32 BE at 0x196C38. Found 2026-04-21 via state2→state3
    // diff (5059 → 5521 after selling consumables + Ansate Cross).
    GOTH:             0x196C38,
  },

  // Known starting-character names used to anchor the army-array scan.
  // Magnus is always present and always the first slot in any save.
  ANCHOR_NAME: "Magnus",

  // Month ids → names (per in-game calendar).
  MONTHS: {
    1: "Januar", 2: "Pisces", 3: "Vernus", 4: "Sombra",
    5: "Festas", 6: "Lusas", 7: "Beisas", 8: "Ferven",
    9: "Kastor", 10: "Noviem", 11: "Umbra", 12: "Crystal",
  },

  // Scenario id → name (partial, just the handful confirmed in game-state.md).
  SCENARIO_LABELS: {
    0: "World map",
    5: "In scenario",
  },

  // World-map location labels (confirmed subset from game-state.md).
  MAP_LOCATIONS: {
    4: "Volmus Mine",
    6: "Tenne Plains",
    7: "Alba",
  },

  // Per-character element override values — from GS guide supplementary codes.
  // Value 0 means "use the class default element" (class def B58).
  ELEMENT_OVERRIDES: {
    0:  "(class default)",
    1:  "Wind",
    2:  "Fire",
    3:  "Earth",
    4:  "Water",
    5:  "Physical clone",
    6:  "Wind clone (Tempest)",
    7:  "Fire clone (Annihilation)",
    8:  "Earth clone (Meteor Strike)",
    9:  "Water clone (White Mute)",
    10: "Virtue clone",
    11: "Bane clone",
  },

  // Alignment label buckets (informational only; the field is a 0-100 number).
  ALIGNMENT_BUCKETS: [
    { min: 0,  max: 33,  label: "Chaotic" },
    { min: 34, max: 66,  label: "Neutral" },
    { min: 67, max: 100, label: "Lawful"  },
  ],
};

// Categorize a regular equipment-item id to one of the inventory tab ids.
// Returns "head"|"weapon"|"spellbook"|"shield"|"armor"|"accessory" or null.
OB64.tabForItemId = function(id) {
  if (id >= 0xDC && id <= 0xF2) return "head";
  if (id >= 0x87 && id <= 0x98) return "shield";
  if (id >= 0xF3 && id <= 0xFC) return "spellbook";
  if (id >= 0xFD && id <= 0x115) return "accessory";
  if (id >= 0x99 && id <= 0xDB) return "armor";
  if (id >= 0x01 && id <= 0x86) return "weapon";
  return null;
};

// Categorize a consumable id (1-44) to "consumable" or "treasure".
OB64.tabForConsumableId = function(id) {
  if (id >= OB64.SAVE.TREASURE_MIN_CONSUMABLE_ID) return "treasure";
  return "consumable";
};

// Consumable name lookup using the Save-tab standalone map.
OB64.consumableName = function(id) {
  if (id === 0) return "(None)";
  return (OB64.SAVE.CONSUMABLE_NAMES && OB64.SAVE.CONSUMABLE_NAMES[id])
    || ("Consumable_" + id);
};
