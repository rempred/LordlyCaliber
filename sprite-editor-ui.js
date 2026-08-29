// LordlyCaliber - Sprite Editor tab UI

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var L = OB64.spriteLibrary;

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

  function notify(options, message) {
    if (options && typeof options.onStatus === 'function') options.onStatus(message);
  }

  function changed(options) {
    if (options && typeof options.onChange === 'function') options.onChange();
  }

  function plainGameLabel(value) {
    var text = String(value == null ? '' : value);
    if (typeof OB64.romNameText === 'function') text = OB64.romNameText(text);
    text = text.replace(/\{0E\}|\{0F\}/g, '').replace(/\{10\}./g, ' ');
    return text.replace(/\x10./g, ' ').replace(/[\x00-\x1F\x7F]/g, '')
      .replace(/\s+/g, ' ').trim();
  }

  function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
  }

  function ensureUi(state) {
    L.indexState(state);
    var ui = state.ui;
    if (!ui.scroll) ui.scroll = {};
    if (!Number.isInteger(ui.frameIndex)) ui.frameIndex = 0;
    if (!Number.isInteger(ui.layerIndex)) ui.layerIndex = 0;
    if (!Number.isInteger(ui.brushSize)) ui.brushSize = 1;
    if (!Number.isInteger(ui.zoom)) ui.zoom = 10;
    if (!Array.isArray(ui.color) || ui.color.length !== 4) {
      ui.color = [255, 255, 255, 255];
    }
    if (ui.background !== 'white') ui.background = 'checkerboard';
    return ui;
  }

  function selectedAsset(state, ui) {
    var asset = state.byId[ui.assetId];
    if (!asset && state.assets.length) {
      asset = state.assets[0];
      ui.assetId = asset.id;
    }
    if (!asset) return null;
    ui.frameIndex = clamp(ui.frameIndex, 0, asset.frames.length - 1);
    var frame = asset.frames[ui.frameIndex];
    ui.layerIndex = clamp(ui.layerIndex, 0, frame.layers.length - 1);
    return asset;
  }

  function captureScroll(panel, ui, includeViewport) {
    if (!panel || !ui) return;
    var rows = panel.querySelectorAll
      ? Array.prototype.slice.call(panel.querySelectorAll('[data-sprite-scroll-key]'))
      : [];
    if (includeViewport && panel.closest) {
      var viewport = panel.closest('.content');
      if (viewport) {
        if (!viewport.getAttribute('data-sprite-scroll-key')) {
          viewport.setAttribute('data-sprite-scroll-key', 'sprite:viewport');
        }
        rows.unshift(viewport);
      }
    }
    rows.forEach(function(row) {
      var key = row.getAttribute('data-sprite-scroll-key');
      if (key) ui.scroll[key] = {
        top: Number(row.scrollTop) || 0,
        left: Number(row.scrollLeft) || 0
      };
    });
  }

  function restoreScroll(panel, ui, includeViewport) {
    if (!panel || !ui || !ui.scroll) return;
    var rows = panel.querySelectorAll
      ? Array.prototype.slice.call(panel.querySelectorAll('[data-sprite-scroll-key]'))
      : [];
    if (includeViewport && panel.closest) {
      var viewport = panel.closest('.content');
      if (viewport) rows.unshift(viewport);
    }
    rows.forEach(function(row) {
      var key = row.getAttribute('data-sprite-scroll-key');
      var saved = key && ui.scroll[key];
      if (!saved) return;
      row.scrollTop = saved.top;
      row.scrollLeft = saved.left;
    });
  }

  function sourceOver(target, source) {
    return L.sourceOver(target, source);
  }

  function checker(context, width, height, scale) {
    context.fillStyle = '#d8d2bc';
    context.fillRect(0, 0, width * scale, height * scale);
    context.fillStyle = '#8d887a';
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        if ((x + y) & 1) context.fillRect(x * scale, y * scale, scale, scale);
      }
    }
  }

  function paintPixels(canvas, width, height, pixels, scale, background, grid,
      selection) {
    scale = Math.max(1, Number(scale) || 1);
    canvas.width = width * scale;
    canvas.height = height * scale;
    var context = canvas.getContext('2d');
    context.imageSmoothingEnabled = false;
    if (background === 'white') {
      context.fillStyle = '#fff';
      context.fillRect(0, 0, canvas.width, canvas.height);
    } else {
      checker(context, width, height, scale);
    }
    var source = document.createElement('canvas');
    source.width = width;
    source.height = height;
    var sourceContext = source.getContext('2d', { alpha: true });
    var image = sourceContext.createImageData(width, height);
    image.data.set(pixels);
    sourceContext.putImageData(image, 0, 0);
    context.drawImage(source, 0, 0, canvas.width, canvas.height);
    if (grid && scale >= 6) {
      context.save();
      context.strokeStyle = 'rgba(55, 37, 16, .18)';
      context.lineWidth = 1;
      for (var gridX = 1; gridX < width; gridX++) {
        context.beginPath();
        context.moveTo(gridX * scale + 0.5, 0);
        context.lineTo(gridX * scale + 0.5, canvas.height);
        context.stroke();
      }
      for (var gridY = 1; gridY < height; gridY++) {
        context.beginPath();
        context.moveTo(0, gridY * scale + 0.5);
        context.lineTo(canvas.width, gridY * scale + 0.5);
        context.stroke();
      }
      context.restore();
    }
    if (selection) {
      context.save();
      context.strokeStyle = '#ffd45c';
      context.lineWidth = 2;
      context.setLineDash([5, 3]);
      context.strokeRect(selection.x * scale + 1, selection.y * scale + 1,
        selection.width * scale - 2, selection.height * scale - 2);
      context.restore();
    }
  }

  function compositeWithLayer(asset, frameIndex, layerIndex, working) {
    var frame = asset.frames[frameIndex];
    var output = new Uint8ClampedArray(asset.width * asset.height * 4);
    frame.layers.forEach(function(layer, index) {
      if (layer.visible === false) return;
      var pixels = index === layerIndex && working ? working : layer.pixels;
      for (var offset = 0; offset < output.length; offset += 4) {
        if (!pixels[offset + 3]) continue;
        var blended = sourceOver(
          [output[offset], output[offset + 1], output[offset + 2], output[offset + 3]],
          [pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]]);
        output[offset] = blended[0];
        output[offset + 1] = blended[1];
        output[offset + 2] = blended[2];
        output[offset + 3] = blended[3];
      }
    });
    return output;
  }

  function coordinate(canvas, event, width, height) {
    var rect = canvas.getBoundingClientRect();
    return {
      x: clamp(Math.floor((event.clientX - rect.left) * width / rect.width), 0, width - 1),
      y: clamp(Math.floor((event.clientY - rect.top) * height / rect.height), 0, height - 1)
    };
  }

  function sameColor(pixels, offset, color) {
    return pixels[offset] === color[0] && pixels[offset + 1] === color[1] &&
      pixels[offset + 2] === color[2] && pixels[offset + 3] === color[3];
  }

  function setColor(pixels, offset, color) {
    pixels[offset] = color[0];
    pixels[offset + 1] = color[1];
    pixels[offset + 2] = color[2];
    pixels[offset + 3] = color[3];
  }

  function flood(pixels, width, height, startX, startY, color) {
    var start = startY * width + startX;
    var sourceOffset = start * 4;
    var original = [pixels[sourceOffset], pixels[sourceOffset + 1],
      pixels[sourceOffset + 2], pixels[sourceOffset + 3]];
    if (original.join(',') === color.join(',')) return false;
    var queue = [start];
    var seen = new Uint8Array(width * height);
    seen[start] = 1;
    var changedAny = false;
    while (queue.length) {
      var index = queue.pop();
      var offset = index * 4;
      if (!sameColor(pixels, offset, original)) continue;
      setColor(pixels, offset, color);
      changedAny = true;
      var x = index % width;
      var rows = [index - width, index + width];
      if (x) rows.push(index - 1);
      if (x + 1 < width) rows.push(index + 1);
      rows.forEach(function(next) {
        if (next >= 0 && next < width * height && !seen[next]) {
          seen[next] = 1;
          queue.push(next);
        }
      });
    }
    return changedAny;
  }

  function applyBrush(pixels, width, height, point, brushSize, color) {
    var radiusBefore = Math.floor((brushSize - 1) / 2);
    var radiusAfter = brushSize - radiusBefore - 1;
    var changedAny = false;
    for (var y = point.y - radiusBefore; y <= point.y + radiusAfter; y++) {
      for (var x = point.x - radiusBefore; x <= point.x + radiusAfter; x++) {
        if (x < 0 || y < 0 || x >= width || y >= height) continue;
        var offset = (y * width + x) * 4;
        if (!sameColor(pixels, offset, color)) {
          setColor(pixels, offset, color);
          changedAny = true;
        }
      }
    }
    return changedAny;
  }

  function copySelection(layer, asset, selection) {
    var output = new Uint8ClampedArray(selection.width * selection.height * 4);
    for (var y = 0; y < selection.height; y++) {
      for (var x = 0; x < selection.width; x++) {
        var sourceOffset = ((selection.y + y) * asset.width + selection.x + x) * 4;
        output.set(layer.pixels.subarray(sourceOffset, sourceOffset + 4),
          (y * selection.width + x) * 4);
      }
    }
    return { width: selection.width, height: selection.height, rgba: output };
  }

  function installCanvasEditing(canvas, state, asset, ui, options, rerender) {
    var drawing = false;
    var working = null;
    var changedPixels = false;
    var selectionStart = null;
    var frameIndex = ui.frameIndex;
    var layerIndex = ui.layerIndex;
    var layer = asset.frames[frameIndex].layers[layerIndex];
    canvas.tabIndex = 0;
    canvas.setAttribute('aria-label', 'Editable sprite canvas');

    function drawWorking() {
      paintPixels(canvas, asset.width, asset.height,
        compositeWithLayer(asset, frameIndex, layerIndex, working),
        ui.zoom, ui.background, ui.showGrid, ui.selection);
    }

    function pointAction(point) {
      var offset = (point.y * asset.width + point.x) * 4;
      if (ui.tool === 'eyedropper') {
        ui.color = [layer.pixels[offset], layer.pixels[offset + 1],
          layer.pixels[offset + 2], layer.pixels[offset + 3]];
        ui.tool = 'pencil';
        rerender();
        return 'done';
      }
      working = new Uint8ClampedArray(layer.pixels);
      if (ui.tool === 'fill') {
        changedPixels = flood(working, asset.width, asset.height,
          point.x, point.y, ui.color);
        return 'instant';
      }
      if (ui.tool === 'replace') {
        var sourceColor = [working[offset], working[offset + 1],
          working[offset + 2], working[offset + 3]];
        for (var pixelOffset = 0; pixelOffset < working.length; pixelOffset += 4) {
          if (sameColor(working, pixelOffset, sourceColor) &&
              !sameColor(working, pixelOffset, ui.color)) {
            setColor(working, pixelOffset, ui.color);
            changedPixels = true;
          }
        }
        return 'instant';
      }
      if (ui.tool === 'select') {
        selectionStart = point;
        ui.selection = { x: point.x, y: point.y, width: 1, height: 1 };
        return 'select';
      }
      var color = ui.tool === 'eraser' ? [0, 0, 0, 0] : ui.color;
      changedPixels = applyBrush(working, asset.width, asset.height,
        point, ui.brushSize, color);
      return 'draw';
    }

    canvas.addEventListener('pointerdown', function(event) {
      canvas.focus();
      var point = coordinate(canvas, event, asset.width, asset.height);
      changedPixels = false;
      var mode = pointAction(point);
      if (mode === 'done') return;
      drawing = true;
      canvas.setPointerCapture(event.pointerId);
      drawWorking();
      if (mode === 'instant') finish(event);
    });
    canvas.addEventListener('pointermove', function(event) {
      if (!drawing) return;
      var point = coordinate(canvas, event, asset.width, asset.height);
      if (ui.tool === 'select') {
        var left = Math.min(selectionStart.x, point.x);
        var top = Math.min(selectionStart.y, point.y);
        ui.selection = {
          x: left,
          y: top,
          width: Math.max(selectionStart.x, point.x) - left + 1,
          height: Math.max(selectionStart.y, point.y) - top + 1
        };
      } else if (ui.tool === 'pencil' || ui.tool === 'eraser') {
        var color = ui.tool === 'eraser' ? [0, 0, 0, 0] : ui.color;
        changedPixels = applyBrush(working, asset.width, asset.height,
          point, ui.brushSize, color) || changedPixels;
      }
      drawWorking();
    });
    function finish(event) {
      if (!drawing) return;
      drawing = false;
      if (event && canvas.hasPointerCapture(event.pointerId)) {
        canvas.releasePointerCapture(event.pointerId);
      }
      if (ui.tool === 'select') {
        rerender();
        return;
      }
      if (changedPixels) {
        L.replaceLayerPixels(state, asset.id, frameIndex, layerIndex, working);
        changed(options);
        notify(options, 'Sprite pixels changed. Save Project to keep this asset.');
        rerender();
      }
    }
    canvas.addEventListener('pointerup', finish);
    canvas.addEventListener('pointercancel', finish);
    canvas.addEventListener('keydown', function(event) {
      if (!(event.ctrlKey || event.metaKey)) return;
      var key = event.key.toLowerCase();
      if (key === 'z' && !event.shiftKey) {
        event.preventDefault();
        if (L.undo(state, asset.id)) { changed(options); rerender(); }
      } else if (key === 'y' || (key === 'z' && event.shiftKey)) {
        event.preventDefault();
        if (L.redo(state, asset.id)) { changed(options); rerender(); }
      } else if (key === 'c' && ui.selection) {
        event.preventDefault();
        ui.clipboard = copySelection(layer, asset, ui.selection);
        notify(options, 'Copied the selected sprite pixels.');
      } else if (key === 'v' && ui.selection && ui.clipboard) {
        event.preventDefault();
        var pasted = new Uint8ClampedArray(layer.pixels);
        for (var y = 0; y < ui.clipboard.height; y++) {
          for (var x = 0; x < ui.clipboard.width; x++) {
            var targetX = ui.selection.x + x;
            var targetY = ui.selection.y + y;
            if (targetX >= asset.width || targetY >= asset.height) continue;
            var sourceOffset = (y * ui.clipboard.width + x) * 4;
            pasted.set(ui.clipboard.rgba.subarray(sourceOffset, sourceOffset + 4),
              (targetY * asset.width + targetX) * 4);
          }
        }
        L.replaceLayerPixels(state, asset.id, frameIndex, layerIndex, pasted);
        changed(options);
        rerender();
      }
    });
  }

  function rgbHex(color) {
    return '#' + color.slice(0, 3).map(function(value) {
      return clamp(Number(value) || 0, 0, 255).toString(16).padStart(2, '0');
    }).join('');
  }

  function parseRgbHex(value) {
    var match = String(value || '').match(/^#([0-9a-f]{6})$/i);
    if (!match) return [255, 255, 255];
    return [parseInt(match[1].slice(0, 2), 16),
      parseInt(match[1].slice(2, 4), 16),
      parseInt(match[1].slice(4, 6), 16)];
  }

  function toolButton(name, label, ui, rerender) {
    var node = button(label, ui.tool === name ? 'active' : '', function() {
      ui.tool = name;
      rerender();
    });
    node.setAttribute('aria-pressed', ui.tool === name ? 'true' : 'false');
    return node;
  }

  function toolbar(state, asset, ui, options, rerender) {
    var bar = element('div', 'sprite-toolbox');
    [['pencil', 'Pencil'], ['eraser', 'Eraser'], ['fill', 'Fill'],
      ['eyedropper', 'Eyedropper'], ['replace', 'Replace Color'],
      ['select', 'Select']].forEach(function(row) {
      bar.appendChild(toolButton(row[0], row[1], ui, rerender));
    });

    var brush = element('label', 'sprite-compact-control');
    brush.appendChild(element('span', '', 'Brush'));
    var brushSelect = element('select');
    [1, 2, 3, 4, 6, 8, 12, 16].forEach(function(size) {
      var option = element('option', '', size + ' px');
      option.value = String(size);
      brushSelect.appendChild(option);
    });
    brushSelect.value = String(ui.brushSize);
    brushSelect.addEventListener('change', function() {
      ui.brushSize = Number(brushSelect.value);
      rerender();
    });
    brush.appendChild(brushSelect);
    bar.appendChild(brush);

    var history = L.historyFor(state, asset.id);
    var undo = button('Undo', 'btn-secondary', function() {
      if (L.undo(state, asset.id)) { changed(options); rerender(); }
    });
    undo.disabled = !history.undo.length;
    bar.appendChild(undo);
    var redo = button('Redo', 'btn-secondary', function() {
      if (L.redo(state, asset.id)) { changed(options); rerender(); }
    });
    redo.disabled = !history.redo.length;
    bar.appendChild(redo);

    var copy = button('Copy', 'btn-secondary', function() {
      if (!ui.selection) return;
      ui.clipboard = copySelection(
        asset.frames[ui.frameIndex].layers[ui.layerIndex], asset, ui.selection);
      notify(options, 'Copied the selected sprite pixels.');
      rerender();
    });
    copy.disabled = !ui.selection;
    bar.appendChild(copy);
    var paste = button('Paste', 'btn-secondary', function() {
      if (!ui.selection || !ui.clipboard) return;
      var layer = asset.frames[ui.frameIndex].layers[ui.layerIndex];
      var output = new Uint8ClampedArray(layer.pixels);
      for (var y = 0; y < ui.clipboard.height; y++) {
        for (var x = 0; x < ui.clipboard.width; x++) {
          var targetX = ui.selection.x + x;
          var targetY = ui.selection.y + y;
          if (targetX >= asset.width || targetY >= asset.height) continue;
          var sourceOffset = (y * ui.clipboard.width + x) * 4;
          output.set(ui.clipboard.rgba.subarray(sourceOffset, sourceOffset + 4),
            (targetY * asset.width + targetX) * 4);
        }
      }
      L.replaceLayerPixels(state, asset.id, ui.frameIndex, ui.layerIndex, output);
      changed(options);
      rerender();
    });
    paste.disabled = !ui.selection || !ui.clipboard;
    bar.appendChild(paste);

    var grid = button(ui.showGrid ? 'Grid: On' : 'Grid: Off',
      ui.showGrid ? 'active' : '', function() {
        ui.showGrid = !ui.showGrid;
        rerender();
      });
    grid.setAttribute('aria-pressed', ui.showGrid ? 'true' : 'false');
    bar.appendChild(grid);
    var background = button(ui.background === 'white'
      ? 'Transparency: White' : 'Transparency: Checkerboard',
      ui.background === 'white' ? 'white-preview' : '', function() {
        ui.background = ui.background === 'white' ? 'checkerboard' : 'white';
        rerender();
      });
    background.title = 'Changes transparent pixels in editor previews only.';
    bar.appendChild(background);
    return bar;
  }

  function colorPanel(ui, rerender) {
    var panel = element('section', 'sprite-color-panel');
    panel.appendChild(element('h3', '', 'Paint Color'));
    var swatch = element('span', 'sprite-color-swatch');
    function swatchColor() {
      return 'rgba(' + ui.color[0] + ',' + ui.color[1] + ',' + ui.color[2] +
        ',' + (ui.color[3] / 255).toFixed(4) + ')';
    }
    swatch.style.background = swatchColor();
    panel.appendChild(swatch);
    var colorLabel = element('label', 'sprite-color-control');
    colorLabel.appendChild(element('span', '', 'RGB color'));
    var color = element('input');
    color.type = 'color';
    color.value = rgbHex(ui.color);
    color.addEventListener('input', function() {
      var rgb = parseRgbHex(color.value);
      ui.color = [rgb[0], rgb[1], rgb[2], ui.color[3]];
      swatch.style.background = swatchColor();
    });
    color.addEventListener('change', rerender);
    colorLabel.appendChild(color);
    panel.appendChild(colorLabel);
    var alphaLabel = element('label', 'sprite-color-control');
    var alphaText = element('span', '', 'Alpha: ' + ui.color[3] + ' / 255');
    alphaLabel.appendChild(alphaText);
    var alpha = element('input');
    alpha.type = 'range'; alpha.min = '0'; alpha.max = '255'; alpha.step = '1';
    alpha.value = String(ui.color[3]);
    alpha.addEventListener('input', function() {
      ui.color[3] = Number(alpha.value);
      alphaText.textContent = 'Alpha: ' + ui.color[3] + ' / 255';
      swatch.style.background = swatchColor();
    });
    alpha.addEventListener('change', rerender);
    alphaLabel.appendChild(alpha);
    panel.appendChild(alphaLabel);
    panel.appendChild(element('small', '',
      'Alpha 0 is transparent. PNG and WebM exports preserve this channel.'));
    return panel;
  }

  function layerPanel(state, asset, ui, options, rerender) {
    var frame = asset.frames[ui.frameIndex];
    var panel = element('aside', 'sprite-layer-panel');
    var heading = element('div', 'sprite-panel-heading');
    heading.appendChild(element('h3', '', 'Layers'));
    heading.appendChild(button('Add', 'btn-secondary', function() {
      L.addLayer(state, asset.id, ui.frameIndex);
      ui.layerIndex = frame.layers.length;
      changed(options);
      rerender();
    }));
    panel.appendChild(heading);
    var list = element('div', 'sprite-layer-list');
    list.setAttribute('data-sprite-scroll-key', 'sprite:layers:' + asset.id + ':' + ui.frameIndex);
    frame.layers.slice().reverse().forEach(function(layer) {
      var index = frame.layers.indexOf(layer);
      var row = element('div', 'sprite-layer-row' +
        (index === ui.layerIndex ? ' selected' : ''));
      var select = button(layer.name, 'sprite-layer-select', function() {
        ui.layerIndex = index;
        ui.selection = null;
        rerender();
      });
      select.setAttribute('aria-pressed', index === ui.layerIndex ? 'true' : 'false');
      row.appendChild(select);
      var visible = element('input');
      visible.type = 'checkbox';
      visible.checked = layer.visible !== false;
      visible.setAttribute('aria-label', 'Show ' + layer.name);
      visible.addEventListener('change', function() {
        L.setLayerVisible(state, asset.id, ui.frameIndex, index, visible.checked);
        changed(options);
        rerender();
      });
      row.appendChild(visible);
      list.appendChild(row);
    });
    panel.appendChild(list);
    var actions = element('div', 'sprite-layer-actions');
    actions.appendChild(button('Duplicate', 'btn-secondary', function() {
      L.addLayer(state, asset.id, ui.frameIndex, ui.layerIndex);
      ui.layerIndex = frame.layers.length;
      changed(options);
      rerender();
    }));
    var rename = button('Rename', 'btn-secondary', function() {
      var wanted = window.prompt('Layer name', frame.layers[ui.layerIndex].name);
      if (wanted === null) return;
      L.renameLayer(state, asset.id, ui.frameIndex, ui.layerIndex, wanted);
      changed(options);
      rerender();
    });
    actions.appendChild(rename);
    var up = button('Up', 'btn-secondary', function() {
      L.moveLayer(state, asset.id, ui.frameIndex, ui.layerIndex,
        ui.layerIndex + 1);
      ui.layerIndex++;
      changed(options);
      rerender();
    });
    up.disabled = ui.layerIndex >= frame.layers.length - 1;
    actions.appendChild(up);
    var down = button('Down', 'btn-secondary', function() {
      L.moveLayer(state, asset.id, ui.frameIndex, ui.layerIndex,
        ui.layerIndex - 1);
      ui.layerIndex--;
      changed(options);
      rerender();
    });
    down.disabled = ui.layerIndex <= 0;
    actions.appendChild(down);
    var remove = button('Remove', 'btn-secondary sprite-danger', function() {
      L.removeLayer(state, asset.id, ui.frameIndex, ui.layerIndex);
      ui.layerIndex = clamp(ui.layerIndex, 0, frame.layers.length - 2);
      changed(options);
      rerender();
    });
    remove.disabled = frame.layers.length <= 1;
    actions.appendChild(remove);
    panel.appendChild(actions);

    var transform = element('div', 'sprite-transform-tools');
    transform.appendChild(element('strong', '', 'Selected layer'));
    [['←', -1, 0], ['→', 1, 0], ['↑', 0, -1], ['↓', 0, 1]].forEach(function(row) {
      transform.appendChild(button(row[0], 'btn-secondary', function() {
        L.shiftLayer(state, asset.id, ui.frameIndex, ui.layerIndex, row[1], row[2]);
        changed(options);
        rerender();
      }));
    });
    transform.appendChild(button('Flip H', 'btn-secondary', function() {
      L.flipLayer(state, asset.id, ui.frameIndex, ui.layerIndex, false);
      changed(options);
      rerender();
    }));
    transform.appendChild(button('Flip V', 'btn-secondary', function() {
      L.flipLayer(state, asset.id, ui.frameIndex, ui.layerIndex, true);
      changed(options);
      rerender();
    }));
    panel.appendChild(transform);
    panel.appendChild(colorPanel(ui, rerender));
    return panel;
  }

  function frameStrip(state, asset, ui, options, rerender) {
    var section = element('section', 'sprite-frame-section');
    var heading = element('div', 'sprite-frame-heading');
    heading.appendChild(element('h3', '', 'Frame Collage'));
    var actions = element('div', 'sprite-frame-heading-actions');
    actions.appendChild(button('Add Blank', 'btn-secondary', function() {
      L.addFrame(state, asset.id, ui.frameIndex, false);
      ui.frameIndex++;
      ui.layerIndex = 0;
      changed(options);
      rerender();
    }));
    actions.appendChild(button('Duplicate', 'btn-secondary', function() {
      L.addFrame(state, asset.id, ui.frameIndex, true);
      ui.frameIndex++;
      ui.layerIndex = 0;
      changed(options);
      rerender();
    }));
    var remove = button('Remove', 'btn-secondary sprite-danger', function() {
      L.removeFrame(state, asset.id, ui.frameIndex);
      ui.frameIndex = clamp(ui.frameIndex, 0, asset.frames.length - 2);
      ui.layerIndex = 0;
      changed(options);
      rerender();
    });
    remove.disabled = asset.frames.length <= 1;
    actions.appendChild(remove);
    var left = button('Move Left', 'btn-secondary', function() {
      L.moveFrame(state, asset.id, ui.frameIndex, ui.frameIndex - 1);
      ui.frameIndex--;
      changed(options);
      rerender();
    });
    left.disabled = ui.frameIndex <= 0;
    actions.appendChild(left);
    var right = button('Move Right', 'btn-secondary', function() {
      L.moveFrame(state, asset.id, ui.frameIndex, ui.frameIndex + 1);
      ui.frameIndex++;
      changed(options);
      rerender();
    });
    right.disabled = ui.frameIndex >= asset.frames.length - 1;
    actions.appendChild(right);
    heading.appendChild(actions);
    section.appendChild(heading);

    var strip = element('div', 'sprite-frame-strip');
    strip.setAttribute('data-sprite-scroll-key', 'sprite:frames:' + asset.id);
    asset.frames.forEach(function(frame, index) {
      var card = button('', 'sprite-frame-card' +
        (index === ui.frameIndex ? ' selected' : ''), function() {
          ui.frameIndex = index;
          ui.layerIndex = clamp(ui.layerIndex, 0, frame.layers.length - 1);
          ui.selection = null;
          rerender();
        });
      card.setAttribute('aria-label', 'Select frame ' + (index + 1));
      card.setAttribute('aria-pressed', index === ui.frameIndex ? 'true' : 'false');
      card.appendChild(element('strong', '', 'Frame ' + (index + 1)));
      var canvas = element('canvas');
      var scale = Math.max(1, Math.min(4,
        Math.floor(96 / Math.max(asset.width, asset.height))));
      paintPixels(canvas, asset.width, asset.height,
        L.compositeFrame(asset, index), scale, ui.background, false);
      card.appendChild(canvas);
      card.appendChild(element('span', '', frame.ticks +
        (frame.ticks === 1 ? ' tick' : ' ticks')));
      strip.appendChild(card);
    });
    section.appendChild(strip);
    return section;
  }

  function canvasWorkspace(state, asset, ui, options, rerender) {
    var workspace = element('div', 'sprite-workspace');
    var main = element('main', 'sprite-canvas-column');
    var canvasControls = element('div', 'sprite-canvas-controls');
    var ticks = element('label', 'sprite-compact-control');
    ticks.appendChild(element('span', '', 'Frame ticks'));
    var tickInput = element('input');
    tickInput.type = 'number'; tickInput.min = '1'; tickInput.max = '255';
    tickInput.step = '1'; tickInput.value = String(asset.frames[ui.frameIndex].ticks);
    tickInput.setAttribute('data-sprite-focus-key', 'frame-ticks');
    tickInput.addEventListener('change', function() {
      try {
        L.setFrameTicks(state, asset.id, ui.frameIndex, Number(tickInput.value));
        changed(options);
        rerender();
      } catch (error) {
        notify(options, 'Frame timing blocked: ' + error.message);
        tickInput.value = String(asset.frames[ui.frameIndex].ticks);
      }
    });
    ticks.appendChild(tickInput);
    canvasControls.appendChild(ticks);
    var zoom = element('label', 'sprite-compact-control sprite-zoom-control');
    var zoomText = element('span', '', 'Zoom ' + ui.zoom + '×');
    zoom.appendChild(zoomText);
    var zoomInput = element('input');
    zoomInput.type = 'range'; zoomInput.min = '1'; zoomInput.max = '24';
    zoomInput.step = '1'; zoomInput.value = String(ui.zoom);
    zoomInput.addEventListener('input', function() {
      ui.zoom = Number(zoomInput.value);
      zoomText.textContent = 'Zoom ' + ui.zoom + '×';
    });
    zoomInput.addEventListener('change', rerender);
    zoom.appendChild(zoomInput);
    canvasControls.appendChild(zoom);
    canvasControls.appendChild(element('span', 'sprite-dimensions',
      asset.width + '×' + asset.height + ' pixels'));
    main.appendChild(canvasControls);
    var scroll = element('div', 'sprite-canvas-scroll');
    scroll.setAttribute('data-sprite-scroll-key', 'sprite:canvas:' + asset.id);
    var canvas = element('canvas', 'sprite-edit-canvas');
    paintPixels(canvas, asset.width, asset.height,
      L.compositeFrame(asset, ui.frameIndex), ui.zoom,
      ui.background, ui.showGrid, ui.selection);
    installCanvasEditing(canvas, state, asset, ui, options, rerender);
    scroll.appendChild(canvas);
    main.appendChild(scroll);
    workspace.appendChild(main);
    workspace.appendChild(layerPanel(state, asset, ui, options, rerender));
    return workspace;
  }

  function downloadBlob(blob, filename) {
    var url = URL.createObjectURL(blob);
    var link = document.createElement('a');
    link.href = url;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
    return filename;
  }

  function assetStem(asset) {
    return String(asset.name || 'sprite-asset').toLowerCase()
      .replace(/[^a-z0-9._-]+/g, '-').replace(/^-+|-+$/g, '') || 'sprite-asset';
  }

  function transparentCanvas(width, height, pixels) {
    var canvas = document.createElement('canvas');
    canvas.width = width; canvas.height = height;
    var context = canvas.getContext('2d', { alpha: true });
    var image = context.createImageData(width, height);
    image.data.set(pixels);
    context.putImageData(image, 0, 0);
    return canvas;
  }

  function exportFramePng(asset, frameIndex, options) {
    var canvas = transparentCanvas(asset.width, asset.height,
      L.compositeFrame(asset, frameIndex));
    canvas.toBlob(function(blob) {
      if (!blob) {
        notify(options, 'PNG export failed.');
        return;
      }
      var name = assetStem(asset) + '-frame-' +
        String(frameIndex + 1).padStart(2, '0') + '.png';
      downloadBlob(blob, name);
      notify(options, 'Frame exported as ' + name + '.');
    }, 'image/png');
  }

  function webmSupported() {
    var canvas = document.createElement('canvas');
    return !!window.MediaRecorder && typeof canvas.captureStream === 'function';
  }

  function webmMimeType() {
    var choices = ['video/webm;codecs=vp9', 'video/webm;codecs=vp8', 'video/webm'];
    if (!window.MediaRecorder) return '';
    if (typeof window.MediaRecorder.isTypeSupported !== 'function') return 'video/webm';
    for (var index = 0; index < choices.length; index++) {
      if (window.MediaRecorder.isTypeSupported(choices[index])) return choices[index];
    }
    return '';
  }

  function waitUntil(deadline) {
    var now = typeof performance !== 'undefined' && performance.now
      ? performance.now() : Date.now();
    return new Promise(function(resolve) {
      setTimeout(resolve, Math.max(0, deadline - now));
    });
  }

  async function exportSequenceWebm(asset, options) {
    if (!webmSupported()) throw new Error('This browser cannot export WebM files.');
    var canvas = transparentCanvas(asset.width, asset.height,
      L.compositeFrame(asset, 0));
    var context = canvas.getContext('2d', { alpha: true });
    var stream = canvas.captureStream(0);
    var track = stream.getVideoTracks()[0];
    var manual = track && typeof track.requestFrame === 'function';
    if (!manual) {
      stream.getTracks().forEach(function(row) { row.stop(); });
      stream = canvas.captureStream(30);
      track = stream.getVideoTracks()[0];
    }
    var mimeType = webmMimeType();
    var recorder = new window.MediaRecorder(stream, {
      mimeType: mimeType,
      videoBitsPerSecond: 6000000
    });
    var chunks = [];
    var completed = new Promise(function(resolve, reject) {
      recorder.addEventListener('dataavailable', function(event) {
        if (event.data && event.data.size) chunks.push(event.data);
      });
      recorder.addEventListener('error', function(event) {
        reject(event.error || new Error('WebM recording failed.'));
      });
      recorder.addEventListener('stop', function() {
        resolve(new Blob(chunks, { type: recorder.mimeType || mimeType }));
      });
    });
    try {
      recorder.start();
      var started = typeof performance !== 'undefined' && performance.now
        ? performance.now() : Date.now();
      var tick = 0;
      for (var frameIndex = 0; frameIndex < asset.frames.length; frameIndex++) {
        var surface = transparentCanvas(asset.width, asset.height,
          L.compositeFrame(asset, frameIndex));
        var repeats = Math.max(1, asset.frames[frameIndex].ticks);
        for (var repeat = 0; repeat < repeats; repeat++) {
          context.clearRect(0, 0, canvas.width, canvas.height);
          context.drawImage(surface, 0, 0);
          if (manual) track.requestFrame();
          tick++;
          await waitUntil(started + tick * (1000 / 30));
        }
      }
      recorder.stop();
      var blob = await completed;
      if (!blob.size) throw new Error('The browser produced an empty WebM file.');
      var filename = assetStem(asset) + '-sequence.webm';
      downloadBlob(blob, filename);
      notify(options, 'Sequence exported as ' + filename + '.');
      return filename;
    } finally {
      if (recorder.state !== 'inactive') recorder.stop();
      stream.getTracks().forEach(function(row) { row.stop(); });
    }
  }

  function spriteImageColorLimit(asset) {
    var template = asset && asset.provenance && asset.provenance.template;
    if (template === 'class-avatar') return 80;
    if (template === 'item-icon') return 255;
    return 256;
  }

  function openLayerImageImportModal(source, state, asset, ui, options, rerender) {
    if (!OB64.art || typeof OB64.art.prepareSpriteImageImport !== 'function') {
      notify(options, 'Layer image import blocked: the image preparation tools are unavailable.');
      return;
    }
    var frameIndex = ui.frameIndex;
    var layerIndex = ui.layerIndex;
    var targetWidth = asset.width;
    var targetHeight = asset.height;
    var maximumColors = spriteImageColorLimit(asset);
    var settings = {
      resizeMode: 'nearest', panX: 0.5, panY: 0.5, dither: false
    };
    var currentResult = null;
    var scheduledFrame = null;
    var overlay = element('div', 'error-modal-overlay art-import-overlay');
    var modal = element('div',
      'error-modal art-import-modal sprite-layer-import-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'sprite-layer-import-title');
    overlay.appendChild(modal);

    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'Prepare Layer Image');
    title.id = 'sprite-layer-import-title';
    header.appendChild(title);
    var closeButton = button('\u00D7', 'error-modal-close', close);
    closeButton.setAttribute('aria-label', 'Cancel layer image import');
    header.appendChild(closeButton);
    modal.appendChild(header);

    var body = element('div', 'error-modal-body art-import-body');
    body.appendChild(element('p', 'art-import-intro', source.name + ' \u00B7 ' +
      source.format + ' \u00B7 ' + source.width + '\u00D7' + source.height +
      ' source pixels. The result is cropped and resized to ' +
      targetWidth + '\u00D7' + targetHeight + '.'));
    var layout = element('div', 'art-import-layout');
    var previewPanel = element('section', 'art-import-preview-panel');
    previewPanel.appendChild(element('h3', '',
      'Converted ' + targetWidth + '\u00D7' + targetHeight + ' Preview'));
    var previewHost = element('div', 'art-import-preview-host');
    previewPanel.appendChild(previewHost);
    var stats = element('p', 'art-import-stats', 'Preparing preview\u2026');
    stats.setAttribute('aria-live', 'polite');
    previewPanel.appendChild(stats);
    layout.appendChild(previewPanel);

    var controls = element('section', 'art-import-controls');
    var resizeLabel = element('label', 'art-import-control');
    resizeLabel.appendChild(element('span', '', 'Resize method'));
    var resizeSelect = element('select');
    [['nearest', 'Pixel Art \u2014 nearest-neighbor'],
      ['smooth', 'Smooth \u2014 area/bilinear']].forEach(function(row) {
      var option = element('option', '', row[1]);
      option.value = row[0];
      resizeSelect.appendChild(option);
    });
    resizeSelect.value = settings.resizeMode;
    resizeSelect.addEventListener('change', function() {
      settings.resizeMode = resizeSelect.value;
      schedulePreview();
    });
    resizeLabel.appendChild(resizeSelect);
    controls.appendChild(resizeLabel);

    var initialCrop = OB64.art.imageCropRect(
      source.width, source.height, targetWidth, targetHeight,
      settings.panX, settings.panY);
    function cropSlider(labelText, key, enabled) {
      var label = element('label', 'art-import-control');
      label.appendChild(element('span', '', labelText));
      var input = element('input');
      input.type = 'range';
      input.min = '0'; input.max = '100'; input.step = '1'; input.value = '50';
      input.disabled = !enabled;
      input.setAttribute('aria-label', labelText);
      input.addEventListener('input', function() {
        settings[key] = Number(input.value) / 100;
        schedulePreview();
      });
      label.appendChild(input);
      if (!enabled) label.classList.add('disabled');
      controls.appendChild(label);
    }
    cropSlider('Horizontal crop position', 'panX',
      initialCrop.horizontalPanAvailable);
    cropSlider('Vertical crop position', 'panY',
      initialCrop.verticalPanAvailable);

    var ditherWrap = element('div', 'art-import-dither');
    var ditherLabel = element('label', 'art-import-checkbox');
    var dither = element('input');
    dither.type = 'checkbox';
    dither.addEventListener('change', function() {
      settings.dither = dither.checked;
      schedulePreview();
    });
    ditherLabel.appendChild(dither);
    ditherLabel.appendChild(document.createTextNode(' Ordered dithering'));
    ditherWrap.appendChild(ditherLabel);
    ditherWrap.appendChild(element('small', '',
      'Off by default. It can smooth gradients when the resized image exceeds ' +
      maximumColors + ' native RGB555 colors.'));
    controls.appendChild(ditherWrap);
    controls.appendChild(element('p', 'art-import-background',
      'PNG transparency remains in this layer. Target editors apply their final alpha rules.'));
    if (asset.provenance && asset.provenance.template === 'item-icon') {
      controls.appendChild(element('p', 'art-import-background',
        'Item icons allow 255 opaque colors. Final import maps them to the selected icon pack palette.'));
    }
    layout.appendChild(controls);
    body.appendChild(layout);
    modal.appendChild(body);

    var footer = element('div', 'error-modal-footer art-import-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var applyButton = button('Import into Layer', 'error-modal-ok', function() {
      if (!currentResult) return;
      try {
        L.replaceLayerPixels(
          state, asset.id, frameIndex, layerIndex, currentResult.rgba);
        changed(options);
        var crop = currentResult.crop;
        notify(options, 'Layer image import applied: file=' + source.name +
          '; format=' + source.format +
          '; source=' + source.width + 'x' + source.height +
          '; target=' + targetWidth + 'x' + targetHeight +
          '; crop=' + crop.width.toFixed(2) + 'x' + crop.height.toFixed(2) +
          '@(' + crop.x.toFixed(2) + ',' + crop.y.toFixed(2) + ')' +
          '; resize=' + currentResult.resizeMode +
          '; RGB555 colors=' + currentResult.sourceNativeColorCount + '->' +
          currentResult.colorCount + '/' + maximumColors +
          '; Wu quantized=' + currentResult.quantized +
          '; ordered dither=' + currentResult.dithered +
          '; non-opaque output pixels=' + currentResult.outputNonOpaquePixels + '.');
        close();
        rerender();
      } catch (error) {
        notify(options, 'Layer image import blocked: ' + error.message);
      }
    });
    applyButton.disabled = true;
    footer.appendChild(applyButton);
    modal.appendChild(footer);

    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    function preview() {
      scheduledFrame = null;
      try {
        currentResult = OB64.art.prepareSpriteImageImport(
          source.rgba, source.width, source.height,
          targetWidth, targetHeight, {
            resizeMode: settings.resizeMode,
            panX: settings.panX,
            panY: settings.panY,
            dither: settings.dither,
            maximumColors: maximumColors
          });
        previewHost.innerHTML = '';
        var scale = Math.max(1, Math.min(8,
          Math.floor(360 / Math.max(targetWidth, targetHeight))));
        var canvas = element('canvas', 'art-import-preview-canvas');
        paintPixels(canvas, targetWidth, targetHeight,
          currentResult.rgba, scale, ui.background, false);
        previewHost.appendChild(canvas);
        var crop = currentResult.crop;
        stats.textContent = 'Crop ' + crop.width.toFixed(1) + '\u00D7' +
          crop.height.toFixed(1) + ' at ' + crop.x.toFixed(1) + ', ' +
          crop.y.toFixed(1) + ' \u00B7 ' +
          currentResult.sourceNativeColorCount + ' native colors \u2192 ' +
          currentResult.colorCount + ' / ' + maximumColors +
          (currentResult.quantized ? ' \u00B7 Wu quantized' :
            ' \u00B7 quantization not required') +
          (currentResult.dithered ? ' \u00B7 ordered dither applied' : '') +
          ' \u00B7 ' + currentResult.outputNonOpaquePixels +
          ' non-opaque pixels';
        stats.classList.remove('blocked');
        applyButton.disabled = false;
      } catch (error) {
        currentResult = null;
        previewHost.innerHTML = '';
        stats.textContent = 'Conversion blocked: ' + error.message;
        stats.classList.add('blocked');
        applyButton.disabled = true;
      }
    }
    function schedulePreview() {
      stats.textContent = 'Preparing preview\u2026';
      applyButton.disabled = true;
      if (scheduledFrame !== null) window.cancelAnimationFrame(scheduledFrame);
      scheduledFrame = window.requestAnimationFrame(preview);
    }
    function close() {
      if (scheduledFrame !== null) window.cancelAnimationFrame(scheduledFrame);
      scheduledFrame = null;
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escapeHandler);
    }
    var escapeHandler = function(event) {
      if (event.key === 'Escape') close();
    };
    document.addEventListener('keydown', escapeHandler);
    document.body.appendChild(overlay);
    resizeSelect.focus();
    schedulePreview();
  }

  function layerImageImportInput(state, asset, ui, options, rerender) {
    var input = element('input', 'art-image-input');
    input.type = 'file';
    input.accept = 'image/png,image/jpeg,.png,.jpg,.jpeg';
    input.setAttribute('aria-label',
      'Select a PNG or JPEG image to import into the selected sprite layer');
    input.addEventListener('change', async function() {
      var file = input.files && input.files[0];
      if (!file) return;
      try {
        if (!OB64.artUI || typeof OB64.artUI.decodeImageSource !== 'function') {
          throw new Error('the image decoder is unavailable');
        }
        var source = await OB64.artUI.decodeImageSource(file);
        openLayerImageImportModal(
          source, state, asset, ui, options, rerender);
      } catch (error) {
        notify(options, 'Layer image import blocked: ' + error.message);
      } finally {
        input.value = '';
      }
    });
    return input;
  }

  function assetActions(state, asset, ui, options, rerender) {
    var bar = element('div', 'sprite-asset-actions');
    bar.appendChild(button('Save Layer as Sprite', 'btn-secondary', function() {
      var added = L.copyPart(state, asset.id, 'sprite', ui.frameIndex, ui.layerIndex);
      ui.assetId = added.id; ui.frameIndex = 0; ui.layerIndex = 0;
      changed(options); rerender();
    }));
    bar.appendChild(button('Save Frame as Asset', 'btn-secondary', function() {
      var added = L.copyPart(state, asset.id, 'frame', ui.frameIndex, ui.layerIndex);
      ui.assetId = added.id; ui.frameIndex = 0; ui.layerIndex = 0;
      changed(options); rerender();
    }));
    var saveSequence = button('Save Sequence as Asset', 'btn-secondary', function() {
      var added = L.copyPart(state, asset.id, 'sequence', ui.frameIndex, ui.layerIndex);
      ui.assetId = added.id; ui.frameIndex = 0; ui.layerIndex = 0;
      changed(options); rerender();
    });
    saveSequence.disabled = asset.frames.length < 2;
    bar.appendChild(saveSequence);
    var importInput = layerImageImportInput(state, asset, ui, options, rerender);
    bar.appendChild(importInput);
    bar.appendChild(button('Import Image into Layer\u2026', 'btn-secondary', function() {
      importInput.click();
    }));
    bar.appendChild(button('Export Frame PNG', 'btn-secondary', function() {
      exportFramePng(asset, ui.frameIndex, options);
    }));
    var webm = button('Export Sequence WebM', 'btn-secondary', function() {
      webm.disabled = true;
      webm.textContent = 'Exporting…';
      exportSequenceWebm(asset, options).catch(function(error) {
        notify(options, 'WebM export failed: ' + error.message);
      }).then(function() {
        webm.disabled = !webmSupported();
        webm.textContent = 'Export Sequence WebM';
      });
    });
    webm.disabled = !webmSupported();
    bar.appendChild(webm);
    bar.appendChild(button('Export Asset File', 'btn-secondary', function() {
      var blob = new Blob([L.assetFileText(asset)], { type: 'application/json' });
      var filename = L.filename(asset);
      downloadBlob(blob, filename);
      notify(options, 'Sprite asset exported as ' + filename + '.');
    }));
    var resize = button('Resize Canvas…', 'btn-secondary', function() {
      var width = window.prompt('Canvas width from 1 through ' + L.MAX_DIMENSION,
        String(asset.width));
      if (width === null) return;
      var height = window.prompt('Canvas height from 1 through ' + L.MAX_DIMENSION,
        String(asset.height));
      if (height === null) return;
      try {
        L.resizeAsset(state, asset.id, Number(width), Number(height));
        changed(options); rerender();
      } catch (error) {
        notify(options, 'Canvas resize blocked: ' + error.message);
      }
    });
    bar.appendChild(resize);
    bar.appendChild(button('Rotate Left', 'btn-secondary', function() {
      L.rotateAsset(state, asset.id, false);
      ui.selection = null; changed(options); rerender();
    }));
    bar.appendChild(button('Rotate Right', 'btn-secondary', function() {
      L.rotateAsset(state, asset.id, true);
      ui.selection = null; changed(options); rerender();
    }));
    if (options && typeof options.onOpenArt === 'function') {
      bar.appendChild(button('Open Art and Animation', 'btn-secondary', function() {
        options.onOpenArt();
      }));
    }
    return bar;
  }

  function editor(state, asset, ui, options, rerender) {
    var main = element('div', 'sprite-editor-main');
    var heading = element('div', 'sprite-editor-heading');
    var title = element('div');
    var name = element('input', 'sprite-name-input');
    name.type = 'text';
    name.value = asset.name;
    name.maxLength = 120;
    name.setAttribute('aria-label', 'Sprite asset name');
    name.setAttribute('data-sprite-focus-key', 'asset-name');
    name.addEventListener('change', function() {
      try {
        L.renameAsset(state, asset.id, name.value);
        changed(options); rerender();
      } catch (error) {
        name.value = asset.name;
        notify(options, 'Asset rename blocked: ' + error.message);
      }
    });
    title.appendChild(name);
    title.appendChild(element('p', '', asset.kind + ' · ' + asset.width + '×' +
      asset.height + ' · ' + asset.frames.length +
      (asset.frames.length === 1 ? ' frame' : ' frames') +
      (asset.provenance && asset.provenance.label
        ? ' · source: ' + asset.provenance.label : '')));
    heading.appendChild(title);
    heading.appendChild(element('span', 'art-badge art-badge-edited', 'Project Asset'));
    main.appendChild(heading);
    main.appendChild(frameStrip(state, asset, ui, options, rerender));
    main.appendChild(toolbar(state, asset, ui, options, rerender));
    main.appendChild(canvasWorkspace(state, asset, ui, options, rerender));
    main.appendChild(assetActions(state, asset, ui, options, rerender));
    main.appendChild(element('p', 'sprite-project-note',
      'Sprite Library assets stay in the Project. Import a compatible asset in Art and Animation before ROM export.'));
    return main;
  }

  function imageImportInput(state, ui, options, rerender) {
    var input = element('input', 'art-image-input');
    input.type = 'file';
    input.accept = 'image/png,image/jpeg,.png,.jpg,.jpeg';
    input.setAttribute('aria-label', 'Import a PNG or JPEG as a Sprite Editor asset');
    input.addEventListener('change', async function() {
      var file = input.files && input.files[0];
      if (!file) return;
      try {
        if (!OB64.artUI || typeof OB64.artUI.decodeImageSource !== 'function') {
          throw new Error('The image decoder is unavailable.');
        }
        var source = await OB64.artUI.decodeImageSource(file);
        var asset = L.assetFromRgba(state, {
          name: file.name.replace(/\.[^.]+$/, ''),
          kind: 'sprite', width: source.width, height: source.height,
          rgba: source.rgba, provenance: {
            source: 'image-file', label: file.name, format: source.format
          }
        });
        L.addAsset(state, asset);
        ui.assetId = asset.id; ui.frameIndex = 0; ui.layerIndex = 0;
        if (Math.max(asset.width, asset.height) > 512) ui.zoom = 1;
        changed(options);
        notify(options, 'Imported ' + file.name + ' into the Sprite Library.');
        rerender();
      } catch (error) {
        notify(options, 'Image import blocked: ' + error.message);
      } finally {
        input.value = '';
      }
    });
    return input;
  }

  function assetFileInput(state, ui, options, rerender) {
    var input = element('input', 'art-image-input');
    input.type = 'file';
    input.accept = '.json,.ob64-sprite.json,application/json';
    input.setAttribute('aria-label', 'Import a LordlyCaliber sprite asset file');
    input.addEventListener('change', function() {
      var file = input.files && input.files[0];
      if (!file) return;
      var reader = new FileReader();
      reader.onload = function(event) {
        try {
          var asset = L.prepareAssetFile(event.target.result);
          asset = L.addAsset(state, asset, { renameCollision: true });
          ui.assetId = asset.id; ui.frameIndex = 0; ui.layerIndex = 0;
          changed(options);
          notify(options, 'Imported sprite asset file ' + file.name + '.');
          rerender();
        } catch (error) {
          notify(options, 'Sprite asset file import blocked: ' + error.message);
        }
      };
      reader.onerror = function() {
        notify(options, 'Sprite asset file import failed while reading ' + file.name + '.');
      };
      reader.readAsText(file);
      input.value = '';
    });
    return input;
  }

  function knownSpriteTemplates(rom) {
    var templates = L.knownSpriteFormats().map(function(format) {
      return Object.assign({ group: 'Known sprite formats' }, format);
    });
    var seenCombatTargets = {};
    animationSources(rom).forEach(function(animation) {
      var spec = animation.spec || {};
      var width = Number(animation.canvas.width);
      var height = Number(animation.canvas.height);
      if (!Number.isInteger(width) || !Number.isInteger(height)) return;
      var className = plainGameLabel(spec.className) || 'Unknown class';
      var actionName = plainGameLabel(spec.actionName) || 'Unknown action';
      var targetKey = className + ':' + actionName + ':' + width + 'x' + height;
      if (seenCombatTargets[targetKey]) return;
      seenCombatTargets[targetKey] = true;
      templates.push({
        id: 'combat-frame:' + String(animation.key || targetKey),
        group: 'Combat animation frames',
        label: className + ' · ' + actionName,
        kind: 'frame', width: width, height: height,
        defaultName: (className + ' ' + actionName + ' Frame').slice(0, 120),
        description: 'Creates a blank frame for this combat animation canvas.',
        animationKey: String(animation.key || '')
      });
    });
    return templates;
  }

  function openNewAssetModal(rom, state, ui, options, rerender) {
    var overlay = element('div', 'error-modal-overlay sprite-modal-overlay');
    var modal = element('div', 'error-modal sprite-small-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'sprite-new-title');
    overlay.appendChild(modal);
    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'New Sprite Asset'); title.id = 'sprite-new-title';
    header.appendChild(title);
    header.appendChild(button('×', 'error-modal-close', close));
    modal.appendChild(header);
    var body = element('div', 'error-modal-body sprite-new-fields');
    function field(labelText, type, value) {
      var label = element('label');
      label.appendChild(element('span', '', labelText));
      var input = element('input'); input.type = type; input.value = value;
      label.appendChild(input); body.appendChild(label); return input;
    }
    var templates = knownSpriteTemplates(rom);
    var templateById = {};
    var formatLabel = element('label');
    formatLabel.appendChild(element('span', '', 'Sprite target'));
    var format = element('select');
    var groups = {};
    templates.forEach(function(template) {
      templateById[template.id] = template;
      if (!groups[template.group]) {
        groups[template.group] = element('optgroup');
        groups[template.group].label = template.group;
        format.appendChild(groups[template.group]);
      }
      var option = element('option', '', template.label + ' · ' +
        template.width + '×' + template.height);
      option.value = template.id;
      groups[template.group].appendChild(option);
    });
    formatLabel.appendChild(format);
    body.appendChild(formatLabel);
    var name = field('Name', 'text', templates[0].defaultName);
    var formatHelp = element('p', 'sprite-new-format-help');
    formatHelp.setAttribute('aria-live', 'polite');
    body.appendChild(formatHelp);
    var nameEdited = false;
    name.addEventListener('input', function() { nameEdited = true; });
    function selectedTemplate() {
      return templateById[format.value] || templates[0];
    }
    function updateTemplate() {
      var template = selectedTemplate();
      formatHelp.textContent = template.width + '×' + template.height + ' pixels. ' +
        template.description;
      if (!nameEdited) name.value = template.defaultName;
    }
    format.addEventListener('change', updateTemplate);
    updateTemplate();
    modal.appendChild(body);
    var footer = element('div', 'error-modal-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    footer.appendChild(button('Create Sprite', 'error-modal-ok', function() {
      try {
        var template = selectedTemplate();
        var asset = L.blankAsset(state, {
          name: name.value, kind: template.kind,
          width: template.width, height: template.height,
          provenance: {
            source: 'blank-template', template: template.id,
            label: template.label, animationKey: template.animationKey || ''
          }
        });
        L.addAsset(state, asset);
        ui.assetId = asset.id; ui.frameIndex = 0; ui.layerIndex = 0;
        changed(options); close(); rerender();
      } catch (error) {
        notify(options, 'New sprite blocked: ' + error.message);
      }
    }));
    modal.appendChild(footer);
    overlay.addEventListener('click', function(event) { if (event.target === overlay) close(); });
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escape);
    }
    function escape(event) { if (event.key === 'Escape') close(); }
    document.addEventListener('keydown', escape);
    document.body.appendChild(overlay);
    format.focus();
  }

  function animationSources(rom) {
    var rows = rom && rom.art && rom.art.animations
      ? rom.art.animations.specs.slice() : [];
    Object.keys(rom && rom.animationSequences &&
      rom.animationSequences.separations || {}).sort().forEach(function(id) {
      var animation = rom.animationSequences.separations[id].syntheticAnimation;
      if (animation) rows.push(animation);
    });
    return rows.filter(function(animation) {
      return animation && animation.canvas && animation.frames && animation.frames.length;
    });
  }

  function animationLabel(animation) {
    return animation.spec.className + ' · ' + animation.spec.actionName + ' · ' +
      (OB64.animationUI && OB64.animationUI.animationArtVariantLabel
        ? OB64.animationUI.animationArtVariantLabel(animation)
        : animation.spec.variantLabel || 'Art');
  }

  function registerAnimationSources(rom, animation) {
    var target = rom.art.animations.artByKey;
    Object.keys(animation.artByKey || {}).forEach(function(key) {
      target[key] = animation.artByKey[key];
    });
  }

  function framesFromAnimation(rom, animation, mode, selectedFrame,
      selectedLayer, childOrdinal) {
    if (!OB64.animationUI) throw new Error('Animation rendering is unavailable.');
    registerAnimationSources(rom, animation);
    var frameRows = mode === 'sequence'
      ? animation.frames : [animation.frames[selectedFrame]];
    return frameRows.map(function(frame, outputIndex) {
      if (!frame) throw new Error('The selected animation frame is unavailable.');
      var sourceFrameIndex = mode === 'sequence' ? outputIndex : selectedFrame;
      if (mode === 'sprite') {
        var layer = frame.layers[selectedLayer];
        if (!layer) throw new Error('The selected sprite layer is unavailable.');
        return {
          name: 'Frame ' + (sourceFrameIndex + 1), ticks: frame.ticks,
          layers: [{
            name: OB64.animationUI.layerDisplayLabel(layer,
              animation.artByKey[layer.sourceKey]),
            rgba: OB64.animationUI.singleLayerPixels(animation, layer,
              rom.art.animations, childOrdinal)
          }]
        };
      }
      return {
        name: 'Frame ' + (sourceFrameIndex + 1), ticks: frame.ticks,
        layers: frame.layers.map(function(layer, layerIndex) {
          return {
            name: OB64.animationUI.layerDisplayLabel(layer,
              animation.artByKey[layer.sourceKey]) || 'Layer ' + (layerIndex + 1),
            rgba: OB64.animationUI.singleLayerPixels(animation, layer,
              rom.art.animations, childOrdinal)
          };
        })
      };
    });
  }

  function cutsceneContext(rom) {
    if (rom.cutsceneStudio && rom.cutsceneStudio.catalog &&
        rom.cutsceneStudio.spriteState) {
      return { catalog: rom.cutsceneStudio.catalog, sprites: rom.cutsceneStudio.spriteState };
    }
    if (!OB64.cutsceneCatalog || !OB64.cutsceneSprites || !OB64.cutsceneData) {
      throw new Error('Cutscene actor art is unavailable.');
    }
    if (!rom.spriteEditorCutsceneContext) {
      var catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
      rom.spriteEditorCutsceneContext = {
        catalog: catalog,
        sprites: OB64.cutsceneSprites.create(rom.z64, catalog)
      };
    }
    return rom.spriteEditorCutsceneContext;
  }

  function replaceOptions(select, rows, valueFor, labelFor) {
    select.innerHTML = '';
    rows.forEach(function(row) {
      var option = element('option', '', labelFor(row));
      option.value = valueFor(row);
      select.appendChild(option);
    });
    if (select.selectedIndex < 0 && select.options.length) select.selectedIndex = 0;
  }

  function openKnownSourceModal(rom, state, ui, options, rerender) {
    var overlay = element('div', 'error-modal-overlay sprite-modal-overlay');
    var modal = element('div', 'error-modal sprite-source-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'sprite-source-title');
    overlay.appendChild(modal);
    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'Import Known Art Source');
    title.id = 'sprite-source-title'; header.appendChild(title);
    header.appendChild(button('×', 'error-modal-close', close));
    modal.appendChild(header);
    var body = element('div', 'error-modal-body sprite-source-body');
    body.appendChild(element('p', '',
      'Import a class avatar, item icon, Army sprite, combat animation, or cutscene actor sequence into the Sprite Library.'));
    var controls = element('div', 'sprite-source-controls');
    function selectField(labelText) {
      var label = element('label');
      label.appendChild(element('span', '', labelText));
      var select = element('select'); label.appendChild(select); controls.appendChild(label);
      return { label: label, select: select };
    }
    var kindField = selectField('Source');
    [['avatar', 'Class avatars'], ['icon', 'Item icons'],
      ['army', 'Army sprites'],
      ['combat', 'Combat animations'], ['cutscene', 'Cutscene actors']]
      .forEach(function(row) {
        var option = element('option', '', row[1]); option.value = row[0];
        kindField.select.appendChild(option);
      });
    var sourceField = selectField('Art source');
    var modeField = selectField('Import as');
    var frameField = selectField('Frame');
    var layerField = selectField('Sprite layer');
    var childField = selectField('Weapon / child sprite');
    var paletteField = selectField('Palette');
    [0, 1].forEach(function(paletteIndex) {
      var paletteOption = element('option', '', 'Palette ' + paletteIndex);
      paletteOption.value = String(paletteIndex);
      paletteField.select.appendChild(paletteOption);
    });
    var appearanceField = selectField('Appearance');
    for (var appearance = 0; appearance < 8; appearance++) {
      var appearanceOption = element('option', '', 'Appearance ' + appearance);
      appearanceOption.value = String(appearance);
      appearanceField.select.appendChild(appearanceOption);
    }
    body.appendChild(controls);
    var preview = element('div', 'sprite-source-preview');
    body.appendChild(preview);
    modal.appendChild(body);
    var footer = element('div', 'error-modal-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var importButton = button('Import Source', 'error-modal-ok', applyImport);
    footer.appendChild(importButton); modal.appendChild(footer);

    var currentRows = [];
    var cutscenePrograms = [];
    var currentAnimation = null;
    var currentName = '';

    function show(field, visible) { field.label.hidden = !visible; }

    function populateModes() {
      var kind = kindField.select.value;
      var modes = kind === 'avatar' || kind === 'icon' || kind === 'army'
        ? [['sprite', 'Sprite']]
        : [['sequence', 'Frame sequence'], ['frame', 'Complete frame'],
          ['sprite', 'One sprite layer']];
      replaceOptions(modeField.select, modes,
        function(row) { return row[0]; }, function(row) { return row[1]; });
    }

    function populateSources() {
      var kind = kindField.select.value;
      populateModes();
      show(appearanceField, kind === 'cutscene');
      show(paletteField, kind === 'army');
      if (kind === 'avatar') {
        currentRows = rom.art.avatar.appearances.slice();
        replaceOptions(sourceField.select, currentRows,
          function(row) { return row.key; },
          function(row) { return row.className + ' · ' + row.label; });
      } else if (kind === 'icon') {
        currentRows = rom.art.icons.icons.slice();
        replaceOptions(sourceField.select, currentRows,
          function(row) { return row.key; },
          function(row) { return row.packLabel + ' · ' + row.name; });
      } else if (kind === 'army') {
        currentRows = rom.art.armySprites && rom.art.armySprites.supported
          ? rom.art.armySprites.models.map(function(model) {
              return {
                key: model.key,
                model: model,
                atlas: rom.art.armySprites.byKey[model.atlasKey]
              };
            }) : [];
        replaceOptions(sourceField.select, currentRows,
          function(row) { return row.key; },
          function(row) {
            return row.atlas.label + ' · Model ' +
              OB64.armySprites.hex(row.model.modelId, 2) + ' · ' +
              (row.model.classNames.length
                ? row.model.classNames.join(', ') : 'unused plane');
          });
      } else if (kind === 'combat') {
        currentRows = animationSources(rom);
        replaceOptions(sourceField.select, currentRows,
          function(row) { return row.key; }, animationLabel);
      } else {
        var context = cutsceneContext(rom);
        cutscenePrograms = [];
        context.catalog.actorArtSources.forEach(function(source) {
          context.catalog.poseProgramsForBank(source.bank, { physical: true })
            .filter(function(program) {
              return program.programId && program.frames && program.frames.length;
            }).forEach(function(program) {
              cutscenePrograms.push({ source: source, program: program });
            });
        });
        currentRows = cutscenePrograms;
        replaceOptions(sourceField.select, currentRows,
          function(row) { return row.program.programId; },
          function(row) {
            return row.source.label + ' · Key ' + row.program.animationKey +
              ' · Facing ' + row.program.facing + ' · State ' + row.program.stateIndex;
          });
      }
      sourceField.select.selectedIndex = 0;
      populateFrames();
    }

    function selectedSourceRow() {
      var value = sourceField.select.value;
      if (kindField.select.value === 'cutscene') {
        return currentRows.find(function(row) { return row.program.programId === value; });
      }
      return currentRows.find(function(row) { return row.key === value; });
    }

    function loadAnimation() {
      var kind = kindField.select.value;
      var row = selectedSourceRow();
      if (kind === 'combat') {
        currentAnimation = row;
        currentName = row ? animationLabel(row) : '';
      } else if (kind === 'cutscene') {
        var context = cutsceneContext(rom);
        currentAnimation = row ? OB64.cutsceneSprites.actorSequence(
          context.sprites, row.program, Number(appearanceField.select.value)) : null;
        if (currentAnimation) registerAnimationSources(rom, currentAnimation);
        currentName = row ? row.source.label + ' · Key ' + row.program.animationKey +
          ' · Facing ' + row.program.facing + ' · State ' + row.program.stateIndex +
          ' · Appearance ' + appearanceField.select.value : '';
      } else {
        currentAnimation = null;
      }
    }

    function populateFrames() {
      loadAnimation();
      var kind = kindField.select.value;
      show(frameField, kind === 'combat' || kind === 'cutscene');
      show(layerField, (kind === 'combat' || kind === 'cutscene') &&
        modeField.select.value === 'sprite');
      show(childField, kind === 'combat');
      if (!currentAnimation) {
        frameField.select.innerHTML = '';
        layerField.select.innerHTML = '';
        updatePreview();
        return;
      }
      replaceOptions(frameField.select, currentAnimation.frames,
        function(frame) { return String(frame.sequenceIndex); },
        function(frame) { return 'Frame ' + (frame.sequenceIndex + 1) +
          ' · ' + frame.ticks + ' ticks'; });
      frameField.select.selectedIndex = 0;
      populateLayers();
    }

    function populateLayers() {
      if (!currentAnimation) return updatePreview();
      var frame = currentAnimation.frames[Number(frameField.select.value)] ||
        currentAnimation.frames[0];
      replaceOptions(layerField.select, frame.layers,
        function(layer) { return String(layer.ordinal); },
        function(layer) {
          return OB64.animationUI.layerDisplayLabel(layer,
            currentAnimation.artByKey[layer.sourceKey]);
        });
      layerField.select.selectedIndex = 0;
      populateChildren(frame);
    }

    function populateChildren(frame) {
      childField.select.innerHTML = '';
      var source = currentAnimation && OB64.animationUI.weaponSourceForFrame(
        currentAnimation, frame, null);
      var children = source && Array.isArray(source.selectableChildOrdinals)
        ? source.selectableChildOrdinals : [0];
      replaceOptions(childField.select, children,
        function(child) { return String(child); },
        function(child) { return 'Sprite ' + (child + 1); });
      childField.select.selectedIndex = 0;
      updatePreview();
    }

    function sourcePreviewRows() {
      var kind = kindField.select.value;
      var row = selectedSourceRow();
      if (!row) return [];
      if (kind === 'avatar') {
        return [{ name: row.className + ' · ' + row.label, width: 40, height: 48,
          ticks: 8, rgba: OB64.artUI.rgbaPixelsForWords(
            OB64.art.currentWords(rom.art, 'avatar', row.key)) }];
      }
      if (kind === 'icon') {
        return [{ name: row.name, width: 16, height: 16, ticks: 8,
          rgba: OB64.artUI.rgbaPixelsForWords(
            OB64.art.currentWords(rom.art, 'icon', row.key)) }];
      }
      if (kind === 'army') {
        var armySource = OB64.armySpriteUI.sourceForModel(
          rom.art.armySprites, row.atlas.key, row.model.modelId,
          Number(paletteField.select.value));
        return [{
          name: armySource.name,
          width: armySource.width,
          height: armySource.height,
          ticks: 8,
          rgba: armySource.rgba
        }];
      }
      if (!currentAnimation) return [];
      var mode = modeField.select.value;
      var frames = mode === 'sequence' ? currentAnimation.frames : [
        currentAnimation.frames[Number(frameField.select.value)] || currentAnimation.frames[0]
      ];
      return frames.map(function(frame) {
        var rgba;
        if (mode === 'sprite') {
          var layer = frame.layers.find(function(candidate) {
            return candidate.ordinal === Number(layerField.select.value);
          }) || frame.layers[0];
          rgba = OB64.animationUI.singleLayerPixels(currentAnimation, layer,
            rom.art.animations, Number(childField.select.value) || 0);
        } else {
          rgba = OB64.animationUI.framePixels(currentAnimation, frame,
            rom.art.animations, null, null,
            Number(childField.select.value) || 0);
        }
        return { name: 'Frame ' + (frame.sequenceIndex + 1),
          width: currentAnimation.canvas.width,
          height: currentAnimation.canvas.height,
          ticks: frame.ticks, rgba: rgba };
      });
    }

    function updatePreview() {
      preview.innerHTML = '';
      var rows;
      try {
        rows = sourcePreviewRows();
      } catch (error) {
        importButton.disabled = true;
        preview.appendChild(element('p', 'sprite-source-error', error.message));
        return;
      }
      importButton.disabled = !rows.length;
      if (!rows.length) {
        preview.appendChild(element('p', '', 'No renderable source is available.'));
        return;
      }
      var strip = element('div', 'sprite-source-frame-strip');
      rows.forEach(function(row) {
        var figure = element('figure');
        var canvas = element('canvas');
        var scale = Math.max(1, Math.min(4,
          Math.floor(128 / Math.max(row.width, row.height))));
        paintPixels(canvas, row.width, row.height, row.rgba, scale,
          ui.background, false);
        figure.appendChild(canvas);
        figure.appendChild(element('figcaption', '', row.name + ' · ' + row.ticks + ' ticks'));
        strip.appendChild(figure);
      });
      preview.appendChild(strip);
    }

    function applyImport() {
      try {
        var kind = kindField.select.value;
        var row = selectedSourceRow();
        var mode = modeField.select.value;
        var asset;
        if (kind === 'avatar') {
          asset = L.assetFromRgba(state, {
            name: row.className + ' ' + row.label,
            kind: 'sprite', width: 40, height: 48,
            rgba: OB64.artUI.rgbaPixelsForWords(
              OB64.art.currentWords(rom.art, 'avatar', row.key)),
            provenance: { source: 'class-avatar', key: row.key,
              label: row.className + ' · ' + row.label }
          });
        } else if (kind === 'icon') {
          asset = L.assetFromRgba(state, {
            name: row.name + ' Icon', kind: 'sprite', width: 16, height: 16,
            rgba: OB64.artUI.rgbaPixelsForWords(
              OB64.art.currentWords(rom.art, 'icon', row.key)),
            provenance: { source: 'item-icon', key: row.key,
              label: row.packLabel + ' · ' + row.name }
          });
        } else if (kind === 'army') {
          var armySource = OB64.armySpriteUI.sourceForModel(
            rom.art.armySprites, row.atlas.key, row.model.modelId,
            Number(paletteField.select.value));
          asset = L.assetFromRgba(state, {
            name: armySource.name,
            kind: 'sprite', width: armySource.width, height: armySource.height,
            rgba: armySource.rgba,
            provenance: {
              source: 'army-sprite', key: row.key,
              atlas: row.atlas.key, modelId: row.model.modelId,
              paletteIndex: armySource.paletteIndex,
              label: armySource.name
            }
          });
        } else {
          var frames = framesFromAnimation(rom, currentAnimation, mode,
            Number(frameField.select.value) || 0,
            Number(layerField.select.value) || 0,
            Number(childField.select.value) || 0);
          asset = L.assetFromFrames(state, {
            name: currentName,
            kind: mode === 'sequence' ? 'sequence' : mode,
            width: currentAnimation.canvas.width,
            height: currentAnimation.canvas.height,
            frames: frames,
            provenance: { source: kind === 'combat'
              ? 'combat-animation' : 'cutscene-actor',
              key: currentAnimation.key, label: currentName }
          });
        }
        asset = L.addAsset(state, asset);
        ui.assetId = asset.id; ui.frameIndex = 0; ui.layerIndex = 0;
        changed(options);
        notify(options, 'Imported ' + asset.name + ' into the Sprite Library.');
        close(); rerender();
      } catch (error) {
        notify(options, 'Known art source import blocked: ' + error.message);
      }
    }

    kindField.select.addEventListener('change', populateSources);
    sourceField.select.addEventListener('change', populateFrames);
    modeField.select.addEventListener('change', populateFrames);
    frameField.select.addEventListener('change', populateLayers);
    layerField.select.addEventListener('change', updatePreview);
    childField.select.addEventListener('change', updatePreview);
    paletteField.select.addEventListener('change', updatePreview);
    appearanceField.select.addEventListener('change', populateFrames);
    overlay.addEventListener('click', function(event) { if (event.target === overlay) close(); });
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escape);
    }
    function escape(event) { if (event.key === 'Escape') close(); }
    document.addEventListener('keydown', escape);
    document.body.appendChild(overlay);
    populateSources();
    kindField.select.focus();
  }

  function browser(rom, state, ui, options, rerender) {
    var sidebar = element('aside', 'sprite-library-browser');
    var actions = element('div', 'sprite-library-actions');
    actions.appendChild(button('New Sprite', 'btn-secondary', function() {
      openNewAssetModal(rom, state, ui, options, rerender);
    }));
    actions.appendChild(button('Import Known Source', 'btn-secondary', function() {
      openKnownSourceModal(rom, state, ui, options, rerender);
    }));
    var imageInput = imageImportInput(state, ui, options, rerender);
    actions.appendChild(imageInput);
    actions.appendChild(button('Import PNG/JPEG', 'btn-secondary', function() {
      imageInput.click();
    }));
    var fileInput = assetFileInput(state, ui, options, rerender);
    actions.appendChild(fileInput);
    actions.appendChild(button('Import Asset File', 'btn-secondary', function() {
      fileInput.click();
    }));
    sidebar.appendChild(actions);
    var search = element('input', 'sprite-library-search');
    search.type = 'search'; search.placeholder = 'Filter Project assets…';
    search.value = ui.search || '';
    search.setAttribute('data-sprite-focus-key', 'library-search');
    search.addEventListener('input', function() {
      ui.search = search.value;
      rerender();
    });
    sidebar.appendChild(search);
    var query = String(ui.search || '').trim().toLowerCase();
    var rows = state.assets.filter(function(asset) {
      return !query || asset.name.toLowerCase().indexOf(query) >= 0 ||
        asset.kind.indexOf(query) >= 0 || asset.id.toLowerCase().indexOf(query) >= 0;
    });
    sidebar.appendChild(element('p', 'sprite-library-summary', rows.length +
      ' Project asset' + (rows.length === 1 ? '' : 's')));
    var list = element('div', 'sprite-library-list');
    list.setAttribute('data-sprite-scroll-key', 'sprite:library');
    rows.forEach(function(asset) {
      var card = button('', 'sprite-library-card' +
        (asset.id === ui.assetId ? ' selected' : ''), function() {
          ui.assetId = asset.id; ui.frameIndex = 0; ui.layerIndex = 0;
          ui.selection = null; rerender();
        });
      var canvas = element('canvas');
      var scale = Math.max(1, Math.min(3,
        Math.floor(64 / Math.max(asset.width, asset.height))));
      paintPixels(canvas, asset.width, asset.height,
        L.compositeFrame(asset, 0), scale, ui.background, false);
      card.appendChild(canvas);
      var copy = element('span');
      copy.appendChild(element('strong', '', asset.name));
      copy.appendChild(element('small', '', asset.kind + ' · ' +
        asset.width + '×' + asset.height + ' · ' + asset.frames.length +
        (asset.frames.length === 1 ? ' frame' : ' frames')));
      card.appendChild(copy);
      list.appendChild(card);
    });
    if (!rows.length) list.appendChild(element('p', 'sprite-library-empty',
      state.assets.length ? 'No assets match this filter.' :
        'Create a sprite or import a known art source.'));
    sidebar.appendChild(list);
    var bottom = element('div', 'sprite-library-bottom-actions');
    var duplicate = button('Duplicate Asset', 'btn-secondary', function() {
      var added = L.duplicateAsset(state, ui.assetId);
      ui.assetId = added.id; ui.frameIndex = 0; ui.layerIndex = 0;
      changed(options); rerender();
    });
    duplicate.disabled = !selectedAsset(state, ui);
    bottom.appendChild(duplicate);
    var remove = button('Delete Asset', 'btn-secondary sprite-danger', function() {
      var asset = selectedAsset(state, ui);
      if (!asset || !window.confirm('Delete Project sprite asset “' + asset.name + '”?')) return;
      L.removeAsset(state, asset.id);
      changed(options); rerender();
    });
    remove.disabled = !selectedAsset(state, ui);
    bottom.appendChild(remove);
    sidebar.appendChild(bottom);
    return sidebar;
  }

  function selectedAssetSource(asset, frameIndex, layerIndex, layerOnly) {
    var rgba = layerOnly
      ? new Uint8ClampedArray(asset.frames[frameIndex].layers[layerIndex].pixels)
      : L.compositeFrame(asset, frameIndex);
    return {
      name: asset.name,
      format: 'Sprite Library',
      width: asset.width,
      height: asset.height,
      rgba: rgba,
      asset: asset,
      frameIndex: frameIndex,
      layerIndex: layerIndex
    };
  }

  function openLibraryPicker(rom, options, onSelect) {
    options = options || {};
    var state = rom && rom.spriteLibrary;
    if (!state || !state.assets.length) {
      if (options.onStatus) options.onStatus(
        'The Sprite Library is empty. Create or import an asset in the Sprite Editor first.');
      return false;
    }
    L.indexState(state);
    var overlay = element('div', 'error-modal-overlay sprite-modal-overlay');
    var modal = element('div', 'error-modal sprite-picker-modal');
    modal.setAttribute('role', 'dialog'); modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'sprite-picker-title');
    overlay.appendChild(modal);
    var header = element('div', 'error-modal-header');
    var title = element('h2', '', options.title || 'Choose Sprite Library Asset');
    title.id = 'sprite-picker-title'; header.appendChild(title);
    header.appendChild(button('×', 'error-modal-close', close)); modal.appendChild(header);
    var body = element('div', 'error-modal-body sprite-picker-body');
    var assetSelect = element('select');
    state.assets.filter(function(asset) {
      return !options.kinds || options.kinds.indexOf(asset.kind) >= 0;
    }).forEach(function(asset) {
      var option = element('option', '', asset.name + ' · ' + asset.kind + ' · ' +
        asset.width + '×' + asset.height);
      option.value = asset.id; assetSelect.appendChild(option);
    });
    body.appendChild(assetSelect);
    var frameStripNode = element('div', 'sprite-picker-frames');
    body.appendChild(frameStripNode);
    var layerWrap = element('label', 'sprite-picker-layer');
    layerWrap.appendChild(element('span', '', 'Layer'));
    var layerSelect = element('select'); layerWrap.appendChild(layerSelect);
    layerWrap.hidden = !options.layerOnly; body.appendChild(layerWrap);
    var preview = element('div', 'sprite-picker-preview'); body.appendChild(preview);
    modal.appendChild(body);
    var footer = element('div', 'error-modal-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var use = button(options.actionLabel || 'Use Asset', 'error-modal-ok', function() {
      var asset = state.byId[assetSelect.value];
      if (!asset) return;
      var source = selectedAssetSource(asset, selectedFrame,
        Number(layerSelect.value) || 0, !!options.layerOnly);
      close(); onSelect(source);
    });
    footer.appendChild(use); modal.appendChild(footer);
    var selectedFrame = 0;
    var selectedLayer = 0;
    function renderPicker() {
      var asset = state.byId[assetSelect.value];
      frameStripNode.innerHTML = ''; layerSelect.innerHTML = ''; preview.innerHTML = '';
      use.disabled = !asset;
      if (!asset) return;
      selectedFrame = clamp(selectedFrame, 0, asset.frames.length - 1);
      selectedLayer = clamp(selectedLayer, 0,
        asset.frames[selectedFrame].layers.length - 1);
      asset.frames.forEach(function(frame, index) {
        var card = button('', 'sprite-frame-card' +
          (index === selectedFrame ? ' selected' : ''), function() {
            selectedFrame = index; renderPicker();
          });
        var canvas = element('canvas');
        var scale = Math.max(1, Math.min(3,
          Math.floor(80 / Math.max(asset.width, asset.height))));
        paintPixels(canvas, asset.width, asset.height,
          L.compositeFrame(asset, index), scale, 'checkerboard', false);
        card.appendChild(canvas);
        card.appendChild(element('span', '', 'Frame ' + (index + 1)));
        frameStripNode.appendChild(card);
      });
      asset.frames[selectedFrame].layers.forEach(function(layer, index) {
        var option = element('option', '', layer.name);
        option.value = String(index); layerSelect.appendChild(option);
      });
      layerSelect.value = String(selectedLayer);
      var source = selectedAssetSource(asset, selectedFrame,
        selectedLayer, !!options.layerOnly);
      var previewCanvas = element('canvas');
      var previewScale = Math.max(1, Math.min(6,
        Math.floor(320 / Math.max(asset.width, asset.height))));
      paintPixels(previewCanvas, asset.width, asset.height, source.rgba,
        previewScale, 'checkerboard', false);
      preview.appendChild(previewCanvas);
    }
    assetSelect.addEventListener('change', function() {
      selectedFrame = 0; selectedLayer = 0; renderPicker();
    });
    layerSelect.addEventListener('change', function() {
      selectedLayer = Number(layerSelect.value) || 0;
      renderPicker();
    });
    overlay.addEventListener('click', function(event) { if (event.target === overlay) close(); });
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escape);
    }
    function escape(event) { if (event.key === 'Escape') close(); }
    document.addEventListener('keydown', escape);
    document.body.appendChild(overlay);
    renderPicker(); assetSelect.focus();
    return true;
  }

  function render(panel, rom, options, preserveViewport) {
    var state = rom && rom.spriteLibrary;
    if (!state) {
      panel.innerHTML = '';
      var unavailable = element('div', 'art-unavailable');
      unavailable.appendChild(element('h2', '', 'Sprite Editor'));
      unavailable.appendChild(element('p', '',
        'Load a supported ROM before opening the Sprite Editor.'));
      panel.appendChild(unavailable);
      return;
    }
    var ui = ensureUi(state);
    var active = document.activeElement;
    var focus = null;
    if (active && panel.contains(active)) {
      var key = active.getAttribute('data-sprite-focus-key');
      if (key) focus = {
        key: key,
        start: typeof active.selectionStart === 'number' ? active.selectionStart : null,
        end: typeof active.selectionEnd === 'number' ? active.selectionEnd : null
      };
    }
    captureScroll(panel, ui, preserveViewport);
    panel.innerHTML = '';
    function rerender() { render(panel, rom, options, true); }
    var heading = element('div', 'art-heading sprite-heading');
    var headingCopy = element('div');
    headingCopy.appendChild(element('h2', '', 'Sprite Editor'));
    headingCopy.appendChild(element('p', '',
      'Build reusable sprites, frames, and frame sequences. Compatible Project assets change ROM bytes only after another editor imports them.'));
    heading.appendChild(headingCopy);
    heading.appendChild(element('span', 'art-badge art-badge-count',
      state.assets.length + (state.assets.length === 1 ? ' asset' : ' assets')));
    panel.appendChild(heading);
    var shell = element('div', 'sprite-editor-shell');
    shell.appendChild(browser(rom, state, ui, options, rerender));
    var asset = selectedAsset(state, ui);
    if (asset) shell.appendChild(editor(state, asset, ui, options, rerender));
    else {
      var empty = element('div', 'sprite-editor-empty');
      empty.appendChild(element('h3', '', 'Create or import a sprite asset'));
      empty.appendChild(element('p', '',
        'Known sources include class avatars, item icons, combat animations, cutscene actors, PNG, JPEG, and Sprite Editor files.'));
      empty.appendChild(button('New Sprite', 'btn-secondary', function() {
        openNewAssetModal(rom, state, ui, options, rerender);
      }));
      shell.appendChild(empty);
    }
    panel.appendChild(shell);
    restoreScroll(panel, ui, preserveViewport);
    if (focus) {
      var replacement = panel.querySelector('[data-sprite-focus-key="' + focus.key + '"]');
      if (replacement) {
        replacement.focus({ preventScroll: true });
        if (focus.start !== null && replacement.setSelectionRange) {
          replacement.setSelectionRange(focus.start, focus.end);
        }
      }
    }
  }

  OB64.spriteEditorUI = {
    render: render,
    paintPixels: paintPixels,
    flood: flood,
    applyBrush: applyBrush,
    captureScroll: captureScroll,
    restoreScroll: restoreScroll,
    selectedAssetSource: selectedAssetSource,
    openLibraryPicker: openLibraryPicker,
    framesFromAnimation: framesFromAnimation,
    knownSpriteTemplates: knownSpriteTemplates,
    spriteImageColorLimit: spriteImageColorLimit,
    webmSupported: webmSupported
  };
})();
