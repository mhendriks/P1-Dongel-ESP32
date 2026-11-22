const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(`${__dirname}/../cdn/DSMRindex.js`, 'utf8');
const helpers = source.slice(source.indexOf('function solarConnectorConfig('), source.indexOf('function initConnectorUI('));
const html = fs.readFileSync(`${__dirname}/../cdn/DSMRindex_body.html`, 'utf8');
const forms = ['enphase', 'sma', 'omniksol'].map(solarSource => {
  const classes = new Set();
  const fields = Object.fromEntries(['enabled', 'gateway-url', 'token', 'wp', 'refresh-interval'].map(name =>
    [name, {type: name === 'enabled' ? 'checkbox' : 'text', value: '', checked: false}]));
  const fieldset = {disabled: true};
  return {
    dataset: {solarSource}, fields, fieldset, listeners: {},
    elements: {namedItem: name => fields[name]},
    querySelector: () => fieldset,
    addEventListener(name, callback) { this.listeners[name] = callback; },
    classList: {contains: name => classes.has(name), add: name => classes.add(name), remove: name => classes.delete(name)}
  };
});
const config = (overrides = {}) => ({enabled: true, 'gateway-url': 'https://gateway/production', token: 'saved-secret', wp: 3500, 'refresh-interval': 60, ...overrides});
const saved = Object.fromEntries(forms.map(form => [form.dataset.solarSource, config()]));
let failSource, onFetch;
const requests = [], status = {};
const context = vm.createContext({
  document: {querySelectorAll: () => forms, getElementById: () => status},
  fetch: async (url, options) => {
    const brand = url.split('/').pop();
    requests.push({brand, options});
    if (onFetch) { const callback = onFetch; onFetch = null; callback(); }
    if (brand === failSource) return {ok: false, status: 500};
    if (options.method === 'POST') saved[brand] = JSON.parse(options.body);
    return {ok: true, json: async () => saved[brand]};
  }
});
vm.runInContext(helpers, context);
(async () => {
  for (const form of forms) assert.ok(html.includes(`data-solar-source="${form.dataset.solarSource}"`));
  assert.ok(!html.includes('id="Solar"'));
  assert.ok(!source.includes('monitoringapi.solaredge.com'));
  await context.loadSolarConnectors();
  assert.equal(requests.length, 3);
  for (const form of forms) {
    assert.equal(form.fieldset.disabled, false);
    assert.equal(form.fields.token.value, 'saved-secret');
    assert.equal(form.fields.enabled.checked, true);
  }
  const form = forms[0];
  form.fields.token.value = 'new-secret'; form.listeners.input();
  await context.loadSolarConnectors();
  assert.equal(form.fields.token.value, 'new-secret', 'refresh must preserve unsaved credentials');
  requests.length = 0;
  await context.saveSolarConnectors();
  assert.deepEqual(requests.map(r => r.brand), ['enphase'], 'only edited brands should be saved');
  assert.equal(saved.enphase.token, 'new-secret');
  assert.equal(form.classList.contains('is-dirty'), false);
  form.fields.enabled.checked = false; form.listeners.change();
  failSource = 'enphase';
  await assert.rejects(context.saveSolarConnectors(), /HTTP 500/);
  assert.equal(form.classList.contains('is-dirty'), true);
  assert.equal(saved.enphase.enabled, true, 'failed save must retain server config');
  failSource = undefined;
  await context.saveSolarConnectors();
  assert.equal(saved.enphase.enabled, false);
  assert.equal(saved.enphase.token, 'new-secret', 'disable must preserve credentials');
  form.fields.wp.value = '4000'; form.listeners.input();
  onFetch = () => { form.fields.wp.value = '5000'; form.listeners.input(); };
  await context.saveSolarConnectors();
  assert.equal(saved.enphase.wp, 4000);
  assert.equal(form.fields.wp.value, '5000');
  assert.equal(form.classList.contains('is-dirty'), true, 'typing during save must remain unsaved');
  // Undo uses the last server-confirmed configuration.
  context.fillSolarConnector(form, JSON.parse(form.dataset.saved));
  assert.equal(form.fields.wp.value, 4000);
  failSource = 'sma'; forms[1].fieldset.disabled = true;
  await context.loadSolarConnectors();
  assert.equal(forms[1].fieldset.disabled, true, 'failed initial load must prevent accidental overwrite');
  assert.match(status.textContent, /sma: HTTP 500/);
  console.log('HTTP PV connectors: existing configs, independent save, disable, Undo, errors and request races passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
