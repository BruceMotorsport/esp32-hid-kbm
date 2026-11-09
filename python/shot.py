#!/usr/bin/env python
"""Screenshot the whole virtual desktop through PIL (cv2/pyautogui are broken here).

    python shot.py                 -> %TEMP%\\hid\\shot.png at native 1920x1080 (coords are 1:1)
    python shot.py --small         -> 1280x720 (cheaper to read; multiply any measured x,y by 1.5)
    python shot.py --out foo.png   -> your own path

Prints the absolute path and the native size. Read the PNG with the Read tool.
"""
import argparse
import os
import sys

from PIL import Image, ImageGrab


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None, help="destination .png (default: %%TEMP%%\\hid\\shot.png)")
    ap.add_argument("--small", action="store_true", help="downscale to 1280 wide")
    ap.add_argument("--seq", action="store_true", help="append an incrementing number so shots are not overwritten")
    args = ap.parse_args()

    out = args.out or os.path.join(os.environ.get("TEMP", "."), "hid", "shot.png")
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    if args.seq:
        stem, ext = os.path.splitext(out)
        n = 0
        while os.path.exists(f"{stem}_{n}{ext}"):
            n += 1
        out = f"{stem}_{n}{ext}"

    img = ImageGrab.grab(all_screens=True)
    native = img.size
    if args.small:
        w, h = native
        img = img.resize((1280, round(1280 * h / w)), Image.LANCZOS)
    img.save(out)

    print(out)
    print(f"native={native[0]}x{native[1]}  saved={img.size[0]}x{img.size[1]}  "
          f"{'coords x1.5' if args.small else 'coords 1:1'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
