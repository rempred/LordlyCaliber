"use strict";
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),root=path.resolve(__dirname,'../..');
new Function('require','__dirname',fs.readFileSync(path.join(__dirname,'cutscene-shared-rom-start.test.js'),'utf8').split('\n(async()=>')[0])(require,__dirname);
const source=fs.readFileSync(path.join(root,'editor/cutscene-ui.js'),'utf8');
// Expose the actual Stage renderer and transport update in memory only.
vm.runInThisContext(source.replace('    loadScene: loadScene,','    loadScene: loadScene, testRenderStageArea: renderStageArea, testUpdateTransport: updateTransport,'));
const raw=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(raw.length);for(let i=0;i<raw.length;i+=2){z64[i]=raw[i+1];z64[i+1]=raw[i];}
function element(tag){return {tag,style:{},children:[],listeners:{},attrs:{},classList:{toggle(){}},appendChild(e){this.children.push(e);return e;},setAttribute(k,v){this.attrs[k]=v;},addEventListener(k,f){assert(!this.listeners[k],'duplicate listener');this.listeners[k]=f;},getBoundingClientRect(){return {left:0,top:0,width:320,height:240};}};}
function textOf(e){return [e.textContent||'',...e.children.map(textOf)].join(' ');}
(async()=>{
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}},state=OB64.cutsceneUI.initialize(rom),results=[];
 function render(scene,document){global.document={createElement:element};state.ui={};const shell=element('main');OB64.cutsceneUI.testRenderStageArea(shell,rom,state,scene,document);return textOf(shell);}
 function checkBoundary(scene,document,expected){const error=state.sourceErrors['runtime-context:'+scene.assetId];assert.equal(error,expected);assert(!state.runtimeByAssetId[scene.assetId]);const text=render(scene,document);assert(text.includes(expected));assert(text.includes('Director playback unavailable:'));assert(text.includes('Approximate storyboard only;'));assert.equal(state.ui.playButton.textContent,'Play storyboard');state.ui.clock.playing=true;OB64.cutsceneUI.testUpdateTransport(state,{frame:0,durationFrames:10});assert.equal(state.ui.playButton.textContent,'Pause storyboard');state.ui.clock.playing=false;OB64.cutsceneUI.testUpdateTransport(state,{frame:0,durationFrames:10});assert.equal(state.ui.playButton.textContent,'Play storyboard');assert.equal(render(scene,document),text,'rerender retains the precise reason and transport label');results.push({assetId:scene.assetId,runtime:false,error,reasonRendered:true,storyboardTransport:true});}
 const unsupported=state.catalog.getScene('rom-director:01F3E500');state.selectedSceneId=unsupported.sceneId;const unsupportedDocument=await OB64.cutsceneUI.loadScene(rom,state,unsupported);
 checkBoundary(unsupported,unsupportedDocument,"This Director stream controls Hugo's Report: People, Events, Miscellany and Tips. Its interactive menus are an interface resource, not a timed cutscene.");
 // Changing selection must not carry the other resource's error into playback.
 const supported=state.catalog.getScene('rom-custom-lz:01FA4D0A');state.selectedSceneId=supported.sceneId;const document=await OB64.cutsceneUI.loadScene(rom,state,supported);assert.equal(state.runtimeByAssetId[supported.assetId].outcome,'modeled-termination');let text=render(supported,document);assert(!text.includes('Director playback unavailable'));assert(!text.includes("Hugo's Report"));assert.equal(state.ui.playButton.textContent,'Play');
 // Keep the loaded retail program, but remove its explicit environment command
 // for this inherited-setup negative control. Exclude caller metadata so the
 // control tests absent inherited state. Startup rejects before execution.
 const retailProgram=state.programByAssetId[supported.assetId],inherited=structuredClone(retailProgram);inherited.primitives.shift();state.programByAssetId[supported.assetId]=inherited;
 const inheritedScene=structuredClone(supported);inheritedScene.launchProfile.parentEventLaunches=[];
 await OB64.cutsceneUI.loadScene(rom,state,inheritedScene);
 checkBoundary(inheritedScene,document,'ROM startup requires inherited Stage and caller state because this stream supplies no environment.');assert.equal(state.boundRuntimeDocument,null,'failed reload unbinds the previous runtime');
 // Restoring the supported program must remove the same resource's prior error.
 state.programByAssetId[supported.assetId]=retailProgram;await OB64.cutsceneUI.loadScene(rom,state,supported);assert.equal(state.runtimeByAssetId[supported.assetId].states.length,235);assert.equal(state.sourceErrors['runtime-context:'+supported.assetId],undefined);text=render(supported,document);assert(!text.includes('Director playback unavailable'));assert(!text.includes('requires inherited Stage'));assert.equal(state.ui.playButton.textContent,'Play');state.ui.clock.playing=true;OB64.cutsceneUI.testUpdateTransport(state,{frame:0,durationFrames:235});assert.equal(state.ui.playButton.textContent,'Pause');
 // An unrelated error remains attributable to its original resource.
 assert(state.sourceErrors['runtime-context:'+unsupported.assetId].includes("Hugo's Report"));results.push({assetId:supported.assetId,runtime:true,passes:235,selectionRecovery:true,sameResourceRecovery:true,staleErrorCleared:true});
 console.log(JSON.stringify({status:'pass',results,scope:'Actual loader, Stage renderer, and transport updates with deterministic DOM substitutes; no browser-layout or Computer Use test.'},null,2));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
