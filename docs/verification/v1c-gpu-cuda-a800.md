# V1c GPU/CUDA backend verification (A800, Linux)

Recorded from the 2026-10-17 Linux migration run and re-run on
2026-10-19 with identical outcomes. Raw logs (build logs, per-test
stdout, environment snapshot) are archived in the project test-report
workspace under `reports/live/T2-gpu/`.

## Environment

- 8x NVIDIA A800-SXM4-80GB server, Ubuntu 22.04
- CUDA 12.8 (driver 570.211.01), `-DCMAKE_CUDA_ARCHITECTURES=80` (sm_80)
- `SPARQ_ENABLE_CUDA=ON`, OpenMP parallel, no TBB

## Build

All CUDA targets compile and link, including `CudaArithmeticTest`,
`CudaBenchmark`, the `Experiment_GPUTime_*` programs and the `_core`
bindings. The single exception is `CudaExecutabilityTest`
(`CondRot_Rational_Bool` vtable/dag symbols undefined) — the known
CondRot-refactor gap that also holds the vendored
`extern/qram-simulator` CUDA path at CPU-only.

## Test matrix

| Test | Result | Note |
| --- | --- | --- |
| CudaArithmeticTest | pass | "All tests passed." |
| CudaBenchmark | pass | CPU-vs-GPU agree (`max \|dAmp\| = 1.552e-17` against the 1e-12 tolerance); normalization and analytic spot-checks pass |
| Experiment_GPUTime_parallel_test | pass | full CUDA timing table (SplitRegister/CombineRegister Cuda, `copy_to_cpu`, ...) |
| Experiment_GPUTime_block_encoding_test | fail (abort) | `std::invalid_argument: Invalid input.` mid-run |
| Experiment_GPUTime_cks_test | fail (abort) | `std::runtime_error: Register not found.` at startup |
| Experiment_GPUTime_qda_test | fail (abort) | `std::invalid_argument: Invalid input.`; fails in CPU mode too — QDA/CondRot refactor WIP, not an sm_80 issue |

## Status claim

Verified: the CUDA backend builds and runs on CUDA 12.8 / sm_80
(Linux) in addition to CUDA 13 / CCCL 3 (Windows), and the GPU kernels
agree with the CPU path on the arithmetic/benchmark suite.

Not claimed: GPU kernels for the production CondRot primitives
(operators still fall back to the CPU path on GPU states) and the
block-encoding/CKS/QDA experiment programs listed as failing above.

## MPI link check (environment-level, out of repo)

OpenMPI 4.1.2 + mpi4py, `mpirun -np 4`: four processes each import
pysparq 0.2.1, build SparseState/registers and complete an
`allgather`. This validates the runtime environment only — the C++
core ships no MPI-distributed execution path.
