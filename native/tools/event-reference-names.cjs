'use strict';
// Match the reviewed parent reconciler's identity rules. A scene resource is
// global even when its launch is contextual; dialogue words and local code are
// scoped identities. Never equate a scenario squad with a character/template ID.
function createNameResolver(entries, eventScenarioBindings = []) {
  const aliases = {cutscene:'scene', director:'scene', 'director-selector':'scene',
    history:'scene-history', 'dated-history':'scene-history', 'history-entry':'scene-history',
    'mode-request':'transition-request', 'campaign-value':'property', 'event-program':'event-row'};
  const kindOf = kind => aliases[kind] || kind;
  const key = (kind, id, scope = 'global') => `${kindOf(kind)}:${id}:${scope}`;
  const names = new Map(entries.map(e => [key(e.kind,e.id,e.scope),e]));
  return ref => {
    const kind = kindOf(ref.kind);
    if (kind === 'local-predicate') return names.get(key('local-code',ref.entry,`row:${ref.row}`));
    const selector = ref.selector ?? ref.cutsceneSelector;
    if (['dialogue','dialogue-choice'].includes(kind) && Number.isInteger(selector) && Number.isInteger(ref.wordOffset))
      return names.get(key(kind,ref.wordOffset,`selector:${selector}`));
    if (kind === 'opposing-squad') {
      const match = /^row:(\d+)$/.exec(ref.scope || '');
      const scenarios = match ? eventScenarioBindings.find(x=>x.row===Number(match[1]))?.scenarioKeys : [];
      if (scenarios?.length === 1) return names.get(key('scenario-squad',ref.id,`scenario:${scenarios[0]}`));
    }
    return names.get(key(kind,ref.id,ref.scope || 'global')) ||
      (['scene','scripted-death-subject'].includes(kind) ? names.get(key(kind,ref.id)) : undefined);
  };
}
module.exports = {createNameResolver};
