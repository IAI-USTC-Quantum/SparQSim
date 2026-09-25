# SparQSim / pysparq

English | [简体中文](README_zh-cn.md)

[![arXiv:QRAM](https://img.shields.io/badge/QRAM_Simulator-arXiv%3A2503%2E13832-b31b1b.svg)](https://arxiv.org/abs/2503.13832)
[![arXiv:SparQ](https://img.shields.io/badge/SparQ-arXiv%3A2503%2E15118-6f42c1.svg)](https://arxiv.org/abs/2503.15118)
[![PyPI](https://img.shields.io/pypi/v/pysparq.svg)](https://pypi.org/project/pysparq/)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![GitHub](https://img.shields.io/badge/GitHub-IAI--USTC--Quantum%2FSparQSim-181717?logo=github)](https://github.com/IAI-USTC-Quantum/SparQSim)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-4D6AE4)](https://iai-ustc-quantum.github.io/SparQSim/)

> **pysparq — a full-featured Python framework for sparse-state quantum circuit simulation (Register Level Programming, native QRAM, dynamic operators, RIR interpreter)**

## Repository Responsibilities

Dependency direction: **SparQSim → QRAM-Simulator**. SparQSim releases the
[`pysparq`](https://pypi.org/project/pysparq/) package and hosts the SparQ C++ framework
(`SparQ/` sparse-state simulator + `SparQ_Algorithm/` algorithm library + all Python bindings and algorithm-class experiments);
the QRAM base (Common + QRAM + ThirdParty) is released independently from the
[QRAM-Simulator repository](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)
and is referenced and compiled by this repository as a git submodule (relative URL `../QRAM-Simulator.git`).

| Repository | Contents | PyPI package |
|------------|----------|--------------|
| **SparQSim** (this repository) | SparQ C++ framework + the full-featured pysparq Python framework (rich core.cpp bindings + pure-Python algorithm layer) | `pysparq` |
| [QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator) | Pure C++ QRAM base (Common + QRAM circuit core + QRAM paper experiments + thin pybind11 bindings) | `qram-simulator` |

## Installation

```bash
pip install pysparq
```

**Requirements**: Python 3.10 – 3.13, NumPy.

If you need the QRAM circuit-level Python API (the `QRAMCircuitQubit`/`QRAMCircuitQutrit`
noise-simulation workflow), additionally install the standalone package released by the
[QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator) repository:

```bash
pip install qram-simulator
```

**GPU support**: the CUDA/GPU backend is disabled by default; enable it at build time by passing `-DSPARQ_ENABLE_CUDA=ON` (requires a local CUDA toolchain; verified with CUDA 13); the PyPI wheel is always CPU-only.

### Building from Source

```bash
git clone --recurse-submodules <this-repo-url>
cd SparQSim
pip install .
```

## 5-Minute Quick Start

```python
import pysparq as ps

# 1. Create a sparse state
state = ps.SparseState()

# 2. Define registers (the core of Register Level Programming)
addr_id = ps.AddRegister("addr", ps.UnsignedInteger, 4)(state)
data_id = ps.AddRegister("data", ps.UnsignedInteger, 8)(state)

# 3. Initialize a superposition (all addresses with equal probability)
ps.Hadamard_Int("addr", 4)(state)

# 4. Create a QRAM and load data
memory = [i * 2 for i in range(16)]
qram = ps.QRAMCircuit_qutrit(addr_size=4, data_size=8, memory=memory)

# 5. Perform the QRAM load: |addr⟩|0⟩ → |addr⟩|memory[addr]⟩
ps.QRAMLoad(qram, "addr", "data")(state)

# 6. Do arithmetic directly (no gate compilation needed!)
ps.Add_ConstUInt_InPlace("data", 5)(state)   # data = data + 5

# 7. Measure / query probabilities
ps.set_seed(0)
outcome, prob = ps.MeasureZ("data")(state)
dist = ps.Probability.distribution(state, "addr")
```

## Key Register-Level APIs

```python
# Quantum arithmetic - direct register operations (core of Register Level Programming)
ps.Add_UInt_UInt(in1, in2, out)      # out = in1 + in2
ps.Add_UInt_ConstUInt(reg, const)    # reg = reg + const
ps.Mult_UInt_ConstUInt(in, c, out)   # out = in * c
ps.ShiftLeft(reg, n)                 # shift left by n bits
ps.ShiftRight(reg, n)                # shift right by n bits

# Basic quantum gates
ps.Hadamard_Int(reg, n_digits)       # apply Hadamard to an integer register
ps.X_Bool(reg, pos)                  # X gate (on a specific bit)
ps.Z_Bool(reg, pos)                  # Z gate

# QRAM operations
ps.QRAMLoad(qram, addr_reg, data_reg)      # QRAM load
ps.QRAMLoadFast(qram, addr_reg, data_reg)  # fast version

# Seedable measurement / reset / probability queries (for dynamic executors: mid-circuit MEASURE/RESET/QIF)
ps.set_seed(seed)                          # seed the global random engine, making sampling results reproducible
outcome, prob = ps.MeasureZ(reg)(state)    # projective Z-basis measurement: collapse + renormalization
measured = ps.Reset(reg, target=0)(state)  # measure + classically conditioned flips, forcing a reset to target
p = ps.Probability(reg, value)(state)      # read-only Born probability query, leaves the state unchanged
dist = ps.Probability.distribution(state, reg)  # full outcome distribution of a single register (read-only)
```

## High-Level Algorithm Library (`pysparq.algorithms`)

An algorithm layer implemented in pure Python that composes public primitives (Grover, Shor, QDA, CKS, state preparation, block encoding, etc.);
its one-to-one correspondence with the C++ experiment code is documented in [docs/algorithm-implementation.md](docs/algorithm-implementation.md).

## Project Structure

```
SparQSim/
├── SparQ/                  # SparQ C++ sparse-state simulator (umbrella target SparQ defined in the root CMake)
├── SparQ_Algorithm/        # High-level algorithm C++ library (state preparation, block encoding, Hamiltonian simulation, QDA, etc.)
├── Experiments/            # Quantum algorithm C++ experiments (QDA/Grover/QFT/Shor/QCNN/CKS/GHZ, etc.)
├── test/                   # C++ tests (SparQ unit tests + full CommonTest; gated by SPARQ_BUILD_TESTS)
├── extern/qram-simulator/  # QRAM base submodule (Common + QRAM + ThirdParty; relative URL ../QRAM-Simulator.git)
├── PySparQ/
│   ├── core.cpp            # pybind11 rich bindings (_core module)
│   ├── pysparq/            # Python package (operators/ algorithms/ rir conformance dynamic_operator)
│   └── test/               # pytest suite
├── docs/                   # Sphinx documentation + algorithm porting guide
├── examples/               # C++ and Python examples (C++ part gated by SPARQ_BUILD_EXAMPLES)
└── pyproject.toml          # pysparq package (scikit-build-core + setuptools-scm)
```

## Release Process

1. Merge changes into main on Gitea (the primary development repository);
2. When a new core version is needed, first wait for [QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)
   to release the corresponding tag, then run `git submodule update --remote` (or check out that tag) and commit the pin;
3. Sync to the GitHub upstream `IAI-USTC-Quantum/SparQSim`;
4. Update `CHANGELOG.md` and tag `vX.Y.Z` (continuing the pysparq version series; next release v0.2.0);
5. Push the tag or create a GitHub Release → the `pypi-publish` workflow automatically builds
   cp310–313 × (manylinux / win_amd64) wheels + sdist (the sdist embeds the core source and is self-contained)
   and publishes them to PyPI (Trusted Publishing / OIDC).

The frozen API dependencies of external consumers (qecc_lang, quantum_cfd QFVM) are listed in
`PySparQ/consumer_runtime_inventory.json` and enforced by `PySparQ/test/test_consumer_runtime_contract.py`.

## Papers and Citation

See the [QRAM-Simulator repository README](https://github.com/IAI-USTC-Quantum/QRAM-Simulator#papers-and-citation).

## About Us

This project is developed by **[IAI-USTC Quantum](https://github.com/IAI-USTC-Quantum)** (the Quantum Artificial Intelligence Team at the Institute of Artificial Intelligence, Hefei Comprehensive National Science Center).

## License

Apache-2.0 License
