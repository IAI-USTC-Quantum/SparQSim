# SparQSim 架构

本文档描述 SparQSim 仓库（SparQ 框架之家）的整体架构、仓库分工和核心模块。动手入门请先阅读 {doc}`快速入门 </guide/quickstart>`。

## 概述

### 项目目标

SparQ 是一个用于模拟**量子随机存取存储器（{doc}`QRAM </operators/qram_ops>`）**和**{doc}`稀疏态 <../guide/core_concepts/sparse_state>` 量子计算**的高性能模拟器框架。它旨在为量子算法研究者和开发者提供：

- 高效模拟大规模量子系统的工具（利用稀疏态表示）
- 精确建模 QRAM 电路的行为和噪声影响
- {doc}`寄存器级编程范式 </guide/core_concepts/index>`：直接对整数/布尔寄存器做算术与逻辑操作，无需手工分解到门
- 完整的 {doc}`算法库 </cpp_api/algorithms>`（Grover、Shor、态制备、{doc}`块编码 </cpp_api/block_encoding>`、哈密顿量模拟、{doc}`离散绝热 QDA </cpp_api/qda>` 等）
- 简洁的 Python API（`pysparq`，见 {doc}`API 参考 </api/index>`）便于快速原型开发

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

- **{doc}`System </guide/core_concepts/system>`**：单个计算基态，包含复数振幅 ``amplitude`` 和寄存器值数组 ``registers``；
  同时以静态成员维护全局寄存器表（名称、类型、位宽），是寄存器级编程的枢纽
- **{doc}`StateStorage </guide/core_concepts/register_types>`**：量子寄存器存储单元
- **{doc}`SparseState </guide/core_concepts/sparse_state>`**：稀疏量子态，托管 ``std::vector<System>``，默认构造创建 ``|0...0⟩`` 初态
- **{doc}`BaseOperator </operators/index>`**：算子的统一接口（``operator()`` / ``dag()``，含 CPU/GPU 重载），支持复合算子和条件算子
- **{doc}`SelfAdjointOperator </operators/index>`**：自伴算子基类（``dag() == operator()``），如 Hadamard、Pauli-X

**主要头文件模块**：

| 头文件 | 内容 |
|--------|------|
| {doc}`basic_components.h </cpp_api/core>` | System / SparseState / BaseOperator 等核心数据结构 |
| {doc}`basic_gates.h </cpp_api/core>` | Phase / Rotation / Pauli / S / T / RX-RI-RZ / SX / U2 / U3 等标准门 |
| {doc}`hadamard.h </cpp_api/core>` | 整数寄存器 Hadamard（叠加态生成） |
| {doc}`qft.h </cpp_api/core>` | QFT / InverseQFT / QFT_Full |
| {doc}`measurement.h </cpp_api/measurement>` | 中间电路测量 MeasureZ / Reset / Probability |
| {doc}`partial_trace.h </cpp_api/measurement>` | 部分迹与读出 |
| {doc}`qram.h </cpp_api/qram>` | QRAMLoad / QRAMLoadFast / QRAMInputGenerator |
| {doc}`quantum_arithmetic.h </cpp_api/arithmetic>` | 加减乘除模、移位、比较等约 50 个量子算术算子 |
| {doc}`system_operations.h </cpp_api/system_ops>` | AddRegister / RemoveRegister / Split / Combine / Push / Pop 等 |
| {doc}`condrot.h </cpp_api/core>` / {doc}`rot.h </cpp_api/core>` | 条件旋转、一般酉旋转与态制备 |
| {doc}`debugger.h </cpp_api/system_ops>` | CheckNormalization / CheckNan / StatePrint 等调试算子 |

### SparQ_Algorithm/ - 高层算法库

组合核心原语实现完整量子算法，每个算法对应 `Experiments/` 中的 C++ 实验
与 `PySparQ/pysparq/algorithms/` 中的 Python 实现（对照关系见
[docs/algorithm-implementation.md](https://github.com/IAI-USTC-Quantum/SparQSim/blob/main/docs/algorithm-implementation.md)）：

- **{doc}`grover.h </cpp_api/algorithms>`**：QRAM oracle 驱动的 Grover 搜索（含振幅放大与量子计数）
- **{doc}`shor.h </cpp_api/algorithms>`**：Shor 因数分解（标准版 + 半经典版）
- **{doc}`state_preparation.h </cpp_api/algorithms>`**：基于 QRAM 的态制备
- **{doc}`BlockEncoding/ </cpp_api/block_encoding>`**：三对角矩阵块编码、基于 QRAM 的块编码
- **{doc}`DiscreteAdiabatic/ </cpp_api/qda>`**：离散绝热（QDA）线性方程组求解器
- **{doc}`hamiltonian_simulation.h </cpp_api/algorithms>`**：量子行走 / LCU / 稀疏矩阵 oracle / QSVT 哈密顿量模拟
- **qcnn.h**：量子卷积网络（当前被 `#if false` 整体禁用）

### PySparQ/ - Python 绑定与纯 Python 层

通过 pybind11（`PySparQ/core.cpp` → 编译为 ``pysparq._core``）暴露核心 C++ API，
再由纯 Python 包 ``pysparq`` 组织：

- ``pysparq/operators/``：算子基类与条件控制 mixin（{ref}`条件执行 <conditional-operations>`）
- ``pysparq/algorithms/``：纯 Python 算法层（Grover、Shor、QDA、CKS、态制备、块编码）——用于 {doc}`示例 </guide/examples>`
- ``pysparq/rir.py``：{doc}`RIR 解释执行 </guide/rir>`（QECC.Lang 中间表示）
- ``pysparq/dynamic_operator/``：运行时 JIT 编译 C++ {doc}`动态算子 </guide/dynamic_operators>`（含独立的 loader）
- ``pysparq/conformance.py``：一致性校验工具

``qram_simulator`` 薄绑定不在本仓库——它由 QRAM-Simulator 核心仓独立打包发布
（`pip install qram-simulator`）。

### extern/qram-simulator/ - QRAM 基座（submodule）

QRAM 电路核心（{doc}`QRAM 算子 </operators/qram_ops>` ``QRAMCircuit`` 的 qutrit/qubit 实现、``CuQRAMCircuit``）与
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
- 寄存器级操作：AddRegister ≈ ⊗|0⟩，RemoveRegister ≈ {doc}`部分迹 </operators/partial_trace>`

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

1. 在 `SparQ/include/` 创建头文件，继承 {doc}`BaseOperator </operators/index>`（一般算子）或
   {doc}`SelfAdjointOperator </operators/index>`（自伴算子），实现 `operator()` 与（如非自伴）`dag()`
2. 在 `SparQ/src/` 同名 .cpp 实现；可利用 `ClassControllable` 宏获得条件控制能力
3. 需要暴露给 Python 时，在 `PySparQ/core.cpp` 添加绑定（见 {doc}`绑定层 </cpp_api/bindings>`），并同步 `_core.pyi` 类型提示

### 添加新算法

1. 在 `SparQ_Algorithm/include/` 创建算法头文件，组合核心算子实现
2. （可选）在 `Experiments/` 添加 C++ 实验入口，在 `PySparQ/pysparq/algorithms/`
   添加 Python 实现，并在 `docs/algorithm-implementation.md` 登记对照关系

### 构建选项

- `SPARQ_BUILD_TESTS` / `SPARQ_BUILD_EXPERIMENTS` / `SPARQ_BUILD_EXAMPLES`：CMake 门控，默认 OFF
- CUDA/GPU 后端：代码保留在 `SparQ/include/cuda/` 与 `SparQ/src/cuda/`（见 {doc}`CUDA 后端 </cpp_api/cuda>` 参考）；当前 CMake 暂时屏蔽 GPU 构建
