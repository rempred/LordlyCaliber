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
for (const component of after.phase3.components.filter(x=>x.owner==='tool.squad-menu-alignment')) {
  const raw = Buffer.from(component.dataHex,'hex');
  const digest = bytes=>require('crypto').createHash('sha256').update(bytes).digest('hex').toUpperCase();
  assert.strictEqual(component.payloadSha256,digest(raw.subarray(0,component.payloadBytes)));
  assert.strictEqual(component.sha256,digest(raw));
}
assert.strictEqual(after.alignmentCorrection.review,'pending');
assert.deepStrictEqual(after.consumables.acceptedSource,before.consumables.acceptedSource);
assert.strictEqual(after.alignmentCorrection.currentConsumableOwnerImages[0].romSha256,'3F71E57BBAAE9624609969D800D70035E0CED094D37BFA053920B6D1C8609508');
const scratch=fs.mkdtempSync(path.join(os.tmpdir(),'ob64-alignment-test-'));
for (const mutation of [r=>{r.components[0].after.romStart++;},r=>{r.components[1].after.bytes++;},r=>{r.components.push(r.components[0]);},r=>{r.runtime='accepted';}]) {
  const r=JSON.parse(fs.readFileSync(report,'utf8'));mutation(r);const file=path.join(scratch,'bad.json');fs.writeFileSync(file,JSON.stringify(r));assert.throws(()=>generator.applyAlignmentCorrection(baseline(),file));
}
console.log('Alignment correction: preservation and four rejection cases pass.');

// The optional bundle argument exercises the real consumer's historical upgrade
// and mixed-owner rejection without opening a browser or emulator.
if (process.argv[3]) {
  const script = String.raw`
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=process.argv[1], report=JSON.parse(fs.readFileSync(process.argv[2],'utf8')), bundle=process.argv[3];
global.window=global;global.module=undefined;global.localStorage={getItem(){return null;},setItem(){},removeItem(){}};
vm.runInThisContext('var OB64 = window.OB64 = {};');
for(const name of ['data.js','rom-names-data.js','portraits.js','parsers.js','repack.js','description-codec.js','consumable-effects.js','patch.js','tools-data.js','tools.js','native-rebuild-data.js','native-rebuild.js'])vm.runInThisContext(fs.readFileSync(name==='native-rebuild-data.js'?bundle:path.join(root,name),'utf8'));
const bytes=new Uint8Array(fs.readFileSync(report.products.phase5aCanonicalNativeConsumables.path));
assert.strictEqual(OB64.nativeRebuild._test.detectPhase3Owner(bytes,'squad-menu-alignment').state,'native');
const provenance=OB64.nativeRebuild._test.classifyConsumableOwner(bytes).provenance;
assert.strictEqual(provenance.source,'alignment-v10-source-product');assert.strictEqual(provenance.lifecycle,'source-regenerated-unreviewed');
const old=bytes.slice();for(const x of report.components)old.set(Buffer.from(x.before.dataHex,'hex'),x.before.romStart);
const detected=OB64.nativeRebuild._test.detectPhase3Owner(old,'squad-menu-alignment');assert.strictEqual(detected.state,'native');assert.strictEqual(detected.updateRequired,true);
const mixed=bytes.slice(),loader=report.components.find(x=>x.before.name==='source-loader').before;mixed.set(Buffer.from(loader.dataHex,'hex'),loader.romStart);assert.strictEqual(OB64.nativeRebuild._test.detectPhase3Owner(mixed,'squad-menu-alignment').state,'mixed');
console.log('Current source provenance, v9 upgrade detection, and mixed-version rejection pass.');
`;
  cp.execFileSync(process.execPath,['-e',script,root,path.resolve(report),path.resolve(process.argv[3])],{stdio:'inherit'});
}
