import { getText, postJson, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { resolveAll } from '../../shared/extractors/index.js';

const CLOUD = 'https://meinecloud.click';

// hdfilme (now hdfilme.cafe, its own search is broken server-side) embeds every title as a
// meinecloud.click player keyed by IMDb id (/movie/<imdb>, /serial/<imdb>), so we go there directly.
async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        if (!meta.imdbId) return [];
        const page = `${CLOUD}/${mediaType === 'tv' ? 'serial' : 'movie'}/${meta.imdbId}`;
        const html = await getText(page, { headers: { Referer: 'https://hdfilme.cafe/' } });
        const m = html.match(/type:\s*['"](\w+)['"],\s*id:\s*"([^"]+)",\s*token:\s*"([^"]+)"/);
        if (!m) return [];
        const res = await postJson(`${CLOUD}/api/embed-links`, { type: m[1], id: m[2], token: m[3] }, { headers: { Referer: page, Origin: CLOUD } });
        let links;
        if (mediaType === 'tv') {
            const s = ((res.tv || {}).seasons || []).find(x => +x.season_number === season) || {};
            links = (s.episodes || []).filter(e => +e.episode_number === episode).map(e => e.url);
        } else {
            links = (res.sources || []).map(s => s.url);
        }
        const streams = await resolveAll(links.filter(Boolean), page);
        return streams.map(s => ({
            name: 'HDFilme',
            title: `${s.host} · Deutsch${s.quality !== 'auto' ? ' · ' + s.quality : ''}`,
            url: s.url,
            quality: s.quality,
            headers: s.headers,
        }));
    } catch (e) {
        console.error(`[HDFilme] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
