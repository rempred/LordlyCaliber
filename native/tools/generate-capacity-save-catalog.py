"""Generate guarded adaptations of the two resident mission save callbacks.

Reads Joe's canonical ROM. No patched ROM is produced. The output contains only
the specific operands to change plus whole-owner identity checks.
"""
from pathlib import Path
import hashlib
import json
import struct

NATIVE = Path(__file__).resolve().parents[1]
ROOT = NATIVE.parent.parent
rom = bytearray((ROOT / 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64').read_bytes())
if rom[:4] == bytes.fromhex('37804012'):
    rom[::2], rom[1::2] = rom[1::2], rom[::2]
assert hashlib.sha256(rom).hexdigest() == '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a'
word = lambda at: struct.unpack_from('>I', rom, at)[0]
owners = json.loads((ROOT / 'wiki/sol-decomp-static-db-r3-20260710/exports/canonical-functions.json').read_text())
result = []
for start, loop_start, loop_end, payload_register, tail_count in [
        (0x5b008, 0x5b064, 0x5b404, 5, 71),
        (0x5b864, 0x5b89c, 0x5bc0c, 3, 73)]:
    end = next(o['rom_end_exclusive'] for o in owners if o['rom_start'] == start)
    patches = []
    def add(at, kind, addend=0):
        assert all(p['site'] != at for p in patches), hex(at)
        patches.append(dict(site=at, expected=word(at), kind=kind, addend=addend))
    counts = {0x5b024: ('mapCount', 50), 0x5b04c: ('enemyCount', 20),
              0x5b3fc: ('mapCount', 50)} if start == 0x5b008 else {
              0x5b884: ('enemyCount', 20), 0x5bc04: ('mapCount', 50)}
    for at, (kind, old) in counts.items():
        assert word(at) & 65535 == old
        add(at, kind)
    memory = {32,33,35,36,37,40,41,43,49,53,57,61}
    for at in range(loop_start, loop_end + 4, 4):
        w = word(at)
        if w >> 26 in memory and w >> 21 & 31 == payload_register and 0x28 <= w & 65535 <= 0xcf:
            add(at, 'snapshotOffset', (w & 65535) - 40)
    assert sum(p['kind'] == 'snapshotOffset' for p in patches) == 54
    for at in range(loop_end + 4, end, 4):
        w = word(at)
        if w >> 26 in memory | {9} and 0x20f8 <= w & 65535 <= 0x23cc:
            aliases = {0x5b648: (9, 0x5b634, 0x00804821),
                       0x5b6a8: (9, 0x5b694, 0x00804821),
                       0x5be3c: (7, 0x5be34, 0x00803821),
                       0x5be44: (7, 0x5be34, 0x00803821),
                       0x5be9c: (6, 0x5be94, 0x00803021),
                       0x5bea4: (6, 0x5be94, 0x00803021),
                       0x5b728: (5, 0x5b71c, 0x00802821),
                       0x5b734: (3, 0x5b72c, 0x00881821),
                       0x5b744: (3, 0x5b72c, 0x00881821),
                       0x5b754: (3, 0x5b72c, 0x00881821),
                       0x5b7f8: (3, 0x5b7f4, 0x00881821),
                       0x5b808: (5, 0x5b7e8, 0x00802821),
                       0x5b810: (3, 0x5b7f4, 0x00881821),
                       0x5bf24: (5, 0x5bf18, 0x00802821),
                       0x5bf38: (3, 0x5bf34, 0x00881821),
                       0x5bf4c: (3, 0x5bf34, 0x00881821),
                       0x5bf60: (3, 0x5bf34, 0x00881821),
                       0x5bffc: (3, 0x5bff8, 0x00881821),
                       0x5c008: (5, 0x5bfe4, 0x00802821),
                       0x5c01c: (3, 0x5bff8, 0x00881821)}
            reg, init, expected = aliases.get(at, (4, at, w))
            assert w >> 21 & 31 == reg and word(init) == expected, (hex(at), hex(w))
            add(at, 'tailOffset', (w & 65535) - 0x20f8)
    assert sum(p['kind'] == 'tailOffset' for p in patches) == tail_count
    # Each relocated global reference is a short complete LUI + constant-low
    # pair, optionally with a dynamic record-stride ADDU in between. Require the
    # exact scratch register chain; all other constants remain original.
    references = []
    for at in range(start, loop_end + 4, 4):
        w = word(at)
        if w >> 26 != 15 or w & 65535 != 0x8019:
            continue
        reg = w >> 16 & 31
        low = at + 4
        if word(low) >> 26 == 0:
            middle = word(low)
            assert middle & 63 == 33 and middle >> 11 & 31 == reg and middle >> 21 & 31 == reg
            assert middle >> 16 & 31 == (3 if start == 0x5b008 else 5)
            low += 4
        ins = word(low)
        if ins >> 21 & 31 != reg:
            continue
        value = 0x80190000 + (ins & 65535)
        if value == 0x801936b8:
            group, offset = 'drops', 0
        elif 0x801910f0 <= value < 0x801911b0:
            group, offset = 'backups', value - 0x801910f0
        else:
            continue
        assert ins >> 26 in memory | {9}
        references.append(dict(high=at, low=low, highWord=w, lowWord=ins, group=group, addend=offset))
    assert len(references) == (59 if start == 0x5b008 else 55), len(references)
    result.append(dict(start=start, size=end-start, address=start+0x8012a100,
        retailSha256=hashlib.sha256(rom[start:end]).hexdigest(),
        patches=sorted(patches,key=lambda p:p['site']), references=references))
out = NATIVE / 'game/scenarios/capacity-save-owners.json'
out.write_text(json.dumps(dict(generator='native/tools/generate-capacity-save-catalog.py',
    inputRomSha256=hashlib.sha256(rom).hexdigest(), owners=result),indent=2)+'\n',newline='\n')
print(json.dumps([dict(start=hex(o['start']), patches=len(o['patches']), references=len(o['references'])) for o in result]))
