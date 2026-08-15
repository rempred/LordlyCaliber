// OB64 Mod Editor - native class-card avatar and item-icon model/export codec
//
// The editor keeps exact RGBA5551 words as its authoring model. Avatars are
// route-scoped detached resources and may use any opaque RGB555 color. Item
// icons rebuild their complete shared pack and remain limited to that pack's
// retail palette. No PNG or compressed-resource bytes enter Project JSON.

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var C = {
    REV0_Z64_SHA256: '571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A',
    REV1_Z64_SHA256: '3BFBAF0AF968795102F6D136713665E347C22723B4CA75BD5494FDC97DF5919E',
    RESOURCE_BASE: 0x00594280,
    ARENA_START: 0x0275E000,
    ARENA_END: 0x02780000,
    ARENA_FILL: 0xFF,
    AVATAR_DESCRIPTOR_KEY: 0x01DFB6EA,
    AVATAR_ROUTE_TABLE: 0x00064BE0,
    AVATAR_CLASS_COUNT: 0xA5,
    AVATAR_ROUTE_STRIDE: 6,
    AVATAR_WIDTH: 40,
    AVATAR_HEIGHT: 48,
    AVATAR_PIXELS: 0x780,
    AVATAR_PALETTE_WORDS: 80,
    AVATAR_DESCRIPTOR_SLOTS: 142,
    AVATAR_COLOR_LIBRARY_SIZE: 3464,
    AVATAR_OPAQUE_COLOR_COUNT: 32768,
    ICON_WIDTH: 16,
    ICON_HEIGHT: 16,
    ICON_PIXELS: 0x100,
    ICON_PALETTE_WORDS: 256,
    ICON_COLOR_LIBRARY_SIZE: 499
  };

  C.AVATAR_DESCRIPTOR_SITES = [
    { name: 'portrait-descriptor-owner-01', lui: 0x00045AFC, ori: 0x00045B00 },
    { name: 'portrait-descriptor-owner-02', lui: 0x00045B6C, ori: 0x00045B70 },
    { name: 'portrait-descriptor-owner-03', lui: 0x00045C58, ori: 0x00045C5C }
  ];

  C.ICON_PACKS = [
    {
      slug: 'equipment', label: 'Equipment', resourceKey: 0x01DEE82C,
      sizeWord: 0x02382AAC, capacity: 37832, decodedLength: 0x11700,
      iconCount: 277, transparentIndex: 255, retailStoredLength: 37831,
      sites: [
        ['equipment-owner-01', 0x000AEE94, 0x000AEE98],
        ['equipment-owner-02', 0x00132BC0, 0x00132BC8],
        ['equipment-owner-03', 0x0019BB80, 0x0019BB88],
        ['equipment-owner-04', 0x001B5E30, 0x001B5E38],
        ['equipment-owner-05', 0x002A53A4, 0x002A53AC],
        ['equipment-owner-06', 0x002A5448, 0x002A5454],
        ['equipment-owner-07', 0x002A6360, 0x002A636C],
        ['equipment-owner-08', 0x002A6640, 0x002A664C],
        ['equipment-owner-09', 0x002A67A0, 0x002A67AC]
      ]
    },
    {
      slug: 'special-item', label: 'Special Item', resourceKey: 0x01DF7BF8,
      sizeWord: 0x0238BE78, capacity: 7406, decodedLength: 0x2E00,
      iconCount: 44, transparentIndex: 0, retailStoredLength: 7405,
      sites: [
        ['special-item-owner-01', 0x000AEEAC, 0x000AEEB0],
        ['special-item-owner-02', 0x00102AC4, 0x00102AD4],
        ['special-item-owner-03', 0x001AB730, 0x001AB73C],
        ['special-item-owner-04', 0x00216AA4, 0x00216AA8],
        ['special-item-owner-05', 0x002A53B0, 0x002A53B4],
        ['special-item-owner-06', 0x002A542C, 0x002A543C],
        ['special-item-owner-07', 0x002A6624, 0x002A6634],
        ['special-item-owner-08', 0x002A6784, 0x002A6794]
      ]
    }
  ];
  C.ICON_PACKS.forEach(function(pack) {
    pack.sites = pack.sites.map(function(site) {
      return { name: site[0], lui: site[1], ori: site[2] };
    });
  });

  function ArtError(message, detail) {
    this.name = 'ArtError';
    this.message = message;
    this.detail = detail || message;
  }
  ArtError.prototype = Object.create(Error.prototype);
  ArtError.prototype.constructor = ArtError;

  function exportFailure(asset, check, observed, expected, action) {
    var line = '[Art export blocked] asset=' + asset + '; check=' + check +
      '; observed=' + observed + '; expected=' + expected + '; action=' + action;
    throw new ArtError(line, line);
  }

  function hex(value, width) {
    return '0x' + (Number(value) >>> 0).toString(16).toUpperCase()
      .padStart(width || 8, '0');
  }

  function cleanName(value, fallback) {
    var text = String(value || '').replace(/[\x00-\x1F\x7F]/g, '').trim();
    return text || fallback;
  }

  function readU16(bytes, offset) {
    if (offset < 0 || offset + 2 > bytes.length) throw new ArtError('u16 read lies outside ROM');
    return (bytes[offset] << 8) | bytes[offset + 1];
  }

  function readU32(bytes, offset) {
    if (offset < 0 || offset + 4 > bytes.length) throw new ArtError('u32 read lies outside ROM');
    return ((bytes[offset] * 0x1000000) + (bytes[offset + 1] << 16) +
      (bytes[offset + 2] << 8) + bytes[offset + 3]) >>> 0;
  }

  function writeU16(bytes, offset, value) {
    bytes[offset] = (value >>> 8) & 0xFF;
    bytes[offset + 1] = value & 0xFF;
  }

  function writeU32(bytes, offset, value) {
    value >>>= 0;
    bytes[offset] = value >>> 24;
    bytes[offset + 1] = (value >>> 16) & 0xFF;
    bytes[offset + 2] = (value >>> 8) & 0xFF;
    bytes[offset + 3] = value & 0xFF;
  }

  function equalBytes(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var i = 0; i < left.length; i++) if (left[i] !== right[i]) return false;
    return true;
  }

  function equalWords(left, right) {
    return equalBytes(left, right);
  }

  function copyRange(target, source, start, end) {
    target.set(source.subarray(start, end), start);
  }

  function wordsFromBytes(bytes, offset, count) {
    var words = new Uint16Array(count);
    for (var i = 0; i < count; i++) words[i] = readU16(bytes, offset + i * 2);
    return words;
  }

  function bytesFromWords(words) {
    var bytes = new Uint8Array(words.length * 2);
    for (var i = 0; i < words.length; i++) writeU16(bytes, i * 2, words[i]);
    return bytes;
  }

  function concatBytes(parts) {
    var length = parts.reduce(function(total, part) { return total + part.length; }, 0);
    var out = new Uint8Array(length);
    var cursor = 0;
    parts.forEach(function(part) { out.set(part, cursor); cursor += part.length; });
    return out;
  }

  function toBase64(bytes) {
    var parts = [];
    for (var start = 0; start < bytes.length; start += 0x8000) {
      var slice = bytes.subarray(start, Math.min(bytes.length, start + 0x8000));
      var chars = '';
      for (var i = 0; i < slice.length; i++) chars += String.fromCharCode(slice[i]);
      parts.push(chars);
    }
    return btoa(parts.join(''));
  }

  function fromBase64(text, label) {
    if (typeof text !== 'string' || !text.length) throw new ArtError(label + ' is not base64 text');
    var raw;
    try { raw = atob(text); } catch (error) { throw new ArtError(label + ' is not valid base64'); }
    var bytes = new Uint8Array(raw.length);
    for (var i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
    return bytes;
  }

  function wordArrayToBase64(words) { return toBase64(bytesFromWords(words)); }

  function wordArrayFromBase64(text, count, label) {
    var bytes = fromBase64(text, label);
    if (bytes.length !== count * 2) {
      throw new ArtError(label + ' decodes to ' + bytes.length + ' bytes; expected ' + (count * 2));
    }
    return wordsFromBytes(bytes, 0, count);
  }

  function rgba5551(word) {
    function expand(value) { return Math.floor((value * 255 + 15) / 31); }
    return [
      expand((word >>> 11) & 31), expand((word >>> 6) & 31),
      expand((word >>> 1) & 31), (word & 1) ? 255 : 0
    ];
  }

  function rgba5551Word(red5, green5, blue5, opaque) {
    var channels = [red5, green5, blue5];
    for (var i = 0; i < channels.length; i++) {
      if (!Number.isInteger(channels[i]) || channels[i] < 0 || channels[i] > 31) {
        throw new ArtError('RGB555 channel must be an integer from 0 through 31');
      }
    }
    return ((red5 << 11) | (green5 << 6) | (blue5 << 1) |
      (opaque === false ? 0 : 1)) & 0xFFFF;
  }

  var nativeChannelMap = null;
  function nativeChannel5(value) {
    if (!nativeChannelMap) {
      nativeChannelMap = new Int8Array(256);
      nativeChannelMap.fill(-1);
      function register(expanded, channel) {
        var prior = nativeChannelMap[expanded];
        nativeChannelMap[expanded] = prior === -1 || prior === channel
          ? channel : -2;
      }
      for (var channel = 0; channel < 32; channel++) {
        register(Math.floor((channel * 255 + 15) / 31), channel);
        register(Math.floor(channel * 255 / 31), channel);
        register(channel << 3, channel);
        register((channel << 3) | (channel >> 2), channel);
      }
    }
    return Number.isInteger(value) && value >= 0 && value <= 255
      ? nativeChannelMap[value] : -1;
  }

  function avatarWordsFromRgbaBytes(rgba, width, height, label) {
    label = label || 'Avatar PNG';
    if (width !== C.AVATAR_WIDTH || height !== C.AVATAR_HEIGHT) {
      throw new ArtError(label + ' is ' + width + 'x' + height +
        '; required dimensions are 40x48 pixels');
    }
    if (!ArrayBuffer.isView(rgba) || rgba.length !== C.AVATAR_PIXELS * 4) {
      throw new ArtError(label + ' did not decode to exactly 1,920 RGBA pixels');
    }
    var words = new Uint16Array(C.AVATAR_PIXELS), colors = new Set();
    for (var pixel = 0; pixel < C.AVATAR_PIXELS; pixel++) {
      var offset = pixel * 4, alpha = rgba[offset + 3];
      var x = pixel % C.AVATAR_WIDTH, y = Math.floor(pixel / C.AVATAR_WIDTH);
      if (alpha !== 255) {
        throw new ArtError(label + ' pixel (' + x + ',' + y + ') has alpha ' +
          alpha + '; avatars must be fully opaque');
      }
      var red = rgba[offset], green = rgba[offset + 1], blue = rgba[offset + 2];
      var red5 = nativeChannel5(red);
      var green5 = nativeChannel5(green);
      var blue5 = nativeChannel5(blue);
      if (red5 < 0 || green5 < 0 || blue5 < 0) {
        throw new ArtError(label + ' pixel (' + x + ',' + y +
          ') uses RGB (' + red + ',' + green + ',' + blue +
          '); one or more channels are not a recognized lossless RGB555 expansion');
      }
      var word = rgba5551Word(red5, green5, blue5, true);
      words[pixel] = word; colors.add(word);
    }
    if (colors.size > C.AVATAR_PALETTE_WORDS) {
      throw new ArtError(label + ' uses ' + colors.size +
        ' opaque colors; maximum is 80');
    }
    return words;
  }

  function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
  }

  function importChannel5(value) {
    var exact = nativeChannel5(value);
    return exact >= 0 ? exact : clamp(Math.round(value * 31 / 255), 0, 31);
  }

  function avatarCropRect(width, height, panX, panY) {
    if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1) {
      throw new ArtError('Avatar source dimensions must be positive integers');
    }
    panX = clamp(Number.isFinite(panX) ? panX : 0.5, 0, 1);
    panY = clamp(Number.isFinite(panY) ? panY : 0.5, 0, 1);
    var targetAspect = C.AVATAR_WIDTH / C.AVATAR_HEIGHT;
    var sourceAspect = width / height;
    if (sourceAspect > targetAspect) {
      var cropWidth = height * targetAspect;
      return {
        x: (width - cropWidth) * panX, y: 0,
        width: cropWidth, height: height,
        horizontalPanAvailable: width - cropWidth > 0.0001,
        verticalPanAvailable: false
      };
    }
    var cropHeight = width / targetAspect;
    return {
      x: 0, y: (height - cropHeight) * panY,
      width: width, height: cropHeight,
      horizontalPanAvailable: false,
      verticalPanAvailable: height - cropHeight > 0.0001
    };
  }

  function opaqueSourceChannels(rgba, pixel, background) {
    var offset = pixel * 4;
    var alpha = rgba[offset + 3];
    if (alpha === 255) return [rgba[offset], rgba[offset + 1], rgba[offset + 2]];
    if (alpha === 0) return background;
    var inverse = 255 - alpha;
    return [
      (rgba[offset] * alpha + background[0] * inverse) / 255,
      (rgba[offset + 1] * alpha + background[1] * inverse) / 255,
      (rgba[offset + 2] * alpha + background[2] * inverse) / 255
    ];
  }

  function nearestAvatarResize(rgba, width, height, crop, background) {
    var output = new Uint8ClampedArray(C.AVATAR_PIXELS * 4);
    for (var y = 0; y < C.AVATAR_HEIGHT; y++) {
      var sourceY = clamp(Math.floor(crop.y + (y + 0.5) * crop.height /
        C.AVATAR_HEIGHT), 0, height - 1);
      for (var x = 0; x < C.AVATAR_WIDTH; x++) {
        var sourceX = clamp(Math.floor(crop.x + (x + 0.5) * crop.width /
          C.AVATAR_WIDTH), 0, width - 1);
        var channels = opaqueSourceChannels(rgba, sourceY * width + sourceX, background);
        var offset = (y * C.AVATAR_WIDTH + x) * 4;
        output[offset] = Math.round(channels[0]);
        output[offset + 1] = Math.round(channels[1]);
        output[offset + 2] = Math.round(channels[2]);
        output[offset + 3] = 255;
      }
    }
    return output;
  }

  function bilinearAvatarResize(rgba, width, height, crop, background) {
    var output = new Uint8ClampedArray(C.AVATAR_PIXELS * 4);
    for (var y = 0; y < C.AVATAR_HEIGHT; y++) {
      var sourceY = crop.y + (y + 0.5) * crop.height / C.AVATAR_HEIGHT - 0.5;
      var rawY0 = Math.floor(sourceY);
      var y0 = clamp(rawY0, 0, height - 1);
      var y1 = clamp(rawY0 + 1, 0, height - 1);
      var fy = clamp(sourceY - rawY0, 0, 1);
      for (var x = 0; x < C.AVATAR_WIDTH; x++) {
        var sourceX = crop.x + (x + 0.5) * crop.width / C.AVATAR_WIDTH - 0.5;
        var rawX0 = Math.floor(sourceX);
        var x0 = clamp(rawX0, 0, width - 1);
        var x1 = clamp(rawX0 + 1, 0, width - 1);
        var fx = clamp(sourceX - rawX0, 0, 1);
        var topLeft = opaqueSourceChannels(rgba, y0 * width + x0, background);
        var topRight = opaqueSourceChannels(rgba, y0 * width + x1, background);
        var bottomLeft = opaqueSourceChannels(rgba, y1 * width + x0, background);
        var bottomRight = opaqueSourceChannels(rgba, y1 * width + x1, background);
        var offset = (y * C.AVATAR_WIDTH + x) * 4;
        for (var channel = 0; channel < 3; channel++) {
          var top = topLeft[channel] * (1 - fx) + topRight[channel] * fx;
          var bottom = bottomLeft[channel] * (1 - fx) + bottomRight[channel] * fx;
          output[offset + channel] = Math.round(top * (1 - fy) + bottom * fy);
        }
        output[offset + 3] = 255;
      }
    }
    return output;
  }

  function areaAvatarResize(rgba, width, height, crop, background) {
    var output = new Uint8ClampedArray(C.AVATAR_PIXELS * 4);
    for (var y = 0; y < C.AVATAR_HEIGHT; y++) {
      var sourceTop = crop.y + y * crop.height / C.AVATAR_HEIGHT;
      var sourceBottom = crop.y + (y + 1) * crop.height / C.AVATAR_HEIGHT;
      var firstY = clamp(Math.floor(sourceTop), 0, height - 1);
      var lastY = clamp(Math.ceil(sourceBottom) - 1, 0, height - 1);
      for (var x = 0; x < C.AVATAR_WIDTH; x++) {
        var sourceLeft = crop.x + x * crop.width / C.AVATAR_WIDTH;
        var sourceRight = crop.x + (x + 1) * crop.width / C.AVATAR_WIDTH;
        var firstX = clamp(Math.floor(sourceLeft), 0, width - 1);
        var lastX = clamp(Math.ceil(sourceRight) - 1, 0, width - 1);
        var sums = [0, 0, 0], total = 0;
        for (var sourceY = firstY; sourceY <= lastY; sourceY++) {
          var yWeight = Math.max(0, Math.min(sourceBottom, sourceY + 1) -
            Math.max(sourceTop, sourceY));
          for (var sourceX = firstX; sourceX <= lastX; sourceX++) {
            var xWeight = Math.max(0, Math.min(sourceRight, sourceX + 1) -
              Math.max(sourceLeft, sourceX));
            var weight = xWeight * yWeight;
            if (!weight) continue;
            var channels = opaqueSourceChannels(
              rgba, sourceY * width + sourceX, background);
            sums[0] += channels[0] * weight;
            sums[1] += channels[1] * weight;
            sums[2] += channels[2] * weight;
            total += weight;
          }
        }
        var offset = (y * C.AVATAR_WIDTH + x) * 4;
        output[offset] = Math.round(sums[0] / total);
        output[offset + 1] = Math.round(sums[1] / total);
        output[offset + 2] = Math.round(sums[2] / total);
        output[offset + 3] = 255;
      }
    }
    return output;
  }

  function resizeAvatarSource(rgba, width, height, crop, mode, background) {
    if (mode === 'nearest') {
      return nearestAvatarResize(rgba, width, height, crop, background);
    }
    if (mode !== 'smooth') throw new ArtError('Avatar resize mode is invalid');
    return crop.width >= C.AVATAR_WIDTH
      ? areaAvatarResize(rgba, width, height, crop, background)
      : bilinearAvatarResize(rgba, width, height, crop, background);
  }

  var WU_SIDE = 33;
  var WU_LENGTH = WU_SIDE * WU_SIDE * WU_SIDE;
  function wuIndex(red, green, blue) {
    return (red * WU_SIDE + green) * WU_SIDE + blue;
  }

  function wuPrefix(moment) {
    for (var red = 1; red < WU_SIDE; red++) {
      for (var green = 1; green < WU_SIDE; green++) {
        for (var blue = 1; blue < WU_SIDE; blue++) {
          var index = wuIndex(red, green, blue);
          moment[index] = moment[index] +
            moment[wuIndex(red - 1, green, blue)] +
            moment[wuIndex(red, green - 1, blue)] +
            moment[wuIndex(red, green, blue - 1)] -
            moment[wuIndex(red - 1, green - 1, blue)] -
            moment[wuIndex(red - 1, green, blue - 1)] -
            moment[wuIndex(red, green - 1, blue - 1)] +
            moment[wuIndex(red - 1, green - 1, blue - 1)];
        }
      }
    }
  }

  function wuVolume(moment, box) {
    return moment[wuIndex(box.r1, box.g1, box.b1)] -
      moment[wuIndex(box.r1, box.g1, box.b0)] -
      moment[wuIndex(box.r1, box.g0, box.b1)] -
      moment[wuIndex(box.r0, box.g1, box.b1)] +
      moment[wuIndex(box.r1, box.g0, box.b0)] +
      moment[wuIndex(box.r0, box.g1, box.b0)] +
      moment[wuIndex(box.r0, box.g0, box.b1)] -
      moment[wuIndex(box.r0, box.g0, box.b0)];
  }

  function wuVariance(moments, box) {
    var weight = wuVolume(moments.weight, box);
    if (!weight) return 0;
    var red = wuVolume(moments.red, box);
    var green = wuVolume(moments.green, box);
    var blue = wuVolume(moments.blue, box);
    return Math.max(0, wuVolume(moments.square, box) -
      (red * red + green * green + blue * blue) / weight);
  }

  function wuBestSplit(moments, box) {
    var parentVariance = wuVariance(moments, box);
    var best = null;
    ['r', 'g', 'b'].forEach(function(axis) {
      for (var cut = box[axis + '0'] + 1; cut < box[axis + '1']; cut++) {
        var left = Object.assign({}, box), right = Object.assign({}, box);
        left[axis + '1'] = cut;
        right[axis + '0'] = cut;
        if (!wuVolume(moments.weight, left) || !wuVolume(moments.weight, right)) {
          continue;
        }
        var gain = parentVariance - wuVariance(moments, left) -
          wuVariance(moments, right);
        if (!best || gain > best.gain + 1e-9) {
          best = { left: left, right: right, gain: gain };
        }
      }
    });
    return best;
  }

  function wordChannels(word) {
    return [(word >>> 11) & 31, (word >>> 6) & 31, (word >>> 1) & 31];
  }

  function nearestPaletteWordForChannels(palette, red, green, blue) {
    var best = palette[0], bestDistance = Infinity;
    for (var index = 0; index < palette.length; index++) {
      var channels = wordChannels(palette[index]);
      var dr = red - channels[0], dg = green - channels[1], db = blue - channels[2];
      var distance = dr * dr + dg * dg + db * db;
      if (distance < bestDistance) {
        best = palette[index]; bestDistance = distance;
        if (!distance) break;
      }
    }
    return best;
  }

  function wuQuantizeAvatarWords(words, maximum, dither) {
    maximum = Math.max(1, Math.min(C.AVATAR_PALETTE_WORDS, maximum ||
      C.AVATAR_PALETTE_WORDS));
    var unique = new Set(words);
    if (unique.size <= maximum) {
      return {
        words: words.slice(), paletteWords: Array.from(unique).sort(function(a, b) {
          return a - b;
        }), quantized: false, dithered: false
      };
    }
    var moments = {
      weight: new Float64Array(WU_LENGTH),
      red: new Float64Array(WU_LENGTH),
      green: new Float64Array(WU_LENGTH),
      blue: new Float64Array(WU_LENGTH),
      square: new Float64Array(WU_LENGTH)
    };
    for (var pixel = 0; pixel < words.length; pixel++) {
      var channels = wordChannels(words[pixel]);
      var index = wuIndex(channels[0] + 1, channels[1] + 1, channels[2] + 1);
      moments.weight[index]++;
      moments.red[index] += channels[0];
      moments.green[index] += channels[1];
      moments.blue[index] += channels[2];
      moments.square[index] += channels[0] * channels[0] +
        channels[1] * channels[1] + channels[2] * channels[2];
    }
    Object.keys(moments).forEach(function(key) { wuPrefix(moments[key]); });
    var boxes = [{ r0: 0, r1: 32, g0: 0, g1: 32, b0: 0, b1: 32 }];
    while (boxes.length < maximum) {
      var chosen = null;
      for (var boxIndex = 0; boxIndex < boxes.length; boxIndex++) {
        var candidate = wuBestSplit(moments, boxes[boxIndex]);
        if (candidate && (!chosen || candidate.gain > chosen.split.gain + 1e-9)) {
          chosen = { index: boxIndex, split: candidate };
        }
      }
      if (!chosen) break;
      boxes[chosen.index] = chosen.split.left;
      boxes.push(chosen.split.right);
    }
    var palette = [];
    boxes.forEach(function(box) {
      var weight = wuVolume(moments.weight, box);
      if (!weight) return;
      var red = clamp(Math.round(wuVolume(moments.red, box) / weight), 0, 31);
      var green = clamp(Math.round(wuVolume(moments.green, box) / weight), 0, 31);
      var blue = clamp(Math.round(wuVolume(moments.blue, box) / weight), 0, 31);
      var word = rgba5551Word(red, green, blue, true);
      if (palette.indexOf(word) < 0) palette.push(word);
    });
    palette.sort(function(a, b) { return a - b; });
    var output = new Uint16Array(words.length);
    var cache = new Map();
    var bayer = [
      0, 8, 2, 10,
      12, 4, 14, 6,
      3, 11, 1, 9,
      15, 7, 13, 5
    ];
    for (var outputPixel = 0; outputPixel < words.length; outputPixel++) {
      var inputWord = words[outputPixel];
      if (!dither && cache.has(inputWord)) {
        output[outputPixel] = cache.get(inputWord); continue;
      }
      var input = wordChannels(inputWord);
      var offset = dither
        ? (bayer[(outputPixel % C.AVATAR_WIDTH) % 4 +
          (Math.floor(outputPixel / C.AVATAR_WIDTH) % 4) * 4] - 7.5) / 8
        : 0;
      var nearest = nearestPaletteWordForChannels(
        palette, input[0] + offset, input[1] + offset, input[2] + offset);
      output[outputPixel] = nearest;
      if (!dither) cache.set(inputWord, nearest);
    }
    return {
      words: output, paletteWords: palette,
      quantized: true, dithered: !!dither
    };
  }

  function prepareAvatarImport(rgba, width, height, options) {
    options = options || {};
    if (!ArrayBuffer.isView(rgba) || rgba.length !== width * height * 4) {
      throw new ArtError('Avatar source did not decode to the declared RGBA dimensions');
    }
    var backgroundWord = options.backgroundWord === undefined
      ? rgba5551Word(0, 0, 0, true) : options.backgroundWord;
    if (!(backgroundWord & 1)) throw new ArtError('Avatar import background must be opaque');
    var background = rgba5551(backgroundWord).slice(0, 3);
    var crop = avatarCropRect(width, height, options.panX, options.panY);
    var mode = options.resizeMode || 'nearest';
    var resized = resizeAvatarSource(rgba, width, height, crop, mode, background);
    var nativeWords = new Uint16Array(C.AVATAR_PIXELS);
    for (var pixel = 0; pixel < C.AVATAR_PIXELS; pixel++) {
      var offset = pixel * 4;
      nativeWords[pixel] = rgba5551Word(
        importChannel5(resized[offset]),
        importChannel5(resized[offset + 1]),
        importChannel5(resized[offset + 2]), true);
    }
    var sourceNativeColorCount = new Set(nativeWords).size;
    var quantized = wuQuantizeAvatarWords(
      nativeWords, C.AVATAR_PALETTE_WORDS, !!options.dither);
    var transparentSourcePixels = 0;
    for (var sourcePixel = 0; sourcePixel < width * height; sourcePixel++) {
      if (rgba[sourcePixel * 4 + 3] !== 255) transparentSourcePixels++;
    }
    return {
      words: quantized.words,
      paletteWords: quantized.paletteWords,
      colorCount: new Set(quantized.words).size,
      sourceNativeColorCount: sourceNativeColorCount,
      quantized: quantized.quantized,
      dithered: quantized.dithered,
      crop: crop,
      resizeMode: mode,
      backgroundWord: backgroundWord,
      transparentSourcePixels: transparentSourcePixels
    };
  }

  function colorCss(word) {
    var color = rgba5551(word);
    return color[3] ? 'rgb(' + color[0] + ',' + color[1] + ',' + color[2] + ')' : 'transparent';
  }

  async function sha256Hex(bytes) {
    if (!window.crypto || !window.crypto.subtle) {
      throw new ArtError('This browser cannot verify the ROM SHA-256 identity.');
    }
    var digest = new Uint8Array(await window.crypto.subtle.digest('SHA-256', bytes));
    return Array.from(digest).map(function(value) {
      return value.toString(16).padStart(2, '0');
    }).join('').toUpperCase();
  }

  function readResource(bytes, key) {
    if (key < 0 || key >= 0x10000000 || (key & 1)) {
      throw new ArtError('resource key is outside the aligned 28-bit domain: ' + hex(key));
    }
    var entry = C.RESOURCE_BASE + key;
    var storedLength = readU32(bytes, entry);
    var start = entry + 4;
    var end = start + storedLength;
    if (end > bytes.length) throw new ArtError('resource payload lies outside ROM: ' + hex(key));
    return {
      key: key, entry: entry, storedLength: storedLength,
      stored: bytes.slice(start, end), envelopeLength: 4 + storedLength + (storedLength & 1)
    };
  }

  function bootLzDecode(stored) {
    if (stored.length < 4) throw new ArtError('boot-LZ resource lacks decoded-length word');
    var wanted = readU32(stored, 0);
    var cursor = 4;
    var output = new Uint8Array(wanted);
    var out = 0;
    function need(count) {
      if (cursor + count > stored.length) throw new ArtError('truncated boot-LZ token');
    }
    function back(distanceField, length) {
      var source = out - distanceField - 1;
      if (source < 0) throw new ArtError('boot-LZ back-reference precedes output');
      if (out + length > wanted) throw new ArtError('boot-LZ back-reference overruns output');
      for (var i = 0; i < length; i++) output[out++] = output[source + i];
    }
    while (out < wanted) {
      need(1);
      var control = stored[cursor++];
      if (control & 0x80) {
        need(1);
        back(((control & 7) << 8) | stored[cursor++], ((control >>> 3) & 15) + 3);
      } else if (control & 0x40) {
        var literalLength = (control & 63) + 1;
        need(literalLength);
        if (out + literalLength > wanted) throw new ArtError('boot-LZ literal overruns output');
        output.set(stored.subarray(cursor, cursor + literalLength), out);
        out += literalLength;
        cursor += literalLength;
      } else if (control & 0x20) {
        var zeroShort = (control & 31) + 2;
        if (out + zeroShort > wanted) throw new ArtError('boot-LZ zero run overruns output');
        out += zeroShort;
      } else if (control & 0x10) {
        need(2);
        var byte1 = stored[cursor++], byte2 = stored[cursor++];
        back(((byte1 & 63) << 8) | byte2,
          ((control & 15) | ((byte1 >>> 2) & 48)) + 4);
      } else if (control === 0) {
        need(3);
        var longLength = stored[cursor++] + 5;
        back((stored[cursor++] << 8) | stored[cursor++], longLength);
      } else if (control === 1 || control === 2) {
        need(1);
        var fillLength = stored[cursor++] + 3;
        if (out + fillLength > wanted) throw new ArtError('boot-LZ fill overruns output');
        output.fill(control === 1 ? 0xFF : 0, out, out + fillLength);
        out += fillLength;
      }
      // Controls 0x03..0x0F are one-byte no-ops in the retail decoder.
    }
    return { output: output, bytesConsumed: cursor };
  }

  function readCompressedResource(bytes, key) {
    var resource = readResource(bytes, key);
    var decoded = bootLzDecode(resource.stored);
    if (decoded.bytesConsumed !== resource.storedLength) {
      throw new ArtError('resource ' + hex(key) + ' leaves unread compressed bytes');
    }
    resource.decoded = decoded.output;
    return resource;
  }

  // Minimum-byte encoder ported from scripts/ob64_lzss_compress.js.
  function bootLzCompressTokens(input) {
    var size = input.length;
    if (!size) return new Uint8Array(0);
    var shortLength = new Uint16Array(size);
    var mediumLength = new Uint16Array(size);
    var longLength = new Uint16Array(size);
    var shortDistance = new Uint32Array(size);
    var mediumDistance = new Uint32Array(size);
    var longDistance = new Uint32Array(size);
    var previous = new Int32Array(size);
    previous.fill(-1);
    var heads = new Map();
    function record(lengths, distances, pos, length, distance, maximum) {
      var capped = Math.min(length, maximum);
      if (capped > lengths[pos] || (capped === lengths[pos] &&
          (!distances[pos] || distance < distances[pos]))) {
        lengths[pos] = capped; distances[pos] = distance;
      }
    }
    for (var pos = 0; pos + 2 < size; pos++) {
      var prefix = (input[pos] << 16) | (input[pos + 1] << 8) | input[pos + 2];
      var candidate = heads.has(prefix) ? heads.get(prefix) : -1;
      previous[pos] = candidate;
      heads.set(prefix, pos);
      var maximumLength = Math.min(260, size - pos);
      while (candidate >= 0) {
        var distance = pos - candidate;
        if (distance > 0x10000) break;
        var length = 3;
        while (length < maximumLength &&
            input[pos + length] === input[pos + length - distance]) length++;
        if (distance <= 2048) record(shortLength, shortDistance, pos, length, distance, 18);
        if (distance <= 16384 && length >= 4) record(mediumLength, mediumDistance, pos, length, distance, 67);
        if (length >= 5) record(longLength, longDistance, pos, length, distance, 260);
        var shortDone = shortLength[pos] === Math.min(18, maximumLength);
        var mediumDone = maximumLength < 4 || mediumLength[pos] === Math.min(67, maximumLength);
        var longDone = maximumLength < 5 || longLength[pos] === maximumLength;
        if (shortDone && mediumDone && longDone) break;
        candidate = previous[candidate];
      }
    }
    var zeroRun = new Uint16Array(size + 1), ffRun = new Uint16Array(size + 1);
    for (var runPos = size - 1; runPos >= 0; runPos--) {
      if (input[runPos] === 0) zeroRun[runPos] = Math.min(258, zeroRun[runPos + 1] + 1);
      if (input[runPos] === 0xFF) ffRun[runPos] = Math.min(258, ffRun[runPos + 1] + 1);
    }
    var byteCost = new Uint32Array(size + 1), commandCost = new Uint32Array(size + 1);
    var choiceToken = new Uint8Array(size), choiceLength = new Uint16Array(size);
    var choiceDistance = new Uint32Array(size);
    for (var planPos = size - 1; planPos >= 0; planPos--) {
      var bestBytes = Number.MAX_SAFE_INTEGER, bestCommands = Number.MAX_SAFE_INTEGER;
      var bestToken = 0, bestLength = 0, bestDistance = 0, bestPriority = 99;
      function consider(token, length, distance, tokenBytes, priority) {
        var totalBytes = tokenBytes + byteCost[planPos + length];
        var totalCommands = 1 + commandCost[planPos + length];
        var better = totalBytes < bestBytes ||
          (totalBytes === bestBytes && totalCommands < bestCommands) ||
          (totalBytes === bestBytes && totalCommands === bestCommands && priority < bestPriority) ||
          (totalBytes === bestBytes && totalCommands === bestCommands && priority === bestPriority && length > bestLength) ||
          (totalBytes === bestBytes && totalCommands === bestCommands && priority === bestPriority && length === bestLength && distance < bestDistance);
        if (!better) return;
        bestBytes = totalBytes; bestCommands = totalCommands; bestToken = token;
        bestLength = length; bestDistance = distance; bestPriority = priority;
      }
      var length;
      for (length = 1; length <= 64 && planPos + length <= size; length++) consider(1, length, 0, length + 1, 6);
      for (length = 2; length <= Math.min(33, zeroRun[planPos]); length++) consider(2, length, 0, 1, 0);
      for (length = 3; length <= ffRun[planPos]; length++) consider(3, length, 0, 2, 1);
      for (length = 3; length <= zeroRun[planPos]; length++) consider(4, length, 0, 2, 2);
      for (length = 3; length <= shortLength[planPos]; length++) consider(5, length, shortDistance[planPos], 2, 3);
      for (length = 4; length <= mediumLength[planPos]; length++) consider(6, length, mediumDistance[planPos], 3, 4);
      for (length = 5; length <= longLength[planPos]; length++) consider(7, length, longDistance[planPos], 4, 5);
      if (!bestToken) throw new ArtError('boot-LZ planner could not encode byte ' + planPos);
      byteCost[planPos] = bestBytes; commandCost[planPos] = bestCommands;
      choiceToken[planPos] = bestToken; choiceLength[planPos] = bestLength;
      choiceDistance[planPos] = bestDistance;
    }
    var output = new Uint8Array(byteCost[0]);
    var inputPos = 0, outputPos = 0;
    while (inputPos < size) {
      var token = choiceToken[inputPos], len = choiceLength[inputPos];
      var dist = choiceDistance[inputPos], encodedDistance = dist - 1;
      if (token === 1) {
        output[outputPos++] = 0x40 | (len - 1);
        output.set(input.subarray(inputPos, inputPos + len), outputPos); outputPos += len;
      } else if (token === 2) output[outputPos++] = 0x20 | (len - 2);
      else if (token === 3) { output[outputPos++] = 1; output[outputPos++] = len - 3; }
      else if (token === 4) { output[outputPos++] = 2; output[outputPos++] = len - 3; }
      else if (token === 5) {
        output[outputPos++] = 0x80 | ((len - 3) << 3) | ((encodedDistance >>> 8) & 7);
        output[outputPos++] = encodedDistance & 0xFF;
      } else if (token === 6) {
        var encodedLength = len - 4;
        output[outputPos++] = 0x10 | (encodedLength & 15);
        output[outputPos++] = ((encodedLength & 48) << 2) | ((encodedDistance >>> 8) & 63);
        output[outputPos++] = encodedDistance & 0xFF;
      } else {
        output[outputPos++] = 0; output[outputPos++] = len - 5;
        output[outputPos++] = (encodedDistance >>> 8) & 0xFF;
        output[outputPos++] = encodedDistance & 0xFF;
      }
      inputPos += len;
    }
    return output;
  }

  function bootLzCompress(decoded) {
    var tokens = bootLzCompressTokens(decoded);
    var stored = new Uint8Array(tokens.length + 4);
    writeU32(stored, 0, decoded.length);
    stored.set(tokens, 4);
    var verified = bootLzDecode(stored);
    if (verified.bytesConsumed !== stored.length || !equalBytes(verified.output, decoded)) {
      throw new ArtError('boot-LZ compressor failed exact independent round trip');
    }
    return stored;
  }

  function resolveSplitKey(bytes, sites, asset) {
    var key = null;
    sites.forEach(function(site) {
      var lui = readU32(bytes, site.lui), ori = readU32(bytes, site.ori);
      var register = (lui >>> 16) & 31;
      if ((lui >>> 26) !== 15 || (ori >>> 26) !== 13 ||
          ((ori >>> 21) & 31) !== register || ((ori >>> 16) & 31) !== register) {
        throw new ArtError(asset + ' key owner ' + site.name + ' lacks its guarded LUI/ORI preimage');
      }
      var observed = (((lui & 0xFFFF) * 0x10000) + (ori & 0xFFFF)) >>> 0;
      if (key === null) key = observed;
      else if (key !== observed) throw new ArtError(asset + ' key owner sites disagree');
    });
    return key;
  }

  function patchSplitKey(bytes, sites, expected, replacement, ranges, log, asset) {
    sites.forEach(function(site) {
      var lui = readU32(bytes, site.lui), ori = readU32(bytes, site.ori);
      var register = (lui >>> 16) & 31;
      var observed = (((lui & 0xFFFF) * 0x10000) + (ori & 0xFFFF)) >>> 0;
      if ((lui >>> 26) !== 15 || (ori >>> 26) !== 13 ||
          ((ori >>> 21) & 31) !== register || ((ori >>> 16) & 31) !== register ||
          observed !== expected) {
        exportFailure(asset, 'pointer owner preimage', hex(observed), hex(expected),
          'reload the original source ROM and reapply the Project');
      }
      writeU32(bytes, site.lui, ((lui & 0xFFFF0000) | ((replacement >>> 16) & 0xFFFF)) >>> 0);
      writeU32(bytes, site.ori, ((ori & 0xFFFF0000) | (replacement & 0xFFFF)) >>> 0);
      ranges.push([site.lui, site.lui + 4], [site.ori, site.ori + 4]);
      log.push(asset + ' pointer ' + site.name + ': ' + hex(expected) + ' -> ' + hex(replacement));
    });
  }

  function parseAvatarBundle(decoded, key) {
    var paletteBytes = C.AVATAR_PALETTE_WORDS * 2;
    if (decoded.length < C.AVATAR_PIXELS + paletteBytes ||
        (decoded.length - C.AVATAR_PIXELS) % paletteBytes) {
      throw new ArtError('avatar bundle ' + hex(key) + ' has an invalid decoded extent');
    }
    var count = (decoded.length - C.AVATAR_PIXELS) / paletteBytes;
    if (count < 1 || count > 8) throw new ArtError('avatar bundle palette count is unsupported');
    var indices = decoded.slice(0, C.AVATAR_PIXELS);
    for (var i = 0; i < indices.length; i++) if (indices[i] >= 80) {
      throw new ArtError('avatar bundle index exceeds 79');
    }
    var palettes = [];
    for (var ordinal = 0; ordinal < count; ordinal++) {
      palettes.push(wordsFromBytes(decoded,
        C.AVATAR_PIXELS + ordinal * paletteBytes, C.AVATAR_PALETTE_WORDS));
    }
    return { key: key, indices: indices, palettes: palettes, decoded: decoded };
  }

  function renderIndexedWords(indices, palette) {
    var words = new Uint16Array(indices.length);
    for (var i = 0; i < indices.length; i++) words[i] = palette[indices[i]];
    return words;
  }

  function distinctCount(words, opaqueOnly) {
    var seen = new Set();
    for (var i = 0; i < words.length; i++) {
      if (!opaqueOnly || (words[i] & 1)) seen.add(words[i]);
    }
    return seen.size;
  }

  function parseAvatars(z64) {
    var descriptorKey = resolveSplitKey(z64, C.AVATAR_DESCRIPTOR_SITES, 'avatar descriptor');
    if (descriptorKey !== C.AVATAR_DESCRIPTOR_KEY) {
      throw new ArtError('vanilla avatar descriptor resolves to unexpected key ' + hex(descriptorKey));
    }
    var descriptor = readResource(z64, descriptorKey);
    if (descriptor.storedLength !== C.AVATAR_DESCRIPTOR_SLOTS * 4) {
      throw new ArtError('avatar descriptor has ' + (descriptor.storedLength / 4) + ' slots; expected 142');
    }
    var slots = [];
    for (var s = 0; s < C.AVATAR_DESCRIPTOR_SLOTS; s++) slots.push(readU32(descriptor.stored, s * 4));
    if (slots[0] !== 0) throw new ArtError('avatar descriptor slot zero is not null');
    var bundleCache = {};
    function bundleFor(key) {
      if (!bundleCache[key]) bundleCache[key] = parseAvatarBundle(readCompressedResource(z64, key).decoded, key);
      return bundleCache[key];
    }
    var appearances = [], byKey = {}, colorSet = new Set();
    for (var classId = 0; classId < C.AVATAR_CLASS_COUNT; classId++) {
      var routes = [];
      for (var selector = 0; selector < 2; selector++) {
        var routeOffset = C.AVATAR_ROUTE_TABLE + classId * C.AVATAR_ROUTE_STRIDE + selector * 2;
        var token = readU16(z64, routeOffset);
        if (!token) continue;
        var memberIndex = token >>> 3, paletteOrdinal = token & 7;
        if (memberIndex >= slots.length || !slots[memberIndex]) throw new ArtError('class avatar route selects invalid member');
        var bundle = bundleFor(slots[memberIndex]);
        if (paletteOrdinal >= bundle.palettes.length) throw new ArtError('class avatar route selects invalid palette');
        var words = renderIndexedWords(bundle.indices, bundle.palettes[paletteOrdinal]);
        routes.push({
          selectorIndex: selector, token: token, memberIndex: memberIndex,
          paletteOrdinal: paletteOrdinal, resourceKey: slots[memberIndex],
          routeOffset: routeOffset, words: words
        });
      }
      if (!routes.length) continue;
      var groups = [];
      routes.forEach(function(route) {
        var match = groups.find(function(group) { return equalWords(group.words, route.words); });
        if (match) match.routes.push(route);
        else groups.push({ words: route.words, routes: [route] });
      });
      groups.forEach(function(group, groupIndex) {
        var selectors = group.routes.map(function(route) { return route.selectorIndex; });
        var key = classId + ':' + selectors.join('-');
        var classLabel = cleanName(OB64.className ? OB64.className(classId) : '',
          'Class ' + hex(classId, 2));
        var appearanceLabel = groups.length === 1
          ? (selectors.length === 2 ? 'Shared appearance' : 'Appearance')
          : (selectors[0] === 0 ? 'Appearance A' : 'Appearance B');
        var appearance = {
          key: key, classId: classId, className: classLabel,
          label: appearanceLabel, selectorIndices: selectors,
          routes: group.routes, originalWords: group.words.slice(), groupIndex: groupIndex
        };
        appearances.push(appearance); byKey[key] = appearance;
      });
    }
    slots.forEach(function(key) { if (key) bundleFor(key); });
    Object.keys(bundleCache).forEach(function(key) {
      bundleCache[key].palettes.forEach(function(palette) {
        bundleCache[key].indices.forEach(function(index) {
          var word = palette[index];
          if (word & 1) colorSet.add(word);
        });
      });
    });
    if (colorSet.size !== C.AVATAR_COLOR_LIBRARY_SIZE) {
      throw new ArtError('avatar color library has ' + colorSet.size + ' colors; expected 3464');
    }
    return {
      descriptor: descriptor, descriptorSlots: slots, bundleCache: bundleCache,
      appearances: appearances, byKey: byKey,
      colorLibrary: Array.from(colorSet), edits: {}, history: {}, bulkUndo: null
    };
  }

  function parseIconPack(z64, spec) {
    var activeKey = resolveSplitKey(z64, spec.sites, spec.label + ' icon pack');
    if (activeKey !== spec.resourceKey) {
      throw new ArtError(spec.label + ' icon pointers select ' + hex(activeKey) +
        ' instead of the verified retail resource ' + hex(spec.resourceKey) +
        '. Loading an already-relocated icon pack is not implemented.');
    }
    var resource = readCompressedResource(z64, spec.resourceKey);
    if (resource.entry !== spec.sizeWord || resource.storedLength !== spec.retailStoredLength) {
      throw new ArtError(spec.label + ' icon pack retail envelope differs from verified layout');
    }
    if (resource.decoded.length !== spec.decodedLength ||
        resource.decoded.length !== C.ICON_PALETTE_WORDS * 2 + spec.iconCount * C.ICON_PIXELS) {
      throw new ArtError(spec.label + ' icon pack decoded length differs from verified layout');
    }
    var palette = wordsFromBytes(resource.decoded, 0, 256);
    var transparent = [];
    for (var p = 0; p < palette.length; p++) if (!(palette[p] & 1)) transparent.push(p);
    if (transparent.length !== 1 || transparent[0] !== spec.transparentIndex) {
      throw new ArtError(spec.label + ' icon pack transparent index differs from verified layout');
    }
    var icons = [], byId = {};
    for (var ordinal = 0; ordinal < spec.iconCount; ordinal++) {
      var planeOffset = 512 + ordinal * 256;
      var plane = resource.decoded.slice(planeOffset, planeOffset + 256);
      var itemId = ordinal + 1;
      var name = spec.slug === 'equipment'
        ? cleanName(OB64.itemName ? OB64.itemName(itemId) : '', 'Equipment ' + itemId)
        : cleanName(OB64.consumableName ? OB64.consumableName(itemId) : '', 'Special Item ' + itemId);
      var icon = {
        key: spec.slug + ':' + itemId, pack: spec.slug, packLabel: spec.label,
        itemId: itemId, name: name, originalPlane: plane,
        originalWords: renderIndexedWords(plane, palette)
      };
      icons.push(icon); byId[itemId] = icon;
    }
    var paletteColorSet = new Set(), colorLibrary = [];
    palette.forEach(function(word) {
      if ((word & 1) && !paletteColorSet.has(word)) {
        paletteColorSet.add(word); colorLibrary.push(word);
      }
    });
    if (colorLibrary.length !== 255) {
      throw new ArtError(spec.label + ' icon pack has ' + colorLibrary.length +
        ' distinct opaque palette colors; expected 255');
    }
    return {
      spec: spec, resource: resource, palette: palette, icons: icons,
      byId: byId, transparentWord: palette[spec.transparentIndex],
      colorLibrary: colorLibrary, paletteColorSet: paletteColorSet, edits: {}
    };
  }

  function parseIcons(z64) {
    var packs = {}, icons = [], byKey = {}, colors = new Set();
    C.ICON_PACKS.forEach(function(spec) {
      var pack = parseIconPack(z64, spec); packs[spec.slug] = pack;
      pack.icons.forEach(function(icon) { icons.push(icon); byKey[icon.key] = icon; });
      pack.icons.forEach(function(icon) {
        icon.originalWords.forEach(function(word) { if (word & 1) colors.add(word); });
      });
    });
    if (colors.size !== C.ICON_COLOR_LIBRARY_SIZE) {
      throw new ArtError('icon color library has ' + colors.size + ' colors; expected 499');
    }
    return { packs: packs, icons: icons, byKey: byKey, colorLibrary: Array.from(colors), edits: {}, history: {} };
  }

  function currentWords(state, kind, key) {
    if (kind === 'avatar') {
      return state.avatar.edits[key] ? state.avatar.edits[key].words : state.avatar.byKey[key].originalWords;
    }
    return state.icons.edits[key] ? state.icons.edits[key].words : state.icons.byKey[key].originalWords;
  }

  function blockedMap(state, kind) {
    return kind === 'avatar' ? state.blocked.avatars : state.blocked.icons;
  }

  function setBlocked(state, kind, key, message) {
    var map = blockedMap(state, kind);
    if (!message) delete map[key];
    else map[key] = { source: 'edit', message: String(message) };
  }

  function blockedReason(state, kind, key) {
    var row = blockedMap(state, kind)[key];
    return row && (row.message || String(row));
  }

  function refreshIconPackBlocked(state, slug) {
    var pack = state.icons.packs[slug], colors = new Set();
    pack.icons.forEach(function(icon) {
      currentWords(state, 'icon', icon.key).forEach(function(word) {
        if (word & 1) colors.add(word);
      });
    });
    var reason = colors.size > 255
      ? pack.spec.label + ' pack uses ' + colors.size + ' opaque colors; maximum is 255.'
      : '';
    pack.icons.forEach(function(icon) {
      var row = state.blocked.icons[icon.key];
      if (row && row.source === 'pack-capacity') delete state.blocked.icons[icon.key];
      if (reason && state.icons.edits[icon.key]) {
        state.blocked.icons[icon.key] = { source: 'pack-capacity', message: reason };
      }
    });
    return colors.size;
  }

  function validateAvatarWords(state, words, label) {
    if (!(words instanceof Uint16Array) || words.length !== C.AVATAR_PIXELS) {
      throw new ArtError(label + ' must contain exactly 1,920 RGBA5551 words');
    }
    for (var i = 0; i < words.length; i++) {
      if (!(words[i] & 1)) throw new ArtError(label + ' contains transparency at pixel ' + i);
    }
    var count = distinctCount(words, true);
    if (count > 80) throw new ArtError(label + ' uses ' + count + ' opaque colors; maximum is 80');
    return count;
  }

  function validateIconWords(state, pack, words, label) {
    if (!(words instanceof Uint16Array) || words.length !== C.ICON_PIXELS) {
      throw new ArtError(label + ' must contain exactly 256 RGBA5551 words');
    }
    for (var i = 0; i < words.length; i++) {
      if (!(words[i] & 1) && words[i] !== pack.transparentWord) {
        throw new ArtError(label + ' uses an unsupported transparent color word');
      }
      if ((words[i] & 1) && !pack.paletteColorSet.has(words[i])) {
        throw new ArtError(label + ' uses color ' + hex(words[i], 4) +
          ' outside the ' + pack.spec.label + ' 256-entry palette');
      }
    }
  }

  function historyFor(state, kind, key) {
    var map = kind === 'avatar' ? state.avatar.history : state.icons.history;
    if (!map[key]) map[key] = { undo: [], redo: [] };
    return map[key];
  }

  function setEditWords(state, kind, key, words, options) {
    options = options || {};
    var object = kind === 'avatar' ? state.avatar.byKey[key] : state.icons.byKey[key];
    if (!object) throw new ArtError('unknown ' + kind + ' asset ' + key);
    var before = currentWords(state, kind, key).slice();
    if (equalWords(before, words)) return false;
    if (kind === 'avatar') validateAvatarWords(state, words, object.className + ' ' + object.label);
    else validateIconWords(state, state.icons.packs[object.pack], words, object.name);
    if (options.history !== false) {
      var history = historyFor(state, kind, key);
      history.undo.push(before); if (history.undo.length > 100) history.undo.shift();
      history.redo = [];
    }
    var original = object.originalWords;
    var edits = kind === 'avatar' ? state.avatar.edits : state.icons.edits;
    if (equalWords(original, words)) delete edits[key];
    else edits[key] = { words: words.slice() };
    setBlocked(state, kind, key, '');
    if (kind === 'icon') refreshIconPackBlocked(state, object.pack);
    return true;
  }

  function undo(state, kind, key) {
    var history = historyFor(state, kind, key);
    if (!history.undo.length) return false;
    var current = currentWords(state, kind, key).slice();
    var prior = history.undo.pop(); history.redo.push(current);
    return setEditWords(state, kind, key, prior, { history: false });
  }

  function redo(state, kind, key) {
    var history = historyFor(state, kind, key);
    if (!history.redo.length) return false;
    var current = currentWords(state, kind, key).slice();
    var next = history.redo.pop(); history.undo.push(current);
    return setEditWords(state, kind, key, next, { history: false });
  }

  function editCount(state) {
    return Object.keys(state.avatar.edits).length + Object.keys(state.icons.edits).length;
  }

  function blockedCount(state) {
    if (!state || !state.blocked) return 0;
    return Object.keys(state.blocked.avatars).length + Object.keys(state.blocked.icons).length;
  }

  function hasPendingExport(state) {
    return editCount(state) > 0 || !!state.lastExport;
  }

  async function initialize(rom) {
    var hash = await sha256Hex(rom.z64);
    var expectedHash = rom.layout && rom.layout.id === 'us-rev0'
      ? C.REV0_Z64_SHA256
      : (rom.layout && rom.layout.id === 'us-rev1' ? C.REV1_Z64_SHA256 : '');
    var state = {
      schemaVersion: 1, identityHash: hash, retailZ64: rom.z64.slice(),
      exactRetail: !!expectedHash && hash === expectedHash,
      supported: false,
      unavailableReason: '', lastExport: null, selectedTab: 'avatars',
      blocked: { avatars: {}, icons: {} }
    };
    if (!rom.layout || rom.layout.id !== 'us-rev0') {
      state.unavailableReason = 'Art editing is verified for US retail header rev 0 only. Other readable editor tabs remain available for this ROM.';
      rom.art = state;
      return state;
    }
    try {
      state.avatar = parseAvatars(rom.z64);
      state.icons = parseIcons(rom.z64);
    } catch (error) {
      state.unavailableReason = 'Native art is unavailable for this ROM: ' +
        (error && error.message ? error.message : String(error));
      rom.art = state;
      return state;
    }
    state.supported = true;
    state.avatarRetailColorSet = new Set(state.avatar.colorLibrary);
    state.iconRetailColorSet = new Set(state.icons.colorLibrary);
    rom.art = state;
    return state;
  }

  function collectProjectPayload(rom) {
    var state = rom && rom.art;
    if (!state || !state.supported) return { schemaVersion: 1, avatars: {}, icons: {} };
    var avatars = {}, icons = {};
    Object.keys(state.avatar.edits).sort().forEach(function(key) {
      var appearance = state.avatar.byKey[key];
      avatars[key] = {
        classId: appearance.classId,
        selectorIndices: appearance.selectorIndices.slice(),
        pixelsRgba5551BeBase64: wordArrayToBase64(state.avatar.edits[key].words)
      };
    });
    Object.keys(state.icons.edits).sort().forEach(function(key) {
      var icon = state.icons.byKey[key];
      icons[key] = {
        pack: icon.pack, itemId: icon.itemId,
        pixelsRgba5551BeBase64: wordArrayToBase64(state.icons.edits[key].words)
      };
    });
    return { schemaVersion: 1, avatars: avatars, icons: icons };
  }

  function prepareProjectPayload(rom, payload) {
    var state = rom && rom.art;
    if (payload === undefined || payload === null) return { avatars: {}, icons: {}, count: 0 };
    if (!state || !state.supported) {
      if (payload && typeof payload === 'object' &&
          !Object.keys(payload.avatars || {}).length && !Object.keys(payload.icons || {}).length) {
        return { avatars: {}, icons: {}, count: 0 };
      }
      throw new ArtError('This ROM revision cannot load Art and Animation Project records');
    }
    if (typeof payload !== 'object' || Array.isArray(payload) || payload.schemaVersion !== 1) {
      throw new ArtError('patches.art must use schemaVersion 1');
    }
    var prepared = { avatars: {}, icons: {}, count: 0 };
    Object.keys(payload.avatars || {}).forEach(function(key) {
      var entry = payload.avatars[key], appearance = state.avatar.byKey[key];
      if (!appearance || !entry || entry.classId !== appearance.classId ||
          !Array.isArray(entry.selectorIndices) ||
          entry.selectorIndices.join(',') !== appearance.selectorIndices.join(',')) {
        throw new ArtError('avatar Project record ' + key + ' does not match a vanilla routed appearance');
      }
      var words = wordArrayFromBase64(entry.pixelsRgba5551BeBase64,
        C.AVATAR_PIXELS, 'avatar Project record ' + key);
      validateAvatarWords(state, words, appearance.className + ' ' + appearance.label);
      prepared.avatars[key] = words; prepared.count++;
    });
    Object.keys(payload.icons || {}).forEach(function(key) {
      var entry = payload.icons[key], icon = state.icons.byKey[key];
      if (!icon || !entry || entry.pack !== icon.pack || entry.itemId !== icon.itemId) {
        throw new ArtError('icon Project record ' + key + ' does not match a vanilla icon');
      }
      var words = wordArrayFromBase64(entry.pixelsRgba5551BeBase64,
        C.ICON_PIXELS, 'icon Project record ' + key);
      validateIconWords(state, state.icons.packs[icon.pack], words, icon.name);
      prepared.icons[key] = words; prepared.count++;
    });
    // Validate each complete icon pack after overlaying the prepared records.
    Object.keys(state.icons.packs).forEach(function(slug) {
      var pack = state.icons.packs[slug], colors = new Set();
      pack.icons.forEach(function(icon) {
        var words = prepared.icons[icon.key] || currentWords(state, 'icon', icon.key);
        words.forEach(function(word) { if (word & 1) colors.add(word); });
      });
      if (colors.size > 255) throw new ArtError(pack.spec.label +
        ' Project records require ' + colors.size + ' opaque colors; maximum is 255');
    });
    return prepared;
  }

  function applyPreparedProjectPayload(rom, prepared) {
    var state = rom.art, applied = 0;
    Object.keys(prepared.avatars).forEach(function(key) {
      if (setEditWords(state, 'avatar', key, prepared.avatars[key])) applied++;
    });
    Object.keys(prepared.icons).forEach(function(key) {
      if (setEditWords(state, 'icon', key, prepared.icons[key])) applied++;
    });
    return applied;
  }

  function buildIndependentAvatar(state, appearance, words) {
    validateAvatarWords(state, words, appearance.className + ' ' + appearance.label);
    var route = appearance.routes[0];
    var source = state.avatar.bundleCache[route.resourceKey];
    var sourcePalette = source.palettes[route.paletteOrdinal];
    var required = [];
    var requiredSet = new Set();
    words.forEach(function(word) { if (!requiredSet.has(word)) { requiredSet.add(word); required.push(word); } });
    var assigned = new Map(), used = new Set();
    required.forEach(function(word) {
      for (var index = 0; index < sourcePalette.length; index++) {
        if (!used.has(index) && sourcePalette[index] === word) {
          assigned.set(word, index); used.add(index); break;
        }
      }
    });
    var palette = sourcePalette.slice();
    var free = [];
    for (var i = 0; i < 80; i++) if (!used.has(i)) free.push(i);
    required.forEach(function(word) {
      if (assigned.has(word)) return;
      var index = free.shift(); assigned.set(word, index); palette[index] = word;
    });
    var indices = new Uint8Array(words.length);
    for (var p = 0; p < words.length; p++) indices[p] = assigned.get(words[p]);
    if (!equalWords(renderIndexedWords(indices, palette), words)) {
      exportFailure(appearance.className + ' ' + appearance.label,
        'CI8 round trip', 'pixel mismatch', 'exact match', 'reset the avatar and retry the edit');
    }
    var decoded = concatBytes([indices, bytesFromWords(palette)]);
    return { decoded: decoded, stored: bootLzCompress(decoded), colorCount: required.length };
  }

  function buildIconPack(state, pack) {
    var wordsByIcon = {}, required = [], requiredSet = new Set();
    pack.icons.forEach(function(icon) {
      var words = currentWords(state, 'icon', icon.key);
      validateIconWords(state, pack, words, icon.name);
      wordsByIcon[icon.itemId] = words;
      words.forEach(function(word) {
        if ((word & 1) && !requiredSet.has(word)) { requiredSet.add(word); required.push(word); }
      });
    });
    if (required.length > 255) exportFailure(pack.spec.label, 'shared palette capacity',
      required.length + ' opaque colors', 'at most 255', 'reduce colors used across this icon pack');
    var palette = pack.palette.slice(), assigned = new Map(), used = new Set([pack.spec.transparentIndex]);
    required.forEach(function(word) {
      for (var index = 0; index < 256; index++) {
        if (!used.has(index) && palette[index] === word && (palette[index] & 1)) {
          assigned.set(word, index); used.add(index); break;
        }
      }
    });
    var free = [];
    for (var i = 0; i < 256; i++) if (!used.has(i)) free.push(i);
    required.forEach(function(word) {
      if (assigned.has(word)) return;
      var index = free.shift(); assigned.set(word, index); palette[index] = word; used.add(index);
    });
    palette[pack.spec.transparentIndex] = pack.transparentWord;
    var planes = [];
    pack.icons.forEach(function(icon) {
      var words = wordsByIcon[icon.itemId], plane = new Uint8Array(256);
      for (var pixel = 0; pixel < 256; pixel++) {
        plane[pixel] = (words[pixel] & 1) ? assigned.get(words[pixel]) : pack.spec.transparentIndex;
      }
      if (!equalWords(renderIndexedWords(plane, palette), words)) {
        exportFailure(icon.name, 'CI8 round trip', 'pixel mismatch', 'exact match',
          'reset the icon and retry the edit');
      }
      planes.push(plane);
    });
    var decoded = concatBytes([bytesFromWords(palette)].concat(planes));
    if (decoded.length !== pack.spec.decodedLength) {
      exportFailure(pack.spec.label, 'decoded pack size', decoded.length,
        pack.spec.decodedLength, 'reload the vanilla ROM and reapply the Project');
    }
    return { decoded: decoded, stored: bootLzCompress(decoded), colorCount: required.length };
  }

  function resourceKeyForEntry(entry) {
    var key = entry - C.RESOURCE_BASE;
    if ((entry & 1) || key < 0 || key >= 0x10000000 || (key & 1)) {
      throw new ArtError('relocated resource entry is outside aligned loader range: ' + hex(entry));
    }
    return key;
  }

  function planAllocations(resources) {
    var cursor = C.ARENA_START, allocations = [];
    resources.forEach(function(resource) {
      cursor = (cursor + 1) & ~1;
      var pad = resource.stored.length & 1;
      var row = {
        name: resource.name, entry: cursor, key: resourceKeyForEntry(cursor),
        stored: resource.stored, pad: pad,
        end: cursor + 4 + resource.stored.length + pad
      };
      if (row.end > C.ARENA_END) exportFailure('native art relocation arena',
        'capacity', hex(row.end), '<= ' + hex(C.ARENA_END),
        'reduce detached avatars or icon-pack size');
      allocations.push(row); cursor = row.end;
    });
    return allocations;
  }

  function assertArenaFill(bytes) {
    for (var offset = C.ARENA_START; offset < C.ARENA_END; offset++) {
      if (bytes[offset] !== C.ARENA_FILL) exportFailure('native art relocation arena',
        'retail fill ownership', hex(bytes[offset], 2) + ' at z64 ' + hex(offset),
        '0xFF', 'use a ROM with an unoccupied native-art relocation arena');
    }
  }

  function writeEnvelope(bytes, allocation, ranges) {
    writeU32(bytes, allocation.entry, allocation.stored.length);
    bytes.set(allocation.stored, allocation.entry + 4);
    if (allocation.pad) bytes[allocation.end - 1] = 0;
    ranges.push([allocation.entry, allocation.end]);
  }

  function mergeRanges(ranges) {
    var ordered = ranges.filter(function(row) { return row[0] < row[1]; })
      .sort(function(a, b) { return a[0] - b[0]; });
    var out = [];
    ordered.forEach(function(row) {
      if (!out.length || row[0] > out[out.length - 1][1]) out.push(row.slice());
      else out[out.length - 1][1] = Math.max(out[out.length - 1][1], row[1]);
    });
    return out;
  }

  function restorePreviousArt(state, bytes) {
    if (!state.lastExport) return;
    state.lastExport.ownedRanges.forEach(function(range) {
      copyRange(bytes, state.retailZ64, range[0], range[1]);
    });
  }

  function makeExportPlan(rom, cleanBase) {
    var state = rom.art;
    var avatarKeys = Object.keys(state.avatar.edits).sort();
    var changedPackSlugs = Object.keys(state.icons.packs).filter(function(slug) {
      return state.icons.packs[slug].icons.some(function(icon) { return !!state.icons.edits[icon.key]; });
    });
    var avatars = avatarKeys.map(function(key, ordinal) {
      var appearance = state.avatar.byKey[key];
      var built = buildIndependentAvatar(state, appearance, state.avatar.edits[key].words);
      return { name: 'avatar-' + ordinal, key: key, appearance: appearance, built: built };
    });
    var iconPacks = changedPackSlugs.map(function(slug) {
      var pack = state.icons.packs[slug], built = buildIconPack(state, pack);
      return {
        name: 'icon-' + slug, slug: slug, pack: pack, built: built,
        placement: built.stored.length <= pack.spec.capacity ? 'in-place' : 'relocated'
      };
    });
    var relocatedResources = avatars.map(function(row) {
      return { name: row.name, stored: row.built.stored };
    }).concat(iconPacks.filter(function(row) { return row.placement === 'relocated'; })
      .map(function(row) { return { name: row.name, stored: row.built.stored }; }));
    var descriptorPlaceholder = null;
    if (avatars.length) {
      descriptorPlaceholder = {
        name: 'avatar-descriptor',
        stored: new Uint8Array((state.avatar.descriptorSlots.length + avatars.length) * 4)
      };
      relocatedResources.push(descriptorPlaceholder);
    }
    if (relocatedResources.length) assertArenaFill(cleanBase);
    var allocations = planAllocations(relocatedResources), allocationByName = {};
    allocations.forEach(function(row) { allocationByName[row.name] = row; });
    var descriptorSlots = state.avatar.descriptorSlots.slice();
    avatars.forEach(function(row) {
      row.allocation = allocationByName[row.name];
      row.memberIndex = descriptorSlots.length;
      if (row.memberIndex >= 0x2000) exportFailure(row.appearance.className,
        'descriptor member index', row.memberIndex, '< 8192', 'reduce detached avatars');
      descriptorSlots.push(row.allocation.key);
    });
    var descriptorAllocation = null;
    if (avatars.length) {
      descriptorAllocation = allocationByName['avatar-descriptor'];
      var descriptorBytes = new Uint8Array(descriptorSlots.length * 4);
      descriptorSlots.forEach(function(key, index) { writeU32(descriptorBytes, index * 4, key); });
      descriptorAllocation.stored = descriptorBytes;
    }
    iconPacks.forEach(function(row) {
      if (row.placement === 'relocated') row.allocation = allocationByName[row.name];
    });
    return {
      avatars: avatars, iconPacks: iconPacks, allocations: allocations,
      descriptorSlots: descriptorSlots, descriptorAllocation: descriptorAllocation,
      cleanBase: cleanBase, log: [], ownedRanges: [], crc: false
    };
  }

  function applyPlanToBytes(rom, plan, bytes) {
    var state = rom.art;
    var ranges = state.lastExport
      ? state.lastExport.ownedRanges.map(function(range) { return range.slice(); })
      : [];
    var log = [];
    restorePreviousArt(state, bytes);
    if (state.lastExport) log.push('Restored the prior session art allocation to the loaded source baseline before rebuilding.');
    if (plan.allocations.length) assertArenaFill(bytes);
    plan.allocations.forEach(function(allocation) { writeEnvelope(bytes, allocation, ranges); });

    plan.avatars.forEach(function(row) {
      var token = row.memberIndex << 3;
      row.appearance.routes.forEach(function(route) {
        var observed = readU16(bytes, route.routeOffset);
        if (observed !== route.token) exportFailure(row.appearance.className + ' ' + row.appearance.label,
          'class route preimage', hex(observed, 4), hex(route.token, 4),
          'reload the original source ROM and reapply the Project');
        writeU16(bytes, route.routeOffset, token);
        ranges.push([route.routeOffset, route.routeOffset + 2]);
        log.push(row.appearance.className + ' ' + row.appearance.label +
          ' route selector ' + route.selectorIndex + ': ' + hex(route.token, 4) + ' -> ' + hex(token, 4));
      });
    });
    if (plan.descriptorAllocation) {
      patchSplitKey(bytes, C.AVATAR_DESCRIPTOR_SITES, C.AVATAR_DESCRIPTOR_KEY,
        plan.descriptorAllocation.key, ranges, log, 'avatar descriptor');
    }

    plan.iconPacks.forEach(function(row) {
      var spec = row.pack.spec;
      if (row.placement === 'in-place') {
        writeU32(bytes, spec.sizeWord, row.built.stored.length);
        bytes.set(row.built.stored, spec.sizeWord + 4);
        bytes.fill(0, spec.sizeWord + 4 + row.built.stored.length,
          spec.sizeWord + 4 + spec.capacity);
        ranges.push([spec.sizeWord, spec.sizeWord + 4 + spec.capacity]);
        log.push(spec.label + ' icon pack: in-place ' + row.built.stored.length +
          '/' + spec.capacity + ' bytes');
      } else {
        patchSplitKey(bytes, spec.sites, spec.resourceKey, row.allocation.key,
          ranges, log, spec.label + ' icon pack');
        log.push(spec.label + ' icon pack: relocated to ' + hex(row.allocation.entry) +
          ' (' + row.built.stored.length + ' bytes)');
      }
    });
    plan.currentCrcAffected = plan.avatars.length > 0 || plan.iconPacks.some(function(row) {
      return row.placement === 'relocated';
    });
    plan.crc = plan.currentCrcAffected || !!(state.lastExport && state.lastExport.crcAffected);
    if (plan.crc) ranges.push([0x10, 0x18]);
    plan.ownedRanges = mergeRanges(ranges);
    var currentRanges = plan.allocations.map(function(allocation) {
      return [allocation.entry, allocation.end];
    });
    plan.avatars.forEach(function(row) {
      row.appearance.routes.forEach(function(route) {
        currentRanges.push([route.routeOffset, route.routeOffset + 2]);
      });
    });
    if (plan.descriptorAllocation) C.AVATAR_DESCRIPTOR_SITES.forEach(function(site) {
      currentRanges.push([site.lui, site.lui + 4], [site.ori, site.ori + 4]);
    });
    plan.iconPacks.forEach(function(row) {
      if (row.placement === 'in-place') {
        currentRanges.push([row.pack.spec.sizeWord,
          row.pack.spec.sizeWord + 4 + row.pack.spec.capacity]);
      } else {
        row.pack.spec.sites.forEach(function(site) {
          currentRanges.push([site.lui, site.lui + 4], [site.ori, site.ori + 4]);
        });
      }
    });
    if (plan.currentCrcAffected) currentRanges.push([0x10, 0x18]);
    plan.currentOwnedRanges = mergeRanges(currentRanges);
    plan.log = log;
    return plan;
  }

  function verifyAppliedPlan(rom, plan, bytes) {
    var state = rom.art;
    if (plan.descriptorAllocation) {
      var key = resolveSplitKey(bytes, C.AVATAR_DESCRIPTOR_SITES, 'avatar descriptor');
      if (key !== plan.descriptorAllocation.key) exportFailure('avatar descriptor',
        'owner key readback', hex(key), hex(plan.descriptorAllocation.key), 'report this editor defect');
      var descriptor = readResource(bytes, key);
      if (!equalBytes(descriptor.stored, plan.descriptorAllocation.stored)) exportFailure(
        'avatar descriptor', 'graph readback', 'mismatch', 'exact match', 'report this editor defect');
    }
    plan.avatars.forEach(function(row) {
      var decoded = readCompressedResource(bytes, row.allocation.key).decoded;
      if (!equalBytes(decoded, row.built.decoded)) exportFailure(row.appearance.className,
        'compressed round trip', 'mismatch', 'exact match', 'report this editor defect');
      row.appearance.routes.forEach(function(route) {
        if (readU16(bytes, route.routeOffset) !== row.memberIndex << 3) exportFailure(
          row.appearance.className, 'route readback', 'mismatch', 'exact token', 'report this editor defect');
      });
    });
    plan.iconPacks.forEach(function(row) {
      var key = row.placement === 'relocated' ? row.allocation.key : row.pack.spec.resourceKey;
      if (row.placement === 'relocated') {
        var ownerKey = resolveSplitKey(bytes, row.pack.spec.sites, row.pack.spec.label);
        if (ownerKey !== key) exportFailure(row.pack.spec.label, 'owner key readback',
          hex(ownerKey), hex(key), 'report this editor defect');
      }
      var decoded = readCompressedResource(bytes, key).decoded;
      if (!equalBytes(decoded, row.built.decoded)) exportFailure(row.pack.spec.label,
        'complete-pack round trip', 'mismatch', 'exact match', 'report this editor defect');
    });
  }

  function prepareExport(rom, candidateRom) {
    var state = rom && rom.art;
    if (!state || !state.supported || !hasPendingExport(state)) return null;
    var cleanBase = candidateRom.z64.slice();
    restorePreviousArt(state, cleanBase);
    var plan = makeExportPlan(rom, cleanBase);
    var simulated = cleanBase.slice();
    applyPlanToBytes(rom, plan, simulated);
    verifyAppliedPlan(rom, plan, simulated);
    plan.simulated = simulated;
    return plan;
  }

  function applyExport(rom, candidateRom, plan) {
    if (!plan) return null;
    applyPlanToBytes(rom, plan, candidateRom.z64);
    verifyAppliedPlan(rom, plan, candidateRom.z64);
    var iconCounts = { equipment: 0, 'special-item': 0 };
    Object.keys(rom.art.icons.edits).forEach(function(key) {
      iconCounts[rom.art.icons.byKey[key].pack]++;
    });
    var relocatedCount = plan.allocations.length;
    var arenaEnd = plan.allocations.length ? plan.allocations[plan.allocations.length - 1].end : C.ARENA_START;
    return {
      crc: plan.crc,
      currentCrcAffected: plan.currentCrcAffected,
      detachedAvatarCount: plan.avatars.length,
      editedIconCounts: iconCounts,
      relocatedResourceCount: relocatedCount,
      arenaStart: C.ARENA_START, arenaEnd: arenaEnd,
      ownedRanges: plan.currentOwnedRanges.map(function(row) { return row.slice(); }),
      log: plan.log.slice(),
      summary: plan.avatars.length + ' detached avatar' + (plan.avatars.length === 1 ? '' : 's') +
        ', ' + iconCounts.equipment + ' equipment icon' + (iconCounts.equipment === 1 ? '' : 's') +
        ', ' + iconCounts['special-item'] + ' special-item icon' + (iconCounts['special-item'] === 1 ? '' : 's') +
        ', ' + relocatedCount + ' relocated resource' + (relocatedCount === 1 ? '' : 's') +
        ', ROM span ' + hex(C.ARENA_START) + '..' + hex(arenaEnd)
    };
  }

  function finalizeExportSummary(result, bytes) {
    if (!result || typeof result.summary !== 'string') {
      throw new ArtError('final Art export summary requires an applied export result');
    }
    var baseSummary = result.baseSummary || result.summary;
    var crc1 = readU32(bytes, 0x10);
    var crc2 = readU32(bytes, 0x14);
    result.baseSummary = baseSummary;
    result.crc1 = crc1;
    result.crc2 = crc2;
    result.summary = baseSummary + ', final CRC1 ' + hex(crc1) + ', CRC2 ' + hex(crc2);
    return result.summary;
  }

  function adoptExport(rom, result) {
    if (!rom || !rom.art || !result) return;
    rom.art.lastExport = editCount(rom.art) || result.ownedRanges.length
      ? {
          ownedRanges: result.ownedRanges.map(function(row) { return row.slice(); }),
          crcAffected: !!result.currentCrcAffected
        }
      : null;
    if (!editCount(rom.art)) rom.art.lastExport = null;
  }

  function patchOwner(plan) {
    if (!plan) return null;
    var ownerRanges = [];
    plan.ownedRanges.forEach(function(range) {
      // The shared export ledger owns CRC1/CRC2. Art records the range only so
      // a later session export can restore its prior after-image before rebuild.
      if (range[0] < 0x10) ownerRanges.push([range[0], Math.min(range[1], 0x10)]);
      if (range[1] > 0x18) ownerRanges.push([Math.max(range[0], 0x18), range[1]]);
    });
    return {
      id: 'native-art', name: 'Art and Animation', category: 'art',
      regions: ownerRanges.map(function(range, index) {
        return {
          kind: 'rom', start: range[0], size: range[1] - range[0],
          label: 'native art write ' + (index + 1)
        };
      })
    };
  }

  function resetAll(state) {
    if (!state || !state.supported || (!editCount(state) && !blockedCount(state))) return false;
    state.bulkUndo = collectStateEdits(state);
    state.avatar.edits = {}; state.icons.edits = {};
    state.blocked = { avatars: {}, icons: {} };
    return true;
  }

  function collectStateEdits(state) {
    var snapshot = { avatars: {}, icons: {} };
    Object.keys(state.avatar.edits).forEach(function(key) {
      snapshot.avatars[key] = state.avatar.edits[key].words.slice();
    });
    Object.keys(state.icons.edits).forEach(function(key) {
      snapshot.icons[key] = state.icons.edits[key].words.slice();
    });
    return snapshot;
  }

  function undoResetAll(state) {
    if (!state || !state.bulkUndo) return false;
    var snapshot = state.bulkUndo; state.bulkUndo = null;
    Object.keys(snapshot.avatars).forEach(function(key) {
      state.avatar.edits[key] = { words: snapshot.avatars[key].slice() };
    });
    Object.keys(snapshot.icons).forEach(function(key) {
      state.icons.edits[key] = { words: snapshot.icons[key].slice() };
    });
    Object.keys(state.icons.packs).forEach(function(slug) {
      refreshIconPackBlocked(state, slug);
    });
    return true;
  }

  OB64.art = {
    constants: C,
    ArtError: ArtError,
    initialize: initialize,
    rgba5551: rgba5551,
    rgba5551Word: rgba5551Word,
    nativeChannel5: nativeChannel5,
    avatarWordsFromRgbaBytes: avatarWordsFromRgbaBytes,
    avatarCropRect: avatarCropRect,
    wuQuantizeAvatarWords: wuQuantizeAvatarWords,
    prepareAvatarImport: prepareAvatarImport,
    colorCss: colorCss,
    hex: hex,
    currentWords: currentWords,
    setEditWords: setEditWords,
    setBlocked: setBlocked,
    blockedReason: blockedReason,
    refreshIconPackBlocked: refreshIconPackBlocked,
    undo: undo,
    redo: redo,
    historyFor: historyFor,
    editCount: editCount,
    blockedCount: blockedCount,
    hasPendingExport: hasPendingExport,
    distinctCount: distinctCount,
    collectProjectPayload: collectProjectPayload,
    prepareProjectPayload: prepareProjectPayload,
    applyPreparedProjectPayload: applyPreparedProjectPayload,
    prepareExport: prepareExport,
    applyExport: applyExport,
    finalizeExportSummary: finalizeExportSummary,
    adoptExport: adoptExport,
    patchOwner: patchOwner,
    resetAll: resetAll,
    undoResetAll: undoResetAll,
    wordArrayToBase64: wordArrayToBase64,
    wordArrayFromBase64: wordArrayFromBase64,
    bootLzDecode: bootLzDecode,
    bootLzCompress: bootLzCompress,
    readCompressedResource: readCompressedResource,
    resolveSplitKey: resolveSplitKey,
    mergeRanges: mergeRanges
  };
})();
