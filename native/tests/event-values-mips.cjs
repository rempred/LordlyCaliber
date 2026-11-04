'use strict';
// Run the resident VM from the newly exported ROM, using its exported event row.
// Queueing recruitment is observed at the constructor boundary; no UI/emulator.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const directory=process.argv[2];if(!directory)throw Error('Supply the event-values export test directory');
const romFile=fs.readdirSync(directory).find(x=>x.endsWith('.v64'));
const rom=fs.readFileSync(path.join(directory,romFile));
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
assert.equal(rom.readUInt32BE(0),0x80371240);
const bytes=fs.readFileSync(path.join(directory,'event-10.bin'));
const m=new FloatMips(),SP=0x807ff000,LOCAL=SP-144+80,PROGRAM=0x80700000;
m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(PROGRAM,bytes);
for(let i=0;i<8;i++)m.w16(LOCAL+2*i,0);
function step(at){m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,at/2);m.call(0x80170954,0x80171b58,10000);}
let recruitment;m.trap(0x80172a28,c=>{recruitment=['a0','a1','a2','a3'].map(r=>c.get(r));});
step(0xa4);step(0xa6);assert.equal(m.r16(LOCAL),526);step(0xa8);
assert.deepEqual(recruitment,[0,1,526,0]);
step(106);assert.equal((m.r16(0x80197b04)+1)*2,136);
m.w16(LOCAL,0xff);step(102);assert.equal(m.r16(LOCAL),8);
const flagByte=0x80193686+(250>>3),flagMask=1<<(250&7);m.w8(flagByte,255);
step(0x424);step(0x426);step(0x428);assert.equal(m.r8(flagByte)&flagMask,0);
console.log('PASS exported native VM: recruitment template 526, branch to 136, arithmetic mask 8, flag 250 cleared. Recruitment construction, dialogue and full mission playback are outside this probe.');
