// Ports of the CloudStream core extractors GermanProviders relies on. Each: (url, referer) -> [{url, quality, headers}]
import { getJson, getText, postForm, postJson, UA, send, fileNames } from '../http.js';
import { fetchPage, jwplayer, origin, quality, scripts, unpack } from './util.js';

const CryptoJS = require('crypto-js');

export async function voe(url, referer) {
    let html = await fetchPage(url, { Referer: referer || url });
    const redirect = html.match(/window\.location\.href\s*=\s*'([^']+)';/);
    if (redirect) {
        url = redirect[1];
        html = await fetchPage(url, { Referer: referer || url });
    }
    const json = (html.match(/<script type="application\/json">([\s\S]*?)<\/script>/) || [])[1];
    if (!json) return [];
    let s = json.trim().replace(/^\["/, '').replace(/"\]$/, '');
    s = s.replace(/[a-zA-Z]/g, c => {
        const base = c <= 'Z' ? 65 : 97;
        return String.fromCharCode((c.charCodeAt(0) - base + 13) % 26 + base);
    });
    for (const p of ['@$', '^^', '~@', '%?', '*~', '!!', '#&']) s = s.split(p).join('_');
    s = atob(s.replace(/_/g, ''));
    s = s.split('').map(c => String.fromCharCode(c.charCodeAt(0) - 3)).reverse().join('');
    const data = JSON.parse(atob(s));
    const headers = { Referer: origin(url) + '/', Origin: origin(url) };
    const out = [];
    if (data.source) out.push({ url: data.source, quality: 'auto', headers });
    if (data.direct_access_url) out.push({ url: data.direct_access_url, quality: 'auto', headers: { Referer: url } });
    // "<title>Watch KonoSuba E01 GerSub AC3 720p BluRay x264-Fuuko - VOE | …" is the only place that tells dub from sub
    const file = (html.match(/<title>Watch ([^<|]*)/) || [])[1];
    if (file) for (const s of out) fileNames[s.url] = file;
    return out;
}

export async function dood(url) {
    const embed = url.replace('/d/', '/e/');
    const res = await send(embed, { headers: { 'User-Agent': UA } });
    const html = await res.text();
    const host = origin(res.url || embed);
    const pass = (html.match(/\/pass_md5\/[^']*/) || [])[0];
    if (!pass) return [];
    const prefix = await getText(host + pass, { headers: { Referer: res.url || embed } });
    let token = '';
    const abc = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    for (let i = 0; i < 10; i++) token += abc[Math.floor(Math.random() * abc.length)];
    const title = (html.match(/<title>([\s\S]*?)<\/title>/) || [])[1];
    return [{ url: `${prefix}${token}?token=${pass.split('/').pop()}&expiry=${Date.now()}`, quality: quality(title), headers: { Referer: host + '/' } }];
}

function aesDecrypt(cipherBytes, key, opts) {
    return CryptoJS.AES.decrypt(CryptoJS.lib.CipherParams.create({ ciphertext: cipherBytes }), key, opts);
}

export async function vidstack(url) {
    const hash = url.split('#').pop().split('/').pop();
    const enc = (await getText(`${origin(url)}/api/v1/video?id=${hash}`, {
        headers: { 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:134.0) Gecko/20100101 Firefox/134.0' },
    })).trim();
    const key = CryptoJS.enc.Utf8.parse('kiemtienmua911ca');
    for (const iv of ['1234567890oiuytr', '0123456789abcdef']) {
        try {
            const text = aesDecrypt(CryptoJS.enc.Hex.parse(enc), key, { iv: CryptoJS.enc.Utf8.parse(iv), mode: CryptoJS.mode.CBC, padding: CryptoJS.pad.Pkcs7 })
                .toString(CryptoJS.enc.Latin1);
            const src = (text.match(/"source":"(.*?)"/) || [])[1];
            if (src) return [{ url: src.replace(/\\\//g, '/'), quality: 'auto', headers: { Referer: url, Origin: origin(url) } }];
        } catch (e) { /* wrong IV, try the next */ }
    }
    return [];
}

export async function supervideo(url) {
    const html = await fetchPage(url);
    return jwplayer(unpack(html) || '', url).map(s => Object.assign(s, { headers: { Referer: origin(url) + '/' } }));
}

export async function vidhidepro(url, referer) {
    const embed = url.replace(/\/(d|download|file|f)\//, '/v/');
    const html = await fetchPage(embed, { Referer: referer || embed });
    const script = unpack(html) || scripts(html).find(s => s.includes('sources:')) || '';
    return jwplayer(script, embed).map(s => Object.assign(s, { headers: { Referer: origin(embed) + '/', Origin: origin(embed) } }));
}

export async function streamwish(url, referer) {
    const embed = url.replace(/\/[fe]\//, '/');
    const html = await fetchPage(embed, { Referer: referer || embed });
    const script = unpack(html)
        || scripts(html).find(s => s.includes('jwplayer("vplayer").setup('))
        || scripts(html).find(s => s.includes('sources:')) || '';
    // CloudStream falls back to a WebView here; a JS plugin can't, so such pages yield nothing.
    return jwplayer(script, embed).map(s => Object.assign(s, { headers: { Referer: origin(embed) + '/', Origin: origin(embed) } }));
}

export async function mixdrop(url) {
    const html = await fetchPage(url.replace('/f/', '/e/'));
    const link = ((unpack(html) || html).match(/wurl.*?=.*?"(.*?)";/) || [])[1];
    // the CDN token is bound to the User-Agent that loaded the embed
    return link ? [{ url: link.startsWith('//') ? 'https:' + link : link, quality: 'auto', headers: { Referer: url, 'User-Agent': UA } }] : [];
}

export async function filemoon(url, referer) {
    const headers = { Referer: url, 'Sec-Fetch-Dest': 'iframe', 'Sec-Fetch-Mode': 'navigate', 'Sec-Fetch-Site': 'cross-site' };
    let html = await fetchPage(url, headers);
    const iframe = (html.match(/<iframe[^>]+src="([^"]+)"/) || [])[1];
    if (iframe) html = await fetchPage(iframe, Object.assign({}, headers, { 'Accept-Language': 'en-US,en;q=0.5' }));
    return jwplayer(unpack(html) || '', iframe || url).map(s => Object.assign(s, { headers: { Referer: origin(iframe || url) + '/' } }));
}

export async function vidoza(url) {
    const html = await fetchPage(url);
    const line = (html.match(/sourcesCode:\s*(\[[^\n]*\])/) || [])[1];
    if (!line) return [];
    return JSON.parse(line.replace(/"?(src|type|label|res)"?\s*:/g, '"$1":'))
        .map(s => ({ url: s.src, quality: quality(s.res || s.label), headers: { Referer: url } }));
}

export async function streamtape(url) {
    const html = await fetchPage(url);
    const line = (html.match(/botlink'\)\.innerHTML\s*=\s*([^\n;]+)/) || [])[1];
    if (!line) return [];
    // expression like: '//streamtape.com/get_video?id=' + ('xcdtoken=abc').substring(1).substring(2)
    const value = line.split('+').map(part => {
        let s = (part.match(/'([^']*)'/) || [])[1] || '';
        const re = /\.substring\((\d+)\)/g;
        let m;
        while ((m = re.exec(part))) s = s.substring(Number(m[1]));
        return s;
    }).join('');
    return value ? [{ url: `https:${value}&stream=1`, quality: 'auto', headers: { Referer: url } }] : [];
}

export async function lulustream(url, referer) {
    const html = await postForm(`${origin(url)}/dl`, { op: 'embed', file_code: url.split('/').pop(), auto: '1', referer: referer || '' });
    const script = scripts(html).find(s => s.includes('vplayer')) || '';
    return jwplayer(script, url).map(s => Object.assign(s, { headers: { Referer: origin(url) + '/' } }));
}

// Unknown host (these services rotate mirror domains constantly): recognise the player by its page
export async function sniff(url, referer) {
    const html = await fetchPage(url, { Referer: referer || url });
    if (html.includes('/pass_md5/')) return dood(url);
    // Vidara (vidara.so, odysseusa.cc...): stream URL comes from POST /api/stream
    if (html.includes('"/api/stream"') && html.includes('filecode')) {
        const d = await getJson(origin(url) + '/api/stream', { method: 'POST', body: JSON.stringify({ filecode: url.split('/').filter(Boolean).pop(), device: 'web' }), headers: { 'Content-Type': 'application/json', Referer: url } });
        return d.streaming_url ? [{ url: d.streaming_url, quality: 'auto', headers: { Referer: origin(url) + '/' } }] : [];
    }
    // FireStream: signed URL from POST /api/videos/<slug>/resolve with the page's token blob
    const blob = (html.match(/id="token-blob"[^>]*>([^<]*)</) || [])[1];
    if (blob) {
        const d = await postJson(`${origin(url)}/api/videos/${url.split('?')[0].split('/').pop()}/resolve`, { blob: blob.trim() }, { headers: { Referer: url } });
        return d.signedVideoUrl ? [{ url: d.signedVideoUrl, quality: 'auto', headers: { Referer: origin(url) + '/', Origin: origin(url) } }] : [];
    }
    // Vidsonic: URL is hex-encoded (|-separated) and reversed
    const hex = (html.match(/_0x1 = '([0-9a-f|]+)'/) || [])[1];
    if (hex) {
        const s = hex.split('|').join('').replace(/../g, b => String.fromCharCode(parseInt(b, 16)));
        return [{ url: s.split('').reverse().join(''), quality: 'auto', headers: { Referer: origin(url) + '/' } }];
    }
    if (html.includes('application/json') && /window\.location\.href|voe/i.test(html)) return voe(url, referer);
    // VOE mirror stub that only redirects to the current VOE domain
    if (/window\.location\.href = 'https?:\/\/[^']+\/e\/\w+'/.test(html)) return voe(url, referer);
    if (html.includes('sourcesCode:')) return vidoza(url);
    if (html.includes("botlink').innerHTML")) return streamtape(url);
    // vidmoly ships an unrelated packed script next to its plain sources: setup
    let found = jwplayer(unpack(html) || '', url);
    if (!found.length) found = jwplayer(scripts(html).find(s => s.includes('sources:')) || '', url);
    // some CDNs (lulust.com's cdn-tnmr.org) bind the token to the page's User-Agent
    return found.map(s => Object.assign(s, { headers: { Referer: origin(url) + '/', Origin: origin(url), 'User-Agent': UA } }));
}
