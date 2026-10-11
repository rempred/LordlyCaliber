"use strict";
// Original image dispatcher, RGBA16 vertex builder and row-size helpers.
// Only bzero is supplied by the host. No rendering/display hardware is modeled.
const fs = require("node:fs"), assert = require("node:assert/strict");
const {Mips} = require("../../tests/lib/mips-interpreter.cjs");
const rom = fs.readFileSync(process.argv[2]);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const m = new Mips(); m.ram = new Uint8Array(0x400000);
m.load(0x801b5d58, rom.subarray(0x1f91e8, 0x1f962c));
m.load(0x801b7284, rom.subarray(0x1fa714, 0x1fad18));
m.load(0x801be9a8, rom.subarray(0x201e38, 0x201f00));
m.trap(0x80093380, c => { for (let i=0; i<c.get("a1"); ++i) c.w8(c.get("a0")+i, 0); });
const input = 0x80300000, vertices = 0x80380000, dl = 0x803a0000, sp = 0x803ffe00;
const cases = [];
for (const owner of ["layered", "world"]) for (const [width,height] of [[64,48],[320,240],[415,303],[128,1]]) {
 const data=Buffer.alloc(8 + ((width+3)&~3)*height*2);data.write("64");data[3]=2;
 data.writeUInt16BE(width,4);data.writeUInt16BE(height,6);m.load(input,data);
 m.w32(0x80197178,vertices);m.w32(0x800e9ba0,dl);m.w32(0x801969b0,0x803c0000);
 m.set("sp",sp);m.set("a0",0x80197178);m.set("a1",0x800e9ba0);m.set("a2",input);m.set("a3",0);
 m.w32(sp+16,0);m.w32(sp+20,0);
 if (owner === "world") { m.w32(0x80330000,input);m.set("a0",0x80330000);m.call(0x801b5d58); }
 else m.call(0x801b77cc);
 const points=[];
 for(let p=vertices;p<m.r32(0x80197178);p+=16)
  points.push([m.r16(p)<<16>>16,m.r16(p+2)<<16>>16,m.r16(p+4)<<16>>16]);
 assert(points.length>0);assert.equal(m.get("sp"),sp);
 cases.push({owner,width,height,points});
}
fs.writeFileSync(process.argv[3], JSON.stringify({cases}));
console.log("Eight original background vertex builds; only bzero controlled.");
