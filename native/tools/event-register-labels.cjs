'use strict';

// Presentation of reviewed, per-instruction roles. Registers have no permanent
// names. Never propagate a role to a later instruction or infer a story identity
// merely because two constants happen to match.
function shortRole(role) {
  if (!role) return '';
  const s = role.replace(/; consumer 0x[0-9a-f]+/gi, '').trim();
  if (/restored saved value/i.test(s)) return 'saved register value';
  if (/or working value/i.test(s)) return 'caller value';
  if (/^(?:Input to:|prepared argument;|argument consumed by|consumed by|copied low byte|prepared16-bit argument)/i.test(s)) return '';
  if (/^(?:comparison|arithmetic operand)/i.test(s))
    return /^comparison/i.test(s) ? 'comparison value' : 'arithmetic operand';
  if (/persistent flag ID|flag ID|Flag\s*\d+.*\bID\b/i.test(s)) return 'flag number';
  if (/Flag\s*\d+ value|persistent story flag/i.test(s)) return 'flag value: ON/OFF';
  if (/dialogue.*(?:control|index)|(?:control|index).*dialogue/i.test(s)) return 'dialogue control';
  if (/dialogue answer|dialogue reply|choice result/i.test(s)) return 'dialogue answer';
  if (/insufficient Goth|current army Goth.*exceed|exceeds current army Goth/i.test(s)) return 'Goth required';
  if (/recruitment template|army operation[01] argument/i.test(s)) return 'recruitment template';
  if (/Director selector|cutscene selector|cutscene\s*\d+|Director\s*\d+/i.test(s)) return 'cutscene ID';
  if (/scenario key|selected scenario|staged.*scenario/i.test(s)) return 'scenario ID';
  if (/transition request|game.mode.*request|request.*game.mode|request token/i.test(s)) return 'next game operation';
  if (/local.call displacement|relative local.call destination/i.test(s)) return 'helper destination';
  if (/ReturnedBoolean|result.*Boolean/i.test(s)) return 'match result: yes/no';
  if (/Requested story identity\/class/i.test(s)) return 'required identity/class';
  if (/Candidate primary identity or secondary class/i.test(s)) return 'candidate identity/class';
  if (/Candidate secondary class/i.test(s)) return 'current class to check';
  if (/last valid.*character index/i.test(s)) return 'last character slot';
  if (/last persistent army record index/i.test(s)) return 'last character slot';
  if (/character record.*scan|army record index.*scan|character.slot|army character index|character.member eligibility scan/i.test(s)) return 'character slot';
  if (/primary.*identity|original identity|story identity|identity.*class/i.test(s)) return 'character identity';
  if (/secondary class|current class/i.test(s)) return 'current class';
  if (/character current HP|member current HP/i.test(s)) return 'current HP';
  if (/status.*&\s*(?:4|0x0?4)\b/i.test(s)) return 'Petrification test';
  if (/character status|member.*status/i.test(s)) return 'status effects';
  if (/squad.*flags|flags.*squad/i.test(s)) return 'squad flags';
  if (/dispatch source squad|contacting source squad|source squad index/i.test(s)) return 'acting squad';
  if (/combat.target squad|opposing squad index/i.test(s)) return 'target squad';
  if (/dispatch target one.based index/i.test(s)) return 'contact target number';
  if (/dispatch target zero.based index/i.test(s)) return 'contact target number';
  if (/squad member|member ID/i.test(s)) return 'squad member';
  if (/member.list position|formation.list position/i.test(s)) return 'formation position';
  if (/game.clock hour|game clock hour/i.test(s)) return 'hour';
  if (/game.clock minute|game clock minute/i.test(s)) return 'minute';
  if (/current calendar day/i.test(s)) return 'calendar day';
  if (/current calendar month/i.test(s)) return 'calendar month';
  if (/current calendar year/i.test(s)) return 'calendar year';
  if (/Chaos Frame/i.test(s)) return 'Chaos Frame';
  if (/consumable\s*ID/i.test(s)) return 'consumable ID';
  if (/inventory slot/i.test(s)) return 'inventory slot';
  if (/arithmetic result/i.test(s)) return 'calculated value';
  if (/mask(?:\s|0x|\d)/i.test(s)) return 'bit mask';
  // Keep short concrete roles from research (e.g. next world reveal-stream
  // byte index), but do not turn whole sentences or register expressions into
  // labels. Unknown or caller-dependent roles remain numeric.
  if (s.length <= 48 && /[a-z]/i.test(s) && !/\bR[0-7]\b|[;:=]|\b(?:prepare|read|write|input|argument|value|context|consumer|predicate)\b/i.test(s))
    return s;
  return '';
}

function flagUse(instructions, at, reg) {
  // Only a contiguous preparation/read or preparation/write sequence can
  // refine "flag number" to "flag to check/change". Stop at all other code;
  // in particular do not borrow a use across branches, calls or handlers.
  for (let n = at + 1; n < instructions.length && n <= at + 5; ++n) {
    if (instructions[n].offset !== instructions[n-1].offset + 2) break;
    const op = instructions[n].word >> 8, arg = instructions[n].word & 255;
    if (arg >= 0x60 && arg <= 0x67) {
      if (op >= 0x60 && op <= 0x67 && (arg & 7) === reg) return 'flag to check';
      if (op >= 0x70 && op <= 0x77) {
        if ((arg & 7) === reg && (op & 7) === reg) return 'flag number and value';
        if ((arg & 7) === reg) return 'flag to change';
        if ((op & 7) === reg) return 'flag value: ON/OFF';
      }
    }
    const immediate = op >= 0x68 && op <= 0x6f;
    const highByte = op >= 0x88 && op <= 0x8f;
    if ((!immediate && !highByte) || (immediate && (op & 7) === reg)) break;
  }
  return '';
}

// Index domains follow the native getter/setter contracts in vm-semantics.json
// and vm.cjs. These qualify each operand, not the whole descriptive sentence.
function contractRoles(native) {
  const input = new Map(), output = new Map();
  if (!native) return {input, output};
  for (const r of native.status === 'verified' ? native.registers || [] : []) {
    const role = shortRole(r.role); if (role) output.set(r.register, role);
  }
  for (const e of native.status === 'verified' ? native.effects || [] : []) {
    if (!['service-read','service-write'].includes(e.type)) continue;
    const primary = e.bank === 'getters' || e.bank === 'setters';
    const id = e.service;
    let index = '';
    if (primary) {
      if (id === 1) { index = 'formation position'; input.set('R7', 'squad index'); }
      else if ((id >= 2 && id <= 6) || id === 28) index = 'character slot';
      else if (id >= 7 && id <= 11) index = 'opposing character slot';
      else if (id === 12) index = e.type === 'service-read' ? 'flag to check' : 'flag to change';
      else if (id === 13) index = 'squad index';
      else if (id === 14 || id === 15) index = 'Soldier slot';
      else if (id === 16 || id === 17) index = 'opposing Soldier slot';
      else if (id === 18) index = 'dialogue control';
      else if (id >= 19 && id <= 21) index = 'town index';
      else if (id === 22) index = 'map location';
      else if (id === 23) index = 'road index';
      else if (id === 24 || id === 25) index = 'scene actor slot';
      else if (id === 27) index = 'temporary flag number';
    } else {
      if (e.bank === 'altGetters' && id === 1) index = 'consumable ID';
      else if (id >= 2 && id <= 4) index = 'consumable inventory slot';
      else if (e.bank === 'altGetters' && id === 5) index = 'equipment ID';
      else if (id >= 6 && id <= 8) index = 'equipment inventory slot';
      else if ((e.bank === 'altGetters' && id === 11) || (e.bank === 'altSetters' && id === 9)) index = 'character slot';
      else if (e.bank === 'altSetters' && id === 10) index = 'map location';
      else if (e.bank === 'altGetters' && id === 12) index = 'build-stamp mask';
    }
    if (index) input.set(e.indexRegister, index);
    if (e.bank === 'getters' && id === 18 && e.indexValue === 3)
      output.set(`R${native.opcode & 7}`, 'dialogue answer');
    if (e.squadRegister) input.set(e.squadRegister, 'squad index');
    if (e.valueRegister) {
      const value = primary && id === 1 ? 'squad member' :
        primary && (id === 12 || id === 27) ? 'flag value: ON/OFF' :
        e.bank === 'altSetters' && id === 9 ? 'character side' : shortRole(e.serviceMeaning);
      if (value) input.set(e.valueRegister, value);
    }
  }
  const op = native.opcode, arg = native.operand;
  if (op === 0x10) input.set('R0', 'cutscene ID');
  else if (op === 0x11) input.set('R0', 'next game operation');
  else if (op === 0x15) { input.set('R0','calendar year'); input.set('R1','calendar month'); input.set('R2','calendar day'); }
  else if (op === 0x17) input.set('R0', 'days to advance');
  else if (op === 0x1a) input.set('R0', 'horizontal map scroll');
  else if (op === 0x1b) { input.set('R7','reveal-stream position'); output.set('R7','reveal-stream position'); }
  else if (op === 0x1c || op === 0x1d) { input.set('R0','unused argument'); input.set('R1','report section'); }
  else if (op === 0x1e) input.set('R0','People report character');
  else if (op === 0x2d) input.set('R0','Goth required');
  else if (op === 0x2e) input.set('R0','Goth amount');
  else if (op === 0x05) input.set(`R${arg & 7}`, 'saved register value');
  else if (op === 0x06) output.set(`R${arg & 7}`, 'saved register value');
  return {input, output};
}

function labelRegisters(text, instruction, instructions, at, native = [], closure) {
  // The old shared birthday prose also mentions R7 on Amazon/Fighter fallback
  // setters that actually read R6 for both fields. Show the real value operand.
  if (/; pair\s*R6\/R7 comes from actual eligible story character or alternating Amazon\/Fighter fallback/.test(text))
    text = text.replace(/; pair\s*R6\/R7 comes from actual eligible story character or alternating Amazon\/Fighter fallback/, `; value from R${(instruction.word >> 8) & 7}`);
  if (closure?.quotedTarget) {
    const quote = text.match(/^(.*?(?:\bdecoded\s*0x[0-9a-f]+|\bContinue\s*@?[0-9a-f]+|\bContinue\s+(?:at\s*)?0x[0-9a-f]+)\s*:\s*)([\s\S]+)$/i);
    const targetAt = instructions.findIndex(i => i.offset === closure.quotedTarget.offset && i.word === closure.quotedTarget.word);
    if (quote && targetAt >= 0)
      return quote[1] + labelRegisters(quote[2], instructions[targetAt], instructions, targetAt, native, closure.quotedTarget);
  }
  const roles = new Map(), op = instruction.word >> 8, arg = instruction.word & 255;
  const assign = (reg, role) => { if (/^R[0-7]$/.test(reg) && role) roles.set(reg, role); };
  const contracts = contractRoles(native[at]);
  for (const [reg, role] of contracts.input) assign(reg, role);
  for (const [reg, role] of contracts.output) assign(reg, role);
  // Sources are input roles; explicit instruction roles may refine them or
  // replace them with the result role of a read/write on the same register.
  for (const r of instruction.registers || []) assign(r.register, shortRole(r.source?.label));
  for (const r of instruction.registers || []) {
    const role = shortRole(r.role);
    if (role && !(role === 'dialogue control' && roles.get(r.register) === 'dialogue answer')) assign(r.register, role);
    // A precise meaning can qualify a copy, a comparison or an index whose
    // mechanical role is generic. Do not override an explicit result domain.
    if (!roles.has(r.register) || ['calculated value','caller value'].includes(roles.get(r.register)))
      assign(r.register, shortRole(r.meaning));
  }
  // Some older research rows describe getter results in prose rather than in
  // their register array. This is local evidence from that exact instruction.
  const read = text.match(/\bRead (.+?) (?:into|to)\s*(R[0-7])\b/i);
  if (read && !roles.has(read[2])) assign(read[2], shortRole(read[1].split(/\s+(?:at|from)\s+/)[0]));
  const time = text.match(/\bproducing HHMM in (R[0-7])\b/i);
  if (time) assign(time[1], 'time: HHMM');
  if (op >= 0x60 && op <= 0x67 && arg >= 0x60 && arg <= 0x67) {
    assign(`R${arg & 7}`, 'flag to check');
    assign(`R${op & 7}`, 'flag value: ON/OFF');
  }
  if (op >= 0x70 && op <= 0x77 && arg >= 0x60 && arg <= 0x67) {
    assign(`R${arg & 7}`, 'flag to change');
    assign(`R${op & 7}`, 'flag value: ON/OFF');
  }
  for (const [reg, role] of roles)
    if (role === 'flag number') roles.set(reg, flagUse(instructions, at, Number(reg[1])) || role);
  if ((op >= 0x68 && op <= 0x6f) || (op >= 0x88 && op <= 0x8f))
    assign(`R${op & 7}`, flagUse(instructions, at, op & 7));
  if (native.length && ((op >= 0x68 && op <= 0x6f) || (op >= 0x88 && op <= 0x8f))) {
    const reg = `R${op & 7}`;
    for (let n=at+1; n<instructions.length && n<=at+5; ++n) {
      if (instructions[n].offset !== instructions[n-1].offset+2) break;
      const use = contractRoles(native[n]);
      if (use.input.has(reg)) { assign(reg,use.input.get(reg)); break; }
      const nextOp = instructions[n].word >> 8;
      const immediate = nextOp>=0x68 && nextOp<=0x6f, high = nextOp>=0x88 && nextOp<=0x8f;
      if ((!immediate && !high) || (immediate && (nextOp & 7)===(op & 7))) break;
    }
  }
  // The closure map is authenticated by row bytes before the importer calls
  // this helper. It supplies instruction-scoped roles missing from compact
  // research rows, never a permanent register nickname. Existing precise
  // operand contracts remain preferable to an inferred join alternative.
  for (const [reg, role] of Object.entries(closure?.roles || {})) {
    const prior = roles.get(reg);
    if (!prior || ['saved register value','caller value','calculated value','comparison value','arithmetic operand'].includes(prior))
      assign(reg, role);
  }
  const shown = new Set();
  return text.replace(/\bR[0-7]\b/g, (reg, offset) => {
    // Jump summaries can quote a different instruction. Its registers are not
    // operands of this jump and must not inherit the current frame's labels.
    if (closure?.quoted?.includes(reg)) return reg;
    let role = roles.get(reg);
    if (!closure?.active?.includes(reg) && closure?.referenceRoles?.[reg]) role = closure.referenceRoles[reg];
    // Expanded value expressions quote the lookup that produced a value. The
    // index belongs to that lookup, even when this instruction no longer reads
    // the register (or now uses the same register for its result).
    const referenced = referencedIndexRole(text.slice(0, offset));
    const directIndex = (native[at]?.effects || []).some(e =>
      ['service-read','service-write'].includes(e.type) && (e.indexRegister === reg || e.squadRegister === reg));
    if (referenced && !directIndex) role = referenced;
    if (read && contracts.input.has(reg) && contracts.output.has(reg))
      role = offset === read.index + read[0].length - read[2].length
        ? contracts.output.get(reg) : contracts.input.get(reg);
    // The same register can supply the flag index and then receive its value.
    // Label those two uses separately rather than attaching the output role to
    // an earlier input occurrence in "Read ... at R0 into R0".
    if (op >= 0x60 && op <= 0x67 && arg >= 0x60 && arg <= 0x67 &&
        (op & 7) === (arg & 7) && reg === `R${op & 7}` && read)
      role = offset === read.index + read[0].length - read[2].length
        ? 'flag value: ON/OFF' : 'flag to check';
    if (read && closure?.input?.[reg] && closure?.output?.[reg] && !contracts.input.has(reg) && !contracts.output.has(reg))
      role = offset === read.index + read[0].length - read[2].length
        ? closure.output[reg] : closure.input[reg];
    const usage = `${reg}:${role}`;
    if (shown.has(usage) || !role) return reg;
    shown.add(usage);
    return `${reg} (${role})`;
  });
}
function referencedIndexRole(prefix) {
  const entity = prefix.match(/\b(army|opposing) (character|Soldier)\b[^.;]*?\bat\s*$/i);
  if (entity) return `${entity[1].toLowerCase()} ${entity[2].toLowerCase()} slot at source read`;
  if (/\bsquad (?:saved )?flags at\s*$/i.test(prefix)) return 'squad number at source read';
  if (/\bmember ID in squad\s*$/i.test(prefix)) return 'squad number at source read';
  if (/\b(?:live )?primary identity\s*$/i.test(prefix)) return 'character identity at source read';
  if (/\bcandidate primary identity\s*$/i.test(prefix)) return 'candidate character identity';
  if (/\bcandidate current class in\s*$/i.test(prefix)) return 'candidate current class';
  if (/\bspeech entry[\d.]+ in\s*$/i.test(prefix)) return 'birthday guest speech entry';
  return '';
}
module.exports = {labelRegisters, referencedIndexRole};
