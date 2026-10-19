# V1d QDA linear-solver 2^20 production on A800 (CUDA sparse-state backend)

Production benchmark: `Experiments/QDA/QDA_Poiseuille/PoiseuilleQDABench.cu`
(CUDA driver, identical contract to the CPU driver and to the validated
`linear_solver_bench.py`).

- Problem: 1D Poisson tridiagonal `A = tridiag(beta, alpha, beta)`,
  `b = ones(n)/sqrt(n)` (hadamard_b), `n = 2^20 = 1048576`, `kappa = 16`
  (`alpha = 2.2666666666564934`, `beta = -1`, analytic `kappa = 16`), `p = 1.3`,
  step budget `ceil(1.1 * 2305 * 16) = 40568`, `tol = 1e-6`, `clearzero_eps = 1e-14`.
- Hardware: single A800-SXM4-80GB (CUDA 12.8, sm_80), CUDA backend
  `CuSparseState`, built with `-DCACHED_REGISTER_SIZE=8`.

## Engine equivalence chain (all cross-validated on this exact case)

| Anchor | Comparison | Result |
| --- | --- | --- |
| dim256, rate 0.1, full 3688-step curve | pysparq (python) vs C++ CPU core | max abs diff `res_dir` 2.3e-14, `fid` ~1e-15 (final values bitwise) |
| dim256, rate 0.1 | pysparq (python) vs CUDA backend (optimized kernels) | max `res_dir` 1.547e-14, `fid` 3.3e-15; final `res_dir` bitwise identical across python / CPU / both CUDA kernel generations |
| dim8, rate 0.05, 1844 steps | C++ CPU vs CUDA backend | max `res_dir` 1.15e-13, `fid` 3.2e-15 |
| n=2^16, truncated 40-step schedule | old (group-thread) vs new (element-parallel) CUDA kernels | every log point bitwise identical (`res_dir` 2.071000e-02 at all 4 points) |

The element-parallel kernel rewrite performs the identical memory operations and
FP ops per element pair; regressions before/after are bitwise equal, so the
2^20 production numbers below are attributable to the same validated math.

## Why a rewrite was needed (n=2^20 feasibility)

1. **Memory** — the sparse-state walk densifies to `2^25` amplitudes
   (`2^20` main x ancilla combinations). At the default 64-slot register cache
   each amplitude costs ~528 B and transient sort/partition copies pushed peak
   demand past 80 GB (OOM at walk step 3; located with a cudaMalloc interceptor
   + addr2line: `split_systems -> thrust partition -> vector_base::append` under
   `Hadamard_Int_Full`). Building with `-DCACHED_REGISTER_SIZE=8` (the circuit
   uses 6 live registers; CUDA asserts <= 64) cuts the payload ~6.6x:
   measured peak **9.6 GB** at steady state.
2. **Speed** — with memory fixed, the walk ran at 47.7 s/step (~21 days for the
   40568-step budget). nsys on an n=2^16 run attributed **98.7%** of GPU time to
   the two `Hadamard_Int_Full_operate_*` kernels: one thread per unique group
   serially expanded/butterflied the whole group through double-indirect
   `state[indices[...]]` accesses (~1% of memory bandwidth). Rewritten
   element-parallel (one thread per child element / per butterfly pair; phases
   split into separate launches so default-stream ordering preserves the
   group-local init -> place -> butterfly dependency).

Measured effect (same binary family, A800, this benchmark):

| Scale | State size | Before | After | Speedup |
| --- | --- | --- | --- | --- |
| dim256 | ~17.5k systems | 60 ms/step | 30 ms/step | 2x |
| n=2^16 | ~4.2M systems | 3.9 s/step | 0.24 s/step | 16x |
| n=2^20 | ~33.5M systems | 47.7 s/step | 1.33 s/step | 36x |

Also fixed in this pass: the `Hadamard_Bool` CUDA debug trap that forced a host
round trip per call (last host fallback in the walk hot path).

## 2^20 production run (pending)

Launched 2026-10-20 05:26 (a800, GPU 3), budget 40568 steps, log every 250
steps, early break when `res_dir < 1e-6`. Results table to be appended on
completion: final `res_dir` / `res_opt` / fidelity / success probability,
steps executed, wall time, and the collected replay artifact path under
`05-测试报告/data/T2-稀疏态虚拟机/`.

Notes recorded for the report (non-judging quantities):

- The CUDA `Normalize` retains slightly more post-selection population than the
  CPU path (dim256 final succP 0.597 vs 0.430); `res_dir` / `fid` are unaffected.
- Early-schedule log points report `nan` residuals while the post-selection
  population is below extraction sensitivity; the replay judgment reads only the
  final summary values.
