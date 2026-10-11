'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),root=require('./project-paths.cjs').PARENT,rootEditor=require('./project-paths.cjs').EDITOR;
global.window=global;global.OB64={};for(const name of ['parsers.js','art.js','cutscene-dialogue-draw.js'])vm.runInThisContext(fs.readFileSync(path.join(rootEditor,name),'utf8'));
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
const drawer=new OB64.cutsceneDialogueDraw(z64),fixture=require('./fixtures/cutscene-opening-dialogue.json');let rectangles=0,peak=0;
for(const c of fixture.cases){assert.equal(c.error,null);const result=drawer.compose(c.recordHex,c.payloadHex);assert.equal(result.commands.length,c.commands.length,c.pass);assert.equal(result.nativeServiceBytes,0);assert(result.artworkCacheBytes<=16384);peak=Math.max(peak,result.temporaryTextureBytes);
 result.commands.forEach((a,i)=>{const b=c.commands[i];for(const field of ['bounds','uv','step','scissor'])assert.deepStrictEqual(a[field],b[field],c.pass+':'+i+':'+field);assert.equal(Buffer.from(a.texture.indices).toString('hex'),b.indicesHex,c.pass+':'+i+':texture');a.texture.palette.forEach((color,j)=>assert.deepStrictEqual(color,OB64.art.rgba5551(b.palette[j])));rectangles++;});
}
const base=fixture.cases.find(c=>c.pass===120);
for(const [offset,value] of [[0x3d,2],[0x47,1],[0x42,2],[0x50,1],[0x56,254],[0x49,254],[0x3a,2],[0x3b,2]]){const p=Buffer.from(base.payloadHex,'hex');p[offset]=value;assert.throws(()=>drawer.compose(base.recordHex,p.toString('hex')),/unsupported/);}
// Vertical pointer tile 1 is native: func_000E6D90 (OB64 Decomp/src/lib/func_000e6d90.c)
// draws the pointer with U=8*pointerSide (+0x3A) and V=19*pointerEdge (+0x3B). Atlas
// member 3 is 8x13 with mirrored, 16-row masked addressing, so V=19..31 samples rows
// 12..0: tile 1 is the same pointer drawn upside down.
const pointerOf=(tile)=>{const p=Buffer.from(base.payloadHex,'hex');p[0x3b]=tile;const result=drawer.compose(base.recordHex,p.toString('hex'));
 const pointer=result.commands.filter(c=>c.scissor.join()==='0,0,319,239');assert.equal(pointer.length,1,'one speech pointer per box');return pointer[0];};
const edge0=pointerOf(0),edge1=pointerOf(1),side=Buffer.from(base.payloadHex,'hex')[0x3a];
assert.deepStrictEqual(edge0.uv,[side*8,0],'tile 0 starts at native V=0');
assert.deepStrictEqual(edge1.uv,[side*8,19],'tile 1 starts at native V=19');
assert.deepStrictEqual(edge1.bounds,edge0.bounds,'the pointer tile does not move the pointer');
const pointerPixels=(command)=>{const frame={rgba:new Uint8Array(307200)};OB64.cutsceneDialogueDraw.paint(frame,{commands:[command]});const b=command.bounds,rows=[];
 for(let y=b[1];y<b[3];y++)rows.push(Buffer.from(frame.rgba.subarray((y*320+b[0])*4,(y*320+b[2])*4)).toString('hex'));return rows;};
const rows0=pointerPixels(edge0),rows1=pointerPixels(edge1);
assert(rows0.some(row=>/[^0]/.test(row)),'the tile-0 pointer draws visible pixels');
assert.notDeepStrictEqual(rows1,rows0,'tile 1 must not repeat the tile-0 artwork');
assert.deepStrictEqual(rows1,rows0.slice().reverse(),'tile 1 samples the mirrored atlas rows: the tile-0 pointer upside down');
const make=c=>{const frame={rgba:new Uint8Array(307200)};OB64.cutsceneDialogueDraw.paint(frame,drawer.compose(c.recordHex,c.payloadHex));return Buffer.from(frame.rgba);};
assert(!make(fixture.cases.find(c=>c.pass===120)).equals(make(fixture.cases.find(c=>c.pass===124))),'current indicator phase must alter pixels');
assert(!make(fixture.cases.find(c=>c.pass===150)).equals(make(fixture.cases.find(c=>c.pass==='left-control'))),'current portrait side must alter pixels');
console.log(JSON.stringify({status:'pass',nativeCases:fixture.cases.length,rectangles,artworkCacheBytes:drawer.assetBytes,peakTemporaryTextureBytes:peak,unrelatedGuards:true}));
