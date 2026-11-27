import { getJson } from './http.js';

// Nuvio injects TMDB_API_KEY as a global; test.js sets it from the environment.
// Every plugin asks TMDB for the same title, so plugins that run in one JS runtime share the answer for a minute (each
// bundle has its own module state, but the global is common). Failed requests are not kept.
const CACHE_MS = 60000;
export function getMeta(tmdbId, mediaType) {
    const cache = globalThis.__germanProvidersMeta || (globalThis.__germanProvidersMeta = {});
    const key = `${mediaType === 'tv' ? 'tv' : 'movie'}:${tmdbId}`;
    const hit = cache[key];
    if (hit && Date.now() - hit.at < CACHE_MS) return hit.promise;
    const promise = fetchMeta(tmdbId, mediaType);
    cache[key] = { at: Date.now(), promise };
    promise.catch(() => { if (cache[key] && cache[key].promise === promise) delete cache[key]; });
    return promise;
}

async function fetchMeta(tmdbId, mediaType) {
    const type = mediaType === 'tv' ? 'tv' : 'movie';
    const d = await getJson(`https://api.themoviedb.org/3/${type}/${tmdbId}?api_key=${globalThis.TMDB_API_KEY}&language=de-DE&append_to_response=external_ids,translations,alternative_titles`);
    const tr = ((d.translations || {}).translations || []);
    const en = tr.find(t => t.iso_639_1 === 'en' && t.iso_3166_1 === 'US') || tr.find(t => t.iso_639_1 === 'en');
    const alts = ((d.alternative_titles || {}).titles || (d.alternative_titles || {}).results || [])
        .filter(a => a.iso_3166_1 === 'DE' || a.iso_3166_1 === 'AT').map(a => a.title);
    const title = d.title || d.name;
    const originalTitle = d.original_title || d.original_name;
    const englishTitle = en && (en.data.title || en.data.name);
    const date = d.release_date || d.first_air_date || '';
    return {
        tmdbId: String(tmdbId),
        type,
        title,
        originalTitle,
        englishTitle,
        year: date ? Number(date.slice(0, 4)) : null,
        imdbId: (d.external_ids || {}).imdb_id || d.imdb_id || null,
        titles: [title, originalTitle, englishTitle].concat(alts).filter((t, i, a) => t && a.indexOf(t) === i),
    };
}
