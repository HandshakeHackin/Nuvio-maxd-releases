import { request, getText, postForm, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';
import { load, all } from '../../shared/dom.js';
import { resolveAll } from '../../shared/extractors/index.js';

let base = null;

// megakino changes its TLD almost daily; megakino4.to redirects to the current one. Pages need the yg_token cookie.
async function session() {
    if (!base) base = (await request('https://megakino4.to/')).url.replace(/\/+$/, '');
    const cookie = ((await request(`${base}/index.php?yg=token`)).headers.get('set-cookie') || '').match(/yg_token=[^;,\s]+/);
    return { Cookie: cookie ? cookie[0] : '' };
}

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const headers = await session();
        let hit = null;
        for (const q of meta.titles) {
            const $ = load(await postForm(base, { do: 'search', subaction: 'search', story: q }, { headers }));
            const items = all($, 'a.poster.grid-item').map(a => {
                const title = $('h3', a).text().trim();
                const m = title.match(/^(.*?) - Staffel (\d+)$/);
                return { title: m ? m[1] : title, s: m && +m[2], year: ($('.poster__subtitle li', a).first().text().match(/\d{4}/) || [])[0], href: a.attr('href') };
            });
            // a season's year is its own air year, not the show's, so only titles are compared for series
            hit = mediaType === 'tv'
                ? pickBest(items.filter(i => i.s === season).map(i => ({ title: i.title, href: i.href })), meta)
                : pickBest(items.filter(i => !i.s), meta);
            if (hit) break;
        }
        if (!hit) return [];
        const $ = load(await getText(base + hit.href.replace(/^https?:\/\/[^/]+/, ''), { headers }));
        const links = mediaType === 'tv'
            ? $(`select[id="ep${episode}"] option`).map((i, o) => $(o).attr('value')).get()
            : $('div.pmovie__player iframe').map((i, f) => $(f).attr('src') || $(f).attr('data-src')).get();
        const streams = await resolveAll(links.filter(l => /^(https?:)?\/\//.test(l || '')), base + '/');
        return streams.map(s => ({
            name: 'Megakino',
            title: `${s.host} · Deutsch${s.quality !== 'auto' ? ' · ' + s.quality : ''}`,
            url: s.url,
            quality: s.quality,
            headers: s.headers,
        }));
    } catch (e) {
        console.error(`[Megakino] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
