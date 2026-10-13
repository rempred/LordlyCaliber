"""Generate guarded, register-preserving member-kind adapters.

The listed sites were traced to a source ID or an explicit player/enemy pool.
Only the original result and the assembler scratch register may change. Branch
delay slots are replayed in their original order, including likely annulment.
This catalog is deliberately explicit; it never searches/replaces all <100s.
"""
from pathlib import Path
import hashlib
import json
import struct

NATIVE = Path(__file__).resolve().parents[1]
ROOT = NATIVE.parent.parent
rom = bytearray((ROOT/'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64').read_bytes())
if rom[:4] == bytes.fromhex('37804012'):
    rom[::2],rom[1::2] = rom[1::2],rom[::2]
digest = hashlib.sha256(rom).hexdigest()
assert digest == '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a'
word = lambda a: struct.unpack_from('>I',rom,a)[0]
signed = lambda n: n-65536 if n&32768 else n
owners = json.loads((ROOT/'wiki/sol-decomp-static-db-r3-20260710/exports/canonical-functions.json').read_text())
maps = [dict(start=word(a+8),end=word(a+12),address=word(a)) for a in range(0x387c0,0x38ab8,40)]
maps += json.loads((ROOT/'tmp/enemy-capacity-20261012/overlay-calls.json').read_text())['calls']
maps += [dict(start=0x1000,end=0x2f000,address=0x80070c00),dict(start=0x40e80,end=0x66e10,address=0x8016af80)]
contexts = {}
def add(sites,context):
    for a in sites.split():
        a=int(a,16)
        assert a not in contexts
        contexts[a]=context

add('133C50 133ADC 12DAC0 130264 1302F0 1588C0',dict(kind='source',register=5))
add('131530',dict(kind='playerFlag',register=9))
add('119234 1192DC',dict(kind='source',register=19))
add('132D84 13307C 132E7C 133174',dict(kind='source',register=17,loads=[['byte',4]]))
add('20982C',dict(kind='source',register=19,loads=[['word',16]]))
add('20ABEC 20B7AC',dict(kind='source',register=29,loads=[['word',52],['word',16]]))
add('21C254',dict(kind='source',register=4))
add('135FB0',dict(kind='source',register=19,loads=[['byte',0]],limit=31))
add('137ABC',dict(kind='source',register=29,loads=[['word',84],['word',0]]))
add('21D4D4 1198E8 1506D8 1508EC 15B9EC 1ADF18 1AE2D4 2AD648 1894F0 15DFC8',dict(kind='enemy'))
add('233690',dict(kind='enemyBase',register=17,playerBase=0x80190f80))
add('55B70',dict(kind='poolFlag',register=3))
add('84814',dict(kind='poolFlag',address=0x801b8228,loads=[['byte',0]]))
add('8E89C',dict(kind='poolFlag',register=18,mask=255))
add('15428C',dict(kind='poolFlag',register=19,mask=255))
add('1386F4',dict(kind='poolFlag',register=21))
add('15956C',dict(kind='playerFlag',register=5))
add('83A10',dict(kind='poolFlag',register=19,loads=[['word',0],['byte',1]],mask=128))
add('15E4EC 15E58C 18B938',dict(kind='poolFlag',address=0x801f36a0,loads=[['byte',0]],mask=1))
add('18931C',dict(kind='eitherEnemy',register=20,address=0x8022b573))
add('BD3D4 BD444 BD620 BD8D8 BDBC4 BDE1C BE074 BE284 BE49C 1526BC 152940 152C70 153014 153308 1535FC 15389C',dict(kind='poolFlag',register=4,mask=255))
add('189858 189868 189908 189918 189960 189A14 189A2C 189E34 189EBC 189EE0 189FA4 18A6C4 18B4E8 18A284 18AF90',dict(kind='poolFlag',register=19))

def branch(w): return w>>26 in (1,4,5,6,7,20,21,22,23)
def transfer(w): return branch(w) or w>>26 in (2,3) or (w>>26==0 and w&63 in (8,9))
def branch_line(w,label):
    op,rs,rt=w>>26,w>>21&31,w>>16&31
    if op in (4,5,20,21): return f"{ {4:'beq',5:'bne',20:'beql',21:'bnel'}[op]} ${rs}, ${rt}, {label}"
    if op in (6,7,22,23): return f"{ {6:'blez',7:'bgtz',22:'blezl',23:'bgtzl'}[op]} ${rs}, {label}"
    assert op==1 and rt in (0,1,2,3)
    return f"{ {0:'bltz',1:'bgez',2:'bltzl',3:'bgezl'}[rt]} ${rs}, {label}"

def predicate(w,c,prefix):
    src,dst=w>>21&31,w>>16&31
    assert src!=1 and dst!=1
    lines=[]
    if c['kind']!='enemy':
        if c['kind']=='enemyBase':
            a=c['playerBase'];lines += [f'lui $at, 0x{(a+32768)>>16:04x}',f'addiu $at, $at, {signed(a&65535)}',f"beq ${c['register']}, $at, {prefix}_player",'nop']
        elif c['kind']=='eitherEnemy':
            lines += [f"bne ${c['register']}, $zero, {prefix}_enemy",'nop']
        if c['kind']=='enemyBase': pass
        elif 'address' in c:
            a=c['address'];lines += [f'lui $at, 0x{(a+32768)>>16:04x}',f'addiu $at, $at, {signed(a&65535)}']
        else: lines += [f"addu $at, ${c['register']}, $zero"]
        for size,offset in c.get('loads',[]): lines += [f"{'lbu' if size=='byte' else 'lw'} $at, {offset}($at)"]
        if c['kind']=='eitherEnemy': lines += ['lbu $at, 0($at)']
        if c.get('mask'): lines += [f"andi $at, $at, {c['mask']}"]
        if c['kind'] in ('source','eitherEnemy'):lines += [f"sltiu $at, $at, {c.get('limit',30)}"]
        if c['kind']!='enemyBase':
            operation='beq' if c['kind']=='poolFlag' else 'bne'
            lines += [f'{operation} $at, $zero, {prefix}_player','nop']
    lines += [prefix+'_enemy:','lui $at, %hi(lc_enemy_types)',f'addu $at, $at, ${src}',
        f'lbu ${dst}, %lo(lc_enemy_types)($at)',f'addiu ${dst}, ${dst}, -2',f'sltu ${dst}, $zero, ${dst}']
    if c['kind']!='enemy': lines += [f'b {prefix}_done','nop',prefix+'_player:',f'.word 0x{w:08x}',prefix+'_done:']
    return lines

rows=[]
for site,c in sorted(contexts.items()):
    w=word(site);assert w>>26 in (10,11) and w&65535==100,hex(site)
    owner=next(o for o in owners if o['rom_start']<=site<o['rom_end_exclusive'])
    start,end=owner['rom_start'],owner['rom_end_exclusive']
    addresses={m['address']+start-m['start'] for m in maps if m['start']<=start<end<=m['end']}
    assert len(addresses)==1,(hex(site),addresses)
    base=addresses.pop();delta=base-start
    name=f'lc_capacity_type_{site:08x}'
    lines=['.set noat','.set noreorder',f'.globl {name}',f'.type {name}, @function',f'{name}:']
    bindings={};displaced=[];entry_offsets={}
    def jump(label): return [f'j {label}','nop']
    if site>start and transfer(word(site-4)):
        b=word(site-4);assert branch(b),hex(site)
        hook=site-4;count=2;kind='precedingBranch'
        target=hook+4+signed(b&65535)*4+delta
        bindings.update(lc_taken=target,lc_continue=site+4+delta)
        lines += [branch_line(b,'taken'),'nop']
        if b>>26 not in (20,21,22,23) and not (b>>26==1 and (b>>16&31) in (2,3)):
            lines += predicate(w,c,'not_taken')
        lines += jump('lc_continue')+['taken:']+predicate(w,c,'taken_pred')+jump('lc_taken')
    elif branch(word(site+4)):
        b=word(site+4);assert not transfer(word(site+8))
        hook=site;count=3;kind='followingBranch'
        bindings.update(lc_taken=site+8+signed(b&65535)*4+delta,lc_continue=site+12+delta,lc_displaced_0=word(site+8))
        displaced.append(dict(symbol='lc_displaced_0',site=site+8))
        lines += predicate(w,c,'pred')
        entry_offsets[str(site+4)] = sum(4 for line in lines if not line.endswith(':') and (not line.startswith('.') or line.startswith('.word')))
        lines += [branch_line(b,'taken'),'.word lc_displaced_0']+jump('lc_continue')+['taken:']+jump('lc_taken')
    else:
        assert not transfer(word(site+4)),hex(site)
        hook=site;count=2;kind='plain'
        bindings.update(lc_continue=site+8+delta,lc_displaced_0=word(site+4))
        displaced.append(dict(symbol='lc_displaced_0',site=site+4))
        lines += predicate(w,c,'pred')+['.word lc_displaced_0']+jump('lc_continue')
    rows.append(dict(site=site,hook=hook,address=hook+delta,kind=kind,context=c,
        expected=[word(hook+4*i) for i in range(count)],name=name,bindings=bindings,displaced=displaced,entryOffsets=entry_offsets,
        owner=dict(start=start,size=end-start,retailSha256=hashlib.sha256(rom[start:end]).hexdigest()),
        source='\n'.join(lines)+f'\n.size {name}, . - {name}\n'))
for a,b in zip(rows,rows[1:]):assert a['hook']+4*len(a['expected'])<=b['hook'],(a['site'],b['site'])
# Some original branches join at another predicate's branch instruction, using
# a result already calculated in the predecessor's delay slot. Preserve that
# entry; do not run the destination predicate again or land on overwritten code.
for row in rows:
    for symbol,value in list(row['bindings'].items()):
        if symbol not in ('lc_taken','lc_continue'):continue
        for target in rows:
            delta=target['address']-target['hook'];site=value-delta
            if target['hook']<=site<target['hook']+4*len(target['expected']):
                if site==target['hook']:offset=0
                else:
                    assert str(site) in target['entryOffsets'],(hex(row['site']),hex(site))
                    offset=target['entryOffsets'][str(site)]
                row['bindings'][symbol]=dict(entry=target['name'],offset=offset,originalAddress=value)
                break
# Reject unhandled local incoming edges into an overwritten interior. Adapted
# branches use the explicit entry bindings above. This check does not claim to
# identify arbitrary computed/indirect jumps from outside these owners.
for target in rows:
    owner=target['owner'];first=owner['start'];last=first+owner['size']
    delta=target['address']-target['hook']
    for at in range(first,last,4):
        w=word(at)
        if branch(w):to=at+4+signed(w&65535)*4
        elif w>>26 in (2,3):to=(0x80000000|((w&0x3ffffff)<<2))-delta
        else:continue
        if target['hook']<to<target['hook']+4*len(target['expected']):
            assert any(r['hook']<=at<r['hook']+4*len(r['expected']) for r in rows),('unadapted incoming edge',hex(at),hex(to))
out=NATIVE/'game/scenarios/capacity-predicates.json'
out.write_text(json.dumps(dict(generator='native/tools/generate-capacity-predicates.py',inputRomSha256=digest,patches=rows),indent=2)+'\n',newline='\n')
print(json.dumps(dict(predicates=len(rows),shapes={k:sum(r['kind']==k for r in rows) for k in ['plain','precedingBranch','followingBranch']})))
