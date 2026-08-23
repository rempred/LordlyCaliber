'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const EXTRACTED = path.resolve(EDITOR, '..', 'ob64_all');
const MASTER = path.resolve(EDITOR, '..', 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of [
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-njpg.js', 'cutscene-assets.js',
  'art.js'
]) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'), { filename: file });
}

function sha256(input) {
  return crypto.createHash('sha256').update(input).digest('hex').toUpperCase();
}

function normalizeV64(raw) {
  const output = new Uint8Array(raw.length);
  for (let index = 0; index < raw.length; index += 2) {
    output[index] = raw[index + 1];
    output[index + 1] = raw[index];
  }
  return output;
}

let renderable = 0;
let explicitUnsupported = 0;
const z64 = normalizeV64(fs.readFileSync(MASTER));
const assetById = new Map(OB64.cutsceneData.imageAssets.map(asset => [asset.assetId, asset]));

function sourceBytes(asset) {
  let bytes;
  if (asset.sourceKind === 'section-c-njpg') {
    bytes = z64.slice(asset.source.z64Start, asset.source.z64EndExclusive);
    assert.strictEqual(sha256(bytes), asset.source.storedSha256,
      asset.assetId + ' normalized-ROM preimage');
  } else if (asset.sourceKind === 'rom-resource') {
    const resource = asset.source.compressionKind === 'ob64-custom-lz'
      ? OB64.art.readCompressedResource(z64, asset.source.resourceKey)
      : OB64.art.readResource(z64, asset.source.resourceKey);
    assert.strictEqual(sha256(resource.stored), asset.source.storedSha256,
      asset.assetId + ' compressed normalized-ROM preimage');
    bytes = asset.source.compressionKind === 'ob64-custom-lz'
      ? resource.decoded : resource.stored;
    assert.strictEqual(sha256(bytes), asset.source.decodedSha256,
      asset.assetId + ' decoded ROM-resource preimage');
  } else {
    const directory = path.join(EXTRACTED, 'e' + asset.archiveIndex);
    const files = fs.readdirSync(directory, { withFileTypes: true })
      .filter(entry => entry.isFile());
    assert.strictEqual(files.length, 1, asset.assetId + ' decoded-file count');
    bytes = new Uint8Array(fs.readFileSync(path.join(directory, files[0].name)));
    assert.strictEqual(sha256(bytes), asset.source.decodedSha256,
      asset.assetId + ' extracted preimage');
  }
  return bytes;
}

function parseAsset(asset) {
  if (asset.compound) {
    return OB64.cutsceneAssets.composeCompoundImageAsset(asset,
      asset.compound.members.map(member => {
        const memberAsset = assetById.get(member.assetId);
        assert(memberAsset, asset.assetId + ' missing compound member ' + member.assetId);
        return {
          assetId: member.assetId,
          image: OB64.cutsceneAssets.parseImageAsset(sourceBytes(memberAsset), memberAsset),
        };
      }));
  }
  return OB64.cutsceneAssets.parseImageAsset(sourceBytes(asset), asset);
}

for (const asset of OB64.cutsceneData.imageAssets) {
  let parsed;
  try {
    parsed = parseAsset(asset);
  } catch (error) {
    if (!asset.renderable) {
      assert(asset.unsupportedReason.length > 0);
      assert(error.message.length > 0, asset.assetId + ' bounded parser error');
      explicitUnsupported++;
      continue;
    }
    throw new Error(asset.assetId + ' parser failure: ' + error.message);
  }
  assert.strictEqual(parsed.width, asset.width, asset.assetId + ' width');
  assert.strictEqual(parsed.height, asset.height, asset.assetId + ' height');
  assert.strictEqual(parsed.renderable, asset.renderable, asset.assetId + ' renderability');
  if (parsed.renderable) {
    assert.strictEqual(parsed.rgba.length, asset.width * asset.height * 4);
    const repeated = parseAsset(asset);
    assert.strictEqual(sha256(parsed.rgba), sha256(repeated.rgba),
      asset.assetId + ' deterministic pixels');
    renderable++;
  } else {
    assert(parsed.warnings.length > 0, asset.assetId + ' bounded unsupported explanation');
    assert(asset.unsupportedReason.length > 0);
    explicitUnsupported++;
  }
}

const graduationBaseAsset = assetById.get('mode2-environment:00236B58');
assert(graduationBaseAsset, 'Graduation native environment resource must be catalogued');
const graduationBase = parseAsset(graduationBaseAsset);
assert.deepStrictEqual([
  graduationBase.width, graduationBase.height,
  graduationBase.originX, graduationBase.originY,
  graduationBase.reference.x, graduationBase.reference.y,
], [495, 383, -229, -235, 277, 235]);
assert.strictEqual(sha256(graduationBase.rgba),
  '84CEB8DDF5ED6508BB820D4DBC3AB58A02FDEBB89E7D6660C94CD2109B728976',
  'native coefficient order, RSP scale, and color convention must remain stable');

// These RGB555 samples come from the retail decoder output saved in the
// Graduation runtime state. A three-level tolerance covers its fixed-point
// IDCT/color rounding while rejecting the former green, transposed preview.
const graduationNativeSamples = [
  [10, 10, 6, 2, 3], [100, 20, 11, 2, 3], [250, 50, 5, 4, 4],
  [400, 50, 5, 4, 4], [50, 150, 10, 11, 11], [200, 180, 19, 12, 6],
  [350, 180, 10, 10, 9], [480, 200, 1, 1, 1], [20, 300, 7, 5, 4],
  [150, 300, 8, 9, 14], [300, 300, 14, 8, 6], [450, 350, 16, 12, 6],
];
graduationNativeSamples.forEach(([x, y, red, green, blue]) => {
  const offset = (y * graduationBase.width + x) * 4;
  [red, green, blue].forEach((expected, channel) => {
    const actual = Math.round(graduationBase.rgba[offset + channel] * 31 / 255);
    assert(Math.abs(actual - expected) <= 3,
      'Graduation native decoder sample ' + x + ',' + y + ' channel ' + channel);
  });
});

assert.strictEqual(renderable, OB64.cutsceneData.counts.renderableImageAssets);
assert.strictEqual(renderable + explicitUnsupported, 366);
console.log('PASS all 366 catalogued image assets parse deterministically or report an explicit unsupported reason');
