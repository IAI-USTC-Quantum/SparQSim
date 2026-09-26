Python API 参考
===============

PySparQ 通过 :mod:`pysparq` 模块将所有量子操作暴露为 Python 类和函数。

核心类
------

概念介绍见 :doc:`System 类 </guide/core_concepts/system>` 与 :doc:`SparseState 类 </guide/core_concepts/sparse_state>`。

.. autoclass:: pysparq._core.System
   :members:
   :show-inheritance:

.. autoclass:: pysparq._core.SparseState
   :members:
   :show-inheritance:

量子算术算子
------------

详细用法见 :doc:`算术算子 </operators/arithmetic>`。

.. autofunction:: pysparq._core.Add_UInt_UInt

.. autofunction:: pysparq._core.Add_UInt_ConstUInt

.. autofunction:: pysparq._core.Mult_UInt_ConstUInt

量子门
------

详细用法见 :doc:`基本量子门 </operators/gates>`、:doc:`Hadamard 操作 </operators/hadamard>` 与 :doc:`QFT </operators/qft>`。

.. autofunction:: pysparq._core.Hadamard_Int

.. autofunction:: pysparq._core.Hadamard_Bool

.. autofunction:: pysparq._core.QFT

.. autofunction:: pysparq._core.InverseQFT

.. autofunction:: pysparq._core.X_Bool

.. autofunction:: pysparq._core.Y_Bool

.. autofunction:: pysparq._core.Z_Bool

QRAM 操作
---------

详细用法见 :doc:`QRAM 算子 </operators/qram_ops>`。

.. autofunction:: pysparq._core.QRAMLoad

.. autofunction:: pysparq._core.QRAMLoadFast

.. autofunction:: pysparq._core.QRAMCircuit_qutrit

状态管理
--------

详细用法见 :doc:`系统操作 </operators/system_ops>`、:doc:`部分追迹 </operators/partial_trace>` 与 :doc:`黑魔法操作 </operators/dark_magic>`（``Normalize``）。

.. autofunction:: pysparq._core.AddRegister

.. autofunction:: pysparq._core.RemoveRegister

.. autofunction:: pysparq._core.Push

.. autofunction:: pysparq._core.Pop

.. autofunction:: pysparq._core.Normalize

.. autofunction:: pysparq._core.CheckNormalization

完整 API 文档
-------------

完整的 API 文档（所有类、函数和方法），请参见 :ref:`modindex`。
