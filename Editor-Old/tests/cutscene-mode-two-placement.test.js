'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
global.window=global;global.OB64={};
for(const file of ['cutscene-model.js','cutscene-preview.js','cutscene-renderer.js'])vm.runInThisContext(fs.readFileSync(path.join(__dirname,'..',file),'utf8'),{filename:file});
const M=OB64.cutsceneModel,R=OB64.cutsceneRenderer;
const doc=M.createSceneDocument({identity:{sceneId:'height-control',technicalName:'Height control',engine:'director',sourceRevision:'us-rev0',directorKey:'00000001',aliases:[],triggerStatus:'fixture'}});
const projection={mode:'native-perspective-runtime',modelScale:.1,eye:{x:82.389,y:41.635,z:40.122},target:{x:-1.5,y:0,z:-1.27},up:{x:0,y:1,z:0},fovYDegrees:12.88,aspect:4/3,near:1,far:5000,screenWidth:320,screenHeight:240};
const rgba=new Uint8ClampedArray(16*24*4).fill(255),frame={width:16,height:24,anchorX:8,anchorY:24,rgba};
function draw(actor){return R.renderFrame(doc,{actors:[actor]},{projection,actorFrames:{a:frame},showMovementPaths:false});}
for(const [baseY,secondaryY,heightModeByte,translateY]of [[0,27,2,0],[27,27,2,0],[19,-40,4,7],[-13,92,0,-4]]){
 const selected=heightModeByte&4?baseY+secondaryY:heightModeByte&2?secondaryY:baseY;
 const actor={id:'a',visible:true,opacityByte:255,x:-100,y:selected+translateY,z:-32,baseY,secondaryY,heightModeByte,uniformScale:1,renderModeByte:0,renderPipeline:'actor-camera-direct',sceneTransform:{translateY}};
 const before=JSON.stringify(actor),actual=draw(actor),expected=draw({...actor,y:baseY+translateY,renderPipeline:undefined});
 assert.deepStrictEqual(actual.rgba,expected.rgba,'ordinary sprite must use base height, including channel translation');
 assert.deepStrictEqual(actual.hitRegions,expected.hitRegions,'selection bounds must follow the visible sprite');
 assert.strictEqual(JSON.stringify(actor),before,'rendering must preserve the runtime state');
 if(baseY!==selected)assert.notDeepStrictEqual(actual.rgba,draw({...actor,renderPipeline:undefined}).rgba,'control must distinguish the old selected-height path');
}
// Other presentation paths retain their own height contract.
const fallback={id:'a',visible:true,opacityByte:255,x:-100,y:27,z:-32,baseY:0,uniformScale:1,renderModeByte:0};
assert.deepStrictEqual(draw(fallback).rgba,draw({...fallback,baseY:undefined}).rgba);
console.log('PASS ordinary mode-two sprite height: replacement, additive, negative base, channel translation, hit regions, unchanged state, fallback isolation.');
