#!/usr/bin/env python3
"""Fuegt gescannten PDFs eine unsichtbare Textebene hinzu (Tesseract, 'best'-Modell).

Seiten werden als Graustufen-PNG gerendert (glaettet 1-Bit-Scans), Tesseract erzeugt
pro Seite ein Text-only-PDF, das als Overlay ueber die unveraenderte Originalseite gelegt wird.
Usage: ocr_pdf.py eingabe.pdf ausgabe.pdf [--lang eng_best] [--dpi 300] [--jobs 12]
"""
import argparse
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pikepdf


def ocr_page(img: Path, lang: str, psm: str, dpi: int) -> Path:
    base = img.with_suffix("")
    env = dict(os.environ, OMP_THREAD_LIMIT="1")
    subprocess.run(
        ["tesseract", str(img), str(base), "-l", lang, "--psm", psm, "--dpi", str(dpi),
         "-c", "textonly_pdf=1", "-c", "preserve_interword_spaces=1", "pdf"],
        check=True, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return base.with_suffix(".pdf")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--lang", default="eng_best")
    ap.add_argument("--dpi", type=int, default=300)
    ap.add_argument("--psm", default="3")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    a = ap.parse_args()

    with tempfile.TemporaryDirectory(prefix="ocrpdf_") as tmp:
        tmp = Path(tmp)
        print("Rendere Seiten ...", flush=True)
        subprocess.run(["pdftoppm", "-r", str(a.dpi), "-gray", "-png", a.input, str(tmp / "p")], check=True)
        imgs = sorted(tmp.glob("p-*.png"))

        print(f"OCR von {len(imgs)} Seiten ...", flush=True)
        done = 0
        with ThreadPoolExecutor(a.jobs) as ex:
            futs = [ex.submit(ocr_page, i, a.lang, a.psm, a.dpi) for i in imgs]
            text_pdfs = []
            for f in futs:
                text_pdfs.append(f.result())
                done += 1
                if done % 25 == 0:
                    print(f"  {done}/{len(imgs)}", flush=True)

        print("Fuege Textebene ein ...", flush=True)
        with pikepdf.open(a.input) as pdf:
            if len(pdf.pages) != len(text_pdfs):
                sys.exit("Seitenzahl stimmt nicht ueberein")
            overlays = [pikepdf.open(tp) for tp in text_pdfs]
            for page, t in zip(pdf.pages, overlays):
                pikepdf.Page(page).add_overlay(t.pages[0], pikepdf.Rectangle(*[float(x) for x in page.mediabox]))
            pdf.save(a.output, compress_streams=True)
            for t in overlays:
                t.close()
    print("Fertig:", a.output)


if __name__ == "__main__":
    main()
