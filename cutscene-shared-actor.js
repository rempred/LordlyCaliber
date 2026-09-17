// Current-state ordinary Actor matrix preparation and scratch projection.
window.OB64=window.OB64||{};
(function(O){
 'use strict';
 const ROOT=0x80381000,LAYERS=0x80383000,ACTOR=0x80382d00,INPUT=0x80382e60,OUTPUT=0x80382e70;
 function fail(message,code){const e=new Error(message);e.code=code||'shared-actor-input';throw e;}
 function bytes(s){return Uint8Array.from(s.match(/../g)||[],x=>parseInt(x,16));}
 function Shared(echo,input,rom){
  if(!echo||!input||input.kind!=='native-ordinary-actor-v1'||!Number.isFinite(input.worldScale)||input.worldScale<=0||input.worldScale>100)fail('Ordinary Actor services require current matrix storage and a declared positive world scale.');
  this.echo=echo;this.machine=echo.machine;this.input=input;const m=this.machine,data=O.cutsceneSharedActorCode,view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength);
  if(!data)fail('Ordinary Actor code is unavailable.');
  data.words.forEach(r=>{if(view.getUint32(r[1])!==r[2])fail('Actor code differs from its qualified ROM.','shared-actor-image');if(m.code[r[0]]!==undefined&&m.code[r[0]]!==r[2])fail('Actor code conflicts with shared matrix code.');m.code[r[0]]=r[2];});
  function install(a,b,writable){
   // Preserve existing matrix constants. Only add uncovered consecutive spans.
   let start=-1;
   for(let i=0;i<=b.length;i++){
    const r=i<b.length&&m.regions.find(r=>a+i>=r.address&&a+i<r.address+r.bytes.length);
    if(r&&r.bytes[a+i-r.address]!==b[i])fail('Actor data overlaps different shared state.');
    if(i<b.length&&!r){if(start<0)start=i;}
    else if(start>=0){m.regions.push({address:a+start,bytes:b.slice(start,i),writable});start=-1;}
   }
  }
  data.tables.forEach(r=>{const b=bytes(r.hex);if(b.some((x,i)=>x!==rom[r.rom+i]))fail('Actor constants differ from the qualified ROM.','shared-actor-image');install(r.address,b,false);});
  install(0x8018fc19,Uint8Array.of(0),false);const b=new DataView(new ArrayBuffer(4));b.setFloat32(0,input.worldScale);install(0x801d06fc,new Uint8Array(b.buffer),false);
  const step=m.step.bind(m);m.step=function(pc){
   const w=this.code[pc],op=w>>>26;if(op!==34&&op!==38&&op!==46)return step(pc);
   const rs=w>>>21&31,rt=w>>>16&31,a=(this.r[rs]+(w<<16>>16))>>>0,k=a&3,base=a-k;
   if(op===34){for(let j=k;j<4;j++){const shift=24-8*(j-k),mask=(255<<shift)>>>0;this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;}}
   else if(op===38){for(let j=0;j<=k;j++){const shift=8*(k-j),mask=(255<<shift)>>>0;this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;}}
   else for(let j=0;j<=k;j++)this.put(base+j,(this.r[rt]>>>(8*(k-j)))&255,1);
   this.r[0]=0;this.steps++;return {target:null,annul:false};
  };
 }
 Shared.prototype.cameras=function(cameras){
  for(const [name,offset]of [['actor',0],['registered',88]]){
   const c=cameras[name];if(!c||c.evidenceStatus==='external-unresolved')fail('Actor service requires qualified current cameras.');
   const values=[c.fovYDegrees,c.aspect,c.near,c.far,1,c.eye.x,c.eye.y,c.eye.z,c.target.x,c.target.y,c.target.z,c.up.x,c.up.y,c.up.z];
   if(values.some(x=>!Number.isFinite(x))||c.fovYDegrees<=0||c.fovYDegrees>=179)fail('Actor camera is outside the finite supported domain.');
   // The unused fifth camera word remains owned by the caller.
   this.echo.floats(0x8022a720+offset,values.slice(0,4));this.echo.floats(0x8022a720+offset+20,values.slice(5));
  }
 };
 Shared.prototype.prepare=function(records,cameras,channels){
  this.cameras(cameras);const m=this.machine,e=this.echo;
  for(let i=0;i<20;i++){
   const c=channels[i];if(!c||![c.rotationX,c.rotationY,c.translateX,c.translateY,c.translateZ,c.uniformScale].every(Number.isFinite))fail('Actor preparation requires twenty current transform channels.');
   e.floats(LAYERS+88*i+64,[c.rotationX,c.rotationY,c.translateX,c.translateY,c.translateZ,c.uniformScale]);
  }
  const key=JSON.stringify([cameras,channels]);
  if(key!==this.preparedCameraKey){m.put(0x8022a997,19,1);e.invoke(0x8023ac68,[]);this.preparedCameraKey=key;}
  const result=[];
  for(const row of records){
   if(row.bytes.length!==336||row.bytes[0x13d]!==0||row.bytes[0x13e]>=20||!Number.isInteger(row.slot)||row.slot<0||row.slot>=28)fail('Actor preparation requires an ordinary record and a valid transform channel.');
   e.write(ACTOR,row.bytes);m.put(ROOT+24+4*row.slot,ACTOR);
   try{e.invoke(0x80239d48,[]);result.push({slot:row.slot,bytes:e.read(ACTOR,336)});}finally{m.put(ROOT+24+4*row.slot,0);}
  }
  return result;
 };
 Shared.prototype.project=function(record,cameras){
  if(!(record instanceof Uint8Array)||record.length!==336||record[0x13d]!==0)fail('Projection requires the current ordinary Actor record.');
  this.cameras(cameras);const e=this.echo;e.write(ACTOR,record);e.write(INPUT,e.read(0x80239240,12));e.write(OUTPUT,new Uint8Array(8));
  e.invoke(0x8022d534,[INPUT,OUTPUT,ACTOR+0xa0]);
  if(e.read(INPUT,12).some(Boolean))fail('Native projection changed its separate zero input.','shared-actor-nonaliasing');
  const output=e.read(OUTPUT,8),v=new DataView(output.buffer),point=[v.getFloat32(0),v.getFloat32(4)];if(point.some(n=>!Number.isFinite(n)))fail('Actor projection exceeds the supported finite domain.','shared-actor-projection-domain');
  return {inputX:0,projectionInput:e.read(INPUT,12),output:point,outputHex:Array.from(output,b=>b.toString(16).padStart(2,'0')).join('')};
 };
 O.cutsceneSharedActor=Shared;
})(window.OB64);

