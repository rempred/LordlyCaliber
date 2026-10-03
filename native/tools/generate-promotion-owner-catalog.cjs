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
fs.writeFileSync(path.join(root, 'native/game/promotion/owners.json'), JSON.stringify(catalog, null, 2) + '\n');
