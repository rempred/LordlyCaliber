// Shared frozen-frame iris lifecycle. The caller declares product capture timing.
window.OB64 = window.OB64 || {};
(function(O) {
  'use strict';
  function fail(message, code) {
    var error = new Error(message);
    error.code = code || 'framebuffer-input';
    throw error;
  }
  function copyRecord(record) { return new Uint8Array(record); }
  function cloneLayers(rows) {
    return rows.map(function(row) {
      return { resource: row.resource, record: copyRecord(row.record) };
    });
  }
  function Iris(count, selected, layers) {
    if (!Number.isInteger(count) || count < 2 || count > 20 ||
        !Number.isInteger(selected) || selected < 0 || selected >= count ||
        !Array.isArray(layers) || layers.length !== 20 || layers.some(function(row) {
          return !row || !(row.record instanceof Uint8Array) || row.record.length !== 88;
        })) fail('Iris requires twenty current layer records and valid active/selected indexes.', 'framebuffer-layers');
    this.count = count;
    this.selected = selected;
    this.layers = cloneLayers(layers);
    this.savedIndex = null;
    this.saved = null;
    this.record = null;
    this.imageId = null;
  }
  Iris.prototype.create = function(words, imageId, actors, effects) {
    if (!Array.isArray(words) || words.length !== 9 ||
        words.some(function(value) { return !Number.isInteger(value) || value < -2147483648 || value > 4294967295; })) {
      fail('Iris requires nine integer operands.');
    }
    var duration = words[6] | 0, phase = words[7] >>> 0;
    if (duration < 1 || duration > 30000 || ![0, 1].includes(phase)) {
      fail('Iris supports a positive bounded duration and paired close/open phases.');
    }
    var args = words.slice(0, 6).map(function(value) { return value << 16 >> 16; });
    if (args[0] < 0 || args[1] < 0 || args[5] < 0 || args[5] > 255 ||
        args.slice(0, 4).some(function(value) { return Math.abs(value) * duration > 2147483647; }) ||
        Math.max(args[0] * 1.3, 640) + Math.abs(args[2]) > 32767 ||
        Math.max(args[1] * 1.3, 480) + Math.abs(args[3]) > 32767) {
      fail('Iris geometry exceeds the supported signed interpolation domain.');
    }
    if (phase === 0 && (this.record || imageId == null)) {
      fail('Closing iris requires a fresh singleton and a current product render target.', 'framebuffer-target');
    }
    if (phase === 1 && (!this.record || this.record.phase !== 0 ||
        this.record.progress <= this.record.duration || !this.saved || this.imageId == null)) {
      fail('Opening iris requires a completed close and its saved layers.', 'framebuffer-phase');
    }
    this.record = {
      progress: phase ? duration : 0, duration: duration,
      radiusX: args[0], radiusY: args[1], centerX: args[2], centerY: args[3],
      stored: args[4], alpha: args[5], phase: phase, layer: this.count - 2
    };
    if (!phase) {
      this.imageId = imageId;
      // The native comparisons differ at equality. Preserve that distinction.
      actors.forEach(function(actor) {
        if (actor.layer >= this.selected) actor.layer = (actor.layer - 1) & 255;
      }, this);
      effects.forEach(function(effect) {
        if (effect.layer > this.selected) effect.layer = (effect.layer - 1) & 255;
      }, this);
      var chosen = this.layers[this.selected];
      for (var i = this.selected; i < this.count - 1; i++) this.layers[i] = this.layers[i + 1];
      this.layers[this.count - 1] = chosen;
      this.savedIndex = this.selected;
      this.selected = this.count - 1;
    } else {
      this.layers = cloneLayers(this.saved);
      // Runtime history retains its own bounded image for seeking.
      this.imageId = null;
    }
  };
  Iris.prototype.query = function() {
    return this.record && this.record.progress <= this.record.duration ? 1 : 0;
  };
  Iris.prototype.advance = function() {
    var record = this.record;
    if (!record) return false;
    if (!record.phase) {
      if (record.progress > record.duration) return false;
      if (record.progress === record.duration) {
        this.saved = cloneLayers(this.layers);
        this.layers[0].resource = { kind: 'framebuffer', id: this.imageId };
        this.layers[0].record.fill(0, 64);
        new DataView(this.layers[0].record.buffer).setFloat32(84, 1);
        this.layers[1] = {
          resource: this.layers[this.selected].resource,
          record: copyRecord(this.layers[this.selected].record)
        };
        for (var i = 2; i < 20; i++) {
          this.layers[i].resource = null;
          this.layers[i].record.fill(0, 64);
          new DataView(this.layers[i].record.buffer).setFloat32(84, 1);
        }
        record.layer = 0;
        record.progress++;
        return true; // The caller must release all current Actors and sprite effects.
      }
      record.progress++;
    } else if (record.progress === 0) {
      this.selected = this.savedIndex;
      var last = this.layers[this.count - 1];
      for (var i = this.count - 2; i >= this.selected; i--) this.layers[i + 1] = this.layers[i];
      this.layers[this.selected] = last;
      this.record = null;
      this.saved = null;
      this.savedIndex = null;
    } else record.progress--;
    return false;
  };
  Iris.prototype.recordBytes = function() {
    if (!this.record) return null;
    var record = this.record, view = new DataView(new ArrayBuffer(24));
    view.setInt32(0, record.progress);
    view.setInt32(4, record.duration);
    ['radiusX', 'radiusY', 'centerX', 'centerY', 'stored', 'alpha'].forEach(function(key, i) {
      view.setInt16(8 + i * 2, record[key]);
    });
    view.setUint8(20, record.phase);
    view.setUint8(21, record.layer);
    return new Uint8Array(view.buffer);
  };
  function geometry(record) {
    if (!record) return null;
    if (record.progress >= record.duration) return { quad: true, alpha: record.alpha };
    return {
      radiusX: Math.trunc(record.radiusX * (record.duration - record.progress) / record.duration),
      radiusY: Math.trunc(record.radiusY * (record.duration - record.progress) / record.duration),
      centerX: Math.trunc(record.centerX * record.progress / record.duration),
      centerY: Math.trunc(record.centerY * record.progress / record.duration),
      alpha: record.alpha
    };
  }
  function mesh(geometry) {
    if (geometry.quad) return [[-320, -240, geometry.alpha], [-320, 240, geometry.alpha],
      [320, 240, geometry.alpha], [320, -240, geometry.alpha]];
    var output = [];
    // Native geometry uses three rings and a duplicate endpoint at 360 degrees.
    for (var degrees = 0; degrees <= 360; degrees += 12) {
      var angle = Math.fround(degrees * Math.PI / 180);
      var cosine = Math.fround(Math.cos(angle)), sine = Math.fround(Math.sin(angle));
      var x = Math.fround(geometry.radiusX * cosine), y = Math.fround(geometry.radiusY * sine);
      output.push([Math.trunc(x) + geometry.centerX, Math.trunc(y) + geometry.centerY, 0],
        [Math.trunc(x * 1.3) + geometry.centerX, Math.trunc(y * 1.3) + geometry.centerY, geometry.alpha],
        [Math.trunc(Math.fround(cosine * 640)) + geometry.centerX,
          Math.trunc(Math.fround(sine * 480)) + geometry.centerY, geometry.alpha]);
    }
    return output;
  }
  function snapshot(iris) {
    return {
      record: iris.record ? Object.assign({}, iris.record) : null,
      selected: iris.selected, count: iris.count, imageId: iris.imageId,
      layers: iris.layers.map(function(row) {
        var view = new DataView(row.record.buffer, row.record.byteOffset, 88);
        return { resource: row.resource, transform: {
          translateX: view.getFloat32(64), translateY: view.getFloat32(68),
          rotationX: view.getFloat32(72), rotationY: view.getFloat32(76),
          translateZ: view.getFloat32(80), uniformScale: view.getFloat32(84)
        } };
      })
    };
  }
  function validateRom(rom) {
    if (!(rom instanceof Uint8Array) || !O.cutsceneFramebufferWords) {
      fail('Framebuffer ROM qualification is unavailable.', 'framebuffer-image');
    }
    var view = new DataView(rom.buffer, rom.byteOffset, rom.byteLength);
    O.cutsceneFramebufferWords.forEach(function(row) {
      if (row[0] + 4 > rom.length || view.getUint32(row[0]) !== row[1]) {
        fail('Framebuffer code differs from the qualified ROM.', 'framebuffer-image');
      }
    });
  }
  O.cutsceneFramebuffer = { validateRom: validateRom, Iris: Iris,
    geometry: geometry, mesh: mesh, snapshot: snapshot, fail: fail };
})(window.OB64);
