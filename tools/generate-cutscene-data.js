#!/usr/bin/env node
'use strict';

// Build the metadata-only Cutscene Studio catalog from accepted parent-side
// products. This tool never reads or embeds the master ROM and never embeds
// decoded pixels or native command words.

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const editorRoot = path.resolve(__dirname, '..');
const workspaceRoot = path.resolve(editorRoot, '..');
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

// These tables are byte-exact static resource selectors. Director opcode
// 0x80000006 uses the scene table only when the runtime Director mode is not 2.
// In mode 2, the command operand is ignored and a separate runtime byte selects
// the battle table. Stored-state observations below close the mode for a bounded
// set of assets. Every other row remains a conditional route.
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

// Stored Project64 states close the runtime selector for these scene loads.
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
    mode2SelectorByte: 0,
    scalarSelectorMirror: 1,
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
    mode2SelectorByte: 57,
    scalarSelectorMirror: 0x39,
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
    mode2SelectorByte: 51,
    scalarSelectorMirror: 0x33,
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
    mode2SelectorByte: 51,
    scalarSelectorMirror: 1,
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
    mode2SelectorByte: 57,
    scalarSelectorMirror: 1,
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
    mode2SelectorByte: 61,
    scalarSelectorMirror: 0x3D,
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
    mode2SelectorByte: 60,
    scalarSelectorMirror: 0x3C,
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
    mode2SelectorByte: 60,
    scalarSelectorMirror: 1,
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
    mode2SelectorByte: 62,
    scalarSelectorMirror: 0x3E,
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
    mode2SelectorByte: 62,
    scalarSelectorMirror: 0,
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
    mode2SelectorByte: 51,
    scalarSelectorMirror: 1,
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
    mode2SelectorByte: 51,
    scalarSelectorMirror: 1,
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
    mode2SelectorByte: 51,
    scalarSelectorMirror: 0,
    selectorTableId: 'background-table:mode2-environment:80',
    candidateGroupIds: Object.freeze([]),
    exactAssetIds: Object.freeze([]),
    candidateAssetIds: Object.freeze([]),
    associationStatus: 'the Director Partial prefix has no accepted background request, and the stored scalar mirror does not show selector consumption'
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
const NORMALIZED_US_REV0_Z64_SHA256 =
  '571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A';

function usage() {
  return [
    'Usage: node tools/generate-cutscene-data.js [options]',
    '',
    'Options:',
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

function backgroundRequestsForAsset(asset, runtimeObservation) {
  return (asset.nodes || []).filter((node) =>
    String(node.opcode).toUpperCase() === '0X80000006').map((node) => {
    const operand = Array.isArray(node.operands) && Number.isInteger(node.operands[0])
      ? node.operands[0] : null;
    const inSceneTable = Number.isInteger(operand) && operand >= 0 &&
      operand < SCENE_BACKGROUND_GROUP_KEYS.length;
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
      mode2Route: {
        condition: 'mode byte equals 2',
        commandOperandDisposition: 'ignored by the selected handler branch',
        selectorSource: 'environment byte 0x80196AED; the overlay selector is stored independently at 0x801CEAB0',
        selectorTableId: 'background-table:mode2-environment:80',
        overlaySelectorTableId: 'background-table:mode2-overlay:80',
        associationStatus: 'runtime environment and overlay selector values unresolved'
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

function directorLaunchMode(asset, runtimeObservation, canonicalNodes) {
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
  if (modeZeroMarkers.length && modeTwoMarkers.length) {
    throw new Error(asset.assetId + ' contains conflicting Director launch-mode markers.');
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
  return {
    value: null,
    evidenceStatus: 'external-unresolved',
    source: 'launch caller outside the Director stream',
    status: 'This stream has no mode-specific command that identifies its launch mode.'
  };
}

function modeTwoBackgroundMirrorEvidence(assets) {
  const rows = [];
  (assets || []).forEach((asset) => {
    const observation = DIRECTOR_BACKGROUND_RUNTIME_OBSERVATIONS[asset.assetId] || null;
    if (!observation || observation.directorMode !== 2 ||
        !Number.isInteger(observation.mode2SelectorByte)) return;
    backgroundRequestsForAsset(asset, observation).forEach((request) => {
      if (!Number.isInteger(request.commandOperand)) return;
      rows.push({
        assetId: asset.assetId,
        commandOperand: request.commandOperand,
        runtimeSelector: observation.mode2SelectorByte,
        matches: request.commandOperand === observation.mode2SelectorByte
      });
    });
  });
  return {
    observedRequestCount: rows.length,
    matchingRequestCount: rows.filter((row) => row.matches).length,
    allObservedRequestsMatch: rows.length > 0 && rows.every((row) => row.matches),
    rows
  };
}

function launchCameraProjection(values, calibrationStatus, calibrationResult) {
  return {
    mode: 'native-perspective-profile',
    coordinateSpace: 'Director fixed-point coordinates divided by 1000; native Actor model scale is 0.1',
    screenWidth: 320,
    screenHeight: 240,
    modelScale: 0.1,
    fovYDegrees: values.fovYDegrees,
    aspect: 4 / 3,
    near: 1,
    far: 5000,
    eye: { ...values.eye },
    target: { ...values.target },
    up: { x: 0, y: 1, z: 0 },
    calibrationStatus,
    calibrationResult
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
      kind: 'mode-two-corpus-family',
      evidenceStatus: 'corpus-family-inferred',
      status: 'Five stored non-title mode-two scenes share this complete Actor camera bank.',
      basisCaptureCount: 5,
      projection: launchCameraProjection(
        COMMON_MODE_TWO_ACTOR_CAMERA,
        'five-capture mode-two family',
        'Five stored non-title mode-two scenes contain identical Actor camera values.'
      )
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
    const selectedOverlay = Number.isInteger(overlaySelector) ? overlaySelector : selector;
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
        selectedOverlay >= 0 && selectedOverlay < MODE_TWO_OVERLAY_RESOURCE_KEYS.length
          ? MODE_TWO_OVERLAY_RESOURCE_KEYS[selectedOverlay] || null : null,
      overlayAssetIds: archiveAssetIds(BATTLE_BACKGROUND_ARCHIVE_ROWS[selectedOverlay] || []),
      stageLayers
    };
  }
  return { members: [], assetIds: [], resourceKey: null, stageLayers: [] };
}

function launchBackgroundProfiles(mode, requests, runtimeObservation, mirrorEvidence) {
  const mirrorUsable = mirrorEvidence.observedRequestCount >= 3 &&
    mirrorEvidence.allObservedRequestsMatch;
  return requests.map((request) => {
    let selectorTableId = null;
    let selector = null;
    let selectorSource = 'external-unresolved';
    let evidenceStatus = 'external-unresolved';
    let status = 'The active background route depends on launch state outside this stream.';
    if (mode.value === 0) {
      selectorTableId = 'background-table:scene:31';
      selector = request.commandOperand;
      selectorSource = 'director-command-operand';
      evidenceStatus = mode.evidenceStatus === 'runtime-observed'
        ? 'runtime-observed-mode-native-command' : 'stream-structural-mode-native-command';
      status = 'The active non-mode-two handler uses the Director command operand.';
    } else if (mode.value === 2 && runtimeObservation &&
        Number.isInteger(runtimeObservation.mode2SelectorByte)) {
      selectorTableId = 'background-table:mode2-environment:80';
      selector = runtimeObservation.mode2SelectorByte;
      selectorSource = 'runtime-observed-external-byte';
      evidenceStatus = 'runtime-observed';
      status = 'Stored scene state supplies the external mode-two background selector.';
    } else if (mode.value === 2 && mirrorUsable &&
        Number.isInteger(request.commandOperand)) {
      selectorTableId = 'background-table:mode2-environment:80';
      selector = request.commandOperand;
      selectorSource = 'corpus-coordinated-command-mirror';
      evidenceStatus = 'corpus-pattern-inferred';
      status = mirrorEvidence.matchingRequestCount + ' of ' +
        mirrorEvidence.observedRequestCount +
        ' stored mode-two requests mirror the external selector in this operand.';
    }
    const route = launchBackgroundRoute(selectorTableId, selector);
    const observedStageLayers = runtimeObservation &&
      Array.isArray(runtimeObservation.stageLayers)
      ? runtimeObservation.stageLayers.map((layer) => ({ ...layer })) : [];
    const stageLayers = observedStageLayers.length
      ? observedStageLayers
      : (mode.value === 2 ? route.stageLayers : []);
    return {
      requestId: request.requestId,
      wordStart: request.wordStart,
      commandOperand: request.commandOperand,
      selectorTableId,
      selector,
      selectorSource,
      evidenceStatus,
      status,
      resourceKey: route.resourceKey,
      members: route.members,
      assetIds: route.assetIds,
      stageLayers,
      stageAssetIds: stageLayers.length
        ? stageLayers.map((layer) => layer.assetId)
        : (mode.value === 0 ? route.assetIds.slice() : [])
    };
  });
}

function launchRosterProfile(asset, actors, canonicalNodes) {
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
    evidenceStatus: materializers.length || externalTemplateSlots.length
      ? 'stream-context-roster' : 'same-stream-records',
    status: materializers.length
      ? 'Roster commands materialize launch-time records represented by the scene templates.'
      : (externalTemplateSlots.length
        ? 'Some Actor commands consume launch-time records represented by the scene templates.'
        : 'Every catalogued Actor template has a same-stream record producer.')
  };
}

function buildLaunchProfile(asset, actors, backgroundObservation,
    actorCameraObservation, backgroundRequests, mirrorEvidence, canonicalNodes) {
  const directorMode = directorLaunchMode(asset, backgroundObservation, canonicalNodes);
  return {
    profileId: 'launch-profile:' + asset.assetId,
    directorMode,
    cameras: launchCameraProfiles(directorMode, actorCameraObservation),
    background: {
      requestCount: backgroundRequests.length,
      requests: launchBackgroundProfiles(
        directorMode, backgroundRequests, backgroundObservation, mirrorEvidence)
    },
    roster: launchRosterProfile(asset, actors, canonicalNodes)
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
    selectors, selectorOwner, mirrorEvidence) {
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
  const backgroundRequests = backgroundRequestsForAsset(asset, backgroundObservation);
  const launchProfile = buildLaunchProfile(asset, actors, backgroundObservation,
    actorCameraObservation, backgroundRequests, mirrorEvidence, canonicalNodes);
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
        ? 'The stream background value matches a registered complete scene and its foreground piece; launch state still supplies the active scene in-game.'
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

function compactDirectorEvents(scenes, grammar) {
  const occurrenceMap = {};
  scenes.forEach((scene) => {
    scene.source.nodes.forEach((node) => {
      const opcode = String(node.opcode || '').toUpperCase();
      if (!occurrenceMap[opcode]) occurrenceMap[opcode] = { count: 0, scenes: new Set() };
      occurrenceMap[opcode].count += 1;
      occurrenceMap[opcode].scenes.add(scene.sceneId);
    });
  });
  return grammar.map((definition) => {
    const opcode = '0x' + Number(definition.opcode_u32).toString(16).toUpperCase();
    const occurrence = occurrenceMap[definition.opcode.toUpperCase()] ||
      occurrenceMap[opcode.toUpperCase()] || { count: 0, scenes: new Set() };
    return {
      eventId: 'director-opcode:' + Number(definition.opcode_u32)
        .toString(16).toUpperCase().padStart(8, '0'),
      opcode,
      family: directorSemanticFamily(definition.name),
      label: semanticLabel(definition.name),
      semanticName: definition.name,
      semanticSummary: definition.semantic_summary,
      confidence: definition.confidence,
      widthKind: definition.width_kind,
      sourceWordSpan: definition.source_word_span,
      capability: 'preview-only',
      unresolvedJoin: null,
      occurrenceCount: occurrence.count,
      sceneCount: occurrence.scenes.size,
      associationStatus: occurrence.count ? 'observed-in-corpus' : 'defined-but-unobserved'
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
    const stageLayers = modeTwoStageLayers(selector, selector).map((layer) => ({ ...layer }));
    return {
      selector,
      resourceKey: resourceKey || null,
      assetId,
      archiveAssetIds: stageLayers.map((layer) => layer.assetId),
      stageLayers,
      stageAssetIds: stageLayers.map((layer) => layer.assetId),
      associationStatus: assetId
        ? 'exact resource-4 environment base; same-index overlay shown separately'
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
      selectionCondition: 'Director opcode 0x80000006 with runtime mode not equal to 2',
      selectorSource: 'command operand',
      tableResourceKey: 0x016B3D18,
      tableEntryZ64: 0x01C47F98,
      entryCount: sceneEntries.length,
      entries: sceneEntries
    },
    {
      tableId: 'background-table:mode2-environment:80',
      label: 'Mode-2 environment bases',
      owner: 'func_00067320 and func_00067B48',
      selectionCondition: 'mode-2 initialization loads the complete environment independently of Director overlays',
      selectorSource: 'runtime byte at RAM 0x80196AED',
      tableResourceKey: 0x00000004,
      tableSizeWordZ64: 0x00594284,
      tableEntryZ64: 0x00594288,
      entryCount: environmentEntries.length,
      entries: environmentEntries
    },
    {
      tableId: 'background-table:mode2-overlay:80',
      label: 'Mode-2 foreground and occlusion resources',
      owner: 'func_001E4C74 and func_001FB32C',
      selectionCondition: 'the mode-2 stage builder selects overlays independently of the environment loader',
      selectorSource: 'runtime scalar at RAM 0x801CEAB0',
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
  for (let ordinal = 0; ordinal < recordCount; ordinal++) {
    const opcode = bytes[cursor];
    const width = POSE_OPCODE_WIDTHS[opcode];
    if (!width || cursor + width > bytes.length) {
      throw new Error(program.id + ' has an invalid pose opcode at record ' + ordinal + '.');
    }
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
  return { frames, controlOpcodes };
}

function compactPoseProgram(program) {
  const decoded = canonicalPoseRecords(program);
  const frames = decoded.frames;
  const controlOpcodes = decoded.controlOpcodes;
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
  const directorSelectorOwners = new Map(directorSelectorTable.assets.map((row) => [
    row.assetId,
    {
      tableResourceKey: directorSelectorTable.owner.resourceKey,
      tablePayloadZ64: directorSelectorTable.owner.payloadZ64,
      selectorRows: row.selectorRows,
      selectorWordZ64: row.selectorWordZ64
    }
  ]));
  const recoveredAssets = manifest.assets.map((asset) =>
    applyDirectorTailRecovery(asset, tailRecoveryMap.get(asset.assetId)));
  const backgroundMirrorEvidence = modeTwoBackgroundMirrorEvidence(recoveredAssets);
  const scenes = recoveredAssets.map((asset) => {
    const selectorOwner = directorSelectorOwners.get(asset.assetId);
    if (!selectorOwner) throw new Error('Director selector owner is missing for ' + asset.assetId + '.');
    return compactScene(asset, corpusAssetMap.get(asset.assetId),
      corpusNodesByAsset.get(asset.assetId), corpusWaitsByAsset.get(asset.assetId),
      selectors, selectorOwner, backgroundMirrorEvidence);
  })
    .sort((left, right) => left.source.z64PayloadStart - right.source.z64PayloadStart);
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
  const directorEvents = compactDirectorEvents(scenes, directorCorpusGrammar);
  const backgroundSelectorTables = compactBackgroundSelectorTables();
  const spriteArtResources = spriteCorpus.cutscene.artResources;
  const distinctSpriteChildren = spriteArtResources.reduce((total, resource) =>
    total + resource.childCount, 0);
  const catalogPoseIds = new Set(poseCatalog.posePrograms.map((program) => program.poseId));
  const renderablePoseIds = new Set(poseCatalog.posePrograms
    .filter((program) => program.frames.length).map((program) => program.poseId));
  const data = {
    format: 'ob64-cutscene-catalog',
    schemaVersion: 4,
    sourceRevision: 'us-rev0',
    counts: {
      scenes: scenes.length,
      presentationScenes: presentationScenes.length,
      partialDirectorResources: partialDirectorResources.length,
      runtimeTiledDirectorResources: directorCorpusAssets.length,
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
        !selector.sourceProgramDefined).length
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
        'two exact static tables plus stored-state associations; selectors 21 and 22 load complete ordered groups, selectors 57 and 62 select exact archives, and unobserved assets retain unresolved runtime selection',
      rawImageConsumerStatus:
        'archives 517 and 518 have exact native materializers; archives 619 through 622 have selector-stage evidence; archive 645 remains a numeric-only candidate',
      formatFiveBackgroundStatus:
        'archive 102 declares six native records; archives 103 through 107 supply the five external records and form one 607×511 background',
      sectionCAnalysisPath: 'OB64 Decomp/build/huff-section-c/section-c-huff-analysis.json',
      sectionCAnalysisSha256: SECTION_C_ANALYSIS_SHA256,
      sectionCConsumerStatus: 'unresolved',
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
        'all 60 accepted Director keys occupy 76 exact owner rows in the 1,693-row table',
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
        'metadata-only; semantic names, operand roles, and boundaries are included without decoded pixels or native command words'
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
    directorEvents,
    backgroundSelectorTables,
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
