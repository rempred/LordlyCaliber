// Bounded native dialogue services. All mutable memory and external outcomes are explicit.
window.OB64 = window.OB64 || {};
(function(OB64) {
  'use strict';
  function boundary(message, code) {
    var error = new Error(message); error.code = code || 'dialogue-native-input'; throw error;
  }
  function validateHelper(helper) {
    function word(v){return Number.isInteger(v)&&v>=0&&v<=0xffffffff;}
    if(!helper||!word(helper.address)||helper.preservesOtherRegisters!==true||!Array.isArray(helper.args)||!helper.args.every(word)||
        !Number.isInteger(helper.result)||helper.result< -2147483648||helper.result>0xffffffff||!Array.isArray(helper.writes))boundary('Dialogue helper outcome has invalid fields.');
    helper.writes.forEach(function(row){if(!row||!word(row.address)||!Array.isArray(row.bytes)||row.address+row.bytes.length>0x100000000||row.bytes.length>131072||!row.bytes.every(function(v){return Number.isInteger(v)&&v>=0&&v<=255;}))boundary('Dialogue helper memory effects are invalid.');});
    if(helper.registerWrites!==undefined && (!Array.isArray(helper.registerWrites)||helper.registerWrites.some(function(row){return !row||!Number.isInteger(row.register)||row.register<1||row.register>30||row.register===29||!word(row.value);})))boundary('Dialogue helper register effects are invalid.');
    ['hi','lo'].forEach(function(key){if(helper[key]!==undefined&&!word(helper[key]))boundary('Dialogue helper arithmetic-register effects are invalid.');});
  }
  function Machine(code, allocations) {
    this.code = code; this.regions = []; this.r = new Uint32Array(32);
    this.hi = 0; this.lo = 0; this.steps = 0;
    var total = 0;
    (allocations || []).forEach(function(row) {
      if (!Number.isInteger(row.address) || row.address < 0 || row.address > 0xffffffff ||
          !(row.bytes instanceof Uint8Array) || !row.bytes.length ||
          row.address + row.bytes.length > 0x100000000) boundary('Invalid dialogue memory allocation.');
      total += row.bytes.length;
      if (total > 131072) boundary('Dialogue memory exceeds its bounded allocation.', 'dialogue-memory-bound');
      if (this.regions.some(function(old) { return row.address < old.address + old.bytes.length && old.address < row.address + row.bytes.length; })) boundary('Dialogue memory allocations overlap.');
      this.regions.push({address:row.address, bytes:new Uint8Array(row.bytes), writable:row.writable === true});
    }, this);
  }
  Machine.prototype.region = function(address, count, write) {
    address >>>= 0;
    var region = this.regions.find(function(row) { return address >= row.address && address + count <= row.address + row.bytes.length; });
    if (!region || (write && !region.writable)) boundary('Dialogue ' + (write ? 'write' : 'read') + ' requires supplied memory at RAM 0x' + address.toString(16) + '.', 'dialogue-reached-memory');
    return region;
  };
  Machine.prototype.get = function(address, count) {
    address >>>= 0; count = count || 4;
    var row = this.region(address, count, false), value = 0;
    for (var i = 0; i < count; i++) value = (value * 256 + row.bytes[address - row.address + i]) >>> 0;
    return value;
  };
  Machine.prototype.put = function(address, value, count) {
    address >>>= 0; count = count || 4;
    var row = this.region(address, count, true);
    for (var i = count - 1; i >= 0; i--) { row.bytes[address - row.address + i] = value & 255; value >>>= 8; }
  };
  Machine.prototype.step = function(pc) {
    if(pc===0x8019db6c)for(var digit=0;digit<3;digit++) {
      var operand=this.get(this.r[4]+digit,1);
      if(operand<48||operand>57)boundary('Dialogue numeric controls require three valid decimal bytes.','dialogue-decimal-input');
    }
    var w = this.code[pc];
    if (w === undefined) boundary('Dialogue reached an unqualified native service at RAM 0x' + pc.toString(16) + '.', 'dialogue-helper-outcome');
    var op=w>>>26, rs=w>>>21&31, rt=w>>>16&31, rd=w>>>11&31, sh=w>>>6&31, fn=w&63;
    var imm=w&65535, si=imm<<16>>16, r=this.r, target=null, annul=false, take;
    if (op === 0) {
      switch(fn) {
      case 0:r[rd]=r[rt]<<sh;break; case 2:r[rd]=r[rt]>>>sh;break; case 3:r[rd]=(r[rt]|0)>>sh;break;
      case 4:r[rd]=r[rt]<<(r[rs]&31);break; case 6:r[rd]=r[rt]>>>(r[rs]&31);break; case 7:r[rd]=(r[rt]|0)>>(r[rs]&31);break;
      case 8:target=r[rs];break; case 9:target=r[rs];r[rd]=(pc+8)>>>0;break;
      case 16:r[rd]=this.hi;break; case 18:r[rd]=this.lo;break;
      case 24:case 25:
        var product=BigInt(fn===24?r[rs]|0:r[rs])*BigInt(fn===24?r[rt]|0:r[rt]);
        this.lo=Number(BigInt.asUintN(32,product));this.hi=Number(BigInt.asUintN(32,product>>32n));break;
      case 26:case 27:
        var a=fn===26?r[rs]|0:r[rs], b=fn===26?r[rt]|0:r[rt];
        if (!b || (fn===26 && a===-2147483648 && b===-1)) boundary('Dialogue division requires valid native operands.', 'dialogue-division-input');
        this.lo=Math.trunc(a/b)>>>0;this.hi=(a-Math.trunc(a/b)*b)>>>0;break;
      case 33:r[rd]=r[rs]+r[rt];break; case 35:r[rd]=r[rs]-r[rt];break;
      case 36:r[rd]=r[rs]&r[rt];break;case 37:r[rd]=r[rs]|r[rt];break;case 38:r[rd]=r[rs]^r[rt];break;case 39:r[rd]=~(r[rs]|r[rt]);break;
      case 42:r[rd]=(r[rs]|0)<(r[rt]|0)?1:0;break;case 43:r[rd]=r[rs]<r[rt]?1:0;break;
      default:boundary('Dialogue reached an unsupported native instruction.', 'dialogue-instruction');
      }
    } else if (op===2 || op===3) {target=(((pc+4)&0xf0000000)|((w&0x3ffffff)<<2))>>>0;if(op===3)r[31]=pc+8;}
    else if (op===1 || op===4 || op===5 || op===6 || op===7 || op===20 || op===21 || op===22 || op===23) {
      if(op===1) {if(rt>3)boundary('Unsupported native branch.', 'dialogue-instruction');take=rt&1?(r[rs]|0)>=0:(r[rs]|0)<0;annul=rt>=2&&!take;}
      else {take=op===4||op===20?r[rs]===r[rt]:op===5||op===21?r[rs]!==r[rt]:op===6||op===22?(r[rs]|0)<=0:(r[rs]|0)>0;annul=op>=20&&!take;}
      target=(take?pc+4+si*4:pc+8)>>>0;
    } else if(op===9)r[rt]=r[rs]+si;
    else if(op===10)r[rt]=(r[rs]|0)<si?1:0;
    else if(op===11)r[rt]=r[rs]<(si>>>0)?1:0;
    else if(op===12)r[rt]=r[rs]&imm;
    else if(op===13)r[rt]=r[rs]|imm;
    else if(op===14)r[rt]=r[rs]^imm;
    else if(op===15)r[rt]=imm<<16;
    else if(op===32||op===33||op===35||op===36||op===37) {
      var n=op===32||op===36?1:op===35?4:2, v=this.get((r[rs]+si)>>>0,n);
      r[rt]=op===32?v<<24>>24:op===33?v<<16>>16:v;
    } else if(op===40||op===41||op===43)this.put((r[rs]+si)>>>0,r[rt],op===40?1:op===41?2:4);
    else if(op===42) {
      var address=(r[rs]+si)>>>0, count=4-(address&3);
      for(var i=0;i<count;i++)this.put(address+i,r[rt]>>>(24-i*8),1);
    }
    else boundary('Dialogue reached an unsupported native instruction.', 'dialogue-instruction');
    r[0]=0;this.steps++;return {target:target,annul:annul};
  };
  // Every suspension leaves exact memory and registers in the machine. Callers must
  // resume this generator before publishing a completed service or query result.
  Machine.prototype.run = function*(entry, args, helpers, limit) {
    this.r.fill(0);this.r[29]=0x807ff000;this.r[31]=0xdead0000;
    (args||[]).forEach(function(v,i){if(i<4)this.r[4+i]=v;else this.put(this.r[29]+16+4*(i-4),v);},this);
    var pc=entry>>>0, start=this.steps, cursor=0;
    helpers=helpers||[];limit=limit||262144;
    while(pc!==0xdead0000) {
      if(this.steps-start>=limit)boundary('Dialogue service exceeded its instruction ceiling.', 'dialogue-instruction-bound');
      var result=this.step(pc);
      if(result.target===null)pc=(pc+4)>>>0;
      else {
        if(!result.annul && this.step((pc+4)>>>0).target!==null)boundary('Native control in a delay slot is unsupported.', 'dialogue-instruction');
        pc=result.target;
        if(this.code[pc]===undefined && pc!==0xdead0000) {
          if(this.serviceHelper&&this.serviceHelper(pc)) {pc=this.r[31];continue;}
          var shared=this.sharedOutcome&&this.sharedOutcome(pc),helper=shared||helpers[cursor];
          // Audio, archive lookup, formatter, copy, free, allocation, registration,
          // and optional position adjustment retain explicit external outcomes.
          var arity=({0x800ea9bc:5,0x8007938c:4,0x800934b0:3,0x80093540:3,
            0x800712c4:1,0x80070f30:1,0x80076f5c:7,0x8019d5d0:0})[pc];
          if(arity===undefined)boundary('Dialogue reached a helper outside the accepted service boundary.','dialogue-unqualified-helper');
          if(helper)validateHelper(helper);
          if(!helper || helper.address!==pc || helper.preservesOtherRegisters!==true || !Array.isArray(helper.args) || helper.args.length!==arity || helper.args.some(function(v,i){return v!==(i<4?this.r[4+i]:this.get(this.r[29]+16+4*(i-4)));},this)) boundary('Dialogue requires the exact next external helper outcome.', 'dialogue-helper-outcome');
          if(pc===0x80076f5c) {
            var freeSlot=-1;for(var slot=0;slot<6;slot++)if(!(this.get(POOL+slot*STRIDE,2)&0x8000)){freeSlot=slot;break;}
            if(freeSlot<0)boundary('Dialogue resource pool is exhausted.','dialogue-resource-exhaustion');
            if(helper.result!==freeSlot)boundary('Dialogue registration outcome must select the first free resource slot.','dialogue-registration-input');
          }
          (helper.writes||[]).forEach(function(row){row.bytes.forEach(function(v,i){this.put(row.address+i,v,1);},this);},this);
          if(!Number.isInteger(helper.result))boundary('Dialogue helper return value is missing.', 'dialogue-helper-outcome');
          (helper.registerWrites||[]).forEach(function(row){this.r[row.register]=row.value;},this);
          if(helper.hi!==undefined)this.hi=helper.hi;if(helper.lo!==undefined)this.lo=helper.lo;
          this.r[2]=helper.result;if(!shared)cursor++;pc=this.r[31];
        }
      }
      if((this.steps-start)%2048<2)yield {kind:'dialogue-native-progress',instructions:this.steps-start};
    }
    if(cursor!==helpers.length)boundary('Dialogue service supplied unused helper outcomes.', 'dialogue-helper-order');
    return this.r[2];
  };
  var POOL=0x800e82c8, RECORD=0x800e7a30, PAYLOAD=0x800e91d0, STRIDE=0xa8;
  function bytes(hex) {
    if(typeof hex!=='string'||!hex.length||hex.length%2||!/^[0-9a-f]+$/i.test(hex))boundary('Dialogue memory requires hexadecimal bytes.');
    return Uint8Array.from(hex.match(/../g),function(v){return parseInt(v,16);});
  }
  function hex(data) {return Array.from(data,function(v){return v.toString(16).padStart(2,'0');}).join('');}
  // Preview-owned first-fit arena. This implements the uncompressed payload
  // wrapper contract; it does not emulate the retail allocator's global trees.
  function PayloadStorage(machine, config) {
    if(!config||config.kind!=='preview-arena-v1'||!Number.isInteger(config.address)||config.address<=0||config.address>0xffffffff||
        config.address%16||!Number.isInteger(config.byteLength)||config.byteLength<16||config.byteLength>65536||config.byteLength%16||config.address<0x80200000||config.address+config.byteLength>0x80700000)
      boundary('Payload storage requires a bounded, aligned preview arena.','dialogue-storage-arena');
    machine.region(config.address,config.byteLength,true);
    this.machine=machine;this.config={kind:config.kind,address:config.address,byteLength:config.byteLength};this.leases=[];
  }
  PayloadStorage.prototype.reserve = function(owner,handle,length) {
    var size=Math.ceil((length+6)/16)*16,c=this.config;
    if(typeof owner!=='string'||!owner||!Number.isInteger(length)||length<1||length>65535||!Number.isInteger(handle)||handle%16||handle<c.address||handle+size>c.address+c.byteLength||
        this.leases.some(function(r){return r.owner===owner||(handle<r.handle+r.size&&r.handle<handle+size);}))
      boundary('Payload allocation is outside its arena or already owned.','dialogue-payload-owner');
    var lease={owner:owner,handle:handle,length:length,size:size};this.leases.push(lease);this.leases.sort(function(a,b){return a.handle-b.handle;});return lease;
  };
  PayloadStorage.prototype.save = function(owner,flags,source,length) {
    if(!Number.isInteger(flags)||flags<0||flags>255||!Number.isInteger(length)||length<0||length>65535||(length&&(flags&1)))
      boundary('Shared payload storage supports uncompressed unsigned-length payloads.','dialogue-payload-mode');
    if(this.leases.some(function(r){return r.owner===owner;}))boundary('Payload owner must restore or release before saving again.','dialogue-payload-owner');
    if(!length)return 0;
    var m=this.machine,c=this.config,size=Math.ceil((length+6)/16)*16,handle=c.address;
    for(var i=0;i<this.leases.length;i++){var lease=this.leases[i];if(handle+size<=lease.handle)break;handle=lease.handle+lease.size;}
    if(handle+size>c.address+c.byteLength)boundary('The declared preview payload arena is exhausted.','dialogue-storage-exhaustion');
    var data=Uint8Array.from({length:length},function(_,j){return m.get(source+j,1);});
    this.reserve(owner,handle,length);
    m.put(handle,flags,1);m.put(handle+2,length,2);m.put(handle+4,length,2);
    data.forEach(function(v,j){m.put(handle+6+j,v,1);});return handle;
  };
  PayloadStorage.prototype.restore = function(owner,handle,destination) {
    var lease=this.leases.find(function(r){return r.owner===owner&&r.handle===handle;}),m=this.machine;
    if(!lease||m.get(handle+2,2)!==lease.length||m.get(handle+4,2)!==lease.length||(m.get(handle,1)&1))
      boundary('Saved payload does not match its arena ownership and header.','dialogue-payload-identity');
    m.region(destination,lease.length,true);
    var data=Uint8Array.from({length:lease.length},function(_,i){return m.get(handle+6+i,1);});
    data.forEach(function(v,i){m.put(destination+i,v,1);});this.release(owner);return lease.length;
  };
  PayloadStorage.prototype.release = function(owner) {this.leases=this.leases.filter(function(r){return r.owner!==owner;});};
  function Engine(input, rom) {
    if(!input || !Array.isArray(input.memory) || !Array.isArray(input.owners) || input.owners.length!==6)boundary('Dialogue requires initial memory and six resource ownership entries.');
    if(!(rom instanceof Uint8Array)||!OB64.cutsceneDialogueWords)boundary('Dialogue requires its qualified original image.');
    var code={};
    var words=OB64.cutsceneDialogueWords;
    if(input.lifecycle!==undefined){if(!input.lifecycle||typeof input.lifecycle!=='object'||!OB64.cutsceneDialogueLifecycleWords||!OB64.cutsceneDialogueLifecycle)boundary('Dialogue lifecycle services are unavailable or invalid.','dialogue-lifecycle-input');words=words.concat(OB64.cutsceneDialogueLifecycleWords);}
    words.forEach(function(row){
      var a=row[1];
      if(a+4>rom.length||((rom[a]*16777216+rom[a+1]*65536+rom[a+2]*256+rom[a+3])>>>0)!==row[2])boundary('Dialogue code differs from its qualified original image.','dialogue-image-identity');
      code[row[0]]=row[2];
    });
    this.machine=new Machine(code,input.memory.map(function(row){return {address:row.address,bytes:bytes(row.hex),writable:row.writable};}));
    this.owners=input.owners.map(function(row,i){
      var flags=this.machine.get(POOL+i*STRIDE,2);
      if(!row){if(flags&0x8000)boundary('Active dialogue resource ownership is missing.');return null;}
      if(typeof row.ownerId!=='string'||!row.ownerId||!(flags&0x8000))boundary('Dialogue resource ownership does not match initial memory.');
      var payload=row.payloadHex?bytes(row.payloadHex):null;
      if(payload && payload.length!==0x478)boundary('Dialogue saved payload must contain 1,144 bytes.');
      if(payload&&new DataView(payload.buffer).getUint16(0x34)>0x300)boundary('Initial dialogue output exceeds its native buffer.','dialogue-output-bound');
      if(this.machine.get(POOL+i*STRIDE+0x10)===0x80198be8 && (flags&0x2000) && !payload)boundary('Initialized dialogue requires its saved payload.');
      if(payload) {
        var handle=this.machine.get(POOL+i*STRIDE+0x24);
        if(!handle||this.machine.get(handle+2,2)!==0x478||payload.some(function(v,j){return this.machine.get(handle+6+j,1)!==v;},this))boundary('Dialogue payload differs from its current saved allocation.','dialogue-payload-identity');
      }
      return {ownerId:row.ownerId,payload:payload};
    },this);
    if(new Set(this.owners.filter(Boolean).map(function(row){return row.ownerId;})).size!==this.owners.filter(Boolean).length)boundary('Dialogue owner identities must be unique.');
    this.payloadStorage=input.payloadStorage===undefined?null:new PayloadStorage(this.machine,input.payloadStorage);
    if(this.payloadStorage)this.owners.forEach(function(owner,slot){if(owner&&owner.payload)this.payloadStorage.reserve(owner.ownerId,this.machine.get(POOL+slot*STRIDE+0x24),owner.payload.length);},this);
    this.lifecycle=input.lifecycle===undefined?null:new OB64.cutsceneDialogueLifecycle(this,input.lifecycle,rom);
  }
  Engine.prototype.copy = function(from,to,length) {
    var data=[];for(var i=0;i<length;i++)data.push(this.machine.get(from+i,1));
    data.forEach(function(v,i){this.machine.put(to+i,v,1);},this);
  };
  Engine.prototype.reconcile = function(event) {
    var claimed=event.registeredOwners||[],used=0;
    for(var slot=0;slot<6;slot++) {
      var active=!!(this.machine.get(POOL+slot*STRIDE,2)&0x8000);
      if(!active){if(this.payloadStorage&&this.owners[slot])this.payloadStorage.release(this.owners[slot].ownerId);this.owners[slot]=null;continue;}
      if(this.owners[slot])continue;
      var row=claimed.find(function(r){return r.slot===slot;});
      if(!row||typeof row.ownerId!=='string'||!row.ownerId||this.owners.some(function(r){return r&&r.ownerId===row.ownerId;}))boundary('Native registration requires the new resource owner identity.','dialogue-registration-owner');
      this.owners[slot]={ownerId:row.ownerId,payload:null};used++;
    }
    if(used!==claimed.length)boundary('Dialogue service supplied unused resource ownership outcomes.','dialogue-registration-owner');
  };
  Engine.prototype.find = function(windowId) {
    for(var i=0;i<6;i++){var r=POOL+i*STRIDE;if((this.machine.get(r,2)&0x8000)&&this.machine.get(r+0x10)===0x80198be8&&this.machine.get(r+0x8f,1)===(windowId&255))return i;}
    return -1;
  };
  Engine.prototype.query = function(windowId) {
    var run=this.machine.run(0x8019f174,[windowId&255],[],1024),result=run.next();
    while(!result.done)result=run.next();
    return result.value;
  };
  Engine.prototype.resume = function(windowId) {
    var slot=this.find(windowId);if(slot<0)return;
    var r=POOL+slot*STRIDE;this.machine.put(r+0x8a,this.machine.get(r+0x8a,1)|2,1);this.machine.put(r+0xe,0xf000,2);
  };
  Engine.prototype.close = function(windowId) {
    var run=this.machine.run(0x8019f354,[windowId&255],[],1024),result=run.next();
    while(!result.done)result=run.next();
  };
  Engine.prototype.register = function(row, words) {
    if(!row||!Array.isArray(row.directorWords)||row.directorWords.length!==words.length||row.directorWords.some(function(v,i){return (v>>>0)!==(words[i]>>>0);}))boundary('Dialogue requires the matching constructor and registration outcome.','dialogue-registration-input');
    var slot=-1;for(var i=0;i<6;i++)if(!(this.machine.get(POOL+i*STRIDE,2)&0x8000)){slot=i;break;}
    if(slot<0)boundary('Dialogue resource pool is exhausted.','dialogue-resource-exhaustion');
    if(row.slot!==slot||typeof row.ownerId!=='string'||!row.ownerId||this.owners.some(function(o){return o&&o.ownerId===row.ownerId;}))boundary('Dialogue registration must use its first free slot and a fresh owner.');
    var record=bytes(row.recordHex),view=new DataView(record.buffer);
    if(record.length!==STRIDE||view.getUint32(0x10)!==0x80198be8||view.getUint16(0)!==0xc800||record[0x8f]!==(words[1]&255))boundary('Dialogue registration outcome has invalid native identity or initialization flags.');
    record.forEach(function(v,i){this.machine.put(POOL+slot*STRIDE+i,v,1);},this);
    for(var control=0;control<8;control++)this.machine.put(0x8019ee40+control,0,1);
    this.owners[slot]={ownerId:row.ownerId,payload:null};return slot;
  };
  Engine.prototype.service = function*(event, nativeDispatch) {
    function rejectUnused(outcomes) {
      if((outcomes||[]).length)boundary('Dialogue service supplied unused helper outcomes.','dialogue-helper-order');
    }
    if(this.payloadStorage&&event.storage!==undefined)boundary('Shared payload storage must not receive recorded storage outcomes.','dialogue-storage-outcome');
    if(!event.eligible) {
      rejectUnused(event.helpers);rejectUnused(event.releaseHelpers);
      if((event.registeredOwners||[]).length)boundary('Dialogue service supplied unused resource ownership outcomes.','dialogue-registration-owner');
    }
    if(event.service==='opening'||event.service==='closing'||event.service==='priority') {
      rejectUnused(event.releaseHelpers);
      if(!event.eligible)return;
      yield* this.machine.run(event.service==='opening'?0x800775ec:event.service==='closing'?0x80077bf8:0x8007819c,[],event.helpers);
      this.reconcile(event);
      return;
    }
    var m=this.machine, slot=event.slot, owner=this.owners[slot], r=POOL+slot*STRIDE;
    if(!Number.isInteger(slot)||slot<0||slot>5||!owner||owner.ownerId!==event.ownerId)boundary('Dialogue service targets a missing or replaced owner.','dialogue-service-owner');
    if(!event.eligible)return;
    var flags=m.get(r,2);if(!(flags&0x8000))boundary('Dialogue callback requires an active resource.');
    var storage=event.storage;
    if(!this.payloadStorage&&(!storage||storage.saveReturned!==true||!Number.isInteger(storage.saveHandle)||storage.saveHandle<=0||storage.saveHandle>0xffffffff))boundary('Dialogue callback requires its payload-save allocation outcome.','dialogue-payload-allocation');
    this.copy(r,RECORD,STRIDE);
    if(nativeDispatch&&slot===m.get(0x800c4c10,2))m.put(RECORD+2,m.get(RECORD+2,1)|4,1);
    if(event.service==='initialize') {
      if(flags&0x2000)boundary('Dialogue initialization cannot repeat for an initialized resource.');
      yield* m.run(0x80198be8,[slot],event.helpers);
      m.put(RECORD,m.get(RECORD,2)|0x2000,2);
    } else if(event.service==='callback') {
      if(!(flags&0x2000)||!owner.payload)boundary('Dialogue callback requires initialized saved payload.');
      var oldHandle=m.get(RECORD+0x24);
      if(!oldHandle||(!this.payloadStorage&&(storage.restoreHandle!==oldHandle||storage.restoreReturned!==true||storage.freeReturned!==true)))boundary('Dialogue callback requires matching payload restoration and release outcomes.','dialogue-payload-restore');
      var restoredLength=m.get(oldHandle+2,2);
      if(restoredLength!==0x478)boundary('Dialogue saved allocation has an unsupported payload length.','dialogue-payload-identity');
      if(this.payloadStorage)this.payloadStorage.restore(owner.ownerId,oldHandle,PAYLOAD);
      else this.copy(oldHandle+6,PAYLOAD,restoredLength);
      if([9,10].includes(m.get(PAYLOAD+0x3c,1)))boundary('Dialogue portrait continuation requires a qualified external producer.','dialogue-portrait-input');
      if(![0,1,2,3,4,5,6,7,8,11,99,100].includes(m.get(PAYLOAD+0x3c,1)))boundary('Dialogue state is outside the accepted continuation domain.','dialogue-state-input');
      if(!event.controller || !['actionMask','directionMask','dummyMask','historyMask','queueHead'].every(function(k){return Number.isInteger(event.controller[k]);}))boundary('Dialogue callback requires selected controller ownership and masks.','dialogue-controller-input');
      var c=event.controller;
      if(c.queueHead!==m.get(0x800c4c10,2))boundary('Dialogue controller owner differs from the current resource queue.','dialogue-controller-owner');
      m.put(0x800e8100,c.actionMask,2);m.put(0x800e8700,c.directionMask,2);m.put(0x800af0a6,c.dummyMask,2);
      if(nativeDispatch)m.put(0x800e8108,c.queueHead===slot?0x800e79b0:0x800af0a6);
      var textState=m.get(PAYLOAD+0x3c,1);
      if(textState===3||textState===6||(textState===7&&m.get(PAYLOAD+0x52,1)))m.put(m.get(0x800e8108),c.historyMask,2);
      m.put(0x800c4bdc,c.queueHead===slot?0x800e8100:0x800af0a6);m.put(0x800c4c4c,c.queueHead===slot?0x800e8700:0x800af0a6);
      yield* m.run(0x8019981c,[slot],event.helpers);
    } else boundary('Dialogue service kind is unsupported.','dialogue-service-input');
    if(nativeDispatch&&slot===m.get(0x800c4c10,2))m.put(RECORD+2,m.get(RECORD+2,1)&~4,1);
    var length=m.get(RECORD+0x20,2),saveFlags=m.get(RECORD+1,1);
    if(length!==0x478||(saveFlags&1))boundary('Dialogue requires a qualified payload storage mode.','dialogue-payload-mode');
    if(this.payloadStorage)storage={saveHandle:this.payloadStorage.save(owner.ownerId,saveFlags,PAYLOAD,length)};
    m.region(storage.saveHandle,length+6,true);
    if(this.owners.some(function(o,i){return i!==slot&&o&&m.get(POOL+i*STRIDE+0x24)===storage.saveHandle;}))boundary('Dialogue payload allocation belongs to another active resource.','dialogue-payload-owner');
    if(!this.payloadStorage){m.put(storage.saveHandle,saveFlags,1);m.put(storage.saveHandle+2,length,2);m.put(storage.saveHandle+4,length,2);this.copy(PAYLOAD,storage.saveHandle+6,length);}
    m.put(RECORD+0x24,storage.saveHandle);
    this.copy(RECORD,r,STRIDE);
    owner.payload=Uint8Array.from({length:0x478},function(_,i){return m.get(PAYLOAD+i,1);});
    if(m.get(r+2,1)&(nativeDispatch?2:4)) {
      yield* m.run(0x80077f88,[slot],event.releaseHelpers||[]);
      if(!(m.get(r,2)&0x8000)){if(this.payloadStorage)this.payloadStorage.release(owner.ownerId);this.owners[slot]=null;}
    } else rejectUnused(event.releaseHelpers);
    this.reconcile(event);
  };
  Engine.prototype.snapshot = function() {
    var snapshot={memory:this.machine.regions.filter(function(r){return r.writable;}).map(function(r){return {address:r.address,hex:hex(r.bytes)};}),
      owners:this.owners.map(function(o){return o?{ownerId:o.ownerId,payloadHex:o.payload?hex(o.payload):null}:null;})};
    if(this.payloadStorage)snapshot.payloadStorage={...this.payloadStorage.config};
    return snapshot;
  };
  Engine.prototype.presentation = function(slot, ownerId) {
    var owner=this.owners[slot],r=POOL+slot*STRIDE;
    if(!owner||owner.ownerId!==ownerId||(this.machine.get(r,2)&0xa000)!==0xa000)return null;
    if(!owner.payload)return {text:'',paused:false,state:null};
    var p=owner.payload,v=new DataView(p.buffer,p.byteOffset,p.byteLength),count=v.getUint16(0x34);
    if(count>0x300)boundary('Dialogue output exceeded the native buffer.','dialogue-output-bound');
    var lineOffset=0xb8+2*p[0x54];
    if(lineOffset+2>p.length)boundary('Dialogue history requires a reached line offset.','dialogue-reached-memory');
    var start=v.getUint16(lineOffset),visible=p.slice(0x178+start,0x178+count),nul=visible.indexOf(0);
    if(nul>=0)visible=visible.slice(0,nul);
    return {text:Array.from(visible,function(b){return String.fromCharCode(b);}).join(''),
      glyphAppearance:'approximate-byte-text',
      outputHex:hex(p.slice(0x178,0x178+count)),paused:!!(this.machine.get(r+0x8a,1)&1),state:p[0x3c],
      presentationGate:p[0x4f],displayedHistoryLine:p[0x54],generatedHistoryLine:p[0x55],
      rectangle:[6,8,10,12].map(function(o){return this.machine.get(r+o,2)<<16>>16;},this)};
  };
  OB64.cutsceneDialogue={Machine:Machine,Engine:Engine,PayloadStorage:PayloadStorage,validateHelper:validateHelper};
})(window.OB64);
