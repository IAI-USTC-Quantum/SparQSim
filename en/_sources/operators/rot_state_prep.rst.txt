Rotation and State Preparation
==============================

Rotation and state preparation operators provide arbitrary-dimensional unitary rotations and the ability to prepare a target quantum state from ``|0⟩``. QRAM-based state preparation built on them is part of the :doc:`algorithm library </cpp_api/algorithms>`.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Rotation and state preparation operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity class
   * - ``Rot_GeneralUnitary``
     - Apply an arbitrary-dimensional unitary matrix
     - BaseOperator
   * - ``Rot_GeneralStatePrep``
     - Prepare a target quantum state from ``|0⟩``
     - BaseOperator

---

Rot_GeneralUnitary (general unitary rotation)
---------------------------------------------

.. autoclass:: pysparq.Rot_GeneralUnitary
   :members:
   :undoc-members:

**Operation**: Applies an arbitrary :math:`2^n \times 2^n` unitary matrix to a register.

**Parameters**:

- ``reg`` — target register (name or ID)
- ``matrix`` — unitary matrix (``DenseMatrix_complex`` or a NumPy array)

**Dagger**: Applies the conjugate transpose of the matrix.

**Purpose**: When the standard gate library cannot directly express the desired transformation, you can specify it directly in matrix form.

.. note::

   The matrix dimension must match the Hilbert-space dimension :math:`2^n` of the register, where :math:`n` is the number of bits in the register.

.. code-block:: python

   import numpy as np
   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   state = ps.SparseState()

   # Define a 4x4 unitary matrix
   matrix = np.eye(4, dtype=complex)
   matrix[0, 0] = 0
   matrix[0, 3] = 1
   matrix[3, 3] = 0
   matrix[3, 0] = 1

   op = ps.Rot_GeneralUnitary("q", matrix)
   op(state)

   # Undo
   op.dag(state)

---

Rot_GeneralStatePrep (quantum state preparation)
------------------------------------------------

.. autoclass:: pysparq.Rot_GeneralStatePrep
   :members:
   :undoc-members:

**Operation**: Prepares a register from the ``|0⟩`` state into a target quantum state.

**Parameters**:

- ``reg`` — target register (name or ID)
- ``state_vector`` — the target state vector (list of complex numbers or a NumPy array)

**Dagger**: Maps the target state back to ``|0⟩``.

**Mathematical representation**: Given a target state :math:`|\psi\rangle = \sum_i \alpha_i |i\rangle`, construct a unitary :math:`U` such that :math:`U|0\rangle = |\psi\rangle`.

.. warning::

   This operator requires the register to currently be in the ``|0⟩`` state. If the register already holds a nonzero value, the behavior is undefined.

.. code-block:: python

   import numpy as np
   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   state = ps.SparseState()

   # Prepare a simplified Bell state: a uniform superposition
   target = np.array([0.5, 0.5, 0.5, 0.5], dtype=complex)

   op = ps.Rot_GeneralStatePrep("q", target)
   op(state)

   ps.pprint(state)
   # |q=0⟩ : (0.5+0j)
   # |q=1⟩ : (0.5+0j)
   # |q=2⟩ : (0.5+0j)
   # |q=3⟩ : (0.5+0j)

Helper Functions
----------------

stateprep_unitary_build_schmidt
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.stateprep_unitary_build_schmidt

**Operation**: Constructs the unitary matrix needed for state preparation using a Schmidt decomposition.

**Purpose**: Use it when you need to manually build or inspect the internal matrix of ``Rot_GeneralStatePrep``.
