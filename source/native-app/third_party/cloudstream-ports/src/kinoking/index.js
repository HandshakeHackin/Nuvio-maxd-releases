import { getText, provider, gather } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';
import { load, all } from '../../shared/dom.js';
import { resolveEmbed, byHoster } from '../../shared/extractors/index.js';
import { meineCloud } from './meinecloud.js';

const BASE = 'https://kinoking.cc';

const resolve = url => (/meinecloud\.click/.test(url) ? meineCloud(url, BASE + '/') : resolveEmbed(url, BASE + '/')).catch(() => []);

// The site sometimes takes 15-20 s per page and Nuvio TV cannot abort a request. When the search was already that slow,
// the episode page is not worth a second wait: giving up frees the plugin slot instead of holding it for 35 s and more.
const SLOW_MS = 8000;

async function findEntry(meta) {
    const t0 = Date.now();
    for (const q of meta.titles) {
        if (Date.now() - t0 > SLOW_MS) break;
        const $ = load(await getText(`${BASE}/index.php?search=${encodeURIComponent(q)}`));
        const items = all($, 'main div#ajax-grid-container > div')
            .map(e => ({ id: e.attr('data-id'), type: e.attr('data-type'), tmdb: e.attr('data-tmdb'), title: e.attr('data-title') }))
            .filter(i => i.id && i.type === 'series');
        const hit = items.find(i => i.tmdb === meta.tmdbId) || pickBest(items.filter(i => !i.tmdb), meta);
        if (hit) return Date.now() - t0 > SLOW_MS ? null : hit;
    }
    return null;
}

// Series only: a film's page alone takes 12-20 s to answer (it looks its mirrors up live), and those mirrors are huhu.to's.
async function getStreams(tmdbId, mediaType, season, episode) {
    if (mediaType !== 'tv') return [];
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const entry = await findEntry(meta);
        if (!entry) return [];
        const html = await getText(`${BASE}/series.php?id=${entry.id}`);
        const json = (html.match(/const allEpisodesData = (.*)/) || [])[1];
        const ep = JSON.parse(json.replace(/;\s*$/, '')).find(e => +e.season_number === season && +e.episode_number === episode);
        const embeds = (ep ? (ep.video_links || '').split(',') : []).map(e => e.trim()).filter((e, i, a) => e && a.indexOf(e) === i);
        const streams = await gather(byHoster(embeds), resolve);
        return streams.map(s => ({
            name: 'KinoKing',
            title: `${s.host} · Deutsch · ${s.quality}`,
            url: s.url,
            quality: s.quality,
            headers: s.headers,
        }));
    } catch (e) {
        console.error(`[KinoKing] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
