'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const ROM = path.join(ROOT,
  'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.module = undefined;
if (!window.crypto || !window.crypto.subtle) {
  Object.defineProperty(window, 'crypto', { value: crypto.webcrypto });
}
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');

vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js'
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

function normalizeV64(bytes) {
  const output = new Uint8Array(bytes);
  for (let offset = 0; offset < output.length; offset += 2) {
    const first = output[offset];
    output[offset] = output[offset + 1];
    output[offset + 1] = first;
  }
  return output;
}

function visiblePixels(state, source, childOrdinal) {
  const child = OB64.animationArt.displayChild(state, source.key, childOrdinal);
  const alpha = source.editable ? child.intensity : child.alpha;
  let count = 0;
  for (const value of alpha) if (value) count++;
  return count;
}

(async function run() {
  const z64 = normalizeV64(fs.readFileSync(ROM));
  const rom = { z64, layout: { id: 'us-rev0' } };
  await OB64.art.initialize(rom);
  const state = rom.art.animations;
  if (!state.supported) throw new Error(state.unavailableReason);

  const variants = new Map();
  for (const animation of state.specs) {
    const variantKey = animation.spec.classId + '|' + animation.spec.variantLabel;
    let row = variants.get(variantKey);
    if (!row) {
      row = {
        classId: animation.spec.classId,
        className: animation.spec.className,
        variantLabel: animation.spec.variantLabel,
        selectedBodyChild: animation.spec.selectedBodyChild,
        sequences: 0,
        bodyBindings: new Map(),
        equipmentBindings: new Set(),
        elementBindings: new Set(),
        noBodySequences: 0
      };
      variants.set(variantKey, row);
    }
    row.sequences++;
    let hasBody = false;
    for (const source of Object.values(animation.artByKey)) {
      if (source.sourceRole === 'body') {
        hasBody = true;
        const childOrdinal = animation.frames.flatMap(frame => frame.layers)
          .find(layer => layer.sourceKey === source.key).selectedChildOrdinal;
        const identity = source.key + '#' + childOrdinal;
        if (!row.bodyBindings.has(identity)) {
          row.bodyBindings.set(identity, {
            bindingId: source.bindingId,
            childOrdinal,
            childCount: source.sprite.childCount,
            visiblePixels: visiblePixels(state, source, childOrdinal)
          });
        }
      } else if (source.sourceRole === 'equipment') {
        row.equipmentBindings.add(source.bindingId);
      } else if (source.sourceRole === 'element-effect') {
        row.elementBindings.add(source.bindingId);
      }
    }
    if (!hasBody) row.noBodySequences++;
  }

  const output = [...variants.values()].map(row => {
    const body = [...row.bodyBindings.values()];
    return {
      classId: '0x' + row.classId.toString(16).toUpperCase().padStart(2, '0'),
      className: row.className,
      variantLabel: row.variantLabel,
      selectedBodyChild: row.selectedBodyChild,
      sequences: row.sequences,
      bodyBindings: body.length,
      blankBodyBindings: body.filter(binding => !binding.visiblePixels).length,
      visibleBodyBindings: body.filter(binding => binding.visiblePixels).length,
      noBodySequences: row.noBodySequences,
      equipmentBindings: row.equipmentBindings.size,
      elementBindings: row.elementBindings.size
    };
  });
  const suspect = output.filter(row =>
    row.blankBodyBindings || row.noBodySequences || !row.visibleBodyBindings);
  console.log(JSON.stringify({
    variantCount: output.length,
    suspectCount: suspect.length,
    affectedClassCount: new Set(suspect.map(row => row.classId)).size,
    suspects: suspect
  }, null, 2));
})().catch(error => {
  console.error(error && error.stack ? error.stack : error);
  process.exitCode = 1;
});
