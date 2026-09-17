// Ordinary dialogue artwork and native-derived texture rectangle composition.
(function(O){
'use strict';
function fail(s){throw new Error('Native dialogue drawing: '+s);}
function bytes(s){return Uint8Array.from(s.match(/../g)||[],x=>parseInt(x,16));}
function signed(v){return v<<16>>16;}
function Draw(z64){
 this.z64=z64;this.assets={};this.assetBytes=0;
}
Draw.prototype.archive=function(key,member){
 const cache=key+':'+member;if(this.assets[cache])return this.assets[cache];
 const raw=O.art.readResource(this.z64,key).stored;let p=0;
 for(let i=0;i<=member;i++){
  if(p+9>raw.length||raw[p]!==0x85)fail('unsupported image archive member');
  const v=new DataView(raw.buffer,raw.byteOffset+p,raw.length-p),c=v.getUint32(1,true),n=v.getUint32(5,true);
  if(!c||n>65536||p+9+c>raw.length)fail('invalid image archive extent');
  if(i===member){if(this.assetBytes+n>16384)fail('dialogue artwork cache exceeds 16 KiB');const decoded=O.lh5Decompress(raw.subarray(p+9,p+9+c),n);this.assetBytes+=decoded.length;return this.assets[cache]=decoded;}
  p+=9+c;
 }
};
Draw.prototype.frame=function(root,group,member){
 const d=O.art.readResource(this.z64,root).stored,v=new DataView(d.buffer,d.byteOffset,d.byteLength);
 if(group*4+4>d.length)fail('frame descriptor group is unavailable');
 const b=this.archive(v.getUint32(group*4),member),h=new DataView(b.buffer,b.byteOffset,b.byteLength);
 if(b[0]!==0x4b||b[2]!==2||b[3]>1)fail('unsupported frame pixel format '+[root,group,member,b[2],b[3]]);
 const width=h.getUint16(4),height=h.getUint16(6),stride=b[3]?Math.ceil(width/8)*8:Math.ceil(width/16)*8,end=8+stride*height;
 if(end>b.length||(b.length-end)%2)fail('invalid frame pixels');
 const palette=[];for(let i=end;i<b.length;i+=2)palette.push(O.art.rgba5551(h.getUint16(i)));
 return {width,height,stride,indices:b.slice(8,end),palette,kind:'frame',bits:b[3]?8:4};
};
Draw.prototype.portrait=function(identity,variant){
 const v=new DataView(this.z64.buffer,this.z64.byteOffset,this.z64.byteLength),token=v.getUint16(0x64be0+(identity&255)*6+(variant?2:0));
 if(!token)fail('portrait route is empty');
 const desc=O.art.readResource(this.z64,0x1dfb6ea).stored,dv=new DataView(desc.buffer,desc.byteOffset,desc.byteLength),b=O.art.readCompressedResource(this.z64,dv.getUint32((token>>>3)*4)).decoded;
 const at=1920+(token&7)*160;if(at+160>b.length)fail('portrait palette is unavailable');
 const p=new DataView(b.buffer,b.byteOffset,b.byteLength),palette=[];for(let i=0;i<80;i++)palette.push(O.art.rgba5551(p.getUint16(at+i*2)));
 return {width:40,height:48,stride:40,indices:b.slice(0,1920),palette,kind:'portrait',bits:8};
};
Draw.prototype.glyph=function(index){
 if(index<0||index>=166)fail('unsupported glyph index '+index);
 const header=0x218c4fa+0x594280,view=new DataView(this.z64.buffer,this.z64.byteOffset,this.z64.byteLength),size=view.getUint32(header);
 if(header+4+size>this.z64.length)fail('invalid glyph resource extent');
 const b=this.z64.subarray(header+4,header+4+size),indices=new Uint8Array(112),start=index*39;
 if(start+39>b.length)fail('glyph source is unavailable');
 for(let y=0;y<14;y++)for(let x=0;x<11;x++){
  const bit=(y*11+x)*2,at=start+(bit>>>3),value=(b[at]>>>(6-(bit&7)))&3;
  indices[y*8+(x>>>1)]|=value<<(x&1?0:4);
 }
 return {width:16,height:14,stride:8,indices,kind:'glyph'};
};
O.cutsceneDialogueDraw=Draw;
// The ordinary callback uses two rectangle helpers. Both round their scaled
// endpoints to integers before constructing the RDP's fixed-point step values.
Draw.prototype.compose=function(recordHex,payloadHex){
 if(typeof recordHex!=='string'||recordHex.length!==336||typeof payloadHex!=='string'||payloadHex.length!==2288||!/^[0-9a-f]+$/i.test(recordHex+payloadHex))fail('complete current dialogue state is required');
 const r=bytes(recordHex),p=bytes(payloadHex);
 if(r.length!==168||p.length!==1144)fail('complete current dialogue state is required');
 const rv=new DataView(r.buffer),pv=new DataView(p.buffer),s=o=>pv.getInt16(o),u=o=>pv.getUint16(o);
 if(p[0x47]!==0||p[0x3d]>1||(r[0x91]&4)||p[0x50]||p[0x42]>1||p[0x56]!==255||p[0x49]!==255)fail('unsupported ordinary dialogue presentation state');
 const left=rv.getInt16(6),top=rv.getInt16(8),right=rv.getInt16(10),bottom=rv.getInt16(12),width=s(0x20),height=s(0x22);
 if(width<=0||height<=0||right<left||bottom<top)fail('invalid dialogue rectangle');
 const fx=Math.fround((right-left+1)/width),fy=Math.fround((bottom-top+1)/height),commands=[],clip=[Math.max(0,left),Math.max(0,top),Math.min(319,right+1),Math.min(239,bottom+1)];
 function rectangle(texture,x,y,x2,y2,s0=0,t0=0,s1=x2-x,t1=y2-y,scissor=clip){
  const b=[Math.trunc(Math.fround(x*fx))+left,Math.trunc(Math.fround(y*fy))+top,Math.trunc(Math.fround((x2+1)*fx))+left,Math.trunc(Math.fround((y2+1)*fy))+top];
  const w=b[2]-b[0],h=b[3]-b[1];if(b[2]<0||b[3]<0||b[0]>=320||b[1]>=240)return;
  const dx=signed(Math.trunc(((s1-s0+1)*1024)/(w===1?1:w-1))-1)/1024,dy=signed(Math.trunc(((t1-t0+1)*1024)/(h===1?1:h-1))-1)/1024;
  const uv=[s0,t0];if(b[0]<0)uv[0]+=Math.trunc(-b[0]*dx*32)/32;if(b[1]<0)uv[1]+=Math.trunc(-b[1]*dy*32)/32;
  commands.push({kind:'texture',texture,bounds:[Math.max(0,b[0]),Math.max(0,b[1]),Math.min(320,b[2]),Math.min(240,b[3])],uv,step:[dx,dy],scissor:scissor.slice()});
 }
 const frame=(member)=>({...this.frame(0x2093576,0,member),wrapX:member===1||member===3});
 rectangle(frame(1),8,0,width-12,60);rectangle(frame(0),0,0,7,60);rectangle(frame(2),width-11,0,width-1,60);
 // The right portrait follows the native text-column count, not frame width.
 const portrait=s(0x14)>=0,portraitX=p[0x3d]?7*u(0x1a)+16:8;
 if(portrait){const mirror=!!(r[0x8a]&32);rectangle(this.portrait(s(0x14),p[0x4a]),portraitX,5,portraitX+39,52,mirror?39:0,0,mirror?0:39,47);}
 if(!(rv.getUint16(0)&0x1000)&&p[0x178]){
  const start=p[0x4f]?u(0xb8+p[0x54]*2):u(0x36),count=u(0x34);
  if(start>count||count>0x300)fail('invalid dialogue text bounds');
  const text=p.subarray(0x178+start,0x178+count),metrics=O.art.readResource(this.z64,0x218c450).stored;
  const origin=left+(portrait&&!p[0x3d]?56:8);let x=origin,y=top+5,color=p[0x4b]<10?p[0x4b]:0,spacing=0,lineGap=3;
  const view=new DataView(this.z64.buffer,this.z64.byteOffset,this.z64.byteLength);
  const emit=(index)=>{
   if(index<0||index>=metrics.length)fail('unsupported glyph index');
   if(x<320&&y<240&&x+11>=0&&y+14>=0){const palette=[];for(let k=0;k<4;k++)palette.push(O.art.rgba5551(view.getUint16(0x66b88+color*8+k*2)));
    commands.push({kind:'texture',texture:{...this.glyph(index),palette},bounds:[Math.max(0,x),Math.max(0,y),Math.min(320,x+11),Math.min(240,y+14)],uv:[Math.max(0,-x),Math.max(0,-y)],step:[1,1],scissor:clip.slice()});}
   x+=metrics[index]+spacing;
  };
  for(let i=0;i<text.length&&text[i];i++){
   const c=text[i];if(c===10){x=origin;y+=14+lineGap;continue;}if(c===13){x=origin;continue;}
   if(c===123){let j=i+2;while(j<text.length&&text[j]>=48&&text[j]<=57)j++;if(j>=text.length||text[j]!==125)fail('unsupported glyph tag');
    const n=Number(String.fromCharCode(...text.subarray(i+2,j))),tag=String.fromCharCode(text[i+1]);
    if(tag==='T')emit(n-1);else if(tag==='C'){if(n>9)fail('unsupported text color');color=n;}else if(tag==='H')spacing=n;else if(tag==='V')lineGap=n;else fail('unsupported glyph tag '+tag);
    i=j;continue;
   }
   if(c<32||c>=128)fail('unsupported encoded dialogue character');if(c===32)x+=metrics[48]+spacing;else emit(c+16);
  }
 }
 if(p[0x42]===1){
  // Native continuation animation selects one 16-row frame from member 4.
  const indicator=frame(4),frameBytes=indicator.stride*16,phase=p[0x43]>>>6;
  if(indicator.width!==16||indicator.height!==64)fail('unsupported continuation artwork');
  indicator.indices=indicator.indices.slice(phase*frameBytes,(phase+1)*frameBytes);indicator.height=16;
  const x=7*u(0x1a)+(portrait&&!p[0x3d]?40:-8);
  rectangle(indicator,x,38,x+15,53);
 }
 // Ordinary speech pointer uses the current payload's local position and tile.
 if(p[0x3a]>1||p[0x3b])fail('unsupported speech pointer tile');
 // Tile 1 begins at U=8 and wraps over the existing eight-pixel texture.
 rectangle(frame(3),s(0x30),s(0x32),s(0x30)+7,s(0x32)+12,p[0x3a]*8,0,p[0x3a]*8+7,12,[0,0,319,239]);
 const seen=new Set();let textureBytes=0;commands.forEach(c=>{if(!seen.has(c.texture.indices)){seen.add(c.texture.indices);textureBytes+=c.texture.indices.byteLength;} });
 return {commands,nativeServiceBytes:0,artworkCacheBytes:this.assetBytes,temporaryTextureBytes:textureBytes};
};
Draw.paint=function(image,result){
 const out=image.rgba||image.data;
 for(const c of result.commands){const t=c.texture,b=c.bounds,s=c.scissor;
  if(!t.palette)fail('texture palette is unavailable');
  for(let y=Math.max(0,Math.ceil(b[1]),s[1]);y<Math.min(240,Math.ceil(b[3]),s[3]);y++)for(let x=Math.max(0,Math.ceil(b[0]),s[0]);x<Math.min(320,Math.ceil(b[2]),s[2]);x++){
   const sourceX=Math.floor(c.uv[0]+(x-b[0])*c.step[0]),sx=t.wrapX?((sourceX%t.width)+t.width)%t.width:Math.min(t.width-1,Math.max(0,sourceX)),sy=Math.min(t.height-1,Math.max(0,Math.floor(c.uv[1]+(y-b[1])*c.step[1])));
   const raw=t.indices[sy*t.stride+(t.bits===8?sx:sx>>>1)],index=t.bits===8?raw:(raw>>>(sx&1?0:4))&15,p=t.palette[index];
   if(!p)fail('texture index exceeds its palette');if(!p[3])continue;
   const at=(y*320+x)*4,alpha=p[3]/255;for(let ch=0;ch<3;ch++)out[at+ch]=Math.round(p[ch]*alpha+out[at+ch]*(1-alpha));out[at+3]=255;
  }
 }
 return image;
};
})(window.OB64);
