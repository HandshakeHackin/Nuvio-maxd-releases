import { getText, getJson, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';

const API = 'https://api.arte.tv/api/emac/v4/de/web';

// Arte assembles pages via ESI; a rate-limited include is inlined as "\n429 - Too Many Requests\n", breaking the JSON
const getPage = async url => JSON.parse((await getText(url)).replace(/\n\d{3} - [^\n]*\n/g, 'null'));

// zone content, fetched directly (all pages) when missing from the page or paginated
async function zoneItems(zone, query, maxPages) {
    const c = zone.content;
    if (c && (!c.pagination || c.pagination.pages <= 1 || maxPages === 1)) return c.data;
    let items = [];
    for (let page = 1, pages = 1; page <= pages && page <= maxPages; page++) {
        const r = await getJson(`${API}/zones/${zone.id.split('_')[0]}/content?${query}&page=${page}`);
        items = items.concat(r.data || []);
        pages = (r.pagination || {}).pages || 1;
    }
    return items;
}

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const isTv = mediaType === 'tv';
        let hit;
        for (const q of meta.titles) {
            const query = `query=${encodeURIComponent(q)}`;
            const zones = (await getPage(`${API}/pages/SEARCH/?page=1&${query}`)).zones || [];
            const listing = zones.find(z => z.code === 'listing_SEARCH') || zones[0];
            const items = listing ? (await zoneItems(listing, `authorizedCountry=DE&${query}`, 1))
                .filter(i => i.programId && i.programId.startsWith('RC-') === isTv && !['TOPIC', 'TRAILER'].includes(i.kind.code)) : [];
            hit = pickBest(items, meta);
            if (hit) break;
        }
        if (!hit) return [];

        let programId = hit.programId;
        if (isTv) {
            const seasons = ((await getPage(`${API}/collections/${programId}`)).zones || []).filter(z => z.id.split('_').length === 3);
            const zone = seasons.find(z => Number((z.slug || '').split('-').pop()) === season) || (seasons.length === 1 && season === 1 && seasons[0]);
            if (!zone) return [];
            const [, collectionId, subCollectionId] = zone.id.split('_');
            const eps = await zoneItems(zone, `collectionId=${collectionId}&subCollectionId=${subCollectionId}`, 20);
            const ep = eps.find(e => (e.episodeInfo || {}).episode === episode)
                || eps.find(e => Number((e.title.match(/\((\d+)\/\d+\)$/) || [])[1]) === episode);
            if (!ep) return [];
            programId = ep.programId;
        }

        const attrs = (await getJson(`https://api.arte.tv/api/player/v2/config/de/${programId}`)).data.attributes;
        return (attrs.streams || [])
            .map(s => ({ s, v: s.versions[0] || {} }))
            .sort((a, b) => (b.v.audioLanguage === 'de') - (a.v.audioLanguage === 'de'))
            .map(({ s, v }) => ({
                name: 'Arte',
                title: `${attrs.metadata.title}${attrs.metadata.subtitle ? ' - ' + attrs.metadata.subtitle : ''} · ${v.audioLanguage === 'de' ? 'Deutsch' : v.shortLabel || v.label} · HLS`,
                url: s.url,
                quality: 'auto', // multi-bitrate master, mainQuality understates it
            }));
    } catch (e) {
        console.error(`[Arte] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
