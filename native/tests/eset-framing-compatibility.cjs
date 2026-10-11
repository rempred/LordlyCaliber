'use strict';
// Check native ESET framing with the independently implemented browser codec.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
if(process.argv.includes('--require-rom')&&!process.env.LORDLY_TEST_ROM) {
  console.log('Private ROM was not supplied for the native ESET corpus check.');process.exit(77);
}
const context={Uint8Array};context.window=context;vm.createContext(context);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../scenario-eset-codec.js'),'utf8'),context);
const codec=context.OB64.scenarioCodec,fixtures=JSON.parse(fs.readFileSync(process.argv[2]));
for(const f of fixtures)for(const kind of ['raw','edited']) {
  const bytes=Buffer.from(f[kind],'hex'),model=codec.parseEset(bytes);
  assert.deepEqual(Buffer.from(codec.serializeEset(model)),bytes,f.name+' '+kind);
}
console.log(`PASS independent ESET codec: ${fixtures.length} original scenarios and ${fixtures.length} edited framing cases, including deployment/node growth and the shared final-row tail. Gameplay semantics and resource selection are outside this check.`);
