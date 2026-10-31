'use strict';
// Original supply initializer, with only allocation, clearing, capacity lookup
// and player-inventory registration supplied by controlled host services.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
assert.equal(rom.readUInt32BE(0), 0x80371240);
const m = new Mips(), buffer = 0x80300010, records = 0x801971f0;
m.ram = new Uint8Array(0x400000);
m.load(0x8021590c, rom.subarray(0x195d9c, 0x1960a8));
m.load(0x80217330, rom.subarray(0x1977c0, 0x1977d8));
let capacity, registered;
m.trap(0x80070f30, c => { assert.equal(c.get('a0'),20); c.set('v0',buffer); });
m.trap(0x80093380, c => c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x801fe190, c => c.set('v0',capacity));
m.trap(0x801dc720, c => { assert.equal(c.get('a1'),1); registered.push(c.get('a0')); });
m.trap(0x800712c4, c => assert.equal(c.get('a0'),buffer));
// Healing presets include empty slots in their repeated construction pattern.
// The game sorts the nonzero values in the first capacity slots, retaining duplicates.
const healing = [[],[1,0,0,0,0],[1,1,0,0],[1,1,2,0,0,0],
                 [2,2,0,0],[2,2,3,0,0,0],[3,3,0,0]];
const fruit = [[],[4,4],[4,5],[5,5],[]], revival = [[],[6],[6,6]];
let cases=0;
for (capacity=0;capacity<=10;capacity++)
for (const source of [0,30,31,49])
for (let h=0;h<=6;h++) for(let f=0;f<=4;f++) for(let r=0;r<=2;r++) {
    // Fruit 4 alone emits nothing and cannot advance the game's loop. It is
    // retained only when already present, never offered for new authoring.
    if (!h && f===4 && !r && capacity) continue;
    const pattern=[...healing[h],...fruit[f],...revival[r]], slots=[];
    while(pattern.length && slots.length<capacity) slots.push(...pattern);
    const expected=slots.slice(0,capacity).filter(Boolean).sort((a,b)=>a-b);
    const row=records+source*25;
    m.load(row-1,Buffer.alloc(27,0xa5)); m.load(buffer-1,Buffer.alloc(22,0xa5));
    registered=[];
    m.set('sp',0x803ffe00);
    [source,h,f,r].forEach((v,i)=>m.set('a'+i,v));
    m.call(0x8021590c,0x80000100,100000);
    const actual=Array.from({length:10},(_,i)=>m.r8(row+13+i));
    assert.deepEqual(actual,[...expected,...Array(10-expected.length).fill(0)],
                     `source ${source}, capacity ${capacity}, presets ${h}/${f}/${r}`);
    assert.deepEqual(registered,source<30?expected:[]);
    for(const at of [row-1,row,row+12,row+23,row+24,row+25,buffer-1,buffer+20])
        assert.equal(m.r8(at),0xa5,`surrounding byte ${at.toString(16)}`);
    cases++;
}
console.log(`PASS ${cases} original-instruction supply cases: capacity 0..10, all supported presets, enemy/player paths, sorting and buffer guards.`);
console.log('Amrius source 31, capacity 2, presets 1/1/2: actual initial supplies [1] (one Heal Leaf).');
capacity=2;registered=[];m.set('sp',0x803ffe00);
[31,0,4,0].forEach((v,i)=>m.set('a'+i,v));
assert.throws(()=>m.call(0x8021590c,0x80000100,10000),/step limit/);
console.log('CONFIRMED: fruit preset 4 with both other presets empty does not terminate at positive capacity.');
capacity=11;registered=[];m.set('sp',0x803ffe00);
const unsafeRow=records+31*25;m.load(unsafeRow-1,Buffer.alloc(27,0xa5));
[31,0,0,1].forEach((v,i)=>m.set('a'+i,v));
m.call(0x8021590c,0x80000100,100000);
assert.equal(m.r8(unsafeRow+23),6); // The eleventh item overwrites the next field.
console.log('CONFIRMED: capacity 11 can overwrite the byte after the ten-slot array; the capacity upper bound remains necessary.');
