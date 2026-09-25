# Changelog

All notable changes to pysparq (SparQSim repository) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **仓库拆分说明**：本仓库由原 QRAM-Simulator monorepo 拆分而来（git 历史经
> filter-repo 完整保留，含 v0.1.0 / v0.1.1 标签）。拆分前的完整变更历史见
> [QRAM-Simulator 仓库 CHANGELOG](https://github.com/IAI-USTC-Quantum/QRAM-Simulator/blob/main/CHANGELOG.md)。

---

## [Unreleased]

### Added
- **`LICENSE`**：补齐 Apache-2.0 全文（pyproject 与 README 此前已声明该许可，
  GitHub 许可检测由此生效）
- **全库中文 Doxygen 注释补齐**：`SparQ_Algorithm/include/` 全部 12 个头文件
  （grover/shor/qcnn/state_preparation/block_encoding/hamiltonian_simulation
  + BlockEncoding/×3 + DiscreteAdiabatic/×3）补齐文件 banner 与类/成员/函数
  docstring（@brief/@param/@return/@note，含数学语义与寄存器约定）；
  `SparQ/include/cuda/` 4 个 .cuh 补齐；PySparQ 绑定层
  （core.h/BindUtils.h/core.cpp）补齐；全部 .cpp/.cu 实现文件补文件级 banner；
  shor.h 补缺失的 `#pragma once`；修复 qda_fundamental.h 分割线注释的嵌套
  `/*` 问题与 rot.h 两处 `@param vec` 参数名不符
- **Sphinx 文档站重写为 SparQ 框架整体**并集成 C++ API：
  - 新增 `docs/doxygen/Doxyfile`（抽取 SparQ/SparQ_Algorithm/PySparQ 头文件
    注释生成 XML），经 Breathe 渲染进 Sphinx（新增 `cpp_api/` 11 个页面，
    按模块组织 C++ API 参考）
  - `conf.py`：project 改为 SparQ、版本改为动态读取（importlib.metadata +
    回退 dev）、source_repository 指向 SparQSim、移除失效 templates_path
  - `index.rst` badges/快速链接更新到 SparQSim 仓库与新 Pages 站点；
    `guide/architecture.md` 按拆分后布局重写（原描述已过时）
  - 构建链：requirements.txt 加 breathe 与 numpy；Makefile html 目标前置
    doxygen 步骤；docs.yml CI 安装 doxygen、触发路径加 `SparQ/**`、
    `SparQ_Algorithm/**`
  - README 增补文档站 badge
- 本地全量验证：Doxygen 零警告；Sphinx 构建成功（C++/Python API 与中文
  渲染抽查通过）

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
- **旧代薄绑定 `bindings/python/` 整体移除**（发版口径最终确定：
  `qram-simulator` 包由 QRAM-Simulator 核心仓独立构建发布，本仓库只发布
  `pysparq`）。该目录是分仓初期"从本仓发第二包"旧方案的遗留，其
  pybind11 模块输出名与 PySparQ 富绑定同为 `_core`，构建时互相覆盖，
  导致 wheel 中 `pysparq._core` 被薄绑定顶替、`import pysparq` 报
  `SparseMatrix` ImportError，且 wheel 内多出残缺的 `qram_simulator`
  顶层包（与独立发布的 `qram-simulator` PyPI 包安装冲突）——移除后
  wheel 收敛为单一 `pysparq` 包，import 与测试恢复
- `.cibuildwheel-hooks/`（cibuildwheel 2.x 不支持 Linux after-build，
  post-hook/inject_stubs 早已成死代码；stub 现为仓库内提交的
  `PySparQ/pysparq/_core.pyi`，由 pre-commit 校验）

### Fixed
- **sdist exclude 模式锚定根目录**（前导 `/`）：gitignore 风格的不带斜杠
  模式匹配任意层级，`test` 曾误伤 `PySparQ/pysparq/test`（0.1.1 起随包
  发布的测试支撑模块），致 wheel 缺 `pysparq.test`、QDA 集成测试收集失败
- **关闭 pybind11 默认 lto**（显式 `CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF`）：
  MSVC 链接期代码生成在 `_core`（单编译单元吸入全部 SparQ 头）上间歇性
  触发编译器内部错误 C1001，牺牲少量链接期优化换取可构建性
- JIT 测试（dynamic_operator / doc_examples）在无 g++ 环境整模块跳过
  而非逐用例报错
- 补声明 `typing_extensions`（Python 3.10）依赖
- 移除孤儿构建目标 `PySparQ/src/`（重复的 `_core` 定义与 `QDAAlgo`）
