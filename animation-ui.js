// OB64 Mod Editor - bounded combat-animation editor UI

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var M = OB64.animationArt;

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

  function badge(text, kind) { return element('span', 'art-badge art-badge-' + kind, text); }
  function notify(options, text) { if (options && options.onStatus) options.onStatus(text); }
  function changed(options) { if (options && options.onChange) options.onChange(); }

  function expand5(value) { return (value << 3) | (value >>> 2); }

  function wordRgb(word) {
    return [expand5((word >>> 11) & 31), expand5((word >>> 6) & 31),
      expand5((word >>> 1) & 31)];
  }

  function normalizeIntensity(value) {
    return Number.isInteger(value) && value >= 0 && value <= 15 ? value : 15;
  }

  function intensityColorCss(word, intensity) {
    var rgb = wordRgb(word);
    var alpha = normalizeIntensity(intensity) / 15;
    return 'rgba(' + rgb[0] + ',' + rgb[1] + ',' + rgb[2] + ',' +
      alpha.toFixed(4) + ')';
  }

  function hueDetails(word, index) {
    var rgb = wordRgb(word);
    var red = rgb[0] / 255, green = rgb[1] / 255, blue = rgb[2] / 255;
    var maximum = Math.max(red, green, blue);
    var minimum = Math.min(red, green, blue);
    var difference = maximum - minimum;
    var hue = 0;
    if (difference) {
      if (maximum === red) hue = ((green - blue) / difference) % 6;
      else if (maximum === green) hue = ((blue - red) / difference) + 2;
      else hue = ((red - green) / difference) + 4;
      hue *= 60;
      if (hue < 0) hue += 360;
    }
    var saturation = maximum ? difference / maximum : 0;
    return {
      index: index,
      neutral: saturation <= 0.18,
      hue: hue,
      hueGroup: Math.floor(hue / 15),
      saturation: saturation,
      value: maximum
    };
  }

  function paletteHueOrder(palette) {
    var entries = [];
    for (var index = 0; index < palette.length; index++) {
      entries.push(hueDetails(palette[index], index));
    }
    entries.sort(function(left, right) {
      if (left.neutral !== right.neutral) return left.neutral ? -1 : 1;
      if (left.neutral) return left.value - right.value || left.index - right.index;
      return left.hueGroup - right.hueGroup || left.value - right.value ||
        left.saturation - right.saturation || left.hue - right.hue ||
        left.index - right.index;
    });
    return entries.map(function(entry) { return entry.index; });
  }

  function sourceOver(target, source) {
    var sourceAlpha = source[3];
    if (!sourceAlpha) return target;
    if (sourceAlpha === 255 || !target[3]) return source;
    var targetAlpha = target[3];
    var outputAlpha = sourceAlpha +
      Math.floor((targetAlpha * (255 - sourceAlpha) + 127) / 255);
    var output = [0, 0, 0, outputAlpha];
    for (var channel = 0; channel < 3; channel++) {
      var premultiplied = source[channel] * sourceAlpha +
        Math.floor((target[channel] * targetAlpha * (255 - sourceAlpha) + 127) / 255);
      output[channel] = Math.floor((premultiplied + Math.floor(outputAlpha / 2)) /
        outputAlpha);
    }
    return output;
  }

  function childOrdinalForSource(source, weaponChildOrdinal) {
    if (!source.weaponSelectable) return source.childOrdinal;
    var ordinal = Number.isInteger(weaponChildOrdinal) ? weaponChildOrdinal : 0;
    return Math.max(0, Math.min(source.sprite.childCount - 1, ordinal));
  }

  function editFor(animationState, sourceKey, childOrdinal, overrides) {
    var identity = M.childKey(sourceKey, childOrdinal);
    return overrides && overrides[identity]
      ? overrides[identity]
      : M.currentEdit(animationState, sourceKey, childOrdinal);
  }

  function drawLayerInto(output, animation, layer, animationState, style, overrides,
      weaponChildOrdinal) {
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = childOrdinalForSource(source, weaponChildOrdinal);
    var edit = editFor(animationState, layer.sourceKey, childOrdinal, overrides);
    var canvas = animation.canvas;
    var left = layer.drawOffsetX - canvas.originX;
    var top = layer.drawOffsetY - canvas.originY;
    var pixel = 0;
    for (var y = 0; y < source.sprite.height; y++) {
      for (var x = 0; x < source.sprite.width; x++, pixel++) {
        var intensity = edit.intensity[pixel];
        if (!intensity) continue;
        var rgb = wordRgb(source.palette[edit.indices[pixel]]);
        var alpha = intensity * 17;
        if (style === 'context') {
          var gray = Math.round((rgb[0] * 3 + rgb[1] * 6 + rgb[2]) / 10);
          rgb = [Math.round((gray + 112) / 2), Math.round((gray + 112) / 2),
            Math.round((gray + 112) / 2)];
          alpha = Math.max(24, Math.round(alpha * 0.34));
        }
        var targetIndex = ((top + y) * canvas.width + left + x) * 4;
        var target = [output[targetIndex], output[targetIndex + 1],
          output[targetIndex + 2], output[targetIndex + 3]];
        var blended = sourceOver(target, [rgb[0], rgb[1], rgb[2], alpha]);
        output[targetIndex] = blended[0]; output[targetIndex + 1] = blended[1];
        output[targetIndex + 2] = blended[2]; output[targetIndex + 3] = blended[3];
      }
    }
  }

  function framePixels(animation, frame, animationState, selectedSourceKey, overrides,
      weaponChildOrdinal) {
    var output = new Uint8ClampedArray(animation.canvas.width * animation.canvas.height * 4);
    if (!selectedSourceKey) {
      frame.layers.forEach(function(layer) {
        drawLayerInto(output, animation, layer, animationState, 'normal', overrides,
          weaponChildOrdinal);
      });
      return output;
    }
    frame.layers.forEach(function(layer) {
      if (layer.sourceKey !== selectedSourceKey) {
        drawLayerInto(output, animation, layer, animationState, 'context', overrides,
          weaponChildOrdinal);
      }
    });
    frame.layers.forEach(function(layer) {
      if (layer.sourceKey === selectedSourceKey) {
        drawLayerInto(output, animation, layer, animationState, 'normal', overrides,
          weaponChildOrdinal);
      }
    });
    return output;
  }

  function checker(context, width, height, scale) {
    context.fillStyle = '#d8d2bc'; context.fillRect(0, 0, width * scale, height * scale);
    context.fillStyle = '#8d887a';
    for (var y = 0; y < height; y++) for (var x = 0; x < width; x++) {
      if ((x + y) & 1) context.fillRect(x * scale, y * scale, scale, scale);
    }
  }

  function paintPixels(canvas, width, height, pixels, scale) {
    canvas.width = width * scale; canvas.height = height * scale;
    var context = canvas.getContext('2d');
    context.imageSmoothingEnabled = false;
    checker(context, width, height, scale);
    for (var y = 0; y < height; y++) for (var x = 0; x < width; x++) {
      var offset = (y * width + x) * 4, alpha = pixels[offset + 3];
      if (!alpha) continue;
      context.fillStyle = 'rgba(' + pixels[offset] + ',' + pixels[offset + 1] + ',' +
        pixels[offset + 2] + ',' + (alpha / 255).toFixed(4) + ')';
      context.fillRect(x * scale, y * scale, scale, scale);
    }
  }

  function drawFrame(canvas, animation, frame, animationState, scale,
      selectedLayer, overrides, editorGrid, weaponChildOrdinal) {
    var selectedKey = selectedLayer ? selectedLayer.sourceKey : null;
    paintPixels(canvas, animation.canvas.width, animation.canvas.height,
      framePixels(animation, frame, animationState, selectedKey, overrides,
        weaponChildOrdinal), scale);
    if (!selectedLayer) return;
    var context = canvas.getContext('2d');
    var left = (selectedLayer.drawOffsetX - animation.canvas.originX) * scale;
    var top = (selectedLayer.drawOffsetY - animation.canvas.originY) * scale;
    var width = selectedLayer.width * scale, height = selectedLayer.height * scale;
    if (editorGrid && scale >= 6) {
      context.save(); context.strokeStyle = 'rgba(255,255,255,0.16)'; context.lineWidth = 1;
      for (var x = 1; x < selectedLayer.width; x++) {
        context.beginPath(); context.moveTo(left + x * scale + 0.5, top);
        context.lineTo(left + x * scale + 0.5, top + height); context.stroke();
      }
      for (var y = 1; y < selectedLayer.height; y++) {
        context.beginPath(); context.moveTo(left, top + y * scale + 0.5);
        context.lineTo(left + width, top + y * scale + 0.5); context.stroke();
      }
      context.restore();
    }
    context.save();
    context.strokeStyle = '#ffd45c'; context.lineWidth = Math.max(2, Math.round(scale / 3));
    context.setLineDash([scale, Math.max(2, Math.round(scale / 2))]);
    context.strokeRect(left + 1, top + 1, Math.max(1, width - 2), Math.max(1, height - 2));
    context.restore();
  }

  function selectedAnimation(state, ui) {
    var animation = state.animations.byKey[ui.animationKey];
    if (!animation) animation = state.animations.specs[0];
    ui.animationKey = animation.key;
    weaponChildForAnimation(ui, animation);
    return animation;
  }

  function weaponChildForAnimation(ui, animation) {
    if (!ui.animationWeaponChildren) ui.animationWeaponChildren = {};
    var ordinal = ui.animationWeaponChildren[animation.key];
    if (!Number.isInteger(ordinal)) {
      ordinal = Number.isInteger(ui.animationWeaponChild) ? ui.animationWeaponChild : 0;
    }
    var maximum = Math.max(0, (animation.spec.weaponChildCount || 1) - 1);
    ordinal = Math.max(0, Math.min(maximum, ordinal));
    ui.animationWeaponChildren[animation.key] = ordinal;
    ui.animationWeaponChild = ordinal;
    return ordinal;
  }

  function setWeaponChild(ui, animation, ordinal) {
    if (!ui.animationWeaponChildren) ui.animationWeaponChildren = {};
    var maximum = Math.max(0, (animation.spec.weaponChildCount || 1) - 1);
    ordinal = Math.max(0, Math.min(maximum,
      Number.isInteger(ordinal) ? ordinal : 0));
    ui.animationWeaponChildren[animation.key] = ordinal;
    ui.animationWeaponChild = ordinal;
    return ordinal;
  }

  function animationLabel(animation) {
    return animation.spec.className + ' ' + animation.spec.actionName +
      (animation.spec.variantLabel ? ' · ' + animation.spec.variantLabel : '');
  }

  function retailMappingText(animation, childCount) {
    var mapped = animation.spec.retailMappedWeaponOrdinals || [];
    if (mapped.length === childCount && mapped.every(function(value, index) {
      return value === index;
    })) return ' Every physical child has a known retail item mapping.';
    return ' Retail item mapping is known for physical child ordinal' +
      (mapped.length === 1 ? ' ' : 's ') + mapped.join(', ') +
      '. Other physical children have no known retail selection.';
  }

  function selectedFrame(animation, ui) {
    ui.animationFrame = Math.max(0, Math.min(animation.frames.length - 1,
      Number.isInteger(ui.animationFrame) ? ui.animationFrame : 0));
    return animation.frames[ui.animationFrame];
  }

  function selectedLayer(frame, ui) {
    var layer = frame.layers.find(function(row) { return row.ordinal === ui.animationLayer; });
    if (!layer) layer = frame.layers[0];
    ui.animationLayer = layer.ordinal;
    return layer;
  }

  function selectedChildOrdinal(source, ui) {
    return childOrdinalForSource(source, ui.animationWeaponChild);
  }

  function selectLayer(state, animation, frame, layer, ui) {
    ui.animationLayer = layer.ordinal;
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = selectedChildOrdinal(source, ui);
    var edit = M.currentEdit(state.animations, layer.sourceKey, childOrdinal);
    var visible = 0;
    while (visible < edit.intensity.length && !edit.intensity[visible]) visible++;
    if (visible >= edit.intensity.length) visible = 0;
    ui.animationPaletteIndex = edit.indices[visible];
    ui.animationIntensity = normalizeIntensity(ui.animationIntensity);
  }

  function samplePixel(ui, edit, pixel) {
    ui.animationPaletteIndex = edit.indices[pixel];
    ui.animationIntensity = normalizeIntensity(edit.intensity[pixel]);
    ui.animationTool = 'pencil';
  }

  function animationStats(animation) {
    var sources = Object.keys(animation.artByKey).map(function(key) {
      return animation.artByKey[key];
    });
    return {
      frames: animation.frames.length,
      layers: animation.frames.reduce(function(total, frame) {
        return total + frame.layers.length;
      }, 0),
      sources: sources.length,
      weaponSources: sources.filter(function(source) {
        return source.weaponSelectable;
      }).length
    };
  }

  function animationPicker(state, selected, ui, rerender) {
    var section = element('section', 'animation-corpus-section');
    var heading = element('div', 'animation-section-heading');
    heading.appendChild(element('h3', '', 'Class and action animation'));
    heading.appendChild(element('p', '',
      'Choose one verified class/action sequence. Each entry is parsed completely from the loaded ROM.'));
    section.appendChild(heading);
    var list = element('div', 'animation-corpus-grid');
    state.animations.specs.slice().sort(function(left, right) {
      return left.spec.displayOrder - right.spec.displayOrder;
    }).forEach(function(animation) {
      var stats = animationStats(animation);
      var card = element('button', 'animation-corpus-card' +
        (animation.key === selected.key ? ' selected' : ''));
      card.type = 'button';
      card.appendChild(element('strong', '', animation.spec.className));
      card.appendChild(element('span', '', animation.spec.actionName +
        (animation.spec.variantLabel ? ' · ' + animation.spec.variantLabel : '')));
      card.appendChild(element('small', '', stats.frames + ' frames · ' +
        stats.layers + ' layers · ' + stats.sources + ' sources'));
      card.appendChild(element('small', '', animation.spec.weaponChildCount
        ? animation.spec.weaponChildCount + ' physical weapon-group children'
        : 'No verified equipment-child group'));
      card.addEventListener('click', function() {
        if (animation.key === selected.key) return;
        ui.animationKey = animation.key;
        ui.animationFrame = 0;
        ui.animationLayer = animation.frames[0].layers[0].ordinal;
        weaponChildForAnimation(ui, animation);
        selectLayer(state, animation, animation.frames[0],
          animation.frames[0].layers[0], ui);
        rerender();
      });
      list.appendChild(card);
    });
    section.appendChild(list);
    return section;
  }

  function sequenceStrip(state, animation, ui, rerender) {
    var section = element('section', 'animation-sequence-section');
    var heading = element('div', 'animation-section-heading');
    heading.appendChild(element('h3', '', 'Frame sequence'));
    heading.appendChild(element('p', '',
      'Tick counts are read-only. Select a frame to load its complete layer stack below.'));
    section.appendChild(heading);
    var strip = element('div', 'animation-sequence-strip');
    strip.setAttribute('data-art-scroll-key', 'animations:sequence:' + animation.key);
    animation.frames.forEach(function(frame) {
      var card = element('button', 'animation-frame-card' +
        (frame.sequenceIndex === ui.animationFrame ? ' selected' : ''));
      card.type = 'button';
      card.appendChild(element('strong', 'animation-frame-ticks', frame.ticks + ' ticks'));
      var canvas = element('canvas', 'animation-frame-thumbnail');
      drawFrame(canvas, animation, frame, state.animations, 4, null, null, false,
        weaponChildForAnimation(ui, animation));
      card.appendChild(canvas);
      card.appendChild(element('span', '', 'Frame ' + (frame.sequenceIndex + 1) +
        ' · token ' + M.hex(frame.token, 2)));
      card.addEventListener('click', function() {
        ui.animationFrame = frame.sequenceIndex; ui.animationLayer = frame.layers[0].ordinal;
        selectLayer(state, animation, frame, frame.layers[0], ui); rerender();
      });
      strip.appendChild(card);
    });
    section.appendChild(strip);
    return section;
  }

  function layerKind(source) {
    if (source.weaponSelectable) {
      return 'Equipment-linked · ' + source.sprite.childCount + ' physical children';
    }
    if (source.sprite.childCount > 1) {
      return 'Body/context · ' + source.sprite.childCount + ' source children';
    }
    return 'Single-child art';
  }

  function childPixels(source, animationState, childOrdinal) {
    var edit = M.currentEdit(animationState, source.key, childOrdinal);
    var output = new Uint8ClampedArray(
      source.sprite.width * source.sprite.height * 4);
    for (var pixel = 0; pixel < edit.indices.length; pixel++) {
      var rgb = wordRgb(source.palette[edit.indices[pixel]]);
      var offset = pixel * 4;
      output[offset] = rgb[0]; output[offset + 1] = rgb[1];
      output[offset + 2] = rgb[2]; output[offset + 3] = edit.intensity[pixel] * 17;
    }
    return output;
  }

  function weaponSourceForFrame(animation, frame, selectedLayer) {
    var selectedSource = selectedLayer && animation.artByKey[selectedLayer.sourceKey];
    if (selectedSource && selectedSource.weaponSelectable) return selectedSource;
    for (var layerIndex = 0; layerIndex < frame.layers.length; layerIndex++) {
      var frameSource = animation.artByKey[frame.layers[layerIndex].sourceKey];
      if (frameSource.weaponSelectable) return frameSource;
    }
    var sourceKeys = Object.keys(animation.artByKey);
    for (var sourceIndex = 0; sourceIndex < sourceKeys.length; sourceIndex++) {
      var source = animation.artByKey[sourceKeys[sourceIndex]];
      if (source.weaponSelectable) return source;
    }
    return null;
  }

  function weaponPicker(state, animation, frame, layer, ui, rerender) {
    var source = weaponSourceForFrame(animation, frame, layer);
    var section = element('section', 'animation-weapon-section');
    var heading = element('div', 'animation-section-heading');
    heading.appendChild(element('h3', '', 'Weapon sprite'));
    if (!source) {
      heading.appendChild(element('p', '',
        'This verified sequence has no identified equipment-linked child group. Each source edits only its verified class/body child.'));
      section.appendChild(heading);
      return section;
    }
    var selectedChild = weaponChildForAnimation(ui, animation);
    heading.appendChild(element('p', '',
      'This sequence stores ' + source.sprite.childCount +
      ' physical weapon-group children in each verified weapon source. The game chooses a child from equipped-item data. This picker previews one child across the complete sequence.' +
      retailMappingText(animation, source.sprite.childCount)));
    section.appendChild(heading);
    var strip = element('div', 'animation-weapon-strip');
    strip.setAttribute('data-art-scroll-key', 'animations:weapons:' + animation.key);
    source.editableChildOrdinals.forEach(function(childOrdinal) {
      var card = element('button', 'animation-weapon-card' +
        (childOrdinal === selectedChild ? ' selected' : ''));
      card.type = 'button';
      card.appendChild(element('strong', '', 'Weapon sprite ' + (childOrdinal + 1)));
      var canvas = element('canvas', 'animation-weapon-thumbnail');
      paintPixels(canvas, source.sprite.width, source.sprite.height,
        childPixels(source, state.animations, childOrdinal), 4);
      card.appendChild(canvas);
      var child = source.sprite.children[childOrdinal];
      card.appendChild(element('small', '', 'child ' + childOrdinal +
        ' · disc ' + M.hex(child.discriminator, 2)));
      if ((animation.spec.retailMappedWeaponOrdinals || [])
          .indexOf(childOrdinal) < 0) {
        var original = source.originalChildren[childOrdinal];
        var originalVisible = Array.prototype.some.call(
          original.intensity, function(value) { return value > 0; });
        card.appendChild(element('small', 'animation-weapon-unmapped',
          originalVisible
            ? 'No known retail item mapping'
            : 'Unused by retail items; empty in ROM'));
      }
      if (M.hasEdit(state.animations, source.key, childOrdinal)) {
        card.appendChild(badge('Edited', 'edited'));
      }
      card.addEventListener('click', function() {
        setWeaponChild(ui, animation, childOrdinal);
        selectLayer(state, animation, frame, layer, ui);
        rerender();
      });
      strip.appendChild(card);
    });
    section.appendChild(strip);
    return section;
  }

  function layerList(state, animation, frame, selected, ui, rerender) {
    var section = element('section', 'animation-layer-section');
    section.appendChild(element('h3', '', 'Frame layers'));
    section.appendChild(element('p', 'animation-help',
      'Choose one source object. Gold is editable; every other layer remains shaded context.'));
    var list = element('div', 'animation-layer-list');
    list.setAttribute('data-art-scroll-key', 'animations:layers:' +
      animation.key + ':' + frame.sequenceIndex);
    frame.layers.forEach(function(layer) {
      var source = animation.artByKey[layer.sourceKey];
      var card = element('button', 'animation-layer-card' +
        (layer.ordinal === selected.ordinal ? ' selected' : ''));
      card.type = 'button';
      var title = element('span', 'animation-layer-title');
      title.appendChild(element('strong', '', 'Layer ' + (layer.ordinal + 1) +
        ' · art ' + M.hex(layer.artId, 2)));
      var editedChildren = M.sourceEditCount(state.animations, layer.sourceKey);
      if (editedChildren) title.appendChild(badge(editedChildren + ' sprite' +
        (editedChildren === 1 ? '' : 's') + ' edited', 'edited'));
      card.appendChild(title);
      card.appendChild(element('small', '', layerKind(source)));
      card.appendChild(element('small', '', source.weaponSelectable
        ? 'Choose any of ' + source.sprite.childCount + ' weapon sprites above'
        : 'Editable child ' + source.childOrdinal + ' of ' +
          source.sprite.childCount + '; every other child is preserved'));
      var usageFrames = source.usageFramesByAnimation[animation.key] || [];
      card.appendChild(element('small', '', layer.width + '×' + layer.height +
        ' · lookup bank ' + layer.lookupBank + ' · used in frame' +
        (usageFrames.length === 1 ? ' ' : 's ') +
        usageFrames.map(function(value) { return value + 1; }).join(', ')));
      card.addEventListener('click', function() {
        selectLayer(state, animation, frame, layer, ui); rerender();
      });
      list.appendChild(card);
    });
    section.appendChild(list);
    return section;
  }

  function flood(indices, intensity, width, start, newIndex, newIntensity) {
    var oldIndex = indices[start], oldIntensity = intensity[start];
    if (oldIndex === newIndex && oldIntensity === newIntensity) return false;
    var queue = [start], seen = new Uint8Array(indices.length); seen[start] = 1;
    while (queue.length) {
      var pixel = queue.pop();
      if (indices[pixel] !== oldIndex || intensity[pixel] !== oldIntensity) continue;
      indices[pixel] = newIndex; intensity[pixel] = newIntensity;
      var x = pixel % width, neighbors = [pixel - width, pixel + width];
      if (x) neighbors.push(pixel - 1);
      if (x + 1 < width) neighbors.push(pixel + 1);
      neighbors.forEach(function(next) {
        if (next >= 0 && next < indices.length && !seen[next]) {
          seen[next] = 1; queue.push(next);
        }
      });
    }
    return true;
  }

  function nativeCoordinate(canvas, event, animation) {
    var rect = canvas.getBoundingClientRect();
    return {
      x: Math.max(0, Math.min(animation.canvas.width - 1,
        Math.floor((event.clientX - rect.left) * animation.canvas.width / rect.width))),
      y: Math.max(0, Math.min(animation.canvas.height - 1,
        Math.floor((event.clientY - rect.top) * animation.canvas.height / rect.height)))
    };
  }

  function localCoordinate(global, animation, layer) {
    var layerLeft = layer.drawOffsetX - animation.canvas.originX;
    var layerTop = layer.drawOffsetY - animation.canvas.originY;
    var x = global.x - layerLeft, y = global.y - layerTop;
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return null;
    return { x: x, y: y, index: y * layer.width + x };
  }

  function hitLayer(state, animation, frame, global, ui) {
    for (var ordinal = frame.layers.length - 1; ordinal >= 0; ordinal--) {
      var layer = frame.layers[ordinal], local = localCoordinate(global, animation, layer);
      if (!local) continue;
      var source = animation.artByKey[layer.sourceKey];
      var childOrdinal = selectedChildOrdinal(source, ui);
      if (M.currentEdit(state.animations, layer.sourceKey, childOrdinal)
          .intensity[local.index]) return layer;
    }
    return null;
  }

  function installEditing(canvas, state, animation, frame, layer, ui, options, rerender) {
    var drawing = false, working = null, changedPixels = false;
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = selectedChildOrdinal(source, ui);
    canvas.tabIndex = 0;
    function redraw() {
      var overrides = {};
      if (working) overrides[M.childKey(layer.sourceKey, childOrdinal)] = working;
      drawFrame(canvas, animation, frame, state.animations, 8, layer, overrides, true,
        weaponChildForAnimation(ui, animation));
    }
    function selectedIntensity() {
      return ui.animationTool === 'eraser' ? 0 : ui.animationIntensity;
    }
    function applyPoint(local) {
      if (ui.animationTool !== 'pencil' && ui.animationTool !== 'eraser') return;
      var wantedIndex = ui.animationPaletteIndex, wantedIntensity = selectedIntensity();
      if (working.indices[local.index] !== wantedIndex ||
          working.intensity[local.index] !== wantedIntensity) {
        working.indices[local.index] = wantedIndex;
        working.intensity[local.index] = wantedIntensity;
        changedPixels = true;
      }
    }
    canvas.addEventListener('pointerdown', function(event) {
      canvas.focus();
      var global = nativeCoordinate(canvas, event, animation);
      var local = localCoordinate(global, animation, layer);
      if (!local) {
        var hit = hitLayer(state, animation, frame, global, ui);
        if (hit) { selectLayer(state, animation, frame, hit, ui); rerender(); }
        return;
      }
      var current = M.currentEdit(state.animations, layer.sourceKey, childOrdinal);
      if (ui.animationTool === 'eyedropper') {
        samplePixel(ui, current, local.index);
        rerender(); return;
      }
      if (ui.animationTool === 'fill' || ui.animationTool === 'replace') {
        var indices = current.indices.slice(), intensity = current.intensity.slice();
        var didChange = false;
        if (ui.animationTool === 'fill') {
          didChange = flood(indices, intensity, source.sprite.width, local.index,
            ui.animationPaletteIndex, ui.animationIntensity);
        } else {
          var oldIndex = indices[local.index];
          if (oldIndex !== ui.animationPaletteIndex) {
            for (var pixel = 0; pixel < indices.length; pixel++) {
              if (indices[pixel] === oldIndex) indices[pixel] = ui.animationPaletteIndex;
            }
            didChange = true;
          }
        }
        if (didChange && M.setEdit(state.animations, layer.sourceKey, childOrdinal,
            indices, intensity)) {
          changed(options); notify(options, 'Updated ' + animationLabel(animation) + ' art ' +
            M.hex(layer.artId, 2) + ' child ' + childOrdinal + '.'); rerender();
        }
        return;
      }
      drawing = true; changedPixels = false;
      working = { indices: current.indices.slice(), intensity: current.intensity.slice() };
      canvas.setPointerCapture(event.pointerId); applyPoint(local); redraw();
    });
    canvas.addEventListener('pointermove', function(event) {
      if (!drawing) return;
      var local = localCoordinate(nativeCoordinate(canvas, event, animation), animation, layer);
      if (!local) return;
      applyPoint(local); redraw();
    });
    function finish(event) {
      if (!drawing) return;
      drawing = false;
      if (event && canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
      if (changedPixels && M.setEdit(state.animations, layer.sourceKey, childOrdinal,
          working.indices, working.intensity)) {
        changed(options); notify(options, 'Updated ' + animationLabel(animation) + ' art ' +
          M.hex(layer.artId, 2) + ' child ' + childOrdinal + '.'); rerender();
      }
      working = null;
    }
    canvas.addEventListener('pointerup', finish);
    canvas.addEventListener('pointercancel', finish);
    canvas.addEventListener('keydown', function(event) {
      if (!(event.ctrlKey || event.metaKey)) return;
      var name = event.key.toLowerCase();
      if (name === 'z' && !event.shiftKey) {
        event.preventDefault(); if (M.undo(state.animations, layer.sourceKey,
            childOrdinal)) {
          changed(options); rerender();
        }
      } else if (name === 'y' || (name === 'z' && event.shiftKey)) {
        event.preventDefault(); if (M.redo(state.animations, layer.sourceKey,
            childOrdinal)) {
          changed(options); rerender();
        }
      }
    });
  }

  function toolbar(state, layer, ui, options, rerender) {
    var bar = element('div', 'art-toolbox animation-toolbox');
    [['pencil', 'Pencil'], ['eraser', 'Erase'], ['fill', 'Fill'],
      ['eyedropper', 'Eyedropper'], ['replace', 'Replace Color']].forEach(function(row) {
      bar.appendChild(button(row[1], ui.animationTool === row[0] ? 'active' : '', function() {
        ui.animationTool = row[0]; rerender();
      }));
    });
    var source = state.animations.artByKey[layer.sourceKey];
    var childOrdinal = selectedChildOrdinal(source, ui);
    var history = M.historyFor(state.animations, layer.sourceKey, childOrdinal);
    var undo = button('Undo', 'btn-secondary', function() {
      if (M.undo(state.animations, layer.sourceKey, childOrdinal)) {
        changed(options); rerender();
      }
    });
    undo.disabled = !history.undo.length; bar.appendChild(undo);
    var redo = button('Redo', 'btn-secondary', function() {
      if (M.redo(state.animations, layer.sourceKey, childOrdinal)) {
        changed(options); rerender();
      }
    });
    redo.disabled = !history.redo.length; bar.appendChild(redo);
    return bar;
  }

  function palettePanel(source, ui, rerender) {
    var panel = element('section', 'animation-palette-panel');
    panel.appendChild(element('h3', '', 'Verified lookup colors'));
    panel.appendChild(element('p', 'animation-help',
      'These are the exact 256 entries accepted by this source object’s lookup bank. Palette editing is locked.'));
    var selectedWord = source.palette[ui.animationPaletteIndex];
    var selected = element('div', 'animation-selected-color');
    var swatch = element('span', 'art-selected-swatch');
    selected.appendChild(swatch);
    selected.appendChild(element('span', '', 'Index ' + ui.animationPaletteIndex +
      ' · RGB555 ' + M.hex(selectedWord, 4)));
    panel.appendChild(selected);
    var grid = element('div', 'animation-palette-grid');
    var paletteChoices = [];
    var orderedIndices = paletteHueOrder(source.palette);
    for (var index = 0; index < orderedIndices.length; index++) {
      (function(paletteIndex) {
        var word = source.palette[paletteIndex];
        var choice = element('button', 'animation-palette-swatch' +
          (paletteIndex === ui.animationPaletteIndex ? ' selected' : ''));
        choice.type = 'button';
        choice.setAttribute('data-palette-index', String(paletteIndex));
        choice.setAttribute('title', 'Index ' + paletteIndex + ' · RGB555 ' + M.hex(word, 4));
        choice.setAttribute('aria-label', 'Palette index ' + paletteIndex +
          ', RGB555 ' + M.hex(word, 4));
        choice.addEventListener('click', function() {
          ui.animationPaletteIndex = paletteIndex; ui.animationTool = 'pencil'; rerender();
        });
        paletteChoices.push({ node: choice, word: word });
        grid.appendChild(choice);
      })(orderedIndices[index]);
    }
    panel.appendChild(grid);
    var intensity = element('label', 'animation-intensity-control');
    var intensityText = element('span', '', 'Visibility / intensity: ' +
      ui.animationIntensity + ' / 15');
    intensity.appendChild(intensityText);
    var slider = element('input'); slider.type = 'range'; slider.min = '0'; slider.max = '15';
    slider.step = '1'; slider.value = String(ui.animationIntensity);
    function paintIntensity() {
      swatch.style.background = intensityColorCss(selectedWord, ui.animationIntensity);
      paletteChoices.forEach(function(entry) {
        entry.node.style.background = intensityColorCss(entry.word, ui.animationIntensity);
      });
    }
    slider.addEventListener('input', function() {
      ui.animationIntensity = Number(slider.value);
      intensityText.textContent = 'Visibility / intensity: ' + ui.animationIntensity + ' / 15';
      paintIntensity();
    });
    paintIntensity();
    intensity.appendChild(slider); panel.appendChild(intensity);
    panel.appendChild(element('p', 'animation-help',
      'The game stores visibility in a separate four-bit plane. Zero is transparent; 15 is fully visible.'));
    return panel;
  }

  function downloadFrame(animation, frame, state, weaponChildOrdinal) {
    var canvas = element('canvas');
    drawFrame(canvas, animation, frame, state.animations, 1, null, null, false,
      weaponChildOrdinal);
    canvas.toBlob(function(blob) {
      if (!blob) return;
      var url = URL.createObjectURL(blob), anchor = document.createElement('a');
      anchor.href = url; anchor.download = animation.key + '-frame-' +
        String(frame.sequenceIndex + 1).padStart(2, '0') + '.png';
      document.body.appendChild(anchor); anchor.click(); anchor.remove();
      setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
    }, 'image/png');
  }

  function editor(state, animation, frame, layer, ui, options, rerender) {
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = selectedChildOrdinal(source, ui);
    var section = element('section', 'animation-editor-section');
    var heading = element('div', 'art-editor-heading');
    var copy = element('div');
    copy.appendChild(element('h3', '', animation.spec.className + ' · ' +
      animation.spec.actionName + ' · frame ' + (frame.sequenceIndex + 1)));
    copy.appendChild(element('p', '', frame.ticks + ' ticks · token ' +
      M.hex(frame.token, 2) + ' · layer ' + (layer.ordinal + 1) +
      ' / ' + frame.layers.length + ' · child ' + childOrdinal +
      ' of ' + source.sprite.childCount));
    heading.appendChild(copy);
    if (M.hasEdit(state.animations, layer.sourceKey, childOrdinal)) {
      heading.appendChild(badge('Edited', 'edited'));
    }
    section.appendChild(heading);
    section.appendChild(element('div', 'animation-lock-note',
      source.weaponSelectable
        ? 'The selected weapon child’s pixel indices and four-bit visibility are editable. Every other child is preserved. Frame timing, offsets, art references, and lookup palettes are locked.'
        : 'Bounded phase: child ' + source.childOrdinal + ' pixel indices and four-bit visibility are editable. Every other child is preserved. Frame timing, offsets, art references, lookup palettes, and non-weapon child selection are locked.'));
    section.appendChild(toolbar(state, layer, ui, options, rerender));
    var canvas = element('canvas', 'animation-edit-canvas');
    drawFrame(canvas, animation, frame, state.animations, 8, layer, null, true,
      weaponChildForAnimation(ui, animation));
    installEditing(canvas, state, animation, frame, layer, ui, options, rerender);
    section.appendChild(canvas);
    var legend = element('div', 'animation-edit-legend');
    legend.appendChild(element('span', 'animation-legend-editable', 'Gold outline / full color: editable source'));
    legend.appendChild(element('span', 'animation-legend-context', 'Gray shading: frame context only'));
    section.appendChild(legend);
    var actual = element('figure', 'animation-actual-preview');
    actual.appendChild(element('figcaption', '', 'Current composed frame · inspection blend'));
    var actualCanvas = element('canvas');
    drawFrame(actualCanvas, animation, frame, state.animations, 4, null, null,
      false, weaponChildForAnimation(ui, animation));
    actual.appendChild(actualCanvas); section.appendChild(actual);
    var actions = element('div', 'art-asset-actions animation-actions');
    var reset = button(source.weaponSelectable
      ? 'Reset Selected Weapon Sprite' : 'Reset Selected Source',
    'btn-secondary', function() {
      if (!M.hasEdit(state.animations, layer.sourceKey, childOrdinal)) return;
      var original = M.originalChild(source, childOrdinal);
      M.setEdit(state.animations, layer.sourceKey, childOrdinal,
        original.indices, original.intensity);
      changed(options); notify(options, 'Restored ' + animationLabel(animation) + ' art ' +
        M.hex(layer.artId, 2) + ' child ' + childOrdinal +
        ' to the vanilla ROM.'); rerender();
    });
    reset.disabled = !M.hasEdit(state.animations, layer.sourceKey, childOrdinal);
    actions.appendChild(reset);
    actions.appendChild(button('Export Current Frame PNG', 'btn-secondary', function() {
      downloadFrame(animation, frame, state, weaponChildForAnimation(ui, animation));
    }));
    section.appendChild(actions);
    return section;
  }

  function render(state, ui, options, rerender) {
    var root = element('main', 'animation-editor-root');
    if (!state.animations || !state.animations.supported) {
      root.appendChild(element('h3', '', 'Combat Animation'));
      root.appendChild(element('p', '', state.animations
        ? state.animations.unavailableReason
        : 'Combat sprite state is unavailable.'));
      return root;
    }
    if (!ui.animationTool) ui.animationTool = 'pencil';
    var animation = selectedAnimation(state, ui);
    var frame = selectedFrame(animation, ui);
    var layer = selectedLayer(frame, ui);
    if (!Number.isInteger(ui.animationPaletteIndex) || ui.animationPaletteIndex < 0 ||
        ui.animationPaletteIndex > 255) ui.animationPaletteIndex = 0;
    ui.animationIntensity = normalizeIntensity(ui.animationIntensity);
    weaponChildForAnimation(ui, animation);

    root.appendChild(animationPicker(state, animation, ui, rerender));
    var stats = animationStats(animation);
    var intro = element('div', 'animation-intro');
    intro.appendChild(element('h3', '', animationLabel(animation) + ' · verified sequence'));
    intro.appendChild(element('p', '',
      stats.frames + ' verified frames and ' + stats.layers +
      ' layer occurrences are available. ' + stats.sources +
      ' distinct source objects can be edited. ' + (stats.weaponSources
        ? stats.weaponSources + ' weapon sources expose all ' +
          animation.spec.weaponChildCount + ' physical weapon-group children independently.' +
          retailMappingText(animation, animation.spec.weaponChildCount)
        : 'No equipment-linked child group has been verified for this sequence.') +
      ' Export rebuilds unique fitting objects in place and relocates overflow copy-on-write.'));
    intro.appendChild(element('p', '', 'Known consumers of this shared descriptor: ' +
      animation.spec.consumerSummary +
      '. Edits affect every class that resolves to this descriptor.'));
    intro.appendChild(element('p', 'animation-warning',
      'This is an inspection composite, not a cycle-exact Nintendo 64 blend. Other pose variants and cross-family weapon remapping remain outside this phase.'));
    root.appendChild(intro);
    root.appendChild(sequenceStrip(state, animation, ui, rerender));
    root.appendChild(weaponPicker(state, animation, frame, layer, ui, rerender));
    var workspace = element('div', 'animation-workspace');
    workspace.appendChild(editor(state, animation, frame, layer, ui, options, rerender));
    var sidebar = element('aside', 'animation-sidebar');
    sidebar.appendChild(layerList(state, animation, frame, layer, ui, rerender));
    sidebar.appendChild(palettePanel(animation.artByKey[layer.sourceKey], ui, rerender));
    workspace.appendChild(sidebar); root.appendChild(workspace);
    return root;
  }

  OB64.animationUI = {
    render: render,
    wordRgb: wordRgb,
    normalizeIntensity: normalizeIntensity,
    intensityColorCss: intensityColorCss,
    paletteHueOrder: paletteHueOrder,
    sourceOver: sourceOver,
    selectLayer: selectLayer,
    samplePixel: samplePixel,
    animationStats: animationStats,
    weaponChildForAnimation: weaponChildForAnimation,
    setWeaponChild: setWeaponChild,
    childOrdinalForSource: childOrdinalForSource,
    childPixels: childPixels,
    weaponSourceForFrame: weaponSourceForFrame,
    framePixels: framePixels,
    drawFrame: drawFrame,
    localCoordinate: localCoordinate,
    flood: flood
  };
})();
