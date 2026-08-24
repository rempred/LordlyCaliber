// Lordly Caliber - deterministic Cutscene Studio Stage renderer.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var WIDTH = 320;
  var HEIGHT = 240;

  function RenderError(message) {
    this.name = 'CutsceneRenderError';
    this.message = message;
  }
  RenderError.prototype = Object.create(Error.prototype);
  RenderError.prototype.constructor = RenderError;

  function fail(message) { throw new RenderError(message); }
  function clamp(value, minimum, maximum) { return Math.max(minimum, Math.min(maximum, value)); }

  function surface(width, height) {
    if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1) {
      fail('Stage surface dimensions must be positive integers.');
    }
    return { width: width, height: height, rgba: new Uint8ClampedArray(width * height * 4) };
  }

  function pixel(output, x, y, color) {
    if (x < 0 || y < 0 || x >= output.width || y >= output.height) return;
    var offset = (y * output.width + x) * 4;
    var alpha = color[3] / 255;
    if (alpha >= 1) {
      output.rgba[offset] = color[0]; output.rgba[offset + 1] = color[1];
      output.rgba[offset + 2] = color[2]; output.rgba[offset + 3] = 255;
      return;
    }
    var targetAlpha = output.rgba[offset + 3] / 255;
    var finalAlpha = alpha + targetAlpha * (1 - alpha);
    for (var channel = 0; channel < 3; channel++) {
      output.rgba[offset + channel] = finalAlpha ? Math.round(
        (color[channel] * alpha + output.rgba[offset + channel] * targetAlpha * (1 - alpha)) /
        finalAlpha) : 0;
    }
    output.rgba[offset + 3] = Math.round(finalAlpha * 255);
  }

  function fillRect(output, x, y, width, height, color) {
    x = Math.floor(x); y = Math.floor(y); width = Math.ceil(width); height = Math.ceil(height);
    for (var row = 0; row < height; row++) {
      for (var column = 0; column < width; column++) pixel(output, x + column, y + row, color);
    }
  }

  function line(output, x0, y0, x1, y1, color) {
    x0 = Math.round(x0); y0 = Math.round(y0); x1 = Math.round(x1); y1 = Math.round(y1);
    var dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    var dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    var error = dx + dy;
    while (true) {
      pixel(output, x0, y0, color);
      if (x0 === x1 && y0 === y1) break;
      var twice = 2 * error;
      if (twice >= dy) { error += dy; x0 += sx; }
      if (twice <= dx) { error += dx; y0 += sy; }
    }
  }

  function fallbackBackground(output) {
    for (var y = 0; y < output.height; y++) {
      var amount = y / Math.max(1, output.height - 1);
      var color = [
        Math.round(24 + 18 * amount),
        Math.round(30 + 22 * amount),
        Math.round(46 + 28 * amount), 255
      ];
      fillRect(output, 0, y, output.width, 1, color);
    }
    var size = 16;
    for (var cy = 0; cy < output.height; cy += size) {
      for (var cx = 0; cx < output.width; cx += size) {
        if ((cx / size + cy / size) % 2) fillRect(output, cx, cy, size, size, [255, 255, 255, 5]);
      }
    }
    line(output, 0, 190, output.width - 1, 190, [192, 171, 119, 90]);
  }

  function blitScaled(output, image, targetX, targetY, targetWidth, targetHeight,
      opacityByte, tint) {
    if (!image || !Number.isInteger(image.width) || !Number.isInteger(image.height) ||
        !(image.rgba instanceof Uint8Array) && !(image.rgba instanceof Uint8ClampedArray) ||
        image.rgba.length !== image.width * image.height * 4) {
      fail('Renderable Stage image has invalid RGBA data.');
    }
    var opacity = Number.isFinite(opacityByte) ? clamp(opacityByte, 0, 255) / 255 : 1;
    tint = tint || { red: 255, green: 255, blue: 255 };
    var tintRed = clamp(Number.isFinite(tint.red) ? tint.red : 255, 0, 255) / 255;
    var tintGreen = clamp(Number.isFinite(tint.green) ? tint.green : 255, 0, 255) / 255;
    var tintBlue = clamp(Number.isFinite(tint.blue) ? tint.blue : 255, 0, 255) / 255;
    for (var y = 0; y < targetHeight; y++) {
      var sourceY = Math.min(image.height - 1, Math.floor(y * image.height / targetHeight));
      for (var x = 0; x < targetWidth; x++) {
        var sourceX = Math.min(image.width - 1, Math.floor(x * image.width / targetWidth));
        var sourceOffset = (sourceY * image.width + sourceX) * 4;
        pixel(output, targetX + x, targetY + y, [
          Math.round(image.rgba[sourceOffset] * tintRed),
          Math.round(image.rgba[sourceOffset + 1] * tintGreen),
          Math.round(image.rgba[sourceOffset + 2] * tintBlue),
          Math.round(image.rgba[sourceOffset + 3] * opacity)
        ]);
      }
    }
  }

  function blitScaledRotated(output, image, originX, originY, targetWidth, targetHeight,
      anchorX, anchorY, rotationDegrees, opacityByte, tint) {
    var normalizedRotation = Number.isFinite(rotationDegrees)
      ? ((rotationDegrees % 360) + 360) % 360 : 0;
    if (normalizedRotation < 0.000001 || Math.abs(normalizedRotation - 360) < 0.000001) {
      blitScaled(output, image, Math.round(originX - anchorX),
        Math.round(originY - anchorY), targetWidth, targetHeight, opacityByte, tint);
      return;
    }
    if (!image || !Number.isInteger(image.width) || !Number.isInteger(image.height) ||
        !(image.rgba instanceof Uint8Array) && !(image.rgba instanceof Uint8ClampedArray) ||
        image.rgba.length !== image.width * image.height * 4) {
      fail('Renderable Stage image has invalid RGBA data.');
    }
    var radians = normalizedRotation * Math.PI / 180;
    var cosine = Math.cos(radians), sine = Math.sin(radians);
    var corners = [
      { x: -anchorX, y: -anchorY },
      { x: targetWidth - anchorX, y: -anchorY },
      { x: targetWidth - anchorX, y: targetHeight - anchorY },
      { x: -anchorX, y: targetHeight - anchorY }
    ].map(function(point) {
      return {
        x: originX + point.x * cosine - point.y * sine,
        y: originY + point.x * sine + point.y * cosine
      };
    });
    var minimumX = Math.max(0, Math.floor(Math.min.apply(null,
      corners.map(function(point) { return point.x; }))));
    var maximumX = Math.min(output.width - 1, Math.ceil(Math.max.apply(null,
      corners.map(function(point) { return point.x; }))));
    var minimumY = Math.max(0, Math.floor(Math.min.apply(null,
      corners.map(function(point) { return point.y; }))));
    var maximumY = Math.min(output.height - 1, Math.ceil(Math.max.apply(null,
      corners.map(function(point) { return point.y; }))));
    var opacity = Number.isFinite(opacityByte) ? clamp(opacityByte, 0, 255) / 255 : 1;
    tint = tint || { red: 255, green: 255, blue: 255 };
    var tintRed = clamp(Number.isFinite(tint.red) ? tint.red : 255, 0, 255) / 255;
    var tintGreen = clamp(Number.isFinite(tint.green) ? tint.green : 255, 0, 255) / 255;
    var tintBlue = clamp(Number.isFinite(tint.blue) ? tint.blue : 255, 0, 255) / 255;
    for (var y = minimumY; y <= maximumY; y++) {
      for (var x = minimumX; x <= maximumX; x++) {
        var deltaX = x + 0.5 - originX;
        var deltaY = y + 0.5 - originY;
        var localX = deltaX * cosine + deltaY * sine + anchorX;
        var localY = -deltaX * sine + deltaY * cosine + anchorY;
        if (localX < 0 || localY < 0 || localX >= targetWidth || localY >= targetHeight) continue;
        var sourceX = Math.min(image.width - 1,
          Math.floor(localX * image.width / targetWidth));
        var sourceY = Math.min(image.height - 1,
          Math.floor(localY * image.height / targetHeight));
        var sourceOffset = (sourceY * image.width + sourceX) * 4;
        pixel(output, x, y, [
          Math.round(image.rgba[sourceOffset] * tintRed),
          Math.round(image.rgba[sourceOffset + 1] * tintGreen),
          Math.round(image.rgba[sourceOffset + 2] * tintBlue),
          Math.round(image.rgba[sourceOffset + 3] * opacity)
        ]);
      }
    }
  }

  function nativeVignetteEllipseAlpha(x, y, radiusX, radiusY, alphaCap) {
    if (!radiusX || !radiusY) return 0;
    var normalizedX = Math.fround(x / radiusX);
    var normalizedY = Math.fround(y / radiusY);
    var distanceSquared = Math.fround(
      Math.fround(normalizedX * normalizedX) +
      Math.fround(normalizedY * normalizedY));
    var remaining = Math.fround(1 - distanceSquared);
    if (!(remaining > 0)) return x === 0 && y === 0 ? alphaCap : 0;
    return Math.trunc(Math.fround(remaining * alphaCap));
  }

  function nativeVignetteEdgeAlpha(x, y, width, height, alpha) {
    var halfWidth = Math.trunc(width / 2);
    var halfHeight = Math.trunc(height / 2);
    var absoluteX = Math.abs(x);
    var absoluteY = Math.abs(y);
    for (var edgeStep = 0; edgeStep < 16; edgeStep++) {
      if (halfHeight - edgeStep < absoluteY ||
          halfWidth - edgeStep < absoluteX) {
        return edgeStep < 8 ? Math.min(alpha, edgeStep * 16) : alpha;
      }
    }
    return alpha;
  }

  function buildSceneVignetteImage(source, alphaCap) {
    if (!source || !Number.isInteger(source.width) || !Number.isInteger(source.height) ||
        !(source.rgba instanceof Uint8Array) &&
          !(source.rgba instanceof Uint8ClampedArray) ||
        source.rgba.length !== source.width * source.height * 4) {
      fail('Scene vignette source has invalid RGBA data.');
    }
    if (!Number.isInteger(alphaCap) || alphaCap < 0 || alphaCap > 255) {
      fail('Scene vignette alpha cap must be an unsigned byte.');
    }
    var width = Math.floor(source.width / 2);
    var height = Math.floor(source.height / 2);
    var output = surface(Math.max(1, width), Math.max(1, height));
    var radiusX = Math.trunc(width * 0.5);
    var radiusY = Math.trunc(Math.fround(height * Math.fround(0.58)));
    var startX = Math.trunc(-width / 2);
    var startY = Math.trunc(-height / 2);
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        var sourceOffset = ((y * 2) * source.width + x * 2) * 4;
        var centeredX = startX + x;
        var centeredY = startY + y;
        var alpha = nativeVignetteEllipseAlpha(
          centeredX, centeredY, radiusX, radiusY, alphaCap);
        alpha = nativeVignetteEdgeAlpha(
          centeredX, centeredY, width, height, alpha);
        var outputOffset = (y * width + x) * 4;
        output.rgba[outputOffset] = source.rgba[sourceOffset];
        output.rgba[outputOffset + 1] = source.rgba[sourceOffset + 1];
        output.rgba[outputOffset + 2] = source.rgba[sourceOffset + 2];
        output.rgba[outputOffset + 3] = alpha & 0xFF;
      }
    }
    output.nativeSourceWidth = source.width;
    output.nativeSourceHeight = source.height;
    output.nativeAlphaCap = alphaCap;
    output.nativeHorizontalRadius = radiusX;
    output.nativeVerticalRadius = radiusY;
    output.nativeDownsampleDivisor = 2;
    return output;
  }

  function renderSceneVignette(output, source, presentation, view, camera) {
    if (!source || !presentation) return null;
    view = view || { x: 0, y: 0, scale: 1 };
    camera = camera || cameraTransform();
    var image = buildSceneVignetteImage(source, presentation.alphaCap);
    var viewScale = Number.isFinite(view.scale) ? view.scale : 1;
    var scaleX = presentation.scaleXPercent / 100 * viewScale;
    var scaleY = presentation.scaleYPercent / 100 * viewScale;
    if (!Number.isFinite(scaleX) || !Number.isFinite(scaleY) ||
        scaleX === 0 || scaleY === 0) return null;
    var width = Math.max(1, Math.round(image.width * Math.abs(scaleX) * camera.scaleX));
    var height = Math.max(1, Math.round(image.height * Math.abs(scaleY) * camera.scaleY));
    var center = transformStagePoint({
      x: WIDTH / 2 + presentation.translateX + (Number(view.x) || 0),
      y: HEIGHT / 2 - presentation.translateY - (Number(view.y) || 0)
    }, camera);
    var rotation = presentation.baseRotationDegrees &&
      Number(presentation.baseRotationDegrees.z) || 0;
    if (scaleX < 0) rotation += 180;
    blitScaledRotated(output, image, center.x, center.y, width, height,
      width / 2, height / 2, rotation);
    return {
      image: image,
      centerX: center.x,
      centerY: center.y,
      width: width,
      height: height,
      rotationDegrees: rotation,
      sourceAssetId: presentation.sourceAssetId,
      evidenceStatus: presentation.evidenceStatus
    };
  }

  function invertMatrix3(matrix) {
    var a = matrix[0], b = matrix[1], c = matrix[2];
    var d = matrix[3], e = matrix[4], f = matrix[5];
    var g = matrix[6], h = matrix[7], i = matrix[8];
    var determinant = a * (e * i - f * h) - b * (d * i - f * g) +
      c * (d * h - e * g);
    if (!Number.isFinite(determinant) || Math.abs(determinant) < 0.000000001) return null;
    return [
      (e * i - f * h) / determinant,
      (c * h - b * i) / determinant,
      (b * f - c * e) / determinant,
      (f * g - d * i) / determinant,
      (a * i - c * g) / determinant,
      (c * d - a * f) / determinant,
      (d * h - e * g) / determinant,
      (b * g - a * h) / determinant,
      (a * e - b * d) / determinant
    ];
  }

  // Maps normalized texture coordinates through the projected four-corner plane.
  function quadHomography(quad) {
    var x0 = quad[0].x, y0 = quad[0].y;
    var x1 = quad[1].x, y1 = quad[1].y;
    var x2 = quad[2].x, y2 = quad[2].y;
    var x3 = quad[3].x, y3 = quad[3].y;
    var dx3 = x0 - x1 + x2 - x3;
    var dy3 = y0 - y1 + y2 - y3;
    var g = 0, h = 0;
    if (Math.abs(dx3) > 0.000000001 || Math.abs(dy3) > 0.000000001) {
      var dx1 = x1 - x2, dx2 = x3 - x2;
      var dy1 = y1 - y2, dy2 = y3 - y2;
      var denominator = dx1 * dy2 - dx2 * dy1;
      if (!Number.isFinite(denominator) || Math.abs(denominator) < 0.000000001) return null;
      g = (dx3 * dy2 - dx2 * dy3) / denominator;
      h = (dx1 * dy3 - dx3 * dy1) / denominator;
    }
    var matrix = [
      x1 - x0 + g * x1, x3 - x0 + h * x3, x0,
      y1 - y0 + g * y1, y3 - y0 + h * y3, y0,
      g, h, 1
    ];
    return { matrix: matrix, inverse: invertMatrix3(matrix) };
  }

  function blitProjectedQuad(output, image, quad, opacityByte, tint) {
    if (!image || !Number.isInteger(image.width) || !Number.isInteger(image.height) ||
        !(image.rgba instanceof Uint8Array) && !(image.rgba instanceof Uint8ClampedArray) ||
        image.rgba.length !== image.width * image.height * 4) {
      fail('Renderable Stage image has invalid RGBA data.');
    }
    if (!Array.isArray(quad) || quad.length !== 4 || quad.some(function(point) {
      return !point || !Number.isFinite(point.x) || !Number.isFinite(point.y);
    })) return false;
    var homography = quadHomography(quad);
    if (!homography || !homography.inverse) return false;
    var inverse = homography.inverse;
    var minimumX = Math.max(0, Math.floor(Math.min.apply(null,
      quad.map(function(point) { return point.x; }))));
    var maximumX = Math.min(output.width - 1, Math.ceil(Math.max.apply(null,
      quad.map(function(point) { return point.x; }))) - 1);
    var minimumY = Math.max(0, Math.floor(Math.min.apply(null,
      quad.map(function(point) { return point.y; }))));
    var maximumY = Math.min(output.height - 1, Math.ceil(Math.max.apply(null,
      quad.map(function(point) { return point.y; }))) - 1);
    if (maximumX < minimumX || maximumY < minimumY) return false;
    var opacity = Number.isFinite(opacityByte) ? clamp(opacityByte, 0, 255) / 255 : 1;
    tint = tint || { red: 255, green: 255, blue: 255 };
    var tintRed = clamp(Number.isFinite(tint.red) ? tint.red : 255, 0, 255) / 255;
    var tintGreen = clamp(Number.isFinite(tint.green) ? tint.green : 255, 0, 255) / 255;
    var tintBlue = clamp(Number.isFinite(tint.blue) ? tint.blue : 255, 0, 255) / 255;
    var drewPixel = false;
    for (var y = minimumY; y <= maximumY; y++) {
      for (var x = minimumX; x <= maximumX; x++) {
        var targetX = x + 0.5, targetY = y + 0.5;
        var sourceU = inverse[0] * targetX + inverse[1] * targetY + inverse[2];
        var sourceV = inverse[3] * targetX + inverse[4] * targetY + inverse[5];
        var sourceW = inverse[6] * targetX + inverse[7] * targetY + inverse[8];
        if (!Number.isFinite(sourceW) || Math.abs(sourceW) < 0.000000001) continue;
        sourceU /= sourceW;
        sourceV /= sourceW;
        if (sourceU < -0.000001 || sourceU > 1.000001 ||
            sourceV < -0.000001 || sourceV > 1.000001) continue;
        var sourceX = Math.min(image.width - 1,
          Math.max(0, Math.floor(clamp(sourceU, 0, 1) * image.width)));
        var sourceY = Math.min(image.height - 1,
          Math.max(0, Math.floor(clamp(sourceV, 0, 1) * image.height)));
        var sourceOffset = (sourceY * image.width + sourceX) * 4;
        pixel(output, x, y, [
          Math.round(image.rgba[sourceOffset] * tintRed),
          Math.round(image.rgba[sourceOffset + 1] * tintGreen),
          Math.round(image.rgba[sourceOffset + 2] * tintBlue),
          Math.round(image.rgba[sourceOffset + 3] * opacity)
        ]);
        drewPixel = true;
      }
    }
    return drewPixel;
  }

  function backgroundImage(entry) {
    return entry && entry.image ? entry.image : entry;
  }

  function backgroundRole(entry) {
    var layer = entry && entry.layer || {};
    return layer.role || layer.source && layer.source.role || null;
  }

  function b5LocalOrigin(image) {
    if (Number(image && image.format) === 5 && image.compoundAssembled !== true) {
      return { x: -160, y: -120 };
    }
    if (!image || !Number.isFinite(image.originX) || !Number.isFinite(image.originY)) {
      return null;
    }
    return { x: image.originX, y: image.originY };
  }

  function renderBackgrounds(output, backgrounds, projection, actorProjection, phase, camera) {
    projection = projection || {};
    phase = phase || 'base';
    camera = camera || cameraTransform();
    backgrounds = backgrounds.filter(function(entry) {
      var foreground = backgroundRole(entry) === 'foreground-mask';
      return phase === 'foreground' ? foreground : !foreground;
    });
    if (projection.mode === 'b5-reference-capture') {
      if (phase !== 'foreground') {
        fillRect(output, 0, 0, output.width, output.height, [0, 0, 0, 255]);
      }
      var captureScale = Number.isFinite(projection.scale) ? projection.scale : 1;
      var screenAnchorX = Number.isFinite(projection.screenAnchorX)
        ? projection.screenAnchorX : 0;
      var screenAnchorY = Number.isFinite(projection.screenAnchorY)
        ? projection.screenAnchorY : 0;
      backgrounds.forEach(function(entry) {
        var image = backgroundImage(entry);
        if (!image || !image.rgba || !image.reference ||
            !Number.isFinite(image.originX) || !Number.isFinite(image.originY)) return;
        var worldX = image.originX + image.reference.x;
        var worldY = image.originY + image.reference.y;
        var captureRect = transformStageRect(
          screenAnchorX + (worldX - projection.cropWorldX) * captureScale,
          screenAnchorY + (worldY - projection.cropWorldY) * captureScale,
          image.width * captureScale, image.height * captureScale, camera);
        blitScaled(output, image, captureRect.x, captureRect.y,
          captureRect.width, captureRect.height);
      });
      return;
    }
    backgrounds.forEach(function(entry) {
      var image = backgroundImage(entry);
      if (!image || !image.rgba) return;
      var nativeGeometry = modeZeroBackgroundGeometry(
        image, entry && entry.layer, actorProjection);
      if (nativeGeometry) {
        blitProjectedQuad(output, image, nativeGeometry.screenQuad.map(function(point) {
          return transformStagePointFloat(point, camera);
        }));
        return;
      }
      var scale = Number.isFinite(projection.scale) ? projection.scale : 1;
      var width = Math.max(1, Math.round(image.width * scale));
      var height = Math.max(1, Math.round(image.height * scale));
      var nativeOrigin = image.container === 'bg2' ? b5LocalOrigin(image) : null;
      var targetX = nativeOrigin
        ? Math.round(WIDTH / 2 + nativeOrigin.x * scale)
        : Math.round((WIDTH - width) / 2);
      var targetY = nativeOrigin
        ? Math.round(HEIGHT / 2 + nativeOrigin.y * scale)
        : Math.round((HEIGHT - height) / 2);
      var targetRect = transformStageRect(targetX, targetY, width, height, camera);
      blitScaled(output, image, targetRect.x, targetRect.y,
        targetRect.width, targetRect.height);
    });
  }

  function applyViewportMask(output, viewport) {
    if (!viewport || !Number.isFinite(viewport.left) || !Number.isFinite(viewport.top) ||
        !Number.isFinite(viewport.width) || !Number.isFinite(viewport.height)) return;
    var left = Math.round(viewport.left), top = Math.round(viewport.top);
    var right = left + Math.round(viewport.width);
    var bottom = top + Math.round(viewport.height);
    if (top > 0) fillRect(output, 0, 0, output.width, top, [0, 0, 0, 255]);
    if (bottom < output.height) {
      fillRect(output, 0, bottom, output.width, output.height - bottom, [0, 0, 0, 255]);
    }
    if (left > 0) fillRect(output, 0, top, left, bottom - top, [0, 0, 0, 255]);
    if (right < output.width) {
      fillRect(output, right, top, output.width - right, bottom - top, [0, 0, 0, 255]);
    }
  }

  function applyScreenTransitionMask(output, transition) {
    if (!transition || transition.presentationKind !== 'cutscene-crop' ||
        !Number.isFinite(transition.currentFirst) ||
        !Number.isFinite(transition.currentSecond)) return;
    var top = clamp(Math.round(transition.currentFirst), 0, output.height);
    var bottomMargin = clamp(Math.round(transition.currentSecond), 0, output.height);
    var bottom = Math.max(top, output.height - bottomMargin);
    if (top > 0) fillRect(output, 0, 0, output.width, top, [0, 0, 0, 255]);
    if (bottom < output.height) {
      fillRect(output, 0, bottom, output.width, output.height - bottom,
        [0, 0, 0, 255]);
    }
  }

  function cameraTransform(value) {
    value = value || {};
    return {
      translateX: Number.isFinite(value.translateX) ? value.translateX : 0,
      translateY: Number.isFinite(value.translateY) ? value.translateY : 0,
      scaleX: clamp(Number.isFinite(value.scaleX) ? value.scaleX : 1, 0.05, 16),
      scaleY: clamp(Number.isFinite(value.scaleY) ? value.scaleY : 1, 0.05, 16),
      activeClipId: value.activeClipId || null,
      timingStatus: value.timingStatus || 'default Stage camera'
    };
  }

  function relativeCameraTransform(current, registered) {
    current = cameraTransform(current);
    registered = cameraTransform(registered);
    var scaleX = current.scaleX / registered.scaleX;
    var scaleY = current.scaleY / registered.scaleY;
    return {
      translateX: current.translateX - scaleX * registered.translateX,
      translateY: current.translateY - scaleY * registered.translateY,
      scaleX: scaleX,
      scaleY: scaleY,
      activeClipId: current.activeClipId,
      timingStatus: current.timingStatus
    };
  }

  function transformStagePointFloat(point, camera) {
    return {
      x: (point.x - WIDTH / 2) * camera.scaleX + WIDTH / 2 + camera.translateX,
      y: (point.y - HEIGHT / 2) * camera.scaleY + HEIGHT / 2 - camera.translateY
    };
  }

  function transformStagePoint(point, camera) {
    var transformed = transformStagePointFloat(point, camera);
    return {
      x: Math.round(transformed.x),
      y: Math.round(transformed.y)
    };
  }

  function transformStageRect(x, y, width, height, camera) {
    var topLeft = transformStagePointFloat({ x: x, y: y }, camera);
    var bottomRight = transformStagePointFloat({ x: x + width, y: y + height }, camera);
    return {
      x: Math.round(Math.min(topLeft.x, bottomRight.x)),
      y: Math.round(Math.min(topLeft.y, bottomRight.y)),
      width: Math.max(1, Math.round(Math.abs(bottomRight.x - topLeft.x))),
      height: Math.max(1, Math.round(Math.abs(bottomRight.y - topLeft.y)))
    };
  }

  function untransformStagePoint(point, value) {
    var camera = cameraTransform(value);
    return {
      x: (point.x - WIDTH / 2 - camera.translateX) / camera.scaleX + WIDTH / 2,
      y: (point.y - HEIGHT / 2 + camera.translateY) / camera.scaleY + HEIGHT / 2
    };
  }

  function applyCameraTransform(source, camera) {
    var output = surface(source.width, source.height);
    for (var y = 0; y < output.height; y++) {
      var sourceY = Math.round((y - HEIGHT / 2 + camera.translateY) /
        camera.scaleY + HEIGHT / 2);
      for (var x = 0; x < output.width; x++) {
        var targetOffset = (y * output.width + x) * 4;
        var sourceX = Math.round((x - WIDTH / 2 - camera.translateX) /
          camera.scaleX + WIDTH / 2);
        if (sourceX < 0 || sourceY < 0 || sourceX >= source.width || sourceY >= source.height) {
          output.rgba[targetOffset + 3] = 255;
          continue;
        }
        var sourceOffset = (sourceY * source.width + sourceX) * 4;
        output.rgba[targetOffset] = source.rgba[sourceOffset];
        output.rgba[targetOffset + 1] = source.rgba[sourceOffset + 1];
        output.rgba[targetOffset + 2] = source.rgba[sourceOffset + 2];
        output.rgba[targetOffset + 3] = source.rgba[sourceOffset + 3];
      }
    }
    return output;
  }

  function scenePoints(document) {
    var points = [];
    document.actors.forEach(function(actor) {
      points.push({ x: actor.initial.x, y: actor.initial.y, z: actor.initial.z });
    });
    document.tracks.forEach(function(track) {
      if (track.type !== 'movement') return;
      track.clips.forEach(function(clip) {
        ['from', 'to'].forEach(function(field) {
          var point = clip.payload[field];
          if (point && Number.isFinite(point.x) && Number.isFinite(point.z)) points.push(point);
        });
      });
    });
    return points.length ? points : [{ x: -100, y: 0, z: -100 }, { x: 100, y: 0, z: 100 }];
  }

  function vectorLength(value) {
    return Math.sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
  }

  function normalizeVector(value) {
    var length = vectorLength(value);
    if (!length) fail('Native Stage projection contains a zero-length vector.');
    return { x: value.x / length, y: value.y / length, z: value.z / length };
  }

  function crossVector(left, right) {
    return {
      x: left.y * right.z - left.z * right.y,
      y: left.z * right.x - left.x * right.z,
      z: left.x * right.y - left.y * right.x
    };
  }

  function dotVector(left, right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
  }

  function multiplyMatrices(left, right) {
    var output = [];
    for (var row = 0; row < 4; row++) {
      output[row] = [];
      for (var column = 0; column < 4; column++) {
        var value = 0;
        for (var inner = 0; inner < 4; inner++) {
          value += left[row][inner] * right[inner][column];
        }
        output[row][column] = value;
      }
    }
    return output;
  }

  function multiplyPoint(point, matrix) {
    var source = [point.x, point.y, point.z, 1];
    var output = [];
    for (var column = 0; column < 4; column++) {
      output[column] = 0;
      for (var row = 0; row < 4; row++) output[column] += source[row] * matrix[row][column];
    }
    return output;
  }

  function nativeProjectionMatrix(projection) {
    var eye = projection.eye;
    var target = projection.target;
    var back = normalizeVector({
      x: eye.x - target.x,
      y: eye.y - target.y,
      z: eye.z - target.z
    });
    var right = normalizeVector(crossVector(projection.up, back));
    var trueUp = crossVector(back, right);
    var view = [
      [right.x, trueUp.x, back.x, 0],
      [right.y, trueUp.y, back.y, 0],
      [right.z, trueUp.z, back.z, 0],
      [-dotVector(eye, right), -dotVector(eye, trueUp), -dotVector(eye, back), 1]
    ];
    var focal = 1 / Math.tan(projection.fovYDegrees * Math.PI / 360);
    var depth = projection.near - projection.far;
    var perspective = [
      [focal / projection.aspect, 0, 0, 0],
      [0, focal, 0, 0],
      [0, 0, (projection.far + projection.near) / depth, -1],
      [0, 0, 2 * projection.far * projection.near / depth, 0]
    ];
    return multiplyMatrices(view, perspective);
  }

  function capturedActorProjection(document) {
    var projection = document && document.background && document.background.projection;
    projection = projection && projection.actorProjection;
    return projection && projection.mode === 'native-perspective-capture' ? projection : null;
  }

  function computeProjection(document, previewState) {
    var captured = capturedActorProjection(document);
    if (captured) return captured;
    var runtimeBounds = previewState && previewState.actorProjection &&
      previewState.actorProjection.previewFitBounds;
    var points = runtimeBounds ? null : scenePoints(document);
    var xMin = runtimeBounds && Number.isFinite(runtimeBounds.xMin)
      ? runtimeBounds.xMin
      : Math.min.apply(null, points.map(function(point) { return point.x; }));
    var xMax = runtimeBounds && Number.isFinite(runtimeBounds.xMax)
      ? runtimeBounds.xMax
      : Math.max.apply(null, points.map(function(point) { return point.x; }));
    var zMin = runtimeBounds && Number.isFinite(runtimeBounds.zMin)
      ? runtimeBounds.zMin
      : Math.min.apply(null, points.map(function(point) { return point.z; }));
    var zMax = runtimeBounds && Number.isFinite(runtimeBounds.zMax)
      ? runtimeBounds.zMax
      : Math.max.apply(null, points.map(function(point) { return point.z; }));
    if (xMax - xMin < 80) { var xMid = (xMin + xMax) / 2; xMin = xMid - 40; xMax = xMid + 40; }
    if (zMax - zMin < 80) { var zMid = (zMin + zMax) / 2; zMin = zMid - 40; zMax = zMid + 40; }
    return {
      mode: 'fit-native-preview',
      status: runtimeBounds
        ? 'stable full-runtime Actor fit because the native launch camera is unresolved'
        : 'approximate',
      xMin: xMin, xMax: xMax, zMin: zMin, zMax: zMax,
      left: 28, right: WIDTH - 28, top: 55, bottom: HEIGHT - 30
    };
  }

  function projectPointFloat(point, projection, modelScaleOverride) {
    if (projection.mode === 'native-perspective-capture' ||
        projection.mode === 'native-perspective-runtime') {
      var scale = Number.isFinite(modelScaleOverride)
        ? modelScaleOverride : projection.modelScale;
      var clip = multiplyPoint({
        x: (Number(point.x) || 0) * scale,
        y: (Number(point.y) || 0) * scale,
        z: (Number(point.z) || 0) * scale
      }, nativeProjectionMatrix(projection));
      if (!clip[3]) return { x: -0x7FFFFFFF, y: -0x7FFFFFFF };
      return {
        x: projection.screenWidth / 2 +
          clip[0] / clip[3] * projection.screenWidth / 2,
        y: projection.screenHeight / 2 -
          clip[1] / clip[3] * projection.screenHeight / 2
      };
    }
    var xAmount = (point.x - projection.xMin) / (projection.xMax - projection.xMin);
    var zAmount = (point.z - projection.zMin) / (projection.zMax - projection.zMin);
    return {
      x: projection.left + clamp(xAmount, -0.15, 1.15) *
        (projection.right - projection.left),
      y: projection.bottom - clamp(zAmount, -0.15, 1.15) *
        (projection.bottom - projection.top) - (Number(point.y) || 0) * 0.2
    };
  }

  function projectPoint(point, projection) {
    var projected = projectPointFloat(point, projection);
    return { x: Math.round(projected.x), y: Math.round(projected.y) };
  }

  function projectionDepth(point, projection, modelScaleOverride) {
    var scale = Number.isFinite(modelScaleOverride)
      ? modelScaleOverride : Number(projection.modelScale);
    if (!Number.isFinite(scale)) return null;
    var back = normalizeVector({
      x: projection.eye.x - projection.target.x,
      y: projection.eye.y - projection.target.y,
      z: projection.eye.z - projection.target.z
    });
    return dotVector({
      x: projection.eye.x - (Number(point.x) || 0) * scale,
      y: projection.eye.y - (Number(point.y) || 0) * scale,
      z: projection.eye.z - (Number(point.z) || 0) * scale
    }, back);
  }

  function perspectivePixelsPerModelUnit(point, projection) {
    if (!projection || (projection.mode !== 'native-perspective-capture' &&
        projection.mode !== 'native-perspective-runtime')) return 1;
    var modelScale = Number(projection.modelScale);
    var depth = projectionDepth(point, projection);
    var tangent = Math.tan(projection.fovYDegrees * Math.PI / 360);
    if (!Number.isFinite(modelScale) || !Number.isFinite(depth) ||
        !Number.isFinite(tangent) || modelScale <= 0 || depth <= 0 || tangent <= 0) return 1;
    return modelScale * projection.screenHeight / 2 / (depth * tangent);
  }

  function actorCameraNormalization(projection) {
    var delta = {
      x: projection.eye.x - projection.target.x,
      y: projection.eye.y - projection.target.y,
      z: projection.eye.z - projection.target.z
    };
    return Math.tan(projection.fovYDegrees * Math.PI / 360) *
      vectorLength(delta) / 120;
  }

  // func_002AFA68 builds this two-angle matrix through func_80092A90.
  // The native helper accepts a third angle, but Director scene channels pass zero.
  function sceneRotationMatrix(rotationX, rotationY) {
    var x = (Number(rotationX) || 0) * Math.PI / 180;
    var y = (Number(rotationY) || 0) * Math.PI / 180;
    var sx = Math.sin(x), cx = Math.cos(x);
    var sy = Math.sin(y), cy = Math.cos(y);
    return [
      [cy, 0, -sy],
      [sx * sy, cx, sx * cy],
      [cx * sy, -sx, cx * cy]
    ];
  }

  function transformModeZeroStagePoint(point, channel, actorProjection) {
    channel = channel || {};
    var rotation = sceneRotationMatrix(channel.rotationX, channel.rotationY);
    var rotated = {
      x: point.x * rotation[0][0] + point.y * rotation[1][0] + point.z * rotation[2][0],
      y: point.x * rotation[0][1] + point.y * rotation[1][1] + point.z * rotation[2][1],
      z: point.x * rotation[0][2] + point.y * rotation[1][2] + point.z * rotation[2][2]
    };
    var normalization = actorCameraNormalization(actorProjection);
    return {
      x: (rotated.x + (Number(channel.translateX) || 0)) * normalization,
      y: (rotated.y + (Number(channel.translateY) || 0)) * normalization,
      z: (rotated.z + (Number(channel.translateZ) || 0)) * normalization
    };
  }

  function modeZeroBackgroundGeometry(image, layer, actorProjection) {
    if (!image || !layer || layer.renderPipeline !== 'mode-zero-b5-actor-camera' ||
        !actorProjection || (actorProjection.mode !== 'native-perspective-capture' &&
          actorProjection.mode !== 'native-perspective-runtime')) return null;
    var origin = b5LocalOrigin(image);
    if (!origin && image && Number.isInteger(image.width) && Number.isInteger(image.height)) {
      origin = { x: -image.width / 2, y: -image.height / 2 };
    }
    if (!origin || !Number.isInteger(image.width) || !Number.isInteger(image.height) ||
        image.width < 1 || image.height < 1) return null;
    var channel = layer.sceneTransform || {};
    var uniformScale = Number.isFinite(channel.uniformScale)
      ? channel.uniformScale : 1;
    var localQuad = [
      { x: origin.x, y: -origin.y, z: 0 },
      { x: origin.x + image.width, y: -origin.y, z: 0 },
      { x: origin.x + image.width, y: -(origin.y + image.height), z: 0 },
      { x: origin.x, y: -(origin.y + image.height), z: 0 }
    ].map(function(point) {
      return {
        x: point.x * uniformScale,
        y: point.y * uniformScale,
        z: point.z * uniformScale
      };
    });
    var sceneQuad = localQuad.map(function(point) {
      return transformModeZeroStagePoint(point, channel, actorProjection);
    });
    if (sceneQuad.some(function(point) {
      var depth = projectionDepth(point, actorProjection, 1);
      return !Number.isFinite(depth) || depth <= 0;
    })) return null;
    var screenQuad = sceneQuad.map(function(point) {
      return projectPointFloat(point, actorProjection, 1);
    });
    if (screenQuad.some(function(point) {
      return !Number.isFinite(point.x) || !Number.isFinite(point.y);
    })) return null;
    return {
      nativeOrdinal: Number.isFinite(layer.nativeOrdinal) ? layer.nativeOrdinal : null,
      origin: origin,
      localQuad: localQuad,
      sceneQuad: sceneQuad,
      screenQuad: screenQuad
    };
  }

  function modeZeroActorGeometry(actor, previewState, actorProjection) {
    if (!actor || actor.renderPipeline !== 'mode-zero-registered-prepass-actor-camera' ||
        !previewState || !nativePerspectiveProjection(actorProjection) ||
        !nativePerspectiveProjection(previewState.registeredProjection)) return null;
    var sourcePoint = {
      x: Number(actor.x) || 0,
      // func_002AE654 projects the Actor's X/Z placement with a zero Y origin.
      // The Actor height at +0x120 enters the later matrix composition.
      y: 0,
      z: Number(actor.z) || 0
    };
    var registeredProjection = previewState.registeredProjection;
    var projected = projectPointFloat(sourcePoint, registeredProjection);
    if (!Number.isFinite(projected.x) || !Number.isFinite(projected.y)) return null;
    // func_0029CBDC writes framebuffer Y from the bottom edge. The editor's
    // ordinary projection helper uses the canvas top edge, so convert it here.
    var registeredScreenPoint = {
      x: projected.x,
      y: registeredProjection.screenHeight - projected.y
    };
    var centeredPoint = {
      x: registeredScreenPoint.x - registeredProjection.screenWidth / 2,
      y: registeredScreenPoint.y - registeredProjection.screenHeight / 2 +
        (Number(actor.y) || 0),
      z: 0
    };
    var channel = actor.sceneTransform || {};
    var scenePoint = transformModeZeroStagePoint(centeredPoint, channel, actorProjection);
    var normalization = actorCameraNormalization(actorProjection);
    var finalProjection = Object.assign({}, actorProjection, {
      // transformModeZeroStagePoint has already applied the Actor-camera
      // normalization, so this matrix consumes the composed scene point once.
      modelScale: 1
    });
    var channelScale = Number.isFinite(channel.uniformScale) ? channel.uniformScale : 1;
    var nativeActorScale = Number.isFinite(actor.nativeUniformScale)
      ? actor.nativeUniformScale
      : ((Number.isFinite(actor.uniformScale) ? actor.uniformScale : 1) /
        (channelScale || 1));
    var registeredScale = perspectivePixelsPerModelUnit(sourcePoint, registeredProjection);
    // func_002AE654 applies the Actor scale while measuring the registered-camera
    // basis. func_002AEB48 multiplies that basis by the same record scale again,
    // then by the scene channel and Actor-camera normalization.
    var actorPlaneScale = nativeActorScale * nativeActorScale * channelScale *
      registeredScale * normalization;
    return {
      sourcePoint: sourcePoint,
      registeredScreenPoint: registeredScreenPoint,
      centeredPoint: centeredPoint,
      scenePoint: scenePoint,
      projection: finalProjection,
      screenPoint: projectPoint(scenePoint, finalProjection),
      registeredScale: registeredScale,
      actorPlaneScale: actorPlaneScale,
      scale: actorPlaneScale * perspectivePixelsPerModelUnit(scenePoint, finalProjection)
    };
  }

  function spritePerspectiveScale(point, projection) {
    if (!projection || (projection.mode !== 'native-perspective-capture' &&
        projection.mode !== 'native-perspective-runtime')) return 1;
    var modelScale = Number(projection.modelScale);
    if (!Number.isFinite(modelScale) || modelScale <= 0) return 1;
    var targetDepth = vectorLength({
      x: projection.eye.x - projection.target.x,
      y: projection.eye.y - projection.target.y,
      z: projection.eye.z - projection.target.z
    });
    var pointDepth = projectionDepth(point, projection);
    if (!Number.isFinite(targetDepth) || !Number.isFinite(pointDepth) ||
        targetDepth <= 0 || pointDepth <= 0) return 1;
    return targetDepth / pointDepth;
  }

  function unprojectPoint(point, projection, nativeY) {
    if (projection.mode === 'native-perspective-capture' ||
        projection.mode === 'native-perspective-runtime') {
      var combined = nativeProjectionMatrix(projection);
      var normalizedX = point.x / projection.screenWidth * 2 - 1;
      var normalizedY = 1 - point.y / projection.screenHeight * 2;
      var modelY = (Number(nativeY) || 0) * projection.modelScale;
      var a00 = combined[0][0] - normalizedX * combined[0][3];
      var a01 = combined[2][0] - normalizedX * combined[2][3];
      var a10 = combined[0][1] - normalizedY * combined[0][3];
      var a11 = combined[2][1] - normalizedY * combined[2][3];
      var b0 = -modelY * (combined[1][0] - normalizedX * combined[1][3]) -
        (combined[3][0] - normalizedX * combined[3][3]);
      var b1 = -modelY * (combined[1][1] - normalizedY * combined[1][3]) -
        (combined[3][1] - normalizedY * combined[3][3]);
      var determinant = a00 * a11 - a01 * a10;
      if (Math.abs(determinant) < 0.0000001) {
        fail('Native Stage projection cannot place this actor on its current height plane.');
      }
      return {
        x: (b0 * a11 - a01 * b1) / determinant / projection.modelScale,
        y: Number(nativeY) || 0,
        z: (a00 * b1 - b0 * a10) / determinant / projection.modelScale
      };
    }
    var xAmount = (point.x - projection.left) / (projection.right - projection.left);
    var zAmount = (projection.bottom - point.y - (Number(nativeY) || 0) * 0.2) /
      (projection.bottom - projection.top);
    return {
      x: projection.xMin + xAmount * (projection.xMax - projection.xMin),
      y: Number(nativeY) || 0,
      z: projection.zMin + zAmount * (projection.zMax - projection.zMin)
    };
  }

  function actorColor(actor) {
    var text = String(actor.artSourceId || actor.id);
    var hash = 2166136261;
    for (var index = 0; index < text.length; index++) {
      hash ^= text.charCodeAt(index); hash = Math.imul(hash, 16777619);
    }
    return [90 + (hash >>> 16 & 95), 90 + (hash >>> 8 & 95), 90 + (hash & 95), 255];
  }

  function modulateSurface(output, color) {
    if (!color) return;
    var red = clamp(Number.isFinite(color.red) ? color.red : 255, 0, 255) / 255;
    var green = clamp(Number.isFinite(color.green) ? color.green : 255, 0, 255) / 255;
    var blue = clamp(Number.isFinite(color.blue) ? color.blue : 255, 0, 255) / 255;
    for (var offset = 0; offset < output.rgba.length; offset += 4) {
      output.rgba[offset] = Math.round(output.rgba[offset] * red);
      output.rgba[offset + 1] = Math.round(output.rgba[offset + 1] * green);
      output.rgba[offset + 2] = Math.round(output.rgba[offset + 2] * blue);
    }
  }

  function actorRenderMode(actor) {
    if (actor && Number.isFinite(actor.renderModeByte)) return actor.renderModeByte & 0xFF;
    if (actor && actor.source && Number.isFinite(actor.source.renderMode)) {
      return actor.source.renderMode & 0xFF;
    }
    return 0;
  }

  function actorHasRenderablePass(actor) {
    var mode = actorRenderMode(actor);
    return mode < 2 || (mode & 0x01) === 0;
  }

  function actorSpriteOpacity(actor) {
    // Even render modes include the native main-Actor pass. That pass supplies
    // alpha 255 and deliberately bypasses the Actor opacity byte. The flattened
    // editor sprite represents this main pass, not the earlier shadow passes.
    if ((actorRenderMode(actor) & 0x01) === 0) return 255;
    return Number.isFinite(actor.opacityByte) ? actor.opacityByte : 255;
  }

  function drawNativeActorLayers(output, frame, point, actorScale, camera, opacity, tint) {
    var bounds = null;
    frame.nativeLayers.forEach(function(layer) {
      var layerScaleX = Number.isFinite(layer.scaleX) ? layer.scaleX : 1;
      var layerScaleY = Number.isFinite(layer.scaleY) ? layer.scaleY : 1;
      var scaleX = actorScale * camera.scaleX * layerScaleX;
      var scaleY = actorScale * camera.scaleY * layerScaleY;
      var width = Math.max(1, Math.round(layer.width * scaleX));
      var height = Math.max(1, Math.round(layer.height * scaleY));
      var left = Math.round(point.x + layer.drawOffsetX * scaleX);
      var top = Math.round(point.y + layer.drawOffsetY * scaleY);
      blitScaled(output, layer, left, top, width, height, opacity, tint);
      var right = left + width;
      var bottom = top + height;
      if (!bounds) {
        bounds = { left: left, top: top, right: right, bottom: bottom };
      } else {
        bounds.left = Math.min(bounds.left, left);
        bounds.top = Math.min(bounds.top, top);
        bounds.right = Math.max(bounds.right, right);
        bounds.bottom = Math.max(bounds.bottom, bottom);
      }
    });
    return bounds;
  }

  function drawStagePropFrame(output, entry, point, scale, camera) {
    var frame = entry && entry.frame;
    if (!frame || !frame.rgba) return null;
    if (Array.isArray(frame.nativeLayers) && frame.nativeLayers.length) {
      return drawNativeActorLayers(output, frame, point, scale, camera, 255, null);
    }
    var width = Math.max(1, Math.round(frame.width * scale * camera.scaleX));
    var height = Math.max(1, Math.round(frame.height * scale * camera.scaleY));
    var anchorX = (Number.isFinite(frame.anchorX) ? frame.anchorX : 0) *
      scale * camera.scaleX;
    var anchorY = (Number.isFinite(frame.anchorY) ? frame.anchorY : 0) *
      scale * camera.scaleY;
    var left = Math.round(point.x - anchorX);
    var top = Math.round(point.y - anchorY);
    blitScaled(output, frame, left, top, width, height);
    return { left: left, top: top, right: left + width, bottom: top + height };
  }

  function renderOrthographicStageProps(output, entries, phase, camera) {
    (entries || []).filter(function(entry) {
      var placement = entry && entry.placement;
      return placement && placement.projection === 'native-320x240-orthographic' &&
        placement.depthPass === phase;
    }).forEach(function(entry) {
      var placement = entry.placement;
      var point = transformStagePoint({
        x: WIDTH / 2 + placement.x,
        y: HEIGHT / 2 - placement.y
      }, camera);
      drawStagePropFrame(output, entry, point, 1, camera);
    });
  }

  function nativePerspectiveProjection(projection) {
    return projection && (projection.mode === 'native-perspective-capture' ||
      projection.mode === 'native-perspective-runtime');
  }

  function drawFallbackActor(output, actor, point, selected) {
    var color = actorColor(actor);
    var outline = selected ? [255, 222, 128, 255] : [11, 14, 23, 235];
    var opacity = clamp(actorSpriteOpacity(actor), 0, 255) / 255;
    color[3] = Math.round(color[3] * opacity);
    outline[3] = Math.round(outline[3] * opacity);
    var stride = actor.activeMovementId ? actor.movementFrame % 4 : 0;
    var bob = actor.activeMovementId ? (stride === 1 || stride === 3 ? 1 : 0) :
      (actor.poseFrame % 16 >= 8 ? 1 : 0);
    var facingMatch = String(actor.facing || '').match(/native-(\d+)/);
    var facing = facingMatch ? Number(facingMatch[1]) : 0;
    var faceOffset = facing % 2 ? -2 : 2;
    point = { x: point.x, y: point.y - bob };
    fillRect(output, point.x - 5, point.y - 24, 11, 8, outline);
    fillRect(output, point.x - 4, point.y - 23, 9, 7, color);
    fillRect(output, point.x + faceOffset, point.y - 21, 2, 2,
      [245, 236, 190, Math.round(255 * opacity)]);
    fillRect(output, point.x - 8, point.y - 16, 17, 18, outline);
    fillRect(output, point.x - 7, point.y - 15, 15, 16, color);
    fillRect(output, point.x - 7 - (stride === 1 ? 2 : 0), point.y + 2, 6, 9, outline);
    fillRect(output, point.x + 2 + (stride === 3 ? 2 : 0), point.y + 2, 6, 9, outline);
    line(output, point.x - 10, point.y + 12, point.x + 10, point.y + 12,
      [0, 0, 0, Math.round(100 * opacity)]);
  }

  function movementPaths(document, projection, selectedActorId) {
    var paths = [];
    document.tracks.forEach(function(track) {
      if (track.type !== 'movement' || selectedActorId && track.actorId !== selectedActorId) return;
      track.clips.forEach(function(clip) {
        if (!clip.payload.from || !clip.payload.to) return;
        paths.push({
          clipId: clip.id,
          actorId: track.actorId,
          from: projectPoint(clip.payload.from, projection),
          to: projectPoint(clip.payload.to, projection)
        });
      });
    });
    return paths;
  }

  function renderFrame(document, previewState, options) {
    options = options || {};
    var output = surface(WIDTH, HEIGHT);
    fallbackBackground(output);
    var backgrounds = Array.isArray(options.backgrounds)
      ? options.backgrounds : (options.background ? [options.background] : []);
    var backgroundProjection = options.backgroundProjection ||
      document.background && document.background.projection || {};
    var projection = options.projection || computeProjection(document);
    if (projection.mode === 'native-perspective-runtime' &&
        projection.evidenceStatus === 'external-unresolved') {
      projection = computeProjection(document, previewState);
      projection.status = 'fit preview because the native launch Actor camera is unresolved';
      projection.evidenceStatus = 'preview-fallback';
    }
    var camera = cameraTransform(options.camera);
    var scenePropFrames = Array.isArray(options.scenePropFrames)
      ? options.scenePropFrames : [];
    var backgroundCamera = backgroundProjection.calibrationCamera
      ? relativeCameraTransform(camera, backgroundProjection.calibrationCamera) : camera;
    renderBackgrounds(output, backgrounds, backgroundProjection, projection,
      'base', backgroundCamera);
    renderOrthographicStageProps(output, scenePropFrames, 'far', backgroundCamera);
    var paths = movementPaths(document, projection, options.selectedActorId);
    paths = paths.map(function(path) {
      return {
        clipId: path.clipId,
        actorId: path.actorId,
        from: transformStagePoint(path.from, camera),
        to: transformStagePoint(path.to, camera)
      };
    });
    paths.forEach(function(path) {
      line(output, path.from.x, path.from.y, path.to.x, path.to.y, [255, 222, 128, 185]);
      fillRect(output, path.to.x - 2, path.to.y - 2, 5, 5, [255, 222, 128, 240]);
    });
    var hitRegions = [];
    var renderActors = previewState.actors.filter(function(actor) {
      return actor.visible && actorHasRenderablePass(actor) && actorSpriteOpacity(actor) > 0;
    });
    var renderQueue = renderActors.map(function(actor, index) {
      var modeZeroGeometry = modeZeroActorGeometry(actor, previewState, projection);
      return {
        kind: 'actor', actor: actor, geometry: modeZeroGeometry, order: index,
        depth: modeZeroGeometry
          ? projectionDepth(modeZeroGeometry.scenePoint, modeZeroGeometry.projection)
          : (nativePerspectiveProjection(projection)
          ? projectionDepth(actor, projection) : null
          )
      };
    });
    scenePropFrames.filter(function(entry) {
      return entry && entry.placement &&
        entry.placement.projection === 'native-actor-perspective';
    }).forEach(function(entry, index) {
      renderQueue.push({
        kind: 'stage-prop', entry: entry, order: renderActors.length + index,
        depth: nativePerspectiveProjection(projection)
          ? projectionDepth(entry.placement, projection) : null
      });
    });
    if (nativePerspectiveProjection(projection)) {
      renderQueue.sort(function(left, right) {
        var leftDepth = Number.isFinite(left.depth) ? left.depth : 0;
        var rightDepth = Number.isFinite(right.depth) ? right.depth : 0;
        return rightDepth - leftDepth || left.order - right.order;
      });
    }
    renderQueue.forEach(function(renderEntry) {
      if (renderEntry.kind === 'stage-prop') {
        var prop = renderEntry.entry.placement;
        var propPoint = transformStagePoint(projectPointFloat(prop, projection), camera);
        var propScale = nativePerspectiveProjection(projection)
          ? perspectivePixelsPerModelUnit(prop, projection) : 1;
        drawStagePropFrame(output, renderEntry.entry, propPoint,
          clamp(propScale, 0.05, 16), camera);
        return;
      }
      var actor = renderEntry.actor;
      var modeZeroGeometry = renderEntry.geometry;
      var point = modeZeroGeometry ? modeZeroGeometry.screenPoint : projectPointFloat(actor, projection);
      point = transformStagePoint(point, camera);
      var frame = options.actorFrames && options.actorFrames[actor.id];
      if (frame && frame.rgba) {
        var actorScale = modeZeroGeometry ? modeZeroGeometry.scale :
          (Number.isFinite(actor.uniformScale) ? actor.uniformScale : 1) *
            perspectivePixelsPerModelUnit(actor, projection);
        actorScale = clamp(actorScale, 0.05, 16);
        if (Array.isArray(frame.nativeLayers) && frame.nativeLayers.length) {
          var nativeBounds = drawNativeActorLayers(output, frame, point, actorScale, camera,
            actorSpriteOpacity(actor), actor.tint);
          if (nativeBounds) {
            hitRegions.push(Object.assign({ actorId: actor.id }, nativeBounds));
          }
          return;
        }
        var width = Math.max(1, Math.round(frame.width * actorScale * camera.scaleX));
        var height = Math.max(1, Math.round(frame.height * actorScale * camera.scaleY));
        var anchorX = (Number.isFinite(frame.anchorX)
          ? frame.anchorX : Math.floor(frame.width / 2)) * actorScale * camera.scaleX;
        var anchorY = (Number.isFinite(frame.anchorY) ? frame.anchorY : frame.height) *
          actorScale * camera.scaleY;
        var left = Math.round(point.x - anchorX);
        var top = Math.round(point.y - anchorY);
        blitScaled(output, frame, left, top, width, height,
          actorSpriteOpacity(actor), actor.tint);
        hitRegions.push({ actorId: actor.id, left: left, top: top,
          right: left + width, bottom: top + height });
      } else if (actor.poseProgramStatus !== 'empty-native-program') {
        drawFallbackActor(output, actor, point, actor.id === options.selectedActorId);
        hitRegions.push({ actorId: actor.id, left: point.x - 10, top: point.y - 25,
          right: point.x + 10, bottom: point.y + 13 });
      }
    });
    renderOrthographicStageProps(output, scenePropFrames, 'near', backgroundCamera);
    (options.effectFrames || []).forEach(function(effect) {
      var image = effect.image;
      if (!image || !image.rgba) return;
      var scale = Number.isFinite(effect.scale) ? clamp(effect.scale, 0.05, 16) : 1;
      var width = Math.max(1, Math.round(image.width * scale * camera.scaleX));
      var height = Math.max(1, Math.round(image.height * scale * camera.scaleY));
      var centerX = Number.isFinite(effect.x) ? effect.x : WIDTH / 2;
      var centerY = Number.isFinite(effect.y) ? effect.y : HEIGHT / 2;
      var center = transformStagePoint({ x: centerX, y: centerY }, camera);
      var anchorX = Number.isFinite(effect.anchorX)
        ? effect.anchorX * scale * camera.scaleX : width / 2;
      var anchorY = Number.isFinite(effect.anchorY)
        ? effect.anchorY * scale * camera.scaleY : height / 2;
      blitScaledRotated(output, image, center.x, center.y, width, height,
        anchorX, anchorY, effect.rotationDegrees);
    });
    var renderedSceneVignette = renderSceneVignette(
      output, options.sceneVignetteImage, options.sceneVignette,
      options.oversizedImageView, camera);
    renderBackgrounds(output, backgrounds, backgroundProjection, projection,
      'foreground', backgroundCamera);
    modulateSurface(output, options.colorModulation);
    (options.overlays || []).forEach(function(overlay) {
      fillRect(output, 0, 0, output.width, output.height, [
        clamp(Number(overlay.red) || 0, 0, 255),
        clamp(Number(overlay.green) || 0, 0, 255),
        clamp(Number(overlay.blue) || 0, 0, 255),
        clamp(Number(overlay.alpha) || 0, 0, 255)
      ]);
    });
    if (backgroundProjection.viewport) {
      applyViewportMask(output, backgroundProjection.viewport);
    }
    applyScreenTransitionMask(output, options.screenTransition);
    return {
      width: WIDTH,
      height: HEIGHT,
      rgba: output.rgba,
      projection: projection,
      camera: camera,
      screenTransition: options.screenTransition || null,
      sceneVignette: renderedSceneVignette,
      hitRegions: hitRegions,
      movementPaths: paths
    };
  }

  function hitTest(rendered, x, y) {
    for (var index = rendered.hitRegions.length - 1; index >= 0; index--) {
      var region = rendered.hitRegions[index];
      if (x >= region.left && x <= region.right && y >= region.top && y <= region.bottom) {
        return region.actorId;
      }
    }
    return null;
  }

  function paintCanvas(canvas, rendered, scale) {
    if (!canvas || typeof canvas.getContext !== 'function') fail('Stage canvas is unavailable.');
    scale = Number.isInteger(scale) && scale > 0 ? scale : 2;
    canvas.width = rendered.width;
    canvas.height = rendered.height;
    canvas.style.imageRendering = 'pixelated';
    canvas.style.width = rendered.width * scale + 'px';
    canvas.style.maxWidth = '100%';
    canvas.style.height = 'auto';
    var context = canvas.getContext('2d');
    var imageData = context.createImageData(rendered.width, rendered.height);
    imageData.data.set(rendered.rgba);
    context.putImageData(imageData, 0, 0);
  }

  OB64.cutsceneRenderer = {
    width: WIDTH,
    height: HEIGHT,
    RenderError: RenderError,
    computeProjection: computeProjection,
    projectPointFloat: projectPointFloat,
    projectPoint: projectPoint,
    perspectivePixelsPerModelUnit: perspectivePixelsPerModelUnit,
    spritePerspectiveScale: spritePerspectiveScale,
    transformModeZeroStagePoint: transformModeZeroStagePoint,
    modeZeroBackgroundGeometry: modeZeroBackgroundGeometry,
    modeZeroActorGeometry: modeZeroActorGeometry,
    unprojectPoint: unprojectPoint,
    untransformStagePoint: untransformStagePoint,
    movementPaths: movementPaths,
    renderFrame: renderFrame,
    hitTest: hitTest,
    paintCanvas: paintCanvas,
    blitScaled: blitScaled,
    blitProjectedQuad: blitProjectedQuad,
    buildSceneVignetteImage: buildSceneVignetteImage,
    renderSceneVignette: renderSceneVignette
  };
})(window.OB64);
