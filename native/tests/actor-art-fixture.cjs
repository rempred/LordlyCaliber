'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['parsers.js','cutscene-data.js','cutscene-model.js','cutscene-assets.js','art.js','animation-art.js','cutscene-sprites.js']){
 let text=fs.readFileSync(path.join(root,f),'utf8');
 if(f==='cutscene-sprites.js')text=text.replace('  OB64.cutsceneSprites = {',
  '  OB64.testPrepareBank=prepareBank; OB64.testPrepareProgram=prepareProgram; OB64.testRenderProgramFrame=renderProgramFrame;\n  OB64.cutsceneSprites = {');
 vm.runInThisContext(text,{filename:f});
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex'),banks=[];
let programs=0,frames=0,layers=0;
for(const source of OB64.cutsceneData.actorArtSources){
 const state={z64:rom,banks:{},programs:{},frames:{},catalog:{getActorArtSource:()=>source}};
 const bank=OB64.testPrepareBank(state,source),p=Buffer.from(bank.pose),count=p.readUInt32BE(0)/4,poses=[];
 const tokens=new Set(),frameRows=[];
 for(let i=0;i<count;i++){
  const start=p.readUInt32BE(i*4),end=i+1<count?p.readUInt32BE(i*4+4):p.length;
  const pose=start===end?{program:p.subarray(start,end),records:[],frames:[]}:OB64.animationArt.parsePoseProgram(p,i,'bank '+source.bank);
  poses.push({sha256:hash(pose.program),records:pose.records.map(r=>[r.opcode,...r.operands])});programs++;
  for(const [token] of pose.frames)tokens.add(token);
 }
 for(const token of tokens){
  const program={bank:source.bank,programId:'test:'+token,frames:[{frameToken:token,durationFrames:1}],durationFrames:1};
  const prepared=OB64.testPrepareProgram(state,program),rendered=OB64.testRenderProgramFrame(state,prepared,0,0);
  frameRows.push({token,layers:rendered.nativeLayers.map(layer=>({x:layer.drawOffsetX,y:layer.drawOffsetY,width:layer.width,height:layer.height,
    flags:layer.flags,scaleX:layer.scaleX,scaleY:layer.scaleY,sha256:hash(layer.rgba)}))});
  frames++;layers+=rendered.nativeLayers.length;
 }
 banks.push({bank:source.bank,descriptorKey:source.descriptorKey,artKeys:bank.members.slice(4),poses,frames:frameRows});
}
fs.writeFileSync(process.argv[3],JSON.stringify({banks,programs,frames,layers}));
console.log(`Actor art: ${banks.length} banks, ${programs} physical programs, ${frames} frames, ${layers} layers.`);
