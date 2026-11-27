#!/usr/bin/env node
// Compares the upstream commit recorded in .upstream.json with upstream/master and writes UPSTREAM_UPDATE.md.
// The report always spans recorded commit -> current upstream head, so it stays complete while an earlier PR is still open.
const fs = require('fs');
const cp = require('child_process');
const run = (cmd) => cp.execSync(cmd, { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 }).trim();
const lines = (s) => s.split('\n').filter(Boolean);

const state = JSON.parse(fs.readFileSync('.upstream.json', 'utf8'));
const oldSha = state.commit;
const newSha = run('git rev-parse upstream/master');
if (oldSha === newSha) process.exit(0);

// upstream may have been force-pushed: fall back to the plain two-commit comparison
let range = oldSha + '..' + newSha;
let rewritten = false;
try { run('git merge-base --is-ancestor ' + oldSha + ' ' + newSha); } catch (e) { rewritten = true; }

const commits = lines(run('git log --format=%h%x09%s ' + (rewritten ? '-n 50 ' + newSha : range)));
const files = rewritten ? [] : lines(run('git diff --name-status ' + range));
const isSource = (p) => /\.(kt|kts)$/.test(p) && !/(^|\/)build\.gradle\.kts$/.test(p);
const localProviders = fs.readdirSync('src', { withFileTypes: true })
  .filter(e => e.isDirectory() && fs.existsSync('src/' + e.name + '/index.js')).map(e => e.name);

// "KellerKino" in upstream, "kellerkino" here: match on path segments / file names, case-insensitively
function providerMatches(path) {
  const segments = path.toLowerCase().split('/');
  const base = segments[segments.length - 1].replace(/\.[^.]+$/, '');
  return localProviders.filter(p => segments.slice(0, -1).includes(p) || base === p || base.startsWith(p));
}

const grouped = new Map();
const other = [];
const added = [];
for (const line of files) {
  const parts = line.split('\t');
  const status = parts[0][0];
  const paths = parts.slice(1);
  const path = paths[paths.length - 1];
  const entry = { status, path, previous: paths.length > 1 ? paths[0] : null };
  if (!isSource(path)) { other.push(entry); continue; }
  const matches = providerMatches(path);
  const key = matches.length ? matches.join(', ') : 'Unmapped / needs review';
  if (!grouped.has(key)) grouped.set(key, []);
  grouped.get(key).push(entry);
  if (!matches.length && status === 'A') added.push(path);
}

let md = '# Upstream update: Bnyro/GermanProviders\n\n';
md += '> This file is generated automatically. The Nuvio JavaScript providers are NOT overwritten automatically.\n\n';
if (rewritten) md += '> ⚠️ The recorded commit is no longer part of upstream history (force-push?). The file list is unavailable; compare manually.\n\n';
md += '## Commits\n\n';
for (const c of commits) md += '- ' + c.replace('\t', ' ') + '\n';
md += '\n## Changed Kotlin files\n\n';
if (!grouped.size) md += '_No Kotlin source files changed; the upstream update may be documentation/configuration only._\n';
for (const [provider, entries] of grouped) {
  md += '### ' + provider + '\n\n';
  for (const e of entries) md += '- ' + e.status + ' ' + e.path + (e.previous ? ' (from ' + e.previous + ')' : '') + '\n';
  md += '\n';
}
if (added.length) {
  md += '## Possible new upstream providers\n\n';
  for (const p of added) md += '- ' + p + '\n';
  md += '\n';
}
if (other.length) {
  md += '## Other changed files\n\n';
  for (const e of other.slice(0, 40)) md += '- ' + e.status + ' ' + e.path + '\n';
  if (other.length > 40) md += '- … and ' + (other.length - 40) + ' more\n';
  md += '\n';
}
md += '## Nuvio files to review\n\n';
const flagged = localProviders.filter(p => [...grouped.keys()].some(k => k.split(', ').includes(p)));
if (!flagged.length) md += '_None mapped automatically._\n';
for (const p of flagged) md += '- src/' + p + '/index.js → likely review target\n';
md += '\n## Upstream comparison\n\n';
md += '- Previous: ' + oldSha + '\n- Current: ' + newSha + '\n';
md += '- Compare: https://github.com/Bnyro/GermanProviders/compare/' + oldSha + '...' + newSha + '\n';
md += '\n## Porting procedure\n\n1. Review the upstream Kotlin diff.\n2. Check the corresponding src/<provider>/index.js.\n3. Port only the functional changes that apply to Nuvio.\n4. Run `npm ci && npm run build && npm test`.\n5. Test the affected provider in Nuvio before merging.\n';
fs.writeFileSync('UPSTREAM_UPDATE.md', md);

state.commit = newSha;
state.checkedAt = new Date().toISOString();
fs.writeFileSync('.upstream.json', JSON.stringify(state, null, 2) + '\n');
