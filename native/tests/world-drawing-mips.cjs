"use strict";
// Original world drawing driver, orthographic setup and packed matrix routines.
// Scene presence, draw services, memory copy, trig and unrelated final services
// are controlled; projection/model matrix construction and pass order execute.
const fs=require("node:fs"),assert=require("node:assert/strict"),{FloatMips}=require("./float-mips.cjs");
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023c9ec,rom.subarray(0x2ac14c,0x2ac700));
m.load(0x801b8f2c,rom.subarray(0x1fc3bc,0x1fc538));
for(const [a,b] of [[0x225d0,0x22720],[0x22e90,0x231b0],[0x28740,0x29160]])m.load(a+0x8006fc00,rom.subarray(a,b));
m.load(0x800aa750,rom.subarray(0x3ab50,0x3ab54));
m.trap(0x80092db0,c=>c.setSingle(0,Math.sin(c.single(12))));
m.trap(0x800907f0,c=>c.setSingle(0,Math.cos(c.single(12))));
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
m.trap(0x80080998,c=>c.load(c.get("a0"),memory(c.get("a1"),c.get("a2"))));
for(const addr of [0x801b5934,0x801b5940])m.trap(addr,c=>c.set("v0",1));
for(const addr of [0x801b574c,0x8022be30,0x80180bdc,0x8023e394,0x80234814,0x802332a0])m.trap(addr,()=>{});
const root=0x80310000,record=0x80318000,graphics=0x80320000,dl=0x80360000,sp=0x803ffe00;
m.w32(0x8022a974,root);m.w32(root+0x19f0,record);m.w32(0x800e9be0,graphics);
m.w32(graphics+8,0x80340000);m.w32(graphics+16,0x80350000);m.w32(0x8018fdc0,0x80321000);
for(const bank of [0x801d0770,0x801d07a0])for(const at of [28,32,36])m.writeFloat(bank+at,1);
let events=[];
function capture(kind,c){
 let clip=null,model=null;
 for(let p=dl;p<c.r32(0x800e9ba0);p+=8){
  const word=c.r32(p);
  if(word===0xda380007)clip=memory(c.r32(p+4),64).toString("hex");
  if(word===0xda380003)model=memory(c.r32(p+4),64).toString("hex");
 }
 events.push({kind,clip,model});
}
m.trap(0x801bb58c,c=>capture("background",c));m.trap(0x8023aecc,c=>capture("actors",c));
m.trap(0x801bb87c,c=>capture("foreground",c));
m.trap(0x8022b100,c=>{assert.equal(c.get("a1"),0);capture("sprites"+c.get("a0"),c);});
const cases=[];
m.w32(0x801ce8bc,0x80370000);
for(const [sx,sy] of [[1,1],[.7,1.3],[-1,2],[0,0],[12.345,-.125]])
for(const [x,y] of [[0,0],[25.375,-90.125],[-160,120]]){
 m.writeFloat(record+0x24,sx);m.writeFloat(record+0x28,sy);m.writeFloat(record+0x30,x);m.writeFloat(record+0x34,y);
 m.w32(0x800e9ba0,dl);m.w16(0x800c4be4,0);m.w16(0x800e79e0,0);
 m.set("sp",sp);events=[];m.call(0x8023c9ec);
 assert.equal(m.r32(0x80376080),cases.length+1);
 assert.equal(m.get("sp"),sp);
 assert.deepEqual(events.map(e=>e.kind),["background","sprites0","actors","foreground","sprites1"]);
 cases.push({sx,sy,x,y,events});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log("15 original world drawing invocations; clip/model matrices and pass order captured.");
