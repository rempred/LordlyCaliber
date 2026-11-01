'use strict';
// Exercise complete exported C entry points. Inventory expectations come from
// the authored project; UI, allocation, sorting and item services are traps.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2])),rom=fs.readFileSync(fixture.rom);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
assert.equal(rom.readUInt32BE(0x27c4008),5);
const owners=require('../game/shops/consumers.json').owners;
const symbols=Object.assign({},...owners.map(o=>o.symbols));
const ram=o=>(o+0x8007d7a0)>>>0, eq=0x8021a010,co=0x8021a0d8;
const list=0x80300000,rows=0x80310000,buttons=0x80320000,window=0x80330000;
const clobber=m=>{for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])m.set(r,0xcccccccc);};
function cpu(){
 const m=new Mips(),events=[];m.ram=new Uint8Array(0x400000);
 for(const o of owners){m.load(o.address,rom.subarray(o.start,o.start+o.size));for(const t of o.tables)m.load(t.address,rom.subarray(t.start,t.start+t.size));}
 m.load(ram(0x19be40),rom.subarray(0x19be40,0x19bff0));m.set('sp',0x80060000);
 for(const [name,at]of Object.entries(symbols))if(name.startsWith('func_'))m.trap(at,c=>{events.push([name,c.get('a0'),c.get('a1'),c.get('a2')]);clobber(c);c.set('v0',0);});
 m.trap(symbols.memset_00023780,c=>{c.load(c.get('a0'),Buffer.alloc(c.get('a1')));clobber(c);});
 m.trap(symbols.resource_free,c=>{assert.equal(c.get('a0'),list);clobber(c);});
 m.trap(symbols.resource_alloc,c=>{assert(c.get('a0')<=70);clobber(c);c.set('v0',rows);});
 m.trap(symbols.func_0019B1B0,c=>{const category=c.get('a0')%6;clobber(c);c.set('v0',category);});
 m.trap(symbols.func_0019B800,c=>{const a=Array.from({length:c.get('a0')},(_,i)=>c.r32(eq+i*4));a.sort((a,b)=>a%6-b%6);a.forEach((v,i)=>c.w32(eq+i*4,v));clobber(c);});
 m.trap(symbols.func_0019B26C,c=>{events.push(['availability',c.get('a0'),c.get('a1')]);clobber(c);c.set('v0',1);});
 m.w32(0x800c4bdc,buttons);m.w32(0x800c4c4c,buttons+4);m.w32(0x801f0ca0,buttons+16);
 m.w16(buttons+16,0x8000);m.w16(buttons+18,0x4000);m.w32(0x80196a6c,9999999);
 m.w32(0x80219f20,list);m.load(eq-16,Buffer.alloc(300,0xa6));m.load(rows-16,Buffer.alloc(104,0xa6));
 // Each display entry writes its own command list; its drawing services do not
 // simulate pixels. Item-ID arguments prove the correct halfword was selected.
 m.w32(0x800e9ba0,0x80380000);m.w16(window+0x18,255);
 for(let i=0;i<12;i++)m.w32(0x80219d28+i*4,0x80000200);
 m.trap(0x80000200,c=>{clobber(c);c.set('v0',0);});
 return {m,events};
}
let initializers=0,guards=0,renderers=0;
for(const stock of fixture.shops.records){
 const {m,events}=cpu();const combined=[...stock.consumables,...stock.equipment.map(x=>x|0x8000),0];
 combined.forEach((v,i)=>m.w16(list+i*2,v));m.w8(0x80219d24,0);m.call(ram(0x1977e0));
 const counts=Array(6).fill(0);stock.equipment.forEach(v=>counts[v%6]++);
 const cats=counts.map((n,i)=>n?i:-1).filter(x=>x>=0).concat(6);
 assert.equal(m.r8(0x8021a122)+1,cats.length);cats.forEach((v,i)=>assert.equal(m.r8(0x8021a11a+i),v));
 stock.consumables.forEach((v,i)=>assert.equal(m.r16(co+i*2),v));
 assert.equal(m.r8(eq-1),0xa6);assert.equal(m.r8(rows-1),0xa6);assert.equal(m.r8(rows+cats.length*10),0xa6);
 initializers++;
 for(let cat=0;cat<cats.length;cat++){
  const kind=cats[cat],count=kind===6?stock.consumables.length:counts[kind];
  assert.equal(m.r16(rows+cat*10+6),(count-1)&65535);
  for(const selection of [-1,0,count-1,count])for(const pressed of [0,0x8000,0x4000,0xc000]){
   m.w8(0x8021a121,cat);m.w16(rows+cat*10,selection);m.w16(buttons,pressed);m.w8(0x80219d24,3);m.w8(0x80219f04,0);
   m.w32(0x80219f08,0xfeed);events.length=0;m.call(ram(0x1977e0));
   const allowed=!!(pressed&0x8000)&&selection>=0&&selection<count;
   assert.equal(m.r8(0x80219d24),allowed?4:(pressed&0x4000)?14:3);
   if(!allowed)assert.equal(m.r32(0x80219f08),0xfeed);
   else if(kind===6)assert.equal(m.r32(0x80219f08),stock.consumables[selection]);
   assert.equal(m.get('sp'),0x80060000);guards++;
  }
 }
 if(!stock.consumables.length)continue;
 const cat=cats.length-1;m.w8(0x8021a121,cat);m.w8(0x80219d24,3);m.w8(0x80219f04,0);
 for(const index of [...new Set([0,stock.consumables.length-1])]){
  m.w16(rows+cat*10,index);m.w16(rows+cat*10+2,index);m.w8(rows+cat*10+5,1);
  for(const [entry,service]of [[0x1989c4,'func_8016F4E0'],[0x1994b4,'func_8016F500'],[0x19a294,'availability']]){
   events.length=0;m.w32(0x800e9ba0,0x80380000);m.set('a0',window);m.set('a1',0);m.call(ram(entry));
   assert(events.some(e=>e[0]===service&&e[1]===stock.consumables[index]),`${entry.toString(16)} must read consumable ${stock.consumables[index]} at ${index}`);
   assert.equal(m.get('sp'),0x80060000);renderers++;
  }
 }
}
console.log(JSON.stringify({initializers,guards,renderers,result:'PASS',limit:'Item/drawing services trapped; no console pixel or full-game claim'}));
