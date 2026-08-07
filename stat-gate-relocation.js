// Bounded class-change stat-gate relocation.
//
// Fitting LZSS streams remain in the 376-byte retail slot. Oversized valid
// streams use one fixed 1 KiB ROM owner and the shared exact-source redirect.
// The game still allocates, decodes, and consumes the table through its retail
// resource path; this feature adds no permanent RAM owner.
window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var TABLE_BYTES = 648;
  var RECORD_BYTES = 8;
  var RECORD_COUNT = 81;
  var RETAIL_STREAM_BYTES = 376;
  var MAX_LOGICAL_STREAM_BYTES = 659;
  var MAX_STORED_STREAM_BYTES = 660;
  var MAX_PAYLOAD_BYTES = 664;
  var MAX_CONTAINER_BYTES = 668;
  var OWNER_START = 0x027B0800;
  var OWNER_SIZE = 0x400;
  var DESCRIPTOR_OFFSET = OWNER_START + 0x3C0;
  var DESCRIPTOR_SIZE = 0x40;
  var DESCRIPTOR_MAGIC = [0x4F, 0x42, 0x53, 0x47]; // OBSG
  var DESCRIPTOR_VERSION = 1;
  var FIELD_NAMES = [
    'str', 'vit', 'int', 'men', 'agi', 'dex', 'alnMin', 'alnMax'
  ];

  var PROFILES = Object.freeze({
    'us-rev0': Object.freeze({
      id: 'us-rev0',
      revision: 0,
      headerOffset: 0x023CDECE,
      sizeSourceOffset: 0x023CDECE,
      payloadSourceOffset: 0x023CDED2,
      secondPayloadSourceOffset: 0x023CE0D2,
    }),
    'us-rev1': Object.freeze({
      id: 'us-rev1',
      revision: 1,
      headerOffset: 0x023CDB22,
      sizeSourceOffset: 0x023CDB22,
      payloadSourceOffset: 0x023CDB26,
      secondPayloadSourceOffset: 0x023CDD26,
    }),
  });

  function readU32(bytes, offset) {
    return (((bytes[offset] << 24) >>> 0) |
      (bytes[offset + 1] << 16) |
      (bytes[offset + 2] << 8) |
      bytes[offset + 3]) >>> 0;
  }

  function writeU32(bytes, offset, value) {
    value >>>= 0;
    bytes[offset] = (value >>> 24) & 0xFF;
    bytes[offset + 1] = (value >>> 16) & 0xFF;
    bytes[offset + 2] = (value >>> 8) & 0xFF;
    bytes[offset + 3] = value & 0xFF;
  }

  function bytesEqual(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] !== b[i]) return false;
    }
    return true;
  }

  function allByte(bytes, start, end, value) {
    for (var i = start; i < end; i++) {
      if (bytes[i] !== value) return false;
    }
    return true;
  }

  function firstDifferentByte(bytes, start, end, value) {
    for (var i = start; i < end; i++) {
      if (bytes[i] !== value) return i;
    }
    return -1;
  }

  function crc32(bytes, start, end) {
    start = start == null ? 0 : start;
    end = end == null ? bytes.length : end;
    var crc = 0xFFFFFFFF;
    for (var i = start; i < end; i++) {
      crc ^= bytes[i];
      for (var bit = 0; bit < 8; bit++) {
        crc = (crc >>> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
      }
    }
    return (crc ^ 0xFFFFFFFF) >>> 0;
  }

  function profileFor(z64, layout) {
    var id = typeof layout === 'string' ? layout : layout && layout.id;
    if (!id && z64 && OB64.detectRomLayout) {
      var detected = OB64.detectRomLayout(z64);
      id = detected && detected.id;
    }
    if (!id && OB64.currentRomLayout) id = OB64.currentRomLayout.id;
    var profile = PROFILES[id];
    if (!profile) {
      throw new Error('Stat-gate relocation supports only US header revisions 0 and 1.');
    }
    return profile;
  }

  function makeRequest(owner, source, destination, label) {
    return {
      owner: owner,
      sourceRomOffset: source,
      destinationRomOffset: destination,
      originalDmaStart: source,
      tailDmaStart: destination,
      label: label,
    };
  }

  function redirectRequests(profile, payloadBytes) {
    var requests = [
      makeRequest(
        'stat-gates',
        profile.sizeSourceOffset,
        OWNER_START,
        'Stat-gate relocated payload-size word'
      ),
      makeRequest(
        'stat-gates',
        profile.payloadSourceOffset,
        OWNER_START + 4,
        'Stat-gate relocated payload'
      ),
    ];
    if (payloadBytes > 0x200) {
      requests.push(makeRequest(
        'stat-gates',
        profile.secondPayloadSourceOffset,
        OWNER_START + 0x204,
        'Stat-gate relocated second payload chunk'
      ));
    }
    return requests;
  }

  function decodeStream(bytes, offset, expectedSize) {
    var decoded = OB64.lzssDecode(bytes, offset, expectedSize);
    if (!decoded || !(decoded.output instanceof Uint8Array) ||
        decoded.output.length !== expectedSize) {
      throw new Error('Stat-gate LZSS stream did not decode to exactly ' +
        expectedSize + ' bytes.');
    }
    return decoded;
  }

  function parseOriginalContainer(z64, profile) {
    var headerOffset = profile.headerOffset;
    if (!(z64 instanceof Uint8Array) ||
        headerOffset < 0 || headerOffset + 8 + RETAIL_STREAM_BYTES > z64.length) {
      throw new Error('The retail stat-gate container falls outside the loaded ROM.');
    }
    var payloadSize = readU32(z64, headerOffset);
    var decodedSize = readU32(z64, headerOffset + 4);
    if (payloadSize !== RETAIL_STREAM_BYTES + 4 ||
        decodedSize !== TABLE_BYTES) {
      throw new Error('The retail stat-gate header has an invalid payload or decoded size.');
    }
    var streamOffset = headerOffset + 8;
    var decoded = decodeStream(z64, streamOffset, TABLE_BYTES);
    if (decoded.bytesConsumed < 1 || decoded.bytesConsumed > RETAIL_STREAM_BYTES) {
      throw new Error('The retail stat-gate LZSS stream has an invalid logical length.');
    }
    var paddingOffset = streamOffset + decoded.bytesConsumed;
    var badPadding = firstDifferentByte(
      z64,
      paddingOffset,
      streamOffset + RETAIL_STREAM_BYTES,
      0x03
    );
    if (badPadding !== -1) {
      throw new Error('The retail stat-gate slot contains undeclared bytes after the decoded stream at normalized-z64 ROM offset 0x' +
        badPadding.toString(16).toUpperCase() + '.');
    }
    return {
      headerOffset: headerOffset,
      streamOffset: streamOffset,
      payloadSize: payloadSize,
      decodedSize: decodedSize,
      logicalStreamBytes: decoded.bytesConsumed,
      decoded: decoded.output,
      containerBytes: z64.slice(
        headerOffset,
        headerOffset + 8 + RETAIL_STREAM_BYTES
      ),
    };
  }

  function descriptorHasMagic(z64) {
    for (var i = 0; i < DESCRIPTOR_MAGIC.length; i++) {
      if (z64[DESCRIPTOR_OFFSET + i] !== DESCRIPTOR_MAGIC[i]) return false;
    }
    return true;
  }

  function descriptorForeign(reason, details) {
    return {
      state: 'foreign',
      active: false,
      reason: reason,
      details: details || {},
      requests: [],
    };
  }

  function parseDescriptor(z64, profile, skipRedirectValidation) {
    if (z64.length < OWNER_START + OWNER_SIZE) {
      return descriptorForeign('The ROM is too small for the stat-gate relocation owner.');
    }
    if (allByte(z64, OWNER_START, OWNER_START + OWNER_SIZE, 0xFF)) {
      return { state: 'retail', active: false, requests: [] };
    }
    if (!descriptorHasMagic(z64)) {
      var occupied = firstDifferentByte(
        z64,
        OWNER_START,
        OWNER_START + OWNER_SIZE,
        0xFF
      );
      return descriptorForeign('The reserved stat-gate relocation owner is occupied without a valid OBSG descriptor.', {
        offset: occupied,
        actual: occupied >= 0 ? z64[occupied] : null,
      });
    }

    var version = z64[DESCRIPTOR_OFFSET + 4];
    var revision = z64[DESCRIPTOR_OFFSET + 5];
    var flags = z64[DESCRIPTOR_OFFSET + 6];
    var reserved = z64[DESCRIPTOR_OFFSET + 7];
    var logicalBytes = readU32(z64, DESCRIPTOR_OFFSET + 8);
    var storedBytes = readU32(z64, DESCRIPTOR_OFFSET + 12);
    var decodedBytes = readU32(z64, DESCRIPTOR_OFFSET + 16);
    var expectedDecodedCrc = readU32(z64, DESCRIPTOR_OFFSET + 20);
    var redirectCount = readU32(z64, DESCRIPTOR_OFFSET + 24);
    var expectedDescriptorCrc = readU32(z64, DESCRIPTOR_OFFSET + 28);

    if (version !== DESCRIPTOR_VERSION || revision !== profile.revision ||
        flags !== 1 || reserved !== 0) {
      return descriptorForeign('The OBSG descriptor version, revision, flags, or reserved byte is invalid.', {
        version: version,
        revision: revision,
        flags: flags,
        reserved: reserved,
      });
    }
    if (logicalBytes <= RETAIL_STREAM_BYTES ||
        logicalBytes > MAX_LOGICAL_STREAM_BYTES ||
        storedBytes < logicalBytes || storedBytes > MAX_STORED_STREAM_BYTES ||
        (storedBytes & 1) !== 0 || storedBytes - logicalBytes > 1 ||
        decodedBytes !== TABLE_BYTES) {
      return descriptorForeign('The OBSG descriptor publishes an invalid logical, stored, or decoded size.', {
        logicalBytes: logicalBytes,
        storedBytes: storedBytes,
        decodedBytes: decodedBytes,
      });
    }
    var payloadBytes = storedBytes + 4;
    var containerBytes = storedBytes + 8;
    var expectedRedirectCount = payloadBytes > 0x200 ? 3 : 2;
    if (payloadBytes > MAX_PAYLOAD_BYTES ||
        containerBytes > MAX_CONTAINER_BYTES ||
        redirectCount !== expectedRedirectCount) {
      return descriptorForeign('The OBSG descriptor exceeds the bounded container or redirect count.', {
        payloadBytes: payloadBytes,
        containerBytes: containerBytes,
        redirectCount: redirectCount,
        expectedRedirectCount: expectedRedirectCount,
      });
    }
    var actualDescriptorCrc = crc32(
      z64,
      DESCRIPTOR_OFFSET,
      DESCRIPTOR_OFFSET + 28
    );
    if (actualDescriptorCrc !== expectedDescriptorCrc) {
      return descriptorForeign('The OBSG descriptor CRC32 does not match.', {
        expected: expectedDescriptorCrc,
        actual: actualDescriptorCrc,
      });
    }
    var reservedTail = firstDifferentByte(
      z64,
      DESCRIPTOR_OFFSET + 32,
      DESCRIPTOR_OFFSET + DESCRIPTOR_SIZE,
      0
    );
    if (reservedTail !== -1) {
      return descriptorForeign('The OBSG descriptor reserved tail is not zero.', {
        offset: reservedTail,
        actual: z64[reservedTail],
      });
    }
    if (readU32(z64, OWNER_START) !== payloadBytes ||
        readU32(z64, OWNER_START + 4) !== TABLE_BYTES) {
      return descriptorForeign('The relocated stat-gate container header does not match its descriptor.');
    }

    var decoded;
    try {
      decoded = decodeStream(z64, OWNER_START + 8, TABLE_BYTES);
    } catch (error) {
      return descriptorForeign(error.message);
    }
    if (decoded.bytesConsumed !== logicalBytes) {
      return descriptorForeign('The relocated stat-gate stream consumed a different logical length than its descriptor.', {
        expected: logicalBytes,
        actual: decoded.bytesConsumed,
      });
    }
    var badStoredPadding = firstDifferentByte(
      z64,
      OWNER_START + 8 + logicalBytes,
      OWNER_START + 8 + storedBytes,
      0x03
    );
    if (badStoredPadding !== -1) {
      return descriptorForeign('The relocated stat-gate stream has invalid even-byte padding.', {
        offset: badStoredPadding,
        actual: z64[badStoredPadding],
      });
    }
    var badOwnerPadding = firstDifferentByte(
      z64,
      OWNER_START + containerBytes,
      DESCRIPTOR_OFFSET,
      0xFF
    );
    if (badOwnerPadding !== -1) {
      return descriptorForeign('The stat-gate relocation owner has undeclared bytes after its container.', {
        offset: badOwnerPadding,
        actual: z64[badOwnerPadding],
      });
    }
    var actualDecodedCrc = crc32(decoded.output);
    if (actualDecodedCrc !== expectedDecodedCrc) {
      return descriptorForeign('The relocated stat-gate decoded CRC32 does not match.', {
        expected: expectedDecodedCrc,
        actual: actualDecodedCrc,
      });
    }

    var requests = redirectRequests(profile, payloadBytes);
    if (!skipRedirectValidation) {
      if (!OB64.sourceRedirect || !OB64.sourceRedirect.validateSubset) {
        return descriptorForeign('The shared PI-source redirect controller is unavailable.');
      }
      var redirectValidation = OB64.sourceRedirect.validateSubset(z64, requests);
      if (!redirectValidation.ok ||
          redirectValidation.entryCount !== redirectCount) {
        return descriptorForeign('The relocated stat-gate container does not have its exact shared redirect entries.', {
          redirect: redirectValidation,
          redirectCount: redirectCount,
        });
      }
    }

    return {
      state: 'owned',
      active: true,
      logicalStreamBytes: logicalBytes,
      storedStreamBytes: storedBytes,
      payloadBytes: payloadBytes,
      containerBytes: containerBytes,
      decoded: decoded.output,
      decodedCrc32: actualDecodedCrc,
      redirectCount: redirectCount,
      requests: requests,
      descriptorBytes: z64.slice(
        DESCRIPTOR_OFFSET,
        DESCRIPTOR_OFFSET + DESCRIPTOR_SIZE
      ),
    };
  }

  function requestUsesStatSource(request, profile) {
    return request.sourceRomOffset === profile.sizeSourceOffset ||
      request.sourceRomOffset === profile.payloadSourceOffset ||
      request.sourceRomOffset === profile.secondPayloadSourceOffset;
  }

  function inspect(z64, layout) {
    var profile;
    try {
      profile = profileFor(z64, layout);
    } catch (error) {
      return descriptorForeign(error.message);
    }
    var original;
    try {
      original = parseOriginalContainer(z64, profile);
    } catch (error2) {
      return descriptorForeign(error2.message);
    }
    var descriptor = parseDescriptor(z64, profile);
    if (descriptor.state === 'foreign') {
      descriptor.profile = profile;
      descriptor.original = original;
      return descriptor;
    }

    var controller = OB64.sourceRedirect &&
      OB64.sourceRedirect.classify
      ? OB64.sourceRedirect.classify(z64)
      : descriptorForeign('The shared PI-source redirect controller is unavailable.');
    if (!controller.ok) {
      return {
        state: 'foreign',
        active: false,
        reason: 'The shared PI-source redirect owner is malformed: ' +
          controller.reason,
        details: controller.details || {},
        profile: profile,
        original: original,
        requests: [],
      };
    }

    if (!descriptor.active) {
      for (var i = 0; i < controller.entries.length; i++) {
        if (requestUsesStatSource(controller.entries[i], profile)) {
          return {
            state: 'foreign',
            active: false,
            reason: 'The shared redirect table contains a stat-gate source without an owned OBSG descriptor.',
            details: { request: controller.entries[i] },
            profile: profile,
            original: original,
            requests: [],
          };
        }
      }
    }

    return {
      state: descriptor.active ? 'owned' : 'in-place',
      active: !!descriptor.active,
      profile: profile,
      original: original,
      descriptor: descriptor,
      controller: controller,
      decoded: descriptor.active ? descriptor.decoded : original.decoded,
      logicalStreamBytes: descriptor.active
        ? descriptor.logicalStreamBytes
        : original.logicalStreamBytes,
      storedStreamBytes: descriptor.active
        ? descriptor.storedStreamBytes
        : RETAIL_STREAM_BYTES,
      payloadBytes: descriptor.active
        ? descriptor.payloadBytes
        : RETAIL_STREAM_BYTES + 4,
      containerBytes: descriptor.active
        ? descriptor.containerBytes
        : RETAIL_STREAM_BYTES + 8,
      requests: descriptor.active ? descriptor.requests : [],
    };
  }

  function modelFromDecoded(decoded, state) {
    var byClass = {};
    for (var classId = 0; classId < RECORD_COUNT; classId++) {
      var offset = classId * RECORD_BYTES;
      byClass[classId] = {
        classId: classId,
        str: decoded[offset],
        vit: decoded[offset + 1],
        int: decoded[offset + 2],
        men: decoded[offset + 3],
        agi: decoded[offset + 4],
        dex: decoded[offset + 5],
        alnMin: decoded[offset + 6],
        alnMax: decoded[offset + 7],
        offset: offset,
      };
    }
    var original = state.original;
    var active = state.state === 'owned';
    return {
      byClass: byClass,
      raw: decoded.slice(),
      meta: {
        payloadSize: state.payloadBytes || (RETAIL_STREAM_BYTES + 4),
        decompSize: TABLE_BYTES,
        compDataOff: active ? OWNER_START + 8 : original.streamOffset,
        compDataSize: active
          ? state.logicalStreamBytes
          : RETAIL_STREAM_BYTES,
        originalHeaderOff: original.headerOffset,
        originalCompDataOff: original.streamOffset,
        originalCompDataSize: RETAIL_STREAM_BYTES,
        originalContainerBytes: original.containerBytes.slice(),
        resolvedHeaderOff: active ? OWNER_START : original.headerOffset,
        resolvedCompDataOff: active ? OWNER_START + 8 : original.streamOffset,
        logicalStreamBytes: state.logicalStreamBytes ||
          original.logicalStreamBytes,
        relocation: {
          state: state.state,
          active: active,
          error: '',
          profileId: state.profile.id,
          revision: state.profile.revision,
          ownerStart: OWNER_START,
          ownerSize: OWNER_SIZE,
          descriptorOffset: DESCRIPTOR_OFFSET,
          requests: active ? state.requests.slice() : [],
        },
      },
    };
  }

  function parse(z64, layout) {
    var state = inspect(z64, layout);
    if (state.state !== 'foreign') {
      return modelFromDecoded(state.decoded, state);
    }

    if (state.original && state.original.decoded && state.profile) {
      var fallbackState = {
        state: 'foreign',
        profile: state.profile,
        original: state.original,
        decoded: state.original.decoded,
        logicalStreamBytes: state.original.logicalStreamBytes,
        payloadBytes: RETAIL_STREAM_BYTES + 4,
      };
      var fallback = modelFromDecoded(state.original.decoded, fallbackState);
      fallback.meta.relocation.error = state.reason;
      fallback.meta.relocation.details = state.details || {};
      return fallback;
    }

    return {
      byClass: {},
      raw: new Uint8Array(0),
      meta: {
        payloadSize: 0,
        decompSize: 0,
        compDataOff: 0,
        compDataSize: 0,
        relocation: {
          state: 'foreign',
          active: false,
          error: state.reason,
          details: state.details || {},
          requests: [],
        },
      },
    };
  }

  function requireByte(value, classId, field) {
    if (!Number.isInteger(value) || value < 0 || value > 0xFF) {
      throw new Error('Stat-gate class ' + classId + ' field ' + field +
        ' must be a whole byte from 0 through 255.');
    }
    return value;
  }

  function buildDecoded(statGates) {
    if (!statGates || !(statGates.raw instanceof Uint8Array) ||
        statGates.raw.length !== TABLE_BYTES || !statGates.byClass) {
      throw new Error('The stat-gate model is incomplete; reload the ROM before exporting.');
    }
    var decoded = statGates.raw.slice();
    Object.keys(statGates.byClass).forEach(function(key) {
      var classId = Number(key);
      if (!Number.isInteger(classId) || classId < 0 ||
          classId >= RECORD_COUNT) {
        throw new Error('The stat-gate model contains an invalid class index.');
      }
      var record = statGates.byClass[key];
      var offset = classId * RECORD_BYTES;
      FIELD_NAMES.forEach(function(field, index) {
        decoded[offset + index] = requireByte(record[field], classId, field);
      });
    });
    return decoded;
  }

  function buildDescriptor(profile, logicalBytes, storedBytes, decoded) {
    var descriptor = new Uint8Array(DESCRIPTOR_SIZE);
    descriptor.set(DESCRIPTOR_MAGIC, 0);
    descriptor[4] = DESCRIPTOR_VERSION;
    descriptor[5] = profile.revision;
    descriptor[6] = 1;
    descriptor[7] = 0;
    writeU32(descriptor, 8, logicalBytes);
    writeU32(descriptor, 12, storedBytes);
    writeU32(descriptor, 16, TABLE_BYTES);
    writeU32(descriptor, 20, crc32(decoded));
    var payloadBytes = storedBytes + 4;
    writeU32(descriptor, 24, payloadBytes > 0x200 ? 3 : 2);
    writeU32(descriptor, 28, crc32(descriptor, 0, 28));
    return descriptor;
  }

  function buildRelocatedOwner(profile, compressed, decoded) {
    var logicalBytes = compressed.length;
    var storedBytes = (logicalBytes + 1) & ~1;
    var payloadBytes = storedBytes + 4;
    var containerBytes = storedBytes + 8;
    var owner = new Uint8Array(OWNER_SIZE);
    owner.fill(0xFF);
    writeU32(owner, 0, payloadBytes);
    writeU32(owner, 4, TABLE_BYTES);
    owner.set(compressed, 8);
    if (storedBytes > logicalBytes) owner[8 + logicalBytes] = 0x03;
    var descriptor = buildDescriptor(
      profile,
      logicalBytes,
      storedBytes,
      decoded
    );
    owner.set(descriptor, DESCRIPTOR_OFFSET - OWNER_START);
    return {
      ownerBytes: owner,
      descriptorBytes: descriptor,
      logicalStreamBytes: logicalBytes,
      storedStreamBytes: storedBytes,
      payloadBytes: payloadBytes,
      containerBytes: containerBytes,
      requests: redirectRequests(profile, payloadBytes),
    };
  }

  function prepare(statGates, z64, layout, options) {
    options = options || {};
    var current = inspect(z64, layout);
    if (current.state === 'foreign') {
      throw new Error('Stat-gate relocation ownership is malformed: ' +
        current.reason);
    }
    if (!options.ignoreModelPreimage && statGates && statGates.meta &&
        statGates.meta.originalContainerBytes instanceof Uint8Array &&
        !bytesEqual(
          statGates.meta.originalContainerBytes,
          current.original.containerBytes
        )) {
      throw new Error('The retail stat-gate slot changed after the ROM was loaded. Export was stopped before writing it.');
    }
    var decoded = buildDecoded(statGates);
    var compressed = OB64.lzssCompress(decoded);
    if (!(compressed instanceof Uint8Array) || compressed.length < 1) {
      throw new Error('The stat-gate compressor returned an invalid stream.');
    }
    if (compressed.length > MAX_LOGICAL_STREAM_BYTES) {
      throw new Error('The stat-gate compressor exceeded its verified 659-byte logical maximum.');
    }
    var verified = decodeStream(compressed, 0, TABLE_BYTES);
    if (verified.bytesConsumed !== compressed.length ||
        !bytesEqual(verified.output, decoded)) {
      throw new Error('The stat-gate compressed stream did not round-trip exactly.');
    }

    var mode = compressed.length <= RETAIL_STREAM_BYTES
      ? 'in-place'
      : 'relocated';
    var relocated = mode === 'relocated'
      ? buildRelocatedOwner(current.profile, compressed, decoded)
      : null;
    return {
      mode: mode,
      profile: current.profile,
      currentState: current.state,
      currentRequests: current.requests.slice(),
      decoded: decoded,
      compressed: compressed,
      logicalStreamBytes: compressed.length,
      storedStreamBytes: relocated
        ? relocated.storedStreamBytes
        : RETAIL_STREAM_BYTES,
      payloadBytes: relocated
        ? relocated.payloadBytes
        : RETAIL_STREAM_BYTES + 4,
      containerBytes: relocated
        ? relocated.containerBytes
        : RETAIL_STREAM_BYTES + 8,
      ownerBytes: relocated ? relocated.ownerBytes : null,
      descriptorBytes: relocated ? relocated.descriptorBytes : null,
      redirectRequests: relocated ? relocated.requests : [],
      originalContainerPreimage: current.original.containerBytes.slice(),
      ownerPreimage: z64.slice(OWNER_START, OWNER_START + OWNER_SIZE),
    };
  }

  function apply(plan, z64) {
    if (!plan || !(plan.compressed instanceof Uint8Array) ||
        !(plan.originalContainerPreimage instanceof Uint8Array) ||
        !(plan.ownerPreimage instanceof Uint8Array)) {
      throw new Error('The stat-gate export plan is incomplete.');
    }
    var original = z64.subarray(
      plan.profile.headerOffset,
      plan.profile.headerOffset + 8 + RETAIL_STREAM_BYTES
    );
    var owner = z64.subarray(OWNER_START, OWNER_START + OWNER_SIZE);
    if (!bytesEqual(original, plan.originalContainerPreimage) ||
        !bytesEqual(owner, plan.ownerPreimage)) {
      throw new Error('The stat-gate ROM owner changed after planning. Export was stopped before writing it.');
    }

    if (plan.mode === 'in-place') {
      var streamOffset = plan.profile.headerOffset + 8;
      z64.set(plan.compressed, streamOffset);
      z64.fill(
        0x03,
        streamOffset + plan.compressed.length,
        streamOffset + RETAIL_STREAM_BYTES
      );
      if (plan.currentState === 'owned') {
        z64.fill(0xFF, OWNER_START, OWNER_START + OWNER_SIZE);
      }
    } else if (plan.mode === 'relocated') {
      if (!(plan.ownerBytes instanceof Uint8Array) ||
          plan.ownerBytes.length !== OWNER_SIZE) {
        throw new Error('The relocated stat-gate owner is incomplete.');
      }
      z64.set(plan.ownerBytes, OWNER_START);
    } else {
      throw new Error('The stat-gate export plan has an unknown mode.');
    }

    var dataValidation = validateDataOnly(z64, plan);
    if (!dataValidation.ok) {
      throw new Error('The stat-gate data write failed verification: ' +
        dataValidation.reason);
    }
    return {
      mode: plan.mode,
      relocated: plan.mode === 'relocated',
      logicalStreamBytes: plan.logicalStreamBytes,
      storedStreamBytes: plan.storedStreamBytes,
      payloadBytes: plan.payloadBytes,
      containerBytes: plan.containerBytes,
      redirectRequests: plan.redirectRequests.slice(),
    };
  }

  function validateDataOnly(z64, plan) {
    if (plan.mode === 'in-place') {
      try {
        var original = parseOriginalContainer(z64, plan.profile);
        if (!bytesEqual(original.decoded, plan.decoded)) {
          return { ok: false, reason: 'The in-place table decodes to different bytes.' };
        }
        if (!allByte(z64, OWNER_START, OWNER_START + OWNER_SIZE, 0xFF)) {
          return { ok: false, reason: 'The removed relocation owner was not restored to 0xFF.' };
        }
        return {
          ok: true,
          state: 'in-place',
          logicalStreamBytes: original.logicalStreamBytes,
        };
      } catch (error) {
        return { ok: false, reason: error.message };
      }
    }

    var descriptor = parseDescriptorWithoutRedirect(z64, plan.profile);
    if (!descriptor.ok) return descriptor;
    if (!bytesEqual(descriptor.decoded, plan.decoded)) {
      return { ok: false, reason: 'The relocated table decodes to different bytes.' };
    }
    return descriptor;
  }

  function parseDescriptorWithoutRedirect(z64, profile) {
    var parsed = parseDescriptor(z64, profile, true);
    return parsed.state === 'owned'
      ? Object.assign({ ok: true }, parsed)
      : { ok: false, reason: parsed.reason, details: parsed.details };
  }

  function validate(z64, layout, expectedDecoded) {
    var state = inspect(z64, layout);
    if (state.state === 'foreign') {
      return {
        ok: false,
        state: 'foreign',
        reason: state.reason,
        details: state.details || {},
      };
    }
    if (expectedDecoded && !bytesEqual(state.decoded, expectedDecoded)) {
      return {
        ok: false,
        state: state.state,
        reason: 'The finished ROM decodes different stat-gate table bytes than the export requested.',
      };
    }
    return {
      ok: true,
      state: state.state,
      active: state.active,
      logicalStreamBytes: state.logicalStreamBytes,
      storedStreamBytes: state.storedStreamBytes,
      payloadBytes: state.payloadBytes,
      containerBytes: state.containerBytes,
      redirectCount: state.requests.length,
      decodedCrc32: crc32(state.decoded),
      requests: state.requests.slice(),
    };
  }

  function currentRequests(statGates) {
    var relocation = statGates && statGates.meta &&
      statGates.meta.relocation;
    if (!relocation || relocation.state !== 'owned' ||
        !relocation.active || !Array.isArray(relocation.requests)) {
      return [];
    }
    return relocation.requests.map(function(request) {
      return Object.assign({}, request);
    });
  }

  function patchRegions(statGates) {
    var meta = statGates && statGates.meta;
    var headerOffset = meta && Number.isInteger(meta.originalHeaderOff)
      ? meta.originalHeaderOff
      : PROFILES['us-rev0'].headerOffset;
    return [
      {
        kind: 'rom',
        start: headerOffset,
        size: RETAIL_STREAM_BYTES + 8,
        label: 'stat-gate retail container',
      },
      {
        kind: 'rom',
        start: OWNER_START,
        size: OWNER_SIZE,
        label: 'stat-gate relocation owner and restoration',
      },
    ];
  }

  function patchOwner(statGates) {
    return {
      id: 'stat-gates',
      name: 'Class-change Stat Gates',
      category: 'statGates',
      regions: patchRegions(statGates),
    };
  }

  var previousParse = OB64.parseStatGates;
  var previousSerialize = OB64.serializeStatGates;

  OB64.parseStatGates = function(z64) {
    return parse(z64, OB64.currentRomLayout);
  };

  OB64.serializeStatGates = function(statGates, z64) {
    var plan = prepare(statGates, z64, OB64.currentRomLayout);
    apply(plan, z64);
    return plan;
  };

  OB64.serializeStatGatesForComparison = function(statGates, z64) {
    var plan = prepare(statGates, z64, OB64.currentRomLayout, {
      ignoreModelPreimage: true,
    });
    apply(plan, z64);
    return plan;
  };

  OB64.statGateRelocation = {
    prepare: prepare,
    apply: apply,
    inspect: inspect,
    parse: parse,
    validate: validate,
    buildDecoded: buildDecoded,
    buildDescriptor: buildDescriptor,
    currentRequests: currentRequests,
    redirectRequests: redirectRequests,
    profileFor: profileFor,
    patchRegions: patchRegions,
    patchOwner: patchOwner,
    crc32: crc32,
    profiles: PROFILES,
    constants: Object.freeze({
      TABLE_BYTES: TABLE_BYTES,
      RECORD_BYTES: RECORD_BYTES,
      RECORD_COUNT: RECORD_COUNT,
      RETAIL_STREAM_BYTES: RETAIL_STREAM_BYTES,
      MAX_LOGICAL_STREAM_BYTES: MAX_LOGICAL_STREAM_BYTES,
      MAX_STORED_STREAM_BYTES: MAX_STORED_STREAM_BYTES,
      MAX_PAYLOAD_BYTES: MAX_PAYLOAD_BYTES,
      MAX_CONTAINER_BYTES: MAX_CONTAINER_BYTES,
      OWNER_START: OWNER_START,
      OWNER_SIZE: OWNER_SIZE,
      DESCRIPTOR_OFFSET: DESCRIPTOR_OFFSET,
      DESCRIPTOR_SIZE: DESCRIPTOR_SIZE,
    }),
    _test: {
      parseOriginalContainer: parseOriginalContainer,
      parseDescriptor: parseDescriptor,
      parseDescriptorWithoutRedirect: parseDescriptorWithoutRedirect,
      buildRelocatedOwner: buildRelocatedOwner,
      validateDataOnly: validateDataOnly,
      previousParse: previousParse,
      previousSerialize: previousSerialize,
    },
  };
})(window.OB64);
