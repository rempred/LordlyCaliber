// Generates labels only. No ROM contents or browser execution code enter the product.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '../..');
const context = { OB64: {} };
context.window = context;
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root, 'data.js'), 'utf8'), context);
const catalog = {};
catalog.CONSUMABLE_NAMES = context.OB64.SAVE.CONSUMABLE_NAMES;
for (const [key, value] of Object.entries(context.OB64)) {
  if (typeof value !== 'function' && key !== 'SAVE' && key !== 'SAVERAM_MAIN_CODEC') catalog[key] = value;
}
vm.runInContext(fs.readFileSync(path.join(root, 'rom-names-data.js'), 'utf8'), context);
catalog.ACTION_NAMES = Object.fromEntries(Object.entries(context.OB64.ACTION_NAMES).map(([id, value]) => [id, context.OB64.romNameText(value)]));
catalog.ACTION_NAMES['0'] = '(none)';
catalog.ACTION_NAMES['255'] = 'Use secondary class';
fs.mkdirSync(path.join(root, 'native/assets'), { recursive: true });
fs.writeFileSync(path.join(root, 'native/assets/catalog.json'), JSON.stringify(catalog, null, 2) + '\n');
console.log('Generated native label catalog from data.js and rom-names-data.js');
