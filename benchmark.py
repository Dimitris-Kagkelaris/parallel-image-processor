#!/usr/bin/env python3
"""
Benchmark the parallel PPM -> PGM converter.

Expected project layout:
    benchmark.py
    bin/
        frontend
        dispatcher
        worker
    images/
        test6000.ppm
        test8000.ppm
        test12000.ppm
        test16000.ppm
        test18000.ppm

For each image:
  1. Run one untimed warm-up.
  2. Benchmark 1, 2, 4, and 8 workers.
  3. Repeat each worker count 3 times.
  4. Save every timing to benchmark_raw.csv.
  5. Save aggregated statistics to benchmark_summary.csv.

No third-party Python packages are required.
"""

from __future__ import annotations

import csv
import os
import platform
import random
import statistics
import subprocess
import sys
import time
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent
FRONTEND = PROJECT_DIR / "bin" / "frontend"

IMAGE_DIR = PROJECT_DIR / "images"

IMAGES = [
    "test6000.ppm",
    "test8000.ppm",
    "test12000.ppm",
    "test16000.ppm",
    "test18000.ppm",
]

WORKERS = [1, 2, 4, 8]
REPETITIONS = 3

# Use 4 workers for the warm-up on the current 4-vCPU VM.
WARMUP_WORKERS = 4

# Reuse one output file so the benchmark does not leave many huge PGM files.
OUTPUT_FILE = PROJECT_DIR / "benchmark_output.pgm"

RAW_CSV = PROJECT_DIR / "benchmark_raw.csv"
SUMMARY_CSV = PROJECT_DIR / "benchmark_summary.csv"

# Prevent a broken run from hanging forever.
TIMEOUT_SECONDS = 15 * 60

# Fixed seed makes worker-order randomization reproducible.
RANDOM_SEED = 20260817


def read_ppm_dimensions(path: Path) -> tuple[int, int]:
    """Read width/height from a binary P6 PPM header, skipping comments."""
    with path.open("rb") as f:
        def next_token() -> bytes:
            token = bytearray()

            while True:
                ch = f.read(1)
                if not ch:
                    raise ValueError(f"Unexpected EOF in PPM header: {path}")

                if ch == b"#":
                    f.readline()
                    continue

                if not ch.isspace():
                    token.extend(ch)
                    break

            while True:
                ch = f.read(1)
                if not ch or ch.isspace():
                    break
                if ch == b"#":
                    f.readline()
                    break
                token.extend(ch)

            return bytes(token)

        magic = next_token()
        if magic != b"P6":
            raise ValueError(f"{path.name} is not a P6 PPM file")

        width = int(next_token())
        height = int(next_token())
        maxval = int(next_token())

        if width <= 0 or height <= 0:
            raise ValueError(f"Invalid dimensions in {path.name}: {width}x{height}")
        if maxval != 255:
            raise ValueError(
                f"{path.name}: expected maxval 255, got {maxval}"
            )

        return width, height


def validate_inputs() -> dict[str, tuple[int, int]]:
    if not FRONTEND.exists():
        raise FileNotFoundError(
            f"Cannot find {FRONTEND}. Run make from the project root first."
        )

    if not os.access(FRONTEND, os.X_OK):
        raise PermissionError(f"{FRONTEND} is not executable")

    dimensions = {}

    for name in IMAGES:
        path = IMAGE_DIR / name
        if not path.exists():
            raise FileNotFoundError(f"Cannot find input image: {path}")
        dimensions[name] = read_ppm_dimensions(path)

    return dimensions


def run_conversion(image: Path, workers: int) -> float:
    """
    Run the complete frontend -> dispatcher -> workers pipeline.

    The frontend accepts:
        1 <workers>
    and then stdin reaches EOF. Its current EOF behavior waits for the
    dispatcher to finish, so subprocess.run() returns when conversion ends.
    """
    command_input = f"1 {workers}\n"

    start_ns = time.perf_counter_ns()

    try:
        result = subprocess.run(
            [str(FRONTEND), str(image), str(OUTPUT_FILE)],
            input=command_input,
            text=True,
            cwd=PROJECT_DIR,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(
            f"Timed out after {TIMEOUT_SECONDS}s: "
            f"{image.name}, {workers} worker(s)"
        ) from exc

    end_ns = time.perf_counter_ns()

    if result.returncode != 0:
        raise RuntimeError(
            f"frontend exited with status {result.returncode}: "
            f"{image.name}, {workers} worker(s). "
            "Run that configuration manually to see its error output."
        )

    return (end_ns - start_ns) / 1_000_000_000.0


def write_raw_csv(rows: list[dict]) -> None:
    fields = [
        "image",
        "width",
        "height",
        "pixels",
        "workers",
        "run",
        "time_seconds",
        "cpu_count",
        "platform",
    ]

    with RAW_CSV.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def make_summary(rows: list[dict]) -> list[dict]:
    grouped: dict[tuple[str, int], list[float]] = {}

    image_metadata = {}
    for row in rows:
        key = (row["image"], row["workers"])
        grouped.setdefault(key, []).append(row["time_seconds"])
        image_metadata[row["image"]] = (
            row["width"],
            row["height"],
            row["pixels"],
        )

    means = {
        key: statistics.mean(values)
        for key, values in grouped.items()
    }

    summary = []

    for image in IMAGES:
        width, height, pixels = image_metadata[image]
        baseline = means[(image, 1)]

        for workers in WORKERS:
            values = grouped[(image, workers)]
            mean_time = statistics.mean(values)
            median_time = statistics.median(values)
            stdev_time = statistics.stdev(values) if len(values) > 1 else 0.0

            speedup = baseline / mean_time
            efficiency = speedup / workers
            throughput_mpix_s = (pixels / 1_000_000.0) / mean_time

            summary.append(
                {
                    "image": image,
                    "width": width,
                    "height": height,
                    "pixels": pixels,
                    "workers": workers,
                    "mean_seconds": mean_time,
                    "median_seconds": median_time,
                    "stdev_seconds": stdev_time,
                    "speedup": speedup,
                    "efficiency": efficiency,
                    "throughput_mpix_s": throughput_mpix_s,
                }
            )

    return summary


def write_summary_csv(rows: list[dict]) -> None:
    fields = [
        "image",
        "width",
        "height",
        "pixels",
        "workers",
        "mean_seconds",
        "median_seconds",
        "stdev_seconds",
        "speedup",
        "efficiency",
        "throughput_mpix_s",
    ]

    with SUMMARY_CSV.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()

        for row in rows:
            formatted = dict(row)
            for field in (
                "mean_seconds",
                "median_seconds",
                "stdev_seconds",
                "speedup",
                "efficiency",
                "throughput_mpix_s",
            ):
                formatted[field] = f"{formatted[field]:.6f}"

            writer.writerow(formatted)


def print_summary(rows: list[dict]) -> None:
    print()
    print("Summary")
    print("=" * 88)
    print(
        f"{'image':<18}"
        f"{'workers':>8}"
        f"{'mean(s)':>12}"
        f"{'stdev':>12}"
        f"{'speedup':>12}"
        f"{'efficiency':>12}"
        f"{'MPix/s':>12}"
    )

    for row in rows:
        print(
            f"{row['image']:<18}"
            f"{row['workers']:>8}"
            f"{row['mean_seconds']:>12.3f}"
            f"{row['stdev_seconds']:>12.3f}"
            f"{row['speedup']:>12.3f}"
            f"{row['efficiency']:>12.3f}"
            f"{row['throughput_mpix_s']:>12.2f}"
        )


def main() -> int:
    dimensions = validate_inputs()

    print("Parallel image-processing benchmark")
    print("=" * 40)
    print(f"Project:       {PROJECT_DIR}")
    print(f"Logical CPUs:  {os.cpu_count()}")
    print(f"Platform:      {platform.platform()}")
    print(f"Workers:       {WORKERS}")
    print(f"Repetitions:   {REPETITIONS}")
    print(f"Warm-up:       {WARMUP_WORKERS} workers/image")
    print()
    print(
        f"Timed runs: {len(IMAGES) * len(WORKERS) * REPETITIONS} "
        f"(+ {len(IMAGES)} warm-ups)"
    )
    print()

    raw_rows: list[dict] = []
    rng = random.Random(RANDOM_SEED)

    try:
        for image_index, image_name in enumerate(IMAGES, start=1):
            image = IMAGE_DIR / image_name
            width, height = dimensions[image_name]
            pixels = width * height

            print(
                f"[{image_index}/{len(IMAGES)}] {image_name} "
                f"({width}x{height}, {pixels / 1_000_000:.1f} MP)"
            )

            print(
                f"  warm-up with {WARMUP_WORKERS} workers...",
                end="",
                flush=True,
            )
            warmup_time = run_conversion(image, WARMUP_WORKERS)
            print(f" {warmup_time:.3f}s")

            # Run in rounds and shuffle worker order inside each round.
            # This reduces bias from thermal drift/background activity while
            # preserving exactly REPETITIONS samples for each worker count.
            for run_number in range(1, REPETITIONS + 1):
                worker_order = WORKERS.copy()
                rng.shuffle(worker_order)

                print(f"  round {run_number}: worker order {worker_order}")

                for workers in worker_order:
                    elapsed = run_conversion(image, workers)

                    print(
                        f"    {workers:>2} worker(s): {elapsed:.3f}s"
                    )

                    raw_rows.append(
                        {
                            "image": image_name,
                            "width": width,
                            "height": height,
                            "pixels": pixels,
                            "workers": workers,
                            "run": run_number,
                            "time_seconds": elapsed,
                            "cpu_count": os.cpu_count(),
                            "platform": platform.platform(),
                        }
                    )

                    # Save after every successful run so results survive if a
                    # later configuration fails or the benchmark is interrupted.
                    write_raw_csv(raw_rows)

            print()

    except KeyboardInterrupt:
        print("\nBenchmark interrupted by user.", file=sys.stderr)
        if raw_rows:
            write_raw_csv(raw_rows)
            print(f"Partial results saved to {RAW_CSV}", file=sys.stderr)
        return 130

    finally:
        try:
            OUTPUT_FILE.unlink()
        except FileNotFoundError:
            pass

    summary_rows = make_summary(raw_rows)
    write_summary_csv(summary_rows)
    print_summary(summary_rows)

    print()
    print(f"Raw timings: {RAW_CSV}")
    print(f"Summary:     {SUMMARY_CSV}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
