System Operations
=================

System operations manage register lifecycles, splitting and merging of basis states, and cleanup of zero amplitudes. Although these operations do not correspond directly to quantum gates, they are indispensable in algorithm workflows.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: System operations overview
   :header-rows: 1

   * - Operator / Function
     - Operation
     - Type
   * - ``Push``
     - Push a register onto the stack, saving its current value
     - BaseOperator
   * - ``Pop``
     - Restore a register's value from the stack
     - SelfAdjointOperator
   * - ``ClearZero``
     - Remove basis states whose amplitude is close to zero
     - SelfAdjointOperator
   * - ``split_systems``
     - Split the set of basis states by condition
     - Free function
   * - ``combine_systems``
     - Merge sets of basis states
     - Free function

---

Register Stack Management
-------------------------

Push (push onto the stack)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Push
   :members:
   :undoc-members:

**Operation**: Saves the register's current value to an internal stack and resets the register to ``|0⟩``.

**Parameters**:

- ``reg`` — target register (name or ID)
- ``garbage_name`` — temporary name used when saving it on the stack (optional)

**Purpose**: When a register needs to be used temporarily as an ancilla, first save its value, then restore it with ``Pop`` once it is no longer needed.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("data", ps.UnsignedInteger, 4)
   ps.System.add_register("temp", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("data", 7)(state)
   ps.Init_Unsafe("temp", 3)(state)

   # Save the current value of temp and reset it to 0
   ps.Push("temp")(state)

   # temp can now be used freely as an ancilla register
   ps.Add_UInt_UInt("data", "temp", "temp")(state)

Pop (pop from the stack)
^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Pop
   :members:
   :undoc-members:

**Operation**: Restores the register's value from the stack.

**Parameters**: ``reg`` — the register to restore (name or ID)

.. warning::

   ``Pop`` requires that the value saved on the stack is not entangled with the other registers; otherwise the behavior is undefined. It should normally be used in pairs with ``Push``.

.. code-block:: python

   # Restore temp to its value from before the Push
   ps.Pop("temp")(state)

The Push/Pop Pattern
""""""""""""""""""""

A typical workflow for ``Push`` / ``Pop``:

.. code-block:: python

   # 1. Save the ancilla register
   ps.Push("ancilla")(state)

   # 2. Compute with the ancilla register
   ps.Add_UInt_UInt("a", "ancilla", "ancilla")(state)
   # ... more operations ...
   ps.Add_UInt_UInt("a", "ancilla", "ancilla")(state)  # undo (SelfAdjoint)

   # 3. Restore the ancilla register
   ps.Pop("ancilla")(state)

---

Basis-State Cleanup
-------------------

ClearZero (clear near-zero amplitudes)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.ClearZero
   :members:
   :undoc-members:

**Operation**: Iterates over all basis states and removes the entries whose amplitude magnitude is below the threshold.

**Parameters**: ``eps`` (optional) — truncation threshold, ``1e-12`` by default.

**Purpose**: After many computations, the amplitudes of some basis states may become extremely small but nonzero due to floating-point error. ``ClearZero`` removes this numerical noise and keeps the sparse state compact.

.. code-block:: python

   ps.ClearZero()(state)       # use the default threshold
   ps.ClearZero(1e-8)(state)   # custom threshold

---

Splitting and Merging Basis States
----------------------------------

split_systems (split basis states)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.split_systems

**Operation**: Splits the basis states of a ``SparseState`` into two groups — those that satisfy and those that do not satisfy the given conditioned_by condition.

**Parameters**:

- ``state`` — the source ``SparseState``
- ``nonzeros`` / ``all_ones`` / ``by_bit`` / ``by_value`` — condition parameters

**Returns**: ``list[System]`` — the list of basis states that satisfy the condition.

.. code-block:: python

   # Split out the basis states where the ctrl register is nonzero
   matching = ps.split_systems(state, nonzeros=["ctrl"])

combine_systems (merge basis states)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.combine_systems

**Operation**: Merges the split-out basis states back into the ``SparseState``.

**Parameters**:

- ``to`` — the target ``SparseState``
- ``from`` — the list of basis states to merge

.. code-block:: python

   # Merge back into the main state
   ps.combine_systems(state, matching)

The Split-Transform-Merge Pattern
"""""""""""""""""""""""""""""""""

``split_systems`` / ``combine_systems`` can be used to implement manual conditional operations:

.. code-block:: python

   # 1. Split
   matching = ps.split_systems(state, nonzeros=["ctrl"])

   # 2. Apply operations to the matching basis states
   # ...

   # 3. Merge
   ps.combine_systems(state, matching)

-----

中文版
===

系统操作
========

系统操作管理寄存器生命周期、基态的拆分与合并、以及零振幅清理。这些操作虽然不直接对应量子门，但在算法流程中不可或缺。

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

概述
----

.. list-table:: 系统操作总览
   :header-rows: 1

   * - 算子 / 函数
     - 操作
     - 类型
   * - ``Push``
     - 将寄存器压入栈，保存当前值
     - BaseOperator
   * - ``Pop``
     - 从栈中恢复寄存器值
     - SelfAdjointOperator
   * - ``ClearZero``
     - 清除振幅接近零的基态
     - SelfAdjointOperator
   * - ``split_systems``
     - 按条件拆分基态集合
     - 自由函数
   * - ``combine_systems``
     - 合并基态集合
     - 自由函数

---

寄存器栈管理
------------

Push（压栈）
^^^^^^^^^^^^

.. autoclass:: pysparq.Push
   :members:
   :undoc-members:

**操作**: 将寄存器当前值保存到内部栈中，并将寄存器重置为 ``|0⟩``。

**参数**:

- ``reg`` — 目标寄存器（名称或 ID）
- ``garbage_name`` — 栈中保存用的临时名称（可选）

**用途**: 在需要临时使用一个寄存器作为辅助（ancilla）时，先保存其值，使用完毕后通过 ``Pop`` 恢复。

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("data", ps.UnsignedInteger, 4)
   ps.System.add_register("temp", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("data", 7)(state)
   ps.Init_Unsafe("temp", 3)(state)

   # 保存 temp 的当前值，重置为 0
   ps.Push("temp")(state)

   # 现在可以自由使用 temp 作为辅助寄存器
   ps.Add_UInt_UInt("data", "temp", "temp")(state)

Pop（弹栈）
^^^^^^^^^^^

.. autoclass:: pysparq.Pop
   :members:
   :undoc-members:

**操作**: 从栈中恢复寄存器的值。

**参数**: ``reg`` — 要恢复的寄存器（名称或 ID）

.. warning::

   ``Pop`` 要求栈中保存的值与当前其他寄存器无纠缠，否则行为未定义。通常应与 ``Push`` 成对使用。

.. code-block:: python

   # 恢复 temp 到 Push 前的值
   ps.Pop("temp")(state)

Push/Pop 模式
"""""""""""""

``Push`` / ``Pop`` 的典型工作流：

.. code-block:: python

   # 1. 保存辅助寄存器
   ps.Push("ancilla")(state)

   # 2. 使用辅助寄存器进行计算
   ps.Add_UInt_UInt("a", "ancilla", "ancilla")(state)
   # ... 更多操作 ...
   ps.Add_UInt_UInt("a", "ancilla", "ancilla")(state)  # 撤销（SelfAdjoint）

   # 3. 恢复辅助寄存器
   ps.Pop("ancilla")(state)

---

基态清理
--------

ClearZero（清除零振幅）
^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.ClearZero
   :members:
   :undoc-members:

**操作**: 遍历所有基态，移除振幅绝对值低于阈值的项。

**参数**: ``eps``（可选）— 截断阈值，默认为 ``1e-12``。

**用途**: 经过多次运算后，某些基态的振幅可能因浮点误差变得极小但不为零。``ClearZero`` 可清除这些数值噪声，保持稀疏态的紧凑性。

.. code-block:: python

   ps.ClearZero()(state)       # 使用默认阈值
   ps.ClearZero(1e-8)(state)   # 自定义阈值

---

基态拆分与合并
--------------

split_systems（拆分基态）
^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.split_systems

**操作**: 根据 conditioned_by 条件将 ``SparseState`` 中的基态分为满足条件和不满足条件两组。

**参数**:

- ``state`` — 源 ``SparseState``
- ``nonzeros`` / ``all_ones`` / ``by_bit`` / ``by_value`` — 条件参数

**返回**: ``list[System]`` — 满足条件的基态列表。

.. code-block:: python

   # 拆分出 ctrl 寄存器非零的基态
   matching = ps.split_systems(state, nonzeros=["ctrl"])

combine_systems（合并基态）
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.combine_systems

**操作**: 将拆分出的基态重新合并回 ``SparseState``。

**参数**:

- ``to`` — 目标 ``SparseState``
- ``from`` — 要合并的基态列表

.. code-block:: python

   # 合并回主状态
   ps.combine_systems(state, matching)

拆分-变换-合并模式
""""""""""""""""""

``split_systems`` / ``combine_systems`` 可用于实现手工条件操作：

.. code-block:: python

   # 1. 拆分
   matching = ps.split_systems(state, nonzeros=["ctrl"])

   # 2. 对匹配的基态执行操作
   # ...

   # 3. 合并
   ps.combine_systems(state, matching)
