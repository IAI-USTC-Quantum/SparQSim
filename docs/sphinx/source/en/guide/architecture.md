# SparQSim Architecture

This document describes the overall architecture of the SparQSim repository (the home of the SparQ framework), the division of work between repositories, and the core modules.

## Overview

### Project Goals

SparQ is a high-performance simulator framework for simulating **quantum random access memory (QRAM)** and **sparse-state quantum computing**. It aims to provide quantum-algorithm researchers and developers with:

- Tools for efficiently simulating large-scale quantum systems (leveraging the sparse-state representation)
- Accurate modeling of QRAM circuit behavior and noise effects
- A register-level programming paradigm: perform arithmetic and logic directly on integer/Boolean registers, with no manual decomposition into gates
- A complete algorithm library (Grover, Shor, state preparation, block encoding, Hamiltonian simulation, discrete adiabatic QDA, etc.)
- A clean Python API (`pysparq`) for rapid prototyping

### Repository Split

Dependency direction: **SparQSim → QRAM-Simulator**. This repository hosts the SparQ C++ framework
(`SparQ/` sparse-state simulator + `SparQ_Algorithm/` algorithm library + all Python bindings and algorithm-style experiments);
the QRAM base (Common + QRAM + ThirdParty) is released independently in the
[QRAM-Simulator repository](https://github.com/IAI-USTC-Quantum/QRAM-Simulator),
and this repository references and builds it as a git submodule (relative URL `../QRAM-Simulator.git`).

## Directory Layout

```
SparQSim/
├── SparQ/                  # SparQ C++ sparse-state simulator (the umbrella target SparQ is defined in the root CMake)
│   ├── include/            # public API headers (incl. cuda/ GPU backend, currently masked out by CMake)
│   └── src/                # implementation (incl. src/cuda/ GPU kernels)
├── SparQ_Algorithm/        # high-level algorithm C++ library
│   ├── include/            # grover / shor / state preparation / block encoding / Hamiltonian simulation / QDA
│   └── src/
├── PySparQ/                # pybind11 rich bindings + pure Python package
│   ├── core.cpp            # _core module bindings (about 1500 lines)
│   ├── include/            # binding-layer helper headers (core.h, BindUtils.h)
│   ├── pysparq/            # Python package: operators/ algorithms/ rir conformance dynamic_operator
│   └── test/               # pytest suite (incl. API contract tests for external consumers)
├── Experiments/            # quantum-algorithm C++ experiments (QDA/Grover/QFT/Shor/QCNN/CKS/GHZ etc.)
├── examples/               # C++ and Python examples (SPARQ_BUILD_EXAMPLES gates the C++ part)
├── test/                   # C++ tests (gated by SPARQ_BUILD_TESTS)
├── extern/qram-simulator/  # QRAM base submodule (Common + QRAM + ThirdParty; do not modify directly)
├── docs/                   # Sphinx docs + Doxygen (C++ API) + algorithm porting guide
└── pyproject.toml          # pysparq package (scikit-build-core + setuptools-scm)
```

### SparQ/ - Sparse-State Simulator Core

**Responsibilities**: implements the core functionality of sparse-state quantum simulation (register management, gates, measurement, QRAM, quantum arithmetic).

**Core classes** (all under the `qram_simulator` namespace, declared in `SparQ/include/basic_components.h`):

- **`System`**: a single computational basis state holding the complex ``amplitude`` and the register value array ``registers``;
  it also maintains the global register table (names, types, widths) as static members, and is the hub of register-level programming
- **`StateStorage`**: a quantum register storage cell
- **`SparseState`**: a sparse quantum state holding ``std::vector<System>``; default construction creates the ``|0...0⟩`` initial state
- **`BaseOperator`**: the unified operator interface (``operator()`` / ``dag()``, with CPU/GPU overloads), supporting composite and conditional operators
- **`SelfAdjointOperator`**: base class of self-adjoint operators (``dag() == operator()``), e.g. Hadamard, Pauli-X

**Main header modules**:

| Header | Contents |
|--------|------|
| `basic_components.h` | core data structures such as System / SparseState / BaseOperator |
| `basic_gates.h` | standard gates such as Phase / Rotation / Pauli / S / T / RX-RI-RZ / SX / U2 / U3 |
| `hadamard.h` | Hadamard on integer registers (superposition generation) |
| `qft.h` | QFT / InverseQFT / QFT_Full |
| `measurement.h` | mid-circuit measurement MeasureZ / Reset / Probability |
| `partial_trace.h` | partial trace and readout |
| `qram.h` | QRAMLoad / QRAMLoadFast / QRAMInputGenerator |
| `quantum_arithmetic.h` | about 50 quantum arithmetic operators for modular add/sub/mul/div, shifts, comparisons, etc. |
| `system_operations.h` | AddRegister / RemoveRegister / Split / Combine / Push / Pop etc. |
| `condrot.h` / `rot.h` | conditional rotation, general unitary rotation, and state preparation |
| `debugger.h` | debug operators such as CheckNormalization / CheckNan / StatePrint |

### SparQ_Algorithm/ - High-Level Algorithm Library

Composes core primitives into complete quantum algorithms; each algorithm corresponds to a C++ experiment in `Experiments/`
and a Python implementation in `PySparQ/pysparq/algorithms/` (see
[docs/algorithm-implementation.md](../algorithm-implementation.md) for the mapping):

- **grover.h**: QRAM-oracle-driven Grover search (incl. amplitude amplification and quantum counting)
- **shor.h**: Shor factoring (standard + semiclassical variants)
- **state_preparation.h**: QRAM-based state preparation
- **BlockEncoding/**: tridiagonal-matrix block encoding and QRAM-based block encoding
- **DiscreteAdiabatic/**: discrete adiabatic (QDA) linear-system solver
- **hamiltonian_simulation.h**: Hamiltonian simulation via quantum walk / LCU / sparse-matrix oracle / QSVT
- **qcnn.h**: quantum convolutional network (currently disabled as a whole by `#if false`)

### PySparQ/ - Python Bindings and Pure-Python Layer

Exposes the core C++ API through pybind11 (`PySparQ/core.cpp` → compiled into ``pysparq._core``),
organized by the pure-Python package ``pysparq``:

- ``pysparq/operators/``: operator base classes and the conditional-control mixin
- ``pysparq/algorithms/``: pure-Python algorithm layer (Grover, Shor, QDA, CKS, state preparation, block encoding)
- ``pysparq/rir.py``: the RIR interpreter (the QECC.Lang intermediate representation)
- ``pysparq/dynamic_operator/``: runtime JIT-compiled C++ dynamic operators (with a standalone loader)
- ``pysparq/conformance.py``: conformance-checking utilities

The thin ``qram_simulator`` binding does not live in this repository — it is packaged and released independently
by the QRAM-Simulator core repository (`pip install qram-simulator`).

### extern/qram-simulator/ - QRAM Base (submodule)

The QRAM circuit core (the qutrit/qubit implementations of ``QRAMCircuit``, ``CuQRAMCircuit``) and
the Common infrastructure (math utilities, matrix wrappers, random-number engines, error handling).
**This directory belongs to another repository; do not modify it directly inside this repository**;
the upgrade path is `git submodule update --remote` followed by committing the new pin.

## Quantum State Representation

```
┌──────────────────────────────────────────────────────────────┐
│                      SparseState                              │
├──────────────────────────────────────────────────────────────┤
│  Sparse storage: std::vector<System> basis_states            │
│  - Each System = { amplitude: complex, registers: uint64_t[] }│
│  - Register-value combinations are unique (amplitudes are summed and deduplicated on interference) │
│  - Only non-zero basis states with |amplitude| > ε are stored │
│  - Default construction creates the |0...0⟩ initial state    │
├──────────────────────────────────────────────────────────────┤
│  System (statically shared metadata)                         │
│  - name_register_map: [(name, type, size, status), ...]      │
│  - Registers are stored as uint64_t, supporting |a⟩|b⟩|c⟩ multi-register encoding │
└──────────────────────────────────────────────────────────────┘
```

**Key properties**:

- Memory usage: O(k × r), where k is the number of non-zero basis states and r the number of registers
- Compared with the O(2^n) of a dense representation, this enables much larger simulations
- Register-level operations: AddRegister ≈ ⊗|0⟩, RemoveRegister ≈ PartialTrace

## Key Design Decisions

### Why Sparse-State Simulation

1. **Memory efficiency**: the memory usage of the sparse representation is proportional to the amount of entanglement
2. **Computational efficiency**: gate operations only need to touch basis states with non-zero amplitudes
3. **Where it shines**: QRAM operations, quantum walks, sparse-Hamiltonian simulation

### Qutrit-based vs Qubit-based QRAM

The core difference between the two lies in **how the address is encoded**, which in turn leads to different error scaling:

- **Qutrit-based** (0/1/wait three-state encoding): the address has an off (wait) state
  - Classical information is encoded via **classical controlled X**
  - The Z basis is passed straight down, with no basis change
  - Error scaling: ε ∈ O(L²), where L is the layer number

- **Qubit-based** (0/1 two-state encoding): the address has no off state and is always on by default (routing to the left)
  - The data qubit cannot be passed down directly; data must be converted to the **Pauli-X basis**
  - Classical information is injected via **classical controlled Z**
  - Error scaling: ε ∈ O(L³), where L is the layer number

## Extensibility

### Adding a New Quantum Gate / Operator

1. Create a header file in `SparQ/include/`, deriving from `BaseOperator` (general operators) or
   `SelfAdjointOperator` (self-adjoint operators), and implement `operator()` and (if not self-adjoint) `dag()`
2. Implement it in a same-named .cpp under `SparQ/src/`; the `ClassControllable` macro can be used to gain conditional-control capability
3. To expose it to Python, add the binding in `PySparQ/core.cpp` and keep the `_core.pyi` type hints in sync

### Adding a New Algorithm

1. Create the algorithm header in `SparQ_Algorithm/include/`, composing core operators
2. (Optional) Add a C++ experiment entry in `Experiments/`, a Python implementation under `PySparQ/pysparq/algorithms/`,
   and register the mapping in `docs/algorithm-implementation.md`

### Build Options

- `SPARQ_BUILD_TESTS` / `SPARQ_BUILD_EXPERIMENTS` / `SPARQ_BUILD_EXAMPLES`: CMake gates, OFF by default
- CUDA/GPU backend: the code is kept in `SparQ/include/cuda/` and `SparQ/src/cuda/`; CMake currently masks the GPU build
