# SparQSim / pysparq

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

See the [QRAM-Simulator repository README](https://github.com/IAI-USTC-Quantum/QRAM-Simulator#论文与引用).

## About Us

This project is developed by **[IAI-USTC Quantum](https://github.com/IAI-USTC-Quantum)** (the Quantum Artificial Intelligence Team at the Institute of Artificial Intelligence, Hefei Comprehensive National Science Center).

## License

Apache-2.0 License

---

## 中文版

# SparQSim / pysparq

[![arXiv:QRAM](https://img.shields.io/badge/QRAM_Simulator-arXiv%3A2503%2E13832-b31b1b.svg)](https://arxiv.org/abs/2503.13832)
[![arXiv:SparQ](https://img.shields.io/badge/SparQ-arXiv%3A2503%2E15118-6f42c1.svg)](https://arxiv.org/abs/2503.15118)
[![PyPI](https://img.shields.io/pypi/v/pysparq.svg)](https://pypi.org/project/pysparq/)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![GitHub](https://img.shields.io/badge/GitHub-IAI--USTC--Quantum%2FSparQSim-181717?logo=github)](https://github.com/IAI-USTC-Quantum/SparQSim)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-4D6AE4)](https://iai-ustc-quantum.github.io/SparQSim/)

> **pysparq —— 稀疏态量子电路模拟器的全功能 Python 框架（Register Level Programming、原生 QRAM、动态算子、RIR 解释器）**

## 仓库分工

依赖方向：**SparQSim → QRAM-Simulator**。SparQSim 发布
[`pysparq`](https://pypi.org/project/pysparq/) 包并承载 SparQ C++ 框架
（`SparQ/` 稀疏态模拟器 + `SparQ_Algorithm/` 算法库 + 全部 Python 绑定与算法类实验）；
QRAM 基座（Common + QRAM + ThirdParty）在
[QRAM-Simulator 仓库](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)独立发版，
本仓库以 git submodule（相对 URL `../QRAM-Simulator.git`）引用并编译。

| 仓库 | 内容 | PyPI 包 |
|------|------|---------|
| **SparQSim**（本仓库） | SparQ C++ 框架 + pysparq 全功能 Python 框架（core.cpp 富绑定 + 纯 Python 算法层） | `pysparq` |
| [QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator) | 纯 C++ QRAM 基座（Common + QRAM 电路核心 + QRAM 论文实验 + pybind11 薄绑定） | `qram-simulator` |

## 安装

```bash
pip install pysparq
```

**要求**：Python 3.10 – 3.13，NumPy。

需要 QRAM 电路级 Python API（`QRAMCircuitQubit`/`QRAMCircuitQutrit` 噪声
仿真工作流）时，另行安装 [QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)
仓库发布的独立包：

```bash
pip install qram-simulator
```

**GPU 支持**：CUDA/GPU 后端默认关闭，构建时传 `-DSPARQ_ENABLE_CUDA=ON` 开启（需本机 CUDA 工具链，CUDA 13 实测通过）；PyPI wheel 恒为 CPU-only。

### 从源码构建

```bash
git clone --recurse-submodules <this-repo-url>
cd SparQSim
pip install .
```

## 5 分钟上手示例

```python
import pysparq as ps

# 1. 创建稀疏态
state = ps.SparseState()

# 2. 定义寄存器（Register Level Programming 的核心）
addr_id = ps.AddRegister("addr", ps.UnsignedInteger, 4)(state)
data_id = ps.AddRegister("data", ps.UnsignedInteger, 8)(state)

# 3. 初始化叠加态（所有地址等概率）
ps.Hadamard_Int("addr", 4)(state)

# 4. 创建 QRAM 并加载数据
memory = [i * 2 for i in range(16)]
qram = ps.QRAMCircuit_qutrit(addr_size=4, data_size=8, memory=memory)

# 5. 执行 QRAM 加载：|addr⟩|0⟩ → |addr⟩|memory[addr]⟩
ps.QRAMLoad(qram, "addr", "data")(state)

# 6. 直接进行算术操作（无需编译成门！）
ps.Add_ConstUInt_InPlace("data", 5)(state)   # data = data + 5

# 7. 测量 / 概率查询
ps.set_seed(0)
outcome, prob = ps.MeasureZ("data")(state)
dist = ps.Probability.distribution(state, "addr")
```

## Register Level 特性的关键 API

```python
# 量子算术 - 直接寄存器操作（Register Level Programming 核心）
ps.Add_UInt_UInt(in1, in2, out)      # out = in1 + in2
ps.Add_UInt_ConstUInt(reg, const)    # reg = reg + const
ps.Mult_UInt_ConstUInt(in, c, out)   # out = in * c
ps.ShiftLeft(reg, n)                 # 左移 n 位
ps.ShiftRight(reg, n)                # 右移 n 位

# 基础量子门
ps.Hadamard_Int(reg, n_digits)       # 对整数寄存器应用 Hadamard
ps.X_Bool(reg, pos)                  # X 门（特定比特位）
ps.Z_Bool(reg, pos)                  # Z 门

# QRAM 操作
ps.QRAMLoad(qram, addr_reg, data_reg)      # QRAM 加载
ps.QRAMLoadFast(qram, addr_reg, data_reg)  # 快速版本

# 可播种的测量 / 复位 / 概率查询（面向动态执行器：mid-circuit MEASURE/RESET/QIF）
ps.set_seed(seed)                          # 播种全局随机数引擎，使采样结果可复现
outcome, prob = ps.MeasureZ(reg)(state)    # 投影式 Z 基测量：坍缩 + 重新归一化
measured = ps.Reset(reg, target=0)(state)  # 测量 + 经典条件翻转，强制复位到 target
p = ps.Probability(reg, value)(state)      # 只读 Born 概率查询，不改变状态
dist = ps.Probability.distribution(state, reg)  # 单寄存器完整结果分布（只读）
```

## 高层算法库（`pysparq.algorithms`）

纯 Python 实现的算法层，组合公开原语（Grover、Shor、QDA、CKS、态制备、块编码等），
与 C++ 实验代码的逐一对应关系见 [docs/algorithm-implementation.md](docs/algorithm-implementation.md)。

## 项目结构

```
SparQSim/
├── SparQ/                  # SparQ C++ 稀疏态模拟器（伞形目标 SparQ 在根 CMake 定义）
├── SparQ_Algorithm/        # 高层算法 C++ 库（态制备、块编码、哈密顿模拟、QDA 等）
├── Experiments/            # 量子算法 C++ 实验（QDA/Grover/QFT/Shor/QCNN/CKS/GHZ 等）
├── test/                   # C++ 测试（SparQ 单测 + CommonTest 完整版；SPARQ_BUILD_TESTS 门控）
├── extern/qram-simulator/  # QRAM 基座 submodule（Common + QRAM + ThirdParty，相对 URL ../QRAM-Simulator.git）
├── PySparQ/
│   ├── core.cpp            # pybind11 富绑定（_core 模块）
│   ├── pysparq/            # Python 包（operators/ algorithms/ rir conformance dynamic_operator）
│   └── test/               # pytest 套件
├── docs/                   # Sphinx 文档 + 算法转译指南
├── examples/               # C++ 与 Python 示例（SPARQ_BUILD_EXAMPLES 门控 C++ 部分）
└── pyproject.toml          # pysparq 包（scikit-build-core + setuptools-scm）
```

## 发版流程

1. 在 Gitea（开发主仓）合并变更到 main；
2. 需要新版核心时，先等 [QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)
   发布对应 tag，然后 `git submodule update --remote`（或 checkout 到该 tag）提交 pin；
3. 同步到 GitHub 上游 `IAI-USTC-Quantum/SparQSim`；
4. 更新 `CHANGELOG.md`，打 tag `vX.Y.Z`（延续 pysparq 版本系列，下一版 v0.2.0）；
5. push tag 或创建 GitHub Release → `pypi-publish` 工作流自动构建
   cp310–313 × (manylinux / win_amd64) wheel + sdist（sdist 内嵌核心源码，自包含）
   并发布到 PyPI（Trusted Publishing / OIDC）。

外部消费者（qecc_lang、quantum_cfd QFVM）的 API 依赖冻结见
`PySparQ/consumer_runtime_inventory.json`，由 `PySparQ/test/test_consumer_runtime_contract.py` 强制。

## 论文与引用

见 [QRAM-Simulator 仓库 README](https://github.com/IAI-USTC-Quantum/QRAM-Simulator#论文与引用)。

## About Us

本项目由 **[IAI-USTC Quantum](https://github.com/IAI-USTC-Quantum)** 开发（合肥综合性国家科学中心人工智能研究院量子人工智能团队）。

## 许可证

Apache-2.0 License
