# Parallel Image Processor

A worker-pool-based system for parallel image transformations in C, using `fork()`/`execv()`, Unix pipes and signals for interprocess coordination.

<p align="center">
  <img src="docs/interface_screenshot.png"
       alt="Parallel Image Processor REPL"
       width="80%">
</p>

## Architecture

Work is distributed across three types of processes:

* **Frontend** — provides the interactive REPL, parses user commands, and sends requests to and receives responses from the dispatcher. It remains blocked waiting for user input when idle.

* **Dispatcher** — divides the image into jobs and maintains them in a job stack. It dynamically creates and removes worker processes, assigns jobs to available workers, and carries out commands received from the frontend. It also detects failed workers, requeues their unfinished jobs, and spawns replacements.

* **Worker** — receives job IDs from the dispatcher, uses `pread()` to read the assigned region of the input image, processes the pixels, and uses `pwrite()` to write the result to the corresponding region of the output image.

<p align="center">
  <img src="docs/architecture.svg" alt="architecture" width="120%">
</p>

## Features

* **Interactive REPL** — controls the application at runtime; can be paired with `rlwrap` for command history and improved line editing.

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


## Design Notes

* **Job Partitioning and Positional I/O** — After the input file header is parsed, the image data is divided into fixed-size, indexed packets (jobs), which are stored in a stack of unfinished work. For each job, the worker calculates the corresponding byte offset in the input file and reads from that position. A matching output offset is then calculated for writing the processed data. Because all workers share the same input and output file descriptors, they use `pread()` and `pwrite()` to perform positional I/O without relying on a shared file offset.

* **Worker–Dispatcher IPC** — Communication between the dispatcher and each worker uses two pipes: a blocking pipe from the dispatcher to the worker, and a non-blocking acknowledgement pipe from the worker back to the dispatcher. Workers can block while waiting for a new job ID because they have no work to perform until one arrives. The dispatcher, however, checks each worker's acknowledgement pipe sequentially, so those pipes are non-blocking to prevent it from waiting on a specific worker while others may have already completed their jobs.

* **Dispatcher Scheduling and Worker Recovery** — During each pass over the worker pool, the dispatcher checks every worker's acknowledgement pipe:

  * If a worker is still busy and no acknowledgement is available, the dispatcher moves on to the next worker.
  * If an acknowledgement is received, the completed job is recorded and, if unfinished jobs remain, the worker is assigned a new job ID from the stack.
  * If EOF is received, the worker is considered dead. If it crashed while processing a job, that job ID is pushed back onto the unfinished-job stack, and a replacement worker is spawned.


* **Frontend–Dispatcher Command IPC** — The frontend communicates with the dispatcher through two pipes and `SIGUSR1`. When the frontend receives a command, it parses and validates it, then sends the corresponding command ID and any required arguments through the command pipe. It then sends `SIGUSR1` to notify the dispatcher that a command is available and waits for the appropriate response. The dispatcher handles the signal by setting a flag, processes the pending command before continuing its worker loop, and sends any requested response back to the frontend through the response pipe.

* **Process Creation and Parent-Death Handling** — Processes are created using `fork()` followed by `execv()`: the frontend creates the dispatcher, and the dispatcher creates worker processes. Each child configures `prctl(PR_SET_PDEATHSIG, SIGTERM)` so that it receives `SIGTERM` if its parent process dies. This creates a parent-child lifetime chain from the frontend to the dispatcher and from the dispatcher to the workers.

* **Application Shutdown** — If the user exits through the frontend, terminating the frontend causes the dispatcher to receive `SIGTERM`, which in turn causes its workers to terminate. When all jobs are completed normally, the dispatcher exits successfully; the frontend receives `SIGCHLD`, checks the dispatcher's exit status, and then exits as well.

## Limitations

1. Job granularity
2. pread + pwrite have syscall overhead, better switch to mmap
3. poll the pipes with the dispatcher instead of spinning
4. Right now only Grayscale, soon there will be more
5. Only PPM format, no compressed formats (if we switch to mmap it might support them too, who knows?)
6. Limited to MAX_WORKERS = 500, because of fd soft limit to 1024 but nevertheless no point having many workers than cores in the computer, as proven in earlier benchmarking

## Future work

what?

# Author/Contact

dimlaris the king


add an MIT license lol...