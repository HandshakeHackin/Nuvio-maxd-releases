import { getJson, provider } from '../../shared/http.js';
import { getMeta } from '../../shared/tmdb.js';
import { pickBest, norm } from '../../shared/match.js';

const API = 'https://api.ardmediathek.de';
const ACCESSIBLE = /Audiodeskription|Gebärdensprache|Klare Sprache/i;
const LANGS = { deu: 'Deutsch', eng: 'Englisch', fra: 'Französisch', ov: 'OV' };

const title = t => (t.shortTitle || t.longTitle || t.title || '').trim();
const targetId = t => ((t.links || {}).target || {}).id || t.id;
const search = (kind, q) => getJson(`${API}/search-system/search/${kind}/ard?query=${encodeURIComponent(q)}&pageSize=30&platform=MEDIA_THEK&sortingCriteria=SCORE_DESC`)
    .then(r => r.teasers || []);

async function findShow(meta, types) {
    for (const q of meta.titles) {
        const shows = (await search('shows', q)).filter(t => types.test(t.coreAssetType || ''));
        const hit = pickBest(shows.map(t => ({ title: title(t), id: targetId(t) })), meta);
        if (hit) return hit;
    }
    return null;
}

// all episode teasers of a grouping's itemsOf* widgets, following pagination via the widget's own self link
async function groupingTeasers(showId, compilation) {
    const page = await getJson(`${API}/page-gateway/pages/ard/grouping/${showId}?seasoned=true&embedded=true`);
    let widgets = (page.widgets || []).filter(w => w.compilationType === compilation);
    if (!widgets.length) widgets = (page.widgets || []).filter(w => /^itemsOf/.test(w.compilationType || ''));
    const out = [];
    for (const w of widgets) {
        let teasers = w.teasers || [];
        const self = ((w.links || {}).self || {}).href;
        if (self && w.pagination && teasers.length < w.pagination.totalElements) {
            teasers = [];
            for (let p = 0; teasers.length < Math.min(w.pagination.totalElements, 1000); p++) {
                const next = (await getJson(self.replace(/pageNumber=\d+/, `pageNumber=${p}`).replace(/pageSize=\d+/, 'pageSize=200'))).teasers || [];
                if (!next.length) break;
                teasers = teasers.concat(next);
            }
        }
        for (const t of teasers) {
            if ((t.coreAssetType === 'EPISODE' || /Originalversion|\(OV\)/.test(title(t))) && !ACCESSIBLE.test(title(t))) out.push(t);
        }
    }
    return out;
}

async function tmdbEpisodeName(tmdbId, season, episode) {
    try {
        return (await getJson(`https://api.themoviedb.org/3/tv/${tmdbId}/season/${season}/episode/${episode}?api_key=${globalThis.TMDB_API_KEY}&language=de-DE`)).name;
    } catch (e) {
        return null;
    }
}

async function findEpisodes(meta, season, episode) {
    const show = await findShow(meta, /SERIES$/);
    if (!show) return [];
    const teasers = await groupingTeasers(show.id, 'itemsOfSeason');
    const tag = new RegExp(`\\(S0*${season}/E0*${episode}\\)`);
    let hits = teasers.filter(t => tag.test(title(t)));
    if (!hits.length) {
        // ponytail: episodes without (Sxx/Eyy) tags are matched by TMDB episode name only; no positional fallback since the Mediathek rarely has a complete season
        const name = norm(await tmdbEpisodeName(meta.tmdbId, season, episode));
        if (name.length > 3) hits = teasers.filter(t => norm(title(t)).split(norm(show.title))[0].trim() === name || norm(title(t)) === name);
    }
    return hits;
}

async function findMovie(meta) {
    const show = await findShow(meta, /^SINGLE$/);
    if (show) return groupingTeasers(show.id, 'itemsOfShow');
    for (const q of meta.titles) {
        // films of editorial collections ("Filme in der ARD") only show up as vods; the duration check drops clips/reports
        const vods = (await search('vods', q)).filter(t => t.coreAssetType === 'EPISODE' && t.duration >= 3000 && !ACCESSIBLE.test(title(t)));
        const hit = pickBest(vods.map(t => ({ title: title(t), t })), meta);
        if (hit) return [hit.t];
    }
    return [];
}

async function streamsOf(teaser) {
    const item = await getJson(`${API}/page-gateway/pages/ard/item/${targetId(teaser)}?embedded=true&mcV6=true`);
    const player = (item.widgets || []).find(w => /^player/.test(w.type || ''));
    const embedded = player && player.mediaCollection && player.mediaCollection.embedded;
    if (!embedded) return [];
    let media = [].concat.apply([], (embedded.streams || []).map(s => s.media || []));
    const standard = media.filter(m => !(m.audios || []).some(a => a.kind === 'audio-description' || a.kind === 'speech-optimized'));
    if (standard.length) media = standard;
    return media.filter(m => m.url).map(m => {
        const hls = /mpegurl/i.test(m.mimeType || '') || /\.m3u8/.test(m.url);
        const lang = ((m.audios || [])[0] || {}).languageCode;
        return {
            name: 'ARD',
            title: [title(teaser), LANGS[lang] || lang, m.forcedLabel, hls ? 'HLS' : 'MP4'].filter(Boolean).join(' · '),
            url: m.url.startsWith('//') ? 'https:' + m.url : m.url,
            quality: hls ? 'auto' : m.maxVResolutionPx ? `${m.maxVResolutionPx}p` : 'auto',
            res: hls ? 1e5 : m.maxVResolutionPx || 0,
        };
    });
}

async function getStreams(tmdbId, mediaType, season, episode) {
    try {
        const meta = await getMeta(tmdbId, mediaType);
        const teasers = mediaType === 'tv' ? await findEpisodes(meta, season || 1, episode || 1) : await findMovie(meta);
        const seen = {};
        const streams = [].concat.apply([], await Promise.all(teasers.map(streamsOf)))
            .filter(s => !seen[s.url] && (seen[s.url] = true))
            .sort((a, b) => b.res - a.res);
        return streams.map(s => ({ name: s.name, title: s.title, url: s.url, quality: s.quality }));
    } catch (e) {
        console.error(`[ARD] ${e.message}`);
    }
    return [];
}

module.exports = provider(getStreams);
