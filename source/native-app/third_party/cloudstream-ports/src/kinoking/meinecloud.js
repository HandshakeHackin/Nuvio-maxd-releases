import { getText, postJson } from '../../shared/http.js';
import { resolveAll } from '../../shared/extractors/index.js';

// meinecloud.click mirror list -> resolved streams. The mirrors used to be <li data-link>, now the page POSTs a signed token to /api/embed-links.
export async function meineCloud(url, referer) {
    const html = await getText(url, { headers: { Referer: referer } });
    let links = (html.match(/data-link="[^"]+"/g) || []).map(m => m.slice(11, -1));
    const m = html.match(/type:\s*['"](\w+)['"],\s*id:\s*"([^"]+)",\s*token:\s*"([^"]+)"/);
    if (!links.length && m) {
        const res = await postJson('https://meinecloud.click/api/embed-links', { type: m[1], id: m[2], token: m[3] }, { headers: { Referer: url, Origin: 'https://meinecloud.click' } });
        links = (res.sources || []).map(s => s.url);
    }
    return resolveAll(links.map(l => l.startsWith('//') ? 'https:' + l : l), url);
}
