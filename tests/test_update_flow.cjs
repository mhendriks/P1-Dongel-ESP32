// Verify that choosing an update is side-effect free until a button is pressed.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(`${__dirname}/../cdn/DSMRindex.js`, 'utf8');

function setup() {
  const events = [], timers = [], handlers = {}, nodes = {};
  const document = {
    activeElement: null,
    getElementById: id => nodes[id],
    querySelector: () => nodes.progress,
    addEventListener: (name, handler) => { handlers[name] = handler; },
    body: {appendChild() {}},
    createElement: () => ({click() { events.push(this.href); }, remove() {}})
  };
  const node = () => ({style: {}, hidden: false, focus() { document.activeElement = this; }});
  for (const id of ['updateTitle', 'updateChoices', 'updateCancel', 'updatePopup', 'progressBar', 'updateSpinner', 'updatestatus', 'progress']) {
    nodes[id] = {...node(), setAttribute() {}, removeAttribute() {}};
  }
  const buttons = [node(), node()];
  nodes.updateChoices.querySelectorAll = () => buttons;
  nodes.updateChoices.querySelector = () => buttons[0];
  const trigger = node();
  document.activeElement = trigger;
  const context = vm.createContext({
    document, t: key => key,
    closeUpdate: () => { nodes.updatePopup.style.visibility = 'hidden'; events.push('close'); },
    setTimeout: callback => timers.push(callback),
    RemoteUpdate: type => events.push(`update:${type}`)
  });
  vm.runInContext('let progressBar;', context);
  vm.runInContext(source.slice(source.indexOf('function showUpdateOverlay('), source.indexOf('let systemActionUrl')), context);
  vm.runInContext(source.slice(source.indexOf('const sleep ='), source.indexOf('function updateProgress()')), context);
  const key = (value, shiftKey = false) => {
    const event = {key: value, shiftKey, prevented: false, preventDefault() { this.prevented = true; }};
    handlers.keydown(event);
    return event;
  };
  return {context, document, nodes, buttons, trigger, events, timers, key};
}

(async () => {
  let app = setup();
  app.context.startUpdateFlow('beta');
  assert.equal(app.nodes.updateChoices.hidden, false);
  assert.equal(app.nodes.progress.style.display, 'none');
  assert.equal(app.nodes.updateTitle.innerText, 'tle-update-confirm');
  assert.deepEqual(app.events, []);
  assert.equal(app.document.activeElement, app.buttons[0]);
  assert.equal(app.nodes.updateCancel.hidden, false);
  app.nodes.updateCancel.focus();
  app.key('Tab', true);
  assert.equal(app.document.activeElement, app.buttons[1]);
  app.key('Tab');
  assert.equal(app.document.activeElement, app.nodes.updateCancel);
  app.key('Escape');
  assert.equal(app.nodes.updatePopup.style.visibility, 'hidden');
  assert.equal(app.document.activeElement, app.trigger);
  await app.context.confirmUpdateFlow();
  assert.deepEqual(app.events, [], 'cancel must prevent any update or download');

  app.context.startUpdateFlow('beta');
  await app.context.confirmUpdateFlow();
  assert.deepEqual(app.events, ['update:beta']);
  assert.equal(app.nodes.updateChoices.hidden, true);
  assert.equal(app.nodes.progress.style.display, 'block');
  assert.equal(app.nodes.updateTitle.innerText, 'lbl_update_title');
  await app.context.confirmUpdateFlow();
  assert.equal(app.events.length, 1, 'double click must not update twice');
  assert.equal(app.nodes.updateCancel.hidden, false);
  assert.equal(app.nodes.updateCancel.disabled, false, 'progress must always be dismissible');
  app.nodes.updateCancel.focus();
  assert.equal(app.key('Tab').prevented, true);
  app.context.cancelUpdateFlow();
  assert.equal(app.nodes.updatePopup.style.visibility, 'hidden');
  assert.deepEqual(app.events, ['update:beta', 'close']);
  app.context.showUpdateOverlay('running');
  app.key('Escape');
  assert.equal(app.nodes.updatePopup.style.visibility, 'hidden');
  assert.deepEqual(app.events, ['update:beta', 'close', 'close']);

  app = setup();
  app.context.startUpdateFlow('stable');
  const pending = app.context.downloadUpdateData();
  assert.ok(app.buttons.every(button => button.disabled));
  assert.equal(app.nodes.updatestatus.innerText, 'txt-update-download-start');
  assert.deepEqual(app.events, ['/RNGdays.json']);
  await app.context.downloadUpdateData();
  app.context.confirmUpdateFlow();
  app.context.cancelUpdateFlow();
  assert.equal(app.nodes.updatePopup.style.visibility, 'visible');
  for (let i = 0; i < 3; i++) {
    assert.equal(app.timers.length, 1);
    app.timers.shift()();
    await Promise.resolve();
    await Promise.resolve();
  }
  await pending;
  assert.deepEqual(app.events, ['/RNGdays.json', '/RNGhours.json', '/RNGmonths.json'], 'downloads must never start an update automatically');
  assert.equal(app.nodes.updateChoices.hidden, false);
  assert.equal(app.nodes.updateCancel.hidden, false);
  assert.equal(app.nodes.updatestatus.innerText, 'txt-update-download-check');
  assert.ok(app.buttons.every(button => !button.disabled));
  app.context.confirmUpdateFlow();
  assert.deepEqual(app.events, ['/RNGdays.json', '/RNGhours.json', '/RNGmonths.json', 'update:stable']);

  app = setup();
  app.context.downloadFilesSequential = async () => { throw new Error('download failed'); };
  app.context.startUpdateFlow('beta');
  await app.context.downloadUpdateData();
  assert.deepEqual(app.events, []);
  assert.equal(app.nodes.updatestatus.innerText, 'txt-update-download-error');
  assert.ok(app.buttons.every(button => !button.disabled));
  assert.equal(app.nodes.updateChoices.hidden, false);
  await app.context.confirmUpdateFlow();
  assert.deepEqual(app.events, ['update:beta'], 'retry without downloads must remain possible');
  console.log('Update flow: cancel, progress close button/Escape, keyboard, direct update, separate downloads, double clicks and retry passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
