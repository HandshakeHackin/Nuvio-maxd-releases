#!/usr/bin/env node
// node scripts/sync-meta.js          regenerates the provider dropdown of the issue template from manifest.json
// node scripts/sync-meta.js --check  fails if the template is out of date
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');
const templateFile = path.join(root, '.github/ISSUE_TEMPLATE/provider-problem.yml');
const manifest = JSON.parse(fs.readFileSync(path.join(root, 'manifest.json'), 'utf8'));
const current = fs.readFileSync(templateFile, 'utf8');

const options = manifest.scrapers.map(s => s.name).concat('Mehrere / alle').map(n => `        - ${n}`).join('\n');
// the first dropdown (id: provider) lists the providers
const next = current.replace(/(id: provider\n\s+attributes:\n\s+label: Provider\n\s+options:\n)(?:\s+- .*\n)+/, `$1${options}\n`);

if (process.argv.includes('--check')) {
    if (next !== current) { console.error('issue template is out of date. Run: npm run meta'); process.exit(1); }
    console.log('issue template ok');
} else {
    fs.writeFileSync(templateFile, next);
    console.log(next === current ? 'issue template unchanged' : 'issue template updated');
}
