Basic Quantum Gates
===================

Basic quantum gates implement the standard single-qubit and multi-qubit quantum gate operations.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Basic quantum gates overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity class
   * - ``X_Bool``
     - Pauli-X (bit flip)
     - SelfAdjoint
   * - ``Y_Bool``
     - Pauli-Y
     - SelfAdjoint
   * - ``Z_Bool``
     - Pauli-Z (phase flip)
     - SelfAdjoint
   * - ``S_Bool``
     - S gate (π/2 phase)
     - SelfAdjoint
   * - ``T_Bool``
     - T gate (π/4 phase)
     - SelfAdjoint
   * - ``Phase_Bool``
     - Arbitrary phase e^{iλ}
     - BaseOperator
   * - ``RX_Bool``
     - Rotation about the X axis
     - SelfAdjoint
   * - ``RY_Bool``
     - Rotation about the Y axis
     - SelfAdjoint
   * - ``RZ_Bool``
     - Rotation about the Z axis
     - SelfAdjoint
   * - ``SX_Bool``
     - √X gate
     - SelfAdjoint
   * - ``U2_Bool``
     - General single-qubit gate (2 parameters)
     - BaseOperator
   * - ``U3_Bool``
     - General single-qubit gate (3 parameters)
     - BaseOperator

Type Constraints
----------------

All quantum gates require:

- Register type: ``Boolean`` (single-qubit gates)
- Bit index: must be within the register size range [0, size)

.. code-block:: python

   # Correct: Boolean type for single-qubit gates
   ps.System.add_register("qubit", ps.Boolean, 1)
   ps.X_Bool("qubit", 0)(state)

   # Wrong: bit index out of range
   # ps.X_Bool("qubit", 1)(state)  # Raises an exception!

---

Pauli Gates
-----------

X_Bool (Pauli-X / NOT)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.X_Bool
   :members:
   :undoc-members:

**Operation**: Bit flip ``|0⟩ ↔ |1⟩``

**Matrix**:

.. math::

   X = \begin{pmatrix} 0 & 1 \\ 1 & 0 \end{pmatrix}

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.Boolean, 1)

   state = ps.SparseState()
   # Initially |q=0⟩

   ps.X_Bool("q", 0)(state)
   # |q=1⟩

   # Applying again restores the original state (self-adjoint)
   ps.X_Bool("q", 0)(state)
   # |q=0⟩

Y_Bool (Pauli-Y)
^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Y_Bool
   :members:
   :undoc-members:

**Matrix**:

.. math::

   Y = \begin{pmatrix} 0 & -i \\ i & 0 \end{pmatrix}

.. code-block:: python

   ps.Y_Bool("q", 0)(state)

Z_Bool (Pauli-Z)
^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Z_Bool
   :members:
   :undoc-members:

**Operation**: Phase flip ``|1⟩ → -|1⟩``

**Matrix**:

.. math::

   Z = \begin{pmatrix} 1 & 0 \\ 0 & -1 \end{pmatrix}

.. code-block:: python

   ps.Z_Bool("q", 0)(state)

---

Phase Gates
-----------

S_Bool (S gate)
^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.S_Bool
   :members:
   :undoc-members:

**Operation**: Phase rotation by π/2

**Matrix**:

.. math::

   S = \begin{pmatrix} 1 & 0 \\ 0 & i \end{pmatrix}

.. code-block:: python

   ps.S_Bool("q", 0)(state)

T_Bool (T gate)
^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.T_Bool
   :members:
   :undoc-members:

**Operation**: Phase rotation by π/4

**Matrix**:

.. math::

   T = \begin{pmatrix} 1 & 0 \\ 0 & e^{i\pi/4} \end{pmatrix}

.. code-block:: python

   ps.T_Bool("q", 0)(state)

Phase_Bool (arbitrary phase)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Phase_Bool
   :members:
   :undoc-members:

**Operation**: Phase rotation e^{iλ}

**Dagger**: Phase rotation e^{-iλ}

.. code-block:: python

   # Phase rotation by π/3
   op = ps.Phase_Bool("q", 0, np.pi / 3)
   op(state)

   # Undo
   op.dag(state)

---

Rotation Gates
--------------

RX_Bool (X-axis rotation)
^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.RX_Bool
   :members:
   :undoc-members:

**Matrix**:

.. math::

   R_X(\theta) = \begin{pmatrix} \cos\frac{\theta}{2} & -i\sin\frac{\theta}{2} \\ -i\sin\frac{\theta}{2} & \cos\frac{\theta}{2} \end{pmatrix}

.. code-block:: python

   import numpy as np

   # X-axis rotation by π/2
   ps.RX_Bool("q", np.pi / 2)(state)

RY_Bool (Y-axis rotation)
^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.RY_Bool
   :members:
   :undoc-members:

**Matrix**:

.. math::

   R_Y(\theta) = \begin{pmatrix} \cos\frac{\theta}{2} & -\sin\frac{\theta}{2} \\ \sin\frac{\theta}{2} & \cos\frac{\theta}{2} \end{pmatrix}

.. code-block:: python

   ps.RY_Bool("q", np.pi / 2)(state)

RZ_Bool (Z-axis rotation)
^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.RZ_Bool
   :members:
   :undoc-members:

**Matrix**:

.. math::

   R_Z(\theta) = \begin{pmatrix} e^{-i\theta/2} & 0 \\ 0 & e^{i\theta/2} \end{pmatrix}

.. code-block:: python

   ps.RZ_Bool("q", np.pi / 2)(state)

SX_Bool (√X gate)
^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.SX_Bool
   :members:
   :undoc-members:

**Operation**: X^{1/2}

.. code-block:: python

   ps.SX_Bool("q", 0)(state)

---

Universal Gates
---------------

U2_Bool (2-parameter universal gate)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.U2_Bool
   :members:
   :undoc-members:

**Matrix**:

.. math::

   U_2(\phi, \lambda) = \frac{1}{\sqrt{2}} \begin{pmatrix} 1 & -e^{i\lambda} \\ e^{i\phi} & e^{i(\phi+\lambda)} \end{pmatrix}

U3_Bool (3-parameter universal gate)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.U3_Bool
   :members:
   :undoc-members:

**Matrix**:

.. math::

   U_3(\theta, \phi, \lambda) = \begin{pmatrix} \cos\frac{\theta}{2} & -e^{i\lambda}\sin\frac{\theta}{2} \\ e^{i\phi}\sin\frac{\theta}{2} & e^{i(\phi+\lambda)}\cos\frac{\theta}{2} \end{pmatrix}

.. code-block:: python

   import numpy as np

   op = ps.U3_Bool("q", np.pi/4, np.pi/2, 0)
   op(state)

   # Undo
   op.dag(state)

---

Other Gates
-----------

Rot_Bool (general rotation)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Rot_Bool
   :members:
   :undoc-members:

**Operation**: Apply an arbitrary 2×2 unitary matrix

.. code-block:: python

   import numpy as np

   # Define a 2x2 matrix
   matrix = np.array([
       [np.cos(np.pi/4), -np.sin(np.pi/4)],
       [np.sin(np.pi/4), np.cos(np.pi/4)]
   ])

   ps.Rot_Bool("q", matrix)(state)

Reflection_Bool (reflection gate)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Reflection_Bool
   :members:
   :undoc-members:

**Operation**: Reflection operation (the diffusion operator in Grover's algorithm)
