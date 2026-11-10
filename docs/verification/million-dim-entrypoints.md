# Task 2.1 million-dimensional solve: run entry points and capability boundaries

This page records the *actual, runnable* entry points behind the
million-dimensional (n = 2^20) linear-solver evidence and separates five
capabilities that are easy to conflate: input export, dimension padding,
trajectory-level MPI, distributed state slicing, and GPU compilation/running.
Anything not listed here as an entry point is a boundary, not a claim.

## Entry-point matrix

| Capability | Entry point | Backend | Status |
| --- | --- | --- | --- |
| 2^20 tridiagonal solve (production) | `Experiments/QDA/QDA_Poiseuille/PoiseuilleQDABench.cu` (`PoiseuilleQDABench_cuda`) | CUDA `CuSparseState` | run to completion on A800 (see `v1d-qda-linsolver-a800.md`) |
| Same contract on CPU | `Experiments/QDA/QDA_Poiseuille/PoiseuilleQDABench.cpp` | CPU sparse state | buildable; used for the equivalence chain at n<=2^16 |
| CKS (LCU) solver, QRAM vs QRAM-free oracle | `Experiments/CKS/CKSTridiagonalProbe.cpp` (`--path qram` / `--path direct`) | CPU sparse state | both paths must agree (see `v1e-cks-direct-oracle-linsolver.md`) |
| CKS Poiseuille GPU bench | `Experiments/CKS/CKSPoiseuilleBench.cu` | CUDA | compiles with the CUDA backend |
| RIR + sparse QRAM execution (standard program/memory channel) | `pysparq rir --program <json> [--memory <json>]` CLI, GUI `/api/rir` panel, `pysparq.run_rir` API | CPU sparse state (wheel) | released; covered by regression |
| Noisy-ensemble trajectory MPI | `qram_simulator.mpi` (QRAM-Simulator submodule bindings), `mpirun -np N python ...` | CPU, single node | provided and verified by the submodule (four-rank CI regression) |

## GPU compilation and running

The CUDA backend is **off by default**; wheels and the standard CI are
CPU-only. Building it requires a local CUDA toolkit (CI pins CUDA 12.8.1 and
13.3.0 via `extern/qram-simulator/tools/bootstrap_cuda.py --release <ver>`;
the local record also covers CUDA 13.4 with sm80 + sm120 device libraries on
an RTX PRO 5000 Blackwell — see `wide-registers.md`).

```bash
# 1) toolchain (or use a system nvcc)
python extern/qram-simulator/tools/bootstrap_cuda.py --release 12.8.1 --output build-cuda/toolkit
# 2) configure the CUDA consumer build (CI form; experiments add -DSPARQ_BUILD_EXPERIMENTS=ON)
cmake -S . -B build-cuda/native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSPARQ_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=80 \
  -DCMAKE_CUDA_COMPILER=$PWD/build-cuda/toolkit/prefix/bin/nvcc
cmake --build build-cuda/native --parallel 4 --target \
  SparQ_Common_cuda SparQ_QRAMSimulator_cuda SparQ_Simulator_cuda SparQ_Algorithm_cuda
# 3) run the million-dimensional production case (defaults: n=2^20, kappa=16)
./build-cuda/native/Experiment_PoiseuilleQDABench_cuda --outdir run_p1 --czEps 1e-16
```

Artifacts per run: `residual_curve.csv` (step, res_dir, res_opt,
success_prob, fidelity) and `qda_summary_cpp.json` (final metrics,
termination reason). The 2^20
production numbers (canonical P1: res_dir 5.178909e-3, fid 0.9999989,
133,230.6 s wall, 50.3M peak systems on one A800-SXM4-80GB) and the
equivalence chain that validates the CUDA kernels against the CPU core and
the Python package are recorded in `v1d-qda-linsolver-a800.md`.

## Dimension contract and padding

Every solver entry point takes the dimension as a qubit count
(`--nqubit`, n = 2^nq) and builds the tridiagonal operator analytically
(`A = tridiag(beta, alpha, beta)`, `b = ones(n)/sqrt(n)`). **There is no
in-repo padding of arbitrary external dimensions**: a caller with N != 2^k
must pad to the next power of two before export and owns the padding
semantics (zero-padding changes the solved system). That is a cross-repo
interface decision recorded in `.agent-task/interfaces.md`, not a capability
of this repository.

## Input export

The solver benches do not read matrix files; they encode their operator
analytically (block encoding of the tridiagonal family). The *standard*
cross-repo input channel this repository ships is the versioned RIR program
plus sparse QRAM memory (JSON), executed by `pysparq.run_rir`, the
`pysparq rir` CLI and the workbench `/api/rir` panel — with digest,
provenance and full-width amplitude export. General dense-matrix ingestion is
not an entry point of this repository.

## Trajectory-level MPI vs distributed state slicing

Two different capabilities that must not be conflated:

- **Trajectory-level (ensemble) MPI — exists.** `qram_simulator.mpi`
  (QRAM-Simulator submodule, single node, CPU) shards statistically
  independent noisy Monte-Carlo trajectories cyclically over ranks with
  deterministic per-trajectory reseeding, so aggregate results are
  independent of the process count. Entry: any rank-counted run of the
  submodule's ensemble API (`mpirun -np 4 python ...`); verified by the
  submodule's four-rank CI regression (`test_mpi_ensemble.py`). The SparQSim
  C++ core itself ships no MPI path (see README "MPI").
- **Distributed state slicing — does not exist in this stack.** No component
  of SparQSim or the pinned QRAM-Simulator partitions one quantum state
  across processes/GPUs. The scale path for a single state is the CUDA
  sparse-state backend (measured 9.6 GB peak at n = 2^20 with
  `-DCACHED_REGISTER_SIZE=8`), not distribution.

## Metric hygiene

Constructor-level improvements (e.g. the sparse-QRAM constructor gains
recorded in the workspace acceptance materials, including the 404x figure)
are memory-construction engineering wins. They are **not** application-level
end-to-end solve-speedup claims, and no application acceleration is asserted from
them anywhere in this repository.

## Target-machine re-test (listed separately, not claimed done)

The commands above are the exact re-test recipe for a user target machine
(CUDA toolkit + `-DSPARQ_ENABLE_CUDA=ON -DSPARQ_BUILD_EXPERIMENTS=ON`, then
`PoiseuilleQDABench_cuda` with the P1 schedule `--czEps 1e-16`). No numbers
from a user target machine are asserted on this page; when such a re-test
happens it will be recorded with its own hardware line in `v1d`.
