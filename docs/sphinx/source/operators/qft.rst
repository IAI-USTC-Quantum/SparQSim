Quantum Fourier Transform (QFT)
===============================

The quantum Fourier transform is a core component of many quantum algorithms.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: QFT operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity class
   * - ``QFT``
     - Quantum Fourier transform
     - BaseOperator
   * - ``InverseQFT``
     - Inverse quantum Fourier transform
     - BaseOperator

Mathematical Definition
-----------------------

The quantum Fourier transform is defined as:

.. math::

   QFT|x\rangle = \frac{1}{\sqrt{2^n}} \sum_{y=0}^{2^n-1} e^{2\pi i xy / 2^n} |y\rangle

The inverse quantum Fourier transform:

.. math::

   QFT^\dagger|y\rangle = \frac{1}{\sqrt{2^n}} \sum_{x=0}^{2^n-1} e^{-2\pi i xy / 2^n} |x\rangle

---

QFT
---

.. autoclass:: pysparq.QFT
   :members:
   :undoc-members:

**Operation**: Performs the quantum Fourier transform on an integer register

**Type constraints**: ``UnsignedInteger`` or ``SignedInteger``

**Bit constraints**: The register size determines the transform dimension

.. code-block:: python

   import pysparq as ps

   ps.System.clear()

   # 4-bit register
   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Initialize to the basis state |1⟩
   ps.Init_Unsafe("q", 1)(state)

   print("Initial state:")
   ps.pprint(state)
   # |q=1⟩ : (1+0j)

   # Apply the QFT
   op = ps.QFT("q")
   op(state)

   print("\nAfter QFT:")
   ps.pprint(state)
   # Produces a superposition with uniformly distributed phases

   # Undo (inverse transform)
   op.dag(state)

InverseQFT
----------

.. autoclass:: pysparq.InverseQFT
   :members:
   :undoc-members:

**Operation**: Performs the inverse quantum Fourier transform on an integer register

**Purpose**: Undo a QFT or extract phase information

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Create a superposition
   ps.Hadamard_Int_Full("q")(state)

   # Apply the inverse QFT
   ps.InverseQFT("q")(state)

   # The inverse QFT and the QFT are inverses of each other
   ps.QFT("q")(state)

---

Use Cases
---------

Period Finding (Shor's Algorithm)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps
   import numpy as np

   ps.System.clear()

   # Phase register
   n_bits = 8
   ps.System.add_register("phase", ps.UnsignedInteger, n_bits)

   state = ps.SparseState()

   # Simulate a phase estimation result (assume period r = 5)
   # |phase⟩ contains periodic phase information
   ps.Hadamard_Int_Full("phase")(state)

   # Apply the inverse QFT to extract the period
   ps.InverseQFT("phase")(state)

   # Measure the phase register to determine the period
   # ...

Phase Estimation
^^^^^^^^^^^^^^^^

.. code-block:: python

   # Assume a phase estimation circuit already exists
   # The inverse QFT converts phase information into the computational basis

   # 1. Prepare the initial state
   ps.Hadamard_Int_Full("estimate")(state)

   # 2. Apply the controlled-unitary operation
   # controlled_U(...)(state)

   # 3. Extract the phase with the inverse QFT
   ps.InverseQFT("estimate")(state)

   # 4. Measure to obtain the phase estimate
   # ...

Quantum Signal Processing
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # The QFT is used for frequency-domain analysis
   ps.System.add_register("signal", ps.UnsignedInteger, 16)

   state = ps.SparseState()
   # Load the signal data...

   # Transform to the frequency domain
   ps.QFT("signal")(state)

   # Frequency-domain processing...

   # Transform back to the time domain
   ps.InverseQFT("signal")(state)

Performance Considerations
--------------------------

- **Number of basis states**: When the QFT acts on a superposition, it produces :math:`2^n` basis states
- **Memory limits**: For large n (e.g. n > 20), memory problems may arise
- **Sparse optimization**: PySparQ stores only the basis states with nonzero amplitudes, which can be efficient for certain inputs

.. note::

   For superpositions that are not complete, the sparse implementation of the QFT can be more efficient than full-state simulation.

Practical Examples
------------------

Full QFT + InverseQFT Cycle
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps
   import numpy as np

   ps.System.clear()

   n = 3
   ps.System.add_register("q", ps.UnsignedInteger, n)

   state = ps.SparseState()

   # Initial value
   initial_value = 5
   ps.Init_Unsafe("q", initial_value)(state)

   print("Initial state:")
   ps.pprint(state)

   # QFT
   ps.QFT("q")(state)
   print("\nAfter QFT:")
   ps.pprint(state)

   # InverseQFT
   ps.InverseQFT("q")(state)
   print("\nAfter InverseQFT:")
   ps.pprint(state)
   # Restored to |q=5⟩

-----

中文版
===

量子傅里叶变换 (QFT)
===================

量子傅里叶变换是许多量子算法的核心组件。

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

概述
----

.. list-table:: QFT 算子总览
   :header-rows: 1

   * - 算子
     - 操作
     - 幺正类
   * - ``QFT``
     - 量子傅里叶变换
     - BaseOperator
   * - ``InverseQFT``
     - 逆量子傅里叶变换
     - BaseOperator

数学定义
--------

量子傅里叶变换定义：

.. math::

   QFT|x\rangle = \frac{1}{\sqrt{2^n}} \sum_{y=0}^{2^n-1} e^{2\pi i xy / 2^n} |y\rangle

逆量子傅里叶变换：

.. math::

   QFT^\dagger|y\rangle = \frac{1}{\sqrt{2^n}} \sum_{x=0}^{2^n-1} e^{-2\pi i xy / 2^n} |x\rangle

---

QFT
---

.. autoclass:: pysparq.QFT
   :members:
   :undoc-members:

**操作**: 对整数寄存器执行量子傅里叶变换

**类型约束**: ``UnsignedInteger`` 或 ``SignedInteger``

**位约束**: 寄存器大小决定变换维度

.. code-block:: python

   import pysparq as ps

   ps.System.clear()

   # 4 比特寄存器
   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # 初始化为基态 |1⟩
   ps.Init_Unsafe("q", 1)(state)

   print("初始状态:")
   ps.pprint(state)
   # |q=1⟩ : (1+0j)

   # 应用 QFT
   op = ps.QFT("q")
   op(state)

   print("\nQFT 后:")
   ps.pprint(state)
   # 产生相位均匀分布的叠加态

   # 撤销（逆变换）
   op.dag(state)

InverseQFT
---------

.. autoclass:: pysparq.InverseQFT
   :members:
   :undoc-members:

**操作**: 对整数寄存器执行逆量子傅里叶变换

**用途**: 撤销 QFT 或解析相位信息

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # 创建叠加态
   ps.Hadamard_Int_Full("q")(state)

   # 应用逆 QFT
   ps.InverseQFT("q")(state)

   # 逆 QFT 和 QFT 互为逆运算
   ps.QFT("q")(state)

---

使用场景
--------

周期发现（Shor 算法）
^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps
   import numpy as np

   ps.System.clear()

   # 相位寄存器
   n_bits = 8
   ps.System.add_register("phase", ps.UnsignedInteger, n_bits)

   state = ps.SparseState()

   # 模拟相位估计结果（假设周期 r = 5）
   # |phase⟩ 包含周期性相位信息
   ps.Hadamard_Int_Full("phase")(state)

   # 应用逆 QFT 解析周期
   ps.InverseQFT("phase")(state)

   # 观测相位寄存器，确定周期
   # ...

相位估计
^^^^^^^^

.. code-block:: python

   # 假设已有相位估计电路
   # 逆 QFT 将相位信息转换到计算基

   # 1. 制备初态
   ps.Hadamard_Int_Full("estimate")(state)

   # 2. 执行受控幺正操作
   # controlled_U(...)(state)

   # 3. 逆 QFT 提取相位
   ps.InverseQFT("estimate")(state)

   # 4. 测量得到相位估计
   # ...

量子信号处理
^^^^^^^^^^^^

.. code-block:: python

   # QFT 用于频域分析
   ps.System.add_register("signal", ps.UnsignedInteger, 16)

   state = ps.SparseState()
   # 加载信号数据...

   # 变换到频域
   ps.QFT("signal")(state)

   # 频域处理...

   # 变换回时域
   ps.InverseQFT("signal")(state)

性能考虑
--------

- **基态数量**: QFT 作用于叠加态时，会产生 :math:`2^n` 个基态
- **内存限制**: 当 n 较大时（如 n > 20），可能导致内存问题
- **稀疏优化**: PySparQ 只存储非零振幅基态，对于某些输入可能高效

.. note::

   对于非完整的叠加态，QFT 的稀疏实现可能比全态模拟更高效。

实际示例
--------

完整 QFT + InverseQFT 循环
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps
   import numpy as np

   ps.System.clear()

   n = 3
   ps.System.add_register("q", ps.UnsignedInteger, n)

   state = ps.SparseState()

   # 初始值
   initial_value = 5
   ps.Init_Unsafe("q", initial_value)(state)

   print("初始状态:")
   ps.pprint(state)

   # QFT
   ps.QFT("q")(state)
   print("\nQFT 后:")
   ps.pprint(state)

   # InverseQFT
   ps.InverseQFT("q")(state)
   print("\nInverseQFT 后:")
   ps.pprint(state)
   # 恢复到 |q=5⟩
