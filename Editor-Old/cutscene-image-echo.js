// Current-state image matrix services. Native code owns matrix arithmetic and timing.
window.OB64=window.OB64||{};
(function(O){
 'use strict';
 const CTX=0x80380000,ROOT=0x80381000,LAYERS=0x80383000,SOURCE=0x80383900,PROCESSED=0x80383a00,SLOTS=0x80384000;
 function fail(message,code){const e=new Error(message);e.code=code||'image-echo-input';throw e;}
 function bytes(s){return Uint8Array.from(s.match(/../g)||[],x=>parseInt(x,16));}
 function hex(b){return Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');}
 const bits=new DataView(new ArrayBuffer(8));
 function word(v){bits.setFloat32(0,v);return bits.getUint32(0);}
 function floating(v){bits.setUint32(0,v);return bits.getFloat32(0);}
 function installFpu(m){
  m.f=new Uint32Array(32);m.condition=false;
  const step=m.step.bind(m);
  function get(i,double){if(!double)return floating(m.f[i]);bits.setUint32(0,m.f[i+1]);bits.setUint32(4,m.f[i]);return bits.getFloat64(0);}
  function put(i,v,double){if(!double){m.f[i]=word(v);return;}bits.setFloat64(0,v);m.f[i+1]=bits.getUint32(0);m.f[i]=bits.getUint32(4);}
  m.step=function(pc){const w=this.code[pc];if(w===undefined)return step(pc);const op=w>>>26,rs=w>>>21&31,rt=w>>>16&31,fs=w>>>11&31,fd=w>>>6&31,fn=w&63,si=w<<16>>16,at=(this.r[rs]+si)>>>0;let target=null,annul=false;
   if(op===49)this.f[rt]=this.get(at);
   else if(op===57)this.put(at,this.f[rt]);
   else if(op===53){this.f[rt+1]=this.get(at);this.f[rt]=this.get(at+4);}
   else if(op===61){this.put(at,this.f[rt+1]);this.put(at+4,this.f[rt]);}
   else if(op===17){
    if(rs===0)this.r[rt]=this.f[fs];
    else if(rs===4)this.f[fs]=this.r[rt];
    else if(rs===8){const take=this.condition===!!(rt&1);target=(take?pc+4+si*4:pc+8)>>>0;annul=!!(rt&2)&&!take;}
    else if(rs===16||rs===17||rs===20){const d=rs===17,a=rs===20?this.f[fs]|0:get(fs,d),b=get(rt,d);let value;
     if(fn>=48){this.condition=!!((fn&4)&&a<b||(fn&2)&&a===b||(fn&1)&&(Number.isNaN(a)||Number.isNaN(b)));}
     else if(fn===13||fn===36){if(!Number.isFinite(a)||a< -2147483648||a>=2147483648)fail('Image matrix conversion exceeds the supported signed domain.','image-echo-matrix-domain');this.f[fd]=Math.trunc(a);}
     else if(fn===32)put(fd,a,false);
     else if(fn===33)put(fd,a,true);
     else {switch(fn){case 0:value=a+b;break;case 1:value=a-b;break;case 2:value=a*b;break;case 3:value=a/b;break;case 4:value=Math.sqrt(a);break;case 5:value=Math.abs(a);break;case 6:value=a;break;case 7:value=-a;break;default:fail('Unsupported image matrix floating instruction.','image-echo-instruction');}put(fd,value,d);}
    }else fail('Unsupported image matrix coprocessor instruction.','image-echo-instruction');
   }else return step(pc);
   this.r[0]=0;this.steps++;return {target,annul};
  };
 }
 function Echo(rom){
  const data=O.cutsceneImageEchoCode;if(!data||!(rom instanceof Uint8Array))fail('Image echo requires qualified code and the current ROM.');
  const view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),code={};
  data.words.forEach(r=>{if(r[1]+4>rom.length||view.getUint32(r[1])!==r[2])fail('Image echo code differs from the qualified ROM.','image-echo-image');code[r[0]]=r[2];});
  data.references.forEach(r=>{if(r[0]+4>rom.length||view.getUint32(r[0])!==r[1])fail('Image echo drawing differs from the qualified ROM.','image-echo-image');});
  const regions=[{address:CTX,bytes:new Uint8Array(0x4100),writable:true},{address:0x8022a700,bytes:new Uint8Array(0x300),writable:true},{address:0x807fe000,bytes:new Uint8Array(8192),writable:true}];
  data.tables.forEach(r=>{const b=bytes(r.hex);if(r.rom+b.length>rom.length||b.some((x,i)=>x!==rom[r.rom+i]))fail('Image echo constants differ from the qualified ROM.','image-echo-image');regions.push({address:r.address,bytes:b,writable:false});});
  this.machine=new O.cutsceneDialogue.Machine(code,regions);installFpu(this.machine);this.leases=new Set();this.initialized=false;this.started=false;
  const m=this.machine;m.serviceHelper=this.helper.bind(this);m.put(0x8022a970,CTX);m.put(0x8022a974,ROOT);
  this.floats(CTX+0x818,[1,1]);
  for(let i=0;i<20;i++){m.put(ROOT+0x1c54+i*4,LAYERS+i*88);this.floats(LAYERS+i*88+64,[0,0,0,0,0,1]);}
 }
 Echo.prototype.read=function(a,n){return Uint8Array.from({length:n},(_,i)=>this.machine.get(a+i,1));};
 Echo.prototype.write=function(a,b){b.forEach((v,i)=>this.machine.put(a+i,v,1));};
 Echo.prototype.floats=function(a,values){values.forEach((v,i)=>this.machine.put(a+i*4,word(v)));};
 Echo.prototype.invoke=function(pc,args){const g=this.machine.run(pc,args,[],65536);let r;do{r=g.next();}while(!r.done);return r.value;};
 Echo.prototype.helper=function(pc){const m=this.machine,a=m.r[4],b=m.r[5];
  if(pc===0x80080998){const n=m.r[6];this.write(a,this.read(b,n));m.r[2]=a;}
  else if(pc===0x80093380){m.region(a,b,true);this.write(a,new Uint8Array(b));m.r[2]=a;}
  else if(pc===0x80070f30){if(a!==64)fail('Image echo allocation must own one matrix.','image-echo-allocation');let p=SLOTS;while(this.leases.has(p))p+=64;if(p>=SLOTS+192)fail('Image echo matrix slots are exhausted.','image-echo-exhaustion');this.leases.add(p);m.r[2]=p;}
  else if(pc===0x800712c4){if(a&&!this.leases.delete(a))fail('Image echo attempted to free an unowned matrix.','image-echo-owner');m.r[2]=0;}
  else if(pc===0x8023d4a4){
   // Shared product image processing retains the original and creates the same
   // half-size masked image header. Pixel processing remains in the renderer.
   if(a!==0||m.get(CTX+0x4c)!==SOURCE)fail('Vignette processing requires its owned source image.','image-echo-owner');
   m.put(CTX+0x83d,b,1);m.put(CTX+0x50,SOURCE);m.put(CTX+0x4c,PROCESSED);
   m.put(PROCESSED,0x37340003);m.put(PROCESSED+4,m.get(SOURCE+4,2)>>1,2);m.put(PROCESSED+6,m.get(SOURCE+6,2)>>1,2);m.r[2]=PROCESSED;
  }else return false;return true;
 };
 Echo.prototype.initialize=function(presentation,camera,width,height){
  if(this.initialized||!presentation||!Array.isArray(camera)||camera.length!==14||camera.some(x=>!Number.isFinite(x))||![width,height].every(x=>Number.isInteger(x)&&x>=2&&x<=2048))fail('Image matrix initialization requires finite current camera and image dimensions.');
  const p=presentation,m=this.machine;if(!Number.isInteger(p.slot)||p.slot<0||p.slot>=20)fail('Image layer is outside the native layer pool.');
  if(![p.translateX,p.translateY,p.scaleXPercent,p.scaleYPercent,p.alphaCap,p.orientationFlags,p.ignoredPayload].every(Number.isFinite)||Math.abs(p.translateX)>16384||Math.abs(p.translateY)>16384||Math.abs(p.scaleXPercent)>10000||Math.abs(p.scaleYPercent)>10000)fail('Image operands exceed the supported finite matrix domain.','image-echo-matrix-domain');
  this.sourceAssetId=p.sourceAssetId;this.width=width;this.height=height;
  m.put(SOURCE,0x37000200);m.put(SOURCE+4,width,2);m.put(SOURCE+6,height,2);m.put(CTX+0x4c,SOURCE);this.floats(0x8022a778,camera);
  this.invoke(0x8023e694,[p.slot,word(p.translateX),word(p.translateY),p.ignoredPayload,word(p.scaleXPercent),word(p.scaleYPercent),p.alphaCap,p.orientationFlags]);this.initialized=true;
 };
 Echo.prototype.create=function(args,layer){
  if(!this.initialized||this.query()||!Array.isArray(args)||args.length!==3||args.some(x=>!Number.isInteger(x)||x<0||x>255)||args[0]>1||args[1]<1)fail('Image echo requires an initialized image, idle effect, and bounded mode/duration/fade operands.');
  if(!layer||![layer.translateX,layer.translateY,layer.uniformScale].every(Number.isFinite))fail('Image echo requires current layer-one translation and scale.');
  this.floats(LAYERS+88+64,[layer.translateX,layer.translateY,layer.rotationX||0,layer.rotationY||0,layer.translateZ||0,layer.uniformScale]);this.invoke(0x8023e998,args);this.started=true;
  const v=new DataView(this.read(LAYERS+88+64,24).buffer);return {translateX:v.getFloat32(0),translateY:v.getFloat32(4),rotationX:v.getFloat32(8),rotationY:v.getFloat32(12),translateZ:v.getFloat32(16),uniformScale:v.getFloat32(20)};
 };
 Echo.prototype.advance=function(){if(this.started)this.invoke(0x8023ead0,[]);};
 Echo.prototype.query=function(){return this.invoke(0x8023eaa0,[]);};
 Echo.prototype.snapshot=function(){const m=this.machine;return {sourceAssetId:this.sourceAssetId,width:this.width,height:this.height,started:this.started,matrixHex:hex(this.read(CTX,64)),endpointA:hex(this.read(CTX+0x798,64)),endpointB:hex(this.read(CTX+0x7d8,64)),fieldsHex:hex(this.read(CTX+0x798,0xa8)),slots:[0,1,2].map(i=>{const p=m.get(CTX+0x40+i*4);return p?hex(this.read(p,64)):null;}),alphas:[0,1,2].map(i=>m.get(CTX+0x820+i,1)),sample:m.get(CTX+0x824)|0,elapsed:m.get(CTX+0x828)|0,duration:m.get(CTX+0x82c)|0,end:m.get(CTX+0x830)|0,mode:m.get(CTX+0x83b,1),zoomState:m.get(CTX+0x83c,1),alphaCap:m.get(CTX+0x83d,1),imageLayer:m.get(ROOT+0x1c08)===SOURCE,liveMatrices:this.leases.size};};
 Echo.matrix=function(text){const v=new DataView(bytes(text).buffer);if(v.byteLength!==64)fail('Image matrix must contain sixteen native fixed-point entries.');return Array.from({length:16},(_,i)=>((v.getInt16(i*2)*65536+v.getUint16(32+i*2))/65536));};
 Echo.draws=function(s){
  // Native mode one deliberately visits sample through sample-3 inclusive.
  // With three live slots, that repeats one slot. Do not normalize it away.
  const trails=[];function add(n){const slot=n%3;if(s.slots[slot])trails.push({matrixHex:s.slots[slot],alpha:s.alphas[slot],kind:'trail',slot});}
  if(s.mode===0){for(let n=Math.max(0,s.sample-3);n<s.sample;n++)add(n);}
  else {for(let n=s.sample;n>Math.max(-1,s.sample-4);n--)add(n);}
  const current={matrixHex:s.matrixHex,alpha:1,kind:'current'};
  return s.mode===0?trails.concat(current):[current].concat(trails);
 };
 Echo.installFpu=installFpu;
 O.cutsceneImageEcho=Echo;
})(window.OB64);
