'use strict';
// Execute maintained C with explicit cartridge, constructor and RNG services.
// This checks the extension core before its caller/loader installation exists.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const f=JSON.parse(fs.readFileSync(process.argv[2])),S=f.symbols,E=f.entries;
const state=S.lc_custom_state,profile=state+16,characters=S.lc_character_records,source=0x801971f0+49*25;
const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(f.address,Buffer.from(f.code,'hex'));
const sp=0x803ffe00,STOP=0x80000100;
function clobber(c){for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(r,0xdeadbeef);}
function call(name,args=[]){
 m.set('sp',sp);for(let i=0;i<args.length;i++)m.set('a'+i,args[i]);
 const saved=['s0','s1','s2','s3','s4','s5','s6','s7','fp'];
 saved.forEach((r,i)=>m.set(r,0xabcd1000+i));
 m.ram.fill(0xa5,m.phys(sp-0x300),m.phys(sp-0x280));
 m.call(E[name],STOP);
 saved.forEach((r,i)=>assert.equal(m.get(r),0xabcd1000+i));assert.equal(m.get('sp'),sp);
 for(let i=-0x300;i< -0x280;i++)assert.equal(m.r8(sp+i),0xa5);
 return m.get('v0');
}
function spec(key=33,slot=9){
 const p=Buffer.alloc(128);p[0]=1;p[1]=5;p[2]=1;p[3]=10;p[5]=key;p[6]=slot;
 const members=[[5,0,8,277,44],[71,255,6,16,277],[16,10,4,0,8],[165,128,2,30,0],[255,127,0,31,1]];
 members.forEach(([cl,level,cell,a,b],i)=>{const at=8+i*8;p[at]=cl;p[at+1]=level;p[at+2]=cell;p.writeUInt16BE(a,at+4);p.writeUInt16BE(b,at+6);});
 [0,1,2].forEach(i=>{p.writeUInt16BE([0x8115,44,0][i],56+i*4);p.writeUInt16BE([2,3,1][i],58+i*4);});
 p.write('@b@%0@n"Bandits!"@a@c\0',68,'ascii');return p;
}
function reset(p=spec()){
 m.ram.fill(0xa5,m.phys(state-16),m.phys(state+160));m.load(state,Buffer.alloc(144));m.load(profile,p);
}
function guard(){for(let i=-16;i<160;i++)if(i<0||i>=144)assert.equal(m.r8(state+i),0xa5);}
let selections=0,materializations=0,rewards=0;
for(let pool=1;pool<=45;pool++)for(let slot=0;slot<10;slot++)for(let side=0;side<2;side++){
 const key=(pool*7+slot*3)%256,p=spec(key,slot);reset();m.w8(S.lc_staged_scenario,key);let dma=0;
 m.trap(S.lc_dma_read,c=>{
  if(!dma){assert.equal(c.get('a0'),S.lc_custom_index+(key*10+slot)*4);assert.equal(c.get('a2'),4);c.w32(c.get('a1'),0x2900000);}
  else{assert.equal(c.get('a0'),0x2900000);assert.equal(c.get('a1'),profile);assert.equal(c.get('a2'),128);c.load(profile,p);}
  dma++;clobber(c);
 });
 assert.equal(call('lc_custom_select',[0,pool,0x801ed794+(pool-1)*20+slot*2+side]),5);
 assert.equal(dma,2);assert.equal(m.r32(state),1);assert.equal(m.r32(state+4),1);
 assert.equal(call('lc_custom_message'),profile+68);guard();selections++;
}
for(const [pool,pointer] of [[0,0],[46,0],[1,0x801ed793],[1,0x801ed7a8]]){
 reset();m.w32(state,1);m.w32(state+4,1);m.trap(S.lc_dma_read,()=>assert.fail('Invalid pair must not DMA'));
 assert.equal(call('lc_custom_select',[255,pool,pointer]),255);assert.equal(m.r32(state),0);assert.equal(m.r32(state+4),0);guard();selections++;
}
for(const bad of ['absent','disabled','short','long','key','slot','class']){
 reset();m.w8(S.lc_staged_scenario,33);const p=spec();let dma=0;
 if(bad==='disabled')p[0]=0;if(bad==='short')p[1]=1;if(bad==='long')p[1]=6;
 if(bad==='key')p[5]=32;if(bad==='slot')p[6]=8;if(bad==='class')p[8]=1;
 m.trap(S.lc_dma_read,c=>{if(!dma)c.w32(c.get('a1'),bad==='absent'?0:0x2900000);else c.load(c.get('a1'),p);dma++;clobber(c);});
 assert.equal(call('lc_custom_select',[71,41,0x801ed794+40*20+18]),71);
 assert.equal(m.r32(state+4),0);assert.equal(call('lc_custom_message'),0);assert.equal(dma,bad==='absent'?1:2);guard();selections++;
}
for(const available of [0,1,2,3,4,5])for(const base of [0,1,10,99,250,255]){
 const p=spec(),free=[1,3,50,118,119].slice(0,available);reset(p);m.w32(state,1);m.w32(state+4,1);
 const original=Buffer.alloc(120*52,0xaa);for(let id=0;id<120;id++)original[id*52+17]=free.includes(id)?0:1;
 m.load(characters,original);m.load(source-8,Buffer.alloc(41,0x55));const before=Buffer.from(m.ram.slice(m.phys(source-8),m.phys(source+33)));
 const calls=[];
 m.trap(S.lc_character_create,c=>{
  calls.push([c.get('a0'),c.get('a1'),c.get('a2'),c.get('a3'),c.r32(c.get('sp')+16)]);
  assert.ok(free.includes((c.get('a0')-characters)/52));c.w8(c.get('a0')+17,1);clobber(c);
 });
 assert.equal(call('lc_custom_materialize',[source,base]),available===5?1:0);
 assert.equal(calls.length,available===5?5:0);assert.equal(m.r32(state),0);assert.equal(m.r32(state+4),available===5?1:0);
 if(available===5){
  for(let i=0;i<5;i++){const at=8+i*8;
   assert.deepEqual(calls[i],[characters+free[i]*52,p[at],(base+p[at+1])&255,p.readUInt16BE(at+4),p.readUInt16BE(at+6)]);
   assert.equal(m.r8(source+2+i),free[i]);assert.equal(m.r8(source+7+i),p[at+2]);original[free[i]*52+17]=1;
   before[8+2+i]=free[i];before[8+7+i]=p[at+2];
  }
 }
 assert.deepEqual(Buffer.from(m.ram.slice(m.phys(characters),m.phys(characters+original.length))),original);
 assert.deepEqual(Buffer.from(m.ram.slice(m.phys(source-8),m.phys(source+33))),before);guard();materializations++;
}
for(const count of [0,1,6,255]){
 reset();m.w32(state,1);m.w32(state+4,1);m.w8(profile+1,count);
 m.trap(S.lc_character_create,()=>assert.fail('Invalid count must not construct'));
 assert.equal(call('lc_custom_materialize',[source,10]),0);assert.equal(m.r32(state+4),0);guard();materializations++;
}
for(const weights of [[2,3,1],[0,0,1],[1,0,0],[0,1,0],[65535,65535,65535],[0,0,0]])for(const persuaded of [0,1]){
 const total=weights.reduce((a,b)=>a+b,0);
 for(const random of [0,1,2,3,5,6,65534,65535,131070,0x3fffffff]){
  const p=spec();weights.forEach((w,i)=>p.writeUInt16BE(w,58+i*4));reset(p);
  m.w32(state,1);m.w32(state+4,1);m.w32(state+8,persuaded);m.w32(state+12,0x80123456);let rolls=0;
  m.trap(S.lc_random,c=>{rolls++;clobber(c);c.set('v0',random);});
  let expected=0;
  if(!persuaded&&total){const roll=random%total;expected=[0x8115,44,0][roll<weights[0]?0:roll<weights[0]+weights[1]?1:2];}
  assert.equal(call('lc_custom_reward'),expected);assert.equal(rolls,!persuaded&&total?1:0);
  for(let at=0;at<16;at+=4)assert.equal(m.r32(state+at),0);
  assert.equal(call('lc_custom_reward'),0xffffffff);assert.equal(call('lc_custom_message'),0);guard();rewards++;
 }
}
console.log(`PASS custom encounter C core: ${selections} selections, ${materializations} materialization/capacity cases, ${rewards} weighted/persuasion reward cases; independent per-member equipment and byte-wrapped levels, guarded 4 MiB RAM. Caller installation, original constructor behavior and full battles are not covered yet.`);
