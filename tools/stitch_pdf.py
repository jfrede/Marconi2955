#!/usr/bin/env python3
"""Setzt in einem Scan-PDF die in zwei A4-Haelften gescannten Grossplaene wieder zusammen.

Ablauf:
 1. Seiten ab --first mit 300 dpi als Graustufen rendern
 2. fuer jede Folgeseite pruefen, ob sie die rechte Haelfte der vorherigen ist
    (Template-Matching der ~1,2 cm breiten Ueberlappung, Verschiebung dx/dy)
 3. Paare entlang einer Naht mit minimalem Strichverlauf zusammensetzen
 4. zusammengesetzte Seiten mit Tesseract erneut texterkennen (unsichtbare Textebene)
 5. neues PDF bauen: alle anderen Seiten bleiben unveraendert (inkl. vorhandener Textebene)

Usage: stitch_pdf.py eingabe_OCR.pdf ausgabe.pdf --first 288
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import cv2
import img2pdf
import numpy as np
import pikepdf

DPI = 300
# Bestueckungsplaene fast ohne Strichinhalt in der Ueberlappung: Versatz (dx, dy) per Hand
# anhand des durchlaufenden Fusszeilentexts ("Component layout : ...") bestimmt.
# Gilt nur fuer Marconi--2955--service--ID11535.pdf; Schluessel = linke Seite des Paares.
MANUAL_SHIFT = {302: (2350, -9), 378: (2319, -7), 383: (2338, 4), 387: (2339, -6), 395: (2324, -1)}


def render(src, first, last, out, jobs):
    def chunk(r):
        a, b = r
        subprocess.run(["pdftoppm", "-f", str(a), "-l", str(b), "-r", str(DPI), "-gray", "-png",
                        src, str(out / "p")], check=True)
    step = max(1, (last - first + 1) // jobs + 1)
    ranges = [(s, min(s + step - 1, last)) for s in range(first, last + 1, step)]
    with ThreadPoolExecutor(jobs) as ex:
        list(ex.map(chunk, ranges))


def load(out, p):
    f = sorted(out.glob(f"p-*{p}.png"))
    f = [x for x in f if int(x.stem.split("-")[1]) == p]
    return cv2.imread(str(f[0]), 0)


def ink(x, s):
    return cv2.GaussianBlur(255 - x, (0, 0), s).astype(np.float32)


def best_match(A, B, tw, x0, sigma, search_from):
    """Position der linken Randzone von B in A: (score, dx, dy)."""
    H = min(A.shape[0], B.shape[0])
    m = 70
    T = ink(B[m:H - m, x0:x0 + tw], sigma)
    S = ink(A[:H, search_from:], sigma)
    r = cv2.matchTemplate(S, T, cv2.TM_CCOEFF_NORMED)
    _, mx, _, loc = cv2.minMaxLoc(r)
    return mx, search_from + loc[0] - x0, loc[1] - m


def find_pairs(out, first, last):
    pairs, p = {}, first
    cache = {}

    def get(q):
        if q not in cache:
            cache[q] = load(out, q)
        return cache[q]

    while p <= last:
        if p < last:
            A, B = get(p), get(p + 1)
            # breites Template (inkl. Fusszeile) fuer die Paar-Erkennung ...
            s1, dx1, dy1 = best_match(A, B, 135, 6, 2.0, 2180)
            if s1 >= 0.4 and 2300 <= dx1 <= 2345 and abs(dy1) <= 30:
                # ... schmales Template fuer den genauen Versatz (breites ist bei dx>2339 abgeschnitten)
                s2, dx2, dy2 = best_match(A, B, 60, 4, 1.5, 2250)
                if p in MANUAL_SHIFT:
                    dx2, dy2 = MANUAL_SHIFT[p]
                elif s2 < 0.6:
                    print(f"  Warnung: Seite {p}/{p+1} nur schwache Uebereinstimmung ({s2:.2f})", file=sys.stderr)
                pairs[p] = (int(dx2), int(dy2))
                cache.pop(p - 1, None)
                p += 2
                continue
        cache.pop(p - 1, None)
        p += 1
    return pairs


def stitch(A, B, dx, dy):
    HA, WA = A.shape
    HB, WB = B.shape
    ya = max(0, -dy)
    yb = ya + dy
    H, W = max(ya + HA, yb + HB), dx + WB
    CA = np.full((H, W), 255, np.uint8)
    CB = np.full((H, W), 255, np.uint8)
    CA[ya:ya + HA, :WA] = A
    CB[yb:yb + HB, dx:dx + WB] = B

    y0, y1 = max(ya, yb), min(ya + HA, yb + HB)
    x0, x1 = dx + 6, WA - 6
    # Hintergrundhelligkeit angleichen (graue Papierflaechen bei Bestueckungsplaenen)
    medA, medB = np.median(CA[y0:y1, x0:x1]), np.median(CB[y0:y1, x0:x1])
    if abs(medA - medB) > 2:
        blk = CB[yb:yb + HB, dx:dx + WB].astype(np.int16) + int(medA - medB)
        CB[yb:yb + HB, dx:dx + WB] = np.clip(blk, 0, 255).astype(np.uint8)

    a_ = CA[y0:y1, x0:x1].astype(np.float32)
    b_ = CB[y0:y1, x0:x1].astype(np.float32)
    cost = cv2.GaussianBlur((255 - np.minimum(a_, b_)) / 255.0 + np.abs(a_ - b_) / 255.0, (0, 0), 1.0)
    n, m = cost.shape
    acc, back = cost.copy(), np.zeros((n, m), np.int8)
    for i in range(1, n):
        prev = acc[i - 1]
        st = np.stack([np.r_[np.inf, prev[:-1]], prev, np.r_[prev[1:], np.inf]])
        k = st.argmin(0)
        acc[i] += st[k, np.arange(m)]
        back[i] = k - 1
    seam = np.zeros(n, np.int32)
    seam[-1] = acc[-1].argmin()
    for i in range(n - 1, 0, -1):
        seam[i - 1] = seam[i] + back[i, seam[i]]
    seam_x = np.full(H, (x0 + x1) // 2, np.int32)
    seam_x[y0:y1] = x0 + seam
    return np.where(np.arange(W)[None, :] >= seam_x[:, None], CB, CA)


def ocr(png, lang, psm, scale):
    # kleine Bauteilbeschriftungen werden hochskaliert deutlich besser erkannt
    big = png.with_name(png.stem + "_ocr.png")
    img = cv2.imread(str(png), 0)
    cv2.imwrite(str(big), cv2.resize(img, None, fx=scale, fy=scale, interpolation=cv2.INTER_CUBIC))
    base = png.with_name(png.stem + "_ocr")
    subprocess.run(["tesseract", str(big), str(base), "-l", lang, "--psm", psm, "--dpi", str(DPI * scale),
                    "-c", "textonly_pdf=1", "-c", "preserve_interword_spaces=1", "pdf"],
                   check=True, env=dict(os.environ, OMP_THREAD_LIMIT="1"),
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return base.with_suffix(".pdf")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--first", type=int, default=288)
    ap.add_argument("--last", type=int)
    ap.add_argument("--lang", default="eng_best")
    ap.add_argument("--psm", default="11", help="11 = sparse text, passt fuer Schaltplaene")
    ap.add_argument("--scale", type=int, default=2, help="Hochskalierung fuer die OCR")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    a = ap.parse_args()

    with pikepdf.open(a.input) as src:
        total = len(src.pages)
    last = a.last or total

    with tempfile.TemporaryDirectory(prefix="stitch_") as tmp:
        tmp = Path(tmp)
        print(f"Rendere Seiten {a.first}-{last} ...", flush=True)
        render(a.input, a.first, last, tmp, a.jobs)

        print("Suche zusammengehoerige Seitenpaare ...", flush=True)
        pairs = find_pairs(tmp, a.first, last)
        print(f"  {len(pairs)} Paare:", sorted(pairs), flush=True)

        print("Setze Paare zusammen ...", flush=True)
        pngs = {}
        for p, (dx, dy) in pairs.items():
            img = stitch(load(tmp, p), load(tmp, p + 1), dx, dy)
            f = tmp / f"s{p}.png"
            cv2.imwrite(str(f), img)
            pngs[p] = f

        print("OCR der zusammengesetzten Seiten ...", flush=True)
        with ThreadPoolExecutor(a.jobs) as ex:
            texts = dict(zip(pngs, ex.map(lambda f: ocr(f, a.lang, a.psm, a.scale), pngs.values())))

        print("Baue PDF ...", flush=True)
        layout = img2pdf.get_fixed_dpi_layout_fun((DPI, DPI))
        with pikepdf.open(a.input) as src, pikepdf.Pdf.new() as dst:
            keep = []
            for i in range(1, total + 1):
                if i in pairs:
                    page_pdf = pikepdf.open(__import__("io").BytesIO(
                        img2pdf.convert(str(pngs[i]), layout_fun=layout)))
                    txt = pikepdf.open(texts[i])
                    page = page_pdf.pages[0]
                    pikepdf.Page(page).add_overlay(txt.pages[0], pikepdf.Rectangle(*[float(x) for x in page.mediabox]))
                    dst.pages.append(page)
                    keep += [page_pdf, txt]
                elif (i - 1) in pairs:
                    continue
                else:
                    dst.pages.append(src.pages[i - 1])
            dst.save(a.output, compress_streams=True)
            for k in keep:
                k.close()
    print("Fertig:", a.output)


if __name__ == "__main__":
    main()
