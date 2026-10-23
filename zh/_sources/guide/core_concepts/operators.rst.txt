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

2. **作用阶段** — 将已构造的算子施加到 :class:`SparseState <pysparq.SparseState>` 上，完成量子态变换。

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
- **组合**：算子可以附加条件（:ref:`conditioned_by <conditional-operations>`），在构造后灵活调整控制逻辑
- **可逆**：构造好的算子支持 :meth:`dag() <pysparq.BaseOperator.dag>` 方法执行逆操作

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

一些高级算子需要算法层面的超参数，例如 :doc:`哈密顿量模拟 </cpp_api/algorithms>` 中的 :math:`\kappa`（条件数）、:math:`\epsilon`（精度），或 :doc:`QDA </cpp_api/qda>` 中的步长参数 :math:`s`。

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

对于 :class:`BaseOperator <pysparq.BaseOperator>` 派生的算子，使用 ``dag()`` 执行逆变换：

.. code-block:: python

   op = ps.ShiftLeft_InPlace("reg", 2)
   op(state)       # 左移 2 位
   op.dag(state)   # 右移 2 位（撤销）

对于 :class:`SelfAdjointOperator <pysparq.SelfAdjointOperator>`，再次调用 ``op(state)`` 即为逆操作（:math:`U^\dagger = U`）。

条件作用
^^^^^^^^

所有算子支持链式条件方法（:ref:`条件执行 <conditional-operations>`），实现受控操作：

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
   * - :doc:`量子算术 </operators/arithmetic>`
     - 加法、乘法、移位、比较等寄存器级运算
     - ``quantum_arithmetic.h``
   * - :doc:`基本量子门 </operators/gates>`
     - Pauli 门、相位门、旋转门、通用门
     - ``basic_gates.h``
   * - :doc:`Hadamard 操作 </operators/hadamard>`
     - 整数/布尔/部分量子比特的 Hadamard 变换
     - ``hadamard.h``
   * - :doc:`QFT </operators/qft>`
     - 量子傅里叶变换及其逆变换
     - ``qft.h``
   * - :doc:`QRAM 算子 </operators/qram_ops>`
     - 量子随机存取存储器的加载操作
     - ``qram.h``
   * - :doc:`条件旋转 </operators/condrot>`
     - 基于寄存器值控制量子比特旋转
     - ``condrot.h``
   * - :doc:`相位与反射 </operators/phase_ops>`
     - 条件相位翻转、全局相位、Grover 反射
     - ``parallel_phase_operations.h``
   * - :doc:`旋转与态制备 </operators/rot_state_prep>`
     - 任意维幺正旋转、量子态制备
     - ``rot.h``
   * - :doc:`系统操作 </operators/system_ops>`
     - 寄存器栈管理、零振幅清理、系统拆分/合并
     - ``system_operations.h``
   * - :doc:`部分追迹 </operators/partial_trace>`
     - 测量、选择性坍缩、范围坍缩
     - ``partial_trace.h``
   * - :doc:`线路中测量 </operators/measurement>`
     - Born 规则测量、主动复位、概率查询
     - ``measurement.h``
   * - :doc:`排序 </operators/sort_ops>`
     - 按键值、振幅等维度排序基态
     - ``sort_state.h``
   * - :doc:`黑魔法操作 </operators/dark_magic>`
     - 直接修改量子态的不安全操作
     - ``dark_magic.h``
   * - :doc:`调试工具 </operators/debug>`
     - 归一化检查、NaN 检测、状态打印
     - ``debugger.h``

各类算子的详细 API 和用法请参阅 :ref:`算子参考 <operator-reference>` 章节。

概念：算子与流程控制类
----------------------

SparQ 的代码中区分两类不同层次的概念：

**算子（Operator）** 继承 :doc:`BaseOperator </cpp_api/core>` 或 :doc:`SelfAdjointOperator </cpp_api/core>`，直接操控量子态（``SparseState``），与量子算法和量子线路高度相关。例如 :class:`CondRot_Fixed_Bool <pysparq.CondRot_Fixed_Bool>`、``T``（CKS 中的态制备算子）、``SparseMatrixOracle1`` 等。

**流程控制类（Flow-Control / Algorithm Classes）** 不直接操控量子态，而是持有寄存器、编排算子执行、管理状态初始化和迭代。它们的职责是 **测试和验证** 量子过程是否正确，而非实现量子门。例如：

- ``QuantumWalkNSteps``（CKS）：管理多步量子游走，注册创建和环境初始化
- ``LCU_Container``（CKS）：管理 Chebyshev LCU 迭代循环
- ``WalkS``（:doc:`QDA </cpp_api/qda>`）：在 QDA 算法中协调块编码和态制备

这种区分的意义在于：算子是可组合、可测试的最小单元；流程控制类则是将算子串联起来用于特定算法流程的粘合剂。流程控制类不应继承 ``Operator`` 基类，因为它们的职责是 **编排** 而非 **作用**。
