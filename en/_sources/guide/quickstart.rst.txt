Quick Start
===========

Standalone simulator workbench
------------------------------

.. code-block:: bash

   pysparq info
   pysparq run --case ghz --qubits 32 --out results/ghz.json
   pysparq run --case qft --qubits 6 --basis 3
   pysparq run --case stateprep --qubits 8 --seed 20261109
   pysparq gui

The installed package has its own local GUI with no external web assets.
The bounded experiments use the actual CPU native simulator, expose complex
amplitudes, and check independent references and inverse operations. Reports
include logical qubits, sparse support, normalization, phase-sensitive error,
native binary hash and separate construction/execution times. GHZ supports
up to 32 qubits; Fourier and dense unitary state preparation support 2..10.
These timings describe classical simulation, not quantum runtime or an
application speedup certificate. The GUI runs each experiment in a separate
subprocess with a 60-second deadline; it submits no hardware jobs.

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

Next Steps
----------

Try the same workflow interactively in the :doc:`PySparQ Quick Start notebook </notebooks/01_quickstart>`, then continue with :doc:`Core Concepts </guide/core_concepts/index>` and :doc:`Examples </guide/examples>`.
