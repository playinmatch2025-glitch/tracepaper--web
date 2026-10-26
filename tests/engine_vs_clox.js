#!/usr/bin/env node
// Conformance: run every tests/programs/*.lox through BOTH the real C VM
// (bytecode-vm/clox) and the JavaScript engine embedded in web/index.html,
// and require the same outcome (ok / compile error / runtime error) and the
// same printed lines. This is what lets us say the web page behaves like clox.
//
//   node tests/engine_vs_clox.js
const fs = require('fs'), path = require('path'), cp = require('child_process');
const root = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'web/index.html'), 'utf8');
let engine = html.match(/\/\*ENGINE\*\/([\s\S]*?)\/\*END\*\//)[1];
// The page stops runaway loops at 6000 steps and records a snapshot per step for the
// animations. Neither is wanted here, so lift the cap and switch recording off.
const before = engine;
engine = engine.replace('++n>6000', '++n>1e9').replace('snap=(F,ip,c,say,jump,x)=>log.push({', 'snap=(F,ip,c,say,jump,x)=>0&&log.push({');
if (engine === before || !engine.includes('0&&log.push')) throw new Error('engine patterns changed: update this test');
const E = new Function(engine + ';return {compile,run}')();
const clox = path.join(root, 'bytecode-vm/clox');
if (!fs.existsSync(clox)) { console.error('Build clox first: make -C bytecode-vm'); process.exit(2); }

const dir = path.join(root, 'tests/programs');
let pass = 0, fail = 0;
for (const f of fs.readdirSync(dir).filter(x => x.endsWith('.lox')).sort()) {
  const file = path.join(dir, f), text = fs.readFileSync(file, 'utf8');
  const c = cp.spawnSync(clox, [file], { encoding: 'utf8' });
  const cKind = c.status === 0 ? 'ok' : c.status === 65 ? 'compile' : c.status === 70 ? 'runtime' : 'crash:' + c.status;
  const cOut = c.stdout.replace(/\n$/, '');
  let jKind, jOut = '';
  try {
    const P = E.compile(text);
    const R = E.run(P);
    jKind = R.err ? 'runtime' : 'ok';
    jOut = R.out.join('\n');
  } catch (e) { jKind = 'compile'; }
  const same = cKind === jKind && (cKind !== 'ok' || cOut === jOut);
  if (same) pass++; else { fail++; console.log(`DIFF  ${f}\n  clox: ${cKind} ${JSON.stringify(cOut.slice(0, 80))}\n  web : ${jKind} ${JSON.stringify(jOut.slice(0, 80))}`); }
}
console.log(`web engine vs real clox: ${pass} agree, ${fail} differ`);
process.exit(fail ? 1 : 0);
