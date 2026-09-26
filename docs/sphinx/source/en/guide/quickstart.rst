Quick Start
===========

Register-Level Programming
--------------------------

PySparQ adopts the ":doc:`register-level programming </guide/core_concepts/index>`" paradigm. Instead of composing circuits from individual gates, you operate directly on named quantum registers. The level of abstraction rises from qubits to quantum registers, and almost all operations take registers as their unit.

Basic Workflow
--------------

1. Call :meth:`System.clear() <pysparq.System.clear>` to clean up static state
2. :doc:`Declare registers </guide/core_concepts/register_management>` (name, type, number of bits)
3. Create :class:`SparseState() <pysparq.SparseState>` — the default constructor automatically creates the ``|0...0⟩`` initial state
4. Apply quantum operations (see the :ref:`operator reference <operator-reference>`)
5. Read out the measurement results with :doc:`partial trace operators </operators/partial_trace>`

Example: Quantum Addition
-------------------------

.. code-block:: python

   import pysparq as ps

   # Step 1: clean up static state
   ps.System.clear()

   # Step 2: declare registers
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   # Step 3: create a sparse quantum state (automatically creates the |a=0, b=0⟩ initial state)
   state = ps.SparseState()

   # Step 4: put the registers into superposition
   ps.Hadamard_Int("a", 4)(state)
   ps.Hadamard_Int("b", 4)(state)

   # Quantum addition: result = a + b
   ps.Add_UInt_UInt("a", "b", "result")(state)

   # The state now holds a superposition of all possible sums
   ps.pprint(state)

The example uses :class:`Hadamard_Int <pysparq.Hadamard_Int>` to put both registers into superposition and :class:`Add_UInt_UInt <pysparq.Add_UInt_UInt>` to add them. See :doc:`Hadamard operations </operators/hadamard>`, :doc:`arithmetic operators </operators/arithmetic>`, and the :doc:`debugging tools </operators/debug>` (``pprint``) for details.

Conditional Operations
----------------------

Operations can be conditioned on the values of other registers (see :ref:`Conditional Operations <conditional-operations>` for the full reference):

.. code-block:: python

   # Add a control register
   ps.AddRegister("control", ps.Boolean, 1)(state)

   # Apply the operation only when control is |1>
   ps.Add_UInt_UInt("a", "b").conditioned_by_nonzeros("control")(state)

Control Types
^^^^^^^^^^^^^

- :ref:`conditioned_by_nonzeros(reg) <conditional-operations>` - execute when the register is non-zero
- :ref:`conditioned_by_all_ones(reg) <conditional-operations>` - execute when the register is all ones
- :ref:`conditioned_by_bit(reg, pos) <conditional-operations>` - execute when a specific bit is 1
- :ref:`conditioned_by_value(reg, val) <conditional-operations>` - execute when the register equals a specific value
