'use strict';
// Execute the exported normal-combat controller with the ROM's actor scripts,
// motion, effect programs and result callbacks all sharing one clock/RDRAM.
// Scores, roster, physical/effect resources and camera inputs are fixtures.
const assert = require('node:assert/strict');
const { AttackPlaybackMachine } = require('./attack-playback-machine.cjs');
const battle = new AttackPlaybackMachine(process.argv[2], { nativePoses: true });
const defaults = { identity: 5, current: 5, visual: 0, single: true, pattern: 1 };
const cases = [
 ['retail Slash', { route: 'melee', action: 4, retail: true }, 'melee'],
 ['linked enemy Phalanx identity with Knight current class',
  { route: 'melee', enemy: true, identity: 117 }, 'melee'],
 ['large creature movement', { route: 'melee', identity: 69, current: 69 }, 'melee'],
 ['miss and guard recovery', { route: 'melee', hit: false, targetIdentity: 5 }, 'miss'],
 ['lethal hit and removal', { route: 'melee', hp: 1 }, 'death'],
 ['shared column strike', { route: 'melee', pattern: 6, shared: 1 }, 'column'],
 ['ranged pose', { route: 'ranged', identity: 3, current: 3 }, 'ranged'],
 ['retail visual with three targets', { visual: 101, pattern: 7 }, 'spell'],
 ['two linked casters and three native impacts',
  { route: 'combined', identity: 12, current: 12, visual: 59, pattern: 7 }, 'combined'],
 ['petrification holds after damage and hurt', { visual: 101, status: 3, targetIdentity: 5 }, 'petrify'],
 ['sleep holds after damage and hurt', { visual: 101, status: 4, targetIdentity: 5 }, 'sleep'],
 ['paralysis holds after damage and hurt', { visual: 101, status: 8, targetIdentity: 5 }, 'paralysis'],
 ['blocked status spell', { visual: 101, effect: 4, hit: false, targetIdentity: 5 }, 'blocked'],
 ['healing an ally', { visual: 32, side: 2, effect: 2 }, 'healing'],
 ['native Life Drain artwork plus authored drain', { visual: 102, after: 1 }, 'drain'],
 ['Mesmerize cross with Sleep and a full-health caster',
  { action: 21, identity: 43, current: 43, targetIdentity: 5, visual: 101, pattern: 8,
   status: 4, after: 1, sourceHP: 500 }, 'mesmerize'],
 ['one Life Drain victim impact drives an authored cross',
  { identity: 43, current: 43, targetIdentity: 5, visual: 102, pattern: 8,
   status: 4, after: 1, sourceHP: 500 }, 'sharedEffect'],
 ['copied Life Drain effect retains its shared impact on the enemy side',
  { action: 161, identity: 43, current: 43, targetIdentity: 5, exportedVisual: true, editedDonor: 102,
   enemy: true, pattern: 7 }, 'sharedEffect'],
 ['fixed footprint 42 keeps covered contacts and cues one outlier',
  { visual: 42, pattern: 8, outside: [16] }, 'footprint'],
 ['fixed footprint 68 keeps covered contacts and cues one outlier',
  { visual: 68, pattern: 8, outside: [16] }, 'footprint'],
 ['fixed footprint 80 keeps covered contacts and cues three outliers',
  { visual: 80, pattern: 8, outside: [12,14,16] }, 'footprint'],
 ['effect 49 retires its trailing artwork after the round',
  { identity: 43, current: 43, targetIdentity: 5, visual: 49, pattern: 8 }, 'effectTail'],
 ['decorative effect 93 uses its first artwork cue',
  { visual: 93, pattern: 7, cuePC: 0x802091d0, cueState: 0x1000 }, 'ambient'],
 ['decorative projectile 94 waits for its native arrival',
  { visual: 94, pattern: 7, cuePC: 0x80225080, cueState: 0x1001 }, 'ambient'],
 ['decorative effect 95 uses its first artwork cue',
  { visual: 95, pattern: 7, cuePC: 0x8020936c, cueState: 0x1000 }, 'ambient'],
 ['decorative effect 96 keeps individual repeated impact cues',
  { visual: 96, pattern: 11, count: 6, hp: 2000, maxHP: 4000,
   cuePC: 0x802092b8, cueState: 0x1000 }, 'ambient'],
 ['authored recoil', { visual: 101, after: 2 }, 'recoil'],
 ['guarded spell', { visual: 101, hit: false }, 'guarded'],
 ['spell then healing in the same shifted queue', { visual: 101, single: false, shifted: true }, 'followup'],
 ['ordinary retail artwork, 18 serialized strikes',
  { visual: 101, pattern: 11, count: 18, hp: 2000, maxHP: 4000, shifted: true }, 'spell'],
 ['Pedra, 18 native strike children',
  { route: 'pedra', visual: 5, pattern: 11, count: 18, hp: 2000, maxHP: 4000 }, 'spell'],
 ['copied retail artwork with its own hue, 6 strikes',
  { action: 161, exportedVisual: true, pattern: 11, count: 6, hp: 2000, maxHP: 4000, enemy: true }, 'spell'],
 ['ordinary zero-delay imported artwork, three simultaneous targets',
  { action: 162, exportedVisual: true, pattern: 7 }, 'imported'],
 ['repeated imported artwork, 6 strikes',
  { action: 162, exportedVisual: true, pattern: 11, count: 6, hp: 2000, maxHP: 4000 }, 'spell'],
];
const filter = process.env.LORDLY_NATIVE_BATTLE_CASE?.toLowerCase();
let selected = 0, passed = 0;
for (const [name, config, kind] of cases) {
 if (filter && !name.toLowerCase().includes(filter)) continue;
 ++selected;
 try {
  battle.reset({ ...defaults, ...config });
  if (config.editedDonor) {
   // Exercise donor identification through an exported independent visual.
   const settings = battle.settings + config.action * 32;
   const visual = battle.m.r32(settings + 24);
   assert.equal(battle.m.r32(visual), 0x4c434658);
   assert.equal(battle.m.r32(visual + 4), 0x30303032);
   battle.m.w16(visual + 14, config.editedDonor);
  }
  const commands = battle.generate();
  const { ticks, effectTail, presentations } = battle.playback(1800);
  const source = battle.source, target = battle.target;
  const poses = id => battle.poses.filter(pose => pose.id === id);
  const from = poses(source), to = poses(target);
  const contacts = battle.trace.filter(event => event.event === 'impact');
  const damage = presentations.filter(result => result.kind === 1 && result.actor !== source);
  const hit = damage[0];
  const state = id => battle.m.r32(battle.actor(id) + 0x74);
  const finalHP = id => battle.m.r16(battle.actor(id) + 0x20);
  const hp = config.hp ?? 200;
  if (['melee', 'miss', 'death', 'column'].includes(kind)) {
   const advance = from.find(pose => pose.state === 2);
   const strike = from.find(pose => pose.state === 6);
   const retreat = from.find(pose => pose.state === 8);
   const idle = from.at(-1);
   assert(advance && strike && retreat, 'the native actor advanced, attacked and retreated');
   assert(advance.wall < strike.wall && strike.wall < retreat.wall);
   assert(from.some(pose => pose.pose === strike.pose && pose.frame !== strike.frame), 'attack frames advanced');
   assert.equal(idle.state, 1, 'attacker ended idle');
   assert.equal(idle.pose, 0, 'the retreat recovery pose finished');
   assert.equal(idle.x, config.enemy ? 114 : -114, 'attacker returned to the formation');
   if (kind === 'miss') {
    assert.equal(finalHP(target), hp);
    assert(presentations.some(result => result.kind === 3 && result.actor === target));
    assert(to.some(pose => pose.pose === 21), 'native guard pose appeared');
    assert.equal(to.at(-1).pose, 0, 'guard released back to idle');
   } else {
    assert(hit.wall >= strike.wall && hit.wall < retreat.wall, 'HP was applied during the attack pose');
    if (config.retail) assert.equal(hit.wall, strike.wall + 10, 'retail Slash pose contact');
    assert(to.some(pose => pose.wall === hit.wall && pose.pose === 17), 'hurt began on the damage tick');
    if (kind === 'death') {
     assert(to.some(pose => pose.pose === 29), 'the native death pose played');
     assert.equal(finalHP(target), 0);
     assert.equal(battle.m.r32(battle.actor(target)), 0, 'dead body left the active presentation');
    } else {
     assert.equal(to.at(-1).pose, 0, 'hurt and recovery finished');
    }
    if (kind === 'column') {
     const approach = commands.find(command => command.type === 2);
     assert.equal(approach.bytes[5], 10, 'the shared strike approached the front actor');
     assert.deepEqual(damage.map(result => result.actor).sort((a, b) => a - b), [10, 13, 16]);
     assert(damage.every(result => result.wall === hit.wall), 'one melee animation damaged the whole column');
    }
   }
  } else if (kind === 'ranged') {
   assert(hit && from.some(pose => pose.state === 6));
   assert.equal(from.at(-1).pose, 0);
  } else if (kind === 'healing') {
   assert.equal(contacts.length, 1);
   const ally = contacts[0].id;
   assert(ally < 9 && ally !== source, 'healing contacted an ally');
   assert.equal(finalHP(ally), hp + 60);
   assert(presentations.some(result => result.actor === ally && result.kind === 2 &&
    result.wall === contacts[0].wall + 1));
  } else if (kind === 'guarded') {
   assert.equal(contacts.length, 1);
   assert.equal(finalHP(target), hp - 30, 'retail spell guard halves damage');
   assert(presentations.some(result => result.actor === target && result.amount === 30 &&
    result.wall === contacts[0].wall + 1));
  } else if (kind === 'blocked') {
   assert.equal(contacts.length, 1);
   assert.equal(finalHP(target), hp);
   assert.equal(battle.m.r32(battle.actor(target) + 0x40) & 4, 0);
   assert(presentations.some(result => result.actor === target && result.kind === 13 &&
    result.wall === contacts[0].wall + 1), 'failed Sleep produced the native no-effect result');
  } else if (kind === 'followup') {
   assert.deepEqual(contacts.map(contact => contact.id), [target, battle.followSource]);
   assert.deepEqual(presentations.map(result => [result.actor, result.kind, result.amount, result.wall]),
    [[target, 1, 60, contacts[0].wall + 1], [battle.followSource, 2, 60, contacts[1].wall + 1]]);
  } else if (kind === 'mesmerize' || kind === 'sharedEffect') {
   const submission = battle.trace.find(event => event.event === 'visual');
   const targets = submission.targets;
   assert.equal(targets.length, config.pattern === 8 ? 5 : 3);
   const main = contacts.filter(contact => targets.includes(contact.id));
   assert.deepEqual(damage.map(result => result.actor).sort((a, b) => a - b), targets.slice().sort((a, b) => a - b));
   if (kind === 'mesmerize') {
    assert.deepEqual(main.map(contact => contact.id), targets, 'Mesmerize keeps every native impact');
    assert.deepEqual(damage.map(result => result.wall), main.map(contact => contact.wall + 1));
    assert(main.every((contact, index) => !index || main[index - 1].wall < contact.wall));
   } else {
    assert.deepEqual(main.map(contact => contact.id), [targets[0]], 'the artwork still plays its one native impact');
    assert(damage.every(result => result.wall === main[0].wall + 1), 'each victim reacts to that same genuine impact');
   }
   if (config.status) assert.equal(presentations.filter(result => result.kind === 5).length, targets.length);
   if (config.after) assert.equal(presentations.filter(result => result.actor === source && result.kind === 2).length, 0);
  } else if (kind === 'footprint' || kind === 'effectTail') {
   const targets = battle.trace.find(event => event.event === 'visual').targets;
   const firstContact = actor => contacts.find(contact => contact.id === actor);
   assert.equal(targets.length, 5);
   assert.deepEqual(damage.map(result => result.actor).sort((a, b) => a - b), targets.slice().sort((a, b) => a - b));
   const uncovered = targets.filter(actor => !firstContact(actor));
   assert.deepEqual(uncovered, config.outside ?? [], 'native footprint outliers in this formation');
   for (const result of damage) {
    const contact = firstContact(result.actor) ?? firstContact(targets[0]);
    assert.equal(result.wall, contact.wall + 1, `correct contact timing for actor ${result.actor}`);
   }
   if (kind === 'effectTail') assert.equal(effectTail, 9, 'native trailing visuals retire independently of the round');
  } else if (kind === 'ambient') {
   const cues = battle.trace.filter(event => event.event === 'artwork-cue');
   const expected = config.pattern === 11 ? config.count : 3;
   assert.equal(contacts.length, expected);
   assert.equal(damage.length, expected);
   if (config.pattern === 11) {
    assert.deepEqual(contacts.map(contact => contact.wall), cues.map(cue => cue.wall));
    assert(cues.every((cue, index) => !index || cues[index - 1].wall < cue.wall));
   } else {
    assert.equal(cues.length, 1);
    assert(contacts.every(contact => contact.wall === cues[0].wall));
   }
   assert.deepEqual(damage.map(result => [result.actor, result.wall, result.amount]),
    contacts.map(contact => [contact.id, contact.wall + 1, 60]));
  } else {
   const expected = config.pattern === 11 ? config.count : config.pattern === 7 ? 3 : 1;
   const main = contacts.filter(contact => contact.id !== source);
   assert.equal(main.length, expected, 'one genuine visual impact for every hit');
   assert.equal(damage.length, expected, 'one main HP presentation for every hit');
   assert.deepEqual(damage.map(result => [result.actor, result.wall, result.amount]),
    main.map(contact => [contact.id, contact.wall + 1, 60]), 'damage followed each native impact');
   if (config.pattern === 11) {
    assert(main.every((contact, index) => !index || main[index - 1].wall < contact.wall),
     'native strikes played on separate ticks');
    assert.equal(finalHP(main[0].id), hp - 60 * expected);
   }
   const held = { petrify: [38, 35, 2], sleep: [34, 37, 4], paralysis: [32, 26, 8] }[kind];
   if (held) {
    const [pose, heldState, flag] = held;
    assert(to.some(pose => pose.pose === 17), 'the native hurt pose played');
    assert.equal(to.at(-1).pose, pose, 'the native status pose held');
    assert.equal(state(target), heldState);
    assert.equal(battle.m.r32(battle.actor(target) + 0x40) & flag, flag);
   }
   if (kind === 'drain' || kind === 'recoil') {
    const after = presentations.find(result => result.actor === source && result.kind === (kind === 'drain' ? 2 : 1));
    assert(after && after.amount === 15 && after.wall >= hit.wall);
    assert.equal(finalHP(source), kind === 'drain' ? 215 : 185);
   }
   if (kind === 'imported') {
    const submission = battle.trace.find(event => event.event === 'visual');
    assert(submission.slot >= 136, 'uses the compiled independent visual slot');
    assert(main.every(contact => contact.wall === submission.wall + 1),
     'zero-delay import waited for its result packets, then hit all three targets together');
   }
   if (kind === 'combined') {
    assert.deepEqual(battle.trace.find(event => event.event === 'visual').sources, [source, source + 1]);
    assert(from.some(pose => pose.pose === 40), 'the native cast pose played');
    assert.equal(from.at(-1).pose, 0, 'the caster returned to idle');
   }
  }
  ++passed;
  console.log(`PASS ${name}, ${ticks} ticks, ${contacts.length} native contacts`);
 } catch (error) {
  console.error(`FAIL ${name}: ${error.message}`);
  console.error(JSON.stringify(battle.trace.filter(event => event.event).slice(-20)));
 }
}
assert(selected, 'case filter selected no native battle cases');
assert.equal(passed, selected, 'native battle playback regressions');
console.log(`${passed}/${selected} native battle playback cases passed`);
