'use strict';
// Execute original portrait selection and its raw ROM loader. Only DMA and the
// heap allocator/free are controlled; reject reads beyond the loaded allocation.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80230744,rom.subarray(0x29ff74,0x2a0088));
m.load(0x8009dbb8,rom.subarray(0x2dfb8,0x2e128));
const root=0x80300000,actor=0x80310000,table=0x80320000,rows=0x80360000,sp=0x803ffe00;
let allocation=0,freed=0,dma=[];
m.trap(0x80070f30,c=>{allocation=c.get('a0');assert(allocation<65536);c.set('v0',table);});
m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),table);++freed;});
m.trap(0x80089f80,c=>{const at=c.get('a0'),length=c.get('a2');assert(at+length<=rom.length);m.load(c.get('a1'),rom.subarray(at,at+length));dma.push([at,length]);});
const read16=m.r16.bind(m);
m.r16=a=>{const p=m.phys(a);if(p>=m.phys(table)&&p<m.phys(table)+0x20200&&p+2>m.phys(table)+allocation)throw Error('outside portrait allocation');return read16(a);};
const cases=[];
function run(identity,bank,variant,portrait){
 m.load(root,Buffer.alloc(0x1cc0));m.load(actor,Buffer.alloc(336));m.load(rows,Buffer.alloc(0x1c4+28*248));
 m.w32(0x8022a974,root);m.w32(0x801ce8bc,rows);m.w32(0x800abd74,0);m.w32(0x800abd70,0);
 m.w32(root+24+7*4,identity<0?0:actor);m.w8(actor+0x147,identity);m.w32(actor+0xe8,bank);m.w8(actor+0x146,variant);
 if(identity>=0&&identity<28)m.w32(rows+0x1c4+identity*248+0x48,portrait);
 allocation=0;freed=0;dma=[];m.set('sp',sp);m.set('a0',7);
 let value=null,error=false;
 try{m.call(0x80230744);value=m.get('v0')|0;assert.equal(m.get('sp'),sp);}
 catch(e){if(e.message!=='outside portrait allocation')throw e;error=true;}
 if(identity===255&&!error){assert.equal(freed,1);assert.deepEqual(dma,[[0x2022a2e,4],[0x2022a32,332]]);}
 cases.push({identity,bank,variant,portrait,value,error});
}
run(-1,0,0,0);
for(const identity of [0,19,27])for(const portrait of [0,84,0x10000,0x10054,0xffffffff])run(identity,30,0,portrait);
for(let bank=0;bank<=69;++bank)for(const variant of [0,1,15,255])run(255,bank,variant,0);
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`${cases.length} original portrait lookups; ${cases.filter(v=>v.error).length} out-of-allocation reads rejected`);
