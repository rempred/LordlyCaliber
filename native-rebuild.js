// Lordly Caliber Editor — authenticated Phase 1 through Phase 5A ROM rebuild.
window.OB64 = window.OB64 || {};

(function(OB64, root) {
  'use strict';

  var DATA_SCHEMA = 'ob64-native-rebuild-data';
  var DATA_VERSION = 1;
  var EXPECTED_PAYLOAD_SHA256 =
    '4B557C54E26C8CA086048FD08FF9A915D207A07CDC3AC646DC2F81B373E0871C';
  var ALIGNMENT_V10_PAYLOAD_SHA256 = 'C2212EECA37F4E40EA061B4063254A9B5C8EB7F81C917A2E9EF11850C12F9354';
  var EXPECTED_GENERATOR_SOURCE_IDENTITY = 'sha256-canonical-utf8-lf-v1';
  var EXPECTED_GENERATOR_CANONICAL_BYTES = 34899;
  var EXPECTED_GENERATOR_SHA256 =
    'C65F81C124AFABA7A6C444E7A7A76578C1EF6A50147596E7A24FE26EAD69D69D';
  var INPUT_CONTRACT_SCHEMA = 'ob64-native-rebuild-editor-input-contract';
  var INPUT_CONTRACT_VERSION = 1;
  var INPUT_CONTRACT_ID = 'lordly-native-rebuild.phase5a.editor-inputs.v1';
  var EXPECTED_INPUT_CONTRACT_SHA256 =
    'A7BE40F1F386239BCA42AAACBF60CCAF0B673051ACD74DFE90D50BA53C11223E';
  var ACCEPTED_COMMIT = 'e50fe500839288c94c538969acc7a5b966075626';
  var ACCEPTED_TREE = 'ef58ea271b2298be71b09be9318a359805c266c0';
  var RETAIL_SHA256 = '571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A';
  var PATCH_MODE = 'patch';
  var REBUILD_MODE = 'rebuild';
  var MODES = Object.freeze({
    patch: Object.freeze({
      id: PATCH_MODE,
      label: 'Patch ROM',
      help: 'Uses the legacy fixed-offset export path and supports US Rev 0 and US Rev 1.',
      shopEquipmentCapacity: 50,
      shopExpendableCapacity: 15,
    }),
    rebuild: Object.freeze({
      id: REBUILD_MODE,
      label: 'Rebuild ROM',
      help: 'Uses authenticated native source owners through Phase 5A. US Rev 0 only; requires 8 MiB RDRAM.',
      shopEquipmentCapacity: 50,
      shopExpendableCapacity: 30,
    }),
  });
  var MIGRATED_TOOL_IDS = Object.freeze([
    'experience-size-scale',
    'cf-army-counter',
    'character-card-luck',
    'squad-menu-alignment',
  ]);
  var PHASE3_TOOL_IDS = Object.freeze([
    'cf-army-counter',
    'character-card-luck',
    'squad-menu-alignment',
  ]);
  var verifiedData = null;
  var byteCache = Object.create(null);
  var authorizedCompositions = new WeakSet();
  var issuedPlanSignatures = new WeakMap();
  var issuedConsumableProvenance = new WeakMap();
  var acceptedConsumableProductsByOwnerImage = Object.create(null);

  function fail(message) {
    throw new Error('Rebuild ROM rejected the export: ' + message);
  }

  function dataBundle() {
    return root.OB64_NATIVE_REBUILD_DATA;
  }

  function deepFreeze(value) {
    if (!value || typeof value !== 'object' || Object.isFrozen(value)) return value;
    Object.keys(value).forEach(function(key) { deepFreeze(value[key]); });
    return Object.freeze(value);
  }

  function utf8Bytes(text) {
    if (typeof TextEncoder !== 'undefined') return new TextEncoder().encode(text);
    var encoded = unescape(encodeURIComponent(text));
    var out = new Uint8Array(encoded.length);
    for (var index = 0; index < encoded.length; index++) out[index] = encoded.charCodeAt(index);
    return out;
  }

  function sha256(bytes) {
    if (!OB64.consumableEffects || !OB64.consumableEffects.sha256HexSync) {
      fail('the synchronous SHA-256 verifier is unavailable');
    }
    return OB64.consumableEffects.sha256HexSync(bytes).toUpperCase();
  }

  function hexBytes(hex) {
    if (typeof hex !== 'string' || (hex.length & 1)) fail('bundled hexadecimal data is malformed');
    var out = new Uint8Array(hex.length / 2);
    for (var index = 0; index < out.length; index++) {
      var value = parseInt(hex.slice(index * 2, index * 2 + 2), 16);
      if (!Number.isInteger(value)) fail('bundled hexadecimal data is malformed');
      out[index] = value;
    }
    return out;
  }

  function componentBytes(component, kind) {
    var key = component.owner + '|' + component.name + '|' + component.sha256 + '|' + kind;
    if (!byteCache[key]) {
      byteCache[key] = hexBytes(kind === 'retail' ? component.retailHex : component.dataHex);
    }
    return byteCache[key];
  }

  function equalBytes(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var index = 0; index < left.length; index++) {
      if (left[index] !== right[index]) return false;
    }
    return true;
  }

  function equalAt(bytes, offset, expected) {
    if (!(bytes instanceof Uint8Array) || offset < 0 || offset + expected.length > bytes.length) {
      return false;
    }
    for (var index = 0; index < expected.length; index++) {
      if (bytes[offset + index] !== expected[index]) return false;
    }
    return true;
  }

  function verifyComponent(component) {
    var nativeBytes = componentBytes(component, 'native');
    var retailBytes = componentBytes(component, 'retail');
    if (nativeBytes.length !== component.bytes || retailBytes.length !== component.bytes) {
      fail('bundled component length drift: ' + component.owner + ' / ' + component.name);
    }
    if (sha256(nativeBytes) !== component.sha256) {
      fail('bundled component hash drift: ' + component.owner + ' / ' + component.name);
    }
  }

  function verifyData() {
    if (verifiedData) return verifiedData;
    var bundle = dataBundle();
    if (!bundle || bundle.schema !== DATA_SCHEMA || bundle.schemaVersion !== DATA_VERSION ||
        !bundle.payload) {
      fail('the authenticated native-owner data bundle is missing or malformed');
    }
    var payloadHash = sha256(utf8Bytes(JSON.stringify(bundle.payload)));
    if (payloadHash !== bundle.payloadSha256 || payloadHash !== EXPECTED_PAYLOAD_SHA256 && payloadHash !== ALIGNMENT_V10_PAYLOAD_SHA256) {
      fail('the native-owner data bundle failed its SHA-256 identity check');
    }
    var data = bundle.payload;
    if (!data.inputContract || data.inputContract.schema !== INPUT_CONTRACT_SCHEMA ||
        data.inputContract.schemaVersion !== INPUT_CONTRACT_VERSION ||
        data.inputContract.id !== INPUT_CONTRACT_ID ||
        data.inputContract.sha256 !== EXPECTED_INPUT_CONTRACT_SHA256 ||
        !Number.isInteger(data.inputContract.canonicalBytes) ||
        data.inputContract.canonicalBytes <= 0) {
      fail('the versioned native input-contract identity drifted');
    }
    if (data.acceptedSource.commit !== ACCEPTED_COMMIT ||
        data.acceptedSource.tree !== ACCEPTED_TREE ||
        data.acceptedSource.retailSha256 !== RETAIL_SHA256 ||
        data.acceptedSource.runtimeRdramBytes !== 0x800000) {
      fail('accepted source commit, tree, retail product, or 8 MiB RDRAM identity drifted');
    }
    if (data.shops.contractSha256 !==
        'C6D71952A0AE7A813C3F79D8EEAAA4CCAADF53168E2AA627BE577726F64FD200' ||
        data.composition.manifestSha256 !==
        'DBCA8D456D8D786DE9A69130CBABF9CCF6FB27B8E746DEC0E0616511B4D420A3') {
      fail('the Shop contract or ownership manifest identity drifted');
    }
    if (!data.generator || data.generator.path !== 'tools/generate-native-rebuild-data.js' ||
        data.generator.sourceIdentity !== EXPECTED_GENERATOR_SOURCE_IDENTITY ||
        !Number.isInteger(data.generator.canonicalBytes) ||
        data.generator.canonicalBytes !== EXPECTED_GENERATOR_CANONICAL_BYTES ||
        data.generator.sha256 !== EXPECTED_GENERATOR_SHA256) {
      fail('the native-owner generator provenance drifted');
    }
    var approvalHashes = (data.approvals || []).map(function(approval) {
      return approval.phase + ':' + approval.sha256;
    }).sort();
    if (approvalHashes.join('|') !== [
      'Phase 3:B458CD6BFC5964938D61BE855AD9792D59A2509D73CF6FCB57844275A97D2ABA',
      'Phase 4:B1ED23C9248BF7D893CF1EE9DBEF2ED8AE2D266B84EFF4045FE03BD348797DEF',
      'Phase 5A:578C512E89429746888772AB8150BA56FFB888E4883B79344F023B8236031743',
    ].join('|')) {
      fail('the accepted source-approval identities drifted');
    }
    if (!data.acceptedSource.inputs || data.acceptedSource.inputs.length !== 52 ||
        !data.acceptedSource.inputs.some(function(input) {
          return input.path.indexOf('composition-matrix.json') !== -1 &&
            input.sha256 ===
              '86419B194D7984FB0CD939AADC150D231472A280363D7BEE8D63B30617FAF96E';
        }) || !data.acceptedSource.inputs.some(function(input) {
          return input.path.indexOf('objects/manifest.json') !== -1 &&
            input.sha256 ===
              '1ABFBB9BDB47DE2BC6D9C1837277DE020B1D8F626838925F2DE180A2B25C7F7C';
        }) || data.acceptedSource.inputs.filter(function(input) {
          return input.path.indexOf('relocation-ledger.json') !== -1;
        }).length !== 3) {
      fail('the immutable source-input identity ledger is incomplete');
    }
    var expectedMigratedOwners = [
      'tool.experience-size-scale',
      'tool.chaos-frame-counter',
      'tool.character-card-luck',
      'tool.squad-menu-alignment',
      'runtime.shop-overrides',
      'effects.consumable-code',
      'text.consumable-descriptions',
    ];
    if (!data.composition ||
        data.composition.migratedOwners.join('|') !== expectedMigratedOwners.join('|') ||
        data.composition.order.length !== 12 ||
        data.composition.order[0].ordinal !== 1 ||
        data.composition.order[11].ordinal !== 12 ||
        data.composition.order[11].step !== 'native-source-overlay') {
      fail('the accepted composition ownership stack drifted');
    }
    if (!data.composition.phase4Matrix ||
        data.composition.phase4Matrix.status !== 'pass' ||
        data.composition.phase4Matrix.stateCount !== 32 ||
        data.composition.phase4Matrix.states.length !== 32) {
      fail('the accepted Phase 4 composition matrix is incomplete');
    }
    var phase4Transition = data.composition.phase4Extension &&
      data.composition.phase4Extension.ownershipTransition;
    if (!phase4Transition ||
        data.composition.phase4Extension.baseManifestSha256 !==
          data.composition.manifestSha256 ||
        phase4Transition.contributorId !== 'runtime.shop-overrides' ||
        phase4Transition.legacyPhysicalOwner !== 'runtime.shared-overrides' ||
        phase4Transition.nativePhysicalOwner !== 'runtime.shop-overrides' ||
        phase4Transition.mixedState !== 'reject') {
      fail('the accepted Phase 4 composition extension drifted');
    }
    var phase5aExtension = data.composition.phase5aExtension;
    if (!phase5aExtension ||
        phase5aExtension.baseManifestSha256 !== data.composition.manifestSha256 ||
        phase5aExtension.acceptedCommit !== ACCEPTED_COMMIT ||
        phase5aExtension.acceptedTree !== ACCEPTED_TREE ||
        phase5aExtension.effectOwner !== 'effects.consumable-code' ||
        phase5aExtension.descriptionOwner !== 'text.consumable-descriptions' ||
        phase5aExtension.retiredDescriptionOwner !== 'effects.healing-descriptions' ||
        phase5aExtension.mixedState !== 'reject' ||
        !Array.isArray(data.composition.retiredOwners) ||
        data.composition.retiredOwners.join('|') !== 'effects.healing-descriptions') {
      fail('the accepted Phase 5A composition extension drifted');
    }
    var phase3Masks = data.phase3.matrix.map(function(entry) { return entry.mask; });
    if (data.phase3.components.length !== 10 ||
        phase3Masks.join(',') !== '0,1,2,3,4,5,6,7' ||
        data.shops.components.length !== 11 ||
        data.shops.contract.recordCount !== 36 ||
        data.shops.contract.realShopCount !== 35 ||
        data.shops.contract.equipmentCapacity !== 50 ||
        data.shops.contract.expendableCapacity !== 30) {
      fail('the accepted native component or Shop contract geometry drifted');
    }
    data.phase3.components.forEach(verifyComponent);
    data.shops.components.forEach(verifyComponent);
    var experienceRetail = hexBytes(data.experience.retailHex);
    if (experienceRetail.length !== data.experience.bytes ||
        data.experience.generator !==
          'tools/lib/lordly_native_rebuild.js::generateTargetSource' ||
        !data.experience.acceptedSource ||
        data.experience.acceptedSource.path !== 'asm/original/rev0/lib/func_002190AC.s' ||
        data.experience.acceptedSource.sha256 !==
          '2AC89C4FFED7A2D2CDDF72B413B672E0486850D544604724AD1373C1705320A9' ||
        !Array.isArray(data.experience.coverageSha256) ||
        data.experience.coverageSha256.length !== 301 ||
        data.experience.coverageSha256[200] !== data.experience.proof200Sha256) {
      fail('the Experience Scale owner length drifted');
    }
    var consumables = data.consumables;
    if (!consumables || consumables.generator !==
        'tools/lib/lordly_native_phase5a_consumables.js::build' ||
        consumables.effect.owner !== 'effects.consumable-code' ||
        consumables.effect.section !== '.ob64.r0815' ||
        consumables.effect.romStart !== 0x41098 ||
        consumables.effect.ramStart !== 0x8016B198 ||
        consumables.effect.bytes !== 1380 ||
        consumables.effect.modelOrder.length !== 10 ||
        consumables.effect.models.length !== 10 ||
        consumables.effect.sourceWords.length !== 16 ||
        consumables.descriptions.owner !== 'text.consumable-descriptions' ||
        consumables.descriptions.retiredOwner !== 'effects.healing-descriptions' ||
        consumables.descriptions.romStart !== 0x237744A ||
        consumables.descriptions.bytes !== 2064 ||
        consumables.descriptions.streamCapacity !== 2055 ||
        consumables.descriptions.recordCount !== 50 ||
        consumables.metadata.owner !== 'direct.consumable-metadata' ||
        consumables.metadata.romStart !== 0x645D0 ||
        consumables.metadata.bytes !== 764 ||
        consumables.acceptedSource.commit !== ACCEPTED_COMMIT ||
        consumables.acceptedSource.tree !== ACCEPTED_TREE ||
        consumables.acceptedSource.products.exactRetail !== RETAIL_SHA256 ||
        consumables.acceptedSource.products.nativeConsumables !==
          '6F7781ADB562EFD9131C03BC127CC3AF26AD32D575CA6BF5AC2EA60FB6843A27' ||
        consumables.acceptedSource.products.canonicalNativeConsumables !==
          '496EA6070FEB0EC5D689A821957BCD3101D9C03C0037357E72772B906731FB37') {
      fail('the accepted Phase 5A consumable owner contract drifted');
    }
    var effectRetail = hexBytes(consumables.effect.retailHex);
    var descriptionRetail = hexBytes(consumables.descriptions.retailHex);
    if (effectRetail.length !== consumables.effect.bytes ||
        sha256(effectRetail) !== consumables.effect.retailSha256 ||
        descriptionRetail.length !== consumables.descriptions.bytes ||
        sha256(descriptionRetail) !== consumables.descriptions.retailSha256 ||
        consumables.effect.productionWritePolicy !==
          'The assembler emits the complete section. Sixteen fixed-offset writes exist only in the frozen legacy oracle.') {
      fail('the accepted Phase 5A retail owner images drifted');
    }
    var modelOrder = OB64.consumableEffects && OB64.consumableEffects.MODEL_ORDER || [];
    if (modelOrder.join('|') !== consumables.effect.modelOrder.join('|')) {
      fail('the Editor consumable model order differs from the accepted source model');
    }
    consumables.effect.sourceWords.forEach(function(word) {
      var def = OB64.consumableEffects.MODEL_DEFS[word.modelKey];
      var valid = def && word.romStart >= consumables.effect.romStart &&
        word.romStart + 4 <= consumables.effect.romStart + consumables.effect.bytes;
      if (valid && def.kind === 'magnitude') {
        valid = def.magnitudeOffsets.indexOf(word.romStart) !== -1;
      } else if (valid && def.kind === 'range') {
        valid = word.romStart === def.widthOffset || word.romStart === def.minimumOffset;
      }
      if (!valid) fail('the accepted Phase 5A source-word model drifted');
    });
    (consumables.acceptedSource.nativeOwnerImages || []).forEach(function(image) {
      var key = image.effectSha256 + '|' + image.descriptionSha256;
      var products = acceptedConsumableProductsByOwnerImage[key] || [];
      products.push(Object.freeze({
        source: 'accepted-phase5a-product',
        lifecycle: 'accepted-product',
        product: image.product,
        romSha256: image.romSha256,
      }));
      acceptedConsumableProductsByOwnerImage[key] = products;
    });
    if (data.alignmentCorrection) {
      var correction = data.alignmentCorrection;
      if (payloadHash !== ALIGNMENT_V10_PAYLOAD_SHA256 || correction.version !== 10 ||
          correction.contractId !== 'lordly-native-rebuild.squad-alignment.v10' ||
          correction.review !== 'pending' || correction.runtime !== 'not-tested') {
        fail('the current Alignment correction contract drifted');
      }
      correction.supersededComponents.forEach(verifyComponent);
      correction.currentConsumableOwnerImages.forEach(function(image) {
        var key = image.effectSha256 + '|' + image.descriptionSha256;
        var products = acceptedConsumableProductsByOwnerImage[key] || [];
        products.push(Object.freeze({source:'alignment-v10-source-product', lifecycle:'source-regenerated-unreviewed',
          product:image.product, romSha256:image.romSha256}));
        acceptedConsumableProductsByOwnerImage[key] = products;
      });
    }
    verifiedData = deepFreeze(data);
    return data;
  }

  function normalizeMode(value, fallback) {
    if (value === PATCH_MODE || value === REBUILD_MODE) return value;
    if (fallback !== undefined) return fallback;
    throw new Error('Unknown ROM build mode "' + value + '".');
  }

  function modeInfo(value) {
    return MODES[normalizeMode(value, PATCH_MODE)];
  }

  function getMode(rom) {
    return normalizeMode(rom && rom.buildMode, PATCH_MODE);
  }

  function setMode(rom, value) {
    if (!rom) throw new Error('Load a ROM before changing its build mode.');
    rom.buildMode = normalizeMode(value);
    return rom.buildMode;
  }

  function readU16(bytes, offset) {
    return ((bytes[offset] << 8) | bytes[offset + 1]) >>> 0;
  }

  function readU32(bytes, offset) {
    return ((bytes[offset] << 24) | (bytes[offset + 1] << 16) |
      (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
  }

  function writeU16(bytes, offset, value) {
    bytes[offset] = (value >>> 8) & 0xFF;
    bytes[offset + 1] = value & 0xFF;
  }

  function writeU32(bytes, offset, value) {
    bytes[offset] = (value >>> 24) & 0xFF;
    bytes[offset + 1] = (value >>> 16) & 0xFF;
    bytes[offset + 2] = (value >>> 8) & 0xFF;
    bytes[offset + 3] = value & 0xFF;
  }

  function signed16(value) {
    return (value & 0x8000) ? value - 0x10000 : value;
  }

  function sameModels(left, right) {
    return JSON.stringify(OB64.consumableEffects.validateAllModels(left)) ===
      JSON.stringify(OB64.consumableEffects.validateAllModels(right));
  }

  function consumableOwnerImageKey(bytes) {
    var data = verifyData().consumables;
    var effect = bytes.slice(data.effect.romStart,
      data.effect.romStart + data.effect.bytes);
    var descriptions = bytes.slice(data.descriptions.romStart,
      data.descriptions.romStart + data.descriptions.bytes);
    return sha256(effect) + '|' + sha256(descriptions);
  }

  function consumableProvenance(bytes) {
    var key = consumableOwnerImageKey(bytes);
    var issued = issuedConsumableProvenance.get(bytes);
    if (issued) {
      var currentRomSha256 = sha256(bytes);
      if (issued.ownerImageKey !== key || issued.romSha256 !== currentRomSha256) {
        return {
          trusted: false,
          mismatch: true,
          expectedOwnerImageKey: issued.ownerImageKey,
          expectedRomSha256: issued.romSha256,
          ownerImageKey: key,
          romSha256: currentRomSha256,
        };
      }
      return {
        trusted: true,
        source: issued.source,
        lifecycle: issued.lifecycle,
        product: issued.product,
        planSignature: issued.planSignature,
        ownerImageKey: key,
        romSha256: currentRomSha256,
      };
    }
    var acceptedProducts = acceptedConsumableProductsByOwnerImage[key] || [];
    if (acceptedProducts.length) {
      var fullRomSha256 = sha256(bytes);
      for (var index = 0; index < acceptedProducts.length; index++) {
        var accepted = acceptedProducts[index];
        if (accepted.romSha256 === fullRomSha256) {
          return {
            trusted: true,
            source: accepted.source,
            lifecycle: accepted.lifecycle,
            product: accepted.product,
            romSha256: fullRomSha256,
            ownerImageKey: key,
          };
        }
      }
    }
    return { trusted: false, mismatch: false, ownerImageKey: key };
  }

  function pendingConsumableProvenance(bytes, plan) {
    return Object.freeze({
      trusted: false,
      source: 'issued-phase5b-plan',
      lifecycle: 'awaiting-validation',
      ownerImageKey: consumableOwnerImageKey(bytes),
      planSignature: issuedPlanSignatures.get(plan),
    });
  }

  function registerValidatedConsumableProvenance(bytes, plan) {
    var key = consumableOwnerImageKey(bytes);
    var romSha256 = sha256(bytes);
    var acceptedProducts = acceptedConsumableProductsByOwnerImage[key] || [];
    var acceptedProduct = null;
    for (var index = 0; index < acceptedProducts.length; index++) {
      if (acceptedProducts[index].romSha256 === romSha256) {
        acceptedProduct = acceptedProducts[index];
        break;
      }
    }
    var record = Object.freeze({
      trusted: true,
      source: 'validated-phase5b-candidate',
      lifecycle: 'validated-candidate',
      product: acceptedProduct && acceptedProduct.product,
      ownerImageKey: key,
      romSha256: romSha256,
      planSignature: issuedPlanSignatures.get(plan),
    });
    issuedConsumableProvenance.set(bytes, record);
    return record;
  }

  function buildConsumableEffectSection(models) {
    var spec = verifyData().consumables.effect;
    var effects = OB64.consumableEffects;
    var checked = effects.validateAllModels(models);
    var section = hexBytes(spec.retailHex);
    effects.MODEL_ORDER.forEach(function(modelKey) {
      var def = effects.MODEL_DEFS[modelKey];
      if (def.kind === 'magnitude') {
        effects.encodeMagnitude(modelKey, checked[modelKey]).words.forEach(function(word) {
          writeU32(section, word.offset - spec.romStart, word.word);
        });
      } else {
        var encoded = effects.encodeRange(modelKey, checked[modelKey]);
        writeU32(section, def.widthOffset - spec.romStart, encoded.widthWord);
        writeU32(section, def.minimumOffset - spec.romStart, encoded.minimumWord);
      }
    });
    return { bytes: section, models: checked, sha256: sha256(section) };
  }

  function decodeConsumableEffectSection(bytes) {
    var spec = verifyData().consumables.effect;
    var effects = OB64.consumableEffects;
    var retail = hexBytes(spec.retailHex);
    var sourceWordOffsets = Object.create(null);
    spec.sourceWords.forEach(function(word) {
      sourceWordOffsets[word.romStart - spec.romStart] = true;
    });
    for (var index = 0; index < spec.bytes; index++) {
      if (sourceWordOffsets[index - (index % 4)]) continue;
      if (bytes[spec.romStart + index] !== retail[index]) {
        throw new Error('immutable effect-function byte differs at section offset ' + index);
      }
    }
    var models = {};
    effects.MODEL_ORDER.forEach(function(modelKey) {
      var def = effects.MODEL_DEFS[modelKey];
      if (def.kind === 'magnitude') {
        var values = def.magnitudeOffsets.map(function(offset, formIndex) {
          var word = readU32(bytes, offset);
          if ((word & 0xFFFF0000) !== def.magnitudeForms[formIndex]) {
            throw new Error(modelKey + ' opcode form is malformed');
          }
          return word & 0xFFFF;
        });
        if (!values.every(function(value) { return value === values[0]; })) {
          throw new Error(modelKey + ' paired words disagree');
        }
        models[modelKey] = { magnitude: effects.validateMagnitude(modelKey, values[0]).magnitude };
      } else {
        var widthWord = readU32(bytes, def.widthOffset);
        var minimumWord = readU32(bytes, def.minimumOffset);
        if ((widthWord & 0xFFFF0000) !== 0x24030000 ||
            (minimumWord & 0xFFFF0000) !== 0x24680000) {
          throw new Error(modelKey + ' opcode form is malformed');
        }
        var width = widthWord & 0xFFFF;
        var minimum = signed16(minimumWord & 0xFFFF);
        var checked = effects.validateRange(modelKey, minimum, minimum + width - 1);
        models[modelKey] = { minimum: checked.minimum, maximum: checked.maximum };
      }
    });
    var rebuilt = buildConsumableEffectSection(models).bytes;
    var actual = bytes.slice(spec.romStart, spec.romStart + spec.bytes);
    if (!equalBytes(actual, rebuilt)) {
      throw new Error('complete effect-function image does not match its decoded source model');
    }
    return {
      models: models,
      retail: equalBytes(actual, retail),
      sha256: sha256(actual),
    };
  }

  function healingValueFromText(text, id) {
    var matches = String(text).match(/[0-9]+(?=HP)/g) || [];
    if (matches.length !== 1) {
      throw new Error('healing description ' + id +
        ' must contain exactly one complete numeric value before HP');
    }
    return Number(matches[0]);
  }

  function classifyConsumableOwner(bytes) {
    var spec = verifyData().consumables;
    var provenance = consumableProvenance(bytes);
    var effect;
    var block;
    try {
      effect = decodeConsumableEffectSection(bytes);
    } catch (error) {
      return {
        state: 'foreign',
        ownerState: provenance.mismatch ? 'partial-native' : 'partial-legacy',
        reason: error.message,
        provenance: provenance,
        models: null,
      };
    }
    try {
      block = OB64.descriptionCodec.parseBlock('consumables', bytes);
      if (!block || block.records.length !== spec.descriptions.recordCount ||
          block.meta.compressedSize > spec.descriptions.streamCapacity) {
        throw new Error('complete description-slot geometry is malformed');
      }
      block.records.forEach(function(record, index) {
        if (!record || record.id !== index) {
          throw new Error('description record order is malformed at ' + index);
        }
      });
    } catch (error) {
      return {
        state: 'foreign',
        ownerState: provenance.mismatch ? 'partial-native' : 'partial-legacy',
        reason: error.message,
        provenance: provenance,
        models: effect.models,
      };
    }
    if (provenance.mismatch) {
      return {
        state: 'foreign',
        ownerState: 'partial-native',
        reason: 'the validated candidate changed after its trusted receipt was issued',
        provenance: provenance,
        models: effect.models,
        descriptions: block,
      };
    }
    var patchCompatible = !provenance.trusted;
    var healing;
    try {
      healing = [1, 2, 3].map(function(id) {
        return healingValueFromText(block.records[id].editableText, id);
      });
    } catch (error) {
      if (patchCompatible) {
        return {
          state: 'legacy',
          ownerState: 'legacy-complete',
          reason: error.message,
          provenance: provenance,
          models: effect.models,
          descriptions: block,
          patchCompatible: true,
          healingSynchronized: false,
        };
      }
      return {
        state: 'foreign',
        ownerState: 'partial-native',
        reason: error.message,
        provenance: provenance,
        models: effect.models,
      };
    }
    var expectedHealing = [
      effect.models.healLeaf.magnitude,
      effect.models.healSeed.magnitude,
      effect.models.healPack.magnitude,
    ];
    if (healing.join('|') !== expectedHealing.join('|')) {
      return {
        state: 'mixed',
        ownerState: 'mixed',
        reason: 'effect values and healing descriptions do not reconcile',
        provenance: provenance,
        models: effect.models,
        descriptions: block,
        patchCompatible: patchCompatible,
        healingSynchronized: false,
      };
    }
    var descriptionBytes = bytes.slice(spec.descriptions.romStart,
      spec.descriptions.romStart + spec.descriptions.bytes);
    var retail = effect.retail && equalBytes(descriptionBytes,
      hexBytes(spec.descriptions.retailHex));
    if (retail) {
      return {
        state: 'retail', ownerState: 'retail', models: effect.models,
        descriptions: block, provenance: provenance, patchCompatible: true,
        healingSynchronized: true,
      };
    }
    return {
      state: provenance.trusted ? 'native' : 'legacy',
      ownerState: provenance.trusted ? 'native-complete' : 'legacy-complete',
      models: effect.models,
      descriptions: block,
      provenance: provenance,
      patchCompatible: patchCompatible,
      healingSynchronized: true,
    };
  }

  function normalizedHealingText(text, id) {
    healingValueFromText(text, id);
    return String(text).replace(/[0-9]+(?=HP)/, '###');
  }

  function buildConsumableDescriptionSlot(rom, retailBase, models) {
    var spec = verifyData().consumables.descriptions;
    var effects = OB64.consumableEffects;
    var codec = OB64.descriptionCodec;
    if (!rom.consumableDescriptions || !Array.isArray(rom.consumableDescriptions.records) ||
        rom.consumableDescriptions.records.length !== spec.recordCount) {
      fail('the complete in-memory consumable description model is unavailable');
    }
    var current = rom.consumableDescriptions;
    var retail = codec.parseBlock('consumables', retailBase);
    var manualIds = [];
    current.records.forEach(function(record, id) {
      if (!record || record.id !== id || !retail.records[id]) {
        fail('the complete in-memory consumable description model is malformed');
      }
      var currentText = record.editableText;
      var retailText = retail.records[id].editableText;
      if (id >= 1 && id <= 3) {
        currentText = normalizedHealingText(currentText, id);
        retailText = normalizedHealingText(retailText, id);
      }
      if (currentText !== retailText) manualIds.push(id);
    });
    var values = [models.healLeaf.magnitude, models.healSeed.magnitude,
      models.healPack.magnitude];
    var retailValues = [1, 2, 3].map(function(id) {
      return healingValueFromText(retail.records[id].editableText, id);
    });
    var retailSlot = hexBytes(spec.retailHex);
    if (!manualIds.length && values.join('|') === retailValues.join('|')) {
      return {
        bytes: retailSlot,
        encodingPasses: 0,
        compressedBytes: retail.meta.compressedSize,
        capacity: spec.streamCapacity,
        manualIds: [],
        healingValues: values,
        logicalOrder: spec.logicalOrder.slice(),
        mode: 'passthrough',
      };
    }
    if (!manualIds.length) {
      var manifest = effects.REVISION_MANIFESTS.rev0 ||
        effects.REVISION_MANIFESTS['us-rev0'];
      var inspected = effects.inspectHealingDescription(retailBase, manifest);
      if (!inspected.ok) fail(inspected.reason);
      var legacyCompatible = effects.buildHealingDescriptionSlot(
        inspected.decoded, values, retailSlot);
      if (legacyCompatible.slot.length !== spec.bytes ||
          legacyCompatible.bytesConsumed > spec.streamCapacity) {
        fail('the unified healing description stream exceeds its accepted slot');
      }
      return {
        bytes: legacyCompatible.slot.slice(),
        encodingPasses: 1,
        compressedBytes: legacyCompatible.bytesConsumed,
        capacity: spec.streamCapacity,
        manualIds: [],
        healingValues: values,
        logicalOrder: spec.logicalOrder.slice(),
        mode: 'legacy-compatible-unified-healing',
      };
    }
    var staged = current;
    [1, 2, 3].forEach(function(id, index) {
      var text = staged.records[id].editableText;
      healingValueFromText(text, id);
      staged = codec.replaceText(staged, id,
        String(text).replace(/[0-9]+(?=HP)/,
          String(values[index]).padStart(3, '0')));
    });
    var scratch = retailBase.slice();
    var measurement = codec.serializeBlock(staged, scratch);
    if (measurement.compressedSize > spec.streamCapacity) {
      fail('the unified consumable description stream exceeds its accepted slot');
    }
    return {
      bytes: scratch.slice(spec.romStart, spec.romStart + spec.bytes),
      encodingPasses: 1,
      compressedBytes: measurement.compressedSize,
      capacity: spec.streamCapacity,
      manualIds: manualIds,
      healingValues: values,
      logicalOrder: spec.logicalOrder.slice(),
      mode: 'unified-manual-and-healing',
    };
  }

  function float32Bits(value) {
    var buffer = new ArrayBuffer(4);
    var view = new DataView(buffer);
    view.setFloat32(0, value, false);
    return view.getUint32(0, false);
  }

  function float32FromBits(value) {
    var buffer = new ArrayBuffer(4);
    var view = new DataView(buffer);
    view.setUint32(0, value >>> 0, false);
    return view.getFloat32(0, false);
  }

  function wordsToBytes(words) {
    var out = new Uint8Array(words.length * 4);
    for (var index = 0; index < words.length; index++) writeU32(out, index * 4, words[index]);
    return out;
  }

  function experienceBytes(percent) {
    var data = verifyData();
    var spec = data.experience;
    if (!Number.isInteger(percent) || percent < spec.minimumPercent ||
        percent > spec.maximumPercent) {
      fail('Experience Scale must be an integer within ' + spec.minimumPercent +
        '..' + spec.maximumPercent + ' percent');
    }
    var out = hexBytes(spec.retailHex);
    if (percent !== spec.retailPercent) {
      var scaleBits = float32Bits(percent / 100);
      for (var siteIndex = 0; siteIndex < spec.sites.length; siteIndex++) {
        var site = spec.sites[siteIndex];
        var words = site.modifiedWords.slice();
        if (siteIndex === 0) {
          words[0] = (0x3C010000 | (scaleBits >>> 16)) >>> 0;
          words[1] = (0x34210000 | (scaleBits & 0xFFFF)) >>> 0;
        }
        out.set(wordsToBytes(words), site.romStart - spec.romStart);
      }
    }
    if (sha256(out) !== spec.coverageSha256[percent]) {
      fail('Experience Scale lacks authenticated rebuilt-source coverage for ' + percent);
    }
    return out;
  }

  function detectExperience(bytes) {
    var spec = verifyData().experience;
    var retail = hexBytes(spec.retailHex);
    if (equalAt(bytes, spec.romStart, retail)) return { state: 'retail', percent: 100 };
    var feature = OB64.tools && OB64.tools.getFeature(spec.toolId);
    var legacy = feature && OB64.tools.featureState(bytes, feature);
    var first = spec.sites[0].romStart;
    var highWord = readU32(bytes, first);
    var lowWord = readU32(bytes, first + 4);
    if ((highWord & 0xFFFF0000) === 0x3C010000 &&
        (lowWord & 0xFFFF0000) === 0x34210000) {
      var percent = Math.round(float32FromBits(
        (((highWord & 0xFFFF) << 16) | (lowWord & 0xFFFF)) >>> 0) * 100);
      if (Number.isInteger(percent) && percent >= spec.minimumPercent &&
          percent <= spec.maximumPercent &&
          equalAt(bytes, spec.romStart, experienceBytes(percent))) {
        return {
          state: 'native',
          percent: percent,
          legacyEquivalent: legacy === 'applied' || legacy === 'outdated',
        };
      }
    }
    return { state: legacy === 'applied' || legacy === 'outdated' ? 'legacy' : 'foreign',
      percent: null };
  }

  function phase3Components(toolId) {
    var data = verifyData();
    var owner = data.phase3.toolOwnerMap[toolId];
    return data.phase3.components.filter(function(component) { return component.owner === owner; });
  }

  function detectPhase3Owner(bytes, toolId) {
    var components = phase3Components(toolId);
    var nativeCount = 0;
    var retailCount = 0;
    components.forEach(function(component) {
      if (equalAt(bytes, component.romStart, componentBytes(component, 'native'))) nativeCount++;
      if (equalAt(bytes, component.romStart, componentBytes(component, 'retail'))) retailCount++;
    });
    if (retailCount === components.length) return { state: 'retail' };
    if (nativeCount === components.length) return { state: 'native' };
    var correction = verifyData().alignmentCorrection;
    if (toolId === 'squad-menu-alignment' && correction && correction.supersededComponents.every(function(component) {
      return equalAt(bytes, component.romStart, hexBytes(component.dataHex));
    })) return { state: 'native', moduleVersion: 9, updateRequired: true };

    var feature = OB64.tools && OB64.tools.getFeature(toolId);
    var legacy = feature && OB64.tools.featureState(bytes, feature);
    if (legacy === 'applied' || legacy === 'outdated') return { state: 'legacy' };
    return { state: nativeCount ? 'mixed' : 'foreign', nativeComponents: nativeCount };
  }

  function checkedIds(values, minimum, maximum, capacity, label) {
    if (!Array.isArray(values)) fail(label + ' must be an array');
    if (values.length > capacity) {
      fail(label + ' contains ' + values.length + ' entries; capacity is ' + capacity);
    }
    var seen = Object.create(null);
    return values.map(function(raw, index) {
      var value = raw;
      if (typeof value !== 'number' || !Number.isInteger(value)) {
        fail(label + '[' + index + '] is not an integer');
      }
      if (value === 0) fail(label + '[' + index + '] uses reserved identifier zero');
      if (value < minimum || value > maximum) {
        fail(label + '[' + index + '] identifier ' + value + ' is outside ' +
          minimum + '..' + maximum);
      }
      if (seen[value]) fail(label + ' contains duplicate identifier ' + value);
      seen[value] = true;
      return value;
    });
  }

  function normalizeShopRecords(records) {
    var contract = verifyData().shops.contract;
    if (!Array.isArray(records) || records.length !== contract.recordCount) {
      fail('the Shop contract must contain exactly ' + contract.recordCount + ' ordered records');
    }
    return records.map(function(record, index) {
      if (!record || typeof record !== 'object' || Array.isArray(record) ||
          !Number.isInteger(record.index) || record.index !== index) {
        fail('Shop record order or shape is invalid at index ' + index);
      }
      var allowed = {
        index: true, equipment: true, expendables: true,
        items: true, consumables: true,
      };
      if (Object.keys(record).some(function(key) { return !allowed[key]; })) {
        fail('Shop record ' + index + ' contains an unknown field or flag');
      }
      var equipment = checkedIds(
        record.equipment == null ? record.items : record.equipment,
        contract.equipmentIdMin, contract.equipmentIdMax,
        contract.equipmentCapacity, 'Shop #' + index + ' equipment');
      var expendables = checkedIds(
        record.expendables == null ? record.consumables : record.expendables,
        contract.expendableIdMin, contract.expendableIdMax,
        contract.expendableCapacity, 'Shop #' + index + ' Expendables');
      if (index === 0 && (equipment.length || expendables.length)) {
        fail('Shop record zero must remain an empty sentinel');
      }
      return { index: index, equipment: equipment, expendables: expendables };
    });
  }

  function recordsFromRom(rom) {
    var contract = verifyData().shops.contract;
    if (!rom || !Array.isArray(rom.shops) || rom.shops.length < contract.recordCount) {
      fail('the parsed ROM does not expose all 36 Shop records');
    }
    var records = [];
    for (var index = 0; index < contract.recordCount; index++) {
      var shop = rom.shops[index] || {};
      var originalShop = rom.original && rom.original.shops &&
        rom.original.shops[index] || {};
      if (index === 0 &&
          (!equalBytes(shop.items || [], originalShop.items || []) ||
           !equalBytes(shop.consumables || [], originalShop.consumables || [])) &&
          ((shop.items || []).length || (shop.consumables || []).length)) {
        fail('Shop record zero is a sentinel and cannot be edited');
      }
      records.push({
        index: index,
        equipment: index === 0 ? [] : (shop.items || []).slice(),
        expendables: index === 0 ? [] : (shop.consumables || []).slice(),
      });
    }
    for (var extra = contract.recordCount; extra < rom.shops.length; extra++) {
      if (!rom.original || !rom.original.shops) continue;
      var currentExtra = rom.shops[extra] || {};
      var originalExtra = rom.original.shops[extra] || {};
      if (!equalBytes(currentExtra.items || [], originalExtra.items || []) ||
          !equalBytes(currentExtra.consumables || [], originalExtra.consumables || [])) {
        fail('Shop #' + extra + ' is outside the accepted 0..35 native contract');
      }
    }
    return normalizeShopRecords(records);
  }

  function encodeShopContract(records) {
    records = normalizeShopRecords(records);
    var data = verifyData();
    var contract = data.shops.contract;
    var out = new Uint8Array(contract.totalBytes);
    writeU32(out, 0x00, contract.magic);
    writeU16(out, 0x04, contract.version);
    writeU16(out, 0x06, contract.headerBytes);
    writeU16(out, 0x08, contract.recordCount);
    writeU16(out, 0x0A, contract.realShopCount);
    writeU16(out, 0x0C, contract.recordStride);
    writeU16(out, 0x0E, contract.equipmentCapacity);
    writeU16(out, 0x10, contract.expendableCapacity);
    writeU16(out, 0x12, 0x0007);
    writeU16(out, 0x14, contract.equipmentIdMin);
    writeU16(out, 0x16, contract.equipmentIdMax);
    writeU16(out, 0x18, contract.expendableIdMin);
    writeU16(out, 0x1A, contract.expendableIdMax);
    writeU32(out, 0x1C, contract.headerBytes);
    writeU32(out, 0x20, contract.totalBytes);
    writeU32(out, 0x24, contract.recordCount * contract.recordStride);
    out.set(hexBytes(data.shops.contractIdentityHex), 0x28);
    records.forEach(function(record) {
      var base = contract.headerBytes + record.index * contract.recordStride;
      out[base] = record.index;
      out[base + 1] = record.index === 0 ? 1 : 2;
      out[base + 2] = record.equipment.length;
      out[base + 3] = record.expendables.length;
      record.equipment.forEach(function(value, index) {
        writeU16(out, base + 0x04 + index * 2, value);
      });
      record.expendables.forEach(function(value, index) {
        writeU16(out, base + 0x6A + index * 2, value);
      });
    });
    return out;
  }

  function decodeShopContract(bytes) {
    var data = verifyData();
    var contract = data.shops.contract;
    if (!(bytes instanceof Uint8Array) || bytes.length !== contract.totalBytes) {
      fail('the Shop contract byte count is invalid');
    }
    if (readU32(bytes, 0x00) !== (contract.magic >>> 0) ||
        readU16(bytes, 0x04) !== contract.version ||
        readU16(bytes, 0x06) !== contract.headerBytes ||
        readU16(bytes, 0x08) !== contract.recordCount ||
        readU16(bytes, 0x0A) !== contract.realShopCount ||
        readU16(bytes, 0x0C) !== contract.recordStride ||
        readU16(bytes, 0x0E) !== contract.equipmentCapacity ||
        readU16(bytes, 0x10) !== contract.expendableCapacity ||
        readU16(bytes, 0x12) !== 0x0007 ||
        readU16(bytes, 0x14) !== contract.equipmentIdMin ||
        readU16(bytes, 0x16) !== contract.equipmentIdMax ||
        readU16(bytes, 0x18) !== contract.expendableIdMin ||
        readU16(bytes, 0x1A) !== contract.expendableIdMax ||
        readU32(bytes, 0x1C) !== contract.headerBytes ||
        readU32(bytes, 0x20) !== contract.totalBytes ||
        readU32(bytes, 0x24) !== contract.recordCount * contract.recordStride ||
        !equalBytes(bytes.slice(0x28, 0x38), hexBytes(data.shops.contractIdentityHex))) {
      fail('the Shop contract header or identity is incompatible');
    }
    for (var reserved = 0x38; reserved < 0x40; reserved++) {
      if (bytes[reserved] !== 0) fail('the Shop contract has unknown header flags');
    }
    var records = [];
    for (var index = 0; index < contract.recordCount; index++) {
      var base = contract.headerBytes + index * contract.recordStride;
      if (bytes[base] !== index || bytes[base + 1] !== (index === 0 ? 1 : 2)) {
        fail('Shop record ' + index + ' has an invalid index or unknown flags');
      }
      var equipmentCount = bytes[base + 2];
      var expendableCount = bytes[base + 3];
      if (equipmentCount > contract.equipmentCapacity) {
        fail('Shop #' + index + ' encodes the 51st equipment entry');
      }
      if (expendableCount > contract.expendableCapacity) {
        fail('Shop #' + index + ' encodes the 31st Expendable entry');
      }
      var equipment = [];
      var expendables = [];
      for (var equipmentSlot = 0; equipmentSlot < contract.equipmentSlots; equipmentSlot++) {
        var equipmentValue = readU16(bytes, base + 0x04 + equipmentSlot * 2);
        if (equipmentSlot < equipmentCount) equipment.push(equipmentValue);
        else if (equipmentValue !== 0) fail('Shop #' + index + ' equipment tail is nonzero');
      }
      for (var expendableSlot = 0; expendableSlot < contract.expendableSlots; expendableSlot++) {
        var expendableValue = readU16(bytes, base + 0x6A + expendableSlot * 2);
        if (expendableSlot < expendableCount) expendables.push(expendableValue);
        else if (expendableValue !== 0) fail('Shop #' + index + ' Expendable tail is nonzero');
      }
      records.push({ index: index, equipment: equipment, expendables: expendables });
    }
    records = normalizeShopRecords(records);
    if (!equalBytes(encodeShopContract(records), bytes)) {
      fail('the Shop contract is not in canonical deterministic form');
    }
    return records;
  }

  function shopModuleComponent() {
    return verifyData().shops.components.filter(function(component) {
      return component.name === 'source-linked-shop-module-and-membership-contract';
    })[0];
  }

  function nativeShopComponentState(bytes, component) {
    if (component !== shopModuleComponent()) {
      if (equalAt(bytes, component.romStart, componentBytes(component, 'native'))) return 'native';
      if (equalAt(bytes, component.romStart, componentBytes(component, 'retail'))) return 'retail';
      return 'foreign';
    }
    var data = verifyData();
    var spec = data.shops.module;
    var current = bytes.slice(component.romStart, component.romStart + component.bytes);
    var template = componentBytes(component, 'native');
    var contractEnd = spec.contractOffset + data.shops.contract.totalBytes;
    if (!equalBytes(current.slice(0, spec.contractOffset), template.slice(0, spec.contractOffset)) ||
        !equalBytes(current.slice(contractEnd), template.slice(contractEnd))) {
      if (equalBytes(current, componentBytes(component, 'retail'))) return 'retail';
      return 'foreign';
    }
    decodeShopContract(current.slice(spec.contractOffset, contractEnd));
    return 'native';
  }

  function legacyShopFindings(bytes, rom) {
    var data = verifyData();
    var legacy = data.shops.legacy;
    var findings = [];
    if (OB64.runtimeOverrides && OB64.squad && rom && rom.layout) {
      var layout = OB64.runtimeOverrides.patchLayout(rom);
      var hook = OB64.runtimeOverrides.buildShopHook(layout);
      if (equalAt(bytes, legacy.hookRom, hook)) findings.push('legacy Shop producer hook');
    }
    if (readU32(bytes, legacy.sharedTailRom + legacy.tableOffset) === 0x4F425348) {
      findings.push('legacy Shop membership table');
    }
    var bootWordOffset = legacy.sharedBootstrapRom + (15 + 1) * 4;
    if (readU32(bytes, bootWordOffset) ===
        ((legacy.sharedTailRam + legacy.resolverOffset) >>> 0)) {
      findings.push('legacy Shop bootstrap call edge');
    }
    return findings;
  }

  function detectShops(bytes, rom) {
    var data = verifyData();
    var nativeCount = 0;
    var retailCount = 0;
    var records = null;
    try {
      data.shops.components.forEach(function(component) {
        var state = nativeShopComponentState(bytes, component);
        if (state === 'native') nativeCount++;
        if (state === 'retail') retailCount++;
      });
      if (nativeCount === data.shops.components.length) {
        var start = data.shops.module.contractRomStart;
        records = decodeShopContract(bytes.slice(start, start + data.shops.contract.totalBytes));
      }
    } catch (error) {
      return { state: nativeCount ? 'mixed' : 'foreign', records: null,
        nativeComponents: nativeCount, reason: error.message };
    }
    var legacy = legacyShopFindings(bytes, rom);
    if (nativeCount === data.shops.components.length && !legacy.length) {
      return { state: 'native', records: records, legacyFindings: [] };
    }
    if (nativeCount && (legacy.length || nativeCount !== data.shops.components.length)) {
      return { state: 'mixed', records: null, nativeComponents: nativeCount,
        legacyFindings: legacy };
    }
    if (legacy.length) return { state: 'legacy', records: null, legacyFindings: legacy };
    if (retailCount === data.shops.components.length) {
      return { state: 'retail', records: null, legacyFindings: [] };
    }
    return { state: 'foreign', records: null, legacyFindings: [] };
  }

  function classifySource(bytes, rom) {
    if (!(bytes instanceof Uint8Array) || bytes.length !== 0x02800000) {
      return { state: 'foreign', reason: 'the source is not a 40 MiB normalized ROM' };
    }
    if (rom && rom.layout && rom.layout.id !== 'us-rev0') {
      var unavailableOwners = {
        'experience-size-scale': { state: 'unavailable' },
        'cf-army-counter': { state: 'unavailable' },
        'character-card-luck': { state: 'unavailable' },
        'squad-menu-alignment': { state: 'unavailable' },
        shops: { state: 'unavailable', records: null },
        consumables: { state: 'unavailable', ownerState: 'unavailable', models: null },
      };
      return {
        state: 'not-applicable',
        reason: 'native source ownership is accepted only for US Rev 0',
        owners: unavailableOwners,
        hasNative: false,
        hasExclusiveNative: false,
        hasLegacy: false,
        hasMixed: false,
      };
    }
    var owners = {
      'experience-size-scale': detectExperience(bytes),
      'cf-army-counter': detectPhase3Owner(bytes, 'cf-army-counter'),
      'character-card-luck': detectPhase3Owner(bytes, 'character-card-luck'),
      'squad-menu-alignment': detectPhase3Owner(bytes, 'squad-menu-alignment'),
      shops: detectShops(bytes, rom),
      consumables: classifyConsumableOwner(bytes),
    };
    var states = Object.keys(owners).map(function(key) { return owners[key].state; });
    var exclusiveNativeOwners = Object.keys(owners).filter(function(key) {
      return owners[key].state === 'native' && !owners[key].legacyEquivalent;
    });
    return {
      state: states.some(function(state) { return state === 'mixed' || state === 'foreign'; })
        ? 'foreign'
        : (states.some(function(state) { return state === 'legacy'; })
          ? 'legacy'
          : (states.some(function(state) { return state === 'native'; }) ? 'native' : 'retail')),
      owners: owners,
      hasNative: states.indexOf('native') !== -1,
      hasExclusiveNative: exclusiveNativeOwners.length !== 0,
      hasLegacy: states.indexOf('legacy') !== -1,
      hasMixed: states.indexOf('mixed') !== -1 || states.indexOf('foreign') !== -1,
    };
  }

  function applyDecodedShopRecords(rom, records) {
    records.forEach(function(record) {
      if (!rom.shops[record.index]) fail('the ROM parser omitted Shop #' + record.index);
      rom.shops[record.index].items = record.equipment.slice();
      rom.shops[record.index].consumables = record.expendables.slice();
      rom.shops[record.index].nativeOverride = true;
      rom.shops[record.index].runtimeOverride = false;
    });
    rom.nativeShopOverridesDetected = records.length - 1;
  }

  function copyParsedShopRecords(rom) {
    return (rom.shops || []).slice(0, 36).map(function(shop, index) {
      return {
        index: index,
        equipment: (shop && shop.items || []).slice(),
        expendables: (shop && shop.consumables || []).slice(),
      };
    });
  }

  function initializeSource(rom) {
    verifyData();
    if (!rom || !(rom.z64 instanceof Uint8Array)) fail('the loaded ROM image is unavailable');
    var classification = classifySource(rom.z64, rom);
    var retailShopRecords = copyParsedShopRecords(rom);
    rom.nativeRebuild = {
      sourceZ64: rom.z64.slice(),
      sourceClassification: classification,
      sourceSha256: sha256(rom.z64),
      acceptedCommit: ACCEPTED_COMMIT,
      acceptedTree: ACCEPTED_TREE,
      lastExport: null,
      pendingRebuildDirty: {},
      shopSelected: classification.owners.shops.state === 'native',
      shopSelectionExplicitlyDisabled: false,
      retailShopRecords: retailShopRecords,
    };
    // Phase 1 reused the legacy Editor's exact Experience Scale bytes. A ROM
    // with other legacy migrated owners is therefore a Patch product, not a
    // mixed product, unless another owner has an exclusively native identity.
    rom.buildMode = classification.hasNative &&
      (classification.hasExclusiveNative || !classification.hasLegacy)
      ? REBUILD_MODE : PATCH_MODE;
    rom.nativeRebuild.initialBuildMode = rom.buildMode;
    if (classification.owners.shops.state === 'native') {
      applyDecodedShopRecords(rom, classification.owners.shops.records);
    }
    return classification;
  }

  function initializeToolState(rom) {
    if (!rom || !rom.tools || !rom.nativeRebuild) return;
    var owners = rom.nativeRebuild.sourceClassification.owners;
    MIGRATED_TOOL_IDS.forEach(function(toolId) {
      var state = owners[toolId];
      if (!state || state.state !== 'native') return;
      rom.tools.initial[toolId] = 'applied';
      rom.tools.desired[toolId] = true;
      if (toolId === 'experience-size-scale') {
        rom.tools.initialValues[toolId] = state.percent;
        rom.tools.desiredValues[toolId] = state.percent;
        rom.tools.desired[toolId] = state.percent !== 100;
      }
    });
  }

  function displayToolState(rom, feature, fallback) {
    if (!rom || !rom.nativeRebuild || MIGRATED_TOOL_IDS.indexOf(feature.id) === -1) {
      return fallback;
    }
    var owner = rom.nativeRebuild.sourceClassification.owners[feature.id];
    if (owner && owner.state === 'native') return 'applied';
    return fallback;
  }

  function displayToolPercent(rom, feature, fallback) {
    if (!rom || !rom.nativeRebuild || feature.id !== 'experience-size-scale') return fallback;
    var owner = rom.nativeRebuild.sourceClassification.owners[feature.id];
    return owner && owner.state === 'native' ? owner.percent : fallback;
  }

  function shopsChanged(rom) {
    if (!rom || !rom.original || !rom.original.shops) return false;
    for (var index = 0; index < 36; index++) {
      var current = rom.shops[index] || {};
      var original = rom.original.shops[index] || {};
      if (!equalBytes(current.items || [], original.items || []) ||
          !equalBytes(current.consumables || [], original.consumables || [])) return true;
    }
    return false;
  }

  function shopOwnerSelected(rom) {
    if (!rom || !rom.nativeRebuild) return false;
    return !!rom.nativeRebuild.shopSelected ||
      (!rom.nativeRebuild.shopSelectionExplicitlyDisabled && shopsChanged(rom));
  }

  function setShopOwnerSelected(rom, selected) {
    if (!rom || !rom.nativeRebuild) return;
    rom.nativeRebuild.shopSelected = !!selected;
    rom.nativeRebuild.shopSelectionExplicitlyDisabled = !selected;
    if (selected) return;
    var retailRecords = rom.nativeRebuild.retailShopRecords;
    if (!Array.isArray(retailRecords) || retailRecords.length !== 36) {
      fail('the retail Shop model needed to disable native Shop ownership is unavailable');
    }
    retailRecords.forEach(function(record) {
      var shop = rom.shops[record.index];
      if (!shop) fail('the ROM parser omitted Shop #' + record.index);
      shop.items = record.equipment.slice();
      shop.consumables = record.expendables.slice();
      shop.nativeOverride = false;
      shop.runtimeOverride = false;
    });
    rom.nativeShopOverridesDetected = 0;
  }

  function restoreComponent(base, component) {
    base.set(componentBytes(component, 'retail'), component.romStart);
  }

  function ownerRecord(id, name, components, category) {
    var regions = [];
    components.forEach(function(component) {
      regions.push({
        kind: 'rom', start: component.romStart, size: component.bytes,
        label: component.name,
      });
      if (Number.isInteger(component.ramStart)) {
        regions.push({
          kind: 'ram', start: component.ramStart, size: component.bytes,
          label: component.name + ' runtime',
        });
      }
    });
    return {
      id: id,
      name: name,
      category: category || 'nativeRebuild',
      regions: regions,
    };
  }

  function regionSize(region) {
    if (Number.isInteger(region.size)) return region.size;
    if (Number.isInteger(region.endExclusive) && Number.isInteger(region.start)) {
      return region.endExclusive - region.start;
    }
    return 0;
  }

  function regionContainedIn(region, bound) {
    var size = regionSize(region);
    return size > 0 && Number.isInteger(region.start) &&
      region.start >= bound.start && region.start + size <= bound.start + bound.size;
  }

  function nativeOwnerRegions(ownerId) {
    var data = verifyData();
    if (ownerId === data.experience.owner) {
      return ownerRecord(ownerId, ownerId, [{
        romStart: data.experience.romStart,
        ramStart: data.experience.ramStart,
        bytes: data.experience.bytes,
        name: 'accepted source-owned Experience Scale function',
      }]).regions;
    }
    if (ownerId === data.shops.owner) {
      return ownerRecord(ownerId, ownerId, data.shops.components).regions;
    }
    if (ownerId === data.consumables.effect.owner) {
      return ownerRecord(ownerId, ownerId, [{
        romStart: data.consumables.effect.romStart,
        ramStart: data.consumables.effect.ramStart,
        bytes: data.consumables.effect.bytes,
        name: 'accepted source-owned consumable effect function',
      }]).regions;
    }
    if (ownerId === data.consumables.descriptions.owner) {
      return ownerRecord(ownerId, ownerId, [{
        romStart: data.consumables.descriptions.romStart,
        bytes: data.consumables.descriptions.bytes,
        name: 'unified consumable description slot',
      }]).regions;
    }
    var phase3 = data.phase3.components.filter(function(component) {
      return component.owner === ownerId;
    });
    return phase3.length ? ownerRecord(ownerId, ownerId, phase3).regions : null;
  }

  function normalizedRegionKeys(regions) {
    return regions.map(function(region) {
      if ((region.kind !== 'rom' && region.kind !== 'ram') ||
          !Number.isInteger(region.start) || !Number.isInteger(regionSize(region)) ||
          regionSize(region) <= 0) {
        fail('an ownership region is malformed');
      }
      return region.kind + ':' + region.start + ':' + regionSize(region);
    }).sort();
  }

  function authorizeComposition(ownerRecords) {
    var data = verifyData();
    var composition = data.composition;
    var definitions = Object.create(null);
    composition.owners.forEach(function(owner) { definitions[owner.id] = owner; });
    var resolved = Object.create(null);
    (ownerRecords || []).forEach(function(owner) {
      if (!owner || typeof owner.id !== 'string' || !Array.isArray(owner.regions)) {
        fail('the production ownership ledger contains a malformed owner');
      }
      var isNativeLayout = owner.category === 'nativeRebuild' ||
        owner.category === 'nativeRebuildRestoration';
      var stableId = isNativeLayout
        ? owner.id
        : (composition.productionOwnerIds[owner.id] ||
          (definitions[owner.id] ? owner.id : null));
      if (!stableId) {
        fail('the Phase 2 ownership manifest does not authorize producer "' + owner.id + '"');
      }
      if (!isNativeLayout && composition.retiredOwners.indexOf(stableId) !== -1) {
        fail('legacy producer "' + owner.id + '" uses retired physical owner "' +
          stableId + '" in Rebuild ROM');
      }
      if (!isNativeLayout && composition.migratedOwners.indexOf(stableId) !== -1) {
        fail('legacy producer "' + owner.id + '" cannot own migrated contributor "' +
          stableId + '" in Rebuild ROM');
      }
      var definition = definitions[stableId];
      if (isNativeLayout) {
        if (composition.migratedOwners.indexOf(stableId) === -1) {
          fail('native producer "' + stableId + '" is not in the accepted migration stack');
        }
        if (resolved[stableId]) {
          fail('native producer "' + stableId + '" has more than one physical owner');
        }
        var expectedNativeRegions = nativeOwnerRegions(stableId);
        if (!expectedNativeRegions ||
            normalizedRegionKeys(owner.regions).join('|') !==
              normalizedRegionKeys(expectedNativeRegions).join('|')) {
          fail('native producer "' + stableId + '" does not match its authenticated layout');
        }
      } else {
        if (!definition || definition.profile !== 'supported') {
          fail('passthrough producer "' + stableId + '" lacks accepted US Rev 0 coverage');
        }
        owner.regions.forEach(function(region) {
          normalizedRegionKeys([region]);
          var kind = region.kind === 'ram' ? 'ram' : 'rom';
          var bounds = definition[kind] || [];
          if (!bounds.some(function(bound) { return regionContainedIn(region, bound); })) {
            fail('passthrough producer "' + stableId + '" exceeds its accepted ' +
              kind.toUpperCase() + ' ownership boundary at ' + region.start);
          }
        });
      }
      if (resolved[stableId] &&
          resolved[stableId].productionIds.indexOf(owner.id) !== -1) {
        fail('producer "' + owner.id + '" appears more than once in the ownership ledger');
      }
      if (!resolved[stableId]) {
        resolved[stableId] = {
          id: stableId,
          productionIds: [],
          order: definition ? definition.order : 80,
          ownershipClass: definition ? definition.ownershipClass : 'native-source',
          regions: [],
        };
      }
      resolved[stableId].productionIds.push(owner.id);
      resolved[stableId].regions = resolved[stableId].regions.concat(
        owner.regions.map(function(region) { return Object.assign({}, region); }));
    });
    var authorization = {
      manifestId: composition.manifestId,
      manifestSha256: composition.manifestSha256,
      owners: Object.keys(resolved).map(function(id) { return resolved[id]; })
        .sort(function(left, right) {
          return left.order - right.order || left.id.localeCompare(right.id);
        }),
    };
    deepFreeze(authorization);
    authorizedCompositions.add(authorization);
    return authorization;
  }

  function ownerSignature(owner) {
    return {
      id: owner.id,
      category: owner.category,
      regions: normalizedRegionKeys(owner.regions),
    };
  }

  function planSignature(plan) {
    return sha256(utf8Bytes(JSON.stringify({
      mode: plan.mode,
      sourceSha256: plan.sourceSha256,
      baseSha256: sha256(plan.baseZ64),
      acceptedCommit: plan.acceptedCommit,
      acceptedTree: plan.acceptedTree,
      contractSha256: plan.contractSha256,
      manifestSha256: plan.manifestSha256,
      generatorProvenance: plan.generatorProvenance,
      inputIdentityCount: plan.inputIdentityCount,
      runtimeRdramBytes: plan.runtimeRdramBytes,
      selected: plan.selected,
      experiencePercent: plan.experiencePercent,
      featureMask: plan.featureMask,
      acceptedMatrixId: plan.acceptedMatrixId,
      shopSelected: plan.shopSelected,
      shopContractSha256: plan.shopRecords
        ? sha256(encodeShopContract(plan.shopRecords)) : null,
      consumables: plan.consumables,
      writes: plan.writes.map(function(write) {
        return {
          owner: write.owner,
          name: write.name,
          offset: write.offset,
          bytesSha256: sha256(write.bytes),
          retailSha256: sha256(write.retailBytes),
        };
      }),
      owners: plan.owners.map(ownerSignature),
      restorationOwners: plan.restorationOwners.map(ownerSignature),
    })));
  }

  function assertPlanComposition(plan) {
    var data = verifyData();
    var expected = plan.owners.concat(plan.restorationOwners)
      .map(function(owner) { return owner.id; }).sort();
    var authorized = plan.compositionAuthorization.owners
      .filter(function(owner) {
        return data.composition.migratedOwners.indexOf(owner.id) !== -1;
      })
      .map(function(owner) { return owner.id; }).sort();
    if (expected.join('|') !== authorized.join('|')) {
      fail('the composition authorization does not cover the exact migrated-owner plan');
    }
  }

  function shopWriteComponents(records) {
    var data = verifyData();
    var contractBytes = encodeShopContract(records);
    return data.shops.components.map(function(component) {
      var bytes = componentBytes(component, 'native').slice();
      if (component === shopModuleComponent()) {
        bytes.set(contractBytes, data.shops.module.contractOffset);
      }
      return { component: component, bytes: bytes };
    });
  }

  function assertSourceForRebuild(rom, classification) {
    if (!rom.layout || rom.layout.id !== 'us-rev0') {
      fail('US Rev 1 has no accepted rebuilt placement. Select Patch ROM to export US Rev 1');
    }
    var blocked = [];
    Object.keys(classification.owners).forEach(function(key) {
      var state = classification.owners[key].state;
      if (key === 'consumables' && state === 'legacy') return;
      if (state === 'legacy' || state === 'foreign' || state === 'mixed') {
        blocked.push(key + '=' + state);
      }
    });
    if (blocked.length) {
      fail('the loaded source contains partial, mixed, or unknown migrated ownership (' +
        blocked.join(', ') + '). Load exact US Rev 0 or a complete recognized product');
    }
  }

  function prepareExport(rom, options) {
    options = options || {};
    var data = verifyData();
    if (!rom || !rom.nativeRebuild || !(rom.nativeRebuild.sourceZ64 instanceof Uint8Array)) {
      fail('the immutable loaded-source snapshot is unavailable');
    }
    var source = rom.nativeRebuild.sourceZ64;
    if (sha256(source) !== rom.nativeRebuild.sourceSha256) {
      fail('the immutable loaded-source snapshot failed its recorded SHA-256 identity');
    }
    var classification = classifySource(source, rom);
    assertSourceForRebuild(rom, classification);

    var base = source.slice();
    base.set(hexBytes(data.experience.retailHex), data.experience.romStart);
    data.phase3.components.forEach(function(component) { restoreComponent(base, component); });
    data.shops.components.forEach(function(component) { restoreComponent(base, component); });
    base.set(hexBytes(data.consumables.effect.retailHex),
      data.consumables.effect.romStart);
    base.set(hexBytes(data.consumables.descriptions.retailHex),
      data.consumables.descriptions.romStart);

    var experienceFeature = OB64.tools.getFeature('experience-size-scale');
    var experiencePercent = OB64.tools.desiredPercent(rom, experienceFeature);
    if (!Number.isInteger(experiencePercent) || experiencePercent < 0 || experiencePercent > 300) {
      fail('Experience Scale lacks authenticated source coverage for ' + experiencePercent);
    }
    var selected = {
      'experience-size-scale': experiencePercent !== 100,
      'cf-army-counter': !!rom.tools.desired['cf-army-counter'],
      'character-card-luck': !!rom.tools.desired['character-card-luck'],
      'squad-menu-alignment': !!rom.tools.desired['squad-menu-alignment'],
    };
    var shopSelected = shopOwnerSelected(rom) || !!options.shopsDirty;
    var shopRecords = shopSelected ? recordsFromRom(rom) : null;
    var writes = [];
    var owners = [];
    var restorationOwners = [];

    if (selected['experience-size-scale']) {
      writes.push({
        owner: data.experience.owner,
        name: 'accepted source-owned Experience Scale function',
        offset: data.experience.romStart,
        bytes: experienceBytes(experiencePercent),
        retailBytes: hexBytes(data.experience.retailHex),
      });
    }
    if (selected['experience-size-scale']) {
      owners.push(ownerRecord(data.experience.owner, 'Native Experience Scale', [{
        romStart: data.experience.romStart,
        ramStart: data.experience.ramStart,
        bytes: data.experience.bytes,
        name: 'accepted source-owned Experience Scale function',
      }]));
    } else if (classification.owners['experience-size-scale'].state === 'native') {
      restorationOwners.push(ownerRecord(data.experience.owner,
        'Retail Experience Scale restoration', [{
          romStart: data.experience.romStart,
          ramStart: data.experience.ramStart,
          bytes: data.experience.bytes,
          name: 'accepted source-owned Experience Scale function',
        }], 'nativeRebuildRestoration'));
    }

    PHASE3_TOOL_IDS.forEach(function(toolId) {
      var components = phase3Components(toolId);
      if (selected[toolId]) {
        components.forEach(function(component) {
          writes.push({
            owner: component.owner,
            name: component.name,
            offset: component.romStart,
            bytes: componentBytes(component, 'native').slice(),
            retailBytes: componentBytes(component, 'retail'),
          });
        });
      }
      if (selected[toolId]) {
        owners.push(ownerRecord(components[0].owner,
          'Native ' + OB64.tools.getFeature(toolId).name, components));
      } else if (classification.owners[toolId].state === 'native') {
        restorationOwners.push(ownerRecord(components[0].owner,
          'Retail ' + OB64.tools.getFeature(toolId).name + ' restoration',
          components, 'nativeRebuildRestoration'));
      }
    });

    if (shopSelected) {
      var shopWrites = shopWriteComponents(shopRecords);
      shopWrites.forEach(function(write) {
        writes.push({
          owner: data.shops.owner,
          name: write.component.name,
          offset: write.component.romStart,
          bytes: write.bytes,
          retailBytes: componentBytes(write.component, 'retail'),
        });
      });
    }
    if (shopSelected) {
      owners.push(ownerRecord(data.shops.owner, 'Native Shop memberships', data.shops.components));
    } else if (classification.owners.shops.state === 'native') {
      restorationOwners.push(ownerRecord(data.shops.owner,
        'Retail Shop ownership restoration', data.shops.components,
        'nativeRebuildRestoration'));
    }

    if (!rom.consumableEffects || !rom.consumableEffects.models) {
      fail('the shared in-memory consumable effect model is unavailable');
    }
    var effectSection = buildConsumableEffectSection(rom.consumableEffects.models);
    var descriptionSlot = buildConsumableDescriptionSlot(
      rom, base, effectSection.models);
    writes.push({
      owner: data.consumables.effect.owner,
      name: 'complete accepted source-owned consumable effect function',
      offset: data.consumables.effect.romStart,
      bytes: effectSection.bytes,
      retailBytes: hexBytes(data.consumables.effect.retailHex),
    });
    writes.push({
      owner: data.consumables.descriptions.owner,
      name: 'unified manual and healing consumable description slot',
      offset: data.consumables.descriptions.romStart,
      bytes: descriptionSlot.bytes,
      retailBytes: hexBytes(data.consumables.descriptions.retailHex),
    });
    owners.push(ownerRecord(data.consumables.effect.owner,
      'Native consumable effect function', [{
        romStart: data.consumables.effect.romStart,
        ramStart: data.consumables.effect.ramStart,
        bytes: data.consumables.effect.bytes,
        name: 'accepted source-owned consumable effect function',
      }]));
    owners.push(ownerRecord(data.consumables.descriptions.owner,
      'Unified consumable descriptions', [{
        romStart: data.consumables.descriptions.romStart,
        bytes: data.consumables.descriptions.bytes,
        name: 'unified consumable description slot',
      }]));

    var featureMask = (selected['character-card-luck'] ? 1 : 0) |
      (selected['squad-menu-alignment'] ? 2 : 0) |
      (selected['cf-army-counter'] ? 4 : 0);
    var plan = {
      schema: 'ob64-native-rebuild-plan',
      schemaVersion: 1,
      mode: REBUILD_MODE,
      modeLabel: MODES.rebuild.label,
      sourceZ64: source,
      sourceSha256: rom.nativeRebuild.sourceSha256,
      baseZ64: base,
      sourceClassification: classification,
      selected: selected,
      experiencePercent: experiencePercent,
      featureMask: featureMask,
      acceptedMatrixId: 'migrated-' + featureMask.toString(2).padStart(3, '0'),
      shopSelected: shopSelected,
      shopRecords: shopRecords,
      consumables: {
        effectSha256: effectSection.sha256,
        models: effectSection.models,
        descriptionSha256: sha256(descriptionSlot.bytes),
        descriptionMode: descriptionSlot.mode,
        descriptionEncodingPasses: descriptionSlot.encodingPasses,
        descriptionCompressedBytes: descriptionSlot.compressedBytes,
        descriptionCapacity: descriptionSlot.capacity,
        manualDescriptionIds: descriptionSlot.manualIds.slice(),
        healingValues: descriptionSlot.healingValues.slice(),
        logicalOrder: descriptionSlot.logicalOrder.slice(),
        conversion: classification.owners.consumables.state === 'legacy'
          ? 'legacy-to-native' : 'none',
        sourceOwnerState: classification.owners.consumables.ownerState,
      },
      writes: writes,
      owners: owners,
      restorationOwners: restorationOwners,
      runtimeRdramBytes: data.acceptedSource.runtimeRdramBytes,
      acceptedCommit: data.acceptedSource.commit,
      acceptedTree: data.acceptedSource.tree,
      contractSha256: data.shops.contractSha256,
      manifestSha256: data.composition.manifestSha256,
      generatorProvenance: {
        bundle: Object.assign({}, data.generator),
        experience: data.experience.generator,
        phase3: data.phase3.generator,
        shops: data.shops.generator,
        consumables: data.consumables.generator,
      },
      inputIdentityCount: data.acceptedSource.inputs.length,
      used: false,
    };
    issuedPlanSignatures.set(plan, planSignature(plan));
    return plan;
  }

  function detachRuntimePlan(runtimePlan) {
    if (!runtimePlan) return runtimePlan;
    var legacy = verifyData().shops.legacy;
    if (runtimePlan.shopOverrideCount && runtimePlan.shopOverrideCount !== 0) {
      fail('the shared runtime plan still contains the legacy Shop contributor');
    }
    if (runtimePlan.blob && runtimePlan.blob.length > legacy.resolverOffset) {
      fail('the shared runtime plan reaches the retired legacy Shop lane');
    }
    runtimePlan.writes = (runtimePlan.writes || []).filter(function(write) {
      return Number(write.offset) !== legacy.hookRom;
    });
    var bootstrap = runtimePlan.writes.filter(function(write) {
      return Number(write.offset) === legacy.sharedBootstrapRom;
    })[0];
    if (bootstrap) {
      var tableWordOffset = (15 + 1) * 4;
      if (bootstrap.bytes.length >= tableWordOffset + 4) {
        var bytes = bootstrap.bytes.slice();
        var current = readU32(bytes, tableWordOffset);
        var legacyTarget = (legacy.sharedTailRam + legacy.resolverOffset) >>> 0;
        if (current !== 0 && current !== legacyTarget) {
          fail('the shared runtime bootstrap contains an unknown Shop dispatch target');
        }
        writeU32(bytes, tableWordOffset, 0);
        bootstrap.bytes = bytes;
        bootstrap.label = 'shared bootstrap with legacy Shop dispatch retired';
      }
    }
    runtimePlan.nativeShopDetached = true;
    return runtimePlan;
  }

  function restoreRetiredShopLane(candidateRom) {
    var legacy = verifyData().shops.legacy;
    var start = legacy.sharedTailRom + legacy.resolverOffset;
    candidateRom.z64.set(hexBytes(legacy.retailLaneHex), start);
  }

  function applyPlan(plan, candidateRom, runtimePlan) {
    if (!plan || plan.schema !== 'ob64-native-rebuild-plan' || plan.mode !== REBUILD_MODE) {
      fail('the native composition plan is missing or malformed');
    }
    if (plan.used) fail('a stale Rebuild ROM plan cannot be reused');
    if (!issuedPlanSignatures.has(plan) ||
        issuedPlanSignatures.get(plan) !== planSignature(plan)) {
      fail('the Rebuild ROM plan changed after authenticated preflight');
    }
    if (!plan.compositionAuthorization ||
        !authorizedCompositions.has(plan.compositionAuthorization)) {
      fail('the accepted Phase 2 composition authorization is missing');
    }
    assertPlanComposition(plan);
    if (!candidateRom || !(candidateRom.z64 instanceof Uint8Array)) {
      fail('the detached ROM candidate is unavailable');
    }
    if (plan.shopSelected) {
      if (runtimePlan && !runtimePlan.nativeShopDetached && runtimePlan.writes) {
        fail('the legacy Shop contributor was not detached from the shared runtime plan');
      }
      restoreRetiredShopLane(candidateRom);
    }
    var ownerOrder = Object.create(null);
    plan.compositionAuthorization.owners.forEach(function(owner, index) {
      ownerOrder[owner.id] = index;
    });
    var orderedWrites = plan.writes.map(function(write, index) {
      return { write: write, originalIndex: index };
    }).sort(function(left, right) {
      var leftOrder = ownerOrder[left.write.owner];
      var rightOrder = ownerOrder[right.write.owner];
      return leftOrder - rightOrder || left.originalIndex - right.originalIndex;
    }).map(function(entry) { return entry.write; });
    orderedWrites.forEach(function(write) {
      if (!equalAt(candidateRom.z64, write.offset, write.retailBytes)) {
        fail(write.owner + ' / ' + write.name +
          ' no longer has its authenticated retail preimage; refusing last-write-wins composition');
      }
      candidateRom.z64.set(write.bytes, write.offset);
    });
    plan.used = true;
    plan.appliedWrites = orderedWrites;
    var consumableProvenanceRecord = pendingConsumableProvenance(candidateRom.z64, plan);
    var crc = orderedWrites.some(function(write) {
      return write.offset < 0x101000 && write.offset + write.bytes.length > 0x1000;
    });
    return {
      mode: REBUILD_MODE,
      modeLabel: MODES.rebuild.label,
      writes: orderedWrites,
      owners: plan.owners,
      restorationOwners: plan.restorationOwners,
      crc: crc,
      featureMask: plan.featureMask,
      acceptedMatrixId: plan.acceptedMatrixId,
      experiencePercent: plan.experiencePercent,
      shopSelected: plan.shopSelected,
      shopRecords: plan.shopRecords,
      consumables: plan.consumables,
      consumableProvenance: consumableProvenanceRecord,
    };
  }

  function recordsEqual(left, right) {
    if (!left || !right || left.length !== right.length) return false;
    for (var index = 0; index < left.length; index++) {
      if (left[index].index !== right[index].index ||
          !equalBytes(left[index].equipment, right[index].equipment) ||
          !equalBytes(left[index].expendables, right[index].expendables)) return false;
    }
    return true;
  }

  function validateApplied(candidateRom, plan, runtimePlan) {
    if (!plan || !plan.used) fail('the native plan was not applied before validation');
    if (!issuedPlanSignatures.has(plan) ||
        issuedPlanSignatures.get(plan) !== planSignature(plan)) {
      fail('the Rebuild ROM plan changed after authenticated application');
    }
    var data = verifyData();
    if (!plan.compositionAuthorization ||
        !authorizedCompositions.has(plan.compositionAuthorization) ||
        plan.compositionAuthorization.manifestSha256 !== data.composition.manifestSha256) {
      fail('the accepted Phase 2 composition authorization is missing');
    }
    assertPlanComposition(plan);
    plan.writes.forEach(function(write) {
      if (!equalAt(candidateRom.z64, write.offset, write.bytes)) {
        fail(write.owner + ' / ' + write.name + ' failed exact readback');
      }
    });
    var rebuilt = classifySource(candidateRom.z64, candidateRom);
    Object.keys(plan.selected).forEach(function(toolId) {
      var expected = plan.selected[toolId] ? 'native' : 'retail';
      if (rebuilt.owners[toolId].state !== expected) {
        fail(toolId + ' read back as ' + rebuilt.owners[toolId].state +
          ' instead of ' + expected);
      }
    });
    if (plan.shopSelected) {
      if (rebuilt.owners.shops.state !== 'native' ||
          !recordsEqual(rebuilt.owners.shops.records, plan.shopRecords)) {
        fail('native Shop memberships did not round-trip through the accepted contract decoder');
      }
      var legacy = data.shops.legacy;
      if (!equalAt(candidateRom.z64,
          legacy.sharedTailRom + legacy.resolverOffset,
          hexBytes(legacy.retailLaneHex))) {
        fail('the retired legacy Shop resolver and table lane is not retail-clean');
      }
      var findings = legacyShopFindings(candidateRom.z64, candidateRom);
      if (findings.length) fail('mixed native-and-legacy Shop ownership remains: ' + findings.join(', '));
      if (runtimePlan) {
        var bootstrapWrite = (runtimePlan.writes || []).filter(function(write) {
          return write.offset === legacy.sharedBootstrapRom;
        })[0];
        if (bootstrapWrite && readU32(candidateRom.z64,
            legacy.sharedBootstrapRom + (15 + 1) * 4) !== 0) {
          fail('the shared runtime bootstrap still dispatches the legacy Shop resolver');
        }
      }
    } else if (rebuilt.owners.shops.state !== 'retail') {
      fail('the unselected native Shop owner was not restored to retail');
    }
    var consumableNoOp = plan.consumables.effectSha256 ===
      data.consumables.effect.retailSha256 && plan.consumables.descriptionSha256 ===
        data.consumables.descriptions.retailSha256;
    var consumableState = rebuilt.owners.consumables.state;
    var acceptableConsumableState = consumableNoOp
      ? consumableState === 'retail'
      : (consumableState === 'legacy' || consumableState === 'native');
    if (!acceptableConsumableState ||
        !sameModels(rebuilt.owners.consumables.models, plan.consumables.models)) {
      fail('native consumable effects and descriptions did not round-trip through trusted provenance');
    }
    var consumableProvenanceRecord = null;
    var validatedConsumableOwnerState = rebuilt.owners.consumables.ownerState;
    if (!consumableNoOp) {
      consumableProvenanceRecord = registerValidatedConsumableProvenance(
        candidateRom.z64, plan);
      validatedConsumableOwnerState = 'native-complete';
    }
    return {
      summary: MODES.rebuild.label + ' authenticated all selected Phase 1 through Phase 5A owners.',
      details: {
        buildMode: REBUILD_MODE,
        buildModeLabel: MODES.rebuild.label,
        acceptedSourceCommit: plan.acceptedCommit,
        acceptedSourceTree: plan.acceptedTree,
        ownershipManifestSha256: plan.manifestSha256,
        shopContractSha256: plan.contractSha256,
        generatorProvenance: plan.generatorProvenance,
        inputIdentityCount: plan.inputIdentityCount,
        runtimeRdramBytes: plan.runtimeRdramBytes,
        migratedFeatureMask: plan.featureMask,
        migratedCombination: plan.acceptedMatrixId,
        experienceScalePercent: plan.experiencePercent,
        nativeShops: plan.shopSelected,
        consumableOwnerState: validatedConsumableOwnerState,
        consumableConversion: plan.consumables.conversion,
        consumableDescriptionMode: plan.consumables.descriptionMode,
        consumableDescriptionEncodingPasses:
          plan.consumables.descriptionEncodingPasses,
        consumableProvenance: consumableProvenanceRecord,
        ownerCount: plan.owners.length,
        writeCount: plan.writes.length,
        authorizedOwners: plan.compositionAuthorization.owners.map(function(owner) {
          return owner.id;
        }),
      },
    };
  }

  function patchSourceCheck(rom) {
    if (!rom || !(rom.z64 instanceof Uint8Array)) throw new Error('Patch ROM source is unavailable.');
    var classification = classifySource(rom.z64, rom);
    var hasPartialNative = classification.owners &&
      Object.keys(classification.owners).some(function(key) {
        var owner = classification.owners[key];
        if (key === 'consumables' && owner.patchCompatible &&
            !owner.provenance.trusted && !owner.provenance.mismatch) return false;
        return owner.state === 'mixed' ||
          (key === 'consumables' && owner.state === 'foreign');
      });
    if (classification.hasExclusiveNative || hasPartialNative) {
      throw new Error('Patch ROM cannot export from a Rebuild ROM or mixed native source. ' +
        'Reload the original retail ROM, then load the Project in Patch ROM mode.');
    }
    return classification;
  }

  function passthroughToolIds() {
    return OB64.tools.features().map(function(feature) { return feature.id; })
      .filter(function(id) { return MIGRATED_TOOL_IDS.indexOf(id) === -1; });
  }

  function passthroughToolOwners(rom) {
    if (!rom || !rom.tools) return [];
    return OB64.tools.features().filter(function(feature) {
      if (MIGRATED_TOOL_IDS.indexOf(feature.id) !== -1) return false;
      if (rom.tools.initial[feature.id] === 'foreign' ||
          rom.tools.initial[feature.id] === 'unsupported') return false;
      var current = OB64.tools.featureState(rom.z64, feature);
      var desired = OB64.tools.isParameterized(feature)
        ? OB64.tools.desiredPercent(rom, feature) !== feature.parameter.default
        : !!rom.tools.desired[feature.id];
      return current === 'outdated' || desired !== (current === 'applied');
    }).map(function(feature) {
      return {
        id: 'tool-' + feature.id,
        name: feature.name,
        category: 'tools',
        regions: OB64.tools.featureRegions(feature),
      };
    });
  }

  function remainingRuntimeRegions(rom, sourceRegions) {
    if (!OB64.runtimeOverrides) return [];
    var legacy = verifyData().shops.legacy;
    var regions = sourceRegions || OB64.runtimeOverrides.patchRegions(rom);
    return regions.filter(function(region) {
      return !(region.kind === 'rom' && region.start === legacy.hookRom && region.size === 24);
    }).map(function(region) {
      var isRetiredRomLane = region.kind === 'rom' &&
        region.start === legacy.sharedTailRom && region.size > legacy.resolverOffset;
      var isRetiredRamLane = region.kind === 'ram' &&
        region.start === legacy.sharedTailRam && region.size > legacy.resolverOffset;
      if (!isRetiredRomLane && !isRetiredRamLane) return region;
      return Object.assign({}, region, {
        size: legacy.resolverOffset,
        label: region.label + ' (native Shop lane excluded)',
      });
    });
  }

  function capacities(mode) {
    var info = modeInfo(mode);
    return { equipment: info.shopEquipmentCapacity, expendables: info.shopExpendableCapacity };
  }

  function markExport(rom, details) {
    if (!rom || !rom.nativeRebuild) return;
    rom.nativeRebuild.lastExport = Object.assign({
      recordedAt: new Date().toISOString(),
      mode: getMode(rom),
      modeLabel: modeInfo(getMode(rom)).label,
    }, details || {});
  }

  OB64.buildModes = {
    PATCH: PATCH_MODE,
    REBUILD: REBUILD_MODE,
    modes: MODES,
    normalize: normalizeMode,
    info: modeInfo,
    get: getMode,
    set: setMode,
  };

  OB64.nativeRebuild = {
    verifyData: verifyData,
    initializeSource: initializeSource,
    initializeToolState: initializeToolState,
    classifySource: classifySource,
    displayToolState: displayToolState,
    displayToolPercent: displayToolPercent,
    prepareExport: prepareExport,
    applyPlan: applyPlan,
    validateApplied: validateApplied,
    detachRuntimePlan: detachRuntimePlan,
    patchSourceCheck: patchSourceCheck,
    encodeShopContract: encodeShopContract,
    decodeShopContract: decodeShopContract,
    recordsFromRom: recordsFromRom,
    shopOwnerSelected: shopOwnerSelected,
    setShopOwnerSelected: setShopOwnerSelected,
    shopDataChanged: shopsChanged,
    capacities: capacities,
    passthroughToolIds: passthroughToolIds,
    passthroughToolOwners: passthroughToolOwners,
    remainingRuntimeRegions: remainingRuntimeRegions,
    authorizeComposition: authorizeComposition,
    markExport: markExport,
    migratedToolIds: MIGRATED_TOOL_IDS.slice(),
    _test: {
      experienceBytes: experienceBytes,
      detectExperience: detectExperience,
      detectPhase3Owner: detectPhase3Owner,
      detectShops: detectShops,
      legacyShopFindings: legacyShopFindings,
      shopWriteComponents: shopWriteComponents,
      equalBytes: equalBytes,
      buildConsumableEffectSection: buildConsumableEffectSection,
      buildConsumableDescriptionSlot: buildConsumableDescriptionSlot,
      classifyConsumableOwner: classifyConsumableOwner,
      consumableOwnerImageKey: consumableOwnerImageKey,
    },
  };
})(window.OB64, typeof window !== 'undefined' ? window : globalThis);
