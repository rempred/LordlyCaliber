'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
let failures = 0;

function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

class FakeElement {
  constructor(tagName) {
    this.tagName = String(tagName || 'div').toUpperCase();
    this.nodeType = 1;
    this.children = [];
    this.parentNode = null;
    this.className = '';
    this.id = '';
    this.textContent = '';
    this.innerHTML = '';
    this.attributes = {};
    this.listeners = {};
    this.dataset = {};
    this.hidden = false;
    this.disabled = false;
    this.inert = false;
    this.clicked = false;
    this.style = { setProperty() {} };
    this.classList = { add() {}, remove() {}, toggle() {} };
  }

  addEventListener(type, listener) {
    (this.listeners[type] = this.listeners[type] || []).push(listener);
  }

  removeEventListener(type, listener) {
    this.listeners[type] = (this.listeners[type] || []).filter(found => found !== listener);
  }

  dispatch(type, event) {
    for (const listener of (this.listeners[type] || []).slice()) {
      listener(event || { target: this });
    }
  }

  click() {
    this.clicked = true;
    this.dispatch('click', { target: this });
  }

  appendChild(child) {
    child.parentNode = this;
    this.children.push(child);
    return child;
  }

  removeChild(child) {
    this.children = this.children.filter(found => found !== child);
    child.parentNode = null;
  }

  setAttribute(name, value) { this.attributes[name] = String(value); }
  getAttribute(name) { return this.attributes[name] || null; }
  hasAttribute(name) { return Object.prototype.hasOwnProperty.call(this.attributes, name); }
  removeAttribute(name) { delete this.attributes[name]; }
  querySelectorAll() { return []; }
  querySelector() { return null; }
  closest() { return null; }
  contains() { return false; }
  focus() { this.focused = true; }
  getBoundingClientRect() {
    return { height: 50, width: 100, left: 0, top: 0, bottom: 50 };
  }
}

function descendants(root) {
  const out = [];
  function visit(node) {
    out.push(node);
    for (const child of node.children || []) visit(child);
  }
  visit(root);
  return out;
}

function hasClass(element, className) {
  return String(element.className || '').split(/\s+/).includes(className);
}

function fullText(element) {
  return [element.textContent || ''].concat((element.children || []).map(fullText)).join(' ');
}

const elementsById = {};
const createdElements = [];
const documentListeners = {};
const body = new FakeElement('body');
const documentElement = new FakeElement('html');

global.window = global;
global.document = {
  body,
  documentElement,
  fonts: null,
  createElement(tagName) {
    const element = new FakeElement(tagName);
    createdElements.push(element);
    return element;
  },
  createElementNS(namespace, tagName) { return this.createElement(tagName); },
  getElementById(id) {
    if (!elementsById[id]) {
      elementsById[id] = new FakeElement('div');
      elementsById[id].id = id;
    }
    return elementsById[id];
  },
  querySelector() { return new FakeElement('div'); },
  querySelectorAll() { return []; },
  addEventListener(type, listener) {
    (documentListeners[type] = documentListeners[type] || []).push(listener);
  },
  removeEventListener(type, listener) {
    documentListeners[type] = (documentListeners[type] || [])
      .filter(found => found !== listener);
  },
};

global.MutationObserver = class { observe() {} };
global.ResizeObserver = class { observe() {} };
global.addEventListener = function() {};
global.innerWidth = 1200;
global.innerHeight = 800;
global.setTimeout = function(callback) { callback(); return 1; };
global.clearTimeout = function() {};

let createdBlob = null;
let revokedUrl = null;
global.Blob = class {
  constructor(parts, options) {
    this.parts = parts;
    this.type = options && options.type || '';
    createdBlob = this;
  }
};
global.URL = {
  createObjectURL(blob) {
    check('download uses the newly created report Blob', blob === createdBlob);
    return 'blob:validation-report';
  },
  revokeObjectURL(url) { revokedUrl = url; },
};

global.OB64 = {};
window.OB64 = global.OB64;
vm.runInThisContext(fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8'), {
  filename: path.join(EDITOR, 'app.js'),
});

const report = {
  schema: 'ob64-rom-export-validation-report',
  schemaVersion: 1,
  generatedAt: '2026-08-06T12:34:56.789Z',
  result: 'failed',
  ok: false,
  errors: [
    {
      code: 'ARCHIVE_BOUNDARY',
      title: 'Archive ends at the wrong place',
      message: 'A rebuilt scenario archive does not fill its assigned ROM slot exactly.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
      technical: { secretDiagnostic: 'raw offset 0x12345678' },
    },
    {
      code: 'ROM_CHECKSUM_INVALID',
      title: 'ROM checksum failed',
      message: 'The finished ROM has an invalid N64 boot checksum.',
      suggestion: 'Keep the error report and recreate the ROM after updating the editor.',
      technical: { computed: '0xDEADBEEF' },
    },
  ],
  checks: [],
  changeRanges: [],
};

let downloadAnywayCalls = 0;
OB64.showRomValidationModal(report, {
  onDownloadAnyway() { downloadAnywayCalls++; },
});
check('validation modal is appended to the document', body.children.length === 1);
const overlay = body.children[0];
const nodes = descendants(overlay);
const modal = nodes.find(element => hasClass(element, 'rom-validation-modal'));
check('popup reuses the existing error-modal CSS class',
  !!modal && hasClass(modal, 'error-modal'));
check('popup is an accessible modal dialog',
  modal && modal.attributes.role === 'dialog' && modal.attributes['aria-modal'] === 'true');

const cards = nodes.filter(element => hasClass(element, 'rom-validation-error'));
check('popup lists every validation error', cards.length === report.errors.length,
  String(cards.length));
const visibleText = fullText(overlay);
check('popup shows plain-language error messages',
  visibleText.includes(report.errors[0].message) &&
  visibleText.includes(report.errors[1].message));
check('popup does not expose machine-only technical details',
  !visibleText.includes('0x12345678') && !visibleText.includes('0xDEADBEEF'), visibleText);

const downloadButton = nodes.find(element =>
  element.textContent === 'Download Error Report');
check('popup includes the requested Download Error Report button', !!downloadButton);
downloadButton.click();
check('downloaded error report uses JSON MIME type',
  createdBlob && createdBlob.type === 'application/json', createdBlob && createdBlob.type);
const downloaded = JSON.parse(createdBlob.parts.join(''));
check('downloaded machine report retains stable error codes and technical evidence',
  downloaded.errors[0].code === 'ARCHIVE_BOUNDARY' &&
  downloaded.errors[0].technical.secretDiagnostic === 'raw offset 0x12345678');
const anchor = createdElements.filter(element => element.tagName === 'A').pop();
check('downloaded report has a timestamped JSON filename',
  anchor && /^ob64-rom-export-errors-20260806T123456Z\.json$/.test(anchor.download),
  anchor && anchor.download);
check('temporary report URL is revoked after the download starts',
  revokedUrl === 'blob:validation-report', String(revokedUrl));

const anywayButton = nodes.find(element => element.textContent === 'Download Anyway');
check('failed-candidate popup includes Download Anyway', !!anywayButton);
anywayButton.click();
check('Download Anyway invokes the supplied failed-candidate action',
  downloadAnywayCalls === 1, String(downloadAnywayCalls));
check('Download Anyway closes the validation popup', body.children.length === 0);

OB64.showRomValidationModal(report);
const noCandidateOverlay = body.children[0];
const noCandidateNodes = descendants(noCandidateOverlay);
check('unexpected errors do not offer a missing candidate download',
  !noCandidateNodes.some(element => element.textContent === 'Download Anyway'));
const closeButton = noCandidateNodes.find(element => hasClass(element, 'rom-validation-close'));
closeButton.click();
check('Close removes the validation popup', body.children.length === 0);

const progress = OB64.showRomExportProgressModal();
check('export progress popup is appended to the document', body.children.length === 1);
const progressOverlay = body.children[0];
const progressNodes = descendants(progressOverlay);
const progressModal = progressNodes.find(element => hasClass(element, 'rom-export-progress-modal'));
const stageField = progressNodes.find(element => hasClass(element, 'rom-export-progress-stage'));
const progressBar = progressNodes.find(element => hasClass(element, 'rom-export-progress-track'));
const progressFill = progressNodes.find(element => hasClass(element, 'rom-export-progress-fill'));
check('progress popup reuses the themed error-modal CSS',
  progressModal && hasClass(progressModal, 'error-modal'));
check('progress popup has a readonly active-stage text field',
  stageField && stageField.readOnly && stageField.value === 'Starting export');
check('progress popup exposes an accessible progress bar',
  progressBar && progressBar.attributes.role === 'progressbar' &&
    progressBar.attributes['aria-valuenow'] === '0');
progress.update('Testing: ROM archive boundaries', 73);
check('progress popup shows the active validation test',
  stageField.value === 'Testing: ROM archive boundaries', stageField.value);
check('progress popup updates its visible and accessible percentage',
  progressBar.attributes['aria-valuenow'] === '73' && progressFill.style.width === '73%',
  JSON.stringify({ aria: progressBar.attributes['aria-valuenow'], width: progressFill.style.width }));
progress.close();
check('progress popup closes after export', body.children.length === 0);

const indexSource = fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8');
const sourceRedirectIndex = indexSource.indexOf('source-redirect.js');
const statGateRelocationIndex = indexSource.indexOf('stat-gate-relocation.js');
const scenarioIndex = indexSource.indexOf('scenario.js');
const validatorIndex = indexSource.indexOf('rom-export-validator.js');
const appIndex = indexSource.indexOf('app.js');
check('shared redirect module loads before stat, Scenario, and validation',
  sourceRedirectIndex >= 0 &&
    sourceRedirectIndex < statGateRelocationIndex &&
    sourceRedirectIndex < scenarioIndex &&
    sourceRedirectIndex < validatorIndex);
check('stat-gate relocation module loads before validation and export',
  statGateRelocationIndex >= 0 &&
    statGateRelocationIndex < validatorIndex &&
    statGateRelocationIndex < appIndex);
check('validator module loads before the export controller',
  validatorIndex >= 0 && validatorIndex < appIndex);
const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8');
check('export controller uses cooperative validation progress',
  appSource.includes('validateAsync'));
check('export controller defers Scenario redirect writes for shared planning',
  /exportScenarioArchives\(candidateRom,\s*\{\s*deferRedirect:\s*true/.test(
    appSource
  ));
check('Tools compatibility uses the Consumable Effects ownership bridge',
  /consumableEffects\.toolCompatibilityOwners\(\s*effectOwners,\s*effectTransaction\s*\)/.test(
    appSource
  ));

const cssSource = fs.readFileSync(path.join(EDITOR, 'style.css'), 'utf8');
for (const selector of [
  '.rom-validation-modal',
  '.rom-validation-error-list',
  '.rom-validation-error',
  '.rom-validation-footer',
  '.rom-validation-download',
  '.rom-validation-anyway',
  '.rom-export-progress-modal',
  '.rom-export-progress-stage',
  '.rom-export-progress-track',
  '.rom-export-progress-fill',
]) {
  check('validation popup CSS defines ' + selector, cssSource.includes(selector));
}

if (failures) {
  console.error('\n' + failures + ' ROM validation UI test(s) failed.');
  process.exit(1);
}
console.log('\nAll ROM validation UI tests passed.');
