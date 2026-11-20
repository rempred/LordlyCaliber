'use strict';
// Boundary checks for the three condition descriptions in SquadRoute.
// Run against the supported game image in private, isolated RAM.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const {createHash} = require('node:crypto');
const {FloatMips} = require('./float-mips.cjs');
const owners = require('../game/accepted/rom-owners.json').sources;
const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE() === 0x37804012) rom.swap16();
else if (rom.readUInt32BE() === 0x40123780) rom.swap32();
assert.equal(rom.readUInt32BE(), 0x80371240);
const machine = new FloatMips();
function mount(part) {
    const bytes = rom.subarray(part.start, part.start + part.size);
    assert.equal(createHash('sha256').update(bytes).digest('hex'), part.sha256.toLowerCase());
    machine.load(part.address, bytes);
    return part.address;
}
const scheduler = owners.find(owner => owner.name === 'func_00108AA0');
const predicate = mount(scheduler.parts.find(part => part.start === 0x10a718));
for (const part of scheduler.parts.filter(part => part.section === '.rodata')) mount(part);
mount(owners.find(owner => owner.name.toLowerCase() === 'func_0005c110').parts[0]);
const {D_801F0E18: count, D_801F0E19: record, D_801F0EBC: result} = scheduler.bindings;
const unit = 0x80300000;
machine.w8(count, 1);
machine.w8(record, 1);
for (const [index, value] of [0, 0, 256, 256].entries())
    machine.writeFloat(count - 128 + 4 * index, value);
let checks = 0;
function check(kind, selector, source, flags, expected, x = 16, z = 32) {
    machine.load(record + 1, Buffer.from([kind, 16, 32, 48, 64, selector]));
    machine.w32(unit, flags);
    machine.w8(unit + 4, source);
    machine.writeFloat(unit + 8, x);
    machine.writeFloat(unit + 16, z);
    machine.w16(result, 0);
    machine.set('sp', 0x803ff000);
    machine.set('a0', unit);
    machine.call(predicate,0x80000100,10000);
    assert.equal(machine.r16(result), Number(expected), JSON.stringify({kind, selector, source, flags, x, z}));
    ++checks;
}
for (const [x,z,inside] of [[16,32,true],[48,64,true],[15,32,false],[49,64,false],[16,31,false],[48,65,false]]) {
    check(5,7,7,0x11,inside,x,z);
}
check(5,7,8,0x11,false);
check(5,7,7,0x10,false);
check(5,7,7,0x01,false);
for (const [selector, flag] of [[1,0], ...Array.from({length:13},(_,i)=>[6+i,1+i]), [23,14]]) {
    const address = 0x80197168 + (flag >> 3);
    machine.w8(address, 0);
    check(6,selector,7,0x11,false);
    machine.w8(address, 1 << (flag & 7));
    check(6,selector,7,0x11,true);
    check(6,selector,7,0x10,false);
    check(6,selector,7,0x01,false);
    machine.w8(address, 0);
}
machine.w16(0x80197168, 0xffff);
check(6,5,7,0x11,false);
for (const flags of [0, 8, 0x10, 0x18, 0x01000000, 0x01000008, 0x01000010, 0x01000018]) {
    check(10,7,7,flags,!(flags & 0x10) || (flags & 0x01000008) === 0x01000000);
    check(10,7,8,flags,false);
}
console.log(`Passed ${checks} original squad-condition calls, including inclusive bounds and all 15 flag mappings.`);
