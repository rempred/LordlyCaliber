// LordlyCaliber - bounded loading for generated feature datasets.

window.OB64 = window.OB64 || {};

(function(OB64) {
  'use strict';

  var DATASETS = {
    'animation-corpus': {
      label: 'combat-animation corpus',
      source: 'animation-corpus-data.js',
      global: 'animationCorpusData',
      promise: null
    },
    'cutscene-data': {
      label: 'Cutscene Studio catalog',
      source: 'cutscene-data.js',
      global: 'cutsceneData',
      promise: null
    }
  };

  function DatasetLoadError(message) {
    this.name = 'DatasetLoadError';
    this.message = message;
  }
  DatasetLoadError.prototype = Object.create(Error.prototype);
  DatasetLoadError.prototype.constructor = DatasetLoadError;

  function valueFor(spec) {
    return Object.prototype.hasOwnProperty.call(OB64, spec.global) &&
      OB64[spec.global] != null
      ? OB64[spec.global]
      : null;
  }

  function load(id) {
    var spec = DATASETS[id];
    if (!spec) {
      return Promise.reject(new DatasetLoadError(
        'Unknown generated dataset "' + id + '".'
      ));
    }
    var loaded = valueFor(spec);
    if (loaded) return Promise.resolve(loaded);
    if (spec.promise) return spec.promise;
    if (!document || typeof document.createElement !== 'function') {
      return Promise.reject(new DatasetLoadError(
        'The ' + spec.label + ' cannot load because script loading is unavailable.'
      ));
    }

    var parent = document.head || document.documentElement || document.body;
    if (!parent || typeof parent.appendChild !== 'function') {
      return Promise.reject(new DatasetLoadError(
        'The ' + spec.label + ' cannot load because the document has no script container.'
      ));
    }

    var script = document.createElement('script');
    script.async = true;
    script.src = spec.source;
    if (script.dataset) script.dataset.ob64Dataset = id;

    var resolveLoad;
    var rejectLoad;
    var promise = new Promise(function(resolve, reject) {
      resolveLoad = resolve;
      rejectLoad = reject;
    });
    spec.promise = promise;

    function clearHandlers() {
      script.onload = null;
      script.onerror = null;
    }

    function fail(message) {
      clearHandlers();
      if (script.parentNode && typeof script.remove === 'function') script.remove();
      if (spec.promise === promise) spec.promise = null;
      rejectLoad(new DatasetLoadError(message));
    }

    script.onload = function() {
      var value = valueFor(spec);
      if (!value) {
        fail(spec.source + ' loaded without defining OB64.' + spec.global + '.');
        return;
      }
      clearHandlers();
      resolveLoad(value);
    };
    script.onerror = function() {
      fail('Could not load the ' + spec.label + ' from ' + spec.source + '.');
    };

    try {
      parent.appendChild(script);
    } catch (error) {
      fail('Could not load the ' + spec.label + ' from ' + spec.source + ': ' +
        (error && error.message ? error.message : String(error)));
    }
    return promise;
  }

  function isLoaded(id) {
    return !!(DATASETS[id] && valueFor(DATASETS[id]));
  }

  OB64.datasetLoader = {
    DatasetLoadError: DatasetLoadError,
    load: load,
    isLoaded: isLoaded
  };
})(window.OB64);
