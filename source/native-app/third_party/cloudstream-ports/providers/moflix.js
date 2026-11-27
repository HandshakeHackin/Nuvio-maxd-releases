var __async = (__this, __arguments, generator) => {
  return new Promise((resolve, reject) => {
    var fulfilled = (value) => {
      try {
        step(generator.next(value));
      } catch (e) {
        reject(e);
      }
    };
    var rejected = (value) => {
      try {
        step(generator.throw(value));
      } catch (e) {
        reject(e);
      }
    };
    var step = (x) => x.done ? resolve(x.value) : Promise.resolve(x.value).then(fulfilled, rejected);
    step((generator = generator.apply(__this, __arguments)).next());
  });
};

// shared/http.js
var UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";
var DEADLINE_MS = 25e3;
var running = 0;
var idle = [];
var deadline = Infinity;
function send(url, opts) {
  if (Date.now() > deadline) return Promise.reject(new Error(`deadline reached, skipped ${url}`));
  running++;
  const done = () => {
    if (--running === 0) idle.splice(0).forEach((resolve) => resolve());
  };
  return fetch(url, opts).then((res) => {
    done();
    return res;
  }, (err) => {
    done();
    throw err;
  });
}
var SUBBED = [[/ger(man)?[\s._-]*sub|untertitel deutsch/i, "Original, dt. UT"], [/eng(lish)?[\s._-]*sub|untertitel englisch/i, "Original, engl. UT"]];
var LANGS = [[/japanisch, dt\. ut/i, "Japanisch, dt. UT"], [/japanisch, engl\. ut/i, "Japanisch, engl. UT"]].concat(SUBBED, [
  [/\bomu\b/i, "OmU"],
  [/\bov\b/i, "OV"],
  [/englisch|\ben\b/i, "Englisch"],
  [/franz|\bfr\b/i, "Franz\xF6sisch"],
  [/spanisch/i, "Spanisch"],
  [/japanisch/i, "Japanisch"],
  [/deutsch|\bde\b/i, "Deutsch"]
]);
var FLAGS = { Deutsch: "\u{1F1E9}\u{1F1EA}", Englisch: "\u{1F1EC}\u{1F1E7}", Franz\u00F6sisch: "\u{1F1EB}\u{1F1F7}", Spanisch: "\u{1F1EA}\u{1F1F8}", Japanisch: "\u{1F1EF}\u{1F1F5}", OV: "\u{1F310}", OmU: "\u{1F310} UT" };
var SUB_FLAGS = { "dt. UT": "\u{1F1E9}\u{1F1EA}", "engl. UT": "\u{1F1EC}\u{1F1E7}" };
function flagLabel(lang) {
  const sub = lang.match(/^(.*), (dt\. UT|engl\. UT)$/);
  if (sub) return `${sub[1] === "Original" ? "\u{1F310}" : FLAGS[sub[1]] || sub[1]} UT ${SUB_FLAGS[sub[2]]}`;
  return FLAGS[lang] || lang;
}
var fileNames = {};
var tierOf = (lang) => lang === "Deutsch" ? 0 : /dt\. UT|OmU/.test(lang) ? 1 : 2;
function decorate(s) {
  const hit = LANGS.find(([re]) => re.test(s.title || ""));
  let lang = hit ? hit[1] : "Deutsch", title = s.title;
  const sub = SUBBED.find(([re]) => re.test(fileNames[s.url] || ""));
  if (sub && !/UT|OmU/.test(lang)) {
    lang = sub[1];
    title = hit ? title.replace(hit[0], lang) : [title, lang].filter(Boolean).join(" \xB7 ");
  }
  const quality2 = s.quality && s.quality !== "auto" ? s.quality : /\.m3u8|\/hls|master/i.test(s.url) ? "HLS" : "MP4";
  const host = ((s.title || "").split(" \xB7 ")[0].match(/^[\w-]+\.[a-z]{2,}$/) || [])[0];
  const tags = releaseTags(fileNames[s.url]);
  if (tags.length) title = [title].concat(tags).filter(Boolean).join(" \xB7 ");
  const stream = Object.assign({}, s, { name: [s.name, flagLabel(lang), host].filter(Boolean).join(" \xB7 "), title, quality: quality2 });
  return { lang, tier: tierOf(lang), host: host || (String(s.url).match(/^https?:\/\/([^/]+)/) || [])[1] || "", stream, bandwidth: 0, dead: false };
}
var CODECS = [[/hvc1|hev1|hevc|x265|h\.?265/i, "H.265"], [/avc1|x264|h\.?264/i, "H.264"], [/av01|\bav1\b/i, "AV1"], [/vp0?9/i, "VP9"]];
var codecName = (text) => (CODECS.find(([re]) => re.test(text)) || [])[1];
var mbit = (bps) => `${(bps / 1e6).toFixed(1).replace(".", ",")} Mbit/s`;
var gigabytes = (bytes) => bytes >= 1e9 ? `${(bytes / 1e9).toFixed(1).replace(".", ",")} GB` : `${Math.round(bytes / 1e6)} MB`;
function releaseTags(name) {
  return [/blu-?ray|remux|web-?dl|web-?rip|hdtv|dvd-?rip/i, /atmos|truehd|dts-?hd|dts|e-?ac-?3|ac-?3|aac/i, /x26[45]|h\.?26[45]|hevc|av1/i].map((re) => (String(name || "").match(re) || [])[0]).filter(Boolean).map((t) => /^(x|h\.?)26[45]$/i.test(t) || /hevc/i.test(t) ? codecName(t) : t);
}
function bestVariant(text) {
  let best = null, m;
  const re = /#EXT-X-STREAM-INF:([^\n]*)/g;
  while (m = re.exec(text)) {
    const attrs = m[1];
    const v = {
      height: Number((attrs.match(/RESOLUTION=\d+x(\d+)/) || [])[1]) || 0,
      bandwidth: Number((attrs.match(/(?:^|,)BANDWIDTH=(\d+)/) || [])[1]) || 0,
      codec: codecName((attrs.match(/CODECS="([^"]*)"/) || [])[1] || ""),
      hdr: /dvh[1e]/.test(attrs) ? "Dolby Vision" : { PQ: "HDR10", HLG: "HLG" }[(attrs.match(/VIDEO-RANGE=(\w+)/) || [])[1]]
    };
    if (!best || v.height > best.height || v.height === best.height && v.bandwidth > best.bandwidth) best = v;
  }
  return best;
}
var DETAIL_TIMEOUT_MS = 3e3;
function timed(url, opts, readBody) {
  return __async(this, null, function* () {
    if (typeof AbortController === "undefined" || typeof setTimeout !== "function") {
      const res = yield send(url, opts);
      return { res, text: readBody && res.ok ? yield res.text() : "" };
    }
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), DETAIL_TIMEOUT_MS);
    try {
      const res = yield send(url, Object.assign({}, opts, { signal: controller.signal }));
      return { res, text: readBody && res.ok ? yield res.text() : "" };
    } finally {
      clearTimeout(timer);
    }
  });
}
function playlistInfo(stream) {
  return __async(this, null, function* () {
    try {
      const { res, text } = yield timed(stream.url, { headers: Object.assign({ "User-Agent": UA }, stream.headers) }, true);
      if (!res.ok) {
        console.error(`[quality] playlist answered HTTP ${res.status}`);
        return res.status === 404 || res.status === 410 ? { dead: true } : null;
      }
      const best = bestVariant(String(text));
      if (!best) console.error("[quality] playlist lists no variants");
      return best;
    } catch (e) {
      console.error(`[quality] ${e.message}`);
      return null;
    }
  });
}
function fileSize(stream) {
  return __async(this, null, function* () {
    try {
      const { res } = yield timed(stream.url, { method: "HEAD", headers: Object.assign({ "User-Agent": UA }, stream.headers) }, false);
      if (res.status === 404 || res.status === 410) return { dead: true };
      const bytes = res.ok && Number(res.headers.get("content-length"));
      return bytes > 1e6 ? { bytes } : null;
    } catch (e) {
      return null;
    }
  });
}
function addDetails(list) {
  return __async(this, null, function* () {
    const pending = list.slice(0, 4).filter((d) => d.stream.quality === "HLS" || d.stream.quality === "MP4");
    if (!pending.length) return;
    if (deadline - Date.now() < 8e3) {
      console.error(`[quality] skipped, only ${Math.max(0, Math.round((deadline - Date.now()) / 1e3))}s of the time budget left`);
      return;
    }
    yield Promise.all(pending.map((d) => __async(null, null, function* () {
      const s = d.stream, add = [];
      if (s.quality === "MP4" || /\.mp4(\?|$)/i.test(s.url) && !/\.m3u8/i.test(s.url)) {
        const info = yield fileSize(s);
        if (info && info.dead) d.dead = true;
        if (info && info.bytes) add.push(gigabytes(info.bytes));
      } else {
        const v = yield playlistInfo(s);
        if (!v) return;
        if (v.dead) {
          d.dead = true;
          return;
        }
        if (v.height && s.quality === "HLS") s.quality = `${v.height}p`;
        d.bandwidth = v.bandwidth || 0;
        add.push(v.codec, v.bandwidth && mbit(v.bandwidth), v.hdr);
      }
      const extra = add.filter((t) => t && !String(s.title || "").includes(t));
      if (extra.length) s.title = [s.title].concat(extra).filter(Boolean).join(" \xB7 ");
    })));
  });
}
var MAX_STREAMS = 4;
var MIN_HEIGHT = 360;
var CAM = /\b(hd-?cam|cam-?rip|cam|hd-?ts|tele-?sync|hd-?tc|tele-?cine)\b/i;
var heightOf = (d) => Number((String(d.stream.quality).match(/(\d{3,4})p/) || [])[1]) || 0;
var keepIfAny = (list, keep) => {
  const kept = list.filter(keep);
  return kept.length ? kept : list;
};
function shape(list) {
  list = keepIfAny(list, (d) => !CAM.test([d.stream.quality, fileNames[d.stream.url], String(d.stream.title || "").split(" \xB7 ").slice(1).join(" ")].join(" ")));
  const best = Math.min(...list.map((d) => d.tier));
  if (best > 1) return [];
  list = list.filter((d) => d.tier === best);
  list = keepIfAny(list, (d) => !d.dead);
  const heavy = (d) => heightOf(d) > 720 ? "big" : "light";
  const better = (a, b) => heightOf(a) - heightOf(b) || a.bandwidth - b.bandwidth;
  const first = {};
  list.forEach((d) => {
    const key = `${d.lang}|${d.host}|${heavy(d)}`;
    if (!first[key] || better(d, first[key]) > 0) first[key] = d;
  });
  list = list.filter((d) => first[`${d.lang}|${d.host}|${heavy(d)}`] === d);
  list = keepIfAny(list, (d) => !(heightOf(d) > 0 && heightOf(d) <= MIN_HEIGHT));
  list = list.map((d, i) => [d, i]).sort(([a, i], [b, j]) => heightOf(b) - heightOf(a) || b.bandwidth - a.bandwidth || i - j).map(([d]) => d);
  const top = list.slice(0, MAX_STREAMS);
  const light = list.find((d) => heavy(d) === "light" && heightOf(d) > 0);
  if (light && !top.includes(light) && top.every((d) => heavy(d) === "big")) top[top.length - 1] = light;
  return top;
}
var BATCH = 5;
var ENOUGH = 6;
var isGerman = (text) => /deutsch|german|\bde\b|\bger\b/i.test(text || "") && !/sub|untertitel|\but\b/i.test(text || "");
var germanFirst = (items, text) => items.map((x, i) => [x, i]).sort(([a, i], [b, j]) => isGerman(text(a)) ? isGerman(text(b)) ? i - j : -1 : isGerman(text(b)) ? 1 : i - j).map(([x]) => x);
function gather(_0, _1) {
  return __async(this, arguments, function* (items, worker, enough = ENOUGH) {
    const out = [];
    for (let i = 0; i < items.length && out.length < enough; i += BATCH) {
      const parts = yield Promise.all(items.slice(i, i + BATCH).map((item) => Promise.resolve().then(() => worker(item)).catch(() => [])));
      parts.forEach((p) => out.push(...p));
    }
    return out;
  });
}
function provider(getStreams2) {
  return {
    getStreams(tmdbId, mediaType, season, episode) {
      return __async(this, null, function* () {
        deadline = Date.now() + DEADLINE_MS;
        if (mediaType !== "movie") mediaType = "tv";
        let streams = [];
        try {
          streams = (yield getStreams2(tmdbId, mediaType, season, episode)) || [];
        } catch (e) {
          console.error(e.message);
        }
        if (running) yield new Promise((resolve) => idle.push(resolve));
        const list = streams.map(decorate);
        if (!list.length) return [];
        yield addDetails(list);
        if (running) yield new Promise((resolve) => idle.push(resolve));
        return shape(list).map((d) => d.stream);
      });
    }
  };
}
function request(_0) {
  return __async(this, arguments, function* (url, opts = {}) {
    const res = yield send(url, Object.assign({}, opts, { headers: Object.assign({ "User-Agent": UA }, opts.headers) }));
    if (!res.ok) throw new Error(`HTTP ${res.status} ${url}`);
    return res;
  });
}
var getText = (url, opts) => __async(null, null, function* () {
  return (yield request(url, opts)).text();
});
var getJson = (url, opts) => __async(null, null, function* () {
  return JSON.parse(yield getText(url, opts));
});
var postJson = (url, body, opts = {}) => getJson(url, Object.assign({}, opts, {
  method: "POST",
  body: JSON.stringify(body),
  headers: Object.assign({ "Content-Type": "application/json" }, opts.headers)
}));
var postForm = (url, form, opts = {}) => getText(url, Object.assign({}, opts, {
  method: "POST",
  body: Object.keys(form).map((k) => encodeURIComponent(k) + "=" + encodeURIComponent(form[k])).join("&"),
  headers: Object.assign({ "Content-Type": "application/x-www-form-urlencoded" }, opts.headers)
}));

// shared/tmdb.js
var CACHE_MS = 6e4;
function getMeta(tmdbId, mediaType) {
  const cache = globalThis.__germanProvidersMeta || (globalThis.__germanProvidersMeta = {});
  const key = `${mediaType === "tv" ? "tv" : "movie"}:${tmdbId}`;
  const hit = cache[key];
  if (hit && Date.now() - hit.at < CACHE_MS) return hit.promise;
  const promise = fetchMeta(tmdbId, mediaType);
  cache[key] = { at: Date.now(), promise };
  promise.catch(() => {
    if (cache[key] && cache[key].promise === promise) delete cache[key];
  });
  return promise;
}
function fetchMeta(tmdbId, mediaType) {
  return __async(this, null, function* () {
    const type = mediaType === "tv" ? "tv" : "movie";
    const d = yield getJson(`https://api.themoviedb.org/3/${type}/${tmdbId}?api_key=${globalThis.TMDB_API_KEY}&language=de-DE&append_to_response=external_ids,translations,alternative_titles`);
    const tr = (d.translations || {}).translations || [];
    const en = tr.find((t) => t.iso_639_1 === "en" && t.iso_3166_1 === "US") || tr.find((t) => t.iso_639_1 === "en");
    const alts = ((d.alternative_titles || {}).titles || (d.alternative_titles || {}).results || []).filter((a) => a.iso_3166_1 === "DE" || a.iso_3166_1 === "AT").map((a) => a.title);
    const title = d.title || d.name;
    const originalTitle = d.original_title || d.original_name;
    const englishTitle = en && (en.data.title || en.data.name);
    const date = d.release_date || d.first_air_date || "";
    return {
      tmdbId: String(tmdbId),
      type,
      title,
      originalTitle,
      englishTitle,
      year: date ? Number(date.slice(0, 4)) : null,
      imdbId: (d.external_ids || {}).imdb_id || d.imdb_id || null,
      titles: [title, originalTitle, englishTitle].concat(alts).filter((t, i, a) => t && a.indexOf(t) === i)
    };
  });
}

// shared/match.js
function norm(s) {
  s = String(s || "").toLowerCase();
  if (s.normalize) s = s.normalize("NFD").replace(/[̀-ͯ]/g, "");
  return s.replace(/ß/g, "ss").replace(/&/g, " and ").replace(/[^a-z0-9]+/g, " ").trim();
}
function score(title, year, meta) {
  const t = norm(title);
  if (!t) return 0;
  let s = 0;
  for (const m of meta.titles.map(norm)) {
    if (t === m) s = Math.max(s, 3);
    else if (m.length > 3 && (t.includes(m) || m.includes(t))) s = Math.max(s, 1);
  }
  if (year && meta.year) {
    const d = Math.abs(Number(year) - meta.year);
    s += d === 0 ? 2 : d === 1 ? 1 : -2;
  }
  return s;
}
function pickBest(items, meta) {
  let best = null, bestScore = 2;
  for (const it of items) {
    const s = score(it.title, it.year, meta);
    if (s > bestScore) {
      best = it;
      bestScore = s;
    }
  }
  return best;
}

// shared/extractors/util.js
var origin = (url) => url.match(/^https?:\/\/[^/]+/)[0];
var scripts = (html) => (html.match(/<script[^>]*>[\s\S]*?<\/script>/gi) || []).map((s) => s.replace(/^<script[^>]*>|<\/script>$/gi, ""));
var fetchPage = (url, headers = {}) => getText(url, { headers: Object.assign({ "User-Agent": UA }, headers) });
function quality(label) {
  const m = String(label || "").match(/(\d{3,4})p/i) || String(label || "").match(/^(\d{3,4})$/);
  return m ? `${m[1]}p` : "auto";
}
function unpack(text) {
  const packed = (text.match(/eval\(function\(p,a,c,k,e,[\s\S]*?\.split\('\|'\)[^)]*\)\)/) || [])[0];
  const m = packed && packed.match(/\}\s*\('([\s\S]*)',\s*(.*?),\s*(\d+),\s*'([\s\S]*?)'\.split\('\|'\)/);
  if (!m) return null;
  const payload = m[1].replace(/\\'/g, "'");
  const radix = parseInt(m[2], 10) || 36;
  const symtab = m[4].split("|");
  if (symtab.length !== Number(m[3])) return null;
  const alphabet = radix > 36 ? (radix <= 62 ? "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" : " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~").slice(0, radix) : null;
  const unbase = (w) => alphabet ? w.split("").reverse().reduce((n, ch, i) => n + Math.pow(radix, i) * alphabet.indexOf(ch), 0) : parseInt(w, radix);
  return payload.replace(/\b\w+\b/g, (w) => symtab[unbase(w)] || w);
}
function jwplayer(script, base) {
  const fix = (u) => {
    u = u.replace(/\\\//g, "/");
    return /^https?:/.test(u) ? u : u.startsWith("//") ? "https:" + u : u.startsWith("/") ? origin(base) + u : `${origin(base)}/${u}`;
  };
  const out = [];
  const re = /"?sources"?:\s*(\[.*?\])/g;
  let m;
  while (m = re.exec(script)) {
    try {
      const json = m[1].replace(/"?(file|label|type)"?\s*:/g, '"$1":').replace(/'/g, '"');
      for (const s of JSON.parse(json)) if (s.file) out.push({ url: fix(s.file), quality: quality(s.label) });
    } catch (e) {
    }
  }
  if (!out.length) {
    const urlRe = /[:=]\s*"([^"\s]+(\.m3u8|master\.txt)[^"\s]*)/g;
    while (m = urlRe.exec(script)) out.push({ url: fix(m[1]), quality: "auto" });
  }
  return out;
}

// shared/extractors/hosters.js
var CryptoJS = require("crypto-js");
function voe(url, referer) {
  return __async(this, null, function* () {
    let html = yield fetchPage(url, { Referer: referer || url });
    const redirect = html.match(/window\.location\.href\s*=\s*'([^']+)';/);
    if (redirect) {
      url = redirect[1];
      html = yield fetchPage(url, { Referer: referer || url });
    }
    const json = (html.match(/<script type="application\/json">([\s\S]*?)<\/script>/) || [])[1];
    if (!json) return [];
    let s = json.trim().replace(/^\["/, "").replace(/"\]$/, "");
    s = s.replace(/[a-zA-Z]/g, (c) => {
      const base = c <= "Z" ? 65 : 97;
      return String.fromCharCode((c.charCodeAt(0) - base + 13) % 26 + base);
    });
    for (const p of ["@$", "^^", "~@", "%?", "*~", "!!", "#&"]) s = s.split(p).join("_");
    s = atob(s.replace(/_/g, ""));
    s = s.split("").map((c) => String.fromCharCode(c.charCodeAt(0) - 3)).reverse().join("");
    const data = JSON.parse(atob(s));
    const headers = { Referer: origin(url) + "/", Origin: origin(url) };
    const out = [];
    if (data.source) out.push({ url: data.source, quality: "auto", headers });
    if (data.direct_access_url) out.push({ url: data.direct_access_url, quality: "auto", headers: { Referer: url } });
    const file = (html.match(/<title>Watch ([^<|]*)/) || [])[1];
    if (file) for (const s2 of out) fileNames[s2.url] = file;
    return out;
  });
}
function dood(url) {
  return __async(this, null, function* () {
    const embed = url.replace("/d/", "/e/");
    const res = yield send(embed, { headers: { "User-Agent": UA } });
    const html = yield res.text();
    const host = origin(res.url || embed);
    const pass = (html.match(/\/pass_md5\/[^']*/) || [])[0];
    if (!pass) return [];
    const prefix = yield getText(host + pass, { headers: { Referer: res.url || embed } });
    let token = "";
    const abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (let i = 0; i < 10; i++) token += abc[Math.floor(Math.random() * abc.length)];
    const title = (html.match(/<title>([\s\S]*?)<\/title>/) || [])[1];
    return [{ url: `${prefix}${token}?token=${pass.split("/").pop()}&expiry=${Date.now()}`, quality: quality(title), headers: { Referer: host + "/" } }];
  });
}
function aesDecrypt(cipherBytes, key, opts) {
  return CryptoJS.AES.decrypt(CryptoJS.lib.CipherParams.create({ ciphertext: cipherBytes }), key, opts);
}
function vidstack(url) {
  return __async(this, null, function* () {
    const hash = url.split("#").pop().split("/").pop();
    const enc = (yield getText(`${origin(url)}/api/v1/video?id=${hash}`, {
      headers: { "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:134.0) Gecko/20100101 Firefox/134.0" }
    })).trim();
    const key = CryptoJS.enc.Utf8.parse("kiemtienmua911ca");
    for (const iv of ["1234567890oiuytr", "0123456789abcdef"]) {
      try {
        const text = aesDecrypt(CryptoJS.enc.Hex.parse(enc), key, { iv: CryptoJS.enc.Utf8.parse(iv), mode: CryptoJS.mode.CBC, padding: CryptoJS.pad.Pkcs7 }).toString(CryptoJS.enc.Latin1);
        const src = (text.match(/"source":"(.*?)"/) || [])[1];
        if (src) return [{ url: src.replace(/\\\//g, "/"), quality: "auto", headers: { Referer: url, Origin: origin(url) } }];
      } catch (e) {
      }
    }
    return [];
  });
}
function supervideo(url) {
  return __async(this, null, function* () {
    const html = yield fetchPage(url);
    return jwplayer(unpack(html) || "", url).map((s) => Object.assign(s, { headers: { Referer: origin(url) + "/" } }));
  });
}
function vidhidepro(url, referer) {
  return __async(this, null, function* () {
    const embed = url.replace(/\/(d|download|file|f)\//, "/v/");
    const html = yield fetchPage(embed, { Referer: referer || embed });
    const script = unpack(html) || scripts(html).find((s) => s.includes("sources:")) || "";
    return jwplayer(script, embed).map((s) => Object.assign(s, { headers: { Referer: origin(embed) + "/", Origin: origin(embed) } }));
  });
}
function streamwish(url, referer) {
  return __async(this, null, function* () {
    const embed = url.replace(/\/[fe]\//, "/");
    const html = yield fetchPage(embed, { Referer: referer || embed });
    const script = unpack(html) || scripts(html).find((s) => s.includes('jwplayer("vplayer").setup(')) || scripts(html).find((s) => s.includes("sources:")) || "";
    return jwplayer(script, embed).map((s) => Object.assign(s, { headers: { Referer: origin(embed) + "/", Origin: origin(embed) } }));
  });
}
function mixdrop(url) {
  return __async(this, null, function* () {
    const html = yield fetchPage(url.replace("/f/", "/e/"));
    const link = ((unpack(html) || html).match(/wurl.*?=.*?"(.*?)";/) || [])[1];
    return link ? [{ url: link.startsWith("//") ? "https:" + link : link, quality: "auto", headers: { Referer: url, "User-Agent": UA } }] : [];
  });
}
function filemoon(url, referer) {
  return __async(this, null, function* () {
    const headers = { Referer: url, "Sec-Fetch-Dest": "iframe", "Sec-Fetch-Mode": "navigate", "Sec-Fetch-Site": "cross-site" };
    let html = yield fetchPage(url, headers);
    const iframe = (html.match(/<iframe[^>]+src="([^"]+)"/) || [])[1];
    if (iframe) html = yield fetchPage(iframe, Object.assign({}, headers, { "Accept-Language": "en-US,en;q=0.5" }));
    return jwplayer(unpack(html) || "", iframe || url).map((s) => Object.assign(s, { headers: { Referer: origin(iframe || url) + "/" } }));
  });
}
function vidoza(url) {
  return __async(this, null, function* () {
    const html = yield fetchPage(url);
    const line = (html.match(/sourcesCode:\s*(\[[^\n]*\])/) || [])[1];
    if (!line) return [];
    return JSON.parse(line.replace(/"?(src|type|label|res)"?\s*:/g, '"$1":')).map((s) => ({ url: s.src, quality: quality(s.res || s.label), headers: { Referer: url } }));
  });
}
function streamtape(url) {
  return __async(this, null, function* () {
    const html = yield fetchPage(url);
    const line = (html.match(/botlink'\)\.innerHTML\s*=\s*([^\n;]+)/) || [])[1];
    if (!line) return [];
    const value = line.split("+").map((part) => {
      let s = (part.match(/'([^']*)'/) || [])[1] || "";
      const re = /\.substring\((\d+)\)/g;
      let m;
      while (m = re.exec(part)) s = s.substring(Number(m[1]));
      return s;
    }).join("");
    return value ? [{ url: `https:${value}&stream=1`, quality: "auto", headers: { Referer: url } }] : [];
  });
}
function lulustream(url, referer) {
  return __async(this, null, function* () {
    const html = yield postForm(`${origin(url)}/dl`, { op: "embed", file_code: url.split("/").pop(), auto: "1", referer: referer || "" });
    const script = scripts(html).find((s) => s.includes("vplayer")) || "";
    return jwplayer(script, url).map((s) => Object.assign(s, { headers: { Referer: origin(url) + "/" } }));
  });
}
function sniff(url, referer) {
  return __async(this, null, function* () {
    const html = yield fetchPage(url, { Referer: referer || url });
    if (html.includes("/pass_md5/")) return dood(url);
    if (html.includes('"/api/stream"') && html.includes("filecode")) {
      const d = yield getJson(origin(url) + "/api/stream", { method: "POST", body: JSON.stringify({ filecode: url.split("/").filter(Boolean).pop(), device: "web" }), headers: { "Content-Type": "application/json", Referer: url } });
      return d.streaming_url ? [{ url: d.streaming_url, quality: "auto", headers: { Referer: origin(url) + "/" } }] : [];
    }
    const blob = (html.match(/id="token-blob"[^>]*>([^<]*)</) || [])[1];
    if (blob) {
      const d = yield postJson(`${origin(url)}/api/videos/${url.split("?")[0].split("/").pop()}/resolve`, { blob: blob.trim() }, { headers: { Referer: url } });
      return d.signedVideoUrl ? [{ url: d.signedVideoUrl, quality: "auto", headers: { Referer: origin(url) + "/", Origin: origin(url) } }] : [];
    }
    const hex = (html.match(/_0x1 = '([0-9a-f|]+)'/) || [])[1];
    if (hex) {
      const s = hex.split("|").join("").replace(/../g, (b) => String.fromCharCode(parseInt(b, 16)));
      return [{ url: s.split("").reverse().join(""), quality: "auto", headers: { Referer: origin(url) + "/" } }];
    }
    if (html.includes("application/json") && /window\.location\.href|voe/i.test(html)) return voe(url, referer);
    if (/window\.location\.href = 'https?:\/\/[^']+\/e\/\w+'/.test(html)) return voe(url, referer);
    if (html.includes("sourcesCode:")) return vidoza(url);
    if (html.includes("botlink').innerHTML")) return streamtape(url);
    let found = jwplayer(unpack(html) || "", url);
    if (!found.length) found = jwplayer(scripts(html).find((s) => s.includes("sources:")) || "", url);
    return found.map((s) => Object.assign(s, { headers: { Referer: origin(url) + "/", Origin: origin(url), "User-Agent": UA } }));
  });
}

// shared/extractors/index.js
var HOSTS = [
  [voe, ["voe.sx", "goofy-banana.com", "urochsunloath.com", "donaldlineelse.com", "charlestoughrace.com", "tubelessceliolymph.com", "simpulumlamerop.com", "nathanfromsubject.com", "yip.su", "metagnathtuggers.com"]],
  [vidstack, ["moflix.upns.xyz", "moflix.rpmplay.xyz"]],
  [supervideo, ["supervideo", "dropload", "abstream.to", "dr0pstream.com"]],
  [vidhidepro, ["vidhide", "filelions", "ryderjet.com", "moflix-stream.click", "smoothpre.com", "dhtpre.com", "peytonepre.com"]],
  [streamwish, ["streamwish", "luluvdo.com", "streamruby.com", "savefiles.com", "wishembed", "swdyu.com", "strwish"]],
  [lulustream, ["lulustream.com", "luluvdoo.com"]],
  [mixdrop, ["mixdrop", "mixdrp", "mxdrop", "mdy48tn97.com"]],
  [filemoon, ["filemoon"]],
  [vidoza, ["vidoza.net", "videzz.net"]],
  [streamtape, ["streamtape", "watchadsontape.com", "shavetape.cash"]]
];
var BLOCKED = ["dood", "d000d.com", "vide0.net", "dsvplay.com", "dooodster.com", "doods.pro", "playmogo.com", "d0000d.com", "ds2play.com", "doodstream.com", "do7go.com"];
var hostOf = (url) => (url.match(/^https?:\/\/(?:www\.)?([^/:?#]+)/i) || [])[1] || "";
var listed = (host, names) => names.some((n) => host === n || !n.includes(".") && host.includes(n));
function decoderFor(url) {
  const hit = HOSTS.find(([, names]) => listed(hostOf(url), names));
  return hit ? hit[0] : sniff;
}
function resolveEmbed(url, referer) {
  return __async(this, null, function* () {
    if (!url) return [];
    if (url.startsWith("//")) url = "https:" + url;
    if (listed(hostOf(url), BLOCKED)) return [];
    try {
      const host = url.split("/")[2].replace(/^www\./, "");
      return (yield decoderFor(url)(url, referer)).filter((s) => s.url).map((s) => Object.assign(s, { host }));
    } catch (e) {
      console.error(`[extractor] ${url}: ${e.message}`);
      return [];
    }
  });
}

// src/moflix/index.js
var BASE = "https://moflix-stream.xyz";
var api = (path) => getJson(`${BASE}/api/v1/${path}`, { headers: { Referer: `${BASE}/` } });
function getStreams(tmdbId, mediaType, season, episode) {
  return __async(this, null, function* () {
    try {
      const meta = yield getMeta(tmdbId, mediaType);
      const isSeries = mediaType === "tv";
      let hit = null;
      for (const q of meta.titles) {
        const html = yield getText(`${BASE}/search/${encodeURIComponent(q)}`);
        const data = JSON.parse((html.match(/window\.bootstrapData\s*=\s*(\{[\s\S]*?\});\s*<\/script>/) || [])[1] || "{}");
        const items = (((data.loaders || {}).searchPage || {}).results || []).filter((r) => r.model_type === "title" && !!r.is_series === isSeries).map((r) => ({ id: r.id, title: r.name, year: r.year, tmdb: String(r.tmdb_id) }));
        hit = items.find((i) => i.tmdb === meta.tmdbId) || pickBest(items, meta);
        if (hit) break;
      }
      if (!hit) return [];
      const videos = isSeries ? ((yield api(`titles/${hit.id}/seasons/${season}/episodes/${episode}?loader=episodePage`)).episode || {}).videos : (yield api(`titles/${hit.id}?loader=titlePage`)).title.videos;
      const full = (videos || []).filter((v) => v.src && /^full$/i.test(v.category || "") && !/premium/i.test(v.name || ""));
      return gather(germanFirst(full, (v) => v.language || "de"), (v) => __async(null, null, function* () {
        const lang = (v.language || "de").toUpperCase();
        const links = v.type === "stream" ? [{ url: v.src, quality: v.quality, host: v.name, headers: { Referer: `${BASE}/` } }] : yield resolveEmbed(v.src, `${BASE}/`);
        return links.map((s) => ({
          name: "Moflix",
          title: `${s.host} \xB7 ${lang} \xB7 ${v.quality || s.quality || "auto"}`,
          url: s.url,
          quality: (`${s.quality} ${v.quality}`.match(/\d{3,4}p/) || ["auto"])[0],
          headers: s.headers
        }));
      }));
    } catch (e) {
      console.error(`[Moflix] ${e.message}`);
    }
    return [];
  });
}
module.exports = provider(getStreams);
