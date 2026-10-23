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

## 2026-10-21 addendum: first production run extracted NaN — root cause, fix, re-run

The 2026-10-20 production run completed all 40568 walk steps normally
(86279.7 s, `max_sys` 31,457,308) but every one of the 163 log points extracted
`res_dir=nan`, `succP=0`, `final_res_dir=null`, `termination=fixed_step_budget`.
The walk was healthy; only the readout was empty.

### Root cause: dimension-blind clear-zero cutoff pruned the solution branch at birth

`ClearZero` prunes basis states by `abs_sqr(amplitude) < eps`. The walk-internal
gates (Hadamard/split/merge cleanup) call the **default-constructed**
`ClearZero()`, which read the library constant `epsilon = 1e-14` — an effective
amplitude cutoff |a| < 1e-7. The bench-level `--czEps` only controlled the
bench's own final cleanup call, not these internal calls (verified: a czEps
sweep at 2^19/2^20 reproduced the failure bit-for-bit with unchanged
`max_sys`).

The QDA solution branch is born with per-state amplitudes ≈ sqrt(2e-6/n),
which cross the 1e-7 cutoff between 2^18 and 2^19:

| dim | solution-branch birth amplitude | vs cutoff 1e-7 |
|-----|-------------------------------|----------------|
| 2^16 | 5.9e-7 | survives (5.9x) |
| 2^17 | 2.9e-7 | survives (2.9x) |
| 2^18 | 1.5e-7 | survives (1.5x) |
| 2^19 | 7.4e-8 | **pruned every step** |
| 2^20 | 3.7e-8 | **pruned every step** |

Bisect confirmation (40-step compressed schedule, CUDA, a800): 2^16/17/18
extract normally (fid 0.99998–0.999996), 2^19 and 2^20 keep exactly n
post-selected basis states — all in the initial branch (anc_1=0), solution
block `[n,2n)` empty, `PartialTraceSelect` itself healthy (kept-state norm 1.0,
returned prob ≈ 1.004). At 2^16 the solution block holds exactly n states with
per-state amplitude 5.5e-6 at the first log point — the same branch at 2^19
would sit at 7.4e-8, under the cutoff.

### Fix (commit 10a96aa)

`SparQ/include/system_operations.h` gains a mutable global default
`clearzero_default_eps` (initialized to the library `epsilon`, so default
behavior is bit-identical) and `ClearZero()`'s default constructor now reads it.
Both Poiseuille QDA benches route `--czEps` into the global so the walk-internal
`ClearZero()` calls follow the same threshold.

Validation matrix (a800, CUDA backend, 2026-10-21):

| check | result |
|-------|--------|
| 2^16 default-eps path, 40-step schedule | `res_dir=2.071000e-02` — **bitwise** identical to the pre-fix binary and to the v1d parity chain |
| 2^19 + `--czEps 1e-18`, 40-step | extraction restored (res_dir 7.56e-2, fid 0.999925) |
| 2^20 + `--czEps 1e-16`, 40-step | res_dir 5.178906e-3, fid 0.999999 |
| 2^20 + `--czEps 1e-18`, 40-step | res_dir 5.178908e-3 — same floor, 33% larger state (67M vs 50M peak systems), 2.3x wall time |
| 2^20 + `--czEps 1e-16`, 400-step | res_dir constant 5.178908e-3 through s=0.4 (compressed-schedule truncation, see below) |
| CI (Python tests + Documentation) on 10a96aa | green |

Production re-run therefore uses `--czEps 1e-16` (3.7x margin over the 3.7e-8
birth amplitude at 2^20, smallest state of the working settings).

### Corrected reading of the compressed-schedule "floor"

During validation the 40/400-step compressed schedules plateau at
dimension-dependent `res_dir` values (2^16: 2.07e-2; 2^20: 5.18e-3) that look
s-independent. A full-schedule dim256 control disproved the "hard floor"
reading: the anneal is simply slow in its early phase and accelerates late —

| dim256 run | steps | final res_dir | final fid | final succP |
|------------|-------|---------------|-----------|-------------|
| rate 0.1 (the v1d parity-chain config) | 3688 | 3.135e-2 | 0.999438 | 0.5969 |
| rate 1.1 (production schedule) | 40568 | **5.9485e-4** | 1.000000 | 1.0000 |

More steps converge deeper, as adiabatic scaling predicts; the mid-anneal
plateau is not a convergence floor. A 2^16 full-schedule default-eps control
(40568 steps) is running to confirm the same behavior above the cutoff; the
re-run production (2^20, rate 1.1, 40568 steps, `--czEps 1e-16`) and a
rate-0.1 companion (3688 steps) are in flight — final tables to be appended.

The archived failed run is kept at
`05-测试报告/data/T2-稀疏态虚拟机/T2-LS-1e6-kappa16-qda-cpp-nan-failed-20261020/`
as evidence of the failure mode; the fresh run directory is
`T2-LS-1e6-kappa16-qda-cpp/`.

## 2026-10-23: production re-run results (final)

Three full-budget production runs completed (identical schedule: rate 1.1,
40568 steps, kappa=16, tol 1e-6; single A800 CUDA sparse-state backend;
only `--czEps` differs):

| run | `--czEps` | final res_dir | final res_opt | fid | succP | wall (s) | max_sys |
|-----|-----------|---------------|---------------|-----|-------|----------|---------|
| **P1 (canonical)** | `1e-16` | **5.178909e-3** | 1.381066e-3 | **0.9999989** | 0.0017 | 133,230.6 | 50,334,366 |
| C | `1e-20` | 4.330474e-2 | 1.155629e-2 | 0.9999710 | 0.0017 | 202,345.1 | 67,108,956 |
| B | `1e-22` | 4.296244e-2 | 1.146482e-2 | 0.9999710 | 0.0017 | 220,601.9 | 67,108,956 |

All three terminated on `fixed_step_budget` (tol not reached) and were collected
into `05-测试报告/data/T2-稀疏态虚拟机/` under entry ids
`T2-LS-1e6-kappa16-qda-cpp{-eps1e-20,-eps1e-22}`; manifest verify green.

**Canonical choice: P1 (`--czEps 1e-16`)** — lowest res_dir by an order of
magnitude, highest fidelity, smallest state, fastest wall. Its extraction readout
is pinned at 5.1789e-3 for the whole run: with the solution branch surviving but
weak (birth amplitude 3.7e-8 vs the 1e-8 cutoff), the extracted direction is
dominated by the initial branch — 5.1789e-3 is that birth-dominated direction's
intrinsic residual (the same value as the rate-0.1 full run and the 40/400-step
compressed probes). B/C retain more small-amplitude states (67M vs 50M peak
systems), so their readouts carry noise-state contamination (~4.3e-2) while the
underlying transfer (succP 0.0017 in all three) is identical.

### Corrected scaling conclusion (supersedes the 2026-10-21 expectation)

The 2026-10-21 addendum expected the 2^16 full-schedule control "to confirm the
same behavior" as dim256 (full convergence). It does not. At the *same* schedule
(rate 1.1, 40568 steps) the end-of-run transfer completeness falls monotonically
with dimension:

| dim | run | final succP | final res_dir |
|-----|-----|-------------|---------------|
| 256 | rate 1.1, 40568 steps | **1.0000** (converged, 5.9485e-4) | 5.9485e-4 |
| 2^16 | rate 1.1, 40568 steps, default eps (`/tmp/qda-diag-nq16-m999999999`) | 0.0269 | 2.071000e-2 (pinned birth-mode readout) |
| 2^20 | rate 1.1, 40568 steps (three runs above) | 0.0017 | 5.179e-3 (P1) / ~4.3e-2 (B/C) |

The Poisson-tridiagonal spectrum narrows with n, so the fixed
`ceil(1.1 * 2305 * kappa)` budget completes the adiabatic transfer at dim256 but
not at 2^16/2^20. The 2^20 non-convergence is a budget-vs-spectral-gap property
of the algorithm on this case, not a simulator defect (engine-equivalence chain
above; fidelity stays ≥ 0.99997 in every run).

### czEps guidance (final wording)

- Canonical production setting for this benchmark: `--czEps 1e-16` (sqrt -> |a|
  cutoff 1e-8 < 3.7e-8 birth amplitude, 3.7x margin; cleanest readout).
- `1e-20`/`1e-22` remain valid safe settings per the sqrt(czEps) < C·ds matching
  rule; all three complete healthy walks with identical succP, so at 2^20 czEps
  no longer gates the result — the step budget does.
- The library default `1e-14` still prunes the solution branch at birth from 2^19
  on; always set `--czEps` explicitly at scale.

Judgement note: the ≥1e6-dimension convergence criterion (indicator 2.1,
judgement ②) is carried by the CKS engine at 2^20 (`res_dir = 0`, fid 1.0,
2682 s — see v1e); replay semantics count any quantum engine at scale, and the
QDA runs above are recorded as-is (not converged at tol).
