The SparseState Class
=====================

The ``SparseState`` class is the core data structure of PySparQ; it represents a sparse quantum state. It internally manages a ``std::vector<System>`` and stores only basis states with non-zero amplitudes.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Sparse Representation Principles
--------------------------------

A traditional full state-vector simulator stores :math:`2^n` amplitudes (:math:`n` being the number of qubits), regardless of whether most of them are zero. PySparQ uses a sparse representation:

- Only basis states with ``amplitude ≠ 0`` are stored
- Each ``System`` represents one computational basis state
- When the number of superposed states is finite, the storage complexity is polynomial

Uniqueness Rule
---------------

A core invariant of ``SparseState`` is: **the register-value combination of every ``System`` must be unique**.

If an operation on the basis states of a ``SparseState`` produces two ``System`` objects with identical register values, this means quantum interference has occurred — in that case the amplitudes of the two ``System`` objects should be added and they should be merged into a single ``System``. This process is usually performed automatically by the ``sort-merge-unique`` mechanism inside operators.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   # SparseState() default construction: creates a single |q=0⟩ basis state
   state = ps.SparseState()
   print(f"basis-state count: {state.size()}")  # 1

   # Apply Hadamard to create 2^2 = 4 basis states
   ps.Hadamard_Int_Full("q")(state)
   print(f"basis-state count: {state.size()}")  # 4

Basic Usage
-----------

Creating a SparseState
^^^^^^^^^^^^^^^^^^^^^^

.. important::

   The default constructor of ``SparseState()`` automatically creates a ``|0...0⟩`` initial state (i.e. a single ``System`` whose register values are all 0 and whose amplitude is 1). You usually do **not** need to manually construct ``System`` objects to create a ``SparseState``.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 10)

   # Default construction: creates a single |q=0⟩ basis state (amplitude 1)
   state = ps.SparseState()

Accessing Basis States
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Get the list of basis states
   for system in state.basis_states:
       print(f"amplitude: {system.amplitude}")

   # Access by index
   first = state[0]
   last = state.basis_states[-1]

   # Get the number of basis states
   n = state.size()

   # Check whether the state is empty
   if state.empty():
       print("state is empty")

State Evolution Example
-----------------------

The following example shows how a ``SparseState`` evolves under operator actions:

.. code-block:: python
   :caption: Example: Hadamard creates a superposition

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   # SparseState creates the |q=0⟩ initial state by default
   state = ps.SparseState()

   print("initial state:")
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 1.000000+0.000000i  q=|0>

   # Apply Hadamard (partial superposition: apply H to bit 0)
   ps.Hadamard_Int("q", 1)(state)

   print("\nafter Hadamard:")
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 0.707107+0.000000i  q=|0>
   # 0.707107+0.000000i  q=|2>

   # Full Hadamard (all output states)
   ps.Hadamard_Int_Full("q")(state)

   print("\nafter full Hadamard:")
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 0.500000+0.000000i  q=|0>
   # 0.500000+0.000000i  q=|1>
   # 0.500000+0.000000i  q=|2>
   # 0.500000+0.000000i  q=|3>

State Printing Modes
--------------------

``ps.StatePrint(state, mode)`` and ``ps.pprint(state, mode)`` support several display modes:

.. list-table:: The StatePrintDisplay enum
   :header-rows: 1

   * - Mode
     - Value
     - Description
     - Example output
   * - ``Default``
     - 0
     - Default mode, decimal values
     - ``0.5+0.000000i |5>``
   * - ``Detail``
     - 1
     - Detailed mode, with register header and amplitudes
     - ``0.500000+0.000000i  q=|5>``
   * - ``Binary``
     - 2
     - Binary representation
     - ``0.5+0.000000i |0101>``
   * - ``Prob``
     - 4
     - Probability view
     - ``0.5+0.000000i (p = 0.25) |5>``

.. code-block:: python

   # Different display modes
   ps.pprint(state)                                                    # Detail (default)
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default))      # Default
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Binary))       # Binary
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Prob))         # Prob

   # Specify the precision
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default, precision=15))
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default, precision=4))

Clearing Near-Zero Amplitudes
-----------------------------

.. code-block:: python

   # Remove basis states with |amplitude|² < epsilon
   ps.ClearZero(epsilon=1e-10)(state)

   # Normalize the state
   ps.Normalize()(state)

Merging Duplicate Basis States
------------------------------

When multiple basis states have identical register values, their amplitudes are merged automatically:

.. code-block:: python

   # The merge_system function: merges the amplitudes of two identical basis states
   # Usually called automatically inside operators

Iterator Support
----------------

``SparseState`` supports the standard Python iterator protocol:

.. code-block:: python

   # Forward iteration
   for system in state.basis_states:
       print(system.amplitude)

   # Reverse iteration
   for system in reversed(state.basis_states):
       print(system.amplitude)

   # Index access
   first = state.basis_states[0]
   last = state.basis_states[-1]

   # Slicing
   first_three = state.basis_states[:3]

API Reference
-------------

.. autoclass:: pysparq.SparseState
   :members:
   :undoc-members:

.. autofunction:: pysparq.split_systems

.. autofunction:: pysparq.combine_systems

.. autofunction:: pysparq.merge_system

.. autofunction:: pysparq.remove_system

----

中文版
===

SparseState 类
==============

``SparseState`` 类是 PySparQ 的核心数据结构，表示一个稀疏量子态。它内部托管了 ``std::vector<System>``，只存储振幅非零的基态。

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

稀疏表示原理
------------

传统全态矢量模拟器存储 :math:`2^n` 个振幅（:math:`n` 为量子比特数），无论大多数振幅是否为零。PySparQ 采用稀疏表示：

- 只存储 ``amplitude ≠ 0`` 的基态
- 每个 ``System`` 代表一个计算基态
- 叠加态数量有限时，存储复杂度为多项式级

唯一性规则
----------

``SparseState`` 中的一个核心不变量是：**所有 ``System`` 的寄存器值组合必须唯一**。

如果对 ``SparseState`` 中的基态进行操作后产生了两个具有相同寄存器值的 ``System``，这意味着发生了量子干涉——此时两个 ``System`` 的振幅应当相加，合并为一个 ``System``。这一过程通常由算子内部的 ``sort-merge-unique`` 机制自动完成。

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   # SparseState() 默认构造：创建单个 |q=0⟩ 基态
   state = ps.SparseState()
   print(f"基态数量: {state.size()}")  # 1

   # 施加 Hadamard 创建 2^2 = 4 个基态
   ps.Hadamard_Int_Full("q")(state)
   print(f"基态数量: {state.size()}")  # 4

基本用法
--------

创建 SparseState
^^^^^^^^^^^^^^^^

.. important::

   ``SparseState()`` 的默认构造函数会自动创建一个 ``|0...0⟩`` 的初态（即一个所有寄存器值为 0、振幅为 1 的 ``System``）。通常**不需要**手动构造 ``System`` 对象来创建 ``SparseState``。

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 10)

   # 默认构造：创建单个 |q=0⟩ 基态（振幅为 1）
   state = ps.SparseState()

访问基态
^^^^^^^^

.. code-block:: python

   # 获取基态列表
   for system in state.basis_states:
       print(f"振幅: {system.amplitude}")

   # 按索引访问
   first = state[0]
   last = state.basis_states[-1]

   # 获取基态数量
   n = state.size()

   # 检查是否为空
   if state.empty():
       print("状态为空")

状态演化示例
------------

下面的示例展示 ``SparseState`` 如何随算子操作演化：

.. code-block:: python
   :caption: 示例：Hadamard 创建叠加态

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   # SparseState 默认创建 |q=0⟩ 初态
   state = ps.SparseState()

   print("初始状态:")
   ps.pprint(state)
   # 输出：
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 1.000000+0.000000i  q=|0>

   # 施加 Hadamard（部分叠加：对第 0 位施 H）
   ps.Hadamard_Int("q", 1)(state)

   print("\nHadamard 后:")
   ps.pprint(state)
   # 输出：
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 0.707107+0.000000i  q=|0>
   # 0.707107+0.000000i  q=|2>

   # 完整 Hadamard（所有输出状态）
   ps.Hadamard_Int_Full("q")(state)

   print("\n完整 Hadamard 后:")
   ps.pprint(state)
   # 输出：
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 0.500000+0.000000i  q=|0>
   # 0.500000+0.000000i  q=|1>
   # 0.500000+0.000000i  q=|2>
   # 0.500000+0.000000i  q=|3>

状态打印模式
------------

``ps.StatePrint(state, mode)`` 和 ``ps.pprint(state, mode)`` 支持多种显示模式：

.. list-table:: StatePrintDisplay 枚举
   :header-rows: 1

   * - 模式
     - 值
     - 说明
     - 示例输出
   * - ``Default``
     - 0
     - 默认模式，十进制值
     - ``0.5+0.000000i |5>``
   * - ``Detail``
     - 1
     - 详细模式，含寄存器头和振幅
     - ``0.500000+0.000000i  q=|5>``
   * - ``Binary``
     - 2
     - 二进制表示
     - ``0.5+0.000000i |0101>``
   * - ``Prob``
     - 4
     - 概率视图
     - ``0.5+0.000000i (p = 0.25) |5>``

.. code-block:: python

   # 不同显示模式
   ps.pprint(state)                                                    # Detail（默认）
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default))      # Default
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Binary))       # Binary
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Prob))         # Prob

   # 指定精度
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default, precision=15))
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default, precision=4))

清除接近零的振幅
----------------

.. code-block:: python

   # 清除 |amplitude|² < epsilon 的基态
   ps.ClearZero(epsilon=1e-10)(state)

   # 归一化状态
   ps.Normalize()(state)

合并重复基态
------------

当多个基态具有相同的寄存器值时，它们的振幅会自动合并：

.. code-block:: python

   # merge_system 函数：合并两个相同基态的振幅
   # 通常由算子内部自动调用

迭代器支持
----------

``SparseState`` 支持标准 Python 迭代器协议：

.. code-block:: python

   # 正向迭代
   for system in state.basis_states:
       print(system.amplitude)

   # 反向迭代
   for system in reversed(state.basis_states):
       print(system.amplitude)

   # 索引访问
   first = state.basis_states[0]
   last = state.basis_states[-1]

   # 切片
   first_three = state.basis_states[:3]

API 参考
--------

.. autoclass:: pysparq.SparseState
   :members:
   :undoc-members:

.. autofunction:: pysparq.split_systems

.. autofunction:: pysparq.combine_systems

.. autofunction:: pysparq.merge_system

.. autofunction:: pysparq.remove_system
