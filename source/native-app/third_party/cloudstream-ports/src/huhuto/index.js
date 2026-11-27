import { request, provider, gather } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { resolveEmbed, byHoster } from '../../shared/extractors/index.js';

const BASE = 'https://huhu.to';
const LANG = { de: 'Deutsch', en: 'Englisch', fr: 'Französisch', es: 'Spanisch', ja: 'Japanisch', jp: 'Japanisch' };

// huhu.to's old /web-vod API is gone; the site is now a MediaURL addon keyed by TMDB/IMDb ids
async function sources(meta, season, episode) {
    const body = { language: 'de', region: 'DE', clientVersion: '3.1.0', type: meta.type === 'tv' ? 'series' : 'movie', ids: { tmdb_id: meta.tmdbId, imdb_id: meta.imdbId }, name: meta.title };
    if (meta.type === 'tv') body.episode = { season, episode };
    const res = await request(`${BASE}/mediaurl-source.json`, {
        method: 'POST',
        body: JSON.stringify(body),
        headers: { 'Content-Type': 'application/json; charset=utf-8', 'User-Agent': 'MediaUrl/2' },
    });
    return res.json();
}

// HuhuToExtractor: huhu.to links only redirect to the real hoster
async function resolve(src) {
    let url = src.url;
    if (url.includes('huhu.to/')) url = (await request(url)).url;
    return (await resolveEmbed(url, `${BASE}/`)).map(s => ({
        name: 'Huhu',
        title: `${s.host} · ${(src.languages || ['de']).map(l => LANG[l] || l.toUpperCase()).join('/')}${src.tag ? ' · ' + src.tag : ''}`,
        url: s.url,
        quality: /\d/.test(s.quality) ? s.quality : ((src.tag || '').match(/\d{3,4}p/) || ['auto'])[0],
        headers: s.headers,
    }));
}

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const german = s => ((s.languages || ['de']).includes('de') ? 0 : 1);
        // German sources first, within them the more reliable hosters first (both sorts are stable)
        const list = byHoster((await sources(meta, season, episode)).filter(s => s.type === 'url' && s.url), s => s.url).sort((a, b) => german(a) - german(b));
        return gather(list, resolve);
    } catch (e) {
        console.error(`[Huhu] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
