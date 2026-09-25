Hadamard Operations
===================

The Hadamard operators create quantum superpositions on registers and are fundamental operations in quantum algorithms.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Hadamard operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Type constraint
     - Unitarity class
   * - ``Hadamard_Int``
     - Hadamard on an integer register
     - Integer type
     - SelfAdjoint
   * - ``Hadamard_Int_Full``
     - Full Hadamard (all output states)
     - Integer type
     - SelfAdjoint
   * - ``Hadamard_Bool``
     - Single-qubit Hadamard
     - Boolean (size=1)
     - SelfAdjoint
   * - ``Hadamard_Partial``
     - Partial-qubit Hadamard
     - Integer type
     - SelfAdjoint

---

Hadamard_Int
------------

.. autoclass:: pysparq.Hadamard_Int
   :members:
   :undoc-members:

**Operation**: Applies a Hadamard to the specified qubits of an integer register

**Mathematical definition**:

.. math::

   H|x\rangle = \frac{1}{\sqrt{2}}(|0\rangle + |1\rangle) \quad \text{for each qubit}

**Type constraints**: ``UnsignedInteger`` or ``SignedInteger``

.. code-block:: python

   import pysparq as ps

   ps.System.clear()

   # 4-bit register
   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   print("Initial state:")
   ps.pprint(state)
   # [1 basis state]
   # |q=0⟩ : (1+0j)

   # Apply Hadamard to the first 2 bits
   ps.Hadamard_Int("q", 2)(state)

   print("\nAfter Hadamard:")
   ps.pprint(state)
   # [2 basis states]
   # |q=0⟩ : (0.707+0j)
   # |q=2⟩ : (0.707+0j)

---

Hadamard_Int_Full
-----------------

.. autoclass:: pysparq.Hadamard_Int_Full
   :members:
   :undoc-members:

**Operation**: Applies a full Hadamard to the entire register, creating a uniform superposition of all :math:`2^n` states

**Mathematical definition**:

.. math::

   H^n|x\rangle = \frac{1}{\sqrt{2^n}}\sum_{y=0}^{2^n-1}|y\rangle

**Type constraints**: Integer type

**Note**: This creates :math:`2^n` basis states; for large n this may cause memory problems.

.. code-block:: python

   ps.System.clear()

   # 2-bit register
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   state = ps.SparseState()

   # Full Hadamard: creates 2^2 = 4 states
   ps.Hadamard_Int_Full("q")(state)

   ps.pprint(state)
   # [4 basis states]
   # |q=0⟩ : (0.5+0j)
   # |q=1⟩ : (0.5+0j)
   # |q=2⟩ : (0.5+0j)
   # |q=3⟩ : (0.5+0j)

   # Applying it again undoes it (self-adjoint)
   ps.Hadamard_Int_Full("q")(state)
   # Back to a single basis state

---

Hadamard_Bool
-------------

.. autoclass:: pysparq.Hadamard_Bool
   :members:
   :undoc-members:

**Operation**: Single-qubit Hadamard

**Matrix**:

.. math::

   H = \frac{1}{\sqrt{2}}\begin{pmatrix} 1 & 1 \\ 1 & -1 \end{pmatrix}

**Type constraints**: ``Boolean`` (the register size must be 1)

**Bit constraints**: None (operates on bit 0 by default)

.. code-block:: python

   ps.System.clear()

   # Single-qubit register
   ps.System.add_register("qubit", ps.Boolean, 1)  # Must be 1 bit!

   state = ps.SparseState()

   ps.Hadamard_Bool("qubit")(state)

   ps.pprint(state)
   # [2 basis states]
   # |qubit=0⟩ : (0.707+0j)
   # |qubit=1⟩ : (0.707+0j)

---

Hadamard_Partial
---------------------

.. autoclass:: pysparq.Hadamard_Partial
   :members:
   :undoc-members:

**Operation**: Applies a Hadamard to the specified qubits of a register

**Type constraints**: Integer type

**Bit constraints**: The specified positions must be within the register range

.. code-block:: python

   ps.System.clear()

   # 4-bit register
   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Apply Hadamard only to bits 1 and 3
   positions = {1, 3}
   ps.Hadamard_Partial("q", positions)(state)

   # Creates 2^2 = 4 superposition states (only positions 1 and 3 flip)

---

Use Cases
---------

Uniform superposition
^^^^^^^^^^^^^^^^^^^^^

Used in quantum search, quantum sampling and similar algorithms:

.. code-block:: python

   # Create a uniform superposition of the address register
   ps.System.add_register("addr", ps.UnsignedInteger, n)
   state = ps.SparseState()
   ps.Hadamard_Int_Full("addr")(state)

   # addr is now uniformly distributed over all possible values

Single-qubit initialization
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Used to initialize a control bit:

.. code-block:: python

   ps.System.add_register("ctrl", ps.Boolean, 1)
   ps.Hadamard_Bool("ctrl")(state)

   # ctrl is in the |+⟩ = (|0⟩ + |1⟩)/√2 state

Partial superposition
^^^^^^^^^^^^^^^^^^^^^

For selective superposition:

.. code-block:: python

   # Superpose only the low 2 bits
   ps.Hadamard_Int("reg", 2)(state)

   # Or superpose specific positions
   ps.Hadamard_Partial("reg", {0, 2})(state)
