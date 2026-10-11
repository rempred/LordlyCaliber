'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
const delta=0x7ff907d0,scene=0x80310000,sp=0x803ffe00,dl=0x80350000;
m.load(0x29a930+delta,rom.subarray(0x29a930,0x29ac44));
m.w32(0x8022a974,scene);m.set('sp',sp);
let transforms=[],draws=[];
m.trap(0x80098c20,c=>{transforms=[{kind:'translation',bits:[c.get('a1'),c.get('a2'),c.get('a3')]}];});
m.trap(0x80098660,c=>{assert.equal(c.get('a2'),0);assert.equal(c.get('a3'),0);assert.equal(c.r32(c.get('sp')+16),0x3f800000);transforms.push({kind:'rotation',bits:[c.get('a1')]});});
m.trap(0x80098aa0,c=>transforms.push({kind:'scale',bits:[c.get('a1'),c.get('a2'),c.get('a3')]}));
m.trap(0x8022fdb8,c=>{assert.equal(c.get('a1'),255);draws.push({slot:c.r32(c.get('a0')+0x2c),pass:c.get('a2'),transforms});});
const records=[];
const xs=[-2147483648,-16777217,-162,-1,0,157,16777217,2147483647];
const rotations=[-32768,-180,-1,0,90,180,270,32767],scales=[-32768,-100,-1,0,1,100,108,32767];
for(let slot=0;slot<30;slot++){
 const r=Buffer.alloc(80);r.fill(255,0,16);r.writeUInt32BE(0x8022fdb8,0x20);
 r.writeInt32BE(xs[slot%8],0x24);r.writeInt32BE(xs[(slot+3)%8],0x28);r.writeInt32BE(slot,0x2c);
 r.writeInt32BE(57,0x30);r.writeInt16BE(rotations[slot%8],0x40);r.writeInt16BE(0,0x42);
 r.writeInt16BE([0,799,800,801,802,-1][slot%6],0x44);r.writeInt16BE(scales[(slot*3)%8],0x48);
 r[0x4a]=[0,1,3,255,3][slot%5];
 const at=0x80320000+slot*80;m.load(at,r);m.w32(scene+0x1ac0+slot*4,at);
 records.push({slot,record:r.toString('hex')});
}
const cases=[];
for(const pass of [-2147483648,-65536,-1,0,1,2,3,255,256,65536,65539])for(const special of [0,1,2,255]){
 draws=[];m.w32(0x800e9ba0,dl);m.w32(0x801969b0,0x80360000);m.set('a0',pass);m.set('a1',special);m.call(0x8022b100);
 assert.equal(m.get('sp'),sp);cases.push({pass,special,draws});
}
// A separate original renderer run stops immediately after its primitive-color
// write. Metadata/image lookup and row-byte sizing are controlled; all color
// arithmetic and emitted display-list bytes are original instructions.
const colors=[];
const c=new FloatMips();c.ram=new Uint8Array(0x400000);
c.load(0x29f5e8+delta,rom.subarray(0x29f5e8,0x29ff74));
c.w32(0x8022a974,scene);c.set('sp',sp);
const sprite=0x80320000,texture=0x80340000,transition=0x80330000;
c.load(texture,Buffer.from([0,0,0,1,0,8,0,8]));
c.trap(0x80233800,m=>m.set('v0',1));c.trap(0x80232ee8,m=>m.set('v0',texture));c.trap(0x801be9a8,m=>m.set('v0',8));
for(const global of [[255,255,255],[128,64,32],[0,-1,32767],[-32768,256,511]])
for(const current of [null,0,1,128,254,255])for(const alpha of [0,1,127,254,255]){
 const record=Buffer.from(records[0].record,'hex');record[0]=153;c.load(sprite,record);
 for(let i=0;i<3;i++)c.w16(0x80220e70+i*2,global[i]);
 c.w32(scene+0x1bb4+3*4,current===null?0:transition);
 if(current!==null){c.load(transition,Buffer.alloc(80));c.w8(transition+4,current);c.w8(transition+5,17);c.w8(transition+6,39);}
 c.w32(0x800e9ba0,dl);c.set('a0',sprite);c.set('a1',alpha);c.set('a2',3);
 c.call(0x8022fdb8,0x29f924+delta);const end=c.r32(0x800e9ba0);
 assert.equal(c.r32(end-8),0xfa000000,JSON.stringify({global,current,alpha,end:end.toString(16)}));
 colors.push({global,current,alpha,colorByte:153,rgba:c.r32(end-4)});
 // The early stop deliberately leaves the original stack frame active.
 c.set('sp',sp);
}
fs.writeFileSync(process.argv[3],JSON.stringify({records,cases,colors}));
console.log(`Original sprite drawing: ${cases.length} pass selections/transforms and ${colors.length} primitive-color writes. Matrix constructors/frame draw and texture metadata controlled.`);
