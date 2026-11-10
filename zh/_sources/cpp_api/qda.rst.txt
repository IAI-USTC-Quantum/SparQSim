离散绝热（DiscreteAdiabatic/）
===============================

QDA 通过块编码求解量子线性方程组；算子级超参数见 :doc:`算子 </guide/core_concepts/operators>`。

QDA 基础组件（``SparQ_Algorithm/include/DiscreteAdiabatic/qda_fundamental.h``）
-------------------------------------------------------------------------------

.. doxygenfile:: qda_fundamental.h
   :project: SparQ

三对角 QDA 求解器（``SparQ_Algorithm/include/DiscreteAdiabatic/qda_tridiagonal.h``）
-------------------------------------------------------------------------------------

.. doxygenfile:: qda_tridiagonal.h
   :project: SparQ

基于 QRAM 的 QDA 求解器（``SparQ_Algorithm/include/DiscreteAdiabatic/qda_via_QRAM.h``）
----------------------------------------------------------------------------------------

.. doxygenfile:: qda_via_QRAM.h
   :project: SparQ
