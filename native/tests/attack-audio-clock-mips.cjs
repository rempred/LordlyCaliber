'use strict';
// Compare exported audio scheduling against the original ROM. The original
// callback, alAudioFrame scheduler and microsecond-to-sample conversion execute.
// RSP mixing/voice output are observed sinks; this is not an acoustic test.
const fs = require('node:fs'), assert = require('node:assert/strict');
const { FloatMips } = require('./float-mips.cjs');
const read = p => { const b = fs.readFileSync(p); if (b.readUInt32BE(0) === 0x37804012) b.swap16(); return b; };
const candidate = read(process.argv[2]), original = read(process.argv[3]);
function machine(rom, install) {
    const m = new FloatMips(); m.ram = new Uint8Array(0x800000);
    m.load(0x80070000, rom.subarray(0x400, 0x2f000));
    m.load(0x800a7c00, rom.subarray(0x38000, 0x3f000));
    m.set('sp', 0x807ff000);
    if (install) {
        const h = 0x27c5300, r = n => rom.readUInt32BE(h + n);
        assert.equal(rom.toString('ascii', h, h + 8), 'LCATK001');
        m.load(r(16), rom.subarray(r(12), r(12) + r(20)));
        let installed = 0;
        for (let i = 0; i < r(64); ++i) {
            const row = r(60) + i * 24, at = m.r32(row);
            if (![0x126ac, 0x1299c, 0x12ee8].includes(at)) continue;
            // Post-DMA state; the separate startup regression executes installer.
            m.w32(0x8006fc00 + at, 0x08000000 | ((m.r32(row + 12) >>> 2) & 0x3ffffff));
            m.w32(0x8006fc00 + at + 4, 0); installed++;
        }
        assert.equal(installed, 3);
    }
    const call = (pc, args = []) => {
        args.forEach((v, i) => i < 4 ? m.set('a' + i, v) : m.w32(m.get('sp') + 16 + (i - 4) * 4, v));
        m.call(pc, 0x80000100, 3000000); return m.get('v0');
    };
    const bank = 0x80104710, fx = 0x8010a760, channels = 0x80400000, voices = 0x80410000;
    m.load(bank, rom.subarray(0x423ff0, 0x429cd0)); m.load(fx, rom.subarray(0x4e3140, 0x4f0fb0));
    m.trap(0x800901c0, () => {});
    call(0x80081db4, [bank, 0x429cd0]); call(0x80081f7c, [fx]); call(0x8008202c, [fx, bank]);
    for (const [a, v] of [[0x800b1804, 68], [0x800b1808, voices], [0x800b180c, channels],
        [0x800b1810, channels + 4 * 0x13c], [0x800b1814, 60], [0x800b1818, 16666],
        [0x800b1820, 1], [0x800b1824, 0x12345678]]) m.w32(a, v);
    m.w16(0x800b181c, 26623); m.w16(0x800b181e, 32767);
    for (let i = 0; i < 68; ++i) call(0x80083738, [channels + i * 0x13c]);
    let ticks = 0; const events = [];
    const exec = m.exec.bind(m); m.exec = (pc, next) => { if (pc === 0x800822ac) ticks++; return exec(pc, next); };
    for (const [pc, kind] of [[0x800884c0, 'start'], [0x80088550, 'stop'], [0x80088410, 'volume'],
        [0x80088380, 'pitch'], [0x800882e0, 'pan'], [0x80088240, 'fxmix']])
        m.trap(pc, c => events.push([ticks, kind, c.get('a0'), c.get('a1'), c.get('a2')]));
    return { m, call, events, ticks: () => ticks, channels };
}
for (const interval of [16666, 20000]) {
    const retail = machine(original, false), edited = machine(candidate, true);
    for (const h of [retail, edited]) h.m.w32(0x800b1818, interval);
    for (let i = 0; i < 8; ++i) {
        const expected = retail.call(0x800822ac, [0x80500100]);
        assert.equal(expected, interval, 'Original callback supplies the scheduler interval');
        assert.equal(edited.call(0x800822ac, [0x80500100]), expected,
            `Exported callback must preserve ${interval} microseconds on call ${i + 1}`);
    }
}
function schedule(rom, install, active) {
    const h = machine(rom, install), { m, call } = h;
    const synth = 0x80500000, client = 0x80500100;
    // Live Project64 callback RA=800888CC identifies this scheduler copy,
    // ROM18C40. Its synthesis sink is separate from the callback/time loop.
    m.w32(0x800a9e54, synth); m.w32(synth, client);
    m.w32(synth + 64, 32000); m.w32(synth + 68, 160); m.w32(client + 8, 0x800822ac);
    m.trap(0x80088b30, c => c.set('v0', c.get('a1')));
    if (active) call(0x80081770, [303]);
    const rows = [];
    for (let i = 0; i < 180; ++i) {
        call(0x80088840, [0x80600000, 0x80502000, 0x80680000, 544]);
        rows.push([h.ticks(), m.r32(client + 16), m.r32(synth + 28), m.r32(synth + 32)]);
    }
    return { rows, events: h.events, channels: Buffer.from(m.ram.subarray(h.channels & 0x7fffff, (h.channels & 0x7fffff) + 68 * 0x13c)) };
}
for (const active of [false, true]) {
    const expected = schedule(original, false, active), actual = schedule(candidate, true, active);
    if (active) assert(expected.events.some(e => e[1] === 'start'), 'Active case must produce voices');
    assert.deepEqual(actual.rows, expected.rows, `Native scheduler cadence, active=${active}`);
    assert.deepEqual(actual.events, expected.events, 'Native voice starts, stops, envelopes and pitch');
    assert.deepEqual(actual.channels, expected.channels, 'Original channels remain unchanged');
}
console.log('PASS: callback intervals preserved; 360 native scheduler buffers match retail with idle and active sound, including voice timing and channel state.');
