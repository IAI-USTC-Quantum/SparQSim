<p align="center">
  <img src="banner.svg" alt="SparQSim Banner" width="100%">
</p>

# SparQSim / pysparq

[English](README.md) | 简体中文

[![arXiv:QRAM](https://img.shields.io/badge/QRAM_Simulator-arXiv%3A2503%2E13832-b31b1b.svg)](https://arxiv.org/abs/2503.13832)
[![arXiv:SparQ](https://img.shields.io/badge/SparQ-arXiv%3A2503%2E15118-6f42c1.svg)](https://arxiv.org/abs/2503.15118)
[![PyPI](https://img.shields.io/pypi/v/pysparq.svg)](https://pypi.org/project/pysparq/)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![GitHub](https://img.shields.io/badge/GitHub-IAI--USTC--Quantum%2FSparQSim-181717?logo=github)](https://github.com/IAI-USTC-Quantum/SparQSim)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-4D6AE4)](https://iai-ustc-quantum.github.io/SparQSim/)

> **pysparq —— 稀疏态量子电路模拟器的全功能 Python 框架（Register Level Programming、原生 QRAM、动态算子、RIR 解释器）**

## 亮点

- **稀疏态量子模拟 + Register Level Programming**——直接寄存器级算术、原生 QRAM、动态算子与 RIR 解释器（见下方快速上手）
- **量子线性方程组求解，双引擎**——离散绝热 **QDA** 与 Chebyshev 量子行走 **CKS** 求解器，在单张 A800 上完成最高 **2^20 维**的端到端验证（[v1d](docs/verification/v1d-qda-linsolver-a800.md)、[v1e](docs/verification/v1e-cks-direct-oracle-linsolver.md)）
- **CPU（OpenMP）与 CUDA 双后端**，内核逐位交叉验证（[v1c](docs/verification/v1c-gpu-cuda-a800.md)）

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

**GPU 支持**：CUDA/GPU 后端默认关闭，构建时传 `-DSPARQ_ENABLE_CUDA=ON` 开启（需本机 CUDA 工具链；CUDA 13 / CCCL 3 与 8×A800 Linux 服务器上的 CUDA 12.8 / sm_80 均实测通过，详见 [`docs/verification/v1c-gpu-cuda-a800.md`](docs/verification/v1c-gpu-cuda-a800.md)；该后端上的 2^20 CKS/QDA 线性求解器生产记录见 [v1e](docs/verification/v1e-cks-direct-oracle-linsolver.md) 与 [v1d](docs/verification/v1d-qda-linsolver-a800.md)）；PyPI wheel 恒为 CPU-only。

**MPI**：SparQSim 的 C++ 核心本身不含 MPI 路径；qram-simulator 绑定侧的轨迹级 MPI 系综并行（单机多进程，结果与进程数无关）由 [QRAM-Simulator 仓库](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)的 `qram_simulator.mpi` 提供——见其「MPI 系综并行」一节。

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

## 量子线性方程组 bench（CKS / QDA，2^20 维已验证）

C++ 层提供 Poiseuille 三对角线性方程组的生产基准
（`A = tridiag(-1, α, -1)`、`b = ones(n)/sqrt(n)`、κ = 16），
两个引擎的 CLI 与产物 schema 完全一致（`residual_curve.csv` + `qda_summary_cpp.json`）：

```bash
# QDA（离散绝热）——2^20 下默认 --czEps 会剪掉解分支，
# 需按维度/调度自适应设置（见下）
./Experiment_PoiseuilleQDABench_cuda --nqubit 20 --kappa 16 --steprate 1.1 \
    --tol 1e-6 --logEvery 250 --czEps 1e-20 --outdir qda_2p20

# CKS（Chebyshev 量子行走，QRAM-free 直接算术 Oracle）
./Experiment_CKS_PoiseuilleBench_cuda --nqubit 20 --kappa 16 --eps 0.01 \
    --ds 32 --coef true --J 1024 --logEvery 64 --tol 1e-6 --outdir cks_2p20
```

已验证成绩（单张 A800，CUDA 稀疏态后端）：

- **CKS 2^20**（QRAM-free 直接 Oracle，构建参数 `-DCACHED_REGISTER_SIZE=16`）：
  方向残差 `0 ≤ 1e-6`（640/1024 项提前终止）、保真度 `1.0000000000`、
  成功概率 `4.75e-2`、20,971,512 个 walk 基态、2682 s；dim16/dim64 成功概率与
  CPU 参考逐位一致——[v1e](docs/verification/v1e-cks-direct-oracle-linsolver.md)。
- **QDA dim256**：全程 40568 步调度收敛到 `res_dir = 5.9485e-4`
  （保真度 `1.000000`、成功概率 `1.0`）；2^20 生产复跑进行中，
  验证记录持续更新——[v1d](docs/verification/v1d-qda-linsolver-a800.md)。

### `--czEps` 按维度与调度自适应（QDA）

`ClearZero` 按 |振幅|² < eps 裁剪基态。QDA 解分支出生时单态幅值 ≈ sqrt(2e-6/n)，
且随调度细化（ds = 1/步数）线性缩小，因此经验规则是
**让 sqrt(czEps) 低于解分支出生幅值**：

| 调度 | 可用 `--czEps` |
|------|----------------|
| 粗调度（`--steprate 0.1`） | `1e-16` |
| 细调度（`--steprate 1.1`，生产） | `1e-20` ~ `1e-22` |

在库默认 `1e-14`（幅值断点 1e-7）下，解分支从 2^19 维起在出生时即被剪掉——
实测断崖：2^17（2.9e-7）、2^18（1.5e-7）存活，2^19（7.4e-8）、2^20（3.7e-8）被剪，
这正是首次 2^20 运行提取全 NaN 的根因。commit `10a96aa` 通过 `--czEps`
使阈值可按维度自适应（默认行为逐位不变）；完整分析见
[v1d 补篇](docs/verification/v1d-qda-linsolver-a800.md)。

## 文档

完整文档站托管在 GitHub Pages：https://iai-ustc-quantum.github.io/SparQSim/

- [快速入门](https://iai-ustc-quantum.github.io/SparQSim/zh/guide/quickstart.html)与[核心概念](https://iai-ustc-quantum.github.io/SparQSim/zh/guide/core_concepts/index.html)
- [算子参考](https://iai-ustc-quantum.github.io/SparQSim/zh/operators/index.html)——寄存器级算子完整目录
- [算法指南](https://iai-ustc-quantum.github.io/SparQSim/zh/guide/algorithms/index.html)——Grover / Shor / QDA / CKS / 块编码
- [交互式教程](https://iai-ustc-quantum.github.io/SparQSim/zh/notebooks/01_quickstart.html)——可运行 notebook
- English site: [English documentation](https://iai-ustc-quantum.github.io/SparQSim/en/index.html)

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

见 [QRAM-Simulator 仓库 README](https://github.com/IAI-USTC-Quantum/QRAM-Simulator#papers-and-citation)。

## About Us

本项目由 **[IAI-USTC Quantum](https://github.com/IAI-USTC-Quantum)** 开发（合肥综合性国家科学中心人工智能研究院量子人工智能团队）。

## 许可证

Apache-2.0 License
