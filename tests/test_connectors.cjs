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
let fail = false, refreshed = 0, activeRequests = 0;
const context = vm.createContext({
  console, URL_DEVICE_SETTINGS:'/api/v2/dev/settings', t:key=>key,
  objDAL:{getDeviceSettings:()=>settings, ensureDeviceInformation:()=>refreshed++},
  document:{
    getElementById:id=>id==='connector_save' ? button : id==='connector_save_status' ? status : null,
    querySelectorAll:selector=>selector.includes('[id^="setFld_"') ? inputs : []
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
  requests.length=0;
  fail=true;
  await context.saveConnectorSettings();
  assert.equal(requests.length,1,'stop on first failure');
  assert.equal(status.textContent,'connector-save-error: HTTP 500');
  assert.equal(button.disabled,false);
  assert.equal(refreshed,1,'do not replace edits after failure');
  inputs[0].reportValidity=()=>false;
  requests.length=0;
  await context.saveConnectorSettings();
  assert.equal(requests.length,0,'invalid input must not be sent');
  console.log('Connector routing, sequential saving, validation and failure handling passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
