#!/usr/bin/env python3
"""Gera as fontes com acentos da HMI (Latin-1) no formato nativo do LVGL.

Por que existe: as fontes Montserrat embutidas no LVGL 9 so trazem ASCII
(0x20-0x7F), o grau e alguns simbolos. Textos em portugues ("Ignicao" com
cedilha e til, "Correcoes", "Combustivel" com acento) apareceriam com buracos.
Este script le a mesma Montserrat-Medium.ttf que o LVGL usa (licenca SIL OFL
1.1, distribuida junto com o LVGL em scripts/built_in_font/) e gera so os
caracteres que faltam: U+00A0-U+00FF, travessao, reticencias e seta.

Cada fonte gerada usa a Montserrat embutida do mesmo tamanho como "fallback":
o LVGL procura a letra na nossa fonte e, se nao achar (letras ASCII e
simbolos), usa a embutida. Assim o desenho e as metricas ficam iguais.

So usa a biblioteca padrao do Python. Uso:
    python tools/gen_font_latin1.py <Montserrat-Medium.ttf> <pasta_saida>
"""
import math
import struct
import sys
from pathlib import Path

# Tamanho -> (line_height, base_line) copiados das fontes embutidas do LVGL 9.2.2
# (src/font/lv_font_montserrat_<n>.c), para a fonte nova encaixar na mesma linha.
SIZES = {14: (16, 3), 16: (18, 3), 20: (22, 4), 28: (30, 5)}
CODEPOINTS = list(range(0xA0, 0x100)) + [0x2013, 0x2026, 0x2192]
SUBSAMPLES = 16  # sub-linhas por pixel no rasterizador (antisserrilhado vertical)
BPP = 4


class Ttf:
    """Leitor minimo de TrueType: cmap formato 4, hmtx, loca e glyf (simples e composto)."""

    def __init__(self, data: bytes):
        self.d = data
        num = struct.unpack(">H", data[4:6])[0]
        self.t = {}
        for i in range(num):
            tag, _cs, off, ln = struct.unpack(">4sIII", data[12 + 16 * i:28 + 16 * i])
            self.t[tag.decode("latin-1")] = (off, ln)
        head = self.t["head"][0]
        self.upem = struct.unpack(">H", data[head + 18:head + 20])[0]
        self.loc_long = struct.unpack(">h", data[head + 50:head + 52])[0] == 1
        hhea = self.t["hhea"][0]
        self.num_hmetrics = struct.unpack(">H", data[hhea + 34:hhea + 36])[0]
        maxp = self.t["maxp"][0]
        self.num_glyphs = struct.unpack(">H", data[maxp + 4:maxp + 6])[0]
        self.cmap = self._read_cmap()

    def _u16(self, o):
        return struct.unpack(">H", self.d[o:o + 2])[0]

    def _i16(self, o):
        return struct.unpack(">h", self.d[o:o + 2])[0]

    def _read_cmap(self):
        base = self.t["cmap"][0]
        n = self._u16(base + 2)
        sub = None
        for i in range(n):
            pid, eid, so = struct.unpack(">HHI", self.d[base + 4 + 8 * i:base + 12 + 8 * i])
            if self._u16(base + so) == 4 and (pid, eid) in ((3, 1), (0, 3)):
                sub = base + so
                break
        if sub is None:
            raise SystemExit("cmap formato 4 nao encontrado")
        segx2 = self._u16(sub + 6)
        ends = sub + 14
        starts = ends + segx2 + 2
        deltas = starts + segx2
        ranges = deltas + segx2
        cmap = {}
        for s in range(segx2 // 2):
            end = self._u16(ends + 2 * s)
            start = self._u16(starts + 2 * s)
            delta = self._i16(deltas + 2 * s)
            ro = self._u16(ranges + 2 * s)
            for c in range(start, end + 1):
                if c == 0xFFFF:
                    continue
                if ro == 0:
                    g = (c + delta) & 0xFFFF
                else:
                    g = self._u16(ranges + 2 * s + ro + 2 * (c - start))
                    if g:
                        g = (g + delta) & 0xFFFF
                if g:
                    cmap[c] = g
        return cmap

    def advance(self, gid):
        base = self.t["hmtx"][0]
        i = min(gid, self.num_hmetrics - 1)
        return self._u16(base + 4 * i)

    def _glyph_range(self, gid):
        loca = self.t["loca"][0]
        if self.loc_long:
            a, b = struct.unpack(">II", self.d[loca + 4 * gid:loca + 4 * gid + 8])
        else:
            a, b = (2 * x for x in struct.unpack(">HH", self.d[loca + 2 * gid:loca + 2 * gid + 4]))
        glyf = self.t["glyf"][0]
        return glyf + a, b - a

    def contours(self, gid, depth=0):
        """Contornos do glifo em unidades da fonte: lista de listas de (x, y, on_curve)."""
        off, ln = self._glyph_range(gid)
        if ln == 0:
            return []
        n_contours = self._i16(off)
        p = off + 10
        if n_contours >= 0:
            end_pts = [self._u16(p + 2 * i) for i in range(n_contours)]
            p += 2 * n_contours
            ins_len = self._u16(p)
            p += 2 + ins_len
            n_pts = end_pts[-1] + 1 if end_pts else 0
            flags = []
            while len(flags) < n_pts:
                f = self.d[p]
                p += 1
                flags.append(f)
                if f & 8:
                    r = self.d[p]
                    p += 1
                    flags.extend([f] * r)
            xs, x = [], 0
            for f in flags:
                if f & 2:
                    dx = self.d[p]
                    p += 1
                    x += dx if f & 16 else -dx
                elif not f & 16:
                    x += self._i16(p)
                    p += 2
                xs.append(x)
            ys, y = [], 0
            for f in flags:
                if f & 4:
                    dy = self.d[p]
                    p += 1
                    y += dy if f & 32 else -dy
                elif not f & 32:
                    y += self._i16(p)
                    p += 2
                ys.append(y)
            out, start = [], 0
            for e in end_pts:
                out.append([(xs[i], ys[i], bool(flags[i] & 1)) for i in range(start, e + 1)])
                start = e + 1
            return out
        # Glifo composto (ex.: "c" + cedilha, "a" + til)
        if depth > 8:
            return []
        out = []
        while True:
            flags = self._u16(p)
            comp = self._u16(p + 2)
            p += 4
            if flags & 1:
                a1, a2 = self._i16(p), self._i16(p + 2)
                p += 4
            else:
                a1, a2 = struct.unpack(">bb", self.d[p:p + 2])
                p += 2
            dx, dy = (a1, a2) if flags & 2 else (0, 0)
            xx, xy, yx, yy = 1.0, 0.0, 0.0, 1.0
            if flags & 8:
                xx = yy = self._i16(p) / 16384.0
                p += 2
            elif flags & 0x40:
                xx, yy = self._i16(p) / 16384.0, self._i16(p + 2) / 16384.0
                p += 4
            elif flags & 0x80:
                xx, xy, yx, yy = (self._i16(p + 2 * k) / 16384.0 for k in range(4))
                p += 8
            for c in self.contours(comp, depth + 1):
                out.append([(x * xx + y * yx + dx, x * xy + y * yy + dy, on) for x, y, on in c])
            if not flags & 0x20:
                break
        return out


def flatten(contour, steps):
    """Converte um contorno TrueType (curvas quadraticas) em poligono."""
    pts = list(contour)
    if not pts:
        return []
    # Garante um ponto inicial sobre a curva
    if not pts[0][2]:
        if pts[-1][2]:
            pts = [pts[-1]] + pts[:-1]
        else:
            mx = (pts[0][0] + pts[-1][0]) / 2
            my = (pts[0][1] + pts[-1][1]) / 2
            pts = [(mx, my, True)] + pts
    poly = [(pts[0][0], pts[0][1])]
    n = len(pts)
    i = 1
    cur = pts[0]
    while i <= n:
        p = pts[i % n]
        if p[2]:
            poly.append((p[0], p[1]))
            cur = p
            i += 1
            continue
        nxt = pts[(i + 1) % n]
        if nxt[2]:
            end = nxt
            i += 2
        else:
            end = ((p[0] + nxt[0]) / 2, (p[1] + nxt[1]) / 2, True)
            i += 1
        for s in range(1, steps + 1):
            t = s / steps
            a = (1 - t) ** 2
            b = 2 * (1 - t) * t
            c = t * t
            poly.append((a * cur[0] + b * p[0] + c * end[0], a * cur[1] + b * p[1] + c * end[1]))
        cur = end
    return poly


def rasterize(polys, w, h, x0, ytop):
    """Cobertura (0..1) por pixel com regra de enrolamento nao nulo."""
    cov = [[0.0] * w for _ in range(h)]
    edges = []
    for poly in polys:
        for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
            if ay == by:
                continue
            wdir = 1 if by > ay else -1
            if ay > by:
                ax, ay, bx, by = bx, by, ax, ay
            edges.append((ay, by, ax, (bx - ax) / (by - ay), wdir))
    for row in range(h):
        acc = cov[row]
        for k in range(SUBSAMPLES):
            y = ytop - row - (k + 0.5) / SUBSAMPLES  # coordenada y (para cima) da sub-linha
            xs = []
            for ay, by, ax, slope, wdir in edges:
                if ay <= y < by:
                    xs.append((ax + (y - ay) * slope - x0, wdir))
            if not xs:
                continue
            xs.sort()
            wind = 0
            for idx in range(len(xs) - 1):
                wind += xs[idx][1]
                if wind == 0:
                    continue
                l, r = xs[idx][0], xs[idx + 1][0]
                if r <= l:
                    continue
                li, ri = int(math.floor(l)), int(math.floor(r))
                if li == ri:
                    if 0 <= li < w:
                        acc[li] += (r - l)
                    continue
                if 0 <= li < w:
                    acc[li] += (li + 1 - l)
                for px in range(max(li + 1, 0), min(ri, w)):
                    acc[px] += 1.0
                if 0 <= ri < w:
                    acc[ri] += (r - ri)
        for px in range(w):
            acc[px] = min(1.0, acc[px] / SUBSAMPLES)
    return cov


def render(ttf: Ttf, cp: int, size: int):
    gid = ttf.cmap.get(cp)
    if gid is None:
        return None
    scale = size / ttf.upem
    adv_w = round(ttf.advance(gid) * scale * 16)
    polys = [flatten(c, 6) for c in ttf.contours(gid)]
    polys = [[(x * scale, y * scale) for x, y in p] for p in polys if len(p) > 2]
    if not polys:
        return dict(cp=cp, adv_w=adv_w, w=0, h=0, ox=0, oy=0, bits=[])
    xs = [x for p in polys for x, _ in p]
    ys = [y for p in polys for _, y in p]
    x0, x1 = math.floor(min(xs)), math.ceil(max(xs))
    y0, y1 = math.floor(min(ys)), math.ceil(max(ys))
    w, h = x1 - x0, y1 - y0
    cov = rasterize(polys, w, h, x0, y1)
    vals = [round(c * 15) for r in cov for c in r]
    # Recorta linhas/colunas vazias nas bordas (como o lv_font_conv faz)
    rows = [vals[i * w:(i + 1) * w] for i in range(h)]
    while rows and not any(rows[0]):
        rows.pop(0)
        y1 -= 1
    while rows and not any(rows[-1]):
        rows.pop()
        y0 += 1
    if not rows:
        return dict(cp=cp, adv_w=adv_w, w=0, h=0, ox=0, oy=0, bits=[])
    while all(r[0] == 0 for r in rows):
        rows = [r[1:] for r in rows]
        x0 += 1
    while all(r[-1] == 0 for r in rows):
        rows = [r[:-1] for r in rows]
    w, h = len(rows[0]), len(rows)
    return dict(cp=cp, adv_w=adv_w, w=w, h=h, ox=x0, oy=y0, bits=[v for r in rows for v in r])


def pack4(vals):
    out = bytearray()
    for i in range(0, len(vals), 2):
        hi = vals[i]
        lo = vals[i + 1] if i + 1 < len(vals) else 0
        out.append((hi << 4) | lo)
    return out


def emit(ttf: Ttf, size: int, out_dir: Path):
    line_height, base_line = SIZES[size]
    glyphs = [g for g in (render(ttf, cp, size) for cp in CODEPOINTS) if g]
    name = f"hmi_font_{size}"
    bitmap = bytearray()
    dsc_lines = ["    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reservado */,"]
    for g in glyphs:
        idx = len(bitmap)
        bitmap += pack4(g["bits"])
        dsc_lines.append(
            f"    {{.bitmap_index = {idx}, .adv_w = {g['adv_w']}, .box_w = {g['w']}, .box_h = {g['h']}, "
            f".ofs_x = {g['ox']}, .ofs_y = {g['oy']}}} /* U+{g['cp']:04X} */,")
    start = glyphs[0]["cp"]
    offsets = [g["cp"] - start for g in glyphs]
    bmp_lines = []
    for i in range(0, len(bitmap), 16):
        bmp_lines.append("    " + ", ".join(f"0x{b:02x}" for b in bitmap[i:i + 16]) + ",")
    text = f"""/*
 * {name}: caracteres Latin-1 (U+00A0-U+00FF), travessao e reticencias
 * da Montserrat Medium {size} px, 4 bpp, sem compressao.
 * ARQUIVO GERADO por tools/gen_font_latin1.py - nao edite a mao.
 * Fonte: Montserrat-Medium.ttf (SIL Open Font License 1.1), a mesma do LVGL.
 * Letras que nao estao aqui (ASCII e simbolos) vem de lv_font_montserrat_{size}.
 */
#include "lvgl.h"

#if !LV_FONT_MONTSERRAT_{size}
#error "Ative LV_FONT_MONTSERRAT_{size} no lv_conf.h: e o fallback desta fonte"
#endif

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {{
{chr(10).join(bmp_lines)}
}};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
{chr(10).join(dsc_lines)}
}};

static const uint16_t unicode_list[] = {{
    {", ".join(f"0x{o:x}" for o in offsets)}
}};

static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = {start}, .range_length = {offsets[-1] + 1}, .glyph_id_start = 1,
        .unicode_list = unicode_list, .glyph_id_ofs_list = NULL, .list_length = {len(offsets)},
        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }}
}};

static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = {BPP},
    .kern_classes = 0,
    .bitmap_format = 0,
}};

const lv_font_t {name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height},
    .base_line = {base_line},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -1,
    .underline_thickness = 1,
    .dsc = &font_dsc,
    .fallback = &lv_font_montserrat_{size},
}};
"""
    path = out_dir / f"{name}.c"
    path.write_text(text, encoding="utf-8", newline="\n")
    return path, len(glyphs), len(bitmap)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    ttf = Ttf(Path(sys.argv[1]).read_bytes())
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    for size in SIZES:
        path, n, nbytes = emit(ttf, size, out_dir)
        print(f"{path}: {n} glifos, {nbytes} bytes de bitmap")


if __name__ == "__main__":
    main()
