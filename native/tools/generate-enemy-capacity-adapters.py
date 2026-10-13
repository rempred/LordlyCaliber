"""Generate narrowly adapted C from byte-qualified accepted source owners.

The bindings input is produced by parent tools/enemy_capacity_bindings.py. All
source hashes and edit counts are checked. Generated files are maintained here;
no upstream Decomp checkout or compiled object is part of the product inputs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re

NATIVE = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--bindings', type=Path, required=True)
args = parser.parse_args()

# Exact source edits. Shared routines retain the player's retail type test.
EDITS = {
    'squad_supply_end': [('member >= 100', 'lc_enemy_is_soldier(member)', 1),
                         ('member < 100', '!lc_enemy_is_soldier(member)', 1)],
    'func_00128E80': [
        ('func_80093380(D_801F0BD8, 200);', 'func_80093380(D_801F0BD8, 100 + LC_ENEMY_HANDLES);', 1),
        ('for (i = 1; i < 100; i++) {\n        if (g_func_0019554C_records_52[i].field12 == 0)',
         'for (i = 1; i < LC_ENEMY_HANDLES; i++) {\n        if (lc_enemy_types[i] != LC_MEMBER_ORDINARY)', 1),
        ('k < 50', 'k < LC_TOTAL_SOURCES', 1),
        ('if (value != 0 && (value = g_func_001957D0_source_records[k].members[j]) < 100)\n                        (*(u8 *)',
         'if (value != 0 && !lc_enemy_is_soldier(value))\n                        (*(u8 *)', 1)],
    'func_0012A828': [('''                if (member >= 100) {
                    D_8019532C[member] = 0;
                    D_80195480[member] = 0;
                } else {
                    func_80093380(&D_80195560[member], 52);
                }''', '                lc_enemy_release(member);', 1)],
    'func_0012A050': [('''    index = 1;
    /* An integer base preserves the shared address before both byte offsets.
       Pointer CSE instead folds the base into a cursor and drops a retail instruction. */
    address = (u32)D_80195560;
    flag = (u8 *)(address + 0x67);
    record = (u8 *)(address + 0x34);
    for (; index < 99; index++) {
        memset_00023780(record, 0x34);
        record += 0x34;
        /* Keep the flag publication after the clear call, as in the ROM. */
        *flag &= 0x7F;
        flag += 0x34;
    }

    for (index = 119; index >= 0; index--) {
        D_801953F4[index] = 0;
    }''', '    lc_enemy_clear_members();', 1),
        ('index < 50', 'index < LC_TOTAL_SOURCES', 1)],
    'func_001960A8': [
        ('((((*(u8 *)((s8 *)(cursor0) + (0))) * 4) + 0x801F0000)) + (0xCB0)',
         '((((*(u8 *)((s8 *)(cursor0) + (0))) * 4) + (u32)D_801F0CB0))', 1),
        ('(u8 *)0x80187C42', 'D_80187C42', 1),
        ('(u8 *)0x80187C44', 'D_80187C44', 1),
        ('(u8 *)0x80187C46', 'D_80187C46', 1),
        ('(u8 *)0x80187C48', 'D_80187C48', 1),
        ('memset_00023780(((u32)D_80195560), 0x1450);', 'lc_enemy_reset();', 1),
        ('memset_00023780(((u32)D_801971F0 + 0x2EE), 0x1F4);',
         'memset_00023780(((u32)D_801971F0 + 0x2EE), LC_ENEMY_SAVE_SOURCES * 25);', 1),
        ('memset_00023780(((u32)D_801F2700), 0xF00);',
         'memset_00023780(((u32)D_801F2700), LC_ENEMY_SAVE_SOURCES * 192);', 1),
        ('memset_00023780(((u32)D_801F0D28), 0x50);',
         'memset_00023780(((u32)D_801F0D28), LC_ENEMY_SAVE_SOURCES * 4);', 1),
        ('index < 0x32', 'index < LC_TOTAL_SOURCES', 1)],
    'func_00129068': [('value >= 100', 'lc_member_is_soldier(record[0] - 1, value)', 1)],
    'func_0012967C': [('value >= 100', 'lc_member_is_soldier(index, value)', 1)],
    'func_0012A958': [('value >= 100', 'lc_member_is_soldier(FIELD(unit, u8, 4), value)', 1)],
    'func_0012E7E8': [('member >= 100', 'lc_member_is_soldier(unit->source_index, member)', 1)],
    'func_0012DA10': [('member >= 100', 'lc_member_is_soldier(source_index, member)', 1)],
    'func_0012F530': [('member >= 100', 'lc_member_is_soldier(source_index, member)', 2)],
    'func_001305B4': [('value >= 100', 'lc_member_is_soldier(id, value)', 2)],
    'func_0012ECDC': [('member >= 100', 'lc_member_is_soldier(source_index, member)', 3)],
    'func_0012F010': [('member >= 100', 'lc_member_is_soldier(source_index, member)', 1)],
    'func_0012F8C0': [('member_id >= 100', 'lc_member_is_soldier(unit->source_index, member_id)', 1)],
    'func_0014F300': [('member < 100', '!lc_member_is_soldier(source_index, member)', 1)],
    'func_00123E38': [('member < 100', '!lc_enemy_is_soldier(member)', 2),
                     ('*(u16 *)(row + member * 2 + 0x532C)', 'lc_enemy_soldier_hp[member]', 1),
                     ('row[member + 0x5480]', 'lc_enemy_soldier_state[member]', 2)],
    'func_00124018': [('member < 100', '!lc_enemy_is_soldier(member)', 2),
                     ('D_8019532C[99]', 'lc_enemy_soldier_max_hp', 1)],
    'func_00124328': [('member < 100', '!lc_enemy_is_soldier(member)', 2)],
    'func_00123458': [('id >= 100', 'lc_enemy_is_soldier(id)', 1)],
    'func_0021B0A0': [('temp_a0_64 < 0x64U', '!lc_enemy_is_soldier(temp_a0_64)', 1),
                     ('((temp_a0_64 * 0x34) + 0x80190000) + 0x5578',
                      '(lc_enemy_characters[temp_a0_64] + 0x18)', 1)],
    'func_0021C3B0': [('actor_id < 100', '!lc_member_is_soldier(side, actor_id)', 1),
                     ('} else {\n                flag = table_base;',
                      '''} else if (side >= 30) {
                lc_enemy_soldier_hp[actor_id] = *(u16 *)(actor + 0x20);
                if (func_0020C2C0(actor) == 0 && func_0020C32C(actor) != 0)
                    lc_enemy_soldier_state[actor_id] |= 4;
                else
                    lc_enemy_soldier_state[actor_id] &= 0xFB;
            } else {
                flag = table_base;''', 1)],
    'func_002ACA3C': [('preset[member + 2] < 100U', '!lc_enemy_is_soldier(preset[member + 2])', 1)],
}
# These owners express enemy record pointers through a shared numeric high half.
# Replace only their enemy row expression and its verified enemy field offsets;
# player and other shared-global arithmetic stays intact.
ROWS = {'func_0012A958': ('value', 1), 'func_0014F300': ('member', 1),
        'func_00123E38': ('member', 2), 'func_00124018': ('member', 4),
        'func_00124328': ('member', 2), 'squad_supply_end': ('member', 4)}
FIELD_COUNTS = {
    'func_0012A958': {'0x5578': 1},
    'func_0014F300': {'0x5571': 1, '0x5572': 1},
    'func_00123E38': {'0x5578': 1, '0x5593': 2},
    'func_00124018': {'0x5578': 10, '0x5576': 4},
    'func_00124328': {'0x5578': 2, '0x5592': 4},
    'squad_supply_end': {'0x5578': 3, '0x5576': 2, '0x5592': 2, '0x5593': 1},
}

def replace(text, old, new, count, name):
    assert text.count(old) == count, (name, old, text.count(old), count)
    return text.replace(old, new)

output = NATIVE / 'game/scenarios/capacity'
output.mkdir(exist_ok=True)
catalog = []
for name, edits in EDITS.items():
    metadata = json.loads((args.bindings / name / 'bindings.json').read_text())
    source = NATIVE / metadata['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == metadata['sourceSha256']
    text = source.read_text()
    for old, new, count in edits:
        text = replace(text, old, new, count, name)
    if name in ROWS:
        variable, count = ROWS[name]
        text = replace(text, f'(u8 *)0x80190000 + {variable} * 52',
                       f'lc_enemy_characters[{variable}]', count, name)
        for old, count in FIELD_COUNTS[name].items():
            text = replace(text, old, f'0x{int(old, 16) - 0x5560:X}', count, name)
    text = (f'/* Generated by native/tools/generate-enemy-capacity-adapters.py from\n'
            f' * byte-qualified {metadata["source"].replace(chr(92), "/")}.\n'
            f' * Enemy tagged handles; player behavior and other fields preserved.\n */\n'
            '#include "enemy-capacity.h"\n'
            'extern unsigned short lc_enemy_soldier_max_hp;\n' + text)
    target = output / (name + '.c')
    target.write_text(text.rstrip() + '\n', encoding='utf-8', newline='\n')
    metadata['adaptedSource'] = target.relative_to(NATIVE).as_posix()
    metadata['adaptedSha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
    for entry in metadata.get('entryPoints', []):
        names = set(re.findall(r'\b' + re.escape(entry['name']) + r'\b', text, re.IGNORECASE))
        assert len(names) == 1, (name, entry, names)
        entry['name'] = names.pop()
    catalog.append(metadata)
(output / 'owners.json').write_text(json.dumps(dict(generator='native/tools/generate-enemy-capacity-adapters.py',
                                                  owners=catalog), indent=2) + '\n', encoding='utf-8', newline='\n')
print(f'Generated {len(catalog)} qualified capacity adapters.')
