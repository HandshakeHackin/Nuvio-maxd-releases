import { getJson, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest } from '../../shared/match.js';

const uuid = () => 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, c => ((Math.random() * 16) | (c === 'y' ? 8 : 0)).toString(16).slice(-1));

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        // region (DE) comes from the client's geo-IP
        const boot = await getJson(`https://boot.pluto.tv/v4/start?appName=web&appVersion=9.22.0&deviceVersion=142.0.0&deviceModel=web&deviceMake=firefox&clientID=${uuid()}&clientModelNumber=1.0.0`);
        const servers = boot.servers;
        const headers = { Authorization: `Bearer ${boot.sessionToken}` };
        const type = mediaType === 'tv' ? 'series' : 'movie';

        for (const q of meta.titles) {
            const found = ((await getJson(`${servers.search}/v1/search?q=${encodeURIComponent(q)}&limit=100`, { headers })).data || [])
                .filter(r => r.type === type);
            if (!found.length) continue;
            const items = (await getJson(`${servers.vod}/v4/vod/items?ids=${found.map(r => r.id).join(',')}`, { headers }))
                .map(i => ({ title: i.name, year: ((i.clip || {}).originalReleaseDate || '').slice(0, 4) || null, id: i._id, stitched: i.stitched }));
            const hit = pickBest(items, meta);
            if (!hit) continue;

            let stitched = hit.stitched;
            if (type === 'series') {
                const info = await getJson(`${servers.vod}/v4/vod/series/${hit.id}/seasons`, { headers });
                const s = (info.seasons || []).find(s => s.number === season);
                const ep = s && s.episodes.find(e => e.number === episode);
                if (!ep) return [];
                stitched = ep.stitched;
            }
            if (!stitched || !stitched.path || (stitched.type && stitched.type !== 'hls')) return [];
            // masterJWTPassthrough makes the stitcher append the jwt to variant URLs, so players that
            // don't forward headers to sub-playlists still work
            return [{
                name: 'PlutoTV',
                title: `${hit.title} · Deutsch · HLS`,
                url: `${servers.stitcher}/v2${stitched.path}?jwt=${boot.sessionToken}&masterJWTPassthrough=true`,
                quality: 'auto',
                headers,
            }];
        }
    } catch (e) {
        console.error(`[PlutoTV] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
