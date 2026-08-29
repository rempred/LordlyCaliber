'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

function parseColor(value) {
  const hex = String(value).match(/^#([0-9a-f]{6})$/i);
  if (hex) {
    return [0, 2, 4].map(offset => parseInt(hex[1].slice(offset, offset + 2), 16))
      .concat(255);
  }
  const color = String(value).match(
    /^rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?\)$/);
  if (!color) throw new Error(`unsupported test color ${value}`);
  return [Number(color[1]), Number(color[2]), Number(color[3]),
    color[4] === undefined ? 255 : Math.round(Number(color[4]) * 255)];
}

class FakeContext {
  constructor(canvas) {
    this.canvas = canvas;
    this.fillStyle = '#000000';
    this.imageSmoothingEnabled = true;
  }

  pixels() {
    const size = this.canvas.width * this.canvas.height * 4;
    if (!this.canvas.pixels || this.canvas.pixels.length !== size) {
      this.canvas.pixels = new Uint8ClampedArray(size);
    }
    return this.canvas.pixels;
  }

  createImageData(width, height) {
    return { width, height, data: new Uint8ClampedArray(width * height * 4) };
  }

  putImageData(imageData, x, y) {
    assert.strictEqual(x, 0);
    assert.strictEqual(y, 0);
    assert.strictEqual(imageData.width, this.canvas.width);
    assert.strictEqual(imageData.height, this.canvas.height);
    this.canvas.pixels = new Uint8ClampedArray(imageData.data);
  }

  clearRect(x, y, width, height) {
    this.paintRect(x, y, width, height, [0, 0, 0, 0], true);
  }

  fillRect(x, y, width, height) {
    this.paintRect(x, y, width, height, parseColor(this.fillStyle), false);
  }

  paintRect(x, y, width, height, source, replace) {
    const left = Math.max(0, Math.floor(Math.min(x, x + width)));
    const right = Math.min(this.canvas.width, Math.ceil(Math.max(x, x + width)));
    const top = Math.max(0, Math.floor(Math.min(y, y + height)));
    const bottom = Math.min(this.canvas.height, Math.ceil(Math.max(y, y + height)));
    const pixels = this.pixels();
    for (let row = top; row < bottom; row++) {
      for (let column = left; column < right; column++) {
        const offset = (row * this.canvas.width + column) * 4;
        if (replace || source[3] === 255) {
          pixels.set(source, offset);
          continue;
        }
        const sourceAlpha = source[3] / 255;
        const targetAlpha = pixels[offset + 3] / 255;
        const outputAlpha = sourceAlpha + targetAlpha * (1 - sourceAlpha);
        for (let channel = 0; channel < 3; channel++) {
          pixels[offset + channel] = outputAlpha
            ? Math.round((source[channel] * sourceAlpha +
              pixels[offset + channel] * targetAlpha * (1 - sourceAlpha)) /
              outputAlpha)
            : 0;
        }
        pixels[offset + 3] = Math.round(outputAlpha * 255);
      }
    }
  }

  save() {}
  restore() {}
  setLineDash() {}
  strokeRect() {}
  beginPath() {}
  moveTo() {}
  lineTo() {}
  stroke() {}
}

class FakeCanvas {
  constructor() {
    this.width = 0;
    this.height = 0;
    this.context = new FakeContext(this);
  }

  getContext(type) {
    assert.strictEqual(type, '2d');
    return this.context;
  }
}

function pixel(canvas, x, y) {
  const offset = (y * canvas.width + x) * 4;
  const pixels = canvas.pixels || canvas.context.pixels();
  return Array.from(pixels.slice(offset, offset + 4));
}

global.window = global;
global.document = {
  createElement(tag) {
    if (tag === 'canvas') return new FakeCanvas();
    return {
      classList: { add() {} },
      addEventListener() {},
      setAttribute() {},
    };
  },
};
global.OB64 = {};

for (const filename of ['art.js', 'art-ui.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, filename), 'utf8'), {
    filename,
  });
}
OB64.animationArt = {};
vm.runInThisContext(fs.readFileSync(path.join(EDITOR, 'animation-ui.js'), 'utf8'), {
  filename: 'animation-ui.js',
});

const ui = {};
assert.strictEqual(OB64.artUI.previewBackgroundMode(ui), 'checkerboard');
assert.strictEqual(OB64.artUI.togglePreviewBackground(ui), 'white');
assert.strictEqual(OB64.artUI.previewBackgroundMode(ui), 'white');
assert.strictEqual(OB64.artUI.togglePreviewBackground(ui), 'checkerboard');

const transparentWord = OB64.art.rgba5551Word(31, 0, 31, false);
const opaqueWord = OB64.art.rgba5551Word(0, 31, 0, true);
assert.deepStrictEqual(Array.from(OB64.artUI.rgbaPixelsForWords(
  new Uint16Array([transparentWord, opaqueWord]))), [
  255, 0, 255, 0,
  0, 255, 0, 255,
]);

const iconPng = OB64.artUI.nativePngCanvas(
  new Uint16Array([transparentWord, opaqueWord]), 2, 1);
assert.deepStrictEqual(pixel(iconPng, 0, 0), [255, 0, 255, 0]);
assert.deepStrictEqual(pixel(iconPng, 1, 0), [0, 255, 0, 255]);

const whiteIconPreview = new FakeCanvas();
OB64.artUI.drawWords(whiteIconPreview, new Uint16Array([transparentWord]),
  1, 1, 2, null, 'white');
assert.deepStrictEqual(pixel(whiteIconPreview, 0, 0), [255, 255, 255, 255]);

const checkerIconPreview = new FakeCanvas();
OB64.artUI.drawWords(checkerIconPreview, new Uint16Array([transparentWord]),
  1, 1, 2, null, 'checkerboard');
assert.strictEqual(pixel(checkerIconPreview, 0, 0)[3], 255);
assert.notDeepStrictEqual(pixel(checkerIconPreview, 0, 0), [255, 255, 255, 255]);

const animationPixels = new Uint8ClampedArray([
  10, 20, 30, 0,
  40, 50, 60, 17,
  70, 80, 90, 255,
]);
const animationPngPixels = new FakeCanvas();
OB64.animationUI.paintPixels(animationPngPixels, 3, 1,
  animationPixels, 1, 'transparent');
assert.deepStrictEqual(Array.from(animationPngPixels.pixels),
  Array.from(animationPixels));

const animationWebmPixels = new FakeCanvas();
OB64.animationUI.paintPixels(animationWebmPixels, 3, 1,
  animationPixels, 4, 'transparent');
assert.deepStrictEqual(pixel(animationWebmPixels, 0, 0), [0, 0, 0, 0]);
assert.deepStrictEqual(pixel(animationWebmPixels, 4, 0), [40, 50, 60, 17]);
assert.deepStrictEqual(pixel(animationWebmPixels, 8, 0), [70, 80, 90, 255]);

const emptyFrame = { sequenceIndex: 0, token: 0, ticks: 1, layers: [] };
const animation = {
  key: 'transparency-test',
  canvas: { width: 2, height: 1, originX: 0, originY: 0 },
  frames: [emptyFrame],
  artByKey: {},
};
const animationState = { animations: { editRevision: 0 } };
const framePng = OB64.animationUI.framePngCanvas(
  animation, emptyFrame, animationState, null);
assert.deepStrictEqual(Array.from(framePng.pixels), new Array(8).fill(0));

const whiteFramePreview = new FakeCanvas();
OB64.animationUI.drawFrame(whiteFramePreview, animation, emptyFrame,
  animationState.animations, 1, null, null, false, null, 'white');
assert.deepStrictEqual(pixel(whiteFramePreview, 0, 0), [255, 255, 255, 255]);

const checkerFramePreview = new FakeCanvas();
OB64.animationUI.drawFrame(checkerFramePreview, animation, emptyFrame,
  animationState.animations, 1, null, null, false, null, 'checkerboard');
assert.strictEqual(pixel(checkerFramePreview, 0, 0)[3], 255);
assert.notDeepStrictEqual(pixel(checkerFramePreview, 0, 0),
  pixel(checkerFramePreview, 1, 0));

const timeline = {
  entries: [{ frame: emptyFrame, frameIndex: 0 }],
};
const checkerSurfaces = OB64.animationUI.animationPreviewSurfaces(
  animationState, animation, timeline, null, 'checkerboard');
const whiteSurfaces = OB64.animationUI.animationPreviewSurfaces(
  animationState, animation, timeline, null, 'white');
assert.notStrictEqual(checkerSurfaces[0], whiteSurfaces[0]);
assert.notDeepStrictEqual(pixel(checkerSurfaces[0], 0, 0),
  pixel(whiteSurfaces[0], 0, 0));
assert.strictEqual(OB64.animationUI.animationPreviewSurfaces(
  animationState, animation, timeline, null, 'white')[0], whiteSurfaces[0]);
const webmSurfaces = OB64.animationUI.animationWebmSurfaces(
  animationState, animation, timeline, null);
assert.notStrictEqual(webmSurfaces[0], whiteSurfaces[0]);
assert.deepStrictEqual(Array.from(webmSurfaces[0].context.pixels()),
  new Array(webmSurfaces[0].width * webmSurfaces[0].height * 4).fill(0));

console.log('Art export transparency tests passed.');
