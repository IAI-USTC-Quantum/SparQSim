# Changelog

All notable changes to pysparq (SparQSim repository) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **仓库拆分说明**：本仓库由原 QRAM-Simulator monorepo 拆分而来（git 历史经
> filter-repo 完整保留，含 v0.1.0 / v0.1.1 标签）。拆分前的完整变更历史见
> [QRAM-Simulator 仓库 CHANGELOG](https://github.com/IAI-USTC-Quantum/QRAM-Simulator/blob/main/CHANGELOG.md)。

---

## [Unreleased]

### Changed
- **第二轮分仓：SparQ C++ 框架整体迁入本仓库**。`SparQ/`（稀疏态模拟器）、
  `SparQ_Algorithm/`（算法库）、`bindings/python/`（`qram_simulator` 薄绑定，
  自核心仓迁入）、`examples/` C++ 示例、算法系实验（QDA/Grover/
  StatePreparation/QCNN/QFT/CKS/Shor/GHZ/ErrorFiltration/GPUTime）与依赖
  SparQ 算子的 QRAM 实验（QRAMFidelity v1、QRAM_Qubit）全部迁至本仓库；
  CommonTest 完整版（含算法块）亦随迁。依赖方向固化为
  **SparQSim → QRAM-Simulator**（基座仓只保留 Common + QRAM + QRAM 实验）
- 伞形 `SparQ` CMake 目标改在**根 CMakeLists** 定义（interface 聚合：
  SparQ_Algorithm + SparQ_Simulator + submodule 的 SparQ_QRAMSimulator +
  SparQ_Common + fmt），原定义位于 monorepo 的 SparQ_Algorithm/src
- 新增开发开关 `SPARQ_BUILD_TESTS` / `SPARQ_BUILD_EXPERIMENTS` /
  `SPARQ_BUILD_EXAMPLES`（默认 OFF；googletest 取自核心 submodule 的
  ThirdParty）
- 根 CMakeLists 安装 `SparQ/include`、`SparQ_Algorithm/include` 到平铺
  `include/`（wheel 的 JIT 头文件布局）
- `dynamic_operator/compiler.py` JIT include 路径适配：SparQ 头在仓库本地，
  QRAM/Common/ThirdParty 仍在 `extern/qram-simulator/` 下
- sdist 增补 `SparQ/*`、`SparQ_Algorithm/*` 并排除开发目录（Experiments/
  test/examples/docs 等）
- 薄绑定模块 target 更名 `qram_simulator_core`（避免与 pysparq `_core`
  冲突），输出文件名保持 `_core.pyd`；`qram-simulator` wheel 的发布为后续工作

### 第一轮：从 QRAM-Simulator monorepo 拆分为独立仓库 SparQSim
- **pysparq 包（PySparQ 富绑定 + 纯 Python 框架）整体迁入**；C++ 核心迁至
  QRAM-Simulator 仓库，本仓库经 git submodule
  （相对 URL `../QRAM-Simulator.git`，路径 `extern/qram-simulator`）引用并编译
- 根 CMakeLists 新建：`add_subdirectory(extern/qram-simulator)`（核心的
  tests/experiments/薄绑定一律关闭）+ `add_subdirectory(PySparQ)`，
  scikit-build-core 驱动，构建方式与原 monorepo 一致
- `dynamic_operator/compiler.py` 源码树根探测改为 `extern/qram-simulator/`
  布局（wheel 安装布局不变：`include/` 与 `pysparq/` 同级）
- sdist 内嵌 `extern/qram-simulator` 源码（`sdist.include`），满足 PyPI
  sdist 自包含要求

### Removed
- `.cibuildwheel-hooks/`（cibuildwheel 2.x 不支持 Linux after-build，
  post-hook/inject_stubs 早已成死代码；stub 现为仓库内提交的
  `PySparQ/pysparq/_core.pyi`，由 pre-commit 校验）

### Fixed
- 补声明 `typing_extensions`（Python 3.10）依赖
- 移除孤儿构建目标 `PySparQ/src/`（重复的 `_core` 定义与 `QDAAlgo`）
