'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x8019a010,rom.subarray(0xe6d90,0xe87f8));
const payload=0x800e91d0,window=0x800e7a30,display=0x80300000,results=[];let trace=[],cursor=display;
function raw(){const end=m.r32(0x800e9ba0);if(end<cursor||end>display+65536)throw Error('display extent');while(cursor<end){trace.push({service:15,args:[m.r32(cursor)|0,m.r32(cursor+4)|0]});cursor+=8;}}
const specs=[[0x4c7d4,6],[0xe48f0,0],[0xe495c,1],[0xe4c54,3],[0xe4be0,3],[0xe4fe4,5],[0xe4dc8,3],[0x4cb6c,7],[0x4c848,9],[0xead04,2],[0x4f1e8,0],[0x4fe04,1],[0x4ef34,3],[0x8a74,4],[0xe4930,0]];
for(let service=0;service<specs.length;service++){
 const [offset,count]=specs[service],address=offset<0x30000?0x8006fc00+offset:offset<0x60000?0x8012a100+offset:0x800b3280+offset;
 m.trap(address,c=>{raw();let args=Array.from({length:count},(_,i)=>(i<4?c.get('a'+i):m.r32(c.get('sp')+16+(i-4)*4))|0);
 if(service===11)args=[m.r8(0x8019e19e),m.r8(0x8019e19a)];
 if(service===12)args[0]=(args[0]-payload)|0;
 trace.push({service,args});});
}
for(const style of [0,1,2,255])for(let variant=0;variant<48;variant++){
 const p=Buffer.alloc(1144),r=Buffer.alloc(168);r.writeUInt16BE(variant%11===0?0x1000:0,0);r[0x91]=variant%3===0?4:0;r[0x8a]=variant&1?32:0;
 for(const [at,v] of [[6,variant%2?-35:20],[8,variant%3?-10:30],[10,250],[12,170]])r.writeInt16BE(v,at);
 for(const [at,v] of [[0x14,variant%4?84:65535],[0x18,variant%5],[0x1a,[0,1,36,65535][variant%4]],[0x1c,variant%3],[0x1e,3],[0x20,240],[0x22,61],[0x30,30],[0x32,50],[0x36,2]])p.writeUInt16BE(v,at);
 p[0x3a]=variant%2;p[0x3b]=(variant>>1)%2;p[0x3d]=(variant>>2)%2;p[0x42]=variant%4;p[0x43]=(variant*37)&255;
 p[0x44]=variant%5;p[0x46]=variant%3;p[0x47]=style;p[0x49]=[0,50,128,254,255][variant%5];p[0x4a]=variant%2;
 p[0x4b]=variant%12;p[0x4f]=variant%3===0?128:0;p[0x50]=variant%4;p[0x54]=variant%4;p[0x56]=[0,1,127,254,255][variant%5];
 for(let i=0;i<96;i++){p[0x57+i]=i%2;p.writeUInt16BE(i,0xb8+i*2);}p.write('ABCDE\0',0x178,'ascii');
 m.load(window,r);m.load(payload,p);m.w32(0x8019e194,payload);m.w32(0x800e9ba0,display);cursor=display;trace=[];
 m.set('sp',0x803ffe00);m.call(0x8019a010);raw();results.push({id:`${style}/${variant}`,window:r.toString('hex'),payload:p.toString('hex'),trace});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));console.log(`${results.length} original full drawing traces. Rendering helpers controlled; original branches, coordinates, format selection and inline display words execute.`);
