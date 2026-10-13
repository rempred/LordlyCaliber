"""Guarded capacity storage/lifecycle adapters for mixed retail layouts.

Generated sources use explicit linker symbols. All ROM addresses are converted
through primary descriptors or witnessed secondary DMA mappings, never a guessed
early-boot delta. Unknown direct interior entries are rejected.
"""
from pathlib import Path
import hashlib,json,struct
NATIVE=Path(__file__).resolve().parents[1];ROOT=NATIVE.parent.parent
rom=bytearray((ROOT/'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64').read_bytes())
if rom[:4]==bytes.fromhex('37804012'):rom[::2],rom[1::2]=rom[1::2],rom[::2]
digest=hashlib.sha256(rom).hexdigest();assert digest=='571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a'
word=lambda at:struct.unpack_from('>I',rom,at)[0]
signed=lambda n:n-65536 if n&32768 else n
owners=json.loads((ROOT/'wiki/sol-decomp-static-db-r3-20260710/exports/canonical-functions.json').read_text())
maps=[dict(start=word(a+8),end=word(a+12),address=word(a)) for a in range(0x387c0,0x38ab8,40)]
maps+=json.loads((ROOT/'tmp/enemy-capacity-20261012/overlay-calls.json').read_text())['calls']
maps += [dict(start=0x1000,end=0x2f000,address=0x80070c00),dict(start=0x40e80,end=0x66e10,address=0x8016af80)]
rows=[]
def add(site,end,body,targets={},reason=''):
    owner=next(o for o in owners if o['rom_start']<=site<o['rom_end_exclusive'])
    first,last=owner['rom_start'],owner['rom_end_exclusive'];assert end<=last
    addresses={m['address']+first-m['start'] for m in maps if m['start']<=first<last<=m['end']};assert len(addresses)==1,(hex(site),addresses)
    delta=addresses.pop()-first
    # A skipped block may have private internal branches; external entries into
    # its interior would need their own adapter and are rejected here.
    for at in range(first,last,4):
        if site<=at<end:continue
        w=word(at);op=w>>26
        if op in (1,4,5,6,7,20,21,22,23):target=at+4+signed(w&65535)*4
        elif op in (2,3):target=(0x80000000|((w&0x3ffffff)<<2))-delta
        else:continue
        assert not site<target<end,('interior entry',hex(at),hex(target))
    name=f'lc_capacity_extra_{site:08x}'
    rows.append(dict(site=site,end=end,address=site+delta,name=name,reason=reason,
        owner=dict(start=first,size=last-first,retailSha256=hashlib.sha256(rom[first:last]).hexdigest()),
        expected=[word(a) for a in range(site,end,4)],bindings={n:v if v>=0x80000000 else v+delta for n,v in targets.items()},
        source=f'.set noat\n.set noreorder\n.globl {name}\n.type {name}, @function\n{name}:\n'+body.strip()+'\n'+f'.size {name}, . - {name}\n'))
def jump(target):return f'j {target}\nnop\n'
def pool(base,enemy,player):
    return f'''lui $at, 0x8019
addiu $at, $at, 0x0f80
beq ${base}, $at, player
nop
{enemy}
b done
nop
player:
{player}
done:
'''

add(0x119240,0x119268,'''sltiu $at, $s3, 30
bne $at, $zero, player
sll $v0, $v1, 1
lui $a0, %hi(lc_enemy_soldier_hp + 196)
addiu $a0, $a0, %lo(lc_enemy_soldier_hp + 196)
b done
nop
player:
lui $a0, 0x8019
addiu $a0, $a0, 0x0f80
done:
'''+jump('lc_continue'),dict(lc_continue=0x119268),'Soldier HP pool follows source ownership, including allied sources')
add(0x133cb0,0x133cb8,'''lui $at, %hi(lc_enemy_soldier_hp)
addiu $at, $at, %lo(lc_enemy_soldier_hp)
addu $a2, $at, $v0
lui $t0, %hi(lc_enemy_soldier_max_hp)
addiu $t0, $t0, %lo(lc_enemy_soldier_max_hp)
'''+jump('lc_continue'),dict(lc_continue=0x133cb8),'Rest healing separates indexed HP from shared maximum')
add(0x20b7d0,0x20b7d8,'''sll $at, $v1, 1
lui $t0, %hi(lc_enemy_soldier_hp)
addu $at, $at, $t0
lhu $t0, %lo(lc_enemy_soldier_hp)($at)
lui $at, %hi(lc_enemy_soldier_max_hp)
lhu $t1, %lo(lc_enemy_soldier_max_hp)($at)
'''+jump('lc_continue'),dict(lc_continue=0x20b7d8),'Formation Soldier HP and maximum')
add(0x21c308,0x21c310,pool('s2','''lui $at, %hi(lc_enemy_soldier_hp)
addiu $at, $at, %lo(lc_enemy_soldier_hp)
sll $v0, $a1, 1
addu $at, $at, $v0
sh $v1, 0($at)
addu $v0, $s2, $v0''','sh $v1, -196($v0)')+'''lui $ra, %hi(lc_continue)
addiu $ra, $ra, %lo(lc_continue)
'''+jump('lc_original_call'),dict(lc_continue=0x21c310,lc_original_call=0x80000000|((word(0x21c308)&0x3ffffff)<<2)),'Combat HP writeback before original status call')
for site in (0x21c328,0x21c33c):
    add(site,site+8,'lbu $v1, 246($s1)\n'+pool('s2','''lui $at, %hi(lc_enemy_soldier_state - 144)
addiu $at, $at, %lo(lc_enemy_soldier_state - 144)
addu $v1, $v1, $at''','addu $v1, $s2, $v1')+jump('lc_continue'),dict(lc_continue=site+8),'Combat state update retains existing set/clear rule')
add(0x23369c,0x2336ac,'''addiu $v1, $v1, -100
sll $v0, $v1, 1
'''+pool('s1','''lui $at, %hi(lc_enemy_soldier_hp + 200)
addiu $at, $at, %lo(lc_enemy_soldier_hp + 200)
addu $v0, $v0, $at
lhu $a0, 0($v0)''','addu $v0, $v0, $s1\nlhu $a0, 4($v0)')+jump('lc_continue'),dict(lc_continue=0x2336ac),'Supporting Soldier HP preserves signed handle-minus100 arithmetic')
add(0x2336b0,0x2336b8,pool('s1','''lui $at, %hi(lc_enemy_soldier_state + 100)
addu $at, $at, $v1
lbu $v0, %lo(lc_enemy_soldier_state + 100)($at)''','addu $v0, $s1, $v1\nlbu $v0, 244($v0)')+jump('lc_continue'),dict(lc_continue=0x2336b8),'Supporting Soldier state')
add(0x2336c4,0x2336d0,pool('s1','''lui $at, %hi(lc_enemy_soldier_max_hp)
lhu $a1, %lo(lc_enemy_soldier_max_hp)($at)''','lhu $a1, 2($s1)')+'''lui $ra, %hi(lc_continue)
addiu $ra, $ra, %lo(lc_continue)
'''+jump('lc_original_call'),dict(lc_continue=0x2336d0,lc_original_call=0x80000000|((word(0x2336c8)&0x3ffffff)<<2)),'Supporting Soldier shared maximum before original calculation')

add(0x41bcc,0x41bd4,'''andi $a0, $s5, 255
jal lc_capacity_delete_enemy
nop
'''+jump('lc_continue'),dict(lc_continue=0x41bfc),'Enemy deletion retains tags for surviving source references')
add(0x45f30,0x45f38,jump('lc_enemy_clear_members'),reason='Reset all tagged handles while retaining template0')
add(0x23b048,0x23b050,jump('lc_capacity_neutral_construct'),reason='Dedicated source60 for temporary encounter')
add(0x21bfac,0x21c044,'''jal lc_capacity_neutral_cleanup
nop
'''+jump('lc_continue'),dict(lc_continue=0x21c044),'Clean up only dedicated temporary source')
add(0x214318,0x214320,'''beq $s1, $zero, refuse
nop
lbu $at, 246($s2)
lui $v0, %hi(lc_enemy_types)
addu $at, $at, $v0
lbu $at, %lo(lc_enemy_types)($at)
addiu $at, $at, -1
bne $at, $zero, refuse
nop
sll $v0, $s1, 3
subu $v0, $v0, $s1
'''+jump('lc_continue')+'refuse:\n'+jump('lc_refuse'),dict(lc_continue=0x214320,lc_refuse=0x214544),'Defensive recruitment guard before clearing a player record')
add(0x1957d0,0x1957d8,'''andi $at, $a1, 127
sltiu $at, $at, 30
bne $at, $zero, player
nop
j lc_enemy_construct
nop
player:
'''+f'.word 0x{word(0x1957d0):08x}\n.word 0x{word(0x1957d4):08x}\n'+jump('lc_continue'),dict(lc_continue=0x1957d8),'Keep original player constructor; route map enemies to tagged allocator')
add(0x249dd4,0x249dfc,'''jal lc_capacity_find_enemy
nop
addu $s1, $v0, $zero
addiu $v0, $zero, 255
'''+jump('lc_continue'),dict(lc_continue=0x249dfc),'Class search includes all ordinary enemy handles, excludes Soldiers/template')
add(0x249ea8,0x249f00,'''addu $a0, $s2, $zero
jal lc_capacity_transfer_slots
addiu $a1, $sp, 16
beq $v0, $zero, refuse
nop
'''+jump('lc_continue')+'refuse:\n'+jump('lc_refuse'),dict(lc_continue=0x249f00,lc_refuse=0x249ea0),'Preflight scripted copy and initialize player IDs by formation slot')
# All volatile registers plus HI/LO survive the new release call. The original
# code has no call here, so normal caller-clobber assumptions would be unsafe.
caller=[2,3,4,5,6,7,8,9,10,11,12,13,14,15,24,25,31]
save='sb $zero, 0($s1)\naddiu $sp, $sp, -96\n'
save+='\n'.join(f'sw ${r}, {16+4*i}($sp)' for i,r in enumerate(caller))+'\n'
save+='mfhi $at\nsw $at, 84($sp)\nmflo $at\nsw $at, 88($sp)\naddu $a0, $s3, $zero\njal lc_capacity_release_detached\naddu $a1, $v1, $zero\n'
save+='lw $at, 84($sp)\nmthi $at\nlw $at, 88($sp)\nmtlo $at\n'
save+='\n'.join(f'lw ${r}, {16+4*i}($sp)' for i,r in enumerate(caller))+'\naddiu $sp, $sp, 96\nlw $s0, 0($s5)\n'
add(0x119278,0x119280,save+jump('lc_continue'),dict(lc_continue=0x119280),'Release a dead detached Soldier without clobbering live loop registers')

def ordinary_filter(index,ordinary,empty):
    return f'''lui $at, %hi(lc_enemy_types)
addu $at, $at, ${index}
lbu $at, %lo(lc_enemy_types)($at)
addiu $at, $at, -1
bne $at, $zero, empty
nop
{ordinary}
b ready
nop
empty:
{empty}
ready:
'''
add(0x1303f4,0x130400,ordinary_filter('s8','lbu $a0, 17($s6)','addu $a0, $zero, $zero')+'''beql $a0, $zero, skipped
addiu $s4, $s4, 1
'''+jump('lc_continue')+'skipped:\n'+jump('lc_skipped'),dict(lc_continue=0x130400,lc_skipped=0x130560),'Ordinary-only enemy recovery scan')
add(0x133298,0x1332a4,'''lui $at, %hi(lc_capacity_availability)
addu $at, $at, $s1
lbu $v0, %lo(lc_capacity_availability)($at)
sltiu $at, $s1, 100
bne $at, $zero, ready
nop
lui $at, %hi(lc_enemy_types)
addu $at, $at, $a3
lbu $at, %lo(lc_enemy_types)($at)
addiu $at, $at, -1
beq $at, $zero, ready
nop
ori $v0, $v0, 1
ready:
bnel $v0, $zero, skipped
addiu $a2, $a2, 52
'''+jump('lc_continue')+'skipped:\n'+jump('lc_skipped'),dict(lc_continue=0x1332a4,lc_skipped=0x1333b4),'Combined player/enemy availability includes all ordinary handles')
add(0x41dec,0x41df8,ordinary_filter('s0','''lui $v0, %hi(lc_enemy_characters + 17)
addu $v0, $v0, $s1
lbu $v0, %lo(lc_enemy_characters + 17)($v0)''','addu $v0, $zero, $zero')+'''beq $v0, $zero, skipped
addu $a0, $s3, $zero
'''+jump('lc_continue')+'skipped:\n'+jump('lc_skipped'),dict(lc_continue=0x41df8,lc_skipped=0x41e10),'Name uniqueness scans ordinary records without interpreting Soldier storage')
add(0x28381c,0x283828,'''beq $a1, $zero, ordinary
nop
lui $at, %hi(lc_enemy_types)
addu $at, $at, $a1
lbu $at, %lo(lc_enemy_types)($at)
addiu $at, $at, -1
bne $at, $zero, skipped
nop
ordinary:
lui $v0, %hi(lc_enemy_characters + 17)
addu $v0, $v0, $a2
lbu $v0, %lo(lc_enemy_characters + 17)($v0)
beq $v0, $a0, found
addiu $a1, $a1, 1
'''+jump('lc_continue')+'skipped:\naddiu $a1, $a1, 1\n'+jump('lc_continue')+'found:\n'+jump('lc_found'),dict(lc_continue=0x283828,lc_found=0x283874),'Class record lookup retains template0 and filters tagged records')
add(0x1d2094,0x1d20a4,'''jal lc_enemy_reset
nop
'''+jump('lc_continue'),dict(lc_continue=0x1d20a4),'New-game/reset path clears all expanded enemy storage')
add(0x219a64,0x219a6c,'''lui $ra, %hi(lc_continue)
addiu $ra, $ra, %lo(lc_continue)
'''+jump('lc_capacity_persuasion'),dict(lc_continue=0x219a6c),'Full player roster prevents destructive recruitment success')

for kind,sites in {'type':[(0x20dd1c,0x20dd28,0x801976e8),(0x20e640,0x20e64c,0x801976dc)],
                   'hp':[(0x20e0f8,0x20e100,0x801976e8),(0x20e954,0x20e95c,0x801976dc)],
                   'fields':[(0x20e310,0x20e330,0x801976e8),(0x20eb80,0x20eba0,0x801976dc)]}.items():
    body=(NATIVE/f'game/scenarios/capacity-battle-{kind}.s').read_text()
    # Keep numeric local labels; the catalog supplies the unique public entry.
    body='\n'.join(line for line in body.splitlines() if not line.startswith(('.text','.globl','.type','.ent','.end','.size','lc_capacity_battle_')))
    for site,end,source in sites:
        targets=dict(lc_battle_source=source)
        if kind=='type':targets.update(lc_battle_ordinary=end,lc_battle_soldier=0x20e0f8 if source==0x801976e8 else 0x20e954)
        else:targets['lc_battle_'+('hp' if kind=='hp' else 'fields')+'_continue']=end
        add(site,end,body,targets,'Combat '+kind+' keeps player layout and uses tagged enemy storage')

out=NATIVE/'game/scenarios/capacity-extra.json'
out.write_text(json.dumps(dict(generator='native/tools/generate-capacity-extra.py',inputRomSha256=digest,patches=rows),indent=2)+'\n',newline='\n')
print(json.dumps(dict(adapters=len(rows))))
