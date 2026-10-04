'use strict';
// Read independent, existing post-load captures and browser placement references.
// The private fixture is generated in the test directory, never shipped.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const name of ['scenario-eset-data.js','scenario-map-calibration.js'])
 vm.runInThisContext(fs.readFileSync(path.join(root,name),'utf8'),{filename:name});
const capture=JSON.parse(fs.readFileSync(path.join(process.argv[2],'wiki/bounds-postmap-recapture.json')));
const rows=capture.results.map(c=>({key:c.runtimeKey,status:c.status,bounds:c.liveBounds?.raw16,
 sites:OB64.SCENARIO_ESET_DATA.scenarios.find(s=>s.runtimeKey===c.runtimeKey)?.sites?.map(s=>({selector:s.selector,x:s.x,z:s.z}))||[],
 deployments:OB64.SCENARIO_MAP_CALIBRATION.scenarios.find(s=>s.runtimeKey===c.runtimeKey)?.points?.filter(p=>p.world&&p.placement).map(p=>({placement:p.placement,world:p.world}))||[]}));
fs.writeFileSync(process.argv[3],JSON.stringify({rows}));
console.log(`Independent references: ${rows.filter(r=>r.bounds).length} post-load rectangles; ${rows.reduce((n,r)=>n+r.sites.length,0)} sites.`);
