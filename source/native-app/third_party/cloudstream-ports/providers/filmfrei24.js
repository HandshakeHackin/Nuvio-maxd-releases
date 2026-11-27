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
  const quality = s.quality && s.quality !== "auto" ? s.quality : /\.m3u8|\/hls|master/i.test(s.url) ? "HLS" : "MP4";
  const host = ((s.title || "").split(" \xB7 ")[0].match(/^[\w-]+\.[a-z]{2,}$/) || [])[0];
  const tags = releaseTags(fileNames[s.url]);
  if (tags.length) title = [title].concat(tags).filter(Boolean).join(" \xB7 ");
  const stream = Object.assign({}, s, { name: [s.name, flagLabel(lang), host].filter(Boolean).join(" \xB7 "), title, quality });
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

// src/filmfrei24/index.js
var BASE = "https://filmfrei24.com";
function catalog(html) {
  let s = html.slice(html.indexOf("const ALL_FILMS"));
  s = s.slice(s.indexOf("=") + 1);
  return JSON.parse(s.slice(0, s.indexOf("\n")).trim().replace(/;$/, ""));
}
function getStreams(tmdbId, mediaType, season, episode) {
  return __async(this, null, function* () {
    try {
      const tv = mediaType === "tv";
      const films = catalog(yield getText(BASE + (tv ? "/serien/" : "/")));
      let hit = films.find((f) => String(f.id) === String(tmdbId));
      if (!hit) hit = pickBest(films, yield getMeta(tmdbId, mediaType));
      if (!hit) return [];
      let url = hit.video;
      if (tv) {
        if (!hit._series_id || !season || !episode) return [];
        const html = yield getText(`${BASE}/player/?s=${encodeURIComponent(hit._series_id)}&type=series&season=${season}&ep=${episode}`);
        url = (html.match(/<video[^>]*data-src="([^"]+)"/) || [])[1];
      }
      if (!url || !/^https?:/.test(url)) return [];
      return [{
        name: "FilmFrei24",
        title: `${hit.language || "Deutsch"} \xB7 ${/\.m3u8/.test(url) ? "HLS" : "MP4"}`,
        url,
        quality: "auto",
        headers: { Referer: BASE + "/" }
      }];
    } catch (e) {
      console.error(`[FilmFrei24] ${e.message}`);
    }
    return [];
  });
}
module.exports = provider(getStreams);
