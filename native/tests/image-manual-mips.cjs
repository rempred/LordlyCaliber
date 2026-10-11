'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023f1e4,rom.subarray(0x2b3fe4,0x2b461c));m.load(0x80243cc8,rom.subarray(0x2b8ac8,0x2b8ae8));
const context=0x80300010,channel=0x80301010,root=0x80302000,pad=0x80306000,sp=0x803ffe00;
m.w32(0x8022a970,context);m.w32(0x8022a974,root);m.w32(root+0x1c58,channel);
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const snap=()=>({context:read(context+0x818,0x30),channel:read(channel,88)});
function run(width,height,x,y,state,enabled,input,count) {
 m.load(context-16,Buffer.alloc(0x848+32,0xa7));m.load(channel-16,Buffer.alloc(120,0xa7));
 m.load(context,Buffer.alloc(0x848));m.load(channel,Buffer.alloc(88));
 m.w16(context+0x834,width);m.w16(context+0x836,height);m.w8(context+0x83f,enabled);m.w8(context+0x83e,0x87);
 m.w8(context+0x83c,state);m.w32(context+0x828,7);m.w32(context+0x830,30);
 m.writeFloat(channel+0x40,x);m.writeFloat(channel+0x44,y);m.writeFloat(channel+0x54,.875);
 m.w32(0x800c47f0,input.present?pad:0);m.w8(pad+2,input.x);m.w8(pad+3,input.y);m.w16(0x800e79b0,input.digital);
 const initial=snap(),steps=[];
 for(let i=0;i<count;i++){m.set('sp',sp);m.call(0x8023f304);assert.equal(m.get('sp'),sp);steps.push(snap());}
 for(const [p,n]of [[context,0x848],[channel,88]])for(let i=0;i<16;i++){assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+n+i),0xa7);}
 return {initial,input,steps};
}
const rows=[],analog=[[-128,-128],[-5,-4],[-4,5],[15,-15],[16,-17],[127,127],[0,0]],buttons=[0,4,8,12,0x100,0x200,0x800,0x400,0xf00,0xf0c];let n=0;
for(const [w,h]of [[280,190],[281,191],[282,192],[641,481],[200,480],[640,100],[32767,32767],[65535,65535]])
 for(const offset of [-1,0,1])for(const state of [0,1,2,3,4])for(const digital of buttons) {
  const [x,y]=analog[n%analog.length],present=n%3!==0,sign=n++%2?-1:1;
  rows.push(run(w,h,sign*Math.trunc((w-280)/2)+offset,sign*Math.trunc((h-190)/2)+offset,state,1,{x,y,digital,present},3));
 }
for(const enabled of [0,2])for(const state of [1,2,3,4])rows.push(run(640,480,0,0,state,enabled,{x:127,y:-128,digital:0xf0c,present:true},3));
for(const state of [3,4])for(const digital of [4,8,12])rows.push(run(640,480,0,0,state,1,{x:0,y:0,digital,present:true},65));
fs.writeFileSync(process.argv[3],JSON.stringify({rows}));console.log(`${rows.length} original manual-input sequences, ${rows.reduce((n,r)=>n+r.steps.length,0)} updates; no movement or zoom helpers controlled.`);
