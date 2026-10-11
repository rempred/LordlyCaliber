"""Generate independent fixtures using the original standalone Scenario Seeder.

Development check only; not used or shipped by the native editor. Inputs are a
user-owned ROM and the existing standalone seeder folder, supplied explicitly.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--standalone', type=Path, required=True)
parser.add_argument('--rom', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.output.exists():
    parser.error('Choose a fresh reference directory.')
sys.path.insert(0, str(args.standalone.resolve()))
from save_data import BatterySave, SaveCodec
from seed_builder import default_seed_army, with_army, scenario_choices, story_flags, make_world_seed
from story_routes import StoryRoutes

codec = SaveCodec(args.rom)
base = BatterySave((args.standalone/'story-start.sra').read_bytes())
army = BatterySave((args.standalone/'default-army.sra').read_bytes())
source = default_seed_army(codec, with_army(codec, base, 0, army, 0), 0)
routing = StoryRoutes(codec.rom)
entries = json.loads((args.standalone/'mission-labels.json').read_text())['keys']
order = {e['key']: e['wikiId'] for e in entries if isinstance(e.get('wikiId'), int)}
ram = codec.decode(source, 0)
choices = scenario_choices(routing, order, story_flags(ram), ram[0x801936a9], initial_ram=ram)
args.output.mkdir(parents=True)
records = []
for i, (route, cf) in enumerate(choices):
    seed, _ = make_world_seed(codec, source, 0, route.node, route, chaos_frame=cf)
    (args.output/f'seed-{i}.sra').write_bytes(seed.raw)
    records.append(dict(index=i, node=route.node, event=route.event_row, chaos=cf,
                        sha256=hashlib.sha256(seed.raw).hexdigest()))
(args.output/'reference.json').write_text(json.dumps(records, indent=2))
(args.output/'inputs.json').write_text(json.dumps(dict(
    generator='native/tools/seeder-reference.py',
    standalone=str(args.standalone.resolve()), rom=str(args.rom.resolve())), indent=2))
print(f'Created {len(records)} reference seeds without emulator I/O.')
