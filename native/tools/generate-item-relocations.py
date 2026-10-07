"""Generate item-record address fixups from the canonical user ROM and accepted owners.
Only relocation metadata/hashes are shipped, never instruction or ROM payloads.
"""
from pathlib import Path
import argparse, collections, hashlib, json
parser = argparse.ArgumentParser()
parser.add_argument('--rom', required=True)
parser.add_argument('--owners', required=True)
args = parser.parse_args()
rom = bytearray(Path(args.rom).read_bytes())
if rom[:4] == bytes.fromhex('37804012'):
    rom[::2], rom[1::2] = rom[1::2], rom[::2]
digest = hashlib.sha256(rom).hexdigest()
assert digest == '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a'
rows = json.loads(Path(args.owners).read_text(encoding='utf-8'))['rows']
word = lambda at: int.from_bytes(rom[at:at+4], 'big')
refs, uses, owners = [], collections.defaultdict(set), {}
base, end = 0x8018c400, 0x8018e6cc
for row in rows:
    if 'code_original_mips' not in row['name']:
        continue
    tags = {}
    for at in range(row['romStart'], row['romEndExclusive'], 4):
        w = word(at)
        op, rs, rt, rd, fn, imm = w >> 26, w >> 21 & 31, w >> 16 & 31, w >> 11 & 31, w & 63, w & 65535
        signed = imm if imm < 32768 else imm - 65536
        if op == 15:
            tags[rt] = (at, imm << 16)
            continue
        if op in [8,9,13,32,33,34,35,36,37,38,39,40,41,42,43,46,49,53,57,61] and rs in tags:
            hi, upper = tags[rs]
            address = (upper + (imm if op == 13 else signed)) & 0xffffffff
            uses[hi].add(address)
            if base <= address < end:
                refs.append((row, hi, at, address))
        if op == 0 and fn in [32,33,37]:
            values = [tags[r] for r in (rs, rt) if r in tags]
            if len(values) == 1: tags[rd] = values[0]
            else: tags.pop(rd, None)
        elif op == 0: tags.pop(rd, None)
        elif op in [8,9,10,11,12,13,14,32,33,34,35,36,37,38,39]: tags.pop(rt, None)
for row, hi, lo, address in refs:
    start, stop = row['romStart'], row['romEndExclusive']
    assert 0 < lo - hi <= 12 and all(base <= v < end for v in uses[hi])
    for at in range(start, stop, 4):
        w = word(at); op = w >> 26
        branch = op in [1,4,5,6,7,20,21,22,23] or (op == 17 and (w >> 21 & 31) == 8)
        if branch:
            offset = w & 65535
            if offset & 32768: offset -= 65536
            target = at + 4 + offset * 4
            assert not hi < target <= lo or hi <= at <= lo
        if hi <= at < lo and (branch or op in [2,3]):
            assert op in [2,3] and at == lo - 4
    if start not in owners:
        owners[start] = dict(start=start, size=stop-start,
            retailSha256=hashlib.sha256(rom[start:stop]).hexdigest(), references=[], calls=[])
    owners[start]['references'].append(dict(group='items', high=hi-start, low=lo-start, addend=address-base))
assert len(refs) == 62 and len(owners) == 48
output = Path(__file__).resolve().parents[1] / 'game/items/relocations.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes((json.dumps(dict(generator='native/tools/generate-item-relocations.py',
    inputRomSha256=digest, ownerSource='config/splat/us_rev0.semantic.json', originalBases=dict(items=base),
    owners=sorted(owners.values(), key=lambda row: row['start'])), indent=2)+'\n').encode('utf-8'))
print(f'{len(refs)} references in {len(owners)} owners: {output}')

# Both description resource consumers share their resident owners with class help.
text_owners = [dict(start=0x55b28, size=0x98c, high=0x898, low=0x8a0),
               dict(start=0x66d4c, size=84, word=0)]
assert word(0x563c0) == 0x3c0501de and word(0x563c8) == 0x34a50090
assert word(0x66d4c) == 0x01de0090
for owner in text_owners:
    owner['retailSha256'] = hashlib.sha256(rom[owner['start']:owner['start']+owner['size']]).hexdigest()
output.with_name('text-owners.json').write_bytes((json.dumps(dict(
    generator='native/tools/generate-item-relocations.py', inputRomSha256=digest,
    owners=text_owners), indent=2)+'\n').encode('utf-8'))
