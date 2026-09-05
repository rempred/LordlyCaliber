// Detached ROM candidate lifecycle for validated exports.

window.OB64 = window.OB64 || {};

(function() {
  'use strict';

  var OB64 = window.OB64;

  function assertRom(value, label) {
    if (!value || !value.z64 || typeof value.z64.slice !== 'function') {
      throw new TypeError((label || 'ROM') + ' must provide a normalized z64 byte buffer.');
    }
  }

  function cloneScenarioState(state) {
    if (!state) return state;
    var cloned = Object.assign({}, state);
    cloned.archiveOriginalSlots = Object.assign({}, state.archiveOriginalSlots || {});
    cloned.slotOwnedArchives = Object.assign({}, state.slotOwnedArchives || {});
    cloned.relocationOwnedWindows = (state.relocationOwnedWindows || []).map(function(window) {
      return Object.assign({}, window);
    });
    return cloned;
  }

  function create(sourceRom) {
    assertRom(sourceRom, 'Source ROM');
    var candidate = Object.assign({}, sourceRom);
    candidate.z64 = sourceRom.z64.slice();
    if (sourceRom.scenarioEditor) {
      candidate.scenarioEditor = cloneScenarioState(sourceRom.scenarioEditor);
    }
    if (sourceRom.scenarioRelocations) {
      candidate.scenarioRelocations = sourceRom.scenarioRelocations.map(function(relocation) {
        return Object.assign({}, relocation);
      });
    }
    return candidate;
  }

  function adopt(targetRom, candidateRom, options) {
    assertRom(targetRom, 'Target ROM');
    assertRom(candidateRom, 'Candidate ROM');
    if (targetRom === candidateRom) {
      throw new TypeError('Candidate ROM must be detached from the target ROM.');
    }

    options = options || {};
    if (!options.romBytesAlreadyAdopted) {
      targetRom.z64 = candidateRom.z64;
    }
    if (candidateRom.scenarioEditor && targetRom.scenarioEditor) {
      targetRom.scenarioEditor.archiveOriginalSlots =
        candidateRom.scenarioEditor.archiveOriginalSlots;
      targetRom.scenarioEditor.slotOwnedArchives =
        candidateRom.scenarioEditor.slotOwnedArchives;
      targetRom.scenarioEditor.relocationOwnedWindows =
        candidateRom.scenarioEditor.relocationOwnedWindows;
    }
    if (candidateRom.scenarioRelocations) {
      targetRom.scenarioRelocations = candidateRom.scenarioRelocations;
    }
    if (candidateRom.statGatePlan && OB64.statGateRelocation) {
      var adoptedStatGates = OB64.statGateRelocation.parse(
        candidateRom.z64,
        targetRom.layout
      );
      targetRom.statGates.raw = adoptedStatGates.raw;
      targetRom.statGates.meta = adoptedStatGates.meta;
      Object.keys(adoptedStatGates.byClass).forEach(function(classId) {
        if (targetRom.statGates.byClass[classId]) {
          Object.assign(
            targetRom.statGates.byClass[classId],
            adoptedStatGates.byClass[classId]
          );
        } else {
          targetRom.statGates.byClass[classId] =
            adoptedStatGates.byClass[classId];
        }
      });
    }
    return targetRom;
  }

  OB64.romExportCandidate = {
    create: create,
    adopt: adopt,
  };
})();
