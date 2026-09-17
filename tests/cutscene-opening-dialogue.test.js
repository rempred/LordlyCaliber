'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),root=path.resolve(__dirname,'../..');
global.window=global;global.OB64={};for(const name of ['parsers.js','art.js','cutscene-dialogue-draw.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',name),'utf8'));
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
const drawer=new OB64.cutsceneDialogueDraw(z64),fixture=require('./fixtures/cutscene-opening-dialogue.json');let rectangles=0,peak=0;
for(const c of fixture.cases){assert.equal(c.error,null);const result=drawer.compose(c.recordHex,c.payloadHex);assert.equal(result.commands.length,c.commands.length,c.pass);assert.equal(result.nativeServiceBytes,0);assert(result.artworkCacheBytes<=16384);peak=Math.max(peak,result.temporaryTextureBytes);
 result.commands.forEach((a,i)=>{const b=c.commands[i];for(const field of ['bounds','uv','step','scissor'])assert.deepStrictEqual(a[field],b[field],c.pass+':'+i+':'+field);assert.equal(Buffer.from(a.texture.indices).toString('hex'),b.indicesHex,c.pass+':'+i+':texture');a.texture.palette.forEach((color,j)=>assert.deepStrictEqual(color,OB64.art.rgba5551(b.palette[j])));rectangles++;});
}
const base=fixture.cases.find(c=>c.pass===120);
for(const [offset,value] of [[0x3d,2],[0x47,1],[0x42,2],[0x50,1],[0x56,254],[0x49,254],[0x3a,2],[0x3b,1]]){const p=Buffer.from(base.payloadHex,'hex');p[offset]=value;assert.throws(()=>drawer.compose(base.recordHex,p.toString('hex')),/unsupported/);}
const make=c=>{const frame={rgba:new Uint8Array(307200)};OB64.cutsceneDialogueDraw.paint(frame,drawer.compose(c.recordHex,c.payloadHex));return Buffer.from(frame.rgba);};
assert(!make(fixture.cases.find(c=>c.pass===120)).equals(make(fixture.cases.find(c=>c.pass===124))),'current indicator phase must alter pixels');
assert(!make(fixture.cases.find(c=>c.pass===150)).equals(make(fixture.cases.find(c=>c.pass==='left-control'))),'current portrait side must alter pixels');
console.log(JSON.stringify({status:'pass',nativeCases:fixture.cases.length,rectangles,artworkCacheBytes:drawer.assetBytes,peakTemporaryTextureBytes:peak,unrelatedGuards:true}));
