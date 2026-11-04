'use strict';
// Original retail event VM, seeded game records, bounded before recruitment
// story setup. No emulator and no compiled editor build are needed.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {FloatMips}=require('./float-mips.cjs');
const root=path.resolve(__dirname,'../../..');
const master=process.env.LORDLY_TEST_ROM||path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
const z=fs.readFileSync(master);if(z.readUInt32BE(0)===0x37804012)z.swap16();assert.equal(z.readUInt32BE(0),0x80371240);
const corpus=JSON.parse(fs.readFileSync(path.join(root,'runs/all-event-semantics-20261103/corpus/rows/row-16.json')));
const program=Buffer.from(corpus.hex,'hex');assert.equal(crypto.createHash('sha256').update(program).digest('hex'),corpus.sha256.toLowerCase());
const results=[];
for(const entry of [330,338])for(const squad of [5,30])for(const member of [0,1])for(const hp of [0,100])for(const status of [0,4])for(const cf of [33,34]){
    const m=new FloatMips(),SP=0x807ff000,LOCAL=SP-144+80,PROGRAM=0x80700000;
    m.load(0x80163910,z.subarray(0x39810,0x66e10));m.load(PROGRAM,program);
    m.w8(0x801976e8,squad);m.w8(0x80197b00,4);m.w8(0x801936a9,cf);
    m.w8(0x801971f2+squad*25,member);
    const base=squad<30?0x80193bd8:0x80195578,stride=squad<30?56:52;
    m.w16(base+member*stride,hp);m.w8((squad<30?0x80193bf3:0x80195593)+member*stride,status);
    for(let r=0;r<8;r++)m.w16(LOCAL+r*2,0);
    let pc=entry,outcome=null;const trace=[];
    for(let count=0;count<45;count++){
        trace.push(pc);if(pc===388){outcome='recruitment-story';break;}
        const op=program[pc];m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);
        if(op===0x2f){m.call(0x80170954);outcome='finish';break;}
        m.call(0x80170954,0x80171b58,10000);pc=(m.r16(0x80197b04)+1)*2;
    }
    const eligible=(entry===338||squad===30)&&(!member||hp===0||!!(status&4))&&cf>33;
    assert.equal(outcome,eligible?'recruitment-story':'finish',JSON.stringify({entry,squad,member,hp,status,cf,trace}));
    results.push({entry,squad,member,hp,status,cf,outcome});
}
console.log('PASS',results.length,'original-instruction Liedel eligibility paths.');
console.log('Full entry330 requires squad30. Eligibility is no first member OR HP0 OR Petrification, followed by Chaos Frame>33.');
console.log('Army-bank cases enter the body at338 because the retail handler requires squad30. Stop before scene playback/recruitment.');
