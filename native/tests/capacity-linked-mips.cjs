'use strict';
// Execute product-compiler output at its final linked addresses. The mission
// resource and unrelated game services are controlled fixtures, not emulation.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const f=JSON.parse(fs.readFileSync(process.argv[2])),s=f.symbols,e=f.entries;
const owners=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/scenarios/capacity/owners.json'))).owners;
const setup=owners.find(o=>o.start===0x1960a8).symbols;
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
// Byte-level oracle for the struct-copy instruction extension, every alignment.
for(const op of [34,38,42,46])for(let n=0;n<4;n++){
 const t=new FloatMips(),memory=[0x11,0x22,0x33,0x44],reg=[0xaa,0xbb,0xcc,0xdd];
 t.load(0x80001000,memory);t.r[9]=0x80001000+n;t.r[8]=0xaabbccdd;
 t.w32(0x80000800,((op<<26)|(9<<21)|(8<<16))>>>0);
 const left=op===34||op===42,load=op===34||op===38,count=left?4-n:n+1;
 for(let i=0;i<count;i++){
  const mi=left?n+i:i,ri=left?i:4-count+i;
  if(load)reg[ri]=memory[mi];else memory[mi]=reg[ri];
 }
 t.exec(0x80000800,0x80000804);
 assert.deepEqual([...t.ram.slice(0x1000,0x1004)],memory);
 assert.equal(t.r[8],Buffer.from(reg).readUInt32BE(0));
}
const read=(a,n)=>Buffer.from(m.ram.slice(m.phys(a),m.phys(a)+n));
const call=(name,...args)=>{m.set('sp',0x803f0000);args.forEach((v,i)=>m.set('a'+i,v));m.call(e[name]||name,0x80000100,4000000);assert.equal(m.get('sp'),0x803f0000);return m.get('v0');};
m.load(f.base,Buffer.from(f.storage,'hex'));m.load(f.codeAddress,Buffer.from(f.code,'hex'));
m.load(f.classBase,Buffer.from(f.classes,'hex'));
if(f.custom)m.load(f.customAddress,Buffer.from(f.custom,'hex'));
for(const o of owners)for(const [name,a] of Object.entries(o.symbols))
 if(/^(func_|boot_|memset_)/.test(name))m.trap(a,c=>c.set('v0',0));
const clear=c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1')));
m.trap(0x80093380,clear);m.trap(setup.memset_00023780,clear);
m.trap(setup.boot_command_stream_dispatch,c=>c.set('v0',0x80361000));
m.trap(0x801d4928,c=>c.set('v0',7));
m.trap(setup.func_0012EA80,c=>m.load(c.get('a1'),Buffer.alloc(12)));
m.trap(setup.func_0012EA80_row,c=>m.load(c.get('a1'),Buffer.alloc(12)));
// Actual constructor is reached through its installed entry shim.
const shims=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/scenarios/capacity-extra.json'))).patches;
const constructor=shims.find(p=>p.site===0x1957d0);
m.w32(constructor.address,(0x08000000|((e[constructor.name]>>>2)&0x3ffffff))>>>0);m.w32(constructor.address+4,0);
m.traps.delete(constructor.address);
let made=0;
m.trap(0x802150bc,c=>{
 const id=c.get('a0'),spec=c.get('a1'),selector=c.get('a2'),row=s.lc_enemy_characters+id*52;
 const offset=selector===0?0:selector===1?7:16;
 m.w8(row+17,m.r8(spec+offset));m.w8(row+18,m.r8(spec+offset));m.w16(row+24,125);made++;
});
// Use the source-built map-object constructor through its normal entry.
for(const name of ['func_0012967C','func_00129068','func_00128E80']) {
 const o=owners.find(o=>o.adaptedSource.endsWith('/'+name+'.c'));
 m.traps.delete(o.address);m.w32(o.address,(0x08000000|((e[name]>>>2)&0x3ffffff))>>>0);m.w32(o.address+4,0);
}
const eset=0x80360000,template=0x80361000;
m.w8(setup.D_800E7AC6,0);m.w32(setup.D_800E7AC6-0x36,eset);
m.w16(eset+2,16);m.w8(eset+16,30);
const spec=Buffer.alloc(35);spec[0]=5;spec[6]=1;spec[7]=8;spec[13]=2;spec[14]=3;spec[15]=4;spec[16]=9;spec[22]=5;m.load(template,spec);
for(let i=0;i<30;i++) {
 const row=eset+17+i*18;m.w8(row+2,30+i);m.w16(row+3,1);m.w8(row+5,1);
 m.w8(row+10,1);m.w16(row+13,200+i);
 const p=s.lc_capacity_sources+i*25;
 m.w8(p,i+1);m.w8(p+1,1);m.w8(p+2,i+1);
 m.w32(s.lc_capacity_pointers+i*4,s.lc_capacity_objects+i*192);
}
const players=read(s.lc_capacity_sources,750),oldItems=Buffer.from([31,41,51,61,71]);m.load(0x8019367c,oldItems);
call('func_001960A8');
assert.equal(made,150);assert.deepEqual(read(s.lc_capacity_sources,750),players);
assert.deepEqual(read(0x8019367c,5),oldItems);
const used=new Set();
for(let i=0;i<61;i++)assert.equal(m.r32(s.lc_capacity_pointers+i*4),s.lc_capacity_objects+i*192,'map pointer '+i);
for(let i=30;i<60;i++) {
 const p=s.lc_capacity_sources+i*25,object=s.lc_capacity_objects+i*192;
 assert.equal(m.r8(p),i+1);assert.equal(m.r8(object+4),i);
 assert.equal(m.r16(s.lc_capacity_drops+(i-30)*2),200+i-30);
 for(let slot=0;slot<5;slot++){const id=m.r8(p+2+slot);assert(id>0&&id<255&&!used.has(id));used.add(id);assert.equal(m.r8(s.lc_enemy_types+id),1);}
}
assert.equal(used.size,150);assert.deepEqual(read(s.lc_capacity_sources+60*25,25),Buffer.alloc(25));
assert.equal(m.r32(s.lc_capacity_objects+60*192),0,'neutral object stays inactive');
console.log(JSON.stringify({linkedMissionMembers:made,mapPointers:61,playerSourcesPreserved:30,drops:30,neutralSlotInactive:true}));
