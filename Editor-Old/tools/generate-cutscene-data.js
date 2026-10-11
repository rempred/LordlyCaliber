#!/usr/bin/env node
'use strict';

// Build the metadata-only Cutscene Studio catalog from accepted parent-side
// products and a read-only US Rev 0 retail ROM. The ROM supplies the complete
// native Director selector inventory and source-envelope fingerprints. This
// tool never writes the ROM and never embeds decoded pixels or command words.

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const editorRoot = path.resolve(__dirname, '..');
const workspaceRoot = path.resolve(editorRoot, '..');
const defaultMasterRom = path.join(
  workspaceRoot, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
const defaultAssetManifest = path.join(
  workspaceRoot, 'tools', 'cutscene-workbench', 'generated', 'asset-manifest.json');
const defaultDirectorCorpusRoot = path.join(
  workspaceRoot, 'wiki', 'cutscene-director-corpus-153-grammar-static-r1-20260821');
const defaultDirectorCorpusAssets = path.join(
  defaultDirectorCorpusRoot, 'exports', 'assets.jsonl');
const defaultDirectorCorpusGrammar = path.join(
  defaultDirectorCorpusRoot, 'exports', 'grammar.jsonl');
const defaultDirectorCorpusNodes = path.join(
  defaultDirectorCorpusRoot, 'exports', 'nodes.jsonl');
const defaultDirectorCorpusRegisteredWaits = path.join(
  defaultDirectorCorpusRoot, 'exports', 'registered-waits.jsonl');
const defaultArchiveCatalog = path.join(
  workspaceRoot, 'scripts', 'ob64_archive_catalog.json');
const defaultPoseVocabulary = path.join(
  workspaceRoot, 'tools', 'cutscene-workbench', 'generated', 'pose-vocabulary.json');
const defaultPoseSelectors = path.join(
  workspaceRoot, 'wiki', 'cutscene-all-bank-pose-rom-map-static-20260711',
  'exports', 'selectors.jsonl');
const defaultPosePhysicalStates = path.join(
  workspaceRoot, 'wiki', 'cutscene-all-bank-pose-rom-map-static-20260711',
  'exports', 'physical-states.jsonl');
const defaultPartialDirectorAssets = path.join(
  workspaceRoot, 'wiki',
  'cutscene-rom-wide-director-asset-census-static-r2-20260712-director-review',
  'exports', 'director-assets.jsonl');
const defaultDirectorTailRecovery = path.join(
  workspaceRoot, 'tools', 'cutscene-workbench', 'generated',
  'director-tail-recovery.json');
const defaultDirectorSelectorTable = path.join(
  workspaceRoot, 'tools', 'cutscene-workbench', 'generated',
  'director-selector-table.json');
const defaultSerifuPresentationSelectors = path.join(
  workspaceRoot, 'tools', 'cutscene-workbench', 'generated',
  'serifu-presentation-selectors.json');
const defaultSpriteCorpus = path.join(
  workspaceRoot, 'wiki', 'sprite-animation-key-resource-research-20260813-v2',
  'corpus-scan.json');
const defaultAudioCatalog = path.join(
  workspaceRoot, 'scripts', 'ob64_anim_block_catalog.json');
const defaultExtractedRoot = path.join(workspaceRoot, 'ob64_all');
const defaultOutput = path.join(editorRoot, 'cutscene-data.js');

const ACTOR_ART_FAMILY_LABELS = Object.freeze({
  3: Object.freeze({
    label: 'Knights family',
    status: 'supported family wording',
    scope: 'visual family only; no scene-local character identity'
  }),
  23: Object.freeze({
    label: 'Hawkman / Vultan / Raven body-art family',
    status: 'candidate exact cross-system body-art overlap; review pending',
    scope: 'visual body-art family only; no scene-local character identity'
  }),
  30: Object.freeze({
    label: 'Magnus family',
    status: 'canonical visual-family label',
    scope: 'visual family only; only one opening slot has a supported Magnus identity join'
  })
});

// These physical words also have an exact control-flow entry inside the node.
// Retain the source bytes, but do not project the overlapping actor command as
// the default visual interpretation.
const CONTROL_ENTRY_ALIASES = Object.freeze({
  'rom-director:01F62EE0:word:00C4': 1,
  'rom-director:01FC98DE:word:0020': 1,
  'rom-director:01FCC166:word:0020': 1,
  'rom-director:01FCD30A:word:0020': 1,
  'rom-director:01FCD6F8:word:0020': 1,
  'rom-custom-lz:01F3E836:word:0171': 1,
  'rom-custom-lz:01F3E836:word:018C': 1,
  'rom-custom-lz:01F3E836:word:01A7': 1,
  'rom-director:01F581EA:word:016F': 1,
  'rom-director:01F64362:word:0088': 1,
  'rom-director:01F76642:word:00BC': 1,
  'rom-director:01F76642:word:00DC': 1,
  'rom-director:01F85F46:word:0297': 1,
  'rom-director:01F86264:word:0212': 1,
  'rom-director:01F86264:word:0292': 1,
  'rom-director:01F86B46:word:01D0': 1,
  'rom-director:01F85F46:word:0205': 5,
  'rom-director:01F3F70A:recovered-word:02A3': 7,
  'rom-director:01F3F70A:recovered-word:02D2': 5,
  'rom-director:01F3F70A:recovered-word:0790': 2,
  'rom-director:01F3F70A:recovered-word:0799': 2,
  'rom-director:01F3F70A:recovered-word:0A33': 8,
  'rom-director:01F3F70A:recovered-word:07C6': 1,
  'rom-director:01F3F70A:recovered-word:07D0': 3,
  'rom-director:01F3F70A:recovered-word:0807': 1
});

const REGISTERED_AUDIO_REQUESTS = Object.freeze([
  { requestId: 90, indexRecordZ64: 0x004E3428, payloadOffset: 0x3F6D,
    z64Start: 0x004E70AD, z64EndExclusive: 0x004E70CC,
    routeStatus: 'conditional fixed-class queue only; no accepted selector-0 opcode-0x70 occurrence selects this resource' },
  { requestId: 120, indexRecordZ64: 0x004E3518, payloadOffset: 0x42F7,
    z64Start: 0x004E7437, z64EndExclusive: 0x004E7459,
    routeStatus: 'conditional fixed-class queue only; no accepted selector-0 opcode-0x70 occurrence selects this resource' },
  { requestId: 180, indexRecordZ64: 0x004E36F8, payloadOffset: 0x4A59,
    z64Start: 0x004E7B99, z64EndExclusive: 0x004E7BBF,
    routeStatus: 'conditional fixed-class queue only; no accepted selector-0 opcode-0x70 occurrence selects this resource' },
  { requestId: 527, indexRecordZ64: 0x004E41D0, payloadOffset: 0x70A2,
    z64Start: 0x004EA1E2, z64EndExclusive: 0x004EA1F6,
    routeStatus: 'exact selector-5 opcode-0x6E request route; semantic name unresolved' },
  { requestId: 472, indexRecordZ64: 0x004E4018, payloadOffset: 0x6B7E,
    z64Start: 0x004E9CBE, z64EndExclusive: 0x004E9CD9,
    payloadSha256: 'B41C1DC3B5FBC60B5C01C889F194473CA0CC3D576E9EE7960B7803AC702D38D9',
    routeStatus: 'exact selector-5 recovered opcode-0x6E request route; semantic name unresolved' },
  { requestId: 701, indexRecordZ64: 0x004E4740, payloadOffset: 0x8222,
    z64Start: 0x004EB362, z64EndExclusive: 0x004EB37D,
    routeStatus: 'exact selector-4 opcode-0x6E request route; semantic name unresolved' },
  { requestId: 707, indexRecordZ64: 0x004E4770, payloadOffset: 0x8296,
    z64Start: 0x004EB3D6, z64EndExclusive: 0x004EB3EB,
    payloadSha256: '826165B5C5158833AD570F20D858118976ADA504C1531F3EA45B8717C34EF0BD',
    routeStatus: 'exact selector-5 recovered opcode-0x6E request route; semantic name unresolved' },
  { requestId: 708, indexRecordZ64: 0x004E4778, payloadOffset: 0x82AB,
    z64Start: 0x004EB3EB, z64EndExclusive: 0x004EB403,
    payloadSha256: 'FAE1E1662B1B88AD07A6142A41F5A5439E82CA356C6E9F087310FAE3C620FB56',
    routeStatus: 'exact selector-5 recovered opcode-0x6E request route; semantic name unresolved' }
]);

const DIRECTOR_AUDIO_BLOCK_BY_REQUEST = Object.freeze({
  3: 2,
  8: 6,
  13: 11,
  14: 12,
  32: 29,
  35: 32,
  36: 33,
  37: 34,
  40: 37,
  42: 39,
  43: 40,
  44: 41,
  47: 44,
  49: 46,
  54: 51
});

// The native start/end table has 66 request rows. Rows 1, 5, and 15 are
// zero-length. The remaining 63 rows map in order to the contiguous audio
// block catalog.
const EMPTY_DIRECTOR_AUDIO_REQUEST_ROWS = Object.freeze([1, 5, 15]);

// These are exact non-Director request references. They describe the calling
// system, not a music or sound-effect title. Scenario keys come from the
// scenario-resource row +7 request byte consumed by func_00101DDC.
const AUDIO_BLOCK_CONTEXTS = Object.freeze({
  0: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([10, 22, 35, 62])
  }),
  8: Object.freeze({
    contextLabel: 'scenario-completion settlement reference',
    sourceFunction: 'func_001390F0'
  }),
  14: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([0, 1, 6, 7, 9, 11, 12, 13, 14, 63])
  }),
  15: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([
      36, 37, 38, 40, 41, 42, 43, 44, 45, 46, 49, 50, 52, 53, 55, 56, 57, 58, 59, 64
    ])
  }),
  16: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([2, 3, 4, 5, 8, 17, 18, 19, 20, 21, 30, 31, 32, 33, 34])
  }),
  17: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([15, 16])
  }),
  18: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([23, 24, 25, 26, 27, 28])
  }),
  20: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([29])
  }),
  21: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([39, 47, 48, 51, 60, 61])
  }),
  22: Object.freeze({
    contextLabel: 'scenario setup reference',
    scenarioRuntimeKeys: Object.freeze([54])
  }),
  30: Object.freeze({
    contextLabel: 'battle-results reference',
    sourceFunction: 'func_002152FC'
  })
});

function nativeAudioBlockIndexForRequest(requestValue) {
  if (!Number.isInteger(requestValue) || requestValue < 0 || requestValue > 65 ||
      EMPTY_DIRECTOR_AUDIO_REQUEST_ROWS.includes(requestValue)) return null;
  return requestValue - EMPTY_DIRECTOR_AUDIO_REQUEST_ROWS.filter(
    (emptyRow) => emptyRow < requestValue).length;
}

function nativeAudioRequestForBlockIndex(blockIndex) {
  if (!Number.isInteger(blockIndex) || blockIndex < 0 || blockIndex >= 63) return null;
  let requestValue = blockIndex;
  EMPTY_DIRECTOR_AUDIO_REQUEST_ROWS.forEach((emptyRow) => {
    if (requestValue >= emptyRow) requestValue += 1;
  });
  return requestValue;
}

const PARTIAL_DIRECTOR_RESOURCE_IDS = Object.freeze([
  'rom-director:01F3F70A',
  'rom-director:01FAA540'
]);

const SCENARIO_1_BRIEFING_ACTORS = Object.freeze([
  { slot: 0, bank: 30, animationKey: 0, facing: 2, x: 65, y: 0, z: 10, variantSelector: 0 },
  { slot: 1, bank: 3, animationKey: 0, facing: 2, x: 120, y: 0, z: 10, variantSelector: 6 },
  { slot: 2, bank: 42, animationKey: 60, facing: 0, x: -34, y: 0, z: 117, variantSelector: 0 },
  { slot: 9, bank: 57, animationKey: 100, facing: 0, x: -39, y: 0, z: 117,
    variantSelector: 0, recoveredWordStart: 0x01B9 },
  { slot: 10, bank: 57, animationKey: 114, facing: 3, x: -1, y: 0, z: 134, variantSelector: 0 },
  { slot: 11, bank: 57, animationKey: 115, facing: 3, x: 30, y: 0, z: 134, variantSelector: 0 },
  { slot: 12, bank: 57, animationKey: 116, facing: 3, x: 62, y: 0, z: 134, variantSelector: 0 },
  { slot: 13, bank: 57, animationKey: 101, facing: 1, x: -4, y: 0, z: 96, variantSelector: 0 },
  { slot: 14, bank: 57, animationKey: 102, facing: 1, x: 27, y: 0, z: 96, variantSelector: 0 },
  { slot: 15, bank: 57, animationKey: 103, facing: 1, x: 59, y: 0, z: 96, variantSelector: 0 }
]);

// These tables are byte-exact static resource selectors. The launch pre-scan in
// func_0004ED60 reads structural opcode 0x80000006 before the Director VM starts.
// func_00067FA8 routes terminal classes 4 and 5 through the 31-entry scene-group
// table and preloads its first member. Other terminal classes can seed the
// independent mode-2 environment selector. The runtime command handler tests
// scene mode 2: mode 2 copies the environment selector into foreground storage,
// while non-mode-2 contexts expand the 31-entry scene group.
const SCENE_BACKGROUND_GROUP_KEYS = Object.freeze([
  0x016CD5E2, 0x016CF8FE, 0x016D1B0A, 0x016D3C90, 0x01702ACC,
  0x0173153A, 0x0175DE04, 0x017627A0, 0x0177C0F2, 0x017AB50E,
  0x017F4B24, 0x0180114A, 0x0181A0E6, 0x018207BC, 0x018585D2,
  0x018585E2, 0x018585F6, 0x01858606, 0x0185A21C, 0x0185B2A2,
  0x0185C31A, 0x01876580, 0x018854DC, 0x018854F4, 0x018854FC,
  0x01885504, 0x0188550C, 0x01885514, 0x0188551C, 0x01885524,
  0x0188552C
]);

// Every nonzero member is retained in its native traversal ordinal. The
// scene-resource rows are ordinary ROM resources outside the LHA catalog.
const SCENE_GROUP_ASSET_ROWS = Object.freeze([
  [0, 0, 'archive:94'], [0, 3, 'scene-resource:016C90D4'],
  [0, 4, 'scene-resource:016CA720'], [0, 5, 'scene-resource:016CBCDE'],
  [0, 6, 'scene-resource:016CC968'],
  [1, 0, 'archive:94'], [1, 1, 'scene-resource:016CD602'],
  [1, 3, 'scene-resource:016C90D4'], [1, 4, 'scene-resource:016CA720'],
  [1, 5, 'scene-resource:016CBCDE'], [1, 6, 'scene-resource:016CC968'],
  [2, 0, 'archive:94'], [2, 1, 'scene-resource:016CF91E'],
  [2, 3, 'scene-resource:016C90D4'], [2, 4, 'scene-resource:016CA720'],
  [2, 5, 'scene-resource:016CBCDE'], [2, 6, 'scene-resource:016CC968'],
  [3, 0, 'archive:94'], [3, 1, 'scene-resource:016D1B2A'],
  [3, 3, 'scene-resource:016C90D4'], [3, 4, 'scene-resource:016CA720'],
  [3, 5, 'scene-resource:016CBCDE'], [3, 6, 'scene-resource:016CC968'],
  [4, 0, 'archive:95'], [4, 1, 'archive:96'], [4, 2, 'scene-resource:016FEB04'],
  [5, 0, 'archive:97'], [5, 1, 'archive:98'], [5, 2, 'scene-resource:0172D0F6'],
  [6, 0, 'archive:99'], [6, 1, 'scene-resource:01753F10'],
  [6, 2, 'scene-resource:01757C4E'], [6, 3, 'scene-resource:01759D2C'],
  [7, 0, 'archive:9'], [7, 1, 'scene-resource:0175DE18'],
  [8, 0, 'archive:100'], [8, 1, 'scene-resource:0177985C'],
  [9, 0, 'archive:101'], [9, 1, 'scene-resource:01780064'],
  [9, 2, 'scene-resource:017A1F72'], [9, 3, 'scene-resource:017A3502'],
  [10, 0, 'archive:102'], [10, 1, 'scene-resource:017ED44E'],
  [11, 0, 'scene-resource:017F4B30'], [11, 3, 'scene-resource:017FD27C'],
  [12, 0, 'archive:108'],
  [13, 0, 'archive:99'], [13, 1, 'scene-resource:0181A0F6'],
  [13, 2, 'scene-resource:0181D3AE'], [13, 3, 'scene-resource:01759D2C'],
  [14, 0, 'scene-resource:018207D0'],
  [15, 0, 'archive:97'], [15, 2, 'archive:98'], [15, 3, 'scene-resource:0172D0F6'],
  [16, 0, 'archive:100'], [16, 2, 'scene-resource:0177985C'],
  [17, 0, 'archive:0'], [17, 2, 'archive:40'],
  [18, 0, 'archive:7'], [18, 1, 'archive:109'], [18, 3, 'archive:110'],
  [19, 0, 'archive:7'], [19, 1, 'archive:111'], [19, 3, 'archive:110'],
  [20, 0, 'archive:7'], [20, 1, 'archive:112'], [20, 3, 'archive:110'],
  [21, 0, 'archive:113'], [21, 1, 'archive:114'],
  [22, 0, 'archive:115'], [22, 1, 'archive:116'], [22, 2, 'archive:117'],
  [22, 3, 'archive:118'], [22, 4, 'archive:119']
]);

// key, size word, stored start/end, stored hash, compression, decoded length,
// decoded hash, B5 format/count/reference/composite dimensions.
const SCENE_GROUP_RESOURCE_ROWS = Object.freeze([
  [0x016C90D4,0x01C5D354,0x01C5D358,0x01C5E9A0,'B1A5002D2936E5E6AD8DFAFE16517194B08FBAC3260BBA112333B3C1668F82E9','ob64-custom-lz',30008,'ADE7425143D7F9823623E96DD009116AF02E86C8E6703E695F89C17EE4762EFD',0,6,200,150,161,96],
  [0x016CA720,0x01C5E9A0,0x01C5E9A4,0x01C5FF5E,'36AB772A590875B1FC58ADEB436B55183EA64346F537DFFB54BED76527DDEAC8','ob64-custom-lz',25464,'37476F30F1C69BFB9299A124F5E6DCC23B24920ECDBEAB1FECEFDA4585F38E7D',0,4,200,150,64,176],
  [0x016CBCDE,0x01C5FF5E,0x01C5FF62,0x01C60BE8,'8CC607EF82A3B8D51D86CC7BC60633F07175F57B8C01E348BAE608BA86FDFC3E','ob64-custom-lz',13240,'98484D434444EFAFCD4E3C0BA9675BC9EE1D5D4BED872F5EBBFD7A0F2288DD45',0,3,200,150,64,76],
  [0x016CC968,0x01C60BE8,0x01C60BEC,0x01C61861,'C2F13B1FE9B0E70667F77BA0DAD2C8A9EF8A2B87A33CB537C8519C2B87CC7E7D','ob64-custom-lz',11816,'7638CC6D7F2E3A336F8F4A6D2E80114F9876DD1613047B73759F3C33B655F0F4',0,2,200,150,48,80],
  [0x016CD602,0x01C61882,0x01C61886,0x01C63B7D,'37854E3B36BB5646DA81D3FD79E3CBC97601333C74813BB485F8EE5867A3B60B','ob64-custom-lz',32632,'181C5CF39AD8B3C983A2D234C25BD4901095C4A6A9348C8FC9597C0DF997DBCF',0,4,200,150,96,128],
  [0x016CF91E,0x01C63B9E,0x01C63BA2,0x01C65D8A,'40E5114867A13BF5B516AFA2E5C17F281FE05F9F9E4C88EB927D68D440AF4DE8','ob64-custom-lz',32632,'4FA24167576D309443FBCBBB5DD60C9442DE5FFB38ED13848B34A295C450E9B6',0,4,200,150,96,128],
  [0x016D1B2A,0x01C65DAA,0x01C65DAE,0x01C67F10,'C3157C4445C8F428907333B38E7FF7AD39B5E2B0512D6518BA202324B682E61F','ob64-custom-lz',32632,'FF75E1B97A77929085CCCDF6762C52D4BEF2D0B3CD58E4219073C51EB458C545',0,4,200,150,96,128],
  [0x016FEB04,0x01C92D84,0x01C92D88,0x01C96D4B,'70AD2F5C40BDF82DFC2502E3EE7B5044E4D183BAE07EAC0541DE4876EB75BA82','ob64-custom-lz',74856,'16839C0685FD055EBC63D227D24F3EE226334718AAF3D1A74AF19BEC2C636B48',0,10,250,187,224,167],
  [0x0172D0F6,0x01CC1376,0x01CC137A,0x01CC57BA,'6522F092018FCBF26C18224EB57B4A0D0ACE6A7E5E4D1AFBF8E7CE75896BB04D','ob64-custom-lz',62528,'976FDF11846F5BA721395093C649B96C1BD394F92B5CD9FF14BE354A20BC425A',0,16,250,187,153,209],
  [0x01753F10,0x01CE8190,0x01CE8194,0x01CEBECE,'1FCDEB0FA10F1E2EF6FB68008182F1B94DF833DE3C8276D9D835E273C164A32C','ob64-custom-lz',52744,'A91B51E58E41B405EEF002692FAA703A9BB2F6E220EE2D393EF821A6282FD6EF',0,3,250,187,128,192],
  [0x01757C4E,0x01CEBECE,0x01CEBED2,0x01CEDFAB,'DD1105DE18B135AECFBA172ABFA21EAF347A9FD4B8A49303D7778C331DC0F4D7','ob64-custom-lz',44056,'E800E98CCD66EE08CCDC700A7A708E60A333DBDBB7D7D051115238BA2A16B521',0,6,250,187,208,112],
  [0x01759D2C,0x01CEDFAC,0x01CEDFB0,0x01CF2084,'8B8D7E1F15F97B9ED78A3CA4CE4BB27D119EF11C3BE57A3AE3939D032F08AAF8','ob64-custom-lz',53904,'6EE0E98527868595AC4510C21080999435C2544B11F451B0EA5E2A861412DC4F',0,11,250,187,372,342],
  [0x0175DE18,0x01CF2098,0x01CF209C,0x01CF6A20,'F83232312491321DB7D72078724A717E2042927CFBA66F309524A0B58D2BE329','ob64-custom-lz',68472,'B6CE0C57B4EA5A9D31FF5868DBA8FDD15DEE418C97CEB2B0BCFD090ECEB13CD1',0,6,200,150,193,145],
  [0x0177985C,0x01D0DADC,0x01D0DAE0,0x01D10371,'76ACEEF111FE78488316DDD1D43BAF806EB09124E83115ECF403BDE340FEA0C0','ob64-custom-lz',41496,'154BAAF30AE91541E45A8FCD33D0DF2B69C83680E58829BB625618A866E0AB98',0,1,202,152,96,144],
  [0x01780064,0x01D142E4,0x01D142E8,0x01D361F1,'1F2B2561ECFE6E7511506AC9E7FE64F1911F70B20BBF1F40FC9287E7E2BF3C9D','ob64-custom-lz',450920,'04812F9D7DCE015F1D8CD17E513165B7CA117CC75778A32D858083D0B0AC6021',0,15,292,248,464,400],
  [0x017A1F72,0x01D361F2,0x01D361F6,0x01D37782,'14006FF4A3A9DFB9A0EF6B141AE84F569D78D6FB11E19F4CF5453D8AAD2E1CE0','ob64-custom-lz',19248,'A2F3E22B70B7582942B61E8D9FA6EE136642642F31C746208DF69DCC2FE01767',0,1,292,248,69,89],
  [0x017A3502,0x01D37782,0x01D37786,0x01D3F78E,'E2B03CF0D4F43C8AE37C8D782212AE7D39AFBA46F890C60C2AAA83A8B78C0383','ob64-custom-lz',97000,'B23AA0861E3A23DA4D1A360C78A91EF677D4F74B224D4A07CDF651A06A7C9107',0,14,292,248,400,160],
  [0x017ED44E,0x01D816CE,0x01D816D2,0x01D88DA4,'21D15158DB7D4616750B40C2B75DC33EEF87262B457AF00F95161A557380B73E','ob64-custom-lz',127496,'F497DD85E244893512334624D2D21F0363CE2B86947DB5D6BF26AA06E9A4B263',0,3,360,320,225,273],
  [0x017F4B30,0x01D88DB0,0x01D88DB4,0x01D914FC,'E4DA5CEEC2E581FB201124FDA2F21CDA172BEBC17F5213A1F9E83E9113D43DE0','none',34632,'E4DA5CEEC2E581FB201124FDA2F21CDA172BEBC17F5213A1F9E83E9113D43DE0',3,4,160,128,447,335],
  [0x017FD27C,0x01D914FC,0x01D91500,0x01D953C9,'3A64106C82D56561181A8FFD646A3372A2C4C029B8B3D6A8A77EFB30A43547B2','ob64-custom-lz',96824,'BDDC8C1BCBD1E769FE7AE4D188E962ECBBEAEA26A16B71FB0115AE7814B94DF9',0,3,160,128,192,192],
  [0x0181A0F6,0x01DAE376,0x01DAE37A,0x01DB162E,'D642DCC560463FB1FC49542CC2B14278682075A34B5341949D3F130BDB0870F2','ob64-custom-lz',33352,'AAA2E4C08C95CD418631C58B98004B2880BC065B791C2DE6DE2304949C43ED37',0,6,250,187,112,144],
  [0x0181D3AE,0x01DB162E,0x01DB1632,0x01DB4A3B,'2EDAB281DE3B8E23DFE3883BF0A66783E08F848C643AFA7AAE40FB2B91F37E87','ob64-custom-lz',59816,'41138BDD6B4D0BB6BC8CE5D61B1E881C5A697040BD74E0183410B8D7208271EE',0,10,250,187,256,128],
  [0x018207D0,0x01DB4A50,0x01DB4A54,0x01DEC852,'FC314BBC57986CFE715BAC4119A9AB29C54E11646DFDD19E95BFAF9A1F30112F','ob64-custom-lz',368880,'AC2695129354C04D8B10B582954FF874A327540238C8AA81BBD725E612BD32DF',1,4,240,190,480,380]
]);

// Resource 4 is the mode-2 environment-base selector table. It is independent
// from resource 0x002938C8, which supplies foreground and occlusion pieces.
const MODE_TWO_ENVIRONMENT_RESOURCE_KEYS = Object.freeze([
  0x00000148, 0x000063DC, 0x0000CBB4, 0x000135F0, 0x00019FF0,
  0x0001EE64, 0x0002507C, 0x0002A5C4, 0x0002F76C, 0x000353CC,
  0x0003AACC, 0x000404A0, 0x00046444, 0x0004B12C, 0x00050378,
  0x00055F10, 0x0005B574, 0x00060E30, 0x0006672C, 0x0006D2C4,
  0x00073BF0, 0x00079B24, 0x0007F310, 0x00083F68, 0x00088E14,
  0x0008E548, 0x000935D4, 0x00098294, 0x0009C938, 0x000A2500,
  0x000B5DC8, 0x000C9A42, 0x000C9A42, 0x000C9A42, 0x000D62F6,
  0x000DCC46, 0x000EECD0, 0x000F5E8C, 0x0010595A, 0x0011C530,
  0x00130C34, 0x0013D472, 0x000EECD0, 0x0014DE06, 0x001550AE,
  0x0010595A, 0x0009C938, 0x0016804E, 0x001760F2, 0x00183352,
  0x00188E36, 0x00197A72, 0x001C03A0, 0x001CAFC4, 0x001FCD04,
  0x00208510, 0x00214344, 0x00236B58, 0x0024714C, 0x00254AA8,
  0x0025DEB4, 0x0026CA20, 0x0027C5F4,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
]);

// Direct resources not already represented by the 29 Section C entries.
// key, size word, stored start/end, hash, container, dimensions, B5 metadata.
const MODE_TWO_DIRECT_ENVIRONMENT_ROWS = Object.freeze([
  [0x000D62F6,0x0066A576,0x0066A57A,0x00670EC6,'2B37F297579F5672CA698F94F3CA8400CD1367F499A55264BC41238A60F00394','embedded-njpg',320,240,null,null,null,null],
  [0x000EECD0,0x00682F50,0x00682F54,0x0068A10C,'5ACA9EFABBFD36629D7B7AF48564B4B19641D812AD67446CD4388EB4154D6B43','embedded-njpg',320,240,null,null,null,null],
  [0x0014DE06,0x006E2086,0x006E208A,0x006E932E,'B6010CCA76276263CDDD27266BFF08D7C8B79A0A0B5051A01ED0159822A768D4','embedded-njpg',320,240,null,null,null,null],
  [0x00183352,0x007175D2,0x007175D6,0x0071D0B6,'D158FDCDAE40B9C18E94E4FBC758D5E8453971FB42626BF1089BDDA0CA8E7785','embedded-njpg',320,240,null,null,null,null],
  [0x00188E36,0x0071D0B6,0x0071D0BA,0x0072BCF2,'17CE7A2BBF1E1D88322C38A9B4794FEC8AA91EEA75C345FC08506322E3D542D1','bg2',495,447,3,4,277,256],
  [0x001C03A0,0x00754620,0x00754624,0x0075F244,'135ADA8B8B6DAEDFB28D39FE7532218BFA25F592F7818BA96C7B00828373054F','bg2',399,415,3,4,283,240],
  [0x001FCD04,0x00790F84,0x00790F88,0x0079C790,'DFDF1F5F157585CA243EFCA60B5B709F8470DE24B72ADA4A2F36B751CC2310EE','bg2',399,399,3,4,283,240],
  [0x00208510,0x0079C790,0x0079C794,0x007A85C4,'2363FAF16C8021CAD0162096BB58CB9E2AFAAAE46C4A6C2C322E698A8E8F9584','bg2',479,399,3,4,283,240],
  [0x00236B58,0x007CADD8,0x007CADDC,0x007DB3CC,'2E88AD33D898DC2C127913424312F15A99501C8C671DE0FCD23837E436FC0A9C','bg2',495,383,3,4,277,235],
  [0x0024714C,0x007DB3CC,0x007DB3D0,0x007E8D28,'CBE2099008CE49AA7E0699F4B44A7D00AE31FF258D5324F426511EEA2BD75C85','bg2',511,335,3,4,214,180],
  [0x00254AA8,0x007E8D28,0x007E8D2C,0x007F2134,'F11008B535B3F1AD66D2E402D6A2F5634F0BD104261F8FF1FF7DAC1A7832AFB7','bg2',463,351,3,4,220,165],
  [0x0026CA20,0x00800CA0,0x00800CA4,0x00810874,'32BF1F7A8A22CF24427E88D97B53C001A6B4C40D4E961637A58967EC36E074BE','bg2',336,655,3,3,160,320],
  [0x0027C5F4,0x00810874,0x00810878,0x00827B48,'63D89B012CC29F5DA7615A72720F8ABD20CE68057CB440927BAA8958CA97AA7E','bg2',422,556,3,6,208,272]
]);

const MODE_TWO_ENVIRONMENT_ASSET_IDS = Object.freeze([
  ...Array.from({ length: 29 }, (_, index) =>
    'section-c-njpg:' + String(index).padStart(2, '0')),
  'archive:0', 'archive:1', 'archive:2', 'archive:2', 'archive:2',
  'mode2-environment:000D62F6', 'archive:3', 'mode2-environment:000EECD0',
  'archive:4', 'archive:5', 'archive:6', 'archive:7', 'archive:8',
  'mode2-environment:000EECD0', 'mode2-environment:0014DE06', 'archive:9',
  'archive:5', 'section-c-njpg:28', 'archive:10', 'archive:11',
  'mode2-environment:00183352', 'mode2-environment:00188E36', 'archive:12',
  'mode2-environment:001C03A0', 'archive:13', 'mode2-environment:001FCD04',
  'mode2-environment:00208510', 'archive:14', 'mode2-environment:00236B58',
  'mode2-environment:0024714C', 'mode2-environment:00254AA8', 'archive:15',
  'mode2-environment:0026CA20', 'mode2-environment:0027C5F4',
  ...Array.from({ length: 17 }, () => null)
]);

const MODE_TWO_OVERLAY_RESOURCE_KEYS = Object.freeze([
  0x00293A0C, 0, 0, 0, 0x002946BE, 0x00298F9E, 0x0029BCFC, 0x0029E2F6,
  0x002A0A6E, 0, 0x002A1CD4, 0x002A37EE, 0x002A49DE, 0x002A77DC,
  0x002AD4EA, 0x002B20C8, 0x002B7784, 0x002B9CFE, 0x002BBC5A, 0x002C00D8,
  0x002C3EF6, 0x002C8638, 0x002CB956, 0, 0x002CFB88, 0x002D4B88,
  0x002D80C6, 0x002DC992, 0x002DDA64, 0, 0x002E2A3C, 0x002E718C,
  0x002E718C, 0x002E718C, 0, 0, 0, 0x002E8E2C, 0, 0,
  0, 0x002EB82C, 0, 0, 0, 0, 0x002DDA64, 0,
  0, 0, 0x002EF2B4, 0x002F1E60, 0x002F6E2C, 0x002FA1FC, 0x002FF76E, 0,
  0, 0x00304464, 0x00308E0C, 0, 0, 0, 0x0030DFBC, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
]);

const BATTLE_BACKGROUND_ARCHIVE_ROWS = Object.freeze({
  0: [16], 4: [17], 5: [18], 6: [19], 7: [20], 8: [21],
  10: [22], 11: [23], 12: [24], 13: [25], 14: [26], 15: [27],
  16: [28], 17: [29], 18: [30], 19: [31], 20: [32], 21: [33],
  22: [34], 24: [35], 25: [36], 26: [37], 27: [38], 28: [39],
  30: [40], 31: [41], 32: [41], 33: [41], 37: [42], 41: [43],
  46: [39], 50: [44], 51: [45], 52: [46], 53: [47], 54: [48],
  57: [49], 58: [50], 62: [51]
});

function modeTwoEnvironmentAssetId(selector) {
  return Number.isInteger(selector) && selector >= 0 &&
    selector < MODE_TWO_ENVIRONMENT_ASSET_IDS.length
    ? MODE_TWO_ENVIRONMENT_ASSET_IDS[selector] : null;
}

function modeTwoStageLayers(environmentSelector, overlaySelector) {
  const layers = [];
  const environmentAssetId = modeTwoEnvironmentAssetId(environmentSelector);
  if (environmentAssetId) {
    layers.push(Object.freeze({
      assetId: environmentAssetId,
      role: 'environment-base',
      depth: 0,
      evidenceStatus: 'native-environment-selector',
      associationStatus: 'resource 4 selects this complete mode-2 environment base'
    }));
  }
  const overlayArchives = Number.isInteger(overlaySelector)
    ? BATTLE_BACKGROUND_ARCHIVE_ROWS[overlaySelector] || [] : [];
  overlayArchives.forEach((archiveIndex, index) => {
    layers.push(Object.freeze({
      assetId: 'archive:' + archiveIndex,
      role: 'foreground-mask',
      depth: 2 + index,
      evidenceStatus: 'native-overlay-selector',
      associationStatus:
        'resource 0x002938C8 selects this foreground or occlusion layer independently of the environment base'
    }));
  });
  return Object.freeze(layers);
}

const MODE_ZERO_ONLY_DIRECTOR_OPCODES = Object.freeze([
  0x08, 0x0D, 0x9B
]);

const MODE_TWO_ONLY_DIRECTOR_OPCODES = Object.freeze([
  0x2A, 0x2B, 0x2C, 0x2D, 0x3D, 0x3E,
  0x92, 0x94, 0x95, 0x96, 0x80000009
]);

function actorCameraCapture(filename, zipSha256, values, result) {
  return Object.freeze({
    mode: 'native-perspective-capture',
    coordinateSpace: 'Director fixed-point coordinates divided by 1000; native Actor model scale is 0.1',
    screenWidth: 320,
    screenHeight: 240,
    modelScale: 0.1,
    fovYDegrees: values.fovYDegrees,
    aspect: 1.3333333333333333,
    near: 1,
    far: 5000,
    eye: Object.freeze({ ...values.eye }),
    target: Object.freeze({ ...values.target }),
    up: Object.freeze({ x: 0, y: 1, z: 0 }),
    calibrationStatus: 'stored-state Actor camera bank',
    calibrationCapture: filename,
    calibrationCaptureSha256: zipSha256,
    calibrationResult: result
  });
}

const COMMON_MODE_TWO_ACTOR_CAMERA = Object.freeze({
  fovYDegrees: 12.880000114440918,
  eye: Object.freeze({
    x: 82.38899993896484,
    y: 41.6349983215332,
    z: 40.12200164794922
  }),
  target: Object.freeze({ x: -1.5, y: 0, z: -1.2699999809265137 })
});

const COMMON_MODE_TWO_ACTOR_PROJECTION = Object.freeze({
  mode: 'native-perspective-runtime',
  coordinateSpace: 'Director fixed-point coordinates divided by 1000; native Actor model scale is 0.1',
  screenWidth: 320,
  screenHeight: 240,
  modelScale: 0.10000000149011612,
  fovYDegrees: COMMON_MODE_TWO_ACTOR_CAMERA.fovYDegrees,
  aspect: 1.3333333333333333,
  near: 1,
  far: 5000,
  eye: COMMON_MODE_TWO_ACTOR_CAMERA.eye,
  target: COMMON_MODE_TWO_ACTOR_CAMERA.target,
  up: Object.freeze({ x: 0, y: 1, z: 0 }),
  evidenceStatus: 'native-static',
  calibrationStatus: 'validated mode-two overlay camera constants',
  calibrationResult: 'The immutable overlay initializer supplies the exact common mode-two Actor camera before Director execution.',
  sourceZ64Range: '0x00211D5C..0x00211D7C',
  sourceRamRange: '0x801CE8CC..0x801CE8E8'
});

const PROLOGUE_MODE_TWO_ACTOR_CAMERA = Object.freeze({
  fovYDegrees: 35,
  eye: Object.freeze({ x: 0, y: -120, z: 150 }),
  target: Object.freeze({ x: 0, y: -120, z: 0 })
});

function nativeStagePropSource(values) {
  return Object.freeze({
    bank: values.bank,
    descriptorKey: values.descriptorKey,
    descriptorMemberCount: values.descriptorMemberCount,
    metadataKey: values.metadataKey,
    poseKey: values.poseKey,
    configKey: values.configKey,
    lookupKey: values.lookupKey,
    metadataDecodedLength: values.metadataDecodedLength,
    poseDecodedLength: values.poseDecodedLength,
    configDecodedLength: values.configDecodedLength,
    lookupDecodedLength: values.lookupDecodedLength,
    artCount: values.artCount,
    lookupBankCount: values.lookupBankCount,
    selectedChildOrdinal: 0
  });
}

function orthographicStageProp(id, poseSelector, x, y, z) {
  return Object.freeze({
    id,
    projection: 'native-320x240-orthographic',
    poseSelector,
    x,
    y,
    z,
    depthPass: z <= -500 ? 'far' : 'near'
  });
}

function perspectiveStageProp(id, poseSelector, x, y, z) {
  return Object.freeze({
    id,
    projection: 'native-actor-perspective',
    poseSelector,
    x,
    y,
    z,
    depthPass: 'perspective'
  });
}

const FORMATION_NATIVE_STAGE_PROPS = Object.freeze({
  coordinateSpace: 'centered 320x240 orthographic pixels plus native Actor-camera objects',
  placementResourceKey: 0x00315736,
  orthographicTableHeaderEntry: 9,
  perspectiveTableHeaderEntry: 10,
  source: nativeStagePropSource({
    bank: 'mode2-stage:00FEA716',
    descriptorKey: 0x00FEA716,
    descriptorMemberCount: 21,
    metadataKey: 0x00FE4E56,
    poseKey: 0x00FE50D2,
    configKey: 0x00FE5150,
    lookupKey: 0x00FE5178,
    metadataDecodedLength: 2150,
    poseDecodedLength: 143,
    configDecodedLength: 72,
    lookupDecodedLength: 2560,
    artCount: 17,
    lookupBankCount: 5
  }),
  orthographicPlacements: Object.freeze([
    orthographicStageProp('formation:prop:0', 0, 73, 55, -6),
    orthographicStageProp('formation:prop:1', 1, 134, 23, -6),
    orthographicStageProp('formation:prop:2', 2, -122, 15, -906),
    orthographicStageProp('formation:prop:3', 3, -81, -31, -906),
    orthographicStageProp('formation:prop:4', 4, -34, -91, -906),
    orthographicStageProp('formation:prop:5', 5, 8, 107, -6),
    orthographicStageProp('formation:prop:6', 5, -207, 113, -6),
    orthographicStageProp('formation:prop:7', 6, -174, 73, -6),
    orthographicStageProp('formation:prop:8', 6, -54, 138, -6)
  ]),
  perspectivePlacements: Object.freeze([
    perspectiveStageProp('formation:object:0', 7, -120, 0, 95)
  ]),
  evidenceStatus: 'static native loader, table decoder, builder, projection, and draw-path trace'
});

const GRADUATION_NATIVE_STAGE_PROPS = Object.freeze({
  coordinateSpace: 'centered 320x240 orthographic pixels plus native Actor-camera objects',
  placementResourceKey: 0x00315736,
  orthographicTableHeaderEntry: 9,
  perspectiveTableHeaderEntry: 10,
  source: nativeStagePropSource({
    bank: 'mode2-stage:01004020',
    descriptorKey: 0x01004020,
    descriptorMemberCount: 23,
    metadataKey: 0x01000366,
    poseKey: 0x0100069C,
    configKey: 0x01000770,
    lookupKey: 0x01000798,
    metadataDecodedLength: 2196,
    poseDecodedLength: 271,
    configDecodedLength: 72,
    lookupDecodedLength: 2048,
    artCount: 19,
    lookupBankCount: 4
  }),
  orthographicPlacements: Object.freeze([
    orthographicStageProp('graduation:prop:0', 0, -5, 89, -6),
    orthographicStageProp('graduation:prop:1', 1, 68, 86, -6),
    orthographicStageProp('graduation:prop:2', 2, 173, 39, -6),
    orthographicStageProp('graduation:prop:3', 3, 192, -6, -6),
    orthographicStageProp('graduation:prop:4', 4, -136, 71, -6),
    orthographicStageProp('graduation:prop:5', 5, -19, -90, -906),
    orthographicStageProp('graduation:prop:6', 6, 16, 85, -6),
    orthographicStageProp('graduation:prop:7', 7, 161, 37, -6),
    orthographicStageProp('graduation:prop:8', 9, -65, 157, -6),
    orthographicStageProp('graduation:prop:9', 10, -104, 152, -6),
    orthographicStageProp('graduation:prop:10', 11, -153, 148, -6)
  ]),
  perspectivePlacements: Object.freeze([
    perspectiveStageProp('graduation:object:0', 8, -100, 0, 90)
  ]),
  evidenceStatus: 'static native loader, table decoder, builder, projection, and draw-path trace'
});

const DIRECTOR_ACTOR_CAMERA_RUNTIME_OBSERVATIONS = Object.freeze({
  'rom-custom-lz:01F3EAD2': actorCameraCapture(
    'Starting choices cutscene Magnus just walking up.pj.zip',
    'C64929322C81ADCF6348BC41AC660E54B46CE6A6911818A316E09F0C2BC91E3D',
    COMMON_MODE_TWO_ACTOR_CAMERA,
    'The stored mode-two Actor camera reproduces all eight Graduation actor anchors.'),
  'rom-custom-lz:01F3F242': actorCameraCapture(
    'Formation Cuutscene.pj.zip',
    'FD7AC846DE29C2EAA8EFD0E2A9A43406E56461AE2F54C0C717B959E1717698D2',
    COMMON_MODE_TWO_ACTOR_CAMERA,
    'The Formation capture contains the same complete mode-two Actor camera bank.'),
  'rom-custom-lz:01FA4D0A': actorCameraCapture(
    'Traveling Cutscene 1.pj.zip',
    'E99336C11267D54D709515E4C24A7D3FC470B74B197838D2CCEBD24A20473235',
    COMMON_MODE_TWO_ACTOR_CAMERA,
    'The Traveling Cutscene 1 capture contains the complete mode-two Actor camera bank.'),
  'rom-custom-lz:01FA4EE4': actorCameraCapture(
    'Traveling Cutscene 2.pj.zip',
    'A7336D97D2E46E0ACE1B537F8C9FE91B830E8BDBDFA4976BD2749B6CE4FC2A96',
    COMMON_MODE_TWO_ACTOR_CAMERA,
    'The Traveling Cutscene 2 capture contains the complete mode-two Actor camera bank.'),
  'rom-custom-lz:01FA5100': actorCameraCapture(
    'Traveling Cutscene 4.pj.zip',
    '46D357300D50692FFFB69074530C57B4AFA49FC801C122A335438DAC4DF807C8',
    COMMON_MODE_TWO_ACTOR_CAMERA,
    'The Traveling Cutscene 4 capture contains the complete mode-two Actor camera bank.'),
  'rom-custom-lz:01FAA4DA': actorCameraCapture(
    'Prologue Title.pj.zip',
    '61E27E35ABAB7502493535086C2B6F5856F6587A12B0B1BC4519C6043EC62ECC',
    PROLOGUE_MODE_TWO_ACTOR_CAMERA,
    'The Prologue Title capture contains its distinct complete mode-two Actor camera bank.')
});

// Stored Project64 states close the independent runtime selectors for these scene loads.
// Scene-group resources load every member in ascending ordinal order. Final
// depth and blend semantics remain unresolved for multi-member groups.
const DIRECTOR_BACKGROUND_RUNTIME_OBSERVATIONS = Object.freeze({
  'rom-custom-lz:01F3E836': Object.freeze({
    captures: Object.freeze([Object.freeze({
      title: 'First Cutscene Flashback',
      filename: '1st cutscene flashback.pj.zip',
      zipSha256: '1EBD65ED9FB44D4DE12AA7345823997328FF51A957060E5EB802007FC6703C33'
    })]),
    directorMode: 0,
    commandOperand: 10,
    selectorTableId: 'background-table:scene:31',
    candidateGroupIds: Object.freeze(['scene-resource-group:017F4B24']),
    exactAssetIds: Object.freeze(['archive:102']),
    candidateAssetIds: Object.freeze(['archive:102']),
    stageProjection: Object.freeze({
      mode: 'stage-fit',
      scale: 1,
      viewport: Object.freeze({ left: 0, top: 23, width: 320, height: 191 }),
      initialPreviewFrame: 210,
      calibrationStatus: 'stored-framebuffer viewport registration',
      calibrationCapture: '1st cutscene flashback.pj.zip',
      calibrationResult: 'The assembled courtyard uses the native 320×191 cutscene viewport.'
    }),
    associationStatus: 'runtime-observed mode-0 selector 10 reaches exact B5 archive #102'
  }),
  'rom-custom-lz:01F3EAD2': Object.freeze({
    captures: Object.freeze([
      Object.freeze({
        stageId: 'graduation-title-card',
        stageLabel: 'Opening ceremony title card',
        filename: 'Loading  Magnus walk opening ceremony cutscene.pj.zip',
        zipSha256: '7A4D568FCB3D842E35087272300C29EC3DFE64AF456965C3B070E7DF7E57D59D'
      }),
      Object.freeze({
        stageId: 'graduation-walk-up',
        stageLabel: 'Magnus walking up',
        previewFrame: 567,
        previewTimingStatus: 'corrected Director scheduler: camera transition starts at tick 489, movement starts at tick 519, and the stored capture follows 48 movement updates',
        filename: 'Starting choices cutscene Magnus just walking up.pj.zip',
        zipSha256: 'C64929322C81ADCF6348BC41AC660E54B46CE6A6911818A316E09F0C2BC91E3D'
      }),
      Object.freeze({
        stageId: 'graduation-dialogue',
        stageLabel: 'Starting choices dialogue',
        filename: 'Starting choices cutscene.pj.zip',
        zipSha256: 'D6FE188717D7DF5B47E926EF0BE0F4563A4CF957D956B6F43C07DD65E8D37A44',
        dialogueAssociation: Object.freeze({
          entryId: 'serifu:159:0',
          archiveSelector: 1,
          entrySelector: 0,
          speaker: 'Archbishop Odiron',
          capability: 'preview-only',
          evidenceStatus: 'unique-stored-capture-text-match',
          status: 'The stored dialogue capture uniquely matches Serifu archive selector 1, entry selector 0. The upstream presentation writer remains unresolved.',
          timelineStatus: 'The exact Director Timeline frame remains unresolved. This capture association is metadata only and does not compile into Director commands.'
        })
      })
    ]),
    directorMode: 2,
    commandOperand: null,
    environmentSelector: 57,
    foregroundSelector: 57,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze(['mode2-environment:00236B58', 'archive:49']),
    candidateAssetIds: Object.freeze(['mode2-environment:00236B58', 'archive:49']),
    stageLayers: modeTwoStageLayers(57, 57),
    stageProjection: Object.freeze({
      mode: 'b5-reference-capture',
      coordinateSpace: 'B5 origin plus reference',
      cropWorldX: 114,
      cropWorldY: 115,
      viewport: Object.freeze({ left: 0, top: 23, width: 320, height: 191 }),
      scale: 1,
      nativeSceneProps: GRADUATION_NATIVE_STAGE_PROPS,
      initialPreviewFrame: 567,
      calibrationStatus: 'capture-calibrated',
      calibrationCapture: 'Starting choices cutscene Magnus just walking up.pj.zip',
      calibrationTimeline: Object.freeze({
        previewFrame: 567,
        openingWaitTicks: 30,
        cameraStartFrame: 489,
        movementStartFrame: 519,
        cameraElapsedTicks: 78,
        movementElapsedTicks: 48,
        timingStatus: 'corrected scheduler state; preview frames retain native updater-tick units'
      }),
      calibrationCamera: Object.freeze({
        previewFrame: 567,
        translateX: -33.20011901855469,
        translateY: 9.200019836425781,
        scaleX: 1,
        scaleY: 1,
        countdown: 72
      }),
      actorProjection: Object.freeze({
        mode: 'native-perspective-capture',
        coordinateSpace: 'Director fixed-point coordinates divided by 1000; native Actor model scale is 0.1',
        screenWidth: 320,
        screenHeight: 240,
        modelScale: 0.1,
        fovYDegrees: 12.880000114440918,
        aspect: 1.3333333333333333,
        near: 1,
        far: 4000,
        eye: Object.freeze({
          x: 82.38899993896484,
          y: 41.6349983215332,
          z: 40.12200164794922
        }),
        target: Object.freeze({ x: -1.5, y: 0, z: -1.2699999809265137 }),
        up: Object.freeze({ x: 0, y: 1, z: 0 }),
        calibrationStatus: 'native matrix path with stored-state inputs',
        calibrationResult: 'The native perspective, look-at, actor model scale, and Stage camera reproduce all eight stored actor anchors in the Graduation capture.'
      }),
      calibrationResult: 'The environment base and transparent foreground mask share exact 1:1 B5 registration. Native table-driven props add the remaining animated lights and statues.'
    }),
    associationStatus: 'Graduation combines the resource-4 environment at 0x00236B58, independently selected foreground archive #49, and the native mode-2 placement tables'
  }),
  'rom-custom-lz:01F3F242': Object.freeze({
    captures: Object.freeze([
      Object.freeze({
        title: 'Formation Cutscene', filename: 'Formation Cuutscene.pj.zip',
        zipSha256: 'FD7AC846DE29C2EAA8EFD0E2A9A43406E56461AE2F54C0C717B959E1717698D2'
      }),
      Object.freeze({
        title: 'Location Information Title Cutscene',
        filename: 'Location Information Title Cutscene.pj.zip',
        zipSha256: 'A6643090A69757C962DEB421E2A14B256F5F91CB7028D7EB99B360A6EE7093EF'
      }),
      Object.freeze({
        title: 'Formation Cutscene Dialogue Box',
        filename: 'Formation Cutscene Dialogue Box.pj.zip',
        zipSha256: 'F48D3FB91B0355EA61F037EF92A1C7212BF2BDB1AC4F2FBC7B3231307F518B98'
      })
    ]),
    directorMode: 2,
    commandOperand: null,
    environmentSelector: 51,
    foregroundSelector: 51,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze(['archive:12', 'archive:45']),
    candidateAssetIds: Object.freeze(['archive:12', 'archive:45']),
    stageLayers: modeTwoStageLayers(51, 51),
    stageProjection: Object.freeze({
      mode: 'b5-reference-capture',
      coordinateSpace: 'shared B5 origin plus reference coordinates',
      cropWorldX: 95.002,
      cropWorldY: 109.482,
      screenAnchorX: 0,
      screenAnchorY: 0,
      scale: 1,
      nativeSceneProps: FORMATION_NATIVE_STAGE_PROPS,
      viewport: Object.freeze({ left: 0, top: 23, width: 320, height: 191 }),
      initialPreviewFrame: 250,
      calibrationCamera: Object.freeze({
        previewFrame: 250,
        translateX: 25.416658401489258,
        translateY: -31.875,
        scaleX: 1,
        scaleY: 1,
        countdown: 65
      }),
      calibrationStatus: 'stored-framebuffer registration',
      calibrationCapture: 'Formation Cuutscene.pj.zip',
      calibrationResult: 'The castle plate and transparent foreground mask retain native 1:1 B5 registration. Native table-driven props add the remaining animated scene pieces.'
    }),
    associationStatus: 'Formation combines archive #12, selector-owned foreground archive #45, and the native mode-2 placement tables'
  }),
  'rom-custom-lz:01F7FCBA': Object.freeze({
    captures: Object.freeze([Object.freeze({
      title: 'Meeting Hugo', filename: 'Meeting Hugo Cutscene.pj.zip',
      zipSha256: '91A9184CCB1275C175A91DA4D149F78166A6219E377E3B97EAE24677E8616BB2'
    })]),
    directorMode: 0,
    commandOperand: 2,
    selectorTableId: 'background-table:scene:31',
    candidateGroupIds: Object.freeze(['scene-resource-group:016D1B0A']),
    exactAssetIds: Object.freeze(['archive:94']),
    candidateAssetIds: Object.freeze(['archive:94']),
    associationStatus: 'runtime-observed mode-0 selector 2 reaches exact archive #94 kaigi_base.bg2'
  }),
  'rom-custom-lz:01FA4C2C': Object.freeze({
    captures: Object.freeze([Object.freeze({
      title: 'Opening Title', filename: 'Opening Title Cutscene.pj.zip',
      zipSha256: '5BD8259C308C0F260F8BEFA71509989C6FCCEF836A2908C5C62DD70DDF0B9F2C'
    })]),
    directorMode: 0,
    commandOperand: 22,
    selectorTableId: 'background-table:scene:31',
    candidateGroupIds: Object.freeze(['scene-resource-group:018854DC']),
    exactAssetIds: Object.freeze([
      'archive:115', 'archive:116', 'archive:117', 'archive:118', 'archive:119'
    ]),
    candidateAssetIds: Object.freeze([
      'archive:115', 'archive:116', 'archive:117', 'archive:118', 'archive:119'
    ]),
    stageProjection: Object.freeze({
      mode: 'stage-fit',
      scale: 1,
      initialPreviewFrame: 46,
      calibrationStatus: 'native first-transform state',
      calibrationCapture: 'Opening Title Cutscene.pj.zip',
      calibrationResult: 'Preview starts after the first transform applies, with the tapestry offscreen and only the ATLUS card visible.'
    }),
    associationStatus: 'runtime-observed mode-0 selector 22 loads all five members; transform channel 0 controls the tapestry and channels 1 through 4 present one title card at a time'
  }),
  'rom-custom-lz:01FA4D0A': Object.freeze({
    captures: Object.freeze([Object.freeze({
      filename: 'Traveling Cutscene 1.pj.zip',
      zipSha256: 'E99336C11267D54D709515E4C24A7D3FC470B74B197838D2CCEBD24A20473235'
    })]),
    directorMode: 2,
    commandOperand: null,
    environmentSelector: 61,
    foregroundSelector: 61,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze(['mode2-environment:0026CA20']),
    candidateAssetIds: Object.freeze(['mode2-environment:0026CA20']),
    stageLayers: modeTwoStageLayers(61, 61),
    stageProjection: Object.freeze({
      mode: 'stage-fit',
      scale: 1,
      viewport: Object.freeze({ left: 0, top: 23, width: 320, height: 191 }),
      calibrationStatus: 'native resource identity; exact saved-frame crop pending',
      calibrationCapture: 'Traveling Cutscene 1.pj.zip'
    }),
    associationStatus: 'Traveling Cutscene 1 loads its complete environment from resource-4 selector 61'
  }),
  'rom-custom-lz:01FA4EE4': Object.freeze({
    captures: Object.freeze([Object.freeze({
      filename: 'Traveling Cutscene 2.pj.zip',
      zipSha256: 'A7336D97D2E46E0ACE1B537F8C9FE91B830E8BDBDFA4976BD2749B6CE4FC2A96'
    })]),
    directorMode: 2,
    commandOperand: null,
    environmentSelector: 60,
    foregroundSelector: 60,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze(['archive:15']),
    candidateAssetIds: Object.freeze(['archive:15']),
    stageLayers: modeTwoStageLayers(60, 60),
    stageProjection: Object.freeze({
      mode: 'stage-fit',
      coordinateSpace: '320×240 framebuffer pixels',
      scale: 1,
      viewport: Object.freeze({ left: 0, top: 23, width: 320, height: 191 }),
      calibrationStatus: 'stored-framebuffer registration',
      calibrationCapture: 'Traveling Cutscene 2.pj.zip',
      calibrationResult: 'The archive is already a full 320×240 scene image; the native cutscene viewport masks its top and bottom rows.'
    }),
    associationStatus: 'Traveling Cutscene 2 uses archive #15 as its launch-time forest-camp environment even though mode-2 selector row 60 is empty'
  }),
  'rom-custom-lz:01FA4FAE': Object.freeze({
    captures: Object.freeze([Object.freeze({
      title: 'Traveling Cutscene 3', filename: 'Traveling Cutscene 3.pj.zip',
      zipSha256: 'BE8BC2D957A3390A460D79CC23651ABB92007D9A4261D5DD229477AFE273AA97'
    })]),
    directorMode: 0,
    commandOperand: 21,
    selectorTableId: 'background-table:scene:31',
    candidateGroupIds: Object.freeze(['scene-resource-group:01876580']),
    exactAssetIds: Object.freeze(['archive:113', 'archive:114']),
    candidateAssetIds: Object.freeze(['archive:113', 'archive:114']),
    associationStatus: 'runtime-observed mode-0 selector 21 loads both group members in exact ordinal order; final depth and blend semantics remain unresolved'
  }),
  'rom-custom-lz:01FA5100': Object.freeze({
    captures: Object.freeze([Object.freeze({
      filename: 'Traveling Cutscene 4.pj.zip',
      zipSha256: '46D357300D50692FFFB69074530C57B4AFA49FC801C122A335438DAC4DF807C8'
    })]),
    directorMode: 2,
    commandOperand: null,
    environmentSelector: 62,
    foregroundSelector: 62,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze(['mode2-environment:0027C5F4', 'archive:51']),
    candidateAssetIds: Object.freeze(['mode2-environment:0027C5F4', 'archive:51']),
    stageLayers: modeTwoStageLayers(62, 62),
    stageProjection: Object.freeze({
      mode: 'stage-fit',
      scale: 1,
      viewport: Object.freeze({ left: 0, top: 23, width: 320, height: 191 }),
      calibrationStatus: 'native resource identity; exact saved-frame crop pending',
      calibrationCapture: 'Traveling Cutscene 4.pj.zip'
    }),
    associationStatus: 'Traveling Cutscene 4 combines resource-4 environment selector 62 with independently selected foreground archive #51'
  }),
  'rom-custom-lz:01FAA4DA': Object.freeze({
    captures: Object.freeze([Object.freeze({
      filename: 'Prologue Title.pj.zip',
      zipSha256: '61E27E35ABAB7502493535086C2B6F5856F6587A12B0B1BC4519C6043EC62ECC'
    })]),
    directorMode: 2,
    commandOperand: null,
    environmentSelector: 62,
    foregroundSelector: null,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze(['mode2-environment:0027C5F4']),
    candidateAssetIds: Object.freeze(['mode2-environment:0027C5F4']),
    stageLayers: modeTwoStageLayers(62, null),
    stageProjection: Object.freeze({
      mode: 'stage-fit',
      scale: 1,
      viewport: Object.freeze({ left: 0, top: 23, width: 320, height: 191 }),
      calibrationStatus: 'native resource identity; exact saved-frame crop pending',
      calibrationCapture: 'Prologue Title.pj.zip'
    }),
    associationStatus: 'Prologue launch state selects resource-4 environment 62; its independent overlay selector is not active in the stored state'
  }),
  'presentation:briefing-map-overview': Object.freeze({
    captures: Object.freeze([Object.freeze({
      title: 'Scenario 1 Briefing Map Overview',
      filename: 'Briefing Map Overview LOADED.pj.zip',
      zipSha256: '5C24E4C113C810CB4BB91B1D8449510B46B4550DAB328F05AB839A517AC5D5DF'
    })]),
    directorPartialAssetId: 'rom-director:01F3F70A',
    directorMode: 0,
    commandOperand: 2,
    selectorTableId: 'background-table:scene:31',
    candidateGroupIds: Object.freeze(['scene-resource-group:016D1B0A']),
    exactAssetIds: Object.freeze(['archive:94']),
    candidateAssetIds: Object.freeze(['archive:94']),
    associationStatus: 'runtime-observed Director Partial selector 2 reaches exact archive #94 kaigi_base.bg2'
  }),
  'presentation:scenario-1-briefing': Object.freeze({
    captures: Object.freeze([Object.freeze({
      title: 'Scenario 1 Briefing Cutscene',
      filename: 'Scenario 1 Breifing Cutscene.pj.zip',
      zipSha256: '95241462112AC381D1F4144F83F0D13E23CF9ED3F4A417A40BDFE7C4A49348BF'
    })]),
    directorPartialAssetId: 'rom-director:01F3F70A',
    directorMode: 0,
    commandOperand: 2,
    selectorTableId: 'background-table:scene:31',
    candidateGroupIds: Object.freeze(['scene-resource-group:016D1B0A']),
    exactAssetIds: Object.freeze(['archive:94']),
    candidateAssetIds: Object.freeze(['archive:94']),
    associationStatus: 'runtime-observed Director Partial selector 2 reaches exact archive #94 kaigi_base.bg2'
  }),
  'presentation:location-information-scenario-1': Object.freeze({
    captures: Object.freeze([Object.freeze({
      filename: 'Location Information Title Scenario 1.pj.zip',
      zipSha256: '579E4D6CD469153AEE2B2831804ED202ED382804C93C87F18C0240C32DB7BBB3'
    })]),
    directorPartialAssetId: 'rom-director:01FAA540',
    directorMode: 2,
    commandOperand: null,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze([]),
    candidateAssetIds: Object.freeze([]),
    associationStatus: 'the Director Partial prefix has no accepted background request, and the stored state does not prove environment or foreground selector consumption'
  })
});

const REVIEWED_TITLES = Object.freeze({
  '1st-cutscene-flashback': 'First Cutscene Flashback',
  'loading-magnus-walk-opening-ceremony-cutscene': 'Graduation Ceremony',
  'formation-cuutscene': 'Formation Cutscene',
  'meeting-hugo-cutscene': 'Meeting Hugo',
  'opening-title-cutscene': 'Opening Title',
  'traveling-cutscene-1': 'Traveling Cutscene 1',
  'traveling-cutscene-2': 'Traveling Cutscene 2',
  'traveling-cutscene-3': 'Traveling Cutscene 3',
  'traveling-cutscene-4': 'Traveling Cutscene 4',
  'prologue-title': 'Prologue Title'
});

const NON_DIRECTOR_PRESENTATIONS = Object.freeze([
  {
    id: 'map-path-1', title: 'Map Path Cutscene 1', engine: 'map-path',
    runtimeAnchor: 'Map Path Cutscene 1.pj.zip',
    runtimeAnchorSha256: '5C75BD5D9043668B3634D87D242CD95FDD5B7B9937E367A5432E1DCEF5BBA9B3',
    backgroundAssetIds: ['archive:92', 'archive:93'],
    backgroundAssociationStatus:
      'runtime-observed Map Path group contains archive #92 and two exact copies of archive #93; its writer, ROM request key, and compositor remain unresolved'
  },
  {
    id: 'map-path-2', title: 'Map Path Cutscene 2', engine: 'map-path',
    runtimeAnchor: 'Map Path Cutscene 2.pj.zip',
    runtimeAnchorSha256: '6363D97E2A0E891A459A789D40B3C09893869AD62D763013FA9B6311D62FDDB3',
    backgroundAssetIds: ['archive:92', 'archive:93'],
    backgroundAssociationStatus:
      'runtime-observed Map Path group contains archive #92 and two exact copies of archive #93; its writer, ROM request key, and compositor remain unresolved'
  },
  {
    id: 'map-path-3', title: 'Map Path Cutscene 3', engine: 'map-path',
    runtimeAnchor: 'Map Path Cutscene 3.pj.zip',
    runtimeAnchorSha256: '9B87EDE8E349405AB9105DD67FC500FD3FD290AD4C12A8DE12019A7F304294EC',
    backgroundAssetIds: ['archive:92', 'archive:93'],
    backgroundAssociationStatus:
      'runtime-observed Map Path group contains archive #92 and two exact copies of archive #93; its writer, ROM request key, and compositor remain unresolved'
  },
  {
    id: 'title-load', title: 'Title Load', engine: 'title-presentation',
    runtimeAnchor: 'Title Load.pj.zip',
    runtimeAnchorSha256: '9C61BA16B603D92A23B3856852E5B8D1B7C7F85BFC9D7B55D6090F07728665E3',
    backgroundCandidateAssetIds: range(81, 92).concat(range(115, 119)).map((index) => 'archive:' + index)
  },
  {
    id: 'title-menu-loaded', title: 'Title Menu Loaded', engine: 'title-presentation',
    runtimeAnchor: 'Title Menu Loaded.pj.zip',
    runtimeAnchorSha256: 'A7495BA294A0CB92D08454AE87DDDAEC184334E10D529C0C390133CDE50BE12B',
    backgroundCandidateAssetIds: range(81, 92).concat(range(115, 119)).map((index) => 'archive:' + index)
  },
  {
    id: 'briefing-map-overview', title: 'Scenario 1 Briefing Map Overview',
    engine: 'director-partial', runtimeAnchor: 'Briefing Map Overview LOADED',
    runtimeAnchorSha256: '5C24E4C113C810CB4BB91B1D8449510B46B4550DAB328F05AB839A517AC5D5DF',
    partialDirectorResourceId: 'rom-director:01F3F70A',
    actors: SCENARIO_1_BRIEFING_ACTORS
  },
  {
    id: 'location-information-scenario-1', title: 'Location Information · Scenario 1',
    engine: 'director-partial', runtimeAnchor: 'Location Information Title Scenario 1',
    runtimeAnchorSha256: '579E4D6CD469153AEE2B2831804ED202ED382804C93C87F18C0240C32DB7BBB3',
    partialDirectorResourceId: 'rom-director:01FAA540',
    backgroundCandidateAssetIds: ['archive:639', 'archive:642']
  },
  {
    id: 'scenario-1-briefing', title: 'Scenario 1 Briefing Cutscene',
    engine: 'director-partial', runtimeAnchor: 'Scenario 1 Breifing Cutscene',
    runtimeAnchorSha256: '95241462112AC381D1F4144F83F0D13E23CF9ED3F4A417A40BDFE7C4A49348BF',
    partialDirectorResourceId: 'rom-director:01F3F70A',
    actors: SCENARIO_1_BRIEFING_ACTORS,
    dialogueAssociations: [{
      entryId: 'serifu:162:8',
      archiveSelector: 4,
      entrySelector: 8,
      status: 'exact Serifu presentation archive and entry selectors; the Scenario 1 load-time writer for pair (4,8) remains unresolved'
    }]
  },
  {
    id: 'world-map-loading-scenario-1', title: 'World Map Loading · Scenario 1',
    engine: 'scenario-presentation', runtimeAnchor: 'World Map Loading Scenario 1',
    runtimeAnchorSha256: 'B70E37929AF89B32A920C261ECFDAACD9F066683F6DB02C89A5A3F67A61B65C8'
  },
  {
    id: 'staff-credits', title: 'Staff Credits', engine: 'credits-presentation',
    staticDataZ64Start: 0x001F0150, staticDataZ64EndExclusive: 0x001F0930
  }
]);

// The archive generator currently drops non-ASCII filename bytes. These names
// come from the raw LHA filename fields and are presentation metadata only.
const SHIFT_JIS_DISPLAY_NAMES = Object.freeze({
  6: '採掘現場付近.n64',
  51: '城壁上ＢＧ.bg2',
  70: 'まるしー改.n64',
  92: 'オープニングマップ(256色).n64',
  93: 'オープニング_道.n64',
  115: 'OP_ﾀﾍﾟｽﾄﾘｰ.n64'
});

// These headerless layouts come from byte-conserving archive analysis. Rows
// with unresolved segmentation or palettes retain warnings in the product
// catalog and remain Preview only.
const RAW_IMAGE_LAYOUTS = {};

function addRawLayouts(indices, layout) {
  for (const index of indices) RAW_IMAGE_LAYOUTS[index] = Object.freeze({ ...layout });
}

function range(start, end) {
  return Array.from({ length: end - start + 1 }, (_, index) => start + index);
}

addRawLayouts([103], {
  pixelFormat: 'rgba5551', width: 320, height: 160, dataOffset: 16,
  nativeRecordHeader: true, family: 'background-component'
});
addRawLayouts([104], {
  pixelFormat: 'rgba5551', width: 288, height: 240, dataOffset: 16,
  nativeRecordHeader: true, family: 'background-component'
});
addRawLayouts([105], {
  pixelFormat: 'rgba5551', width: 288, height: 160, dataOffset: 16,
  nativeRecordHeader: true, family: 'background-component'
});
addRawLayouts([106], {
  pixelFormat: 'rgba5551', width: 320, height: 128, dataOffset: 16,
  nativeRecordHeader: true, family: 'background-component'
});
addRawLayouts([107], {
  pixelFormat: 'rgba5551', width: 288, height: 128, dataOffset: 16,
  nativeRecordHeader: true, family: 'background-component'
});
addRawLayouts([517, 518], {
  pixelFormat: 'rgba5551', frameWidth: 14, frameHeight: 14, frameCount: 7,
  frameColumns: 7,
  warnings: ['The 7×14×14 tile segmentation is byte-exact. Native materializers are exact; texture submission and cleanup remain unresolved.']
});
addRawLayouts([578], {
  pixelFormat: 'rgba5551', frameWidth: 16, frameHeight: 16, frameCount: 7,
  frameColumns: 7,
  warnings: ['The seven-frame sequence is structurally supported; its runtime meaning remains unresolved.']
});
addRawLayouts([581], {
  pixelFormat: 'rgba5551', width: 128, height: 107,
  warnings: ['A four-plane 32×107 segmentation also conserves the source bytes.']
});
addRawLayouts(range(585, 588), { pixelFormat: 'rgba5551', width: 32, height: 28 });
addRawLayouts(range(589, 591), { pixelFormat: 'rgba5551', width: 32, height: 16 });
addRawLayouts(range(592, 597), { pixelFormat: 'rgba5551', width: 64, height: 32 });
addRawLayouts([598], { pixelFormat: 'rgba5551', width: 32, height: 16 });
addRawLayouts([599], { pixelFormat: 'rgba5551', width: 92, height: 76 });
addRawLayouts(range(600, 610), { pixelFormat: 'rgba5551', width: 84, height: 67 });
addRawLayouts(range(611, 618), { pixelFormat: 'rgba5551', width: 20, height: 20 });
addRawLayouts(range(619, 621), {
  pixelFormat: 'rgba5551', width: 24, height: 33,
  warnings: ['The three files share an exact alpha mask and contain baked color variants. Native selector construction is exact; selector-to-archive materialization remains unresolved.']
});
addRawLayouts([622], {
  pixelFormat: 'rgba5551', width: 104, height: 52,
  warnings: ['Alternative tiled segmentations remain possible. Native selector construction is exact; selector-to-archive materialization remains unresolved.']
});
addRawLayouts(range(623, 630), { pixelFormat: 'rgba5551', width: 20, height: 13 });
addRawLayouts([631], { pixelFormat: 'rgba5551', width: 68, height: 10 });
addRawLayouts([632], { pixelFormat: 'rgba5551', width: 124, height: 39 });
addRawLayouts([633], { pixelFormat: 'rgba5551', width: 80, height: 10 });
addRawLayouts([634], { pixelFormat: 'rgba5551', width: 52, height: 10 });
addRawLayouts([635], { pixelFormat: 'rgba5551', width: 16, height: 3 });
addRawLayouts([636], {
  pixelFormat: 'ci8', width: 256, height: 48,
  paletteOffset: 0, paletteCount: 32, paletteBankCount: 11, dataOffset: 0x2C0,
  warnings: ['The eleven exact 32-color palettes animate one shared CI8 image. The 256×48 dimensions are a high-confidence structural layout; its natural renderer remains unresolved.']
});
addRawLayouts([638], { pixelFormat: 'rgba5551', width: 84, height: 16 });
addRawLayouts([639], {
  pixelFormat: 'ci4', width: 192, height: 23, paletteOffset: 0,
  paletteCount: 16, dataOffset: 32
});
addRawLayouts([640], {
  pixelFormat: 'ci4', width: 150, height: 64, paletteOffset: 0,
  paletteCount: 16, dataOffset: 64,
  warnings: ['The source contains two 16-color palettes. The preview uses the first palette.']
});
addRawLayouts([641], {
  pixelFormat: 'i8', width: 32, height: 24,
  warnings: ['The intensity plane and no-palette renderer are exact. Its archive-to-resource selector remains unresolved.']
});
addRawLayouts([642], {
  pixelFormat: 'ci4', width: 104, height: 56, paletteOffset: 0,
  paletteCount: 16, dataOffset: 32
});
addRawLayouts([643], { pixelFormat: 'rgba5551', width: 24, height: 9 });
addRawLayouts([644, 645], {
  pixelFormat: 'ci4', width: 32, height: 24, paletteOffset: 0,
  paletteCount: 16, dataOffset: 64,
  warnings: ['The source contains two 16-color palettes. The preview uses the first palette.']
});
addRawLayouts(range(817, 822), { pixelFormat: 'rgba5551', width: 128, height: 40 });
addRawLayouts([823], { pixelFormat: 'i8', width: 128, height: 80 });
addRawLayouts([824], {
  pixelFormat: 'i4', frameWidth: 144, frameHeight: 15, frameCount: 6,
  frameColumns: 1,
  warnings: ['The six 144×15 I4 deity-name panels and no-palette renderer are exact. The upstream panel selector meaning remains unresolved.']
});

const ARCHIVE_CONSUMER_EVIDENCE = Object.freeze({
  517: Object.freeze({
    evidenceGrade: 'direct',
    consumerStatus: 'exact archive materializer; texture submission and cleanup unresolved',
    selectorValue: 0x0205,
    loaderFunction: 'func_0001B2E0',
    loaderFunctionZ64: '0x0001B2E0',
    materializerFunction: 'func_0001A0F8',
    materializerFunctionZ64: '0x0001A0F8',
    firstMissingJoin: 'decoded object pointer to texture submission and cleanup'
  }),
  518: Object.freeze({
    evidenceGrade: 'direct',
    consumerStatus: 'exact archive materializer; texture submission and cleanup unresolved',
    selectorValue: 0x0206,
    loaderFunction: 'func_0001B320',
    loaderFunctionZ64: '0x0001B320',
    materializerFunction: 'func_0001A0F8',
    materializerFunctionZ64: '0x0001A0F8',
    firstMissingJoin: 'decoded object pointer to texture submission and cleanup'
  }),
  619: Object.freeze({
    evidenceGrade: 'supported-selector-stage',
    consumerStatus: 'selector builder located; archive materializer join unresolved',
    selectorValue: 0x026B,
    selectorFunction: 'func_0012099C',
    selectorFunctionZ64: '0x0012099C',
    firstMissingJoin: 'selector value to physical archive materializer'
  }),
  620: Object.freeze({
    evidenceGrade: 'supported-selector-stage',
    consumerStatus: 'selector builder located; archive materializer join unresolved',
    selectorValue: 0x026C,
    selectorFunction: 'func_0012099C',
    selectorFunctionZ64: '0x0012099C',
    firstMissingJoin: 'selector value to physical archive materializer'
  }),
  621: Object.freeze({
    evidenceGrade: 'supported-selector-stage',
    consumerStatus: 'selector builder located; archive materializer join unresolved',
    selectorValue: 0x026D,
    selectorFunction: 'func_0012099C',
    selectorFunctionZ64: '0x0012099C',
    firstMissingJoin: 'selector value to physical archive materializer'
  }),
  622: Object.freeze({
    evidenceGrade: 'supported-selector-stage',
    consumerStatus: 'selector builder located; archive materializer join unresolved',
    selectorValue: 0x026E,
    selectorFunction: 'func_0012099C',
    selectorFunctionZ64: '0x0012099C',
    firstMissingJoin: 'selector value to physical archive materializer'
  }),
  645: Object.freeze({
    evidenceGrade: 'candidate-numeric-only',
    consumerStatus: 'matching numeric value reaches an overlay routine; archive identity is unproved',
    selectorValue: 0x0285,
    selectorCallZ64: '0x0007C8C4',
    firstMissingJoin: 'numeric value to physical archive materializer'
  })
});

const SECTION_C_ASSETS = Object.freeze([
  { index: 0, start: 5850056, end: 5875292, size: 25236, sha256: 'AEF158F3DA46CEA3949AB6A16A4FE310C0A0DD9D0D8BA5B2E5FE6BA188C3B13C' },
  { index: 1, start: 5875292, end: 5901876, size: 26584, sha256: '794DE72D8C12E79F555668AE1A5B2261BEC3CDF8AC28995BF1888A3BF52C205D' },
  { index: 2, start: 5901876, end: 5929072, size: 27196, sha256: '968380FABE904F664052AA727F9AFD713F4E43224EF13AA3C50C2237A5A6F5E6' },
  { index: 3, start: 5929072, end: 5956208, size: 27136, sha256: '6F126D8A754E32704E15D50DF6A9EB6A2A937B5CDE5DC224FC41736F83D66439' },
  { index: 4, start: 5956208, end: 5976292, size: 20084, sha256: '7E6E468E7DC28204A990646CC3FAA4AD90C14A8CD9A23A190E126E5BF616AF55' },
  { index: 5, start: 5976292, end: 6001404, size: 25112, sha256: '75AC9EB96474EA8460B9788C25D23B7F1D0E538AF649D28A47DF7F82B97EA0E0' },
  { index: 6, start: 6001404, end: 6023236, size: 21832, sha256: '9BE85B0727EAACB5C1BD51FA3BCB15B36B7C125F83B047F11F550C7B45247A8E' },
  { index: 7, start: 6023236, end: 6044140, size: 20904, sha256: '09246DA4E19BCA98829FC67762AFC3C3AF720DB1E290DC7BF88DBBA902117AC9' },
  { index: 8, start: 6044140, end: 6067788, size: 23648, sha256: 'A9D1B8C0778BD91951494FFC20F056D83E41B32C1E35B0A37DB85C3FD8D259EC' },
  { index: 9, start: 6067788, end: 6090060, size: 22272, sha256: '36BDD14C8300590BF50C6323520DC452915000621F505B81A7F3E22097B1ABE8' },
  { index: 10, start: 6090060, end: 6113056, size: 22996, sha256: '7190CC001ACE0457B7ED19A0F404CB925D7F661E7A14EC32D0E4673332E9A2F5' },
  { index: 11, start: 6113056, end: 6137540, size: 24484, sha256: '07520AB3D7176BA4E429B38E22FF70720B48B00EB2D49381C473CD8E9AAD5C01' },
  { index: 12, start: 6137540, end: 6157228, size: 19688, sha256: 'C80D4785639B110503D4D22D8B16EACA8ED0F4FF9224B03568A95B7AFED39973' },
  { index: 13, start: 6157228, end: 6178296, size: 21068, sha256: '0A11840536836863C925EE6042FE575C22D785AA3A650EF98365971C44157D0C' },
  { index: 14, start: 6178296, end: 6201744, size: 23448, sha256: '33A476FAC93106DD8523CA95B63EE1550D94B7D6F8605D32A12FD0EA5278F905' },
  { index: 15, start: 6201744, end: 6223860, size: 22116, sha256: '8607A3F17C7E1E03D016E6289A973BC09A985C2D203B4E1C000924C22DC62F62' },
  { index: 16, start: 6223860, end: 6246576, size: 22716, sha256: 'DD316778D29A7E6A2580F7C77BEF10E6D2D469D967ABE02A49EEC9200F236EAC' },
  { index: 17, start: 6246576, end: 6269356, size: 22780, sha256: '06A0C1AD7DC5AB940511AF82A2EFD44BEEECB775AB93202722B900A578F063C3' },
  { index: 18, start: 6269356, end: 6296900, size: 27544, sha256: 'B9CF259B8F08DF2D4B107F97514EBB2E2B368CDCD7D205961957A20FF41FE719' },
  { index: 19, start: 6296900, end: 6323824, size: 26924, sha256: 'A982CEF2AD2494726135CFC6362413D92D106ED2D0029C658EDE43149D3EAE73' },
  { index: 20, start: 6323824, end: 6348196, size: 24372, sha256: 'BFFB40564F806829039ECE519E8A51FAE1BDE2D6279CD9579BB16DC3349AB263' },
  { index: 21, start: 6348196, end: 6370704, size: 22508, sha256: 'EA5054D43889AF920811CB86994BBF593E8CB5CFEA589D659B8E40975230C194' },
  { index: 22, start: 6370704, end: 6390248, size: 19544, sha256: 'A84138359D10195A8A41685DD35285DCBDF712417AF84A47DC7BDF914802B8A6' },
  { index: 23, start: 6390248, end: 6410388, size: 20140, sha256: '1345F29C29C3495129D28BD3CC03B7BE82970E3566F43CC3AB9B7885B3AA635E' },
  { index: 24, start: 6410388, end: 6432712, size: 22324, sha256: '779A36713FA2E87050ED883AEB02FF1D713141D221A78C24DE23AB03553F34EA' },
  { index: 25, start: 6432712, end: 6453332, size: 20620, sha256: '9D2AC05EC6B6A483B13A576114C7E29A05AF53FFF73C6A9225E8DEF04EA4D27D' },
  { index: 26, start: 6453332, end: 6472980, size: 19648, sha256: 'A5CFF6E4D5798F6A474C360DA7A257E4769CA9E1E3A55C36B7EDB68792935F99' },
  { index: 27, start: 6472980, end: 6491064, size: 18084, sha256: 'E8D031BDD3485B8AEB08BF9BD06FB22D3B7CD9A622460FB8E4702AB5C9226097' },
  { index: 28, start: 6491064, end: 6514560, size: 23496, sha256: 'F989E672A6EE41F3CF2B192E02F127C5FA206841D944FC40A59770F5D0866D04' }
]);
const SECTION_C_ANALYSIS_SHA256 =
  'C9F9AE135B12C7C4A1C98CC70131559163AFD2B77477C0307D09B2490D30BC96';
const RAW_US_REV0_V64_SHA256 =
  '6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12';
const NORMALIZED_US_REV0_Z64_SHA256 =
  '571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A';
const DIRECTOR_RESOURCE_BASE_Z64 = 0x00594280;
const CLASS_EVOLUTION_MEDIA_TABLE_Z64 = 0x000654A0;
const CLASS_EVOLUTION_MEDIA_ROW_COUNT = 69;
const CLASS_EVOLUTION_MEDIA_ROW_BYTES = 9;
const CLASS_EVOLUTION_MEDIA_SELECTOR_OFFSET = 3;
const CLASS_EVOLUTION_MEDIA_TABLE_SHA256 =
  '540C6836EB33DF804CF1B9AD5F03FC381F96BB8052A05EA258A4FDE195A1FF74';
const OVERSIZED_IMAGE_ROOT_KEY = 0x018BD022;
const OVERSIZED_IMAGE_ROOT_CHILD_COUNT = 41;
const OVERSIZED_IMAGE_ROOT_PAYLOAD_SHA256 =
  '484B728AA0C5CF5C5F51797FE639D14DAD4594B7EFB652928AAE42E606D65CE2';
const SCENE_RESOURCE_PATH_ROOT_KEY = 0x01A8D7A6;
const SCENE_RESOURCE_PATH_GROUP_COUNT = 59;
const SCENE_RESOURCE_PATH_ENTRY_COUNT = 134;
const SCENE_RESOURCE_PATH_POPULATED_COUNT = 121;
const MODE_TWO_STAGE_PLACEMENT_RESOURCE_KEY = 0x00315736;
const MODE_TWO_STAGE_PLACEMENT_PREFIX_Z64 = 0x008A99B6;
const MODE_TWO_STAGE_PLACEMENT_PAYLOAD_Z64 = 0x008A99BA;
const MODE_TWO_STAGE_PLACEMENT_PAYLOAD_BYTES = 0x2B00;
const MODE_TWO_STAGE_PLACEMENT_PAYLOAD_SHA256 =
  '94248DB2C97D0B6E4169200ED13C7F19FAC0E35DD4D85189DD292736D612C7E6';
const MODE_TWO_STAGE_ORTHOGRAPHIC_HEADER_ENTRY = 9;
const MODE_TWO_STAGE_PERSPECTIVE_HEADER_ENTRY = 10;
const MODE_TWO_STAGE_ORTHOGRAPHIC_TABLE_OFFSET = 0x12D8;
const MODE_TWO_STAGE_PERSPECTIVE_TABLE_OFFSET = 0x1CB4;
const MODE_TWO_STAGE_SELECTOR_COUNT = 80;
const MODE_TWO_STAGE_ORTHOGRAPHIC_ROW_BYTES = 22;
const MODE_TWO_STAGE_PERSPECTIVE_ROW_BYTES = 26;
const MODE_TWO_STAGE_ORTHOGRAPHIC_ROWS = 105;
const MODE_TWO_STAGE_PERSPECTIVE_ROWS = 63;
const MODE_TWO_STAGE_SPECIAL_ROWS = 3;
const MODE_TWO_STAGE_NORMAL_ROWS = 165;
const MODE_TWO_STAGE_NONEMPTY_SELECTORS = 25;
const MODE_TWO_STAGE_DESCRIPTOR_TABLE_KEY = 0x003B6CD0;
const MODE_TWO_STAGE_DESCRIPTOR_TABLE_PAYLOAD_Z64 = 0x0094AF54;
const MODE_TWO_STAGE_DESCRIPTOR_TABLE_BYTES = 0x340;
const MODE_TWO_STAGE_DESCRIPTOR_TABLE_SHA256 =
  'D85D920B576BEAA6141979CFFD7D885932E6B35517715453C680E73EA2E63374';
const MODE_TWO_STAGE_DESCRIPTOR_COUNT = 208;
const MODE_TWO_SIGNED_TERRAIN_TABLE_Z64 = 0x000694B0;
const MODE_TWO_SIGNED_SCENARIO_TABLE_Z64 = 0x00069518;
const MODE_TWO_UNSIGNED_TERRAIN_TABLE_Z64 = 0x00069560;
const MODE_TWO_UNSIGNED_SCENARIO_TABLE_Z64 = 0x000695C8;
const MODE_TWO_DERIVED_TERRAIN_TABLE_BYTES = 0x68;
const MODE_TWO_DERIVED_SCENARIO_TABLE_BYTES = 0x3E;
const MODE_TWO_DERIVED_TERRAIN_TABLE_SHA256 =
  'A22515B3603C7FD2DD66A4FFEBEDC289F3D12AE13D3FA452206AD2A723AD666B';
const MODE_TWO_DERIVED_SCENARIO_TABLE_SHA256 =
  'C292DB825ECF4C40838FF69B45F40625F94FDC104828BFAF297F7DCA2D330402';
const MODE_TWO_DERIVED_SCENARIO_OVERRIDES = Object.freeze([
  [1, 31, 1], [2, 35, 1], [3, 31, 2], [8, 31, 1],
  [12, 31, 2], [39, 31, 55], [45, 31, 51], [46, 31, 51],
  [47, 31, 57], [52, 31, 1], [60, 31, 57]
]);
const DIRECTOR_SELECTOR_TABLE_KEY = 0x019A8804;
const DIRECTOR_SELECTOR_TABLE_PREFIX_Z64 = 0x01F3CA84;
const DIRECTOR_SELECTOR_TABLE_PAYLOAD_Z64 = 0x01F3CA88;
const DIRECTOR_SELECTOR_TABLE_BYTES = 0x1A74;
const DIRECTOR_SELECTOR_ROWS = 1693;
const DIRECTOR_CONTINUATION_TABLE_KEY = 0x0189099E;
const DIRECTOR_CONTINUATION_TABLE_BYTES = 0x14;
const DIRECTOR_CONTINUATION_KEYS = Object.freeze([
  0x018909B6, 0x01890A14, 0x01890A52, 0x01890A8A, 0x01890ACC
]);
const DIRECTOR_CONTINUATION_DECODED_BYTES = Object.freeze([
  260, 144, 96, 124, 92
]);
const DIRECTOR_TERMINAL_CLASS_DISPATCH_ROUTES = Object.freeze({
  1: -3,
  2: -6,
  3: -5,
  4: -4,
  5: -7,
  6: -8,
  7: -9,
  8: -10
});
const RETAIL_DIRECTOR_POPULATED_ROWS = 1548;
const RETAIL_DIRECTOR_UNIQUE_RESOURCES = 1498;
const RETAIL_DIRECTOR_WORDS = 550019;
const RETAIL_DIRECTOR_SUBSTREAM_CALLS = 247;
const RETAIL_DIRECTOR_SELECTOR_EXPANDED_SUBSTREAM_CALLS = 248;
const RETAIL_DIRECTOR_TAIL_CALLS = 4;
const RETAIL_DIRECTOR_OPCODE_5E_OCCURRENCES = 10;
const EVENT_DIRECTORY_KEY = 0x003A5668;
const EVENT_DIRECTORY_ROWS = 115;
const EVENT_DIRECTORY_POPULATED_ROWS = 113;
const EVENT_SCHEDULER_LAST_ROW = 109;
const EVENT_SCHEDULER_OUTER_ENTRY_COUNT = 6;
const EVENT_SPECIAL_OUTER_ENTRY_COUNT = 1;
const EVENT_OUTER_ENTRY_ROWS = 653;
const EVENT_DISTINCT_OUTER_CURSORS = 640;
const EVENT_SEQUENCE_TABLES = 623;
const EVENT_SEQUENCE_ENTRY_ROWS = 1496;
const EVENT_DIRECT_OUTER_SEQUENCES = 17;
const EVENT_DISTINCT_SEQUENCE_CURSORS = 1513;
const EVENT_LAUNCH_SEQUENCE_CURSORS = 1443;
const DIRECT_EVENT_DIRECTOR_LAUNCHES = 1998;
const DIRECT_EVENT_DIRECTOR_SELECTORS = 1520;
const DIRECT_EVENT_DIRECTOR_RESOURCES = 1472;
const EVENT_TRANSLATION_TABLE_TRACKED_ENTRIES = 17;
const EVENT_STATIC_INVOCATION_CONTEXTS = 2076;
const EVENT_MULTI_INVOCATION_LAUNCHES = 38;
const EVENT_DISTINCT_INVOCATION_CURSORS = 1717;
const EVENT_EXTERNAL_REQUEST_PHYSICAL_SITES = 45;
const EVENT_EXTERNAL_REQUEST_HANDOFFS = 47;
const EVENT_TRANSLATION_PHYSICAL_SITES = 69;
const EVENT_TRANSLATION_WRITE_CONTEXTS = 416;
const EVENT_TRANSLATION_EXACT_CONTEXTS = 128;
const EVENT_TRANSLATION_UNRESOLVED_CONTEXTS = 288;
const EVENT_SUBSTITUTION_SOURCE_PHYSICAL_SITES = 42;
const EVENT_SUBSTITUTION_SOURCE_WRITE_CONTEXTS = 42;
const EVENT_SUBSTITUTION_SOURCE_A_WRITE_CONTEXTS = 21;
const EVENT_SUBSTITUTION_SOURCE_B_WRITE_CONTEXTS = 21;
const EVENT_SUBSTITUTION_SOURCE_EXACT_INDEX_CONTEXTS = 2;
const EVENT_SUBSTITUTION_SOURCE_UNRESOLVED_INDEX_CONTEXTS = 40;
const EVENT_SUBSTITUTION_SOURCE_EXACT_VALUE_CONTEXTS = 4;
const EVENT_SUBSTITUTION_SOURCE_UNRESOLVED_VALUE_CONTEXTS = 38;
const EVENT_RETAIL_TRANSLATION_PHYSICAL_SITES = 21;
const EVENT_RETAIL_TRANSLATION_WRITE_CONTEXTS = 344;
const EVENT_RETAIL_TRANSLATION_EXACT_CONTEXTS = 74;
const EVENT_RETAIL_TRANSLATION_UNRESOLVED_CONTEXTS = 270;
const EVENT_NONRETAIL_TRANSLATION_PHYSICAL_SITES = 48;
const EVENT_NONRETAIL_TRANSLATION_WRITE_CONTEXTS = 72;
const EVENT_SUBSTITUTION_SOURCES = [
  {
    sourceId: 'A',
    semantic: 'primary-class-id',
    characterRecordBaseRamAddress: '0x80193BC0',
    characterRecordFieldOffset: 0x11,
    characterRecordStride: 56,
    storageRamRange: '0x8019367C..0x80193680',
    slotCount: 5,
    getterFunctionZ64: '0x00046314',
    setterFunctionZ64: '0x0004649C',
    evidenceStatus: 'native-static-event-vm-and-character-record'
  },
  {
    sourceId: 'B',
    semantic: 'secondary-class-id',
    characterRecordBaseRamAddress: '0x80193BC0',
    characterRecordFieldOffset: 0x12,
    characterRecordStride: 56,
    storageRamRange: '0x80193681..0x80193685',
    slotCount: 5,
    getterFunctionZ64: '0x00046324',
    setterFunctionZ64: '0x000464AC',
    evidenceStatus: 'native-static-event-vm-and-character-record'
  }
];
const EVENT_SPECIAL_PROPERTY_WIDTHS = new Map([
  [0xE4, 8], [0xE5, 8], [0xE6, 16], [0xE7, 8], [0xE8, 8], [0xE9, 8],
  [0xEB, 8], [0xEC, 16], [0xED, 8], [0xEE, 16], [0xEF, 8], [0xF0, 8],
  [0xF9, 8], [0xFA, 16], [0xFB, 8], [0xFD, 8], [0xFE, 8], [0xFF, 8]
]);
const EVENT_SPECIAL_GETTER_OPERANDS = new Set([
  0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0,
  0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC,
  0xFD, 0xFE, 0xFF
]);

function usage() {
  return [
    'Usage: node tools/generate-cutscene-data.js [options]',
    '',
    'Options:',
    '  --rom PATH              Read-only US Rev 0 master V64',
    '  --asset-manifest PATH   Workbench asset-manifest.json',
    '  --director-corpus-assets PATH  Corrected 153-command asset JSONL',
    '  --director-corpus-grammar PATH  Corrected 153-command grammar JSONL',
    '  --director-corpus-nodes PATH  Corrected full-stream node JSONL',
    '  --director-corpus-waits PATH  Corrected registered-wait JSONL',
    '  --archive-catalog PATH   ROM-order archive catalog JSON',
    '  --audio-catalog PATH     Sequenced-audio block catalog JSON',
    '  --pose-vocabulary PATH   Cutscene actor pose-vocabulary JSON',
    '  --pose-selectors PATH     All-bank pose selector JSONL',
    '  --pose-physical-states PATH  All-bank physical pose-state JSONL',
    '  --partial-directors PATH  ROM-wide Director asset census JSONL',
    '  --director-tail-recovery PATH  Runtime-tiled Director tail recovery JSON',
    '  --director-selector-table PATH  Director selector-table ownership JSON',
    '  --serifu-presentation-selectors PATH  Serifu presentation selector JSON',
    '  --sprite-corpus PATH      Cutscene 0x5554 descriptor corpus JSON',
    '  --extracted-root PATH    Extracted ob64_all root',
    '  --output PATH            Generated JavaScript output',
    '  --check                  Fail if the generated output differs',
    '  --help                   Show this message'
  ].join('\n');
}

function parseArgs(argv) {
  const options = {
    rom: defaultMasterRom,
    assetManifest: defaultAssetManifest,
    directorCorpusAssets: defaultDirectorCorpusAssets,
    directorCorpusGrammar: defaultDirectorCorpusGrammar,
    directorCorpusNodes: defaultDirectorCorpusNodes,
    directorCorpusWaits: defaultDirectorCorpusRegisteredWaits,
    archiveCatalog: defaultArchiveCatalog,
    audioCatalog: defaultAudioCatalog,
    poseVocabulary: defaultPoseVocabulary,
    poseSelectors: defaultPoseSelectors,
    posePhysicalStates: defaultPosePhysicalStates,
    partialDirectors: defaultPartialDirectorAssets,
    directorTailRecovery: defaultDirectorTailRecovery,
    directorSelectorTable: defaultDirectorSelectorTable,
    serifuPresentationSelectors: defaultSerifuPresentationSelectors,
    spriteCorpus: defaultSpriteCorpus,
    extractedRoot: defaultExtractedRoot,
    output: defaultOutput,
    check: false
  };
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === '--help') {
      process.stdout.write(usage() + '\n');
      process.exit(0);
    }
    if (argument === '--check') {
      options.check = true;
      continue;
    }
    const field = {
      '--rom': 'rom',
      '--asset-manifest': 'assetManifest',
      '--director-corpus-assets': 'directorCorpusAssets',
      '--director-corpus-grammar': 'directorCorpusGrammar',
      '--director-corpus-nodes': 'directorCorpusNodes',
      '--director-corpus-waits': 'directorCorpusWaits',
      '--archive-catalog': 'archiveCatalog',
      '--audio-catalog': 'audioCatalog',
      '--pose-vocabulary': 'poseVocabulary',
      '--pose-selectors': 'poseSelectors',
      '--pose-physical-states': 'posePhysicalStates',
      '--partial-directors': 'partialDirectors',
      '--director-tail-recovery': 'directorTailRecovery',
      '--director-selector-table': 'directorSelectorTable',
      '--serifu-presentation-selectors': 'serifuPresentationSelectors',
      '--sprite-corpus': 'spriteCorpus',
      '--extracted-root': 'extractedRoot',
      '--output': 'output'
    }[argument];
    if (!field || index + 1 >= argv.length) {
      throw new Error('Unknown or incomplete argument: ' + argument + '\n' + usage());
    }
    options[field] = path.resolve(argv[++index]);
  }
  return options;
}

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function readJsonLines(filePath) {
  return fs.readFileSync(filePath, 'utf8').split(/\r?\n/)
    .filter((line) => line.trim()).map((line) => JSON.parse(line));
}

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function sha256File(filePath) {
  return sha256(fs.readFileSync(filePath));
}

function readU32(bytes, offset, label) {
  if (!Buffer.isBuffer(bytes) || offset < 0 || offset + 4 > bytes.length) {
    throw new Error((label || 'ROM word') + ' lies outside its byte source.');
  }
  return bytes.readUInt32BE(offset);
}

function readU16(bytes, offset, label) {
  if (!Buffer.isBuffer(bytes) || offset < 0 || offset + 2 > bytes.length) {
    throw new Error((label || 'ROM halfword') + ' lies outside its byte source.');
  }
  return bytes.readUInt16BE(offset);
}

function readI16(bytes, offset, label) {
  if (!Buffer.isBuffer(bytes) || offset < 0 || offset + 2 > bytes.length) {
    throw new Error((label || 'ROM signed halfword') + ' lies outside its byte source.');
  }
  return bytes.readInt16BE(offset);
}

function readI32(bytes, offset, label) {
  if (!Buffer.isBuffer(bytes) || offset < 0 || offset + 4 > bytes.length) {
    throw new Error((label || 'ROM signed word') + ' lies outside its byte source.');
  }
  return bytes.readInt32BE(offset);
}

function signed(value) {
  return Number(value) | 0;
}

function normalizeV64(raw) {
  if (!Buffer.isBuffer(raw) || raw.length % 2) {
    throw new Error('The source V64 must contain an even number of bytes.');
  }
  const output = Buffer.allocUnsafe(raw.length);
  for (let offset = 0; offset < raw.length; offset += 2) {
    output[offset] = raw[offset + 1];
    output[offset + 1] = raw[offset];
  }
  return output;
}

function nativeResourceEnvelope(z64, resourceKey, label) {
  const normalizedKey = Number(resourceKey) & 0x0FFFFFFF;
  const prefixStart = DIRECTOR_RESOURCE_BASE_Z64 + normalizedKey;
  const storedLength = readU32(z64, prefixStart, label + ' size prefix');
  const payloadStart = prefixStart + 4;
  const payloadEndExclusive = payloadStart + storedLength;
  if (storedLength < 1 || payloadEndExclusive > z64.length) {
    throw new Error(label + ' has an invalid native resource envelope.');
  }
  return {
    resourceKey: normalizedKey,
    prefixStart,
    payloadStart,
    payloadEndExclusive,
    storedLength,
    payload: z64.subarray(payloadStart, payloadEndExclusive),
    payloadSha256: sha256(z64.subarray(payloadStart, payloadEndExclusive))
  };
}

function f32(value) {
  return Math.fround(value);
}

function f32Add(left, right) {
  return f32(f32(left) + f32(right));
}

function f32Subtract(left, right) {
  return f32(f32(left) - f32(right));
}

function f32Multiply(left, right) {
  return f32(f32(left) * f32(right));
}

function f32Divide(left, right) {
  return f32(f32(left) / f32(right));
}

function f32Distance2d(leftX, leftY, rightX, rightY) {
  const deltaX = f32Subtract(rightX, leftX);
  const deltaY = f32Subtract(rightY, leftY);
  return f32(Math.sqrt(f32Add(
    f32Multiply(deltaX, deltaX), f32Multiply(deltaY, deltaY))));
}

// func_002A1860 solves the natural cubic system with coefficients scaled by
// one third. func_002A1CF4 consumes that scale directly in its cubic formula.
function nativeNaturalSplineCoefficients(parameters, values) {
  const count = parameters.length;
  const coefficients = Array(count).fill(0);
  if (count <= 2) return coefficients;
  const upper = Array(count).fill(0);
  const right = Array(count).fill(0);
  for (let index = 1; index < count - 1; index += 1) {
    const previousWidth = Number(parameters[index]) - Number(parameters[index - 1]);
    const nextWidth = Number(parameters[index + 1]) - Number(parameters[index]);
    const previousSlope = (Number(values[index]) - Number(values[index - 1])) /
      previousWidth;
    const nextSlope = (Number(values[index + 1]) - Number(values[index])) /
      nextWidth;
    const diagonal = 2 * (previousWidth + nextWidth) -
      previousWidth * upper[index - 1];
    upper[index] = nextWidth / diagonal;
    right[index] = ((nextSlope - previousSlope) -
      previousWidth * right[index - 1]) / diagonal;
  }
  for (let index = count - 2; index > 0; index -= 1) {
    coefficients[index] = f32(right[index] -
      upper[index] * coefficients[index + 1]);
  }
  return coefficients;
}

function nativeSplineValue(parameter, parameters, values, coefficients) {
  let low = 0;
  let high = parameters.length - 1;
  while (low < high) {
    const middle = Math.trunc((low + high) / 2);
    if (parameters[middle] < parameter) low = middle + 1;
    else high = middle;
  }
  const interval = Math.max(0, low - (low > 0 ? 1 : 0));
  const offset = f32Subtract(parameter, parameters[interval]);
  const width = f32Subtract(parameters[interval + 1], parameters[interval]);
  const coefficient = coefficients[interval];
  const nextCoefficient = coefficients[interval + 1];
  const slope = f32Divide(
    f32Subtract(values[interval + 1], values[interval]), width);
  const linear = f32Subtract(slope, f32Multiply(width,
    f32Add(f32Add(coefficient, coefficient), nextCoefficient)));
  const quadratic = f32Add(f32Multiply(3, coefficient),
    f32Multiply(offset, f32Divide(
      f32Subtract(nextCoefficient, coefficient), width)));
  return f32Add(values[interval], f32Multiply(offset,
    f32Add(linear, f32Multiply(offset, quadratic))));
}

// This reproduces the two-axis path sampler at func_002A1EC4. Output sample
// zero is the first authored point and the final authored endpoint is omitted.
function nativeSampleScenePath(points, sampleCount) {
  if (!Array.isArray(points) || points.length < 2 || !Number.isInteger(sampleCount) ||
      sampleCount < 1) {
    throw new Error('Native scene-path sampling requires two points and a positive count.');
  }
  const sourceX = points.map((point) => f32(point.x));
  const sourceY = points.map((point) => f32(point.y));
  const output = [];
  if (points.length === 2) {
    const stepX = f32Divide(f32Subtract(sourceX[1], sourceX[0]), sampleCount);
    const stepY = f32Divide(f32Subtract(sourceY[1], sourceY[0]), sampleCount);
    output.push({ x: sourceX[0], y: sourceY[0] });
    for (let index = 1; index < sampleCount; index += 1) {
      output.push({
        x: f32Add(output[index - 1].x, stepX),
        y: f32Add(output[index - 1].y, stepY)
      });
    }
    return output;
  }
  const parameters = Array(points.length).fill(0);
  for (let index = 1; index < points.length; index += 1) {
    parameters[index] = f32Add(parameters[index - 1], f32Distance2d(
      sourceX[index - 1], sourceY[index - 1], sourceX[index], sourceY[index]));
  }
  const total = parameters[parameters.length - 1];
  if (!(total > 0)) throw new Error('Native scene path has zero two-dimensional length.');
  for (let index = 1; index < parameters.length; index += 1) {
    parameters[index] = f32Divide(parameters[index], total);
  }
  const coefficientX = nativeNaturalSplineCoefficients(parameters, sourceX);
  const coefficientY = nativeNaturalSplineCoefficients(parameters, sourceY);
  const step = 1 / sampleCount;
  for (let index = 0; index < sampleCount; index += 1) {
    const parameter = f32(step * index);
    output.push({
      x: nativeSplineValue(parameter, parameters, sourceX, coefficientX),
      y: nativeSplineValue(parameter, parameters, sourceY, coefficientY)
    });
  }
  return output;
}

function nativeSampledPathLength(samples) {
  let length = 0;
  for (let index = 1; index < samples.length; index += 1) {
    length = f32Add(length, f32Distance2d(samples[index - 1].x,
      samples[index - 1].y, samples[index].x, samples[index].y));
  }
  return length;
}

function nativeScenePathHeading(points) {
  const coarseSamples = nativeSampleScenePath(points, 400);
  const measuredLength = nativeSampledPathLength(coarseSamples);
  const denseSampleCount = Math.trunc(measuredLength / 2) * 2;
  if (denseSampleCount < 3) {
    throw new Error('Native scene path is too short to produce its starting heading.');
  }
  const denseSamples = nativeSampleScenePath(points, denseSampleCount);
  const deltaX = f32Subtract(denseSamples[2].x, denseSamples[0].x);
  const deltaY = f32Subtract(denseSamples[2].y, denseSamples[0].y);
  const storedHeading = Math.trunc(Math.atan2(deltaY, deltaX) * 180 / Math.PI);
  return {
    measuredLength,
    denseSampleCount,
    deltaX,
    deltaY,
    storedHeading,
    rotationDegrees: storedHeading === 180 ? 0 : storedHeading + 180
  };
}

function readSceneResourcePaths(z64) {
  const root = nativeResourceEnvelope(z64, SCENE_RESOURCE_PATH_ROOT_KEY,
    'Scene resource-path root');
  if (root.storedLength !== SCENE_RESOURCE_PATH_GROUP_COUNT * 4) {
    throw new Error('Scene resource-path root must contain exactly 59 group keys.');
  }
  const entries = [];
  const groups = [];
  for (let groupIndex = 0; groupIndex < SCENE_RESOURCE_PATH_GROUP_COUNT;
      groupIndex += 1) {
    const groupKey = readU32(root.payload, groupIndex * 4,
      'Scene resource-path group key');
    const group = nativeResourceEnvelope(z64, groupKey,
      'Scene resource-path group ' + groupIndex);
    if (group.storedLength % 4) {
      throw new Error('Scene resource-path group ' + groupIndex +
        ' is not a word array.');
    }
    const entryCount = group.storedLength / 4;
    groups.push({
      groupIndex,
      resourceKey: groupKey,
      entryCount,
      z64PrefixStart: group.prefixStart,
      z64PayloadStart: group.payloadStart,
      storedLength: group.storedLength,
      payloadSha256: group.payloadSha256
    });
    for (let entryIndex = 0; entryIndex < entryCount; entryIndex += 1) {
      const resourceKey = readU32(group.payload, entryIndex * 4,
        'Scene resource-path entry key');
      if (resourceKey === 0) {
        entries.push({
          pathId: 'scene-resource-path:' + groupIndex + ':' + entryIndex,
          groupIndex,
          entryIndex,
          groupResourceKey: groupKey,
          resourceKey: null,
          status: 'empty-native-entry',
          pointCount: 0,
          nativeStoredHeading: null,
          rotationDegrees: null
        });
        continue;
      }
      const envelope = nativeResourceEnvelope(z64, resourceKey,
        'Scene resource path ' + groupIndex + ':' + entryIndex);
      const decoded = decodeCustomLz(envelope.payload,
        'Scene resource path ' + groupIndex + ':' + entryIndex).bytes;
      if (decoded.length < 24 || decoded.length % 12) {
        throw new Error('Scene resource path ' + groupIndex + ':' + entryIndex +
          ' must contain at least two complete 12-byte points.');
      }
      const points = [];
      for (let offset = 0; offset < decoded.length; offset += 12) {
        points.push({
          linkedSpriteSlot: readI32(decoded, offset, 'Scene path point link'),
          x: readI32(decoded, offset + 4, 'Scene path point X'),
          y: readI32(decoded, offset + 8, 'Scene path point Y')
        });
      }
      if (points.some((point) => point.linkedSpriteSlot !== -1)) {
        throw new Error('Retail scene resource path ' + groupIndex + ':' + entryIndex +
          ' unexpectedly depends on an animated-sprite slot.');
      }
      const heading = nativeScenePathHeading(points);
      entries.push({
        pathId: 'scene-resource-path:' + groupIndex + ':' + entryIndex,
        groupIndex,
        entryIndex,
        groupResourceKey: groupKey,
        resourceKey,
        status: 'native-static-path-heading',
        pointCount: points.length,
        firstPoint: points[0],
        secondPoint: points[1],
        finalPoint: points[points.length - 1],
        nativeMeasuredLength: heading.measuredLength,
        nativeDenseSampleCount: heading.denseSampleCount,
        nativeHeadingDeltaX: heading.deltaX,
        nativeHeadingDeltaY: heading.deltaY,
        nativeStoredHeading: heading.storedHeading,
        rotationDegrees: heading.rotationDegrees,
        z64PrefixStart: envelope.prefixStart,
        z64PayloadStart: envelope.payloadStart,
        storedLength: envelope.storedLength,
        storedPayloadSha256: envelope.payloadSha256,
        decodedLength: decoded.length,
        decodedSha256: sha256(decoded)
      });
    }
  }
  if (entries.length !== SCENE_RESOURCE_PATH_ENTRY_COUNT ||
      entries.filter((entry) => entry.resourceKey !== null).length !==
        SCENE_RESOURCE_PATH_POPULATED_COUNT) {
    throw new Error('Scene resource-path inventory changed from 134 entries and 121 paths.');
  }
  return {
    resourceKey: SCENE_RESOURCE_PATH_ROOT_KEY,
    z64PrefixStart: root.prefixStart,
    z64PayloadStart: root.payloadStart,
    storedLength: root.storedLength,
    payloadSha256: root.payloadSha256,
    groups,
    entries
  };
}

function corpusInteger(value, label) {
  if (Number.isInteger(value)) return value;
  if (typeof value === 'string' && /^0x[0-9a-f]+$/i.test(value)) {
    return Number.parseInt(value.slice(2), 16);
  }
  throw new Error(label + ' is not an integer or hexadecimal integer.');
}

function modeTwoStageSource(descriptorHandle, descriptorKeys, descriptorsByKey) {
  if (!Number.isInteger(descriptorHandle) || descriptorHandle < 1 ||
      descriptorHandle > MODE_TWO_STAGE_DESCRIPTOR_COUNT) {
    throw new Error('Mode-two Stage descriptor handle ' + descriptorHandle +
      ' lies outside the direct descriptor table.');
  }
  const descriptorKey = readU32(descriptorKeys, (descriptorHandle - 1) * 4,
    'Mode-two Stage descriptor handle ' + descriptorHandle);
  const descriptor = descriptorsByKey.get(descriptorKey);
  if (!descriptor) {
    throw new Error('Mode-two Stage descriptor handle ' + descriptorHandle +
      ' selects uncatalogued descriptor 0x' + hex(descriptorKey, 8) + '.');
  }
  return nativeStagePropSource({
    bank: 'mode2-stage:' + hex(descriptorKey, 8),
    descriptorKey,
    descriptorMemberCount: descriptor.memberCount,
    metadataKey: corpusInteger(descriptor.metadataKey,
      'Mode-two Stage descriptor metadata key'),
    poseKey: corpusInteger(descriptor.poseKey,
      'Mode-two Stage descriptor pose key'),
    configKey: corpusInteger(descriptor.configKey,
      'Mode-two Stage descriptor config key'),
    lookupKey: corpusInteger(descriptor.lookupKey,
      'Mode-two Stage descriptor lookup key'),
    metadataDecodedLength: descriptor.metadataDecodedLength,
    poseDecodedLength: descriptor.poseDecodedLength,
    configDecodedLength: descriptor.configDecodedLength,
    lookupDecodedLength: descriptor.lookupDecodedLength,
    artCount: descriptor.artCount,
    lookupBankCount: descriptor.lookupBankCount
  });
}

function modeTwoPlacementFingerprint(placements) {
  return placements.map((placement) => [
    placement.poseSelector, placement.x, placement.y, placement.z
  ]);
}

function assertModeTwoPlacementMatch(actual, expected, label) {
  ['orthographicPlacements', 'perspectivePlacements'].forEach((field) => {
    if (JSON.stringify(modeTwoPlacementFingerprint(actual[field])) !==
        JSON.stringify(modeTwoPlacementFingerprint(expected[field]))) {
      throw new Error(label + ' no longer matches the accepted native Stage placements.');
    }
    if (actual[field].some((placement) =>
      placement.source.descriptorKey !== expected.source.descriptorKey)) {
      throw new Error(label + ' no longer selects its accepted Stage sprite descriptor.');
    }
  });
}

function compactModeTwoStagePlacementProfiles(z64, spriteCorpus) {
  const placement = nativeResourceEnvelope(z64,
    MODE_TWO_STAGE_PLACEMENT_RESOURCE_KEY, 'Mode-two Stage placement resource');
  if (placement.prefixStart !== MODE_TWO_STAGE_PLACEMENT_PREFIX_Z64 ||
      placement.payloadStart !== MODE_TWO_STAGE_PLACEMENT_PAYLOAD_Z64 ||
      placement.storedLength !== MODE_TWO_STAGE_PLACEMENT_PAYLOAD_BYTES ||
      placement.payloadSha256 !== MODE_TWO_STAGE_PLACEMENT_PAYLOAD_SHA256) {
    throw new Error('The mode-two Stage placement resource identity changed.');
  }
  const descriptorTable = nativeResourceEnvelope(z64,
    MODE_TWO_STAGE_DESCRIPTOR_TABLE_KEY, 'Mode-two direct descriptor table');
  if (descriptorTable.payloadStart !== MODE_TWO_STAGE_DESCRIPTOR_TABLE_PAYLOAD_Z64 ||
      descriptorTable.storedLength !== MODE_TWO_STAGE_DESCRIPTOR_TABLE_BYTES ||
      descriptorTable.payloadSha256 !== MODE_TWO_STAGE_DESCRIPTOR_TABLE_SHA256) {
    throw new Error('The mode-two direct descriptor table identity changed.');
  }
  if (descriptorTable.storedLength !== MODE_TWO_STAGE_DESCRIPTOR_COUNT * 4) {
    throw new Error('The mode-two direct descriptor table row count changed.');
  }
  const corpusDescriptors = spriteCorpus && spriteCorpus.combat &&
    spriteCorpus.combat.descriptors;
  if (!Array.isArray(corpusDescriptors)) {
    throw new Error('The combat sprite corpus lacks its descriptor inventory.');
  }
  const descriptorsByKey = new Map(corpusDescriptors.map((descriptor) => [
    corpusInteger(descriptor.descriptorKey, 'Combat sprite descriptor key'), descriptor
  ]));
  const payload = placement.payload;
  const orthographicTableOffset = readU16(payload,
    MODE_TWO_STAGE_ORTHOGRAPHIC_HEADER_ENTRY * 2,
    'Mode-two orthographic placement table pointer');
  const perspectiveTableOffset = readU16(payload,
    MODE_TWO_STAGE_PERSPECTIVE_HEADER_ENTRY * 2,
    'Mode-two perspective placement table pointer');
  if (orthographicTableOffset !== MODE_TWO_STAGE_ORTHOGRAPHIC_TABLE_OFFSET ||
      perspectiveTableOffset !== MODE_TWO_STAGE_PERSPECTIVE_TABLE_OFFSET) {
    throw new Error('The mode-two Stage placement table headers changed.');
  }

  function placementRows(tableKind, tableOffset, rowBytes) {
    return Array.from({ length: MODE_TWO_STAGE_SELECTOR_COUNT }, (_, foregroundSelector) => {
      const listOffset = readI16(payload, tableOffset + foregroundSelector * 2,
        'Mode-two ' + tableKind + ' selector ' + foregroundSelector + ' list offset');
      let rowOffset = tableOffset + listOffset;
      const rows = [];
      for (let rowIndex = 0; rowIndex < 512; rowIndex += 1, rowOffset += rowBytes) {
        const status = readI16(payload, rowOffset,
          'Mode-two ' + tableKind + ' selector ' + foregroundSelector + ' row');
        if (status === -1) return rows;
        const rawFields = Array.from({ length: rowBytes / 2 }, (_, fieldIndex) =>
          readI16(payload, rowOffset + fieldIndex * 2,
            'Mode-two ' + tableKind + ' selector ' + foregroundSelector + ' field'));
        rows.push({ status, rowIndex, rawFields });
      }
      throw new Error('Mode-two ' + tableKind + ' selector ' + foregroundSelector +
        ' has no terminating row.');
    });
  }

  const orthographicRows = placementRows('orthographic', orthographicTableOffset,
    MODE_TWO_STAGE_ORTHOGRAPHIC_ROW_BYTES);
  const perspectiveRows = placementRows('perspective', perspectiveTableOffset,
    MODE_TWO_STAGE_PERSPECTIVE_ROW_BYTES);
  let orthographicRowCount = 0;
  let perspectiveRowCount = 0;
  let specialRowCount = 0;
  let normalRowCount = 0;
  const profiles = Array.from({ length: MODE_TWO_STAGE_SELECTOR_COUNT },
    (_, foregroundSelector) => {
      const specialRows = [];
      function compactRows(rows, tableKind) {
        return rows.flatMap((row) => {
          if (tableKind === 'orthographic') orthographicRowCount += 1;
          else perspectiveRowCount += 1;
          const id = 'mode2-stage:' + hex(foregroundSelector, 2) + ':' +
            tableKind + ':' + row.rowIndex;
          if (row.status === -2) {
            specialRowCount += 1;
            const descriptorHandle = row.rawFields[1];
            specialRows.push({
              id,
              tableKind,
              status: -2,
              descriptorHandle,
              source: modeTwoStageSource(descriptorHandle,
                descriptorTable.payload, descriptorsByKey),
              rawFields: row.rawFields,
              evidenceStatus: 'native-special-builder-row-preserved',
              renderStatus: 'withheld until the native special-status builder branch is modeled'
            });
            return [];
          }
          normalRowCount += 1;
          const values = row.rawFields;
          const source = modeTwoStageSource(row.status,
            descriptorTable.payload, descriptorsByKey);
          return [{
            id,
            projection: tableKind === 'orthographic'
              ? 'native-320x240-orthographic' : 'native-actor-perspective',
            descriptorHandle: row.status,
            rowParameter: values[1],
            poseSelector: values[2],
            x: values[3],
            y: values[4],
            z: values[5],
            depthPass: tableKind === 'orthographic'
              ? (values[5] <= -500 ? 'far' : 'near') : 'perspective',
            source,
            rawFields: values,
            evidenceStatus: 'native-static'
          }];
        });
      }
      return {
        foregroundSelector,
        coordinateSpace:
          'centered 320x240 orthographic pixels plus native Actor-camera objects',
        placementResourceKey: MODE_TWO_STAGE_PLACEMENT_RESOURCE_KEY,
        orthographicTableHeaderEntry: MODE_TWO_STAGE_ORTHOGRAPHIC_HEADER_ENTRY,
        perspectiveTableHeaderEntry: MODE_TWO_STAGE_PERSPECTIVE_HEADER_ENTRY,
        orthographicPlacements: compactRows(
          orthographicRows[foregroundSelector], 'orthographic'),
        perspectivePlacements: compactRows(
          perspectiveRows[foregroundSelector], 'perspective'),
        specialRows,
        evidenceStatus:
          'byte-exact native placement resource, row resolvers, descriptor table, and Stage builder'
      };
    });
  const nonemptySelectorCount = profiles.filter((profile) =>
    profile.orthographicPlacements.length || profile.perspectivePlacements.length ||
      profile.specialRows.length).length;
  if (orthographicRowCount !== MODE_TWO_STAGE_ORTHOGRAPHIC_ROWS ||
      perspectiveRowCount !== MODE_TWO_STAGE_PERSPECTIVE_ROWS ||
      specialRowCount !== MODE_TWO_STAGE_SPECIAL_ROWS ||
      normalRowCount !== MODE_TWO_STAGE_NORMAL_ROWS ||
      nonemptySelectorCount !== MODE_TWO_STAGE_NONEMPTY_SELECTORS) {
    throw new Error('The mode-two Stage placement census changed: orthographic=' +
      orthographicRowCount + ', perspective=' + perspectiveRowCount +
      ', normal=' + normalRowCount + ', special=' + specialRowCount +
      ', selectors=' + nonemptySelectorCount + '.');
  }
  assertModeTwoPlacementMatch(profiles[51], FORMATION_NATIVE_STAGE_PROPS,
    'Formation selector 51');
  assertModeTwoPlacementMatch(profiles[57], GRADUATION_NATIVE_STAGE_PROPS,
    'Graduation selector 57');
  return {
    profiles,
    orthographicRowCount,
    perspectiveRowCount,
    specialRowCount,
    normalRowCount,
    nonemptySelectorCount,
    placementResource: placement,
    descriptorTable
  };
}

function decodeCustomLz(source, label) {
  if (!Buffer.isBuffer(source) || source.length < 4) {
    throw new Error((label || 'Custom-LZ source') + ' is missing its size header.');
  }
  const outputSize = readU32(source, 0, label);
  if (outputSize > 16 * 1024 * 1024) {
    throw new Error((label || 'Custom-LZ source') + ' exceeds the decoded-size limit.');
  }
  const output = Buffer.alloc(outputSize);
  let sourceOffset = 4;
  let outputOffset = 0;
  function needSource(count, tokenOffset) {
    if (sourceOffset + count > source.length) {
      throw new Error((label || 'Custom-LZ source') + ' has a truncated token at +' +
        hex(tokenOffset, 4) + '.');
    }
  }
  function needOutput(count, tokenOffset) {
    if (outputOffset + count > output.length) {
      throw new Error((label || 'Custom-LZ source') + ' exceeds its declared output at +' +
        hex(tokenOffset, 4) + '.');
    }
  }
  function copyBackReference(distance, length, tokenOffset) {
    const start = outputOffset - distance - 1;
    if (start < 0) {
      throw new Error((label || 'Custom-LZ source') + ' has an invalid back-reference at +' +
        hex(tokenOffset, 4) + '.');
    }
    needOutput(length, tokenOffset);
    for (let index = 0; index < length; index += 1) {
      const readOffset = start + index;
      if (readOffset >= outputOffset + index) {
        throw new Error((label || 'Custom-LZ source') +
          ' reads unwritten output at +' + hex(tokenOffset, 4) + '.');
      }
      output[outputOffset++] = output[readOffset];
    }
  }
  while (outputOffset < output.length) {
    needSource(1, sourceOffset);
    const tokenOffset = sourceOffset;
    const control = source[sourceOffset++];
    let length;
    let distance;
    if (control & 0x80) {
      needSource(1, tokenOffset);
      distance = ((control & 7) << 8) | source[sourceOffset++];
      length = ((control >> 3) & 15) + 3;
      copyBackReference(distance, length, tokenOffset);
    } else if (control & 0x40) {
      length = (control & 63) + 1;
      needSource(length, tokenOffset);
      needOutput(length, tokenOffset);
      source.copy(output, outputOffset, sourceOffset, sourceOffset + length);
      sourceOffset += length;
      outputOffset += length;
    } else if (control & 0x20) {
      length = (control & 31) + 2;
      needOutput(length, tokenOffset);
      output.fill(0, outputOffset, outputOffset + length);
      outputOffset += length;
    } else if (control & 0x10) {
      needSource(2, tokenOffset);
      const byte1 = source[sourceOffset++];
      const byte2 = source[sourceOffset++];
      distance = ((byte1 & 63) << 8) | byte2;
      length = ((control & 15) | ((byte1 >> 2) & 48)) + 4;
      copyBackReference(distance, length, tokenOffset);
    } else if (control === 0) {
      needSource(3, tokenOffset);
      length = source[sourceOffset++] + 5;
      distance = (source[sourceOffset++] << 8) | source[sourceOffset++];
      copyBackReference(distance, length, tokenOffset);
    } else if (control === 1 || control === 2) {
      needSource(1, tokenOffset);
      length = source[sourceOffset++] + 3;
      needOutput(length, tokenOffset);
      output.fill(control === 1 ? 0xFF : 0, outputOffset, outputOffset + length);
      outputOffset += length;
    } else {
      throw new Error((label || 'Custom-LZ source') + ' has unknown control 0x' +
        hex(control, 2) + ' at +' + hex(tokenOffset, 4) + '.');
    }
  }
  if (sourceOffset !== source.length) {
    throw new Error((label || 'Custom-LZ source') +
      ' was not consumed exactly; consumed ' + sourceOffset + ' of ' + source.length + ' bytes.');
  }
  return { bytes: output, consumed: sourceOffset };
}

function wordsFromBytes(bytes, label) {
  if (!Buffer.isBuffer(bytes) || bytes.length % 4) {
    throw new Error((label || 'Director payload') + ' is not word-aligned.');
  }
  const words = new Array(bytes.length / 4);
  for (let index = 0; index < words.length; index += 1) {
    words[index] = readU32(bytes, index * 4, label);
  }
  return words;
}

function titleFromSlug(slug) {
  return String(slug || '')
    .split('-')
    .filter(Boolean)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(' ');
}

function hex(value, width) {
  return Number(value).toString(16).toUpperCase().padStart(width || 8, '0');
}

function canonicalEditPolicy(node, legacyNode, options) {
  options = options || {};
  const opcode = Number(node.opcode_u32);
  const operandValues = Array.isArray(node.operands)
    ? node.operands.map((operand) => operand.alternate_s32) : [];
  const nativeAudioBlockRequest = opcode === 0x6E &&
    operandValues[0] === 0 && operandValues[1] === 0 &&
    nativeAudioBlockIndexForRequest(operandValues[2]) !== null;
  if (nativeAudioBlockRequest) return 'typed-audio-block-request';
  if (opcode === 0x80000006 && options.nativeBackgroundGroup === true) {
    return 'typed-background-group';
  }
  if (opcode === 0x14) return 'typed-place';
  if (opcode === 0x03) return 'typed-state';
  if (opcode === 0x07) return 'typed-move';
  if (opcode === 0x2C) {
    return 'typed-projection-transform';
  }
  if (opcode === 0x46) {
    return 'typed-native-sprite-effect';
  }
  if (opcode === 0x48) {
    return 'typed-actor-opacity';
  }
  return legacyNode && legacyNode.editPolicy === 'immutable-gap'
    ? 'preserve-native' : 'preserve-native';
}

function compactCanonicalNode(node, legacyNode, options) {
  options = options || {};
  const sameLegacyBoundary = legacyNode &&
    legacyNode.startWord === node.word_start &&
    legacyNode.endWord === node.word_end_exclusive;
  const operandRoles = Array.isArray(node.operands)
    ? node.operands.map((operand) => operand.role) : [];
  if (node.termination && operandRoles.length === node.word_count - 2) {
    operandRoles.push('termination_trailer');
  }
  const output = {
    id: node.node_id,
    startWord: node.word_start,
    endWord: node.word_end_exclusive,
    wordCount: node.word_count,
    nodeType: node.node_type,
    name: node.name,
    semanticSummary: node.semantic_summary,
    confidence: node.confidence,
    opcode: node.opcode,
    opcodeU32: node.opcode_u32,
    operandRoles,
    queryRecordKind: node.query ? node.query.record_kind : null,
    terminationKind: node.termination ? node.termination.kind : null,
    runtimeReachability: node.runtime_reachability,
    newlyRecoveredFromDispatch: node.newly_recovered_from_dispatch === true,
    editPolicy: canonicalEditPolicy(node, legacyNode, options),
    insertBefore: sameLegacyBoundary && legacyNode.insertBefore === true,
    segmentId: sameLegacyBoundary && legacyNode.segmentId != null
      ? legacyNode.segmentId : null,
    segmentIds: sameLegacyBoundary && Array.isArray(legacyNode.segmentIds)
      ? legacyNode.segmentIds.slice() : [],
    unknown: false
  };
  return output;
}

function tailRecoverySummary(recovery) {
  const unknownSkipNodeCount = Object.values(recovery.unknownSkipCounts || {})
    .reduce((total, count) => total + Number(count || 0), 0);
  const knownCommandCounts = {};
  (Array.isArray(recovery.nodes) ? recovery.nodes : []).forEach((node) => {
    if (node.unknown === true || node.nodeType !== 'command') return;
    const opcode = String(node.opcode).toUpperCase().replace('0X', '0x');
    knownCommandCounts[opcode] = (knownCommandCounts[opcode] || 0) + 1;
  });
  return {
    schemaVersion: '2026-08-20.director-runtime-tail-recovery-v1',
    acceptedPrefixEndWord: recovery.acceptedPrefixEndWord,
    recoveredEndWord: recovery.recoveredEndWord,
    recoveredWordCount: recovery.recoveredWordCount,
    recoveredNodeCount: Array.isArray(recovery.nodes) ? recovery.nodes.length : 0,
    unknownSkipNodeCount,
    knownCommandCounts,
    remainingGapWordCount: recovery.remainingGapWordCount,
    recoveredTailSha256: recovery.recoveredTailSha256,
    structuralStatus:
      'complete runtime tiling; command semantics and runtime reachability remain bounded',
    runtimeRules: { ...recovery.runtimeRules }
  };
}

function applyDirectorTailRecovery(asset, recovery) {
  if (!recovery) return asset;
  if (recovery.partialResource === true || recovery.assetId !== asset.assetId) {
    throw new Error('Director tail recovery identity mismatch for ' + asset.assetId + '.');
  }
  if (recovery.decodedWordCount !== asset.decodedWordCount ||
      recovery.remainingGapWordCount !== 0 || !Array.isArray(recovery.nodes) ||
      !recovery.nodes.length) {
    throw new Error('Director tail recovery is incomplete for ' + asset.assetId + '.');
  }
  const historicalGaps = Array.isArray(asset.gaps)
    ? asset.gaps.map((gap) => ({ ...gap })) : [];
  if (historicalGaps.length !== 1 ||
      historicalGaps[0].startWord !== recovery.acceptedPrefixEndWord ||
      historicalGaps[0].endWord !== asset.decodedWordCount) {
    throw new Error('Director tail recovery does not match the historical gap for ' +
      asset.assetId + '.');
  }
  const prefixNodes = asset.nodes.filter((node) => node.endWord <= recovery.acceptedPrefixEndWord);
  let cursor = 0;
  prefixNodes.concat(recovery.nodes).forEach((node) => {
    if (node.startWord !== cursor || node.endWord - node.startWord !== node.wordCount) {
      throw new Error('Director tail recovery does not tile ' + asset.assetId +
        ' at word ' + cursor + '.');
    }
    cursor = node.endWord;
  });
  if (cursor !== asset.decodedWordCount) {
    throw new Error('Director tail recovery does not own the complete payload for ' +
      asset.assetId + '.');
  }
  return {
    ...asset,
    parseStatus: 'runtime-tiled-static',
    nodes: prefixNodes.concat(recovery.nodes),
    gaps: [],
    historicalGaps,
    tailRecovery: tailRecoverySummary(recovery)
  };
}

function recoveredMediaRequests(assetId, recovery) {
  const mediaOpcodes = new Set(['0X6E', '0X6F', '0X70', '0XB4']);
  return (recovery && Array.isArray(recovery.nodes) ? recovery.nodes : [])
    .filter((node) => mediaOpcodes.has(String(node.opcode || '').toUpperCase()))
    .map((node) => ({
      requestId: assetId + ':recovered-media:w' +
        Number(node.startWord).toString(16).toUpperCase().padStart(4, '0'),
      occurrenceId: node.id,
      wordStart: node.startWord,
      opcode: String(node.opcode).toUpperCase().replace('0X', '0x'),
      operands: Array.isArray(node.operands) ? node.operands.slice() : [],
      structuralStatus: 'exact runtime command boundary; semantic meaning remains opcode-specific'
    }));
}

function recoveredNativeSpriteEffects(assetId, recovery, selectors) {
  return (recovery && Array.isArray(recovery.nodes) ? recovery.nodes : [])
    .filter((node) => String(node.opcode || '').toUpperCase() === '0X46' &&
      Array.isArray(node.operands) && node.operands.length === 9)
    .map((node) => {
      const operands = node.operands.map((value) => Number(value));
      const bank = operands[1];
      const animationKey = operands[2];
      const facing = operands[3];
      const appearanceSelector = operands[8] & 0xFF;
      const selector = selectors.get(actorSelectorKey(bank, animationKey, facing)) || null;
      return {
        effectId: assetId + ':recovered-sprite-effect:w' +
          Number(node.startWord).toString(16).toUpperCase().padStart(4, '0'),
        occurrenceId: node.id,
        wordStart: node.startWord,
        opcode: '0x46',
        nativeEffectSlot: operands[0] & 0xFF,
        bank,
        animationKey,
        facing,
        poseId: selector ? 'cutscene-pose:' + bank + ':' + animationKey + ':' + facing : null,
        physicalStateId: selector ? selector.physicalStateId : null,
        stateIndex: selector ? selector.stateIndex : null,
        appearanceSelector,
        modelX: operands[4],
        modelY: -operands[5],
        renderPassSelector: operands[6] & 0xFF,
        scalePercent: operands[7],
        renderClosureStatus: selector && bank === 57 && appearanceSelector === 0
          ? 'exact Bank 57 physical program and single-child sprite closure'
          : (selector
            ? 'exact physical state; descriptor-qualified sprite closure remains occurrence-specific'
            : 'resolver-invalid physical-state selector')
      };
    });
}

function recoveredActorEvents(assetId, nodes, recoveredStartWord, selectors) {
  const records = new Map();
  const events = [];
  (Array.isArray(nodes) ? nodes : []).forEach((node) => {
    const opcode = String(node.opcode || '').toUpperCase();
    const operands = Array.isArray(node.operands) ? node.operands.map(Number) : [];
    const recovered = Number(node.startWord) >= recoveredStartWord;
    const controlOffset = CONTROL_ENTRY_ALIASES[node.id];
    if (opcode === '0X13' && operands.length === 1 && !controlOffset) {
      const removedSlot = operands[0];
      if (removedSlot === -1) records.clear();
      else records.delete(removedSlot);
      return;
    }
    if (!['0X3', '0X14'].includes(opcode)) return;
    const isPlace = opcode === '0X14';
    const slot = controlOffset ? null : operands[0];
    const bank = controlOffset ? null : operands[1];
    const animationKey = controlOffset ? null : operands[2];
    const facing = controlOffset ? null : operands[3];
    const rawAppearanceSelector = controlOffset ? null : operands[isPlace ? 8 : 7];
    const selector = controlOffset ? null :
      selectors.get(actorSelectorKey(bank, animationKey, facing)) || null;
    const recordBefore = slot == null ? null : records.get(slot) || null;
    let appearanceSelector = null;
    let eventStatus;
    if (controlOffset) {
      eventStatus = 'control-overlap';
    } else if (isPlace) {
      appearanceSelector = rawAppearanceSelector & 0xFF;
      records.set(slot, selector ? {
        bank, animationKey, facing, appearanceSelector,
        physicalStateId: selector.physicalStateId,
        stateIndex: selector.stateIndex
      } : { bank, animationKey, facing, appearanceSelector });
      eventStatus = selector ? 'applied' : 'resolver-invalid-record-created';
    } else if (!recordBefore) {
      eventStatus = 'no-record';
    } else if (!selector) {
      eventStatus = 'resolver-invalid';
    } else {
      appearanceSelector = rawAppearanceSelector === -1
        ? recordBefore.appearanceSelector : rawAppearanceSelector & 0xFF;
      records.set(slot, {
        bank, animationKey, facing, appearanceSelector,
        physicalStateId: selector.physicalStateId,
        stateIndex: selector.stateIndex
      });
      eventStatus = 'applied';
    }
    if (!recovered) return;
    const recordAfter = slot == null ? null : records.get(slot) || null;
    events.push({
      eventId: assetId + ':recovered-actor:w' +
        Number(node.startWord).toString(16).toUpperCase().padStart(4, '0'),
      occurrenceId: node.id,
      wordStart: node.startWord,
      opcode: isPlace ? '0x14' : '0x03',
      slot,
      bank,
      animationKey,
      facing,
      rawAppearanceSelector,
      appearanceSelector,
      poseId: selector ? 'cutscene-pose:' + bank + ':' + animationKey + ':' + facing : null,
      physicalStateId: selector ? selector.physicalStateId : null,
      stateIndex: selector ? selector.stateIndex : null,
      sourceProgramDefined: selector ? selector.sourceProgramDefined === true : false,
      x: controlOffset || operands[4] === -1000 ? null : operands[4] / 1000,
      y: controlOffset || operands[5] === -1000 ? null : operands[5] / 1000,
      z: controlOffset || operands[6] === -1000 ? null : operands[6] / 1000,
      recordAvailableBefore: controlOffset ? null : recordBefore !== null,
      recordAvailableAfter: controlOffset ? null : recordAfter !== null,
      eventStatus,
      controlEntryAlias: controlOffset ? {
        entryWord: node.startWord + controlOffset,
        offsetWords: controlOffset,
        disposition: 'preserve bytes; actor interpretation withheld'
      } : null,
      rawOperands: operands,
      renderClosureStatus: eventStatus === 'applied'
        ? 'exact physical ROM program and descriptor-qualified sprite closure'
        : (eventStatus === 'control-overlap'
          ? 'not an actor command on the exact control-entry path'
          : (eventStatus === 'no-record'
            ? 'valid selector cannot apply without a same-stream actor record'
            : 'actor record remains at its previous valid visual state'))
    });
  });
  return events;
}

function recoveredActorInitializations(asset, selectors, actors) {
  if (!asset.tailRecovery) return [];
  const occupied = new Set(actors.map((actor) => actor.slot));
  const rawIdentity = asset.assetId.split(':').pop().toLowerCase();
  return recoveredActorEvents(
    asset.assetId, asset.nodes, asset.tailRecovery.acceptedPrefixEndWord, selectors
  ).filter((event) => event.opcode === '0x14' && event.eventStatus === 'applied' &&
    !occupied.has(event.slot)).filter((event, index, rows) =>
    rows.findIndex((candidate) => candidate.slot === event.slot) === index).map((event) => {
    occupied.add(event.slot);
    return {
      actorId: 'actor:' + rawIdentity + ':slot:' + String(event.slot).padStart(2, '0'),
      slot: event.slot,
      label: 'Actor slot ' + event.slot,
      bank: event.bank,
      animationKey: event.animationKey,
      facing: event.facing,
      x: event.x,
      y: event.y,
      z: event.z,
      poseResolutionId: event.poseId,
      sourcePoseResolutionId: null,
      physicalStateId: event.physicalStateId,
      stateIndex: event.stateIndex,
      sourceProgramDefined: event.sourceProgramDefined,
      initializationCandidateId: null,
      initializationSourceOpcode: '0X14',
      initializationStatus: 'exact recovered opcode-0x14 record producer',
      controlEntryAlias: null,
      recordProducer: true,
      variantSelector: event.appearanceSelector,
      rawVariantSelector: event.rawAppearanceSelector,
      variantSelectorStatus: 'exact recovered opcode-0x14 appearance byte',
      selectorStatus: 'physical state and exact ROM program located',
      visibilityStatus:
        'visual preview from exact recovered record allocation; no dedicated logical show flag is proved'
    };
  });
}

function actorSelectorKey(bank, animationKey, facing) {
  return [bank, animationKey, facing].join(':');
}

function poseSelectorMap(rows) {
  const selectors = new Map();
  rows.forEach((row) => {
    const key = actorSelectorKey(row.bank, row.key, row.facing);
    if (selectors.has(key)) throw new Error('Duplicate all-bank pose selector ' + key + '.');
    selectors.set(key, row);
  });
  return selectors;
}

function compactActor(actor, nodes, selectors) {
  const candidate = actor.initializationCandidate || {};
  const sourceNode = (nodes || []).find((node) =>
    node.startWord === candidate.source_word_start);
  const discoveryWords = actor.acceptedTrack &&
    Array.isArray(actor.acceptedTrack.discovery_word_starts)
    ? actor.acceptedTrack.discovery_word_starts : [];
  const controlEntryNode = sourceNode && CONTROL_ENTRY_ALIASES[sourceNode.id]
    ? sourceNode
    : (nodes || []).find((node) => discoveryWords.includes(node.startWord) &&
      CONTROL_ENTRY_ALIASES[node.id]);
  const sourceOpcode = sourceNode ? String(sourceNode.opcode).toUpperCase() : null;
  const controlEntryAlias = controlEntryNode
    ? {
      entryWord: controlEntryNode.startWord + CONTROL_ENTRY_ALIASES[controlEntryNode.id],
      offsetWords: CONTROL_ENTRY_ALIASES[controlEntryNode.id]
    } : null;
  const recordProducer = sourceOpcode === '0X14' && !controlEntryAlias;
  const variantOperandIndex = sourceOpcode === '0X14' ? 8 :
    (sourceOpcode === '0X3' ? 7 : null);
  const rawVariantSelector = sourceNode && Array.isArray(sourceNode.operands) &&
    variantOperandIndex != null && Number.isInteger(sourceNode.operands[variantOperandIndex])
    ? sourceNode.operands[variantOperandIndex] : null;
  const variantSelector = recordProducer && Number.isInteger(rawVariantSelector)
    ? rawVariantSelector & 0xFF : null;
  const selector = recordProducer && Number.isInteger(actor.bank) &&
    Number.isInteger(actor.key) && Number.isInteger(actor.facing)
    ? selectors.get(actorSelectorKey(actor.bank, actor.key, actor.facing)) || null
    : null;
  const initializationStatus = controlEntryAlias
    ? 'source node overlaps an exact control entry; actor interpretation withheld'
    : (!sourceNode
      ? 'no record producer or source command located'
      : (!recordProducer
      ? 'source command requires a pre-existing actor record'
      : (selector
        ? 'opcode 0x14 record producer with located physical state'
        : 'opcode 0x14 record producer with resolver-invalid selector')));
  function actorCoordinate(field, operandIndex) {
    const rawOperand = sourceNode && Array.isArray(sourceNode.operands)
      ? sourceNode.operands[operandIndex] : null;
    if (sourceOpcode === '0X3' && rawOperand === -1000) return null;
    return actor[field] == null ? null : actor[field];
  }
  return {
    actorId: actor.actorId,
    slot: actor.slot,
    label: actor.label || null,
    bank: actor.bank == null ? null : actor.bank,
    animationKey: actor.key == null ? null : actor.key,
    facing: actor.facing == null ? null : actor.facing,
    x: actorCoordinate('x', 4),
    y: actorCoordinate('y', 5),
    z: actorCoordinate('z', 6),
    poseResolutionId: selector
      ? 'cutscene-pose:' + actor.bank + ':' + actor.key + ':' + actor.facing : null,
    sourcePoseResolutionId: candidate.pose_resolution_id || null,
    physicalStateId: selector ? selector.physicalStateId : null,
    stateIndex: selector ? selector.stateIndex : null,
    sourceProgramDefined: selector ? selector.sourceProgramDefined === true : false,
    initializationCandidateId: candidate.candidate_id || null,
    initializationSourceOpcode: sourceOpcode,
    initializationStatus,
    controlEntryAlias,
    recordProducer,
    variantSelector,
    rawVariantSelector,
    variantSelectorStatus: variantSelector == null
      ? (recordProducer ? 'opcode 0x14 appearance operand unavailable' : 'no record-producing appearance value')
      : 'exact opcode-0x14 operand 9 narrowed to its runtime byte',
    selectorStatus: selector
      ? (selector.sourceProgramDefined
        ? 'physical state and source program located'
        : 'exact physical ROM state located; scene-local source publication is absent')
      : (recordProducer ? 'record produced but selector resolver fails' : 'no record-producing selector'),
    visibilityStatus: actor.visibilityStatus || 'unresolved'
  };
}

function archiveAssetIds(indices) {
  return (indices || []).map((index) => 'archive:' + index);
}

function sceneGroupMembers(selector) {
  return SCENE_GROUP_ASSET_ROWS.filter((row) => row[0] === selector)
    .map((row) => ({ ordinal: row[1], assetId: row[2] }));
}

function sceneGroupAssetIds(selector) {
  return sceneGroupMembers(selector).map((member) => member.assetId);
}

function readOversizedImagePresentationRules(z64, archiveCatalog) {
  const tableBytes = z64.subarray(CLASS_EVOLUTION_MEDIA_TABLE_Z64,
    CLASS_EVOLUTION_MEDIA_TABLE_Z64 +
      CLASS_EVOLUTION_MEDIA_ROW_COUNT * CLASS_EVOLUTION_MEDIA_ROW_BYTES);
  if (tableBytes.length !==
      CLASS_EVOLUTION_MEDIA_ROW_COUNT * CLASS_EVOLUTION_MEDIA_ROW_BYTES ||
      sha256(tableBytes) !== CLASS_EVOLUTION_MEDIA_TABLE_SHA256) {
    throw new Error('The resident class-evolution media rows changed.');
  }

  const root = nativeResourceEnvelope(z64, OVERSIZED_IMAGE_ROOT_KEY,
    'Oversized-image root');
  if (root.storedLength !== OVERSIZED_IMAGE_ROOT_CHILD_COUNT * 4 ||
      sha256(root.payload) !== OVERSIZED_IMAGE_ROOT_PAYLOAD_SHA256) {
    throw new Error('The oversized-image child root changed.');
  }

  const archiveByHeader = new Map(archiveCatalog.map((archive) => [
    parseInt(archive.romOffset, 16), archive
  ]));
  const children = [];
  for (let childSelector = 0;
      childSelector < OVERSIZED_IMAGE_ROOT_CHILD_COUNT; childSelector += 1) {
    const resourceKey = readU32(root.payload, childSelector * 4,
      'Oversized-image child ' + childSelector);
    if (resourceKey === 0) {
      children.push({
        childSelector,
        resourceKey: null,
        resourcePrefixZ64: null,
        archiveIndex: null,
        assetId: null,
        filename: null,
        disposition: 'native-null-child'
      });
      continue;
    }
    const resourcePrefixZ64 = DIRECTOR_RESOURCE_BASE_Z64 + resourceKey;
    const archive = archiveByHeader.get(resourcePrefixZ64 + 4);
    if (!archive) {
      throw new Error('Oversized-image child ' + childSelector +
        ' does not resolve to a catalogued archive header.');
    }
    children.push({
      childSelector,
      resourceKey,
      resourcePrefixZ64,
      archiveIndex: archive.index,
      assetId: 'archive:' + archive.index,
      filename: archive.filename,
      disposition: 'exact-native-resource-child'
    });
  }
  const populated = children.filter((child) => child.assetId !== null);
  const nullChildren = children.filter((child) => child.assetId === null)
    .map((child) => child.childSelector);
  if (populated.length !== 38 || nullChildren.join(',') !== '0,34,37' ||
      populated[0].archiveIndex !== 120 ||
      populated[populated.length - 1].archiveIndex !== 157 ||
      populated.some((child, index) => child.archiveIndex !== 120 + index)) {
    throw new Error('Oversized-image archive ownership changed.');
  }

  const rows = [];
  for (let rowSelector = 0;
      rowSelector < CLASS_EVOLUTION_MEDIA_ROW_COUNT; rowSelector += 1) {
    const mediaSelector = tableBytes.readInt8(
      rowSelector * CLASS_EVOLUTION_MEDIA_ROW_BYTES +
        CLASS_EVOLUTION_MEDIA_SELECTOR_OFFSET);
    const childSelector = mediaSelector - 4;
    const child = childSelector >= 0 && childSelector < children.length
      ? children[childSelector] : null;
    rows.push({
      rowSelector,
      mediaSelector,
      childSelector,
      resourceKey: child && child.resourceKey || null,
      archiveIndex: child && child.archiveIndex !== null ? child.archiveIndex : null,
      assetId: child && child.assetId || null,
      disposition: !child ? 'child-selector-outside-root' : child.disposition
    });
  }
  return {
    classTableZ64: CLASS_EVOLUTION_MEDIA_TABLE_Z64,
    classTableRowCount: CLASS_EVOLUTION_MEDIA_ROW_COUNT,
    classTableRowBytes: CLASS_EVOLUTION_MEDIA_ROW_BYTES,
    classTableSelectorOffset: CLASS_EVOLUTION_MEDIA_SELECTOR_OFFSET,
    classTableSha256: CLASS_EVOLUTION_MEDIA_TABLE_SHA256,
    rootResourceKey: OVERSIZED_IMAGE_ROOT_KEY,
    rootPrefixZ64: root.prefixStart,
    rootPayloadZ64: root.payloadStart,
    rootPayloadSha256: OVERSIZED_IMAGE_ROOT_PAYLOAD_SHA256,
    children,
    rows
  };
}

function signedHalfword(value) {
  return Number(value) << 16 >> 16;
}

function oversizedImagePresentationProfile(asset, launchContext,
    parentEventLaunches, rules) {
  if (!launchContext || launchContext.classId !== 4) return null;
  const nodes = (asset.nodes || []).filter((node) =>
    String(node.opcode).toUpperCase() === '0X80000007');
  if (nodes.length > 1) {
    throw new Error(asset.assetId +
      ' has more than one oversized-image launch-row selector.');
  }

  const contextRows = (parentEventLaunches || []).flatMap((launch) =>
    (launch.eventInvocationContexts || []).map((context) => ({
      launchId: launch.launchId,
      eventDirectoryRow: launch.eventDirectoryRow,
      eventEntryCursor: launch.eventEntryCursor,
      eventInvocationCursor: context.eventInvocationCursor,
      rowSelector: Number.isInteger(eventContextProperty(context, 0xE9))
        ? eventContextProperty(context, 0xE9) : null
    })));
  const exactContextRows = contextRows.map((context) => context.rowSelector)
    .filter(Number.isInteger);
  const exactContextRowSet = new Set(exactContextRows);
  const node = nodes[0] || null;
  const rawRowSelector = node && Array.isArray(node.operands) &&
      Number.isInteger(node.operands[0]) ? node.operands[0] : null;
  const rowSelector = node ? signedHalfword(rawRowSelector) :
    (contextRows.length > 0 && exactContextRows.length === contextRows.length &&
      exactContextRowSet.size === 1 ? exactContextRows[0] : null);
  const row = Number.isInteger(rowSelector) && rowSelector >= 0 &&
      rowSelector < rules.rows.length ? rules.rows[rowSelector] : null;
  const source = node ? 'director-launch-prescan-opcode-0x80000007' :
    (row ? 'event-property-0xE9-fallback' : 'event-property-0xE9-unresolved');
  return {
    active: true,
    source,
    sourceNodeId: node ? node.id : null,
    wordStart: node ? node.startWord : null,
    rawRowSelector,
    rowSelector,
    mediaSelector: row ? row.mediaSelector : null,
    childSelector: row ? row.childSelector : null,
    resourceKey: row ? row.resourceKey : null,
    archiveIndex: row ? row.archiveIndex : null,
    assetId: row ? row.assetId : null,
    contextCount: contextRows.length,
    exactContextCount: exactContextRows.length,
    contexts: contextRows,
    evidenceStatus: row && row.assetId
      ? 'native-static-exact'
      : (row ? row.disposition : 'launch-inputs-unresolved'),
    status: row && row.assetId
      ? 'The class-4 launch row selects this exact oversized-image archive through the resident class-evolution table and resource root.'
      : (row
        ? 'The native launch row selects an empty or invalid oversized-image child.'
        : 'The class-4 owner falls back to event property 0xE9, which is unresolved for this launch context.'),
    initialView: { x: 0, y: 0, scale: 1, zoomState: 4 }
  };
}

function readModeTwoDerivedEnvironmentRules(z64) {
  function readTable(start, length, expectedSha256, label) {
    const bytes = z64.subarray(start, start + length);
    if (bytes.length !== length || sha256(bytes) !== expectedSha256) {
      throw new Error(label + ' no longer matches the retail ROM.');
    }
    return Array.from(bytes);
  }
  const signedTerrain = readTable(
    MODE_TWO_SIGNED_TERRAIN_TABLE_Z64,
    MODE_TWO_DERIVED_TERRAIN_TABLE_BYTES,
    MODE_TWO_DERIVED_TERRAIN_TABLE_SHA256,
    'Signed derived-environment terrain table');
  const unsignedTerrain = readTable(
    MODE_TWO_UNSIGNED_TERRAIN_TABLE_Z64,
    MODE_TWO_DERIVED_TERRAIN_TABLE_BYTES,
    MODE_TWO_DERIVED_TERRAIN_TABLE_SHA256,
    'Unsigned derived-environment terrain table');
  const signedScenario = readTable(
    MODE_TWO_SIGNED_SCENARIO_TABLE_Z64,
    MODE_TWO_DERIVED_SCENARIO_TABLE_BYTES,
    MODE_TWO_DERIVED_SCENARIO_TABLE_SHA256,
    'Signed derived-environment scenario table');
  const unsignedScenario = readTable(
    MODE_TWO_UNSIGNED_SCENARIO_TABLE_Z64,
    MODE_TWO_DERIVED_SCENARIO_TABLE_BYTES,
    MODE_TWO_DERIVED_SCENARIO_TABLE_SHA256,
    'Unsigned derived-environment scenario table');
  if (signedTerrain.some((value, index) => value !== unsignedTerrain[index]) ||
      signedScenario.some((value, index) => value !== unsignedScenario[index])) {
    throw new Error('The paired derived-environment lookup tables no longer match.');
  }
  function mapper(mapperId, functionZ64, terrainTableZ64, scenarioTableZ64,
      signedScenarioBytes) {
    return {
      mapperId,
      functionZ64,
      terrainTableZ64,
      terrainTableSha256: MODE_TWO_DERIVED_TERRAIN_TABLE_SHA256,
      terrainRows: Array.from({ length: signedTerrain.length / 4 }, (_, index) =>
        signedTerrain.slice(index * 4, index * 4 + 4)),
      scenarioTableZ64,
      scenarioTableSha256: MODE_TWO_DERIVED_SCENARIO_TABLE_SHA256,
      scenarioValues: signedScenario.slice(),
      signedScenarioBytes
    };
  }
  return {
    evidenceStatus: 'native-static-launch-mappers',
    selectorConversion: 'one-based mapper output minus one',
    inputProperties: {
      scenarioKey: 0xE9,
      currentUnitSelector: 0xFD,
      battleTerrain: 0xFC
    },
    randomChoiceSourceRam: '0x8009C7CC',
    scenarioOverrides: MODE_TWO_DERIVED_SCENARIO_OVERRIDES.map((row) => ({
      scenarioKey: row[0],
      currentUnitSelector: row[1],
      nativeEnvironmentNumber: row[2],
      environmentSelector: row[2] - 1
    })),
    mappers: [
      mapper('signed-direct-loader', '0x000670DC',
        MODE_TWO_SIGNED_TERRAIN_TABLE_Z64, MODE_TWO_SIGNED_SCENARIO_TABLE_Z64, true),
      mapper('unsigned-existing-context-loader', '0x00067600',
        MODE_TWO_UNSIGNED_TERRAIN_TABLE_Z64,
        MODE_TWO_UNSIGNED_SCENARIO_TABLE_Z64, false)
    ],
    status: 'The launch mappers consume event properties 0xE9 and 0xFD, battle terrain, and selected native query or unit-record state. Their one-based result is decremented before environment-table lookup.'
  };
}

function modeTwoDerivedMapper(rules, mapperId) {
  const mapper = rules.mappers.find((row) => row.mapperId === mapperId);
  if (!mapper) throw new Error('Unknown derived-environment mapper ' + mapperId + '.');
  return mapper;
}

function eventContextProperty(context, propertyOperand) {
  const row = (context.eventPropertyValues || []).find((property) =>
    property.propertyOperand === propertyOperand);
  return row ? row.value : null;
}

function exactModeTwoDerivedEnvironment(rules, mapperId, inputs) {
  const mapper = modeTwoDerivedMapper(rules, mapperId);
  const scenarioKey = inputs.scenarioKey;
  const currentUnitSelector = inputs.currentUnitSelector;
  const battleTerrain = inputs.battleTerrain;
  const randomChoice = inputs.randomChoice;
  if (Number.isInteger(scenarioKey) && Number.isInteger(currentUnitSelector)) {
    const special = rules.scenarioOverrides.find((row) =>
      row.scenarioKey === scenarioKey &&
      row.currentUnitSelector === currentUnitSelector);
    if (special) {
      return {
        nativeEnvironmentNumber: special.nativeEnvironmentNumber,
        source: 'scenario-unit-override'
      };
    }
    if (currentUnitSelector === 30 && scenarioKey >= 0 &&
        scenarioKey < mapper.scenarioValues.length) {
      const rawValue = mapper.scenarioValues[scenarioKey];
      const nativeEnvironmentNumber = mapper.signedScenarioBytes
        ? eventSigned8(rawValue) : rawValue;
      if (nativeEnvironmentNumber !== -1) {
        return {
          nativeEnvironmentNumber,
          source: mapper.signedScenarioBytes
            ? 'signed-scenario-table' : 'unsigned-scenario-table'
        };
      }
    }
  }
  if (!Number.isInteger(battleTerrain)) return null;
  const terrain = battleTerrain & 0xFF;
  if (terrain === 100) {
    return { nativeEnvironmentNumber: 24, source: 'terrain-100' };
  }
  if ((terrain & 0x80) !== 0) return null;
  if (terrain === 15 || terrain === 19) {
    return { nativeEnvironmentNumber: 30, source: 'terrain-15-or-19' };
  }
  const rowIndex = Math.floor((terrain - 1) / 26);
  if (rowIndex < 0 || rowIndex >= mapper.terrainRows.length ||
      !Number.isInteger(randomChoice)) return null;
  const choice = ((randomChoice % 4) + 4) % 4;
  return {
    nativeEnvironmentNumber: mapper.terrainRows[rowIndex][choice],
    source: 'terrain-four-choice-table'
  };
}

function modeTwoDerivedEnvironmentCandidates(rules, mapperId, inputs) {
  const exact = exactModeTwoDerivedEnvironment(rules, mapperId, inputs);
  if (exact) {
    const selector = exact.nativeEnvironmentNumber - 1;
    return {
      nativeEnvironmentNumbers: [exact.nativeEnvironmentNumber],
      environmentSelectors: selector >= 0 && selector < 80 ? [selector] : [],
      outOfRangeEnvironmentSelectors: selector >= 0 && selector < 80 ? [] : [selector]
    };
  }
  const mapper = modeTwoDerivedMapper(rules, mapperId);
  const outputs = new Set();
  function addOutput(value) {
    if (Number.isInteger(value)) outputs.add(value);
  }
  const scenarioKey = inputs.scenarioKey;
  const currentUnitSelector = inputs.currentUnitSelector;
  rules.scenarioOverrides.forEach((row) => {
    if ((scenarioKey === null || row.scenarioKey === scenarioKey) &&
        (currentUnitSelector === null ||
          row.currentUnitSelector === currentUnitSelector)) {
      addOutput(row.nativeEnvironmentNumber);
    }
  });
  if (currentUnitSelector === null || currentUnitSelector === 30) {
    if (Number.isInteger(scenarioKey) && scenarioKey >= 0 &&
        scenarioKey < mapper.scenarioValues.length) {
      const rawValue = mapper.scenarioValues[scenarioKey];
      const value = mapper.signedScenarioBytes ? eventSigned8(rawValue) : rawValue;
      if (value !== -1) addOutput(value);
    } else if (scenarioKey === null) {
      mapper.scenarioValues.forEach((rawValue) => {
        const value = mapper.signedScenarioBytes ? eventSigned8(rawValue) : rawValue;
        if (value !== -1) addOutput(value);
      });
    }
  }
  const battleTerrain = inputs.battleTerrain;
  if (Number.isInteger(battleTerrain)) {
    const terrain = battleTerrain & 0xFF;
    if (terrain === 100) {
      addOutput(24);
    } else if ((terrain & 0x80) !== 0) {
      [21, 22, 24, 40, 59].forEach(addOutput);
    } else if (terrain === 15 || terrain === 19) {
      addOutput(30);
    } else {
      const rowIndex = Math.floor((terrain - 1) / 26);
      if (rowIndex >= 0 && rowIndex < mapper.terrainRows.length) {
        mapper.terrainRows[rowIndex].forEach(addOutput);
      }
    }
  } else {
    mapper.terrainRows.flat().forEach(addOutput);
    [21, 22, 24, 30, 40, 59].forEach(addOutput);
  }
  const nativeEnvironmentNumbers = Array.from(outputs).sort((left, right) => left - right);
  const selectors = nativeEnvironmentNumbers.map((value) => value - 1);
  return {
    nativeEnvironmentNumbers,
    environmentSelectors: selectors.filter((selector) => selector >= 0 && selector < 80),
    outOfRangeEnvironmentSelectors:
      selectors.filter((selector) => selector < 0 || selector >= 80)
  };
}

function modeTwoDerivedEnvironmentProfile(parentEventLaunches, rules) {
  const mapperId = 'unsigned-existing-context-loader';
  const contexts = (parentEventLaunches || []).flatMap((launch) =>
    (launch.eventInvocationContexts || []).map((context) => {
      const inputs = {
        scenarioKey: eventContextProperty(context, 0xE9),
        currentUnitSelector: eventContextProperty(context, 0xFD),
        battleTerrain: eventContextProperty(context, 0xFC),
        randomChoice: null,
        unitRecordFlags: null,
        auxiliaryHighTerrainState: null
      };
      const exact = exactModeTwoDerivedEnvironment(rules, mapperId, inputs);
      const candidates = modeTwoDerivedEnvironmentCandidates(rules, mapperId, inputs);
      const selector = exact ? exact.nativeEnvironmentNumber - 1 : null;
      return {
        launchId: launch.launchId,
        eventDirectoryRow: launch.eventDirectoryRow,
        eventEntryCursor: launch.eventEntryCursor,
        eventInvocationCursor: context.eventInvocationCursor,
        mapperId,
        inputs,
        nativeEnvironmentNumber: exact ? exact.nativeEnvironmentNumber : null,
        environmentSelector: selector,
        resolutionSource: exact ? exact.source : null,
        environmentSelectorCandidates: candidates.environmentSelectors,
        outOfRangeEnvironmentSelectorCandidates:
          candidates.outOfRangeEnvironmentSelectors,
        resolutionStatus: exact ? 'exact-native-mapper-result' : 'launch-inputs-unresolved',
        evidenceStatus: 'native-static-event-vm-and-launch-mapper'
      };
    }));
  const contextCandidates = contexts.length ? contexts.map((context) => ({
    environmentSelectors: context.environmentSelectorCandidates,
    outOfRangeEnvironmentSelectors: context.outOfRangeEnvironmentSelectorCandidates
  })) : [modeTwoDerivedEnvironmentCandidates(rules, mapperId, {
    scenarioKey: null,
    currentUnitSelector: null,
    battleTerrain: null,
    randomChoice: null
  })];
  const environmentSelectorCandidates = Array.from(new Set(contextCandidates.flatMap((row) =>
    row.environmentSelectors))).sort((left, right) => left - right);
  const outOfRangeEnvironmentSelectorCandidates = Array.from(new Set(
    contextCandidates.flatMap((row) => row.outOfRangeEnvironmentSelectors)))
    .sort((left, right) => left - right);
  const exactSelectors = contexts.map((context) => context.environmentSelector)
    .filter((selector) => selector !== null);
  const exactSelectorSet = new Set(exactSelectors);
  const environmentSelector = contexts.length > 0 &&
      exactSelectors.length === contexts.length && exactSelectorSet.size === 1
    ? exactSelectors[0] : null;
  return {
    mapperId,
    mapperFunctionZ64: modeTwoDerivedMapper(rules, mapperId).functionZ64,
    selectorConversion: rules.selectorConversion,
    contextCount: contexts.length,
    exactContextCount: exactSelectors.length,
    unresolvedContextCount: contexts.length - exactSelectors.length,
    environmentSelector,
    environmentSelectorCandidates,
    outOfRangeEnvironmentSelectorCandidates,
    requiredInputs: [
      'event-property-0xE9-scenario-key',
      'event-property-0xFD-current-unit-selector',
      'event-property-0xFC-battle-terrain',
      'four-choice-query-or-high-terrain-unit-state'
    ],
    contexts,
    evidenceStatus: environmentSelector === null
      ? 'native-static-launch-inputs-unresolved' : 'native-static-exact',
    status: environmentSelector === null
      ? 'The native mapper is exact, but this launch lacks enough static scenario, unit, terrain, or query state to select one environment.'
      : 'Every reachable launch context produces the same exact environment selector.'
  };
}

function backgroundRequestsForAsset(asset, runtimeObservation, launchContext) {
  const launchClass = launchContext && Number.isInteger(launchContext.classId)
    ? launchContext.classId : null;
  const classPreloadsSceneGroup = launchClass === 4 || launchClass === 5;
  return (asset.nodes || []).filter((node) =>
    String(node.opcode).toUpperCase() === '0X80000006').map((node) => {
    const operand = Array.isArray(node.operands) && Number.isInteger(node.operands[0])
      ? node.operands[0] : null;
    const inSceneTable = Number.isInteger(operand) && operand >= 0 &&
      operand < SCENE_BACKGROUND_GROUP_KEYS.length;
    const inModeTwoEnvironmentTable = Number.isInteger(operand) && operand >= 0 &&
      operand < MODE_TWO_ENVIRONMENT_RESOURCE_KEYS.length;
    const sceneMembers = inSceneTable ? sceneGroupMembers(operand) : [];
    return {
      requestId: asset.assetId + ':background-request:w' +
        Number(node.startWord).toString(16).toUpperCase().padStart(4, '0'),
      nodeId: node.id,
      wordStart: node.startWord,
      commandOperand: operand,
      modeStatus: runtimeObservation && Number.isInteger(runtimeObservation.directorMode)
        ? 'runtime-observed Director mode ' + runtimeObservation.directorMode
        : 'runtime Director mode byte 0x8018FC19 unresolved for this asset',
      launchPreloadRoute: {
        terminalClass: launchClass,
        condition: 'terminal trailer class equals 4 or 5',
        active: classPreloadsSceneGroup,
        commandOperandDisposition: classPreloadsSceneGroup && inSceneTable
          ? 'exact scene-table selector' : 'not selected by this terminal class',
        selector: classPreloadsSceneGroup ? operand : null,
        selectorTableId: 'background-table:scene:31',
        groupResourceKey: classPreloadsSceneGroup && inSceneTable
          ? SCENE_BACKGROUND_GROUP_KEYS[operand] : null,
        members: classPreloadsSceneGroup ? sceneMembers : [],
        archiveAssetIds: classPreloadsSceneGroup
          ? sceneMembers.map((member) => member.assetId) : [],
        associationStatus: classPreloadsSceneGroup && inSceneTable
          ? 'exact launch-time scene-group preload; ordinal zero is cached for the later non-mode-2 materializer'
          : 'terminal class does not select the scene-group preload path'
      },
      mode2Route: {
        condition: 'terminal class is not 4 or 5 and runtime mode byte equals 2',
        commandOperandDisposition: classPreloadsSceneGroup
          ? 'terminal class routes the operand to the scene-group preload'
          : (inModeTwoEnvironmentTable
          ? 'exact launch environment selector'
          : (operand === -1
            ? 'native derived-environment sentinel'
            : 'outside the 80-entry environment table')),
        selectorSource: 'func_0004ED60 output +2 seeds environment byte 0x80196AED only outside terminal classes 4 and 5; foreground scalar 0x801CEAB0 remains independently mutable',
        selectorTableId: 'background-table:mode2-environment:80',
        overlaySelectorTableId: 'background-table:mode2-overlay:80',
        associationStatus: classPreloadsSceneGroup
          ? 'inactive because the launch pre-scan selected the scene-group preload'
          : (inModeTwoEnvironmentTable
          ? 'exact launch environment; final foreground depends on an external mode-2 flag'
          : (operand === -1
            ? 'environment and foreground are derived from launch state'
            : 'mode-2 launch route is outside the located selector table'))
      },
      nonMode2Route: {
        condition: 'mode byte does not equal 2',
        commandOperandDisposition: inSceneTable
          ? 'exact scene-table selector' : 'outside the 31-entry scene table',
        selector: operand,
        selectorTableId: 'background-table:scene:31',
        groupResourceKey: inSceneTable ? SCENE_BACKGROUND_GROUP_KEYS[operand] : null,
        members: sceneMembers,
        archiveAssetIds: sceneMembers.map((member) => member.assetId),
        associationStatus: !inSceneTable
          ? 'non-mode-2 path would index outside the located table'
          : (sceneMembers.length
            ? 'conditional exact static group membership'
            : 'valid empty scene group')
      }
    };
  });
}

function directorOpcodeSet(asset, canonicalNodes) {
  const nodes = Array.isArray(canonicalNodes) && canonicalNodes.length
    ? canonicalNodes : (asset.nodes || []);
  return new Set(nodes.map((node) => {
    const value = Number.isInteger(node.opcode_u32)
      ? node.opcode_u32 : Number(node.opcode);
    return Number.isFinite(value) ? value >>> 0 : null;
  }));
}

function launchTranslationIndex(word) {
  const value = Number(word) >>> 0;
  return (value & 0xFFFFFF00) === 0x08880000 ? value & 0xFF : null;
}

function launchOperandTranslationProfile(nodes, parentEventLaunches) {
  const occurrences = [];
  (nodes || []).forEach((node) => {
    const words = Array.isArray(node.rawWords)
      ? node.rawWords : [node.opcode_u32].concat(node.operands || []);
    words.forEach((word, wordOffset) => {
      const tableIndex = launchTranslationIndex(word);
      if (tableIndex === null) return;
      occurrences.push({
        nodeId: node.id,
        wordStart: Number.isInteger(node.startWord) ? node.startWord : node.word_start,
        wordOffset,
        sourceWord: (Number.isInteger(node.startWord) ? node.startWord : node.word_start) +
          wordOffset,
        tableIndex,
        operandRole: wordOffset === 0 ? 'opcode' :
          ((node.definition && node.definition.operandRoles || [])[wordOffset - 1] ||
            'operand_' + (wordOffset - 1))
      });
    });
  });
  const tableIndexes = Array.from(new Set(occurrences.map((row) => row.tableIndex)))
    .sort((left, right) => left - right);
  const launchContexts = tableIndexes.length === 0 ? [] :
    (parentEventLaunches || []).flatMap((launch) =>
      (launch.eventInvocationContexts || []).map((context) => ({
        launchId: launch.launchId,
        eventInvocationCursor: context.eventInvocationCursor,
        precedingDirectorLaunchCount: context.precedingDirectorLaunchCount,
        tableValues: tableIndexes.map((tableIndex) =>
          context.launchTranslationTable[tableIndex]),
        evidenceStatus: 'native-static-event-vm'
      })));
  const resolvedContextCount = launchContexts.filter((context) =>
    context.tableValues.every((value) => value !== null)).length;
  return {
    required: occurrences.length > 0,
    placeholderCount: occurrences.length,
    tableIndexes,
    occurrences,
    launchContexts,
    resolvedContextCount,
    unresolvedContextCount: launchContexts.length - resolvedContextCount,
    evidenceStatus: occurrences.length
      ? 'native-static-loader-and-event-vm' : 'not-required',
    status: occurrences.length
      ? 'The native Director loader replaces these operands from its launch-populated halfword table before parsing. Event-VM constants are attached per invocation; unresolved values remain explicit preview inputs.'
      : 'This Director resource contains no launch-translation placeholders.'
  };
}

function directorLaunchMode(asset, runtimeObservation, canonicalNodes,
    parentEventLaunches, launchContext) {
  if (runtimeObservation && Number.isInteger(runtimeObservation.directorMode)) {
    return {
      value: runtimeObservation.directorMode,
      evidenceStatus: 'runtime-observed',
      source: 'stored Project64 scene state',
      status: 'Stored scene state supplies the Director launch mode.'
    };
  }
  const opcodes = directorOpcodeSet(asset, canonicalNodes);
  const modeZeroMarkers = MODE_ZERO_ONLY_DIRECTOR_OPCODES.filter((opcode) =>
    opcodes.has(opcode));
  const modeTwoMarkers = MODE_TWO_ONLY_DIRECTOR_OPCODES.filter((opcode) =>
    opcodes.has(opcode));
  if (launchContext && (launchContext.classId === 4 || launchContext.classId === 5)) {
    return {
      value: 0,
      evidenceStatus: 'native-static-launch-class',
      source: 'terminal class 4/5 scene-group preload',
      sourceOpcodes: modeZeroMarkers.concat(modeTwoMarkers).map((opcode) =>
        '0x' + opcode.toString(16).toUpperCase()),
      status: 'The launch class selects the non-mode-two scene-group presentation path; mode-specific commands outside the selected embedded sequence do not override it.'
    };
  }
  if (modeZeroMarkers.length && modeTwoMarkers.length) {
    return {
      value: null,
      evidenceStatus: 'stream-multi-context',
      source: 'mode-guarded commands from more than one launch context',
      sourceOpcodes: modeZeroMarkers.concat(modeTwoMarkers).map((opcode) =>
        '0x' + opcode.toString(16).toUpperCase()),
      status: 'The physical stream contains mode-guarded commands from more than one context; its caller owns the launch mode.'
    };
  }
  if (modeZeroMarkers.length) {
    return {
      value: 0,
      evidenceStatus: 'stream-structural',
      source: 'mode-zero-only Director commands',
      sourceOpcodes: modeZeroMarkers.map((opcode) =>
        '0x' + opcode.toString(16).toUpperCase()),
      status: 'Mode-zero-only commands identify this stream as a mode-zero scene.'
    };
  }
  if (modeTwoMarkers.length) {
    return {
      value: 2,
      evidenceStatus: 'stream-structural',
      source: 'mode-two-only Director commands',
      sourceOpcodes: modeTwoMarkers.map((opcode) =>
        '0x' + opcode.toString(16).toUpperCase()),
      status: 'Mode-two-only commands identify this stream as a mode-two scene.'
    };
  }
  if (Array.isArray(parentEventLaunches) && parentEventLaunches.length) {
    return {
      value: null,
      evidenceStatus: 'parent-event-context-unresolved',
      source: 'direct event opcode-0x10 preserves parent scene mode',
      status: 'The exact parent-event launch selects this Director but preserves a scene mode initialized outside the launch opcode.'
    };
  }
  return {
    value: null,
    evidenceStatus: 'external-unresolved',
    source: 'launch caller outside the Director stream',
    status: 'This stream has no mode-specific command that identifies its launch mode.'
  };
}

function launchCameraProfiles(mode, actorCameraObservation) {
  const registered = mode.value === 0
    ? {
      kind: 'mode-zero-initializer',
      evidenceStatus: 'native-static',
      status: 'The mode-zero initializer sets the registered-object camera bank.',
      projection: null
    }
    : {
      kind: 'external-unresolved',
      evidenceStatus: 'external-unresolved',
      status: 'The mode-two registered-object launch camera is not a Director-stream input.',
      projection: null
    };
  let actor;
  if (actorCameraObservation) {
    actor = {
      kind: 'runtime-observed',
      evidenceStatus: 'runtime-observed',
      status: actorCameraObservation.calibrationResult,
      projection: JSON.parse(JSON.stringify(actorCameraObservation))
    };
  } else if (mode.value === 0) {
    actor = {
      kind: 'mode-zero-initializer',
      evidenceStatus: 'native-static',
      status: 'The mode-zero initializer sets the Actor camera bank.',
      projection: null
    };
  } else if (mode.value === 2) {
    actor = {
      kind: 'mode-two-overlay-initializer',
      evidenceStatus: 'native-static',
      status: 'The mode-two initializer copies immutable overlay camera constants into the Actor camera bank.',
      projection: JSON.parse(JSON.stringify(COMMON_MODE_TWO_ACTOR_PROJECTION))
    };
  } else {
    actor = {
      kind: 'external-unresolved',
      evidenceStatus: 'external-unresolved',
      status: 'The launch caller supplies the initial Actor camera.',
      projection: null
    };
  }
  return { registered, actor };
}

function directorLaunchContextProfile(nodes) {
  const terminal = Array.isArray(nodes) && nodes.length ? nodes[nodes.length - 1] : null;
  if (!terminal || terminal.opcode !== 0x80000001 || terminal.wordCount !== 2) {
    throw new Error('Director launch context requires an exact terminal-with-trailer node.');
  }
  const classId = Number(terminal.rawWords[1]) & 0xFF;
  const effectiveResourceRoute = DIRECTOR_TERMINAL_CLASS_DISPATCH_ROUTES[classId];
  if (!Number.isInteger(effectiveResourceRoute)) {
    throw new Error('Director terminal class ' + classId +
      ' is outside the native resource-class dispatcher table.');
  }
  const resourceLoaderModeWrite = classId === 2 ? 1 : (classId === 7 ? 2 : null);
  return {
    classId,
    source: 'terminal-trailer-low-byte',
    evidenceStatus: 'native-static',
    directEventInitialResourceRoute: -5,
    effectiveResourceRoute,
    resourceRouteSource: 'decoded-terminal-class-resource-mapper',
    resourceClassMapperFunctionZ64: '0x00283E14',
    resourceClassJumpTableZ64: '0x00286B90',
    resourceLoaderModeWrite,
    resourceLoaderModeEffect: resourceLoaderModeWrite === null
      ? 'dispatcher-route-preserves-entry-value'
      : 'dispatcher-route-writes-' + resourceLoaderModeWrite,
    backgroundPreload: classId === 4 || classId === 5
      ? 'scene-group-first-member' : 'mode-two-environment-or-derived-state',
    eventRequestFlagEffect: classId === 2 || classId === 7
      ? 'event-launch-preserves-request-flags'
      : 'event-launch-adds-0x0800',
    status: 'The Director terminal trailer supplies the native launch-context class and replaces a below-minus-two dispatcher hint with its class-owned resource route.'
  };
}

function launchBackgroundRoute(tableId, selector, overlaySelector) {
  if (!Number.isInteger(selector)) {
    return { members: [], assetIds: [], resourceKey: null, stageLayers: [] };
  }
  if (tableId === 'background-table:scene:31') {
    const members = selector >= 0 && selector < SCENE_BACKGROUND_GROUP_KEYS.length
      ? sceneGroupMembers(selector) : [];
    return {
      members,
      assetIds: members.map((member) => member.assetId),
      resourceKey: selector >= 0 && selector < SCENE_BACKGROUND_GROUP_KEYS.length
        ? SCENE_BACKGROUND_GROUP_KEYS[selector] : null,
      stageLayers: []
    };
  }
  if (tableId === 'background-table:mode2-environment:80') {
    const selectedOverlay = Number.isInteger(overlaySelector) ? overlaySelector : null;
    const stageLayers = modeTwoStageLayers(selector, selectedOverlay)
      .map((layer) => ({ ...layer }));
    const assetIds = stageLayers.map((layer) => layer.assetId);
    return {
      members: stageLayers.map((layer, ordinal) => ({ ordinal, assetId: layer.assetId })),
      assetIds,
      resourceKey: selector >= 0 && selector < MODE_TWO_ENVIRONMENT_RESOURCE_KEYS.length
        ? MODE_TWO_ENVIRONMENT_RESOURCE_KEYS[selector] || null : null,
      environmentResourceKey:
        selector >= 0 && selector < MODE_TWO_ENVIRONMENT_RESOURCE_KEYS.length
          ? MODE_TWO_ENVIRONMENT_RESOURCE_KEYS[selector] || null : null,
      environmentAssetId: modeTwoEnvironmentAssetId(selector),
      overlaySelector: selectedOverlay,
      overlayResourceKey:
        Number.isInteger(selectedOverlay) && selectedOverlay >= 0 &&
          selectedOverlay < MODE_TWO_OVERLAY_RESOURCE_KEYS.length
          ? MODE_TWO_OVERLAY_RESOURCE_KEYS[selectedOverlay] || null : null,
      overlayAssetIds: Number.isInteger(selectedOverlay)
        ? archiveAssetIds(BATTLE_BACKGROUND_ARCHIVE_ROWS[selectedOverlay] || []) : [],
      stageLayers
    };
  }
  return { members: [], assetIds: [], resourceKey: null, stageLayers: [] };
}

function launchBackgroundProfiles(mode, requests, runtimeObservation, launchContext,
    parentEventLaunches, derivedEnvironmentRules) {
  return requests.map((request) => {
    let selectorTableId = null;
    let selector = null;
    let foregroundSelectorTableId = null;
    let foregroundSelector = null;
    let selectorSource = 'external-unresolved';
    let foregroundSelectorSource = 'not-applicable';
    let evidenceStatus = 'external-unresolved';
    let status = 'The active background route depends on launch state outside this stream.';
    let foregroundStatus = 'No foreground selector applies to this launch route.';
    let environmentSelectorCandidates = [];
    let foregroundSelectorCandidates = [];
    let derivedEnvironment = null;
    const classPreloadsSceneGroup = launchContext &&
      (launchContext.classId === 4 || launchContext.classId === 5);
    if (mode.value === 2 && runtimeObservation &&
        Number.isInteger(runtimeObservation.environmentSelector)) {
      selectorTableId = 'background-table:mode2-environment:80';
      selector = runtimeObservation.environmentSelector;
      environmentSelectorCandidates = [selector];
      foregroundSelectorTableId = 'background-table:mode2-overlay:80';
      foregroundSelector = Number.isInteger(runtimeObservation.foregroundSelector)
        ? runtimeObservation.foregroundSelector : null;
      selectorSource = 'runtime-observed-environment-selector';
      foregroundSelectorSource = foregroundSelector === null
        ? 'runtime-observed-inactive' : 'runtime-observed-foreground-selector';
      foregroundSelectorCandidates = foregroundSelector === null
        ? [] : [foregroundSelector];
      evidenceStatus = 'runtime-observed';
      status = 'Stored launch state independently supplies the mode-two environment and foreground selectors.';
      foregroundStatus = foregroundSelector === null
        ? 'The stored launch has no active foreground selector.'
        : 'Stored launch state supplies the exact foreground selector.';
    } else if (classPreloadsSceneGroup) {
      selectorTableId = 'background-table:scene:31';
      selector = request.commandOperand;
      environmentSelectorCandidates = Number.isInteger(selector) ? [selector] : [];
      selectorSource = 'director-launch-prescan-class-4-or-5-scene-group';
      evidenceStatus = 'native-static-launch-prescan';
      status = 'Terminal class ' + launchContext.classId +
        ' makes the native launch pre-scan load this scene group and cache its first member.';
    } else if (mode.value === 0) {
      selectorTableId = 'background-table:scene:31';
      selector = request.commandOperand;
      environmentSelectorCandidates = Number.isInteger(selector) ? [selector] : [];
      selectorSource = 'director-command-operand';
      evidenceStatus = mode.evidenceStatus === 'runtime-observed'
        ? 'runtime-observed-mode-native-command' : 'stream-structural-mode-native-command';
      status = 'The active non-mode-two handler uses the Director command operand.';
    } else if (mode.value === 2) {
      selectorTableId = 'background-table:mode2-environment:80';
      foregroundSelectorTableId = 'background-table:mode2-overlay:80';
      const parentEventContext = Array.isArray(parentEventLaunches) &&
        parentEventLaunches.length > 0;
      const commandSeedsEnvironment = Number.isInteger(request.commandOperand) &&
        request.commandOperand >= 0 &&
        request.commandOperand < MODE_TWO_ENVIRONMENT_RESOURCE_KEYS.length;
      if (commandSeedsEnvironment) {
        selector = request.commandOperand;
        environmentSelectorCandidates = [request.commandOperand];
        selectorSource = 'director-launch-prescan-command-operand';
        foregroundSelector = parentEventContext ? request.commandOperand : null;
        foregroundSelectorSource = parentEventContext
          ? 'event-context-mode-two-environment-copy'
          : 'source-only-launch-route-unresolved';
        foregroundSelectorCandidates = parentEventContext
          ? [request.commandOperand]
          : [];
        evidenceStatus = 'native-static-launch-prescan';
        status = 'The native launch pre-scan uses the Director command operand as the mode-two environment selector. Both mode-two Stage constructors apply the shared identity B5 launch crop before Director transforms run.';
        foregroundStatus = parentEventContext
          ? 'The event-context mode-two Director initializer copies the environment selector into independent foreground storage.'
          : 'No parent event owns this source-only resource. The fixed-overlay request is statically limited to Director selector zero, so this resource needs its external launch caller before choosing a foreground.';
      } else if (request.commandOperand === -1) {
        derivedEnvironment = modeTwoDerivedEnvironmentProfile(
          parentEventLaunches, derivedEnvironmentRules);
        selector = derivedEnvironment.environmentSelector;
        environmentSelectorCandidates =
          derivedEnvironment.environmentSelectorCandidates.slice();
        selectorSource = 'director-launch-prescan-derived-sentinel';
        foregroundSelector = parentEventContext ? selector : null;
        foregroundSelectorSource = parentEventContext
          ? 'event-context-mode-two-derived-environment-copy'
          : 'source-only-launch-route-unresolved';
        foregroundSelectorCandidates = parentEventContext
          ? environmentSelectorCandidates.slice() : [];
        evidenceStatus = derivedEnvironment.evidenceStatus;
        status = derivedEnvironment.status;
        foregroundStatus = parentEventContext
          ? 'The event-context mode-two initializer copies the derived environment selector after the mapper resolves its launch inputs.'
          : 'No parent event owns this source-only resource. The fixed-overlay request is statically limited to Director selector zero, so this resource needs its external launch caller before choosing a foreground.';
      } else {
        foregroundSelectorSource = 'external-unresolved';
        status = 'The Director launch pre-scan does not resolve this mode-two environment selector.';
        foregroundStatus = 'The final foreground selector remains external and unresolved.';
      }
    }
    const route = launchBackgroundRoute(selectorTableId, selector, foregroundSelector);
    const observedStageLayers = runtimeObservation &&
      Array.isArray(runtimeObservation.stageLayers)
      ? runtimeObservation.stageLayers.map((layer) => ({ ...layer })) : [];
    const staticSceneGroupLayers = selectorTableId === 'background-table:scene:31'
      ? route.members.map((member, index) => ({
        assetId: member.assetId,
        role: index === 0 ? 'environment-base' : 'ordered-layer',
        depth: member.ordinal,
        nativeOrdinal: member.ordinal,
        evidenceStatus,
        associationStatus: 'exact ordered member of the command-selected scene group'
      })) : [];
    const stageLayers = observedStageLayers.length
      ? observedStageLayers
      : (selectorTableId === 'background-table:mode2-environment:80'
        ? route.stageLayers : staticSceneGroupLayers);
    return {
      requestId: request.requestId,
      wordStart: request.wordStart,
      commandOperand: request.commandOperand,
      selectorTableId,
      selector,
      environmentSelector: selectorTableId === 'background-table:mode2-environment:80'
        ? selector : null,
      foregroundSelectorTableId,
      foregroundSelector,
      selectorSource,
      environmentSelectorCandidates,
      foregroundSelectorSource,
      foregroundSelectorCandidates,
      foregroundStatus,
      derivedEnvironment,
      evidenceStatus,
      status,
      resourceKey: route.resourceKey,
      members: route.members,
      assetIds: route.assetIds,
      stageLayers,
      stageAssetIds: stageLayers.length
        ? stageLayers.map((layer) => layer.assetId)
        : (selectorTableId === 'background-table:scene:31'
          ? route.assetIds.slice() : [])
    };
  });
}

function launchRosterProfile(asset, actors, canonicalNodes, parentEventLaunches) {
  const nodes = Array.isArray(canonicalNodes) && canonicalNodes.length
    ? canonicalNodes : (asset.nodes || []);
  const materializers = nodes.filter((node) => {
    const opcode = Number.isInteger(node.opcode_u32)
      ? node.opcode_u32 : Number(node.opcode) >>> 0;
    return [0x45, 0xAB].includes(opcode);
  });
  const records = actors.map((actor) => ({
    actorId: actor.actorId,
    slot: actor.slot,
    recordProducer: actor.recordProducer === true,
    initializationStatus: actor.initializationStatus
  }));
  const externalTemplateSlots = records.filter((record) => !record.recordProducer)
    .map((record) => record.slot);
  return {
    templateCount: records.length,
    records,
    recordProducerSlots: records.filter((record) => record.recordProducer)
      .map((record) => record.slot),
    externalTemplateSlots,
    materializationWordStarts: materializers.map((node) =>
      Number.isInteger(node.word_start) ? node.word_start : node.startWord),
    nativeActorInputSource: 'current-gameplay-unit-members',
    nativeActorInputRowCapacity: 20,
    nativeUnitMemberCapacity: 5,
    nativeMaximumCurrentUnitRows: 10,
    modeTwoForceInitialization: true,
    fixedOverlayForceInitialization: false,
    secondUnitLeaderOnlyPropertyMask: 0x8000,
    externalRosterDependency: true,
    evidenceStatus: materializers.length || externalTemplateSlots.length
      ? 'stream-context-roster' : 'same-stream-records',
    status: materializers.length
      ? 'Roster commands materialize launch-time records represented by the scene templates.'
      : (externalTemplateSlots.length
        ? 'Some Actor commands consume launch-time records represented by the scene templates.'
        : 'Every catalogued Actor template has a same-stream record producer.')
  };
}

function launchPreservationSnapshotProfile(parentEventLaunches) {
  const contexts = (parentEventLaunches || []).flatMap((launch) =>
    (launch.eventInvocationContexts || []).map((context) =>
      context.launchPreservationSnapshot));
  const exactContexts = contexts.filter((value) => value !== null);
  return {
    condition: 'launch-flag-bit-0x08-or-event-property-0xE6-nonzero',
    directEventLaunchFlagBit08: contexts.length ? false : null,
    contextCount: contexts.length,
    exactContextCount: exactContexts.length,
    requiredContextCount: exactContexts.filter(Boolean).length,
    notRequiredContextCount: exactContexts.filter((value) => !value).length,
    unresolvedContextCount: contexts.length - exactContexts.length,
    evidenceStatus: contexts.length
      ? 'native-static-mode-two-initializer-and-event-vm'
      : 'external-launch-context-unresolved',
    status: contexts.length
      ? 'Direct event launches keep flag bit 0x08 clear. Exact event-property 0xE6 values therefore resolve the separate gameplay-state preservation snapshot per invocation.'
      : 'This stream has no direct event invocation from which to resolve the gameplay-state preservation snapshot.'
  };
}

function launchStageTransformProfile(directorMode) {
  const modeTwo = directorMode && directorMode.value === 2;
  return {
    initial: {
      translateX: 0,
      translateY: 0,
      scaleX: 1,
      scaleY: 1
    },
    evidenceStatus: modeTwo
      ? 'native-static-mode-two-stage-initializers'
      : 'editor-preview-default',
    ownerFunctionsZ64: modeTwo
      ? ['0x001FFA8C', '0x001FFAD0', '0x001FB32C'] : [],
    status: modeTwo
      ? 'Both Director mode-two Stage constructors clear shared Stage state, initialize identity X/Y translation and scale, then build the selected B5 Stage.'
      : 'The editor starts this non-mode-two or unresolved presentation at identity; this profile does not claim a native non-mode-two transform initializer.'
  };
}

function buildLaunchProfile(asset, actors, backgroundObservation,
    actorCameraObservation, backgroundRequests, canonicalNodes,
    parentEventLaunches, operandTranslation, launchContext,
    derivedEnvironmentRules, oversizedImageRules) {
  const eventLaunches = Array.isArray(parentEventLaunches)
    ? parentEventLaunches.map((row) => ({ ...row })) : [];
  const directorMode = directorLaunchMode(
    asset, backgroundObservation, canonicalNodes, eventLaunches, launchContext);
  return {
    profileId: 'launch-profile:' + asset.assetId,
    directorMode,
    launchContext: launchContext || null,
    parentEventLaunches: eventLaunches,
    operandTranslation: operandTranslation || launchOperandTranslationProfile([]),
    launchPreservationSnapshot: launchPreservationSnapshotProfile(eventLaunches),
    stageTransform: launchStageTransformProfile(directorMode),
    cameras: launchCameraProfiles(directorMode, actorCameraObservation),
    background: {
      requestCount: backgroundRequests.length,
      requests: launchBackgroundProfiles(
        directorMode, backgroundRequests, backgroundObservation, launchContext,
        eventLaunches, derivedEnvironmentRules),
      inheritedPresentation: null,
      inheritanceContexts: []
    },
    oversizedImagePresentation: oversizedImagePresentationProfile(
      asset, launchContext, eventLaunches, oversizedImageRules),
    roster: launchRosterProfile(asset, actors, canonicalNodes, eventLaunches)
  };
}

function directorAudioAssociations(asset) {
  return (asset.nodes || []).flatMap((node) => {
    if (String(node.opcode || '').toUpperCase() !== '0X6E' ||
        !Array.isArray(node.operands) || node.operands.length < 3) return [];
    const selector = node.operands[0];
    const mode = node.operands[1];
    const requestValue = node.operands[2];
    const blockIndex = selector === 0 && mode === 0
      ? nativeAudioBlockIndexForRequest(requestValue) : null;
    if (Number.isInteger(blockIndex)) {
      return [{
        associationId: asset.assetId + ':audio:' + String(node.startWord).padStart(4, '0'),
        occurrenceId: node.id,
        wordStart: node.startWord,
        opcode: '0x6E',
        contextSelector: selector,
        mode,
        requestValue,
        audioBlockId: 'sequenced-audio:' + String(blockIndex).padStart(2, '0'),
        associationStatus: 'exact selector-0 controller-table route; semantic media name unresolved'
      }];
    }
    const registered = selector === 4 && requestValue === 701 ||
      selector === 5 && [472, 527, 707, 708].includes(requestValue);
    return registered ? [{
      associationId: asset.assetId + ':audio:' + String(node.startWord).padStart(4, '0'),
      occurrenceId: node.id,
      wordStart: node.startWord,
      opcode: '0x6E',
      contextSelector: selector,
      mode,
      requestValue,
      registeredAudioRequestAssetId: 'registered-audio-request:' + requestValue,
      associationStatus: 'exact registered-table payload route; semantic media name unresolved'
    }] : [];
  });
}

function compactRegisteredWait(wait) {
  return {
    id: wait.wait_id,
    startWord: wait.word_start,
    endWord: wait.word_end_exclusive,
    ticks: wait.ticks_s32,
    nodeIds: wait.physical_owner_node_ids.slice(),
    runtimeReachability: wait.runtime_reachability
  };
}

function compactScene(asset, canonicalAsset, canonicalNodes, registeredWaits,
    selectors, selectorOwner, retailResource, derivedEnvironmentRules,
    oversizedImageRules) {
  const rawIdentity = asset.assetId.split(':').pop();
  if (!canonicalAsset || canonicalAsset.asset_id !== asset.assetId ||
      canonicalAsset.decoded_word_count !== asset.decodedWordCount ||
      canonicalAsset.decoded_sha256 !== asset.decodedSha256 ||
      canonicalAsset.physical_word_conservation !== true ||
      canonicalAsset.new_raw_gap_words !== 0 ||
      canonicalAsset.unknown_default_skip_count !== 0) {
    throw new Error('Corrected Director corpus identity mismatch for ' + asset.assetId + '.');
  }
  if (!Array.isArray(canonicalNodes) ||
      canonicalNodes.length !== canonicalAsset.node_count) {
    throw new Error('Corrected Director node count mismatch for ' + asset.assetId + '.');
  }
  const legacyByStart = new Map(asset.nodes.map((node) => [node.startWord, node]));
  const actors = asset.derivedActorContext && Array.isArray(asset.derivedActorContext.actors)
    ? asset.derivedActorContext.actors.map((actor) => compactActor(actor, asset.nodes, selectors))
    : [];
  actors.push(...recoveredActorInitializations(asset, selectors, actors));
  const friendlyName = REVIEWED_TITLES[asset.canonicalScene] || null;
  const technicalName = friendlyName ||
    (asset.canonicalScene && !/^rom-director-exact-/.test(asset.canonicalScene)
      ? titleFromSlug(asset.canonicalScene)
      : 'Director resource ' + rawIdentity);
  const backgroundObservation = DIRECTOR_BACKGROUND_RUNTIME_OBSERVATIONS[asset.assetId] || null;
  const actorCameraObservation =
    DIRECTOR_ACTOR_CAMERA_RUNTIME_OBSERVATIONS[asset.assetId] || null;
  const backgroundRequests = backgroundRequestsForAsset(asset, backgroundObservation,
    retailResource && retailResource.launchContext);
  const launchProfile = buildLaunchProfile(asset, actors, backgroundObservation,
    actorCameraObservation, backgroundRequests, canonicalNodes,
    retailResource && retailResource.parentEventLaunches,
    retailResource && retailResource.operandTranslation,
    retailResource && retailResource.launchContext,
    derivedEnvironmentRules, oversizedImageRules);
  const nativeBackgroundGroup = backgroundObservation &&
    backgroundObservation.directorMode === 0;
  const backgroundAssetIds = backgroundObservation
    ? backgroundObservation.exactAssetIds.slice() : [];
  const profiledStageRequests = launchProfile.background.requests.filter((request) =>
    request.stageAssetIds.length > 0);
  const backgroundCandidateAssetIds = Array.from(new Set(backgroundRequests.flatMap((request) =>
    request.nonMode2Route.archiveAssetIds).concat(
      backgroundObservation ? backgroundObservation.candidateAssetIds : [],
      launchProfile.background.requests.flatMap((request) => request.stageAssetIds),
      backgroundAssetIds)));
  const audioAssociations = directorAudioAssociations(asset);
  const dialogueAssociations = backgroundObservation &&
    Array.isArray(backgroundObservation.captures)
    ? backgroundObservation.captures.filter((capture) => capture.dialogueAssociation)
      .map((capture) => ({
        ...capture.dialogueAssociation,
        captureStageId: capture.stageId,
        captureFilename: capture.filename,
        captureSha256: capture.zipSha256
      }))
    : [];
  return {
    sceneId: 'scene:director:' + rawIdentity.toLowerCase(),
    storageId: asset.assetId,
    assetId: asset.assetId,
    canonicalScene: asset.canonicalScene,
    technicalName,
    friendlyName,
    aliases: Array.isArray(asset.aliases) ? asset.aliases.slice() : [],
    aliasNames: Array.isArray(asset.aliasNames) ? asset.aliasNames.slice() : [],
    reviewedTimelineOverlay: asset.reviewedTimelineOverlay
      ? JSON.parse(JSON.stringify(asset.reviewedTimelineOverlay)) : null,
    engine: 'director',
    sourceRevision: 'us-rev0',
    directorKey: hex(asset.cutsceneLoadKey, 8),
    triggerStatus: asset.runtimeProof === 'opening-accepted'
      ? 'runtime-observed'
      : (asset.runtimeProof === 'static-census-only' ? 'static-census' : 'runtime-pending'),
    runtimeProof: asset.runtimeProof,
    parseStatus: 'canonical-153-complete',
    actorBearing: actors.length > 0,
    actorCount: actors.length,
    actors,
    backgroundAssetIds,
    backgroundCandidateAssetIds,
    backgroundAssociationStatus: backgroundObservation
      ? backgroundObservation.associationStatus
      : (profiledStageRequests.length
        ? 'Native launch routing selects a renderable environment from the Director command without changing the stream.'
        : (backgroundRequests.length
          ? 'The game supplies the complete scene through launch state; this Director stream contains no complete background image.'
          : 'This Director stream contains no background command; its launch context supplies any scenery.')),
    backgroundRuntimeObservation: backgroundObservation
      ? JSON.parse(JSON.stringify(backgroundObservation)) : null,
    actorCameraObservation: actorCameraObservation
      ? JSON.parse(JSON.stringify(actorCameraObservation)) : null,
    launchProfile,
    backgroundRequests,
    dialogueAssociations,
    audioAssociations,
    recoveredMediaRequests: asset.tailRecovery
      ? recoveredMediaRequests(asset.assetId, {
        nodes: asset.nodes.filter((node) =>
          node.startWord >= asset.tailRecovery.acceptedPrefixEndWord)
      }) : [],
    recoveredActorEvents: asset.tailRecovery
      ? recoveredActorEvents(asset.assetId, asset.nodes,
        asset.tailRecovery.acceptedPrefixEndWord, selectors) : [],
    recoveredNativeSpriteEffects: asset.tailRecovery
      ? recoveredNativeSpriteEffects(asset.assetId, {
        nodes: asset.nodes.filter((node) =>
          node.startWord >= asset.tailRecovery.acceptedPrefixEndWord)
      }, selectors) : [],
    previewCapability: 'preview-only',
    exportCapability: 'needs-research',
    source: {
      masterRomSha256: asset.masterRomSha256,
      z64PrefixStart: asset.z64PrefixStart,
      z64PrefixEndExclusive: asset.z64PrefixEndExclusive,
      z64PayloadStart: asset.z64PayloadStart,
      z64PayloadEndExclusive: asset.z64PayloadEndExclusive,
      rawV64PayloadStart: asset.rawV64PayloadStart,
      storedPayloadLength: asset.storedPayloadLength,
      dmaExtent: asset.dmaExtent,
      decodedLength: asset.decodedLength,
      decodedWordCount: asset.decodedWordCount,
      decodedSha256: asset.decodedSha256,
      directorSelectorTableResourceKey: selectorOwner.tableResourceKey,
      directorSelectorTablePayloadZ64: selectorOwner.tablePayloadZ64,
      directorSelectorRows: selectorOwner.selectorRows.slice(),
      directorSelectorWordZ64: selectorOwner.selectorWordZ64.slice(),
      crcWindowOverlap: asset.crcWindowOverlap === true,
      codecVersion: asset.codecVersion,
      corpusNodeCount: canonicalAsset.node_count,
      corpusQueryCount: canonicalAsset.query_count,
      corpusRegisteredWaitCount: canonicalAsset.registered_wait_count,
      terminationWordStart: canonicalAsset.termination_word_start,
      terminationWordEndExclusive: canonicalAsset.termination_word_end_exclusive,
      nodes: canonicalNodes.map((node) => compactCanonicalNode(
        node, legacyByStart.get(node.word_start) || null, { nativeBackgroundGroup })),
      registeredWaits: registeredWaits.map(compactRegisteredWait),
      gaps: [],
      historicalGaps: canonicalAsset.old_r4_had_raw_gap
        ? [{ wordCount: canonicalAsset.old_r4_raw_gap_words,
          status: 'superseded by corrected 153-command corpus' }] : [],
      tailRecovery: null
    }
  };
}

function compactPartialDirectorResource(row, recovery, selectors) {
  if (!recovery || recovery.partialResource !== true || recovery.assetId !== row.assetId ||
      recovery.remainingGapWordCount !== 0 ||
      recovery.decodedWordCount !== row.decodedWordCount) {
    throw new Error('Complete Director Partial recovery is missing for ' + row.assetId + '.');
  }
  const historicalGapWordCount = row.gapFields ? row.gapFields.gapWordCount : 0;
  return {
    resourceId: row.assetId,
    directorKey: hex(row.cutsceneLoadKey, 8),
    disposition: row.disposition,
    parseStatus: row.parseStatus,
    parsedWordCount: row.features.parsedWordCount,
    decodedWordCount: row.decodedWordCount,
    gapWordCount: historicalGapWordCount,
    historicalGapWordCount,
    structurallyOwnedWordCount: row.decodedWordCount,
    remainingGapWordCount: recovery.remainingGapWordCount,
    recoveredMediaRequests: recoveredMediaRequests(row.assetId, recovery),
    recoveredActorEvents: recoveredActorEvents(row.assetId,
      recovery.allNodes || recovery.nodes, recovery.acceptedPrefixEndWord, selectors),
    recoveredNativeSpriteEffects:
      recoveredNativeSpriteEffects(row.assetId, recovery, selectors),
    tailRecovery: tailRecoverySummary(recovery),
    source: {
      z64PrefixStart: row.z64PrefixStart,
      z64PrefixEndExclusive: row.z64PrefixEndExclusive,
      z64PayloadStart: row.z64PayloadStart,
      z64PayloadEndExclusive: row.z64PayloadEndExclusive,
      z64PayloadDmaEndExclusive: row.z64PayloadDmaEndExclusive,
      storedPayloadLength: row.storedPayloadLength,
      dmaExtent: row.dmaExtent,
      compressedSha256: row.compressedSha256,
      decodedLength: row.decodedLength,
      decodedSha256: row.decodedSha256,
      codecVersion: row.codecVersion
    },
    associationStatus:
      'exact runtime presentation field to Director Partial resource; every word has a runtime structural boundary, while native presentation export remains locked'
  };
}

function compactPresentationActors(row, selectors) {
  return (row.actors || []).map((actor) => {
    const selector = selectors.get(actorSelectorKey(
      actor.bank, actor.animationKey, actor.facing)) || null;
    return {
      actorId: 'actor:presentation:' + row.id + ':slot:' + String(actor.slot).padStart(2, '0'),
      slot: actor.slot,
      label: 'Actor slot ' + actor.slot,
      bank: actor.bank,
      animationKey: actor.animationKey,
      facing: actor.facing,
      x: actor.x,
      y: actor.y,
      z: actor.z,
      poseResolutionId: selector
        ? 'cutscene-pose:' + actor.bank + ':' + actor.animationKey + ':' + actor.facing : null,
      sourcePoseResolutionId: null,
      physicalStateId: selector ? selector.physicalStateId : null,
      stateIndex: selector ? selector.stateIndex : null,
      sourceProgramDefined: selector ? selector.sourceProgramDefined === true : false,
      initializationCandidateId: null,
      initializationSourceOpcode: '0X14',
      initializationStatus: actor.recoveredWordStart == null
        ? 'exact opcode-0x14 record producer in the located Director Partial prefix'
        : 'exact opcode-0x14 record producer in the recovered Director Partial stream',
      controlEntryAlias: null,
      recordProducer: true,
      variantSelector: actor.variantSelector,
      rawVariantSelector: actor.variantSelector,
      variantSelectorStatus: 'exact opcode-0x14 appearance byte',
      selectorStatus: selector
        ? (selector.sourceProgramDefined
          ? 'physical state and source program located'
          : 'exact physical ROM state located; scene-local source publication is absent')
        : 'record produced but selector resolver fails',
      visibilityStatus: 'visual preview from exact record allocation; no dedicated logical show flag is proved'
    };
  });
}

function compactPresentationScene(row, selectors, partialResourceMap, tailRecoveryMap) {
  const actors = compactPresentationActors(row, selectors);
  const partialResource = row.partialDirectorResourceId
    ? partialResourceMap.get(row.partialDirectorResourceId) : null;
  const tailRecovery = row.partialDirectorResourceId
    ? tailRecoveryMap.get(row.partialDirectorResourceId) : null;
  if (row.partialDirectorResourceId && !partialResource) {
    throw new Error('Missing Director Partial resource ' + row.partialDirectorResourceId + '.');
  }
  if (row.partialDirectorResourceId && !tailRecovery) {
    throw new Error('Missing Director Partial tail recovery ' + row.partialDirectorResourceId + '.');
  }
  const backgroundObservation = DIRECTOR_BACKGROUND_RUNTIME_OBSERVATIONS[
    'presentation:' + row.id] || null;
  const backgroundAssetIds = Array.from(new Set((row.backgroundAssetIds || []).concat(
    backgroundObservation ? backgroundObservation.exactAssetIds : [])));
  const backgroundCandidateAssetIds = Array.from(new Set(
    (row.backgroundCandidateAssetIds || []).concat(
      backgroundObservation ? backgroundObservation.candidateAssetIds : [],
      backgroundAssetIds)));
  return {
    sceneId: 'scene:presentation:' + row.id,
    storageId: 'presentation:' + row.id,
    assetId: 'presentation:' + row.id,
    canonicalScene: row.id,
    technicalName: row.title,
    friendlyName: row.title,
    aliases: [],
    aliasNames: row.runtimeAnchor ? [row.runtimeAnchor] : [],
    engine: row.engine,
    sourceRevision: 'us-rev0',
    directorKey: null,
    triggerStatus: row.runtimeAnchorSha256 ? 'runtime-observed' : 'static-data',
    runtimeProof: partialResource ? 'runtime-anchor-with-exact-director-partial-resource' :
      (row.runtimeAnchorSha256 ? 'non-director-runtime-anchor' : 'static-data-only'),
    parseStatus: partialResource ? 'runtime-tiled-adapter-unresolved' : 'adapter-unresolved',
    actorBearing: actors.length > 0,
    actorCount: actors.length,
    actors,
    backgroundAssetIds,
    backgroundCandidateAssetIds,
    backgroundAssociationStatus: row.backgroundAssociationStatus ||
      (backgroundObservation ? backgroundObservation.associationStatus :
        (backgroundCandidateAssetIds.length
          ? 'filename or presentation-family candidates; consumer unresolved'
          : 'presentation visual resource unresolved')),
    backgroundRuntimeObservation: backgroundObservation
      ? JSON.parse(JSON.stringify(backgroundObservation)) : null,
    backgroundRequests: [],
    dialogueAssociations: (row.dialogueAssociations || []).map((entry) => ({ ...entry })),
    audioAssociations: partialResource
      ? directorAudioAssociations({ assetId: 'presentation:' + row.id,
        nodes: tailRecovery.nodes }) : [],
    recoveredMediaRequests: partialResource
      ? recoveredMediaRequests('presentation:' + row.id, tailRecovery) : [],
    recoveredActorEvents: partialResource
      ? recoveredActorEvents('presentation:' + row.id,
        tailRecovery.allNodes || tailRecovery.nodes,
        tailRecovery.acceptedPrefixEndWord, selectors) : [],
    recoveredNativeSpriteEffects: partialResource
      ? recoveredNativeSpriteEffects('presentation:' + row.id, tailRecovery, selectors) : [],
    previewCapability: 'preview-only',
    exportCapability: 'needs-research',
    source: {
      sourceKind: partialResource ? 'runtime-anchor-with-director-partial' :
        (row.runtimeAnchorSha256 ? 'runtime-anchor' : 'static-data-range'),
      runtimeAnchor: row.runtimeAnchor || null,
      runtimeAnchorSha256: row.runtimeAnchorSha256 || null,
      partialDirectorResourceId: row.partialDirectorResourceId || null,
      partialDirectorKey: partialResource ? partialResource.directorKey : null,
      tailRecovery: partialResource ? { ...partialResource.tailRecovery } : null,
      z64Start: row.staticDataZ64Start == null ? null : row.staticDataZ64Start,
      z64EndExclusive: row.staticDataZ64EndExclusive == null
        ? null : row.staticDataZ64EndExclusive,
      adapterStatus: partialResource
        ? 'all Director words have exact runtime boundaries; semantics, presentation ownership, and native export remain locked'
        : 'engine parser and native export adapter unresolved',
      nodes: [],
      gaps: []
    }
  };
}

function decodedFileForArchive(root, archive) {
  const directory = path.join(root, 'e' + archive.index);
  const entries = fs.readdirSync(directory, { withFileTypes: true })
    .filter((entry) => entry.isFile())
    .map((entry) => path.join(directory, entry.name));
  if (entries.length !== 1) {
    throw new Error('Expected one decoded file in ' + directory + ', found ' + entries.length);
  }
  return entries[0];
}

function readableSerifuText(raw, inlineSpeaker) {
  let text = String(raw || '');
  if (inlineSpeaker) text = text.replace(/^@k[^@]*@n/, '');
  return text
    .replace(/@T(\d+)/g, '[timing $1]')
    .replace(/@%(\d+)/g, '[name $1]')
    .replace(/@n/g, '\n')
    .replace(/@p/g, '\n\n')
    .replace(/@s/g, '[pause]')
    .replace(/@c/g, '[clear]')
    .replace(/@w/g, '[wait]')
    .replace(/@a/g, '[auto]')
    .replace(/@-/g, '')
    .replace(/@x/g, '')
    .replace(/@l\d+/g, '')
    .replace(/@r/g, '')
    .replace(/@S\d+/g, '')
    .replace(/@A/g, '')
    .replace(/@=/g, '')
    .replace(/\r/g, '')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
}

function parseSerifuArchive(bytes, archive) {
  if (bytes.length < 8) throw new Error('Serifu archive #' + archive.index + ' is too small.');
  const first = u32(bytes, 0);
  if (first < 4 || first % 4 !== 0 || first > bytes.length) {
    throw new Error('Serifu archive #' + archive.index + ' has an invalid pointer table.');
  }
  const entryCount = first / 4;
  const pointers = [];
  for (let index = 0; index < entryCount; index += 1) pointers.push(u32(bytes, index * 4));
  if (pointers[0] !== first || pointers.some((value, index) =>
    value < first || value >= bytes.length || index && value <= pointers[index - 1])) {
    throw new Error('Serifu archive #' + archive.index + ' has invalid entry pointers.');
  }
  return pointers.map((start, index) => {
    let end = index + 1 < pointers.length ? pointers[index + 1] : bytes.length;
    const terminator = bytes.indexOf(0, start);
    if (terminator !== -1 && terminator < end) end = terminator;
    const rawText = bytes.subarray(start, end).toString('latin1');
    const speakerMatch = rawText.match(/@S(\d+)/);
    const inlineSpeakerMatch = rawText.match(/^@k([^@]*)@n/);
    const speakerLabel = inlineSpeakerMatch && inlineSpeakerMatch[1].trim() || null;
    return {
      entryId: 'serifu:' + archive.index + ':' + index,
      archiveIndex: archive.index,
      entryIndex: index,
      displayEntryNumber: index + 1,
      sourceOffset: start,
      sourceLength: end - start,
      speakerId: speakerMatch ? Number(speakerMatch[1]) : null,
      speakerLabel,
      text: readableSerifuText(rawText, speakerLabel),
      rawText,
      associationStatus: archive.index === 162 && index === 8
        ? 'exact presentation entry selector 8 and Scenario 1 Dio payload; load-time selector writer unresolved'
        : 'entry index is the exact presentation entry selector after archive selection; scene association unresolved'
    };
  });
}

function parseMeswinArchive(bytes, archive) {
  const entryCount = 631;
  const tableSize = entryCount * 4;
  if (bytes.length < tableSize || u32(bytes, 0) !== tableSize) {
    throw new Error('MESWIN archive #' + archive.index + ' has an invalid 631-entry pointer table.');
  }
  const pointers = [];
  for (let index = 0; index < entryCount; index += 1) pointers.push(u32(bytes, index * 4));
  if (pointers.some((value, index) => value < tableSize || value >= bytes.length ||
      index && value < pointers[index - 1])) {
    throw new Error('MESWIN archive #' + archive.index + ' has invalid entry pointers.');
  }
  const shiftJis = new TextDecoder('shift_jis');
  return pointers.map((start, index) => {
    let end = index + 1 < pointers.length ? pointers[index + 1] : bytes.length;
    const terminator = bytes.indexOf(0, start);
    if (terminator !== -1 && terminator < end) end = terminator;
    const sourceBytes = bytes.subarray(start, end);
    const encodedText = shiftJis.decode(sourceBytes);
    return {
      entryId: 'meswin:' + archive.index + ':' + index,
      archiveIndex: archive.index,
      entryIndex: index,
      displayEntryNumber: index + 1,
      sourceOffset: start,
      sourceLength: end - start,
      speakerId: null,
      speakerLabel: null,
      encoding: sourceBytes.some((value) => value >= 0x80) ? 'mixed-shift-jis' : 'ascii',
      text: readableSerifuText(encodedText, null),
      rawText: sourceBytes.toString('latin1'),
      associationStatus:
        'exact MESWIN entry selector in presentation family 0x80; named scene scheduler unresolved'
    };
  });
}

function compactDialogueArchives(root, archiveCatalog, selectorCatalog) {
  const archives = archiveCatalog.filter((archive) =>
    archive.index >= 158 && archive.index <= 505 && archive.contentType === 'parsed:serifu');
  if (archives.length !== 348) throw new Error('Expected all 348 Serifu archives.');
  const selectorByArchive = new Map((selectorCatalog.entries || [])
    .filter((entry) => Number.isInteger(entry.archiveIndex))
    .map((entry) => [entry.archiveIndex, entry]));
  const output = archives.map((archive) => {
    const decodedPath = decodedFileForArchive(root, archive);
    const bytes = fs.readFileSync(decodedPath);
    const entries = parseSerifuArchive(bytes, archive);
    const selector = selectorByArchive.get(archive.index);
    if (!selector) throw new Error('No presentation selector reaches Serifu archive ' + archive.index + '.');
    return {
      archiveId: 'serifu:' + archive.index,
      archiveIndex: archive.index,
      presentationFamily: 'serifu',
      presentationFamilyBits: 0,
      presentationSelector: selector.selector,
      presentationResourceKey: parseInt(selector.resourceKey, 16),
      filename: archive.filename,
      label: 'Dialogue archive #' + archive.index + ' · ' + archive.filename,
      entryCount: entries.length,
      associationStatus: archive.index === 162
        ? 'exact presentation archive selector 4 and one Scenario 1 payload; load-time pair writer unresolved'
        : 'exact presentation archive selector; scene and trigger association unresolved',
      selectorStatus:
        'func_000E5968 selects this archive through the exact 503-slot presentation table',
      previewCapability: 'preview-only',
      exportCapability: 'needs-research',
      source: {
        archiveHeaderZ64: parseInt(archive.romOffset, 16),
        compressedSize: archive.compSize,
        decodedSize: bytes.length,
        decodedSha256: sha256(bytes)
      },
      entries
    };
  });
  const totalEntries = output.reduce((total, archive) => total + archive.entryCount, 0);
  if (totalEntries !== 4569) throw new Error('Expected all 4,569 Serifu entries; found ' + totalEntries + '.');
  const meswinArchive = archiveCatalog.find((archive) =>
    archive.index === 815 && archive.contentType === 'text');
  if (!meswinArchive) throw new Error('MESWIN archive #815 is missing from the archive catalog.');
  const meswinPath = decodedFileForArchive(root, meswinArchive);
  const meswinBytes = fs.readFileSync(meswinPath);
  const meswinEntries = parseMeswinArchive(meswinBytes, meswinArchive);
  output.push({
    archiveId: 'meswin:815',
    archiveIndex: 815,
    presentationFamily: 'meswin',
    presentationFamilyBits: 0x80,
    presentationSelector: 0,
    presentationResourceKey: 0x021B8BA4,
    presentationLeafResourceKey: 0x021B8BAC,
    filename: meswinArchive.filename,
    label: 'System message bank #815 · ' + meswinArchive.filename,
    entryCount: meswinEntries.length,
    associationStatus:
      'exact presentation family 0x80 and archive selector 0; named scene and entry scheduler unresolved',
    selectorStatus:
      'func_000E5968 selects this one-slot MESWIN archive through resource key 0x021B8BA4',
    previewCapability: 'preview-only',
    exportCapability: 'needs-research',
    source: {
      archiveHeaderZ64: parseInt(meswinArchive.romOffset, 16),
      compressedSize: meswinArchive.compSize,
      decodedSize: meswinBytes.length,
      decodedSha256: sha256(meswinBytes)
    },
    entries: meswinEntries
  });
  return output;
}

function compactAudioBlocks(audioCatalog) {
  if (!audioCatalog || !audioCatalog.region || !Array.isArray(audioCatalog.blocks) ||
      audioCatalog.blocks.length !== 63) {
    throw new Error('Sequenced-audio catalog must contain all 63 blocks.');
  }
  const requestByBlock = {};
  Object.entries(DIRECTOR_AUDIO_BLOCK_BY_REQUEST).forEach(([requestValue, blockIndex]) => {
    if (!requestByBlock[blockIndex]) requestByBlock[blockIndex] = [];
    requestByBlock[blockIndex].push(Number(requestValue));
  });
  return audioCatalog.blocks.map((block, index) => {
    const directorRequestValues = requestByBlock[index] || [];
    const context = AUDIO_BLOCK_CONTEXTS[index] || null;
    return {
      blockId: 'sequenced-audio:' + String(index).padStart(2, '0'),
      blockIndex: index,
      nativeRequestValue: nativeAudioRequestForBlockIndex(index),
      label: 'Sequenced audio block ' + String(index).padStart(2, '0') +
        ' · ' + block.offset_hex,
      channels: block.channels,
      runtimeChannelStride: block.stride,
      storedSize: block.size,
      directorRequestValues,
      contextLabel: context ? context.contextLabel : null,
      scenarioRuntimeKeys: context && context.scenarioRuntimeKeys
        ? context.scenarioRuntimeKeys.slice() : [],
      nonDirectorSourceFunction: context && context.sourceFunction
        ? context.sourceFunction : null,
      contextStatus: context
        ? 'exact non-Director request reference; this is a system association, not a cue title'
        : 'no supported semantic context',
      roundtripStatus: block.roundtrip_ok === true ? 'byte-identical-container' : 'unverified',
      endBoundaryStatus: block.end_corroborated === true
        ? 'next-header-corroborated' : block.end_source,
      associationStatus: directorRequestValues.length
        ? 'observed selector-0 opcode-0x6E route; semantic media name unresolved'
        : 'exact native request-table row; no retail Director occurrence located',
      previewCapability: 'metadata-only',
      exportCapability: 'native-source-substitution',
      source: {
        z64Start: block.offset,
        z64EndExclusive: block.offset + block.size,
        controllerDmaEndExclusive: directorRequestValues.length
          ? block.offset + block.size + 4 : null
      }
    };
  });
}

function directorSemanticFamily(name) {
  if (/dialogue|text|portrait/.test(name)) return 'dialogue-presentation';
  if (/audio/.test(name)) return 'audio';
  if (/actor|body_pose/.test(name)) return 'actor';
  if (/camera|projection|pan_zoom/.test(name)) return 'view';
  if (/sprite|effect|overlay|tint|color|opacity/.test(name)) return 'effects';
  if (/background|resource|scene_image/.test(name)) return 'scene-resources';
  if (/query|branch|barrier|bridge|counter|marker|substream|termin/.test(name)) {
    return 'flow';
  }
  return 'director-state';
}

function semanticLabel(name) {
  return String(name || '').split('_').filter(Boolean).map((part) =>
    part.charAt(0).toUpperCase() + part.slice(1)).join(' ');
}

function compactDirectorEvents(grammar, opcodeStats) {
  return grammar.map((definition) => {
    const opcodeValue = Number(definition.opcodeU32) >>> 0;
    const opcode = '0x' + opcodeValue.toString(16).toUpperCase();
    const occurrence = opcodeStats.get(opcodeValue) || { count: 0, scenes: new Set() };
    return {
      eventId: 'director-opcode:' + opcodeValue
        .toString(16).toUpperCase().padStart(8, '0'),
      opcode,
      family: directorSemanticFamily(definition.name),
      label: semanticLabel(definition.name),
      semanticName: definition.name,
      semanticSummary: definition.semanticSummary,
      confidence: definition.confidence,
      widthKind: definition.widthKind,
      sourceWordSpan: definition.sourceWordSpan,
      capability: 'preview-only',
      unresolvedJoin: definition.semanticStatus === 'structural-width-only'
        ? 'active native callee meaning' : null,
      occurrenceCount: occurrence.count,
      sceneCount: occurrence.scenes.size,
      associationStatus: occurrence.count
        ? 'observed-in-retail-selector-table' : 'defined-but-unobserved-in-retail-table'
    };
  });
}

function compactBackgroundSelectorTables() {
  const sceneEntries = SCENE_BACKGROUND_GROUP_KEYS.map((resourceKey, selector) => {
    const members = sceneGroupMembers(selector);
    return {
      selector,
      groupResourceKey: resourceKey,
      members,
      archiveAssetIds: members.map((member) => member.assetId),
      associationStatus: selector === 10
        ? 'group contains the direct B5 payload that also encloses physical archive #102'
        : (members.length ? 'exact static ordered group membership' : 'empty scene group')
    };
  });
  const environmentEntries = MODE_TWO_ENVIRONMENT_RESOURCE_KEYS.map((resourceKey, selector) => {
    const assetId = modeTwoEnvironmentAssetId(selector);
    const stageLayers = modeTwoStageLayers(selector, null).map((layer) => ({ ...layer }));
    return {
      selector,
      resourceKey: resourceKey || null,
      assetId,
      archiveAssetIds: stageLayers.map((layer) => layer.assetId),
      stageLayers,
      stageAssetIds: stageLayers.map((layer) => layer.assetId),
      associationStatus: assetId
        ? 'exact resource-4 environment base selected independently of foreground state'
        : 'empty environment selector row'
    };
  });
  const overlayEntries = MODE_TWO_OVERLAY_RESOURCE_KEYS.map((resourceKey, selector) => {
    const archiveIds = archiveAssetIds(BATTLE_BACKGROUND_ARCHIVE_ROWS[selector] || []);
    return {
      selector,
      resourceKey: resourceKey || null,
      archiveAssetIds: archiveIds,
      associationStatus: archiveIds.length
        ? 'exact foreground or occlusion resource selected independently of the environment base'
        : (resourceKey ? 'exact overlay resource; archive asset join unresolved' : 'empty overlay selector row')
    };
  });
  return [
    {
      tableId: 'background-table:scene:31',
      label: 'Scene resource groups',
      owner: 'func_002B0D30',
      selectionCondition: 'terminal class 4/5 launch preload or runtime Director opcode 0x80000006 with mode not equal to 2',
      selectorSource: 'command operand',
      tableResourceKey: 0x016B3D18,
      tableEntryZ64: 0x01C47F98,
      entryCount: sceneEntries.length,
      entries: sceneEntries
    },
    {
      tableId: 'background-table:mode2-environment:80',
      label: 'Mode-2 environment bases',
      owner: 'func_0004ED60, func_00067FA8, func_00067320, and func_00067B48',
      selectionCondition: 'mode-2 launch pre-scan seeds a nonnegative opcode-0x80000006 operand before the environment loader runs',
      selectorSource: 'launch environment byte at RAM 0x80196AED; opcode-0x80000006 operand or external derived state',
      tableResourceKey: 0x00000004,
      tableSizeWordZ64: 0x00594284,
      tableEntryZ64: 0x00594288,
      entryCount: environmentEntries.length,
      entries: environmentEntries
    },
    {
      tableId: 'background-table:mode2-overlay:80',
      label: 'Mode-2 foreground and occlusion resources',
      owner: 'func_001F309C, func_002ABFD4, func_001E4C74, and func_001FB32C',
      selectionCondition: 'the event-context initializer copies the environment selector; the fixed-overlay initializer maps launch-property bit 0x08 to selector 0 or 49; other callers remain independent',
      selectorSource: 'independent runtime scalar at RAM 0x801CEAB0',
      tableResourceKey: 0x002938C8,
      tableSizeWordZ64: 0x00827B48,
      tableEntryZ64: 0x00827B4C,
      entryCount: overlayEntries.length,
      entries: overlayEntries
    }
  ];
}

function u16(bytes, offset) {
  return (bytes[offset] << 8) | bytes[offset + 1];
}

function s16(bytes, offset) {
  const value = u16(bytes, offset);
  return value & 0x8000 ? value - 0x10000 : value;
}

function u32(bytes, offset) {
  return ((bytes[offset] * 0x1000000) + (bytes[offset + 1] << 16) +
    (bytes[offset + 2] << 8) + bytes[offset + 3]) >>> 0;
}

function aligned(value, multiple) {
  return Math.ceil(value / multiple) * multiple;
}

function imageFamily(filename, fallback) {
  const lower = String(filename || '').toLowerCase();
  if (/(efe|effect|kira|hurt|ash|point|circle)/.test(lower)) return 'effect';
  return fallback || 'sprite';
}

function classifyB5(bytes) {
  const format = bytes[2];
  const declaredCount = bytes[3];
  const recordCount = format === 5 ? 1 : declaredCount;
  let cursor = 8;
  let minX = 0;
  let minY = 0;
  let maxX = 0;
  let maxY = 0;
  let embeddedHuffCount = 0;
  const nativeRecords = [];
  let complete = recordCount > 0;
  for (let ordinal = 0; ordinal < recordCount && complete; ordinal++) {
    if (cursor + 16 > bytes.length) { complete = false; break; }
    const x = s16(bytes, cursor);
    const y = s16(bytes, cursor + 2);
    const width = u16(bytes, cursor + 4);
    const height = u16(bytes, cursor + 6);
    const size = u32(bytes, cursor + 8);
    const reserved = u32(bytes, cursor + 12);
    const colorStride = aligned(width, 4) * (format === 3 ? 4 : 2);
    const maskStride = aligned(width, 8);
    const expected = (colorStride + (format === 0 ? maskStride : 0)) * height;
    const dataOffset = cursor + 16;
    const embeddedHuff = format === 3 && size >= 14 && dataOffset + size <= bytes.length &&
      u32(bytes, dataOffset) === 0x4855FE00 &&
      u32(bytes, dataOffset + 4) === ((width << 16) | height) >>> 0 &&
      bytes.subarray(dataOffset + 8, dataOffset + 12).toString('ascii') === 'HUFF' &&
      u16(bytes, dataOffset + 12) === width / 16 * (height / 16);
    if (!width || !height || reserved !== 0 ||
        !embeddedHuff && size !== expected || cursor + 16 + size > bytes.length) {
      complete = false;
      break;
    }
    if (embeddedHuff) embeddedHuffCount += 1;
    nativeRecords.push({
      ordinal,
      x,
      y,
      width,
      height,
      dataSize: size,
      reserved
    });
    if (!ordinal) {
      minX = x; minY = y; maxX = x + width; maxY = y + height;
    } else {
      minX = Math.min(minX, x); minY = Math.min(minY, y);
      maxX = Math.max(maxX, x + width); maxY = Math.max(maxY, y + height);
    }
    cursor += 16 + size;
  }
  complete = complete && cursor === bytes.length && [0, 1, 3, 5].includes(format);
  return {
    family: 'background', container: 'bg2',
    width: complete ? maxX - minX : null,
    height: complete ? maxY - minY : null,
    format, subtype: embeddedHuffCount ? 'embedded-huff-njpg' : null, renderable: complete,
    declaredRecordCount: declaredCount,
    reference: { x: u16(bytes, 4), y: u16(bytes, 6) },
    nativeRecords: complete ? nativeRecords : null,
    previewCapability: complete
      ? (format === 0 || format === 5 || embeddedHuffCount ? 'preview-only' : 'native')
      : 'needs-research',
    unsupportedReason: complete ? null : 'The B5 record boundaries or pixel extents are invalid.'
  };
}

function classify64(bytes, filename) {
  const type = bytes[2];
  const subtype = bytes[3];
  const width = u16(bytes, 4);
  const height = u16(bytes, 6);
  let required = 0;
  let known = true;
  if (type === 0 && subtype === 2) required = 8 + aligned(width, 4) * 2 * height;
  else if (type === 0 && subtype === 3) required = 8 + aligned(width, 4) * 4 * height;
  else if (type === 2 && subtype === 0) required = 8 + aligned(width, 16) / 2 * height + 32;
  else if (type === 2 && subtype === 1) required = 8 + aligned(width, 8) * height + 512;
  else if (type === 4 && subtype === 0) required = 8 + aligned(width, 16) / 2 * height;
  else if (type === 4 && subtype === 1) required = 8 + aligned(width, 8) * height;
  else known = false;
  const complete = known && width > 0 && height > 0 && required <= bytes.length;
  return {
    family: imageFamily(filename, 'image'), container: '64', width, height,
    format: type, subtype, renderable: complete,
    previewCapability: complete ? 'native' : 'needs-research',
    unsupportedReason: complete ? null : (known
      ? 'The 64 container pixels or palette are truncated.'
      : '64 container type ' + type + ', subtype ' + subtype + ' has no Stage decoder.')
  };
}

function classifyK(bytes, filename) {
  const width = u16(bytes, 4);
  const height = u16(bytes, 6);
  const pixels = width * height;
  let complete = false;
  let format = null;
  let subtype = null;
  if (bytes[1] === 0 && bytes[2] === 0 && bytes[3] === 2) {
    format = 'rgba5551';
    complete = 8 + pixels * 2 === bytes.length;
  } else if (bytes[2] === 2 && (bytes[3] === 0 || bytes[3] === 1)) {
    format = bytes[3] === 0 ? 'ci4' : 'ci8';
    subtype = bytes[1] + 1;
    const packedSize = bytes[3] === 0 ? Math.ceil(pixels / 2) : pixels;
    complete = 8 + packedSize + subtype * 2 === bytes.length;
  }
  return {
    family: imageFamily(filename), container: 'K', width, height,
    format, subtype, renderable: complete,
    previewCapability: complete ? 'native' : 'needs-research',
    unsupportedReason: complete ? null : 'The K image header, pixels, or embedded palette are not byte-complete.'
  };
}

function rawFrameBytes(layout) {
  const width = layout.frameWidth || layout.width;
  const height = layout.frameHeight || layout.height;
  const stride = layout.rowStride || (layout.pixelFormat === 'rgba5551'
    ? width * 2 : (layout.pixelFormat === 'ci4' || layout.pixelFormat === 'i4'
      ? Math.ceil(width / 2) : width));
  return stride * height;
}

function classifyRaw(bytes, filename, layout) {
  const frameCount = layout.paletteBankCount || layout.frameCount || 1;
  const frameWidth = layout.frameWidth || layout.width;
  const frameHeight = layout.frameHeight || layout.height;
  const columns = Math.max(1, Math.min(frameCount, layout.frameColumns || frameCount));
  const storedFrameCount = layout.paletteBankCount ? 1 : frameCount;
  const expectedEnd = (layout.dataOffset || 0) +
    (layout.frameBytes || rawFrameBytes(layout)) * storedFrameCount;
  let nativeRecord = null;
  let nativeRecordValid = true;
  if (layout.nativeRecordHeader) {
    nativeRecordValid = bytes.length >= 16;
    if (nativeRecordValid) {
      nativeRecord = {
        x: s16(bytes, 0),
        y: s16(bytes, 2),
        width: u16(bytes, 4),
        height: u16(bytes, 6),
        dataSize: u32(bytes, 8),
        reserved: u32(bytes, 12),
        headerSize: 16
      };
      nativeRecordValid = nativeRecord.width === frameWidth &&
        nativeRecord.height === frameHeight && nativeRecord.dataSize === bytes.length - 16 &&
        nativeRecord.reserved === 0 && layout.dataOffset === 16;
    }
  }
  const complete = frameWidth > 0 && frameHeight > 0 && expectedEnd === bytes.length &&
    nativeRecordValid;
  return {
    family: layout.family || imageFamily(filename), container: 'raw',
    width: complete ? frameWidth * columns : null,
    height: complete ? frameHeight * Math.ceil(frameCount / columns) : null,
    format: layout.pixelFormat, subtype: null, renderable: complete,
    previewCapability: complete ? 'preview-only' : 'needs-research',
    unsupportedReason: complete ? null : (layout.nativeRecordHeader && !nativeRecordValid
      ? 'The external B5 record header does not match its mapped pixel extent.'
      : 'The mapped raw layout does not conserve every decoded byte.'),
    layout,
    nativeRecord: complete ? nativeRecord : null
  };
}

function classifyImage(bytes, filename, archiveIndex) {
  if (bytes.length >= 24 && bytes[0] === 0x42 && bytes[1] === 0x35) {
    return classifyB5(bytes);
  }
  if (bytes.length >= 8 && bytes[0] === 0x36 && bytes[1] === 0x34) {
    return classify64(bytes, filename);
  }
  if (bytes.length >= 8 && bytes[0] === 0x4B) return classifyK(bytes, filename);
  if (RAW_IMAGE_LAYOUTS[archiveIndex]) {
    return classifyRaw(bytes, filename, RAW_IMAGE_LAYOUTS[archiveIndex]);
  }
  return {
    family: imageFamily(filename), container: 'unknown',
    width: null,
    height: null,
    format: null,
    subtype: null,
    renderable: false,
    previewCapability: 'needs-research',
    unsupportedReason: 'The archive is image-like, but its container signature is unsupported.'
  };
}

function isImageCandidate(archive) {
  const filename = String(archive.filename || '').toLowerCase();
  const signature = String(archive.files && archive.files[0] && archive.files[0].signature || '')
    .replace(/[^0-9a-f]/gi, '').toUpperCase();
  return filename.endsWith('.bg2') || filename.endsWith('.n64') || filename.endsWith('.k64') ||
    !!RAW_IMAGE_LAYOUTS[archive.index] ||
    signature.startsWith('4235') || signature.startsWith('3634');
}

function compactArchiveAsset(root, archive) {
  const filePath = decodedFileForArchive(root, archive);
  const bytes = fs.readFileSync(filePath);
  const classification = classifyImage(bytes, archive.filename, archive.index);
  const consumerEvidence = ARCHIVE_CONSUMER_EVIDENCE[archive.index] || null;
  return {
    assetId: 'archive:' + archive.index,
    sourceKind: 'lha-archive',
    archiveIndex: archive.index,
    filename: archive.filename,
    displayName: SHIFT_JIS_DISPLAY_NAMES[archive.index] || archive.filename,
    family: classification.family,
    container: classification.container,
    width: classification.width,
    height: classification.height,
    format: classification.format,
    subtype: classification.subtype,
    renderable: classification.renderable,
    unsupportedReason: classification.unsupportedReason,
    previewCapability: classification.previewCapability,
    exportCapability: 'needs-research',
    layout: classification.layout || null,
    declaredRecordCount: classification.declaredRecordCount || null,
    reference: classification.reference || null,
    nativeRecords: classification.nativeRecords || null,
    nativeRecord: classification.nativeRecord || null,
    compound: null,
    consumerEvidence: consumerEvidence ? { ...consumerEvidence } : null,
    source: {
      archiveHeaderZ64: parseInt(archive.romOffset, 16),
      decodedSize: bytes.length,
      decodedSha256: sha256(bytes),
      signatureHex: bytes.subarray(0, Math.min(16, bytes.length)).toString('hex').toUpperCase(),
      compressionMethod: archive.method,
      compressedSize: archive.compSize,
      uncompressedSize: archive.uncompSize,
      consumerStatus: consumerEvidence
        ? consumerEvidence.consumerStatus : 'unresolved'
    }
  };
}

function assembleFormatFiveCompounds(assets) {
  const byArchiveIndex = new Map(assets.map((asset) => [asset.archiveIndex, asset]));
  assets.filter((asset) => asset.container === 'bg2' && asset.format === 5)
    .forEach((root) => {
      if (!root.renderable || !Number.isInteger(root.declaredRecordCount) ||
          root.declaredRecordCount < 1 || !Array.isArray(root.nativeRecords) ||
          root.nativeRecords.length !== 1 || !root.reference) {
        throw new Error(root.assetId + ' has incomplete format-5 root metadata.');
      }
      const members = [];
      for (let ordinal = 0; ordinal < root.declaredRecordCount; ordinal++) {
        const asset = byArchiveIndex.get(root.archiveIndex + ordinal);
        if (!asset) {
          throw new Error(root.assetId + ' is missing external record ordinal ' + ordinal + '.');
        }
        const record = ordinal === 0 ? root.nativeRecords[0] : asset.nativeRecord;
        if (!record || record.width !== asset.width || record.height !== asset.height) {
          throw new Error(asset.assetId + ' does not match format-5 record ordinal ' + ordinal + '.');
        }
        members.push({
          ordinal,
          assetId: asset.assetId,
          x: record.x,
          y: record.y,
          width: record.width,
          height: record.height,
          dataSize: record.dataSize
        });
      }
      const minX = Math.min(...members.map((member) => member.x));
      const minY = Math.min(...members.map((member) => member.y));
      const maxX = Math.max(...members.map((member) => member.x + member.width));
      const maxY = Math.max(...members.map((member) => member.y + member.height));
      root.width = maxX - minX;
      root.height = maxY - minY;
      root.compound = {
        kind: 'b5-format5-external-records',
        declaredMemberCount: root.declaredRecordCount,
        originX: minX,
        originY: minY,
        width: root.width,
        height: root.height,
        reference: { ...root.reference },
        members
      };
      members.slice(1).forEach((member) => {
        const asset = byArchiveIndex.get(Number(member.assetId.split(':')[1]));
        asset.compoundOwnerAssetId = root.assetId;
      });
    });
  return assets;
}

function compactSectionCAsset(row, masterRomSha256) {
  const ordinal = String(row.index).padStart(2, '0');
  const resourceKey = MODE_TWO_ENVIRONMENT_RESOURCE_KEYS[row.index];
  return {
    assetId: 'section-c-njpg:' + ordinal,
    sourceKind: 'section-c-njpg',
    archiveIndex: null,
    filename: null,
    displayName: 'Mode-2 environment ' + ordinal,
    family: 'background',
    container: 'section-c-njpg',
    width: 320,
    height: 240,
    format: 'HUFF',
    subtype: 'native-rsp-transposed-flat4-bt601',
    renderable: true,
    unsupportedReason: null,
    previewCapability: 'preview-only',
    exportCapability: 'needs-research',
    layout: null,
    consumerEvidence: {
      evidenceGrade: 'direct',
      consumerStatus: 'resource-4 mode-2 environment selector ' + row.index,
      firstMissingJoin: 'native fixed-point pixel rounding only'
    },
    source: {
      resourceKey,
      masterRomSha256,
      normalizedZ64Sha256: NORMALIZED_US_REV0_Z64_SHA256,
      z64Start: row.start,
      z64EndExclusive: row.end,
      storedSize: row.size,
      storedSha256: row.sha256,
      consumerStatus: 'native mode-2 environment base',
      identityStatus: 'exact resource-4 environment selector ' + row.index
    }
  };
}

function compactModeTwoDirectEnvironmentAsset(row) {
  const resourceKey = row[0];
  const resourceHex = hex(resourceKey, 8);
  const container = row[5];
  return {
    assetId: 'mode2-environment:' + resourceHex,
    sourceKind: 'rom-resource',
    archiveIndex: null,
    filename: null,
    displayName: 'Mode-2 environment 0x' + resourceHex,
    family: 'background',
    container,
    width: row[6],
    height: row[7],
    format: row[8] == null ? 'HUFF' : row[8],
    subtype: container === 'embedded-njpg'
      ? 'native-rsp-transposed-flat4-bt601' : 'embedded-huff-njpg',
    renderable: true,
    unsupportedReason: null,
    previewCapability: 'preview-only',
    exportCapability: 'native-mode2-environment-selector',
    layout: null,
    declaredRecordCount: row[9],
    reference: row[10] == null ? null : { x: row[10], y: row[11] },
    nativeRecords: null,
    nativeRecord: null,
    compound: null,
    consumerEvidence: {
      evidenceGrade: 'direct',
      consumerStatus: 'exact resource-4 mode-2 environment base',
      firstMissingJoin: 'native fixed-point pixel rounding only'
    },
    source: {
      resourceKey,
      sizeWordZ64: row[1],
      z64Start: row[2],
      z64EndExclusive: row[3],
      storedSize: row[3] - row[2],
      storedSha256: row[4],
      compressionKind: 'none',
      decodedSize: row[3] - row[2],
      decodedSha256: row[4],
      consumerStatus: 'native mode-2 environment base'
    }
  };
}

function compactSceneGroupResourceAsset(row) {
  const resourceKey = row[0];
  const resourceHex = hex(resourceKey, 8);
  const compressionKind = row[5];
  return {
    assetId: 'scene-resource:' + resourceHex,
    sourceKind: 'rom-resource',
    archiveIndex: null,
    filename: null,
    displayName: 'Scene visual resource 0x' + resourceHex,
    family: 'background',
    container: 'bg2',
    width: row[12],
    height: row[13],
    format: row[8],
    subtype: row[8] === 3 ? 'embedded-huff-njpg' : null,
    renderable: true,
    unsupportedReason: null,
    previewCapability: 'preview-only',
    exportCapability: 'native-group-selector',
    layout: null,
    consumerEvidence: {
      evidenceGrade: 'direct',
      consumerStatus: 'exact ordered member of one or more native scene groups',
      firstMissingJoin: 'final per-member depth and blend semantics'
    },
    source: {
      resourceKey,
      sizeWordZ64: row[1],
      z64Start: row[2],
      z64EndExclusive: row[3],
      storedSize: row[3] - row[2],
      storedSha256: row[4],
      compressionKind,
      decodedSize: row[6],
      decodedSha256: row[7],
      b5Format: row[8],
      b5RecordCount: row[9],
      b5ReferenceWidth: row[10],
      b5ReferenceHeight: row[11],
      consumerStatus: 'native scene-group member'
    }
  };
}

const POSE_OPCODE_WIDTHS = Object.freeze([
  1, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1,
  1, 4, 3, 3, 2, 3, 3, 3, 3, 3, 4
]);

function canonicalPoseRecords(program) {
  const raw = String(program.canonicalRawHex || '').replace(/^0x/i, '');
  if (!raw.length || raw.length % 2 || !/^[0-9A-F]+$/i.test(raw)) {
    throw new Error(program.id + ' has no canonical pose-program bytes.');
  }
  const bytes = Buffer.from(raw, 'hex');
  const recordCount = bytes[0];
  let cursor = 1;
  const frames = [];
  const controlOpcodes = [];
  const records = [];
  for (let ordinal = 0; ordinal < recordCount; ordinal++) {
    const opcode = bytes[cursor];
    const width = POSE_OPCODE_WIDTHS[opcode];
    if (!width || cursor + width > bytes.length) {
      throw new Error(program.id + ' has an invalid pose opcode at record ' + ordinal + '.');
    }
    records.push({
      ordinal,
      opcode,
      operands: Array.from(bytes.subarray(cursor + 1, cursor + width))
    });
    if (opcode === 1) {
      frames.push({ frameToken: bytes[cursor + 1], durationFrames: bytes[cursor + 2] });
    } else if (opcode === 0x15) {
      frames.push({
        frameToken: bytes[cursor + 1] | (bytes[cursor + 2] << 8),
        durationFrames: bytes[cursor + 3]
      });
    } else if (opcode !== 0) {
      const label = '0x' + opcode.toString(16).toUpperCase().padStart(2, '0');
      if (!controlOpcodes.includes(label)) controlOpcodes.push(label);
    }
    cursor += width;
  }
  if (cursor !== bytes.length) throw new Error(program.id + ' canonical pose bytes have a trailing gap.');
  return { frames, controlOpcodes, records };
}

function compactPoseProgram(program) {
  const decoded = canonicalPoseRecords(program);
  const frames = decoded.frames;
  const controlOpcodes = decoded.controlOpcodes;
  const records = decoded.records;
  return {
    poseId: 'cutscene-pose:' + program.bank + ':' + program.key + ':' + program.facing,
    programId: program.id,
    physicalStateId: program.physicalStateId,
    bank: program.bank,
    animationKey: program.key,
    facing: program.facing,
    stateIndex: program.stateIndex,
    variant: program.variant,
    descriptorSlot: program.descriptorSlot,
    sourceProgramDefined: program.sourceProgramDefined,
    sourceProgramIdentityIds: program.sourceProgramIdentityIds.slice(),
    frames,
    records,
    durationFrames: frames.reduce((total, frame) => total + frame.durationFrames, 0),
    controlOpcodes,
    emptyProgram: frames.length === 0 && controlOpcodes.length === 0,
    executionStatus: frames.length === 0 && controlOpcodes.length === 0
      ? 'zero-entry pose program; yields without changing the actor frame'
      : 'counted physical pose program',
    canonicalRawSha256: program.canonicalRawSha256 || null,
    sourceArtifact: program.sourceArtifact || null,
    publicationStatus: program.publicationStatus || 'accepted physical-program publication',
    variantStatus: 'runtime actor variant selects sprite children; physical pose bytecode is variant-independent',
    descriptorSlotStatus: 'scene-local descriptor occurrence is provenance only; the bank selects one exact descriptor',
    selectedChildOrdinal: 0,
    childSelectionStatus: 'actor variant selector; out-of-range values fall back to child 0',
    previewCapability: 'preview-only',
    exportCapability: 'native'
  };
}

function numericKey(value, label) {
  const parsed = typeof value === 'number' ? value : parseInt(String(value), 16);
  if (!Number.isInteger(parsed) || parsed <= 0) throw new Error(label + ' is not a resource key.');
  return parsed;
}

function compactPoseCatalog(vocabulary, spriteCorpus, scenes, selectorRows, physicalStateRows) {
  const sourcePrograms = vocabulary.physicalProgramIdentities || [];
  const physicalStateById = new Map();
  physicalStateRows.forEach((state) => {
    if (physicalStateById.has(state.id)) throw new Error('Duplicate physical pose state ' + state.id + '.');
    physicalStateById.set(state.id, state);
  });
  const programs = selectorRows.map((selector) => {
    const state = physicalStateById.get(selector.physicalStateId);
    if (!state || state.bank !== selector.bank || state.stateIndex !== selector.stateIndex ||
        state.selector.key !== selector.key || state.selector.facing !== selector.facing) {
      throw new Error(selector.id + ' has no matching physical ROM pose state.');
    }
    if (Array.isArray(state.decodeWarnings) && state.decodeWarnings.length) {
      throw new Error(selector.id + ' physical ROM pose state has decode warnings.');
    }
    return {
      id: 'physical-program:bank:' + String(selector.bank).padStart(3, '0') +
        ':state:' + String(selector.stateIndex).padStart(4, '0'),
      physicalStateId: state.id,
      bank: selector.bank,
      key: selector.key,
      facing: selector.facing,
      stateIndex: selector.stateIndex,
      variant: null,
      descriptorSlot: null,
      sourceProgramDefined: selector.sourceProgramDefined === true,
      sourceProgramIdentityIds: (state.sourceProgramIdentityIds || []).slice(),
      canonicalRawHex: state.canonicalRawHex,
      canonicalRawSha256: state.rawSha256,
      sourceArtifact: 'wiki/cutscene-all-bank-pose-rom-map-static-20260711/' + state.artifact,
      publicationStatus: selector.sourceProgramDefined
        ? 'exact physical ROM state with scene-local source-program publication'
        : 'exact physical ROM state; scene-local source-program publication is absent'
    };
  }).map(compactPoseProgram)
    .sort((left, right) => left.bank - right.bank ||
      left.animationKey - right.animationKey || left.facing - right.facing ||
      left.stateIndex - right.stateIndex);
  const poseCounts = new Map();
  for (const program of programs) {
    poseCounts.set(program.poseId, (poseCounts.get(program.poseId) || 0) + 1);
  }
  for (const program of programs) program.alternativeProgramCount = poseCounts.get(program.poseId);
  const byBank = new Map();
  for (const program of programs) {
    if (!byBank.has(program.bank)) byBank.set(program.bank, []);
    byBank.get(program.bank).push(program);
  }
  const descriptors = spriteCorpus && spriteCorpus.cutscene && spriteCorpus.cutscene.descriptors;
  if (!Array.isArray(descriptors) || descriptors.length !== 68) {
    throw new Error('Sprite corpus must contain all 68 Cutscene descriptors.');
  }
  const usesByBank = new Map();
  scenes.forEach((scene) => scene.actors.forEach((actor) => {
    if (!Number.isInteger(actor.bank) || actor.bank < 1 || actor.bank > 68) return;
    if (!usesByBank.has(actor.bank)) {
      usesByBank.set(actor.bank, { actorChannels: 0, sceneIds: new Set() });
    }
    const use = usesByBank.get(actor.bank);
    use.actorChannels += 1;
    use.sceneIds.add(scene.sceneId);
  }));
  const selectorsByBank = new Map();
  (selectorRows || []).forEach((selector) => {
    if (!selectorsByBank.has(selector.bank)) selectorsByBank.set(selector.bank, []);
    selectorsByBank.get(selector.bank).push(selector);
  });
  const actorArtSources = (vocabulary.globalBanks || []).map((bank) => {
    const rows = byBank.get(bank) || [];
    const physicalSelectors = selectorsByBank.get(bank) || [];
    const renderableSelectors = new Set(rows.filter((row) => row.frames.length)
      .map((row) => row.poseId));
    const descriptor = descriptors[bank - 1];
    if (!descriptor) throw new Error('Sprite corpus has no descriptor for Cutscene bank ' + bank + '.');
    const family = ACTOR_ART_FAMILY_LABELS[bank] || null;
    const use = usesByBank.get(bank) || { actorChannels: 0, sceneIds: new Set() };
    return {
      assetId: 'cutscene-art-bank:' + bank,
      bank,
      label: family
        ? family.label + ' · Actor Art Source ' + bank : 'Actor Art Source ' + bank,
      familyLabel: family ? family.label : null,
      identityStatus: family ? family.status : 'technical identity only',
      identityScope: family ? family.scope : 'no supported friendly actor or family identity',
      stockActorChannelCount: use.actorChannels,
      stockSceneUseCount: use.sceneIds.size,
      stockSceneIds: Array.from(use.sceneIds).sort(),
      poseCount: physicalSelectors.length,
      renderablePoseCount: renderableSelectors.size,
      physicalProgramCount: new Set(rows.map((row) => row.physicalStateId)).size,
      animationKeys: Array.from(new Set(physicalSelectors.map((row) => row.key))).sort((a, b) => a - b),
      facings: Array.from(new Set(physicalSelectors.map((row) => row.facing))).sort((a, b) => a - b),
      descriptorKey: numericKey(descriptor.descriptorKey, 'Descriptor key'),
      descriptorMemberCount: descriptor.memberCount,
      artCount: descriptor.artCount,
      metadataKey: numericKey(descriptor.metadataKey, 'Metadata key'),
      metadataDecodedLength: descriptor.metadataDecodedLength,
      poseKey: numericKey(descriptor.poseKey, 'Pose key'),
      poseDecodedLength: descriptor.poseDecodedLength,
      configKey: numericKey(descriptor.configKey, 'Configuration key'),
      configDecodedLength: descriptor.configDecodedLength,
      lookupKey: numericKey(descriptor.lookupKey, 'Lookup key'),
      lookupDecodedLength: descriptor.lookupDecodedLength,
      lookupBankCount: descriptor.lookupBankCount,
      selectedChildOrdinal: 0,
      childSelectionStatus: 'actor variant selector; out-of-range values fall back to child 0',
      previewCapability: renderableSelectors.size ? 'preview-only' : 'needs-research',
      exportCapability: 'native'
    };
  });
  return {
    actorArtSources,
    posePrograms: programs,
    sourcePoseProgramCount: sourcePrograms.length,
    sourcePublishedPhysicalStateCount: programs.filter((program) =>
      program.sourceProgramDefined).length
  };
}

function compactPoseSelectors(rows) {
  return rows.map((row) => ({
    selectorId: row.id,
    poseId: 'cutscene-pose:' + row.bank + ':' + row.key + ':' + row.facing,
    bank: row.bank,
    animationKey: row.key,
    facing: row.facing,
    stateIndex: row.stateIndex,
    physicalStateId: row.physicalStateId,
    sourceProgramDefined: row.sourceProgramDefined === true,
    noProgramReason: row.noProgramReason || null,
    resolutionDisposition: row.resolutionDisposition
  })).sort((left, right) => left.bank - right.bank ||
    left.animationKey - right.animationKey || left.facing - right.facing ||
    left.stateIndex - right.stateIndex);
}

function buildRetailDirectorGrammar(corpusGrammar, corpusNodes) {
  const samples = new Map();
  corpusNodes.forEach((node) => {
    const opcode = Number(node.opcode_u32) >>> 0;
    if (!samples.has(opcode)) samples.set(opcode, node);
  });
  const definitions = corpusGrammar.map((definition) => {
    const opcode = Number(definition.opcode_u32) >>> 0;
    const sample = samples.get(opcode) || null;
    const wordCount = Number(definition.source_word_span);
    let operandRoles = sample && Array.isArray(sample.operands)
      ? sample.operands.map((operand) => operand.role) : [];
    if (sample && sample.termination && operandRoles.length === wordCount - 2) {
      operandRoles.push('termination_trailer');
    }
    if (!sample) {
      if (definition.width_kind === 'query') {
        operandRoles = ['compare_mode', 'target'];
        if (wordCount === 4) operandRoles.push('producer_input');
      } else if (definition.width_kind === 'terminal-with-trailer') {
        operandRoles = ['termination_trailer'];
      } else {
        operandRoles = Array.from({ length: wordCount - 1 }, (_, index) =>
          'operand_' + index);
      }
    }
    if (operandRoles.length !== wordCount - 1) {
      throw new Error('Director grammar operand roles are incomplete for ' +
        definition.opcode + '.');
    }
    return {
      opcodeU32: opcode,
      name: definition.name,
      semanticSummary: definition.semantic_summary,
      confidence: definition.confidence,
      sourceWordSpan: wordCount,
      widthKind: definition.width_kind,
      nodeType: sample ? sample.node_type :
        (definition.width_kind === 'query' ? 'query' :
          (definition.width_kind === 'terminal-with-trailer' ? 'termination' : 'command')),
      operandRoles,
      queryRecordKind: definition.width_kind === 'query'
        ? (wordCount === 4 ? 'Q4' : 'Q3') : null,
      terminationKind: definition.width_kind === 'terminal-with-trailer'
        ? 'stream_terminator_with_trailer' : null,
      newlyRecoveredFromDispatch: definition.newly_recovered_from_dispatch === true,
      semanticStatus: 'accepted-153-command-definition'
    };
  });
  definitions.push({
    opcodeU32: 0x5E,
    name: 'unresolved_native_command_5e',
    semanticSummary:
      'Consumes three native operands. The parser width and dispatch are exact; the active callee meaning remains unresolved.',
    confidence: 'Structural',
    sourceWordSpan: 4,
    widthKind: 'fixed',
    nodeType: 'command',
    operandRoles: ['operand_0', 'operand_1', 'operand_2'],
    queryRecordKind: null,
    terminationKind: null,
    newlyRecoveredFromDispatch: true,
    semanticStatus: 'structural-width-only'
  });
  definitions.sort((left, right) => left.opcodeU32 - right.opcodeU32);
  if (definitions.length !== 154 ||
      new Set(definitions.map((definition) => definition.opcodeU32)).size !== 154) {
    throw new Error('Retail Director grammar must contain 154 unique command values.');
  }
  return definitions;
}

function tileRetailDirector(words, assetId, grammarByOpcode, options = {}) {
  const prefix = assetId.split(':').pop().toUpperCase();
  const nodes = [];
  let cursor = 0;
  while (cursor < words.length) {
    const opcode = Number(words[cursor]) >>> 0;
    const definition = grammarByOpcode.get(opcode);
    if (!definition) {
      throw new Error(assetId + ' has no grammar for opcode 0x' + hex(opcode, 8) +
        ' at word ' + cursor + '.');
    }
    const terminalWithoutTrailer = options.terminalWithoutTrailer === true &&
      opcode === 0x80000001 && cursor === words.length - 1;
    const wordCount = terminalWithoutTrailer ? 1 : definition.sourceWordSpan;
    const endWord = cursor + wordCount;
    if (endWord > words.length) {
      throw new Error(assetId + ' command at word ' + cursor +
        ' extends beyond its decoded payload.');
    }
    nodes.push({
      id: 'node:' + prefix + ':w' + hex(cursor, 4),
      startWord: cursor,
      endWord,
      wordCount,
      opcode,
      opcode_u32: opcode,
      opcodeText: '0x' + opcode.toString(16).toUpperCase(),
      operands: words.slice(cursor + 1, endWord).map(signed),
      rawWords: words.slice(cursor, endWord),
      definition
    });
    cursor = endWord;
  }
  const terminal = nodes[nodes.length - 1];
  const expectedTerminalWords = options.terminalWithoutTrailer === true ? 1 : 2;
  if (!terminal || terminal.opcode !== 0x80000001 ||
      terminal.wordCount !== expectedTerminalWords) {
    throw new Error(assetId + ' lacks its exact ' +
      (expectedTerminalWords === 1 ? 'continuation terminal' :
        'terminal-with-trailer boundary') + '.');
  }
  return nodes;
}

function animatedSceneSpriteRotationRouteCounts(nodes) {
  const counts = { directOperand: 0, sampledScenePath: 0, resourcePath: 0 };
  nodes.forEach((node) => {
    if (node.opcode !== 0x63) return;
    if (signed(node.rawWords[9]) === -1) counts.directOperand += 1;
    else if (signed(node.rawWords[8]) === -1) counts.sampledScenePath += 1;
    else counts.resourcePath += 1;
  });
  return counts;
}

function readDirectorContinuationStreams(z64, grammar) {
  const grammarByOpcode = new Map(grammar.map((definition) => [
    definition.opcodeU32, definition
  ]));
  const tablePrefixStart = DIRECTOR_RESOURCE_BASE_Z64 +
    DIRECTOR_CONTINUATION_TABLE_KEY;
  if (readU32(z64, tablePrefixStart, 'Director continuation table prefix') !==
      DIRECTOR_CONTINUATION_TABLE_BYTES) {
    throw new Error('The Director continuation table size prefix changed.');
  }
  const tablePayloadStart = tablePrefixStart + 4;
  const keys = DIRECTOR_CONTINUATION_KEYS.map((expectedKey, selector) => {
    const key = readU32(z64, tablePayloadStart + selector * 4,
      'Director continuation selector ' + selector);
    if (key !== expectedKey) {
      throw new Error('Director continuation selector ' + selector +
        ' changed from 0x' + hex(expectedKey, 8) + '.');
    }
    return key;
  });
  return keys.map((resourceKey, selector) => {
    const z64PrefixStart = DIRECTOR_RESOURCE_BASE_Z64 + resourceKey;
    const storedPayloadLength = readU32(z64, z64PrefixStart,
      'Director continuation ' + selector + ' prefix');
    const z64PayloadStart = z64PrefixStart + 4;
    const z64PayloadEndExclusive = z64PayloadStart + storedPayloadLength;
    if (storedPayloadLength < 4 || z64PayloadEndExclusive > z64.length) {
      throw new Error('Director continuation ' + selector +
        ' has an invalid stored envelope.');
    }
    const payload = z64.subarray(z64PayloadStart, z64PayloadEndExclusive);
    const decoded = decodeCustomLz(payload,
      'Director continuation ' + selector);
    if (decoded.bytes.length !== DIRECTOR_CONTINUATION_DECODED_BYTES[selector]) {
      throw new Error('Director continuation ' + selector +
        ' decoded length changed.');
    }
    const words = wordsFromBytes(decoded.bytes,
      'Director continuation ' + selector);
    const assetId = 'director-continuation:' + selector;
    const nodes = tileRetailDirector(words, assetId, grammarByOpcode, {
      terminalWithoutTrailer: true
    });
    return {
      selector,
      assetId,
      resourceKey,
      z64PrefixStart,
      z64PayloadStart,
      z64PayloadEndExclusive,
      storedPayloadLength,
      decodedLength: decoded.bytes.length,
      decodedWordCount: words.length,
      decodedSha256: sha256(decoded.bytes),
      runtimeNodeCount: nodes.length,
      queryCount: nodes.filter((node) =>
        node.definition.widthKind === 'query').length,
      animatedSceneSpriteRotationRoutes:
        animatedSceneSpriteRotationRouteCounts(nodes),
      terminalWordStart: nodes[nodes.length - 1].startWord,
      terminalWithoutTrailer: true,
      evidenceStatus: 'native-static-continuation-table'
    };
  });
}

function retailActorTemplates(assetId, nodes, selectors) {
  const firstPlaceBySlot = new Map();
  nodes.forEach((node) => {
    if (node.opcode !== 0x14 || node.wordCount !== 10) return;
    const slot = Number(node.rawWords[1]) >>> 0;
    if (!firstPlaceBySlot.has(slot)) firstPlaceBySlot.set(slot, node);
  });
  return Array.from(firstPlaceBySlot.entries()).sort((left, right) => left[0] - right[0])
    .map(([slot, node]) => {
      const bank = signed(node.rawWords[2]);
      const animationKey = signed(node.rawWords[3]);
      const facing = signed(node.rawWords[4]);
      const selector = selectors.get(actorSelectorKey(bank, animationKey, facing)) || null;
      const rawVariantSelector = signed(node.rawWords[9]);
      const variantTranslationIndex = launchTranslationIndex(node.rawWords[9]);
      const variantSelector = variantTranslationIndex === null
        ? rawVariantSelector & 0xFF : 0;
      function coordinate(raw) {
        raw = signed(raw);
        return raw === -1000 ? null : raw / 1000;
      }
      const actorId = 'actor:' + assetId.split(':').pop().toLowerCase() + ':slot:' +
        String(slot).padStart(2, '0');
      return {
        actorId,
        slot,
        label: null,
        bank,
        animationKey,
        facing,
        x: coordinate(node.rawWords[5]),
        y: coordinate(node.rawWords[6]),
        z: coordinate(node.rawWords[7]),
        poseResolutionId: selector
          ? 'cutscene-pose:' + bank + ':' + animationKey + ':' + facing : null,
        sourcePoseResolutionId: null,
        physicalStateId: selector ? selector.physicalStateId : null,
        stateIndex: selector ? selector.stateIndex : null,
        sourceProgramDefined: selector ? selector.sourceProgramDefined === true : false,
        initializationCandidateId: null,
        initializationSourceOpcode: '0X14',
        initializationStatus: selector
          ? 'opcode 0x14 record producer with located physical state'
          : 'opcode 0x14 record producer with resolver-invalid selector',
        controlEntryAlias: null,
        recordProducer: true,
        variantSelector,
        rawVariantSelector,
        variantSelectorTranslationIndex: variantTranslationIndex,
        variantSelectorStatus: variantTranslationIndex === null
          ? 'exact opcode-0x14 operand 9 narrowed to its runtime byte'
          : 'native launch-table operand unresolved; appearance zero is an explicit preview fallback',
        selectorStatus: selector
          ? (selector.sourceProgramDefined
            ? 'physical state and source program located'
            : 'exact physical ROM state located; scene-local source publication is absent')
          : 'record produced but selector resolver fails',
        visibilityStatus: 'record producer is exact; runtime reachability remains unresolved'
      };
    });
}

function sourceNodesForRetailProfile(nodes) {
  return nodes.map((node) => ({
    id: node.id,
    startWord: node.startWord,
    endWord: node.endWord,
    opcode: node.opcodeText,
    opcode_u32: node.opcode,
    operands: node.operands.slice()
  }));
}

function tryReadTerminatedEventCursorTable(decoded, tableCursor) {
  const entries = [];
  for (let offset = tableCursor; offset + 1 < decoded.length; offset += 2) {
    const cursor = decoded.readUInt16BE(offset);
    if (cursor === 0xFFFF) {
      return { tableCursor, entries, terminatorOffset: offset };
    }
    if ((cursor & 1) !== 0 || cursor >= decoded.length) return null;
    entries.push({ entryIndex: entries.length, cursor });
  }
  return null;
}

function readEventSequenceInventory(decoded, eventRow, label) {
  const outerEntryCount = eventRow <= EVENT_SCHEDULER_LAST_ROW
    ? EVENT_SCHEDULER_OUTER_ENTRY_COUNT : EVENT_SPECIAL_OUTER_ENTRY_COUNT;
  if (decoded.length < outerEntryCount * 2) {
    throw new Error(label + ' is shorter than its outer cursor table.');
  }
  const outerEntries = [];
  for (let outerIndex = 0; outerIndex < outerEntryCount; outerIndex += 1) {
    const cursor = decoded.readUInt16BE(outerIndex * 2);
    if (cursor !== 0 && cursor !== 0xFFFF &&
        ((cursor & 1) !== 0 || cursor >= decoded.length)) {
      throw new Error(label + ' has invalid outer cursor 0x' + hex(cursor, 4) +
        ' at index ' + outerIndex + '.');
    }
    outerEntries.push({ outerIndex, cursor });
  }
  const outerIndexesByCursor = new Map();
  outerEntries.forEach((entry) => {
    if (entry.cursor === 0 || entry.cursor === 0xFFFF) return;
    if (!outerIndexesByCursor.has(entry.cursor)) outerIndexesByCursor.set(entry.cursor, []);
    outerIndexesByCursor.get(entry.cursor).push(entry.outerIndex);
  });
  const sequenceTables = [];
  const sequenceByCursor = new Map();
  function addSequence(cursor, path) {
    if (!sequenceByCursor.has(cursor)) {
      sequenceByCursor.set(cursor, { cursor, paths: [] });
    }
    sequenceByCursor.get(cursor).paths.push(path);
  }
  outerIndexesByCursor.forEach((outerEntryIndexes, outerCursor) => {
    const table = tryReadTerminatedEventCursorTable(decoded, outerCursor);
    if (!table) {
      addSequence(outerCursor, {
        kind: 'direct-outer-sequence',
        outerEntryIndexes: outerEntryIndexes.slice(),
        sequenceTableCursor: null,
        sequenceEntryIndexes: []
      });
      return;
    }
    sequenceTables.push(table);
    const entryIndexesByCursor = new Map();
    table.entries.forEach((entry) => {
      if (!entryIndexesByCursor.has(entry.cursor)) entryIndexesByCursor.set(entry.cursor, []);
      entryIndexesByCursor.get(entry.cursor).push(entry.entryIndex);
    });
    entryIndexesByCursor.forEach((sequenceEntryIndexes, cursor) => {
      addSequence(cursor, {
        kind: 'nested-scheduler-sequence',
        outerEntryIndexes: outerEntryIndexes.slice(),
        sequenceTableCursor: outerCursor,
        sequenceEntryIndexes: sequenceEntryIndexes.slice()
      });
    });
  });
  const sequences = Array.from(sequenceByCursor.values())
    .sort((left, right) => left.cursor - right.cursor);
  return {
    outerEntries,
    distinctOuterCursorCount: outerIndexesByCursor.size,
    sequenceTables,
    sequenceEntryCount: sequenceTables.reduce((total, table) =>
      total + table.entries.length, 0),
    directOuterSequenceCount: sequences.filter((sequence) => sequence.paths.some((path) =>
      path.kind === 'direct-outer-sequence')).length,
    sequences
  };
}

function eventLaunchEntryOwner(sequenceInventory, launchOffset, label) {
  const eligible = sequenceInventory.sequences.filter((sequence) =>
    sequence.cursor <= launchOffset);
  if (!eligible.length) {
    throw new Error(label + ' at byte 0x' + hex(launchOffset, 4) +
      ' precedes every event bytecode-sequence cursor.');
  }
  const sequence = eligible[eligible.length - 1];
  return {
    entryCursor: sequence.cursor,
    entryOffset: launchOffset - sequence.cursor,
    entryPaths: sequence.paths.map((path) => ({
      kind: path.kind,
      outerEntryIndexes: path.outerEntryIndexes.slice(),
      sequenceTableCursor: path.sequenceTableCursor,
      sequenceEntryIndexes: path.sequenceEntryIndexes.slice()
    }))
  };
}

function eventUnsigned16(value) {
  return value & 0xFFFF;
}

function eventSigned16(value) {
  return value & 0x8000 ? value - 0x10000 : value;
}

function eventSigned8(value) {
  return value & 0x80 ? value - 0x100 : value;
}

function eventRelativeByteCursor(currentByteCursor, relativeWords) {
  return (((currentByteCursor / 2) + 1 + relativeWords) & 0xFFFF) * 2;
}

function initialEventTranslationTable() {
  return new Map(Array.from({ length: EVENT_TRANSLATION_TABLE_TRACKED_ENTRIES },
    (_, index) => [index, 0]));
}

function eventExternalRequestProfile(operand) {
  const requestCodes = [
    0x8002, 0x8003, 0x8005, 0x8007,
    0xFFFE, 0xFFFE, 0xFFFE, 0xFFFE, 0xFFFE, 0xFFFE,
    0x0002, 0x0003, 0x0005, 0x0007,
    0xFFFE, 0xFFFE, 0xFFFE,
    0x0013, 0x0011, 0x8007, 0x8007,
    0x8016, 0x8016, 0x8016, 0x8016, 0xFFFD, 0x8016
  ];
  const stateWrites = [];
  if (operand === 4 || operand === 14 || operand === 20 || operand === 21) {
    stateWrites.push({
      field: 'request-variant',
      ramAddress: '0x801939D1',
      value: operand === 20 ? 2 : (operand === 21 ? 0 : 1)
    });
  }
  if (operand >= 22 && operand <= 25 || operand === 27) {
    const modeA = operand === 22 ? 1 : (operand === 27 ? 3 : 0);
    stateWrites.push({
      field: 'request-mode-a',
      ramAddress: '0x8018FAE0',
      value: modeA
    });
    if (operand >= 23 && operand <= 25) {
      stateWrites.push({
        field: 'request-mode-b',
        ramAddress: '0x8018FBDC',
        value: operand - 23
      });
    }
  }
  return {
    operand,
    requestCode: operand >= 1 && operand <= requestCodes.length
      ? requestCodes[operand - 1] : 0xFFFE,
    requestCodeStorage: 'RAM halfword 0x8018F1A2',
    stateWrites,
    requestAcceptanceSignal: 'RAM byte 0x80197B02',
    requestAcceptanceCondition: 'set-synchronously-by-opcode-0x13',
    resumeTiming: 'next-event-state-processor-call',
    evidenceStatus: 'native-static-event-vm'
  };
}

function cloneEventStaticState(state) {
  return {
    pc: state.pc,
    invocationCursor: state.invocationCursor,
    registers: state.registers.slice(),
    callStack: state.callStack.slice(),
    valueStack: state.valueStack.slice(),
    properties: new Map(state.properties),
    translations: new Map(state.translations),
    substitutionSourceA: new Map(state.substitutionSourceA),
    substitutionSourceB: new Map(state.substitutionSourceB),
    precedingDirectorSelector: state.precedingDirectorSelector,
    precedingDirectorLaunchOffset: state.precedingDirectorLaunchOffset,
    precedingDirectorInvocationCursor: state.precedingDirectorInvocationCursor,
    updatesSincePrecedingDirectorRequest: state.updatesSincePrecedingDirectorRequest,
    precedingDirectorLaunchCount: state.precedingDirectorLaunchCount,
    precedingExternalRequest: state.precedingExternalRequest === null
      ? null : {
        ...state.precedingExternalRequest,
        stateWrites: state.precedingExternalRequest.stateWrites.map((write) => ({ ...write }))
      }
  };
}

function eventStaticStateKey(state) {
  return state.pc + ':' + state.invocationCursor + ':' +
    state.callStack.map((value) => value === null ? '?' : value).join(',') + ':' +
    state.valueStack.length + ':' + state.precedingDirectorLaunchCount + ':' +
    (state.precedingDirectorLaunchOffset === null
      ? '?' : state.precedingDirectorLaunchOffset) + ':' +
    (state.updatesSincePrecedingDirectorRequest === null
      ? '?' : state.updatesSincePrecedingDirectorRequest);
}

function joinEventStaticValue(left, right) {
  return left === right ? left : null;
}

function mergeEventStaticState(target, incoming) {
  let changed = false;
  for (let index = 0; index < target.registers.length; index += 1) {
    const joined = joinEventStaticValue(target.registers[index], incoming.registers[index]);
    if (joined !== target.registers[index]) {
      target.registers[index] = joined;
      changed = true;
    }
  }
  for (let index = 0; index < target.callStack.length; index += 1) {
    const joined = joinEventStaticValue(target.callStack[index], incoming.callStack[index]);
    if (joined !== target.callStack[index]) {
      target.callStack[index] = joined;
      changed = true;
    }
  }
  for (let index = 0; index < target.valueStack.length; index += 1) {
    const joined = joinEventStaticValue(target.valueStack[index], incoming.valueStack[index]);
    if (joined !== target.valueStack[index]) {
      target.valueStack[index] = joined;
      changed = true;
    }
  }
  for (const key of Array.from(target.properties.keys())) {
    if (!incoming.properties.has(key) ||
        incoming.properties.get(key) !== target.properties.get(key)) {
      target.properties.delete(key);
      changed = true;
    }
  }
  for (const key of Array.from(target.translations.keys())) {
    if (!incoming.translations.has(key) ||
        incoming.translations.get(key) !== target.translations.get(key)) {
      target.translations.delete(key);
      changed = true;
    }
  }
  for (const key of Array.from(target.substitutionSourceA.keys())) {
    if (!incoming.substitutionSourceA.has(key) ||
        incoming.substitutionSourceA.get(key) !== target.substitutionSourceA.get(key)) {
      target.substitutionSourceA.delete(key);
      changed = true;
    }
  }
  for (const key of Array.from(target.substitutionSourceB.keys())) {
    if (!incoming.substitutionSourceB.has(key) ||
        incoming.substitutionSourceB.get(key) !== target.substitutionSourceB.get(key)) {
      target.substitutionSourceB.delete(key);
      changed = true;
    }
  }
  const precedingDirectorSelector = joinEventStaticValue(
    target.precedingDirectorSelector, incoming.precedingDirectorSelector);
  if (precedingDirectorSelector !== target.precedingDirectorSelector) {
    target.precedingDirectorSelector = precedingDirectorSelector;
    changed = true;
  }
  [
    'precedingDirectorLaunchOffset',
    'precedingDirectorInvocationCursor',
    'updatesSincePrecedingDirectorRequest'
  ].forEach((field) => {
    const joined = joinEventStaticValue(target[field], incoming[field]);
    if (joined !== target[field]) {
      target[field] = joined;
      changed = true;
    }
  });
  if (JSON.stringify(target.precedingExternalRequest) !==
      JSON.stringify(incoming.precedingExternalRequest) &&
      target.precedingExternalRequest !== null) {
    target.precedingExternalRequest = null;
    changed = true;
  }
  return changed;
}

function eventConditionalBranchResult(opcode, registers) {
  const family = opcode & 0xF0;
  const low = opcode & 0x0F;
  const pair = [1, 2, 3, 6, 7, 11].includes(low);
  const single = low >= 12;
  if (!pair && !single) return undefined;
  if (pair) {
    const left = registers[opcode & 3];
    const right = registers[(opcode & 0x0C) >> 2];
    if (left === null || right === null) return null;
    if (family === 0xA0) return left === right;
    if (family === 0xB0) return left !== right;
    if (family === 0xC0) return eventSigned16(left) < eventSigned16(right);
    if (family === 0xD0) return eventSigned16(left) > eventSigned16(right);
    if (family === 0xE0) return eventSigned16(left) <= eventSigned16(right);
    if (family === 0xF0) return eventSigned16(left) >= eventSigned16(right);
  }
  if (single) {
    const value = registers[opcode & 3];
    if (value === null) return null;
    if (family === 0xA0) return value === 0;
    if (family === 0xB0) return value !== 0;
    if (family === 0xC0) return eventSigned16(value) > 0;
    if (family === 0xD0) return eventSigned16(value) < 0;
    if (family === 0xE0) return eventSigned16(value) >= 0;
    if (family === 0xF0) return eventSigned16(value) <= 0;
  }
  return undefined;
}

function eventInvocationContext(state, launchOffset) {
  const eventPropertyE6 = state.properties.has(0xE6)
    ? state.properties.get(0xE6) : null;
  const eventPropertyE9 = state.properties.has(0xE9)
    ? state.properties.get(0xE9) : null;
  const eventPropertyFB = state.properties.has(0xFB)
    ? state.properties.get(0xFB) : null;
  const eventPropertyFC = state.properties.has(0xFC)
    ? state.properties.get(0xFC) : null;
  const eventPropertyFD = state.properties.has(0xFD)
    ? state.properties.get(0xFD) : null;
  return {
    eventInvocationCursor: state.invocationCursor,
    eventInvocationOffset: launchOffset - state.invocationCursor,
    precedingDirectorLaunchCount: state.precedingDirectorLaunchCount,
    precedingDirectorSelector: state.precedingDirectorSelector,
    precedingDirectorLaunchOffset: state.precedingDirectorLaunchOffset,
    precedingDirectorInvocationCursor: state.precedingDirectorInvocationCursor,
    concurrentDirectorTickOffset: state.updatesSincePrecedingDirectorRequest,
    launchTranslationTable: Array.from({
      length: EVENT_TRANSLATION_TABLE_TRACKED_ENTRIES
    }, (_, index) => state.translations.has(index)
      ? state.translations.get(index) : null),
    eventPropertyValues: Array.from(state.properties.entries())
      .sort((left, right) => left[0] - right[0])
      .map(([propertyOperand, value]) => ({ propertyOperand, value })),
    launchFlagBit08: false,
    eventPropertyE6,
    eventPropertyE9,
    eventPropertyFB,
    eventPropertyFC,
    eventPropertyFD,
    scenarioKey: eventPropertyE9,
    battleTerrain: eventPropertyFC,
    currentUnitSelector: eventPropertyFD,
    launchPreservationSnapshot: eventPropertyE6 === null
      ? null : eventPropertyE6 !== 0,
    secondRosterUnitLeaderOnly: eventPropertyE6 === null
      ? null : (eventPropertyE6 & 0x8000) !== 0,
    precedingExternalRequest: state.precedingExternalRequest === null
      ? null : {
        ...state.precedingExternalRequest,
        stateWrites: state.precedingExternalRequest.stateWrites.map((write) => ({ ...write }))
      },
    evidenceStatus: 'native-static-event-vm'
  };
}

function analyzeEventSequenceLaunches(decoded, entryCursor, targetOffsets, eventRow) {
  const initial = {
    pc: entryCursor,
    invocationCursor: entryCursor,
    registers: Array(8).fill(null),
    callStack: [],
    valueStack: [],
    properties: new Map([[0xE8, eventRow]]),
    translations: initialEventTranslationTable(),
    substitutionSourceA: new Map(),
    substitutionSourceB: new Map(),
    precedingDirectorSelector: null,
    precedingDirectorLaunchOffset: null,
    precedingDirectorInvocationCursor: null,
    updatesSincePrecedingDirectorRequest: null,
    precedingDirectorLaunchCount: 0,
    precedingExternalRequest: null
  };
  const states = new Map();
  const queue = [];
  const hits = new Map();
  const externalRequestSites = new Map();
  const externalRequestHandoffs = new Map();
  const translationWrites = new Map();
  const substitutionSourceWrites = new Map();
  let unknownLongJumps = 0;
  function enqueue(state) {
    if (state.pc < 0 || state.pc + 1 >= decoded.length || (state.pc & 1) !== 0 ||
        state.callStack.length > 16 || state.valueStack.length > 32 ||
        state.precedingDirectorLaunchCount > 64) return;
    const key = eventStaticStateKey(state);
    const prior = states.get(key);
    if (!prior) {
      states.set(key, state);
      queue.push(state);
    } else if (mergeEventStaticState(prior, state)) {
      queue.push(prior);
    }
  }
  enqueue(initial);
  let steps = 0;
  while (queue.length && steps < 500000) {
    steps += 1;
    const state = queue.shift();
    const opcode = decoded[state.pc];
    const operand = decoded[state.pc + 1];
    const nextPc = state.pc + 2;
    if (opcode === 0x10) {
      if (targetOffsets.has(state.pc)) {
        if (!hits.has(state.pc)) hits.set(state.pc, new Map());
        const hitKey = state.invocationCursor + ':' +
          state.precedingDirectorLaunchCount;
        const prior = hits.get(state.pc).get(hitKey);
        if (!prior) hits.get(state.pc).set(hitKey, cloneEventStaticState(state));
        else mergeEventStaticState(prior, state);
      }
      const resumed = cloneEventStaticState(state);
      resumed.pc = nextPc;
      resumed.invocationCursor = nextPc;
      resumed.registers.fill(null);
      resumed.callStack = [];
      resumed.valueStack = [];
      resumed.translations = initialEventTranslationTable();
      resumed.precedingDirectorSelector = state.registers[0] === null
        ? null : eventUnsigned16(state.registers[0]);
      resumed.precedingDirectorLaunchOffset = state.pc;
      resumed.precedingDirectorInvocationCursor = state.invocationCursor;
      resumed.updatesSincePrecedingDirectorRequest = 1;
      resumed.precedingDirectorLaunchCount += 1;
      resumed.precedingExternalRequest = null;
      enqueue(resumed);
      continue;
    }
    if (opcode === 0x11 || opcode === 0x12 || opcode === 0x2F) continue;
    if (opcode === 0x13) {
      const request = eventExternalRequestProfile(operand);
      externalRequestSites.set(state.pc, request);
      externalRequestHandoffs.set([
        state.pc,
        state.invocationCursor,
        state.precedingDirectorLaunchCount
      ].join(':'), {
        decodedByteOffset: state.pc,
        eventInvocationCursor: state.invocationCursor,
        eventInvocationOffset: state.pc - state.invocationCursor,
        precedingDirectorLaunchCount: state.precedingDirectorLaunchCount,
        precedingDirectorSelector: state.precedingDirectorSelector,
        ...request,
        stateWrites: request.stateWrites.map((write) => ({ ...write }))
      });
      const resumed = cloneEventStaticState(state);
      resumed.pc = nextPc;
      resumed.invocationCursor = nextPc;
      resumed.registers.fill(null);
      resumed.callStack = [];
      resumed.valueStack = [];
      resumed.translations = initialEventTranslationTable();
      if (resumed.updatesSincePrecedingDirectorRequest !== null) {
        resumed.updatesSincePrecedingDirectorRequest += 1;
      }
      resumed.precedingExternalRequest = {
        decodedByteOffset: state.pc,
        ...request,
        stateWrites: request.stateWrites.map((write) => ({ ...write }))
      };
      enqueue(resumed);
      continue;
    }
    if (opcode === 0x01) {
      const next = cloneEventStaticState(state);
      next.pc = eventRelativeByteCursor(state.pc, eventSigned8(operand));
      enqueue(next);
      continue;
    }
    if (opcode === 0x02 || opcode === 0x03) {
      const low = state.registers[0] === null ? null : state.registers[0] & 0xFF;
      if (low === null) {
        unknownLongJumps += 1;
        continue;
      }
      const next = cloneEventStaticState(state);
      if (opcode === 0x03) next.callStack.push(state.pc);
      next.pc = eventRelativeByteCursor(state.pc, (operand << 8) | low);
      enqueue(next);
      continue;
    }
    if (opcode === 0x04) {
      if (!state.callStack.length) continue;
      const next = cloneEventStaticState(state);
      const returnCursor = next.callStack.pop();
      if (returnCursor !== null) {
        next.pc = eventRelativeByteCursor(returnCursor, 0);
        enqueue(next);
      }
      continue;
    }
    if (opcode === 0x05) {
      const next = cloneEventStaticState(state);
      next.valueStack.push(next.registers[operand & 7]);
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode === 0x06) {
      if (!state.valueStack.length) continue;
      const next = cloneEventStaticState(state);
      next.registers[operand & 7] = next.valueStack.pop();
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x60 && opcode <= 0x67) {
      const next = cloneEventStaticState(state);
      let value = null;
      if (operand >= 0xC0 && operand <= 0xC7) {
        const sourceIndex = next.registers[operand & 7];
        if (sourceIndex !== null &&
            next.substitutionSourceA.has(sourceIndex & 0xFFFF)) {
          value = next.substitutionSourceA.get(sourceIndex & 0xFFFF);
        }
      } else if (operand >= 0xC8 && operand <= 0xCF) {
        const sourceIndex = next.registers[operand & 7];
        if (sourceIndex !== null &&
            next.substitutionSourceB.has(sourceIndex & 0xFFFF)) {
          value = next.substitutionSourceB.get(sourceIndex & 0xFFFF);
        }
      } else if (EVENT_SPECIAL_GETTER_OPERANDS.has(operand) &&
          next.properties.has(operand)) {
        value = next.properties.get(operand);
      }
      if (operand === 0xF8 && next.properties.has(0xFD)) {
        value = eventUnsigned16(next.properties.get(0xFD) + 1);
      }
      next.registers[opcode & 7] = value;
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x68 && opcode <= 0x6F) {
      const next = cloneEventStaticState(state);
      next.registers[opcode & 7] = operand;
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x70 && opcode <= 0x77) {
      const next = cloneEventStaticState(state);
      const value = next.registers[opcode & 7];
      if (EVENT_SPECIAL_PROPERTY_WIDTHS.has(operand)) {
        if (value === null) next.properties.delete(operand);
        else next.properties.set(operand,
          EVENT_SPECIAL_PROPERTY_WIDTHS.get(operand) === 8
            ? value & 0xFF : value & 0xFFFF);
      } else if (operand >= 0xC0 && operand <= 0xCF) {
        const sourceId = operand < 0xC8 ? 'A' : 'B';
        const source = sourceId === 'A'
          ? next.substitutionSourceA : next.substitutionSourceB;
        const sourceIndex = next.registers[operand & 7];
        substitutionSourceWrites.set([
          state.pc,
          state.invocationCursor,
          state.precedingDirectorLaunchCount
        ].join(':'), {
          decodedByteOffset: state.pc,
          eventInvocationCursor: state.invocationCursor,
          eventInvocationOffset: state.pc - state.invocationCursor,
          precedingDirectorLaunchCount: state.precedingDirectorLaunchCount,
          opcode,
          operand,
          sourceRegister: opcode & 7,
          indexRegister: operand & 7,
          sourceId,
          sourceSemantic: sourceId === 'A'
            ? 'primary-class-id' : 'secondary-class-id',
          characterRecordFieldOffset: sourceId === 'A' ? 0x11 : 0x12,
          characterRecordStride: 56,
          sourceIndex,
          value,
          sourceValueOrigin: value === null
            ? 'runtime-character-record-or-branch-dependent'
            : 'event-program-constant',
          resolutionStatus: sourceIndex === null
            ? 'source-index-unresolved'
            : (value === null ? 'source-value-unresolved' : 'exact'),
          evidenceStatus: 'native-static-event-vm'
        });
        if (sourceIndex === null) {
          source.clear();
        } else if (sourceIndex >= 0 && sourceIndex < 5) {
          if (value === null) source.delete(sourceIndex);
          else source.set(sourceIndex, value & 0xFF);
        }
      } else if (operand >= 0xD0 && operand <= 0xD7) {
        const tableIndex = next.registers[operand & 7];
        translationWrites.set([
          state.pc,
          state.invocationCursor,
          state.precedingDirectorLaunchCount
        ].join(':'), {
          decodedByteOffset: state.pc,
          eventInvocationCursor: state.invocationCursor,
          eventInvocationOffset: state.pc - state.invocationCursor,
          precedingDirectorLaunchCount: state.precedingDirectorLaunchCount,
          opcode,
          operand,
          sourceRegister: opcode & 7,
          indexRegister: operand & 7,
          tableIndex,
          value,
          resolutionStatus: tableIndex === null
            ? 'table-index-unresolved'
            : (value === null ? 'replacement-value-unresolved' : 'exact'),
          evidenceStatus: 'native-static-event-vm'
        });
        if (tableIndex !== null && tableIndex >= 0 && tableIndex < 256) {
          if (value === null) next.translations.delete(tableIndex);
          else next.translations.set(tableIndex, value & 0xFFFF);
        } else {
          next.translations.clear();
        }
      }
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x78 && opcode <= 0x7F) {
      const next = cloneEventStaticState(state);
      next.registers[opcode & 7] = null;
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x80 && opcode <= 0x87) {
      const next = cloneEventStaticState(state);
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x88 && opcode <= 0x8F) {
      const next = cloneEventStaticState(state);
      const register = opcode & 7;
      next.registers[register] = next.registers[register] === null ? null :
        (operand << 8) | (next.registers[register] & 0xFF);
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x90 && opcode <= 0x97) {
      const next = cloneEventStaticState(state);
      const destination = opcode & 7;
      const source = operand & 7;
      const operation = operand >> 3;
      const left = next.registers[destination];
      const right = next.registers[source];
      let value = null;
      if (left !== null && right !== null && operation < 7 &&
          !(operation >= 5 && right === 0)) {
        if (operation === 0) value = left & right;
        if (operation === 1) value = left | right;
        if (operation === 2) value = eventUnsigned16(left + right);
        if (operation === 3) value = eventUnsigned16(left - right);
        if (operation === 4) value = eventUnsigned16(left * right);
        if (operation === 5) value = eventUnsigned16(Math.floor(left / right));
        if (operation === 6) value = eventUnsigned16(left % right);
      }
      next.registers[destination] = value;
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0x98 && opcode <= 0x9F) {
      const next = cloneEventStaticState(state);
      const destination = opcode & 7;
      const immediate = operand & 0x1F;
      const operation = operand >> 5;
      const left = next.registers[destination];
      let value = null;
      if (left !== null && operation < 7 && !(operation >= 5 && immediate === 0)) {
        if (operation === 0) value = left & immediate;
        if (operation === 1) value = left | immediate;
        if (operation === 2) value = eventUnsigned16(left + immediate);
        if (operation === 3) value = eventUnsigned16(left - immediate);
        if (operation === 4) value = eventUnsigned16(left * immediate);
        if (operation === 5) {
          value = eventUnsigned16(Math.trunc(eventSigned16(left) / immediate));
        }
        if (operation === 6) value = eventUnsigned16(eventSigned16(left) % immediate);
      }
      next.registers[destination] = value;
      next.pc = nextPc;
      enqueue(next);
      continue;
    }
    if (opcode >= 0xA1) {
      const condition = eventConditionalBranchResult(opcode, state.registers);
      if (condition === undefined) {
        const next = cloneEventStaticState(state);
        next.pc = nextPc;
        enqueue(next);
      } else {
        if (condition !== false) {
          const taken = cloneEventStaticState(state);
          taken.pc = eventRelativeByteCursor(state.pc, eventSigned8(operand));
          enqueue(taken);
        }
        if (condition !== true) {
          const untaken = cloneEventStaticState(state);
          untaken.pc = nextPc;
          enqueue(untaken);
        }
      }
      continue;
    }
    const next = cloneEventStaticState(state);
    if (opcode === 0x1B) {
      next.registers[7] = next.registers[7] === null ? null :
        eventUnsigned16(next.registers[7] + 1);
    }
    if (opcode === 0x28) {
      next.registers[6] = null;
      next.registers[7] = null;
    }
    if (opcode === 0x2D) {
      const taken = cloneEventStaticState(next);
      taken.pc = eventRelativeByteCursor(state.pc, eventSigned8(operand));
      enqueue(taken);
    }
    next.pc = nextPc;
    enqueue(next);
  }
  const contextsByOffset = new Map();
  hits.forEach((statesAtOffset, launchOffset) => {
    contextsByOffset.set(launchOffset, Array.from(statesAtOffset.values())
      .map((state) => eventInvocationContext(state, launchOffset))
      .sort((left, right) =>
        left.eventInvocationCursor - right.eventInvocationCursor ||
        left.precedingDirectorLaunchCount - right.precedingDirectorLaunchCount));
  });
  return {
    contextsByOffset,
    externalRequestSites: Array.from(externalRequestSites.entries())
      .map(([decodedByteOffset, request]) => ({
        decodedByteOffset,
        ...request,
        stateWrites: request.stateWrites.map((write) => ({ ...write }))
      }))
      .sort((left, right) => left.decodedByteOffset - right.decodedByteOffset),
    externalRequestHandoffs: Array.from(externalRequestHandoffs.values())
      .sort((left, right) =>
        left.decodedByteOffset - right.decodedByteOffset ||
        left.eventInvocationCursor - right.eventInvocationCursor ||
        left.precedingDirectorLaunchCount - right.precedingDirectorLaunchCount),
    translationWrites: Array.from(translationWrites.values())
      .sort((left, right) =>
        left.decodedByteOffset - right.decodedByteOffset ||
        left.eventInvocationCursor - right.eventInvocationCursor ||
        left.precedingDirectorLaunchCount - right.precedingDirectorLaunchCount),
    substitutionSourceWrites: Array.from(substitutionSourceWrites.values())
      .sort((left, right) =>
        left.decodedByteOffset - right.decodedByteOffset ||
        left.eventInvocationCursor - right.eventInvocationCursor ||
        left.precedingDirectorLaunchCount - right.precedingDirectorLaunchCount),
    unknownLongJumps,
    capped: steps >= 500000
  };
}

function readEventDirectorLaunchInventory(z64, directorSelectorTable) {
  const prefixStart = DIRECTOR_RESOURCE_BASE_Z64 + EVENT_DIRECTORY_KEY;
  const directoryLength = readU32(z64, prefixStart, 'Parent event directory prefix');
  if (directoryLength !== EVENT_DIRECTORY_ROWS * 4) {
    throw new Error('The parent event directory no longer contains 115 rows.');
  }
  const directory = z64.subarray(prefixStart + 4, prefixStart + 4 + directoryLength);
  const launches = [];
  let populatedRows = 0;
  let outerEntryCount = 0;
  let distinctOuterCursorCount = 0;
  let sequenceTableCount = 0;
  let sequenceEntryCount = 0;
  let directOuterSequenceCount = 0;
  let distinctSequenceCursorCount = 0;
  let unknownLongJumps = 0;
  let cappedSequenceAnalyses = 0;
  const externalRequestPhysicalSites = new Set();
  const externalRequests = [];
  const translationWrites = [];
  const substitutionSourceWrites = [];
  for (let eventRow = 0; eventRow < EVENT_DIRECTORY_ROWS; eventRow += 1) {
    const eventResourceKey = readU32(directory, eventRow * 4, 'Parent event directory row');
    if (!eventResourceKey) continue;
    populatedRows += 1;
    const eventPrefix = DIRECTOR_RESOURCE_BASE_Z64 + eventResourceKey;
    const storedLength = readU32(z64, eventPrefix, 'Parent event resource prefix');
    const payload = z64.subarray(eventPrefix + 4, eventPrefix + 4 + storedLength);
    const decoded = decodeCustomLz(payload,
      'Parent event resource 0x' + hex(eventResourceKey, 8)).bytes;
    const sequenceInventory = readEventSequenceInventory(decoded, eventRow,
      'Parent event resource 0x' + hex(eventResourceKey, 8));
    outerEntryCount += sequenceInventory.outerEntries.length;
    distinctOuterCursorCount += sequenceInventory.distinctOuterCursorCount;
    sequenceTableCount += sequenceInventory.sequenceTables.length;
    sequenceEntryCount += sequenceInventory.sequenceEntryCount;
    directOuterSequenceCount += sequenceInventory.directOuterSequenceCount;
    distinctSequenceCursorCount += sequenceInventory.sequences.length;
    const rowLaunches = [];
    for (let offset = 4; offset + 1 < decoded.length; offset += 2) {
      if (decoded[offset] !== 0x10 || decoded[offset + 1] !== 0x00) continue;
      let selector = null;
      if (decoded[offset - 4] === 0x68 && decoded[offset - 2] === 0x88) {
        selector = decoded[offset - 3] | decoded[offset - 1] << 8;
      } else if (decoded[offset - 2] === 0x68) {
        selector = decoded[offset - 1];
      }
      if (!Number.isInteger(selector) || selector < 0 || selector >= DIRECTOR_SELECTOR_ROWS) {
        throw new Error('Parent event row ' + eventRow + ' has an unrecognized direct ' +
          'Director launch at decoded byte 0x' + hex(offset, 4) + '.');
      }
      const directorKey = readU32(directorSelectorTable, selector * 4,
        'Parent event Director selector');
      if (!directorKey) {
        throw new Error('Parent event row ' + eventRow +
          ' launches an empty Director selector ' + selector + '.');
      }
      const entryOwner = eventLaunchEntryOwner(sequenceInventory, offset,
        'Parent event row ' + eventRow + ' launch');
      rowLaunches.push({
        launchId: 'event-director:' + eventRow + ':b' + hex(offset, 4),
        eventDirectoryRow: eventRow,
        eventResourceKey: '0x' + hex(eventResourceKey, 8),
        decodedByteOffset: offset,
        eventEntryCursor: entryOwner.entryCursor,
        eventEntryOffset: entryOwner.entryOffset,
        eventEntryPaths: entryOwner.entryPaths,
        directorSelector: selector,
        directorResourceKey: '0x' + hex(directorKey, 8),
        evidenceStatus: 'direct-static-event-launch'
      });
    }
    sequenceInventory.sequences.forEach((sequence) => {
      const ownedLaunches = rowLaunches.filter((launch) =>
        launch.eventEntryCursor === sequence.cursor);
      const analysis = analyzeEventSequenceLaunches(decoded, sequence.cursor,
        new Set(ownedLaunches.map((launch) => launch.decodedByteOffset)), eventRow);
      unknownLongJumps += analysis.unknownLongJumps;
      if (analysis.capped) cappedSequenceAnalyses += 1;
      analysis.externalRequestSites.forEach((request) => {
        externalRequestPhysicalSites.add(eventRow + ':' + request.decodedByteOffset);
      });
      analysis.externalRequestHandoffs.forEach((request) => {
        externalRequests.push({
          eventDirectoryRow: eventRow,
          eventResourceKey: '0x' + hex(eventResourceKey, 8),
          eventEntryCursor: sequence.cursor,
          ...request,
          stateWrites: request.stateWrites.map((write) => ({ ...write }))
        });
      });
      analysis.translationWrites.forEach((write) => {
        translationWrites.push({
          eventDirectoryRow: eventRow,
          eventResourceKey: '0x' + hex(eventResourceKey, 8),
          eventEntryCursor: sequence.cursor,
          ...write
        });
      });
      analysis.substitutionSourceWrites.forEach((write) => {
        substitutionSourceWrites.push({
          eventDirectoryRow: eventRow,
          eventResourceKey: '0x' + hex(eventResourceKey, 8),
          eventEntryCursor: sequence.cursor,
          ...write
        });
      });
      if (!ownedLaunches.length) return;
      ownedLaunches.forEach((launch) => {
        const contexts = analysis.contextsByOffset.get(launch.decodedByteOffset) || [];
        if (!contexts.length) {
          throw new Error(launch.launchId +
            ' is not reachable from its native event sequence cursor.');
        }
        contexts.forEach((context) => {
          if (context.precedingDirectorSelector === null) {
            context.precedingDirectorResourceKey = null;
            context.precedingDirectorLaunchId = null;
            return;
          }
          if (context.precedingDirectorSelector < 0 ||
              context.precedingDirectorSelector >= DIRECTOR_SELECTOR_ROWS) {
            throw new Error(launch.launchId + ' has an out-of-range preceding Director ' +
              'selector ' + context.precedingDirectorSelector + '.');
          }
          const precedingKey = readU32(directorSelectorTable,
            context.precedingDirectorSelector * 4,
            'Preceding parent-event Director selector');
          context.precedingDirectorResourceKey = precedingKey
            ? '0x' + hex(precedingKey, 8) : null;
          const precedingLaunch = Number.isInteger(context.precedingDirectorLaunchOffset)
            ? rowLaunches.find((candidate) =>
              candidate.decodedByteOffset === context.precedingDirectorLaunchOffset &&
              candidate.directorSelector === context.precedingDirectorSelector)
            : null;
          context.precedingDirectorLaunchId = precedingLaunch
            ? precedingLaunch.launchId : null;
        });
        launch.eventInvocationContexts = contexts;
      });
    });
    rowLaunches.forEach((launch) => {
      if (!Array.isArray(launch.eventInvocationContexts) ||
          launch.eventInvocationContexts.length === 0) {
        throw new Error(launch.launchId + ' has no event invocation context.');
      }
    });
    launches.push(...rowLaunches);
  }
  const selectors = new Set(launches.map((row) => row.directorSelector));
  const resources = new Set(launches.map((row) => row.directorResourceKey));
  const launchEntryCursorCount = new Set(launches.map((launch) =>
    launch.eventDirectoryRow + ':' + launch.eventEntryCursor)).size;
  const invocationContextCount = launches.reduce((total, launch) =>
    total + launch.eventInvocationContexts.length, 0);
  const multiInvocationLaunchCount = launches.filter((launch) =>
    launch.eventInvocationContexts.length > 1).length;
  const distinctInvocationCursorCount = new Set(launches.flatMap((launch) =>
    launch.eventInvocationContexts.map((context) =>
      launch.eventDirectoryRow + ':' + context.eventInvocationCursor))).size;
  const translationPhysicalSiteCount = new Set(translationWrites.map((write) =>
    write.eventDirectoryRow + ':' + write.decodedByteOffset)).size;
  const exactTranslationWrites = translationWrites.filter((write) =>
    write.resolutionStatus === 'exact');
  const unresolvedTranslationWrites = translationWrites.filter((write) =>
    write.resolutionStatus === 'replacement-value-unresolved');
  const retailTranslationWrites = translationWrites.filter((write) =>
    Number.isInteger(write.tableIndex) &&
    write.tableIndex >= 0 && write.tableIndex < EVENT_TRANSLATION_TABLE_TRACKED_ENTRIES);
  const retailTranslationPhysicalSiteCount = new Set(retailTranslationWrites.map((write) =>
    write.eventDirectoryRow + ':' + write.decodedByteOffset)).size;
  const exactRetailTranslationWrites = retailTranslationWrites.filter((write) =>
    write.resolutionStatus === 'exact');
  const unresolvedRetailTranslationWrites = retailTranslationWrites.filter((write) =>
    write.resolutionStatus === 'replacement-value-unresolved');
  const nonretailTranslationWrites = translationWrites.filter((write) =>
    write.tableIndex === 0xFF);
  const nonretailTranslationPhysicalSiteCount = new Set(
    nonretailTranslationWrites.map((write) =>
      write.eventDirectoryRow + ':' + write.decodedByteOffset)).size;
  const substitutionSourcePhysicalSiteCount = new Set(substitutionSourceWrites.map((write) =>
    write.eventDirectoryRow + ':' + write.decodedByteOffset)).size;
  const substitutionSourceAWrites = substitutionSourceWrites.filter((write) =>
    write.sourceId === 'A');
  const substitutionSourceBWrites = substitutionSourceWrites.filter((write) =>
    write.sourceId === 'B');
  const exactSubstitutionSourceIndexWrites = substitutionSourceWrites.filter((write) =>
    write.sourceIndex !== null);
  const unresolvedSubstitutionSourceIndexWrites = substitutionSourceWrites.filter((write) =>
    write.sourceIndex === null);
  const exactSubstitutionSourceValueWrites = substitutionSourceWrites.filter((write) =>
    write.value !== null);
  const unresolvedSubstitutionSourceValueWrites = substitutionSourceWrites.filter((write) =>
    write.value === null);
  if (populatedRows !== EVENT_DIRECTORY_POPULATED_ROWS ||
      launches.length !== DIRECT_EVENT_DIRECTOR_LAUNCHES ||
      selectors.size !== DIRECT_EVENT_DIRECTOR_SELECTORS ||
      resources.size !== DIRECT_EVENT_DIRECTOR_RESOURCES ||
      outerEntryCount !== EVENT_OUTER_ENTRY_ROWS ||
      distinctOuterCursorCount !== EVENT_DISTINCT_OUTER_CURSORS ||
      sequenceTableCount !== EVENT_SEQUENCE_TABLES ||
      sequenceEntryCount !== EVENT_SEQUENCE_ENTRY_ROWS ||
      directOuterSequenceCount !== EVENT_DIRECT_OUTER_SEQUENCES ||
      distinctSequenceCursorCount !== EVENT_DISTINCT_SEQUENCE_CURSORS ||
      launchEntryCursorCount !== EVENT_LAUNCH_SEQUENCE_CURSORS ||
      invocationContextCount !== EVENT_STATIC_INVOCATION_CONTEXTS ||
      multiInvocationLaunchCount !== EVENT_MULTI_INVOCATION_LAUNCHES ||
      distinctInvocationCursorCount !== EVENT_DISTINCT_INVOCATION_CURSORS ||
      externalRequestPhysicalSites.size !== EVENT_EXTERNAL_REQUEST_PHYSICAL_SITES ||
      externalRequests.length !== EVENT_EXTERNAL_REQUEST_HANDOFFS ||
      translationPhysicalSiteCount !== EVENT_TRANSLATION_PHYSICAL_SITES ||
      translationWrites.length !== EVENT_TRANSLATION_WRITE_CONTEXTS ||
      exactTranslationWrites.length !== EVENT_TRANSLATION_EXACT_CONTEXTS ||
      unresolvedTranslationWrites.length !== EVENT_TRANSLATION_UNRESOLVED_CONTEXTS ||
      substitutionSourcePhysicalSiteCount !== EVENT_SUBSTITUTION_SOURCE_PHYSICAL_SITES ||
      substitutionSourceWrites.length !== EVENT_SUBSTITUTION_SOURCE_WRITE_CONTEXTS ||
      substitutionSourceAWrites.length !== EVENT_SUBSTITUTION_SOURCE_A_WRITE_CONTEXTS ||
      substitutionSourceBWrites.length !== EVENT_SUBSTITUTION_SOURCE_B_WRITE_CONTEXTS ||
      exactSubstitutionSourceIndexWrites.length !==
        EVENT_SUBSTITUTION_SOURCE_EXACT_INDEX_CONTEXTS ||
      unresolvedSubstitutionSourceIndexWrites.length !==
        EVENT_SUBSTITUTION_SOURCE_UNRESOLVED_INDEX_CONTEXTS ||
      exactSubstitutionSourceValueWrites.length !==
        EVENT_SUBSTITUTION_SOURCE_EXACT_VALUE_CONTEXTS ||
      unresolvedSubstitutionSourceValueWrites.length !==
        EVENT_SUBSTITUTION_SOURCE_UNRESOLVED_VALUE_CONTEXTS ||
      retailTranslationPhysicalSiteCount !== EVENT_RETAIL_TRANSLATION_PHYSICAL_SITES ||
      retailTranslationWrites.length !== EVENT_RETAIL_TRANSLATION_WRITE_CONTEXTS ||
      exactRetailTranslationWrites.length !== EVENT_RETAIL_TRANSLATION_EXACT_CONTEXTS ||
      unresolvedRetailTranslationWrites.length !==
        EVENT_RETAIL_TRANSLATION_UNRESOLVED_CONTEXTS ||
      nonretailTranslationPhysicalSiteCount !==
        EVENT_NONRETAIL_TRANSLATION_PHYSICAL_SITES ||
      nonretailTranslationWrites.length !== EVENT_NONRETAIL_TRANSLATION_WRITE_CONTEXTS ||
      unknownLongJumps !== 0 || cappedSequenceAnalyses !== 0) {
    throw new Error('The direct parent-event Director launch inventory changed: rows=' +
      populatedRows + ', launches=' + launches.length + ', selectors=' + selectors.size +
      ', resources=' + resources.size + ', outer=' + outerEntryCount +
      ', tables=' + sequenceTableCount + ', sequences=' +
      distinctSequenceCursorCount + ', launchEntries=' + launchEntryCursorCount +
      ', contexts=' + invocationContextCount + ', multiContexts=' +
      multiInvocationLaunchCount + ', invocationCursors=' +
      distinctInvocationCursorCount + ', requestSites=' +
      externalRequestPhysicalSites.size + ', requestHandoffs=' +
      externalRequests.length + ', translationSites=' + translationPhysicalSiteCount +
      ', translationWrites=' + translationWrites.length +
      ', sourceSites=' + substitutionSourcePhysicalSiteCount +
      ', sourceWrites=' + substitutionSourceWrites.length +
      ', sourceA=' + substitutionSourceAWrites.length +
      ', sourceB=' + substitutionSourceBWrites.length +
      ', sourceExactIndexes=' + exactSubstitutionSourceIndexWrites.length +
      ', sourceUnresolvedIndexes=' + unresolvedSubstitutionSourceIndexWrites.length +
      ', sourceExactValues=' + substitutionSourceWrites.filter((write) =>
        write.value !== null).length +
      ', retailTranslationSites=' + retailTranslationPhysicalSiteCount +
      ', retailTranslationWrites=' + retailTranslationWrites.length +
      ', unknownLongJumps=' + unknownLongJumps +
      ', capped=' + cappedSequenceAnalyses + '.');
  }
  const bySelector = new Map();
  launches.forEach((launch) => {
    if (!bySelector.has(launch.directorSelector)) bySelector.set(launch.directorSelector, []);
    bySelector.get(launch.directorSelector).push(launch);
  });
  return {
    launches,
    bySelector,
    populatedRows,
    outerEntryCount,
    distinctOuterCursorCount,
    sequenceTableCount,
    sequenceEntryCount,
    directOuterSequenceCount,
    distinctSequenceCursorCount,
    launchEntryCursorCount,
    invocationContextCount,
    multiInvocationLaunchCount,
    distinctInvocationCursorCount,
    externalRequests,
    externalRequestPhysicalSiteCount: externalRequestPhysicalSites.size,
    externalRequestHandoffCount: externalRequests.length,
    translationWrites,
    substitutionSourceWrites,
    translationPhysicalSiteCount,
    substitutionSourcePhysicalSiteCount,
    substitutionSourceAWriteCount: substitutionSourceAWrites.length,
    substitutionSourceBWriteCount: substitutionSourceBWrites.length,
    exactSubstitutionSourceIndexWriteCount: exactSubstitutionSourceIndexWrites.length,
    unresolvedSubstitutionSourceIndexWriteCount:
      unresolvedSubstitutionSourceIndexWrites.length,
    exactSubstitutionSourceValueWriteCount: exactSubstitutionSourceValueWrites.length,
    unresolvedSubstitutionSourceValueWriteCount:
      unresolvedSubstitutionSourceValueWrites.length,
    exactTranslationWriteCount: exactTranslationWrites.length,
    unresolvedTranslationWriteCount: unresolvedTranslationWrites.length,
    retailTranslationPhysicalSiteCount,
    retailTranslationWriteCount: retailTranslationWrites.length,
    exactRetailTranslationWriteCount: exactRetailTranslationWrites.length,
    unresolvedRetailTranslationWriteCount: unresolvedRetailTranslationWrites.length,
    nonretailTranslationPhysicalSiteCount,
    nonretailTranslationWriteCount: nonretailTranslationWrites.length,
    selectorCount: selectors.size,
    resourceCount: resources.size,
    directorySha256: sha256(directory)
  };
}

function readRetailDirectorInventory(options, manifest, grammar) {
  const raw = fs.readFileSync(options.rom);
  const rawSha256 = sha256(raw);
  if (rawSha256 !== RAW_US_REV0_V64_SHA256) {
    throw new Error('The source V64 hash does not match Ogre Battle 64 US Rev 0.');
  }
  const z64 = normalizeV64(raw);
  const normalizedSha256 = sha256(z64);
  if (normalizedSha256 !== NORMALIZED_US_REV0_Z64_SHA256) {
    throw new Error('The normalized z64 hash does not match Ogre Battle 64 US Rev 0.');
  }
  if (readU32(z64, DIRECTOR_SELECTOR_TABLE_PREFIX_Z64,
      'Director selector-table prefix') !== DIRECTOR_SELECTOR_TABLE_BYTES) {
    throw new Error('The Director selector-table size prefix changed.');
  }
  const table = z64.subarray(DIRECTOR_SELECTOR_TABLE_PAYLOAD_Z64,
    DIRECTOR_SELECTOR_TABLE_PAYLOAD_Z64 + DIRECTOR_SELECTOR_TABLE_BYTES);
  const eventLaunchInventory = readEventDirectorLaunchInventory(z64, table);
  const selectorRowsByKey = new Map();
  for (let row = 0; row < DIRECTOR_SELECTOR_ROWS; row += 1) {
    const key = readU32(table, row * 4, 'Director selector-table row');
    if (!selectorRowsByKey.has(key)) selectorRowsByKey.set(key, []);
    selectorRowsByKey.get(key).push(row);
  }
  const populatedRows = Array.from(selectorRowsByKey.entries())
    .filter(([key]) => key !== 0).reduce((total, entry) => total + entry[1].length, 0);
  const uniqueKeys = Array.from(selectorRowsByKey.keys()).filter((key) => key !== 0);
  if (populatedRows !== RETAIL_DIRECTOR_POPULATED_ROWS ||
      uniqueKeys.length !== RETAIL_DIRECTOR_UNIQUE_RESOURCES) {
    throw new Error('The native Director selector inventory changed.');
  }
  const manifestByKey = new Map(manifest.assets.map((asset) => [
    Number(asset.cutsceneLoadKey) >>> 0, asset
  ]));
  const grammarByOpcode = new Map(grammar.map((definition) => [
    definition.opcodeU32, definition
  ]));
  const opcodeStats = new Map();
  const resources = uniqueKeys.map((key) => {
    const prefixStart = DIRECTOR_RESOURCE_BASE_Z64 + key;
    const storedPayloadLength = readU32(z64, prefixStart,
      'Director resource 0x' + hex(key, 8) + ' prefix');
    const payloadStart = prefixStart + 4;
    const payloadEnd = payloadStart + storedPayloadLength;
    if (storedPayloadLength < 4 || payloadEnd > z64.length) {
      throw new Error('Director resource 0x' + hex(key, 8) +
        ' has an invalid stored envelope.');
    }
    const payload = z64.subarray(payloadStart, payloadEnd);
    const decoded = decodeCustomLz(payload, 'Director resource 0x' + hex(key, 8));
    const decodedWords = wordsFromBytes(decoded.bytes,
      'Director resource 0x' + hex(key, 8));
    const manifestAsset = manifestByKey.get(key) || null;
    const assetId = manifestAsset ? manifestAsset.assetId :
      'rom-director:' + hex(payloadStart, 8);
    const nodes = tileRetailDirector(decodedWords, assetId, grammarByOpcode);
    const selectorRows = selectorRowsByKey.get(key).slice();
    const parentEventLaunches = selectorRows.flatMap((row) =>
      eventLaunchInventory.bySelector.get(row) || []);
    nodes.forEach((node) => {
      if (!opcodeStats.has(node.opcode)) {
        opcodeStats.set(node.opcode, { count: 0, scenes: new Set() });
      }
      const stats = opcodeStats.get(node.opcode);
      stats.count += 1;
      stats.scenes.add(assetId);
    });
    const decodedSha256 = sha256(decoded.bytes);
    if (manifestAsset && (manifestAsset.z64PrefixStart !== prefixStart ||
        manifestAsset.z64PayloadStart !== payloadStart ||
        manifestAsset.storedPayloadLength !== storedPayloadLength ||
        manifestAsset.decodedLength !== decoded.bytes.length ||
        manifestAsset.decodedSha256 !== decodedSha256)) {
      throw new Error(manifestAsset.assetId +
        ' does not match the retail Director inventory.');
    }
    return {
      assetId,
      manifestAsset,
      directorKeyValue: key,
      selectorRows,
      selectorWordZ64: selectorRows.map((row) =>
        DIRECTOR_SELECTOR_TABLE_PAYLOAD_Z64 + row * 4),
      z64PrefixStart: prefixStart,
      z64PayloadStart: payloadStart,
      z64PayloadEndExclusive: payloadEnd,
      rawV64PayloadStart: payloadStart ^ 1,
      storedPayloadLength,
      dmaExtent: (storedPayloadLength + 1) & ~1,
      decodedLength: decoded.bytes.length,
      decodedWordCount: decodedWords.length,
      decodedSha256,
      runtimeNodeCount: nodes.length,
      queryCount: nodes.filter((node) => node.definition.widthKind === 'query').length,
      terminationWordStart: nodes[nodes.length - 1].startWord,
      terminationWordEndExclusive: nodes[nodes.length - 1].endWord,
      launchContext: directorLaunchContextProfile(nodes),
      parentEventLaunches,
      operandTranslation: launchOperandTranslationProfile(nodes, parentEventLaunches),
      nodes
    };
  }).sort((left, right) => left.z64PayloadStart - right.z64PayloadStart);
  const totalWords = resources.reduce((total, resource) =>
    total + resource.decodedWordCount, 0);
  const totalNodes = resources.reduce((total, resource) =>
    total + resource.runtimeNodeCount, 0);
  const selectorExpandedSubstreamCalls = resources.reduce((total, resource) =>
    total + resource.nodes.filter((node) => node.opcode === 0x99).length *
      resource.selectorRows.length, 0);
  const substreamCalls = opcodeStats.get(0x99) || { count: 0 };
  const tailCalls = opcodeStats.get(0x80000003) || { count: 0 };
  const opcode5E = opcodeStats.get(0x5E) || { count: 0 };
  if (totalWords !== RETAIL_DIRECTOR_WORDS ||
      substreamCalls.count !== RETAIL_DIRECTOR_SUBSTREAM_CALLS ||
      selectorExpandedSubstreamCalls !== RETAIL_DIRECTOR_SELECTOR_EXPANDED_SUBSTREAM_CALLS ||
      tailCalls.count !== RETAIL_DIRECTOR_TAIL_CALLS ||
      opcode5E.count !== RETAIL_DIRECTOR_OPCODE_5E_OCCURRENCES) {
    throw new Error('The retail Director grammar scan no longer matches the native table: ' +
      'words=' + totalWords + ', calls=' + substreamCalls.count +
      ', selectorCalls=' + selectorExpandedSubstreamCalls +
      ', tailCalls=' + tailCalls.count + ', opcode5E=' + opcode5E.count + '.');
  }
  return {
    z64,
    resources,
    opcodeStats,
    totalWords,
    totalNodes,
    selectorExpandedSubstreamCalls,
    populatedRows,
    uniqueResources: resources.length,
    tableSha256: sha256(table),
    eventLaunchInventory,
    rawSha256,
    normalizedSha256
  };
}

function compactRetailScene(resource, selectors, derivedEnvironmentRules,
    oversizedImageRules) {
  const rawIdentity = resource.assetId.split(':').pop();
  const profileNodes = sourceNodesForRetailProfile(resource.nodes);
  const profileAsset = { assetId: resource.assetId, nodes: profileNodes };
  const actors = retailActorTemplates(resource.assetId, resource.nodes, selectors);
  const backgroundRequests = backgroundRequestsForAsset(
    profileAsset, null, resource.launchContext);
  const launchProfile = buildLaunchProfile(profileAsset, actors, null, null,
    backgroundRequests, profileNodes, resource.parentEventLaunches,
    resource.operandTranslation, resource.launchContext, derivedEnvironmentRules,
    oversizedImageRules);
  const profiledStageRequests = launchProfile.background.requests.filter((request) =>
    request.stageAssetIds.length > 0);
  const backgroundCandidateAssetIds = Array.from(new Set(backgroundRequests.flatMap((request) =>
    request.nonMode2Route.archiveAssetIds).concat(
      launchProfile.background.requests.flatMap((request) => request.stageAssetIds))));
  return {
    sceneId: 'scene:director:' + rawIdentity.toLowerCase(),
    storageId: resource.assetId,
    assetId: resource.assetId,
    canonicalScene: null,
    technicalName: 'Director resource ' + rawIdentity,
    friendlyName: null,
    aliases: resource.selectorRows.map((row) => 'director-selector-' + row),
    aliasNames: resource.selectorRows.map((row) => 'Director selector ' + row),
    reviewedTimelineOverlay: null,
    engine: 'director',
    sourceRevision: 'us-rev0',
    directorKey: hex(resource.directorKeyValue, 8),
    triggerStatus: 'native-selector-table-static',
    runtimeProof: 'native-selector-table-static',
    parseStatus: 'retail-grammar-complete',
    actorBearing: actors.length > 0,
    actorCount: actors.length,
    actors,
    backgroundAssetIds: [],
    backgroundCandidateAssetIds,
    backgroundAssociationStatus: profiledStageRequests.length
      ? 'Native launch routing selects a renderable environment from the structural Director command.'
      : (backgroundRequests.length
        ? 'The launch context still owns the active background route for this stream.'
        : 'This stream has no background initialization command; launch context owns any scenery.'),
    backgroundRuntimeObservation: null,
    actorCameraObservation: null,
    launchProfile,
    backgroundRequests,
    dialogueAssociations: [],
    audioAssociations: directorAudioAssociations(profileAsset),
    recoveredMediaRequests: [],
    recoveredActorEvents: [],
    recoveredNativeSpriteEffects: [],
    previewCapability: 'preview-only',
    exportCapability: 'needs-research',
    source: {
      masterRomSha256: RAW_US_REV0_V64_SHA256,
      z64PrefixStart: resource.z64PrefixStart,
      z64PrefixEndExclusive: resource.z64PayloadStart,
      z64PayloadStart: resource.z64PayloadStart,
      z64PayloadEndExclusive: resource.z64PayloadEndExclusive,
      rawV64PayloadStart: resource.rawV64PayloadStart,
      storedPayloadLength: resource.storedPayloadLength,
      dmaExtent: resource.dmaExtent,
      decodedLength: resource.decodedLength,
      decodedWordCount: resource.decodedWordCount,
      decodedSha256: resource.decodedSha256,
      directorSelectorTableResourceKey: '0x' + hex(DIRECTOR_SELECTOR_TABLE_KEY, 8),
      directorSelectorTablePayloadZ64: DIRECTOR_SELECTOR_TABLE_PAYLOAD_Z64,
      directorSelectorRows: resource.selectorRows.slice(),
      directorSelectorWordZ64: resource.selectorWordZ64.slice(),
      crcWindowOverlap: false,
      codecVersion: '2026-08-23.retail-director-table-custom-lz-v1',
      corpusNodeCount: 0,
      corpusQueryCount: 0,
      corpusRegisteredWaitCount: 0,
      runtimeNodeCount: resource.runtimeNodeCount,
      runtimeQueryCount: resource.queryCount,
      dynamicGrammar: true,
      terminationWordStart: resource.terminationWordStart,
      terminationWordEndExclusive: resource.terminationWordEndExclusive,
      nodes: [],
      registeredWaits: [],
      gaps: [],
      historicalGaps: [],
      tailRecovery: null
    }
  };
}

function directorResourceKeyValue(value) {
  if (Number.isInteger(value)) return value >>> 0;
  const parsed = Number.parseInt(String(value || '').replace(/^0x/i, ''), 16);
  return Number.isFinite(parsed) ? parsed >>> 0 : null;
}

function stagePresentationFingerprint(presentation) {
  return sha256(JSON.stringify({
    selectorTableId: presentation.selectorTableId,
    selector: presentation.selector,
    environmentSelector: presentation.environmentSelector,
    foregroundSelectorTableId: presentation.foregroundSelectorTableId,
    foregroundSelector: presentation.foregroundSelector,
    resourceKey: presentation.resourceKey,
    stageLayers: presentation.stageLayers,
    stageAssetIds: presentation.stageAssetIds
  }));
}

function attachConcurrentDirectorContextOwners(scenes) {
  const sceneByDirectorKey = new Map(scenes.map((scene) => [
    directorResourceKeyValue(scene.directorKey), scene
  ]));
  let contextCount = 0;
  let exactOwnerCount = 0;
  let exactLaunchCount = 0;
  scenes.forEach((scene) => {
    scene.launchProfile.parentEventLaunches.forEach((launch) => {
      launch.eventInvocationContexts.forEach((context) => {
        contextCount += 1;
        const resourceKey = directorResourceKeyValue(
          context.precedingDirectorResourceKey);
        const owner = resourceKey === null
          ? null : sceneByDirectorKey.get(resourceKey) || null;
        context.concurrentDirectorSceneId = owner ? owner.sceneId : null;
        context.concurrentDirectorAssetId = owner ? owner.assetId : null;
        context.sceneStateRelation = owner
          ? 'previous-event-request-concurrent-scene-state'
          : 'no-exact-previous-director-request';
        if (owner) exactOwnerCount += 1;
        if (owner && context.precedingDirectorLaunchId) exactLaunchCount += 1;
      });
    });
  });
  return { contextCount, exactOwnerCount, exactLaunchCount };
}

function attachInheritedStagePresentations(scenes) {
  const sceneByDirectorKey = new Map(scenes.map((scene) => [
    directorResourceKeyValue(scene.directorKey), scene
  ]));
  const resolvedBySceneId = new Map();
  scenes.forEach((scene) => {
    const presentations = scene.launchProfile.background.requests.filter((request) =>
      Array.isArray(request.stageAssetIds) && request.stageAssetIds.length > 0);
    if (presentations.length !== 1) return;
    resolvedBySceneId.set(scene.sceneId, {
      presentation: presentations[0],
      fingerprint: stagePresentationFingerprint(presentations[0]),
      lineageDepth: 0,
      rootRequestIds: [presentations[0].requestId]
    });
  });

  let changed = true;
  while (changed) {
    changed = false;
    scenes.forEach((scene) => {
      if (resolvedBySceneId.has(scene.sceneId) ||
          scene.launchProfile.background.requests.length > 0) return;
      const contexts = scene.launchProfile.parentEventLaunches.flatMap((launch) =>
        launch.eventInvocationContexts.map((context) => ({ launch, context })));
      if (!contexts.length) return;
      const predecessors = contexts.map((row) => {
        const resourceKey = directorResourceKeyValue(
          row.context.precedingDirectorResourceKey);
        const predecessor = resourceKey === null
          ? null : sceneByDirectorKey.get(resourceKey);
        const resolution = predecessor
          ? resolvedBySceneId.get(predecessor.sceneId) : null;
        return { predecessor, resolution };
      });
      if (predecessors.some((row) => !row.resolution)) return;
      const fingerprints = new Set(predecessors.map((row) =>
        row.resolution.fingerprint));
      if (fingerprints.size !== 1) return;
      const source = predecessors[0].resolution;
      const immediatePredecessorSceneIds = Array.from(new Set(predecessors.map((row) =>
        row.predecessor.sceneId))).sort();
      const immediatePredecessorResourceKeys = Array.from(new Set(contexts.map((row) =>
        row.context.precedingDirectorResourceKey))).sort();
      const rootRequestIds = Array.from(new Set(predecessors.flatMap((row) =>
        row.resolution.rootRequestIds))).sort();
      const lineageDepth = Math.max(...predecessors.map((row) =>
        row.resolution.lineageDepth)) + 1;
      const presentation = {
        presentationId: scene.assetId + ':inherited-stage',
        sourceKind: 'parent-event-predecessor',
        selectorTableId: source.presentation.selectorTableId,
        selector: source.presentation.selector,
        environmentSelector: source.presentation.environmentSelector,
        foregroundSelectorTableId: source.presentation.foregroundSelectorTableId,
        foregroundSelector: source.presentation.foregroundSelector,
        foregroundSelectorCandidates:
          (source.presentation.foregroundSelectorCandidates || []).slice(),
        foregroundStatus: source.presentation.foregroundStatus,
        resourceKey: source.presentation.resourceKey,
        members: stableCopy(source.presentation.members || []),
        assetIds: (source.presentation.assetIds || []).slice(),
        stageLayers: stableCopy(source.presentation.stageLayers || []),
        stageAssetIds: (source.presentation.stageAssetIds || []).slice(),
        immediatePredecessorSceneIds,
        immediatePredecessorResourceKeys,
        rootRequestIds,
        contextCount: contexts.length,
        lineageDepth,
        sentinel: -2,
        evidenceStatus: 'native-static-parent-event-inheritance',
        status: 'The native -2 launch sentinel preserves the unanimous Stage established by the preceding Director request.'
      };
      scene.launchProfile.background.inheritedPresentation = presentation;
      resolvedBySceneId.set(scene.sceneId, {
        presentation,
        fingerprint: source.fingerprint,
        lineageDepth,
        rootRequestIds
      });
      changed = true;
    });
  }

  scenes.forEach((scene) => {
    if (scene.launchProfile.background.requests.length > 0) return;
    const inherited = scene.launchProfile.background.inheritedPresentation;
    const contexts = scene.launchProfile.parentEventLaunches.flatMap((launch) =>
      launch.eventInvocationContexts.map((context) => {
        const resourceKey = directorResourceKeyValue(
          context.precedingDirectorResourceKey);
        const predecessor = resourceKey === null
          ? null : sceneByDirectorKey.get(resourceKey);
        const resolution = predecessor
          ? resolvedBySceneId.get(predecessor.sceneId) : null;
        return {
          launchId: launch.launchId,
          eventDirectoryRow: launch.eventDirectoryRow,
          eventInvocationCursor: context.eventInvocationCursor,
          precedingDirectorLaunchCount: context.precedingDirectorLaunchCount,
          precedingDirectorSelector: context.precedingDirectorSelector,
          precedingDirectorResourceKey: context.precedingDirectorResourceKey,
          precedingSceneId: predecessor ? predecessor.sceneId : null,
          presentationFingerprint: resolution ? resolution.fingerprint : null,
          resolutionStatus: !context.precedingDirectorResourceKey
            ? 'no-preceding-director'
            : (resolution
              ? (inherited
                ? 'resolved-unanimous-stage'
                : 'context-stage-resolved-launch-selection-required')
              : 'predecessor-stage-unresolved')
        };
      }));
    scene.launchProfile.background.inheritanceContexts = contexts;
    if (!inherited) return;
    scene.backgroundCandidateAssetIds = Array.from(new Set(
      scene.backgroundCandidateAssetIds.concat(inherited.stageAssetIds)));
    if (!scene.backgroundRuntimeObservation) {
      scene.backgroundAssociationStatus =
        'The native -2 launch sentinel preserves one unanimous Stage from the preceding Director request.';
    }
  });

  return {
    exactSceneCount: scenes.filter((scene) =>
      scene.launchProfile.background.inheritedPresentation !== null).length,
    exactContextCount: scenes.reduce((total, scene) => total +
      (scene.launchProfile.background.inheritedPresentation
        ? scene.launchProfile.background.inheritanceContexts.length : 0), 0),
    contextOnlyResolvedCount: scenes.reduce((total, scene) => total +
      scene.launchProfile.background.inheritanceContexts.filter((context) =>
        context.resolutionStatus ===
          'context-stage-resolved-launch-selection-required').length, 0),
    unresolvedContextCount: scenes.reduce((total, scene) => total +
      scene.launchProfile.background.inheritanceContexts.filter((context) =>
        context.resolutionStatus === 'no-preceding-director' ||
        context.resolutionStatus === 'predecessor-stage-unresolved').length, 0)
  };
}

function stableCopy(value) {
  if (Array.isArray(value)) return value.map(stableCopy);
  if (value && Object.prototype.toString.call(value) === '[object Object]') {
    const output = {};
    Object.keys(value).sort().forEach((key) => { output[key] = stableCopy(value[key]); });
    return output;
  }
  return value;
}

function build(options) {
  const manifest = readJson(options.assetManifest);
  const directorCorpusAssets = readJsonLines(options.directorCorpusAssets);
  const directorCorpusGrammar = readJsonLines(options.directorCorpusGrammar);
  const directorCorpusNodes = readJsonLines(options.directorCorpusNodes);
  const directorCorpusWaits = readJsonLines(options.directorCorpusWaits);
  const archiveCatalog = readJson(options.archiveCatalog);
  const audioCatalog = readJson(options.audioCatalog);
  const poseVocabulary = readJson(options.poseVocabulary);
  const poseSelectors = readJsonLines(options.poseSelectors);
  const posePhysicalStates = readJsonLines(options.posePhysicalStates);
  const directorAssetCensus = readJsonLines(options.partialDirectors);
  const directorTailRecovery = readJson(options.directorTailRecovery);
  const directorSelectorTable = readJson(options.directorSelectorTable);
  const serifuPresentationSelectors = readJson(options.serifuPresentationSelectors);
  const spriteCorpus = readJson(options.spriteCorpus);
  if (!Array.isArray(manifest.assets) || manifest.assets.length !== 60) {
    throw new Error('Cutscene manifest must contain exactly 60 physical director resources.');
  }
  if (directorCorpusAssets.length !== 60 || directorCorpusGrammar.length !== 153 ||
      directorCorpusNodes.length !== 8451 || directorCorpusWaits.length !== 464) {
    throw new Error('Corrected Director corpus must contain 60 assets, 153 commands, ' +
      '8,451 nodes, and 464 registered waits.');
  }
  if (new Set(directorCorpusGrammar.map((row) => row.opcode_u32)).size !== 153 ||
      new Set(directorCorpusGrammar.map((row) => row.name)).size !== 153 ||
      directorCorpusAssets.reduce((total, row) => total + row.decoded_word_count, 0) !== 21927 ||
      directorCorpusNodes.reduce((total, row) => total + row.word_count, 0) !== 21927) {
    throw new Error('Corrected Director corpus grammar or word conservation is stale.');
  }
  if (!Array.isArray(archiveCatalog) || archiveCatalog.length !== 825) {
    throw new Error('Archive catalog must contain exactly 825 ROM-order members.');
  }
  if (MODE_TWO_ENVIRONMENT_RESOURCE_KEYS.length !== 80 ||
      MODE_TWO_ENVIRONMENT_ASSET_IDS.length !== 80 ||
      MODE_TWO_OVERLAY_RESOURCE_KEYS.length !== 80 ||
      MODE_TWO_ENVIRONMENT_RESOURCE_KEYS[57] !== 0x00236B58 ||
      MODE_TWO_ENVIRONMENT_ASSET_IDS[57] !== 'mode2-environment:00236B58' ||
      MODE_TWO_OVERLAY_RESOURCE_KEYS[57] !== 0x00304464 ||
      !BATTLE_BACKGROUND_ARCHIVE_ROWS[57] ||
      BATTLE_BACKGROUND_ARCHIVE_ROWS[57][0] !== 49) {
    throw new Error('Mode-2 environment and overlay selector tables are stale or conflated.');
  }
  if (!Array.isArray(poseVocabulary.globalBanks) ||
      poseVocabulary.globalBanks.length !== 68 ||
      !Array.isArray(poseVocabulary.physicalProgramIdentities)) {
    throw new Error('Pose vocabulary must contain all 68 Cutscene actor-art banks.');
  }
  if (poseSelectors.length !== 2275) {
    throw new Error('All-bank pose selector export must contain exactly 2,275 rows.');
  }
  if (posePhysicalStates.length !== 2275) {
    throw new Error('All-bank physical pose-state export must contain exactly 2,275 rows.');
  }
  if (directorTailRecovery.schemaVersion !==
      '2026-08-20.director-runtime-tail-recovery-v1' ||
      !Array.isArray(directorTailRecovery.assets) ||
      directorTailRecovery.assets.length !== 8 ||
      !directorTailRecovery.counts ||
      directorTailRecovery.counts.remainingGapWords !== 0) {
    throw new Error('Director tail recovery must contain eight complete runtime-tiled assets.');
  }
  if (directorSelectorTable.schemaVersion !==
      '2026-08-20.director-selector-table-v1' ||
      !directorSelectorTable.counts ||
      directorSelectorTable.counts.selectorRows !== 1693 ||
      directorSelectorTable.counts.acceptedAssets !== 60 ||
      directorSelectorTable.counts.acceptedSelectorRows !== 76 ||
      !Array.isArray(directorSelectorTable.assets) ||
      directorSelectorTable.assets.length !== 60) {
    throw new Error('Director selector-table ownership must contain 60 assets and 76 rows.');
  }
  if (serifuPresentationSelectors.schemaVersion !==
      '2026-08-20.serifu-presentation-selector-v1' ||
      !Array.isArray(serifuPresentationSelectors.entries) ||
      serifuPresentationSelectors.entries.length !== 503 ||
      !serifuPresentationSelectors.counts ||
      serifuPresentationSelectors.counts.populatedSelectors !== 348) {
    throw new Error('Serifu presentation selector artifact must contain 503 slots and 348 archives.');
  }
  const retailDirectorGrammar = buildRetailDirectorGrammar(
    directorCorpusGrammar, directorCorpusNodes);
  const retailDirectorInventory = readRetailDirectorInventory(
    options, manifest, retailDirectorGrammar);
  const directorContinuationStreams = readDirectorContinuationStreams(
    retailDirectorInventory.z64, retailDirectorGrammar);
  const animatedSceneSpriteRotationRoutes = retailDirectorInventory.resources
    .reduce((counts, resource) => {
      const routes = animatedSceneSpriteRotationRouteCounts(resource.nodes);
      counts.directOperand += routes.directOperand;
      counts.sampledScenePath += routes.sampledScenePath;
      counts.resourcePath += routes.resourcePath;
      return counts;
    }, { directOperand: 0, sampledScenePath: 0, resourcePath: 0 });
  directorContinuationStreams.forEach((stream) => {
    const routes = stream.animatedSceneSpriteRotationRoutes;
    animatedSceneSpriteRotationRoutes.directOperand += routes.directOperand;
    animatedSceneSpriteRotationRoutes.sampledScenePath += routes.sampledScenePath;
    animatedSceneSpriteRotationRoutes.resourcePath += routes.resourcePath;
  });
  if (animatedSceneSpriteRotationRoutes.directOperand !== 2001 ||
      animatedSceneSpriteRotationRoutes.sampledScenePath !== 0 ||
      animatedSceneSpriteRotationRoutes.resourcePath !== 0) {
    throw new Error('Retail Director animated scene-sprite rotation routes changed: ' +
      JSON.stringify(animatedSceneSpriteRotationRoutes) + '.');
  }
  const modeTwoStagePlacementInventory = compactModeTwoStagePlacementProfiles(
    retailDirectorInventory.z64, spriteCorpus);
  const modeTwoDerivedEnvironmentRules = readModeTwoDerivedEnvironmentRules(
    retailDirectorInventory.z64);
  const oversizedImagePresentationRules = readOversizedImagePresentationRules(
    retailDirectorInventory.z64, archiveCatalog);
  const sceneResourcePathInventory = readSceneResourcePaths(
    retailDirectorInventory.z64);
  const tailRecoveryMap = new Map();
  directorTailRecovery.assets.forEach((recovery) => {
    if (tailRecoveryMap.has(recovery.assetId)) {
      throw new Error('Duplicate Director tail recovery ' + recovery.assetId + '.');
    }
    tailRecoveryMap.set(recovery.assetId, recovery);
  });
  const selectors = poseSelectorMap(poseSelectors);
  const corpusAssetMap = new Map();
  const corpusNodesByAsset = new Map();
  const corpusWaitsByAsset = new Map();
  directorCorpusAssets.forEach((row) => {
    if (corpusAssetMap.has(row.asset_id)) {
      throw new Error('Duplicate corrected Director asset ' + row.asset_id + '.');
    }
    corpusAssetMap.set(row.asset_id, row);
    corpusNodesByAsset.set(row.asset_id, []);
    corpusWaitsByAsset.set(row.asset_id, []);
  });
  directorCorpusNodes.forEach((row) => {
    const rows = corpusNodesByAsset.get(row.asset_id);
    if (!rows) throw new Error('Corrected Director node has an unknown asset ' + row.asset_id + '.');
    rows.push(row);
  });
  directorCorpusWaits.forEach((row) => {
    const rows = corpusWaitsByAsset.get(row.asset_id);
    if (!rows) throw new Error('Corrected Director wait has an unknown asset ' + row.asset_id + '.');
    rows.push(row);
  });
  corpusAssetMap.forEach((asset, assetId) => {
    const nodes = corpusNodesByAsset.get(assetId).sort((left, right) =>
      left.word_start - right.word_start);
    const waits = corpusWaitsByAsset.get(assetId).sort((left, right) =>
      left.word_start - right.word_start);
    let cursor = 0;
    nodes.forEach((node) => {
      if (node.word_start !== cursor || node.word_end_exclusive - node.word_start !==
          node.word_count) {
        throw new Error('Corrected Director nodes do not tile ' + assetId +
          ' at word ' + cursor + '.');
      }
      cursor = node.word_end_exclusive;
    });
    if (cursor !== asset.decoded_word_count || nodes.length !== asset.node_count ||
        waits.length !== asset.registered_wait_count) {
      throw new Error('Corrected Director asset counts are stale for ' + assetId + '.');
    }
  });
  const directorSelectorOwners = new Map(retailDirectorInventory.resources.map((resource) => [
    resource.assetId,
    {
      tableResourceKey: '0x' + hex(DIRECTOR_SELECTOR_TABLE_KEY, 8),
      tablePayloadZ64: DIRECTOR_SELECTOR_TABLE_PAYLOAD_Z64,
      selectorRows: resource.selectorRows,
      selectorWordZ64: resource.selectorWordZ64
    }
  ]));
  const retailResourceByAssetId = new Map(retailDirectorInventory.resources.map((resource) =>
    [resource.assetId, resource]));
  const recoveredAssets = manifest.assets.map((asset) =>
    applyDirectorTailRecovery(asset, tailRecoveryMap.get(asset.assetId)));
  const profiledScenes = recoveredAssets.map((asset) => {
    const selectorOwner = directorSelectorOwners.get(asset.assetId);
    if (!selectorOwner) throw new Error('Director selector owner is missing for ' + asset.assetId + '.');
    return compactScene(asset, corpusAssetMap.get(asset.assetId),
      corpusNodesByAsset.get(asset.assetId), corpusWaitsByAsset.get(asset.assetId),
      selectors, selectorOwner, retailResourceByAssetId.get(asset.assetId),
      modeTwoDerivedEnvironmentRules, oversizedImagePresentationRules);
  });
  const profiledSceneByAssetId = new Map(profiledScenes.map((scene) =>
    [scene.assetId, scene]));
  const scenes = retailDirectorInventory.resources.map((resource) =>
    profiledSceneByAssetId.get(resource.assetId) || compactRetailScene(
      resource, selectors, modeTwoDerivedEnvironmentRules,
      oversizedImagePresentationRules));
  const concurrentDirectorContextInventory =
    attachConcurrentDirectorContextOwners(scenes);
  const inheritedStageInventory = attachInheritedStagePresentations(scenes);
  const partialDirectorResources = PARTIAL_DIRECTOR_RESOURCE_IDS.map((resourceId) => {
    const row = directorAssetCensus.find((candidate) => candidate.assetId === resourceId);
    if (!row || row.disposition !== 'director-partial') {
      throw new Error('Director Partial census row is missing for ' + resourceId + '.');
    }
    return compactPartialDirectorResource(row, tailRecoveryMap.get(resourceId), selectors);
  });
  const partialResourceMap = new Map(partialDirectorResources.map((resource) =>
    [resource.resourceId, resource]));
  const presentationScenes = NON_DIRECTOR_PRESENTATIONS.map((row) =>
    compactPresentationScene(row, selectors, partialResourceMap, tailRecoveryMap));
  const archiveImageAssets = assembleFormatFiveCompounds(
    archiveCatalog.filter(isImageCandidate)
      .map((archive) => compactArchiveAsset(options.extractedRoot, archive))
      .sort((left, right) => left.archiveIndex - right.archiveIndex));
  const sectionCImageAssets = SECTION_C_ASSETS.map((row) =>
    compactSectionCAsset(row, manifest.masterRomSha256));
  const sceneGroupImageAssets = SCENE_GROUP_RESOURCE_ROWS.map(compactSceneGroupResourceAsset);
  const modeTwoDirectEnvironmentAssets = MODE_TWO_DIRECT_ENVIRONMENT_ROWS
    .map(compactModeTwoDirectEnvironmentAsset);
  const imageAssets = archiveImageAssets.concat(
    sceneGroupImageAssets, modeTwoDirectEnvironmentAssets, sectionCImageAssets);
  const poseCatalog = compactPoseCatalog(
    poseVocabulary, spriteCorpus, scenes, poseSelectors, posePhysicalStates);
  const compactSelectors = compactPoseSelectors(poseSelectors);
  const dialogueArchives = compactDialogueArchives(
    options.extractedRoot, archiveCatalog, serifuPresentationSelectors);
  const dialogueEntries = dialogueArchives.reduce((total, archive) =>
    total + archive.entryCount, 0);
  const audioBlocks = compactAudioBlocks(audioCatalog);
  const directorEvents = compactDirectorEvents(
    retailDirectorGrammar, retailDirectorInventory.opcodeStats);
  const backgroundSelectorTables = compactBackgroundSelectorTables();
  const spriteArtResources = spriteCorpus.cutscene.artResources;
  const distinctSpriteChildren = spriteArtResources.reduce((total, resource) =>
    total + resource.childCount, 0);
  const catalogPoseIds = new Set(poseCatalog.posePrograms.map((program) => program.poseId));
  const renderablePoseIds = new Set(poseCatalog.posePrograms
    .filter((program) => program.frames.length).map((program) => program.poseId));
  const modeTwoDirectorScenes = scenes.filter((scene) =>
    scene.launchProfile.directorMode.value === 2);
  const modeTwoBackgroundRequests = modeTwoDirectorScenes.flatMap((scene) =>
    scene.launchProfile.background.requests);
  const sceneHasProfiledStage = (scene) =>
    (scene.backgroundRuntimeObservation &&
      Array.isArray(scene.backgroundRuntimeObservation.stageLayers) &&
      scene.backgroundRuntimeObservation.stageLayers.length) ||
    scene.launchProfile.background.requests.some((request) =>
      request.stageAssetIds.length) ||
    (scene.launchProfile.background.inheritedPresentation &&
      scene.launchProfile.background.inheritedPresentation.stageAssetIds.length);
  const modeTwoCommandSeededEnvironmentScenes = modeTwoDirectorScenes.filter((scene) =>
    scene.launchProfile.background.requests.some((request) =>
      Number.isInteger(request.commandOperand) && request.commandOperand >= 0 &&
      request.commandOperand < MODE_TWO_ENVIRONMENT_RESOURCE_KEYS.length));
  const unresolvedModeTwoForegroundScenes = modeTwoDirectorScenes.filter((scene) =>
    scene.launchProfile.background.requests.length === 0 ||
    !scene.launchProfile.background.requests.some((request) =>
      Number.isInteger(request.foregroundSelector)));
  const modeTwoDerivedEnvironmentRequests = modeTwoBackgroundRequests.filter((request) =>
    request.commandOperand === -1);
  const modeTwoDerivedEnvironmentContexts = modeTwoDerivedEnvironmentRequests.flatMap((request) =>
    request.derivedEnvironment ? request.derivedEnvironment.contexts : []);
  if (modeTwoDirectorScenes.length !== 1175 || modeTwoBackgroundRequests.length !== 1163 ||
      modeTwoCommandSeededEnvironmentScenes.length !== 1146 ||
      modeTwoDerivedEnvironmentRequests.length !== 17 ||
      modeTwoDerivedEnvironmentContexts.some((context) =>
        context.resolutionStatus !== 'launch-inputs-unresolved') ||
      modeTwoBackgroundRequests.some((request) => request.wordStart !== 0) ||
      unresolvedModeTwoForegroundScenes.length !== 43) {
    throw new Error('Mode-two launch pre-scan coverage is stale.');
  }
  const sceneGroupPreloadScenes = scenes.filter((scene) =>
    scene.launchProfile.launchContext &&
    [4, 5].includes(scene.launchProfile.launchContext.classId));
  const sceneGroupPreloadRequests = sceneGroupPreloadScenes.flatMap((scene) =>
    scene.launchProfile.background.requests);
  if (sceneGroupPreloadScenes.length !== 209 || sceneGroupPreloadRequests.length !== 191 ||
      sceneGroupPreloadRequests.some((request) =>
        request.selectorTableId !== 'background-table:scene:31' ||
        request.selectorSource !== 'director-launch-prescan-class-4-or-5-scene-group')) {
    throw new Error('Terminal-class scene-group preload coverage is stale.');
  }
  const oversizedImageScenes = scenes.filter((scene) =>
    scene.launchProfile.oversizedImagePresentation !== null);
  const exactOversizedImageScenes = oversizedImageScenes.filter((scene) =>
    scene.launchProfile.oversizedImagePresentation.assetId !== null);
  const structuralOversizedImageScenes = oversizedImageScenes.filter((scene) =>
    scene.launchProfile.oversizedImagePresentation.source ===
      'director-launch-prescan-opcode-0x80000007');
  if (oversizedImageScenes.length !== 117 || exactOversizedImageScenes.length !== 106 ||
      structuralOversizedImageScenes.length !== 106) {
    throw new Error('Terminal-class-4 oversized-image coverage is stale.');
  }
  const sceneVignetteCommands = scenes.flatMap((scene) => {
    const resource = retailResourceByAssetId.get(scene.assetId);
    return (resource ? resource.nodes : []).filter((node) => node.opcode === 0x3A)
      .map((node) => ({ scene, node }));
  });
  const sceneVignetteScenes = new Set(sceneVignetteCommands.map((row) =>
    row.scene.assetId));
  const exactSceneVignetteScenes = new Set(sceneVignetteCommands.filter((row) =>
    row.scene.launchProfile.oversizedImagePresentation &&
      row.scene.launchProfile.oversizedImagePresentation.assetId !== null)
    .map((row) => row.scene.assetId));
  if (sceneVignetteCommands.length !== 107 || sceneVignetteScenes.size !== 107 ||
      exactSceneVignetteScenes.size !== 106 ||
      sceneVignetteCommands.some(({ scene, node }) =>
        scene.launchProfile.launchContext.classId !== 4 ||
        node.rawWords.length !== 9 ||
        signed(node.rawWords[1]) !== 2 ||
        signed(node.rawWords[4]) !== 0 ||
        signed(node.rawWords[5]) <= 0 ||
        signed(node.rawWords[5]) !== signed(node.rawWords[6]) ||
        signed(node.rawWords[7]) !== 130 ||
        ![4, 8].includes(Number(node.rawWords[8]) >>> 0))) {
    throw new Error('Opcode-0x3A scene-vignette coverage is stale.');
  }
  const launchTranslatedScenes = scenes.filter((scene) =>
    scene.launchProfile.operandTranslation.required === true);
  const launchTranslationPlaceholders = launchTranslatedScenes.reduce((total, scene) =>
    total + scene.launchProfile.operandTranslation.placeholderCount, 0);
  const launchTranslationIndexes = new Set(launchTranslatedScenes.flatMap((scene) =>
    scene.launchProfile.operandTranslation.tableIndexes));
  if (launchTranslatedScenes.length !== 19 || launchTranslationPlaceholders !== 398 ||
      launchTranslationIndexes.size !== 17) {
    throw new Error('Director launch-translation placeholder coverage is stale.');
  }
  const directEventInvocationContexts =
    retailDirectorInventory.eventLaunchInventory.launches.flatMap((launch) =>
      launch.eventInvocationContexts);
  const exactEventPropertyE6Contexts = directEventInvocationContexts.filter((context) =>
    context.eventPropertyE6 !== null);
  const exactEventPropertyE9Contexts = directEventInvocationContexts.filter((context) =>
    context.eventPropertyE9 !== null);
  const exactEventPropertyFCContexts = directEventInvocationContexts.filter((context) =>
    context.eventPropertyFC !== null);
  const exactEventPropertyFDContexts = directEventInvocationContexts.filter((context) =>
    context.eventPropertyFD !== null);
  const requiredLaunchPreservationSnapshotContexts =
    exactEventPropertyE6Contexts.filter((context) =>
      context.launchPreservationSnapshot === true);
  const omittedLaunchPreservationSnapshotContexts =
    exactEventPropertyE6Contexts.filter((context) =>
      context.launchPreservationSnapshot === false);
  const secondRosterUnitLeaderOnlyContexts =
    exactEventPropertyE6Contexts.filter((context) =>
      context.secondRosterUnitLeaderOnly === true);
  if (directEventInvocationContexts.length !== EVENT_STATIC_INVOCATION_CONTEXTS ||
      exactEventPropertyE6Contexts.length !== 18 ||
      requiredLaunchPreservationSnapshotContexts.length !== 16 ||
      omittedLaunchPreservationSnapshotContexts.length !== 2 ||
      secondRosterUnitLeaderOnlyContexts.length !== 4 ||
      directEventInvocationContexts.some((context) => context.launchFlagBit08 !== false)) {
    throw new Error('Direct-event launch preservation and roster coverage is stale.');
  }
  if (exactEventPropertyE9Contexts.length !== 183 ||
      exactEventPropertyFCContexts.length !== 0 ||
      exactEventPropertyFDContexts.length !== 25) {
    throw new Error('Direct-event derived-environment input coverage is stale.');
  }
  if (inheritedStageInventory.exactSceneCount !== 90 ||
      inheritedStageInventory.exactContextCount !== 177 ||
      inheritedStageInventory.contextOnlyResolvedCount !== 18 ||
      inheritedStageInventory.unresolvedContextCount !== 209) {
    throw new Error('Parent-event Stage inheritance coverage is stale: ' +
      'scenes=' + inheritedStageInventory.exactSceneCount + ', contexts=' +
      inheritedStageInventory.exactContextCount + ', contextOnly=' +
      inheritedStageInventory.contextOnlyResolvedCount + ', unresolved=' +
      inheritedStageInventory.unresolvedContextCount + '.');
  }
  const data = {
    format: 'ob64-cutscene-catalog',
    schemaVersion: 16,
    sourceRevision: 'us-rev0',
    counts: {
      scenes: scenes.length,
      presentationScenes: presentationScenes.length,
      partialDirectorResources: partialDirectorResources.length,
      profiledDirectorResources: directorCorpusAssets.length,
      runtimeTiledDirectorResources: retailDirectorInventory.uniqueResources,
      retailDirectorSelectorRows: DIRECTOR_SELECTOR_ROWS,
      populatedRetailDirectorSelectorRows: retailDirectorInventory.populatedRows,
      retailDirectorResources: retailDirectorInventory.uniqueResources,
      retailDirectorWords: retailDirectorInventory.totalWords,
      retailDirectorNodes: retailDirectorInventory.totalNodes,
      retailDirectorOpcodeDefinitions: retailDirectorGrammar.length,
      retailDirectorSubstreamCalls: RETAIL_DIRECTOR_SUBSTREAM_CALLS,
      retailDirectorSelectorSubstreamCalls:
        retailDirectorInventory.selectorExpandedSubstreamCalls,
      directorContinuationStreams: directorContinuationStreams.length,
      retailDirectorTailCalls: RETAIL_DIRECTOR_TAIL_CALLS,
      directEventDirectorLaunches:
        retailDirectorInventory.eventLaunchInventory.launches.length,
      directEventDirectorSelectors:
        retailDirectorInventory.eventLaunchInventory.selectorCount,
      directEventDirectorResources:
        retailDirectorInventory.eventLaunchInventory.resourceCount,
      parentEventOuterEntries:
        retailDirectorInventory.eventLaunchInventory.outerEntryCount,
      parentEventDistinctOuterCursors:
        retailDirectorInventory.eventLaunchInventory.distinctOuterCursorCount,
      parentEventSequenceTables:
        retailDirectorInventory.eventLaunchInventory.sequenceTableCount,
      parentEventSequenceEntries:
        retailDirectorInventory.eventLaunchInventory.sequenceEntryCount,
      parentEventDirectOuterSequences:
        retailDirectorInventory.eventLaunchInventory.directOuterSequenceCount,
      parentEventDistinctSequenceCursors:
        retailDirectorInventory.eventLaunchInventory.distinctSequenceCursorCount,
      directEventLaunchEntryCursors:
        retailDirectorInventory.eventLaunchInventory.launchEntryCursorCount,
      directEventInvocationContexts:
        retailDirectorInventory.eventLaunchInventory.invocationContextCount,
      directEventConcurrentContextOwners:
        concurrentDirectorContextInventory.exactOwnerCount,
      directEventExactConcurrentLaunchOwners:
        concurrentDirectorContextInventory.exactLaunchCount,
      directEventMultiInvocationLaunches:
        retailDirectorInventory.eventLaunchInventory.multiInvocationLaunchCount,
      parentEventDistinctInvocationCursors:
        retailDirectorInventory.eventLaunchInventory.distinctInvocationCursorCount,
      parentEventExternalRequestPhysicalSites:
        retailDirectorInventory.eventLaunchInventory.externalRequestPhysicalSiteCount,
      parentEventExternalRequestHandoffs:
        retailDirectorInventory.eventLaunchInventory.externalRequestHandoffCount,
      parentEventTranslationWriteContexts:
        retailDirectorInventory.eventLaunchInventory.translationWrites.length,
      parentEventSubstitutionSourceWriteContexts:
        retailDirectorInventory.eventLaunchInventory.substitutionSourceWrites.length,
      parentEventSubstitutionSourcePhysicalSites:
        retailDirectorInventory.eventLaunchInventory.substitutionSourcePhysicalSiteCount,
      parentEventSubstitutionSourceAWriteContexts:
        retailDirectorInventory.eventLaunchInventory.substitutionSourceAWriteCount,
      parentEventSubstitutionSourceBWriteContexts:
        retailDirectorInventory.eventLaunchInventory.substitutionSourceBWriteCount,
      parentEventExactSubstitutionSourceIndexContexts:
        retailDirectorInventory.eventLaunchInventory.exactSubstitutionSourceIndexWriteCount,
      parentEventUnresolvedSubstitutionSourceIndexContexts:
        retailDirectorInventory.eventLaunchInventory.unresolvedSubstitutionSourceIndexWriteCount,
      parentEventExactSubstitutionSourceValueContexts:
        retailDirectorInventory.eventLaunchInventory.exactSubstitutionSourceValueWriteCount,
      parentEventUnresolvedSubstitutionSourceValueContexts:
        retailDirectorInventory.eventLaunchInventory.unresolvedSubstitutionSourceValueWriteCount,
      parentEventTranslationPhysicalSites:
        retailDirectorInventory.eventLaunchInventory.translationPhysicalSiteCount,
      parentEventExactTranslationWriteContexts:
        retailDirectorInventory.eventLaunchInventory.exactTranslationWriteCount,
      parentEventUnresolvedTranslationWriteContexts:
        retailDirectorInventory.eventLaunchInventory.unresolvedTranslationWriteCount,
      parentEventRetailTranslationPhysicalSites:
        retailDirectorInventory.eventLaunchInventory.retailTranslationPhysicalSiteCount,
      parentEventRetailTranslationWriteContexts:
        retailDirectorInventory.eventLaunchInventory.retailTranslationWriteCount,
      parentEventExactRetailTranslationWriteContexts:
        retailDirectorInventory.eventLaunchInventory.exactRetailTranslationWriteCount,
      parentEventUnresolvedRetailTranslationWriteContexts:
        retailDirectorInventory.eventLaunchInventory.unresolvedRetailTranslationWriteCount,
      parentEventNonretailTranslationPhysicalSites:
        retailDirectorInventory.eventLaunchInventory.nonretailTranslationPhysicalSiteCount,
      parentEventNonretailTranslationWriteContexts:
        retailDirectorInventory.eventLaunchInventory.nonretailTranslationWriteCount,
      directEventExactPropertyE6Contexts: exactEventPropertyE6Contexts.length,
      directEventExactPropertyE9Contexts: exactEventPropertyE9Contexts.length,
      directEventExactPropertyFCContexts: exactEventPropertyFCContexts.length,
      directEventExactPropertyFDContexts: exactEventPropertyFDContexts.length,
      directEventRequiredLaunchPreservationSnapshotContexts:
        requiredLaunchPreservationSnapshotContexts.length,
      directEventOmittedLaunchPreservationSnapshotContexts:
        omittedLaunchPreservationSnapshotContexts.length,
      directEventSecondRosterUnitLeaderOnlyContexts:
        secondRosterUnitLeaderOnlyContexts.length,
      retailDirectorLaunchContextClasses: retailDirectorInventory.resources.reduce(
        (counts, resource) => {
          const classId = resource.launchContext.classId;
          counts[classId] = (counts[classId] || 0) + 1;
          return counts;
        }, {}),
      launchTranslatedDirectorResources: launchTranslatedScenes.length,
      launchTranslationPlaceholders,
      launchTranslationIndexes: launchTranslationIndexes.size,
      recoveredDirectorTailNodes: directorTailRecovery.counts.recoveredNodes,
      remainingDirectorGapWords: 0,
      directorWords: directorCorpusNodes.reduce((total, node) => total + node.word_count, 0),
      directorNodes: directorCorpusNodes.length,
      directorOpcodeDefinitions: directorCorpusGrammar.length,
      registeredDirectorWaits: directorCorpusWaits.length,
      selectableScenes: scenes.length + presentationScenes.length,
      actorBearingScenes: scenes.filter((scene) => scene.actorBearing).length,
      actorTracks: scenes.reduce((total, scene) => total + scene.actorCount, 0),
      imageAssets: imageAssets.length,
      renderableImageAssets: imageAssets.filter((asset) => asset.renderable).length,
      nativeSceneResourceImages: sceneGroupImageAssets.length,
      nativeModeTwoDirectEnvironmentImages: modeTwoDirectEnvironmentAssets.length,
      backgroundAssets: imageAssets.filter((asset) => asset.family === 'background').length,
      backgroundCandidateAssets: imageAssets.filter((asset) =>
        asset.family === 'background-candidate').length,
      effectAssets: imageAssets.filter((asset) => asset.family === 'effect').length,
      directArchiveImageLoaders: imageAssets.filter((asset) =>
        asset.consumerEvidence && asset.consumerEvidence.evidenceGrade === 'direct').length,
      boundedArchiveImageConsumerLeads: imageAssets.filter((asset) =>
        asset.consumerEvidence != null).length,
      dialogueArchives: dialogueArchives.length,
      dialogueEntries,
      serifuPresentationSelectorSlots: serifuPresentationSelectors.entries.length,
      populatedSerifuPresentationSelectors:
        serifuPresentationSelectors.counts.populatedSelectors,
      audioBlocks: audioBlocks.length,
      registeredAudioRequests: REGISTERED_AUDIO_REQUESTS.length,
      recoveredFullStreamSpriteEffects: scenes.reduce((total, scene) =>
        total + scene.recoveredNativeSpriteEffects.length, 0),
      recoveredPartialSpriteEffects: partialDirectorResources.reduce((total, resource) =>
        total + resource.recoveredNativeSpriteEffects.length, 0),
      recoveredFullStreamActorEvents: scenes.reduce((total, scene) =>
        total + scene.recoveredActorEvents.length, 0),
      recoveredPartialActorEvents: partialDirectorResources.reduce((total, resource) =>
        total + resource.recoveredActorEvents.length, 0),
      recoveredAppliedActorEvents: scenes.flatMap((scene) => scene.recoveredActorEvents)
        .concat(partialDirectorResources.flatMap((resource) => resource.recoveredActorEvents))
        .filter((event) => event.eventStatus === 'applied').length,
      recoveredSpriteEffectPhysicalStates: new Set(scenes.concat(presentationScenes)
        .flatMap((scene) => scene.recoveredNativeSpriteEffects)
        .map((effect) => effect.physicalStateId).filter(Boolean)).size,
      directorEvents: directorEvents.reduce((total, event) => total + event.occurrenceCount, 0),
      backgroundSelectorTables: backgroundSelectorTables.length,
      backgroundSelectorRows: backgroundSelectorTables.reduce((total, table) =>
        total + table.entryCount, 0),
      modeTwoStagePlacementProfiles:
        modeTwoStagePlacementInventory.profiles.length,
      modeTwoStagePlacementSelectors:
        modeTwoStagePlacementInventory.nonemptySelectorCount,
      modeTwoOrthographicStagePlacementRows:
        modeTwoStagePlacementInventory.orthographicRowCount,
      modeTwoPerspectiveStagePlacementRows:
        modeTwoStagePlacementInventory.perspectiveRowCount,
      modeTwoNormalStagePlacements:
        modeTwoStagePlacementInventory.normalRowCount,
      modeTwoSpecialStagePlacementRows:
        modeTwoStagePlacementInventory.specialRowCount,
      evidenceBackedDirectorStages: scenes.filter(sceneHasProfiledStage).length,
      inheritedStageDirectorResources: inheritedStageInventory.exactSceneCount,
      inheritedStageLaunchContexts: inheritedStageInventory.exactContextCount,
      contextOnlyResolvedStageInheritanceContexts:
        inheritedStageInventory.contextOnlyResolvedCount,
      unresolvedStageInheritanceContexts: inheritedStageInventory.unresolvedContextCount,
      sceneGroupPreloadDirectorResources: sceneGroupPreloadScenes.length,
      sceneGroupPreloadBackgroundCommands: sceneGroupPreloadRequests.length,
      classFourOversizedImageScenes: oversizedImageScenes.length,
      exactClassFourOversizedImageScenes: exactOversizedImageScenes.length,
      unresolvedClassFourOversizedImageScenes:
        oversizedImageScenes.length - exactOversizedImageScenes.length,
      sceneVignetteDirectorResources: sceneVignetteScenes.size,
      exactSceneVignetteDirectorResources: exactSceneVignetteScenes.size,
      unresolvedSceneVignetteDirectorResources:
        sceneVignetteScenes.size - exactSceneVignetteScenes.size,
      modeTwoCommandSeededEnvironmentStages: modeTwoCommandSeededEnvironmentScenes.length,
      modeTwoDerivedEnvironmentSentinels: modeTwoDirectorScenes.filter((scene) =>
        scene.launchProfile.background.requests.some((request) =>
          request.commandOperand === -1)).length,
      modeTwoDerivedEnvironmentInvocationContexts:
        modeTwoDerivedEnvironmentContexts.length,
      exactModeTwoDerivedEnvironmentInvocationContexts:
        modeTwoDerivedEnvironmentContexts.filter((context) =>
          context.resolutionStatus === 'exact-native-mapper-result').length,
      modeTwoScenesWithoutBackgroundCommand: modeTwoDirectorScenes.filter((scene) =>
        scene.launchProfile.background.requests.length === 0).length,
      unresolvedModeTwoLaunchStages: modeTwoDirectorScenes.filter((scene) =>
        !sceneHasProfiledStage(scene)).length,
      unresolvedModeTwoForegroundSelections: unresolvedModeTwoForegroundScenes.length,
      observedActorLaunchCameras: scenes.filter((scene) =>
        scene.launchProfile.cameras.actor.evidenceStatus === 'runtime-observed').length,
      actorArtSources: poseCatalog.actorArtSources.length,
      actorArtResources: spriteArtResources.length,
      actorArtChildren: distinctSpriteChildren,
      actorAppearanceValues: 8,
      posePhysicalStates: compactSelectors.length,
      posePrograms: poseCatalog.posePrograms.length,
      sourcePosePrograms: poseCatalog.sourcePoseProgramCount,
      sourcePublishedPhysicalStates: poseCatalog.sourcePublishedPhysicalStateCount,
      poseSelectors: catalogPoseIds.size,
      renderablePoseSelectors: renderablePoseIds.size,
      controlOnlyPoseSelectors: catalogPoseIds.size - renderablePoseIds.size,
      emptyPosePrograms: poseCatalog.posePrograms.filter((program) =>
        program.emptyProgram === true).length,
      structuralPoseStates: compactSelectors.filter((selector) =>
        !catalogPoseIds.has(selector.poseId)).length,
      sourceUnpublishedPoseStates: compactSelectors.filter((selector) =>
        !selector.sourceProgramDefined).length,
      sceneResourcePathGroups: sceneResourcePathInventory.groups.length,
      sceneResourcePathEntries: sceneResourcePathInventory.entries.length,
      populatedSceneResourcePaths: sceneResourcePathInventory.entries.filter((entry) =>
        entry.resourceKey !== null).length,
      animatedSceneSpriteDirectRotationCommands:
        animatedSceneSpriteRotationRoutes.directOperand,
      animatedSceneSpriteSampledPathRotationCommands:
        animatedSceneSpriteRotationRoutes.sampledScenePath,
      animatedSceneSpriteResourcePathRotationCommands:
        animatedSceneSpriteRotationRoutes.resourcePath
    },
    provenance: {
      generator: 'tools/generate-cutscene-data.js',
      assetManifestPath: 'tools/cutscene-workbench/generated/asset-manifest.json',
      assetManifestSha256: sha256File(options.assetManifest),
      assetManifestSchemaVersion: manifest.schemaVersion,
      directorCorpusAssetsPath:
        'wiki/cutscene-director-corpus-153-grammar-static-r1-20260821/exports/assets.jsonl',
      directorCorpusAssetsSha256: sha256File(options.directorCorpusAssets),
      directorCorpusGrammarPath:
        'wiki/cutscene-director-corpus-153-grammar-static-r1-20260821/exports/grammar.jsonl',
      directorCorpusGrammarSha256: sha256File(options.directorCorpusGrammar),
      directorCorpusNodesPath:
        'wiki/cutscene-director-corpus-153-grammar-static-r1-20260821/exports/nodes.jsonl',
      directorCorpusNodesSha256: sha256File(options.directorCorpusNodes),
      directorCorpusRegisteredWaitsPath:
        'wiki/cutscene-director-corpus-153-grammar-static-r1-20260821/exports/registered-waits.jsonl',
      directorCorpusRegisteredWaitsSha256: sha256File(options.directorCorpusWaits),
      directorCorpusStatus:
        'all 60 resources, 8,451 nodes, and 21,927 words use the corrected 153-command grammar with zero raw gaps',
      retailDirectorStatus:
        'all 1,498 unique nonzero resources from 1,548 populated selector rows decode, tile, and end in an exact terminal boundary; interactive execution loops remain runtime inputs; opcode 0x5E remains structural-width-only',
      parentEventDirectorLaunchStatus:
        'all 1,998 direct 0x68/[0x88]/0x10 0x00 launch sites are statically reachable from their native event entry cursors; 2,076 admissible invocation contexts preserve post-Director resumes, one-call external-request handoffs, and 16-bit wrapping calls',
      parentEventExternalRequestStatus:
        '45 physical opcode-0x13 sites produce 47 statically distinct invocation handoffs; each writes its request code and acceptance byte synchronously, then resumes at the following instruction on the next event-state processor call; the former 84-loop count came from an invalid dual-edge model',
      parentEventRosterStatus:
        'mode-two launch Actor-input rows come from one or two current gameplay units, not the preservation snapshot; 18 direct-event contexts have exact event-property 0xE6 values, and four set bit 0x8000 to limit the second unit to its leader',
      parentEventPreservationStatus:
        'direct event programs keep launch flag bit 0x08 clear; exact event-property 0xE6 values require the separate gameplay-state preservation snapshot in 16 contexts and omit it in two',
      parentEventDirectoryResourceKey: '0x' + hex(EVENT_DIRECTORY_KEY, 8),
      parentEventDirectorySha256:
        retailDirectorInventory.eventLaunchInventory.directorySha256,
      launchOperandTranslationStatus:
        'all 1,513 event sequences reach 416 writes at 69 physical translation-setter sites; the 1,443 launch-owning sequences account for 411 writes at 64 sites; 344 contexts from 21 sites target retail placeholder indexes zero through 16, with 74 exact values and 270 roster-dependent values; event-VM propagation attaches only path-safe launch halfwords',
      launchSubstitutionSourceStatus:
        'event row 67 contains all 42 source-bank setter sites; 21 writes target primary-class slots and 21 target secondary-class slots; 38 values come from roster-dependent paths and four are fixed fallback class values; the native program compacts matching 56-byte character records into five source pairs before the translated Director family launches',
      masterRomPath: path.basename(options.rom),
      rawV64Sha256: retailDirectorInventory.rawSha256,
      normalizedZ64Sha256: retailDirectorInventory.normalizedSha256,
      retailDirectorSelectorTableResourceKey: '0x' + hex(DIRECTOR_SELECTOR_TABLE_KEY, 8),
      retailDirectorSelectorTablePayloadZ64: DIRECTOR_SELECTOR_TABLE_PAYLOAD_Z64,
      retailDirectorSelectorTableSha256: retailDirectorInventory.tableSha256,
      archiveCatalogPath: 'scripts/ob64_archive_catalog.json',
      archiveCatalogSha256: sha256File(options.archiveCatalog),
      audioCatalogPath: 'scripts/ob64_anim_block_catalog.json',
      audioCatalogSha256: sha256File(options.audioCatalog),
      audioCatalogIdentity: '63 contiguous sequenced-audio containers; 15 exact selector-0 opcode-0x6E request values',
      registeredAudioTableRange: 'z64 0x004E3140..0x004F0FB0',
      registeredAudioRequestStatus: 'requests 472, 527, 701, 707, and 708 have exact opcode-0x6E routes; 90, 120, and 180 remain conditional fixed-queue resources',
      recoveredSpriteEffectStatus:
        '14 full-stream and 18 Director Partial opcode-0x46 commands close through 12 Bank 57 physical states and single-child sprite art',
      recoveredActorStatus:
        '135 recovered opcode-0x03 and 17 recovered opcode-0x14 boundaries contain 142 applied actor updates, eight exact control overlaps, one no-record event, and one resolver-invalid event',
      backgroundSelectorStatus:
        'three exact static tables plus stored-state associations; command 0x80000006 seeds new environments, while the native -2 pre-scan sentinel preserves 90 unanimous predecessor Stages without changing Director bytes',
      modeTwoDerivedEnvironmentStatus:
        'the signed and unsigned launch mappers use byte-identical scenario and terrain tables with distinct signedness; all 17 current -1 Director requests remain context-dependent because their event paths do not fix scenario, current unit, or battle terrain',
      modeTwoDerivedEnvironmentTerrainTableSha256:
        MODE_TWO_DERIVED_TERRAIN_TABLE_SHA256,
      modeTwoDerivedEnvironmentScenarioTableSha256:
        MODE_TWO_DERIVED_SCENARIO_TABLE_SHA256,
      oversizedImageClassTableZ64:
        oversizedImagePresentationRules.classTableZ64,
      oversizedImageClassTableSha256:
        oversizedImagePresentationRules.classTableSha256,
      oversizedImageRootResourceKey:
        '0x' + hex(oversizedImagePresentationRules.rootResourceKey, 8),
      oversizedImageRootPayloadZ64:
        oversizedImagePresentationRules.rootPayloadZ64,
      oversizedImageRootPayloadSha256:
        oversizedImagePresentationRules.rootPayloadSha256,
      oversizedImagePresentationStatus:
        'terminal class 4 resolves 106 Director-owned launch rows through the resident class-evolution table and 41-child image root; 11 fallback contexts still require event property 0xE9',
      sceneVignetteStatus:
        '107 terminal-class-4 resources execute one physical opcode 0x3A; 106 have exact launch-owned media and one still requires event property 0xE9; every retail command uses positive equal-axis scale, alpha cap 130, and orientation flags 4 or 8',
      modeTwoStagePlacementResourceKey:
        '0x' + hex(MODE_TWO_STAGE_PLACEMENT_RESOURCE_KEY, 8),
      modeTwoStagePlacementPayloadZ64:
        modeTwoStagePlacementInventory.placementResource.payloadStart,
      modeTwoStagePlacementPayloadSha256:
        modeTwoStagePlacementInventory.placementResource.payloadSha256,
      modeTwoStageDescriptorTableResourceKey:
        '0x' + hex(MODE_TWO_STAGE_DESCRIPTOR_TABLE_KEY, 8),
      modeTwoStageDescriptorTablePayloadZ64:
        modeTwoStagePlacementInventory.descriptorTable.payloadStart,
      modeTwoStageDescriptorTableSha256:
        modeTwoStagePlacementInventory.descriptorTable.payloadSha256,
      modeTwoStagePlacementStatus:
        'both native placement tables contain 80 foreground-selector lists; 165 normal rows resolve through the direct descriptor table, while three special-status rows remain byte-preserved and withheld from rendering',
      rawImageConsumerStatus:
        'archives 517 and 518 have exact native materializers; archives 619 through 622 have selector-stage evidence; archive 645 remains a numeric-only candidate',
      formatFiveBackgroundStatus:
        'archive 102 declares six native records; archives 103 through 107 supply the five external records and form one 607×511 background',
      sectionCAnalysisPath: 'OB64 Decomp/build/huff-section-c/section-c-huff-analysis.json',
      sectionCAnalysisSha256: SECTION_C_ANALYSIS_SHA256,
      sectionCConsumerStatus:
        'resource-4 mode-two environment ownership is exact; other Section C consumers remain unresolved',
      poseVocabularyPath: 'tools/cutscene-workbench/generated/pose-vocabulary.json',
      poseVocabularySha256: sha256File(options.poseVocabulary),
      poseVocabularySchemaVersion: poseVocabulary.schemaVersion,
      poseSelectorsPath:
        'wiki/cutscene-all-bank-pose-rom-map-static-20260711/exports/selectors.jsonl',
      poseSelectorsSha256: sha256File(options.poseSelectors),
      posePhysicalStatesPath:
        'wiki/cutscene-all-bank-pose-rom-map-static-20260711/exports/physical-states.jsonl',
      posePhysicalStatesSha256: sha256File(options.posePhysicalStates),
      posePhysicalStateStatus:
        'all 2,275 selector states have exact decoded ROM bytecode; scene-local publication is provenance, not availability',
      sceneResourcePathRootKey:
        '0x' + hex(sceneResourcePathInventory.resourceKey, 8),
      sceneResourcePathRootPayloadZ64:
        sceneResourcePathInventory.z64PayloadStart,
      sceneResourcePathRootSha256:
        sceneResourcePathInventory.payloadSha256,
      sceneResourcePathStatus:
        'all 59 native groups and 134 entries are catalogued; 121 populated paths use exact decoded points and the native two-pass spline sampler to produce their stored starting headings',
      animatedSceneSpriteRotationStatus:
        'all 2,001 opcode-0x63 commands across the 1,498 retail Director resources and five continuation streams select the direct rotation operand; none select sampled or resource paths',
      partialDirectorAssetsPath:
        'wiki/cutscene-rom-wide-director-asset-census-static-r2-20260712-director-review/exports/director-assets.jsonl',
      partialDirectorAssetsSha256: sha256File(options.partialDirectors),
      directorTailRecoveryPath:
        'tools/cutscene-workbench/generated/director-tail-recovery.json',
      directorTailRecoverySha256: sha256File(options.directorTailRecovery),
      directorTailRecoverySchemaVersion: directorTailRecovery.schemaVersion,
      directorTailRecoveryStatus:
        'historical input retained only for two Director Partial resources and legacy association metadata; full streams use the corrected 153-command corpus',
      directorSelectorTablePath:
        'tools/cutscene-workbench/generated/director-selector-table.json',
      directorSelectorTableSha256: sha256File(options.directorSelectorTable),
      directorSelectorTableSchemaVersion: directorSelectorTable.schemaVersion,
      directorSelectorTableStatus:
        'the native 1,693-row table contains 1,548 populated rows and 1,498 unique resources; the reviewed 60-resource subset occupies 76 rows',
      directorContinuationTableResourceKey:
        '0x' + hex(DIRECTOR_CONTINUATION_TABLE_KEY, 8),
      directorContinuationTableZ64:
        DIRECTOR_RESOURCE_BASE_Z64 + DIRECTOR_CONTINUATION_TABLE_KEY,
      directorContinuationStatus:
        'five exact one-level continuation streams use a terminal word without a top-level class trailer',
      serifuPresentationSelectorsPath:
        'tools/cutscene-workbench/generated/serifu-presentation-selectors.json',
      serifuPresentationSelectorsSha256: sha256File(options.serifuPresentationSelectors),
      serifuPresentationSelectorsSchemaVersion: serifuPresentationSelectors.schemaVersion,
      serifuPresentationSelectorStatus:
        'func_000E5968 selects all 348 Serifu archives and their entry indices; scene-specific load-time writers remain unresolved',
      spriteCorpusPath: 'wiki/sprite-animation-key-resource-research-20260813-v2/corpus-scan.json',
      spriteCorpusSha256: sha256File(options.spriteCorpus),
      spriteCorpusSchemaVersion: spriteCorpus.schemaVersion,
      spriteChildStatus:
        'actor record +0x146 selects child 0..7; each art resource falls back to child 0 when out of range',
      masterRomSha256: manifest.masterRomSha256,
      contentPolicy:
        'metadata-only; the generator reads the retail ROM without mutation and emits hashes, envelopes, selector ownership, grammar templates, and semantic profiles without native command words or pixels'
    },
    scenes,
    presentationScenes,
    partialDirectorResources,
    imageAssets,
    dialogueArchives,
    serifuPresentationSelectors: serifuPresentationSelectors.entries,
    serifuPresentationOwner: serifuPresentationSelectors.owner,
    audioBlocks,
    registeredAudioRequests: REGISTERED_AUDIO_REQUESTS.map((request) => ({
      requestAssetId: 'registered-audio-request:' + request.requestId,
      requestId: request.requestId,
      label: 'Registered audio request ' + request.requestId,
      associationStatus: request.routeStatus,
      semanticName: null,
      previewCapability: 'metadata-only',
      exportCapability: 'needs-research',
      source: {
        indexRecordZ64: request.indexRecordZ64,
        payloadOffset: request.payloadOffset,
        z64Start: request.z64Start,
        z64EndExclusive: request.z64EndExclusive,
        payloadSha256: request.payloadSha256 || null
      }
    })),
    directorGrammar: retailDirectorGrammar,
    directorContinuationStreams,
    directorEvents,
    backgroundSelectorTables,
    parentEventExternalRequests:
      retailDirectorInventory.eventLaunchInventory.externalRequests,
    parentEventTranslationWrites:
      retailDirectorInventory.eventLaunchInventory.translationWrites,
    parentEventSubstitutionSources: EVENT_SUBSTITUTION_SOURCES,
    parentEventSubstitutionSourceWrites:
      retailDirectorInventory.eventLaunchInventory.substitutionSourceWrites,
    modeTwoDerivedEnvironmentRules,
    oversizedImagePresentationRules,
    modeTwoStagePlacementProfiles: modeTwoStagePlacementInventory.profiles,
    sceneResourcePaths: sceneResourcePathInventory.entries,
    actorArtSources: poseCatalog.actorArtSources,
    poseSelectors: compactSelectors,
    posePrograms: poseCatalog.posePrograms
  };
  const json = JSON.stringify(stableCopy(data));
  return [
    '// Generated by tools/generate-cutscene-data.js.',
    '// Metadata only: no decoded pixels and no native command words.',
    'window.OB64 = window.OB64 || {};',
    'window.OB64.cutsceneData = ' + json + ';',
    ''
  ].join('\n');
}

function main() {
  const options = parseArgs(process.argv.slice(2));
  const output = build(options);
  if (options.check) {
    const current = fs.existsSync(options.output) ? fs.readFileSync(options.output, 'utf8') : '';
    if (current !== output) {
      throw new Error(path.relative(process.cwd(), options.output) + ' is stale; regenerate it.');
    }
    process.stdout.write('PASS cutscene-data.js is deterministic and current.\n');
    return;
  }
  fs.writeFileSync(options.output, output);
  process.stdout.write('Wrote ' + path.relative(process.cwd(), options.output) + '.\n');
}

try {
  main();
} catch (error) {
  process.stderr.write((error && error.stack || error) + '\n');
  process.exitCode = 1;
}
