Register Management
===================

Registers are the core abstraction of PySparQ's "Register Level Programming". Unlike traditional qubit-level programming, PySparQ raises the level of operations to quantum registers, which makes algorithm development more intuitive.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

The Nature of Registers
-----------------------

A register in the System hosts storage of ``n`` ``uint64_t`` values. Each register has a name, a type, and a bit width, and its value is stored as a ``uint64_t``. This design allows us to encode quantum states in a multi-register form such as :math:`|a\rangle|b\rangle|c\rangle` without having to care about how the underlying qubits are encoded.

For example, the QRAM access :math:`|i\rangle|0\rangle \to |i\rangle|d[i]\rangle` requires only an address register ``i`` and a data register ``d``: the :class:`QRAMLoad <pysparq.QRAMLoad>` operator completes the mapping directly at the register level, without managing any qubits at all.

.. math::

   |\psi\rangle = \sum_j \alpha_j \, |a_j\rangle |b_j\rangle |c_j\rangle

where each :math:`|a_j\rangle |b_j\rangle |c_j\rangle` corresponds to one entry of the ``registers`` array in a ``System``.

Adding Registers: AddRegister
-----------------------------

Adding a register is equivalent to taking the tensor product with a :math:`|0\rangle` register. The new register has the initial value 0 in all existing basis states.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   state = ps.SparseState()

   # Add register "b": equivalent to |a⟩ ⊗ |0⟩
   ps.AddRegister("b", ps.UnsignedInteger, 4)(state)

   # The "b" value of every basis state in state is 0
   ps.pprint(state)

``AddRegister`` updates both the static metadata (``name_register_map``) and the register values of all existing basis states.

You can also use ``AddRegisterWithHadamard`` to apply a Hadamard while adding the register, directly creating a uniform superposition:

.. code-block:: python

   # Add "q" and create a uniform superposition of |0⟩, |1⟩, ..., |2^n - 1⟩
   ps.AddRegisterWithHadamard("q", ps.UnsignedInteger, 2)(state)

Removing Registers: RemoveRegister
----------------------------------

Removing a register is equivalent to taking the :doc:`PartialTrace </operators/partial_trace>` over that register, i.e. eliminating its contribution to the quantum state. In a simulation this has the same effect as measuring the register and then discarding the measurement result.

.. code-block:: python

   # Remove register "b": equivalent to a PartialTrace over "b"
   ps.RemoveRegister("b")(state)

.. important::

   ``RemoveRegister`` checks, before executing, whether the register is entangled with the remaining registers (via :doc:`TestRemovable </operators/debug>`). If entanglement exists, the removal raises an exception, because in that case the PartialTrace over a single register can no longer be treated simply as discarding it.

Splitting Registers: SplitRegister
----------------------------------

``SplitRegister`` splits one register into two: the original register keeps the high bits, and the new register receives the low bits.

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("full", ps.UnsignedInteger, 8)
   state = ps.SparseState()
   ps.Init_Unsafe("full", 0b10110011)(state)

   # Split: the original "full" keeps the high 4 bits, the new "low" receives the low 4 bits
   ps.SplitRegister("full", "low", 4)(state)
   # "full" = 0b1011 (high 4 bits), "low" = 0b0011 (low 4 bits)

The split process:

1. Add the new register (it receives the low bits)
2. Shrink the bit width of the original register
3. In all basis states, keep the high bits of the original value in the original register and write the low bits into the new register

Combining Registers: CombineRegister
------------------------------------

``CombineRegister`` merges two registers into one: the value of the first register is shifted left and concatenated with the value of the second register.

.. code-block:: python

   # Combine: "full" = (full << 4) + low
   ps.CombineRegister("full", "low")(state)
   # "full" = 0b10110011

The merge process:

1. Extend the bit width of the first register (by the width of the second)
2. In all basis states, shift the first register's value left by the second register's width and add the second register's value
3. Remove the second register

PartialTrace: Measurement
-------------------------

In PySparQ, :class:`PartialTrace <pysparq.PartialTrace>` is treated as being equivalent to a measurement. This is reasonable at the simulation level — taking a partial trace over some registers is the same as measuring them and discarding the results.

``PartialTrace`` provides three modes:

.. list-table:: PartialTrace variants
   :header-rows: 1

   * - Class
     - Behavior
     - Return value
   * - :class:`PartialTrace <pysparq.PartialTrace>`
     - Random measurement: randomly pick a measurement outcome according to the probability distribution, then collapse the state
     - ``(measured_values, probability)``
   * - :class:`PartialTraceSelect <pysparq.PartialTraceSelect>`
     - Selective collapse: keep the basis states whose specified register has a specified value, and renormalize the remaining ones
     - Normalized probability
   * - :class:`PartialTraceSelectRange <pysparq.PartialTraceSelectRange>`
     - Range collapse: keep the basis states whose specified register value lies within a given range
     - Normalized probability

.. code-block:: python
   :caption: PartialTrace measurement example

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 2)
   ps.System.add_register("b", ps.UnsignedInteger, 2)
   state = ps.SparseState()

   ps.Hadamard_Int("a", 2)(state)
   ps.Hadamard_Int("b", 2)(state)

   # Randomly measure register "a"
   measured_values, prob = ps.PartialTrace("a")(state)
   print(f"Measured values: {measured_values}, probability: {prob}")
   # The value of "a" in state collapses to the measurement result

   # Selective collapse: force "b" = 2
   prob = ps.PartialTraceSelect("b", [2])(state)

.. code-block:: python
   :caption: PartialTraceSelect deterministic collapse example

   # Keep only the basis states with "a" = 1 and normalize
   prob = ps.PartialTraceSelect("a", [1])(state)

Register Stack Operations: Push / Pop
-------------------------------------

:class:`Push <pysparq.Push>` and :class:`Pop <pysparq.Pop>` (documented under :doc:`system operations </operators/system_ops>`) provide stack management for temporary registers, which is useful when an algorithm needs temporary variables:

.. code-block:: python

   # Push: push a temporary register onto the stack
   ps.Push("temp", ps.UnsignedInteger, 4)(state)

   # Perform computations with the temporary register...

   # Pop: pop the temporary register
   ps.Pop()(state)

Summary: Register Operations and the Quantum State
--------------------------------------------------

.. list-table:: Physical meaning of register operations
   :header-rows: 1

   * - Operation
     - Physical meaning
     - Effect on the quantum state
   * - ``AddRegister``
     - Tensor product with :math:`|0\rangle`
     - Every basis state gains one register with value 0
   * - ``RemoveRegister``
     - PartialTrace (measure and discard)
     - Eliminates the register, provided it is not entangled
   * - ``SplitRegister``
     - Splits one subsystem into two
     - Number of basis states unchanged, number of registers +1
   * - ``CombineRegister``
     - Merges two subsystems into one
     - Number of basis states unchanged, number of registers -1
   * - ``PartialTrace``
     - Measurement
     - Collapses the state according to the probabilities, reducing the number of basis states
   * - ``Push`` / ``Pop``
     - Push/pop of temporary registers
     - Auxiliary operations that save/restore register state

API Reference
-------------

For the complete API documentation of the register management operators, see the following operator reference pages:

- :doc:`System operations such as Push / Pop / ClearZero </operators/system_ops>`
- :doc:`PartialTrace measurement operators </operators/partial_trace>`

or consult the :ref:`Operator Reference <operator-reference>` section for the complete list of all operators.
