'use strict';
// Execute the exported town handler through the original resident VM.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const directory=process.argv[2];if(!directory)throw Error('Supply the grouped event export test directory');
const rom=fs.readFileSync(path.join(directory,fs.readdirSync(directory).find(x=>x.endsWith('.v64'))));
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();assert.equal(rom.readUInt32BE(0),0x80371240);
const program=fs.readFileSync(path.join(directory,'event-4.bin'));
const results=[];
for(const town of [1,2,3,4]){
    const m=new FloatMips(),SP=0x807ff000,LOCAL=SP-144+80,PROGRAM=0x80700000;
    m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(PROGRAM,program);
    for(let r=0;r<8;r++)m.w16(LOCAL+r*2,0);
    // The scheduler stores a zero-based site; property F8 adds one.
    m.w8(0x801976e8,town-1);m.w8(0x80197b00,2);m.w32(0x80197b14,0);
    let pc=0xa8,scene=null,returned=false;
    for(let steps=0;steps<12;steps++){
        const op=program[pc];m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);
        if(op===0x2f){m.call(0x80170954);returned=true;break;}
        if(op===0x10){m.call(0x80170954);scene=m.r16(0x8018f1a0)&0x3fff;pc+=2;continue;}
        if(op===0x11){assert.equal(m.r16(LOCAL)&255,0);m.call(0x80170954);returned=true;break;}
        m.call(0x80170954,0x80171b58,10000);pc=(m.r16(0x80197b04)+1)*2;
    }
    assert(returned);assert.equal(scene,town===3?611:null);results.push({town,scene,returned});
}
console.log('PASS exported town event:',JSON.stringify(results));
console.log('Native town test, branches, selector preparation, cutscene request and return executed. Rendering the cutscene and full mission scheduling are not exercised.');
