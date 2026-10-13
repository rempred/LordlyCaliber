'use strict';
// Runs the actual Compiler::compileModule output supplied by engine_tests.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2])),m=new Mips();
const bytes=Buffer.from(fixture.bytes,'hex'),values=[13,71,9,43,109,2,881,17];
m.load(fixture.address,bytes);m.set('sp',0x803f0000);
for(let i=0;i<32;i++){m.set('a0',i);m.call(fixture.entry);assert.equal(m.get('v0'),values[i&7]);}
assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(fixture.address),m.phys(fixture.address)+bytes.length)),bytes);
console.log('32 compiled lookup results correct; code and constants unchanged');
