'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const ROM = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.module = undefined;
if (!window.crypto || !window.crypto.subtle) {
  Object.defineProperty(window, 'crypto', { value: crypto.webcrypto });
}
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');

vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of ['art.js', 'animation-art.js', 'animation-ui.js']) {
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

function u32(bytes, offset) {
  return ((bytes[offset] * 0x1000000) + (bytes[offset + 1] << 16) +
    (bytes[offset + 2] << 8) + bytes[offset + 3]) >>> 0;
}

function rgb555Word(red, green, blue) {
  return (red << 11) | (green << 6) | (blue << 1) | 1;
}

(async function run() {
  assert.strictEqual(OB64.animationUI.normalizeIntensity(undefined), 15);
  assert.strictEqual(OB64.animationUI.normalizeIntensity(-1), 15);
  assert.strictEqual(OB64.animationUI.normalizeIntensity(16), 15);
  assert.strictEqual(OB64.animationUI.normalizeIntensity(7), 7);
  assert.strictEqual(OB64.animationUI.intensityColorCss(rgb555Word(31, 0, 0), 15),
    'rgba(255,0,0,1.0000)');
  assert.strictEqual(OB64.animationUI.intensityColorCss(rgb555Word(31, 0, 0), 0),
    'rgba(255,0,0,0.0000)');
  assert.notStrictEqual(
    OB64.animationUI.intensityColorCss(rgb555Word(31, 0, 0), 7),
    OB64.animationUI.intensityColorCss(rgb555Word(31, 0, 0), 15));
  const hueFixture = new Uint16Array([
    rgb555Word(0, 0, 31), rgb555Word(31, 0, 0), rgb555Word(0, 31, 0),
    rgb555Word(0, 0, 0), rgb555Word(31, 31, 31), rgb555Word(16, 15, 15),
    rgb555Word(10, 0, 0),
  ]);
  assert.deepStrictEqual(OB64.animationUI.paletteHueOrder(hueFixture),
    [3, 5, 4, 6, 1, 2, 0]);

  const z64 = normalizeV64(fs.readFileSync(ROM));
  const rom = { z64, layout: { id: 'us-rev0' } };
  await OB64.art.initialize(rom);

  assert.strictEqual(rom.art.supported, true);
  assert.strictEqual(rom.art.animations.supported, true,
    rom.art.animations.unavailableReason);
  const corpusFixtures = {
    'fighter-slash': {
      frames: [[0x11, 6], [0x12, 14], [0x13, 2], [0x14, 2], [0x15, 2],
        [0x16, 4], [0x17, 4], [0x18, 2], [0x19, 8]],
      layers: 57, sources: 42, weaponSources: 7, weaponChildren: 17,
      childCounts: [1, 4, 17],
      canvas: { originX: -39, originY: -53, endX: 37, endY: 5, width: 76, height: 58 },
      pose: [558, 606, 15, 48],
      hashes: [
        '08EF5CA1334F2D2151173914FFAF681A952B2EE0A34141A27CF26923A185D5DC',
        '05310DB11AC058D2F41D218F4C50A9BF12DF3FA7329F5013BCF06D5C283E0F2B',
        'EF25E5E86CC2AF360A6403E8439F130368E615508AF84B968EC624AB60C9092C',
        '4F0254EB6B8D9660C54ADDE7F7EE5F168CBFDD90391B8284F7FC5926C08670D4',
        '73DF0836BB54AE1BF4D8C95E84E4E5F41AC96B9B1F68BB1E6E8158DF81F110A5',
        '2D0DF77E9333D38788FFEC3AEF374D30F801D2C998223FEBBFBA5AAA88609837',
        'E827B671BEAC9B4F82DE47C9443D49A3496D5F35E9FD72089532038069F91F52',
        'FAED139A22C29635D799970B0CDBA00F6F215EA7279D86DDE046ED904077A35B',
        'E01488A687C52D0E170E8C8B780DF236135D043E8586DAF577B6A4EDC13C79EC',
      ],
    },
    'soldier-thrust': {
      frames: [[0x00, 8], [0x01, 12], [0x02, 4], [0x02, 6],
        [0x03, 2], [0x03, 4], [0x03, 20], [0x04, 8]],
      layers: 8, sources: 5, weaponSources: 0, weaponChildren: 0,
      childCounts: [2],
      canvas: { originX: -25, originY: -27, endX: 15, endY: 5, width: 40, height: 32 },
      pose: [614, 663, 15, 49],
      hashes: [
        '008106A1BC30E74B5D9046F6CD95A8812B68860B4CE0094199496FD57AEC9E3B',
        '7F975FCBBBDB1A280892777A03B8118703EBB034F7CB1530928BF4832A96335F',
        'AC8239B0C79CC0EC61329D95F9298554A74D5051737703B4DB987A69E6EDC477',
        'AC8239B0C79CC0EC61329D95F9298554A74D5051737703B4DB987A69E6EDC477',
        '7F0A59D33797F57010C98E7B07841C2633FDBD56215881DA476FC7400911DFDB',
        '7F0A59D33797F57010C98E7B07841C2633FDBD56215881DA476FC7400911DFDB',
        '7F0A59D33797F57010C98E7B07841C2633FDBD56215881DA476FC7400911DFDB',
        'E9805C12138F50587EC9773AFC9451082C822D4CC9D5734B1B2FC3053FD2B080',
      ],
    },
    'berserker-strike': {
      frames: [[0x11, 8], [0x12, 8], [0x13, 24], [0x15, 4], [0x17, 4],
        [0x19, 4], [0x1B, 4], [0x1D, 4], [0x1D, 2], [0x1E, 6],
        [0x1F, 6], [0x20, 8], [0x21, 8]],
      layers: 45, sources: 37, weaponSources: 12, weaponChildren: 12,
      childCounts: [1, 3, 12],
      canvas: { originX: -39, originY: -57, endX: 47, endY: 10, width: 86, height: 67 },
      pose: [545, 611, 21, 66],
      hashes: [
        '29582EAEF5886986BE8AA5DB5839C355DFDFEC2ECF6DAA942E07E161E3D40A3D',
        '2A58E44DEB7327BF6BE5D47D1BD23CD97150EA1B09BCB5AEA0418E9925C9EBDE',
        '732730E52A72135C1C5CF704B9FD0DB8591A475F5626AE065313F6BA78F8CEB2',
        '52EED79B4D4DA114ED17158214BE3D59C7B6851DC7053AC8836F39C110903736',
        '1D34D4B649EC96C29CF3D95B2033BAB9317F068CA18D4AD11B7C4A7A52D59C10',
        '5FFDFD42860CD9BC7BAEA3CD41BDA07FFC570F563E8920D6890B2E0C5296CD0F',
        '4D046D2726FBD420B45512C0A738B20BD14F6C73F1472AADE9A8D78C3F477937',
        'E1332F1480344023A681990C558C2E7DC0551B8761B6B941CB6F5BC1C6963926',
        'E1332F1480344023A681990C558C2E7DC0551B8761B6B941CB6F5BC1C6963926',
        'EEF928A7E2D3BDFF6846D8781D81562AA538C1145ABEA46F41CACEEBF95C2701',
        'BEED0AE6637E7D29A2DC6326FEF521078AE02D462F55218450323F70510202E0',
        '2E3488FCFCA5CB5448A7F37AB6B769C6D7828BF8E5A71DFBAB8E4FF00F3F5DF1',
        '5EDF8CCA2EF2E582D1AF8E06054531B5132898DAAC0738744DAEFBAC80FFA2C9',
      ],
    },
    'black-knight-cleave': {
      frames: [[0x1E, 6], [0x1F, 8], [0x20, 20], [0x21, 6], [0x23, 8],
        [0x25, 4], [0x27, 4], [0x28, 4], [0x2A, 4], [0x29, 4],
        [0x2B, 8], [0x2C, 8], [0x2D, 8], [0x2E, 6], [0x2F, 6]],
      layers: 44, sources: 27, weaponSources: 8, weaponChildren: 13,
      childCounts: [1, 3, 13],
      canvas: { originX: -46, originY: -55, endX: 42, endY: 6, width: 88, height: 61 },
      pose: [568, 644, 24, 76],
      hashes: [
        '9A6188802E80F3BEDA4D5560430F77C3E619B44FCC1402308C7E88D7D045DA4A',
        '0058E370EB9D0ABADD5860C7872E23259B5C15BF8AB9F4E302E69AA8CE650C22',
        '39F549E4F5E4975CF6F344596A643822ADC54E1B6A04EFF7590306A629F62BE2',
        '91F0D6F9DF1D7E56E9E317F56D969451ECAEEE38E8E62AB41E969475A555AC31',
        '17EF484CB5263D2159ABBC1E45EA74F59E14A34FC1F1DA4970AF42CBC2CDDDCC',
        '799348302B468C67D18026F19F4913B7FE3D1323A2D3A07A70DD5CC78ADCBC09',
        '7F9139CA9E6B88DD924CCAD640FE227B030F3A1160947C90BE1156103A3B8553',
        '5FCB7AF25B987586C3E657B5A0B52572066DE10A4A83DF81FEF83CD8027241FA',
        '91EABDEF200107B48E4D7E507BB705E03193D1E00CE0610367CC2EEC7775CD1D',
        '77999669839CAF859893CB3CD9F70152DD3ACFDB9375C28FB944AAE8738A2727',
        'C22FD399AAF1389BC6E4E26F348B775BE21365437270E8B6240553C584B902F6',
        '9943BCF4DA98EAB1F796C7F9ABD56780B9F97CE0CF07576F834B959A15985067',
        '81700E6914274D5844BC54A68470A55FECBBD655EA732630701A83BAAC0B6EC1',
        'CA7AF17C3390137396BD540683591847F581FBBA22D95832DF0DBF30F93A47F4',
        'E9D4E3C643E480EB0A76C5FF526F2C0A47941D66DB1F0D46579595E3737FC940',
      ],
    },
    'fighter-slash-blocked': {
      frames: [[0x11, 6], [0x12, 10], [0x12, 4], [0x13, 2], [0x14, 4],
        [0x14, 2], [0x14, 2], [0x13, 24], [0x11, 8]],
      layers: 54, sources: 24, weaponSources: 4, weaponChildren: 17,
      childCounts: [4, 17],
      canvas: { originX: -39, originY: -44, endX: 23, endY: 5, width: 62, height: 49 },
      pose: [606, 650, 14, 44],
      hashes: [
        '22F5B4310565DA8C12DE13B3AB44CFA0F20AE360123C19D00065C8721F994A38',
        'C9D966C2B35D457A3839D397EBD56513D4985745BF1A5BC0D01351B6FB38281A',
        'C9D966C2B35D457A3839D397EBD56513D4985745BF1A5BC0D01351B6FB38281A',
        '412646CBAFB04AB4A595078D4D801F597C503A19BF9C6B1DD4BE7055ACD76799',
        '5E6D107BFA3C09F18DB5B460729DC3A07CFAEE06A3EE46FDC11BE243E3D7466F',
        '5E6D107BFA3C09F18DB5B460729DC3A07CFAEE06A3EE46FDC11BE243E3D7466F',
        '5E6D107BFA3C09F18DB5B460729DC3A07CFAEE06A3EE46FDC11BE243E3D7466F',
        '412646CBAFB04AB4A595078D4D801F597C503A19BF9C6B1DD4BE7055ACD76799',
        '22F5B4310565DA8C12DE13B3AB44CFA0F20AE360123C19D00065C8721F994A38',
      ],
    },
    'soldier-thrust-blocked': {
      frames: [[0x00, 8], [0x01, 12], [0x02, 4], [0x03, 4], [0x03, 2],
        [0x03, 2], [0x02, 4], [0x01, 4], [0x01, 4], [0x01, 24], [0x00, 4]],
      layers: 11, sources: 4, weaponSources: 0, weaponChildren: 0,
      childCounts: [2],
      canvas: { originX: -23, originY: -27, endX: 15, endY: 5, width: 38, height: 32 },
      pose: [663, 725, 19, 62],
      hashes: [
        'D80FA362400C5E5579AF4CAF487944B5E5051C478B07783BFA355BB8634FC753',
        'FB2C216D76E613B12E5A145063D7BF7517B474EE75FEA6B40247E0043241C285',
        '3AAD0EE3CF9E1959F333E3A844BCDB4747A4FB4764722527DB90766DD50C6D25',
        '095793478C172AA7FC2553CC3B3ABD3CAFCF2FCD74AE85D1F6E3D40197F0D9FE',
        '095793478C172AA7FC2553CC3B3ABD3CAFCF2FCD74AE85D1F6E3D40197F0D9FE',
        '095793478C172AA7FC2553CC3B3ABD3CAFCF2FCD74AE85D1F6E3D40197F0D9FE',
        '3AAD0EE3CF9E1959F333E3A844BCDB4747A4FB4764722527DB90766DD50C6D25',
        'FB2C216D76E613B12E5A145063D7BF7517B474EE75FEA6B40247E0043241C285',
        'FB2C216D76E613B12E5A145063D7BF7517B474EE75FEA6B40247E0043241C285',
        'FB2C216D76E613B12E5A145063D7BF7517B474EE75FEA6B40247E0043241C285',
        'D80FA362400C5E5579AF4CAF487944B5E5051C478B07783BFA355BB8634FC753',
      ],
    },
    'berserker-strike-blocked': {
      frames: [[0x11, 8], [0x12, 8], [0x13, 22], [0x13, 2], [0x14, 4],
        [0x14, 2], [0x14, 2], [0x13, 4], [0x13, 4], [0x13, 12],
        [0x12, 8], [0x11, 8]],
      layers: 36, sources: 9, weaponSources: 4, weaponChildren: 12,
      childCounts: [3, 12],
      canvas: { originX: -20, originY: -46, endX: 47, endY: 8, width: 67, height: 54 },
      pose: [611, 696, 25, 85],
      hashes: [
        'C52EA4ECA6F00E08FA5122FC20BE93014FCBCE323147868666D002C2EB025715',
        'B47CD3B7B471C8C04C860781E4F9A3D2A00E90092D7BFF9A520B7CD95AC73E55',
        'F7FAF631D94B010A4FA9BBA53DD62EF141C4B5F19D24C7675A5252AAA2624B7E',
        'F7FAF631D94B010A4FA9BBA53DD62EF141C4B5F19D24C7675A5252AAA2624B7E',
        '87541ADD1A0E4FC712D434A33C62712935F90DFD5DBD6D0CBA061F9276AF8954',
        '87541ADD1A0E4FC712D434A33C62712935F90DFD5DBD6D0CBA061F9276AF8954',
        '87541ADD1A0E4FC712D434A33C62712935F90DFD5DBD6D0CBA061F9276AF8954',
        'F7FAF631D94B010A4FA9BBA53DD62EF141C4B5F19D24C7675A5252AAA2624B7E',
        'F7FAF631D94B010A4FA9BBA53DD62EF141C4B5F19D24C7675A5252AAA2624B7E',
        'F7FAF631D94B010A4FA9BBA53DD62EF141C4B5F19D24C7675A5252AAA2624B7E',
        'B47CD3B7B471C8C04C860781E4F9A3D2A00E90092D7BFF9A520B7CD95AC73E55',
        'C52EA4ECA6F00E08FA5122FC20BE93014FCBCE323147868666D002C2EB025715',
      ],
    },
    'black-knight-cleave-blocked': {
      frames: [[0x1E, 6], [0x1F, 8], [0x20, 20], [0x21, 8],
        [0x22, 4], [0x22, 2], [0x22, 2], [0x21, 20]],
      layers: 18, sources: 10, weaponSources: 4, weaponChildren: 13,
      childCounts: [3, 13],
      canvas: { originX: -24, originY: -51, endX: 38, endY: 6, width: 62, height: 57 },
      pose: [644, 690, 15, 46],
      hashes: [
        '067BDEE076A28059199C73FC31C713EF1123A41E03035CF62C9C0305DCAECBCC',
        '2AF190CCEFFA7054626C4B290DDA7C9CD44B864DACA8C4E4AC1B9B3735BE1D73',
        '8070672BAB1E51530AFAAD24AA6AC0E8C68B064D270845BC32130968B5E10673',
        '0864F3DBB50704E9AB5E950527D892A2A3D709928997715E9529270373DCEB48',
        '07F59241E0B7FED070B74481B016D8A70511B6172892DA1E180472EDAF739742',
        '07F59241E0B7FED070B74481B016D8A70511B6172892DA1E180472EDAF739742',
        '07F59241E0B7FED070B74481B016D8A70511B6172892DA1E180472EDAF739742',
        '0864F3DBB50704E9AB5E950527D892A2A3D709928997715E9529270373DCEB48',
      ],
    },
    'black-knight-elemental-magic': {
      frames: [[0x1E, 6], [0x1F, 8], [0x20, 50], [0x21, 6], [0x23, 8],
        [0x25, 4], [0x27, 4], [0x28, 4], [0x2A, 4], [0x29, 4],
        [0x2B, 8], [0x2C, 8], [0x2D, 8], [0x2E, 6], [0x2F, 6]],
      layers: 44, sources: 27, weaponSources: 8, weaponChildren: 13,
      childCounts: [1, 3, 13],
      canvas: { originX: -46, originY: -55, endX: 42, endY: 6, width: 88, height: 61 },
      pose: [690, 766, 24, 76],
      hashes: [
        '9A6188802E80F3BEDA4D5560430F77C3E619B44FCC1402308C7E88D7D045DA4A',
        '0058E370EB9D0ABADD5860C7872E23259B5C15BF8AB9F4E302E69AA8CE650C22',
        '39F549E4F5E4975CF6F344596A643822ADC54E1B6A04EFF7590306A629F62BE2',
        '91F0D6F9DF1D7E56E9E317F56D969451ECAEEE38E8E62AB41E969475A555AC31',
        '17EF484CB5263D2159ABBC1E45EA74F59E14A34FC1F1DA4970AF42CBC2CDDDCC',
        '799348302B468C67D18026F19F4913B7FE3D1323A2D3A07A70DD5CC78ADCBC09',
        '7F9139CA9E6B88DD924CCAD640FE227B030F3A1160947C90BE1156103A3B8553',
        '5FCB7AF25B987586C3E657B5A0B52572066DE10A4A83DF81FEF83CD8027241FA',
        '91EABDEF200107B48E4D7E507BB705E03193D1E00CE0610367CC2EEC7775CD1D',
        '77999669839CAF859893CB3CD9F70152DD3ACFDB9375C28FB944AAE8738A2727',
        'C22FD399AAF1389BC6E4E26F348B775BE21365437270E8B6240553C584B902F6',
        '9943BCF4DA98EAB1F796C7F9ABD56780B9F97CE0CF07576F834B959A15985067',
        '81700E6914274D5844BC54A68470A55FECBBD655EA732630701A83BAAC0B6EC1',
        'CA7AF17C3390137396BD540683591847F581FBBA22D95832DF0DBF30F93A47F4',
        'E9D4E3C643E480EB0A76C5FF526F2C0A47941D66DB1F0D46579595E3737FC940',
      ],
    },
    'wizard-elemental-magic': {
      frames: [[0x12, 8], [0x13, 16], [0x13, 8], [0x14, 8], [0x15, 8],
        [0x14, 6], [0x13, 6], [0x14, 4], [0x15, 4], [0x14, 2],
        [0x13, 2], [0x14, 2], [0x15, 12], [0x16, 4], [0x17, 32], [0x18, 8]],
      layers: 80, sources: 28, weaponSources: 4, weaponChildren: 12,
      childCounts: [5, 12],
      canvas: { originX: -29, originY: -53, endX: 27, endY: 10, width: 56, height: 63 },
      pose: [565, 618, 18, 53],
      hashes: [
        '4871F20CAC83FD6E7368D50C98E3EE7381AE3122D8FBAAE10FA3F8C4427411B6',
        'D7749A52E0BD6C0275D3A2BE1F0FE831A2B848C65B2F7627949B45D7F6D4C4D4',
        'D7749A52E0BD6C0275D3A2BE1F0FE831A2B848C65B2F7627949B45D7F6D4C4D4',
        '31F26A5492966101E6CB186A531A19ADD4D8E125EC78AE3C63D59DA8B4108BF6',
        'CD4029D471FA050EAB626EE7D4186F09FD49B5A15B6C7CDED30736C9391148CF',
        '31F26A5492966101E6CB186A531A19ADD4D8E125EC78AE3C63D59DA8B4108BF6',
        'D7749A52E0BD6C0275D3A2BE1F0FE831A2B848C65B2F7627949B45D7F6D4C4D4',
        '31F26A5492966101E6CB186A531A19ADD4D8E125EC78AE3C63D59DA8B4108BF6',
        'CD4029D471FA050EAB626EE7D4186F09FD49B5A15B6C7CDED30736C9391148CF',
        '31F26A5492966101E6CB186A531A19ADD4D8E125EC78AE3C63D59DA8B4108BF6',
        'D7749A52E0BD6C0275D3A2BE1F0FE831A2B848C65B2F7627949B45D7F6D4C4D4',
        '31F26A5492966101E6CB186A531A19ADD4D8E125EC78AE3C63D59DA8B4108BF6',
        'CD4029D471FA050EAB626EE7D4186F09FD49B5A15B6C7CDED30736C9391148CF',
        '7F46AF676273D1066A8A5A0CB6C75BC1EF1B58D2203B60574E18970C81271FC3',
        '2B2061734333D841E6469134CB3EF9719A588C32F6441482C96E662FBC843EA0',
        'FED435A788F7BF5A935B713570AEC41EEE4A2B3840FB898DAC5116359F3063B7',
      ],
    },
    'siren-elemental-magic': {
      frames: [[0x29, 6], [0x2A, 8], [0x2B, 12], [0x2C, 10], [0x2D, 6],
        [0x2E, 6], [0x2A, 4], [0x2B, 4], [0x2C, 2], [0x2D, 2],
        [0x2E, 2], [0x2F, 4], [0x30, 4], [0x31, 4], [0x30, 4],
        [0x31, 6], [0x30, 6], [0x31, 8], [0x30, 8], [0x31, 8],
        [0x32, 8], [0x33, 8], [0x34, 8]],
      layers: 56, sources: 18, weaponSources: 5, weaponChildren: 11,
      childCounts: [5, 11],
      canvas: { originX: -28, originY: -47, endX: 31, endY: 7, width: 59, height: 54 },
      pose: [726, 816, 29, 90],
      hashes: [
        'E03B116A37F6B5D3D812FCB2BE7ED60499D9DF4CEA5F74CDB4BE038568D35B45',
        'B2EBF7E5C5CC12916D69C2E348FA81ACE20C194C912D9442E4278DA1006E97F6',
        'C4CE2031F5284BAFFC7E33600FB923CE70754164BE02E2EE86A4E8FE2AE238F4',
        'F5899DEABD6EA36CB88352C7A09AC4E48C1C031B1E6B4F87A6B43A0C49620988',
        'B3EB795A4DC4C223336E16817926911F92A276F3DCB79AF928EE725560996303',
        'CD3F6F47B740C18A870C1CFA3EA7F719971193B3680B12F55B57277C13809F32',
        'B2EBF7E5C5CC12916D69C2E348FA81ACE20C194C912D9442E4278DA1006E97F6',
        'C4CE2031F5284BAFFC7E33600FB923CE70754164BE02E2EE86A4E8FE2AE238F4',
        'F5899DEABD6EA36CB88352C7A09AC4E48C1C031B1E6B4F87A6B43A0C49620988',
        'B3EB795A4DC4C223336E16817926911F92A276F3DCB79AF928EE725560996303',
        'CD3F6F47B740C18A870C1CFA3EA7F719971193B3680B12F55B57277C13809F32',
        '85E926D684B1CFACB71E939742EA087864D3A500DA8AE12EC343346E9BB17DF1',
        '8C4B4C17B38DD705385E1A41AB9F7B6EBBE99FA92D9BF13C924C8DED469FFD99',
        '44104EC1BF3CF9700F86A05933E8DF61C00A4F542959523BF6C13961EA32BB2C',
        '8C4B4C17B38DD705385E1A41AB9F7B6EBBE99FA92D9BF13C924C8DED469FFD99',
        '44104EC1BF3CF9700F86A05933E8DF61C00A4F542959523BF6C13961EA32BB2C',
        '8C4B4C17B38DD705385E1A41AB9F7B6EBBE99FA92D9BF13C924C8DED469FFD99',
        '44104EC1BF3CF9700F86A05933E8DF61C00A4F542959523BF6C13961EA32BB2C',
        '8C4B4C17B38DD705385E1A41AB9F7B6EBBE99FA92D9BF13C924C8DED469FFD99',
        '44104EC1BF3CF9700F86A05933E8DF61C00A4F542959523BF6C13961EA32BB2C',
        '8DF7520D31157CC0BA74D0871EDA05F3DBDDE1B59946FC306A6FC51D7C3731F7',
        '63C2AE0F274160C776FFCD53BB139A007B73786CE5FFFA73498E10FBC88E09F8',
        'CCC25E5590E1FC594A8037E1DE94C26DACE62DA079F49EE116921CC18C411656',
      ],
    },
  };
  assert.deepStrictEqual(rom.art.animations.specs.map(row => row.key),
    Object.keys(corpusFixtures));
  assert.deepStrictEqual(rom.art.animations.specs.map(row =>
    row.spec.retailMappedWeaponOrdinals), [
    Array.from({ length: 17 }, (_, ordinal) => ordinal),
    [],
    Array.from({ length: 12 }, (_, ordinal) => ordinal),
    Array.from({ length: 12 }, (_, ordinal) => ordinal),
    Array.from({ length: 17 }, (_, ordinal) => ordinal),
    [],
    Array.from({ length: 12 }, (_, ordinal) => ordinal),
    Array.from({ length: 12 }, (_, ordinal) => ordinal),
    Array.from({ length: 12 }, (_, ordinal) => ordinal),
    [0, 1, 3, 5, 6, 8, 9, 10, 11],
    [0, 1, 3, 5, 6, 8, 9, 10],
  ]);
  Object.entries(corpusFixtures).forEach(([key, fixture]) => {
    const parsed = rom.art.animations.byKey[key];
    const sources = Object.values(parsed.artByKey);
    assert.deepStrictEqual(parsed.frames.map(frame => [frame.token, frame.ticks]),
      fixture.frames, key + ' frame sequence');
    assert.strictEqual(parsed.frames.reduce((sum, frame) => sum + frame.layers.length, 0),
      fixture.layers, key + ' layer count');
    assert.strictEqual(sources.length, fixture.sources, key + ' source count');
    assert.strictEqual(sources.filter(row => row.weaponSelectable).length,
      fixture.weaponSources, key + ' weapon-source count');
    assert.strictEqual(parsed.spec.weaponChildCount, fixture.weaponChildren,
      key + ' equipment-child count');
    assert.deepStrictEqual(Array.from(new Set(sources.map(row => row.sprite.childCount)))
      .sort((left, right) => left - right), fixture.childCounts, key + ' child-count domain');
    assert.deepStrictEqual(parsed.canvas, fixture.canvas, key + ' canvas');
    assert.deepStrictEqual([parsed.poseProgram.start, parsed.poseProgram.end,
      parsed.poseProgram.recordCount, parsed.poseProgram.program.length], fixture.pose,
    key + ' exact pose-program consumption');
    assert.deepStrictEqual(parsed.frames.map(frame => crypto.createHash('sha256')
      .update(OB64.animationUI.framePixels(parsed, frame, rom.art.animations, null))
      .digest('hex').toUpperCase()), fixture.hashes, key + ' frame composites');
  });
  assert.strictEqual(Object.keys(rom.art.animations.artByKey).length, 157,
    'shared descriptor members must exist once in the global edit model');
  for (const [normalKey, relatedKey] of [
    ['fighter-slash', 'fighter-slash-blocked'],
    ['soldier-thrust', 'soldier-thrust-blocked'],
    ['berserker-strike', 'berserker-strike-blocked'],
    ['black-knight-cleave', 'black-knight-cleave-blocked'],
  ]) {
    const normal = rom.art.animations.byKey[normalKey];
    const related = rom.art.animations.byKey[relatedKey];
    Object.keys(related.artByKey).forEach(key => {
      assert.strictEqual(related.artByKey[key], normal.artByKey[key],
        `${relatedKey} must reuse ${normalKey} physical source ${key}`);
    });
  }
  assert.deepStrictEqual(
    Object.keys(rom.art.animations.byKey['black-knight-elemental-magic'].artByKey),
    Object.keys(rom.art.animations.byKey['black-knight-cleave'].artByKey),
    'Black Knight Cleave and Elemental Magic must use the same physical art set');
  const animation = rom.art.animations.byKey['fighter-slash'];
  const blockedFighter = rom.art.animations.byKey['fighter-slash-blocked'];
  const sharedFrameSourceKey = animation.frames[0].layers.map(layer => layer.sourceKey)
    .find(key => blockedFighter.frames[0].layers.some(layer => layer.sourceKey === key));
  assert(sharedFrameSourceKey, 'Fighter normal and blocked sequences need shared frame art');
  const sharedFrameSource = animation.artByKey[sharedFrameSourceKey];
  assert.deepStrictEqual(sharedFrameSource.animationKeys,
    ['fighter-slash', 'fighter-slash-blocked']);
  assert(sharedFrameSource.usageFramesByAnimation['fighter-slash'].length > 0);
  assert(sharedFrameSource.usageFramesByAnimation['fighter-slash-blocked'].length > 0);
  const sharedOriginal = OB64.animationArt.currentEdit(
    rom.art.animations, sharedFrameSourceKey, sharedFrameSource.childOrdinal);
  const sharedIndices = sharedOriginal.indices.slice();
  const sharedIntensity = sharedOriginal.intensity.slice();
  const sharedVisible = sharedIntensity.findIndex(value => value > 0);
  assert(sharedVisible >= 0);
  const normalBefore = OB64.animationUI.framePixels(
    animation, animation.frames[0], rom.art.animations, null);
  const blockedBefore = OB64.animationUI.framePixels(
    blockedFighter, blockedFighter.frames[0], rom.art.animations, null);
  sharedIndices[sharedVisible] = (sharedIndices[sharedVisible] + 1) & 0xFF;
  assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations,
    sharedFrameSourceKey, sharedFrameSource.childOrdinal,
    sharedIndices, sharedIntensity), true);
  assert.notDeepStrictEqual(Array.from(OB64.animationUI.framePixels(
    animation, animation.frames[0], rom.art.animations, null)), Array.from(normalBefore));
  assert.notDeepStrictEqual(Array.from(OB64.animationUI.framePixels(
    blockedFighter, blockedFighter.frames[0], rom.art.animations, null)),
  Array.from(blockedBefore));
  const sharedBuild = OB64.animationArt.buildResources(rom.art.animations);
  assert.deepStrictEqual(sharedBuild.map(row => row.key), [sharedFrameSourceKey],
    'one shared edit must rebuild one physical source');
  assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations,
    sharedFrameSourceKey, sharedFrameSource.childOrdinal,
    sharedOriginal.indices, sharedOriginal.intensity), true);
  assert(animation);
  assert.deepStrictEqual(animation.frames.map(frame => frame.token),
    [0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19]);
  assert.deepStrictEqual(animation.frames.map(frame => frame.ticks),
    [6, 14, 2, 2, 2, 4, 4, 2, 8]);
  assert.strictEqual(animation.frames.reduce((sum, frame) => sum + frame.layers.length, 0), 57);
  assert.strictEqual(Object.keys(animation.artByKey).length, 42);
  assert.strictEqual(Object.values(animation.artByKey)
    .filter(sourceRow => sourceRow.weaponSelectable).length, 7);
  assert(Object.values(animation.artByKey).every(sourceRow =>
    sourceRow.descriptorReferenceCount === 1 && sourceRow.inPlaceEligible));
  assert.deepStrictEqual(animation.canvas, {
    originX: -39, originY: -53, endX: 37, endY: 5, width: 76, height: 58,
  });
  const expectedFrameHashes = corpusFixtures['fighter-slash'].hashes;
  const actualFrameHashes = animation.frames.map(frame => crypto.createHash('sha256')
    .update(OB64.animationUI.framePixels(animation, frame, rom.art.animations, null))
    .digest('hex').toUpperCase());
  assert.deepStrictEqual(actualFrameHashes, expectedFrameHashes);

  const firstLayer = animation.frames[0].layers[0];
  const source = animation.artByKey[firstLayer.sourceKey];
  const firstVisibleIntensity = OB64.animationArt.currentEdit(
    rom.art.animations, firstLayer.sourceKey, 0).intensity.find(value => value > 0);
  const preservedIntensity = firstVisibleIntensity === 15 ? 14 : 15;
  const selectionUi = {
    animationIntensity: preservedIntensity,
    animationWeaponChild: 0,
  };
  OB64.animationUI.selectLayer(rom.art, animation, animation.frames[0],
    firstLayer, selectionUi);
  assert.strictEqual(selectionUi.animationIntensity, preservedIntensity,
    'layer selection must preserve the authoring intensity');
  const sampleUi = { animationPaletteIndex: 0, animationIntensity: 15,
    animationTool: 'eyedropper' };
  OB64.animationUI.samplePixel(sampleUi, {
    indices: new Uint8Array([42]),
    intensity: new Uint8Array([6]),
  }, 0);
  assert.strictEqual(sampleUi.animationPaletteIndex, 42);
  assert.strictEqual(sampleUi.animationIntensity, 6,
    'eyedropper sampling must update the intensity control state');
  assert.strictEqual(sampleUi.animationTool, 'pencil');
  animation.frames.forEach(frame => {
    const nonWeaponLayer = frame.layers.find(layer =>
      !animation.artByKey[layer.sourceKey].weaponSelectable);
    const pickerSource = OB64.animationUI.weaponSourceForFrame(
      animation, frame, nonWeaponLayer);
    assert(pickerSource && pickerSource.weaponSelectable,
      `frame ${frame.sequenceIndex + 1} must retain a visible weapon picker`);
  });
  for (const parsed of rom.art.animations.specs.filter(row =>
    row.spec.weaponChildCount > 0)) {
    parsed.frames.forEach(frame => {
      assert(OB64.animationUI.weaponSourceForFrame(parsed, frame, frame.layers[0]),
        `${parsed.key} frame ${frame.sequenceIndex + 1} must retain a weapon picker`);
    });
  }
  const animationUi = { animationWeaponChild: 16, animationWeaponChildren: {} };
  assert.strictEqual(OB64.animationUI.weaponChildForAnimation(
    animationUi, animation), 16);
  const berserker = rom.art.animations.byKey['berserker-strike'];
  const blackKnight = rom.art.animations.byKey['black-knight-cleave'];
  const wizard = rom.art.animations.byKey['wizard-elemental-magic'];
  const siren = rom.art.animations.byKey['siren-elemental-magic'];
  const soldier = rom.art.animations.byKey['soldier-thrust'];
  const sirenBodySources = Object.values(siren.artByKey)
    .filter(row => !row.weaponSelectable);
  assert.strictEqual(sirenBodySources.length, 13);
  sirenBodySources.forEach(sourceRow => {
    assert.strictEqual(sourceRow.childOrdinal, 2,
      'Siren body sources must use the verified Siren child');
    assert.deepStrictEqual(sourceRow.editableChildOrdinals, [2]);
    assert.strictEqual(sourceRow.sprite.children[2].discriminator, 2,
      'Siren body child must be a direct discriminator match');
  });
  const sirenBody55 = siren.artById[55];
  assert(sirenBody55 && !sirenBody55.weaponSelectable);
  const sirenBody55Child0 = OB64.animationArt.decodeChild(sirenBody55.sprite, 0);
  const sirenBody55Child2 = OB64.animationArt.decodeChild(sirenBody55.sprite, 2);
  assert.strictEqual(Array.from(sirenBody55Child0.intensity)
    .filter(value => value > 0).length, 0,
  'shared child 0 must remain transparent for Siren body art 55');
  assert.strictEqual(Array.from(sirenBody55Child2.intensity)
    .filter(value => value > 0).length, 687,
  'Siren child 2 must preserve the complete body art 55');
  assert.strictEqual(OB64.animationUI.childOrdinalForSource(sirenBody55), 2);
  assert.strictEqual(OB64.animationUI.weaponChildForAnimation(
    animationUi, berserker), 11);
  assert.strictEqual(OB64.animationUI.setWeaponChild(animationUi, blackKnight, 12), 12);
  assert.strictEqual(OB64.animationUI.weaponChildForAnimation(animationUi, soldier), 0);
  assert.strictEqual(OB64.animationUI.weaponChildForAnimation(
    animationUi, animation), 16, 'each animation must preserve its own weapon selection');
  assert.strictEqual(OB64.animationUI.weaponChildForAnimation(
    animationUi, blackKnight), 12);
  assert.strictEqual(OB64.animationUI.setWeaponChild(animationUi, wizard, 11), 11);
  assert.strictEqual(OB64.animationUI.setWeaponChild(animationUi, siren, 10), 10);
  const sirenWeaponSource = Object.values(siren.artByKey)
    .find(row => row.weaponSelectable);
  assert.strictEqual(OB64.animationUI.childOrdinalForSource(sirenWeaponSource), 0,
    'Siren staff selection must remain independent from its body child');
  function visiblePixels(sourceRow, childOrdinal) {
    return Array.from(OB64.animationArt.decodeChild(
      sourceRow.sprite, childOrdinal).intensity)
      .filter(value => value > 0).length;
  }
  function weaponSources(parsed) {
    return Object.values(parsed.artByKey).filter(row => row.weaponSelectable);
  }
  for (const parsed of [animation, berserker]) {
    weaponSources(parsed).forEach(sourceRow => {
      sourceRow.editableChildOrdinals.forEach(childOrdinal => {
        assert(visiblePixels(sourceRow, childOrdinal) > 0,
          parsed.key + ' mapped weapon child must contain pixels');
      });
    });
  }
  weaponSources(blackKnight).forEach(sourceRow => {
    assert.strictEqual(visiblePixels(sourceRow, 12), 0,
      'Black Knight unused child 12 must remain an empty retail record');
  });
  weaponSources(wizard).forEach(sourceRow => {
    [2, 4, 7].forEach(childOrdinal => {
      assert(visiblePixels(sourceRow, childOrdinal) > 0,
        'Wizard unmapped child must retain its physical artwork');
    });
  });
  weaponSources(siren).forEach(sourceRow => {
    [2, 4, 7].forEach(childOrdinal => {
      assert.strictEqual(visiblePixels(sourceRow, childOrdinal), 0,
        'Siren unused staff child must remain an empty retail record');
    });
  });

  for (const parsed of [berserker, blackKnight, wizard, siren]) {
    const lastChild = parsed.spec.weaponChildCount - 1;
    const weaponSource = Object.values(parsed.artByKey)
      .find(row => row.weaponSelectable);
    assert(weaponSource, parsed.key + ' needs a verified weapon source');
    assert.deepStrictEqual(weaponSource.editableChildOrdinals,
      Array.from({ length: parsed.spec.weaponChildCount }, (_, ordinal) => ordinal));
    const originalObject = weaponSource.sprite.decoded.slice();
    const originalChild = OB64.animationArt.currentEdit(
      rom.art.animations, weaponSource.key, lastChild);
    const changedIndices = originalChild.indices.slice();
    const changedIntensity = originalChild.intensity.slice();
    const firstVisible = changedIntensity.findIndex(value => value > 0);
    const changedPixel = firstVisible >= 0 ? firstVisible : 0;
    changedIndices[changedPixel] = (changedIndices[changedPixel] + 1) & 0xFF;
    changedIntensity[changedPixel] = changedIntensity[changedPixel] === 15
      ? 14 : changedIntensity[changedPixel] + 1;
    assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations,
      weaponSource.key, lastChild, changedIndices, changedIntensity), true);
    const rebuiltObject = OB64.animationArt.buildDecoded(
      weaponSource, rom.art.animations.edits[weaponSource.key].children);
    for (let child = 0; child < weaponSource.sprite.childCount; child++) {
      if (child === lastChild) continue;
      const span = weaponSource.sprite.children[child];
      assert.deepStrictEqual(Array.from(rebuiltObject.subarray(span.start, span.end)),
        Array.from(originalObject.subarray(span.start, span.end)),
        `${parsed.key} sibling child ${child} changed`);
    }
    assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations,
      weaponSource.key, lastChild, originalChild.indices, originalChild.intensity), true);
    assert.strictEqual(OB64.animationArt.hasEdit(
      rom.art.animations, weaponSource.key, lastChild), false);
  }

  const expandedRom = { z64: z64.slice(), layout: { id: 'us-rev0' } };
  await OB64.art.initialize(expandedRom);
  const expandedSelections = [
    {
      animation: expandedRom.art.animations.byKey['soldier-thrust'],
      source: Object.values(expandedRom.art.animations.byKey['soldier-thrust'].artByKey)
        .find(row => row.descriptorReferenceCount > 1),
      child: 0,
    },
    {
      animation: expandedRom.art.animations.byKey['berserker-strike'],
      source: Object.values(expandedRom.art.animations.byKey['berserker-strike'].artByKey)
        .find(row => row.weaponSelectable),
      child: 11,
    },
    {
      animation: expandedRom.art.animations.byKey['black-knight-cleave'],
      source: Object.values(expandedRom.art.animations.byKey['black-knight-cleave'].artByKey)
        .find(row => row.weaponSelectable),
      child: 12,
    },
    {
      animation: expandedRom.art.animations.byKey['wizard-elemental-magic'],
      source: Object.values(expandedRom.art.animations.byKey['wizard-elemental-magic'].artByKey)
        .find(row => row.weaponSelectable),
      child: 11,
    },
    {
      animation: expandedRom.art.animations.byKey['siren-elemental-magic'],
      source: Object.values(expandedRom.art.animations.byKey['siren-elemental-magic'].artByKey)
        .find(row => row.weaponSelectable),
      child: 10,
    },
  ];
  expandedSelections.forEach((selection, ordinal) => {
    assert(selection.source, selection.animation.key + ' export source');
    const pixels = OB64.animationArt.currentEdit(expandedRom.art.animations,
      selection.source.key, selection.child);
    const changedIndices = pixels.indices.slice();
    const changedIntensity = pixels.intensity.slice();
    const firstVisible = changedIntensity.findIndex(value => value > 0);
    const changedPixel = firstVisible >= 0 ? firstVisible : 0;
    changedIndices[changedPixel] = (changedIndices[changedPixel] + ordinal + 1) & 0xFF;
    changedIntensity[changedPixel] = changedIntensity[changedPixel] === 15
      ? 14 : changedIntensity[changedPixel] + 1;
    assert.strictEqual(OB64.animationArt.setEdit(expandedRom.art.animations,
      selection.source.key, selection.child, changedIndices, changedIntensity), true);
  });
  const expandedPayload = OB64.art.collectProjectPayload(expandedRom);
  assert.strictEqual(OB64.art.prepareProjectPayload(
    expandedRom, expandedPayload).animations.count, 5);
  const expandedCandidate = { z64: z64.slice() };
  const expandedPlan = OB64.art.prepareExport(expandedRom, expandedCandidate);
  assert.strictEqual(expandedPlan.animations.length, 5);
  assert.strictEqual(expandedPlan.animations.find(row =>
    row.key === expandedSelections[0].source.key).placement, 'relocated',
  'shared Soldier source must detach copy-on-write');
  const expandedResult = OB64.art.applyExport(
    expandedRom, expandedCandidate, expandedPlan);
  assert.strictEqual(expandedResult.editedCombatSpriteCount, 5);
  assert(expandedResult.log.some(line => line.includes('Soldier Thrust art')));
  assert(expandedResult.log.some(line => line.includes('Berserker Strike art')));
  assert(expandedResult.log.some(line => line.includes('Black Knight Cleave art')));
  assert(expandedResult.log.some(line => line.includes('Wizard Elemental Magic art')));
  assert(expandedResult.log.some(line => line.includes('Siren Elemental Magic art')));
  const paletteOrder = OB64.animationUI.paletteHueOrder(source.palette);
  assert.strictEqual(paletteOrder.length, 256);
  assert.deepStrictEqual(paletteOrder.slice().sort((left, right) => left - right),
    Array.from({ length: 256 }, (_, index) => index));
  assert.strictEqual(source.resourceKey, 0x003CD046);
  assert.strictEqual(source.sprite.childCount, 17);
  assert.strictEqual(source.sprite.firstStride, 16);
  assert.strictEqual(source.sprite.secondStride, 8);
  assert.strictEqual(source.originalIndices.length, 16 * 30);
  assert.deepStrictEqual(source.editableChildOrdinals,
    Array.from({ length: 17 }, (_, ordinal) => ordinal));
  assert.strictEqual(Object.keys(source.originalChildren).length, 17);
  assert.strictEqual(OB64.animationUI.childOrdinalForSource(source, 5), 5);
  const weaponFiveFrameHash = crypto.createHash('sha256')
    .update(OB64.animationUI.framePixels(animation, animation.frames[0],
      rom.art.animations, null, null, 5))
    .digest('hex').toUpperCase();
  assert.notStrictEqual(weaponFiveFrameHash, expectedFrameHashes[0]);
  assert.strictEqual(OB64.animationUI.childPixels(
    source, rom.art.animations, 5).length, 16 * 30 * 4);

  const originalDecoded = source.sprite.decoded.slice();
  const weaponChild = 5;
  const original = OB64.animationArt.currentEdit(
    rom.art.animations, source.key, weaponChild);
  const indices = original.indices.slice();
  const intensity = original.intensity.slice();
  const visiblePixel = intensity.findIndex(value => value > 0);
  assert(visiblePixel >= 0);
  indices[visiblePixel] = (indices[visiblePixel] + 1) & 0xFF;
  intensity[visiblePixel] = Math.max(1, intensity[visiblePixel] - 1);
  assert.strictEqual(OB64.animationArt.setEdit(
    rom.art.animations, source.key, weaponChild, indices, intensity), true);
  assert.strictEqual(OB64.animationArt.hasEdit(
    rom.art.animations, source.key, weaponChild), true);
  assert.strictEqual(OB64.animationArt.hasEdit(
    rom.art.animations, source.key, 0), false);

  const rebuilt = OB64.animationArt.buildDecoded(
    source, rom.art.animations.edits[source.key].children);
  const reparsed = OB64.animationArt.parseSpriteObject(rebuilt, source.resourceKey);
  const rebuiltChild = OB64.animationArt.decodeChild(reparsed, weaponChild);
  assert.deepStrictEqual(Array.from(rebuiltChild.indices), Array.from(indices));
  assert.deepStrictEqual(Array.from(rebuiltChild.intensity), Array.from(intensity));
  for (let child = 0; child < source.sprite.children.length; child++) {
    if (child === weaponChild) continue;
    const span = source.sprite.children[child];
    assert.deepStrictEqual(
      Array.from(rebuilt.subarray(span.start, span.end)),
      Array.from(originalDecoded.subarray(span.start, span.end)),
      `sibling child ${child} changed`
    );
  }

  const payload = OB64.art.collectProjectPayload(rom);
  assert.strictEqual(payload.schemaVersion, 2);
  assert.deepStrictEqual(Object.keys(payload.animations), [source.key]);
  assert.deepStrictEqual(Object.keys(payload.animations[source.key].children),
    [String(weaponChild)]);
  const prepared = OB64.art.prepareProjectPayload(rom, payload);
  assert.strictEqual(prepared.animations.count, 1);
  const nestedEntry = payload.animations[source.key];
  const legacyFlatEntry = Object.assign({}, nestedEntry, {
    childOrdinal: weaponChild,
    ci8IndicesBase64: nestedEntry.children[weaponChild].ci8IndicesBase64,
    i4IntensityBase64: nestedEntry.children[weaponChild].i4IntensityBase64,
  });
  delete legacyFlatEntry.children;
  const legacyPrepared = OB64.art.prepareProjectPayload(rom, {
    schemaVersion: 2, avatars: {}, icons: {},
    animations: { [source.key]: legacyFlatEntry },
  });
  assert.strictEqual(legacyPrepared.animations.count, 1);

  const secondWeaponChild = 6;
  const secondOriginal = OB64.animationArt.currentEdit(
    rom.art.animations, source.key, secondWeaponChild);
  const secondIndices = secondOriginal.indices.slice();
  const secondIntensity = secondOriginal.intensity.slice();
  const secondVisible = secondIntensity.findIndex(value => value > 0);
  assert(secondVisible >= 0);
  secondIndices[secondVisible] = (secondIndices[secondVisible] + 2) & 0xFF;
  assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations, source.key,
    secondWeaponChild, secondIndices, secondIntensity), true);
  assert.strictEqual(OB64.animationArt.editCount(rom.art.animations), 2);
  const multiChildResources = OB64.animationArt.buildResources(rom.art.animations);
  assert.strictEqual(multiChildResources.length, 1,
    'two weapon-child edits must rebuild one shared source object');
  assert.strictEqual(multiChildResources[0].placement, 'in-place');
  const multiChildPayload = OB64.art.collectProjectPayload(rom);
  assert.deepStrictEqual(Object.keys(
    multiChildPayload.animations[source.key].children), ['5', '6']);
  assert.strictEqual(OB64.art.prepareProjectPayload(
    rom, multiChildPayload).animations.count, 2);

  const candidate = { z64: z64.slice() };
  const plan = OB64.art.prepareExport(rom, candidate);
  assert(plan);
  assert.strictEqual(plan.animations.length, 1);
  assert.strictEqual(plan.animations[0].placement, 'in-place');
  assert.strictEqual(plan.animations[0].allocation, undefined);
  const result = OB64.art.applyExport(rom, candidate, plan);
  assert.strictEqual(result.editedCombatSpriteCount, 1);
  assert.strictEqual(result.inPlaceCombatSpriteCount, 1);
  assert.strictEqual(result.relocatedCombatSpriteCount, 0);
  assert.strictEqual(result.relocatedResourceCount, 0);
  assert.strictEqual(result.currentCrcAffected, false);

  const descriptorPointer = u32(candidate.z64, source.descriptorEntryOffset);
  assert.strictEqual(descriptorPointer, source.resourceKey);
  assert.notDeepStrictEqual(
    Array.from(candidate.z64.subarray(source.resource.entry,
      source.resource.entry + source.resource.envelopeLength)),
    Array.from(z64.subarray(source.resource.entry,
      source.resource.entry + source.resource.envelopeLength)),
    'in-place export did not change the source resource'
  );
  const inPlace = OB64.art.readCompressedResource(candidate.z64, descriptorPointer);
  assert.deepStrictEqual(Array.from(inPlace.decoded),
    Array.from(plan.animations[0].built.decoded));

  const relocationSource = Object.values(animation.artByKey)
    .filter(row => row.key !== source.key && row.sprite.childCount === 1)
    .sort((left, right) => left.resource.storedLength - right.resource.storedLength)[0];
  assert(relocationSource, 'expected a one-child source for the relocation test');
  const relocationOriginal = OB64.animationArt.currentEdit(
    rom.art.animations, relocationSource.key, 0);
  const relocationIndices = relocationOriginal.indices.slice();
  const relocationIntensity = relocationOriginal.intensity.slice();
  let seed = 0x4F473634;
  for (let pixel = 0; pixel < relocationIndices.length; pixel++) {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    relocationIndices[pixel] = seed >>> 24;
    relocationIntensity[pixel] = (seed >>> 20) & 0x0F;
  }
  assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations,
    relocationSource.key, 0, relocationIndices, relocationIntensity), true);

  const hybridCandidate = { z64: z64.slice() };
  const hybridPlan = OB64.art.prepareExport(rom, hybridCandidate);
  const hybridInPlace = hybridPlan.animations.find(row => row.key === source.key);
  const hybridRelocated = hybridPlan.animations.find(row =>
    row.key === relocationSource.key);
  assert.strictEqual(hybridInPlace.placement, 'in-place');
  assert.strictEqual(hybridRelocated.placement, 'relocated');
  assert.strictEqual(hybridRelocated.allocation.entry, 0x0275E000);
  const hybridResult = OB64.art.applyExport(rom, hybridCandidate, hybridPlan);
  assert.strictEqual(hybridResult.editedCombatSpriteCount, 2);
  assert.strictEqual(hybridResult.inPlaceCombatSpriteCount, 1);
  assert.strictEqual(hybridResult.relocatedCombatSpriteCount, 1);
  assert.strictEqual(hybridResult.relocatedResourceCount, 1);
  assert.strictEqual(hybridResult.currentCrcAffected, false);

  const relocatedPointer = u32(hybridCandidate.z64,
    relocationSource.descriptorEntryOffset);
  assert.strictEqual(relocatedPointer, hybridRelocated.allocation.key);
  assert.notStrictEqual(relocatedPointer, relocationSource.resourceKey);
  assert.deepStrictEqual(
    Array.from(hybridCandidate.z64.subarray(relocationSource.resource.entry,
      relocationSource.resource.entry + relocationSource.resource.envelopeLength)),
    Array.from(z64.subarray(relocationSource.resource.entry,
      relocationSource.resource.entry + relocationSource.resource.envelopeLength)),
    'copy-on-write relocation changed the retail source resource'
  );
  const relocated = OB64.art.readCompressedResource(hybridCandidate.z64,
    relocatedPointer);
  assert.deepStrictEqual(Array.from(relocated.decoded),
    Array.from(hybridRelocated.built.decoded));

  const appearance = rom.art.avatar.appearances[0];
  const avatarWords = OB64.art.currentWords(
    rom.art, 'avatar', appearance.key).slice();
  const replacementWord = avatarWords.find(word => word !== avatarWords[0]);
  assert.notStrictEqual(replacementWord, undefined);
  avatarWords[0] = replacementWord;
  assert.strictEqual(OB64.art.setEditWords(
    rom.art, 'avatar', appearance.key, avatarWords), true);
  const combinedCandidate = { z64: z64.slice() };
  const combinedPlan = OB64.art.prepareExport(rom, combinedCandidate);
  assert.strictEqual(combinedPlan.avatars.length, 1);
  assert.deepStrictEqual(combinedPlan.allocations.map(row => row.name),
    ['avatar-0', hybridRelocated.name, 'avatar-descriptor']);
  for (let allocation = 1; allocation < combinedPlan.allocations.length; allocation++) {
    assert(combinedPlan.allocations[allocation - 1].end <=
      combinedPlan.allocations[allocation].entry);
  }
  const combinedResult = OB64.art.applyExport(rom, combinedCandidate, combinedPlan);
  assert.strictEqual(combinedResult.detachedAvatarCount, 1);
  assert.strictEqual(combinedResult.editedCombatSpriteCount, 2);
  assert.strictEqual(combinedResult.relocatedResourceCount, 3);
  assert.strictEqual(combinedResult.currentCrcAffected, true);

  rom.z64 = combinedCandidate.z64.slice();
  OB64.art.adoptExport(rom, combinedResult);
  const weaponOriginal = OB64.animationArt.originalChild(source, weaponChild);
  assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations, source.key,
    weaponChild, weaponOriginal.indices, weaponOriginal.intensity), true);
  assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations, source.key,
    secondWeaponChild, secondOriginal.indices, secondOriginal.intensity), true);
  assert.strictEqual(OB64.animationArt.setEdit(rom.art.animations,
    relocationSource.key, 0, relocationSource.originalIndices,
    relocationSource.originalIntensity), true);
  const revertedCandidate = { z64: rom.z64.slice() };
  const revertedPlan = OB64.art.prepareExport(rom, revertedCandidate);
  assert.strictEqual(revertedPlan.animations.length, 0);
  assert.strictEqual(revertedPlan.avatars.length, 1);
  const revertedResult = OB64.art.applyExport(rom, revertedCandidate, revertedPlan);
  assert.strictEqual(revertedResult.editedCombatSpriteCount, 0);
  assert.deepStrictEqual(
    Array.from(revertedCandidate.z64.subarray(source.resource.entry,
      source.resource.entry + source.resource.envelopeLength)),
    Array.from(z64.subarray(source.resource.entry,
      source.resource.entry + source.resource.envelopeLength))
  );
  assert.strictEqual(u32(revertedCandidate.z64,
    relocationSource.descriptorEntryOffset), relocationSource.resourceKey);

  console.log('PASS 11 verified class/action sequences consume their exact pose programs');
  console.log('PASS all 139 editor composites match their frozen reference pixels');
  console.log('PASS combat-sprite edit preserves padding and every untouched child');
  console.log('PASS shared pose variants edit and rebuild each physical source once');
  console.log('PASS 11-, 12-, 13-, and 17-child weapon groups preserve sibling children');
  console.log('PASS physical and spellcaster edits export together with exact readback');
  console.log('PASS Project v19 round-trip and hybrid in-place/relocated exports read back exactly');
  console.log('PASS detached avatars and relocated combat sprites share the art arena without overlap');
  console.log('PASS a later same-session export restores removed combat-sprite edits to retail bytes');
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
