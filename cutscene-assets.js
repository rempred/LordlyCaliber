// Lordly Caliber - Cutscene Studio image-container decoders.
//
// The decoders expose partial formats honestly. A parsed header is not treated
// as a renderable or exportable image until its palette and pixel path exist.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var CAPABILITIES = OB64.cutsceneModel
    ? OB64.cutsceneModel.capabilities
    : {
        NATIVE: 'native', CONVERTED: 'converted',
        PREVIEW_ONLY: 'preview-only', NEEDS_RESEARCH: 'needs-research'
      };
  var MAX_DIMENSION = 8192;
  var MAX_PIXELS = 2048 * 2048;

  function CutsceneAssetError(message) {
    this.name = 'CutsceneAssetError';
    this.message = message;
  }
  CutsceneAssetError.prototype = Object.create(Error.prototype);
  CutsceneAssetError.prototype.constructor = CutsceneAssetError;

  function fail(message) { throw new CutsceneAssetError(message); }

  function bytesView(bytes) {
    if (bytes instanceof Uint8Array) return bytes;
    if (bytes instanceof ArrayBuffer) return new Uint8Array(bytes);
    fail('Image input must be a Uint8Array or ArrayBuffer.');
  }

  function readU16(bytes, offset) {
    if (offset < 0 || offset + 2 > bytes.length) fail('Image header is truncated.');
    return (bytes[offset] << 8) | bytes[offset + 1];
  }

  function readS16(bytes, offset) {
    var value = readU16(bytes, offset);
    return value & 0x8000 ? value - 0x10000 : value;
  }

  function readU32(bytes, offset) {
    if (offset < 0 || offset + 4 > bytes.length) fail('Image header is truncated.');
    return ((bytes[offset] * 0x1000000) + (bytes[offset + 1] << 16) +
      (bytes[offset + 2] << 8) + bytes[offset + 3]) >>> 0;
  }

  function aligned(value, multiple) {
    return Math.ceil(value / multiple) * multiple;
  }

  function dimensions(width, height) {
    if (!Number.isInteger(width) || !Number.isInteger(height) ||
        width < 1 || height < 1 || width > MAX_DIMENSION || height > MAX_DIMENSION ||
        width * height > MAX_PIXELS) {
      fail('Image dimensions exceed the supported axis or ' + MAX_PIXELS + '-pixel budget.');
    }
    return width * height;
  }

  function rgba5551(word, greenChromaKey) {
    if (greenChromaKey && word === 0x07C0) return [0, 0, 0, 0];
    return [
      ((word >>> 11) & 31) * 255 / 31,
      ((word >>> 6) & 31) * 255 / 31,
      ((word >>> 1) & 31) * 255 / 31,
      (word & 1) ? 255 : 0
    ].map(function(value) { return Math.round(value); });
  }

  function paletteWords(bytes, offset, count) {
    if (!Number.isInteger(offset) || offset < 0 || offset + count * 2 > bytes.length) {
      fail('Palette lies outside the image container.');
    }
    var output = new Uint16Array(count);
    for (var index = 0; index < count; index++) {
      output[index] = readU16(bytes, offset + index * 2);
    }
    return output;
  }

  function decodeRgba5551(bytes, offset, width, height, options) {
    options = options || {};
    var count = dimensions(width, height);
    var rowStride = Number.isInteger(options.rowStride) ? options.rowStride : width * 2;
    if (rowStride < width * 2 || !Number.isInteger(offset) || offset < 0 ||
        offset + rowStride * height > bytes.length) {
      fail('RGBA5551 pixels lie outside the image container.');
    }
    var output = new Uint8ClampedArray(count * 4);
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        var index = y * width + x;
        var color = rgba5551(readU16(bytes, offset + y * rowStride + x * 2),
          !!options.greenChromaKey);
        output.set(color, index * 4);
      }
    }
    return output;
  }

  function decodeRgba32(bytes, offset, width, height, options) {
    options = options || {};
    var count = dimensions(width, height);
    var rowStride = Number.isInteger(options.rowStride) ? options.rowStride : width * 4;
    if (rowStride < width * 4 || !Number.isInteger(offset) || offset < 0 ||
        offset + rowStride * height > bytes.length) {
      fail('RGBA32 pixels lie outside the image container.');
    }
    var output = new Uint8ClampedArray(count * 4);
    for (var y = 0; y < height; y++) {
      output.set(bytes.slice(offset + y * rowStride, offset + y * rowStride + width * 4),
        y * width * 4);
    }
    return output;
  }

  function decodeIndexed(indices, palette, greenChromaKey) {
    var output = new Uint8ClampedArray(indices.length * 4);
    for (var index = 0; index < indices.length; index++) {
      if (indices[index] >= palette.length) fail('Pixel index exceeds its palette.');
      output.set(rgba5551(palette[indices[index]], !!greenChromaKey), index * 4);
    }
    return output;
  }

  function unpackCi4(bytes, offset, width, height, rowStride) {
    var pixelCount = dimensions(width, height);
    rowStride = Number.isInteger(rowStride) ? rowStride : Math.ceil(width / 2);
    if (rowStride < Math.ceil(width / 2) || offset < 0 ||
        offset + rowStride * height > bytes.length) {
      fail('CI4 pixels lie outside the image container.');
    }
    var output = new Uint8Array(pixelCount);
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        var value = bytes[offset + y * rowStride + (x >>> 1)];
        output[y * width + x] = x & 1 ? value & 15 : value >>> 4;
      }
    }
    return output;
  }

  function unpackCi8(bytes, offset, width, height, rowStride) {
    dimensions(width, height);
    rowStride = Number.isInteger(rowStride) ? rowStride : width;
    if (rowStride < width || offset < 0 || offset + rowStride * height > bytes.length) {
      fail('CI8 pixels lie outside the image container.');
    }
    var output = new Uint8Array(width * height);
    for (var y = 0; y < height; y++) {
      output.set(bytes.slice(offset + y * rowStride, offset + y * rowStride + width), y * width);
    }
    return output;
  }

  function decodeIntensity(indices, maximum, transparentZero) {
    var output = new Uint8ClampedArray(indices.length * 4);
    for (var index = 0; index < indices.length; index++) {
      var value = Math.round(indices[index] * 255 / maximum);
      output[index * 4] = value;
      output[index * 4 + 1] = value;
      output[index * 4 + 2] = value;
      output[index * 4 + 3] = transparentZero && !indices[index] ? 0 : 255;
    }
    return output;
  }

  function decodeDiagnosticIndices(indices, maximum) {
    var output = new Uint8ClampedArray(indices.length * 4);
    for (var index = 0; index < indices.length; index++) {
      var value = indices[index];
      output[index * 4] = value ? 48 + value * 149 % 208 : 18;
      output[index * 4 + 1] = value ? 56 + value * 83 % 192 : 18;
      output[index * 4 + 2] = value ? 64 + value * 197 % 192 : 18;
      output[index * 4 + 3] = 255;
    }
    return output;
  }

  function parseN64Image(input, options) {
    options = options || {};
    var bytes = bytesView(input);
    if (bytes.length < 8 || bytes[0] !== 0x36 || bytes[1] !== 0x34) {
      fail('The image does not use the 64 container.');
    }
    var type = bytes[2];
    var subtype = bytes[3];
    var width = readU16(bytes, 4);
    var height = readU16(bytes, 6);
    dimensions(width, height);
    var result = {
      container: '64',
      type: type,
      subtype: subtype,
      width: width,
      height: height,
      dataOffset: 8,
      variant: 'unknown',
      capability: CAPABILITIES.NEEDS_RESEARCH,
      renderable: false,
      rgba: null,
      indices: null,
      paletteWords: null,
      warnings: []
    };

    var rowStride, dataEnd;
    if (type === 0 && subtype === 2) {
      result.variant = 'rgba5551';
      rowStride = aligned(width, 4) * 2;
      result.rgba = decodeRgba5551(bytes, 8, width, height, {
        rowStride: rowStride, greenChromaKey: options.greenChromaKey
      });
      result.rowStride = rowStride;
      result.capability = CAPABILITIES.NATIVE;
      result.renderable = true;
      return result;
    }
    if (type === 0 && subtype === 3) {
      result.variant = 'rgba32';
      rowStride = aligned(width, 4) * 4;
      result.rgba = decodeRgba32(bytes, 8, width, height, { rowStride: rowStride });
      result.rowStride = rowStride;
      result.capability = CAPABILITIES.NATIVE;
      result.renderable = true;
      return result;
    }
    if (type === 2 && subtype === 1) {
      result.variant = 'ci8';
      rowStride = aligned(width, 8);
      dataEnd = 8 + rowStride * height;
      if (dataEnd + 512 > bytes.length) fail('CI8 image has no complete trailing palette.');
      result.indices = unpackCi8(bytes, 8, width, height, rowStride);
      result.paletteWords = paletteWords(bytes, dataEnd, 256);
      result.rgba = decodeIndexed(result.indices, result.paletteWords, options.greenChromaKey);
      result.rowStride = rowStride;
      result.capability = CAPABILITIES.NATIVE;
      result.renderable = true;
      return result;
    }
    if (type === 2 && subtype === 0) {
      result.variant = 'ci4';
      rowStride = aligned(width, 16) / 2;
      dataEnd = 8 + rowStride * height;
      result.indices = unpackCi4(bytes, 8, width, height, rowStride);
      var paletteOffset = Number.isInteger(options.paletteOffset)
        ? options.paletteOffset : (dataEnd + 32 <= bytes.length ? dataEnd : null);
      if (paletteOffset !== null) {
        result.paletteWords = paletteWords(bytes, paletteOffset, 16);
        result.rgba = decodeIndexed(result.indices, result.paletteWords, options.greenChromaKey);
        result.capability = CAPABILITIES.NATIVE;
        result.renderable = true;
      } else {
        result.warnings.push('CI4 palette location is not mapped for this asset.');
      }
      result.rowStride = rowStride;
      return result;
    }
    if (type === 4 && subtype === 0) {
      result.variant = 'i4';
      rowStride = aligned(width, 16) / 2;
      result.indices = unpackCi4(bytes, 8, width, height, rowStride);
      result.rgba = decodeIntensity(result.indices, 15, false);
      result.rowStride = rowStride;
      result.capability = CAPABILITIES.NATIVE;
      result.renderable = true;
      dataEnd = 8 + rowStride * height;
      if (dataEnd !== bytes.length) {
        result.warnings.push((bytes.length - dataEnd) +
          ' trailing bytes are preserved outside the I4 preview.');
      }
      return result;
    }
    if (type === 4 && subtype === 1) {
      result.variant = 'i8';
      rowStride = aligned(width, 8);
      result.indices = unpackCi8(bytes, 8, width, height, rowStride);
      result.rgba = decodeIntensity(result.indices, 255, false);
      result.rowStride = rowStride;
      result.capability = CAPABILITIES.NATIVE;
      result.renderable = true;
      return result;
    }

    result.variant = 'type-' + type + '-subtype-' + subtype;
    result.warnings.push('This 64 image subtype is not decoded.');
    return result;
  }

  function parseBg2(input) {
    var bytes = bytesView(input);
    if (bytes.length < 24 || bytes[0] !== 0x42 || bytes[1] !== 0x35) {
      fail('The image does not use the B5 background container.');
    }
    var format = bytes[2];
    var declaredCount = bytes[3];
    if ([0, 1, 3, 5].indexOf(format) < 0) {
      fail('The B5 background uses unsupported format ' + format + '.');
    }
    var recordCount = format === 5 ? 1 : declaredCount;
    var records = [];
    var cursor = 8;
    var usesEmbeddedHuff = false;
    var minX = 0, minY = 0, maxX = 0, maxY = 0;
    for (var ordinal = 0; ordinal < recordCount; ordinal++) {
      if (cursor + 16 > bytes.length) fail('Background record header is truncated.');
      var record = {
        ordinal: ordinal,
        x: readS16(bytes, cursor),
        y: readS16(bytes, cursor + 2),
        width: readU16(bytes, cursor + 4),
        height: readU16(bytes, cursor + 6),
        dataSize: readU32(bytes, cursor + 8),
        reserved: readU32(bytes, cursor + 12),
        headerOffset: cursor,
        dataOffset: cursor + 16
      };
      dimensions(record.width, record.height);
      if (record.reserved !== 0 || record.dataOffset + record.dataSize > bytes.length) {
        fail('Background record ' + ordinal + ' has an invalid extent.');
      }
      var colorStride = aligned(record.width, 4) * 2;
      var maskStride = aligned(record.width, 8);
      var expectedSize = format === 0
        ? (colorStride + maskStride) * record.height
        : (format === 3 ? aligned(record.width, 4) * 4 * record.height
          : colorStride * record.height);
      var embeddedHuff = format === 3 && record.dataSize >= 14 &&
        readU32(bytes, record.dataOffset) === 0x4855FE00;
      if (!embeddedHuff && record.dataSize !== expectedSize) {
        fail('Background record ' + ordinal + ' has ' + record.dataSize +
          ' bytes; expected ' + expectedSize + '.');
      }
      if (embeddedHuff) {
        if (!OB64.cutsceneNjpg || typeof OB64.cutsceneNjpg.parseEmbedded !== 'function') {
          fail('cutscene-njpg.js is required for embedded B5 HUFF previews.');
        }
        var decodedHuff = OB64.cutsceneNjpg.parseEmbedded(
          bytes.slice(record.dataOffset, record.dataOffset + record.dataSize));
        if (decodedHuff.width !== record.width || decodedHuff.height !== record.height) {
          fail('Background record ' + ordinal + ' HUFF geometry disagrees with its B5 header.');
        }
        record.rgba = decodedHuff.rgba;
        record.embeddedContainer = decodedHuff.container;
        record.previewWarnings = decodedHuff.warnings;
        usesEmbeddedHuff = true;
      } else if (format === 3) {
        record.rgba = decodeRgba32(bytes, record.dataOffset, record.width, record.height, {
          rowStride: aligned(record.width, 4) * 4
        });
      } else {
        record.rgba = decodeRgba5551(bytes, record.dataOffset, record.width, record.height, {
          rowStride: colorStride
        });
      }
      if (format === 0) {
        record.maskOffset = record.dataOffset + colorStride * record.height;
        for (var y = 0; y < record.height; y++) {
          for (var x = 0; x < record.width; x++) {
            record.rgba[(y * record.width + x) * 4 + 3] =
              bytes[record.maskOffset + y * maskStride + x];
          }
        }
      }
      records.push(record);
      if (!ordinal) {
        minX = record.x; minY = record.y;
        maxX = record.x + record.width; maxY = record.y + record.height;
      } else {
        minX = Math.min(minX, record.x); minY = Math.min(minY, record.y);
        maxX = Math.max(maxX, record.x + record.width);
        maxY = Math.max(maxY, record.y + record.height);
      }
      cursor = record.dataOffset + record.dataSize;
    }
    if (cursor !== bytes.length) {
      fail('Background records consumed ' + cursor + ' of ' + bytes.length + ' bytes.');
    }
    var width = maxX - minX, height = maxY - minY;
    dimensions(width, height);
    var layers = records.map(function(record) {
      return {
        rgba: record.rgba, width: record.width, height: record.height,
        x: record.x - minX, y: record.y - minY, depth: record.ordinal
      };
    });
    var result = {
      container: 'bg2',
      format: format,
      recordCount: recordCount,
      declaredRecordCount: declaredCount,
      reference: { x: readU16(bytes, 4), y: readU16(bytes, 6) },
      originX: minX,
      originY: minY,
      width: width,
      height: height,
      records: records,
      capability: format === 0 || format === 5 || usesEmbeddedHuff
        ? CAPABILITIES.PREVIEW_ONLY : CAPABILITIES.NATIVE,
      renderable: true,
      rgba: compositeLayers(layers, width, height),
      warnings: usesEmbeddedHuff ? [
        'The embedded HUFF/NJPG layers use the native coefficient layout and RSP scale; fixed-point rounding remains approximated.'
      ] : []
    };
    if (format === 0) {
      result.warnings.push('The paired I8 plane is previewed as alpha. Native blend and occlusion semantics remain unresolved.');
    }
    if (format === 5 && declaredCount !== 1) {
      result.warnings.push('Format 5 stores one inline image while its remaining declared members are externally owned.');
    }
    if (records.length > 1) {
      result.warnings.push('Record order is used as preview depth; native depth ordering remains unresolved.');
    }
    if (result.reference.x || result.reference.y) {
      result.warnings.push('B5 reference coordinates are retained as container metadata; Stage placement uses the signed native record coordinates.');
    }
    return result;
  }

  function parseKImage(input, options) {
    options = options || {};
    var bytes = bytesView(input);
    if (bytes.length < 8 || bytes[0] !== 0x4B) fail('The image does not use a K container.');
    var width = readU16(bytes, 4), height = readU16(bytes, 6);
    var pixelCount = dimensions(width, height);
    var result = {
      container: 'K', width: width, height: height,
      variant: 'unknown', capability: CAPABILITIES.NEEDS_RESEARCH,
      renderable: false, rgba: null, indices: null, paletteWords: null,
      warnings: []
    };
    if (bytes[1] === 0 && bytes[2] === 0 && bytes[3] === 2) {
      if (8 + pixelCount * 2 !== bytes.length) fail('K RGBA5551 extent is invalid.');
      result.variant = 'rgba5551';
      result.rgba = decodeRgba5551(bytes, 8, width, height, options);
      result.capability = CAPABILITIES.NATIVE;
      result.renderable = true;
      return result;
    }
    if (bytes[2] !== 2 || bytes[3] !== 0 && bytes[3] !== 1) {
      result.warnings.push('This K header variant is not mapped.');
      return result;
    }
    var paletteCount = bytes[1] + 1;
    var packedSize = bytes[3] === 0 ? Math.ceil(pixelCount / 2) : pixelCount;
    var paletteOffset = 8 + packedSize;
    if (paletteOffset + paletteCount * 2 !== bytes.length) fail('K indexed extent is invalid.');
    result.variant = bytes[3] === 0 ? 'ci4' : 'ci8';
    result.indices = bytes[3] === 0
      ? unpackCi4(bytes, 8, width, height, Math.ceil(width / 2))
      : unpackCi8(bytes, 8, width, height, width);
    result.paletteWords = paletteWords(bytes, paletteOffset, paletteCount);
    result.rgba = decodeIndexed(result.indices, result.paletteWords, options.greenChromaKey);
    result.capability = CAPABILITIES.NATIVE;
    result.renderable = true;
    return result;
  }

  function arrangeFrames(frames, frameWidth, frameHeight, columns) {
    columns = Math.max(1, Math.min(frames.length, columns || frames.length));
    var rows = Math.ceil(frames.length / columns);
    var width = frameWidth * columns, height = frameHeight * rows;
    var layers = frames.map(function(rgba, index) {
      return {
        rgba: rgba, width: frameWidth, height: frameHeight,
        x: index % columns * frameWidth,
        y: Math.floor(index / columns) * frameHeight,
        depth: index
      };
    });
    return { width: width, height: height, rgba: compositeLayers(layers, width, height) };
  }

  function parseRawImage(input, layout) {
    var bytes = bytesView(input);
    layout = layout || {};
    var frameWidth = layout.frameWidth || layout.width;
    var frameHeight = layout.frameHeight || layout.height;
    var frameCount = layout.paletteBankCount || layout.frameCount || 1;
    dimensions(frameWidth, frameHeight);
    var dataOffset = layout.dataOffset || 0;
    var frameBytes = layout.frameBytes;
    var frames = [];
    var palette = null;
    var paletteBanks = null;
    var nativeRecord = null;
    if (layout.nativeRecordHeader) {
      if (bytes.length < 16 || dataOffset !== 16) {
        fail('The external B5 record header is truncated or misplaced.');
      }
      nativeRecord = {
        x: readS16(bytes, 0),
        y: readS16(bytes, 2),
        width: readU16(bytes, 4),
        height: readU16(bytes, 6),
        dataSize: readU32(bytes, 8),
        reserved: readU32(bytes, 12),
        headerSize: 16
      };
      if (nativeRecord.width !== frameWidth || nativeRecord.height !== frameHeight ||
          nativeRecord.dataSize !== bytes.length - 16 || nativeRecord.reserved !== 0) {
        fail('The external B5 record header does not match its pixel extent.');
      }
    }
    if (layout.paletteCount && layout.paletteBankCount) {
      paletteBanks = [];
      for (var paletteBank = 0; paletteBank < layout.paletteBankCount; paletteBank++) {
        paletteBanks.push(paletteWords(bytes,
          (layout.paletteOffset || 0) + paletteBank * layout.paletteCount * 2,
          layout.paletteCount));
      }
    } else if (layout.paletteCount) {
      palette = paletteWords(bytes, layout.paletteOffset || 0, layout.paletteCount);
    }
    for (var frame = 0; frame < frameCount; frame++) {
      var sourceFrame = layout.paletteBankCount ? 0 : frame;
      var framePalette = paletteBanks ? paletteBanks[frame] : palette;
      var offset = dataOffset + (frameBytes || 0) * sourceFrame;
      var indices, rgba;
      if (layout.pixelFormat === 'rgba5551') {
        frameBytes = frameBytes || frameWidth * frameHeight * 2;
        offset = dataOffset + frameBytes * sourceFrame;
        rgba = decodeRgba5551(bytes, offset, frameWidth, frameHeight, {
          rowStride: layout.rowStride || frameWidth * 2,
          greenChromaKey: layout.greenChromaKey
        });
      } else if (layout.pixelFormat === 'ci4') {
        frameBytes = frameBytes || Math.ceil(frameWidth * frameHeight / 2);
        offset = dataOffset + frameBytes * sourceFrame;
        indices = unpackCi4(bytes, offset, frameWidth, frameHeight,
          layout.rowStride || Math.ceil(frameWidth / 2));
        rgba = framePalette ? decodeIndexed(indices, framePalette, layout.greenChromaKey)
          : decodeDiagnosticIndices(indices, 15);
      } else if (layout.pixelFormat === 'ci8') {
        frameBytes = frameBytes || frameWidth * frameHeight;
        offset = dataOffset + frameBytes * sourceFrame;
        indices = unpackCi8(bytes, offset, frameWidth, frameHeight,
          layout.rowStride || frameWidth);
        rgba = framePalette ? decodeIndexed(indices, framePalette, layout.greenChromaKey)
          : decodeDiagnosticIndices(indices, 255);
      } else if (layout.pixelFormat === 'i4') {
        frameBytes = frameBytes || Math.ceil(frameWidth * frameHeight / 2);
        offset = dataOffset + frameBytes * sourceFrame;
        indices = unpackCi4(bytes, offset, frameWidth, frameHeight,
          layout.rowStride || Math.ceil(frameWidth / 2));
        rgba = decodeIntensity(indices, 15, false);
      } else if (layout.pixelFormat === 'i8') {
        frameBytes = frameBytes || frameWidth * frameHeight;
        offset = dataOffset + frameBytes * frame;
        indices = unpackCi8(bytes, offset, frameWidth, frameHeight,
          layout.rowStride || frameWidth);
        rgba = decodeIntensity(indices, 255, false);
      } else {
        fail('Raw image pixel format is not mapped.');
      }
      frames.push(rgba);
    }
    var arranged = arrangeFrames(frames, frameWidth, frameHeight, layout.frameColumns);
    var warnings = Array.isArray(layout.warnings) ? layout.warnings.slice() : [];
    if ((layout.pixelFormat === 'ci4' || layout.pixelFormat === 'ci8') &&
        !palette && !paletteBanks) {
      warnings.push('The external palette is unresolved. Colors are a diagnostic index visualization.');
    }
    return {
      container: layout.container || 'raw',
      variant: layout.pixelFormat,
      width: arranged.width,
      height: arranged.height,
      frameWidth: frameWidth,
      frameHeight: frameHeight,
      frameCount: frameCount,
      frames: frames.map(function(rgba) {
        return { width: frameWidth, height: frameHeight, rgba: rgba };
      }),
      capability: layout.previewCapability || CAPABILITIES.PREVIEW_ONLY,
      renderable: true,
      rgba: arranged.rgba,
      paletteWords: palette,
      paletteBanks: paletteBanks,
      nativeRecord: nativeRecord,
      warnings: warnings
    };
  }

  function parseImageAsset(input, asset) {
    asset = asset || {};
    if (asset.sourceKind === 'section-c-njpg' || asset.container === 'section-c-njpg') {
      if (!OB64.cutsceneNjpg || typeof OB64.cutsceneNjpg.parse !== 'function') {
        fail('cutscene-njpg.js is required for Section C previews.');
      }
      return OB64.cutsceneNjpg.parse(input);
    }
    if (asset.container === 'embedded-njpg') {
      if (!OB64.cutsceneNjpg || typeof OB64.cutsceneNjpg.parseEmbedded !== 'function') {
        fail('cutscene-njpg.js is required for native HUFF environment previews.');
      }
      return OB64.cutsceneNjpg.parseEmbedded(input);
    }
    if (asset.container === 'bg2') return parseBg2(input);
    if (asset.container === '64') return parseN64Image(input, asset.decodeOptions || {});
    if (asset.container === 'K' || asset.container === 'K64') {
      return parseKImage(input, asset.decodeOptions || {});
    }
    if (asset.container === 'raw') return parseRawImage(input, asset.layout || {});
    fail('The selected image container has no Cutscene Stage decoder.');
  }

  function solidRgba(width, height, color) {
    var count = dimensions(width, height);
    if (!Array.isArray(color) || color.length !== 4) fail('Solid color must contain RGBA values.');
    var output = new Uint8ClampedArray(count * 4);
    for (var index = 0; index < count; index++) output.set(color, index * 4);
    return output;
  }

  function compositeLayers(layers, width, height) {
    dimensions(width, height);
    if (!Array.isArray(layers)) fail('Stage layers must be an array.');
    var output = new Uint8ClampedArray(width * height * 4);
    var ordered = layers.map(function(layer, index) {
      return { layer: layer, index: index };
    }).filter(function(row) {
      return row.layer && row.layer.visible !== false;
    }).sort(function(left, right) {
      return Number(left.layer.depth || 0) - Number(right.layer.depth || 0) || left.index - right.index;
    });

    ordered.forEach(function(row) {
      var layer = row.layer;
      if (!(layer.rgba instanceof Uint8Array) && !(layer.rgba instanceof Uint8ClampedArray)) {
        fail('Renderable stage layers require RGBA bytes.');
      }
      var layerWidth = layer.width;
      var layerHeight = layer.height;
      dimensions(layerWidth, layerHeight);
      if (layer.rgba.length !== layerWidth * layerHeight * 4) fail('Stage layer RGBA length is invalid.');
      var originX = Number.isInteger(layer.x) ? layer.x : 0;
      var originY = Number.isInteger(layer.y) ? layer.y : 0;
      var opacity = Number.isFinite(layer.opacity) ? Math.max(0, Math.min(1, layer.opacity)) : 1;
      for (var sourceY = 0; sourceY < layerHeight; sourceY++) {
        var targetY = sourceY + originY;
        if (targetY < 0 || targetY >= height) continue;
        for (var sourceX = 0; sourceX < layerWidth; sourceX++) {
          var targetX = sourceX + originX;
          if (targetX < 0 || targetX >= width) continue;
          var sourceOffset = (sourceY * layerWidth + sourceX) * 4;
          var targetOffset = (targetY * width + targetX) * 4;
          var sourceAlpha = layer.rgba[sourceOffset + 3] / 255 * opacity;
          if (sourceAlpha <= 0) continue;
          var targetAlpha = output[targetOffset + 3] / 255;
          var finalAlpha = sourceAlpha + targetAlpha * (1 - sourceAlpha);
          for (var channel = 0; channel < 3; channel++) {
            var sourceValue = layer.rgba[sourceOffset + channel];
            var targetValue = output[targetOffset + channel];
            output[targetOffset + channel] = finalAlpha
              ? Math.round((sourceValue * sourceAlpha + targetValue * targetAlpha * (1 - sourceAlpha)) / finalAlpha)
              : 0;
          }
          output[targetOffset + 3] = Math.round(finalAlpha * 255);
        }
      }
    });
    return output;
  }

  function composeCompoundImageAsset(asset, parsedMembers) {
    var compound = asset && asset.compound;
    if (!compound || compound.kind !== 'b5-format5-external-records' ||
        !Array.isArray(compound.members) || !compound.members.length) {
      fail('The image asset has no supported compound-background description.');
    }
    if (!Array.isArray(parsedMembers) || parsedMembers.length !== compound.members.length) {
      fail('The compound background does not contain every declared image member.');
    }
    var byAssetId = {};
    parsedMembers.forEach(function(entry) {
      if (!entry || !entry.assetId || !entry.image) {
        fail('Compound background members require an asset ID and decoded image.');
      }
      if (byAssetId[entry.assetId]) fail('Compound background member IDs must be unique.');
      byAssetId[entry.assetId] = entry.image;
    });
    var minX = Math.min.apply(null, compound.members.map(function(member) { return member.x; }));
    var minY = Math.min.apply(null, compound.members.map(function(member) { return member.y; }));
    var maxX = Math.max.apply(null, compound.members.map(function(member) {
      return member.x + member.width;
    }));
    var maxY = Math.max.apply(null, compound.members.map(function(member) {
      return member.y + member.height;
    }));
    var width = maxX - minX;
    var height = maxY - minY;
    if (width !== compound.width || height !== compound.height ||
        minX !== compound.originX || minY !== compound.originY) {
      fail('The compound background bounds disagree with its native member records.');
    }
    var warnings = [];
    var layers = compound.members.map(function(member, index) {
      var image = byAssetId[member.assetId];
      if (!image || image.renderable !== true || !image.rgba ||
          image.width !== member.width || image.height !== member.height) {
        fail('Compound background member ' + member.assetId + ' has invalid decoded pixels.');
      }
      (image.warnings || []).forEach(function(warning) {
        if (warning.indexOf('remaining declared members are externally owned') < 0 &&
            warnings.indexOf(warning) < 0) warnings.push(warning);
      });
      return {
        rgba: image.rgba,
        width: image.width,
        height: image.height,
        x: member.x - minX,
        y: member.y - minY,
        depth: Number.isFinite(member.ordinal) ? member.ordinal : index
      };
    });
    warnings.unshift(compound.members.length +
      ' native format-5 records assembled from their ROM archives.');
    return {
      container: 'bg2',
      format: 5,
      recordCount: compound.members.length,
      declaredRecordCount: compound.declaredMemberCount,
      reference: {
        x: compound.reference.x,
        y: compound.reference.y
      },
      originX: minX,
      originY: minY,
      width: width,
      height: height,
      records: compound.members.map(function(member) {
        return Object.assign({}, member);
      }),
      capability: CAPABILITIES.PREVIEW_ONLY,
      renderable: true,
      rgba: compositeLayers(layers, width, height),
      compoundAssembled: true,
      warnings: warnings
    };
  }

  OB64.cutsceneAssets = {
    CutsceneAssetError: CutsceneAssetError,
    maxDimension: MAX_DIMENSION,
    rgba5551: rgba5551,
    decodeRgba5551: decodeRgba5551,
    decodeRgba32: decodeRgba32,
    decodeIndexed: decodeIndexed,
    parseN64Image: parseN64Image,
    parseBg2: parseBg2,
    parseKImage: parseKImage,
    parseRawImage: parseRawImage,
    parseImageAsset: parseImageAsset,
    composeCompoundImageAsset: composeCompoundImageAsset,
    solidRgba: solidRgba,
    compositeLayers: compositeLayers
  };
})(window.OB64);
