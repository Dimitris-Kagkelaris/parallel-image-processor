# Parallel Image Processor

A worker pool for parallel image transformations in C, using fork/exec, pipes, signals — with a REPL-driven frontend.
![Screenshot](docs/interface_screenshot.png)

## Architecture
Work is distributed to 3 distinct processes:

1: frontend handles commands from the user (is blocked at input)

2: dispatcher divides image into packets (jobs), dynamically spawns and kills workers
and executes commands received from the frontend

3: worker receives jobs from the dispatcher and for each job it preads from a given offset in the input file and pwrites to another offset in the output file

![Architecture](docs/architecture.svg)
