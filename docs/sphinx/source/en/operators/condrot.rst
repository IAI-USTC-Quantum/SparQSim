Conditional Rotation Operators
==============================

Conditional rotation operators rotate a target register based on the value of an input register.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Conditional rotation operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity class
   * - ``CondRot_Fixed_Bool``
     - Fixed conditional rotation driven by a Rational angle register
     - BaseOperator

Rotation Mechanism
------------------

Conditional rotation converts the value of the input register into a rotation angle and then applies it to the output register.

For Rational-type input:

.. math::

   \theta = \frac{value}{2^n} \cdot 2\pi

Rotation matrix:

.. math::

   R(\theta) = \begin{pmatrix} \cos\theta & -\sin\theta \\ \sin\theta & \cos\theta \end{pmatrix}

---

CondRot_Fixed_Bool
------------------

.. autoclass:: pysparq.CondRot_Fixed_Bool
   :members:
   :undoc-members:

**Operation**: Rotates a Boolean register according to the value of a Rational register

**Type constraints**:
- Input register: ``Rational``
- Output register: ``Boolean`` (size must be 1)

**Dagger**: Uses the inverse rotation matrix

.. code-block:: python

   import pysparq as ps
   import numpy as np

   ps.System.clear()

   # Rational is used for angle encoding
   ps.System.add_register("angle", ps.Rational, 16)  # High-precision angle
   ps.System.add_register("target", ps.Boolean, 1)

   state = ps.SparseState()

   # Set the angle (e.g. π/4 → value = 2^16 / 8 = 8192)
   angle_value = int(2**16 * 0.125)  # 0.125 = 1/8 of a turn
   ps.Init_Unsafe("angle", angle_value)(state)

   # Initialize the target to |0⟩
   # Initial state: |angle=8192, target=0⟩

   # Conditional rotation
   op = ps.CondRot_Fixed_Bool("angle", "target")
   op(state)

   ps.pprint(state)
   # The target state is rotated by an angle ≈ π/4

   # Undo
   op.dag(state)

Use Cases
---------

Phase estimation support
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # The angle register stores the estimated phase
   ps.System.add_register("phase", ps.Rational, 16)
   ps.System.add_register("ancilla", ps.Boolean, 1)

   # Initialize the ancilla to |+⟩
   ps.Hadamard_Bool("ancilla")(state)

   # The conditional rotation encodes the phase information
   ps.CondRot_Fixed_Bool("phase", "ancilla")(state)

Hamiltonian simulation
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Time-step angle
   dt = 0.01
   angle_val = int(dt * 2**16)

   ps.Init_Unsafe("dt_angle", angle_val)(state)
   ps.CondRot_Fixed_Bool("dt_angle", "qubit")(state)

Quantum amplitude encoding
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Rotate the target bit according to a data value
   def amplitude_encoding(value: int) -> np.ndarray:
       theta = np.arccos(value / 255.0)  # Assume 8-bit data
       return np.array([
           [np.cos(theta), -np.sin(theta)],
           [np.sin(theta), np.cos(theta)]
       ])

   # Preferred pattern:
   # 1. compute angle into a Rational register using an arithmetic adapter
   # 2. rotate with CondRot_Fixed_Bool
   # 3. apply the same adapter again to uncompute
   ps.CondRot_Fixed_Bool("angle", "qubit")(state)
