import { getJson, postJson } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest, norm } from '../../shared/match.js';

const API = 'https://public.aurora.enhanced.live';

// Discovery/WBD "Aurora" sites (DMAX, TELE 5, TLC) share one API, differing only in environment/slug/realm.
export function aurora({ name, mainUrl, serviceIdentifier, mediathekSlug, apiTokenRealm }) {
    const env = `filter%5Benvironment%5D=${serviceIdentifier}&v=2&include=default`;

    async function findPage(meta) {
        // the tagline can also sit on the TMDB side only ("Steel Buddies – Stahlharte Geschäfte" vs. the site's "Steel Buddies"),
        // so every tagline-separated part of a TMDB title counts as a title of its own
        const parts = [].concat(...meta.titles.map(t => t.split(/ [-–:] |: /))).map(p => p.trim()).filter(p => p.length > 3);
        meta = Object.assign({}, meta, { titles: meta.titles.concat(parts).filter((t, i, a) => a.indexOf(t) === i) });
        // search is a substring match that silently drops punctuation like ':' or "'" from the query, so "Star Trek: Picard"
        // finds nothing; retry with each title's longest punctuation-free run, then its longest word
        const longest = re => meta.titles.map(t => t.split(re).map(x => x.trim()).sort((a, b) => b.length - a.length)[0]);
        const queries = meta.titles.concat(longest(/[^\wäöüß -]+/i), longest(/[^\wäöüß]+/i));
        for (const q of queries.filter((q, i, a) => q && a.indexOf(q) === i)) {
            let res;
            try {
                res = await getJson(`${API}/site/search/page/?q=${encodeURIComponent(q)}&${env}&filter%5Btype%5D=showpage&page%5Bsize%5D=50`);
            } catch (e) {
                continue; // broad queries can hit a 504 upstream timeout
            }
            // datePublished is the CMS page date, not the release year, so match on title only;
            // titles often carry a German tagline ("The Last Woodsmen - Holzfäller am Limit"), so fall back to the part before it
            const items = (res.data || []).map(d => ({ title: d.title, slug: d.slug }));
            const hit = pickBest(items, meta) || pickBest(items.map(i => ({ title: i.title.split(/ [-–:] /)[0], slug: i.slug })), meta);
            if (hit) return getJson(`${API}/site/page/${hit.slug}/?${env}&parent_slug=${mediathekSlug}`);
        }
        return null;
    }

    async function getStreams(tmdbId, mediaType, season, episode) {
        try {
            const meta = await getMeta(tmdbId, mediaType);
            const page = await findPage(meta);
            if (!page) return [];
            const blocks = page.blocks || [];
            let videoId, label;
            if (mediaType === 'tv') {
                const show = blocks.find(b => b.showId);
                const ep = show && (show.items || []).find(i => i.id && Number(i.seasonNumber) === Number(season) && Number(i.episodeNumber) === Number(episode));
                if (ep) { videoId = ep.id; label = `S${season}E${episode} ${ep.title || ''}`.trim(); }
            } else {
                const v = blocks.find(b => b.videoId && norm(b.title) === norm(page.title));
                if (v) { videoId = v.videoId; label = page.title; }
            }
            if (!videoId) return [];

            const token = (await getJson(`${API}/token?realm=${apiTokenRealm}`)).data.attributes.token;
            const info = await postJson(`${API}/playback/v3/videoPlaybackInfo`, {
                videoId,
                deviceInfo: { adBlocker: false, drmSupported: false, hdrCapabilities: ['SDR'], hwDecodingCapabilities: [], soundCapabilities: ['STEREO'] },
                wisteriaProperties: {},
            }, { headers: { Authorization: `Bearer ${token}`, Referer: `${mainUrl}/` } });

            return info.data.attributes.streaming
                .filter(s => s.type === 'hls' && !(s.protection && (s.protection.drmEnabled || s.protection.clearkeyEnabled)))
                .map(s => ({ name, title: `${label} · Deutsch · HLS`, url: s.url, quality: 'auto' }));
        } catch (e) {
            console.error(`[${name}] ${e.message}`);
        }
        return [];
    }

    return getStreams;
}
