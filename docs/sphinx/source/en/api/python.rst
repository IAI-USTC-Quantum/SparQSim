Python API Reference
====================

PySparQ exposes all quantum operations as Python classes and functions through the :mod:`pysparq` module.

Core Classes
------------

.. autoclass:: pysparq._core.System
   :members:
   :show-inheritance:

.. autoclass:: pysparq._core.SparseState
   :members:
   :show-inheritance:

Quantum Arithmetic Operators
----------------------------

.. autofunction:: pysparq._core.Add_UInt_UInt

.. autofunction:: pysparq._core.Add_UInt_ConstUInt

.. autofunction:: pysparq._core.Add_ConstUInt

.. autofunction:: pysparq._core.Mult_UInt_ConstUInt

.. autofunction:: pysparq._core.Mod_Mult_UInt_ConstUInt

.. autofunction:: pysparq._core.AddAssign_AnyInt_AnyInt

Quantum Gates
-------------

.. autofunction:: pysparq._core.Hadamard_Int

.. autofunction:: pysparq._core.Hadamard_Bool

.. autofunction:: pysparq._core.QFT

.. autofunction:: pysparq._core.InverseQFT

.. autofunction:: pysparq._core.X_Bool

.. autofunction:: pysparq._core.Y_Bool

.. autofunction:: pysparq._core.Z_Bool

QRAM Operations
---------------

.. autofunction:: pysparq._core.QRAMLoad

.. autofunction:: pysparq._core.QRAMLoadFast

.. autofunction:: pysparq._core.QRAMCircuit_qutrit

State Management
----------------

.. autofunction:: pysparq._core.AddRegister

.. autofunction:: pysparq._core.RemoveRegister

.. autofunction:: pysparq._core.Push

.. autofunction:: pysparq._core.Pop

.. autofunction:: pysparq._core.Normalize

.. autofunction:: pysparq._core.CheckNormalization

Complete API Documentation
--------------------------

For the full API documentation (all classes, functions, and methods), see :ref:`modindex`.
