'use strict';
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const rom = fs.readFileSync(process.env.LORDLY_TEST_ROM);
if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
const m = new Mips(), dir = process.argv[2];
m.load(0x80070c00, rom.subarray(0x1000, 0x2f000));
m.load(0x8016af80, rom.subarray(0x40e80, 0x66e10));
m.set('sp', 0x803f0000);
const descriptor = 0x80187444, payload = 0x80350000;
if (process.argv[3] === 'verify-supplies') {
    const fixture = JSON.parse(fs.readFileSync(path.join(dir, 'supply-verify.json')));
    const delta = 0x801135a0, live = at => (at + delta) >>> 0;
    const root = 0x80260000, saved = 0x80310000, active = 0x80311000;
    m.w32(0x80196af8, root);
    m.load(live(0xbc3b8), rom.subarray(0xbc3b8, 0xbc464));
    for (const check of fixture.choices) {
        m.load(0x8018e6cc, Buffer.from(check.records, 'hex'));
        for (const first of [1, 40]) {
            m.load(0x80193ac0, Buffer.alloc(160));
            const ids = Array.from({length: Math.min(40, 45-first)}, (_,i) => first+i);
            ids.forEach((id,i) => {m.w16(0x80193ac0+i*4,id);m.w16(0x80193ac2+i*4,1);});
            m.call(live(0xbc3b8));
            const selected = Array.from({length:m.get('v0')},(_,i) => ids[m.r16(root+7150+i*2)]);
            assert.deepEqual(selected, check.allowed.filter(id => ids.includes(id)), 'original Army item choices');
        }
    }
    m.load(live(0xc192c), rom.subarray(0xc192c, 0xc1964));
    m.load(live(0xc1f58), rom.subarray(0xc1f58, 0xc1f94));
    for (const check of fixture.cases) {
        m.load(payload, Buffer.from(check.payload, 'hex'));
        m.set('a0', payload);m.set('a1',descriptor);m.call(0x800742f4,0x80000100,5000000);
        const record = Buffer.from(m.ram.slice(0x1971f0,0x1971f0+25));
        assert.deepEqual([...record.subarray(13,23)], check.items, 'original save decoder');
        m.load(saved,record);m.load(active,Buffer.alloc(54,0xa7));
        m.set('s4',saved);m.set('s2',active);m.call(live(0xc192c),live(0xc1964));
        assert.deepEqual(Array.from({length:10},(_,i)=>m.r16(active+34+i*2)),check.items,'original Army item load');
        assert.equal(m.r8(active+32),check.items.filter(Boolean).length,'load-stage occupied count');
        m.set('s0',saved);m.set('s1',active);m.call(live(0xc1f58),live(0xc1f94));
        const compact=check.items.filter(Boolean);while(compact.length<10)compact.push(0);
        assert.deepEqual([...m.ram.slice(m.phys(saved)+13,m.phys(saved)+23)],compact,'original Army item save');
        assert.deepEqual(Buffer.from(m.ram.slice(m.phys(saved),m.phys(saved)+13)),record.subarray(0,13));
        assert.deepEqual(Buffer.from(m.ram.slice(m.phys(saved)+23,m.phys(saved)+25)),record.subarray(23,25));
    }
    console.log('Original Army selection, packed-save decode and carried-item conversion passed:',fixture.cases.length,'edited saves');
    process.exit(0);
}
if (process.argv[3] !== 'verify') {
    // Explicit saved Soldier base, matching the captured Army's class pair1/1.
    m.w8(0x80193bd1, 1); m.w8(0x80193bd2, 1);
    for (let id = 1; id <= 9; ++id) {
        const at = 0x80193bc0 + id * 56, name = Buffer.alloc(13);
        Buffer.from('Member' + id).forEach((c, i) => name[i] = c + 16);
        name[7] = 255;
        m.load(at, name);
        m.w8(at + 17, 2); m.w8(at + 18, 2); m.w8(at + 19, 10);
        m.w16(at + 22, 100); m.w16(at + 24, 90);
        m.w8(at + 51, [1,4,5,6,7].includes(id) ? 2 : 0); m.w8(at + 52, id);
    }
    for (const [index, flags, ids, cells] of [
        [0, 1, [1, 2, 3, 0, 0], [0, 4, 8, 0, 0]],
        [1, 0x81, [1, 0, 0, 0, 0], [0, 0, 0, 0, 0]],
        [2, 1, [4, 100, 0, 0, 0], [1, 7, 0, 0, 0]],
        [3, 1, [5, 219, 0, 0, 0], [1, 7, 0, 0, 0]],
        [4, 1, [6, 220, 0, 0, 0], [1, 7, 0, 0, 0]],
        [5, 1, [7, 8, 9, 0, 0], [0, 4, 8, 0, 0]]
    ]) {
        const at = 0x801971f0 + index * 25;
        m.w8(at, index + 1); m.w8(at + 1, flags);
        ids.forEach((id, i) => m.w8(at + 2 + i, id));
        cells.forEach((cell, i) => m.w8(at + 7 + i, cell));
        m.w8(at + 12, 23); m.w8(at + 13, 9); m.w8(at + 23, 2); m.w8(at + 24, 2);
    }
    for (let i = 1; i < 100; ++i) m.w8(0x80197710 + i, i <= 9 ? i : 127);
    for (let i = 0; i < 30; ++i) m.w8(0x801977d8 + i, [0,2,3,4,5][i] ?? 31);
    m.w8(0x80190f80, 45); m.w8(0x80190f81, 9); m.w16(0x80190f82, 300);
    m.w16(0x80190f84, 83); m.w16(0x80190f84 + 119 * 2, 44);
    m.w8(0x80191074, 2); m.w8(0x80191074 + 119, 5);
    m.set('a0', payload); m.set('a1', descriptor); m.call(0x80074494, 0x80000100, 5000000);
    fs.writeFileSync(path.join(dir, 'unit-payload.bin'), Buffer.from(m.ram.slice(m.phys(payload), m.phys(payload) + 6180)));
    fs.writeFileSync(path.join(dir, 'unit-ram.bin'), Buffer.from(m.ram));
    const variant = name => {
        m.set('a0', payload); m.set('a1', descriptor); m.call(0x80074494, 0x80000100, 5000000);
        fs.writeFileSync(path.join(dir, name), Buffer.from(m.ram.slice(m.phys(payload), m.phys(payload) + 6180)));
    };
    for (let i=14;i<=17;++i) m.w8(0x801971f0+i,9); // Six capacity, five carried; losing one member is unsafe.
    variant('unit-capacity-payload.bin');
    for (let i=14;i<=17;++i) m.w8(0x801971f0+i,0);
    m.w8(0x80193bd1,44); m.w8(0x80193bd2,44); // Type7 overrides the ordinary members, unlike Soldier pair1/1.
    for (let i=14;i<=16;++i) m.w8(0x801971f0+2*25+i,9);
    variant('unit-soldier-overload-payload.bin');
    m.w8(0x801971f0+2*25+16,0);
    variant('unit-soldier-base-payload.bin');
    m.w8(0x80193bc0+10*56+17,2); m.w8(0x80193bc0+10*56+18,2); m.w8(0x80193bc0+10*56+19,10);
    m.w8(0x801971f0+2*25+4,10); m.w8(0x801971f0+2*25+9,0);
    for (const [member,id,cell] of [[3,2,1],[4,3,2]]) {
        m.w8(0x801971f0+5*25+2+member,id); m.w8(0x801971f0+5*25+7+member,cell);
    }
    m.w8(0x801971f3,0); m.w8(0x801971f4,0); m.w8(0x801971f8,0); m.w8(0x801971f9,0);
    variant('unit-full-payload.bin');
    m.w8(0x80193bc0 + 2 * 56 + 17, 0);
    variant('unit-missing-character-payload.bin');
    m.w8(0x801971f0+13, 21);
    variant('unit-unknown-supply-payload.bin');

} else {
    const fixture = JSON.parse(fs.readFileSync(path.join(dir, 'unit-verify.json')));
    for (const row of fixture.sizes) {
        m.set('a0', row.primary); m.set('a1', row.secondary);
        m.call(0x8016de1c);
        assert.equal(m.get('v0'), row.expected, `size for ${row.primary}/${row.secondary}`);
        m.set('a0', row.primary); m.set('a1', row.secondary);
        m.call(0x8016d200);
        assert.equal(m.get('v0'), row.capacity, `carrying capacity for ${row.primary}/${row.secondary}`);
        if (row.movement !== undefined) {
            m.set('a0', row.primary); m.set('a1', row.secondary); m.call(0x8016dbd8);
            assert.equal(m.get('v0'), row.movement, `movement type for ${row.primary}/${row.secondary}`);
        }
    }
    for (const [bits, expected] of fixture.movementCases || []) {
        for (let i=0;i<4;++i) m.set('a'+i, (bits >>> (i*3)) & 7);
        m.w32(m.get('sp')+16, (bits >>> 12) & 7);
        m.call(0x8016b088);
        assert.equal(m.get('v0'), expected, `movement priority tuple ${bits}`);
    }
    m.load(payload, Buffer.from(fixture.payload, 'hex'));
    m.set('a0', payload); m.set('a1', descriptor); m.call(0x800742f4, 0x80000100, 5000000);
    assert.deepEqual(Buffer.from(m.ram.slice(0x1971f0, 0x1971f0 + 150)), Buffer.from(fixture.units, 'hex'));
    // Whole-function identity was qualified against the Army snapshot. Execute
    // only field conversion blocks here; OS, menu allocation and natural load
    // are outside this reference check.
    const delta = 0x801135a0, live = at => (at + delta) >>> 0;
    m.load(live(0xb7fc8), rom.subarray(0xb7fc8, 0xb7fd0));
    for (const unit of [0, 5]) for (const unity of [0, 1, 23, 99, 100, 127]) {
        const base = 0x80260000 + unit * 54, at = base + 4507;
        m.w8(at - 1, 0xa7); m.w8(at, unity); m.w8(at + 1, 0xa7);
        m.set('v1', base); m.set('v0', 1); // The original ordinary nonleader removal flag.
        m.call(live(0xb7fc8), live(0xb82ac));
        assert.equal(m.r8(at), 0); assert.equal(m.r8(at - 1), 0xa7); assert.equal(m.r8(at + 1), 0xa7);
    }
    m.load(live(0xb8258), rom.subarray(0xb8258, 0xb82b0));
    for (const unit of [0,5]) for (const mode of [0,1,2,3]) for (const unity of [0,23,100]) {
        const root=0x80260000, active=root+0x117c+unit*54, metadata=0x80320000;
        m.w32(0x80196af8,root); m.w16(root+386,mode); m.w8(active+31,unity);
        m.w8(active+30,0xa7); m.w8(active+32,0xa7);
        m.set('v1',unit); m.set('s4',2); m.set('s5',1); m.set('s1',metadata); m.set('s7',0);
        m.call(live(0xb8258),live(0xb82b0));
        assert.equal(m.r8(active+5),2); assert.equal(m.r8(metadata),130);
        assert.equal(m.r8(active+31),mode===2 ? 0 : unity);
        assert.equal(m.r8(active+30),0xa7); assert.equal(m.r8(active+32),0xa7);
    }
    m.load(live(0xc2fec), rom.subarray(0xc2fec, 0xc3044));
    for (const id of [100, 101, 219]) {
        m.set('s6', id); m.set('sp', 0x803f0000);
        m.call(live(0xc2fec), live(0xc3044));
        assert.equal(m.get('a0'), fixture.soldierSize, `Soldier placement size ${id}`);
    }
    m.load(live(0xc188c), rom.subarray(0xc188c, 0xc192c));
    m.load(live(0xc1ed0), rom.subarray(0xc1ed0, 0xc1f58));
    const saved = 0x80310000, active = 0x80311000, root = 0x80260000;
    m.w32(0x80196af8, root);
    for (const row of fixture.cases) {
        const record = Buffer.from(row.record, 'hex');
        m.load(saved, record); m.load(active, Buffer.alloc(54, 0xff));
        m.w8(active + 1, 0);
        m.set('s4', saved); m.set('s2', active); m.set('s8', 8);
        m.call(live(0xc188c), live(0xc192c));
        const grid = Array.from({length: 9}, (_, i) => m.r8(active + 4 + i));
        assert.deepEqual(grid, row.grid);
        assert.equal(m.r8(active + 3), row.leaderCell);
        m.set('s0', saved); m.set('s1', active); m.set('s6', 0x80193bc0); m.set('s3', 255);
        m.call(live(0xc1ed0), live(0xc1f58));
        assert.equal(m.r8(saved + 2), record[2], 'leader identity remains first');
        assert.equal(m.r8(saved + 7), row.leaderCell);
        const pairs = [];
        for (let i = 0; i < 5; ++i) if (m.r8(saved + 2 + i)) pairs.push([m.r8(saved + 2 + i), m.r8(saved + 7 + i)]);
        const expected = row.grid.flatMap((id, cell) => id === 255 ? [] : [[id, cell]]);
        assert.deepEqual(pairs.sort((a,b) => a[0]-b[0]), expected.sort((a,b) => a[0]-b[0]));
    }
    m.load(live(0xbd154),rom.subarray(0xbd154,0xbd26c));
    m.load(live(0xbcee4),rom.subarray(0xbcee4,0xbd0d8));
    for (const row of fixture.unitMovements || []) {
        const record=Buffer.from(fixture.units,'hex').subarray(row.index*25,row.index*25+25);
        const dest=root+0x117c+row.index*54;
        m.load(saved,record); m.load(dest,Buffer.alloc(54,0xff)); m.w8(dest+1,0);
        m.set('s4',saved);m.set('s2',dest);m.set('s8',8);m.call(live(0xc188c),live(0xc192c));
        m.set('a0',row.index);m.call(live(0xbd154));
        assert.equal(m.get('v0'),row.expected,`derived movement for squad ${row.index}`);
        if(row.carried!==undefined) {
            for(let i=0;i<10;++i)m.w16(dest+34+i*2,i<row.carried?9:0);
            m.set('a0',row.index);m.call(live(0xbcee4));
            assert.equal(m.r8(dest+32),row.capacity,`native carrying capacity for squad ${row.index}`);
            for(let i=0;i<row.carried;++i)assert.equal(m.r16(dest+34+i*2),9);
        }
    }
    for (const row of fixture.reserveChecks || []) {
        m.load(payload, Buffer.from(row.payload, 'hex'));
        m.set('a0', payload); m.set('a1', descriptor); m.call(0x800742f4, 0x80000100, 5000000);
        m.w32(0x80196af8, root);
        // Original initialization clears assignment flags and discovers ordinary
        // characters by their primary class, then initializes the free count.
        m.load(live(0xc1738), rom.subarray(0xc1738, 0xc1814));
        m.call(live(0xc1738), live(0xc1814));
        for (let index = 0; index < 30; ++index) {
            const source = 0x801971f0 + index * 25;
            if ((m.r8(source + 1) & 0x81) !== 1) continue;
            m.load(active, Buffer.alloc(54, 0xff));
            m.w8(active + 1, (m.r8(source + 1) & 4) ? 16 : 0);
            m.set('s4', source); m.set('s2', active); m.set('s8', 8);
            m.call(live(0xc188c), live(0xc192c));
        }
        const actual = [];
        for (let id = 1; id < 100; ++id)
            if (m.r8(0x80193bc0 + id * 56 + 17) && !(m.r8(root + 6258 + id * 2) & 8)) actual.push(id);
        assert.deepEqual(actual, row.members, 'original Army reserve membership');
    }
    console.log(`PASS ${fixture.sizes.length} original class pairs, ${(fixture.movementCases||[]).length} movement tuples, 12 removal/24 placement Unity cases, save decoder, ${fixture.cases.length} formation conversions, ${(fixture.unitMovements||[]).length} unit finalizers and ${(fixture.reserveChecks||[]).length} reserve snapshots. No natural game launch or complete menu claim.`);
}
