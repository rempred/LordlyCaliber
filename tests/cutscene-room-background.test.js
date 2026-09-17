'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
global.window=global;global.OB64={};
for(const f of ['cutscene-model.js','cutscene-renderer.js'])vm.runInThisContext(fs.readFileSync(path.join(__dirname,'..',f),'utf8'));
const R=OB64.cutsceneRenderer,fixture=require('./fixtures/cutscene-room-background.json');
for(const c of fixture.cases){
 const layer={nativeOrdinal:c.layer,renderPipeline:'mode-zero-b5-actor-camera',sceneTransform:c.transform};
 const actual=R.modeZeroBackgroundGeometry(c.image,layer,fixture.projection),b=Buffer.from(c.matrixHex,'hex'),m=[];
 for(let i=0;i<16;i++)m.push(b.readInt16BE(i*2)+b.readUInt16BE(32+i*2)/65536);
 const points=[[c.image.originX,-c.image.originY],[c.image.originX+c.image.width,-c.image.originY],[c.image.originX+c.image.width,-c.image.originY-c.image.height],[c.image.originX,-c.image.originY-c.image.height]];
 const expected=points.map(([x,y])=>R.projectPointFloat({x:x*m[0]+y*m[4]+m[12],y:x*m[1]+y*m[5]+m[13],z:x*m[2]+y*m[6]+m[14]},fixture.projection,1));
 assert(Math.max(...expected.flatMap((p,i)=>[Math.abs(p.x-actual.screenQuad[i].x),Math.abs(p.y-actual.screenQuad[i].y)]))<.2);
}
// Opaque overlapping artwork distinguishes layer ownership from global depth order.
const projection={mode:'native-perspective-runtime',modelScale:1,eye:{x:0,y:0,z:340},target:{x:0,y:0,z:0},up:{x:0,y:1,z:0},fovYDegrees:38,aspect:4/3,near:1,far:4000,screenWidth:320,screenHeight:240};
const document=OB64.cutsceneModel.createSceneDocument({identity:{sceneId:'scene:room-order',technicalName:'Room order',engine:'director',sourceRevision:'us-rev0',directorKey:'00000002',aliases:[],triggerStatus:'fixture'}});
const image={container:'bg2',format:0,originX:-10,originY:-10,width:20,height:20,rgba:new Uint8ClampedArray(20*20*4)};
for(let i=0;i<image.rgba.length;i+=4)image.rgba.set([0,255,0,255],i);
const sprite={width:20,height:20,anchorX:10,anchorY:10,rgba:new Uint8ClampedArray(image.rgba.length)};
for(let i=0;i<sprite.rgba.length;i+=4)sprite.rgba.set([255,0,0,255],i);
const actor={id:'a',visible:true,x:0,y:0,z:0,opacity:255,uniformScale:1,transformChannel:0};
const options={showMovementPaths:false,projection,actorFrames:{a:sprite},backgrounds:[{image,layer:{nativeOrdinal:1,renderPipeline:'mode-zero-b5-actor-camera',sceneTransform:{uniformScale:1}}}]};
function center(channel){const result=R.renderFrame(document,{actors:[{...actor,transformChannel:channel}]},options);return Array.from(result.rgba.slice((120*320+160)*4,(120*320+160)*4+4));}
assert.deepStrictEqual(center(0),[0,255,0,255],'a later background must cover an earlier Actor');
assert.deepStrictEqual(center(1),[255,0,0,255],'the Actor follows its own background');
assert.deepStrictEqual(center(2),[255,0,0,255],'changing only the Actor channel changes layer ownership');
console.log('PASS seven native background matrix controls and Actor layer occlusion.');
