// Developer-only migration of validated function extents and hashes. No ROM bytes
// enter the output. Native exports compile maintained sources using the user's ROM.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const sandbox = {window: {}};
vm.runInNewContext(fs.readFileSync(path.join(root, 'native-classes-data.js'), 'utf8'), sandbox);
const data = sandbox.window.OB64.NATIVE_CLASSES_DATA.payload;
const sha = hex => crypto.createHash('sha256').update(Buffer.from(hex, 'hex')).digest('hex');
const catalog = {
  generator: 'native/tools/generate-promotion-owner-catalog.cjs',
  input: 'native-classes-data.js (validated Rebuild baseline 559b231); C menu verified by native/tests/class_menu_mips.cjs',
  romStart: data.families.romStart,
  romSize: data.families.romSize,
  resourceKey: data.families.resourceKey,
  owners: data.families.owners.map(owner => ({
    name: path.basename(owner.source, '.s'), start: owner.start, size: owner.size,
    retailSha256: sha(owner.retailHex), familySha256: sha(owner.familyHex),
    // Padded complete owner from the pinned compiler. The MIPS comparison test
    // independently checks this hash against the maintained C and configuration.
    cSource: owner.start === 0xbf248 ? 'family-menu.c' : undefined,
    cSourceSha256: owner.start === 0xbf248 ? 'ec2b6c857dcbc793d9f5187c2b8f1ac06280c391ace34a3c786e6b8dc0417f42' : undefined,
    directGateSha256: data.promotion.loaders.find(loader => loader.start === owner.start)?.relocatedSha256.toLowerCase(),
    keySite: data.promotion.loaders.find(loader => loader.start === owner.start)?.keySite
  }))
};
// Complete US Rev 0 owner, previously unchanged by family exports. Hashes only;
// the comparison test reads the user's optional private ROM for original code.
catalog.owners.push({
  name:'func_000c91a0', start:0xc91a0, size:0x184,
  retailSha256:'0ba40366cf40546ea2004f3e0bb90ee5c881e49bde239da2313ddbabed84277a',
  familySha256:'0ba40366cf40546ea2004f3e0bb90ee5c881e49bde239da2313ddbabed84277a',
  cSource:'class-selection.c',
  cSourceSha256:'95b1d89159f2d4832d25984e27595cf6b508d8edc12df2c89033f5aaaa89ca0a',
  symbols:{lc_army_owner:0x80196AF8, lc_input_buttons:0x800E8100,
    lc_repeat_buttons:0x800E8700, lc_menu_sound:0x801D8A60,
    lc_menu_update:0x801CFC24, lc_menu_equipment:0x801D2B7C}
});
catalog.owners.push({
  name:'func_000c3dac', start:0xc3dac, size:0x1a4,
  retailSha256:'9b5699e4d982c6bd35760915925d9c575bc9d22cb4a3cdbbb8f8264fd30417cb',
  familySha256:'9b5699e4d982c6bd35760915925d9c575bc9d22cb4a3cdbbb8f8264fd30417cb',
  cSource:'class-discovery.c',
  cSourceSha256:'3dfdb788788a84869c42d87793d972af83cf05648095dc94389d9daff6acedf1',
  symbols:{lc_roster:0x80193BF8,lc_named_class_map:0x801EF3B8,lc_class_eligible:0x801D6EA0}
});
catalog.owners.push({
  name:'func_000a4a00', start:0xa4a00, size:0x27c8,
  retailSha256:'a65a57c3b4641ef6f8c7e9190e55684039a16dd9f0595c39350b6660b36e31c2',
  familySha256:'a65a57c3b4641ef6f8c7e9190e55684039a16dd9f0595c39350b6660b36e31c2',
  cFragment:'class-render-rows.c', fragmentEntry:'class-render-entry.s',
  fragmentStart:0xa4d5c, fragmentEnd:0xa4ed8,
  styleSource:'class-render-style.s', styleStart:0xa5668, styleSize:76,
  cSourceSha256:'49532166880bb6aff99023a64852baf01bd28d1073121df1f3fad637f5b94798',
  symbols:{lc_army_owner:0x80196AF8,lc_class_coordinates:0x801C6B28,lc_class_matrix:0x80098C20}
});
catalog.owners.push({
  name:'func_000bbd80', start:0xbbd80, size:44,
  requiresFamilyResource:false, onMaskEdit:true,
  retailSha256:'d3afceb458105688a1c504d15d83489fb7e5b33dfed380ff8b17106f5a51f2ef',
  familySha256:'d3afceb458105688a1c504d15d83489fb7e5b33dfed380ff8b17106f5a51f2ef',
  cSource:'compatibility-mask.c',
  cSourceSha256:'d3afceb458105688a1c504d15d83489fb7e5b33dfed380ff8b17106f5a51f2ef'
});
catalog.owners.push({
  name:'func_000bf5dc', start:0xbf5dc, size:332,
  retailSha256:'5dc1d784f93d857fd73877b9a9908d8766ce6e15a863ddaf1fb886dfbdc5cbd6',
  familySha256:'5dc1d784f93d857fd73877b9a9908d8766ce6e15a863ddaf1fb886dfbdc5cbd6',
  cSource:'equipment-class-menu.c',
  cSourceSha256:'596f62bbad0375119955a0524b6baabf613c67b05142728bc6aefd742e8d4ed1',
  requiresFamilyResource:false, onFlagsEdit:true,
  symbols:{lc_army_owner:0x80196AF8,lc_goth:0x80196A6C,lc_equipment_summary:0x801D29F8,
    lc_equipment_cost:0x801D2EE0,lc_equipment_full:0x801D2E2C}
});
catalog.owners.push({
  name:'func_000bc0bc', start:0xbc0bc, size:412,
  retailSha256:'b797279fc0a015a591c5a75a3eaa843c7d3554e94a72241b47f3ff079e7f72f0',
  familySha256:'b797279fc0a015a591c5a75a3eaa843c7d3554e94a72241b47f3ff079e7f72f0',
  cSource:'class-categories.c',
  cSourceSha256:'d70c4e823eb4dffe9067061244528dcb01d67c8653f321f232b390e35338196f',
  requiresFamilyResource:false, onMaskEdit:true,
  symbols:{lc_army_owner:0x80196AF8}
});
catalog.owners.push({
  name:'func_000bbdac', start:0xbbdac, size:784,
  retailSha256:'59604003e43aefac3d1ff4efc31de5d56902b3635fc8c7bda9f228406e4dfed3',
  familySha256:'59604003e43aefac3d1ff4efc31de5d56902b3635fc8c7bda9f228406e4dfed3',
  cSource:'category-class-counts.c',
  cSourceSha256:'dc2acf0669eecdb8517188318dbd35ca40cc31cd5abf279cec19e6852c183378',
  cReferences:[{high:668,low:676,group:'classes',addend:141}],
  requiresFamilyResource:false, onMaskEdit:true,
  symbols:{lc_army_owner:0x80196AF8,lc_roster:0x80193BF8,lc_named_class_map:0x801EF3B8,
    lc_class_records:0x80187C14,lc_clear:0x80093380,lc_check_change:0x801D2358}
});
catalog.owners.find(owner=>owner.start===0xcd3e8).assemblyFragments = [
  {offset:0xcd638-0xcd3e8,size:8,source:'equipment-first-class.s'}
];
Object.assign(catalog.owners.find(owner=>owner.start===0xd0150), {
  cSource:'class-screen.c', cExtent:560,
  cSourceSha256:'e0b97def94deb8b58d02286b96ce05b876c3f23e8c3c6d46ce96a1066f11644e',
  cReferences:[{high:208,low:216,group:'classes',addend:141}],
  symbols:{lc_army_owner:0x80196AF8,lc_roster:0x80193BF8,lc_named_class_map:0x801EF3B8,
    lc_class_records:0x80187C14,lc_prepare_class:0x801E2BCC,lc_prepare_actor:0x801DA3D8,
    lc_family_menu:0x801D27E8,lc_equipment_summary:0x801D29F8,
    lc_equipment_entry:0x801CEA1C,lc_family_lookup:0x801CF2F0}
});
fs.writeFileSync(path.join(root, 'native/game/promotion/owners.json'), JSON.stringify(catalog, null, 2) + '\n');
