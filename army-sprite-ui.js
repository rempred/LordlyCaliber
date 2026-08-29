// OB64 Mod Editor - Army sprite tab UI

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var A = OB64.art;
  var M = OB64.armySprites;

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
    if (options && options.onStatus) options.onStatus(message);
  }

  function changed(options) {
    if (options && options.onChange) options.onChange();
  }

  function previewBackground(ui) {
    return OB64.artUI.previewBackgroundMode(ui);
  }

  function ensureUi(army, ui) {
    if (ui.armyBrowseMode !== 'atlas' && ui.armyBrowseMode !== 'classes') {
      ui.armyBrowseMode = 'classes';
    }
    if (ui.armySide !== 'enemy' && ui.armySide !== 'player') {
      ui.armySide = 'player';
    }
    if (!Number.isInteger(ui.armyClassId) ||
        !army.byClassId[ui.armyClassId]) {
      ui.armyClassId = army.classRoutes[0].classId;
    }
    if (ui.armyBrowseMode === 'classes') {
      var route = army.byClassId[ui.armyClassId];
      ui.armyAtlasKey = ui.armySide === 'player'
        ? route.playerAtlasKey : route.enemyAtlasKey;
      ui.armyModelId = route.modelId;
    }
    var atlas = army.byKey[ui.armyAtlasKey] || army.atlases[0];
    ui.armyAtlasKey = atlas.key;
    if (!Number.isInteger(ui.armyModelId) || ui.armyModelId < 0 ||
        ui.armyModelId >= atlas.modelCount) ui.armyModelId = 0;
    if (ui.armyPaletteIndex !== 0 && ui.armyPaletteIndex !== 1) {
      ui.armyPaletteIndex = 0;
    }
    if (!Number.isInteger(ui.armySelectedIndex) ||
        ui.armySelectedIndex < 0 || ui.armySelectedIndex > 255) {
      ui.armySelectedIndex = atlas.palettes[ui.armyPaletteIndex]
        .findIndex(function(word) { return !!(word & 1); });
    }
    if (!ui.armyTool) ui.armyTool = 'pencil';
    if (typeof ui.armySearch !== 'string') ui.armySearch = '';
    if (typeof ui.armyModifiedOnly !== 'boolean') ui.armyModifiedOnly = false;
    return atlas;
  }

  function drawPlane(canvas, atlas, indices, paletteIndex, scale, selection, ui) {
    OB64.artUI.drawWords(canvas,
      M.renderWords(atlas, indices, paletteIndex),
      atlas.width, atlas.height, scale, selection, previewBackground(ui));
  }

  function planeCanvas(atlas, indices, paletteIndex, scale, className, ui,
      selection) {
    var canvas = element('canvas', className || 'art-pixel-canvas');
    drawPlane(canvas, atlas, indices, paletteIndex, scale, selection, ui);
    return canvas;
  }

  function atlasLabel(atlas) {
    return atlas.label + ' · ' + atlas.width + '×' + atlas.height +
      ' · ' + atlas.retailModelCount + ' retail' +
      (atlas.targetModelCount === atlas.retailModelCount
        ? '' : ' / ' + atlas.targetModelCount + ' routed');
  }

  function modelTitle(model) {
    return 'Model ' + M.hex(model.modelId, 2);
  }

  function classSummary(model) {
    return model.classNames.length
      ? model.classNames.join(', ')
      : 'No class route uses this plane.';
  }

  function browseModeTabs(sidebar, ui, rerender) {
    var tabs = element('div', 'art-pack-tabs army-browse-tabs');
    [['classes', 'Classes'], ['atlas', 'Atlas Planes']].forEach(function(row) {
      tabs.appendChild(button(row[1], ui.armyBrowseMode === row[0]
        ? 'active' : '', function() {
        ui.armyBrowseMode = row[0];
        ui.armySelection = null;
        rerender();
      }));
    });
    sidebar.appendChild(tabs);
  }

  function appendFilters(sidebar, ui, rerender, placeholder) {
    var filters = element('div', 'art-filter-controls');
    var search = element('input', 'art-search');
    search.type = 'search';
    search.placeholder = placeholder;
    search.value = ui.armySearch;
    search.setAttribute('data-art-focus-key', 'army-search');
    search.addEventListener('input', function() {
      ui.armySearch = search.value;
      rerender();
    });
    filters.appendChild(search);
    var modifiedLabel = element('label', 'art-modified-filter');
    var modified = element('input');
    modified.type = 'checkbox';
    modified.checked = ui.armyModifiedOnly;
    modified.addEventListener('change', function() {
      ui.armyModifiedOnly = modified.checked;
      rerender();
    });
    modifiedLabel.appendChild(modified);
    modifiedLabel.appendChild(document.createTextNode(' Modified only'));
    filters.appendChild(modifiedLabel);
    sidebar.appendChild(filters);
  }

  function classCardSummary(route, side) {
    return (side === 'player' ? 'Player / Back' : 'Enemy / Front') +
      ' · ' + route.lane[0].toUpperCase() + route.lane.slice(1) +
      ' · Model ' + M.hex(route.modelId, 2);
  }

  function classBrowser(army, ui, rerender, sidebar) {
    var sideTabs = element('div', 'art-pack-tabs army-side-tabs');
    [['player', 'Player / Back'], ['enemy', 'Enemy / Front']]
      .forEach(function(row) {
        sideTabs.appendChild(button(row[1], ui.armySide === row[0]
          ? 'active' : '', function() {
          ui.armySide = row[0];
          ui.armySelection = null;
          rerender();
        }));
      });
    sidebar.appendChild(sideTabs);
    appendFilters(sidebar, ui, rerender, 'Search class or model ID');
    var query = ui.armySearch.trim().toLowerCase();
    var rows = army.classRoutes.filter(function(route) {
      var model = ui.armySide === 'player'
        ? route.playerModel : route.enemyModel;
      if (ui.armyModifiedOnly && !army.edits[model.key]) return false;
      return !query || route.className.toLowerCase().indexOf(query) >= 0 ||
        M.hex(route.classId, 2).toLowerCase().indexOf(query) >= 0 ||
        M.hex(route.modelId, 2).toLowerCase().indexOf(query) >= 0 ||
        String(route.classId).indexOf(query) >= 0 ||
        String(route.modelId).indexOf(query) >= 0;
    });
    sidebar.appendChild(element('div', 'art-browser-summary', rows.length +
      ' class' + (rows.length === 1 ? '' : 'es')));
    var list = element('div', 'art-browser-list army-model-list');
    list.setAttribute('data-art-scroll-key', 'army:classes:' + ui.armySide);
    rows.forEach(function(route) {
      var atlas = army.byKey[ui.armySide === 'player'
        ? route.playerAtlasKey : route.enemyAtlasKey];
      var model = ui.armySide === 'player'
        ? route.playerModel : route.enemyModel;
      var present = M.hasCurrentPlane(army, atlas.key, model.modelId);
      var card = button('', 'art-browser-card army-class-card' +
        (ui.armyClassId === route.classId ? ' selected' : '') +
        (!present ? ' missing' : ''), function() {
        ui.armyClassId = route.classId;
        ui.armyAtlasKey = atlas.key;
        ui.armyModelId = model.modelId;
        ui.armySelection = null;
        rerender();
      });
      card.appendChild(planeCanvas(atlas,
        M.currentIndices(army, atlas.key, model.modelId),
        ui.armyPaletteIndex, 3, 'art-list-icon army-list-sprite', ui));
      var copy = element('span', 'art-browser-copy');
      copy.appendChild(element('strong', '', M.hex(route.classId, 2) +
        ' · ' + route.className));
      copy.appendChild(element('small', '', classCardSummary(route,
        ui.armySide)));
      if (!present) {
        copy.appendChild(element('span', 'art-badge army-badge-missing',
          ui.armySide === 'player'
            ? 'No player sprite' : 'No enemy sprite'));
      } else if (!model.retailPresent) {
        copy.appendChild(element('span', 'art-badge army-badge-custom',
          'Custom player sprite'));
      } else if (army.edits[model.key]) {
        copy.appendChild(element('span', 'art-badge art-badge-edited',
          'Edited'));
      }
      card.appendChild(copy);
      list.appendChild(card);
    });
    if (!rows.length) {
      list.appendChild(element('p', 'art-browser-empty',
        'No classes match this filter.'));
    }
    sidebar.appendChild(list);
  }

  function atlasBrowser(army, ui, rerender, sidebar) {
    var atlas = ensureUi(army, ui);
    var atlasLabelNode = element('label', 'army-atlas-select');
    atlasLabelNode.appendChild(element('span', '', 'Atlas'));
    var atlasSelect = element('select');
    army.atlases.forEach(function(row) {
      var option = element('option', '', atlasLabel(row));
      option.value = row.key;
      atlasSelect.appendChild(option);
    });
    atlasSelect.value = atlas.key;
    atlasSelect.setAttribute('data-art-focus-key', 'army-atlas');
    atlasSelect.addEventListener('change', function() {
      ui.armyAtlasKey = atlasSelect.value;
      ui.armyModelId = 0;
      ui.armySelection = null;
      var selectedAtlas = army.byKey[ui.armyAtlasKey];
      ui.armySelectedIndex = selectedAtlas.palettes[ui.armyPaletteIndex]
        .findIndex(function(word) { return !!(word & 1); });
      rerender();
    });
    atlasLabelNode.appendChild(atlasSelect);
    sidebar.appendChild(atlasLabelNode);

    appendFilters(sidebar, ui, rerender, 'Search class or model ID');

    var query = ui.armySearch.trim().toLowerCase();
    var rows = atlas.models.filter(function(model) {
      if (ui.armyModifiedOnly && !army.edits[model.key]) return false;
      return !query || modelTitle(model).toLowerCase().indexOf(query) >= 0 ||
        M.hex(model.modelId, 2).toLowerCase().indexOf(query) >= 0 ||
        String(model.modelId).indexOf(query) >= 0 ||
        model.classNames.some(function(name) {
          return name.toLowerCase().indexOf(query) >= 0;
        });
    });
    sidebar.appendChild(element('div', 'art-browser-summary', rows.length +
      ' plane' + (rows.length === 1 ? '' : 's')));
    var list = element('div', 'art-browser-list army-model-list');
    list.setAttribute('data-art-scroll-key', 'army:' + atlas.key);
    rows.forEach(function(model) {
      var card = button('', 'art-browser-card' +
        (ui.armyModelId === model.modelId ? ' selected' : '') +
        (!M.hasCurrentPlane(army, atlas.key, model.modelId)
          ? ' missing' : ''), function() {
        ui.armyModelId = model.modelId;
        ui.armySelection = null;
        rerender();
      });
      card.appendChild(planeCanvas(atlas,
        M.currentIndices(army, atlas.key, model.modelId),
        ui.armyPaletteIndex, 3, 'art-list-icon army-list-sprite', ui));
      var copy = element('span', 'art-browser-copy');
      copy.appendChild(element('strong', '', modelTitle(model)));
      copy.appendChild(element('small', '', classSummary(model)));
      if (!M.hasCurrentPlane(army, atlas.key, model.modelId)) {
        copy.appendChild(element('span', 'art-badge army-badge-missing',
          'Missing in retail'));
      } else if (!model.retailPresent) {
        copy.appendChild(element('span', 'art-badge army-badge-custom',
          'Custom'));
      } else if (army.edits[model.key]) {
        copy.appendChild(element('span', 'art-badge art-badge-edited', 'Edited'));
      }
      card.appendChild(copy);
      list.appendChild(card);
    });
    if (!rows.length) {
      list.appendChild(element('p', 'art-browser-empty',
        'No Army sprite planes match this filter.'));
    }
    sidebar.appendChild(list);
    return sidebar;
  }

  function browser(army, ui, rerender) {
    ensureUi(army, ui);
    var sidebar = element('aside', 'art-browser army-sprite-browser');
    browseModeTabs(sidebar, ui, rerender);
    if (ui.armyBrowseMode === 'classes') {
      classBrowser(army, ui, rerender, sidebar);
      return sidebar;
    }
    return atlasBrowser(army, ui, rerender, sidebar);
  }

  function coordinate(canvas, event, width, height) {
    var rect = canvas.getBoundingClientRect();
    return {
      x: Math.max(0, Math.min(width - 1,
        Math.floor((event.clientX - rect.left) * width / rect.width))),
      y: Math.max(0, Math.min(height - 1,
        Math.floor((event.clientY - rect.top) * height / rect.height)))
    };
  }

  function flood(indices, width, start, replacement) {
    var original = indices[start];
    if (original === replacement) return false;
    var queue = [start];
    var seen = new Uint8Array(indices.length);
    seen[start] = 1;
    var changedAny = false;
    while (queue.length) {
      var index = queue.pop();
      if (indices[index] !== original) continue;
      indices[index] = replacement;
      changedAny = true;
      var x = index % width;
      var neighbors = [index - width, index + width];
      if (x) neighbors.push(index - 1);
      if (x + 1 < width) neighbors.push(index + 1);
      neighbors.forEach(function(next) {
        if (next >= 0 && next < indices.length && !seen[next]) {
          seen[next] = 1;
          queue.push(next);
        }
      });
    }
    return changedAny;
  }

  function paintIndex(atlas, ui) {
    return ui.armyTool === 'eraser'
      ? atlas.transparentIndices[0]
      : ui.armySelectedIndex;
  }

  function installCanvas(canvas, army, atlas, model, scale, ui, options,
      rerender) {
    var drawing = false;
    var working = null;
    var selectionStart = null;
    var changedPixels = false;
    canvas.tabIndex = 0;
    canvas.setAttribute('data-art-focus-key', 'army-edit-canvas');

    function applyPoint(point) {
      var index = point.y * atlas.width + point.x;
      var replacement = paintIndex(atlas, ui);
      if (working[index] !== replacement) {
        working[index] = replacement;
        changedPixels = true;
      }
    }

    canvas.addEventListener('pointerdown', function(event) {
      event.preventDefault();
      canvas.focus({ preventScroll: true });
      var point = coordinate(canvas, event, atlas.width, atlas.height);
      var pixel = point.y * atlas.width + point.x;
      var current = M.currentIndices(army, atlas.key, model.modelId);
      if (ui.armyTool === 'eyedropper') {
        ui.armySelectedIndex = current[pixel];
        ui.armyTool = 'pencil';
        rerender();
        return;
      }
      if (ui.armyTool === 'fill' || ui.armyTool === 'replace') {
        var next = current.slice();
        var didChange = false;
        if (ui.armyTool === 'fill') {
          didChange = flood(next, atlas.width, pixel, paintIndex(atlas, ui));
        } else {
          var source = next[pixel];
          var replacement = paintIndex(atlas, ui);
          if (source !== replacement) {
            for (var index = 0; index < next.length; index++) {
              if (next[index] === source) {
                next[index] = replacement;
                didChange = true;
              }
            }
          }
        }
        if (didChange && M.setEdit(army, atlas.key, model.modelId, next)) {
          changed(options);
          rerender();
        }
        return;
      }
      if (ui.armyTool === 'select') {
        drawing = true;
        selectionStart = point;
        ui.armySelection = { x: point.x, y: point.y, width: 1, height: 1 };
        canvas.setPointerCapture(event.pointerId);
        drawPlane(canvas, atlas, current, ui.armyPaletteIndex, scale,
          ui.armySelection, ui);
        return;
      }
      drawing = true;
      working = current.slice();
      changedPixels = false;
      canvas.setPointerCapture(event.pointerId);
      applyPoint(point);
      drawPlane(canvas, atlas, working, ui.armyPaletteIndex, scale,
        ui.armySelection, ui);
    });

    canvas.addEventListener('pointermove', function(event) {
      if (!drawing) return;
      var point = coordinate(canvas, event, atlas.width, atlas.height);
      if (ui.armyTool === 'select') {
        var left = Math.min(selectionStart.x, point.x);
        var top = Math.min(selectionStart.y, point.y);
        ui.armySelection = {
          x: left,
          y: top,
          width: Math.max(selectionStart.x, point.x) - left + 1,
          height: Math.max(selectionStart.y, point.y) - top + 1
        };
        drawPlane(canvas, atlas,
          M.currentIndices(army, atlas.key, model.modelId),
          ui.armyPaletteIndex, scale, ui.armySelection, ui);
      } else {
        applyPoint(point);
        drawPlane(canvas, atlas, working, ui.armyPaletteIndex, scale,
          ui.armySelection, ui);
      }
    });

    function finish(event) {
      if (!drawing) return;
      drawing = false;
      if (event && canvas.hasPointerCapture(event.pointerId)) {
        canvas.releasePointerCapture(event.pointerId);
      }
      if (ui.armyTool === 'select') {
        rerender();
        return;
      }
      if (changedPixels && M.setEdit(
          army, atlas.key, model.modelId, working)) {
        changed(options);
        rerender();
      }
    }
    canvas.addEventListener('pointerup', finish);
    canvas.addEventListener('pointercancel', finish);
    canvas.addEventListener('lostpointercapture', finish);
    canvas.addEventListener('keydown', function(event) {
      if (!(event.ctrlKey || event.metaKey)) return;
      var key = event.key.toLowerCase();
      if (key === 'z' && !event.shiftKey) {
        event.preventDefault();
        if (M.undo(army, atlas.key, model.modelId)) {
          changed(options); rerender();
        }
      } else if (key === 'y' || (key === 'z' && event.shiftKey)) {
        event.preventDefault();
        if (M.redo(army, atlas.key, model.modelId)) {
          changed(options); rerender();
        }
      }
    });
  }

  function toolbox(army, atlas, model, ui, options, rerender) {
    var bar = element('div', 'art-toolbox');
    [['pencil', 'Pencil'], ['eraser', 'Eraser'], ['fill', 'Fill'],
      ['eyedropper', 'Eyedropper'], ['replace', 'Replace Color'],
      ['select', 'Select']].forEach(function(row) {
      bar.appendChild(button(row[1], ui.armyTool === row[0] ? 'active' : '',
        function() {
          ui.armyTool = row[0];
          rerender();
        }));
    });
    var history = M.historyFor(army, atlas.key, model.modelId);
    var undo = button('Undo', 'btn-secondary', function() {
      if (M.undo(army, atlas.key, model.modelId)) {
        changed(options); rerender();
      }
    });
    undo.disabled = !history.undo.length;
    bar.appendChild(undo);
    var redo = button('Redo', 'btn-secondary', function() {
      if (M.redo(army, atlas.key, model.modelId)) {
        changed(options); rerender();
      }
    });
    redo.disabled = !history.redo.length;
    bar.appendChild(redo);

    var copy = button('Copy', 'btn-secondary', function() {
      if (!ui.armySelection) return;
      var current = M.currentIndices(army, atlas.key, model.modelId);
      var selection = ui.armySelection;
      var pixels = new Uint8Array(selection.width * selection.height);
      var cursor = 0;
      for (var y = 0; y < selection.height; y++) {
        for (var x = 0; x < selection.width; x++) {
          pixels[cursor++] = current[(selection.y + y) * atlas.width +
            selection.x + x];
        }
      }
      ui.armyClipboard = {
        width: selection.width,
        height: selection.height,
        indices: pixels,
        atlasKey: atlas.key
      };
      notify(options, 'Copied ' + pixels.length + ' Army sprite pixels.');
      rerender();
    });
    copy.disabled = !ui.armySelection;
    bar.appendChild(copy);
    var paste = button('Paste', 'btn-secondary', function() {
      if (!ui.armySelection || !ui.armyClipboard) return;
      var current = M.currentIndices(army, atlas.key, model.modelId).slice();
      var selection = ui.armySelection;
      var clip = ui.armyClipboard;
      for (var y = 0; y < clip.height; y++) {
        for (var x = 0; x < clip.width; x++) {
          var targetX = selection.x + x;
          var targetY = selection.y + y;
          if (targetX < atlas.width && targetY < atlas.height) {
            current[targetY * atlas.width + targetX] =
              clip.indices[y * clip.width + x];
          }
        }
      }
      if (M.setEdit(army, atlas.key, model.modelId, current)) {
        changed(options); rerender();
      }
    });
    paste.disabled = !ui.armySelection || !ui.armyClipboard ||
      ui.armyClipboard.atlasKey !== atlas.key;
    paste.setAttribute('title', paste.disabled && ui.armyClipboard
      ? 'CI8 indices can be pasted only within the same atlas palette.'
      : 'Paste copied CI8 indices at the selection origin.');
    bar.appendChild(paste);

    var white = previewBackground(ui) === 'white';
    var preview = button('Transparent Preview: ' +
      (white ? 'White' : 'Checkerboard'), 'btn-secondary', function() {
      OB64.artUI.togglePreviewBackground(ui);
      rerender();
    });
    preview.setAttribute('aria-pressed', white ? 'true' : 'false');
    bar.appendChild(preview);
    return bar;
  }

  function previewGrid(atlas, model, current, ui) {
    var grid = element('div', 'art-preview-pair army-preview-grid');
    [0, 1].forEach(function(paletteIndex) {
      [[model.originalIndices ? 'Original' : 'Retail plane missing',
        model.originalIndices || model.blankIndices], ['Current', current]]
        .forEach(function(row) {
          var card = element('figure', 'art-preview-card');
          card.appendChild(element('figcaption', '', row[0] +
            ' · Palette ' + paletteIndex));
          card.appendChild(planeCanvas(atlas, row[1], paletteIndex, 4,
            'art-preview-canvas', ui));
          grid.appendChild(card);
        });
    });
    return grid;
  }

  function palettePanel(atlas, ui, rerender) {
    var panel = element('aside', 'art-palette-panel army-palette-panel');
    panel.appendChild(element('h4', '', 'Fixed CI8 Palette'));
    var tabs = element('div', 'art-pack-tabs army-palette-tabs');
    [0, 1].forEach(function(paletteIndex) {
      tabs.appendChild(button('Palette ' + paletteIndex,
        ui.armyPaletteIndex === paletteIndex ? 'active' : '', function() {
          ui.armyPaletteIndex = paletteIndex;
          rerender();
        }));
    });
    panel.appendChild(tabs);
    var selectedWord = atlas.palettes[ui.armyPaletteIndex][ui.armySelectedIndex];
    var selected = element('div', 'army-selected-index');
    var swatch = element('span', 'army-palette-swatch');
    if (selectedWord & 1) swatch.style.background = A.colorCss(selectedWord);
    else swatch.classList.add('transparent');
    selected.appendChild(swatch);
    selected.appendChild(element('span', '', 'Index ' +
      M.hex(ui.armySelectedIndex, 2) + ' · ' + A.hex(selectedWord, 4) +
      (selectedWord & 1 ? ' opaque' : ' transparent')));
    panel.appendChild(selected);
    var grid = element('div', 'army-palette-grid');
    atlas.palettes[ui.armyPaletteIndex].forEach(function(word, index) {
      var color = button('', 'army-palette-swatch' +
        (index === ui.armySelectedIndex ? ' selected' : '') +
        (!(word & 1) ? ' transparent' : ''), function() {
        ui.armySelectedIndex = index;
        ui.armyTool = 'pencil';
        rerender();
      });
      if (word & 1) color.style.background = A.colorCss(word);
      color.setAttribute('aria-label', 'Palette index ' + M.hex(index, 2) +
        ', word ' + A.hex(word, 4) +
        (word & 1 ? ', opaque' : ', transparent'));
      color.setAttribute('title', M.hex(index, 2) + ' · ' + A.hex(word, 4));
      grid.appendChild(color);
    });
    panel.appendChild(grid);
    panel.appendChild(element('p', 'art-palette-help',
      'The resource owns two fixed palettes. Pixel edits change the shared CI8 plane and affect both palette previews.'));
    return panel;
  }

  function downloadPng(atlas, indices, paletteIndex, filename, options) {
    var canvas = OB64.artUI.nativePngCanvas(
      M.renderWords(atlas, indices, paletteIndex), atlas.width, atlas.height);
    canvas.toBlob(function(blob) {
      if (!blob) {
        notify(options, 'Army sprite PNG export failed.');
        return;
      }
      var url = URL.createObjectURL(blob);
      var anchor = document.createElement('a');
      anchor.href = url;
      anchor.download = filename;
      document.body.appendChild(anchor);
      anchor.click();
      anchor.remove();
      setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
      notify(options, 'Army sprite exported as ' + filename + '.');
    }, 'image/png');
  }

  function openImportDialog(source, army, atlas, model, ui, options, rerender) {
    var settings = {
      resizeMode: 'nearest', panX: 0.5, panY: 0.5,
      dither: false, paletteIndex: ui.armyPaletteIndex
    };
    var result = null;
    var scheduled = null;
    var overlay = element('div', 'error-modal-overlay art-import-overlay');
    var modal = element('div', 'error-modal art-import-modal army-import-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'army-import-title');
    overlay.appendChild(modal);
    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'Prepare Army Sprite Image');
    title.id = 'army-import-title';
    header.appendChild(title);
    header.appendChild(button('×', 'error-modal-close', close));
    modal.appendChild(header);
    var body = element('div', 'error-modal-body art-import-body');
    body.appendChild(element('p', 'art-import-intro', source.name + ' · ' +
      source.format + ' · ' + source.width + '×' + source.height +
      ' source pixels. The result is cropped and resized to ' + atlas.width +
      '×' + atlas.height + '.'));
    var layout = element('div', 'art-import-layout');
    var previewPanel = element('section', 'art-import-preview-panel');
    previewPanel.appendChild(element('h3', '', 'Converted CI8 Preview'));
    var previewHost = element('div', 'art-import-preview-host');
    previewPanel.appendChild(previewHost);
    var stats = element('p', 'art-import-stats', 'Preparing preview…');
    stats.setAttribute('aria-live', 'polite');
    previewPanel.appendChild(stats);
    layout.appendChild(previewPanel);
    var controls = element('section', 'art-import-controls');

    function selectControl(labelText, values, current, change) {
      var label = element('label', 'art-import-control');
      label.appendChild(element('span', '', labelText));
      var select = element('select');
      values.forEach(function(row) {
        var option = element('option', '', row[1]);
        option.value = row[0];
        select.appendChild(option);
      });
      select.value = String(current);
      select.addEventListener('change', function() {
        change(select.value);
        schedulePreview();
      });
      label.appendChild(select);
      controls.appendChild(label);
      return select;
    }

    var resizeSelect = selectControl('Resize method', [
      ['nearest', 'Pixel Art — nearest-neighbor'],
      ['smooth', 'Smooth — area/bilinear']
    ], settings.resizeMode, function(value) { settings.resizeMode = value; });
    selectControl('Target palette', [
      ['0', 'Palette 0'], ['1', 'Palette 1']
    ], settings.paletteIndex, function(value) {
      settings.paletteIndex = Number(value);
    });

    var initialCrop = A.imageCropRect(source.width, source.height,
      atlas.width, atlas.height, settings.panX, settings.panY);
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
      'Off by default. Dithering can reduce visible bands after palette mapping.'));
    controls.appendChild(ditherWrap);
    controls.appendChild(element('p', 'art-import-background',
      'Alpha below 128 maps to the atlas transparent index. Opaque pixels map to the selected fixed palette.'));
    layout.appendChild(controls);
    body.appendChild(layout);
    modal.appendChild(body);
    var footer = element('div', 'error-modal-footer art-import-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var apply = button('Import Sprite', 'error-modal-ok', function() {
      if (!result) return;
      if (M.setEdit(army, atlas.key, model.modelId, result.indices,
          { create: true })) {
        ui.armyPaletteIndex = result.paletteIndex;
        changed(options);
      }
      notify(options, source.name + ' mapped to ' + atlas.label +
        ' model ' + M.hex(model.modelId, 2) + ' with Palette ' +
        result.paletteIndex + '.');
      close();
      rerender();
    });
    apply.disabled = true;
    footer.appendChild(apply);
    modal.appendChild(footer);

    function preview() {
      scheduled = null;
      try {
        result = M.prepareImageImport(source, atlas, settings.paletteIndex,
          settings);
        previewHost.innerHTML = '';
        var scale = Math.max(1, Math.min(12,
          Math.floor(360 / Math.max(atlas.width, atlas.height))));
        previewHost.appendChild(planeCanvas(atlas, result.indices,
          result.paletteIndex, scale, 'art-import-preview-canvas', ui));
        var crop = result.crop;
        stats.textContent = 'Crop ' + crop.width.toFixed(1) + '×' +
          crop.height.toFixed(1) + ' at ' + crop.x.toFixed(1) + ', ' +
          crop.y.toFixed(1) + ' · Palette ' + result.paletteIndex + ' · ' +
          result.sourceNativeColorCount + ' source RGB555 colors · ' +
          result.transparentPixels + ' transparent pixels' +
          (result.dithered ? ' · ordered dither applied' : '');
        stats.classList.remove('blocked');
        apply.disabled = false;
      } catch (error) {
        result = null;
        previewHost.innerHTML = '';
        stats.textContent = 'Conversion blocked: ' + error.message;
        stats.classList.add('blocked');
        apply.disabled = true;
      }
    }
    function schedulePreview() {
      stats.textContent = 'Preparing preview…';
      apply.disabled = true;
      if (scheduled !== null) window.cancelAnimationFrame(scheduled);
      scheduled = window.requestAnimationFrame(preview);
    }
    function close() {
      if (scheduled !== null) window.cancelAnimationFrame(scheduled);
      scheduled = null;
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escape);
    }
    function escape(event) { if (event.key === 'Escape') close(); }
    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    document.addEventListener('keydown', escape);
    document.body.appendChild(overlay);
    resizeSelect.focus();
    schedulePreview();
  }

  function imageInput(army, atlas, model, ui, options, rerender) {
    var input = element('input', 'art-image-input');
    input.type = 'file';
    input.accept = 'image/png,image/jpeg,.png,.jpg,.jpeg';
    input.setAttribute('aria-label',
      'Select a PNG or JPEG image to import as the current Army sprite');
    input.addEventListener('change', async function() {
      var file = input.files && input.files[0];
      if (!file) return;
      try {
        var source = await OB64.artUI.decodeImageSource(file);
        openImportDialog(source, army, atlas, model, ui, options, rerender);
      } catch (error) {
        notify(options, 'Army sprite image import blocked: ' + error.message);
      } finally {
        input.value = '';
      }
    });
    return input;
  }

  function actions(rom, army, atlas, model, ui, options, rerender) {
    var bar = element('div', 'art-asset-actions army-sprite-actions');
    var edited = !!army.edits[model.key];
    var present = M.hasCurrentPlane(army, atlas.key, model.modelId);
    if (!present) {
      bar.appendChild(button('Create Blank Player Sprite', 'btn-primary',
        function() {
          if (M.createBlankPlane(army, atlas.key, model.modelId)) {
            changed(options);
            notify(options, atlas.label + ' ' + modelTitle(model) +
              ' created as a transparent custom plane.');
            rerender();
          }
        }));
      var counterpartKey = (atlas.side === 'player'
        ? 'enemy-front-' : 'player-back-') + atlas.lane;
      var counterpartAtlas = army.byKey[counterpartKey];
      var counterpart = counterpartAtlas &&
        M.modelFor(army, counterpartKey, model.modelId);
      var copyCounterpart = button(atlas.side === 'player'
        ? 'Copy Enemy Sprite' : 'Copy Player Sprite', 'btn-secondary',
        function() {
          try {
            var sourceIndices = M.currentIndices(army, counterpartAtlas.key,
              counterpart.modelId);
            var source = {
              width: counterpartAtlas.width,
              height: counterpartAtlas.height,
              rgba: M.rgbaPixels(counterpartAtlas, sourceIndices,
                ui.armyPaletteIndex)
            };
            var converted = M.prepareImageImport(source, atlas,
              ui.armyPaletteIndex, { resizeMode: 'nearest' });
            if (M.setEdit(army, atlas.key, model.modelId,
                converted.indices, { create: true })) {
              changed(options);
              notify(options, 'Copied the opposite-side sprite into ' +
                atlas.label + ' ' + modelTitle(model) + '.');
              rerender();
            }
          } catch (error) {
            notify(options, 'Opposite-side sprite copy blocked: ' +
              error.message);
          }
        });
      copyCounterpart.disabled = !counterpartAtlas || !counterpart ||
        !M.hasCurrentPlane(army, counterpartAtlas.key, counterpart.modelId);
      bar.appendChild(copyCounterpart);
    }
    var reset = button(model.retailPresent
      ? 'Reset Current Sprite' : 'Remove Custom Player Sprite',
      'btn-secondary', function() {
      if (M.resetModel(army, atlas.key, model.modelId)) {
        changed(options);
        notify(options, model.retailPresent
          ? atlas.label + ' ' + modelTitle(model) +
            ' restored to the loaded ROM.'
          : atlas.label + ' ' + modelTitle(model) +
            ' removed from the Project.');
        rerender();
      }
    });
    reset.disabled = !edited;
    bar.appendChild(reset);
    var input = imageInput(army, atlas, model, ui, options, rerender);
    bar.appendChild(input);
    bar.appendChild(button('Import PNG/JPEG…', 'btn-secondary', function() {
      input.click();
    }));
    if (OB64.spriteEditorUI && OB64.spriteEditorUI.openLibraryPicker) {
      bar.appendChild(button('Import from Sprite Library…', 'btn-secondary',
        function() {
          OB64.spriteEditorUI.openLibraryPicker(rom, {
            title: 'Choose Army Sprite Source',
            actionLabel: 'Prepare Army Sprite',
            onStatus: function(message) { notify(options, message); }
          }, function(source) {
            openImportDialog(source, army, atlas, model, ui, options, rerender);
          });
        }));
    }
    [0, 1].forEach(function(paletteIndex) {
      var exportButton = button('Export Palette ' + paletteIndex + ' PNG',
        'btn-secondary', function() {
          downloadPng(atlas,
            M.currentIndices(army, atlas.key, model.modelId), paletteIndex,
            atlas.key + '-model-' + model.modelId.toString(16)
              .padStart(2, '0') + '-palette-' + paletteIndex + '.png', options);
        });
      exportButton.disabled = !present;
      bar.appendChild(exportButton);
    });
    return bar;
  }

  function editor(rom, army, ui, options, rerender) {
    var atlas = ensureUi(army, ui);
    var model = atlas.models[ui.armyModelId] || atlas.models[0];
    ui.armyModelId = model.modelId;
    var current = M.currentIndices(army, atlas.key, model.modelId);
    var present = M.hasCurrentPlane(army, atlas.key, model.modelId);
    var route = ui.armyBrowseMode === 'classes'
      ? army.byClassId[ui.armyClassId] : null;
    var main = element('main', 'art-editor army-sprite-editor');
    var heading = element('div', 'art-editor-heading');
    var copy = element('div');
    copy.appendChild(element('h3', '', (route
      ? M.hex(route.classId, 2) + ' · ' + route.className + ' · '
      : '') + atlas.label + ' · ' + modelTitle(model)));
    copy.appendChild(element('p', '', classSummary(model)));
    copy.appendChild(element('p', '', 'Resource ' + M.hex(atlas.resourceKey) +
      ' · routed plane ' + M.hex(model.decodedOffset, 4) +
      ' · ' + atlas.orientation));
    heading.appendChild(copy);
    if (!present) {
      heading.appendChild(element('span', 'art-badge army-badge-missing',
        'No player sprite'));
    } else if (!model.retailPresent) {
      heading.appendChild(element('span', 'art-badge army-badge-custom',
        'Custom player sprite'));
    } else if (army.edits[model.key]) {
      heading.appendChild(element('span', 'art-badge art-badge-edited', 'Edited'));
    }
    main.appendChild(heading);
    main.appendChild(element('div', 'art-pack-note', !present
      ? 'The class route exists, but the retail player atlas has no plane for this model ID.'
      : 'One CI8 plane is shared by both fixed palettes. Pixel edits change both palette previews.'));
    if (!model.retailPresent && model.classIds.length > 1) {
      main.appendChild(element('div', 'art-pack-note',
        'This routed model is shared by ' + model.classIds.length +
        ' classes. One custom sprite changes every listed class.'));
    }
    main.appendChild(previewGrid(atlas, model, current, ui));
    if (present) {
      main.appendChild(toolbox(army, atlas, model, ui, options, rerender));
    }
    var workspace = element('div', 'art-workspace army-sprite-workspace');
    var scale = atlas.width > 16 ? 14 : 20;
    var canvas = planeCanvas(atlas, current, ui.armyPaletteIndex, scale,
      'art-edit-canvas army-sprite-edit-canvas', ui, ui.armySelection);
    if (present) {
      installCanvas(canvas, army, atlas, model, scale, ui, options, rerender);
    } else {
      canvas.classList.add('army-missing-canvas');
      canvas.setAttribute('aria-label',
        'No player sprite exists for this routed model');
    }
    workspace.appendChild(canvas);
    workspace.appendChild(palettePanel(atlas, ui, rerender));
    main.appendChild(workspace);
    main.appendChild(actions(rom, army, atlas, model, ui, options, rerender));
    main.appendChild(element('p', 'army-export-boundary',
      model.retailPresent
        ? 'Existing planes rebuild inside the verified resource envelope.'
        : 'A custom missing plane expands and relocates the player atlas. Static owner and compressed-data readback run during export. In-game verification remains required.'));
    return main;
  }

  function sourceForModel(army, atlasKey, modelId, paletteIndex) {
    var atlas = M.atlasFor(army, atlasKey);
    var model = M.modelFor(army, atlasKey, modelId);
    return {
      name: atlas.label + ' ' + modelTitle(model) + ' Palette ' + paletteIndex,
      format: 'Army Sprite',
      width: atlas.width,
      height: atlas.height,
      rgba: M.rgbaPixels(atlas,
        M.currentIndices(army, atlas.key, model.modelId), paletteIndex),
      atlas: atlas,
      model: model,
      paletteIndex: paletteIndex
    };
  }

  function render(state, ui, options, rerender, rom) {
    var army = state && state.armySprites;
    if (!army || !army.supported) {
      return {
        browser: null,
        editor: element('div', 'art-unavailable army-sprite-unavailable',
          army && army.unavailableReason ||
          'Army sprite resources are unavailable for this ROM.')
      };
    }
    ensureUi(army, ui);
    return {
      browser: browser(army, ui, rerender),
      editor: editor(rom, army, ui, options, rerender)
    };
  }

  OB64.armySpriteUI = {
    render: render,
    flood: flood,
    drawPlane: drawPlane,
    sourceForModel: sourceForModel,
    openImportDialog: openImportDialog
  };
})();
