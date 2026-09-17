'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),root=path.resolve(__dirname,'../..');
global.window=global;global.OB64={};for(const file of ['parsers.js','art.js','cutscene-dialogue-draw.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',file),'utf8'),{filename:file});
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
const drawer=new OB64.cutsceneDialogueDraw(z64),fixture=require('./fixtures/cutscene-active-dialogue-drawing.json');let rectangles=0,peakTextureBytes=0;
for(const native of fixture.cases){
 const actual=drawer.compose(native.recordHex,native.payloadHex);assert.equal(actual.nativeServiceBytes,0);assert(actual.artworkCacheBytes<=16384);peakTextureBytes=Math.max(peakTextureBytes,actual.temporaryTextureBytes);
 assert.equal(actual.commands.length,native.commands.length);
 actual.commands.forEach((a,i)=>{const b=native.commands[i];for(const field of ['bounds','uv','step','scissor'])assert.deepStrictEqual(a[field],b[field],native.sample+':'+i+':'+field);
  assert.equal(Buffer.from(a.texture.indices).toString('hex'),b.indicesHex);
  if(a.texture.kind==='frame'){assert.equal(b.clampS,false);assert.equal(a.texture.wrapX,b.maskS>0);if(b.maskS)assert.equal(1<<b.maskS,a.texture.width);}
  a.texture.palette.forEach((color,j)=>assert.deepStrictEqual(color,OB64.art.rgba5551(b.palette[j])));rectangles++;
 });
 const image={rgba:new Uint8Array(320*240*4)};OB64.cutsceneDialogueDraw.paint(image,actual);
 assert(image.rgba.some((v,i)=>i%4!==3&&v));
}
const a=fixture.cases[2],b=fixture.cases[3],first={rgba:new Uint8Array(307200)},changed={rgba:new Uint8Array(307200)};
OB64.cutsceneDialogueDraw.paint(first,drawer.compose(a.recordHex,a.payloadHex));OB64.cutsceneDialogueDraw.paint(changed,drawer.compose(b.recordHex,b.payloadHex));assert.notDeepStrictEqual(first.rgba,changed.rgba);
for(const offset of [0x47,0x3d,0x42]){const p=Buffer.from(a.payloadHex,'hex');p[offset]=1;assert.throws(()=>drawer.compose(a.recordHex,p.toString('hex')),/unsupported ordinary dialogue/);}
const unsupported=Buffer.from(a.payloadHex,'hex');unsupported[0x178]=0x81;assert.throws(()=>drawer.compose(a.recordHex,unsupported.toString('hex')),/unsupported encoded/);
assert.throws(()=>drawer.compose('',a.payloadHex),/complete current/);
console.log(JSON.stringify({status:'pass',nativeCases:fixture.cases.length,rectangles,changedTextPlacement:true,negativeClipping:true,nativeServiceBytes:0,artworkCacheBytes:drawer.assetBytes,peakTemporaryTextureBytes:peakTextureBytes}));
