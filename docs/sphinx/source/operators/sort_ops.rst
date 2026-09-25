Sorting Operators
=================

Sorting operators sort the basis states in a ``SparseState`` along different dimensions. Sorting itself does not change the quantum state (amplitudes are unchanged), but it affects the display order of tools such as ``StatePrint`` and is used for correct matching in internal conditional operations.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Sorting operators overview
   :header-rows: 1

   * - Operator
     - Sort key
   * - ``SortByKey``
     - Sort by the value of a specified register
   * - ``SortByKey2``
     - Joint sort by the values of two registers
   * - ``SortExceptKey``
     - Sort by all registers except the specified one
   * - ``SortExceptBit``
     - Sort by all bits except the specified bit
   * - ``SortExceptKeyHadamard``
     - Sort by the register after the Hadamard transform
   * - ``SortUnconditional``
     - Unconditional sort (by the full basis-state key)
   * - ``SortByAmplitude``
     - Sort by amplitude magnitude

---

SortByKey (sort by key)
-----------------------

.. autoclass:: pysparq.SortByKey
   :members:
   :undoc-members:

**Operation**: Sorts the basis states in ascending order by the value of the specified register.

**Parameters**: ``key`` — the register to sort by (name or ID).

.. code-block:: python

   import pysparq as ps

   ps.SortByKey("addr")(state)

SortByKey2 (two-key sort)
-------------------------

.. autoclass:: pysparq.SortByKey2
   :members:
   :undoc-members:

**Operation**: Jointly sorts by the values of two registers (first by ``key1``, then by ``key2``).

**Parameters**: ``key1``, ``key2`` — the two sort-key registers.

.. code-block:: python

   ps.SortByKey2("addr", "data")(state)

SortExceptKey (sort excluding a key)
------------------------------------

.. autoclass:: pysparq.SortExceptKey
   :members:
   :undoc-members:

**Operation**: Sorts by the values of all registers except the specified one.

**Parameters**: ``key`` — the register to exclude.

**Purpose**: Use this when the value of a certain register should be ignored during sorting; it is often needed for correct matching in internal conditional operations.

.. code-block:: python

   ps.SortExceptKey("temp")(state)

SortExceptBit (sort excluding a bit)
------------------------------------

.. autoclass:: pysparq.SortExceptBit
   :members:
   :undoc-members:

**Operation**: Sorts by all bits except a specified bit of a specified register.

**Parameters**: ``key`` — the register, ``digit`` — the index of the bit to exclude.

.. code-block:: python

   ps.SortExceptBit("q", 0)(state)

SortExceptKeyHadamard (sort excluding a Hadamard key)
-----------------------------------------------------

.. autoclass:: pysparq.SortExceptKeyHadamard
   :members:
   :undoc-members:

**Operation**: Sorts by excluding the specified qubits after a Hadamard transform.

**Parameters**: ``key`` — the register, ``qubit_ids`` — the set of qubit indices to exclude.

.. code-block:: python

   ps.SortExceptKeyHadamard("q", {0, 2})(state)

SortUnconditional (unconditional sort)
--------------------------------------

.. autoclass:: pysparq.SortUnconditional
   :members:
   :undoc-members:

**Operation**: Unconditionally sorts by the full basis-state key. No parameters are required.

.. code-block:: python

   ps.SortUnconditional()(state)

SortByAmplitude (sort by amplitude)
-----------------------------------

.. autoclass:: pysparq.SortByAmplitude
   :members:
   :undoc-members:

**Operation**: Sorts by the magnitude of the basis-state amplitudes. No parameters are required.

**Purpose**: Quickly see which basis states contribute the most when debugging.

.. code-block:: python

   ps.SortByAmplitude()(state)

-----

中文版
===

排序算子
========

排序算子对 ``SparseState`` 中的基态按不同维度进行排序。排序本身不改变量子态（振幅不变），但会影响 ``StatePrint`` 等工具的显示顺序，并用于内部条件操作的正确匹配。

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

概述
----

.. list-table:: 排序算子总览
   :header-rows: 1

   * - 算子
     - 排序依据
   * - ``SortByKey``
     - 按指定寄存器值排序
   * - ``SortByKey2``
     - 按两个寄存器值联合排序
   * - ``SortExceptKey``
     - 按除指定寄存器外的所有寄存器排序
   * - ``SortExceptBit``
     - 按除指定位外的所有位排序
   * - ``SortExceptKeyHadamard``
     - 按 Hadamard 变换后的寄存器排序
   * - ``SortUnconditional``
     - 无条件排序（按完整基态键值）
   * - ``SortByAmplitude``
     - 按振幅绝对值排序

---

SortByKey（按键排序）
--------------------

.. autoclass:: pysparq.SortByKey
   :members:
   :undoc-members:

**操作**: 按指定寄存器的值对基态进行升序排序。

**参数**: ``key`` — 排序依据的寄存器（名称或 ID）。

.. code-block:: python

   import pysparq as ps

   ps.SortByKey("addr")(state)

SortByKey2（双键排序）
---------------------

.. autoclass:: pysparq.SortByKey2
   :members:
   :undoc-members:

**操作**: 按两个寄存器的值联合排序（先按 ``key1``，再按 ``key2``）。

**参数**: ``key1``, ``key2`` — 两个排序依据的寄存器。

.. code-block:: python

   ps.SortByKey2("addr", "data")(state)

SortExceptKey（排除键排序）
--------------------------

.. autoclass:: pysparq.SortExceptKey
   :members:
   :undoc-members:

**操作**: 按除指定寄存器外的所有寄存器值排序。

**参数**: ``key`` — 要排除的寄存器。

**用途**: 当需要忽略某个寄存器的值进行排序时使用，常用于内部条件操作的正确匹配。

.. code-block:: python

   ps.SortExceptKey("temp")(state)

SortExceptBit（排除位排序）
--------------------------

.. autoclass:: pysparq.SortExceptBit
   :members:
   :undoc-members:

**操作**: 按除指定寄存器的某一位外的所有位排序。

**参数**: ``key`` — 寄存器，``digit`` — 要排除的位索引。

.. code-block:: python

   ps.SortExceptBit("q", 0)(state)

SortExceptKeyHadamard（排除 Hadamard 键排序）
--------------------------------------------

.. autoclass:: pysparq.SortExceptKeyHadamard
   :members:
   :undoc-members:

**操作**: 按 Hadamard 变换后排除指定量子比特的方式排序。

**参数**: ``key`` — 寄存器，``qubit_ids`` — 要排除的量子比特位索引集合。

.. code-block:: python

   ps.SortExceptKeyHadamard("q", {0, 2})(state)

SortUnconditional（无条件排序）
-------------------------------

.. autoclass:: pysparq.SortUnconditional
   :members:
   :undoc-members:

**操作**: 按完整基态键值无条件排序。无需任何参数。

.. code-block:: python

   ps.SortUnconditional()(state)

SortByAmplitude（按振幅排序）
----------------------------

.. autoclass:: pysparq.SortByAmplitude
   :members:
   :undoc-members:

**操作**: 按基态振幅的绝对值进行排序。无需任何参数。

**用途**: 调试时快速查看哪些基态贡献最大。

.. code-block:: python

   ps.SortByAmplitude()(state)
