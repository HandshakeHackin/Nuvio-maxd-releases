import { getJson, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';

const API = 'https://api.netzkino.de.simplecache.net/capi-2.0a';

async function getStreams(tmdbId, mediaType) {
    if (mediaType !== 'movie') return [];
    try {
        const meta = await getMeta(tmdbId, mediaType);
        // the API's search is word-based and misses many full titles, so also try each title's longest word
        const words = meta.titles.map(t => t.split(/[^\wäöüß]+/i).sort((a, b) => b.length - a.length)[0]);
        for (const q of meta.titles.concat(words).filter((q, i, a) => q && a.indexOf(q) === i)) {
            const posts = (await getJson(`${API}/search?q=${encodeURIComponent(q)}&d=www`)).posts || [];
            const items = posts.map(p => {
                const f = p.custom_fields || {};
                return { title: p.title, year: (f.Jahr || [])[0], imdb: ((f['IMDb-Link'] || [])[0] || '').split('/').pop(), streams: f.Streaming || [] };
            });
            const hit = (meta.imdbId && items.find(i => i.imdb === meta.imdbId)) || pickBest(items, meta);
            if (hit) {
                return hit.streams.map(slug => ({
                    name: 'Netzkino',
                    title: `${hit.title} · MP4`,
                    url: `https://pmd.netzkino-seite.netzkino.de/${slug}.mp4`,
                    quality: '720p',
                    headers: { Referer: 'https://www.netzkino.de/' },
                }));
            }
        }
    } catch (e) {
        console.error(`[Netzkino] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
