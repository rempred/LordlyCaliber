'use strict';
// Exercise the exported producer, native queue and playback name consumers.
// Actor discovery, scoring and visual timing are controlled; this tests label
// identity, not full-console rendering or effect synchronization.
const assert=require('node:assert/strict');
const {AttackPlaybackMachine}=require('./attack-playback-machine.cjs');
const h=new AttackPlaybackMachine(process.argv[2]),m=h.m;
const text=at=>{let value='';while(m.r8(at)){assert(value.length<128);value+=String.fromCharCode(m.r8(at++));}return value;};
// Queue stress deliberately substitutes the inverse name lookup. This check
// must execute the real one, including its native/independent-name distinction.
m.traps.delete(0x801be034);
let displayed=[];
m.trap(h.e.func_001F2BC0,c=>displayed.push(text(c.get('a0'))));
const ids=[25];
const count=h.rom.readUInt32BE(0x27c5300+36);
for(let id=159;id<count;id++) {
 const kind=m.r8(h.records+id*16+4);
 const donor=m.r8(h.settings+id*32);
 // Spell arrays choose another action before the producer runs.
 const array=(donor>=45&&donor<=54)||donor===145;
 if(kind>=2 && !array && !m.r8(h.settings+id*32+20))ids.push(id);
}
const shifted=h.rom.readUInt32BE(0x27a0004)===23?[false,true]:[false];
let cases=0;
for(const id of ids)for(const enemy of [false,true])for(const shift of shifted) {
 h.reset({action:id,retail:true,single:true,identity:73,current:73,enemy,shifted:shift});
 const expected=text(m.r32(h.records+id*16));
 const commands=h.generate();
 assert(commands.some(x=>x.type===11),`attack ${id} generated a battle label`);
 displayed=[];
 h.playback();
 assert.deepEqual(displayed,[expected],`attack ${id}, enemy=${enemy}, shifted=${shift}`);
 assert(h.presentations.some(x=>x.kind===14&&x.amount===id),'action log preserves attack identity');
 console.log(`PASS name flow ${id}: ${expected}; ${enemy?'enemy':'player'}; ${shift?'expanded':'ordinary'} stream`);
 cases++;
}
console.log(`PASS ${cases} complete battle-name paths (controlled visual timing).`);
