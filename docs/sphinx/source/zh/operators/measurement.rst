线路中测量与随机种子
====================

测量算子对 :class:`SparseState <pysparq.SparseState>` 执行**非幺正**操作：Born 规则采样、主动复位与只读概率查询。它们是量子算法中线路中测量、动态分支与经典读出的构建模块。与把寄存器坍缩以提取读出值的 :doc:`部分追迹算子 </operators/partial_trace>` 不同，``MeasureZ`` 采样一个测量结果，``Reset`` 制备确定的经典值，``Probability`` 在不修改量子态的情况下查询结果分布。

所有采样算子都从一个**可设种子的全局随机引擎**中抽取随机数（见 `随机种子函数`_），因此可以让运行结果可复现，便于测试。

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

概述
----

.. list-table:: 测量算子总览
   :header-rows: 1

   * - 算子
     - 操作
     - 是否修改态
   * - :class:`MeasureZ <pysparq.MeasureZ>`
     - Z 基投影测量（Born 规则采样 + 坍缩）
     - 是（坍缩并重新归一化）
   * - :class:`Reset <pysparq.Reset>`
     - 主动复位：先测量，再强制为确定的经典值
     - 是
   * - :class:`Probability <pysparq.Probability>`
     - 对指定寄存器值的只读概率查询
     - 否

基本用法
--------

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)
   state = ps.SparseState()
   ps.Hadamard_Int("q", 2)(state)   # |0..3> 上的均匀叠加

   # 可复现采样
   ps.set_seed(0)
   outcome, prob = ps.MeasureZ("q")(state)      # 采样 + 坍缩
   print(outcome, prob)

   # 主动复位为确定值（默认为 0）
   measured = ps.Reset("q", 3)(state)           # 强制 q = 3

   # 只读概率查询
   p = ps.Probability("q", 3)(state)            # q == 3 的概率
   dist = ps.Probability.distribution(state, "q")  # 完整结果分布

由于这些操作是非幺正的，它们不提供 ``dag()``，也不能用于要求可逆性的 :ref:`条件操作 <conditional-operations>` 块中。

API 参考
--------

.. autoclass:: pysparq.MeasureZ
   :members:
   :undoc-members:

.. autoclass:: pysparq.Reset
   :members:
   :undoc-members:

.. autoclass:: pysparq.Probability
   :members:
   :undoc-members:

随机种子函数
------------

采样算子（:class:`MeasureZ <pysparq.MeasureZ>` 与 :class:`Reset <pysparq.Reset>`）与 :doc:`部分追迹算子 </operators/partial_trace>` 共用同一个全局随机引擎。采样前先设置种子即可让结果可复现——动态执行器的确定性重放与测试都依赖这一点。

.. autofunction:: pysparq.set_seed

.. autofunction:: pysparq.get_seed

.. autofunction:: pysparq.reseed

.. autofunction:: pysparq.time_seed
