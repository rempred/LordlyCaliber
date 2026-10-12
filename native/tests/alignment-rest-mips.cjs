'use strict';
// Execute whole exported kill-alignment and rest-healing owners. Alignment's
// unchanged eligibility helpers are controlled test services; healing runs all
// native member, fatigue, HP and Soldier-group branches without service traps.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
function rom(path) { const b=fs.readFileSync(path); if(b.readUInt32BE(0)===0x37804012)b.swap16(); return b; }
const edited=rom(process.argv[2]), retail=rom(process.argv[3]);
const cap=Number(process.argv[4]), healing=Number(process.argv[5]);
const clamp=(n,lo,hi)=>Math.max(lo,Math.min(hi,n));
function alignmentTerm(d) {const n=Math.abs(d);return Math.sign(d)*(n>75?5:n>50?4:n>25?3:n>5?2:n>0?1:0);}
function levelTerm(d) {return d<0?(d<-50?5:d<-25?4:d<-10?3:d<-3?2:1):d>0?-(d>=50?5:d>=25?4:d>=10?3:d>=3?2:1):0;}
let kills=0, rests=0;
// Retail only needs checking once; the default export is also tested separately.
for(const [bytes,limit,hpGain] of [...(cap===0?[[retail,5,1]]:[]),[edited,cap,healing]]) {
    const m=new Mips(), sp=0x803fff00, actor=0x803e0000, target=0x803e0100, unit=0x803e2000;
    m.load(0x801eff40,bytes.subarray(0x233210,0x2333b4));
    m.load(0x801f69d0,bytes.subarray(0x239ca0,0x239cd4));
    m.w32(0x801ce8bc,unit);
    let exclusion='';
    m.trap(0x801c8aec,c=>c.set('v0',exclusion==='training'?1:0));
    m.trap(0x801c8afc,c=>c.set('v0',exclusion==='script'?1:0));
    m.trap(0x801c8b68,c=>c.set('v0',exclusion==='enemy'?1:0));
    m.trap(0x801c8c74,c=>c.set('v0',(exclusion==='attackerSoldier'&&c.get('a0')===actor)||(exclusion==='targetSoldier'&&c.get('a0')===target)?1:0));
    function kill(a,t,l,tactic,excluded='') {
        exclusion=excluded;m.r.fill(0);m.set('sp',sp);m.set('s0',0x12345678);m.set('s1',0x23456789);
        m.set('a0',excluded==='nullAttacker'?0:actor);m.set('a1',excluded==='nullTarget'?0:target);
        m.w8(actor+0x34,a);m.w8(target+0x34,t);
        m.w8(actor+0x31,l<0?1:1+l);m.w8(target+0x31,l<0?1-l:1);
        m.w32(actor+0x48,excluded.startsWith('class')?Number(excluded.slice(5)):2);
        m.w8(0x801976dc,excluded==='selector'?30:0);m.w32(unit+0x1c0,tactic);
        const record=Buffer.from(m.ram.subarray(m.phys(actor),m.phys(target)+0x80));
        const expected=excluded?0:clamp(a+clamp(alignmentTerm(a-t)+levelTerm(l)+[-1,0,1,0][tactic],-limit,limit),0,100)-a;
        m.call(0x801eff40);
        assert.equal(m.get('v0')|0,expected,JSON.stringify({limit,a,t,l,tactic,excluded}));
        assert.equal(m.get('sp'),sp);assert.equal(m.get('s0'),0x12345678);assert.equal(m.get('s1'),0x23456789);
        assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(actor),m.phys(target)+0x80)),record);kills++;
    }
    for(const a of [0,1,5,6,25,26,50,51,75,76,95,99,100])
        for(const t of [0,1,5,6,25,26,50,51,75,76,95,99,100])
            for(const l of [-98,-51,-50,-26,-25,-11,-10,-4,-3,-1,0,1,2,3,9,10,24,25,49,50,98])
                for(let tactic=0;tactic<4;++tactic) kill(a,t,l,tactic);
    for(const reason of ['training','script','enemy','attackerSoldier','targetSoldier','selector','nullAttacker','nullTarget','class81','class82','class83']) kill(75,25,-50,2,reason);

    const h=new Mips();h.load(0x801df4a0,bytes.subarray(0x133be0,0x133ea0));
    function start(source,flags) {
        h.r.fill(0);h.set('sp',sp);h.set('s0',0x12345678);h.set('s1',0x23456789);h.set('s2',0x34567890);
        h.set('a0',unit);h.w32(unit,flags);h.w8(unit+4,source);
        for(let i=0;i<5;++i)h.w8(0x801971f2+source*25+i,0);
    }
    function run() {h.call(0x801df4a0);assert.equal(h.get('sp'),sp);assert.equal(h.get('s0'),0x12345678);assert.equal(h.get('s1'),0x23456789);assert.equal(h.get('s2'),0x34567890);rests++;}
    for(const source of [0,30]) for(const max of [1,100,999,65535])
        for(const hp of [...new Set([0,1,Math.max(1,max-50),max-1,max,...(max<65535?[max+1]:[])])])
            for(const fatigue of [0,1,2,3,100]) {
                start(source,0);const base=source?0x80195560:0x80193bc0,stride=source?52:56;
                const expected=[];
                for(let id=1;id<=4;++id) {
                    const r=base+id*stride;h.load(r,Buffer.alloc(stride,0x55));h.w8(r+0x12,id===4?0:2);
                    h.w16(r+0x16,max);h.w16(r+0x18,id===2?0:hp);h.w8(r+0x32,fatigue);h.w8(r+0x33,id===3?4:0);
                    h.w8(0x801971f2+source*25+id-1,id);
                    const record=Buffer.from(h.ram.subarray(h.phys(r),h.phys(r)+stride));
                    if(id===1&&hp>0) {record[0x32]=Math.max(0,fatigue-2);record.writeUInt16BE(hp<max?Math.min(max,hp+hpGain):hp,0x18);}
                    expected.push([r,record]);
                }
                run();assert.equal(h.get('v0'),hp>0?Math.max(0,fatigue-2):0);
                for(const [r,record] of expected)assert.deepEqual(Buffer.from(h.ram.subarray(h.phys(r),h.phys(r)+stride)),record,JSON.stringify({source,max,hp,fatigue,hpGain}));
            }
    // Soldier pools stop at native one-third/two-thirds/full-HP boundaries.
    for(const flags of [0,8]) for(const max of [3,100,999,65535]) {
        const third=Math.floor(max/3),base=flags?0x80190ebc:0x8019532c, status=flags?0x80191010:0x80195480;
        for(const hp of [...new Set([0,1,third-1,third,third+1,third*2,third*2+1,max-1,max,...(max<65535?[max+1]:[])])])for(const petrified of [0,4]) {
            start(0,flags);h.w8(0x801971f2,100);h.w16(base+198,max);h.w16(base+200,hp);h.w8(status+100,petrified);
            const bound=hp<=third?third:hp<=third*2?third*2:max;
            // At the artificial 65535 endpoint, retail's one-point Soldier
            // increment wraps before clamping. Keep exact retail behavior at
            // the default; enlarged healing must retain its full-width sum.
            const sum=hpGain>1?hp+hpGain:(hp+hpGain)&65535;
            const expected=hp>max?max:Math.min(bound,sum);
            run();assert.equal(h.r16(base+200),petrified?hp:expected,JSON.stringify({flags,max,hp,hpGain,petrified}));
            assert.equal(h.r16(base+198),max);assert.equal(h.r8(status+100),petrified);
        }
    }
}
console.log(`PASS ${kills} complete kill-alignment calculations/exclusions and ${rests} complete rest-healing calls; cap ${cap}, HP ${healing}. Checked endpoints, fatigue, ordinary/Soldier eligibility and maximum-HP overflow.`);
