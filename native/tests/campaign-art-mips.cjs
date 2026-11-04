'use strict';
// Executes the actual location renderer and its freshly exported C selectors.
// Texture submission is captured; this is not an RDP/pixel or full-console test.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
function rom(file){const b=fs.readFileSync(file);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const image=rom(process.argv[2]),stock=rom(process.argv[3]);
const source=fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8'),start=source.indexOf('OB64.lzssDecode = function');
const codec={OB64:{}};vm.runInNewContext(source.slice(start,source.indexOf('\n};',start)+3),codec);
function resource(b,at){return Buffer.from(codec.OB64.lzssDecode(b,at+8,b.readUInt32BE(at+4)).output);}
const header=((image.readUInt32BE(0x51524)&65535)*65536+image.readInt16BE(0x5152a))-80;assert.ok([5,6].includes(image.readUInt32BE(header+8)));
const overlay=image.readUInt32BE(header+12),size=image.readUInt32BE(header+16),ram=0x8019a7a0;
const offset=a=>overlay+a-ram;
const art=image.readUInt32BE(offset(0x801a8270+8192-4));
const directory=image.readUInt32BE(offset(art)+48);
const metadata=JSON.parse(image.subarray(offset(art)+96,offset(art)+96+image.readUInt32BE(offset(art)+92)));
const markers=resource(stock,0x594280+0x1dd2486);
function machine(edited){
 const m=new FloatMips();m.load(ram,edited?image.subarray(overlay,overlay+size):stock.subarray(0x79730,0x87200));
 m.load(0x80600000,markers);m.set('sp',0x807ff000);m.w32(0x80197af8,0x80700000);m.w32(0x80700004,0x80600000);
 return m;
}
function render(m,id,state,tick){
 for(let n=0;n<40;n++){m.w16(0x801a7510+n*8,n===id-1?100:0);m.w16(0x801a7512+n*8,100);}
 m.load(0x80196a86,Buffer.alloc(24));m.w8(0x80196a86+((id-1)>>2),state<<(((id-1)&3)*2));
 m.w32(0x80700108,tick);m.w32(0x800e9ba0,0x80500000);let captured=[];
 m.trap(0x8019f560,()=>{
  const a=m.get('a0'),b=m.get('a1'),w=m.get('a2'),h=m.get('a3');
  captured.push({w,h,palette:Buffer.from(Array.from({length:32},(_,i)=>m.r8(a+i))),pixels:Buffer.from(Array.from({length:w*h/2},(_,i)=>m.r8(b+i)))});
 });
 m.trap(0x8019f81c,()=>{});m.set('a0',32);m.set('a1',32);m.call(0x801a2d64,0x80000100,100000);return captured;
}
const edited=machine(true),original=machine(false);let cases=0;
const phases=stock.subarray(0x871b4,0x871b8);
for(let id=1;id<=40;id++)for(let state=0;state<=3;state++)for(const tick of [0,1,2,3,4,5,6,12,17,89,90,91,180]){
 const icon=image[offset(art)+52+id-1],record=metadata[icon];assert.ok(record);
 const actual=render(edited,id,state,tick);
 if(state===0||state===3)assert.equal(actual.length,0);
 else{
  assert.equal(actual.length,1);const frame=record.kind?0:state===1?((id-1)+Math.floor(tick/6))%3:tick%90<4?phases[tick%90]:0;
  const data=Buffer.from(record.frames,'base64').subarray(((state-1)*3+frame)*160);
  const height=!record.kind&&state===2?13:16;
  assert.deepEqual(actual[0],{w:16,h:height,palette:data.subarray(0,32),pixels:data.subarray(32,32+8*height)},`location ${id}, state ${state}, tick ${tick}`);
  // Unchanged original icons retain the original renderer's texture bytes.
  const nativeCastle=stock.subarray(0x871a8,0x871b2).includes(id);
  if(icon<2 && nativeCastle===!!record.kind)assert.deepEqual(actual,render(original,id,state,tick));
 }
 cases++;
}
// Run the original two-word resource selection plus call, stopping at its return.
let selectedKey=0;edited.trap(0x8009dd38,()=>{selectedKey=edited.get('a0');edited.set('v0',0x80680000);});
edited.set('v0',0x80700000);edited.call(0x8019a820,0x8019a83c,10000);
assert.equal(selectedKey,image.readUInt32BE(offset(art)+8)-0x594280);
assert.equal(resource(image,selectedKey+0x594280).length,512+512*320);
console.log(`PASS: ${cases} native icon renders across 40 locations, all four states and animation boundaries; original icon texture parity; exported map loader resource key. Rasterization and full-console gameplay are not modeled.`);
