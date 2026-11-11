'use strict';
// Offline original libmus sequencer probe. No game/emulator state is modified.
// Synth voice sinks are observed; this does not yet produce or validate audio.
const fs=require('node:fs'),path=require('node:path');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);
const m=new FloatMips();m.ram=new Uint8Array(0x800000);
m.load(0x80070000,rom.subarray(0x400,0x2f000));
// Qualified resident tables and floating constants used by this family.
m.load(0x800a7c00,rom.subarray(0x38000,0x3f000));
m.set('sp',0x807ff000);
const bank=0x80104710,fx=0x8010a760,channels=0x80400000,voices=0x80410000;
m.load(bank,rom.subarray(0x423ff0,0x429cd0));m.load(fx,rom.subarray(0x4e3140,0x4f0fb0));
const call=(pc,args=[])=>{args.slice(0,4).forEach((a,i)=>m.set('a'+i,a));args.slice(4).forEach((a,i)=>m.w32(m.get('sp')+16+i*4,a));m.call(pc,0x80000100,2000000);return m.get('v0');};
let tick=0;const events=[];
m.trap(0x800901c0,()=>{}); // data cache writeback; isolated host memory
for(const [pc,kind] of [[0x800884c0,'start'],[0x80088550,'stop'],[0x80088410,'volume'],[0x80088380,'pitch'],[0x800882e0,'pan'],[0x80088240,'fxmix']])
 m.trap(pc,c=>events.push({tick,kind,voice:(c.get('a0')-voices)/28,value:c.get('a1')}));
call(0x80081db4,[bank,0x429cd0]);call(0x80081f7c,[fx]);
for(const [a,v] of [[0x800b1804,68],[0x800b1808,voices],[0x800b180c,channels],[0x800b1810,channels+4*0x13c],[0x800b1814,60],[0x800b1818,16666],[0x800b1820,1],[0x800b1824,0x12345678]])m.w32(a,v);
m.w16(0x800b181c,26623);m.w16(0x800b181e,32767);
for(let i=0;i<68;i++)call(0x80083738,[channels+i*0x13c]);
const id=+(process.argv[3]||303);
// Follow the game's queue path; do not derive a libmus index from a UI label.
m.load(0x800e9c20,rom.subarray(0x3f1b0,0x40e80));
const requests=process.argv[4]?JSON.parse(process.argv[4]):[{tick:0,request:id}];
const dispatched=[];
const execute=m.exec.bind(m);
m.exec=(pc,next)=>{if(pc===0x80081770)dispatched.push({tick,id:m.get('a0')});return execute(pc,next);};

for(;tick<900;tick++){
 for(const request of requests) if(request.tick*2===tick)call(0x800ea604,[0,request.request]);
 call(0x800eac24);call(0x800eaf1c);
 call(0x800822ac);
 let active=false;for(let i=4;i<68;i++)active||=m.r32(channels+i*0x13c+4)!==0;
 if(!active && requests.every(r=>r.tick*2<=tick))break;
}
const starts=events.filter(e=>e.kind==='start').map(e=>({...e,wave:Array.from({length:5},(_,i)=>m.r32(e.value+i*4).toString(16))}));
const pitches=events.filter(e=>e.kind==='pitch').map(e=>{const b=Buffer.alloc(4);b.writeUInt32BE(e.value);return b.readFloatBE();});
const hashes=[];
for(let index=0;index<rom.readUInt32BE(0x423ff0+0x20);index++){
 const base=0x423ff0, table=base+rom.readUInt32BE(base+0x2c);
 const wave=base+rom.readUInt32BE(table+index*4),start=0x429cd0+rom.readUInt32BE(wave),length=rom.readUInt32BE(wave+4),book=base+rom.readUInt32BE(wave+16);
 const order=rom.readUInt32BE(book),count=rom.readUInt32BE(book+4);if(order!==2)throw Error('Reference decoder supports order2');
 const output=Buffer.alloc(length/9*16*2),history=Array(8).fill(0);let out=0;
 // Independent direct recurrence (same public codec format, not VADPCM library).
 for(let frame=0;frame<length;frame+=9){
  const control=rom[start+frame],scale=2**(control>>>4),predictor=control&15;if(predictor>=count)throw Error('predictor');
  const coefficients=Array.from({length:16},(_,j)=>rom.readInt16BE(book+8+predictor*32+j*2));
  for(let half=0;half<2;half++){
   const residual=Array.from({length:8},(_,j)=>{const byte=rom[start+frame+1+half*4+(j>>>1)],nibble=j&1?byte&15:byte>>>4;return (nibble<8?nibble:nibble-16)*scale;});
   const decoded=residual.map((value,i)=>{
    let sum=coefficients[i]*history[6]+coefficients[i+8]*history[7]+value*2048;
    for(let k=0;k<i;k++)sum+=residual[k]*coefficients[8+i-k-1];
    return Math.min(32767,Math.max(-32768,sum>>11));
   });
   for(let j=0;j<8;j++){history[j]=decoded[j];output.writeInt16LE(decoded[j],out);out+=2;}
  }
 }
 hashes.push(require('node:crypto').createHash('sha256').update(output).digest('hex'));
}
console.log(JSON.stringify({id,dispatched,ticks:Math.min(900,tick+1),voices:starts.length,limited:tick>=900,
 parts:starts.map(e=>({tick:e.tick,voice:e.voice,sampleRomOffset:parseInt(e.wave[0],16)})),hashes}));
