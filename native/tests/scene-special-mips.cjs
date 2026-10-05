'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
const late=p=>p+0x802395c0-0x2a8d20;
m.load(late(0x2adaf4),rom.subarray(0x2adaf4,0x2adf30));m.load(late(0x2aa608),rom.subarray(0x2aa608,0x2aa62c));
const root=0x80300000,pool=0x80310010,dl=0x80360000,matrices=0x80370000,sp=0x803ffe00,callback=0x803f0000;
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
let calls=[],gradient=[],processing=0;
m.trap(0x802397c4,()=>{});m.trap(0x80098aa0,()=>{});
m.trap(0x80080998,c=>c.load(c.get('a0'),memory(c.get('a1'),c.get('a2'))));
m.trap(0x8023e83c,c=>gradient.push((c.get('a0')-pool)/368));
m.trap(0x801c22d0,c=>processing|=c.get('a0'));
m.trap(0x801c22e8,c=>processing&=~c.get('a0'));
m.trap(callback,c=>calls.push({slot:(c.get('a0')-pool)/368,alpha:c.get('a1'),pass:c.get('a2'),opacity:c.r8(c.get('a0')+0x13c),processing}));
const cases=[];
for(const enabled of [false,true])for(const sources of [[],[30],[81],[30,82,83,163],[30,30,163],[80,84,162,164],[83,82,81]])for(const selector of [-1,2,4])for(const progress of [0,1,4]){
 m.load(root,Buffer.alloc(0x1d00));m.w32(0x8022a974,root);m.w32(0x800e9ba0,dl);m.w32(0x801869b0,matrices);m.set('sp',sp);calls=[];gradient=[];processing=0;
 m.load(root+0x1c00,Buffer.from('003c001e','hex'));if(enabled)m.w32(root+0x1cac,root+0x1c00);
 if(selector>=0){m.w32(root+0x1ca4,root+0x1c10);m.w8(root+0x1c10,selector);m.w16(root+0x1c12,progress);m.w16(root+0x1c14,4);}
 sources.forEach((source,slot)=>{const p=pool+slot*368;m.load(p-16,Buffer.alloc(368,0xa7));m.load(p,Buffer.alloc(336));m.w32(p+0xe8,source);m.w32(p+0xe0,callback);m.w8(p+0x13c,17+slot);m.w8(p+0x141,slot%4);m.w32(root+0x18+slot*4,p);});
 m.call(late(0x2adaf4));assert.equal(m.get('sp'),sp);
 sources.forEach((source,slot)=>{const p=pool+slot*368;assert.equal(m.r8(p+0x13c),17+slot);assert(memory(p-16,16).equals(Buffer.alloc(16,0xa7)));assert(memory(p+336,16).equals(Buffer.alloc(16,0xa7)));});
 cases.push({enabled,sources,selector,progress,calls,gradient,processing});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`${cases.length} original special-black Actor passes; selection, callback arguments, processing flags and restored opacity checked; graphics and image callbacks controlled.`);
