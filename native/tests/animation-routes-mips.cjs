'use strict';

// Execute the cartridge's loader and selector, with only heap/resource IO trapped.
// This verifies the expanded resource's consumer contract, not the full game heap.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const input = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const rom = fs.readFileSync(input.rom);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
else if (rom.readUInt32BE(0) === 0x40123780) rom.swap32();
assert.equal(rom.readUInt32BE(0), 0x80371240);
const data = Buffer.from(input.resource, 'hex');
const cpu = new Mips();
cpu.ram = new Uint8Array(0x400000);
cpu.set('sp', 0x80060000);
const temporary = 0x802A0000, resident = 0x802C0000;
const copySize = data.readUInt16BE(30);
const calls = [];
cpu.trap(0x8009DAF4, c => {
  assert.equal(c.get('a0'), input.key); c.set('v0', data.length); calls.push('size');
});
cpu.trap(0x80071C04, c => {
  assert.equal(c.get('a0'), data.length); c.set('v0', temporary); calls.push('temporary');
});
cpu.trap(0x8009DBB8, c => {
  assert.equal(c.get('a0'), temporary); assert.equal(c.get('a1'), input.key);
  c.load(temporary, data); calls.push('read');
});
cpu.trap(0x80070F30, c => {
  assert.equal(c.get('a0'), copySize); c.set('v0', resident); calls.push('resident');
});
cpu.trap(0x80093060, c => {
  assert.equal(c.get('a0'), temporary); assert.equal(c.get('a1'), resident);
  assert.equal(c.get('a2'), copySize);
  c.load(resident, c.ram.slice(c.phys(temporary), c.phys(temporary) + copySize)); calls.push('copy');
});
cpu.trap(0x800712C4, c => { assert.equal(c.get('a0'), temporary); calls.push('free'); });
cpu.load(resident - 16, Buffer.alloc(copySize + 32, 0x5A));
cpu.load(0x801BE71C, rom.subarray(0x201BAC, 0x201D24));
cpu.call(0x801BE71C);
assert.deepEqual(calls, ['size','temporary','read','resident','copy','free']);
assert.equal(copySize, data.length);
assert.deepEqual(Buffer.from(cpu.ram.slice(cpu.phys(resident), cpu.phys(resident) + copySize)), data);
for (let i=0;i<16;i++) {
  assert.equal(cpu.r8(resident - 16 + i), 0x5A);
  assert.equal(cpu.r8(resident + copySize + i), 0x5A);
}
assert.equal(cpu.r32(0x801D06AC), resident);
const globals = [0x690,0x698,0x680,0x684,0x688,0x738,0x6F4,0x694,0x68C,0x69C,0x6A0,0x6A4,0x6A8,0x73C,0x830];
globals.forEach((offset,index)=>assert.equal(cpu.r32(0x801D0000+offset), resident+data.readUInt16BE(index*2)));

// The two short prefix loaders have only straight-line code and absolute service
// calls. Execute their entire bodies at an isolated test address, retaining all
// original instructions except the exporter-produced key/copy-size operands.
for (const spec of [
  {start:0x6E810,end:0x6E8A8,base:0x8019A798,handles:0x8019A790,equipment:0x8019A794,extra:false},
  {start:0x1B902C,end:0x1B9120,base:0x8021C9E8,handles:0x8021C9E0,equipment:0x8021C9E4,extra:true}
]) {
  const c=new Mips();c.ram=new Uint8Array(0x400000);c.set('sp',0x80060000);
  let allocations=0,reads=0,copies=0,frees=0;
  c.load(resident-16,Buffer.alloc(copySize+32,0x5A));
  c.trap(0x8009DAF4,x=>{assert.equal(x.get('a0'),input.key);x.set('v0',data.length);});
  c.trap(0x80070F30,x=>{
    const sizes=[data.length,copySize,8],addresses=[temporary,resident,0x802E0000];
    assert.equal(x.get('a0'),sizes[allocations]);x.set('v0',addresses[allocations++]);
  });
  c.trap(0x8009DBB8,x=>{assert.equal(x.get('a0'),temporary);assert.equal(x.get('a1'),input.key);x.load(temporary,data);reads++;});
  c.trap(0x80093060,x=>{
    assert.equal(x.get('a0'),temporary);assert.equal(x.get('a1'),resident);assert.equal(x.get('a2'),copySize);
    x.load(resident,x.ram.slice(x.phys(temporary),x.phys(temporary)+copySize));copies++;
  });
  c.trap(0x800712C4,x=>{assert.equal(x.get('a0'),temporary);frees++;});
  c.trap(0x80093380,x=>{assert.equal(x.get('a0'),0x802E0000);assert.equal(x.get('a1'),8);x.load(0x802E0000,Buffer.alloc(8));});
  c.trap(0x8021B5EC,x=>x.set('v0',0x802F0000)); // unrelated post-load object construction
  c.load(0x80100000,rom.subarray(spec.start,spec.end));c.call(0x80100000);
  assert.equal(allocations,spec.extra?3:2);assert.equal(reads,1);assert.equal(copies,1);assert.equal(frees,1);
  assert.equal(c.r32(spec.base),resident);
  assert.equal(c.r32(spec.handles),resident+data.readUInt16BE(0));
  assert.equal(c.r32(spec.equipment),resident+data.readUInt16BE(4));
  assert.deepEqual(Buffer.from(c.ram.slice(c.phys(resident),c.phys(resident)+copySize)),data);
  for(let i=0;i<16;i++){assert.equal(c.r8(resident-16+i),0x5A);assert.equal(c.r8(resident+copySize+i),0x5A);}
}

// The Stage reader retains the entire first allocation and publishes sections
// 6,9,10, so no copy-size operand is changed for it.
{
  const c=new Mips();c.ram=new Uint8Array(0x400000);c.set('sp',0x80060000);
  c.trap(0x8009DAF4,x=>{assert.equal(x.get('a0'),input.key);x.set('v0',data.length);});
  c.trap(0x80070F30,x=>{assert.equal(x.get('a0'),data.length);x.set('v0',temporary);});
  c.trap(0x8009DBB8,x=>{assert.equal(x.get('a0'),temporary);assert.equal(x.get('a1'),input.key);x.load(temporary,data);});
  c.load(0x80100000,rom.subarray(0x1EA0B8,0x1EA134));c.call(0x80100000);
  assert.equal(c.r32(0x801BA6B0),temporary);
  for(const [address,header] of [[0x801BA6BC,6],[0x801BA6B4,9],[0x801BA6B8,10]])assert.equal(c.r32(address),temporary+data.readUInt16BE(header*2));
}

cpu.load(input.classAddress, rom.subarray(input.classOffset,input.classOffset+input.classSize));
cpu.load(0x801BE308, rom.subarray(0x201798,0x201838));
const entries = input.entries.map(x=>({...x, selectors:Buffer.from(x.selectors,'hex')}));
let cases=0;
for (let source=1;source<entries.length;source++) {
  const owners = source < 165 ? [source] : [source,2,3,165];
  for (const owner of owners) {
    const row = entries[source].family===owner && entries[source].selectors.length ? entries[source] : entries[owner];
    assert(row.selectors.length, 'test owner needs a native available selector group');
    for (const action of [0,5,11,17,30,124,125,254]) for (let mode=0;mode<3;mode++) {
      let expected=0x28;
      for(let at=0;at+1<row.selectors.length;at+=4)
        if(row.selectors[at]===action){expected=row.selectors[at+1+mode];break;}
      cpu.set('a0',source);cpu.set('a1',owner);cpu.set('a2',action);cpu.set('a3',mode);
      cpu.call(0x801BE308);
      assert.equal(cpu.get('v0'),expected,`source ${source}, owner ${owner}, action ${action}, mode ${mode}`);
      cases++;
    }
  }
}
console.log(`All 4 resource loaders retained ${copySize} bytes with correct published pointers; ${cases} original-selector cases passed in 4 MiB model memory.`);
