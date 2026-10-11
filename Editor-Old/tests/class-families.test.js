'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
const harness=fs.readFileSync(path.join(__dirname,'native-classes-export.test.js'),'utf8').split('(async () => {')[0];
const {fresh,runExport}=new Function('require','__dirname',harness+'\nreturn {fresh,runExport};')(require,__dirname);
const {Mips}=require('./lib/mips-interpreter.cjs');
const O=global.OB64,D=O.nativeClasses.verifyData().families;
function machine(z,rebuilt,model){
  const cpu=new Mips();cpu.set('sp',0x807FF000);
  D.owners.forEach(owner=>cpu.load(owner.start+0x801135A0,rebuilt?Buffer.from(owner.familyHex,'hex'):z.subarray(owner.start,owner.start+owner.size)));
  cpu.load(0x801CF320,z.subarray(0xBBD80,0xBBDAC));
  cpu.load(0x801EF2CC,z.subarray(0xDBD2C,0xDBDD0));
  cpu.load(0x801EF370,z.subarray(0xDBDD0,0xDBDD8));
  cpu.w32(0x80196AF8,0x80300000);cpu.w32(0x80300080,0x80400000);
  const byId=new Map(model.families.map(f=>[f.id,f]));
  model.assignments.forEach((id,i)=>cpu.w32(0x80400000+648+i*4,(byId.get(id).behavior<<16)|id));
  cpu.trap(0x801D2358,c=>c.set('v0',c.get('a1')%3?1:0));
  return cpu;
}
function compatibility(cpu,a,b){cpu.set('a0',a);cpu.set('a1',b);cpu.call(0x801D6D8C);return cpu.get('v0');}
function discoveryFamily(cpu,a,b){
  cpu.set('s2',a);cpu.set('s3',b);cpu.set('s4',0);
  cpu.call(0xC3A78+0x801135A0,0xC3AA8+0x801135A0);return cpu.get('v0');
}
function menu(cpu,id,seed){
  cpu.w16(0x803005E8,id);cpu.w8(0x8030018B,5);
  for(let i=0;i<11;i++)cpu.w8(0x80196A76+i,seed===null?255:(seed+i*37)&255);
  cpu.call(0x801D27E8);
  const count=cpu.r16(0x80301BFC),out=[];
  for(let i=0;i<count;i++)out.push(cpu.r16(0x80301C0E+i*2));
  assert.strictEqual(cpu.r16(0x80301C0E+count*2),0);return out;
}
async function main(){
  const rom=fresh(),retail=O.classFamilies.ensure(rom);
  const original=machine(rom.z64,false,retail),rebuilt=machine(rom.z64,true,retail);
  for(let a=0;a<81;a++)for(let b=0;b<81;b++)assert.strictEqual(compatibility(rebuilt,a,b),compatibility(original,a,b),a+':'+b);
  for(let id=0;id<=0xA4;id++)for(const seed of [null,23])assert.deepStrictEqual(menu(rebuilt,id,seed),menu(original,id,seed),'menu '+id);
  console.log('PASS complete rebuilt compatibility and menu functions preserve retail results');
  assert.throws(()=>O.classFamilies.add(rom,'Invalid None copy',0),/numbered family/);
  const family=O.classFamilies.add(rom,'Order of Dawn',1);assert.strictEqual(family,16);
  O.classFamilies.assign(rom,2,family);O.classFamilies.assign(rom,3,family);
  const custom=machine(rom.z64,true,rom.classFamilies);
  custom.set('a0',2);custom.call(0x801CF2F0);assert.strictEqual(custom.get('v0')&65535,16);
  const rows=menu(custom,2,null).map(v=>v&127);
  assert(rows.includes(3)&&rows.includes(0x26)&&rows.includes(0x2B));
  assert(!rows.includes(4),'unassigned retail family member must not leak into new family');
  const dragons=retail.assignments.map((id,i)=>id===5?i:-1).filter(i=>i>=0);
  assert(dragons.length>=2);
  const dragonFamily=O.classFamilies.add(rom,'New dragon line',5);
  O.classFamilies.assign(rom,dragons[0],dragonFamily);O.classFamilies.assign(rom,dragons[1],dragonFamily);
  const dragonCpu=machine(rom.z64,true,rom.classFamilies);
  assert.strictEqual(compatibility(dragonCpu,dragons[0],dragons[1]),1);
  assert.strictEqual(discoveryFamily(dragonCpu,dragons[0],dragons[1]),1);
  if(dragons.length>2)assert.strictEqual(compatibility(dragonCpu,dragons[0],dragons[2]),0);
  if(dragons.length>2)assert.strictEqual(discoveryFamily(dragonCpu,dragons[0],dragons[2]),0);
  console.log('PASS new families above the retail nibble range retain distinct membership and inherited behavior');
  const exported=await runExport(rom,{classFamilies:true});
  const reopened=fresh(exported.candidate.z64);
  assert.deepStrictEqual(O.classFamilies.ensure(reopened),rom.classFamilies);
  assert.strictEqual(O.nativeClasses.promotionLoaderState(reopened.z64),'families');
  assert.throws(()=>O.nativeRebuild.patchSourceCheck(reopened),/Rebuild ROM/);
  vm.runInThisContext(fs.readFileSync(path.join(__dirname,'../rom-compatibility.js'),'utf8'));
  const assessed=O.loadROM(exported.candidate.z64.slice().buffer);
  const identity=await O.consumableEffects.inspectSourceIdentity(exported.candidate.z64,'family-test.z64');
  await O.romCompatibility.identify(assessed,identity);
  O.nativeRebuild.initializeSource(assessed);
  O.consumableEffects.initializeSession(assessed,identity,{filename:'family-test.z64'});
  O.tools.initState(assessed);O.nativeRebuild.initializeToolState(assessed);
  O.combatAnimationOverrides.initialize(assessed);O.romCompatibility.assessFeatures(assessed);
  assert(assessed.compatibility.canExport,JSON.stringify(assessed.compatibility));
  assert(O.romCompatibility.tabState(assessed,'classes').canEdit!==false);
  const project=O.patch.collectPatch(rom);
  assert.strictEqual(project.version,36);
  const target=fresh(),dirty={};O.patch.applyPatch(target,JSON.parse(JSON.stringify(project)),dirty);
  assert.deepStrictEqual(O.classFamilies.ensure(target),rom.classFamilies);assert(dirty.classFamilies);
  const repeat=await runExport(reopened,{});
  assert(Buffer.from(repeat.candidate.z64).equals(Buffer.from(exported.candidate.z64)));
  console.log('PASS family export, Project round trip, ROM reopen, checksum, and deterministic re-export');
  const bad=JSON.parse(JSON.stringify(project));bad.patches.classFamilies.assignments[2]=65534;
  const before=JSON.stringify(target.classFamilies);assert.throws(()=>O.patch.applyPatch(target,bad,{}),/undefined family/);
  assert.strictEqual(JSON.stringify(target.classFamilies),before);
  const occupied=fresh();O.classFamilies.add(occupied,'Occupied test',1);occupied.z64[D.romStart]=0;
  assert.throws(()=>O.nativeClasses.prepare(occupied,occupied.z64,{classFamilies:true}),/occupied/);
  const damaged=exported.candidate.z64.slice();damaged[D.owners[0].start]^=1;
  assert.strictEqual(O.classFamilies.inspect(damaged).state,'foreign');
  const restored=fresh(exported.candidate.z64);restored.classFamilies=O.classFamilies.retail(restored);
  const restore=await runExport(restored,{classFamilies:true});
  assert.strictEqual(O.classFamilies.inspect(restore.candidate.z64).state,'absent');
  D.owners.forEach(owner=>assert(Buffer.from(restore.candidate.z64.subarray(owner.start,owner.start+owner.size)).equals(Buffer.from(owner.retailHex,'hex'))));
  console.log('PASS malformed family input, mixed source owners, and complete retail restoration');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
