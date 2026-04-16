# ABOUTME: Extracts per-shade luminance from calibration photos of the Inkplate display.
# ABOUTME: Reads JPEG files, inverts sRGB gamma to linear light, outputs INKPLATE_SHADE_LUMINANCE.

"""
Usage:
    python3 tools/extract_shade_luminance.py ~/Pictures/"Inkplate Calibration"

Expects exactly 8 JPEG files in the directory, sorted by filename, corresponding
to Inkplate shades 0-7 photographed in ascending order.

Output is the INKPLATE_SHADE_LUMINANCE table for use in the image conversion
pipeline (tools/image_converter.py).
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image


# Sample the centre N% of the image to avoid edges and vignetting
SAMPLE_FRACTION = 0.15


def srgb_to_linear(values: np.ndarray) -> np.ndarray:
    """Invert sRGB gamma encoding to get linear light values (0.0-1.0)."""
    v = values / 255.0
    return np.where(v <= 0.04045, v / 12.92, ((v + 0.055) / 1.055) ** 2.4)


def extract_linear_luminance(jpeg_path: Path) -> float:
    """Return mean linear luminance (0.0-1.0) from the centre of a JPEG."""
    img = Image.open(jpeg_path).convert("RGB")
    arr = np.asarray(img, dtype=np.float32)

    h, w = arr.shape[:2]
    dy = int(h * SAMPLE_FRACTION)
    dx = int(w * SAMPLE_FRACTION)
    cy, cx = h // 2, w // 2
    centre = arr[cy - dy : cy + dy, cx - dx : cx + dx]

    linear = srgb_to_linear(centre)

    # Rec. 709 luminance weights
    luma = 0.2126 * linear[:, :, 0] + 0.7152 * linear[:, :, 1] + 0.0722 * linear[:, :, 2]
    return float(luma.mean())


def main(calibration_dir: Path) -> None:
    jpeg_files = sorted(calibration_dir.glob("*.JPG"))
    if not jpeg_files:
        jpeg_files = sorted(calibration_dir.glob("*.jpg"))

    if len(jpeg_files) != 8:
        print(f"Error: expected 8 JPEG files, found {len(jpeg_files)}", file=sys.stderr)
        sys.exit(1)

    print(f"Reading {len(jpeg_files)} JPEG files from {calibration_dir}\n")

    raw_luminances = []
    for shade, path in enumerate(jpeg_files):
        luma = extract_linear_luminance(path)
        raw_luminances.append(luma)
        print(f"  shade {shade}: {path.name}  ->  linear luma = {luma:.5f}")

    max_luma = raw_luminances[-1]
    if max_luma == 0:
        print("Error: shade 7 has zero luminance — check the photo", file=sys.stderr)
        sys.exit(1)

    normalised = [v / max_luma for v in raw_luminances]

    print("\n# Paste this into tools/image_converter.py:")
    print("INKPLATE_SHADE_LUMINANCE = [")
    for shade, v in enumerate(normalised):
        print(f"    {v:.4f},  # shade {shade}")
    print("]")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <calibration_dir>", file=sys.stderr)
        sys.exit(1)

    main(Path(sys.argv[1]).expanduser())
