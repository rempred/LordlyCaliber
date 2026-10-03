// Authored class families and the expanded native promotion resource (US Rev 0).
window.OB64 = window.OB64 || {};
(function(O) {
  'use strict';
  var MAGIC = 0x4C43464D; // LCFM, descriptor at the end of this resource owner.
  function fail(message) { throw new Error('Class families: ' + message); }
  function spec() { return O.nativeClasses.verifyData().families; }
  function copy(value) { return JSON.parse(JSON.stringify(value)); }
  function same(a,b) { return a.length===b.length&&a.every(function(v,i){return v===b[i];}); }
  function bytes(hex) { return Uint8Array.from(hex.match(/../g),function(v){return parseInt(v,16);}); }
  function matches(z,start,hex) { return same(z.subarray(start,start+hex.length/2),bytes(hex)); }
  function blank(z,start,size) { return z.subarray(start,start+size).length===size&&z.subarray(start,start+size).every(function(v){return v===255;}); }
  function retail(rom) {
    var assignments=[],families=[];
    for(var i=0;i<81;i++) assignments.push(rom.z64[0xDBD2C+i*2]&15);
    for(var id=0;id<=10;id++) families.push({id:id,name:id?'Family '+id:'None',behavior:id});
    return {schemaVersion:1,families:families,assignments:assignments};
  }
  function validate(value) {
    if(!value||value.schemaVersion!==1||!Array.isArray(value.families)||!Array.isArray(value.assignments)||value.assignments.length!==81)fail('invalid family catalog');
    if(Object.keys(value).sort().join(',')!=='assignments,families,schemaVersion')fail('unknown catalog fields');
    var seen=Object.create(null),names=Object.create(null);
    value.families.forEach(function(f){
      if(!f||Object.keys(f).sort().join(',')!=='behavior,id,name'||!Number.isInteger(f.id)||f.id<0||f.id>65534||seen[f.id])fail('invalid or duplicate family ID');
      if(!Number.isInteger(f.behavior)||f.behavior<0||f.behavior>10)fail('unrecognized family behavior');
      if(typeof f.name!=='string'||!f.name.trim()||f.name!==f.name.trim()||f.name.length>80||/[\x00-\x1F\x7F]/.test(f.name)||names[f.name.toLowerCase()])fail('family names must be unique, nonempty, and at most 80 characters');
      if(f.id<=10&&f.behavior!==f.id)fail('retail family behavior must be preserved');
      if(f.id>10&&f.behavior===0)fail('new families require a numbered family template; None is the absence of a family');
      if(f.id===0&&f.name!=='None')fail('family zero must remain None');
      seen[f.id]=true;names[f.name.toLowerCase()]=true;
    });
    for(var i=0;i<=10;i++)if(!seen[i])fail('missing retail family '+i);
    value.assignments.forEach(function(id){if(!Number.isInteger(id)||!seen[id])fail('class references an undefined family');});
    if(value.assignments[0]!==0)fail('the empty class must remain in family zero');
    var out=copy(value);out.families.sort(function(a,b){return a.id-b.id;});return out;
  }
  function hasEvidence(z) {
    var d=spec(),end=d.romStart+d.romSize-64;
    return O.readU32BE(z,end)===MAGIC||d.owners.some(function(owner){
      return matches(z,owner.start,owner.familyHex);
    });
  }
  function inspect(z) {
    var d=spec(),end=d.romStart+d.romSize-64;
    try {
      if(!hasEvidence(z))return {state:'absent'};
      if(O.readU32BE(z,end)!==MAGIC||O.readU32BE(z,end+4)!==1)fail('missing or invalid resource descriptor');
      var logical=O.readU32BE(z,end+8),size=O.readU32BE(z,end+12),stored=(logical+1)&~1;
      if(logical<1||stored+8>d.romSize-64||size<d.metadataOffset+8||size>65536)fail('resource exceeds its allocation');
      if(O.readU32BE(z,d.romStart)!==stored+4||O.readU32BE(z,d.romStart+4)!==size)fail('resource header differs from descriptor');
      if(O.statGateRelocation.crc32(z.subarray(end,end+20))!==O.readU32BE(z,end+20))fail('descriptor checksum mismatch');
      if(!z.subarray(end+24,end+64).every(function(v){return v===0;}))fail('unknown descriptor data');
      var decoded=O.lzssDecode(z,d.romStart+8,size);
      if(decoded.bytesConsumed!==logical)fail('compressed resource size mismatch');
      if(O.statGateRelocation.crc32(decoded.output)!==O.readU32BE(z,end+16))fail('resource checksum mismatch');
      if(!z.subarray(d.romStart+8+logical,d.romStart+8+stored).every(function(v){return v===3;})||!blank(z,d.romStart+8+stored,end-(d.romStart+8+stored)))fail('resource padding mismatch');
      if(!blank(z,0x27B0800,1024))fail('conflicting legacy promotion allocation');
      d.owners.forEach(function(owner){if(!matches(z,owner.start,owner.familyHex))fail('incomplete rebuilt family code: '+owner.source);});
      var raw=decoded.output,offset=d.metadataOffset;
      if(O.readU32BE(raw,offset)!==MAGIC||O.readU32BE(raw,offset+4)!==size-offset-8)fail('invalid catalog header');
      var model=validate(JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(raw.subarray(offset+8))));
      var byId={};model.families.forEach(function(f){byId[f.id]=f;});
      model.assignments.forEach(function(id,index){
        if(O.readU32BE(raw,d.tableOffset+index*4)!==((byId[id].behavior*65536+id)>>>0))fail('catalog and runtime table disagree');
      });
      return {state:'owned',model:model,decoded:raw,logicalStreamBytes:logical,storedStreamBytes:stored,
        payloadBytes:stored+4,containerBytes:stored+8};
    } catch(error) { return {state:'foreign',reason:error.message}; }
  }
  function ensure(rom) {
    if(!rom.layout||rom.layout.id!=='us-rev0')return null;
    if(!rom.classFamilies){
      var found=inspect(rom.z64);
      if(found.state==='foreign')fail(found.reason);
      rom.classFamilies=found.state==='owned'?found.model:retail(rom);
    }
    return rom.classFamilies;
  }
  function collect(rom) {
    var model=ensure(rom);if(!model)return null;
    var clean=validate(model);
    return JSON.stringify(clean)===JSON.stringify(retail(rom))?null:clean;
  }
  function add(rom,name,templateId) {
    if(typeof name!=='string')fail('a family name is required');
    var current=ensure(rom),model=copy(current),template=model.families.find(function(f){return f.id===templateId;});
    if(!template)fail('select a family template');
    var id=Math.max.apply(null,[15].concat(model.families.map(function(f){return f.id;})))+1;
    model.families.push({id:id,name:String(name).trim(),behavior:template.behavior});
    rom.classFamilies=validate(model);return id;
  }
  function assign(rom,classId,familyId) {
    if(!Number.isInteger(classId)||classId<1||classId>=81)fail('this class has no rebuilt promotion entry yet');
    var model=copy(ensure(rom));model.assignments[classId]=familyId;
    rom.classFamilies=validate(model);
  }
  function build(rom) {
    var d=spec(),model=validate(ensure(rom));
    var json=new TextEncoder().encode(JSON.stringify(model));
    var raw=new Uint8Array(d.metadataOffset+8+json.length);
    if(raw.length>65536)fail('family catalog exceeds the resource allocation');
    raw.set(O.statGateRelocation.buildDecoded(rom.statGates));
    var byId={};model.families.forEach(function(f){byId[f.id]=f;});
    model.assignments.forEach(function(id,index){O.writeU32BE(raw,d.tableOffset+index*4,byId[id].behavior*65536+id);});
    O.writeU32BE(raw,d.metadataOffset,MAGIC);O.writeU32BE(raw,d.metadataOffset+4,json.length);raw.set(json,d.metadataOffset+8);
    var compressed=O.lzssCompress(raw),stored=(compressed.length+1)&~1;
    if(stored+8>d.romSize-64)fail('compressed catalog exceeds its allocation');
    if(!same(O.lzssDecode(compressed,0,raw.length).output,raw))fail('resource compression failed readback');
    var out=new Uint8Array(d.romSize);out.fill(255);
    O.writeU32BE(out,0,stored+4);O.writeU32BE(out,4,raw.length);out.fill(3,8,8+stored);out.set(compressed,8);
    var end=d.romSize-64;out.fill(0,end);
    [MAGIC,1,compressed.length,raw.length,O.statGateRelocation.crc32(raw)].forEach(function(v,i){O.writeU32BE(out,end+i*4,v);});
    O.writeU32BE(out,end+20,O.statGateRelocation.crc32(out.subarray(end,end+20)));
    return {bytes:out,model:model,decoded:raw};
  }
  // The ordinary stat parser keeps exposing its eight-byte-per-class model.
  function statState(z,profile,original) {
    var result=inspect(z);
    if(result.state!=='owned')return {state:'foreign',active:false,profile:profile,original:original,reason:result.reason,requests:[]};
    var controller=O.sourceRedirect.classify(z);
    if(!controller.ok||controller.entries.some(function(r){return [profile.sizeSourceOffset,profile.payloadSourceOffset,profile.secondPayloadSourceOffset].indexOf(r.sourceRomOffset)!==-1;}))
      return {state:'foreign',active:false,profile:profile,original:original,reason:'Family resource has malformed or leftover promotion redirects.',requests:[]};
    return Object.assign({},result,{active:true,profile:profile,original:original,controller:controller,
      familyOwnerStart:spec().romStart,familyOwnerSize:spec().romSize,
      descriptor:{directNative:true,family:true,requests:[],redirectCount:0},requests:[],decoded:result.decoded.slice(0,648)});
  }
  O.classFamilies={ensure:ensure,retail:retail,validate:validate,collect:collect,add:add,assign:assign,
    build:build,inspect:inspect,hasEvidence:hasEvidence,statState:statState,
    isActive:function(rom){return !!collect(rom);},bytes:bytes};
})(window.OB64);
