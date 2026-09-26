// Rebuild the editable StarUML project from the current Zioop class inventory.
// Usage: node docs/uml/generate-staruml.cjs
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const output = path.join(__dirname, 'zioop.mdj');
const uid = (key) => crypto.createHash('sha1').update(`zioop-staruml:${key}`).digest('base64').slice(0, 22);
const ref = (obj) => ({ $ref: typeof obj === 'string' ? obj : obj._id });
const project = { _type: 'Project', _id: uid('project'), name: 'Zioop', ownedElements: [] };
const model = { _type: 'UMLModel', _id: uid('model'), _parent: ref(project), name: 'Zioop', ownedElements: [] };
project.ownedElements.push(model);

const packages = {};
const colors = {
  core: '#eaf2ff', network: '#e8f6ec', gui: '#fff4dd', ai: '#f2eafe',
  'clients/ai': '#e7f7f8',
};
for (const name of Object.keys(colors)) {
  const pkg = { _type: 'UMLPackage', _id: uid(`package:${name}`), _parent: ref(model), name, ownedElements: [] };
  packages[name] = pkg;
  model.ownedElements.push(pkg);
}

// An attribute is [name, type, visibility]. An operation is [name, return type].
const specs = {
  'core.Game': { a: [['m_matrix', 'Matrix', 'private'], ['m_agents', 'unordered_map<size_t, AgentEntry>', 'private']], o: [['addAgent', 'optional<AgentPlacement>'], ['move', 'optional<Position>'], ['look', 'optional<LookResult>'], ['take', 'optional<TakeResult>'], ['inventory', 'optional<vector<InventoryResource>>'], ['ritual', 'optional<RitualStartResult>'], ['tick', 'vector<RitualCompleted>'], ['getMatrix', 'MatrixSnapshot'], ['removeClientAgent', 'void']] },
  'core.Matrix': { a: [['m_cells', 'vector<vector<Cell>>', 'private'], ['m_sizeX', 'size_t', 'private'], ['m_sizeY', 'size_t', 'private']], o: [['addAgent', 'optional<Position>'], ['removeAgent', 'void'], ['moveAgent', 'optional<Position>'], ['getCell', 'Cell*'], ['getVisiblePositions', 'vector<Position>']] },
  'core.Cell': { a: [['m_pos', 'Position', 'private'], ['agents', 'array<Agent*, 4>', 'private'], ['m_resources', 'vector<unique_ptr<Resource>>', 'private']], o: [['emplace', 'bool'], ['remove', 'bool'], ['take', 'unique_ptr<Resource>'], ['addResource', 'void'], ['updateResources', 'void']] },
  'core.Agent': { a: [['m_id', 'size_t', 'private'], ['m_cell', 'Cell*', 'private'], ['m_level', 'int', 'private'], ['m_inventory', 'vector<unique_ptr<Resource>>', 'private']], o: [['move', 'optional<Position>'], ['look', 'optional<LookResult>'], ['take', 'bool'], ['startRitual', 'bool'], ['completeRitual', 'bool'], ['getInventory', 'vector<InventoryResource>']] },
  'core.Resource': { abstract: true, a: [['m_type', 'ResourceType', 'protected'], ['m_rotting', 'atomic_int', 'protected']], o: [['startRotting', 'void'], ['decaying', 'void']] },
  'core.Fruit': { o: [['decaying', 'void']] },
  'core.Position': { stereotype: 'struct', a: [['x', 'size_t'], ['y', 'size_t']] },
  'core.LookResult': { stereotype: 'struct', a: [['cells', 'vector<ObservedCell>']] },
  'core.MatrixSnapshot': { stereotype: 'struct', a: [['sizeX', 'size_t'], ['sizeY', 'size_t'], ['resources', 'vector<ResourceSnapshot>'], ['agents', 'vector<AgentSnapshot>']] },
  'core.InventoryResource': { stereotype: 'struct', a: [['type', 'ResourceType'], ['quantity', 'size_t']] },
  'network.Server': { a: [['m_game', 'core::Game', 'private'], ['m_nextClientId', 'size_t', 'private'], ['m_connections', 'vector<shared_ptr<Connection>>', 'private']], o: [['Start', 'void'], ['stop', 'void'], ['tick', 'void'], ['processDisconnections', 'void'], ['handleFrame', 'void'], ['handleMessage', 'void']] },
  'network.Connection': { a: [['m_clientId', 'optional<size_t>', 'private'], ['m_socket', 'atomic<SOCKET>', 'private'], ['m_thread', 'thread', 'private'], ['m_receiveBuffer', 'vector<byte>', 'private']], o: [['getClientId', 'size_t'], ['send', 'void'], ['start', 'void'], ['run', 'void'], ['requestStop', 'void'], ['join', 'void']] },
  'network.Client': { stereotype: 'déclaré', a: [['m_connection', 'unique_ptr<Connection>', 'private'], ['m_responses', 'queue<Protocol::Response>', 'private']], o: [['connect', 'void'], ['send', 'void'], ['waitForResponse', 'Protocol::Response'], ['disconnect', 'void']] },
  'network.Protocol': { o: [['encode', 'vector<byte>'], ['decode', 'optional<Frame>']] },
  'gui.Renderer': { a: [['m_connection', 'MatrixConnection', 'private'], ['m_matrix', 'MatrixConnection::MatrixSnapshot', 'private'], ['m_agentMotions', 'unordered_map<size_t, AgentMotion>', 'private']], o: [['drawWorld', 'void'], ['updateMatrix', 'void'], ['DrawAgents', 'void'], ['DrawResources', 'void']] },
  'gui.MatrixConnection': { a: [['m_socket', 'SOCKET', 'private']], o: [['request', 'MatrixSnapshot'], ['open', 'void'], ['close', 'void']] },
  'gui.Agent': { a: [['m_model', 'Model', 'private'], ['m_animations', 'ModelAnimation*', 'private']], o: [['getModel', 'Model&']] },
  'ai.Bot': { abstract: true, a: [['m_socket', 'SOCKET', 'private'], ['m_agentId', 'size_t', 'protected'], ['m_history', 'deque<Decision>', 'private']], o: [['run', 'void'], ['think', 'void'], ['move', 'void'], ['look', 'void'], ['take', 'void'], ['inventory', 'void'], ['ritual', 'void']] },
  'ai.Claude': { o: [['think', 'void'], ['getName', 'string_view']] },
  'ai.Test': { stereotype: 'ancien code', o: [['run', 'void'], ['chooseDirection', 'core::Direction']] },
  'clients/ai.Brain': { stereotype: 'interface', a: [['name', 'string'], ['threadId', 'string | null']], o: [['decide', 'Promise<string>']] },
  'clients/ai.GameConnection': { a: [['socket', 'Socket', 'private'], ['decoder', 'FrameDecoder', 'private'], ['pending', 'callbacks | undefined', 'private']], o: [['connect', 'Promise<void>'], ['request', 'Promise<GameReply>'], ['close', 'void']] },
  'clients/ai.FrameDecoder': { a: [['buffer', 'Buffer', 'private']], o: [['push', 'Frame[]']] },
  'clients/ai.Journal': { a: [['path', 'string'], ['entries', 'HistoryEntry[]', 'private']], o: [['append', 'Promise<HistoryEntry>'], ['page', 'Page']] },
  'clients/ai.HistoryEntry': { stereotype: 'type', a: [['sequence', 'number'], ['command', 'string'], ['response', 'GameReply'], ['durationMs', 'number']] },
  'clients/ai.State': { stereotype: 'type', a: [['agentId', 'number'], ['level', 'number'], ['position', '{x, y}'], ['inventory', 'unknown'], ['memory', 'string']] },
  'clients/ai.Decision': { stereotype: 'type', a: [['command', 'Command'], ['direction', 'Direction | null'], ['reason', 'string'], ['memory', 'string']] },
  'clients/ai.AgentRunner': { stereotype: 'module agent.ts', o: [['runAgent', 'Promise<State>'], ['initialState', 'State'], ['updateState', 'void']] },
  'clients/ai.CodexFactory': { stereotype: 'module brain.ts', o: [['createCodexBrain', 'Promise<Brain>']] },
  'clients/ai.ClaudeFactory': { stereotype: 'module claude-brain.ts', o: [['createClaudeBrain', 'Promise<Brain>']] },
};

const classes = {};
for (const [key, spec] of Object.entries(specs)) {
  const i = key.lastIndexOf('.');
  const pkg = packages[key.slice(0, i)];
  const name = key.slice(i + 1);
  const cls = {
    _type: 'UMLClass', _id: uid(`class:${key}`), _parent: ref(pkg), name,
    ...(spec.stereotype ? { stereotype: spec.stereotype } : {}),
    isAbstract: Boolean(spec.abstract), attributes: [], operations: [], ownedElements: [],
  };
  for (const [index, [attrName, type, visibility = 'public']] of (spec.a || []).entries()) {
    cls.attributes.push({ _type: 'UMLAttribute', _id: uid(`attr:${key}:${index}`), _parent: ref(cls), name: attrName, type, visibility });
  }
  for (const [index, [opName, returnType]] of (spec.o || []).entries()) {
    const op = { _type: 'UMLOperation', _id: uid(`op:${key}:${index}`), _parent: ref(cls), name: opName, visibility: 'public', parameters: [] };
    op.parameters.push({ _type: 'UMLParameter', _id: uid(`return:${key}:${index}`), _parent: ref(op), name: '', type: returnType, direction: 'return' });
    cls.operations.push(op);
  }
  classes[key] = cls;
  pkg.ownedElements.push(cls);
}

const relations = {};
function association(key, from, to, { kind = 'none', targetMultiplicity = '', sourceMultiplicity = '', name = '' } = {}) {
  const source = classes[from], target = classes[to];
  const rel = { _type: 'UMLAssociation', _id: uid(`rel:${key}`), _parent: ref(source), name,
    end1: { _type: 'UMLAssociationEnd', _id: uid(`end1:${key}`), _parent: ref(uid(`rel:${key}`)), reference: ref(source), aggregation: kind, multiplicity: sourceMultiplicity, navigable: false },
    end2: { _type: 'UMLAssociationEnd', _id: uid(`end2:${key}`), _parent: ref(uid(`rel:${key}`)), reference: ref(target), aggregation: 'none', multiplicity: targetMultiplicity, navigable: true },
  };
  source.ownedElements.push(rel);
  relations[key] = { model: rel, from, to };
}
function simpleRelation(key, type, from, to) {
  const source = classes[from], target = classes[to];
  const rel = { _type: type, _id: uid(`rel:${key}`), _parent: ref(source), source: ref(source), target: ref(target) };
  source.ownedElements.push(rel);
  relations[key] = { model: rel, from, to };
}

association('game-matrix', 'core.Game', 'core.Matrix', { kind: 'composite', targetMultiplicity: '1' });
association('game-agent', 'core.Game', 'core.Agent', { kind: 'composite', targetMultiplicity: '0..*', name: 'm_agents' });
association('matrix-cell', 'core.Matrix', 'core.Cell', { kind: 'composite', targetMultiplicity: '0..*' });
association('cell-agent', 'core.Cell', 'core.Agent', { sourceMultiplicity: '0..1', targetMultiplicity: '0..4', name: 'occupants' });
association('cell-resource', 'core.Cell', 'core.Resource', { kind: 'composite', targetMultiplicity: '0..*' });
association('agent-resource', 'core.Agent', 'core.Resource', { kind: 'composite', targetMultiplicity: '0..*', name: 'inventory' });
simpleRelation('fruit-resource', 'UMLGeneralization', 'core.Fruit', 'core.Resource');
association('server-game', 'network.Server', 'core.Game', { kind: 'composite', targetMultiplicity: '1' });
association('server-connection', 'network.Server', 'network.Connection', { kind: 'shared', targetMultiplicity: '0..*' });
association('client-connection', 'network.Client', 'network.Connection', { kind: 'composite', targetMultiplicity: '0..1' });
simpleRelation('server-protocol', 'UMLDependency', 'network.Server', 'network.Protocol');
simpleRelation('connection-protocol', 'UMLDependency', 'network.Connection', 'network.Protocol');
simpleRelation('client-protocol', 'UMLDependency', 'network.Client', 'network.Protocol');
association('renderer-matrixconnection', 'gui.Renderer', 'gui.MatrixConnection', { kind: 'composite', targetMultiplicity: '1' });
simpleRelation('matrixconnection-protocol', 'UMLDependency', 'gui.MatrixConnection', 'network.Protocol');
simpleRelation('claude-bot', 'UMLGeneralization', 'ai.Claude', 'ai.Bot');
simpleRelation('test-bot', 'UMLGeneralization', 'ai.Test', 'ai.Bot');
simpleRelation('bot-protocol', 'UMLDependency', 'ai.Bot', 'network.Protocol');
association('gameconnection-decoder', 'clients/ai.GameConnection', 'clients/ai.FrameDecoder', { kind: 'composite', targetMultiplicity: '1' });
association('journal-entry', 'clients/ai.Journal', 'clients/ai.HistoryEntry', { kind: 'composite', targetMultiplicity: '0..*' });
simpleRelation('runner-brain', 'UMLDependency', 'clients/ai.AgentRunner', 'clients/ai.Brain');
simpleRelation('runner-connection', 'UMLDependency', 'clients/ai.AgentRunner', 'clients/ai.GameConnection');
simpleRelation('runner-journal', 'UMLDependency', 'clients/ai.AgentRunner', 'clients/ai.Journal');
simpleRelation('runner-state', 'UMLDependency', 'clients/ai.AgentRunner', 'clients/ai.State');
simpleRelation('runner-decision', 'UMLDependency', 'clients/ai.AgentRunner', 'clients/ai.Decision');
simpleRelation('codex-brain', 'UMLDependency', 'clients/ai.CodexFactory', 'clients/ai.Brain');
simpleRelation('claude-brain', 'UMLDependency', 'clients/ai.ClaudeFactory', 'clients/ai.Brain');

function style(type, id, parent, modelRef, left, top, width, height, visible = true, extra = {}) {
  return { _type: type, _id: id, _parent: ref(parent), model: ref(modelRef), visible,
    enabled: true, lineColor: '#334155', fillColor: '#ffffff', fontColor: '#0f172a',
    font: 'Arial;12;0', parentStyle: true, left, top, width, height, autoResize: false, ...extra };
}

function classView(diagram, key, x, y, detail) {
  const cls = classes[key];
  const pkgName = key.slice(0, key.lastIndexOf('.'));
  const attrs = detail ? cls.attributes : [];
  const ops = detail ? cls.operations : [];
  const width = detail ? 350 : 190;
  const nameHeight = 43;
  const attrHeight = attrs.length ? 4 + attrs.length * 17 : 0;
  const opHeight = ops.length ? 4 + ops.length * 17 : 0;
  const height = nameHeight + attrHeight + opHeight + 6;
  const viewId = uid(`view:${diagram._id}:${key}`);
  const v = { _type: 'UMLClassView', _id: viewId, _parent: ref(diagram), model: ref(cls), subViews: [],
    visible: true, enabled: true, lineColor: '#334155', fillColor: colors[pkgName], fontColor: '#0f172a',
    font: 'Arial;12;0', parentStyle: false, showShadow: false,
    left: x, top: y, width, height, autoResize: false, stereotypeDisplay: 'label',
    showVisibility: true, showNamespace: true, showProperty: true, showType: true,
    wordWrap: false, suppressAttributes: !detail, suppressOperations: !detail,
    showMultiplicity: true, showOperationSignature: true };
  const nameId = uid(`name:${viewId}`);
  const nameComp = style('UMLNameCompartmentView', nameId, v, cls, x, y, width, nameHeight);
  nameComp.subViews = [];
  const labelSpecs = [
    ['stereotypeLabel', cls.stereotype ? `«${cls.stereotype}»` : '', Boolean(cls.stereotype), y + 1],
    ['nameLabel', cls.name, true, y + 13],
    ['namespaceLabel', `(from ${pkgName})`, true, y + 28],
    ['propertyLabel', '', false, y],
  ];
  for (const [role, value, visible, ly] of labelSpecs) {
    const label = style('LabelView', uid(`${role}:${viewId}`), nameComp, cls,
      x + 5, ly, width - 10, 14, visible,
      { text: value, font: role === 'nameLabel' ? 'Arial;12;1' : 'Arial;10;0', horizontalAlignment: 2, verticalAlignment: 5 });
    nameComp.subViews.push(label);
    nameComp[role] = ref(label);
  }
  v.subViews.push(nameComp);
  v.nameCompartment = ref(nameComp);

  const attrId = uid(`attrs:${viewId}`);
  const attrComp = style('UMLAttributeCompartmentView', attrId, v, cls, x, y + nameHeight, width, Math.max(attrHeight, 10), detail && attrs.length > 0);
  attrComp.subViews = [];
  for (const [i, attr] of attrs.entries()) {
    const symbol = attr.visibility === 'private' ? '-' : attr.visibility === 'protected' ? '#' : '+';
    const text = `${symbol}${attr.name}: ${attr.type}`;
    attrComp.subViews.push(style('UMLAttributeView', uid(`attrview:${viewId}:${i}`), attrComp, attr,
      x + 6, y + nameHeight + 2 + i * 17, width - 12, 16, true,
      { text, horizontalAlignment: 0, verticalAlignment: 5 }));
  }
  v.subViews.push(attrComp);
  v.attributeCompartment = ref(attrComp);

  const opId = uid(`ops:${viewId}`);
  const opComp = style('UMLOperationCompartmentView', opId, v, cls, x, y + nameHeight + attrHeight, width, Math.max(opHeight, 10), detail && ops.length > 0);
  opComp.subViews = [];
  for (const [i, op] of ops.entries()) {
    const type = op.parameters[0].type;
    opComp.subViews.push(style('UMLOperationView', uid(`opview:${viewId}:${i}`), opComp, op,
      x + 6, y + nameHeight + attrHeight + 2 + i * 17, width - 12, 16, true,
      { text: `+${op.name}(): ${type}`, horizontalAlignment: 0, verticalAlignment: 5 }));
  }
  v.subViews.push(opComp);
  v.operationCompartment = ref(opComp);

  const template = style('UMLTemplateParameterCompartmentView', uid(`template:${viewId}`), v, cls, x, y, 10, 10, false);
  v.subViews.push(template);
  v.templateParameterCompartment = ref(template);
  diagram.ownedViews.push(v);
  return v;
}

function edgeView(diagram, key, views) {
  const rel = relations[key];
  const tail = views[rel.from], head = views[rel.to];
  if (!tail || !head) throw new Error(`Relation ${key}: élément absent du diagramme`);
  const tc = [tail.left + tail.width / 2, tail.top + tail.height / 2];
  const hc = [head.left + head.width / 2, head.top + head.height / 2];
  let start, end;
  if (Math.abs(hc[0] - tc[0]) > Math.abs(hc[1] - tc[1])) {
    const dir = Math.sign(hc[0] - tc[0]) || 1;
    start = [tc[0] + dir * tail.width / 2, tc[1]];
    end = [hc[0] - dir * head.width / 2, hc[1]];
  } else {
    const dir = Math.sign(hc[1] - tc[1]) || 1;
    start = [tc[0], tc[1] + dir * tail.height / 2];
    end = [hc[0], hc[1] - dir * head.height / 2];
  }
  const edge = {
    _type: `${rel.model._type}View`, _id: uid(`edge:${diagram._id}:${key}`),
    _parent: ref(diagram), model: ref(rel.model), visible: true, enabled: true,
    lineColor: '#64748b', fillColor: '#ffffff', fontColor: '#0f172a', font: 'Arial;11;0',
    parentStyle: false, head: ref(head), tail: ref(tail), lineStyle: 0,
    points: `${Math.round(start[0])}:${Math.round(start[1])};${Math.round(end[0])}:${Math.round(end[1])}`,
  };
  diagram.ownedViews.push(edge);
}

function diagram(name, keys, relationKeys, { columns = 4, detail = false } = {}) {
  const d = { _type: 'UMLClassDiagram', _id: uid(`diagram:${name}`), _parent: ref(model), name,
    visible: true, defaultDiagram: name === 'Vue d’ensemble', ownedViews: [] };
  model.ownedElements.push(d);
  const views = {};
  const xStep = detail ? 475 : 330;
  const yStep = detail ? 350 : 190;
  for (const [index, key] of keys.entries()) {
    const x = 70 + (index % columns) * xStep;
    const y = 70 + Math.floor(index / columns) * yStep;
    views[key] = classView(d, key, x, y, detail);
  }
  for (const key of relationKeys) edgeView(d, key, views);
}

diagram('Vue d’ensemble', [
  'network.Server', 'network.Connection', 'network.Client', 'network.Protocol',
  'core.Game', 'core.Matrix', 'core.Cell', 'core.Agent', 'core.Resource',
  'core.Fruit', 'gui.Renderer', 'gui.MatrixConnection', 'gui.Agent', 'ai.Bot',
  'ai.Claude',
  'clients/ai.AgentRunner', 'clients/ai.Brain', 'clients/ai.GameConnection', 'clients/ai.Journal',
], [
  'game-matrix', 'game-agent', 'matrix-cell', 'cell-agent', 'cell-resource', 'fruit-resource',
  'server-game', 'server-connection', 'client-connection',
  'renderer-matrixconnection', 'claude-bot',
  'runner-brain', 'runner-connection', 'runner-journal',
], { columns: 5 });

diagram('Moteur C++', [
  'core.Game', 'core.Matrix', 'core.Cell', 'core.Agent', 'core.Resource', 'core.Fruit',
  'core.Position', 'core.LookResult', 'core.MatrixSnapshot', 'core.InventoryResource',
], [ 'game-matrix', 'game-agent', 'matrix-cell', 'cell-agent', 'cell-resource', 'agent-resource', 'fruit-resource' ], { columns: 3, detail: true });

diagram('Réseau C++', [
  'network.Server', 'network.Connection', 'network.Client',
  'network.Protocol', 'core.Game',
], [
  'server-connection', 'client-connection',
  'server-protocol', 'connection-protocol', 'client-protocol', 'server-game',
], { columns: 3, detail: true });

diagram('GUI et bots C++', [
  'gui.Renderer', 'gui.MatrixConnection', 'gui.Agent', 'ai.Bot', 'ai.Claude', 'ai.Test',
  'network.Protocol',
], [
  'renderer-matrixconnection', 'matrixconnection-protocol',
  'claude-bot', 'test-bot', 'bot-protocol',
], { columns: 3, detail: true });

diagram('Client IA TypeScript', [
  'clients/ai.AgentRunner', 'clients/ai.Brain', 'clients/ai.GameConnection',
  'clients/ai.Journal', 'clients/ai.FrameDecoder', 'clients/ai.HistoryEntry',
  'clients/ai.State', 'clients/ai.Decision', 'clients/ai.CodexFactory',
  'clients/ai.ClaudeFactory',
], [
  'runner-brain', 'runner-connection', 'runner-journal', 'runner-state', 'runner-decision',
  'gameconnection-decoder', 'journal-entry', 'codex-brain', 'claude-brain',
], { columns: 3, detail: true });

fs.writeFileSync(output, `${JSON.stringify(project, null, 2)}\n`, 'utf8');
console.log(output);
