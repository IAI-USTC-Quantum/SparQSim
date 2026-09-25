# Changelog

All notable changes to pysparq (SparQSim repository) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Repository split note**: this repository was split out of the former QRAM-Simulator
> monorepo (git history fully preserved via filter-repo, including the v0.1.0 / v0.1.1
> tags). For the complete change history prior to the split, see the
> [QRAM-Simulator repository CHANGELOG](https://github.com/IAI-USTC-Quantum/QRAM-Simulator/blob/main/CHANGELOG.md).

---

## [Unreleased]

## [0.2.1] - 2026-09-25

### Fixed
- **Co-installation compatibility with the `qram-simulator` package**: the
  `QRAMCircuit_qutrit` binding now uses `py::module_local()` — the C++ type
  `qram_qutrit::QRAMCircuit` is also registered by the qram-simulator package's
  thin bindings (Python name `QRAMCircuitQutrit`), and pybind11's global
  registration keyed on the C++ typeid caused
  `generic_type: type "QRAMCircuit_qutrit" is already registered` when both
  packages were imported into the same process. With the localized registration
  the two packages can coexist (verified for both import orders); instances of
  this type are only created/passed inside the pysparq module (as QRAMLoad
  arguments) and never flow across modules.

## [0.2.0] - 2026-09-25

### Added
- **`LICENSE`**: added the full Apache-2.0 text (pyproject and README had
  already declared the license, so GitHub license detection now takes effect)
- **Library-wide completion of Chinese Doxygen comments**: all 12 headers under
  `SparQ_Algorithm/include/`
  (grover/shor/qcnn/state_preparation/block_encoding/hamiltonian_simulation
  + BlockEncoding/×3 + DiscreteAdiabatic/×3) received file banners and
  class/member/function docstrings (@brief/@param/@return/@note, covering
  mathematical semantics and register conventions); the 4 .cuh files under
  `SparQ/include/cuda/` were completed; the PySparQ binding layer
  (core.h/BindUtils.h/core.cpp) was completed; every .cpp/.cu implementation
  file received a file-level banner; shor.h gained the missing `#pragma once`;
  fixed the nested `/*` problem in a separator comment of qda_fundamental.h
  and two `@param vec` parameter-name mismatches in rot.h
- **Sphinx documentation site rewritten around the SparQ framework as a whole
  and integrated with the C++ API**:
  - Added `docs/doxygen/Doxyfile` (extracts doc comments from the
    SparQ/SparQ_Algorithm/PySparQ headers into XML), rendered into Sphinx via
    Breathe (11 new `cpp_api/` pages organizing the C++ API reference by
    module)
  - `conf.py`: project renamed to SparQ, version now read dynamically
    (importlib.metadata + dev fallback), source_repository points to SparQSim,
    stale templates_path removed
  - `index.rst` badges/quick links updated to the SparQSim repository and the
    new Pages site; `guide/architecture.md` rewritten for the post-split
    layout (the previous description was outdated)
  - Build chain: requirements.txt gained breathe and numpy; the Makefile html
    target gained a prerequisite doxygen step; docs.yml CI installs doxygen
    and its trigger paths gained `SparQ/**`, `SparQ_Algorithm/**`
  - README gained a documentation-site badge
- Full local verification: zero Doxygen warnings; Sphinx build succeeds
  (spot checks of the C++/Python API and Chinese rendering passed)

### Changed
- **Second split round: the SparQ C++ framework moved into this repository as
  a whole**. `SparQ/` (sparse-state simulator), `SparQ_Algorithm/` (algorithm
  library), `bindings/python/` (`qram_simulator` thin bindings, moved in from
  the core repository), the `examples/` C++ examples, the algorithm-class
  experiments (QDA/Grover/
  StatePreparation/QCNN/QFT/CKS/Shor/GHZ/ErrorFiltration/GPUTime) and the QRAM
  experiments that depend on SparQ operators (QRAMFidelity v1, QRAM_Qubit)
  all moved into this repository; the full CommonTest suite (including the
  algorithm blocks) moved as well. The dependency direction is now fixed as
  **SparQSim → QRAM-Simulator** (the base repository keeps only Common + QRAM
  + QRAM experiments)
- The umbrella `SparQ` CMake target is now defined in the **root CMakeLists**
  (interface aggregation: SparQ_Algorithm + SparQ_Simulator + the submodule's
  SparQ_QRAMSimulator + SparQ_Common + fmt); it was previously defined in the
  monorepo's SparQ_Algorithm/src
- Added the development switches `SPARQ_BUILD_TESTS` / `SPARQ_BUILD_EXPERIMENTS` /
  `SPARQ_BUILD_EXAMPLES` (default OFF; googletest is taken from the core
  submodule's ThirdParty)
- The root CMakeLists installs `SparQ/include` and `SparQ_Algorithm/include`
  into a flat `include/` (the wheel's JIT header layout)
- `dynamic_operator/compiler.py` JIT include paths adapted: SparQ headers are
  now repository-local, while QRAM/Common/ThirdParty remain under
  `extern/qram-simulator/`
- sdist gained `SparQ/*` and `SparQ_Algorithm/*` and excludes development
  directories (Experiments/, test/, examples/, docs/, etc.)
- The thin-binding module target was renamed to `qram_simulator_core` (to
  avoid clashing with pysparq's `_core`); the output file name stays
  `_core.pyd`; releasing the `qram-simulator` wheel is left for later

### Round One: splitting from the QRAM-Simulator monorepo into the standalone SparQSim repository
- **The pysparq package (PySparQ rich bindings + pure-Python framework) moved
  in as a whole**; the C++ core moved to the QRAM-Simulator repository, which
  this repository references and compiles via a git submodule (relative URL
  `../QRAM-Simulator.git`, path `extern/qram-simulator`)
- New root CMakeLists: `add_subdirectory(extern/qram-simulator)` (the core's
  tests/experiments/thin bindings all disabled) + `add_subdirectory(PySparQ)`,
  driven by scikit-build-core, with the build behaving the same as in the
  original monorepo
- `dynamic_operator/compiler.py` source-tree root detection adapted to the
  `extern/qram-simulator/` layout (the wheel install layout is unchanged:
  `include/` sits next to `pysparq/`)
- sdist embeds the `extern/qram-simulator` source (`sdist.include`),
  satisfying PyPI's self-contained sdist requirement

### Removed
- **The legacy thin bindings `bindings/python/` were removed entirely** (the
  release story is now final: the `qram-simulator` package is built and
  released independently from the QRAM-Simulator core repository, and this
  repository releases only `pysparq`). That directory was a leftover of the
  old early-split-era plan to "publish a second package from this
  repository"; its pybind11 module output name was also `_core`, identical to
  the PySparQ rich bindings, so the two overwrote each other at build time —
  the wheel ended up with `pysparq._core` replaced by the thin bindings,
  `import pysparq` raised a `SparseMatrix` ImportError, and the wheel carried
  a broken extra top-level `qram_simulator` package (installing it conflicts
  with the independently released `qram-simulator` PyPI package) — after the
  removal the wheel converged to the single `pysparq` package and imports and
  tests recovered
- `.cibuildwheel-hooks/` (cibuildwheel 2.x does not support Linux after-build;
  the post-hook/inject_stubs had long been dead code; the stub is now the
  committed in-repo `PySparQ/pysparq/_core.pyi`, validated by pre-commit)

### Fixed
- **sdist exclude patterns are now anchored to the root directory** (leading
  `/`): gitignore-style patterns without a slash match at any level, so `test`
  had inadvertently hit `PySparQ/pysparq/test` (a test-support module shipped
  with the package since 0.1.1), leaving the wheel without `pysparq.test` and
  breaking QDA integration-test collection
- **Disabled pybind11's default LTO** (explicit
  `CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF`): MSVC link-time code generation
  intermittently hit internal compiler error C1001 on `_core` (a single
  translation unit ingesting all SparQ headers); a little link-time
  optimization is sacrificed for buildability
- JIT tests (dynamic_operator / doc_examples) now skip as whole modules in
  environments without g++ instead of failing case by case
- Declared the previously missing `typing_extensions` (Python 3.10)
  dependency
- Removed the orphaned build targets `PySparQ/src/` (a duplicate `_core`
  definition and `QDAAlgo`)

---

## 中文版

# Changelog

All notable changes to pysparq (SparQSim repository) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **仓库拆分说明**：本仓库由原 QRAM-Simulator monorepo 拆分而来（git 历史经
> filter-repo 完整保留，含 v0.1.0 / v0.1.1 标签）。拆分前的完整变更历史见
> [QRAM-Simulator 仓库 CHANGELOG](https://github.com/IAI-USTC-Quantum/QRAM-Simulator/blob/main/CHANGELOG.md)。

---

## [Unreleased]

## [0.2.1] - 2026-09-25

### Fixed
- **与 `qram-simulator` 包共装兼容**：`QRAMCircuit_qutrit` 绑定改用
  `py::module_local()`——C++ 类型 `qram_qutrit::QRAMCircuit` 同时被
  qram-simulator 包的薄绑定注册（Python 名 `QRAMCircuitQutrit`），
  pybind11 按 C++ typeid 全局注册导致同一进程导入两个包时报
  `generic_type: type "QRAMCircuit_qutrit" is already registered`。
  局部化后两包可共存（双向导入顺序均验证）；该类型实例仅在
  pysparq 模块内创建/传递（QRAMLoad 参数），无跨模块流动。

## [0.2.0] - 2026-09-25

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
