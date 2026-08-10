// OB64 Mod Editor — fail-closed validation for finished ROM export candidates.
//
// This module does not download or adopt a ROM. It inspects the detached
// candidate produced by app.js and returns a JSON-safe report. The caller may
// download the ROM only when report.ok is true.
window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var REPORT_SCHEMA = 'ob64-rom-export-validation-report';
  var REPORT_VERSION = 1;

  function equalBytes(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }

  function firstMismatch(a, b) {
    var length = Math.min(a ? a.length : 0, b ? b.length : 0);
    for (var i = 0; i < length; i++) if (a[i] !== b[i]) return i;
    return a && b && a.length === b.length ? -1 : length;
  }

  function hex(value, width) {
    return '0x' + Number(value || 0).toString(16).toUpperCase().padStart(width || 8, '0');
  }

  function readU16LE(bytes, offset) {
    return (bytes[offset] | (bytes[offset + 1] << 8)) >>> 0;
  }

  function readU32LE(bytes, offset) {
    return (bytes[offset] | (bytes[offset + 1] << 8) |
      (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24)) >>> 0;
  }

  function issue(code, title, message, suggestion, technical) {
    var error = new Error(message);
    error.validationIssue = {
      code: code,
      title: title,
      message: message,
      suggestion: suggestion || '',
      technical: technical || {},
    };
    return error;
  }

  function issueFromException(error, fallback) {
    if (error && error.validationIssue) return error.validationIssue;
    return {
      code: fallback.code,
      title: fallback.title,
      message: fallback.message,
      suggestion: fallback.suggestion || '',
      technical: {
        exceptionName: error && error.name || 'Error',
        exceptionMessage: error && error.message || String(error),
      },
    };
  }

  function runCheck(report, id, name, fallback, check) {
    var started = Date.now();
    try {
      var details = check() || {};
      report.checks.push({
        id: id,
        name: name,
        status: details.status || 'passed',
        summary: details.summary || 'Passed.',
        durationMs: Date.now() - started,
        details: details.details || {},
      });
      return details.value;
    } catch (error) {
      var found = issueFromException(error, fallback);
      report.checks.push({
        id: id,
        name: name,
        status: 'failed',
        summary: found.message,
        durationMs: Date.now() - started,
        details: found.technical || {},
      });
      report.errors.push(found);
      return null;
    }
  }

  function* runCheckStep(report, id, name, fallback, check) {
    yield { id: id, name: name };
    return runCheck(report, id, name, fallback, check);
  }

  function skipped(summary) {
    return { status: 'skipped', summary: summary, details: {} };
  }

  function sha256(bytes) {
    if (!OB64.consumableEffects || !OB64.consumableEffects.sha256HexSync) {
      throw issue(
        'VALIDATOR_DEPENDENCY_MISSING',
        'Validation component is missing',
        'The editor could not load its ROM hashing component.',
        'Reload the editor and try the export again.',
        { dependency: 'OB64.consumableEffects.sha256HexSync' }
      );
    }
    return OB64.consumableEffects.sha256HexSync(bytes);
  }

  function imageIdentity(rom) {
    var z64 = rom && rom.z64;
    if (!(z64 instanceof Uint8Array)) return { size: 0, sha256: null };
    var layout = OB64.detectRomLayout(z64);
    return {
      size: z64.length,
      normalizedByteOrder: OB64.detectRomByteOrder(z64),
      layout: layout ? layout.id : null,
      sha256: sha256(z64),
      headerCrc1: hex(OB64.readU32BE(z64, 0x10), 8),
      headerCrc2: hex(OB64.readU32BE(z64, 0x14), 8),
    };
  }

  function parseArchiveAt(z64, offset) {
    if (!(z64 instanceof Uint8Array) || !Number.isInteger(offset) ||
        offset < 0 || offset + 24 > z64.length) {
      throw issue(
        'ARCHIVE_HEADER_INVALID',
        'Archive header is invalid',
        'A rebuilt archive header falls outside the ROM image.',
        'Keep the error report and recreate the ROM after updating the editor.',
        { offset: offset, imageSize: z64 && z64.length }
      );
    }
    var method = String.fromCharCode(
      z64[offset + 2], z64[offset + 3], z64[offset + 4],
      z64[offset + 5], z64[offset + 6]
    );
    if (!/^\-lh[0-9s]\-$/.test(method)) {
      throw issue(
        'ARCHIVE_HEADER_INVALID',
        'Archive header is invalid',
        'A scenario archive no longer has a recognizable LHA header.',
        'Keep the error report and recreate the ROM after updating the editor.',
        { offset: hex(offset), method: method }
      );
    }
    var level = z64[offset + 20];
    var headerSize = level === 2 ? readU16LE(z64, offset) : 2 + z64[offset];
    var compressedSize = readU32LE(z64, offset + 7);
    var uncompressedSize = readU32LE(z64, offset + 11);
    var minimumHeader = level === 2 ? 26 : 24;
    if (headerSize < minimumHeader || offset + headerSize > z64.length ||
        compressedSize < 1 || uncompressedSize < 1 ||
        offset + headerSize + compressedSize > z64.length) {
      throw issue(
        'ARCHIVE_SIZE_INVALID',
        'Archive size is invalid',
        'A scenario archive declares a size that does not fit inside the ROM.',
        'Keep the error report and recreate the ROM after updating the editor.',
        {
          offset: hex(offset),
          level: level,
          headerSize: headerSize,
          compressedSize: compressedSize,
          uncompressedSize: uncompressedSize,
          imageSize: z64.length,
        }
      );
    }
    return {
      offset: offset,
      method: method,
      level: level,
      totalHeaderSize: headerSize,
      compSize: compressedSize,
      uncompSize: uncompressedSize,
      end: offset + headerSize + compressedSize,
      dataCrc: readU16LE(z64, offset + 21),
    };
  }

  function validateCommonHeaderCrc(z64, member) {
    if (member.level !== 2) {
      throw issue(
        'ARCHIVE_HEADER_LEVEL',
        'Archive header format is wrong',
        'A rebuilt scenario archive does not use the required level-2 LHA header.',
        'Keep the error report and recreate the ROM after updating the editor.',
        { offset: hex(member.offset), level: member.level }
      );
    }
    var header = z64.slice(member.offset, member.offset + member.totalHeaderSize);
    var cursor = 24;
    var crcOffset = -1;
    var terminated = false;
    while (cursor + 2 <= header.length) {
      var extensionSize = readU16LE(header, cursor);
      if (extensionSize === 0) {
        terminated = true;
        break;
      }
      if (extensionSize < 3 || cursor + extensionSize > header.length) {
        throw issue(
          'ARCHIVE_HEADER_CHAIN',
          'Archive header extensions are invalid',
          'A rebuilt scenario archive has a broken LHA header extension chain.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { offset: hex(member.offset), extensionOffset: cursor, extensionSize: extensionSize }
        );
      }
      if (header[cursor + 2] === 0x00 && extensionSize >= 5) {
        crcOffset = cursor + 3;
        break;
      }
      cursor += extensionSize;
    }
    if (crcOffset < 0) {
      throw issue(
        'ARCHIVE_HEADER_CRC_MISSING',
        'Archive header checksum is missing',
        'A rebuilt scenario archive is missing its LHA header checksum.',
        'Keep the error report and recreate the ROM after updating the editor.',
        { offset: hex(member.offset), chainTerminated: terminated }
      );
    }
    var stored = readU16LE(header, crcOffset);
    header[crcOffset] = 0;
    header[crcOffset + 1] = 0;
    var computed = OB64.crc16(header);
    if (stored !== computed) {
      throw issue(
        'ARCHIVE_HEADER_CRC',
        'Archive header checksum failed',
        'A rebuilt scenario archive has a damaged LHA header checksum.',
        'Keep the error report and recreate the ROM after updating the editor.',
        { offset: hex(member.offset), stored: hex(stored, 4), computed: hex(computed, 4) }
      );
    }
    return { stored: stored, computed: computed };
  }

  function validateArchiveTarget(candidateZ64, target) {
    var member = parseArchiveAt(candidateZ64, Number(target.offset));
    if (member.method !== '-lh5-' && member.method !== '-lh0-') {
      throw issue(
        'ARCHIVE_METHOD_UNSUPPORTED',
        'Archive compression method is unsupported',
        'A rebuilt scenario archive uses a compression method the game export does not support.',
        'Keep the error report and recreate the ROM after updating the editor.',
        { archive: target.archive, method: member.method, offset: hex(member.offset) }
      );
    }
    validateCommonHeaderCrc(candidateZ64, member);

    if (target.kind === 'fixed' && member.end !== Number(target.slotEnd)) {
      throw issue(
        'ARCHIVE_BOUNDARY',
        'Archive ends at the wrong place',
        'A rebuilt scenario archive does not fill its assigned ROM slot exactly.',
        'Keep the error report and recreate the ROM after updating the editor.',
        {
          archive: target.archive,
          offset: hex(member.offset),
          declaredEnd: hex(member.end),
          assignedEnd: hex(target.slotEnd),
          difference: member.end - Number(target.slotEnd),
        }
      );
    }
    if (target.kind === 'relocated') {
      var expectedEnd = Number(target.offset) + Number(target.memberSize);
      if (member.end !== expectedEnd) {
        throw issue(
          'ARCHIVE_BOUNDARY',
          'Archive ends at the wrong place',
          'A relocated scenario archive does not end at its assigned ROM boundary.',
          'Keep the error report and recreate the ROM after updating the editor.',
          {
            archive: target.archive,
            offset: hex(member.offset),
            declaredEnd: hex(member.end),
            assignedEnd: hex(expectedEnd),
            difference: member.end - expectedEnd,
          }
        );
      }
      if (candidateZ64[Number(target.terminatorOffset)] !== 0) {
        throw issue(
          'ARCHIVE_TERMINATOR',
          'Archive terminator is missing',
          'A relocated scenario archive is missing the zero byte that ends its member list.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { archive: target.archive, terminatorOffset: hex(target.terminatorOffset) }
        );
      }
    }

    var expected = target.expectedRaw;
    if (!(expected instanceof Uint8Array)) {
      throw issue(
        'ARCHIVE_EXPECTATION_MISSING',
        'Archive verification data is missing',
        'The editor did not retain the intended scenario data for extract-back verification.',
        'Reload the editor and recreate the ROM.',
        { archive: target.archive, label: target.label }
      );
    }
    if (member.uncompSize !== expected.length) {
      throw issue(
        'ARCHIVE_UNCOMPRESSED_SIZE',
        'Archive data size is wrong',
        'A rebuilt scenario archive declares the wrong uncompressed data size.',
        'Keep the error report and recreate the ROM after updating the editor.',
        {
          archive: target.archive,
          declaredSize: member.uncompSize,
          intendedSize: expected.length,
        }
      );
    }
    var extracted;
    try {
      extracted = OB64.extractArchive(candidateZ64, member);
    } catch (extractError) {
      throw issue(
        'ARCHIVE_EXTRACT_FAILED',
        'Archive could not be reopened',
        'The editor could not extract a rebuilt scenario archive from the finished ROM.',
        'Keep the error report and recreate the ROM after updating the editor.',
        { archive: target.archive, exceptionMessage: extractError.message }
      );
    }
    var dataCrc = OB64.crc16(extracted);
    if (dataCrc !== member.dataCrc) {
      throw issue(
        'ARCHIVE_DATA_CRC',
        'Archive data checksum failed',
        'A rebuilt scenario archive has a damaged data checksum.',
        'Keep the error report and recreate the ROM after updating the editor.',
        {
          archive: target.archive,
          stored: hex(member.dataCrc, 4),
          computed: hex(dataCrc, 4),
        }
      );
    }
    var mismatch = firstMismatch(extracted, expected);
    if (mismatch !== -1) {
      throw issue(
        'ARCHIVE_EXTRACT_MISMATCH',
        'Archive contents changed during packing',
        'A rebuilt scenario archive does not extract back to the data the editor intended to save.',
        'Keep the error report and recreate the ROM after updating the editor.',
        {
          archive: target.archive,
          mismatchOffset: mismatch,
          extractedSize: extracted.length,
          intendedSize: expected.length,
          extractedByte: mismatch < extracted.length ? extracted[mismatch] : null,
          intendedByte: mismatch < expected.length ? expected[mismatch] : null,
        }
      );
    }

    if (target.contentKind === 'eset') {
      try {
        var model = OB64.scenarioCodec.parseEset(extracted, {
          sourcePath: target.label || ('archive-' + target.archive),
        });
        var validation = OB64.scenarioCodec.validateEset(model);
        if (validation.errors.length) {
          throw new Error(validation.errors.map(function(found) {
            return found.code || found.message || String(found);
          }).join(', '));
        }
        var rebuilt = OB64.scenarioCodec.serializeEset(model);
        if (!equalBytes(rebuilt, extracted)) {
          throw new Error('parse/serialize bytes differ');
        }
      } catch (esetError) {
        throw issue(
          'SCENARIO_CONSISTENCY',
          'Scenario data is inconsistent',
          'A rebuilt scenario fails the editor’s structural scenario checks.',
          'Review that scenario’s squads and triggers, then export again.',
          {
            archive: target.archive,
            runtimeKey: target.runtimeKey,
            exceptionMessage: esetError.message,
          }
        );
      }
    }

    return {
      summary: (target.label || ('Archive #' + target.archive)) + ' reopened successfully.',
      details: {
        archive: target.archive,
        label: target.label,
        contentKind: target.contentKind,
        placement: target.kind,
        offset: hex(member.offset),
        method: member.method,
        headerSize: member.totalHeaderSize,
        compressedSize: member.compSize,
        uncompressedSize: member.uncompSize,
        memberEnd: hex(member.end),
        extractedSha256: sha256(extracted),
        intendedSha256: sha256(expected),
      },
    };
  }

  function validateArchiveCatalog(sourceRom, candidateRom, rebuiltTargets) {
    var sourceArchives = sourceRom.archives || OB64.findArchives(sourceRom.z64);
    var rebuiltByOffset = {};
    (rebuiltTargets || []).forEach(function(target) {
      if (target && target.offset != null && Number.isInteger(Number(target.offset))) {
        rebuiltByOffset[Number(target.offset)] = target;
      }
    });
    var authorizedMethodChanges = [];
    if (!sourceArchives.length) {
      throw issue(
        'ARCHIVE_CATALOG_EMPTY',
        'ROM archive catalog is missing',
        'The source ROM does not contain the archive catalog required by this editor.',
        'Reload a supported US ROM and try again.',
        {}
      );
    }
    for (var i = 0; i < sourceArchives.length; i++) {
      var source = sourceArchives[i];
      var candidate = parseArchiveAt(candidateRom.z64, source.offset);
      var rebuiltTarget = rebuiltByOffset[source.offset];
      var methodChanged = candidate.method !== source.method;
      var methodChangeAuthorized = !!(methodChanged && rebuiltTarget &&
        (source.method === '-lh0-' || source.method === '-lh5-') &&
        (candidate.method === '-lh0-' || candidate.method === '-lh5-'));
      if (candidate.level !== source.level || (methodChanged && !methodChangeAuthorized)) {
        throw issue(
          'ARCHIVE_CATALOG_CHANGED',
          'ROM archive catalog shifted',
          'An archive header changed type or disappeared at an established ROM location.',
          'Keep the error report and recreate the ROM after updating the editor.',
          {
            archive: i,
            offset: hex(source.offset),
            sourceMethod: source.method,
            candidateMethod: candidate.method,
            sourceLevel: source.level,
            candidateLevel: candidate.level,
          }
        );
      }
      if (methodChangeAuthorized) {
        authorizedMethodChanges.push({
          archive: i,
          offset: hex(source.offset),
          sourceMethod: source.method,
          candidateMethod: candidate.method,
          validationTarget: rebuiltTarget.label || rebuiltTarget.archive,
        });
      }
      var boundary = i + 1 < sourceArchives.length
        ? sourceArchives[i + 1].offset
        : candidateRom.z64.length;
      if (candidate.end > boundary) {
        throw issue(
          'ARCHIVE_OVERLAP',
          'ROM archives overlap',
          'An archive extends into the next established archive location.',
          'Keep the error report and recreate the ROM after updating the editor.',
          {
            archive: i,
            offset: hex(source.offset),
            memberEnd: hex(candidate.end),
            nextArchiveOffset: hex(boundary),
          }
        );
      }
    }
    return {
      summary: sourceArchives.length + ' established archive locations remain readable and non-overlapping.',
      details: {
        establishedArchiveCount: sourceArchives.length,
        authorizedMethodChanges: authorizedMethodChanges,
      },
    };
  }

  function romRegions(regions) {
    return (regions || []).filter(function(region) {
      return !region.kind || region.kind === 'rom';
    });
  }

  function compareRegions(actual, expected, regions, feature, phase) {
    for (var r = 0; r < regions.length; r++) {
      var region = regions[r];
      var start = Number(region.start != null ? region.start : region.offset);
      var size = Number(region.size != null ? region.size : region.length);
      if (!Number.isInteger(start) || !Number.isInteger(size) || size <= 0 ||
          start < 0 || start + size > actual.length || start + size > expected.length) {
        throw issue(
          'SEMANTIC_READBACK_RANGE',
          'Readback range is invalid',
          'The editor could not locate the ROM data needed to verify ' + feature + '.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { feature: feature, phase: phase, start: start, size: size }
        );
      }
      var actualSlice = actual.subarray(start, start + size);
      var expectedSlice = expected.subarray(start, start + size);
      var mismatch = firstMismatch(actualSlice, expectedSlice);
      if (mismatch !== -1) {
        throw issue(
          'SEMANTIC_READBACK_MISMATCH',
          'Saved values did not read back correctly',
          'The finished ROM does not read back the intended ' + feature + ' values.',
          'Keep the error report, reload the project, and try again after updating the editor.',
          {
            feature: feature,
            phase: phase,
            region: region.label || 'data region',
            mismatchOffset: hex(start + mismatch),
            expectedByte: mismatch < expectedSlice.length ? expectedSlice[mismatch] : null,
            actualByte: mismatch < actualSlice.length ? actualSlice[mismatch] : null,
          }
        );
      }
    }
  }

  function validateSemanticRoundTrip(options) {
    var sourceRom = options.sourceRom;
    var candidateRom = options.candidateRom;
    var reloadedRom = options.reloadedRom;
    var expected = sourceRom.z64.slice();
    var reparsed = sourceRom.z64.slice();
    if (options.prepareReloaded) options.prepareReloaded(reloadedRom, sourceRom);
    options.serializer(sourceRom[options.property], expected);
    options.serializer(reloadedRom[options.property], reparsed);
    compareRegions(candidateRom.z64, expected, options.regions, options.label, 'candidate image');
    compareRegions(reparsed, expected, options.regions, options.label, 'parse and serialize readback');
    return {
      summary: options.label + ' values match after reopening the finished ROM.',
      details: { feature: options.property, regionCount: options.regions.length },
    };
  }

  function validateExactWrites(candidateZ64, writes, label) {
    for (var i = 0; i < (writes || []).length; i++) {
      var write = writes[i];
      if (!(write.bytes instanceof Uint8Array) || !Number.isInteger(write.offset) ||
          write.offset < 0 || write.offset + write.bytes.length > candidateZ64.length) {
        throw issue(
          'PATCH_WRITE_INVALID',
          'Runtime patch write is invalid',
          'The editor created an invalid planned write for ' + label + '.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { label: label, writeIndex: i, offset: write.offset, length: write.bytes && write.bytes.length }
        );
      }
      var actual = candidateZ64.subarray(write.offset, write.offset + write.bytes.length);
      var mismatch = firstMismatch(actual, write.bytes);
      if (mismatch !== -1) {
        throw issue(
          'PATCH_INTEGRITY',
          'Runtime patch did not verify',
          'The finished ROM does not contain the complete planned ' + label + ' patch.',
          'Keep the error report and recreate the ROM after updating the editor.',
          {
            label: label,
            write: write.label || i,
            mismatchOffset: hex(write.offset + mismatch),
            expectedByte: write.bytes[mismatch],
            actualByte: actual[mismatch],
          }
        );
      }
    }
    return {
      summary: label + ' patch bytes match the complete write plan.',
      details: { writeCount: (writes || []).length },
    };
  }

  function normalizeShopOverrides(entries) {
    return (entries || []).map(function(entry) {
      return {
        shopIndex: Number(entry.shopIndex),
        items: (entry.items || []).map(Number),
        consumables: (entry.consumables || []).map(Number),
      };
    }).sort(function(a, b) { return a.shopIndex - b.shopIndex; });
  }

  function validateShopOverrides(candidateRom, expectedEntries) {
    var parsedMap = OB64.runtimeOverrides.parseShopOverrides(candidateRom.z64, candidateRom);
    var parsed = Object.keys(parsedMap).map(function(key) { return parsedMap[key]; });
    var expected = normalizeShopOverrides(expectedEntries);
    parsed = normalizeShopOverrides(parsed);
    if (JSON.stringify(parsed) !== JSON.stringify(expected)) {
      throw issue(
        'SEMANTIC_READBACK_MISMATCH',
        'Shop overrides did not read back correctly',
        'The finished ROM does not contain the intended per-shop item lists.',
        'Keep the error report, review the affected shops, and export again.',
        { expected: expected, actual: parsed }
      );
    }
    return {
      summary: expected.length + ' runtime shop override' + (expected.length === 1 ? '' : 's') + ' read back correctly.',
      details: { overrideCount: expected.length },
    };
  }

  function validateTools(candidateRom, sourceRom, toolsResult) {
    var expected = {};
    (toolsResult.applied || []).concat(toolsResult.upgraded || []).forEach(function(name) {
      expected[name] = 'applied';
    });
    (toolsResult.removed || []).forEach(function(name) { expected[name] = 'clean'; });
    var features = OB64.tools.features();
    var checked = 0;
    for (var i = 0; i < features.length; i++) {
      var wanted = expected[features[i].name];
      if (!wanted) continue;
      var actual = OB64.tools.featureState(candidateRom.z64, features[i]);
      if (actual !== wanted) {
        throw issue(
          'PATCH_INTEGRITY',
          'Tool patch did not verify',
          'The finished ROM does not contain the complete ' + features[i].name + ' setting.',
          'Keep the error report and export again after updating the editor.',
          { featureId: features[i].id, expectedState: wanted, actualState: actual }
        );
      }
      checked++;
    }
    return {
      summary: checked + ' changed tool patch' + (checked === 1 ? '' : 'es') + ' verified.',
      details: { changedFeatureCount: checked, skipped: (toolsResult.skipped || []).slice() },
    };
  }

  function validateCombatOverrides(candidateRom, selectorPlan) {
    validateExactWrites(candidateRom.z64, selectorPlan.writes || [], 'attack-animation override');
    var classified = OB64.combatAnimationOverrides.classify(candidateRom.z64);
    if (selectorPlan.mode === 'uninstall') {
      if (classified.kind !== 'retail') {
        throw issue(
          'PATCH_INTEGRITY',
          'Attack-animation removal did not verify',
          'The finished ROM did not restore the retail attack-animation code completely.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { expectedState: 'retail', actualState: classified.kind, reason: classified.reason }
        );
      }
    } else {
      var expected = OB64.combatAnimationOverrides.validateLogicalEntries(
        selectorPlan.state && selectorPlan.state.desired || []
      );
      if (classified.kind !== 'owned-v2' || classified.advanced ||
          JSON.stringify(classified.logical || []) !== JSON.stringify(expected)) {
        throw issue(
          'PATCH_INTEGRITY',
          'Attack-animation patch did not verify',
          'The finished ROM does not contain the intended attack-animation mappings.',
          'Keep the error report and recreate the ROM after updating the editor.',
          {
            expectedState: 'owned-v2',
            actualState: classified.kind,
            reason: classified.reason,
            expectedEntries: expected,
            actualEntries: classified.logical || [],
          }
        );
      }
    }
    return {
      summary: 'Attack-animation mappings and patch bytes read back correctly.',
      details: { mode: selectorPlan.mode, state: classified.kind },
    };
  }

  function validateEffectTransaction(candidateRom, sourceRom, transaction) {
    OB64.consumableEffects.validateFinalAfterImage(
      transaction,
      candidateRom.z64,
      sourceRom.consumableEffects
    );
    for (var i = 0; i < transaction.writes.length; i++) {
      var write = transaction.writes[i];
      var actual = OB64.readU32BE(candidateRom.z64, write.offset) >>> 0;
      if (actual !== (write.afterWord >>> 0)) {
        throw issue(
          'PATCH_INTEGRITY',
          'Consumable-effect patch did not verify',
          'The finished ROM does not contain the intended consumable-effect code.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { offset: hex(write.offset), expectedWord: hex(write.afterWord), actualWord: hex(actual) }
        );
      }
    }
    return {
      summary: 'Consumable-effect code and text read back correctly.',
      details: {
        codeWriteCount: transaction.writes.length,
        descriptionChanged: !!transaction.descriptionWrite,
      },
    };
  }

  function validateRuntimeRemoval(sourceRom, candidateRom) {
    var expected = sourceRom.z64.slice();
    if (OB64.runtimeOverrides) OB64.runtimeOverrides.restoreAll(expected, sourceRom);
    else OB64.squad.restoreVanilla(expected, sourceRom);
    var regions = OB64.runtimeOverrides
      ? OB64.runtimeOverrides.patchRegions(sourceRom)
      : OB64.squad.patchRegions(sourceRom);
    compareRegions(candidateRom.z64, expected, romRegions(regions), 'runtime overrides', 'retail restoration');
    return {
      summary: 'Removed runtime overrides restored every owned ROM region.',
      details: { regionCount: romRegions(regions).length },
    };
  }

  function* buildReportSteps(options) {
    var started = Date.now();
    var sourceRom = options && options.sourceRom;
    var candidateRom = options && options.candidateRom;
    var dirty = options && options.dirty || {};
    var report = {
      schema: REPORT_SCHEMA,
      schemaVersion: REPORT_VERSION,
      generatedAt: new Date().toISOString(),
      editorVersion: OB64.consumableEffects && OB64.consumableEffects.EDITOR_VERSION || null,
      result: 'failed',
      ok: false,
      outputByteOrder: sourceRom && (sourceRom.exportByteOrder || sourceRom.byteOrder) || null,
      dirtyCategories: Object.keys(dirty).filter(function(key) { return !!dirty[key]; }).sort(),
      touched: (options && options.touched || []).slice(),
      source: null,
      candidate: null,
      checks: [],
      errors: [],
      changeRanges: [],
      durationMs: 0,
    };

    if (!sourceRom || !candidateRom || !(sourceRom.z64 instanceof Uint8Array) ||
        !(candidateRom.z64 instanceof Uint8Array)) {
      report.errors.push({
        code: 'VALIDATION_INPUT_MISSING',
        title: 'ROM validation could not start',
        message: 'The editor did not retain both the source ROM and the finished candidate.',
        suggestion: 'Reload the ROM and project, then export again.',
        technical: {
          hasSourceRom: !!sourceRom,
          hasCandidateRom: !!candidateRom,
          sourceHasImage: !!(sourceRom && sourceRom.z64 instanceof Uint8Array),
          candidateHasImage: !!(candidateRom && candidateRom.z64 instanceof Uint8Array),
        },
      });
      report.durationMs = Date.now() - started;
      return report;
    }

    try {
      report.source = imageIdentity(sourceRom);
      report.candidate = imageIdentity(candidateRom);
    } catch (identityError) {
      report.errors.push(issueFromException(identityError, {
        code: 'ROM_IDENTITY_FAILED',
        title: 'ROM identity could not be verified',
        message: 'The editor could not identify the source and finished ROM images.',
        suggestion: 'Reload the editor and export again.',
      }));
    }

    yield* runCheckStep(report, 'rom-envelope', 'ROM format and revision', {
      code: 'ROM_ENVELOPE_INVALID',
      title: 'ROM format check failed',
      message: 'The finished file no longer matches the loaded Ogre Battle 64 ROM format.',
      suggestion: 'Reload a supported US ROM and export again.',
    }, function() {
      if (sourceRom.z64.length !== candidateRom.z64.length) {
        throw issue(
          'ROM_SIZE_CHANGED',
          'ROM size changed',
          'The finished ROM is not the same size as the loaded source ROM.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { sourceSize: sourceRom.z64.length, candidateSize: candidateRom.z64.length }
        );
      }
      if (OB64.detectRomByteOrder(candidateRom.z64) !== 'z64') {
        throw issue(
          'ROM_NORMALIZED_MAGIC',
          'ROM header is damaged',
          'The finished ROM has an invalid internal N64 byte-order header.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { detectedByteOrder: OB64.detectRomByteOrder(candidateRom.z64) }
        );
      }
      var sourceLayout = OB64.detectRomLayout(sourceRom.z64);
      var candidateLayout = OB64.detectRomLayout(candidateRom.z64);
      if (!sourceLayout || !candidateLayout || sourceLayout.id !== candidateLayout.id) {
        throw issue(
          'ROM_REVISION_CHANGED',
          'ROM revision changed',
          'The finished ROM no longer identifies as the same supported game revision.',
          'Reload a supported US ROM and export again.',
          {
            sourceLayout: sourceLayout && sourceLayout.id,
            candidateLayout: candidateLayout && candidateLayout.id,
          }
        );
      }
      var outputOrder = sourceRom.exportByteOrder || sourceRom.byteOrder || 'v64';
      if (['v64', 'z64', 'n64'].indexOf(outputOrder) === -1) {
        throw issue(
          'ROM_BYTE_ORDER_UNSUPPORTED',
          'Output byte order is unsupported',
          'The editor does not recognize the selected ROM output byte order.',
          'Reload the original ROM and export again.',
          { outputByteOrder: outputOrder }
        );
      }
      return {
        summary: 'ROM size, normalized header, and game revision match the source.',
        details: { size: candidateRom.z64.length, layout: candidateLayout.id, outputByteOrder: outputOrder },
      };
    });

    yield* runCheckStep(report, 'cic-6102-checksum', 'N64 boot checksum', {
      code: 'ROM_CHECKSUM_INVALID',
      title: 'ROM checksum failed',
      message: 'The finished ROM has an invalid N64 boot checksum.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      var verification = OB64.consumableEffects.verifyIndependentCrc(candidateRom.z64);
      if (!verification.ok) {
        throw issue(
          'ROM_CHECKSUM_INVALID',
          'ROM checksum failed',
          'The finished ROM has an invalid N64 boot checksum.',
          'Keep the error report and recreate the ROM after updating the editor.',
          verification
        );
      }
      return {
        summary: 'The independent CIC-6102 checksum matches the ROM header.',
        details: verification,
      };
    });

    var serialized = yield* runCheckStep(report, 'deterministic-serialization', 'Output serialization', {
      code: 'ROM_SERIALIZATION_FAILED',
      title: 'ROM serialization failed',
      message: 'The editor could not reproduce the finished download bytes consistently.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      var outputOrder = sourceRom.exportByteOrder || sourceRom.byteOrder || 'v64';
      var first = OB64.serializeRomImage(candidateRom.z64, outputOrder);
      var second = OB64.serializeRomImage(candidateRom.z64, outputOrder);
      if (!equalBytes(first, second)) {
        throw issue(
          'ROM_SERIALIZATION_NONDETERMINISTIC',
          'ROM serialization changed between runs',
          'The editor produced different download bytes from the same finished ROM.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { firstSha256: sha256(first), secondSha256: sha256(second) }
        );
      }
      if (OB64.detectRomByteOrder(first) !== outputOrder) {
        throw issue(
          'ROM_BYTE_ORDER_MISMATCH',
          'ROM byte order is wrong',
          'The download bytes do not use the same N64 byte order as the loaded ROM.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { expected: outputOrder, actual: OB64.detectRomByteOrder(first) }
        );
      }
      var normalized = OB64.normalizeRomImage(first);
      if (!normalized.z64 || !equalBytes(normalized.z64, candidateRom.z64)) {
        throw issue(
          'ROM_SERIALIZATION_ROUNDTRIP',
          'ROM serialization did not round-trip',
          'The download bytes do not reopen as the finished ROM the editor validated.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { outputByteOrder: outputOrder }
        );
      }
      return {
        value: first,
        summary: 'Two serializations match and reopen in the requested byte order.',
        details: { byteOrder: outputOrder, size: first.length, sha256: sha256(first) },
      };
    });

    var targets = options.scenarioResult && options.scenarioResult.validationTargets || [];
    yield* runCheckStep(report, 'archive-catalog', 'ROM archive boundaries', {
      code: 'ARCHIVE_CATALOG_INVALID',
      title: 'ROM archive catalog failed',
      message: 'One or more established ROM archives are missing, invalid, or overlapping.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      return validateArchiveCatalog(sourceRom, candidateRom, targets);
    });

    if (!targets.length) {
      yield* runCheckStep(report, 'scenario-archive-roundtrip', 'Rebuilt scenario archives', {
        code: 'ARCHIVE_VALIDATION_FAILED',
        title: 'Scenario archive validation failed',
        message: 'A rebuilt scenario archive did not pass validation.',
        suggestion: 'Keep the error report and export again.',
      }, function() { return skipped('No scenario archive was rebuilt during this export.'); });
    } else {
      for (var targetIndex = 0; targetIndex < targets.length; targetIndex++) {
        var target = targets[targetIndex];
        yield* runCheckStep(report, 'scenario-archive-' + targetIndex, 'Rebuilt archive: ' + (target.label || target.archive), {
          code: 'ARCHIVE_VALIDATION_FAILED',
          title: 'Scenario archive validation failed',
          message: 'A rebuilt scenario archive did not pass its header, boundary, checksum, and extract-back checks.',
          suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
        }, function() { return validateArchiveTarget(candidateRom.z64, target); });
      }
    }

    yield* runCheckStep(report, 'source-redirect-integrity', 'Shared PI-source redirect', {
      code: 'SOURCE_REDIRECT_INTEGRITY',
      title: 'Shared ROM redirect failed',
      message: 'The finished ROM does not contain the exact planned PI-source redirect table.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      if (!options.sourceRedirectPlan) {
        return skipped('No shared PI-source redirect change or owned entry participated in this export.');
      }
      if (!OB64.sourceRedirect || !OB64.sourceRedirect.validate) {
        throw issue(
          'VALIDATOR_DEPENDENCY_MISSING',
          'Validation component is missing',
          'The editor could not load its shared PI-source redirect checker.',
          'Reload the editor and try the export again.',
          { dependency: 'OB64.sourceRedirect.validate' }
        );
      }
      var sharedRedirect = OB64.sourceRedirect.validate(
        candidateRom.z64,
        options.sourceRedirectPlan.requests || []
      );
      if (!sharedRedirect.ok) {
        throw issue(
          'SOURCE_REDIRECT_INTEGRITY',
          'Shared ROM redirect failed',
          'The finished ROM does not contain the exact planned PI-source redirect table.',
          'Keep the error report and recreate the ROM after updating the editor.',
          sharedRedirect
        );
      }
      return {
        summary: sharedRedirect.entryCount
          ? ('Verified ' + sharedRedirect.entryCount + ' shared PI-source redirect entries.')
          : 'The shared PI-source redirect is safely disabled.',
        details: sharedRedirect,
      };
    });

    yield* runCheckStep(report, 'stat-gate-relocation-integrity', 'Class-change stat-gate container', {
      code: 'STAT_GATE_RELOCATION_INTEGRITY',
      title: 'Stat-gate container failed',
      message: 'The finished ROM does not contain a complete bounded stat-gate container.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      if (!dirty.statGates) {
        return skipped('Class-change stat gates did not change during this export.');
      }
      if (!OB64.statGateRelocation || !OB64.statGateRelocation.validate) {
        throw issue(
          'VALIDATOR_DEPENDENCY_MISSING',
          'Validation component is missing',
          'The editor could not load its stat-gate relocation checker.',
          'Reload the editor and try the export again.',
          { dependency: 'OB64.statGateRelocation.validate' }
        );
      }
      var intendedStatBytes = OB64.statGateRelocation.buildDecoded(
        sourceRom.statGates
      );
      var statValidation = OB64.statGateRelocation.validate(
        candidateRom.z64,
        candidateRom.layout || sourceRom.layout,
        intendedStatBytes
      );
      if (!statValidation.ok) {
        throw issue(
          'STAT_GATE_RELOCATION_INTEGRITY',
          'Stat-gate container failed',
          'The finished ROM does not contain a complete bounded stat-gate container.',
          'Keep the error report and recreate the ROM after updating the editor.',
          statValidation
        );
      }
      if (options.statGatePlan) {
        var expectedState = options.statGatePlan.mode === 'relocated'
          ? 'owned'
          : 'in-place';
        if (statValidation.state !== expectedState ||
            statValidation.logicalStreamBytes !==
              options.statGatePlan.logicalStreamBytes ||
            statValidation.payloadBytes !== options.statGatePlan.payloadBytes ||
            statValidation.containerBytes !==
              options.statGatePlan.containerBytes) {
          throw issue(
            'STAT_GATE_RELOCATION_INTEGRITY',
            'Stat-gate container failed',
            'The finished stat-gate container sizes or ownership state differ from the export plan.',
            'Keep the error report and recreate the ROM after updating the editor.',
            {
              expectedState: expectedState,
              expectedLogicalStreamBytes: options.statGatePlan.logicalStreamBytes,
              expectedPayloadBytes: options.statGatePlan.payloadBytes,
              expectedContainerBytes: options.statGatePlan.containerBytes,
              actual: statValidation,
            }
          );
        }
      }
      return {
        summary: statValidation.active
          ? ('Verified relocated stat gates: ' +
            statValidation.logicalStreamBytes + ' logical, ' +
            statValidation.payloadBytes + ' payload, ' +
            statValidation.containerBytes + ' container bytes.')
          : ('Verified in-place stat gates: ' +
            statValidation.logicalStreamBytes + ' logical bytes and no relocation artifact.'),
        details: statValidation,
      };
    });

    yield* runCheckStep(report, 'scenario-relocation-integrity', 'Scenario archive redirect', {
      code: 'SCENARIO_RELOCATION_INTEGRITY',
      title: 'Scenario archive redirect failed',
      message: 'The finished ROM does not contain a complete scenario archive redirect.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      if (!options.scenarioResult) {
        return skipped('No scenario archive export ran during this ROM build.');
      }
      if (!OB64.scenario || !OB64.scenario.validateRelocationRedirect) {
        throw issue(
          'VALIDATOR_DEPENDENCY_MISSING',
          'Validation component is missing',
          'The editor could not load its scenario archive redirect checker.',
          'Reload the editor and try the export again.',
          { dependency: 'OB64.scenario.validateRelocationRedirect' }
        );
      }
      var redirect = OB64.scenario.validateRelocationRedirect(
        candidateRom,
        options.scenarioResult.relocations || [],
        !!options.scenarioResult.crc
      );
      if (!redirect.ok) {
        throw issue(
          'SCENARIO_RELOCATION_INTEGRITY',
          'Scenario archive redirect failed',
          'The finished ROM does not contain a complete scenario archive redirect.',
          'Keep the error report and recreate the ROM after updating the editor.',
          redirect
        );
      }
      return {
        summary: redirect.entryCount ?
          ('Verified ' + redirect.entryCount + ' scenario archive redirect entries.') :
          'The scenario archive redirect is safely disabled.',
        details: redirect,
      };
    });

    yield* runCheckStep(report, 'authorized-write-audit', 'Authorized ROM changes and patch collisions', {
      code: 'ROM_CHANGE_AUDIT_FAILED',
      title: 'ROM change audit failed',
      message: 'The finished ROM contains a change that could not be assigned safely to one editor feature.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      try {
        report.changeRanges = OB64.consumableEffects.buildChangeRanges(
          sourceRom.z64,
          candidateRom.z64,
          options.owners || []
        );
      } catch (auditError) {
        var message = auditError && auditError.message || String(auditError);
        if (message.indexOf('Concrete patch owner collision') !== -1) {
          throw issue(
            'PATCH_COLLISION',
            'Editor features overlap',
            'Two editor features tried to change the same ROM bytes.',
            'Disable one conflicting feature, then export again.',
            { exceptionMessage: message }
          );
        }
        if (message.indexOf('Unexplained candidate delta') !== -1) {
          throw issue(
            'UNPLANNED_ROM_WRITE',
            'Unexpected ROM change found',
            'The finished ROM contains changed bytes that no export feature claimed.',
            'Keep the error report and recreate the ROM after updating the editor.',
            { exceptionMessage: message }
          );
        }
        throw auditError;
      }
      if (!(options.touched || []).length && report.changeRanges.length) {
        throw issue(
          'NOOP_IDENTITY_FAILED',
          'No-change export altered the ROM',
          'The editor changed ROM bytes even though no feature reported an export change.',
          'Keep the error report and recreate the ROM after updating the editor.',
          { changeRangeCount: report.changeRanges.length }
        );
      }
      return {
        summary: report.changeRanges.length + ' changed ROM range' +
          (report.changeRanges.length === 1 ? '' : 's') +
          ' belong to one authorized export feature each.',
        details: { changeRangeCount: report.changeRanges.length },
      };
    });

    var reloadedRom = null;
    if (serialized) {
      reloadedRom = yield* runCheckStep(report, 'rom-reopen', 'Finished ROM reopen', {
        code: 'ROM_REOPEN_FAILED',
        title: 'Finished ROM could not be reopened',
        message: 'The editor could not reopen and parse the finished ROM image.',
        suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
      }, function() {
        var loaded = OB64.loadROM(serialized.slice().buffer);
        if (!loaded.layout || !sourceRom.layout || loaded.layout.id !== sourceRom.layout.id ||
            !equalBytes(loaded.z64, candidateRom.z64)) {
          throw issue(
            'ROM_REOPEN_IDENTITY',
            'Finished ROM reopened differently',
            'The downloaded byte order does not reopen as the exact ROM that was validated.',
            'Keep the error report and recreate the ROM after updating the editor.',
            {
              expectedLayout: sourceRom.layout && sourceRom.layout.id,
              actualLayout: loaded.layout && loaded.layout.id,
              normalizedBytesMatch: equalBytes(loaded.z64, candidateRom.z64),
            }
          );
        }
        var keyArchives = [647, 691, 751];
        for (var i = 0; i < keyArchives.length; i++) {
          var index = keyArchives[i];
          if (!sourceRom.archives[index] || !loaded.archives[index] ||
              sourceRom.archives[index].offset !== loaded.archives[index].offset) {
            throw issue(
              'ARCHIVE_INDEX_SHIFT',
              'ROM archive index shifted',
              'The finished ROM no longer resolves a required game archive at its established index.',
              'Keep the error report and recreate the ROM after updating the editor.',
              {
                archive: index,
                expectedOffset: sourceRom.archives[index] && hex(sourceRom.archives[index].offset),
                actualOffset: loaded.archives[index] && hex(loaded.archives[index].offset),
              }
            );
          }
        }
        return {
          value: loaded,
          summary: 'The serialized ROM reopened as the same revision and normalized image.',
          details: { layout: loaded.layout.id, archiveCount: loaded.archives.length },
        };
      });
    } else {
      yield* runCheckStep(report, 'rom-reopen', 'Finished ROM reopen', {
        code: 'ROM_REOPEN_FAILED',
        title: 'Finished ROM could not be reopened',
        message: 'The editor could not reopen and parse the finished ROM image.',
        suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
      }, function() { return skipped('Output serialization failed, so reopening was not attempted.'); });
    }

    var semanticSpecs = [
      {
        dirty: 'items', property: 'itemStats', label: 'item', serializer: OB64.serializeItemStats,
        regions: [{ start: OB64.ITEM_STAT_OFFSET - 4,
          size: OB64.ITEM_STAT_COUNT * OB64.ITEM_STAT_SIZE + 4, label: 'item records' }],
      },
      {
        dirty: 'classDefs', property: 'classDefs', label: 'class', serializer: OB64.serializeClassDefs,
        regions: [{ start: OB64.CLASS_DEF_OFFSET,
          size: OB64.CLASS_DEF_TOTAL * OB64.CLASS_DEF_RECORD_SIZE, label: 'class records' }],
      },
      {
        dirty: 'encounters', property: 'neutralEncounters', label: 'neutral encounter',
        serializer: OB64.serializeNeutralEncounters,
        regions: [
          { start: OB64.NEUTRAL_TERRAIN_RATE_OFFSET, size: 0x40, label: 'terrain encounter tables' },
          { start: OB64.NEUTRAL_ENCOUNTER_OFFSET, size: 0x330, label: 'neutral encounter records' },
          { start: OB64.NEUTRAL_GLOBAL_DIV_HI_OFFSET, size: 0x44, label: 'global encounter roll code' },
        ],
        prepareReloaded: function(loaded, source) {
          if (loaded.neutralEncounters && loaded.neutralEncounters.globalRate &&
              source.neutralEncounters && source.neutralEncounters.globalRate) {
            loaded.neutralEncounters.globalRate.modified =
              !!source.neutralEncounters.globalRate.modified;
          }
        },
      },
      {
        dirty: 'creatureDrops', property: 'creatureDrops', label: 'creature drop',
        serializer: OB64.serializeCreatureDrops,
        regions: [{ start: OB64.CREATURE_DROP_OFFSET,
          size: OB64.CREATURE_DROP_COUNT * OB64.CREATURE_DROP_STRIDE, label: 'creature drop records' }],
      },
      {
        dirty: 'consumables', property: 'consumables', label: 'consumable',
        serializer: OB64.serializeConsumables,
        regions: [{ start: OB64.CONSUMABLE_TABLE_OFFSET,
          size: sourceRom.consumables.length * OB64.CONSUMABLE_RECORD_SIZE, label: 'consumable records' }],
      },
      {
        dirty: 'itemDescriptions', property: 'itemDescriptions',
        label: 'item description', serializer: OB64.serializeItemDescriptions,
        regions: sourceRom.itemDescriptions && OB64.descriptionCodec
          ? [OB64.descriptionCodec.ownerRegion(sourceRom.itemDescriptions)] : [],
      },
      {
        dirty: 'consumableDescriptions', property: 'consumableDescriptions',
        label: 'consumable description', serializer: OB64.serializeConsumableDescriptions,
        regions: sourceRom.consumableDescriptions && OB64.descriptionCodec
          ? [OB64.descriptionCodec.ownerRegion(sourceRom.consumableDescriptions)] : [],
      },
      {
        dirty: 'classDescriptions', property: 'classDescriptions',
        label: 'class description', serializer: OB64.serializeClassDescriptions,
        regions: sourceRom.classDescriptions && OB64.descriptionCodec
          ? [OB64.descriptionCodec.ownerRegion(sourceRom.classDescriptions)] : [],
      },
      {
        dirty: 'actionDescriptions', property: 'actionDescriptions',
        label: 'action description', serializer: OB64.serializeActionDescriptions,
        regions: sourceRom.actionDescriptions && OB64.descriptionCodec
          ? [OB64.descriptionCodec.ownerRegion(sourceRom.actionDescriptions)] : [],
      },
      {
        dirty: 'statGates', property: 'statGates', label: 'class-change stat gate',
        serializer: OB64.serializeStatGatesForComparison ||
          OB64.serializeStatGates,
        regions: sourceRom.statGates && sourceRom.statGates.meta &&
          OB64.statGateRelocation
          ? OB64.statGateRelocation.patchRegions(sourceRom.statGates)
          : (sourceRom.statGates && sourceRom.statGates.meta ? [{
            start: sourceRom.statGates.meta.compDataOff - 8,
            size: sourceRom.statGates.meta.compDataSize + 8,
            label: 'stat-gate compressed slot',
          }] : []),
      },
    ];

    for (var semanticIndex = 0; semanticIndex < semanticSpecs.length; semanticIndex++) {
      var spec = semanticSpecs[semanticIndex];
      yield* runCheckStep(report, 'semantic-readback-' + spec.dirty, spec.label + ' semantic readback', {
        code: 'SEMANTIC_READBACK_MISMATCH',
        title: 'Saved values did not read back correctly',
        message: 'The finished ROM does not read back the intended ' + spec.label + ' values.',
        suggestion: 'Keep the error report, reload the project, and export again.',
      }, function() {
        if (!dirty[spec.dirty]) return skipped('This feature did not change during the export.');
        if (!reloadedRom) {
          return skipped('The ROM could not be reopened, so semantic readback was not attempted.');
        }
        if (!spec.serializer || !spec.regions.length) {
          throw issue(
            'SEMANTIC_READBACK_UNAVAILABLE',
            'Saved values could not be verified',
            'The editor lacks the serializer or ROM range needed to verify ' + spec.label + ' values.',
            'Keep the error report and recreate the ROM after updating the editor.',
            { feature: spec.property }
          );
        }
        return validateSemanticRoundTrip({
          sourceRom: sourceRom,
          candidateRom: candidateRom,
          reloadedRom: reloadedRom,
          property: spec.property,
          label: spec.label,
          serializer: spec.serializer,
          regions: spec.regions,
          prepareReloaded: spec.prepareReloaded,
        });
      });
    }

    yield* runCheckStep(report, 'runtime-override-integrity', 'Shops and squads runtime patch', {
      code: 'PATCH_INTEGRITY',
      title: 'Runtime override patch did not verify',
      message: 'The finished ROM does not contain the complete planned shops and squads runtime patch.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      if (options.runtimeWritePlan) {
        return validateExactWrites(candidateRom.z64, options.runtimeWritePlan.writes, 'shops and squads runtime');
      }
      if (options.runtimeMode === 'restore') return validateRuntimeRemoval(sourceRom, candidateRom);
      return skipped('No shops or squads runtime patch changed during this export.');
    });

    yield* runCheckStep(report, 'shop-override-readback', 'Runtime shop semantic readback', {
      code: 'SEMANTIC_READBACK_MISMATCH',
      title: 'Shop overrides did not read back correctly',
      message: 'The finished ROM does not contain the intended per-shop item lists.',
      suggestion: 'Keep the error report, review the affected shops, and export again.',
    }, function() {
      if (!dirty.shops && !options.runtimeWritePlan && options.runtimeMode !== 'restore') {
        return skipped('Runtime shops did not change during this export.');
      }
      return validateShopOverrides(candidateRom, options.shopOverrides || []);
    });

    yield* runCheckStep(report, 'tools-patch-integrity', 'Tools patch integrity', {
      code: 'PATCH_INTEGRITY',
      title: 'Tool patch did not verify',
      message: 'The finished ROM does not contain a complete requested Tools-tab patch.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      if (!options.toolsResult) return skipped('No Tools-tab patch changed during this export.');
      return validateTools(candidateRom, sourceRom, options.toolsResult);
    });

    yield* runCheckStep(report, 'combat-animation-integrity', 'Attack-animation patch integrity', {
      code: 'PATCH_INTEGRITY',
      title: 'Attack-animation patch did not verify',
      message: 'The finished ROM does not contain the intended attack-animation mappings.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      if (!options.selectorPlan || options.selectorPlan.mode === 'none') {
        return skipped('Attack-animation overrides did not change during this export.');
      }
      return validateCombatOverrides(candidateRom, options.selectorPlan);
    });

    yield* runCheckStep(report, 'consumable-effect-integrity', 'Consumable-effect patch integrity', {
      code: 'PATCH_INTEGRITY',
      title: 'Consumable-effect patch did not verify',
      message: 'The finished ROM does not contain the intended consumable-effect code and text.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
    }, function() {
      if (!options.effectTransaction) {
        return skipped('Consumable-effect code did not change during this export.');
      }
      return validateEffectTransaction(candidateRom, sourceRom, options.effectTransaction);
    });

    report.ok = report.errors.length === 0;
    report.result = report.ok ? 'passed' : 'failed';
    report.durationMs = Date.now() - started;
    return report;
  }

  function buildReport(options) {
    var iterator = buildReportSteps(options || {});
    var step = iterator.next();
    while (!step.done) step = iterator.next();
    return step.value;
  }

  function validationStepCount(options) {
    var targets = options && options.scenarioResult &&
      options.scenarioResult.validationTargets || [];
    return 18 + Math.max(targets.length, 1);
  }

  function yieldForProgressPaint() {
    return new Promise(function(resolve) {
      if (typeof requestAnimationFrame === 'function') {
        requestAnimationFrame(function() { setTimeout(resolve, 0); });
      } else {
        setTimeout(resolve, 0);
      }
    });
  }

  async function buildReportAsync(options, onProgress) {
    options = options || {};
    var iterator = buildReportSteps(options);
    var total = validationStepCount(options);
    var index = 0;
    var step = iterator.next();
    while (!step.done) {
      index++;
      if (typeof onProgress === 'function') {
        try {
          onProgress({
            phase: 'validation',
            id: step.value.id,
            name: step.value.name,
            index: index,
            total: total,
            complete: false,
          });
        } catch (progressError) {
          // A presentation-only callback must never weaken ROM validation.
        }
      }
      await yieldForProgressPaint();
      step = iterator.next();
    }
    if (typeof onProgress === 'function') {
      try {
        onProgress({
          phase: 'validation',
          id: 'validation-complete',
          name: 'ROM validation complete',
          index: total,
          total: total,
          complete: true,
        });
      } catch (completeProgressError) {
        // A presentation-only callback must never weaken ROM validation.
      }
    }
    return step.value;
  }

  function unexpectedReport(error, stage) {
    var found = error && error.message || String(error);
    return {
      schema: REPORT_SCHEMA,
      schemaVersion: REPORT_VERSION,
      generatedAt: new Date().toISOString(),
      editorVersion: OB64.consumableEffects && OB64.consumableEffects.EDITOR_VERSION || null,
      result: 'failed',
      ok: false,
      outputByteOrder: null,
      dirtyCategories: [],
      touched: [],
      source: null,
      candidate: null,
      checks: [{
        id: 'unexpected-' + (stage || 'export'),
        name: 'Unexpected export failure',
        status: 'failed',
        summary: 'The editor stopped because an unexpected export error occurred.',
        durationMs: 0,
        details: { stage: stage || 'export', exceptionMessage: found },
      }],
      errors: [{
        code: 'UNEXPECTED_EXPORT_ERROR',
        title: 'Unexpected export error',
        message: 'The editor stopped before downloading the ROM because an unexpected error occurred.',
        suggestion: 'Download the error report, then keep the project and source ROM for diagnosis.',
        technical: {
          stage: stage || 'export',
          exceptionName: error && error.name || 'Error',
          exceptionMessage: found,
          stack: error && error.stack || null,
        },
      }],
      changeRanges: [],
      durationMs: 0,
    };
  }

  OB64.romExportValidator = {
    REPORT_SCHEMA: REPORT_SCHEMA,
    REPORT_VERSION: REPORT_VERSION,
    validate: buildReport,
    validateAsync: buildReportAsync,
    unexpectedReport: unexpectedReport,
    _test: {
      parseArchiveAt: parseArchiveAt,
      validateCommonHeaderCrc: validateCommonHeaderCrc,
      validateArchiveTarget: validateArchiveTarget,
      validateArchiveCatalog: validateArchiveCatalog,
      equalBytes: equalBytes,
    },
  };
})(window.OB64);
