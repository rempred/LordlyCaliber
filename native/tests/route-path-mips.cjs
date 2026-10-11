'use strict';
// Compare the portable kernel against original ROM instructions, including all
// three priority-queue helpers. Only inputs and scheduling are host-provided.
const fs=require('node:fs'), assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const input=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
const rom=fs.readFileSync(input.rom); if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const rows=[];
for(const c of input.cases){
 const m=new Mips(), terrain=0x80300000, costs=0x80310000, aux=0x80320000,
 metadata=0x80321000, flags=0x80322000, distances=0x80324000, predecessors=0x80328000,
 queue=0x8032c000, mask=0x80340000;
 // Descriptor 7, authenticated retail layout (not the early-boot delta).
 m.load(0x801ad5a0,rom.subarray(0x101ce0,0x145210));
 m.load(terrain,Buffer.from(c.terrain,'hex'));
 c.costs.forEach((v,i)=>m.w16(costs+i*2,v));
 m.load(0x801eb520,Buffer.from(c.transitions,'hex'));
 m.w32(metadata+0x10,64);m.w32(metadata+0x14,64);m.w32(metadata+0x18,terrain);m.w32(metadata+0x1c,costs);
 m.w32(aux,c.start);m.w32(aux+4,c.goal);m.w32(aux+0x10,63);m.w32(aux+0x14,63);
 m.w32(aux+0x18,metadata);m.w32(aux+0x1c,flags);m.w32(aux+0x20,distances);
 m.w32(aux+0x24,predecessors);m.w32(aux+0x30,queue);
 if(c.blocked){m.load(mask,Buffer.from(c.blocked,'hex'));m.w32(aux+0x3c,mask);}
 m.w32(0x801e8670,90);
 let batches=0;
 do {
  m.set('sp',0x803ffe00);m.set('a0',aux);
  m.call(0x801b36f8,0x80000100,10000000);
  assert.equal(m.get('sp'),0x803ffe00);
  assert(++batches<100);
 }while(m.get('v0')===0);
 const reached=m.r8(flags+c.goal)!==0, path=[];
 if(reached){let at=c.goal;for(;;){path.push(at);assert(path.length<=4096);if(at===c.start)break;at=m.r32(predecessors+at*4);}path.reverse();}
 rows.push({name:c.name,reached,path,cost:reached?m.r32(distances+c.goal*4):0});
}
fs.writeFileSync(process.argv[3],JSON.stringify(rows));
console.log(`${rows.length} original pathfinder/queue comparisons; no pathfinding service doubles.`);
