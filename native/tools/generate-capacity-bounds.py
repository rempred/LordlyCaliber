"""Guarded source/map count changes; values are semantic, never a global 50 edit.

Owner paths were traced through source records, pointer tables and snapshot
copies. In particular Chaos Frame50, battle actor20 and object flag49 remain.
"""
from pathlib import Path
import hashlib
import json
import struct

NATIVE = Path(__file__).resolve().parents[1]
ROOT = NATIVE.parent.parent
rom = bytearray((ROOT/'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64').read_bytes())
if rom[:4] == bytes.fromhex('37804012'):
    rom[::2], rom[1::2] = rom[1::2], rom[::2]
digest = hashlib.sha256(rom).hexdigest()
assert digest == '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a'
word = lambda at: struct.unpack_from('>I',rom,at)[0]
canonical = json.loads((ROOT/'wiki/sol-decomp-static-db-r3-20260710/exports/canonical-functions.json').read_text())
rows = {}

def add(sites, expected, value, reason):
    for site in sites:
        assert word(site) == expected, (hex(site),hex(word(site)))
        owner = next(o for o in canonical if o['rom_start']<=site<o['rom_end_exclusive'])
        start,end = owner['rom_start'],owner['rom_end_exclusive']
        row = rows.setdefault(start,dict(start=start,size=end-start,
            retailSha256=hashlib.sha256(rom[start:end]).hexdigest(),patches=[]))
        assert not any(p['site']==site for p in row['patches'])
        row['patches'].append(dict(site=site,expected=expected,value=value,reason=reason))

add([0x12e950],0x28830032,0x2883003d,'source ID validator')
add([0x135ef4],0x28820032,0x2882003d,'source pointer selector')
add([0x118d44],0x24120032,0x2412003d,'source reference clearing')
add([0x101eac,0x13107c,0x131350],0x2a220032,0x2a22003d,'active source and object refresh loops')
add([0x1206d8],0x2a420032,0x2a42003d,'placement/path object scan')
add([0x109aa4],0x28820032,0x2882003d,'enemy object scan')
add([0x10adf0,0x10ae3c],0x2c620032,0x2c62003d,'batched object scheduler end')
add([0x10ae58],0x2c420033,0x2c42003f,'batched object scheduler wrap, next multiple of three after source count')
add([0x1299d4,0x129b30],0x24c82580,0x24c82dc0,'61 snapshots,192 bytes each')
add([0x1299a0,0x129af4,0x129c50,0x129db4,0x129f50,0x129fec,0x12a01c],
    0x2a020032,0x2a02003d,'map state initialize/copy/restore loops')
add([0x10985c,0x1099dc,0x109c28],0x28a20032,0x28a2003d,'enemy object flag scans')
add([0x10a5dc,0x10a5e4,0x10a5f0],0x28820032,0x2882003d,'target object search and exhaustion')
add([0x10e724],0x2a420032,0x2a42003d,'source object flag loop')
add([0x116bbc],0x2a220032,0x2a22003d,'source object display list loop')
add([0x117c6c],0x28a20032,0x28a2003d,'enemy proximity search')
add([0x117f58],0x28820032,0x2882003d,'enemy proximity search')
add([0x1184c0],0x2a420032,0x2a42003d,'source object loop')
add([0x117c64,0x117c78,0x117f50,0x117f64],0x24020032,0x2402003d,'exhausted proximity search sentinel')
add([0x12a4d8],0x2a220032,0x2a22003d,'clear source flag4')
add([0x108d88],0x240400cc,0x240400f8,'heap candidate list:61 IDs plus -1 terminator')
add([0x108ec8],0x2a220032,0x2a22003d,'candidate list enemy producer loop')
add([0x41e14],0x2a020064,0x2a020100,'ordinary enemy name scan; player bound unchanged')
add([0x130574],0x2bc20064,0x2bc20100,'ordinary enemy recovery scan')
add([0x133284],0x2a2200c8,0x2a220164,'combined100 player plus256 tagged enemy availability')
add([0x1333e8],0x284200c8,0x28420164,'combined availability loop end')
add([0x283828],0x28a20064,0x28a20100,'enemy class-record lookup; player lookup bound unchanged')
add([0x249e3c,0x249e50],0x24020032,0x2402003c,'scripted copy searches only30 authored map sources')
add([0x249e44],0x2c620032,0x2c62003c,'scripted copy source loop')
add([0x1d20b0],0x240504e2,0x240505f5,'new-game clear of61 source records')
add([0x12383c,0x123c3c],0x2c42000f,0x2c42001e,'route activation:30 active enemy/allied squads')
add([0x123898],0x2c62000f,0x2c62001e,'dormant squad activation:30 active enemy/allied squads')
add([0x1238ac],0x2410000f,0x2410001e,'available activation slots for the entire movement group')
for row in rows.values():row['patches'].sort(key=lambda p:p['site'])
out=NATIVE/'game/scenarios/capacity-bounds.json'
out.write_text(json.dumps(dict(generator='native/tools/generate-capacity-bounds.py',
    inputRomSha256=digest,owners=sorted(rows.values(),key=lambda o:o['start'])),indent=2)+'\n',newline='\n')
print(json.dumps(dict(owners=len(rows),patches=sum(len(r['patches']) for r in rows.values()))))
