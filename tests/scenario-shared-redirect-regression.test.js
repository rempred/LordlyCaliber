'use strict';

// The parent lifecycle suite is a protected task-external surface. Supply its
// new browser dependency, then run every original assertion unchanged.
const path = require('path');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');

require('./source-redirect-test-preload.cjs');
require(path.join(ROOT, 'scripts', 'ob64_scenario_export_lifecycle_test.js'));
