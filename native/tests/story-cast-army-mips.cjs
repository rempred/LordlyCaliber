"use strict";
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const data=JSON.parse(fs.readFileSync(process.argv[2]));
class EventMips extends Mips {
 exec(pc,next){
  // Stop immediately after the cast-construction segment, before unrelated
  // birthday/calendar flags. All selection bytecode and getters execute intact.
  if(pc===0x801709a0&&this.r16(0x80197b04)===0x2304/2)return[0x80000100,0x80000104];
  return super.exec(pc,next);
 }
}
for(const row of data.cases){
 const m=new EventMips(),roster=Buffer.from(row.roster,'hex');
 m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));m.load(0x80300000,Buffer.from(data.program,'hex'));m.load(0x80193bc0,roster);
 m.w16(0x80197b04,0x1be4/2);m.set('sp',0x803f0000);m.set('a0',0x80300000);m.call(0x80170954,0x80000100,4000000);
 const cast=Array.from({length:5},(_,i)=>[m.r8(0x8019367c+i),m.r8(0x80193681+i)]).flat();
 assert.deepEqual(cast,row.cast,'original event cast selection');
 assert.deepEqual(Buffer.from(m.ram.slice(0x193bc0,0x1951a0)),roster,'selection preserves roster');
}
console.log(`PASS ${data.cases.length} original event-VM cast constructions. Date/trigger gating is outside the preview selection contract.`);
