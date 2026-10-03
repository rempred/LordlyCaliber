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
  input: 'native-classes-data.js (validated Rebuild baseline 559b231)',
  romStart: data.families.romStart,
  romSize: data.families.romSize,
  resourceKey: data.families.resourceKey,
  owners: data.families.owners.map(owner => ({
    name: path.basename(owner.source, '.s'), start: owner.start, size: owner.size,
    retailSha256: sha(owner.retailHex), familySha256: sha(owner.familyHex),
    directGateSha256: data.promotion.loaders.find(loader => loader.start === owner.start)?.relocatedSha256.toLowerCase(),
    keySite: data.promotion.loaders.find(loader => loader.start === owner.start)?.keySite
  }))
};
fs.writeFileSync(path.join(root, 'native/game/promotion/owners.json'), JSON.stringify(catalog, null, 2) + '\n');
