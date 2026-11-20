'use strict';
// Regenerate physical ROM metadata from the same upstream commit as the accepted C library.
const fs = require('node:fs');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const native = path.resolve(__dirname, '..');
const checkout = process.argv[2];
if (!checkout) throw Error('Usage: node generate-agent-rom-index.cjs <decomp-checkout>');
const {revision} = JSON.parse(fs.readFileSync(path.join(native, 'game/accepted/rom-owners.json')));
function upstream(file) {
    return JSON.parse(execFileSync('git', ['-C', checkout, 'show', revision + ':' + file], {maxBuffer:64*1024*1024}));
}
const sources = {
    ranges: 'config/splat/us_rev0.semantic.json',
    assembly: 'asm/original/rev0/manifest.json',
    build: 'config/phase7/conventional-build.json',
    overlays: 'config/splat/us_rev0.overlay-linker-inputs.json'
};
const {rows} = upstream(sources.ranges);
const {rom, expected, nonDescriptorLoadSlabs} = upstream(sources.build);
const sections = new Map();
for (const chunk of upstream(sources.assembly).chunks)
    for (const part of chunk.parts)
        sections.set(Number(part.romStart), part);
const overlays=upstream(sources.overlays).overlayReservations;
const placements = [{name:'early boot',start:rom.codeRegionStart,end:rom.earlyBootLinearEndExclusive,address:rom.earlyBootLinearBase+rom.codeRegionStart},
    ...overlays.map(d=>({name:'descriptor '+d.descriptor_id,start:d.rom_start,end:d.rom_end_exclusive,address:d.vram_start})),
    ...nonDescriptorLoadSlabs.map(s=>({name:s.id,start:s.romStart,end:s.romEndExclusive,address:s.vramStart}))];
let cursor=0,assemblyCount=0;
const entries=rows.map(row=>{
    if (row.romStart!==cursor || row.bytes!==row.romEndExclusive-cursor || row.bytes<=0)
        throw Error('Noncontiguous source map at '+cursor);
    cursor=row.romEndExclusive;
    const part=sections.get(row.romStart);
    const value={name:part?.name??row.name, offset:row.romStart,size:row.bytes,kind:row.primaryClass};
    if(row.ambiguous) value.ambiguous=true;
    if(row.processorClass!=='cpu-or-non-cpu') value.processor=row.processorClass;
    if(part) {
        if(Number(part.romEndExclusive)!==cursor) throw Error('Assembly allocation differs: '+part.file);
        value.assembly=part.file; ++assemblyCount;
    }
    return value;
});
if(cursor!==rom.bytes || assemblyCount!==expected.assemblyOwners || entries.length!==expected.primaryRows)
    throw Error('Incomplete native ROM source map');
const out=path.join(native,'game/accepted/rom-index.json');
const header={generator:'native/tools/generate-agent-rom-index.cjs',revision,profile:'us-rev0',sources,placements};
fs.writeFileSync(out,JSON.stringify(header,null,2).slice(0,-1)+',\n  "regions": [\n'+entries.map(v=>'    '+JSON.stringify(v)).join(',\n')+'\n  ]\n}\n');
console.log(entries.length+' ROM regions, '+assemblyCount+' assembly ranges, '+cursor+' bytes');
