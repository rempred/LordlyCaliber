'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);
m.load(0x80076f5c,rom.subarray(0x735c,0x745c));m.load(0x8007f648,rom.subarray(0xfa48,0xfc44));
const pool=0x800e82c8,sp=0x803ffe00,results=[];
function run(mask,margin,args,window){
 const initial=Buffer.alloc(6*168);for(let i=0;i<initial.length;i++)initial[i]=(i*37+19)&255;
 for(let i=0;i<6;i++)initial.writeUInt16BE(mask&(1<<i)?0xa123:0x123,168*i);
 m.load(pool-16,Buffer.alloc(initial.length+32,0xa7));m.load(pool,initial);
 const context=0x1234fedc,parent=0xffed;
 m.w32(0x800e79a0,margin);m.w32(0x800c4c20,context);m.w16(0x800e810e,parent);m.set('sp',sp);
 args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+i*4,v));
 m.call(window?0x8007f648:0x80076f5c);assert.equal(m.get('sp'),sp);
 for(let i=0;i<16;i++){assert.equal(m.r8(pool-16+i),0xa7);assert.equal(m.r8(pool+initial.length+i),0xa7);}
 const after=Buffer.from(m.ram.subarray(m.phys(pool),m.phys(pool)+initial.length));
 results.push({initial:initial.toString('hex'),margin,context,parent,args,window,slot:m.get('v0'),after:after.toString('hex')});
}
for(let mask=0;mask<63;mask++)for(const flags of [0,0x8000,0xc800,0xffffffff])
 run(mask,0,[flags,0x80198be8,-1,0x10000,0x12345678,-32769,-1234],false);
const coordinates=[[-1,-1],[0,0],[127,127],[128,128],[129,129],[239,239],[240,240],[319,319],[320,320],
 [32767,32767],[32768,32768],[65535,65535],[65536,65536],[2147483647,-2147483648],[10,128],[300,200],[-20,200],[300,-20]];
for(let free=0;free<6;free++)for(const margin of [0,1,16,320,-1,65536,2147483647,-2147483648])for(const [x,y] of coordinates)
 run(63^(1<<free),margin,[0x80198be8,x,y,-1,0x12345678],true);
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original UI registration: ${results.length} complete shared-pool comparisons, no controlled helpers.`);
