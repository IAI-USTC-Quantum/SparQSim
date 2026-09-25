# SparQSim Documentation

English | [简体中文](README_zh-cn.md)

This directory holds the sources of the **Sphinx site** (published on
[GitHub Pages](https://iai-ustc-quantum.github.io/SparQSim/)) and the
algorithm porting guide. The site is **bilingual (English by default,
Chinese available)** and carries:

- **User guide** (installation / quickstart / architecture / core concepts /
  RIR / dynamic operators) — reStructuredText + MyST Markdown
- **Operator reference** — register-level operator catalog with semantics
  and examples
- **C++ API reference** — Doxygen header comments ingested into Sphinx via
  **breathe**
- **Python API reference** — `pysparq` package parsed by **sphinx-autoapi**
  (requires pysparq installed)
- **Notebooks** — rendered statically via **nbsphinx** (not executed)
- **Algorithm porting guide** — [`algorithm-implementation.md`](algorithm-implementation.md)
  (one-to-one mapping between the C++ experiments and `pysparq.algorithms`)

The rich `pysparq` bindings live in `PySparQ/` of this repository; the pure
C++ QRAM base lives in the
[QRAM-Simulator repository](https://github.com/IAI-USTC-Quantum/QRAM-Simulator).

## Directory layout

```
docs/
├── algorithm-implementation.md       # algorithm porting guide (English; see algorithm-implementation_zh-cn.md)
├── doxygen/                          # Doxyfile -> build/xml (input for breathe)
└── sphinx/
    ├── Makefile                      # make html (builds both trees)
    ├── requirements.txt              # Sphinx build dependencies
    └── source/
        ├── _conf_base.py             # shared Sphinx config (theme, breathe, autoapi, nbsphinx)
        ├── _shared/                  # shared static files + templates (language switcher)
        ├── en/                       # English tree (default language)
        │   ├── conf.py, index.rst
        │   ├── guide/                # install/quickstart/architecture/core_concepts/development
        │   ├── operators/            # operator reference
        │   ├── api/                  # Python/dynamic operator/RIR API pages
        │   ├── cpp_api/              # C++ API (breathe)
        │   └── notebooks/            # static notebooks (nbsphinx)
        └── zh/                       # Chinese tree (same layout as en/)
```

## Local build

```bash
# 0. Doxygen XML (breathe input; requires doxygen on the system) — also run
#    automatically by `make html`
cd docs/doxygen && doxygen Doxyfile && cd ../..

# 1. Sphinx dependencies + pysparq (autoapi imports the package; without the
#    compiled _core extension the dynamic_operator / rir pages fail)
pip install -r docs/sphinx/requirements.txt .
#    pandoc is required by nbsphinx (choco install pandoc / apt install pandoc)

# 2. Sphinx site (both language trees)
cd docs/sphinx && make html
# -> docs/sphinx/build/html/{zh,en}
```

## CI (docs.yml)

push/PR trigger -> doxygen -> pip install . -> sphinx-build (zh + en) ->
artifact upload; on push to main the site is deployed to gh-pages together
with a root redirect page (full replacement; the root URL redirects to the
English tree).
