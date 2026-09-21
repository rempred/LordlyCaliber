// ROM-derived fresh mode-two preview setup. No captured memory or Actor records.
window.OB64=window.OB64||{};
(function(O){
 'use strict';
 const hex=b=>Array.from(b,v=>v.toString(16).padStart(2,'0')).join('');
 function fail(s,code){const e=new Error(s);e.code=code||'rom-start-input';throw e;}
 function resource(rom,key){const p=key+0x594280,v=new DataView(rom.buffer,rom.byteOffset,rom.byteLength);if(p<0||p+4>rom.length)fail('Resource key exceeds the loaded ROM.');const n=v.getUint32(p);if(!n||n>65536||p+4+n>rom.length)fail('Resource extent exceeds the preview bound.');return rom.slice(p+4,p+4+n);}
 // func_00283E14 maps terminal class 1 to route -3. func_002827EC then
 // installs scene mode two and callback 0x802260F0. These are separate values.
 function analyze(program){
  const reject=(code,reason)=>({supported:false,code,reason});
  if(!program||!Array.isArray(program.primitives)||!program.primitives.length)return reject('rom-start-stream','ROM startup requires a decoded Director stream.');
  if(program.primitives.some(n=>!Array.isArray(n.rawWords)||!n.rawWords.length||n.opcode!==n.rawWords[0]||n.rawWords.some(w=>!Number.isInteger(w)||w<0||w>0xffffffff)))return reject('rom-start-stream','ROM startup requires complete unsigned Director words.');
  const words=program.primitives.flatMap(n=>n.rawWords),last=words.at(-1);
  if(words.length>16384||(last>>>24)!==255)return reject('rom-start-trailer','ROM startup requires the native terminal resource class.');
  const terminal=program.primitives.at(-1);
  if(terminal.opcode!==0x80000001||terminal.rawWords.length!==2||program.primitives.filter(n=>n.opcode===0x80000001).length!==1)return reject('rom-start-trailer','ROM startup requires one final terminal command and resource class.');
  const terminalClass=last&255;
  if(terminalClass===2){if(program.primitives.some(n=>[0x80000006,0x80000007,0x80000008].includes(n.opcode)))return reject('rom-start-preserved-setup','Preserved Stage preview does not implement an additional environment setup command.');return {supported:true,terminalClass,sceneMode:2,environmentSelector:0,mapKind:0,preservedStage:true};}
  if(terminalClass!==1)return reject('rom-start-class','ROM startup does not implement terminal resource class '+terminalClass+' and its caller setup.');
  // The word-wise pre-scan must retain missing fields as unresolved. In
  // particular, an absent environment command must not become environment zero.
  let flags=0,environmentSelector=null;
  for(let i=0;i<words.length;i++){
   if(words[i]===0x80000008)flags|=4;
   if(words[i]===0x80000007)flags|=2;
   if(words[i]===0x80000006){if(i+1>=words.length)return reject('rom-start-environment','Environment pre-scan has no operand.');flags|=1;environmentSelector=words[i+1]<<16>>16;}
   if(flags===4||words[i]===0x80000001)break;
  }
  if(environmentSelector===null)return reject('rom-start-inherited-stage','ROM startup requires inherited Stage and caller state because this stream supplies no environment.');
  if(environmentSelector<0)return reject('rom-start-derived-environment','ROM startup requires the caller environment mapper for environment sentinel '+environmentSelector+'.');
  if(environmentSelector>=80)return reject('rom-start-environment','ROM startup environment exceeds the supported ROM table.');
  if(flags!==1)return reject('rom-start-prescan','ROM startup does not implement the secondary or stop pre-scan route.');
  const setups=program.primitives.filter(n=>n.opcode===0x80000006);
  if(setups.length!==1||program.primitives[0]!==setups[0]||setups[0].rawWords.length!==2||setups[0].rawWords[1]!==environmentSelector)return reject('rom-start-stage-sequence','ROM startup requires one initial environment command; delayed or repeated Stage construction is not implemented.');
  return {supported:true,terminalClass,sceneMode:2,environmentSelector,mapKind:24};
 }
 function supports(scene,program){return !!scene&&analyze(program).supported;}
 function qualify(rom){
  // Source-derived caller and name-binding rules must match the current ROM.
  for(const row of O.cutsceneRomStartData.contracts){if(hex(rom.subarray(row.rom,row.rom+row.hex.length/2))!==row.hex)fail('ROM startup caller or dialogue rules differ from the qualified ROM: '+row.name+'.','rom-start-code');}
 }
 function input(rom,scene,program){

  if(!scene||!program)return null;
  const contract=analyze(program);if(!contract.supported)fail(contract.reason,contract.code);
  qualify(rom);
  const environmentSelector=contract.environmentSelector;
  const directory=resource(rom,0x019a8804),dv=new DataView(directory.buffer),key=parseInt(scene.directorKey,16);let selector=-1;
  if(directory.length%4)fail('Director directory is not word aligned.','rom-start-directory');
  for(let i=0;i<directory.length;i+=4)if(dv.getUint32(i)===key){selector=i/4;break;}if(selector<0)fail('Selected Director resource is absent from the loaded ROM directory.','rom-start-directory');
  const memory=[],add=(address,length)=>{const bytes=new Uint8Array(length);memory.push({address,bytes});return new DataView(bytes.buffer);};
  const region=(a,n=4)=>{const r=memory.find(r=>a>=r.address&&a+n<=r.address+r.bytes.length);if(!r)fail('Fresh setup field is outside its allocated region.');return [new DataView(r.bytes.buffer),a-r.address];};
  const put=(a,v,n=4)=>{const[b,i]=region(a,n);n===1?b.setUint8(i,v):n===2?b.setUint16(i,v):b.setUint32(i,v);};
  // Clean isolated pool, controller, scratch, audio queue and preview allocation state.
  [[0x800af0a6,2],[0x800af0c0,8],[0x800a8740,4],[0x800c4800,0x460],[0x800e7900,0x2400],[0x8018f500,16],[0x8018fc70,8],[0x80190f74,2],[0x80193bf8,8],[0x8019ee30,32],[0x80380000,8192],[0x80350000,8192],[0x80383400,17],[0x807fe000,8192]].forEach(r=>add(...r));
  put(0x800c4c20,0xffffffff);put(0x800c4c26,0xffff,2);put(0x800c49d0,1,2);put(0x800e82c8,0xc000,2);put(0x800e82cc,6,1);put(0x800e82c8+0x10,contract.preservedStage?0x80226324:0x802260f0);
  put(0x800e8108,0x800e79b0);put(0x800c4bdc,0x800e8100);put(0x800c4c4c,0x800e8700);put(0x80190f74,0xffff,2);put(0x800e9c0c,150,2);
  // Metric arrays are retail resources. Preview addresses are owned allocations.
  for(const [address,key]of [[0x80383000,0x0218c450],[0x80383100,0x0218de48]]){const bytes=resource(rom,key);memory.push({address,bytes});}
  put(0x8018fc70,0x80383000);put(0x8018fc74,0x80383100);
  memory.push({address:0x8019e180,bytes:rom.slice(0xeaf00,0xebbb0)});
  const launch={kind:'rom-mode-two-director-v1',preservedStage:contract.preservedStage===true,sceneMode:contract.sceneMode,selector,environmentSelector,proximityFlags:0,actorPresentationWord:0,previewHeroName:'Magnus',world:{mapKind:contract.mapKind,scenarioByte:0,eventState:0,red:255,green:255,blue:255,alternateContextPointer:0},arena:{address:0x80360000,byteLength:49152},cameraBaseHex:hex(rom.slice(0x2866f0,0x2866f0+144)),audioQueueHex:'00'.repeat(128),operandTranslations:{}};
  return {schema:'ob64-cutscene-launch-inputs.v1',assetId:scene.assetId,invocationId:'rom-start',sourceIdentity:contract.preservedStage?'Loaded ROM Stage; declared level-one Hero and Fighter preview unit, environment 0':'Loaded ROM startup; isolated preview pool and empty playthrough roster',evidenceGrade:'Candidate',schedulerBranch:{status:'known',value:'normal'},externalProducers:{status:'known',value:{throughTick:29999,events:[],menuCreates:[],colorCreates:[],poseCalls:[],initialColor:null,initialDialogue:{memory:memory.map(r=>({address:r.address,hex:hex(r.bytes),writable:true})),owners:[{ownerId:'rom-director',payloadHex:null},null,null,null,null,null],payloadStorage:{kind:'preview-arena-v1',address:0x80380000,byteLength:8192},lifecycle:{kind:'shared-dialogue-v1',computeAlignment:true,archiveArena:{address:0x80350000,byteLength:8192}}},resourceSchedule:{kind:'resident-resource-pass-v1',directorSlot:0,directorCallback:0x80226190,directorBinding:'rom-mode-two-v1',pageAdvancePolicy:'automatic',controller:{throughPass:29999,changes:[{pass:0,actionMask:0,directionMask:0,historyMask:0,dummyMask:0}]},helperOutcomes:[]},directorLaunch:launch}}};
 }
 function installCode(code,regions,rom){qualify(rom);const v=new DataView(rom.buffer,rom.byteOffset,rom.byteLength);for(const [a,p,w]of O.cutsceneRomStartData.words){if(v.getUint32(p)!==w)fail('Mode-two initialization code differs from the qualified ROM.');code[a]=w;}for(const r of O.cutsceneRomStartData.constants)regions.push({address:r.address,bytes:rom.slice(r.rom,r.rom+r.length),writable:false});}
 function attachLaunch(l){
  const m=l.machine;
  // Class definitions are ROM data. Actor-input rows start empty in this isolated preview.
  m.regions.push({address:0x80187c20,bytes:l.rom.slice(0x5db20,0x5db20+165*72),writable:false});
  m.regions.push({address:0x8018c400,bytes:l.rom.slice(0x62300,0x62300+256*32),writable:false});
  m.regions.push({address:0x801ce8bc,bytes:new Uint8Array(8),writable:true},{address:0x801ce8fc,bytes:new Uint8Array(4),writable:true});
  l.poseRegistry=[];
  const view=new DataView(l.rom.buffer,l.rom.byteOffset,l.rom.byteLength);
  for(const data of [O.cutsceneImageEchoCode,O.cutsceneSharedActorCode]){
   for(const r of data.words){if(!(r[0]<0x800a0000||(r[0]>=0x8022d534&&r[0]<0x8022d928)||(r[0]>=0x80233ae0&&r[0]<0x80233b74)))continue;if(view.getUint32(r[1])!==r[2])fail('Projection code differs from the qualified ROM.');m.code[r[0]]=r[2];}
   for(const r of data.tables){const b=Uint8Array.from(r.hex.match(/../g),x=>parseInt(x,16));if(b.some((v,i)=>v!==l.rom[r.rom+i]))fail('Projection constants differ from the qualified ROM.');let start=-1;for(let i=0;i<=b.length;i++){const old=i<b.length&&m.regions.find(x=>r.address+i>=x.address&&r.address+i<x.address+x.bytes.length);if(old&&old.bytes[r.address+i-old.address]!==b[i])fail('Projection constants conflict.');if(i<b.length&&!old){if(start<0)start=i;}else if(start>=0){m.regions.push({address:r.address+start,bytes:b.slice(start,i),writable:false});start=-1;}}}
  }
  m.regions.push({address:0x801d06fc,bytes:new Uint8Array(0x144),writable:true});
  O.cutsceneImageEcho.installFpu(m);
 }

 function preparePose(l,source,owner,flagA,flagB,equipment){
  if(source<0||source>=165||owner<0||owner>=165||![0,1].includes(flagA)||![0,1].includes(flagB))fail('Unsupported class-body registration tuple.');
  const handles=resource(l.rom,0x315736),hv=new DataView(handles.buffer),base=l.rom[0x5db59+source*72]===owner?source:owner,index=4*base+2*flagA+flagB,raw=hv.getUint16(hv.getUint16(0)+index*2),handle=raw&4095;
  if(!handle)fail('Class-body registration selected an empty handle.');
  let row=l.poseRegistry.find(r=>r.sourceArt===source&&r.ownerContext===owner&&r.flagA===flagA&&r.flagB===flagB&&r.equipment===equipment);if(row)return row;
  if(l.poseRegistry.length>=20)fail('Class-body preview registration pool is full.');
  const directory=resource(l.rom,0x3b6cd0),v=new DataView(directory.buffer);if(handle*4>directory.length)fail('Class-body handle exceeds descriptor directory.');
  const descriptorKey=v.getUint32(handle*4-4),descriptor=resource(l.rom,descriptorKey),poseKey=new DataView(descriptor.buffer).getUint32(4),pose=O.cutsceneCodec.decodeCustomLz(resource(l.rom,poseKey),{requireExact:false,maxOutput:65536}).bytes;
  const pv=new DataView(pose.buffer),count=pv.getUint32(0)/4;if(!Number.isInteger(count)||count<1||count*4>pose.length)fail('Class-body pose directory is invalid.');
  const programs=[];for(let i=0;i<count;i++){const start=pv.getUint32(i*4),end=i+1<count?pv.getUint32(i*4+4):pose.length;if(start<count*4||end<start||end>pose.length)fail('Class-body program extent is invalid.');programs.push({state:i,programHex:hex(pose.slice(start,end))});}
  row={sourceArt:source,ownerContext:owner,flagA,flagB,equipment,handle,descriptorKey,poseKey,programs};l.poseRegistry.push(row);return row;
 }
 function bodyPose(l,words,records){
  const m=l.machine,root=m.get(0x8022a974);
  if(!m.get(0x801ce8bc)){const linked=l.allocate(0x6094);l.write(linked,new Uint8Array(0x6094));m.put(0x801ce8bc,linked);m.put(0x801ce8c0,linked+0x57e0);m.put(linked+0x6084,1,1);}
  for(const r of records){const a=m.get(root+24+r.slot*4);if(!a)fail('Body initialization requires its current Actor allocation.');l.write(a,r.bytes);}
  const original=m.serviceHelper;let prepared=false,poseCalls=0;
  m.serviceHelper=function(pc){
   if(pc===0x8009c970){m.region(m.r[4],m.r[6],true);for(let i=0;i<m.r[6];i++)m.put(m.r[4]+i,m.r[5],1);return true;}
   if(pc===0x801c41c8){
    const art=m.get(m.r[4]),owner=m.get(m.r[5]),flagB=m.get(m.r[6]),flagA=m.get(m.r[7]),equipment=m.get(m.get(m.r[29]+16));
    if(m.get(m.r[29]+20)!==1||art>=57&&art<=70)fail('Body preparation requires an implemented ordinary single-resource variant.');
    preparePose(l,art,owner,flagA,flagB,equipment);prepared=true;return true;
   }
   if(pc===0x8022e9e8){if(!prepared)fail('Immediate pose requires successful ROM registration.');poseCalls++;return true;}
   return original(pc);
  };
  try{const g=m.run(0x80239c04,[words[1],words[2],words[6],words[3],words[4],words[5],0,0],[],65536);let r;do{r=g.next();}while(!r.done);}finally{m.serviceHelper=original;}
  if(poseCalls!==1)fail('Body initialization requires a single immediate pose in this profile.');
  return l.read(m.get(root+24*1+words[1]*4),336);
 }
 function projection(l,target,duration){
  const m=l.machine,b=new DataView(new ArrayBuffer(4)),word=x=>{b.setFloat32(0,x);return b.getUint32(0);};
  if(target){m.f[12]=word(target.translateX);m.f[14]=word(target.translateY);const g=m.run(0x80239a84,[0,0,word(target.scaleX),word(target.scaleY),duration],[],4096);while(!g.next().done){}}
  else {const g=m.run(0x80239af8,[],[],4096);while(!g.next().done){}}
  return projectionState(l);
 }
 function projectionState(l){const m=l.machine,p=m.get(m.get(0x8022a974)+0x19f0),b=new DataView(l.read(p,76).buffer);return {translateX:b.getFloat32(0x30),translateY:b.getFloat32(0x34),scaleX:b.getFloat32(0x24),scaleY:b.getFloat32(0x28)};}
 function prepareDialogue(l,engine,slot){
  // func_002A0088 installs four text substitutions before the constructor.
  // Binding 0 is the name found by primary classes 0x51, 0x52, then 0x53.
  // This name-only preview default creates no gameplay or Actor roster row.
  const name=l.input.previewHeroName;
  if(typeof name!=='string'||!name.length||name.length>16||!/^[\x20-\x7e]+$/.test(name))fail('The preview protagonist name must contain 1-16 printable ASCII bytes.','rom-start-player-name');
  const m=engine.machine,nameAddress=0x80383400;
  for(let i=0;i<17;i++)m.put(nameAddress+i,i<name.length?name.charCodeAt(i):0,1);
  const root=l.machine.get(0x8022a974);
  if(!Number.isInteger(slot)||slot<0||slot>=28)fail('Dialogue name setup requires a valid current Actor slot.','rom-start-dialogue-name');
  const actor=l.machine.get(root+24+slot*4);
  let actorNameAbsent=!actor;
  if(actor){const index=l.machine.get(actor+0x147,1);actorNameAbsent=index>=20;
   if(!actorNameAbsent){const scene=l.machine.get(0x801ce8bc);if(!scene)fail('Dialogue name setup requires the current scene roster.','rom-start-dialogue-name');actorNameAbsent=l.machine.get(scene+0x1c4+index*0xf8+0x48)===0;}}
  m.romTextBindingStatus=[null,actorNameAbsent?null:'the current Actor roster name and suffixes','the player army name','the selected unit leader name'];
  for(let i=0;i<4;i++)m.put(0x8018f500+i*4,i===0||i===1&&actorNameAbsent?nameAddress:0);
  if(!m.romTextBindingGuard){const get=m.get;m.get=function(a,n){if((n===undefined||n===4)&&a>=0x8018f500&&a<0x8018f510&&a%4===0){const reason=this.romTextBindingStatus[(a-0x8018f500)/4];if(reason)fail('Dialogue substitution '+((a-0x8018f500)/4)+' requires '+reason+'.','rom-start-dialogue-name');}return get.call(this,a,n);};m.romTextBindingGuard=true;}
 }
 function dialoguePoint(l,actor,camera){
  // func_002AA3B0 supplies world scale; func_002A9AD0 composes the Actor
  // origin before packing. With zero record rotations, only translation
  // affects the zero point consumed by func_0029CD64. No captured matrix enters.
  const record=actor.nativeRecordBase||actor.source&&actor.source.recordHex;if(record){const v=new DataView(Uint8Array.from(record.match(/../g),x=>parseInt(x,16)).buffer);if([0x110,0x114,0x118].some(at=>v.getFloat32(at)!==0))fail('Dialogue origin requires supported Actor rotation.');}
  if(l.machine.get(l.machine.get(0x8022a974)+0x88+actor.slot*4))fail('Dialogue origin requires its current depth attachment.');
  if(actor.heightModeByte&1&&!l.stage)fail('Dialogue origin requires the current terrain height.');
  const m=l.machine,b=new DataView(new ArrayBuffer(4)),put=(a,v)=>{b.setFloat32(0,v);m.put(a,b.getUint32(0));};
  const input=0x807fe100,out=0x807fe120,mat=0x807fe140;
  for(let i=0;i<12;i++)m.put(input+i,0,1);
  const y=actor.heightModeByte&1?l.stage.height(actor.x,actor.z):actor.heightModeByte&4?actor.y+actor.secondaryY:actor.heightModeByte&2?actor.secondaryY:actor.y;
  const scale=new DataView(l.read(0x801ce8e4,4).buffer).getFloat32(0);
  const values=[1,0,0,0,0,1,0,0,0,0,1,0,Math.fround(actor.x*scale),Math.fround(y*scale),Math.fround(actor.z*scale),1];
  values.forEach((v,i)=>{const n=Math.trunc(Math.fround(v*65536));m.put(mat+i*2,n>>>16,2);m.put(mat+32+i*2,n&65535,2);});
  for(const [a,v]of [[0x801d0720,camera.eye.x],[0x801d072c,camera.eye.y],[0x801d0704,camera.eye.z],[0x801d0730,camera.target.x],[0x801d083c,camera.target.y],[0x801d0700,camera.target.z],[0x801d0710,camera.fovYDegrees]])put(a,v);
  const run=m.run(0x8022d534,[input,out,mat],[],32768);while(!run.next().done){}
  if(l.read(input,12).some(Boolean))fail('Native projection changed its separate zero input.');
  const f=a=>{b.setUint32(0,m.get(a));return b.getFloat32(0);};return {x:f(out),y:f(out+4)};
 }
 O.cutsceneRomStart={analyze,supports,input,installCode,attachLaunch,resource,bodyPose,preparePose,dialoguePoint,prepareDialogue,projection,projectionState};
})(window.OB64);
