'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

function createDocument() {
  const scripts = [];
  const head = {
    appendChild(script) {
      script.parentNode = head;
      scripts.push(script);
      return script;
    }
  };
  return {
    scripts,
    head,
    createElement(tag) {
      assert.strictEqual(tag, 'script');
      return {
        async: false,
        dataset: {},
        parentNode: null,
        remove() {
          this.parentNode = null;
          this.removed = true;
        }
      };
    }
  };
}

function loadModule(document) {
  const context = {
    document,
    Error,
    Object,
    Promise,
    String,
    window: { OB64: {} }
  };
  vm.createContext(context);
  vm.runInContext(fs.readFileSync(path.join(EDITOR, 'dataset-loader.js'), 'utf8'),
    context, { filename: 'dataset-loader.js' });
  return context.window.OB64;
}

(async function run() {
  const document = createDocument();
  const OB64 = loadModule(document);
  const first = OB64.datasetLoader.load('animation-corpus');
  const concurrent = OB64.datasetLoader.load('animation-corpus');

  assert.strictEqual(first, concurrent,
    'concurrent requests must share one dataset promise');
  assert.strictEqual(document.scripts.length, 1,
    'concurrent requests must append one script');
  assert.strictEqual(document.scripts[0].src, 'animation-corpus-data.js');
  assert.strictEqual(document.scripts[0].async, true);
  assert.strictEqual(document.scripts[0].dataset.ob64Dataset, 'animation-corpus');

  const corpus = { schemaVersion: 'test' };
  OB64.animationCorpusData = corpus;
  document.scripts[0].onload();
  assert.strictEqual(await first, corpus);
  assert.strictEqual(await concurrent, corpus);
  assert.strictEqual(OB64.datasetLoader.isLoaded('animation-corpus'), true);
  assert.strictEqual(await OB64.datasetLoader.load('animation-corpus'), corpus,
    'repeat use must return the existing dataset');
  assert.strictEqual(document.scripts.length, 1,
    'repeat use must not append another script');

  const failed = OB64.datasetLoader.load('cutscene-data');
  assert.strictEqual(document.scripts.length, 2);
  document.scripts[1].onerror();
  await assert.rejects(failed, error =>
    error.name === 'DatasetLoadError' &&
    /cutscene-data\.js/.test(error.message));
  assert.strictEqual(document.scripts[1].removed, true,
    'a failed script must be removable before retry');

  const retry = OB64.datasetLoader.load('cutscene-data');
  assert.strictEqual(document.scripts.length, 3,
    'a failed dataset must permit one fresh retry');
  const catalog = { schemaVersion: 'test-catalog' };
  OB64.cutsceneData = catalog;
  document.scripts[2].onload();
  assert.strictEqual(await retry, catalog);

  const missingDocument = loadModule(null);
  await assert.rejects(
    missingDocument.datasetLoader.load('animation-corpus'),
    error => error.name === 'DatasetLoadError' && /script loading is unavailable/.test(error.message)
  );
  await assert.rejects(
    OB64.datasetLoader.load('not-a-dataset'),
    error => error.name === 'DatasetLoadError' && /Unknown generated dataset/.test(error.message)
  );

  const index = fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8');
  assert(index.includes('<script src="dataset-loader.js"></script>'));
  assert(!index.includes('<script src="animation-corpus-data.js"></script>'));
  assert(!index.includes('<script src="cutscene-data.js"></script>'));
  assert(index.indexOf('dataset-loader.js') < index.indexOf('animation-art.js'),
    'the loader must exist before animation feature code');

  console.log('PASS dataset loader coalesces, retries, and preserves deferred globals');
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
