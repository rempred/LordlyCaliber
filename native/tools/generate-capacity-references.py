"""Package qualified parent references with exact original instruction guards."""
from pathlib import Path
import argparse, hashlib, json

p = argparse.ArgumentParser()
p.add_argument('--report', type=Path, required=True)
p.add_argument('--boundaries', type=Path, required=True)
p.add_argument('--rom', type=Path, required=True)
a = p.parse_args()
rom = bytearray(a.rom.read_bytes())
if rom[:4] == bytes.fromhex('37804012'):
    rom[::2], rom[1::2] = rom[1::2], rom[::2]
r = json.loads(a.report.read_text())
assert hashlib.sha256(rom).hexdigest() == r['inputRomSha256']
assert not r['summary']['problems']
# Source-indexed halfword drop access uses a base 60 bytes before slot30.
# The same numerical address is also a separate five-byte item list: those
# byte accesses at46320/464A8/493A4 MUST remain at their original addresses.
aliases = {0x102434, 0x1028bc, 0x106c9c, 0x196e34}
for owner in json.loads(a.boundaries.read_text())['owners']:
    refs = []
    for ref in owner['references']:
        if any(u['low'] in aliases for u in ref['uses']):
            assert all(u['low'] in aliases for u in ref['uses'])
            refs.append(ref)
    if refs:
        existing = next((v for v in r['owners'] if v['start'] == owner['start']), None)
        if existing is None:
            existing = {**owner, 'references': []}
            r['owners'].append(existing)
        existing['references'].extend(refs)
for owner in r['owners']:
    assert hashlib.sha256(rom[owner['start']:owner['start']+owner['size']]).hexdigest() == owner['sha256']
    for ref in owner['references']:
        assert not ref['problems'] and not ref['otherUses']
        ref['highWord'] = int.from_bytes(rom[ref['high']:ref['high']+4], 'big')
        for use in ref['uses']:
            use['lowWord'] = int.from_bytes(rom[use['low']:use['low']+4], 'big')
r['generator'] = 'tools/enemy_capacity_references.py; native/tools/generate-capacity-references.py'
r['scope'] = 'Qualified instruction references. Final linker verifies instruction guards and shared high halves. Full rewritten owners use maintained C bindings instead.'
r['summary'] = dict(owners=len(r['owners']), pairs=sum(len(o['references']) for o in r['owners']))
target = Path(__file__).resolve().parents[1] / 'game/scenarios/capacity-references.json'
target.write_text(json.dumps(r, indent=2)+'\n', encoding='utf-8', newline='\n')
print(r['summary'])
