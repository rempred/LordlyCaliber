'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),root=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-native-actor-drawing.json');
global.window=global;global.OB64={};
for(const file of ['cutscene-model.js','cutscene-preview.js','cutscene-renderer.js','cutscene-dialogue-data.js','cutscene-dialogue.js','cutscene-image-echo-data.js','cutscene-image-echo.js','cutscene-shared-actor-data.js','cutscene-shared-actor.js','cutscene-framebuffer.js'])vm.runInThisContext(fs.readFileSync(path.join(root,'editor',file),'utf8'),{filename:file});
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),rom=new Uint8Array(raw.length);
for(let i=0;i<raw.length;i+=2){rom[i]=raw[i+1];rom[i+1]=raw[i];}
const f32=word=>{const b=new DataView(new ArrayBuffer(4));b.setUint32(0,word);return b.getFloat32(0);};
function camera(a){return {evidenceStatus:'native-test',fovYDegrees:a[0],aspect:a[1],near:a[2],far:a[3],eye:{x:a[5],y:a[6],z:a[7]},target:{x:a[8],y:a[9],z:a[10]},up:{x:a[11],y:a[12],z:a[13]}};}
for(const row of fixture.cameras){
 const echo=new OB64.cutsceneImageEcho(rom),service=new OB64.cutsceneSharedActor(echo,{kind:'native-ordinary-actor-v1',worldScale:.1},rom),c=camera(row.camera);
 echo.floats(0x8022a720,row.camera);const callerScale=echo.machine.get(0x8022a730);echo.machine.put(0x8022a730,0);
 const result=service.drawingCamera({actor:c,registered:c},callerScale);assert.equal(result.perspectiveScaleWord,callerScale);
 assert.equal(result.projectionMatrixHex,row.projectionHex,row.name+' perspective');assert.equal(result.viewMatrixHex,row.viewHex,row.name+' view');
 assert.strictEqual(service.drawingCamera({actor:c,registered:c}),result,'unchanged camera reuses preparation');
}
// The independent CPU projection uses floating camera matrices. Packed drawing
// matrices agree within the measured fixed-point quantization difference.
for(const row of require('./fixtures/cutscene-shared-actor.json').cases){
 const c=fixture.cameras.find(r=>r.name===row.name),p=camera(row.camera),actor={id:row.name,slot:0};
 const preview={actorProjection:p,registeredProjection:p,nativeActorDrawing:{camera:{viewMatrixHex:c.viewHex,projectionMatrixHex:c.projectionHex},cameraKey:OB64.cutsceneSharedActor.cameraDrawingKey(p,p),actors:[{slot:0,key:OB64.cutsceneSharedActor.actorDrawingKey(actor),matrixHex:row.matrixHex}]}};
 const point=OB64.cutsceneRenderer.nativeActorGeometry(actor,preview).screenPoint;
 assert(Math.abs(point.x-row.output[0])<.01&&Math.abs(point.y-row.output[1])<.01,'native projection origin '+row.name);
}
const identity=()=>Array.from({length:4},(_,r)=>Array.from({length:4},(_,c)=>r===c?1:0));let vertices=0;
{
 const echo=new OB64.cutsceneImageEcho(rom),service=new OB64.cutsceneSharedActor(echo,{kind:'native-ordinary-actor-v1',worldScale:.1},rom),c=camera(fixture.cameras[0].camera);
 service.prepare([],{actor:c,registered:c},fixture.transform.channels.map(r=>r.fields));
 const layers=fixture.transform.channels.map(row=>{
  const bytes=echo.read(0x80383000+row.channel*88,88),view=new DataView(bytes.buffer);
  assert.deepStrictEqual(Array.from({length:6},(_,i)=>view.getFloat32(64+4*i)),row.nativeOrder,'native keyframe writer field order');return {resource:null,record:bytes};
 });
 const iris=new OB64.cutsceneFramebuffer.Iris(20,0,layers),snapshot=OB64.cutsceneFramebuffer.snapshot(iris);
 snapshot.layers.forEach((row,i)=>assert.deepStrictEqual(row.transform,fixture.transform.channels[i].fields,'iris native record readback'));
}
for(const draw of fixture.draws){
 assert.equal(draw.error,null);assert.equal(draw.vertices.length,draw.layers.length*4);
 draw.layers.forEach((row,index)=>{
  const layer={drawOffsetX:row[1]|0,drawOffsetY:row[2]|0,width:row[3],height:row[4],scaleX:f32(row[6]),scaleY:f32(row[7])};
  const actual=OB64.cutsceneRenderer.nativeActorLayerGeometry({nativePacked:true,matrix:identity()},layer).localQuad;
  const expected=[0,1,3,2].map(i=>{const v=draw.vertices[index*4+i].xyz,s=draw.scales[index];return {x:v[0]*s[0],y:v[1]*s[1],z:v[2]*s[2]};});
  assert.deepStrictEqual(actual,expected,'native callback vertices and layer scale');vertices+=4;
 });
}
const matrix=identity();matrix[0][2]=2;matrix[3][2]=-.5;
const layer={drawOffsetX:-1,drawOffsetY:-1,width:3,height:3,scaleX:.5,scaleY:.5};
const partial=OB64.cutsceneRenderer.nativeActorLayerGeometry({nativePacked:true,matrix},layer);
assert.equal(partial.polygon.length,4);assert.equal(Math.min(...partial.polygon.map(p=>p.x)),120);assert(partial.polygon.every(p=>p.inverseW>0));
matrix[0][2]=0;matrix[3][2]=-2;assert.equal(OB64.cutsceneRenderer.nativeActorLayerGeometry({nativePacked:true,matrix},layer).polygon.length,0,'behind near plane');
matrix[3][2]=0;matrix[3][3]=-1;assert.equal(OB64.cutsceneRenderer.nativeActorLayerGeometry({nativePacked:true,matrix},layer).polygon.length,0,'behind eye');
console.log(JSON.stringify({status:'pass',packedCameras:fixture.cameras.length,nativeProjectionOrigins:4,nativeVertices:vertices,nativeDraws:fixture.draws.length,transformChannels:20,partialNearClip:true,behindEyeRejected:true}));
