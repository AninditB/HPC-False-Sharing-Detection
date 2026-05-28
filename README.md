# HPC False Sharing Detection

**False Sharing Detection in Multithreaded Programs using Machine Learning**

This project generates labeled hardware-performance-counter data from OpenMP C programs that deliberately exhibit different memory access pathologies, then trains a machine learning classifier to distinguish those pathologies from the raw `perf` output.

---

## Overview

Modern multicore processors cache data in 64-byte cache lines. **False sharing** occurs when two threads write to different variables that happen to occupy the same cache line, causing unnecessary cache invalidations and a measurable performance penalty. This project:

1. Implements six OpenMP benchmark programs that operate in three modes — `good` (cache-friendly), `bad-fs` (false sharing), and `bad-ma` (inefficient/random memory access).
2. Collects 19 hardware performance counters via Linux `perf stat` across a sweep of thread counts and data sizes.
3. Trains a Decision Tree classifier (with Lasso-based feature selection and SMOTE oversampling) to identify the access mode from the raw counter values.

---

## Repository Structure

```
.
├── array_sum_false_sharing_sim_14.c    # Array sum – simple false sharing simulation
├── array_sum_memory_access_28.c        # Array sum – padded structs to isolate false sharing
├── array_sum_performance_variation_10.c # Sequential array sum – linear / random / strided
├── matrix_compare_memory_modes_31.c    # Matrix element comparison across memory modes
├── matrix_init_access_modes_23.c       # Matrix initialisation – row vs column-major
├── matrix_init_access_variation_29.c   # Array sum with random-access bad-ma variant
├── build.sh                            # Compiles all six programs with GCC + OpenMP
├── perf_data.sh                        # Sweeps configurations, runs perf stat, writes CSV
└── regression.py                       # ML pipeline: feature selection + Decision Tree
```

---

## Benchmark Programs

Each program accepts `<mode> <size> <threads>` on the command line.

| Source file | Executable | Modes | Operation |
|---|---|---|---|
| `array_sum_false_sharing_sim_14.c` | `vec_14` | `good`, `bad-fs`, `bad-ma` | Parallel array reduction; `bad-fs` uses unpadded per-thread accumulators on a shared array |
| `array_sum_memory_access_28.c` | `sc_28` | `good`, `bad-fs`, `bad-ma` | Same reduction; `good` uses 64-byte padded structs; `bad-ma` uses strided (co-prime) index traversal |
| `array_sum_performance_variation_10.c` | `seq_10` | `good`, `bad` | Single-threaded; `good` = linear scan + modify; `bad` = random + strided access |
| `matrix_compare_memory_modes_31.c` | `mc_31` | `good`, `bad-fs`, `bad-ma` | Counts differing elements between two N×N matrices; `bad-ma` uses shuffled index access |
| `matrix_init_access_modes_23.c` | `vec_23` | `good`, `bad-fs`, `bad-ma` | Initialises an N×N matrix; `good` = row-major; `bad-ma` = column-major (cache-unfriendly) |
| `matrix_init_access_variation_29.c` | `sc_29` | `good`, `bad-fs`, `bad-ma` | Array sum; `bad-ma` uses randomly shuffled indices |

### Memory access modes

| Mode | Description |
|---|---|
| `good` | Linear, cache-friendly access; per-thread accumulators padded to a full cache line |
| `bad-fs` | Per-thread accumulators packed without padding — multiple accumulators share a cache line, causing false sharing |
| `bad-ma` | Strided or randomised index access that defeats hardware prefetching |

---

## Prerequisites

- GCC with OpenMP support (`-fopenmp`)
- Linux `perf` tool (`linux-tools-common` / `linux-perf`)
- Python 3 with: `pandas`, `numpy`, `scikit-learn`, `imbalanced-learn`, `matplotlib`, `seaborn`, `joblib`

---

## Usage

### 1. Build

```bash
bash build.sh
```

This compiles all six source files and produces the executables `vec_14`, `sc_28`, `seq_10`, `mc_31`, `vec_23`, `sc_29` in the current directory.

### 2. Collect performance data

```bash
bash perf_data.sh
```

The script:
- Sweeps each program over its supported modes, thread counts (1–8), and five data sizes.
- Runs each configuration **3 times** for statistical stability.
- Calls `perf stat` with 15 hardware events per run.
- Writes results to `perf_data.csv` (appending if it already exists; a timestamped backup is created automatically).
- Logs all output to `perf_run.log`; errors go to `error.log`.

**CSV columns**

`Program, Mode, Threads, Data_Size, Run,` followed by 18 metric columns:

| Counter group | Metrics |
|---|---|
| Cache | `cache_references`, `cache_misses` |
| L1 data cache | `L1_dcache_loads`, `L1_dcache_load_misses`, `L1_dcache_prefetches` |
| TLB | `dTLB_loads`, `dTLB_load_misses` |
| Branch | `branch_instructions`, `branch_misses` |
| Scheduler | `context_switches`, `cpu_migrations` |
| Pipeline | `stalled_cycles_backend`, `stalled_cycles_frontend` |
| Core | `cpu_cycles`, `instructions` |
| Time | `elapsed_time`, `user_time`, `sys_time` |

### 3. Train the classifier

```bash
python regression.py
```

The pipeline:

1. Loads `perf_data.csv` and aggregates the 3 runs per configuration into mean + std features.
2. Encodes the `Mode` column as the classification target (`good` / `bad-fs` / `bad-ma`).
3. Splits data 80/20, scales with `StandardScaler`, and balances training classes with SMOTE.
4. Selects features via Lasso Logistic Regression (L1 penalty) and Recursive Feature Elimination on a Decision Tree.
5. Retrains a Decision Tree on the union of selected features.
6. Prints a classification report, confusion matrix, and 5-fold cross-validation accuracy.
7. Saves four artefacts:
   - `decision_tree_classifier.pkl`
   - `scaler.pkl`
   - `label_encoder.pkl`
   - `feature_importance.pkl`

---

## Pipeline Flowchart

See [FLOWCHART.md](FLOWCHART.md) for the full end-to-end Mermaid diagram.

---

## Author

Anindit Bordoloi
