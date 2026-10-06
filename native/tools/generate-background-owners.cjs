'use strict';
// Store identities of already-qualified launch owners, never their game-code bytes.
const fs = require('node:fs'), path = require('node:path'), vm = require('node:vm'),
      crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..'), context = {window: {OB64: {}}};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root, 'cutscene-rom-start-data.js'), 'utf8'), context);
const names = new Set(['func_0004ED60', 'func_00283E14', 'func_002827EC',
                       'callerRouteTable', 'terminalClassTable']);
const owners = context.window.OB64.cutsceneRomStartData.contracts.filter(r => names.has(r.name)).map(r => ({
    name: r.name, rom: r.rom, size: r.hex.length / 2,
    sha256: crypto.createHash('sha256').update(Buffer.from(r.hex, 'hex')).digest('hex')
}));
if (owners.length !== names.size) throw new Error('Missing launch owner');
fs.writeFileSync(path.join(root, 'native/game/director/background-owners.json'), JSON.stringify({
    generator: 'native/tools/generate-background-owners.cjs',
    input: 'cutscene-rom-start-data.js qualified original launch owners', owners
}, null, 2) + '\n');
console.log(`Recorded ${owners.length} background launch owner identities.`);
