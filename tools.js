// OB64 Mod Editor - Tools engine
//
// Byte-level ROM feature settings for the Tools tab. Feature definitions live
// in tools-data.js (generated from the verified research builds); this file
// is the hand-written engine that detects, applies, and removes them.
//
// Each feature is a set of writes into the z64 buffer. A write knows both its
// patched bytes and the original retail bytes (or a zero-filled cave), so a
// feature can be detected in a loaded ROM, applied on export, or removed
// again by restoring the originals. Anything else occupying those bytes means
// some other patch touched them -- the feature reports 'foreign' and the UI
// refuses to toggle it rather than corrupt an unknown mod.

window.OB64 = window.OB64 || {};

(function() {

  function hexToBytes(hex) {
    var out = new Uint8Array(hex.length / 2);
    for (var i = 0; i < out.length; i++) {
      out[i] = parseInt(hex.substr(i * 2, 2), 16);
    }
    return out;
  }

  function patchedBytes(write) {
    if (!write._patched) write._patched = hexToBytes(write.patched);
    return write._patched;
  }

  function originalBytes(write) {
    if (!write._original) {
      write._original = write.original
        ? hexToBytes(write.original)
        : new Uint8Array(write.originalZeros);
    }
    return write._original;
  }

  function isParameterized(feature) {
    return !!(feature && feature.kind === 'percent-scale' && feature.parameter);
  }

  function parameterDefault(feature) {
    return Number(feature.parameter.default);
  }

  function validateParameterValue(feature, value) {
    if (!isParameterized(feature)) {
      throw new Error((feature && feature.name || 'Tool') + ' is not a percentage setting.');
    }
    var spec = feature.parameter;
    var numeric = Number(value);
    var step = Number(spec.step);
    if (!Number.isFinite(numeric) || !Number.isInteger(numeric) ||
        numeric < Number(spec.min) || numeric > Number(spec.max) ||
        !Number.isFinite(step) || step <= 0 ||
        Math.abs((numeric - Number(spec.min)) / step -
          Math.round((numeric - Number(spec.min)) / step)) > 1e-9) {
      throw new Error(feature.name + ' must be an integer percentage from ' +
        spec.min + '% through ' + spec.max + '% in ' + spec.step + '% steps.');
    }
    return numeric;
  }

  function float32Bits(value) {
    var buffer = new ArrayBuffer(4);
    var view = new DataView(buffer);
    view.setFloat32(0, value, false);
    return view.getUint32(0, false) >>> 0;
  }

  function float32FromBits(value) {
    var buffer = new ArrayBuffer(4);
    var view = new DataView(buffer);
    view.setUint32(0, value >>> 0, false);
    return view.getFloat32(0, false);
  }

  function parameterBits(feature, percent) {
    var divisor = Number(feature.parameter.encoding.divisor || 100);
    return float32Bits(percent / divisor);
  }

  function dynamicByteOffsets(feature, writeIndex) {
    if (!isParameterized(feature)) return [];
    var encoding = feature.parameter.encoding || {};
    return Number(encoding.writeIndex) === writeIndex
      ? (encoding.byteOffsets || []).map(Number)
      : [];
  }

  function validateParameterDefinition(feature) {
    if (!isParameterized(feature)) return;
    var spec = feature.parameter;
    var encoding = spec.encoding || {};
    validateParameterValue(feature, parameterDefault(feature));
    if (spec.schema !== 1 || spec.type !== 'percent' ||
        encoding.type !== 'mips-f32-lui-ori' ||
        !Number.isInteger(Number(encoding.writeIndex)) ||
        !Array.isArray(encoding.byteOffsets) || encoding.byteOffsets.length !== 4 ||
        !Number.isFinite(Number(encoding.divisor)) || Number(encoding.divisor) <= 0) {
      throw new Error('Invalid percentage encoding metadata for ' + feature.name + '.');
    }
    var write = feature.writes[Number(encoding.writeIndex)];
    var offsets = encoding.byteOffsets.map(Number);
    if (!write || offsets.some(function(offset) {
      return !Number.isInteger(offset) || offset < 0 || offset >= patchedLength(write);
    })) {
      throw new Error('Percentage encoding bytes fall outside ' + feature.name + '.');
    }
  }

  function hex(n, width) {
    var s = (n >>> 0).toString(16).toUpperCase();
    while (width && s.length < width) s = '0' + s;
    return '0x' + s;
  }

  function patchedLength(write) {
    return patchedBytes(write).length;
  }

  function assertRomOffset(z64, offset, len, label) {
    if (!Number.isFinite(offset) || offset < 0) {
      throw new Error('Invalid ROM write offset for ' + label + ': ' + offset);
    }
    if (offset >= 0x80000000) {
      throw new Error('Patch write for ' + label + ' uses a RAM address as a ROM offset: ' + hex(offset, 8));
    }
    if (offset + len > z64.length) {
      throw new Error('Patch write for ' + label + ' exceeds ROM length: ' +
        hex(offset, 6) + ' + ' + len + ' > ' + z64.length);
    }
  }

  function regionEquals(z64, offset, bytes) {
    assertRomOffset(z64, offset, bytes.length, 'feature detection');
    for (var i = 0; i < bytes.length; i++) {
      if (z64[offset + i] !== bytes[i]) return false;
    }
    return true;
  }

  function writeRegion(z64, offset, bytes) {
    assertRomOffset(z64, offset, bytes.length, 'feature apply');
    z64.set(bytes, offset);
  }

  function normalizeRegion(owner, region, fromWrite) {
    if (!region) return null;
    var start = Number(region.start != null ? region.start : region.offset);
    var size = Number(region.size != null ? region.size : region.length);
    var kind = region.kind || (fromWrite ? 'rom' : null);
    if (!kind || !Number.isFinite(start) || !Number.isFinite(size) || size <= 0) return null;
    return {
      ownerId: owner.id,
      ownerName: owner.name,
      kind: kind,
      start: start,
      end: start + size,
      size: size,
      label: region.label || (fromWrite ? 'ROM write' : 'region'),
    };
  }

  function featureRegions(feature) {
    var out = [];
    var writes = feature.writes || [];
    for (var i = 0; i < writes.length; i++) {
      out.push(normalizeRegion(feature, {
        kind: 'rom',
        start: writes[i].offset,
        size: patchedLength(writes[i]),
        label: writes[i].label || ('write ' + i),
      }, true));
    }
    var explicit = feature.regions || [];
    for (var r = 0; r < explicit.length; r++) {
      var nr = normalizeRegion(feature, explicit[r], false);
      if (nr) out.push(nr);
    }
    return out.filter(Boolean);
  }

  function featureUnsupportedReason(rom, feature) {
    if (!rom || !rom.layout || !feature) return '';
    if (rom.layout.supportsTools === false) {
      return rom.layout.unsupportedFeaturesReason || 'Tools are not available for this ROM revision.';
    }
    var blocked = rom.layout.unsupportedTools || {};
    if (Object.prototype.hasOwnProperty.call(blocked, feature.id)) {
      return blocked[feature.id] || 'This tool is not available for this ROM revision.';
    }
    var allowed = rom.layout.supportedTools || null;
    if (allowed && allowed[feature.id] !== true) {
      return 'This tool has not been enabled for this ROM revision.';
    }
    return '';
  }

  function featureSupported(rom, feature) {
    return !featureUnsupportedReason(rom, feature);
  }

  function rangesOverlap(a, b) {
    return a.kind === b.kind && a.start < b.end && b.start < a.end;
  }

  function explicitlyExclusive(a, b) {
    var ax = a.exclusiveWith || [];
    var bx = b.exclusiveWith || [];
    return ax.indexOf(b.id) !== -1 || bx.indexOf(a.id) !== -1;
  }

  function describeConflict(c) {
    return c.a.ownerName + ' ' + c.a.kind + ' ' + hex(c.a.start, 6) + '..' + hex(c.a.end - 1, 6) +
      ' (' + c.a.label + ') overlaps ' +
      c.b.ownerName + ' ' + c.b.kind + ' ' + hex(c.b.start, 6) + '..' + hex(c.b.end - 1, 6) +
      ' (' + c.b.label + ')';
  }

  function findRegionConflicts(regionOwners, options) {
    options = options || {};
    var owners = regionOwners || [];
    var regions = [];
    var byId = {};
    for (var i = 0; i < owners.length; i++) {
      byId[owners[i].id] = owners[i];
      var rs = owners[i].regions || featureRegions(owners[i]);
      for (var r = 0; r < rs.length; r++) {
        var nr = rs[r].ownerId ? rs[r] : normalizeRegion(owners[i], rs[r], false);
        if (nr) regions.push(nr);
      }
    }
    var out = [];
    for (var a = 0; a < regions.length; a++) {
      for (var b = a + 1; b < regions.length; b++) {
        if (regions[a].ownerId === regions[b].ownerId) continue;
        if (!rangesOverlap(regions[a], regions[b])) continue;
        var ownerA = byId[regions[a].ownerId] || {};
        var ownerB = byId[regions[b].ownerId] || {};
        var exclusive = explicitlyExclusive(ownerA, ownerB);
        if (!exclusive || options.includeExclusive) {
          out.push({ a: regions[a], b: regions[b], exclusive: exclusive });
        }
      }
    }
    return out;
  }

  function runtimeRegistry() {
    return OB64.RUNTIME_PATCH_REGISTRY || null;
  }

  function regionContained(inner, outer) {
    var a = inner.ownerId ? inner : normalizeRegion({ id: 'inner', name: 'inner' }, inner, false);
    var b = outer.ownerId ? outer : normalizeRegion({ id: 'outer', name: 'outer' }, outer, false);
    return !!a && !!b && a.kind === b.kind && a.start >= b.start && a.end <= b.end;
  }

  function validateFeatureRegistry(z64) {
    var list = features();
    for (var i = 0; i < list.length; i++) {
      validateParameterDefinition(list[i]);
      var writes = list[i].writes || [];
      for (var w = 0; w < writes.length; w++) {
        assertRomOffset(z64, writes[w].offset, patchedLength(writes[w]), list[i].name + ' / ' + (writes[w].label || w));
      }
    }
    var conflicts = findRegionConflicts(list, { includeExclusive: false });
    if (conflicts.length) {
      throw new Error('Tool patch region collision:\n  ' + conflicts.map(describeConflict).join('\n  '));
    }
    var registry = runtimeRegistry();
    if (!registry) throw new Error('The central runtime patch ownership registry is unavailable.');
    var owners = registry.owners || [];
    var ownerById = {};
    for (var oi = 0; oi < owners.length; oi++) {
      if (ownerById[owners[oi].id]) {
        throw new Error('Duplicate central runtime owner id: ' + owners[oi].id);
      }
      ownerById[owners[oi].id] = owners[oi];
    }
    var registryConflicts = findRegionConflicts(owners, { includeExclusive: false });
    if (registryConflicts.length) {
      throw new Error('Central runtime patch region collision:\n  ' +
        registryConflicts.map(describeConflict).join('\n  '));
    }
    for (var fi = 0; fi < list.length; fi++) {
      var owner = ownerById[list[fi].id];
      if (!owner) throw new Error('Tool feature has no central runtime owner: ' + list[fi].id);
      var regions = featureRegions(list[fi]);
      for (var ri = 0; ri < regions.length; ri++) {
        var covered = false;
        for (var cr = 0; cr < (owner.regions || []).length; cr++) {
          if (regionContained(regions[ri], owner.regions[cr])) {
            covered = true;
            break;
          }
        }
        if (!covered) {
          throw new Error('Tool feature region is missing from central ownership: ' +
            list[fi].id + ' / ' + regions[ri].label);
        }
      }
    }
  }

  function desiredRegionConflicts(rom, extraOwners) {
    if (!rom || !rom.tools) return [];
    var owners = [];
    var list = features();
    for (var i = 0; i < list.length; i++) {
      if (rom.tools.initial[list[i].id] === 'foreign') continue;
      if (!featureSupported(rom, list[i])) continue;
      if (isParameterized(list[i])) {
        if (desiredPercent(rom, list[i]) !== parameterDefault(list[i])) owners.push(list[i]);
      } else if (rom.tools.desired[list[i].id]) {
        owners.push(list[i]);
      }
    }
    if (extraOwners && extraOwners.length) owners = owners.concat(extraOwners);
    return findRegionConflicts(owners, { includeExclusive: true });
  }

  function assertDesiredCompatible(rom, extraOwners) {
    var conflicts = desiredRegionConflicts(rom, extraOwners);
    if (conflicts.length) {
      throw new Error('Selected patches cannot be enabled together:\n  ' +
        conflicts.map(describeConflict).join('\n  '));
    }
  }

  function writesMatch(z64, writes, key) {
    for (var i = 0; i < writes.length; i++) {
      var bytes = key === 'patched' ? patchedBytes(writes[i]) : originalBytes(writes[i]);
      if (!regionEquals(z64, writes[i].offset, bytes)) return false;
    }
    return true;
  }

  function parameterPatchedValue(z64, feature) {
    if (!isParameterized(feature)) return null;
    var encoding = feature.parameter.encoding;
    var encodedWriteIndex = Number(encoding.writeIndex);
    var encodedOffsets = dynamicByteOffsets(feature, encodedWriteIndex);
    for (var wi = 0; wi < feature.writes.length; wi++) {
      var write = feature.writes[wi];
      var expected = patchedBytes(write);
      assertRomOffset(z64, write.offset, expected.length, feature.name + ' detection');
      var ignored = dynamicByteOffsets(feature, wi);
      for (var bi = 0; bi < expected.length; bi++) {
        if (ignored.indexOf(bi) !== -1) continue;
        if (z64[write.offset + bi] !== expected[bi]) return null;
      }
    }

    var encodedWrite = feature.writes[encodedWriteIndex];
    var encoded = 0;
    for (var oi = 0; oi < encodedOffsets.length; oi++) {
      encoded = ((encoded << 8) | z64[encodedWrite.offset + encodedOffsets[oi]]) >>> 0;
    }
    var divisor = Number(encoding.divisor || 100);
    var percent = Math.round(float32FromBits(encoded) * divisor);
    try {
      validateParameterValue(feature, percent);
    } catch (_error) {
      return null;
    }
    return parameterBits(feature, percent) === encoded ? percent : null;
  }

  function parameterValue(z64, feature) {
    if (!isParameterized(feature)) return null;
    if (writesMatch(z64, feature.writes, 'original')) return parameterDefault(feature);
    return parameterPatchedValue(z64, feature);
  }

  function desiredPercent(rom, feature) {
    if (!rom || !rom.tools || !isParameterized(feature)) return null;
    var values = rom.tools.desiredValues || {};
    return values[feature.id] == null
      ? parameterDefault(feature)
      : Number(values[feature.id]);
  }

  function setDesiredPercent(rom, featureId, value) {
    var feature = getFeature(featureId);
    var percent = validateParameterValue(feature, value);
    if (!rom || !rom.tools) throw new Error('Load a ROM before changing a Tool setting.');
    rom.tools.desiredValues = rom.tools.desiredValues || {};
    rom.tools.desiredValues[featureId] = percent;
    rom.tools.desired[featureId] = percent !== parameterDefault(feature);
    return percent;
  }

  function parameterizedWrites(feature, percent) {
    percent = validateParameterValue(feature, percent);
    var encoded = parameterBits(feature, percent);
    var encoding = feature.parameter.encoding;
    var writeIndex = Number(encoding.writeIndex);
    var offsets = dynamicByteOffsets(feature, writeIndex);
    var encodedBytes = [
      (encoded >>> 24) & 0xFF,
      (encoded >>> 16) & 0xFF,
      (encoded >>> 8) & 0xFF,
      encoded & 0xFF,
    ];
    return feature.writes.map(function(write, index) {
      var bytes = patchedBytes(write).slice();
      if (index === writeIndex) {
        for (var i = 0; i < offsets.length; i++) bytes[offsets[i]] = encodedBytes[i];
      }
      return { offset: write.offset, bytes: bytes };
    });
  }

  function effectiveWeights(feature, percent) {
    if (!isParameterized(feature)) return [];
    percent = validateParameterValue(feature, percent);
    return (feature.parameter.effectiveWeights || []).map(function(weight) {
      return {
        size: weight.size,
        label: weight.label,
        percent: Number(weight.retailPercent) * percent / 100,
      };
    });
  }

  // Older shipped build whose bytes fully match the ROM, if any.
  function matchedSuperseded(z64, feature) {
    var list = feature.superseded || [];
    for (var i = 0; i < list.length; i++) {
      if (writesMatch(z64, list[i].writes, 'patched')) return list[i];
    }
    return null;
  }

  // 'applied'  - every write region holds the current patched bytes
  // 'clean'    - every write region holds the original retail bytes
  // 'outdated' - the regions hold a known older build of this feature
  // 'foreign'  - anything else (partial apply, other mod, unknown build)
  function featureState(z64, feature) {
    if (isParameterized(feature)) {
      if (writesMatch(z64, feature.writes, 'original')) return 'clean';
      if (parameterPatchedValue(z64, feature) !== null) return 'applied';
      return 'foreign';
    }
    if (writesMatch(z64, feature.writes, 'patched')) return 'applied';
    if (writesMatch(z64, feature.writes, 'original')) return 'clean';
    if (matchedSuperseded(z64, feature)) return 'outdated';
    return 'foreign';
  }

  function features() {
    var registry = runtimeRegistry();
    return (registry && registry.toolFeatures) || OB64.TOOLS_FEATURES || [];
  }

  function getFeature(id) {
    var list = features();
    for (var i = 0; i < list.length; i++) {
      if (list[i].id === id) return list[i];
    }
    return null;
  }

  // Call once after OB64.loadROM(). Stores per-feature state on the rom:
  //   rom.tools.initial[id]       - state detected in the loaded ROM
  //   rom.tools.desired[id]       - the fixed-feature toggle
  //   rom.tools.initialValues[id] - detected parameter value
  //   rom.tools.desiredValues[id] - staged parameter value
  // Fixed toggles start on for applied AND
  //                            outdated (an outdated feature upgrades on the
  //                            next export unless the user switches it off)
  function initState(rom) {
    rom.tools = {
      initial: {},
      desired: {},
      initialValues: {},
      desiredValues: {},
      unsupportedReasons: {},
    };
    if (rom.layout && rom.layout.supportsTools === false) {
      rom.tools.disabledReason = rom.layout.unsupportedFeaturesReason || 'Tools are not available for this ROM revision.';
      var disabledList = features();
      for (var di = 0; di < disabledList.length; di++) {
        rom.tools.initial[disabledList[di].id] = 'unsupported';
        rom.tools.desired[disabledList[di].id] = false;
        if (isParameterized(disabledList[di])) {
          rom.tools.initialValues[disabledList[di].id] = parameterDefault(disabledList[di]);
          rom.tools.desiredValues[disabledList[di].id] = parameterDefault(disabledList[di]);
        }
        rom.tools.unsupportedReasons[disabledList[di].id] = rom.tools.disabledReason;
      }
      return;
    }
    validateFeatureRegistry(rom.z64);
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      var unsupportedReason = featureUnsupportedReason(rom, f);
      if (unsupportedReason) {
        rom.tools.initial[f.id] = 'unsupported';
        rom.tools.desired[f.id] = false;
        if (isParameterized(f)) {
          rom.tools.initialValues[f.id] = parameterDefault(f);
          rom.tools.desiredValues[f.id] = parameterDefault(f);
        }
        rom.tools.unsupportedReasons[f.id] = unsupportedReason;
        continue;
      }
      var st = featureState(rom.z64, f);
      rom.tools.initial[f.id] = st;
      if (isParameterized(f)) {
        var value = st === 'foreign' ? null : parameterValue(rom.z64, f);
        rom.tools.initialValues[f.id] = value;
        rom.tools.desiredValues[f.id] = value === null ? parameterDefault(f) : value;
        rom.tools.desired[f.id] = value !== null && value !== parameterDefault(f);
      } else {
        rom.tools.desired[f.id] = (st === 'applied' || st === 'outdated');
      }
    }
  }

  // Number of features whose toggle differs from what the z64 currently
  // holds. An outdated feature with the toggle on counts as pending (the
  // upgrade itself is the change).
  function pendingChanges(rom) {
    if (!rom.tools) return 0;
    if (rom.tools.disabledReason) return 0;
    var n = 0;
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      if (!featureSupported(rom, f)) continue;
      var cur = featureState(rom.z64, f);
      if (cur === 'foreign') continue;
      if (isParameterized(f)) {
        var currentPercent = parameterValue(rom.z64, f);
        var wantedPercent = desiredPercent(rom, f);
        if (cur === 'applied' && wantedPercent === parameterDefault(f)) n++;
        else if (currentPercent !== wantedPercent) n++;
        continue;
      }
      var want = !!rom.tools.desired[f.id];
      if (cur === 'outdated' || want !== (cur === 'applied')) n++;
    }
    return n;
  }

  function restoreWrites(z64, writes) {
    for (var i = 0; i < writes.length; i++) {
      writeRegion(z64, writes[i].offset, originalBytes(writes[i]));
    }
  }

  // Write every pending toggle into rom.z64. Returns
  // { applied: [names], upgraded: [names], updated: [names], removed: [names],
  //   skipped: [names], values: {id: percent}, expectedStates: {id: state},
  //   crc: bool }. crc is true when any write touched the CIC-6102 CRC
  // window, in which case the caller must run OB64.recalcN64CRC before
  // exporting.
  function applyDesired(rom) {
    var res = {
      applied: [],
      upgraded: [],
      updated: [],
      removed: [],
      skipped: [],
      values: {},
      expectedStates: {},
      crc: false,
    };
    if (!rom.tools) return res;
    if (rom.tools.disabledReason) return res;
    assertDesiredCompatible(rom);
    var list = features();
    for (var i = 0; i < list.length; i++) {
      var f = list[i];
      if (!featureSupported(rom, f)) {
        var unsupportedSelected = isParameterized(f)
          ? desiredPercent(rom, f) !== parameterDefault(f)
          : !!rom.tools.desired[f.id];
        if (unsupportedSelected) res.skipped.push(f.name);
        continue;
      }
      var cur = featureState(rom.z64, f);
      if (isParameterized(f)) {
        var currentPercent = cur === 'foreign' ? null : parameterValue(rom.z64, f);
        var wantedPercent = desiredPercent(rom, f);
        if (cur === 'foreign') {
          if (wantedPercent !== rom.tools.initialValues[f.id]) res.skipped.push(f.name);
          continue;
        }
        if (wantedPercent === parameterDefault(f)) {
          if (cur === 'applied') {
            restoreWrites(rom.z64, f.writes);
            res.removed.push(f.name);
            res.expectedStates[f.id] = 'clean';
            if (f.crcWindow) res.crc = true;
          }
          continue;
        }
        if (cur === 'applied' && currentPercent === wantedPercent) continue;
        var dynamicWrites = parameterizedWrites(f, wantedPercent);
        for (var pi = 0; pi < dynamicWrites.length; pi++) {
          writeRegion(rom.z64, dynamicWrites[pi].offset, dynamicWrites[pi].bytes);
        }
        (cur === 'clean' ? res.applied : res.updated).push(f.name);
        res.values[f.id] = wantedPercent;
        res.expectedStates[f.id] = 'applied';
        if (f.crcWindow) res.crc = true;
        continue;
      }
      var want = !!rom.tools.desired[f.id];
      if (cur === 'foreign') {
        if (want !== (rom.tools.initial[f.id] === 'applied')) res.skipped.push(f.name);
        continue;
      }
      var old = cur === 'outdated' ? matchedSuperseded(rom.z64, f) : null;
      if (want && (cur === 'clean' || cur === 'outdated')) {
        if (old) restoreWrites(rom.z64, old.writes);
        for (var wi = 0; wi < f.writes.length; wi++) {
          writeRegion(rom.z64, f.writes[wi].offset, patchedBytes(f.writes[wi]));
        }
        (old ? res.upgraded : res.applied).push(f.name);
        res.expectedStates[f.id] = 'applied';
        if (f.crcWindow) res.crc = true;
      } else if (!want && (cur === 'applied' || cur === 'outdated')) {
        if (old) restoreWrites(rom.z64, old.writes);
        else restoreWrites(rom.z64, f.writes);
        res.removed.push(f.name);
        res.expectedStates[f.id] = 'clean';
        if (f.crcWindow) res.crc = true;
      }
    }
    return res;
  }

  OB64.tools = {
    features: features,
    getFeature: getFeature,
    featureState: featureState,
    initState: initState,
    pendingChanges: pendingChanges,
    applyDesired: applyDesired,
    featureRegions: featureRegions,
    findRegionConflicts: findRegionConflicts,
    desiredRegionConflicts: desiredRegionConflicts,
    assertDesiredCompatible: assertDesiredCompatible,
    validateFeatureRegistry: validateFeatureRegistry,
    featureUnsupportedReason: featureUnsupportedReason,
    featureSupported: featureSupported,
    isParameterized: isParameterized,
    parameterValue: parameterValue,
    desiredPercent: desiredPercent,
    setDesiredPercent: setDesiredPercent,
    validateParameterValue: validateParameterValue,
    effectiveWeights: effectiveWeights,
    runtimeRegistry: runtimeRegistry,
  };

})();
