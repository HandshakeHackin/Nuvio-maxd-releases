import { getJson, getText, provider, gather, germanFirst } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';
import { resolveEmbed } from '../../shared/extractors/index.js';

const BASE = 'https://moflix-stream.xyz';
// the API answers 401 without a same-site Referer
const api = path => getJson(`${BASE}/api/v1/${path}`, { headers: { Referer: `${BASE}/` } });

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const isSeries = mediaType === 'tv';
        let hit = null;
        for (const q of meta.titles) {
            // the search API sits behind a Cloudflare challenge, the search page renders the same results into bootstrapData
            const html = await getText(`${BASE}/search/${encodeURIComponent(q)}`);
            const data = JSON.parse((html.match(/window\.bootstrapData\s*=\s*(\{[\s\S]*?\});\s*<\/script>/) || [])[1] || '{}');
            const items = ((((data.loaders || {}).searchPage) || {}).results || [])
                .filter(r => r.model_type === 'title' && !!r.is_series === isSeries)
                .map(r => ({ id: r.id, title: r.name, year: r.year, tmdb: String(r.tmdb_id) }));
            hit = items.find(i => i.tmdb === meta.tmdbId) || pickBest(items, meta);
            if (hit) break;
        }
        if (!hit) return [];
        const videos = isSeries
            ? ((await api(`titles/${hit.id}/seasons/${season}/episodes/${episode}?loader=episodePage`)).episode || {}).videos
            : (await api(`titles/${hit.id}?loader=titlePage`)).title.videos;
        // "Premium (No Ads)" links serve the master playlist but 403 every variant without a premium login
        const full = (videos || []).filter(v => v.src && /^full$/i.test(v.category || '') && !/premium/i.test(v.name || ''));
        return gather(germanFirst(full, v => v.language || 'de'), async v => {
            const lang = (v.language || 'de').toUpperCase();
            // "stream" entries are the site's own direct HLS/MP4 links; everything else is a hoster embed
            const links = v.type === 'stream'
                ? [{ url: v.src, quality: v.quality, host: v.name, headers: { Referer: `${BASE}/` } }]
                : await resolveEmbed(v.src, `${BASE}/`);
            return links.map(s => ({
                name: 'Moflix',
                title: `${s.host} · ${lang} · ${v.quality || s.quality || 'auto'}`,
                url: s.url,
                quality: (`${s.quality} ${v.quality}`.match(/\d{3,4}p/) || ['auto'])[0],
                headers: s.headers,
            }));
        });
    } catch (e) {
        console.error(`[Moflix] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
