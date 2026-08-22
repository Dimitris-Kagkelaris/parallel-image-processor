# Parallel Image Processor

A worker-pool-based system for parallel image transformations in C, using `fork()`/`exec()`, Unix pipes, signals, and a REPL-driven frontend.

<p align="center">
  <img src="docs/interface_screenshot.png"
       alt="Parallel Image Processor REPL"
       width="700">
</p>

## Architecture

Work is distributed across three types of processes:

* **Frontend** — provides the interactive REPL, parses user commands, and sends requests to the dispatcher through pipes and `SIGUSR1`. It remains blocked waiting for user input when idle.

* **Dispatcher** — divides the image into jobs, maintains the job stack, dynamically creates and removes workers, assigns jobs, and executes commands received from the frontend. It also detects failed workers, requeues unfinished jobs, and spawns replacements.

* **Worker** — receives job IDs from the dispatcher, uses `pread()` to read the assigned region of the input image, processes the pixels, and uses `pwrite()` to write the result to the corresponding region of the output image.

<p align="center">
  <img src="docs/architecture.svg" alt="architecture" width="120%">
</p>

## Features

* **Interactive REPL** for controlling the application at runtime; can be paired with `rlwrap` for command history and improved line editing.

* **Dynamic worker scaling** — add or remove worker processes while image processing is in progress.

* **Live progress and worker status** — inspect processing progress and view each worker's PID, current job, and completed job count.

* **Automatic worker recovery** — detects crashed workers, returns unfinished jobs to the pending-job stack, and spawns replacement workers.

* **Extensible parallel image processing** — currently demonstrates RGB-to-grayscale conversion, with a modular architecture designed to support additional image transformations.

* **P6 PPM input support** — currently supports 8-bit P6 PPM images and produces grayscale PGM output.

<table>
  <tr>
    <td align="center">
      <img src="docs/input_preview.png" alt="Input PPM image" width="100%">
    </td>
    <td align="center">
      <img src="docs/output_preview.png" alt="Output PGM image" width="100%">
    </td>
  </tr>
  <tr>
    <td align="center"><strong>Input — P6 PPM</strong></td>
    <td align="center"><strong>Output — P5 PGM</strong></td>
  </tr>
</table>

## Builds

All builds use GCC with the `-Wall` and `-Wextra` warning flags.

* **`make release`** — builds optimized binaries using GCC's `-O2` optimization level.

* **`make debug`** — builds with `-O0` and `-g`, and enables additional debug logging to `stderr`.

* **`make sleep-debug`** — same as `make debug`, but adds a `sleep(1)` after each completed worker job, making it easier to interact with the frontend while processing is underway.

> **Important:** Run `make clean` before switching between build configurations.

## Run

Place the input image in the `./images/` directory before running the application.

* **`make run IN=image.ppm OUT=processed.pgm`** — runs the application using the specified input and output filenames. The output file is created if it does not already exist. `IN` defaults to `input.ppm` and `OUT` defaults to `output.pgm`. Diagnostic output from `stderr` is redirected to `logs.txt`.

* **`make run-rlwrap IN=image.ppm OUT=processed.pgm`** — runs the application through `rlwrap`, providing command history and improved line editing for the REPL. `rlwrap` must be installed separately.

## How to use

Once the application is running, the frontend provides an interactive command interface for controlling the worker pool and monitoring processing.

| Command          | Description                                           |
| ---------------- | ----------------------------------------------------- |
| `add <count>`    | Add worker processes, up to the maximum worker limit. |
| `remove <count>` | Remove worker processes, stopping at zero.            |
| `status`         | Show frontend, dispatcher, and worker information.    |
| `progress`       | Show the percentage of completed jobs.                |
| `help`           | Display the available commands.                       |
| `clear`          | Clear the terminal screen.                            |
| `exit`           | Exit the application.                                 |

