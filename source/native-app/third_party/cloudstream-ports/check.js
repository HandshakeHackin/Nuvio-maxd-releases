#!/usr/bin/env node
// Offline self-check of the extractor logic that has no live-network test: node check.js
const assert = require('assert');
const esbuild = require('esbuild');

esbuild.buildSync({ entryPoints: ['shared/extractors/util.js', 'shared/extractors/index.js', 'shared/http.js'], outdir: 'node_modules/.cache/check', bundle: true, format: 'cjs', platform: 'neutral', external: ['cheerio', 'crypto-js'] });
const { unpack, jwplayer } = require('./node_modules/.cache/check/extractors/util.js');

const packed = `<script>eval(function(p,a,c,k,e,d){while(c--)if(k[c])p=p.replace(new RegExp('\\\\b'+c.toString(a)+'\\\\b','g'),k[c]);return p}('0 1=\\'2\\';3.4({5:[{6:"7://8.9/a.b"}]})',12,12,'var|x|h\\u00e9llo|jwplayer|setup|sources|file|https|cdn|example|master|m3u8'.split('|'),0,{}))</script>`;
const code = unpack(packed);
assert.ok(code.startsWith("var x='"), code);
assert.deepStrictEqual(jwplayer(code, 'https://host.tld/e/1'), [{ url: 'https://cdn.example/master.m3u8', quality: 'auto' }]);
assert.deepStrictEqual(jwplayer('sources: [{file:"/v.mp4",label:"720p"}]', 'https://h.tld/x'), [{ url: 'https://h.tld/v.mp4', quality: '720p' }]);


// a provider that fails early while a request is still running must not answer before that request is done
(async () => {
    const { provider, send } = require('./node_modules/.cache/check/http.js');
    let finished = false;
    globalThis.fetch = () => new Promise(resolve => setTimeout(() => { finished = true; resolve({ ok: true }); }, 50));
    console.error = () => {};
    const streams = await provider(async () => { send('https://slow.example'); throw new Error('early'); }).getStreams('1', 'movie');
    assert.deepStrictEqual(streams, []);
    assert.ok(finished, 'getStreams answered while a request was still running');

    // only the best language that exists: German dub, else German subtitles, else nothing; huhu.to files "…GerSub…" under plain German
    globalThis.fetch = async () => ({ ok: false, status: 405 });
    const { fileNames } = require('./node_modules/.cache/check/http.js');
    const names = async (list) => (await provider(async () => list).getStreams('1', 'tv', 1, 1)).map(s => s.name);
    const s = (title, url) => ({ name: 'X', title, url });
    fileNames['https://v/gersub'] = 'KonoSuba E01 GerSub AC3 720p BluRay x264-Fuuko';
    assert.deepStrictEqual(await names([s('voe.sx · Ger-Sub', 'https://v/1'), s('mixdrop.ps · Englisch', 'https://v/2'), s('streamtape.com · Deutsch', 'https://v/3')]),
        ['X · 🇩🇪 · streamtape.com']);
    // the file name outranks a title that only says German: that "DE" file is subtitled, so it is no dub
    assert.deepStrictEqual(await names([s('voe.sx · DE', 'https://v/gersub'), s('mixdrop.ps · Englisch', 'https://v/4'), s('vidoza.net · Japanisch, dt. UT', 'https://v/5')]),
        ['X · 🌐 UT 🇩🇪 · voe.sx', 'X · 🇯🇵 UT 🇩🇪 · vidoza.net']);
    // nothing German at all: the plugin shows nothing (other addons cover other languages)
    assert.deepStrictEqual(await names([s('mixdrop.ps · Englisch', 'https://v/6'), s('voe.sx · Französisch', 'https://v/7')]), []);
    // the file name adds source and audio tags to the title
    const tagged = await provider(async () => [s('voe.sx · DE', 'https://v/gersub')]).getStreams('1', 'tv', 1, 1);
    assert.strictEqual(tagged[0].title, 'voe.sx · Original, dt. UT · BluRay · AC3 · H.264');

    // camera recordings go unless nothing else is left
    const q = (host, quality, url) => ({ name: 'X', title: `${host} · Deutsch`, url: url || `https://${host}/v.mp4`, quality });
    assert.deepStrictEqual((await provider(async () => [q('a.com', 'CAM'), q('b.com', '720p')]).getStreams('1', 'movie')).map(x => x.title), ['b.com · Deutsch']);
    assert.strictEqual((await provider(async () => [q('a.com', 'HDCAM')]).getStreams('1', 'movie')).length, 1);

    // best resolution first, one entry per hoster, 360p and below dropped, at most four, unknown resolutions last
    const mix = await provider(async () => [q('a.com', '360p'), q('b.com', '1080p'), q('c.com', '480p'), q('c.com', '720p'), q('d.com', '480p'), q('e.com', 'auto'), q('f.com', '1080p')]).getStreams('1', 'movie');
    assert.deepStrictEqual(mix.map(x => `${x.title.split(' · ')[0]} ${x.quality}`), ['b.com 1080p', 'f.com 1080p', 'c.com 720p', 'd.com 480p']);
    // a lighter variant of the same hoster survives next to the Full HD one, even when the cap would cut it
    const lite = await provider(async () => [q('m.com', '1080p', 'https://m/1'), q('m.com', '720p', 'https://m/2'), q('b.com', '1080p'), q('c.com', '1080p'), q('d.com', '1080p'), q('e.com', '1080p')]).getStreams('1', 'movie');
    assert.ok(lite.length <= 4 && lite.some(x => x.quality === '720p'));
    // nothing better than 360p: show it anyway
    assert.strictEqual((await provider(async () => [q('a.com', '360p'), q('b.com', '240p')]).getStreams('1', 'movie')).length, 2);

    // German hosters first, otherwise the early stop could skip them (English ones listed before the German one)
    const { germanFirst, gather } = require('./node_modules/.cache/check/http.js');
    assert.deepStrictEqual(germanFirst(['Englisch', 'Deutsch Sub', 'de', 'Deutsch', 'fr'], t => t), ['de', 'Deutsch', 'Englisch', 'Deutsch Sub', 'fr']);
    const late = [...Array(5)].map((_, i) => ({ lang: 'Englisch', n: i })).concat({ lang: 'Deutsch', n: 9 });
    const pickedGerman = await provider(async () => gather(germanFirst(late, v => v.lang), async v => [1, 2].map(k => ({ name: 'M', title: `h${v.n}${k}.com · ${v.lang}`, url: `https://h${v.n}${k}.com/v.mp4`, quality: '720p' })))).getStreams('1', 'movie');
    assert.deepStrictEqual(pickedGerman.map(x => x.name), ['M · 🇩🇪 · h91.com', 'M · 🇩🇪 · h92.com']);

    // stop starting requests once enough streams are in
    const started = [];
    const got = await gather([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], async n => { started.push(n); return [n]; });
    assert.deepStrictEqual(started, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    assert.strictEqual(got.length, 10);
    const few = [];
    await gather([1, 2, 3, 4, 5, 6, 7, 8], async n => { few.push(n); return [n, n]; });
    assert.deepStrictEqual(few, [1, 2, 3, 4, 5], 'five at a time, then enough streams are in');
    assert.deepStrictEqual(await gather([1, 2], async n => { if (n === 1) throw new Error('x'); return [n]; }), [2]);

    // "HLS" becomes the best resolution of the master playlist, codec/bitrate/HDR join the title, MP4 gets its size from a HEAD
    // request; failing playlists and servers keep the plain labels, vanished ones (404/410) are dropped
    globalThis.fetch = async (url, opts = {}) => {
        if (url.includes('good')) return { ok: true, text: async () => '#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1000000,RESOLUTION=1280x720,CODECS="avc1.4d401f"\na.m3u8\n#EXT-X-STREAM-INF:BANDWIDTH=6000000,RESOLUTION=1920x1080,CODECS="hvc1.2.4.L120",VIDEO-RANGE=PQ\nb.m3u8' };
        if (url.includes('sized') && opts.method === 'HEAD') return { ok: true, headers: { get: h => (h === 'content-length' ? '1500000000' : null) } };
        if (url.includes('gone')) return { ok: false, status: 404 };
        return { ok: false, status: 403 };
    };
    const hls = await provider(async () => [
        { name: 'X', title: 'a.com', url: 'https://cdn/good/master.m3u8', quality: 'auto' },
        { name: 'X', title: 'b.com', url: 'https://cdn/bad/master.m3u8', quality: 'auto' },
        { name: 'X', title: 'c.com', url: 'https://cdn/sized.mp4', quality: 'auto' },
        { name: 'X', title: 'd.com', url: 'https://cdn/x.mp4', quality: '720p' },
        { name: 'X', title: 'e.com', url: 'https://cdn/gone/master.m3u8', quality: 'auto' },
    ]).getStreams('1', 'movie');
    assert.deepStrictEqual(hls.map(x => x.quality), ['1080p', '720p', 'HLS', 'MP4']);
    assert.deepStrictEqual(hls.map(x => x.title), ['a.com · H.265 · 6,0 Mbit/s · HDR10', 'd.com', 'b.com', 'c.com · 1,5 GB']);
    // a host that never answers must not hold the list up: the details request is aborted, the stream stays
    globalThis.fetch = (url, opts = {}) => new Promise((resolve, reject) => opts.signal && opts.signal.addEventListener('abort', () => reject(new Error('aborted'))));
    const t0 = Date.now();
    const slow = await provider(async () => [{ name: 'X', title: 'a.com', url: 'https://cdn/slow/master.m3u8', quality: 'auto' }]).getStreams('1', 'movie');
    assert.deepStrictEqual(slow.map(x => x.quality), ['HLS']);
    assert.ok(Date.now() - t0 < 8000, 'a hanging playlist held the list up');
    // the only stream is gone: better to show it than an empty list
    globalThis.fetch = async (url, opts = {}) => (url.includes('gone') ? { ok: false, status: 404 } : { ok: false, status: 403 });
    assert.strictEqual((await provider(async () => [{ name: 'X', title: 'e.com', url: 'https://cdn/gone/master.m3u8', quality: 'auto' }]).getStreams('1', 'movie')).length, 1);

    // Dood only ever answers with a Cloudflare challenge, so it must not cost a request
    let asked = 0;
    globalThis.fetch = async () => { asked++; return { ok: false, status: 403 }; };
    const { resolveEmbed } = require('./node_modules/.cache/check/extractors/index.js');
    assert.deepStrictEqual(await resolveEmbed('https://dood.to/e/abc', 'https://x.example/'), []);
    assert.deepStrictEqual(await resolveEmbed('//playmogo.com/e/abc', 'https://x.example/'), []);
    assert.strictEqual(asked, 0);

    // TMDB counts Re:ZERO as one season of 85 episodes, the sites as four seasons of 25/25/16/19
    esbuild.buildSync({ entryPoints: ['src/serienstream/common.js'], outfile: 'node_modules/.cache/check/common.js', bundle: true, format: 'cjs', platform: 'neutral' });
    const { splitSeasonPath } = require('./node_modules/.cache/check/common.js');
    globalThis.fetch = async url => {
        const s = Number(url.match(/staffel-(\d+)$/)[1]);
        const row = i => `<a href="/anime/stream/x/staffel-${s}/episode-${i + 1}">${i + 1}</a>`;
        // every episode is linked twice (number and title column)
        return { ok: true, text: async () => Array.from({ length: [25, 25, 16, 19][s - 1] || 0 }, (_, i) => row(i) + row(i)).join('') };
    };
    assert.strictEqual(await splitSeasonPath('https://a.example', '/anime/stream/x', 1, 78), '/anime/stream/x/staffel-4/episode-12');
    assert.strictEqual(await splitSeasonPath('https://a.example', '/anime/stream/x', 2, 26), '/anime/stream/x/staffel-3/episode-1');
    assert.strictEqual(await splitSeasonPath('https://a.example', '/anime/stream/x', 1, 5), null);
    assert.strictEqual(await splitSeasonPath('https://a.example', '/anime/stream/x', 1, 999), null);
    // s.to's hoster links may need the cookie of the episode page
    const { cookieHeader, followRedirect } = require('./node_modules/.cache/check/common.js');
    assert.strictEqual(cookieHeader('a=1; Path=/; HttpOnly, b=2; Expires=Wed, 01 Jan 2031 00:00:00 GMT; Path=/'), 'a=1; b=2');
    let sent;
    globalThis.fetch = async (url, opts) => { sent = opts.headers; return { ok: true, status: 200, url: 'https://hoster.example/e/1', headers: { get: () => null } }; };
    assert.strictEqual(await followRedirect('https://s.example/r/1', 'https://s.example/ep', 'a=1'), 'https://hoster.example/e/1');
    assert.strictEqual(sent.Cookie, 'a=1');
    // better hosters first, Dood last, equal ranks keep the site's order
    const { byHoster } = require('./node_modules/.cache/check/extractors/index.js');
    assert.deepStrictEqual(byHoster(['https://mixdrop.ps/e/1', 'https://dood.to/e/2', 'https://other.example/e/3', 'https://voe.sx/e/4', 'https://vidsonic.net/e/5', 'https://unknown.example/e/6']),
        ['https://voe.sx/e/4', 'https://vidsonic.net/e/5', 'https://other.example/e/3', 'https://unknown.example/e/6', 'https://mixdrop.ps/e/1', 'https://dood.to/e/2']);
    assert.deepStrictEqual(byHoster([{ h: 'Streamtape' }, { h: 'VOE' }, { h: 'Doodstream' }], x => x.h).map(x => x.h), ['VOE', 'Streamtape', 'Doodstream']);

    // TMDB: plugins in one runtime share the answer; a failed request is not kept
    esbuild.buildSync({ entryPoints: ['shared/tmdb.js'], outfile: 'node_modules/.cache/check/tmdb.js', bundle: true, format: 'cjs', platform: 'neutral' });
    const { getMeta } = require('./node_modules/.cache/check/tmdb.js');
    let tmdbCalls = 0, fail = true;
    globalThis.fetch = async () => {
        tmdbCalls++;
        if (fail) throw new Error('network');
        return { ok: true, text: async () => JSON.stringify({ title: 'Dexter', original_title: 'Dexter', first_air_date: '2006-10-01', external_ids: { imdb_id: 'tt0773262' } }) };
    };
    await getMeta('1405', 'tv').catch(() => {});
    fail = false;
    const [m1, m2] = await Promise.all([getMeta('1405', 'tv'), getMeta('1405', 'tv')]);
    assert.strictEqual(tmdbCalls, 2, 'one failed and one successful request, the second caller shares the first');
    assert.strictEqual(m1, m2);
    assert.strictEqual(m1.imdbId, 'tt0773262');
    await getMeta('1405', 'tv');
    assert.strictEqual(tmdbCalls, 2, 'cached');
    // s.to's "frameBridge" page: no redirect means a browser check is wanted, which a plugin cannot pass
    globalThis.fetch = async url => ({ ok: true, status: 200, url, headers: { get: () => 'text/html; charset=utf-8' } });
    await assert.rejects(followRedirect('https://s.example/r/1', 'https://s.example/ep', 'a=1'), e => e.gate === true);
    console.log('check ok');
})();
