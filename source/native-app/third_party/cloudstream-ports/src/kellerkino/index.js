import { getJson, getText, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';
import { load, all } from '../../shared/dom.js';
import { resolveAll } from '../../shared/extractors/index.js';

const BASE = 'https://www.kellerkino.com';

async function getStreams(tmdbId, mediaType) {
    if (mediaType !== 'movie') return [];
    try {
        const meta = await getMeta(tmdbId, mediaType);
        for (const q of meta.titles) {
            // the HTML search (?s=) sits behind a Cloudflare challenge, the WordPress REST search does not (no year, so exact titles only)
            const results = await getJson(`${BASE}/wp-json/wp/v2/search?search=${encodeURIComponent(q)}&per_page=20`);
            const items = results.filter(r => r.subtype === 'post').map(r => ({
                title: String(r.title).replace(/&#(\d+);/g, (_, n) => String.fromCharCode(n)).replace(/&amp;/g, '&'),
                url: r.url,
            }));
            const hit = pickBest(items, meta);
            if (!hit) continue;
            const page = load(await getText(hit.url));
            const imdb = page('article.movie-detail .nfo-movie-imdb-id').text().replace(/^[:\s]+/, '').trim();
            if (imdb && meta.imdbId && imdb !== meta.imdbId) continue;
            const embeds = all(page, 'iframe').map(e => e.attr('src')).filter(Boolean);
            return (await resolveAll(embeds, BASE + '/')).map(s => ({
                name: 'KellerKino',
                title: `${s.host} · Deutsch · ${s.quality}`,
                url: s.url,
                quality: s.quality,
                headers: s.headers,
            }));
        }
    } catch (e) {
        console.error(`[KellerKino] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
