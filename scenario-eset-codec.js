// Scenario ESET codec subset ported from parent tools/eset_codec.js.
window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var SECTION1_ROW_SIZE = 18;
  var SECTION2_ROW_SIZE = 18;
  var SECTION3_ROW_SIZE = 10;
  var SECTION1_START = 0x12;

  var GATE_OPERATORS = {
    0: { name: 'single-extra', summary: 'extra A' },
    1: { name: 'branch-choice', summary: 'A fork, else B fork' },
    2: { name: 'and', summary: 'extra A and extra B' },
    3: { name: 'or', summary: 'extra A or extra B' },
  };

  var DEFAULT_LIMITS = {
    // Measured live in-emulator: 20 rows land with source 0x31; source 0x32 gets no object
    // slot and the whole scenario fails to load (the runtime object pool ends at source 0x31
    // inclusive, 50 slots). Archive slot fit is a separate per-key export gate.
    section1RowsMax: 20,
    sourceIdMax: 0x31,
    section2RowsMax: 16,
    section3RowsMax: 16,
    descriptorGroupsMax: 10,
    descriptorMembersMax: 5,
    enemydatRecords: 556,
  };

  function readU16BE(buf, off) {
    return ((buf[off] || 0) << 8) | (buf[off + 1] || 0);
  }

  function writeU16BE(buf, off, value) {
    buf[off] = (value >> 8) & 0xFF;
    buf[off + 1] = value & 0xFF;
  }

  function cloneBytes(bytes) {
    return Array.prototype.slice.call(bytes || []);
  }

  function bytesToHex(bytes) {
    return cloneBytes(bytes).map(function(b) {
      return Number(b || 0).toString(16).toUpperCase().padStart(2, '0');
    }).join(' ');
  }

  function bytesToCompactHex(bytes) {
    return cloneBytes(bytes).map(function(b) {
      return Number(b || 0).toString(16).padStart(2, '0');
    }).join('');
  }

  function compactHexToBytes(hex) {
    var clean = String(hex || '').replace(/[^0-9a-f]/gi, '');
    if (clean.length % 2) throw new Error('Hex string has an odd number of digits');
    var out = new Uint8Array(clean.length / 2);
    for (var i = 0; i < out.length; i++) out[i] = parseInt(clean.substr(i * 2, 2), 16);
    return out;
  }

  function hx(value, width) {
    width = width || 2;
    return '0x' + Number(value || 0).toString(16).toUpperCase().padStart(width, '0');
  }

  function decodeSection1Row(bytes, row, offset, isFinalRow) {
    return {
      row: row,
      offset: offset,
      isFinalRow: !!isFinalRow,
      bytes: bytes,
      raw18: bytesToHex(bytes),
      sourceId: bytes[0] || 0,
      edatOneBased: readU16BE(bytes, 1),
      placement: {
        mode: bytes[4] === 0 ? 'selector' : 'coordinate',
        selectorOrX: bytes[3] || 0,
        zOrMode: bytes[4] || 0,
      },
      startNode: bytes[6] || 0,
      behaviorBytes: [bytes[7] || 0, bytes[8] || 0],
      tierByte: bytes[9] || 0,
      dropFlag: bytes[10] || 0,
      dropRaw: readU16BE(bytes, 11),
      routeHelperBytes: [bytes[13] || 0, bytes[14] || 0, bytes[15] || 0],
      tail: [bytes[16] || 0, bytes[17] || 0],
    };
  }

  function decodeSection2Gate(bytes) {
    var operator = bytes[11] || 0;
    var info = GATE_OPERATORS[operator] || { name: 'unknown', summary: 'unknown' };
    var extensionOperator = bytes[13] || 0;
    var extensionExtra = bytes[14] || 0;
    return {
      extraA: bytes[10] || 0,
      operator: operator,
      operatorName: info.name,
      operatorSummary: info.summary,
      extraB: bytes[12] || 0,
      isCompound: operator !== 0,
      branchChoiceNextOnExtraA: bytes[16] || 0,
      nextNodeOnTrue: bytes[17] || 0,
      extendedTerm: (extensionOperator || extensionExtra) ? {
        operator: extensionOperator,
        operatorName: (GATE_OPERATORS[extensionOperator] || { name: 'unknown' }).name,
        extra: extensionExtra,
        nextNodeOnExtraA: bytes[16] || 0,
      } : null,
    };
  }

  function decodeSection2Row(bytes, row, offset) {
    var gate = decodeSection2Gate(bytes);
    return {
      row: row,
      offset: offset,
      bytes: bytes,
      raw18: bytesToHex(bytes),
      nodeId: bytes[0] || 0,
      kind: bytes[1] || 0,
      subtype: bytes[2] || 0,
      section3Ref: bytes[10] || 0,
      gate: gate,
      nextNode: bytes[17] || 0,
    };
  }

  function decodeSection3Row(bytes, row, offset) {
    return {
      row: row,
      offset: offset,
      bytes: bytes,
      raw10: bytesToHex(bytes),
      extraId: bytes[0] || 0,
      kind: bytes[1] || 0,
    };
  }

  function parseEset(input, options) {
    options = options || {};
    var buf = input instanceof Uint8Array ? input : compactHexToBytes(input);
    if (buf.length < 16) throw new Error('ESET file is shorter than the 16-byte header');

    var section2Offset = readU16BE(buf, 4);
    var section3Offset = readU16BE(buf, 6);
    var section1Count = readU16BE(buf, 14);
    var section3Present = section3Offset !== 0;
    var section1 = [];
    var section2 = [];
    var section3 = [];

    for (var i = 0; i < section1Count; i++) {
      var off1 = SECTION1_START + i * SECTION1_ROW_SIZE;
      section1.push(decodeSection1Row(cloneBytes(buf.slice(off1, off1 + SECTION1_ROW_SIZE)), i, off1, i === section1Count - 1));
    }

    var section2Count = section2Offset < buf.length ? buf[section2Offset] : 0;
    for (var j = 0; j < section2Count; j++) {
      var off2 = section2Offset + 1 + j * SECTION2_ROW_SIZE;
      section2.push(decodeSection2Row(cloneBytes(buf.slice(off2, off2 + SECTION2_ROW_SIZE)), j, off2));
    }

    var section3Count = section3Present && section3Offset < buf.length ? buf[section3Offset] : 0;
    for (var k = 0; k < section3Count; k++) {
      var off3 = section3Offset + 1 + k * SECTION3_ROW_SIZE;
      section3.push(decodeSection3Row(cloneBytes(buf.slice(off3, off3 + SECTION3_ROW_SIZE)), k, off3));
    }

    return {
      format: 'ob64-eset',
      sourcePath: options.sourcePath || null,
      byteLength: buf.length,
      header: cloneBytes(buf.slice(0, 16)),
      seed: cloneBytes(buf.slice(0x10, 0x12)),
      offsets: {
        stream0: readU16BE(buf, 0),
        countStream: readU16BE(buf, 2),
        section2: section2Offset,
        section3: section3Offset,
      },
      mission: {
        sequence: buf[8] || 0,
        variant: buf[10] || 0,
        subFlag: buf[11] || 0,
        extendedField: readU16BE(buf, 12),
      },
      section3Present: section3Present,
      section1: section1,
      section2: section2,
      section3: section3,
    };
  }

  function computeOffsets(model) {
    var section2Offset = 0x10 + model.section1.length * SECTION1_ROW_SIZE;
    var section3Present = model.section3Present || model.section3.length > 0;
    var section3Offset = section3Present ? section2Offset + 1 + model.section2.length * SECTION2_ROW_SIZE : 0;
    var byteLength = section3Present
      ? section3Offset + 1 + model.section3.length * SECTION3_ROW_SIZE
      : section2Offset + 1 + model.section2.length * SECTION2_ROW_SIZE;
    return { section2Offset: section2Offset, section3Offset: section3Offset, section3Present: section3Present, byteLength: byteLength };
  }

  function refreshDecodedRows(model) {
    for (var i = 0; i < model.section1.length; i++) {
      model.section1[i] = decodeSection1Row(model.section1[i].bytes, i, SECTION1_START + i * SECTION1_ROW_SIZE, i === model.section1.length - 1);
    }
    var offsets = computeOffsets(model);
    for (var j = 0; j < model.section2.length; j++) {
      model.section2[j] = decodeSection2Row(model.section2[j].bytes, j, offsets.section2Offset + 1 + j * SECTION2_ROW_SIZE);
    }
    for (var k = 0; k < model.section3.length; k++) {
      model.section3[k] = decodeSection3Row(model.section3[k].bytes, k, offsets.section3Offset + 1 + k * SECTION3_ROW_SIZE);
    }
    return model;
  }

  function serializeEset(model) {
    refreshDecodedRows(model);
    if (model.section1.length && model.section2.length === 0) {
      throw new Error('Cannot serialize ESET with Section 1 rows and zero Section 2 nodes; the final Section 1 row overlaps the Section 2 count byte.');
    }
    var offsets = computeOffsets(model);
    var out = new Uint8Array(offsets.byteLength);
    for (var h = 0; h < Math.min(16, model.header.length); h++) out[h] = model.header[h] & 0xFF;
    writeU16BE(out, 0, 0x0008);
    writeU16BE(out, 2, 0x000F);
    writeU16BE(out, 4, offsets.section2Offset);
    writeU16BE(out, 6, offsets.section3Offset);
    writeU16BE(out, 14, model.section1.length);
    for (var s = 0; s < Math.min(2, model.seed.length); s++) out[0x10 + s] = model.seed[s] & 0xFF;

    for (var i = 0; i < model.section1.length; i++) {
      out.set(model.section1[i].bytes.slice(0, SECTION1_ROW_SIZE), SECTION1_START + i * SECTION1_ROW_SIZE);
    }
    out[offsets.section2Offset] = model.section2.length & 0xFF;
    for (var j = 0; j < model.section2.length; j++) {
      out.set(model.section2[j].bytes.slice(0, SECTION2_ROW_SIZE), offsets.section2Offset + 1 + j * SECTION2_ROW_SIZE);
    }
    if (offsets.section3Present) {
      out[offsets.section3Offset] = model.section3.length & 0xFF;
      for (var k = 0; k < model.section3.length; k++) {
        out.set(model.section3[k].bytes.slice(0, SECTION3_ROW_SIZE), offsets.section3Offset + 1 + k * SECTION3_ROW_SIZE);
      }
    }
    return out;
  }

  function descriptorGroups(section1) {
    var groups = [];
    var current = null;
    for (var row = 1; row < section1.length; row++) {
      var prev = section1[row - 1].bytes;
      var groupId = prev[16] || 0;
      if (!groupId) {
        current = null;
        continue;
      }
      if (!current || current.groupId !== groupId) {
        current = { groupId: groupId, pattern: prev[17] || 0, startRow: row, rows: [] };
        groups.push(current);
      }
      current.rows.push(row);
    }
    return groups;
  }

  function issue(list, code, message, context) {
    var obj = { code: code, message: message };
    context = context || {};
    for (var k in context) obj[k] = context[k];
    list.push(obj);
  }

  function validateEset(model, options) {
    options = options || {};
    refreshDecodedRows(model);
    var limits = {};
    for (var lk in DEFAULT_LIMITS) limits[lk] = DEFAULT_LIMITS[lk];
    if (options.limits) for (var ok in options.limits) limits[ok] = options.limits[ok];
    var errors = [];
    var warnings = [];
    var info = [];
    var offsets = computeOffsets(model);
    var nodeIds = {};
    var extraIds = {};
    var nodeCounts = {};
    var extraCounts = {};

    if (model.offsets.stream0 !== 0x0008) issue(errors, 'header-stream0-offset', 'Header [0..1] must be 0x0008', { actual: model.offsets.stream0 });
    if (model.offsets.countStream !== 0x000F) issue(errors, 'header-count-stream-offset', 'Header [2..3] must be 0x000F', { actual: model.offsets.countStream });
    if (model.offsets.section2 !== offsets.section2Offset) issue(errors, 'section2-offset', 'Section 2 offset does not match Section 1 count', { actual: model.offsets.section2, expected: offsets.section2Offset });
    if (model.offsets.section3 !== offsets.section3Offset) issue(errors, 'section3-offset', 'Section 3 offset does not match recomputed offset', { actual: model.offsets.section3, expected: offsets.section3Offset });
    if ((model.header[14] || 0) !== 0) issue(errors, 'section1-count-high-byte', 'Section 1 count high byte must be zero', { actual: model.header[14] });
    if (model.section1.length > limits.section1RowsMax) issue(errors, 'section1-cap', 'Section 1 rows exceed conservative limit', { count: model.section1.length, limit: limits.section1RowsMax });
    if (model.section1.length && model.section2.length === 0) issue(errors, 'section2-empty', 'Section 1 rows require at least one Section 2 node', { section1Rows: model.section1.length });
    if (model.section2.length > limits.section2RowsMax) issue(errors, 'section2-cap', 'Section 2 rows exceed hard limit', { count: model.section2.length, limit: limits.section2RowsMax });
    if (model.section3.length > limits.section3RowsMax) issue(errors, 'section3-cap', 'Section 3 rows exceed hard limit', { count: model.section3.length, limit: limits.section3RowsMax });

    model.section2.forEach(function(row) {
      nodeCounts[row.nodeId] = (nodeCounts[row.nodeId] || 0) + 1;
      nodeIds[row.nodeId] = true;
      if (row.nodeId < 0x04 || row.nodeId >= 0x14) issue(errors, 'section2-node-id-domain', 'Section 2 node id must be 0x04..0x13', { row: row.row, nodeId: row.nodeId });
      if (row.nodeId !== 4 + row.row) issue(errors, 'section2-node-id-sequence', 'Section 2 node id must equal 4 + row index', { row: row.row, nodeId: row.nodeId, expected: 4 + row.row });
    });
    model.section3.forEach(function(row) {
      extraCounts[row.extraId] = (extraCounts[row.extraId] || 0) + 1;
      extraIds[row.extraId] = true;
      if (row.extraId < 0x01 || row.extraId > 0x10) issue(errors, 'section3-extra-id-domain', 'Section 3 extra id must be 0x01..0x10', { row: row.row, extraId: row.extraId });
      if (row.extraId !== 1 + row.row) issue(errors, 'section3-extra-id-sequence', 'Section 3 extra id must equal 1 + row index', { row: row.row, extraId: row.extraId, expected: 1 + row.row });
    });
    Object.keys(nodeCounts).forEach(function(id) {
      if (nodeCounts[id] > 1) issue(errors, 'section2-node-id-duplicate', 'Section 2 node id appears more than once', { nodeId: Number(id), count: nodeCounts[id] });
    });
    Object.keys(extraCounts).forEach(function(id) {
      if (extraCounts[id] > 1) issue(errors, 'section3-extra-id-duplicate', 'Section 3 extra id appears more than once', { extraId: Number(id), count: extraCounts[id] });
    });
    model.section1.forEach(function(row) {
      if (row.sourceId > limits.sourceIdMax) issue(errors, 'source-id-cap', 'sourceId exceeds conservative limit', { row: row.row, sourceId: row.sourceId, limit: limits.sourceIdMax });
      if (row.edatOneBased < 1 || row.edatOneBased > limits.enemydatRecords) {
        issue(errors, 'section1-edat-range', 'Section 1 edatOneBased must reference an enemydat record', { row: row.row, edatOneBased: row.edatOneBased, min: 1, max: limits.enemydatRecords });
      }
    });

    if (model.section1.length) {
      var final = model.section1[model.section1.length - 1];
      var firstNode = model.section2[0] ? model.section2[0].nodeId : 0;
      if (final.bytes[16] !== model.section2.length || final.bytes[17] !== firstNode) {
        issue(errors, 'final-row-section2-alias', 'Final Section 1 tail must alias Section 2 count and first node id', {
          finalTail: [final.bytes[16], final.bytes[17]],
          expected: [model.section2.length, firstNode],
        });
      }
    }

    var groups = descriptorGroups(model.section1);
    if (groups.length > limits.descriptorGroupsMax) issue(errors, 'descriptor-group-cap', 'Route descriptor group cap exceeded', { count: groups.length });
    groups.forEach(function(group) {
      if (group.rows.length > limits.descriptorMembersMax) issue(errors, 'descriptor-member-cap', 'Route descriptor member cap exceeded', { groupId: group.groupId, rows: group.rows });
    });

    model.section2.forEach(function(row) {
      if (row.nextNode === 0xFF) issue(warnings, 'terminal-ff-next-node', '0xFF terminal/one-way sentinel', { row: row.row, nodeId: row.nodeId });
      else if (row.nextNode === 1) issue(info, 'next-node-hold-sentinel', '0x01 is the known hold-position sentinel', { row: row.row, nodeId: row.nodeId });
      else if (row.nextNode !== 0 && !nodeIds[row.nextNode]) issue(warnings, 'next-node-unresolved', 'Next node target is missing', { row: row.row, nodeId: row.nodeId, nextNode: row.nextNode });
      if (row.section3Ref !== 0 && !extraIds[row.section3Ref]) issue(warnings, 'section3-ref-unresolved', 'Section 3 reference is missing or an alias case', { row: row.row, nodeId: row.nodeId, section3Ref: row.section3Ref });
      if (!GATE_OPERATORS[row.gate.operator]) issue(errors, 'section2-gate-operator-domain', 'Gate operator must be 0, 1, 2, or 3', { row: row.row, operator: row.gate.operator });
      if (row.gate.operator !== 0 && !extraIds[row.gate.extraB]) issue(errors, 'section2-gate-extraB-unresolved', 'Compound gate extra B must reference an existing extra', { row: row.row, operator: row.gate.operator, extraB: row.gate.extraB });
      if (row.gate.extendedTerm && row.gate.extendedTerm.extra && !extraIds[row.gate.extendedTerm.extra]) {
        issue(warnings, 'section2-gate-extension-extra-unresolved', 'Extension gate term references a missing extra', { row: row.row, nodeId: row.nodeId, extra: row.gate.extendedTerm.extra, operator: row.gate.extendedTerm.operator });
      }
    });

    model.section1.forEach(function(row) {
      var start = model.section2.filter(function(node) { return node.nodeId === row.startNode; })[0];
      if (start && start.kind === 2 && (start.nextNode === 0 || start.section3Ref === 0)) {
        issue(warnings, 'dormant-node-needs-order', 'Dormant kind-2 start needs a next node and condition', { section1Row: row.row, sourceId: row.sourceId });
      }
    });
    if (model.section3.length === 0) issue(info, 'no-section3', 'No Section 3 stream');
    return { ok: errors.length === 0, errors: errors, warnings: warnings, info: info, limits: limits, descriptorGroups: groups };
  }

  function cloneModel(model) {
    return parseEset(serializeEset(model), { sourcePath: model.sourcePath || null });
  }

  function equalBytes(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function roundTripAll(data) {
    var scenarios = (data && data.scenarios) || [];
    var results = [];
    var passed = 0;
    var errors = 0;
    scenarios.forEach(function(entry) {
      if (entry.missing || !entry.rawHex) {
        results.push({ runtimeKey: entry.runtimeKey, missing: true, byteIdentical: false, errors: 1 });
        errors += 1;
        return;
      }
      var raw = compactHexToBytes(entry.rawHex);
      var model = parseEset(raw, { sourcePath: entry.relPath || entry.filename });
      var rebuilt = serializeEset(model);
      var validation = validateEset(model);
      var byteIdentical = equalBytes(raw, rebuilt);
      if (byteIdentical && validation.errors.length === 0) passed += 1;
      errors += validation.errors.length;
      results.push({
        runtimeKey: entry.runtimeKey,
        archive: entry.archive,
        filename: entry.filename,
        byteIdentical: byteIdentical,
        errors: validation.errors.length,
        warnings: validation.warnings.length,
      });
    });
    return {
      summary: {
        files: scenarios.length,
        passed: passed,
        failed: scenarios.length - passed,
        errors: errors,
      },
      results: results,
    };
  }

  OB64.scenarioCodec = {
    DEFAULT_LIMITS: DEFAULT_LIMITS,
    GATE_OPERATORS: GATE_OPERATORS,
    parseEset: parseEset,
    serializeEset: serializeEset,
    validateEset: validateEset,
    refreshDecodedRows: refreshDecodedRows,
    computeOffsets: computeOffsets,
    descriptorGroups: descriptorGroups,
    cloneModel: cloneModel,
    bytesToHex: bytesToHex,
    bytesToCompactHex: bytesToCompactHex,
    compactHexToBytes: compactHexToBytes,
    equalBytes: equalBytes,
    hexByte: hx,
    roundTripAll: roundTripAll,
  };
})(window.OB64);
