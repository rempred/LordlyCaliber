'use strict';
// Original ROM instructions are the independent oracle. Optional fixture data
// comes from the C++ models, not a second implementation of the emitters.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]); if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const fixture=process.argv[3]?JSON.parse(fs.readFileSync(process.argv[3],'utf8')):null;
const from64=s=>Buffer.from(s,'base64');
const groups=[Array.from({length:8},(_,i)=>0x45+i),[0x4e,0x4f,0x50],Array.from({length:13},(_,i)=>0x38+i),[0x31,0x34,0x35,0x37]];
const retailRows={9:[10,10,10,10],10:[0,10,10,10],14:[0,0,10,10],24:[15,10,10,10],90:[15,10,10,10],25:[0,15,10,10],123:[0,15,10,10],29:[0,0,15,10],43:[0,0,0,10],54:[0,0,0,10],116:[0,0,0,15]};
function combat(owner,rows) {
    const m=new Mips(),pool=0x80100000,actors=[0,1,2].map(i=>pool+0x1c4+i*0xf8),sp=0x803fff00;
    m.load(0x801f14e8,owner);
    for(const [off,address,size] of [[0x20c014,0x801c8b84,32],[0x20c788,0x801c92f8,136],[0x20c810,0x801c9380,140],[0x20c2c0,0x801c8e30,24],[0x20c32c,0x801c8e9c,28]])m.load(address,rom.subarray(off,off+size));
    m.w32(0x801ce8bc,pool); let calls=0;
    function call(leader,member,side,gate='',identity=leader) {
        m.ram.fill(0,m.phys(pool),m.phys(pool)+0x1800);
        for(const [i,cls,flags] of [[0,9,0x200|((side^1)<<8)],[1,leader,0x200|(side<<8)|(gate==='leader'?1:0)],[2,member,(side<<8)|(gate==='member'?2:0)]]) {
            m.w32(actors[i]+0x48,i===1?identity:cls);m.w32(actors[i]+0x4c,cls);m.w32(actors[i]+0x40,flags);
        }
        if(gate==='missing')m.w32(actors[1]+0x48,0);
        const before=Buffer.from(m.ram.subarray(m.phys(pool),m.phys(pool)+0x1800));
        m.r.fill(0); m.set('sp',sp);m.set('s0',0x12345678);m.set('s1',0x23456789);m.set('a0',actors[2]);
        m.call(0x801f14e8); const group=groups.findIndex(g=>g.includes(member));
        const expected=gate||!leader||group<0?0:(rows[leader]||[0,0,0,0])[group];
        assert.equal(m.get('v0')|0,expected,JSON.stringify({leader,member,side,gate,identity}));
        assert.equal(m.get('sp'),sp);assert.equal(m.get('s0'),0x12345678);assert.equal(m.get('s1'),0x23456789);
        assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(pool),m.phys(pool)+0x1800)),before);calls++;
    }
    for(let side=0;side<2;++side)for(let leader=1;leader<256;++leader)for(let member=1;member<256;++member)call(leader,member,side);
    for(const side of [0,1])for(const gate of ['leader','member','missing'])call(9,0x38,side,gate);
    // Identity IDs must not override the actor's current class.
    call(24,0x45,0,'',0x76);call(6,0x45,0,'',24);call(90,0x45,0,'',1);
    return calls;
}
console.log(`PASS retail combat: ${combat(rom.subarray(0x2347b8,0x2349d0),retailRows)} full calls, both sides and eligibility gates.`);
if(fixture) console.log(`PASS edited combat: ${combat(from64(fixture.combat.owner),fixture.combat.rows)} calls including signed and custom-class strengths.`);
const resource=rom.subarray(0x8b15b0,0x8b15b0+rom.readUInt32BE(0x8b15ac)),ranks=new Map;
for(let at=resource.readUInt32BE(0);resource[at]!==255;at+=2)ranks.set(resource[at],resource[at+1]);
const originalScore=(leader,member)=>{if(!leader)return 0;const a=ranks.get(leader)??255,b=ranks.get(member)??255;return a===99?60:a===b?80:Math.abs(a-b)===1?60:30;};
function persuasion(owner,resource,key,expected,loadsPerCall) {
    const m=new Mips(),ptr=0x80300000,sp=0x803fff00; let loads=0,frees=0,calls=0;
    m.load(0x801d1480,owner);m.load(ptr,resource);
    function clobber(c) {for(const reg of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(reg,0xdeadbeef);}
    m.trap(0x8009dbb8,c=>{assert.equal(c.get('a0'),0);assert.equal(c.get('a1'),key);clobber(c);c.set('v0',ptr);loads++;});
    m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),ptr);clobber(c);frees++;});
    for(let leader=0;leader<256;++leader)for(let member=0;member<256;++member) {
        const before=loads; m.r.fill(0);m.set('a0',leader);m.set('a1',member);m.set('sp',sp);
        for(let s=0;s<8;++s)m.set('s'+s,0x12345678+s);
        m.call(0x801d1480);assert.equal(m.get('v0'),expected(leader,member),JSON.stringify({leader,member}));
        assert.equal(loads-before,loadsPerCall(leader));assert.equal(loads,frees);assert.equal(m.get('sp'),sp);
        for(let s=0;s<8;++s)assert.equal(m.get('s'+s),0x12345678+s);
        assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(ptr),m.phys(ptr)+resource.length)),resource);calls++;
    }
    return calls;
}
console.log(`PASS retail persuasion: ${persuasion(rom.subarray(0x214750,0x21488c),resource,0x31d32c,originalScore,()=>2)} class pairs including absent ranks.`);
if(fixture)for(const sample of fixture.persuasion) {
    const expected=sample.retail?originalScore:(leader,member)=> {
        if(!leader)return 0; const data=sample.table;
        const group=data.groups.findIndex((g,i)=>i>0&&g.members.includes(member));
        return (data.leaders[leader]||data.fallback)[Math.max(group,0)];
    };
    console.log(`PASS ${sample.retail?'equivalent':'edited'} persuasion: ${persuasion(from64(sample.owner),from64(sample.runtime),sample.key,expected,l=>l?1:0)} calls; allocated data released, saved registers and input data preserved.`);
}
