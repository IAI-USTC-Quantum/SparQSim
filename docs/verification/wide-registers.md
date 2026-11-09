# Wide logical registers and sparse wide-QRAM execution

User requirement: lift the 64-bit logical-register limit together with
OracQ/QRAM-Simulator. Logical registers and QRAM words of any width must be a
real native capability — not one adapter splice, not silent low-64-bit
truncation — while narrow-word optimizations and the old SDK stay intact.

## Native ABI boundary (measured)

The C++ machine-word ABI addresses registers up to 64 bits. On the installed
0.3.0 native extension:

- `AddRegister(name, kind, width>64)` raises `ValueError: Invalid input`.
- `AddRegister` **silently permits duplicate names** (two active registers with
  one name make name-based gate addressing ambiguous).

Consequences: wide logical registers must be backed by several native 64-bit
machine words, and the layout layer must screen derived word names against the
live register table (`System.name_register_map`) before allocating.

## Implementation shape

`pysparq.register_layout.RegisterLayout` maps one logical register to
little-endian bits of `<name>__word_<i>` native registers (last word holds the
remainder). Values are reconstructed as full-width Python ints; recycling
requires zero restoration (checked across every word); duplicate logical names
and native word-name collisions are rejected. The C++ ABI and every executed
operator are unchanged: gates, controls, phases, `Add_ConstUInt_InPlace` on
aligned single words, and controlled native X queries all run on the real
sparse-state backend.

`pysparq.rir.run_rir` routes QRAM resources by width: `address_width <= 20`
and `address_width + data_width <= 64` keep the dense native qutrit path (its
`memory_entry_t` is `uint64_t`, so wider packing is impossible); anything wider
executes through the ideal sparse `SparseWordMemory` XOR path
(`pysparq._qram_words`, installed from the QRAM-Simulator submodule), which
applies exact controlled native `X_Bool` queries over a zero-default sparse
cell dictionary and never allocates 2^address space. Zero/one controls that
coincide with address bits are merged before any temporary toggling, so a
zero control overlapping the address register cannot redirect the query.

## Local regression (2026-11-09)

Development environment: uv venv `out/native-env`, installed formal
`pysparq==0.3.0` wheel (cp313) overlaid with the branch's Python sources and
the QRAM candidate `word_memory.py` (sha256
`0e1ef2212906f103eca8ddbd5de40d23e64f9b2b967b8e0036e12578957a0539`,
byte-identical to the file the CMake install will take from the submodule;
the copy is an environment overlay, not a second source in this repository).

- `pytest PySparQ/test -q` → **555 passed, 2 skipped** (the two pre-existing
  CKS replacement-API skips from the issue-84 refactor, unrelated).
- New `PySparQ/test/test_wide_registers.py`: 24 cases covering widths
  64/65/127/128/129/256, cross-word carry/wrap (`add_const` over 2^96+3 onto
  2^129-1, view wrap across the word-60 boundary), views (slices, multi-span
  concatenation, xor/swap across words), high-bit entanglement and relative
  phase (Rot on bit 129 / bit 255), controlled global phase on high word bits,
  adjoint identity on non-zero wide state, wide workspace uncompute
  (accept/reject), wide QRAM with non-zero initial data, unquery
  (`Adjoint` of a controlled load restores the data register), mixed 0/1
  controls across the address word boundary, dense↔sparse native path
  agreement, a 40-bit-address sparse memory (no 2^40 allocation), layout API
  edge cases (duplicates, collisions, value reconstruction, registry clean).
- Native C++ (source build, `-DSPARQ_BUILD_TESTS=ON`): `ctest` → **190/190
  pass** (5 `DynamicOperatorTest` JIT cases are skipped by default behind
  `ENABLE_DYNAMIC_OPERATOR_TEST=1`, pre-existing split-repo dev gating; the
  real JIT behavior is covered by the installed-product Python tests, green).
- CUDA: with CUDA 13.4 (`/usr/local/cuda`), `SparQ_Algorithm_cuda` failed to
  compile — the `GetAs` macro's `as<size_t>` template argument trips the 13.4
  device frontend inside the host template `check_removable` (CI's 12.8.1 and
  13.3.0 pass; sm80 reproduces under 13.4, so it is not arch-specific).
  Reading the raw register value (zero-checks are truncation-invariant) fixes
  it; all four device libraries (`SparQ_Common_cuda`,
  `SparQ_QRAMSimulator_cuda`, `SparQ_Simulator_cuda`, `SparQ_Algorithm_cuda`)
  then compile for **both sm80 and sm120** (local GPU: RTX PRO 5000 Blackwell;
  no GPU runtime results are asserted here).
- Cross-validation: `out/xval_oracq.py` runs the same programs through the
  OracQ classical interpreter (`oracq.infrastructure.execution.simulate`,
  candidate `oracq/out/wide-registers` — released 0.4.0 still caps widths at
  64) and `pysparq.run_rir`: **20/20 programs agree component-wise** (narrow
  bell/controls/rotations/arithmetic/call-repeat-adjoint/dense QRAM; wide
  h+phase at six widths, cross-word carry, view xor/swap, multi-span view,
  adjoint add_const, workspace uncompute, wide QRAM with non-zero data +
  mixed controls, unquery, 40-bit sparse address space). Log:
  `out/xval_oracq.log`. The same narrow program set (plus a 64-bit-boundary
  register case) also agrees **7/7 against the formally released OracQ
  0.4.0** from PyPI (`out/xval_oracq_formal.log`); wide cases against the
  formal interpreter become possible once OracQ lifts its 0..64 validation.
- Product paths: `pysparq rir` CLI (real wide RIR file input, full-width
  amplitude export) and the workbench `/api/rir` panel (session/origin/size
  guarded, subprocess execution) — 24 demo tests pass.

## Candidate packaging rehearsal (not a release claim)

With the QRAM candidate `word_memory.py` temporarily placed in the submodule
**working copy** (restored afterwards; the gitlink stays at `edacf6e`), a
wheel was built from source (`python -m build --wheel`):
`pysparq-0.3.1.dev3+gcf78da7eb-cp313`. Against that installed wheel:

- `tools/check_installed_product.py` (now also requiring the installed
  `pysparq/_qram_words.py` to be byte-identical to the pinned submodule's
  `word_memory.py`) → 24 checks + 18 notices pass.
- `pytest test_wide_registers.py test_rir.py test_demo.py` → 73 passed.
- Installed CLI `pysparq rir` executes the 65/129-bit wide query correctly.

The formal gitlink bump, 8-platform wheel + sdist matrix, tag and release
happen only after QRAM-Simulator lands and releases the helper on its main;
those results will be recorded separately.
