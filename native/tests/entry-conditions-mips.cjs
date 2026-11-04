'use strict';
// Run edited world-entry predicates through the actual exported game VM.
// The event-program loader/rendering boundary is deliberately not entered.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const directory=process.argv[2];if(!directory)throw Error('Supply entry-conditions export directory');
const files=fs.readdirSync(directory).filter(x=>x.endsWith('.v64'));assert.equal(files.length,1);
const rom=fs.readFileSync(path.join(directory,files[0]));if(rom.readUInt32BE(0)===0x37804012)rom.swap16();assert.equal(rom.readUInt32BE(0),0x80371240);
const script=fs.readFileSync(path.join(directory,'event-0.bin'));
const fixture=JSON.parse(fs.readFileSync(path.join(directory,'entry-fixture.json'),'utf8'));
function run(node,flags,chaos){
 const m=new FloatMips(),SP=0x807ff000,PROGRAM=0x80700000;
 m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(PROGRAM,script);
 for(const [id,value] of Object.entries(flags)){const at=0x80193686+(Number(id)>>3),mask=1<<(Number(id)&7);m.w8(at,(m.r8(at)&~mask)|(value?mask:0));}
 m.w8(0x801936a9,chaos);let pc=script.readUInt16BE(node*2);
 for(let n=0;n<1000;n++){
  assert(pc>=0&&pc+2<=script.length&&pc%2===0);
  m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);
  if(script[pc]===0x12)return script[pc+1];
  if(script[pc]===0x2f){m.call(0x80170954);return null;}
  m.call(0x80170954,0x80171b58,20000);pc=(m.r16(0x80197b04)+1)*2;
 }
 throw Error('Entry did not finish');
}
let cases=0;
for(const f29 of [0,1])for(const old30 of [0,1])for(const f127 of [0,1])for(const f128 of [0,1])for(const f132 of [0,1]){
 const flags={29:f29,30:old30,127:f127,128:f128,132:f132};
 const expected=f132?(!f128?31:null):!f29?(!f127?32:null):(!f128?31:null);
 assert.equal(run(20,flags,50),expected,JSON.stringify(flags));cases++;
}
for(let chaos=0;chaos<=100;chaos++){
 assert.equal(run(fixture.node,fixture.flags,chaos),chaos<=fixture.threshold?fixture.event:null,`Chaos Frame ${chaos}`);cases++;
}
console.log(`PASS ${cases} exported native entry cases: flag retargeting, inverted ON/OFF test, changed destination and Chaos Frame threshold. Original flag30 no longer controls the edited branch.`);
console.log('Executes exported VM predicates and jumps up to program selection; does not render or play the destination mission.');
