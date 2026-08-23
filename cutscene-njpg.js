// Lordly Caliber - native HUFF/NJPG preview decoder.
//
// The retail image converter consumes the entropy coefficients in transposed
// zigzag order. Its RSP path applies a four-times coefficient scale, leaves Y
// at its decoded level, and centers the decoded Cb/Cr levels at 128. The
// remaining preview difference is the retail fixed-point IDCT/color rounding.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var WIDTH = 320;
  var HEIGHT = 240;
  var JPEG_ZIGZAG = [
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
  ];
  var NATIVE_COEFFICIENT_ORDER = JPEG_ZIGZAG.map(function(index) {
    return (index & 7) * 8 + (index >>> 3);
  });
  var NATIVE_IDCT_SCALE = 4;

  var HUFFMAN_SPECS = {
    dcLuma: {
      bits: [0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0],
      values: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
    },
    dcChroma: {
      bits: [0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0],
      values: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
    },
    acLuma: {
      bits: [0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D],
      values: hexBytes(
        '01 02 03 00 04 11 05 12 21 31 41 06 13 51 61 07 22 71 14 32 81 91 A1 08 ' +
        '23 42 B1 C1 15 52 D1 F0 24 33 62 72 82 09 0A 16 17 18 19 1A 25 26 27 28 ' +
        '29 2A 34 35 36 37 38 39 3A 43 44 45 46 47 48 49 4A 53 54 55 56 57 58 59 ' +
        '5A 63 64 65 66 67 68 69 6A 73 74 75 76 77 78 79 7A 83 84 85 86 87 88 89 ' +
        '8A 92 93 94 95 96 97 98 99 9A A2 A3 A4 A5 A6 A7 A8 A9 AA B2 B3 B4 B5 B6 ' +
        'B7 B8 B9 BA C2 C3 C4 C5 C6 C7 C8 C9 CA D2 D3 D4 D5 D6 D7 D8 D9 DA E1 E2 ' +
        'E3 E4 E5 E6 E7 E8 E9 EA F1 F2 F3 F4 F5 F6 F7 F8 F9 FA')
    },
    acChroma: {
      bits: [0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77],
      values: hexBytes(
        '00 01 02 03 11 04 05 21 31 06 12 41 51 07 61 71 13 22 32 81 08 14 42 91 ' +
        'A1 B1 C1 09 23 33 52 F0 15 62 72 D1 0A 16 24 34 E1 25 F1 17 18 19 1A 26 ' +
        '27 28 29 2A 35 36 37 38 39 3A 43 44 45 46 47 48 49 4A 53 54 55 56 57 58 ' +
        '59 5A 63 64 65 66 67 68 69 6A 73 74 75 76 77 78 79 7A 82 83 84 85 86 87 ' +
        '88 89 8A 92 93 94 95 96 97 98 99 9A A2 A3 A4 A5 A6 A7 A8 A9 AA B2 B3 B4 ' +
        'B5 B6 B7 B8 B9 BA C2 C3 C4 C5 C6 C7 C8 C9 CA D2 D3 D4 D5 D6 D7 D8 D9 DA ' +
        'E2 E3 E4 E5 E6 E7 E8 E9 EA F2 F3 F4 F5 F6 F7 F8 F9 FA')
    }
  };

  function NjpgError(message) {
    this.name = 'CutsceneNjpgError';
    this.message = message;
  }
  NjpgError.prototype = Object.create(Error.prototype);
  NjpgError.prototype.constructor = NjpgError;

  function fail(message) { throw new NjpgError(message); }

  function hexBytes(text) {
    return text.trim().split(/\s+/).map(function(value) { return parseInt(value, 16); });
  }

  function bytesView(input) {
    if (input instanceof Uint8Array) return input;
    if (input instanceof ArrayBuffer) return new Uint8Array(input);
    fail('Section C input must be a Uint8Array or ArrayBuffer.');
  }

  function readU16(bytes, offset) {
    if (offset < 0 || offset + 2 > bytes.length) fail('Section C header is truncated.');
    return (bytes[offset] << 8) | bytes[offset + 1];
  }

  function readU32(bytes, offset) {
    if (offset < 0 || offset + 4 > bytes.length) fail('Section C header is truncated.');
    return ((bytes[offset] * 0x1000000) + (bytes[offset + 1] << 16) +
      (bytes[offset + 2] << 8) + bytes[offset + 3]) >>> 0;
  }

  function buildHuffmanTable(spec) {
    var table = {};
    var code = 0;
    var valueIndex = 0;
    for (var length = 1; length <= 16; length++) {
      var count = spec.bits[length - 1];
      for (var index = 0; index < count; index++) {
        table[length + ':' + code] = spec.values[valueIndex++];
        code++;
      }
      code <<= 1;
    }
    if (valueIndex !== spec.values.length) fail('The standard JPEG Huffman table is invalid.');
    return table;
  }

  var HUFFMAN_TABLES = {
    dcLuma: buildHuffmanTable(HUFFMAN_SPECS.dcLuma),
    dcChroma: buildHuffmanTable(HUFFMAN_SPECS.dcChroma),
    acLuma: buildHuffmanTable(HUFFMAN_SPECS.acLuma),
    acChroma: buildHuffmanTable(HUFFMAN_SPECS.acChroma)
  };

  function BitReader(bytes, startOffset) {
    this.bytes = bytes;
    this.bitPosition = startOffset * 8;
  }
  BitReader.prototype.readBit = function() {
    if (this.bitPosition >= this.bytes.length * 8) {
      fail('Section C entropy stream ended at bit ' + this.bitPosition + '.');
    }
    var value = this.bytes[this.bitPosition >>> 3];
    var shift = 7 - (this.bitPosition & 7);
    this.bitPosition++;
    return value >>> shift & 1;
  };
  BitReader.prototype.readBits = function(count) {
    var value = 0;
    for (var index = 0; index < count; index++) value = value * 2 + this.readBit();
    return value;
  };

  function decodeSymbol(reader, table) {
    var code = 0;
    for (var length = 1; length <= 16; length++) {
      code = code * 2 + reader.readBit();
      var key = length + ':' + code;
      if (Object.prototype.hasOwnProperty.call(table, key)) return table[key];
    }
    fail('Section C entropy stream contains an invalid Huffman code.');
  }

  function receiveExtend(reader, size) {
    if (!size) return 0;
    var value = reader.readBits(size);
    var threshold = Math.pow(2, size - 1);
    return value >= threshold ? value : value - (Math.pow(2, size) - 1);
  }

  function decodeCoefficients(bytes, entropyOffset, macroblocks) {
    var reader = new BitReader(bytes, entropyOffset);
    var coefficients = new Int16Array(macroblocks * 6 * 64);
    var dc = [0, 0, 0];
    var outputOffset = 0;
    for (var macroblock = 0; macroblock < macroblocks; macroblock++) {
      for (var blockIndex = 0; blockIndex < 6; blockIndex++) {
        var luma = blockIndex < 4;
        var component = luma ? 0 : (blockIndex === 4 ? 1 : 2);
        var dcTable = luma ? HUFFMAN_TABLES.dcLuma : HUFFMAN_TABLES.dcChroma;
        var acTable = luma ? HUFFMAN_TABLES.acLuma : HUFFMAN_TABLES.acChroma;
        var dcSize = decodeSymbol(reader, dcTable);
        dc[component] += receiveExtend(reader, dcSize);
        coefficients[outputOffset] = dc[component];
        var coefficient = 1;
        while (coefficient < 64) {
          var runSize = decodeSymbol(reader, acTable);
          if (runSize === 0) break;
          if (runSize === 0xF0) {
            coefficient += 16;
            if (coefficient > 64) fail('Section C zero run exceeds its coefficient block.');
            continue;
          }
          coefficient += runSize >>> 4;
          if (coefficient >= 64) fail('Section C AC run exceeds its coefficient block.');
          coefficients[outputOffset + coefficient] = receiveExtend(reader, runSize & 15);
          coefficient++;
        }
        outputOffset += 64;
      }
    }
    return {
      coefficients: coefficients,
      consumedBytes: Math.ceil(reader.bitPosition / 8),
      slackBytes: bytes.length - Math.ceil(reader.bitPosition / 8)
    };
  }

  var COSINE = [];
  for (var frequency = 0; frequency < 8; frequency++) {
    COSINE[frequency] = [];
    for (var sample = 0; sample < 8; sample++) {
      COSINE[frequency][sample] = Math.cos(((2 * sample + 1) * frequency * Math.PI) / 16);
    }
  }

  function coefficientScale(value) { return value === 0 ? Math.SQRT1_2 : 1; }

  function inverseDct(block) {
    var output = new Float64Array(64);
    for (var y = 0; y < 8; y++) {
      for (var x = 0; x < 8; x++) {
        var sum = 0;
        for (var v = 0; v < 8; v++) {
          for (var u = 0; u < 8; u++) {
            sum += coefficientScale(u) * coefficientScale(v) *
              block[v * 8 + u] * COSINE[u][x] * COSINE[v][y];
          }
        }
        output[y * 8 + x] = sum / 4;
      }
    }
    return output;
  }

  function renderCoefficients(coefficients, width, height, macroblocks) {
    var yPlane = new Float64Array(width * height);
    var cbPlane = new Float64Array(width / 2 * height / 2);
    var crPlane = new Float64Array(width / 2 * height / 2);
    var coefficientOffset = 0;
    for (var macroblock = 0; macroblock < macroblocks; macroblock++) {
      var macroblockX = macroblock % (width / 16);
      var macroblockY = Math.floor(macroblock / (width / 16));
      for (var blockIndex = 0; blockIndex < 6; blockIndex++) {
        var natural = new Float64Array(64);
        for (var coefficient = 0; coefficient < 64; coefficient++) {
          natural[NATIVE_COEFFICIENT_ORDER[coefficient]] =
            coefficients[coefficientOffset++];
        }
        var pixels = inverseDct(natural);
        if (blockIndex < 4) {
          var blockX = macroblockX * 16 + (blockIndex & 1) * 8;
          var blockY = macroblockY * 16 + (blockIndex >>> 1) * 8;
          for (var y = 0; y < 8; y++) {
            for (var x = 0; x < 8; x++) {
              yPlane[(blockY + y) * width + blockX + x] =
                pixels[y * 8 + x] * NATIVE_IDCT_SCALE;
            }
          }
        } else {
          var plane = blockIndex === 4 ? cbPlane : crPlane;
          var chromaX = macroblockX * 8;
          var chromaY = macroblockY * 8;
          for (var cy = 0; cy < 8; cy++) {
            for (var cx = 0; cx < 8; cx++) {
              plane[(chromaY + cy) * (width / 2) + chromaX + cx] =
                pixels[cy * 8 + cx] * NATIVE_IDCT_SCALE;
            }
          }
        }
      }
    }

    var rgba = new Uint8ClampedArray(width * height * 4);
    for (var outputY = 0; outputY < height; outputY++) {
      for (var outputX = 0; outputX < width; outputX++) {
        var luminance = yPlane[outputY * width + outputX];
        var chromaOffset = (outputY >>> 1) * (width / 2) + (outputX >>> 1);
        var cb = cbPlane[chromaOffset] - 128;
        var cr = crPlane[chromaOffset] - 128;
        var outputOffset = (outputY * width + outputX) * 4;
        rgba[outputOffset] = Math.round(luminance + 1.402 * cr);
        rgba[outputOffset + 1] = Math.round(luminance - 0.344136 * cb - 0.714136 * cr);
        rgba[outputOffset + 2] = Math.round(luminance + 1.772 * cb);
        rgba[outputOffset + 3] = 255;
      }
    }
    return rgba;
  }

  function parseContainer(input, embedded) {
    var bytes = bytesView(input);
    var base = embedded ? 0 : 4;
    if (bytes.length < base + 14 || !embedded && readU32(bytes, 0) + 4 !== bytes.length) {
      fail('Section C container length does not match its leading size word.');
    }
    if (readU32(bytes, base) !== 0x4855FE00) fail('Section C wrapper word is missing.');
    var geometry = readU32(bytes, base + 4);
    var width = geometry >>> 16;
    var height = geometry & 0xFFFF;
    if (!width || !height || width % 16 || height % 16) {
      fail('HUFF/NJPG dimensions must use complete 16×16 macroblocks.');
    }
    if (bytes[base + 8] !== 0x48 || bytes[base + 9] !== 0x55 ||
        bytes[base + 10] !== 0x46 || bytes[base + 11] !== 0x46) {
      fail('Section C HUFF magic is missing.');
    }
    var macroblocks = readU16(bytes, base + 12);
    if (macroblocks !== width / 16 * (height / 16)) {
      fail('HUFF/NJPG macroblock count does not match its dimensions.');
    }
    var decoded = decodeCoefficients(bytes, base + 14, macroblocks);
    return {
      container: embedded ? 'embedded-njpg' : 'section-c-njpg',
      variant: 'native-rsp-transposed-flat4-bt601',
      width: width,
      height: height,
      macroblocks: macroblocks,
      renderable: true,
      capability: 'preview-only',
      rgba: renderCoefficients(decoded.coefficients, width, height, macroblocks),
      consumedBytes: decoded.consumedBytes,
      slackBytes: decoded.slackBytes,
      warnings: [
        'Pixels use the native transposed coefficient order and four-times RSP scale; fixed-point rounding remains approximated.',
        embedded
          ? 'This HUFF/NJPG image is an exact member of a native scene group.'
          : 'The gameplay consumer and original scene association remain unresolved.'
      ]
    };
  }

  function parse(input) { return parseContainer(input, false); }
  function parseEmbedded(input) { return parseContainer(input, true); }

  OB64.cutsceneNjpg = {
    Error: NjpgError,
    parse: parse,
    parseEmbedded: parseEmbedded
  };
})(window.OB64);
