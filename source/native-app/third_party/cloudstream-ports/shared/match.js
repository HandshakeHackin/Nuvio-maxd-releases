export function norm(s) {
    s = String(s || '').toLowerCase();
    if (s.normalize) s = s.normalize('NFD').replace(/[̀-ͯ]/g, '');
    return s.replace(/ß/g, 'ss').replace(/&/g, ' and ').replace(/[^a-z0-9]+/g, ' ').trim();
}

export function score(title, year, meta) {
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

// items: [{title, year?, ...}] -> best item scoring >= 3, else null
export function pickBest(items, meta) {
    let best = null, bestScore = 2;
    for (const it of items) {
        const s = score(it.title, it.year, meta);
        if (s > bestScore) { best = it; bestScore = s; }
    }
    return best;
}
