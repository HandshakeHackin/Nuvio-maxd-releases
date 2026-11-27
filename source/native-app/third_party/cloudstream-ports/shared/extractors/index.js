import * as h from './hosters.js';
import { gather } from '../http.js';

// host (without www.) -> decoder; mirrors from GermanProviders' extractor registrations + CloudStream core
const HOSTS = [
    [h.voe, ['voe.sx', 'goofy-banana.com', 'urochsunloath.com', 'donaldlineelse.com', 'charlestoughrace.com', 'tubelessceliolymph.com', 'simpulumlamerop.com', 'nathanfromsubject.com', 'yip.su', 'metagnathtuggers.com']],
    [h.vidstack, ['moflix.upns.xyz', 'moflix.rpmplay.xyz']],
    [h.supervideo, ['supervideo', 'dropload', 'abstream.to', 'dr0pstream.com']],
    [h.vidhidepro, ['vidhide', 'filelions', 'ryderjet.com', 'moflix-stream.click', 'smoothpre.com', 'dhtpre.com', 'peytonepre.com']],
    [h.streamwish, ['streamwish', 'luluvdo.com', 'streamruby.com', 'savefiles.com', 'wishembed', 'swdyu.com', 'strwish']],
    [h.lulustream, ['lulustream.com', 'luluvdoo.com']],
    [h.mixdrop, ['mixdrop', 'mixdrp', 'mxdrop', 'mdy48tn97.com']],
    [h.filemoon, ['filemoon']],
    [h.vidoza, ['vidoza.net', 'videzz.net']],
    [h.streamtape, ['streamtape', 'watchadsontape.com', 'shavetape.cash']],
];

// Dood answers every non-browser client with a Cloudflare challenge on all of these domains, so asking only costs a
// request, and Nuvio TV runs a plugin's requests one after the other. Unknown Dood mirrors still go through sniff().
const BLOCKED = ['dood', 'd000d.com', 'vide0.net', 'dsvplay.com', 'dooodster.com', 'doods.pro', 'playmogo.com', 'd0000d.com', 'ds2play.com', 'doodstream.com', 'do7go.com'];

const hostOf = url => (url.match(/^https?:\/\/(?:www\.)?([^/:?#]+)/i) || [])[1] || '';
const listed = (host, names) => names.some(n => host === n || (!n.includes('.') && host.includes(n)));

export function decoderFor(url) {
    const hit = HOSTS.find(([, names]) => listed(hostOf(url), names));
    return hit ? hit[0] : h.sniff;
}

// embed URL -> [{url, quality, headers, host}], never throws
export async function resolveEmbed(url, referer) {
    if (!url) return [];
    if (url.startsWith('//')) url = 'https:' + url;
    if (listed(hostOf(url), BLOCKED)) return [];
    try {
        const host = url.split('/')[2].replace(/^www\./, '');
        return (await decoderFor(url)(url, referer)).filter(s => s.url).map(s => Object.assign(s, { host }));
    } catch (e) {
        console.error(`[extractor] ${url}: ${e.message}`);
        return [];
    }
}

// Which hosters to try first. Together with the early stop in gather() this decides which streams a list is built from.
// An educated guess, not measured: VOE and Vidsonic/Vidara are the ones that work for the most people, Dood never does.
const PREFERRED = [/voe|goofy-banana|nathanfromsubject|metagnathtuggers/, /vidsonic|vidara|odysseusa/, /vidhide|filelions|ryderjet|smoothpre|dhtpre|peytonepre/,
    /streamwish|wishembed|luluvdo|lulustream|swdyu|strwish|streamruby|savefiles/, /supervideo|dropload|abstream|dr0pstream/, /filemoon/, /vidoza|videzz/,
    /streamtape|shavetape|watchadsontape/, /mixdrop|mixdrp|mxdrop/];
const hosterRank = text => {
    const t = String(text || '').toLowerCase();
    if (/dood|d000d|vide0\.net|playmogo|dsvplay|ds2play/.test(t)) return PREFERRED.length + 1;
    const i = PREFERRED.findIndex(re => re.test(t));
    return i < 0 ? 3 : i; // unknown hosters rank in the middle
};
// stable: equal ranks keep the site's order. text(item) is the host's URL or name
export const byHoster = (items, text = x => x) => items.map((x, i) => [x, i]).sort(([a, i], [b, j]) => hosterRank(text(a)) - hosterRank(text(b)) || i - j).map(([x]) => x);

export const resolveAll = (urls, referer) => gather(byHoster(urls), u => resolveEmbed(u, referer));
