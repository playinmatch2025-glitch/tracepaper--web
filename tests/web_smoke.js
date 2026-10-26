#!/usr/bin/env node
// Headless smoke test of web/index.html: loads the page script against a tiny
// fake DOM, opens every chapter, steps through the whole run (which calls the
// renderers at every step), and checks the lesson tasks can be solved.
//   node tests/web_smoke.js
const fs = require('fs'), path = require('path'), vm = require('vm');
const html = fs.readFileSync(path.join(__dirname, '../web/index.html'), 'utf8');
const code = html.match(/<script>([\s\S]*)<\/script>/)[1];

const cache = {};
const El = () => ({ innerHTML: '', textContent: '', value: '', style: {}, dataset: {}, rows: 0, hidden: false, checked: false,
  classList: { add() {}, remove() {} }, setAttribute() {}, append() {}, remove() {}, closest: () => null,
  querySelector: () => null, querySelectorAll: () => [], getBoundingClientRect: () => ({ left: 0, top: 0, right: 0, width: 0, height: 0 }) });
global.document = { querySelector: s => s === '.hv' ? null : (cache[s] = cache[s] || El()), querySelectorAll: () => [],
  documentElement: { dataset: {} }, createElement: El, body: El(), head: El() };
global.localStorage = { getItem: () => null, setItem() {} };
global.matchMedia = () => ({ matches: true });
global.addEventListener = () => {};
global.alert = () => {};
vm.runInThisContext(code);

const ev = s => vm.runInThisContext(s);
let bad = 0;
const check = (name, ok, extra = '') => { console.log((ok ? 'ok    ' : 'FAIL  ') + name + (ok ? '' : '  ' + extra)); if (!ok) bad++; };

const n = ev('CH.length');
for (let i = 0; i < n; i++) {
  if (!ev(`!!CH[${i}].p`)) continue;
  const title = ev(`CH[${i}].n`);
  try {
    ev(`chapter(${i})`);
    const len = ev('R.log.length');
    for (let k = 0; k < Math.min(len, 400); k++) ev('go(1)');   // renders every step
    check(`chapter ${i + 1} "${title}" renders ${len} steps`, true);
  } catch (e) { check(`chapter ${i + 1} "${title}"`, false, e.stack.split('\n').slice(0, 3).join(' | ')); }
}

const idx = name => ev(`CH.findIndex(c=>c.n===${JSON.stringify(name)})`);
const setSrc = (i, s) => { ev(`chapter(${i})`); cache['#src'].value = s; ev('recompute()'); };

// Memory: the lesson program must crash, a smaller loop must finish.
let i = idx('Memory');
ev(`chapter(${i})`);
check('Memory: lesson program runs out of cells', /cells/.test(ev('R.err')), ev('R.err'));
check('Memory: not solved yet', !ev(`done.has(${i})`));
setSrc(i, 'var s = "";\nfor (var i = 0; i < 6; i = i + 1) {\n  s = s + "a";\n}\nprint s;');
check('Memory: smaller loop solves it', ev(`done.has(${i})`));

// Garbage collection: lesson program prints, collector frees, nil-ing keep frees "hello".
i = idx('Garbage collection');
ev(`chapter(${i})`);
check('GC: lesson program output', ev('R.out.join("|")') === 'hello|aaaaaaaaaaaaaa', ev('R.out.join("|")') + ' ' + ev('R.err'));
check('GC: sweeps happened', ev('R.log.filter(x=>x.gx&&x.gx.ph==="sweep").length') >= 2);
check('GC: "hello" survives while keep points at it', !ev('R.log.some(x=>x.gx&&x.gx.freed&&x.gx.freed.some(o=>o.s==="hello"))'));
check('GC: not solved yet', !ev(`done.has(${i})`));
ev('pos=R.log.findIndex(x=>x.gx&&x.gx.ph==="mark")');  ev('draw()');
check('GC: mark step shows ticked cells', /cell live mk/.test(cache['#viz'].innerHTML));
ev('pos=R.log.findIndex(x=>x.gx&&x.gx.ph==="sweep")'); ev('draw()');
check('GC: sweep step shows dissolving cells', /cell dis/.test(cache['#viz'].innerHTML));
check('GC: grid has 8 cells', (cache['#viz'].innerHTML.match(/class="cell/g) || []).length === 8);
setSrc(i, 'var keep = "he" + "llo";\nkeep = nil;\nvar s = "";\nfor (var i = 0; i < 14; i = i + 1) {\n  s = s + "a";\n}\nprint s;');
check('GC: cutting the arrow frees "hello" and solves it', ev(`done.has(${i})`));
setSrc(i, 'var a1 = "x" + "1";var a2 = "x" + "2";var a3 = "x" + "3";var a4 = "x" + "4";var a5 = "x" + "5";var a6 = "x" + "6";var a7 = "x" + "7";var a8 = "x" + "8";var a9 = "x" + "9";');
check('GC: nine live strings overflow the grid with a clear message', /still reachable/.test(ev('R.err')), ev('R.err'));

console.log(bad ? `\n${bad} problem(s)` : '\nweb page OK');
process.exit(bad ? 1 : 0);
