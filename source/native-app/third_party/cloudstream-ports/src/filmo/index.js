import { getText, request, UA, send, provider, gather, germanFirst } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { norm, pickBest } from '../../shared/match.js';
import { load, all } from '../../shared/dom.js';
import { resolveEmbed } from '../../shared/extractors/index.js';

const BASE = 'https://filmo.to';

async function moviePage(url) {
    const res = await request(url);
    const cookies = {};
    String(res.headers.get('set-cookie') || '').replace(/(?:^|[,;]\s*)(XSRF-TOKEN|filmo-session)=([^;,\s]+)/g, (m, k, v) => { cookies[k] = v; });
    const $ = load(await res.text());
    const year = all($, 'div.details-group dl').map(dl => dl.find('dt').text().trim() === 'Erscheinungsdatum' ? dl.find('dd').text().trim().slice(0, 4) : '').find(Boolean);
    return { url, $, cookies, year };
}

// chip payload -> slug (POST /n) -> /n/<slug> either 302s to the hoster or shows a "leaving Filmo" page with the link
async function embedUrl(payload, cookies) {
    const cookie = Object.keys(cookies).map(k => `${k}=${cookies[k]}`).join('; ');
    const headers = { 'User-Agent': UA, Cookie: cookie };
    const slug = JSON.parse(await getText(`${BASE}/n`, {
        method: 'POST',
        body: JSON.stringify({ p: payload }),
        headers: Object.assign({ 'Content-Type': 'application/json', 'X-XSRF-TOKEN': decodeURIComponent(cookies['XSRF-TOKEN'] || '') }, headers),
    })).x;
    // follow instead of reading the 30x itself, see serienstream/common.js
    const url = `${BASE}/n/${slug}`;
    const res = await send(url, { headers });
    if (res.url && res.url !== url) return res.url;
    return ((await res.text()).match(/<a class="open" href="([^"]+)"/) || [])[1];
}

async function getStreams(tmdbId, mediaType) {
    if (mediaType !== 'movie') return [];
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const titles = meta.titles.map(norm);
        let hit = null;
        for (const q of meta.titles) {
            const $ = load(await getText(`${BASE}/search?q=${encodeURIComponent(q)}`));
            const urls = all($, 'section.search-top-results article > a, a.movie-poster-grid-card')
                .filter(a => titles.includes(norm(a.find('[class*=__title]').text())))
                .map(a => a.attr('href'))
                .filter((u, i, arr) => u && arr.indexOf(u) === i)
                .slice(0, 4);
            const pages = await Promise.all(urls.map(u => moviePage(u.startsWith('http') ? u : BASE + u).catch(() => null)));
            hit = pickBest(pages.filter(Boolean).map(p => Object.assign(p, { title: p.$('.primary-container h1').text().trim() })), meta);
            if (hit) break;
        }
        if (!hit) return [];
        const seen = {};
        const chips = all(hit.$, '.provider-chip').filter(c => {
            const id = c.attr('data-movie-link-id') || c.attr('data-p');
            return seen[id] ? false : (seen[id] = true);
        });
        return gather(germanFirst(chips, c => c.text()), async chip => {
            const label = chip.text().replace(/\s+/g, ' ').trim();
            try {
                const embed = await embedUrl(chip.attr('data-p'), hit.cookies);
                return (await resolveEmbed(embed, BASE + '/')).map(s => {
                    const q = s.quality !== 'auto' ? s.quality : ((label.match(/\d{3,4}p/) || [])[0] || 'auto');
                    return { name: 'Filmo', title: `${s.host} · ${label}`, url: s.url, quality: q, headers: s.headers };
                });
            } catch (e) {
                console.error(`[Filmo] ${label}: ${e.message}`);
                return [];
            }
        });
    } catch (e) {
        console.error(`[Filmo] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
