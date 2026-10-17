'use strict';
// Complete original mode-zero Actor main/shadow matrix construction.
// Original sine/cosine execute; only atan2 and memory copy are supplied.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
class ActorMatrixMips extends FloatMips {
 exec(pc,next){
  const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31;
  // These original table copies use paired unaligned instructions at aligned
  // word boundaries. Each access covers the complete word; reject other lanes.
  if([34,38,42,46].includes(op)){
   const a=(this.r[rs]+(w<<16>>16))>>>0,last=op===38||op===46;
   assert.equal(a&3,last?3:0);const aligned=(a&~3)>>>0;
   if(op===34||op===38){if(rt)this.r[rt]=this.r32(aligned);}else this.w32(aligned,this.r[rt]);
   return[next,(next+4)>>>0];
  }
  return super.exec(pc,next);
 }
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new ActorMatrixMips();m.ram=new Uint8Array(0x400000);
for(const [a,b] of [[0x10fe0,0x11168],[0x20d40,0x210c0],[0x23018,0x231b0],[0x28740,0x29160]])m.load(a+0x8006fc00,rom.subarray(a,b));
for(const [a,b] of [[0x29eab0,0x29eafc],[0x29cbdc,0x29cd64],[0x2a1484,0x2a1610],[0x2a3310,0x2a33a4]])m.load(a+0x7ff907d0,rom.subarray(a,b));
for(const [a,b] of [[0x2ae654,0x2aeaa8],[0x2aeb48,0x2aefb8]])m.load(a+0x7ff8b200,rom.subarray(a,b));
for(const [ram,offset,size] of [[0x800abae0,0x3bee0,4],[0x800a9880,0x39c80,4],[0x800aa750,0x3ab50,4],[0x800ae890,0x3ec90,112],
 [0x802391b0,0x2a89e0,112],[0x80239330,0x2a8b60,32],[0x80243c00,0x2b8a00,120]])m.load(ram,rom.subarray(offset,offset+size));
m.trap(0x80080998,c=>{const destination=c.get('a0'),source=c.get('a1'),size=c.get('a2');assert.equal(size,64);c.load(destination,Buffer.from(c.ram.subarray(c.phys(source),c.phys(source)+size)));});
require('./graphics-math-reference.cjs').loadGraphicsMath(m,rom);
m.trap(0x8009cfe0,c=>c.setSingle(0,Math.atan2(c.single(12),c.single(14))));
const root=0x80300000,actor=0x80303000,channel=0x80303200,sp=0x803ffe00,output=0x80310000;
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const cases=[],inputs=[];
for(const eye of [[0,0,400],[20,100,600],[240,80,20]])
for(const position of [[-40,0,0],[80,20,40],[0,15,100]])
for(const angles of [[0,0],[20,-30],[-50,75]])for(const scale of [0.5,1.25])for(const probeAspect of [1.333,1.1])
 inputs.push({eye,position,angles,scale,probeAspect,facing:0,rotation:30,flags:0});
for(const eye of [[0,0,400],[20,100,600],[240,80,20]])for(let facing=0;facing<10;facing++)
for(const rotation of [-450,0,180,360])for(const flags of [0,2,6])
 inputs.push({eye,position:[40,25,30],angles:[20,-30],scale:1.25,probeAspect:1.333,facing,rotation,flags});
for(const {eye,position,angles,scale,probeAspect,facing,rotation,flags} of inputs){
 const camera=Buffer.from(rom.subarray(0x286748,0x286748+56));camera.writeFloatBE(55,0);
 for(let i=0;i<3;i++){camera.writeFloatBE(eye[i],20+i*4);camera.writeFloatBE(0,32+i*4);}
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w32(root+0x18,actor);
 const aspect=Buffer.alloc(8);aspect.writeDoubleBE(probeAspect);m.load(0x80239340,aspect);
 m.load(0x8022a778,camera);m.writeFloat(0x801d06fc,0.1);
 const initial=Buffer.alloc(336,0xa7);for(let i=0;i<3;i++){
  initial.writeFloatBE(scale+i*0.1,0x104+i*4);initial.writeFloatBE(position[i],0x11c+i*4);
 }initial[0x13e]=5;initial[0x13f]=facing;initial[0x145]=flags;
 initial.writeFloatBE(rotation,0x12c);initial.writeFloatBE(0.4,0x130);initial.writeFloatBE(-10,0x128);m.load(actor,initial);
 const transform=Buffer.alloc(88);transform.writeFloatBE(angles[0],0x48);transform.writeFloatBE(angles[1],0x4c);
 transform.writeFloatBE(2,0x40);transform.writeFloatBE(-4,0x44);transform.writeFloatBE(6,0x50);transform.writeFloatBE(1,0x54);
 // A nontrivial existing channel matrix is an explicit input to this owner.
 for(let i=0;i<4;i++)transform.writeInt16BE(1,i*10);
 transform.writeInt16BE(2,24);transform.writeInt16BE(-4,26);transform.writeInt16BE(6,28);
 m.load(channel,transform);m.w32(root+0x1c54+5*4,channel);
 m.set('sp',sp);m.set('a0',output);m.set('a1',output+0x100);m.set('a2',output+0x200);m.call(0x80239854);
 assert.equal(m.get('sp'),sp);
 const prepass={scale:memory(output,8).toString('hex'),position:memory(output+0x100,8).toString('hex')};
 m.set('sp',sp);m.call(0x80239d48);assert.equal(m.get('sp'),sp);
 const result=memory(actor,336);assert.deepEqual(result.subarray(0x20,0x60),result.subarray(0x60,0xa0));
 for(let i=0;i<336;i++)if(!(i>=0x20&&i<0xe0)&&!(i>=0xfc&&i<0x104))assert.equal(result[i],initial[i]);
 cases.push({probeAspect,camera:camera.toString('hex'),actor:initial.toString('hex'),channel:transform.toString('hex'),prepass,result:result.toString('hex')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original actor main/shadow matrices: ${cases.length} camera/position/rotation/scale cases; atan2 and copy controlled; complete function and balanced stack.`);
