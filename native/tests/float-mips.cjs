'use strict';
// Test-only o32 single/double COP1 extension. Finite nearest-rounding paths;
// exceptions, subnormal flushing and configurable FCSR are not modeled.
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
class FloatMips extends Mips {
 constructor(){super();this.fp=new Uint32Array(32);this.condition=false;this.bits=new DataView(new ArrayBuffer(8));}
 single(i){this.bits.setUint32(0,this.fp[i]);return this.bits.getFloat32(0);}
 setSingle(i,v){this.bits.setFloat32(0,v);this.fp[i]=this.bits.getUint32(0);}
 double(i){this.bits.setUint32(0,this.fp[i+1]);this.bits.setUint32(4,this.fp[i]);return this.bits.getFloat64(0);}
 setDouble(i,v){this.bits.setFloat64(0,v);this.fp[i+1]=this.bits.getUint32(0);this.fp[i]=this.bits.getUint32(4);}
 readFloat(a){this.bits.setUint32(0,this.r32(a));return this.bits.getFloat32(0);}
 writeFloat(a,v){this.bits.setFloat32(0,v);this.w32(a,this.bits.getUint32(0));}
 exec(pc,next){
  const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31,fs=w>>>11&31,fd=w>>>6&31,fn=w&63;
  const address=(this.r[rs]+(w<<16>>16))>>>0,advance=()=>[next,(next+4)>>>0];
  if(op===0&&fn===17){this.hi=this.r[rs];return advance();}
  if(op===0&&fn===19){this.lo=this.r[rs];return advance();}
  // Big-endian unaligned word pairs used by compiled fixed-size struct copies.
  if([34,38,42,46].includes(op)){
   const left=op===34||op===42,load=op===34||op===38,n=address&3;
   const count=left?4-n:n+1,start=left?address:(address&~3)>>>0;
   let value=this.r[rt];
   for(let i=0;i<count;i++){
    const shift=left?24-i*8:(count-1-i)*8;
    if(load)value=((value&~(255<<shift))|(this.r8(start+i)<<shift))>>>0;
    else this.w8(start+i,value>>>shift);
   }
   if(load&&rt)this.r[rt]=value;return advance();
  }
  if(op===49){this.fp[rt]=this.r32(address);return advance();}
  if(op===57){this.w32(address,this.fp[rt]);return advance();}
  if(op===53){this.fp[rt+1]=this.r32(address);this.fp[rt]=this.r32(address+4);return advance();}
  if(op===61){this.w32(address,this.fp[rt+1]);this.w32(address+4,this.fp[rt]);return advance();}
  if(op!==17)return super.exec(pc,next);
  if(rs===0){if(rt)this.r[rt]=this.fp[fs];return advance();}
  if(rs===4){this.fp[fs]=this.r[rt];return advance();}
  if(rs===8){
   const take=this.condition===!!(rt&1),target=(next+((w<<16>>16)*4))>>>0;
   if((rt&2)&&!take)return[(next+4)>>>0,(next+8)>>>0];
   return[next,take?target:(next+4)>>>0];
  }
  if(![16,17,20].includes(rs))throw Error(`Unsupported COP1 format ${rs} at ${pc.toString(16)}`);
  const read=i=>rs===16?this.single(i):rs===17?this.double(i):this.fp[i]|0;
  const put=v=>rs===16?this.setSingle(fd,v):this.setDouble(fd,v),a=read(fs),b=read(rt);
  switch(fn){
   case 0:put(a+b);break;case 1:put(a-b);break;case 2:put(a*b);break;case 3:put(a/b);break;
   case 4:put(Math.sqrt(a));break;case 5:put(Math.abs(a));break;case 6:put(a);break;case 7:put(-a);break;
   case 13:if(!Number.isFinite(a)||a< -2147483648||a>=2147483648)throw Error('Unsupported FP conversion');this.fp[fd]=Math.trunc(a);break;
   case 32:this.setSingle(fd,a);break;case 33:this.setDouble(fd,a);break;
   case 50:this.condition=a===b;break;case 60:this.condition=a<b;break;case 62:this.condition=a<=b;break;
   default:throw Error(`Unsupported COP1 function ${fn} at ${pc.toString(16)}`);
  }
  return advance();
 }
}
module.exports={FloatMips};
