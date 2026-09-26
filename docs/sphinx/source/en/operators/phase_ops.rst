Phase and Reflection Operators
==============================

Phase and reflection operators implement conditional phase flips, global phases, Grover reflections and similar operations, and are widely used in quantum search and amplitude amplification algorithms. A complete Grover search implementation is available in the :doc:`algorithm library </cpp_api/algorithms>`.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Phase operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity class
   * - ``ZeroConditionalPhaseFlip``
     - Flip the phase when the specified registers are all zero
     - SelfAdjoint
   * - ``Reflection_Bool``
     - Reflection about the ``|0⟩`` state (Grover diffusion)
     - SelfAdjoint
   * - ``GlobalPhase``
     - Multiply by a complex global phase factor
     - BaseOperator

---

ZeroConditionalPhaseFlip (zero-conditional phase flip)
------------------------------------------------------

.. autoclass:: pysparq.ZeroConditionalPhaseFlip
   :members:
   :undoc-members:

**Operation**: Applies a :math:`-1` phase flip to the basis states when all of the specified registers have value zero.

**Parameters**: A list of register identifiers (list of names or list of IDs).

**Mathematical representation**:

.. math::

   |x_1, x_2, \ldots, x_n\rangle \rightarrow \begin{cases} -|x_1, x_2, \ldots, x_n\rangle & \text{if } x_1 = x_2 = \cdots = x_n = 0 \\ |x_1, x_2, \ldots, x_n\rangle & \text{otherwise} \end{cases}

**Purpose**: Building the Oracle in Grover's algorithm. When the target state is ``|0...0⟩``, this operator is exactly the phase Oracle.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("addr", ps.UnsignedInteger, 3)
   ps.System.add_register("data", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Hadamard_Int("addr", 3)(state)
   # After loading, flip the phase when the data corresponding to addr is 0
   ps.ZeroConditionalPhaseFlip(["data"])(state)

   # A list of register names also works
   ps.ZeroConditionalPhaseFlip(["addr", "data"])(state)

---

Reflection_Bool (reflection)
----------------------------

.. autoclass:: pysparq.Reflection_Bool
   :members:
   :undoc-members:

**Operation**: Reflection about the ``|0⟩`` state, i.e. the core component of the Grover diffusion operator.

**Parameters**:

- ``reg`` — target register (name or ID, or a list)
- ``inverse`` — whether to use the inverse reflection (optional, default ``False``)

**Mathematical representation**:

.. math::

   R = 2|0\rangle\langle 0| - I

**Purpose**: The diffusion operator in Grover's algorithm is composed of Hadamard + Reflection + Hadamard.

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("q", ps.Boolean, 1)

   state = ps.SparseState()
   ps.Hadamard_Bool("q")(state)

   # Reflection
   ps.Reflection_Bool("q")(state)

---

GlobalPhase (global phase)
--------------------------

.. autoclass:: pysparq.GlobalPhase
   :members:
   :undoc-members:

**Operation**: Multiplies the entire quantum state by a complex global phase factor.

**Parameters**: ``c`` — the complex phase factor.

**Dagger**: Multiplies by the complex conjugate.

**Note**: A global phase is unobservable in quantum mechanics, but in some algorithms (such as amplitude encoding, QDA) it must be tracked exactly.

.. code-block:: python

   import numpy as np

   # Global phase rotation e^{iπ/4}
   op = ps.GlobalPhase(np.exp(1j * np.pi / 4))
   op(state)

   # Undo
   op.dag(state)
