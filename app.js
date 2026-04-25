// OB64 Mod Editor — App Logic
// Tab rendering, inline editing, cross-references, export pipeline

window.OB64 = window.OB64 || {};

(function() {
  var rom = null;       // Parsed ROM data from OB64.loadROM()
  var saveState = null; // Parsed save state from OB64.parseSaveFile() — independent of rom
  var saveFileName = null; // Original save filename for export naming
  var changes = 0;      // Pending change count
  var activeTab = 'shops';
  // Per-subsystem dirty flags — only re-splice/rewrite archives that the
  // user actually edited. LH5 round-trip can inflate untouched archives
  // past their original ROM slot, which previously broke unrelated exports.
  var dirty = { shops: false, enemies: false, items: false, classDefs: false, encounters: false, creatureDrops: false, consumables: false, statGates: false };

  // ============================================================
  // DOM refs
  // ============================================================
  var fileInput = document.getElementById('rom-file');
  var btnExport = document.getElementById('btn-export');
  var btnSavePatch = document.getElementById('btn-save-patch');
  var patchFileInput = document.getElementById('patch-file');
  var btnLoadPatchLabel = document.getElementById('btn-load-patch-label');
  var patchChip = document.getElementById('patch-chip');
  var tabBar = document.getElementById('tab-bar');
  var emptyState = document.getElementById('empty-state');
  var statusBar = document.getElementById('status-bar');

  // Name of the most-recently loaded or saved patch (for status display)
  var lastPatchFilename = null;

  // ============================================================
  // ROM Loading
  // ============================================================
  fileInput.addEventListener('change', function(e) {
    var file = e.target.files[0];
    if (!file) return;
    statusBar.textContent = 'Loading ROM...';
    var reader = new FileReader();
    reader.onload = function(ev) {
      try {
        rom = OB64.loadROM(ev.target.result);
        OB64.patch.snapshotOriginal(rom);   // baseline for later diffing
        changes = 0;
        dirty = { shops: false, enemies: false, items: false, classDefs: false, encounters: false, creatureDrops: false, consumables: false, statGates: false };
        lastPatchFilename = null;
        emptyState.style.display = 'none';
        btnExport.disabled = false;
        btnSavePatch.disabled = false;
        patchFileInput.disabled = false;
        btnLoadPatchLabel.setAttribute('aria-disabled', 'false');
        updatePatchChip();
        statusBar.textContent = 'ROM loaded: ' + file.name + ' (' + (file.size / 1048576).toFixed(1) + ' MB) | ' + rom.archives.length + ' archives | 0 pending changes';
        renderTab(activeTab);

        // Warn if any shop in the loaded ROM is already over the per-shop
        // cap — that ROM will crash the shop menu when the player opens it.
        var bigShops = [];
        for (var si = 0; si < rom.shops.length; si++) {
          if (rom.shops[si].items.length > OB64.SHOP_MAX_ITEMS_PER_SHOP) {
            bigShops.push({ idx: si, count: rom.shops[si].items.length });
          }
        }
        if (bigShops.length) {
          var detail = bigShops.map(function(b) {
            return '  Shop #' + b.idx + ': ' + b.count + ' items';
          }).join('\n');
          showErrorModal('Shop over per-shop capacity',
            'One or more shops in this ROM hold more items than the vanilla ' +
            'maximum (' + OB64.SHOP_MAX_ITEMS_PER_SHOP + '). A 277-item shop has ' +
            'been confirmed to crash the shop menu on load, so any shop past this ' +
            'threshold is a likely crash source.\n\n' + detail + '\n\n' +
            'Open the Shops tab and reduce the over-capacity shops before playing.');
        }
      } catch(err) {
        statusBar.textContent = 'Error loading ROM: ' + err.message;
        console.error(err);
      }
    };
    reader.readAsArrayBuffer(file);
  });

  // ============================================================
  // Export
  // ============================================================
  btnExport.addEventListener('click', function() {
    if (!rom) return;
    statusBar.textContent = 'Exporting...';
    try {
      var touched = [];

      // Shops (archive #751 / shopcsv.bin)
      if (dirty.shops) {
        var shopBuf = OB64.serializeShops(rom.shops);
        var shopComp = OB64.lh5Compress(shopBuf);
        var shopArc = OB64.buildLHAArchive(shopComp, shopBuf, 'shopcsv.bin');
        var shopResult = OB64.spliceArchive(rom.z64, rom.archives[751], shopArc);
        if (!shopResult.success) {
          var shopArch = rom.archives[751];
          var slotSize = shopArch.totalHeaderSize + shopArch.compSize;
          var overBy = shopArc.length - slotSize;
          var totalItems = OB64.totalShopItems(rom.shops);
          showErrorModal('Export failed — shop overfill',
            'The compressed shop archive is ' + overBy +
            ' bytes larger than its ROM slot (' + shopArc.length +
            ' / ' + slotSize + ' bytes).\n\n' +
            'Current total: ' + totalItems + ' items across all shops.\n' +
            'Recommended maximum: ' + OB64.SHOP_ITEM_LIMIT + ' items.\n\n' +
            'Remove ' + Math.max(1, totalItems - OB64.SHOP_ITEM_LIMIT) +
            ' or more item(s) from one or more shops and try again.');
          statusBar.textContent = 'Export failed (shops): ' + shopResult.error;
          return;
        }
        touched.push('shops');
      }

      // Enemydat (archive #647)
      if (dirty.enemies) {
        var edBuf = OB64.serializeEnemydat(rom.enemySquads);
        var edComp = OB64.lh5Compress(edBuf);
        var edArc = OB64.buildLHAArchive(edComp, edBuf, 'enemydat.bin');
        var edResult = OB64.spliceArchive(rom.z64, rom.archives[647], edArc);
        if (!edResult.success) {
          statusBar.textContent = 'Export failed (enemydat): ' + edResult.error;
          return;
        }
        touched.push('enemies');
      }

      // Item stats (direct z64 patch at 0x62310)
      if (dirty.items) {
        for (var i = 0; i < rom.itemStats.length; i++) {
          var item = rom.itemStats[i];
          var off = OB64.ITEM_STAT_OFFSET + i * OB64.ITEM_STAT_SIZE;
          rom.z64[off + 0] = item.equipType;
          rom.z64[off + 1] = item.element;
          rom.z64[off + 2] = item.grade;
          OB64.writeU16BE(rom.z64, off + 4, item.price);
          rom.z64[off + 6] = item.strRaw;
          rom.z64[off + 7] = item.intRaw;
          rom.z64[off + 8] = item.agiRaw;
          rom.z64[off + 9] = item.dexRaw;
          rom.z64[off + 10] = item.vitRaw;
          rom.z64[off + 11] = item.menRaw;
          rom.z64[off + 12] = item.b12Raw;
          var sb = function(v) { return v < 0 ? v + 256 : v; };
          rom.z64[off + 13] = sb(item.resPhys);
          rom.z64[off + 14] = sb(item.resWind);
          rom.z64[off + 15] = sb(item.resFire);
          rom.z64[off + 16] = sb(item.resEarth);
          rom.z64[off + 17] = sb(item.resWater);
          rom.z64[off + 18] = sb(item.resVirtue);
          rom.z64[off + 19] = sb(item.resBane);
        }
        touched.push('items');
      }

      // Class definitions (direct z64 patch at 0x5DAD8)
      if (dirty.classDefs) {
        OB64.serializeClassDefs(rom.classDefs, rom.z64);
        touched.push('classes');
      }

      // Neutral encounter pool at 0x141ED0 — outside CRC window, no recalc.
      if (dirty.encounters) {
        OB64.serializeNeutralEncounters(rom.neutralEncounters, rom.z64);
        touched.push('encounters');
      }

      // Creature drop table at 0x142258 — outside CRC window, no recalc.
      if (dirty.creatureDrops) {
        OB64.serializeCreatureDrops(rom.creatureDrops, rom.z64);
        touched.push('creature drops');
      }

      // Consumable master table at 0x645CC — outside CRC window, no recalc.
      if (dirty.consumables) {
        OB64.serializeConsumables(rom.consumables, rom.z64);
        touched.push('consumables');
      }

      // Stat gate thresholds — LZSS block at GAP_START + 0x3A960C.
      // Recompresses and splices back into the same slot; throws on overfill.
      // Past CRC window — no recalc needed.
      if (dirty.statGates) {
        try {
          OB64.serializeStatGates(rom.statGates, rom.z64);
          touched.push('stat gates');
        } catch (e) {
          showErrorModal('Export failed — stat-gate recompress',
            e.message + '\n\nRevert some stat-gate edits and try again, ' +
            'or file a follow-up to teach the compressor about long ' +
            'back-references (would gain ~10-20% compression headroom).');
          statusBar.textContent = 'Export failed (stat gates): ' + e.message;
          return;
        }
      }

      // CRC must be recalculated whenever we patch inside the first 1 MB
      // of z64 (items + classDefs are in-region; shop/enemydat archives and
      // encounter/drop tables live past the CRC window so don't require it).
      if (dirty.items || dirty.classDefs) {
        OB64.recalcN64CRC(rom.z64);
      }

      if (touched.length === 0) {
        statusBar.textContent = 'Nothing to export — no edits made yet.';
        return;
      }

      OB64.exportROM(rom.z64);
      var exportMsg = 'ROM exported as ob64_modified.v64 ('
        + touched.join(', ') + ') | ' + changes + ' changes applied';
      // Clear dirty so subsequent exports without edits do nothing,
      // but keep the success message visible in the status bar
      dirty = { shops: false, enemies: false, items: false, classDefs: false, encounters: false, creatureDrops: false, consumables: false, statGates: false };
      changes = 0;
      statusBar.textContent = exportMsg;
    } catch(err) {
      statusBar.textContent = 'Export error: ' + err.message;
      console.error(err);
    }
  });

  // ============================================================
  // Patch — Save / Load
  // ============================================================
  btnSavePatch.addEventListener('click', function() {
    if (!rom) return;
    try {
      var patch = OB64.patch.collectPatch(rom);
      var shopsN = patch.summary.shops_modified;
      var pricesN = patch.summary.item_prices_modified;
      var globalRateN = patch.summary.neutral_global_rate_modified || 0;
      if (shopsN + pricesN + globalRateN === 0) {
        statusBar.textContent = 'No edits to save — patch would be empty.';
        return;
      }
      OB64.patch.downloadPatch(patch);
      lastPatchFilename = 'ob64_patch_' + patch.created_at.replace(/[:.]/g, '-') + '.json';
      updatePatchChip();
      var parts = [];
      if (shopsN) parts.push(shopsN + ' shop' + (shopsN === 1 ? '' : 's'));
      if (pricesN) parts.push(pricesN + ' price' + (pricesN === 1 ? '' : 's'));
      if (globalRateN) parts.push('global encounter roll');
      statusBar.textContent = 'Patch saved (' + parts.join(', ') + ' changed).';
    } catch (err) {
      statusBar.textContent = 'Save Patch failed: ' + err.message;
      console.error(err);
    }
  });

  patchFileInput.addEventListener('change', function(e) {
    var file = e.target.files[0];
    if (!file || !rom) return;
    var reader = new FileReader();
    reader.onload = function(ev) {
      try {
        var patch = OB64.patch.parsePatchFile(ev.target.result);
        var result = OB64.patch.applyPatch(rom, patch, dirty);
        // Count applied changes so status + export modal show the right counts
        changes += result.applied.shops + result.applied.prices + (result.applied.neutralGlobalRate || 0);
        lastPatchFilename = file.name;
        updatePatchChip();
        renderTab(activeTab);

        var loadedParts = [];
        if (result.applied.shops) loadedParts.push(result.applied.shops + ' shop' + (result.applied.shops === 1 ? '' : 's'));
        if (result.applied.prices) loadedParts.push(result.applied.prices + ' price' + (result.applied.prices === 1 ? '' : 's'));
        if (result.applied.neutralGlobalRate) loadedParts.push('global encounter roll');
        if (!loadedParts.length) loadedParts.push('0 changes');
        var msg = 'Patch loaded: ' + file.name + ' (' +
          loadedParts.join(', ') + ').';
        if (result.warnings.length) {
          console.warn('[patch] warnings:', result.warnings);
          msg += ' (' + result.warnings.length + ' warning' + (result.warnings.length === 1 ? '' : 's') + ' — see console)';
        }
        statusBar.textContent = msg;
      } catch (err) {
        statusBar.textContent = 'Load Patch failed: ' + err.message;
        console.error(err);
      } finally {
        // Clear the input so re-loading the same filename re-fires change
        patchFileInput.value = '';
      }
    };
    reader.readAsText(file);
  });

  function updatePatchChip() {
    if (!patchChip) return;
    if (lastPatchFilename) {
      patchChip.textContent = 'patch: ' + lastPatchFilename;
      patchChip.hidden = false;
    } else {
      patchChip.textContent = '';
      patchChip.hidden = true;
    }
  }

  // ============================================================
  // Tab Switching
  // ============================================================
  tabBar.addEventListener('click', function(e) {
    if (e.target.tagName !== 'BUTTON') return;
    var tab = e.target.dataset.tab;
    if (!tab) return;
    activeTab = tab;

    var buttons = tabBar.querySelectorAll('button');
    for (var i = 0; i < buttons.length; i++) {
      buttons[i].classList.toggle('active', buttons[i].dataset.tab === tab);
    }

    var panels = document.querySelectorAll('.tab-panel');
    for (var i = 0; i < panels.length; i++) {
      panels[i].classList.toggle('active', panels[i].id === 'panel-' + tab);
    }

    // The Save tab works independently of the ROM — it edits a save state,
    // not the ROM. Hide the "No ROM Loaded" scroll when the save tab is
    // active so the user sees the save panel instead.
    if (tab === 'save') {
      emptyState.style.display = 'none';
      renderSaveGame(document.getElementById('panel-save'));
    } else if (!rom) {
      emptyState.style.display = '';
    } else {
      renderTab(tab);
    }
  });

  // ============================================================
  // Render dispatcher
  // ============================================================
  function renderTab(tab) {
    var panel = document.getElementById('panel-' + tab);
    switch(tab) {
      case 'shops':     renderShops(panel); break;
      case 'enemies':   renderEnemies(panel); break;
      case 'missions':  renderMissions(panel); break;
      case 'scenarios': renderScenarios(panel); break;
      case 'classes':   renderClasses(panel); break;
      case 'items':     renderItems(panel); break;
      case 'encounters': renderEncounters(panel); break;
      case 'save':      renderSaveGame(panel); break;
      case 'map':       renderMap(panel); break;
    }
  }

  function updateStatus() {
    if (!rom) return;
    var text = 'ROM loaded | ' + rom.archives.length + ' archives';
    if (changes > 0) text += ' | <span class="changes">' + changes + ' pending changes</span>';
    else text += ' | 0 pending changes';
    statusBar.innerHTML = text;
  }

  function markChanged() {
    changes++;
    // Map the currently-active tab to a dirty flag so export can skip
    // subsystems the user never touched. This avoids LH5 round-trip
    // bloat rewriting untouched archives (e.g. enemydat was growing
    // 358 B past its ROM slot on every export).
    switch (activeTab) {
      case 'shops':    dirty.shops = true; break;
      case 'enemies':  dirty.enemies = true; break;
      case 'items':    dirty.items = true; break;
      case 'classes':  dirty.classDefs = true; break;
      case 'encounters':
        // Encounters tab edits can touch either the neutral-encounter pool,
        // the creature drop table, or both. The renderer sets the specific
        // flag directly (dirty.encounters / dirty.creatureDrops) before
        // calling markChanged(), so here we only bump the change counter.
        break;
      case 'save':
        // Save-state edits are tracked on the saveState object itself so
        // the export button can enable/disable. Nothing else needs to fire.
        if (saveState) saveState.dirty = true;
        renderSaveExportButtonState();
        break;
      // Consumable edits happen inside the Shops tab's Expendable modal;
      // that modal sets dirty.consumables directly before calling markChanged().
      // missions/scenarios/map aren't editable in the current UI
    }
    updateStatus();
  }

  function renderSaveExportButtonState() {
    var btn = document.getElementById('btn-save-export');
    if (btn) btn.disabled = !(saveState && saveState.dirty);
  }

  // ============================================================
  // Shared editing helpers — dropdown, searchable input, numeric
  // ============================================================

  // Small enum dropdown (<30 options). options = {value: label, ...}
  function makeDropdown(td, options, currentVal, onCommit) {
    if (td.querySelector('select')) return;
    var prev = td.textContent;
    var sel = document.createElement('select');
    for (var k in options) {
      var opt = document.createElement('option');
      opt.value = k;
      opt.textContent = options[k];
      if (String(k) === String(currentVal)) opt.selected = true;
      sel.appendChild(opt);
    }
    td.textContent = '';
    td.appendChild(sel);
    sel.focus();

    function commit() {
      var v = parseInt(sel.value);
      if (!isNaN(v)) {
        onCommit(v);
        td.classList.add('modified');
        markChanged();
      }
    }
    sel.addEventListener('change', function() { commit(); });
    sel.addEventListener('blur', function() { commit(); });
    sel.addEventListener('keydown', function(e) {
      if (e.key === 'Escape') { td.textContent = prev; }
    });
  }

  // Large searchable input (items=277, classes=164). options = {value: label, ...}
  function makeSearchableInput(td, options, currentVal, onCommit) {
    if (td.querySelector('input')) return;
    var prev = td.textContent;
    var listId = 'dl-' + Math.random().toString(36).slice(2, 8);
    var dl = document.createElement('datalist');
    dl.id = listId;
    // Build reverse map: label → value
    var reverseMap = {};
    for (var k in options) {
      var opt = document.createElement('option');
      opt.value = options[k] + ' [' + k + ']';
      dl.appendChild(opt);
      reverseMap[options[k] + ' [' + k + ']'] = parseInt(k);
      reverseMap[options[k]] = parseInt(k);
    }
    var inp = document.createElement('input');
    inp.setAttribute('list', listId);
    // Leave input empty so Chrome doesn't filter the datalist to a substring match
    // of the current value (showing only 1 option). Show the current value as
    // placeholder instead; empty commit keeps the existing value.
    inp.value = '';
    inp.placeholder = options[currentVal]
      ? 'Current: ' + options[currentVal] + ' [' + currentVal + ']'
      : 'Current: ' + String(currentVal) + ' (none)';
    td.textContent = '';
    td.appendChild(dl);
    td.appendChild(inp);
    inp.focus();

    function commit() {
      var text = inp.value.trim();
      // Empty input = no change, restore previous display
      if (text === '') { td.textContent = prev; return; }
      var v = reverseMap[text];
      // Also try parsing [NNN] from the end
      if (v === undefined) {
        var m = text.match(/\[(\d+)\]\s*$/);
        if (m) v = parseInt(m[1]);
      }
      if (v === undefined) v = parseInt(text);
      if (!isNaN(v)) {
        onCommit(v);
        td.classList.add('modified');
        markChanged();
      } else {
        td.textContent = prev;
      }
    }
    inp.addEventListener('blur', function() { commit(); });
    inp.addEventListener('keydown', function(e) {
      if (e.key === 'Enter') { e.preventDefault(); commit(); }
      if (e.key === 'Escape') { td.textContent = prev; }
    });
  }

  // Numeric input with min/max validation
  function makeNumericInput(td, currentVal, min, max, onCommit) {
    if (td.querySelector('input')) return;
    var prev = td.textContent;
    var inp = document.createElement('input');
    inp.type = 'number';
    inp.min = min;
    inp.max = max;
    inp.value = currentVal;
    td.textContent = '';
    td.appendChild(inp);
    inp.focus();
    inp.select();

    function commit() {
      var v = parseInt(inp.value);
      if (!isNaN(v) && v >= min && v <= max) {
        onCommit(v);
        td.classList.add('modified');
        markChanged();
      }
    }
    inp.addEventListener('blur', function() { commit(); });
    inp.addEventListener('keydown', function(e) {
      if (e.key === 'Enter') { e.preventDefault(); commit(); }
      if (e.key === 'Escape') { td.textContent = prev; }
    });
  }

  // ============================================================
  // Item picker modal (shop-tab-style, reused across tabs).
  // Used by Enemies/Classes/Encounters/Save-Game-Editor wherever an item or
  // enum field was formerly edited via makeSearchableInput.
  //   opts = { title, options: {id: label} or [{id,label}], currentId,
  //            onSelect(id), withIcons (bool, default true) }
  // ============================================================
  function openItemPickerFromDict(opts) {
    var items = [];
    if (Array.isArray(opts.options)) {
      items = opts.options.map(function(o) { return { id: o.id, name: o.label, kind: 'enum' }; });
    } else {
      for (var k in opts.options) {
        items.push({ id: parseInt(k), name: opts.options[k], kind: 'enum' });
      }
    }
    openSaveItemPickerModal({
      title:     opts.title || 'Select',
      items:     items,
      currentId: opts.currentId,
      withIcons: opts.withIcons !== false, // default true for items
      onSelect:  function(id) {
        if (opts.onSelect) opts.onSelect(id);
        markChanged();
      }
    });
  }

  // ============================================================
  // Filter helper
  // ============================================================
  function makeFilterBar(placeholder, onFilter) {
    var div = document.createElement('div');
    div.className = 'filter-bar';
    var input = document.createElement('input');
    input.type = 'text';
    input.placeholder = placeholder;
    input.addEventListener('input', function() { onFilter(input.value.toLowerCase()); });
    div.appendChild(input);
    return div;
  }

  // ============================================================
  // Sortable columns — click any <th> to sort
  // ============================================================
  function makeSortable(table) {
    var headers = table.querySelectorAll('thead th');
    var tbody = table.querySelector('tbody');
    var sortCol = -1;
    var sortAsc = true;

    for (var h = 0; h < headers.length; h++) {
      headers[h].dataset.colIdx = h;
      headers[h].addEventListener('click', function() {
        var col = parseInt(this.dataset.colIdx);
        if (sortCol === col) {
          sortAsc = !sortAsc;
        } else {
          sortCol = col;
          sortAsc = true;
        }

        var rows = Array.from(tbody.querySelectorAll('tr'));
        rows.sort(function(a, b) {
          var aCell = a.cells[col];
          var bCell = b.cells[col];
          if (!aCell || !bCell) return 0;
          var aText = aCell.textContent.trim();
          var bText = bCell.textContent.trim();

          // Treat em-dash as empty/lowest
          var aEmpty = (aText === '\u2014' || aText === '-' || aText === '');
          var bEmpty = (bText === '\u2014' || bText === '-' || bText === '');
          if (aEmpty && bEmpty) return 0;
          if (aEmpty) return sortAsc ? 1 : -1;
          if (bEmpty) return sortAsc ? -1 : 1;

          // Hex values (0x...)
          if (aText.indexOf('0x') === 0 && bText.indexOf('0x') === 0) {
            return sortAsc
              ? parseInt(aText, 16) - parseInt(bText, 16)
              : parseInt(bText, 16) - parseInt(aText, 16);
          }

          // Archive refs (#NNN)
          if (aText.charAt(0) === '#' && bText.charAt(0) === '#') {
            return sortAsc
              ? parseInt(aText.slice(1)) - parseInt(bText.slice(1))
              : parseInt(bText.slice(1)) - parseInt(aText.slice(1));
          }

          // Numeric
          var aNum = parseFloat(aText);
          var bNum = parseFloat(bText);
          if (!isNaN(aNum) && !isNaN(bNum)) {
            return sortAsc ? aNum - bNum : bNum - aNum;
          }

          // String
          return sortAsc ? aText.localeCompare(bText) : bText.localeCompare(aText);
        });

        for (var i = 0; i < rows.length; i++) {
          tbody.appendChild(rows[i]);
        }

        // Update indicators
        for (var j = 0; j < headers.length; j++) {
          headers[j].classList.remove('sort-asc', 'sort-desc');
        }
        this.classList.add(sortAsc ? 'sort-asc' : 'sort-desc');
      });
    }
  }

  // ============================================================
  // SHOPS TAB
  // ============================================================
  var SHOP_CATEGORIES = ['head', 'weapon', 'accessory', 'body', 'expendable'];

  // Categorize an item ID into one of the 5 shop columns based on equipType.
  // Items not present in the stat table (or with equipType outside the equipment
  // range) are treated as expendable consumables.
  function categorizeShopItem(id) {
    var stat = rom && rom.itemStats ? rom.itemStats[id] : null;
    if (!stat) return 'expendable';
    var eq = stat.equipType;
    if (eq === 0x15 || eq === 0x16) return 'head';                          // Helm, Headgear
    if ((eq >= 0x01 && eq <= 0x0D) || eq === 0x18) return 'weapon';         // swords..doll + fan
    if (eq === 0x0E || eq === 0x0F || eq === 0x17 || eq === 0x19) return 'accessory'; // shields, spellbook, accessory
    if (eq >= 0x10 && eq <= 0x14) return 'body';                            // armor/robe/clothing
    return 'expendable';
  }

  var SHOP_CATEGORY_LABELS = {
    head: 'Head', weapon: 'Weapon', accessory: 'Accessory',
    body: 'Body', expendable: 'Expendable'
  };

  // ktenmain mission ID → wiki scene label. Each ktenmain mission's
  // stronghold set is matched against the ogrebattle64.net scene pages by
  // stronghold-overlap (Jaccard score). 34 of 40 ktenmain mission groups
  // resolve cleanly; missions 1, 41, 46, 47 are likely cutscene or
  // special-event tactical maps that the fan wiki doesn't document
  // separately and fall through to "Mission N".
  var SCENARIO_NAMES = {
    2:  'Scene 5: Zenobian Border',
    3:  'Scene 3: Crenel Canyon',
    4:  'Scene 2: Volmus Mine',
    5:  'Scene 6: Volmus Mine',
    6:  'Scene 1: Tenne Plains',
    7:  'Scene 9: Alba',
    8:  'Scene 8: Dardunnelles',
    9:  'Scene 7: Gunther Piedmont',
    11: 'Scene 4: Mylesia',
    12: 'Scene 20: Gules Hills',
    14: 'Scene 23: Tremos Mountains South',
    16: 'Scene 21: Fair Heights',
    17: 'Scene 27: Temple of Berthe',
    18: 'Scene 24: Capitrium',
    20: 'Scene 26: Celesis',
    21: 'Scene 25: Tremos Mountains North',
    22: 'Scene 13: Sable Lowlands',
    23: 'Scene 14: Audvera Heights',
    24: 'Scene 12: The Highlands of Soathon',
    25: 'Scene 15: Mount Ithaca',
    26: 'Scene 16: Azure Plains',
    27: 'Scene 40: Wentinus',
    28: 'Scene 42: Mount Keryoleth',
    30: 'Scene 36: Tybell',
    31: 'Scene 35: Argent',
    33: 'Scene 22: Vert Plateau',
    34: 'Scene 39: Aurua Plains',
    35: 'Scene 34: Barpheth',
    36: 'Scene 37: Latium',
    37: 'Scene 32: The Blue Basilica',
    38: 'Scene 33: Ptia',
    39: 'Scene 30: Romulus',
    40: 'Scene 43: Aurua Plains',
    43: 'Scene 18: Wentinus',
    44: 'Scene 19: Dardunnelles',
    48: 'Scene 31: Fort Romulus',
  };

  // Stronghold-name overrides for ROM spellings that diverge from the fan
  // wiki (ogrebattle64.net). Most are simple transpositions/typos the
  // matcher caught while doing the scene-overlap analysis; we honor the
  // wiki spelling in the UI but the ROM bytes are left alone.
  var STRONGHOLD_ALIASES = {
    'Amdelm':         'Andelm',
    'Danillof':       'Danillor',
    'Castle Grann':   'Castle Gramm',
    'Purolva':        'Purlova',
    'Cotoltus':       'Cotolus',
    'Crotal Castle':  'Castle Crotal',
    'Nakina':         'Nakima',
    'Chelefteu':      'Cheleftu',
    'Castle Andvari': 'Castle Andarvi',
  };
  function strongholdDisplay(name) {
    return STRONGHOLD_ALIASES[name] || name;
  }
  function scenarioName(missionId) {
    return SCENARIO_NAMES[missionId] || ('Mission ' + missionId);
  }
  // Extract the wiki scene number from the SCENARIO_NAMES label so card sort
  // can use true in-game order (Scene 1 → 2 → 3 → …) rather than ktenmain's
  // internal mission-ID order. Missions with no wiki mapping return Infinity
  // and fall to the end.
  function sceneIdOfMission(missionId) {
    var label = SCENARIO_NAMES[missionId];
    if (!label) return Infinity;
    var m = /^Scene (\d+):/.exec(label);
    return m ? parseInt(m[1], 10) : Infinity;
  }

  // URL for an item-icon PNG in resources/Item Icons/.
  // encodeURIComponent handles spaces and apostrophes safely.
  function itemIconURL(name) {
    if (!name) return null;
    return 'resources/Item%20Icons/' + encodeURIComponent(name) + '.png';
  }

  // All items in the game that belong to a given category, sorted by equipType.
  // For expendable: return ALL 45 consumable master-table records. The modal
  // lets the user toggle shop-visibility (flagHi common/warp ↔ quest) and
  // edit prices per record. "Selected" in the modal maps to "shop-visible"
  // for consumables (flagHi ∈ {0x0000, 0x0200}). See renderShops' modal
  // click-handler for the commit logic.
  function allItemsInCategory(cat) {
    if (cat === 'expendable') {
      if (!rom.consumables) return [];
      return rom.consumables
        .filter(function(c) { return c.flagHi !== 0xFFFF; }) // skip the "None" sentinel
        .map(function(c) {
          return {
            id: -1 - c.index,          // synthetic id (consumables aren't in equipment ID-space)
            name: c.name,
            equipType: null,
            equipTypeName: 'Consumable',
            isConsumable: true,
            consumableIndex: c.index,
            price: c.price,
            flagHi: c.flagHi
          };
        });
    }
    var out = [];
    if (!rom.itemStats) return out;
    for (var i = 1; i < rom.itemStats.length; i++) {
      var stat = rom.itemStats[i];
      if (!stat || stat.equipType === 0 || stat.equipType === 0xFF) continue;
      if (categorizeShopItem(i) !== cat) continue;
      var name = OB64.itemName(i);
      if (!name || name === '(None)') continue;
      out.push({
        id: i, name: name,
        equipType: stat.equipType,
        equipTypeName: OB64.equipTypeName(stat.equipType),
        price: stat.price, isConsumable: false
      });
    }
    out.sort(function(a, b) {
      if (a.equipType !== b.equipType) return a.equipType - b.equipType;
      return a.id - b.id;
    });
    return out;
  }

  function renderShops(panel) {
    panel.innerHTML = '';

    // shopIdx → list of stronghold records (for stronghold names + mission IDs).
    // Skips:
    //   - Mission-objective strongholds (B24=0xFF): boss target; the game
    //     doesn't expose their shop at runtime even when shopIdx is set.
    //     Dev-leftover data on Tacikent (m1) and Dardunnelles (m44).
    //   - ktenmain mission group 1: the opening tutorial. Not documented in
    //     the fan wiki and would otherwise clutter every shop it touches
    //     (Shop #1 via Shafsabus/Tobolisk, Shop #2 via Gerall Avad).
    var shopRecs = {};
    for (var i = 0; i < rom.strongholds.length; i++) {
      var sh = rom.strongholds[i];
      if (sh.shopIdx === 0) continue;
      if (sh.isObjective) continue;
      if (sh.missionId === 1) continue;
      if (!shopRecs[sh.shopIdx]) shopRecs[sh.shopIdx] = [];
      shopRecs[sh.shopIdx].push(sh);
    }

    // Capacity counter — items across all shops vs ROM slot limit
    var total = OB64.totalShopItems(rom.shops);
    var limit = OB64.SHOP_ITEM_LIMIT;
    var counter = document.createElement('div');
    counter.className = 'shop-counter';
    var state = total > limit ? 'over' : (total >= limit - 10 ? 'near' : 'ok');
    counter.classList.add('shop-counter-' + state);
    var countSpan = '<strong>' + total + '</strong> / ' + limit;
    var suffix = '';
    if (state === 'over') {
      suffix = ' <span class="shop-counter-warn">over limit — export may fail due to ROM slot size</span>';
    } else if (state === 'near') {
      suffix = ' <span class="shop-counter-warn">approaching limit</span>';
    }
    counter.innerHTML = 'Items across all shops: ' + countSpan + suffix;
    panel.appendChild(counter);

    var filter = makeFilterBar('Filter by stronghold, scenario, or item name...', function(q) {
      var cards = panel.querySelectorAll('.shop-card');
      for (var i = 0; i < cards.length; i++) {
        cards[i].style.display = cards[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
    });
    panel.appendChild(filter);

    var grid = document.createElement('div');
    grid.className = 'shop-cards';

    // Render order: sort shops by their lowest wiki scene number so cards
    // appear in true in-game progression (Scene 1 Tenne Plains → Scene 2
    // Volmus Mine → …). Ties fall back to ktenmain mission ID, then shop
    // index so shops sharing a scene keep a stable order. Shops with no
    // wiki-mapped stronghold land at the end (min scene = Infinity).
    // A shop is shown if it either holds items OR is referenced by at
    // least one stronghold — that way emptying a real shop doesn't hide
    // its card, but shopcsv padding slots stay hidden.
    var shopOrder = [];
    for (var si = 0; si < rom.shops.length; si++) {
      var shopRecsList = shopRecs[si] || [];
      if (rom.shops[si].items.length === 0 && shopRecsList.length === 0) continue;
      var minScene = Infinity;
      var minMission = Infinity;
      for (var mi = 0; mi < shopRecsList.length; mi++) {
        var rr = shopRecsList[mi];
        var sceneId = sceneIdOfMission(rr.missionId);
        if (sceneId < minScene) minScene = sceneId;
        if (rr.missionId < minMission) minMission = rr.missionId;
      }
      shopOrder.push({ idx: si, minScene: minScene, minMission: minMission });
    }
    shopOrder.sort(function(a, b) {
      if (a.minScene !== b.minScene) return a.minScene - b.minScene;
      if (a.minMission !== b.minMission) return a.minMission - b.minMission;
      return a.idx - b.idx;
    });

    for (var oi = 0; oi < shopOrder.length; oi++) {
      var s = shopOrder[oi].idx;
      var shop = rom.shops[s];

      var card = document.createElement('div');
      card.className = 'shop-card';

      // 64x64 thumbnail (blank for now)
      var thumb = document.createElement('div');
      thumb.className = 'shop-thumb';
      thumb.title = 'Shop #' + s;
      card.appendChild(thumb);

      // Per-shop item counter — warns when the shop exceeds the vanilla
      // max (24). Confirmed 2026-04-19 that a 277-item shop crashes the
      // shop menu on load, so this is a real gameplay limit, not just a
      // display preference.
      var perShopCount = shop.items.length;
      var perShopCap = OB64.SHOP_MAX_ITEMS_PER_SHOP;
      var shopCounter = document.createElement('div');
      shopCounter.className = 'shop-item-count';
      if (perShopCount > perShopCap) shopCounter.classList.add('shop-item-count-over');
      else if (perShopCount >= perShopCap - 3) shopCounter.classList.add('shop-item-count-near');
      shopCounter.textContent = perShopCount + ' / ' + perShopCap + ' items';
      if (perShopCount > perShopCap) {
        shopCounter.title = 'Shops larger than ' + perShopCap + ' items are known to crash the shop menu on load.';
      }
      card.appendChild(shopCounter);

      // Per-stronghold location rows: "Stronghold, Scenario".
      // A single stronghold may appear under multiple mission groups (same
      // town, different scenario) — collapse those into one row and join
      // the scenario names so the card never shows the same stronghold twice.
      var locs = document.createElement('div');
      locs.className = 'shop-locations';
      var recs = shopRecs[s] || [];
      if (recs.length === 0) {
        var none = document.createElement('div');
        none.className = 'shop-location unassigned';
        none.textContent = '(unassigned shop #' + s + ')';
        locs.appendChild(none);
      } else {
        var groupOrder = [];
        var groups = {};
        for (var r = 0; r < recs.length; r++) {
          var rec = recs[r];
          var key = strongholdDisplay(rec.name) || '(no name)';
          if (!groups[key]) { groups[key] = []; groupOrder.push(key); }
          var sn = scenarioName(rec.missionId);
          if (groups[key].indexOf(sn) === -1) groups[key].push(sn);
        }
        for (var g = 0; g < groupOrder.length; g++) {
          var name = groupOrder[g];
          var row = document.createElement('div');
          row.className = 'shop-location';
          row.textContent = name + ', ' + groups[name].join(' / ');
          locs.appendChild(row);
        }
      }
      card.appendChild(locs);

      // Bucket items into the 5 categories
      var buckets = { head: [], weapon: [], accessory: [], body: [], expendable: [] };
      for (var k = 0; k < shop.items.length; k++) {
        buckets[categorizeShopItem(shop.items[k])].push(shop.items[k]);
      }
      // For expendable: show the global consumable list (read-only)
      var expNames = (rom.consumables ? OB64.shopExpendables(rom.consumables) : [])
        .map(function(c) { return c.name; });

      var catsEl = document.createElement('div');
      catsEl.className = 'shop-categories';
      SHOP_CATEGORIES.forEach(function(cat) {
        var section = document.createElement('div');
        section.className = 'shop-category';
        section.dataset.shopIdx = s;
        section.dataset.category = cat;

        var label = document.createElement('div');
        label.className = 'shop-category-label';
        label.textContent = SHOP_CATEGORY_LABELS[cat];
        section.appendChild(label);

        var items = document.createElement('div');
        items.className = 'shop-category-items';
        var names;
        if (cat === 'expendable') {
          names = expNames;
        } else {
          names = buckets[cat].map(function(id) { return OB64.itemName(id); });
        }
        items.textContent = names.length ? names.join(', ') : '(none)';
        section.appendChild(items);

        section.addEventListener('click', shopCategoryClick);
        catsEl.appendChild(section);
      });
      card.appendChild(catsEl);

      grid.appendChild(card);
    }
    panel.appendChild(grid);
  }

  function shopCategoryClick(e) {
    var el = e.currentTarget;
    var shopIdx = parseInt(el.dataset.shopIdx);
    var category = el.dataset.category;
    openItemModal(shopIdx, category);
  }

  // ---------- Shop-item modal ----------
  function openItemModal(shopIdx, category) {
    var shop = rom.shops[shopIdx];
    // Expendable is NOT stored per-shop; the tab contents are a global
    // filter over the consumable master table. We treat the modal as a
    // master-table editor: "selected" = shop-visible (flagHi common/warp),
    // and clicking an item flips its flagHi. Edits propagate to every shop.
    var isConsumableModal = (category === 'expendable');
    var items = allItemsInCategory(category);

    // Initial selection state.
    //   equipment categories → items in THIS shop's inventory
    //   expendable            → items whose flagHi currently passes the shop filter
    var selected = {};
    if (isConsumableModal) {
      items.forEach(function(item) {
        if (item.flagHi === 0x0000 || item.flagHi === 0x0200) selected[item.id] = true;
      });
    } else {
      shop.items.forEach(function(id) {
        if (categorizeShopItem(id) === category) selected[id] = true;
      });
    }

    var overlay = document.createElement('div');
    overlay.className = 'item-modal-overlay';
    overlay.addEventListener('click', function(ev) {
      if (ev.target === overlay) closeModal();
    });

    var modal = document.createElement('div');
    modal.className = 'item-modal';
    overlay.appendChild(modal);

    var header = document.createElement('div');
    header.className = 'item-modal-header';
    var title = document.createElement('h2');
    title.textContent = isConsumableModal
      ? 'Consumables — global list (affects every shop\u2019s Expendable tab)'
      : 'Shop #' + shopIdx + ' — ' + SHOP_CATEGORY_LABELS[category];
    header.appendChild(title);
    var btnClose = document.createElement('button');
    btnClose.className = 'item-modal-close';
    btnClose.textContent = '×';
    btnClose.addEventListener('click', closeModal);
    header.appendChild(btnClose);
    modal.appendChild(header);

    var body = document.createElement('div');
    body.className = 'item-modal-body';

    // Group by equipType so we can emit sub-type separators, max 20 per column
    var columns = [];
    var currentColumn = [];
    var currentType = null;
    items.forEach(function(item) {
      var typeKey = item.isConsumable ? 'consumable' : item.equipType;
      if (currentType !== null && typeKey !== currentType && currentColumn.length >= 4) {
        // Start a new column when sub-type changes AND current has some content
        // (still enforce 20-per-column cap below)
      }
      if (currentColumn.length >= 20) {
        columns.push(currentColumn);
        currentColumn = [];
      }
      if (typeKey !== currentType) {
        currentColumn.push({ kind: 'header', label: item.equipTypeName || 'Consumable' });
        currentType = typeKey;
      }
      currentColumn.push({ kind: 'item', item: item });
    });
    if (currentColumn.length) columns.push(currentColumn);

    columns.forEach(function(colEntries) {
      var col = document.createElement('div');
      col.className = 'item-modal-col';
      colEntries.forEach(function(entry) {
        if (entry.kind === 'header') {
          var h = document.createElement('div');
          h.className = 'item-modal-type-header';
          h.textContent = entry.label;
          col.appendChild(h);
        } else {
          var item = entry.item;
          var row = document.createElement('div');
          row.className = 'item-modal-row';
          if (selected[item.id]) row.classList.add('selected');
          if (!isConsumableModal && !selected[item.id] && !isItemInAnyShop(item.id)) {
            row.classList.add('unused');
          }
          if (item.isConsumable) row.classList.add('consumable-item');

          var img = document.createElement('img');
          img.className = 'item-modal-icon';
          img.src = itemIconURL(item.name);
          img.alt = '';
          img.addEventListener('error', function() { img.style.visibility = 'hidden'; });
          row.appendChild(img);

          var name = document.createElement('span');
          name.className = 'item-modal-name';
          name.textContent = item.name;
          row.appendChild(name);

          // Price chip — shown for equipment and consumables alike.
          // Consumables use a separate price-edit path (writes to
          // rom.consumables[i].price, dirty.consumables).
          if (typeof item.price === 'number' && item.price >= 0) {
            var price = document.createElement('span');
            price.className = 'item-modal-price editable-price';
            price.textContent = item.price + 'g';
            price.title = 'Click to edit price';
            (function(priceEl, itm) {
              priceEl.addEventListener('click', function(ev) {
                ev.stopPropagation();
                beginPriceEdit(priceEl, itm);
              });
            })(price, item);
            row.appendChild(price);
          }

          if (isConsumableModal) {
            row.addEventListener('click', function() {
              var rec = rom.consumables[item.consumableIndex];
              if (!rec) return;
              if (selected[item.id]) {
                delete selected[item.id];
                row.classList.remove('selected');
                rec.flagHi = 0x0100;
                item.flagHi = 0x0100;
              } else {
                selected[item.id] = true;
                row.classList.add('selected');
                rec.flagHi = 0x0000;
                item.flagHi = 0x0000;
              }
              dirty.consumables = true;
              changes++;
              updateStatus();
            });
          } else {
            row.addEventListener('click', function(ev) {
              // Ctrl/Cmd+click: strip this item from every OTHER shop but
              // leave the current shop's selection untouched.
              if (ev.ctrlKey || ev.metaKey) {
                ev.preventDefault();
                var removedFrom = removeItemFromOtherShops(shopIdx, item.id);
                flashHint(hint, removedFrom
                  ? 'Removed ' + item.name + ' from ' + removedFrom + ' other shop' + (removedFrom === 1 ? '' : 's') + '.'
                  : item.name + ' is not in any other shop.');
                updateRowUnused(row, item.id);
                return;
              }
              if (selected[item.id]) {
                delete selected[item.id];
                row.classList.remove('selected');
              } else {
                selected[item.id] = true;
                row.classList.add('selected');
              }
              commitSelectionToShop(shopIdx, category, selected);
              updateRowUnused(row, item.id);
            });
          }
          col.appendChild(row);
        }
      });
      body.appendChild(col);
    });

    modal.appendChild(body);

    // Hint strip at the bottom. Kept as a single persistent element so
    // flashHint() can swap in transient status lines (e.g. after Ctrl+click
    // cascade removal) and revert to the default tip after a couple seconds.
    var hint = document.createElement('div');
    hint.className = 'item-modal-hint';
    hint.dataset.defaultText = isConsumableModal
      ? 'Consumables are global. Toggling an item or editing price affects every shop.'
      : 'Tip: Ctrl+click an item to remove it from every other shop.';
    hint.textContent = hint.dataset.defaultText;
    modal.appendChild(hint);

    document.body.appendChild(overlay);
    function closeModal() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      renderShops(document.getElementById('panel-shops'));
    }
    // ESC to close
    var escHandler = function(ev) { if (ev.key === 'Escape') { closeModal(); document.removeEventListener('keydown', escHandler); } };
    document.addEventListener('keydown', escHandler);
  }

  function flashHint(hint, message) {
    if (!hint) return;
    hint.textContent = message;
    hint.classList.add('item-modal-hint-flash');
    clearTimeout(hint._resetTimer);
    hint._resetTimer = setTimeout(function() {
      hint.textContent = hint.dataset.defaultText || '';
      hint.classList.remove('item-modal-hint-flash');
    }, 2000);
  }

  function removeItemFromOtherShops(currentShopIdx, itemId) {
    var removedFrom = 0;
    for (var i = 0; i < rom.shops.length; i++) {
      if (i === currentShopIdx) continue;
      var shop = rom.shops[i];
      var before = shop.items.length;
      shop.items = shop.items.filter(function(id) { return id !== itemId; });
      if (shop.items.length !== before) removedFrom++;
    }
    if (removedFrom > 0) markChanged();
    return removedFrom;
  }

  function isItemInAnyShop(itemId) {
    for (var i = 0; i < rom.shops.length; i++) {
      if (rom.shops[i].items.indexOf(itemId) !== -1) return true;
    }
    return false;
  }

  function updateRowUnused(row, itemId) {
    if (isItemInAnyShop(itemId)) row.classList.remove('unused');
    else row.classList.add('unused');
  }

  // Inline price edit inside the shop-item modal. The price actually lives
  // in the item-stat table (not the shop), so committing sets dirty.items
  // directly rather than going through markChanged() (which keys off the
  // active tab and would misfile the edit under dirty.shops).
  function beginPriceEdit(priceEl, item) {
    if (priceEl.querySelector('input')) return;
    // Consumables live in the global consumable master table; equipment
    // prices live in the item-stat table. Route writes accordingly.
    var isConsumable = !!item.isConsumable;
    var rec = isConsumable ? rom.consumables[item.consumableIndex] : rom.itemStats[item.id];
    if (!rec) return;
    var cur = rec.price;

    var input = document.createElement('input');
    input.type = 'number';
    input.min = 0;
    input.max = 65535;
    input.value = cur;
    input.className = 'item-modal-price-input';
    priceEl.textContent = '';
    priceEl.appendChild(input);
    input.focus();
    input.select();

    var committed = false;
    function finish(save) {
      if (committed) return;
      committed = true;
      if (save) {
        var val = parseInt(input.value, 10);
        if (!isNaN(val) && val >= 0 && val <= 65535 && val !== cur) {
          rec.price = val;
          item.price = val;
          priceEl.classList.add('price-modified');
          if (isConsumable) dirty.consumables = true;
          else              dirty.items = true;
          changes++;
          updateStatus();
        }
      }
      priceEl.textContent = rec.price + 'g';
    }
    input.addEventListener('click',   function(ev) { ev.stopPropagation(); });
    input.addEventListener('blur',    function() { finish(true); });
    input.addEventListener('keydown', function(ev) {
      ev.stopPropagation();
      if (ev.key === 'Enter')  { finish(true);  input.blur(); }
      if (ev.key === 'Escape') { finish(false); input.blur(); }
    });
  }

  // ---------- Generic error modal ----------
  function showErrorModal(title, message) {
    var overlay = document.createElement('div');
    overlay.className = 'error-modal-overlay';
    overlay.addEventListener('click', function(ev) {
      if (ev.target === overlay) closeErr();
    });

    var modal = document.createElement('div');
    modal.className = 'error-modal';
    overlay.appendChild(modal);

    var header = document.createElement('div');
    header.className = 'error-modal-header';
    var h2 = document.createElement('h2');
    h2.textContent = title;
    header.appendChild(h2);
    var btnX = document.createElement('button');
    btnX.className = 'error-modal-close';
    btnX.textContent = '×';
    btnX.addEventListener('click', closeErr);
    header.appendChild(btnX);
    modal.appendChild(header);

    var body = document.createElement('div');
    body.className = 'error-modal-body';
    body.textContent = message;
    modal.appendChild(body);

    var footer = document.createElement('div');
    footer.className = 'error-modal-footer';
    var btnOk = document.createElement('button');
    btnOk.className = 'error-modal-ok';
    btnOk.textContent = 'OK';
    btnOk.addEventListener('click', closeErr);
    footer.appendChild(btnOk);
    modal.appendChild(footer);

    document.body.appendChild(overlay);
    btnOk.focus();

    function closeErr() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escHandler);
    }
    var escHandler = function(ev) { if (ev.key === 'Escape') closeErr(); };
    document.addEventListener('keydown', escHandler);
  }

  function commitSelectionToShop(shopIdx, category, selected) {
    var shop = rom.shops[shopIdx];
    var kept = shop.items.filter(function(id) { return categorizeShopItem(id) !== category; });
    var newIds = Object.keys(selected).map(function(k) { return parseInt(k, 10); });
    shop.items = kept.concat(newIds);
    markChanged();
  }

  // ============================================================
  // ENEMIES TAB
  // ============================================================
  function renderEnemies(panel) {
    panel.innerHTML = '';
    var filter = makeFilterBar('Filter by index, class, or item name...', function(q) {
      var rows = panel.querySelectorAll('tbody tr');
      for (var i = 0; i < rows.length; i++) {
        rows[i].style.display = rows[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
    });
    panel.appendChild(filter);

    // Class options map including 0=None
    var classOpts = {0: 'None'};
    for (var k in OB64.CLASS_NAMES) classOpts[k] = OB64.CLASS_NAMES[k];
    // Item options map including 0=None
    var itemOpts = {0: 'None'};
    for (var k in OB64.ITEM_NAMES) itemOpts[k] = OB64.ITEM_NAMES[k];

    var table = document.createElement('table');
    table.innerHTML = '<thead><tr>' +
      '<th>#</th>' +
      '<th>Class A</th><th>Ct</th>' +
      '<th>Class B</th><th>Pos</th>' +
      '<th>Class C</th>' +
      '<th>Equip A</th><th>Equip B</th><th>Equip C</th>' +
      '</tr></thead>';
    var tbody = document.createElement('tbody');

    for (var i = 0; i < rom.enemySquads.length; i++) {
      var s = rom.enemySquads[i];
      if (!s.classA && !s.classB && !s.classC) continue;

      var tr = document.createElement('tr');
      tr.id = 'enemy-' + i;
      td(tr, i);

      // Class A (searchable dropdown)
      (function(sq) {
        var c = td(tr, sq.classA ? OB64.className(sq.classA) : '\u2014');
        c.className = 'editable';
        c.addEventListener('click', function() {
          makeSearchableInput(c, classOpts, sq.classA, function(v) {
            sq.classA = v;
            c.textContent = v ? OB64.className(v) : '\u2014';
          });
        });
      })(s);

      // Count A (number)
      (function(sq) {
        var c = td(tr, sq.countA);
        c.className = 'editable num';
        c.addEventListener('click', function() {
          makeNumericInput(c, sq.countA, 0, 10, function(v) {
            sq.countA = v;
            c.textContent = v;
          });
        });
      })(s);

      // Class B (searchable dropdown)
      (function(sq) {
        var c = td(tr, sq.classB ? OB64.className(sq.classB) : '\u2014');
        c.className = 'editable';
        c.addEventListener('click', function() {
          makeSearchableInput(c, classOpts, sq.classB, function(v) {
            sq.classB = v;
            c.textContent = v ? OB64.className(v) : '\u2014';
          });
        });
      })(s);

      // Pos B (number)
      (function(sq) {
        var c = td(tr, sq.posB);
        c.className = 'editable num';
        c.addEventListener('click', function() {
          makeNumericInput(c, sq.posB, 0, 255, function(v) {
            sq.posB = v;
            c.textContent = v;
          });
        });
      })(s);

      // Class C (searchable dropdown)
      (function(sq) {
        var c = td(tr, sq.classC ? OB64.className(sq.classC) : '\u2014');
        c.className = 'editable';
        c.addEventListener('click', function() {
          makeSearchableInput(c, classOpts, sq.classC, function(v) {
            sq.classC = v;
            c.textContent = v ? OB64.className(v) : '\u2014';
          });
        });
      })(s);

      // Equip A (pop-up item picker — shop-tab-style modal)
      (function(sq) {
        var c = td(tr, sq.equipA ? OB64.itemName(sq.equipA) : '\u2014');
        c.className = 'editable';
        c.addEventListener('click', function() {
          openItemPickerFromDict({
            title: 'Equipment slot A',
            options: itemOpts, currentId: sq.equipA,
            onSelect: function(v) {
              sq.equipA = v;
              c.textContent = v ? OB64.itemName(v) : '\u2014';
              c.classList.add('modified');
            }
          });
        });
      })(s);

      // Equip B (pop-up item picker)
      (function(sq) {
        var c = td(tr, sq.equipB ? OB64.itemName(sq.equipB) : '\u2014');
        c.className = 'editable';
        c.addEventListener('click', function() {
          openItemPickerFromDict({
            title: 'Equipment slot B',
            options: itemOpts, currentId: sq.equipB,
            onSelect: function(v) {
              sq.equipB = v;
              c.textContent = v ? OB64.itemName(v) : '\u2014';
              c.classList.add('modified');
            }
          });
        });
      })(s);

      // Equip C (pop-up item picker)
      (function(sq) {
        var c = td(tr, sq.equipC ? OB64.itemName(sq.equipC) : '\u2014');
        c.className = 'editable';
        c.addEventListener('click', function() {
          openItemPickerFromDict({
            title: 'Equipment slot C',
            options: itemOpts, currentId: sq.equipC,
            onSelect: function(v) {
              sq.equipC = v;
              c.textContent = v ? OB64.itemName(v) : '\u2014';
              c.classList.add('modified');
            }
          });
        });
      })(s);

      tbody.appendChild(tr);
    }

    table.appendChild(tbody);
    panel.appendChild(table);
    makeSortable(table);
  }

  // ============================================================
  // MISSIONS TAB
  // ============================================================
  function renderMissions(panel) {
    panel.innerHTML = '';
    var filter = makeFilterBar('Filter by archive # or class name...', function(q) {
      var rows = panel.querySelectorAll('tbody tr');
      for (var i = 0; i < rows.length; i++) {
        rows[i].style.display = rows[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
    });
    panel.appendChild(filter);

    var table = document.createElement('table');
    table.innerHTML = '<thead><tr><th>Archive</th><th>Seq</th><th>Squads</th><th>Nodes</th><th>Extra</th><th>Squad Details</th></tr></thead>';
    var tbody = document.createElement('tbody');

    for (var m = 0; m < rom.esets.length; m++) {
      var eset = rom.esets[m];
      var tr = document.createElement('tr');

      td(tr, '#' + eset.archive);
      td(tr, '0x' + eset.missionSeq.toString(16).padStart(2, '0'));
      td(tr, eset.squadCount);
      td(tr, eset.mapNodeCount);
      td(tr, eset.extraCount);

      // Squad summary with cross-refs
      var tdSquads = document.createElement('td');
      for (var sq = 0; sq < Math.min(eset.squads.length, 8); sq++) {
        var entry = eset.squads[sq];
        var edIdx = entry.enemydatIdx;
        var span = document.createElement('span');
        span.className = 'xref';
        span.textContent = '[' + edIdx + ']';
        span.dataset.enemyIdx = edIdx;
        span.addEventListener('click', jumpToEnemy);
        tdSquads.appendChild(span);

        var label = document.createTextNode(' ' + getSquadLabel(edIdx));
        tdSquads.appendChild(label);

        if (entry.entryType) {
          var tag = document.createTextNode(' (type:' + entry.entryType + ')');
          tdSquads.appendChild(tag);
        }

        if (sq < eset.squads.length - 1) tdSquads.appendChild(document.createElement('br'));
      }
      if (eset.squads.length > 8) {
        tdSquads.appendChild(document.createTextNode('... +' + (eset.squads.length - 8) + ' more'));
      }
      tr.appendChild(tdSquads);

      tbody.appendChild(tr);
    }

    table.appendChild(tbody);
    panel.appendChild(table);
    makeSortable(table);
  }

  function getSquadLabel(edIdx) {
    if (!rom || edIdx >= rom.enemySquads.length) return '(invalid)';
    var s = rom.enemySquads[edIdx];
    if (!s.classA && !s.classB) return '(empty)';
    var label = s.countA + 'x ' + OB64.className(s.classA);
    if (s.classB) label += ' + ' + OB64.className(s.classB);
    if (s.classC) label += ' + ' + OB64.className(s.classC);
    return label;
  }

  function jumpToEnemy(e) {
    var idx = parseInt(e.target.dataset.enemyIdx);
    // Switch to enemies tab
    activeTab = 'enemies';
    var buttons = tabBar.querySelectorAll('button');
    for (var i = 0; i < buttons.length; i++) {
      buttons[i].classList.toggle('active', buttons[i].dataset.tab === 'enemies');
    }
    var panels = document.querySelectorAll('.tab-panel');
    for (var i = 0; i < panels.length; i++) {
      panels[i].classList.toggle('active', panels[i].id === 'panel-enemies');
    }
    renderEnemies(document.getElementById('panel-enemies'));
    var row = document.getElementById('enemy-' + idx);
    if (row) {
      row.scrollIntoView({ behavior: 'smooth', block: 'center' });
      row.style.outline = '2px solid #e94560';
      setTimeout(function() { row.style.outline = ''; }, 2000);
    }
  }

  // ============================================================
  // SCENARIOS TAB
  // ============================================================
  function renderScenarios(panel) {
    panel.innerHTML = '';
    var filter = makeFilterBar('Filter by archive #, class name, or flag...', function(q) {
      var rows = panel.querySelectorAll('tbody tr');
      for (var i = 0; i < rows.length; i++) {
        rows[i].style.display = rows[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
    });
    panel.appendChild(filter);

    var table = document.createElement('table');
    table.innerHTML = '<thead><tr><th>Archive</th><th>Entries</th><th>EnemyDat Range</th><th>Squad Details</th></tr></thead>';
    var tbody = document.createElement('tbody');

    for (var s = 0; s < rom.scincsvs.length; s++) {
      var sc = rom.scincsvs[s];
      var tr = document.createElement('tr');

      td(tr, '#' + sc.archive);
      td(tr, sc.count);

      // EnemyDat range (min-max index)
      var minIdx = Infinity, maxIdx = -Infinity;
      for (var e = 0; e < sc.entries.length; e++) {
        var idx = sc.entries[e].enemydatIdx;
        if (idx < minIdx) minIdx = idx;
        if (idx > maxIdx) maxIdx = idx;
      }
      td(tr, minIdx === maxIdx ? '' + minIdx : minIdx + '-' + maxIdx);

      // Squad details: enemydat cross-ref + flag label + squad composition
      var tdDetail = document.createElement('td');
      for (var e = 0; e < sc.entries.length; e++) {
        var entry = sc.entries[e];
        var edIdx = entry.enemydatIdx;

        // Cross-ref link to Enemies tab
        var span = document.createElement('span');
        span.className = 'xref';
        span.textContent = '[' + edIdx + ']';
        span.dataset.enemyIdx = edIdx;
        span.addEventListener('click', jumpToEnemy);
        tdDetail.appendChild(span);

        // Squad composition label
        var label = document.createTextNode(' ' + getSquadLabel(edIdx));
        tdDetail.appendChild(label);

        // Flag badge
        var flagName = OB64.scincsvFlagName(entry.flags);
        if (flagName !== 'None') {
          var badge = document.createElement('span');
          badge.className = 'flag-badge flag-' + flagName.toLowerCase();
          badge.textContent = flagName;
          tdDetail.appendChild(document.createTextNode(' '));
          tdDetail.appendChild(badge);
        }

        if (e < sc.entries.length - 1) tdDetail.appendChild(document.createElement('br'));
      }
      tr.appendChild(tdDetail);

      tbody.appendChild(tr);
    }

    table.appendChild(tbody);
    panel.appendChild(table);
    makeSortable(table);
  }

  // ============================================================
  // ITEMS TAB
  // ============================================================
  function renderItems(panel) {
    panel.innerHTML = '';
    var filter = makeFilterBar('Filter by name, type, element, or category...', function(q) {
      var rows = panel.querySelectorAll('tbody tr');
      for (var i = 0; i < rows.length; i++) {
        rows[i].style.display = rows[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
    });
    panel.appendChild(filter);

    var table = document.createElement('table');
    table.className = 'items-table';
    table.innerHTML = '<thead><tr>' +
      '<th>ID</th>' +
      '<th>Name</th>' +
      '<th>Type</th>' +
      '<th>Elem</th>' +
      '<th>Gr</th>' +
      '<th>Price</th>' +
      '<th title="Strength">STR</th>' +
      '<th title="Intelligence">INT</th>' +
      '<th title="Agility">AGI</th>' +
      '<th title="Dexterity">DEX</th>' +
      '<th title="Vitality">VIT</th>' +
      '<th title="Mentality">MEN</th>' +
      '<th title="Physical Resistance">Phys</th>' +
      '<th title="Wind Resistance">Wind</th>' +
      '<th title="Fire Resistance">Fire</th>' +
      '<th title="Earth Resistance">Earth</th>' +
      '<th title="Water Resistance">Water</th>' +
      '<th title="Virtue Resistance">Virt</th>' +
      '<th title="Bane Resistance">Bane</th>' +
      '</tr></thead>';
    var tbody = document.createElement('tbody');

    // Stat field definitions: field name, CSS class, raw field name
    var STAT_FIELDS = [
      { field: 'str', css: 'stat-atk', raw: 'strRaw' },
      { field: 'int', css: 'stat-int', raw: 'intRaw' },
      { field: 'agi', css: 'stat-agi', raw: 'agiRaw' },
      { field: 'dex', css: 'stat-dex', raw: 'dexRaw' },
      { field: 'vit', css: 'stat-vit', raw: 'vitRaw' },
      { field: 'men', css: 'stat-men', raw: 'menRaw' },
    ];

    var RES_FIELDS = [
      { field: 'resPhys', label: 'Phys' },
      { field: 'resWind', label: 'Wind' },
      { field: 'resFire', label: 'Fire' },
      { field: 'resEarth', label: 'Earth' },
      { field: 'resWater', label: 'Water' },
      { field: 'resVirtue', label: 'Virt' },
      { field: 'resBane', label: 'Bane' },
    ];

    for (var i = 1; i < rom.itemStats.length; i++) { // skip index 0 (sentinel)
      var item = rom.itemStats[i];
      if (item.equipType === 0xFF) continue; // skip sentinel entries
      var tr = document.createElement('tr');

      // ID
      td(tr, '0x' + item.gameId.toString(16).padStart(2, '0'));

      // Name
      var tdName = td(tr, OB64.itemName(item.gameId));
      tdName.className = 'item-name';

      // Equip Type (dropdown)
      (function(itm) {
        var tdType = td(tr, OB64.equipTypeName(itm.equipType));
        tdType.className = 'equip-type editable';
        tdType.addEventListener('click', function() {
          makeDropdown(tdType, OB64.EQUIP_TYPES, itm.equipType, function(v) {
            itm.equipType = v;
            tdType.textContent = OB64.equipTypeName(v);
          });
        });
      })(item);

      // Element (dropdown)
      (function(itm) {
        var tdElem = td(tr, OB64.elementName(itm.element));
        tdElem.className = 'element element-' + itm.element + ' editable';
        tdElem.addEventListener('click', function() {
          makeDropdown(tdElem, OB64.ELEMENT_NAMES, itm.element, function(v) {
            itm.element = v;
            tdElem.textContent = OB64.elementName(v);
            tdElem.className = 'element element-' + v + ' editable modified';
          });
        });
      })(item);

      // Grade (number input)
      (function(itm) {
        var tdGrade = td(tr, itm.grade || '\u2014');
        tdGrade.className = 'num editable' + (itm.grade ? '' : ' dim');
        tdGrade.addEventListener('click', function() {
          makeNumericInput(tdGrade, itm.grade, 0, 255, function(v) {
            itm.grade = v;
            tdGrade.textContent = v || '\u2014';
            tdGrade.classList.toggle('dim', !v);
          });
        });
      })(item);

      // Price (editable)
      var tdPrice = document.createElement('td');
      tdPrice.className = 'editable num';
      tdPrice.textContent = item.price;
      tdPrice.dataset.itemIdx = i;
      tdPrice.dataset.field = 'price';
      tdPrice.addEventListener('click', itemCellClick);
      tr.appendChild(tdPrice);

      // 6 character stats — STR, INT, AGI, DEX, VIT, MEN (all signed, all editable)
      for (var sf = 0; sf < STAT_FIELDS.length; sf++) {
        var def = STAT_FIELDS[sf];
        var val = item[def.field];
        var tdStat = document.createElement('td');
        if (val !== 0) {
          tdStat.textContent = val;
          tdStat.className = (val < 0) ? 'num stat-neg editable' : 'num ' + def.css + ' editable';
        } else {
          tdStat.textContent = '\u2014';
          tdStat.className = 'num dim';
        }
        tdStat.dataset.itemIdx = i;
        tdStat.dataset.field = def.field;
        tdStat.addEventListener('click', itemCellClick);
        tr.appendChild(tdStat);
      }

      // 7 Resistances — each in its own column, editable
      for (var ri = 0; ri < RES_FIELDS.length; ri++) {
        (function(resIdx, itm) {
          var rdef = RES_FIELDS[resIdx];
          var rv = itm[rdef.field];
          var tdR = document.createElement('td');
          if (rv !== 0) {
            tdR.textContent = rv;
            tdR.className = (rv < 0) ? 'num stat-neg editable' : 'num stat-def editable';
          } else {
            tdR.textContent = '\u2014';
            tdR.className = 'num dim';
          }
          tdR.dataset.itemIdx = i;
          tdR.dataset.field = rdef.field;
          tdR.addEventListener('click', itemCellClick);
          tr.appendChild(tdR);
        })(ri, item);
      }

      tbody.appendChild(tr);
    }

    table.appendChild(tbody);
    panel.appendChild(table);
    makeSortable(table);
  }

  // Signed stat fields (all character stats + resistances)
  var SIGNED_ITEM_FIELDS = {
    str: 'strRaw', int: 'intRaw', agi: 'agiRaw', dex: 'dexRaw',
    vit: 'vitRaw', men: 'menRaw', b12: 'b12Raw',
    resPhys: true, resWind: true, resFire: true, resEarth: true,
    resWater: true, resVirtue: true, resBane: true,
  };

  function itemCellClick(e) {
    var tcell = e.currentTarget;
    if (tcell.querySelector('input')) return;
    var idx = parseInt(tcell.dataset.itemIdx);
    var field = tcell.dataset.field;
    var item = rom.itemStats[idx];

    // Signed fields: all stats and resistances. Unsigned: price only.
    var isSigned = field in SIGNED_ITEM_FIELDS;
    var maxVal = field === 'price' ? 65535 : 127;
    var minVal = isSigned ? -128 : 0;
    var curVal = item[field];

    var input = document.createElement('input');
    input.type = 'number';
    input.min = minVal;
    input.max = maxVal;
    input.value = curVal;
    tcell.textContent = '';
    tcell.appendChild(input);
    input.focus();
    input.select();

    input.addEventListener('blur', function() { commitItemEdit(tcell, input, idx, field, minVal, maxVal, isSigned); });
    input.addEventListener('keydown', function(ev) {
      if (ev.key === 'Enter') commitItemEdit(tcell, input, idx, field, minVal, maxVal, isSigned);
      if (ev.key === 'Escape') { tcell.textContent = curVal; }
    });
  }

  function commitItemEdit(tcell, input, idx, field, minVal, maxVal, isSigned) {
    var val = parseInt(input.value);
    if (!isNaN(val) && val >= minVal && val <= maxVal) {
      if (val !== rom.itemStats[idx][field]) {
        rom.itemStats[idx][field] = val;
        // Update raw byte value for export
        if (isSigned) {
          var rawField = SIGNED_ITEM_FIELDS[field];
          if (typeof rawField === 'string') {
            rom.itemStats[idx][rawField] = val < 0 ? val + 256 : val;
          }
        }
        tcell.classList.add('modified');
        markChanged();
      }
    }
    tcell.textContent = rom.itemStats[idx][field];
  }

  // ============================================================
  // CLASSES TAB
  // ============================================================

  // Build a lookup from class definition records to class IDs.
  // The class def table has 166 x 72-byte records at ROM 0x5DAD8.
  // Mapping: record_index = class_id + 1.
  // Verified by cross-referencing all VANILLA class stats/growths/resistances/
  // combat multipliers from the H2F Mod class chart CSV against ROM hex data.
  // Every field matches perfectly with offset +1 for all 14+ tested classes.
  // Record 0 = pointer table (header). Record 1 = 0xFFFF terminator (class 0x00 "None").
  // Records 2-N cover class IDs 0x01 (Soldier) through 0xA4 (Death Bahamut /
  // Grozz Nuy) per the GameShark mapping — 164 classes total, with intermediate
  // terminator/sentinel rows that the inner guard skips.
  function buildClassDefMap(classDefs) {
    var map = {}; // classId -> array of def records
    for (var cid = 0; cid <= 0xA4; cid++) {
      var ri = cid + 1;
      if (ri < classDefs.length) {
        var r = classDefs[ri];
        if (!r.isTerm && !r.isSentinel) {
          map[cid] = [r];
        }
      }
    }
    return map;
  }

  // Build evolution lookup by deriving promotion links from ROM data.
  //
  // Promotion data comes from class def bytes B54-56 (parsed as reqLevel,
  // reqClass, reqClassLevel). B55 = required class ID: if class X has
  // B55 = Y, then Y promotes to X. This gives us all intermediate→advanced
  // promotion links directly from the ROM.
  //
  // For intermediate classes (B55=0, B54>0), the required BASE class
  // (Fighter or Amazon) is encoded in the MIPS handler functions at
  // ROM 0x1AB030, not in the class def table. Those links are not shown.
  //
  // The evolution table at ROM 0x654A0 is only used for tier display —
  // its "tree" field is per-category (not global) and cannot derive chains.
  function buildEvolutionLookup(classEvolution) {
    var defMap = buildClassDefMap(rom.classDefs);

    // Derive promotions by reversing B55 (reqClass) across all class defs
    var promotions = {}; // classId -> [target classIds]
    var demotions = {};  // classId -> [source classIds]
    for (var cid = 0; cid <= 0xA4; cid++) {
      var defs = defMap[cid];
      if (!defs || defs.length === 0) continue;
      var def = defs[0];
      if (def.reqClass > 0) {
        // This class requires reqClass → reqClass promotes to this class
        var src = def.reqClass;
        if (!promotions[src]) promotions[src] = [];
        if (promotions[src].indexOf(cid) === -1) {
          promotions[src].push(cid);
        }
        if (!demotions[cid]) demotions[cid] = [];
        if (demotions[cid].indexOf(src) === -1) {
          demotions[cid].push(src);
        }
      }
    }

    // Build class -> evolution entry lookup (for tier display only)
    var byClass = {};
    for (var i = 0; i < classEvolution.length; i++) {
      var e = classEvolution[i];
      if (!e.isSeparator) byClass[e.classId] = e;
    }
    return { promotions: promotions, demotions: demotions, byClass: byClass };
  }

  // Stat order in class def table (B0-B23): STR, VIT, INT, MEN, AGI, DEX
  // Confirmed by cross-referencing H2F Mod class chart CSV against ROM hex data.
  var CLASS_STAT_NAMES = ['STR', 'VIT', 'INT', 'MEN', 'AGI', 'DEX'];
  var CLASS_STAT_CSS = ['stat-atk', 'stat-vit', 'stat-int', 'stat-men', 'stat-agi', 'stat-dex'];

  function renderClasses(panel) {
    panel.innerHTML = '';

    var filter = makeFilterBar('Filter by class name, category, or ID...', function(q) {
      // Filter both table rows and cards
      var rows = panel.querySelectorAll('.classes-table tbody tr');
      for (var i = 0; i < rows.length; i++) {
        rows[i].style.display = rows[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
      var cards = panel.querySelectorAll('.class-card');
      for (var i = 0; i < cards.length; i++) {
        cards[i].style.display = cards[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
    });
    panel.appendChild(filter);

    // View toggle
    var viewToggle = document.createElement('div');
    viewToggle.className = 'class-view-toggle';
    var btnTable = document.createElement('button');
    btnTable.textContent = 'Table View';
    btnTable.className = 'active';
    var btnCards = document.createElement('button');
    btnCards.textContent = 'Card View';
    viewToggle.appendChild(btnTable);
    viewToggle.appendChild(btnCards);
    panel.appendChild(viewToggle);

    var tableContainer = document.createElement('div');
    tableContainer.id = 'classes-table-view';
    var cardsContainer = document.createElement('div');
    cardsContainer.id = 'classes-card-view';
    cardsContainer.className = 'class-cards';
    cardsContainer.style.display = 'none';

    btnTable.addEventListener('click', function() {
      btnTable.classList.add('active');
      btnCards.classList.remove('active');
      tableContainer.style.display = '';
      cardsContainer.style.display = 'none';
    });
    btnCards.addEventListener('click', function() {
      btnCards.classList.add('active');
      btnTable.classList.remove('active');
      tableContainer.style.display = 'none';
      cardsContainer.style.display = '';
    });

    // Data
    var defMap = buildClassDefMap(rom.classDefs);
    var evoLookup = buildEvolutionLookup(rom.classEvolution);

    // Build per-slot filtered item dropdowns based on item stat B0=equipType.
    //
    //   Weapon slot: filtered per-class by the class's current weapon type so
    //                animations stay consistent (Fighter with 1H Sword only
    //                sees other 1H Swords). `weaponsByType[equipType]` has a
    //                map per weapon type; `allWeapons` is the fallback for
    //                classes with no default weapon (beast/dragon classes).
    //
    //   Body slot:   armor/robes/clothing (0x10-0x14)
    //   Head slot:   helms/headgear (0x15-0x16)
    //   Accessory:   shields, spellbooks, amulets (0x0E, 0x0F, 0x17, 0x19) —
    //                the "off-hand" slot in the char struct (+0x2F).
    //
    // Every map starts with a "None [0]" entry so users can explicitly clear
    // a slot from the dropdown.
    var weaponsByType = {};
    var allWeapons = { 0: 'None' };
    var bodyItems = { 0: 'None' };
    var headItems = { 0: 'None' };
    var accessoryItems = { 0: 'None' };
    for (var _k in OB64.ITEM_NAMES) {
      var _iid = parseInt(_k);
      var _stat = rom.itemStats[_iid];
      if (!_stat) continue;
      var _et = _stat.equipType;
      var _name = OB64.ITEM_NAMES[_k];
      if ((_et >= 0x01 && _et <= 0x0D) || _et === 0x18) {
        if (!weaponsByType[_et]) weaponsByType[_et] = { 0: 'None' };
        weaponsByType[_et][_iid] = _name;
        allWeapons[_iid] = _name;
      } else if (_et >= 0x10 && _et <= 0x14) {
        bodyItems[_iid] = _name;
      } else if (_et === 0x15 || _et === 0x16) {
        headItems[_iid] = _name;
      } else if (_et === 0x0E || _et === 0x0F || _et === 0x17 || _et === 0x19) {
        accessoryItems[_iid] = _name;
      }
    }

    // Options map for a class's weapon slot — restricted to the current
    // default weapon's type. Falls back to all weapons when the class has
    // no default weapon.
    function getWeaponOptions(def) {
      var wpnId = def && def.defaultEquip[0];
      if (wpnId && rom.itemStats[wpnId]) {
        var t = rom.itemStats[wpnId].equipType;
        if (weaponsByType[t]) return weaponsByType[t];
      }
      return allWeapons;
    }

    // Options map for a defaultEquip slot index (0..3). Used by both the
    // table view (via COL_TO_SLOT) and the card view (iterates slots directly).
    function getSlotOptions(slotIdx, def) {
      if (slotIdx === 0) return getWeaponOptions(def);
      if (slotIdx === 1) return bodyItems;
      if (slotIdx === 2) return accessoryItems;
      return headItems; // slotIdx === 3
    }

    // Collect all class IDs
    var allClassIds = [];
    for (var id in OB64.CLASS_NAMES) {
      var nid = parseInt(id);
      if (nid > 0) allClassIds.push(nid);
    }
    allClassIds.sort(function(a, b) { return a - b; });

    // Helper: get class category string
    function getCatName(cid) {
      for (var cat in OB64.CLASS_CATEGORIES) {
        if (OB64.CLASS_CATEGORIES[cat].indexOf(cid) >= 0) return cat;
      }
      return '';
    }

    // Helper: render promo links into a td
    function renderPromoLinks(container, ids) {
      if (ids && ids.length > 0) {
        for (var p = 0; p < ids.length; p++) {
          if (p > 0) container.appendChild(document.createTextNode(', '));
          var link = document.createElement('span');
          link.className = 'xref';
          link.textContent = OB64.className(ids[p]);
          link.dataset.classId = ids[p];
          link.addEventListener('click', jumpToClass);
          container.appendChild(link);
        }
      } else {
        container.textContent = '\u2014';
        container.className += ' dim';
      }
    }

    // Helper: alignment label
    function alnLabel(al) {
      if (al >= 70) return al + ' (Lawful)';
      if (al >= 55) return al + ' (Lawful-leaning)';
      if (al >= 45) return al + ' (Neutral)';
      if (al >= 30) return al + ' (Chaotic-leaning)';
      return al + ' (Chaotic)';
    }

    var RES_SHORT = ['Phys', 'Air', 'Fire', 'Earth', 'Water', 'Virtue', 'Bane'];
    // Display-column labels (table view order: Wpn, Body, Head, Acc)
    var EQUIP_LABELS = ['Wpn', 'Body', 'Head', 'Acc'];
    // Slot-indexed labels (defaultEquip[0..3] = weapon, body, off-hand/acc, head)
    var SLOT_LABELS_FULL = ['Weapon', 'Body', 'Accessory', 'Headgear'];
    // Maps a display-column index (0..3) to the defaultEquip slot index it shows.
    // Col 2 (Head) reads slot 3, col 3 (Acc) reads slot 2 — the class def order
    // is [weapon, body, off-hand, head] but we want Head before Acc visually.
    var COL_TO_SLOT = [0, 1, 3, 2];

    // ---- TABLE VIEW (sub-tabbed by field group) ----
    //
    // The classes table is split into 4 sub-views (Stats / Equipment & Combat /
    // Promotion / Unit Info) so column count stays scannable. A second-level
    // toggle under the main Table/Card switch picks the active sub-view; the
    // choice is persisted in localStorage.
    var TABLE_SUBVIEWS = [
      { id: 'stats',     label: 'Stats' },
      { id: 'combat',    label: 'Equipment & Combat' },
      { id: 'promotion', label: 'Promotion' },
      { id: 'unit',      label: 'Unit Info' }
    ];
    var activeSubview = localStorage.getItem('ob64_classes_subview') || 'stats';
    if (!TABLE_SUBVIEWS.some(function(s) { return s.id === activeSubview; })) {
      activeSubview = 'stats';
    }

    var subviewBar = document.createElement('div');
    subviewBar.className = 'classes-subview-toggle';
    var subviewBtns = {};
    TABLE_SUBVIEWS.forEach(function(sv) {
      var b = document.createElement('button');
      b.textContent = sv.label;
      if (sv.id === activeSubview) b.classList.add('active');
      b.addEventListener('click', function() {
        activeSubview = sv.id;
        localStorage.setItem('ob64_classes_subview', sv.id);
        for (var k in subviewBtns) subviewBtns[k].classList.toggle('active', k === sv.id);
        renderSubview();
      });
      subviewBtns[sv.id] = b;
      subviewBar.appendChild(b);
    });
    tableContainer.appendChild(subviewBar);

    var tableHost = document.createElement('div');
    tableContainer.appendChild(tableHost);

    // Cell builders — each appends exactly ONE <td>. `def` is passed in so the
    // click-handler closures capture the correct class-def record.
    function addNumericCell(tr, def, field, max, extraCls) {
      var c = td(tr, def ? def[field] : 0);
      c.className = 'editable' + (extraCls ? ' ' + extraCls : '');
      if (def) {
        c.addEventListener('click', function() {
          makeNumericInput(c, def[field], 0, max, function(nv) {
            def[field] = nv;
            c.textContent = nv;
            markChanged();
          });
        });
      }
      return c;
    }
    function addRawByteCell(tr, def, field, hint) {
      var c = addNumericCell(tr, def, field, 255, 'raw-byte');
      c.title = hint || 'Uncertain field \u2014 edit with caution';
      return c;
    }
    function addStatBaseCell(tr, def, statIdx) {
      var v = def && def.stats.length > statIdx ? def.stats[statIdx].base : 0;
      var c = td(tr, v);
      c.className = 'editable ' + CLASS_STAT_CSS[statIdx];
      if (def) {
        c.addEventListener('click', function() {
          makeNumericInput(c, def.stats[statIdx].base, 0, 65535, function(nv) {
            def.stats[statIdx].base = nv;
            c.textContent = nv;
            markChanged();
          });
        });
      }
      return c;
    }
    function addStatGrowthCell(tr, def, statIdx, growthField) {
      var c = td(tr, def ? def[growthField] : 0);
      c.className = 'editable col-growth';
      c.title = 'Growth mean (B' + (2 + statIdx * 4) + ') \u2014 confirmed via level-up diff';
      if (def) {
        c.addEventListener('click', function() {
          makeNumericInput(c, def[growthField], 0, 255, function(nv) {
            def[growthField] = nv;
            def.stats[statIdx].g1 = nv; // keep legacy stats[i].g1 in sync for the serializer
            c.textContent = nv;
            markChanged();
          });
        });
      }
      return c;
    }
    function addResCell(tr, def, resIdx) {
      var rv = def && def.resistances.length > resIdx ? def.resistances[resIdx] : 50;
      var c = td(tr, rv);
      c.className = 'editable';
      if (rv < 50) c.classList.add('resist-strong');
      else if (rv > 50) c.classList.add('resist-weak');
      if (def) {
        c.addEventListener('click', function() {
          makeNumericInput(c, def.resistances[resIdx], 0, 255, function(nv) {
            def.resistances[resIdx] = nv;
            c.textContent = nv;
            c.classList.remove('resist-strong', 'resist-weak');
            if (nv < 50) c.classList.add('resist-strong');
            else if (nv > 50) c.classList.add('resist-weak');
            markChanged();
          });
        });
      }
      return c;
    }
    function addDropdownCell(tr, def, field, optTable, nameFn) {
      var c = td(tr, def ? nameFn(def[field]) : '\u2014');
      c.className = 'editable';
      if (def) {
        c.addEventListener('click', function() {
          makeDropdown(c, optTable, def[field], function(nv) {
            def[field] = nv;
            c.textContent = nameFn(nv);
            markChanged();
          });
        });
      }
      return c;
    }
    function addEquipCell(tr, def, displayCol) {
      var slotIdx = COL_TO_SLOT[displayCol];
      var itemId = def && def.defaultEquip.length > slotIdx ? def.defaultEquip[slotIdx] : 0;
      var c = td(tr, itemId > 0 ? OB64.itemName(itemId) : '\u2014');
      c.className = 'editable equip-config';
      c.title = EQUIP_LABELS[displayCol] + ': 0x' + itemId.toString(16).padStart(4, '0');
      if (def) {
        c.addEventListener('click', function() {
          var opts = getSlotOptions(slotIdx, def);
          openItemPickerFromDict({
            title: 'Default ' + EQUIP_LABELS[displayCol] + ' \u2014 ' + (def.name || ''),
            options: opts, currentId: def.defaultEquip[slotIdx],
            onSelect: function(nv) {
              def.defaultEquip[slotIdx] = nv;
              c.textContent = nv > 0 ? OB64.itemName(nv) : '\u2014';
              c.title = EQUIP_LABELS[displayCol] + ': 0x' + nv.toString(16).padStart(4, '0');
              c.classList.add('modified');
            }
          });
        });
      }
      return c;
    }
    // Read-only cell for data that isn't part of the class def (e.g. stat-gate
    // thresholds from the LZSS-compressed block, back-row attack counts from
    // wiki data). Rendered with .read-only-cell for a muted look.
    function addReadOnlyCell(tr, text, title, extraCls) {
      var c = td(tr, text === null || text === undefined ? '\u2014' : text);
      c.className = 'read-only-cell' + (extraCls ? ' ' + extraCls : '');
      if (title) c.title = title;
      return c;
    }
    // Editable stat-gate cell. Writes to rom.statGates.byClass[cid][field]
    // and sets dirty.statGates directly (bypasses markChanged's tab-based
    // mapping, which would misfile the edit under dirty.classDefs).
    // For classes without a gate record, falls back to read-only dash.
    function addStatGateCell(tr, gate, field, title) {
      if (!gate) return addReadOnlyCell(tr, '\u2014', title, 'col-gate');
      var v = gate[field] | 0;
      var c = td(tr, v || '\u2014');
      c.className = 'editable col-gate';
      if (title) c.title = title;
      c.addEventListener('click', function() {
        makeNumericInput(c, gate[field] | 0, 0, 255, function(nv) {
          gate[field] = nv & 0xFF;
          c.textContent = nv || '\u2014';
          dirty.statGates = true;
          changes++;
          updateStatus();
        });
      });
      return c;
    }

    function addReqClassCell(tr, def) {
      var c = td(tr, def && def.reqClass > 0 ? OB64.className(def.reqClass) : '\u2014');
      c.className = 'editable';
      if (def) {
        c.addEventListener('click', function() {
          var classOpts = { 0: 'None' };
          for (var k in OB64.CLASS_NAMES) classOpts[k] = OB64.CLASS_NAMES[k];
          makeSearchableInput(c, classOpts, def.reqClass, function(nv) {
            def.reqClass = nv;
            c.textContent = nv > 0 ? OB64.className(nv) : '\u2014';
            markChanged();
          });
        });
      }
      return c;
    }

    function renderSubview() {
      tableHost.innerHTML = '';
      var table = document.createElement('table');
      table.className = 'classes-table';

      var cols; // [{label, title?, cls?}]
      var fillRow; // function(cid, tr, def)

      if (activeSubview === 'stats') {
        cols = [
          { label: 'ID', cls: 'col-sticky' }, { label: 'Name', cls: 'col-sticky-name' },
          { label: 'STR', title: 'B0 base (u16)' }, { label: 'STR-g', title: 'B2 growth mean', cls: 'col-growth' },
          { label: 'VIT', title: 'B4 base' }, { label: 'VIT-g', title: 'B6 growth', cls: 'col-growth' },
          { label: 'INT', title: 'B8 base' }, { label: 'INT-g', title: 'B10 growth', cls: 'col-growth' },
          { label: 'MEN', title: 'B12 base' }, { label: 'MEN-g', title: 'B14 growth', cls: 'col-growth' },
          { label: 'AGI', title: 'B16 base' }, { label: 'AGI-g', title: 'B18 growth', cls: 'col-growth' },
          { label: 'DEX', title: 'B20 base' }, { label: 'DEX-g', title: 'B22 growth', cls: 'col-growth' },
          { label: 'LCK', title: 'B23 Luck base (40-60 typical)' },
          { label: 'ALN', title: 'B24 Alignment (0-100)' },
          { label: 'Phys' }, { label: 'Wind' }, { label: 'Fire' }, { label: 'Earth' },
          { label: 'Water' }, { label: 'Virt' }, { label: 'Bane' }
        ];
        var STAT_GROWTH_FIELDS = ['strGrowth', 'vitGrowth', 'intGrowth', 'menGrowth', 'agiGrowth', 'dexGrowth'];
        fillRow = function(cid, tr, def) {
          for (var si = 0; si < 6; si++) {
            addStatBaseCell(tr, def, si);
            addStatGrowthCell(tr, def, si, STAT_GROWTH_FIELDS[si]);
          }
          addNumericCell(tr, def, 'lck', 255);
          addNumericCell(tr, def, 'alignment', 100);
          for (var ri = 0; ri < 7; ri++) addResCell(tr, def, ri);
        };
      } else if (activeSubview === 'combat') {
        cols = [
          { label: 'ID', cls: 'col-sticky' }, { label: 'Name', cls: 'col-sticky-name' },
          { label: 'Move', title: 'B32 movement type' },
          { label: 'Wpn', title: 'B34-35 default weapon' },
          { label: 'Body', title: 'B36-37 default body armor' },
          { label: 'Head', title: 'B40-41 default headgear' },
          { label: 'Acc', title: 'B38-39 default accessory/off-hand' },
          { label: 'FrontAtks', title: 'B44 front row attack count (verified in-game)' },
          { label: 'MidAtks', title: 'B46 middle row attack count (verified in-game)' },
          { label: 'RearAtks', title: 'B48 rear row attack count (verified via "Class Chart.csv" cross-check, 79/79 match). Previously mislabeled "atkType".' },
          { label: 'PAtk', title: 'B49 physical attack multiplier' },
          { label: 'MAtk', title: 'B50 magic attack multiplier' },
          { label: 'PDef', title: 'B51 physical defense multiplier' },
          { label: 'MDef', title: 'B52 magic defense multiplier' },
          { label: 'Flags', title: 'B53 combat flags \u2014 not decoded', cls: 'col-raw' },
          { label: 'FixEq', title: 'B42 \u2014 fixed-equip-slots bitmask (0x01=Wpn, 0x02=Offhand, 0x04=Body, 0x08=Head). Identified via CSV "Fixed Equips" column.', cls: 'col-raw' },
          { label: 'B43', title: 'B43 \u2014 unknown, often matches B45 (front attack ID) but not always', cls: 'col-raw' },
          { label: 'F-AtkID', title: 'B45 \u2014 front-row attack ID (index into combat action table at ROM 0x60988). e.g. Thrust=1, Slash=4, Strike=9, [Elem. Magic]=45. Identified via CSV.', cls: 'col-raw' },
          { label: 'R-AtkID', title: 'B47 \u2014 rear-row attack ID (combat action table index). May differ from B45 for caster/boss classes (e.g. Valkyrie front=Cleave(5), rear=Lightning(51)). Mid-row reuses B45.', cls: 'col-raw' }
        ];
        fillRow = function(cid, tr, def) {
          addDropdownCell(tr, def, 'moveType', OB64.MOVEMENT_TYPES, OB64.moveTypeName);
          for (var ei = 0; ei < 4; ei++) addEquipCell(tr, def, ei);
          addNumericCell(tr, def, 'frontAtks', 255);
          addNumericCell(tr, def, 'midAtks', 255);
          addNumericCell(tr, def, 'rearAtks', 255);
          addNumericCell(tr, def, 'physAtk', 255);
          addNumericCell(tr, def, 'magAtk', 255);
          addNumericCell(tr, def, 'physDef', 255);
          addNumericCell(tr, def, 'magDef', 255);
          addRawByteCell(tr, def, 'flagsRaw', 'B53 combat flags \u2014 not decoded');
          addRawByteCell(tr, def, 'b42Raw', 'B42 \u2014 fixed-equip-slots bitmask: 0x01=Wpn, 0x02=Offhand, 0x04=Body, 0x08=Head');
          addRawByteCell(tr, def, 'b43Raw', 'B43 \u2014 unknown, often equals B45 (front attack ID)');
          addRawByteCell(tr, def, 'b45Raw', 'B45 \u2014 front-row attack ID (combat action table index). Thrust=1, Slash=4, Strike=9, [Elem. Magic]=45');
          addRawByteCell(tr, def, 'b47Raw', 'B47 \u2014 rear-row attack ID (combat action table index). Differs from B45 for classes with unique rear attacks.');
        };
      } else if (activeSubview === 'promotion') {
        cols = [
          { label: 'ID', cls: 'col-sticky' }, { label: 'Name', cls: 'col-sticky-name' },
          { label: 'ReqLv', title: 'B54 required level' },
          { label: 'ReqClass', title: 'B55 required class' },
          { label: 'ReqClLv', title: 'B56 required class level' },
          { label: 'AddlReq', title: 'B57 additional requirement \u2014 uncertain', cls: 'col-raw' },
          { label: 'Element', title: 'B58 default damage element (CSV-verified: 0x00=Physical, 0x01=Wind, 0x02=Flame, 0x03=Earth, 0x04=Water, 0xFF=Random/None)' },
          { label: 'Category', title: 'B59 category/tier' },
          // Stat-gate promotion thresholds — live in LZSS block at z64 0x3A960C,
          // decompressed by OB64.parseStatGates. Read-only (edit would require
          // LZSS recompress + block splice on export, not yet implemented).
          { label: 'G-STR', title: 'Stat gate: STR threshold for promoting INTO this class (LZSS block)', cls: 'col-gate' },
          { label: 'G-VIT', title: 'Stat gate: VIT threshold', cls: 'col-gate' },
          { label: 'G-INT', title: 'Stat gate: INT threshold', cls: 'col-gate' },
          { label: 'G-MEN', title: 'Stat gate: MEN threshold', cls: 'col-gate' },
          { label: 'G-AGI', title: 'Stat gate: AGI threshold', cls: 'col-gate' },
          { label: 'G-DEX', title: 'Stat gate: DEX threshold', cls: 'col-gate' },
          { label: 'G-AlnMin', title: 'Stat gate: minimum alignment (inclusive)', cls: 'col-gate' },
          { label: 'G-AlnMax', title: 'Stat gate: maximum alignment (inclusive)', cls: 'col-gate' },
          { label: 'Promotes To' }, { label: 'Promotes From' }
        ];
        fillRow = function(cid, tr, def) {
          addNumericCell(tr, def, 'reqLevel', 255);
          addReqClassCell(tr, def);
          addNumericCell(tr, def, 'reqClassLevel', 255);
          addRawByteCell(tr, def, 'additionalReqRaw', 'B57 additional requirement \u2014 uncertain');
          addDropdownCell(tr, def, 'dragonElement', OB64.DEFAULT_ELEMENTS, OB64.defaultElementName);
          addDropdownCell(tr, def, 'category', OB64.CLASS_TIERS, OB64.classTierName);
          // Stat gates — class-id-indexed (NOT class_id+1 like class defs).
          // Missing entry = no stat gate defined for this class (promotion not
          // gated by stats). Dash-fill all 8 cells in that case.
          var gate = rom.statGates && rom.statGates.byClass[cid];
          var gateLabel = gate ? 'Stat gate: promote INTO ' + OB64.className(cid) : 'No stat gate defined for ' + OB64.className(cid);
          addStatGateCell(tr, gate, 'str', gateLabel);
          addStatGateCell(tr, gate, 'vit', gateLabel);
          addStatGateCell(tr, gate, 'int', gateLabel);
          addStatGateCell(tr, gate, 'men', gateLabel);
          addStatGateCell(tr, gate, 'agi', gateLabel);
          addStatGateCell(tr, gate, 'dex', gateLabel);
          addStatGateCell(tr, gate, 'alnMin', gateLabel);
          addStatGateCell(tr, gate, 'alnMax', gateLabel);
          var tdTo = document.createElement('td'); renderPromoLinks(tdTo, evoLookup.promotions[cid]); tr.appendChild(tdTo);
          var tdFrom = document.createElement('td'); renderPromoLinks(tdFrom, evoLookup.demotions[cid]); tr.appendChild(tdFrom);
        };
      } else { // unit
        cols = [
          { label: 'ID', cls: 'col-sticky' }, { label: 'Name', cls: 'col-sticky-name' },
          { label: 'UnitType', title: 'B64 humanoid (1) or beast (2)' },
          { label: 'SpriteType', title: 'B65 sprite/body type' },
          { label: 'CombatBehav', title: 'B66 combat behavior tier' },
          { label: 'Power', title: 'B69 power/stat rating' },
          { label: 'UnitCount', title: 'B70 formation size' },
          { label: 'B33', title: 'B33 padding', cls: 'col-raw' },
          { label: 'B67', title: 'B67 padding', cls: 'col-raw' },
          { label: 'B68', title: 'B68 sentinel', cls: 'col-raw' },
          { label: 'B71', title: 'B71 padding', cls: 'col-raw' }
        ];
        fillRow = function(cid, tr, def) {
          addDropdownCell(tr, def, 'unitType', OB64.UNIT_TYPES, OB64.unitTypeName);
          addDropdownCell(tr, def, 'spriteType', OB64.SPRITE_TYPES, OB64.spriteTypeName);
          addDropdownCell(tr, def, 'combatBehavior', OB64.COMBAT_BEHAVIORS, OB64.combatBehaviorName);
          addNumericCell(tr, def, 'powerRating', 255);
          addNumericCell(tr, def, 'unitCount', 255);
          addRawByteCell(tr, def, 'b33Raw', 'B33 padding');
          addRawByteCell(tr, def, 'b67Raw', 'B67 padding');
          addRawByteCell(tr, def, 'b68Raw', 'B68 sentinel');
          addRawByteCell(tr, def, 'b71Raw', 'B71 padding');
        };
      }

      var thead = document.createElement('thead');
      var hTr = document.createElement('tr');
      cols.forEach(function(col) {
        var th = document.createElement('th');
        if (col.cls) th.className = col.cls;
        if (col.title) th.title = col.title;
        th.textContent = col.label;
        hTr.appendChild(th);
      });
      thead.appendChild(hTr);
      table.appendChild(thead);

      var tbody = document.createElement('tbody');
      for (var ci = 0; ci < allClassIds.length; ci++) {
        var cid = allClassIds[ci];
        var defs = defMap[cid];
        var def = defs && defs.length > 0 ? defs[0] : null;
        var tr = document.createElement('tr');
        tr.id = 'class-' + cid;
        var tdId = td(tr, '0x' + cid.toString(16).padStart(2, '0'));
        tdId.className = 'col-sticky';
        var tdName = td(tr, OB64.className(cid));
        tdName.className = 'class-name col-sticky-name';
        fillRow(cid, tr, def);
        tbody.appendChild(tr);
      }
      table.appendChild(tbody);
      tableHost.appendChild(table);
      makeSortable(table);

      // Re-apply any active text filter after rebuild
      var filterInput = filter.querySelector('input');
      var q = filterInput ? filterInput.value.toLowerCase() : '';
      if (q) {
        var rows = tbody.querySelectorAll('tr');
        for (var i = 0; i < rows.length; i++) {
          rows[i].style.display = rows[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
        }
      }
    }

    renderSubview();
    panel.appendChild(tableContainer);

    // ---- CARD VIEW ----
    //
    // Every class-def byte is editable. Uncertain fields (B42/43/45/47/48/53/57
    // + padding bytes B33/67/68/71 + growth-pair bytes B3/7/11/15/19) render
    // with the .raw-byte class and a "caution" tooltip. Sections below the
    // three always-open blocks (Stats / Alignment / Resistances) are
    // collapsible <details> elements.

    // Collapsible-section factory.
    function makeSection(title, open) {
      var wrap = document.createElement('details');
      if (open) wrap.open = true;
      var summary = document.createElement('summary');
      summary.className = 'class-card-section-label';
      summary.textContent = title;
      wrap.appendChild(summary);
      return wrap;
    }

    // Editable numeric tile. opts: {raw: bool, title: string, max: number}
    function tileNumeric(d, field, label, opts) {
      opts = opts || {};
      var e = document.createElement('div');
      e.className = 'stat-entry editable' + (opts.raw ? ' raw-byte' : '');
      e.title = opts.title || (opts.raw ? 'Uncertain field \u2014 edit with caution' : '');
      var lbl = document.createElement('span');
      lbl.className = 'stat-label';
      lbl.textContent = label;
      e.appendChild(lbl);
      var val = document.createElement('span');
      val.className = 'stat-value';
      val.textContent = d[field];
      e.appendChild(val);
      e.addEventListener('click', function() {
        makeNumericInput(e, d[field], 0, opts.max || 255, function(nv) {
          d[field] = nv;
          e.textContent = '';
          e.appendChild(lbl);
          val.textContent = nv;
          e.appendChild(val);
          markChanged();
        });
      });
      return e;
    }

    // Editable stat-gate tile — writes to rom.statGates.byClass[cid][field]
    // and sets dirty.statGates directly (same pattern as the table's
    // addStatGateCell). Value 0 displays as em-dash.
    function tileStatGate(gate, field, label) {
      var e = document.createElement('div');
      e.className = 'stat-entry editable';
      var lbl = document.createElement('span');
      lbl.className = 'stat-label';
      lbl.textContent = label;
      e.appendChild(lbl);
      var val = document.createElement('span');
      val.className = 'stat-value';
      val.textContent = (gate[field] | 0) || '\u2014';
      e.appendChild(val);
      e.addEventListener('click', function() {
        makeNumericInput(e, gate[field] | 0, 0, 255, function(nv) {
          gate[field] = nv & 0xFF;
          e.textContent = '';
          e.appendChild(lbl);
          val.textContent = nv || '\u2014';
          e.appendChild(val);
          dirty.statGates = true;
          changes++;
          updateStatus();
        });
      });
      return e;
    }

    // Editable dropdown tile.
    function tileDropdown(d, field, label, optTable, nameFn) {
      var e = document.createElement('div');
      e.className = 'stat-entry editable';
      var lbl = document.createElement('span');
      lbl.className = 'stat-label';
      lbl.textContent = label;
      e.appendChild(lbl);
      var val = document.createElement('span');
      val.className = 'stat-value';
      val.textContent = nameFn(d[field]);
      e.appendChild(val);
      e.addEventListener('click', function() {
        makeDropdown(e, optTable, d[field], function(nv) {
          d[field] = nv;
          e.textContent = '';
          e.appendChild(lbl);
          val.textContent = nameFn(nv);
          e.appendChild(val);
          markChanged();
        });
      });
      return e;
    }

    for (var ci = 0; ci < allClassIds.length; ci++) {
      (function(cid) {
        var name = OB64.className(cid);
        var defs = defMap[cid];
        var def = defs && defs.length > 0 ? defs[0] : null;

        var card = document.createElement('div');
        card.className = 'class-card';

        // Header
        var cardHeader = document.createElement('div');
        cardHeader.className = 'class-card-header';
        var hName = document.createElement('span');
        hName.className = 'class-card-name';
        hName.textContent = name;
        var hId = document.createElement('span');
        hId.className = 'class-card-id';
        hId.textContent = '0x' + cid.toString(16).padStart(2, '0');
        cardHeader.appendChild(hName);
        cardHeader.appendChild(hId);
        card.appendChild(cardHeader);

        // Meta strip
        var catName = getCatName(cid);
        var evoEntry = evoLookup.byClass[cid];
        var tierLabel = evoEntry ? (OB64.EVOLUTION_TIERS[evoEntry.category] || 'Tier ' + evoEntry.category) : '';
        var metaParts = [];
        if (catName) metaParts.push(catName);
        if (tierLabel) metaParts.push(tierLabel);
        if (def) {
          metaParts.push(OB64.moveTypeName(def.moveType));
          metaParts.push(OB64.unitTypeName(def.unitType));
          if (def.dragonElement !== 0xFF) metaParts.push(OB64.dragonElementName(def.dragonElement));
        }
        if (metaParts.length > 0) {
          var meta = document.createElement('div');
          meta.className = 'class-card-meta';
          meta.textContent = metaParts.join(' \u2022 ');
          card.appendChild(meta);
        }

        if (def) {
          // --- Base Stats (always visible) — 6 stat tiles with base + growth badge, plus LCK
          var statsWrap = document.createElement('div');
          statsWrap.className = 'class-card-stats';
          var sLabel = document.createElement('div');
          sLabel.className = 'class-card-section-label';
          sLabel.textContent = 'Base Stats';
          statsWrap.appendChild(sLabel);
          var statsGrid = document.createElement('div');
          statsGrid.className = 'stats-grid';
          var STAT_G = ['strGrowth', 'vitGrowth', 'intGrowth', 'menGrowth', 'agiGrowth', 'dexGrowth'];
          if (def.stats.length === 6) {
            for (var s = 0; s < 6; s++) {
              (function(statIdx) {
                var entry = document.createElement('div');
                entry.className = 'stat-entry';
                var sName = document.createElement('span');
                sName.className = 'stat-label ' + CLASS_STAT_CSS[statIdx];
                sName.textContent = CLASS_STAT_NAMES[statIdx];
                entry.appendChild(sName);
                // Base (click to edit)
                var sVal = document.createElement('span');
                sVal.className = 'stat-value editable';
                sVal.textContent = def.stats[statIdx].base;
                sVal.title = 'Base stat (u16 BE)';
                sVal.addEventListener('click', function(ev) {
                  ev.stopPropagation();
                  makeNumericInput(sVal, def.stats[statIdx].base, 0, 65535, function(nv) {
                    def.stats[statIdx].base = nv;
                    sVal.textContent = nv;
                    markChanged();
                  });
                });
                entry.appendChild(sVal);
                // Growth sub-badge (click to edit)
                var gVal = document.createElement('span');
                gVal.className = 'growth-sub editable';
                gVal.textContent = '+' + def[STAT_G[statIdx]];
                gVal.title = 'Growth mean (B' + (2 + statIdx * 4) + ') \u2014 level-up gain per turn';
                gVal.addEventListener('click', function(ev) {
                  ev.stopPropagation();
                  makeNumericInput(gVal, def[STAT_G[statIdx]], 0, 255, function(nv) {
                    def[STAT_G[statIdx]] = nv;
                    def.stats[statIdx].g1 = nv; // keep legacy shape in sync for serializer
                    gVal.textContent = '+' + nv;
                    markChanged();
                  });
                });
                entry.appendChild(gVal);
                statsGrid.appendChild(entry);
              })(s);
            }
          }
          // LCK tile (u8, no growth pair)
          (function() {
            var entry = document.createElement('div');
            entry.className = 'stat-entry editable';
            var lckName = document.createElement('span');
            lckName.className = 'stat-label';
            lckName.textContent = 'LCK';
            entry.appendChild(lckName);
            var lckVal = document.createElement('span');
            lckVal.className = 'stat-value';
            lckVal.textContent = def.lck;
            lckVal.title = 'B23 LCK base (40-60 typical)';
            entry.appendChild(lckVal);
            entry.addEventListener('click', function() {
              makeNumericInput(entry, def.lck, 0, 255, function(nv) {
                def.lck = nv;
                entry.textContent = '';
                entry.appendChild(lckName);
                lckVal.textContent = nv;
                entry.appendChild(lckVal);
                markChanged();
              });
            });
            statsGrid.appendChild(entry);
          })();
          statsWrap.appendChild(statsGrid);
          card.appendChild(statsWrap);

          // --- Alignment (always visible)
          var alignDiv = document.createElement('div');
          alignDiv.className = 'class-card-alignment';
          var alLabel = document.createElement('div');
          alLabel.className = 'class-card-section-label';
          alLabel.textContent = 'Alignment';
          alignDiv.appendChild(alLabel);
          (function() {
            var alVal = document.createElement('span');
            alVal.className = 'alignment-value editable';
            var al = def.alignment;
            alVal.textContent = alnLabel(al);
            alVal.classList.add(al >= 55 ? 'align-law' : al <= 45 ? 'align-chaos' : 'align-neutral');
            alVal.addEventListener('click', function() {
              makeNumericInput(alVal, def.alignment, 0, 100, function(nv) {
                def.alignment = nv;
                alVal.textContent = alnLabel(nv);
                alVal.classList.remove('align-law', 'align-chaos', 'align-neutral');
                alVal.classList.add(nv >= 55 ? 'align-law' : nv <= 45 ? 'align-chaos' : 'align-neutral');
                markChanged();
              });
            });
            alignDiv.appendChild(alVal);
          })();
          card.appendChild(alignDiv);

          // --- Resistances (always visible)
          if (def.resistances.length === 7) {
            var resDiv = document.createElement('div');
            resDiv.className = 'class-card-resistances';
            var rLabel = document.createElement('div');
            rLabel.className = 'class-card-section-label';
            rLabel.textContent = 'Resistances';
            resDiv.appendChild(rLabel);
            var resGrid = document.createElement('div');
            resGrid.className = 'resist-grid';
            for (var ri = 0; ri < 7; ri++) {
              (function(resIdx) {
                var rv = def.resistances[resIdx];
                var entry = document.createElement('div');
                entry.className = 'resist-entry editable';
                var rName = document.createElement('span');
                rName.className = 'resist-label resist-' + OB64.RESISTANCE_NAMES[resIdx].toLowerCase();
                rName.textContent = OB64.RESISTANCE_NAMES[resIdx];
                entry.appendChild(rName);
                var rVal = document.createElement('span');
                rVal.className = 'resist-value';
                rVal.textContent = rv;
                if (rv < 50) rVal.classList.add('resist-strong');
                else if (rv > 50) rVal.classList.add('resist-weak');
                entry.appendChild(rVal);
                entry.addEventListener('click', function() {
                  makeNumericInput(entry, def.resistances[resIdx], 0, 255, function(nv) {
                    def.resistances[resIdx] = nv;
                    entry.textContent = '';
                    entry.appendChild(rName);
                    rVal.textContent = nv;
                    rVal.className = 'resist-value';
                    if (nv < 50) rVal.classList.add('resist-strong');
                    else if (nv > 50) rVal.classList.add('resist-weak');
                    entry.appendChild(rVal);
                    markChanged();
                  });
                });
                resGrid.appendChild(entry);
              })(ri);
            }
            resDiv.appendChild(resGrid);
            card.appendChild(resDiv);
          }

          // --- Equipment Defaults (collapsible, open) — 4 searchable dropdowns
          var eqSec = makeSection('Equipment Defaults', true);
          var eqGrid = document.createElement('div');
          eqGrid.className = 'stats-grid';
          for (var de = 0; de < 4; de++) {
            (function(displayCol) {
              var slotIdx = COL_TO_SLOT[displayCol];
              var entry = document.createElement('div');
              entry.className = 'stat-entry editable';
              var lbl = document.createElement('span');
              lbl.className = 'stat-label';
              lbl.textContent = EQUIP_LABELS[displayCol];
              entry.appendChild(lbl);
              var vs = document.createElement('span');
              vs.className = 'stat-value';
              var iid = def.defaultEquip[slotIdx];
              vs.textContent = iid > 0 ? OB64.itemName(iid) : 'None';
              entry.appendChild(vs);
              entry.addEventListener('click', function() {
                var opts = getSlotOptions(slotIdx, def);
                openItemPickerFromDict({
                  title: 'Default ' + EQUIP_LABELS[displayCol] + ' \u2014 ' + (def.name || ''),
                  options: opts, currentId: def.defaultEquip[slotIdx],
                  onSelect: function(nv) {
                    def.defaultEquip[slotIdx] = nv;
                    vs.textContent = nv > 0 ? OB64.itemName(nv) : 'None';
                    entry.classList.add('modified');
                  }
                });
              });
              eqGrid.appendChild(entry);
            })(de);
          }
          eqSec.appendChild(eqGrid);
          card.appendChild(eqSec);

          // --- Combat (collapsible, open) — row attacks + mults + raw bytes
          var combatSec = makeSection('Combat', true);
          var combatGrid = document.createElement('div');
          combatGrid.className = 'stats-grid';
          combatGrid.appendChild(tileNumeric(def, 'frontAtks', 'FrontAtks',
            {title: 'B44 front row attack count (verified in-game 2026-04-17)'}));
          combatGrid.appendChild(tileNumeric(def, 'midAtks', 'MidAtks',
            {title: 'B46 middle row attack count (verified in-game 2026-04-17)'}));
          combatGrid.appendChild(tileNumeric(def, 'rearAtks', 'RearAtks',
            {title: 'B48 rear row attack count. Decoded via CSV cross-check (79/79 match) \u2014 previously mislabeled "atkType".'}));
          combatGrid.appendChild(tileNumeric(def, 'physAtk', 'PAtk', {title: 'B49'}));
          combatGrid.appendChild(tileNumeric(def, 'magAtk', 'MAtk', {title: 'B50'}));
          combatGrid.appendChild(tileNumeric(def, 'physDef', 'PDef', {title: 'B51'}));
          combatGrid.appendChild(tileNumeric(def, 'magDef', 'MDef', {title: 'B52'}));
          combatGrid.appendChild(tileNumeric(def, 'flagsRaw', 'Flags',
            {raw: true, title: 'B53 combat flags \u2014 not decoded, edit with caution'}));
          combatGrid.appendChild(tileNumeric(def, 'b42Raw', 'FixEq Mask',
            {raw: true, title: 'B42 \u2014 fixed-equip-slots bitmask (identified via CSV "Fixed Equips"). 0x01=Wpn, 0x02=Offhand, 0x04=Body, 0x08=Head. Lycanthrope=0x0F (all fixed), Soldier=0x03 (Wpn+Off).'}));
          combatGrid.appendChild(tileNumeric(def, 'b43Raw', 'B43',
            {raw: true, title: 'B43 \u2014 unknown, often matches B45 (front attack ID)'}));
          combatGrid.appendChild(tileNumeric(def, 'b45Raw', 'Front AtkID',
            {raw: true, title: 'B45 \u2014 front-row attack ID (combat action table index). e.g. Thrust=1, Slash=4, Strike=9, [Elem. Magic]=45'}));
          combatGrid.appendChild(tileNumeric(def, 'b47Raw', 'Rear AtkID',
            {raw: true, title: 'B47 \u2014 rear-row attack ID. May differ from front (e.g. Valkyrie front=Cleave(5), rear=Lightning(51)). Mid-row reuses B45.'}));
          combatSec.appendChild(combatGrid);
          card.appendChild(combatSec);

          // --- Promotion (collapsible)
          var promoSec = makeSection('Promotion', false);
          var promoGrid = document.createElement('div');
          promoGrid.className = 'stats-grid';
          promoGrid.appendChild(tileNumeric(def, 'reqLevel', 'ReqLevel', {title: 'B54'}));
          // ReqClass uses a class-searchable dropdown
          (function() {
            var entry = document.createElement('div');
            entry.className = 'stat-entry editable';
            var lbl = document.createElement('span');
            lbl.className = 'stat-label';
            lbl.textContent = 'ReqClass';
            entry.appendChild(lbl);
            var vs = document.createElement('span');
            vs.className = 'stat-value';
            vs.textContent = def.reqClass > 0 ? OB64.className(def.reqClass) : 'None';
            entry.appendChild(vs);
            entry.addEventListener('click', function() {
              var classOpts = { 0: 'None' };
              for (var k in OB64.CLASS_NAMES) classOpts[k] = OB64.CLASS_NAMES[k];
              makeSearchableInput(entry, classOpts, def.reqClass, function(nv) {
                def.reqClass = nv;
                entry.textContent = '';
                entry.appendChild(lbl);
                vs.textContent = nv > 0 ? OB64.className(nv) : 'None';
                entry.appendChild(vs);
                markChanged();
              });
            });
            promoGrid.appendChild(entry);
          })();
          promoGrid.appendChild(tileNumeric(def, 'reqClassLevel', 'ReqClassLv', {title: 'B56'}));
          promoGrid.appendChild(tileNumeric(def, 'additionalReqRaw', 'AddlReq',
            {raw: true, title: 'B57 additional requirement \u2014 uncertain (0x5A/0x5B rare)'}));
          promoSec.appendChild(promoGrid);

          // Stat-gate thresholds: LZSS-compressed block at z64 0x3A960C,
          // class-id-indexed (8 bytes per class). Edits set dirty.statGates
          // directly; on export the block is recompressed and spliced back.
          // Only shown when the class has a gate entry.
          var gate = rom.statGates && rom.statGates.byClass[cid];
          if (gate) {
            var gateHdr = document.createElement('div');
            gateHdr.className = 'class-card-section-label class-card-gate-label';
            gateHdr.textContent = 'Stat Gate (Promotion Thresholds)';
            gateHdr.title = 'LZSS block at z64 0x3A960C. Editing recompresses the block on export.';
            promoSec.appendChild(gateHdr);
            var gateGrid = document.createElement('div');
            gateGrid.className = 'stats-grid';
            var gateFields = [
              ['STR \u2265', 'str'],
              ['VIT \u2265', 'vit'],
              ['INT \u2265', 'int'],
              ['MEN \u2265', 'men'],
              ['AGI \u2265', 'agi'],
              ['DEX \u2265', 'dex'],
              ['Aln \u2265', 'alnMin'],
              ['Aln \u2264', 'alnMax']
            ];
            gateFields.forEach(function(gf) {
              gateGrid.appendChild(tileStatGate(gate, gf[1], gf[0]));
            });
            promoSec.appendChild(gateGrid);
          }

          card.appendChild(promoSec);

          // --- Classification (collapsible)
          var clsSec = makeSection('Classification', false);
          var clsGrid = document.createElement('div');
          clsGrid.className = 'stats-grid';
          clsGrid.appendChild(tileDropdown(def, 'dragonElement', 'Element', OB64.DEFAULT_ELEMENTS, OB64.defaultElementName));
          clsGrid.appendChild(tileDropdown(def, 'category', 'Category', OB64.CLASS_TIERS, OB64.classTierName));
          clsGrid.appendChild(tileDropdown(def, 'unitType', 'UnitType', OB64.UNIT_TYPES, OB64.unitTypeName));
          clsGrid.appendChild(tileDropdown(def, 'spriteType', 'SpriteType', OB64.SPRITE_TYPES, OB64.spriteTypeName));
          clsGrid.appendChild(tileDropdown(def, 'combatBehavior', 'Behavior', OB64.COMBAT_BEHAVIORS, OB64.combatBehaviorName));
          clsGrid.appendChild(tileDropdown(def, 'moveType', 'Move', OB64.MOVEMENT_TYPES, OB64.moveTypeName));
          clsGrid.appendChild(tileNumeric(def, 'powerRating', 'Power', {title: 'B69'}));
          clsGrid.appendChild(tileNumeric(def, 'unitCount', 'UnitCount', {title: 'B70'}));
          clsSec.appendChild(clsGrid);
          card.appendChild(clsSec);

          // --- Raw / Padding (collapsed by default) — padding + growth-pair bytes
          var rawSec = makeSection('Raw / Padding', false);
          var rawGrid = document.createElement('div');
          rawGrid.className = 'stats-grid';
          rawGrid.appendChild(tileNumeric(def, 'b33Raw', 'B33', {raw: true, title: 'B33 padding'}));
          rawGrid.appendChild(tileNumeric(def, 'b67Raw', 'B67', {raw: true, title: 'B67 padding'}));
          rawGrid.appendChild(tileNumeric(def, 'b68Raw', 'B68', {raw: true, title: 'B68 sentinel'}));
          rawGrid.appendChild(tileNumeric(def, 'b71Raw', 'B71', {raw: true, title: 'B71 padding'}));
          rawGrid.appendChild(tileNumeric(def, 'b3Raw', 'B3', {raw: true, title: 'Pair byte after STR growth'}));
          rawGrid.appendChild(tileNumeric(def, 'b7Raw', 'B7', {raw: true, title: 'Pair byte after VIT growth'}));
          rawGrid.appendChild(tileNumeric(def, 'b11Raw', 'B11', {raw: true, title: 'Pair byte after INT growth'}));
          rawGrid.appendChild(tileNumeric(def, 'b15Raw', 'B15', {raw: true, title: 'Pair byte after MEN growth'}));
          rawGrid.appendChild(tileNumeric(def, 'b19Raw', 'B19', {raw: true, title: 'Pair byte after AGI growth'}));
          rawSec.appendChild(rawGrid);
          card.appendChild(rawSec);
        }

        // Evolution xref links (kept outside the `if (def)` branch)
        var promoIds = evoLookup.promotions[cid];
        var fromIds = evoLookup.demotions[cid];
        if ((promoIds && promoIds.length > 0) || (fromIds && fromIds.length > 0)) {
          var evoDiv = document.createElement('div');
          evoDiv.className = 'class-card-evo';
          var evLabel = document.createElement('div');
          evLabel.className = 'class-card-section-label';
          evLabel.textContent = 'Evolution';
          evoDiv.appendChild(evLabel);
          if (fromIds && fromIds.length > 0) {
            var fromLine = document.createElement('div');
            fromLine.innerHTML = '<span class="evo-arrow">\u2190</span> From: ';
            renderPromoLinks(fromLine, fromIds);
            evoDiv.appendChild(fromLine);
          }
          if (promoIds && promoIds.length > 0) {
            var toLine = document.createElement('div');
            toLine.innerHTML = '<span class="evo-arrow">\u2192</span> To: ';
            renderPromoLinks(toLine, promoIds);
            evoDiv.appendChild(toLine);
          }
          card.appendChild(evoDiv);
        }

        cardsContainer.appendChild(card);
      })(allClassIds[ci]);
    }

    panel.appendChild(cardsContainer);
  }

  function jumpToClass(e) {
    var cid = parseInt(e.target.dataset.classId);
    // If already on classes tab, just scroll
    if (activeTab === 'classes') {
      var row = document.getElementById('class-' + cid);
      if (row) {
        row.scrollIntoView({ behavior: 'smooth', block: 'center' });
        row.style.outline = '2px solid #e94560';
        setTimeout(function() { row.style.outline = ''; }, 2000);
      }
      return;
    }
    // Switch to classes tab
    activeTab = 'classes';
    var buttons = tabBar.querySelectorAll('button');
    for (var i = 0; i < buttons.length; i++) {
      buttons[i].classList.toggle('active', buttons[i].dataset.tab === 'classes');
    }
    var panels = document.querySelectorAll('.tab-panel');
    for (var i = 0; i < panels.length; i++) {
      panels[i].classList.toggle('active', panels[i].id === 'panel-classes');
    }
    renderClasses(document.getElementById('panel-classes'));
    var row = document.getElementById('class-' + cid);
    if (row) {
      row.scrollIntoView({ behavior: 'smooth', block: 'center' });
      row.style.outline = '2px solid #e94560';
      setTimeout(function() { row.style.outline = ''; }, 2000);
    }
  }

  // ============================================================
  // ENCOUNTERS TAB — neutral-encounter pool + creature drop table.
  // One card per scenario slice (skipping empty slices). Each card shows
  // 10 terrain slots with creature chips and inline drop lists. Drops
  // are class-keyed so edits propagate across every scenario using
  // that creature. See docs/neutral-encounters.md for structure.
  // ============================================================
  function renderEncounters(panel) {
    panel.innerHTML = '';

    // Shared category helpers for the class-picker drop-availability badge.
    // Story / NPC / buggy classes (0x51+) get a stronger warning since
    // they likely won't render as a tactical-map wild encounter.
    function dropStatus(classId) {
      if (!classId) return 'empty';
      var hasDrops = rom.creatureDrops.byClass[classId] && !rom.creatureDrops.byClass[classId].isSentinel;
      if (classId >= 0x51) return hasDrops ? 'story-drops' : 'story';
      return hasDrops ? 'ok' : 'no-drops';
    }
    function dropStatusLabel(status) {
      switch(status) {
        case 'ok':          return 'Drops OK';
        case 'no-drops':    return 'No drops';
        case 'story':       return 'Story/NPC class — untested as wild';
        case 'story-drops': return 'Story class with drops';
        default:            return '';
      }
    }

    // Filter bar — filters cards by scenario name / creature name / row
    var filter = makeFilterBar('Filter by scenario, creature, or row...', function(q) {
      var cards = panel.querySelectorAll('.encounter-card');
      for (var i = 0; i < cards.length; i++) {
        cards[i].style.display = cards[i].textContent.toLowerCase().indexOf(q) >= 0 ? '' : 'none';
      }
    });
    panel.appendChild(filter);

    // Heads-up note about shared drops / row structure
    var note = document.createElement('div');
    note.className = 'encounter-help';
    note.innerHTML =
      '<strong>Neutral encounters</strong> — wild creatures that spawn while walking a tactical map. ' +
      'Each card is one scenario slice (20 B, 10 terrain slots) keyed by the dispatcher\u2019s <code>$s0</code> index. ' +
      'Slot\u2192terrain mapping is globally consistent, and all 39 non-empty slices are named. ' +
      '<em>Drops are class-keyed</em>: editing Wyrm\u2019s drops in any card changes them for every scenario using Wyrm. ' +
      'Classes without a drop-table entry still spawn but yield no loot.';
    panel.appendChild(note);

    // Build the "vanilla encounter pool" class list once — the set of class
    // IDs that appear in ANY slot of any scenario in the current ROM. This is
    // the "Vanilla" tab in the picker modal. Re-computed on each render so it
    // reflects edits the user has made within the session.
    function renderGlobalRatePanel() {
      var globalRate = rom.neutralEncounters && rom.neutralEncounters.globalRate;
      if (!globalRate) return null;

      var SOFT_MAX_BP = 1000;   // 10.00%: already extremely frequent in-game.
      var HARD_MAX_BP = 10000;  // 100.00%: useful for diagnostics.
      var startBp = Math.max(0, Math.min(HARD_MAX_BP, Math.round(globalRate.basisPoints || 0)));
      var maxBp = startBp > SOFT_MAX_BP ? HARD_MAX_BP : SOFT_MAX_BP;

      function pct(bp) {
        return (bp / 100).toFixed(2) + '%';
      }
      function exactChance(threshold, divisor) {
        if (!divisor || threshold == null || threshold < 0) return 'unknown';
        return (threshold + 1) + ' / ' + divisor + ' = ' + (((threshold + 1) * 100) / divisor).toFixed(4) + '%';
      }
      function describeMode() {
        if (globalRate.mode === 'never') return 'Current ROM: globally disabled by branch patch.';
        if (globalRate.mode === 'always') return 'Current ROM: global roll always passes by branch patch.';
        if (globalRate.mode === 'threshold') {
          var normal = exactChance(globalRate.normalThreshold, globalRate.divisor);
          var alt = exactChance(globalRate.alternateThreshold, globalRate.divisor);
          if (globalRate.normalBasisPoints !== globalRate.alternateBasisPoints) {
            return 'Current normal path: ' + normal + '. Alternate branch: ' + alt + '. Editing writes both branches to the selected rate.';
          }
          return 'Current ROM: ' + normal + '. Editing writes both state-bit branches to the selected rate.';
        }
        return 'Current ROM pattern is not recognized. Editing will overwrite the known global-roll sites with the standard slider patch.';
      }
      function sync(bp, numberInput, rangeInput, valueEl, thresholdEl) {
        if (!isFinite(bp)) bp = 0;
        bp = Math.max(0, Math.min(maxBp, Math.round(bp)));
        globalRate.basisPoints = bp;
        numberInput.value = (bp / 100).toFixed(2);
        rangeInput.value = String(bp);
        valueEl.textContent = pct(bp);
        thresholdEl.textContent = bp === 0 ? 'always fail' : ((bp - 1) + ' / 10000');
      }
      function commit(bp, numberInput, rangeInput, valueEl, thresholdEl) {
        sync(bp, numberInput, rangeInput, valueEl, thresholdEl);
        globalRate.modified = true;
        dirty.encounters = true;
        markChanged();
      }

      var wrap = document.createElement('div');
      wrap.className = 'global-rate-panel';

      var head = document.createElement('div');
      head.className = 'terrain-rate-head';
      var title = document.createElement('div');
      title.className = 'terrain-rate-title';
      title.textContent = 'Global encounter roll';
      head.appendChild(title);
      var meta = document.createElement('div');
      meta.className = 'terrain-rate-meta';
      meta.textContent = 'ROM 0x13C1E8 / 0x13C1FC / 0x13C200';
      head.appendChild(meta);
      wrap.appendChild(head);

      var help = document.createElement('div');
      help.className = 'terrain-rate-help';
      help.textContent = 'This is the first neutral-encounter gate before unit selection and terrain rates. Use it as the main frequency knob: terrain rates mostly shape where encounters can happen after this global roll passes.';
      wrap.appendChild(help);

      var current = document.createElement('div');
      current.className = 'global-rate-current';
      current.textContent = describeMode();
      wrap.appendChild(current);

      var controls = document.createElement('div');
      controls.className = 'global-rate-controls';

      var label = document.createElement('div');
      label.className = 'terrain-rate-label';
      label.textContent = 'Pass rate';
      controls.appendChild(label);

      var range = document.createElement('input');
      range.className = 'global-rate-range';
      range.type = 'range';
      range.min = '0';
      range.max = String(maxBp);
      range.step = '1';
      range.value = String(startBp);
      controls.appendChild(range);

      var number = document.createElement('input');
      number.className = 'global-rate-number';
      number.type = 'number';
      number.min = '0';
      number.max = (maxBp / 100).toFixed(2);
      number.step = '0.01';
      number.value = (startBp / 100).toFixed(2);
      controls.appendChild(number);

      var value = document.createElement('div');
      value.className = 'terrain-rate-value';
      value.textContent = pct(startBp);
      controls.appendChild(value);

      var threshold = document.createElement('div');
      threshold.className = 'global-rate-threshold';
      threshold.textContent = startBp === 0 ? 'always fail' : ((startBp - 1) + ' / 10000');
      controls.appendChild(threshold);

      range.addEventListener('input', function() {
        sync(parseInt(range.value, 10), number, range, value, threshold);
      });
      range.addEventListener('change', function() {
        commit(parseInt(range.value, 10), number, range, value, threshold);
      });
      number.addEventListener('change', function() {
        commit(parseFloat(number.value) * 100, number, range, value, threshold);
      });

      wrap.appendChild(controls);

      var unlockWrap = document.createElement('label');
      unlockWrap.className = 'global-rate-unlock';
      var unlock = document.createElement('input');
      unlock.type = 'checkbox';
      unlock.checked = maxBp === HARD_MAX_BP;
      unlockWrap.appendChild(unlock);
      var unlockText = document.createElement('span');
      unlockText.innerHTML =
        '<strong>Unlock extreme rates above 10%</strong> for testing. ' +
        '10% global can already feel close to every step because the game rolls per eligible check, not per visible map step.';
      unlockWrap.appendChild(unlockText);
      unlock.addEventListener('change', function() {
        maxBp = unlock.checked ? HARD_MAX_BP : SOFT_MAX_BP;
        range.max = String(maxBp);
        number.max = (maxBp / 100).toFixed(2);
        if ((globalRate.basisPoints || 0) > maxBp) {
          commit(maxBp, number, range, value, threshold);
        } else {
          sync(globalRate.basisPoints || 0, number, range, value, threshold);
        }
      });
      wrap.appendChild(unlockWrap);

      var note = document.createElement('div');
      note.className = 'terrain-rate-global-note';
      note.innerHTML =
        '<strong>Export behavior:</strong> once edited, this slider patches the divisor to <code>10000</code> and writes both state-bit branches to the same selected basis-point rate. ' +
        '<code>0%</code> uses an always-fail branch; the normal slider is capped at <code>10%</code> because that is already extremely frequent in live play. ' +
        'Terrain rates still apply after this gate. Example: <code>50%</code> global with <code>50%</code> terrain is roughly a <code>25%</code> chance per eligible check, or about <code>4</code> eligible checks on average.';
      wrap.appendChild(note);

      return wrap;
    }

    function renderTerrainRatePanel() {
      var terrainRates = rom.neutralEncounters && rom.neutralEncounters.terrainRates;
      var entries = terrainRates && terrainRates.entries ? terrainRates.entries : [];
      if (!entries.length) return null;

      var wrap = document.createElement('div');
      wrap.className = 'terrain-rate-panel';

      var head = document.createElement('div');
      head.className = 'terrain-rate-head';
      var title = document.createElement('div');
      title.className = 'terrain-rate-title';
      title.textContent = 'Terrain encounter rates';
      head.appendChild(title);
      var meta = document.createElement('div');
      meta.className = 'terrain-rate-meta';
      meta.textContent = 'ROM 0x141E80 -> RAM 0x801ED740';
      head.appendChild(meta);
      wrap.appendChild(head);

      var help = document.createElement('div');
      help.className = 'terrain-rate-help';
      help.textContent = 'These are the terrain-byte thresholds for the per-unit rand % 100 check after the global encounter roll. 0 disables that terrain byte; 100 guarantees this local check. Decoy Cap doubles the active terrain rate.';
      wrap.appendChild(help);

      var globalNote = document.createElement('div');
      globalNote.className = 'terrain-rate-global-note';
      globalNote.innerHTML =
        '<strong>Important:</strong> 100% terrain rate does not mean an encounter every step. ' +
        'The game first passes a global/base roll, then applies this terrain roll. ' +
        'Vanilla normal play passes <code>51 / 72000</code> attempts before terrain is checked; ' +
        'use the Global encounter roll panel above to change that first gate. ' +
        'Terrain rates are best treated as terrain weighting/filter knobs, while the global roll controls overall pacing.';
      wrap.appendChild(globalNote);

      var grid = document.createElement('div');
      grid.className = 'terrain-rate-grid';

      function fmtByte(n) {
        return '0x' + n.toString(16).toUpperCase().padStart(2, '0');
      }

      function commitRate(entry, next, numberInput, rangeInput, valueEl) {
        var nv = parseInt(next, 10);
        if (!isFinite(nv)) nv = 0;
        if (nv < 0) nv = 0;
        if (nv > 100) nv = 100;
        entry.rate = nv;
        numberInput.value = String(nv);
        rangeInput.value = String(nv);
        valueEl.textContent = nv + '%';
        dirty.encounters = true;
        markChanged();
      }

      for (var i = 0; i < entries.length; i++) {
        var entry = entries[i];
        if (!entry.enabled) continue;
        (function(entry) {
          var row = document.createElement('div');
          row.className = 'terrain-rate-row terrain-rate-slot-' + entry.encounterSlot;

          var label = document.createElement('div');
          label.className = 'terrain-rate-label';
          label.textContent = entry.terrainName;
          row.appendChild(label);

          var detail = document.createElement('div');
          detail.className = 'terrain-rate-detail';
          detail.textContent = fmtByte(entry.terrainByte) + ' -> slot ' + entry.encounterSlot + ' (lookup ' + entry.rawLookup + ')';
          row.appendChild(detail);

          var range = document.createElement('input');
          range.className = 'terrain-rate-range';
          range.type = 'range';
          range.min = '0';
          range.max = '100';
          range.step = '1';
          range.value = String(entry.rate);
          row.appendChild(range);

          var number = document.createElement('input');
          number.className = 'terrain-rate-number';
          number.type = 'number';
          number.min = '0';
          number.max = '100';
          number.step = '1';
          number.value = String(entry.rate);
          row.appendChild(number);

          var value = document.createElement('div');
          value.className = 'terrain-rate-value';
          value.textContent = entry.rate + '%';
          row.appendChild(value);

          range.addEventListener('input', function() {
            number.value = range.value;
            value.textContent = range.value + '%';
          });
          range.addEventListener('change', function() {
            commitRate(entry, range.value, number, range, value);
          });
          number.addEventListener('change', function() {
            commitRate(entry, number.value, number, range, value);
          });

          grid.appendChild(row);
        })(entry);
      }

      wrap.appendChild(grid);
      return wrap;
    }

    var globalRatePanel = renderGlobalRatePanel();
    if (globalRatePanel) panel.appendChild(globalRatePanel);

    var ratePanel = renderTerrainRatePanel();
    if (ratePanel) panel.appendChild(ratePanel);

    function computeVanillaClasses() {
      var set = {};
      var records = rom.neutralEncounters.records;
      for (var i = 0; i < records.length; i++) {
        var r = records[i];
        for (var s = 0; s < r.slots.length; s++) {
          if (r.slots[s].classA) set[r.slots[s].classA] = true;
          if (r.slots[s].classB) set[r.slots[s].classB] = true;
        }
      }
      return Object.keys(set).map(function(k) { return parseInt(k); }).sort(function(a, b) { return a - b; });
    }
    var vanillaClasses = computeVanillaClasses();

    // Per-slot rendering helper.
    //   `linked` = true when classA === classB; picks commit to both fields.
    //   `which`  = 'classA' | 'classB' when linked=false; commits to just that side.
    function renderCreatureSlot(card, rec, slot, which, linked) {
      var chip = document.createElement('div');
      var classId = slot[which];
      chip.className = 'creature-chip';

      // Shared commit callback used by both the empty-slot and populated
      // branches. Updates the right side(s) of the slot and rebuilds.
      function commit(newClass) {
        if (newClass === undefined) return; // cancel
        if (linked) {
          slot.classA = newClass;
          slot.classB = newClass;
        } else {
          slot[which] = newClass;
        }
        dirty.encounters = true;
        markChanged();
        rebuildCardBody(card, rec);
      }

      if (!classId) {
        chip.classList.add('empty-slot');
        chip.textContent = '+';
        chip.title = 'Click to add a creature for ' + slot.terrainName;
        chip.addEventListener('click', function() {
          openClassPickerModal(0, function(newClass) {
            if (!newClass) return;
            // For empty single-creature case we also mirror the other side.
            slot.classA = newClass;
            slot.classB = newClass;
            dirty.encounters = true;
            markChanged();
            rebuildCardBody(card, rec);
          });
        });
        return chip;
      }

      var status = dropStatus(classId);
      chip.classList.add('drop-' + status);

      var name = document.createElement('span');
      name.className = 'creature-name';
      name.textContent = OB64.className(classId);
      chip.appendChild(name);

      var dot = document.createElement('span');
      dot.className = 'drop-dot drop-dot-' + status;
      dot.title = dropStatusLabel(status);
      chip.appendChild(dot);

      chip.title = 'Class 0x' + classId.toString(16).padStart(2, '0') + ' \u2014 click to change (or remove)';
      chip.addEventListener('click', function(e) {
        if (e.target !== chip && e.target !== name && e.target !== dot) return;
        openClassPickerModal(classId, commit);
      });

      // Inline drops strip below creature name
      var dropsRow = document.createElement('div');
      dropsRow.className = 'drops-row';
      var dropsRec = rom.creatureDrops.byClass[classId];
      if (dropsRec) {
        for (var s = 0; s < 3; s++) {
          (function(slotIdx) {
            var dropSlot = dropsRec.slots[slotIdx];
            var dchip = document.createElement('span');
            dchip.className = 'drop-chip' + (dropSlot.isEquipment ? ' drop-chip-eq' : ' drop-chip-exp');
            dchip.textContent = dropSlot.itemId ? OB64.itemName(dropSlot.itemId) : '\u2014';
            dchip.title = (dropSlot.isEquipment ? 'Equipment' : 'Expendable')
              + ' \u2014 item 0x' + dropSlot.itemId.toString(16).padStart(4, '0')
              + ' (affects all scenarios using ' + OB64.className(classId) + ')';
            dchip.addEventListener('click', function(e) {
              e.stopPropagation();
              openItemPicker(dchip, dropSlot.itemId, function(newItemId, kind) {
                if (newItemId === undefined) return;
                dropSlot.itemId = newItemId;
                // Derive equipment flag. Prefer the kind the user picked from
                // the modal (disambiguates overlapping IDs between the
                // equipment and consumable namespaces). Fall back to the
                // item-stat equipType lookup for legacy paths.
                if (kind === 'consumable') {
                  dropSlot.isEquipment = false;
                } else if (kind === 'equip') {
                  dropSlot.isEquipment = true;
                } else if (kind === 'none' || newItemId === 0) {
                  dropSlot.isEquipment = false;
                } else {
                  var stat = rom.itemStats[newItemId];
                  dropSlot.isEquipment = !!(stat && stat.equipType && stat.equipType > 0);
                }
                dropSlot.raw = (dropSlot.itemId & 0x7FFF) | (dropSlot.isEquipment ? 0x8000 : 0);
                dirty.creatureDrops = true;
                markChanged();
                dchip.className = 'drop-chip' + (dropSlot.isEquipment ? ' drop-chip-eq' : ' drop-chip-exp');
                var displayName = kind === 'consumable'
                  ? OB64.consumableName(newItemId)
                  : (newItemId ? OB64.itemName(newItemId) : '\u2014');
                dchip.textContent = displayName;
              });
            });
            dropsRow.appendChild(dchip);
          })(s);
        }
      } else {
        var nodrop = document.createElement('span');
        nodrop.className = 'drops-missing';
        nodrop.textContent = 'no drop entry';
        nodrop.title = OB64.className(classId) + ' is not in the creature drop table (ROM 0x142258). Extending that table is a follow-up.';
        dropsRow.appendChild(nodrop);
      }
      chip.appendChild(dropsRow);
      return chip;
    }

    function rebuildCardBody(card, rec) {
      // Replace the body (grid) while keeping the header intact
      var body = card.querySelector('.encounter-body');
      if (!body) return;
      body.innerHTML = '';
      // Two sub-grids: 6 warm tiles in a 3-col grid (fills cleanly), 4 cold
      // tiles in a 4-col grid (fills cleanly). Keeps cold tiles from being
      // stretched by a tall warm neighbour in the same row.
      var warmSection = document.createElement('div');
      warmSection.className = 'encounter-section encounter-section-warm';
      var coldSection = document.createElement('div');
      coldSection.className = 'encounter-section encounter-section-cold';
      for (var s = 0; s < rec.slots.length; s++) {
        var slot = rec.slots[s];
        var tile = document.createElement('div');
        tile.className = 'terrain-tile terrain-slot-' + s;
        if (s >= 6) tile.classList.add('terrain-cold');
        var label = document.createElement('div');
        label.className = 'terrain-label';
        label.textContent = slot.terrainName;
        tile.appendChild(label);

        // One creature column per row side (A and B). Usually both sides
        // hold the same class (single creature); if different, it renders
        // as a 50/50 pair.
        var pair = document.createElement('div');
        pair.className = 'creature-pair';
        if (slot.classA === slot.classB) {
          // Single creature — one chip whose commit updates BOTH sides.
          pair.appendChild(renderCreatureSlot(card, rec, slot, 'classA', true));
        } else {
          // 50/50 pair — two chips, each edits its own side independently.
          pair.appendChild(renderCreatureSlot(card, rec, slot, 'classA', false));
          pair.appendChild(renderCreatureSlot(card, rec, slot, 'classB', false));
        }
        tile.appendChild(pair);

        // Secondary chip: "make this a 50/50 pair" affordance (only when
        // slot has a single creature, not empty)
        if (slot.classA && slot.classA === slot.classB) {
          (function(slot_) {
            var splitBtn = document.createElement('button');
            splitBtn.className = 'slot-split-btn';
            splitBtn.textContent = '+ pair';
            splitBtn.title = 'Add a second creature so this terrain spawns a 50/50 mix';
            splitBtn.addEventListener('click', function() {
              openClassPickerModal(0, function(newClass) {
                if (!newClass || newClass === slot_.classA) return;
                slot_.classB = newClass;
                dirty.encounters = true;
                markChanged();
                rebuildCardBody(card, rec);
              });
            });
            tile.appendChild(splitBtn);
          })(slot);
        }

        (s < 6 ? warmSection : coldSection).appendChild(tile);
      }
      body.appendChild(warmSection);
      body.appendChild(coldSection);
    }

    // Build cards — one per non-empty record
    var grid = document.createElement('div');
    grid.className = 'encounter-grid';
    var records = rom.neutralEncounters.records;
    for (var i = 0; i < records.length; i++) {
      var rec = records[i];
      if (rec.isEmpty) continue;
      (function(rec) {
        var card = document.createElement('div');
        card.className = 'encounter-card';

        // Header
        var header = document.createElement('div');
        header.className = 'encounter-header';
        var sceneName = OB64.ENCOUNTER_SCENARIO_NAMES[rec.s0];
        var nameEl = document.createElement('div');
        nameEl.className = 'encounter-name';
        nameEl.textContent = sceneName || ('Scenario ' + rec.s0 + ' (unmapped)');
        header.appendChild(nameEl);
        var meta = document.createElement('div');
        meta.className = 'encounter-meta';
        meta.textContent = '$s0 ' + rec.s0 + ' \u00b7 ROM 0x' + rec.offset.toString(16);
        header.appendChild(meta);
        card.appendChild(header);

        var body = document.createElement('div');
        body.className = 'encounter-body';
        card.appendChild(body);
        rebuildCardBody(card, rec);

        grid.appendChild(card);
      })(rec);
    }
    panel.appendChild(grid);

    // ============================================================
    // Class picker modal — opens a centred dialog with two tabs:
    //   1. Vanilla — the 32-ish classes already present in the encounter
    //      table (computed at render time). These are safe & combat-tested.
    //   2. All classes — every other class 0x01-0xA4 with drop-status tags
    //      so the user knows which won't drop loot or might not render.
    //
    // Calls onPick(classId) with the chosen class ID (0 = remove).
    // Closes on overlay click, close button, Escape, or after a commit.
    // ============================================================
    function openClassPickerModal(currentClass, onPick) {
      var committed = false;

      var overlay = document.createElement('div');
      overlay.className = 'class-picker-overlay';

      var modal = document.createElement('div');
      modal.className = 'class-picker-modal';
      overlay.appendChild(modal);

      var header = document.createElement('div');
      header.className = 'class-picker-header';
      var title = document.createElement('div');
      title.className = 'class-picker-title';
      title.textContent = currentClass
        ? 'Change creature (currently: ' + OB64.className(currentClass) + ')'
        : 'Pick a creature';
      header.appendChild(title);
      var closeBtn = document.createElement('button');
      closeBtn.className = 'class-picker-close';
      closeBtn.textContent = '\u00d7';
      closeBtn.title = 'Cancel (Esc)';
      closeBtn.addEventListener('click', close);
      header.appendChild(closeBtn);
      modal.appendChild(header);

      // Tab bar
      var tabBar = document.createElement('div');
      tabBar.className = 'class-picker-tabs';
      var tabVanilla = document.createElement('button');
      tabVanilla.className = 'class-picker-tab active';
      tabVanilla.textContent = 'Vanilla encounters (' + vanillaClasses.length + ')';
      tabVanilla.addEventListener('click', function() { switchTab('vanilla'); });
      tabBar.appendChild(tabVanilla);
      var tabAll = document.createElement('button');
      tabAll.className = 'class-picker-tab';
      // Total class count minus class 0 minus the vanilla set
      var allClassIds = Object.keys(OB64.CLASS_NAMES).map(function(x){return parseInt(x);}).filter(function(x){return x > 0;});
      var otherClasses = allClassIds.filter(function(id) { return vanillaClasses.indexOf(id) === -1; }).sort(function(a,b){return a-b;});
      tabAll.textContent = 'All other classes (' + otherClasses.length + ')';
      tabAll.addEventListener('click', function() { switchTab('all'); });
      tabBar.appendChild(tabAll);
      modal.appendChild(tabBar);

      // Search input
      var searchWrap = document.createElement('div');
      searchWrap.className = 'class-picker-search-wrap';
      var search = document.createElement('input');
      search.type = 'text';
      search.className = 'class-picker-search';
      search.placeholder = 'Filter by class name or ID...';
      search.addEventListener('input', filterList);
      searchWrap.appendChild(search);
      modal.appendChild(searchWrap);

      // Scrollable class list
      var listEl = document.createElement('div');
      listEl.className = 'class-picker-list';
      modal.appendChild(listEl);

      // Footer — cancel + remove-current
      var footer = document.createElement('div');
      footer.className = 'class-picker-footer';
      if (currentClass) {
        var removeBtn = document.createElement('button');
        removeBtn.className = 'class-picker-remove';
        removeBtn.textContent = 'Remove creature (set empty)';
        removeBtn.addEventListener('click', function() { commit(0); });
        footer.appendChild(removeBtn);
      }
      var cancelBtn = document.createElement('button');
      cancelBtn.className = 'class-picker-cancel';
      cancelBtn.textContent = 'Cancel';
      cancelBtn.addEventListener('click', close);
      footer.appendChild(cancelBtn);
      modal.appendChild(footer);

      // Active tab state
      var activeTab = 'vanilla';
      function switchTab(t) {
        activeTab = t;
        tabVanilla.classList.toggle('active', t === 'vanilla');
        tabAll.classList.toggle('active', t === 'all');
        renderList();
      }

      function renderList() {
        listEl.innerHTML = '';
        var ids = activeTab === 'vanilla' ? vanillaClasses : otherClasses;
        var q = (search.value || '').toLowerCase();
        for (var i = 0; i < ids.length; i++) {
          var id = ids[i];
          var name = OB64.className(id);
          var idStr = '0x' + id.toString(16).padStart(2, '0');
          if (q && name.toLowerCase().indexOf(q) === -1 && idStr.indexOf(q) === -1) continue;
          (function(cid) {
            var row = document.createElement('div');
            var status = dropStatus(cid);
            row.className = 'class-picker-row drop-' + status;
            if (cid === currentClass) row.classList.add('current');
            var n = document.createElement('span');
            n.className = 'class-picker-row-name';
            n.textContent = OB64.className(cid);
            row.appendChild(n);
            var idTag = document.createElement('span');
            idTag.className = 'class-picker-row-id';
            idTag.textContent = '0x' + cid.toString(16).padStart(2, '0');
            row.appendChild(idTag);
            var statusTag = document.createElement('span');
            statusTag.className = 'class-picker-row-status class-picker-status-' + status;
            statusTag.textContent = dropStatusLabel(status);
            row.appendChild(statusTag);
            row.addEventListener('click', function() { commit(cid); });
            listEl.appendChild(row);
          })(id);
        }
        if (!listEl.children.length) {
          var empty = document.createElement('div');
          empty.className = 'class-picker-empty';
          empty.textContent = 'No classes match';
          listEl.appendChild(empty);
        }
      }

      function filterList() { renderList(); }

      function commit(classId) {
        committed = true;
        close();
        onPick(classId);
      }

      function close() {
        overlay.remove();
        document.removeEventListener('keydown', onKey);
      }

      function onKey(e) {
        if (e.key === 'Escape') close();
      }
      document.addEventListener('keydown', onKey);

      overlay.addEventListener('click', function(e) {
        if (e.target === overlay) close();
      });

      document.body.appendChild(overlay);
      renderList();
      search.focus();
    }

    // ============================================================
    // Item picker for drop editing — pop-up modal (shop-tab style).
    // Accepts both equipment (bit 15 = 1 when raw is built) AND consumables
    // (bit 15 = 0). Consumables show a kindLabel tag to disambiguate IDs
    // that collide across the two namespaces.
    // ============================================================
    function openItemPicker(targetEl, currentItem, onPick) {
      var items = [];
      items.push({ id: 0, name: '(none)', kind: 'none' });
      for (var id in OB64.ITEM_NAMES) {
        items.push({ id: parseInt(id), name: OB64.ITEM_NAMES[id], kind: 'equip' });
      }
      for (var cid in OB64.SAVE.CONSUMABLE_NAMES) {
        var cn = parseInt(cid);
        if (cn === 0) continue;
        items.push({
          id: cn, name: OB64.SAVE.CONSUMABLE_NAMES[cid],
          kind: 'consumable', kindLabel: 'consumable'
        });
      }
      openSaveItemPickerModal({
        title: 'Drop item',
        items: items,
        currentId: currentItem,
        onSelect: function(v, kind) { onPick(v | 0, kind); }
      });
    }
  }

  // ============================================================
  // MAP TAB
  // ============================================================
  function renderMap(panel) {
    panel.innerHTML = '';

    var table = document.createElement('table');
    table.innerHTML = '<thead><tr><th>Node A</th><th>Location A</th><th>Node B</th><th>Location B</th></tr></thead>';
    var tbody = document.createElement('tbody');

    var map = rom.worldMap;
    for (var i = 0; i < map.edges.length; i++) {
      var edge = map.edges[i];
      var tr = document.createElement('tr');

      td(tr, edge.nodeA);
      td(tr, map.locations[edge.nodeA] || '?');
      td(tr, edge.nodeB);
      td(tr, map.locations[edge.nodeB] || '?');

      tbody.appendChild(tr);
    }

    table.appendChild(tbody);
    panel.appendChild(table);
    makeSortable(table);
  }

  // ============================================================
  // SAVE-GAME TAB
  // Edits a RetroArch Mupen64Plus-Next save state or raw 8 MB RDRAM dump.
  // Independent of the ROM — loads its own file, exports its own file.
  // ============================================================
  function renderSaveGame(panel) {
    panel.innerHTML = '';

    // --- Top bar: load / export / status ---
    var bar = document.createElement('div');
    bar.className = 'save-bar';

    var loadInput = document.createElement('input');
    loadInput.type = 'file';
    loadInput.id = 'save-file-input';
    loadInput.accept = '.state,.state1,.state2,.state3,.state4,.state5,.state6,.state7,.state8,.state9,.bin';
    loadInput.style.display = 'none';

    var loadLabel = document.createElement('label');
    loadLabel.className = 'save-load-btn';
    loadLabel.htmlFor = 'save-file-input';
    loadLabel.textContent = 'Load Save';

    var exportBtn = document.createElement('button');
    exportBtn.id = 'btn-save-export';
    exportBtn.className = 'save-export-btn';
    exportBtn.textContent = 'Export Save';
    exportBtn.disabled = !(saveState && saveState.dirty);
    exportBtn.addEventListener('click', handleSaveExport);

    var status = document.createElement('span');
    status.className = 'save-status';
    status.id = 'save-status';
    status.textContent = saveState
      ? buildSaveStatusLine()
      : 'No save loaded. Accepts RetroArch .state (RZIP or uncompressed) or 8 MB .bin RDRAM dumps.';

    bar.appendChild(loadLabel);
    bar.appendChild(loadInput);
    bar.appendChild(exportBtn);
    bar.appendChild(status);
    panel.appendChild(bar);

    loadInput.addEventListener('change', function(e) {
      var file = e.target.files[0];
      if (!file) return;
      handleSaveFileLoad(file);
    });

    if (!saveState) {
      var hint = document.createElement('div');
      hint.className = 'save-empty-hint';
      hint.innerHTML = [
        '<h3>How to produce a save file</h3>',
        '<ul>',
        '<li><strong>RetroArch:</strong> press F2 in-game to save a state. Files live in <code>RetroArch/states/Mupen64Plus-Next/</code> as <code>&lt;rom&gt;.state</code>, <code>.state1</code>, etc.</li>',
        '</ul>',
        '<p class="save-empty-note">Your edits produce <code>&lt;name&gt;-edited.&lt;ext&gt;</code>. The original file is never overwritten.</p>',
      ].join('\n');
      panel.appendChild(hint);
      return;
    }

    // --- Roster ---
    var rosterHeading = document.createElement('h2');
    rosterHeading.className = 'save-section-heading';
    rosterHeading.textContent = 'Roster \u2014 ' + saveState.characters.length + ' characters';
    panel.appendChild(rosterHeading);

    // Add Character button commented out 2026-04-21 — seeded characters
    // don't appear in-game even with +0x1A/+0x1B/alignment filled. There's
    // another activation list or field we haven't located. Re-enable once
    // the real validation mechanism is decoded.
    // var rosterActions = document.createElement('div');
    // rosterActions.className = 'save-inv-actions';
    // var addCharBtn = document.createElement('button');
    // addCharBtn.className = 'save-inv-add';
    // addCharBtn.textContent = '+ Add Character (to reserve)';
    // addCharBtn.addEventListener('click', openAddCharacterModal);
    // rosterActions.appendChild(addCharBtn);
    // panel.appendChild(rosterActions);

    var grid = document.createElement('div');
    grid.className = 'class-cards save-roster';
    for (var i = 0; i < saveState.characters.length; i++) {
      grid.appendChild(buildCharacterCard(saveState.characters[i]));
    }
    panel.appendChild(grid);

    // --- Game state ---
    var gsHeading = document.createElement('h2');
    gsHeading.className = 'save-section-heading';
    gsHeading.textContent = 'Game State';
    panel.appendChild(gsHeading);
    panel.appendChild(buildGameStatePanel(saveState.gameState));

    // --- Army inventory (tabbed, mirrors in-game Item menu) ---
    var invHeading = document.createElement('h2');
    invHeading.className = 'save-section-heading';
    var eqTotal = saveState.inventory.entries.length;
    var cTotal = saveState.consumableInventory.entries.length;
    invHeading.textContent = 'Army Inventory \u2014 ' + eqTotal + ' equipment, ' + cTotal + ' consumable/treasure';
    panel.appendChild(invHeading);
    panel.appendChild(buildTabbedInventory());
  }

  function buildSaveStatusLine() {
    if (!saveState) return '';
    var fmtLabel = { 'rzip': 'RZIP .state', 'state-raw': 'uncompressed .state', 'bin': '8 MB .bin' }[saveState.format] || saveState.format;
    var armyHex = '0x' + saveState.armyBase.toString(16).padStart(6, '0');
    return 'Loaded ' + (saveFileName || 'save') + ' \u2014 ' + fmtLabel + ' \u2014 ' +
           saveState.characters.length + ' characters \u2014 roster at RDRAM ' + armyHex;
  }

  function handleSaveFileLoad(file) {
    var reader = new FileReader();
    var statusEl = document.getElementById('save-status');
    if (statusEl) statusEl.textContent = 'Loading ' + file.name + '...';
    reader.onload = function(ev) {
      try {
        saveState = OB64.parseSaveFile(ev.target.result);
        saveState.dirty = false;
        saveFileName = file.name;
        renderSaveGame(document.getElementById('panel-save'));
      } catch (err) {
        console.error(err);
        if (statusEl) statusEl.textContent = 'Error: ' + err.message;
        showErrorModal('Save load failed', err.message);
      }
    };
    reader.readAsArrayBuffer(file);
  }

  function handleSaveExport() {
    if (!saveState) return;
    try {
      OB64.downloadSaveFile(saveState, saveFileName);
      // Clear dirty flag after successful export so the button disables
      // until the next edit.
      saveState.dirty = false;
      renderSaveExportButtonState();
      var statusEl = document.getElementById('save-status');
      if (statusEl) statusEl.textContent = buildSaveStatusLine() + ' \u2014 exported.';
    } catch (err) {
      console.error(err);
      showErrorModal('Save export failed', err.message);
    }
  }

  function buildCharacterCard(ch) {
    var card = document.createElement('div');
    card.className = 'class-card save-char-card';
    card.dataset.slotOff = ch.slotOff;

    // Header: name + slot index badge + action buttons
    var hdr = document.createElement('div');
    hdr.className = 'class-card-header';
    var nameEl = document.createElement('div');
    nameEl.className = 'class-card-name save-name';
    nameEl.textContent = ch.name;
    nameEl.title = 'Click to edit name';
    nameEl.addEventListener('click', function() { editCharName(nameEl, ch); });
    var idEl = document.createElement('div');
    idEl.className = 'class-card-id';
    idEl.textContent = 'Slot ' + ch.slotIndex;
    hdr.appendChild(nameEl);
    hdr.appendChild(idEl);
    card.appendChild(hdr);

    // Repair button — shown on slots where ANY of +0x1A/+0x1B/+0x28 are zero.
    // Real characters carry non-zero values in all three. Seeds from older
    // versions of Add Character miss one or more.
    var R = saveState.rdram, F = OB64.SAVE.FIELD;
    var isBrokenSeed = (R[ch.slotOff + F.FLAG_1A] === 0 ||
                       R[ch.slotOff + F.FLAG_1B] === 0 ||
                       R[ch.slotOff + F.ALIGNMENT] === 0);
    var actions = document.createElement('div');
    actions.className = 'save-char-actions';
    if (isBrokenSeed) {
      var repair = document.createElement('button');
      repair.className = 'save-char-btn save-char-repair';
      repair.textContent = 'Repair';
      repair.title = 'Fill in +0x1A/+0x1B/alignment bytes that earlier versions of "Add Character" missed. The game hides slots where these are all zero.';
      repair.addEventListener('click', function() { repairSeededCharacter(ch); });
      actions.appendChild(repair);
    }
    var del = document.createElement('button');
    del.className = 'save-char-btn save-char-delete';
    del.textContent = '\u2715';
    del.title = 'Delete this character (zero the 56-byte slot).';
    del.addEventListener('click', function() {
      if (confirm('Delete ' + ch.name + ' (slot ' + ch.slotIndex + ')? This zeroes the slot.')) {
        deleteCharacter(ch);
      }
    });
    actions.appendChild(del);
    card.appendChild(actions);

    // Meta row: class + level + HP
    var meta = document.createElement('div');
    meta.className = 'class-card-meta save-meta';
    var classEl = document.createElement('span');
    classEl.className = 'save-class-name creature-name';
    classEl.textContent = OB64.className(ch.classId);
    classEl.title = 'Click to change class';
    classEl.addEventListener('click', function() { editCharClass(classEl, ch); });
    var lvlEl = document.createElement('span');
    lvlEl.className = 'save-level';
    lvlEl.innerHTML = 'Lv <strong>' + ch.level + '</strong>';
    lvlEl.title = 'Click to edit level';
    lvlEl.addEventListener('click', function() { editCharLevel(lvlEl, ch); });
    var hpEl = document.createElement('span');
    hpEl.className = 'save-hp save-hp-edit';
    hpEl.innerHTML = 'HP <strong>' + ch.hpCur + '</strong>/<strong>' + ch.hpMax + '</strong>';
    hpEl.title = 'Click to edit max HP (and sets current to max).';
    hpEl.addEventListener('click', function() { editCharHp(hpEl, ch); });
    meta.appendChild(classEl);
    meta.appendChild(lvlEl);
    meta.appendChild(hpEl);
    card.appendChild(meta);

    // Element / Alignment / Experience row
    var attrLabel = document.createElement('div');
    attrLabel.className = 'class-card-section-label';
    attrLabel.textContent = 'Attributes';
    card.appendChild(attrLabel);

    var attrRow = document.createElement('div');
    attrRow.className = 'save-attr-row';
    attrRow.appendChild(buildElementField(ch));
    attrRow.appendChild(buildAlignmentField(ch));
    attrRow.appendChild(buildExpField(ch));
    card.appendChild(attrRow);

    // Stats grid
    var statsLabel = document.createElement('div');
    statsLabel.className = 'class-card-section-label';
    statsLabel.textContent = 'Stats';
    card.appendChild(statsLabel);
    var statsGrid = document.createElement('div');
    statsGrid.className = 'stats-grid';
    var statKeys = ['STR', 'VIT', 'INT', 'MEN', 'AGI', 'DEX'];
    for (var i = 0; i < statKeys.length; i++) {
      statsGrid.appendChild(buildSaveStatEntry(statKeys[i], ch));
    }
    card.appendChild(statsGrid);

    // Equipment — u8 item id overrides at +0x2B/+0x2D/+0x2F/+0x31.
    // Byte = 0 means "use class default from class def B34-41".
    var equipLabel = document.createElement('div');
    equipLabel.className = 'class-card-section-label';
    equipLabel.textContent = 'Equipment';
    card.appendChild(equipLabel);
    var equipTags = document.createElement('div');
    equipTags.className = 'equip-tags';
    [
      { key: 'weapon',  label: 'Wpn'     },
      { key: 'body',    label: 'Body'    },
      { key: 'offhand', label: 'Off'     },
      { key: 'head',    label: 'Head'    },
    ].forEach(function(eq) {
      equipTags.appendChild(buildSaveEquipTag(eq.key, eq.label, ch));
    });
    card.appendChild(equipTags);

    return card;
  }

  function buildSaveEquipTag(field, label, ch) {
    var tag = document.createElement('span');
    tag.className = 'equip-tag save-equip-tag';
    var itemId = ch.equip[field];
    var itemLabel = itemId ? OB64.itemName(itemId) : '(class default)';
    tag.innerHTML = '<em>' + label + ':</em> ' + itemLabel;
    var hint = field === 'offhand'
      ? 'Off-hand slot \u2014 shield / spellbook / accessory. 0 = class default.'
      : '0 = class default.';
    tag.title = 'Click to change. ' + hint;
    tag.addEventListener('click', function() { editCharEquip(tag, ch, field, label); });
    return tag;
  }

  function editCharEquip(el, ch, field, label) {
    // Slot-appropriate item pool, rendered as a pop-up modal (shop-tab style).
    var items = itemsForCharacterSlot(field);
    openSaveItemPickerModal({
      title: label + ' \u2014 ' + ch.name,
      items: items,
      currentId: ch.equip[field] || 0,
      includeNone: true,
      onSelect: function(id) {
        ch.equip[field] = id;
        OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
        var itemLabel = id ? OB64.itemName(id) : '(class default)';
        el.innerHTML = '<em>' + label + ':</em> ' + itemLabel;
        markChanged();
      }
    });
  }

  // Lightweight enum-picker modal — same shell as the item picker, but rows
  // are just labels (no sprite, no id hint). Used for Month, Element, Gender,
  // and any other small enum field the user wants to edit via modal.
  function openSaveEnumPickerModal(opts) {
    // Accept options as either {id: label, ...} or [{id, label}].
    var items = [];
    if (Array.isArray(opts.options)) {
      items = opts.options.slice();
    } else {
      for (var k in opts.options) {
        items.push({ id: parseInt(k), label: opts.options[k] });
      }
    }
    var overlay = document.createElement('div');
    overlay.className = 'item-modal-overlay';
    overlay.addEventListener('click', function(ev) { if (ev.target === overlay) close(); });

    var modal = document.createElement('div');
    modal.className = 'item-modal save-enum-picker';
    overlay.appendChild(modal);

    var header = document.createElement('div');
    header.className = 'item-modal-header';
    var title = document.createElement('h2');
    title.textContent = opts.title;
    header.appendChild(title);
    var btnClose = document.createElement('button');
    btnClose.className = 'item-modal-close';
    btnClose.textContent = '\u00D7';
    btnClose.addEventListener('click', close);
    header.appendChild(btnClose);
    modal.appendChild(header);

    var body = document.createElement('div');
    body.className = 'item-modal-body save-enum-picker-body';
    var col = document.createElement('div');
    col.className = 'item-modal-col';
    items.forEach(function(item) {
      var row = document.createElement('div');
      row.className = 'item-modal-row save-enum-row';
      if (item.id === opts.currentId) row.classList.add('selected');
      var name = document.createElement('span');
      name.className = 'item-modal-name';
      name.textContent = item.label;
      row.appendChild(name);
      var idHint = document.createElement('span');
      idHint.className = 'item-modal-price';
      idHint.textContent = String(item.id);
      row.appendChild(idHint);
      row.addEventListener('click', function() { opts.onSelect(item.id); close(); });
      col.appendChild(row);
    });
    body.appendChild(col);
    modal.appendChild(body);

    document.body.appendChild(overlay);
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', esc);
    }
    var esc = function(ev) { if (ev.key === 'Escape') close(); };
    document.addEventListener('keydown', esc);
  }

  // Allowed items per character slot.
  //   weapon  → all weapons (0x01-0x86)
  //   body    → armor + robes/clothing (0x99-0xDB)
  //   offhand → shields + spellbooks + accessories
  //   head    → helms/headgear + accessories (helms slot can hold accessories
  //             for some classes)
  function itemsForCharacterSlot(field) {
    var out = [];
    for (var iid in OB64.ITEM_NAMES) {
      var id = parseInt(iid);
      if (id === 0) continue;
      var t = OB64.tabForItemId(id);
      var keep =
        (field === 'weapon'  && t === 'weapon') ||
        (field === 'body'    && t === 'armor') ||
        (field === 'offhand' && (t === 'shield' || t === 'spellbook' || t === 'accessory')) ||
        (field === 'head'    && (t === 'head'   || t === 'accessory'));
      if (keep) out.push({ id: id, name: OB64.itemName(id), kind: 'equip' });
    }
    return out;
  }

  function buildElementField(ch) {
    var wrap = document.createElement('div');
    wrap.className = 'save-attr-field';
    var lbl = document.createElement('span');
    lbl.className = 'save-attr-label';
    lbl.textContent = 'Element';
    wrap.appendChild(lbl);
    var val = document.createElement('span');
    val.className = 'save-attr-value save-attr-clickable';
    val.textContent = OB64.SAVE.ELEMENT_OVERRIDES[ch.element] || String(ch.element);
    val.title = 'Click to change element override. 0 = class default. Experimental field \u2014 verify in-game.';
    val.addEventListener('click', function() {
      openSaveEnumPickerModal({
        title: 'Element override \u2014 ' + ch.name,
        options: OB64.SAVE.ELEMENT_OVERRIDES,
        currentId: ch.element,
        onSelect: function(v) {
          ch.element = v;
          OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
          val.textContent = OB64.SAVE.ELEMENT_OVERRIDES[v] || String(v);
          val.classList.add('modified');
          markChanged();
        }
      });
    });
    wrap.appendChild(val);
    return wrap;
  }

  function alignmentLabel(v) {
    for (var i = 0; i < OB64.SAVE.ALIGNMENT_BUCKETS.length; i++) {
      var b = OB64.SAVE.ALIGNMENT_BUCKETS[i];
      if (v >= b.min && v <= b.max) return b.label;
    }
    return '';
  }

  function buildAlignmentField(ch) {
    var wrap = document.createElement('div');
    wrap.className = 'save-attr-field';
    var lbl = document.createElement('span');
    lbl.className = 'save-attr-label';
    lbl.textContent = 'Align';
    wrap.appendChild(lbl);
    var val = document.createElement('span');
    val.className = 'save-attr-value save-attr-clickable';
    val.textContent = ch.alignment + ' (' + alignmentLabel(ch.alignment) + ')';
    val.title = 'Click to edit. 0=Chaotic, 50=Neutral, 100=Lawful.';
    val.addEventListener('click', function() {
      makeNumericInput(val, ch.alignment, 0, 100, function(v) {
        ch.alignment = v;
        OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
        val.textContent = v + ' (' + alignmentLabel(v) + ')';
      });
    });
    wrap.appendChild(val);
    return wrap;
  }

  function buildExpField(ch) {
    var wrap = document.createElement('div');
    wrap.className = 'save-attr-field';
    var lbl = document.createElement('span');
    lbl.className = 'save-attr-label';
    lbl.textContent = 'Exp';
    wrap.appendChild(lbl);
    var val = document.createElement('span');
    val.className = 'save-attr-value save-attr-clickable';
    val.textContent = ch.exp;
    val.title = 'Click to edit experience toward next level (0-99).';
    val.addEventListener('click', function() {
      makeNumericInput(val, ch.exp, 0, 99, function(v) {
        ch.exp = v;
        OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
        val.textContent = v;
      });
    });
    wrap.appendChild(val);
    return wrap;
  }

  function buildSaveStatEntry(key, ch) {
    var entry = document.createElement('div');
    entry.className = 'stat-entry save-stat-entry';
    var statColor = {
      STR: 'ob-stat-str', VIT: 'ob-stat-vit',
      INT: 'ob-stat-int', MEN: 'ob-stat-men',
      AGI: 'ob-stat-agi', DEX: 'ob-stat-dex',
    }[key] || '';
    var labelEl = document.createElement('span');
    labelEl.className = 'stat-label ' + statColor;
    labelEl.textContent = key;
    var valEl = document.createElement('span');
    valEl.className = 'stat-value';
    valEl.textContent = ch.stats[key];
    valEl.title = 'Click to edit';
    valEl.addEventListener('click', function() { editCharStat(valEl, ch, key); });
    entry.appendChild(labelEl);
    entry.appendChild(valEl);
    return entry;
  }


  // Active inventory sub-tab (persists across re-renders of the Save tab).
  var saveInvActiveTab = 'weapon';

  function buildTabbedInventory() {
    var wrap = document.createElement('div');
    wrap.className = 'save-inv-wrap';

    var eqByTab = {}, conByTab = {};
    OB64.SAVE.INVENTORY_TABS.forEach(function(t) { eqByTab[t.id] = []; conByTab[t.id] = []; });
    saveState.inventory.entries.forEach(function(e) {
      var tab = OB64.tabForItemId(e.itemId);
      if (tab) eqByTab[tab].push(e);
    });
    saveState.consumableInventory.entries.forEach(function(e) {
      var tab = OB64.tabForConsumableId(e.consumableId);
      conByTab[tab].push(e);
    });

    var tabBar = document.createElement('div');
    tabBar.className = 'save-inv-tabs';
    var body = document.createElement('div');
    body.className = 'save-inv-body';

    OB64.SAVE.INVENTORY_TABS.forEach(function(t) {
      var btn = document.createElement('button');
      btn.className = 'save-inv-tab';
      if (t.id === saveInvActiveTab) btn.classList.add('active');
      var count = (eqByTab[t.id] || []).length + (conByTab[t.id] || []).length;
      var iconUrl = t.icon ? ('resources/Item%20Icons/' + encodeURIComponent(t.icon)) : '';
      btn.innerHTML =
        (iconUrl ? '<img class="save-inv-tab-icon" src="' + iconUrl + '" alt="">' : '') +
        '<span class="save-inv-tab-label">' + t.label + '</span>' +
        '<span class="save-inv-tab-count">' + count + '</span>';
      btn.addEventListener('click', function() {
        saveInvActiveTab = t.id;
        tabBar.querySelectorAll('.save-inv-tab').forEach(function(b) { b.classList.remove('active'); });
        btn.classList.add('active');
        body.innerHTML = '';
        body.appendChild(buildInventoryTabBody(t.id, eqByTab[t.id], conByTab[t.id]));
      });
      tabBar.appendChild(btn);
    });
    wrap.appendChild(tabBar);

    body.appendChild(buildInventoryTabBody(saveInvActiveTab, eqByTab[saveInvActiveTab], conByTab[saveInvActiveTab]));
    wrap.appendChild(body);
    return wrap;
  }

  function buildInventoryTabBody(tabId, eqEntries, conEntries) {
    var wrap = document.createElement('div');

    var actions = document.createElement('div');
    actions.className = 'save-inv-actions';
    var addBtn = document.createElement('button');
    addBtn.className = 'save-inv-add';
    addBtn.textContent = '+ Add ' + tabId.charAt(0).toUpperCase() + tabId.slice(1);
    addBtn.addEventListener('click', function() { openInventoryAddModal(tabId); });
    actions.appendChild(addBtn);
    wrap.appendChild(actions);

    var total = (eqEntries || []).length + (conEntries || []).length;
    if (total === 0) {
      var empty = document.createElement('div');
      empty.className = 'save-inv-empty';
      empty.textContent = 'No ' + tabId + ' items in the army inventory.';
      wrap.appendChild(empty);
      return wrap;
    }

    var list = document.createElement('div');
    list.className = 'save-inv-list';
    (eqEntries || []).forEach(function(e) { list.appendChild(buildInventoryRowEq(e)); });
    (conEntries || []).forEach(function(e) { list.appendChild(buildInventoryRowConsumable(e)); });
    wrap.appendChild(list);
    return wrap;
  }

  function buildInventoryRowEq(entry) {
    var row = document.createElement('div');
    row.className = 'save-inv-row';
    var name = OB64.itemName(entry.itemId);
    row.appendChild(makeSaveItemIcon(name));
    var nm = document.createElement('span');
    nm.className = 'save-inv-name';
    nm.textContent = name;
    row.appendChild(nm);
    var count = document.createElement('span');
    count.className = 'save-inv-count';
    count.textContent = entry.owned;
    count.title = 'Click to edit owned count';
    count.addEventListener('click', function() {
      makeNumericInput(count, entry.owned, 0, 255, function(v) {
        entry.owned = v;
        OB64.writeInventoryEntry(saveState.rdram, entry);
        count.textContent = v;
      });
    });
    row.appendChild(count);
    return row;
  }

  function buildInventoryRowConsumable(entry) {
    var row = document.createElement('div');
    row.className = 'save-inv-row';
    var name = OB64.consumableName(entry.consumableId);
    row.appendChild(makeSaveItemIcon(name));
    var nm = document.createElement('span');
    nm.className = 'save-inv-name';
    nm.textContent = name;
    row.appendChild(nm);
    var count = document.createElement('span');
    count.className = 'save-inv-count';
    count.textContent = entry.count;
    count.title = 'Click to edit count';
    count.addEventListener('click', function() {
      makeNumericInput(count, entry.count, 0, 255, function(v) {
        entry.count = v;
        OB64.writeConsumableInventoryEntry(saveState.rdram, entry);
        count.textContent = v;
      });
    });
    row.appendChild(count);
    return row;
  }

  function makeSaveItemIcon(name) {
    var img = document.createElement('img');
    img.className = 'save-inv-icon';
    img.src = itemIconURL(name);
    img.alt = '';
    img.addEventListener('error', function() { img.style.visibility = 'hidden'; });
    return img;
  }

  function refreshInventorySection() {
    renderSaveGame(document.getElementById('panel-save'));
  }

  // Find the first unused slot in the character array. A slot is "free" if
  // its class_id byte (+0x11) is zero — matches OB64.parseCharacter's "empty"
  // check. Returns the absolute RDRAM offset, or -1 if no free slot exists.
  function findFirstFreeSlot() {
    var base = saveState.armyBase;
    var stride = OB64.SAVE.CHAR_STRIDE;
    var max = OB64.SAVE.MAX_SLOTS;
    for (var i = 0; i < max; i++) {
      var off = base + i * stride;
      if (off + stride > saveState.rdram.length) break;
      if (saveState.rdram[off + OB64.SAVE.FIELD.CLASS_ID] === 0) return off;
    }
    return -1;
  }

  // "Add character" flow: pick a class via modal, then write a seed record
  // into the first free character slot. Lands in the reserve pool by default
  // (no unit table manipulation needed \u2014 the unit table is a separate
  // structure we haven't decoded, pending-tasks #10).
  function openAddCharacterModal() {
    var freeOff = findFirstFreeSlot();
    if (freeOff < 0) {
      showErrorModal('No free slot', 'The character array is full. Remove a character before adding a new one.');
      return;
    }
    // Class picker (modal, icon-less \u2014 class ids are not items)
    openSaveEnumPickerModal({
      title: 'Add character \u2014 pick class',
      options: OB64.CLASS_NAMES,
      currentId: 0,
      onSelect: function(classId) {
        seedNewCharacter(freeOff, classId);
        refreshInventorySection();
      }
    });
  }

  // Writes a minimal, in-game-safe character record at the given slot.
  // Seeds all fields the game looks at to consider a slot valid/visible:
  //   +0x14 gender, +0x1A/+0x1B (unknown but always non-zero on real chars),
  //   +0x28 alignment (0=Chaotic/hidden slot), HP, stats, slot_index.
  function seedNewCharacter(slotOff, classId) {
    var F = OB64.SAVE.FIELD;
    var slotIndex = ((slotOff - saveState.armyBase) / OB64.SAVE.CHAR_STRIDE) + 1;
    var ch = {
      slotOff: slotOff,
      slotIndex: slotIndex,
      name: 'Recruit',
      classId: classId,
      level: 1,
      gender: 0,
      element: 0,
      alignment: 50,
      exp: 0,
      hpMax: 50,
      hpCur: 50,
      stats: { STR: 50, VIT: 50, INT: 50, MEN: 50, AGI: 50, DEX: 50 },
      equip: { weapon: 0, body: 0, offhand: 0, head: 0 },
    };
    // Zero the slot first, then write the seed, then fill the "must be
    // non-zero" bytes real characters carry (+0x1A, +0x1B) — observed on
    // every real character in state9; missing on slot-49-52 adds that the
    // game refused to display.
    for (var i = 0; i < OB64.SAVE.CHAR_STRIDE; i++) saveState.rdram[slotOff + i] = 0;
    saveState.rdram[slotOff + F.SLOT_INDEX] = slotIndex & 0xFF;
    OB64.writeCharacter(saveState.rdram, slotOff, ch);
    saveState.rdram[slotOff + F.FLAG_1A] = 0x02;
    saveState.rdram[slotOff + F.FLAG_1B] = 0x30;
    markChanged();
  }

  // Repair an existing slot that was seeded before the FLAG_1A/1B/alignment
  // fix landed. Called from the "Repair" button on broken seed cards.
  function repairSeededCharacter(ch) {
    var F = OB64.SAVE.FIELD;
    if (saveState.rdram[ch.slotOff + F.FLAG_1A] === 0) saveState.rdram[ch.slotOff + F.FLAG_1A] = 0x02;
    if (saveState.rdram[ch.slotOff + F.FLAG_1B] === 0) saveState.rdram[ch.slotOff + F.FLAG_1B] = 0x30;
    if (saveState.rdram[ch.slotOff + F.ALIGNMENT] === 0) saveState.rdram[ch.slotOff + F.ALIGNMENT] = 50;
    markChanged();
    refreshInventorySection();
  }

  // Zero a character's slot (removes from the roster).
  function deleteCharacter(ch) {
    for (var i = 0; i < OB64.SAVE.CHAR_STRIDE; i++) saveState.rdram[ch.slotOff + i] = 0;
    markChanged();
    refreshInventorySection();
  }

  // ============================================================
  // Save-tab modal pickers — replace dropdowns with shop-tab-style modals.
  // ============================================================

  // Returns a list of {id, name, kind: 'equip'|'consumable'} items that belong
  // to the named tab. "equip" kinds reference the 295-item equipment table;
  // "consumable" kinds index the 45-entry consumable master.
  function saveItemsForTab(tabId) {
    var out = [];
    if (tabId === 'consumable' || tabId === 'treasure') {
      for (var cid = 1; cid <= 44; cid++) {
        if (OB64.tabForConsumableId(cid) === tabId) {
          out.push({ id: cid, name: OB64.consumableName(cid), kind: 'consumable' });
        }
      }
    } else {
      for (var iid in OB64.ITEM_NAMES) {
        var id = parseInt(iid);
        if (id === 0) continue;
        if (OB64.tabForItemId(id) === tabId) {
          out.push({ id: id, name: OB64.itemName(id), kind: 'equip' });
        }
      }
    }
    return out;
  }

  // Shop-tab-style modal: list of icon + name rows, click to select.
  //   opts = { title, items, currentId, includeNone, onSelect(id) }
  function openSaveItemPickerModal(opts) {
    var overlay = document.createElement('div');
    overlay.className = 'item-modal-overlay';
    overlay.addEventListener('click', function(ev) { if (ev.target === overlay) close(); });

    var modal = document.createElement('div');
    modal.className = 'item-modal save-item-picker';
    overlay.appendChild(modal);

    var header = document.createElement('div');
    header.className = 'item-modal-header';
    var title = document.createElement('h2');
    title.textContent = opts.title;
    header.appendChild(title);
    var btnClose = document.createElement('button');
    btnClose.className = 'item-modal-close';
    btnClose.textContent = '\u00D7';
    btnClose.addEventListener('click', close);
    header.appendChild(btnClose);
    modal.appendChild(header);

    // Search bar — filters the visible rows by substring match (case-insensitive).
    var searchWrap = document.createElement('div');
    searchWrap.className = 'item-modal-search-wrap';
    var search = document.createElement('input');
    search.type = 'text';
    search.className = 'item-modal-search';
    search.placeholder = 'Search \u2026';
    searchWrap.appendChild(search);
    modal.appendChild(searchWrap);

    var body = document.createElement('div');
    body.className = 'item-modal-body save-item-picker-body';

    var items = opts.items.slice();
    if (opts.includeNone) items.unshift({ id: 0, name: '(class default)', kind: 'none' });

    var col = document.createElement('div');
    col.className = 'item-modal-col';
    // Track rows + searchable strings so filter can toggle visibility in-place.
    var entries = [];
    items.forEach(function(item) {
      var row = document.createElement('div');
      row.className = 'item-modal-row';
      if (item.id === opts.currentId) row.classList.add('selected');
      var img = document.createElement('img');
      img.className = 'item-modal-icon';
      img.src = itemIconURL(item.name);
      img.alt = '';
      img.addEventListener('error', function() { img.style.visibility = 'hidden'; });
      row.appendChild(img);
      var name = document.createElement('span');
      name.className = 'item-modal-name';
      name.textContent = item.name;
      if (item.kindLabel) {
        var tag = document.createElement('span');
        tag.className = 'item-modal-kind';
        tag.textContent = item.kindLabel;
        name.appendChild(tag);
      }
      row.appendChild(name);
      var idHint = document.createElement('span');
      idHint.className = 'item-modal-price';
      idHint.textContent = item.id ? ('0x' + item.id.toString(16)) : '';
      row.appendChild(idHint);
      row.addEventListener('click', function() {
        opts.onSelect(item.id, item.kind);
        close();
      });
      col.appendChild(row);
      entries.push({ row: row, needle: (item.name + ' ' + (item.kindLabel || '')).toLowerCase() });
    });
    body.appendChild(col);
    modal.appendChild(body);

    function applyFilter() {
      var q = search.value.trim().toLowerCase();
      var visible = 0;
      entries.forEach(function(e) {
        var match = !q || e.needle.indexOf(q) !== -1;
        e.row.style.display = match ? '' : 'none';
        if (match) visible++;
      });
    }
    search.addEventListener('input', applyFilter);
    // Defer focus until after modal lands in the DOM.
    setTimeout(function() { search.focus(); }, 0);

    document.body.appendChild(overlay);
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', esc);
    }
    var esc = function(ev) { if (ev.key === 'Escape') close(); };
    document.addEventListener('keydown', esc);
  }

  // Small count-edit modal (for equipped/owned on equipment inventory, or count
  // on consumable inventory).
  function openSaveInventoryEditor(entry, isConsumable, onChange) {
    var overlay = document.createElement('div');
    overlay.className = 'item-modal-overlay';
    overlay.addEventListener('click', function(ev) { if (ev.target === overlay) close(); });

    var modal = document.createElement('div');
    modal.className = 'item-modal save-count-editor';
    overlay.appendChild(modal);

    var header = document.createElement('div');
    header.className = 'item-modal-header';
    var title = document.createElement('h2');
    var itemName = isConsumable ? OB64.consumableName(entry.consumableId) : OB64.itemName(entry.itemId);
    title.textContent = itemName;
    header.appendChild(title);
    var btnClose = document.createElement('button');
    btnClose.className = 'item-modal-close';
    btnClose.textContent = '\u00D7';
    btnClose.addEventListener('click', close);
    header.appendChild(btnClose);
    modal.appendChild(header);

    var body = document.createElement('div');
    body.className = 'item-modal-body save-count-editor-body';

    if (isConsumable) {
      body.appendChild(makeCountField('Count', entry.count, function(v) {
        entry.count = v;
        OB64.writeConsumableInventoryEntry(saveState.rdram, entry);
        markChanged();
      }));
    } else {
      body.appendChild(makeCountField('Equipped', entry.equipped, function(v) {
        entry.equipped = v;
        OB64.writeInventoryEntry(saveState.rdram, entry);
        markChanged();
      }));
      body.appendChild(makeCountField('Owned', entry.owned, function(v) {
        entry.owned = v;
        OB64.writeInventoryEntry(saveState.rdram, entry);
        markChanged();
      }));
    }

    var footer = document.createElement('div');
    footer.className = 'item-modal-hint';
    footer.textContent = 'Edits save immediately. Press Esc or click outside to close.';
    modal.appendChild(body);
    modal.appendChild(footer);

    document.body.appendChild(overlay);
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', esc);
      if (onChange) onChange();
    }
    var esc = function(ev) { if (ev.key === 'Escape') close(); };
    document.addEventListener('keydown', esc);
  }

  function makeCountField(label, value, onCommit) {
    var row = document.createElement('div');
    row.className = 'save-count-row';
    var lbl = document.createElement('span');
    lbl.className = 'save-count-label';
    lbl.textContent = label;
    row.appendChild(lbl);
    var input = document.createElement('input');
    input.type = 'number';
    input.min = '0';
    input.max = '255';
    input.value = value;
    input.className = 'save-count-input';
    input.addEventListener('change', function() {
      var v = parseInt(input.value);
      if (!isNaN(v) && v >= 0 && v <= 255) onCommit(v);
    });
    row.appendChild(input);
    return row;
  }

  // Invoked by buildInventoryRow* / "Add" buttons.
  function openInventoryAddModal(tabId) {
    var items = saveItemsForTab(tabId);
    openSaveItemPickerModal({
      title: 'Add ' + tabId.charAt(0).toUpperCase() + tabId.slice(1),
      items: items,
      currentId: 0,
      onSelect: function(id) {
        addItemToInventory(tabId, id);
        refreshInventorySection();
      }
    });
  }
  function openInventoryEditModal(entry, isConsumable) {
    openSaveInventoryEditor(entry, isConsumable, refreshInventorySection);
  }

  // Append an entry to the right inventory list. Writes to the end of the
  // existing zero-terminated list in RDRAM, shifting the terminator.
  function addItemToInventory(tabId, id) {
    if (!id) return;
    if (tabId === 'consumable' || tabId === 'treasure') {
      var list = saveState.consumableInventory.entries;
      var off = OB64.SAVE.CONSUMABLE_INV_BASE + list.length * OB64.SAVE.CONSUMABLE_INV_ENTRY_SIZE;
      var entry = { off: off, consumableId: id, count: 1 };
      list.push(entry);
      OB64.writeConsumableInventoryEntry(saveState.rdram, entry);
    } else {
      var list = saveState.inventory.entries;
      var off = OB64.SAVE.INVENTORY_BASE + list.length * OB64.SAVE.INVENTORY_ENTRY_SIZE;
      var entry = { off: off, itemId: id, equipped: 0, owned: 1 };
      list.push(entry);
      OB64.writeInventoryEntry(saveState.rdram, entry);
    }
    markChanged();
  }

  function buildGameStatePanel(gs) {
    var wrap = document.createElement('div');
    wrap.className = 'save-gamestate';

    var rows = [
      { key: 'goth',     label: 'Goth',     min: 0, max: 0xFFFFFFFF },
      { key: 'scenario', label: 'Scenario', min: 0, max: 255, labels: OB64.SAVE.SCENARIO_LABELS },
    ];
    for (var i = 0; i < rows.length; i++) {
      wrap.appendChild(buildGameStateRow(rows[i], gs));
    }
    return wrap;
  }

  function buildGameStateRow(def, gs) {
    var row = document.createElement('div');
    row.className = 'save-gs-row';
    var lbl = document.createElement('span');
    lbl.className = 'save-gs-label';
    lbl.textContent = def.label;
    var val = document.createElement('span');
    val.className = 'save-gs-value';
    var current = gs[def.key];
    var displayText = String(current);
    if (def.options && def.options[current]) displayText = def.options[current] + ' (' + current + ')';
    if (def.labels && def.labels[current]) displayText = def.labels[current] + ' (' + current + ')';
    val.textContent = displayText;
    val.title = 'Click to edit';
    val.addEventListener('click', function() {
      if (def.options) {
        // Modal picker for enum-style fields (Month, etc.) — per user
        // request, all non-numerical selectors use pop-up modals.
        openSaveEnumPickerModal({
          title: def.label,
          options: def.options,
          currentId: saveState.gameState[def.key],
          onSelect: function(v) {
            saveState.gameState[def.key] = v;
            OB64.writeGameState(saveState.rdram, saveState.gameState);
            val.textContent = (def.options[v] || String(v)) + ' (' + v + ')';
            val.classList.add('modified');
            markChanged();
          }
        });
      } else {
        makeNumericInput(val, current, def.min, def.max, function(v) {
          saveState.gameState[def.key] = v;
          OB64.writeGameState(saveState.rdram, saveState.gameState);
          var t = String(v);
          if (def.labels && def.labels[v]) t = def.labels[v] + ' (' + v + ')';
          val.textContent = t;
        });
      }
    });
    row.appendChild(lbl);
    row.appendChild(val);
    return row;
  }

  // --- Character field editors -----------------------------------

  function editCharName(el, ch) {
    if (el.querySelector('input')) return;
    var prev = el.textContent;
    var inp = document.createElement('input');
    inp.type = 'text';
    inp.maxLength = OB64.SAVE.NAME_MAX_LEN;
    inp.value = ch.name;
    el.textContent = '';
    el.appendChild(inp);
    inp.focus();
    inp.select();
    function commit() {
      var name = inp.value.trim();
      if (name.length > 0 && name !== ch.name) {
        ch.name = name;
        OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
        el.classList.add('modified');
        markChanged();
      }
      el.textContent = ch.name;
    }
    inp.addEventListener('blur', commit);
    inp.addEventListener('keydown', function(e) {
      if (e.key === 'Enter') { e.preventDefault(); commit(); }
      if (e.key === 'Escape') { el.textContent = prev; }
    });
  }

  function editCharClass(el, ch) {
    // Filter to classes that share the character's current unit type
    // (humanoid vs beast/dragon — class def byte B64). Changing between
    // unit types isn't allowed in-game. Requires the ROM to be loaded so
    // we can read classDefs; if not loaded, fall back to the full list.
    var items = classItemsForUnitType(ch.classId);
    var title = 'Change class \u2014 ' + ch.name;
    if (rom && rom.classDefs) {
      var curDef = classDefFor(ch.classId);
      var curType = curDef ? curDef.unitType : null;
      if (curType) title += ' (' + (OB64.unitTypeName(curType)) + ' only)';
    } else {
      title += ' (ROM not loaded \u2014 showing all classes)';
    }
    openSaveEnumPickerModal({
      title: title,
      options: items,
      currentId: ch.classId,
      onSelect: function(v) {
        ch.classId = v;
        OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
        el.textContent = OB64.className(v);
        markChanged();
      }
    });
  }

  // Look up a class-def record by class id. parseClassDefs uses record_index
  // = class_id + 1, so classDefs[class_id + 1] is the record.
  function classDefFor(classId) {
    if (!rom || !rom.classDefs) return null;
    return rom.classDefs[classId + 1] || null;
  }

  // Build the option list for a class-change modal. If the ROM is loaded,
  // filter to classes with the same unit type as the input class. Otherwise
  // return every known class.
  function classItemsForUnitType(classId) {
    var out = [];
    var curDef = classDefFor(classId);
    var curType = curDef ? curDef.unitType : null;
    for (var id in OB64.CLASS_NAMES) {
      var idNum = parseInt(id);
      if (idNum === 0) continue;
      if (curType && rom && rom.classDefs) {
        var def = classDefFor(idNum);
        // Skip if the class def is missing (terminator/sentinel records) or
        // its unit type differs. Also skip records with unitType = 0 since
        // those are the two non-class terminator rows in the table.
        if (!def || def.unitType === 0 || def.unitType !== curType) continue;
      }
      out.push({ id: idNum, label: OB64.className(idNum) });
    }
    return out;
  }

  function editCharLevel(el, ch) {
    makeNumericInput(el, ch.level, 1, 50, function(v) {
      ch.level = v;
      OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
      el.innerHTML = 'Lv <strong>' + v + '</strong>';
    });
  }

  function editCharHp(el, ch) {
    makeNumericInput(el, ch.hpMax, 1, 255, function(v) {
      ch.hpMax = v;
      ch.hpCur = v; // heal to full on max-hp edit
      OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
      el.innerHTML = 'HP <strong>' + v + '</strong>/<strong>' + v + '</strong>';
    });
  }

  function editCharStat(el, ch, key) {
    makeNumericInput(el, ch.stats[key], 0, 999, function(v) {
      ch.stats[key] = v;
      OB64.writeCharacter(saveState.rdram, ch.slotOff, ch);
      el.textContent = v;
    });
  }


  // ============================================================
  // Helpers
  // ============================================================
  function td(tr, text) {
    var el = document.createElement('td');
    el.textContent = text;
    tr.appendChild(el);
    return el;
  }

})();
