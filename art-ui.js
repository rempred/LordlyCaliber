// OB64 Mod Editor - Art and Animation tab UI

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var A = OB64.art;
  var C = A.constants;

  function element(tag, className, text) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function button(text, className, handler) {
    var node = element('button', className || '', text);
    node.type = 'button';
    if (handler) node.addEventListener('click', handler);
    return node;
  }

  function ensureUi(state) {
    if (state.ui) {
      if (!state.ui.browserScroll) state.ui.browserScroll = {};
      state.ui.transparentPreviewBackground = previewBackgroundMode(state.ui);
      if (!state.ui.animationKey && state.animations && state.animations.specs.length) {
        state.ui.animationKey = state.animations.byKey['fighter-slash']
          ? state.animations.byKey['fighter-slash'].key
          : state.animations.specs[0].key;
      }
      if (!Number.isInteger(state.ui.animationFrame)) state.ui.animationFrame = 0;
      if (!Number.isInteger(state.ui.animationLayer)) state.ui.animationLayer = 0;
      if (!Number.isInteger(state.ui.animationPaletteIndex)) state.ui.animationPaletteIndex = 0;
      if (!Number.isInteger(state.ui.animationIntensity)) state.ui.animationIntensity = 15;
      if (!Number.isInteger(state.ui.animationWeaponChild)) state.ui.animationWeaponChild = 0;
      if (!state.ui.animationWeaponChildren) state.ui.animationWeaponChildren = {};
      return state.ui;
    }
    var firstAvatar = state.avatar && state.avatar.appearances[0];
    var firstIconPack = state.icons && state.icons.packs.equipment;
    var avatarColor = firstAvatar ? firstAvatar.originalWords[0] : A.rgba5551Word(31, 31, 31);
    var iconColor = firstIconPack ? firstIconPack.colorLibrary[0] : A.rgba5551Word(31, 31, 31);
    state.ui = {
      subtab: state.selectedTab || 'avatars',
      avatarKey: state.avatar && state.avatar.appearances.length
        ? state.avatar.appearances[0].key : null,
      iconKey: state.icons && state.icons.icons.length ? state.icons.icons[0].key : null,
      iconPack: 'equipment', avatarSearch: '', iconSearch: '',
      avatarModifiedOnly: false, iconModifiedOnly: false,
      tool: 'pencil', selectedAvatarColor: avatarColor,
      selectedIconColor: iconColor,
      avatarWheelValue: Math.max((avatarColor >>> 11) & 31,
        (avatarColor >>> 6) & 31, (avatarColor >>> 1) & 31),
      iconWheelValue: 31,
      selection: null, clipboard: null,
      animationKey: state.animations && state.animations.specs.length
        ? state.animations.specs[0].key : null,
      animationFrame: 0, animationLayer: 0,
      animationPaletteIndex: 0, animationIntensity: 15,
      animationWeaponChild: 0,
      animationWeaponChildren: {},
      transparentPreviewBackground: 'checkerboard',
      browserScroll: {}
    };
    if (state.animations && state.animations.byKey['fighter-slash']) {
      state.ui.animationKey = state.animations.byKey['fighter-slash'].key;
    }
    return state.ui;
  }

  function openAnimationRoute(state, request) {
    if (!state || !state.supported || !state.animations ||
        !state.animations.supported || !OB64.animationUI) return false;
    var ui = ensureUi(state);
    ui.subtab = 'animations';
    state.selectedTab = 'animations';
    OB64.animationUI.requestAnimationRoute(ui, request);
    return true;
  }

  function scrollContainers(panel, includeViewport) {
    if (!panel) return [];
    var containers = [];
    if (panel.querySelectorAll) {
      containers = Array.prototype.slice.call(
        panel.querySelectorAll('[data-art-scroll-key]'));
    } else if (panel.querySelector) {
      var one = panel.querySelector('[data-art-scroll-key]');
      if (one) containers.push(one);
    }
    if (includeViewport && panel.closest) {
      var viewport = panel.closest('.content');
      if (viewport && containers.indexOf(viewport) < 0) {
        if (!viewport.getAttribute('data-art-scroll-key')) {
          viewport.setAttribute('data-art-scroll-key', 'art:viewport');
        }
        containers.unshift(viewport);
      }
    }
    return containers;
  }

  function captureBrowserScroll(ui, panel, includeViewport) {
    if (!ui) return;
    if (!ui.browserScroll) ui.browserScroll = {};
    scrollContainers(panel, includeViewport).forEach(function(list) {
      var key = list.getAttribute('data-art-scroll-key');
      if (!key) return;
      ui.browserScroll[key] = {
        top: Number(list.scrollTop) || 0,
        left: Number(list.scrollLeft) || 0
      };
    });
  }

  function restoreBrowserScroll(ui, panel, includeViewport) {
    if (!ui || !ui.browserScroll) return;
    scrollContainers(panel, includeViewport).forEach(function(list) {
      var key = list.getAttribute('data-art-scroll-key');
      var saved = key && ui.browserScroll[key];
      if (!saved) return;
      list.scrollTop = saved.top;
      list.scrollLeft = saved.left;
    });
  }

  function previewBackgroundMode(ui) {
    return ui && ui.transparentPreviewBackground === 'white'
      ? 'white' : 'checkerboard';
  }

  function togglePreviewBackground(ui) {
    ui.transparentPreviewBackground = previewBackgroundMode(ui) === 'white'
      ? 'checkerboard' : 'white';
    return ui.transparentPreviewBackground;
  }

  function previewBackgroundButton(ui, rerender) {
    var whitePreview = previewBackgroundMode(ui) === 'white';
    var previewToggle = button('Transparent Preview: ' +
      (whitePreview ? 'White' : 'Checkerboard'), 'btn-secondary', function() {
      togglePreviewBackground(ui);
      rerender();
    });
    previewToggle.setAttribute('aria-pressed', whitePreview ? 'true' : 'false');
    previewToggle.setAttribute('title',
      'Changes transparent pixels in editor previews only. Exports and Project data do not change.');
    return previewToggle;
  }

  function canvasBackgroundMode(mode) {
    if (mode === 'white' || mode === 'transparent') return mode;
    return 'checkerboard';
  }

  function rgbaPixelsForWords(words) {
    var pixels = new Uint8ClampedArray(words.length * 4);
    for (var index = 0; index < words.length; index++) {
      pixels.set(A.rgba5551(words[index]), index * 4);
    }
    return pixels;
  }

  function drawWords(canvas, words, width, height, scale, selection, backgroundMode) {
    canvas.width = width * scale;
    canvas.height = height * scale;
    var context = canvas.getContext('2d');
    context.imageSmoothingEnabled = false;
    context.clearRect(0, 0, canvas.width, canvas.height);
    backgroundMode = canvasBackgroundMode(backgroundMode);
    if (backgroundMode === 'transparent' && scale === 1 && !selection) {
      var imageData = context.createImageData(width, height);
      imageData.data.set(rgbaPixelsForWords(words));
      context.putImageData(imageData, 0, 0);
      return;
    }
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        var word = words[y * width + x];
        if (word & 1) {
          context.fillStyle = A.colorCss(word);
          context.fillRect(x * scale, y * scale, scale, scale);
        } else if (backgroundMode === 'white') {
          context.fillStyle = '#ffffff';
          context.fillRect(x * scale, y * scale, scale, scale);
        } else if (backgroundMode === 'checkerboard') {
          var checker = Math.max(1, Math.floor(scale / 2));
          context.fillStyle = '#e9e4d0';
          context.fillRect(x * scale, y * scale, scale, scale);
          context.fillStyle = '#9f9a89';
          context.fillRect(x * scale, y * scale, checker, checker);
          context.fillRect(x * scale + checker, y * scale + checker,
            scale - checker, scale - checker);
        }
      }
    }
    if (selection) {
      context.save();
      context.strokeStyle = '#fff'; context.lineWidth = 2;
      context.setLineDash([5, 3]);
      context.strokeRect(selection.x * scale + 1, selection.y * scale + 1,
        selection.width * scale - 2, selection.height * scale - 2);
      context.strokeStyle = '#15171b'; context.lineWidth = 1;
      context.setLineDash([5, 3]); context.lineDashOffset = 4;
      context.strokeRect(selection.x * scale + 1.5, selection.y * scale + 1.5,
        selection.width * scale - 3, selection.height * scale - 3);
      context.restore();
    }
  }

  function wordCanvas(words, width, height, scale, className, selection,
      backgroundMode) {
    var canvas = element('canvas', className || 'art-pixel-canvas');
    drawWords(canvas, words, width, height, scale, selection, backgroundMode);
    return canvas;
  }

  function nativePngCanvas(words, width, height) {
    return wordCanvas(words, width, height, 1, '', null, 'transparent');
  }

  function nativePngDownload(words, width, height, filename) {
    var canvas = nativePngCanvas(words, width, height);
    canvas.toBlob(function(blob) {
      if (!blob) return;
      var url = URL.createObjectURL(blob);
      var anchor = document.createElement('a');
      anchor.href = url; anchor.download = filename;
      document.body.appendChild(anchor); anchor.click(); anchor.remove();
      setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
    }, 'image/png');
  }

  function classIdLabel(classId) { return A.hex(classId, 2); }

  function routeMetadata(appearance) {
    return 'Class ' + classIdLabel(appearance.classId) + ' · ' +
      appearance.routes.map(function(route) {
        return 'route ' + (route.selectorIndex ? 'B' : 'A') +
          ' / member ' + route.memberIndex + ' / palette ' + route.paletteOrdinal;
      }).join(' · ');
  }

  function makeBadge(text, kind) { return element('span', 'art-badge art-badge-' + kind, text); }

  function blockedBadge(state, kind, key) {
    var reason = A.blockedReason(state, kind, key);
    if (!reason) return null;
    var badge = makeBadge('Blocked', 'blocked');
    badge.setAttribute('title', reason);
    return badge;
  }

  function notify(options, message) {
    if (options && options.onStatus) options.onStatus(message);
  }

  function changed(options) {
    if (options && options.onChange) options.onChange();
  }

  var COLOR_WHEEL_SIZE = 320;

  function hsvForWord(word) {
    var red = ((word >>> 11) & 31) / 31;
    var green = ((word >>> 6) & 31) / 31;
    var blue = ((word >>> 1) & 31) / 31;
    var max = Math.max(red, green, blue), min = Math.min(red, green, blue);
    var delta = max - min, hue = 0;
    if (delta) {
      if (max === red) hue = ((green - blue) / delta + (green < blue ? 6 : 0)) / 6;
      else if (max === green) hue = ((blue - red) / delta + 2) / 6;
      else hue = ((red - green) / delta + 4) / 6;
    }
    return { hue: hue, saturation: max ? delta / max : 0, value5: Math.round(max * 31) };
  }

  function wordForHsv(hue, saturation, value5) {
    hue = ((hue % 1) + 1) % 1;
    saturation = Math.max(0, Math.min(1, saturation));
    value5 = Math.max(0, Math.min(31, Math.round(value5)));
    var value = value5 / 31, chroma = value * saturation;
    var section = hue * 6, x = chroma * (1 - Math.abs((section % 2) - 1));
    var red = 0, green = 0, blue = 0;
    switch (Math.floor(section) % 6) {
      case 0: red = chroma; green = x; break;
      case 1: red = x; green = chroma; break;
      case 2: green = chroma; blue = x; break;
      case 3: green = x; blue = chroma; break;
      case 4: red = x; blue = chroma; break;
      default: red = chroma; blue = x; break;
    }
    var match = value - chroma;
    return A.rgba5551Word(
      Math.round((red + match) * 31),
      Math.round((green + match) * 31),
      Math.round((blue + match) * 31), true
    );
  }

  function paletteLookup(pack) {
    if (pack.nearestPaletteLut) return pack.nearestPaletteLut;
    var colors = pack.colorLibrary.map(function(word) {
      return {
        word: word, red: (word >>> 11) & 31,
        green: (word >>> 6) & 31, blue: (word >>> 1) & 31
      };
    });
    var lookup = new Uint16Array(32768), slot = 0;
    for (var red = 0; red < 32; red++) {
      for (var green = 0; green < 32; green++) {
        for (var blue = 0; blue < 32; blue++, slot++) {
          var best = colors[0], bestDistance = Infinity;
          for (var index = 0; index < colors.length; index++) {
            var row = colors[index];
            var dr = red - row.red, dg = green - row.green, db = blue - row.blue;
            var distance = dr * dr + dg * dg + db * db;
            if (distance < bestDistance) {
              best = row; bestDistance = distance;
              if (!distance) break;
            }
          }
          lookup[slot] = best.word;
        }
      }
    }
    pack.nearestPaletteLut = lookup;
    return lookup;
  }

  function nearestPaletteWord(pack, word) {
    var index = (((word >>> 11) & 31) << 10) |
      (((word >>> 6) & 31) << 5) | ((word >>> 1) & 31);
    return paletteLookup(pack)[index];
  }

  function wheelPixels(state, kind, pack, value5) {
    if (!state.colorWheelCache) state.colorWheelCache = {};
    var key = kind + ':' + (pack ? pack.spec.slug : 'all') + ':' + value5;
    if (state.colorWheelCache[key]) return state.colorWheelCache[key];
    var size = COLOR_WHEEL_SIZE, center = (size - 1) / 2, radius = center - 2;
    var data = new Uint8ClampedArray(size * size * 4);
    for (var y = 0; y < size; y++) {
      for (var x = 0; x < size; x++) {
        var dx = (x - center) / radius, dy = (y - center) / radius;
        var saturation = Math.sqrt(dx * dx + dy * dy);
        if (saturation > 1) continue;
        var hue = (Math.atan2(dx, -dy) / (Math.PI * 2) + 1) % 1;
        var word = wordForHsv(hue, saturation, value5);
        if (kind === 'icon') word = nearestPaletteWord(pack, word);
        var offset = (y * size + x) * 4;
        data[offset] = Math.floor((((word >>> 11) & 31) * 255 + 15) / 31);
        data[offset + 1] = Math.floor((((word >>> 6) & 31) * 255 + 15) / 31);
        data[offset + 2] = Math.floor((((word >>> 1) & 31) * 255 + 15) / 31);
        data[offset + 3] = 255;
      }
    }
    state.colorWheelCache[key] = data;
    return data;
  }

  function colorWheel(state, kind, pack, ui, onSelect) {
    var wrap = element('div', 'art-color-wheel-wrap');
    var canvas = element('canvas', 'art-color-wheel');
    canvas.width = COLOR_WHEEL_SIZE; canvas.height = COLOR_WHEEL_SIZE;
    canvas.tabIndex = 0;
    canvas.setAttribute('data-art-focus-key', kind + '-color-wheel');
    canvas.setAttribute('aria-label', kind === 'avatar'
      ? 'Opaque 16-bit RGB555 avatar color wheel'
      : pack.spec.label + ' 256-entry palette color wheel');
    var valueKey = kind === 'avatar' ? 'avatarWheelValue' : 'iconWheelValue';
    var selected = kind === 'avatar' ? ui.selectedAvatarColor : ui.selectedIconColor;
    var selectedHsv = hsvForWord(selected);
    var value5 = Number.isInteger(ui[valueKey]) ? ui[valueKey] : selectedHsv.value5;
    var currentWord = selected;
    var context = canvas.getContext('2d');
    var image = context.createImageData(COLOR_WHEEL_SIZE, COLOR_WHEEL_SIZE);
    function loadWheelPixels() {
      image.data.set(wheelPixels(state, kind, pack, value5));
    }
    function drawMarker(word) {
      context.putImageData(image, 0, 0);
      if (!(word & 1)) return;
      var markerHsv = hsvForWord(word);
      var center = (COLOR_WHEEL_SIZE - 1) / 2, radius = center - 2;
      var angle = markerHsv.hue * Math.PI * 2;
      var markerX = center + Math.sin(angle) * markerHsv.saturation * radius;
      var markerY = center - Math.cos(angle) * markerHsv.saturation * radius;
      context.beginPath(); context.arc(markerX, markerY, 10, 0, Math.PI * 2);
      context.strokeStyle = '#111'; context.lineWidth = 5; context.stroke();
      context.beginPath(); context.arc(markerX, markerY, 10, 0, Math.PI * 2);
      context.strokeStyle = '#fff'; context.lineWidth = 2; context.stroke();
    }
    function updateLiveSelection(word) {
      canvas.setAttribute('aria-valuetext', A.hex(word, 4) + ' RGBA5551');
      var palette = wrap.parentElement;
      if (!palette) return;
      var swatch = palette.querySelector('[data-art-selected-swatch]');
      var label = palette.querySelector('[data-art-selected-label]');
      if (swatch) swatch.style.background = A.colorCss(word);
      if (label) label.textContent = 'Selected ' + A.hex(word, 4) + ' RGBA5551';
      if (kind === 'avatar') {
        var values = [(word >>> 11) & 31, (word >>> 6) & 31, (word >>> 1) & 31];
        palette.querySelectorAll('[data-art-rgb-index]').forEach(function(input) {
          input.value = String(values[Number(input.getAttribute('data-art-rgb-index'))]);
        });
      }
    }
    function selectFromPointer(event) {
      var rect = canvas.getBoundingClientRect();
      var x = (event.clientX - rect.left) * canvas.width / rect.width;
      var y = (event.clientY - rect.top) * canvas.height / rect.height;
      var center = (canvas.width - 1) / 2, radius = center - 2;
      var dx = (x - center) / radius, dy = (y - center) / radius;
      var saturation = Math.sqrt(dx * dx + dy * dy);
      if (saturation > 1) return false;
      var hue = (Math.atan2(dx, -dy) / (Math.PI * 2) + 1) % 1;
      var word = wordForHsv(hue, saturation, value5);
      if (kind === 'icon') word = nearestPaletteWord(pack, word);
      currentWord = word;
      drawMarker(word);
      updateLiveSelection(word);
      onSelect(word, false);
      return true;
    }
    loadWheelPixels();
    drawMarker(currentWord);
    updateLiveSelection(currentWord);

    var activePointer = null;
    canvas.addEventListener('pointerdown', function(event) {
      event.preventDefault();
      if (!selectFromPointer(event)) return;
      activePointer = event.pointerId;
      canvas.focus({ preventScroll: true });
      canvas.setPointerCapture(event.pointerId);
    });
    canvas.addEventListener('pointermove', function(event) {
      if (activePointer !== event.pointerId || !canvas.hasPointerCapture(event.pointerId)) return;
      event.preventDefault();
      selectFromPointer(event);
    });
    function finishPointer(event) {
      if (activePointer !== event.pointerId) return;
      if (canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
      activePointer = null;
      onSelect(currentWord, true);
    }
    canvas.addEventListener('pointerup', finishPointer);
    canvas.addEventListener('pointercancel', finishPointer);
    canvas.addEventListener('keydown', function(event) {
      if (event.key !== 'Enter' && event.key !== ' ') return;
      event.preventDefault();
      onSelect(currentWord, true);
    });
    wrap.appendChild(canvas);

    var brightness = element('label', 'art-wheel-brightness');
    var brightnessText = element('span', '', 'Brightness');
    brightness.appendChild(brightnessText);
    var slider = element('input');
    slider.type = 'range'; slider.min = '0'; slider.max = '31'; slider.step = '1';
    slider.value = String(value5);
    slider.setAttribute('aria-label', 'Brightness');
    slider.setAttribute('data-art-focus-key', kind + '-brightness');
    slider.setAttribute('title', 'Brightness uses the 32 exact levels available in RGB555.');
    slider.addEventListener('input', function() {
      value5 = Number(slider.value);
      ui[valueKey] = value5;
      var currentHsv = hsvForWord(currentWord);
      var word = wordForHsv(currentHsv.hue, currentHsv.saturation, value5);
      if (kind === 'icon') word = nearestPaletteWord(pack, word);
      currentWord = word;
      loadWheelPixels();
      drawMarker(word);
      updateLiveSelection(word);
      onSelect(word, false);
    });
    slider.addEventListener('change', function() { onSelect(currentWord, true); });
    brightness.appendChild(slider); wrap.appendChild(brightness);
    return wrap;
  }

  function hasPngSignature(bytes) {
    var signature = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
    return bytes.length >= signature.length && signature.every(function(value, index) {
      return bytes[index] === value;
    });
  }

  function hasJpegSignature(bytes) {
    return bytes.length >= 3 && bytes[0] === 0xFF &&
      bytes[1] === 0xD8 && bytes[2] === 0xFF;
  }

  async function decodeAvatarImageSource(file) {
    if (!file) throw new A.ArtError('No avatar image was selected');
    var signature = new Uint8Array(await file.slice(0, 12).arrayBuffer());
    var format = hasPngSignature(signature) ? 'PNG' :
      (hasJpegSignature(signature) ? 'JPEG' : '');
    if (!format) {
      throw new A.ArtError(file.name + ' is not a supported PNG or JPEG image');
    }
    var bitmap;
    try {
      bitmap = await window.createImageBitmap(file, {
        colorSpaceConversion: 'none', premultiplyAlpha: 'none'
      });
    } catch (firstError) {
      try { bitmap = await window.createImageBitmap(file); }
      catch (secondError) {
        throw new A.ArtError(file.name + ' could not be decoded as ' + format);
      }
    }
    try {
      if (!bitmap.width || !bitmap.height) throw new A.ArtError(file.name + ' has no pixels');
      var canvas = document.createElement('canvas');
      canvas.width = bitmap.width; canvas.height = bitmap.height;
      var context = canvas.getContext('2d', { willReadFrequently: true });
      context.imageSmoothingEnabled = false;
      context.clearRect(0, 0, canvas.width, canvas.height);
      context.drawImage(bitmap, 0, 0);
      var rgba = context.getImageData(0, 0, canvas.width, canvas.height).data;
      return {
        name: file.name,
        format: format,
        width: canvas.width,
        height: canvas.height,
        rgba: new Uint8ClampedArray(rgba)
      };
    } finally {
      if (bitmap.close) bitmap.close();
    }
  }

  async function decodeAvatarPngSource(file) {
    var source = await decodeAvatarImageSource(file);
    if (source.format !== 'PNG') {
      throw new A.ArtError(file.name + ' is not a PNG file');
    }
    return source;
  }

  async function decodeAvatarPngFile(file) {
    var source = await decodeAvatarPngSource(file);
    return A.avatarWordsFromRgbaBytes(
      source.rgba, source.width, source.height, source.name);
  }

  function showAvatarImportDialog(source, appearance, state, options, rerender) {
    var settings = {
      resizeMode: 'nearest', panX: 0.5, panY: 0.5,
      dither: false, backgroundWord: appearance.originalWords[0]
    };
    var currentResult = null, scheduledFrame = null;
    var overlay = element('div', 'error-modal-overlay art-import-overlay');
    var modal = element('div', 'error-modal art-import-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'art-import-title');
    overlay.appendChild(modal);

    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'Prepare Avatar Image');
    title.id = 'art-import-title';
    header.appendChild(title);
    var closeButton = button('\u00D7', 'error-modal-close', close);
    closeButton.setAttribute('aria-label', 'Cancel avatar image import');
    header.appendChild(closeButton); modal.appendChild(header);

    var body = element('div', 'error-modal-body art-import-body');
    var intro = element('p', 'art-import-intro', source.name + ' \u00B7 ' +
      source.format + ' \u00B7 ' + source.width + '\u00D7' + source.height +
      ' source pixels. Adjust the crop and conversion before applying it.');
    body.appendChild(intro);
    var layout = element('div', 'art-import-layout');
    var previewPanel = element('section', 'art-import-preview-panel');
    previewPanel.appendChild(element('h3', '', 'Converted 40\u00D748 Preview'));
    var previewHost = element('div', 'art-import-preview-host');
    previewPanel.appendChild(previewHost);
    var stats = element('p', 'art-import-stats', 'Preparing preview\u2026');
    stats.setAttribute('aria-live', 'polite');
    previewPanel.appendChild(stats); layout.appendChild(previewPanel);

    var controls = element('section', 'art-import-controls');
    var resizeLabel = element('label', 'art-import-control');
    resizeLabel.appendChild(element('span', '', 'Resize method'));
    var resizeSelect = element('select');
    [['nearest', 'Pixel Art \u2014 nearest-neighbor'],
      ['smooth', 'Smooth \u2014 area/bilinear']].forEach(function(row) {
      var option = element('option', '', row[1]); option.value = row[0];
      resizeSelect.appendChild(option);
    });
    resizeSelect.value = settings.resizeMode;
    resizeSelect.addEventListener('change', function() {
      settings.resizeMode = resizeSelect.value; schedulePreview();
    });
    resizeLabel.appendChild(resizeSelect); controls.appendChild(resizeLabel);

    var initialCrop = A.avatarCropRect(
      source.width, source.height, settings.panX, settings.panY);
    function cropSlider(labelText, key, enabled) {
      var label = element('label', 'art-import-control');
      label.appendChild(element('span', '', labelText));
      var input = element('input');
      input.type = 'range'; input.min = '0'; input.max = '100'; input.step = '1';
      input.value = '50'; input.disabled = !enabled;
      input.setAttribute('aria-label', labelText);
      input.addEventListener('input', function() {
        settings[key] = Number(input.value) / 100; schedulePreview();
      });
      label.appendChild(input);
      if (!enabled) label.classList.add('disabled');
      controls.appendChild(label);
    }
    cropSlider('Horizontal crop position', 'panX', initialCrop.horizontalPanAvailable);
    cropSlider('Vertical crop position', 'panY', initialCrop.verticalPanAvailable);

    var ditherLabel = element('label', 'art-import-checkbox');
    var dither = element('input'); dither.type = 'checkbox';
    dither.checked = settings.dither;
    dither.addEventListener('change', function() {
      settings.dither = dither.checked; schedulePreview();
    });
    ditherLabel.appendChild(dither);
    ditherLabel.appendChild(document.createTextNode(' Ordered dithering'));
    var ditherHelp = element('small', '',
      'Off by default. It can smooth gradients after an image exceeds 80 native colors.');
    var ditherWrap = element('div', 'art-import-dither');
    ditherWrap.appendChild(ditherLabel); ditherWrap.appendChild(ditherHelp);
    controls.appendChild(ditherWrap);

    var background = element('div', 'art-import-background');
    var backgroundSwatch = element('span', 'art-selected-swatch');
    backgroundSwatch.style.background = A.colorCss(settings.backgroundWord);
    background.appendChild(backgroundSwatch);
    var backgroundCopy = element('span', '', 'Transparency fills with the original avatar background ' +
      A.hex(settings.backgroundWord, 4) + '.');
    background.appendChild(backgroundCopy); controls.appendChild(background);
    layout.appendChild(controls); body.appendChild(layout); modal.appendChild(body);

    var footer = element('div', 'error-modal-footer art-import-footer');
    var cancelButton = button('Cancel', 'error-modal-ok', close);
    footer.appendChild(cancelButton);
    var applyButton = button('Apply Converted Avatar', 'error-modal-ok', function() {
      if (!currentResult) return;
      var didChange = A.setEditWords(
        state, 'avatar', appearance.key, currentResult.words);
      A.setBlocked(state, 'avatar', appearance.key, '');
      if (didChange) changed(options);
      var crop = currentResult.crop;
      notify(options, 'Avatar image import applied: file=' + source.name +
        '; format=' + source.format +
        '; source=' + source.width + 'x' + source.height +
        '; crop=' + crop.width.toFixed(2) + 'x' + crop.height.toFixed(2) +
        '@(' + crop.x.toFixed(2) + ',' + crop.y.toFixed(2) + ')' +
        '; resize=' + currentResult.resizeMode +
        '; RGB555 colors=' + currentResult.sourceNativeColorCount + '->' +
        currentResult.colorCount + '/80' +
        '; Wu quantized=' + currentResult.quantized +
        '; ordered dither=' + currentResult.dithered +
        '; alpha background=' + A.hex(currentResult.backgroundWord, 4) +
        '; source non-opaque pixels=' + currentResult.transparentSourcePixels +
        '; route detachment=' + (!!state.avatar.edits[appearance.key]) + '.');
      close(); rerender();
    });
    applyButton.disabled = true; footer.appendChild(applyButton); modal.appendChild(footer);

    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    function preview() {
      scheduledFrame = null;
      try {
        currentResult = A.prepareAvatarImport(
          source.rgba, source.width, source.height, settings);
        previewHost.innerHTML = '';
        previewHost.appendChild(wordCanvas(
          currentResult.words, C.AVATAR_WIDTH, C.AVATAR_HEIGHT, 6,
          'art-import-preview-canvas'));
        var crop = currentResult.crop;
        stats.textContent = 'Crop ' + crop.width.toFixed(1) + '\u00D7' +
          crop.height.toFixed(1) + ' at ' + crop.x.toFixed(1) + ', ' +
          crop.y.toFixed(1) + ' \u00B7 ' + currentResult.sourceNativeColorCount +
          ' native colors \u2192 ' + currentResult.colorCount + ' / 80' +
          (currentResult.quantized ? ' \u00B7 Wu quantized' : ' \u00B7 quantization not required') +
          (currentResult.dithered ? ' \u00B7 ordered dither applied' : '') +
          (currentResult.transparentSourcePixels ? ' \u00B7 ' +
            currentResult.transparentSourcePixels + ' source alpha pixels' : '');
        stats.classList.remove('blocked'); applyButton.disabled = false;
      } catch (error) {
        currentResult = null; previewHost.innerHTML = '';
        stats.textContent = 'Conversion blocked: ' + error.message;
        stats.classList.add('blocked'); applyButton.disabled = true;
      }
    }
    function schedulePreview() {
      stats.textContent = 'Preparing preview\u2026'; applyButton.disabled = true;
      if (scheduledFrame !== null) window.cancelAnimationFrame(scheduledFrame);
      scheduledFrame = window.requestAnimationFrame(preview);
    }
    function close() {
      if (scheduledFrame !== null) window.cancelAnimationFrame(scheduledFrame);
      scheduledFrame = null;
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escapeHandler);
    }
    var escapeHandler = function(event) { if (event.key === 'Escape') close(); };
    document.addEventListener('keydown', escapeHandler);
    document.body.appendChild(overlay);
    resizeSelect.focus(); schedulePreview();
  }

  function selectedColorBlock(state, kind, ui, rerender) {
    var wrap = element('div', 'art-selected-color');
    var word = kind === 'avatar' ? ui.selectedAvatarColor : ui.selectedIconColor;
    var swatch = element('span', 'art-selected-swatch');
    swatch.setAttribute('data-art-selected-swatch', '');
    swatch.style.background = A.colorCss(word);
    wrap.appendChild(swatch);
    var label = element('span', '', 'Selected ' + A.hex(word, 4) + ' RGBA5551');
    label.setAttribute('data-art-selected-label', '');
    wrap.appendChild(label);
    if (kind === 'avatar') {
      var values = [(word >>> 11) & 31, (word >>> 6) & 31, (word >>> 1) & 31];
      var exact = element('div', 'art-rgb555-controls');
      ['R', 'G', 'B'].forEach(function(channel, index) {
        var field = element('label', '', channel);
        var input = element('input');
        input.type = 'number'; input.min = '0'; input.max = '31'; input.step = '1';
        input.value = String(values[index]);
        input.setAttribute('aria-label', channel + ' five-bit channel');
        input.setAttribute('data-art-focus-key', 'avatar-' + channel.toLowerCase() + '-channel');
        input.setAttribute('data-art-rgb-index', String(index));
        input.addEventListener('input', function() {
          var value = Number(input.value);
          if (!Number.isInteger(value) || value < 0 || value > 31) {
            input.value = String(values[index]); return;
          }
          values[index] = value;
          ui.selectedAvatarColor = A.rgba5551Word(values[0], values[1], values[2], true);
          ui.avatarWheelValue = Math.max(values[0], values[1], values[2]);
          ui.tool = 'pencil'; rerender();
        });
        field.appendChild(input); exact.appendChild(field);
      });
      exact.setAttribute('title', 'Exact five-bit channels provide direct access to every opaque RGB555 color.');
      wrap.appendChild(exact);
    }
    if (kind === 'icon') {
      var transparent = button('Transparent', 'art-transparent-button', function() {
        var icon = state.icons.byKey[ui.iconKey];
        ui.selectedIconColor = state.icons.packs[icon.pack].transparentWord;
        ui.tool = 'pencil'; rerender();
      });
      if (previewBackgroundMode(ui) === 'white') {
        transparent.classList.add('white-preview');
      }
      transparent.setAttribute('title', 'Paint the pack\'s one exact transparent palette entry.');
      wrap.appendChild(transparent);
    }
    return wrap;
  }

  function topHeader(panel, rom, state, ui, options, rerender) {
    var header = element('div', 'art-heading');
    var copy = element('div');
    copy.appendChild(element('h2', '', 'Art and Animation'));
    copy.appendChild(element('p', '',
      'Edit class-card avatars, item icons, Army sprites, and the complete verified combat-sprite sequence corpus.'));
    header.appendChild(copy);
    var actions = element('div', 'art-heading-actions');
    var reset = button('Reset All Art', 'btn-secondary', function() {
      function execute() {
        if (!A.resetAll(state, rom)) return;
        changed(options); notify(options, 'All avatar, item icon, Army sprite, and combat sprite edits reset to the vanilla ROM. Use Undo Reset All to restore them this session.');
        rerender();
      }
      if (OB64.showConfirmModal) {
        OB64.showConfirmModal('Reset all art?',
          'This removes every avatar, item icon, Army sprite, and combat sprite Project record. The reset remains undoable during this editor session.',
          execute, 'Reset All Art');
      } else if (window.confirm('Reset every avatar, item icon, Army sprite, and combat sprite edit?')) execute();
    });
    reset.disabled = A.editCount(state) === 0 && A.blockedCount(state) === 0;
    actions.appendChild(reset);
    var undoReset = button('Undo Reset All', 'btn-secondary', function() {
      if (!A.undoResetAll(state, rom)) return;
      changed(options); notify(options, 'Restored all art edits removed by Reset All Art.'); rerender();
    });
    undoReset.disabled = !state.bulkUndo;
    actions.appendChild(undoReset);
    header.appendChild(actions);
    panel.appendChild(header);

    var tabs = element('div', 'art-subtabs');
    [['avatars', 'Avatars'], ['icons', 'Item Icons'], ['army', 'Army Sprites'],
      ['animations', 'Combat Animation']].forEach(function(row) {
      var tab = button(row[1], ui.subtab === row[0] ? 'active' : '', function() {
        ui.subtab = row[0]; state.selectedTab = row[0]; ui.selection = null; rerender();
      });
      var count = row[0] === 'avatars'
        ? Object.keys(state.avatar.edits).length
        : (row[0] === 'icons'
          ? Object.keys(state.icons.edits).length
          : (row[0] === 'army'
            ? (OB64.armySprites
              ? OB64.armySprites.editCount(state.armySprites) : 0)
            : (OB64.animationArt
              ? OB64.animationArt.editCount(state.animations) : 0)));
      if (count) tab.appendChild(makeBadge(String(count), 'count'));
      tabs.appendChild(tab);
    });
    panel.appendChild(tabs);
  }

  function filterControls(kind, ui, rerender) {
    var wrap = element('div', 'art-filter-controls');
    var input = element('input', 'art-search');
    input.type = 'search'; input.placeholder = kind === 'avatar' ? 'Search class name or ID' : 'Search item name or ID';
    input.setAttribute('data-art-focus-key', kind + '-search');
    input.value = kind === 'avatar' ? ui.avatarSearch : ui.iconSearch;
    input.addEventListener('input', function() {
      if (kind === 'avatar') ui.avatarSearch = input.value;
      else ui.iconSearch = input.value;
      rerender();
    });
    wrap.appendChild(input);
    var label = element('label', 'art-modified-filter');
    var checkbox = element('input'); checkbox.type = 'checkbox';
    checkbox.checked = kind === 'avatar' ? ui.avatarModifiedOnly : ui.iconModifiedOnly;
    checkbox.addEventListener('change', function() {
      if (kind === 'avatar') ui.avatarModifiedOnly = checkbox.checked;
      else ui.iconModifiedOnly = checkbox.checked;
      rerender();
    });
    label.appendChild(checkbox); label.appendChild(document.createTextNode(' Modified only'));
    wrap.appendChild(label);
    return wrap;
  }

  function avatarBrowser(state, ui, rerender) {
    var browser = element('aside', 'art-browser');
    browser.appendChild(filterControls('avatar', ui, rerender));
    var query = ui.avatarSearch.toLowerCase().trim();
    var rows = state.avatar.appearances.filter(function(appearance) {
      if (ui.avatarModifiedOnly && !state.avatar.edits[appearance.key] &&
          !A.blockedReason(state, 'avatar', appearance.key)) return false;
      return !query || appearance.className.toLowerCase().indexOf(query) >= 0 ||
        appearance.label.toLowerCase().indexOf(query) >= 0 ||
        classIdLabel(appearance.classId).toLowerCase().indexOf(query) >= 0 ||
        String(appearance.classId).indexOf(query) >= 0;
    });
    var summary = element('div', 'art-browser-summary', rows.length + ' routed appearance' + (rows.length === 1 ? '' : 's'));
    browser.appendChild(summary);
    var list = element('div', 'art-browser-list');
    list.setAttribute('data-art-scroll-key', 'avatars');
    rows.forEach(function(appearance) {
      var card = element('button', 'art-browser-card' + (ui.avatarKey === appearance.key ? ' selected' : ''));
      card.type = 'button';
      card.appendChild(wordCanvas(A.currentWords(state, 'avatar', appearance.key), 40, 48, 2, 'art-list-avatar'));
      var copy = element('span', 'art-browser-copy');
      copy.appendChild(element('strong', '', appearance.className));
      copy.appendChild(element('span', '', appearance.label));
      copy.appendChild(element('small', '', routeMetadata(appearance)));
      if (state.avatar.edits[appearance.key]) copy.appendChild(makeBadge('Edited · Detached', 'edited'));
      var blocked = blockedBadge(state, 'avatar', appearance.key);
      if (blocked) copy.appendChild(blocked);
      card.appendChild(copy);
      card.addEventListener('click', function() { ui.avatarKey = appearance.key; ui.selection = null; rerender(); });
      list.appendChild(card);
    });
    if (!rows.length) list.appendChild(element('p', 'art-browser-empty', 'No avatar appearances match this filter.'));
    browser.appendChild(list);
    return browser;
  }

  function iconBrowser(state, ui, rerender) {
    var browser = element('aside', 'art-browser');
    var packTabs = element('div', 'art-pack-tabs');
    [['equipment', 'Equipment'], ['special-item', 'Special Item']].forEach(function(row) {
      packTabs.appendChild(button(row[1], ui.iconPack === row[0] ? 'active' : '', function() {
        ui.iconPack = row[0];
        var pack = state.icons.packs[row[0]];
        if (!state.icons.byKey[ui.iconKey] || state.icons.byKey[ui.iconKey].pack !== row[0]) ui.iconKey = pack.icons[0].key;
        ui.selectedIconColor = pack.colorLibrary[0];
        ui.iconWheelValue = 31;
        ui.selection = null; rerender();
      }));
    });
    browser.appendChild(packTabs);
    browser.appendChild(filterControls('icon', ui, rerender));
    var query = ui.iconSearch.toLowerCase().trim(), pack = state.icons.packs[ui.iconPack];
    var rows = pack.icons.filter(function(icon) {
      if (ui.iconModifiedOnly && !state.icons.edits[icon.key] &&
          !A.blockedReason(state, 'icon', icon.key)) return false;
      return !query || icon.name.toLowerCase().indexOf(query) >= 0 ||
        String(icon.itemId).indexOf(query) >= 0 ||
        A.hex(icon.itemId, 3).toLowerCase().indexOf(query) >= 0;
    });
    browser.appendChild(element('div', 'art-browser-summary', rows.length + ' icon' + (rows.length === 1 ? '' : 's')));
    var list = element('div', 'art-browser-list art-icon-list');
    list.setAttribute('data-art-scroll-key', 'icons:' + ui.iconPack);
    rows.forEach(function(icon) {
      var card = element('button', 'art-browser-card' + (ui.iconKey === icon.key ? ' selected' : ''));
      card.type = 'button';
      card.appendChild(wordCanvas(A.currentWords(state, 'icon', icon.key),
        16, 16, 4, 'art-list-icon', null, previewBackgroundMode(ui)));
      var copy = element('span', 'art-browser-copy');
      copy.appendChild(element('strong', '', icon.name));
      copy.appendChild(element('small', '', icon.packLabel + ' · ID ' + icon.itemId + ' / ' + A.hex(icon.itemId, 3)));
      if (state.icons.edits[icon.key]) copy.appendChild(makeBadge('Edited', 'edited'));
      var blocked = blockedBadge(state, 'icon', icon.key);
      if (blocked) copy.appendChild(blocked);
      card.appendChild(copy);
      card.addEventListener('click', function() { ui.iconKey = icon.key; ui.selection = null; rerender(); });
      list.appendChild(card);
    });
    if (!rows.length) list.appendChild(element('p', 'art-browser-empty', 'No icons match this filter.'));
    browser.appendChild(list);
    return browser;
  }

  function coordinate(canvas, event, width, height) {
    var rect = canvas.getBoundingClientRect();
    return {
      x: Math.max(0, Math.min(width - 1, Math.floor((event.clientX - rect.left) * width / rect.width))),
      y: Math.max(0, Math.min(height - 1, Math.floor((event.clientY - rect.top) * height / rect.height)))
    };
  }

  function flood(words, width, start, replacement) {
    var original = words[start];
    if (original === replacement) return false;
    var queue = [start], seen = new Uint8Array(words.length), changedAny = false;
    seen[start] = 1;
    while (queue.length) {
      var index = queue.pop();
      if (words[index] !== original) continue;
      words[index] = replacement; changedAny = true;
      var x = index % width;
      var neighbors = [index - width, index + width];
      if (x) neighbors.push(index - 1);
      if (x + 1 < width) neighbors.push(index + 1);
      neighbors.forEach(function(next) {
        if (next >= 0 && next < words.length && !seen[next]) { seen[next] = 1; queue.push(next); }
      });
    }
    return changedAny;
  }

  function toolButtons(ui, kind, state, key, rerender, options) {
    var toolbar = element('div', 'art-toolbox');
    [['pencil', 'Pencil'], ['fill', 'Fill'], ['eyedropper', 'Eyedropper'],
      ['replace', 'Replace Color'], ['select', 'Select']].forEach(function(row) {
      toolbar.appendChild(button(row[1], ui.tool === row[0] ? 'active' : '', function() {
        ui.tool = row[0]; rerender();
      }));
    });
    var history = A.historyFor(state, kind, key);
    var undo = button('Undo', 'btn-secondary', function() {
      if (A.undo(state, kind, key)) { changed(options); notify(options, 'Undid the last art edit.'); rerender(); }
    });
    undo.disabled = !history.undo.length; toolbar.appendChild(undo);
    var redo = button('Redo', 'btn-secondary', function() {
      if (A.redo(state, kind, key)) { changed(options); notify(options, 'Redid the art edit.'); rerender(); }
    });
    redo.disabled = !history.redo.length; toolbar.appendChild(redo);
    var copy = button('Copy', 'btn-secondary', function() {
      if (!ui.selection) return;
      var words = A.currentWords(state, kind, key), width = kind === 'avatar' ? 40 : 16;
      var data = new Uint16Array(ui.selection.width * ui.selection.height), cursor = 0;
      for (var y = 0; y < ui.selection.height; y++) for (var x = 0; x < ui.selection.width; x++) {
        data[cursor++] = words[(ui.selection.y + y) * width + ui.selection.x + x];
      }
      ui.clipboard = { width: ui.selection.width, height: ui.selection.height, words: data };
      notify(options, 'Copied ' + data.length + ' pixels from the selection.'); rerender();
    });
    copy.disabled = !ui.selection; toolbar.appendChild(copy);
    var paste = button('Paste', 'btn-secondary', function() {
      if (!ui.selection || !ui.clipboard) return;
      var width = kind === 'avatar' ? 40 : 16, height = kind === 'avatar' ? 48 : 16;
      var words = A.currentWords(state, kind, key).slice();
      for (var y = 0; y < ui.clipboard.height; y++) for (var x = 0; x < ui.clipboard.width; x++) {
        var tx = ui.selection.x + x, ty = ui.selection.y + y;
        if (tx < width && ty < height) words[ty * width + tx] = ui.clipboard.words[y * ui.clipboard.width + x];
      }
      try {
        if (A.setEditWords(state, kind, key, words)) {
          changed(options); notify(options, 'Pasted the copied pixels.'); rerender();
        }
      } catch (error) {
        A.setBlocked(state, kind, key, error.message);
        notify(options, error.message); rerender();
      }
    });
    paste.disabled = !ui.selection || !ui.clipboard; toolbar.appendChild(paste);
    if (kind === 'icon') toolbar.appendChild(previewBackgroundButton(ui, rerender));
    return toolbar;
  }

  function installCanvasEditing(canvas, state, kind, key, width, height, scale, ui, options, rerender) {
    var drawing = false, working = null, selectionStart = null, changedPixels = false;
    var backgroundMode = kind === 'icon'
      ? previewBackgroundMode(ui) : 'checkerboard';
    canvas.tabIndex = 0;
    function selectedWord() {
      return kind === 'avatar' ? ui.selectedAvatarColor : ui.selectedIconColor;
    }
    function applyPoint(point) {
      var index = point.y * width + point.x, word = selectedWord();
      if (ui.tool === 'pencil') {
        if (working[index] !== word) { working[index] = word; changedPixels = true; }
      }
    }
    canvas.addEventListener('pointerdown', function(event) {
      canvas.focus();
      var point = coordinate(canvas, event, width, height), index = point.y * width + point.x;
      var current = A.currentWords(state, kind, key);
      if (ui.tool === 'eyedropper') {
        if (kind === 'avatar') {
          ui.selectedAvatarColor = current[index];
          ui.avatarWheelValue = Math.max((current[index] >>> 11) & 31,
            (current[index] >>> 6) & 31, (current[index] >>> 1) & 31);
        } else {
          ui.selectedIconColor = current[index];
          if (current[index] & 1) {
            ui.iconWheelValue = Math.max((current[index] >>> 11) & 31,
              (current[index] >>> 6) & 31, (current[index] >>> 1) & 31);
          }
        }
        ui.tool = 'pencil'; rerender(); return;
      }
      if (ui.tool === 'fill' || ui.tool === 'replace') {
        var words = current.slice(), didChange = false;
        if (ui.tool === 'fill') didChange = flood(words, width, index, selectedWord());
        else {
          var source = words[index], replacement = selectedWord();
          if (source !== replacement) for (var i = 0; i < words.length; i++) {
            if (words[i] === source) { words[i] = replacement; didChange = true; }
          }
        }
        try {
          if (didChange && A.setEditWords(state, kind, key, words)) {
            changed(options); rerender();
          }
        } catch (error) {
          A.setBlocked(state, kind, key, error.message);
          notify(options, error.message); rerender();
        }
        return;
      }
      if (ui.tool === 'select') {
        drawing = true; selectionStart = point;
        ui.selection = { x: point.x, y: point.y, width: 1, height: 1 };
        canvas.setPointerCapture(event.pointerId);
        drawWords(canvas, current, width, height, scale, ui.selection, backgroundMode);
        return;
      }
      drawing = true; working = current.slice(); changedPixels = false;
      canvas.setPointerCapture(event.pointerId); applyPoint(point);
      drawWords(canvas, working, width, height, scale, ui.selection, backgroundMode);
    });
    canvas.addEventListener('pointermove', function(event) {
      if (!drawing) return;
      var point = coordinate(canvas, event, width, height);
      if (ui.tool === 'select') {
        var left = Math.min(selectionStart.x, point.x), top = Math.min(selectionStart.y, point.y);
        ui.selection = {
          x: left, y: top,
          width: Math.max(selectionStart.x, point.x) - left + 1,
          height: Math.max(selectionStart.y, point.y) - top + 1
        };
        drawWords(canvas, A.currentWords(state, kind, key), width, height,
          scale, ui.selection, backgroundMode);
      } else {
        applyPoint(point);
        drawWords(canvas, working, width, height, scale, ui.selection, backgroundMode);
      }
    });
    function finish(event) {
      if (!drawing) return;
      drawing = false;
      if (event && canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
      if (ui.tool === 'select') { rerender(); return; }
      if (!changedPixels) return;
      try {
        if (A.setEditWords(state, kind, key, working)) { changed(options); rerender(); }
      } catch (error) {
        A.setBlocked(state, kind, key, error.message);
        notify(options, error.message); rerender();
      }
    }
    canvas.addEventListener('pointerup', finish);
    canvas.addEventListener('pointercancel', finish);
    canvas.addEventListener('keydown', function(event) {
      if (!(event.ctrlKey || event.metaKey)) return;
      var keyName = event.key.toLowerCase();
      if (keyName === 'z' && !event.shiftKey) {
        event.preventDefault(); if (A.undo(state, kind, key)) { changed(options); rerender(); }
      } else if (keyName === 'y' || (keyName === 'z' && event.shiftKey)) {
        event.preventDefault(); if (A.redo(state, kind, key)) { changed(options); rerender(); }
      }
    });
  }

  function previewPair(original, current, width, height, backgroundMode) {
    var wrap = element('div', 'art-preview-pair');
    [['Original', original], ['Current', current]].forEach(function(row) {
      var card = element('figure', 'art-preview-card');
      card.appendChild(element('figcaption', '', row[0]));
      card.appendChild(wordCanvas(row[1], width, height, 4,
        'art-preview-canvas', null, backgroundMode));
      wrap.appendChild(card);
    });
    return wrap;
  }

  function avatarEditor(state, rom, ui, options, rerender) {
    var appearance = state.avatar.byKey[ui.avatarKey] || state.avatar.appearances[0];
    ui.avatarKey = appearance.key;
    var current = A.currentWords(state, 'avatar', appearance.key), edited = !!state.avatar.edits[appearance.key];
    var blockedReason = A.blockedReason(state, 'avatar', appearance.key);
    var editor = element('main', 'art-editor');
    var heading = element('div', 'art-editor-heading');
    var copy = element('div'); copy.appendChild(element('h3', '', appearance.className + ' — ' + appearance.label));
    copy.appendChild(element('p', '', routeMetadata(appearance)));
    heading.appendChild(copy); if (edited) heading.appendChild(makeBadge('Edited · Detached', 'edited'));
    var blocked = blockedBadge(state, 'avatar', appearance.key);
    if (blocked) heading.appendChild(blocked);
    editor.appendChild(heading);
    if (edited) editor.appendChild(element('div', 'art-detach-note',
      'This routed appearance is detached automatically. Other classes and other appearances remain attached to retail art.'));
    editor.appendChild(previewPair(appearance.originalWords, current, 40, 48));
    editor.appendChild(toolButtons(ui, 'avatar', state, appearance.key, rerender, options));
    var workspace = element('div', 'art-workspace');
    var canvas = wordCanvas(current, 40, 48, 12, 'art-edit-canvas', ui.selection);
    installCanvasEditing(canvas, state, 'avatar', appearance.key, 40, 48, 12, ui, options, rerender);
    workspace.appendChild(canvas);
    var palette = element('aside', 'art-palette-panel');
    var count = A.distinctCount(current, true);
    var counter = element('div', 'art-color-counter', 'Colors used: ' + count + ' / 80');
    counter.setAttribute('title', 'An avatar contains 1,920 pixels, but those pixels may reuse at most 80 opaque RGBA5551 palette colors. Detaching an appearance does not raise this limit.');
    palette.appendChild(counter);
    palette.appendChild(selectedColorBlock(state, 'avatar', ui, rerender));
    palette.appendChild(element('p', 'art-palette-help',
      'Choose any opaque 16-bit RGB555 color. The wheel is quantized to native five-bit channels; use the brightness slider or exact R/G/B controls for the complete 32,768-color space.'));
    palette.appendChild(colorWheel(state, 'avatar', null, ui, function(word, commit) {
      ui.selectedAvatarColor = word; ui.tool = 'pencil';
      if (commit) rerender();
    }));
    workspace.appendChild(palette); editor.appendChild(workspace);
    var actions = element('div', 'art-asset-actions');
    var reset = button('Reset Current Avatar', 'btn-secondary', function() {
      if (!edited && !blockedReason) return;
      if (edited) A.setEditWords(state, 'avatar', appearance.key, appearance.originalWords);
      A.setBlocked(state, 'avatar', appearance.key, '');
      changed(options); notify(options, appearance.className + ' restored to the vanilla ROM; detachment cancelled.'); rerender();
    }); reset.disabled = !edited && !blockedReason; actions.appendChild(reset);
    var importInput = element('input', 'art-image-input');
    importInput.type = 'file';
    importInput.accept = 'image/png,image/jpeg,.png,.jpg,.jpeg';
    importInput.setAttribute('aria-label',
      'Select a PNG or JPEG image to import and convert as an avatar');
    importInput.addEventListener('change', async function() {
      var file = importInput.files && importInput.files[0];
      if (!file) return;
      try {
        var source = await decodeAvatarImageSource(file);
        showAvatarImportDialog(source, appearance, state, options, rerender);
      } catch (error) {
        var message = 'Avatar image import blocked: ' + error.message;
        A.setBlocked(state, 'avatar', appearance.key, message);
        notify(options, message);
        rerender();
      } finally {
        importInput.value = '';
      }
    });
    actions.appendChild(importInput);
    actions.appendChild(button('Import & Convert Image', 'btn-secondary', function() {
      importInput.click();
    }));
    if (OB64.spriteEditorUI && OB64.spriteEditorUI.openLibraryPicker) {
      actions.appendChild(button('Import from Sprite Library…', 'btn-secondary', function() {
        OB64.spriteEditorUI.openLibraryPicker(rom, {
          title: 'Choose Avatar Source',
          actionLabel: 'Convert to Avatar',
          onStatus: function(message) { notify(options, message); }
        }, function(source) {
          showAvatarImportDialog(source, appearance, state, options, rerender);
        });
      }));
    }
    actions.appendChild(button('Export Avatar PNG', 'btn-secondary', function() {
      nativePngDownload(current, 40, 48,
        'class-' + appearance.classId.toString(16).padStart(2, '0') + '-' + appearance.label.toLowerCase().replace(/[^a-z0-9]+/g, '-') + '.png');
    }));
    editor.appendChild(actions);
    return editor;
  }

  function packColorCount(state, pack) {
    var colors = new Set();
    pack.icons.forEach(function(icon) {
      A.currentWords(state, 'icon', icon.key).forEach(function(word) { if (word & 1) colors.add(word); });
    });
    return colors.size;
  }

  function iconEditor(state, rom, ui, options, rerender) {
    var icon = state.icons.byKey[ui.iconKey];
    if (!icon || icon.pack !== ui.iconPack) icon = state.icons.packs[ui.iconPack].icons[0];
    ui.iconKey = icon.key;
    var pack = state.icons.packs[icon.pack];
    if (ui.selectedIconColor !== pack.transparentWord &&
        !pack.paletteColorSet.has(ui.selectedIconColor)) {
      ui.selectedIconColor = pack.colorLibrary[0];
      ui.iconWheelValue = 31;
    }
    var current = A.currentWords(state, 'icon', icon.key);
    var edited = !!state.icons.edits[icon.key], editor = element('main', 'art-editor');
    var blockedReason = A.blockedReason(state, 'icon', icon.key);
    var heading = element('div', 'art-editor-heading'), copy = element('div');
    copy.appendChild(element('h3', '', icon.name));
    copy.appendChild(element('p', '', icon.packLabel + ' · item ID ' + icon.itemId + ' / ' + A.hex(icon.itemId, 3)));
    heading.appendChild(copy); if (edited) heading.appendChild(makeBadge('Edited', 'edited'));
    var blocked = blockedBadge(state, 'icon', icon.key);
    if (blocked) heading.appendChild(blocked);
    editor.appendChild(heading);
    editor.appendChild(element('div', 'art-pack-note',
      'This icon remains in its shared complete pack. Export rebuilds every plane and the pack-wide 256-entry palette together.'));
    var backgroundMode = previewBackgroundMode(ui);
    editor.appendChild(previewPair(icon.originalWords, current, 16, 16,
      backgroundMode));
    editor.appendChild(toolButtons(ui, 'icon', state, icon.key, rerender, options));
    var workspace = element('div', 'art-workspace');
    var canvas = wordCanvas(current, 16, 16, 24,
      'art-edit-canvas art-icon-edit-canvas', ui.selection, backgroundMode);
    installCanvasEditing(canvas, state, 'icon', icon.key, 16, 16, 24, ui, options, rerender);
    workspace.appendChild(canvas);
    var palette = element('aside', 'art-palette-panel');
    var count = packColorCount(state, pack);
    var counter = element('div', 'art-color-counter', 'Pack colors used: ' + count + ' / 255');
    if (count > 255) counter.classList.add('blocked');
    counter.setAttribute('title', 'Every icon in this pack shares one 256-entry CI8 palette. One entry is reserved for transparency, leaving at most 255 opaque RGBA5551 colors across the complete pack.');
    palette.appendChild(counter);
    palette.appendChild(selectedColorBlock(state, 'icon', ui, rerender));
    palette.appendChild(element('p', 'art-palette-help',
      'Every visible wheel color is an exact entry from this pack\'s 256-entry palette. Transparency uses the selected preview background.'));
    palette.appendChild(colorWheel(state, 'icon', pack, ui, function(word, commit) {
      ui.selectedIconColor = word; ui.tool = 'pencil';
      if (commit) rerender();
    }));
    workspace.appendChild(palette); editor.appendChild(workspace);
    var actions = element('div', 'art-asset-actions');
    var reset = button('Reset Current Icon', 'btn-secondary', function() {
      if (!edited && !blockedReason) return;
      if (edited) A.setEditWords(state, 'icon', icon.key, icon.originalWords);
      A.setBlocked(state, 'icon', icon.key, '');
      A.refreshIconPackBlocked(state, icon.pack);
      changed(options); notify(options, icon.name + ' restored to the vanilla ROM; its complete pack will be recalculated.'); rerender();
    }); reset.disabled = !edited && !blockedReason; actions.appendChild(reset);
    if (OB64.spriteEditorUI && OB64.spriteEditorUI.openLibraryPicker) {
      actions.appendChild(button('Import from Sprite Library…', 'btn-secondary', function() {
        OB64.spriteEditorUI.openLibraryPicker(rom, {
          title: 'Choose Item Icon Source',
          actionLabel: 'Convert to Item Icon',
          onStatus: function(message) { notify(options, message); }
        }, function(source) {
          try {
            var rgba = OB64.spriteLibrary.nearestResize(source.rgba,
              source.width, source.height, 16, 16);
            var words = new Uint16Array(256);
            for (var pixel = 0; pixel < 256; pixel++) {
              var offset = pixel * 4;
              if (rgba[offset + 3] < 128) {
                words[pixel] = pack.transparentWord;
              } else {
                var word = A.rgba5551Word(
                  Math.round(rgba[offset] * 31 / 255),
                  Math.round(rgba[offset + 1] * 31 / 255),
                  Math.round(rgba[offset + 2] * 31 / 255), true);
                words[pixel] = nearestPaletteWord(pack, word);
              }
            }
            if (A.setEditWords(state, 'icon', icon.key, words)) {
              changed(options);
              notify(options, source.name + ' converted to the current item-icon palette.');
            }
            rerender();
          } catch (error) {
            notify(options, 'Sprite Library icon import blocked: ' + error.message);
          }
        });
      }));
    }
    actions.appendChild(button('Export Icon PNG', 'btn-secondary', function() {
      nativePngDownload(current, 16, 16,
        icon.pack + '-' + String(icon.itemId).padStart(3, '0') + '-' + icon.name.toLowerCase().replace(/[^a-z0-9]+/g, '-') + '.png');
    }));
    editor.appendChild(actions);
    return editor;
  }

  function render(panel, rom, options, preserveViewport) {
    var active = document.activeElement;
    var focus = null;
    if (active && panel.contains(active)) {
      var focusKey = active.getAttribute('data-art-focus-key');
      if (focusKey) {
        focus = {
          key: focusKey,
          start: typeof active.selectionStart === 'number' ? active.selectionStart : null,
          end: typeof active.selectionEnd === 'number' ? active.selectionEnd : null
        };
      }
    }
    var state = rom && rom.art;
    var ui = state && state.supported ? ensureUi(state) : null;
    captureBrowserScroll(ui, panel, preserveViewport);
    panel.innerHTML = '';
    if (!state) return;
    if (!state.supported) {
      var unavailable = element('div', 'art-unavailable');
      unavailable.appendChild(element('h2', '', 'Art and Animation'));
      unavailable.appendChild(element('p', '', state.unavailableReason));
      panel.appendChild(unavailable); return;
    }
    function rerender() { render(panel, rom, options, true); }
    topHeader(panel, rom, state, ui, options, rerender);
    var shell = element('div', 'art-shell');
    if (ui.subtab === 'avatars') {
      shell.appendChild(avatarBrowser(state, ui, rerender));
      shell.appendChild(avatarEditor(state, rom, ui, options, rerender));
    } else if (ui.subtab === 'icons') {
      shell.appendChild(iconBrowser(state, ui, rerender));
      shell.appendChild(iconEditor(state, rom, ui, options, rerender));
    } else if (ui.subtab === 'army') {
      if (OB64.armySpriteUI) {
        var armyParts = OB64.armySpriteUI.render(
          state, ui, options, rerender, rom);
        if (armyParts.browser) shell.appendChild(armyParts.browser);
        shell.appendChild(armyParts.editor);
        if (!armyParts.browser) shell.classList.add('art-single-panel-shell');
      } else {
        shell.classList.add('art-single-panel-shell');
        shell.appendChild(element('div', 'art-unavailable',
          'Army sprite component is not loaded.'));
      }
    } else if (OB64.animationUI) {
      shell.classList.add('art-animation-shell');
      shell.appendChild(OB64.animationUI.render(state, ui, options, rerender, rom));
    } else {
      shell.appendChild(element('div', 'art-unavailable',
        'Combat animation component is not loaded.'));
    }
    panel.appendChild(shell);
    restoreBrowserScroll(ui, panel, preserveViewport);
    if (focus) {
      var replacement = panel.querySelector('[data-art-focus-key="' + focus.key + '"]');
      if (replacement) {
        replacement.focus({ preventScroll: true });
        if (focus.start !== null && replacement.setSelectionRange) {
          replacement.setSelectionRange(focus.start, focus.end);
        }
      }
    }
  }

  OB64.artUI = {
    render: render,
    drawWords: drawWords,
    rgbaPixelsForWords: rgbaPixelsForWords,
    nativePngCanvas: nativePngCanvas,
    previewBackgroundMode: previewBackgroundMode,
    togglePreviewBackground: togglePreviewBackground,
    hsvForWord: hsvForWord,
    wordForHsv: wordForHsv,
    nearestPaletteWord: nearestPaletteWord,
    openAnimationRoute: openAnimationRoute,
    captureBrowserScroll: captureBrowserScroll,
    restoreBrowserScroll: restoreBrowserScroll,
    decodeImageSource: decodeAvatarImageSource,
    decodeAvatarImageSource: decodeAvatarImageSource,
    decodeAvatarPngSource: decodeAvatarPngSource,
    decodeAvatarPngFile: decodeAvatarPngFile
  };
})();
