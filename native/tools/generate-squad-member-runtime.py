"""Generate the compiled-owner catalog using the editor's pinned MIPS compiler."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--tools', type=Path, default=root / '.local/n64-toolchain')
args = parser.parse_args()
build = root / 'build/squad-member-owner'
build.mkdir(parents=True, exist_ok=True)
symbols = {
    'lc_player_records': 0x80193bc0, 'lc_enemy_records': 0x80195560,
    'lc_classes': 0x80187c14, 'lc_sources': 0x801971f0,
    'lc_scenario_level': 0x801f1035, 'lc_special_name': 0x802172b0,
    'lc_initialize': 0x8016f11c, 'lc_equipment': 0x80214f80,
    'lc_finish_character': 0x8016bd20, 'lc_special_scenario': 0x801dc054,
    'lc_clear': 0x80093380, 'lc_copy_name': 0x8009c550,
}
def run(tool, *argv):
    subprocess.run([str(args.tools / tool), *map(str, argv)], cwd=build, check=True, capture_output=True)
pins = json.loads((root / 'rebuild/toolchain.json').read_text())['tools']
for name, expected in pins.items():
    assert hashlib.sha256((args.tools / name).read_bytes()).hexdigest() == expected
source = root / 'game/scenarios/squad-member.c'
run('cpp.exe', '-P', '-undef', '-nostdinc', '-D__mips=3', '-D__mips__', '-D_MIPSEB', '-D__MIPSEB__', source, 'member.i')
run('cc1.exe', '-quiet', '-O2', '-meb', '-mips3', '-mgp32', '-mfp32', '-G', '0', '-fno-PIC', '-mno-abicalls', '-fno-builtin', '-funsigned-char', 'member.i', '-o', 'member.s')
run('mips-kmc-elf-as.exe', '-G', '0', '-mips3', '-mabi=32', '-force-n64align', '-EB', '-o', 'member.o', 'member.s')
address, size = 0x802150bc, 644
(build / 'member.ld').write_text(''.join(f'{name} = 0x{value:x};\n' for name, value in symbols.items()) + f'SECTIONS {{ .text 0x{address:x} : {{ *(.text) }} }}\n')
run('mips-kmc-elf-ld.exe', '-EB', '-m', 'elf32bmip', '-T', 'member.ld', '-o', 'member.elf', 'member.o')
run('mips-kmc-elf-objcopy.exe', '-O', 'binary', *['--remove-section=' + s for s in ('.data','.bss','.rdata','.reginfo','.pdr','.mdebug','.comment','.lit4','.lit8')], 'member.elf', 'member.bin')
code = (build / 'member.bin').read_bytes()
assert len(code) <= size, f'{len(code)} bytes exceed the {size}-byte owner'
code = code.ljust(size, b'\0')
obj = (build / 'member.o').read_bytes()
u32 = lambda b, at: struct.unpack_from('>I', b, at)[0]
u16 = lambda b, at: struct.unpack_from('>H', b, at)[0]
sections = [obj[u32(obj,32)+40*i:u32(obj,32)+40*(i+1)] for i in range(u16(obj,48))]
contents = lambda s: obj[u32(s,16):u32(s,16)+u32(s,20)]
names = contents(sections[u16(obj,50)])
string = lambda table, at: table[at:table.index(b'\0',at)].decode()
section = {string(names,u32(s,0)): s for s in sections}
sym = contents(section['.symtab']); strings = contents(sections[u32(section['.symtab'],24)])
pending, references = [], []
linked = (build / 'member.elf').read_bytes()
linked_sections = [linked[u32(linked,32)+40*i:u32(linked,32)+40*(i+1)] for i in range(u16(linked,48))]
padding = None
for header in linked_sections:
    if u32(header,4) != 2: continue
    label_header = linked_sections[u32(header,24)]
    labels = linked[u32(label_header,16):u32(label_header,16)+u32(label_header,20)]
    for at in range(u32(header,16),u32(header,16)+u32(header,20),16):
        if string(labels,u32(linked,at)) == 'lc_scenario_member': padding = u32(linked,at+4)-address
assert padding is not None and padding <= 12 and code[:padding] == bytes(padding)
rel = contents(section['.rel.text'])
for at in range(0,len(rel),8):
    site, info = u32(rel,at)+padding, u32(rel,at+4)
    name = string(strings,u32(sym,(info>>8)*16))
    if name != 'lc_classes': continue
    if info & 255 == 5: pending.append(site)
    elif info & 255 == 6:
        for high in pending:
            value = ((u32(code,high)&65535)<<16) + struct.unpack('>h',code[site+2:site+4])[0]
            references.append({'high':high,'low':site,'group':'classes','addend':value-0x80187bcc})
        pending.clear()
assert references and not pending
catalog = {'generator':'native/tools/generate-squad-member-runtime.py',
           'toolchain':'rebuild/toolchain.json',
           'acceptedSource':'game/accepted/library/src/lib/func_0019554C.c',
           'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),
           'start':0x19554c,'size':size,'address':address,
           'cSourceSha256':hashlib.sha256(code).hexdigest(),'cReferences':references,'symbols':symbols}
(root/'game/scenarios/squad-member.json').write_text(json.dumps(catalog,indent=2)+'\n', encoding='utf-8', newline='\n')
(build/'member-padded.bin').write_bytes(code)
print(f'Compiled squad member owner: {len((build/"member.bin").read_bytes())}/{size} bytes; {len(references)} class reference(s).')
