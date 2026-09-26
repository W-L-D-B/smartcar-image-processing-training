#!/usr/bin/env python3
"""Generate and process fully synthetic camera-car lane frames."""
from __future__ import annotations

import argparse
import csv
import math
import time
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 160, 120
SEED = 20260927


def make_frame(case: str) -> tuple[np.ndarray, np.ndarray]:
    image = np.full((HEIGHT, WIDTH), 218, dtype=np.uint8)
    centers = np.zeros(HEIGHT, dtype=np.float32)
    for y in range(HEIGHT):
        t = y / max(1, HEIGHT - 1)
        if case == "sharp_curve":
            center = 78 + 48 * t * t
        else:
            center = 79 + 11 * (t - 0.4)
        half_width = 20 + 18 * t
        centers[y] = center
        left = max(4, int(round(center - half_width)))
        right = min(WIDTH - 5, int(round(center + half_width)))
        image[y, left + 2:right - 2] = 145
        image[y, max(0, left - 2):left + 2] = 38
        image[y, right - 1:min(WIDTH, right + 3)] = 42

    if case == "shadow":
        image[28:94, :84] = (image[28:94, :84].astype(np.float32) * 0.30).astype(np.uint8)
    elif case == "glare":
        image[27:94, 66:103] = 250
    elif case == "noise":
        rng = np.random.default_rng(SEED)
        count = 230
        ys = rng.integers(0, HEIGHT, count)
        xs = rng.integers(0, WIDTH, count)
        vals = rng.choice([0, 255], count)
        image[ys, xs] = vals
    elif case == "missing_left":
        for y in range(58, HEIGHT):
            t = y / (HEIGHT - 1)
            left = max(4, int(round(79 + 11 * (t - 0.4) - (20 + 18 * t))))
            image[y, max(0, left - 3):left + 4] = 180
    elif case == "low_resolution":
        small = cv2.resize(image, (40, 30), interpolation=cv2.INTER_AREA)
        return small, cv2.resize(centers, (40, 1), interpolation=cv2.INTER_LINEAR).reshape(-1)
    return image, centers


def row_scan(binary: np.ndarray, centers: np.ndarray) -> tuple[list[tuple[int, int, int]], int, float]:
    h, w = binary.shape
    cx = w // 2
    points: list[tuple[int, int, int]] = []
    errors = []
    for y in range(4, h - 4):
        left = next((x for x in range(cx - 1, -1, -1) if binary[y, x] > 0), None)
        right = next((x for x in range(cx + 1, w) if binary[y, x] > 0), None)
        if left is not None and right is not None and right - left > 3:
            mid = (left + right) / 2.0
            points.append((y, left, right))
            truth_y = min(len(centers) - 1, y)
            scale = w / WIDTH
            errors.append(abs(mid - float(centers[truth_y]) * scale))
    coverage = len(points) / max(1, h - 8)
    mean_error = float(np.mean(errors)) if errors else math.nan
    return points, len(points), mean_error


def draw_rows(gray: np.ndarray, points: list[tuple[int, int, int]]) -> np.ndarray:
    out = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    for y, left, right in points:
        mid = (left + right) // 2
        cv2.circle(out, (left, y), 1, (255, 80, 30), -1)
        cv2.circle(out, (right, y), 1, (30, 120, 255), -1)
        cv2.circle(out, (mid, y), 1, (20, 210, 120), -1)
    return out


def write_image(path: Path, frame: np.ndarray) -> None:
    """Write an image on Windows even when the output path contains CJK text."""
    ok, encoded = cv2.imencode(path.suffix or ".png", frame)
    if not ok:
        raise OSError(f"image encoder failed for {path.name}")
    encoded.tofile(str(path))


def save_contact_sheet(entries: list[tuple[str, np.ndarray]], out_path: Path) -> None:
    font_path = Path("C:/Windows/Fonts/msyh.ttc")
    try:
        font = ImageFont.truetype(str(font_path), 20)
    except OSError:
        font = ImageFont.load_default()
    thumb_w, thumb_h, caption_h = 360, 270, 42
    cols, rows = 3, math.ceil(len(entries) / 3)
    sheet = Image.new("RGB", (cols * thumb_w, rows * (thumb_h + caption_h)), "#F5F3EC")
    draw = ImageDraw.Draw(sheet)
    for i, (caption, frame) in enumerate(entries):
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) if frame.ndim == 3 else cv2.cvtColor(frame, cv2.COLOR_GRAY2RGB)
        tile = Image.fromarray(rgb).resize((thumb_w - 16, thumb_h - 12), Image.Resampling.NEAREST)
        x = (i % cols) * thumb_w + 8
        y = (i // cols) * (thumb_h + caption_h) + 6
        sheet.paste(tile, (x, y))
        draw.text((x, y + thumb_h - 2), caption, fill="#183235", font=font)
    sheet.save(out_path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("generated"))
    parser.add_argument("--threshold", type=int, default=65)
    args = parser.parse_args()
    if not 0 <= args.threshold <= 255:
        parser.error("--threshold must be between 0 and 255")
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    rows_out = []
    cases = ["normal", "shadow", "glare", "noise", "missing_left", "sharp_curve", "low_resolution"]

    for case in cases:
        gray, centers = make_frame(case)
        h, w = gray.shape
        start = time.perf_counter_ns()
        _, fixed = cv2.threshold(gray, args.threshold, 255, cv2.THRESH_BINARY_INV)
        fixed_ms = (time.perf_counter_ns() - start) / 1e6

        start = time.perf_counter_ns()
        otsu_t, otsu = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)
        otsu_ms = (time.perf_counter_ns() - start) / 1e6

        block = min(15, min(h, w) if min(h, w) % 2 else min(h, w) - 1)
        block = max(3, block if block % 2 else block - 1)
        start = time.perf_counter_ns()
        adaptive = cv2.adaptiveThreshold(gray, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                         cv2.THRESH_BINARY_INV, block, 4)
        adaptive_ms = (time.perf_counter_ns() - start) / 1e6

        if case == "normal":
            blurred = cv2.GaussianBlur(gray, (5, 5), 0)
            _, blur_otsu = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)
            median = cv2.medianBlur(gray, 3)
            kernel = np.ones((3, 3), np.uint8)
            close = cv2.morphologyEx(fixed, cv2.MORPH_CLOSE, kernel)
            canny = cv2.Canny(gray, 50, 120)
            points, valid, error = row_scan(fixed, centers)
            row_img = draw_rows(gray, points)
            stages = [
                ("01 原始灰度", gray), ("02 固定阈值", fixed), ("03 Otsu", otsu),
                ("04 自适应阈值", adaptive), ("05 高斯 + Otsu", blur_otsu),
                ("06 中值滤波", median), ("07 形态学闭运算", close),
                ("08 Canny 边缘", canny), ("09 逐行扫描/中点", row_img),
            ]
            for filename, frame in [
                ("normal_gray.png", gray), ("normal_binary_fixed.png", fixed),
                ("normal_binary_otsu.png", otsu), ("normal_binary_adaptive.png", adaptive),
                ("normal_blur_otsu.png", blur_otsu), ("normal_median.png", median),
                ("normal_morph_close.png", close), ("normal_edges_canny.png", canny),
                ("normal_rows.png", row_img),
            ]:
                write_image(out / filename, frame)
            save_contact_sheet(stages, out / "normal_pipeline.png")
        else:
            points, valid, error = row_scan(fixed, centers)
            marked = draw_rows(gray, points)
            write_image(out / f"scenario_{case}_gray.png", gray)
            write_image(out / f"scenario_{case}_fixed.png", fixed)
            write_image(out / f"scenario_{case}_rows.png", marked)

        rows_out.append({
            "scenario": case, "width": w, "height": h, "format": "GRAY8",
            "threshold_fixed": args.threshold, "otsu_threshold": round(float(otsu_t), 2),
            "fixed_threshold_ms_pc": f"{fixed_ms:.6f}", "otsu_ms_pc": f"{otsu_ms:.6f}",
            "adaptive_ms_pc": f"{adaptive_ms:.6f}", "valid_scan_rows": valid,
            "scan_row_ratio": f"{valid / max(1, h - 8):.4f}",
            "mean_center_abs_error_px_synthetic": "" if math.isnan(error) else f"{error:.3f}",
            "gray8_bytes": w * h, "binary_1bit_packed_bytes": (w * h + 7) // 8,
        })

    with (out / "metrics.csv").open("w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows_out[0].keys()))
        writer.writeheader()
        writer.writerows(rows_out)
    print(f"OpenCV {cv2.__version__}; fixed threshold={args.threshold}; normal frame={WIDTH}x{HEIGHT} GRAY8")
    print(f"160x120 GRAY8={WIDTH * HEIGHT:,} B; packed 1-bit={(WIDTH * HEIGHT + 7) // 8:,} B")
    print(f"Generated {len(rows_out)} synthetic scenarios and outputs at: {out.resolve()}")
    print("Processing times are this PC's single-run observations, not MCU performance claims.")


if __name__ == "__main__":
    main()
