#!/usr/bin/env node
// node test.js <provider> <tmdbId> <movie|tv> [season] [episode]   runs the BUNDLED providers/<provider>.js
// node test.js --extractor <embedUrl> [referer]                    runs shared/extractors directly
// Needs TMDB_API_KEY in the environment or in .tmdb_key (Nuvio injects its own key in the app). Probes every returned URL.
const path = require('path');
const esbuild = require('esbuild');

const fs = require('fs');
const keyFile = path.join(__dirname, '.tmdb_key');
globalThis.TMDB_API_KEY = process.env.TMDB_API_KEY || (fs.existsSync(keyFile) ? fs.readFileSync(keyFile, 'utf8').trim() : undefined);

// Nuvio (iOS/Android mobile) closes its QuickJS runtime as soon as getStreams settles; a fetch still in flight then crashes the app.
let inFlight = 0;
const nativeFetch = globalThis.fetch;
globalThis.fetch = (...args) => {
    inFlight++;
    return nativeFetch(...args).finally(() => inFlight--);
};

async function probe(s) {
    try {
        const res = await fetch(s.url, { headers: Object.assign({ Range: 'bytes=0-2047' }, s.headers) });
        const type = res.headers.get('content-type') || '';
        const body = Buffer.from(await res.arrayBuffer()).toString('latin1', 0, 16);
        const ok = res.status < 400 && (/video|mpegurl|octet|mp4|binary/i.test(type) || body.startsWith('#EXTM3U'));
        return `${ok ? 'PLAYABLE' : 'CHECK'} ${res.status} ${type}`;
    } catch (e) {
        return `ERROR ${e.message}`;
    }
}

async function main() {
    const [a, ...rest] = process.argv.slice(2);
    let streams;
    if (a === '--extractor') {
        const out = path.join(__dirname, 'node_modules/.cache/extractors.js');
        esbuild.buildSync({ entryPoints: [path.join(__dirname, 'shared/extractors/index.js')], outfile: out, bundle: true, format: 'cjs', platform: 'neutral', external: ['cheerio', 'crypto-js'] });
        streams = await require(out).resolveEmbed(rest[0], rest[1]);
    } else {
        const [tmdbId, type, s, e] = rest;
        const { getStreams } = require(path.join(__dirname, 'providers', `${a}.js`));
        streams = await getStreams(tmdbId, type || 'movie', s ? Number(s) : null, e ? Number(e) : null);
        if (inFlight) console.log(`CRASH-RISK: ${inFlight} fetch(es) still running when getStreams returned`);
    }
    for (const s of streams) console.log(`${await probe(s)}\n  ${JSON.stringify(s)}`);
    console.log(`${streams.length} stream(s)`);
    process.exitCode = streams.length && !inFlight ? 0 : 1;
}

main().catch(e => { console.error(e); process.exitCode = 1; });
