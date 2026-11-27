export const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36';

// Nuvio on iOS closes the QuickJS runtime as soon as getStreams settles (or after its 60 s timeout); a fetch still
// in flight at that moment aborts the whole app. So every request goes through send(), and provider() only
// answers once none is running. After DEADLINE_MS no new request starts, to stay clear of Nuvio's timeout.
// Nuvio TV's fetch blocks, so a plugin's requests run one after the other and cannot be aborted, and it runs ten
// plugins at a time: a slow one holds a slot the others wait for. The budget is what keeps that short.
const DEADLINE_MS = 25000;
let running = 0, idle = [], deadline = Infinity;

export function send(url, opts) {
    if (Date.now() > deadline) return Promise.reject(new Error(`deadline reached, skipped ${url}`));
    running++;
    const done = () => { if (--running === 0) idle.splice(0).forEach(resolve => resolve()); };
    return fetch(url, opts).then(res => { done(); return res; }, err => { done(); throw err; });
}

// Nuvio mobile/desktop show only `name` and `quality` (not `title`), Nuvio TV shows `name - quality` plus `title`,
// so the language goes into the name and 'auto' becomes the stream format.
const SUBBED = [[/ger(man)?[\s._-]*sub|untertitel deutsch/i, 'Original, dt. UT'], [/eng(lish)?[\s._-]*sub|untertitel englisch/i, 'Original, engl. UT']];
const LANGS = [[/japanisch, dt\. ut/i, 'Japanisch, dt. UT'], [/japanisch, engl\. ut/i, 'Japanisch, engl. UT']].concat(SUBBED, [
    [/\bomu\b/i, 'OmU'], [/\bov\b/i, 'OV'], [/englisch|\ben\b/i, 'Englisch'], [/franz|\bfr\b/i, 'Französisch'], [/spanisch/i, 'Spanisch'],
    [/japanisch/i, 'Japanisch'], [/deutsch|\bde\b/i, 'Deutsch']]);

// internal language label -> what the stream list shows: flags read faster than words, and subtitles stay marked "UT"
const FLAGS = { Deutsch: '🇩🇪', Englisch: '🇬🇧', Französisch: '🇫🇷', Spanisch: '🇪🇸', Japanisch: '🇯🇵', OV: '🌐', OmU: '🌐 UT' };
const SUB_FLAGS = { 'dt. UT': '🇩🇪', 'engl. UT': '🇬🇧' };
export function flagLabel(lang) {
    const sub = lang.match(/^(.*), (dt\. UT|engl\. UT)$/);
    if (sub) return `${sub[1] === 'Original' ? '🌐' : FLAGS[sub[1]] || sub[1]} UT ${SUB_FLAGS[sub[2]]}`;
    return FLAGS[lang] || lang;
}

// stream URL -> the hoster's file name, where the hoster shows one. Sites like huhu.to file "…GerSub.720p…" under plain
// German, so a file name that says "subbed" beats a title that only says German.
export const fileNames = {};

// 0 = German dub, 1 = German subtitles, 2 = everything else
const tierOf = lang => (lang === 'Deutsch' ? 0 : /dt\. UT|OmU/.test(lang) ? 1 : 2);

function decorate(s) {
    const hit = LANGS.find(([re]) => re.test(s.title || ''));
    let lang = hit ? hit[1] : 'Deutsch', title = s.title;
    const sub = SUBBED.find(([re]) => re.test(fileNames[s.url] || ''));
    if (sub && !/UT|OmU/.test(lang)) {
        lang = sub[1];
        // Nuvio TV shows the title under the name, so it must not keep saying "Deutsch"
        title = hit ? title.replace(hit[0], lang) : [title, lang].filter(Boolean).join(' · ');
    }
    const quality = s.quality && s.quality !== 'auto' ? s.quality : (/\.m3u8|\/hls|master/i.test(s.url) ? 'HLS' : 'MP4');
    const host = ((s.title || '').split(' · ')[0].match(/^[\w-]+\.[a-z]{2,}$/) || [])[0];
    const tags = releaseTags(fileNames[s.url]);
    if (tags.length) title = [title].concat(tags).filter(Boolean).join(' · ');
    const stream = Object.assign({}, s, { name: [s.name, flagLabel(lang), host].filter(Boolean).join(' · '), title, quality });
    return { lang, tier: tierOf(lang), host: host || (String(s.url).match(/^https?:\/\/([^/]+)/) || [])[1] || '', stream, bandwidth: 0, dead: false };
}

// Details for the stream list's second line (Nuvio TV shows `title`). Everything here never throws and only costs requests
// for the first few streams while enough of the time budget is left.
const CODECS = [[/hvc1|hev1|hevc|x265|h\.?265/i, 'H.265'], [/avc1|x264|h\.?264/i, 'H.264'], [/av01|\bav1\b/i, 'AV1'], [/vp0?9/i, 'VP9']];
const codecName = text => (CODECS.find(([re]) => re.test(text)) || [])[1];
const mbit = bps => `${(bps / 1e6).toFixed(1).replace('.', ',')} Mbit/s`;
const gigabytes = bytes => (bytes >= 1e9 ? `${(bytes / 1e9).toFixed(1).replace('.', ',')} GB` : `${Math.round(bytes / 1e6)} MB`);

// release name -> source and audio tags ("BluRay · AC3"), for hosters that show the file name
function releaseTags(name) {
    return [/blu-?ray|remux|web-?dl|web-?rip|hdtv|dvd-?rip/i, /atmos|truehd|dts-?hd|dts|e-?ac-?3|ac-?3|aac/i, /x26[45]|h\.?26[45]|hevc|av1/i]
        .map(re => (String(name || '').match(re) || [])[0]).filter(Boolean).map(t => (/^(x|h\.?)26[45]$/i.test(t) || /hevc/i.test(t) ? codecName(t) : t));
}

// best variant of a master playlist: { height, bandwidth, codec, hdr }; null for a plain media playlist
function bestVariant(text) {
    let best = null, m;
    const re = /#EXT-X-STREAM-INF:([^\n]*)/g;
    while ((m = re.exec(text))) {
        const attrs = m[1];
        const v = {
            height: Number((attrs.match(/RESOLUTION=\d+x(\d+)/) || [])[1]) || 0,
            bandwidth: Number((attrs.match(/(?:^|,)BANDWIDTH=(\d+)/) || [])[1]) || 0,
            codec: codecName((attrs.match(/CODECS="([^"]*)"/) || [])[1] || ''),
            hdr: /dvh[1e]/.test(attrs) ? 'Dolby Vision' : ({ PQ: 'HDR10', HLG: 'HLG' })[(attrs.match(/VIDEO-RANGE=(\w+)/) || [])[1]],
        };
        if (!best || v.height > best.height || (v.height === best.height && v.bandwidth > best.bandwidth)) best = v;
    }
    return best;
}

// The details are optional, so a slow host must not hold the stream list up: abort after DETAIL_TIMEOUT_MS where the
// runtime can (AbortController and timers); elsewhere the request just runs as before.
const DETAIL_TIMEOUT_MS = 3000;
async function timed(url, opts, readBody) {
    if (typeof AbortController === 'undefined' || typeof setTimeout !== 'function') {
        const res = await send(url, opts);
        return { res, text: readBody && res.ok ? await res.text() : '' };
    }
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), DETAIL_TIMEOUT_MS);
    try {
        const res = await send(url, Object.assign({}, opts, { signal: controller.signal }));
        return { res, text: readBody && res.ok ? await res.text() : '' };
    } finally {
        clearTimeout(timer);
    }
}

async function playlistInfo(stream) {
    try {
        const { res, text } = await timed(stream.url, { headers: Object.assign({ 'User-Agent': UA }, stream.headers) }, true);
        if (!res.ok) {
            console.error(`[quality] playlist answered HTTP ${res.status}`);
            // 404/410 mean the stream is gone; 403 and friends may only block this request, so those stay
            return res.status === 404 || res.status === 410 ? { dead: true } : null;
        }
        const best = bestVariant(String(text));
        if (!best) console.error('[quality] playlist lists no variants');
        return best;
    } catch (e) {
        console.error(`[quality] ${e.message}`);
        return null;
    }
}

// MP4 size from a HEAD request (no body, unlike a range request that a server may ignore)
async function fileSize(stream) {
    try {
        const { res } = await timed(stream.url, { method: 'HEAD', headers: Object.assign({ 'User-Agent': UA }, stream.headers) }, false);
        if (res.status === 404 || res.status === 410) return { dead: true };
        const bytes = res.ok && Number(res.headers.get('content-length'));
        return bytes > 1e6 ? { bytes } : null;
    } catch (e) {
        return null;
    }
}

async function addDetails(list) {
    // only streams whose resolution is still unknown cost an extra request ('HLS'/'MP4' are the labels for "unknown")
    const pending = list.slice(0, 4).filter(d => d.stream.quality === 'HLS' || d.stream.quality === 'MP4');
    if (!pending.length) return;
    if (deadline - Date.now() < 8000) {
        console.error(`[quality] skipped, only ${Math.max(0, Math.round((deadline - Date.now()) / 1000))}s of the time budget left`);
        return;
    }
    await Promise.all(pending.map(async d => {
        const s = d.stream, add = [];
        if (s.quality === 'MP4' || (/\.mp4(\?|$)/i.test(s.url) && !/\.m3u8/i.test(s.url))) {
            const info = await fileSize(s);
            if (info && info.dead) d.dead = true;
            if (info && info.bytes) add.push(gigabytes(info.bytes));
        } else {
            const v = await playlistInfo(s);
            if (!v) return;
            if (v.dead) {
                d.dead = true;
                return;
            }
            if (v.height && s.quality === 'HLS') s.quality = `${v.height}p`;
            d.bandwidth = v.bandwidth || 0;
            add.push(v.codec, v.bandwidth && mbit(v.bandwidth), v.hdr);
        }
        const extra = add.filter(t => t && !String(s.title || '').includes(t));
        if (extra.length) s.title = [s.title].concat(extra).filter(Boolean).join(' · ');
    }));
}

// ---- what the stream list shows ----
// Nuvio lists every plugin in its own group, so these rules shape each plugin's own list. Each one falls back to
// "show everything" instead of leaving the list empty (except the language rule: no German, no streams).
const MAX_STREAMS = 4;   // per plugin, best first
const MIN_HEIGHT = 360;  // known resolutions up to this are dropped when anything better exists
const CAM = /\b(hd-?cam|cam-?rip|cam|hd-?ts|tele-?sync|hd-?tc|tele-?cine)\b/i;

const heightOf = d => Number((String(d.stream.quality).match(/(\d{3,4})p/) || [])[1]) || 0;
const keepIfAny = (list, keep) => {
    const kept = list.filter(keep);
    return kept.length ? kept : list;
};

function shape(list) {
    // camera recordings (labelled by the site, the hoster's file name or the quality)
    list = keepIfAny(list, d => !CAM.test([d.stream.quality, fileNames[d.stream.url], String(d.stream.title || '').split(' · ').slice(1).join(' ')].join(' ')));
    // only the best language that exists: German dub, else German subtitles. A plugin with nothing German shows nothing.
    const best = Math.min(...list.map(d => d.tier));
    if (best > 1) return [];
    list = list.filter(d => d.tier === best);
    // gone streams (HTTP 404/410 on the playlist or file)
    list = keepIfAny(list, d => !d.dead);
    // one entry per hoster, language and weight class (Full HD and up / lighter): the best resolution, then bitrate, else
    // the first. The lighter one stays because boxes like a Fire TV stall on the big stream.
    const heavy = d => (heightOf(d) > 720 ? 'big' : 'light');
    const better = (a, b) => heightOf(a) - heightOf(b) || a.bandwidth - b.bandwidth;
    const first = {};
    list.forEach(d => {
        const key = `${d.lang}|${d.host}|${heavy(d)}`;
        if (!first[key] || better(d, first[key]) > 0) first[key] = d;
    });
    list = list.filter(d => first[`${d.lang}|${d.host}|${heavy(d)}`] === d);
    list = keepIfAny(list, d => !(heightOf(d) > 0 && heightOf(d) <= MIN_HEIGHT));
    // best first: known resolutions descending, unknown ones behind them
    list = list.map((d, i) => [d, i]).sort(([a, i], [b, j]) => heightOf(b) - heightOf(a) || b.bandwidth - a.bandwidth || i - j).map(([d]) => d);
    const top = list.slice(0, MAX_STREAMS);
    // the cut must not leave only heavy streams: swap the last one for the best lighter one
    const light = list.find(d => heavy(d) === 'light' && heightOf(d) > 0);
    if (light && !top.includes(light) && top.every(d => heavy(d) === 'big')) top[top.length - 1] = light;
    return top;
}

// Runs worker over items a few at a time and stops starting new ones once enough streams are in: fewer requests,
// quicker answers, and fewer links spent on sites that count them. Requests already running always finish.
const BATCH = 5, ENOUGH = 6;

// Sites list hosters in any language order. German ones go first, or the early stop below could skip them.
const isGerman = text => /deutsch|german|\bde\b|\bger\b/i.test(text || '') && !/sub|untertitel|\but\b/i.test(text || '');
export const germanFirst = (items, text) => items.map((x, i) => [x, i]).sort(([a, i], [b, j]) => isGerman(text(a)) ? (isGerman(text(b)) ? i - j : -1) : (isGerman(text(b)) ? 1 : i - j)).map(([x]) => x);
export async function gather(items, worker, enough = ENOUGH) {
    const out = [];
    for (let i = 0; i < items.length && out.length < enough; i += BATCH) {
        const parts = await Promise.all(items.slice(i, i + BATCH).map(item => Promise.resolve().then(() => worker(item)).catch(() => [])));
        parts.forEach(p => out.push(...p));
    }
    return out;
}

export function provider(getStreams) {
    return {
        async getStreams(tmdbId, mediaType, season, episode) {
            deadline = Date.now() + DEADLINE_MS;
            // Nuvio's "Test provider" passes 'series' where playback passes 'tv'
            if (mediaType !== 'movie') mediaType = 'tv';
            let streams = [];
            try {
                streams = (await getStreams(tmdbId, mediaType, season, episode)) || [];
            } catch (e) {
                console.error(e.message);
            }
            if (running) await new Promise(resolve => idle.push(resolve));
            const list = streams.map(decorate);
            if (!list.length) return [];
            await addDetails(list);
            if (running) await new Promise(resolve => idle.push(resolve));
            return shape(list).map(d => d.stream);
        },
    };
}

export async function request(url, opts = {}) {
    const res = await send(url, Object.assign({}, opts, { headers: Object.assign({ 'User-Agent': UA }, opts.headers) }));
    if (!res.ok) throw new Error(`HTTP ${res.status} ${url}`);
    return res;
}

export const getText = async (url, opts) => (await request(url, opts)).text();

export const getJson = async (url, opts) => JSON.parse(await getText(url, opts));

export const postJson = (url, body, opts = {}) => getJson(url, Object.assign({}, opts, {
    method: 'POST',
    body: JSON.stringify(body),
    headers: Object.assign({ 'Content-Type': 'application/json' }, opts.headers),
}));

export const postForm = (url, form, opts = {}) => getText(url, Object.assign({}, opts, {
    method: 'POST',
    body: Object.keys(form).map(k => encodeURIComponent(k) + '=' + encodeURIComponent(form[k])).join('&'),
    headers: Object.assign({ 'Content-Type': 'application/x-www-form-urlencoded' }, opts.headers),
}));

export function absolute(url, base) {
    if (!url) return url;
    if (url.startsWith('//')) return 'https:' + url;
    if (/^https?:/.test(url)) return url;
    const origin = base.match(/^https?:\/\/[^/]+/)[0];
    return url.startsWith('/') ? origin + url : base.replace(/[^/]*$/, '') + url;
}
