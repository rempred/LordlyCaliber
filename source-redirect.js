// Shared exact-source PI redirect owner.
//
// Scenario archive relocation and stat-gate relocation both substitute
// cartridge source addresses at the same permanent boot-code call site. This
// module is the only writer for that hook, cave, and redirect table.
window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var HOOK_ROM = 0x0001BFE4;
  var HOOK_DELAY_ROM = 0x0001BFE8;
  var CAVE_ROM = 0x000318DC;
  var CAVE_SIZE = 0x320;
  var BOOT_RAM_BASE = 0x8006FC00;
  var STUB_BYTES = 0x80;
  var ENTRY_BYTES = 8;
  var RETAIL_HOOK_WORD = 0x00431024;
  var RETAIL_DELAY_WORD = 0xAC820000;
  var MAX_ENTRIES = Math.floor(
    (CAVE_SIZE - STUB_BYTES - ENTRY_BYTES) / ENTRY_BYTES
  );

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

  function mipsJ(ramAddr) {
    return (0x08000000 | ((ramAddr >>> 2) & 0x03FFFFFF)) >>> 0;
  }

  function mipsJal(ramAddr) {
    return (0x0C000000 | ((ramAddr >>> 2) & 0x03FFFFFF)) >>> 0;
  }

  function mipsLui(rt, imm) {
    return ((0x0F << 26) | (rt << 16) | (imm & 0xFFFF)) >>> 0;
  }

  function mipsOri(rt, rs, imm) {
    return ((0x0D << 26) | (rs << 21) | (rt << 16) |
      (imm & 0xFFFF)) >>> 0;
  }

  function mipsLw(rt, base, off) {
    return ((0x23 << 26) | (base << 21) | (rt << 16) |
      (off & 0xFFFF)) >>> 0;
  }

  function mipsSw(rt, base, off) {
    return ((0x2B << 26) | (base << 21) | (rt << 16) |
      (off & 0xFFFF)) >>> 0;
  }

  function mipsAddiu(rt, rs, imm) {
    return ((0x09 << 26) | (rs << 21) | (rt << 16) |
      (imm & 0xFFFF)) >>> 0;
  }

  function mipsBeq(rs, rt, imm) {
    return ((0x04 << 26) | (rs << 21) | (rt << 16) |
      (imm & 0xFFFF)) >>> 0;
  }

  function mipsBne(rs, rt, imm) {
    return ((0x05 << 26) | (rs << 21) | (rt << 16) |
      (imm & 0xFFFF)) >>> 0;
  }

  function stubWords() {
    var caveRam = (BOOT_RAM_BASE + CAVE_ROM) >>> 0;
    var tableRam = (caveRam + STUB_BYTES) >>> 0;
    return [
      mipsLui(25, tableRam >>> 16),
      mipsOri(25, 25, tableRam & 0xFFFF),
      mipsLw(24, 25, 0),
      mipsBeq(24, 0, 9),
      0x00000000,
      mipsBne(2, 24, 4),
      0x00000000,
      mipsLw(2, 25, 4),
      mipsJ(caveRam + 0x34),
      0x00000000,
      mipsAddiu(25, 25, ENTRY_BYTES),
      mipsJ(caveRam + 0x08),
      0x00000000,
      mipsSw(2, 4, 0),
      0x03E00008,
      0x00000000,
    ];
  }

  function expectedHookWord() {
    return mipsJal((BOOT_RAM_BASE + CAVE_ROM) >>> 0);
  }

  function bytesEqual(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] !== b[i]) return false;
    }
    return true;
  }

  function firstNonzero(bytes, start, end) {
    for (var i = start; i < end; i++) {
      if (bytes[i] !== 0) return i;
    }
    return -1;
  }

  function controllerPreimage(z64) {
    var out = new Uint8Array(8 + CAVE_SIZE);
    out.set(z64.subarray(HOOK_ROM, HOOK_ROM + 8), 0);
    out.set(z64.subarray(CAVE_ROM, CAVE_ROM + CAVE_SIZE), 8);
    return out;
  }

  function cartAddress(romOffset) {
    return (0x10000000 | (romOffset >>> 0)) >>> 0;
  }

  function romOffset(cart) {
    return (cart & 0x0FFFFFFF) >>> 0;
  }

  function requestOwnerList(request) {
    var owners = [];
    if (Array.isArray(request.owners)) owners = request.owners.slice();
    if (request.owner && owners.indexOf(String(request.owner)) === -1) {
      owners.push(String(request.owner));
    }
    owners.sort();
    return owners;
  }

  function normalizeRequest(request, index, imageSize) {
    request = request || {};
    var source = request.sourceRomOffset;
    if (!Number.isInteger(source)) source = request.originalDmaStart;
    var destination = request.destinationRomOffset;
    if (!Number.isInteger(destination)) destination = request.tailDmaStart;
    if (!Number.isInteger(source) || !Number.isInteger(destination)) {
      throw new Error('PI-source redirect request ' + (index + 1) +
        ' is missing a numeric ROM source or destination.');
    }
    if (source < 0 || source > 0x0FFFFFFF || destination < 0 ||
        destination > 0x0FFFFFFF) {
      throw new Error('PI-source redirect request ' + (index + 1) +
        ' falls outside the cartridge-addressable ROM range.');
    }
    if (Number.isInteger(imageSize) &&
        (source + 4 > imageSize || destination + 4 > imageSize)) {
      throw new Error('PI-source redirect request ' + (index + 1) +
        ' points outside the loaded ROM image.');
    }
    return {
      sourceRomOffset: source >>> 0,
      destinationRomOffset: destination >>> 0,
      originalDmaStart: source >>> 0,
      tailDmaStart: destination >>> 0,
      owner: request.owner ? String(request.owner) : '',
      owners: requestOwnerList(request),
      label: request.label ? String(request.label) : '',
    };
  }

  function normalizeRequests(requests, imageSize) {
    var normalized = (requests || []).map(function(request, index) {
      return normalizeRequest(request, index, imageSize);
    });
    normalized.sort(function(a, b) {
      return a.sourceRomOffset - b.sourceRomOffset ||
        a.destinationRomOffset - b.destinationRomOffset;
    });

    var merged = [];
    normalized.forEach(function(request) {
      var previous = merged.length ? merged[merged.length - 1] : null;
      if (previous && previous.sourceRomOffset === request.sourceRomOffset) {
        if (previous.destinationRomOffset !== request.destinationRomOffset) {
          throw new Error('PI-source redirect collision at normalized-z64 ROM offset 0x' +
            request.sourceRomOffset.toString(16).toUpperCase() +
            ': two features requested different destinations.');
        }
        request.owners.forEach(function(owner) {
          if (previous.owners.indexOf(owner) === -1) previous.owners.push(owner);
        });
        previous.owners.sort();
        if (!previous.label && request.label) previous.label = request.label;
        return;
      }
      merged.push(request);
    });

    if (merged.length > MAX_ENTRIES) {
      throw new Error('The shared PI-source redirect table needs ' + merged.length +
        ' entries, but its verified capacity is ' + MAX_ENTRIES + '.');
    }
    return merged;
  }

  function requestPairsEqual(a, b) {
    if (!a || !b || a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i].sourceRomOffset !== b[i].sourceRomOffset ||
          a[i].destinationRomOffset !== b[i].destinationRomOffset) {
        return false;
      }
    }
    return true;
  }

  function foreign(reason, details) {
    return {
      state: 'foreign',
      ok: false,
      reason: reason,
      details: details || {},
      entries: [],
    };
  }

  function classify(z64) {
    if (!(z64 instanceof Uint8Array) || z64.length < CAVE_ROM + CAVE_SIZE) {
      return foreign('The loaded ROM is too small for the shared PI-source redirect owner.');
    }
    var hookWord = readU32(z64, HOOK_ROM);
    var delayWord = readU32(z64, HOOK_DELAY_ROM);
    var retail = hookWord === RETAIL_HOOK_WORD &&
      delayWord === RETAIL_DELAY_WORD;
    var installed = hookWord === expectedHookWord() &&
      delayWord === RETAIL_HOOK_WORD;

    if (retail) {
      var unexpected = firstNonzero(z64, CAVE_ROM, CAVE_ROM + CAVE_SIZE);
      if (unexpected !== -1) {
        return foreign('The retail PI-source hook has unexpected data in its reserved redirect cave.', {
          offset: unexpected,
          actual: z64[unexpected],
        });
      }
      return {
        state: 'retail',
        ok: true,
        entries: [],
        hookWord: hookWord,
        delayWord: delayWord,
      };
    }

    if (!installed) {
      return foreign('The shared PI-source redirect hook has an unrecognized preimage.', {
        hookWord: hookWord,
        delayWord: delayWord,
        expectedRetailHookWord: RETAIL_HOOK_WORD,
        expectedRetailDelayWord: RETAIL_DELAY_WORD,
        expectedInstalledHookWord: expectedHookWord(),
      });
    }

    var words = stubWords();
    for (var wordIndex = 0; wordIndex < words.length; wordIndex++) {
      var wordOffset = CAVE_ROM + wordIndex * 4;
      var actualWord = readU32(z64, wordOffset);
      if (actualWord !== words[wordIndex]) {
        return foreign('The shared PI-source redirect code is incomplete or damaged.', {
          offset: wordOffset,
          expected: words[wordIndex],
          actual: actualWord,
        });
      }
    }
    var stubPadding = firstNonzero(
      z64,
      CAVE_ROM + words.length * 4,
      CAVE_ROM + STUB_BYTES
    );
    if (stubPadding !== -1) {
      return foreign('The shared PI-source redirect stub padding is not clean.', {
        offset: stubPadding,
        actual: z64[stubPadding],
      });
    }

    var table = CAVE_ROM + STUB_BYTES;
    var entries = [];
    var seen = {};
    var terminatorOffset = -1;
    for (var entryIndex = 0; entryIndex <= MAX_ENTRIES; entryIndex++) {
      var entryOffset = table + entryIndex * ENTRY_BYTES;
      var sourceCart = readU32(z64, entryOffset);
      var destinationCart = readU32(z64, entryOffset + 4);
      if (sourceCart === 0 && destinationCart === 0) {
        terminatorOffset = entryOffset;
        break;
      }
      if (sourceCart === 0 || destinationCart === 0 ||
          (sourceCart >>> 28) !== 1 || (destinationCart >>> 28) !== 1) {
        return foreign('A shared PI-source redirect entry is malformed.', {
          entryIndex: entryIndex,
          source: sourceCart,
          destination: destinationCart,
        });
      }
      var source = romOffset(sourceCart);
      var destination = romOffset(destinationCart);
      if (source + 4 > z64.length || destination + 4 > z64.length) {
        return foreign('A shared PI-source redirect entry points outside the ROM image.', {
          entryIndex: entryIndex,
          sourceRomOffset: source,
          destinationRomOffset: destination,
        });
      }
      if (seen[source]) {
        return foreign('The shared PI-source redirect table contains a duplicate source.', {
          entryIndex: entryIndex,
          sourceRomOffset: source,
        });
      }
      if (entries.length && entries[entries.length - 1].sourceRomOffset >= source) {
        return foreign('The shared PI-source redirect table is not in canonical source order.', {
          entryIndex: entryIndex,
          sourceRomOffset: source,
        });
      }
      seen[source] = true;
      entries.push({
        sourceRomOffset: source,
        destinationRomOffset: destination,
        originalDmaStart: source,
        tailDmaStart: destination,
        owners: [],
        owner: '',
        label: '',
      });
    }

    if (terminatorOffset === -1) {
      return foreign('The shared PI-source redirect table has no ending marker.');
    }
    var trailing = firstNonzero(
      z64,
      terminatorOffset + ENTRY_BYTES,
      CAVE_ROM + CAVE_SIZE
    );
    if (trailing !== -1) {
      return foreign('The shared PI-source redirect table has stale data after its ending marker.', {
        offset: trailing,
        actual: z64[trailing],
      });
    }
    if (!entries.length) {
      return foreign('The PI-source redirect hook is installed without any redirect entries.');
    }

    return {
      state: 'installed',
      ok: true,
      entries: entries,
      hookWord: hookWord,
      delayWord: delayWord,
      terminatorOffset: terminatorOffset,
    };
  }

  function prepare(z64, desiredRequests, options) {
    options = options || {};
    var current = classify(z64);
    if (!current.ok) {
      throw new Error('Shared PI-source redirect ownership is malformed: ' +
        current.reason);
    }
    var known = normalizeRequests(
      options.knownCurrentRequests || [],
      z64.length
    );
    if (current.state === 'retail' && known.length) {
      throw new Error('The editor recorded shared redirect ownership, but the ROM has the retail hook.');
    }
    if (current.state === 'installed' &&
        !requestPairsEqual(current.entries, known)) {
      throw new Error('The installed PI-source redirect table contains entries that this editor session cannot safely own. Load the matching project or a clean ROM before exporting.');
    }
    var desired = normalizeRequests(desiredRequests || [], z64.length);
    var changed = current.state === 'retail'
      ? desired.length > 0
      : !requestPairsEqual(current.entries, desired);
    return {
      currentState: current.state,
      currentEntries: current.entries,
      knownCurrentRequests: known,
      requests: desired,
      changed: changed,
      preimage: controllerPreimage(z64),
    };
  }

  function apply(z64, plan) {
    if (!plan || !Array.isArray(plan.requests) ||
        !(plan.preimage instanceof Uint8Array)) {
      throw new Error('Shared PI-source redirect plan is incomplete.');
    }
    if (!bytesEqual(controllerPreimage(z64), plan.preimage)) {
      throw new Error('The shared PI-source redirect changed after planning. Export was stopped before writing it.');
    }
    if (!plan.changed) {
      var unchanged = validate(z64, plan.requests);
      if (!unchanged.ok) {
        throw new Error('The unchanged shared PI-source redirect did not validate: ' +
          unchanged.reason);
      }
      return {
        changed: false,
        crc: false,
        state: unchanged.state,
        entryCount: unchanged.entryCount,
      };
    }

    if (!plan.requests.length) {
      writeU32(z64, HOOK_ROM, RETAIL_HOOK_WORD);
      writeU32(z64, HOOK_DELAY_ROM, RETAIL_DELAY_WORD);
      z64.fill(0, CAVE_ROM, CAVE_ROM + CAVE_SIZE);
    } else {
      z64.fill(0, CAVE_ROM, CAVE_ROM + CAVE_SIZE);
      var words = stubWords();
      for (var wordIndex = 0; wordIndex < words.length; wordIndex++) {
        writeU32(z64, CAVE_ROM + wordIndex * 4, words[wordIndex]);
      }
      var table = CAVE_ROM + STUB_BYTES;
      plan.requests.forEach(function(request, index) {
        var offset = table + index * ENTRY_BYTES;
        writeU32(z64, offset, cartAddress(request.sourceRomOffset));
        writeU32(z64, offset + 4,
          cartAddress(request.destinationRomOffset));
      });
      writeU32(z64, table + plan.requests.length * ENTRY_BYTES, 0);
      writeU32(z64, table + plan.requests.length * ENTRY_BYTES + 4, 0);
      writeU32(z64, HOOK_ROM, expectedHookWord());
      writeU32(z64, HOOK_DELAY_ROM, RETAIL_HOOK_WORD);
    }

    var result = validate(z64, plan.requests);
    if (!result.ok) {
      throw new Error('The shared PI-source redirect failed its write verification: ' +
        result.reason);
    }
    return {
      changed: true,
      crc: true,
      state: result.state,
      entryCount: result.entryCount,
    };
  }

  function validate(z64, expectedRequests) {
    var state = classify(z64);
    if (!state.ok) return state;
    var expected;
    try {
      expected = normalizeRequests(expectedRequests || [], z64.length);
    } catch (error) {
      return foreign(error.message);
    }
    if (!expected.length) {
      if (state.state !== 'retail') {
        return foreign('The shared PI-source redirect should be disabled, but its hook is installed.', {
          entryCount: state.entries.length,
        });
      }
      return { ok: true, state: 'retail', entryCount: 0, entries: [] };
    }
    if (state.state !== 'installed') {
      return foreign('The shared PI-source redirect should be installed, but the retail hook remains.');
    }
    if (!requestPairsEqual(state.entries, expected)) {
      return foreign('The installed PI-source redirect entries do not match the planned ownership.', {
        expectedEntryCount: expected.length,
        actualEntryCount: state.entries.length,
      });
    }
    return {
      ok: true,
      state: 'installed',
      entryCount: state.entries.length,
      entries: state.entries,
    };
  }

  function validateSubset(z64, expectedRequests) {
    var state = classify(z64);
    if (!state.ok) return state;
    var expected;
    try {
      expected = normalizeRequests(expectedRequests || [], z64.length);
    } catch (error) {
      return foreign(error.message);
    }
    if (!expected.length) {
      return {
        ok: true,
        state: state.state,
        entryCount: 0,
        sharedEntryCount: state.entries.length,
      };
    }
    if (state.state !== 'installed') {
      return foreign('The required PI-source redirect entries are absent.');
    }
    for (var i = 0; i < expected.length; i++) {
      var found = false;
      for (var j = 0; j < state.entries.length; j++) {
        if (expected[i].sourceRomOffset === state.entries[j].sourceRomOffset &&
            expected[i].destinationRomOffset ===
              state.entries[j].destinationRomOffset) {
          found = true;
          break;
        }
      }
      if (!found) {
        return foreign('A required PI-source redirect entry is missing.', {
          sourceRomOffset: expected[i].sourceRomOffset,
          destinationRomOffset: expected[i].destinationRomOffset,
        });
      }
    }
    return {
      ok: true,
      state: 'installed',
      entryCount: expected.length,
      sharedEntryCount: state.entries.length,
    };
  }

  function scenarioRequests(relocations) {
    return (relocations || []).map(function(relocation, index) {
      return {
        owner: 'scenario',
        sourceRomOffset: relocation.originalDmaStart,
        destinationRomOffset: relocation.tailDmaStart,
        label: 'Scenario relocation ' + (index + 1),
      };
    });
  }

  function patchRegions() {
    return [
      {
        kind: 'rom',
        start: HOOK_ROM,
        size: 8,
        label: 'shared PI-source redirect hook',
      },
      {
        kind: 'rom',
        start: CAVE_ROM,
        size: CAVE_SIZE,
        label: 'shared PI-source redirect code and table',
      },
    ];
  }

  function patchOwner() {
    return {
      id: 'pi-source-redirect',
      name: 'Shared PI-Source Redirect',
      category: 'sourceRedirect',
      regions: patchRegions(),
    };
  }

  OB64.sourceRedirect = {
    prepare: prepare,
    apply: apply,
    classify: classify,
    validate: validate,
    validateSubset: validateSubset,
    normalizeRequests: normalizeRequests,
    requestsEqual: requestPairsEqual,
    scenarioRequests: scenarioRequests,
    patchRegions: patchRegions,
    patchOwner: patchOwner,
    constants: Object.freeze({
      HOOK_ROM: HOOK_ROM,
      HOOK_DELAY_ROM: HOOK_DELAY_ROM,
      CAVE_ROM: CAVE_ROM,
      CAVE_SIZE: CAVE_SIZE,
      BOOT_RAM_BASE: BOOT_RAM_BASE,
      STUB_BYTES: STUB_BYTES,
      ENTRY_BYTES: ENTRY_BYTES,
      MAX_ENTRIES: MAX_ENTRIES,
      RETAIL_HOOK_WORD: RETAIL_HOOK_WORD,
      RETAIL_DELAY_WORD: RETAIL_DELAY_WORD,
      INSTALLED_HOOK_WORD: expectedHookWord(),
    }),
    _test: {
      stubWords: stubWords,
      controllerPreimage: controllerPreimage,
      cartAddress: cartAddress,
      romOffset: romOffset,
    },
  };
})(window.OB64);
