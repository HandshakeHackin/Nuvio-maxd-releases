import { getText, getJson, provider } from '../../shared/http.js';

const BASE = 'https://www.southpark.de';
const TOPAZ = 'https://topaz.paramount.tech/topaz/api';

async function getStreams(tmdbId, mediaType, season, episode) {
    // the site only hosts South Park itself (TMDB tv 2190); its season/episode numbering matches TMDB
    if (String(tmdbId) !== '2190' || mediaType !== 'tv' || !season) return [];
    try {
        // season 1 is the default /seasons/south-park page, the others are linked from it as .../<id>/staffel-N
        let path = '/seasons/south-park';
        if (season !== 1) {
            const m = (await getText(BASE + path)).match(new RegExp(`/seasons/south-park/[a-z0-9]+/staffel-${season}"`));
            if (!m) return [];
            path = m[0].slice(0, -1);
        }
        const mgid = (await getText(BASE + path)).match(/mgid:arc:season:southpark\.intl:[0-9a-f-]+/);
        if (!mgid) return [];
        const items = (await getJson(`${BASE}/api/context/${encodeURIComponent(mgid[0])}/episode/1/100`)).items || [];
        const ep = items.find(i => {
            const h = i.meta.header.title;
            const m = String((h && h.text) || h).match(/S(\d+)\D+(\d+)/);
            return m && Number(m[1]) === season && Number(m[2]) === episode;
        });
        if (!ep) return [];

        const streams = [];
        for (const [ns, lang] of [['de', 'Deutsch'], ['en', 'Englisch']]) {
            const res = await getJson(`${TOPAZ}/mgid:arc:episode:shared.southpark.gsa.${ns}:${ep.id}/mica.json?clientPlatform=mobile`);
            const src = res.stitchedstream && res.stitchedstream.source;
            if (src) streams.push({ name: 'South Park', title: `S${season}E${episode} ${ep.meta.subHeader || ''} · ${lang} · HLS`, url: src, quality: 'auto' });
        }
        return streams;
    } catch (e) {
        console.error(`[South Park] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
