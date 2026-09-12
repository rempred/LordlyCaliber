'use strict';
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const vm = require('vm');
const cp = require('child_process');
const generator = require('../tools/generate-native-rebuild-data');
const report = process.argv[2];
assert(report, 'Pass the current source-linked correction.json');
const root = path.resolve(__dirname, '..');
const source = cp.execFileSync('git', ['-C',root,'show','aaeffde75b394dc2c41162933299d099c317d792:native-rebuild-data.js'], {encoding:'utf8',maxBuffer:1024*1024});
function baseline() { const c={};vm.runInNewContext(source,c);return JSON.parse(JSON.stringify(c.OB64_NATIVE_REBUILD_DATA.payload)); }
const before=baseline(), after=generator.applyAlignmentCorrection(baseline(),report);
assert.deepStrictEqual(after.approvals,before.approvals);
assert.deepStrictEqual(after.composition,before.composition);
assert.deepStrictEqual(after.experience,before.experience);
assert.deepStrictEqual(after.shops.components,before.shops.components);
assert.deepStrictEqual(after.shops.contract,before.shops.contract);
assert.deepStrictEqual(after.phase3.components.filter(x=>x.owner!=='tool.squad-menu-alignment'),before.phase3.components.filter(x=>x.owner!=='tool.squad-menu-alignment'));
assert.strictEqual(after.phase3.components.find(x=>x.owner==='tool.squad-menu-alignment'&&x.name==='source-module').payloadBytes,3328);
assert.strictEqual(after.alignmentCorrection.review,'pending');
assert.strictEqual(after.consumables.acceptedSource.products.canonicalNativeConsumables,'3F71E57BBAAE9624609969D800D70035E0CED094D37BFA053920B6D1C8609508');
const scratch=fs.mkdtempSync(path.join(os.tmpdir(),'ob64-alignment-test-'));
for (const mutation of [r=>{r.components[0].after.romStart++;},r=>{r.components[1].after.bytes++;},r=>{r.components.push(r.components[0]);},r=>{r.runtime='accepted';}]) {
  const r=JSON.parse(fs.readFileSync(report,'utf8'));mutation(r);const file=path.join(scratch,'bad.json');fs.writeFileSync(file,JSON.stringify(r));assert.throws(()=>generator.applyAlignmentCorrection(baseline(),file));
}
console.log('Alignment correction: preservation and four rejection cases pass.');
