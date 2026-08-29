// OB64 Mod Editor - complete ROM-mapped combat-animation corpus UI

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var M = OB64.animationArt;
  var IDLE_ACTION_ID = -1;
  var CLASS_MOTION_SPECS = [
    {
      actionId: -2,
      actionName: 'Walk / Run · Advance',
      kind: 'advance',
      selector: 0x05
    },
    {
      actionId: -3,
      actionName: 'Walk / Run · Return',
      kind: 'return',
      selector: 0x0B
    }
  ];
  var animationPreviewSurfaceCache = null;
  var activePreviewBackground = 'checkerboard';

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

  function nextBrowserPaint() {
    return new Promise(function(resolve) {
      if (typeof window !== 'undefined' &&
          typeof window.requestAnimationFrame === 'function') {
        window.requestAnimationFrame(function() { window.setTimeout(resolve, 0); });
      } else {
        setTimeout(resolve, 0);
      }
    });
  }

  function badge(text, kind) { return element('span', 'art-badge art-badge-' + kind, text); }
  function notify(options, text) { if (options && options.onStatus) options.onStatus(text); }
  function changed(options) { if (options && options.onChange) options.onChange(); }

  function animationRouteChanged(options) {
    if (options && options.onAnimationRouteChange) {
      options.onAnimationRouteChange();
      return;
    }
    changed(options);
    if (options && options.onAnimationMappingChange) {
      options.onAnimationMappingChange();
    }
  }

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

  function childOrdinalForSource(source, weaponChildOrdinal, layer) {
    if (!source.weaponSelectable) {
      return M.childOrdinalOrFallback(source,
        layer && Number.isInteger(layer.selectedChildOrdinal)
          ? layer.selectedChildOrdinal : source.childOrdinal);
    }
    var ordinal = Number.isInteger(weaponChildOrdinal) ? weaponChildOrdinal : 0;
    return M.childOrdinalOrFallback(source, ordinal);
  }

  function editFor(animationState, sourceKey, childOrdinal, overrides) {
    var identity = M.childKey(sourceKey, childOrdinal);
    return overrides && overrides[identity]
      ? overrides[identity]
      : M.currentEdit(animationState, sourceKey, childOrdinal);
  }

  function layerPreviewMetrics(animation, layer) {
    var explicit = animation && animation.previewLayerMetrics && layer &&
      layer.copyPreviewKey
      ? animation.previewLayerMetrics[layer.copyPreviewKey] || null : null;
    if (explicit) return explicit;
    var source = animation && layer && animation.artByKey
      ? animation.artByKey[layer.sourceKey] : null;
    if (!source ||
        source.childSelectionPolicy !== 'cutscene-actor-appearance') return null;
    var scaleX = Number(layer.scaleXRaw) / 1024;
    var scaleY = Number(layer.scaleYRaw) / 1024;
    if (!Number.isFinite(scaleX) || scaleX < 0 ||
        !Number.isFinite(scaleY) || scaleY < 0) return null;
    return {
      left: Math.round(layer.drawOffsetX * scaleX),
      top: Math.round(layer.drawOffsetY * scaleY),
      width: Math.max(1, Math.round(layer.width * scaleX)),
      height: Math.max(1, Math.round(layer.height * scaleY)),
      flipX: (layer.flags & 0x01) !== 0,
      flipY: (layer.flags & 0x02) !== 0
    };
  }

  function drawLayerInto(output, animation, layer, animationState, style, overrides,
      weaponChildOrdinal) {
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = childOrdinalForSource(source, weaponChildOrdinal, layer);
    var pixels = source.editable
      ? editFor(animationState, layer.sourceKey, childOrdinal, overrides)
      : M.displayChild(animationState, layer.sourceKey, childOrdinal);
    var palette = source.editable ? M.childPalette(source, childOrdinal) : null;
    var canvas = animation.canvas;
    var metrics = layerPreviewMetrics(animation, layer);
    var left = (metrics ? metrics.left : layer.drawOffsetX) - canvas.originX;
    var top = (metrics ? metrics.top : layer.drawOffsetY) - canvas.originY;
    var drawWidth = metrics ? metrics.width : layer.width;
    var drawHeight = metrics ? metrics.height : layer.height;
    for (var y = 0; y < drawHeight; y++) {
      for (var x = 0; x < drawWidth; x++) {
        var outputX = left + x, outputY = top + y;
        if (outputX < 0 || outputY < 0 || outputX >= canvas.width ||
            outputY >= canvas.height) continue;
        var sourceX = metrics ? Math.min(layer.width - 1,
          Math.floor(x * layer.width / drawWidth)) : x;
        var sourceY = metrics ? Math.min(layer.height - 1,
          Math.floor(y * layer.height / drawHeight)) : y;
        if (metrics && metrics.flipX) sourceX = layer.width - 1 - sourceX;
        if (metrics && metrics.flipY) sourceY = layer.height - 1 - sourceY;
        var pixel = sourceY * source.sprite.width + sourceX;
        var alpha, rgb;
        if (source.editable) {
          var intensity = pixels.intensity[pixel];
          if (!intensity) continue;
          rgb = wordRgb(palette[pixels.indices[pixel]]);
          alpha = intensity * 17;
        } else {
          alpha = pixels.alpha[pixel];
          if (!alpha) continue;
          rgb = wordRgb(pixels.words[pixel]);
        }
        if (style === 'context') {
          var gray = Math.round((rgb[0] * 3 + rgb[1] * 6 + rgb[2]) / 10);
          rgb = [Math.round((gray + 112) / 2), Math.round((gray + 112) / 2),
            Math.round((gray + 112) / 2)];
          alpha = Math.max(24, Math.round(alpha * 0.34));
        }
        var targetIndex = (outputY * canvas.width + outputX) * 4;
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

  function singleLayerPixels(animation, layer, animationState, weaponChildOrdinal) {
    var output = new Uint8ClampedArray(
      animation.canvas.width * animation.canvas.height * 4);
    drawLayerInto(output, animation, layer, animationState, 'normal', null,
      weaponChildOrdinal);
    return output;
  }

  function checker(context, width, height, scale) {
    context.fillStyle = '#d8d2bc'; context.fillRect(0, 0, width * scale, height * scale);
    context.fillStyle = '#8d887a';
    for (var y = 0; y < height; y++) for (var x = 0; x < width; x++) {
      if ((x + y) & 1) context.fillRect(x * scale, y * scale, scale, scale);
    }
  }

  function normalizePreviewBackground(mode) {
    if (mode === 'white' || mode === 'transparent') return mode;
    return 'checkerboard';
  }

  function paintPixels(canvas, width, height, pixels, scale, backgroundMode) {
    canvas.width = width * scale; canvas.height = height * scale;
    var context = canvas.getContext('2d');
    context.imageSmoothingEnabled = false;
    backgroundMode = normalizePreviewBackground(backgroundMode === undefined
      ? activePreviewBackground : backgroundMode);
    if (backgroundMode === 'transparent' && scale === 1) {
      var imageData = context.createImageData(width, height);
      imageData.data.set(pixels);
      context.putImageData(imageData, 0, 0);
      return;
    }
    if (backgroundMode === 'white') {
      context.fillStyle = '#ffffff';
      context.fillRect(0, 0, width * scale, height * scale);
    } else if (backgroundMode === 'checkerboard') {
      checker(context, width, height, scale);
    }
    for (var y = 0; y < height; y++) for (var x = 0; x < width; x++) {
      var offset = (y * width + x) * 4, alpha = pixels[offset + 3];
      if (!alpha) continue;
      context.fillStyle = 'rgba(' + pixels[offset] + ',' + pixels[offset + 1] + ',' +
        pixels[offset + 2] + ',' + (alpha / 255).toFixed(4) + ')';
      context.fillRect(x * scale, y * scale, scale, scale);
    }
  }

  function drawFrame(canvas, animation, frame, animationState, scale,
      selectedLayer, overrides, editorGrid, weaponChildOrdinal, backgroundMode) {
    var selectedKey = selectedLayer ? selectedLayer.sourceKey : null;
    paintPixels(canvas, animation.canvas.width, animation.canvas.height,
      framePixels(animation, frame, animationState, selectedKey, overrides,
        weaponChildOrdinal), scale, backgroundMode);
    if (!selectedLayer) return;
    var context = canvas.getContext('2d');
    var metrics = layerPreviewMetrics(animation, selectedLayer);
    var left = ((metrics ? metrics.left : selectedLayer.drawOffsetX) -
      animation.canvas.originX) * scale;
    var top = ((metrics ? metrics.top : selectedLayer.drawOffsetY) -
      animation.canvas.originY) * scale;
    var width = (metrics ? metrics.width : selectedLayer.width) * scale;
    var height = (metrics ? metrics.height : selectedLayer.height) * scale;
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
    var selectedSource = animation.artByKey[selectedLayer.sourceKey];
    context.strokeStyle = selectedSource.editable ? '#ffd45c' : '#a81913';
    context.lineWidth = Math.max(2, Math.round(scale / 3));
    context.setLineDash([scale, Math.max(2, Math.round(scale / 2))]);
    context.strokeRect(left + 1, top + 1, Math.max(1, width - 2), Math.max(1, height - 2));
    context.restore();
  }

  function selectorFlags(animation) {
    if (animation && animation.mappingStatus &&
        animation.mappingStatus.selectorFlags) {
      return animation.mappingStatus.selectorFlags;
    }
    var match = String(animation && animation.spec
      ? animation.spec.variantLabel || '' : '').match(/flags\s+(\d)\/(\d)/);
    return match ? match[1] + '/' + match[2] : null;
  }

  function selectorFlagParts(animation) {
    var match = String(selectorFlags(animation) || '').match(/^(\d)\/(\d)$/);
    return match ? [Number(match[1]), Number(match[2])] : [2, 2];
  }

  function animationSideLabel(animation) {
    return selectorFlagParts(animation)[1] === 1
      ? 'Enemy Side' : 'Player Side';
  }

  function animationSideClass(animation) {
    return selectorFlagParts(animation)[1] === 1
      ? 'animation-variant-enemy' : 'animation-variant-player';
  }

  function reversedEnemyArtVariants(animation) {
    var status = animation && animation.mappingStatus;
    var artClassId = status && Number.isInteger(status.artClassId)
      ? status.artClassId : animation.spec.classId;
    return selectorFlagParts(animation)[1] === 1 &&
      (artClassId === 0x0F || artClassId === 0x1E);
  }

  function animationArtVariantLabel(animation) {
    var alternate = selectorFlagParts(animation)[0] === 1;
    if (reversedEnemyArtVariants(animation)) alternate = !alternate;
    return alternate
      ? 'Alternate Art' : 'Base Art';
  }

  function animationLaneKey(animation) {
    if (animation && animation.spec && animation.spec.idleSequence) {
      return 'idle';
    }
    if (animation && animation.spec && animation.spec.classMotionKind) {
      return animation.spec.classMotionKind;
    }
    return animation && animation.spec && animation.spec.rawMode === 2
      ? 'blocked' : 'normal';
  }

  function animationLaneLabel(animation) {
    var laneKey = animationLaneKey(animation);
    if (laneKey === 'idle') return 'Idle Loop';
    if (laneKey === 'advance') return 'Walk / Run · Advance';
    if (laneKey === 'return') return 'Walk / Run · Return';
    return laneKey === 'blocked' ? 'Attack Blocked' : 'Normal Attack';
  }

  function isIdleAnimation(animation) {
    return !!(animation && animation.spec && animation.spec.idleSequence);
  }

  function isClassMotionAnimation(animation) {
    return !!(animation && animation.spec && animation.spec.classMotionKind);
  }

  function classMotionSpec(value) {
    var kind = value && value.spec ? value.spec.classMotionKind : value;
    return CLASS_MOTION_SPECS.find(function(spec) {
      return spec.kind === kind || spec.actionId === Number(kind);
    }) || null;
  }

  function idleLoopFrames(animation) {
    var records = animation && animation.poseProgram &&
      animation.poseProgram.records;
    if (!Array.isArray(records) || !records.length) {
      throw new Error('selector 0x00 has no readable pose records');
    }
    var loop = records[records.length - 1];
    if (loop.opcode !== 0x04 || !loop.operands ||
        !Number.isInteger(loop.operands[0])) {
      throw new Error('selector 0x00 does not end with a readable idle-loop jump');
    }
    var frameRecords = records.filter(function(record) {
      return record.opcode === 0x01;
    });
    var firstLoopFrame = frameRecords.findIndex(function(record) {
      return record.ordinal >= loop.operands[0];
    });
    if (firstLoopFrame < 0 || firstLoopFrame >= animation.frames.length) {
      throw new Error('selector 0x00 idle-loop jump does not reach a frame');
    }
    return animation.frames.slice(firstLoopFrame).map(function(frame, index) {
      return Object.assign({}, frame, { sequenceIndex: index });
    });
  }

  function signedPoseByte(value) {
    value = Number(value) & 0xFF;
    return value & 0x80 ? value - 0x100 : value;
  }

  function animationPoseOffsetSummary(animation) {
    var records = animation && animation.poseProgram &&
      animation.poseProgram.records;
    if (!Array.isArray(records)) {
      return {
        label: 'Pose Offsets Unknown',
        title: 'The body program could not be read, so its local pose offsets ' +
          'are unknown. These fields do not prove a visible art shift or ' +
          'battlefield movement.'
      };
    }
    var offset = [0, 0, 0], peak = [0, 0, 0], recordCount = 0;
    records.forEach(function(record) {
      if (!record || record.opcode !== 0x0C ||
          !record.operands || record.operands.length < 3) return;
      recordCount++;
      var delta = [
        -signedPoseByte(record.operands[0]),
        signedPoseByte(record.operands[1]),
        signedPoseByte(record.operands[2])
      ];
      for (var field = 0; field < 3; field++) {
        offset[field] += delta[field];
        peak[field] = Math.max(peak[field], Math.abs(offset[field]));
      }
    });
    var changed = peak.some(function(value) { return value !== 0; });
    if (!changed) {
      return {
        label: 'No Pose Offset',
        title: 'This body program has no nonzero opcode 0x0C changes to its ' +
          'three local pose-source fields. This does not prove that the sprite ' +
          'art or battlefield actor stands still.',
        recordCount: recordCount,
        peak: peak,
        end: offset,
        returns: true
      };
    }
    var returns = offset.every(function(value) { return value === 0; });
    return {
      label: returns ? 'Pose Offsets Return' : 'Pose Offset Remains',
      title: 'This body program contains ' + recordCount + ' opcode 0x0C ' +
        'local pose-offset record' + (recordCount === 1 ? '' : 's') + '. ' +
        'Peak absolute changes for local pose-source fields +0x10, +0x0E, and +0x0C ' +
        'are ' + peak[0] + ', ' + peak[1] + ', and ' + peak[2] + '. ' +
        (returns ? 'All three local pose-source fields return to zero. ' :
          'The final local pose-source changes are ' + offset[0] + ', ' + offset[1] +
          ', and ' + offset[2] + '. ') +
        'These fields do not prove a visible art shift or battlefield movement.',
      recordCount: recordCount,
      peak: peak,
      end: offset,
      returns: returns
    };
  }

  function animationArtName(animation) {
    var status = animation && animation.mappingStatus;
    return status && status.artClassName
      ? status.artClassName : animation.spec.className;
  }

  var animationSequenceIdentityCache = typeof WeakMap === 'function'
    ? new WeakMap() : null;

  function animationSequenceStorageIdentity(animation) {
    var spec = animation && animation.spec || {};
    if (animation && (animation.separationId || spec.separatedCopy)) {
      return 'project:' + String(animation.separationId || animation.key || spec.key);
    }
    var poseProgram = animation && animation.poseProgram || {};
    if (Number.isInteger(spec.poseKey) &&
        Number.isInteger(poseProgram.start) &&
        Number.isInteger(poseProgram.end)) {
      return ['rom', spec.poseKey, poseProgram.start, poseProgram.end].join(':');
    }
    return 'animation:' + String(animation && animation.key || spec.key || 'unknown');
  }

  function animationPoseProgramBytes(animation) {
    var program = animation && animation.poseProgram &&
      animation.poseProgram.program;
    return program && Number.isInteger(program.length)
      ? Array.prototype.slice.call(program) : null;
  }

  function animationSequenceIdentity(animation) {
    var cacheable = !(animation && animation.spec && animation.spec.separatedCopy);
    if (cacheable && animationSequenceIdentityCache &&
        animationSequenceIdentityCache.has(animation)) {
      return animationSequenceIdentityCache.get(animation);
    }
    var identity = JSON.stringify([
      animationSequenceStorageIdentity(animation),
      animationPoseProgramBytes(animation),
      animation.spec.descriptorKey,
      animation.spec.metadataKey,
      animation.spec.poseKey,
      animation.spec.configKey,
      animation.spec.lookupKey,
      animation.spec.selector,
      animation.spec.selectedBodyChild,
      animation.spec.weaponChildCount,
      animation.spec.equipmentGroupIds || [],
      animation.canvas,
      animation.frames.map(function(frame) {
        return [frame.token, frame.ticks, frame.layers.map(function(layer) {
          return [layer.sourceKey, layer.selectedChildOrdinal, layer.artId,
            layer.width, layer.height, layer.drawOffsetX, layer.drawOffsetY,
            layer.lookupBank];
        })];
      })
    ]);
    if (cacheable && animationSequenceIdentityCache) {
      animationSequenceIdentityCache.set(animation, identity);
    }
    return identity;
  }

  function sameAnimationSequence(left, right) {
    return !!left && !!right &&
      animationSequenceIdentity(left) === animationSequenceIdentity(right);
  }

  function variantChoice(rows, laneKey, normalRows, splitNormal) {
    var first = rows[0];
    var rawModes = rows.map(function(row) { return row.spec.rawMode; })
      .sort(function(left, right) { return left - right; });
    var pointsToNormalRoute = laneKey === 'blocked' && normalRows.some(function(row) {
      return sameAnimationSequence(first, row);
    });
    var laneLabel = laneKey === 'idle' ? 'Idle Loop' :
      (laneKey === 'advance' ? 'Walk / Run · Advance' :
        (laneKey === 'return' ? 'Walk / Run · Return' :
          (laneKey === 'blocked' ? 'Attack Blocked' : 'Normal Attack')));
    if (splitNormal) laneLabel += ' · mode ' + rawModes.join('/');
    var issue = rows.some(function(row) {
      return row.mappingStatus && row.mappingStatus.severity === 'failure';
    });
    var poseOffsets = animationPoseOffsetSummary(first);
    return {
      key: first.key + ':ui-' + laneKey + '-' + rawModes.join('-'),
      representative: first,
      rows: rows,
      rawModes: rawModes,
      flags: selectorFlags(first),
      sideLabel: animationSideLabel(first),
      optionClass: animationSideClass(first),
      artName: animationArtName(first),
      artVariantLabel: animationArtVariantLabel(first),
      laneKey: laneKey,
      laneLabel: laneLabel,
      poseOffsets: poseOffsets,
      pointsToNormalRoute: pointsToNormalRoute,
      label: (issue ? '[Issue] ' : '') + animationArtVariantLabel(first) +
        ' · ' + laneLabel + ' · ' + poseOffsets.label,
      optionTitle: animationArtVariantLabel(first) + ' · Raw mode' +
        (rawModes.length === 1 ? ' ' : 's ') +
        rawModes.join(', ') + ' · ROM flags ' + (selectorFlags(first) || '?') +
        (reversedEnemyArtVariants(first)
          ? ' · Enemy-side Valkyrie/Freya art reverses the Base and Alternate slots'
          : '') + ' · ' + poseOffsets.title
    };
  }

  function nativeProgramVariantChoices(variants) {
    var flagOrder = [], byFlags = {};
    (variants || []).forEach(function(animation) {
      var flags = selectorFlags(animation) || '?';
      if (!byFlags[flags]) {
        byFlags[flags] = [];
        flagOrder.push(flags);
      }
      byFlags[flags].push(animation);
    });
    var output = [];
    flagOrder.forEach(function(flags) {
      var groups = [], byIdentity = new Map();
      byFlags[flags].slice().sort(function(left, right) {
        return left.spec.rawMode - right.spec.rawMode;
      }).forEach(function(animation) {
        var identity = animationSequenceIdentity(animation);
        var group = byIdentity.get(identity);
        if (!group) {
          group = [];
          byIdentity.set(identity, group);
          groups.push(group);
        }
        group.push(animation);
      });
      groups.forEach(function(rows, groupIndex) {
        var first = rows[0];
        var rawModes = rows.map(function(row) {
          return Number(row.spec.rawMode);
        }).filter(function(rawMode, index, values) {
          return values.indexOf(rawMode) === index;
        }).sort(function(left, right) { return left - right; });
        var issue = rows.some(function(row) {
          return row.mappingStatus && row.mappingStatus.severity === 'failure';
        });
        var poseOffsets = animationPoseOffsetSummary(first);
        output.push({
          key: first.key + ':ui-source-' + rawModes.join('-'),
          representative: first,
          rows: rows,
          rawModes: rawModes,
          flags: selectorFlags(first),
          sideLabel: animationSideLabel(first),
          optionClass: animationSideClass(first),
          artName: animationArtName(first),
          artVariantLabel: animationArtVariantLabel(first),
          laneKey: 'source',
          laneLabel: '',
          poseOffsets: poseOffsets,
          pointsToNormalRoute: false,
          nativeProgramChoice: true,
          nativeProgramOrdinal: groupIndex + 1,
          nativeProgramCount: groups.length,
          label: (issue ? '[Issue] ' : '') +
            animationArtVariantLabel(first) + ' · ' + poseOffsets.label,
          optionTitle: animationArtVariantLabel(first) +
            ' · Native body program source · Resolved raw modes ' +
            rawModes.join(', ') + ' · ROM flags ' +
            (selectorFlags(first) || '?') + ' · ' + poseOffsets.title
        });
      });
    });
    return output;
  }

  function variantChoiceReference(choice) {
    var parts = [];
    if (choice.sourceActionLabel) parts.push(choice.sourceActionLabel);
    parts.push(choice.sideLabel, choice.artName + ' Art', choice.artVariantLabel);
    if (choice.laneLabel) parts.push(choice.laneLabel);
    return parts.join(' · ');
  }

  function variantLinkReference(choice, target) {
    var parts = [];
    if (choice.sourceActionId !== target.sourceActionId &&
        target.sourceActionLabel) parts.push(target.sourceActionLabel);
    if (choice.sideLabel !== target.sideLabel) parts.push(target.sideLabel);
    if (choice.artName !== target.artName) parts.push(target.artName + ' Art');
    if (choice.artVariantLabel !== target.artVariantLabel) {
      parts.push(target.artVariantLabel);
    }
    if (choice.laneLabel !== target.laneLabel && target.laneLabel) {
      parts.push(target.laneLabel);
    }
    return parts.length ? parts.join(' / ') : variantChoiceReference(target);
  }

  function tagLinkedVariantChoices(choices) {
    var canonicalByIdentity = new Map();
    choices.forEach(function(choice) {
      var identity = animationSequenceIdentity(choice.representative);
      var target = canonicalByIdentity.get(identity);
      if (!target) {
        canonicalByIdentity.set(identity, choice);
        return;
      }
      choice.linkedToKey = target.key;
      choice.linkedToLabel = variantChoiceReference(target);
      choice.linkedReference = variantLinkReference(choice, target);
      choice.linkedTitle = 'Linked means this entry points to the exact same ' +
        'body program as ' + choice.linkedToLabel + '. Both entries share one ' +
        'stored body program, every program byte, frame, tick count, layer ' +
        'binding, and metadata record. Editing either entry changes the same ' +
        'sequence; no copy exists.';
      choice.optionTitle += ' · ' + choice.linkedTitle;
    });
    return choices;
  }

  function editedSequenceBadge() {
    var edited = badge('Edited', 'edited');
    edited.title = 'Edited means this is a private project sequence created ' +
      'with Copy From and Separate. It is stored separately from the original sequence.';
    edited.tabIndex = 0;
    return edited;
  }

  function linkedSequenceBadge(choice, targetAnimation) {
    var linked = badge('Linked', 'linked');
    var representative = choice && choice.representative;
    var selector = representative && representative.spec
      ? M.hex(representative.spec.selector, 2) : 'unknown';
    linked.title = choice && choice.linkedTitle
      ? choice.linkedTitle
      : 'Linked means this entry and the selected target point to the exact ' +
        'same body program. Both use selector ' + selector + ' and share one ' +
        'stored body program, every program byte, frame, tick count, layer ' +
        'binding, and metadata record. Editing either entry changes the same ' +
        'sequence; no copy exists.';
    if (targetAnimation && targetAnimation.spec) {
      linked.title += ' The selected target is ' + targetAnimation.spec.className +
        ' · ' + targetAnimation.spec.actionName + ' · ' +
        animationArtVariantLabel(targetAnimation) + ' · ' +
        animationLaneLabel(targetAnimation) + '.';
    }
    linked.tabIndex = 0;
    linked.setAttribute('aria-label', linked.title);
    return linked;
  }

  function animationVariantChoices(variants, options) {
    var flagOrder = [], byFlags = {};
    (variants || []).forEach(function(animation) {
      var flags = selectorFlags(animation) || '?';
      if (!byFlags[flags]) {
        byFlags[flags] = [];
        flagOrder.push(flags);
      }
      byFlags[flags].push(animation);
    });
    var output = [];
    flagOrder.forEach(function(flags) {
      var rows = byFlags[flags].slice().sort(function(left, right) {
        return left.spec.rawMode - right.spec.rawMode;
      });
      var idleRows = rows.filter(isIdleAnimation);
      if (idleRows.length) {
        output.push(variantChoice(idleRows, 'idle', [], false));
        return;
      }
      var motionRows = rows.filter(isClassMotionAnimation);
      if (motionRows.length) {
        output.push(variantChoice(
          motionRows, motionRows[0].spec.classMotionKind, [], false));
        return;
      }
      var normalRows = rows.filter(function(row) {
        return row.spec.rawMode !== 2;
      });
      var normalGroups = [], normalByIdentity = new Map();
      normalRows.forEach(function(row) {
        var identity = animationSequenceIdentity(row);
        var matching = normalByIdentity.get(identity);
        if (matching) matching.push(row);
        else {
          matching = [row];
          normalByIdentity.set(identity, matching);
          normalGroups.push(matching);
        }
      });
      normalGroups.forEach(function(group) {
        output.push(variantChoice(group, 'normal', normalRows,
          normalGroups.length > 1));
      });
      rows.filter(function(row) {
        return row.spec.rawMode === 2;
      }).forEach(function(row) {
        output.push(variantChoice([row], 'blocked', normalRows, false));
      });
    });
    return options && options.tagLinks === false
      ? output : tagLinkedVariantChoices(output);
  }

  function idleAnimationRows(animationState, classId) {
    classId = Number(classId);
    if (!animationState ||
        typeof animationState.resolveSelectorCandidate !== 'function') return [];
    if (!animationState.idleAnimationsByClass) {
      animationState.idleAnimationsByClass = {};
      animationState.idleAnimationsByKey = {};
      animationState.idleSequenceFailures = {};
    }
    if (animationState.idleAnimationsByClass[classId]) {
      return animationState.idleAnimationsByClass[classId];
    }
    var templates = [], byFlags = {};
    var routeTable = animationState.artRouteTemplatesByClass &&
      animationState.artRouteTemplatesByClass[classId];
    var routeTemplates = routeTable
      ? ['0/0', '0/1', '1/0', '1/1'].map(function(flags) {
        return routeTable[flags];
      }).filter(Boolean)
      : animationState.specs.filter(function(animation) {
        return animation.spec.classId === classId;
      }).sort(function(left, right) {
        return left.spec.rawMode - right.spec.rawMode ||
          left.spec.displayOrder - right.spec.displayOrder;
      });
    routeTemplates.forEach(function(animation) {
      var flags = selectorFlags(animation);
      if (!flags || byFlags[flags]) return;
      byFlags[flags] = true;
      templates.push(animation);
    });
    var failures = [];
    var rows = [];
    templates.forEach(function(template, index) {
      try {
        var candidate = animationState.resolveSelectorCandidate(template, 0, 0);
        var key = 'idle:' + candidate.key;
        var idle = Object.assign({}, candidate, {
          key: key,
          corpusId: key,
          compatibilityKey: null,
          frames: idleLoopFrames(candidate),
          spec: Object.assign({}, candidate.spec, {
            key: key,
            id: key,
            compatibilityKey: null,
            actionId: IDLE_ACTION_ID,
            actionName: 'Idle / Rest',
            rawMode: 0,
            modeLabel: 'Idle loop',
            selector: 0,
            idleSequence: true,
            displayOrder: index
          }),
          idleSequence: true,
          selectedAction: null
        });
        rows.push(idle);
        animationState.idleAnimationsByKey[key] = idle;
      } catch (error) {
        failures.push({
          classId: classId,
          flags: selectorFlags(template),
          rawMode: 0,
          selector: 0,
          message: error && error.message ? error.message : String(error)
        });
      }
    });
    animationState.idleAnimationsByClass[classId] = rows;
    animationState.idleSequenceFailures[classId] = failures;
    return rows;
  }

  function classMotionAnimationRows(animationState, classId, requestedKind) {
    classId = Number(classId);
    if (!animationState ||
        typeof animationState.resolveSelectorCandidate !== 'function') return [];
    if (!animationState.classMotionAnimationsByClass) {
      animationState.classMotionAnimationsByClass = {};
      animationState.classMotionAnimationsByKey = {};
      animationState.classMotionSequenceFailures = {};
    }
    if (!animationState.classMotionAnimationsByClass[classId]) {
      var routeTable = animationState.artRouteTemplatesByClass &&
        animationState.artRouteTemplatesByClass[classId];
      var templates = routeTable
        ? ['0/0', '0/1', '1/0', '1/1'].map(function(flags) {
          return routeTable[flags];
        }).filter(Boolean)
        : animationState.specs.filter(function(animation) {
          return animation.spec.classId === classId;
        }).filter(function(animation, index, rows) {
          return rows.findIndex(function(row) {
            return selectorFlags(row) === selectorFlags(animation);
          }) === index;
        });
      var failures = [], rows = [];
      CLASS_MOTION_SPECS.forEach(function(motion, motionIndex) {
        templates.forEach(function(template, routeIndex) {
          try {
            var candidate = animationState.resolveSelectorCandidate(
              template, motion.selector, 0);
            var key = 'class-motion:' + motion.kind + ':' + candidate.key;
            var animation = Object.assign({}, candidate, {
              key: key,
              corpusId: key,
              compatibilityKey: null,
              spec: Object.assign({}, candidate.spec, {
                key: key,
                id: key,
                compatibilityKey: null,
                actionId: motion.actionId,
                actionName: motion.actionName,
                rawMode: 0,
                modeLabel: motion.actionName,
                selector: motion.selector,
                classMotionKind: motion.kind,
                sequenceCatalogGroupKey: 'class-motion-' + motion.kind,
                displayOrder: motionIndex * 4 + routeIndex
              }),
              classMotionKind: motion.kind,
              selectedAction: null
            });
            rows.push(animation);
            animationState.classMotionAnimationsByKey[key] = animation;
          } catch (error) {
            failures.push({
              classId: classId,
              flags: selectorFlags(template),
              rawMode: 0,
              selector: motion.selector,
              actionId: motion.actionId,
              kind: motion.kind,
              message: error && error.message ? error.message : String(error)
            });
          }
        });
      });
      animationState.classMotionAnimationsByClass[classId] = rows;
      animationState.classMotionSequenceFailures[classId] = failures;
    }
    var classRows = animationState.classMotionAnimationsByClass[classId];
    if (!requestedKind) return classRows;
    return classRows.filter(function(animation) {
      return animation.spec.classMotionKind === requestedKind;
    });
  }

  function animationCopyCatalogOptions(idleTarget, replacing) {
    if (idleTarget) return { idleOnly: true };
    return replacing ? null : {
      includeIdle: true,
      includeClassMotion: true
    };
  }

  function animationSequenceCatalogRows(animationState, sequenceState, classId,
      side, options) {
    classId = Number(classId);
    side = Number(side);
    var requiredFlags = options && /^\d\/\d$/.test(String(options.flags || ''))
      ? String(options.flags) : null;
    if (options && options.classMotionKind) {
      return classMotionAnimationRows(
        animationState, classId, options.classMotionKind)
        .filter(function(animation) {
          return selectorFlagParts(animation)[1] === side &&
            (!requiredFlags || selectorFlags(animation) === requiredFlags);
        });
    }
    if (options && options.idleOnly) {
      var idleRows = idleAnimationRows(animationState, classId).filter(function(animation) {
        return selectorFlagParts(animation)[1] === side &&
          (!requiredFlags || selectorFlags(animation) === requiredFlags);
      });
      var privateIdle = Object.keys(sequenceState && sequenceState.separations || {})
        .map(function(id) { return sequenceState.separations[id]; })
        .filter(function(separation) {
          var animation = separation.syntheticAnimation;
          return separation.laneKey === 'idle' && animation &&
            animation.spec.classId === classId &&
            selectorFlagParts(animation)[1] === side &&
            (!requiredFlags || selectorFlags(animation) === requiredFlags);
        }).map(function(separation) { return separation.syntheticAnimation; });
      return idleRows.concat(privateIdle);
    }
    var includedIdle = options && options.includeIdle
      ? idleAnimationRows(animationState, classId).filter(function(animation) {
        return selectorFlagParts(animation)[1] === side &&
          (!requiredFlags || selectorFlags(animation) === requiredFlags);
      }) : [];
    var includedClassMotion = options && options.includeClassMotion
      ? classMotionAnimationRows(animationState, classId)
        .filter(function(animation) {
          return selectorFlagParts(animation)[1] === side &&
            (!requiredFlags || selectorFlags(animation) === requiredFlags);
        }) : [];
    var vanilla = animationState.specs.filter(function(animation) {
      return animation.spec.classId === classId &&
        selectorFlagParts(animation)[1] === side &&
        (!requiredFlags || selectorFlags(animation) === requiredFlags);
    }).sort(function(left, right) {
      return left.spec.displayOrder - right.spec.displayOrder;
    });
    var native = [];
    animationState.sequenceCatalogFailures = [];
    var api = OB64.combatAnimationOverrides;
    if (api && typeof api.selectorOptions === 'function' &&
        typeof animationState.resolveSelectorCandidate === 'function') {
      var templates = [], seenFlags = {};
      vanilla.forEach(function(animation) {
        var flags = selectorFlags(animation);
        if (seenFlags[flags]) return;
        seenFlags[flags] = true;
        templates.push(animation);
      });
      api.selectorOptions(classId).forEach(function(option) {
        templates.forEach(function(template) {
          [0, 1, 2].forEach(function(rawMode) {
            var represented = vanilla.some(function(animation) {
              return selectorFlags(animation) === selectorFlags(template) &&
                Number(animation.spec.rawMode) === rawMode &&
                Number(animation.spec.selector) === Number(option.id);
            });
            if (represented) return;
            try {
              var candidate = animationState.resolveSelectorCandidate(
                template, option.id, rawMode);
              var catalogCandidate = Object.assign({}, candidate, {
                spec: Object.assign({}, candidate.spec, {
                  sequenceCatalogGroupKey: 'native-selector-' + option.id,
                  nativeSelectorCandidate: true,
                  nativeSelectorStatus: option.status,
                  classId: template.spec.classId,
                  className: template.spec.className,
                  variantLabel: template.spec.variantLabel,
                  selectedBodyChild: template.spec.selectedBodyChild,
                  selectedChildOrdinal: template.spec.selectedBodyChild,
                  route: template.spec.route,
                  displayOrder: template.spec.displayOrder
                }),
                mappingStatus: template.mappingStatus
              });
              native.push(catalogCandidate);
              Object.keys(catalogCandidate.artByKey || {}).forEach(function(sourceKey) {
                if (!animationState.activeSourceAnimations) return;
                var uses = animationState.activeSourceAnimations[sourceKey] ||
                  (animationState.activeSourceAnimations[sourceKey] = []);
                if (!uses.some(function(animation) {
                  return animation.key === catalogCandidate.key;
                })) uses.push(catalogCandidate);
              });
            } catch (error) {
              animationState.sequenceCatalogFailures.push({
                classId: classId,
                flags: selectorFlags(template),
                rawMode: rawMode,
                selector: Number(option.id),
                message: error && error.message ? error.message : String(error)
              });
            }
          });
        });
      });
    }
    var modified = Object.keys(sequenceState && sequenceState.separations || {})
      .map(function(id) { return sequenceState.separations[id]; })
      .filter(function(separation) {
        var animation = separation.syntheticAnimation;
        return animation && animation.spec.classId === classId &&
          selectorFlagParts(animation)[1] === side &&
          (!requiredFlags || selectorFlags(animation) === requiredFlags);
      }).sort(function(left, right) {
        return left.actionId - right.actionId || left.bodyFlags - right.bodyFlags ||
          (left.laneKey === right.laneKey ? 0 :
            (left.laneKey === 'normal' ? -1 : 1));
      }).map(function(separation) { return separation.syntheticAnimation; });
    return includedIdle.concat(includedClassMotion, vanilla, native, modified);
  }

  function animationClassVariantChoices(rows) {
    var groups = [], byGroup = {};
    (rows || []).forEach(function(animation) {
      var actionId = Number(animation.spec.actionId);
      var groupKey = animation.spec.sequenceCatalogGroupKey ||
        'action-' + actionId;
      if (!byGroup[groupKey]) {
        byGroup[groupKey] = [];
        byGroup[groupKey].catalogGroupKey = groupKey;
        groups.push(byGroup[groupKey]);
      }
      byGroup[groupKey].push(animation);
    });
    var choices = [];
    groups.forEach(function(group) {
      var nativeGroup = group.every(function(animation) {
        return !!animation.spec.nativeSelectorCandidate;
      });
      var groupChoices = nativeGroup
        ? nativeProgramVariantChoices(group)
        : animationVariantChoices(group, { tagLinks: false });
      groupChoices.forEach(function(choice) {
        var representative = choice.representative;
        var nativeSelector = !!representative.spec.nativeSelectorCandidate;
        var idleSequence = isIdleAnimation(representative);
        var motionSequence = isClassMotionAnimation(representative);
        var actionId = nativeSelector || idleSequence || motionSequence ? null :
          Number(representative.spec.actionId);
        var actionName = idleSequence ? 'Idle / Rest' : (motionSequence
          ? representative.spec.actionName
          : (nativeSelector
            ? 'Native body program ' + M.hex(representative.spec.selector, 2) +
              (choice.nativeProgramCount > 1
                ? ' · Program variant ' + choice.nativeProgramOrdinal : '')
            : (OB64.ACTION_TEMPLATE_LABELS && OB64.ACTION_TEMPLATE_LABELS[actionId]
              ? OB64.ACTION_TEMPLATE_LABELS[actionId]
              : representative.spec.actionName)));
        choice.sequenceKind = choice.representative.spec.separatedCopy
          ? 'modified' : 'vanilla';
        choice.catalogGroupKey = group.catalogGroupKey;
        choice.sourceActionId = actionId;
        choice.sourceActionName = actionName;
        choice.sourceActionLabel = nativeSelector || idleSequence || motionSequence
          ? actionName : M.hex(actionId, 2) + ' · ' + actionName;
        if (!motionSequence) {
          choice.label = choice.sourceActionLabel + ' · ' + choice.label;
        }
        choice.optionTitle = (choice.sequenceKind === 'modified'
          ? 'Edited project sequence' : 'Original ROM sequence') +
          (idleSequence
            ? ' · Selector 0x00 combat idle/rest loop'
            : (motionSequence
              ? ' · Fixed class movement selector ' +
                M.hex(representative.spec.selector, 2)
              : (nativeSelector
                ? ' · Structurally valid native selector not used by a mapped action'
                : ' · Source action ' + choice.sourceActionLabel))) +
          ' · ' + choice.optionTitle;
        choices.push(choice);
      });
    });
    var mappedIdentities = new Set();
    choices.forEach(function(choice) {
      if (!choice.nativeProgramChoice) {
        mappedIdentities.add(animationSequenceIdentity(choice.representative));
      }
    });
    choices = choices.filter(function(choice) {
      return !choice.nativeProgramChoice ||
        !mappedIdentities.has(animationSequenceIdentity(choice.representative));
    });
    return tagLinkedVariantChoices(choices);
  }

  function animationActionFamily(actionId) {
    var info = OB64.combatAnimationOverrides &&
      OB64.combatAnimationOverrides.actionInfo
      ? OB64.combatAnimationOverrides.actionInfo(Number(actionId)) : null;
    return info && info.opcodeFamily ? String(info.opcodeFamily) : '';
  }

  function currentSequenceChoiceForTarget(choices, targetAnimation) {
    if (!targetAnimation || !targetAnimation.spec) return null;
    if (targetAnimation.effectiveMapping &&
        targetAnimation.effectiveMapping.assignmentRequired) return null;
    var targetActionId = Number(targetAnimation.spec.actionId);
    var targetFamily = animationActionFamily(targetActionId);
    var targetFlags = selectorFlags(targetAnimation);
    var targetLane = animationLaneKey(targetAnimation);
    var best = null, bestScore = -1;
    (choices || []).forEach(function(choice) {
      if (!sameAnimationSequence(choice.representative, targetAnimation)) return;
      var score = 0;
      if (Number.isInteger(choice.sourceActionId) &&
          Number(choice.sourceActionId) === targetActionId) score += 100;
      if (Number.isInteger(choice.sourceActionId) && targetFamily &&
          animationActionFamily(choice.sourceActionId) === targetFamily) {
        score += 40;
      }
      if (choice.laneKey === targetLane) score += 20;
      if (choice.flags === targetFlags) score += 10;
      if (score > bestScore) {
        best = choice;
        bestScore = score;
      }
    });
    return best;
  }

  function sequenceAssignmentSummary(currentChoice, targetAnimation,
      needsUserAssignment, previewChoice) {
    var targetLane = animationLaneLabel(targetAnimation);
    if (previewChoice && (!currentChoice || previewChoice.key !== currentChoice.key)) {
      return 'Previewing · ' + previewChoice.label + ' · ' + targetLane +
        (needsUserAssignment ? ' remains unassigned' : ' assignment unchanged');
    }
    if (isIdleAnimation(targetAnimation)) {
      return currentChoice ? 'Idle Loop · ' + currentChoice.label :
        'Choose an idle loop';
    }
    if (isClassMotionAnimation(targetAnimation)) {
      return currentChoice ? currentChoice.label :
        'Choose ' + animationLaneLabel(targetAnimation).toLowerCase();
    }
    if (needsUserAssignment) {
      return targetLane + ' target · Choose a body animation · current game ' +
        'fallback ' + M.hex(targetAnimation.spec.selector, 2);
    }
    return currentChoice
      ? 'Assigned to ' + targetLane + ' · ' + currentChoice.label
      : targetLane + ' target · Choose a body animation';
  }

  function animationArtRouteChoices(rows) {
    var flagsOrder = [], byFlags = {};
    (rows || []).forEach(function(animation) {
      var flags = selectorFlags(animation);
      if (!flags || byFlags[flags]) return;
      byFlags[flags] = animation;
      flagsOrder.push(flags);
    });
    return flagsOrder.map(function(flags) {
      var animation = byFlags[flags];
      return {
        flags: flags,
        representative: animation,
        optionClass: animationSideClass(animation),
        label: animationSideLabel(animation) + ' · ' +
          animationArtName(animation) + ' Art · ' +
          animationArtVariantLabel(animation),
        optionTitle: 'ROM flags ' + flags
      };
    });
  }

  function animationActionChoices(classRows, fallback) {
    classRows = classRows || [];
    fallback = fallback || classRows[0] || { spec: {} };
    var idleAction = classRows.find(isIdleAnimation) || {
      spec: {
        classId: fallback.spec.classId,
        className: fallback.spec.className,
        actionId: IDLE_ACTION_ID,
        actionName: 'Idle / Rest',
        idleSequence: true
      }
    };
    var actions = [idleAction];
    var actionSeen = {};
    actionSeen[String(IDLE_ACTION_ID)] = true;
    CLASS_MOTION_SPECS.forEach(function(motion) {
      var existing = classRows.find(function(animation) {
        return animation.spec.actionId === motion.actionId;
      });
      actions.push(existing || {
        spec: {
          classId: fallback.spec.classId,
          className: fallback.spec.className,
          actionId: motion.actionId,
          actionName: motion.actionName,
          rawMode: 0,
          selector: motion.selector,
          classMotionKind: motion.kind
        }
      });
      actionSeen[String(motion.actionId)] = true;
    });
    classRows.filter(function(animation) {
      return !isIdleAnimation(animation) && !isClassMotionAnimation(animation);
    }).forEach(function(animation) {
      var actionKey = String(animation.spec.actionId);
      if (actionSeen[actionKey]) return;
      actionSeen[actionKey] = true;
      actions.push(animation);
    });
    return actions;
  }

  function classSearchMatches(animation, query) {
    if (!animation || !animation.spec) return false;
    query = String(query || '').trim().toLowerCase();
    if (!query) return true;
    var classId = Number(animation.spec.classId);
    var searchable = M.hex(classId, 2) + ' ' + classId + ' ' +
      String(animation.spec.className || '');
    return searchable.toLowerCase().indexOf(query) >= 0;
  }

  function animationClassChoices(animations) {
    var byClass = {};
    (animations || []).forEach(function(animation) {
      if (!byClass[animation.spec.classId]) {
        byClass[animation.spec.classId] = animation;
      }
    });
    var classIds = {};
    Object.keys(OB64.CLASS_NAMES || {}).forEach(function(classId) {
      classId = Number(classId);
      if (classId) classIds[classId] = true;
    });
    Object.keys(byClass).forEach(function(classId) {
      classIds[Number(classId)] = true;
    });
    return Object.keys(classIds).map(Number).sort(function(left, right) {
      return left - right;
    }).map(function(classId) {
      if (byClass[classId]) return byClass[classId];
      return {
        missingAnimation: true,
        spec: {
          classId: classId,
          className: OB64.className ? OB64.className(classId) :
            OB64.CLASS_NAMES[classId]
        }
      };
    });
  }

  function usesEnemyPreviewSide(classId) {
    var categories = OB64.CLASS_CATEGORIES || {};
    var bosses = categories.Boss || [];
    classId = Number(classId);
    return bosses.indexOf(classId) >= 0 || classId >= 0x87;
  }

  function variantQuality(animation) {
    var status = animation && animation.mappingStatus;
    if (!status) return 3;
    if (status.state === 'visible-failure') return 2;
    if (status.state === 'soldier-alias') return 1;
    return 0;
  }

  function preferredAnimation(rows, options) {
    if (!rows || !rows.length) return null;
    options = options || {};
    var ordered = rows.slice().sort(function(left, right) {
      return left.spec.displayOrder - right.spec.displayOrder;
    });
    var rawMode = Number.isInteger(options.rawMode) ? options.rawMode : 0;
    var modeRows = ordered.filter(function(animation) {
      return animation.spec.rawMode === rawMode;
    });
    if (!modeRows.length) modeRows = ordered;
    var classId = modeRows[0].spec.classId;
    var preferredSide = options.side === 0 || options.side === 1
      ? options.side : (usesEnemyPreviewSide(classId) ? 1 : 0);
    modeRows.sort(function(left, right) {
      var leftFlags = selectorFlagParts(left);
      var rightFlags = selectorFlagParts(right);
      var difference = variantQuality(left) - variantQuality(right);
      if (difference) return difference;
      difference = (leftFlags[1] === preferredSide ? 0 : 1) -
        (rightFlags[1] === preferredSide ? 0 : 1);
      if (difference) return difference;
      difference = (leftFlags[0] === 0 ? 0 : 1) -
        (rightFlags[0] === 0 ? 0 : 1);
      return difference || left.spec.displayOrder - right.spec.displayOrder;
    });
    return modeRows[0];
  }

  function defaultPreviewExplanation(rows, preferred) {
    if (!preferred) return '';
    var preferredSide = usesEnemyPreviewSide(preferred.spec.classId) ? 1 : 0;
    var flags = selectorFlagParts(preferred);
    var quality = variantQuality(preferred);
    var sideLabel = preferredSide ? 'enemy-facing' : 'player-facing';
    var result = 'Default preview: raw mode ' + preferred.spec.rawMode +
      ', flags ' + selectorFlags(preferred) + '. ';
    if (quality) {
      result += 'No usable class-specific row exists for this action, so the ' +
        (quality === 1 ? 'Soldier alias' : 'visible mapping issue') +
        ' remains the default. ';
    } else if (flags[1] !== preferredSide) {
      result += 'The ' + sideLabel +
        ' rows contain no usable class art, so this uses the other side. ';
    } else if (flags[0] === 1) {
      result += 'Bank 0 has no usable class art on the ' + sideLabel +
        ' side, so this uses bank 1. ';
    } else {
      result += 'Bank 0 contains usable class art on the ' + sideLabel +
        ' side. ';
    }
    return result + 'Every raw variant remains selectable.';
  }

  function classDefinition(rom, classId) {
    return rom && rom.classDefs ? rom.classDefs[Number(classId) + 1] || null : null;
  }

  function canonicalAnimationCatalog(state) {
    var specs = state.animations.specs.slice();
    Object.keys(state.animations.artRouteTemplatesByClass || {})
      .map(Number).sort(function(left, right) { return left - right; })
      .forEach(function(classId) {
        if (specs.some(function(animation) {
          return animation.spec.classId === classId;
        })) return;
        idleAnimationRows(state.animations, classId).forEach(function(animation) {
          specs.push(animation);
        });
      });
    var byKey = {};
    specs.forEach(function(animation) { byKey[animation.key] = animation; });
    return {
      effective: false,
      specs: specs,
      byKey: byKey,
      failures: [],
      diagnostic: '',
      sourceAnimations: null
    };
  }

  function orderedLiveMappings(api, overrideState, definition, classId) {
    var rows = api.modalRows(overrideState, definition, classId);
    var byAction = {}, output = [];
    rows.forEach(function(row) { byAction[row.actionId] = row; });
    [definition && definition.b43Raw, definition && definition.b45Raw,
      definition && definition.b47Raw].forEach(function(actionId) {
      actionId = Number(actionId || 0);
      if (!actionId || !byAction[actionId] || output.indexOf(byAction[actionId]) >= 0) {
        return;
      }
      output.push(byAction[actionId]);
    });
    rows.forEach(function(row) {
      if (output.indexOf(row) < 0) output.push(row);
    });
    return output;
  }

  function effectiveRouteKey(classId, actionId, flags, rawMode, selector) {
    return ['effective', classId, actionId, String(flags).replace('/', '-'),
      rawMode, selector].join(':');
  }

  function effectiveRouteAnimation(candidate, template, mapping, action,
      rawMode, selector, displayOrder) {
    var flags = selectorFlags(template);
    var key = effectiveRouteKey(template.spec.classId, mapping.actionId,
      flags, rawMode, selector);
    var spec = Object.assign({}, candidate.spec, {
      key: key,
      id: key,
      compatibilityKey: null,
      classId: template.spec.classId,
      className: template.spec.className,
      actionId: mapping.actionId,
      actionName: action ? action.name : 'Action ' + M.hex(mapping.actionId, 2),
      consumerSummary: template.spec.className + ' effective Class Combat route',
      variantLabel: template.spec.variantLabel,
      selectedBodyChild: template.spec.selectedBodyChild,
      selectedChildOrdinal: template.spec.selectedBodyChild,
      rawMode: rawMode,
      modeLabel: 'Raw mode ' + rawMode,
      selector: selector,
      displayOrder: displayOrder,
      frozenParity: null,
      effectiveRoute: true
    });
    return Object.assign({}, candidate, {
      key: key,
      corpusId: key,
      compatibilityKey: null,
      spec: spec,
      mappingStatus: template.mappingStatus,
      effectiveMapping: {
        source: mapping.mappingSource ||
          (mapping.overridden ? 'override' : 'vanilla'),
        overridden: !!mapping.overridden,
        assignmentRequired: !!mapping.assignmentRequired,
        classId: template.spec.classId,
        actionId: mapping.actionId,
        selector: selector,
        laneKey: rawMode === 2 ? 'blocked' : 'normal',
        ranks: mapping.ranks.slice(),
        assigned: mapping.ranks.length > 0,
        candidateKey: candidate.key
      }
    });
  }

  function unassignedRouteAnimation(template, mapping, action, rawMode,
      selector, displayOrder, message) {
    var flags = selectorFlags(template);
    var key = effectiveRouteKey(template.spec.classId, mapping.actionId,
      flags, rawMode, 'unassigned-' + selector);
    var spec = Object.assign({}, template.spec, {
      key: key,
      id: key,
      compatibilityKey: null,
      classId: template.spec.classId,
      className: template.spec.className,
      actionId: mapping.actionId,
      actionName: action ? action.name : 'Action ' + M.hex(mapping.actionId, 2),
      consumerSummary: template.spec.className +
        ' unassigned Class Combat art route',
      rawMode: rawMode,
      modeLabel: 'Raw mode ' + rawMode,
      displayOrder: displayOrder,
      frozenParity: null,
      effectiveRoute: true,
      assignmentPlaceholder: true
    });
    return Object.assign({}, template, {
      key: key,
      corpusId: key,
      compatibilityKey: null,
      spec: spec,
      mappingStatus: template.mappingStatus,
      effectiveMapping: {
        source: 'unassigned',
        overridden: false,
        assignmentRequired: true,
        classId: template.spec.classId,
        actionId: mapping.actionId,
        selector: Number(selector),
        laneKey: rawMode === 2 ? 'blocked' : 'normal',
        ranks: mapping.ranks.slice(),
        assigned: mapping.ranks.length > 0,
        candidateKey: template.key,
        reason: message || 'This art route has no body program assignment.'
      }
    });
  }

  function separatedRouteAnimation(animation, mapping, action, rawMode,
      displayOrder) {
    var key = animation.key + ':route-' + mapping.actionId + ':mode-' + rawMode;
    var spec = Object.assign({}, animation.spec, {
      key: key,
      id: key,
      classId: mapping.classId,
      actionId: mapping.actionId,
      actionName: action ? action.name : 'Action ' + M.hex(mapping.actionId, 2),
      rawMode: rawMode,
      modeLabel: 'Raw mode ' + rawMode,
      displayOrder: displayOrder
    });
    return Object.assign({}, animation, {
      key: key,
      corpusId: key,
      spec: spec,
      effectiveMapping: Object.assign({}, animation.effectiveMapping, {
        source: 'separated',
        overridden: true,
        classId: mapping.classId,
        actionId: mapping.actionId,
        ranks: mapping.ranks.slice(),
        assigned: mapping.ranks.length > 0,
        laneKey: rawMode === 2 ? 'blocked' : 'normal',
        selector: animation.spec.selector,
        candidateKey: animation.donorAnimation && animation.donorAnimation.key
      }),
      selectedAction: action || null
    });
  }

  function routePairForAnimation(rom, animation) {
    var api = OB64.combatAnimationOverrides;
    var sequences = OB64.animationSequences;
    var overrideState = rom && rom.combatAnimationOverrides;
    if (!api || !sequences || !overrideState || !animation) return null;
    var classId = Number(animation.spec.classId);
    var actionId = Number(animation.spec.actionId);
    var bodyFlags = sequences.bodyFlagsFor(animation);
    var exact = api.exactEntry(overrideState, classId, actionId, bodyFlags);
    if (exact) {
      return {
        normalSelector: Number(exact.normalSelector),
        blockedSelector: Number(exact.blockedSelector)
      };
    }
    var definition = classDefinition(rom, classId);
    var mapping = orderedLiveMappings(api, overrideState, definition, classId)
      .find(function(row) { return row.actionId === actionId; });
    if (mapping && Number.isInteger(mapping.normalSelector) &&
        Number.isInteger(mapping.blockedSelector)) {
      return {
        normalSelector: Number(mapping.normalSelector),
        blockedSelector: Number(mapping.blockedSelector)
      };
    }
    var flags = selectorFlags(animation);
    var rows = rom.art.animations.specs.filter(function(row) {
      return row.spec.classId === classId && row.spec.actionId === actionId &&
        selectorFlags(row) === flags;
    });
    var normal = rows.find(function(row) { return row.spec.rawMode !== 2; });
    var blocked = rows.find(function(row) { return row.spec.rawMode === 2; });
    if (!normal || !blocked) return null;
    return {
      normalSelector: Number(normal.spec.selector),
      blockedSelector: Number(blocked.spec.selector)
    };
  }

  function indexCatalogSources(catalog) {
    var bySource = {};
    addAnimationsToSourceIndex(bySource, catalog.specs);
    catalog.sourceAnimations = bySource;
    return catalog;
  }

  function addAnimationsToSourceIndex(bySource, animations) {
    (animations || []).forEach(function(animation) {
      Object.keys(animation.artByKey).forEach(function(sourceKey) {
        if (!bySource[sourceKey]) bySource[sourceKey] = [];
        if (!bySource[sourceKey].some(function(existing) {
          return existing.key === animation.key;
        })) bySource[sourceKey].push(animation);
      });
    });
    return bySource;
  }

  function editScopeSourceIndex(animationState, catalog, sequenceState) {
    var cached = animationState.editScopeSourceIndexCache;
    if (cached && cached.catalog === catalog) {
      animationState.activeAssignedSourceAnimations =
        cached.assignedSourceAnimations;
      return cached.sourceAnimations;
    }
    var bySource = {}, assignedBySource = {};
    // The effective catalog contains current Class Combat assignments. The
    // accepted corpus also contains vanilla routes that remain selectable and
    // can share the same editable sprite binding, including boss copies.
    addAnimationsToSourceIndex(bySource, animationState.specs);
    addAnimationsToSourceIndex(bySource, catalog && catalog.specs);
    addAnimationsToSourceIndex(assignedBySource,
      (catalog && catalog.specs || []).filter(function(animation) {
        return !animation.effectiveMapping || animation.effectiveMapping.assigned;
      }));
    Object.keys(sequenceState && sequenceState.separations || {}).forEach(function(id) {
      var separation = sequenceState.separations[id];
      if (separation && separation.syntheticAnimation) {
        addAnimationsToSourceIndex(bySource, [separation.syntheticAnimation]);
      }
    });
    animationState.editScopeSourceIndexCache = {
      catalog: catalog,
      sourceAnimations: bySource,
      assignedSourceAnimations: assignedBySource
    };
    animationState.activeAssignedSourceAnimations = assignedBySource;
    return bySource;
  }

  function effectiveCatalogSignature(rom) {
    var overrideState = rom && rom.combatAnimationOverrides;
    var parts = [
      'catalog-v1',
      overrideState && overrideState.readOnly ? 1 : 0,
      overrideState && overrideState.disabledReason || '',
      rom && rom.animationSequences ? Number(rom.animationSequences.revision) || 0 : 0
    ];
    (overrideState && overrideState.desired || []).forEach(function(row) {
      parts.push('o', row.classId, row.actionId,
        Number.isInteger(row.bodyFlags) ? row.bodyFlags : '-',
        row.normalSelector, row.blockedSelector);
    });
    Object.keys(rom && rom.classDefs || {}).sort(function(left, right) {
      return Number(left) - Number(right);
    }).forEach(function(key) {
      var definition = rom.classDefs[key];
      if (!definition) return;
      parts.push('c', key, definition.b43Raw, definition.b45Raw,
        definition.b47Raw, definition.isTerm ? 1 : 0,
        definition.isSentinel ? 1 : 0);
    });
    return parts.join(':');
  }

  function effectiveAnimationCatalog(state, rom) {
    var api = OB64.combatAnimationOverrides;
    var overrideState = rom && rom.combatAnimationOverrides;
    var animationState = state.animations;
    var signature = effectiveCatalogSignature(rom);
    var cached = animationState.effectiveCatalogCache;
    if (cached && cached.rom === rom && cached.signature === signature) {
      return cached.catalog;
    }
    function remember(catalog) {
      animationState.effectiveCatalogCache = {
        rom: rom, signature: signature, catalog: catalog
      };
      return catalog;
    }
    var fallback = canonicalAnimationCatalog(state);
    if (!api || !overrideState || !rom.classDefs ||
        typeof animationState.resolveSelectorCandidate !== 'function') {
      return remember(indexCatalogSources(fallback));
    }
    if (overrideState.readOnly) {
      fallback.diagnostic = 'Class Combat mapping is not synchronized because ' +
        (overrideState.disabledReason || overrideState.diagnostic ||
          'the selector override lane is advanced or foreign.');
      return remember(indexCatalogSources(fallback));
    }
    var byClass = {};
    animationState.specs.forEach(function(animation) {
      var classId = animation.spec.classId;
      if (!byClass[classId]) byClass[classId] = [];
      byClass[classId].push(animation);
    });
    Object.keys(animationState.artRouteTemplatesByClass || {})
      .forEach(function(classId) {
        if (!byClass[classId]) byClass[classId] = [];
      });
    var specs = [], failures = [], displayOrder = 0;
    function appendCanonicalRows(rows) {
      rows.slice().sort(function(left, right) {
        return left.spec.displayOrder - right.spec.displayOrder;
      }).forEach(function(animation) {
        specs.push(Object.assign({}, animation, {
          spec: Object.assign({}, animation.spec, { displayOrder: displayOrder++ })
        }));
      });
    }
    Object.keys(byClass).map(Number).sort(function(left, right) {
      return left - right;
    }).forEach(function(classId) {
      var classRows = byClass[classId];
      var routeTable = animationState.artRouteTemplatesByClass &&
        animationState.artRouteTemplatesByClass[classId];
      var routeTemplates = routeTable
        ? ['0/0', '0/1', '1/0', '1/1'].map(function(flags) {
          return routeTable[flags];
        }).filter(Boolean)
        : classRows;
      var classInfo = api.classInfo(classId);
      var definition = classDefinition(rom, classId);
      if (!classInfo || !definition || definition.isTerm || definition.isSentinel) {
        appendCanonicalRows(classRows);
        if (!classRows.length) appendCanonicalRows(
          idleAnimationRows(animationState, classId));
        return;
      }
      var mappings = orderedLiveMappings(api, overrideState, definition, classId);
      if (!mappings.length) {
        appendCanonicalRows(classRows);
        appendCanonicalRows(idleAnimationRows(animationState, classId));
        return;
      }
      var templates = [], seenFlags = {};
      routeTemplates.slice().sort(function(left, right) {
        return left.spec.displayOrder - right.spec.displayOrder;
      }).forEach(function(animation) {
        var flags = selectorFlags(animation);
        if (seenFlags[flags]) return;
        seenFlags[flags] = true;
        templates.push(animation);
      });
      mappings.forEach(function(mapping) {
        mapping.ranks = api.liveRanks(definition, mapping.actionId);
        var action = api.actionInfo(mapping.actionId);
        templates.forEach(function(template) {
          var bodyFlags = OB64.animationSequences
            ? OB64.animationSequences.bodyFlagsFor(template) : null;
          var exact = Number.isInteger(bodyFlags)
            ? api.exactEntry(overrideState, classId, mapping.actionId, bodyFlags)
            : null;
          var effectiveMapping = exact ? Object.assign({}, mapping, {
            normalSelector: exact.normalSelector,
            blockedSelector: exact.blockedSelector,
            overridden: true,
            mappingSource: 'route',
            assignmentRequired: false
          }) : mapping;
          [0, 1, 2].forEach(function(rawMode) {
            var laneKey = rawMode === 2 ? 'blocked' : 'normal';
            var selector = rawMode === 2
              ? effectiveMapping.blockedSelector : effectiveMapping.normalSelector;
            var separated = Number.isInteger(bodyFlags) && OB64.animationSequences
              ? OB64.animationSequences.routeAnimation(
                rom, classId, mapping.actionId, bodyFlags, laneKey)
              : null;
            if (!separated && Number.isInteger(bodyFlags) &&
                Number.isInteger(selector) && OB64.animationSequences &&
                OB64.animationSequences.selectorAnimation) {
              separated = OB64.animationSequences.selectorAnimation(
                rom, classId, bodyFlags, selector);
            }
            if (separated) {
              specs.push(separatedRouteAnimation(separated, effectiveMapping,
                action, rawMode, displayOrder++));
              return;
            }
            var routeMapping = effectiveMapping;
            if (!Number.isInteger(selector)) {
              var canonical = classRows.find(function(animation) {
                return animation.spec.actionId === mapping.actionId &&
                  animation.spec.rawMode === rawMode &&
                  selectorFlags(animation) === selectorFlags(template);
              });
              if (!canonical) {
                failures.push({
                  classId: classId, actionId: mapping.actionId,
                  flags: selectorFlags(template), rawMode: rawMode,
                  selector: selector,
                  message: 'No selector or accepted corpus route is resolved.'
                });
                specs.push(unassignedRouteAnimation(template, mapping, action,
                  rawMode, Number.isInteger(selector) ? selector : 0x28,
                  displayOrder++,
                  'No selector or accepted corpus route is resolved.'));
                return;
              }
              selector = canonical.spec.selector;
              routeMapping = Object.assign({}, mapping, {
                mappingSource: 'corpus'
              });
            }
            try {
              var candidate = animationState.resolveSelectorCandidate(
                template, selector, rawMode);
              specs.push(effectiveRouteAnimation(candidate, template, routeMapping,
                action, rawMode, selector, displayOrder++));
            } catch (error) {
              var message = error && error.message ? error.message : String(error);
              failures.push({
                classId: classId, actionId: mapping.actionId,
                flags: selectorFlags(template), rawMode: rawMode,
                selector: selector,
                message: message
              });
              specs.push(unassignedRouteAnimation(template, mapping, action,
                rawMode, selector, displayOrder++, message));
            }
          });
        });
      });
    });
    if (!specs.length) return remember(indexCatalogSources(fallback));
    var byKey = {};
    specs.forEach(function(animation) { byKey[animation.key] = animation; });
    Object.keys(rom.animationSequences && rom.animationSequences.separations || {})
      .forEach(function(id) {
        var modified = rom.animationSequences.separations[id].syntheticAnimation;
        if (modified) byKey[modified.key] = modified;
      });
    return remember(indexCatalogSources({
      effective: true,
      specs: specs,
      byKey: byKey,
      failures: failures,
      diagnostic: '',
      sourceAnimations: null
    }));
  }

  function requestAnimationRoute(ui, request) {
    if (!ui || !request) return;
    var route = { classId: Number(request.classId) };
    if (Number.isInteger(Number(request.actionId))) {
      route.actionId = Number(request.actionId);
    }
    if (request.laneKey === 'normal' || request.laneKey === 'blocked') {
      route.laneKey = request.laneKey;
    }
    if (/^\d\/\d$/.test(String(request.flags || ''))) {
      route.flags = String(request.flags);
    }
    ui.animationRouteRequest = route;
  }

  function spriteEditScope(animationState, sourceKey, childOrdinal) {
    var source = animationState && animationState.artByKey[sourceKey];
    if (!source) return null;
    var routes = [];
    var classIds = {};
    var activeAnimations = animationState.activeSourceAnimations &&
      animationState.activeSourceAnimations[sourceKey];
    var animations = activeAnimations || (source.animationKeys || []).map(function(key) {
      return animationState.byKey[key];
    }).filter(Boolean);
    var hasAssignedIndex = !!animationState.activeAssignedSourceAnimations;
    var assignedAnimations = animationState.activeAssignedSourceAnimations &&
      animationState.activeAssignedSourceAnimations[sourceKey] || [];
    function isAssigned(animation) {
      return assignedAnimations.some(function(candidate) {
        return candidate.spec.classId === animation.spec.classId &&
          candidate.spec.actionId === animation.spec.actionId &&
          selectorFlags(candidate) === selectorFlags(animation) &&
          animationLaneKey(candidate) === animationLaneKey(animation) &&
          sameAnimationSequence(candidate, animation);
      });
    }
    animations.forEach(function(animation) {
      if (!animation) return;
      var animationKey = animation.key;
      var frameNumbers = [];
      var frameIndices = source.usageFramesByAnimation &&
        source.usageFramesByAnimation[animationKey];
      if (!frameIndices) {
        frameIndices = animation.frames.map(function(frame, frameIndex) {
          return frameIndex;
        });
      }
      frameIndices.forEach(function(frameIndex) {
        var frame = animation.frames[frameIndex];
        if (!frame) return;
        var usesChild = frame.layers.some(function(layer) {
          if (layer.sourceKey !== sourceKey) return false;
          return source.weaponSelectable ||
            layer.selectedChildOrdinal === childOrdinal;
        });
        if (usesChild) frameNumbers.push(frame.sequenceIndex + 1);
      });
      if (!frameNumbers.length) return;
      classIds[animation.spec.classId] = true;
      var matching = routes.find(function(route) {
        return route.classId === animation.spec.classId &&
          route.actionId === animation.spec.actionId &&
          route.flags === selectorFlags(animation) &&
          route.laneKey === animationLaneKey(animation) &&
          sameAnimationSequence(route.representative, animation);
      });
      if (matching) {
        matching.animationKeys.push(animation.key);
        matching.assigned = matching.assigned || isAssigned(animation);
        matching.rawModes.push(animation.spec.rawMode);
        frameNumbers.forEach(function(frameNumber) {
          if (matching.frameNumbers.indexOf(frameNumber) < 0) {
            matching.frameNumbers.push(frameNumber);
          }
        });
        matching.rawModes.sort(function(left, right) { return left - right; });
        matching.frameNumbers.sort(function(left, right) { return left - right; });
        return;
      }
      routes.push({
        animationKeys: [animation.key],
        representative: animation,
        displayOrder: animation.spec.displayOrder,
        classId: animation.spec.classId,
        className: animation.spec.className,
        actionId: animation.spec.actionId,
        actionName: animation.spec.actionName,
        assigned: hasAssignedIndex ? isAssigned(animation) :
          (!animation.effectiveMapping || animation.effectiveMapping.assigned),
        ranks: animation.effectiveMapping
          ? animation.effectiveMapping.ranks.slice() : [],
        laneKey: animationLaneKey(animation),
        laneLabel: animationLaneLabel(animation),
        sideLabel: animationSideLabel(animation),
        artName: animationArtName(animation),
        artVariantLabel: animationArtVariantLabel(animation),
        rawModes: [animation.spec.rawMode],
        flags: selectorFlags(animation),
        frameNumbers: frameNumbers
      });
    });
    routes.sort(function(left, right) {
      return left.displayOrder - right.displayOrder;
    });
    return {
      sourceKey: sourceKey,
      childOrdinal: childOrdinal,
      frameUses: routes.reduce(function(total, route) {
        return total + route.frameNumbers.length;
      }, 0),
      routeCount: routes.length,
      classCount: Object.keys(classIds).length,
      routes: routes
    };
  }

  function scopeCount(value, singular, plural) {
    return value + ' ' + (value === 1 ? singular : plural);
  }

  function identicalSpriteSlots(source, childOrdinal) {
    if (!source || !source.editable || !Number.isInteger(childOrdinal) ||
        source.editableChildOrdinals.indexOf(childOrdinal) < 0) return [];
    var selected = M.originalChild(source, childOrdinal);
    return source.editableChildOrdinals.filter(function(otherOrdinal) {
      if (otherOrdinal === childOrdinal) return false;
      var other = M.originalChild(source, otherOrdinal);
      return M.equalBytes(selected.indices, other.indices) &&
        M.equalBytes(selected.intensity, other.intensity);
    });
  }

  function spriteSlotLabel(source, childOrdinal) {
    return source.weaponSelectable
      ? 'weapon sprite ' + (childOrdinal + 1)
      : 'sprite slot ' + childOrdinal;
  }

  function independentSlotNote(source, childOrdinal) {
    var identical = identicalSpriteSlots(source, childOrdinal);
    if (!identical.length) return '';
    return ' ' + (identical.length === 1 ? 'Slot ' : 'Slots ') +
      identical.join(', ') +
      (identical.length === 1 ? ' has' : ' have') +
      ' the same vanilla pixels, but ' +
      (identical.length === 1 ? 'it is' : 'they are') +
      ' a separate editable slot.';
  }

  function editScopePanel(animationState, animation, source, childOrdinal) {
    var scope = spriteEditScope(animationState, source.key, childOrdinal);
    if (!scope || !scope.frameUses) return null;
    var slotLabel = spriteSlotLabel(source, childOrdinal);
    var separateSlotNote = independentSlotNote(source, childOrdinal);
    var shared = scope.frameUses > 1 || scope.routeCount > 1 ||
      scope.classCount > 1;
    if (!shared) {
      var unique = element('div', 'animation-edit-scope animation-edit-scope-unique');
      unique.appendChild(element('strong', '', 'Edit scope: '));
      unique.appendChild(element('span', '', 'selected frame only · ' + slotLabel));
      if (separateSlotNote) {
        unique.appendChild(element('p', 'animation-edit-scope-note',
          separateSlotNote.trim()));
      }
      return unique;
    }
    var details = element('details', 'animation-edit-scope animation-edit-scope-shared');
    details.setAttribute('title',
      'Counts include only routes affected by this exact sprite slot. A bundle can contain other independent slots.');
    var summary = element('summary');
    summary.appendChild(badge(source.weaponSelectable
      ? 'Shared weapon sprite' : 'Shared sprite', 'shared'));
    summary.appendChild(element('span', '',
      slotLabel + ' · ' +
      scopeCount(scope.frameUses, 'frame use', 'frame uses') + ' · ' +
      scopeCount(scope.routeCount, 'variant', 'variants') + ' · ' +
      scopeCount(scope.classCount, 'class', 'classes')));
    details.appendChild(summary);
    details.appendChild(element('p', 'animation-edit-scope-note',
      source.weaponSelectable
        ? 'Every listed route uses this binding. The edit appears when ' +
          slotLabel + ' is selected by the preview or equipped item.'
        : 'Every listed route uses this exact sprite slot. Other slots in the ' +
          'same bundle remain unchanged.' + separateSlotNote));
    var list = element('div', 'animation-edit-scope-list');
    scope.routes.forEach(function(route) {
      var row = element('div', 'animation-edit-scope-route' +
        (route.animationKeys.indexOf(animation.key) >= 0 ? ' current' : ''));
      row.title = 'Raw mode' + (route.rawModes.length === 1 ? ' ' : 's ') +
        route.rawModes.join(', ') + ' · ROM flags ' + (route.flags || '?');
      row.appendChild(element('strong', '', M.hex(route.classId, 2) + ' · ' +
        route.className));
      row.appendChild(element('span', '', route.actionName + ' · ' +
        route.sideLabel + ' · ' + route.artName + ' Art · ' +
        route.artVariantLabel + ' · ' + route.laneLabel + ' · ' +
        'frame' + (route.frameNumbers.length === 1 ? ' ' : 's ') +
        route.frameNumbers.join(', ') +
        (route.assigned ? '' : ' Â· not assigned')));
      list.appendChild(row);
    });
    details.appendChild(list);
    return details;
  }

  function rememberAnimationSelection(ui, animation) {
    ui.animationClassId = animation.spec.classId;
    ui.animationActionId = animation.spec.actionId;
    ui.animationFlags = selectorFlags(animation);
    ui.animationLaneKey = animationLaneKey(animation);
    ui.animationRawMode = animation.spec.rawMode;
  }

  function rememberAnimationTarget(ui, animation) {
    ui.animationTargetClassId = animation.spec.classId;
    ui.animationTargetActionId = animation.spec.actionId;
    ui.animationTargetFlags = selectorFlags(animation);
    ui.animationTargetLaneKey = animationLaneKey(animation);
  }

  function assignmentTargetAnimation(ui, catalog, fallback) {
    var classId = Number.isInteger(ui.animationTargetClassId)
      ? ui.animationTargetClassId : fallback.spec.classId;
    var actionId = Number.isInteger(ui.animationTargetActionId)
      ? ui.animationTargetActionId : fallback.spec.actionId;
    var flags = ui.animationTargetFlags || selectorFlags(fallback);
    var laneKey = ui.animationTargetLaneKey || animationLaneKey(fallback);
    var rows = catalog.specs.filter(function(row) {
      return row.spec.classId === classId && row.spec.actionId === actionId &&
        selectorFlags(row) === flags && animationLaneKey(row) === laneKey;
    });
    return preferredAnimation(rows, {
      rawMode: laneKey === 'blocked' ? 2 : 0
    }) || fallback;
  }

  function selectedAnimation(state, ui, catalog) {
    var previousKey = ui.animationKey;
    var request = ui.animationRouteRequest;
    var animation = null;
    if (request) {
      var requestRows = catalog.specs.filter(function(row) {
        return row.spec.classId === request.classId;
      });
      if (Number.isInteger(request.actionId)) {
        var requestedActionRows = requestRows.filter(function(row) {
          return row.spec.actionId === request.actionId;
        });
        if (requestedActionRows.length) requestRows = requestedActionRows;
      }
      if (request.flags) {
        var requestedFlagRows = requestRows.filter(function(row) {
          return selectorFlags(row) === request.flags;
        });
        if (requestedFlagRows.length) requestRows = requestedFlagRows;
      }
      var requestLane = request.laneKey || 'normal';
      var requestedLaneRows = requestRows.filter(function(row) {
        return animationLaneKey(row) === requestLane;
      });
      if (requestedLaneRows.length) requestRows = requestedLaneRows;
      animation = preferredAnimation(requestRows, {
        rawMode: requestLane === 'blocked' ? 2 : 0
      });
      delete ui.animationRouteRequest;
    }
    if (!animation) animation = catalog.byKey[ui.animationKey];
    if (!animation && state.animations.idleAnimationsByKey) {
      animation = state.animations.idleAnimationsByKey[ui.animationKey];
    }
    if (!animation && state.animations.classMotionAnimationsByKey) {
      animation = state.animations.classMotionAnimationsByKey[ui.animationKey];
    }
    if (!animation && state.animations.selectorCandidates) {
      animation = Object.keys(state.animations.selectorCandidates)
        .map(function(key) { return state.animations.selectorCandidates[key]; })
        .find(function(candidate) {
          return candidate && candidate.key === ui.animationKey;
        });
    }
    if (!animation) {
      var prior = state.animations.byKey[ui.animationKey];
      var classId = Number.isInteger(ui.animationClassId)
        ? ui.animationClassId : (prior && prior.spec.classId);
      var actionId = Number.isInteger(ui.animationActionId)
        ? ui.animationActionId : (prior && prior.spec.actionId);
      var flags = ui.animationFlags || (prior && selectorFlags(prior));
      var laneKey = ui.animationLaneKey || (prior && animationLaneKey(prior));
      var rows = catalog.specs.filter(function(row) {
        return !Number.isInteger(classId) || row.spec.classId === classId;
      });
      var actionRows = rows.filter(function(row) {
        return !Number.isInteger(actionId) || row.spec.actionId === actionId;
      });
      if (actionRows.length) rows = actionRows;
      var variantRows = rows.filter(function(row) {
        return (!flags || selectorFlags(row) === flags) &&
          (!laneKey || animationLaneKey(row) === laneKey);
      });
      if (variantRows.length) rows = variantRows;
      animation = preferredAnimation(rows, {
        rawMode: laneKey === 'blocked' ? 2 :
          (Number.isInteger(ui.animationRawMode) ? ui.animationRawMode : 0)
      }) || catalog.specs[0];
    }
    ui.animationKey = animation.key;
    rememberAnimationSelection(ui, animation);
    if (request || !Number.isInteger(ui.animationTargetClassId)) {
      rememberAnimationTarget(ui, animation);
    }
    weaponChildForAnimation(ui, animation);
    if (animation.key !== previousKey) {
      ui.animationFrame = 0;
      ui.animationLayer = animation.frames[0].layers[0].ordinal;
      selectLayer(state, animation, animation.frames[0],
        animation.frames[0].layers[0], ui);
    }
    return animation;
  }

  function animationWeaponChildMaximum(animation) {
    var maximum = Math.max(0, (animation.spec.weaponChildCount || 1) - 1);
    Object.keys(animation.artByKey || {}).forEach(function(key) {
      var source = animation.artByKey[key];
      if (source.weaponSelectable) {
        maximum = Math.max(maximum, source.sprite.childCount - 1);
      }
    });
    return maximum;
  }

  function weaponChildForAnimation(ui, animation) {
    if (!ui.animationWeaponChildren) ui.animationWeaponChildren = {};
    var ordinal = ui.animationWeaponChildren[animation.key];
    if (!Number.isInteger(ordinal)) {
      ordinal = Number.isInteger(ui.animationWeaponChild) ? ui.animationWeaponChild : 0;
    }
    var maximum = animationWeaponChildMaximum(animation);
    ordinal = Math.max(0, Math.min(maximum, ordinal));
    ui.animationWeaponChildren[animation.key] = ordinal;
    ui.animationWeaponChild = ordinal;
    return ordinal;
  }

  function setWeaponChild(ui, animation, ordinal) {
    if (!ui.animationWeaponChildren) ui.animationWeaponChildren = {};
    var maximum = animationWeaponChildMaximum(animation);
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

  function weaponItemsForChild(animation, childOrdinal, childCount) {
    var group = animation && animation.equipmentGroup;
    var result = {
      familyLabel: group && group.equipmentFamily
        ? group.equipmentFamily : 'Weapon',
      direct: [],
      fallback: []
    };
    if (!group) return result;
    var child = group.children.find(function(row) {
      return row.ordinal === childOrdinal;
    });
    if (child) {
      result.direct = child.mappedItems.map(function(item) {
        return { itemId: item.itemId, name: item.itemName };
      });
    }
    if (childOrdinal === group.selectionTable.outOfRangeFallbackChild) {
      result.fallback = group.fallbackItems.map(function(item) {
        return { itemId: item.itemId, name: item.itemName,
          requestedOrdinal: item.requestedOrdinal };
      });
    }
    return result;
  }

  function readableItemNames(items) {
    var names = items.map(function(item) { return item.name; });
    if (names.length < 2) return names.join('');
    if (names.length === 2) return names[0] + ' and ' + names[1];
    return names.slice(0, -1).join(', ') + ', and ' + names[names.length - 1];
  }

  function retailMappingText(animation, childCount) {
    var mapped = animation.spec.retailMappedWeaponOrdinals || [];
    var group = animation.equipmentGroup;
    var familyLabel = group && group.equipmentFamily
      ? group.equipmentFamily.toLowerCase() : 'weapon';
    var fallback = weaponItemsForChild(animation, 0, childCount).fallback;
    if (mapped.length === childCount && mapped.every(function(value, index) {
      return value === index;
    }) && !fallback.length) {
      return ' Every physical child has a known vanilla ' + familyLabel + ' mapping.';
    }
    var text = ' Vanilla ' + familyLabel +
      ' items directly select physical child ordinal' +
      (mapped.length === 1 ? ' ' : 's ') + mapped.join(', ') +
      '.';
    if (fallback.length) {
      text += ' ' + readableItemNames(fallback) +
        (fallback.length === 1 ? ' uses' : ' use') +
        ' child 0 as a fallback because the requested appearance lies outside this group.';
    }
    return text + ' Other physical children have no vanilla ' +
      familyLabel + ' selection.';
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

  function selectedChildOrdinal(layer, source, ui) {
    return childOrdinalForSource(source, ui.animationWeaponChild, layer);
  }

  function selectLayer(state, animation, frame, layer, ui) {
    ui.animationLayer = layer.ordinal;
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = selectedChildOrdinal(layer, source, ui);
    if (!source.editable) {
      ui.animationPaletteIndex = 0;
      ui.animationIntensity = 15;
      return;
    }
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
      editableSources: sources.filter(function(source) {
        return source.editable;
      }).length,
      lockedSources: sources.filter(function(source) {
        return !source.editable;
      }).length,
      weaponSources: sources.filter(function(source) {
        return source.weaponSelectable;
      }).length
    };
  }

  function mappingBanner(animation) {
    var status = animation.mappingStatus || {
      severity: 'failure', title: 'Mapping unavailable'
    };
    if (status.severity !== 'failure') return null;
    var warning = badge(status.title, status.severity);
    warning.className += ' animation-mapping-warning';
    return warning;
  }

  function animationPicker(state, selected, ui, rerender, catalog, rom, options) {
    var section = element('section', 'animation-corpus-section');
    var heading = element('div', 'animation-section-heading');
    heading.appendChild(element('h3', '', 'Class and action animation'));
    section.appendChild(heading);
    var ordered = catalog.specs.slice().sort(function(left, right) {
      return left.spec.displayOrder - right.spec.displayOrder;
    });
    var idleClassId = ui.animationTargetLaneKey === 'idle' &&
      Number.isInteger(ui.animationTargetClassId)
      ? ui.animationTargetClassId
      : (isIdleAnimation(selected) ? selected.spec.classId : null);
    if (Number.isInteger(idleClassId)) {
      idleAnimationRows(state.animations, idleClassId).forEach(function(animation) {
        if (!ordered.some(function(row) { return row.key === animation.key; })) {
          ordered.push(animation);
        }
      });
      Object.keys(rom.animationSequences &&
          rom.animationSequences.separations || {}).forEach(function(id) {
        var separation = rom.animationSequences.separations[id];
        var animation = separation && separation.syntheticAnimation;
        if (separation && separation.laneKey === 'idle' && animation &&
            animation.spec.classId === idleClassId &&
            !ordered.some(function(row) { return row.key === animation.key; })) {
          ordered.push(animation);
        }
      });
    }
    var selectedMotion = isClassMotionAnimation(selected)
      ? classMotionSpec(selected) : null;
    var targetMotion = classMotionSpec(ui.animationTargetLaneKey) || selectedMotion;
    var motionClassId = targetMotion &&
      Number.isInteger(ui.animationTargetClassId)
      ? ui.animationTargetClassId
      : (targetMotion && selected ? selected.spec.classId : null);
    if (Number.isInteger(motionClassId)) {
      classMotionAnimationRows(state.animations, motionClassId)
        .forEach(function(animation) {
          if (!ordered.some(function(row) { return row.key === animation.key; })) {
            ordered.push(animation);
          }
        });
    }
    function activate(animation, updateTarget) {
      if (!animation) return;
      if (updateTarget !== false) rememberAnimationTarget(ui, animation);
      if (animation.key === selected.key) {
        rerender();
        return;
      }
      ui.animationKey = animation.key;
      rememberAnimationSelection(ui, animation);
      ui.animationFrame = 0;
      ui.animationLayer = animation.frames[0].layers[0].ordinal;
      weaponChildForAnimation(ui, animation);
      selectLayer(state, animation, animation.frames[0],
        animation.frames[0].layers[0], ui);
      rerender();
    }
    function activeIdleRoute(animation) {
      if (!animation || !isIdleAnimation(animation) ||
          !OB64.animationSequences) return animation;
      var bodyFlags = OB64.animationSequences.bodyFlagsFor(animation);
      return OB64.animationSequences.routeAnimation(
        rom, animation.spec.classId, IDLE_ACTION_ID, bodyFlags, 'idle') ||
        animation;
    }
    function selector(labelText, rows, value, valueFor, textFor, onSelect,
        extraOptions) {
      var label = element('label', 'animation-corpus-field');
      label.appendChild(element('span', '', labelText));
      var select = element('select');
      rows.forEach(function(row) {
        var option = element('option', row.optionClass || '', textFor(row));
        option.value = valueFor(row);
        if (row.optionClass) {
          option.setAttribute('data-select-class', row.optionClass);
        }
        if (row.optionTitle) option.title = row.optionTitle;
        if (option.value === value) {
          option.selected = true;
          if (row.optionClass) select.classList.add(row.optionClass);
        }
        select.appendChild(option);
      });
      (extraOptions || []).forEach(function(extra) {
        var option = element('option', extra.optionClass || '', extra.text);
        option.value = extra.value;
        option.disabled = true;
        if (extra.title) option.title = extra.title;
        option.setAttribute('data-mapping-failure', 'true');
        select.appendChild(option);
      });
      select.addEventListener('change', function() {
        select.classList.remove('animation-variant-player',
          'animation-variant-enemy');
        var option = select.options[select.selectedIndex];
        var selectedClass = option && option.getAttribute('data-select-class');
        if (selectedClass) select.classList.add(selectedClass);
        onSelect(select.value);
      });
      label.appendChild(select);
      return label;
    }
    function searchableClassSelector(rows, value, onSelect) {
      var field = element('div',
        'animation-corpus-field animation-corpus-class-field');
      field.appendChild(element('span', '', 'Class'));
      var selectedClass = rows.find(function(row) {
        return !row.missingAnimation && String(row.spec.classId) === value;
      });
      var dropdown = element('details', 'animation-class-dropdown');
      var summary = element('summary', 'animation-class-dropdown-toggle',
        selectedClass ? M.hex(selectedClass.spec.classId, 2) + ' · ' +
          selectedClass.spec.className : 'Choose a class');
      dropdown.appendChild(summary);
      var panel = element('div', 'animation-class-dropdown-panel');
      var search = element('input', 'animation-corpus-search');
      search.type = 'search';
      search.placeholder = 'Search name, hex ID, or decimal ID';
      search.autocomplete = 'off';
      search.spellcheck = false;
      search.value = ui.animationClassSearch || '';
      search.setAttribute('aria-label', 'Search animation classes');
      panel.appendChild(search);
      var count = element('div', 'animation-class-dropdown-count');
      panel.appendChild(count);
      var table = element('div', 'animation-class-dropdown-table');
      table.setAttribute('role', 'listbox');
      table.setAttribute('aria-label', 'Animation classes');
      var records = rows.map(function(row) {
        var choice = element('button', 'animation-class-dropdown-row' +
          (String(row.spec.classId) === value ? ' selected' : '') +
          (row.missingAnimation ? ' missing' : ''));
        choice.type = 'button';
        choice.setAttribute('role', 'option');
        choice.setAttribute('aria-selected',
          String(row.spec.classId) === value ? 'true' : 'false');
        choice.appendChild(element('span', 'animation-class-dropdown-id',
          M.hex(row.spec.classId, 2)));
        choice.appendChild(element('span', 'animation-class-dropdown-name',
          row.spec.className));
        choice.appendChild(element('span', 'animation-class-dropdown-status',
          row.missingAnimation ? 'No mapped sequence' :
            (String(row.spec.classId) === value ? 'Selected' : '')));
        if (row.missingAnimation) {
          choice.disabled = true;
          choice.setAttribute('data-mapping-failure', 'true');
        } else {
          choice.addEventListener('click', function() {
            choose(row);
          });
        }
        table.appendChild(choice);
        return { animation: row, node: choice };
      });
      panel.appendChild(table);
      var noMatches = element('div', 'animation-class-dropdown-empty',
        'No classes match this search.');
      panel.appendChild(noMatches);
      dropdown.appendChild(panel);
      field.appendChild(dropdown);
      var selectableMatches = [];
      function populate() {
        ui.animationClassSearch = search.value;
        var matchCount = 0;
        selectableMatches = [];
        records.forEach(function(record) {
          var matches = classSearchMatches(record.animation, search.value);
          record.node.hidden = !matches;
          if (!matches) return;
          matchCount++;
          if (!record.animation.missingAnimation) {
            selectableMatches.push(record.animation);
          }
        });
        count.textContent = matchCount + ' of ' + rows.length + ' classes';
        noMatches.hidden = !!matchCount;
      }
      function choose(row) {
        if (!row || row.missingAnimation) return;
        search.value = '';
        ui.animationClassSearch = '';
        dropdown.open = false;
        onSelect(String(row.spec.classId));
        if (String(row.spec.classId) === value) populate();
      }
      search.addEventListener('input', populate);
      search.addEventListener('keydown', function(event) {
        if (event.key === 'Escape') {
          event.preventDefault();
          dropdown.open = false;
          summary.focus();
          return;
        }
        if (event.key !== 'Enter' || selectableMatches.length !== 1) return;
        event.preventDefault();
        choose(selectableMatches[0]);
      });
      dropdown.addEventListener('toggle', function() {
        if (!dropdown.open) return;
        window.requestAnimationFrame(function() {
          search.focus();
          search.select();
        });
      });
      field.addEventListener('focusout', function() {
        window.setTimeout(function() {
          if (dropdown.open && !field.contains(document.activeElement)) {
            dropdown.open = false;
          }
        }, 0);
      });
      populate();
      return field;
    }
    function variantDropdown(rows, selectedChoice, missingRows, targetActionId,
        liveClassRows) {
      var field = element('div',
        'animation-corpus-field animation-variant-field');
      field.appendChild(element('span', '', 'Variant'));
      var dropdown = element('details', 'animation-variant-dropdown');
      var summary = element('summary', 'animation-variant-dropdown-toggle ' +
        (selectedChoice ? selectedChoice.optionClass : ''),
      selectedChoice ? selectedChoice.label : 'Choose a variant');
      dropdown.appendChild(summary);
      var panel = element('div', 'animation-variant-dropdown-panel');
      panel.setAttribute('role', 'listbox');
      panel.setAttribute('aria-label', 'Animation variants and assignment actions');
      rows.forEach(function(choice) {
        var representative = choice.representative;
        var isSelected = choice === selectedChoice;
        var targetRawMode = choice.laneKey === 'blocked'
          ? 2 : representative.spec.rawMode;
        var targetRepresentative = (liveClassRows || []).find(function(animation) {
          return animation.spec.actionId === Number(targetActionId) &&
            selectorFlags(animation) === choice.flags &&
            animation.spec.rawMode === targetRawMode;
        }) || (liveClassRows || []).find(function(animation) {
          return animation.spec.actionId === Number(targetActionId) &&
            selectorFlags(animation) === choice.flags &&
            animationLaneKey(animation) === choice.laneKey;
        });
        var separation = OB64.animationSequences
          ? OB64.animationSequences.separationFor(
            targetRepresentative, rom.animationSequences) : null;
        var pair = routePairForAnimation(rom, targetRepresentative);
        var bodyFlags = OB64.animationSequences && targetRepresentative
          ? OB64.animationSequences.bodyFlagsFor(targetRepresentative) : null;
        var exact = Number.isInteger(bodyFlags) && OB64.combatAnimationOverrides
          ? OB64.combatAnimationOverrides.exactEntry(
            rom.combatAnimationOverrides, selected.spec.classId,
            Number(targetActionId), bodyFlags)
          : null;
        var laneSelector = exact && choice.laneKey === 'blocked'
          ? exact.blockedSelector : (exact ? exact.normalSelector : null);
        var assigned = !separation && exact &&
          laneSelector === representative.spec.selector;
        var row = element('div', 'animation-variant-dropdown-row ' +
          choice.optionClass + (isSelected ? ' selected' : '') +
          (separation ? ' separated' : ''));
        row.setAttribute('role', 'option');
        row.setAttribute('aria-selected', isSelected ? 'true' : 'false');
        var preview = button(choice.label,
          'animation-variant-dropdown-preview ' + choice.optionClass, function() {
            dropdown.open = false;
            activate(representative);
          });
        preview.title = choice.optionTitle;
        row.appendChild(preview);
        var rowStatus = element('div', 'animation-variant-dropdown-status');
        if (separation) rowStatus.appendChild(badge('Separated', 'edited'));
        else if (assigned) rowStatus.appendChild(badge('Assigned', 'mapped'));
        row.appendChild(rowStatus);
        var actions = element('div', 'animation-variant-dropdown-actions');
        var assign = button('Assign', 'animation-variant-assign', function(event) {
          event.preventDefault(); event.stopPropagation();
          try {
            OB64.animationSequences.assignShared(rom, representative, pair,
              targetRepresentative);
            rememberAnimationSelection(ui, targetRepresentative);
            if (options && options.onAnimationMappingChange) {
              options.onAnimationMappingChange();
            }
            notify(options, choice.sourceActionLabel + ' ' + choice.sideLabel +
              ' ' + choice.artVariantLabel + ' ' + choice.laneLabel +
              ' is assigned to ' + selected.spec.className + ' ' +
              (OB64.ACTION_TEMPLATE_LABELS &&
                OB64.ACTION_TEMPLATE_LABELS[Number(targetActionId)]
                ? OB64.ACTION_TEMPLATE_LABELS[Number(targetActionId)]
                : selected.spec.actionName) + '.');
            rerender();
          } catch (error) {
            notify(options, 'Animation assignment blocked: ' + error.message);
          }
        });
        assign.disabled = !targetRepresentative || !pair || !!separation || !!assigned ||
          !rom.animationSequences || !rom.animationSequences.supported;
        if (separation) {
          assign.title = 'This route is separated. Use Copy From to replace its private sequence.';
        } else if (assigned) {
          assign.title = 'This exact class, attack, side, and art route is already assigned.';
        } else if (!targetRepresentative || !pair) {
          assign.title = 'The selected action has no matching art route or selector pair.';
        } else if (representative.spec.actionId !== Number(targetActionId)) {
          assign.title = 'Reuse this source action sequence for the selected action without copying it.';
        }
        actions.appendChild(assign);
        row.appendChild(actions);
        panel.appendChild(row);
      });
      (missingRows || []).forEach(function(missing) {
        var issue = element('div', 'animation-variant-dropdown-row missing ' +
          missing.optionClass, missing.text);
        issue.title = missing.title;
        issue.setAttribute('data-mapping-failure', 'true');
        panel.appendChild(issue);
      });
      dropdown.appendChild(panel);
      field.appendChild(dropdown);
      field.addEventListener('focusout', function() {
        window.setTimeout(function() {
          if (dropdown.open && !field.contains(document.activeElement)) {
            dropdown.open = false;
          }
        }, 0);
      });
      return field;
    }

    function sequenceDropdown(rows, currentChoice, targetAnimation, failures) {
      var field = element('div',
        'animation-corpus-field animation-variant-field animation-sequence-field');
      field.appendChild(element('span', '', 'Body Sprite Sequence'));
      var idleTarget = isIdleAnimation(targetAnimation);
      var motionTarget = isClassMotionAnimation(targetAnimation);
      var fixedTarget = idleTarget || motionTarget;
      var separation = !motionTarget && OB64.animationSequences
        ? OB64.animationSequences.routeSeparationFor(
          targetAnimation, rom.animationSequences) : null;
      var pair = fixedTarget ? null : routePairForAnimation(rom, targetAnimation);
      var needsUserAssignment = !!(targetAnimation.effectiveMapping &&
        targetAnimation.effectiveMapping.assignmentRequired);
      var targetLaneLabel = animationLaneLabel(targetAnimation);
      var previewChoice = selected && selected.key !== targetAnimation.key
        ? rows.find(function(choice) {
          return choice.rows.some(function(row) { return row.key === selected.key; });
        }) : null;
      var displayedChoice = previewChoice || currentChoice;
      var dropdown = element('details', 'animation-variant-dropdown');
      var summaryText = sequenceAssignmentSummary(
        currentChoice, targetAnimation, needsUserAssignment, previewChoice);
      var summary = element('summary', 'animation-variant-dropdown-toggle ' +
        animationSideClass(targetAnimation));
      summary.appendChild(element('span', 'animation-sequence-summary-label',
        summaryText));
      if (displayedChoice && displayedChoice.sequenceKind === 'modified') {
        summary.appendChild(editedSequenceBadge());
      }
      if (displayedChoice && displayedChoice.linkedToKey) {
        summary.appendChild(linkedSequenceBadge(displayedChoice, targetAnimation));
      }
      dropdown.appendChild(summary);
      var panel = element('div', 'animation-variant-dropdown-panel');
      panel.setAttribute('role', 'listbox');
      panel.setAttribute('aria-label', 'Original and edited ' +
        animationSideLabel(targetAnimation) + ' sequences available to this class');
      rows.forEach(function(choice) {
        var representative = choice.representative;
        var previewed = choice.rows.some(function(row) {
          return row.key === selected.key;
        });
        var sameCurrentBody = sameAnimationSequence(
          targetAnimation, representative);
        var assigned = !needsUserAssignment && !!currentChoice &&
          choice.key === currentChoice.key;
        var fallbackBody = needsUserAssignment && sameCurrentBody;
        var sameBodyOnly = sameCurrentBody && !assigned && !fallbackBody;
        var sharedIssue = !fixedTarget && OB64.animationSequences &&
          OB64.animationSequences.sharedAssignmentIssue
          ? OB64.animationSequences.sharedAssignmentIssue(
            representative, targetAnimation) : '';
        var row = element('div', 'animation-variant-dropdown-row ' +
          choice.optionClass + (previewed ? ' selected' : ''));
        row.setAttribute('role', 'option');
        row.setAttribute('aria-selected', previewed ? 'true' : 'false');
        var preview = button(choice.label,
          'animation-variant-dropdown-preview ' + choice.optionClass, function() {
            dropdown.open = false;
            activate(representative, false);
          });
        preview.title = (idleTarget
          ? 'Show this idle loop. '
          : (motionTarget
            ? 'Show this fixed class movement sequence. '
            : 'Preview this sequence without changing the assignment target. ')) +
          choice.optionTitle;
        row.appendChild(preview);
        var rowStatus = element('div', 'animation-variant-dropdown-status');
        if (choice.sequenceKind === 'modified') {
          rowStatus.appendChild(editedSequenceBadge());
        }
        if (choice.linkedToKey || sameBodyOnly) {
          rowStatus.appendChild(linkedSequenceBadge(choice, targetAnimation));
        }
        if (assigned) rowStatus.appendChild(badge(
          fixedTarget ? 'Current' : 'Assigned', 'mapped'));
        else if (fallbackBody) rowStatus.appendChild(badge('Game fallback', 'warning'));
        else if (previewed) rowStatus.appendChild(badge('Preview', 'shared'));
        row.appendChild(rowStatus);
        if (fixedTarget) {
          panel.appendChild(row);
          return;
        }
        var actions = element('div', 'animation-variant-dropdown-actions');
        var assign = button('Assign', 'animation-variant-assign', function(event) {
          event.preventDefault(); event.stopPropagation();
          try {
            OB64.animationSequences.assignShared(rom, representative, pair,
              targetAnimation);
            ui.animationKey = targetAnimation.key;
            rememberAnimationTarget(ui, targetAnimation);
            rememberAnimationSelection(ui, targetAnimation);
            if (options && options.onAnimationMappingChange) {
              options.onAnimationMappingChange();
            }
            notify(options, choice.label + ' is assigned to ' +
              targetAnimation.spec.className + ' ' +
              targetAnimation.spec.actionName + ' · ' +
              animationSideLabel(targetAnimation) + ' · ' +
              animationArtVariantLabel(targetAnimation) + ' · ' +
              animationLaneLabel(targetAnimation) + '.');
            rerender();
          } catch (error) {
            notify(options, 'Animation assignment blocked: ' + error.message);
          }
        });
        assign.textContent = 'Assign to ' + targetLaneLabel;
        assign.disabled = !pair || !!separation || !!assigned || !!sameBodyOnly ||
          !!sharedIssue ||
          !rom.animationSequences || !rom.animationSequences.supported;
        if (separation) {
          assign.textContent = 'Replace below';
          assign.title = 'This target has a separated sequence. Use Replace From below.';
        } else if (assigned) {
          assign.textContent = 'Current: ' + targetLaneLabel;
          assign.title = 'This body sequence is already assigned to the selected target.';
        } else if (fallbackBody) {
          assign.title = 'Explicitly assign the currently displayed fallback body ' +
            'animation to this action and mode.';
        } else if (sameBodyOnly) {
          assign.textContent = 'No change: ' + targetLaneLabel;
          assign.title = 'This linked label already uses the body program assigned ' +
            'to the selected target. Assigning it again would not change the ROM.';
        } else if (sharedIssue) {
          assign.textContent = 'Copy for ' + targetLaneLabel;
          assign.title = sharedIssue +
            '. Preview it, then use Copy From and Separate below.';
        } else {
          assign.title = 'Point the selected mode to this sequence without copying it.';
        }
        actions.appendChild(assign);
        row.appendChild(actions);
        panel.appendChild(row);
      });
      (failures || []).forEach(function(failure) {
        var sideLabel = String(failure.flags).split('/')[1] === '1'
          ? 'Enemy Side' : 'Player Side';
        var issue = element('div', 'animation-variant-dropdown-row missing ' +
          (sideLabel === 'Enemy Side'
            ? 'animation-variant-enemy' : 'animation-variant-player'),
        '[Issue] ' + (failure.kind && classMotionSpec(failure.kind)
          ? classMotionSpec(failure.kind).actionName
          : 'Native body program ' + M.hex(failure.selector, 2)) + ' · ' +
          'flags ' + failure.flags + ' · mode ' +
          failure.rawMode + ' · ' + failure.message);
        issue.setAttribute('data-mapping-failure', 'true');
        panel.appendChild(issue);
      });
      dropdown.appendChild(panel);
      field.appendChild(dropdown);
      field.addEventListener('focusout', function() {
        window.setTimeout(function() {
          if (dropdown.open && !field.contains(document.activeElement)) {
            dropdown.open = false;
          }
        }, 0);
      });
      return field;
    }
    var classes = animationClassChoices(ordered);
    var targetAnimation = assignmentTargetAnimation(ui, catalog, selected);
    rememberAnimationTarget(ui, targetAnimation);
    var classRows = ordered.filter(function(animation) {
      return animation.spec.classId === targetAnimation.spec.classId;
    });
    var actions = animationActionChoices(classRows, targetAnimation);
    var targetActionRows = classRows.filter(function(animation) {
      return animation.spec.actionId === targetAnimation.spec.actionId;
    });
    var artRoutes = animationArtRouteChoices(targetActionRows);
    var sequenceRows = animationSequenceCatalogRows(
      state.animations, rom.animationSequences, targetAnimation.spec.classId,
      selectorFlagParts(targetAnimation)[1], {
        idleOnly: isIdleAnimation(targetAnimation),
        classMotionKind: isClassMotionAnimation(targetAnimation)
          ? targetAnimation.spec.classMotionKind : null,
        flags: selectorFlags(targetAnimation)
      });
    var variantChoices = animationClassVariantChoices(sequenceRows);
    var currentSequenceChoice = currentSequenceChoiceForTarget(
      variantChoices, targetAnimation);
    var controls = element('div', 'animation-corpus-controls');
    controls.appendChild(searchableClassSelector(classes,
      String(targetAnimation.spec.classId), function(value) {
        if (isIdleAnimation(targetAnimation)) {
          var nextIdleRows = idleAnimationRows(state.animations, Number(value));
          var matchingIdleRows = nextIdleRows.filter(function(row) {
            return selectorFlags(row) === selectorFlags(targetAnimation);
          });
          var nextIdle = preferredAnimation(
            matchingIdleRows.length ? matchingIdleRows : nextIdleRows, {
              rawMode: 0,
              side: selectorFlagParts(targetAnimation)[1]
            });
          nextIdle = activeIdleRoute(nextIdle);
          if (!nextIdle) {
            notify(options, 'Idle / Rest is unavailable for the selected class.');
            return;
          }
          activate(nextIdle, true);
          return;
        }
        if (isClassMotionAnimation(targetAnimation)) {
          var nextMotionRows = classMotionAnimationRows(
            state.animations, Number(value),
            targetAnimation.spec.classMotionKind);
          var matchingMotionRows = nextMotionRows.filter(function(row) {
            return selectorFlags(row) === selectorFlags(targetAnimation);
          });
          var nextMotion = preferredAnimation(
            matchingMotionRows.length ? matchingMotionRows : nextMotionRows, {
              rawMode: 0,
              side: selectorFlagParts(targetAnimation)[1]
            });
          if (!nextMotion) {
            notify(options, animationLaneLabel(targetAnimation) +
              ' is unavailable for the selected class.');
            return;
          }
          activate(nextMotion, true);
          return;
        }
        var nextClassRows = ordered.filter(function(row) {
          return row.spec.classId === Number(value);
        });
        var firstAction = nextClassRows[0];
        var firstActionRows = nextClassRows.filter(function(row) {
          return row.spec.actionId === firstAction.spec.actionId;
        });
        var sameRouteRows = firstActionRows.filter(function(row) {
          return selectorFlags(row) === selectorFlags(targetAnimation) &&
            animationLaneKey(row) === animationLaneKey(targetAnimation);
        });
        activate(preferredAnimation(sameRouteRows.length
          ? sameRouteRows : firstActionRows, {
          rawMode: animationLaneKey(targetAnimation) === 'blocked' ? 2 : 0
        }), true);
      }));
    controls.appendChild(selector('Action', actions,
      String(targetAnimation.spec.actionId),
      function(row) { return String(row.spec.actionId); },
      function(row) {
        if (isIdleAnimation(row)) return 'Idle / Rest';
        if (isClassMotionAnimation(row)) return row.spec.actionName;
        return M.hex(row.spec.actionId, 2) + ' · ' + row.spec.actionName;
      }, function(value) {
        if (Number(value) === IDLE_ACTION_ID) {
          var idleRows = idleAnimationRows(
            state.animations, targetAnimation.spec.classId);
          var sameIdleRoute = idleRows.filter(function(row) {
            return selectorFlags(row) === selectorFlags(targetAnimation);
          });
          var idle = preferredAnimation(
            sameIdleRoute.length ? sameIdleRoute : idleRows, {
              rawMode: 0,
              side: selectorFlagParts(targetAnimation)[1]
            });
          idle = activeIdleRoute(idle);
          if (!idle) {
            notify(options, 'Idle / Rest is unavailable for the selected class.');
            return;
          }
          activate(idle, true);
          return;
        }
        var motion = classMotionSpec(Number(value));
        if (motion) {
          var motionRows = classMotionAnimationRows(
            state.animations, targetAnimation.spec.classId, motion.kind);
          var sameMotionRoute = motionRows.filter(function(row) {
            return selectorFlags(row) === selectorFlags(targetAnimation);
          });
          var nextMotion = preferredAnimation(
            sameMotionRoute.length ? sameMotionRoute : motionRows, {
              rawMode: 0,
              side: selectorFlagParts(targetAnimation)[1]
            });
          if (!nextMotion) {
            notify(options, motion.actionName +
              ' is unavailable for the selected class.');
            return;
          }
          activate(nextMotion, true);
          return;
        }
        var nextActionRows = classRows.filter(function(row) {
          return !isIdleAnimation(row) && !isClassMotionAnimation(row) &&
            row.spec.actionId === Number(value);
        });
        var sameRouteRows = nextActionRows.filter(function(row) {
          return selectorFlags(row) === selectorFlags(targetAnimation) &&
            animationLaneKey(row) === animationLaneKey(targetAnimation);
        });
        activate(preferredAnimation(sameRouteRows.length
          ? sameRouteRows : nextActionRows, {
          rawMode: animationLaneKey(targetAnimation) === 'blocked' ? 2 : 0
        }), true);
      }));
    var missingVariants =
      ((state.animations.mappingAudit.byClass[targetAnimation.spec.classId] || {})
        .missingFlags || []).map(function(flagLabel) {
        var flagParts = flagLabel.split('/');
        var sideLabel = flagParts[1] === '1' ? 'Enemy Side' : 'Player Side';
        var routeLabel = flagParts[0] === '1' ? 'B' : 'A';
        return {
          value: 'missing-flags-' + flagLabel,
          text: '[Issue] ' + sideLabel + ' · Missing art route ' + routeLabel,
          optionClass: flagParts[1] === '1'
            ? 'animation-variant-enemy' : 'animation-variant-player',
          title: 'ROM flags ' + flagLabel + ' · no descriptor row exists'
        };
      });
    controls.appendChild(selector('Art Variant', artRoutes,
      selectorFlags(targetAnimation),
      function(row) { return row.flags; },
      function(row) { return row.label; },
      function(value) {
        var nextRouteRows = targetActionRows.filter(function(row) {
          return selectorFlags(row) === value &&
            animationLaneKey(row) === animationLaneKey(targetAnimation);
        });
        var nextRoute = preferredAnimation(nextRouteRows, {
          rawMode: animationLaneKey(targetAnimation) === 'blocked' ? 2 : 0
        });
        activate(activeIdleRoute(nextRoute), true);
      }, missingVariants));
    var modes = [
      { key: 'normal', label: 'Normal (modes 0/1)', rawMode: 0 },
      { key: 'blocked', label: 'Attack Blocked (mode 2)', rawMode: 2 }
    ];
    if (!isIdleAnimation(targetAnimation) &&
        !isClassMotionAnimation(targetAnimation)) {
      controls.appendChild(selector('Mode', modes,
        animationLaneKey(targetAnimation),
        function(row) { return row.key; },
        function(row) { return row.label; },
        function(value) {
          var nextModeRows = targetActionRows.filter(function(row) {
            return selectorFlags(row) === selectorFlags(targetAnimation) &&
              animationLaneKey(row) === value;
          });
          activate(preferredAnimation(nextModeRows, {
            rawMode: value === 'blocked' ? 2 : 0
          }), true);
        }));
    }
    var sequenceFailures = isIdleAnimation(targetAnimation)
      ? (state.animations.idleSequenceFailures[targetAnimation.spec.classId] || [])
        .filter(function(failure) {
          return String(failure.flags).split('/')[1] ===
            String(selectorFlagParts(targetAnimation)[1]);
        })
      : (isClassMotionAnimation(targetAnimation)
        ? (state.animations.classMotionSequenceFailures[
          targetAnimation.spec.classId] || []).filter(function(failure) {
          return failure.kind === targetAnimation.spec.classMotionKind &&
            String(failure.flags).split('/')[1] ===
              String(selectorFlagParts(targetAnimation)[1]);
        })
        : state.animations.sequenceCatalogFailures);
    controls.appendChild(sequenceDropdown(
      variantChoices, currentSequenceChoice, targetAnimation,
      sequenceFailures));
    section.appendChild(controls);
    if (catalog.diagnostic) {
      var catalogDiagnostic = element('div',
        'animation-effective-diagnostic animation-mapping-failure',
      catalog.diagnostic);
      catalogDiagnostic.setAttribute('role', 'note');
      section.appendChild(catalogDiagnostic);
    }
    if (targetAnimation.effectiveMapping) {
      var effective = targetAnimation.effectiveMapping;
      var route = element('div', 'animation-effective-mapping');
      var sourceLabel = effective.source === 'separated'
        ? 'Separated sequence' : (effective.source === 'route'
          ? 'Exact route assignment' : (effective.source === 'override'
            ? 'Class Combat override' : (effective.source === 'corpus'
              ? 'Accepted corpus trace' : (effective.source === 'fallback'
                ? 'Game fallback' : 'Class Combat vanilla'))));
      route.appendChild(badge(sourceLabel,
      effective.assignmentRequired ? 'warning' :
        (effective.overridden ? 'edited' : 'mapped')));
      var targetActionLabel = OB64.actionEditorName
        ? OB64.actionEditorName(targetAnimation.spec.actionId)
        : targetAnimation.spec.actionName;
      route.appendChild(element('span', '', effective.assignmentRequired
        ? 'Attack ' + M.hex(targetAnimation.spec.actionId, 2) + ' · ' +
          targetActionLabel + ' · no body animation assigned · the game currently ' +
          'uses fallback ' + M.hex(effective.selector, 2) + ' · ' +
          (effective.ranks.length
            ? effective.ranks.join(', ') : 'not assigned to a rank')
        : 'Attack ' + M.hex(targetAnimation.spec.actionId, 2) + ' · ' +
          targetActionLabel + ' · ' +
          (effective.laneKey === 'blocked' ? 'Blocked' : 'Normal') +
          ' body animation ' + M.hex(effective.selector, 2) + ' · ' +
          (effective.ranks.length
            ? effective.ranks.join(', ') : 'not assigned to a rank')));
      route.title = effective.assignmentRequired
        ? 'Choose a body animation below. The editor has not selected one for you.'
        : 'The visible attack combines this attack action with this body animation.';
      section.appendChild(route);
    }
    var routeFailures = catalog.failures.filter(function(failure) {
      return failure.classId === targetAnimation.spec.classId;
    });
    if (routeFailures.length) {
      var failureDetails = element('details',
        'animation-effective-failures animation-mapping-warning');
      failureDetails.appendChild(element('summary', '', '[Issue] ' +
        routeFailures.length + ' effective selector preview' +
        (routeFailures.length === 1 ? '' : 's') + ' unavailable'));
      routeFailures.forEach(function(failure) {
        failureDetails.appendChild(element('div', '',
          M.hex(failure.actionId, 2) + ' Â· flags ' + failure.flags +
          ' Â· mode ' + failure.rawMode + ' Â· selector ' +
          (Number.isInteger(failure.selector)
            ? M.hex(failure.selector, 2) : 'unresolved') + ' Â· ' +
          failure.message));
      });
      section.appendChild(failureDetails);
    }
    var mappingWarning = mappingBanner(selected);
    if (mappingWarning) section.appendChild(mappingWarning);
    return section;
  }

  function openCopyFromModal(state, rom, separation, targetAnimation,
      previewAnimation, ui, options, rerender) {
    if (!targetAnimation || !OB64.animationSequences) return;
    var idleTarget = isIdleAnimation(targetAnimation);
    var replacing = !!separation;
    var copyCatalogOptions = animationCopyCatalogOptions(
      idleTarget, replacing);
    var pair = routePairForAnimation(rom, targetAnimation);
    if (!separation && ((!idleTarget && !pair) || !rom.animationSequences ||
        !rom.animationSequences.supported)) {
      notify(options, 'Copy From is unavailable because this target has no writable sequence route.');
      return;
    }
    var sourceAnimations = state.animations.specs.slice();
    Object.keys(rom.animationSequences && rom.animationSequences.separations || {})
      .sort().forEach(function(id) {
        var modified = rom.animationSequences.separations[id].syntheticAnimation;
        if (modified) sourceAnimations.push(modified);
      });
    var initialDonor = separation
      ? OB64.animationSequences.resolveRef(state.animations, separation.donorRef)
      : (previewAnimation || targetAnimation);
    if (!initialDonor) initialDonor = targetAnimation;
    var overlay = element('div', 'error-modal-overlay animation-copy-overlay');
    var modal = element('div', 'error-modal animation-copy-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'animation-copy-title');
    overlay.appendChild(modal);
    var header = element('div', 'error-modal-header');
    var title = element('h2', '', separation
      ? 'Replace Animation Sequence From'
      : 'Copy From and Separate Animation Sequence');
    title.id = 'animation-copy-title';
    header.appendChild(title);
    var closeButton = button('\u00D7', 'error-modal-close', close);
    closeButton.setAttribute('aria-label', 'Cancel sequence copy');
    header.appendChild(closeButton);
    modal.appendChild(header);
    var body = element('div', 'error-modal-body animation-copy-body');
    var intro = element('p', 'animation-copy-intro', separation
      ? 'Choose a source sequence. This replaces every frame and weapon sprite in the current private sequence.'
      : 'Choose a source sequence. This creates a private copy of every frame and weapon sprite, then assigns it to the selected target.');
    body.appendChild(intro);
    var controls = element('div', 'animation-copy-controls');
    function selectField(labelText) {
      var label = element('label', 'animation-corpus-field');
      label.appendChild(element('span', '', labelText));
      var select = element('select');
      label.appendChild(select);
      controls.appendChild(label);
      return select;
    }
    var classSelect = selectField('Class');
    var sequenceSelect = selectField('Sequence');
    body.appendChild(controls);
    var preview = element('div', 'animation-copy-preview');
    body.appendChild(preview);
    modal.appendChild(body);
    var footer = element('div', 'error-modal-footer animation-copy-footer');
    var cancelButton = button('Cancel', 'error-modal-ok', close);
    footer.appendChild(cancelButton);
    var copyInProgress = false;
    var defaultCopyLabel = separation ? 'Replace Sequence' : 'Create Separated Copy';
    var copyButton = button(separation ? 'Replace Sequence' : 'Create Separated Copy',
      'error-modal-ok', async function() {
      if (copyInProgress) return;
      var donor = currentDonor();
      if (!donor) return;
      setCopyBusy(true);
      await nextBrowserPaint();
      try {
        if (separation) {
          OB64.animationSequences.copyFrom(rom, separation, donor);
          changed(options);
        } else {
          separation = OB64.animationSequences.separateAndAssign(
            rom, donor, pair, targetAnimation);
          if (idleTarget) changed(options);
          else animationRouteChanged(options);
        }
        ui.animationKey = separation && separation.syntheticAnimation
          ? separation.syntheticAnimation.key : targetAnimation.key;
        rememberAnimationTarget(ui, targetAnimation);
        rememberAnimationSelection(ui, separation && separation.syntheticAnimation
          ? separation.syntheticAnimation : targetAnimation);
        notify(options, (replacing
          ? (idleTarget ? 'Private idle loop replaced from ' :
            'Private sequence replaced from ')
          : (idleTarget ? 'Private idle loop copied from ' :
            'Private sequence copied and assigned from ')) +
          donor.spec.className + ' ' + donor.spec.actionName + ' · ' +
           animationSideLabel(donor) + ' · ' + animationArtVariantLabel(donor) +
           ' · ' + animationLaneLabel(donor) + '.');
        close(true);
        rerender();
      } catch (error) {
        setCopyBusy(false);
        notify(options, 'Sequence copy blocked: ' + error.message);
      }
    });
    footer.appendChild(copyButton);
    modal.appendChild(footer);

    var donorClassAnimations = sourceAnimations.slice();
    if (copyCatalogOptions) {
      donorClassAnimations = donorClassAnimations.concat(
        state.animations.artRouteTemplates || []);
    }
    var classRows = animationClassChoices(donorClassAnimations).filter(function(row) {
      return !row.missingAnimation;
    });
    classRows.forEach(function(row) {
      var option = element('option', '', M.hex(row.spec.classId, 2) + ' · ' +
        row.spec.className);
      option.value = String(row.spec.classId);
      classSelect.appendChild(option);
    });
    classSelect.value = String(initialDonor.spec.classId);
    if (classSelect.selectedIndex < 0) classSelect.selectedIndex = 0;
    var currentChoices = [];

    function replaceOptions(select, rows, valueFor, textFor) {
      select.innerHTML = '';
      rows.forEach(function(row) {
        var option = element('option', row.optionClass || '', textFor(row));
        option.value = valueFor(row);
        if (row.optionTitle) option.title = row.optionTitle;
        select.appendChild(option);
      });
    }

    function rowsForClass() {
      var classId = Number(classSelect.value);
      return animationSequenceCatalogRows(
        state.animations, rom.animationSequences, classId, 0,
        copyCatalogOptions).concat(
        animationSequenceCatalogRows(
          state.animations, rom.animationSequences, classId, 1,
          copyCatalogOptions));
    }
    function populateSequences(wantedAnimation) {
      currentChoices = animationClassVariantChoices(rowsForClass());
      replaceOptions(sequenceSelect, currentChoices,
        function(choice) { return choice.key; },
        function(choice) { return choice.label; });
      var wanted = wantedAnimation && currentChoices.find(function(choice) {
        return choice.rows.some(function(row) {
          return sameAnimationSequence(row, wantedAnimation);
        });
      });
      if (wanted) sequenceSelect.value = wanted.key;
      if (sequenceSelect.selectedIndex < 0) sequenceSelect.selectedIndex = 0;
      updatePreview();
    }
    function currentChoice() {
      return currentChoices.find(function(choice) {
        return choice.key === sequenceSelect.value;
      }) || null;
    }
    function currentDonor() {
      var choice = currentChoice();
      return choice && choice.representative;
    }
    function setCopyBusy(busy) {
      copyInProgress = busy;
      modal.setAttribute('aria-busy', busy ? 'true' : 'false');
      classSelect.disabled = busy;
      sequenceSelect.disabled = busy;
      closeButton.disabled = busy;
      cancelButton.disabled = busy;
      copyButton.disabled = busy || !currentDonor();
      copyButton.textContent = busy
        ? (separation ? 'Replacing…' : 'Creating…') : defaultCopyLabel;
      intro.textContent = busy
        ? (separation ? 'Replacing the private sequence…' :
          'Creating and assigning the private sequence…')
        : (separation
          ? 'Choose a source sequence. This replaces every frame and weapon sprite in the current private sequence.'
          : 'Choose a source sequence. This creates a private copy of every frame and weapon sprite, then assigns it to the selected target.');
    }
    function updatePreview() {
      preview.innerHTML = '';
      var donor = currentDonor();
      copyButton.disabled = copyInProgress || !donor;
      if (!donor) return;
      var choice = currentChoice();
      var sourceHeading = element('div', 'animation-copy-source-heading');
      sourceHeading.appendChild(element('strong', 'animation-copy-source-label ' +
        (choice ? choice.optionClass : animationSideClass(donor)),
      donor.spec.className + ' · ' + (choice ? choice.label :
        animationArtVariantLabel(donor) + ' · ' + animationLaneLabel(donor))));
      if (choice && choice.sequenceKind === 'modified') {
        sourceHeading.appendChild(editedSequenceBadge());
      }
      if (choice && choice.linkedToKey) {
        sourceHeading.appendChild(linkedSequenceBadge(choice));
      }
      preview.appendChild(sourceHeading);
      var strip = element('div', 'animation-copy-frame-strip');
      donor.frames.forEach(function(frame) {
        var figure = element('figure', 'animation-copy-frame');
        var canvas = element('canvas');
        drawFrame(canvas, donor, frame, state.animations, 2, null, null, false,
          weaponChildForAnimation(ui, donor));
        figure.appendChild(canvas);
        figure.appendChild(element('figcaption', '',
          'Frame ' + (frame.sequenceIndex + 1) + ' · ' + frame.ticks + ' ticks'));
        strip.appendChild(figure);
      });
      preview.appendChild(strip);
    }
    classSelect.addEventListener('change', function() {
      initialDonor = null;
      populateSequences(null);
    });
    sequenceSelect.addEventListener('change', updatePreview);
    populateSequences(initialDonor);

    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    function close(force) {
      if (copyInProgress && force !== true) return;
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escapeHandler);
    }
    var escapeHandler = function(event) { if (event.key === 'Escape') close(); };
    document.addEventListener('keydown', escapeHandler);
    document.body.appendChild(overlay);
    classSelect.focus();
  }

  function cutsceneActorCopyContext(rom) {
    if (rom.cutsceneStudio && rom.cutsceneStudio.catalog &&
        rom.cutsceneStudio.spriteState) {
      return {
        catalog: rom.cutsceneStudio.catalog,
        sprites: rom.cutsceneStudio.spriteState
      };
    }
    if (rom.cutsceneActorCopyContext) return rom.cutsceneActorCopyContext;
    if (!OB64.cutsceneCatalog || !OB64.cutsceneData || !OB64.cutsceneSprites ||
        typeof OB64.cutsceneCatalog.createCatalog !== 'function' ||
        typeof OB64.cutsceneSprites.create !== 'function' ||
        typeof OB64.cutsceneSprites.actorSequence !== 'function') {
      throw new Error('Cutscene actor art is unavailable.');
    }
    var catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
    rom.cutsceneActorCopyContext = {
      catalog: catalog,
      sprites: OB64.cutsceneSprites.create(rom.z64, catalog)
    };
    return rom.cutsceneActorCopyContext;
  }

  function registerCopyDonor(animationState, donor) {
    Object.keys(donor && donor.artByKey || {}).forEach(function(key) {
      animationState.artByKey[key] = donor.artByKey[key];
    });
    return donor;
  }

  function copyDonorLabel(donor) {
    if (donor && donor.sourceKind === 'cutscene-actor') {
      var program = donor.cutsceneProgram;
      return donor.actorArtSource.label + ' · Key ' + program.animationKey +
        ' · facing ' + program.facing + ' · state ' + program.stateIndex +
        ' · Appearance ' + donor.selectedAppearance;
    }
    return donor ? donor.spec.className + ' · ' +
      animationArtVariantLabel(donor) : '';
  }

  function uniqueNumbers(rows, field) {
    var seen = {};
    return (rows || []).reduce(function(output, row) {
      var value = Number(row[field]);
      if (!Number.isInteger(value) || seen[value]) return output;
      seen[value] = true;
      output.push(value);
      return output;
    }, []).sort(function(left, right) { return left - right; });
  }

  function openSpriteCopyModal(state, rom, separation, targetAnimation,
      targetFrame, targetLayer, operation, ui, options, rerender) {
    if (!separation || !targetAnimation || !targetFrame ||
        !OB64.animationSequences) return;
    var copiesFrame = operation === 'copy-frame';
    var sourceAnimations = state.animations.specs.slice();
    Object.keys(rom.animationSequences.separations || {}).sort().forEach(function(id) {
      var modified = rom.animationSequences.separations[id].syntheticAnimation;
      if (modified) sourceAnimations.push(modified);
    });
    var overlay = element('div', 'error-modal-overlay animation-copy-overlay');
    var modal = element('div', 'error-modal animation-copy-modal animation-sprite-copy-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'animation-sprite-copy-title');
    overlay.appendChild(modal);
    var header = element('div', 'error-modal-header');
    var titleText = copiesFrame ? 'Copy Frame From' : 'Copy Sprite Layer From';
    var title = element('h2', '', titleText);
    title.id = 'animation-sprite-copy-title';
    header.appendChild(title);
    var closeButton = button('\u00D7', 'error-modal-close', close);
    closeButton.setAttribute('aria-label', 'Cancel sprite copy');
    header.appendChild(closeButton);
    modal.appendChild(header);
    var body = element('div', 'error-modal-body animation-copy-body');
    var intro = element('p', 'animation-copy-intro', copiesFrame
      ? 'Choose any source frame. Its complete layer stack and positions will replace this frame.'
      : 'Choose any sprite layer. Its art will replace this layer while keeping this layer position.');
    body.appendChild(intro);
    var controls = element('div', 'animation-copy-controls animation-sprite-copy-controls');
    function selectField(labelText) {
      var label = element('label', 'animation-corpus-field');
      label.appendChild(element('span', '', labelText));
      var select = element('select');
      label.appendChild(select); controls.appendChild(label);
      return { label: label, select: select };
    }
    var sourceField = copiesFrame ? null : selectField('Source');
    var classField = selectField('Class');
    var sequenceField = selectField('Sequence');
    var actorArtField = copiesFrame ? null : selectField('Actor Art Source');
    var actorAnimationField = copiesFrame ? null : selectField('Animation');
    var actorFacingField = copiesFrame ? null : selectField('Facing');
    var actorPoseField = copiesFrame ? null : selectField('Physical Pose');
    var actorAppearanceField = copiesFrame ? null : selectField('Appearance');
    var frameField = selectField('Frame');
    if (sourceField) {
      controls.classList.add('layer-copy');
      var combatOption = element('option', '', 'Combat classes');
      combatOption.value = 'combat';
      sourceField.select.appendChild(combatOption);
      var cutsceneOption = element('option', '', 'Cutscene actors');
      cutsceneOption.value = 'cutscene';
      sourceField.select.appendChild(cutsceneOption);
      sourceField.select.value = 'combat';
    }
    body.appendChild(controls);
    var preview = element('div', 'animation-copy-preview animation-sprite-copy-preview');
    body.appendChild(preview); modal.appendChild(body);
    var footer = element('div', 'error-modal-footer animation-copy-footer');
    var exportFrameButton = null;
    var exportWebmButton = null;
    var webmExporting = false;
    if (!copiesFrame) {
      var exportActions = element('div', 'animation-copy-export-actions');
      exportFrameButton = button('Export Frame PNG', 'btn-secondary',
        exportCurrentDonorFrame);
      exportWebmButton = button('Export Sequence WebM', 'btn-secondary',
        exportCurrentDonorWebm);
      exportActions.appendChild(exportFrameButton);
      exportActions.appendChild(exportWebmButton);
      footer.appendChild(exportActions);
    }
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var copyCompleteFrameButton = null;
    if (!copiesFrame) {
      copyCompleteFrameButton = button('Copy Complete Frame',
        'error-modal-ok', applyCompleteFrameCopy);
      copyCompleteFrameButton.title =
        'Copy every selected cutscene layer. The target frame timing stays unchanged.';
      copyCompleteFrameButton.hidden = true;
      footer.appendChild(copyCompleteFrameButton);
    }
    var applyButton = button(copiesFrame ? 'Copy Frame' : 'Copy Sprite',
      'error-modal-ok', applyCopy);
    footer.appendChild(applyButton); modal.appendChild(footer);
    var classRows = animationClassChoices(sourceAnimations).filter(function(row) {
      return !row.missingAnimation;
    });
    classRows.forEach(function(row) {
      var option = element('option', '', M.hex(row.spec.classId, 2) + ' · ' +
        row.spec.className);
      option.value = String(row.spec.classId);
      classField.select.appendChild(option);
    });
    classField.select.value = String(targetAnimation.spec.classId);
    if (classField.select.selectedIndex < 0) classField.select.selectedIndex = 0;
    var sequenceChoices = [];
    var selectedLayerOrdinal = targetLayer ? targetLayer.ordinal : 0;
    var cutsceneContext = null;
    var cutsceneDonor = null;
    var cutsceneDonorError = '';

    function usesCutsceneActors() {
      return !!sourceField && sourceField.select.value === 'cutscene';
    }

    function showField(field, visible) {
      if (field) field.label.hidden = !visible;
    }

    function updateSourceFields() {
      var cutscene = usesCutsceneActors();
      showField(classField, !cutscene);
      showField(sequenceField, !cutscene);
      showField(actorArtField, cutscene);
      showField(actorAnimationField, cutscene);
      showField(actorFacingField, cutscene);
      showField(actorPoseField, cutscene);
      showField(actorAppearanceField, cutscene);
      controls.classList.toggle('cutscene', cutscene);
      if (!copiesFrame) {
        intro.textContent = cutscene
          ? 'Choose a sprite layer, or copy the complete frame. A sprite copy keeps the target layer position.'
          : 'Choose any sprite layer. Its art will replace this layer while keeping this layer position.';
      }
      if (copyCompleteFrameButton) copyCompleteFrameButton.hidden = !cutscene;
    }

    function selectedChoice() {
      return sequenceChoices.find(function(choice) {
        return choice.key === sequenceField.select.value;
      }) || null;
    }
    function selectedDonor() {
      if (usesCutsceneActors()) return cutsceneDonor;
      var choice = selectedChoice();
      return choice && choice.representative;
    }
    function selectedDonorFrame() {
      var donor = selectedDonor();
      return donor && donor.frames[Number(frameField.select.value)] || null;
    }
    function selectedDonorLayer() {
      var frame = selectedDonorFrame();
      return frame && frame.layers.find(function(layer) {
        return layer.ordinal === selectedLayerOrdinal;
      }) || null;
    }
    function replaceOptions(select, rows, valueFor, textFor) {
      select.innerHTML = '';
      rows.forEach(function(row) {
        var option = element('option', row.optionClass || '', textFor(row));
        option.value = valueFor(row);
        if (row.optionTitle) option.title = row.optionTitle;
        select.appendChild(option);
      });
    }
    function ensureCutsceneContext() {
      if (!cutsceneContext) cutsceneContext = cutsceneActorCopyContext(rom);
      return cutsceneContext;
    }
    function renderableActorPrograms() {
      if (!cutsceneContext || !actorArtField.select.value) return [];
      return cutsceneContext.catalog.poseProgramsForBank(
        Number(actorArtField.select.value), { physical: true }).filter(function(program) {
          return program.programId && program.frames && program.frames.length;
        });
    }
    function selectedActorProgram() {
      return cutsceneContext && actorPoseField.select.value
        ? cutsceneContext.catalog.getPoseProgramById(actorPoseField.select.value)
        : null;
    }
    function refreshCutsceneDonor(wantedFrame) {
      cutsceneDonor = null;
      cutsceneDonorError = '';
      try {
        var program = selectedActorProgram();
        if (!program) throw new Error('The selected Actor Art Source has no renderable pose.');
        cutsceneDonor = registerCopyDonor(state.animations,
          OB64.cutsceneSprites.actorSequence(cutsceneContext.sprites, program,
            Number(actorAppearanceField.select.value)));
      } catch (error) {
        cutsceneDonorError = error && error.message ? error.message : String(error);
      }
      populateFrames(Number.isInteger(wantedFrame) ? wantedFrame : 0);
    }
    function populateActorPoses() {
      var animationKey = Number(actorAnimationField.select.value);
      var facing = Number(actorFacingField.select.value);
      var programs = renderableActorPrograms().filter(function(program) {
        return program.animationKey === animationKey && program.facing === facing;
      });
      replaceOptions(actorPoseField.select, programs,
        function(program) { return program.programId; },
        function(program) {
          return 'State ' + program.stateIndex + ' · ' + program.frames.length +
            ' frames · ' + program.durationFrames + ' ticks';
        });
      if (actorPoseField.select.selectedIndex < 0) actorPoseField.select.selectedIndex = 0;
      refreshCutsceneDonor(0);
    }
    function populateActorFacings() {
      var animationKey = Number(actorAnimationField.select.value);
      var programs = renderableActorPrograms().filter(function(program) {
        return program.animationKey === animationKey;
      });
      var facings = uniqueNumbers(programs, 'facing');
      replaceOptions(actorFacingField.select, facings,
        function(facing) { return String(facing); },
        function(facing) { return 'Facing ' + facing; });
      if (actorFacingField.select.selectedIndex < 0) actorFacingField.select.selectedIndex = 0;
      populateActorPoses();
    }
    function populateActorAnimations() {
      var programs = renderableActorPrograms();
      var animations = uniqueNumbers(programs, 'animationKey');
      replaceOptions(actorAnimationField.select, animations,
        function(animationKey) { return String(animationKey); },
        function(animationKey) { return 'Key ' + animationKey; });
      if (actorAnimationField.select.selectedIndex < 0) {
        actorAnimationField.select.selectedIndex = 0;
      }
      populateActorFacings();
    }
    function populateActorArtSources() {
      try {
        var context = ensureCutsceneContext();
        if (!actorArtField.select.children.length) {
          context.catalog.actorArtSources.forEach(function(source) {
            var option = element('option', '', source.label + ' · ' +
              source.renderablePoseCount + ' sequences');
            option.value = String(source.bank);
            actorArtField.select.appendChild(option);
          });
          for (var appearance = 0; appearance < 8; appearance++) {
            var appearanceOption = element('option', '', 'Appearance ' + appearance);
            appearanceOption.value = String(appearance);
            actorAppearanceField.select.appendChild(appearanceOption);
          }
        }
        if (actorArtField.select.selectedIndex < 0) actorArtField.select.selectedIndex = 0;
        if (actorAppearanceField.select.selectedIndex < 0) {
          actorAppearanceField.select.selectedIndex = 0;
        }
        populateActorAnimations();
      } catch (error) {
        cutsceneDonor = null;
        cutsceneDonorError = error && error.message ? error.message : String(error);
        populateFrames(0);
      }
    }
    function rowsForClass() {
      var classId = Number(classField.select.value);
      return animationSequenceCatalogRows(
        state.animations, rom.animationSequences, classId, 0).concat(
        animationSequenceCatalogRows(
          state.animations, rom.animationSequences, classId, 1));
    }
    function populateSequences(wanted) {
      sequenceChoices = animationClassVariantChoices(rowsForClass());
      replaceOptions(sequenceField.select, sequenceChoices,
        function(choice) { return choice.key; },
        function(choice) { return choice.label; });
      var match = wanted && sequenceChoices.find(function(choice) {
        return choice.rows.some(function(row) {
          return row === wanted || row.key === wanted.key;
        });
      });
      if (!match && wanted) {
        match = sequenceChoices.find(function(choice) {
          return choice.rows.some(function(row) {
            return sameAnimationSequence(row, wanted);
          });
        });
      }
      if (match) sequenceField.select.value = match.key;
      if (sequenceField.select.selectedIndex < 0) sequenceField.select.selectedIndex = 0;
      populateFrames(wanted === targetAnimation ? targetFrame.sequenceIndex : 0);
    }
    function populateFrames(wantedIndex) {
      var donor = selectedDonor(), rows = donor ? donor.frames : [];
      replaceOptions(frameField.select, rows,
        function(frame) { return String(frame.sequenceIndex); },
        function(frame) {
          return 'Frame ' + (frame.sequenceIndex + 1) + ' · ' + frame.ticks + ' ticks';
        });
      frameField.select.value = String(wantedIndex);
      if (frameField.select.selectedIndex < 0) frameField.select.selectedIndex = 0;
      populateLayers(targetLayer ? targetLayer.ordinal : 0);
    }
    function populateLayers(wantedOrdinal) {
      if (copiesFrame) { updatePreview(); return; }
      var frame = selectedDonorFrame();
      var rows = frame ? frame.layers : [];
      selectedLayerOrdinal = rows.some(function(layer) {
        return layer.ordinal === wantedOrdinal;
      }) ? wantedOrdinal : (rows.length ? rows[0].ordinal : 0);
      updatePreview();
    }
    function updatePreview() {
      var oldBodyScroll = body.scrollTop;
      var oldLayerList = preview.querySelector('.animation-copy-layer-list');
      var oldLayerScroll = oldLayerList ? oldLayerList.scrollTop : 0;
      preview.innerHTML = '';
      var donor = selectedDonor(), frame = selectedDonorFrame();
      applyButton.disabled = !donor || !frame || (!copiesFrame && !selectedDonorLayer());
      if (copyCompleteFrameButton) {
        copyCompleteFrameButton.disabled = !usesCutsceneActors() || !donor || !frame;
      }
      if (exportFrameButton) exportFrameButton.disabled = !donor || !frame;
      if (exportWebmButton) {
        exportWebmButton.disabled = webmExporting || !donor || !animationWebmSupported();
        exportWebmButton.title = animationWebmSupported()
          ? 'Download the complete selected source sequence with transparent pixels.'
          : 'This browser cannot record canvas video as WebM.';
      }
      if (!donor || !frame) {
        if (cutsceneDonorError) {
          preview.appendChild(element('p', 'art-unavailable',
            'Cutscene actor source unavailable: ' + cutsceneDonorError));
        }
        setTimeout(function() {
          if (body.isConnected) body.scrollTop = oldBodyScroll;
        }, 0);
        return;
      }
      var heading = element('strong', 'animation-copy-source-label' +
        (donor.sourceKind === 'cutscene-actor' ? '' : ' ' + animationSideClass(donor)),
      copyDonorLabel(donor) + ' · Frame ' + (frame.sequenceIndex + 1));
      preview.appendChild(heading);
      if (donor.sourceKind === 'cutscene-actor' && donor.appearanceFallbackCount) {
        preview.appendChild(element('p', 'animation-copy-source-note',
          donor.appearanceFallbackCount +
          ' sprite source' + (donor.appearanceFallbackCount === 1 ? '' : 's') +
          (donor.appearanceFallbackCount === 1 ? ' uses ' : ' use ') +
          'Appearance 0 because the selected Appearance is unavailable.'));
      }
      var showAnimationPreview = !copiesFrame &&
        donor.sourceKind === 'cutscene-actor';
      var previewGrid = element('div', 'animation-sprite-copy-preview-grid' +
        (copiesFrame ? ' frame-only' : '') +
        (showAnimationPreview ? ' with-sequence-preview' : ''));
      var fullPane = element('figure', 'animation-sprite-copy-pane');
      fullPane.appendChild(element('figcaption', '', 'Complete frame'));
      var fullCanvas = element('canvas');
      var weaponChild = donor.sourceKind === 'cutscene-actor'
        ? 0 : weaponChildForAnimation(ui, donor);
      drawFrame(fullCanvas, donor, frame, state.animations, 4,
        null, null, false, weaponChild);
      fullPane.appendChild(fullCanvas); previewGrid.appendChild(fullPane);
      if (!copiesFrame) {
        var layerList = element('div', 'animation-copy-layer-list');
        layerList.appendChild(element('strong', '', 'Sprite layers'));
        frame.layers.forEach(function(row) {
          var selected = row.ordinal === selectedLayerOrdinal;
          var layerButton = button(
            layerDisplayLabel(row, donor.artByKey[row.sourceKey]),
            'animation-copy-layer-choice' + (selected ? ' selected' : ''),
            function() {
              selectedLayerOrdinal = row.ordinal;
              updatePreview();
            });
          layerButton.setAttribute('aria-pressed', selected ? 'true' : 'false');
          layerList.appendChild(layerButton);
        });
        previewGrid.appendChild(layerList);
        var selectedPane = element('figure', 'animation-sprite-copy-pane');
        selectedPane.appendChild(element('figcaption', '', 'Selected sprite only'));
        var selectedCanvas = element('canvas');
        paintPixels(selectedCanvas, donor.canvas.width, donor.canvas.height,
          singleLayerPixels(donor, selectedDonorLayer(), state.animations,
            weaponChild), 4);
        selectedPane.appendChild(selectedCanvas); previewGrid.appendChild(selectedPane);
        if (showAnimationPreview) {
          previewGrid.appendChild(animationSequencePreview(state, donor, ui, {
            className: 'animation-sprite-copy-pane animation-copy-sequence-pane',
            caption: 'Animation preview · 30 ticks/sec',
            ariaLabel: 'Looping cutscene actor animation preview',
            weaponChildOrdinal: 0
          }));
        }
        setTimeout(function() {
          if (layerList.isConnected) layerList.scrollTop = oldLayerScroll;
        }, 0);
      }
      preview.appendChild(previewGrid);
      setTimeout(function() {
        if (body.isConnected) body.scrollTop = oldBodyScroll;
      }, 0);
    }

    function exportCurrentDonorFrame() {
      var donor = selectedDonor(), frame = selectedDonorFrame();
      if (!donor || !frame || !exportFrameButton) return;
      exportFrameButton.disabled = true;
      downloadFrame(donor, frame, state, donor.sourceKind === 'cutscene-actor'
        ? 0 : weaponChildForAnimation(ui, donor)).then(function(filename) {
        notify(options, 'Frame exported as ' + filename + '.');
      }).catch(function(error) {
        notify(options, error && error.message ? error.message : String(error));
      }).then(function() {
        exportFrameButton.disabled = !selectedDonorFrame();
      });
    }

    function exportCurrentDonorWebm() {
      var donor = selectedDonor();
      if (!donor || !exportWebmButton || exportWebmButton.disabled) return;
      webmExporting = true;
      exportWebmButton.disabled = true;
      exportWebmButton.textContent = 'Exporting Sequence…';
      notify(options, 'Recording the complete transparent source sequence at 30 ticks per second.');
      downloadAnimationWebm(donor, state, donor.sourceKind === 'cutscene-actor'
        ? 0 : weaponChildForAnimation(ui, donor)).then(function(filename) {
        notify(options, 'Sequence exported as ' + filename + '.');
      }).catch(function(error) {
        notify(options, error && error.message ? error.message : String(error));
      }).then(function() {
        webmExporting = false;
        exportWebmButton.textContent = 'Export Sequence WebM';
        exportWebmButton.disabled = !selectedDonor() || !animationWebmSupported();
      });
    }

    function applySelectedCopy(copyCompleteFrame) {
      var donor = selectedDonor(), donorFrame = selectedDonorFrame();
      if (!donor || !donorFrame) return;
      var copiesCompleteFrame = copiesFrame || copyCompleteFrame;
      try {
        if (copiesCompleteFrame) {
          OB64.animationSequences.copyFrameFrom(rom, separation,
            targetFrame.sequenceIndex, donor, donorFrame.sequenceIndex);
          ui.animationLayer = 0;
        } else {
          var donorLayer = selectedDonorLayer();
          if (!donorLayer) return;
          ui.animationLayer = OB64.animationSequences.copyLayerFrom(
            rom, separation, targetFrame.sequenceIndex, targetLayer.ordinal,
            donor, donorFrame.sequenceIndex, donorLayer.ordinal);
        }
        var updatedAnimation = separation.syntheticAnimation;
        var updatedFrame = updatedAnimation.frames[targetFrame.sequenceIndex];
        selectLayer(state, updatedAnimation, updatedFrame,
          updatedFrame.layers[ui.animationLayer], ui);
        changed(options);
        notify(options, (copiesCompleteFrame ? 'Copied complete frame from ' :
          'Copied sprite from ') +
          copyDonorLabel(donor) + ' frame ' + (donorFrame.sequenceIndex + 1) + '.');
        close(); rerender();
      } catch (error) {
        notify(options, (copiesCompleteFrame
          ? 'Complete frame copy blocked: ' : 'Sprite copy blocked: ') +
          error.message);
      }
    }
    function applyCopy() {
      applySelectedCopy(false);
    }
    function applyCompleteFrameCopy() {
      if (!usesCutsceneActors()) return;
      applySelectedCopy(true);
    }
    classField.select.addEventListener('change', function() {
      populateSequences(null);
    });
    sequenceField.select.addEventListener('change', function() { populateFrames(0); });
    if (sourceField) sourceField.select.addEventListener('change', function() {
      updateSourceFields();
      if (usesCutsceneActors()) populateActorArtSources();
      else populateSequences(targetAnimation);
    });
    if (actorArtField) actorArtField.select.addEventListener('change',
      populateActorAnimations);
    if (actorAnimationField) actorAnimationField.select.addEventListener('change',
      populateActorFacings);
    if (actorFacingField) actorFacingField.select.addEventListener('change',
      populateActorPoses);
    if (actorPoseField) actorPoseField.select.addEventListener('change', function() {
      refreshCutsceneDonor(0);
    });
    if (actorAppearanceField) actorAppearanceField.select.addEventListener('change',
      function() { refreshCutsceneDonor(Number(frameField.select.value) || 0); });
    frameField.select.addEventListener('change', function() { populateLayers(0); });
    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escapeHandler);
    }
    var escapeHandler = function(event) { if (event.key === 'Escape') close(); };
    document.addEventListener('keydown', escapeHandler);
    document.body.appendChild(overlay);
    updateSourceFields();
    populateSequences(targetAnimation);
    (sourceField || classField).select.focus();
  }

  function importLibrarySequence(state, rom, separation, targetAnimation,
      previewAnimation, asset, ui, options, rerender) {
    if (!asset || asset.kind !== 'sequence' || !asset.frames.length) {
      notify(options, 'Sprite Library sequence import blocked: choose a frame sequence asset.');
      return;
    }
    if (asset.frames.length > 256) {
      notify(options,
        'Sprite Library sequence import blocked: combat sequences support at most 256 frames.');
      return;
    }
    var idleTarget = isIdleAnimation(targetAnimation);
    var pair = idleTarget ? null : routePairForAnimation(rom, targetAnimation);
    if (!separation && !idleTarget && !pair) {
      notify(options,
        'Sprite Library sequence import blocked: this target has no writable sequence route.');
      return;
    }
    var baseAnimation = separation && separation.syntheticAnimation
      ? separation.syntheticAnimation : (previewAnimation || targetAnimation);
    try {
      var preparedFrames = asset.frames.map(function(frame) {
        return {
          ticks: frame.ticks,
          prepared: OB64.art.prepareAnimationFrameImport(
            OB64.spriteLibrary.compositeFrame(asset,
              asset.frames.indexOf(frame)),
            asset.width, asset.height,
            baseAnimation.canvas.width, baseAnimation.canvas.height,
            { resizeMode: 'nearest', panX: 0.5, panY: 0.5, dither: false })
        };
      });
      var created = !separation;
      if (!separation) {
        separation = OB64.animationSequences.separateAndAssign(
          rom, baseAnimation, pair, targetAnimation);
      }
      var animation = separation.syntheticAnimation;
      while (animation.frames.length > preparedFrames.length) {
        OB64.animationSequences.removeFrame(
          rom, separation, animation.frames.length - 1);
        animation = separation.syntheticAnimation;
      }
      while (animation.frames.length < preparedFrames.length) {
        OB64.animationSequences.addBlankFrame(
          rom, separation, animation.frames.length - 1, 0);
        animation = separation.syntheticAnimation;
      }
      preparedFrames.forEach(function(row, frameIndex) {
        OB64.animationSequences.importFrame(
          rom, separation, frameIndex, row.prepared, { keepEquipment: false });
        OB64.animationSequences.setFrameTicks(
          rom, separation, frameIndex, row.ticks);
      });
      animation = separation.syntheticAnimation;
      ui.animationKey = animation.key;
      ui.animationFrame = 0;
      ui.animationLayer = 0;
      rememberAnimationTarget(ui, targetAnimation);
      rememberAnimationSelection(ui, animation);
      selectLayer(state, animation, animation.frames[0],
        animation.frames[0].layers[0], ui);
      if (created && !idleTarget) animationRouteChanged(options);
      else changed(options);
      notify(options, 'Imported Sprite Library sequence ' + asset.name +
        ' as ' + preparedFrames.length + ' private combat frame' +
        (preparedFrames.length === 1 ? '' : 's') + '.');
      rerender();
    } catch (error) {
      notify(options, 'Sprite Library sequence import blocked: ' + error.message);
    }
  }

  function sequenceStrip(state, animation, targetAnimation, ui, options,
      rerender, rom) {
    var section = element('section', 'animation-sequence-section');
    var heading = element('div', 'animation-section-heading');
    heading.appendChild(element('h3', '', 'Frame sequence'));
    var idleTarget = isIdleAnimation(targetAnimation);
    var motionTarget = isClassMotionAnimation(targetAnimation);
    var separation = !motionTarget && OB64.animationSequences
      ? OB64.animationSequences.routeSeparationFor(
        targetAnimation, rom.animationSequences)
      : null;
    var editableSeparation = animation.spec.separatedCopy &&
      OB64.animationSequences
      ? OB64.animationSequences.separationFor(
        animation, rom.animationSequences)
      : null;
    var headingActions = element('div', 'animation-sequence-heading-actions');
    if (separation) headingActions.appendChild(badge('Separated', 'edited'));
    if (!motionTarget) {
      var copyFrom = button(separation
        ? 'Replace From…' : 'Copy From and Separate…',
        'btn-secondary animation-copy-from', function() {
          openCopyFromModal(state, rom, separation, targetAnimation,
            animation, ui, options, rerender);
        });
      var pair = idleTarget ? null : routePairForAnimation(rom, targetAnimation);
      copyFrom.disabled = !targetAnimation ||
        (!idleTarget && !separation && !pair) ||
        !rom.animationSequences || !rom.animationSequences.supported;
      copyFrom.title = separation
        ? 'Replace this complete private sequence from another compatible sequence.'
        : (idleTarget
          ? 'Create a private copy of this idle loop for the selected class and art route.'
          : 'Create a complete private copy from another class, action, and variant, then assign it to this target.');
      headingActions.appendChild(copyFrom);
      if (OB64.spriteEditorUI && OB64.spriteEditorUI.openLibraryPicker) {
        var librarySequence = button(separation
          ? 'Replace from Library…' : 'Import Library Sequence…',
        'btn-secondary animation-copy-from', function() {
          OB64.spriteEditorUI.openLibraryPicker(rom, {
            title: separation
              ? 'Replace Private Sequence from Sprite Library'
              : 'Create Private Sequence from Sprite Library',
            actionLabel: separation ? 'Replace Sequence' : 'Import Sequence',
            kinds: ['sequence'],
            onStatus: function(message) { notify(options, message); }
          }, function(source) {
            importLibrarySequence(state, rom, separation, targetAnimation,
              animation, source.asset, ui, options, rerender);
          });
        });
        librarySequence.disabled = !targetAnimation ||
          (!idleTarget && !separation && !pair) ||
          !rom.animationSequences || !rom.animationSequences.supported;
        librarySequence.title = separation
          ? 'Replace this complete private sequence with a Sprite Library sequence.'
          : 'Convert a Sprite Library sequence into a private combat sequence and assign it to this target.';
        headingActions.appendChild(librarySequence);
      }
    }
    heading.appendChild(headingActions);
    section.appendChild(heading);
    var strip = element('div', 'animation-sequence-strip');
    strip.setAttribute('data-art-scroll-key', 'animations:sequence:' + animation.key);
    var draggedFrameIndex = null, dragged = false;
    animation.frames.forEach(function(frame) {
      var frameIndex = frame.sequenceIndex;
      var isSelected = frame.sequenceIndex === ui.animationFrame;
      var entry = element('div', 'animation-frame-entry');
      var card = element('button', 'animation-frame-card' +
        (isSelected ? ' selected' : ''));
      card.type = 'button';
      card.draggable = !!editableSeparation;
      card.setAttribute('aria-pressed', isSelected ? 'true' : 'false');
      var cardHeading = element('span', 'animation-card-selection-row');
      if (editableSeparation) {
        var dragHandle = element('span', 'animation-frame-drag-handle', '☰');
        dragHandle.setAttribute('aria-hidden', 'true');
        dragHandle.title = 'Drag to change frame order';
        cardHeading.appendChild(dragHandle);
      }
      cardHeading.appendChild(element('strong', 'animation-frame-number',
        'Frame ' + (frame.sequenceIndex + 1)));
      if (isSelected) cardHeading.appendChild(badge('Selected', 'selected'));
      if (animation.mappingStatus &&
          animation.mappingStatus.emptyFrameIndices.indexOf(frame.sequenceIndex) >= 0) {
        cardHeading.appendChild(badge('No body pixels', 'failure'));
      }
      card.appendChild(cardHeading);
      var canvas = element('canvas', 'animation-frame-thumbnail');
      drawFrame(canvas, animation, frame, state.animations, 4, null, null, false,
        weaponChildForAnimation(ui, animation));
      card.appendChild(canvas);
      card.addEventListener('click', function() {
        if (dragged) { dragged = false; return; }
        ui.animationFrame = frame.sequenceIndex; ui.animationLayer = frame.layers[0].ordinal;
        selectLayer(state, animation, frame, frame.layers[0], ui); rerender();
      });
      if (editableSeparation) {
        card.addEventListener('dragstart', function(event) {
          draggedFrameIndex = frameIndex;
          dragged = true;
          card.classList.add('dragging');
          if (event.dataTransfer) {
            event.dataTransfer.effectAllowed = 'move';
            event.dataTransfer.setData('text/plain', String(frameIndex));
          }
        });
        card.addEventListener('dragover', function(event) {
          event.preventDefault();
          card.classList.add('drag-target');
          if (event.dataTransfer) event.dataTransfer.dropEffect = 'move';
        });
        card.addEventListener('dragleave', function() {
          card.classList.remove('drag-target');
        });
        card.addEventListener('drop', function(event) {
          event.preventDefault();
          card.classList.remove('drag-target');
          if (!Number.isInteger(draggedFrameIndex)) return;
          var selectedFrame = animation.frames[ui.animationFrame];
          var selectedLayer = selectedFrame &&
            selectedFrame.layers[ui.animationLayer];
          try {
            OB64.animationSequences.moveFrame(rom, editableSeparation,
              draggedFrameIndex, frameIndex);
            var updatedAnimation = editableSeparation.syntheticAnimation;
            var selectedFrameIndex = updatedAnimation.frames.indexOf(selectedFrame);
            if (selectedFrameIndex < 0) selectedFrameIndex = 0;
            var updatedFrame = updatedAnimation.frames[selectedFrameIndex];
            var selectedLayerIndex = updatedFrame.layers.indexOf(selectedLayer);
            if (selectedLayerIndex < 0) selectedLayerIndex = 0;
            ui.animationFrame = selectedFrameIndex;
            ui.animationLayer = selectedLayerIndex;
            selectLayer(state, updatedAnimation, updatedFrame,
              updatedFrame.layers[selectedLayerIndex], ui);
            changed(options);
            notify(options, 'Updated frame order.');
            rerender();
          } catch (error) {
            notify(options, 'Frame reorder blocked: ' + error.message);
          }
        });
        card.addEventListener('dragend', function() {
          draggedFrameIndex = null;
          Array.prototype.forEach.call(
            strip.querySelectorAll('.animation-frame-card'), function(node) {
              node.classList.remove('dragging', 'drag-target');
            });
          setTimeout(function() { dragged = false; }, 0);
        });
      }
      entry.appendChild(card);
      var tickEditor = element('label', 'animation-frame-tick-editor');
      tickEditor.appendChild(element('span', 'animation-frame-ticks', 'Ticks'));
      var tickInput = element('input', 'animation-frame-tick-input');
      tickInput.type = 'number';
      tickInput.min = '0';
      tickInput.max = '255';
      tickInput.step = '1';
      tickInput.value = String(frame.ticks);
      tickInput.disabled = !editableSeparation;
      tickInput.setAttribute('aria-label',
        'Frame ' + (frame.sequenceIndex + 1) + ' ticks');
      tickInput.title = editableSeparation
        ? 'Set this frame duration from 0 through 255 ticks.'
        : 'Create a separated private sequence before changing frame ticks.';
      tickInput.addEventListener('change', function() {
        var wanted = tickInput.value === '' ? NaN : Number(tickInput.value);
        try {
          if (OB64.animationSequences.setFrameTicks(
              rom, editableSeparation, frameIndex, wanted)) {
            changed(options);
            notify(options, 'Set frame ' + (frameIndex + 1) + ' duration to ' +
              wanted + (wanted === 1 ? ' tick.' : ' ticks.'));
            rerender();
          } else {
            tickInput.value = String(frame.ticks);
          }
        } catch (error) {
          tickInput.value = String(frame.ticks);
          notify(options, 'Frame tick change blocked: ' + error.message);
        }
      });
      tickInput.addEventListener('keydown', function(event) {
        if (event.key === 'Enter') {
          event.preventDefault();
          tickInput.blur();
        } else if (event.key === 'Escape') {
          event.preventDefault();
          tickInput.value = String(frame.ticks);
          tickInput.blur();
        }
      });
      tickEditor.appendChild(tickInput);
      entry.appendChild(tickEditor);
      strip.appendChild(entry);
    });
    section.appendChild(strip);
    return section;
  }

  function layerDisplayLabel(layer, source) {
    return 'Layer ' + (layer.ordinal + 1) +
      (source.weaponSelectable ? ' · Weapon' : '');
  }

  function childPixels(source, animationState, childOrdinal) {
    childOrdinal = M.childOrdinalOrFallback(source, childOrdinal);
    var child = M.displayChild(animationState, source.key, childOrdinal);
    var palette = source.editable ? M.childPalette(source, childOrdinal) : null;
    var output = new Uint8ClampedArray(
      source.sprite.width * source.sprite.height * 4);
    var pixels = source.sprite.width * source.sprite.height;
    for (var pixel = 0; pixel < pixels; pixel++) {
      var rgb = source.editable
        ? wordRgb(palette[child.indices[pixel]])
        : wordRgb(child.words[pixel]);
      var offset = pixel * 4;
      output[offset] = rgb[0]; output[offset + 1] = rgb[1];
      output[offset + 2] = rgb[2];
      output[offset + 3] = source.editable
        ? child.intensity[pixel] * 17 : child.alpha[pixel];
    }
    return output;
  }

  function weaponSourceForFrame(animation, frame, selectedLayer) {
    var selectedSource = selectedLayer && animation.artByKey[selectedLayer.sourceKey];
    if (selectedSource && selectedSource.weaponSelectable) return selectedSource;
    var fallback = null;
    for (var layerIndex = 0; layerIndex < frame.layers.length; layerIndex++) {
      var frameSource = animation.artByKey[frame.layers[layerIndex].sourceKey];
      if (frameSource.weaponSelectable && frameSource.editable) return frameSource;
      if (frameSource.weaponSelectable && !fallback) fallback = frameSource;
    }
    var sourceKeys = Object.keys(animation.artByKey);
    for (var sourceIndex = 0; sourceIndex < sourceKeys.length; sourceIndex++) {
      var source = animation.artByKey[sourceKeys[sourceIndex]];
      if (source.weaponSelectable && source.editable) return source;
      if (source.weaponSelectable && !fallback) fallback = source;
    }
    return fallback;
  }

  function weaponPicker(state, animation, frame, layer, ui, rerender) {
    var source = weaponSourceForFrame(animation, frame, layer);
    var section = element('section', 'animation-weapon-section');
    var heading = element('div', 'animation-section-heading');
    heading.appendChild(element('h3', '', 'Weapon sprite'));
    if (!source) {
      heading.appendChild(element('p', '', 'No weapon sprites.'));
      section.appendChild(heading);
      return section;
    }
    var selectedChild = weaponChildForAnimation(ui, animation);
    section.appendChild(heading);
    var strip = element('div', 'animation-weapon-strip');
    strip.setAttribute('data-art-scroll-key', 'animations:weapons:' + animation.key);
    source.selectableChildOrdinals.forEach(function(childOrdinal) {
      var isSelected = childOrdinal === selectedChild;
      var card = element('button', 'animation-weapon-card' +
        (isSelected ? ' selected' : ''));
      card.type = 'button';
      card.setAttribute('aria-pressed', isSelected ? 'true' : 'false');
      var cardHeading = element('span', 'animation-card-selection-row');
      cardHeading.appendChild(element('strong', '', 'Weapon sprite ' + (childOrdinal + 1)));
      if (isSelected) cardHeading.appendChild(badge('Selected', 'selected'));
      card.appendChild(cardHeading);
      var canvas = element('canvas', 'animation-weapon-thumbnail');
      paintPixels(canvas, source.sprite.width, source.sprite.height,
        childPixels(source, state.animations, childOrdinal), 4);
      card.appendChild(canvas);
      var weaponItems = weaponItemsForChild(animation, childOrdinal,
        source.sprite.childCount);
      var itemName = element('span', 'animation-weapon-item-name',
        weaponItems.direct.length
          ? weaponItems.direct.map(function(item) { return item.name; }).join(' / ')
          : 'Unmapped ' + weaponItems.familyLabel + ' sprite');
      if (weaponItems.direct.length) {
        itemName.title = weaponItems.direct.map(function(item) {
          return item.name + ' (item ' + M.hex(item.itemId, 2) + ')';
        }).join('\n');
      }
      card.appendChild(itemName);
      if (weaponItems.fallback.length) {
        card.appendChild(element('small', 'animation-weapon-fallback',
          readableItemNames(weaponItems.fallback) +
          (weaponItems.fallback.length === 1 ? ' uses' : ' use') +
          ' this sprite as a fallback'));
      }
      if ((animation.spec.retailMappedWeaponOrdinals || [])
          .indexOf(childOrdinal) < 0) {
        var originalPixels = childPixels(source, state.animations, childOrdinal);
        var originalVisible = false;
        for (var alphaOffset = 3; alphaOffset < originalPixels.length;
          alphaOffset += 4) {
          if (originalPixels[alphaOffset]) { originalVisible = true; break; }
        }
        card.appendChild(element('small', 'animation-weapon-unmapped',
          originalVisible
            ? 'No vanilla ' + weaponItems.familyLabel + ' selects this sprite'
            : 'No vanilla ' + weaponItems.familyLabel +
              ' selects this sprite; empty in ROM'));
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

  function openLayerRotateModal(state, rom, separation, animation, frame,
      layer, ui, options, rerender) {
    if (!separation || !animation || !frame || !layer) return;
    var source = animation.artByKey[layer.sourceKey];
    if (!source || !source.editable || source.formatKind !== 'indexed-ci8') {
      notify(options, 'Layer rotation blocked: only editable indexed sprite layers can be rotated.');
      return;
    }
    var childOrdinal = selectedChildOrdinal(layer, source, ui);
    var pixels = M.currentEdit(state.animations, source.key, childOrdinal);
    var palette = M.childPalette(source, childOrdinal);
    var angle = 0, currentResult = null, scheduledFrame = null;
    var overlay = element('div',
      'error-modal-overlay animation-layer-transform-overlay');
    var modal = element('div',
      'error-modal animation-layer-transform-modal animation-layer-rotate-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'animation-layer-rotate-title');
    overlay.appendChild(modal);

    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'Rotate Sprite Layer');
    title.id = 'animation-layer-rotate-title';
    header.appendChild(title);
    var closeButton = button('\u00D7', 'error-modal-close', close);
    closeButton.setAttribute('aria-label', 'Cancel sprite layer rotation');
    header.appendChild(closeButton); modal.appendChild(header);

    var body = element('div',
      'error-modal-body animation-layer-transform-body');
    body.appendChild(element('p', 'animation-layer-transform-intro',
      'Choose any whole-degree angle. Positive values turn right; negative values turn left.'));
    if (source.weaponSelectable && source.sprite.childCount > 1) {
      body.appendChild(element('p', 'animation-layer-transform-equipment',
        'The preview shows the selected weapon. The rotation applies to all ' +
        source.sprite.childCount + ' equipped-item appearances in this pose.'));
    }
    var layout = element('div', 'animation-layer-rotate-layout');
    var previewPanel = element('section', 'animation-layer-transform-preview-panel');
    previewPanel.appendChild(element('h3', '', 'Preview'));
    var previewHost = element('div', 'animation-layer-transform-preview-host');
    previewPanel.appendChild(previewHost);
    var previewStatus = element('p', 'animation-layer-transform-status');
    previewStatus.setAttribute('aria-live', 'polite');
    previewPanel.appendChild(previewStatus); layout.appendChild(previewPanel);

    var controls = element('section', 'animation-layer-rotate-controls');
    var angleLabel = element('label', 'animation-layer-rotate-angle');
    angleLabel.appendChild(element('span', '', 'Angle'));
    var angleInput = element('input');
    angleInput.type = 'number'; angleInput.min = '-180'; angleInput.max = '180';
    angleInput.step = '1'; angleInput.value = '0';
    angleInput.setAttribute('inputmode', 'numeric');
    angleLabel.appendChild(angleInput); controls.appendChild(angleLabel);
    var angleSlider = element('input', 'animation-layer-rotate-slider');
    angleSlider.type = 'range'; angleSlider.min = '-180'; angleSlider.max = '180';
    angleSlider.step = '1'; angleSlider.value = '0';
    angleSlider.setAttribute('aria-label', 'Rotation angle in degrees');
    controls.appendChild(angleSlider);
    var shortcuts = element('div', 'animation-layer-rotate-shortcuts');
    [-90, -15, -1, 1, 15, 90].forEach(function(delta) {
      shortcuts.appendChild(button((delta > 0 ? '+' : '−') +
        Math.abs(delta) + '°', 'btn-secondary', function() {
        setAngle(Math.max(-180, Math.min(180, angle + delta)));
      }));
    });
    controls.appendChild(shortcuts);
    controls.appendChild(element('small', '',
      'Rotation uses nearest-neighbor pixels and expands the sprite bounds as needed.'));
    layout.appendChild(controls); body.appendChild(layout); modal.appendChild(body);

    var footer = element('div',
      'error-modal-footer animation-layer-transform-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var applyButton = button('Rotate Layer', 'error-modal-ok', applyRotation);
    applyButton.disabled = true;
    footer.appendChild(applyButton); modal.appendChild(footer);

    function rgbaForResult(result) {
      var rgba = new Uint8ClampedArray(
        result.width * result.height * 4);
      for (var pixel = 0; pixel < result.indices.length; pixel++) {
        var rgb = wordRgb(palette[result.indices[pixel]]);
        var offset = pixel * 4;
        rgba[offset] = rgb[0]; rgba[offset + 1] = rgb[1];
        rgba[offset + 2] = rgb[2];
        rgba[offset + 3] = result.intensity[pixel] * 17;
      }
      return rgba;
    }
    function readAngle(value) {
      value = Number(value);
      return Number.isInteger(value) && value >= -180 && value <= 180
        ? value : null;
    }
    function setAngle(value) {
      angle = value;
      angleInput.value = String(value);
      angleSlider.value = String(value);
      schedulePreview();
    }
    function preview() {
      scheduledFrame = null;
      try {
        currentResult = OB64.animationSequences.rotateIndexedPixels(
          pixels.indices, pixels.intensity,
          source.sprite.width, source.sprite.height, angle);
        previewHost.innerHTML = '';
        var scale = Math.max(1, Math.min(6,
          Math.floor(300 / Math.max(currentResult.width, currentResult.height))));
        var canvas = element('canvas', 'animation-layer-transform-preview-canvas');
        paintPixels(canvas, currentResult.width, currentResult.height,
          rgbaForResult(currentResult), scale);
        previewHost.appendChild(canvas);
        previewStatus.textContent = angle + '° · ' + currentResult.width + '×' +
          currentResult.height + ' pixels';
        previewStatus.classList.remove('blocked');
        applyButton.disabled = angle === 0;
      } catch (error) {
        currentResult = null; previewHost.innerHTML = '';
        previewStatus.textContent = 'Rotation blocked: ' + error.message;
        previewStatus.classList.add('blocked'); applyButton.disabled = true;
      }
    }
    function schedulePreview() {
      previewStatus.textContent = 'Preparing preview…'; applyButton.disabled = true;
      if (scheduledFrame !== null) window.cancelAnimationFrame(scheduledFrame);
      scheduledFrame = window.requestAnimationFrame(preview);
    }
    angleInput.addEventListener('input', function() {
      var value = readAngle(angleInput.value);
      if (value === null) {
        if (scheduledFrame !== null) window.cancelAnimationFrame(scheduledFrame);
        scheduledFrame = null; currentResult = null;
        previewStatus.textContent =
          'Angle must be a whole number from −180 through 180.';
        previewStatus.classList.add('blocked'); applyButton.disabled = true; return;
      }
      angle = value; angleSlider.value = String(value); schedulePreview();
    });
    angleSlider.addEventListener('input', function() {
      setAngle(Number(angleSlider.value));
    });
    function applyRotation() {
      if (!currentResult || angle === 0 || applyButton.disabled) return;
      try {
        var ordinal = OB64.animationSequences.rotateLayer(
          rom, separation, frame.sequenceIndex, layer.ordinal, angle);
        var updatedAnimation = separation.syntheticAnimation;
        var updatedFrame = updatedAnimation.frames[frame.sequenceIndex];
        var updatedLayer = updatedFrame.layers[ordinal];
        ui.animationLayer = ordinal;
        selectLayer(state, updatedAnimation, updatedFrame, updatedLayer, ui);
        changed(options);
        notify(options, 'Rotated the selected sprite layer ' + Math.abs(angle) +
          '° ' + (angle > 0 ? 'right' : 'left') +
          (source.weaponSelectable
            ? ' across all ' + source.sprite.childCount + ' weapon appearances.'
            : '.'));
        close(); rerender();
      } catch (error) {
        notify(options, 'Layer rotation blocked: ' + error.message);
      }
    }
    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    function close() {
      if (scheduledFrame !== null) window.cancelAnimationFrame(scheduledFrame);
      scheduledFrame = null;
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escapeHandler);
    }
    var escapeHandler = function(event) { if (event.key === 'Escape') close(); };
    document.addEventListener('keydown', escapeHandler);
    document.body.appendChild(overlay);
    angleInput.focus(); angleInput.select(); schedulePreview();
  }

  function openLayerResizeModal(state, rom, separation, animation, frame,
      layer, ui, options, rerender) {
    if (!separation || !animation || !frame || !layer) return;
    var source = animation.artByKey[layer.sourceKey];
    if (!source || !source.editable || source.formatKind !== 'indexed-ci8') {
      notify(options, 'Layer resize blocked: only editable indexed sprite layers can be resized.');
      return;
    }
    var currentWidth = source.sprite.width;
    var currentHeight = source.sprite.height;
    var overlay = element('div',
      'error-modal-overlay animation-layer-resize-overlay');
    var modal = element('div', 'error-modal animation-layer-resize-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'animation-layer-resize-title');
    overlay.appendChild(modal);

    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'Resize Sprite Layer');
    title.id = 'animation-layer-resize-title';
    header.appendChild(title);
    var closeButton = button('\u00D7', 'error-modal-close', close);
    closeButton.setAttribute('aria-label', 'Cancel sprite layer resize');
    header.appendChild(closeButton); modal.appendChild(header);

    var body = element('div',
      'error-modal-body animation-layer-resize-body');
    body.appendChild(element('p', 'animation-layer-resize-intro',
      'Current size: ' + currentWidth + '\u00D7' + currentHeight +
      ' pixels. The editor uses nearest-neighbor resizing and keeps the layer centered.'));
    if (source.weaponSelectable && source.sprite.childCount > 1) {
      body.appendChild(element('p', 'animation-layer-resize-equipment',
        'This weapon pose contains ' + source.sprite.childCount +
        ' equipped-item appearances. The resize applies to all of them.'));
    }
    var fields = element('div', 'animation-layer-resize-fields');
    function dimensionField(labelText, value) {
      var label = element('label', 'animation-layer-resize-field');
      label.appendChild(element('span', '', labelText));
      var input = element('input');
      input.type = 'number'; input.min = '1'; input.max = '512'; input.step = '1';
      input.value = String(value);
      input.setAttribute('inputmode', 'numeric');
      label.appendChild(input); fields.appendChild(label);
      return input;
    }
    var widthInput = dimensionField('Width', currentWidth);
    var heightInput = dimensionField('Height', currentHeight);
    body.appendChild(fields);
    var proportionLabel = element('label',
      'animation-layer-resize-proportions');
    var proportions = element('input'); proportions.type = 'checkbox';
    proportions.checked = true;
    proportionLabel.appendChild(proportions);
    proportionLabel.appendChild(document.createTextNode(' Keep proportions'));
    body.appendChild(proportionLabel);
    var status = element('p', 'animation-layer-resize-status');
    status.setAttribute('aria-live', 'polite'); body.appendChild(status);
    modal.appendChild(body);

    var footer = element('div',
      'error-modal-footer animation-layer-resize-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var applyButton = button('Resize Layer', 'error-modal-ok', applyResize);
    footer.appendChild(applyButton); modal.appendChild(footer);
    var syncing = false;
    function readDimension(input) {
      var value = Number(input.value);
      return Number.isInteger(value) && value >= 1 && value <= 512
        ? value : null;
    }
    function updateStatus() {
      var width = readDimension(widthInput);
      var height = readDimension(heightInput);
      if (width === null || height === null) {
        status.textContent =
          'Width and height must be whole numbers from 1 through 512.';
        status.classList.add('blocked'); applyButton.disabled = true; return;
      }
      if (width === currentWidth && height === currentHeight) {
        status.textContent = 'Choose a different size.';
        status.classList.remove('blocked'); applyButton.disabled = true; return;
      }
      status.textContent = 'New size: ' + width + '\u00D7' + height + ' pixels.';
      status.classList.remove('blocked'); applyButton.disabled = false;
    }
    function pairedDimension(value, numerator, denominator) {
      return Math.max(1, Math.min(512,
        Math.round(value * numerator / denominator)));
    }
    widthInput.addEventListener('input', function() {
      if (!syncing && proportions.checked) {
        var width = readDimension(widthInput);
        if (width !== null) {
          syncing = true;
          heightInput.value = String(pairedDimension(
            width, currentHeight, currentWidth));
          syncing = false;
        }
      }
      updateStatus();
    });
    heightInput.addEventListener('input', function() {
      if (!syncing && proportions.checked) {
        var height = readDimension(heightInput);
        if (height !== null) {
          syncing = true;
          widthInput.value = String(pairedDimension(
            height, currentWidth, currentHeight));
          syncing = false;
        }
      }
      updateStatus();
    });
    proportions.addEventListener('change', updateStatus);
    function applyResize() {
      var width = readDimension(widthInput);
      var height = readDimension(heightInput);
      if (width === null || height === null || applyButton.disabled) return;
      try {
        var ordinal = OB64.animationSequences.resizeLayer(
          rom, separation, frame.sequenceIndex, layer.ordinal, width, height);
        var updatedAnimation = separation.syntheticAnimation;
        var updatedFrame = updatedAnimation.frames[frame.sequenceIndex];
        var updatedLayer = updatedFrame.layers[ordinal];
        ui.animationLayer = ordinal;
        selectLayer(state, updatedAnimation, updatedFrame, updatedLayer, ui);
        changed(options);
        notify(options, 'Resized the selected sprite layer from ' +
          currentWidth + '\u00D7' + currentHeight + ' to ' + width + '\u00D7' +
          height + ' pixels' + (source.weaponSelectable
            ? ' across all ' + source.sprite.childCount + ' weapon appearances.'
            : '.'));
        close(); rerender();
      } catch (error) {
        notify(options, 'Layer resize blocked: ' + error.message);
      }
    }
    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    function close() {
      if (overlay.parentNode) overlay.parentNode.removeChild(overlay);
      document.removeEventListener('keydown', escapeHandler);
    }
    var escapeHandler = function(event) { if (event.key === 'Escape') close(); };
    document.addEventListener('keydown', escapeHandler);
    document.body.appendChild(overlay);
    widthInput.focus(); widthInput.select(); updateStatus();
  }

  function layerList(state, rom, animation, frame, selected, ui, options, rerender) {
    var section = element('section', 'animation-layer-section');
    var separation = animation.spec.separatedCopy && OB64.animationSequences
      ? OB64.animationSequences.separationFor(animation, rom.animationSequences) : null;
    var heading = element('div', 'animation-section-heading animation-layer-heading');
    heading.appendChild(element('h3', '', 'Frame layers'));
    var actions = element('div', 'animation-layer-heading-actions');
    var addLayer = button('Add Layer', 'btn-secondary', function() {
      try {
        var ordinal = OB64.animationSequences.addBlankLayer(
          rom, separation, frame.sequenceIndex, selected.ordinal);
        var updatedAnimation = separation.syntheticAnimation;
        var updatedFrame = updatedAnimation.frames[frame.sequenceIndex];
        ui.animationLayer = ordinal;
        selectLayer(state, updatedAnimation, updatedFrame,
          updatedFrame.layers[ordinal], ui);
        changed(options);
        notify(options, 'Added a blank full-frame sprite layer.');
        rerender();
      } catch (error) {
        notify(options, 'Layer addition blocked: ' + error.message);
      }
    });
    var copyLayer = button('Copy From…', 'btn-secondary', function() {
      openSpriteCopyModal(state, rom, separation, animation, frame, selected,
        'copy-layer', ui, options, rerender);
    });
    var removeLayer = button('Remove Layer',
      'btn-secondary animation-remove-action', function() {
        try {
          var nextOrdinal = OB64.animationSequences.removeLayer(
            rom, separation, frame.sequenceIndex, selected.ordinal);
          var updatedAnimation = separation.syntheticAnimation;
          var updatedFrame = updatedAnimation.frames[frame.sequenceIndex];
          ui.animationLayer = nextOrdinal;
          selectLayer(state, updatedAnimation, updatedFrame,
            updatedFrame.layers[nextOrdinal], ui);
          changed(options); notify(options, 'Removed the selected sprite layer.');
          rerender();
        } catch (error) {
          notify(options, 'Layer removal blocked: ' + error.message);
        }
      });
    addLayer.disabled = copyLayer.disabled = !separation;
    removeLayer.disabled = !separation || frame.layers.length <= 1;
    addLayer.title = separation
      ? 'Add a transparent full-frame layer using the selected layer palette.'
      : 'Create a separated private sequence before adding layers.';
    copyLayer.title = separation
      ? 'Replace this sprite from any class, sequence, frame, and layer.'
      : 'Create a separated private sequence before replacing a layer source.';
    removeLayer.title = !separation
      ? 'Create a separated private sequence before removing a layer.'
      : (frame.layers.length <= 1
        ? 'A frame must retain at least one sprite layer.'
        : 'Remove the selected sprite layer from this private frame.');
    actions.appendChild(addLayer); actions.appendChild(copyLayer);
    actions.appendChild(removeLayer);
    heading.appendChild(actions); section.appendChild(heading);
    var list = element('div', 'animation-layer-list');
    list.setAttribute('data-art-scroll-key', 'animations:layers:' +
      animation.key + ':' + frame.sequenceIndex);
    var draggedOrdinal = null, dragged = false;
    frame.layers.forEach(function(layer) {
      var source = animation.artByKey[layer.sourceKey];
      var isSelected = layer.ordinal === selected.ordinal;
      var card = element('button', 'animation-layer-card' +
        (isSelected ? ' selected' : ''));
      card.type = 'button';
      card.draggable = !!separation;
      card.setAttribute('aria-pressed', isSelected ? 'true' : 'false');
      var title = element('span', 'animation-layer-title');
      if (separation) {
        var handle = element('span', 'animation-layer-drag-handle', '☰');
        handle.setAttribute('aria-hidden', 'true');
        handle.title = 'Drag to change layer order';
        title.appendChild(handle);
      }
      title.appendChild(element('strong', '', layerDisplayLabel(layer, source)));
      if (isSelected) title.appendChild(badge('Selected', 'selected'));
      if (!source.editable) title.appendChild(badge('Read-only', 'locked'));
      var editedChildren = M.sourceEditCount(state.animations, layer.sourceKey);
      if (editedChildren) title.appendChild(badge(editedChildren + ' sprite' +
        (editedChildren === 1 ? '' : 's') + ' edited', 'edited'));
      var visibleChild = selectedChildOrdinal(layer, source, ui);
      if (!M.childHasVisiblePixels(source, visibleChild)) {
        title.appendChild(badge('Empty in ROM', 'empty'));
      }
      card.appendChild(title);
      card.addEventListener('click', function() {
        if (dragged) { dragged = false; return; }
        selectLayer(state, animation, frame, layer, ui); rerender();
      });
      if (separation) {
        card.addEventListener('dragstart', function(event) {
          draggedOrdinal = layer.ordinal; dragged = true;
          card.classList.add('dragging');
          if (event.dataTransfer) {
            event.dataTransfer.effectAllowed = 'move';
            event.dataTransfer.setData('text/plain', String(layer.ordinal));
          }
        });
        card.addEventListener('dragover', function(event) {
          event.preventDefault();
          card.classList.add('drag-target');
          if (event.dataTransfer) event.dataTransfer.dropEffect = 'move';
        });
        card.addEventListener('dragleave', function() {
          card.classList.remove('drag-target');
        });
        card.addEventListener('drop', function(event) {
          event.preventDefault(); card.classList.remove('drag-target');
          if (!Number.isInteger(draggedOrdinal)) return;
          try {
            var selectedWasDragged = selected.ordinal === draggedOrdinal;
            var newOrdinal = OB64.animationSequences.moveLayer(rom, separation,
              frame.sequenceIndex, draggedOrdinal, layer.ordinal);
            if (selectedWasDragged) ui.animationLayer = newOrdinal;
            else {
              var selectedNow = frame.layers.indexOf(selected);
              ui.animationLayer = selectedNow >= 0 ? selectedNow : 0;
            }
            changed(options); notify(options, 'Updated frame layer order.');
            rerender();
          } catch (error) {
            notify(options, 'Layer reorder blocked: ' + error.message);
          }
        });
        card.addEventListener('dragend', function() {
          draggedOrdinal = null;
          Array.prototype.forEach.call(list.children, function(node) {
            node.classList.remove('dragging', 'drag-target');
          });
          setTimeout(function() { dragged = false; }, 0);
        });
      }
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

  function animationBrushIndices(width, height, x, y, size) {
    width = Math.max(1, Math.floor(Number(width) || 1));
    height = Math.max(1, Math.floor(Number(height) || 1));
    size = Math.max(1, Math.min(16, Math.floor(Number(size) || 1)));
    x = Math.floor(Number(x) || 0);
    y = Math.floor(Number(y) || 0);
    var startX = x - Math.floor(size / 2);
    var startY = y - Math.floor(size / 2);
    var output = [];
    for (var brushY = startY; brushY < startY + size; brushY++) {
      if (brushY < 0 || brushY >= height) continue;
      for (var brushX = startX; brushX < startX + size; brushX++) {
        if (brushX < 0 || brushX >= width) continue;
        output.push(brushY * width + brushX);
      }
    }
    return output;
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

  function rawNativeCoordinate(canvas, event, animation) {
    var rect = canvas.getBoundingClientRect();
    return {
      x: Math.floor((event.clientX - rect.left) * animation.canvas.width / rect.width),
      y: Math.floor((event.clientY - rect.top) * animation.canvas.height / rect.height)
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
      var childOrdinal = selectedChildOrdinal(layer, source, ui);
      var child = M.displayChild(state.animations, layer.sourceKey, childOrdinal);
      var sourcePixel = local.y * source.sprite.width + local.x;
      if ((source.editable ? child.intensity[sourcePixel] : child.alpha[sourcePixel])) {
        return layer;
      }
    }
    return null;
  }

  function installEditing(canvas, state, rom, separation, animation, frame, layer,
      ui, options, rerender) {
    var drawing = false, moving = false, working = null, changedPixels = false;
    var moveStart = null, lastPaintPoint = null;
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = selectedChildOrdinal(layer, source, ui);
    canvas.tabIndex = 0;
    if (ui.animationTool === 'move' && separation) {
      canvas.classList.add('animation-edit-canvas-move');
    }
    function redraw() {
      var overrides = {};
      if (working) overrides[M.childKey(layer.sourceKey, childOrdinal)] = working;
      drawFrame(canvas, animation, frame, state.animations, 8, layer, overrides,
        source.editable,
        weaponChildForAnimation(ui, animation));
    }
    function selectedIntensity() {
      return ui.animationTool === 'eraser' ? 0 : ui.animationIntensity;
    }
    function applyPoint(local) {
      if (ui.animationTool !== 'pencil' && ui.animationTool !== 'eraser') return;
      var wantedIndex = ui.animationPaletteIndex, wantedIntensity = selectedIntensity();
      animationBrushIndices(source.sprite.width, source.sprite.height,
        local.x, local.y, ui.animationBrushSize).forEach(function(pixelIndex) {
          if (working.indices[pixelIndex] !== wantedIndex ||
              working.intensity[pixelIndex] !== wantedIntensity) {
            working.indices[pixelIndex] = wantedIndex;
            working.intensity[pixelIndex] = wantedIntensity;
            changedPixels = true;
          }
        });
    }
    function applyLine(from, to) {
      if (!from) { applyPoint(to); return; }
      var x = from.x, y = from.y;
      var dx = Math.abs(to.x - x), sx = x < to.x ? 1 : -1;
      var dy = -Math.abs(to.y - y), sy = y < to.y ? 1 : -1;
      var error = dx + dy;
      while (true) {
        applyPoint({ x: x, y: y, index: y * layer.width + x });
        if (x === to.x && y === to.y) break;
        var doubled = 2 * error;
        if (doubled >= dy) { error += dy; x += sx; }
        if (doubled <= dx) { error += dx; y += sy; }
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
      if (ui.animationTool === 'move' && separation) {
        moving = true;
        moveStart = {
          pointer: rawNativeCoordinate(canvas, event, animation),
          x: layer.drawOffsetX, y: layer.drawOffsetY
        };
        canvas.setPointerCapture(event.pointerId);
        event.preventDefault();
        return;
      }
      if (!source.editable) return;
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
      lastPaintPoint = local;
      canvas.setPointerCapture(event.pointerId); applyPoint(local); redraw();
    });
    canvas.addEventListener('pointermove', function(event) {
      if (moving) {
        var point = rawNativeCoordinate(canvas, event, animation);
        layer.drawOffsetX = Math.max(-0x8000, Math.min(0x7FFF,
          moveStart.x + point.x - moveStart.pointer.x));
        layer.drawOffsetY = Math.max(-0x8000, Math.min(0x7FFF,
          moveStart.y + point.y - moveStart.pointer.y));
        redraw(); event.preventDefault(); return;
      }
      if (!drawing) return;
      var local = localCoordinate(nativeCoordinate(canvas, event, animation), animation, layer);
      if (!local) { lastPaintPoint = null; return; }
      applyLine(lastPaintPoint, local); lastPaintPoint = local; redraw();
    });
    function finish(event) {
      if (moving) {
        moving = false;
        var wantedX = layer.drawOffsetX, wantedY = layer.drawOffsetY;
        layer.drawOffsetX = moveStart.x; layer.drawOffsetY = moveStart.y;
        moveStart = null;
        if (event && canvas.hasPointerCapture(event.pointerId)) {
          canvas.releasePointerCapture(event.pointerId);
        }
        try {
          if (OB64.animationSequences.setLayerPosition(rom, separation,
              frame.sequenceIndex, layer.ordinal, wantedX, wantedY)) {
            changed(options);
            notify(options, 'Moved layer to X ' + wantedX + ', Y ' + wantedY + '.');
          }
          rerender();
        } catch (error) {
          notify(options, 'Layer move blocked: ' + error.message); rerender();
        }
        return;
      }
      if (!drawing) return;
      drawing = false;
      lastPaintPoint = null;
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

  function appendPreviewBackgroundButton(bar, ui, rerender) {
    var whitePreview = ui.transparentPreviewBackground === 'white';
    var previewToggle = button('Transparent Preview: ' +
      (whitePreview ? 'White' : 'Checkerboard'), 'btn-secondary', function() {
      ui.transparentPreviewBackground = whitePreview ? 'checkerboard' : 'white';
      rerender();
    });
    previewToggle.setAttribute('aria-pressed', whitePreview ? 'true' : 'false');
    previewToggle.setAttribute('title',
      'Changes transparent pixels in editor previews only. Exports and Project data do not change.');
    bar.appendChild(previewToggle);
  }

  function toolbar(state, rom, animation, frame, layer, separation,
      ui, options, rerender) {
    var bar = element('div', 'art-toolbox animation-toolbox');
    var source = state.animations.artByKey[layer.sourceKey];
    if (source.editable) {
      [['pencil', 'Pencil'], ['eraser', 'Erase'], ['fill', 'Fill'],
        ['eyedropper', 'Eyedropper'], ['replace', 'Replace Color']].forEach(function(row) {
        bar.appendChild(button(row[1], ui.animationTool === row[0] ? 'active' : '', function() {
          ui.animationTool = row[0]; rerender();
        }));
      });
      var brushControl = element('label', 'animation-brush-size-control');
      var brushLabel = element('span', '', 'Brush ' + ui.animationBrushSize + ' px');
      var brushSize = element('input');
      brushSize.type = 'range';
      brushSize.min = '1';
      brushSize.max = '16';
      brushSize.step = '1';
      brushSize.value = String(ui.animationBrushSize);
      brushSize.title = 'Square pencil and eraser size';
      brushSize.setAttribute('aria-label', 'Brush size in pixels');
      brushSize.addEventListener('input', function() {
        ui.animationBrushSize = Math.max(1, Math.min(16,
          Math.floor(Number(brushSize.value) || 1)));
        brushLabel.textContent = 'Brush ' + ui.animationBrushSize + ' px';
      });
      brushControl.appendChild(brushLabel);
      brushControl.appendChild(brushSize);
      bar.appendChild(brushControl);
    }
    if (separation) {
      bar.appendChild(button('Move Layer',
        ui.animationTool === 'move' ? 'active animation-move-tool' :
          'animation-move-tool', function() {
          ui.animationTool = 'move'; rerender();
        }));
    }
    if (!source.editable) {
      appendPreviewBackgroundButton(bar, ui, rerender);
      return bar;
    }
    var transformAvailable = !!separation &&
      source.formatKind === 'indexed-ci8';
    var transformTitle = !separation
      ? 'Create a separated private sequence before transforming a layer.'
      : (source.formatKind !== 'indexed-ci8'
        ? 'Only editable indexed sprite layers can be transformed.'
        : 'Only this frame layer changes. Other frames retain their current sprite.');
    var rotate = button('Rotate…', 'btn-secondary animation-transform-tool',
      function() {
        openLayerRotateModal(state, rom, separation, animation, frame, layer,
          ui, options, rerender);
      });
    rotate.disabled = !transformAvailable;
    rotate.title = 'Rotate by any whole-degree angle. ' + transformTitle;
    bar.appendChild(rotate);
    var resize = button('Resize…', 'btn-secondary animation-transform-tool',
      function() {
        openLayerResizeModal(state, rom, separation, animation, frame, layer,
          ui, options, rerender);
      });
    resize.disabled = !transformAvailable;
    resize.title = 'Resize with nearest-neighbor pixels. ' + transformTitle;
    bar.appendChild(resize);
    var childOrdinal = selectedChildOrdinal(layer, source, ui);
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
    appendPreviewBackgroundButton(bar, ui, rerender);
    return bar;
  }

  function palettePanel(source, childOrdinal, ui, rerender) {
    var panel = element('section', 'animation-palette-panel');
    if (!source.editable) {
      panel.appendChild(element('h3', '', 'Colors'));
      return panel;
    }
    var palette = M.childPalette(source, childOrdinal);
    panel.appendChild(element('h3', '', 'Colors'));
    var selectedWord = palette[ui.animationPaletteIndex];
    var selected = element('div', 'animation-selected-color');
    var swatch = element('span', 'art-selected-swatch');
    selected.appendChild(swatch);
    selected.appendChild(element('span', '', 'Index ' + ui.animationPaletteIndex +
      ' · RGB555 ' + M.hex(selectedWord, 4)));
    panel.appendChild(selected);
    var grid = element('div', 'animation-palette-grid');
    var paletteChoices = [];
    var orderedIndices = paletteHueOrder(palette);
    for (var index = 0; index < orderedIndices.length; index++) {
      (function(paletteIndex) {
        var word = palette[paletteIndex];
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
    return panel;
  }

  function framePngCanvas(animation, frame, state, weaponChildOrdinal) {
    var canvas = element('canvas');
    drawFrame(canvas, animation, frame, state.animations, 1, null, null, false,
      weaponChildOrdinal, 'transparent');
    return canvas;
  }

  function importLibrarySpritePixels(animationState, targetSource,
      childOrdinal, sourceAsset) {
    var width = targetSource.sprite.width;
    var height = targetSource.sprite.height;
    var rgba = OB64.spriteLibrary.nearestResize(sourceAsset.rgba,
      sourceAsset.width, sourceAsset.height, width, height);
    var palette = M.childPalette(targetSource, childOrdinal);
    var paletteRgb = Array.prototype.map.call(palette, wordRgb);
    var nearest = {};
    var indices = new Uint8Array(width * height);
    var intensity = new Uint8Array(width * height);
    for (var pixel = 0; pixel < width * height; pixel++) {
      var offset = pixel * 4;
      intensity[pixel] = Math.max(0, Math.min(15,
        Math.round(rgba[offset + 3] * 15 / 255)));
      var key = rgba[offset] + ',' + rgba[offset + 1] + ',' + rgba[offset + 2];
      var paletteIndex = nearest[key];
      if (!Number.isInteger(paletteIndex)) {
        var bestDistance = Infinity;
        paletteIndex = 0;
        for (var candidate = 0; candidate < paletteRgb.length; candidate++) {
          var red = paletteRgb[candidate][0] - rgba[offset];
          var green = paletteRgb[candidate][1] - rgba[offset + 1];
          var blue = paletteRgb[candidate][2] - rgba[offset + 2];
          var distance = red * red + green * green + blue * blue;
          if (distance < bestDistance) {
            bestDistance = distance;
            paletteIndex = candidate;
            if (!distance) break;
          }
        }
        nearest[key] = paletteIndex;
      }
      indices[pixel] = paletteIndex;
    }
    return M.setEdit(animationState, targetSource.key, childOrdinal,
      indices, intensity);
  }

  function downloadFrame(animation, frame, state, weaponChildOrdinal) {
    var canvas = framePngCanvas(animation, frame, state, weaponChildOrdinal);
    return new Promise(function(resolve, reject) {
      canvas.toBlob(function(blob) {
        if (!blob) {
          reject(new Error('The browser could not encode the animation frame as PNG.'));
          return;
        }
        var url = URL.createObjectURL(blob), anchor = document.createElement('a');
        var filename = animationDownloadStem(animation) + '-frame-' +
          String(frame.sequenceIndex + 1).padStart(2, '0') + '.png';
        anchor.href = url;
        anchor.download = filename;
        document.body.appendChild(anchor); anchor.click(); anchor.remove();
        setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
        resolve(filename);
      }, 'image/png');
    });
  }

  function animationExportFrames(timeline) {
    var frames = [];
    (timeline && timeline.entries || []).forEach(function(entry) {
      var ticks = Math.max(1, Math.round(Number(entry.ticks) || 0));
      for (var tick = 0; tick < ticks; tick++) frames.push(entry);
    });
    return frames;
  }

  function animationWebmMimeType(Recorder) {
    var choices = [
      'video/webm;codecs=vp9',
      'video/webm;codecs=vp8',
      'video/webm'
    ];
    if (!Recorder || typeof Recorder !== 'function') return '';
    if (typeof Recorder.isTypeSupported !== 'function') return 'video/webm';
    for (var index = 0; index < choices.length; index++) {
      if (Recorder.isTypeSupported(choices[index])) return choices[index];
    }
    return '';
  }

  function animationWebmSupported() {
    if (typeof window === 'undefined' || typeof document === 'undefined') return false;
    var canvas = document.createElement('canvas');
    return !!animationWebmMimeType(window.MediaRecorder) &&
      typeof canvas.captureStream === 'function';
  }

  function animationExportClock() {
    return typeof performance !== 'undefined' &&
      typeof performance.now === 'function' ? performance.now() : Date.now();
  }

  function waitForAnimationExport(deadline) {
    return new Promise(function(resolve) {
      setTimeout(resolve, Math.max(0, deadline - animationExportClock()));
    });
  }

  function animationDownloadStem(animation) {
    var stem = String(animation && animation.key || 'combat-animation')
      .replace(/[^a-z0-9._-]+/gi, '-')
      .replace(/^-+|-+$/g, '');
    return stem || 'combat-animation';
  }

  function animationWebmSurfaces(state, animation, timeline, weaponChildOrdinal) {
    return animationPreviewSurfaces(state, animation, timeline,
      weaponChildOrdinal, 'transparent');
  }

  async function downloadAnimationWebm(animation, state, weaponChildOrdinal) {
    if (!animationWebmSupported()) {
      throw new Error('This browser cannot record the animation preview as WebM.');
    }
    var timeline = animationPreviewTimeline(animation);
    var frames = animationExportFrames(timeline);
    if (!frames.length) throw new Error('This sequence has no timed frames to export.');
    var surfaces = animationWebmSurfaces(state, animation, timeline,
      weaponChildOrdinal);
    var firstSurface = surfaces[frames[0].frameIndex];
    if (!firstSurface) throw new Error('The first animation frame could not be rendered.');

    var canvas = document.createElement('canvas');
    canvas.width = firstSurface.width;
    canvas.height = firstSurface.height;
    var context = canvas.getContext('2d', { alpha: true });
    context.imageSmoothingEnabled = false;
    context.drawImage(firstSurface, 0, 0);

    var Recorder = window.MediaRecorder;
    var mimeType = animationWebmMimeType(Recorder);
    var stream = canvas.captureStream(0);
    var track = stream.getVideoTracks()[0];
    var manualFrames = track && typeof track.requestFrame === 'function';
    if (!manualFrames) {
      stream.getTracks().forEach(function(row) { row.stop(); });
      stream = canvas.captureStream(30);
      track = stream.getVideoTracks()[0];
    }
    var recorder = new Recorder(stream, {
      mimeType: mimeType,
      videoBitsPerSecond: 6000000
    });
    var chunks = [];
    var completed = new Promise(function(resolve, reject) {
      recorder.addEventListener('dataavailable', function(event) {
        if (event.data && event.data.size) chunks.push(event.data);
      });
      recorder.addEventListener('error', function(event) {
        reject(event.error || new Error('The browser video recorder failed.'));
      });
      recorder.addEventListener('stop', function() {
        var type = recorder.mimeType || mimeType || 'video/webm';
        resolve(new Blob(chunks, { type: type }));
      });
    });

    try {
      recorder.start();
      await nextBrowserPaint();
      var frameDuration = 1000 / timeline.ticksPerSecond;
      var startedAt = animationExportClock();
      for (var index = 0; index < frames.length; index++) {
        var surface = surfaces[frames[index].frameIndex];
        context.clearRect(0, 0, canvas.width, canvas.height);
        context.drawImage(surface, 0, 0);
        if (manualFrames) track.requestFrame();
        await waitForAnimationExport(startedAt + (index + 1) * frameDuration);
      }
      recorder.stop();
      var blob = await completed;
      if (!blob.size) throw new Error('The browser produced an empty WebM file.');
      var url = URL.createObjectURL(blob);
      var anchor = document.createElement('a');
      var filename = animationDownloadStem(animation) + '-animation.webm';
      anchor.href = url;
      anchor.download = filename;
      document.body.appendChild(anchor);
      anchor.click();
      anchor.remove();
      setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
      return filename;
    } finally {
      if (recorder.state !== 'inactive') recorder.stop();
      stream.getTracks().forEach(function(row) { row.stop(); });
    }
  }

  function animationPreviewTimeline(animation) {
    var entries = [], cursor = 0;
    (animation && animation.frames || []).forEach(function(frame, frameIndex) {
      var ticks = Number(frame.ticks);
      if (!Number.isFinite(ticks) || ticks <= 0) return;
      entries.push({
        frame: frame,
        frameIndex: frameIndex,
        startTick: cursor,
        endTick: cursor + ticks,
        ticks: ticks
      });
      cursor += ticks;
    });
    if (!entries.length) {
      (animation && animation.frames || []).forEach(function(frame, frameIndex) {
        entries.push({
          frame: frame,
          frameIndex: frameIndex,
          startTick: frameIndex,
          endTick: frameIndex + 1,
          ticks: 1
        });
      });
      cursor = entries.length;
    }
    return { entries: entries, totalTicks: cursor, ticksPerSecond: 30 };
  }

  function animationPreviewFrameAtMs(timeline, elapsedMs) {
    if (!timeline || !timeline.entries || !timeline.entries.length ||
        !timeline.totalTicks) return null;
    elapsedMs = Number(elapsedMs);
    if (!Number.isFinite(elapsedMs) || elapsedMs < 0) elapsedMs = 0;
    var tick = (elapsedMs * timeline.ticksPerSecond / 1000) %
      timeline.totalTicks;
    return timeline.entries.find(function(entry) {
      return tick >= entry.startTick && tick < entry.endTick;
    }) || timeline.entries[timeline.entries.length - 1];
  }

  function animationPreviewStructureSignature(animation) {
    var parts = [animation.key, animation.canvas.width, animation.canvas.height];
    animation.frames.forEach(function(frame) {
      parts.push('f', frame.sequenceIndex, frame.token, frame.ticks);
      frame.layers.forEach(function(layer) {
        var metrics = layerPreviewMetrics(animation, layer);
        parts.push(layer.sourceKey, layer.selectedChildOrdinal,
          layer.drawOffsetX, layer.drawOffsetY, layer.width, layer.height);
        if (metrics) parts.push(metrics.left, metrics.top, metrics.width,
          metrics.height, metrics.flipX ? 1 : 0, metrics.flipY ? 1 : 0);
      });
    });
    return parts.join(':');
  }

  function animationPreviewSurfaces(state, animation, timeline,
      weaponChildOrdinal, backgroundMode) {
    var animationState = state.animations;
    var editRevision = Number(animationState.editRevision) || 0;
    var signature = animationPreviewStructureSignature(animation);
    backgroundMode = normalizePreviewBackground(backgroundMode === undefined
      ? activePreviewBackground : backgroundMode);
    var cached = animationPreviewSurfaceCache;
    if (cached && cached.animationState === animationState &&
        cached.signature === signature &&
        cached.weaponChildOrdinal === weaponChildOrdinal &&
        cached.backgroundMode === backgroundMode &&
        cached.editRevision === editRevision) return cached.surfaces;
    var surfaces = [];
    timeline.entries.forEach(function(entry) {
      var surface = document.createElement('canvas');
      drawFrame(surface, animation, entry.frame, animationState, 4, null, null,
        false, weaponChildOrdinal, backgroundMode);
      surfaces[entry.frameIndex] = surface;
    });
    animationPreviewSurfaceCache = {
      animationState: animationState,
      signature: signature,
      weaponChildOrdinal: weaponChildOrdinal,
      backgroundMode: backgroundMode,
      editRevision: editRevision,
      surfaces: surfaces
    };
    return surfaces;
  }

  function fullFramePreview(state, animation, frame, ui) {
    var preview = element('figure', 'animation-actual-preview');
    preview.appendChild(element('figcaption', '', 'Full frame'));
    var canvas = element('canvas');
    drawFrame(canvas, animation, frame, state.animations, 4, null, null,
      false, weaponChildForAnimation(ui, animation));
    preview.appendChild(canvas);
    return preview;
  }

  function animationSequencePreview(state, animation, ui, previewOptions) {
    previewOptions = previewOptions || {};
    var preview = element('figure', 'animation-sequence-preview' +
      (previewOptions.className ? ' ' + previewOptions.className : ''));
    preview.appendChild(element('figcaption', '', previewOptions.caption ||
      'Animation · 30 ticks/sec'));
    var canvas = element('canvas');
    canvas.setAttribute('aria-label', previewOptions.ariaLabel ||
      'Looping full animation preview');
    preview.appendChild(canvas);
    var status = element('span', 'animation-sequence-preview-status');
    preview.appendChild(status);
    var timeline = animationPreviewTimeline(animation);
    var weaponChildOrdinal = Number.isInteger(previewOptions.weaponChildOrdinal)
      ? previewOptions.weaponChildOrdinal : weaponChildForAnimation(ui, animation);
    var surfaces = animationPreviewSurfaces(state, animation, timeline,
      weaponChildOrdinal, activePreviewBackground);
    var context = canvas.getContext('2d');
    context.imageSmoothingEnabled = false;
    var displayedFrameIndex = -1;
    function show(entry) {
      if (!entry || entry.frameIndex === displayedFrameIndex) return;
      var surface = surfaces[entry.frameIndex];
      if (!surface) return;
      displayedFrameIndex = entry.frameIndex;
      if (canvas.width !== surface.width || canvas.height !== surface.height) {
        canvas.width = surface.width;
        canvas.height = surface.height;
        context = canvas.getContext('2d');
        context.imageSmoothingEnabled = false;
      }
      context.drawImage(surface, 0, 0);
      status.textContent = 'Frame ' + (entry.frame.sequenceIndex + 1) +
        ' · ' + entry.frame.ticks + ' ticks';
      status.setAttribute('data-animation-preview-frame',
        String(entry.frame.sequenceIndex));
    }
    show(timeline.entries[0]);
    if (timeline.entries.length > 1 && typeof window !== 'undefined' &&
        typeof window.requestAnimationFrame === 'function') {
      var startedAt = null;
      function advance(timestamp) {
        if (!canvas.isConnected) return;
        if (startedAt === null) startedAt = timestamp;
        show(animationPreviewFrameAtMs(timeline, timestamp - startedAt));
        window.requestAnimationFrame(advance);
      }
      window.requestAnimationFrame(advance);
    }
    return preview;
  }

  function openFrameImportModal(source, state, rom, separation,
      animation, frame, ui, options, rerender) {
    if (!separation || !animation || !frame) return;
    var targetWidth = animation.canvas.width;
    var targetHeight = animation.canvas.height;
    var equipmentCount = frame.layers.filter(function(layer) {
      var layerSource = animation.artByKey[layer.sourceKey];
      return layerSource && layerSource.sourceRole === 'equipment';
    }).length;
    var settings = {
      resizeMode: 'nearest', panX: 0.5, panY: 0.5,
      dither: false, keepEquipment: true
    };
    var currentResult = null, scheduledFrame = null;
    var overlay = element('div', 'error-modal-overlay art-import-overlay');
    var modal = element('div', 'error-modal art-import-modal animation-frame-import-modal');
    modal.setAttribute('role', 'dialog');
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'animation-frame-import-title');
    overlay.appendChild(modal);

    var header = element('div', 'error-modal-header');
    var title = element('h2', '', 'Prepare Animation Frame');
    title.id = 'animation-frame-import-title';
    header.appendChild(title);
    var closeButton = button('\u00D7', 'error-modal-close', close);
    closeButton.setAttribute('aria-label', 'Cancel animation frame import');
    header.appendChild(closeButton); modal.appendChild(header);

    var body = element('div', 'error-modal-body art-import-body');
    body.appendChild(element('p', 'art-import-intro', source.name + ' \u00B7 ' +
      source.format + ' \u00B7 ' + source.width + '\u00D7' + source.height +
      ' source pixels. The result is cropped and resized to the sequence canvas ' +
      targetWidth + '\u00D7' + targetHeight + '.'));
    var layout = element('div', 'art-import-layout');
    var previewPanel = element('section', 'art-import-preview-panel');
    previewPanel.appendChild(element('h3', '', 'Final Frame Preview'));
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

    var initialCrop = OB64.art.imageCropRect(source.width, source.height,
      targetWidth, targetHeight, settings.panX, settings.panY);
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
    cropSlider('Horizontal crop position', 'panX',
      initialCrop.horizontalPanAvailable);
    cropSlider('Vertical crop position', 'panY',
      initialCrop.verticalPanAvailable);

    var ditherWrap = element('div', 'art-import-dither');
    var ditherLabel = element('label', 'art-import-checkbox');
    var dither = element('input'); dither.type = 'checkbox';
    dither.addEventListener('change', function() {
      settings.dither = dither.checked; schedulePreview();
    });
    ditherLabel.appendChild(dither);
    ditherLabel.appendChild(document.createTextNode(' Ordered dithering'));
    ditherWrap.appendChild(ditherLabel);
    ditherWrap.appendChild(element('small', '',
      'Off by default. It can smooth gradients when the image exceeds 256 RGB555 colors.'));
    controls.appendChild(ditherWrap);

    var layerMode = element('label', 'art-import-control');
    layerMode.appendChild(element('span', '', 'Existing layers'));
    var layerSelect = element('select');
    var keepOption = element('option', '', 'Keep weapon layers separate (recommended)');
    keepOption.value = 'keep'; layerSelect.appendChild(keepOption);
    var flattenOption = element('option', '', 'Replace every layer (weapon is baked in)');
    flattenOption.value = 'replace'; layerSelect.appendChild(flattenOption);
    layerSelect.value = 'keep';
    layerSelect.addEventListener('change', function() {
      settings.keepEquipment = layerSelect.value === 'keep';
      schedulePreview();
    });
    layerMode.appendChild(layerSelect); controls.appendChild(layerMode);
    controls.appendChild(element('small', 'animation-frame-import-layer-help',
      equipmentCount
        ? 'This frame has ' + equipmentCount + ' weapon layer' +
          (equipmentCount === 1 ? '' : 's') + '. Keeping them separate preserves equipped-item artwork.'
        : 'This frame has no weapon layer. Both choices produce one imported body layer.'));
    controls.appendChild(element('p', 'art-import-background',
      'PNG transparency becomes the sprite intensity channel. JPEG backgrounds remain opaque.'));
    layout.appendChild(controls); body.appendChild(layout); modal.appendChild(body);

    var footer = element('div', 'error-modal-footer art-import-footer');
    footer.appendChild(button('Cancel', 'error-modal-ok', close));
    var applyButton = button('Import Frame', 'error-modal-ok', function() {
      if (!currentResult) return;
      try {
        var newOrdinal = OB64.animationSequences.importFrame(
          rom, separation, frame.sequenceIndex, currentResult,
          { keepEquipment: settings.keepEquipment });
        var updatedAnimation = separation.syntheticAnimation;
        var updatedFrame = updatedAnimation.frames[frame.sequenceIndex];
        var updatedLayer = updatedFrame.layers[newOrdinal];
        ui.animationLayer = newOrdinal;
        selectLayer(state, updatedAnimation, updatedFrame, updatedLayer, ui);
        changed(options);
        var crop = currentResult.crop;
        notify(options, 'Animation frame image import applied: file=' + source.name +
          '; format=' + source.format +
          '; source=' + source.width + 'x' + source.height +
          '; target=' + targetWidth + 'x' + targetHeight +
          '; crop=' + crop.width.toFixed(2) + 'x' + crop.height.toFixed(2) +
          '@(' + crop.x.toFixed(2) + ',' + crop.y.toFixed(2) + ')' +
          '; resize=' + currentResult.resizeMode +
          '; visible RGB555 colors=' + currentResult.sourceNativeColorCount +
          '->' + currentResult.colorCount + '/256' +
          '; Wu quantized=' + currentResult.quantized +
          '; ordered dither=' + currentResult.dithered +
          '; transparent output pixels=' + currentResult.transparentPixels +
          '; weapon layers kept=' + settings.keepEquipment + '.');
        close(); rerender();
      } catch (error) {
        notify(options, 'Animation frame import blocked: ' + error.message);
      }
    });
    applyButton.disabled = true; footer.appendChild(applyButton); modal.appendChild(footer);

    overlay.addEventListener('click', function(event) {
      if (event.target === overlay) close();
    });
    function blendImportedPixels(output, imported) {
      for (var pixel = 0; pixel < targetWidth * targetHeight; pixel++) {
        var offset = pixel * 4;
        if (!imported[offset + 3]) continue;
        var blended = sourceOver(
          [output[offset], output[offset + 1], output[offset + 2], output[offset + 3]],
          [imported[offset], imported[offset + 1], imported[offset + 2],
            imported[offset + 3]]);
        output[offset] = blended[0]; output[offset + 1] = blended[1];
        output[offset + 2] = blended[2]; output[offset + 3] = blended[3];
      }
    }
    function finalPreviewPixels(result) {
      if (!settings.keepEquipment || !equipmentCount) return result.rgba;
      var output = new Uint8ClampedArray(result.rgba.length);
      var inserted = false;
      frame.layers.forEach(function(frameLayer) {
        var layerSource = animation.artByKey[frameLayer.sourceKey];
        if (layerSource && layerSource.sourceRole === 'equipment') {
          drawLayerInto(output, animation, frameLayer, state.animations,
            'normal', null, weaponChildForAnimation(ui, animation));
        } else if (!inserted) {
          blendImportedPixels(output, result.rgba); inserted = true;
        }
      });
      if (!inserted) blendImportedPixels(output, result.rgba);
      return output;
    }
    function preview() {
      scheduledFrame = null;
      try {
        currentResult = OB64.art.prepareAnimationFrameImport(
          source.rgba, source.width, source.height,
          targetWidth, targetHeight, settings);
        previewHost.innerHTML = '';
        var scale = Math.max(1, Math.min(6,
          Math.floor(360 / Math.max(targetWidth, targetHeight))));
        var previewCanvas = element('canvas', 'art-import-preview-canvas');
        paintPixels(previewCanvas, targetWidth, targetHeight,
          finalPreviewPixels(currentResult), scale);
        previewHost.appendChild(previewCanvas);
        var crop = currentResult.crop;
        stats.textContent = 'Crop ' + crop.width.toFixed(1) + '\u00D7' +
          crop.height.toFixed(1) + ' at ' + crop.x.toFixed(1) + ', ' +
          crop.y.toFixed(1) + ' \u00B7 ' +
          currentResult.sourceNativeColorCount + ' native colors \u2192 ' +
          currentResult.colorCount + ' / 256' +
          (currentResult.quantized ? ' \u00B7 Wu quantized' :
            ' \u00B7 quantization not required') +
          (currentResult.dithered ? ' \u00B7 ordered dither applied' : '') +
          ' \u00B7 ' + currentResult.transparentPixels + ' transparent pixels' +
          (settings.keepEquipment && equipmentCount
            ? ' \u00B7 preview includes ' + equipmentCount + ' weapon layer' +
              (equipmentCount === 1 ? '' : 's') : '');
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

  function editor(state, rom, animation, frame, layer, ui, options, rerender) {
    var source = animation.artByKey[layer.sourceKey];
    var childOrdinal = selectedChildOrdinal(layer, source, ui);
    var separation = animation.spec.separatedCopy && OB64.animationSequences
      ? OB64.animationSequences.separationFor(animation, rom.animationSequences) : null;
    var section = element('section', 'animation-editor-section');
    var heading = element('div', 'art-editor-heading');
    var copy = element('div');
    copy.appendChild(element('h3', '', 'Frame ' + (frame.sequenceIndex + 1) +
      ' · ' + layerDisplayLabel(layer, source)));
    heading.appendChild(copy);
    var headingActions = element('div', 'animation-frame-heading-actions');
    if (M.hasEdit(state.animations, layer.sourceKey, childOrdinal)) {
      headingActions.appendChild(badge('Edited', 'edited'));
    } else if (!source.editable) {
      headingActions.appendChild(badge('Read-only', 'locked'));
    }
    var importInput = element('input', 'art-image-input');
    importInput.type = 'file';
    importInput.accept = 'image/png,image/jpeg,.png,.jpg,.jpeg';
    importInput.setAttribute('aria-label',
      'Select a PNG or JPEG image to import as this animation frame');
    importInput.addEventListener('change', async function() {
      var file = importInput.files && importInput.files[0];
      if (!file) return;
      try {
        if (!OB64.artUI || typeof OB64.artUI.decodeImageSource !== 'function') {
          throw new Error('the image decoder is unavailable');
        }
        var imageSource = await OB64.artUI.decodeImageSource(file);
        openFrameImportModal(imageSource, state, rom, separation,
          animation, frame, ui, options, rerender);
      } catch (error) {
        notify(options, 'Animation frame image import blocked: ' + error.message);
      } finally {
        importInput.value = '';
      }
    });
    headingActions.appendChild(importInput);
    var importFrame = button('Import Frame…', 'btn-secondary', function() {
      importInput.click();
    });
    importFrame.disabled = !separation;
    importFrame.title = separation
      ? 'Import any PNG or JPEG, crop it to this sequence canvas, and convert it to CI8 plus I4 combat art.'
      : 'Create a separated private sequence before importing a frame.';
    headingActions.appendChild(importFrame);
    if (OB64.spriteEditorUI && OB64.spriteEditorUI.openLibraryPicker) {
      var importLibraryFrame = button('Import Library Frame…', 'btn-secondary', function() {
        OB64.spriteEditorUI.openLibraryPicker(rom, {
          title: 'Choose Sprite Library Frame',
          actionLabel: 'Convert to Animation Frame',
          onStatus: function(message) { notify(options, message); }
        }, function(sourceAsset) {
          openFrameImportModal(sourceAsset, state, rom, separation,
            animation, frame, ui, options, rerender);
        });
      });
      importLibraryFrame.disabled = !separation;
      importLibraryFrame.title = separation
        ? 'Choose any Sprite Library frame and convert it to this private sequence canvas.'
        : 'Create a separated private sequence before importing a Sprite Library frame.';
      headingActions.appendChild(importLibraryFrame);
      var importLibrarySprite = button('Import Library Sprite…', 'btn-secondary', function() {
        OB64.spriteEditorUI.openLibraryPicker(rom, {
          title: 'Choose Sprite Library Art',
          actionLabel: 'Convert to Selected Layer',
          onStatus: function(message) { notify(options, message); }
        }, function(sourceAsset) {
          try {
            if (importLibrarySpritePixels(state.animations, source,
                childOrdinal, sourceAsset)) {
              changed(options);
              notify(options, sourceAsset.name +
                ' converted to the selected layer palette and dimensions.');
            }
            rerender();
          } catch (error) {
            notify(options, 'Sprite Library layer import blocked: ' + error.message);
          }
        });
      });
      importLibrarySprite.disabled = !source.editable;
      importLibrarySprite.title = source.editable
        ? 'Convert a Sprite Library frame to this sprite source, palette, and intensity channel.'
        : 'The selected sprite source is read-only.';
      headingActions.appendChild(importLibrarySprite);
    }
    var copyFrame = button('Copy Frame From…', 'btn-secondary', function() {
      openSpriteCopyModal(state, rom, separation, animation, frame, layer,
        'copy-frame', ui, options, rerender);
    });
    copyFrame.disabled = !separation;
    copyFrame.title = separation
      ? 'Replace this frame from any class, sequence, and frame.'
      : 'Create a separated private sequence before replacing a frame.';
    headingActions.appendChild(copyFrame);
    var addFrame = button('Add Frame', 'btn-secondary', function() {
      try {
        var frameIndex = OB64.animationSequences.addBlankFrame(
          rom, separation, frame.sequenceIndex, layer.ordinal);
        var updatedAnimation = separation.syntheticAnimation;
        var updatedFrame = updatedAnimation.frames[frameIndex];
        ui.animationFrame = frameIndex;
        ui.animationLayer = 0;
        selectLayer(state, updatedAnimation, updatedFrame,
          updatedFrame.layers[0], ui);
        changed(options);
        notify(options, 'Added a blank frame after the selected frame. ' +
          'Its duration is ' + updatedFrame.ticks + ' ticks.');
        rerender();
      } catch (error) {
        notify(options, 'Frame addition blocked: ' + error.message);
      }
    });
    addFrame.disabled = !separation;
    addFrame.title = separation
      ? 'Insert a blank frame after this frame using the same duration and selected palette.'
      : 'Create a separated private sequence before adding a frame.';
    headingActions.appendChild(addFrame);
    var removeFrame = button('Remove Frame',
      'btn-secondary animation-remove-action', function() {
        try {
          var oldFrameNumber = frame.sequenceIndex + 1;
          var nextFrameIndex = OB64.animationSequences.removeFrame(
            rom, separation, frame.sequenceIndex);
          var updatedAnimation = separation.syntheticAnimation;
          var updatedFrame = updatedAnimation.frames[nextFrameIndex];
          ui.animationFrame = nextFrameIndex;
          ui.animationLayer = 0;
          selectLayer(state, updatedAnimation, updatedFrame,
            updatedFrame.layers[0], ui);
          changed(options); notify(options,
            'Removed frame ' + oldFrameNumber + ' from this private sequence.');
          rerender();
        } catch (error) {
          notify(options, 'Frame removal blocked: ' + error.message);
        }
      });
    removeFrame.disabled = !separation || animation.frames.length <= 1;
    removeFrame.title = !separation
      ? 'Create a separated private sequence before removing a frame.'
      : (animation.frames.length <= 1
        ? 'An animation sequence must retain at least one frame.'
        : 'Remove this frame and its frame command from the private sequence.');
    headingActions.appendChild(removeFrame);
    heading.appendChild(headingActions);
    section.appendChild(heading);
    if (source.editable) {
      var scopePanel = editScopePanel(state.animations, animation, source,
        childOrdinal);
      if (scopePanel) section.appendChild(scopePanel);
    }
    if (source.editable || separation) {
      section.appendChild(toolbar(state, rom, animation, frame, layer,
        separation, ui, options, rerender));
    }
    var workbench = element('div', 'animation-frame-workbench');
    workbench.appendChild(fullFramePreview(state, animation, frame, ui));
    var editStage = element('div', 'animation-edit-stage');
    var canvas = element('canvas', 'animation-edit-canvas');
    drawFrame(canvas, animation, frame, state.animations, 8, layer, null,
      source.editable,
      weaponChildForAnimation(ui, animation));
    if (source.editable || separation) {
      installEditing(canvas, state, rom, separation, animation, frame, layer,
        ui, options, rerender);
    }
    editStage.appendChild(canvas);
    workbench.appendChild(editStage);
    workbench.appendChild(animationSequencePreview(state, animation, ui));
    section.appendChild(workbench);
    var actions = element('div', 'art-asset-actions animation-actions');
    if (source.editable) {
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
    }
    actions.appendChild(button('Export Current Frame PNG', 'btn-secondary', function() {
      downloadFrame(animation, frame, state,
        weaponChildForAnimation(ui, animation)).catch(function(error) {
        notify(options, error && error.message ? error.message : String(error));
      });
    }));
    var exportAnimation = button('Export Animation WebM', 'btn-secondary', function() {
      exportAnimation.disabled = true;
      exportAnimation.textContent = 'Exporting Animation…';
      notify(options, 'Recording one transparent animation loop at 30 ticks per second.');
      downloadAnimationWebm(animation, state,
        weaponChildForAnimation(ui, animation)).then(function(filename) {
        notify(options, 'Animation exported as ' + filename + '.');
      }).catch(function(error) {
        notify(options, error && error.message ? error.message : String(error));
      }).then(function() {
        exportAnimation.disabled = !animationWebmSupported();
        exportAnimation.textContent = 'Export Animation WebM';
      });
    });
    exportAnimation.disabled = !animationWebmSupported();
    if (exportAnimation.disabled) {
      exportAnimation.title = 'This browser cannot record canvas video as WebM.';
    } else {
      exportAnimation.title =
        'Download one transparent 4x loop at 30 video frames per second.';
    }
    actions.appendChild(exportAnimation);
    section.appendChild(actions);
    return section;
  }

  function render(state, ui, options, rerender, rom) {
    activePreviewBackground = ui && ui.transparentPreviewBackground === 'white'
      ? 'white' : 'checkerboard';
    var root = element('main', 'animation-editor-root');
    if (!state.animations || !state.animations.supported) {
      root.appendChild(element('h3', '', 'Combat Animation'));
      root.appendChild(element('p', '', state.animations
        ? state.animations.unavailableReason
        : 'Combat sprite state is unavailable.'));
      return root;
    }
    if (!ui.animationTool) ui.animationTool = 'pencil';
    if (!Number.isInteger(ui.animationBrushSize) ||
        ui.animationBrushSize < 1 || ui.animationBrushSize > 16) {
      ui.animationBrushSize = 1;
    }
    var catalog = effectiveAnimationCatalog(state, rom);
    state.animations.activeSourceAnimations = editScopeSourceIndex(
      state.animations, catalog, rom && rom.animationSequences);
    var animation = selectedAnimation(state, ui, catalog);
    if (ui.animationTool === 'move' && !animation.spec.separatedCopy) {
      ui.animationTool = 'pencil';
    }
    if (isIdleAnimation(animation)) {
      addAnimationsToSourceIndex(state.animations.activeSourceAnimations,
        idleAnimationRows(state.animations, animation.spec.classId));
    }
    if (isClassMotionAnimation(animation)) {
      addAnimationsToSourceIndex(state.animations.activeSourceAnimations,
        classMotionAnimationRows(state.animations, animation.spec.classId,
          animation.spec.classMotionKind));
    }
    var frame = selectedFrame(animation, ui);
    var layer = selectedLayer(frame, ui);
    if (!Number.isInteger(ui.animationPaletteIndex) || ui.animationPaletteIndex < 0 ||
        ui.animationPaletteIndex > 255) ui.animationPaletteIndex = 0;
    ui.animationIntensity = normalizeIntensity(ui.animationIntensity);
    weaponChildForAnimation(ui, animation);

    root.appendChild(animationPicker(
      state, animation, ui, rerender, catalog, rom, options));
    var targetAnimation = assignmentTargetAnimation(ui, catalog, animation);
    var layout = element('div', 'animation-editor-layout');
    var weaponSidebar = element('aside', 'animation-weapon-sidebar');
    weaponSidebar.appendChild(weaponPicker(state, animation, frame, layer, ui, rerender));
    layout.appendChild(weaponSidebar);
    var mainColumn = element('div', 'animation-main-column');
    mainColumn.appendChild(sequenceStrip(
      state, animation, targetAnimation, ui, options, rerender, rom));
    var workspace = element('div', 'animation-workspace');
    workspace.appendChild(editor(
      state, rom, animation, frame, layer, ui, options, rerender));
    var sidebar = element('aside', 'animation-sidebar');
    sidebar.appendChild(layerList(
      state, rom, animation, frame, layer, ui, options, rerender));
    var selectedSource = animation.artByKey[layer.sourceKey];
    if (selectedSource.editable) {
      sidebar.appendChild(palettePanel(selectedSource,
        selectedChildOrdinal(layer, selectedSource, ui), ui, rerender));
    }
    workspace.appendChild(sidebar);
    mainColumn.appendChild(workspace);
    layout.appendChild(mainColumn);
    root.appendChild(layout);
    return root;
  }

  OB64.animationUI = {
    render: render,
    wordRgb: wordRgb,
    normalizeIntensity: normalizeIntensity,
    intensityColorCss: intensityColorCss,
    paletteHueOrder: paletteHueOrder,
    animationBrushIndices: animationBrushIndices,
    sourceOver: sourceOver,
    selectLayer: selectLayer,
    samplePixel: samplePixel,
    animationStats: animationStats,
    weaponItemsForChild: weaponItemsForChild,
    retailMappingText: retailMappingText,
    weaponChildForAnimation: weaponChildForAnimation,
    setWeaponChild: setWeaponChild,
    childOrdinalForSource: childOrdinalForSource,
    childPixels: childPixels,
    weaponSourceForFrame: weaponSourceForFrame,
    framePixels: framePixels,
    singleLayerPixels: singleLayerPixels,
    paintPixels: paintPixels,
    layerDisplayLabel: layerDisplayLabel,
    drawFrame: drawFrame,
    framePngCanvas: framePngCanvas,
    selectorFlags: selectorFlags,
    selectorFlagParts: selectorFlagParts,
    animationSideLabel: animationSideLabel,
    animationSideClass: animationSideClass,
    reversedEnemyArtVariants: reversedEnemyArtVariants,
    animationArtVariantLabel: animationArtVariantLabel,
    animationLaneKey: animationLaneKey,
    animationLaneLabel: animationLaneLabel,
    isIdleAnimation: isIdleAnimation,
    isClassMotionAnimation: isClassMotionAnimation,
    classMotionSpec: classMotionSpec,
    idleAnimationRows: idleAnimationRows,
    classMotionAnimationRows: classMotionAnimationRows,
    animationCopyCatalogOptions: animationCopyCatalogOptions,
    animationPreviewTimeline: animationPreviewTimeline,
    animationPreviewFrameAtMs: animationPreviewFrameAtMs,
    animationPreviewSurfaces: animationPreviewSurfaces,
    animationExportFrames: animationExportFrames,
    animationWebmSurfaces: animationWebmSurfaces,
    animationWebmMimeType: animationWebmMimeType,
    animationPoseOffsetSummary: animationPoseOffsetSummary,
    animationArtName: animationArtName,
    animationSequenceStorageIdentity: animationSequenceStorageIdentity,
    animationSequenceIdentity: animationSequenceIdentity,
    sameAnimationSequence: sameAnimationSequence,
    tagLinkedVariantChoices: tagLinkedVariantChoices,
    animationVariantChoices: animationVariantChoices,
    animationSequenceCatalogRows: animationSequenceCatalogRows,
    animationClassVariantChoices: animationClassVariantChoices,
    editScopeSourceIndex: editScopeSourceIndex,
    animationActionFamily: animationActionFamily,
    currentSequenceChoiceForTarget: currentSequenceChoiceForTarget,
    sequenceAssignmentSummary: sequenceAssignmentSummary,
    animationArtRouteChoices: animationArtRouteChoices,
    animationActionChoices: animationActionChoices,
    classSearchMatches: classSearchMatches,
    animationClassChoices: animationClassChoices,
    usesEnemyPreviewSide: usesEnemyPreviewSide,
    variantQuality: variantQuality,
    preferredAnimation: preferredAnimation,
    defaultPreviewExplanation: defaultPreviewExplanation,
    canonicalAnimationCatalog: canonicalAnimationCatalog,
    effectiveAnimationCatalog: effectiveAnimationCatalog,
    requestAnimationRoute: requestAnimationRoute,
    rememberAnimationSelection: rememberAnimationSelection,
    rememberAnimationTarget: rememberAnimationTarget,
    assignmentTargetAnimation: assignmentTargetAnimation,
    selectedAnimation: selectedAnimation,
    spriteEditScope: spriteEditScope,
    editScopePanel: editScopePanel,
    identicalSpriteSlots: identicalSpriteSlots,
    localCoordinate: localCoordinate,
    flood: flood
  };
})();
