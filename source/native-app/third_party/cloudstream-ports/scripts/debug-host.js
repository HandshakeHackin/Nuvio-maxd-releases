#!/usr/bin/env node
// node scripts/debug-host.js <embedUrl> [referer]
// Diagnostic for a video hoster the plugins cannot read (e.g. vinovo.to answers HTTP 403): shows what the page answers to a
// plugin-style request and to a browser-style one. Prints status, headers, title, hints and the scripts that mention a stream.
const cheerio = require('cheerio');

const [url, referer = 'https://filmpalast.to/'] = process.argv.slice(2);
if (!url) {
    console.error('usage: node scripts/debug-host.js <embedUrl> [referer]');
    process.exit(2);
}
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36';
const BROWSER = {
    'User-Agent': UA,
    Accept: 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8',
    'Accept-Language': 'de-DE,de;q=0.9,en;q=0.8',
    'Upgrade-Insecure-Requests': '1',
    'Sec-Fetch-Dest': 'iframe', 'Sec-Fetch-Mode': 'navigate', 'Sec-Fetch-Site': 'cross-site',
};
const HINTS = ['cloudflare', 'cf-chl', 'turnstile', 'captcha', 'just a moment', 'eval(function(p,a,c,k,e', 'm3u8', 'sources:', 'file:', '/api/', 'atob(', 'jwplayer', 'token', 'pass_md5'];

async function show(label, opts) {
    console.log(`\n=== ${label}`);
    try {
        const res = await fetch(url, opts);
        const body = await res.text();
        const $ = cheerio.load(body);
        console.log(`status ${res.status}, final url ${res.url}, content-type ${res.headers.get('content-type')}, ${body.length} bytes`);
        console.log(`server: ${res.headers.get('server')}, cf-ray: ${res.headers.get('cf-ray') || '-'}, location: ${res.headers.get('location') || '-'}`);
        console.log(`title: ${$('title').text().trim()}`);
        console.log(`hints: ${HINTS.filter(h => body.toLowerCase().includes(h)).join(', ') || '-'}`);
        console.log(`text: ${$('body').text().replace(/\s+/g, ' ').trim().slice(0, 400)}`);
        for (const s of $('script').map((i, el) => $(el).html() || '').get().filter(t => /m3u8|\.mp4|sources|file\s*:|\/api\//i.test(t)).slice(0, 3)) {
            console.log(`script: ${s.replace(/\s+/g, ' ').slice(0, 500)}`);
        }
    } catch (e) {
        console.log(`failed: ${e.message}`);
    }
}

(async () => {
    await show('1 plugin style (UA + Referer)', { headers: { 'User-Agent': UA, Referer: referer } });
    await show('2 browser style', { headers: Object.assign({ Referer: referer }, BROWSER) });
    await show('3 browser style without Referer', { headers: BROWSER });
})();
