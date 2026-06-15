/*
 * squads.js - "Squads" tab: per-scenario squad overrides.
 *
 * Edits squad composition + formation as a per-scenario override (the runtime
 * hook in squadblob.js). Does NOT touch global enemydat. Each override is a
 * 35-byte replacement record keyed by (scenario, target edat).
 *
 * Unit sizes: a class is regular (1 cell) or LARGE (B64==2; rendered as a 2x2
 * footprint). The enemy format stores each unit's anchor cell (1..9), not strict
 * 2x2 occupancy, so we store anchor cells and surface size via validation.
 *
 * State: rom.squadOverrides = { "<scenarioId>:<edatId>": Uint8Array(35) }.
 */
(function (OB64) {
  'use strict';

  var GRID = [3, 2, 1, 6, 5, 4, 9, 8, 7];   // display order: back row top, front row bottom
  var sel = { scenarioId: null, edatId: null };
  var ui = { search: '', notice: '' };
  var STYLE_ID = 'squads-style';

  function hexToBytes(h) { var b = new Uint8Array(h.length / 2); for (var i = 0; i < b.length; i++) b[i] = parseInt(h.substr(i * 2, 2), 16); return b; }
  function key(sid, eid) { return sid + ':' + eid; }
  function cn(id) { return (id && OB64.className) ? OB64.className(id) : (id ? '0x' + id.toString(16) : 'None'); }
  function isLarge(cls) { return !!(OB64.SQUAD_DATA.largeSizes && OB64.SQUAD_DATA.largeSizes[cls]); }
  // boss = display-only label carried forward from the hand-curated boss edat notes.
  // large units don't take a rigid 2x2; they just forbid any unit in an adjacent
  // (8-neighbour) cell. cell -> display grid (row,col): row=(cell-1)//3, col=2-((cell-1)%3).
  function adj(a, b) { var ra = (a - 1) / 3 | 0, ca = 2 - ((a - 1) % 3), rb = (b - 1) / 3 | 0, cb = 2 - ((b - 1) % 3); return a !== b && Math.abs(ra - rb) <= 1 && Math.abs(ca - cb) <= 1; }
  function adjacencyOk(rec) {
    var u = units(rec);
    for (var i = 0; i < u.length; i++) { if (!isLarge(u[i].cls)) continue; for (var j = 0; j < u.length; j++) if (i !== j && adj(u[i].cell, u[j].cell)) return false; }
    return true;
  }
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }

  function classOptionsHtml(cur) {
    var names = OB64.CLASS_NAMES || {}, html = '<option value="0">-- pick a class --</option>';
    for (var k in names) {
      var id = parseInt(k);
      html += '<option value="' + id + '"' + (id === cur ? ' selected' : '') + '>' + esc(names[k]) + (isLarge(id) ? ' (large)' : '') + '</option>';
    }
    return html;
  }

  // ---- record helpers (35-byte record is source of truth) ----
  function units(rec) {
    var out = [];
    if (rec[0]) out.push({ role: 'L', field: 6, cls: rec[0], cell: rec[6] });
    [13, 14, 15].forEach(function (f) { if (rec[f]) out.push({ role: 'B', field: f, cls: rec[7], cell: rec[f] }); });
    [22, 23].forEach(function (f) { if (rec[f]) out.push({ role: 'C', field: f, cls: rec[16], cell: rec[f] }); });
    return out;
  }
  function unitAtCell(rec, cell) { var u = units(rec); for (var i = 0; i < u.length; i++) if (u[i].cell === cell) return u[i]; return null; }
  function occupied(rec) { var s = {}; units(rec).forEach(function (u) { s[u.cell] = 1; }); return s; }
  function freeCell(rec) { var o = occupied(rec); for (var c = 1; c <= 9; c++) if (!o[c]) return c; return 0; }
  function memberCount(rec) { return units(rec).length; }
  function moveUnit(rec, from, to) {
    if (from === to) return;
    var s = unitAtCell(rec, from); if (!s) return;
    var d = unitAtCell(rec, to);
    rec[s.field] = to; if (d) rec[d.field] = from;
  }
  // member groups: B uses class rec[7] + cells 13/14/15; C uses rec[16] + 22/23
  function groupFields(role) { return role === 'B' ? [13, 14, 15] : [22, 23]; }
  function groupClassField(role) { return role === 'B' ? 7 : 16; }
  function groupCount(rec, role) { var f = groupFields(role), n = 0; for (var i = 0; i < f.length; i++) if (rec[f[i]]) n++; return n; }
  function freeGroupFor(rec, cls) {
    if (rec[7] === cls && groupCount(rec, 'B') < 3) return 'B';
    if (rec[16] === cls && groupCount(rec, 'C') < 2) return 'C';
    if (!rec[7]) return 'B';
    if (!rec[16]) return 'C';
    return null;
  }
  function addUnitOfClass(rec, cls) {
    var role = freeGroupFor(rec, cls);
    if (!role) return 'A squad can have the leader plus two member types. Remove one before adding a new class.';
    var f = groupFields(role), slot = 0, i;
    for (i = 0; i < f.length; i++) if (!rec[f[i]]) { slot = f[i]; break; }
    if (!slot) return 'That member type is full.';
    var saved = rec[groupClassField(role)]; rec[groupClassField(role)] = cls;
    var occ = occupied(rec);
    for (var c = 1; c <= 9; c++) {                 // first free cell that keeps adjacency valid
      if (occ[c]) continue;
      rec[slot] = c;
      if (adjacencyOk(rec)) return null;
      rec[slot] = 0;
    }
    rec[groupClassField(role)] = saved;
    return 'No open cell. Large units cannot sit next to another unit.';
  }
  function removeCell(rec, cell) {
    var u = unitAtCell(rec, cell); if (!u || u.role === 'L') return;
    rec[u.field] = 0;
    if (u.role === 'B' && groupCount(rec, 'B') === 0) rec[7] = 0;
    if (u.role === 'C' && groupCount(rec, 'C') === 0) rec[16] = 0;
  }
  function clearGroup(rec, role) {
    var f = groupFields(role);
    for (var i = 0; i < f.length; i++) rec[f[i]] = 0;
    rec[groupClassField(role)] = 0;
  }
  function setGroupClass(rec, role, cls) {
    if (!cls) { clearGroup(rec, role); return; }
    rec[groupClassField(role)] = cls;
  }

  // ---- data ----
  function scenarioById(id) { var d = OB64.SQUAD_DATA.scenarios; for (var i = 0; i < d.length; i++) if (d[i].id === id) return d[i]; return null; }
  function vanillaRec(scn, eid) { for (var i = 0; i < scn.squads.length; i++) if (scn.squads[i].e === eid) return hexToBytes(scn.squads[i].rec); return null; }
  function compLabel(rec) {
    var parts = [cn(rec[0])], nb = groupCount(rec, 'B'), nc = groupCount(rec, 'C');
    if (nb) parts.push((nb > 1 ? nb + 'x ' : '') + cn(rec[7]));
    if (nc) parts.push((nc > 1 ? nc + 'x ' : '') + cn(rec[16]));
    return parts.join(' + ');
  }
  function bossName(sq) { return typeof sq.boss === 'string' ? sq.boss : (sq.bossLabel || ''); }
  function scenarioSearchText(scn) {
    return [scn.name, scn.wikiLabel || '', scn.identityNote || '', scn.wikiHint || '', (scn.tags || []).join(' ')].join(' ');
  }
  function scenarioTraceText(scn) {
    var parts = [];
    if (scn.enemyUnits) parts.push(scn.enemyUnits + ' traced units');
    parts.push(scn.squads.length + ' edats');
    if (scn.traceMethod) parts.push(scn.traceMethod);
    return parts.join(' / ');
  }
  function squadTraceText(sq) {
    var parts = [];
    if (sq.units) parts.push(sq.units + ' traced units');
    if (sq.placements && sq.placements > 1) parts.push(sq.placements + ' placements');
    return parts.join(' / ');
  }

  // ---- styles (themed; injected once) ----
  function injectStyle() {
    if (document.getElementById(STYLE_ID)) return;
    var css = [
      '#panel-squads{--sq-line:rgba(62,45,25,.26);--sq-soft:rgba(104,74,36,.12);--sq-hot:rgba(152,32,24,.12)}',
      '#panel-squads .sq-page{max-width:1180px;margin:0 auto}',
      '#panel-squads .sq-titlebar{display:flex;align-items:flex-end;justify-content:space-between;gap:16px;margin:0 0 12px}',
      '#panel-squads .sq-titlebar h2{color:var(--ob-gold-bright);margin:0;font-size:22px;line-height:1.1}',
      '#panel-squads .sq-kpis{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}',
      '#panel-squads .sq-kpi{border:1px solid var(--ob-parchment-edge);background:var(--ob-parchment);color:var(--ob-ink);border-radius:6px;padding:5px 9px;font-size:12px;box-shadow:var(--ob-shadow-sm)}',
      '#panel-squads .sq-wrap{display:grid;grid-template-columns:minmax(280px,330px) minmax(560px,820px);gap:14px;align-items:start}',
      '#panel-squads .sq-card{background:var(--ob-parchment);color:var(--ob-ink);border:1px solid var(--ob-parchment-edge);border-radius:6px;box-shadow:var(--ob-shadow-sm)}',
      '#panel-squads .sq-list{max-height:calc(100vh - 245px);min-height:520px;overflow:auto;padding:10px}',
      '#panel-squads .sq-detail{padding:14px}',
      '#panel-squads .sq-list-tools{position:sticky;top:0;z-index:2;background:var(--ob-parchment);padding-bottom:8px;border-bottom:1px solid var(--sq-line);margin-bottom:8px}',
      '#panel-squads input.sq-search{width:100%;height:32px;border:1px solid var(--ob-parchment-edge);border-radius:5px;background:#f7ebce;color:var(--ob-ink);padding:0 9px;font-size:13px}',
      '#panel-squads .sq-list-count{display:block;margin-top:5px;color:var(--ob-ink-soft);font-size:11px}',
      '#panel-squads .sq-scn{font-size:12px;font-weight:700;color:var(--ob-ink-soft);margin:9px 2px 4px;display:flex;gap:8px;justify-content:space-between;align-items:center;cursor:pointer;text-transform:uppercase;letter-spacing:.4px}',
      '#panel-squads .sq-scn.on{color:var(--ob-ink)}',
      '#panel-squads .sq-scn-name{min-width:0;overflow:hidden}',
      '#panel-squads .sq-scn-name span{display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}',
      '#panel-squads .sq-scn-name small{display:block;margin-top:1px;font-size:10px;font-weight:700;line-height:1.2;text-transform:none;letter-spacing:0;color:var(--ob-ink-soft);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}',
      '#panel-squads .sq-scn-meta{display:flex;gap:4px;align-items:center;justify-content:flex-end;flex:0 0 auto}',
      '#panel-squads .sq-scn .sq-chip{flex:0 0 auto}',
      '#panel-squads .sq-row{font-size:13px;color:var(--ob-ink);padding:7px 8px;border-radius:5px;cursor:pointer;display:grid;grid-template-columns:minmax(0,1fr) auto;gap:8px;align-items:center;border:1px solid transparent}',
      '#panel-squads .sq-row:hover{background:var(--sq-soft);border-color:var(--sq-line)}',
      '#panel-squads .sq-row.on{background:var(--ob-wood-lo);color:var(--ob-parchment);border-color:var(--ob-wood-hi)}',
      '#panel-squads .sq-row.boss:not(.on){background:var(--sq-hot)}',
      '#panel-squads .sq-row-title{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}',
      '#panel-squads .sq-row-meta{display:flex;gap:4px;align-items:center;justify-content:flex-end}',
      '#panel-squads .sq-chip{display:inline-flex;align-items:center;min-height:17px;border-radius:4px;padding:1px 5px;font-size:10px;font-weight:700;line-height:1;text-transform:uppercase;letter-spacing:.35px;background:var(--ob-parchment-dark);color:var(--ob-ink)}',
      '#panel-squads .sq-row.on .sq-chip{background:rgba(245,230,200,.18);color:var(--ob-parchment)}',
      '#panel-squads .sq-chip.boss{background:var(--ob-wax-red);color:#f5e6c8}',
      '#panel-squads .sq-detail-head{display:flex;justify-content:space-between;gap:12px;align-items:flex-start;border-bottom:1px solid var(--sq-line);padding-bottom:10px;margin-bottom:10px}',
      '#panel-squads .sq-head{font-size:16px;font-weight:700;color:var(--ob-ink);line-height:1.2}',
      '#panel-squads .sq-sub{font-size:12px;color:var(--ob-ink-soft);margin-top:3px;line-height:1.35}',
      '#panel-squads .sq-toggle{display:flex;gap:8px;align-items:center;font-size:13px;padding:8px 10px;border:1px solid var(--ob-parchment-edge);border-radius:6px;margin:0 0 12px;color:var(--ob-ink);background:#f7ebce;max-width:max-content}',
      '#panel-squads .sq-editor-grid{display:grid;grid-template-columns:230px minmax(0,1fr);gap:18px;align-items:start}',
      '#panel-squads .sq-section-label{font-size:12px;font-weight:700;color:var(--ob-ink-soft);margin:0 0 6px;text-transform:uppercase;letter-spacing:.35px}',
      '#panel-squads .sq-grid{display:grid;grid-template-columns:repeat(3,64px);grid-template-rows:repeat(3,58px);gap:6px}',
      '#panel-squads .sq-cell{border:1px dashed var(--ob-parchment-dark);border-radius:5px;display:flex;flex-direction:column;align-items:center;justify-content:center;font-size:10px;line-height:1.15;text-align:center;position:relative;min-width:0;background:rgba(255,255,255,.16)}',
      '#panel-squads .sq-cell.u{border-style:solid;cursor:grab;box-shadow:inset 0 0 0 1px rgba(255,255,255,.16)}',
      '#panel-squads .sq-cell.readonly{cursor:default}',
      '#panel-squads .sq-cell.lead{background:var(--ob-gold);border-color:var(--ob-gold-dim);color:var(--ob-ink)}',
      '#panel-squads .sq-cell.mem{background:var(--ob-bar-teal);border-color:#1d6e56;color:#04261f}',
      '#panel-squads .sq-cell.large{box-shadow:inset 0 0 0 2px rgba(0,0,0,.34)}',
      '#panel-squads .sq-cell-role{position:absolute;top:3px;left:4px;font-size:9px;font-weight:800;opacity:.74}',
      '#panel-squads .sq-cell-name{display:block;max-width:56px;overflow:hidden;text-overflow:ellipsis}',
      '#panel-squads .sq-remove{position:absolute;top:2px;right:2px;width:17px;height:17px;border:0;border-radius:3px;background:rgba(0,0,0,.18);color:inherit;cursor:pointer;font-size:12px;line-height:17px;padding:0}',
      '#panel-squads .sq-pick{display:grid;grid-template-columns:repeat(2,minmax(180px,1fr));gap:10px 12px}',
      '#panel-squads .sq-field{min-width:0}',
      '#panel-squads .sq-pick label{display:block;font-size:12px;font-weight:700;color:var(--ob-ink-soft);margin:0 0 3px}',
      '#panel-squads .sq-pick select{width:100%;height:32px;border:1px solid var(--ob-parchment-edge);border-radius:5px;background:#f7ebce;color:var(--ob-ink);font-size:13px}',
      '#panel-squads .sq-add-row{display:flex;gap:6px}',
      '#panel-squads .sq-add-row select{flex:1;min-width:0}',
      '#panel-squads .sq-action-row{display:flex;gap:8px;align-items:center;margin-top:10px}',
      '#panel-squads .sq-action-row button,#panel-squads #sq-add{font-size:12px;white-space:nowrap}',
      '#panel-squads .sq-readout{min-width:0;padding-top:2px}',
      '#panel-squads .sq-foot{font-size:12px;color:var(--ob-ink-soft);margin-top:12px;padding-top:9px;border-top:1px solid var(--sq-line);display:flex;justify-content:space-between;gap:10px;flex-wrap:wrap}',
      '#panel-squads .sq-status{font-weight:700;color:#1f5f3c}',
      '#panel-squads .sq-warn{font-weight:700;color:var(--ob-wax-red)}',
      '#panel-squads .sq-notice{margin-top:10px;border:1px solid var(--ob-wax-red);background:rgba(152,32,24,.10);color:var(--ob-wax-red);border-radius:5px;padding:7px 9px;font-size:12px}',
      '#panel-squads .sq-empty{color:var(--ob-ink-soft);font-size:13px;padding:14px 4px}',
      '@media (max-width:980px){#panel-squads .sq-wrap{grid-template-columns:1fr}#panel-squads .sq-list{max-height:320px;min-height:260px}#panel-squads .sq-editor-grid{grid-template-columns:1fr}#panel-squads .sq-pick{grid-template-columns:1fr}}'
    ].join('');
    var s = document.createElement('style'); s.id = STYLE_ID; s.textContent = css; document.head.appendChild(s);
  }

  function ensureInit(rom) { if (!rom.squadOverrides) rom.squadOverrides = {}; }

  function squadStats(rom) {
    var stats = { scenarios: 0, squads: 0, overrides: 0 };
    if (!OB64.SQUAD_DATA || !OB64.SQUAD_DATA.scenarios) return stats;
    stats.scenarios = OB64.SQUAD_DATA.scenarios.length;
    for (var i = 0; i < OB64.SQUAD_DATA.scenarios.length; i++) {
      var scn = OB64.SQUAD_DATA.scenarios[i];
      stats.squads += scn.squads.length;
      for (var j = 0; j < scn.squads.length; j++) {
        if (rom.squadOverrides[key(scn.id, scn.squads[j].e)]) stats.overrides++;
      }
    }
    return stats;
  }

  function renderSquads(panel) {
    if (!OB64.SQUAD_DATA) { panel.innerHTML = '<p>Squad data not loaded.</p>'; return; }
    var rom = OB64._romRef && OB64._romRef(); if (!rom) return;
    ensureInit(rom); injectStyle();
    if (sel.scenarioId == null) {
      var first = OB64.SQUAD_DATA.scenarios[0];
      if (first) { sel.scenarioId = first.id; sel.edatId = first.squads[0] && first.squads[0].e; }
    }
    var stats = squadStats(rom);
    panel.innerHTML =
      '<div class="sq-page">' +
      '<div class="sq-titlebar"><h2>Squads</h2><div class="sq-kpis">' +
      '<span class="sq-kpi">' + stats.scenarios + ' runtime keys</span>' +
      '<span class="sq-kpi">' + stats.squads + ' edat rows</span>' +
      '<span class="sq-kpi">' + stats.overrides + ' edited</span>' +
      '</div></div>' +
      '<div class="sq-wrap"><div id="sq-list" class="sq-card sq-list"></div><div id="sq-detail" class="sq-card sq-detail"></div></div>' +
      '</div>';
    renderList(rom); renderDetail(rom);
  }

  function renderList(rom) {
    var el = document.getElementById('sq-list'); if (!el) return;
    var q = ui.search.toLowerCase().trim(), shownScenarios = 0, shownSquads = 0;
    var html = '<div class="sq-list-tools"><input class="sq-search" placeholder="Search runtime keys, bosses, classes, edats" value="' + esc(ui.search) + '">' +
      '<span class="sq-list-count"></span></div><div class="sq-list-body">';
    OB64.SQUAD_DATA.scenarios.forEach(function (scn) {
      var scnText = scenarioSearchText(scn);
      var scenarioMatch = q && scnText.toLowerCase().indexOf(q) >= 0;
      var nOver = 0, rows = [], anyRowMatch = false;
      scn.squads.forEach(function (sq) {
        var over = rom.squadOverrides[key(scn.id, sq.e)], rec = over || hexToBytes(sq.rec), boss = !!sq.boss, bn = bossName(sq);
        if (over) nOver++;
        var text = (bn ? bn + ' - ' : '') + compLabel(rec);
        var hay = (scnText + ' edat ' + sq.e + ' ' + text + ' ' + squadTraceText(sq)).toLowerCase();
        var match = !q || scenarioMatch || hay.indexOf(q) >= 0;
        if (match) anyRowMatch = true;
        rows.push({ sq: sq, over: over, boss: boss, bossName: bn, text: text, match: match });
      });
      if (q && !scenarioMatch && !anyRowMatch) return;
      shownScenarios++;
      var expanded = scn.id === sel.scenarioId || !!q;
      var scnSub = scn.wikiId ? (scn.identityNote ? scn.wikiLabel + ' - ' + scn.identityNote : scn.wikiLabel) : (scn.identityNote || scn.wikiLabel || '');
      html += '<div class="sq-scn' + (scn.id === sel.scenarioId ? ' on' : '') + '" data-scn="' + scn.id + '">' +
        '<span class="sq-scn-name"><span>' + esc(scn.name) + '</span>' +
        (scnSub ? '<small>' + esc(scnSub) + '</small>' : '') + '</span><span class="sq-scn-meta">' +
        (scn.wikiId ? '<span class="sq-chip">wiki ' + scn.wikiId + '</span>' : '') +
        '<span class="sq-chip">key ' + scn.id + '</span>' +
        (nOver ? '<span class="sq-chip">' + nOver + ' edited</span>' : '') + '</span></div>';
      if (!expanded) return;
      rows.forEach(function (row) {
        if (q && !row.match) return;
        shownSquads++;
        html += '<div class="sq-row' + (scn.id === sel.scenarioId && row.sq.e === sel.edatId ? ' on' : '') + (row.boss ? ' boss' : '') +
          '" data-scn="' + scn.id + '" data-eid="' + row.sq.e + '">' +
          '<span class="sq-row-title">' + esc(row.text) + '</span><span class="sq-row-meta">' +
          '<span class="sq-chip">edat ' + row.sq.e + '</span>' +
          (row.sq.units ? '<span class="sq-chip">' + row.sq.units + ' units</span>' : '') +
          (row.boss ? '<span class="sq-chip boss">boss</span>' : '') +
          (row.over ? '<span class="sq-chip">edited</span>' : '') +
          '</span></div>';
      });
    });
    html += shownScenarios ? '</div>' : '<div class="sq-empty">No squads match the current search.</div></div>';
    el.innerHTML = html;
    var count = el.querySelector('.sq-list-count');
    if (count) count.textContent = q ? (shownSquads + ' matching squads') : (shownScenarios + ' scenarios');
    el.querySelectorAll('.sq-scn').forEach(function (n) {
      n.onclick = function () {
        sel.scenarioId = parseInt(this.dataset.scn);
        var s = scenarioById(sel.scenarioId);
        sel.edatId = s.squads[0] && s.squads[0].e;
        ui.notice = '';
        renderSquads(document.getElementById('panel-squads'));
      };
    });
    el.querySelectorAll('.sq-row').forEach(function (n) {
      n.onclick = function () {
        sel.scenarioId = parseInt(this.dataset.scn);
        sel.edatId = parseInt(this.dataset.eid);
        ui.notice = '';
        renderSquads(document.getElementById('panel-squads'));
      };
    });
    var srch = el.querySelector('.sq-search');
    if (srch) srch.oninput = function () {
      ui.search = this.value;
      renderList(rom);
      var next = document.querySelector('#panel-squads .sq-search');
      if (next) { next.focus(); next.setSelectionRange(next.value.length, next.value.length); }
    };
  }

  function gridHtml(rec, editable) {
    editable = editable !== false;
    var h = '<div class="sq-board"><div class="sq-section-label">Formation</div><div class="sq-grid" aria-label="Formation grid">';
    GRID.forEach(function (cell) {
      var u = unitAtCell(rec, cell);
      if (u) {
        var large = isLarge(u.cls), role = u.role === 'L' ? 'Leader' : u.role;
        h += '<div class="sq-cell u ' + (editable ? '' : 'readonly ') + (u.role === 'L' ? 'lead' : 'mem') + (large ? ' large' : '') +
          '" ' + (editable ? 'draggable="true" ' : '') + 'data-cell="' + cell + '">' +
          '<span class="sq-cell-role">' + role + (large ? ' 2x' : '') + '</span>' +
          '<span class="sq-cell-name">' + esc(cn(u.cls)) + '</span>' +
          (editable && u.role !== 'L' ? '<button type="button" class="sq-remove" data-cell="' + cell + '" title="Remove">x</button>' : '') +
          '</div>';
      } else {
        h += '<div class="sq-cell' + (editable ? '' : ' readonly') + '" data-cell="' + cell + '"></div>';
      }
    });
    return h + '</div></div>';
  }

  function pickersHtml(rec) {
    var h = '<div class="sq-pick">';
    h += '<div class="sq-field"><label>Leader class</label><select data-grp="L">' + classOptionsHtml(rec[0]) + '</select></div>';
    if (rec[7]) {
      h += '<div class="sq-field"><label>Member B - ' + esc(cn(rec[7])) + ' x' + groupCount(rec, 'B') + (isLarge(rec[7]) ? ' large' : '') +
        '</label><select data-grp="B">' + classOptionsHtml(rec[7]) + '</select></div>';
    }
    if (rec[16]) {
      h += '<div class="sq-field"><label>Member C - ' + esc(cn(rec[16])) + ' x' + groupCount(rec, 'C') + (isLarge(rec[16]) ? ' large' : '') +
        '</label><select data-grp="C">' + classOptionsHtml(rec[16]) + '</select></div>';
    }
    h += '<div class="sq-field"><label>Add unit</label><div class="sq-add-row"><select id="sq-add-cls">' + classOptionsHtml(0) + '</select><button type="button" id="sq-add" class="btn-secondary">Add</button></div></div>';
    h += '</div>';
    return h;
  }

  function renderDetail(rom) {
    var el = document.getElementById('sq-detail'); if (!el) return;
    var scn = scenarioById(sel.scenarioId);
    if (!scn || sel.edatId == null) {
      el.innerHTML = '<p class="sq-empty">Select a squad.</p>';
      return;
    }
    var van = vanillaRec(scn, sel.edatId), k = key(scn.id, sel.edatId), over = rom.squadOverrides[k], rec = over || van;
    var selected = null;
    for (var si = 0; si < scn.squads.length; si++) if (scn.squads[si].e === sel.edatId) { selected = scn.squads[si]; break; }
    var bn = selected ? bossName(selected) : '';
    var gate = scn.gate || scn.id;
    var trace = selected ? squadTraceText(selected) : '';
    var headChips = '<span class="sq-chip">key ' + scn.id + '</span><span class="sq-chip">gate 0x' + gate.toString(16) + '</span>';
    if (scn.traceMethod) headChips += '<span class="sq-chip">' + esc(scn.traceMethod) + '</span>';
    var html = '<div class="sq-detail-head">' +
      '<div><div class="sq-head">' + esc(scn.name) + ' / edat ' + sel.edatId + '</div>' +
      (scn.wikiLabel ? '<div class="sq-sub">' + esc(scn.wikiLabel) + '</div>' : '') +
      (scn.identityNote ? '<div class="sq-sub">' + esc(scn.identityNote) + '</div>' : '') +
      '<div class="sq-sub">' + (bn ? esc(bn) + ' - ' : '') + 'vanilla: ' + esc(compLabel(van)) + ' - ' + memberCount(van) + ' units' + (trace ? ' - ' + esc(trace) : '') + '</div>' +
      '</div>' +
      '<span class="sq-row-meta">' + headChips + '</span></div>';
    html += '<label class="sq-toggle"><input type="checkbox" id="sq-override"' + (over ? ' checked' : '') + '> <span>Override in this scenario</span></label>';
    if (over) {
      html += '<div class="sq-editor-grid">' + gridHtml(rec, true) + pickersHtml(rec) + '</div>';
      if (ui.notice) html += '<div class="sq-notice">' + esc(ui.notice) + '</div>';
      var n = memberCount(rec), adjOk = adjacencyOk(rec), hasLeader = !!rec[0] && !!rec[6];
      var ok = hasLeader && adjOk;
      var status = ok ? 'Valid' : (!hasLeader ? 'Leader class required' : 'Large-unit spacing conflict');
      html += '<div class="sq-foot"><span class="' + (ok ? 'sq-status' : 'sq-warn') + '">' + status + ' - ' + n + ' units</span>' +
        '<span>' + esc(scenarioTraceText(scn)) + '</span></div>';
      html += '<div class="sq-action-row"><button type="button" id="sq-reset" class="btn-secondary">Reset to vanilla</button></div>';
    } else {
      html += '<div class="sq-editor-grid">' + gridHtml(van, false) +
        '<div class="sq-readout"><div class="sq-section-label">Vanilla squad</div>' +
        '<div class="sq-sub">' + esc(compLabel(van)) + '</div>' +
        '<div class="sq-sub">' + esc(scenarioTraceText(scn)) + '</div></div></div>';
    }
    el.innerHTML = html;
    var toggle = el.querySelector('#sq-override');
    if (toggle) toggle.onchange = function () {
      ui.notice = '';
      if (this.checked) rom.squadOverrides[k] = van.slice(0); else delete rom.squadOverrides[k];
      if (OB64._squadChanged) OB64._squadChanged();
      renderSquads(document.getElementById('panel-squads'));
    };
    if (over) wireDetail(rom, scn, rec, k);
  }

  function wireDetail(rom, scn, rec, k) {
    var el = document.getElementById('sq-detail');
    el.querySelectorAll('select[data-grp]').forEach(function (s) {
      s.onchange = function () {
        var v = parseInt(this.value), grp = this.dataset.grp;
        if (grp === 'L') {
          if (!v) { ui.notice = 'Leader class is required.'; renderDetail(rom); return; }
          rec[0] = v;
        } else {
          setGroupClass(rec, grp, v);
        }
        commit(rom, scn);
      };
    });
    var addBtn = el.querySelector('#sq-add');
    if (addBtn) addBtn.onclick = function () {
      var v = parseInt(el.querySelector('#sq-add-cls').value); if (!v) return;
      var err = addUnitOfClass(rec, v);
      if (err) { ui.notice = err; renderDetail(rom); return; }
      commit(rom, scn);
    };
    el.querySelectorAll('.sq-remove').forEach(function (x) { x.onclick = function (e) { e.stopPropagation(); removeCell(rec, parseInt(this.dataset.cell)); commit(rom, scn); }; });
    var rb = el.querySelector('#sq-reset'); if (rb) rb.onclick = function () { rom.squadOverrides[k] = vanillaRec(scn, sel.edatId).slice(0); commit(rom, scn); };
    el.querySelectorAll('.sq-cell').forEach(function (c) {
      c.ondragstart = function (e) { e.dataTransfer.setData('text/plain', this.dataset.cell); e.dataTransfer.effectAllowed = 'move'; };
      c.ondragover = function (e) { e.preventDefault(); this.style.outline = '2px solid var(--ob-gold-bright)'; };
      c.ondragleave = function () { this.style.outline = ''; };
      c.ondrop = function (e) { e.preventDefault(); this.style.outline = ''; var from = parseInt(e.dataTransfer.getData('text/plain')), to = parseInt(this.dataset.cell); if (!isNaN(from) && !isNaN(to)) { moveUnit(rec, from, to); commit(rom, scn); } };
    });
  }

  function commit(rom, scn) {
    ui.notice = '';
    if (OB64._squadChanged) OB64._squadChanged();
    renderDetail(rom); renderList(rom);
  }

  function collectSquadOverrides(rom) {
    var out = [];
    if (!rom.squadOverrides) return out;
    for (var k in rom.squadOverrides) {
      var sid = parseInt(k.split(':')[0]), eid = parseInt(k.split(':')[1]);
      var scn = scenarioById(sid); if (!scn) continue;
      var van = vanillaRec(scn, eid); if (!van) continue;
      out.push({ gateId: scn.gate || scn.id, original: van, record: rom.squadOverrides[k] });
    }
    return out;
  }
  function countUnmapped(rom) { return 0; }

  OB64.renderSquads = renderSquads;
  OB64.collectSquadOverrides = collectSquadOverrides;
  OB64.squadCountUnmapped = countUnmapped;
})(typeof OB64 !== 'undefined' ? OB64 : (window.OB64 = window.OB64 || {}));
