'use strict';
// Complete original mode-two camera basis and Actor matrices, including both
// shadow branches and point-light helpers. Only trig and terrain are controlled.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
for(const [a,b] of [[0x11170,0x111c0],[0x22e90,0x231b0],[0x28be0,0x29200],[0x2cb50,0x2cb80]])
 m.load(a+0x8006fc00,rom.subarray(a,b));
for(const [a,b] of [[0x29d158,0x29d2b4],[0x29eab0,0x29eafc]])m.load(a+0x7ff907d0,rom.subarray(a,b));
m.load(0x8023a370,rom.subarray(0x2a9ad0,0x2aa3b0));
for(const [ram,offset,size] of [[0x800aa750,0x3ab50,4],[0x800ae400,0x3e800,8],
 [0x8023ec10,0x2ae370,64],[0x80239220,0x2a8a50,16]])m.load(ram,rom.subarray(offset,offset+size));
m.trap(0x80092db0,c=>c.setSingle(0,Math.sin(c.single(12))));
m.trap(0x800907f0,c=>c.setSingle(0,Math.cos(c.single(12))));
m.trap(0x8009cfe0,c=>c.setSingle(0,Math.atan2(c.single(12),c.single(14))));
m.trap(0x8009cfb0,c=>c.setSingle(0,Math.atan(c.single(12))));
const root=0x80300000,actor=0x80303000,basis=0x80304000,scale=0x80304100,cameraAt=0x80304200,attachment=0x80304300,sp=0x803ffe00;
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const bits=v=>{const b=Buffer.alloc(4);b.writeFloatBE(v);return b.readUInt32BE(0);};
const inputs=[],cases=[];
for(const eye of [[0,80,300],[40,100,-200],[240,-60,0]])
for(const position of [[-40,0,-30],[80,20,40],[0,100.25,100]])for(let flags=0;flags<8;flags++)
for(const point of [[0,0,0],[40,100,100],[-20,-10,40]])for(const depth of [-123,0,250])
 inputs.push({eye,position,flags,point,depth,angle:0,ratio:1});
for(const angle of [0,149.999,150,180,329.999,330,450])for(const ratio of [0,0.4,1,-1])
for(const height of [100.5,100.51,-1.6])
 inputs.push({eye:[30,80,300],position:[40,height,100],flags:4,point:[40,100.49,100],depth:-12,angle,ratio});
for(const input of inputs){
 const {eye,position,flags,point,depth,angle,ratio}=input;
 const camera=Buffer.from(rom.subarray(0x2866f0,0x2866f0+56));
 for(let i=0;i<3;i++){camera.writeFloatBE(eye[i],20+i*4);camera.writeFloatBE([10,0,-5][i],32+i*4);}
 const initial=Buffer.alloc(336,0xa7);for(let i=0;i<3;i++){
  initial.writeFloatBE([0.75,1.2,0.9][i],0x104+i*4);initial.writeFloatBE([12,-20,5][i],0x110+i*4);
  initial.writeFloatBE(position[i],0x11c+i*4);
 }initial.writeFloatBE(-15,0x128);initial.writeFloatBE(angle,0x12c);initial.writeFloatBE(ratio,0x130);
 initial.writeUInt32BE(3,0xe4);initial[0x145]=flags;
 const planar=[[40,-30,flags%3===0?-5:flags%3===1?50:105],[-20,100,flags%2?0:100]];
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w32(root+0x88+3*4,attachment);m.w16(attachment,depth);
 for(let i=0;i<3;i++)m.writeFloat(root+4+i*4,point[i]);
 for(const [p,v] of [[0x801d0768,planar[0][0]],[0x801d0814,planar[0][1]],[0x801d0718,planar[0][2]],
 [0x801d06f0,planar[1][0]],[0x801d0764,planar[1][1]],[0x801d0740,planar[1][2]]])m.writeFloat(p,v);
 m.load(cameraAt,camera);m.load(actor,initial);m.set('sp',sp);m.set('a0',cameraAt);m.call(0x8023aa8c);
 m.set('a0',basis);for(let i=0;i<3;i++)m.set(['a1','a2','a3'][i],m.r32(0x801ce8ec+i*4));m.call(0x80092a90);
 m.set('a0',scale);for(const reg of ['a1','a2','a3'])m.set(reg,bits(0.1));m.call(0x80098b60);
 const terrain=-7.25;m.trap(0x801bc35c,c=>{assert.equal(c.get('a0')|0,Math.trunc(position[0])<<16>>16);assert.equal(c.get('a1')|0,Math.trunc(position[2])<<16>>16);c.setSingle(0,terrain);});
 m.set('a0',basis);m.set('a1',scale);m.set('a2',actor);m.call(0x8023a370);assert.equal(m.get('sp'),sp);
 const result=memory(actor,336);for(let i=0;i<336;i++)if(i<0x20||i>=0xe0)assert.equal(result[i],initial[i]);
 cases.push({camera:camera.toString('hex'),actor:initial.toString('hex'),point,planar,depth,terrain,result:result.toString('hex')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`Original mode-two camera/main/two-shadow matrices: ${cases.length} cases; only trig and terrain controlled.`);
