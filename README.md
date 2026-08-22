# Parallel Image Processor
ADD IMAGES SOMEWHERE, maybe?
A worker-pool-based system for parallel image transformations in C, using `fork()`/`exec()`, Unix pipes, signals, and a REPL-driven frontend.

<p align="center">
  <img src="docs/interface_screenshot.png" alt="Interface screenshot" width="700">
</p>

## Architecture

Work is distributed across three types of processes:

* **Frontend** — provides the interactive REPL, parses user commands, and sends requests to the dispatcher through pipes and `SIGUSR1`. It remains blocked waiting for user input when idle.

* **Dispatcher** — divides the image into jobs, maintains the job stack, dynamically creates and removes workers, assigns jobs, and executes commands received from the frontend. It also detects failed workers, requeues unfinished jobs, and spawns replacements.

* **Worker** — receives job IDs from the dispatcher, uses `pread()` to read the assigned region of the input image, processes the pixels, and uses `pwrite()` to write the result to the corresponding region of the output image.

<p align="center">
  <img src="docs/architecture.svg" alt="Architecture" width="1200">
</p>

## Features

* REPL interface that can be paired with rlwrap for a seamless user experience
* Number of workers can be scaled at runtime
* Live progress + report of workers' status
* Automatic detection and respawn of crashed workers
* various builds? 
* currently compatible with PPM P6 files