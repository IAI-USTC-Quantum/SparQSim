Python Binding Layer (C++ Side)
===============================

pybind11 Binding Entry (the ``pysparq._core`` module generated from ``PySparQ/core.cpp``)
------------------------------------------------------------------------------------------

.. doxygenfile:: core.h
   :project: SparQ

Binding Helper Macros (``PySparQ/include/BindUtils.h``)
-------------------------------------------------------

.. doxygenfile:: BindUtils.h
   :project: SparQ

Dynamic Operator Loader (``PySparQ/pysparq/dynamic_operator/include/dynamic_operator_loader.h``)
------------------------------------------------------------------------------------------------

.. doxygenfile:: dynamic_operator_loader.h
   :project: SparQ

.. note::

   For the final interface that the binding layer exposes to Python, consult :doc:`/api/index` directly;
   for a usage guide on dynamic operators (runtime-compiled C++ operators), see :doc:`/guide/dynamic_operators`.

----

中文版
===

Python 绑定层（C++ 侧）
=======================

pybind11 绑定入口（``PySparQ/core.cpp`` 生成的 ``pysparq._core`` 模块）
----------------------------------------------------------------------

.. doxygenfile:: core.h
   :project: SparQ

绑定辅助宏（``PySparQ/include/BindUtils.h``）
---------------------------------------------

.. doxygenfile:: BindUtils.h
   :project: SparQ

动态算子加载器（``PySparQ/pysparq/dynamic_operator/include/dynamic_operator_loader.h``）
----------------------------------------------------------------------------------------

.. doxygenfile:: dynamic_operator_loader.h
   :project: SparQ

.. note::

   绑定层暴露给 Python 的最终接口请直接查阅 :doc:`/api/index`；
   动态算子（运行时编译 C++ 算子）的使用指南见 :doc:`/guide/dynamic_operators`。
