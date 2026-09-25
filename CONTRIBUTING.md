# Contributing Guide — SparQSim (pysparq)

First of all, thank you for considering contributing to the SparQSim project! 🎉

## Repository Relationship

- This repository: the pysparq Python package (rich bindings + pure-Python framework)
- C++ core: [QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator),
  referenced as a submodule (`extern/qram-simulator`, relative URL `../QRAM-Simulator.git`)

## Development Environment

```bash
git clone --recurse-submodules <repo-url>
cd SparQSim

python -m venv .venv && source .venv/bin/activate  # or: uv venv
pip install . pytest
pytest PySparQ/test -v
```

Note: `test_dynamic_operator.py` requires a working `g++`.

## Core Change Workflow

- C++ core changes → open a PR in the QRAM-Simulator repository; after a release
  tag is published, run `git submodule update --remote` back in this repository
  and commit the pin
- Binding/Python-layer changes → make them directly in this repository
- When changing the C++ API, mind the dual binding surface: this repository's
  `PySparQ/core.cpp` (rich bindings) and QRAM-Simulator's `bindings/python/`
  (thin bindings)

## Coding Standards

- **Python**: black (line-length=100), isort (profile=black), flake8
- **C++** (binding layer): clang-format (LLVM style, 4-space indent, 120 columns)
- Run `pre-commit run --all-files` before committing

---

## 中文版

# 贡献指南 — SparQSim (pysparq)

首先，感谢你考虑为 SparQSim 项目做出贡献！🎉

## 仓库关系

- 本仓库：pysparq Python 包（富绑定 + 纯 Python 框架）
- C++ 核心：[QRAM-Simulator](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)，
  以 submodule 引用（`extern/qram-simulator`，相对 URL `../QRAM-Simulator.git`）

## 开发环境

```bash
git clone --recurse-submodules <repo-url>
cd SparQSim

python -m venv .venv && source .venv/bin/activate  # 或 uv venv
pip install . pytest
pytest PySparQ/test -v
```

注意：`test_dynamic_operator.py` 需要可用的 `g++`。

## 核心变更工作流

- C++ 核心改动 → 去 QRAM-Simulator 仓库提 PR；发布 tag 后回本仓库
  `git submodule update --remote` 并提交 pin
- 绑定/Python 层改动 → 本仓库直接改
- 改动 C++ API 时注意双绑定面：本仓库的 `PySparQ/core.cpp`（富绑定）与
  QRAM-Simulator 的 `bindings/python/`（薄绑定）

## 编码规范

- **Python**: black（line-length=100）、isort（profile=black）、flake8
- **C++**（绑定层）: clang-format（LLVM 风格，4 空格缩进，120 列）
- 提交前运行 `pre-commit run --all-files`
