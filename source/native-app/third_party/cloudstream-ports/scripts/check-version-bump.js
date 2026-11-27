#!/usr/bin/env node
// node scripts/check-version-bump.js [base]   fails when plugin files changed against base but manifest.json kept its version
// (Nuvio only offers an update when the version changes). base defaults to origin/main.
const cp = require('child_process');
const fs = require('fs');

const base = process.argv[2] || 'origin/main';
const run = cmd => cp.execSync(cmd, { encoding: 'utf8' }).trim();
const changed = run(`git diff --name-only ${base}...HEAD`).split('\n').filter(f => /^(src|shared|providers)\//.test(f));
if (!changed.length) {
    console.log('no plugin files changed, no version bump needed');
    process.exit(0);
}
const parse = v => String(v).split('.').map(Number);
const newer = (a, b) => parse(a).some((n, i) => n !== parse(b)[i]) && parse(a).reduce((r, n, i) => r || Math.sign(n - parse(b)[i]), 0) > 0;
const before = JSON.parse(run(`git show ${base}:manifest.json`)).version;
const now = JSON.parse(fs.readFileSync('manifest.json', 'utf8')).version;
if (!newer(now, before)) {
    console.error(`${changed.length} plugin file(s) changed (${changed.slice(0, 3).join(', ')}${changed.length > 3 ? ', …' : ''}), but manifest.json is at ${now} (${base}: ${before}).\nNuvio only offers an update when the version goes up. Run: npm run bump -- patch && npm run build`);
    process.exit(1);
}
console.log(`version ${before} -> ${now}`);
