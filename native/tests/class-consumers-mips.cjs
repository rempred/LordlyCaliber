'use strict';
// Compare host consumer results with original resident getters. No lookup traps.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const input=JSON.parse(fs.readFileSync(process.argv[2]));
const rom=fs.readFileSync(input.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(0x80187c14,Buffer.from(input.classes,'hex'));m.set('sp',0x803ffe00);
let checks=0;
for(const row of input.cases){
 const [identity,current,...expected]=row,actual=[];
 for(const entry of [0x43e88,0x43edc,0x43f30,0x43f84]){
  m.set('a0',identity);m.set('a1',current);m.call(entry+0x8012a100);actual.push(m.get('v0'));++checks;
 }
 for(const entry of [0x44130,0x44238])for(let rank=0;rank<3;++rank){
  m.set('a0',identity);m.set('a1',current);m.set('a2',rank);m.call(entry+0x8012a100);actual.push(m.get('v0'));++checks;
 }
 assert.deepEqual(actual,expected,`identity ${identity}, current ${current}`);
 assert.equal(m.get('sp'),0x803ffe00);
}
console.log(`${checks} original equipment/attack/count getter results agree across ${input.cases.length} class pairs.`);
