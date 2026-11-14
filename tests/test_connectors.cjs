const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(`${__dirname}/../cdn/DSMRindex.js`, 'utf8');
const functions = source.slice(source.indexOf('function connectorSettingPane('), source.indexOf('function markDirty('));
const settings = {
  battery_power_scale: {value:1}, pv_modbus_enabled:false,
  pv_energy_type:{value:2}, mqtt_interval:{value:10}
};
const inputs = [
  {id:'setFld_pv_modbus_enabled',type:'checkbox',checked:true},
  {id:'setFld_battery_power_scale',type:'number',value:'0.1'},
  {id:'setFld_pv_energy_type',type:'select-one',value:'4'}
].map(input => ({...input, reportValidity:()=>true}));
const status = {}, button = {}, requests = [];
let rowIsDirty = true;
const dirtyRow = {classList:{remove:()=>{rowIsDirty=false;}}};
let fail = false, refreshed = 0, activeRequests = 0;
const context = vm.createContext({
  console, URL_DEVICE_SETTINGS:'/api/v2/dev/settings', t:key=>key,
  objDAL:{getDeviceSettings:()=>settings, ensureDeviceInformation:()=>refreshed++},
  document:{
    getElementById:id=>id==='connector_save' ? button : id==='connector_save_status' ? status : null,
    querySelectorAll:selector=>selector.includes('[id^="setFld_"') ? inputs : selector === "#Connectors .is-dirty" && rowIsDirty ? [dirtyRow] : []
  },
  fetch:async (_url, options)=>{
    assert.equal(activeRequests++,0,'writes must be sequential');
    await Promise.resolve();
    activeRequests--;
    const request = JSON.parse(options.body);
    requests.push(request);
    return {ok:!fail,status:500,json:async()=>request};
  }
});
vm.runInContext(functions, context);
assert.equal(context.connectorSettingPane('victron_accu_ip'),'connector_battery_connection');
assert.equal(context.connectorSettingPane('battery_discharging_code'),'connector_battery_mapping');
assert.equal(context.connectorSettingPane('pv_modbus_ip'),'connector_pv_connection');
assert.equal(context.connectorSettingPane('pv_energy_register'),'connector_pv_mapping');
assert.equal(context.connectorSettingPane('mb_map'),null);
(async()=>{
  await context.saveConnectorSettings();
  assert.deepEqual(requests.map(r=>r.name),['battery_power_scale','pv_energy_type','pv_modbus_enabled']);
  assert.equal(requests[0].value,'0.1');
  assert.equal(requests[1].value,'4');
  assert.equal(requests[2].value,true);
  assert.equal(status.textContent,'connector-saved');
  assert.equal(button.disabled,false);
  assert.equal(refreshed,1);
  assert.equal(rowIsDirty,false);
  rowIsDirty=true;
  requests.length=0;
  fail=true;
  await context.saveConnectorSettings();
  assert.equal(requests.length,1,'stop on first failure');
  assert.equal(status.textContent,'connector-save-error: HTTP 500');
  assert.equal(button.disabled,false);
  assert.equal(refreshed,1,'do not replace edits after failure');
  assert.equal(rowIsDirty,true,'failed saves must retain unsaved edits');
  inputs[0].reportValidity=()=>false;
  requests.length=0;
  await context.saveConnectorSettings();
  assert.equal(requests.length,0,'invalid input must not be sent');
  inputs[0].reportValidity=()=>true;
  fail=false;
  settings.battery_power_scale.value='0.1';
  settings.pv_modbus_enabled=true;
  settings.pv_energy_type.value='4';
  await context.saveConnectorSettings();
  assert.equal(requests.length,0,'restoring saved preset values needs no write');
  assert.equal(rowIsDirty,false,'a successful empty save must clear dirty state');
  console.log('Connector routing, sequential saving, validation and failure handling passed');
})().catch(error=>{console.error(error);process.exitCode=1;});

// Moving mapper controls must preserve row IDs/listeners and remain idempotent
// when settings arrive again; this is where the original branch lost fields.
{
  const byId = new Map();
  class Element {
    constructor(tag = 'div', id = '') {
      this.tagName = tag; this.children = []; this.parent = null;
      this.className = ''; this.dataset = {}; this.id = id;
      if (id) byId.set(id, this);
    }
    appendChild(child) {
      if (child.parent) child.parent.children.splice(child.parent.children.indexOf(child), 1);
      this.children.push(child); child.parent = this;
      return child;
    }
    querySelector(selector) {
      const className = selector.substring(1);
      for (const child of this.children) {
        if (child.className.split(' ').includes(className)) return child;
        const found = child.querySelector(selector);
        if (found) return found;
      }
      return null;
    }
  }
  const fields = [{key:'battery_power',label:'power'}, {key:'battery_soc',label:'soc'}, {key:'battery_state',label:'state'}];
  const container = new Element('div','connector_battery_mapping');
  const controls = [];
  for (const field of fields) {
    for (const suffix of ['register','type','scale','swap']) {
      const row = new Element('div',`settingR_${field.key}_${suffix}`);
      row.className = 'settingDiv';
      const control = new Element('input',`setFld_${field.key}_${suffix}`);
      control.listener = Symbol('original listener'); controls.push(control);
      row.appendChild(control); container.appendChild(row);
    }
  }
  const extra = new Element('div','settingR_battery_idle_code');
  container.appendChild(extra);
  const mapperContext = vm.createContext({
    t:key=>key,
    document:{getElementById:id=>byId.get(id),createElement:tag=>new Element(tag)}
  });
  vm.runInContext(functions, mapperContext);
  mapperContext.buildConnectorMapper('connector_battery_mapping',fields,['battery_idle_code']);
  const table = container.querySelector('.mapper-matrix');
  assert.ok(table);
  assert.equal(table.children[1].children.length,3);
  assert.equal(table.children[0].children[0].children.length,6);
  assert.equal(table.children[1].children[0].children.length,6);
  for (const control of controls) {
    assert.equal(byId.get(control.id),control);
    assert.ok(control.listener);
    assert.equal(control.parent.parent.tagName,'td');
  }
  assert.equal(extra.parent.className,'mapper-state-codes');
  mapperContext.buildConnectorMapper('connector_battery_mapping',fields,['battery_idle_code']);
  assert.equal(container.querySelector('.mapper-matrix'),table);
  assert.equal(table.children[1].children.length,3);
  console.log('Mapper keeps controls and rows intact across repeated refreshes');
}

// Presets only change the form. Choosing a preset must never activate it or
// overwrite the customer's network address, interval, enabled state or Wp.
{
  const nodes = new Map(), dirty = new Set(), rootEvents = {};
  const makeControl = (value = '') => ({value, checked:false, disabled:false, dataset:{}, events:{},
    addEventListener(name, handler) { this.events[name] = handler; }});
  const presetContext = vm.createContext({
    t:key=>key,
    document:{getElementById:id=>nodes.get(id)},
    fetch:()=>{throw new Error('Applying a preset must not send a request');}
  });
  vm.runInContext(functions, presetContext);
  vm.runInContext(source.slice(source.indexOf('function markDirty('),source.indexOf('function clearDirtySettings(')),presetContext);
  nodes.set('Connectors',{dataset:{},addEventListener:(event,handler)=>{rootEvents[event]=handler;}});
  nodes.set('connector_save_status',{});
  for (const kind of ['battery','pv']) {
    const preset = presetContext.connectorPreset(kind);
    nodes.set(`connector_${kind}_preset`,makeControl('custom'));
    nodes.set(`connector_${kind}_apply_preset`,makeControl());
    for (const [key,value] of Object.entries(preset.values)) {
      const input = makeControl('999');
      input.id = `setFld_${key}`;
      input.checked = typeof value === 'boolean';
      input.closest = ()=>({classList:{add:()=>dirty.add(key)}});
      nodes.set(input.id,input);
    }
  }
  const preserved = {
    victron_accu_ip:'192.168.1.12',victron_accu_enabled:true,victron_accu_interval:37,
    pv_modbus_ip:'192.168.1.13',pv_modbus_enabled:true,pv_modbus_interval:47,pv_modbus_wp:9000
  };
  for (const [key,value] of Object.entries(preserved)) {
    const input = makeControl(String(value)); input.checked = value === true;
    nodes.set(`setFld_${key}`,input);
  }
  presetContext.initConnectorPresets();
  assert.equal(dirty.size,0,'opening the page must not apply defaults');
  for (const kind of ['battery','pv']) {
    const preset = presetContext.connectorPreset(kind);
    const select = nodes.get(`connector_${kind}_preset`);
    const button = nodes.get(`connector_${kind}_apply_preset`);
    assert.equal(select.value,'custom');
    assert.equal(button.disabled,true);
    const dirtyBeforeSelection=dirty.size;
    select.value=preset.id;
    select.events.change();
    assert.equal(dirty.size,dirtyBeforeSelection,'selection alone must not edit fields');
    assert.equal(button.disabled,false);
    button.events.click();
    for (const [key,value] of Object.entries(preset.values)) {
      const input = nodes.get(`setFld_${key}`);
      assert.equal(typeof value==='boolean' ? input.checked : Number(input.value),value,key);
    }
    assert.equal(select.value,preset.id);
    const key=kind==='battery' ? 'battery_power_register' : 'pv_power_register';
    const input=nodes.get(`setFld_${key}`);
    input.value='123';
    rootEvents.input({target:input});
    assert.equal(select.value,'custom','manual mapping edits should select Custom');
    assert.equal(button.disabled,true);
    select.events.change();
    assert.equal(input.value,'123','Custom must keep manual values');
  }
  for (const [key,value] of Object.entries(preserved)) {
    const input=nodes.get(`setFld_${key}`);
    assert.equal(typeof value==='boolean' ? input.checked : input.value,typeof value==='boolean' ? value : String(value),key);
    assert.equal(dirty.has(key),false);
  }
  assert.equal(presetContext.connectorPresetFieldMatches({value:'1.0'},1),true);
  assert.equal(presetContext.connectorPresetFieldMatches({value:''},0),false);
  console.log('Battery/PV presets preserve connection settings and never send requests before Save');
}
