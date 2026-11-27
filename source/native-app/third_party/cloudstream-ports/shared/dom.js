// Nuvio's cheerio is a jsoup-backed shim: only $(sel[, ctx]), find, first, eq, text, html, attr, each, map, toArray, length are safe.
const cheerio = require('cheerio');

export const load = html => cheerio.load(html);

export const all = ($, sel, ctx) => (ctx ? $(sel, ctx) : $(sel)).toArray().map(e => $(e));
