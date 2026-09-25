Quick Start
===========

Register-Level Programming
--------------------------

PySparQ adopts the "register-level programming" paradigm. Instead of composing circuits from individual gates, you operate directly on named quantum registers. The level of abstraction rises from qubits to quantum registers, and almost all operations take registers as their unit.

Basic Workflow
--------------

1. Call ``System.clear()`` to clean up static state
2. Declare registers (name, type, number of bits)
3. Create ``SparseState()`` — the default constructor automatically creates the ``|0...0⟩`` initial state
4. Apply quantum operations
5. Read out the measurement results

Example: Quantum Addition
-------------------------

.. code-block:: python

   import pysparq as ps

   # Step 1: clean up static state
   ps.System.clear()

   # Step 2: declare registers
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   # Step 3: create a sparse quantum state (automatically creates the |a=0, b=0⟩ initial state)
   state = ps.SparseState()

   # Step 4: put the registers into superposition
   ps.Hadamard_Int("a")(state)
   ps.Hadamard_Int("b")(state)

   # Quantum addition: a += b
   ps.Add_UInt_UInt("b", "a")(state)

   # The state now holds a superposition of all possible sums
   ps.pprint(state)

Conditional Operations
----------------------

Operations can be conditioned on the values of other registers:

.. code-block:: python

   # Add a control register
   ps.AddRegister("control", ps.Boolean, 1)(state)

   # Apply the operation only when control is |1>
   ps.Add_UInt_UInt("a", "b").conditioned_by_nonzeros("control")(state)

Control Types
^^^^^^^^^^^^^

- :meth:`conditioned_by_nonzeros(reg)` - execute when the register is non-zero
- :meth:`conditioned_by_all_ones(reg)` - execute when the register is all ones
- :meth:`conditioned_by_bit(reg, pos)` - execute when a specific bit is 1
- :meth:`conditioned_by_value(reg, pos)` - execute when the value at the specified position equals a specific value
