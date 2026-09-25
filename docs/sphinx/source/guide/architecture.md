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

---

## 中文版

# SparQSim 架构

本文档描述 SparQSim 仓库（SparQ 框架之家）的整体架构、仓库分工和核心模块。

## 概述

### 项目目标

SparQ 是一个用于模拟**量子随机存取存储器（QRAM）**和**稀疏态量子计算**的高性能模拟器框架。它旨在为量子算法研究者和开发者提供：

- 高效模拟大规模量子系统的工具（利用稀疏态表示）
- 精确建模 QRAM 电路的行为和噪声影响
- 寄存器级编程范式：直接对整数/布尔寄存器做算术与逻辑操作，无需手工分解到门
- 完整的算法库（Grover、Shor、态制备、块编码、哈密顿量模拟、离散绝热 QDA 等）
- 简洁的 Python API（`pysparq`）便于快速原型开发

### 仓库分工

依赖方向：**SparQSim → QRAM-Simulator**。本仓库承载 SparQ C++ 框架
（`SparQ/` 稀疏态模拟器 + `SparQ_Algorithm/` 算法库 + 全部 Python 绑定与算法类实验）；
QRAM 基座（Common + QRAM + ThirdParty）在
[QRAM-Simulator 仓库](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)独立发版，
本仓库以 git submodule（相对 URL `../QRAM-Simulator.git`）引用并编译。

## 目录结构

```
SparQSim/
├── SparQ/                  # SparQ C++ 稀疏态模拟器（伞形目标 SparQ 在根 CMake 定义）
│   ├── include/            # 公共 API 头文件（含 cuda/ GPU 后端，暂被 CMake 屏蔽）
│   └── src/                # 实现（含 src/cuda/ GPU 内核）
├── SparQ_Algorithm/        # 高层算法 C++ 库
│   ├── include/            # grover / shor / 态制备 / 块编码 / 哈密顿模拟 / QDA
│   └── src/
├── PySparQ/                # pybind11 富绑定 + 纯 Python 包
│   ├── core.cpp            # _core 模块绑定（约 1500 行）
│   ├── include/            # 绑定层辅助头（core.h、BindUtils.h）
│   ├── pysparq/            # Python 包：operators/ algorithms/ rir conformance dynamic_operator
│   └── test/               # pytest 套件（含外部消费者 API 契约测试）
├── Experiments/            # 量子算法 C++ 实验（QDA/Grover/QFT/Shor/QCNN/CKS/GHZ 等）
├── examples/               # C++ 与 Python 示例（SPARQ_BUILD_EXAMPLES 门控 C++ 部分）
├── test/                   # C++ 测试（SPARQ_BUILD_TESTS 门控）
├── extern/qram-simulator/  # QRAM 基座 submodule（Common + QRAM + ThirdParty，勿直接修改）
├── docs/                   # Sphinx 文档 + Doxygen（C++ API）+ 算法转译指南
└── pyproject.toml          # pysparq 包（scikit-build-core + setuptools-scm）
```

### SparQ/ - 稀疏态模拟器核心

**职责**：实现稀疏态量子模拟的核心功能（寄存器管理、门、测量、QRAM、量子算术）。

**核心类**（均在 `qram_simulator` 命名空间下，声明于 `SparQ/include/basic_components.h`）：

- **`System`**：单个计算基态，包含复数振幅 ``amplitude`` 和寄存器值数组 ``registers``；
  同时以静态成员维护全局寄存器表（名称、类型、位宽），是寄存器级编程的枢纽
- **`StateStorage`**：量子寄存器存储单元
- **`SparseState`**：稀疏量子态，托管 ``std::vector<System>``，默认构造创建 ``|0...0⟩`` 初态
- **`BaseOperator`**：算子的统一接口（``operator()`` / ``dag()``，含 CPU/GPU 重载），支持复合算子和条件算子
- **`SelfAdjointOperator`**：自伴算子基类（``dag() == operator()``），如 Hadamard、Pauli-X

**主要头文件模块**：

| 头文件 | 内容 |
|--------|------|
| `basic_components.h` | System / SparseState / BaseOperator 等核心数据结构 |
| `basic_gates.h` | Phase / Rotation / Pauli / S / T / RX-RI-RZ / SX / U2 / U3 等标准门 |
| `hadamard.h` | 整数寄存器 Hadamard（叠加态生成） |
| `qft.h` | QFT / InverseQFT / QFT_Full |
| `measurement.h` | 中间电路测量 MeasureZ / Reset / Probability |
| `partial_trace.h` | 部分迹与读出 |
| `qram.h` | QRAMLoad / QRAMLoadFast / QRAMInputGenerator |
| `quantum_arithmetic.h` | 加减乘除模、移位、比较等约 50 个量子算术算子 |
| `system_operations.h` | AddRegister / RemoveRegister / Split / Combine / Push / Pop 等 |
| `condrot.h` / `rot.h` | 条件旋转、一般酉旋转与态制备 |
| `debugger.h` | CheckNormalization / CheckNan / StatePrint 等调试算子 |

### SparQ_Algorithm/ - 高层算法库

组合核心原语实现完整量子算法，每个算法对应 `Experiments/` 中的 C++ 实验
与 `PySparQ/pysparq/algorithms/` 中的 Python 实现（对照关系见
[docs/algorithm-implementation.md](../algorithm-implementation.md)）：

- **grover.h**：QRAM oracle 驱动的 Grover 搜索（含振幅放大与量子计数）
- **shor.h**：Shor 因数分解（标准版 + 半经典版）
- **state_preparation.h**：基于 QRAM 的态制备
- **BlockEncoding/**：三对角矩阵块编码、基于 QRAM 的块编码
- **DiscreteAdiabatic/**：离散绝热（QDA）线性方程组求解器
- **hamiltonian_simulation.h**：量子行走 / LCU / 稀疏矩阵 oracle / QSVT 哈密顿量模拟
- **qcnn.h**：量子卷积网络（当前被 `#if false` 整体禁用）

### PySparQ/ - Python 绑定与纯 Python 层

通过 pybind11（`PySparQ/core.cpp` → 编译为 ``pysparq._core``）暴露核心 C++ API，
再由纯 Python 包 ``pysparq`` 组织：

- ``pysparq/operators/``：算子基类与条件控制 mixin
- ``pysparq/algorithms/``：纯 Python 算法层（Grover、Shor、QDA、CKS、态制备、块编码）
- ``pysparq/rir.py``：RIR 解释器（QECC.Lang 中间表示）
- ``pysparq/dynamic_operator/``：运行时 JIT 编译 C++ 动态算子（含独立的 loader）
- ``pysparq/conformance.py``：一致性校验工具

``qram_simulator`` 薄绑定不在本仓库——它由 QRAM-Simulator 核心仓独立打包发布
（`pip install qram-simulator`）。

### extern/qram-simulator/ - QRAM 基座（submodule）

QRAM 电路核心（``QRAMCircuit`` 的 qutrit/qubit 实现、``CuQRAMCircuit``）与
Common 基础设施（数学工具、矩阵封装、随机数引擎、错误处理）。
**此目录属于另一仓库，不要在本仓库内直接修改**；升级方式是
`git submodule update --remote` 后提交新 pin。

## 量子态表示

```
┌──────────────────────────────────────────────────────────────┐
│                      SparseState                              │
├──────────────────────────────────────────────────────────────┤
│  稀疏存储: std::vector<System> basis_states                  │
│  - 每个 System = { amplitude: complex, registers: uint64_t[] }│
│  - 寄存器值组合唯一（干涉时振幅相加去重）                    │
│  - 仅存储 |amplitude| > ε 的非零基态                         │
│  - 默认构造创建 |0...0⟩ 初态                                 │
├──────────────────────────────────────────────────────────────┤
│  System（静态共享元数据）                                     │
│  - name_register_map: [(name, type, size, status), ...]      │
│  - 寄存器以 uint64_t 存储，支持 |a⟩|b⟩|c⟩ 多寄存器编码     │
└──────────────────────────────────────────────────────────────┘
```

**关键特性**：

- 内存使用：O(k × r)，k 为非零基态数，r 为寄存器数
- 对比稠密表示的 O(2^n)，可实现更大规模的模拟
- 寄存器级操作：AddRegister ≈ ⊗|0⟩，RemoveRegister ≈ PartialTrace

## 关键设计决策

### 为什么使用稀疏态模拟

1. **内存效率**：稀疏表示内存使用与纠缠程度成正比
2. **计算效率**：门操作只需处理非零振幅的基态
3. **适用场景**：QRAM 操作、量子漫步、稀疏哈密顿量模拟

### Qutrit-based vs Qubit-based QRAM

两者的核心区别在于 **address 编码方式**，进而导致不同的 error scaling：

- **Qutrit-based**（0/1/wait 三态编码）：address 存在关闭（wait）状态
  - 经典信息以 **classical controlled X** 方式编码
  - Z-basis 直接传下去，无需基变换
  - Error scaling: ε ∈ O(L²)，L 为 layer number

- **Qubit-based**（0/1 两态编码）：address 无关闭状态，默认常开（向左 routing）
  - 无法直接将 data qubit 传下去，需要将 data 换到 **Pauli-X basis**
  - 经典信息以 **classical controlled Z** 方式传入
  - Error scaling: ε ∈ O(L³)，L 为 layer number

## 扩展性

### 添加新量子门 / 算子

1. 在 `SparQ/include/` 创建头文件，继承 `BaseOperator`（一般算子）或
   `SelfAdjointOperator`（自伴算子），实现 `operator()` 与（如非自伴）`dag()`
2. 在 `SparQ/src/` 同名 .cpp 实现；可利用 `ClassControllable` 宏获得条件控制能力
3. 需要暴露给 Python 时，在 `PySparQ/core.cpp` 添加绑定，并同步 `_core.pyi` 类型提示

### 添加新算法

1. 在 `SparQ_Algorithm/include/` 创建算法头文件，组合核心算子实现
2. （可选）在 `Experiments/` 添加 C++ 实验入口，在 `PySparQ/pysparq/algorithms/`
   添加 Python 实现，并在 `docs/algorithm-implementation.md` 登记对照关系

### 构建选项

- `SPARQ_BUILD_TESTS` / `SPARQ_BUILD_EXPERIMENTS` / `SPARQ_BUILD_EXAMPLES`：CMake 门控，默认 OFF
- CUDA/GPU 后端：代码保留在 `SparQ/include/cuda/` 与 `SparQ/src/cuda/`，当前 CMake 暂时屏蔽 GPU 构建
