import { getText, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';

const BASE = 'https://filmfrei24.com';

// the catalog is inlined as `const ALL_FILMS = [...];` (one line); ids are TMDB ids
function catalog(html) {
    let s = html.slice(html.indexOf('const ALL_FILMS'));
    s = s.slice(s.indexOf('=') + 1);
    return JSON.parse(s.slice(0, s.indexOf('\n')).trim().replace(/;$/, ''));
}

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const tv = mediaType === 'tv';
        const films = catalog(await getText(BASE + (tv ? '/serien/' : '/')));
        let hit = films.find(f => String(f.id) === String(tmdbId));
        if (!hit) hit = pickBest(films, await getMeta(tmdbId, mediaType));
        if (!hit) return [];
        let url = hit.video;
        if (tv) {
            if (!hit._series_id || !season || !episode) return [];
            const html = await getText(`${BASE}/player/?s=${encodeURIComponent(hit._series_id)}&type=series&season=${season}&ep=${episode}`);
            url = (html.match(/<video[^>]*data-src="([^"]+)"/) || [])[1];
        }
        if (!url || !/^https?:/.test(url)) return [];
        return [{
            name: 'FilmFrei24',
            title: `${hit.language || 'Deutsch'} · ${/\.m3u8/.test(url) ? 'HLS' : 'MP4'}`,
            url,
            quality: 'auto',
            headers: { Referer: BASE + '/' },
        }];
    } catch (e) {
        console.error(`[FilmFrei24] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
