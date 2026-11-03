'use strict';
const assert=require('node:assert/strict'),test=require('node:test'),fs=require('node:fs'),path=require('node:path');
const {claims,overlaps,check,fixedStorage,checkStorage}=require('../runtime-ownership.cjs');
const policies=JSON.parse(fs.readFileSync(path.join(__dirname,'../runtime-sharing.json'),'utf8'));
test('catalogued independent exporters have explicit sharing contracts',()=>check(claims(),policies));
test('native fixed ROM storage and extension modules cannot overlap',()=>{
 const storage=fixedStorage();checkStorage(storage);
 assert(storage.rom.length>=31&&storage.ram.length>=6,'all current fixed storage families collected');
 const occupied=storage.rom[0];
 assert.throws(()=>checkStorage({rom:[...storage.rom,{...occupied,group:'conflicting addition'}]}),/collision/);
 assert.throws(()=>checkStorage({rom:[{group:'too large',name:'test',start:0x27ff000,end:0x2800010}]}),/reserved area/);
});
test('overlap gate catches writes, containing replacements, growth, and new feature pairs',()=>{
 const rows=[{group:'one',name:'hook',start:16,end:24},{group:'two',name:'whole owner',start:8,end:32}];
 const approved={'one + two':{reason:'tested composition',ranges:[['10','18']]}};
 assert.equal(check(rows,approved).length,1);
 assert.throws(()=>check(rows,{}),/Unreviewed runtime sharing/);
 assert.throws(()=>check([{...rows[0],end:28},rows[1]],approved),/Unreviewed/);
 assert.throws(()=>check([rows[0],{...rows[1],group:'three'}],approved),/Unreviewed/);
 assert.equal(overlaps([{...rows[0],end:24},{...rows[1],start:24}]).length,0,'adjacent owners are separate');
});
