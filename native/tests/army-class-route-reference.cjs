'use strict';
// Character routing oracle: execute US Rev 0's actual model/size getters and
// small-atlas branch with every identity/current-class pair. No editor resolver.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
assert.equal(require('node:crypto').createHash('sha256').update(rom).digest('hex'),
 '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a',
 'This oracle executes the pinned US Rev 0 instructions.');
const m=new Mips();m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
// This shared formation renderer's placement is also used by the campaign
// probes. Confirm it from the internal jump joining the two atlas branches.
assert.equal(rom.readUInt32BE(0x84c00)>>>26,2);
const delta=(((rom.readUInt32BE(0x84c00)&0x3ffffff)*4+0x80000000)>>>0)-0x84c14;
assert.equal(delta,0x80121070);
m.load(0x84bd4+delta,rom.subarray(0x84bd4,0x84c14));
const owner=0x80300000;m.w32(0x801a7960,owner);m.w32(owner+20,101);m.w32(owner+28,102);
const routes=[];
for(let identity=0;identity<165;identity++)for(let current=0;current<165;current++){
 m.r.fill(0);m.set('sp',0x803fff00);m.set('a0',identity);m.set('a1',current);
 m.call(0x8016de1c);const size=m.get('v0');
 m.set('a0',identity);m.set('a1',current);m.call(0x8016df18);const model=m.get('v0');
 let lane=2;
 if(size!==2){m.set('s1',identity);m.call(0x84bd4+delta,0x84c14+delta);lane=m.get('t4')-101;assert([0,1].includes(lane));}
 routes.push([identity,current,lane,model]);
}
fs.writeFileSync(process.argv[3],JSON.stringify(routes));
console.log(`Executed ${routes.length} retail identity/current-class pairs.`);
