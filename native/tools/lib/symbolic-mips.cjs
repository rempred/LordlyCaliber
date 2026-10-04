'use strict';
// Convert authenticated instruction annotations to position-preserving source.
const registers=['zero','at','v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','s0','s1','s2','s3','s4','s5','s6','s7','t8','t9','k0','k1','gp','sp','s8','ra'];
const label=at=>'L_'+at.toString(16);
function rows(reference,rom,start,end){
  const result=[...reference.matchAll(/\/\* 0x([0-9A-F]{8}) 0x[0-9A-F]{8} 0x([0-9A-F]{8}) \*\/ \.word 0x[0-9A-F]{8} # (.+)/g)]
    .map(m=>({at:parseInt(m[1],16),word:parseInt(m[2],16),text:m[3].trim()}));
  if(result.length*4!==end-start||result.some((r,i)=>r.at!==start+i*4||rom.readUInt32BE(r.at)!==r.word))throw Error('Reference source mismatch');
  return result;
}
function instruction(row,{start,end,ram,parameters=new Map(),calls={}}){
  const {at,word}=row,op=word>>>26;let text=row.text;
  if(op===1||(op>=4&&op<=7)||(op>=20&&op<=23)||(op===17&&((word>>>21)&31)===8)){
    const target=at+4+((word<<16)>>16)*4;
    if(target<start||target>=end)throw Error('External conditional branch');
    text=text.replace(/0x[0-9A-Fa-f]+$/,label(target));
  }else if(op===2||op===3){
    const target=(0x80000000|((word&0x3ffffff)<<2))>>>0;
    const local=target>=ram&&target<ram+end-start;
    const name=local?label(start+target-ram):'service_'+target.toString(16);
    if(!local)calls[name]=target;
    text=(op===2?'j ':'jal ')+name;
  }else if(op===0&&(word&63)===13){
    if((word>>>6)&1023)throw Error('Unsupported break subcode');
    text=`break ${(word>>>16)&1023}`;
  }else if(op===0&&((word&63)===26||(word&63)===27)){
    text=text.replace(/^(divu?) /,'$1 $zero, ');
  }else if(text.startsWith('move ')){
    const reg=n=>'$'+registers[n];
    if(![33,37].includes(word&63))throw Error('Unknown move encoding');
    text=`${(word&63)===33?'addu':'or'} ${reg((word>>>11)&31)}, ${reg((word>>>21)&31)}, ${reg((word>>>16)&31)}`;
  }
  if(parameters.has(at))text=text.replace(/[^,]+$/, ' '+parameters.get(at));
  return text;
}
module.exports={rows,instruction,label};
