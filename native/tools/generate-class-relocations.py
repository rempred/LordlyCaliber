"""Generate checked MIPS address fixups; never embed user-ROM instruction bytes.

Inputs are the canonical user-owned Rev 0 ROM, accepted owner ledger and parent
reference analysis. Only short direct LUI/low pairs are emitted. Relative field
accesses are deliberately retained. This establishes the emitted fixups, not a
claim that every consumer of additional class IDs has been migrated.
"""
from pathlib import Path
import argparse
import hashlib
import json

parser = argparse.ArgumentParser()
parser.add_argument('--rom', required=True)
parser.add_argument('--ledger', required=True)
parser.add_argument('--references', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
rom = bytearray(Path(args.rom).read_bytes())
if rom[:4] == bytes.fromhex('37804012'):
    rom[::2], rom[1::2] = rom[1::2], rom[::2]
digest = hashlib.sha256(rom).hexdigest()
assert digest == '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a'
word = lambda offset: int.from_bytes(rom[offset:offset+4], 'big')
rows = json.loads(Path(args.ledger).read_text())['splatPrimaryRows']
by_name = {row['name']: row for row in rows}
owners = {}

def owner(row):
    start, end = row['romStart'], row['romEndExclusive']
    if start not in owners:
        owners[start] = dict(start=start, size=end-start,
                             retailSha256=hashlib.sha256(rom[start:end]).hexdigest(),
                             references=[], calls=[])
    return owners[start]

groups = [('classes', 'reference-candidates.json', 0x80187BCC, 0x8018AA7C),
          ('appearances', 'appearance-reference-candidates.json', 0x8018ECE0, 0x8018F0BE),
          ('directory', 'pool-reference-candidates.json', 0x800AEDB0, 0x800AEDE0)]
for group, filename, base, end in groups:
    analysis = json.loads((Path(args.references) / filename).read_text())
    assert analysis['romSha256'] == digest
    for ref in analysis['references']:
        low, address = int(ref['site'], 16), int(ref['base'], 16)
        if word(low) & 65535 != address & 65535:
            continue
        assert len(ref['lui']) == 1 and not ref['mixedBaseUses'], ref
        high = int(ref['lui'][0], 16)
        assert 0 < low-high <= 12 and word(high) >> 26 == 15, ref
        low_opcode = word(low) >> 26
        assert low_opcode in [8, 9, 13, 32, 33, 34, 35, 36, 37, 38, 39,
                              40, 41, 42, 43, 46, 49, 53, 57, 61], ref
        immediate = word(low) & 65535
        if low_opcode != 13 and immediate & 32768:
            immediate -= 65536
        assert (((word(high) & 65535) << 16) + immediate) & 0xffffffff == address
        assert base <= address < end
        row = by_name[ref['owner']]
        assert row['romStart'] <= high < low < row['romEndExclusive']
        # No relative branch may enter a pair after its high-half definition.
        # Calls/jumps before the low part are allowed only when it is their
        # delay instruction, which runs before control transfers.
        for pos in range(row['romStart'], row['romEndExclusive'], 4):
            opcode = word(pos) >> 26
            branch = opcode in [1, 4, 5, 6, 7, 20, 21, 22, 23] or (
                opcode == 17 and (word(pos) >> 21 & 31) == 8)
            if branch:
                displacement = word(pos) & 65535
                if displacement & 32768:
                    displacement -= 65536
                target = pos + 4 + displacement * 4
                assert not high < target <= low or high <= pos <= low, ref
            if high <= pos < low and (branch or opcode in [2, 3]):
                assert opcode in [2, 3] and pos == low-4, ref
        owner(row)['references'].append(dict(high=high-row['romStart'],
                                             low=low-row['romStart'],
                                             group=group, addend=address-base))

services = {0x80070C60: 'lc_heap_reset', 0x80070F14: 'lc_secondary_reset'}
for row in rows:
    if 'code_original_mips' not in row['name']:
        continue
    for pos in range(row['romStart'], row['romEndExclusive'], 4):
        instruction = word(pos)
        if instruction >> 26 not in [2, 3]:
            continue
        target = 0x80000000 | (instruction & 0x3ffffff) << 2
        if target in services:
            symbol = 'lc_boot_classes' if pos == 0x25A8 else services[target]
            owner(row)['calls'].append(dict(offset=pos-row['romStart'], symbol=symbol,
                                            originalTarget=target))

# Explicitly reviewed allocator capacity and instruction-cache preparation.
for pos, field in [(0x1148, 'directoryCapacity'), (0x2438, 'codeCacheEndHigh'), (0x243C, 'codeCacheEndLow')]:
    row = next(row for row in rows if row['romStart'] <= pos < row['romEndExclusive'])
    owner(row).setdefault('constants', []).append(dict(offset=pos-row['romStart'], field=field))
assert word(0x1148) == 0x2C420004 and word(0x2438) == 0x3C058018 and word(0x243C) == 0x24A56310

result = dict(generator='native/tools/generate-class-relocations.py',
              inputRomSha256=digest,
              originalBases=dict(classes=0x80187BCC, appearances=0x8018ECE0,
                                 directory=0x800AEDB0),
              owners=sorted(owners.values(), key=lambda item: item['start']))
output = root / 'native/game/classes/relocations.json'
output.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(owners=len(owners),
                     references=sum(len(item['references']) for item in owners.values()),
                     calls=sum(len(item['calls']) for item in owners.values()),
                     output=str(output))))
