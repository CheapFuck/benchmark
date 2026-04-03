# System Benchmark

A simple CPU and memory benchmark written in C. Run it, get a score, and optionally submit to the [online leaderboard](https://cheapfuck.github.io/benchmark/leaderboard.html).

## What it tests

| Test | Weight | Description |
|------|--------|-------------|
| Integer | 20% | Prime sieve up to 20M (single-thread) |
| Float ST | 25% | Heavy math - sin, cos, sqrt (single-thread) |
| Float MT | 30% | Same math workload, multi-threaded (OpenMP) |
| Memory | 25% | Sequential read throughput (128 MB × 5 passes) |

A score of **1000** is the reference baseline. Higher is better.

## Quick start

### Linux / WSL / macOS

```bash
git clone https://github.com/CheapFuck/benchmark.git
cd benchmark
make
./benchmark
```

### Requirements

- `gcc` with OpenMP support (`sudo apt install gcc` on Ubuntu/Debian)
- `curl` (for submitting results - comes pre-installed on most systems)
- `make`

### Without OpenMP

If you don't have OpenMP, you can compile without it:

```bash
gcc benchmark.c -O2 -lm -o benchmark
./benchmark
```

The multi-threaded test will just run single-threaded.

### Silent mode

Run with `-s` or `--silent` to suppress all output and auto-submit results. Useful for startup scripts:

```bash
./benchmark -s
```

## Leaderboard

After the benchmark finishes you'll be asked:

```
Submit results to online leaderboard? [y/N]
```

Press `y` and your CPU name + scores get posted to the public leaderboard:

**https://cheapfuck.github.io/benchmark/leaderboard.html**
