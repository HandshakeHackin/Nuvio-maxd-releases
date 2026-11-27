#!/usr/bin/env node
// node scripts/smoke.js [provider...]   runs the BUNDLED providers against known titles (needs TMDB_API_KEY and network)
// Writes smoke-report.md and smoke-failures.txt. Exit code is 0 even when providers are down: the workflow reads the files.
//   ✅ streams found   ❌ real failure   ⚠️ blocked from this IP/region   ⚪ optional provider, nothing found
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');
globalThis.TMDB_API_KEY = process.env.TMDB_API_KEY;
if (!globalThis.TMDB_API_KEY) { console.error('TMDB_API_KEY is not set'); process.exit(2); }

const config = JSON.parse(fs.readFileSync(path.join(__dirname, 'smoke.json'), 'utf8'));
delete config._comment;
const manifest = JSON.parse(fs.readFileSync(path.join(root, 'manifest.json'), 'utf8'));
const wanted = process.argv.slice(2);
const ids = manifest.scrapers.filter(s => s.enabled !== false).map(s => s.id).filter(id => !wanted.length || wanted.includes(id));

// providers swallow their errors and return [], so their console.error output is the only hint why nothing came back
const logged = [];
console.error = (...args) => { logged.push(args.map(String).join(' ').replace(/\s+/g, ' ').replace(/\?\S*/g, '')); };
const BLOCKED = /HTTP (401|403|429|451|503)|captcha|cloudflare|challenge|timeout|ECONNRESET|ENOTFOUND|ETIMEDOUT|fetch failed/i;
const withTimeout = (p, s) => Promise.race([p, new Promise((_, rej) => setTimeout(() => rej(new Error('timeout')), s * 1000))]);

const CHALLENGE = /just a moment|cf-chl|challenge-platform|cf-browser-verification|captcha|attention required/i;
async function probe(url) {
    try {
        const res = await withTimeout(fetch(url, { headers: { 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36' } }), 30);
        const body = await res.text();
        if ([401, 403, 429, 451, 503].includes(res.status)) return { blocked: true, note: `HTTP ${res.status}, Seite lässt diese IP nicht herein` };
        if (CHALLENGE.test(body)) return { blocked: true, note: `HTTP ${res.status}, Cloudflare-/Captcha-Prüfung statt Inhalt` };
        return { blocked: false, note: `HTTP ${res.status}, erreichbar` };
    } catch (e) {
        return { blocked: true, note: e.message };
    }
}

// the first (best) stream: does the URL answer with video data? Reads only the first chunk, never the whole file.
async function playable(stream) {
    try {
        const res = await withTimeout(fetch(stream.url, { headers: Object.assign({ 'User-Agent': 'Mozilla/5.0', Range: 'bytes=0-2047' }, stream.headers) }), 25);
        const type = res.headers.get('content-type') || '';
        let head = '';
        if (res.body && res.body.getReader) {
            const reader = res.body.getReader();
            const { value } = await reader.read();
            head = Buffer.from(value || []).toString('latin1', 0, 16);
            await reader.cancel().catch(() => {});
        }
        if (res.status < 400 && (/video|mpegurl|octet|mp4|binary/i.test(type) || head.startsWith('#EXTM3U'))) return { ok: true };
        if (res.status === 404 || res.status === 410) return { dead: true, note: `HTTP ${res.status}` };
        return { note: `HTTP ${res.status} ${type}`.trim() };
    } catch (e) {
        return { note: e.message };
    }
}

async function check(id) {
    const entry = config[id];
    if (!entry) return { id, status: 'fail', note: 'no smoke candidates in scripts/smoke.json' };
    const { getStreams } = require(path.join(root, 'providers', `${id}.js`));
    const notes = [];
    let blocked = false;
    for (const c of entry.candidates) {
        const label = `${c.type} ${c.tmdbId}${c.season ? ` S${c.season}E${c.episode}` : ''}`;
        logged.length = 0;
        try {
            const streams = await withTimeout(getStreams(String(c.tmdbId), c.type, c.season || null, c.episode || null), entry.timeout || 90);
            if (streams && streams.length && /^https?:/.test(streams[0].url)) {
                const found = `${streams.length} stream(s) for ${label} (${streams.slice(0, 6).map(s => s.quality).join(', ')})`;
                const p = await playable(streams[0]);
                if (p.ok) return { id, status: 'ok', note: `${found}, plays` };
                // a 404 means the link is dead; anything else may only be this IP being refused
                return { id, status: p.dead ? 'fail' : 'blocked', note: `${found}, but the best stream does not answer: ${p.note}` };
            }
        } catch (e) {
            logged.push(e.message);
        }
        const why = [...new Set(logged)].slice(0, 2).map(l => l.slice(0, 110)).join(' / ');
        if (BLOCKED.test(why)) blocked = true;
        notes.push(`${label}: ${why || 'no match'}`);
    }
    // the provider's own log may be silent (captcha pages are HTTP 200), so ask the site directly whether this IP is let in
    let probed = '';
    if (entry.probe) {
        const p = await probe(entry.probe);
        if (p.blocked) blocked = true;
        probed = ` | Direktaufruf der Seite: ${p.note}`;
    }
    const status = blocked ? 'blocked' : entry.optional ? 'optional' : 'fail';
    return { id, status, note: notes.join('; ') + probed };
}

(async () => {
    // fetches that outlive a timed-out provider must not crash the process
    process.on('unhandledRejection', () => {});
    const results = [];
    // one provider at a time: the log lines have to belong to one provider, and hosts like s.to dislike bursts
    for (const id of ids) results.push(await check(id));

    const icon = { ok: '✅', fail: '❌', blocked: '⚠️', optional: '⚪' };
    const names = Object.fromEntries(manifest.scrapers.map(s => [s.id, s.name]));
    let md = `# Provider smoke test\n\nRun: ${new Date().toISOString()}\n\n| Provider | Status | Details |\n|---|---|---|\n`;
    for (const r of results) md += `| ${names[r.id]} | ${icon[r.status]} | ${r.note.replace(/\|/g, '/')} |\n`;
    const count = s => results.filter(r => r.status === s).length;
    md += `\n✅ ${count('ok')} · ❌ ${count('fail')} · ⚠️ ${count('blocked')} (von dieser IP/Region blockiert oder zu langsam, nicht eindeutig) · ⚪ ${count('optional')} (Katalog wechselt, kein Treffer für den Testtitel)\n`;
    fs.writeFileSync(path.join(root, 'smoke-report.md'), md);
    fs.writeFileSync(path.join(root, 'smoke-failures.txt'), results.filter(r => r.status === 'fail').map(r => names[r.id]).join('\n'));
    process.stdout.write(md + '\n');
    process.exit(0);
})();
