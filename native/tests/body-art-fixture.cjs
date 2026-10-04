'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),crypto=require('node:crypto');
global.window=global;global.OB64={};const root=path.resolve(__dirname,'../..');
for(const f of ['parsers.js','cutscene-data.js','cutscene-model.js','cutscene-assets.js','art.js','animation-art.js','cutscene-sprites.js']){
 let text=fs.readFileSync(path.join(root,f),'utf8');if(f==='cutscene-sprites.js')text=text.replace('  OB64.cutsceneSprites = {','  OB64.testPrepareBank=prepareBank; OB64.testPrepareProgram=prepareProgram; OB64.testRenderProgramFrame=renderProgramFrame;\n  OB64.cutsceneSprites = {');vm.runInThisContext(text,{filename:f});
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decoded=k=>Buffer.from(OB64.art.readCompressedResource(rom,k).decoded);
const handles=raw(0x315736),directory=raw(0x3b6cd0),base=handles.readUInt16BE(0),routes=[],banks=[],seen=new Set();
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');let programs=0,frames=0,layers=0;
for(let sourceClass=0;sourceClass<165;sourceClass++)for(let flags=0;flags<4;flags++){
 const handle=handles.readUInt16BE(base+(sourceClass*4+flags)*2),slot=(handle&0xfff)-1;
 if(slot<0||slot>=directory.length/4){routes.push({sourceClass,flags,handle,invalid:true});continue;}
 const descriptorKey=directory.readUInt32BE(slot*4),identity=descriptorKey+':'+(handle>>>12);
 routes.push({sourceClass,flags,handle,descriptorKey});if(seen.has(identity))continue;seen.add(identity);
 const members=raw(descriptorKey),controls=[0,4,8,12].map(i=>members.readUInt32BE(i)),resources=controls.map(decoded);
 const source={bank:banks.length+1,descriptorKey,descriptorMemberCount:members.length/4,metadataKey:controls[0],poseKey:controls[1],configKey:controls[2],lookupKey:controls[3],
  metadataDecodedLength:resources[0].length,poseDecodedLength:resources[1].length,configDecodedLength:resources[2].length,lookupDecodedLength:resources[3].length,
  artCount:members.length/4-4,lookupBankCount:resources[3].length/512};
 const context={z64:rom,banks:{},programs:{},frames:{},catalog:{getActorArtSource:()=>source}};
 const bank=OB64.testPrepareBank(context,source),pose=Buffer.from(bank.pose),count=pose.readUInt32BE(0)/4,poses=[],tokens=new Set();
 for(let i=0;i<count;i++){
  const start=pose.readUInt32BE(i*4),end=i+1<count?pose.readUInt32BE(i*4+4):pose.length;
  const p=start===end?{program:pose.subarray(start,end),records:[],frames:[]}:OB64.animationArt.parsePoseProgram(pose,i,'body');
  poses.push({hash:hash(p.program),records:p.records.map(r=>[r.opcode,...r.operands])});programs++;
  for(const [token]of p.frames)tokens.add(token);
 }
 const pictures=[];
 for(const token of tokens){
  const p=OB64.testPrepareProgram(context,{bank:source.bank,programId:'test:'+token,frames:[{frameToken:token,durationFrames:1}],durationFrames:1});
  const r=OB64.testRenderProgramFrame(context,p,0,handle>>>12);
  pictures.push({token,layers:r.nativeLayers.map(l=>({x:l.drawOffsetX,y:l.drawOffsetY,width:l.width,height:l.height,flags:l.flags,sx:l.scaleX,sy:l.scaleY,hash:hash(l.rgba)}))});frames++;layers+=r.nativeLayers.length;
 }
 banks.push({sourceClass,flags,handle,descriptorKey,poses,pictures});
}
fs.writeFileSync(process.argv[3],JSON.stringify({routes,banks,programs,frames,layers}));
console.log(`Body art: ${routes.length} class routes, ${banks.length} descriptor/variant combinations, ${programs} programs, ${frames} frames, ${layers} layers.`);
