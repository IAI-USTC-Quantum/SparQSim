# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SparQSim publishes the `pysparq` Python package: the full-featured pybind11 bindings
(`PySparQ/core.cpp` → `_core`) plus a pure-Python framework (operators, algorithms,
RIR interpreter, dynamic operator compilation). The C++ core lives in the separate
**QRAM-Simulator** repository, consumed as a git submodule at `extern/qram-simulator`
(relative URL `../QRAM-Simulator.git` — resolves on both Gitea and GitHub; do not
rewrite it to an absolute URL).

## Build Commands

```bash
# Clone with the core
git submodule update --init --recursive

# Build and install pysparq (scikit-build-core drives the root CMakeLists,
# which add_subdirectory's the core submodule with tests/experiments OFF)
uv venv .venv && source .venv/bin/activate   # use uv venv ONLY (do not use pip/conda for env creation)
pip install .

# Run tests (from project root; needs a C++ toolchain — dynamic-operator
# tests JIT-compile generated operators with g++)
pytest PySparQ/test -v

# Regenerate and diff the committed stubs
pip install pybind11-stubgen
pybind11-stubgen pysparq._core -o /tmp/stubs
diff PySparQ/pysparq/_core.pyi /tmp/stubs/pysparq/_core.pyi

# Format/lint (pre-commit)
pre-commit run --all-files

# Bump the core pin (after QRAM-Simulator publishes a tag)
git submodule update --remote extern/qram-simulator
git add extern/qram-simulator && git commit -m "chore: bump qram-simulator to vX.Y.Z"
```

## Architecture

- **`PySparQ/core.cpp`** — the full pybind11 binding (~150 exported names, incl.
  `conditioned_by_*` control surface via `BindUtils.h` macros). This is the
  "rich" binding; QRAM-Simulator's repo carries a separate thin binding —
  C++ API changes may need updates in both places.
- **`PySparQ/pysparq/`** — pure Python, imports only `._core`/numpy/stdlib:
  - `operators/` — Python-side operator framework (`ControllableOperatorMixin`)
  - `algorithms/` — Grover, Shor, QDA, CKS, state preparation, block encoding
    (compose public primitives; see `algorithms/REFACTORING_NOTES.md`)
  - `rir.py` — QECC.Lang RIR JSON interpreter
  - `conformance.py` — declarative operator-conformance harness
  - `dynamic_operator/` — JIT compile_operator: `compiler.py` compiles generated
    C++ against headers; source-tree root detection expects `extern/qram-simulator/`
    (installed wheels use the flat `include/` next to `pysparq/`)
- **`PySparQ/consumer_runtime_inventory.json`** — frozen record of PySparQ symbols
  consumed at runtime by external repos (qecc_lang, quantum_cfd_qfvm); enforced by
  `test_consumer_runtime_contract.py`. Treat as a public-API contract.

### Python API Design

- Algorithm classes (`TOperator`, `QuantumWalk`, …) inherit `ControllableOperatorMixin` (`.conditioned_by_*()` chaining, `.dag()`)
- `ps.SparseState` is a pybind11 C++ object — **no deepcopy/pickle**; in-place mutation is expected
- Two API generations coexist; the old one (mutable state + mid-call `ps.System.clear()`) is deprecated via `__init__.py` aliases

## Tests

```
PySparQ/test/
  conftest.py             fresh_system fixture (autouse), helpers
  test_doc_examples.py    doc examples execute
  test_dynamic_operator.py  JIT compile (needs g++)
  algorithms/             per-algorithm unit + end-to-end fidelity tests
```
CKS/QDA end-to-end fidelity tests compare against C++ reference values from
QRAM-Simulator's `test/CPUTest/CommonTest/CorrectnessTest_*.inl` (values are
transcribed; the C++ files are not read at test time).

## Code Style

- **C++** (bindings): LLVM clang-format, 4-space indent, 120 cols
- **Python**: black (line-length=100), isort (profile=black), flake8
- **CMake**: cmake-format + cmake-lint

## Git Workflow

**IMPORTANT: Push active development only to the Gitea origin. Never push to
the GitHub upstream remote.**

- `origin` → `git@git.chenzhaoyun.com:agony/SparQSim.git` (Gitea, primary)
- `upstream` → `git@github.com:IAI-USTC-Quantum/SparQSim.git` (GitHub, releases)

The Gitea CI (`.gitea/workflows/ci.yml`) clones with `--recurse-submodules`
(credential rewrite via `url.insteadOf`) and runs pytest. GitHub runs
python-tests (3.10/3.12/3.13 × ubuntu/windows), Sphinx docs, and the
pypi-publish workflow on `v*` tags.

## Releasing

Version comes from setuptools-scm over this repo's tags (history carries the
monorepo-era v0.1.x tags; next release is v0.2.0). Before tagging, bump the
submodule pin to a released QRAM-Simulator tag and update CHANGELOG.md.

## Critical: CKS/QDA Python Porting Status

**CKS 和 QDA Python 实现的端到端 fidelity 测试状态：**

### CKS (Chebyshev-Kothari-Somma Linear Solver)

Python 实现在 `PySparQ/pysparq/algorithms/cks_solver.py`：
- `ChebyshevPolynomialCoefficient` ✓；`get_coef_positive_only` / `get_coef_common` ✓（酉性验证通过）；`SparseMatrix` ✓；`TOperator` ✓；`QuantumWalk` / `QuantumWalkNSteps` ✓
- 端到端 fidelity 测试已激活：`test_quantum_walk_chebyshev_fidelity`（对应 C++ `Chebyshev_test()`，fidelity >= 0.999）、`test_lcu_linear_solver_fidelity`（fidelity >= 0.9999）
- **已知限制**：`QuantumBinarySearch._find_column_position` 的逆操作在某些情况下未完全 uncompute，可能导致 `RemoveRegister` 抛 `RuntimeError`

### QDA (Quantum Discrete Adiabatic Linear Solver)

Python 实现在 `PySparQ/pysparq/algorithms/qda_solver.py`：
- `compute_fs` / `compute_rotation_matrix` / `chebyshev_T` / `dolph_chebyshev` / `compute_fourier_coeffs` / `WalkS` / `BlockEncodingHs` / `LCU` / `Filtering` ✓；`BlockEncodingHs.dag()` ✓（21 步逆操作）；`qda_solve()` ✓
- 端到端 fidelity 测试已激活（对比 C++ `CorrectnessTest_QDA_CompareList.inl` 参考值，diff < 1e-5）

### 下一步行动

1. **完善 QDA 结果提取**：`qda_solve` 当前返回 `np.linalg.solve` 的经典结果，量子振幅提取是 TODO
2. **修复 CKS uncompute 边界情况**
3. **完善 QDA `is_positive_definite=True` 分支测试**
