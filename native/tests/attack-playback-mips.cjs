'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {AttackPlaybackMachine}=require('./attack-playback-machine.cjs');
const h=new AttackPlaybackMachine(process.argv[2]);
const seed=Number(process.env.LORDLY_PLAYBACK_SEED||0x4c43504c)>>>0;
const count=Number(process.env.LORDLY_PLAYBACK_CASES||128);
if(!Number.isInteger(count)||count<0||count>100000)throw Error('LORDLY_PLAYBACK_CASES must be an integer from 0 to 100000');
if(!seed)throw Error('LORDLY_PLAYBACK_SEED must be nonzero');
let state=seed;
function random(){state^=state<<13;state^=state>>>17;state^=state<<5;return state>>>0;}
const pick=a=>a[random()%a.length];
const cases=[{}, {after:1}, {after:2}, {after:1,sourceHP:500},
 {effect:2,side:2},{effect:4},{status:4},{status:8},{hp:1},{hp:60},
 {pattern:11,count:18,hp:2000,maxHP:4000},{hit:false},
 ...[1,2,30,254,255,260,300,510,520,700].map(impactDelay=>({impactDelay})),
 {dead:[9,10,11,12,13,14,15,16,17]},
 {effect:3,status:11,side:3,impactDelay:520}];
for(const route of ['melee','ranged','alternate'])for(const shared of [0,1])
 for(const effect of [1,2,4,10])for(const status of [0,4,16])
  cases.push({route,shared,effect,status,pattern:7,after:1,impactDelay:300});
for(const shifted of [false,true])for(const count of [1,6,18])
 for(const after of [0,1,2])cases.push({shifted,count,after,pattern:11,status:16,hp:2000,maxHP:4000,impactDelay:520});
for(const route of ['combined','pedra'])for(const shifted of [false,true])
 for(const effect of [1,2,3,4,5,6,8,10,11,16])
  cases.push({route,shifted,effect,pattern:11,count:18,status:4,after:1,hp:2000,maxHP:4000,impactDelay:520});
for(let i=0;i<count;i++) {
 const effect=pick([1,2,3,4,5,6,8,10,11,16]);
 cases.push({effect,pattern:pick([1,2,3,4,5,6,7,8,9,10,11,12]),side:effect===2?2:pick([1,2,3]),
  hp:pick([1,59,60,61,200,500]),sourceHP:pick([1,200,499,500]),power:pick([0,1,60,9999]),
  route:pick(['generic','generic','melee','ranged','alternate','combined','pedra']),shared:pick([0,1]),shifted:pick([false,true]),
  status:pick([0,3,4,5,6,8,10,11,16]),after:pick([0,1,2]),afterPercent:pick([0,1,25,100,1000]),
  afterBasis:pick([0,1,2,3]),chance:pick([0,1,50,99,100]),condition:pick([0,1,2]),random:pick([0,49,99]),
  count:pick([1,6,9,10,18]),hit:pick([true,false]),immune:pick([false,true]),
  impactDelay:pick([2,30,255,300,520]),impactGap:pick([1,3,10]),mask:pick([1,257,511])});
}
const replay=process.argv[3]?JSON.parse(fs.readFileSync(process.argv[3],'utf8')):null;
const selected=replay?[replay.minimized||replay.config||replay]:cases;
const failures=[],results=[];
for(const [index,config] of selected.entries()) {
 try{h.reset(config);h.generate();const result=h.playback();results.push({index,ticks:result.ticks,commands:result.commands,maxInstructions:result.maxInstructions});}
 catch(error){failures.push({index,config,error:error.message,commands:h.commands,trace:h.trace.slice(-80),presentations:h.presentations});}
}
// Minimize the first representative of each error. Keep both the original and
// reduced inputs; replay accepts either a failure object or a configuration.
const minimizedErrors=new Set();
for(const failure of failures) {
 const category=failure.error.split('\n')[0];
 if(minimizedErrors.has(category))continue;minimizedErrors.add(category);
 const reduced={...failure.config};
 for(const key of Object.keys(reduced)) {
  const attempt={...reduced};delete attempt[key];
  try{h.reset(attempt);h.generate();h.playback();}
  catch(error){if(error.message.split('\n')[0]===category)delete reduced[key];}
 }
 failure.minimized=reduced;
}
const report={format:'lordly-attack-playback',version:1,candidate:path.resolve(process.argv[2]),
 normalizedRomSha256:crypto.createHash('sha256').update(h.rom).digest('hex'),
 seed,cases:selected.length,passed:results.length,failures,results,
 scope:'Exported calculation, native queue insertion, real impacts/HP/status presentation commands and playback clocks; controlled scores, immunity, sprite/effect timing. No full console rendering.'};
const file=path.join(path.dirname(process.argv[2]),'attack-playback-report.json');
fs.writeFileSync(file,JSON.stringify(report,null,2)+'\n');
console.log(`Attack playback: ${results.length}/${selected.length} passed; seed ${seed}; report ${file}`);
for(const f of failures.slice(0,8))console.error(JSON.stringify({index:f.index,config:f.config,error:f.error}));
if(failures.length)process.exitCode=1;
