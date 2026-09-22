// Native dialogue editing. Rebuild whole shared archives and preserve untouched entries.
window.OB64 = window.OB64 || {};

(function(O) {
  'use strict';

  var RESOURCE_BASE = 0x594280;
  var DIRECTORY = RESOURCE_BASE + 0x1A3B7B2;

  function fail(message) { throw new Error(message); }

  function read(rom, selector) {
    var view = new DataView(rom.buffer, rom.byteOffset, rom.byteLength);
    if (DIRECTORY + 4 > rom.length) fail('Dialogue archive directory is missing.');
    var tableSize = view.getUint32(DIRECTORY);
    if (tableSize % 4 || DIRECTORY + 4 + tableSize > rom.length ||
        !Number.isInteger(selector) || selector < 0 || selector >= tableSize / 4) {
      fail('Dialogue archive selector is outside the ROM table.');
    }
    var key = view.getUint32(DIRECTORY + 4 + selector * 4);
    var prefix = key + RESOURCE_BASE;
    var start = prefix + 4;
    if (key >>> 28 || start + 24 > rom.length) fail('Dialogue resource reference is invalid.');
    var capacity = view.getUint32(prefix);
    var level = rom[start + 20];
    var header = level === 2 ? view.getUint16(start, true) : rom[start] + 2;
    var packed = view.getUint32(start + 7, true);
    var length = view.getUint32(start + 11, true);
    var method = String.fromCharCode.apply(null, rom.slice(start + 2, start + 7));
    if ([0, 2].indexOf(level) === -1 || header < 24 || header > 1024 || packed < 1 ||
        length < 4 || length > 65536 || header + packed > capacity || start + capacity > rom.length) {
      fail('Dialogue resource extent is invalid.');
    }
    var data = method === '-lh5-' ? O.lh5Decompress(rom.slice(start + header, start + header + packed), length)
      : method === '-lh0-' ? rom.slice(start + header, start + header + packed) : null;
    if (!data || data.length !== length) fail('Dialogue archive compression is unsupported.');
    var crcAt = level === 2 ? 21 : 22 + rom[start + 21];
    if (crcAt + 2 > header || O.crc16(data) !== view.getUint16(start + crcAt, true)) {
      fail('Dialogue archive checksum differs.');
    }
    var decoded = new DataView(data.buffer, data.byteOffset, data.byteLength);
    var first = decoded.getUint32(0);
    if (first < 4 || first > length || first % 4) fail('Dialogue offset table is invalid.');
    var entries = [];
    for (var index = 0; index < first / 4; index++) {
      var offset = decoded.getUint32(index * 4);
      var end = index + 1 < first / 4 ? decoded.getUint32((index + 1) * 4) : length;
      if (offset < first || end <= offset || end > length) fail('Dialogue entry range is invalid.');
      var zero = offset;
      while (zero < end && data[zero]) zero++;
      if (zero === end) fail('Dialogue entry lacks a terminator.');
      entries.push({ offset: offset, end: end, rawText: String.fromCharCode.apply(null, data.slice(offset, zero)) });
    }
    var selectorWords = [];
    for (var at = DIRECTORY + 4; at < DIRECTORY + 4 + tableSize; at += 4) {
      if (view.getUint32(at) === key) selectorWords.push(at);
    }
    return { key: key, prefix: prefix, start: start, capacity: capacity, header: header,
      level: level, crcAt: crcAt, data: data, entries: entries, selectorWords: selectorWords };
  }

  function entry(rom, selector, index) {
    var row = read(rom, selector).entries[index];
    if (!row) fail('Dialogue entry selector is outside its archive.');
    return row;
  }

  function encodeArchive(rom, archive, data) {
    var compressed = O.lh5Compress(data);
    // Keep a fitting resource in its original slot. Larger resources use the shared allocator.
    var capacity = Math.max(archive.capacity, archive.header + compressed.length);
    var template = rom.slice(archive.start, archive.start + archive.header);
    template.set([45, 108, 104, 53, 45], 2);
    var bytes;
    if (archive.level === 2) {
      bytes = O.buildLHAArchiveFromTemplate(compressed, data, template, capacity - archive.header);
    } else {
      bytes = new Uint8Array(capacity);
      bytes.set(template);
      bytes.set(compressed, archive.header);
      var view = new DataView(bytes.buffer);
      view.setUint32(7, capacity - archive.header, true);
      view.setUint32(11, data.length, true);
      view.setUint16(archive.crcAt, O.crc16(data), true);
      var sum = 0;
      for (var index = 2; index < archive.header; index++) sum = (sum + bytes[index]) & 255;
      bytes[1] = sum;
    }
    if (!O.cutsceneCodec.equalBytes(O.lh5Decompress(bytes.slice(archive.header), data.length), data)) {
      fail('Edited dialogue archive failed to decompress.');
    }
    return bytes;
  }

  function resources(rom, documents) {
    var archives = new Map();
    var selectors = new Map();
    documents.forEach(function(document) {
      document.tracks.forEach(function(track) {
        track.clips.forEach(function(clip) {
          var payload = clip.payload;
          if (!payload.nativeDialogueEditable || payload.rawText === payload.originalRawText) return;
          if (typeof payload.rawText !== 'string' || !payload.rawText.length || /[^\x20-\x7e]/.test(payload.rawText)) {
            fail('Native dialogue requires printable ASCII and its existing @ control tokens.');
          }
          var selector = payload.presentationArchiveSelector;
          var archive = selectors.get(selector);
          if (!archive) { archive = read(rom, selector); selectors.set(selector, archive); }
          var index = payload.presentationEntrySelector;
          if (!Number.isInteger(index) || !archive.entries[index]) fail('Dialogue entry selector is outside its archive.');
          if (archive.entries[index].rawText !== payload.originalRawText) fail('Dialogue entry changed since this Project was created.');
          var group = archives.get(archive.key);
          if (!group) { group = { archive: archive, edits: new Map(), selector: selector }; archives.set(archive.key, group); }
          if (group.edits.has(index) && group.edits.get(index) !== payload.rawText) {
            fail('Two scenes assign different text to shared dialogue archive ' + selector + ', entry ' + index + '.');
          }
          group.edits.set(index, payload.rawText);
        });
      });
    });
    var output = [];
    archives.forEach(function(group) {
      var archive = group.archive;
      var parts = archive.entries.map(function(row, index) {
        return group.edits.has(index) ? Uint8Array.from(group.edits.get(index) + '\0', function(c) { return c.charCodeAt(0); })
          : archive.data.slice(row.offset, row.end);
      });
      var size = parts.length * 4 + parts.reduce(function(total, part) { return total + part.length; }, 0);
      if (size > 65536) fail('Dialogue archive exceeds its 64 KiB decoded bound.');
      var data = new Uint8Array(size);
      var view = new DataView(data.buffer);
      var cursor = parts.length * 4;
      parts.forEach(function(part, index) { view.setUint32(index * 4, cursor); data.set(part, cursor); cursor += part.length; });
      output.push({ archive: archive, selector: group.selector, data: data, bytes: encodeArchive(rom, archive, data) });
    });
    return output;
  }

  O.cutsceneAuthoring = { readDialogue: read, dialogueEntry: entry, dialogueResources: resources };
})(window.OB64);
