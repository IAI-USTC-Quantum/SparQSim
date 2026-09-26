# SparQSim 文档

[English](README.md) | 简体中文

本目录承载 **Sphinx 站点**源码（发布在
[GitHub Pages](https://iai-ustc-quantum.github.io/SparQSim/)）与算法转译指南。
站点为**中英双语（默认英文，可切换中文）**，内容包括：

- **用户指南**（安装 / 快速上手 / 架构 / 核心概念 / RIR / 动态算子）——
  reStructuredText + MyST Markdown
- **算子参考** —— 寄存器级算子目录（语义与示例）
- **C++ API 参考** —— 头文件 Doxygen 注释经 **breathe** 汇入 Sphinx
- **Python API 参考** —— **sphinx-autoapi** 解析 `pysparq` 包（需先安装 pysparq）
- **Notebook** —— 经 **nbsphinx** 静态渲染（不执行）
- **算法转译指南** —— [`algorithm-implementation_zh-cn.md`](algorithm-implementation_zh-cn.md)
  （C++ 实验与 `pysparq.algorithms` 的逐一对应）

富绑定 `pysparq` 位于本仓库 `PySparQ/`；纯 C++ QRAM 基座见
[QRAM-Simulator 仓库](https://github.com/IAI-USTC-Quantum/QRAM-Simulator)。

## 目录结构

```
docs/
├── algorithm-implementation.md       # 算法转译指南（英文版；中文版见 algorithm-implementation_zh-cn.md）
├── verification/                     # 运行时验证记录（如 v1b-runtime.md）
├── doxygen/                          # Doxyfile -> build/xml（breathe 输入）
└── sphinx/
    ├── Makefile                      # make html（两棵树一起构建）
    ├── requirements.txt              # Sphinx 构建依赖
    └── source/
        ├── _conf_base.py             # 共享 Sphinx 配置（主题、breathe、autoapi、nbsphinx）
        ├── _shared/                  # 共享静态资源与模板（语言切换器）
        ├── en/                       # 英文源树（默认语言）
        │   ├── conf.py, index.rst
        │   ├── guide/                # 安装/快速上手/架构/核心概念/开发指南
        │   ├── operators/            # 算子参考
        │   ├── api/                  # Python/动态算子/RIR API 页面
        │   ├── cpp_api/              # C++ API（breathe）
        │   └── notebooks/            # 静态 notebook（nbsphinx）
        └── zh/                       # 中文源树（与 en/ 同构）
```

## 本地构建

```bash
# 0. Doxygen XML（breathe 输入；系统需装有 doxygen）——`make html` 也会自动执行
cd docs/doxygen && doxygen Doxyfile && cd ../..

# 1. Sphinx 依赖 + pysparq（autoapi 需导入包；缺少编译出的 _core 扩展时
#    dynamic_operator / rir 页面会失败）
pip install -r docs/sphinx/requirements.txt .
#    nbsphinx 需要 pandoc（choco install pandoc / apt install pandoc）

# 2. Sphinx 站点（两棵语言树一起构建）
cd docs/sphinx && make html
# -> docs/sphinx/build/html/{zh,en}
```

## CI（docs.yml）

push/PR 触发 -> doxygen -> pip install . -> sphinx-build（zh + en）->
上传 artifact；push 到 main 时连同根跳转页一起部署到 gh-pages
（全量替换；根 URL 重定向到英文树）。
