# V1e CKS linear solver with QRAM-free direct oracles (2^20 production on A800)

CKS (Chebyshev quantum-walk LCU) linear-system solver on the same Poiseuille case
as the QDA benchmark (V1d): `A = tridiag(-1, 2.2666666666564934, -1)`,
`b = ones(n)/sqrt(n)`, `n = 2^20`, `kappa = 16`. New in this round: the CKS walk's
two QRAM table oracles are replaced by **direct arithmetic oracles**
(`SparQ_Algorithm/include/hamiltonian_simulation_tridiagonal.h`,
CPU + CUDA), removing the QRAM circuit entirely.

## What was broken upstream and how it was fixed

1. **`T::impl/impl_dag` never registered the `"rotation"` Rational register**
   (`SparQ_Algorithm/include/hamiltonian_simulation.h`): the name-based
   `GetQWRotateAngle_Int_Int_Int` constructor resolves registers immediately, so
   any whole-solver CKS run threw "Register not found" before producing a single
   step. Fixed by `AddRegister("rotation", Rational, 32)` before first use in
   both directions.
2. **Stock Chebyshev coefficient family floors at res ~ 0.2-0.4 on this case**
   (classical check against the exact `T_{2j+1}` recursion; `j0` 241->1200
   bitwise-identical results, `eps` 1e-2->1e-5 only improves res 0.396->0.211 —
   truncation is not the bottleneck, the family is). Replaced by
   `ChebyshevInverseCoefficients`: collocation least squares (Lobatto grid,
   4097 points) of `1/x ~ sum_j d_j T_{2j+1}(x)` on the *exact quantized* walk
   spectrum. Quantum engine results with the true family:
   dim16 J=640 `res_dir = 2.1e-8`, dim64 J=1024 `res_dir <= 1e-7`,
   both `fid = 1.00000000` (vs Thomas solve). The quantum run costs ~2 orders
   vs the classical lstsq fit at equal J (double-precision cancellation against
   `sum|d|`), so the production truncation is `J = 1024` for margin.
3. **Sign handling**: the walk rotation uses `|v|` (positive-only encoding), so
   the walk implements `|A|`, not `A`. For all-negative off-diagonals
   `|A| = D*A*D` with `D = diag((-1)^i)` is an exact similarity, hence
   `A^{-1}|b> = D|A|^{-1}D|b>`: input `D|b>` (Hadamard + `TridiagParityPhase`),
   multiply extracted amplitudes by `(-1)^i` classically. Verified on the QRAM
   path (dim16 fid vs signed solution 0.860 -> 0.955 with stock coefficients,
   1.0 with true coefficients).

## Direct oracles: bit-exact mirror of the QRAM circuit

The QRAM path stores one combined memory `[elements | sparsity]` of `8n` words;
`SparseMatrixOracle2` converts slot<->column via quantum binary search + QRAM
XOR-load + row-address add/sub, tagging garbage branches into the
`k_comp` (binary-search result) register. The direct oracles
(`TridiagOracle1/2`) reproduce the composite register maps arithmetically:

- element load: `data ^= mem[(data_offset + 4j + k) & (8n-1)]` with `mem`
  computed from `(alpha_enc, beta_enc, window_base)` (O(1), no table);
- slot->column (dagger): `k += row_addr`; `k ^= mem[k]`; `k_comp ^= hit_addr`;
- column->slot (forward): binary search = range test `k in [base, base+4)`;
  `k_comp ^= hit_addr`; `k ^= mem[k_comp]`; swap; `k -= row_addr` (mod 2^32).

**Why the junk tagging matters** (found by op-by-op norm instrumentation,
`--path walkdebug`): the final `Hadamard_Int` of `T.dag` scatters group
amplitudes into buckets `k - value_mask`; an earlier XOR-only prototype left
garbage columns in multi-system groups with mixed high bits, writing amplitudes
out of bounds and corrupting the norm (1.0 -> 0.9393 at dim8 step 0). With the
QRAM-compatible junk isolation every garbage branch sits in a singleton
`k_comp`-tagged group (safe `n==1` path) and clean groups span `k in {0..3}`.
After the fix the direct walk's per-op system counts match the QRAM reference
exactly (54 -> 98 -> 152 at dim8).

| Anchor (CPU probe `Experiment_CKS_TridiagonalProbe`) | Result |
| --- | --- |
| dim16 `--path both --coef stock` (direct vs QRAM reference) | all metrics identical: `res_dir=3.962820e-01`, `res_opt=1.522807e-01`, `fid=0.95515840`, `p_succ=2.288431e-01` |
| dim8 `--path walkdebug` (4 walk steps, per-op norm) | norm `1.000000000000` after every operator |
| dim16 `--coef true --J 640` | `res_dir=2.107342e-08`, `fid=1.00000000` |
| dim64 `--coef true --J 1024` | `res_dir<=1e-7` (prints 0), `fid=1.00000000` |

## CUDA backend additions

The CKS walk had never run on the CUDA sparse-state backend; the missing
operator implementations were added (bit-exact mirrors of the CPU semantics):

- `CondRot_Rational_Bool` CUDA (`SparQ/src/cuda/condrot.cu`): per-system scatter
  into the two output-bit branches (`a*cos` own branch, `+-a*sin` flipped
  branch) followed by sort-merge-dedup; exact for <=2 contributions per key
  (IEEE addition is commutative), matching the CPU pairwise rotation.
- `GetQWRotateAngle_Int_Int_Int` CUDA (appended to
  `SparQ_Algorithm/src/cuda/hamiltonian_simulation.cu`): per-system ratio ->
  `acos(sqrt(ratio))/(2 pi)` -> rational fixed-point XOR.
- GPU-native `add_systems` (`SparQ/src/cuda/system_operations.cu`): replaced
  the unconditional `copy_to_cpu` round-trip (GBs of PCIe traffic per LCU term
  at 2^20) by scaled append + `CuSortMergeUniqueErase` (sort + reduce-by-key +
  epsilon-prune), semantics identical to the CPU `sort_merge_unique_erase`.
- LCU accumulator residency (`hamiltonian_simulation_tridiagonal.h`): the
  container's `current_state` is default-constructed CPU-resident, which made
  every per-term `add_systems` take the host fallback — a D2H copy plus a
  single-threaded `std::sort` of the whole accumulated state (the 2^20 run
  stalled there: 100% one core, 0% GPU, no progress past FirstStep; dim14
  spent 90% of its 222 s wall in the same branch, invisible because the
  wrapper profiler entry is named "add_systems Cuda" either way). The
  constructor now moves the accumulator to the GPU under `USE_CUDA`; the
  GPU-native merge is validated by the dim16/dim64 succP parity below (bit
  identical to both the CPU branch and the CPU reference), and dim14 drops
  222 s -> 28 s.
- The tridiagonal oracles themselves (`hamiltonian_tridiagonal.cu`): thrust
  functors using the same formulas as the CPU implementation.

Build: dedicated `build-cks` directory. The register-layout define must reach
every translation unit (nvcc and host compiler alike); it is passed as
`-DCMAKE_CUDA_FLAGS=-DCACHED_REGISTER_SIZE=16 -DCMAKE_CXX_FLAGS=-DCACHED_REGISTER_SIZE=16`
(the same mechanism the QDA `build-cuda` uses with 8; the `CACHED_REGISTER_SIZE`
CMake cache option in `extern/qram-simulator` does not propagate to the SparQ
targets, and a mismatched layout between translation units silently corrupts
the sparse state). 16 covers the CKS walk's ~11 peak simultaneous registers
(6 walk + input + `data` + `rotation` + `k_comp` + transient) with headroom.
`-DCMAKE_CUDA_ARCHITECTURES=80`, `-DSPARQ_ENABLE_CUDA=ON`,
`-DCMAKE_BUILD_TYPE=Release`.

Two bugs were found and fixed while bringing the CUDA backend up (both
reproduced on a 4-system `--debugStages` unit test with known amplitudes,
rotation pi/4; expected j=5 pair output `(0.4243, -0.5657)/(0.4243, +0.5657)`):

- the scatter kernel zipped the two output slots against each other instead of
  against the input vector, so it rotated default-constructed garbage
  (norm 1.0 -> 4222 through the first `T.C` at dim16);
- `CuSortMergeUniqueErase` erased only the pruned prefix of `keys_out`, leaving
  stale default-constructed systems beyond the reduce-by-key group count in the
  state (phantom amplitude-1.0 systems).

| Anchor (CUDA `Experiment_CKS_PoiseuilleBench_cuda`) | Result |
| --- | --- |
| dim16 `--coef true --J 640` | `succP=1.004941e-04` (CPU: `1.004941e-04`, all 7 printed digits), `res_dir=0 <= tol`, `fid=1.0`, `max_system_size=312` (= CPU) |
| dim64 `--coef true --J 1024` | `succP=5.101613e-02` (CPU: `5.101613e-02`, all 8 printed digits), `res_dir=3.33e-8`, `fid=1.0`, `max_system_size=1272` |
| 2^20 production | see below |

## 2^20 production run

Configuration: `--nqubit 20 --kappa 16 --eps 0.01 --ds 32 --coef true --J 1024
--logEvery 64 --tol 1e-6` on one A800 (GPU5), output `residual_curve.csv` +
`qda_summary_cpp.json` (artifact contract identical to the QDA benchmark;
collected with `bench/linear_solver_cpp_collect.py --engine cks-cpp
--entry-id T2-LS-1e6-cks`).

Results (2026-10-20, run dir `T2-LS-1e6-kappa16-cks-cpp`):

- quantized walk spectrum `[0.015625000, 0.250000000]`, `kappa_interval = 16.0`
  exactly as the analytic prediction;
- walk state after FirstStep = 20,971,512 systems (max_system_size, steady
  through the run); ~4.2 s per Chebyshev term at full GPU utilization;
- residual curve (direction residual `res_dir = sqrt(2 - 2<y,b>/||y||)`):
  term 192 `9.0e-4` -> term 448 `4.2e-5` -> term 576 `5.3e-6` ->
  term 640 `0` — terminated early by `residual_below_tol(1e-06)` at
  640/1024 terms (walk_steps 1281), `final_res_dir = 0 <= tol`,
  `res_opt = 0`, `fid = 1.0000000000`, `succP = 4.75e-2`;
- wall time 2681.9 s (44.7 min) including all LCU merges and extractions.

`res_dir = 0` means `A<x|/||A<x||` is parallel to `b` to the last double bit
(the formula `2 - 2*proj/||y||` rounds to zero at ~1e-16 relative alignment);
the fidelity `|<xref|x>|^2 = 1 + 3.6e-11` shows the rounding scale. Together
with the dim16/dim64 success-probability parity this anchors the GPU pipeline
against the CPU reference end to end.
