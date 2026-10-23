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
