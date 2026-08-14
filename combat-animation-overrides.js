// OB64 combat body-animation Normal/Blocked selector override support.
window.OB64 = window.OB64 || {};
(function() {
  'use strict';
  var D = OB64.COMBAT_ANIMATION_OVERRIDE_DATA;
  if (!D || D.schemaVersion !== 2) throw new Error('OBSO-v2 combat animation data was not loaded');
  var L = D.layout, V1 = D.legacyV1;

  function bytes(hex) { var out=new Uint8Array(hex.length/2); for(var i=0;i<out.length;i++)out[i]=parseInt(hex.substr(i*2,2),16); return out; }
  function same(a,b) { if(!a||!b||a.length!==b.length)return false; for(var i=0;i<a.length;i++)if(a[i]!==b[i])return false; return true; }
  function slice(z,o,n) { return z.slice(o,o+n); }
  function fill(a,v) { for(var i=0;i<a.length;i++)if(a[i]!==v)return false; return true; }
  function cloneEntries(a) { return (a||[]).map(function(x){return {classId:x.classId,actionId:x.actionId,normalSelector:x.normalSelector,blockedSelector:x.blockedSelector};}); }
  function key(x) { return x.classId+':'+x.actionId; }
  function sortEntries(a) { return a.sort(function(x,y){return x.classId-y.classId||x.actionId-y.actionId;}); }
  function classInfo(id) { for(var i=0;i<D.classes.length;i++)if(D.classes[i].id===Number(id))return D.classes[i]; return null; }
  function actionInfo(id) { for(var i=0;i<D.actions.length;i++)if(D.actions[i].id===Number(id))return D.actions[i]; return null; }
  function selectorInfo(c,id) { for(var i=0;c&&i<c.selectors.length;i++)if(c.selectors[i].id===Number(id))return c.selectors[i]; return null; }
  function hex(n,w) { return '0x'+Number(n).toString(16).toUpperCase().padStart(w||2,'0'); }

  function sha256(input) {
    var k=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
    var bit=input.length*8,len=((input.length+9+63)>>6)<<6,msg=new Uint8Array(len);msg.set(input);msg[input.length]=0x80;
    var dv=new DataView(msg.buffer);dv.setUint32(len-4,bit>>>0,false);dv.setUint32(len-8,Math.floor(bit/0x100000000),false);
    var h=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19],w=new Uint32Array(64);
    function rr(x,n){return (x>>>n)|(x<<(32-n));}
    for(var off=0;off<len;off+=64){for(var i=0;i<16;i++)w[i]=dv.getUint32(off+i*4,false);for(i=16;i<64;i++){var s0=rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>>3),s1=rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>>10);w[i]=(w[i-16]+s0+w[i-7]+s1)>>>0;}var a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];for(i=0;i<64;i++){var S1=rr(e,6)^rr(e,11)^rr(e,25),ch=(e&f)^(~e&g),t1=(hh+S1+ch+k[i]+w[i])>>>0,S0=rr(a,2)^rr(a,13)^rr(a,22),maj=(a&b)^(a&c)^(b&c),t2=(S0+maj)>>>0;hh=g;g=f;f=e;e=(d+t1)>>>0;d=c;c=b;b=a;a=(t1+t2)>>>0;}for(i=0;i<8;i++)h[i]=(h[i]+[a,b,c,d,e,f,g,hh][i])>>>0;}
    var out=new Uint8Array(32),od=new DataView(out.buffer);for(i=0;i<8;i++)od.setUint32(i*4,h[i],false);return out;
  }

  function validateLogicalEntries(entries) {
    if(!Array.isArray(entries))throw new Error('Combat animation overrides must be an array');
    if(entries.length>L.capacity)throw new Error('Combat animation override capacity exceeded: '+entries.length+' / '+L.capacity);
    var seen={},out=[];
    for(var i=0;i<entries.length;i++){
      var x=entries[i];if(!x||typeof x!=='object'||Array.isArray(x))throw new Error('Override '+i+' must be an object');
      var allowed={classId:1,actionId:1,normalSelector:1,blockedSelector:1};for(var f in x)if(!allowed[f])throw new Error('Override '+i+' has unknown field '+f);
      var cid=Number(x.classId),aid=Number(x.actionId),normal=Number(x.normalSelector),blocked=Number(x.blockedSelector),ci=classInfo(cid);
      if(!Number.isInteger(cid)||cid<0||cid>255)throw new Error('Override '+i+' classId must be a u8');
      if(!Number.isInteger(aid)||aid<0||aid>255||!actionInfo(aid))throw new Error('Override '+i+' uses unsupported action '+hex(aid));
      if(!Number.isInteger(normal)||normal<0||normal>255)throw new Error('Override '+i+' normalSelector must be a u8');
      if(!Number.isInteger(blocked)||blocked<0||blocked>255)throw new Error('Override '+i+' blockedSelector must be a u8');
      if(!ci)throw new Error('Override '+i+' uses unsupported ordinary class '+hex(cid));
      if(!selectorInfo(ci,normal))throw new Error((ci.name||hex(cid))+' Normal selector '+hex(normal)+' is not nonempty and valid in all four ordinary variants');
      if(!selectorInfo(ci,blocked))throw new Error((ci.name||hex(cid))+' Blocked selector '+hex(blocked)+' is not nonempty and valid in all four ordinary variants');
      var k=cid+':'+aid;if(seen[k])throw new Error('Duplicate combat animation override for class '+hex(cid)+' / action '+hex(aid));seen[k]=true;
      out.push({classId:cid,actionId:aid,normalSelector:normal,blockedSelector:blocked});
    }
    return sortEntries(out);
  }

  function compileTable(entries) {
    entries=validateLogicalEntries(entries);
    var records=entries.map(function(x){var c=classInfo(x.classId);return {sourceArt:c.sourceArt,rawOwner:c.rawOwner,action:x.actionId,flags:0,original:0,normal:x.normalSelector,blocked:x.blockedSelector};});
    records.sort(function(a,b){return a.sourceArt-b.sourceArt||a.rawOwner-b.rawOwner||a.action-b.action||((a.flags?0:1)-(b.flags?0:1))||a.original-b.original;});
    var table=new Uint8Array(1024);for(var i=0;i<records.length;i++){var o=i*8,r=records[i];table[o]=r.sourceArt;table[o+1]=r.rawOwner;table[o+2]=r.action;table[o+3]=r.flags;table[o+4]=r.original;table[o+5]=r.normal;table[o+6]=r.blocked;}
    return {table:table,records:records};
  }
  function buildModule(entries) {var c=compileTable(entries),m=bytes(D.moduleTemplateHex),count=c.records.length;m[0x0C]=(count>>>8)&255;m[0x0D]=count&255;m.set(sha256(c.table),0x20);m.set(c.table,L.tableOffset);return {module:m,table:c.table,records:c.records};}
  function lookupRecords(records,sourceArt,rawOwner,action,original,rawMode){if(rawMode!==0&&rawMode!==1&&rawMode!==2)return original;for(var i=0;i<(records||[]).length;i++){var r=records[i];if(r.sourceArt!==sourceArt||r.rawOwner!==rawOwner||r.action!==action)continue;if(r.flags&&r.original!==original)continue;return rawMode===2?r.blocked:r.normal;}return original;}

  var RETAIL_HOOK=bytes(D.retailHookHex),V2_HOOK=bytes(D.ownedHookHex),V2_BOOT=bytes(D.bootstrapHex),V2_RETURN=bytes(D.selectorReturnPostimageHex),RETAIL_RETURN=bytes(D.selectorReturnPreimageHex);
  var V1_HOOK=bytes(V1.ownedHookHex),V1_BOOT=bytes(V1.bootstrapHex),CARRIERS=D.carrierPatches.map(function(x){return {data:x,pre:bytes(x.preimageHex),post:bytes(x.postimageHex)};});
  function immutableModuleMatches(mod,template) {var copy=new Uint8Array(mod);copy[0x0C]=copy[0x0D]=0;for(var i=0x20;i<0x40;i++)copy[i]=0;for(i=L.tableOffset;i<L.moduleSize;i++)copy[i]=0;return same(copy,bytes(template));}
  function inspectOwned(z,version) {
    var mod=slice(z,L.moduleZ64,L.moduleSize);if(mod.length!==L.moduleSize)return {ok:false,reason:'module lane is out of range'};
    if(String.fromCharCode(mod[0],mod[1],mod[2],mod[3])!=='OBSO')return {ok:false,reason:'module magic is not OBSO'};
    var dv=new DataView(mod.buffer,mod.byteOffset,mod.byteLength),fields=[dv.getUint16(4),dv.getUint16(6),dv.getUint16(8),dv.getUint16(10),dv.getUint32(0x10),dv.getUint32(0x14),dv.getUint32(0x18),dv.getUint32(0x1C)];
    if(fields.join(',')!==[version,0x40,8,128,0x800,0x40,0x400,1].join(',')||dv.getUint16(0x0E)!==0)return {ok:false,reason:'OBSO v'+version+' immutable header shape mismatch'};
    var count=dv.getUint16(0x0C);if(count>128)return {ok:false,reason:'OBSO record count exceeds 128'};
    var table=mod.slice(0x400,0x800);if(!same(sha256(table),mod.slice(0x20,0x40)))return {ok:false,reason:'OBSO table SHA-256 mismatch'};
    var raw=[],logical=[],advanced=[],previous=null,seen={};
    for(var i=0;i<128;i++){
      var o=i*8,zero=true;for(var q=0;q<8;q++)if(table[o+q])zero=false;if(i>=count){if(!zero)return {ok:false,reason:'OBSO unused table bytes are not zero'};continue;}
      var r={sourceArt:table[o],rawOwner:table[o+1],action:table[o+2],flags:table[o+3],original:table[o+4],normal:table[o+5],blocked:version===1?table[o+5]:table[o+6]};
      if((r.flags!==0&&r.flags!==1)||(!r.flags&&r.original!==0)||(version===1?(table[o+6]||table[o+7]):table[o+7]))return {ok:false,reason:'OBSO record '+i+' is malformed'};
      var sk=[r.sourceArt,r.rawOwner,r.action,r.flags?0:1,r.original].map(function(v){return String(v).padStart(3,'0');}).join(':');if(previous!==null&&sk<previous)return {ok:false,reason:'OBSO records are not in canonical order'};previous=sk;
      var dk=[r.sourceArt,r.rawOwner,r.action,r.flags,r.flags?r.original:0].join(':');if(seen[dk])return {ok:false,reason:'OBSO table contains a duplicate record'};seen[dk]=true;raw.push(r);
      var matches=D.classes.filter(function(c){return c.sourceArt===r.sourceArt&&c.rawOwner===r.rawOwner;});
      if(r.flags!==0||matches.length!==1||!actionInfo(r.action)||!selectorInfo(matches[0],r.normal)||!selectorInfo(matches[0],r.blocked))advanced.push(r);
      else logical.push({classId:matches[0].id,actionId:r.action,normalSelector:r.normal,blockedSelector:r.blocked});
    }
    var template=version===1?V1.moduleTemplateHex:D.moduleTemplateHex;if(!immutableModuleMatches(mod,template))return {ok:false,reason:'OBSO v'+version+' immutable module code mismatch'};
    if(!advanced.length){var rebuilt=version===2?buildModule(logical).module:null;if(rebuilt&&!same(rebuilt,mod))return {ok:false,reason:'OBSO module is not editor/Python-owned v2'};}
    return {ok:true,version:version,count:count,raw:raw,logical:sortEntries(logical),advanced:advanced.length>0};
  }
  function carrierState(z,post) {for(var i=0;i<CARRIERS.length;i++)if(!same(slice(z,CARRIERS[i].data.z64,4),post?CARRIERS[i].post:CARRIERS[i].pre))return false;return true;}
  function classify(z) {
    if(!z||z.length<L.moduleZ64+L.moduleSize)return {kind:'foreign',reason:'ROM is too small for selector override lanes'};
    var h=slice(z,L.hookZ64,8),boot=slice(z,L.bootstrapZ64,L.bootstrapSize),mod=slice(z,L.moduleZ64,L.moduleSize),ret=slice(z,L.selectorReturnZ64,8);
    if(same(h,RETAIL_HOOK)&&fill(boot,0)&&fill(mod,0xFF)&&same(ret,RETAIL_RETURN)&&carrierState(z,false))return {kind:'retail'};
    if(same(h,V1_HOOK)&&same(boot.slice(0,V1.bootstrapSize),V1_BOOT)&&fill(boot.slice(V1.bootstrapSize),0)&&same(ret,RETAIL_RETURN)&&carrierState(z,false)){var v1=inspectOwned(z,1);if(v1.ok){v1.kind='owned-v1';return v1;}return {kind:'foreign',reason:v1.reason};}
    if(same(h,V2_HOOK)&&same(boot,V2_BOOT)&&same(ret,V2_RETURN)&&carrierState(z,true)){var v2=inspectOwned(z,2);if(v2.ok){v2.kind='owned-v2';return v2;}return {kind:'foreign',reason:v2.reason};}
    return {kind:'foreign',reason:'selector override lanes are partial, mixed-version, or foreign'};
  }

  function initialize(rom) {
    var layout=rom&&rom.layout,supported=!!layout&&layout.id==='us-rev0'&&layout.supportsCombatAnimationOverrides!==false;
    var state={supported:supported,disabledReason:supported?'':((layout&&layout.combatAnimationOverridesReason)||'Attack Animation overrides are available only for US Rev 0.'),laneState:'unsupported',baseline:[],desired:[],advanced:false,readOnly:false,diagnostic:'',dirty:false};
    if(supported){var c=classify(rom.z64);state.laneState=c.kind;if(c.kind==='owned-v1'||c.kind==='owned-v2'){state.advanced=!!c.advanced;state.readOnly=!!c.advanced;state.diagnostic=c.advanced?'Advanced OBSO v'+c.version+' table uses exact/raw/unmappable records and is preserved read-only.':'';state.baseline=cloneEntries(c.logical);state.desired=cloneEntries(c.logical);state.rawRecords=c.raw;state.loadedVersion=c.version;}else if(c.kind==='foreign'){state.readOnly=true;state.diagnostic=c.reason;state.disabledReason='Attack Animation override lanes are foreign or partial: '+c.reason;}}
    rom.combatAnimationOverrides=state;return state;
  }
  function diff(before,after){var a={},b={},added=[],removed=[],replaced=[];(before||[]).forEach(function(x){a[key(x)]=x;});(after||[]).forEach(function(x){b[key(x)]=x;});for(var k in b)if(!a[k])added.push(b[k]);else if(a[k].normalSelector!==b[k].normalSelector||a[k].blockedSelector!==b[k].blockedSelector)replaced.push({before:a[k],after:b[k]});for(k in a)if(!b[k])removed.push(a[k]);return {added:added,replaced:replaced,removed:removed,count:added.length+replaced.length+removed.length};}
  function refresh(state){state.desired=validateLogicalEntries(state.desired);state.dirty=diff(state.baseline,state.desired).count>0;return state.dirty;}
  function setEntry(state,entry){if(!state.supported||state.readOnly)throw new Error(state.disabledReason||state.diagnostic);var next=cloneEntries(state.desired),found=false;for(var i=0;i<next.length;i++)if(key(next[i])===key(entry)){next[i]={classId:Number(entry.classId),actionId:Number(entry.actionId),normalSelector:Number(entry.normalSelector),blockedSelector:Number(entry.blockedSelector)};found=true;}if(!found)next.push(entry);state.desired=validateLogicalEntries(next);refresh(state);return state;}
  function removeEntry(state,cid,aid){if(!state.supported||state.readOnly)throw new Error(state.disabledReason||state.diagnostic);state.desired=state.desired.filter(function(x){return x.classId!==Number(cid)||x.actionId!==Number(aid);});refresh(state);return state;}
  function v2Writes(module) {var writes=[{offset:L.hookZ64,bytes:V2_HOOK,label:'selector hook'},{offset:L.bootstrapZ64,bytes:V2_BOOT,label:'selector bootstrap'},{offset:L.moduleZ64,bytes:module,label:'selector module/table'},{offset:L.selectorReturnZ64,bytes:V2_RETURN,label:'selector-return raw-mode hook'}];CARRIERS.forEach(function(x){writes.push({offset:x.data.z64,bytes:x.post,label:x.data.label});});return writes;}
  function retailWrites() {var writes=[{offset:L.hookZ64,bytes:RETAIL_HOOK,label:'restore retail selector hook'},{offset:L.bootstrapZ64,bytes:new Uint8Array(L.bootstrapSize),label:'clear selector bootstrap'},{offset:L.moduleZ64,bytes:new Uint8Array(L.moduleSize).fill(0xFF),label:'clear selector module'},{offset:L.selectorReturnZ64,bytes:RETAIL_RETURN,label:'restore selector-return preimage'}];CARRIERS.forEach(function(x){writes.push({offset:x.data.z64,bytes:x.pre,label:'restore '+x.data.label});});return writes;}
  function prepareExport(rom){var s=rom.combatAnimationOverrides;if(!s||!s.supported||s.readOnly){if(s&&s.dirty)throw new Error(s.disabledReason||s.diagnostic);return {mode:'none',writes:[],crc:false,state:s};}refresh(s);if(!s.dirty)return {mode:'none',writes:[],crc:false,state:s};if(!s.desired.length)return {mode:'uninstall',writes:retailWrites(),crc:true,state:s};var built=buildModule(s.desired);return {mode:s.laneState==='retail'?'install':(s.laneState==='owned-v1'?'upgrade':'update'),writes:v2Writes(built.module),crc:true,state:s,built:built};}
  function applyPlan(candidate,plan){if(!plan||plan.mode==='none')return {touched:[],crc:false};var current=classify(candidate.z64),expected=plan.state.laneState;if(current.kind!==expected)throw new Error('Selector override guard changed before write: expected '+expected+', found '+current.kind+(current.reason?' ('+current.reason+')':''));var touched=[];for(var i=0;i<plan.writes.length;i++){var w=plan.writes[i];candidate.z64.set(w.bytes,w.offset);touched.push(w.label);}return {touched:touched,crc:plan.crc};}
  function adopt(rom){var s=rom.combatAnimationOverrides;if(!s)return;s.baseline=cloneEntries(s.desired);s.laneState=s.desired.length?'owned-v2':'retail';s.loadedVersion=s.desired.length?2:null;s.advanced=false;s.readOnly=false;s.diagnostic='';s.dirty=false;}
  function regions(){var out=[{kind:'rom',start:L.hookZ64,size:8,label:'selector hook'},{kind:'ram',start:L.hookRam,size:8,label:'selector hook runtime'},{kind:'rom',start:L.bootstrapZ64,size:L.bootstrapSize,label:'selector bootstrap'},{kind:'ram',start:L.bootstrapRam,size:L.bootstrapSize,label:'selector bootstrap runtime'},{kind:'rom',start:L.moduleZ64,size:L.moduleSize,label:'selector module/table'},{kind:'ram',start:L.moduleRam,size:L.moduleSize,label:'selector module/table runtime'},{kind:'rom',start:L.selectorReturnZ64,size:8,label:'selector-return hook'},{kind:'ram',start:L.selectorReturnRam,size:8,label:'selector-return hook runtime'}];CARRIERS.forEach(function(x){out.push({kind:'rom',start:x.data.z64,size:4,label:x.data.label});out.push({kind:'ram',start:x.data.ram,size:4,label:x.data.label+' runtime'});});return out;}
  function collisionOwner(rom){var s=rom&&rom.combatAnimationOverrides;if(!s||(!s.dirty&&s.laneState!=='owned-v1'&&s.laneState!=='owned-v2'&&!s.desired.length))return null;return {id:'combat-animation-overrides',name:'Combat Attack Animation Overrides',regions:regions()};}
  function collectProjectPayload(state){if(!state||diff(state.baseline,state.desired).count===0)return null;return {entries:cloneEntries(state.desired)};}
  function validateProjectPayload(rom,payload,projectVersion){if(!payload||typeof payload!=='object'||Array.isArray(payload)||Object.keys(payload).some(function(k){return k!=='entries';}))throw new Error('combatAnimationOverrides payload must contain only entries');var state=rom.combatAnimationOverrides;if(!state||!state.supported||state.readOnly)throw new Error((state&&state.disabledReason)||'Combat animation overrides are unavailable');if(projectVersion===14){if(!Array.isArray(payload.entries))throw new Error('combatAnimationOverrides entries must be an array');return validateLogicalEntries(payload.entries.map(function(x,i){if(!x||typeof x!=='object'||Array.isArray(x)||Object.keys(x).some(function(k){return !{classId:1,actionId:1,selector:1}[k];})||!Object.prototype.hasOwnProperty.call(x,'selector'))throw new Error('Version-14 override '+i+' must contain only classId, actionId, and selector');return {classId:x.classId,actionId:x.actionId,normalSelector:x.selector,blockedSelector:x.selector};}));}if(projectVersion<15)throw new Error('combatAnimationOverrides requires Project version 14 or newer');return validateLogicalEntries(payload.entries);}
  function applyProjectPayload(state,entries){var before=cloneEntries(state.desired);state.desired=cloneEntries(entries);refresh(state);return diff(before,state.desired).count;}
  function selectorOptions(cid){var c=classInfo(cid);return c?c.selectors.map(function(x){return {id:x.id,status:x.status};}):[];}
  function liveRanks(def,actionId){var out=[],aid=Number(actionId);if(!def)return out;if(Number(def.b43Raw||0)===aid)out.push('Front');if(Number(def.b45Raw||0)===aid)out.push('Middle');if(Number(def.b47Raw||0)===aid)out.push('Rear');return out;}
  function liveRankStatus(def,actionId){var ranks=liveRanks(def,actionId);return ranks.length?'currently assigned: '+ranks.join(', '):'not currently assigned';}
  function eligibleSelector(ci,value){return !!selectorInfo(ci,Number(value));}
  function contextPair(ci,context){var modes=context&&context.selectorsByRawMode;if(!modes||modes.length<3)return null;if(Number(modes[0])!==Number(modes[1]))throw new Error((ci.name||hex(ci.id))+' '+context.row+' vanilla modes 0 and 1 disagree');var pair={normalSelector:Number(modes[0]),blockedSelector:Number(modes[2])};return eligibleSelector(ci,pair.normalSelector)&&eligibleSelector(ci,pair.blockedSelector)?pair:null;}
  function vanillaPairForLiveAction(classId,def,actionId){var ci=classInfo(classId),aid=Number(actionId),action=null;if(!ci)return null;for(var i=0;i<ci.currentActions.length;i++)if(Number(ci.currentActions[i].id)===aid){action=ci.currentActions[i];break;}if(!action)return null;var ranks=liveRanks(def,aid).map(function(x){return x.toLowerCase();});for(i=0;i<ranks.length;i++)for(var q=0;q<action.contexts.length;q++)if(action.contexts[q].row===ranks[i]){var pair=contextPair(ci,action.contexts[q]);if(pair)return pair;}for(i=0;i<action.contexts.length;i++){pair=contextPair(ci,action.contexts[i]);if(pair)return pair;}return null;}
  function vanillaSelectorForLiveAction(classId,def,actionId){var pair=vanillaPairForLiveAction(classId,def,actionId);return pair?pair.normalSelector:null;}
  function modalRows(state,def,classId){var cid=Number(classId),overrides={},seen={},out=[];(state&&state.desired||[]).forEach(function(row){if(row.classId===cid)overrides[row.actionId]=row;});[def&&def.b43Raw,def&&def.b45Raw,def&&def.b47Raw].forEach(function(value){var aid=Number(value||0),override=overrides[aid],pair;if(!aid||seen[aid]||!actionInfo(aid))return;seen[aid]=true;pair=override||vanillaPairForLiveAction(cid,def,aid);out.push({classId:cid,actionId:aid,normalSelector:pair?Number(pair.normalSelector):null,blockedSelector:pair?Number(pair.blockedSelector):null,overridden:!!override});});(state&&state.desired||[]).forEach(function(row){if(row.classId!==cid||seen[row.actionId])return;seen[row.actionId]=true;out.push({classId:cid,actionId:Number(row.actionId),normalSelector:Number(row.normalSelector),blockedSelector:Number(row.blockedSelector),overridden:true});});return out.sort(function(a,b){return a.actionId-b.actionId;});}
  function rearContextPair(ci,actionId){for(var i=0;ci&&i<ci.currentActions.length;i++){var action=ci.currentActions[i];if(actionId!==null&&Number(action.id)!==Number(actionId))continue;for(var q=0;q<action.contexts.length;q++)if(action.contexts[q].row==='rear'){var pair=contextPair(ci,action.contexts[q]);if(pair)return pair;}}return null;}
  function defaultPairForLiveAttack(state,classId,capturedRearAction){var ci=classInfo(classId);if(!ci)throw new Error('No accepted selector options exist for class '+hex(classId));var donor=null;for(var i=0;state&&i<state.desired.length;i++){var row=state.desired[i];if(row.classId===Number(classId)&&row.actionId===Number(capturedRearAction)&&eligibleSelector(ci,row.normalSelector)&&eligibleSelector(ci,row.blockedSelector)){donor={normalSelector:Number(row.normalSelector),blockedSelector:Number(row.blockedSelector)};break;}}if(!donor)donor=rearContextPair(ci,capturedRearAction);if(!donor)donor=rearContextPair(ci,null);if(!donor){var selector=null;for(i=0;i<ci.selectors.length;i++)if(ci.selectors[i].status==='current_explicit'){selector=Number(ci.selectors[i].id);break;}if(selector===null&&ci.selectors.length)selector=Number(ci.selectors[0].id);if(selector!==null)donor={normalSelector:selector,blockedSelector:selector};}if(!donor)throw new Error('No eligible selector default exists for '+ci.name);return donor;}
  function defaultSelectorForLiveAttack(state,classId,capturedRearAction){return defaultPairForLiveAttack(state,classId,capturedRearAction).normalSelector;}
  function applyLiveAttackChange(state,def,classId,field,newActionId){var fields={b43Raw:1,b45Raw:1,b47Raw:1};if(!def||!fields[field])throw new Error('Live class attack field is unavailable');var nextAction=Number(newActionId);if(!Number.isInteger(nextAction)||nextAction<0||nextAction>255)throw new Error('Live class attack must be a u8');var capturedRear=Number(def.b47Raw||0),previous=Number(def[field]||0),existing=null,added=false,pair=null,writable=!!state&&state.supported&&!state.readOnly;if(nextAction!==0&&writable){for(var i=0;i<state.desired.length;i++)if(state.desired[i].classId===Number(classId)&&state.desired[i].actionId===nextAction){existing=state.desired[i];break;}if(!existing){if(state.desired.length>=L.capacity)throw new Error('Combat animation override capacity is full. Remove an animation mapping first.');pair=defaultPairForLiveAttack(state,classId,capturedRear);state.desired=validateLogicalEntries(state.desired.concat([{classId:Number(classId),actionId:nextAction,normalSelector:pair.normalSelector,blockedSelector:pair.blockedSelector}]));refresh(state);added=true;}}def[field]=nextAction;return {classChanged:previous!==nextAction,overrideAdded:added,capturedRearAction:capturedRear,previousAction:previous,newAction:nextAction,normalSelector:existing?existing.normalSelector:(pair&&pair.normalSelector),blockedSelector:existing?existing.blockedSelector:(pair&&pair.blockedSelector)};}
  function describeEntry(x){var c=classInfo(x.classId),a=actionInfo(x.actionId);return (c?c.name:hex(x.classId))+' '+(a?a.name:hex(x.actionId))+' '+hex(x.actionId)+' -> Normal '+hex(x.normalSelector)+', Blocked '+hex(x.blockedSelector);}

  OB64.combatAnimationOverrides={data:D,initialize:initialize,classify:classify,validateLogicalEntries:validateLogicalEntries,compileTable:compileTable,buildModule:buildModule,lookupRecords:lookupRecords,sha256:sha256,selectorOptions:selectorOptions,classInfo:classInfo,actionInfo:actionInfo,setEntry:setEntry,removeEntry:removeEntry,refresh:refresh,diff:diff,count:function(s){return s.desired.length;},capacity:L.capacity,prepareExport:prepareExport,applyPlan:applyPlan,adopt:adopt,patchRegions:regions,collisionOwner:collisionOwner,collectProjectPayload:collectProjectPayload,validateProjectPayload:validateProjectPayload,applyProjectPayload:applyProjectPayload,liveRanks:liveRanks,liveRankStatus:liveRankStatus,vanillaSelectorForLiveAction:vanillaSelectorForLiveAction,vanillaPairForLiveAction:vanillaPairForLiveAction,modalRows:modalRows,defaultSelectorForLiveAttack:defaultSelectorForLiveAttack,defaultPairForLiveAttack:defaultPairForLiveAttack,applyLiveAttackChange:applyLiveAttackChange,describeEntry:describeEntry};
})();
