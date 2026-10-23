QRAM 操作
=========

Python 侧的用法见 :doc:`QRAM 算子 </operators/qram_ops>`。

量子随机存取存储器（``SparQ/include/qram.h``）
----------------------------------------------

.. doxygenfile:: qram.h
   :project: SparQ

.. note::

   QRAM 电路核心（``QRAMCircuit`` 的 qutrit/qubit 实现）位于
   `QRAM-Simulator 仓库 <https://github.com/IAI-USTC-Quantum/QRAM-Simulator>`_
   （本仓库 ``extern/qram-simulator`` submodule），其文档请参阅该仓库。
