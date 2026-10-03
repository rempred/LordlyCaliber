#!/usr/bin/env node
'use strict';

// Extracts the Director parser model from the accepted byte-exact C source of
// the command-stream dispatcher (OB64 Decomp `src/lib/func_00284288.c`, ROM
// 0x00284288..0x002861C8). The dispatcher walks one opcode word per loop
// iteration and each `case` adds `cursor += k` for its operand words, so the
// static footprint of every command, its query-record class, its director-mode
// guard, and its callees can be read mechanically instead of maintained by hand.
//
// The result is written as `cutscene-director-parser-model.js`. The catalog
// cross-checks the semantic `directorGrammar` against it at load, and the codec
// annotates retail nodes with the mode guards.
//
// Usage: node tools/generate-cutscene-director-parser-model.js [--source <c>]
//        [--output <js>] [--check]

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const editorRoot = path.resolve(__dirname, '..');
const { PARENT } = require(path.join(editorRoot, 'tests', 'project-paths.cjs'));
const defaultSource = path.join(PARENT, 'OB64 Decomp', 'src', 'lib', 'func_00284288.c');
const defaultOutput = path.join(editorRoot, 'cutscene-director-parser-model.js');

const DIRECTOR_MODE_GLOBAL = 'D_8018FC19';
const GUARD_GLOBALS = {
  D_8018FC19: 'director mode',
  D_8018F1A4: 'alternate presentation context pointer',
  D_801CFC70: 'combat presentation lifecycle phase'
};
const COMPARE_HELPER = 'func_002861C8';

function parseArgs(argv) {
  const options = { source: defaultSource, output: defaultOutput, check: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === '--source') options.source = path.resolve(argv[++index]);
    else if (arg === '--output') options.output = path.resolve(argv[++index]);
    else if (arg === '--check') options.check = true;
    else throw new Error('Unknown argument ' + arg);
  }
  return options;
}

function stripComments(text) {
  return text.replace(/\/\*[\s\S]*?\*\//g, '');
}

function matchBrace(text, openIndex) {
  let depth = 0;
  for (let index = openIndex; index < text.length; index += 1) {
    const ch = text[index];
    if (ch === '{') depth += 1;
    else if (ch === '}') {
      depth -= 1;
      if (depth === 0) return index;
    }
  }
  throw new Error('Unbalanced braces after offset ' + openIndex);
}

function parseInt32(literal) {
  return literal.startsWith('0x') || literal.startsWith('0X')
    ? parseInt(literal, 16) : parseInt(literal, 10);
}

// Splits the switch body into lines annotated with the brace depth at the start
// of each line (depth 0 = directly inside the switch braces).
function annotateLines(body) {
  const lines = [];
  let depth = 0;
  body.split('\n').forEach((raw, number) => {
    const line = raw.replace(/\r$/, '');
    lines.push({ number, text: line, depth });
    for (const ch of line) {
      if (ch === '{') depth += 1;
      else if (ch === '}') depth -= 1;
    }
  });
  return lines;
}

function segmentCases(lines) {
  const segments = [];
  const labels = {};
  let current = null;
  lines.forEach((line, index) => {
    const caseMatch = line.depth === 0 && /^\s*case (0x[0-9A-Fa-f]+):\s*$/.exec(line.text);
    if (caseMatch) {
      current = { opcodeU32: parseInt32(caseMatch[1]) >>> 0, start: index, end: index + 1 };
      segments.push(current);
      return;
    }
    const labelMatch = line.depth === 0 && /^\s*([A-Za-z_][A-Za-z0-9_]*):\s*$/.exec(line.text);
    if (labelMatch && labelMatch[1] !== 'default') labels[labelMatch[1]] = index;
    if (current) current.end = index + 1;
  });
  return { segments, labels };
}

function labelBlock(lines, labelIndex) {
  // A join label's block runs until the first `break;` or `return ...;` at any
  // depth after the label. Nested labels are included as plain text.
  const collected = [];
  for (let index = labelIndex + 1; index < lines.length; index += 1) {
    const text = lines[index].text;
    if (/^\s*case 0x[0-9A-Fa-f]+:\s*$/.test(text)) break;
    collected.push(text);
    if (/^\s*(break;|return[^;]*;)\s*$/.test(text)) break;
  }
  return collected.join('\n');
}

function expandGotos(lines, segment, labels) {
  const own = lines.slice(segment.start + 1, segment.end).map(line => line.text);
  let text = own.join('\n');
  const visited = new Set();
  const queue = [];
  const collect = source => {
    const pattern = /goto ([A-Za-z_][A-Za-z0-9_]*);/g;
    let match;
    while ((match = pattern.exec(source))) queue.push(match[1]);
  };
  collect(text);
  while (queue.length) {
    const label = queue.shift();
    if (visited.has(label)) continue;
    visited.add(label);
    const labelIndex = labels[label];
    if (labelIndex === undefined) continue;
    // Labels inside the segment itself are already part of `own`.
    if (labelIndex > segment.start && labelIndex < segment.end) continue;
    const block = labelBlock(lines, labelIndex);
    text += '\n' + block;
    collect(block);
  }
  return { own: own.join('\n'), expanded: text };
}

function lastStatement(ownText) {
  const statements = ownText.split('\n').map(line => line.trim())
    .filter(line => line && line !== '}' && !/^[A-Za-z_][A-Za-z0-9_]*:$/.test(line));
  return statements.length ? statements[statements.length - 1] : '';
}

// Parses one level of `{ ... }` blocks in a statement list. Each block records
// the keyword that opened it (`if`, `else`, `for`, `while`, or `block`), its
// body, and the statement text before it at this level.
function parseBlocks(text) {
  const items = [];
  let depth = 0;
  let level = '';
  let bodyStart = -1;
  let opener = null;
  for (let index = 0; index < text.length; index += 1) {
    const ch = text[index];
    if (ch === '{') {
      if (depth === 0) {
        if (/\belse\s*$/.test(level)) opener = 'else';
        else if (/\bif\s*\((?:[^()]|\([^()]*\))*\)\s*$/.test(level)) opener = 'if';
        else if (/\bfor\s*\((?:[^()]|\([^()]*\))*\)\s*$/.test(level)) opener = 'for';
        else if (/\bwhile\s*\((?:[^()]|\([^()]*\))*\)\s*$/.test(level)) opener = 'while';
        else opener = 'block';
        items.push({ kind: 'text', text: level });
        level = '';
        bodyStart = index + 1;
      }
      depth += 1;
    } else if (ch === '}') {
      depth -= 1;
      if (depth === 0) {
        items.push({ kind: opener, body: text.slice(bodyStart, index) });
        opener = null;
      }
    } else if (depth === 0) {
      level += ch;
    }
  }
  items.push({ kind: 'text', text: level });
  return items;
}

function hasEffects(text) {
  return /\b(func_[0-9A-Fa-f]+|resource_free|memset_[0-9a-f]+)\(|\bD_[0-9A-F]{8}\b\s*=[^=]|command_result =|^\s*\*\([^;]*\)\s*=[^=]/m
    .test(text);
}

function flatIncrements(text) {
  let total = 0;
  const pattern = /cursor \+= (0x[0-9A-Fa-f]+|\d+);|cursor\+\+;/g;
  let match;
  while ((match = pattern.exec(text))) total += match[1] === undefined ? 1 : parseInt32(match[1]);
  return total;
}

// Returns { words, conditional } where `words` is the operand words every path
// through the case adds to the cursor, and `conditional` reports a path that
// adds a different amount.
function cursorIncrements(text) {
  const items = parseBlocks(text);
  let words = 0;
  let conditional = false;
  for (let index = 0; index < items.length; index += 1) {
    const item = items[index];
    if (item.kind === 'text') {
      words += flatIncrements(item.text);
      continue;
    }
    const inner = cursorIncrements(item.body);
    if (inner.conditional) conditional = true;
    if (item.kind === 'block') {
      words += inner.words;
    } else if (item.kind === 'if') {
      const next = items[index + 2];
      const hasElse = items[index + 1] && items[index + 1].kind === 'text' &&
        /^\s*else\s*$/.test(items[index + 1].text) && next && next.kind === 'else';
      if (hasElse) {
        const other = cursorIncrements(next.body);
        if (other.conditional) conditional = true;
        if (other.words === inner.words) words += inner.words;
        else conditional = true;
        index += 2;
      } else if (inner.words) {
        conditional = true;
      }
    } else if (inner.words) {
      conditional = true;
    }
  }
  return { words, conditional };
}

// Splits a case body into its alternative arms: statement runs at brace depth
// zero that end in `break;`, `continue;`, or `return ...;`. A shared-tail label
// after a break starts another arm that a `goto` reaches. Every non-empty arm
// must add the same operand words for the command to have one static width.
function splitArms(text) {
  const arms = [];
  let depth = 0;
  let current = '';
  let statement = '';
  for (let index = 0; index < text.length; index += 1) {
    const ch = text[index];
    if (ch === '{') depth += 1;
    else if (ch === '}') depth -= 1;
    current += ch;
    if (depth === 0) {
      statement += ch;
      if (ch === ';' || ch === '\n') {
        const trimmed = statement.trim();
        statement = '';
        if (depth === 0 && /^(break;|continue;|return[^;]*;)$/.test(trimmed)) {
          arms.push(current);
          current = '';
        }
      }
    }
  }
  if (current.trim()) arms.push(current);
  return arms.filter(arm => arm.split('\n').some(line => {
    const trimmed = line.trim();
    return trimmed && !/^[A-Za-z_][A-Za-z0-9_]*:$/.test(trimmed) &&
      !/^(break;|continue;|return[^;]*;)$/.test(trimmed);
  }));
}

function armIncrements(text) {
  const sums = splitArms(text).map(arm => cursorIncrements(arm));
  if (!sums.length) return { words: 0, conditional: false };
  const conditional = sums.some(sum => sum.conditional) ||
    sums.some(sum => sum.words !== sums[0].words);
  return { words: sums[0].words, conditional };
}

function guards(text) {
  const found = [];
  const pattern = new RegExp(
    'if \\((' + Object.keys(GUARD_GLOBALS).join('|') + ') (==|!=) (-?\\w+)\\)\\s*' +
    '(\\{\\s*(break;|goto [A-Za-z0-9_]+;)\\s*\\}|\\{|break;|goto [A-Za-z0-9_]+;)',
    'g');
  let match;
  while ((match = pattern.exec(text))) {
    const operator = match[2];
    const value = parseInt32(match[3]);
    const tail = match[4];
    let form;
    if (/break;/.test(tail)) form = 'early-break';
    else if (/goto/.test(tail)) form = 'skips-to-label';
    else form = 'wraps-block';
    const effectOperator = form === 'wraps-block' ? operator : (operator === '==' ? '!=' : '==');
    let scope;
    const before = text.slice(0, match.index);
    if (form === 'wraps-block') {
      const open = text.indexOf('{', match.index);
      const close = matchBrace(text, open);
      let rest = text.slice(close + 1);
      let elseBody = null;
      const elseMatch = /^\s*else\s*\{/.exec(rest);
      if (elseMatch) {
        const elseOpen = rest.indexOf('{');
        const elseClose = matchBrace(rest, elseOpen);
        elseBody = rest.slice(elseOpen + 1, elseClose);
        rest = rest.slice(elseClose + 1);
      }
      if (elseBody !== null && hasEffects(elseBody)) scope = 'branch-select';
      else scope = hasEffects(before) || hasEffects(rest) ? 'partial' : 'whole-effect';
    } else if (form === 'early-break') {
      scope = hasEffects(before) ? 'partial' : 'whole-effect';
    } else {
      const label = /goto ([A-Za-z0-9_]+);/.exec(tail)[1];
      const labelPattern = new RegExp('^\\s*' + label + ':\\s*$', 'm');
      const labelMatch = labelPattern.exec(text.slice(match.index));
      const rest = labelMatch ? text.slice(match.index + labelMatch.index) : '';
      scope = hasEffects(before) || hasEffects(rest) ? 'partial' : 'whole-effect';
    }
    found.push({
      global: match[1],
      meaning: GUARD_GLOBALS[match[1]],
      condition: { operator, value },
      form,
      scope,
      effectRequires: { operator: effectOperator, value }
    });
  }
  return found;
}

// The director mode a command's whole effect requires, or null when the
// command has no mode guard, when the guard only selects a branch, or when it
// covers only part of the effect.
function directorModeRequirement(guardList) {
  const mode = guardList.filter(guard => guard.global === DIRECTOR_MODE_GLOBAL &&
    guard.scope === 'whole-effect' && guard.effectRequires.operator === '==');
  if (!mode.length) return null;
  const values = Array.from(new Set(mode.map(guard => guard.effectRequires.value))).sort();
  return values.length === 1 ? values[0] : null;
}

function callees(text) {
  const names = new Set();
  const pattern = /\b(func_[0-9A-Fa-f]+|resource_free|memset_[0-9a-f]+)\(/g;
  let match;
  while ((match = pattern.exec(text))) if (match[1] !== COMPARE_HELPER) names.add(match[1]);
  return Array.from(names).sort();
}

function operandWordsRead(text) {
  const words = new Set();
  const byteOffset = [
    /command_address\)?\s*\+\s*\(?(0x[0-9A-Fa-f]+|\d+)\)?\)/g,
    /args_[0-9a-f]+(?:_\d+)?\)?\s*\+\s*\(?(0x[0-9A-Fa-f]+|\d+)\)?\)/g,
    /\(s32\)stream\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\)/g,
    /stream\)+\s*\+\s*\((0x[0-9A-Fa-f]+|\d+)\)\)/g,
    /stream\s*\+\s*\(cursor \* 4 \+ (0x[0-9A-Fa-f]+|\d+)\)\)/g,
    /value_[0-9a-f]+_?\d*\s*\+\s*\(?(0x[0-9A-Fa-f]+|\d+)\)/g
  ];
  byteOffset.forEach(pattern => {
    let match;
    while ((match = pattern.exec(text))) {
      const offset = parseInt32(match[1]);
      if (offset >= 4 && offset < 64) words.add(Math.floor(offset / 4));
    }
  });
  const index = /\bargs(?:_[0-9a-f]+)?\[(\d+)\]/g;
  let match;
  while ((match = index.exec(text))) {
    const word = parseInt(match[1], 10);
    if (word >= 1 && word < 16) words.add(word);
  }
  return Array.from(words).sort((a, b) => a - b);
}

function buildEntry(lines, segment, labels, next) {
  const { own, expanded } = expandGotos(lines, segment, labels);
  const ownTrimmed = own.trim();
  const last = lastStatement(own);
  const fallsThrough = !ownTrimmed || !/^(break;|return[^;]*;|goto [A-Za-z0-9_]+;)$/.test(last);
  let text = expanded;
  let fallsThroughTo = null;
  if (fallsThrough && next) {
    fallsThroughTo = next.opcodeU32;
    text += '\n' + expandGotos(lines, next, labels).expanded;
  }
  const increments = armIncrements(text);
  const conditional = increments.conditional;
  const executedOperandWords = increments.words;
  const producesQuery = /command_result = /.test(text);
  const scanModes = Array.from(new Set(Array.from(text.matchAll(/(?<![a-z_])scan_mode = (\d);/g))
    .map(match => Number(match[1]))));
  const stateScanMode = /state_scan_mode = 1;/.test(text);
  const returns = /return [^;]*;/.test(own) ||
    (fallsThroughTo === null && /return [^;]*;/.test(text));
  const cursorReplacing = /D_8022A950 = func_8022A428\(args_/.test(own);
  const guardList = guards(text);
  const words = operandWordsRead(text);

  let widthKind;
  let staticWordSpan;
  let queryRecordKind = null;
  if (producesQuery) {
    if (scanModes.length !== 1) {
      throw new Error('Opcode 0x' + segment.opcodeU32.toString(16) + ' sets scan_mode ambiguously.');
    }
    queryRecordKind = scanModes[0] === 1 ? 'Q4' : 'Q3';
    widthKind = 'query';
    staticWordSpan = scanModes[0] === 1 ? 4 : 3;
    if (executedOperandWords !== 0) {
      throw new Error('Opcode 0x' + segment.opcodeU32.toString(16) + ' increments inside a query case.');
    }
  } else if (cursorReplacing) {
    widthKind = 'cursor-replacing';
    staticWordSpan = 1 + (words.length ? Math.max.apply(null, words) : 0);
  } else if (returns && executedOperandWords === 0 && segment.opcodeU32 === 0x80000001) {
    widthKind = 'terminal-with-trailer';
    staticWordSpan = null;
  } else if (returns && executedOperandWords === 0) {
    // Stop, teardown, bridge, and tail-call paths leave the loop without
    // advancing; their static footprint is the opcode plus the operand words
    // they read.
    widthKind = 'returning';
    staticWordSpan = 1 + (words.length ? Math.max.apply(null, words) : 0);
  } else if (conditional) {
    widthKind = 'conditional';
    staticWordSpan = null;
  } else {
    widthKind = 'fixed';
    staticWordSpan = 1 + executedOperandWords;
  }
  if (widthKind === 'fixed' && words.length && Math.max.apply(null, words) > staticWordSpan - 1) {
    throw new Error('Opcode 0x' + segment.opcodeU32.toString(16) +
      ' reads operand word ' + Math.max.apply(null, words) + ' beyond its footprint.');
  }
  return {
    opcodeU32: segment.opcodeU32,
    opcode: '0x' + segment.opcodeU32.toString(16).toUpperCase().padStart(8, '0'),
    widthKind,
    staticWordSpan,
    executedOperandWords,
    queryRecordKind,
    entersResyncMode: stateScanMode,
    returnsFromParser: returns,
    fallsThroughTo,
    operandWordsRead: words,
    guards: guardList,
    directorModeRequirement: directorModeRequirement(guardList),
    callees: callees(text)
  };
}

function buildModel(sourcePath) {
  const raw = fs.readFileSync(sourcePath);
  const text = stripComments(raw.toString('utf8'));
  const switchStart = text.indexOf('switch (command) {');
  if (switchStart < 0) throw new Error('No `switch (command)` in ' + sourcePath);
  const open = text.indexOf('{', switchStart);
  const close = matchBrace(text, open);
  const body = text.slice(open + 1, close);
  const lines = annotateLines(body);
  const { segments, labels } = segmentCases(lines);
  const entries = segments.map((segment, index) =>
    buildEntry(lines, segment, labels, segments[index + 1] || null));
  entries.sort((a, b) => a.opcodeU32 - b.opcodeU32);
  const seen = new Set();
  entries.forEach(entry => {
    if (seen.has(entry.opcodeU32)) throw new Error('Duplicate case ' + entry.opcode);
    seen.add(entry.opcodeU32);
  });
  if (!/if \(command_result != -0x64\) \{\s*cursor = func_002861C8\(/.test(text)) {
    throw new Error('The query compare helper call after the switch was not found.');
  }
  return {
    schemaVersion: 1,
    source: {
      path: 'OB64 Decomp/src/lib/func_00284288.c',
      romRange: '0x00284288..0x002861C8',
      byteLength: raw.length,
      sha256: crypto.createHash('sha256').update(raw).digest('hex').toUpperCase()
    },
    parser: {
      opcodeWordsPerIteration: 1,
      queryCompareHelper: COMPARE_HELPER,
      queryRecordWords: { Q3: 3, Q4: 4 },
      directorModeGlobal: DIRECTOR_MODE_GLOBAL,
      cursorReplaceHelper: 'func_8022A428'
    },
    counts: {
      entries: entries.length,
      ordinary: entries.filter(entry => entry.opcodeU32 < 0x80000000).length,
      extended: entries.filter(entry => entry.opcodeU32 >= 0x80000000).length,
      queries: entries.filter(entry => entry.widthKind === 'query').length,
      directorModeGuarded: entries.filter(entry => entry.directorModeRequirement !== null).length
    },
    entries
  };
}

function render(model) {
  return '// Generated by tools/generate-cutscene-director-parser-model.js from the\n' +
    '// accepted byte-exact C dispatcher. Do not edit by hand.\n' +
    'window.OB64 = window.OB64 || {};\n' +
    'window.OB64.cutsceneDirectorParserModel = ' + JSON.stringify(model, null, 2) + ';\n';
}

function main() {
  const options = parseArgs(process.argv.slice(2));
  if (!fs.existsSync(options.source)) {
    throw new Error('Accepted dispatcher source not found: ' + options.source);
  }
  const model = buildModel(options.source);
  const output = render(model);
  if (options.check) {
    const existing = fs.existsSync(options.output) ? fs.readFileSync(options.output, 'utf8') : null;
    if (existing !== output) {
      console.error('cutscene-director-parser-model.js is stale for ' + options.source);
      process.exit(1);
    }
    console.log('cutscene-director-parser-model.js matches ' + options.source);
    return;
  }
  fs.writeFileSync(options.output, output);
  console.log('Wrote ' + options.output + ': ' + model.counts.entries + ' opcodes (' +
    model.counts.ordinary + ' ordinary, ' + model.counts.extended + ' extended, ' +
    model.counts.queries + ' queries, ' + model.counts.directorModeGuarded +
    ' director-mode guarded).');
}

if (require.main === module) main();

module.exports = { buildModel, render };
