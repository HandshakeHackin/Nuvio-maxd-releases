import { getText, UA } from '../http.js';

export const origin = url => url.match(/^https?:\/\/[^/]+/)[0];

export const scripts = html => (html.match(/<script[^>]*>[\s\S]*?<\/script>/gi) || []).map(s => s.replace(/^<script[^>]*>|<\/script>$/gi, ''));

export const fetchPage = (url, headers = {}) => getText(url, { headers: Object.assign({ 'User-Agent': UA }, headers) });

export function quality(label) {
    const m = String(label || '').match(/(\d{3,4})p/i) || String(label || '').match(/^(\d{3,4})$/);
    return m ? `${m[1]}p` : 'auto';
}

// P.A.C.K.E.R. (eval(function(p,a,c,k,e,d)...)) unpacker, port of CloudStream's JsUnpacker
export function unpack(text) {
    const packed = (text.match(/eval\(function\(p,a,c,k,e,[\s\S]*?\.split\('\|'\)[^)]*\)\)/) || [])[0];
    const m = packed && packed.match(/\}\s*\('([\s\S]*)',\s*(.*?),\s*(\d+),\s*'([\s\S]*?)'\.split\('\|'\)/);
    if (!m) return null;
    const payload = m[1].replace(/\\'/g, "'");
    const radix = parseInt(m[2], 10) || 36;
    const symtab = m[4].split('|');
    if (symtab.length !== Number(m[3])) return null;
    const alphabet = radix > 36
        ? (radix <= 62 ? '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
            : ' !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~').slice(0, radix)
        : null;
    const unbase = w => alphabet
        ? w.split('').reverse().reduce((n, ch, i) => n + Math.pow(radix, i) * alphabet.indexOf(ch), 0)
        : parseInt(w, radix);
    return payload.replace(/\b\w+\b/g, w => symtab[unbase(w)] || w);
}

// JWPlayer setup script -> [{url, quality}], port of CloudStream's JwPlayerHelper (without HLS variant expansion)
export function jwplayer(script, base) {
    const fix = u => {
        u = u.replace(/\\\//g, '/');
        return /^https?:/.test(u) ? u : u.startsWith('//') ? 'https:' + u : u.startsWith('/') ? origin(base) + u : `${origin(base)}/${u}`;
    };
    const out = [];
    const re = /"?sources"?:\s*(\[.*?\])/g;
    let m;
    while ((m = re.exec(script))) {
        try {
            const json = m[1].replace(/"?(file|label|type)"?\s*:/g, '"$1":').replace(/'/g, '"');
            for (const s of JSON.parse(json)) if (s.file) out.push({ url: fix(s.file), quality: quality(s.label) });
        } catch (e) { /* not JSON, fall through to the URL regex */ }
    }
    if (!out.length) {
        const urlRe = /[:=]\s*"([^"\s]+(\.m3u8|master\.txt)[^"\s]*)/g;
        while ((m = urlRe.exec(script))) out.push({ url: fix(m[1]), quality: 'auto' });
    }
    return out;
}
