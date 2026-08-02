# ABOUTME: Tests for the pure sRGB-to-linear and luminance math in tools/extract_shade_luminance.py.
import numpy as np
import pytest
from PIL import Image

from extract_shade_luminance import extract_linear_luminance, srgb_to_linear


def test_srgb_to_linear_endpoints():
    result = srgb_to_linear(np.array([0.0, 255.0]))
    assert result[0] == pytest.approx(0.0)
    assert result[1] == pytest.approx(1.0)


def test_srgb_to_linear_matches_known_midpoint():
    # sRGB 188/255 is the standard "perceptual middle gray" reference point (~50% linear luminance).
    result = srgb_to_linear(np.array([188.0]))
    assert result[0] == pytest.approx(0.5, abs=0.01)


def test_srgb_to_linear_below_and_above_toe_use_different_formulas():
    below_toe = srgb_to_linear(np.array([5.0]))  # v <= 0.04045 branch
    above_toe = srgb_to_linear(np.array([128.0]))
    assert below_toe[0] == pytest.approx(5.0 / 255.0 / 12.92)
    assert above_toe[0] > 0


def test_extract_linear_luminance_white_image(tmp_path):
    path = tmp_path / "white.jpg"
    Image.new("RGB", (100, 100), color=(255, 255, 255)).save(path, "JPEG", quality=95)
    assert extract_linear_luminance(path) == pytest.approx(1.0, abs=0.01)


def test_extract_linear_luminance_black_image(tmp_path):
    path = tmp_path / "black.jpg"
    Image.new("RGB", (100, 100), color=(0, 0, 0)).save(path, "JPEG", quality=95)
    assert extract_linear_luminance(path) == pytest.approx(0.0, abs=0.01)


def test_extract_linear_luminance_samples_only_centre_region(tmp_path):
    # Edges are pure black, a small centre block is pure white; SAMPLE_FRACTION=0.15 means
    # the sampled window (30% of each dimension) sits entirely inside the white block.
    path = tmp_path / "centre.jpg"
    img = Image.new("RGB", (200, 200), color=(0, 0, 0))
    for y in range(50, 150):
        for x in range(50, 150):
            img.putpixel((x, y), (255, 255, 255))
    img.save(path, "JPEG", quality=100)
    assert extract_linear_luminance(path) == pytest.approx(1.0, abs=0.02)
