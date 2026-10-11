'use strict';
const assert=require('assert'),fs=require('fs'),path=require('path'),vm=require('vm');
const harness=fs.readFileSync(path.join(__dirname,'native-classes-export.test.js'),'utf8').split('(async () => {')[0];
const {fresh}=new Function('require','__dirname',harness+'\nreturn {fresh};')(require,__dirname);
class Element {
  constructor(tag){this.tagName=tag;this.children=[];this.events={};this.attributes={};this.textContent='';this.value='';}
  appendChild(child){this.children.push(child);return child;}
  setAttribute(name,value){this.attributes[name]=value;}
  addEventListener(name,action){this.events[name]=action;}
}
global.document={createElement:tag=>new Element(tag)};
vm.runInThisContext(fs.readFileSync(path.join(__dirname,'../class-families-ui.js'),'utf8'));
function descendants(node){return [node].concat(node.children.flatMap(descendants));}
const rom=fresh(),parent=new Element('div');let changed=0;
OB64.classFamiliesUI.mount(rom,parent,()=>changed++);
let nodes=descendants(parent);
nodes.find(n=>n.attributes['aria-label']==='New family name').value='Order of Dawn';
nodes.find(n=>n.textContent==='New Family').events.click();
assert.strictEqual(changed,1);assert(rom.classFamilies.families.some(f=>f.id===16&&f.name==='Order of Dawn'));
const select=OB64.classFamiliesUI.select(rom,2,()=>changed++);
assert(select.children.some(n=>n.value==='16'));select.value='16';select.events.change();
assert.strictEqual(rom.classFamilies.assignments[2],16);assert.strictEqual(changed,2);
nodes.find(n=>n.textContent==='New Family').events.click();
assert.strictEqual(changed,2);assert.match(nodes.find(n=>n.attributes.role==='status').textContent,/unique/);
OB64.buildModes.set(rom,'patch');
assert(OB64.classFamiliesUI.select(rom,2,()=>{}).disabled);
const patchPanel=new Element('div');OB64.classFamiliesUI.mount(rom,patchPanel,()=>{});
assert(descendants(patchPanel).find(n=>n.textContent==='New Family').disabled);
assert.strictEqual(OB64.classFamiliesUI.select(rom,0xA4,()=>{}).tagName,'span');
const app=fs.readFileSync(path.join(__dirname,'../app.js'),'utf8');
assert(app.includes("{ label: 'Family', title:"));
assert.strictEqual((app.match(/classFamiliesUI\.select\(rom, cid, familyChanged\)/g)||[]).length,2);
assert(app.includes('class_families_modified || 0'));
console.log('PASS new-family creation, assignment, duplicate validation, Rebuild-only controls, and table/card integration');
