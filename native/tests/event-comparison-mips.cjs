'use strict';
// Original interpreter comparison handlers; no build, ROM mutation or emulator.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=process.env.LORDLY_TEST_ROM||path.resolve(__dirname,'../../../Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
const z=fs.readFileSync(rom);if(z.readUInt32BE(0)===0x37804012)z.swap16();assert.equal(z.readUInt32BE(0),0x80371240);
const m=new FloatMips(),SP=0x807ff000,LOCAL=SP-144+80,PROGRAM=0x80700000;
m.load(0x80163910,z.subarray(0x39810,0x66e10));let cases=0;
const signed=value=>value<<16>>16;
for(const family of [10,11,12,13,14,15])for(const low of [1,2,3,6,7,11,12,13,14,15])
for(const registers of [[0,0,0,0],[1,2,3,4],[4,3,2,1],[0xffff,0x8000,0x7fff,0],[0x8000,0xffff,0,0x7fff]])for(const distance of [3,-3]) {
    const opcode=family<<4|low,single=low>=12,left=single?low&3:low>>2&3,right=low&3;
    let a=registers[left],b=single?0:registers[right];if(family>=12){a=signed(a);b=signed(b);}
    const expected=family===10?a===b:family===11?a!==b:family===12?a>b:family===13?a<b:family===14?a>=b:a<=b;
    m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,0);
    for(let r=0;r<8;r++)m.w16(LOCAL+r*2,registers[r]||0);
    m.load(PROGRAM,Buffer.from([opcode,distance&255]));
    m.call(0x80170954,0x80171b58,10000);
    assert.equal(m.r16(0x80197b04),expected?distance&65535:0,`opcode ${opcode.toString(16)}, values ${registers}, distance ${distance}`);
    for(let r=0;r<4;r++)assert.equal(m.r16(LOCAL+r*2),registers[r]);
    cases++;
}
console.log(`PASS ${cases} native comparison branches: six relations, ten register layouts, zero/signed boundary values and forward/backward displacements.`);
console.log('Original instruction mechanics only. New editor persistence/export tests are source additions pending the build hold.');
