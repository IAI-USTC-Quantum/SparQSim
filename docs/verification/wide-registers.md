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
the QRAM `word_memory.py` as committed on the QRAM candidate branch
(`985ff35`, sha256
`0e1ef2212906f103eca8ddbd5de40d23e64f9b2b967b8e0036e12578957a0539`,
byte-identical to the file the CMake install will take from the submodule;
the copy is an environment overlay, not a second source in this repository).
The committed version adds condition-aliasing validation, portable
`conditioned_by_bit` return handling and read-only capacity properties over
the earlier draft; the full regression and cross-validation below were
re-run against exactly this committed content.

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

> Update 2026-11-09: the forward-looking sentence above is kept as the
> historical rehearsal record it was; the formal gitlink bump to `53f0261`,
> the 8-platform wheel + sdist matrix and the releases have since happened
> (v0.4.0 and v0.4.1 tags on main; see the repository releases).

## Round 2 (2026-11-09): narrow zero-control shared-address defect, fixed

The narrow dense path had the exact defect the wide path was designed
against: `_with_conditions` emulated zero controls by toggling the control
bits, and a toggled bit that also belonged to the query address re-addressed
the dense query. Reported reproduction (see
`out/acceptance/20261109T210828/`, local evidence only):
`Bits(address=4, data=3)`, `H(address[3])`, querying `bank={0:5, 3:7, 8:1}`
under `address[3] == 0` returned the word of the bit-flipped cell 8 (result
`(0, 1)` instead of `(0, 5)`) on the released **pysparq 0.4.0** from PyPI.
The classical interpreter and the OracQ gate-level adapter both give
`(0, 5)`.

Fix (`PySparQ/pysparq/rir.py`), mirroring the OracQ candidate adapter:

- a narrow query with a zero-expectation control bit inside its address view
  routes through the same authoritative `SparseWordMemory.apply_xor` (single
  source in the pinned submodule), which merges the original address and
  control constraints and restores the toggled zero bits;
- queries without such sharing (including one-controls sharing the address)
  keep the dense native fast path unchanged;
- accumulated `Control` conditions merge per bit — contradictory
  requirements are a structural no-op, duplicate requirements collapse —
  in `_with_conditions` and `_gphase` alike;
- the sparse word path now enforces the `max_states` budget after the query
  like every other event.

Evidence (all under `out/acceptance/20261109T210828/`):

- New regression `PySparQ/test/test_rir.py::TestControlledQueries` (11
  cases: reported scenario, one-control sharing, dense 4/3 uniform
  superposition under a zero control, external 0/1 controls, condition
  conflicting with cells, contradictory controls, non-zero target XOR,
  double-query restore, module call, Repeat, relative phase). Against the
  released 0.4.0 wheel: **7 of 11 fail**; against the fixed tree: **all
  pass** with the full suite at 555 passed / 2 skipped.
- Three-way cross-validation (`xval_controlled_queries.py`): the OracQ
  candidate (`feat/wide-registers` c554ab1) agrees **11/11 scenarios** on
  all four paths — `simulate`, `run_pysparq`, `run_pysparq_rir` (through the
  installed fixed wheel) and direct `pysparq.run_rir`. The formally released
  OracQ 0.4.0 still rejects address-sharing controls at construction
  ("operands modify a protected control register") and agrees on the 2
  scenarios it accepts; the overlap relaxation rides the OracQ candidate
  (recorded in `.agent-task/interfaces.md`).
- CLI/GUI parity (`cli_gui_rir_parity.py`): `pysparq rir` and the workbench
  `/api/rir` share `run_rir_document`; 15/15 checks pass — identical
  schema/digest/registers/amplitudes/scope on the zero-control and the
  65/129-bit wide fixtures, identical rejection texts for invalid program
  and missing memory (CLI exit 2 / GUI 422), 403 session and 400 size
  guards, and fresh results per request (documented boundary: the GUI body
  cap is a server-side anti-DoS guard; the CLI reads local files without a
  cap).
- Real-browser check (`browser_rir_panel.py`, headless Chrome): 11/11 —
  execute the zero-control program, correct `(0, 5)` result shown, export
  produces a JSON blob download, editing the program invalidates the stale
  result and disables export, re-execution works, zero browser console
  errors, and the exported record reproduces via CLI re-run.
