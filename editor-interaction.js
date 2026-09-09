/* Shared interaction state for Art and Animation and Sprite Editor. */
window.OB64 = window.OB64 || {};
(function() {
  'use strict';
  var runtimes = new Map(), dialogs = new WeakMap(), objectKeys = new WeakMap(), nextObjectKey = 0, dialogStack = [];
  function objectKey(value) { if (!objectKeys.has(value)) objectKeys.set(value, String(++nextObjectKey)); return objectKeys.get(value); }
  function node(tag, text) { var result = document.createElement(tag); if (text !== undefined) result.textContent = text; return result; }
  function timeline(frames) {
    var total = 0;
    var entries = frames.map(function(frame, index) {
      var duration = Math.max(0, Number(frame.ticks) || 0), start = total; total += duration;
      return { index: index, start: start, end: total };
    });
    return { entries: entries, total: total };
  }
  function frameAt(frames, milliseconds) {
    var table = timeline(frames);
    if (!table.total) return 0;
    var tick = Math.max(0, milliseconds) * 30 / 1000 % table.total;
    var entry = table.entries.find(function(row) { return tick >= row.start && tick < row.end; });
    return entry ? entry.index : 0;
  }
  function transportState(ui, key, identity, frame) {
    var model = ui[key];
    if (!model || model.identity !== identity) ui[key] = model = { identity: identity, playing: false, speed: 1, ms: 0, frame: frame || 0 };
    return model;
  }
  function syncTransport(model, frames, selected, identity) {
    if (model.editingFrame !== selected || model.count !== frames.length) {
      model.playing = false; model.frame = Math.max(0, Math.min(frames.length - 1, selected));
      model.ms = timeline(frames).entries[model.frame].start * 1000 / 30; model.manualIdentity = identity;
    }
    model.editingFrame = selected; model.count = frames.length;
  }
  function releaseWithin(root) {
    runtimes.forEach(function(runtime, model) {
      runtime.listeners = runtime.listeners.filter(function(row) { return !root.contains(row.host); });
      if (!runtime.listeners.length) {
        if (runtime.request !== null) window.cancelAnimationFrame(runtime.request);
        runtime.request = null; runtime.last = null; runtimes.delete(model);
      }
    });
  }
  function stopAll() {
    runtimes.forEach(function(runtime, model) {
      model.playing = false;
      if (runtime.request !== null) window.cancelAnimationFrame(runtime.request);
      runtime.request = null; runtime.last = null;
      runtime.listeners.forEach(function(row) { if (row.host.isConnected !== false) row.update(); });
    });
  }
  if (typeof document !== 'undefined') document.addEventListener('visibilitychange', function() { if (document.hidden) stopAll(); });
  function mountTransport(host, model, frames, draw, options) {
    options = options || {};
    var runtime = runtimes.get(model);
    if (!runtime) { runtime = { listeners: [], request: null, last: null }; runtimes.set(model, runtime); }
    var bar = node('div'); bar.className = 'editor-transport'; host.appendChild(bar);
    var feedback = node('span'); feedback.setAttribute('aria-live', 'off'); bar.appendChild(feedback);
    var play, scrub;
    function update() {
      var index = !model.playing && model.manualIdentity === options.identity ? Math.min(frames.length - 1, model.frame) : frameAt(frames, model.ms);
      index = Math.max(0, index); if (options.controls !== false) model.frame = index; draw(index);
      feedback.textContent = 'Playback frame ' + (index + 1) + ' / ' + frames.length;
      if (play) play.textContent = model.playing ? 'Pause' : 'Play';
      if (scrub) scrub.value = String(index);
    }
    function emit() { runtime.listeners = runtime.listeners.filter(function(row) { return row.host.isConnected !== false; }); runtime.listeners.forEach(function(row) { row.update(); }); }
    function schedule() {
      if (!model.playing || runtime.request !== null || !window.requestAnimationFrame) return;
      runtime.request = window.requestAnimationFrame(function(timestamp) {
        runtime.request = null;
        runtime.listeners = runtime.listeners.filter(function(row) { return row.host.isConnected !== false; });
        if (!runtime.listeners.length) { model.playing = false; runtime.last = null; runtimes.delete(model); return; }
        if (document.hidden) { model.playing = false; runtime.last = null; emit(); return; }
        if (runtime.last !== null) model.ms += Math.max(0, timestamp - runtime.last) * model.speed;
        runtime.last = timestamp; emit(); schedule();
      });
    }
    function seek(index) {
      model.playing = false; model.frame = Math.max(0, Math.min(frames.length - 1, index));
      model.manualIdentity = options.identity; model.ms = timeline(frames).entries[model.frame].start * 1000 / 30;
      if (runtime.request !== null) window.cancelAnimationFrame(runtime.request); runtime.request = null;
      runtime.last = null; emit(); if (options.onSelect) options.onSelect(model.frame);
    }
    function action(label, callback) { var control = node('button', label); control.type = 'button'; control.addEventListener('click', callback); bar.appendChild(control); return control; }
    if (options.controls !== false) {
      play = action(model.playing ? 'Pause' : 'Play', function() {
        model.playing = !model.playing && timeline(frames).total > 0 && frames.length > 1;
        if (!model.playing && runtime.request !== null) { window.cancelAnimationFrame(runtime.request); runtime.request = null; }
        runtime.last = null; emit(); schedule();
      });
      play.setAttribute('data-art-focus-key', 'transport-play'); play.setAttribute('data-sprite-focus-key', 'transport-play');
      play.disabled = frames.length < 2 || timeline(frames).total === 0;
      action('Previous frame', function() { seek(((model.playing ? frameAt(frames, model.ms) : model.frame) + frames.length - 1) % frames.length); });
      action('Next frame', function() { seek(((model.playing ? frameAt(frames, model.ms) : model.frame) + 1) % frames.length); });
      var speed = node('select'); speed.setAttribute('aria-label', 'Playback speed');
      [0.25, 0.5, 1, 2, 4].forEach(function(value) { var option = node('option', value + '×'); option.value = String(value); speed.appendChild(option); });
      speed.value = String(model.speed); speed.addEventListener('change', function() { model.speed = Number(speed.value); runtime.last = null; }); bar.appendChild(speed);
      scrub = node('input'); scrub.type = 'range'; scrub.min = '0'; scrub.max = String(frames.length - 1); scrub.step = '1';
      scrub.setAttribute('aria-label', 'Playback frame'); scrub.addEventListener('input', function() { seek(Number(scrub.value)); }); bar.appendChild(scrub);
      bar.appendChild(node('small', 'Approximate editor timing · 30 ticks/sec. Editing frame changes when stepping or scrubbing.'));
    }
    runtime.listeners.push({ host: host, update: update }); update(); schedule();
    return { seek: seek, update: emit, model: model };
  }
  function focusable(root) {
    return Array.from(root.querySelectorAll('button, input, select, textarea, a[href], [tabindex]')).filter(function(item) {
      return !item.disabled && item.tabIndex !== -1 && item.type !== 'hidden' && !item.hidden &&
        !(item.closest && item.closest('[hidden]')) && (!item.getClientRects || item.getClientRects().length > 0);
    });
  }
  function bindDialog(overlay) {
    if (dialogs.has(overlay)) return;
    stopAll();
    var launcher = document.activeElement;
    function trap(event) {
      if (event.key !== 'Tab' || event.defaultPrevented || dialogStack[dialogStack.length - 1] !== overlay) return;
      var rows = focusable(overlay); if (!rows.length) { event.preventDefault(); return; }
      var index = rows.indexOf(document.activeElement);
      if (index < 0 || (event.shiftKey && index === 0) || (!event.shiftKey && index === rows.length - 1)) {
        event.preventDefault(); rows[event.shiftKey ? rows.length - 1 : 0].focus();
      }
    }
    overlay.addEventListener('keydown', trap); document.addEventListener('keydown', trap); dialogStack.push(overlay);
    dialogs.set(overlay, { launcher: launcher, trap: trap, spriteKey: launcher && launcher.getAttribute('data-sprite-focus-key'), artKey: launcher && launcher.getAttribute('data-art-focus-key') });
  }
  function releaseDialog(overlay) {
    releaseWithin(overlay);
    var saved = dialogs.get(overlay); if (!saved) return;
    overlay.removeEventListener('keydown', saved.trap); document.removeEventListener('keydown', saved.trap); dialogs.delete(overlay);
    dialogStack = dialogStack.filter(function(row) { return row !== overlay; });
    var target = saved.launcher;
    if (!target || target.isConnected === false) {
      target = saved.spriteKey ? document.querySelector('[data-sprite-focus-key="' + saved.spriteKey + '"]') :
        saved.artKey ? document.querySelector('[data-art-focus-key="' + saved.artKey + '"]') : null;
    }
    if (!target || target.isConnected === false || target.disabled) target = document.querySelector('[data-editor-focus-fallback]');
    if (target && target.focus) target.focus({ preventScroll: true });
  }
  function semanticFocus(root, prefix, identity) {
    var counts = {};
    Array.from(root.querySelectorAll('input, select, button, canvas, textarea')).forEach(function(control) {
      if (control.getAttribute(prefix)) return;
      var label = control.getAttribute('aria-label') || control.textContent || control.name || control.type || control.tagName;
      var key = identity + ':' + String(label).trim().replace(/[^a-zA-Z0-9_-]+/g, '-');
      counts[key] = (counts[key] || 0) + 1;
      control.setAttribute(prefix, key + ':' + counts[key]);
    });
  }
  function preserveFocus(root, prefix, identity, action) {
    semanticFocus(root, prefix, identity);
    var active = document.activeElement, inside = active && root.contains(active);
    var key = inside ? active.getAttribute(prefix) : null;
    var start = inside && typeof active.selectionStart === 'number' ? active.selectionStart : null;
    var end = inside && typeof active.selectionEnd === 'number' ? active.selectionEnd : null;
    var scroll = [root].concat(Array.from(root.querySelectorAll('*'))).filter(function(row) { return row.scrollTop || row.scrollLeft; })
      .map(function(row) { return { node: row, top: row.scrollTop, left: row.scrollLeft }; });
    var result = action(); semanticFocus(root, prefix, identity);
    if (inside) {
      var replacement = key ? root.querySelector('[' + prefix + '="' + key + '"]') : null;
      if (!replacement || replacement.disabled) replacement = focusable(root)[0];
      if (replacement) {
        replacement.focus({ preventScroll: true });
        if (start !== null && replacement.setSelectionRange) replacement.setSelectionRange(start, end);
      }
    }
    scroll.forEach(function(row) { if (root.contains(row.node)) { row.node.scrollTop = row.top; row.node.scrollLeft = row.left; } });
    return result;
  }

  function pixelKeyboard(canvas, ui, key, width, height, action, redraw) {
    var cursor = ui[key] || { x: 0, y: 0 }; cursor.anchor = null;
    cursor.x = Math.max(0, Math.min(width - 1, cursor.x)); cursor.y = Math.max(0, Math.min(height - 1, cursor.y)); ui[key] = cursor;
    canvas.setAttribute('aria-label', 'Editable pixel canvas. Arrows move; Space paints; Delete erases; I samples; Shift+arrows selects.');
    canvas.addEventListener('keydown', function(event) {
      if (event.target !== canvas || event.ctrlKey || event.metaKey || event.altKey) return;
      var moves = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
      if (moves[event.key]) {
        event.preventDefault();
        if (event.shiftKey && !cursor.anchor) cursor.anchor = { x: cursor.x, y: cursor.y };
        if (!event.shiftKey) cursor.anchor = null;
        cursor.x = Math.max(0, Math.min(width - 1, cursor.x + moves[event.key][0]));
        cursor.y = Math.max(0, Math.min(height - 1, cursor.y + moves[event.key][1]));
        if (cursor.anchor) action('select', cursor, { x: Math.min(cursor.x, cursor.anchor.x), y: Math.min(cursor.y, cursor.anchor.y),
          width: Math.abs(cursor.x - cursor.anchor.x) + 1, height: Math.abs(cursor.y - cursor.anchor.y) + 1 });
        redraw(cursor); return;
      }
      var operation = { ' ': 'paint', Enter: 'paint', Delete: 'erase', Backspace: 'erase', i: 'sample', I: 'sample' }[event.key];
      if (operation) { event.preventDefault(); action(operation, cursor); redraw(cursor); }
    });
    return cursor;
  }
  OB64.editorInteraction = { timeline: timeline, frameAt: frameAt, transportState: transportState,
    mountTransport: mountTransport, releaseWithin: releaseWithin, syncTransport: syncTransport, stopAll: stopAll, bindDialog: bindDialog, releaseDialog: releaseDialog,
    semanticFocus: semanticFocus, preserveFocus: preserveFocus, objectKey: objectKey, pixelKeyboard: pixelKeyboard };
})();
