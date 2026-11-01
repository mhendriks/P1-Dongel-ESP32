// Exercise startup, menu/widget links and browser history with the real page IDs.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(`${__dirname}/../cdn/DSMRindex.js`, 'utf8');
const html = fs.readFileSync(`${__dirname}/../cdn/DSMRindex_body.html`, 'utf8');

function classes(value = '') {
  const values = new Set(value.split(/\s+/));
  return {
    add: value => values.add(value), remove: value => values.delete(value),
    contains: value => values.has(value)
  };
}

function setup(hash = '') {
  const opened = [], pushed = [], commands = [], handlers = {};
  let displayWrites = 0;
  const sections = [...html.matchAll(/<section\s+id="([^"]+)"\s+class="tabName\b/g)].map(match => ({
    id: match[1], matches: selector => selector === 'section.tabName',
    style: new Proxy({}, {set(target, key, value) {
      displayWrites++;
      target[key] = value;
      return true;
    }})
  }));
  const rootMenu = {id: 'myTopnav', className: 'main-navigation'};
  const makeLink = (href, id = '') => ({
    id, target: '', classList: classes(),
    getAttribute: name => name === 'href' ? href : null,
    hasAttribute: () => false,
    closest: selector => selector === 'ul' ? rootMenu : null,
    click() { throw new Error('Startup must never simulate a link click'); }
  });
  const links = [...html.matchAll(/<a\b[^>]*>/g)].filter(match => /\bnav-item\b/.test(match[0])).map(match => {
    const href = match[0].match(/href=["']([^"']*)["']/)?.[1] || '';
    const id = match[0].match(/id=["']([^"']*)["']/)?.[1] || '';
    return makeLink(href, id);
  });
  const ui = {
    myTopnav: rootMenu, menuid: {classList: classes('mdi-menu')},
    languageSelector: {}, innerDash: {addEventListener() {}}, dongle_io: {addEventListener() {}}
  };
  ui['inner-dash'] = ui.innerDash;
  const location = {hash};
  const context = vm.createContext({
    console: {log() {}}, location, URLSearchParams,
    window: {
      addEventListener(name, fn) { handlers[name] = fn; },
      history: {pushState(_state, _title, href) { pushed.push(href); location.hash = href; }}
    },
    document: {
      body: {classList: classes()},
      getElementById: id => sections.find(s => s.id === id) || ui[id] || links.find(link => link.id === id),
      querySelectorAll(selector) {
        if (selector === 'section.tabName') return sections;
        if (selector === '#myTopnav .active') return links.filter(link => link.classList.contains('active'));
        if (selector === '#myTopnav a.nav-item') return links;
        throw new Error(`Unexpected selector: ${selector}`);
      },
      addEventListener(name, fn) { handlers[name] = fn; }
    },
    localStorage: {getItem() { return null; }},
    activeTab: 'bDashTab', presentationType: 'TAB',
    loadTranslations() {}, loadDashboardPreferences() {}, createDashboardGauges() {},
    handleDashboardWidgetClick() {}, NTSWupdateVisibility() {}, initDAL() {}, updateFromDAL() {},
    setMonthTableType() {}, initActualGraph() {}, setPresentationType() {}, BurnupBootstrap() {},
    UpdateStart: param => commands.push(['update', param]),
    handleRedirect: () => commands.push(['redirect']),
    openTab() {
      opened.push(context.activeTab);
      if (context.activeTab === 'bSettings') context.activeTab = 'bEditSettings';
    }
  });
  vm.runInContext(source.slice(source.indexOf('let tabNavigationReady ='), source.indexOf('function applyShowCols(')), context);
  vm.runInContext(source.slice(source.indexOf('function handle_menu_click()'), source.indexOf('//====== I18N.JS')), context);
  vm.runInContext(source.slice(source.indexOf('function bootsTrapMain()'), source.indexOf('} // bootsTrapMain()') + 1), context);
  function expectVisible(id) {
    assert.deepEqual(sections.filter(s => s.style.display === 'block').map(s => s.id), [id]);
  }
  function click(href, overrides = {}, linkOverrides = {}) {
    const link = Object.assign(makeLink(href), linkOverrides);
    const event = {
      button: 0, target: {closest: () => link}, defaultPrevented: false,
      preventDefault() { this.defaultPrevented = true; }, ...overrides
    };
    handlers.click(event);
    return event;
  }
  return {context, sections, opened, pushed, commands, handlers, ui, click, expectVisible, writes: () => displayWrites};
}

// Safari can emit popstate on initial load; it must not initialize a tab early.
for (const hash of ['', '#ActualTab', '#Settings', '#APIdocTab', '#missing']) {
  const app = setup(hash);
  app.handlers.popstate();
  assert.equal(app.opened.length, 0);
  app.context.bootsTrapMain();
  const id = hash === '' || hash === '#missing' ? 'DashTab' : hash.slice(1);
  app.expectVisible(id);
  assert.deepEqual(app.opened, ['b' + id]);
  assert.deepEqual(app.pushed, [], 'startup must not navigate or add history');
  app.handlers.popstate();
  app.handlers.hashchange();
  assert.equal(app.opened.length, 1);
}

const app = setup();
app.context.bootsTrapMain();
for (const section of app.sections.filter(s => !['DashTab', 'Redirect', 'Logout'].includes(s.id))) {
  const before = app.opened.length;
  assert.equal(app.click('#' + section.id).defaultPrevented, true);
  app.expectVisible(section.id);
  assert.equal(app.opened.length, before + 1);
  const writes = app.writes();
  app.handlers.popstate();
  app.handlers.hashchange();
  app.click('#' + section.id);
  assert.equal(app.opened.length, before + 1, 'same route/events must not restart polling');
  assert.equal(app.writes(), writes, 'unchanged routes must not rewrite visibility');
}

// Back and forward may each produce both popstate and hashchange.
for (const hash of ['#Settings', '#ActualTab', '', '#ActualTab']) {
  const before = app.opened.length;
  app.context.location.hash = hash;
  app.handlers.popstate();
  app.handlers.hashchange();
  app.expectVisible(hash.slice(1) || 'DashTab');
  assert.equal(app.opened.length, before + 1);
}
app.context.location.hash = '#Settings';
app.handlers.popstate();
assert.equal(app.context.activeTab, 'bEditSettings');
app.click('#Settings');
assert.equal(app.context.activeTab, 'bEditSettings', 'same-tab clicks preserve settings edit state');

// Modified clicks, downloads, external links and canceled widget clicks retain native behavior.
const before = app.opened.length;
for (const overrides of [{ctrlKey: true}, {metaKey: true}, {shiftKey: true}, {altKey: true}, {button: 1}, {defaultPrevented: true}]) {
  const event = app.click('#ActualTab', overrides);
  if (!overrides.defaultPrevented) assert.equal(event.defaultPrevented, false);
}
for (const href of ['#', '#no-such-section', '/ReBoot', 'https://example.org/']) {
  assert.equal(app.click(href).defaultPrevented, false);
}
assert.equal(app.click('#ActualTab', {}, {target: '_blank'}).defaultPrevented, false);
assert.equal(app.click('#ActualTab', {}, {hasAttribute: name => name === 'download'}).defaultPrevented, false);
assert.equal(app.opened.length, before);

for (const [hash, command] of [['#Redirect?msg=Update', ['redirect']], ['#UpdateStart?version=test', ['update', 'version=test']]]) {
  const special = setup(hash);
  special.context.bootsTrapMain();
  assert.deepEqual(special.commands, [command]);
  assert.equal(special.opened.length, 0);
}
assert.doesNotMatch(html.match(/<a\b[^>]*id='Home'[^>]*>/)[0], /onclick/);
console.log('Tab navigation: startup, all sections, links, history and update routes passed');
