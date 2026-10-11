'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8023da68,rom.subarray(0x2b2868,0x2b3300));
m.load(0x80098aa0,rom.subarray(0x28ea0,0x29160));m.load(0x800988a0,rom.subarray(0x28ca0,0x28da0));
const context=0x80300000,source=0x80310000,processed=0x80311000,trail=0x80312000,dl=0x80320000,matrices=0x80330000,sp=0x803ffe00;
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
m.trap(0x80080998,c=>c.load(c.get('a0'),read(c.get('a1'),c.get('a2'))));m.trap(0x8024075c,()=>{});
const packed=(x,y,s)=>{const b=Buffer.alloc(64);[s,0,0,0,0,s,0,0,0,0,1,0,x,y,0,1].forEach((v,i)=>{const n=Math.trunc(v*65536)>>>0;b.writeUInt16BE(n>>>16,i*2);b.writeUInt16BE(n&65535,32+i*2);});return b;};
let cursor,stack,primitive,combine,draws,special;
function flush(){for(;cursor<m.r32(0x800e9ba0);cursor+=8){const a=m.r32(cursor),b=m.r32(cursor+4);switch(a>>>24){
 case 0xda:{const matrix=read(b,64).toString('hex'),flag=a&3;assert.equal(flag,0);stack.push(matrix);break;}
 case 0xd8:assert.equal(b,64);assert(stack.length>1);stack.pop();break;
 case 0xfa:primitive=b;break;
 case 0xfc:combine=[a,b];break;
}}}
m.trap(0x801b7284,c=>{flush();draws.push({image:c.get('a0')===processed?'processed':'source',x:c.get('a1')|0,y:c.get('a2')|0,primitive,combine,stack:[...stack]});});
m.trap(0x8022b100,c=>{flush();special={pass:c.get('a0'),special:c.get('a1'),stack:[...stack]};});
const cases=[],parent=packed(7,-9,1.5);
for(const mode of [0,1])for(const sample of [0,1,2,3,4,5,20])for(const mask of [0,1,5,7])for(const selected of [source,processed]) {
 m.load(context,Buffer.alloc(0x848));m.load(context,packed(-102,39,.29));m.w32(0x8022a970,context);m.w32(context+0x4c,processed);
 m.w32(context+0x824,sample);m.w32(context+0x828,11);m.w32(context+0x82c,30);m.w8(context+0x83b,mode);m.w8(context+0x83d,130);
 const saved=[];for(let i=0;i<3;i++){const b=packed(10+i*20,-6+i,1+i*.1);m.load(trail+i*64,b);m.w32(context+0x40+i*4,mask&(1<<i)?trail+i*64:0);m.w8(context+0x820+i,50+i*70);saved.push(b.toString('hex'));}
 m.w32(source,0x37340002);m.w16(source+4,640);m.w16(source+6,480);m.w32(processed,0x37340003);m.w16(processed+4,320);m.w16(processed+6,240);
 m.w32(0x800e9ba0,dl);m.w32(0x801969b0,matrices);cursor=dl;stack=[parent.toString('hex')];primitive=0;combine=[];draws=[];special=null;
 m.set('sp',sp);m.set('a0',selected);m.call(0x8023da68);flush();assert.equal(stack.length,1);assert.equal(m.get('sp'),sp);
 assert.deepEqual(special,{pass:1,special:1,stack:[parent.toString('hex')]});
 cases.push({mode,sample,mask,selected:selected===processed?'processed':'source',context:read(context,0x848).toString('hex'),saved,draws,special});
}
fs.writeFileSync(process.argv[3],JSON.stringify({parent:parent.toString('hex'),cases}));
console.log(`${cases.length} complete image drawing invocations; original matrix pushes/pops, alpha, combiner and texture calls captured. Special sprites inherit the caller's parent matrix in every case.`);
