'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const inputs=JSON.parse(fs.readFileSync(process.argv[3]));
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8023fb90,rom.subarray(0x2b4990,0x2b4d58));
m.load(0x80243ce8,rom.subarray(0x2b8ae8,0x2b8af8));
const context=0x80300000,root=0x80302000,records=0x80306000,sp=0x803ffe00;
m.w32(0x8022a970,context);m.w32(0x8022a974,root);
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const pointer=i=>root+0x1ac0+(29-i)*4;
const call=(p,args=[])=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p);assert.equal(m.get('sp'),sp);};
let constructors=[];
m.trap(0x8022af94,c=>{
 const args=[c.get('a0'),c.get('a1'),c.get('a2'),c.get('a3'),...Array.from({length:5},(_,i)=>c.r32(c.get('sp')+16+i*4))];constructors.push(args);
 const i=29-args[0],p=records+0x400+i*80;m.load(p,Buffer.from(inputs.created[i],'hex'));m.w32(pointer(i),p);
});
m.trap(0x8022b414,c=>m.w32(root+0x1ac0+c.get('a0')*4,0));
const snapshot=()=>({context:read(context,0x848).toString('hex'),attached:[0,1,2,3].map(i=>{const p=m.r32(pointer(i));return p?read(p,80).toString('hex'):null;}),retained:[0,1,2,3].map(i=>{const p=m.r32(context+0xa8+i*4);return p?read(p,80).toString('hex'):null;})});
const cases=[];
for(const [width,height]of [[0,0],[280,190],[281,191],[640,480],[200,480],[640,100],[65535,65535]])for(const occupied of [false,true]) {
 m.load(context,Buffer.alloc(0x848));m.load(root,Buffer.alloc(0x2000));m.w16(context+0x834,width);m.w16(context+0x836,height);m.w8(context+0x83e,255);m.w8(0x8022a997,2);constructors=[];
 if(occupied)for(let i=0;i<4;i++){const p=records+i*80;m.load(p,Buffer.from(inputs.existing[i],'hex'));m.w32(pointer(i),p);}
 const initial=snapshot();call(0x8023fb90,[56]);const built=snapshot();assert.equal(m.r8(0x8022a997),3);
 const count=constructors.length;call(0x8023fb90,[999]);assert.equal(constructors.length,count);assert.deepEqual(snapshot(),built);
 const steps=[],available=(height>=191&&height<32768?0x42:0)|(width>=281&&width<32768?0x18:0);
 for(const mask of [255,0,2,0x42,0x18,255,0xd6,0x6b,0xf8,0x1f]) {
  m.w8(context+0x83e,mask&available);call(0x8023fe14);steps.push({mask:mask&available,after:snapshot()});
 }
 // Another command replaces the final arrow. Detachment must retain the old
 // image-owned record, while reattachment copies the last live pose counters.
 if(available===0x5a) {
  m.load(records+0x800,Buffer.from(inputs.replacement,'hex'));m.w32(pointer(3),records+0x800);
  for(const mask of [0x40,0x5a,0,0x5a]){m.w8(context+0x83e,mask);call(0x8023fe14);steps.push({mask,replace:steps.length===10,after:snapshot()});}
 }
 call(0x8023fd88);const disabled=snapshot();assert.equal(m.r8(0x8022a997),2);
 cases.push({width,height,occupied,initial,built,constructors,steps,disabled});
}
fs.writeFileSync(process.argv[4],JSON.stringify({cases}));console.log(`${cases.length} complete original enable/visibility/disable sequences; sprite creation/release controlled with supplied real records.`);
