Core Concepts
=============

This section introduces the fundamental building blocks of sparse-state quantum simulation in PySparQ. Understanding these concepts is a prerequisite for using the Register Level Programming paradigm.

.. toctree::
   :maxdepth: 2

   system
   sparse_state
   register_management
   register_types
   operators

Core Abstractions
-----------------

PySparQ is built on three core abstractions:

1. **System** — a single computational basis state, containing one complex amplitude ``amplitude`` and the values of all registers ``registers``
2. **SparseState** — a sparse quantum state managing a ``std::vector<System>``, storing only basis states with non-zero amplitudes
3. **Registers** — named quantum variables with typed storage, using ``uint64_t`` as the storage unit, allowing multi-register encodings of the form ``|a⟩|b⟩|c⟩``

Core Data Relationships
-----------------------

- The default constructor of ``SparseState`` creates a ``|0...0⟩`` initial state (internally containing a single ``System`` whose register values are all 0 and whose amplitude is 1)
- The register-value combination of each ``System`` instance must be unique within a ``SparseState`` — if a duplicate appears, quantum interference has occurred, and the two amplitudes should be added and de-duplicated
- The operational level of quantum programming rises from qubits up to quantum registers; almost all operations take registers as their unit

Unlike traditional full state-vector simulators, which store :math:`2^n` amplitudes, PySparQ stores only the non-zero basis states, so quantum algorithms with a finite number of superposed states can be simulated with polynomial resources.

----

中文版
===

核心概念
========

本节介绍 PySparQ 稀疏态量子模拟的基础构建模块。理解这些概念是使用 Register Level Programming 范式的前提。

.. toctree::
   :maxdepth: 2

   system
   sparse_state
   register_management
   register_types
   operators

核心抽象
--------

PySparQ 基于三个核心抽象构建：

1. **System** — 单个计算基态，包含一个复数振幅 ``amplitude`` 和所有寄存器的值 ``registers``
2. **SparseState** — 托管 ``std::vector<System>`` 的稀疏量子态，仅存储振幅非零的基态
3. **Registers** — 具有类型存储的命名量子变量，以 ``uint64_t`` 为存储单位，允许 ``|a⟩|b⟩|c⟩`` 式的多寄存器编码

核心数据关系
------------

- ``SparseState`` 的默认构造函数会创建一个 ``|0...0⟩`` 的初态（内部包含一个所有寄存器值为 0、振幅为 1 的 ``System``）
- 每个 ``System`` 实例中的寄存器值组合在 ``SparseState`` 中必须是唯一的——若出现重复，意味着发生了量子干涉，两个振幅应相加并去重
- 量子编程的操作层面从量子比特上升到了量子寄存器，几乎所有操作都以寄存器为单位

与传统全态矢量模拟器存储 :math:`2^n` 个振幅不同，PySparQ 仅存储非零基态，使得叠加态数量有限的量子算法可以在多项式资源内模拟。
