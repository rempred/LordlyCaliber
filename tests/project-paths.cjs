'use strict';

// Locates the Editor checkout under test and the parent research repository
// that holds the master ROM, ob64_all/, scripts/, and Rev 1.1/. The Editor can
// run from <parent>/editor or from a separate worktree. OB64_PARENT_ROOT
// overrides the parent search.
const fs = require('fs');
const path = require('path');

const EDITOR = path.resolve(__dirname, '..');
const MASTER_NAME = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';

function findParent() {
  if (process.env.OB64_PARENT_ROOT) return path.resolve(process.env.OB64_PARENT_ROOT);
  const candidates = [
    path.resolve(EDITOR, '..'),
    path.resolve(EDITOR, '..', '..', 'OgreBattlel64'),
  ];
  return candidates.find(candidate => fs.existsSync(path.join(candidate, MASTER_NAME))) ||
    candidates[0];
}

module.exports = { EDITOR, PARENT: findParent(), MASTER_NAME };
