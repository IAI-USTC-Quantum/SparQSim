Operators
=========

The operator (Operator) is the core abstraction for quantum operations in SparQ. Unlike traditional quantum circuit simulators, which describe quantum algorithms as sequences of gates, SparQ abstracts every transformation as a **callable object** (a C++ functor), separating the definition of an operator from the objects it acts upon.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

The Two-Phase Model
-------------------

SparQ operators follow a two-phase model of **construction → application**:

1. **Construction phase** — specify which registers the operator acts on, together with the relevant parameters. No quantum state is involved in this phase.

2. **Application phase** — apply the constructed operator to a ``SparseState``, completing the quantum state transformation.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Phase 1: construction — specify registers and parameters
   add_op = ps.Add_UInt_UInt("a", "b", "result")

   # Phase 2: application — act on the quantum state
   add_op(state)

The benefits of this separation:

- **Reuse**: the same operator object can be applied to different quantum states repeatedly, without reconstructing it
- **Composition**: operators can carry conditions (conditioned_by), so the control logic can be adjusted flexibly after construction
- **Reversibility**: a constructed operator supports the ``dag()`` method to perform the inverse operation

Construction Parameters
-----------------------

Construction parameters determine the behavior of an operator and usually fall into the following categories:

Register Identifiers
^^^^^^^^^^^^^^^^^^^^

Almost every operator needs to specify its target registers. This can be done with a name string or an integer index:

.. code-block:: python

   # By name (recommended)
   op = ps.Add_UInt_UInt("a", "b", "result")

   # By index
   op = ps.Add_UInt_UInt(0, 1, 2)

Constant Parameters
^^^^^^^^^^^^^^^^^^^

Some operators accept classical constants as parameters, for example the addend in an addition or the multiplier in a multiplication:

.. code-block:: python

   # Constant addition: specify the constant 7
   ps.Add_ConstUInt_InPlace("counter", 7)

   # Constant multiplication: specify the multiplier 3
   ps.Mult_UInt_ConstUInt("input", 3, "result")

Hyperparameters
^^^^^^^^^^^^^^^

Some advanced operators require algorithm-level hyperparameters, such as :math:`\kappa` (condition number) and :math:`\epsilon` (precision) in Hamiltonian simulation, or the step-size parameter :math:`s` in QDA.

Bit Indices and Angles
^^^^^^^^^^^^^^^^^^^^^^

Single-qubit gates need to specify which bit of the register they act on, and rotation gates need the rotation angle:

.. code-block:: python

   # Apply an X gate to bit 0 of register "q"
   ps.X_Bool("q", 0)

   # Rotate by π/4 around the X axis
   ps.RX_Bool("q", 0, np.pi / 4)

Applying Operators
------------------

Direct Application
^^^^^^^^^^^^^^^^^^

After construction, the function-call syntax ``op(state)`` applies the operator to a ``SparseState``:

.. code-block:: python

   op = ps.Hadamard_Int("reg", 4)
   op(state)

Inverse Application
^^^^^^^^^^^^^^^^^^^

For operators derived from ``BaseOperator``, use ``dag()`` to perform the inverse transformation:

.. code-block:: python

   op = ps.ShiftLeft_InPlace("reg", 2)
   op(state)       # shift left by 2 bits
   op.dag(state)   # shift right by 2 bits (undo)

For ``SelfAdjointOperator``, calling ``op(state)`` again is the inverse operation (:math:`U^\dagger = U`).

Conditional Application
^^^^^^^^^^^^^^^^^^^^^^^

All operators support chained conditional methods to implement controlled operations:

.. code-block:: python

   op = ps.Add_UInt_UInt("a", "b", "result")

   # Execute only when "ctrl" is non-zero
   op.conditioned_by_nonzeros("ctrl")(state)

   # Stacking multiple conditions
   op.conditioned_by_nonzeros(["ctrl1", "ctrl2"])(state)

Operator Categories
-------------------

SparQ's operators are divided into the following major categories by function:

.. list-table:: Operator category overview
   :header-rows: 1

   * - Category
     - Description
     - Header file
   * - Quantum arithmetic
     - Register-level operations such as addition, multiplication, shifting, and comparison
     - ``quantum_arithmetic.h``
   * - Basic quantum gates
     - Pauli gates, phase gates, rotation gates, general-purpose gates
     - ``basic_gates.h``
   * - Hadamard operations
     - Hadamard transforms over integers / booleans / partial qubits
     - ``hadamard.h``
   * - QFT
     - Quantum Fourier transform and its inverse
     - ``qft.h``
   * - QRAM operators
     - Quantum random access memory load operations
     - ``qram.h``
   * - Conditional rotation
     - Rotating qubits conditioned on register values
     - ``condrot.h``
   * - Phase and reflection
     - Conditional phase flips, global phase, Grover reflection
     - ``parallel_phase_operations.h``
   * - Rotation and state preparation
     - Unitary rotations of arbitrary dimension, quantum state preparation
     - ``rot.h``
   * - System operations
     - Register stack management, zero-amplitude cleanup, system split/merge
     - ``system_operations.h``
   * - Partial trace
     - Measurement, selective collapse, range collapse
     - ``partial_trace.h``
   * - Sorting
     - Sorting basis states by key value, amplitude, and other dimensions
     - ``sort_state.h``
   * - Dark magic operations
     - Unsafe operations that directly modify the quantum state
     - ``dark_magic.h``
   * - Debugging tools
     - Normalization checks, NaN detection, state printing
     - ``debugger.h``

For the detailed API and usage of each category of operators, see the :ref:`Operator Reference <算子参考>` section.

Concept: Operators vs. Flow-Control Classes
-------------------------------------------

SparQ's codebase distinguishes between two concepts at different levels:

**Operators** inherit from ``BaseOperator`` or ``SelfAdjointOperator`` and directly manipulate the quantum state (``SparseState``); they are closely tied to quantum algorithms and quantum circuits. Examples include ``CondRot_Fixed_Bool``, ``T`` (the state-preparation operator in CKS), and ``SparseMatrixOracle1``.

**Flow-control classes (Flow-Control / Algorithm Classes)** do not manipulate the quantum state directly; instead they hold registers, orchestrate operator execution, and manage state initialization and iteration. Their responsibility is to **test and verify** whether a quantum procedure is correct, not to implement quantum gates. For example:

- ``QuantumWalkNSteps`` (CKS): manages multi-step quantum walks, register creation, and environment initialization
- ``LCU_Container`` (CKS): manages the Chebyshev LCU iteration loop
- ``WalkS`` (QDA): coordinates block encoding and state preparation in the QDA algorithm

The point of this distinction: operators are the minimal composable, testable units, while flow-control classes are the glue that chains operators together for a particular algorithmic workflow. Flow-control classes should not inherit from the ``Operator`` base class, because their responsibility is **orchestration** rather than **application**.

----

中文版
===

算子
====

算子（Operator）是 SparQ 中对量子操作的核心抽象。与传统量子电路模拟器以门序列描述量子算法不同，SparQ 将一切变换抽象为 **可调用对象**（C++ functor），实现了算子定义与作用对象的分离。

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

两阶段模型
----------

SparQ 的算子遵循 **构造 → 作用** 的两阶段模型：

1. **构造阶段** — 指定算子作用于哪些寄存器以及相关参数。此阶段不涉及任何量子态。

2. **作用阶段** — 将已构造的算子施加到 ``SparseState`` 上，完成量子态变换。

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # 阶段 1：构造 — 指定寄存器和参数
   add_op = ps.Add_UInt_UInt("a", "b", "result")

   # 阶段 2：作用 — 施加到量子态
   add_op(state)

这种分离带来的好处：

- **复用**：同一个算子对象可以反复作用于不同的量子态，无需重新构造
- **组合**：算子可以附加条件（conditioned_by），在构造后灵活调整控制逻辑
- **可逆**：构造好的算子支持 ``dag()`` 方法执行逆操作

构造参数
--------

构造参数决定了算子的行为，通常包括以下几类：

寄存器标识
^^^^^^^^^^

几乎所有算子都需要指定目标寄存器。可以用名称字符串或下标整数：

.. code-block:: python

   # 使用名称（推荐）
   op = ps.Add_UInt_UInt("a", "b", "result")

   # 使用下标
   op = ps.Add_UInt_UInt(0, 1, 2)

常量参数
^^^^^^^^

部分算子接受经典常量作为参数，例如加法中的常数、乘法中的乘数：

.. code-block:: python

   # 常量加法：指定常数 7
   ps.Add_ConstUInt_InPlace("counter", 7)

   # 常量乘法：指定乘数 3
   ps.Mult_UInt_ConstUInt("input", 3, "result")

超参数
^^^^^^

一些高级算子需要算法层面的超参数，例如哈密顿量模拟中的 :math:`\kappa`（条件数）、:math:`\epsilon`（精度），或 QDA 中的步长参数 :math:`s`。

位索引与角度
^^^^^^^^^^^^

单量子比特门需要指定作用在寄存器中的哪一位，旋转门需要指定旋转角度：

.. code-block:: python

   # 在寄存器 "q" 的第 0 位作用 X 门
   ps.X_Bool("q", 0)

   # 绕 X 轴旋转 π/4
   ps.RX_Bool("q", 0, np.pi / 4)

作用方式
--------

直接作用
^^^^^^^^

构造后通过函数调用语法 ``op(state)`` 将算子施加到 ``SparseState``：

.. code-block:: python

   op = ps.Hadamard_Int("reg", 4)
   op(state)

逆作用
^^^^^^

对于 ``BaseOperator`` 派生的算子，使用 ``dag()`` 执行逆变换：

.. code-block:: python

   op = ps.ShiftLeft_InPlace("reg", 2)
   op(state)       # 左移 2 位
   op.dag(state)   # 右移 2 位（撤销）

对于 ``SelfAdjointOperator``，再次调用 ``op(state)`` 即为逆操作（:math:`U^\dagger = U`）。

条件作用
^^^^^^^^

所有算子支持链式条件方法，实现受控操作：

.. code-block:: python

   op = ps.Add_UInt_UInt("a", "b", "result")

   # 当 "ctrl" 非零时才执行
   op.conditioned_by_nonzeros("ctrl")(state)

   # 多条件叠加
   op.conditioned_by_nonzeros(["ctrl1", "ctrl2"])(state)

算子分类
--------

SparQ 的算子按功能分为以下几大类：

.. list-table:: 算子分类概览
   :header-rows: 1

   * - 分类
     - 说明
     - 对应头文件
   * - 量子算术
     - 加法、乘法、移位、比较等寄存器级运算
     - ``quantum_arithmetic.h``
   * - 基本量子门
     - Pauli 门、相位门、旋转门、通用门
     - ``basic_gates.h``
   * - Hadamard 操作
     - 整数/布尔/部分量子比特的 Hadamard 变换
     - ``hadamard.h``
   * - QFT
     - 量子傅里叶变换及其逆变换
     - ``qft.h``
   * - QRAM 算子
     - 量子随机存取存储器的加载操作
     - ``qram.h``
   * - 条件旋转
     - 基于寄存器值控制量子比特旋转
     - ``condrot.h``
   * - 相位与反射
     - 条件相位翻转、全局相位、Grover 反射
     - ``parallel_phase_operations.h``
   * - 旋转与态制备
     - 任意维幺正旋转、量子态制备
     - ``rot.h``
   * - 系统操作
     - 寄存器栈管理、零振幅清理、系统拆分/合并
     - ``system_operations.h``
   * - 部分追迹
     - 测量、选择性坍缩、范围坍缩
     - ``partial_trace.h``
   * - 排序
     - 按键值、振幅等维度排序基态
     - ``sort_state.h``
   * - 黑魔法操作
     - 直接修改量子态的不安全操作
     - ``dark_magic.h``
   * - 调试工具
     - 归一化检查、NaN 检测、状态打印
     - ``debugger.h``

各类算子的详细 API 和用法请参阅 :ref:`算子参考 <算子参考>` 章节。

概念：算子与流程控制类
----------------------

SparQ 的代码中区分两类不同层次的概念：

**算子（Operator）** 继承 ``BaseOperator`` 或 ``SelfAdjointOperator``，直接操控量子态（``SparseState``），与量子算法和量子线路高度相关。例如 ``CondRot_Fixed_Bool``、``T``（CKS 中的态制备算子）、``SparseMatrixOracle1`` 等。

**流程控制类（Flow-Control / Algorithm Classes）** 不直接操控量子态，而是持有寄存器、编排算子执行、管理状态初始化和迭代。它们的职责是 **测试和验证** 量子过程是否正确，而非实现量子门。例如：

- ``QuantumWalkNSteps``（CKS）：管理多步量子游走，注册创建和环境初始化
- ``LCU_Container``（CKS）：管理 Chebyshev LCU 迭代循环
- ``WalkS``（QDA）：在 QDA 算法中协调块编码和态制备

这种区分的意义在于：算子是可组合、可测试的最小单元；流程控制类则是将算子串联起来用于特定算法流程的粘合剂。流程控制类不应继承 ``Operator`` 基类，因为它们的职责是 **编排** 而非 **作用**。
