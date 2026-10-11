"use strict";
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const data=JSON.parse(fs.readFileSync(process.argv[2])),m=new Mips(),record=0x80320000;
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
if(data.items)m.load(0x8018c40c,Buffer.from(data.items,'hex'));
let draws=[],cursor=0;m.trap(0x8009c7cc,x=>{assert.ok(cursor<draws.length,'unexpected random draw');x.set('v0',draws[cursor++]);});
for(const row of data.cases){
 m.load(0x80187c14,Buffer.from(row.classes,'hex'));m.load(record,Buffer.from(row.before,'hex'));
 draws=row.draws;cursor=0;m.set('sp',0x803f0000);m.set('a0',record);m.set('a1',row.level||1);m.call(0x8016f11c);
 assert.deepEqual(Buffer.from(m.ram.slice(m.phys(record),m.phys(record)+56)),Buffer.from(row.after,'hex'),`original initializer, class ${Buffer.from(row.before,'hex')[17]}, level ${row.level||1}`);
 assert.equal(cursor,row.used,'random consumption');
}
console.log(`PASS ${data.cases.length} original character initializers through level 99. Synthetic records; no natural recruitment-menu run.`);
