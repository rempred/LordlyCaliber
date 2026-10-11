'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
const {EDITOR,PARENT,MASTER_NAME}=require('./project-paths.cjs');
global.window=global;global.module=undefined;global.localStorage={getItem(){return null;}};
vm.runInThisContext('var OB64 = window.OB64 = {};');
for(const name of ['data.js','rom-names-data.js','portraits.js','parsers.js','repack.js',
  'description-codec.js','source-redirect.js','stat-gate-relocation.js','consumable-effects.js',
  'tools-data.js','tools.js','native-rebuild-data.js','native-rebuild.js','native-classes-data.js','native-classes.js'])
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR,name),'utf8'),{filename:name});
const master=new Uint8Array(fs.readFileSync(path.join(PARENT,MASTER_NAME)));
const fresh=()=>OB64.loadROM(master.slice().buffer);
const equal=(a,b)=>assert.deepStrictEqual(Buffer.from(a),Buffer.from(b));
let passed=0;
function check(name,action){action();console.log('PASS '+name);passed++;}
function apply(rom,dirty){const candidate={z64:rom.z64.slice(),layout:rom.layout};
  const plan=OB64.nativeClasses.prepare(rom,candidate.z64,dirty);
  const old=OB64.statGateRelocation.currentRequests(rom.statGates);
  if(plan.promotion&&old.length)OB64.sourceRedirect.apply(candidate.z64,OB64.sourceRedirect.prepare(candidate.z64,[],{knownCurrentRequests:old}));
  OB64.nativeClasses.apply(candidate,plan);OB64.nativeClasses.validateApplied(candidate,plan);return {candidate,plan};}
check('unmodified complete record build is byte-identical',()=>{const rom=fresh();const spec=OB64.nativeClasses.verifyData().records;
  equal(OB64.nativeClasses.buildRecords(rom.classDefs,rom.z64),rom.z64.subarray(spec.start,spec.start+spec.size));});
check('all 72 editable record bytes reach the correct physical bytes and reopen',()=>{
  const rom=fresh(),spec=OB64.nativeClasses.verifyData().records,record=rom.classDefs[3];
  for(const field of spec.fields){const parts=field.path.split('.'),last=parts.pop();const target=parts.reduce((v,k)=>v[k],record);
    target[last]=field.width===2?0x1234:123;}
  const {candidate}=apply(rom,{classDefs:true});const parsed=OB64.parseClassDefs(candidate.z64)[3];
  for(const field of spec.fields){const expected=field.width===2?0x1234:123;
    assert.strictEqual(field.path.split('.').reduce((v,k)=>v[k],parsed),expected,field.path);}
  const start=spec.start+3*72-12,end=start+72;equal(candidate.z64.subarray(0,start),rom.z64.subarray(0,start));
  equal(candidate.z64.subarray(end),rom.z64.subarray(end));});
check('invalid and missing records reject before writing',()=>{const rom=fresh();rom.classDefs[3].alignment=256;
  assert.throws(()=>apply(rom,{classDefs:true}),/outside/);rom.classDefs[3]=null;
  assert.throws(()=>apply(rom,{classDefs:true}),/missing/);});
check('class and action text are built as whole resources and reopened',()=>{const rom=fresh();
  rom.classDescriptions=OB64.descriptionCodec.withText(rom.classDescriptions,3,'A rebuilt class.');
  rom.actionDescriptions=OB64.descriptionCodec.withText(rom.actionDescriptions,3,'Type: Physical.\nA rebuilt action.');
  const {candidate}=apply(rom,{classDescriptions:true,actionDescriptions:true});
  assert.strictEqual(OB64.descriptionCodec.parseBlock('classes',candidate.z64).records[3].editableText,'A rebuilt class.');
  assert.strictEqual(OB64.descriptionCodec.parseBlock('actions',candidate.z64).records[3].editableText,'Type: Physical.\nA rebuilt action.');});
let promoted;
check('maximum-entropy promotion uses native references with no PI hook',()=>{const rom=fresh();let n=0x12345678;
  for(const record of Object.values(rom.statGates.byClass))for(const key of ['str','vit','int','men','agi','dex','alnMin','alnMax']){
    n=(Math.imul(n,1664525)+1013904223)>>>0;record[key]=n>>>24;}
  promoted=apply(rom,{statGates:true});assert.strictEqual(promoted.plan.promotion.mode,'relocated');
  assert.deepStrictEqual(promoted.plan.promotion.redirectRequests,[]);
  equal(promoted.candidate.z64.subarray(0x1BFE4,0x1BFEC),rom.z64.subarray(0x1BFE4,0x1BFEC));
  equal(promoted.candidate.z64.subarray(0x318DC,0x31BFC),rom.z64.subarray(0x318DC,0x31BFC));
  assert.strictEqual(OB64.nativeClasses.promotionLoaderState(promoted.candidate.z64),'relocated');
  const reread=OB64.loadROM(promoted.candidate.z64.slice().buffer);equal(reread.statGates.raw,promoted.plan.promotion.decoded);});
check('native promotion export reopens in Rebuild and cannot use Patch',()=>{
  const rom=OB64.loadROM(promoted.candidate.z64.slice().buffer);OB64.nativeRebuild.initializeSource(rom);
  assert.strictEqual(rom.buildMode,'rebuild');assert.throws(()=>OB64.nativeRebuild.patchSourceCheck(rom),/Rebuild|native/i);});
check('mixed native references and missing resources reject',()=>{const bytes=promoted.candidate.z64.slice();bytes[0xAEF83]^=1;
  assert.strictEqual(OB64.statGateRelocation.inspect(bytes,promoted.candidate.layout).state,'foreign');
  assert.throws(()=>OB64.nativeRebuild.patchSourceCheck({z64:bytes,layout:promoted.candidate.layout}),/mixed native/);
  bytes.set(promoted.candidate.z64);bytes.fill(255,0x27B0800,0x27B0C00);
  assert.strictEqual(OB64.statGateRelocation.inspect(bytes,promoted.candidate.layout).state,'foreign');});
check('native promotion rejects leftover legacy redirect entries',()=>{
  const bytes=promoted.candidate.z64.slice(),legacy=OB64.statGateRelocation.prepare(fresh().statGates,fresh().z64,promoted.candidate.layout);
  const requests=OB64.statGateRelocation.redirectRequests(legacy.profile,promoted.plan.promotion.payloadBytes);
  OB64.sourceRedirect.apply(bytes,OB64.sourceRedirect.prepare(bytes,requests,{knownCurrentRequests:[]}));
  const result=OB64.statGateRelocation.inspect(bytes,promoted.candidate.layout);
  assert.strictEqual(result.state,'foreign');assert.match(result.reason,/redirect-based/);});
check('return to retail-sized gates restores complete source owners',()=>{
  const reopened=OB64.loadROM(promoted.candidate.z64.slice().buffer),retail=fresh();
  reopened.statGates.byClass=retail.statGates.byClass;
  const {candidate,plan}=apply(reopened,{statGates:true});assert.strictEqual(plan.promotion.mode,'in-place');
  assert.strictEqual(OB64.nativeClasses.promotionLoaderState(candidate.z64),'retail');
  for(const loader of OB64.nativeClasses.verifyData().promotion.loaders)equal(candidate.z64.subarray(loader.start,loader.start+loader.size),retail.z64.subarray(loader.start,loader.start+loader.size));
  equal(OB64.parseStatGates(candidate.z64).raw,retail.statGates.raw);});
check('legacy Classes writers cannot enter Rebuild composition',()=>{const rom=fresh();
  for(const id of ['classes','class-descriptions','action-descriptions','stat-gates',
    'direct.class-definitions','text.class-descriptions','text.action-descriptions','stat.class-change-gates'])
    assert.throws(()=>OB64.nativeRebuild.authorizeComposition([{id,regions:[]}]),/cannot bypass/);
  const plan=OB64.nativeClasses.prepare(rom,rom.z64,{classDefs:true,statGates:true});
  assert.strictEqual(OB64.nativeRebuild.authorizeComposition(plan.owners).owners.length,2);});
check('stale plans and candidate interference reject atomically',()=>{const rom=fresh();const candidate={z64:rom.z64.slice(),layout:rom.layout};
  const plan=OB64.nativeClasses.prepare(rom,candidate.z64,{classDefs:true,statGates:true});candidate.z64[0xAEF80]^=1;
  const before=candidate.z64.slice();assert.throws(()=>OB64.nativeClasses.apply(candidate,plan),/changed after planning/);equal(candidate.z64,before);
  const other=OB64.nativeClasses.prepare(rom,rom.z64,{classDefs:true});other.writes[0].bytes[0]^=1;
  assert.throws(()=>OB64.nativeClasses.apply({z64:rom.z64.slice()},other),/changed after preflight/);
  const decoded=OB64.nativeClasses.prepare(rom,rom.z64,{statGates:true});decoded.promotion.decoded[0]^=1;
  assert.throws(()=>OB64.nativeClasses.apply({z64:rom.z64.slice()},decoded),/changed after preflight/);});
console.log('Passed '+passed+' Classes rebuild checks.');
