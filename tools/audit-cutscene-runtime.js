'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const MASTER = path.join(ROOT,
  'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

// Commands with an explicit state transition in cutscene-runtime.js. Queries
// and parser-control nodes are dispatched through their own shared handlers.
const RUNTIME_MODELED_COMMAND_OPCODES = new Set([
  0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x13, 0x14, 0x15,
  0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x22, 0x2A, 0x2C, 0x33, 0x35,
  0x36, 0x39, 0x3A, 0x3B, 0x3D, 0x3F, 0x45, 0x46, 0x47, 0x48, 0x56, 0x59, 0x5F,
  0x62, 0x63, 0x64, 0x66, 0x69, 0x6B, 0x6E, 0x6F, 0x70, 0x73, 0x76, 0x7B, 0x7D,
  0x7E, 0x83, 0x8B, 0x8C, 0x99, 0x9A, 0xAB, 0xAF, 0xB0, 0xB4, 0xBB, 0xBF,
  0x80000001, 0x80000006, 0x80000007,
]);

function isRuntimeModeledNode(node) {
  if (!node) return true;
  if (node.query) return true;
  if (node.name === 'handoff_marker' || node.name === 'branch_barrier' ||
      node.name === 'control_bridge_and_pending_substream_handoff') return true;
  return RUNTIME_MODELED_COMMAND_OPCODES.has(node.rawWords[0] >>> 0);
}

global.window = global;
global.module = undefined;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const file of [
  'data.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js',
  'cutscene-director.js', 'cutscene-codec.js', 'cutscene-runtime.js',
  'cutscene-renderer.js',
]) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, file), 'utf8'),
    { filename: file });
}

function normalizeV64(raw) {
  const output = new Uint8Array(raw.length);
  for (let index = 0; index < raw.length; index += 2) {
    output[index] = raw[index + 1];
    output[index + 1] = raw[index];
  }
  return output;
}

function hashBytes(input) {
  return crypto.createHash('sha256').update(Buffer.from(input))
    .digest('hex').toUpperCase();
}

function option(name, fallback) {
  const prefix = '--' + name + '=';
  const value = process.argv.slice(2).find(argument => argument.startsWith(prefix));
  return value ? value.slice(prefix.length) : fallback;
}

(async function main() {
  const maxTicks = Number(option('max-ticks', '30000'));
  if (!Number.isInteger(maxTicks) || maxTicks < 1) {
    throw new Error('--max-ticks must be a positive integer');
  }
  const requested = option('scene', '').split(',').filter(Boolean);
  const corpus = option('corpus', 'all');
  if (!['all', 'byte-proven'].includes(corpus)) {
    throw new Error('--corpus must be all or byte-proven');
  }
  const summaryOnly = option('summary-only', 'false') === 'true';
  const includeQueryComparisons = option(
    'include-query-comparisons', summaryOnly ? 'false' : 'true') === 'true';
  const progressEvery = Number(option('progress-every', '0'));
  if (!Number.isInteger(progressEvery) || progressEvery < 0) {
    throw new Error('--progress-every must be a nonnegative integer');
  }
  const listLimited = option('list-limited', summaryOnly ? 'false' : 'true') === 'true';
  const limitedDetailCount = Number(option('limited-detail-count', '0'));
  if (!Number.isInteger(limitedDetailCount) || limitedDetailCount < 0) {
    throw new Error('--limited-detail-count must be a nonnegative integer');
  }
  const listNoBackground = option('list-no-background', 'false') === 'true';
  const listNoVisibleActors = option('list-no-visible-actors', 'false') === 'true';
  const queryFilter = option('query', '');
  const primitiveNameFilter = option('primitive-name', '');
  const selectedOnly = option('selected-only', 'false') === 'true';
  const includeLaunch = option('include-launch', 'false') === 'true';
  const nativeLaunchContexts = option('native-launch-contexts', 'false') === 'true';
  const contextSceneIdentity = option('context-scene', '');
  const contextTickOffset = Number(option('context-tick-offset', '1'));
  if (!Number.isInteger(contextTickOffset)) {
    throw new Error('--context-tick-offset must be an integer');
  }
  if (nativeLaunchContexts && contextSceneIdentity) {
    throw new Error('--native-launch-contexts and --context-scene are mutually exclusive');
  }
  const traceNode = option('trace-node', '');
  const controllerMaskText = option('controller-mask', '');
  const stateTicks = option('state-ticks', '').split(',').filter(Boolean)
    .map(value => Number.parseInt(value, 10));
  if (stateTicks.some(value => !Number.isInteger(value) || value < 0)) {
    throw new Error('--state-ticks must be a comma-separated list of nonnegative integers');
  }
  const controllerMask = controllerMaskText
    ? Number.parseInt(controllerMaskText, /^0x/i.test(controllerMaskText) ? 16 : 10)
    : null;
  if (controllerMaskText && !Number.isInteger(controllerMask)) {
    throw new Error('--controller-mask must be an integer or hexadecimal value');
  }
  const primitiveRangeText = option('primitive-range', '');
  const primitiveRangeMatch = /^(?:0x)?([0-9a-f]+)[:.-](?:0x)?([0-9a-f]+)$/i
    .exec(primitiveRangeText);
  if (primitiveRangeText && !primitiveRangeMatch) {
    throw new Error('--primitive-range must be hexadecimal START:END');
  }
  const primitiveRange = primitiveRangeMatch ? {
    start: parseInt(primitiveRangeMatch[1], 16),
    end: parseInt(primitiveRangeMatch[2], 16),
  } : null;
  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scenes = requested.length
    ? requested.map(identity => catalog.getScene(identity))
    : catalog.directorScenes.filter(scene =>
      corpus === 'all' || scene.source.dynamicGrammar !== true);
  if (scenes.some(scene => !scene || scene.engine !== 'director')) {
    throw new Error('--scene must identify Director scenes');
  }
  const z64 = normalizeV64(fs.readFileSync(MASTER));
  const projectionCache = new Map();
  const nativeRuntimeCache = new Map();

  async function projectScene(scene) {
    if (projectionCache.has(scene.assetId)) return projectionCache.get(scene.assetId);
    const promise = OB64.cutsceneCodec.loadSceneSource(
      z64, scene, { hashBytes }).then(source =>
      OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog));
    projectionCache.set(scene.assetId, promise);
    return promise;
  }

  function launchChoices(scene) {
    return (scene.launchProfile.parentEventLaunches || []).flatMap(launch =>
      (launch.eventInvocationContexts || []).map((context, contextIndex) => ({
        id: launch.launchId + ':' + context.eventInvocationCursor + ':' + contextIndex,
        launch,
        context,
      })));
  }

  function precedingChoice(scene, parentContext) {
    const choices = launchChoices(scene);
    return choices.find(choice =>
      choice.launch.launchId === parentContext.precedingDirectorLaunchId &&
      (parentContext.precedingDirectorInvocationCursor === null ||
        choice.context.eventInvocationCursor ===
          parentContext.precedingDirectorInvocationCursor)) || choices[0] || null;
  }

  function launchTranslations(scene, choice) {
    const values = choice && choice.context.launchTranslationTable;
    const indexes = scene.launchProfile.operandTranslation.tableIndexes || [];
    return Object.fromEntries(indexes.filter(index =>
      Array.isArray(values) && Number.isInteger(values[index])).map(index =>
      [index, values[index]]));
  }

  async function compileNativeContext(scene, forcedChoice, ancestry, compactOutput) {
    const choice = forcedChoice || launchChoices(scene)[0] || null;
    const cacheKey = scene.assetId + '|' + (choice ? choice.id : 'standalone') +
      '|' + (compactOutput ? 'compact' : 'full');
    if (nativeRuntimeCache.has(cacheKey)) return nativeRuntimeCache.get(cacheKey);
    ancestry = ancestry || [];
    const promise = projectScene(scene).then(async projected => {
      let concurrentRuntime = null;
      const contextSceneId = choice && choice.context.concurrentDirectorSceneId;
      if (contextSceneId) {
        const contextScene = catalog.getScene(contextSceneId);
        if (contextScene && !ancestry.includes(contextScene.assetId) &&
            contextScene.assetId !== scene.assetId) {
          const ownerChoice = precedingChoice(contextScene, choice.context);
          const owner = await compileNativeContext(
            contextScene, ownerChoice, ancestry.concat(scene.assetId), true);
          concurrentRuntime = owner.runtime;
        }
      }
      const runtimeOptions = Object.assign({
        z64,
        maxTicks,
        launchContext: choice ? choice.context : null,
        launchOperandTranslations: launchTranslations(scene, choice),
      }, controllerMask == null ? {} : { controllerMask });
      if (concurrentRuntime) {
        runtimeOptions.contextRuntime = concurrentRuntime;
        runtimeOptions.contextTickOffset = Number.isInteger(
          choice.context.concurrentDirectorTickOffset)
          ? choice.context.concurrentDirectorTickOffset : 1;
      }
      const runtime = OB64.cutsceneRuntime.compile(
        projected.document, projected.program, scene, catalog, runtimeOptions);
      return {
        projected,
        choice,
        runtime: compactOutput
          ? OB64.cutsceneRuntime.compactContextRuntime(runtime) : runtime,
      };
    });
    nativeRuntimeCache.set(cacheKey, promise);
    return promise;
  }

  let contextRuntime = null;
  if (contextSceneIdentity) {
    const contextScene = catalog.getScene(contextSceneIdentity);
    if (!contextScene || contextScene.engine !== 'director') {
      throw new Error('--context-scene must identify one Director scene');
    }
    const contextSource = await OB64.cutsceneCodec.loadSceneSource(
      z64, contextScene, { hashBytes });
    const contextProjected = OB64.cutsceneCodec.projectSceneDocument(
      contextScene, contextSource, catalog);
    contextRuntime = OB64.cutsceneRuntime.compile(
      contextProjected.document, contextProjected.program, contextScene, catalog,
      { z64, maxTicks, controllerMask: controllerMask == null ? undefined : controllerMask });
  }
  const rows = [];
  const queryComparisons = {};
  for (const scene of scenes) {
    let projected;
    let runtime;
    let selectedNativeContext = null;
    if (nativeLaunchContexts) {
      const compiled = await compileNativeContext(scene, null, [], false);
      projected = compiled.projected;
      runtime = compiled.runtime;
      selectedNativeContext = compiled.choice;
    } else {
      projected = await projectScene(scene);
      runtime = OB64.cutsceneRuntime.compile(
        projected.document, projected.program, scene, catalog, Object.assign(
          { z64, maxTicks }, controllerMask == null ? {} : { controllerMask },
          contextRuntime ? { contextRuntime, contextTickOffset } : {}));
    }
    if (includeQueryComparisons) {
      projected.program.primitives.forEach(node => {
        if (!node.query || queryFilter && node.name !== queryFilter) return;
        const key = [node.name, node.query.compareMode, node.query.target,
          node.query.producerInput == null ? '' : node.query.producerInput].join('|');
        queryComparisons[key] = (queryComparisons[key] || 0) + 1;
      });
    }
    const finalState = runtime.states[runtime.states.length - 1];
    const finalComposite = projected.program.composites[
      Math.max(0, finalState.runtime.compositeIndex - 1)];
    const branchQueries = runtime.trace.filter(row => row.kind === 'branch-query');
    const loopWindowStart = Math.max(0, runtime.durationTicks - 10000);
    const loopBranchQueryGroups = new Map();
    branchQueries.filter(row => row.tick >= loopWindowStart).forEach(row => {
      const node = projected.program.primitiveById[row.nodeId];
      if (!node || !node.query) return;
      const key = [row.nodeId, row.compareMode, row.target,
        node.query.producerInput == null ? '' : node.query.producerInput].join('|');
      if (!loopBranchQueryGroups.has(key)) {
        loopBranchQueryGroups.set(key, {
          id: row.nodeId,
          name: node.name,
          compareMode: row.compareMode,
          target: row.target,
          producerInput: node.query.producerInput,
          count: 0,
          passedCount: 0,
          failedCount: 0,
          actualCounts: {}
        });
      }
      const group = loopBranchQueryGroups.get(key);
      group.count += 1;
      if (row.passed) group.passedCount += 1;
      else group.failedCount += 1;
      const actualKey = String(row.actual);
      group.actualCounts[actualKey] = (group.actualCounts[actualKey] || 0) + 1;
    });
    const recurringLoopBranchQueries = Array.from(loopBranchQueryGroups.values())
      .filter(group => group.count > 1)
      .sort((left, right) => right.failedCount - left.failedCount ||
        right.count - left.count || left.id.localeCompare(right.id));
    const dominantFailedLoopQuery = recurringLoopBranchQueries.find(group =>
      group.failedCount > 0) || null;
    const cursorReplacementGroups = new Map();
    runtime.trace.filter(row => row.kind === 'cursor-replacement' &&
      row.tick >= loopWindowStart).forEach(row => {
      const key = [row.sourceNodeId, row.marker, row.destinationNodeId].join('|');
      if (!cursorReplacementGroups.has(key)) {
        cursorReplacementGroups.set(key, {
          sourceNodeId: row.sourceNodeId,
          marker: row.marker,
          destinationNodeId: row.destinationNodeId,
          destinationWord: row.destinationWord,
          count: 0
        });
      }
      cursorReplacementGroups.get(key).count += 1;
    });
    const recurringCursorReplacements = Array.from(cursorReplacementGroups.values())
      .filter(group => group.count > 1)
      .sort((left, right) => right.count - left.count ||
        left.sourceNodeId.localeCompare(right.sourceNodeId));
    const finalCompositeQuery = finalComposite && finalComposite.nodeIds
      .map(nodeId => projected.program.primitiveById[nodeId])
      .filter(node => node && node.query).at(-1) || null;
    const lastBranchQuery = branchQueries.at(-1) || null;
    const lastBranchNode = lastBranchQuery
      ? projected.program.primitiveById[lastBranchQuery.nodeId] : null;
    const limitingQueryNode = finalState.runtime.blockKind === 'query'
      ? finalCompositeQuery : lastBranchNode;
    const finalVisibleActors = finalState.actors.filter(actor => actor.visible);
    const maxVisibleActorCount = runtime.states.reduce((maximum, state) =>
      Math.max(maximum, state.actors.filter(actor => actor.visible).length), 0);
    const maxBackgroundLayerCount = runtime.states.reduce((maximum, state) =>
      Math.max(maximum, state.background.layers.length), 0);
    const representativeActorState = maxVisibleActorCount > 0
      ? runtime.states.find(state =>
        state.actors.filter(actor => actor.visible).length === maxVisibleActorCount)
      : null;
    let representativeProjection = representativeActorState &&
      representativeActorState.actorProjection;
    if (representativeProjection &&
        representativeProjection.evidenceStatus === 'external-unresolved') {
      representativeProjection = OB64.cutsceneRenderer.computeProjection(
        projected.document, representativeActorState);
    }
    const representativeActorScales = representativeActorState
      ? representativeActorState.actors.filter(actor => actor.visible).map(actor => {
        const geometry = OB64.cutsceneRenderer.modeZeroActorGeometry(
          actor, representativeActorState, representativeProjection);
        const rawScale = geometry ? geometry.scale :
          (Number.isFinite(actor.uniformScale) ? actor.uniformScale : 1) *
            OB64.cutsceneRenderer.perspectivePixelsPerModelUnit(
              actor, representativeProjection);
        return {
          slot: actor.slot,
          rawScale,
          renderedScale: Math.min(16, Math.max(0.05, rawScale)),
        };
      }).filter(row => Number.isFinite(row.rawScale))
      : [];
    const actorCreateNodes = projected.program.primitives.filter(node =>
      (node.rawWords[0] >>> 0) === 0x14);
    const actorRosterNodes = projected.program.primitives.filter(node =>
      (node.rawWords[0] >>> 0) === 0x45 || (node.rawWords[0] >>> 0) === 0xAB);
    const executedNodeIds = new Set(runtime.executedNodeIds);
    const unmodeledCommandNodes = projected.program.primitives.filter(node =>
      !isRuntimeModeledNode(node)).map(node => ({
        id: node.id,
        opcode: node.rawWords[0] >>> 0,
        opcodeHex: node.opcodeHex,
        name: node.name,
        executed: executedNodeIds.has(node.id),
      }));
    rows.push({
      sceneId: scene.sceneId,
      selectedNativeLaunchContext: selectedNativeContext ? {
        launchId: selectedNativeContext.launch.launchId,
        eventInvocationCursor:
          selectedNativeContext.context.eventInvocationCursor,
        concurrentDirectorAssetId:
          selectedNativeContext.context.concurrentDirectorAssetId,
        concurrentDirectorTickOffset:
          selectedNativeContext.context.concurrentDirectorTickOffset,
      } : null,
      concurrentContext: runtime.concurrentContext,
      launchSceneStatePolicy: runtime.launchSceneStatePolicy,
      sourceOffset: scene.source.romOffset,
      terminationWordStart: scene.source.terminationWordStart,
      launchClass: scene.launchProfile.launchContext &&
        scene.launchProfile.launchContext.classId,
      launchDirectorMode: scene.launchProfile.directorMode &&
        scene.launchProfile.directorMode.value,
      backgroundRequestCount: scene.launchProfile.background &&
        scene.launchProfile.background.requestCount || 0,
      backgroundRequests: (scene.launchProfile.background &&
        scene.launchProfile.background.requests || []).map(request => ({
          selector: request.selector,
          selectorTableId: request.selectorTableId,
          assetIds: (request.assetIds || []).slice()
        })),
      parentEventLaunchCount: (scene.launchProfile.parentEventLaunches || []).length,
      concurrentContextOwnerIds: Array.from(new Set(
        (scene.launchProfile.parentEventLaunches || []).flatMap(launch =>
          (launch.eventInvocationContexts || []).map(context =>
            context.concurrentDirectorAssetId)).filter(Boolean))),
      launchProfile: includeLaunch ? scene.launchProfile : undefined,
      sourceProfile: includeLaunch ? scene.source : undefined,
      ticks: runtime.durationTicks,
      terminated: runtime.terminated,
      terminationReason: runtime.terminationReason,
      safetyLimited: runtime.safetyLimited,
      finalCompositeIndex: finalState.runtime.compositeIndex,
      finalBlockKind: finalState.runtime.blockKind,
      finalBlockLabel: finalState.runtime.blockLabel,
      limitingQuery: limitingQueryNode ? {
        source: finalState.runtime.blockKind === 'query'
          ? 'blocked-composite' : 'last-branch',
        id: limitingQueryNode.id,
        name: limitingQueryNode.name,
        compareMode: limitingQueryNode.query.compareMode,
        target: limitingQueryNode.query.target,
        producerInput: limitingQueryNode.query.producerInput,
        actual: lastBranchQuery && lastBranchQuery.nodeId === limitingQueryNode.id
          ? lastBranchQuery.actual : null,
        passed: lastBranchQuery && lastBranchQuery.nodeId === limitingQueryNode.id
          ? lastBranchQuery.passed : null,
      } : null,
      finalComposite: finalComposite ? {
        id: finalComposite.id,
        kind: finalComposite.kind,
        nodes: finalComposite.nodeIds.map(nodeId => {
          const node = projected.program.primitiveById[nodeId];
          return {
            id: node.id,
            name: node.name,
            rawWords: node.rawWords,
            query: node.query,
          };
        }),
      } : null,
      stateSamples: stateTicks.map(tick => {
        const state = runtime.states[Math.min(tick, runtime.states.length - 1)];
        return {
          requestedTick: tick,
          actualTick: state.frame,
          runtime: state.runtime,
          actorProjection: state.actorProjection,
          registeredProjection: state.registeredProjection,
          cameraState: state.cameraState,
          transformChannels: state.transformChannels.map((channel, channelIndex) => ({
            channelIndex,
            rotationX: channel.rotationX,
            rotationY: channel.rotationY,
            translateX: channel.translateX,
            translateY: channel.translateY,
            translateZ: channel.translateZ,
            uniformScale: channel.uniformScale,
          })).filter(channel => channel.rotationX !== 0 || channel.rotationY !== 0 ||
            channel.translateX !== 0 || channel.translateY !== 0 ||
            channel.translateZ !== 0 || channel.uniformScale !== 1),
          backgroundLayers: state.background.layers.map(layer => ({
            assetId: layer.assetId,
            sceneTransform: layer.sceneTransform,
          })),
          visibleActors: state.actors.filter(actor => actor.visible).map(actor => ({
            slot: actor.slot, x: actor.x, y: actor.y, z: actor.z,
            uniformScale: actor.uniformScale,
          })),
          dialogue: state.dialogue.map(row => ({
            label: row.label,
            paused: row.payload && row.payload.paused,
            entrySelector: row.payload && row.payload.presentationEntrySelector,
          })),
        };
      }),
      primitiveRange: primitiveRange
        ? projected.program.primitives.filter(node =>
          node.startWord >= primitiveRange.start && node.startWord <= primitiveRange.end)
          .map(node => ({
            id: node.id,
            startWord: node.startWord,
            name: node.name,
            rawWords: node.rawWords,
            query: node.query,
            compositeId: projected.program.compositeByNodeId[node.id],
          }))
        : [],
      selectedPrimitives: primitiveNameFilter
        ? projected.program.primitives.filter(node =>
          node.name.indexOf(primitiveNameFilter) !== -1).map(node => ({
            id: node.id,
            startWord: node.startWord,
            name: node.name,
            rawWords: node.rawWords,
            query: node.query,
            compositeId: projected.program.compositeByNodeId[node.id],
          }))
        : [],
      primitiveCount: runtime.sourcePrimitiveCount,
      executedPrimitiveCount: runtime.executedPrimitiveCount,
      primitiveBranchQueries: summaryOnly ? [] : projected.program.primitives.filter(node =>
        node.query && projected.program.compositeById[
          projected.program.compositeByNodeId[node.id]].kind === 'primitive')
        .map(node => {
          const index = projected.program.primitives.indexOf(node);
          return {
            id: node.id,
            name: node.name,
            rawWords: node.rawWords,
            query: node.query,
            following: projected.program.primitives.slice(index + 1, index + 5)
              .map(next => ({ id: next.id, name: next.name, rawWords: next.rawWords })),
          };
        }),
      branchQueryCount: branchQueries.length,
      failedBranchQueryCount: branchQueries.filter(row => !row.passed).length,
      cursorReplacementCount: runtime.trace.filter(
        row => row.kind === 'cursor-replacement').length,
      loopWindowStart,
      recurringLoopBranchQueries,
      dominantFailedLoopQuery,
      recurringCursorReplacements,
      parserBoundaryCount: runtime.trace.filter(
        row => row.kind === 'parser-resume-commit').length,
      backgroundLayerCount: finalState.background.layers.length,
      maxBackgroundLayerCount,
      backgroundMissingInputs: runtime.missingInputs.filter(message =>
        /background|environment|foreground|stage|b5/i.test(message)),
      visibleActorCount: finalVisibleActors.length,
      maxVisibleActorCount,
      representativeActorFrame: representativeActorState
        ? representativeActorState.frame : null,
      representativeProjectionMode: representativeProjection
        ? representativeProjection.mode : null,
      representativeProjectionEvidence: representativeProjection
        ? representativeProjection.evidenceStatus || null : null,
      representativeActorScales,
      minimumRepresentativeActorScale: representativeActorScales.length
        ? Math.min(...representativeActorScales.map(row => row.rawScale)) : null,
      maximumRepresentativeActorScale: representativeActorScales.length
        ? Math.max(...representativeActorScales.map(row => row.rawScale)) : null,
      actorCreateCommandCount: actorCreateNodes.length,
      executedActorCreateCommandCount: actorCreateNodes.filter(node =>
        executedNodeIds.has(node.id)).length,
      actorRosterMaterializerCount: actorRosterNodes.length,
      executedActorRosterMaterializerCount: actorRosterNodes.filter(node =>
        executedNodeIds.has(node.id)).length,
      unmodeledCommandNodes,
      finalVisibleActors: summaryOnly ? [] : finalVisibleActors.map(actor => ({
        slot: actor.slot, bank: actor.bank, animationKey: actor.animationKey,
        facing: actor.nativeFacing, variant: actor.variantSelector,
        x: actor.baseX, y: actor.baseY, z: actor.baseZ,
        uniformScale: actor.nativeUniformScale,
      })),
      finalDialogue: summaryOnly ? [] : finalState.dialogue.map(row => ({
        label: row.label,
        paused: row.payload && row.payload.paused,
        entrySelector: row.payload && row.payload.presentationEntrySelector,
      })),
      missingInputs: summaryOnly ? [] : runtime.missingInputs,
      assumptions: summaryOnly ? [] : runtime.assumptions,
      finalFlowTrace: summaryOnly ? [] : runtime.trace.filter(row =>
        row.kind === 'branch-query' || row.kind === 'cursor-replacement').slice(-60),
      selectedTrace: traceNode
        ? runtime.trace.filter(row => row.nodeId === traceNode ||
          row.sourceNodeId === traceNode || row.destinationNodeId === traceNode)
        : [],
      finalTrace: summaryOnly ? [] : runtime.trace.slice(-12),
      diagnosticTail: runtime.trace.slice(-12),
      diagnosticDialogue: finalState.dialogue.map(row => ({
        label: row.label,
        paused: row.payload && row.payload.paused,
        entrySelector: row.payload && row.payload.presentationEntrySelector,
      })),
    });
    if (nativeLaunchContexts) nativeRuntimeCache.clear();
    projectionCache.clear();
    if (typeof global.gc === 'function' && rows.length % 10 === 0) global.gc();
    if (progressEvery && rows.length % progressEvery === 0) {
      process.stderr.write('Audited ' + rows.length + '/' + scenes.length + ' scenes.\n');
    }
  }

  const limited = rows.filter(row => row.safetyLimited);
  const noBackground = rows.filter(row => row.maxBackgroundLayerCount === 0);
  const noVisibleActors = rows.filter(row => row.maxVisibleActorCount === 0);
  function classifyLimited(row) {
    const query = row.limitingQuery;
    if (row.finalBlockKind === 'query') {
      if (!query) return 'blocked-query-without-owner';
      if (query.name === 'dialogue_pause_query') {
        return 'dialogue-wait-at-safety-limit';
      }
      if (query.name === 'registered_counter_query' ||
          query.name === 'a_button_skippable_registered_wait_query') {
        return 'registered-counter-wait-at-safety-limit';
      }
      if (query.name === 'actor_movement_countdown_query' ||
          query.name === 'actor_facing_turn_activity_query' ||
          query.name === 'scene_projection_transform_countdown_query_mode2' ||
          query.name === 'scene_projection_transform_countdown_query_unguarded' ||
          query.name === 'color_overlay_countdown_query' ||
          query.name === 'actor_state_pose_opcode_query' ||
          query.name === 'scene_transform_sequence_query' ||
          query.name === 'prologue_title_reveal_query') {
        return 'modeled-job-wait-at-safety-limit';
      }
      return 'external-input-wait-at-safety-limit';
    }
    if (row.cursorReplacementCount > 0 && row.failedBranchQueryCount > 0) {
      return 'branch-history-at-safety-limit';
    }
    if (row.finalBlockKind === 'parser-boundary' && row.parserBoundaryCount > 0) {
      return 'parser-boundary-after-resume-at-safety-limit';
    }
    if (row.finalBlockKind === 'parser-boundary') return 'parser-boundary-at-safety-limit';
    return 'unclassified-safety-limit';
  }
  function classifyLoopOwner(row) {
    if (row.dominantFailedLoopQuery) {
      return [row.dominantFailedLoopQuery.name,
        row.dominantFailedLoopQuery.compareMode,
        row.dominantFailedLoopQuery.target,
        row.dominantFailedLoopQuery.producerInput == null
          ? '' : row.dominantFailedLoopQuery.producerInput].join('|');
    }
    if (row.recurringCursorReplacements.length) {
      return 'cursor-replacement-without-recurring-failed-query';
    }
    if (row.finalBlockKind === 'query' && row.limitingQuery) {
      return ['nonrecurring-wait', row.limitingQuery.name,
        row.limitingQuery.compareMode, row.limitingQuery.target,
        row.limitingQuery.producerInput == null
          ? '' : row.limitingQuery.producerInput].join('|');
    }
    return 'no-recurring-query-owner';
  }
  function countBy(inputRows, keyForRow) {
    const counts = {};
    inputRows.forEach(row => {
      const key = keyForRow(row);
      counts[key] = (counts[key] || 0) + 1;
    });
    return Object.fromEntries(Object.entries(counts)
      .sort((left, right) => right[1] - left[1] || left[0].localeCompare(right[0])));
  }
  function classifyNoVisibleActor(row) {
    if (row.actorCreateCommandCount === 0 && row.actorRosterMaterializerCount === 0) {
      return 'actor-free-stream';
    }
    if (row.actorCreateCommandCount === 0 && row.actorRosterMaterializerCount > 0) {
      return row.executedActorRosterMaterializerCount > 0
        ? 'launch-roster-input-required'
        : 'roster-materializer-not-reached';
    }
    if (row.executedActorCreateCommandCount === 0) {
      return 'actor-create-command-not-reached';
    }
    return 'actor-producer-executed-without-visible-state';
  }
  function classifyActorScale(row) {
    if (row.maxVisibleActorCount === 0) return 'no-visible-actor';
    const maximum = row.maximumRepresentativeActorScale;
    if (!Number.isFinite(maximum)) return 'unmeasured';
    if (maximum < 0.05) return 'all-below-render-clamp';
    if (maximum < 0.25) return 'all-below-quarter-scale';
    if (maximum < 0.5) return 'all-below-half-scale';
    if (maximum < 0.75) return 'all-below-three-quarter-scale';
    return 'at-least-one-three-quarter-scale';
  }
  const selectedPrimitiveRows = primitiveNameFilter ? rows.flatMap(row =>
    row.selectedPrimitives.map(primitive => ({
      sceneId: row.sceneId,
      launchClass: row.launchClass,
      startWord: primitive.startWord,
      operands: primitive.rawWords.slice(1).map(word => word >>> 0),
      tailDistance: row.terminationWordStart -
        (primitive.startWord + primitive.rawWords.length)
    }))) : [];
  const selectedPrimitiveExampleGroups = primitiveNameFilter
    ? Array.from(new Set(selectedPrimitiveRows.map(row =>
      row.operands.join(',') + '|tail+' + row.tailDistance))) : [];
  const unmodeledCommandGroups = new Map();
  rows.forEach(row => row.unmodeledCommandNodes.forEach(node => {
    const key = node.opcodeHex + '|' + node.name;
    if (!unmodeledCommandGroups.has(key)) {
      unmodeledCommandGroups.set(key, {
        opcode: node.opcode,
        opcodeHex: node.opcodeHex,
        name: node.name,
        sourceNodeCount: 0,
        executedNodeCount: 0,
        sourceSceneIds: new Set(),
        executedSceneIds: new Set(),
      });
    }
    const group = unmodeledCommandGroups.get(key);
    group.sourceNodeCount += 1;
    group.sourceSceneIds.add(row.sceneId);
    if (node.executed) {
      group.executedNodeCount += 1;
      group.executedSceneIds.add(row.sceneId);
    }
  }));
  const unmodeledCommandCoverage = Array.from(unmodeledCommandGroups.values())
    .map(group => ({
      opcode: group.opcode,
      opcodeHex: group.opcodeHex,
      name: group.name,
      sourceSceneCount: group.sourceSceneIds.size,
      sourceNodeCount: group.sourceNodeCount,
      executedSceneCount: group.executedSceneIds.size,
      executedNodeCount: group.executedNodeCount,
      executedSceneExamples: Array.from(group.executedSceneIds).slice(0, 8),
    }))
    .sort((left, right) => right.executedSceneCount - left.executedSceneCount ||
      right.executedNodeCount - left.executedNodeCount ||
      left.opcode - right.opcode);
  const summary = {
    resourceCount: rows.length,
    corpus,
    maxTicks,
    nativeLaunchContexts,
    contextualRuntimeCount: rows.filter(row => row.concurrentContext !== null).length,
    terminatedCount: rows.filter(row => row.terminated).length,
    terminationReasons: countBy(rows.filter(row => row.terminated), row =>
      row.terminationReason || 'unclassified-terminal'),
    safetyLimitedCount: limited.length,
    backgroundCount: rows.filter(row => row.backgroundLayerCount > 0).length,
    everBackgroundCount: rows.filter(row => row.maxBackgroundLayerCount > 0).length,
    visibleActorCount: rows.filter(row => row.visibleActorCount > 0).length,
    everVisibleActorCount: rows.filter(row => row.maxVisibleActorCount > 0).length,
    noBackgroundSceneIds: listNoBackground ? noBackground.map(row => row.sceneId) : [],
    noBackgroundDetails: listNoBackground ? noBackground.map(row => ({
      sceneId: row.sceneId,
      launchClass: row.launchClass,
      launchDirectorMode: row.launchDirectorMode,
      backgroundRequests: row.backgroundRequests,
      parentEventLaunchCount: row.parentEventLaunchCount,
      concurrentContextOwnerIds: row.concurrentContextOwnerIds,
      backgroundMissingInputs: row.backgroundMissingInputs
    })) : [],
    noVisibleActorSceneIds: listNoVisibleActors
      ? noVisibleActors.map(row => row.sceneId) : [],
    noVisibleActorClasses: countBy(noVisibleActors, classifyNoVisibleActor),
    noVisibleActorClassExamples: Object.fromEntries(Object.keys(
      countBy(noVisibleActors, classifyNoVisibleActor)).map(classification => [
        classification,
        noVisibleActors.filter(row => classifyNoVisibleActor(row) === classification)
          .slice(0, 8).map(row => row.sceneId)
      ])),
    representativeActorScaleClasses: countBy(rows, classifyActorScale),
    representativeActorScaleClassExamples: Object.fromEntries(Object.keys(
      countBy(rows, classifyActorScale)).map(classification => [
        classification,
        rows.filter(row => classifyActorScale(row) === classification)
          .slice(0, 8).map(row => ({
            sceneId: row.sceneId,
            frame: row.representativeActorFrame,
            projectionMode: row.representativeProjectionMode,
            projectionEvidence: row.representativeProjectionEvidence,
            minimumScale: row.minimumRepresentativeActorScale,
            maximumScale: row.maximumRepresentativeActorScale,
          }))
      ])),
    noBackgroundLaunchClasses: countBy(noBackground, row => String(row.launchClass)),
    noBackgroundDirectorModes: countBy(noBackground, row =>
      row.launchDirectorMode == null ? 'unresolved' : String(row.launchDirectorMode)),
    noBackgroundRequestCounts: countBy(noBackground, row =>
      String(row.backgroundRequestCount)),
    noBackgroundParentEventOwnership: countBy(noBackground, row =>
      row.parentEventLaunchCount > 0 ? 'event-owned' : 'source-only'),
    unmodeledCommandCoverage,
    selectedPrimitiveSceneCount: primitiveNameFilter
      ? rows.filter(row => row.selectedPrimitives.length > 0).length : 0,
    selectedPrimitiveOccurrenceCount: selectedPrimitiveRows.length,
    selectedPrimitiveLaunchClasses: primitiveNameFilter ? countBy(
      selectedPrimitiveRows, row => String(row.launchClass)) : {},
    selectedPrimitiveSceneIds: primitiveNameFilter
      ? rows.filter(row => row.selectedPrimitives.length > 0).map(row => row.sceneId) : [],
    selectedPrimitiveOperandShapes: primitiveNameFilter ? countBy(
      selectedPrimitiveRows, row => row.operands.join(',')) : {},
    selectedPrimitiveTailDistances: primitiveNameFilter ? countBy(
      selectedPrimitiveRows, row => String(row.tailDistance)) : {},
    selectedPrimitiveExamples: Object.fromEntries(selectedPrimitiveExampleGroups.map(group => [
      group,
      selectedPrimitiveRows.filter(row =>
        row.operands.join(',') + '|tail+' + row.tailDistance === group).slice(0, 8)
    ])),
    failedBranchQueryCount: rows.reduce(
      (total, row) => total + row.failedBranchQueryCount, 0),
    cursorReplacementCount: rows.reduce(
      (total, row) => total + row.cursorReplacementCount, 0),
    cursorReplacementSceneIds: rows.filter(row => row.cursorReplacementCount > 0)
      .map(row => row.sceneId),
    limitedSceneIds: listLimited ? limited.map(row => row.sceneId) : [],
    limitedContextualCount: limited.filter(row => row.concurrentContext !== null).length,
    limitedWithBackgroundCount: limited.filter(row => row.backgroundLayerCount > 0).length,
    limitedWithVisibleActorsCount: limited.filter(row => row.visibleActorCount > 0).length,
    limitedFinalBlocks: countBy(limited, row => [
      row.finalBlockKind || 'none', row.finalBlockLabel || 'none'].join('|')),
    limitedQueryClasses: countBy(limited, row => row.limitingQuery ? [
      row.limitingQuery.source,
      row.limitingQuery.name,
      row.limitingQuery.compareMode,
      row.limitingQuery.target,
      row.limitingQuery.producerInput == null ? '' : row.limitingQuery.producerInput,
    ].join('|') : 'none'),
    limitedRuntimeClasses: countBy(limited, classifyLimited),
    limitedRuntimeClassExamples: Object.fromEntries(Object.keys(
      countBy(limited, classifyLimited)).map(classification => [
        classification,
        limited.filter(row => classifyLimited(row) === classification)
          .slice(0, 8).map(row => row.sceneId)
      ])),
    limitedLoopOwnerClasses: countBy(limited, classifyLoopOwner),
    limitedLoopOwnerExamples: Object.fromEntries(Object.keys(
      countBy(limited, classifyLoopOwner)).map(classification => [
        classification,
        limited.filter(row => classifyLoopOwner(row) === classification)
          .slice(0, 8).map(row => row.sceneId)
      ])),
    limitedDetails: limitedDetailCount ? limited.slice(0, limitedDetailCount).map(row => ({
      sceneId: row.sceneId,
      ticks: row.ticks,
      classification: classifyLimited(row),
      finalCompositeIndex: row.finalCompositeIndex,
      finalBlockKind: row.finalBlockKind,
      finalBlockLabel: row.finalBlockLabel,
      limitingQuery: row.limitingQuery,
      finalComposite: row.finalComposite,
      branchQueryCount: row.branchQueryCount,
      failedBranchQueryCount: row.failedBranchQueryCount,
      cursorReplacementCount: row.cursorReplacementCount,
      loopWindowStart: row.loopWindowStart,
      dominantFailedLoopQuery: row.dominantFailedLoopQuery,
      recurringLoopBranchQueries: row.recurringLoopBranchQueries,
      recurringCursorReplacements: row.recurringCursorReplacements,
      parserBoundaryCount: row.parserBoundaryCount,
      diagnosticDialogue: row.diagnosticDialogue,
      diagnosticTail: row.diagnosticTail,
    })) : [],
    queryComparisons: includeQueryComparisons
      ? Object.fromEntries(Object.entries(queryComparisons)
        .sort((left, right) => left[0].localeCompare(right[0])))
      : undefined,
  };
  if (selectedOnly) {
    process.stdout.write(JSON.stringify({
      resourceCount: summary.resourceCount,
      primitiveNameFilter,
      selectedPrimitiveSceneCount: summary.selectedPrimitiveSceneCount,
      selectedPrimitiveOccurrenceCount: summary.selectedPrimitiveOccurrenceCount,
      selectedPrimitiveLaunchClasses: summary.selectedPrimitiveLaunchClasses,
      selectedPrimitiveSceneIds: summary.selectedPrimitiveSceneIds,
      selectedPrimitives: selectedPrimitiveRows
    }, null, 2) + '\n');
    return;
  }
  process.stdout.write(JSON.stringify(summaryOnly ? { summary } : { summary, rows },
    null, 2) + '\n');
})().catch(error => {
  process.stderr.write((error && error.stack || String(error)) + '\n');
  process.exitCode = 1;
});
