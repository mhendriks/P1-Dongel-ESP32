// Exercise opt-in capture, polling lifecycle, rendering, and HTTP clipboard fallbacks.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(`${__dirname}/../cdn/DSMRindex.js`, 'utf8');
const translations = JSON.parse(fs.readFileSync(`${__dirname}/../cdn/lang/nl.json`, 'utf8'));
const functions = source.slice(source.indexOf('let connectorDiagnosticsTimer ='), source.indexOf('function connectorSettingPane('));
class Element {
  constructor(tag = 'div') { this.tag = tag; this.children = []; this.hidden = true; this.textContent = ''; }
  append(...children) { this.children.push(...children); }
  appendChild(child) { this.append(child); }
  replaceChildren() { this.children = []; }
  focus() { this.focused = true; }
  select() { this.selected = true; }
}
function setup() {
  const nodes = Object.fromEntries(['enabled', 'status', 'results', 'copy', 'copy_text'].map(id => [`connector_diagnostics_${id}`, new Element()]));
  const requests = [], timers = new Map();
  let nextTimer = 0, snapshot = {enabled: false, battery: {status: 'off'}, pv: {status: 'off'}};
  const document = {getElementById: id => nodes[id], createElement: tag => new Element(tag), execCommand: () => false};
  const context = vm.createContext({
    activeTab: 'bConnectors', document, navigator: {}, t: key => translations[key] || key,
    setTimeout(fn) { timers.set(++nextTimer, fn); return nextTimer; },
    clearTimeout(id) { timers.delete(id); },
    fetch: async (url, options) => {
      requests.push({url, ...options});
      return {ok: true, json: async () => snapshot};
    }
  });
  vm.runInContext(functions, context);
  return {context, nodes, document, requests, timers, setSnapshot: value => { snapshot = value; }};
}
const example = {
  enabled: true, firmware: '5.11.0', uptime_ms: 20000,
  battery: {status: 'disabled', requests: 0, successful_cycles: 0, failures: 0, recent: []},
  pv: {
    status: 'error', connection: {enabled: true, ip: '192.168.1.2', port: 1502, unit_id: 1, interval_s: 5},
    last_success_age_ms: 7000, requests: 5, successful_cycles: 1, failures: 1,
    last_error: {age_ms: 2000, register: 40083, code: 224, message: 'Timeout'},
    mapping: [{register: 40083, type: 1, multiplier: 1, word_swap: false, sf_register: 40084}],
    recent: [
      {age_ms: 2000, register: 40083, field: 0, error_code: 224, error: 'Timeout'},
      {age_ms: 7000, register: 40084, field: 0, is_sf: true, raw: [65535], exponent: -1, value: 123.4}
    ]
  }
};
function allText(element) {
  return [element.textContent, ...element.children.map(allText)].join(' ');
}
(async () => {
  let app = setup();
  await app.context.refreshConnectorDiagnostics(0);
  assert.equal(app.requests.length, 1);
  assert.equal(app.requests[0].method, undefined, 'opening the page must not enable capture');
  assert.equal(app.nodes.connector_diagnostics_enabled.checked, false);
  assert.equal(app.nodes.connector_diagnostics_results.hidden, true);
  assert.equal(app.nodes.connector_diagnostics_copy.disabled, true);
  assert.equal(app.timers.size, 0, 'disabled diagnostics must not keep polling');

  app.setSnapshot(example);
  await app.context.setConnectorDiagnostics(true);
  assert.deepEqual(JSON.parse(app.requests[1].body), {enabled: true});
  assert.equal(app.requests[1].method, 'POST');
  assert.equal(app.nodes.connector_diagnostics_results.hidden, false);
  assert.equal(app.nodes.connector_diagnostics_copy.disabled, false);
  assert.equal(app.nodes.connector_diagnostics_enabled.disabled, false);
  assert.equal(app.timers.size, 1);
  const visible = allText(app.nodes.connector_diagnostics_results);
  for (const text of ['Uitleesfout', 'Timeout', '40083', '0xE0', '0xFFFF', '-1', '123.4', '192.168.1.2:1502']) assert.ok(visible.includes(text), text);

  let copied;
  app.context.navigator.clipboard = {writeText: async report => { copied = JSON.parse(report); }};
  await app.context.copyConnectorDiagnostics();
  assert.equal(copied.firmware, '5.11.0');
  assert.equal(copied.pv.mapping[0].sf_register, 40084);
  assert.ok(copied.captured_at);
  assert.equal(app.nodes.connector_diagnostics_status.textContent, translations['connector-diag-copied']);

  // Clipboard rejection on HTTP must offer a usable fallback, including manual copying.
  app.context.navigator.clipboard = {writeText: async () => { throw new Error('Not allowed'); }};
  await app.context.copyConnectorDiagnostics();
  const manual = app.nodes.connector_diagnostics_copy_text;
  assert.equal(manual.hidden, false);
  assert.equal(manual.selected, true);
  assert.equal(JSON.parse(manual.value).pv.last_error.code, 224);
  assert.equal(app.nodes.connector_diagnostics_status.textContent, translations['connector-diag-copy-manually']);
  app.context.renderConnectorDiagnostics(example);
  assert.equal(manual.hidden, false, 'polling must not hide the manual copy fallback');
  delete app.context.navigator.clipboard;
  app.document.execCommand = command => command === 'copy';
  await app.context.copyConnectorDiagnostics();
  assert.equal(manual.hidden, true);
  assert.equal(app.nodes.connector_diagnostics_status.textContent, translations['connector-diag-copied']);

  app.context.stopConnectorDiagnosticsPolling();
  assert.equal(app.timers.size, 0);
  app.context.activeTab = 'bDashTab';
  const before = app.nodes.connector_diagnostics_results;
  await app.context.refreshConnectorDiagnostics(0);
  assert.equal(app.nodes.connector_diagnostics_results, before);
  assert.equal(app.timers.size, 0, 'leaving Connectors must stop polling');

  app.context.activeTab = 'bConnectors';
  app.setSnapshot({enabled: false});
  await app.context.setConnectorDiagnostics(false);
  assert.deepEqual(JSON.parse(app.requests.at(-1).body), {enabled: false});
  assert.equal(app.nodes.connector_diagnostics_results.hidden, true);
  assert.equal(app.nodes.connector_diagnostics_results.children.length, 0);
  assert.equal(app.nodes.connector_diagnostics_copy.disabled, true);
  assert.equal(app.timers.size, 0);

  app = setup();
  app.context.fetch = async () => ({ok: false, status: 404});
  await app.context.setConnectorDiagnostics(true);
  assert.equal(app.nodes.connector_diagnostics_enabled.checked, false);
  assert.equal(app.nodes.connector_diagnostics_enabled.disabled, false);
  assert.match(app.nodes.connector_diagnostics_status.textContent, /HTTP 404/);
  assert.equal(app.timers.size, 0);

  // An old GET arriving after enabling capture must not turn the switch back off.
  app = setup();
  let finishOldRequest;
  app.context.fetch = () => new Promise(resolve => { finishOldRequest = resolve; });
  const oldRequest = app.context.refreshConnectorDiagnostics(0);
  app.context.fetch = async () => ({ok: true, json: async () => example});
  await app.context.setConnectorDiagnostics(true);
  finishOldRequest({ok: true, json: async () => ({enabled: false})});
  await oldRequest;
  assert.equal(app.nodes.connector_diagnostics_enabled.checked, true);
  assert.equal(app.timers.size, 1);
  console.log('Connector diagnostics: opt-in, rendering, saved export, HTTP copy fallbacks, disabling, navigation, old firmware and request races passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
