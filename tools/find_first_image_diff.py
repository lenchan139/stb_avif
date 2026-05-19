#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image


def load_rgba(path: Path) -> np.ndarray:
    return np.array(Image.open(path).convert("RGBA"), dtype=np.uint8)


def summarize_pair(output_path: Path, reference_path: Path, radius: int) -> int:
    output = load_rgba(output_path)
    reference = load_rgba(reference_path)

    print(f"PAIR {output_path.name} vs {reference_path.name}")
    print(f"  output_shape={tuple(output.shape)} reference_shape={tuple(reference.shape)}")
    if output.shape != reference.shape:
        print("  status=shape-mismatch")
        return 2

    diff = np.any(output != reference, axis=2)
    differing_pixels = int(diff.sum())
    print(f"  differing_pixels={differing_pixels}")
    if differing_pixels == 0:
        print("  status=exact-match")
        return 0

    y, x = np.argwhere(diff)[0]
    max_abs = np.abs(output.astype(np.int16) - reference.astype(np.int16)).max(axis=2)
    y0 = max(0, y - radius)
    y1 = min(output.shape[0], y + radius + 1)
    x0 = max(0, x - radius)
    x1 = min(output.shape[1], x + radius + 1)

    print(f"  first_diff_xy=({x},{y})")
    print(f"  output_rgba={output[y, x].tolist()}")
    print(f"  reference_rgba={reference[y, x].tolist()}")
    print(f"  max_channel_absdiff_at_first={int(max_abs[y, x])}")
    print(f"  local_window=({x0},{y0})-({x1 - 1},{y1 - 1})")
    print(f"  local_differing_pixels={int(diff[y0:y1, x0:x1].sum())}")
    print(f"  global_max_channel_absdiff={int(max_abs.max())}")

    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Locate the first differing pixel/block between two images.")
    parser.add_argument("output", type=Path, help="Path to decoder output image")
    parser.add_argument("reference", type=Path, help="Path to reference image")
    parser.add_argument("--radius", type=int, default=4, help="Half-size of local diff window")
    args = parser.parse_args()

    return summarize_pair(args.output, args.reference, args.radius)


if __name__ == "__main__":
    raise SystemExit(main())