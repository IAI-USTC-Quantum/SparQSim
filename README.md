# SparQSim / pysparq

[![arXiv:QRAM](https://img.shields.io/badge/QRAM_Simulator-arXiv%3A2503%2E13832-b31b1b.svg)](https://arxiv.org/abs/2503.13832)
[![arXiv:SparQ](https://img.shields.io/badge/SparQ-arXiv%3A2503%2E15118-6f42c1.svg)](https://arxiv.org/abs/2503.15118)
[![PyPI](https://img.shields.io/pypi/v/pysparq.svg)](https://pypi.org/project/pysparq/)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![GitHub](https://img.shields.io/badge/GitHub-IAI--USTC--Quantum%2FSparQSim-181717?logo=github)](https://github.com/IAI-USTC-Quantum/SparQSim)

> **pysparq —— 稀疏态量子电路模拟器的全功能 Python 框架（Register Level Programming、原生 QRAM、动态算子、RIR 解释器）**

## 仓库分工

SparQSim 仓库发布 [`pysparq`](https://pypi.org/project/pysparq/) 包。C++ 核心在
[QRAM-Simulator 仓库](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)独立发版
（其自带一个最小化的 `qram_simulator` 薄绑定）；本仓库以 git submodule
（相对 URL `../QRAM-Simulator.git`）引用并编译核心。

| 仓库 | 内容 | PyPI 包 |
|------|------|---------|
| **SparQSim**（本仓库） | pysparq 全功能 Python 框架（core.cpp 富绑定 + 纯 Python 算法层） | `pysparq` |
| [QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator) | C++ 稀疏态模拟器核心 + 薄绑定 | `qram-simulator` |

## 安装

```bash
pip install pysparq
```

**要求**：Python 3.10 – 3.13，NumPy。

**GPU 支持**：CUDA/GPU 后端当前在 CMake 中临时屏蔽，默认只构建 CPU 路径。

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
├── extern/qram-simulator/   # C++ 核心 submodule（相对 URL ../QRAM-Simulator.git）
├── PySparQ/
│   ├── core.cpp             # pybind11 富绑定（_core 模块）
│   ├── pysparq/             # Python 包（operators/ algorithms/ rir conformance dynamic_operator）
│   └── test/                # pytest 套件
├── docs/                    # Sphinx 文档 + 算法转译指南
├── examples/                # Python 示例
└── pyproject.toml           # pysparq 包（scikit-build-core + setuptools-scm）
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
