Dark Magic Operations
=====================

.. raw:: html

   <div class="admonition warning">
   <p class="admonition-title">Warning</p>
   <p>The operations in this section bypass the normal constraints of quantum mechanics and are <strong>not guaranteed to be unitary</strong>. Use them only for debugging, initialization, or when you clearly understand the consequences.</p>
   </div>

Dark magic operations provide the ability to directly modify the internal representation of a quantum state without going through normal quantum gate transformations. These operations do not correspond to any physical quantum operations.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Dark magic operations overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity
   * - ``Normalize``
     - Normalize the quantum state
     - Non-unitary
   * - ``Init_Unsafe``
     - Set a register value directly
     - Non-unitary

---

Normalize
------------------

.. autoclass:: pysparq.Normalize
   :members:
   :undoc-members:

**Operation**: Normalizes the amplitudes of the entire ``SparseState`` so that :math:`\sum_i |\alpha_i|^2 = 1`.

**Parameters**: None.

**Purpose**: Use this operator to repair a quantum state that has been left unnormalized by manual manipulation (e.g. ``Init_Unsafe``) or by numerical error.

.. code-block:: python

   import pysparq as ps

   # Suppose some operations have left the quantum state unnormalized
   ps.Normalize()(state)

   # You can check it with ViewNormalization
   ps.ViewNormalization()(state)

---

Init_Unsafe (unsafe initialization)
-----------------------------------

.. autoclass:: pysparq.Init_Unsafe
   :members:
   :undoc-members:

**Operation**: Directly sets the value of the specified register to a given constant, **without going through any quantum gate**.

**Parameters**:

- ``reg`` — target register (name or ID)
- ``value`` — the value to set (integer)

**Behavior**: Iterates over all basis states and forcibly sets the specified register's value to ``value``. If multiple basis states end up with duplicate keys after the assignment, their amplitudes are summed.

.. warning::

   This operation is **not guaranteed to be unitary**. It destroys superposition information — if the register was in a superposition, all components other than ``value`` are overwritten.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Set the initial values directly
   ps.Init_Unsafe("a", 3)(state)
   ps.Init_Unsafe("b", 5)(state)

   ps.pprint(state)
   # |a=3,b=5⟩ : (1+0j)

.. note::

   The most common use of ``Init_Unsafe`` is to set the initial values of the input registers at the start of an algorithm. Be especially careful when using it on a superposed state.

-----

中文版
===

黑魔法操作
==========

.. raw:: html

   <div class="admonition warning">
   <p class="admonition-title">警告</p>
   <p>本节操作绕过正常的量子力学约束，<strong>不保证幺正性</strong>。仅在调试、初始化或明确了解后果时使用。</p>
   </div>

黑魔法操作提供直接修改量子态内部表示的能力，不经过正常的量子门变换。这些操作不对应任何物理量子操作。

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

概述
----

.. list-table:: 黑魔法操作总览
   :header-rows: 1

   * - 算子
     - 操作
     - 幺正性
   * - ``Normalize``
     - 归一化量子态
     - 非幺正
   * - ``Init_Unsafe``
     - 直接设置寄存器值
     - 非幺正

---

Normalize（归一化）
------------------

.. autoclass:: pysparq.Normalize
   :members:
   :undoc-members:

**操作**: 将整个 ``SparseState`` 的振幅归一化，使得 :math:`\sum_i |\alpha_i|^2 = 1`。

**参数**: 无。

**用途**: 当手工操作（如 ``Init_Unsafe``）或数值误差导致量子态未归一化时，使用此算子修复。

.. code-block:: python

   import pysparq as ps

   # 假设某些操作导致量子态未归一化
   ps.Normalize()(state)

   # 可以通过 ViewNormalization 检查
   ps.ViewNormalization()(state)

---

Init_Unsafe（不安全初始化）
---------------------------

.. autoclass:: pysparq.Init_Unsafe
   :members:
   :undoc-members:

**操作**: 直接将指定寄存器的值设置为给定常数，**不经过任何量子门**。

**参数**:

- ``reg`` — 目标寄存器（名称或 ID）
- ``value`` — 要设置的值（整数）

**行为**: 遍历所有基态，将指定寄存器的值强制设为 ``value``。如果多个基态在设置后键值重复，它们的振幅会相加。

.. warning::

   此操作 **不保证幺正性**。它会破坏叠加态信息——如果寄存器原本处于叠加态，所有非 ``value`` 的分量将被覆盖。

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # 直接设置初始值
   ps.Init_Unsafe("a", 3)(state)
   ps.Init_Unsafe("b", 5)(state)

   ps.pprint(state)
   # |a=3,b=5⟩ : (1+0j)

.. note::

   ``Init_Unsafe`` 最常见的用途是在算法开始时设置输入寄存器的初值。在叠加态上使用时需格外小心。
