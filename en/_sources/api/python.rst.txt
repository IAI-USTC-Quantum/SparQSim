Python API Reference
====================

PySparQ exposes all quantum operations as Python classes and functions through the :mod:`pysparq` module.

Core Classes
------------

Concept introductions: :doc:`The System Class </guide/core_concepts/system>` and :doc:`The SparseState Class </guide/core_concepts/sparse_state>`.

.. autoclass:: pysparq._core.System
   :members:
   :show-inheritance:

.. autoclass:: pysparq._core.SparseState
   :members:
   :show-inheritance:

Quantum Arithmetic Operators
----------------------------

Detailed usage: :doc:`Arithmetic Operators </operators/arithmetic>`.

.. autofunction:: pysparq._core.Add_UInt_UInt

.. autofunction:: pysparq._core.Add_UInt_ConstUInt

.. autofunction:: pysparq._core.Mult_UInt_ConstUInt

Quantum Gates
-------------

Detailed usage: :doc:`Basic Quantum Gates </operators/gates>`, :doc:`Hadamard Operations </operators/hadamard>`, and :doc:`QFT </operators/qft>`.

.. autofunction:: pysparq._core.Hadamard_Int

.. autofunction:: pysparq._core.Hadamard_Bool

.. autofunction:: pysparq._core.QFT

.. autofunction:: pysparq._core.InverseQFT

.. autofunction:: pysparq._core.X_Bool

.. autofunction:: pysparq._core.Y_Bool

.. autofunction:: pysparq._core.Z_Bool

QRAM Operations
---------------

Detailed usage: :doc:`QRAM Operators </operators/qram_ops>`.

.. autofunction:: pysparq._core.QRAMLoad

.. autofunction:: pysparq._core.QRAMLoadFast

.. autofunction:: pysparq._core.QRAMCircuit_qutrit

State Management
----------------

Detailed usage: :doc:`System Operations </operators/system_ops>`, :doc:`Partial Trace </operators/partial_trace>`, and :doc:`Dark Magic Operations </operators/dark_magic>` (``Normalize``).

.. autofunction:: pysparq._core.AddRegister

.. autofunction:: pysparq._core.RemoveRegister

.. autofunction:: pysparq._core.Push

.. autofunction:: pysparq._core.Pop

.. autofunction:: pysparq._core.Normalize

.. autofunction:: pysparq._core.CheckNormalization

Complete API Documentation
--------------------------

For the full API documentation (all classes, functions, and methods), see :ref:`modindex`.
