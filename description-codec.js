// Lossless editor codec for item, consumable, class, and combat-action text.
// The game stores each group as one fixed-capacity LZSS block in the ROM gap.

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var CRLF = new Uint8Array([0x0D, 0x0A]);
  var LINE_BREAK = new Uint8Array([0x81, 0x97]);
  var REFERENCE_PREFIX = new Uint8Array([0x81, 0x96]);
  var FORMAT_STYLE = 0x30;

  var SPECS = {
    items: {
      kind: 'items', property: 'itemDescriptions', dirty: 'itemDescriptions',
      label: 'Item descriptions', recordLabel: 'Item description',
      gapOffset: 0x34FA4E, capacity: 9702, slotSize: 9710,
      recordCount: 278, layout: 'paged', opaquePairs: ['876A']
    },
    consumables: {
      kind: 'consumables', property: 'consumableDescriptions', dirty: 'consumableDescriptions',
      label: 'Consumable descriptions', recordLabel: 'Consumable description',
      gapOffset: 0x352B88, capacity: 2055, slotSize: 2064,
      recordCount: 50, layout: 'paged', opaquePairs: ['876A']
    },
    classes: {
      kind: 'classes', property: 'classDescriptions', dirty: 'classDescriptions',
      label: 'Class descriptions', recordLabel: 'Class description',
      gapOffset: 0x353F92, capacity: 6399, slotSize: 6408,
      recordCount: 225, layout: 'plain', opaquePairs: ['876A'],
      referenceMarker: 'HELP', decodeShiftJis: true
    },
    actions: {
      kind: 'actions', property: 'actionDescriptions', dirty: 'actionDescriptions',
      label: 'Action descriptions', recordLabel: 'Action description',
      gapOffset: 0x356180, capacity: 3145, slotSize: 3154,
      recordCount: 159, layout: 'paged', opaquePairs: ['876A'],
      referenceMarker: 'REF'
    }
  };

  function own(object, key) {
    return !!object && Object.prototype.hasOwnProperty.call(object, key);
  }

  function hexByte(value) {
    return (value & 0xFF).toString(16).toUpperCase().padStart(2, '0');
  }

  function bytesHex(bytes) {
    var out = '';
    for (var i = 0; i < bytes.length; i++) out += hexByte(bytes[i]);
    return out;
  }

  function hexBytes(value) {
    if (!/^(?:[0-9A-Fa-f]{2})+$/.test(value || '')) {
      throw new Error('Invalid hexadecimal byte string.');
    }
    var out = new Uint8Array(value.length / 2);
    for (var i = 0; i < out.length; i++) {
      out[i] = parseInt(value.slice(i * 2, i * 2 + 2), 16);
    }
    return out;
  }

  function concatBytes(parts) {
    var length = 0;
    for (var i = 0; i < parts.length; i++) length += parts[i].length;
    var out = new Uint8Array(length);
    var offset = 0;
    for (var j = 0; j < parts.length; j++) {
      out.set(parts[j], offset);
      offset += parts[j].length;
    }
    return out;
  }

  function equalBytes(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function asciiBytes(value, label) {
    if (typeof value !== 'string') throw new TypeError(label + ' text must be a string.');
    var out = new Uint8Array(value.length);
    for (var i = 0; i < value.length; i++) {
      var code = value.charCodeAt(i);
      if (code < 0x20 || code > 0x7E) {
        throw new Error(label + ' contains unsupported character U+' +
          code.toString(16).toUpperCase().padStart(4, '0') + '.');
      }
      out[i] = code;
    }
    return out;
  }

  function isShiftJisLead(value) {
    return (value >= 0x81 && value <= 0x9F) || (value >= 0xE0 && value <= 0xEF);
  }

  function isShiftJisTrail(value) {
    return value >= 0x40 && value <= 0xFC && value !== 0x7F;
  }

  function validReference(spec, value) {
    if (spec.kind === 'actions') return value === '10';
    if (spec.kind === 'classes') {
      var numeric = Number(value);
      return /^28\d{2}$/.test(value) && numeric >= 2800 && numeric <= 2822;
    }
    return false;
  }

  function appendText(tokens, value) {
    var previous = tokens[tokens.length - 1];
    if (previous && previous.type === 'text') previous.value += value;
    else tokens.push({ type: 'text', value: value });
  }

  function appendEncodedText(tokens, bytes, value) {
    var previous = tokens[tokens.length - 1];
    if (previous && previous.type === 'encodedText') {
      previous.bytes += bytes;
      previous.value += value;
    } else {
      tokens.push({ type: 'encodedText', encoding: 'shift_jis', bytes: bytes, value: value });
    }
  }

  function parseRecord(input, id, spec) {
    var raw = input.slice();
    var tokens = [];
    var opaque = {};
    for (var oi = 0; oi < spec.opaquePairs.length; oi++) opaque[spec.opaquePairs[oi]] = true;
    var decoder = spec.decodeShiftJis && typeof TextDecoder !== 'undefined'
      ? new TextDecoder('shift_jis') : null;

    for (var offset = 0; offset < raw.length; offset++) {
      var value = raw[offset];
      if (value >= 0x20 && value <= 0x7E) {
        appendText(tokens, String.fromCharCode(value));
      } else if (value === 0x0E) {
        tokens.push({ type: 'formatStart' });
      } else if (value === 0x0F) {
        tokens.push({ type: 'formatEnd' });
      } else if (value === 0x10) {
        if (offset + 1 >= raw.length) {
          throw new Error(spec.recordLabel + ' ' + id + ' ends with an incomplete format token.');
        }
        tokens.push({ type: 'formattedLineBreak', style: raw[++offset] });
      } else if (value === 0x81 && raw[offset + 1] === 0x97) {
        tokens.push({ type: 'lineBreak' });
        offset++;
      } else if (value === 0x81 && raw[offset + 1] === 0x96 && spec.referenceMarker) {
        var end = offset + 2;
        while (end < raw.length && raw[end] >= 0x30 && raw[end] <= 0x39) end++;
        var reference = '';
        for (var ri = offset + 2; ri < end; ri++) reference += String.fromCharCode(raw[ri]);
        if (reference && validReference(spec, reference)) {
          tokens.push({ type: 'reference', marker: spec.referenceMarker, value: reference });
          offset = end - 1;
        } else {
          tokens.push({ type: 'byte', value: value });
        }
      } else if (offset + 1 < raw.length && opaque[hexByte(value) + hexByte(raw[offset + 1])]) {
        tokens.push({ type: 'control', bytes: hexByte(value) + hexByte(raw[offset + 1]) });
        offset++;
      } else if (decoder && isShiftJisLead(value) && offset + 1 < raw.length &&
          isShiftJisTrail(raw[offset + 1])) {
        var encoded = raw.subarray(offset, offset + 2);
        appendEncodedText(tokens, bytesHex(encoded), decoder.decode(encoded));
        offset++;
      } else {
        tokens.push({ type: 'byte', value: value });
      }
    }

    var record = { id: id, raw: raw, rawHex: bytesHex(raw), tokens: tokens };
    record.text = renderRecord(record, false);
    record.displayText = renderRecord(record, true);
    record.editableText = record.text;
    return record;
  }

  function renderRecord(record, unicodeShiftJis) {
    var rendered = '';
    for (var i = 0; i < record.tokens.length; i++) {
      var token = record.tokens[i];
      if (token.type === 'text') rendered += token.value;
      else if (token.type === 'encodedText') {
        rendered += unicodeShiftJis ? token.value : '{SJIS:' + token.bytes.toUpperCase() + '}';
      } else if (token.type === 'lineBreak' || token.type === 'formattedLineBreak') {
        rendered += '\n';
      } else if (token.type === 'reference') {
        rendered += '{' + (token.marker || 'REF') + ':' + token.value + '}';
      } else if (token.type === 'control') {
        rendered += '{CTRL:' + token.bytes.toUpperCase() + '}';
      } else if (token.type === 'byte') {
        rendered += '{BYTE:' + hexByte(token.value) + '}';
      }
    }
    var lines = rendered.split('\n').map(function(line) { return line.replace(/ +$/u, ''); });
    while (lines.length > 1 && lines[lines.length - 1] === '') lines.pop();
    return lines.join('\n');
  }

  function serializeTokens(record, spec) {
    var parts = [];
    for (var i = 0; i < record.tokens.length; i++) {
      var token = record.tokens[i];
      if (token.type === 'text') parts.push(asciiBytes(token.value, spec.recordLabel));
      else if (token.type === 'encodedText') parts.push(hexBytes(token.bytes));
      else if (token.type === 'formatStart') parts.push(new Uint8Array([0x0E]));
      else if (token.type === 'formatEnd') parts.push(new Uint8Array([0x0F]));
      else if (token.type === 'formattedLineBreak') {
        parts.push(new Uint8Array([0x10, token.style & 0xFF]));
      } else if (token.type === 'lineBreak') parts.push(LINE_BREAK);
      else if (token.type === 'reference') {
        if (!validReference(spec, token.value)) {
          throw new Error(spec.recordLabel + ' ' + record.id + ' has an invalid reference.');
        }
        parts.push(REFERENCE_PREFIX, asciiBytes(token.value, spec.recordLabel));
      } else if (token.type === 'control') parts.push(hexBytes(token.bytes));
      else if (token.type === 'byte') parts.push(new Uint8Array([token.value & 0xFF]));
      else throw new Error(spec.recordLabel + ' ' + record.id + ' has an unknown token.');
    }
    return concatBytes(parts);
  }

  function normalizeLines(text, label, maxLines) {
    if (typeof text !== 'string') throw new TypeError(label + ' text must be a string.');
    if (/\r(?!\n)/u.test(text)) throw new Error(label + ' contains a lone carriage return.');
    var lines = text.replace(/\r\n/gu, '\n').split('\n');
    if (lines.length > maxLines) {
      throw new Error(label + ' supports at most ' + maxLines + ' display lines.');
    }
    return lines;
  }

  function encodeSegment(segment, spec) {
    var opaque = {};
    for (var oi = 0; oi < spec.opaquePairs.length; oi++) opaque[spec.opaquePairs[oi]] = true;
    var parts = [];
    var marker = /\{(CTRL|BYTE|SJIS|REF|HELP):([0-9A-Fa-f]+)\}/gu;
    var cursor = 0;
    var match;

    function appendAscii(value) {
      if (/\{(?:CTRL|BYTE|SJIS|REF|HELP):/iu.test(value)) {
        throw new Error('Malformed ' + spec.recordLabel.toLowerCase() + ' byte marker.');
      }
      if (value) parts.push(asciiBytes(value, spec.recordLabel));
    }

    while ((match = marker.exec(segment)) !== null) {
      appendAscii(segment.slice(cursor, match.index));
      var kind = match[1].toUpperCase();
      var value = match[2].toUpperCase();
      if (kind === 'CTRL') {
        if (!/^[0-9A-F]{4}$/.test(value) || !opaque[value]) {
          throw new Error('Unsupported control marker ' + match[0] + '.');
        }
        parts.push(hexBytes(value));
      } else if (kind === 'BYTE') {
        if (!/^[0-9A-F]{2}$/.test(value)) throw new Error('Invalid byte marker ' + match[0] + '.');
        parts.push(hexBytes(value));
      } else if (kind === 'SJIS') {
        if (!spec.decodeShiftJis || !/^(?:[0-9A-F]{2})+$/.test(value)) {
          throw new Error('Unsupported Shift-JIS marker ' + match[0] + '.');
        }
        parts.push(hexBytes(value));
      } else {
        if (!spec.referenceMarker || kind !== spec.referenceMarker ||
            !validReference(spec, match[2])) {
          throw new Error('Unsupported reference marker ' + match[0] + '.');
        }
        parts.push(REFERENCE_PREFIX, asciiBytes(match[2], spec.recordLabel));
      }
      cursor = marker.lastIndex;
    }
    appendAscii(segment.slice(cursor));
    return concatBytes(parts);
  }

  function assertNoCrlf(encoded, label) {
    for (var i = 0; i + 1 < encoded.length; i++) {
      if (encoded[i] === 0x0D && encoded[i + 1] === 0x0A) {
        throw new Error(label + ' encoded an embedded record delimiter.');
      }
    }
    return encoded;
  }

  function encodePlainText(text, spec, maxLines) {
    var lines = normalizeLines(text, spec.recordLabel, maxLines == null ? 4 : maxLines);
    var parts = [];
    for (var i = 0; i < lines.length; i++) {
      if (i) parts.push(LINE_BREAK);
      parts.push(encodeSegment(lines[i], spec));
    }
    return assertNoCrlf(concatBytes(parts), spec.recordLabel);
  }

  function encodePagedText(text, spec) {
    var lines = normalizeLines(text, spec.recordLabel, 4);
    var encoded = lines.map(function(line) { return encodeSegment(line, spec); });
    var parts;
    if (encoded.length === 1) {
      parts = [encoded[0]];
    } else if (encoded.length === 2) {
      parts = [encoded[0], LINE_BREAK, encoded[1]];
    } else {
      var fourth = encoded.length === 4 && encoded[3].length
        ? encoded[3] : asciiBytes(' ', spec.recordLabel);
      parts = [
        new Uint8Array([0x0E]), encoded[0], new Uint8Array([0x10, FORMAT_STYLE]),
        encoded[1], new Uint8Array([0x0F]), LINE_BREAK,
        new Uint8Array([0x0E]), encoded[2], new Uint8Array([0x10, FORMAT_STYLE]),
        fourth, new Uint8Array([0x0F])
      ];
    }
    return assertNoCrlf(concatBytes(parts), spec.recordLabel);
  }

  function findDelimiter(bytes, start) {
    for (var i = start; i + 1 < bytes.length; i++) {
      if (bytes[i] === CRLF[0] && bytes[i + 1] === CRLF[1]) return i;
    }
    return -1;
  }

  function classifyClasses(records) {
    for (var i = 0; i < records.length; i++) {
      var record = records[i];
      record.messageId = 2600 + i;
      record.kind = 'reserved';
      record.editableText = record.text;
      delete record.referenceSuffixTokens;
      delete record.structuralPrefixTokens;
      if (i === 0) record.kind = 'sentinel';
      else if (i >= 1 && i <= 0xA4) {
        record.kind = 'class';
        record.classId = i;
        var referenceIndex = -1;
        for (var ti = 0; ti < record.tokens.length; ti++) {
          if (record.tokens[ti].type === 'reference') { referenceIndex = ti; break; }
        }
        if (referenceIndex >= 0) {
          var start = referenceIndex > 0 && record.tokens[referenceIndex - 1].type === 'lineBreak'
            ? referenceIndex - 1 : referenceIndex;
          record.helpReference = Number(record.tokens[referenceIndex].value);
          record.referenceSuffixTokens = record.tokens.slice(start);
          record.editableText = renderRecord({ tokens: record.tokens.slice(0, start) }, false);
        }
      } else if (i >= 200 && i <= 222) {
        record.kind = 'classChange';
        if (record.tokens[0] && record.tokens[0].type === 'lineBreak') {
          record.structuralPrefixTokens = record.tokens.slice(0, 1);
          record.editableText = renderRecord({ tokens: record.tokens.slice(1) }, false);
        }
      } else if (i === 224) record.kind = 'special';
    }
  }

  function classifyActions(records) {
    for (var i = 0; i < records.length; i++) {
      records[i].editableText = renderRecord(records[i], false)
        .replace(/\{REF:10\}/gu, 'Physical');
    }
  }

  function parseDecompressed(kind, bytes) {
    var spec = SPECS[kind];
    if (!spec) throw new Error('Unknown description block ' + kind + '.');
    var records = [];
    var cursor = 0;
    while (cursor < bytes.length) {
      var delimiter = findDelimiter(bytes, cursor);
      if (delimiter < 0) {
        throw new Error(spec.label + ' contain unterminated trailing bytes.');
      }
      records.push(parseRecord(bytes.subarray(cursor, delimiter), records.length, spec));
      cursor = delimiter + 2;
    }
    if (records.length !== spec.recordCount) {
      throw new Error(spec.label + ' contain ' + records.length +
        ' records; expected ' + spec.recordCount + '.');
    }
    if (kind === 'classes') classifyClasses(records);
    if (kind === 'actions') classifyActions(records);
    return records;
  }

  function serializeRecords(block) {
    var spec = SPECS[block.kind];
    if (!spec || !Array.isArray(block.records) || block.records.length !== spec.recordCount) {
      throw new Error('Invalid ' + (spec ? spec.label.toLowerCase() : 'description block') + '.');
    }
    var parts = [];
    for (var i = 0; i < block.records.length; i++) {
      if (block.records[i].id !== i) {
        throw new Error(spec.recordLabel + ' ' + i + ' carries the wrong record ID.');
      }
      parts.push(serializeTokens(block.records[i], spec), CRLF);
    }
    return concatBytes(parts);
  }

  function actionLayout(record) {
    for (var i = 0; i < record.tokens.length; i++) {
      var type = record.tokens[i].type;
      if (type === 'formatStart' || type === 'formatEnd' || type === 'formattedLineBreak') return 'paged';
    }
    var breaks = record.tokens.filter(function(token) { return token.type === 'lineBreak'; }).length;
    return breaks >= 2 ? 'plain' : 'paged';
  }

  function replaceText(block, id, text) {
    var spec = SPECS[block.kind];
    if (!Number.isInteger(id) || id < 0 || id >= block.records.length) {
      throw new RangeError(spec.recordLabel + ' ID ' + id + ' is outside the block.');
    }
    var original = block.records[id];
    var encoded;
    if (block.kind === 'classes') {
      var preservePrefix = original.structuralPrefixTokens && text.charAt(0) !== '\n';
      var preserveSuffix = original.referenceSuffixTokens && text.indexOf('{HELP:') === -1;
      var prefixTokens = preservePrefix ? original.structuralPrefixTokens : [];
      var suffixTokens = preserveSuffix ? original.referenceSuffixTokens : [];
      var structuralLines = prefixTokens.concat(suffixTokens).filter(function(token) {
        return token.type === 'lineBreak';
      }).length;
      var classParts = [];
      if (prefixTokens.length) classParts.push(serializeTokens({ id: id, tokens: prefixTokens }, spec));
      classParts.push(encodePlainText(text, spec, 4 - structuralLines));
      if (suffixTokens.length) classParts.push(serializeTokens({ id: id, tokens: suffixTokens }, spec));
      encoded = concatBytes(classParts);
    } else if (block.kind === 'actions') {
      var sourceText = text;
      var hadPhysicalReference = original.tokens.some(function(token) {
        return token.type === 'reference' && token.value === '10';
      });
      if (hadPhysicalReference && sourceText.indexOf('{REF:10}') === -1) {
        if (sourceText.indexOf('Type: Physical.') === -1) {
          throw new Error('This action description must preserve its Physical type.');
        }
        sourceText = sourceText.replace('Type: Physical.', 'Type: {REF:10}.');
      }
      encoded = actionLayout(original) === 'plain'
        ? encodePlainText(sourceText, spec, 4)
        : encodePagedText(sourceText, spec);
    } else {
      encoded = encodePagedText(text, spec);
    }

    var records = block.records.slice();
    records[id] = parseRecord(encoded, id, spec);
    if (block.kind === 'classes') classifyClasses(records);
    if (block.kind === 'actions') classifyActions(records);
    return { kind: block.kind, records: records, meta: Object.assign({}, block.meta) };
  }

  // Dynamic-programming encoder for the game's existing LZSS token format.
  // It minimizes byte length and uses stable tie-breaking for repeatable ROMs.
  var TOKEN_LITERAL = 1;
  var TOKEN_ZERO_SHORT = 2;
  var TOKEN_FF_FILL = 3;
  var TOKEN_ZERO_LONG = 4;
  var TOKEN_BACKREF_SHORT = 5;
  var TOKEN_BACKREF_MEDIUM = 6;
  var TOKEN_BACKREF_LONG = 7;

  function buildBackReferences(data) {
    var size = data.length;
    var shortLength = new Uint16Array(size);
    var mediumLength = new Uint16Array(size);
    var longLength = new Uint16Array(size);
    var shortDistance = new Uint32Array(size);
    var mediumDistance = new Uint32Array(size);
    var longDistance = new Uint32Array(size);
    var previous = new Int32Array(size);
    previous.fill(-1);
    var heads = new Map();

    function record(lengths, distances, position, length, distance, maximum) {
      var encodable = Math.min(length, maximum);
      if (encodable > lengths[position] ||
          (encodable === lengths[position] &&
           (!distances[position] || distance < distances[position]))) {
        lengths[position] = encodable;
        distances[position] = distance;
      }
    }

    for (var position = 0; position + 2 < size; position++) {
      var prefix = (data[position] << 16) |
        (data[position + 1] << 8) | data[position + 2];
      var candidate = heads.has(prefix) ? heads.get(prefix) : -1;
      previous[position] = candidate;
      heads.set(prefix, position);
      var maximumLength = Math.min(260, size - position);

      while (candidate >= 0) {
        var distance = position - candidate;
        if (distance > 0x10000) break;
        var length = 3;
        while (length < maximumLength &&
            data[position + length] === data[position + length - distance]) {
          length++;
        }
        if (distance <= 2048) {
          record(shortLength, shortDistance, position, length, distance, 18);
        }
        if (distance <= 16384 && length >= 4) {
          record(mediumLength, mediumDistance, position, length, distance, 67);
        }
        if (length >= 5) {
          record(longLength, longDistance, position, length, distance, 260);
        }
        var shortComplete = shortLength[position] === Math.min(18, maximumLength);
        var mediumComplete = maximumLength < 4 ||
          mediumLength[position] === Math.min(67, maximumLength);
        var longComplete = maximumLength < 5 || longLength[position] === maximumLength;
        if (shortComplete && mediumComplete && longComplete) break;
        candidate = previous[candidate];
      }
    }
    return {
      shortLength: shortLength, mediumLength: mediumLength, longLength: longLength,
      shortDistance: shortDistance, mediumDistance: mediumDistance, longDistance: longDistance
    };
  }

  function buildFillRuns(data) {
    var zeroRun = new Uint16Array(data.length + 1);
    var ffRun = new Uint16Array(data.length + 1);
    for (var position = data.length - 1; position >= 0; position--) {
      if (data[position] === 0) zeroRun[position] = Math.min(258, zeroRun[position + 1] + 1);
      if (data[position] === 0xFF) ffRun[position] = Math.min(258, ffRun[position + 1] + 1);
    }
    return { zeroRun: zeroRun, ffRun: ffRun };
  }

  function lzssCompressOptimal(input) {
    var size = input.length;
    if (!size) return new Uint8Array(0);
    var matches = buildBackReferences(input);
    var fills = buildFillRuns(input);
    var byteCost = new Uint32Array(size + 1);
    var commandCost = new Uint32Array(size + 1);
    var choiceToken = new Uint8Array(size);
    var choiceLength = new Uint16Array(size);
    var choiceDistance = new Uint32Array(size);

    for (var position = size - 1; position >= 0; position--) {
      var bestBytes = Number.MAX_SAFE_INTEGER;
      var bestCommands = Number.MAX_SAFE_INTEGER;
      var bestToken = 0;
      var bestLength = 0;
      var bestDistance = 0;
      var bestPriority = Number.MAX_SAFE_INTEGER;

      function consider(token, length, distance, tokenBytes, priority) {
        var totalBytes = tokenBytes + byteCost[position + length];
        var totalCommands = 1 + commandCost[position + length];
        var better = totalBytes < bestBytes ||
          (totalBytes === bestBytes && totalCommands < bestCommands) ||
          (totalBytes === bestBytes && totalCommands === bestCommands && priority < bestPriority) ||
          (totalBytes === bestBytes && totalCommands === bestCommands &&
           priority === bestPriority && length > bestLength) ||
          (totalBytes === bestBytes && totalCommands === bestCommands &&
           priority === bestPriority && length === bestLength && distance < bestDistance);
        if (!better) return;
        bestBytes = totalBytes;
        bestCommands = totalCommands;
        bestToken = token;
        bestLength = length;
        bestDistance = distance;
        bestPriority = priority;
      }

      for (var literalLength = 1;
          literalLength <= 64 && position + literalLength <= size;
          literalLength++) {
        consider(TOKEN_LITERAL, literalLength, 0, literalLength + 1, 6);
      }
      for (var zeroShort = 2;
          zeroShort <= Math.min(33, fills.zeroRun[position]); zeroShort++) {
        consider(TOKEN_ZERO_SHORT, zeroShort, 0, 1, 0);
      }
      for (var ffLength = 3; ffLength <= fills.ffRun[position]; ffLength++) {
        consider(TOKEN_FF_FILL, ffLength, 0, 2, 1);
      }
      for (var zeroLong = 3; zeroLong <= fills.zeroRun[position]; zeroLong++) {
        consider(TOKEN_ZERO_LONG, zeroLong, 0, 2, 2);
      }
      for (var shortMatch = 3;
          shortMatch <= matches.shortLength[position]; shortMatch++) {
        consider(TOKEN_BACKREF_SHORT, shortMatch,
          matches.shortDistance[position], 2, 3);
      }
      for (var mediumMatch = 4;
          mediumMatch <= matches.mediumLength[position]; mediumMatch++) {
        consider(TOKEN_BACKREF_MEDIUM, mediumMatch,
          matches.mediumDistance[position], 3, 4);
      }
      for (var longMatch = 5;
          longMatch <= matches.longLength[position]; longMatch++) {
        consider(TOKEN_BACKREF_LONG, longMatch,
          matches.longDistance[position], 4, 5);
      }
      if (!bestToken || !bestLength) {
        throw new Error('LZSS planner could not encode input byte ' + position + '.');
      }
      byteCost[position] = bestBytes;
      commandCost[position] = bestCommands;
      choiceToken[position] = bestToken;
      choiceLength[position] = bestLength;
      choiceDistance[position] = bestDistance;
    }

    var output = new Uint8Array(byteCost[0]);
    var inputPosition = 0;
    var outputPosition = 0;
    while (inputPosition < size) {
      var token = choiceToken[inputPosition];
      var length = choiceLength[inputPosition];
      var distance = choiceDistance[inputPosition];
      if (token === TOKEN_LITERAL) {
        output[outputPosition++] = 0x40 | (length - 1);
        output.set(input.subarray(inputPosition, inputPosition + length), outputPosition);
        outputPosition += length;
      } else if (token === TOKEN_ZERO_SHORT) {
        output[outputPosition++] = 0x20 | (length - 2);
      } else if (token === TOKEN_FF_FILL) {
        output[outputPosition++] = 0x01;
        output[outputPosition++] = length - 3;
      } else if (token === TOKEN_ZERO_LONG) {
        output[outputPosition++] = 0x02;
        output[outputPosition++] = length - 3;
      } else if (token === TOKEN_BACKREF_SHORT) {
        var shortDistance = distance - 1;
        output[outputPosition++] = 0x80 | ((length - 3) << 3) |
          ((shortDistance >>> 8) & 0x07);
        output[outputPosition++] = shortDistance & 0xFF;
      } else if (token === TOKEN_BACKREF_MEDIUM) {
        var mediumLength = length - 4;
        var mediumDistance = distance - 1;
        output[outputPosition++] = 0x10 | (mediumLength & 0x0F);
        output[outputPosition++] = ((mediumLength & 0x30) << 2) |
          ((mediumDistance >>> 8) & 0x3F);
        output[outputPosition++] = mediumDistance & 0xFF;
      } else if (token === TOKEN_BACKREF_LONG) {
        var longDistance = distance - 1;
        output[outputPosition++] = 0x00;
        output[outputPosition++] = length - 5;
        output[outputPosition++] = (longDistance >>> 8) & 0xFF;
        output[outputPosition++] = longDistance & 0xFF;
      }
      inputPosition += length;
    }
    if (outputPosition !== output.length) {
      throw new Error('LZSS planner output length does not match its plan.');
    }
    return output;
  }

  function measureBlock(block) {
    var spec = SPECS[block.kind];
    var decompressed = serializeRecords(block);
    var compressed = lzssCompressOptimal(decompressed);
    block.meta = Object.assign({}, block.meta, {
      decompSize: decompressed.length,
      compressedSize: compressed.length,
      compressedCapacity: spec.capacity
    });
    return {
      block: block,
      decompressed: decompressed,
      compressed: compressed,
      compressedSize: compressed.length,
      compressedCapacity: spec.capacity,
      remaining: spec.capacity - compressed.length,
      fits: compressed.length <= spec.capacity
    };
  }

  function prepareBlock(block) {
    var spec = SPECS[block.kind];
    var measured = measureBlock(block);
    if (!measured.fits) {
      throw new Error(spec.label + ' need ' + measured.compressedSize +
        ' compressed bytes, but their ROM slot holds ' + spec.capacity + '.');
    }
    var roundTrip = OB64.lzssDecode(
      measured.compressed,
      0,
      measured.decompressed.length
    ).output;
    if (!equalBytes(roundTrip, measured.decompressed)) {
      throw new Error(spec.label + ' failed compression round-trip verification.');
    }
    return measured;
  }

  function parseBlock(kind, z64) {
    var spec = SPECS[kind];
    var headerOffset = OB64.LZSS_GAP_START + spec.gapOffset;
    if (headerOffset < 0 || headerOffset + spec.slotSize > z64.length) {
      throw new Error(spec.label + ' slot lies outside this ROM image.');
    }
    var payloadSize = OB64.readU32BE(z64, headerOffset);
    var decompSize = OB64.readU32BE(z64, headerOffset + 4);
    var encodedCapacity = payloadSize - 4;
    if (payloadSize < 4 || encodedCapacity > spec.capacity) {
      throw new Error(spec.label + ' header exceeds its fixed ROM slot.');
    }
    var decoded = OB64.lzssDecode(z64, headerOffset + 8, decompSize);
    if (decoded.output.length !== decompSize) {
      throw new Error(spec.label + ' decoded ' + decoded.output.length +
        ' bytes; expected ' + decompSize + '.');
    }
    return {
      kind: kind,
      records: parseDecompressed(kind, decoded.output),
      meta: {
        headerOffset: headerOffset,
        gapOffset: spec.gapOffset,
        payloadSize: payloadSize,
        decompSize: decompSize,
        compressedSize: decoded.bytesConsumed,
        compressedCapacity: spec.capacity,
        slotSize: spec.slotSize
      }
    };
  }

  function serializeBlock(block, z64) {
    if (!(z64 instanceof Uint8Array)) throw new TypeError('Description export requires a ROM byte array.');
    var spec = SPECS[block.kind];
    var prepared = prepareBlock(block);
    var headerOffset = block.meta.headerOffset != null
      ? block.meta.headerOffset : OB64.LZSS_GAP_START + spec.gapOffset;
    if (headerOffset < 0 || headerOffset + spec.slotSize > z64.length) {
      throw new Error(spec.label + ' slot lies outside the export image.');
    }
    OB64.writeU32BE(z64, headerOffset, spec.capacity + 4);
    OB64.writeU32BE(z64, headerOffset + 4, prepared.decompressed.length);
    z64.fill(0x03, headerOffset + 8, headerOffset + 8 + spec.capacity);
    z64.set(prepared.compressed, headerOffset + 8);
    var verification = OB64.lzssDecode(z64, headerOffset + 8, prepared.decompressed.length);
    if (!equalBytes(verification.output, prepared.decompressed)) {
      throw new Error(spec.label + ' did not read back from the finished slot.');
    }
    return {
      headerOffset: headerOffset,
      slotSize: spec.slotSize,
      decompSize: prepared.decompressed.length,
      compressedSize: prepared.compressed.length,
      compressedCapacity: spec.capacity
    };
  }

  function withText(block, id, text) {
    var updated = replaceText(block, id, text);
    prepareBlock(updated);
    return updated;
  }

  function measureText(block, id, text) {
    return measureBlock(replaceText(block, id, text));
  }

  function parseAll(z64) {
    return {
      items: parseBlock('items', z64),
      consumables: parseBlock('consumables', z64),
      classes: parseBlock('classes', z64),
      actions: parseBlock('actions', z64)
    };
  }

  function snapshotAll(rom) {
    var snapshot = {};
    Object.keys(SPECS).forEach(function(kind) {
      var block = rom[SPECS[kind].property];
      snapshot[kind] = block ? block.records.map(function(record) {
        return record.editableText;
      }) : [];
    });
    return snapshot;
  }

  function hasTextChanges(block, originalTexts) {
    if (!block || !Array.isArray(originalTexts) ||
        originalTexts.length !== block.records.length) return false;
    for (var i = 0; i < block.records.length; i++) {
      if (block.records[i].editableText !== originalTexts[i]) return true;
    }
    return false;
  }

  function collectProjectPayload(rom, originalDescriptions) {
    var payload = {};
    Object.keys(SPECS).forEach(function(kind) {
      var spec = SPECS[kind];
      var block = rom[spec.property];
      var baseline = originalDescriptions && originalDescriptions[kind] || [];
      var changes = {};
      if (block) {
        for (var i = 0; i < block.records.length; i++) {
          if (block.records[i].editableText !== baseline[i]) {
            changes[String(i)] = block.records[i].editableText;
          }
        }
      }
      payload[kind] = changes;
    });
    return payload;
  }

  function prepareProjectChanges(rom, payload) {
    if (payload == null) payload = {};
    if (typeof payload !== 'object' || Array.isArray(payload)) {
      throw new Error('Description Project data must be an object.');
    }
    var prepared = { blocks: {}, counts: {} };
    Object.keys(SPECS).forEach(function(kind) {
      var spec = SPECS[kind];
      var changes = own(payload, kind) ? payload[kind] : {};
      if (changes == null || typeof changes !== 'object' || Array.isArray(changes)) {
        throw new Error(spec.label + ' Project data must be an ID-to-text object.');
      }
      var block = rom[spec.property];
      if (!block) {
        if (Object.keys(changes).length) throw new Error(spec.label + ' are unavailable in this ROM parse.');
        prepared.counts[kind] = 0;
        return;
      }
      var next = block;
      var count = 0;
      Object.keys(changes).forEach(function(key) {
        if (!/^(?:0|[1-9]\d*)$/.test(key)) {
          throw new Error(spec.label + ' contain invalid record ID ' + JSON.stringify(key) + '.');
        }
        var id = Number(key);
        if (!Number.isInteger(id) || id < 0 || id >= spec.recordCount) {
          throw new Error(spec.label + ' record ID ' + id + ' is outside 0..' +
            (spec.recordCount - 1) + '.');
        }
        if (typeof changes[key] !== 'string') {
          throw new Error(spec.recordLabel + ' ' + id + ' Project value must be text.');
        }
        next = replaceText(next, id, changes[key]);
        count++;
      });
      if (count) prepareBlock(next);
      prepared.blocks[spec.property] = next;
      prepared.counts[kind] = count;
    });
    return prepared;
  }

  function ownerRegion(block, label) {
    var spec = SPECS[block.kind];
    return {
      kind: 'rom',
      start: block.meta.headerOffset,
      size: spec.slotSize,
      label: label || spec.label.toLowerCase() + ' LZSS slot'
    };
  }

  OB64.lzssCompressOptimal = lzssCompressOptimal;
  OB64.descriptionCodec = {
    specs: SPECS,
    parseAll: parseAll,
    parseBlock: parseBlock,
    serializeBlock: serializeBlock,
    serializeRecords: serializeRecords,
    withText: withText,
    measureText: measureText,
    snapshotAll: snapshotAll,
    hasTextChanges: hasTextChanges,
    collectProjectPayload: collectProjectPayload,
    prepareProjectChanges: prepareProjectChanges,
    ownerRegion: ownerRegion
  };

  OB64.serializeItemDescriptions = function(block, z64) { return serializeBlock(block, z64); };
  OB64.serializeConsumableDescriptions = function(block, z64) { return serializeBlock(block, z64); };
  OB64.serializeClassDescriptions = function(block, z64) { return serializeBlock(block, z64); };
  OB64.serializeActionDescriptions = function(block, z64) { return serializeBlock(block, z64); };
})();
