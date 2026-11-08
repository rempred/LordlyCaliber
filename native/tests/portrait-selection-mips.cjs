'use strict';
// Execute the retail Army card's identity load and the portrait token decoder.
// Only the image-cache fetch is substituted; selector decisions remain MIPS.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
assert.equal(require('node:crypto').createHash('sha256').update(rom).digest('hex'),
 '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
const m=new Mips();m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
// Placement from accepted C6BEC's qualified ROM owner, not the boot mapping.
const owner=require('../game/accepted/rom-owners.json').sources.find(s=>s.name==='func_000c6bec');
const delta=owner.address-owner.parts[0].start;
assert.equal(delta,0x801135a0);
m.load(0xc77a8+delta,rom.subarray(0xc77a8,0xc77cc));
let selected=-1,member=-1;
m.trap(owner.address,c=>{selected=c.get('a0');});
const cards=[];
for(let identity=0;identity<165;++identity)for(let current=0;current<165;++current){
 m.w8(0x80193bc0+56+17,identity);m.w8(0x80193bc0+56+18,current);
 m.set('s0',1);m.set('sp',0x803ffe00);selected=-1;
 m.call(0xc77a8+delta,0xc77cc+delta);assert.equal(selected,identity);
 cards.push([identity,current,selected]);
}
const image=0x80340000,out=0x80350000,portraits=[];
m.trap(0x8007938c,c=>{assert.equal(c.get('a0'),0x1dfb6ea);member=c.get('a1');c.set('v0',image);});
for(let identity=0;identity<165;++identity)for(let variant=0;variant<2;++variant){
 member=-1;m.w32(out,0);m.set('sp',0x803ffe00);m.set('a0',identity);m.set('a1',variant);m.set('a2',out);
 m.call(0x8016fcec);let token=0;
 if(m.get('v0')){assert.equal(m.get('v0'),image);const palette=(m.r32(out)-image-1920)/160;assert(Number.isInteger(palette));token=member*8+palette;}
 portraits.push([identity,variant,token]);
}
fs.writeFileSync(process.argv[3],JSON.stringify({cards,portraits}));
console.log(`${cards.length} Army identity selections; ${portraits.length} portrait slots.`);
