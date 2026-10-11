'use strict';
// Original 2D sequencer and counted decoder. Resource registry and audio sink
// are controlled; all record writes and sound-table selection execute in MIPS.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const file of ['art.js','cutscene-data.js'])vm.runInThisContext(fs.readFileSync(path.join(root,file),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);
const delta=0x8022efb0-0x29e7e0,sprite=0x80300010,program=0x80310010,sp=0x803ffe00;
m.load(0x29d000+delta,rom.subarray(0x29d000,0x2a8d00));
m.load(0x801ceee0,rom.subarray(0x212370,0x212f00));
m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
m.trap(0x80232e44,c=>c.set('v0',program));
let audio=[],steps=0;
m.trap(0x801c5938,c=>audio.push([c.get('a0')===0x800eb240?0:1,c.get('a1')]));
const read=()=>Buffer.from(m.ram.subarray(m.phys(sprite),m.phys(sprite)+80)).toString('hex');
function run(bank,state,data,x,y,ticks=24){
 m.load(program-16,Buffer.alloc(data.length+32,0xa7));m.load(program,data);
 const initial=Buffer.alloc(80,0xa7);initial.fill(255,0,16);initial.fill(0,16,32);
 initial.writeInt32BE(x,0x24);initial.writeInt32BE(y,0x28);initial.writeInt32BE(bank,0x30);
 initial.writeInt32BE(-1,0x34);initial.writeInt32BE(0,0x38);initial.writeInt32BE(0,0x3c);
 initial.writeInt16BE(state,0x42);initial[0x4c]=0;
 m.load(sprite-16,Buffer.alloc(112,0xa7));m.load(sprite,initial);m.set('sp',sp);
 const calls=[];
 for(let tick=0;tick<ticks;tick++){
  audio=[];m.set('a0',sprite);m.call(0x8022efb0,0x80000100,200000);assert.equal(m.get('sp'),sp);steps+=m.steps;
  calls.push({record:read(),result:m.get('v0'),audio});
 }
 for(let i=0;i<16;i++){assert.equal(m.r8(sprite-16+i),0xa7);assert.equal(m.r8(sprite+80+i),0xa7);assert.equal(m.r8(program-16+i),0xa7);assert.equal(m.r8(program+data.length+i),0xa7);}
 return {bank,state,initial:initial.toString('hex'),calls};
}
const rows=[];
for(const source of OB64.cutsceneData.actorArtSources){
 const key=rom.readUInt32BE(source.descriptorKey+0x594284+4);
 const data=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded);
 const count=data.readUInt32BE(0)/4;
 for(let state=0;state<count;state++)rows.push(run(source.bank,state,data,123,-47));
}
const custom=[];
function customCase(commands,x,y){
 const data=Buffer.from([0,0,0,4,commands.length,...commands.flat()]);
 custom.push({...run(57,0,data,x,y,4),program:data.toString('hex')});
}
// Every non-branch opcode, signed-byte/word wrap, colors stay unchanged during
// delay, and state jumps do not overwrite the unrelated +0x46 halfword.
customCase([[2,128,127],[12,1,2,3],[13,255,200],[16,255,128],[13,15,17],[16,0,7],
 [6],[7],[8],[9],[10],[11],[14,4,5],[15,7],[21,0x34,0x12,3],[0]],-2147483648,2147483647);
customCase([[4,2],[2,1,1],[3,1],[5,0]],0,0);
for(const opcode of [17,18,19,20])for(const key of [0,0x24e,0x2f8])
 for(const y of [-2147483648,-1,105,106,159,160,212,213,2147483647])
  customCase([[opcode,key>>8,key&255],[1,7,3],[0]],999,y);
fs.writeFileSync(process.argv[3],JSON.stringify({rows,custom,steps}));
console.log(`Original sprite pose: ${rows.length*24} retail invocations and ${custom.length*4} boundary invocations; ${steps} instructions. Registry/audio sink controlled.`);
