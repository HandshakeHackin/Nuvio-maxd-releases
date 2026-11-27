import { getText, provider, gather } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';
import { load, all } from '../../shared/dom.js';
import { resolveEmbed, byHoster } from '../../shared/extractors/index.js';

const BASE = 'https://filmpalast.to';
const pad = n => String(n).padStart(2, '0');

// series are listed as one entry per episode, titled "<Show> S01E02"
async function findPage(meta, season, episode) {
    const tag = meta.type === 'tv' ? ` S${pad(season)}E${pad(episode)}` : '';
    for (const t of meta.titles) {
        const $ = load(await getText(`${BASE}/search/title/${encodeURIComponent(t + tag)}`));
        const items = all($, '#content .glowliste h2 a').map(a => {
            const m = a.attr('title').match(/^(.*) S(\d+)E(\d+)$/);
            return { title: m ? m[1] : a.attr('title'), s: m && +m[2], e: m && +m[3], url: 'https:' + a.attr('href').replace(/^https?:/, '') };
        }).filter(i => (tag ? i.s === season && i.e === episode : !i.s));
        const hit = pickBest(items, meta);
        if (hit) return hit.url;
    }
    return null;
}

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const url = await findPage(meta, season, episode);
        if (!url) return [];
        const $ = load(await getText(url));
        const mirrors = all($, '#content ul.currentStreamLinks').map(ul => {
            const a = $('a.iconPlay', ul).first();
            return { host: $('.hostName', ul).text().trim(), link: a.attr('data-player-url') || a.attr('href') };
        }).filter(m => /^https?:/.test(m.link || ''));
        const streams = await gather(byHoster(mirrors, m => `${m.host} ${m.link}`), m => resolveEmbed(m.link, BASE + '/').then(r => r.map(s => ({
            name: 'FilmPalast',
            title: `${s.host || m.host} · Deutsch${s.quality !== 'auto' ? ' · ' + s.quality : ''}`,
            url: s.url,
            quality: s.quality,
            headers: s.headers,
        }))));
        return streams;
    } catch (e) {
        console.error(`[FilmPalast] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
