import { getText, postForm, absolute, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';
import { load, all } from '../../shared/dom.js';
import { resolveEmbed } from '../../shared/extractors/index.js';

const BASE = 'https://flixitv-stream.eu';

async function search(query) {
    // rate limits are per PHP session, so every search gets a fresh one
    const sid = Math.random().toString(36).slice(2) + Date.now().toString(36);
    const $ = load(await postForm(`${BASE}/search/`, { srh: query.slice(0, 20) }, { headers: { Cookie: `PHPSESSID=${sid}` } }));
    return all($, 'a.card-link').map(a => {
        const name = a.find('h5').text().trim();
        return {
            title: name.replace(/\s*\([^)]*\)\s*$/, ''),
            year: (name.match(/\((\d{4})/) || [])[1],
            url: absolute(a.attr('href'), BASE + '/'),
            series: /Serie\s*$/.test(a.text().trim()),
        };
    });
}

async function hubu(url) {
    const src = ((await getText(url)).match(/<source[^>]+src="([^"]+)"/) || [])[1];
    return src ? [{ url: src, quality: 'auto', host: 'hubu.cloud' }] : [];
}

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const tv = mediaType === 'tv';
        const meta = await getMeta(tmdbId, mediaType);
        let hit = null;
        // site titles are short ("Terminator 2 (1991)") and the search field takes max 20 chars, so also try the part before a subtitle
        const queries = meta.titles.concat(meta.titles.map(t => t.split(/\s[-–:]\s|:\s/)[0]))
            .map(q => q.trim().slice(0, 20)).filter((q, i, a) => q.length >= 3 && a.indexOf(q) === i);
        for (const q of queries) {
            hit = pickBest((await search(q)).filter(i => tv ? i.series : /\/watch/.test(i.url)), meta);
            if (hit) break;
        }
        if (!hit) return [];
        let watch = hit.url.replace('/watch?', '/watch/?');
        if (tv) {
            const $s = load(await getText(hit.url.replace('/serie?', '/serie/?')));
            const seasons = all($s, 'a.card-link');
            const link = seasons.find(a => a.text().trim() === `Staffel ${season}`) || seasons[season - 1];
            if (!link) return [];
            const $e = load(await getText(absolute(link.attr('href'), `${BASE}/serie/`)));
            const row = all($e, 'tbody tr').find(r => r.find('td:nth-child(2)').text().trim() === String(episode));
            if (!row) return [];
            watch = absolute(row.find('td:first-child a').attr('href'), BASE + '/').replace('/watch?', '/watch/?');
        }
        const iframe = load(await getText(watch))('iframe').first().attr('src');
        if (!iframe) return [];
        const streams = /hubu\.cloud/.test(iframe) ? await hubu(iframe) : await resolveEmbed(iframe, BASE + '/');
        return streams.map(s => ({ name: 'FlixiTV', title: `${s.host} · Deutsch`, url: s.url, quality: s.quality, headers: s.headers }));
    } catch (e) {
        console.error(`[FlixiTV] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
