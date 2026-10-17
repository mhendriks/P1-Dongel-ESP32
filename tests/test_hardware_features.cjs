// Exercise the Systeminfo presentation with API responses from each interface layout.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(`${__dirname}/../cdn/DSMRindex.js`, 'utf8');
const translations = JSON.parse(fs.readFileSync(`${__dirname}/../cdn/lang/nl.json`, 'utf8'));
const context = vm.createContext({
  t: key => translations[key] || key,
  td: key => translations[`dict_${key}`] || key,
  addDeviceInfoRow: (rows, label, value) => rows.push([label, value])
});
vm.runInContext(source.slice(source.indexOf('function deviceHardwareRevision('), source.indexOf('function renderDeviceInformation(')), context);
function rows(info) {
  const result = [];
  context.addDeviceHardwareRows(result, info);
  return result;
}
assert.deepEqual(rows({hardware: 'W1MC', hardware_revision: 102, hardware_features: ['HAN-NVE'], hardware_slots: ['H2O']}), [
  [translations.dict_hardware, 'W1MC V1.02'], ['Vaste aansluitingen', 'HAN-NVE'], ['Slot 1', 'H2O']
]);
assert.deepEqual(rows({hardware: 'W1MC', hardware_revision: 200, hardware_features: [], hardware_slots: ['NONE']}), [
  [translations.dict_hardware, 'W1MC V2.00'], ['Slot 1', 'Leeg']
]);
assert.deepEqual(rows({hardware: 'W1MC', hardware_revision: 301, hardware_features: ['RS485', 'HAN-NVE'], hardware_slots: []}), [
  [translations.dict_hardware, 'W1MC V3.01'], ['Vaste aansluitingen', 'RS485, HAN-NVE']
]);
assert.deepEqual(rows({hardware: 'D1MC', hardware_revision: 100, hardware_features: ['RS485', 'HAN-NVE', 'Linky TIC'], hardware_slots: []}), [
  [translations.dict_hardware, 'D1MC'], ['Hardwarerevisie', 'V1.00'], ['Vaste aansluitingen', 'RS485, HAN-NVE, Linky TIC']
]);
assert.deepEqual(rows({hardware: 'P1UX2', hardware_features: [], hardware_slots: ['RS485', 'UNKNOWN']}), [
  [translations.dict_hardware, 'P1UX2'], ['Slot 1', 'RS485'], ['Slot 2', 'Onbekend']
]);
// Cached/older firmware responses still render without synthetic interfaces or revisions.
assert.deepEqual(rows({hardware: 'NRGD'}), [[translations.dict_hardware, 'NRGD']]);
assert.equal(context.deviceHardwareLabel({hardware: 'W1MC', hardware_revision: 0}), 'W1MC V1');
console.log('Systeminfo hardware presentation passed');
