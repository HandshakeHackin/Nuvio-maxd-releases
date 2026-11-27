#!/usr/bin/env node
// node scripts/bump.js [patch|minor|major|x.y.z]   sets one version in manifest.json (global + every scraper) and package.json
// node scripts/bump.js --check                     fails if those versions disagree
const fs = require('fs');
const path = require('path');

const manifestFile = path.join(__dirname, '..', 'manifest.json');
const packageFile = path.join(__dirname, '..', 'package.json');
const manifest = JSON.parse(fs.readFileSync(manifestFile, 'utf8'));
const pkg = JSON.parse(fs.readFileSync(packageFile, 'utf8'));
const arg = process.argv[2] || 'patch';

if (arg === '--check') {
    const versions = new Set([manifest.version, pkg.version, ...manifest.scrapers.map(s => s.version)]);
    if (versions.size > 1) {
        console.error(`version mismatch: manifest ${manifest.version}, package ${pkg.version}, scrapers ${[...new Set(manifest.scrapers.map(s => s.version))].join('/')}. Run: npm run bump -- ${manifest.version}`);
        process.exit(1);
    }
    console.log(`versions ok (${manifest.version})`);
    process.exit(0);
}

let next = arg;
if (['patch', 'minor', 'major'].includes(arg)) {
    const [major, minor, patch] = manifest.version.split('.').map(Number);
    next = arg === 'major' ? `${major + 1}.0.0` : arg === 'minor' ? `${major}.${minor + 1}.0` : `${major}.${minor}.${patch + 1}`;
}
if (!/^\d+\.\d+\.\d+$/.test(next)) { console.error(`invalid version: ${next}`); process.exit(1); }

manifest.version = next;
for (const s of manifest.scrapers) s.version = next;
pkg.version = next;
fs.writeFileSync(manifestFile, JSON.stringify(manifest, null, 2) + '\n');
fs.writeFileSync(packageFile, JSON.stringify(pkg, null, 2) + '\n');
console.log(`version ${next}`);
