快速入门
========

寄存器级编程
------------

PySparQ 采用":doc:`寄存器级编程 </guide/core_concepts/index>`"范式。与从单个门组合电路不同，您直接对命名的量子寄存器进行操作。操作层面从量子比特上升到了量子寄存器，几乎所有操作都以寄存器为单位。

基本工作流程
------------

1. 调用 :meth:`System.clear() <pysparq.System.clear>` 清理静态状态
2. :doc:`声明寄存器 </guide/core_concepts/register_management>`（名称、类型、比特数）
3. 创建 :class:`SparseState() <pysparq.SparseState>`——默认构造函数自动创建 ``|0...0⟩`` 初态
4. 应用量子操作（见 :ref:`算子参考 <operator-reference>`）
5. 通过 :doc:`部分迹算子 </operators/partial_trace>` 读取测量结果

示例：量子加法
--------------

.. code-block:: python

   import pysparq as ps

   # 第一步：清理静态状态
   ps.System.clear()

   # 第二步：声明寄存器
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   # 第三步：创建稀疏量子态（自动创建 |a=0, b=0⟩ 初态）
   state = ps.SparseState()

   # 第四步：将寄存器置于叠加态
   ps.Hadamard_Int("a", 4)(state)
   ps.Hadamard_Int("b", 4)(state)

   # 量子加法：result = a + b
   ps.Add_UInt_UInt("a", "b", "result")(state)

   # 状态现在包含所有可能求和结果的叠加态
   ps.pprint(state)

示例使用了 :class:`Hadamard_Int <pysparq.Hadamard_Int>` 将两个寄存器置于叠加态，并用 :class:`Add_UInt_UInt <pysparq.Add_UInt_UInt>` 完成求和。
详见 :doc:`Hadamard 操作 </operators/hadamard>`、:doc:`算术算子 </operators/arithmetic>` 与 :doc:`调试工具 </operators/debug>` （``pprint``）。

条件操作
--------

操作可以根据其他寄存器的值进行条件控制（完整参考见 :ref:`条件执行 <conditional-operations>`）：

.. code-block:: python

   # 添加控制寄存器
   ps.AddRegister("control", ps.Boolean, 1)(state)

   # 仅当 control 为 |1> 时应用操作
   ps.Add_UInt_UInt("a", "b").conditioned_by_nonzeros("control")(state)

控制类型
^^^^^^^^

- :ref:`conditioned_by_nonzeros(reg) <conditional-operations>` - 当寄存器非零时执行
- :ref:`conditioned_by_all_ones(reg) <conditional-operations>` - 当寄存器全为1时执行
- :ref:`conditioned_by_bit(reg, pos) <conditional-operations>` - 当特定位为1时执行
- :ref:`conditioned_by_value(reg, val) <conditional-operations>` - 当寄存器值等于特定值时执行

下一步
------

在 :doc:`PySparQ 快速入门 notebook </notebooks/01_quickstart>` 中以交互方式尝试同样的工作流，然后继续阅读 :doc:`核心概念 </guide/core_concepts/index>` 与 :doc:`示例 </guide/examples>`。
