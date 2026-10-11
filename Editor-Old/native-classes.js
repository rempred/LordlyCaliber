// Classes data rebuild and source-built promotion loader owners (US Rev 0).
// Animation remains with its existing shared Art and Animation exporter.
window.OB64 = window.OB64 || {};
(function(OB64) {
  'use strict';
  var PIN = 'FA220708F417013307C96DF296334D194BA866D5E8D3F2415EAC6A8B265BABA0';
  var issued = new WeakMap();
  var verified = null;
  var RETIRED = ['classes', 'class-descriptions', 'action-descriptions', 'stat-gates',
    'direct.class-definitions', 'text.class-descriptions', 'text.action-descriptions', 'stat.class-change-gates'];
  function fail(message) { throw new Error('Classes rebuild: ' + message); }
  function sha(bytes) { return OB64.consumableEffects.sha256HexSync(bytes).toUpperCase(); }
  function hashObject(value) { return sha(new TextEncoder().encode(JSON.stringify(value))); }
  function freeze(value) {
    if (value && typeof value === 'object') { Object.keys(value).forEach(function(k) { freeze(value[k]); }); Object.freeze(value); }
    return value;
  }
  function data() {
    if (verified) return verified;
    var bundle = OB64.NATIVE_CLASSES_DATA;
    if (!bundle || bundle.payloadSha256 !== PIN || hashObject(bundle.payload) !== PIN) fail('generated source/layout bundle failed authentication');
    verified = freeze(bundle.payload);
    return verified;
  }
  function hex(value) {
    var out = new Uint8Array(value.length / 2);
    for (var i=0;i<out.length;i++) out[i]=parseInt(value.slice(i*2,i*2+2),16);
    return out;
  }
  function equal(a,b) { if(a.length!==b.length)return false;for(var i=0;i<a.length;i++)if(a[i]!==b[i])return false;return true; }
  function at(bytes,start,expected) { return equal(bytes.subarray(start,start+expected.length),expected); }
  function region(start,size,label) { return {kind:'rom',start:start,size:size,label:label}; }
  function ownerRegions(id) {
    var d=data();
    if(id==='classes.records')return [region(d.records.start,d.records.size,'complete class records')];
    var text=d.descriptions.find(function(row){return id==='classes.text.'+row.kind;});
    if(text)return [region(text.start,text.size,'complete '+text.kind+' description resource')];
    if(id==='classes.promotion')return [region(0x23CDECE,384,'promotion retail resource'),
      region(0x27B0800,1024,'promotion resource allocation'),
      region(d.families.romStart,d.families.romSize,'expanded family resource')]
      .concat(d.families.owners.map(function(loader){return region(loader.start,loader.size,loader.source);}));
    return null;
  }
  function owns(id) { return id==='classes.records'||id==='classes.text.classes'||id==='classes.text.actions'||id==='classes.promotion'; }
  function rejectsLegacy(id) { return RETIRED.indexOf(id)!==-1; }
  function promotionLoaderState(bytes) {
    if(OB64.classFamilies&&OB64.classFamilies.hasEvidence(bytes))return OB64.classFamilies.inspect(bytes).state==='owned'?'families':'foreign';
    var states=data().promotion.loaders.map(function(loader){
      if(at(bytes,loader.start,hex(loader.retailHex)))return 'retail';
      if(at(bytes,loader.start,hex(loader.relocatedHex)))return 'relocated';
      return 'foreign';
    });
    return states.every(function(state){return state===states[0];})?states[0]:'mixed';
  }
  function hasNativePromotionReference(bytes) {
    if(OB64.classFamilies&&OB64.classFamilies.hasEvidence(bytes))return true;
    return data().promotion.loaders.some(function(loader){
      var expected=hex(loader.relocatedHex);
      return at(bytes,loader.keySite,expected.subarray(loader.keySite-loader.start,loader.keySite-loader.start+8));
    });
  }
  function buildRecords(records,source) {
    var spec=data().records;
    if(!records||records.length!==spec.count)fail('the complete 166-record model is required');
    var out=source.slice(spec.start,spec.start+spec.size);
    if(out.length!==spec.size)fail('class table is outside the source');
    for(var index=spec.firstEditable;index<spec.count;index++) {
      var record=records[index];
      if(!record||record.index!==index)fail('class record '+index+' is missing or reordered');
      spec.fields.forEach(function(field){
        var value=field.path.split('.').reduce(function(current,key){return current==null?undefined:current[key];},record);
        var maximum=field.width===2?65535:255;
        if(!Number.isInteger(value)||value<0||value>maximum)fail('class '+(index-1)+' '+field.path+' is outside 0..'+maximum);
        var offset=index*spec.stride+field.offset;
        if(field.width===2)OB64.writeU16BE(out,offset,value);else out[offset]=value;
      });
    }
    return out;
  }
  function signature(plan) {
    return hashObject({owners:plan.owners,crc:plan.crc,family:plan.family,
      writes:plan.writes.map(function(w){return [w.owner,w.start,sha(w.before),sha(w.bytes)];}),
      records:plan.records&&sha(plan.records),
      text:plan.text.map(function(t){return [t.kind,sha(t.decoded)];}),
      promotion:plan.promotion&&[plan.promotion.mode,sha(plan.promotion.decoded),plan.promotion.redirectRequests]});
  }
  function prepare(rom,source,dirty) {
    if(!rom.layout||rom.layout.id!=='us-rev0')fail('only US Rev 0 has rebuilt placement');
    var d=data(),plan={owners:[],writes:[],promotion:null,text:[],records:null,crc:false};
    function addOwner(id) { plan.owners.push({id:id,name:id,category:'nativeRebuild',regions:ownerRegions(id)}); }
    function write(id,start,bytes) {
      var before=source.slice(start,start+bytes.length);
      if(before.length!==bytes.length)fail('owner exceeds the ROM');
      plan.writes.push({owner:id,start:start,bytes:bytes,before:before});
      if(start<0x101000&&start+bytes.length>0x1000)plan.crc=true;
    }
    if(dirty.classDefs) {
      addOwner('classes.records');
      plan.records=buildRecords(rom.classDefs,source);
      write('classes.records',d.records.start,plan.records);
    }
    d.descriptions.forEach(function(spec){
      if(!dirty[spec.dirty])return;
      var block=rom[spec.property];
      if(!block||block.kind!==spec.kind||block.meta.headerOffset!==spec.start)fail('description resource placement is unrecognized');
      var built=OB64.descriptionCodec.buildSlot(block);
      if(built.bytes.length!==spec.size)fail('description owner size changed');
      var codecSpec=OB64.descriptionCodec.specs[spec.kind];
      built.bytes.set(source.subarray(spec.start+8+codecSpec.capacity,spec.start+spec.size),8+codecSpec.capacity);
      var id='classes.text.'+spec.kind;addOwner(id);write(id,spec.start,built.bytes);
      plan.text.push({kind:spec.kind,decoded:built.decoded});
    });
    var existingPromotion=OB64.statGateRelocation.inspect(source,rom.layout);
    var migratePromotion=existingPromotion.state==='owned'&&!existingPromotion.descriptor.directNative;
    var familyActive=OB64.classFamilies&&OB64.classFamilies.isActive(rom);
    var familyLoaded=OB64.classFamilies&&OB64.classFamilies.hasEvidence(source);
    if(familyActive) {
      var familyState=OB64.classFamilies.inspect(source);
      if(familyState.state==='foreign'||existingPromotion.state==='foreign')fail(familyState.reason||existingPromotion.reason);
      if(familyState.state==='absent'&&!source.subarray(d.families.romStart,d.families.romStart+d.families.romSize).every(function(v){return v===255;}))fail('family resource allocation is occupied');
      d.families.owners.forEach(function(owner){
        var loader=d.promotion.loaders.find(function(l){return l.start===owner.start;});
        if(!at(source,owner.start,hex(owner.retailHex))&&!at(source,owner.start,hex(owner.familyHex))&&
            !(loader&&at(source,loader.start,hex(loader.relocatedHex))))fail('family source owner was modified: '+owner.source);
      });
      var builtFamily=OB64.classFamilies.build(rom);
      addOwner('classes.promotion');plan.family=builtFamily.model;
      plan.promotion={mode:'relocated',decoded:builtFamily.decoded.slice(0,648),redirectRequests:[],
        logicalStreamBytes:OB64.readU32BE(builtFamily.bytes,d.families.romSize-56),
        payloadBytes:OB64.readU32BE(builtFamily.bytes,0),containerBytes:OB64.readU32BE(builtFamily.bytes,0)+4};
      var emptyLegacy=new Uint8Array(1024);emptyLegacy.fill(255);
      write('classes.promotion',0x27B0800,emptyLegacy);
      write('classes.promotion',d.families.romStart,builtFamily.bytes);
      d.families.owners.forEach(function(owner){write('classes.promotion',owner.start,hex(owner.familyHex));});
    } else if(dirty.statGates||migratePromotion||familyLoaded) {
      var state=promotionLoaderState(source);
      if(state!=='retail'&&state!=='relocated'&&state!=='families')fail('promotion loader functions are mixed or modified outside this owner');
      var promotion=OB64.statGateRelocation.prepare(rom.statGates,source,rom.layout,{directNative:true});
      plan.promotion=promotion;addOwner('classes.promotion');
      var original=promotion.originalContainerPreimage.slice();
      var allocated=new Uint8Array(1024);allocated.fill(0xFF);
      if(promotion.mode==='in-place') {
        original.fill(0x03,8);original.set(promotion.compressed,8);
      } else allocated=promotion.ownerBytes;
      write('classes.promotion',0x23CDECE,original);
      write('classes.promotion',0x27B0800,allocated);
      d.promotion.loaders.forEach(function(loader){write('classes.promotion',loader.start,
        hex(promotion.mode==='relocated'?loader.relocatedHex:loader.retailHex));});
      if(familyLoaded){
        var emptyFamily=new Uint8Array(d.families.romSize);emptyFamily.fill(255);
        write('classes.promotion',d.families.romStart,emptyFamily);
        d.families.owners.filter(function(owner){return !d.promotion.loaders.some(function(l){return l.start===owner.start;});})
          .forEach(function(owner){write('classes.promotion',owner.start,hex(owner.retailHex));});
      }
    }
    issued.set(plan,{signature:signature(plan),used:false});
    return plan;
  }
  function checkPlan(plan) {
    var record=issued.get(plan);
    if(!record||record.signature!==signature(plan))fail('rebuild plan was changed after preflight');
    return record;
  }
  function apply(candidate,plan) {
    var record=checkPlan(plan);
    if(record.used)fail('rebuild plan was already used');
    // Check every preimage before publishing any Classes section.
    plan.writes.forEach(function(w){if(!at(candidate.z64,w.start,w.before))fail(w.owner+' changed after planning');});
    plan.writes.forEach(function(w){candidate.z64.set(w.bytes,w.start);});
    record.used=true;
    return {crc:plan.crc,owners:plan.owners.map(function(owner){return owner.id;}),writes:plan.writes.length};
  }
  function validateApplied(candidate,plan) {
    if(!checkPlan(plan).used)fail('rebuild plan was not applied');
    plan.writes.forEach(function(w){if(!at(candidate.z64,w.start,w.bytes))fail(w.owner+' failed complete-section readback');});
    if(plan.records&&!equal(buildRecords(OB64.parseClassDefs(candidate.z64),candidate.z64),plan.records))fail('class fields failed reopen');
    plan.text.forEach(function(text){var parsed=OB64.descriptionCodec.parseBlock(text.kind,candidate.z64);
      if(!equal(OB64.descriptionCodec.serializeRecords(parsed),text.decoded))fail(text.kind+' text failed reopen');});
    if(plan.promotion) {
      var result=OB64.statGateRelocation.validate(candidate.z64,candidate.layout,plan.promotion.decoded);
      if(!result.ok)fail(result.reason);
      if(promotionLoaderState(candidate.z64)!==(plan.family?'families':(plan.promotion.mode==='relocated'?'relocated':'retail')))fail('promotion loader references failed reopen');
      if(plan.family&&JSON.stringify(OB64.classFamilies.inspect(candidate.z64).model)!==JSON.stringify(plan.family))fail('family catalog failed reopen');
    }
    return {summary:'Complete Classes sections and native promotion references verified',details:{owners:plan.owners.map(function(o){return o.id;}),writes:plan.writes.length}};
  }
  OB64.nativeClasses={verifyData:data,owns:owns,ownerRegions:ownerRegions,rejectsLegacy:rejectsLegacy,
    promotionLoaderState:promotionLoaderState,hasNativePromotionReference:hasNativePromotionReference,
    buildRecords:buildRecords,prepare:prepare,apply:apply,validateApplied:validateApplied};
})(window.OB64);
