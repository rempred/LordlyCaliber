// Map-first Scenario tab.
window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var STYLE_ID = 'scenario-style';
  var ui = {
    selectedKey: 1,
    selectedPoint: null,
    selectedSite: null,
    selectedTrigger: null,
    selectedTreasure: null,
    selectedNode: null,
    search: '',
    // 'auto' = art on site-fitted registrations, schematic on provisional ones.
    viewMode: 'auto',
    zoom: 0.45,
    advanced: false,
    layers: { squads: true, sites: true, routes: true, triggers: true, treasure: true },
    gateText: '',
  };
  // Active one-shot map tool ('rect' | 'pick' | 'add-squad' | null). While set, background
  // clicks must not clear the selection; tools clear it a tick AFTER their final click so the
  // trailing click event cannot deselect.
  var mapTool = null;
  var RELOC_TAIL_START = 0x027C0000;
  var RELOC_HOOK_ROM = 0x0001BFE4;
  var RELOC_HOOK_DELAY_ROM = 0x0001BFE8;
  var RELOC_CAVE_ROM = 0x000318DC;
  var RELOC_CAVE_SIZE = 0x320;
  var RELOC_BOOT_RAM_BASE = 0x8006FC00;
  var RELOC_STUB_BYTES = 0x80;
  var RELOC_ENTRY_BYTES = 8;
  // Site-snap radius in SCREEN pixels (converted to image px per current zoom at each use).
  // A fixed image-pixel radius shrinks with zoom-to-fit (~14 screen px), making it easy to
  // miss a town and silently write near-town coordinate bytes instead of the site selector.
  var SNAP_SCREEN_PX = 30;

  function releaseMapTool() {
    setTimeout(function() { mapTool = null; }, 0);
  }

  // Map-tool captures consume the POINTERDOWN, but the browser still synthesizes a CLICK from
  // the same press - it lands on whatever marker sits under the cursor and would switch the
  // sidebar selection away from the squad being edited. Eat exactly one follow-up click in
  // the capture phase; the timeout drops the guard if no click materializes.
  function eatNextMapClick(inner) {
    var eat = function(ev) {
      ev.preventDefault();
      ev.stopPropagation();
      inner.removeEventListener('click', eat, true);
    };
    inner.addEventListener('click', eat, true);
    setTimeout(function() { inner.removeEventListener('click', eat, true); }, 400);
  }

  function clearSelection() {
    ui.selectedPoint = null;
    ui.selectedSite = null;
    ui.selectedTrigger = null;
    ui.selectedTreasure = null;
    ui.selectedNode = null;
    renderScenarioTab(document.getElementById('panel-scenario'));
  }

  // Select a Section-2 node (mirrors selectTrigger): opens the node editor, which lists every
  // squad that starts on or routes through it. Node detail sits LAST in the dispatcher, so any
  // squad/site/trigger selection takes precedence and this only shows when nothing else is picked.
  function selectNode(nodeId) {
    ui.selectedNode = ui.selectedNode === nodeId ? null : nodeId;
    ui.selectedPoint = null;
    ui.selectedSite = null;
    ui.selectedTrigger = null;
    ui.selectedTreasure = null;
    renderScenarioTab(document.getElementById('panel-scenario'));
  }

  // Behavior-builder form state, persisted across the full re-renders every commit triggers.
  // Without it, applying or live re-gating wipes in-progress selections and the confirmation
  // message, which reads as "didn't save". Reset when a different squad is selected.
  var builder = null;

  function builderFor(key, rowIndex) {
    if (!builder || builder.key !== key || builder.rowIndex !== rowIndex) {
      // owned = Section 2/3 records this builder allocated for this row; live re-applies
      // REWRITE them in place instead of allocating fresh ones, so exploring templates
      // does not erode the 16-node/16-extra caps.
      builder = { key: key, rowIndex: rowIndex, template: '', trigger: null, threshold: null, dest: null, msg: '', msgOk: true, owned: { dest: null, gate: null, extra: null } };
    }
    return builder;
  }

  function resetBuilderState() {
    builder = null;
  }

  function esc(value) {
    return String(value == null ? '' : value).replace(/[&<>"]/g, function(c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
    });
  }

  function num(value, fallback) {
    value = Number(value);
    return Number.isFinite(value) ? value : fallback;
  }

  function clamp(value, lo, hi) {
    value = num(value, lo);
    return Math.max(lo, Math.min(hi, value));
  }

  function injectStyle() {
    if (document.getElementById(STYLE_ID)) return;
    var css = [
      '#panel-scenario{--sc-line:rgba(62,45,25,.28);--sc-panel:#efe0bd;--sc-soft:rgba(255,255,255,.18);--sc-red:#b7372f;--sc-blue:#2d6fbc;--sc-green:#2f8f4e}',
      '#panel-scenario .sc-page{max-width:1500px;margin:0 auto;color:var(--ob-ink)}',
      '#panel-scenario .sc-titlebar{display:flex;align-items:flex-end;justify-content:space-between;gap:12px;margin:0 0 12px}',
      '#panel-scenario h2{margin:0;color:var(--ob-gold-bright);font-size:var(--ob-text-lg);line-height:1.1}',
      '#panel-scenario .sc-actions{display:flex;gap:8px;align-items:center;flex-wrap:wrap;justify-content:flex-end}',
      '#panel-scenario .sc-actions button,#panel-scenario .sc-actions label{height:32px;display:inline-flex;align-items:center;border-radius:5px;font-size:var(--ob-text-sm);font-weight:700}',
      // Parchment/stone button theme (mirrors .header-buttons in style.css) for every scenario
      // control that previously fell through to browser-default white.
      '#panel-scenario .sc-actions button,#panel-scenario .sc-actions label,#panel-scenario .sc-inline-btn{font-family:var(--ob-display);letter-spacing:.6px;text-transform:uppercase;padding:0 12px;color:var(--ob-ink);background:linear-gradient(180deg,var(--ob-parchment) 0%,var(--ob-parchment-2) 55%,var(--ob-parchment-3) 100%);border:1px solid var(--ob-wood-darkest);cursor:pointer;text-shadow:0 1px 0 rgba(255,240,200,.45);box-shadow:inset 0 1px 0 rgba(255,245,210,.65),inset 0 -2px 0 rgba(122,81,32,.45),0 2px 3px rgba(0,0,0,.35);transition:filter .12s,transform .04s}',
      '#panel-scenario .sc-inline-btn{height:28px;border-radius:5px;font-size:var(--ob-text-xs);font-weight:800;display:inline-flex;align-items:center}',
      '#panel-scenario .sc-actions button:hover,#panel-scenario .sc-actions label:hover,#panel-scenario .sc-inline-btn:hover{filter:brightness(1.08)}',
      '#panel-scenario .sc-actions button:active,#panel-scenario .sc-actions label:active,#panel-scenario .sc-inline-btn:active{transform:translateY(1px);box-shadow:inset 0 2px 3px rgba(80,50,20,.55),0 1px 2px rgba(0,0,0,.35)}',
      '#panel-scenario .sc-danger{color:var(--ob-wax-red);border-color:var(--ob-wax-red);background:linear-gradient(180deg,var(--ob-parchment-2) 0%,var(--ob-parchment-3) 100%)}',
      '#panel-scenario .sc-danger:hover{filter:brightness(1.05);background:rgba(152,32,24,.12)}',
      '#panel-scenario .sc-actions input[type=text]{height:32px;width:210px;border:1px solid var(--ob-parchment-edge);border-radius:5px;background:#f7ebce;color:var(--ob-ink);padding:0 8px}',
      '#panel-scenario .sc-gate{font-size:var(--ob-text-sm);color:var(--ob-ink-soft);min-height:18px}',
      '#panel-scenario .sc-layout{display:grid;grid-template-columns:280px minmax(460px,1fr) 500px;gap:12px;align-items:start}',
      // Embedded Squads comp editor: stack formation grid over pickers so the full modal fits the sidebar.
      '#sc-comp-host .sq-editor-grid{display:grid;grid-template-columns:1fr !important;gap:10px}',
      '#sc-comp-host .sq-pick{grid-template-columns:1fr !important}',
      '#panel-scenario .sc-trigger-row{display:block;width:100%;text-align:left;border:1px solid var(--sc-line);border-radius:5px;background:rgba(255,255,255,.14);color:var(--ob-ink);padding:6px 8px;margin:4px 0;cursor:pointer;font-size:var(--ob-text-sm)}',
      '#panel-scenario .sc-trigger-row .sc-sub{display:block;margin-top:2px}',
      '#panel-scenario .sc-trigger-row:hover{background:rgba(104,74,36,.14)}',
      '#panel-scenario .sc-trigger-row.on{outline:2px solid var(--ob-gold-bright);background:rgba(245,210,98,.18)}',
      // Draw-interaction feedback: rubber-band rect, destination pick ghost, drop snap ring.
      '#panel-scenario .sc-rubber-band{border:2px dashed rgba(245,210,98,.95);background:rgba(245,210,98,.14);border-radius:3px}',
      '#panel-scenario .sc-pick-ghost{width:14px;height:14px;margin:-7px 0 0 -7px;border-radius:50%;border:2px solid rgba(245,210,98,.95);background:rgba(245,210,98,.35);box-shadow:0 0 0 3px rgba(0,0,0,.25)}',
      '#panel-scenario .sc-pick-ghost.set{background:rgba(47,143,78,.85);border-color:#fff;transition:transform .3s;transform:scale(1.8)}',
      '#panel-scenario .sc-snap-ring{width:44px;height:44px;margin:-22px 0 0 -22px;border-radius:50%;border:3px dashed rgba(245,210,98,.95);animation:sc-snap-pulse .8s infinite}',
      '@keyframes sc-snap-pulse{0%{transform:scale(1);opacity:1}50%{transform:scale(1.12);opacity:.7}100%{transform:scale(1);opacity:1}}',
      '#sc-comp-host .sq-pick select,#sc-comp-host .sq-field select{max-width:100% !important;width:100%}',
      // Scenario embed: stack the head so the metadata chips sit ABOVE the squad title and the
      // descriptive body runs the full panel width (vanilla Squads tab lays them side-by-side).
      '#sc-comp-host .sq-detail-head{display:flex;flex-direction:column-reverse;align-items:stretch;gap:6px;padding:0 0 6px;margin:0 0 6px}',
      '#sc-comp-host .sq-row-meta{display:flex;flex-wrap:wrap;gap:4px;justify-content:flex-start}',
      '#sc-comp-host .sq-foot{flex-wrap:wrap}',
      '#panel-scenario .sc-list,#panel-scenario .sc-map-panel,#panel-scenario .sc-detail{background:var(--ob-parchment);border:1px solid var(--ob-parchment-edge);border-radius:6px;box-shadow:var(--ob-shadow-sm)}',
      '#panel-scenario .sc-list{max-height:calc(100vh - 205px);min-height:620px;overflow:auto;padding:10px}',
      '#panel-scenario .sc-list-tools{position:sticky;top:0;z-index:3;background:var(--ob-parchment);border-bottom:1px solid var(--sc-line);padding-bottom:8px;margin-bottom:8px}',
      '#panel-scenario .sc-list-tools input{width:100%;height:32px;border:1px solid var(--ob-parchment-edge);border-radius:5px;background:#f7ebce;color:var(--ob-ink);padding:0 8px}',
      '#panel-scenario .sc-group{font-size:var(--ob-text-xs);font-weight:800;text-transform:uppercase;color:var(--ob-ink-soft);letter-spacing:.4px;margin:10px 2px 4px}',
      '#panel-scenario .sc-key{width:100%;border:1px solid transparent;background:transparent;color:var(--ob-ink);border-radius:5px;padding:7px 8px;text-align:left;display:grid;grid-template-columns:minmax(0,1fr) auto;gap:8px;cursor:pointer}',
      '#panel-scenario .sc-key:hover{background:rgba(104,74,36,.12);border-color:var(--sc-line)}',
      '#panel-scenario .sc-key.on{background:var(--ob-wood-lo);color:var(--ob-parchment);border-color:var(--ob-wood-hi)}',
      '#panel-scenario .sc-key-name{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:var(--ob-text-sm);font-weight:700}',
      '#panel-scenario .sc-key-sub{grid-column:1/-1;color:inherit;opacity:.72;font-size:var(--ob-text-xs);line-height:1.25;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}',
      '#panel-scenario .sc-chip{display:inline-flex;align-items:center;min-height:18px;border-radius:4px;padding:1px 5px;font-size:var(--ob-text-xs);font-weight:800;line-height:1;text-transform:uppercase;background:var(--ob-parchment-dark);color:var(--ob-ink);white-space:nowrap}',
      '#panel-scenario .sc-key.on .sc-chip{background:rgba(245,230,200,.18);color:var(--ob-parchment)}',
      '#panel-scenario .sc-key-dev:not(.on){opacity:.6}',
      '#panel-scenario .sc-key-dev:not(.on) .sc-key-name{font-style:italic;font-weight:600}',
      '#panel-scenario .sc-map-panel{padding:10px;min-width:0}',
      '#panel-scenario .sc-map-head{display:flex;justify-content:space-between;align-items:center;gap:10px;margin:0 0 8px}',
      '#panel-scenario .sc-map-title{font-size:var(--ob-text-md);font-weight:800;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}',
      '#panel-scenario .sc-map-tools{display:flex;gap:6px;align-items:center;flex-wrap:wrap;justify-content:flex-end}',
      '#panel-scenario .sc-map-tools select,#panel-scenario .sc-map-tools input{height:28px;border:1px solid var(--ob-parchment-edge);border-radius:5px;background:#f7ebce;color:var(--ob-ink);font-size:var(--ob-text-sm)}',
      '#panel-scenario .sc-layer-toggles{display:flex;gap:7px;align-items:center;flex-wrap:wrap;border-top:1px solid var(--sc-line);padding-top:8px;margin-top:8px}',
      '#panel-scenario .sc-route-legend{display:flex;gap:14px;align-items:center;flex-wrap:wrap;font-size:var(--ob-text-xs);color:var(--ob-ink-soft);padding:6px 2px 0;margin-top:6px}',
      '#panel-scenario .sc-route-legend .sc-leg-t{font-weight:800;color:var(--ob-ink)}',
      '#panel-scenario .sc-route-legend .sc-leg{display:inline-flex;align-items:center;gap:5px}',
      '#panel-scenario .sc-leg-dot{display:inline-block;width:9px;height:9px;border-radius:2px}',
      '#panel-scenario .sc-squad-chip{display:grid;grid-template-columns:30px minmax(0,1fr);gap:8px;align-items:center;width:100%;text-align:left;border:1px solid var(--sc-line);border-radius:5px;background:rgba(255,255,255,.14);color:var(--ob-ink);padding:4px 8px;margin:4px 0;cursor:pointer;font-size:var(--ob-text-sm)}',
      '#panel-scenario .sc-squad-chip:hover{background:rgba(104,74,36,.14)}',
      '#panel-scenario .sc-squad-chip:active{transform:translateY(1px)}',
      '#panel-scenario .sc-squad-chip img,#panel-scenario .sc-squad-chip .sc-chip-noicon{width:26px;height:26px;object-fit:contain;image-rendering:pixelated;border-radius:50%;background:#1d1a16;display:inline-block}',
      '#panel-scenario .sc-squad-chip strong{font-weight:800}',
      '#panel-scenario .sc-squad-chip .sc-chip-sub{display:block;color:var(--ob-ink-soft);font-size:var(--ob-text-xs)}',
      '#panel-scenario .sc-layer-toggles label{font-size:var(--ob-text-sm);display:flex;gap:4px;align-items:center}',
      '#panel-scenario .sc-map-scroll{height:620px;overflow:auto;border:1px solid var(--sc-line);border-radius:5px;background:#32281d;position:relative}',
      '#panel-scenario .sc-map-inner{position:relative;transform-origin:0 0;min-width:720px;min-height:520px;background:#243128;overflow:hidden}',
      '#panel-scenario .sc-map-img{position:absolute;left:0;top:0;width:100%;height:100%;object-fit:contain;display:block}',
      '#panel-scenario .sc-schematic{position:absolute;left:0;top:0;width:100%;height:100%;background:#2f3b32}',
      '#panel-scenario .sc-bounds{position:absolute;border:2px dashed rgba(245,230,200,.65);background:rgba(0,0,0,.08);pointer-events:none}',
      '#panel-scenario .sc-layer-svg{position:absolute;left:0;top:0;width:100%;height:100%;overflow:visible;pointer-events:none}',
      '#panel-scenario .sc-marker{position:absolute;border:0;background:transparent;padding:0;transform:translate(-50%,-50%);cursor:pointer;z-index:10}',
      '#panel-scenario .sc-treasure-marker{width:32px;height:32px;border-radius:50%;background:rgba(42,30,18,.86);border:2px solid rgba(245,210,98,.92);display:flex;align-items:center;justify-content:center;box-shadow:0 2px 5px rgba(0,0,0,.45),0 0 0 2px rgba(60,38,18,.55);overflow:visible}',
      '#panel-scenario .sc-treasure-marker img{width:24px;height:24px;object-fit:contain;image-rendering:pixelated;filter:drop-shadow(0 1px 1px rgba(0,0,0,.7))}',
      '#panel-scenario .sc-treasure-marker.on{outline:3px solid var(--ob-gold-bright);outline-offset:2px;background:rgba(92,58,24,.95);z-index:28}',
      '#panel-scenario .sc-treasure-marker.sc-add-ghost{opacity:.72;pointer-events:none}',
      '#panel-scenario .sc-treasure-row{display:grid;grid-template-columns:28px minmax(0,1fr) auto;gap:8px;align-items:center;border:1px solid var(--sc-line);border-radius:5px;background:rgba(255,255,255,.14);padding:5px 8px;margin:4px 0;text-align:left;color:var(--ob-ink);cursor:pointer;font-size:var(--ob-text-sm)}',
      '#panel-scenario .sc-treasure-row:hover{background:rgba(104,74,36,.14)}',
      '#panel-scenario .sc-treasure-row.on{outline:2px solid var(--ob-gold-bright);background:rgba(245,210,98,.18)}',
      '#panel-scenario .sc-treasure-row img,.sc-treasure-current img{width:24px;height:24px;object-fit:contain;image-rendering:pixelated}',
      '#panel-scenario .sc-treasure-current{display:grid;grid-template-columns:34px minmax(0,1fr);gap:8px;align-items:center}',
      '#panel-scenario .sc-squad-marker{width:38px;height:38px;border-radius:50%;background:#1d1a16;box-shadow:0 2px 6px rgba(0,0,0,.45)}',
      '#panel-scenario .sc-squad-marker img{width:30px;height:30px;object-fit:contain;image-rendering:pixelated;border-radius:50%;margin:4px}',
      '#panel-scenario .sc-squad-marker.enemy{outline:3px solid var(--sc-red)}',
      '#panel-scenario .sc-squad-marker.allied{outline:3px solid var(--sc-blue)}',
      '#panel-scenario .sc-squad-marker.neutral{outline:3px solid var(--sc-green)}',
      '#panel-scenario .sc-squad-marker.dormant{opacity:.54;filter:grayscale(.45)}',
      '#panel-scenario .sc-squad-marker.on{box-shadow:0 0 0 4px rgba(245,210,98,.7),0 2px 6px rgba(0,0,0,.45)}',
      '#panel-scenario .sc-squad-marker.added{outline:3px dashed var(--ob-gold-bright)}',
      '#panel-scenario .sc-add-ghost{opacity:.7;transform:translate(-50%,-50%);outline:3px dashed var(--ob-gold-bright)}',
      '#panel-scenario .sc-back{margin:0 0 8px}',
      '#panel-scenario .sc-badge{position:absolute;right:-5px;bottom:-5px;min-width:16px;height:16px;border-radius:8px;background:var(--ob-gold);color:#2a1b0c;font-size:var(--ob-text-xs);font-weight:900;line-height:16px;text-align:center;border:1px solid rgba(0,0,0,.35)}',
      // Town = a ring CENTERED on the site anchor, larger than and BELOW the squad icon, so a
      // site-placed squad stands inside its town's allegiance ring (old 16px offset read as
      // "dots are not on towns").
      '#panel-scenario .sc-site-marker{width:48px;height:48px;border-radius:50%;background:transparent;border:3px solid var(--sc-green);box-shadow:0 1px 4px rgba(0,0,0,.35);z-index:8;transform:translate(-50%,-50%)}',
      '#panel-scenario .sc-site-marker:hover{background:rgba(245,230,200,.2)}',
      '#panel-scenario .sc-site-marker.enemy{border-color:var(--sc-red)}',
      '#panel-scenario .sc-site-marker.allied{border-color:var(--sc-blue)}',
      '#panel-scenario .sc-site-marker.neutral{border-color:var(--sc-green)}',
      '#panel-scenario .sc-site-marker.on{box-shadow:0 0 0 4px rgba(245,210,98,.7),0 1px 4px rgba(0,0,0,.5)}',
      '#panel-scenario .sc-detail{padding:12px;min-height:620px}',
      '#panel-scenario .sc-detail-head{border-bottom:1px solid var(--sc-line);padding-bottom:9px;margin-bottom:10px}',
      '#panel-scenario .sc-head-title{font-size:var(--ob-text-md);font-weight:800;line-height:1.2}',
      '#panel-scenario .sc-sub{font-size:var(--ob-text-sm);color:var(--ob-ink-soft);line-height:1.35;margin-top:3px}',
      '#panel-scenario .sc-meter-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:8px 0 10px}',
      '#panel-scenario .sc-meter{border:1px solid var(--sc-line);border-radius:5px;padding:6px;background:rgba(255,255,255,.14)}',
      '#panel-scenario .sc-meter strong{display:block;font-size:var(--ob-text-md);line-height:1}',
      '#panel-scenario .sc-meter span{font-size:var(--ob-text-xs);text-transform:uppercase;color:var(--ob-ink-soft);font-weight:800}',
      '#panel-scenario .sc-section{border-top:1px solid var(--sc-line);padding-top:10px;margin-top:10px}',
      '#panel-scenario .sc-label{display:block;font-size:var(--ob-text-xs);font-weight:900;text-transform:uppercase;color:var(--ob-ink-soft);letter-spacing:.35px;margin-bottom:4px}',
      '#panel-scenario .sc-form-row{display:grid;grid-template-columns:120px minmax(0,1fr);gap:8px;align-items:center;margin:6px 0}',
      '#panel-scenario .sc-form-row select,#panel-scenario .sc-form-row input{height:30px;min-width:0;border:1px solid var(--ob-parchment-edge);border-radius:5px;background:#f7ebce;color:var(--ob-ink);padding:0 7px}',
      '#panel-scenario .sc-mini-grid{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:6px}',
      '#panel-scenario .sc-unit{min-width:0;text-align:center;border:1px solid var(--sc-line);border-radius:5px;padding:5px;background:rgba(255,255,255,.14)}',
      '#panel-scenario .sc-unit img{width:42px;height:36px;object-fit:contain;image-rendering:pixelated;display:block;margin:0 auto 2px}',
      '#panel-scenario .sc-unit span{display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:var(--ob-text-xs);font-weight:800}',
      '#panel-scenario .sc-node-list{display:grid;gap:6px;max-height:220px;overflow:auto}',
      '#panel-scenario .sc-node{border:1px solid var(--sc-line);border-radius:5px;background:rgba(255,255,255,.14);padding:6px;font-size:var(--ob-text-sm);line-height:1.35}',
      '#panel-scenario .sc-raw-grid{display:grid;grid-template-columns:repeat(6,1fr);gap:5px}',
      '#panel-scenario .sc-byte{display:grid;grid-template-columns:1fr;gap:2px;font-size:var(--ob-text-xs);color:var(--ob-ink-soft);font-weight:800}',
      '#panel-scenario .sc-byte input{width:100%;height:26px;text-align:center;border:1px solid var(--ob-parchment-edge);border-radius:4px;background:#f7ebce;color:var(--ob-ink)}',
      '#panel-scenario .sc-warning{border:1px solid var(--ob-wax-red);background:rgba(152,32,24,.10);color:var(--ob-wax-red);border-radius:5px;padding:7px 8px;font-size:var(--ob-text-sm);line-height:1.35;margin-top:8px}',
      '#panel-scenario .sc-ok{border:1px solid #2f8f4e;background:rgba(47,143,78,.10);color:#185c34;border-radius:5px;padding:7px 8px;font-size:var(--ob-text-sm);line-height:1.35;margin-top:8px}',
      '#panel-scenario .sc-table{width:100%;font-size:var(--ob-text-sm)}',
      '#panel-scenario .sc-table th,#panel-scenario .sc-table td{padding:4px 5px}',
      // Squad roster under the map: one row per Section 1 squad, click-to-select.
      '#panel-scenario .sc-roster{border-top:1px solid var(--sc-line);margin-top:8px;padding-top:8px}',
      '#panel-scenario .sc-roster-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}',
      '#panel-scenario .sc-roster-list{display:grid;gap:4px;max-height:300px;overflow:auto}',
      '#panel-scenario .sc-roster-row{display:grid;grid-template-columns:34px minmax(120px,1fr) minmax(140px,1.4fr) auto auto;gap:8px;align-items:center;border:1px solid var(--sc-line);border-radius:5px;background:rgba(255,255,255,.14);padding:4px 8px;font-size:var(--ob-text-sm);cursor:pointer;text-align:left}',
      '#panel-scenario .sc-roster-row:hover{background:rgba(104,74,36,.14)}',
      '#panel-scenario .sc-roster-row.on{outline:2px solid var(--ob-gold-bright);background:rgba(245,210,98,.18)}',
      '#panel-scenario .sc-roster-row img{width:28px;height:28px;object-fit:contain;image-rendering:pixelated;border-radius:50%;background:#1d1a16}',
      '#panel-scenario .sc-roster-row .sc-sub{margin-top:0}',
      '#panel-scenario .sc-roster-row .sc-chips{display:flex;gap:4px;justify-content:flex-end}',
      '@media (max-width:1180px){#panel-scenario .sc-layout{grid-template-columns:1fr}#panel-scenario .sc-list{min-height:240px;max-height:320px}#panel-scenario .sc-detail{min-height:360px}#panel-scenario .sc-map-scroll{height:520px}}'
    ].join('');
    var style = document.createElement('style');
    style.id = STYLE_ID;
    style.textContent = css;
    document.head.appendChild(style);
  }

  function dataScenarios() {
    return (OB64.SCENARIO_ESET_DATA && OB64.SCENARIO_ESET_DATA.scenarios) || [];
  }

  function calibrationScenarios() {
    return (OB64.SCENARIO_MAP_CALIBRATION && OB64.SCENARIO_MAP_CALIBRATION.scenarios) || [];
  }

  function scenarioData(runtimeKey) {
    return dataScenarios().filter(function(s) { return s.runtimeKey === runtimeKey; })[0] || null;
  }

  function calibrationData(runtimeKey) {
    return calibrationScenarios().filter(function(s) { return s.runtimeKey === runtimeKey; })[0] || null;
  }

  function squadScenario(runtimeKey) {
    var scenarios = (OB64.SQUAD_DATA && OB64.SQUAD_DATA.scenarios) || [];
    return scenarios.filter(function(s) { return s.id === runtimeKey; })[0] || null;
  }

  function treasureData() {
    return OB64.SCENARIO_TREASURE_DATA || {};
  }

  function treasureArchiveForKey(runtimeKey) {
    var map = treasureData().runtimeKeyToArchive || {};
    return map[runtimeKey] || map[String(runtimeKey)] || null;
  }

  function treasureArchiveEntry(archive) {
    var archives = treasureData().archives || {};
    return archives[archive] || archives[String(archive)] || null;
  }

  function cloneTreasureRecord(record, archive, index) {
    return {
      index: index,
      archive: archive,
      globalId: record.globalId != null ? record.globalId : record.global_id,
      x: record.x & 0xFF,
      y: record.y & 0xFF,
      table: record.table & 0xFF,
      itemId: record.itemId != null ? record.itemId : record.item_id,
      itemName: record.itemName || record.item_name || '',
      itemNamespace: record.itemNamespace || record.item_namespace || '',
      added: !!record.added,
    };
  }

  function serializeTreasureRecords(records) {
    var out = new Uint8Array(1 + records.length * 6);
    out[0] = records.length & 0xFF;
    records.forEach(function(record, i) {
      var off = 1 + i * 6;
      out[off] = record.globalId & 0xFF;
      out[off + 1] = record.x & 0xFF;
      out[off + 2] = record.y & 0xFF;
      out[off + 3] = record.table & 0xFF;
      out[off + 4] = (record.itemId >>> 8) & 0xFF;
      out[off + 5] = record.itemId & 0xFF;
    });
    return out;
  }

  function refreshTreasureRecord(record) {
    record.itemName = treasureItemName(record);
    record.itemNamespace = record.table === 1 ? 'equipment' : (record.table === 2 ? 'special' : 'unknown');
    return record;
  }

  function parseTreasureBytes(bytes, archive, entry) {
    var count = bytes[0] || 0;
    var records = [];
    for (var i = 0; i < count; i++) {
      var off = 1 + i * 6;
      records.push(refreshTreasureRecord({
        index: i,
        archive: archive,
        globalId: bytes[off] || 0,
        x: bytes[off + 1] || 0,
        y: bytes[off + 2] || 0,
        table: bytes[off + 3] || 0,
        itemId: ((bytes[off + 4] || 0) << 8) | (bytes[off + 5] || 0),
        added: false,
      }));
    }
    return {
      archive: archive,
      filename: entry && entry.filename,
      records: records,
    };
  }

  function initTreasureState(state) {
    if (state.treasureArchives) return;
    state.treasureArchives = {};
    state.originalTreasureBytes = {};
    state.modifiedTreasureArchives = {};
    var archives = treasureData().archives || {};
    Object.keys(archives).forEach(function(key) {
      var archive = Number(key);
      var entry = archives[key];
      var raw = entry.rawHex && OB64.scenarioCodec ? OB64.scenarioCodec.compactHexToBytes(entry.rawHex) : serializeTreasureRecords(entry.records || []);
      state.originalTreasureBytes[archive] = raw;
      state.treasureArchives[archive] = {
        archive: archive,
        filename: entry.filename,
        relPath: entry.relPath,
        runtimeKeys: entry.runtimeKeys || [],
        validationStatus: entry.validationStatus || '',
        records: (entry.records || []).map(function(record, i) { return cloneTreasureRecord(record, archive, i); }),
      };
    });
  }

  function defaultImageBase() {
    return localStorage.getItem('ob64_scenario_image_base') || 'resources/maps/vgmaps/';
  }

  function createScenarioState(preserved) {
    preserved = preserved || {};
    var state = {
      models: {},
      originalBytes: {},
      metadata: {},
      sourceRows: {},
      sites: {},
      siteAllegiances: {},
      addedSquads: [],
      modifiedKeys: {},
      settings: preserved.settings || { imageBasePath: defaultImageBase() },
      archiveOriginalSlots: preserved.archiveOriginalSlots || {},
      slotOwnedArchives: preserved.slotOwnedArchives || {},
      relocationOwnedWindows: preserved.relocationOwnedWindows || [],
    };
    dataScenarios().forEach(function(entry) {
      if (entry.missing || !entry.rawHex) return;
      var raw = OB64.scenarioCodec.compactHexToBytes(entry.rawHex);
      var model = OB64.scenarioCodec.parseEset(raw, { sourcePath: entry.relPath || entry.filename });
      state.models[entry.runtimeKey] = model;
      state.originalBytes[entry.runtimeKey] = raw;
      state.metadata[entry.runtimeKey] = entry;
      state.sourceRows[entry.runtimeKey] = entry.sourceRows || [];
      state.sites[entry.runtimeKey] = entry.sites || [];
    });
    initTreasureState(state);
    return state;
  }

  function ensureState(rom) {
    if (!rom.scenarioEditor) {
      rom.scenarioEditor = createScenarioState();
    }
    if (!rom.scenarioEditor.archiveOriginalSlots) rom.scenarioEditor.archiveOriginalSlots = {};
    if (!rom.scenarioEditor.slotOwnedArchives) rom.scenarioEditor.slotOwnedArchives = {};
    if (!rom.scenarioEditor.relocationOwnedWindows) rom.scenarioEditor.relocationOwnedWindows = [];
    initTreasureState(rom.scenarioEditor);
    return rom.scenarioEditor;
  }

  function resetScenarioState(rom) {
    var old = ensureState(rom);
    rom.scenarioEditor = createScenarioState({
      settings: old.settings,
      archiveOriginalSlots: old.archiveOriginalSlots,
      slotOwnedArchives: old.slotOwnedArchives,
      relocationOwnedWindows: old.relocationOwnedWindows,
    });
    return rom.scenarioEditor;
  }

  function modelFor(rom, runtimeKey) {
    var state = ensureState(rom);
    return state.models[runtimeKey] || null;
  }

  function rowMatchesIdentity(row, sourceId, edatZeroBased) {
    return !!row && row.sourceId === sourceId && (row.edatOneBased - 1) === edatZeroBased;
  }

  function rowEdatId(row) {
    if (!row || !row.bytes) return null;
    return ((((row.bytes[1] || 0) << 8) | (row.bytes[2] || 0)) - 1);
  }

  function runtimeRowMatchesLive(row, runtimeRow) {
    if (!row || !runtimeRow) return true;
    var edat = runtimeRow.edat != null ? runtimeRow.edat : (runtimeRow.edatOneBased != null ? runtimeRow.edatOneBased - 1 : null);
    return runtimeRow.sourceId === row.sourceId && edat === row.edatOneBased - 1;
  }

  function rowRuntime(rom, runtimeKey, rowIndex) {
    var rows = ensureState(rom).sourceRows[runtimeKey] || [];
    var runtimeRow = rows.filter(function(row) { return row.section1Row === rowIndex; })[0] || null;
    var model = modelFor(rom, runtimeKey);
    var liveRow = model && model.section1[rowIndex];
    return runtimeRowMatchesLive(liveRow, runtimeRow) ? runtimeRow : null;
  }

  // A squad deploys DORMANT when its start node is kind 2: the loader keeps such rows
  // inactive at placement and the dispatcher wakes them through the node's gate. Computing
  // this from the live model means ambush behaviors authored in the editor (including added
  // squads) shade/badge exactly like vanilla ambushers; the static runtime flag is only a
  // fallback for rows the model cannot resolve.
  function rowIsDormant(rom, runtimeKey, model, rowIndex) {
    var row = model && model.section1[rowIndex];
    if (row) {
      var start = row.bytes[6];
      if (start >= 4 && start <= 0x13) {
        var node = nodeById(model, start);
        if (node) return node.kind === 2;
      }
      return false;
    }
    var runtimeRow = rowRuntime(rom, runtimeKey, rowIndex);
    return runtimeRow ? !!runtimeRow.dormant : false;
  }

  function pointFor(runtimeKey, rowIndex) {
    var cal = calibrationData(runtimeKey);
    if (!cal || !cal.points) return null;
    return cal.points.filter(function(point) { return point.section1Row === rowIndex; })[0] || null;
  }

  function resolvePointForRow(rom, key, rowIndex) {
    var model = modelFor(rom, key);
    var row = model && model.section1[rowIndex];
    if (!row) return null;
    var p = pointFor(key, rowIndex);
    if (p && !rowMatchesIdentity(row, p.sourceId, p.edat)) p = null;
    return p || syntheticPoint(rom, key, rowIndex, row);
  }

  function displayLabel(runtimeKey) {
    var scn = squadScenario(runtimeKey);
    var cal = calibrationData(runtimeKey);
    return (scn && scn.name) || (cal && cal.editorLabel) || ('Runtime Key ' + runtimeKey);
  }

  function siteForSelector(rom, runtimeKey, selector) {
    return (ensureState(rom).sites[runtimeKey] || []).filter(function(s) { return s.selector === selector; })[0] || null;
  }

  // Initial town allegiance comes from each site's OWN scincsv descriptor addend (the stream is
  // selected per key by the scenario resource table). addend & 0x2000 = allied, addend == 0x0000
  // = neutral, otherwise (or descriptor absent) = enemy - most towns start enemy-held. The
  // generator (tools/gen_scenario_eset_data.js) resolves this into site.initialAllegiance.
  function siteAllegiance(rom, runtimeKey, selector) {
    var intent = (ensureState(rom).siteAllegiances[runtimeKey] || {})[selector];
    if (intent === 'enemy' || intent === 'neutral' || intent === 'allied') return intent;
    var site = siteForSelector(rom, runtimeKey, selector);
    if (site && (site.initialAllegiance === 'enemy' || site.initialAllegiance === 'neutral' || site.initialAllegiance === 'allied')) return site.initialAllegiance;
    return 'enemy';
  }

  function setSiteAllegiance(rom, runtimeKey, selector, value) {
    var state = ensureState(rom);
    if (!state.siteAllegiances[runtimeKey]) state.siteAllegiances[runtimeKey] = {};
    state.siteAllegiances[runtimeKey][selector] = value;
    state.modifiedKeys[runtimeKey] = true;
    changed();
  }

  function treasureModelForKey(rom, runtimeKey) {
    var archive = treasureArchiveForKey(runtimeKey);
    if (!archive) return null;
    return ensureState(rom).treasureArchives[archive] || null;
  }

  function treasureItemName(record) {
    if (!record) return '';
    if (record.table === 1) return OB64.itemName ? OB64.itemName(record.itemId) : ('Item ' + record.itemId);
    if (record.table === 2) return OB64.consumableName ? OB64.consumableName(record.itemId) : ('Special ' + record.itemId);
    return 'Unknown table ' + record.table + ' item ' + record.itemId;
  }

  function treasureItemIcon(record) {
    var name = treasureItemName(record);
    if (OB64.itemIconURL) return OB64.itemIconURL(name);
    return 'resources/Item%20Icons/' + encodeURIComponent(name) + '.png';
  }

  function treasureWorldForKey(key, record) {
    return byteToWorld(calibrationData(key), record.x, record.y);
  }

  function treasureSelected(rom, key) {
    if (!ui.selectedTreasure) return null;
    var archive = treasureArchiveForKey(key);
    if (!archive || ui.selectedTreasure.archive !== archive) return null;
    var model = ensureState(rom).treasureArchives[archive];
    if (!model) return null;
    var record = model.records[ui.selectedTreasure.index];
    if (!record) return null;
    return { archive: archive, index: ui.selectedTreasure.index, model: model, record: record };
  }

  function reindexTreasureModel(model) {
    (model.records || []).forEach(function(record, i) {
      record.index = i;
      record.archive = model.archive;
      refreshTreasureRecord(record);
    });
  }

  function treasureArchiveModified(rom, archive) {
    var state = ensureState(rom);
    var model = state.treasureArchives[archive];
    var original = state.originalTreasureBytes[archive];
    if (!model || !original) return false;
    return !OB64.scenarioCodec.equalBytes(serializeTreasureRecords(model.records), original);
  }

  function treasureMinHeaderSize(filename) {
    return 24 + 2 + (1 + String(filename || '').length + 2);
  }

  function treasureMaxRecordsForArchive(rom, archive, filename) {
    var archiveDir = rom && rom.archives && rom.archives[archive];
    if (!archiveDir) return 0;
    var slotSize = (archiveDir.totalHeaderSize || 0) + (archiveDir.compSize || 0);
    var maxPayload = slotSize - treasureMinHeaderSize(filename || ('maizo' + archive + '.bin'));
    return Math.max(0, Math.floor((maxPayload - 1) / 6));
  }

  function commitTreasureEdit(rom, archive, message) {
    var state = ensureState(rom);
    state.modifiedTreasureArchives[archive] = true;
    if (message) ui.gateText = message;
    changed();
    renderScenarioTab(document.getElementById('panel-scenario'));
  }

  function clearOtherSelectionsForTreasure() {
    ui.selectedPoint = null;
    ui.selectedSite = null;
    ui.selectedTrigger = null;
    ui.selectedNode = null;
  }

  function changed() {
    if (OB64._scenarioChanged) OB64._scenarioChanged();
  }

  function modelBytes(model) {
    return OB64.scenarioCodec.serializeEset(model);
  }

  function keyModified(rom, runtimeKey) {
    var state = ensureState(rom);
    var model = state.models[runtimeKey];
    var original = state.originalBytes[runtimeKey];
    if (!model || !original) return false;
    return !OB64.scenarioCodec.equalBytes(modelBytes(model), original);
  }

  // Live archive-fit prediction: values over the fixed slot now route to the grow/relocate
  // lane at export time. The fit flag is still useful in the UI as a note, not a block.
  function archiveFitInfo(rom, key) {
    var meta = ensureState(rom).metadata[key] || scenarioData(key);
    var model = modelFor(rom, key);
    if (!meta || !model || !rom.archives || !rom.archives[meta.archive] || !OB64.lh5Compress || !OB64.buildLHAArchive) return null;
    var arc = rom.archives[meta.archive];
    var slot = (arc.totalHeaderSize || 0) + (arc.compSize || 0);
    if (!slot) return null;
    var raw = modelBytes(model);
    var built = OB64.buildLHAArchive(OB64.lh5Compress(raw), raw, meta.filename || 'eset.bin');
    return { size: built.length - 1, slot: slot, fits: (built.length - 1) <= slot };
  }

  function archiveSlotSize(archiveDir) {
    return (archiveDir.totalHeaderSize || 0) + (archiveDir.compSize || 0);
  }

  function snapshotArchiveSlot(rom, state, archive) {
    if (state.archiveOriginalSlots[archive] || !rom.archives || !rom.archives[archive]) return;
    var dir = rom.archives[archive];
    state.archiveOriginalSlots[archive] = rom.z64.slice(dir.offset, dir.offset + archiveSlotSize(dir));
  }

  function restoreArchiveSlot(rom, state, archive) {
    var bytes = state.archiveOriginalSlots[archive];
    var dir = rom.archives && rom.archives[archive];
    if (!bytes || !dir) return false;
    rom.z64.set(bytes, dir.offset);
    delete state.slotOwnedArchives[archive];
    return true;
  }

  function readU32(z64, off) {
    return ((z64[off] << 24) | (z64[off + 1] << 16) | (z64[off + 2] << 8) | z64[off + 3]) >>> 0;
  }

  function writeU32(z64, off, value) {
    value >>>= 0;
    z64[off] = (value >>> 24) & 0xFF;
    z64[off + 1] = (value >>> 16) & 0xFF;
    z64[off + 2] = (value >>> 8) & 0xFF;
    z64[off + 3] = value & 0xFF;
  }

  function mipsJ(ramAddr) { return (0x08000000 | ((ramAddr >>> 2) & 0x03FFFFFF)) >>> 0; }
  function mipsJal(ramAddr) { return (0x0C000000 | ((ramAddr >>> 2) & 0x03FFFFFF)) >>> 0; }
  function mipsLui(rt, imm) { return ((0x0F << 26) | (rt << 16) | (imm & 0xFFFF)) >>> 0; }
  function mipsOri(rt, rs, imm) { return ((0x0D << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)) >>> 0; }
  function mipsLw(rt, base, off) { return ((0x23 << 26) | (base << 21) | (rt << 16) | (off & 0xFFFF)) >>> 0; }
  function mipsSw(rt, base, off) { return ((0x2B << 26) | (base << 21) | (rt << 16) | (off & 0xFFFF)) >>> 0; }
  function mipsAddiu(rt, rs, imm) { return ((0x09 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)) >>> 0; }
  function mipsAddu(rd, rs, rt) { return ((rs << 21) | (rt << 16) | (rd << 11) | 0x21) >>> 0; }
  function mipsSubu(rd, rs, rt) { return ((rs << 21) | (rt << 16) | (rd << 11) | 0x23) >>> 0; }
  function mipsSltu(rd, rs, rt) { return ((rs << 21) | (rt << 16) | (rd << 11) | 0x2B) >>> 0; }
  function mipsBeq(rs, rt, imm) { return ((0x04 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)) >>> 0; }
  function mipsBne(rs, rt, imm) { return ((0x05 << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)) >>> 0; }

  function relocationStubWords() {
    var caveRam = (RELOC_BOOT_RAM_BASE + RELOC_CAVE_ROM) >>> 0;
    var tableRam = (caveRam + RELOC_STUB_BYTES) >>> 0;
    return [
      mipsLui(25, tableRam >>> 16),            // lui   t9,hi(table)
      mipsOri(25, 25, tableRam & 0xFFFF),      // ori   t9,t9,lo(table)
      mipsLw(24, 25, 0),                       // loop: lw t8,0(t9)
      mipsBeq(24, 0, 9),                       // beq   t8,zero,store
      0x00000000,                              // nop
      mipsBne(2, 24, 4),                       // bne   v0,t8,next
      0x00000000,                              // nop
      mipsLw(2, 25, 4),                        // lw    v0,4(t9)
      mipsJ(caveRam + 0x34),                   // j     store
      0x00000000,                              // nop
      mipsAddiu(25, 25, RELOC_ENTRY_BYTES),    // next: addiu t9,t9,8
      mipsJ(caveRam + 0x08),                   // j     loop
      0x00000000,                              // nop
      mipsSw(2, 4, 0),                         // store: sw v0,0(a0)
      0x03E00008,                              // jr    ra
      0x00000000,
    ];
  }

  function assertRelocationCaveClean(z64) {
    for (var i = 0; i < RELOC_CAVE_SIZE; i++) {
      var b = z64[RELOC_CAVE_ROM + i];
      if (b !== 0x00) throw new Error('Scenario relocation cave is not clean at z64 0x' + (RELOC_CAVE_ROM + i).toString(16).toUpperCase());
    }
  }

  // ESET fetches DMA in 0x200-byte windows on a fixed grid at phase 0x3E (observed live:
  // key1 archive 0x27478C2 <- window 0x274783E, key30 0x2749AC3 <- 0x2749A3E, and key6's
  // 0x2747D90 <- TWO windows 0x2747C3E/0x2747E3E, which refutes any fixed archive-relative
  // delta). The redirect table exact-matches the window-start cart address, so this model
  // must reproduce the loader's computation; only single-window archives may relocate.
  var DMA_WINDOW_SIZE = 0x200;
  var DMA_WINDOW_PHASE = 0x3E;

  function dmaWindowForArchive(arc) {
    var archiveOffset = arc.offset >>> 0;
    var delta = (((archiveOffset - DMA_WINDOW_PHASE) % DMA_WINDOW_SIZE) + DMA_WINDOW_SIZE) % DMA_WINDOW_SIZE;
    var start = (archiveOffset - delta) >>> 0;
    return { start: start, delta: delta };
  }

  function align(n, step) {
    return Math.ceil(n / step) * step;
  }

  function cartAddress(romOffset) {
    return (0x10000000 | (romOffset >>> 0)) >>> 0;
  }

  function relocationExpectedJal() {
    return mipsJal((RELOC_BOOT_RAM_BASE + RELOC_CAVE_ROM) >>> 0);
  }

  function relocationHookState(rom) {
    var hookWord = readU32(rom.z64, RELOC_HOOK_ROM);
    var delayWord = readU32(rom.z64, RELOC_HOOK_DELAY_ROM);
    var expectedJal = relocationExpectedJal();
    return {
      hookWord: hookWord,
      delayWord: delayWord,
      clean: hookWord === 0x00431024 && delayWord === 0xAC820000,
      installed: hookWord === expectedJal && delayWord === 0x00431024,
      expectedJal: expectedJal,
    };
  }

  function hasKnownRelocationOwnership(rom, state) {
    return !!((rom.scenarioRelocations && rom.scenarioRelocations.length) ||
      (state.relocationOwnedWindows && state.relocationOwnedWindows.length));
  }

  function restoreRelocationRedirect(rom) {
    var state = relocationHookState(rom);
    if (state.clean) return false;
    if (!state.installed) {
      throw new Error('Scenario relocation hook site is not clean (0x' + state.hookWord.toString(16).toUpperCase() +
        '/0x' + state.delayWord.toString(16).toUpperCase() + ').');
    }
    writeU32(rom.z64, RELOC_HOOK_ROM, 0x00431024);
    writeU32(rom.z64, RELOC_HOOK_DELAY_ROM, 0xAC820000);
    for (var i = 0; i < RELOC_CAVE_SIZE; i++) rom.z64[RELOC_CAVE_ROM + i] = 0;
    if (OB64.recalcN64CRC) OB64.recalcN64CRC(rom.z64);
    return true;
  }

  function installRelocationRedirect(rom, entries) {
    var z64 = rom.z64;
    var maxEntries = Math.floor((RELOC_CAVE_SIZE - RELOC_STUB_BYTES - RELOC_ENTRY_BYTES) / RELOC_ENTRY_BYTES);
    if (entries.length > maxEntries) throw new Error('Too many relocated scenario archives for the redirect table (' + entries.length + '/' + maxEntries + ').');
    if (!entries.length) return restoreRelocationRedirect(rom);
    var hookState = relocationHookState(rom);
    if (!(hookState.clean || hookState.installed)) {
      throw new Error('Scenario relocation hook site is not clean (0x' + hookState.hookWord.toString(16).toUpperCase() + '/0x' + hookState.delayWord.toString(16).toUpperCase() + ').');
    }
    if (!hookState.installed) assertRelocationCaveClean(z64);

    var words = relocationStubWords();
    for (var i = 0; i < words.length; i++) writeU32(z64, RELOC_CAVE_ROM + i * 4, words[i]);
    var table = RELOC_CAVE_ROM + RELOC_STUB_BYTES;
    entries.forEach(function(entry, idx) {
      var off = table + idx * RELOC_ENTRY_BYTES;
      writeU32(z64, off, cartAddress(entry.originalDmaStart));
      writeU32(z64, off + 4, cartAddress(entry.tailDmaStart));
    });
    writeU32(z64, table + entries.length * RELOC_ENTRY_BYTES, 0);
    writeU32(z64, table + entries.length * RELOC_ENTRY_BYTES + 4, 0);
    writeU32(z64, RELOC_HOOK_ROM, hookState.expectedJal);
    writeU32(z64, RELOC_HOOK_DELAY_ROM, 0x00431024);
    if (OB64.recalcN64CRC) OB64.recalcN64CRC(z64);
    return true;
  }

  function relocationPatchRegions(relocations) {
    var regions = [
      { kind: 'rom', start: RELOC_HOOK_ROM, size: 8, label: 'scenario relocation DMA hook' },
      { kind: 'rom', start: RELOC_CAVE_ROM, size: RELOC_CAVE_SIZE, label: 'scenario relocation cave/table' },
    ];
    (relocations || []).forEach(function(entry, idx) {
      regions.push({
        kind: 'rom',
        start: entry.tailDmaStart,
        size: entry.windowSize,
        label: 'scenario relocation tail window ' + (idx + 1),
      });
    });
    return regions;
  }

  function relocationPatchOwner(relocations) {
    return {
      id: 'scenario-eset-relocation',
      name: 'Scenario ESET Relocation',
      regions: relocationPatchRegions(relocations),
    };
  }

  function planRelocationToTail(rom, arc, builtArchive, tailCursor, opts) {
    opts = opts || {};
    var win = dmaWindowForArchive(arc);
    var archiveSize = opts.fullArchiveLength ? builtArchive.length : builtArchive.length - 1;
    // The proven redirect exact-matches ONE window-start cart address and the loader sizes
    // its fetch from the original resource, so relocation is only safe when the original
    // fetch was a single window AND the rebuilt archive still fits that same window.
    var originalSize = (arc.totalHeaderSize || 0) + (arc.compSize || 0);
    if (win.delta + originalSize > DMA_WINDOW_SIZE) {
      throw new Error('relocation unavailable: the original archive spans multiple DMA windows (0x' +
        win.delta.toString(16).toUpperCase() + ' window prefix + ' + originalSize + 'B > ' +
        DMA_WINDOW_SIZE + 'B); multi-window relocation is not yet proven');
    }
    if (win.delta + archiveSize + 1 > DMA_WINDOW_SIZE) {
      throw new Error('relocation limit: rebuilt archive is ' + archiveSize + 'B but the single DMA window fits ' +
        (DMA_WINDOW_SIZE - win.delta - 1) + 'B after its 0x' + win.delta.toString(16).toUpperCase() +
        ' prefix; reduce content');
    }
    var tailDmaStart = tailCursor;
    var tailArchiveOffset = tailDmaStart + win.delta;
    var total = win.delta + archiveSize;
    var windowSize = align(total + 0x200, 0x200);
    if (tailArchiveOffset + archiveSize > rom.z64.length) throw new Error('Scenario relocation tail write exceeds ROM size.');
    return {
      originalDmaStart: win.start,
      tailDmaStart: tailDmaStart,
      windowSize: windowSize,
      tailArchiveOffset: tailArchiveOffset,
      nextTailCursor: align(tailDmaStart + windowSize, 0x10),
      _sourceWindowStart: win.start,
      _sourceWindowDelta: win.delta,
      _archiveSize: archiveSize,
    };
  }

  function tailOffsetOwned(off, ownedWindows) {
    for (var i = 0; i < (ownedWindows || []).length; i++) {
      var w = ownedWindows[i];
      if (off >= w.tailDmaStart && off < w.tailDmaStart + w.windowSize) return true;
    }
    return false;
  }

  function assertRelocationTailFree(rom, moved, ownedWindows) {
    for (var i = 0; i < moved.windowSize; i++) {
      var off = moved.tailDmaStart + i;
      if (off >= rom.z64.length) break;
      if (tailOffsetOwned(off, ownedWindows)) continue;
      if (rom.z64[off] !== 0xFF && rom.z64[off] !== 0x00) throw new Error('Scenario relocation tail is occupied at z64 0x' + off.toString(16).toUpperCase());
    }
  }

  function resetOwnedRelocationWindows(rom, state) {
    (state.relocationOwnedWindows || []).forEach(function(w) {
      if (!w.originalBytes) return;
      rom.z64.set(w.originalBytes, w.tailDmaStart);
    });
  }

  function snapshotRelocationWindow(rom, state, moved) {
    var found = null;
    (state.relocationOwnedWindows || []).forEach(function(w) {
      if (!found && w.tailDmaStart === moved.tailDmaStart && w.windowSize === moved.windowSize) found = w;
    });
    if (found && found.originalBytes) return found;
    return {
      tailDmaStart: moved.tailDmaStart,
      windowSize: moved.windowSize,
      originalBytes: rom.z64.slice(moved.tailDmaStart, moved.tailDmaStart + moved.windowSize),
    };
  }

  function writeRelocatedArchive(rom, builtArchive, moved) {
    rom.z64.set(
      rom.z64.slice(moved._sourceWindowStart, moved._sourceWindowStart + moved._sourceWindowDelta),
      moved.tailDmaStart
    );
    rom.z64.set(builtArchive.slice(0, moved._archiveSize), moved.tailArchiveOffset);
    rom.z64[moved.tailArchiveOffset + moved._archiveSize] = 0x00;
  }

  function publicRelocationRegions(relocations) {
    return relocationPatchRegions(relocations || []);
  }

  function unitCountFromRecord(b) {
    if (!b || !b[0]) return 0;
    var c = 1;
    if (b[7]) [13, 14, 15].forEach(function(f) { if (b[f]) c++; });
    if (b[16]) [22, 23, 24].forEach(function(f) { if (b[f]) c++; });
    return c;
  }

  function hexRecordBytes(hex) {
    if (!hex) return null;
    var out = [];
    for (var i = 0; i < hex.length; i += 2) out.push(parseInt(hex.substr(i, 2), 16));
    return out;
  }

  // Predicted deployed-unit records at load for this key. The runtime table holds 100
  // stride-52 records including the padding record; the busiest vanilla load observed live
  // uses 88, so the cap is real headroom, not theory.
  function predictedUnits(rom, key) {
    var model = modelFor(rom, key);
    var records = ((OB64.SCENARIO_ESET_DATA || {}).enemydat || {}).records || [];
    if (!model) return null;
    var total = 1; // padding record 0
    model.section1.forEach(function(row) {
      var e = (((row.bytes[1] || 0) << 8) | (row.bytes[2] || 0)) - 1;
      var over = rom.squadOverrides && rom.squadOverrides[key + ':' + e];
      total += unitCountFromRecord(over ? Array.from(over) : hexRecordBytes(records[e]));
    });
    return total;
  }

  function anyProjectStub(rom) {
    var state = ensureState(rom);
    var siteKeys = Object.keys(state.siteAllegiances).filter(function(key) {
      return Object.keys(state.siteAllegiances[key] || {}).length > 0;
    });
    return {
      siteAllegianceKeys: siteKeys,
      addedSquads: state.addedSquads || [],
    };
  }

  function classIconForPoint(point) {
    var classes = point && point.classes ? point.classes : [];
    var cls = classes[0] && classes[0].classId;
    return cls && OB64.classPortraitUrl ? OB64.classPortraitUrl(cls) : '';
  }

  var iconProvider = {
    leaderIconUrl: classIconForPoint,
  };

  function projectionFor(cal, useImage) {
    var width = useImage && cal.image ? cal.image.width : 1000;
    var height = useImage && cal.image ? cal.image.height : 1000;
    return {
      naturalWidth: width,
      naturalHeight: height,
      pointToImage: function(point) {
        if (useImage && point && point.image) return { x: point.image.x, y: point.image.y };
        if (point && point.world) return worldToImage(cal, point.world.x, point.world.z, useImage);
        return { x: width / 2, y: height / 2 };
      },
      worldToImage: function(x, z) {
        return worldToImage(cal, x, z, useImage);
      },
      imageToWorld: function(x, y) {
        if (useImage && cal.worldToImage) {
          return {
            x: (x - (cal.worldToImage.c || 0)) / (cal.worldToImage.a || 1),
            z: (y - (cal.worldToImage.f || 0)) / (cal.worldToImage.e || 1),
          };
        }
        var b = cal.boundsWorld || { xMin: -16, xMax: 16, zMin: -16, zMax: 16 };
        return {
          x: b.xMin + (x / width) * (b.xMax - b.xMin),
          z: b.zMin + (y / height) * (b.zMax - b.zMin),
        };
      },
    };
  }

  function worldToImage(cal, x, z, useImage) {
    if (useImage && cal.worldToImage) {
      return {
        x: cal.worldToImage.a * x + (cal.worldToImage.b || 0) * z + cal.worldToImage.c,
        y: (cal.worldToImage.d || 0) * x + cal.worldToImage.e * z + cal.worldToImage.f,
      };
    }
    var b = cal.boundsWorld || { xMin: -16, xMax: 16, zMin: -16, zMax: 16 };
    return {
      x: ((x - b.xMin) / Math.max(0.001, b.xMax - b.xMin)) * 1000,
      y: ((z - b.zMin) / Math.max(0.001, b.zMax - b.zMin)) * 1000,
    };
  }

  function useImageFor(cal) {
    if (!cal || !cal.image || cal._artMissing || ui.viewMode === 'schematic') return false;
    return cal.registrationGrade === 'site-fitted';
  }

  function imagePath(rom, cal) {
    var base = ensureState(rom).settings.imageBasePath || defaultImageBase();
    if (!cal || !cal.image) return '';
    return base.replace(/[\\\/]?$/, '/') + cal.image.filename;
  }

  function pointTitle(point, runtimeRow) {
    var parts = [];
    parts.push('source ' + (runtimeRow ? runtimeRow.sourceId : point.sourceId));
    parts.push('edat ' + point.edat);
    if (point.wikiSquad) parts.push(point.wikiSquad);
    if (point.classes) parts.push(point.classes.map(function(c) { return c.className; }).join(' + '));
    return parts.join(' / ');
  }

  function renderScenarioTab(panel) {
    injectStyle();
    if (!OB64.scenarioCodec || !OB64.SCENARIO_ESET_DATA || !OB64.SCENARIO_MAP_CALIBRATION) {
      panel.innerHTML = '<p>Scenario data files are not loaded.</p>';
      return;
    }
    var rom = OB64._romRef && OB64._romRef();
    if (!rom) return;
    ensureState(rom);
    if (!modelFor(rom, ui.selectedKey)) {
      var first = dataScenarios().filter(function(s) { return !s.missing; })[0];
      ui.selectedKey = first ? first.runtimeKey : 1;
    }

    // Full re-render resets every scrollbar; capture and restore them (list, map, detail, page).
    var scrolls = {
      list: panel.querySelector('#sc-list'),
      map: panel.querySelector('#sc-map-panel .sc-map-scroll'),
      detail: panel.querySelector('#sc-detail'),
    };
    var saved = {
      winX: window.pageXOffset || 0,
      winY: window.pageYOffset || 0,
    };
    Object.keys(scrolls).forEach(function(k) {
      saved[k] = scrolls[k] ? { top: scrolls[k].scrollTop, left: scrolls[k].scrollLeft } : null;
    });

    panel.innerHTML =
      '<div class="sc-page">' +
        '<div class="sc-titlebar">' +
          '<div><h2>Scenario</h2><div class="sc-gate" id="sc-gate">' + esc(ui.gateText) + '</div></div>' +
          '<div class="sc-actions">' +
            // Codec self-test kept wired but hidden: it validates the editor build, not the
            // user's edits, so it reads as noise in the toolbar. Re-enable by removing the
            // style attribute if a support workflow needs one-click verification again.
            '<button type="button" id="sc-run-gate" class="btn-secondary" style="display:none" title="Self-test: rebuilds all 63 vanilla mission archives through the editor codec and confirms every one is byte-identical. Does not change your ROM or edits.">Validate Missions</button>' +
            '<button type="button" id="sc-add-squad">Add Squad</button>' +
          '</div>' +
        '</div>' +
        '<div class="sc-layout">' +
          '<div class="sc-list" id="sc-list"></div>' +
          '<div class="sc-map-panel" id="sc-map-panel"></div>' +
          '<div class="sc-detail" id="sc-detail"></div>' +
        '</div>' +
      '</div>';
    wireToolbar(panel, rom);
    renderList(panel.querySelector('#sc-list'), rom);
    renderMapPanel(panel.querySelector('#sc-map-panel'), rom);
    renderDetail(panel.querySelector('#sc-detail'), rom);

    var keyChanged = ui.lastFitKey !== ui.selectedKey;
    var restore = {
      list: panel.querySelector('#sc-list'),
      map: panel.querySelector('#sc-map-panel .sc-map-scroll'),
      detail: panel.querySelector('#sc-detail'),
    };
    Object.keys(restore).forEach(function(k) {
      if (k === 'map' && keyChanged) return; // fresh scenario: zoom-to-fit instead
      if (restore[k] && saved[k]) { restore[k].scrollTop = saved[k].top; restore[k].scrollLeft = saved[k].left; }
    });
    window.scrollTo(saved.winX, saved.winY);

    if (keyChanged) {
      ui.lastFitKey = ui.selectedKey;
      var cal2 = calibrationData(ui.selectedKey);
      var proj = cal2 ? projectionFor(cal2, useImageFor(cal2)) : null;
      var scroller = panel.querySelector('#sc-map-panel .sc-map-scroll');
      if (proj && scroller) {
        var x0 = 0, y0 = 0, x1 = proj.naturalWidth, y1 = proj.naturalHeight;
        if (cal2 && cal2.boundsWorld) {
          var b2 = cal2.boundsWorld;
          var pA = proj.worldToImage(b2.xMin, b2.zMin);
          var pB = proj.worldToImage(b2.xMax, b2.zMax);
          x0 = Math.min(pA.x, pB.x); x1 = Math.max(pA.x, pB.x);
          y0 = Math.min(pA.y, pB.y); y1 = Math.max(pA.y, pB.y);
        }
        var vw = scroller.clientWidth || 720, vh = scroller.clientHeight || 620;
        var fit = clamp(Math.min(vw / Math.max(1, x1 - x0), vh / Math.max(1, y1 - y0)) * 0.92, 0.15, 2);
        if (Math.abs(fit - ui.zoom) > 0.01) {
          ui.zoom = fit;
          renderMapPanel(panel.querySelector('#sc-map-panel'), rom);
          scroller = panel.querySelector('#sc-map-panel .sc-map-scroll');
        }
        scroller.scrollLeft = ((x0 + x1) / 2) * ui.zoom - vw / 2;
        scroller.scrollTop = ((y0 + y1) / 2) * ui.zoom - vh / 2;
      }
    }
  }

  function wireToolbar(panel, rom) {
    var gate = panel.querySelector('#sc-run-gate');
    if (gate) gate.onclick = function() {
      var result = OB64.scenarioCodec.roundTripAll(OB64.SCENARIO_ESET_DATA);
      ui.gateText = 'Mission validation: ' + result.summary.passed + '/' + result.summary.files + ' vanilla missions rebuild byte-identical, errors=' + result.summary.errors +
        (result.summary.errors === 0 && result.summary.passed === result.summary.files ? ' - mission editing is healthy.' : ' - REPORT THIS: the codec disagrees with this ROM.');
      renderScenarioTab(panel);
    };
    var add = panel.querySelector('#sc-add-squad');
    if (add) add.onclick = function() {
      beginAddSquadPlacement(rom, ui.selectedKey);
    };
  }

  // Dev/internal/special-loader runtime keys, sorted to the bottom of the scenario list.
  // 10/22/35/62 = the eset0_00 internal-alias keys (no wiki mission of their own);
  // 54/63/64 = bugged dev/special-loader keys (54 is the units-less runtime dupe of the
  // wiki-42 Keryoleth II pair 52/53). This is a curatorial classification from the
  // runtime-key wiki-identity audit plus live observation, not a decodable ROM byte.
  var DEV_KEYS = [10, 22, 35, 54, 62, 63, 64];
  function isDevKey(runtimeKey) { return DEV_KEYS.indexOf(runtimeKey) >= 0; }

  function renderList(el, rom) {
    var q = ui.search.toLowerCase().trim();
    var scenarios = dataScenarios().slice().sort(function(a, b) {
      var ad = isDevKey(a.runtimeKey), bd = isDevKey(b.runtimeKey);
      if (ad !== bd) return ad ? 1 : -1;
      return a.runtimeKey - b.runtimeKey;
    });
    var html = '<div class="sc-list-tools"><input id="sc-search" placeholder="Search runtime keys, missions, branches" value="' + esc(ui.search) + '"></div>';
    var lastGroup = null;
    scenarios.forEach(function(entry) {
      var cal = calibrationData(entry.runtimeKey);
      var scn = squadScenario(entry.runtimeKey);
      var label = displayLabel(entry.runtimeKey);
      var group = isDevKey(entry.runtimeKey)
        ? 'Dev / internal keys (not normal scenarios)'
        : (scn && scn.wikiId ? ('Wiki ' + scn.wikiId + ': ' + (scn.wikiTitle || scn.wikiHint || label)) : 'Internal or branch aliases');
      var hay = [entry.runtimeKey, label, group, cal && cal.mapName, scn && scn.branchStatus, scn && scn.branchConfidence].join(' ').toLowerCase();
      if (q && hay.indexOf(q) < 0) return;
      if (group !== lastGroup) {
        html += '<div class="sc-group">' + esc(group) + '</div>';
        lastGroup = group;
      }
      var tArchive = treasureArchiveForKey(entry.runtimeKey);
      var modified = keyModified(rom, entry.runtimeKey) || (tArchive && treasureArchiveModified(rom, tArchive));
      html += '<button type="button" class="sc-key' + (entry.runtimeKey === ui.selectedKey ? ' on' : '') + (isDevKey(entry.runtimeKey) ? ' sc-key-dev' : '') + '" data-key="' + entry.runtimeKey + '">' +
        '<span class="sc-key-name">' + esc(label) + '</span>' +
        '<span class="sc-chip">key ' + entry.runtimeKey + '</span>' +
        '<span class="sc-key-sub">' + esc((cal && cal.mapName ? cal.mapName + ' / ' : '') + (cal ? cal.registrationGrade : 'no map') + (modified ? ' / edited' : '')) + '</span>' +
      '</button>';
    });
    el.innerHTML = html;
    el.querySelectorAll('.sc-key').forEach(function(btn) {
      btn.onclick = function() {
        ui.selectedKey = parseInt(this.dataset.key, 10);
        ui.selectedPoint = null;
        ui.selectedSite = null;
        ui.selectedTrigger = null;
        ui.selectedTreasure = null;
        ui.selectedNode = null;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
    });
    var search = el.querySelector('#sc-search');
    if (search) search.oninput = function() {
      ui.search = this.value;
      renderList(el, rom);
      var next = document.getElementById('sc-search');
      if (next) {
        next.focus();
        next.setSelectionRange(next.value.length, next.value.length);
      }
    };
  }

  function renderMapPanel(el, rom) {
    var key = ui.selectedKey;
    var cal = calibrationData(key);
    var model = modelFor(rom, key);
    var state = ensureState(rom);
    if (!cal || !model) {
      el.innerHTML = '<div class="sc-warning">No map or ESET data for key ' + key + '.</div>';
      return;
    }
    var useImage = useImageFor(cal);
    var projection = projectionFor(cal, useImage);
    var width = Math.max(720, Math.round(projection.naturalWidth * ui.zoom));
    var height = Math.max(520, Math.round(projection.naturalHeight * ui.zoom));
    el.innerHTML =
      '<div class="sc-map-head">' +
        '<div class="sc-map-title">' + esc(displayLabel(key)) + '</div>' +
        '<div class="sc-map-tools">' +
          '<select id="sc-view-mode"><option value="auto">Art (calibrated)</option><option value="schematic">Schematic</option></select>' +
          '<span class="sc-chip" id="sc-zoom-chip" title="Scroll the map to zoom">' + Math.round(ui.zoom * 100) + '%</span>' +
          '<span class="sc-chip">' + esc(cal.registrationGrade || 'ungraded') + '</span>' +
        '</div>' +
      '</div>' +
      '<div class="sc-map-scroll"><div id="sc-map-inner" class="sc-map-inner" style="width:' + width + 'px;height:' + height + 'px"></div></div>' +
      '<div class="sc-layer-toggles">' +
        layerToggleHtml('squads', 'Squads') +
        layerToggleHtml('sites', 'Sites') +
        layerToggleHtml('routes', 'Routes') +
        layerToggleHtml('triggers', 'Triggers') +
        layerToggleHtml('treasure', 'Treasure') +
      '</div>' +
      '<div class="sc-route-legend">' +
        '<span class="sc-leg-t">Route lines:</span>' +
        '<span class="sc-leg"><svg width="26" height="8"><line x1="0" y1="4" x2="26" y2="4" stroke="#6a4d28" stroke-width="3"></line></svg> marches at start</span>' +
        '<span class="sc-leg"><svg width="26" height="8"><line x1="0" y1="4" x2="26" y2="4" stroke="#6a4d28" stroke-width="3" stroke-dasharray="9 7"></line></svg> waits before advancing</span>' +
        '<span class="sc-leg"><svg width="26" height="8"><line x1="0" y1="4" x2="26" y2="4" stroke="#2f8f4e" stroke-width="2" stroke-dasharray="3 5"></line></svg> conditional fork</span>' +
        '<span class="sc-leg"><span class="sc-leg-dot" style="background:#4db0d2"></span><span class="sc-leg-dot" style="background:#e6a92e"></span><span class="sc-leg-dot" style="background:#d2564b"></span> each color = one squad (matches roster bar)</span>' +
      '</div>' +
      '<div class="sc-roster" id="sc-roster"></div>';
    var view = el.querySelector('#sc-view-mode');
    if (view) {
      if (ui.viewMode !== 'schematic') ui.viewMode = 'auto';
      view.value = ui.viewMode;
      view.onchange = function() {
        ui.viewMode = this.value;
        renderMapPanel(el, rom);
      };
    }
    // Wheel-to-zoom, anchored at the cursor so the point under the pointer stays put.
    var scroller = el.querySelector('.sc-map-scroll');
    if (scroller) scroller.addEventListener('wheel', function(ev) {
      if (mapTool) return; // draw/pick/place tools own the map; zooming would strand their listeners
      ev.preventDefault();
      var rect = scroller.getBoundingClientRect();
      var offX = ev.clientX - rect.left;
      var offY = ev.clientY - rect.top;
      var cx = scroller.scrollLeft + offX;
      var cy = scroller.scrollTop + offY;
      var next = clamp(ui.zoom * Math.pow(1.0015, -ev.deltaY), 0.12, 3);
      if (Math.abs(next - ui.zoom) < 0.0001) return;
      var scale = next / ui.zoom;
      ui.zoom = next;
      renderMapPanel(el, rom);
      var s2 = el.querySelector('.sc-map-scroll');
      if (s2) {
        s2.scrollLeft = cx * scale - offX;
        s2.scrollTop = cy * scale - offY;
      }
    }, { passive: false });
    el.querySelectorAll('input[data-layer]').forEach(function(box) {
      box.onchange = function() {
        ui.layers[this.dataset.layer] = !!this.checked;
        // Preserve the scroll position across the re-render so toggling a filter doesn't jump the view.
        var sc = el.querySelector('.sc-map-scroll');
        var sl = sc ? sc.scrollLeft : 0, st = sc ? sc.scrollTop : 0;
        renderMapPanel(el, rom);
        var sc2 = el.querySelector('.sc-map-scroll');
        if (sc2) { sc2.scrollLeft = sl; sc2.scrollTop = st; }
      };
    });
    var inner = el.querySelector('#sc-map-inner');
    if (useImage && cal.image) {
      inner.innerHTML = '<img class="sc-map-img" src="' + esc(imagePath(rom, cal)) + '" alt="">';
      var img = inner.querySelector('img');
      img.onerror = function() {
        // Fall back for THIS map only; a missing PNG must not clobber the global view mode.
        cal._artMissing = true;
        renderMapPanel(el, rom);
      };
    } else {
      inner.innerHTML = '<svg class="sc-schematic" viewBox="0 0 1000 1000" preserveAspectRatio="none">' +
        '<rect x="0" y="0" width="1000" height="1000" fill="#2f3b32"></rect>' +
        '<path d="M0 500H1000M500 0V1000" stroke="rgba(245,230,200,.18)" stroke-width="2"></path>' +
        '<rect x="60" y="60" width="880" height="880" fill="none" stroke="rgba(245,230,200,.48)" stroke-width="4" stroke-dasharray="12 10"></rect>' +
      '</svg>';
    }
    renderBounds(inner, cal, projection, useImage, ui.zoom);
    buildLayers(rom, key, cal, model, projection, ui.zoom).forEach(function(layer) {
      if (ui.layers[layer.id]) layer.render(inner);
    });
    renderSquadRoster(el.querySelector('#sc-roster'), rom, key, model);
    // Clicking empty map (not a marker/shape) returns the sidebar to the scenario overview.
    inner.addEventListener('click', function(ev) {
      if (mapTool) return; // a draw/pick/place tool owns the map right now
      var t = ev.target;
      var isBackground = t === inner ||
        (t.classList && t.classList.contains('sc-map-img')) ||
        (t.closest && t.closest('.sc-schematic'));
      if (!isBackground) return;
      if (ui.selectedPoint == null && !ui.selectedSite && ui.selectedTrigger == null && !ui.selectedTreasure) return;
      clearSelection();
    });
  }

  function layerToggleHtml(id, label) {
    return '<label><input type="checkbox" data-layer="' + id + '"' + (ui.layers[id] ? ' checked' : '') + '> ' + esc(label) + '</label>';
  }

  function renderBounds(inner, cal, projection, useImage, zoom) {
    if (!cal.boundsPixels && !cal.boundsWorld) return;
    var left, top, right, bottom;
    if (useImage && cal.boundsPixels) {
      left = cal.boundsPixels.left * zoom;
      top = cal.boundsPixels.top * zoom;
      right = cal.boundsPixels.right * zoom;
      bottom = cal.boundsPixels.bottom * zoom;
    } else {
      var b = cal.boundsWorld;
      var p1 = projection.worldToImage(b.xMin, b.zMin);
      var p2 = projection.worldToImage(b.xMax, b.zMax);
      left = Math.min(p1.x, p2.x) * zoom;
      top = Math.min(p1.y, p2.y) * zoom;
      right = Math.max(p1.x, p2.x) * zoom;
      bottom = Math.max(p1.y, p2.y) * zoom;
    }
    var div = document.createElement('div');
    div.className = 'sc-bounds';
    div.style.left = left + 'px';
    div.style.top = top + 'px';
    div.style.width = Math.max(1, right - left) + 'px';
    div.style.height = Math.max(1, bottom - top) + 'px';
    inner.appendChild(div);
  }

  function buildLayers(rom, key, cal, model, projection, zoom) {
    return [
      { id: 'routes', name: 'Routes', render: function(inner) { renderRouteLayer(inner, rom, key, cal, model, projection, zoom); } },
      { id: 'triggers', name: 'Triggers', render: function(inner) { renderTriggerLayer(inner, rom, key, cal, model, projection, zoom); } },
      { id: 'sites', name: 'Sites', render: function(inner) { renderSiteLayer(inner, rom, key, projection, zoom); } },
      { id: 'treasure', name: 'Treasure', render: function(inner) { renderTreasureLayer(inner, rom, key, projection, zoom); } },
      { id: 'squads', name: 'Squads', render: function(inner) { renderSquadLayer(inner, rom, key, projection, zoom); } },
    ];
  }

  function renderSiteLayer(inner, rom, key, projection, zoom) {
    var sites = ensureState(rom).sites[key] || [];
    sites.forEach(function(site) {
      var p = projection.worldToImage(site.x, site.z);
      var allegiance = siteAllegiance(rom, key, site.selector);
      var btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'sc-marker sc-site-marker ' + allegiance + (ui.selectedSite && ui.selectedSite.selector === site.selector ? ' on' : '');
      btn.style.left = (p.x * zoom) + 'px';
      btn.style.top = (p.y * zoom) + 'px';
      btn.title = site.siteName + ' / selector ' + site.selector + ' / ' + allegiance;
      btn.dataset.selector = site.selector;
      btn.onclick = function() {
        ui.selectedPoint = null;
        ui.selectedTrigger = null;
        ui.selectedTreasure = null;
        ui.selectedNode = null;
        ui.selectedSite = site;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
      inner.appendChild(btn);
    });
  }

  function renderTreasureLayer(inner, rom, key, projection, zoom) {
    var model = treasureModelForKey(rom, key);
    if (!model) return;
    model.records.forEach(function(record, index) {
      var world = treasureWorldForKey(key, record);
      if (!world) return;
      var p = projection.worldToImage(world.x, world.z);
      var selected = ui.selectedTreasure && ui.selectedTreasure.archive === model.archive && ui.selectedTreasure.index === index;
      var btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'sc-marker sc-treasure-marker' + (selected ? ' on' : '');
      btn.style.left = (p.x * zoom) + 'px';
      btn.style.top = (p.y * zoom) + 'px';
      btn.title = treasureItemName(record) + ' / gid ' + record.globalId + ' / x ' + record.x + ' y ' + record.y;
      btn.dataset.archive = model.archive;
      btn.dataset.index = index;
      btn.innerHTML = '<img src="' + esc(treasureItemIcon(record)) + '" alt="">';
      var img = btn.querySelector('img');
      if (img) img.onerror = function() { img.style.visibility = 'hidden'; };
      btn.onclick = function() {
        ui.selectedTreasure = { archive: model.archive, index: index };
        clearOtherSelectionsForTreasure();
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
      wireTreasureDrag(btn, rom, key, model.archive, index, projection, zoom);
      inner.appendChild(btn);
    });
  }

  function wireTreasureDrag(btn, rom, key, archive, index, projection, zoom) {
    btn.addEventListener('pointerdown', function(ev) {
      if (ev.button !== 0) return;
      ev.preventDefault();
      var moved = false;
      var startX = ev.clientX;
      var startY = ev.clientY;
      var move = function(mv) {
        if (!moved && Math.hypot(mv.clientX - startX, mv.clientY - startY) < 5) return;
        moved = true;
        btn.style.cursor = 'grabbing';
        var inner = document.getElementById('sc-map-inner');
        if (!inner) return;
        var rect = inner.getBoundingClientRect();
        btn.style.left = clamp(mv.clientX - rect.left, 0, rect.width) + 'px';
        btn.style.top = clamp(mv.clientY - rect.top, 0, rect.height) + 'px';
      };
      var up = function(uv) {
        document.removeEventListener('pointermove', move);
        document.removeEventListener('pointerup', up);
        btn.style.cursor = '';
        if (!moved) return;
        btn.addEventListener('click', function block(ce) {
          ce.stopPropagation(); ce.preventDefault();
          btn.removeEventListener('click', block, true);
        }, true);
        var inner = document.getElementById('sc-map-inner');
        if (!inner) return;
        var rect = inner.getBoundingClientRect();
        var imageX = clamp((uv.clientX - rect.left) / zoom, 0, projection.naturalWidth);
        var imageY = clamp((uv.clientY - rect.top) / zoom, 0, projection.naturalHeight);
        moveTreasureFromImage(rom, key, archive, index, imageX, imageY, projection);
      };
      document.addEventListener('pointermove', move);
      document.addEventListener('pointerup', up);
    });
  }

  // Marker list = every CURRENT Section 1 row: calibration points where they exist, synthetic
  // points for rows without one (added squads, uncalibrated vanilla rows).
  function pointsForAllRows(rom, key) {
    var model = modelFor(rom, key);
    var out = [];
    ((model && model.section1) || []).forEach(function(row, i) {
      out.push(resolvePointForRow(rom, key, i));
    });
    return out;
  }

  function syntheticPoint(rom, key, i, row) {
    return {
      section1Row: i,
      sourceId: row.sourceId,
      edat: row.edatOneBased - 1,
      world: rowWorld(rom, key, i, null),
      added: isAddedRow(rom, key, i),
    };
  }

  // Leader icon from the LIVE record: squad override first (added squads always have one, and
  // vanilla squads reflect comp edits immediately), then static calibration classes, then the
  // wiki vanilla record.
  function liveLeaderIcon(rom, key, point) {
    if (!point) return '';
    var over = rom.squadOverrides && rom.squadOverrides[key + ':' + point.edat];
    if (over && over[0] && OB64.classPortraitUrl) return OB64.classPortraitUrl(over[0]);
    var icon = classIconForPoint(point);
    if (icon) return icon;
    var scn = squadScenario(key);
    var sq = scn && (scn.squads || []).filter(function(s) { return s.e === point.edat; })[0];
    if (sq && sq.rec && OB64.classPortraitUrl) {
      var cls = parseInt(sq.rec.substr(0, 2), 16);
      if (cls) return OB64.classPortraitUrl(cls);
    }
    return '';
  }

  function renderSquadLayer(inner, rom, key, projection, zoom) {
    var model = modelFor(rom, key);
    pointsForAllRows(rom, key).forEach(function(point) {
      var runtimeRow = rowRuntime(rom, key, point.section1Row);
      var liveWorld = rowWorld(rom, key, point.section1Row, point);
      if (!liveWorld && !point.world && !point.image) return; // no resolvable position
      var p = liveWorld ? projection.worldToImage(liveWorld.x, liveWorld.z) : projection.pointToImage(point);
      var dormant = rowIsDormant(rom, key, model, point.section1Row);
      var img = liveLeaderIcon(rom, key, point);
      var btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'sc-marker sc-squad-marker enemy' + (dormant ? ' dormant' : '') +
        (point.added ? ' added' : '') +
        (ui.selectedPoint === point.section1Row ? ' on' : '');
      btn.style.left = (p.x * zoom) + 'px';
      btn.style.top = (p.y * zoom) + 'px';
      btn.title = pointTitle(point, runtimeRow) + (point.added ? ' / ADDED squad' : '');
      btn.dataset.row = point.section1Row;
      btn.innerHTML = (img ? '<img src="' + esc(img) + '" alt="">' : '') +
        (point.added && dormant ? '<span class="sc-badge">+!</span>' :
          point.added ? '<span class="sc-badge">+</span>' :
          dormant ? '<span class="sc-badge">!</span>' : '');
      btn.onclick = function() {
        ui.selectedPoint = point.section1Row;
        ui.selectedSite = null;
        ui.selectedTrigger = null;
        ui.selectedTreasure = null;
        ui.selectedNode = null;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
      wireMarkerDrag(btn, rom, key, point, projection, zoom);
      inner.appendChild(btn);
    });
  }

  // Effective 35-byte comp record for a row: live squad override first, else the vanilla
  // record from the scenario squad data. Null when neither is known.
  function effectiveRecordFor(rom, key, point) {
    var over = rom.squadOverrides && rom.squadOverrides[key + ':' + point.edat];
    if (over && over.length) return over;
    var scn = squadScenario(key);
    var sq = scn && (scn.squads || []).filter(function(s) { return s.e === point.edat; })[0];
    if (sq && sq.rec) return hexRecordBytes(sq.rec);
    return null;
  }

  // Squad roster under the map: every Section 1 row with leader, comp size, behavior, and
  // Select/Delete actions. Clicking a row selects the squad exactly like its map marker.
  function renderSquadRoster(host, rom, key, model) {
    if (!host || !model) return;
    var points = pointsForAllRows(rom, key);
    var html = '<div class="sc-roster-head"><span class="sc-label" style="margin:0">Squads in this scenario (' + points.length + ')</span></div>' +
      '<div class="sc-roster-list">';
    points.forEach(function(point) {
      var row = model.section1[point.section1Row];
      if (!row) return;
      var dormant = rowIsDormant(rom, key, model, point.section1Row);
      var rec = effectiveRecordFor(rom, key, point);
      var leader = rec && rec[0] ? (OB64.className ? OB64.className(rec[0]) : '0x' + rec[0].toString(16)) : 'unknown leader';
      var units = rec ? unitCountFromRecord(rec) : null;
      var icon = liveLeaderIcon(rom, key, point);
      var behavior = describeBehavior(rom, key, model, row);
      // Color bar = this squad's route color on the map (only squads that actually march).
      var marches = behavior && behavior.indexOf('Guard') !== 0 && behavior !== 'unknown';
      var barStyle = marches ? 'border-left:5px solid ' + routeColor(point.section1Row) + ';' : '';
      html += '<div class="sc-roster-row' + (ui.selectedPoint === point.section1Row ? ' on' : '') + '" data-row="' + point.section1Row + '" role="button" tabindex="0" style="' + barStyle + '">' +
        (icon ? '<img src="' + esc(icon) + '" alt="">' : '<span></span>') +
        '<span><strong>Source ' + esc(row.sourceId) + ' / EDAT ' + esc(point.edat) + '</strong>' +
        '<span class="sc-sub" style="display:block">' + esc(leader) + (units != null ? ' - ' + units + ' unit' + (units === 1 ? '' : 's') : '') + '</span></span>' +
        '<span class="sc-sub">' + esc(behavior || '') + '</span>' +
        '<span class="sc-chips">' +
        (point.added ? '<span class="sc-chip">Added</span>' : '') +
        (dormant ? '<span class="sc-chip">Dormant</span>' : '') +
        '</span>' +
        '<button type="button" class="sc-inline-btn sc-danger sc-roster-del" data-row="' + point.section1Row + '" title="Remove this squad from the mission">Delete</button>' +
        '</div>';
    });
    if (!points.length) html += '<div class="sc-node">No squads in this scenario.</div>';
    html += '</div>';
    host.innerHTML = html;
    host.querySelectorAll('.sc-roster-row').forEach(function(rowEl) {
      rowEl.onclick = function(ev) {
        if (ev.target.classList && ev.target.classList.contains('sc-roster-del')) return;
        ui.selectedPoint = parseInt(this.dataset.row, 10);
        ui.selectedSite = null;
        ui.selectedTrigger = null;
        ui.selectedTreasure = null;
        ui.selectedNode = null;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
    });
    host.querySelectorAll('.sc-roster-del').forEach(function(btn) {
      btn.onclick = function(ev) {
        ev.stopPropagation();
        deleteSquadRow(rom, key, parseInt(this.dataset.row, 10));
      };
    });
  }

  // Delete ANY Section 1 squad row. Added squads route through deleteAddedSquad (which also
  // retires their override + donor bookkeeping); vanilla rows splice out of the mission ESET
  // (the global enemydat record is untouched - the squad just never deploys here).
  function deleteSquadRow(rom, key, rowIndex) {
    if (isAddedRow(rom, key, rowIndex)) { deleteAddedSquad(rom, key, rowIndex); return; }
    var state = ensureState(rom);
    var model = state.models[key];
    var row = model && model.section1[rowIndex];
    if (!row) return;
    confirmThemed('Remove squad from mission',
      'Remove squad source ' + row.sourceId + ' / EDAT ' + (row.edatOneBased - 1) +
      ' from this mission?\n\nThe squad will not deploy here anymore. Its choreography nodes stay ' +
      'in the mission (harmless), and the global enemy record is untouched.',
      'Remove squad',
      function() {
        var edatId = rowEdatId(row);
        model.section1.splice(rowIndex, 1);
        state.addedSquads.forEach(function(r) {
          if (r.runtimeKey === key && r.section1Row != null && r.section1Row > rowIndex) r.section1Row--;
        });
        if (rom.squadOverrides && edatId != null) {
          var stillReferenced = model.section1.some(function(r) { return rowEdatId(r) === edatId; });
          if (!stillReferenced && rom.squadOverrides[key + ':' + edatId]) {
            delete rom.squadOverrides[key + ':' + edatId];
            if (OB64._squadChanged) OB64._squadChanged();
          }
        }
        var gc = runNodeGc(model);
        if (!gc.changed) {
          syncStructuralOffsets(model);
          OB64.scenarioCodec.refreshDecodedRows(model);
        }
        state.modifiedKeys[key] = true;
        changed();
        if (ui.selectedPoint === rowIndex) ui.selectedPoint = null;
        else if (ui.selectedPoint != null && ui.selectedPoint > rowIndex) ui.selectedPoint--;
        ui.gateText = 'Squad removed from this mission (source ' + row.sourceId + ').' + (gc.message ? ' ' + gc.message : '');
        renderScenarioTab(document.getElementById('panel-scenario'));
      });
  }

  // Themed confirm with a plain-confirm fallback so the flow still works if app.js has not
  // exported the modal (e.g. module loaded standalone in tests).
  function confirmThemed(title, message, confirmLabel, onConfirm) {
    if (OB64.showConfirmModal) OB64.showConfirmModal(title, message, onConfirm, confirmLabel);
    else if (window.confirm(message)) onConfirm();
  }

  function confirmIfSharedNode(rom, key, model, rowIndex, node, title, actionText, onConfirm) {
    var refs = node ? nodeConsumerRows(model, node.nodeId) : [];
    var others = otherSquadRefs(refs, rowIndex);
    if (!others.length) { onConfirm(); return; }
    confirmThemed(title,
      actionText + '\n\nThis start node is shared. Affected squads: ' + formatSquadRefs(refs) + '.',
      'Apply anyway',
      onConfirm);
  }

  function confirmIfSharedExtra(rom, key, model, rowIndex, extraId, title, actionText, onConfirm) {
    var refs = nodesUsedBySquads(model, triggerRefNodes(model, extraId));
    var others = otherSquadRefs(refs, rowIndex);
    if (!others.length) { onConfirm(); return; }
    confirmThemed(title,
      actionText + '\n\nThis trigger is shared. Affected squads: ' + formatSquadRefs(refs) + '.',
      'Apply anyway',
      onConfirm);
  }

  function replacementClause(replaced) {
    return replaced ? ' (replaces ' + replaced + ')' : '';
  }

  function startGateStatus(triggerId, replaced) {
    if (triggerId) return 'Advance gate set to E' + triggerId + replacementClause(replaced) + '.';
    return 'Advance gate cleared' + replacementClause(replaced) + ' - route is now UNGATED (solid line): the unit may advance immediately. Use "Remove route" to make it hold position.';
  }

  function wireMarkerDrag(btn, rom, key, point, projection, zoom) {
    btn.addEventListener('pointerdown', function(ev) {
      if (ev.button !== 0) return;
      ev.preventDefault();
      var moved = false;
      var startX = ev.clientX;
      var startY = ev.clientY;
      var snapRing = null;
      var move = function(mv) {
        if (!moved && Math.hypot(mv.clientX - startX, mv.clientY - startY) < 5) return;
        moved = true;
        btn.style.cursor = 'grabbing';
        var inner = document.getElementById('sc-map-inner');
        if (!inner) return;
        var rect = inner.getBoundingClientRect();
        btn.style.left = clamp(mv.clientX - rect.left, 0, rect.width) + 'px';
        btn.style.top = clamp(mv.clientY - rect.top, 0, rect.height) + 'px';
        // Snap preview: highlight the site this drop would attach to (same screen-px rule
        // as placementBytesFromImage).
        var imageX = (mv.clientX - rect.left) / zoom;
        var imageY = (mv.clientY - rect.top) / zoom;
        var nearest = null, best = Infinity;
        (ensureState(rom).sites[key] || []).forEach(function(site) {
          var sp = projection.worldToImage(site.x, site.z);
          var d = Math.hypot(sp.x - imageX, sp.y - imageY);
          if (d < best) { best = d; nearest = sp; }
        });
        if (nearest && best < SNAP_SCREEN_PX / zoom) {
          if (!snapRing) snapRing = mapGhost(inner, 'sc-snap-ring');
          snapRing.style.left = (nearest.x * zoom) + 'px';
          snapRing.style.top = (nearest.y * zoom) + 'px';
          snapRing.style.display = '';
        } else if (snapRing) {
          snapRing.style.display = 'none';
        }
      };
      var up = function(uv) {
        document.removeEventListener('pointermove', move);
        document.removeEventListener('pointerup', up);
        if (snapRing) snapRing.remove();
        btn.style.cursor = '';
        if (!moved) return; // plain click: let btn.onclick select
        // Swallow the click that follows a completed drag so selection state is not clobbered.
        btn.addEventListener('click', function block(ce) {
          ce.stopPropagation(); ce.preventDefault();
          btn.removeEventListener('click', block, true);
        }, true);
        var inner = document.getElementById('sc-map-inner');
        if (!inner) return;
        var rect = inner.getBoundingClientRect();
        var imageX = clamp((uv.clientX - rect.left) / zoom, 0, projection.naturalWidth);
        var imageY = clamp((uv.clientY - rect.top) / zoom, 0, projection.naturalHeight);
        updatePlacementFromImage(rom, key, point.section1Row, imageX, imageY, projection);
        ui.selectedPoint = point.section1Row;
        ui.selectedSite = null;
        ui.selectedTrigger = null;
        ui.selectedTreasure = null;
        ui.selectedNode = null;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
      document.addEventListener('pointermove', move);
      document.addEventListener('pointerup', up);
    });
  }

  // Resolve a scene-relative ktenmain record index to a site. This is the SECOND site index
  // space in the format, shared by kind-12 triggers and kind-1 subtype-1 waypoints, and is
  // anchored by two live in-game observations (a kind-12 trigger resolving to Hou, and a
  // subtype-1 waypoint whose squad marched to Mulsuk). 97/121 subtype-1 waypoints corpus-wide
  // resolve to selector-table sites; the rest reference scene records beyond the selector
  // table and render as unresolved.
  function siteBySceneRecord(rom, key, rel) {
    var sites = ensureState(rom).sites[key] || [];
    var min = null;
    sites.forEach(function(s) {
      if (s.ktenmainRecordIndex != null && (min === null || s.ktenmainRecordIndex < min)) min = s.ktenmainRecordIndex;
    });
    if (min === null) return null;
    return sites.filter(function(s) { return s.ktenmainRecordIndex === min + rel; })[0] || null;
  }

  // Waypoint payload decode (waypoint-field-confirmed 2026-07-05):
  //   [5] != 0 -> ([4],[5]) bounds-normalized byte coordinates (/256, game-exact).
  //   [5] == 0 -> SELECTOR-table target: selector = [4] - [3] (ALL subtypes; selector 0 = sentinel,
  //       unresolved). The march destination is the runtime object WAYPOINT (+0x28) = this site;
  //       a separate aggro field (+0x4C) diverts the squad to intercept the nearest player in range,
  //       which is why on a normal playthrough you see it chase your units rather than march cleanly.
  //       (The old subtype-1 "scene-record" reading was the +0x4C aggro field mis-decoded.)
  function nodeWorld(rom, key, node) {
    if (!node || node.kind !== 1) return null;
    if (!node.bytes) return null;
    if (node.bytes[5] === 0) {
      // Selector-space march destination for ALL subtypes. CONFIRMED 2026-07-05 via the runtime
      // waypoint field (map-unit object +0x28): key3 terminal nodes with [4]=5 set waypoint to
      // Baldera's exact world coords = the site at selector 5. The old subtype-1 "scene-record"
      // reading (sceneMin+[4]) was wrong - it came from reading the +0x4C AGGRO field (which chases
      // the nearest player unit) instead of the +0x28 march waypoint. See docs march-to checklist.
      var sel = node.bytes[4] - node.bytes[3];
      if (sel <= 0) return null; // selector-0 sentinel; semantics undecoded, render unresolved
      var site = (ensureState(rom).sites[key] || []).filter(function(s) { return s.selector === sel; })[0];
      return site ? { x: site.x, z: site.z, siteName: site.siteName, selector: site.selector } : null;
    }
    var b = calibrationData(key) && calibrationData(key).boundsWorld;
    if (!b) return null;
    return {
      x: b.xMin + (node.bytes[4] / 256) * (b.xMax - b.xMin),
      z: b.zMin + (node.bytes[5] / 256) * (b.zMax - b.zMin),
    };
  }

  // Live world position from the CURRENT model bytes (selector site or coordinate pair);
  // static calibration point is only the fallback. Rendering from live bytes is what makes
  // drag edits visible.
  function rowWorld(rom, key, rowIndex, fallbackPoint) {
    var model = modelFor(rom, key);
    var row = model && model.section1[rowIndex];
    if (row && row.bytes) {
      if (row.bytes[4] === 0) {
        var site = (ensureState(rom).sites[key] || []).filter(function(s) { return s.selector === row.bytes[3]; })[0];
        if (site) return { x: site.x, z: site.z };
      } else {
        var w = byteToWorld(calibrationData(key), row.bytes[3], row.bytes[4]);
        if (w) return w;
      }
    }
    return fallbackPoint && fallbackPoint.world ? fallbackPoint.world : null;
  }

  function nodeById(model, nodeId) {
    for (var i = 0; i < model.section2.length; i++) {
      if (model.section2[i].nodeId === nodeId) return model.section2[i];
    }
    return null;
  }

  // Walk the REAL route: Section 1 byte [6] start node -> Section 2 [17] next links
  // (operator-1 gates fork to byte [16]). Returns [{node, world|null}] max 20 hops.
  function walkNodeChain(rom, key, model, startNodeId) {
    var chain = [];
    var seen = {};
    var nodeId = startNodeId;
    for (var hop = 0; hop < 20; hop++) {
      if (!nodeId || nodeId === 0xFF || seen[nodeId]) break;
      var node = nodeById(model, nodeId);
      if (!node) break;
      seen[nodeId] = true;
      chain.push({ node: node, world: nodeWorld(rom, key, node) });
      var op = node.bytes ? node.bytes[11] : 0;
      if (op === 1 && node.bytes[16]) {
        chain[chain.length - 1].forkNodeId = node.bytes[16];
      }
      nodeId = node.bytes ? node.bytes[17] : 0;
    }
    return chain;
  }

  // Human-readable classification of a squad's CURRENT behavior from its decoded bytes.
  function describeBehavior(rom, key, model, row) {
    if (!row || !row.bytes) return 'unknown';
    var start = row.bytes[6];
    if (!start || start < 4 || start > 0x13) return 'Guard / hold position';
    var startNode = nodeById(model, start);
    if (!startNode) return 'Guard / hold position';
    var chain = walkNodeChain(rom, key, model, start);
    var last = chain[chain.length - 1];
    var terminal = last && last.node.bytes[17] === 0xFF;
    // Multi-hop routes chain several waypoints (e.g. key7 EDAT532: -> Mosaka -> Takua). The
    // FINAL resolved node is the destination; earlier resolved town stops are "via" points.
    var stops = [];
    chain.forEach(function(h) { if (h.world && h.world.siteName) stops.push(h.world.siteName.trim()); });
    var destName = '', destIsTown = false;
    for (var i = chain.length - 1; i >= 0; i--) {
      if (chain[i].world) {
        // town = selRaw selector site (byte[5]==0); coordinate node (byte[5]!=0) = an open
        // map point (camp/patrol anchor, mostly not on a town per the byte[5]!=0 corpus).
        if (chain[i].world.siteName) { destName = chain[i].world.siteName.trim(); destIsTown = true; }
        else { destName = 'a map point'; }
        break;
      }
    }
    var viaStops = destIsTown ? stops.slice(0, -1) : stops;
    var via = viaStops.length ? ' via ' + viaStops.join(' → ') : '';
    var gateA = startNode.bytes[10];
    var op = startNode.bytes[11];
    var gate = '';
    if (gateA) {
      var extra = model.section3.filter(function(x) { return x.extraId === gateA; })[0];
      var kindNames = { 1: 'player area', 4: 'player at site', 8: 'unit in area', 9: 'squads remaining', 12: 'site flag' };
      gate = 'E' + gateA + (extra && kindNames[extra.kind] ? ' (' + kindNames[extra.kind] + ')' : '');
      if (op === 2 && startNode.bytes[12]) gate += ' AND E' + startNode.bytes[12];
      if (op === 3 && startNode.bytes[12]) gate += ' OR E' + startNode.bytes[12];
      if (op === 1 && startNode.bytes[12]) gate += ' / else E' + startNode.bytes[12];
    }
    var marchWord = destName ? 'march to ' + destName + via : (chain.length > 1 ? 'march' : 'act');
    // No "diverts to intercept" note: vanilla town-marchers were observed diverting to nearby
    // players, but that is NOT reproducible for an editor-created marcher (a marching squad has no
    // Wait=Initiate order), so advertising it on the behavior line was misleading. See
    // docs/enemy-system.md "Enemy movement / aggro AI" (the open +0x92-vs-+0xBB march-intercept item).
    if (startNode.kind === 0 && !destName && !gateA) return startNode.bytes[3] === 1 ? 'Holds position, attacks anyone who comes near (sally)' : 'Holds position (hold node)';
    if (startNode.kind === 2) return 'Ambush - dormant until ' + (gate || 'advance trigger') + '; on pass, ' + marchWord + (terminal ? ' + camp' : '');
    if (gateA) return 'Wait for ' + gate + '; on pass, ' + marchWord + (terminal ? ' + camp' : '');
    if (destName) return 'March to ' + destName + via + (terminal ? ' + permanent camp' : '');
    return terminal ? 'March + permanent camp' : 'Patrol route (nodes ' + chain.map(function(h) { return h.node.nodeId; }).join('>') + ')';
  }

  // Plain-English guidance per Behavior template. The point users keep missing: movement comes from
  // NODES, and a plain march-to-a-point is PASSIVE - a squad only attacks while moving if it ADVANCES
  // from a gated hold/ambush node to a waypoint (the vanilla intercepting-marcher structure). The
  // default (no template selected = viewing the current behavior) states that model. See
  // docs/enemy-system.md "Enemy movement / aggro AI".
  function templateHelp(tpl) {
    if (tpl === 'guard-site') return 'Sits where it deploys and fights only what reaches it - never moves, never chases. This is a SENTINEL: it uses NO node (the cheapest option). It has no orders, so it cannot sally - use "Attacks anyone who comes near" for that.';
    if (tpl === 'guard-sally') return 'Sits at its post but attacks any player squad that comes within range (a "sally"). Uses ONE hold node with Wait = Initiate - a sentinel has no orders, so this is the node-backed version. Templates assert their Move/Wait orders; adjust Squad orders after applying if you want custom behavior.';
    if (tpl === 'march-chain') return 'PASSIVE: walks straight to the destination and ignores the player - it will NOT chase or intercept. For a marcher that attacks, use "Wait for trigger, then advance to destination" instead.';
    if (tpl === 'wait-march') return 'Holds at its post until the trigger fires, then advances to the destination. The trigger gates the move from the hold node to the next waypoint; it does not activate the hold node itself. Needs an Advance trigger + a Destination. Templates assert their Move/Wait orders; adjust Squad orders after applying if you want custom behavior.';
    if (tpl === 'solo-ambush') return 'Hidden and inert until the trigger fires, then advances to the destination (pursues, like a vanilla ambush). The trigger gates the move from the ambush node to its next node. Needs an Advance trigger + a Destination. Templates assert their Move/Wait orders; adjust Squad orders after applying if you want custom behavior.';
    if (tpl === 'reinforce-remnant') return 'Stays out of the fight until <= N enemy squads remain, then deploys. Set the threshold below. Templates assert their Move/Wait orders; adjust Squad orders after applying if you want custom behavior.';
    if (tpl === 'camp-terminal') return 'Marches to the destination and camps there permanently (one-way).';
    return 'Movement comes from NODES, not the unit. A squad attacks while moving only if it ADVANCES from a gated hold/ambush node to a waypoint - a plain "March to destination" is passive. Templates assert their declared Move/Wait bytes; adjust Squad orders afterward for manual tweaks.';
  }

  function builderDestLabel(dest) {
    if (!dest) return 'Pick on map';
    if (dest.siteName) return 'Dest: ' + dest.siteName;
    if (dest.selector != null) return 'Dest: site ' + dest.selector;
    return 'Dest: ' + dest.x.toFixed(1) + ', ' + dest.z.toFixed(1);
  }

  var EXTRA_KIND_NAMES = {
    1: 'Player enters area', 4: 'Player at site', 6: 'Mission event flag', 8: 'Unit in area',
    9: 'Squads-remaining threshold', 12: 'Site flag test', 19: 'Referenced-object check',
    24: 'Object state check', 25: 'Member-status check', 26: 'High-flag consume',
  };

  function siteName(rom, key, selector) {
    var site = (ensureState(rom).sites[key] || []).filter(function(s) { return s.selector === selector; })[0];
    return site ? (site.siteName || '').trim() || ('site ' + selector) : 'site ' + selector;
  }

  // Plain-English decode of a Section 3 trigger, with geometry info when location-based.
  function describeExtra(rom, key, extra) {
    var b = extra.bytes || [];
    var kind = extra.kind;
    var out = { id: extra.extraId, kind: kind, label: '', geometry: null, detail: '' };
    if (kind === 1 || kind === 8) {
      var lo = byteToWorld(calibrationData(key), b[2], b[3]);
      var hi = byteToWorld(calibrationData(key), b[4], b[5]);
      out.geometry = 'rect';
      out.label = (kind === 1 ? 'Player enters area' : 'Unit ' + b[6] + ' enters area');
      out.detail = lo && hi ? 'world x ' + lo.x.toFixed(1) + '..' + hi.x.toFixed(1) + ', z ' + lo.z.toFixed(1) + '..' + hi.z.toFixed(1) : 'byte rect ' + b[2] + ',' + b[3] + '..' + b[4] + ',' + b[5];
    } else if (kind === 4) {
      out.geometry = 'site';
      out.label = 'Player at ' + siteName(rom, key, b[6]);
      out.detail = 'latches when a player-flagged unit occupies site slot ' + b[6];
    } else if (kind === 9) {
      out.label = '≤ ' + b[6] + ' enemy squads remain';
      out.detail = 'remnant-count trigger (live count at 0x801F0FDE)';
    } else if (kind === 12) {
      // SCENE-RECORD space (shared with kind-1 subtype-1 waypoints): site = sceneMin + b[6].
      // The mapping rides ktenmainRecordIndex in the generated site data and is anchored by
      // two independent live in-game observations (see siteBySceneRecord).
      var recSite = siteBySceneRecord(rom, key, b[6]);
      var recName = recSite ? ((recSite.siteName || '').trim() || ('scene record #' + b[6])) : '';
      out.label = 'Site flag test (' + (recSite ? recName : 'scene record #' + b[6] + ', unresolved') + ')';
      out.detail = 'tests bit 0x0004 on the runtime site record for scene record ' + b[6] +
        (recSite ? ' = ' + recName : ' (beyond the selector-table sites)') +
        '; flag semantics partially open';
      out.geometry = !!recSite;
    } else {
      out.label = (EXTRA_KIND_NAMES[kind] || 'Kind ' + kind + ' (undecoded)');
      out.detail = 'payload ' + b.slice(2).map(function(v) { return OB64.scenarioCodec.hexByte(v); }).join(' ');
    }
    return out;
  }

  function nodeReferencesExtra(node, extraId) {
    var b = node.bytes || [];
    return b[10] === extraId ||
      (b[11] !== 0 && b[12] === extraId) ||
      ((b[13] !== 0 || b[14] !== 0) && b[14] === extraId);
  }

  // Which nodes gate on this extra, and which squads pass through those nodes.
  function extraConsumers(rom, key, model, extraId) {
    var nodes = model.section2.filter(function(n) { return nodeReferencesExtra(n, extraId); });
    var nodeIds = nodes.map(function(n) { return n.nodeId; });
    var refs = [];
    model.section1.forEach(function(row, idx) {
      if (!row.bytes) return;
      var chain = walkNodeChain(rom, key, model, row.bytes[6]);
      if (chain.some(function(h) { return nodeIds.indexOf(h.node.nodeId) >= 0; })) {
        refs.push({ sourceId: row.sourceId, edat: (((row.bytes[1] || 0) << 8) | (row.bytes[2] || 0)) - 1, rowIndex: idx });
      }
    });
    return { nodeIds: nodeIds, squadSourceIds: refs.map(function(r) { return r.sourceId; }), squadRefs: refs };
  }

  // Which squads use a node: it is their start node (Sec1 [+6]) or appears in their route chain.
  function nodeConsumers(rom, key, model, nodeId) {
    var refs = [];
    model.section1.forEach(function(row, idx) {
      if (!row.bytes) return;
      var chain = walkNodeChain(rom, key, model, row.bytes[6]);
      var uses = row.bytes[6] === nodeId || chain.some(function(h) { return h.node.nodeId === nodeId; });
      if (uses) refs.push({ sourceId: row.sourceId, edat: (((row.bytes[1] || 0) << 8) | (row.bytes[2] || 0)) - 1, rowIndex: idx, isStart: row.bytes[6] === nodeId });
    });
    return refs;
  }

  function rowSquadRef(row, idx) {
    return {
      sourceId: row && row.sourceId,
      edat: row && row.bytes ? ((((row.bytes[1] || 0) << 8) | (row.bytes[2] || 0)) - 1) : null,
      rowIndex: idx,
    };
  }

  function walkNodeChainModel(model, startNodeId) {
    var chain = [];
    var seen = {};
    var nodeId = startNodeId;
    for (var hop = 0; hop < 20; hop++) {
      if (!nodeId || nodeId === 0xFF || seen[nodeId]) break;
      var node = nodeById(model, nodeId);
      if (!node) break;
      seen[nodeId] = true;
      chain.push(node);
      nodeId = node.bytes ? node.bytes[17] : 0;
    }
    return chain;
  }

  function nodeConsumerRows(model, nodeId) {
    var refs = [];
    (model.section1 || []).forEach(function(row, idx) {
      if (!row.bytes) return;
      var chain = walkNodeChainModel(model, row.bytes[6]);
      var uses = row.bytes[6] === nodeId || chain.some(function(n) { return n.nodeId === nodeId; });
      if (uses) {
        var ref = rowSquadRef(row, idx);
        ref.isStart = row.bytes[6] === nodeId;
        refs.push(ref);
      }
    });
    return refs;
  }

  function nodesUsedBySquads(model, nodeIds) {
    var idSet = {};
    (nodeIds || []).forEach(function(id) { idSet[id] = true; });
    var refs = [];
    (model.section1 || []).forEach(function(row, idx) {
      if (!row.bytes) return;
      var chain = walkNodeChainModel(model, row.bytes[6]);
      var uses = idSet[row.bytes[6]] || chain.some(function(n) { return idSet[n.nodeId]; });
      if (uses) refs.push(rowSquadRef(row, idx));
    });
    return refs;
  }

  function otherSquadRefs(refs, rowIndex) {
    return (refs || []).filter(function(r) { return r.rowIndex !== rowIndex; });
  }

  function formatSquadRefs(refs) {
    return (refs || []).map(function(r) {
      return 'source ' + (r.sourceId != null ? r.sourceId : '?') + ' / EDAT ' + (r.edat != null ? r.edat : '?');
    }).join(', ');
  }

  function gateSummaryFromBytes(b) {
    if (!b || !b[10]) return '';
    var out = 'E' + b[10];
    if (b[11] === 2 && b[12]) out += ' AND E' + b[12];
    else if (b[11] === 3 && b[12]) out += ' OR E' + b[12];
    else if (b[11] === 1 && b[12]) out += ' ELSE E' + b[12];
    return out;
  }

  function applyStartGateReplacement(model, rowIndex, triggerId) {
    var row = model.section1[rowIndex];
    var startNode = row && nodeById(model, row.bytes[6]);
    if (!startNode) return { ok: false, message: 'No start node on this squad.' };
    var replaced = gateSummaryFromBytes(startNode.bytes);
    startNode.bytes[10] = triggerId ? (triggerId & 0xFF) : 0;
    startNode.bytes[11] = 0;
    startNode.bytes[12] = 0;
    return { ok: true, node: startNode, replaced: replaced };
  }

  // Per-squad route colors: line TYPE (solid/dashed) encodes gated-ness, COLOR identifies the
  // squad (keyed on its Section 1 row so it stays stable across renders and matches the roster).
  var ROUTE_COLORS = ['#4db0d2', '#e6a92e', '#d2564b', '#63c069', '#b072d6', '#e6863a', '#42c4a6',
    '#d76bab', '#a6c845', '#6a86e6', '#d8d05a', '#c88a5e', '#8fd0e0', '#8ad07a', '#e07a7a', '#b0a0e0'];
  function routeColor(i) { return ROUTE_COLORS[((i % ROUTE_COLORS.length) + ROUTE_COLORS.length) % ROUTE_COLORS.length]; }

  function polyline(svg, pts, zoom, color, dash, width) {
    if (pts.length < 2) return;
    var el = document.createElementNS('http://www.w3.org/2000/svg', 'polyline');
    el.setAttribute('points', pts.map(function(p) { return (p.x * zoom) + ',' + (p.y * zoom); }).join(' '));
    el.setAttribute('fill', 'none');
    el.setAttribute('stroke', color);
    el.setAttribute('stroke-width', width || 3);
    if (dash) el.setAttribute('stroke-dasharray', dash);
    el.setAttribute('stroke-linejoin', 'round');
    svg.appendChild(el);
  }

  function waypointDot(svg, p, zoom, node, selected) {
    var c = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    c.setAttribute('cx', p.x * zoom);
    c.setAttribute('cy', p.y * zoom);
    c.setAttribute('r', selected ? 8 : 6);
    c.setAttribute('fill', node && node.bytes && node.bytes[17] === 0xFF ? 'rgba(183,55,47,.9)' : 'rgba(73,176,210,.9)');
    c.setAttribute('stroke', 'rgba(20,16,10,.8)');
    c.setAttribute('stroke-width', '2');
    c.setAttribute('data-node-id', node ? node.nodeId : '');
    c.style.pointerEvents = 'auto';
    c.style.cursor = 'grab';
    svg.appendChild(c);
    return c;
  }

  function renderRouteLayer(inner, rom, key, cal, model, projection, zoom) {
    var svg = svgLayer(projection, zoom);
    pointsForAllRows(rom, key).forEach(function(point) {
      var row = model.section1[point.section1Row];
      if (!row || !row.bytes || !row.bytes[6]) return;
      var chain = walkNodeChain(rom, key, model, row.bytes[6]);
      if (!chain.length) return;
      var startWorld = rowWorld(rom, key, point.section1Row, point);
      var pts = [startWorld ? projection.worldToImage(startWorld.x, startWorld.z) : projection.pointToImage(point)];
      chain.forEach(function(hop) {
        if (hop.world) pts.push(projection.worldToImage(hop.world.x, hop.world.z));
      });
      var isSel = ui.selectedPoint === point.section1Row;
      // Line TYPE encodes gating: dashed = waits on a gate at its start node; solid = ungated,
      // marches immediately. Line COLOR identifies the squad (stable per Section 1 row).
      var startN = chain[0] && chain[0].node;
      var gated = !!(startN && startN.kind !== 1 && startN.bytes[10]);
      var color = routeColor(point.section1Row);
      if (isSel) polyline(svg, pts, zoom, 'rgba(245,210,98,.55)', gated ? '9 7' : null, 8); // selection halo
      polyline(svg, pts, zoom, color, gated ? '9 7' : null, isSel ? 4 : 3);
      // Waypoint handles for every coordinate node in this squad's chain (drag to move).
      chain.forEach(function(hop) {
        if (!hop.world) return;
        var p = projection.worldToImage(hop.world.x, hop.world.z);
        var dot = waypointDot(svg, p, zoom, hop.node, isSel);
        wireWaypointDrag(dot, rom, key, hop.node, projection, zoom);
        // Gate badge: waypoints that wait on a trigger show which one (E-id + operator).
        var gA = hop.node.bytes[10], gOp = hop.node.bytes[11], gB = hop.node.bytes[12];
        if (gA) {
          var glyph = 'E' + gA + (gOp === 2 && gB ? '&E' + gB : gOp === 3 && gB ? '|E' + gB : gOp === 1 && gB ? '?E' + gB : '');
          // Gates only BLOCK on kind-0/kind-2 nodes (live-verified key11: units marched past
          // gated kind-1 waypoints); kind-1 badges render dimmed as informational.
          var blocking = hop.node.kind !== 1;
          var gt = document.createElementNS('http://www.w3.org/2000/svg', 'text');
          gt.setAttribute('x', p.x * zoom + 9);
          gt.setAttribute('y', p.y * zoom - 8);
          gt.setAttribute('fill', blocking ? (isSel ? 'rgba(245,210,98,1)' : 'rgba(245,230,200,.92)') : 'rgba(245,230,200,.45)');
          gt.setAttribute('font-size', '11');
          gt.setAttribute('font-weight', '800');
          gt.setAttribute('paint-order', 'stroke');
          gt.setAttribute('stroke', 'rgba(20,16,10,.75)');
          gt.setAttribute('stroke-width', '2.5');
          gt.textContent = glyph;
          gt.style.pointerEvents = 'auto';
          gt.style.cursor = 'pointer';
          gt.addEventListener('click', function(ev) { ev.stopPropagation(); selectTrigger(gA); });
          svg.appendChild(gt);
        }
        if (hop.forkNodeId) {
          var forkNode = nodeById(model, hop.forkNodeId);
          var fw = nodeWorld(rom, key, forkNode);
          if (fw) {
            var fp = projection.worldToImage(fw.x, fw.z);
            polyline(svg, [p, fp], zoom, 'rgba(47,143,78,.85)', '3 5', 2);
          }
        }
      });
    });
    inner.appendChild(svg);
  }

  // Drag a coordinate waypoint: writes Section 2 bytes [3],[4] from the drop position.
  function wireWaypointDrag(dot, rom, key, node, projection, zoom) {
    dot.addEventListener('pointerdown', function(ev) {
      ev.preventDefault();
      ev.stopPropagation();
      var move = function(mv) {
        var inner = document.getElementById('sc-map-inner');
        var rect = inner.getBoundingClientRect();
        dot.setAttribute('cx', clamp(mv.clientX - rect.left, 0, rect.width));
        dot.setAttribute('cy', clamp(mv.clientY - rect.top, 0, rect.height));
      };
      var up = function(uv) {
        document.removeEventListener('pointermove', move);
        document.removeEventListener('pointerup', up);
        var inner = document.getElementById('sc-map-inner');
        var rect = inner.getBoundingClientRect();
        var world = projection.imageToWorld(
          clamp((uv.clientX - rect.left) / zoom, 0, projection.naturalWidth),
          clamp((uv.clientY - rect.top) / zoom, 0, projection.naturalHeight));
        // Dual waypoint encoding: snap to a site -> b3=0, b4=selector; else byte coordinates.
        var sites = ensureState(rom).sites[key] || [];
        var nearest = null, best = Infinity;
        sites.forEach(function(site) {
          var sp = projection.worldToImage(site.x, site.z);
          var d = Math.hypot(sp.x * zoom - (uv.clientX - rect.left), sp.y * zoom - (uv.clientY - rect.top));
          if (d < best) { best = d; nearest = site; }
        });
        if (node.bytes) {
          node.bytes[2] = 2; // drag normalizes the node to sub-2 selector/coordinate space
          if (nearest && best < SNAP_SCREEN_PX) {
            node.bytes[3] = 0; // sel = [4] - [3]: clear any prior offset so the snap reads back
            node.bytes[4] = nearest.selector & 0xFF;
            node.bytes[5] = 0;
          } else {
            var b = calibrationData(key) && calibrationData(key).boundsWorld;
            if (b) {
              node.bytes[4] = clamp(Math.round(((world.x - b.xMin) / Math.max(0.001, b.xMax - b.xMin)) * 256), 0, 255);
              node.bytes[5] = clamp(Math.round(((world.z - b.zMin) / Math.max(0.001, b.zMax - b.zMin)) * 256), 1, 255);
            }
          }
          var state = ensureState(rom);
          state.modifiedKeys[key] = true;
          changed();
        }
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
      document.addEventListener('pointermove', move);
      document.addEventListener('pointerup', up);
    });
  }

  // Byte->world decode is /256 (NOT /255): proven f32-exact against live object positions
  // read out of the running game (4/4 coordinates). /256 also makes byte->world->byte a
  // perfect identity, so mode switches and drag round-trips cannot drift a placement.
  function byteToWorld(cal, xb, zb) {
    var b = cal && cal.boundsWorld;
    if (!b) return null;
    return {
      x: b.xMin + (xb / 256) * (b.xMax - b.xMin),
      z: b.zMin + (zb / 256) * (b.zMax - b.zMin),
    };
  }

  // Real trigger geometry from decoded Section 3 payloads:
  // kinds 0x01 (player-in-rect) and 0x08 (unit-in-rect): byte rect [2]=xLo [3]=zLo [4]=xHi [5]=zHi.
  // kind 0x04 (player-at-site) and 0x0C (site-flag): payload byte [6] selects the subject site.
  // kind 0x09 (remnant count) has no geometry - shown in the detail panel only.
  // Floating feedback elements for map draw interactions.
  function mapGhost(inner, cls) {
    var el = document.createElement('div');
    el.className = cls;
    el.style.position = 'absolute';
    el.style.pointerEvents = 'none';
    el.style.zIndex = 60;
    inner.appendChild(el);
    return el;
  }

  // One-shot rect drawing on the map with a live rubber-band; writes byte-rect via cb.
  function drawRectOnMap(rom, key, onStatus, cb) {
    var inner = document.getElementById('sc-map-inner');
    if (!inner) { if (onStatus) onStatus('Map not available.', false); return; }
    if (onStatus) onStatus('Drag a rectangle on the map...', true);
    mapTool = 'rect';
    inner.style.cursor = 'crosshair';
    var proj = null;
    try { var cal = calibrationData(key); proj = projectionFor(cal, useImageFor(cal)); } catch (e) {}
    var zoom = ui.zoom;
    var start = null;
    var band = null;
    var move = function(ev) {
      if (!start || !band) return;
      var r = inner.getBoundingClientRect();
      var x = ev.clientX - r.left, y = ev.clientY - r.top;
      band.style.left = Math.min(start.x, x) + 'px';
      band.style.top = Math.min(start.y, y) + 'px';
      band.style.width = Math.abs(x - start.x) + 'px';
      band.style.height = Math.abs(y - start.y) + 'px';
    };
    var down = function(ev) {
      ev.preventDefault(); ev.stopPropagation();
      var r = inner.getBoundingClientRect();
      start = { x: ev.clientX - r.left, y: ev.clientY - r.top };
      band = mapGhost(inner, 'sc-rubber-band');
      band.style.left = start.x + 'px';
      band.style.top = start.y + 'px';
      inner.addEventListener('pointermove', move, true);
    };
    var up = function(ev) {
      inner.removeEventListener('pointerdown', down, true);
      inner.removeEventListener('pointerup', up, true);
      inner.removeEventListener('pointermove', move, true);
      inner.style.cursor = '';
      eatNextMapClick(inner); // the drag's release must not select the marker under it
      releaseMapTool();
      if (band) band.remove();
      if (!start || !proj) { if (onStatus) onStatus('Cancelled.', false); return; }
      var r = inner.getBoundingClientRect();
      var end = { x: ev.clientX - r.left, y: ev.clientY - r.top };
      var w1 = proj.imageToWorld(Math.min(start.x, end.x) / zoom, Math.min(start.y, end.y) / zoom);
      var w2 = proj.imageToWorld(Math.max(start.x, end.x) / zoom, Math.max(start.y, end.y) / zoom);
      var b1 = worldToBytePair(calibrationData(key), Math.min(w1.x, w2.x), Math.min(w1.z, w2.z));
      var b2 = worldToBytePair(calibrationData(key), Math.max(w1.x, w2.x), Math.max(w1.z, w2.z));
      cb([b1[0], b1[1], b2[0], b2[1]]);
    };
    inner.addEventListener('pointerdown', down, true);
    inner.addEventListener('pointerup', up, true);
  }

  function selectTrigger(extraId) {
    ui.selectedTrigger = ui.selectedTrigger === extraId ? null : extraId;
    ui.selectedPoint = null;
    ui.selectedSite = null;
    ui.selectedTreasure = null;
    ui.selectedNode = null;
    renderScenarioTab(document.getElementById('panel-scenario'));
  }

  function renderTriggerLayer(inner, rom, key, cal, model, projection, zoom) {
    var svg = svgLayer(projection, zoom);
    var sites = ensureState(rom).sites[key] || [];
    model.section3.forEach(function(extra) {
      var kind = extra.kind;
      var b = extra.bytes || [];
      var sel = ui.selectedTrigger === extra.extraId;
      var shape = null;
      if (kind === 1 || kind === 8) {
        var lo = byteToWorld(cal, b[2], b[3]);
        var hi = byteToWorld(cal, b[4], b[5]);
        if (!lo || !hi) return;
        var p1 = projection.worldToImage(lo.x, lo.z);
        var p2 = projection.worldToImage(hi.x, hi.z);
        var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        rect.setAttribute('x', Math.min(p1.x, p2.x) * zoom);
        rect.setAttribute('y', Math.min(p1.y, p2.y) * zoom);
        rect.setAttribute('width', Math.abs(p2.x - p1.x) * zoom);
        rect.setAttribute('height', Math.abs(p2.y - p1.y) * zoom);
        rect.setAttribute('rx', 3);
        rect.setAttribute('fill', kind === 1 ? 'rgba(183,55,47,' + (sel ? '.28' : '.13') + ')' : 'rgba(45,111,188,' + (sel ? '.28' : '.13') + ')');
        rect.setAttribute('stroke', sel ? 'rgba(245,210,98,.95)' : (kind === 1 ? 'rgba(183,55,47,.8)' : 'rgba(45,111,188,.8)'));
        rect.setAttribute('stroke-width', sel ? '4' : '2');
        svg.appendChild(rect);
        var tag = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        tag.setAttribute('x', Math.min(p1.x, p2.x) * zoom + 4);
        tag.setAttribute('y', Math.min(p1.y, p2.y) * zoom + 13);
        tag.setAttribute('fill', 'rgba(245,230,200,.95)');
        tag.setAttribute('font-size', '11');
        tag.setAttribute('font-weight', '800');
        tag.textContent = 'E' + extra.extraId + ' ' + describeExtra(rom, key, extra).label;
        svg.appendChild(tag);
        shape = rect;
      } else if (kind === 4 || kind === 12) {
        // kind 4 compares object +0x74 == b[6]-1, i.e. SELECTOR space; kind 12 uses
        // SCENE-RECORD space (both live-proven in-game). ktenmainRecordIndex maps scene
        // records to sites so both kinds can draw their target ring.
        var site = kind === 4
          ? (sites.filter(function(s) { return s.selector === b[6]; })[0] || null)
          : siteBySceneRecord(rom, key, b[6]);
        if (!site) return;
        var p = projection.worldToImage(site.x, site.z);
        var ring = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        ring.setAttribute('cx', p.x * zoom);
        ring.setAttribute('cy', p.y * zoom);
        ring.setAttribute('r', sel ? 32 : 28);
        ring.setAttribute('fill', 'none');
        ring.setAttribute('stroke', sel ? 'rgba(245,210,98,.95)' : 'rgba(245,210,98,.9)');
        ring.setAttribute('stroke-width', sel ? '5' : '3');
        ring.setAttribute('stroke-dasharray', '5 4');
        svg.appendChild(ring);
        shape = ring;
      }
      if (shape) {
        shape.setAttribute('data-extra-id', extra.extraId);
        shape.style.pointerEvents = 'auto';
        shape.style.cursor = 'pointer';
        shape.addEventListener('click', function(ev) {
          ev.stopPropagation();
          selectTrigger(extra.extraId);
        });
      }
    });
    inner.appendChild(svg);
  }

  function svgLayer(projection, zoom) {
    var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('class', 'sc-layer-svg');
    svg.setAttribute('width', projection.naturalWidth * zoom);
    svg.setAttribute('height', projection.naturalHeight * zoom);
    return svg;
  }

  function renderDetail(el, rom) {
    if (OB64.releaseSquadCompEditor) OB64.releaseSquadCompEditor(); // detach any stale embed host
    var key = ui.selectedKey;
    var model = modelFor(rom, key);
    var cal = calibrationData(key);
    if (!model) {
      el.innerHTML = '<div class="sc-warning">No ESET model.</div>';
      return;
    }
    if (ui.selectedTrigger != null && !model.section3.filter(function(x) { return x.extraId === ui.selectedTrigger; })[0]) {
      ui.selectedTrigger = null; // stale across scenario change
    }
    if (ui.selectedNode != null && !nodeById(model, ui.selectedNode)) {
      ui.selectedNode = null; // stale across scenario change
    }
    if (ui.selectedTreasure && !treasureSelected(rom, key)) {
      ui.selectedTreasure = null; // stale across scenario/archive change
    }
    if (ui.selectedTrigger != null) {
      renderTriggerDetail(el, rom, key, model, ui.selectedTrigger);
    } else if (ui.selectedTreasure) {
      var treas = treasureSelected(rom, key);
      if (treas) renderTreasureDetail(el, rom, key, treas);
      else renderScenarioOverview(el, rom, key, model, cal);
    } else if (ui.selectedSite) {
      renderSiteDetail(el, rom, key, ui.selectedSite);
    } else if (ui.selectedPoint != null) {
      renderSquadDetail(el, rom, key, ui.selectedPoint);
    } else if (ui.selectedNode != null) {
      renderNodeDetail(el, rom, key, model, ui.selectedNode);
    } else {
      renderScenarioOverview(el, rom, key, model, cal);
    }
  }

  function renderTreasureDetail(el, rom, key, selected) {
    var record = selected.record;
    var model = selected.model;
    var world = treasureWorldForKey(key, record);
    var shared = (model.runtimeKeys || []).filter(function(k) { return k !== key; });
    var html = backToOverviewHtml() + detailHead('Buried treasure', [
      treasureItemName(record),
      'gid ' + record.globalId + ' / archive ' + selected.archive,
      'x ' + record.x + ' / y ' + record.y,
    ]);
    html += '<div class="sc-section"><span class="sc-label">Reward</span>' +
      '<div class="sc-treasure-current">' +
        '<img src="' + esc(treasureItemIcon(record)) + '" alt="">' +
        '<div><strong>' + esc(treasureItemName(record)) + '</strong>' +
          '<span class="sc-sub" style="display:block">' + (record.table === 1 ? 'Equipment' : 'Special') +
          ' table ' + record.table + ' / item ' + record.itemId + '</span></div>' +
      '</div>' +
      '<button type="button" class="sc-inline-btn" id="sc-treasure-item" style="margin-top:8px">Change item</button>' +
    '</div>';
    html += '<div class="sc-section"><span class="sc-label">Position</span>' +
      '<label class="sc-label">Raw X/Y</label>' +
      '<div style="display:flex;gap:6px;align-items:center;margin:0 0 6px">' +
        '<input id="sc-treasure-x" type="number" min="0" max="255" value="' + record.x + '" style="width:64px">' +
        '<input id="sc-treasure-y" type="number" min="0" max="255" value="' + record.y + '" style="width:64px">' +
        '<button type="button" class="sc-inline-btn" id="sc-treasure-set" style="flex:0 0 auto">Set</button>' +
      '</div>' +
      '<div class="sc-sub">' + (world ? ('World: x ' + world.x.toFixed(3) + ' / z ' + world.z.toFixed(3)) : 'No calibrated bounds for this key.') + '</div>' +
      '<button type="button" class="sc-inline-btn" id="sc-treasure-move" style="margin-top:8px">Move on map</button>' +
    '</div>';
    html += '<div class="sc-section"><span class="sc-label">Source</span>' +
      '<div class="sc-sub">' + esc(model.filename || ('archive ' + selected.archive)) + ' / record ' + (selected.index + 1) + ' of ' + model.records.length + '</div>' +
      (shared.length ? '<div class="sc-sub">Shared by runtime keys: ' + shared.join(', ') + '</div>' : '') +
      '<button type="button" class="sc-inline-btn sc-danger" id="sc-treasure-delete" style="margin-top:8px">Remove treasure</button>' +
    '</div>';
    el.innerHTML = html;
    wireBackButton(el);
    var icon = el.querySelector('.sc-treasure-current img');
    if (icon) icon.onerror = function() { icon.style.visibility = 'hidden'; };
    var item = el.querySelector('#sc-treasure-item');
    if (item) item.onclick = function() { openTreasureItemPicker(rom, key, selected.archive, selected.index); };
    var set = el.querySelector('#sc-treasure-set');
    if (set) set.onclick = function() {
      var x = clamp(parseInt((el.querySelector('#sc-treasure-x') || {}).value, 10) || 0, 0, 255);
      var y = clamp(parseInt((el.querySelector('#sc-treasure-y') || {}).value, 10) || 0, 0, 255);
      record.x = x; record.y = y;
      commitTreasureEdit(rom, selected.archive, 'Treasure moved to x ' + x + ' / y ' + y + '.');
    };
    var move = el.querySelector('#sc-treasure-move');
    if (move) move.onclick = function() { beginPickTreasurePlacement(rom, key, selected.archive, selected.index); };
    var del = el.querySelector('#sc-treasure-delete');
    if (del) del.onclick = function() { deleteTreasure(rom, key, selected.archive, selected.index); };
  }

  function renderTriggerDetail(el, rom, key, model, extraId) {
    var extra = model.section3.filter(function(x) { return x.extraId === extraId; })[0];
    var d = describeExtra(rom, key, extra);
    var use = extraConsumers(rom, key, model, extraId);
    var b = extra.bytes;
    var sites = ensureState(rom).sites[key] || [];
    var html = backToOverviewHtml() + detailHead('Trigger E' + extraId, [
      d.label,
      'kind ' + extra.kind + (EXTRA_KIND_NAMES[extra.kind] ? ' - ' + EXTRA_KIND_NAMES[extra.kind] : ''),
      d.geometry ? 'location-based (highlighted on map)' : 'no map geometry',
    ]);
    html += '<div class="sc-section"><span class="sc-label">Meaning</span><div class="sc-sub">' + esc(d.label) + '</div>' +
      '<div class="sc-sub">' + esc(d.detail) + '</div></div>';
    // --- Editor ---
    html += '<div class="sc-section"><span class="sc-label">Edit trigger</span>' +
      '<div class="sc-form-row"><label class="sc-label">Kind</label><select id="sc-trig-kind">' +
      [[1, 'Player enters area'], [4, 'Player at site'], [8, 'Unit enters area'], [9, 'Squads-remaining threshold'], [12, 'Site flag test']]
        .map(function(k) { return option(String(k[0]), k[0] + ': ' + k[1], String(extra.kind)); }).join('') +
      (![1, 4, 8, 9, 12].includes(extra.kind) ? option(String(extra.kind), extra.kind + ': undecoded (edit raw bytes)', String(extra.kind)) : '') +
      '</select></div>';
    if (extra.kind === 1 || extra.kind === 8) {
      html += '<div class="sc-form-row"><label class="sc-label">Area</label>' +
        '<button type="button" id="sc-trig-redraw" class="sc-inline-btn">Redraw on map</button></div>';
      if (extra.kind === 8) {
        html += '<div class="sc-form-row"><label class="sc-label">Watched unit</label>' +
          '<input id="sc-trig-unit" type="number" min="0" max="255" value="' + b[6] + '"></div>';
      }
    } else if (extra.kind === 4) {
      html += '<div class="sc-form-row"><label class="sc-label">Site</label><select id="sc-trig-site">' +
        sites.map(function(s) { return option(String(s.selector), s.selector + ': ' + (s.siteName || '').trim(), String(b[6])); }).join('') +
        '</select></div>';
    } else if (extra.kind === 9) {
      html += '<div class="sc-form-row"><label class="sc-label">Threshold N</label>' +
        '<input id="sc-trig-n" type="number" min="1" max="30" value="' + b[6] + '"></div>';
    } else if (extra.kind === 12) {
      // Scene-record space, mapped through ktenmainRecordIndex (see siteBySceneRecord).
      var recSites = sites.filter(function(s) { return s.ktenmainRecordIndex != null; });
      var recMin = recSites.length ? Math.min.apply(null, recSites.map(function(s) { return s.ktenmainRecordIndex; })) : null;
      if (recMin != null) {
        var hasCurrent = recSites.some(function(s) { return s.ktenmainRecordIndex - recMin === b[6]; });
        html += '<div class="sc-form-row"><label class="sc-label">Site</label><select id="sc-trig-rec-sel">' +
          recSites.map(function(s) {
            var rel = s.ktenmainRecordIndex - recMin;
            return option(String(rel), rel + ': ' + (s.siteName || '').trim(), String(b[6]));
          }).join('') +
          (hasCurrent ? '' : option(String(b[6]), b[6] + ': (beyond the selector-table sites)', String(b[6]))) +
          '</select></div>';
      } else {
        html += '<div class="sc-form-row"><label class="sc-label">Scene record #</label>' +
          '<input id="sc-trig-rec" type="number" min="0" max="30" value="' + b[6] + '"></div>';
      }
    }
    html += '<div class="sc-form-row"><label class="sc-label">Raw payload</label><div class="sc-mini-grid" style="grid-template-columns:repeat(8,1fr)">' +
      b.slice(2).map(function(v, i) {
        return '<input class="sc-trig-raw" data-off="' + (i + 2) + '" value="' + hx2(v) + '" style="min-width:0">';
      }).join('') + '</div></div>';
    html += '</div>';
    html += '<div class="sc-section"><span class="sc-label">Used by</span>';
    if (use.nodeIds.length) {
      html += '<div class="sc-sub">Advance gate on node' + (use.nodeIds.length > 1 ? 's' : '') + ' ' + use.nodeIds.join(', ') + ': this trigger lets the node advance to its Next node; it does not activate the current node.</div>';
      if (use.squadRefs.length) {
        var points = pointsForAllRows(rom, key);
        var pointByRow = {}; points.forEach(function(p) { pointByRow[p.section1Row] = p; });
        html += '<div class="sc-sub" style="margin:6px 0 3px">Squads (' + use.squadRefs.length + '):</div>';
        use.squadRefs.forEach(function(r) {
          var point = pointByRow[r.rowIndex], srow = model.section1[r.rowIndex];
          var rec = point ? effectiveRecordFor(rom, key, point) : null;
          var leader = rec && rec[0] ? (OB64.className ? OB64.className(rec[0]) : '0x' + rec[0].toString(16)) : 'unknown';
          var icon = point ? liveLeaderIcon(rom, key, point) : null;
          var behavior = describeBehavior(rom, key, model, srow);
          var bar = (behavior && behavior.indexOf('Guard') !== 0 && behavior !== 'unknown') ? 'border-left:5px solid ' + routeColor(r.rowIndex) + ';' : '';
          html += '<button type="button" class="sc-squad-chip" data-row="' + r.rowIndex + '" style="' + bar + '">' +
            (icon ? '<img src="' + esc(icon) + '" alt="">' : '<span class="sc-chip-noicon"></span>') +
            '<span><strong>Source ' + esc(r.sourceId) + ' / EDAT ' + esc(r.edat) + '</strong>' +
            '<span class="sc-chip-sub">' + esc(leader) + '</span></span></button>';
        });
      } else {
        html += '<div class="sc-sub">No squads chain through these nodes.</div>';
      }
    } else {
      html += '<div class="sc-sub">No squad advance gates reference this trigger - it is an <strong>objective / event trigger</strong>: its condition (e.g. the player reaching this site) fires a story event, win/lose check, or <strong>cutscene</strong> rather than advancing a squad route. (Confirmed: key2 E3 = player-at-Ishro plays a cutscene.)</div>';
    }
    html += '</div>';
    // Delete: extra ids are stored in the record, so deletion never renumbers survivors.
    // An unreferenced trigger deletes directly; a referenced one deletes by first clearing
    // the gate bytes on every referencing node (those nodes become ungated = always pass).
    var refNodes = triggerRefNodes(model, extraId);
    html += '<div class="sc-section">';
    if (refNodes.length) {
      html += '<div class="sc-sub">Node' + (refNodes.length > 1 ? 's' : '') + ' ' + refNodes.join(', ') +
        ' advance gate' + (refNodes.length > 1 ? 's' : '') + ' on this trigger; deleting it clears those gates (the nodes advance ungated).</div>' +
        '<button type="button" class="sc-inline-btn sc-danger" id="sc-trig-delete" data-ungate="1">Delete trigger + clear advance gate on ' +
        refNodes.length + ' node' + (refNodes.length > 1 ? 's' : '') + '</button>';
    } else {
      html += '<button type="button" class="sc-inline-btn sc-danger" id="sc-trig-delete">Delete this trigger</button>';
    }
    html += '</div>';
    el.innerHTML = html;
    wireBackButton(el);
    // "Used by" squad cards jump to that squad's detail.
    el.querySelectorAll('.sc-squad-chip').forEach(function(a) {
      a.onclick = function(ev) {
        ev.preventDefault();
        ui.selectedPoint = parseInt(this.dataset.row, 10);
        ui.selectedSite = null;
        ui.selectedTrigger = null;
        ui.selectedTreasure = null;
        ui.selectedNode = null;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
    });

    var commitTrig = function() { ensureState(rom).modifiedKeys[key] = true; changed(); renderScenarioTab(document.getElementById('panel-scenario')); };
    var kindSel = el.querySelector('#sc-trig-kind');
    if (kindSel) kindSel.onchange = function() { extra.bytes[1] = parseInt(this.value, 10) & 0xFF; extra.kind = extra.bytes[1]; commitTrig(); };
    var redraw = el.querySelector('#sc-trig-redraw');
    if (redraw) redraw.onclick = function() {
      drawRectOnMap(rom, key, null, function(rect) {
        extra.bytes[2] = rect[0]; extra.bytes[3] = rect[1]; extra.bytes[4] = rect[2]; extra.bytes[5] = rect[3];
        commitTrig();
      });
    };
    var unit = el.querySelector('#sc-trig-unit');
    if (unit) unit.onchange = function() { extra.bytes[6] = parseInt(this.value, 10) & 0xFF; commitTrig(); };
    var siteSel = el.querySelector('#sc-trig-site');
    if (siteSel) siteSel.onchange = function() { extra.bytes[6] = parseInt(this.value, 10) & 0xFF; commitTrig(); };
    var n = el.querySelector('#sc-trig-n');
    if (n) n.onchange = function() { extra.bytes[6] = clamp(parseInt(this.value, 10) || 4, 1, 30); commitTrig(); };
    var rec = el.querySelector('#sc-trig-rec');
    if (rec) rec.onchange = function() { extra.bytes[6] = parseInt(this.value, 10) & 0xFF; commitTrig(); };
    var recSel = el.querySelector('#sc-trig-rec-sel');
    if (recSel) recSel.onchange = function() { extra.bytes[6] = parseInt(this.value, 10) & 0xFF; commitTrig(); };
    el.querySelectorAll('.sc-trig-raw').forEach(function(inp) {
      inp.onchange = function() {
        var v = parseByte(this.value);
        if (v == null) { this.value = hx2(extra.bytes[+this.dataset.off]); return; }
        extra.bytes[+this.dataset.off] = v;
        commitTrig();
      };
    });
    var del = el.querySelector('#sc-trig-delete');
    if (del) del.onclick = function() { deleteTrigger(rom, key, model, extra); };
  }

  // Nodes whose gate references this extra (extraA at [10]; extraB at [12] when an operator
  // is set — [10] doubles as the codec's section3Ref, so clearing the gate bytes fully
  // un-references the trigger).
  function triggerRefNodes(model, extraId) {
    return model.section2.filter(function(n) {
      return nodeReferencesExtra(n, extraId);
    }).map(function(n) { return n.nodeId; });
  }

  function liveNodeIdMap(model) {
    var out = {};
    (model.section2 || []).forEach(function(n) { out[n.nodeId] = true; });
    return out;
  }

  function extraRenumberMapAfterDelete(model, deleteIdx) {
    var map = {};
    (model.section3 || []).forEach(function(extra, i) {
      if (i === deleteIdx) return;
      map[extra.extraId] = i < deleteIdx ? i + 1 : i;
    });
    return map;
  }

  function planDeleteTrigger(model, extra) {
    // Keep Section 3 gapless on deletion. Retail never distinguishes scan-by-id from
    // (id-1)*10 arithmetic lookup, so gapped ids are reserved for a future live probe.
    var idx = model.section3.indexOf(extra);
    if (idx < 0) return { blocked: true, message: 'Trigger E' + (extra && extra.extraId) + ' is not in this model.' };
    var extraId = extra.extraId;
    var idMap = extraRenumberMapAfterDelete(model, idx);
    var nodeIds = liveNodeIdMap(model);
    var survivorTakesDeletedId = Object.keys(idMap).some(function(id) { return idMap[id] === extraId; });
    var ambiguous = [];
    model.section2.forEach(function(n) {
      var next = n.bytes[17] || 0;
      if (idMap[next] != null && idMap[next] !== next && nodeIds[next]) {
        ambiguous.push({ nodeId: n.nodeId, value: next });
      } else if (next === extraId && next !== 1 && !nodeIds[next] && survivorTakesDeletedId) {
        ambiguous.push({ nodeId: n.nodeId, value: next });
      }
    });
    if (ambiguous.length) {
      return {
        blocked: true,
        message: 'Cannot delete trigger E' + extraId + ' yet: Section-2 byte [17] on node ' +
          ambiguous.map(function(a) { return a.nodeId + ' (value ' + a.value + ')'; }).join(', ') +
          ' could mean either a next-node id or a shifted trigger id. Delete the highest-id trigger first, or clear/change that node [17] value before deleting E' + extraId + '.',
        ambiguous: ambiguous,
      };
    }
    var actions = [];
    model.section2.forEach(function(n) {
      var b = n.bytes;
      if (b[10] === extraId && b[11] !== 0 && b[12] && b[12] !== extraId) actions.push('node ' + n.nodeId + ': promote gate B to A');
      else if (b[10] === extraId) actions.push('node ' + n.nodeId + ': clear main gate');
      else if (b[11] !== 0 && b[12] === extraId) actions.push('node ' + n.nodeId + ': clear gate B only');
      if ((b[13] !== 0 || b[14] !== 0) && b[14] === extraId) actions.push('node ' + n.nodeId + ': clear extension gate term');
      [10, 12, 14, 17].forEach(function(off) {
        if ((off === 12 && b[11] === 0) || (off === 14 && b[13] === 0 && b[14] === 0)) return;
        if (idMap[b[off]] != null && idMap[b[off]] !== b[off]) actions.push('node ' + n.nodeId + ': remap byte [' + off + '] E' + b[off] + ' -> E' + idMap[b[off]]);
      });
    });
    return { blocked: false, extraId: extraId, index: idx, idMap: idMap, actions: actions };
  }

  function remapExtraByte(b, off, idMap) {
    if (idMap[b[off]] != null) b[off] = idMap[b[off]];
  }

  function applyDeleteTriggerPlan(model, plan) {
    var extraId = plan.extraId;
    var idMap = plan.idMap || {};
    model.section2.forEach(function(n) {
      var b = n.bytes;
      var op = b[11] || 0;
      var extraB = b[12] || 0;
      if (b[10] === extraId) {
        if (op !== 0 && extraB && extraB !== extraId) {
          b[10] = extraB;
          b[11] = 0;
          b[12] = 0;
        } else {
          b[10] = 0;
          b[11] = 0;
          b[12] = 0;
        }
      } else if (op !== 0 && extraB === extraId) {
        b[11] = 0;
        b[12] = 0;
      }
      if ((b[13] !== 0 || b[14] !== 0) && b[14] === extraId) {
        b[13] = 0;
        b[14] = 0;
      }
      remapExtraByte(b, 10, idMap);
      if (b[11] !== 0) remapExtraByte(b, 12, idMap);
      if (b[13] !== 0 || b[14] !== 0) remapExtraByte(b, 14, idMap);
      remapExtraByte(b, 17, idMap);
    });
    model.section3.splice(plan.index, 1);
    model.section3.forEach(function(extra, i) { extra.bytes[0] = i + 1; extra.extraId = i + 1; });
    syncStructuralOffsets(model);
    OB64.scenarioCodec.refreshDecodedRows(model);
    resetBuilderState();
    return model;
  }

  function liveExtraIdMap(model) {
    var out = {};
    (model.section3 || []).forEach(function(extra) { out[extra.extraId] = true; });
    return out;
  }

  function isNodeDomainValue(value) {
    return value >= 4 && value <= 0x13;
  }

  function planNodeGc(model) {
    var nodeIds = liveNodeIdMap(model);
    var extraIds = liveExtraIdMap(model);
    var referenced = {};
    (model.section1 || []).forEach(function(row) {
      var start = row.bytes && row.bytes[6];
      if (nodeIds[start]) referenced[start] = true;
    });
    (model.section2 || []).forEach(function(node) {
      var b = node.bytes || [];
      if (isNodeDomainValue(b[17]) && nodeIds[b[17]]) referenced[b[17]] = true;
      if (b[11] === 1 && nodeIds[b[16]]) referenced[b[16]] = true;
    });
    var removed = [];
    var survivors = [];
    (model.section2 || []).forEach(function(node) {
      if (referenced[node.nodeId]) survivors.push(node);
      else removed.push(node);
    });
    if (!survivors.length && removed.length) {
      return {
        blocked: true,
        changed: false,
        removed: removed.map(function(n) { return n.nodeId; }),
        idMap: {},
        message: 'Node cleanup skipped: Section 2 must keep at least one node row for this ESET format.',
      };
    }
    if (!removed.length) return { blocked: false, changed: false, removed: [], idMap: {} };
    var idMap = {};
    survivors.forEach(function(node, i) { idMap[node.nodeId] = 4 + i; });
    var ambiguous = [];
    survivors.forEach(function(node) {
      var next = node.bytes && node.bytes[17];
      if (idMap[next] != null && idMap[next] !== next && next >= 4 && next <= 0x10 && extraIds[next]) {
        ambiguous.push({ nodeId: node.nodeId, value: next, mapped: idMap[next] });
      }
    });
    if (ambiguous.length) {
      return {
        blocked: true,
        changed: false,
        removed: removed.map(function(n) { return n.nodeId; }),
        idMap: idMap,
        ambiguous: ambiguous,
        message: 'Node cleanup skipped: Section-2 [17] on node ' +
          ambiguous.map(function(a) { return a.nodeId + ' has overlap value ' + a.value; }).join(', ') +
          ', which is both a node id and a trigger id.',
      };
    }
    return {
      blocked: false,
      changed: true,
      removed: removed.map(function(n) { return n.nodeId; }),
      survivors: survivors,
      idMap: idMap,
      message: 'Node cleanup removed orphan node' + (removed.length === 1 ? '' : 's') + ' ' +
        removed.map(function(n) { return n.nodeId; }).join(', ') + '.',
    };
  }

  function remapNodeRefByte(b, off, idMap) {
    if (idMap[b[off]] != null) b[off] = idMap[b[off]];
  }

  function applyNodeGcPlan(model, plan) {
    if (!plan || plan.blocked || !plan.changed) return { changed: false, skipped: !!(plan && plan.blocked), message: plan && plan.message };
    var idMap = plan.idMap || {};
    (model.section1 || []).forEach(function(row) {
      if (row.bytes) remapNodeRefByte(row.bytes, 6, idMap);
    });
    var survivors = (plan.survivors || []).map(function(node) { return node; });
    survivors.forEach(function(node) {
      var b = node.bytes || [];
      remapNodeRefByte(b, 17, idMap);
      if (b[11] === 1) remapNodeRefByte(b, 16, idMap);
    });
    model.section2 = survivors;
    model.section2.forEach(function(node, i) {
      node.row = i;
      node.nodeId = 4 + i;
      node.bytes[0] = node.nodeId;
      node.kind = node.bytes[1];
    });
    syncStructuralOffsets(model);
    OB64.scenarioCodec.refreshDecodedRows(model);
    resetBuilderState();
    return { changed: true, skipped: false, removed: plan.removed || [], message: plan.message };
  }

  function runNodeGc(model) {
    var plan = planNodeGc(model);
    if (plan.blocked || !plan.changed) return { changed: false, skipped: !!plan.blocked, message: plan.message };
    return applyNodeGcPlan(model, plan);
  }

  function nodeKindName(k) {
    return k === 0 ? 'hold' : k === 1 ? 'waypoint' : k === 2 ? 'ambush' : 'kind ' + k;
  }

  // Node editor - mirrors renderTriggerDetail: edit the node's kind / orders / gate / next / raw
  // bytes, and list every squad that starts on or routes through it (clickable -> that squad).
  function renderNodeDetail(el, rom, key, model, nodeId) {
    var node = nodeById(model, nodeId);
    if (!node) { el.innerHTML = backToOverviewHtml() + '<div class="sc-warning">Node ' + nodeId + ' not found.</div>'; wireBackButton(el); return; }
    var b = node.bytes;
    var use = nodeConsumers(rom, key, model, nodeId);
    var nextTxt = b[17] === 0xFF ? 'terminal / camp' : (b[17] ? 'node ' + b[17] : 'none');
    var html = backToOverviewHtml() + detailHead('Node ' + nodeId, [
      nodeKindName(node.kind) + ' node',
      'next: ' + nextTxt,
      'used by ' + use.length + ' squad' + (use.length === 1 ? '' : 's'),
    ]);
    html += '<div class="sc-section"><span class="sc-label">Edit node</span>';
    html += '<div class="sc-form-row"><label class="sc-label">Kind</label><select id="sc-node-kind">' +
      [[0, 'Hold (carries Move/Wait orders)'], [1, 'Waypoint (march target)'], [2, 'Ambush (dormant until woken)']]
        .map(function(k) { return option(String(k[0]), k[0] + ': ' + k[1], String(node.kind)); }).join('') +
      ([0, 1, 2].indexOf(node.kind) < 0 ? option(String(node.kind), node.kind + ': undecoded (edit raw bytes)', String(node.kind)) : '') +
      '</select></div>';
    html += '<div class="sc-sub">Hold = sits at its post (Wait=Initiate makes it sally). Waypoint = a march destination. Ambush = hidden until woken. A unit becomes an <b>aggressive marcher</b> by ADVANCING from a hold/ambush node to a waypoint (set that waypoint as this node\'s Next).</div>';
    if (node.kind === 0) {
      var ag = orderAggro(b[3] & 0xFF);
      html += '<div class="' + ag.cls + '" style="margin-top:0' + ag.style + '"><strong>' + esc(ag.verb) + '</strong> &mdash; ' + ag.detail + '</div>' +
        '<div class="sc-form-row"><label class="sc-label">Wait</label>' + orderSelect('sc-node-wait', node.row, 3, b[3] & 0xFF, WAIT_NAMES, WAIT_BLURB) + '</div>' +
        '<div class="sc-form-row"><label class="sc-label">Move</label>' + orderSelect('sc-node-move', node.row, 2, b[2] & 0xFF, MOVE_NAMES, MOVE_BLURB) + '</div>';
    }
    html += '<div class="sc-form-row"><label class="sc-label" title="Condition required before this node advances to its Next node">Advance gate</label><select id="sc-node-gate">' +
      option('0', 'None (always pass)', String(b[10])) +
      model.section3.map(function(x) { return option(String(x.extraId), 'E' + x.extraId + ': ' + describeExtra(rom, key, x).label, String(b[10])); }).join('') +
      '</select></div>';
    html += '<div class="sc-form-row"><label class="sc-label">Next node</label><select id="sc-node-next">' +
      option('0', 'None (stop here)', String(b[17])) +
      option('255', 'Terminal / permanent camp (0xFF)', String(b[17])) +
      model.section2.filter(function(n) { return n.nodeId !== nodeId; })
        .map(function(n) { return option(String(n.nodeId), 'node ' + n.nodeId + ' (' + nodeKindName(n.bytes[1]) + ')', String(b[17])); }).join('') +
      '</select></div>';
    html += '<div class="sc-sub"><b>Advance gate means:</b> the squad is already using this node. When the trigger condition passes, this node may advance to <b>Next node</b>. It is not a trigger to activate this current node.</div>';
    if (node.kind === 1) {
      var w = nodeWorld(rom, key, node);
      var sites = ensureState(rom).sites[key] || [];
      var curSel = (b[5] === 0) ? (b[4] - b[3]) : null;
      var curVal = (curSel != null && curSel > 0) ? ('sel:' + curSel) : '';
      html += '<div class="sc-form-row"><label class="sc-label">March target</label><select id="sc-node-target">' +
        option('', '- set where this waypoint marches -', curVal) +
        sites.map(function(s) { return option('sel:' + s.selector, 'town ' + s.selector + ': ' + (s.siteName || '').trim(), curVal); }).join('') +
        '</select></div>' +
        '<div class="sc-form-row"><label class="sc-label">or click map</label><button type="button" id="sc-node-pick" class="sc-inline-btn">Pick target on map</button></div>' +
        '<label class="sc-label">or coordinate</label>' +
        '<div style="display:flex;gap:6px;align-items:center;margin:0 0 6px">' +
        '<input id="sc-node-tx" type="number" step="0.1" value="' + (w ? w.x.toFixed(1) : '') + '" placeholder="X" style="width:64px">' +
        '<input id="sc-node-tz" type="number" step="0.1" value="' + (w ? w.z.toFixed(1) : '') + '" placeholder="Z" style="width:64px">' +
        '<button type="button" id="sc-node-tset" class="sc-inline-btn" style="flex:0 0 auto">Set</button>' +
        '</div>' +
        '<div class="sc-sub">This waypoint marches its squad to: ' +
        (w ? (w.siteName ? '<b>' + esc(w.siteName) + '</b> (selector ' + w.selector + ')' : 'coordinate (' + w.x.toFixed(1) + ', ' + w.z.toFixed(1) + ')') :
             (b[5] === 0 ? 'selector ' + (b[4] - b[3]) + ' (no matching town / uncalibrated)' : 'coordinate (uncalibrated)')) +
        '. A new waypoint starts unset - pick a town or type X/Z (you can also drag its dot on the map once it is on a squad route).</div>';
    }
    html += '<div class="sc-form-row"><label class="sc-label">Raw bytes</label><div class="sc-mini-grid" style="display:grid;grid-template-columns:repeat(9,1fr);gap:3px">' +
      b.map(function(v, i) { return '<input class="sc-node-raw" data-off="' + i + '" value="' + hx2(v) + '" title="[+' + i + ']" style="min-width:0">'; }).join('') + '</div></div>';
    html += '</div>';
    html += '<div class="sc-section"><span class="sc-label">Used by</span>';
    if (use.length) {
      var points = pointsForAllRows(rom, key), pointByRow = {};
      points.forEach(function(p) { pointByRow[p.section1Row] = p; });
      html += '<div class="sc-sub">' + use.length + ' squad' + (use.length === 1 ? '' : 's') + ' start on or route through this node:</div>';
      use.forEach(function(r) {
        var point = pointByRow[r.rowIndex];
        var rec = point ? effectiveRecordFor(rom, key, point) : null;
        var leader = rec && rec[0] ? (OB64.className ? OB64.className(rec[0]) : '0x' + rec[0].toString(16)) : 'unknown';
        var icon = point ? liveLeaderIcon(rom, key, point) : null;
        html += '<button type="button" class="sc-squad-chip" data-row="' + r.rowIndex + '" style="border-left:5px solid ' + routeColor(r.rowIndex) + ';">' +
          (icon ? '<img src="' + esc(icon) + '" alt="">' : '<span class="sc-chip-noicon"></span>') +
          '<span><strong>Source ' + esc(r.sourceId) + ' / EDAT ' + esc(r.edat) + (r.isStart ? ' (start)' : '') + '</strong>' +
          '<span class="sc-chip-sub">' + esc(leader) + '</span></span></button>';
      });
    } else {
      html += '<div class="sc-sub">No squad starts on or routes through this node yet.</div>';
    }
    html += '</div>';
    el.innerHTML = html;
    wireBackButton(el);
    el.querySelectorAll('.sc-squad-chip').forEach(function(a) {
      a.onclick = function(ev) {
        ev.preventDefault();
        ui.selectedPoint = parseInt(this.dataset.row, 10);
        ui.selectedNode = null; ui.selectedSite = null; ui.selectedTrigger = null; ui.selectedTreasure = null;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
    });
    var kindSel = el.querySelector('#sc-node-kind');
    if (kindSel) kindSel.onchange = function() { b[1] = parseInt(this.value, 10) & 0xFF; commitScenarioEdit(rom, key); };
    var moveO = el.querySelector('#sc-node-move');
    if (moveO) moveO.onchange = function() { b[2] = parseInt(this.value, 10) & 0xFF; commitScenarioEdit(rom, key); };
    var waitO = el.querySelector('#sc-node-wait');
    if (waitO) waitO.onchange = function() { b[3] = parseInt(this.value, 10) & 0xFF; commitScenarioEdit(rom, key); };
    var gateSel = el.querySelector('#sc-node-gate');
    if (gateSel) gateSel.onchange = function() { b[10] = parseInt(this.value, 10) & 0xFF; commitScenarioEdit(rom, key); };
    var nextSel = el.querySelector('#sc-node-next');
    if (nextSel) nextSel.onchange = function() { b[17] = parseInt(this.value, 10) & 0xFF; commitScenarioEdit(rom, key); };
    // Waypoint march target: town selector (mirrors the map-drag: [4]=selector, [5]=0) or a world
    // coordinate projected through the map bounds ([4]=x, [5]=z, min 1).
    var tgtSel = el.querySelector('#sc-node-target');
    if (tgtSel) tgtSel.onchange = function() {
      if (this.value.indexOf('sel:') !== 0) return;
      b[2] = 2; // match the drag/map-pick: normalize to sub-2 selector/coordinate space
      b[4] = parseInt(this.value.slice(4), 10) & 0xFF; b[5] = 0; b[3] = 0;
      commitScenarioEdit(rom, key);
    };
    var tset = el.querySelector('#sc-node-tset');
    if (tset) tset.onclick = function() {
      var bw = calibrationData(key) && calibrationData(key).boundsWorld;
      var tx = parseFloat((el.querySelector('#sc-node-tx') || {}).value);
      var tz = parseFloat((el.querySelector('#sc-node-tz') || {}).value);
      if (!bw || isNaN(tx) || isNaN(tz)) return;
      b[2] = 2; // match the drag/map-pick: normalize to sub-2 selector/coordinate space
      b[4] = clamp(Math.round(((tx - bw.xMin) / Math.max(0.001, bw.xMax - bw.xMin)) * 256), 0, 255);
      b[5] = clamp(Math.round(((tz - bw.zMin) / Math.max(0.001, bw.zMax - bw.zMin)) * 256), 1, 255);
      commitScenarioEdit(rom, key);
    };
    var pickBtn = el.querySelector('#sc-node-pick');
    if (pickBtn) pickBtn.onclick = function() { beginPickWaypointTarget(rom, key, node); };
    el.querySelectorAll('.sc-node-raw').forEach(function(inp) {
      inp.onchange = function() {
        var v = parseByte(this.value);
        if (v == null) { this.value = hx2(b[+this.dataset.off]); return; }
        b[+this.dataset.off] = v;
        commitScenarioEdit(rom, key);
      };
    });
  }

  function deleteTrigger(rom, key, model, extra) {
    var extraId = extra.extraId;
    var plan = planDeleteTrigger(model, extra);
    if (plan.blocked) {
      ui.gateText = plan.message;
      if (window.alert) window.alert(plan.message);
      var panel = document.getElementById('panel-scenario');
      if (panel) renderScenarioTab(panel);
      return;
    }
    var refNodes = triggerRefNodes(model, extraId);
    var message = refNodes.length
      ? 'Delete trigger E' + extraId + '?\n\nNode' + (refNodes.length > 1 ? 's' : '') + ' ' + refNodes.join(', ') +
        ' will be updated safely. Gate A matches will clear the gate or promote gate B to A; gate B matches will clear only B; extension matches will clear only the extension term. Shifted trigger ids will be renumbered and references remapped.' +
        (plan.actions.length ? '\n\nPlanned edits:\n- ' + plan.actions.join('\n- ') : '')
      : 'Delete trigger E' + extraId + '? Nothing references it.';
    confirmThemed('Delete trigger', message, 'Delete trigger', function() {
      applyDeleteTriggerPlan(model, plan);
      ui.selectedTrigger = null;
      ui.gateText = 'Trigger E' + extraId + ' deleted. Remaining triggers renumbered and references remapped.';
      ensureState(rom).modifiedKeys[key] = true;
      changed();
      renderScenarioTab(document.getElementById('panel-scenario'));
    });
  }

  function renderScenarioOverview(el, rom, key, model, cal) {
    var validation = OB64.scenarioCodec.validateEset(model);
    var stubs = anyProjectStub(rom);
    var html = detailHead(displayLabel(key), [
      'runtime key ' + key,
      cal && cal.mapName ? cal.mapName : 'no map image',
      cal ? cal.registrationGrade : 'ungraded',
    ]);
    // Newcomer guide: the scenario model + the one non-obvious rule + copy-paste recipes. Collapsible
    // (native <details>), open by default so a first-time user is oriented before they touch anything.
    html += '<details class="sc-help" open style="border:1px solid var(--sc-line);border-radius:6px;padding:8px 10px;margin:0 0 12px;background:var(--sc-panel)">' +
      '<summary style="cursor:pointer;font-weight:800;color:var(--ob-ink)">❓ How scenarios work — start here</summary>' +
      '<div class="sc-sub" style="margin-top:8px;line-height:1.5">' +
        '<p style="margin:0 0 6px">A scenario is a set of enemy squads placed on a mission map. Pick a scenario key on the left, select a squad, then choose its <b>Behavior</b>. The map shows where each squad starts and any route or trigger logic attached to it.</p>' +
        '<p style="margin:0 0 3px"><b>Main pieces</b></p>' +
        '<ul style="margin:0 0 6px;padding-left:18px">' +
          '<li><b>Squad</b>: an enemy unit group. Its composition, formation, placement, and behavior are edited in the sidebar.</li>' +
          '<li><b>Node</b>: a route or behavior instruction. A node can make a squad hold position, march to a waypoint, wait behind an <b>advance gate</b>, spawn hidden as an ambush, or chain into another node through <b>Next</b>.</li>' +
          '<li><b>Trigger</b>: a condition such as the player reaching an area, a site flag changing, or the number of remaining enemy squads dropping. When a node uses a trigger, it gates the move to that node\'s <b>Next</b> node; it does not activate the current node.</li>' +
          '<li><b>Squad orders</b>: Move / Wait values stored on hold nodes. <b>Wait = Initiate</b> is the important one: it makes a standing squad attack nearby player units.</li>' +
        '</ul>' +
        '<p style="margin:0 0 3px"><b>Core rule</b></p>' +
        '<p style="margin:0 0 6px">A squad with <b>no node</b> is a stationary sentinel. It holds its spawn point and fights if reached, but it has no Move / Wait orders, so it cannot sally, chase, wait for triggers, or march.</p>' +
        '<p style="margin:0 0 6px">Anything active needs a node. A squad that should attack nearby units needs a hold node with <b>Wait = Initiate</b>. A squad that should move needs a waypoint node. A squad that should become aggressive while moving needs to advance from a hold or ambush node into a waypoint. A plain <b>March to destination</b> is passive: it walks to the destination and ignores the player on the way.</p>' +
        '<p style="margin:0 0 3px"><b>Common setups</b></p>' +
        '<ul style="margin:0;padding-left:18px">' +
          '<li><b>Guard</b>: holds position with no node. Best for simple town guards and filler garrisons.</li>' +
          '<li><b>Attacks anyone who comes near</b>: creates one hold node with <b>Wait = Initiate</b>. The squad stays put but sallies against nearby player units.</li>' +
          '<li><b>March to destination</b>: creates one waypoint node. The squad walks to the destination but does not actively intercept the player.</li>' +
          '<li><b>Wait for trigger, then advance</b>: creates a hold node whose trigger gates the move to its next waypoint. This is the structure used for aggressive triggered marchers.</li>' +
          '<li><b>Ambush</b>: creates a hidden ambush node whose trigger gates the move to its next node. If you set a destination, it wakes and moves there; without one, it wakes from the ambush point.</li>' +
          '<li><b>Reinforce when N squads remain</b>: creates a squads-remaining trigger and an ambush-style start node. Optionally give it a destination if the reinforcement should move after appearing.</li>' +
          '<li><b>March + permanent camp</b>: creates a waypoint node marked as a terminal camp. The squad moves there and stays.</li>' +
        '</ul>' +
        '<p style="margin:8px 0 0"><b>Node budget</b></p>' +
        '<p style="margin:0">Each scenario only has 16 nodes. Use node-free <b>Guard</b> squads for simple stationary enemies. If several squads should share the same route, trigger, or sally behavior, point them at the same <b>Start node</b> instead of creating duplicate nodes. The node editor’s <b>Used by</b> list shows every squad sharing that node.</p>' +
      '</div>' +
    '</details>';
    var fit = archiveFitInfo(rom, key);
    var units = predictedUnits(rom, key);
    html += '<div class="sc-meter-grid">' +
      meter(model.section1.length + '/' + OB64.scenarioCodec.DEFAULT_LIMITS.section1RowsMax, 'source rows') +
      meter(model.section2.length + '/16', 'nodes') +
      meter(model.section3.length + '/16', 'extras') +
      meter(fit ? fit.size + '/' + fit.slot + 'B' : 'n/a', 'archive fit') +
      meter(units != null ? units + '/100' : 'n/a', 'units at load') +
    '</div>';
    if (fit && !fit.fits) {
      html += '<div class="sc-ok">This mission has outgrown its original ROM slot (' + fit.size + 'B compressed vs ' + fit.slot + 'B). Export will relocate it to free ROM-tail space automatically (single fetch-window missions); if this mission does not qualify, export reports the exact limit.</div>';
    }
    if (units != null && units > 100) {
      html += '<div class="sc-warning">Predicted deployed units (' + units + ') exceed the 100-record table - reduce squad sizes or squad count.</div>';
    }
    html += validation.errors.length
      ? '<div class="sc-warning">Validation errors: ' + validation.errors.map(function(e) { return e.code; }).join(', ') + '</div>'
      : '<div class="sc-ok">Codec validation: zero errors, ' + validation.warnings.length + ' warnings</div>';
    if (stubs.siteAllegianceKeys.length) {
      html += '<div class="sc-ok">Town allegiance edits export to ROM: they rewrite the scincsv descriptor addend for the town. Several runtime keys can share one scincsv archive, so an edit here also moves that town in the keys that read the same descriptor.</div>';
    }
    var tModel = treasureModelForKey(rom, key);
    if (tModel) {
      html += '<div class="sc-section"><span class="sc-label">Buried treasure</span>' +
        '<div class="sc-sub">' + esc(tModel.filename || ('archive ' + tModel.archive)) + ' / ' + tModel.records.length + ' records' +
        (treasureArchiveModified(rom, tModel.archive) ? ' / modified' : '') + '</div>' +
        '<div class="sc-node-list">';
      tModel.records.forEach(function(record, i) {
        var selected = ui.selectedTreasure && ui.selectedTreasure.archive === tModel.archive && ui.selectedTreasure.index === i;
        html += '<div class="sc-treasure-row' + (selected ? ' on' : '') + '" data-treasure-index="' + i + '" role="button" tabindex="0">' +
          '<img src="' + esc(treasureItemIcon(record)) + '" alt="">' +
          '<span><strong>' + esc(treasureItemName(record)) + '</strong>' +
            '<span class="sc-sub" style="display:block">gid ' + record.globalId + ' / x ' + record.x + ' y ' + record.y + ' / ' +
            (record.table === 1 ? 'equipment' : 'special') + ' ' + record.itemId + '</span></span>' +
          '<button type="button" class="sc-inline-btn sc-danger sc-treasure-row-del" data-treasure-index="' + i + '">Delete</button>' +
        '</div>';
      });
      if (!tModel.records.length) html += '<div class="sc-node">No buried treasure records.</div>';
      html += '</div>' +
        '<button type="button" class="sc-inline-btn" id="sc-new-treasure" style="margin-top:8px">+ Add treasure</button>' +
      '</div>';
    } else {
      html += '<div class="sc-section"><span class="sc-label">Buried treasure</span><div class="sc-sub">No maizo file is mapped for this runtime key.</div></div>';
    }
    html += '<div class="sc-section"><span class="sc-label">Choreography nodes</span><div class="sc-node-list">';
    model.section2.forEach(function(node) {
      html += nodeSummaryHtml(model, node);
    });
    if (!model.section2.length) html += '<div class="sc-node">No Section 2 nodes.</div>';
    html += '</div>';
    // + Add node (mirrors + Add trigger): allocNode assigns nodeId = 4 + count, capped at 16. The
    // new node opens in the editor; point a squad at it via that squad's Start-node picker.
    html += '<div class="sc-form-row" style="grid-template-columns:minmax(0,1fr) auto;margin-top:6px">' +
      '<select id="sc-new-node-kind">' +
        '<option value="0">Hold node (Move/Wait orders)</option>' +
        '<option value="1">Waypoint node (march target)</option>' +
        '<option value="2">Ambush node (dormant until woken)</option>' +
      '</select>' +
      '<button type="button" class="sc-inline-btn" id="sc-new-node"' + (model.section2.length >= 16 ? ' disabled title="Section 2 is at its 16-node cap"' : '') + '>+ Add node</button></div>';
    html += '</div>';
    html += '<div class="sc-section"><span class="sc-label">Triggers</span><div class="sc-node-list">';
    model.section3.forEach(function(extra) {
      var d = describeExtra(rom, key, extra);
      html += '<div class="sc-trigger-row' + (ui.selectedTrigger === extra.extraId ? ' on' : '') + '" data-extra="' + extra.extraId + '" role="button" tabindex="0">' +
        '<strong>E' + extra.extraId + '</strong> ' + esc(d.label) +
        (d.geometry ? ' <span class="sc-chip">on map</span>' : '') +
        '<button type="button" class="sc-inline-btn sc-danger sc-trig-row-del" data-extra="' + extra.extraId + '" title="Delete this trigger" style="float:right">Delete</button>' +
        '<span class="sc-sub">' + esc(d.detail) + '</span></div>';
    });
    if (!model.section3.length) html += '<div class="sc-node">No Section 3 stream.</div>';
    // Kind-FIRST creation: pick the trigger type, then only geometric kinds enter draw mode;
    // static kinds (site / threshold / record) create immediately and open their editor.
    html += '<div class="sc-form-row" style="grid-template-columns:minmax(0,1fr) auto;margin-top:6px">' +
      '<select id="sc-new-trig-kind">' +
        '<option value="1">Player enters area (draw on map)</option>' +
        '<option value="8">Unit enters area (draw on map)</option>' +
        '<option value="4">Player at site</option>' +
        '<option value="9">Squads-remaining threshold</option>' +
        '<option value="12">Site flag test</option>' +
      '</select>' +
      '<button type="button" class="sc-inline-btn" id="sc-new-trigger">+ Add trigger</button></div>';
    html += '</div></div>';
    var addedHere = [];
    (ensureState(rom).addedSquads || []).forEach(function(r, i) { if (r.runtimeKey === key) addedHere.push({ r: r, i: i }); });
    if (addedHere.length) {
      html += '<div class="sc-section"><span class="sc-label">Added squads</span><div class="sc-node-list">';
      addedHere.forEach(function(item) {
        var r = item.r;
        var placed = r.section1Row != null;
        html += '<div class="sc-node">' + (placed ? 'placed' : 'legacy reserved (not on map)') +
          ' / source ' + esc(r.sourceId) + ' / edat ' + esc(r.edatId) + ' ' +
          '<button type="button" class="sc-inline-btn sc-added-row" data-idx="' + item.i + '" data-row="' + (placed ? r.section1Row : '') + '">' +
          (placed ? 'Select' : 'Remove') + '</button></div>';
      });
      html += '</div></div>';
    }
    el.innerHTML = html;
    el.querySelectorAll('.sc-treasure-row').forEach(function(row) {
      row.onclick = function(ev) {
        if (ev.target.classList && ev.target.classList.contains('sc-treasure-row-del')) return;
        var modelNow = treasureModelForKey(rom, key);
        if (!modelNow) return;
        ui.selectedTreasure = { archive: modelNow.archive, index: parseInt(this.dataset.treasureIndex, 10) };
        clearOtherSelectionsForTreasure();
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
      var img = row.querySelector('img');
      if (img) img.onerror = function() { img.style.visibility = 'hidden'; };
    });
    el.querySelectorAll('.sc-treasure-row-del').forEach(function(btn) {
      btn.onclick = function(ev) {
        ev.stopPropagation();
        var modelNow = treasureModelForKey(rom, key);
        if (!modelNow) return;
        deleteTreasure(rom, key, modelNow.archive, parseInt(this.dataset.treasureIndex, 10));
      };
    });
    var addTreasure = el.querySelector('#sc-new-treasure');
    if (addTreasure) addTreasure.onclick = function() { beginAddTreasurePlacement(rom, key); };
    el.querySelectorAll('.sc-trigger-row:not(.sc-node-row)').forEach(function(btn) {
      btn.onclick = function(ev) {
        if (ev.target.classList && ev.target.classList.contains('sc-trig-row-del')) return;
        selectTrigger(parseInt(this.dataset.extra, 10));
      };
    });
    el.querySelectorAll('.sc-node-row').forEach(function(btn) {
      btn.onclick = function() { selectNode(parseInt(this.dataset.nodeId, 10)); };
    });
    el.querySelectorAll('.sc-trig-row-del').forEach(function(btn) {
      btn.onclick = function(ev) {
        ev.stopPropagation();
        var id = parseInt(this.dataset.extra, 10);
        var extra = model.section3.filter(function(x) { return x.extraId === id; })[0];
        if (extra) deleteTrigger(rom, key, model, extra);
      };
    });
    el.querySelectorAll('.sc-added-row').forEach(function(btn) {
      btn.onclick = function() {
        if (this.dataset.row !== '') {
          ui.selectedPoint = parseInt(this.dataset.row, 10);
          ui.selectedSite = null;
          ui.selectedTrigger = null;
          ui.selectedTreasure = null;
          ui.selectedNode = null;
        } else {
          ensureState(rom).addedSquads.splice(parseInt(this.dataset.idx, 10), 1);
          changed();
        }
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
    });
    var newNode = el.querySelector('#sc-new-node');
    if (newNode) newNode.onclick = function() {
      var node = allocNode(model, { kind: parseInt((el.querySelector('#sc-new-node-kind') || {}).value, 10) || 0 });
      if (!node) return; // at the 16-node cap
      ui.selectedNode = node.nodeId; // open the new node's editor
      ui.selectedPoint = null; ui.selectedSite = null; ui.selectedTrigger = null; ui.selectedTreasure = null;
      commitScenarioEdit(rom, key); // refreshes decoded fields (gate/next/raw18) + re-renders
    };
    var newTrig = el.querySelector('#sc-new-trigger');
    if (newTrig) newTrig.onclick = function() {
      if (model.section3.length >= 16) { ui.gateText = 'Section 3 is at its 16-extra cap.'; renderScenarioTab(document.getElementById('panel-scenario')); return; }
      var kind = parseInt((el.querySelector('#sc-new-trig-kind') || {}).value, 10) || 1;
      var finish = function(extra) {
        if (!extra) return;
        ensureState(rom).modifiedKeys[key] = true;
        changed();
        ui.selectedTrigger = extra.extraId; // open its editor for parameter tuning
        ui.selectedTreasure = null;
        ui.selectedNode = null;
        ui.selectedPoint = null;
        ui.selectedSite = null;
        renderScenarioTab(document.getElementById('panel-scenario'));
      };
      if (kind === 1 || kind === 8) {
        drawRectOnMap(rom, key, null, function(rect) { finish(allocExtra(model, kind, rect)); });
        var gate = document.getElementById('sc-gate');
        if (gate) gate.textContent = 'Drag a rectangle on the map for the new trigger area...';
        return;
      }
      // Static kinds create immediately with a sensible default parameter (payload[4] -> byte [6]).
      var sites = ensureState(rom).sites[key] || [];
      var param = kind === 4 ? ((sites[0] && sites[0].selector) || 1) : (kind === 9 ? 4 : 1);
      finish(allocExtra(model, kind, [0, 0, 0, 0, param]));
    };
  }

  function nodeSummaryHtml(model, node) {
    var op = node.gate.operatorName;
    var target = node.nextNode === 0xFF ? 'terminal 0xFF' : ('next ' + node.nextNode);
    var extraA = extraName(model, node.gate.extraA);
    var extraB = node.gate.operator ? extraName(model, node.gate.extraB) : '';
    return '<div class="sc-trigger-row sc-node-row' + (ui.selectedNode === node.nodeId ? ' on' : '') + '" data-node-id="' + node.nodeId + '" role="button" tabindex="0">' +
      '<strong>node ' + node.nodeId + '</strong> ' + esc(nodeKindName(node.kind)) +
      (node.kind === 0 && node.bytes[3] === 1 ? ' <span class="sc-chip">aggro</span>' : '') +
      '<span class="sc-sub">advance gate: ' + esc(extraA + (extraB ? ' ' + op + ' ' + extraB : '')) + ' &rarr; ' + esc(target) +
      ' &middot; <code>' + esc(node.raw18) + '</code></span></div>';
  }

  function extraName(model, id) {
    if (!id) return 'none';
    var extra = model.section3.filter(function(e) { return e.extraId === id; })[0];
    return extra ? ('extra ' + id + ' kind ' + extra.kind) : ('extra ' + id);
  }

  function siteAllegianceReason(site, allegiance, intent) {
    var desc = site.siteDescriptor || {};
    if (intent === 'enemy' || intent === 'neutral' || intent === 'allied') {
      return intent.toUpperCase() + ' (edit; exports to ROM by rewriting the scincsv descriptor addend).';
    }
    var file = desc.scincsvFilename || '?';
    var town = desc.ownKtenmainName || site.siteName || 'this site';
    var addend = desc.descriptorAddendHex || '0x????';
    if (!desc.scincsvFilename) {
      return 'ENEMY: no scincsv descriptor stream is selected for this scenario key (default enemy-held).';
    }
    if (!desc.descriptorPresent) {
      return 'ENEMY: ' + town + ' has no descriptor in scincsv ' + file + ' (default enemy-held).';
    }
    if (allegiance === 'allied') {
      return 'ALLIED: scincsv ' + file + ' descriptor for ' + town + ' has addend ' + addend + ' (bit 0x2000 set).';
    }
    if (allegiance === 'neutral') {
      return 'NEUTRAL: scincsv ' + file + ' descriptor for ' + town + ' has addend 0x0000.';
    }
    return 'ENEMY: scincsv ' + file + ' descriptor for ' + town + ' has addend ' + addend + ' (bit 0x2000 clear = enemy-held).';
  }

  function renderSiteDetail(el, rom, key, site) {
    var allegiance = siteAllegiance(rom, key, site.selector);
    var intent = (ensureState(rom).siteAllegiances[key] || {})[site.selector];
    var why = siteAllegianceReason(site, allegiance, intent);
    var desc = site.siteDescriptor || {};
    var html = backToOverviewHtml() + detailHead(site.siteName || ('Site ' + site.selector), [
      'selector ' + site.selector,
      'x ' + site.x.toFixed(3) + ' / z ' + site.z.toFixed(3),
      allegiance.toUpperCase() + (site.isObjective ? ' / objective-flagged' : ''),
    ]);
    html += '<div class="sc-section"><span class="sc-label">Initial allegiance</span>' +
      '<div class="sc-sub">' + esc(why) + '</div>';
    if (desc.scincsvFilename) {
      html += '<div class="sc-sub">Source: ' + esc(desc.scincsvFilename) +
        (desc.ownKtenmainRecordIndex === null || desc.ownKtenmainRecordIndex === undefined ? '' : (' / ktenmain rec ' + desc.ownKtenmainRecordIndex)) +
        (desc.descriptorPresent ? (' / addend ' + (desc.descriptorAddendHex || '0x????')) : ' / no descriptor') + '</div>';
    }
    var canAuthor = !!desc.descriptorPresent;
    html += '<div class="sc-form-row"><label class="sc-label">Allegiance</label><select id="sc-site-allegiance"' + (canAuthor ? '' : ' disabled') + '>' +
        option('', 'Static default (' + (site.initialAllegiance || 'enemy') + ')', intent || '') +
        option('allied', 'Allied', intent || '') +
        option('neutral', 'Neutral', intent || '') +
        option('enemy', 'Enemy', intent || '') +
      '</select></div>';
    if (canAuthor) {
      html += '<div class="sc-ok">Exports to ROM: rewrites the scincsv ' + esc(desc.scincsvFilename || '') +
        ' addend for this town (a tiny archive, outside the CRC window). Runtime keys that share this archive move together.</div>';
    } else {
      html += '<div class="sc-warning">This town has no scincsv descriptor, so its allegiance cannot be authored yet ' +
        '(that would need adding a new descriptor row). It defaults to enemy-held.</div>';
    }
    html += '</div>';
    el.innerHTML = html;
    wireBackButton(el);
    var sel = el.querySelector('#sc-site-allegiance');
    if (sel) sel.onchange = function() {
      if (this.value) setSiteAllegiance(rom, key, site.selector, this.value);
      else {
        var bucket = ensureState(rom).siteAllegiances[key] || {};
        delete bucket[site.selector];
        changed();
      }
      renderScenarioTab(document.getElementById('panel-scenario'));
    };
  }

  function renderSquadDetail(el, rom, key, rowIndex) {
    var model = modelFor(rom, key);
    var row = model.section1[rowIndex];
    if (!row) {
      el.innerHTML = '<div class="sc-warning">Selected row is not available.</div>';
      return;
    }
    var point = resolvePointForRow(rom, key, rowIndex);
    var validation = OB64.scenarioCodec.validateEset(model);
    var added = isAddedRow(rom, key, rowIndex);
    var dormant = rowIsDormant(rom, key, model, rowIndex);
    var html = backToOverviewHtml();
    html += detailHead('Source ' + row.sourceId + ' / EDAT ' + point.edat, [
      point.wikiSquad || 'runtime row ' + rowIndex,
      (added ? 'ADDED squad' : 'vanilla squad') + ' / ' + (dormant ? 'deploys dormant (ambush)' : 'deploys active'),
      'drop raw ' + OB64.scenarioCodec.hexByte(row.dropRaw, 4),
    ]);
    html += '<div class="sc-section"><span class="sc-label">Squad Comp</span>' +
      '<div id="sc-comp-host"></div></div>';
    html += '<div class="sc-section"><span class="sc-label">Placement</span>' + placementEditorHtml(rom, key, row, point) + '</div>';
    // Reflect the squad's CURRENT gate/threshold in the builder controls.
    var curStartNode = nodeById(model, row.bytes[6]);
    var curGate = curStartNode ? (curStartNode.bytes[10] || 0) : 0;
    var curGateStr = curGate ? String(curGate) : '';
    var curThresh = 4;
    var curGateIsThreshold = false;
    if (curGate) {
      var curExtra = model.section3.filter(function(x) { return x.extraId === curGate; })[0];
      if (curExtra && curExtra.kind === 9) {
        curThresh = curExtra.bytes[6] || 4;
        curGateIsThreshold = true;
      }
    }
    var bld = builderFor(key, rowIndex);
    var selTemplate = bld.template || '';
    var selTrigger = bld.trigger != null ? bld.trigger : curGateStr;
    var selThresh = bld.threshold != null ? bld.threshold : curThresh;
    html += '<div class="sc-section"><span class="sc-label">Behavior</span>' +
      '<div class="sc-sub">Now: <b>' + esc(describeBehavior(rom, key, model, row)) + '</b></div>' +
      '<div class="sc-form-row"><label class="sc-label">Set to</label><select id="sc-template">' +
      option('', '- pick a behavior to apply -', selTemplate) +
      option('guard-site', 'Guard - hold position (dumb, no node)', selTemplate) +
      option('guard-sally', 'Attacks anyone who comes near (stays put)', selTemplate) +
      option('march-chain', 'March to destination (passive - ignores you)', selTemplate) +
      option('wait-march', 'Wait for trigger, then advance to destination', selTemplate) +
      option('solo-ambush', 'Ambush - hidden until trigger, then advances', selTemplate) +
      option('reinforce-remnant', 'Reinforce when N squads remain', selTemplate) +
      option('camp-terminal', 'March + permanent camp', selTemplate) +
      '</select></div>' +
      '<div id="sc-tpl-help" class="sc-sub" style="margin-top:2px">' + esc(templateHelp(selTemplate)) + '</div>' +
      '<div class="sc-sub"><b>Trigger here is an advance gate:</b> it controls when the squad leaves its current start node for the next node, not when the current node activates.</div>' +
      '<div class="sc-form-row"><label class="sc-label" title="Condition required before this squad advances from its current start node to the next node">Advance trigger</label><select id="sc-tpl-trigger">' +
      option('', 'None', selTrigger) +
      model.section3.map(function(x) {
        return option(String(x.extraId), 'E' + x.extraId + ': ' + describeExtra(rom, key, x).label, selTrigger);
      }).join('') +
      option('new-rect', '+ New player rect (draw on map)', selTrigger) +
      '</select></div>' +
      '<div class="sc-form-row"><label class="sc-label">Destination</label>' +
      '<button type="button" id="sc-tpl-dest" class="sc-inline-btn">' +
      esc(builderDestLabel(bld.dest)) +
      '</button></div>' +
      // Threshold N = the parameter of the kind-9 squads-remaining predicate; only rendered
      // where it applies (the Reinforce template, or live-editing an existing kind-9 gate).
      (selTemplate === 'reinforce-remnant' || (!selTemplate && curGateIsThreshold)
        ? '<div class="sc-form-row"><label class="sc-label">Squads left ≤ N</label>' +
          '<input id="sc-tpl-threshold" type="number" min="1" max="30" value="' + selThresh + '"></div>'
        : '') +
      '<div class="sc-form-row"><label class="sc-label"></label>' +
      '<button type="button" id="sc-tpl-clear-route" class="sc-inline-btn">Remove route (guard / hold position)</button></div>' +
      '<div id="sc-tpl-msg" class="sc-sub" style="' + (bld.msgOk ? '' : 'color:var(--sc-red)') + '">' + esc(bld.msg || '') + '</div>' +
      '</div>';
    // Squad standing orders live on the START NODE (Section 2), NOT Section 1 [7]/[8]. For a kind-0
    // HOLD node, node byte [2] = Move order and byte [3] = Wait order; func_00121F38 copies them at
    // deploy to the live object's +0x91 (Move) / +0x92 (Wait) - the SAME enum the player's own units
    // use. Wait == 1 (Initiate) is the aggro/sally gate (break formation, seek & attack nearby player
    // squads). Decoded from the AI dispatcher/resolver - see docs/enemy-system.md "Enemy movement /
    // aggro AI". Editing the node affects EVERY squad that starts on it. (Section 1 [7]/[8] were
    // tested and do NOT drive aggro; the node orders do.) The headline/summary rebuild from the live
    // bytes on every edit because commitScenarioEdit -> renderScenarioTab re-renders the whole tab,
    // so the onchange handlers stay a plain byte write (no in-place DOM patch, no cross-closure call).
    var orderNode = nodeById(model, row.bytes[6]);
    html += '<div class="sc-section"><span class="sc-label">Squad orders</span>';
    // Start-node picker: reassign which Section-2 node this squad deploys on (Sec1 [+6]), or jump to
    // that node's editor. 1 = the "hold at spawn" sentinel; >=4 = a real node.
    html += '<div class="sc-form-row"><label class="sc-label">Start node</label><select id="sc-start-node">' +
      option('1', '1 - hold at spawn (sentinel, no node)', String(row.bytes[6])) +
      model.section2.map(function(n) {
        var kn = n.bytes[1] === 0 ? 'hold' : n.bytes[1] === 1 ? 'waypoint' : n.bytes[1] === 2 ? 'ambush' : 'kind ' + n.bytes[1];
        return option(String(n.nodeId), n.nodeId + ' - ' + kn + ' node', String(row.bytes[6]));
      }).join('') +
      '</select>' +
      (row.bytes[6] >= 4 && orderNode ? '<button type="button" id="sc-edit-node" class="sc-inline-btn" style="margin-left:6px">Edit node ' + orderNode.nodeId + ' &rsaquo;</button>' : '') +
      '</div>';
    if (orderNode && orderNode.bytes[1] === 0) {
      var mv = orderNode.bytes[2] & 0xFF, wt = orderNode.bytes[3] & 0xFF;
      var ag = orderAggro(wt);
      // 1) Unmissable aggro headline - color box driven purely by the Wait byte.
      html += '<div class="' + ag.cls + '" style="margin-top:0' + ag.style + '"><strong>' + esc(ag.verb) + '</strong> &mdash; ' + ag.detail + '</div>';
      // 2) Live one-line combined effect (Wait/idle clause + Move/travel clause).
      html += '<div class="sc-sub" style="margin-top:4px">' + esc(orderEffectSentence(mv, wt)) + '</div>';
      // 3) The two controls. Wait first (it is the sally gate). Each carries a mono byte-identity chip:
      //    source node id, the Section-2 offset it writes, raw value hex+dec, and the live-object offset.
      html += '<div class="sc-form-row"><label class="sc-label" title="What it does idle at its post - THE aggro/sally gate">Wait <span class="sc-sub" style="display:inline;font-weight:400">(idle)</span></label>' +
        orderSelect('sc-wait-order', orderNode.row, 3, wt, WAIT_NAMES, WAIT_BLURB) +
        '<div class="sc-sub" style="font-family:var(--ob-mono,monospace);margin-top:2px">node ' + orderNode.nodeId + ' [+3] ' + hx2(wt) + ' (' + wt + ') &rarr; live +0x92</div></div>';
      html += '<div class="sc-form-row"><label class="sc-label" title="How it travels once it IS engaging/advancing">Move <span class="sc-sub" style="display:inline;font-weight:400">(travel)</span></label>' +
        orderSelect('sc-move-order', orderNode.row, 2, mv, MOVE_NAMES, MOVE_BLURB) +
        '<div class="sc-sub" style="font-family:var(--ob-mono,monospace);margin-top:2px">node ' + orderNode.nodeId + ' [+2] ' + hx2(mv) + ' (' + mv + ') &rarr; live +0x91</div></div>';
      // 4) Context + scoped provisional note.
      html += '<div class="sc-sub">Same enum as your own units. Shared by <b>every</b> squad whose start node (Sec1 [+6]) = ' + orderNode.nodeId + '.</div>' +
        '<div class="sc-sub" style="opacity:.8"><i>Provisional:</i> the sally gate (Wait=Initiate) is player-menu-proven for the enum and data-corroborated across scenarios; enemy-side labels for Move 1/2 and Wait=Retreat, plus a cold-boot of a newly-authored Initiate garrison, are still pending in-game confirmation.</div>';
    } else if (row.bytes[6] === 1) {
      html += '<div class="sc-ok" style="margin-top:0"><strong>STATIC &mdash; holds where it spawns.</strong> &mdash; fights any player squad that reaches it, but never gives chase.</div>' +
        '<div class="sc-sub" style="font-family:var(--ob-mono,monospace)">Sec1 [+6] = ' + hx2(row.bytes[6]) + ' (1) &mdash; no-movement-script sentinel (no Section-2 node, no Move/Wait to set).</div>' +
        '<div class="sc-sub">Apply a hold/march behavior above to attach a kind-0 node whose Move/Wait orders become editable here.</div>';
    } else {
      var k = orderNode ? (orderNode.bytes[1] & 0xFF) : null;
      var kName = k === 1 ? 'waypoint' : (k === 2 ? 'ambush' : null);
      html += '<div class="sc-sub" style="font-family:var(--ob-mono,monospace)">Sec1 [+6] = ' + hx2(row.bytes[6]) + ' (' + (row.bytes[6] & 0xFF) + ') &rarr; ' +
        (orderNode ? 'node ' + orderNode.nodeId + ' [+1] kind ' + hx2(k) + ' (' + k + (kName ? ', ' + kName : '') + ')' : '<b>missing node</b>') + '</div>' +
        '<div class="sc-sub">Move/Wait orders ([+2]/[+3]) live only on a kind-0 <b>hold</b> node. This squad starts on a ' +
        (orderNode ? 'kind-' + k + (kName ? ' ' + kName : '') : 'missing') + ' node, so there are no standing orders to edit here. Route it through a hold node to expose the sally lever.</div>';
    }
    html += '</div>';
    html += '<div class="sc-section"><label><input type="checkbox" id="sc-advanced"' + (ui.advanced ? ' checked' : '') + '> Advanced</label></div>';
    html += ui.advanced ? advancedHtml(model, rowIndex) : nodePreviewHtml(model, row);
    if (added) {
      html += '<div class="sc-section"><div class="sc-sub">Added squad on donor record ' + point.edat +
        ' (verified unreferenced; comp applies via the per-scenario override at record-build time).</div>' +
        '<div class="sc-ok">Exports to ROM: the row splices into this mission\'s ESET and the comp rides the ' +
        'squad-override blob - both verified by cold-boot testing in Project64.</div>' +
        '<button type="button" class="sc-inline-btn sc-danger" id="sc-delete-added">Delete this added squad</button></div>';
    } else {
      html += '<div class="sc-section"><button type="button" class="sc-inline-btn sc-danger" id="sc-delete-squad" ' +
        'title="Remove this squad from the mission (the global enemy record is untouched)">Remove squad from mission</button></div>';
    }
    html += validation.errors.length
      ? '<div class="sc-warning">Validation errors: ' + validation.errors.map(function(e) { return e.code; }).join(', ') + '</div>'
      : '<div class="sc-ok">Codec validation: zero errors for current model</div>';
    el.innerHTML = html;
    wireSquadDetail(el, rom, key, rowIndex);
  }

  function placementEditorHtml(rom, key, row, point) {
    var sites = ensureState(rom).sites[key] || [];
    var mode = row.bytes[4] === 0 ? 'selector' : 'coordinate';
    var selector = mode === 'selector' ? row.bytes[3] : '';
    var world = rowWorld(rom, key, row.row, point) || point.world || { x: 0, z: 0 };
    var html = '<div class="sc-form-row"><label class="sc-label">Mode</label><select id="sc-placement-mode">' +
      option('selector', 'Site selector', mode) + option('coordinate', 'Coordinate', mode) + '</select></div>';
    html += '<div class="sc-form-row"><label class="sc-label">Site</label><select id="sc-placement-site">';
    html += '<option value="">Coordinate</option>';
    sites.forEach(function(site) {
      html += '<option value="' + site.selector + '"' + (String(selector) === String(site.selector) ? ' selected' : '') + '>' +
        esc(site.selector + ': ' + site.siteName) + '</option>';
    });
    html += '</select></div>';
    html += '<div class="sc-form-row"><label class="sc-label">World X</label><input id="sc-world-x" type="number" step="0.01" value="' + esc(world.x.toFixed(2)) + '"></div>';
    html += '<div class="sc-form-row"><label class="sc-label">World Z</label><input id="sc-world-z" type="number" step="0.01" value="' + esc(world.z.toFixed(2)) + '"></div>';
    return html;
  }

  function nodePreviewHtml(model, row) {
    var related = [];
    [row.startNode].concat(row.behaviorBytes || []).forEach(function(id) {
      model.section2.forEach(function(node) {
        if (node.nodeId === id) related.push(node);
      });
    });
    var html = '<div class="sc-section"><span class="sc-label">Linked nodes</span><div class="sc-node-list">';
    related.forEach(function(node) { html += nodeSummaryHtml(model, node); });
    if (!related.length) html += '<div class="sc-node">No linked node rows resolved.</div>';
    html += '</div></div>';
    return html;
  }

  function advancedHtml(model, rowIndex) {
    var row = model.section1[rowIndex];
    var html = '<div class="sc-section"><span class="sc-label">Section 1 raw row</span>' + rawGridHtml('s1', row.bytes, rowIndex, 's1') + '</div>';
    html += '<div class="sc-section"><span class="sc-label">Section 2 advance gates</span>' +
      '<div class="sc-sub">Gate bytes block the transition from this node to <b>Next</b>. They do not activate the current node.</div>' +
      '<table class="sc-table"><thead><tr><th>Node</th><th>Kind</th><th>A</th><th>Op</th><th>B</th><th>Next</th></tr></thead><tbody>';
    model.section2.forEach(function(node) {
      html += '<tr data-node="' + node.row + '"><td>' + node.nodeId + '</td><td>' + node.kind + '</td>' +
        '<td><input class="sc-node-byte" data-row="' + node.row + '" data-off="10" value="' + hx2(node.bytes[10]) + '"></td>' +
        '<td><select class="sc-node-byte" data-row="' + node.row + '" data-off="11">' +
          [0,1,2,3].map(function(op) { return '<option value="' + op + '"' + (node.bytes[11] === op ? ' selected' : '') + '>' + op + ' ' + OB64.scenarioCodec.GATE_OPERATORS[op].name + '</option>'; }).join('') +
        '</select></td>' +
        '<td><input class="sc-node-byte" data-row="' + node.row + '" data-off="12" value="' + hx2(node.bytes[12]) + '"></td>' +
        '<td><input class="sc-node-byte" data-row="' + node.row + '" data-off="17" value="' + hx2(node.bytes[17]) + '"></td></tr>';
    });
    html += '</tbody></table></div>';
    html += '<div class="sc-section"><span class="sc-label">Section 3 extras</span><div class="sc-node-list">';
    model.section3.forEach(function(extra) {
      html += '<div class="sc-node">extra ' + extra.extraId + ' kind <input class="sc-extra-byte" data-row="' + extra.row + '" data-off="1" value="' + hx2(extra.bytes[1]) + '"> ' +
        rawGridHtml('s3-' + extra.row, extra.bytes, extra.row, 's3', 'sc-extra-grid') + '</div>';
    });
    if (!model.section3.length) html += '<div class="sc-node">No extras.</div>';
    html += '</div></div>';
    return html;
  }

  function rawGridHtml(prefix, bytes, rowIndex, kind, extraClass) {
    var html = '<div class="sc-raw-grid' + (extraClass ? ' ' + esc(extraClass) : '') + '">';
    for (var i = 0; i < bytes.length; i++) {
      html += '<label class="sc-byte">' + i + '<input class="sc-byte-input" data-kind="' + esc(kind || 's1') + '" data-prefix="' + prefix + '" data-row="' + rowIndex + '" data-off="' + i + '" value="' + hx2(bytes[i]) + '"></label>';
    }
    return html + '</div>';
  }

  function wireSquadDetail(el, rom, key, rowIndex) {
    var model = modelFor(rom, key);
    var detailPoint = resolvePointForRow(rom, key, rowIndex);
    wireBackButton(el);
    var del = el.querySelector('#sc-delete-added');
    if (del) del.onclick = function() { deleteAddedSquad(rom, key, rowIndex); };
    var delVanilla = el.querySelector('#sc-delete-squad');
    if (delVanilla) delVanilla.onclick = function() { deleteSquadRow(rom, key, rowIndex); };
    // Embed the full Squads comp editor in the sidebar (override toggle, formation grid,
    // class pickers, drag cells) - renders live against the same override state.
    var compHost = el.querySelector('#sc-comp-host');
    if (compHost && OB64.renderSquadCompEditor) {
      OB64.renderSquadCompEditor(compHost, rom, key, detailPoint.edat);
    }
    var mode = el.querySelector('#sc-placement-mode');
    if (mode) mode.onchange = function() {
      var row = model.section1[rowIndex];
      if (this.value === 'selector') {
        var site = (ensureState(rom).sites[key] || [])[0];
        row.bytes[3] = site ? site.selector : row.bytes[3];
        row.bytes[4] = 0;
      } else {
        var w = rowWorld(rom, key, rowIndex, detailPoint) || { x: 0, z: 0 };
        setCoordinateBytesFromWorld(calibrationData(key), row, w.x, w.z);
      }
      commitScenarioEdit(rom, key);
    };
    var site = el.querySelector('#sc-placement-site');
    if (site) site.onchange = function() {
      var row = model.section1[rowIndex];
      if (this.value) {
        row.bytes[3] = parseInt(this.value, 10) & 0xFF;
        row.bytes[4] = 0;
        commitScenarioEdit(rom, key);
      }
    };
    var x = el.querySelector('#sc-world-x');
    var z = el.querySelector('#sc-world-z');
    function commitWorld() {
      var row = model.section1[rowIndex];
      setCoordinateBytesFromWorld(calibrationData(key), row, parseFloat(x.value), parseFloat(z.value));
      commitScenarioEdit(rom, key);
    }
    if (x) x.onchange = commitWorld;
    if (z) z.onchange = commitWorld;
    // Behavior builder is fully LIVE - there is no Apply button. Every template/trigger/
    // destination/threshold change re-applies immediately through the builder's owned
    // Section 2/3 slots; requirement gaps surface as hints without touching any bytes.
    // Form state lives in builderFor(key,rowIndex) so it SURVIVES the full re-render every
    // commit triggers; msg() persists the same way.
    var bld = builderFor(key, rowIndex);
    var msg = function(text, ok) {
      bld.msg = text || '';
      bld.msgOk = ok !== false;
      var m = el.querySelector('#sc-tpl-msg');
      if (m) { m.textContent = bld.msg; m.style.color = bld.msgOk ? '' : 'var(--sc-red)'; }
    };
    // Current gate/threshold seed the live apply when the form fields are untouched.
    var curNode0 = nodeById(model, model.section1[rowIndex].bytes[6]);
    var curGate0 = curNode0 ? (curNode0.bytes[10] || 0) : 0;
    var curThresh0 = 4;
    if (curGate0) {
      var curExtra0 = model.section3.filter(function(x) { return x.extraId === curGate0; })[0];
      if (curExtra0 && curExtra0.kind === 9) curThresh0 = curExtra0.bytes[6] || 4;
    }
    var liveApply = function() {
      if (!bld.template) return;
      var trigRaw = bld.trigger != null ? bld.trigger : (curGate0 ? String(curGate0) : '');
      var triggerId = trigRaw && trigRaw !== 'new-rect' ? (parseInt(trigRaw, 10) || 0) : 0;
      var applyParams = {
        trigger: triggerId,
        dest: bld.dest,
        threshold: bld.threshold != null ? bld.threshold : curThresh0,
        owned: bld.owned,
      };
      var err = applyTemplate(model, rowIndex, bld.template, calibrationData(key), applyParams);
      if (err) { msg(err, false); return; }
      if (applyParams.gcResult && applyParams.gcResult.changed) bld = builderFor(key, rowIndex);
      msg('Live: behavior updated.' + (applyParams.gcResult && applyParams.gcResult.message ? ' ' + applyParams.gcResult.message : ''), true);
      commitScenarioEdit(rom, key);
    };
    var tplSelEl = el.querySelector('#sc-template');
    if (tplSelEl) tplSelEl.onchange = function() {
      bld.template = this.value;
      var hp = el.querySelector('#sc-tpl-help');   // update the guidance immediately, even if apply errors (no re-render)
      if (hp) hp.textContent = templateHelp(this.value);
      if (!bld.template) { msg('', true); return; }
      liveApply();
    };
    var thresholdEl = el.querySelector('#sc-tpl-threshold');
    if (thresholdEl) thresholdEl.onchange = function() {
      bld.threshold = clamp(parseInt(this.value, 10) || 4, 1, 30);
      if (bld.template === 'reinforce-remnant') { liveApply(); return; }
      // No template selected: live-edit the CURRENT kind-9 gate's threshold byte.
      if (curGate0) {
        var ex = model.section3.filter(function(x) { return x.extraId === curGate0; })[0];
        if (ex && ex.kind === 9) {
          confirmIfSharedExtra(rom, key, model, rowIndex, ex.extraId, 'Edit shared trigger threshold',
            'Set E' + ex.extraId + ' threshold to ' + bld.threshold + '?',
            function() {
              ex.bytes[6] = bld.threshold & 0xFF;
              msg('E' + ex.extraId + ' threshold set to ' + bld.threshold + '.', true);
              commitScenarioEdit(rom, key);
            });
        }
      }
    };
    var destBtn = el.querySelector('#sc-tpl-dest');
    if (destBtn) destBtn.onclick = function() {
      var inner = document.getElementById('sc-map-inner');
      if (!inner) return;
      msg('Click the map to set the destination...', true);
      mapTool = 'pick';
      inner.style.cursor = 'crosshair';
      var ghost = mapGhost(inner, 'sc-pick-ghost');
      var follow = function(mv) {
        var r = inner.getBoundingClientRect();
        ghost.style.left = (mv.clientX - r.left) + 'px';
        ghost.style.top = (mv.clientY - r.top) + 'px';
      };
      inner.addEventListener('pointermove', follow, true);
      var once = function(ev) {
        inner.removeEventListener('pointerdown', once, true);
        inner.removeEventListener('pointermove', follow, true);
        inner.style.cursor = '';
        eatNextMapClick(inner); // the pick must not also select the marker under the cursor
        releaseMapTool();
        ev.preventDefault();
        ev.stopPropagation();
        var rect = inner.getBoundingClientRect();
        var cal = calibrationData(key);
        var proj = projectionFor(cal, useImageFor(cal));
        var zoom = ui.zoom;
        var imageX = clamp((ev.clientX - rect.left) / zoom, 0, proj.naturalWidth);
        var imageY = clamp((ev.clientY - rect.top) / zoom, 0, proj.naturalHeight);
        var world = proj.imageToWorld(imageX, imageY);
        // Snap the destination onto a town within the standard snap radius.
        var snapped = null;
        var best = Infinity;
        (ensureState(rom).sites[key] || []).forEach(function(site) {
          var p = proj.worldToImage(site.x, site.z);
          var d = Math.hypot(p.x - imageX, p.y - imageY);
          if (d < best) { best = d; snapped = site; }
        });
        if (snapped && best < SNAP_SCREEN_PX / Math.max(0.05, zoom)) {
          world = { x: snapped.x, z: snapped.z, selector: snapped.selector, siteName: (snapped.siteName || ('site ' + snapped.selector)).trim() };
        } else {
          snapped = null;
        }
        bld.dest = world;
        destBtn.textContent = builderDestLabel(world);
        ghost.classList.add('set');
        setTimeout(function() { ghost.remove(); }, 450);
        msg(snapped ? 'Destination set on ' + snapped.siteName + '.' : 'Destination set.', true);
        liveApply();
      };
      inner.addEventListener('pointerdown', once, true);
    };
    // Trigger changes are live in both modes: with a template selected they re-apply it;
    // with no template they re-gate the squad's CURRENT start node (byte [10]) directly.
    var trigSelEl = el.querySelector('#sc-tpl-trigger');
    if (trigSelEl) trigSelEl.onchange = function() {
      bld.trigger = this.value;
      var setCurrentStartGate = function(triggerId) {
        var startNode = nodeById(model, model.section1[rowIndex].bytes[6]);
        if (!startNode) { msg('No start node on this squad - pick a template first.', false); return; }
        var replaced = gateSummaryFromBytes(startNode.bytes);
        var action = (triggerId ? 'Set this squad\'s advance gate to E' + triggerId : 'Clear this squad\'s advance gate') +
          replacementClause(replaced) + '?';
        confirmIfSharedNode(rom, key, model, rowIndex, startNode, 'Edit shared advance gate', action, function() {
          var result = applyStartGateReplacement(model, rowIndex, triggerId);
          if (!result.ok) { msg(result.message, false); return; }
          bld.trigger = null; // baked in
          msg(startGateStatus(triggerId, result.replaced), true);
          commitScenarioEdit(rom, key);
        });
      };
      if (this.value === 'new-rect') {
        drawRectOnMap(rom, key, msg, function(rect) {
          if (bld.template) {
            var extra = allocExtra(model, 1, rect);
            if (!extra) { msg('Section 3 is at its 16-extra cap', false); return; }
            bld.trigger = String(extra.extraId);
            msg('E' + extra.extraId + ' created.', true);
            liveApply();
            return;
          }
          var startNodeA = nodeById(model, model.section1[rowIndex].bytes[6]);
          if (!startNodeA) {
            var loose = allocExtra(model, 1, rect);
            if (!loose) { msg('Section 3 is at its 16-extra cap', false); return; }
            msg('E' + loose.extraId + ' created - pick a template to use it.', false);
            commitScenarioEdit(rom, key);
            return;
          }
          var replaced = gateSummaryFromBytes(startNodeA.bytes);
          confirmIfSharedNode(rom, key, model, rowIndex, startNodeA, 'Edit shared advance gate',
            'Create a new player-rect trigger and set it as this squad\'s advance gate' + replacementClause(replaced) + '?',
            function() {
              var extra = allocExtra(model, 1, rect);
              if (!extra) { msg('Section 3 is at its 16-extra cap', false); return; }
              var result = applyStartGateReplacement(model, rowIndex, extra.extraId);
              if (!result.ok) { msg(result.message, false); return; }
              bld.trigger = null; // baked into the node; form reflects the new current gate
              msg('E' + extra.extraId + ' created and set as the advance gate' + replacementClause(result.replaced) + '.', true);
              commitScenarioEdit(rom, key);
            });
        });
        return;
      }
      if (bld.template) { liveApply(); return; }
      setCurrentStartGate(this.value ? (parseInt(this.value, 10) || 0) : 0);
    };
    var clearRoute = el.querySelector('#sc-tpl-clear-route');
    if (clearRoute) clearRoute.onclick = function() {
      model.section1[rowIndex].bytes[6] = 1; // +0xBA = 1: hold position, no route
      var gc = runNodeGc(model);
      bld.template = '';
      if (gc.changed) bld = builderFor(key, rowIndex);
      msg('Route removed - unit guards its position.' + (gc.message ? ' ' + gc.message : ''), true);
      commitScenarioEdit(rom, key);
    };
    var adv = el.querySelector('#sc-advanced');
    if (adv) adv.onchange = function() {
      ui.advanced = !!this.checked;
      renderScenarioTab(document.getElementById('panel-scenario'));
    };
    var moveO = el.querySelector('#sc-move-order');
    if (moveO) moveO.onchange = function() {
      model.section2[parseInt(this.dataset.nodeRow, 10)].bytes[parseInt(this.dataset.off, 10)] = parseInt(this.value, 10) & 0xFF;
      commitScenarioEdit(rom, key);
    };
    var waitO = el.querySelector('#sc-wait-order');
    if (waitO) waitO.onchange = function() {
      model.section2[parseInt(this.dataset.nodeRow, 10)].bytes[parseInt(this.dataset.off, 10)] = parseInt(this.value, 10) & 0xFF;
      commitScenarioEdit(rom, key);
    };
    var startNodeSel = el.querySelector('#sc-start-node');
    if (startNodeSel) startNodeSel.onchange = function() {
      model.section1[rowIndex].bytes[6] = parseInt(this.value, 10) & 0xFF;
      commitScenarioEdit(rom, key);
    };
    var editNode = el.querySelector('#sc-edit-node');
    if (editNode) editNode.onclick = function() { selectNode(model.section1[rowIndex].bytes[6]); };
    el.querySelectorAll('.sc-byte-input').forEach(function(inp) {
      inp.onchange = function() {
        var kind = this.dataset.kind || 's1';
        var row = parseInt(this.dataset.row, 10);
        var off = parseInt(this.dataset.off, 10);
        var target = kind === 's3' ? (model.section3[row] && model.section3[row].bytes) : (model.section1[row] && model.section1[row].bytes);
        if (!target) return;
        var v = parseByte(this.value);
        if (v == null) { this.value = hx2(target[off]); return; }
        target[off] = v;
        commitScenarioEdit(rom, key);
      };
    });
    el.querySelectorAll('.sc-node-byte').forEach(function(inp) {
      inp.onchange = function() {
        var row = parseInt(this.dataset.row, 10);
        var off = parseInt(this.dataset.off, 10);
        var target = model.section2[row] && model.section2[row].bytes;
        if (!target) return;
        var v = parseByte(this.value);
        if (v == null) { this.value = hx2(target[off]); return; }
        target[off] = v;
        commitScenarioEdit(rom, key);
      };
    });
    el.querySelectorAll('.sc-extra-byte').forEach(function(inp) {
      inp.onchange = function() {
        var row = parseInt(this.dataset.row, 10);
        var off = parseInt(this.dataset.off, 10);
        var target = model.section3[row] && model.section3[row].bytes;
        if (!target) return;
        var v = parseByte(this.value);
        if (v == null) { this.value = hx2(target[off]); return; }
        target[off] = v;
        commitScenarioEdit(rom, key);
      };
    });
  }

  function parseByte(value) {
    var s = String(value).trim();
    var v;
    if (/^0x[0-9a-f]+$/i.test(s)) v = parseInt(s, 16);
    else if (/^-?[0-9]+$/.test(s)) v = parseInt(s, 10);
    else return null;
    if (!Number.isFinite(v)) return null;
    return clamp(v, 0, 255) & 0xFF;
  }

  function hx2(value) {
    return '0x' + Number(value || 0).toString(16).toUpperCase().padStart(2, '0');
  }

  // ---- Enemy standing-order enums (shared with the player's own Move/Wait menu) ---------------
  // Node byte [2] = Move order (-> live object +0x91), byte [3] = Wait order (-> +0x92). Same enum
  // the player sets in-game. Wait == 1 (Initiate) is the aggro/sally gate. See docs/enemy-system.md
  // "Enemy movement / aggro AI". These are module-level so both renderSquadDetail and any caller can
  // reach them (the two functions do NOT share a closure).
  var MOVE_NAMES = ['Direct', 'Agressive', 'Evasion'];
  var WAIT_NAMES = ['Guard', 'Initiate', 'Retreat'];
  // Short per-option meanings shown inline in the dropdowns.
  var MOVE_BLURB = ['close straight in', '', 'avoid contact'];
  var WAIT_BLURB = ['hold post', 'seek & attack', 'flee'];

  // Aggro headline, driven purely by the Wait byte (node [3] -> live +0x92): Guard = green "holds",
  // Initiate = red "sallies" (the gate), Retreat = neutral "flees", anything else = raw/unverified.
  // Returns { cls, style, verb, detail } for the colored strip. detail is injected UNescaped by the
  // caller, so any & / < in it must already be HTML-encoded here.
  function orderAggro(wait) {
    var strip = ';border:1px solid var(--sc-line);background:var(--sc-soft);color:var(--ob-ink-soft);' +
      'border-radius:5px;padding:7px 8px;font-size:var(--ob-text-sm);line-height:1.35';
    if (wait === 0) return { cls: 'sc-ok', style: '', verb: 'AGGRO: OFF - HOLDS', detail: 'guards its post; fights on contact but never leaves' };
    if (wait === 1) return { cls: 'sc-warning', style: '', verb: 'AGGRO: ON - SALLIES', detail: 'breaks formation to seek &amp; attack nearby player squads' };
    if (wait === 2) return { cls: '', style: strip, verb: 'RETREATS', detail: 'holds, but actively avoids nearby player squads' };
    return { cls: '', style: strip, verb: 'RAW WAIT=' + hx2(wait), detail: 'non-standard Wait value; behavior unverified' };
  }

  // One-line plain-English effect = Wait/idle clause + Move/travel clause. The caller passes this
  // through esc(), so plain '&' is fine here.
  function orderEffectSentence(move, wait) {
    var w = wait === 0 ? 'Holds its post; fights only when a player squad reaches it'
      : wait === 1 ? 'SALLIES: breaks formation to seek and attack nearby player squads'
      : wait === 2 ? 'Holds its post but actively flees from player squads'
      : 'Unknown Wait value ' + hx2(wait) + ' (raw)';
    var m = move === 0 ? ', closing straight in on its target'
      : move === 1 ? ', striking then withdrawing (hit & run)'
      : move === 2 ? ', avoiding contact as it moves (evasion)'
      : ', with unknown Move value ' + hx2(move) + ' (raw)';
    return w + m + '.';
  }

  // Order dropdown: "N - Name (meaning)"; out-of-range current value shown as "0xNN / NN (raw)".
  // Module-level so both the squad editor and the node editor can build it (they don't share a
  // closure). data-node-row/data-off carry the Section-2 row + byte offset the handler writes.
  function orderSelect(id, nodeRow, off, cur, names, blurbs) {
    var opts = '';
    if (cur >= names.length) opts += '<option value="' + cur + '" selected>' + hx2(cur) + ' / ' + cur + ' (raw)</option>';
    for (var v = 0; v < names.length; v++) {
      var g = (blurbs && blurbs[v]) ? ' (' + blurbs[v] + ')' : '';
      opts += '<option value="' + v + '"' + (v === cur ? ' selected' : '') + '>' + v + ' - ' + esc(names[v] + g) + '</option>';
    }
    return '<select id="' + id + '" data-node-row="' + nodeRow + '" data-off="' + off + '">' + opts + '</select>';
  }

  function option(value, label, current) {
    return '<option value="' + esc(value) + '"' + (String(value) === String(current) ? ' selected' : '') + '>' + esc(label) + '</option>';
  }

  function detailHead(title, chips) {
    return '<div class="sc-detail-head"><div class="sc-head-title">' + esc(title) + '</div>' +
      '<div class="sc-sub">' + (chips || []).map(esc).join(' / ') + '</div></div>';
  }

  // Every element detail view (squad/site/trigger) opens with this; clicking empty map space
  // does the same thing.
  function backToOverviewHtml() {
    return '<button type="button" class="sc-inline-btn sc-back" id="sc-back-overview">&#8592; Scenario overview</button>';
  }

  function wireBackButton(el) {
    var b = el.querySelector('#sc-back-overview');
    if (b) b.onclick = clearSelection;
  }

  function meter(value, label) {
    return '<div class="sc-meter"><strong>' + esc(value) + '</strong><span>' + esc(label) + '</span></div>';
  }

  function commitScenarioEdit(rom, key) {
    var state = ensureState(rom);
    var model = state.models[key];
    OB64.scenarioCodec.refreshDecodedRows(model);
    state.modifiedKeys[key] = true;
    changed();
    renderScenarioTab(document.getElementById('panel-scenario'));
  }

  function setCoordinateBytesFromWorld(cal, row, x, z) {
    var b = cal && cal.boundsWorld ? cal.boundsWorld : { xMin: -16, xMax: 16, zMin: -16, zMax: 16 };
    var xb = Math.round(((x - b.xMin) / Math.max(0.001, b.xMax - b.xMin)) * 256);
    var zb = Math.round(((z - b.zMin) / Math.max(0.001, b.zMax - b.zMin)) * 256);
    row.bytes[3] = clamp(xb, 0, 255);
    row.bytes[4] = clamp(zb || 1, 1, 255);
  }

  // Snap-or-coordinate placement write (shared by marker drop and add-squad place): a site
  // within SNAP_SCREEN_PX screen px wins selector mode; otherwise coordinate bytes.
  function placementBytesFromImage(rom, key, row, imageX, imageY, projection) {
    var world = projection.imageToWorld(imageX, imageY);
    var sites = ensureState(rom).sites[key] || [];
    var nearest = null;
    var best = Infinity;
    sites.forEach(function(site) {
      var p = projection.worldToImage(site.x, site.z);
      var d = Math.hypot(p.x - imageX, p.y - imageY);
      if (d < best) { best = d; nearest = site; }
    });
    if (nearest && best < SNAP_SCREEN_PX / Math.max(0.05, ui.zoom)) {
      row.bytes[3] = nearest.selector & 0xFF;
      row.bytes[4] = 0;
    } else {
      setCoordinateBytesFromWorld(calibrationData(key), row, world.x, world.z);
    }
  }

  function updatePlacementFromImage(rom, key, rowIndex, imageX, imageY, projection) {
    var state = ensureState(rom);
    placementBytesFromImage(rom, key, state.models[key].section1[rowIndex], imageX, imageY, projection);
    state.modifiedKeys[key] = true;
    changed();
  }

  // After ANY structural change (row/node/extra count), the parse-time offset copy and the
  // final Section 1 row's alias tail ([16]=Section 2 count, [17]=first node id - the game's
  // section-2 locator, validated by 'final-row-section2-alias') both go stale. Re-sync them
  // so codec validation keeps reporting on the model the serializer will actually write.
  function syncStructuralOffsets(model) {
    var off = OB64.scenarioCodec.computeOffsets(model);
    model.offsets.section2 = off.section2Offset;
    model.offsets.section3 = off.section3Offset;
    model.section3Present = off.section3Present;
    if (model.section1.length && model.section2.length) {
      var fin = model.section1[model.section1.length - 1];
      fin.bytes[16] = model.section2.length & 0xFF;
      fin.bytes[17] = model.section2[0].nodeId & 0xFF;
    }
  }

  function writeNodeFields(node, fields) {
    fields = fields || {};
    var b = node.bytes;
    var kind = fields.kind || 0;
    b[1] = kind;
    if (kind === 1) {
      b[2] = fields.subtype != null ? (fields.subtype & 0xFF) : 0;
      b[3] = fields.selectorOffset != null ? (fields.selectorOffset & 0xFF) : 0;
    } else {
      b[2] = fields.moveOrder != null ? (fields.moveOrder & 0xFF) : 0;
      b[3] = fields.waitOrder != null ? (fields.waitOrder & 0xFF) : 0;
    }
    b[4] = 0;
    b[5] = 0;
    if (fields.coordBytes) {
      b[4] = fields.coordBytes[0] & 0xFF;
      b[5] = Math.max(1, fields.coordBytes[1] & 0xFF);
    }
    if (fields.siteSelector != null) {
      b[3] = 0;
      b[4] = fields.siteSelector & 0xFF;
      b[5] = 0;
    }
    b[10] = fields.gateExtra || 0;
    b[11] = fields.gateOp || 0;
    b[12] = fields.gateExtraB || 0;
    b[13] = 0;
    b[14] = 0;
    b[15] = 0;
    b[16] = fields.forkNode || 0;
    b[17] = fields.next != null ? fields.next : 0;
    node.kind = b[1];
    return node;
  }

  // Section 2/3 allocation. Hard caps are structural RAM layout: 16 nodes (ids 0x04..0x13),
  // 16 extras (ids 0x01..0x10). Returns null when the mission is at cap.
  // Section 3 runtime lookup is still scan-vs-arithmetic open because retail keeps
  // extraId == row+1. The editor preserves that invariant on delete/serialize until a
  // gapped-id live probe settles the runtime resolver.
  function allocNode(model, fields) {
    if (model.section2.length >= 16) return null;
    var nodeId = 4 + model.section2.length;
    var bytes = new Array(18).fill(0);
    bytes[0] = nodeId;
    var node = { nodeId: nodeId, kind: bytes[1], bytes: bytes };
    writeNodeFields(node, fields);
    model.section2.push(node);
    syncStructuralOffsets(model);
    return node;
  }

  function allocExtra(model, kind, payload) {
    if (model.section3.length >= 16) return null;
    var used = {};
    model.section3.forEach(function(extra) { used[extra.extraId] = true; });
    var extraId = null;
    for (var i = 1; i <= 16; i++) {
      if (!used[i]) { extraId = i; break; }
    }
    if (extraId == null) return null;
    var bytes = new Array(10).fill(0);
    bytes[0] = extraId;
    bytes[1] = kind;
    (payload || []).forEach(function(v, i) { bytes[2 + i] = v & 0xFF; });
    var extra = { extraId: extraId, kind: kind, bytes: bytes };
    model.section3.push(extra);
    syncStructuralOffsets(model);
    return extra;
  }

  function worldToBytePair(cal, x, z) {
    var b = cal && cal.boundsWorld ? cal.boundsWorld : { xMin: -16, xMax: 16, zMin: -16, zMax: 16 };
    return [
      clamp(Math.round(((x - b.xMin) / Math.max(0.001, b.xMax - b.xMin)) * 256), 0, 255),
      clamp(Math.round(((z - b.zMin) / Math.max(0.001, b.zMax - b.zMin)) * 256), 0, 255),
    ];
  }

  function treasureBytesFromImage(key, imageX, imageY, projection) {
    var world = projection.imageToWorld(imageX, imageY);
    return worldToBytePair(calibrationData(key), world.x, world.z);
  }

  function moveTreasureFromImage(rom, key, archive, index, imageX, imageY, projection) {
    var model = ensureState(rom).treasureArchives[archive];
    var record = model && model.records[index];
    if (!record) return;
    var pair = treasureBytesFromImage(key, imageX, imageY, projection);
    record.x = pair[0];
    record.y = pair[1];
    ui.selectedTreasure = { archive: archive, index: index };
    clearOtherSelectionsForTreasure();
    commitTreasureEdit(rom, archive, 'Treasure moved to x ' + record.x + ' / y ' + record.y + '.');
  }

  // Waypoint March target via a map click: mirrors the behavior-builder destination picker but
  // writes straight to the node's target bytes. Snap onto a town within the standard radius (town
  // selector: [4]=selector,[5]=0,[3]=0); otherwise a bounds-projected coordinate ([4]=x,[5]=z>=1).
  function beginPickWaypointTarget(rom, key, node) {
    var panel = document.getElementById('panel-scenario');
    var inner = document.getElementById('sc-map-inner');
    if (!inner) { ui.gateText = 'The map is not available for this scenario - use the X/Z inputs.'; renderScenarioTab(panel); return; }
    mapTool = 'pick';
    inner.style.cursor = 'crosshair';
    var gate = document.getElementById('sc-gate');
    if (gate) gate.textContent = 'Click the map to set this waypoint’s march target (Esc to cancel)...';
    var ghost = mapGhost(inner, 'sc-pick-ghost');
    var follow = function(mv) {
      var r = inner.getBoundingClientRect();
      ghost.style.left = (mv.clientX - r.left) + 'px';
      ghost.style.top = (mv.clientY - r.top) + 'px';
    };
    var cleanup = function() {
      inner.removeEventListener('pointerdown', once, true);
      inner.removeEventListener('pointermove', follow, true);
      document.removeEventListener('keydown', onKey, true);
      inner.style.cursor = '';
      ghost.remove();
      releaseMapTool();
    };
    var onKey = function(ev) {
      if (ev.key !== 'Escape') return;
      cleanup();
      ui.gateText = 'March target unchanged.';
      renderScenarioTab(panel);
    };
    var once = function(ev) {
      cleanup();
      ev.preventDefault();
      ev.stopPropagation();
      eatNextMapClick(inner);
      var rect = inner.getBoundingClientRect();
      var cal = calibrationData(key);
      var proj = projectionFor(cal, useImageFor(cal));
      var imageX = clamp((ev.clientX - rect.left) / ui.zoom, 0, proj.naturalWidth);
      var imageY = clamp((ev.clientY - rect.top) / ui.zoom, 0, proj.naturalHeight);
      var world = proj.imageToWorld(imageX, imageY);
      var snapped = null, best = Infinity;
      (ensureState(rom).sites[key] || []).forEach(function(site) {
        var p = proj.worldToImage(site.x, site.z);
        var d = Math.hypot(p.x - imageX, p.y - imageY);
        if (d < best) { best = d; snapped = site; }
      });
      var b = node.bytes;
      b[2] = 2; // match wireWaypointDrag: normalize the node to sub-2 selector/coordinate space
      if (snapped && best < SNAP_SCREEN_PX / Math.max(0.05, ui.zoom)) {
        b[4] = snapped.selector & 0xFF; b[5] = 0; b[3] = 0;
        ui.gateText = 'March target set on ' + (snapped.siteName || ('town ' + snapped.selector)) + '.';
      } else {
        var bw = cal && cal.boundsWorld;
        if (!bw) { ui.gateText = 'This scenario has no world bounds - use the X/Z inputs.'; renderScenarioTab(panel); return; }
        b[4] = clamp(Math.round(((world.x - bw.xMin) / Math.max(0.001, bw.xMax - bw.xMin)) * 256), 0, 255);
        b[5] = clamp(Math.round(((world.z - bw.zMin) / Math.max(0.001, bw.zMax - bw.zMin)) * 256), 1, 255);
        ui.gateText = 'March target set to (' + world.x.toFixed(1) + ', ' + world.z.toFixed(1) + ').';
      }
      commitScenarioEdit(rom, key);
    };
    inner.addEventListener('pointermove', follow, true);
    inner.addEventListener('pointerdown', once, true);
    document.addEventListener('keydown', onKey, true);
  }

  function beginPickTreasurePlacement(rom, key, archive, index) {
    var inner = document.getElementById('sc-map-inner');
    if (!inner) { ui.gateText = 'Map is not available for this scenario.'; renderScenarioTab(document.getElementById('panel-scenario')); return; }
    var record = (ensureState(rom).treasureArchives[archive] || {}).records[index];
    if (!record) return;
    mapTool = 'pick';
    inner.style.cursor = 'crosshair';
    var gate = document.getElementById('sc-gate');
    if (gate) gate.textContent = 'Click the map to move this treasure...';
    var ghost = mapGhost(inner, 'sc-marker sc-treasure-marker sc-add-ghost');
    ghost.innerHTML = '<img src="' + esc(treasureItemIcon(record)) + '" alt="">';
    var follow = function(mv) {
      var r = inner.getBoundingClientRect();
      ghost.style.left = (mv.clientX - r.left) + 'px';
      ghost.style.top = (mv.clientY - r.top) + 'px';
    };
    var cleanup = function() {
      inner.removeEventListener('pointerdown', once, true);
      inner.removeEventListener('pointermove', follow, true);
      document.removeEventListener('keydown', onKey, true);
      inner.style.cursor = '';
      ghost.remove();
      releaseMapTool();
    };
    var onKey = function(ev) {
      if (ev.key !== 'Escape') return;
      cleanup();
      ui.gateText = 'Move treasure cancelled.';
      renderScenarioTab(document.getElementById('panel-scenario'));
    };
    var once = function(ev) {
      cleanup();
      ev.preventDefault();
      ev.stopPropagation();
      eatNextMapClick(inner);
      var rect = inner.getBoundingClientRect();
      var cal = calibrationData(key);
      var proj = projectionFor(cal, useImageFor(cal));
      var imageX = clamp((ev.clientX - rect.left) / ui.zoom, 0, proj.naturalWidth);
      var imageY = clamp((ev.clientY - rect.top) / ui.zoom, 0, proj.naturalHeight);
      moveTreasureFromImage(rom, key, archive, index, imageX, imageY, proj);
    };
    inner.addEventListener('pointermove', follow, true);
    inner.addEventListener('pointerdown', once, true);
    document.addEventListener('keydown', onKey, true);
  }

  function nextTreasureGlobalId(rom) {
    var used = {};
    var state = ensureState(rom);
    Object.keys(state.treasureArchives || {}).forEach(function(archive) {
      (state.treasureArchives[archive].records || []).forEach(function(record) {
        used[record.globalId & 0xFF] = true;
      });
    });
    for (var id = 231; id <= 255; id++) {
      if (!used[id]) return id;
    }
    for (var fallback = 1; fallback <= 255; fallback++) {
      if (!used[fallback]) return fallback;
    }
    return null;
  }

  function beginAddTreasurePlacement(rom, key) {
    var archive = treasureArchiveForKey(key);
    var model = archive && ensureState(rom).treasureArchives[archive];
    var panel = document.getElementById('panel-scenario');
    if (!model) { ui.gateText = 'No maizo treasure archive is mapped for this scenario key.'; renderScenarioTab(panel); return; }
    var maxRecords = treasureMaxRecordsForArchive(rom, archive, model.filename);
    if (model.records.length >= maxRecords) { ui.gateText = 'This maizo file is at its fixed -lh0- slot capacity.'; renderScenarioTab(panel); return; }
    var gid = nextTreasureGlobalId(rom);
    if (gid == null) { ui.gateText = 'No unused treasure global id remains.'; renderScenarioTab(panel); return; }
    var inner = document.getElementById('sc-map-inner');
    if (!inner) { ui.gateText = 'Map is not available for this scenario.'; renderScenarioTab(panel); return; }
    mapTool = 'add-treasure';
    inner.style.cursor = 'crosshair';
    var gate = document.getElementById('sc-gate');
    if (gate) gate.textContent = 'Click the map to place a new treasure...';
    var ghost = mapGhost(inner, 'sc-marker sc-treasure-marker sc-add-ghost');
    ghost.innerHTML = '<img src="' + esc((OB64.itemIconURL ? OB64.itemIconURL('Heal Leaf') : 'resources/Item%20Icons/Heal%20Leaf.png')) + '" alt="">';
    var follow = function(mv) {
      var r = inner.getBoundingClientRect();
      ghost.style.left = (mv.clientX - r.left) + 'px';
      ghost.style.top = (mv.clientY - r.top) + 'px';
    };
    var cleanup = function() {
      inner.removeEventListener('pointerdown', once, true);
      inner.removeEventListener('pointermove', follow, true);
      document.removeEventListener('keydown', onKey, true);
      inner.style.cursor = '';
      ghost.remove();
      releaseMapTool();
    };
    var onKey = function(ev) {
      if (ev.key !== 'Escape') return;
      cleanup();
      ui.gateText = 'Add treasure cancelled.';
      renderScenarioTab(panel);
    };
    var once = function(ev) {
      cleanup();
      ev.preventDefault();
      ev.stopPropagation();
      eatNextMapClick(inner);
      var rect = inner.getBoundingClientRect();
      var cal = calibrationData(key);
      var proj = projectionFor(cal, useImageFor(cal));
      var imageX = clamp((ev.clientX - rect.left) / ui.zoom, 0, proj.naturalWidth);
      var imageY = clamp((ev.clientY - rect.top) / ui.zoom, 0, proj.naturalHeight);
      createTreasureAt(rom, key, archive, imageX, imageY, proj, gid);
    };
    inner.addEventListener('pointermove', follow, true);
    inner.addEventListener('pointerdown', once, true);
    document.addEventListener('keydown', onKey, true);
  }

  function createTreasureAt(rom, key, archive, imageX, imageY, projection, globalId) {
    var model = ensureState(rom).treasureArchives[archive];
    if (!model) return;
    var pair = treasureBytesFromImage(key, imageX, imageY, projection);
    var record = refreshTreasureRecord({
      index: model.records.length,
      archive: archive,
      globalId: globalId,
      x: pair[0],
      y: pair[1],
      table: 2,
      itemId: 1,
      added: true,
    });
    model.records.push(record);
    reindexTreasureModel(model);
    ui.selectedTreasure = { archive: archive, index: record.index };
    clearOtherSelectionsForTreasure();
    commitTreasureEdit(rom, archive, 'Treasure added: gid ' + record.globalId + ' / Heal Leaf. Use Change item to choose the reward.');
  }

  function deleteTreasure(rom, key, archive, index) {
    var model = ensureState(rom).treasureArchives[archive];
    var record = model && model.records[index];
    if (!record) return;
    confirmThemed('Remove buried treasure',
      'Remove treasure gid ' + record.globalId + ' (' + treasureItemName(record) + ') from this maizo file?',
      'Remove treasure',
      function() {
        model.records.splice(index, 1);
        reindexTreasureModel(model);
        ui.selectedTreasure = null;
        commitTreasureEdit(rom, archive, 'Treasure removed from archive ' + archive + '.');
      });
  }

  function openTreasureItemPicker(rom, key, archive, index) {
    var model = ensureState(rom).treasureArchives[archive];
    var record = model && model.records[index];
    if (!record) return;
    var items = [];
    Object.keys(OB64.ITEM_NAMES || {}).map(Number).filter(function(id) { return id > 0; }).sort(function(a, b) { return a - b; }).forEach(function(id) {
      items.push({ id: id, name: OB64.itemName(id), kind: 'equip', kindLabel: 'Equipment' });
    });
    for (var cid = 1; cid <= 44; cid++) {
      items.push({ id: cid, name: OB64.consumableName(cid), kind: 'consumable', kindLabel: 'Special' });
    }
    var currentKind = record.table === 2 ? 'consumable' : 'equip';
    if (OB64.openSaveItemPickerModal) {
      OB64.openSaveItemPickerModal({
        title: 'Select treasure item',
        items: items,
        currentId: record.itemId,
        currentKind: currentKind,
        onSelect: function(id, kind) {
          record.table = kind === 'consumable' ? 2 : 1;
          record.itemId = id & 0xFFFF;
          refreshTreasureRecord(record);
          commitTreasureEdit(rom, archive, 'Treasure reward set to ' + treasureItemName(record) + '.');
        },
      });
      return;
    }
    var raw = window.prompt('Item id (equipment table 1)', String(record.itemId));
    var id = parseInt(raw, 10);
    if (!isNaN(id)) {
      record.table = 1;
      record.itemId = id & 0xFFFF;
      refreshTreasureRecord(record);
      commitTreasureEdit(rom, archive, 'Treasure reward set to ' + treasureItemName(record) + '.');
    }
  }

  function nodeExclusiveToRow(model, nodeId, rowIndex) {
    var refs = nodeConsumerRows(model, nodeId);
    return refs.length === 1 && refs[0].rowIndex === rowIndex;
  }

  function deriveTemplateReuse(model, rowIndex, owned) {
    var reuse = { slots: {}, used: {} };
    owned = owned || {};
    function claim(slot, node) {
      if (!node || reuse.slots[slot] || reuse.used[node.nodeId]) return;
      if (!nodeExclusiveToRow(model, node.nodeId, rowIndex)) return;
      reuse.slots[slot] = node.nodeId;
      reuse.used[node.nodeId] = true;
    }
    ['gate', 'dest'].forEach(function(slot) {
      var ownedNode = owned[slot] != null ? nodeById(model, owned[slot]) : null;
      claim(slot, ownedNode);
    });
    var row = model.section1[rowIndex];
    var chain = row && row.bytes ? walkNodeChainModel(model, row.bytes[6]) : [];
    chain.forEach(function(node, i) {
      if (i === 0 && (node.kind === 0 || node.kind === 2)) claim('gate', node);
      if (node.kind === 1) claim('dest', node);
    });
    return reuse;
  }

  function claimTemplateNode(model, owned, slot, reuse) {
    var nodeId = reuse && reuse.slots ? reuse.slots[slot] : null;
    var node = nodeId != null ? nodeById(model, nodeId) : null;
    if (node) {
      owned[slot] = node.nodeId;
      return node;
    }
    return null;
  }

  function extraById(model, extraId) {
    return (model.section3 || []).filter(function(x) { return x.extraId === extraId; })[0] || null;
  }

  function extraExclusiveToRow(model, extraId, rowIndex) {
    var refNodes = triggerRefNodes(model, extraId);
    var refs = nodesUsedBySquads(model, refNodes);
    return otherSquadRefs(refs, rowIndex).length === 0;
  }

  function reusableExtra9ForRow(model, rowIndex, owned) {
    var ownedExtra = owned && owned.extra != null ? extraById(model, owned.extra) : null;
    if (ownedExtra && ownedExtra.kind === 9 && extraExclusiveToRow(model, ownedExtra.extraId, rowIndex)) return ownedExtra.extraId;
    var row = model.section1[rowIndex];
    var startNode = row && row.bytes ? nodeById(model, row.bytes[6]) : null;
    var gateId = startNode && startNode.bytes ? startNode.bytes[10] : 0;
    var gateExtra = gateId ? extraById(model, gateId) : null;
    if (gateExtra && gateExtra.kind === 9 && extraExclusiveToRow(model, gateExtra.extraId, rowIndex)) return gateExtra.extraId;
    return null;
  }

  // Owned-slot writers for the live behavior builder. Same-session ownership is only a fast
  // path now: before allocation, templates also derive reusable nodes from the squad's current
  // exclusive chain, so re-selecting and re-applying cannot leak orphan nodes.
  function ownedNodeWrite(model, owned, slot, fields, reuse) {
    var node = claimTemplateNode(model, owned, slot, reuse);
    if (!node) {
      node = allocNode(model, fields);
      if (node) owned[slot] = node.nodeId;
      return node;
    }
    return writeNodeFields(node, fields);
  }

  function ownedExtra9Write(model, owned, threshold, reusableExtraId) {
    var extra = reusableExtraId != null ? extraById(model, reusableExtraId) : null;
    if (!extra) {
      extra = allocExtra(model, 9, [0, 0, 0, 0, clamp(threshold || 4, 1, 30)]);
      if (extra) owned.extra = extra.extraId;
      return extra;
    }
    extra.kind = 9;
    extra.bytes[1] = 9;
    extra.bytes[2] = 0;
    extra.bytes[3] = 0;
    extra.bytes[4] = 0;
    extra.bytes[5] = 0;
    extra.bytes[6] = clamp(threshold || 4, 1, 30) & 0xFF;
    return extra;
  }

  function templateDestinationFields(cal, dest, next) {
    if (!dest) return null;
    var fields = { kind: 1, subtype: 2, next: next != null ? next : 0 };
    if (dest.selector != null) fields.siteSelector = dest.selector;
    else fields.coordBytes = worldToBytePair(cal, dest.x, dest.z);
    return fields;
  }

  // Behavior templates write the DECODED grammar:
  //   Section 1 byte [6] = start node id (object +0xBA); kind-2 start node => spawns dormant
  //   (loader clears the active bit - the ambush mechanism); node byte [10]/[11]/[12] = compound
  //   gate; node byte [17] = next node (0xFF = permanent camp); coordinate waypoints are kind 1
  //   subtype 2 with bounds-normalized byte coords in [4],[5] ([5] kept >= 1 so it cannot
  //   misread as a selector).
  // params: { trigger: extraId|0, dest: {x,z,selector?}|null, threshold: n, owned: builder owned slots }
  function applyTemplate(model, rowIndex, template, cal, params) {
    var row = model.section1[rowIndex];
    if (!template) return 'no template';
    params = params || {};
    var owned = params.owned || { dest: null, gate: null, extra: null };
    var reuse = deriveTemplateReuse(model, rowIndex, owned);
    var destFields = templateDestinationFields(cal, params.dest, 0);

    if (template === 'guard-site' || template === 'guard-coordinate') {
      row.bytes[6] = 1; // +0xBA = 1: no route, hold position (scripted-tier idle convention)
      params.gcResult = runNodeGc(model);
      return null;
    }
    if (template === 'guard-sally') {
      // A STATIONARY hold node (kind 0) with Wait = Initiate, so it attacks nearby player squads.
      // Unlike the sentinel above this needs a node - a sentinel ([6]=1) has no orders to carry.
      var sally = ownedNodeWrite(model, owned, 'gate', { kind: 0, moveOrder: 0, waitOrder: 1, next: 0 }, reuse);
      if (!sally) return 'Section 2 is at its 16-node cap';
      row.bytes[6] = sally.nodeId;
      params.gcResult = runNodeGc(model);
      return null;
    }
    if (template === 'march-chain') {
      if (!destFields) return 'march needs a destination - click Destination, then the map';
      var dest = ownedNodeWrite(model, owned, 'dest', destFields, reuse);
      if (!dest) return 'Section 2 is at its 16-node cap';
      row.bytes[6] = dest.nodeId;
      params.gcResult = runNodeGc(model);
      return null;
    }
    if (template === 'wait-march') {
      if (!destFields) return 'wait-march needs a destination - click Destination, then the map';
      if (!params.trigger) return 'wait-march needs an advance trigger - pick one in Advance trigger';
      var wDest = ownedNodeWrite(model, owned, 'dest', destFields, reuse);
      if (!wDest) return 'Section 2 is at its 16-node cap';
      var hold = ownedNodeWrite(model, owned, 'gate', { kind: 0, moveOrder: 0, waitOrder: 0, gateExtra: params.trigger, next: wDest.nodeId }, reuse);
      if (!hold) return 'Section 2 is at its 16-node cap';
      row.bytes[6] = hold.nodeId;
      params.gcResult = runNodeGc(model);
      return null;
    }
    if (template === 'solo-ambush') {
      if (!params.trigger) return 'ambush needs an advance trigger - pick one in Advance trigger';
      var order = destFields ? ownedNodeWrite(model, owned, 'dest', destFields, reuse) : null;
      var lair = ownedNodeWrite(model, owned, 'gate', {
        kind: 2, moveOrder: 0, waitOrder: 0, gateExtra: params.trigger, next: order ? order.nodeId : 1,
      }, reuse);
      if (!lair) return 'Section 2 is at its 16-node cap';
      row.bytes[6] = lair.nodeId; // kind-2 start => spawns dormant, wakes via path B with orders
      params.gcResult = runNodeGc(model);
      return null;
    }
    if (template === 'reinforce-remnant') {
      var extra = ownedExtra9Write(model, owned, params.threshold, reusableExtra9ForRow(model, rowIndex, owned));
      if (!extra) return 'Section 3 is at its 16-extra cap';
      var rDest = destFields ? ownedNodeWrite(model, owned, 'dest', destFields, reuse) : null;
      var gate = ownedNodeWrite(model, owned, 'gate', { kind: 2, moveOrder: 0, waitOrder: 0, gateExtra: extra.extraId, next: rDest ? rDest.nodeId : 1 }, reuse);
      if (!gate) return 'Section 2 is at its 16-node cap';
      row.bytes[6] = gate.nodeId;
      params.gcResult = runNodeGc(model);
      return null;
    }
    if (template === 'camp-terminal') {
      var campFields = templateDestinationFields(cal, params.dest, 0xFF);
      if (!campFields) return 'camp needs a destination - click Destination, then the map';
      var camp = ownedNodeWrite(model, owned, 'dest', campFields, reuse);
      if (!camp) return 'Section 2 is at its 16-node cap';
      row.bytes[6] = camp.nodeId;
      params.gcResult = runNodeGc(model);
      return null;
    }
    return 'unknown template';
  }

  // Donor pick for an added squad. Donors come from the generated verified-donor list
  // (records referenced by NO eset / runtime trace / wiki mapping - likely the training
  // opponent pool, so they are override donors ONLY, never overwritten). The override
  // resolver matches on record CONTENT, so the donor's bytes must also be unique among
  // everything this scenario loads (and among other donors used here).
  function firstFreeEdat(rom, key, model) {
    var data = OB64.SCENARIO_ESET_DATA || {};
    var pool = (data.enemydat && data.enemydat.donorCandidates) || [];
    var records = (data.enemydat && data.enemydat.records) || [];
    var used = {};
    var usedContent = {};
    model.section1.forEach(function(row) {
      var e = row.edatOneBased - 1;
      used[e] = true;
      if (records[e]) usedContent[records[e]] = true;
    });
    var scn = squadScenario(key);
    if (scn) (scn.squads || []).forEach(function(sq) {
      used[sq.e] = true;
      if (records[sq.e]) usedContent[records[sq.e]] = true;
    });
    (ensureState(rom).addedSquads || []).forEach(function(r) {
      if (r.runtimeKey !== key) return;
      used[r.edatId] = true;
      if (records[r.edatId]) usedContent[records[r.edatId]] = true;
    });
    for (var i = 0; i < pool.length; i++) {
      var e2 = pool[i];
      if (used[e2]) continue;
      if (records[e2] && usedContent[records[e2]]) continue; // byte-equal content collision
      return e2;
    }
    // No verified donor available (should not happen with 175 candidates): legacy scan.
    var edat = 0;
    while (used[edat] && edat < 700) edat++;
    return edat;
  }

  // Default composition for a freshly placed squad: clone the scenario's first vanilla record
  // (keeps scenario-appropriate non-composition bytes), then reduce it to a lone leader in the
  // center cell. Falls back to a bare Fighter when the scenario has no squad data.
  function defaultSquadSeed(rom, key) {
    var out = new Uint8Array(35);
    var scn = squadScenario(key);
    var sq = scn && scn.squads && scn.squads[0];
    if (sq && sq.rec) {
      var src = OB64.scenarioCodec.compactHexToBytes(sq.rec);
      out.set(src.slice(0, 35));
    }
    out[6] = 5; // leader anchor: center cell
    [7, 13, 14, 15, 16, 22, 23, 24].forEach(function(f) { out[f] = 0; }); // drop B/C groups
    if (!out[0]) out[0] = 0x01; // Fighter
    return out;
  }

  function seedSquadOverride(rom, key, edatId) {
    if (!rom.squadOverrides) rom.squadOverrides = {};
    var k = key + ':' + edatId;
    if (!rom.squadOverrides[k]) {
      rom.squadOverrides[k] = defaultSquadSeed(rom, key);
      // The comp rides the squad-override export lane; mark it dirty so app.js
      // includes the blob writes even if the comp is never edited afterwards.
      if (OB64._squadChanged) OB64._squadChanged();
    }
    return rom.squadOverrides[k];
  }

  function isAddedRow(rom, key, rowIndex) {
    return (ensureState(rom).addedSquads || []).some(function(r) {
      return r.runtimeKey === key && r.section1Row === rowIndex;
    });
  }

  // Add Squad = a unit-place tool: crosshair + ghost marker, one click places a real Section 1
  // row (guard behavior, default lone-leader comp seeded as a squad override so the sidebar
  // modal edits it and the marker wears the leader's icon). Exports for real: the row splices
  // with the mission ESET, the comp rides the squad-override blob (both cold-boot proven).
  function beginAddSquadPlacement(rom, key) {
    var state = ensureState(rom);
    var model = state.models[key];
    var panel = document.getElementById('panel-scenario');
    if (!model) return;
    var limit = OB64.scenarioCodec.DEFAULT_LIMITS.section1RowsMax;
    if (model.section1.length >= limit) {
      ui.gateText = 'Section 1 is at its ' + limit + '-row conservative cap for this scenario.';
      renderScenarioTab(panel);
      return;
    }
    var maxSource = model.section1.reduce(function(max, row) { return Math.max(max, row.sourceId); }, 0);
    if (maxSource + 1 > OB64.scenarioCodec.DEFAULT_LIMITS.sourceIdMax) {
      ui.gateText = 'Next source id would exceed 0x31 - the measured 50-slot pool ceiling (source 0x32 breaks the scenario load).';
      renderScenarioTab(panel);
      return;
    }
    var unitsNow = predictedUnits(rom, key);
    if (unitsNow != null && unitsNow >= 100) {
      ui.gateText = 'Deployed-unit table is full (' + unitsNow + '/100) - reduce squad sizes first.';
      renderScenarioTab(panel);
      return;
    }
    var inner = document.getElementById('sc-map-inner');
    if (!inner) { ui.gateText = 'Map is not available for this scenario.'; renderScenarioTab(panel); return; }

    mapTool = 'add-squad';
    inner.style.cursor = 'crosshair';
    var gateEl = document.getElementById('sc-gate');
    if (gateEl) gateEl.textContent = 'Click the map to place the new squad (snaps to towns, Esc cancels)...';

    var ghost = mapGhost(inner, 'sc-squad-marker sc-add-ghost');
    var seedRec = defaultSquadSeed(rom, key);
    var iconUrl = seedRec[0] && OB64.classPortraitUrl ? OB64.classPortraitUrl(seedRec[0]) : '';
    ghost.innerHTML = iconUrl ? '<img src="' + esc(iconUrl) + '" alt="">' : '';
    var follow = function(mv) {
      var r = inner.getBoundingClientRect();
      ghost.style.left = (mv.clientX - r.left) + 'px';
      ghost.style.top = (mv.clientY - r.top) + 'px';
    };
    var cleanup = function() {
      inner.removeEventListener('pointerdown', once, true);
      inner.removeEventListener('pointermove', follow, true);
      document.removeEventListener('keydown', onKey, true);
      inner.style.cursor = '';
      ghost.remove();
      releaseMapTool();
    };
    var onKey = function(ke) {
      if (ke.key !== 'Escape') return;
      cleanup();
      ui.gateText = 'Add squad cancelled.';
      renderScenarioTab(panel);
    };
    var once = function(ev) {
      cleanup();
      ev.preventDefault();
      ev.stopPropagation();
      var rect = inner.getBoundingClientRect();
      var cal = calibrationData(key);
      var proj = projectionFor(cal, useImageFor(cal));
      var imageX = clamp((ev.clientX - rect.left) / ui.zoom, 0, proj.naturalWidth);
      var imageY = clamp((ev.clientY - rect.top) / ui.zoom, 0, proj.naturalHeight);
      createAddedSquadAt(rom, key, imageX, imageY, proj);
    };
    inner.addEventListener('pointermove', follow, true);
    inner.addEventListener('pointerdown', once, true);
    document.addEventListener('keydown', onKey, true);
  }

  function createAddedSquadAt(rom, key, imageX, imageY, projection) {
    var state = ensureState(rom);
    var model = state.models[key];
    var last = model.section1[model.section1.length - 1] || null;
    var maxSource = model.section1.reduce(function(max, row) { return Math.max(max, row.sourceId); }, 0);
    var edatId = firstFreeEdat(rom, key, model);
    var bytes = new Array(18).fill(0);
    bytes[0] = (maxSource + 1) & 0xFF;
    bytes[1] = ((edatId + 1) >> 8) & 0xFF;
    bytes[2] = (edatId + 1) & 0xFF;
    if (last) {
      bytes[5] = last.bytes[5]; // phase byte: keep the file's own convention
      bytes[9] = last.bytes[9]; // tier byte: same
    }
    // Section 1 [7]/[8] are non-zero in every vanilla row (range 1..4) but do NOT drive squad AI
    // (tested: they are not the aggro/order control - that is the start node's [2]/[3] orders). Keep
    // them in-range by inheriting the mission's convention instead of leaving a fresh-fill 0.
    bytes[7] = (last && last.bytes[7]) || 0x02;
    bytes[8] = (last && last.bytes[8]) || 0x01;
    bytes[6] = 1; // start node 1 = hold position (guard) until a behavior is applied
    placementBytesFromImage(rom, key, { bytes: bytes }, imageX, imageY, projection);
    // The appended row becomes the final row and must carry the Section 2 alias tail; the old
    // final row's tail reverts to plain descriptor space (zero = no descriptor group).
    if (last) { last.bytes[16] = 0; last.bytes[17] = 0; }
    model.section1.push({ bytes: bytes });
    syncStructuralOffsets(model);
    OB64.scenarioCodec.refreshDecodedRows(model);
    var rowIndex = model.section1.length - 1;
    state.addedSquads.push({
      runtimeKey: key,
      sourceId: bytes[0],
      edatId: edatId,
      section1Row: rowIndex,
      status: 'placed',
      createdAt: new Date().toISOString(),
    });
    seedSquadOverride(rom, key, edatId);
    state.modifiedKeys[key] = true;
    changed();
    ui.selectedPoint = rowIndex;
    ui.selectedSite = null;
    ui.selectedTrigger = null;
    ui.selectedTreasure = null;
    ui.selectedNode = null;
    ui.gateText = 'Squad placed: source ' + bytes[0] + ' / edat ' + edatId + '. Edit the comp in the sidebar; exports with the mission ESET + squad-override blob.';
    var fitAfter = archiveFitInfo(rom, key);
    if (fitAfter && !fitAfter.fits) {
      ui.gateText += ' This ESET exceeds its old slot (' + fitAfter.size + '/' + fitAfter.slot + 'B); export will relocate it to ROM tail.';
    }
    renderScenarioTab(document.getElementById('panel-scenario'));
  }

  function deleteAddedSquad(rom, key, rowIndex) {
    var state = ensureState(rom);
    var model = state.models[key];
    var entryIndex = -1;
    state.addedSquads.forEach(function(r, i) {
      if (entryIndex < 0 && r.runtimeKey === key && r.section1Row === rowIndex) entryIndex = i;
    });
    if (entryIndex < 0) return;
    var entry = state.addedSquads[entryIndex];
    model.section1.splice(rowIndex, 1);
    state.addedSquads.splice(entryIndex, 1);
    state.addedSquads.forEach(function(r) {
      if (r.runtimeKey === key && r.section1Row != null && r.section1Row > rowIndex) r.section1Row--;
    });
    if (rom.squadOverrides) {
      delete rom.squadOverrides[key + ':' + entry.edatId];
      if (OB64._squadChanged) OB64._squadChanged();
    }
    var gc = runNodeGc(model);
    if (!gc.changed) {
      syncStructuralOffsets(model);
      OB64.scenarioCodec.refreshDecodedRows(model);
    }
    state.modifiedKeys[key] = true;
    changed();
    ui.selectedPoint = null;
    ui.gateText = 'Added squad removed (source ' + entry.sourceId + ' / edat ' + entry.edatId + ').' + (gc.message ? ' ' + gc.message : '');
    renderScenarioTab(document.getElementById('panel-scenario'));
  }

  function collectProject(rom) {
    var state = ensureState(rom);
    var modifiedEsets = {};
    Object.keys(state.models).forEach(function(key) {
      if (keyModified(rom, Number(key))) {
        modifiedEsets[key] = {
          runtimeKey: Number(key),
          archive: state.metadata[key] && state.metadata[key].archive,
          filename: state.metadata[key] && state.metadata[key].filename,
          rawHex: OB64.scenarioCodec.bytesToCompactHex(modelBytes(state.models[key])),
        };
      }
    });
    var modifiedTreasures = {};
    Object.keys(state.treasureArchives || {}).forEach(function(archiveKey) {
      var archive = Number(archiveKey);
      if (!treasureArchiveModified(rom, archive)) return;
      var model = state.treasureArchives[archive];
      modifiedTreasures[archive] = {
        archive: archive,
        filename: model.filename,
        rawHex: OB64.scenarioCodec.bytesToCompactHex(serializeTreasureRecords(model.records)),
      };
    });
    return {
      format: 'ob64-scenario-project',
      version: 2,
      created_at: new Date().toISOString(),
      source: 'LordlyCaliber Scenario tab',
      settings: state.settings,
      modifiedEsets: modifiedEsets,
      modifiedTreasures: modifiedTreasures,
      siteAllegiances: state.siteAllegiances,
      // Carry each added squad's comp record (its squad override) so a project reload
      // restores the sidebar-editable composition, not just the placement row.
      addedSquads: state.addedSquads.map(function(r) {
        var copy = {};
        for (var f in r) copy[f] = r[f];
        var over = rom.squadOverrides && rom.squadOverrides[r.runtimeKey + ':' + r.edatId];
        if (over) copy.compRecHex = OB64.scenarioCodec.bytesToCompactHex(over);
        return copy;
      }),
      layers: {},
    };
  }

  function loadProject(rom, project) {
    if (!project || project.format !== 'ob64-scenario-project') throw new Error('Not an OB64 Scenario project file');
    if (project.version != null && project.version > 2) {
      throw new Error('Scenario project version ' + project.version + ' is newer than this editor supports (max 2).');
    }
    var oldState = ensureState(rom);
    var oldAdded = (oldState.addedSquads || []).slice();
    var state = resetScenarioState(rom);
    if (project.settings) state.settings = project.settings;
    state.siteAllegiances = project.siteAllegiances || {};
    state.addedSquads = (project.addedSquads || []).map(function(r) {
      var copy = {};
      for (var f in r) copy[f] = r[f];
      return copy;
    });
    var keepAddedOverrides = {};
    state.addedSquads.forEach(function(r) {
      keepAddedOverrides[r.runtimeKey + ':' + r.edatId] = true;
    });
    state.addedSquads.forEach(function(r) {
      if (!r.compRecHex) return;
      if (!rom.squadOverrides) rom.squadOverrides = {};
      rom.squadOverrides[r.runtimeKey + ':' + r.edatId] = OB64.scenarioCodec.compactHexToBytes(r.compRecHex);
    });
    var prunedOverrides = false;
    oldAdded.forEach(function(r) {
      var key = r.runtimeKey + ':' + r.edatId;
      if (!keepAddedOverrides[key] && rom.squadOverrides && rom.squadOverrides[key]) {
        delete rom.squadOverrides[key];
        prunedOverrides = true;
      }
    });
    if (prunedOverrides && OB64._squadChanged) OB64._squadChanged();
    var esets = project.modifiedEsets || {};
    Object.keys(esets).forEach(function(key) {
      var raw = OB64.scenarioCodec.compactHexToBytes(esets[key].rawHex);
      state.models[key] = OB64.scenarioCodec.parseEset(raw, { sourcePath: esets[key].filename || key });
      state.modifiedKeys[key] = true;
    });
    var treasures = project.modifiedTreasures || {};
    Object.keys(treasures).forEach(function(archiveKey) {
      var archive = Number(archiveKey);
      var entry = treasureArchiveEntry(archive) || { filename: treasures[archiveKey].filename };
      var raw = OB64.scenarioCodec.compactHexToBytes(treasures[archiveKey].rawHex);
      state.treasureArchives[archive] = parseTreasureBytes(raw, archive, entry);
      state.modifiedTreasureArchives[archive] = true;
    });
    changed();
  }

  // The override resolver matches on record CONTENT within a scenario gate. Every added
  // squad's donor bytes must be unique among that scenario's loaded records - hand edits in
  // the Advanced grid can retarget rows in ways the donor picker never saw.
  function addedSquadCollisions(rom) {
    var data = OB64.SCENARIO_ESET_DATA || {};
    var records = (data.enemydat && data.enemydat.records) || [];
    var state = ensureState(rom);
    var issues = [];
    (state.addedSquads || []).forEach(function(r) {
      if (r.section1Row == null) return;
      var model = state.models[r.runtimeKey];
      if (!model) return;
      var donorHex = records[r.edatId];
      if (!donorHex) {
        issues.push('key ' + r.runtimeKey + ' edat ' + r.edatId + ': no record bytes available for the override original');
        return;
      }
      var matches = 0;
      model.section1.forEach(function(row) {
        // read the edat from raw bytes (decoded fields can be stale mid-edit)
        var e = (((row.bytes[1] || 0) << 8) | (row.bytes[2] || 0)) - 1;
        if (records[e] === donorHex) matches++;
      });
      if (matches > 1) {
        issues.push('key ' + r.runtimeKey + ' source ' + r.sourceId + ': donor record ' + r.edatId +
          ' content collides with another row this scenario loads (the override would re-skin both)');
      }
    });
    return issues;
  }

  // Map-unit leader sprite gate (root cause of a LOADING hang first hit by a user export).
  // The LOADING map-unit visual builder resolves the squad LEADER's class through two static
  // tables in the 0x141000 overlay module: slot = u8[0x14E074 + classId], then
  // entry = u32BE[0x14DF30 + slot*4]. Slot 0's entry is the 0xFFFFFFFF "no map sprite"
  // sentinel (85/165 classes: monsters, undead, Ninja, soldiers, specials). The game passes
  // the sentinel's low half (0xFFFF) UNGUARDED as an index into the 1272-entry type-5 sprite
  // directory (blob 0x0209D322), reads a garbage child offset, trusts a garbage u32 blob
  // length (~1.1 GB), and runs the 0x200-window cart DMA off the ROM end (PI-540 hang).
  // So: any squad the player can DEPLOY on the map must have a sprite-valid leader class.
  var SPRITE_CLASS_SLOT_TABLE_Z64 = 0x14E074; // 0xA5 bytes, indexed by class id
  var SPRITE_SLOT_ENTRY_TABLE_Z64 = 0x14DF30; // 0x51 u32 BE entries
  var SPRITE_NONE_SENTINEL = 0xFFFFFFFF;

  function leaderClassHasMapSprite(rom, classId) {
    if (!(classId >= 1 && classId <= 0xA4)) return false;
    var slot = rom.z64[SPRITE_CLASS_SLOT_TABLE_Z64 + classId];
    var off = SPRITE_SLOT_ENTRY_TABLE_Z64 + slot * 4;
    var entry = (((rom.z64[off] << 24) | (rom.z64[off + 1] << 16) | (rom.z64[off + 2] << 8) | rom.z64[off + 3]) >>> 0);
    return entry !== SPRITE_NONE_SENTINEL;
  }

  function classLabel(id) {
    var name = OB64.className ? OB64.className(id) : null;
    return (name || 'class') + ' (0x' + id.toString(16).toUpperCase().padStart(2, '0') + ')';
  }

  // Every override/added-squad leader that can deploy as a map unit must have a map sprite.
  // Added squads are checked on their EFFECTIVE comp (override record, else donor default).
  // Vanilla-record overrides are checked only when they CHANGE the leader byte — unchanged
  // vanilla leaders (e.g. story classes like 0x8A) display through a separate named-character
  // path and are load-proven as shipped.
  function spritelessLeaderIssues(rom) {
    var data = OB64.SCENARIO_ESET_DATA || {};
    var records = (data.enemydat && data.enemydat.records) || [];
    var state = ensureState(rom);
    var issues = [];
    var addedByKey = {};
    (state.addedSquads || []).forEach(function(r) {
      if (r.section1Row == null) return;
      addedByKey[r.runtimeKey + ':' + r.edatId] = r;
      var over = rom.squadOverrides && rom.squadOverrides[r.runtimeKey + ':' + r.edatId];
      var donorHex = records[r.edatId] || '';
      var leader = over ? over[0] : parseInt(donorHex.slice(0, 2) || '0', 16);
      if (!leaderClassHasMapSprite(rom, leader)) {
        issues.push('key ' + r.runtimeKey + ' added squad (source ' + r.sourceId + ', edat ' + r.edatId + '): leader ' +
          classLabel(leader) + ' has no map-unit sprite; deploying it hangs LOADING in a runaway DMA. Pick a leader class with a map sprite.');
      }
    });
    Object.keys(rom.squadOverrides || {}).forEach(function(k) {
      if (addedByKey[k]) return;
      var over = rom.squadOverrides[k];
      if (!over || !over.length) return;
      var edatId = Number(k.split(':')[1]);
      var donorHex = records[edatId] || '';
      var originalLeader = parseInt(donorHex.slice(0, 2) || '0', 16);
      if (over[0] !== originalLeader && !leaderClassHasMapSprite(rom, over[0])) {
        issues.push('squad override ' + k + ': new leader ' + classLabel(over[0]) +
          ' has no map-unit sprite; deploying it hangs LOADING in a runaway DMA. Keep the original leader or pick a class with a map sprite.');
      }
    });
    return issues;
  }

  function dataScincsvArchives() {
    return (OB64.SCENARIO_ESET_DATA && OB64.SCENARIO_ESET_DATA.scincsvArchives) || {};
  }

  // Author-selected allegiance -> scincsv descriptor addend halfword. The game reads SPECIFIC
  // canonical values, not bit patterns: corpus scan is enemy=0x0004 (366/366 uniform),
  // neutral=0x0000 (uniform), allied=0x2012 (58/65; 0x2002 the other 7). An earlier "flip only
  // the 0x2000 bit" approach was WRONG - clearing 0x2000 off an ex-allied 0x2002 gives 0x0002,
  // which the game does NOT treat as enemy (proven in-game: Jadd stayed allied at 0x0002 while
  // Billney at 0x0004 flipped correctly). So always emit the canonical value for the target state.
  function allegianceTargetAddend(current, target) {
    if (target === 'neutral') return 0x0000;
    if (target === 'allied') return 0x2012;
    return 0x0004; // enemy
  }

  // Collect town-allegiance intents and group them by the shared scincsv archive they target.
  // Several runtime keys can read one scincsv archive (e.g. keys 52/53 share scincsv28b), so an
  // edit to one key's town also moves the others reading the same descriptor - intended, but a
  // conflicting intent for the same descriptor blocks the export. Returns
  // { edits: {archive: {archive, filename, payload, changes:[...]}}, blocked: [msg...] }.
  function planAllegianceEdits(rom) {
    var state = ensureState(rom);
    var archives = dataScincsvArchives();
    var byArchive = {};
    var blocked = [];
    var claims = {}; // "archive:offset" -> { to, label }
    Object.keys(state.siteAllegiances).forEach(function(keyStr) {
      var runtimeKey = Number(keyStr);
      var intents = state.siteAllegiances[keyStr] || {};
      Object.keys(intents).forEach(function(selStr) {
        var selector = Number(selStr);
        var intent = intents[selStr];
        if (!intent || intent === 'static') return;
        var site = siteForSelector(rom, runtimeKey, selector);
        var desc = site && site.siteDescriptor;
        var label = ((site && (site.siteName || site.name)) || ('Site ' + selector)) + ' (key ' + runtimeKey + ')';
        if (!desc || desc.scincsvArchive == null || desc.descriptorByteOffset == null || !desc.descriptorPresent) {
          blocked.push(label + ': no scincsv descriptor to edit - authoring this town needs a new descriptor row, not yet supported.');
          return;
        }
        var arcInfo = archives[desc.scincsvArchive];
        if (!arcInfo || !arcInfo.payloadHex) {
          blocked.push(label + ': scincsv archive ' + desc.scincsvArchive + ' payload is unavailable.');
          return;
        }
        var current = desc.descriptorAddend || 0;
        var to = allegianceTargetAddend(current, intent);
        if (to === (current & 0xFFFF)) return; // no-op: already this state
        var off = desc.descriptorByteOffset;
        var ckey = desc.scincsvArchive + ':' + off;
        if (claims[ckey]) {
          if (claims[ckey].to !== to) {
            blocked.push('Allegiance conflict: ' + label + ' and ' + claims[ckey].label + ' share scincsv archive ' + desc.scincsvArchive + ' but request different states for the same town.');
          }
          return; // same target already claimed - idempotent, do not double-write
        }
        claims[ckey] = { to: to, label: label };
        if (!byArchive[desc.scincsvArchive]) {
          byArchive[desc.scincsvArchive] = {
            archive: desc.scincsvArchive,
            filename: arcInfo.filename,
            payload: OB64.scenarioCodec.compactHexToBytes(arcInfo.payloadHex),
            changes: [],
          };
        }
        byArchive[desc.scincsvArchive].changes.push({ offset: off, from: current, to: to, label: label });
      });
    });
    return { edits: byArchive, blocked: blocked };
  }

  function exportScenarioArchives(rom) {
    var state = ensureState(rom);
    var blocked = [];
    var hookState = relocationHookState(rom);
    if (hookState.installed && !hasKnownRelocationOwnership(rom, state)) {
      blocked.push('Scenario relocation hook is already installed in this ROM, but this editor session did not create or adopt its redirect table. Load a clean ROM or the source project JSON before exporting scenario edits; pre-relocated ROM adoption is not implemented yet.');
    } else if (!hookState.clean && !hookState.installed) {
      blocked.push('Scenario relocation hook site is not clean (0x' + hookState.hookWord.toString(16).toUpperCase() + '/0x' + hookState.delayWord.toString(16).toUpperCase() + ').');
    }

    // Town allegiance now exports: intents rewrite the scincsv descriptor addend halfword in a
    // tiny LH5 archive (~25-45 B at ROM ~0x2745E5B, outside the CIC-6102 CRC window). Grouping +
    // conflict/absent-descriptor validation is planned here; the splice/relocate runs below.
    var allegiancePlan = planAllegianceEdits(rom);
    if (allegiancePlan.blocked.length) blocked = blocked.concat(allegiancePlan.blocked);
    var stubs = anyProjectStub(rom);
    // Slot overflow is handled below by the relocation lane. Keep the fit number as
    // a user-facing note, not an export blocker.
    // Added squads EXPORT: appended-row deployment and the override re-skin are both
    // cold-boot proven (a control squad kept its donor comp while the overridden one
    // deployed its replacement comp, in the same run). The row splices with its mission
    // ESET below; the comp rides the squad-override blob lane in app.js. Only donor
    // content collisions still block (the resolver matches on record content).
    if (stubs.addedSquads.length) {
      var collisions = addedSquadCollisions(rom);
      if (collisions.length) {
        blocked.push('Added-squad donor content collisions must be fixed before export: ' + collisions.join(' | '));
      }
    }
    var spriteless = spritelessLeaderIssues(rom);
    if (spriteless.length) {
      blocked.push('Squad leaders without a map-unit sprite must be fixed before export: ' + spriteless.join(' | '));
    }

    var touched = [];
    var inlineWrites = [];
    var restoreSlots = [];
    var relocations = [];
    var relocationWrites = [];
    var tailCursor = RELOC_TAIL_START;
    var ownedWindows = state.relocationOwnedWindows || [];

    Object.keys(state.models).sort(function(a, b) { return Number(a) - Number(b); }).forEach(function(key) {
      var runtimeKey = Number(key);
      var model = state.models[key];
      var validation = OB64.scenarioCodec.validateEset(model);
      if (validation.errors.length) {
        blocked.push('Scenario key ' + runtimeKey + ' validator errors: ' +
          validation.errors.map(function(e) { return e.code; }).join(', '));
        return;
      }
      var raw;
      try {
        raw = modelBytes(model);
      } catch (e) {
        blocked.push('Scenario key ' + runtimeKey + ' serialize failed: ' + e.message);
        return;
      }
      var original = state.originalBytes[key];
      var meta = state.metadata[key] || scenarioData(runtimeKey);
      var archive = meta && meta.archive;
      var archiveDir = archive != null && rom.archives && rom.archives[archive];
      if (!original || OB64.scenarioCodec.equalBytes(raw, original)) {
        if (archiveDir && state.slotOwnedArchives[archive]) {
          restoreSlots.push({ archive: archive, label: 'scenario key ' + runtimeKey + ' restored' });
        }
        return;
      }
      if (!meta || archive == null || !archiveDir) {
        blocked.push('Missing ROM archive for runtime key ' + runtimeKey);
        return;
      }
      var comp = OB64.lh5Compress(raw);
      var arc = OB64.buildLHAArchive(comp, raw, meta.filename || ('eset_key_' + runtimeKey + '.bin'));
      var arcBody = arc.slice ? arc.slice(0, arc.length - 1) : arc.subarray(0, arc.length - 1);
      if (arcBody.length <= archiveSlotSize(archiveDir)) {
        inlineWrites.push({ archive: archive, archiveDir: archiveDir, bytes: arcBody, label: 'scenario key ' + runtimeKey });
        return;
      }
      var moved;
      try {
        moved = planRelocationToTail(rom, archiveDir, arc, tailCursor);
        assertRelocationTailFree(rom, moved, ownedWindows);
      } catch (e2) {
        blocked.push('Scenario key ' + runtimeKey + ' ' + e2.message);
        return;
      }
      tailCursor = moved.nextTailCursor;
      moved.runtimeKey = runtimeKey;
      moved.archive = archive;
      relocations.push(moved);
      relocationWrites.push({ moved: moved, archive: arc });
      if (state.slotOwnedArchives[archive]) restoreSlots.push({ archive: archive, label: 'scenario key ' + runtimeKey + ' fixed slot restored before relocation' });
    });

    // Buried treasure edits: maizo payloads are already stored (-lh0-) in retail. Keep the total
    // archive slot size fixed by resizing the level-2 header, so add/remove/move edits splice
    // directly outside the CRC window without shifting the following LHA archive.
    Object.keys(state.treasureArchives || {}).sort(function(a, b) { return Number(a) - Number(b); }).forEach(function(archiveKey) {
      var archive = Number(archiveKey);
      var model = state.treasureArchives[archive];
      var archiveDir = rom.archives && rom.archives[archive];
      if (!treasureArchiveModified(rom, archive)) {
        if (archiveDir && state.slotOwnedArchives[archive]) {
          restoreSlots.push({ archive: archive, label: 'buried treasure (maizo archive ' + archive + ') restored' });
        }
        return;
      }
      var payload = serializeTreasureRecords(model.records);
      if (!archiveDir) {
        blocked.push('Missing ROM archive ' + archive + ' for maizo treasure edit');
        return;
      }
      var filename = model.filename || ((treasureArchiveEntry(archive) || {}).filename) || ('maizo' + archive + '.bin');
      var slotSize = archiveSlotSize(archiveDir);
      var minHeaderSize = 24 + 2 + (1 + filename.length + 2);
      var treasureHeaderSize = slotSize - payload.length;
      if (treasureHeaderSize < minHeaderSize || treasureHeaderSize > 0xFFFF) {
        blocked.push('Buried treasure maizo archive ' + archive + ' does not fit its fixed -lh0- slot');
        return;
      }
      var arc = OB64.buildLHAArchiveUncompressed(payload, filename, treasureHeaderSize);
      if (arc.length > slotSize) {
        blocked.push('Buried treasure maizo archive ' + archive + ' new archive is ' + (arc.length - slotSize) + ' bytes larger than original slot');
        return;
      }
      inlineWrites.push({ archive: archive, archiveDir: archiveDir, bytes: arc, label: 'buried treasure (maizo archive ' + archive + ')' });
    });

    // Town-allegiance edits: one rebuilt scincsv archive per shared descriptor stream. Same
    // splice-in-place / relocate-to-tail path as the ESET archives above. scincsv archives sit
    // outside the CRC window, so a splice-in-place needs no CRC recalc; a relocation installs the
    // boot-cave redirect (inside the window) and sets crc below via relocations.length.
    Object.keys(allegiancePlan.edits).sort(function(a, b) { return Number(a) - Number(b); }).forEach(function(arcKey) {
      var plan = allegiancePlan.edits[arcKey];
      var payload = plan.payload.slice ? plan.payload.slice(0) : new Uint8Array(plan.payload);
      plan.changes.forEach(function(ch) {
        payload[ch.offset] = (ch.to >>> 8) & 0xFF;
        payload[ch.offset + 1] = ch.to & 0xFF;
      });
      var archiveDir = rom.archives && rom.archives[plan.archive];
      if (!archiveDir) {
        blocked.push('Missing ROM archive ' + plan.archive + ' for town allegiance edit');
        return;
      }
      // Store the descriptor stream UNCOMPRESSED (-lh0-). scincsv payloads are tiny, the game's
      // loader accepts -lh0- (index 750 ships that way), and this dodges an lh5Compress bug on
      // small payloads. The stored body still fits the original slot (headers are ~74 B).
      var arc = OB64.buildLHAArchiveUncompressed(payload, plan.filename || ('scincsv_' + plan.archive + '.bin'), archiveDir.totalHeaderSize);
      if (arc.length <= archiveSlotSize(archiveDir)) {
        inlineWrites.push({ archive: plan.archive, archiveDir: archiveDir, bytes: arc, label: 'town allegiance (scincsv ' + plan.archive + ')' });
        return;
      }
      var moved;
      try {
        moved = planRelocationToTail(rom, archiveDir, arc, tailCursor, { fullArchiveLength: true });
        assertRelocationTailFree(rom, moved, ownedWindows);
      } catch (e) {
        blocked.push('Town allegiance scincsv ' + plan.archive + ' ' + e.message);
        return;
      }
      tailCursor = moved.nextTailCursor;
      moved.archive = plan.archive;
      relocations.push(moved);
      relocationWrites.push({ moved: moved, archive: arc });
      if (state.slotOwnedArchives[plan.archive]) restoreSlots.push({ archive: plan.archive, label: 'town allegiance (scincsv ' + plan.archive + ') fixed slot restored before relocation' });
    });

    var publicRelocations = relocations.map(function(moved) {
      return {
        originalDmaStart: moved.originalDmaStart,
        tailDmaStart: moved.tailDmaStart,
        windowSize: moved.windowSize,
        tailArchiveOffset: moved.tailArchiveOffset,
        runtimeKey: moved.runtimeKey,
        archive: moved.archive,
      };
    });
    if (publicRelocations.length && OB64.tools) {
      try {
        OB64.tools.assertDesiredCompatible(rom, [relocationPatchOwner(publicRelocations)]);
      } catch (e3) {
        blocked.push(e3.message);
      }
    }
    if (blocked.length) return { touched: [], blocked: blocked, relocations: rom.scenarioRelocations || [] };

    resetOwnedRelocationWindows(rom, state);

    restoreSlots.forEach(function(w) {
      if (restoreArchiveSlot(rom, state, w.archive)) touched.push(w.label);
    });

    inlineWrites.forEach(function(w) {
      snapshotArchiveSlot(rom, state, w.archive);
      rom.z64.set(w.bytes, w.archiveDir.offset);
      if (w.bytes.length < archiveSlotSize(w.archiveDir)) {
        rom.z64.fill(0, w.archiveDir.offset + w.bytes.length, w.archiveDir.offset + archiveSlotSize(w.archiveDir));
      }
      state.slotOwnedArchives[w.archive] = true;
      touched.push(w.label);
    });

    var newOwnedWindows = [];
    relocationWrites.forEach(function(w) {
      var owned = snapshotRelocationWindow(rom, state, w.moved);
      writeRelocatedArchive(rom, w.archive, w.moved);
      newOwnedWindows.push(owned);
      touched.push(w.moved.runtimeKey != null ? ('scenario key ' + w.moved.runtimeKey + ' relocated') : ('town allegiance (archive ' + w.moved.archive + ') relocated'));
    });
    state.relocationOwnedWindows = newOwnedWindows;
    var redirectChanged = installRelocationRedirect(rom, publicRelocations);
    if (redirectChanged && !publicRelocations.length) touched.push('scenario relocation redirect removed');
    rom.scenarioRelocations = publicRelocations;
    return { touched: touched, blocked: [], relocations: publicRelocations, crc: !!redirectChanged };
  }

  function siteAllegianceCensus(rom) {
    var state = ensureState(rom);
    var totals = { keys: 0, sites: 0, enemy: 0, neutral: 0, allied: 0, projectIntent: 0 };
    var keys = dataScenarios().slice().sort(function(a, b) { return a.runtimeKey - b.runtimeKey; }).map(function(entry) {
      var runtimeKey = entry.runtimeKey;
      var sites = (state.sites[runtimeKey] || []).slice().sort(function(a, b) { return a.selector - b.selector; }).map(function(site) {
        var intent = (state.siteAllegiances[runtimeKey] || {})[site.selector] || '';
        var allegiance = siteAllegiance(rom, runtimeKey, site.selector);
        var desc = site.siteDescriptor || {};
        totals.sites++;
        if (totals[allegiance] != null) totals[allegiance]++;
        if (intent) totals.projectIntent++;
        return {
          selector: site.selector,
          siteName: site.siteName || ('Site ' + site.selector),
          x: site.x,
          z: site.z,
          allegiance: allegiance,
          initialAllegiance: site.initialAllegiance || '',
          neutralAtStart: !!site.neutralAtStart,
          isObjective: !!site.isObjective,
          projectIntent: intent,
          ktenmainRecordIndex: site.ktenmainRecordIndex,
          ktenmainMoraleOffset: site.ktenmainMoraleOffset,
          reason: siteAllegianceReason(site, allegiance, intent),
          descriptor: {
            scincsvArchive: desc.scincsvArchive,
            scincsvFilename: desc.scincsvFilename,
            ownKtenmainRecordIndex: desc.ownKtenmainRecordIndex,
            ownKtenmainName: desc.ownKtenmainName,
            descriptorPresent: desc.descriptorPresent,
            descriptorIndex: desc.descriptorIndex,
            descriptorAddendHex: desc.descriptorAddendHex,
          },
        };
      });
      totals.keys++;
      return {
        runtimeKey: runtimeKey,
        label: displayLabel(runtimeKey),
        archive: entry.archive,
        filename: entry.filename,
        source: entry.source || null,
        siteCount: sites.length,
        sites: sites,
      };
    });
    return { summary: totals, keys: keys };
  }

  // The embedded Squads comp editor calls this on every commit so squad markers repaint with
  // the current leader icon without a full tab re-render (scroll and focus stay put).
  OB64._scenarioSquadEdit = function() {
    var panel = document.getElementById('panel-scenario');
    if (!panel) return;
    var mapPanel = panel.querySelector('#sc-map-panel');
    var rom = OB64._romRef && OB64._romRef();
    if (!mapPanel || !rom) return;
    var scroller = mapPanel.querySelector('.sc-map-scroll');
    var saved = scroller ? { left: scroller.scrollLeft, top: scroller.scrollTop } : null;
    renderMapPanel(mapPanel, rom);
    var next = mapPanel.querySelector('.sc-map-scroll');
    if (next && saved) { next.scrollLeft = saved.left; next.scrollTop = saved.top; }
  };

  OB64.renderScenarioTab = renderScenarioTab;
  OB64.scenario = {
    ensureState: ensureState,
    collectProject: collectProject,
    loadProject: loadProject,
    exportScenarioArchives: exportScenarioArchives,
    patchRegions: publicRelocationRegions,
    iconProvider: iconProvider,
    keyModified: keyModified,
    siteAllegianceCensus: siteAllegianceCensus,
    leaderClassHasMapSprite: leaderClassHasMapSprite,
    spritelessLeaderIssues: spritelessLeaderIssues,
    _modelTest: {
      resolvePointForRow: resolvePointForRow,
      rowRuntime: rowRuntime,
      parseByte: parseByte,
      allocNode: allocNode,
      allocExtra: allocExtra,
      applyTemplate: applyTemplate,
      planNodeGc: planNodeGc,
      applyNodeGcPlan: applyNodeGcPlan,
      runNodeGc: runNodeGc,
      applyStartGateReplacement: applyStartGateReplacement,
      planDeleteTrigger: planDeleteTrigger,
      applyDeleteTriggerPlan: applyDeleteTriggerPlan,
      triggerRefNodes: triggerRefNodes,
      describeExtra: describeExtra,
      resetBuilderState: resetBuilderState,
    },
  };
})(window.OB64);
